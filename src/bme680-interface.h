#ifndef BME680_INTERFACE_H
#define BME680_INTERFACE_H

#include <bme68x.h>

#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

	/**
	 * @brief BME680 interface configuration struct
	 *
	 * This is passed to the initialize function to fill out
	 */
	typedef struct {
		i2c_inst_t *i2c;
		uint8_t dev_addr;
		int32_t timeout;
		struct bme68x_dev bme_dev;
		struct bme68x_conf conf;
		struct bme68x_heatr_conf heatr;
	} bme680_intf;

	/**
	 * @brief operating modes implemented in the BME68x vendor
	 * library.
	 *
	 * Currently only the forced-mode is implemented in the Pico
	 * SDK interface.
	 */
	typedef enum {
		FORCED_MODE
	} bme680_run_mode;

	/**
	 * @brief Pico-sdk user function to read sensor
	 */
	int8_t bme680_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
			       uint32_t len, void *intf_ptr);

	/**
	 * @brief Pico-sdk user function to write sensor
	 */
	int8_t bme680_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
				uint32_t len, void *intf_ptr);

	/**
	 * @brief Pico-sdk user function for delays by sensor
	 */
	void bme680_delay_us(uint32_t period, void *intf_ptr);

	/**
	 * @brief Initialize and make initial contact with sensor
	 *
	 * @param b_intf is the device interface config struct,
	 * @param dev_addr is the i2c 7-bit device address, and
	 * @param mode is the starting operating mode. Only forced
	 * mode is currently implemented.
	 * Returns 0 if successful and non-zero on error
	 */
	int init_bme680_sensor(bme680_intf *b_intf, uint8_t dev_addr,
			       bme680_run_mode mode);

	/**
	 * @brief Read measurement from sensor and fill @p d struct
	 */
	int sample_bme680_sensor(bme680_run_mode mode, bme680_intf *b_intf,
				 struct bme68x_data *d);

	/**
	 * @brief De-initialize pico-sdk's i2c interface
	 */
	int deinit_bme680_sensor(bme680_intf *b_intf);

	/**
	 * @brief Run BME680 Self Test
	 */
	int8_t selftest_bme680_sensor(bme680_intf *b_intf, uint8_t dev_addr);


#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* #ifndef BME680_INTERFACE_H */
