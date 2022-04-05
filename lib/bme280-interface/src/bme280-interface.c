#include "bme280-interface.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"

/* Note: Template from BME280 documentation */

void user_delay_us(uint32_t period, void *intf_ptr)
{
    /*
     * Return control or wait,
     * for a period amount of milliseconds
     */

	sleep_us((uint32_t) period);
}

int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    int16_t rslt = 0; /* Return 0 for Success, non-zero for failure */
    bme280_intf *intf = (bme280_intf*) intf_ptr;

    /*
     * The parameter intf_ptr can be used as a variable to store the I2C address of the device
     */

    /*
     * Data on the bus should be like
     * |------------+---------------------|
     * | I2C action | Data                |
     * |------------+---------------------|
     * | Start      | -                   |
     * | Write      | (reg_addr)          |
     * | Stop       | -                   |
     * | Start      | -                   |
     * | Read       | (reg_data[0])       |
     * | Read       | (....)              |
     * | Read       | (reg_data[len - 1]) |
     * | Stop       | -                   |
     * |------------+---------------------|
     */

    rslt = i2c_write_blocking(i2c_default, intf->addr, &reg_addr,
			      1, true);

    if (rslt == PICO_ERROR_GENERIC) {
	    return BME280_E_COMM_FAIL;
    }

    rslt = i2c_read_blocking(i2c_default, intf->addr, reg_data, len,
			     false);

    if (rslt == PICO_ERROR_GENERIC) {
	    return BME280_E_COMM_FAIL;
    }

    return 0;
}

int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    int8_t rslt = 0; /* Return 0 for Success, non-zero for failure */
    bme280_intf *intf = (bme280_intf*) intf_ptr;
    uint8_t d[len * 2]; /* Write reg address with each byte */

    /*
     * The parameter intf_ptr can be used as a variable to store the I2C address of the device
     */

    /*
     * Data on the bus should be like
     * |------------+---------------------|
     * | I2C action | Data                |
     * |------------+---------------------|
     * | Start      | -                   |
     * | Write      | (reg_addr)          |
     * | Write      | (reg_data[0])       |
     * | Write      | (....)              |
     * | Write      | (reg_data[len - 1]) |
     * | Stop       | -                   |
     * |------------+---------------------|
     */

    d[0] = reg_addr;

    for (uint8_t i = 0; i < len; i++) {
	    d[i * 2] = reg_addr + i;
	    d[i * 2 + 1] = reg_data[i];
    }

    rslt = i2c_write_blocking(i2c_default, intf->addr, d, len * 2,
			      false);

    if (rslt == PICO_ERROR_GENERIC) {
	    return rslt;
    }

    return 0;
}
