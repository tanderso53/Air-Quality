#include "aq-stdio.h"
#include "debugmsg.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"

#define ARRAY_LEN(array) sizeof(array)/sizeof(array[0])

typedef struct {
	void *data;
	void (*task)(void*);
	unsigned int priority;
	int wait;
} _aq_stdio_task;

typedef struct {
	char buf[AQ_STDIO_BUFFER_SIZE];
	bool dir; /* Send TRUE, receive FALSE */
	semaphore_t sem;
} _aq_iobuf;

static bool _aq_stdio_is_init = false;
static aq_status *_aq_s = NULL;
static esp_at_status *_esp_s = NULL;
static _aq_iobuf _buffers[AQ_STDIO_BUFFER_NUM];
static semaphore_t _sem;
static queue_t _q_tasks;

static _aq_iobuf *_aq_retrieve_buf();
static bool _aq_release_buf(_aq_iobuf *buf);
static void _aq_enqueue_uart(_aq_iobuf *buf);
static void _aq_enqueue_wifi(_aq_iobuf *buf);
static void _aq_send_uart(void *buf);
static void _aq_send_wifi(void *buf);
static void _aq_stdio_thread_entry();
static void _aq_process_tasks();

void aq_stdio_init(aq_status *s, esp_at_status *e)
{
	_aq_s = s;
	_esp_s = e;

	for (size_t i = 0; i < ARRAY_LEN(_buffers); ++i) {
		/* One permit for UART, one for WiFi */
		sem_init(&_buffers[i].sem, 2, 2);
	}

	sem_init(&_sem, AQ_STDIO_BUFFER_NUM, AQ_STDIO_BUFFER_NUM);

	queue_init(&_q_tasks, sizeof(_aq_stdio_task),
		   2 * AQ_STDIO_BUFFER_NUM);

	multicore_launch_core1(_aq_stdio_thread_entry);

	_aq_stdio_is_init = true;
}

void aq_nprintf(const char * restrict format, ...)
{
	_aq_iobuf *s;
	va_list ap;

	s = _aq_retrieve_buf();
	va_start(ap, format);

	vsnprintf(s->buf, sizeof(s->buf) - 1, format, ap);
	s->buf[ARRAY_LEN(s->buf) - 1] = '\0';

	va_end(ap);

	_aq_enqueue_uart(s);
	_aq_enqueue_wifi(s);
	
}

void aq_stdio_deinit()
{
	queue_free(&_q_tasks);
	multicore_reset_core1();
	_aq_s = NULL;
	_esp_s = NULL;
	_aq_stdio_is_init = false;
}

void aq_stdio_process()
{
	_aq_process_tasks();
}

_aq_iobuf *_aq_retrieve_buf()
{
	_aq_iobuf *ret = NULL;

	DEBUGMSG("Acquiring buffer");

	sem_acquire_blocking(&_sem);

	for (size_t i = 0; i < ARRAY_LEN(_buffers); ++i) {
		if (sem_available(&_buffers[i].sem) == 2) {
			ret = &_buffers[i];
			DEBUGDATA("Acquired buffer", i, "%u");
			break;
		}
	}

	if (ret) {
		sem_reset(&ret->sem, 0);
	}

	return ret;
}

bool _aq_release_buf(_aq_iobuf *buf)
{
	if (!buf)
		return false;

	sem_release(&buf->sem);

	if (sem_available(&buf->sem) == 2) {
		sem_release(&_sem);
		DEBUGMSG("Buffer fully released");
		return true;
	}

	DEBUGMSG("Buffer partially released");

	return false;
}

void _aq_enqueue_uart(_aq_iobuf *buf)
{
	_aq_stdio_task u_task = {
		.priority = 3,
		.wait = 2,
		.task = _aq_send_uart,
		.data = (void *) buf
	};

	DEBUGDATA("Adding to UART queue", buf->buf, "%s");

	queue_add_blocking(&_q_tasks, &u_task);

	DEBUGMSG("SUCCESS");
}

void _aq_enqueue_wifi(_aq_iobuf *buf)
{
	_aq_stdio_task w_task = {
		.priority = 1, /* WiFi fails if lower priority */
		.wait = 5,
		.task = _aq_send_wifi,
		.data = (void*) buf
	};

	DEBUGDATA("Adding to WIFI queue", buf->buf, "%s");

	queue_add_blocking(&_q_tasks, &w_task);

	DEBUGMSG("SUCCESS");
}

void _aq_send_uart(void *buf)
{
	_aq_iobuf *s = (_aq_iobuf*) buf;

	if (_aq_s->status & AQ_STATUS_I_USBCOMM_CONNECTED) {
		printf("%s", s->buf);
	}

	DEBUGMSG("UART send complete, releasing buffer sem");
	_aq_release_buf(s);
}

void _aq_send_wifi(void *buf)
{
	_aq_iobuf *s = (_aq_iobuf*) buf;

	if (_aq_s->status & AQ_STATUS_I_CLIENT_CONNECTED) {
		DEBUGDATA("Attempting to write WiFi", s->buf, "%s");
		esp_at_cipsend_string(s->buf, sizeof(s->buf),
				      _esp_s);
	}

	DEBUGMSG("WIFI send complete, releasing buffer sem");
	_aq_release_buf(buf);
}

void _aq_stdio_thread_entry()
{
	DEBUGMSG("Entering CORE1");

	for (;;) {
		_aq_process_tasks();
	}
}

void _aq_process_tasks()
{
	_aq_stdio_task a = {0};
	_aq_stdio_task b = {0};

	_aq_stdio_task *new = &a;
	_aq_stdio_task *old = &b;
	_aq_stdio_task *p = NULL;
	_aq_stdio_task *no_p = NULL;

	if (!queue_try_remove(&_q_tasks, old)) {
		old->data = NULL;
	}

	for (;;) {
		if (!queue_try_remove(&_q_tasks, new)) {
			new->data = NULL;
		}

		if (!new->data && !old->data) {
			break;
		}

		DEBUGMSG("Processing task");

		/* Choose highest priority task */
		if (old->data && (old->wait <= 0)) {
			p = old;
			no_p = new;
		} else if (new->data && !old->data) {
			p = new;
			no_p = old;
		} else if (old->data && !new->data) {
			p = old;
			no_p = new;
		} else if (new->priority < old->priority) {
			p = new;
			no_p = old;

			--no_p->wait;
		} else {
			p = old;
			no_p = new;

			--no_p->wait;
		}

		/* Run the task */
		p->task(p->data);

		/* Set new/old pointers for next loop */
		new->data = NULL;
		old = no_p;
	}
}
