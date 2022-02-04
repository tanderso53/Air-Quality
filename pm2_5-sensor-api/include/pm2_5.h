/*
 * Copyright (c) 2022 Tyler J. Anderson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 *    3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file pm2_5.h
 *
 * @brief API for generalized PMS 5003 PM2.5 sensor UART communication
 */

#ifndef PM2_5_H
#define PM2_5_H

#include <stdlib.h>
#include <stdint.h>

/* UART API errors, warnings and return codes */
#define PM2_5_OK		 0
#define PM2_5_E_NULL_PTR	-1
#define PM2_5_E_COMM_FAILURE	-2
#define PM2_5_E_BAD_CHECKSUM	-3
#define PM2_5_E_ARRAY_LEN	-4
#define PM2_5_E_BAD_CMD		-5
#define PM2_5_E_BAD_START_BYTES	-6
#define PM2_5_W_NO_DATA		 1

/* UART Communication definitions */
#define PM2_5_DEFAULT_BAUD 9600
#define PM2_5_CHECK_BIT 0
#define PM2_5_STOP_BIT 1

/* Mode masks */
#define PM2_5_MODE_ACTIVE 0x00
#define PM2_5_MODE_PASSIVE 0x01

/* Command transmissions */
#define PM2_5_CMD_PASSIVE_READ 0xe2
#define PM2_5_CMD_CHANGE_MODE 0xe1
#define PM2_5_CMD_SET_SLEEP 0xe4

#define PM2_5_ARG_NO_ARG 0x0000
#define PM2_5_ARG_CHANGE_MODE_PASSIVE 0x0000
#define PM2_5_ARG_CHANGE_MODE_ACTIVE 0x0001
#define PM2_5_ARG_SET_SLEEP_SLEEP 0x0000
#define PM2_5_ARG_SET_SLEEP_WAKEUP 0x0001

#define PM2_5_CMD_LEN_BYTES 7

/* Received data definitions */
#define PM2_5_START_BYTE_1 0x42
#define PM2_5_START_BYTE_2 0x4d
#define PM2_5_RX_DATA_LEN_BYTES 32
#define PM2_5_RX_DATA_BIT_DEPTH 16
#define PM2_5_RX_DATA_NUM_POINTS 13

/* Data Positions */
#define PM2_5_POS_PM1_0_STD 0
#define PM2_5_POS_PM2_5_STD 1
#define PM2_5_POS_PM10_STD 2
#define PM2_5_POS_PM1_0_ATM 3
#define PM2_5_POS_PM2_5_ATM 4
#define PM2_5_POS_UNIT_ATM 5
#define PM2_5_POS_NP_0_3 6
#define PM2_5_POS_NP_0_5 7
#define PM2_5_POS_NP_1_0 8
#define PM2_5_POS_NP_2_5 9
#define PM2_5_POS_NP_5_0 10
#define PM2_5_POS_NP_10 11
#define PM2_5_POS_NP_RES 12

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __CPLUSPLUS */

/*
**********************************************************************
********************** USER FUNCTION POINTERS ************************
**********************************************************************
*/
	/**
	 * @brief User function pointer to send data to UART
	 *
	 * @code
	 * int8_t pm2_5_user_send(const uint8_t *data,
	 *                        uint8_t len, void *intf_ptr)
	 * {
	 *         Your interface code here
	 *
	 *         return 0 on success, return otherwise on failure
	 * }
	 * @endcode
	 */
	typedef int8_t (*pm2_5_send_ptr)(const uint8_t*,
					 uint8_t, void*);

	/**
	 * @brief User function pointer to receive data to UART
	 *
	 * @code
	 * int8_t pm2_5_user_receive(uint8_t *data,
	 *                           uint8_t len, void *intf_ptr)
	 * {
	 *         Your interface code here
	 *
	 *         return 0 on success, return otherwise on failure
	 * }
	 * @endcode
	 */
	typedef int8_t (*pm2_5_receive_ptr)(uint8_t*,
					    uint8_t, void*);

	/**
	 * @brief User function pointer to check if UART data is
	 * available
	 *
	 * @code
	 * uint8_t pm2_5_user_available(void *intf_ptr)
	 * {
	 *         Your interface code here
	 *
	 *         return 0 if no data, return 1 if data
	 * }
	 * @endcode
	 */
	typedef uint8_t (*pm2_5_available_ptr)(void*);

/*
**********************************************************************
******************* PM2.5 API OBJECT DEFINITIONS *********************
**********************************************************************
*/

	/**
	 * @brief device identification object
	 */
	typedef struct pm2_5_dev_node {
		void *intf_ptr;
		pm2_5_send_ptr send_cb;
		pm2_5_receive_ptr receive_cb;
		pm2_5_available_ptr available_cb;
		uint8_t mode;
		uint8_t sleep;
	} pm2_5_dev;

	/**
	 * @brief Struct to hold parsed data from sensor output stream
	 *
	 * @var pm1_0_std PM1.0 concentration in ug/m^3, CF = 1
	 *
	 * @var pm2_5_std PM2.5 concentration in ug/m^3, CF = 1
	 *
	 * @var pm10_std PM10 concentration in ug/m^3, CF = 1
	 *
	 * @var pm1_0_atm PM1.0 concentration under atmospheric
	 * environment in ug/m^3
	 *
	 * @var pm2_5_atm PM2.5 concentration under atmospheric
	 * environment in ug/m^3
	 *
	 * @var atm_unit Concentration unit for atmospheric environment
	 * (ug/m^3)
	 *
	 * @var np_0_3 Number of particles with diameter greater than
	 * 0.3 um in 0.1 L of air
	 *
	 * @var np_0_5 Number of particles with diameter greater than
	 * 0.5 um in 0.1 L of air
	 *
	 * @var np_1_0 Number of particles with diameter greater than
	 * 1.0 um in 0.1 L of air
	 *
	 * @var np_2_5 Number of particles with diameter greater than
	 * 2.5 um in 0.1 L of air
	 *
	 * @var np_5_0 Number of particles with diameter greater than
	 * 5.0 um in 0.1 L of air
	 *
	 * @var np_10 Number of particles with diameter greater than
	 * 10 um in 0.1 L of air
	 */
	typedef struct pm2_5_data_node {
		uint16_t pm1_0_std;
		uint16_t pm2_5_std;
		uint16_t pm10_std;
		uint16_t pm1_0_atm;
		uint16_t pm2_5_atm;
		uint16_t atm_unit;
		uint16_t np_0_3;
		uint16_t np_0_5;
		uint16_t np_1_0;
		uint16_t np_2_5;
		uint16_t np_5_0;
		uint16_t np_10;
	} pm2_5_data;

/*
**********************************************************************
************************ PM2.5 API FUNCTIONS *************************
**********************************************************************
*/

	/**
	 * @brief initialize pm2_5 object
	 */
	int8_t pm2_5_init(pm2_5_dev *dev);

	/**
	 * @brief Set PM2.5 sensor mode to active or passive
	 *
	 * In active mode, the sensor will push out new datasets
	 * on the UART each 800ms to 2.5s, depending on how much the
	 * values change. In passive mode the sensor will send data
	 * only when asked.
	 *
	 * @param dev The device object to act on
	 *
	 * @param mode Either PM2_5_MODE_ACTIVE or PM2_5_MODE_PASSIVE,
	 * Active mode is default
	 */
	int8_t pm2_5_set_mode(pm2_5_dev *dev, uint8_t mode);

	/**
	 * @brief Fill data structure with the next data from sensor
	 */
	int8_t pm2_5_get_data(pm2_5_dev *dev, pm2_5_data *data);

	/**
	 * @brief Put sensor to sleep
	 */
	int8_t pm2_5_sleep(pm2_5_dev *dev);

	/**
	 * @brief Wake up sensor
	 */
	int8_t pm2_5_wake(pm2_5_dev *dev);

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* #ifndef PM2_5_H */
