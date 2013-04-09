/*
 * we include the c source file to get at the opaque types and
 * api functions
 */
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include "squeue.c"
#include "t-utils.h"

static void squeue_foreach(squeue_t *q, int (*walker)(squeue_event *, void *), void *arg)
{
	squeue_t *dup;
	void *e, *dup_d;

	dup = squeue_create(q->size);
	dup_d = dup->d;
	memcpy(dup, q, sizeof(*q));
	dup->d = dup_d;
	memcpy(dup->d, q->d, (q->size * sizeof(void *)));

	while ((e = pqueue_pop(dup))) {
		walker(e, arg);
	}
	squeue_destroy(dup, 0);
}

#define t(expr, args...) \
	do { \
		if ((expr)) { \
			t_pass(#expr); \
		} else { \
			t_fail(#expr " @%s:%d", __FILE__, __LINE__); \
		} \
	} while(0)

typedef struct sq_test_event {
	unsigned long id;
	squeue_event *evt;
} sq_test_event;

static time_t sq_high = 0;
static int sq_walker(squeue_event *evt, void *arg)
{
	static int walks = 0;

	walks++;
	t(sq_high <= evt->when.tv_sec, "sq_high: %lu; evt->when: %lu\n",
	  sq_high, evt->when.tv_sec);
	sq_high = (unsigned long)evt->when.tv_sec;

	return 0;
}

#define EVT_ARY 65101
static int sq_test_random(squeue_t *sq)
{
	unsigned long size, i;
	unsigned long long numbers[EVT_ARY], *d, max = 0;
	struct timeval now;

	size = squeue_size(sq);
	now.tv_sec = time(NULL);
	srand((int)now.tv_sec);
	for (i = 0; i < EVT_ARY; i++) {
		now.tv_usec = (time_t)rand();
		squeue_add_tv(sq, &now, &numbers[i]);
		numbers[i] = evt_compute_pri(&now);
		t(squeue_size(sq) == i + 1 + size);
	}

	t(pqueue_is_valid(sq));

	/*
	 * make sure we pop events in increasing "priority",
	 * since we calculate priority based on time and later
	 * is lower prio
	 */
	for (i = 0; i < EVT_ARY; i++) {
		d = (unsigned long long *)squeue_pop(sq);
		t(max <= *d, "popping randoms. i: %lu; delta: %lld; max: %llu; *d: %llu\n",
		  i, max - *d, max, *d);
		max = *d;
		t(squeue_size(sq) == size + (EVT_ARY - i - 1));
	}
	t(pqueue_is_valid(sq));

	return 0;
}

int main(int argc, char **argv)
{
	squeue_t *sq;
	struct timeval tv;
	sq_test_event a, b, c, d, *x;

	t_set_colors(0);
	t_start("squeue tests");

	a.id = 1;
	b.id = 2;
	c.id = 3;
	d.id = 4;

	gettimeofday(&tv, NULL);
	/* Order in is a, b, c, d, but we should get b, c, d, a out. */
	srand(tv.tv_usec ^ tv.tv_sec);
	t((sq = squeue_create(1024)) != NULL);
	t(squeue_size(sq) == 0);

	/* we fill and empty the squeue completely once before testing */
	sq_test_random(sq);
	t(squeue_size(sq) == 0, "Size should be 0 after first sq_test_random");

	t((a.evt = squeue_add(sq, time(NULL) + 9, &a)) != NULL);
	t(squeue_size(sq) == 1);
	t((b.evt = squeue_add(sq, time(NULL) + 3, &b)) != NULL);
	t(squeue_size(sq) == 2);
	t((c.evt = squeue_add_msec(sq, time(NULL) + 5, 0, &c)) != NULL);
	t(squeue_size(sq) == 3);
	t((d.evt = squeue_add_usec(sq, time(NULL) + 5, 1, &d)) != NULL);
	t(squeue_size(sq) == 4);

	/* add and remove lots. remainder should be what we have above */
	sq_test_random(sq);

	/* testing squeue_peek() */
	t((x = (sq_test_event *)squeue_peek(sq)) != NULL);
	t(x == &b, "x: %p; a: %p; b: %p; c: %p; d: %p\n", x, &a, &b, &c, &d);
	t(x->id == b.id);
	t(squeue_size(sq) == 4);

	/* testing squeue_remove() and re-add */
	t(squeue_remove(sq, b.evt) == 0);
	t(squeue_size(sq) == 3);
	t((x = squeue_peek(sq)) != NULL);
	t(x == &c);
	t((b.evt = squeue_add(sq, time(NULL) + 3, &b)) != NULL);
	t(squeue_size(sq) == 4);

	/* peek should now give us the &b event (again) */
	t((x = squeue_peek(sq)) != NULL);
	if (x != &b) {
		printf("about to fail pretty fucking hard...\n");
		printf("ea: %p; &b: %p; &c: %p; ed: %p; x: %p\n",
			   &a, &b, &c, &d, x);
	}
	t(x == &b);
	t(x->id == b.id);
	t(squeue_size(sq) == 4);

	/* testing squeue_pop(), lifo manner */
	t((x = squeue_pop(sq)) != NULL);
	t(squeue_size(sq) == 3,
	  "squeue_size(sq) = %d\n", squeue_size(sq));
	t(x == &b, "x: %p; &b: %p\n", x, &b);
	t(x->id == b.id, "x->id: %lu; d.id: %lu\n", x->id, d.id);

	/* Test squeue_pop() */
	t((x = squeue_pop(sq)) != NULL);
	t(squeue_size(sq) == 2);
	t(x == &c, "x->id: %lu; c.id: %lu\n", x->id, c.id);
	t(x->id == c.id, "x->id: %lu; c.id: %lu\n", x->id, c.id);

	/* this should fail gracefully (-1 return from squeue_remove()) */
	t(squeue_remove(NULL, NULL) == -1);
	t(squeue_remove(NULL, a.evt) == -1);

	squeue_foreach(sq, sq_walker, NULL);

	/* clean up to prevent false valgrind positives */
	squeue_destroy(sq, 0);

	return t_end();
}
