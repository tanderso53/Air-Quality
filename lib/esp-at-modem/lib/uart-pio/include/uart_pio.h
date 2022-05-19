/**
 * @file uart_pio.h
 * @author Tyler J. Anderson
 * @brief RPi Pico PIO higher-level UART API
 */

#ifndef UART_PIO_H
#define UART_PIO_H

#include <stdint.h>

#include "hardware/pio.h"

/**
 * @defgroup uartpio UART over Pico PIO API
 * @{
 */

#define UART_PIO_OK			0
#define UART_PIO_E_HARDWARE_FAIL	-1
#define UART_PIO_E_NULL_PTR		-2

/** @brief Config for the PIO uart
 *
 * Fill out completely before passing to the init function
 */
typedef struct {
	PIO pio; /**< @brief Hardware PIO to use */
	uint sm_tx; /**< @brief State machine for TX */
	uint sm_rx; /**< @brief State machine for RX */
	uint pin_tx; /**< @brief GPIO pin for TX */
	uint pin_rx; /**< @brief GPIO pin for RX */
	uint baud; /**< @brief The UART speed setting */
} uart_pio_cfg;

/** @brief Initialize a PIO as a UART */
uint uart_pio_init(uart_pio_cfg *cfg);

/** @brief Check if uart is writable */
bool uart_pio_is_writable(uart_pio_cfg *cfg);

/** @brief Check if uart is readable */
bool uart_pio_is_readable(uart_pio_cfg *cfg);

/** @brief Transmit a character and block until it is sent */
void uart_pio_putc_blocking(uart_pio_cfg *cfg, char c);

/** @brief Transmit a character string and block until send */
void uart_pio_puts_blocking(uart_pio_cfg *cfg, const char *s);

/** @brief Attempt to transmit a character with a timeout
 *
 * Will block for a given timeout until the timeout is reached, or
 * the character is send.
 *
 * @param cfg The UART PIO object to send the character on
 * @param c The character to send
 * @param us The number of microseconds to block until returning
 * failure
 *
 * @return true on success, false on failure
 */
bool uart_pio_putc_timeout(uart_pio_cfg *cfg, char c, uint64_t us);

/** @brief Attempt to transmit a character string with a timeout
 *
 * Will block for a given timeout until the timeout is reached, or
 * all the characters in the string are sent.
 *
 * @param cfg The UART PIO object to send the characters on
 * @param s The character string to send
 * @param us The number of microseconds to block until returning
 * failure
 *
 * @return true on success, false on failure
 */
bool uart_pio_puts_timeout(uart_pio_cfg *cfg, const char *s,
			   uint64_t us);

/** @brief Receive a character from the UART PIO, blocking until
 * complete
 */
char uart_pio_getc_blocking(uart_pio_cfg *cfg);

/** @brief Attempt to receive a character over UART PIO with timeout
 *
 * The function will block until the timeout is reached or a character
 * is available for reading on the RX FIFO.
 *
 * @param cfg The UART PIO object to get the character from
 * @param c A pointer to the object to fill. Will be '\0' on failure
 * @param us The number of microseconds to block until returning
 * failure
 *
 * @return True on success, False on failure
 */
bool uart_pio_getc_timeout(uart_pio_cfg *cfg, char *c, uint64_t us);

/** @brief Flush TX FIFO */
void uart_pio_flush_tx(uart_pio_cfg *cfg);

/** @brief Flush RX FIFO */
void uart_pio_flush_rx(uart_pio_cfg *cfg);

/**
 * @}
 */
#endif /* #ifndef UART_PIO_H */
