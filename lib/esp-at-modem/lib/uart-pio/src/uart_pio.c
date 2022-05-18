/**
 * @file uart_pio.c
 * @author Tyler J. Anderson
 * @brief RPi Pico PIO higher-level UART implementation
 */

#include "uart_pio.h"
#include "uart_pio_program.pio.h"
#include "debugmsg.h"

#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"

uint uart_pio_init(uart_pio_cfg *cfg)
{
	uint offset;

	if (!cfg) {
		return UART_PIO_E_NULL_PTR;
	}

	/* Install and start the TX program */
	offset = pio_add_program(cfg->pio, &uart_tx_program);
	uart_tx_program_init(cfg->pio, cfg->sm_tx, offset, cfg->pin_tx,
			     cfg->baud);

	/* Install and start the RX program */
	offset = pio_add_program(cfg->pio, &uart_rx_program);
	uart_rx_program_init(cfg->pio, cfg->sm_rx, offset,
			     cfg->pin_rx, cfg->baud);

	return UART_PIO_OK;
}

bool uart_pio_is_writable(uart_pio_cfg *cfg)
{
	return !pio_sm_is_tx_fifo_full(cfg->pio, cfg->sm_tx);
}

bool uart_pio_is_readable(uart_pio_cfg *cfg)
{
	return !pio_sm_is_rx_fifo_empty(cfg->pio, cfg->sm_rx);
}

void uart_pio_putc_blocking(uart_pio_cfg *cfg, char c)
{
	uart_tx_program_putc(cfg->pio, cfg->sm_tx, c);
}

void uart_pio_puts_blocking(uart_pio_cfg *cfg, const char *s)
{
	int i = 0;

	while (s[i] != '\0') {
		uart_pio_putc_blocking(cfg, s[i++]);
	}
}

bool uart_pio_putc_timeout(uart_pio_cfg *cfg, char c, uint64_t us)
{
	absolute_time_t to = make_timeout_time_us(us);

	while (!uart_pio_is_writable(cfg) &&
	       absolute_time_diff_us(to, get_absolute_time()) < 0);

	if (uart_pio_is_writable(cfg)) {
		uart_pio_putc_blocking(cfg, c);
		return true;
	}

	return false;
}

bool uart_pio_puts_timeout(uart_pio_cfg *cfg, const char *s,
			   uint64_t us)
{
	int i = 0;

	while (s[i] != '\0') {
		if (!uart_pio_putc_timeout(cfg, s[i++], us)) {
			return false;
		}
	}

	return true;
}

char uart_pio_getc_blocking(uart_pio_cfg *cfg)
{
	return uart_rx_program_getc(cfg->pio, cfg->sm_rx);
}

bool uart_pio_getc_timeout(uart_pio_cfg *cfg, char *c, uint64_t us)
{
	absolute_time_t to = make_timeout_time_us(us);
	bool readable;

	while (!(readable = uart_pio_is_readable(cfg)) &&
	       absolute_time_diff_us(to, get_absolute_time()) < 0) {
		tight_loop_contents();
	}

	if (readable) {
		*c = uart_pio_getc_blocking(cfg);
		return true;
	}

	*c = '\0';

	return false;
}

void uart_pio_flush_tx(uart_pio_cfg *cfg)
{
	pio_sm_clear_fifos(cfg->pio, cfg->sm_tx);
}

void uart_pio_flush_rx(uart_pio_cfg *cfg)
{
	pio_sm_clear_fifos(cfg->pio, cfg->sm_rx);
}
