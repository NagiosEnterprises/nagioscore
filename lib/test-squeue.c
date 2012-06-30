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

typedef struct test_suite {
	int pass;
	int fail;
	int latest;
} test_suite;

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

int t_print(test_suite *ts)
{
	printf("total: %d; passed: %d; failed: %d; status: %s\n",
		   ts->pass + ts->fail, ts->pass, ts->fail, ts->fail ? "FAILED" : "PASSED");
	return ts->fail ? -1 : 0;
}

#define TEST_PASS 0
#define TEST_FAIL 1

#define t(expr, args...) \
	if ((expr)) { \
		ts->pass++; \
		ts->latest = TEST_PASS; \
		if (getenv("TEST_VERBOSE")) \
			printf("pass: " #expr "\n"); \
	} else { \
		ts->fail++; \
		ts->latest = TEST_FAIL; \
		printf("fail (@%s:%d):\n ##\t" #expr "\n", __FILE__, __LINE__); \
		printf(" " args); \
		kill(getpid(), SIGSEGV); \
		return -1; /* this will leak memory in some tests */ \
	}

typedef struct sq_test_event {
	unsigned long id;
	squeue_event *evt;
} sq_test_event;

static unsigned long sq_high = 0;
static int sq_walker(squeue_event *evt, void *arg)
{
	static int walks = 0;

	walks++;
	test_suite *ts = (test_suite *)arg;
	t(sq_high <= evt->when.tv_sec, "sq_high: %lu; evt->when: %lu\n",
	  sq_high, evt->when.tv_sec);
	sq_high = (unsigned long)evt->when.tv_sec;

	return 0;
}

#define EVT_ARY 65101
static int sq_test_random(squeue_t *sq, test_suite *ts)
{
	unsigned long size, i;
	unsigned long long numbers[EVT_ARY], *d, max = 0;
	struct timeval now;

	size = squeue_size(sq);
	now.tv_sec = time(NULL);
	srand((int)now.tv_sec);
	for (i = 0; i < EVT_ARY; i++) {
		now.tv_usec = (time_t)rand();
		numbers[i] = evt_compute_pri(&now);
		squeue_add_tv(sq, &now, &numbers[i]);
		t(squeue_size(sq) == i + 1 + size);
	}

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

	return 0;
}

#if 0
static void print_squeue_event(FILE *f, void *e)
{
	sq_test_event *te = ((squeue_event *)e)->data;
	te = (sq_test_event *)squeue_event_data((squeue_event *)e);

	fprintf(f, "%lu\n", te->id);
}
#endif

static int test_squeue(test_suite *ts)
{
	squeue_t *sq;
	struct timeval tv;
	sq_test_event a, b, c, d, *x;

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
	sq_test_random(sq, ts);
	t(squeue_size(sq) == 0, "Size should be 0 after first sq_test_random");

	t((a.evt = squeue_add(sq, time(NULL) + 9, &a)) != NULL);
	t(squeue_size(sq) == 1);
	t((b.evt = squeue_add(sq, time(NULL) + 3, &b)) != NULL);
	t(squeue_size(sq) == 2);
	t((c.evt = squeue_add(sq, time(NULL) + 5, &c)) != NULL);
	t(squeue_size(sq) == 3);
	t((d.evt = squeue_add(sq, time(NULL) + 6, &d)) != NULL);
	t(squeue_size(sq) == 4);

	/* add and remove lots. remainder should be what we have above */
	sq_test_random(sq, ts);

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

	squeue_foreach(sq, sq_walker, ts);

	/* clean up to prevent false valgrind positives */
	squeue_destroy(sq, 0);

	return t_print(ts);
}

int main(int argc, char **argv)
{
	test_suite ts = { 0 };
	test_squeue(&ts);
//	test_kvvec();
	return 0;
}
