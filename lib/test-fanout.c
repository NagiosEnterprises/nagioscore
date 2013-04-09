#define _GNU_SOURCE 1
#include <stdio.h>
#include "fanout.c"
#include "t-utils.h"

struct tcase {
	unsigned long key;
	unsigned long value;
};

static int destroyed;

static void destructor(void *ptr)
{
	destroyed++;
}

static void run_tests(int ntests, int fo_size)
{
	struct tcase *tc;
	unsigned long last_ptr, *ptr;
	int i, added = 0, removed = 0;
	fanout_table *fo;

	last_ptr = ntests;

	fo = fanout_create(fo_size);
	tc = calloc(ntests, sizeof(*tc));
	for (i = 0; i < ntests; i++) {
		tc[i].value = i;
		if (!fanout_add(fo, tc[i].key, &tc[i].value))
			added++;
	}
	ok_int(added, ntests, "Adding stuff must work");

	while ((ptr = (unsigned long *)fanout_remove(fo, 0))) {
		ok_int((int)*ptr, (int)last_ptr - 1, "Removing a bunch of them");
		removed++;
		last_ptr = *ptr;
	}
	ok_int(added, removed, "Removing should work as well as adding");
	fanout_destroy(fo, destructor);
	ok_int(destroyed, 0, "Expected no entries in destructor");

	fo = fanout_create(fo_size);
	for (i = 0; i < ntests; i++) {
		tc[i].value = i;
		if (!fanout_add(fo, tc[i].key, &tc[i].value))
			added++;
	}
	fanout_destroy(fo, destructor);
	ok_int(destroyed, ntests, "Expected ntest entries in destructor");
	destroyed = 0;
	free(tc);
}

struct test_data {
	unsigned long key;
	char *name;
};

static fanout_table *fot;
static void pdest(void *arg_)
{
	struct test_data *td = (struct test_data *)arg_;
	fanout_remove(fot, td->key);
	free(td->name);
	free(td);
	destroyed++;
}

int main(int argc, char **argv)
{
	unsigned long k;
	t_set_colors(0);
	t_start("fanout tests");
	struct test_data *td;

	run_tests(10, 64);
	run_tests(512, 64);
	run_tests(64, 64);
	run_tests(511, 17);

	destroyed = 0;
	fot = fanout_create(512);
	ok_int(fanout_remove(fot, 12398) == NULL, 1,
		"remove on empty table must yield NULL");
	ok_int(fanout_get(fot, 123887987) == NULL, 1,
		"get on empty table must yield NULL");
	for (k = 0; k < 16385; k++) {
		struct test_data *tdata = calloc(1, sizeof(*td));
		tdata->key = k;
		asprintf(&tdata->name, "%lu", k);
		fanout_add(fot, k, tdata);
	}
	td = fanout_get(fot, k - 1);
	ok_int(td != NULL, 1, "get must get what add inserts");
	ok_int(fanout_remove(fot, k + 1) == NULL, 1,
		"remove on non-inserted key must yield NULL");
	ok_int(fanout_get(fot, k + 1) == NULL, 1,
		"get on non-inserted must yield NULL");
	fanout_destroy(fot, pdest);
	ok_int((int)destroyed, (int)k, "destroy counter while free()'ing");

	return t_end();
}
