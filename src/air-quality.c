#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/i2c.h"
#include "bme680.h"

typedef enum {
	FORCED_MODE
} bme680_run_mode;

uint8_t bme280_addr = 0xFF;

int8_t bme680_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data,
		       uint16_t len)
{
	HAL_StatusTypeDef retstatus;

	retstatus = HAL_I2C_Read(dev_id, reg_addr, reg_data,
				 (uint16_t) len);

	if (retstatus == HAL_OK) {
		return 0;
	}

	return 1;
}

int8_t bme680_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data,
		       uint16_t len)
{
	HAL_StatusTypeDef retstatus;

	retstatus = HAL_I2C_Write(dev_id, reg_addr, reg_data,
				 (uint16_t) len);

	if (retstatus == HAL_OK) {
		return 0;
	}

	return 1;
}

void bme280_delay_ms(uint32_t period, void *intf_ptr)
{
	k_sleep(K_MSEC(period));
}

struct bme68x_dev bme680_dev = {
	.intf_ptr = &bme280_addr,
	.intf = BME68X_I2C_INTF,
	.amb_temp = 20,
	.read = bme680_i2c_read,
	.write = bme680_i2c_write,
	.delay_us = bme680_delay_ms
};

int init_bme680_sensor(bme680_run_mode mode) {
	uint8_t ret;
	struct bme68x_conf c;
	struct bme68x_heatr_conf h;
	/* Initialize I2C */

	/* Contact BME680 */
	if ((ret = bme68x_init(&bme680_dev)) != BME68X_OK) {
		return 1;
	}

	c.filter = BME68X_FILTER_OFF;
	c.odr = BME68X_ODR_NONE;
	c.os_hum = BME68X_OS_16X;
	c.os_pres = BEM68X_OS_1X;
	c.os_temp = BME68X_OS_2X;

	if ((ret = bme68x_set_conf(&c, &bme680_dev)) != BME68X_OK) {
		return 1;
	}

	h.enable = BME68X_ENABLE;
	h.heatr_temp = 300;
	heatr_conf.heatr_dur = 100;
	
	if ((ret = bme68x_set_heater_conf(BME68X_FORCED_MODE, &h, &bme680_dev)) != BME68X_OK) {
	}

	return 0;
}

int sample_bme680_sensor(bm680_run_mode mode, struct bme68x_conf* c,
			 struct bme68x_heater_conf* h, struct bme68x_data* d) {
	uint8_t ret;
	uint32_t dur;
	uint8_t num_fields;

	switch (mode) {
	case FORCED_MODE:
		ret = bme68x_set_op_mode(BME68X_FORCED_MODE, &bme680_dev);

		if (ret != BME68X_OK) {
			return 1;
		}

		dur = bme68x_get_meas_dur(BME68X_FORCED_MODE, c, &bme680_dev)
			+ (h.heatr_dur * 1000);
		bme680_dev.delay_us(dur, bme680_dev.intf_ptr);
		ret = bme68x_get_data(BME68X_FORCED_MODE, d, &num_fields,
				      &bme680_dev);

		if (ret != BME68X_OK) {
			return 1;
		}

		break;

	default:
		return 1;
	}

	if (n_fields > 0) {
		return 0;
	}

	return 1;
}

int main() {
	bm680_run_mode m = FORCED_MODE;
	int8_t ret = 0;

	while (ret == 0) {
		if (init_bme680_sensor(m) != 0) {
			air_quality_handle_error();
			sleep(100);
		}
	}

	for (;;) {
		if (sample_bme680_sensor(m, &c, &h, &d) != 0) {
			air_quality_handle_error();
			break;
		}

		air_quality_print_data(&d);
	}

	return 1;
}
