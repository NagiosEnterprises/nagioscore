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
	struct tcase *test;
	unsigned long last_ptr, *ptr;
	int i, added = 0, removed = 0;
	fanout_table *fo;

	last_ptr = ntests;

	fo = fanout_create(fo_size);
	test = calloc(ntests, sizeof(*test));
	for (i = 0; i < ntests; i++) {
		test[i].value = i;
		if (!fanout_add(fo, test[i].key, &test[i].value))
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
		test[i].value = i;
		if (!fanout_add(fo, test[i].key, &test[i].value))
			added++;
	}
	fanout_destroy(fo, destructor);
	ok_int(destroyed, ntests, "Expected ntest entries in destructor");
	destroyed = 0;
	free(test);
}

int main(int argc, char **argv)
{
	int i;

	t_set_colors(0);
	t_start("fanout tests");
	run_tests(10, 64);
	run_tests(512, 64);
	run_tests(64, 64);
	return t_end();
}
