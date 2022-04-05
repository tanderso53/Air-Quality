/**
 * @file bme280-interface-error.h
 *
 * @brief Header to help pico projects translate BME280 Vendor API
 * errors and warnings
 *
 * WARNING: Do not include this file in headers!!!!!!!!
 */

#ifndef BME280_INTERFACE_ERROR_H
#define BME280_INTERFACE_ERROR_H

#include <bme280.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

	enum bme280_interface_error_level {
		ERROR,
		WARNING,
		INFO,
		NOT_FOUND
	};

	/** @brief Struct describing error in BME280 vendor API */
	typedef struct {
		const int8_t errno;
		const enum bme280_interface_error_level level;
		const char* name;
		const char* description;
	} bme280_interface_error;

	/** @brief List of errors that can be produced from BME280
	 * vendor API */
	const bme280_interface_error bme280_interface_errlist[] = {
		{
			.errno = 0,
			.level = INFO,
			.name = "BME280_OK",
			.description = "BME280 OK"
		},

		{
			.errno = -1,
			.level = ERROR,
			.name = "BME280_E_NULL_PTR",
			.description = "BME280 Passed Null Pointer"
		},
		{
			.errno = -2,
			.level = ERROR,
			.name = "BME280_E_DEV_NOT_FOUND",
			.description = "BME280 Device Not Found"
		},

		{
			.errno = -3,
			.level = ERROR,
			.name = "BME280_E_INVALID_LEN",
			.description = "BME280 Invalid Length"
		},

		{
			.errno = -4,
			.level = ERROR,
			.name = "BME280_E_COMM_FAIL",
			.description = "BME280 Communication Failure"
		},

		{
			.errno = -5,
			.level = ERROR,
			.name = "BME280_E_SLEEP_MODE_FAIL",
			.description = "BME280 Failed to Enter Sleep Mode"
		},

		{
			.errno = -6,
			.level = ERROR,
			.name = "BME280_E_NVM_COPY_FAILED",
			.description = "BME280 NVM Copy Failed"
		},

		{
			.errno = 1,
			.level = WARNING,
			.name = "BME280_W_INVALID_OSR_MACRO",
			.description = "BME280 Invalid Oversampling Setting"
		}
	};

	/** @brief Get error level from errno */
	inline enum bme280_interface_error_level bme280_iface_err_level(int8_t iface_errno) {
		const uint8_t size = sizeof(bme280_interface_errlist)/
			sizeof(bme280_interface_errlist[0]);

		for (uint8_t i = 0; i < size; i++) {

			if (bme280_interface_errlist[i].errno == iface_errno) {
				return bme280_interface_errlist[i].level;
			}

		}

		return NOT_FOUND;
	}

	/** @brief Get error name from errno */
	inline const char* bme280_iface_err_name(int8_t iface_errno) {
		const uint8_t size = sizeof(bme280_interface_errlist)/
			sizeof(bme280_interface_errlist[0]);

		for (uint8_t i = 0; i < size; i++) {

			if (bme280_interface_errlist[i].errno == iface_errno) {
				return bme280_interface_errlist[i].name;
			}

		}

		return NULL;
	}

	/** @brief Get error description from errno */
	inline const char* bme280_iface_err_description(int8_t iface_errno) {
		const uint8_t size = sizeof(bme280_interface_errlist)/
			sizeof(bme280_interface_errlist[0]);

		for (uint8_t i = 0; i < size; i++) {

			if (bme280_interface_errlist[i].errno == iface_errno) {
				return bme280_interface_errlist[i].description;
			}

		}

		return NULL;
	}
#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* #define BME280_INTERFACE_ERROR_H */
