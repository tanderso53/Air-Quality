#include <bme68x.h>

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"

typedef struct {
	i2c_inst_t *i2c;
	uint8_t dev_addr;
	struct bme68x_dev bme_dev;
	struct bme68x_conf conf;
	struct bme68x_heatr_conf heatr;
} bme680_intf;

typedef enum {
	FORCED_MODE
} bme680_run_mode;

int8_t bme680_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
		       uint32_t len, void *intf_ptr);

int8_t bme680_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
			uint32_t len, void *intf_ptr);

void bme680_delay_us(uint32_t period, void *intf_ptr);

int init_bme680_sensor(bme680_intf *b_intf, uint8_t dev_addr,
		       bme680_run_mode mode);

int sample_bme680_sensor(bme680_run_mode mode, bme680_intf *b_intf,
			 struct bme68x_data *d);

int deinit_bme680_sensor(bme680_intf *b_intf);

void air_quality_handle_error();

void air_quality_print_data(struct bme68x_data *d);

int8_t bme680_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
		       uint32_t len, void *intf_ptr)
{
	int num_bytes;
	bme680_intf *intf = (bme680_intf*) intf_ptr;

	if (intf_ptr == NULL) {
		return 1;
	}

	/* Write a byte with the register offset first but don't send
	 * stop. Then begin the read that will start at that offset */
	i2c_write_blocking(intf->i2c, intf->dev_addr,
			   &reg_addr, 1, true);
	num_bytes = i2c_read_blocking(intf->i2c, intf->dev_addr,
				      reg_data, (uint16_t) len, false);

	if (num_bytes == PICO_ERROR_GENERIC) {
		return PICO_ERROR_GENERIC;
	}

	if ((uint32_t) num_bytes != len) {
		return 1;
	}

	return 0;
}

int8_t bme680_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
			uint32_t len, void *intf_ptr)
{
	int num_bytes;
	bme680_intf *intf = (bme680_intf*) intf_ptr;

	if (intf_ptr == NULL) {
		return 1;
	}

	/* Write a byte with the register offset first but don't send
	 * stop. Then begin the read that will start at that offset */
	i2c_write_blocking(intf->i2c, intf->dev_addr,
			   &reg_addr, 1, true);
	num_bytes = i2c_write_blocking(intf->i2c, intf->dev_addr,
				       reg_data, (uint16_t) len, false);

	if (num_bytes == PICO_ERROR_GENERIC) {
		return PICO_ERROR_GENERIC;
	}

	if ((uint32_t) num_bytes != len) {
		return 1;
	}

	return 0;
}

void bme680_delay_us(uint32_t period, void *intf_ptr)
{
	sleep_us(period);
}

int init_bme680_sensor(bme680_intf *b_intf, uint8_t dev_addr,
		       bme680_run_mode mode)
{
	uint8_t ret;

	/* needed for logging, hopefully board config picks right
	 * input/outputs */
	stdio_init_all();
	
	/* Initialize I2C,
	 * note: given baudrate may note match actual */
	if (!b_intf->i2c) {
		b_intf->i2c = i2c_default;
	}

	i2c_init(b_intf->i2c, 100000);

	/* TODO: Alow gpio of non-default i2c pins to be set up */
	gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
	gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

	b_intf->dev_addr = dev_addr;

	/* Set up BME680 */
	b_intf->bme_dev.intf_ptr = (void*) b_intf;
	b_intf->bme_dev.intf = BME68X_I2C_INTF;
	b_intf->bme_dev.amb_temp = 20;
	b_intf->bme_dev.read = bme680_i2c_read;
	b_intf->bme_dev.write = bme680_i2c_write;
	b_intf->bme_dev.delay_us = bme680_delay_us;

	/* Attempt to make initial contact */
	ret = bme68x_init(&b_intf->bme_dev);

	if (ret != BME68X_OK) {
		return 1;
	}

	/* Initial configuration settings and check success */
	b_intf->conf.filter = BME68X_FILTER_OFF;
	b_intf->conf.odr = BME68X_ODR_NONE;
	b_intf->conf.os_hum = BME68X_OS_16X;
	b_intf->conf.os_pres = BME68X_OS_1X;
	b_intf->conf.os_temp = BME68X_OS_2X;

	ret = bme68x_set_conf(&b_intf->conf, &b_intf->bme_dev);

	if (ret != BME68X_OK) {
		return 1;
	}

	/* Configure heater sequence and check success */
	b_intf->heatr.enable = BME68X_ENABLE;
	b_intf->heatr.heatr_temp = 300;
	b_intf->heatr.heatr_dur = 100;
	
	ret = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &b_intf->heatr,
				    &b_intf->bme_dev);

	if (ret != BME68X_OK) {
		return 1;
	}

	return 0;
}

int sample_bme680_sensor(bme680_run_mode mode, bme680_intf *b_intf,
			 struct bme68x_data *d)
{
	uint8_t ret;
	uint32_t dur;
	uint8_t num_fields;

	switch (mode) {
	case FORCED_MODE:
		ret = bme68x_set_op_mode(BME68X_FORCED_MODE,
					 &b_intf->bme_dev);

		if (ret != BME68X_OK) {
			return 1;
		}

		dur = bme68x_get_meas_dur(BME68X_FORCED_MODE, &b_intf->conf,
					  &b_intf->bme_dev)
			+ (b_intf->heatr.heatr_dur * 1000);

		b_intf->bme_dev.delay_us(dur, b_intf->bme_dev.intf_ptr);
		ret = bme68x_get_data(BME68X_FORCED_MODE, d, &num_fields,
				      &b_intf->bme_dev);

		if (ret != BME68X_OK) {
			return 1;
		}

		break;

	default:
		return 1;
	}

	if (num_fields > 0) {
		return 0;
	}

	return 1;
}

int deinit_bme680_sensor(bme680_intf *b_intf)
{
	if (!b_intf->i2c) {
		return 1;
	}

	i2c_deinit(b_intf->i2c);

	return 0;
}

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
