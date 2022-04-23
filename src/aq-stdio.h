/**
 * @file aq-stdio.h
 * @author Tyler J. Anderson
 * @brief Air quality program API for multiplexed data output
 */

#ifndef AQ_STDIO_H

#include "aq-error-state.h"
#include "esp-at-modem.h"

#ifndef AQ_STDIO_BUFFER_SIZE
#define AQ_STDIO_BUFFER_SIZE 256
#endif /* #ifndef AQ_STDIO_BUFFER_SIZE */

#ifndef AQ_STDIO_BUFFER_NUM
#define AQ_STDIO_BUFFER_NUM 20
#endif /* #ifndef AQ_STDIO_BUFFER_SIZE */

void aq_stdio_init(aq_status *s, esp_at_status *e);
void aq_nprintf(const char *restrict format, ...);
void aq_stdio_deinit();
void aq_stdio_process();
void aq_stdio_sleep_until(absolute_time_t time);

#endif /* #ifndef AQ_STDIO_H */
