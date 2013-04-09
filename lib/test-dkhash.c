#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dkhash.c"
#include "t-utils.h"

static struct {
	char *k1, *k2;
} keys[] = {
	{ "nisse", "banan" },
	{ "foo", "bar" },
	{ "kalle", "penslar" },
	{ "hello", "world" },
	{ "test", "fnurg" },
	{ "barsk", "nitfol" },
	{ "andreas", "regerar" },
	{ "Nagios", "rules" },
};

static int removed;
static struct test_data {
	int x, i, j;
} del;

unsigned int dkhash_count_entries(dkhash_table *table)
{
	unsigned int i, count = 0;

	for (i = 0; i < table->num_buckets; i++) {
		dkhash_bucket *bkt;
		for (bkt = table->buckets[i]; bkt; bkt = bkt->next)
			count++;
	}

	return count;
}

int dkhash_check_table(dkhash_table *t)
{
	return t ? t->entries - dkhash_count_entries(t) : 0;
}

void dkhash_debug_print_table(dkhash_table *t, const char *name, int force)
{
	int delta = dkhash_check_table(t);
	unsigned int count;
	if (!delta && !force)
		return;

	count = dkhash_count_entries(t);
	printf("debug data for dkhash table '%s'\n", name);
	printf("  entries: %u; counted: %u; delta: %d\n",
	       t->entries, count, delta);
	printf("  added: %u; removed: %u; delta: %d\n",
	       t->added, t->removed, t->added - t->removed);
}
#define dkhash_debug_table(t, force) dkhash_debug_print_table(t, #t, force)

static struct test_data *ddup(int x, int i, int j)
{
	struct test_data *d;

	d = malloc(sizeof(*d));
	d->x = x;
	d->i = i;
	d->j = j;
	return d;
}

struct dkhash_check {
	uint entries, count, max, added, removed;
	int ent_delta, addrm_delta;
};

static int del_matching(void *data)
{
	struct test_data *d = (struct test_data *)data;

	if (!memcmp(d, &del, sizeof(del))) {
		removed++;
		return DKHASH_WALK_REMOVE;
	}

	return 0;
}

int main(int argc, char **argv)
{
	dkhash_table *tx, *t;
	unsigned int x;
	int ret, r2;
	struct test_data s;
	char *p1, *p2;
	char *strs[10];
	char tmp[32];

	t_set_colors(0);
	t_start("dkhash basic test");
	t = dkhash_create(512);

	p1 = strdup("a not-so secret value");
	dkhash_insert(t, "nisse", NULL, p1);
	ok_int(dkhash_num_entries_max(t), 1, "Added one entry, so that's max");
	ok_int(dkhash_num_entries_added(t), 1, "Added one entry, so one added");
	ok_int(dkhash_table_size(t), 512, "Table must be sized properly");
	ok_int(dkhash_collisions(t), 0, "One entry, so zero collisions");
	p2 = dkhash_get(t, "nisse", NULL);
	test(p1 == p2, "get should get what insert set");
	dkhash_insert(t, "kalle", "bananas", p1);
	p2 = dkhash_get(t, "kalle", "bezinga");
	test(p1 != p2, "we should never get the wrong key");
	ok_int(2, dkhash_num_entries(t), "should be 2 entries after 2 inserts");
	p2 = dkhash_remove(t, "kalle", "bezinga");
	ok_int(2, dkhash_num_entries(t), "should be 2 entries after 2 inserts and 1 failed remove");
	ok_int(0, dkhash_num_entries_removed(t), "should be 0 removed entries after failed remove");
	p2 = dkhash_remove(t, "kalle", "bananas");
	test(p1 == p2, "dkhash_remove() should return removed data");
	ok_int(dkhash_num_entries(t), 1, "should be 1 entries after 2 inserts and 1 successful remove");
	p2 = dkhash_remove(t, "nisse", NULL);
	test(p1 == p2, "dkhash_remove() should return removed data");
	ret = t_end();

	t_reset();
	/* lots of tests below, so we shut up while they're running */
	t_verbose = 0;

	t_start("dkhash_walk_data() test");
	memset(&s, 0, sizeof(s));
	/* first we set up the dkhash-tables */
	tx = dkhash_create(16);
	for (x = 0; x < ARRAY_SIZE(keys); x++) {
		dkhash_insert(tx, keys[x].k1, NULL, ddup(x, 0, 0));
		dkhash_insert(tx, keys[x].k2, NULL, ddup(x, 0, 0));
		dkhash_insert(tx, keys[x].k1, keys[x].k2, ddup(x, 0, 0));
		s.x += 3;
		ok_int(s.x, dkhash_num_entries(tx), "x table adding");
	}

	ok_int(s.x, dkhash_num_entries(tx), "x table done adding");

	for (x = 0; x < ARRAY_SIZE(keys); x++) {
		del.x = x;
		del.i = del.j = 0;

		ok_int(s.x, dkhash_num_entries(tx), "x table pre-delete");
		s.x -= 3;
		dkhash_walk_data(tx, del_matching);
		ok_int(s.x, dkhash_num_entries(tx), "x table post-delete");
	}

	test(0 == dkhash_num_entries(tx), "x table post all ops");
	test(0 == dkhash_check_table(tx), "x table consistency post all ops");
	dkhash_debug_table(tx, 0);
	r2 = t_end();
	ret = r2 ? r2 : ret;

	t_reset();
	for(x = 0; x < 10; x++) {
		sprintf(tmp, "string %d", x);
		strs[x] = strdup(tmp);
	}

	t_start("dkhash single bucket add remove forward");

	t = dkhash_create(1);
	for(x = 0; x < 10; x++) {
		dkhash_insert(t, strs[x], NULL, strs[x]);
	}
	for(x = 0; x < 10; x++) {
		p1 = strs[x];
		p2 = dkhash_remove(t, p1, NULL);
		test(p1 == p2, "remove should return a value");
	}
	r2 = t_end();
	ret = r2 ? r2 : ret;
	t_reset();

	t_start("dkhash single bucket add remove backward");

	t = dkhash_create(1);
	for(x = 0; x < 10; x++) {
		dkhash_insert(t, strs[x], NULL, strs[x]);
	}
	for(x = 9; x; x--) {
		p1 = strs[x];
		p2 = dkhash_remove(t, p1, NULL);
		test(p1 == p2, "remove should return a value");
	}

	dkhash_destroy(t);

	r2 = t_end();
	return r2 ? r2 : ret;
}
