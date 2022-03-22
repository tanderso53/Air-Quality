/**
 * @file esp-at-modem.h
 *
 * @brief API definitions for a pico-compatible ESP-AT WiFi library
 */

#ifndef ESP_AT_MODEM_H
#define ESP_AT_MODEM_H

#include <stddef.h>
#include <stdint.h>

#ifndef ESP_AT_MAX_CONN
#define ESP_AT_MAX_CONN 8
#endif

#ifndef AIR_QUALITY_WIFI_TX_PIN
#define AIR_QUALITY_WIFI_TX_PIN 10
#endif

#ifndef AIR_QUALITY_WIFI_RX_PIN
#define AIR_QUALITY_WIFI_RX_PIN 11
#endif

#ifndef AIR_QUALITY_WIFI_GPIO_EN_PIN
#define AIR_QUALITY_WIFI_GPIO_EN_PIN 12
#endif

#ifndef AIR_QUALITY_WIFI_GPIO_RESET_PIN
#define AIR_QUALITY_WIFI_GPIO_RESET_PIN 13
#endif

#ifndef AIR_QUALITY_WIFI_PIO
#define AIR_QUALITY_WIFI_PIO pio1
#endif

#ifndef AIR_QUALITY_WIFI_BAUD
#define AIR_QUALITY_WIFI_BAUD 115200
#endif

#ifndef AIR_QUALITY_WIFI_TX_SM
#define AIR_QUALITY_WIFI_TX_SM 0
#endif

#ifndef AIR_QUALITY_WIFI_RX_SM
#define AIR_QUALITY_WIFI_RX_SM 1
#endif

/** AT device status flags */
typedef enum {
	ESP_AT_STATUS_WIFI_CONNECTED = 0x01,
	ESP_AT_STATUS_CIPMUX_ON = 0x02,
	ESP_AT_STATUS_SERVER_ON = 0x04,
	ESP_AT_STATUS_CLIENT_CONNECTED = 0x08,
	ESP_AT_STATUS_AS_CLIENT = 0x10
} esp_at_status_byte;

/** AT CIP connection protocols */
typedef enum {
	ESP_AT_CIP_PROTO_NULL = 0,
	ESP_AT_CIP_PROTO_TCP = 0x01,
	ESP_AT_CIP_PROTO_TCPV6 = 0x02,
	ESP_AT_CIP_PROTO_UDP = 0x04,
	ESP_AT_CIP_PROTO_UDPV6 = 0x08,
	ESP_AT_CIP_PROTO_SSL = 0x10,
	ESP_AT_CIP_PROTO_SSLV6 = 0x20
} esp_at_cip_proto;

/** Structure to describe a client connected to the co-processor */
typedef struct {
	int index; /**< Client position as described by ESP-AT string */
	char ipv4[16]; /**< Client IPv4 address */
	esp_at_cip_proto proto; /**< Connection protocol */
	uint16_t r_port; /**< Remote port */
	uint16_t l_port; /**< Local port */
	uint8_t passive; /**< 1, ESP device is server, 0 it is a client */
} esp_at_clients;

/** Structure with status information on co-processor
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
	char ipv4[16]; /**< IP address of co-processor */
	uint16_t port; /**< Port CIP server is listening on */
	char ipv4_gateway[16]; /**< IP address of the gateway */
	char ssid[128]; /**< SSID of wireless network */
} esp_at_status;

/** Initialize interface to co-processor and test connection
 *
 * @return Number of char returned from test cmd
 */
int esp_at_init_module();

/** Configure and turn on cipserver
 *
 * @return 0 on success, <0 on failure
 */
int esp_at_cipserver_init();

/** Send string to all connected clients
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
int esp_at_cipsend_string(const char *s, size_t len,
			  esp_at_status *clientlist);

/** Get status information from co-processor
 *
 * @param clientlist @ref esp_at_status structure to fill with data
 *
 * @return Number of clients connected on success
 * @return <0 on failure
 */
int esp_at_cipstatus(esp_at_status *clientlist);

/** Command co-processor to enter deep sleep
 *
 * @param time_ms Number of ms to sleep before waking up
 *
 * @return 0 on success, non-zero on failure
 */
int esp_at_deep_sleep(unsigned long time_ms);

/** Command co-processor to enter sleep
 *
 * @return 0 on success, non-zero on failure
 */
int esp_at_sleep();

/** Command co-processor to wake up from sleep or deep sleep
 *
 * @return 0 on success
 * @return non-zero on failure
 */
int esp_at_wake_up();

int esp_at_send_cmd(const char *cmd, char *rsp, unsigned int len);

/** Open stdio shell to send cmds directly to co-processor
 *
 * Primarily this is for debugging. Type ESP-AT commands on CLI prompt
 * and press enter. The command is passed directly to the co-processor
 * unless it matches a special command such as 'help' or 'exit'
 *
 * The shell will exit and the program will continue when the
 * interpreter receives the 'exit' command.
 */
void esp_at_passthrough();

#endif /* #ifndef ESP_AT_MODEM_H */
