#include <stdlib.h>
#include <string.h>
#include "dkhash.h"
#include "lnag-utils.h"

#define dkhash_func(k) sdbm((unsigned char *)k)
#define dkhash_func2(k1, k2) (dkhash_func(k1) ^ dkhash_func(k2))

typedef struct dkhash_bucket {
	const char *key;
	const char *key2;
	void *data;
	struct dkhash_bucket *next;
} dkhash_bucket;

struct dkhash_table {
	dkhash_bucket **buckets;
	unsigned int num_buckets;
	unsigned int added, removed;
	unsigned int entries;
	unsigned int max_entries;
};

/* struct data access functions */
unsigned int dkhash_num_entries(dkhash_table *t)
{
	return t ? t->entries : 0;
}

unsigned int dkhash_num_entries_max(dkhash_table *t)
{
	return t ? t->max_entries : 0;
}

unsigned int dkhash_num_entries_added(dkhash_table *t)
{
	return t ? t->added : 0;
}

unsigned int dkhash_num_entries_removed(dkhash_table *t)
{
	return t ? t->removed : 0;
}

unsigned int dkhash_table_size(dkhash_table *t)
{
	return t ? t->num_buckets : 0;
}

/*
 * polynomial conversion ignoring overflows
 *
 * Ozan Yigit's original sdbm algorithm, but without Duff's device
 * so we can use it without knowing the length of the string
 */
static inline unsigned int sdbm(register const unsigned char *k)
{
	register unsigned int h = 0;

	while (*k)
		h = *k++ + 65599 * h;

	return h;
}

static int dkhash_insert_bucket(dkhash_table *t, const char *k1, const char *k2, void *data, unsigned int h)
{
	dkhash_bucket *bkt;

	if (!(bkt = malloc(sizeof(*bkt))))
		return -1;

	h &= t->num_buckets - 1;

	t->added++;
	bkt->data = data;
	bkt->key = k1;
	bkt->key2 = k2;
	bkt->next = t->buckets[h];
	t->buckets[h] = bkt;

	if (++t->entries > t->max_entries)
		t->max_entries = t->entries;

	return 0;
}

int dkhash_insert(dkhash_table *t, const char *k1, const char *k2, void *data)
{
	unsigned int h = k2 ? dkhash_func2(k1, k2) : dkhash_func(k1);

	return dkhash_insert_bucket(t, k1, k2, data, h);
}

static dkhash_bucket *dkhash_get_bucket(dkhash_table *t, const char *key)
{
	dkhash_bucket *bkt;

	if (!t)
		return NULL;

	bkt = t->buckets[dkhash_func(key) % t->num_buckets];
	for (; bkt; bkt = bkt->next) {
		if (!strcmp(key, bkt->key))
			return bkt;
	}

	return NULL;
}

static dkhash_bucket *dkhash_get_bucket2(dkhash_table *t, const char *k1, const char *k2)
{
	dkhash_bucket *bkt;

	if (!t)
		return NULL;

	bkt = t->buckets[dkhash_func2(k1, k2) % t->num_buckets];
	for (; bkt; bkt = bkt->next) {
		if (!strcmp(k1, bkt->key) && !strcmp(k2, bkt->key2))
			return bkt;
	}

	return NULL;
}

void *dkhash_get(dkhash_table *t, const char *k1, const char *k2)
{
	dkhash_bucket *bkt;

	if (k2)
		bkt = dkhash_get_bucket2(t, k1, k2);
	else
		bkt = dkhash_get_bucket(t, k1);

	return bkt ? bkt->data : NULL;
}

dkhash_table *dkhash_create(unsigned int size)
{
	dkhash_table *t = calloc(sizeof(dkhash_table), 1);
	unsigned int po2;

	if (!size)
		return NULL;

	/* we'd like size to be a power of 2, so we round up */
	po2 = rup2pof2(size);

	if (t) {
		t->buckets = calloc(po2, sizeof(dkhash_bucket *));
		if (t->buckets) {
			t->num_buckets = po2;
			return t;
		}

		free(t);
	}

	return NULL;
}

void *dkhash_update(dkhash_table *t, const char *k1, const char *k2, void *data)
{
	dkhash_bucket *bkt;

	bkt = dkhash_get_bucket2(t, k1, k2);
	if (!bkt) {
		dkhash_insert(t, k1, k2, data);
		return NULL;
	}

	bkt->data = data;
	return NULL;
}

static inline void *dkhash_destroy_bucket(dkhash_bucket *bkt)
{
	void *data;

	data = bkt->data;
	free(bkt);
	return data;
}

void *dkhash_remove(dkhash_table *t, const char *k1, const char *k2)
{
	unsigned int h;
	dkhash_bucket *bkt, *prev;

	h = k2 ? dkhash_func2(k1, k2) : dkhash_func(k1);
	h &= t->num_buckets - 1;

	if (!(bkt = t->buckets[h]))
		return NULL;

	if (!strcmp(k1, bkt->key) && !strcmp(k2, bkt->key2)) {
		t->buckets[h] = bkt->next;
		t->entries--;
		t->removed++;
		return dkhash_destroy_bucket(bkt);
	}

	prev = bkt;
	for (bkt = bkt->next; bkt; bkt = bkt->next) {
		if (!strcmp(k1, bkt->key) && !strcmp(k2, bkt->key2)) {
			prev->next = bkt->next;
			t->entries--;
			t->removed++;
			return dkhash_destroy_bucket(bkt);
		}
	}

	return NULL;
}

void dkhash_walk_data(dkhash_table *t, int (*walker)(void *))
{
	dkhash_bucket *bkt, *prev;
	unsigned int i;

	if (!t->entries)
		return;

	for (i = 0; i < t->num_buckets; i++) {
		int depth = 0;

		prev = t->buckets[i];
		dkhash_bucket *next;
		for (bkt = t->buckets[i]; bkt; bkt = next) {
			next = bkt->next;

			if (walker(bkt->data) != DKHASH_WALK_REMOVE) {
				/* only update prev if we don't remove current */
				prev = bkt;
				depth++;
				continue;
			}
			t->removed++;
			t->entries--;
			dkhash_destroy_bucket(bkt);
			if (depth) {
				prev->next = next;
			}
			else {
				t->buckets[i] = next;
			}
		}
	}
}
