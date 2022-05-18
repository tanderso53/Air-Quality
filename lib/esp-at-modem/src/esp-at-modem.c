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
 * @file esp-at-modem.c
 *
 * @brief Interface implementation for a pico-compatible ESP-AT WiFi
 * library
 */

#include "esp-at-modem.h"
#include "at-parse.h"
#include "debugmsg.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "pico/stdlib.h"
#ifdef ESP_AT_MULTICORE_ENABLED
#include "pico/multicore.h"
#endif /* #ifdef ESP_AT_MULTICORE_ENABLED */

#define _ESP_EN_DELAY_US 2000000
#define _ESP_RESET_HOLD_US 20000
#define _ESP_RESPONSE_BUFFER_LEN 2048
#define _ESP_UART_WAIT_US 10000000

#define ARRAY_LEN(array) sizeof(array)/sizeof(array[0])

#ifdef ESP_AT_MULTICORE_ENABLED
static recursive_mutex_t _esp_mtx;
#endif /* #ifdef ESP_AT_MULTICORE_ENABLED */

static int _esp_query(esp_at_cfg * cfg, const char * cmd,
		      at_rsp_lines *rsp);

static void _esp_en_gpio_setup(esp_at_cfg * cfg);
static void _esp_reset_gpio_setup(esp_at_cfg * cfg);
static void _esp_set_enabled(esp_at_cfg * cfg, bool en); /* True to enable, false to disable */
static void _esp_reset(esp_at_cfg * cfg);
static int _esp_cipsend_data(esp_at_cfg * cfg, const char *data,
			     size_t len,
			     unsigned int client_index);
static int _esp_check_cipsta(esp_at_cfg * cfg,
			     esp_at_status *clientlist);
static int _esp_check_cipstatus(esp_at_cfg * cfg,
				esp_at_status *clientlist);
static int _esp_check_cipmux(esp_at_cfg * cfg,
			     esp_at_status *clientlist);
static int _esp_transmit_cmd(esp_at_cfg *cfg, const char *cmd);
static int _esp_receive_response(esp_at_cfg *cfg, char *rsp,
				 size_t len);
static bool _esp_check_at_end_sequence(const char *rsp);
static int _esp_check_rsp_success(const char *rsp);

/*
**********************************************************************
******************* API FUNCTION IMPLEMENTATION **********************
**********************************************************************
*/

int esp_at_init_module(esp_at_cfg *cfg, PIO pio, uint sm_tx,
		       uint sm_rx, uint pin_tx, uint pin_rx,
		       uint baud, uint en_pin, uint reset_pin)
{
	int rslt;
	const unsigned int rxlen = 64;
	char rxbuf[rxlen];

	DEBUGMSG("Initializing WiFi");

#ifdef ESP_AT_MULTICORE_ENABLED
	recursive_mutex_init(&_esp_mtx);
#endif /* #ifdef ESP_AT_MULTICORE_ENABLED */

	cfg->ptr = NULL;
	cfg->en_pin = en_pin;
	cfg->reset_pin = reset_pin;

	/* Initialize gpio pins for enable and reset */
	_esp_en_gpio_setup(cfg);
	_esp_reset_gpio_setup(cfg);

	/* Set up UART */
	cfg->uart_cfg.pio = pio;
	cfg->uart_cfg.sm_tx = sm_tx;
	cfg->uart_cfg.sm_rx = sm_rx;
	cfg->uart_cfg.pin_tx = pin_tx;
	cfg->uart_cfg.pin_rx = pin_rx;
	cfg->uart_cfg.baud = baud;

	if (uart_pio_init(&cfg->uart_cfg) != UART_PIO_OK)
		return -1;

	/* Reset and enable ESP8266 */
	_esp_set_enabled(cfg, true);
	_esp_reset(cfg);
	sleep_us(_ESP_EN_DELAY_US); /* Module may need time to boot */
	esp_at_passthrough(cfg);

	/* Check connection */
	for (int tries = 5; tries > 0; --tries)
	{
		rslt = esp_at_send_cmd(cfg, "AT", rxbuf, rxlen);

		if (rslt > 0)
			break;

		/* Turn off and on again if there was a UART timing
		 * error */
		if (uart_pio_check_flags_and_clear(&cfg->uart_cfg)) {
			DEBUGDATA("UART PIO Framing Error, retry no",
				  tries, "%u");
			_esp_reset(cfg);
		}
	}

	if (rslt > 0) {
		cfg->ptr = cfg;
	}

	return rslt;
}

int esp_at_cipserver_init(esp_at_cfg *cfg)
{
	int ret;
	char rsp[_ESP_RESPONSE_BUFFER_LEN];

	/* Set up server */
	ret = esp_at_send_cmd(cfg, "AT+CIPMUX=1", rsp, ARRAY_LEN(rsp));
	DEBUGDATA("When muxing ESP", rsp, "%s");

	if (ret < 0) {
		return ret;
	}

	ret = esp_at_send_cmd(cfg, "AT+CIPSERVER=1", rsp,
			      ARRAY_LEN(rsp));

	if (ret < 0) {
		return ret;
	}

	return 0;
}

int esp_at_cipsend_string(esp_at_cfg *cfg, const char *s, size_t len,
			  esp_at_status *clientlist)
{
	int ret;

	if (strnlen(s, len) == 0)
		return 0;

	if (!clientlist) {
		ret = _esp_cipsend_data(cfg, s, len, 0);
		return ret;
	}

	for (unsigned int i = 0; i < clientlist->ncli; ++i) {
		unsigned int ci = clientlist->cli[i].index;

		ret = _esp_cipsend_data(cfg, s, len, ci);

		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

int esp_at_cipstatus(esp_at_cfg *cfg, esp_at_status *clientlist)
{
	int ret;

	clientlist->cfg = cfg;

	ret = _esp_check_cipsta(cfg, clientlist);

	if (ret < 0) {
		return ret;
	}

	ret = _esp_check_cipstatus(cfg, clientlist);

	if (ret < 0) {
		return ret;
	}

	ret = _esp_check_cipmux(cfg, clientlist);

	if (ret < 0) {
		return ret;
	}

	return 0;
}

int esp_at_send_cmd(esp_at_cfg *cfg, const char *cmd, char *rsp,
		    unsigned int len)
{
	int rslt;

	DEBUGDATA("Sending AT command", cmd, "%s");

#ifdef ESP_AT_MULTICORE_ENABLED
	recursive_mutex_enter_blocking(&_esp_mtx);
#endif /* #ifdef ESP_AT_MULTICORE_ENABLED */

	/* Remove any junk that found its way into the RX buffer
	 * before we try any commands */
	uart_pio_flush_rx(&cfg->uart_cfg);

	/* Returns 0 if successful */
	rslt = _esp_transmit_cmd(cfg, cmd);

	if (rslt != 0) {
		DEBUGDATA("ESP response timeout", cmd, "%s");
#ifdef ESP_AT_MULTICORE_ENABLED
		recursive_mutex_exit(&_esp_mtx);
#endif /* #ifdef ESP_AT_MULTICORE_ENABLED */
		return -1;
	}

	DEBUGMSG("Checking AT response");

	rslt = _esp_receive_response(cfg, rsp, len);

#ifdef ESP_AT_MULTICORE_ENABLED
	recursive_mutex_exit(&_esp_mtx);
#endif /* #ifdef ESP_AT_MULTICORE_ENABLED */

	return rslt;
}

void esp_at_passthrough(esp_at_cfg *cfg)
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
				esp_at_send_cmd(cfg, cmd, rsp,
						ARRAY_LEN(rsp));
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

int esp_at_deep_sleep(esp_at_cfg *cfg, unsigned long time_ms)
{
	char buf[512] = {'\0'};
	char cmd[36] = {'\0'};

	snprintf(cmd, sizeof(cmd) - 1, "AT+GSLP=%lu",
		time_ms);

	return esp_at_send_cmd(cfg, cmd, buf, sizeof(buf));
}

int esp_at_sleep(esp_at_cfg *cfg)
{
	char buf[512] = {'\0'};

	return esp_at_send_cmd(cfg, "AT+SLEEP=1", buf, sizeof(buf));
}

int esp_at_wake_up(esp_at_cfg *cfg)
{
	char buf[512] = {'\0'};

	return esp_at_send_cmd(cfg, "AT+SLEEP=0", buf, sizeof(buf));
}

/*
**********************************************************************
******************* INTERNAL IMPLEMENTATION **************************
**********************************************************************
*/

void _esp_en_gpio_setup(esp_at_cfg *cfg)
{
	const uint gpin = cfg->en_pin;

	gpio_init(gpin);

	/* Set GPIO settings */
	gpio_set_dir(gpin, true);
	gpio_disable_pulls(gpin);

	/* Set GPIO low to start disabled */
	gpio_put(gpin, false);
}

void _esp_reset_gpio_setup(esp_at_cfg *cfg)
{
	const uint gpin = cfg->reset_pin;

	gpio_init(gpin);

	/* Set GPIO settings */
	gpio_set_dir(gpin, true);
	gpio_disable_pulls(gpin);

	/* Set GPIO high to start in the non-reset position */
	gpio_put(gpin, true);
}

void _esp_set_enabled(esp_at_cfg *cfg, bool en)
{
	const uint gpin = cfg->en_pin;

	/* High enabled, low disabled */
	gpio_put(gpin, en);
}

void _esp_reset(esp_at_cfg *cfg)
{
	const uint gpin = cfg->reset_pin;

	gpio_put(gpin, false); /* Drive low to reset module */

	sleep_us(_ESP_RESET_HOLD_US); /* Hold down reset for a bit */

	gpio_put(gpin, true); /* Return reset pin to normal */
}

int _esp_parse_cw_wifi_state(esp_at_cfg *cfg,
			     esp_at_status *clientlist)
{
	int ret;
	char rsp[_ESP_RESPONSE_BUFFER_LEN];
	const char *re_str = "+CWSTATE:";
	char v[24][6];
	int n = 0;

	/* Get WiFi connection state */
	ret = esp_at_send_cmd(cfg, "AT+CWSTATE?", rsp, ARRAY_LEN(rsp));

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

int _esp_query(esp_at_cfg *cfg, const char * cmd, at_rsp_lines *rsp)
{
	int ret;
	char buf[4096];

	ret = esp_at_send_cmd(cfg, cmd, buf, ARRAY_LEN(buf));

	if (ret < 0) {
		return -1;
	}

	ret = at_rsp_get_lines(buf, rsp);

	return ret;
}

int _esp_cipsend_data(esp_at_cfg *cfg, const char *data, size_t len,
		      unsigned int client_index)
{
	int ret;
	char cmd[64];
	char rsp[2048];

	snprintf(cmd, ARRAY_LEN(cmd) - 1, "AT+CIPSEND=%u,%u",
		 client_index, strnlen(data, len));

#ifdef ESP_AT_MULTICORE_ENABLED
	/* Make sure cmd and data are given sequentially when
	 * multithreaded */
	recursive_mutex_enter_blocking(&_esp_mtx);
#endif /* #ifdef ESP_AT_MULTICORE_ENABLED */

	ret = esp_at_send_cmd(cfg, cmd, rsp, ARRAY_LEN(rsp));

	DEBUGDATA("AT Send CMD", rsp, "%s");

	if (ret < 0)
		return ret;

	ret = esp_at_send_cmd(cfg, data, rsp, sizeof(rsp));

#ifdef ESP_AT_MULTICORE_ENABLED
	/* Make sure cmd and data are given sequentially when
	 * multithreaded */
	recursive_mutex_exit(&_esp_mtx);
#endif /* #ifdef ESP_AT_MULTICORE_ENABLED */

	DEBUGDATA("AT data response", rsp, "%s");

	return ret;
}

int _esp_check_cipsta(esp_at_cfg *cfg, esp_at_status *clientlist)
{
	int ret;
	at_rsp_lines rsp;

	at_rsp_tk *ipv4;
	at_rsp_tk *gateway;
	at_rsp_tk *netmask;

	/* Get WiFi IP address */
	ret = _esp_query(cfg, "AT+CIPSTA?", &rsp);

	if (ret < 0) {
		return ret;
	}

	ipv4 = &at_rsp_get_property("ip", &rsp)->tokenlist[0];
	gateway = &at_rsp_get_property("gateway", &rsp)->tokenlist[0];
	netmask = &at_rsp_get_property("netmask", &rsp)->tokenlist[0];

	if (! (ipv4 && gateway && netmask)) {
		DEBUGMSG("No network detected");
		clientlist->status &= ~ESP_AT_STATUS_WIFI_CONNECTED;
		clientlist->ipv4[0] ='\0';
		clientlist->ipv4_gateway[0] = '\0';
		clientlist->ipv4_netmask[0] = '\0';
		return 0;
	}

	clientlist->status |= ESP_AT_STATUS_WIFI_CONNECTED;
	strncpy(clientlist->ipv4, at_rsp_token_as_str(ipv4),
		sizeof(clientlist->ipv4) - 1);
	clientlist->ipv4[ARRAY_LEN(clientlist->ipv4) - 1] = '\0';
	strncpy(clientlist->ipv4_gateway,
		at_rsp_token_as_str(gateway),
		sizeof(clientlist->ipv4_gateway) - 1);
	clientlist->ipv4_gateway[ARRAY_LEN(clientlist->ipv4_gateway) - 1] = '\0';
	strncpy(clientlist->ipv4_netmask,
		at_rsp_token_as_str(netmask),
		sizeof(clientlist->ipv4_netmask) - 1);
	clientlist->ipv4_netmask[ARRAY_LEN(clientlist->ipv4_netmask) - 1] = '\0';

	return 0;
}


int _esp_check_cipstatus(esp_at_cfg *cfg, esp_at_status *clientlist)
{
	int ret;
	at_rsp_lines rsp;
	at_rsp_tk *status;

	/* Some ESP8266 modules don't support AT+CIPSTATE, so use
	 * AT+CIPSTATUS to get most of the networking info */
	ret = _esp_query(cfg, "AT+CIPSTATUS", &rsp);

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

	return 0;
}

int _esp_check_cipmux(esp_at_cfg *cfg, esp_at_status *clientlist)
{
	int ret;
	at_rsp_lines rsp;
	at_rsp_tk *value;
	esp_at_status_byte *status = &clientlist->status;

	/* Query specific server parameters */
	ret = _esp_query(cfg, "AT+CIPMUX?", &rsp);

	if (ret < 0) {
		return ret;
	}

	value = &at_rsp_get_property("+CIPMUX", &rsp)->tokenlist[0];

	if (at_rsp_token_as_int(value)) {
		*status |= ESP_AT_STATUS_CIPMUX_ON;
	} else {
		*status &= ~ESP_AT_STATUS_CIPMUX_ON;
	}

	return 0;
}

int _esp_transmit_cmd(esp_at_cfg *cfg, const char *cmd)
{
	char outstr[256];

	/* Add CR and LF to string */
	strncpy(outstr, cmd, sizeof(outstr));
	strlcat(outstr, "\r\n", sizeof(outstr));

	/* Semi-blocking with hard-coded timeout */
	if (uart_pio_puts_timeout(&cfg->uart_cfg, cmd,
				  _ESP_UART_WAIT_US)) {
		return 0;
	}

	DEBUGMSG("ESP send cmd timeout");

	/* Flush TX on failure */
	/* uart_pio_flush_tx(&cfg->uart_cfg); */

	return -1;
}

int _esp_receive_response(esp_at_cfg *cfg, char *rsp, size_t len)
{
	int rslt = 0;

	for (unsigned int i = 0; i < len - 1; i++) {
		char c = '\0';

		if (!uart_pio_getc_timeout(&cfg->uart_cfg, &c,
					   _ESP_UART_WAIT_US)) {
			DEBUGMSG("ESP response timeout");
			break;
		}

		if (c) {
			rsp[i] = c;
			rsp[i + 1] = '\0'; /* Make sure this is a valid string */
		} else {
			--i;
		}

		if (!_esp_check_at_end_sequence(rsp))
			continue;

		DEBUGMSG("Found AT end sequence");
		DEBUGDATA("AT Response so far", rsp, "%s");

		/* rslt = 1 for OK, -1 for ERROR, and 0 if no match */
		if ((rslt = _esp_check_rsp_success(rsp)))
			break;

		/* DEBUGDATA("AT Response so far", rsp, "%s"); */
	}

	switch (rslt) {
	case 1:
		/* Return number of characters read if successful */
		DEBUGMSG("Received AT response OK for command");
		return strlen(rsp);
	case 0:
		DEBUGMSG("Received no AT response before buffer filled command");
		break;
	default:
		/* Ran out of buffer before received OK or ERROR */
		DEBUGMSG("Received AT response ERROR for command");
		break;
	}

	return -1;
}

bool _esp_check_at_end_sequence(const char *rsp)
{
	unsigned int len = strlen(rsp);

	if (len < 2)
		return false;
	return !strcmp(&rsp[len - 2], "\r\n");
}

int _esp_check_rsp_success(const char *rsp)
{
	const char *okptrn = "OK\r\n";
	const char *errptrn = "ERROR\r\n";

	const int len = strlen(rsp);
	const int okpos = len - (int) strlen(okptrn);
	const int errpos = len - (int) strlen(errptrn);

	if (okpos >= 0 && !strcmp(&rsp[okpos], okptrn)) {
		DEBUGMSG("Received AT response OK for command");

		return 1;
	}

	if (errpos >= 0 && !strcmp(&rsp[errpos], errptrn)) {
		DEBUGMSG("Received AT response ERROR for command");

		return -1;
	}

	return 0;
}
