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
	{ "bar", "nitfol" },
	{ "andreas", "regerar" },
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
	dkhash_table *ti, *tj, *tx, *t;
	uint i, j, x;
	struct test_data s;
	char *p1, *p2;

	t_set_colors(0);
	t_start("dkhash basic test");
	t_verbose = 1;
	t = dkhash_create(512);

	p1 = strdup("a not-so secret value");
	dkhash_insert(t, "nisse", NULL, p1);
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
	t_end();

	/* lots of tests below, so we shut up while they're running */
	t_verbose = 0;

	t_start("dkhash_walk_data() test");
	memset(&s, 0, sizeof(s));
	/* first we set up the dkhash-tables */
	ti = dkhash_create(16);
	tj = dkhash_create(16);
	tx = dkhash_create(16);
	x = i = j = 0;
	for (x = 0; x < ARRAY_SIZE(keys); x++) {
		dkhash_insert(tx, keys[x].k1, NULL, ddup(x, 0, 0));
		dkhash_insert(tx, keys[x].k2, NULL, ddup(x, 0, 0));
		dkhash_insert(tx, keys[x].k1, keys[x].k2, ddup(x, 0, 0));
		s.x += 3;
		ok_int(s.x, dkhash_num_entries(tx), "x table adding");

		for (i = 0; i < ARRAY_SIZE(keys); i++) {
			dkhash_insert(ti, keys[i].k1, NULL, ddup(x, i, 0));
			dkhash_insert(ti, keys[i].k1, NULL, ddup(x, i, 0));
			dkhash_insert(ti, keys[i].k1, keys[i].k2, ddup(x, i, 0));
			s.i += 3;
			ok_int(s.i, dkhash_num_entries(ti), "i table adding");

			for (j = 0; j < ARRAY_SIZE(keys); j++) {
				dkhash_insert(tj, keys[j].k1, NULL, ddup(x, i, j));
				dkhash_insert(tj, keys[j].k2, NULL, ddup(x, i, j));
				dkhash_insert(tj, keys[j].k1, keys[j].k2, ddup(x, i, j));
				s.j += 3;
				ok_int(s.j, dkhash_num_entries(tj), "j table adding");
			}
		}
	}

	ok_int(s.x, dkhash_num_entries(tx), "x table done adding");
	ok_int(s.i, dkhash_num_entries(ti), "i table done adding");
	ok_int(s.j, dkhash_num_entries(tj), "j table done adding");

	for (x = 0; x < ARRAY_SIZE(keys); x++) {
		del.x = x;
		del.i = del.j = 0;

		ok_int(s.x, dkhash_num_entries(tx), "x table pre-delete");
		s.x -= 3;
		dkhash_walk_data(tx, del_matching);
		ok_int(s.x, dkhash_num_entries(tx), "x table post-delete");

		for (i = 0; i < ARRAY_SIZE(keys); i++) {
			del.i = i;
			del.j = 0;
			ok_int(s.i, dkhash_num_entries(ti), "i table pre-delete");
			dkhash_walk_data(ti, del_matching);
			s.i -= 3;
			ok_int(s.i, dkhash_num_entries(ti), "i table post-delete");

			for (j = 0; j < ARRAY_SIZE(keys); j++) {
				del.j = j;
				ok_int(s.j, dkhash_num_entries(tj), "j table pre-delete");
				dkhash_walk_data(tj, del_matching);
				s.j -= 3;
				ok_int(s.j, dkhash_num_entries(tj), "j table post-delete");
			}
		}
	}

	test(0 == dkhash_num_entries(tx), "x table post all ops");
	test(0 == dkhash_check_table(tx), "x table consistency post all ops");
	test(0 == dkhash_num_entries(ti), "i table post all ops");
	test(0 == dkhash_check_table(ti), "i table consistency post all ops");
	test(0 == dkhash_num_entries(tj), "j table post all ops");
	test(0 == dkhash_check_table(tj), "j table consistency post all ops");
	dkhash_debug_table(tx, 0);
	dkhash_debug_table(ti, 0);
	dkhash_debug_table(tj, 0);

	return t_end();
}
