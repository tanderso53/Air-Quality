#include "bme680-interface.h"
#include "bme280-interface.h"
#include "bme280-interface-error.h"
#include "pm2_5-interface.h"
#include <pm2_5-error.h>
#include "ws2812.pio.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#ifndef AIR_QUALITY_STATUS_LED
#define AIR_QUALITY_STATUS_LED 13
#endif

#ifndef AIR_QUALITY_INFO_LED_PIN
#define AIR_QUALITY_INFO_LED_PIN 16
#endif

#ifndef AIR_QUALITY_PM2_5_TX_PIN
#define AIR_QUALITY_PM2_5_TX_PIN 0
#endif

#ifndef AIR_QUALITY_PM2_5_RX_PIN
#define AIR_QUALITY_PM2_5_RX_PIN 1
#endif

#ifndef AIR_QUALITY_PM2_5_UART_PIN
#define AIR_QUALITY_PM2_5_UART uart0
#endif

#ifndef PICO_BOARD
#define PICO_BOARD "unknown"
#endif

#ifndef PICO_TARGET_NAME
#define PICO_TARGET_NAME "unknown"
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
	printf("{\"sensor\": \"BME680\", \"data\": [");
	printf("{\"name\": \"temperature\", "
	       "\"value\": %.2f, "
	       "\"unit\": \"degC\", "
	       "\"timemillis\": %lu}, ", d->temperature,
	       (unsigned long) millis);
	printf("{\"name\": \"pressure\", "
	       "\"value\": %.2f, "
	       "\"unit\": \"Pa\", "
	       "\"timemillis\": %lu}, ", d->pressure,
	       (unsigned long) millis);
	printf("{\"name\": \"humidity\", "
	       "\"value\": %.2f, "
	       "\"unit\": \"%%\", "
	       "\"timemillis\": %lu}, ", d->humidity,
	       (unsigned long) millis);
	printf("{\"name\": \"gas resistance\", "
	       "\"value\": %.2f, "
	       "\"unit\": \"ul\", "
	       "\"timemillis\": %lu}], ", 
	       d->gas_resistance, (unsigned long) millis);
	printf("\"status\": {"
	       "\"sensor\": \"%#x\"}}",
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

static int8_t aq_bme280_init(bme280_intf *b_intf)
{
	int8_t rslt;

	/* Set up device interface */
	b_intf->addr = BME280_I2C_ADDR_SEC;
	b_intf->dev.intf_ptr = (void*) b_intf;
	b_intf->dev.intf = BME280_I2C_INTF;
	b_intf->dev.read = user_i2c_read;
	b_intf->dev.write = user_i2c_write;
	b_intf->dev.delay_us = user_delay_us;

	rslt = bme280_init(&b_intf->dev);

	if (rslt != BME280_OK) {
		return rslt;
	}

	return 0;
}

static int8_t aq_bme280_configure(bme280_intf *b_intf,
				  bme280_op_mode mode)
{
	int8_t rslt;
	uint8_t settings_sel;

	/* Configure oversampling */
	b_intf->dev.settings.osr_h = BME280_OVERSAMPLING_1X;
	b_intf->dev.settings.osr_p = BME280_OVERSAMPLING_16X;
	b_intf->dev.settings.osr_t = BME280_OVERSAMPLING_2X;
	b_intf->dev.settings.filter = BME280_FILTER_COEFF_16;

	switch (mode) {
	case BME280_IFACE_NORMAL_MODE:
		b_intf->dev.settings.standby_time = BME280_STANDBY_TIME_62_5_MS;

		settings_sel = BME280_OSR_PRESS_SEL;
		settings_sel |= BME280_OSR_TEMP_SEL;
		settings_sel |= BME280_OSR_HUM_SEL;
		settings_sel |= BME280_STANDBY_SEL;
		settings_sel |= BME280_FILTER_SEL;

		rslt = bme280_set_sensor_settings(settings_sel, &b_intf->dev);

		if (rslt != BME280_OK) {
			return rslt;
		}

		rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, &b_intf->dev);

		if (rslt != BME280_OK) {
			return rslt;
		}

		break;

	case BME280_IFACE_FORCED_MODE:
		break;

	default:
		break;
	}

	return 0;
}

int8_t aq_bme280_sample(bme280_intf *b_intf)
{
	int8_t rslt;

	rslt = bme280_get_sensor_data(BME280_ALL, &b_intf->data,
				      &b_intf->dev);

	if (rslt != BME280_OK) {
		return -3;
	}

	return 0;
}

void aq_bme280_print_data(bme280_intf *b_intf, unsigned long millis)
{
	struct bme280_data *d = &b_intf->data;

	printf("{\"sensor\": \"BME280\", "
	       "\"data\": [");
	printf("{\"name\": \"temperature\", "
	       "\"value\": %0.2f, "
	       "\"unit\": \"degC\", "
	       "\"timemillis\": %lu}, ",
	       d->temperature, millis);
	printf("{\"name\": \"pressure\", "
	       "\"value\": %0.2f, "
	       "\"unit\": \"Pa\", "
	       "\"timemillis\": %lu}, ",
	       d->pressure, millis);
	printf("{\"name\": \"humidity\", "
	       "\"value\": %0.2f, "
	       "\"unit\": \"%%\", "
	       "\"timemillis\": %lu}]}",
	       d->humidity, millis);
}

void aq_bme280_handle_error(int8_t i_errno)
{
	char level[16];
	if (i_errno == BME280_OK) {
		return;
	}

	switch (bme280_iface_err_level(i_errno)) {
	case INFO:
		strcpy(level, "INFO");
		break;
	case WARNING:
		strcpy(level, "WARNING");
		write_info_led_color(50, 50, 0);
		break;
	case ERROR:
		strcpy(level, "ERROR");
		write_info_led_color(50, 0, 0);
		break;
	default:
		strcpy(level, "UNKNOWN");
		break;
	}

	printf("%s: %s\n", level,
	       bme280_iface_err_description(i_errno));
}

void aq_pm2_5_print_data(pm2_5_data *d, unsigned long millis)
{
	printf("{\"sensor\": \"PMS 5003\", "
	       "\"data\": [");
	printf("{\"name\": \"PM1.0 Std\", "
	       "\"value\": %u, "
	       "\"unit\": \"ug/m^3\", "
	       "\"timemillis\": %lu}, ",
	       d->pm1_0_std, millis);
	printf("{\"name\": \"PM2.5 Std\", "
	       "\"value\": %u, "
	       "\"unit\": \"ug/m^3\", "
	       "\"timemillis\": %lu}, ",
	       d->pm2_5_std, millis);
	printf("{\"name\": \"pm10_std\", "
	       "\"value\": %u, "
	       "\"unit\": \"ug/m^3\", "
	       "\"timemillis\": %lu}]}",
	       d->pm10_std, millis);
}

void aq_pm2_5_handle_error(int8_t i_errno)
{
	char level[16];
	if (i_errno == BME280_OK) {
		return;
	}

	switch (pm2_5_err_level(i_errno)) {
	case PM2_5_INFO:
		strcpy(level, "INFO");
		break;
	case PM2_5_WARNING:
		strcpy(level, "WARNING");
		write_info_led_color(50, 50, 0);
		break;
	case PM2_5_ERROR:
		strcpy(level, "ERROR");
		write_info_led_color(50, 0, 0);
		break;
	default:
		strcpy(level, "UNKNOWN");
		break;
	}

	printf("%s: %s\n", level,
	       pm2_5_err_description(i_errno));
}

int main() {
	int8_t ret = 0;

	/* Interfaces */
	bme680_intf b_intf;
	bme280_intf b280_intf;
	pm2_5_intf p_intf;

	/* Output Data Structures */
	struct bme68x_data d;
	pm2_5_data pdata;

	/* Configuration Parameters */
	bme680_run_mode m = FORCED_MODE;
	air_quality_led_config led_conf;
	const uint16_t sample_delay_ms = 10000;
	absolute_time_t next_sample_time;

	stdio_init_all();

	init_info_led();
	write_info_led_color(0, 75, 0);

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

	/* initialize variables in interface struct */
	b_intf.i2c = NULL; /* NULL i2c will select default */
	b_intf.timeout = 1000; /* 500ms timeout on i2c read/write */

#ifdef BME680_INTERFACE_SELFTEST
	/* Option to compile in a selft test of sensor at start of
	 * MCU */
	printf("Beginning BME680 Selftest...Standby...\n");
	ret = selftest_bme680_sensor(&b_intf, BME68X_I2C_ADDR_LOW);

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
		ret = init_bme680_sensor(&b_intf, BME68X_I2C_ADDR_LOW, m);

		if (ret >= 0) {
			break;
		}

		air_quality_handle_error(ret);
		sleep_ms(1000);
	}

	/* Start BME280 */
	ret = aq_bme280_init(&b280_intf);
	aq_bme280_handle_error(ret);

	ret = aq_bme280_configure(&b280_intf, BME280_IFACE_NORMAL_MODE);
	aq_bme280_handle_error(ret);

	/* Start PM2_5 Sensor */
	p_intf.uart = AIR_QUALITY_PM2_5_UART;
	ret = pm2_5_intf_init(&p_intf, AIR_QUALITY_PM2_5_TX_PIN,
			      AIR_QUALITY_PM2_5_RX_PIN);
	aq_pm2_5_handle_error(ret);

	ret = pm2_5_set_mode(&p_intf.dev, PM2_5_MODE_PASSIVE);
	aq_pm2_5_handle_error(ret);

	/* Set status led */
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

		if (ret == BME68X_W_NO_NEW_DATA) {
			continue;
		}

		if (ret < 0) {
			air_quality_handle_error(ret);
			break;
		}

		ret = aq_bme280_sample(&b280_intf);
		aq_bme280_handle_error(ret);

		ret = pm2_5_get_data(&p_intf.dev, &pdata);
		aq_pm2_5_handle_error(ret);

		/* Print out all the data */
		printf("{\"program\": \"%s\", \"board\": \"%s\", "
		       "\"output\": [",
		       PICO_TARGET_NAME, PICO_BOARD);
		air_quality_print_data(&d, to_ms_since_boot(readtime));
		printf(", ");
		aq_bme280_print_data(&b280_intf, to_ms_since_boot(readtime));
		printf(", ");
		aq_pm2_5_print_data(&pdata, readtime);
		printf("]}\n");
	}

	/* Deinit i2c if loop broke */
	deinit_bme680_sensor(&b_intf);
	air_quality_status_led_off(&led_conf);
	pm2_5_intf_deinit(&p_intf);

	return 1;
}
