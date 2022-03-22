#include "esp-at-modem.h"
#include "at-parse.h"
#include "debugmsg.h"

#include "uart_tx.pio.h"
#include "uart_rx.pio.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "pico/stdlib.h"

#define _ESP_EN_DELAY_US 500000
#define _ESP_RESET_HOLD_US 20000
#define _ESP_RESPONSE_BUFFER_LEN 2048

#define ARRAY_LEN(array) sizeof(array)/sizeof(array[0])

static int _esp_query(const char * cmd, at_rsp_lines *rsp);

static void _esp_en_gpio_setup();
static void _esp_reset_gpio_setup();
static void _esp_set_enabled(bool en); /* True to enable, false to disable */
static void _esp_reset();
static int _esp_cipsend_data(const char *data, size_t len,
			     unsigned int client_index);

int esp_at_init_module()
{
	uint offset;
	const unsigned int rxlen = 64;
	char rxbuf[rxlen];

	DEBUGMSG("Initializing WiFi");

	/* Initialize gpio pins for enable and reset */
	_esp_en_gpio_setup();
	_esp_reset_gpio_setup();

	/* Set up TX */
	offset = pio_add_program(AIR_QUALITY_WIFI_PIO, &uart_tx_program);

	uart_tx_program_init(AIR_QUALITY_WIFI_PIO, AIR_QUALITY_WIFI_TX_SM,
			     offset, AIR_QUALITY_WIFI_TX_PIN,
			     AIR_QUALITY_WIFI_BAUD);

	/* Set up RX */
	offset = pio_add_program(AIR_QUALITY_WIFI_PIO, &uart_rx_program);

	uart_rx_program_init(AIR_QUALITY_WIFI_PIO, AIR_QUALITY_WIFI_RX_SM,
			     offset, AIR_QUALITY_WIFI_RX_PIN,
			     AIR_QUALITY_WIFI_BAUD);

	/* Reset and enable ESP8266 */
	_esp_set_enabled(true);
	_esp_reset();
	sleep_us(_ESP_EN_DELAY_US); /* Module may need time to boot */

	/* Check connection */
	return esp_at_send_cmd("AT", rxbuf, rxlen);
}

int esp_at_cipserver_init()
{
	int ret;
	char rsp[_ESP_RESPONSE_BUFFER_LEN];

	/* Set up server */
	ret = esp_at_send_cmd("AT+CIPMUX=1", rsp, ARRAY_LEN(rsp));
	DEBUGDATA("When muxing ESP", rsp, "%s");

	if (ret < 0) {
		return ret;
	}

	ret = esp_at_send_cmd("AT+CIPSERVER=1", rsp, ARRAY_LEN(rsp));

	if (ret < 0) {
		return ret;
	}

	return 0;
}

int esp_at_cipsend_string(const char *s, size_t len,
			  esp_at_status *clientlist)
{
	int ret;

	if (strnlen(s, len) == 0)
		return 0;

	if (!clientlist) {
		ret = _esp_cipsend_data(s, len, 0);
		return ret;
	}

	for (unsigned int i = 0; i < clientlist->ncli; ++i) {
		unsigned int ci = clientlist->cli[i].index;

		ret = _esp_cipsend_data(s, len, ci);

		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

int esp_at_cipstatus(esp_at_status *clientlist)
{
	int ret;
	at_rsp_lines rsp;

	do {
		at_rsp_tk *ipv4;
		at_rsp_tk *gateway;

		/* Get WiFi IP address */
		ret = _esp_query("AT+CIPSTA?", &rsp);

		if (ret < 0) {
			return ret;
		}

		ipv4 = &at_rsp_get_property("ipv4", &rsp)->tokenlist[0];
		gateway = &at_rsp_get_property("ipv4", &rsp)->tokenlist[0];

		if (! (ipv4 && gateway)) {
			clientlist->status &= ~ESP_AT_STATUS_WIFI_CONNECTED;
			clientlist->ipv4[0] ='\0';
			clientlist->ipv4_gateway[0] = '\0';
			break;
		}

		clientlist->status |= ESP_AT_STATUS_WIFI_CONNECTED;
		strncpy(clientlist->ipv4, at_rsp_token_as_str(ipv4),
			sizeof(clientlist->ipv4) - 1);
		clientlist->ipv4[ARRAY_LEN(clientlist->ipv4) - 1] = '\0';
		strncpy(clientlist->ipv4_gateway,
			at_rsp_token_as_str(gateway),
			sizeof(clientlist->ipv4_gateway) - 1);
		clientlist->ipv4_gateway[ARRAY_LEN(clientlist->ipv4_gateway) - 1] = '\0';
	} while (0);

	do {
		at_rsp_tk *status;

		/* Some ESP8266 modules don't support AT+CIPSTATE, so use
		 * AT+CIPSTATUS to get most of the networking info */
		ret = _esp_query("AT+CIPSTATUS", &rsp);

		if (ret < 0) {
			return ret;
		}

		status = &at_rsp_get_property("STATUS", &rsp)->tokenlist[0];

		switch (at_rsp_token_as_int(status)) {
		case 0:
		case 1:
		case 5:
			clientlist->status &= ~(ESP_AT_STATUS_SERVER_ON |
						ESP_AT_STATUS_CLIENT_CONNECTED);
			break;
		case 2:
		case 3:
		case 4:
			clientlist->status |= ESP_AT_STATUS_SERVER_ON;
			break;
		default:
			return -1; /* malformed case */
		}

		/* Clients must be zeroed out before re-examining
		 * them */
		clientlist->ncli = 0;
		clientlist->status &= ~(ESP_AT_STATUS_CLIENT_CONNECTED |
					ESP_AT_STATUS_AS_CLIENT);

		for (unsigned int i = 0; i < rsp.nlines; ++i) {
			esp_at_clients *cptr;
			const char *preamble = rsp.tokenlists[i].preamble;
			at_rsp_tk *tkptr;
			char proto[6] = {'\0'};

			DEBUGDATA("CIPSTATUS line", i, "%u");
			DEBUGDATA("Preamble", preamble, "%s");

			tkptr = rsp.tokenlists[i].tokenlist;

			if (strcmp(preamble, "+CIPSTATUS") != 0) {
				DEBUGDATA("Doesn't match +CIPSTATUS",
					  i, "%u");
				continue;
			}

			cptr = &clientlist->cli[clientlist->ncli];
			cptr->index = at_rsp_token_as_int(&tkptr[0]);
			DEBUGDATA("Working on index", cptr->index, "%d");

			/* protocol requires furthur processing
			 * later */
			strncpy(proto, at_rsp_token_as_str(&tkptr[1]),
				ARRAY_LEN(proto) - 1);

			strncpy(cptr->ipv4,
				at_rsp_token_as_str(&tkptr[2]),
				sizeof(cptr->ipv4) - 1);
			cptr->r_port = at_rsp_token_as_int(&tkptr[3]);
			cptr->l_port = at_rsp_token_as_int(&tkptr[4]);
			cptr->passive = at_rsp_token_as_int(&tkptr[5]);

			if (cptr->passive) {
				clientlist->status |= ESP_AT_STATUS_CLIENT_CONNECTED;
			} else {
				clientlist->status |= ESP_AT_STATUS_AS_CLIENT;
			}

			/* This is the furthur processing for
			 * protocol */
			if (strcmp(proto, "TCP") == 0) {
				cptr->proto = ESP_AT_CIP_PROTO_TCP;
			} else if (strcmp(proto, "UDP") == 0) {
				cptr->proto = ESP_AT_CIP_PROTO_UDP;
			} else if (strcmp(proto, "SSL") == 0) {
				cptr->proto = ESP_AT_CIP_PROTO_SSL;
			} else {
				cptr->proto = ESP_AT_CIP_PROTO_NULL;
			}

			++clientlist->ncli;
		}
	} while (0);

	do {
		at_rsp_tk *value;
		esp_at_status_byte *status = &clientlist->status;
		/* Query specific server parameters */
		ret = _esp_query("AT+CIPMUX?", &rsp);

		if (ret < 0) {
			return ret;
		}

		value = &at_rsp_get_property("+CIPMUX", &rsp)->tokenlist[0];

		if (at_rsp_token_as_int(value)) {
			*status |= ESP_AT_STATUS_CIPMUX_ON;
		} else {
			*status &= ~ESP_AT_STATUS_CIPMUX_ON;
		}
	} while (0);

	return 0;
}

int esp_at_send_cmd(const char *cmd, char *rsp, unsigned int len)
{
	DEBUGDATA("Sending AT command", cmd, "%s");

	uart_tx_program_puts(AIR_QUALITY_WIFI_PIO, AIR_QUALITY_WIFI_TX_SM,
			     cmd);

	uart_tx_program_puts(AIR_QUALITY_WIFI_PIO, AIR_QUALITY_WIFI_TX_SM,
			     "\r\n");

	DEBUGMSG("Checking AT response");

	for (unsigned int i = 0; i < len - 1; i++) {

		char c;

		c = uart_rx_program_getc(AIR_QUALITY_WIFI_PIO,
					 AIR_QUALITY_WIFI_RX_SM);

		rsp[i] = c;

		if (i > 0 && rsp[i - 1] == '\r' && rsp[i] == '\n') {
			DEBUGMSG("Found AT end sequence");
			if (memcmp(&rsp[i - 3], "OK", sizeof(char) * 2) == 0) {
				if (i + 1 < len) {
					rsp[i + 1] = '\0';
				}

				DEBUGDATA("Received AT response OK for command",
					  cmd, "%s");

				return 0;
			}

			if (memcmp(&rsp[i - 6], "ERROR", sizeof(char) * 5) == 0) {

				if (i + 1 < len) {
					rsp[i + 1] = '\0';
				}

				DEBUGDATA("Received AT response ERROR for command",
					  cmd, "%s");
			
				return -1;
			}

			rsp[i+1] = '\0';

			DEBUGDATA("AT Response so far", rsp, "%s");
		}
	}

	DEBUGDATA("Received no AT response before buffer filled command",
		  cmd, "%s");

	return len;
}

void esp_at_passthrough()
{
	char cmd[128];
	size_t i = 0;
	const char *prompt = "Prompt> ";

	printf("Initializing ESP-AT Command Passthrough...\n" "%s",
	       prompt);

	for (;;) {
		char c;
		int rst;

		rst = getchar_timeout_us(30000);

		/* Ignore timeouts */
		if (rst == PICO_ERROR_TIMEOUT)
			continue;

		c = (char) rst;

		/* DEBUGDATA("Received char", c, "%c"); */

		switch (c) {
		case '\n':
		case '\r':
			cmd[i] = '\0';
			printf("\n");

			/* User can break loop if desired */
			if (strcmp(cmd, "exit") == 0) {
				return;
			}

			/* Provide a help message */
			if (strcmp(cmd, "help") == 0) {
				printf("Help comes to those who ask for"
				       " it.\n"
				       "All commands are passed to WiFi"
				       " co-MCU, except the following\n\n"
				       "Commands:\n"
				       "exit	Break loop and run main program\n"
				       "help	Print this message\n");
			} else if (strlen(cmd) == 0) {
				/* Index to zero, string to empty */
				i = 0;
				cmd[0] = '\0';
				printf("%s", prompt);

				break;
			} else {
				char rsp[258];

				/* Send command to ESP module */
				esp_at_send_cmd(cmd, rsp, ARRAY_LEN(rsp));
				printf("%s", rsp);
			}

			/* Index to zero, string to empty */
			i = 0;
			cmd[0] = '\0';
			printf("%s", prompt);

			break;
		case 127: /* Backspace */
			/* Only backspace if buffer isn't empty */
			if (i > 0) {
				printf("\b");
				--i;
			}

			break;
		default:
			cmd[i] = c;

			/* Echo character */
			printf("%c", c);

			++i;

			break;
		}
	}
}

int esp_at_deep_sleep(unsigned long time_ms)
{
	char buf[512] = {'\0'};
	char cmd[36] = {'\0'};

	snprintf(cmd, sizeof(cmd) - 1, "AT+GSLP=%lu",
		time_ms);

	return esp_at_send_cmd(cmd, buf, sizeof(buf));
}

int esp_at_sleep()
{
	char buf[512] = {'\0'};

	return esp_at_send_cmd("AT+SLEEP=1", buf, sizeof(buf));
}

int esp_at_wake_up()
{
	char buf[512] = {'\0'};

	return esp_at_send_cmd("AT+SLEEP=0", buf, sizeof(buf));
}

/*
**********************************************************************
******************* INTERNAL IMPLEMENTATION **************************
**********************************************************************
*/

void _esp_en_gpio_setup()
{
	const uint gpin = AIR_QUALITY_WIFI_GPIO_EN_PIN;

	gpio_init(gpin);

	/* Set GPIO settings */
	gpio_set_dir(gpin, true);
	gpio_disable_pulls(gpin);

	/* Set GPIO low to start disabled */
	gpio_put(gpin, false);
}

void _esp_reset_gpio_setup()
{
	const uint gpin = AIR_QUALITY_WIFI_GPIO_RESET_PIN;

	gpio_init(gpin);

	/* Set GPIO settings */
	gpio_set_dir(gpin, true);
	gpio_disable_pulls(gpin);

	/* Set GPIO high to start in the non-reset position */
	gpio_put(gpin, true);
}

void _esp_set_enabled(bool en)
{
	const uint gpin = AIR_QUALITY_WIFI_GPIO_EN_PIN;

	/* High enabled, low disabled */
	gpio_put(gpin, en);
}

void _esp_reset()
{
	const uint gpin = AIR_QUALITY_WIFI_GPIO_RESET_PIN;

	gpio_put(gpin, false); /* Drive low to reset module */

	sleep_us(_ESP_RESET_HOLD_US); /* Hold down reset for a bit */

	gpio_put(gpin, true); /* Return reset pin to normal */
}

int _esp_parse_cw_wifi_state(esp_at_status *clientlist)
{
	int ret;
	char rsp[_ESP_RESPONSE_BUFFER_LEN];
	const char *re_str = "+CWSTATE:";
	char v[24][6];
	int n = 0;

	/* Get WiFi connection state */
	ret = esp_at_send_cmd("AT+CWSTATE?", rsp, ARRAY_LEN(rsp));

	if (ret < 0) {
		return ret;
	}

	/* Match the response */
	char *addr = strstr(rsp, re_str);

	if (!addr) {
		return -1;
	}

	for (char *si = strtok(&addr[1], ","); si; strtok(si, ",")) {
		strcpy(v[n], si);
		++n;
	}

	if (n == 0) {
		DEBUGDATA("Failed to parse", rsp, "%s");
		return -1;
	}

	int status = strtol(v[0], NULL, 10);

	switch (status) {
	case 0:
	case 1:
	case 3:
	case 4:
		clientlist->status &= ~ESP_AT_STATUS_WIFI_CONNECTED;
		return 0;
	case 2:
		clientlist->status |= ESP_AT_STATUS_WIFI_CONNECTED;
		break;
	default:
		return -1;
	}

	if (n > 1) {
		strncpy(clientlist->ssid, v[1],
			sizeof(clientlist->ssid) - 1);

		clientlist->ssid[ARRAY_LEN(clientlist->ssid) - 1] = '\0';
	}

	return 0;
}

int _esp_query(const char * cmd, at_rsp_lines *rsp)
{
	int ret;
	char buf[4096];

	ret = esp_at_send_cmd(cmd, buf, ARRAY_LEN(buf));

	if (ret < 0) {
		return -1;
	}

	ret = at_rsp_get_lines(buf, rsp);

	return ret;
}

int _esp_cipsend_data(const char *data, size_t len,
		      unsigned int client_index)
{
	int ret;
	char cmd[64];
	char rsp[2048];

	snprintf(cmd, ARRAY_LEN(cmd) - 1, "AT+CIPSEND=%u,%u",
		 client_index, strnlen(data, len));
	
	ret = esp_at_send_cmd(cmd, rsp, ARRAY_LEN(rsp));

	DEBUGDATA("AT Send CMD", rsp, "%s");

	if (ret < 0)
		return ret;

	ret = esp_at_send_cmd(data, rsp, sizeof(rsp));

	DEBUGDATA("AT data response", rsp, "%s");

	return ret;
}
