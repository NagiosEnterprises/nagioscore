/**
 * @file squeue.c
 * @brief pqeue wrapper library
 *
 * This library wraps the libpqueue library and handles the boring
 * parts for all manner of events that want to use it, while hiding
 * the implementation details of the pqueue's binary heap from the
 * callers.
 *
 * peek() is O(1)
 * add(), pop() and remove() are O(lg n), although remove() is
 * impossible unless caller maintains the pointer to the scheduled
 * event.
 */

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "squeue.h"
#include "pqueue.h"

struct squeue_event {
	unsigned int pos;
	pqueue_pri_t pri;
	struct timeval when;
	void *data;
};


static pqueue_pri_t evt_compute_pri(struct timeval *tv)
{
	pqueue_pri_t ret;

	/* keep weird compilers on 32-bit systems from doing wrong */
	if(sizeof(pqueue_pri_t) < 8) {
		ret = tv->tv_sec;
		ret += !!tv->tv_usec;
	} else {
		ret = tv->tv_sec;
		ret <<= 32;
		ret |= tv->tv_usec;
	}

	return ret;
}

static int sq_cmp_pri(pqueue_pri_t next, pqueue_pri_t cur)
{
	return next > cur;
}

static unsigned long long sq_get_pri(void *a)
{
	return ((squeue_event *)a)->pri;
}

static void sq_set_pri(void *a, pqueue_pri_t pri)
{
	((squeue_event *)a)->pri = pri;
}

static unsigned int sq_get_pos(void *a)
{
	return ((squeue_event *)a)->pos;
}

static void sq_set_pos(void *a, unsigned int pos)
{
	((squeue_event *)a)->pos = pos;
}

const struct timeval *squeue_event_runtime(squeue_event *evt)
{
	if (evt)
		return &evt->when;
	return NULL;
}

void *squeue_event_data(squeue_event *evt)
{
	if (evt)
		return evt->data;
	return NULL;
}

squeue_t *squeue_create(unsigned int horizon)
{
	if (!horizon)
		horizon = 127; /* makes pqueue allocate 128 elements */

	return pqueue_init(horizon, sq_cmp_pri, sq_get_pri, sq_set_pri, sq_get_pos, sq_set_pos);
}

squeue_event *squeue_add_tv(squeue_t *q, struct timeval *tv, void *data)
{
	squeue_event *evt;

	if (!q)
		return NULL;

	evt = calloc(1, sizeof(*evt));
	if (!evt)
		return NULL;

	/* we can't schedule events in the past */
	if (tv->tv_sec < time(NULL))
		tv->tv_sec = time(NULL);
	evt->when.tv_sec = tv->tv_sec;
	evt->when.tv_usec = tv->tv_usec;
	evt->data = data;

	evt->pri = evt_compute_pri(&evt->when);

	if (!pqueue_insert(q, evt))
		return evt;

	return NULL;
}

squeue_event *squeue_add(squeue_t *q, time_t when, void *data)
{
	struct timeval tv;

	/*
	 * we fetch real microseconds first, so events with same
	 * timestamp get different priorities for FIFO ordering.
	 */
	gettimeofday(&tv, NULL);
	tv.tv_sec = when;

	return squeue_add_tv(q, &tv, data);
}

squeue_event *squeue_add_usec(squeue_t *q, time_t when, time_t usec, void *data)
{
	struct timeval tv;
	tv.tv_sec = when;
	tv.tv_usec = usec;
	assert(usec < 1000000);
	return squeue_add_tv(q, &tv, data);
}

squeue_event *squeue_add_msec(squeue_t *q, time_t when, time_t msec, void *data)
{
	return squeue_add_usec(q, when, msec * 1000, data);
}

void *squeue_peek(squeue_t *q)
{
	squeue_event *evt = pqueue_peek(q);
	if (evt)
		return evt->data;
	return NULL;
}

void *squeue_pop(squeue_t *q)
{
	squeue_event *evt;
	void *ptr = NULL;

	evt = pqueue_pop(q);
	if (evt) {
		ptr = evt->data;
		free(evt);
	}
	return ptr;
}

int squeue_remove(squeue_t *q, squeue_event *evt)
{
	int ret;

	if (!q || !evt)
		return -1;
	ret = pqueue_remove(q, evt);
	if (evt)
		free(evt);

	return ret;
}

void squeue_destroy(squeue_t *q, int flags)
{
	int i;

	if (!q || pqueue_size(q) < 1)
		return;

	/*
	 * Using two separate loops is a lot faster than
	 * doing 1 cmp+branch for every queued item
	 */
	if (flags & SQUEUE_FREE_DATA) {
		for (i = 0; i < pqueue_size(q); i++) {
			free(((squeue_event *)q->d[i + 1])->data);
			free(q->d[i + 1]);
		}
	} else {
		for (i = 0; i < pqueue_size(q); i++) {
			free(q->d[i + 1]);
		}
	}
	pqueue_free(q);
}

unsigned int squeue_size(squeue_t *q)
{
	if (!q)
		return 0;
	return pqueue_size(q);
}
