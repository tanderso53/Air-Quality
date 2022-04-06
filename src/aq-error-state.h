/**
 * @file aq-error-state.h
 * @author Tyler J. Anderson
 * @brief Standardized status codes and status LED colors for Air
 * Quality
 */

#ifndef AQ_ERROR_STATE_H
#define AQ_ERROR_STATE_H

#include "ws2812.pio.h"

#include <stdint.h>

#include "pico/stdlib.h"

/**
 * @defgroup aq-status Air Quality Status Tracking
 * @{
 */

/** @brief Type for all status conditions and masks */
#define STATUS_TYPE				uint32_t
#define S_T(v)					((STATUS_TYPE) v)

/* List of codes */
/* Empty status */
#define AQ_STATUS_OK				0

/* Board */
#define AQ_STATUS_W_BATT_LOW			S_T(0x00000001)
#define AQ_STATUS_U_REQ_USB			S_T(0x00000002)
#define AQ_STATUS_U_REQ_USER_INPUT		S_T(0x00000004)

/* Networking/WiFi */
#define AQ_STATUS_W_WIFI_DISCONNECTED		S_T(0x01000000)
#define AQ_STATUS_I_CLIENT_CONNECTED		S_T(0x02000000)
#define AQ_STATUS_E_WIFI_FAIL			S_T(0x04000000)

/* BME280 Temp/Humidity/Pressure */
#define AQ_STATUS_E_BME280_COMM_FAIL		S_T(0x00010000)
#define AQ_STATUS_E_BME280_SLEEP_FAIL		S_T(0x00020000)
#define AQ_STATUS_E_BME280_NVM_FAIL		S_T(0x00040000)
#define AQ_STATUS_E_BME280_NOT_FOUND		S_T(0x00080000)
#define AQ_STATUS_E_BME280_GENERAL_FAIL		S_T(0x00100000)
#define AQ_STATUS_W_BME280_OSR_INVALID		S_T(0x00200000)
#define AQ_STATUS_I_BME280_READING		S_T(0x00400000)

/* BME680 Gas Sensor */
#define AQ_STATUS_E_BME680_SELFTEST_FAIL	S_T(0x00000100)
#define AQ_STATUS_E_BME680_COMM_FAIL		S_T(0x00000200)
#define AQ_STATUS_E_BME680_GENERAL_FAIL		S_T(0x00000400)
#define AQ_STATUS_W_BME680_GAS_INVALID		S_T(0x00000800)
#define AQ_STATUS_W_BME680_GAS_UNSTABLE		S_T(0x00001000)
#define AQ_STATUS_I_BME680_READING		S_T(0x00002000)

/* PM2.5 Particle sensor */
#define AQ_STATUS_E_PM2_5_COMM_FAIL		S_T(0x00000010)
#define AQ_STATUS_E_PM2_5_GENERAL_FAIL		S_T(0x00000020)
#define AQ_STATUS_W_PM2_5_NO_DATA		S_T(0x00000040)
#define AQ_STATUS_I_PM2_5_READING		S_T(0x00000080)

/* Masks to define current state */
#define AQ_STATUS_MASK_WAIT			\
	(AQ_STATUS_U_REQ_USB |			\
	 AQ_STATUS_U_REQ_USER_INPUT)

#define AQ_STATUS_MASK_INFO			\
	(AQ_STATUS_I_CLIENT_CONNECTED |		\
	 AQ_STATUS_I_BME280_READING |		\
	 AQ_STATUS_I_BME680_READING |		\
	 AQ_STATUS_I_PM2_5_READING)

#define AQ_STATUS_MASK_WARNING			\
	(AQ_STATUS_W_BATT_LOW |			\
	 AQ_STATUS_W_WIFI_DISCONNECTED |	\
	 AQ_STATUS_W_BME280_OSR_INVALID |	\
	 AQ_STATUS_W_BME680_GAS_INVALID |	\
	 AQ_STATUS_W_BME680_GAS_UNSTABLE |	\
	 AQ_STATUS_W_PM2_5_NO_DATA)

#define AQ_STATUS_MASK_ERROR			\
	(~(AQ_STATUS_MASK_WAIT |		\
	   AQ_STATUS_MASK_INFO |		\
	   AQ_STATUS_MASK_WARNING))

/* Masks to define device status regions */
#define AQ_STATUS_REGION_BOARD			S_T(0x0000000f)
#define AQ_STATUS_REGION_WIFI			S_T(0xff000000)
#define AQ_STATUS_REGION_BME280			S_T(0x00ff0000)
#define AQ_STATUS_REGION_BME680			S_T(0x0000ff00)
#define AQ_STATUS_REGION_PM2_5			S_T(0x000000f0)

/* Color definitions, 24-bit RGB */
#define AQ_STATUS_COLOR_OK			0x001400
#define AQ_STATUS_COLOR_WAIT			0x000014
#define AQ_STATUS_COLOR_INFO			0x001400
#define AQ_STATUS_COLOR_WARNING			0x0a0a00
#define AQ_STATUS_COLOR_ERROR			0x140000

/** @brief Air Quality program status object interface */
typedef struct {
	PIO led_pio; /**< @brief PIO to use */
	uint led_sm; /**< @brief PIO State Machine to use */
	uint led_pin; /**< @brief GPIO pin attached to LED */
	STATUS_TYPE status; /**< @brief Status register for project */
	uint32_t led_rgb; /**< @brief Current LED color */
} aq_status;

/** @brief Initialize status LED */
void aq_status_init(aq_status *s);

/** @brief Write 24 bit RGB code to status LED */
void aq_status_write_color(uint32_t rgb, aq_status *s);

/** @brief Apply logical OR to status and update LED */
void aq_status_set_status(STATUS_TYPE status, aq_status *s);

/** @brief Apply logical NOT AND to status and update LED */
void aq_status_unset_status(STATUS_TYPE status, aq_status *s);

/** @brief Clear all status bits and update LED */
void aq_status_clear(aq_status *s);

/**
 * @}
 */

#endif /* #ifndef AQ_ERROR_STATE_H */
