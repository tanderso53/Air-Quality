#include "bme680-interface.h"
#include "ws2812.pio.h"

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#ifndef AIR_QUALITY_STATUS_LED
#define AIR_QUALITY_STATUS_LED 13
#endif

#ifndef AIR_QUALITY_INFO_LED_PIN
#define AIR_QUALITY_INFO_LED_PIN 16
#endif

typedef struct {
	absolute_time_t lastblink;
	uint8_t laststate;
	int16_t blink_delay; /* in ms */
} air_quality_led_config;

/** @brief Print errors to stdout when things go wrong */
void air_quality_handle_error(int16_t);

/** @brief Print out data from environmental sensors as json string
 * @p d Data struct from bme68x vendor library
 */
void air_quality_print_data(struct bme68x_data *d, uint32_t millis);

static void init_info_led();

static void write_info_led_color(uint8_t r, uint8_t g, uint8_t b);

static int32_t aq_read_raw_humidity(bme680_intf *b_intf);

void air_quality_handle_error(int16_t err)
{
	printf("There was an error %d\n", err);

	if (err > 0) {
		write_info_led_color(50, 50, 0);
	} else if (err < 0) {
		write_info_led_color(75, 0, 0);
	}
}

void air_quality_print_data(struct bme68x_data *d, uint32_t millis)
{
	printf("{\"data\": ["
	       "{\"name\": \"temperature\", "
	       "\"value\": %.2f, "
	       "\"unit\": \"degC\", "
	       "\"timemillis\": %d},"
	       "{\"name\": \"pressure\", "
	       "\"value\": %.2f, "
	       "\"unit\": \"Pa\", "
	       "\"timemillis\": %d},"
	       "{\"name\": \"humidity\", "
	       "\"value\": %.2f, "
	       "\"unit\": \"Percent\", "
	       "\"timemillis\": %d},"
	       "{\"name\": \"gas resistance\", "
	       "\"value\": %.2f, "
	       "\"unit\": \"ul\", "
	       "\"timemillis\": %d}],"
	       "\"status\": {"
	       "\"sensor\": \"0x%x\"}}\n",
	       d->temperature, millis,
	       d->pressure, millis,
	       d->humidity, millis,
	       d->gas_resistance, millis,
	       d->status);
}

void air_quality_status_led_init(air_quality_led_config *conf,
				 int16_t blink_delay)
{
	gpio_init(AIR_QUALITY_STATUS_LED);
	gpio_set_dir(AIR_QUALITY_STATUS_LED, GPIO_OUT);
	gpio_put(AIR_QUALITY_STATUS_LED, 0);

	conf->lastblink = 0;
	conf->laststate = 0;
	conf->blink_delay = blink_delay;
}

uint8_t air_quality_status_led_test_state(air_quality_led_config *conf)
{
	absolute_time_t time_from;
	absolute_time_t time_to;

	time_from = conf->lastblink;
	time_to = delayed_by_ms(conf->lastblink, conf->blink_delay);

	if (absolute_time_diff_us(time_from, time_to) > 0) {
		return 1;
	}

	return 0;
}

void air_quality_status_led_on(air_quality_led_config *conf)
{
	if (conf->blink_delay < 0) {
		gpio_put(AIR_QUALITY_STATUS_LED, 1);
		conf->laststate = 1;
		conf->lastblink = get_absolute_time();
		return;
	}

	if (air_quality_status_led_test_state(conf)) {

		if (conf->laststate) {
			gpio_put(AIR_QUALITY_STATUS_LED, 0);
			conf->laststate = 0;
		} else {
			gpio_put(AIR_QUALITY_STATUS_LED, 1);
			conf->laststate = 1;
		}

		conf->lastblink = get_absolute_time();
		return;
	}
}

void air_quality_status_led_off(air_quality_led_config *conf)
{
	gpio_put(AIR_QUALITY_STATUS_LED, 0);
	conf->laststate = 0;
	conf->lastblink = get_absolute_time();
}

static void init_info_led()
{
	ws2812_program_init(pio0, 0, pio_add_program(pio0, &ws2812_program),
			    AIR_QUALITY_INFO_LED_PIN, 800000, false);
}

static void write_info_led_color(uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t urgb;

	/* Load int 32u bit int ordered g, r, b left to right */
	urgb = ((uint32_t) (r) << 8) | ((uint32_t) (g) << 16) | (uint32_t) (b);

	pio_sm_put_blocking(pio0, 0, urgb << 8u); /* RGB value only 24 bit */

	/* Note: this function will block if pio buffer is full */
}

static int32_t aq_read_raw_humidity(bme680_intf *b_intf)
{
	const uint8_t st_addr = 0x25;
	const uint8_t len = 2;
	int32_t ret;
	uint32_t rst = 0;
	uint8_t d[] = {0x00, 0x00};

	if (b_intf == NULL) {
		return -13;
	}

	ret = bme68x_get_regs(st_addr, d, len, &b_intf->bme_dev);

	if (ret < 0) {
		return ret;
	}

	for (uint8_t i = 0; i < len; i++) {
		rst = rst | ((uint32_t) d[i] << i * 8);
	}

	return (int32_t) rst;
}

int main() {
	bme680_intf b_intf;
	bme680_run_mode m = FORCED_MODE;
	struct bme68x_data d;
	int8_t ret = 0;
	air_quality_led_config led_conf;
	const uint16_t sample_delay_ms = 10000;
	absolute_time_t next_sample_time;

	stdio_init_all();

#ifdef AIR_QUALITY_WAIT_CONNECTION
	for (;;) {

		if (stdio_usb_connected()) {
			printf("Welcome! You are connected!\n");
			break;
		}

		sleep_ms(100);
	}
#endif /* AIR_QUALITY_WAIT_CONNECTION */

	/* Turn on status LED */
	air_quality_status_led_init(&led_conf, -1);
	air_quality_status_led_on(&led_conf);
	init_info_led();
	write_info_led_color(0, 75, 0);

	/* initialize variables in interface struct */
	b_intf.i2c = NULL; /* NULL i2c will select default */
	b_intf.timeout = 1000; /* 500ms timeout on i2c read/write */

#ifdef BME680_INTERFACE_SELFTEST
	/* Option to compile in a selft test of sensor at start of
	 * MCU */
	printf("Beginning BME680 Selftest...Standby...\n");
	ret = selftest_bme680_sensor(&b_intf, BME68X_I2C_ADDR_HIGH);

	if (ret == BME68X_OK) {
		printf("BME680 Selftest SUCCESS...Continuing...\n");
	} else if (ret > 0) {
		printf("BME680 Selftest WARNING with code %d...Continuing...\n",
		       ret);
		write_info_led_color(50, 50, 0);
	} else {
		printf("BME680 Selftest FAILURE with code %d...Ending...\n",
			ret);
		write_info_led_color(75, 0, 0);
		return 1;
	}
#endif /* #ifdef BME680_INTERFACE_SELFTEST */

	/* Keep trying to connect to sensor until there is a
	 * success */
	for (;;) {
		ret = init_bme680_sensor(&b_intf, BME68X_I2C_ADDR_HIGH, m);

		if (ret >= 0) {
			break;
		}

		air_quality_handle_error(ret);
		sleep_ms(1000);
	}

	air_quality_status_led_init(&led_conf, 2000);
	next_sample_time = make_timeout_time_ms(sample_delay_ms);

	/* Keep polling the sensor for data if initialization was
	 * successful. This loop will only break on error. */
	for (;;) {
		absolute_time_t readtime;

		air_quality_status_led_on(&led_conf);

		if (absolute_time_diff_us(next_sample_time, get_absolute_time()) < 0) {
			continue;
		}

		ret = sample_bme680_sensor(m, &b_intf, &d);
		readtime = get_absolute_time();
		next_sample_time = delayed_by_ms(readtime, sample_delay_ms);

		int32_t rawhum = aq_read_raw_humidity(&b_intf);
		printf("{\"Raw Humidity\": %d}\n", rawhum);

		if (ret == BME68X_W_NO_NEW_DATA) {
			continue;
		}

		if (ret < 0) {
			air_quality_handle_error(ret);
			break;
		}

		air_quality_print_data(&d, to_ms_since_boot(readtime));
	}

	/* Deinit i2c if loop broke */
	deinit_bme680_sensor(&b_intf);
	air_quality_status_led_off(&led_conf);

	return 1;
}
