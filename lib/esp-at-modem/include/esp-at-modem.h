/*
* Copyright (c) 2022 Tyler J. Anderson.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in
*    the documentation and/or other materials provided with the
*    distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
*    contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
* ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file esp-at-modem.h
 *
 * @brief API definitions for a pico-compatible ESP-AT WiFi library
 */

#ifndef ESP_AT_MODEM_H
#define ESP_AT_MODEM_H

#include <stddef.h>
#include <stdint.h>

#include "uart_pio.h"

/**
 * @defgroup espatlibrary RPi Pico ESP-AT WiFi Library
 * @{
 */

#ifndef ESP_AT_MAX_CONN
#define ESP_AT_MAX_CONN 8
#endif

/** @brief AT device status flags */
typedef enum {
	ESP_AT_STATUS_WIFI_CONNECTED = 0x01,
	ESP_AT_STATUS_CIPMUX_ON = 0x02,
	ESP_AT_STATUS_SERVER_ON = 0x04,
	ESP_AT_STATUS_CLIENT_CONNECTED = 0x08,
	ESP_AT_STATUS_AS_CLIENT = 0x10
} esp_at_status_byte;

/** @brief AT CIP connection protocols */
typedef enum {
	ESP_AT_CIP_PROTO_NULL = 0,
	ESP_AT_CIP_PROTO_TCP = 0x01,
	ESP_AT_CIP_PROTO_TCPV6 = 0x02,
	ESP_AT_CIP_PROTO_UDP = 0x04,
	ESP_AT_CIP_PROTO_UDPV6 = 0x08,
	ESP_AT_CIP_PROTO_SSL = 0x10,
	ESP_AT_CIP_PROTO_SSLV6 = 0x20
} esp_at_cip_proto;

/** @brief Structure to describe a client connected to the
 * co-processor */
typedef struct {
	int index; /**< Client position as described by ESP-AT string */
	char ipv4[16]; /**< Client IPv4 address */
	esp_at_cip_proto proto; /**< Connection protocol */
	uint16_t r_port; /**< Remote port */
	uint16_t l_port; /**< Local port */
	uint8_t passive; /**< 1, ESP device is server, 0 it is a client */
} esp_at_clients;

/** @brief Configuration object for WiFi co-processor
 *
 * Pass this structure to the initialization function first to fill
 * out all the information and start the UART. Then it may be used
 * to identify the module in the other library functions.
 */
typedef struct esp_at_cfg_node {
	uart_pio_cfg uart_cfg; /**< UART config structure for module */
	uint en_pin; /**< GPIO pin to use for enable */
	uint reset_pin; /**< GPIO pin to use for reset */
	struct esp_at_cfg_node  *ptr; /**< NULL if uninitialized, this if init */
} esp_at_cfg;

/** @brief Structure with status information on co-processor
 *
 * This structure is intended to be passed to
 * the @ref esp_at_cipstatus function to retrieve status information
 * on connection parameters and system state by the main MCU
 */
typedef struct {
	esp_at_status_byte status; /**< General status byte */
	uint8_t sleep; /**< 0 for awake, 1 for sleep, 2 for deep sleep */
	esp_at_clients cli[ESP_AT_MAX_CONN]; /**< List of connected clients */
	uint16_t ncli; /**< Number of connected clients */
	char ipv4[24]; /**< IP address of co-processor */
	uint16_t port; /**< Port CIP server is listening on */
	char ipv4_gateway[24]; /**< IP address of the gateway */
	char ipv4_netmask[24]; /**< Netmask of local network */
	char ssid[128]; /**< SSID of wireless network */
	esp_at_cfg *cfg; /**< Ptr to the config for this co-proc */
} esp_at_status;

/** @brief Initialize interface to co-processor and test connection
 *
 * @param uart_pio PIO device to use for UART interface
 * @param uart_sm_tx PIO state machine to use for TX
 * @param uart_sm_rx PIO state machine to use for RX
 * @param uart_pin_tx GPIO pin to use for TX
 * @param uart_pin_rx GPIO pin to use for RX
 * @param uart_baud UART communication speed
 * @param en_pin GPIO pin to use for enable
 * @param reset_pin GPIO pin to use for reset
 *
 * @return Number of char returned from test cmd
 */
int esp_at_init_module(esp_at_cfg *cfg, PIO uart_pio, uint uart_sm_tx,
		       uint uart_sm_rx, uint uart_pin_tx,
		       uint uart_pin_rx, uint uart_baud, uint en_pin,
		       uint reset_pin);

/** @brief Configure and turn on cipserver
 *
 * @return 0 on success, <0 on failure
 */
int esp_at_cipserver_init(esp_at_cfg *cfg);

/** @brief Send string to all connected clients
 *
 * @param s C-string to send
 *
 * @param len Number of char to send
 *
 * @param clientlist Initialized @ref esp_at_status object with list
 * of clients. If NULL is passed, a client with index of 0 will be
 * used
 *
 * @return Number of char sent on success
 * @return <0 on failure
 */
int esp_at_cipsend_string(esp_at_cfg *cfg, const char *s, size_t len,
			  esp_at_status *clientlist);

/** @brief Get status information from co-processor
 *
 * @param clientlist @ref esp_at_status structure to fill with data
 *
 * @return Number of clients connected on success
 * @return <0 on failure
 */
int esp_at_cipstatus(esp_at_cfg *cfg, esp_at_status *clientlist);

/** @brief Command co-processor to enter deep sleep
 *
 * @param time_ms Number of ms to sleep before waking up
 *
 * @return 0 on success, non-zero on failure
 */
int esp_at_deep_sleep(esp_at_cfg *cfg, unsigned long time_ms);

/** @brief Command co-processor to enter sleep
 *
 * @return 0 on success, non-zero on failure
 */
int esp_at_sleep(esp_at_cfg *cfg);

/** @brief Command co-processor to wake up from sleep or deep sleep
 *
 * @return 0 on success
 * @return non-zero on failure
 */
int esp_at_wake_up(esp_at_cfg *cfg);

/** @brief Send the provided command and store the response
 *
 * @note Use of the higher level commands in the API is recommended
 * when there is one for the desired effect instead of this one
 *
 * @return Number of characters received on success
 * @return <0 on failure
 */
int esp_at_send_cmd(esp_at_cfg *cfg, const char *cmd, char *rsp,
		    unsigned int len);

/** @brief Open stdio shell to send cmds directly to co-processor
 *
 * Primarily this is for debugging. Type ESP-AT commands on CLI prompt
 * and press enter. The command is passed directly to the co-processor
 * unless it matches a special command such as 'help' or 'exit'
 *
 * The shell will exit and the program will continue when the
 * interpreter receives the 'exit' command.
 */
void esp_at_passthrough(esp_at_cfg *cfg);

/**
 * @}
 */

#endif /* #ifndef ESP_AT_MODEM_H */
