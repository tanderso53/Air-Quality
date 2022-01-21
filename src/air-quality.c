#include "bme680-interface.h"

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#ifndef AIR_QUALITY_STATUS_LED
#define AIR_QUALITY_STATUS_LED 13
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
void air_quality_print_data(struct bme68x_data *d);

void air_quality_handle_error(int16_t err)
{
	printf("There was an error %d\n", err);
}

void air_quality_print_data(struct bme68x_data *d)
{
	printf("{\"data\": {"
	       "\"temperature\": %.2f, "
	       "\"pressure\": %.2f, "
	       "\"humididity\": %.2f, "
	       "\"gas_resistance\": %.2f}, "
	       "\"status\": {"
	       "\"sensor\": \"%x\"}}",
	       d->temperature, d->pressure, d->humidity,
	       d->gas_resistance, d->status);
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

int main() {
	bme680_intf b_intf;
	bme680_run_mode m = FORCED_MODE;
	struct bme68x_data d;
	int8_t ret = 0;
	air_quality_led_config led_conf;
	const uint16_t sample_delay_ms = 10000;
	absolute_time_t next_sample_time;

	stdio_init_all();

	for (;;) {

		if (stdio_usb_connected()) {
			printf("Welcome! You are connected!\n");
			break;
		}

		sleep_ms(100);
	}

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
	ret = selftest_bme680_sensor(&b_intf, BME68X_I2C_ADDR_HIGH);

	if (ret == BME68X_OK) {
		printf("BME680 Selftest SUCCESS...Continuing...\n");
	} else if (ret > 0) {
		printf("BME680 Selftest WARNING with code %d...Continuing...\n",
		       ret);
	} else {
		printf("BME680 Selftest FAILURE with code %d...Ending...\n",
			ret);
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
		air_quality_status_led_on(&led_conf);

		if (absolute_time_diff_us(next_sample_time, get_absolute_time()) < 0) {
			continue;
		}

		ret = sample_bme680_sensor(m, &b_intf, &d);
		next_sample_time = make_timeout_time_ms(sample_delay_ms);

		if (ret == BME68X_W_NO_NEW_DATA) {
			continue;
		}

		if (ret < 0) {
			air_quality_handle_error(ret);
			break;
		}

		air_quality_print_data(&d);
	}

	/* Deinit i2c if loop broke */
	deinit_bme680_sensor(&b_intf);
	air_quality_status_led_off(&led_conf);

	return 1;
}
