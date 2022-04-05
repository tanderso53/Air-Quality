/**
 * @file aq-error-state.c
 * @author Tyler J. Anderson
 * @brief Functions for standardized control of board error state
 */

#include "aq-error-state.h"

static void _aq_status_set_led(aq_status *s);

void aq_status_init(aq_status *s)
{
	s->status = 0;
	s->led_rgb = -1;

	ws2812_program_init(s->led_pio, s->led_sm,
			    pio_add_program(s->led_pio,
					    &ws2812_program),
			    s->led_pin, 800000, false);

	_aq_status_set_led(s);
}

void aq_status_write_color(uint32_t rgb, aq_status *s)
{
	uint32_t urgb;
	uint32_t r;
	uint32_t g;
	uint32_t b;

	/* Don't bother if the LED color won't change */
	if (rgb == s->led_rgb)
		return;

	/* Separate out R, G, and B so we can reorder them */
	r = ((rgb >> 16) & 0x000000ff);
	g = ((rgb >> 8) & 0x000000ff);
	b = (rgb & 0x000000ff);

	/* Load int 32u bit int ordered g, r, b left to right */
	urgb = ((r) << 8) | ((g) << 16) | (b);

	/* RGB value only 24 bit, so shift all the way to the left */
	pio_sm_put_blocking(s->led_pio, s->led_sm, urgb << 8u);

	/** @note This function will block if pio buffer is full */

	s->led_rgb = rgb;
}

void aq_status_set_status(STATUS_TYPE status, aq_status *s)
{
	s->status |= status;

	_aq_status_set_led(s);
}

void aq_status_unset_status(STATUS_TYPE status, aq_status *s)
{
	s->status &= ~status;

	_aq_status_set_led(s);
}

void aq_status_clear(aq_status *s)
{
}

void _aq_status_set_led(aq_status *s)
{
	if (s->status & AQ_STATUS_MASK_ERROR) {
		aq_status_write_color(AQ_STATUS_COLOR_ERROR, s);
		return;
	}

	if (s->status & AQ_STATUS_MASK_WARNING) {
		aq_status_write_color(AQ_STATUS_COLOR_WARNING, s);
		return;
	}

	if (s->status & AQ_STATUS_MASK_INFO) {
		aq_status_write_color(AQ_STATUS_COLOR_INFO, s);
		return;
	}

	aq_status_write_color(AQ_STATUS_COLOR_OK, s);
}
