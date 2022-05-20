#include "aq-stdio.h"
#include "debugmsg.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"

#define ARRAY_LEN(array) sizeof(array)/sizeof(array[0])

typedef struct _aq_stdio_task_node {
	struct _aq_stdio_task_node *prev;
	struct _aq_stdio_task_node *next;
	void *data;
	void (*task)(void*);
	unsigned int priority;
} _aq_stdio_task;

typedef struct {
	char buf[AQ_STDIO_BUFFER_SIZE];
	bool dir; /* Send TRUE, receive FALSE */
	semaphore_t sem;
} _aq_iobuf;

static bool _aq_stdio_is_init = false;
static aq_status *_aq_s = NULL;
static esp_at_cfg *_esp_cfg = NULL;
static esp_at_status *_esp_s = NULL;
static _aq_iobuf _buffers[AQ_STDIO_BUFFER_NUM];
static _aq_stdio_task _tasks[2 * AQ_STDIO_BUFFER_NUM];
static _aq_stdio_task *_free_tasks;
static _aq_stdio_task *_task_list;
static semaphore_t _sem;
static semaphore_t _task_sem;
static queue_t _q_tasks;
static absolute_time_t _wup_time;

static _aq_iobuf *_aq_retrieve_buf();
static bool _aq_release_buf(_aq_iobuf *buf);
static void _aq_enqueue_uart(_aq_iobuf *buf);
static void _aq_enqueue_wifi(_aq_iobuf *buf);
static void _aq_sort_tasks();
static void _aq_send_uart(void *buf);
static void _aq_send_wifi(void *buf);
static void _aq_sleep_until(void *time);
static void _aq_stdio_thread_entry();
static void _aq_process_tasks();
static int _aq_compare_tasks(_aq_stdio_task *left,
			     _aq_stdio_task *right);
static _aq_stdio_task *_aq_pop_task();
static void _aq_free_task(_aq_stdio_task *task);
static void _aq_task_list_init();

void aq_stdio_init(aq_status *s, esp_at_status *e)
{
	_aq_s = s;
	_esp_s = e;
	_esp_cfg = e->cfg;

	for (size_t i = 0; i < ARRAY_LEN(_buffers); ++i) {
		/* One permit for UART, one for WiFi */
		sem_init(&_buffers[i].sem, 2, 2);
	}

	/* Semephore for output buffer management */
	sem_init(&_sem, AQ_STDIO_BUFFER_NUM, AQ_STDIO_BUFFER_NUM);

	/* Semephore for access to task list */
	sem_init(&_task_sem, 1, 1);

	/* Initialization of task list */
	_aq_task_list_init();

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

void aq_stdio_sleep_until(absolute_time_t time)
{
	_wup_time = time;
	_aq_stdio_task sleep_task = {
		.next = NULL,
		.prev = NULL,
		.priority = 10,
		.task = _aq_sleep_until,
		.data = &_wup_time
	};

	queue_add_blocking(&_q_tasks, &sleep_task);
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
		.prev = NULL,
		.next = NULL,
		.priority = 3,
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
		.prev = NULL,
		.next = NULL,
		.priority = 3, /* WiFi fails if lower priority */
		.task = _aq_send_wifi,
		.data = (void*) buf
	};

	DEBUGDATA("Adding to WIFI queue", buf->buf, "%s");

	queue_add_blocking(&_q_tasks, &w_task);

	DEBUGMSG("SUCCESS");
}

void _aq_sort_tasks()
{
	_aq_stdio_task *freetask;
	_aq_stdio_task *next;
	_aq_stdio_task *listitem;
	_aq_stdio_task *listitemprev;

	/* Make sure no one else is working on the task list */
	sem_acquire_blocking(&_task_sem);
	freetask = _free_tasks;

	while (freetask) {
		next = _free_tasks->next;
		listitem = _task_list;
		listitemprev = NULL;

		/* Nothing to do if queue is empty */
		if (!queue_try_remove(&_q_tasks, freetask))
			break;

		/* Find a ptr to the element to insert in front of */
		while (_aq_compare_tasks(listitem, freetask) <= 0) {
			listitemprev = listitem;
			listitem = listitem->next;
		}

		/* Once we have found a spot, update the surrounding
		 * ptrs. Order is very important here. */
		if (listitem) {
			freetask->prev = listitem->prev;
			freetask->next = listitem;
			listitem->prev->next = freetask;
			listitem->prev = freetask;
		} else if (listitemprev) {
			freetask->prev = listitemprev;
			freetask->next = NULL;
			listitemprev->next = freetask;
		} else { /* reset the head of the list */
			freetask->prev = NULL;
			freetask->next = NULL;
			_task_list = freetask;
		}

		freetask = next;
		_free_tasks = freetask; /* Finish the pop by reseting head */
	}

	/* Must return the sem before leaving! */
	sem_release(&_task_sem);
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
		int rslt = 0;

		DEBUGDATA("Attempting to write WiFi", s->buf, "%s");
		rslt = esp_at_cipsend_string(_esp_cfg, s->buf,
					     sizeof(s->buf), _esp_s);

		if (rslt < 0) {
			_aq_s->status |= AQ_STATUS_E_WIFI_FAIL;
		} else {
			_aq_s->status &= ~AQ_STATUS_E_WIFI_FAIL;
		}
	}

	DEBUGMSG("WIFI send complete, releasing buffer sem");
	_aq_release_buf(buf);
}

void _aq_sleep_until(void *time)
{
	absolute_time_t *wup = (absolute_time_t*) time;

	sleep_until(*wup);
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
	for (;;) {
		_aq_stdio_task *task;

		_aq_sort_tasks();
		task = _aq_pop_task();

		if (!task) {
			break; /* Must break or core0 may freeze */
		}

		DEBUGMSG("Processing task");

		/* Run the task */
		task->task(task->data);

		/* Free the task so it may be reused */
		_aq_free_task(task);
	}
}

int _aq_compare_tasks(_aq_stdio_task *left,
		      _aq_stdio_task *right)
{
	/* Returns 1 if right is higher priority then left, -1 if
	 * lower priority, and 0 if same priority
	 * Lower priority number means high priority */
	if (!left)
		return 1;

	if (!right)
		return -1;

	if (right->priority < left->priority)
		return 1;

	if (left->priority < right->priority)
		return -1;

	return 0;
}

_aq_stdio_task *_aq_pop_task()
{
	_aq_stdio_task *ret;

	sem_acquire_blocking(&_task_sem);

	ret = _task_list;

	if (ret) {
		if (ret->next) {
			_task_list = ret->next;
			_task_list->prev = NULL;
		} else {
			_task_list = NULL;
		}

		ret->next = NULL;
		ret->prev = NULL;
	}

	sem_release(&_task_sem);

	return ret;
}

void _aq_free_task(_aq_stdio_task *task)
{
	_aq_stdio_task *elem;
	_aq_stdio_task *prevelem;

	sem_acquire_blocking(&_task_sem);

	elem = _free_tasks;
	prevelem = NULL;

	while (elem) {
		prevelem = elem;
		elem = elem->next;
	}

	if (!prevelem) {
		_free_tasks = elem;
	}

	elem->prev = prevelem;
	elem->next = NULL;

	sem_release(&_task_sem);
}

void _aq_task_list_init()
{
	sem_acquire_blocking(&_task_sem);

	/* No tasks set yet, so head of list will be null */
	_task_list = NULL;

	/* All tasks are free, so head points to beginning of array */
	_free_tasks = _tasks;

	/* Set all the prev ptrs */
	_tasks[0].prev = NULL;

	for (unsigned int i = 1; i < ARRAY_LEN(_tasks); ++i) {
		_tasks[i].prev = &_tasks[i - 1];
	}

	/* Set all the next ptrs */
	_tasks[ARRAY_LEN(_tasks) - 1].next = NULL;

	for (unsigned int i = 0; i < ARRAY_LEN(_tasks) - 1; ++i) {
		_tasks[i].next = &_tasks[i + 1];
	}

	sem_release(&_task_sem);
}
