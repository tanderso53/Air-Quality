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
 * @file pm2_5-interface.h
 *
 * @brief Pico interface driver for PMS 5003 PM2.5 sensor API
 */

#ifndef PM2_5_INTERFACE_H
#define PM2_5_INTERFACE_H

#include <pm2_5.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"

#ifndef PM2_5_INTERFACE_TIMEOUT_US
#define PM2_5_INTERFACE_TIMEOUT_US 500000
#endif

#ifndef PM2_5_INTERFACE_GPIO_EN_PIN
#define PM2_5_INTERFACE_GPIO_EN_PIN 6
#endif

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

	/**
	 * @brief interface object to describe a sensor
	 */
	typedef struct pm2_5_intf_node {
		pm2_5_dev dev;
		uart_inst_t *uart;
	} pm2_5_intf;

	int8_t pm2_5_intf_init(pm2_5_intf *intf, uint tx, uint rx);

	int8_t pm2_5_intf_deinit(pm2_5_intf *intf);

	int8_t pm2_5_user_send(const uint8_t *data, uint8_t len,
			       void *intf_ptr);

	int8_t pm2_5_user_receive(uint8_t *data,
				  uint8_t len, void *intf_ptr);

	uint8_t pm2_5_user_available(void *intf_ptr);

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* #ifndef PM2_5_INTERFACE_H */
