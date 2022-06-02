#include "bme680-interface.h"

#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"

int8_t bme680_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
		       uint32_t len, void *intf_ptr)
{
	int num_bytes;
	bme680_intf *intf = (bme680_intf*) intf_ptr;

	if (intf_ptr == NULL) {
		return BME68X_E_NULL_PTR;
	}

	/* Write a byte with the register offset first but don't send
	 * stop. Then begin the read that will start at that offset */
	if (intf->timeout < 0) {
		i2c_write_blocking(intf->i2c, intf->dev_addr,
				   &reg_addr, 1, true);
		num_bytes = i2c_read_blocking(intf->i2c, intf->dev_addr,
					      reg_data, (uint16_t) len, false);
	} else {
		absolute_time_t to;

		to = make_timeout_time_ms(intf->timeout);
		num_bytes = i2c_write_blocking_until(intf->i2c, intf->dev_addr,
						     &reg_addr, 1, true, to);

		if (num_bytes == PICO_ERROR_TIMEOUT) {
			return BME68X_E_COM_FAIL;
		}

		to = make_timeout_time_ms(intf->timeout * len);
		num_bytes = i2c_read_blocking_until(intf->i2c, intf->dev_addr,
						    reg_data, (uint16_t) len,
						    false, to);

		if (num_bytes == PICO_ERROR_TIMEOUT) {
			return BME68X_E_COM_FAIL;
		}
	}

	if (num_bytes == PICO_ERROR_GENERIC) {
		return BME68X_E_COM_FAIL;
	}

	return BME68X_OK;
}

int8_t bme680_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
			uint32_t len, void *intf_ptr)
{
	int num_bytes;
	bme680_intf *intf = (bme680_intf*) intf_ptr;
	uint8_t d[len + 1];

	if (intf_ptr == NULL) {
		return BME68X_E_NULL_PTR;
	}

	d[0] = reg_addr;
	memcpy(&d[1], reg_data, len);

	/* Use timeout only if set with value greater than 0, other
	 * wise fully block */
	if (intf->timeout > 0) {
		absolute_time_t to = make_timeout_time_ms(intf->timeout);

		num_bytes = i2c_write_blocking_until(intf->i2c,
						     intf->dev_addr,
						     d, len + 1,
						     false, to);
	} else {
		num_bytes = i2c_write_blocking(intf->i2c,
					       intf->dev_addr,
					       d, len + 1, false);
	}

	return num_bytes == len + 1 ? BME68X_OK : BME68X_E_COM_FAIL;
}

void bme680_delay_us(uint32_t period, void *intf_ptr)
{
	sleep_us(period);
}

int bme680_init(bme680_intf *b_intf, uint8_t dev_addr,
		bme680_run_mode mode)
{
	uint8_t ret;

	/* Initialize I2C,
	 * note: given baudrate may note match actual */
	if (!b_intf->i2c) {
		b_intf->i2c = i2c_default;
	}

	i2c_init(b_intf->i2c, 500000);

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
		return ret;
	}

	/* Initial configuration settings and check success */
	b_intf->conf.filter = BME68X_FILTER_OFF;
	b_intf->conf.odr = BME68X_ODR_NONE;
	b_intf->conf.os_hum = BME68X_OS_16X;
	b_intf->conf.os_pres = BME68X_OS_1X;
	b_intf->conf.os_temp = BME68X_OS_2X;

	ret = bme68x_set_conf(&b_intf->conf, &b_intf->bme_dev);

	if (ret != BME68X_OK) {
		return ret;
	}

	/* Configure heater sequence and check success */
	b_intf->heatr.enable = BME68X_ENABLE;
	b_intf->heatr.heatr_temp = 300;
	b_intf->heatr.heatr_dur = 100;
	
	ret = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &b_intf->heatr,
				    &b_intf->bme_dev);

	if (ret != BME68X_OK) {
		return ret;
	}

	return 0;
}

int bme680_sample(bme680_run_mode mode, bme680_intf *b_intf,
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
			return ret;
		}

		dur = bme68x_get_meas_dur(BME68X_FORCED_MODE, &b_intf->conf,
					  &b_intf->bme_dev)
			+ (b_intf->heatr.heatr_dur * 1000);

		b_intf->bme_dev.delay_us(dur, b_intf->bme_dev.intf_ptr);
		ret = bme68x_get_data(BME68X_FORCED_MODE, d, &num_fields,
				      &b_intf->bme_dev);

		if (ret != BME68X_OK) {
			return ret;
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

int bme680_deinit(bme680_intf *b_intf)
{
	if (!b_intf->i2c) {
		return 1;
	}

	i2c_deinit(b_intf->i2c);

	return 0;
}

int8_t bme680_selftest(bme680_intf *b_intf, uint8_t dev_addr)
{
	int8_t ret;

	if (!b_intf) {
		return 1;
	}

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

	/* Run BME68x library selftest */
	ret = bme68x_selftest_check(&b_intf->bme_dev);

	bme680_deinit(b_intf);

	return ret;
}
