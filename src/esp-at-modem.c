#include "esp-at-modem.h"
#include "debugmsg.h"

#include "uart_tx.pio.h"
#include "uart_rx.pio.h"

#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"

#define _ESP_EN_DELAY_US 500000
#define _ESP_RESET_HOLD_US 20000

#define ARRAY_LEN(array) sizeof(array)/sizeof(array[0])

static void _esp_en_gpio_setup();
static void _esp_reset_gpio_setup();
static void _esp_set_enabled(bool en); /* True to enable, false to disable */
static void _esp_reset();

int init_wifi_module()
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
	return send_wifi_cmd("AT", rxbuf, rxlen);
}

int send_wifi_cmd(const char *cmd, char *rsp, unsigned int len)
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

void wifi_passthrough()
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
				send_wifi_cmd(cmd, rsp, ARRAY_LEN(rsp));
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