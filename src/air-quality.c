#include "bme680-interface.h"

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

/** @brief Print errors to stdout when things go wrong */
void air_quality_handle_error();

/** @brief Print out data from environmental sensors as json string
 * @p d Data struct from bme68x vendor library
 */
void air_quality_print_data(struct bme68x_data *d);

void air_quality_handle_error()
{
	printf("There was an error\n");
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

int main() {
	bme680_intf b_intf;
	bme680_run_mode m = FORCED_MODE;
	struct bme68x_data d;
	int8_t ret = 0;

	/* initialize variables in interface struct */
	b_intf.dev_addr = 0xFF;
	b_intf.i2c = NULL; /* NULL i2c will select default */

	/* Keep trying to connect to sensor until there is a
	 * success */
	while (ret == 0) {
		if (init_bme680_sensor(&b_intf, BME68X_I2C_ADDR_HIGH, m) != 0) {
			air_quality_handle_error();
			sleep_ms(10000);
		}
	}

	/* Keep polling the sensor for data if initialization was
	 * successful. This loop will only break on error. */
	for (;;) {
		if (sample_bme680_sensor(m, &b_intf, &d) != 0) {
			air_quality_handle_error();
			break;
		}

		air_quality_print_data(&d);
	}

	/* Deinit i2c if loop broke */
	deinit_bme680_sensor(&b_intf);

	return 1;
}
