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
 * @file pm2_5-interface.c
 *
 * @brief Pico interface driver implementation for PMS 5003 PM2.5
 * sensor API
 */

#include "pm2_5-interface.h"

int8_t pm2_5_intf_init(pm2_5_intf *intf, uint tx, uint rx)
{
	int8_t rst;

	if (!intf || !intf->uart) {
		return PM2_5_E_NULL_PTR;
	}

	/* Check uart pointer */
	if (intf->uart != uart1 && intf->uart != uart0) {
		return PM2_5_E_NULL_PTR;
	}

	/* Initialize the uart interface */
	uart_init(intf->uart, PM2_5_DEFAULT_BAUD);
	uart_set_format(intf->uart, 8, PM2_5_STOP_BIT,
			UART_PARITY_NONE);

	/* Set GPIO function */
	gpio_set_function(tx, GPIO_FUNC_UART);
	gpio_set_function(rx, GPIO_FUNC_UART);

	/* Set the callbacks */
	intf->dev.send_cb = pm2_5_user_send;
	intf->dev.receive_cb = pm2_5_user_receive;
	intf->dev.available_cb = pm2_5_user_available;
	intf->dev.intf_ptr = intf;

	rst = pm2_5_init(&intf->dev);

	if (rst != PM2_5_OK) {
		return rst;
	}

	return PM2_5_OK;
}

int8_t pm2_5_intf_deinit(pm2_5_intf *intf)
{
	/* Make sure the pico uart driver is enabled */
	if (!uart_is_enabled(intf->uart)) {
		return PM2_5_E_COMM_FAILURE;
	}

	/* deinit uart interface */
	uart_deinit(intf->uart);

	return PM2_5_OK;
}

int8_t pm2_5_user_send(const uint8_t *data, uint8_t len,
		       void *intf_ptr)
{
	pm2_5_intf *i_ptr = (pm2_5_intf*) intf_ptr;

	/* Make sure the pico uart driver is enabled */
	if (!uart_is_enabled(i_ptr->uart)) {
		return -1;
	}

	/* Checks if TX FIFO has space to write */
	if (!uart_is_writable(i_ptr->uart)) {
		return -1;
	}

	uart_write_blocking(i_ptr->uart, data, len);

	return 0;
}

int8_t pm2_5_user_receive(uint8_t *data,
			  uint8_t len, void *intf_ptr)
{
	pm2_5_intf *i_ptr = (pm2_5_intf*) intf_ptr;

	/* Make sure the pico uart driver is enabled */
	if (!uart_is_enabled(i_ptr->uart)) {
		return -1;
	}

	/* Time-out if no data found within timeout period */
	for (uint8_t i = 0; i < len; i++) {

		if (uart_is_readable_within_us(i_ptr->uart, PM2_5_INTERFACE_TIMEOUT_US)) {
			uart_read_blocking(i_ptr->uart, &data[i], 1);
		} else {
			return -1;
		}
	}

	return 0;
}

uint8_t pm2_5_user_available(void *intf_ptr)
{
	pm2_5_intf *i_ptr = (pm2_5_intf*) intf_ptr;

	return uart_is_readable(i_ptr->uart) ? 1 : 0;
}

