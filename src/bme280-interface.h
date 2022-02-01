#ifndef BME280_INTERFACE_H

#define BME280_INTERFACE_H

#include <bme280.h>

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

	typedef enum {
		BME280_IFACE_FORCED_MODE,
		BME280_IFACE_NORMAL_MODE
	} bme280_op_mode;

	typedef struct {
		struct bme280_dev dev;
		uint8_t addr;
		struct bme280_data data;
	} bme280_intf;

	void user_delay_us(uint32_t period, void *intf_ptr);

	int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
			     uint32_t len, void *intf_ptr);

	int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
			      uint32_t len, void *intf_ptr);

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* #ifndef BME280_INTERFACE_H */
