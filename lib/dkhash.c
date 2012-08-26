#include <stdlib.h>
#include <string.h>
#include "dkhash.h"
#include "lnag-utils.h"

#define dkhash_func(k) hash((unsigned char *)k)
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
	unsigned int collisions;
};

/* struct data access functions */
unsigned int dkhash_collisions(dkhash_table *t)
{
	return t ? t->collisions : 0;
}

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
 * polynomial conversion ignoring overflows.
 * Pretty standard hash, once based on Ozan Yigit's sdbm() hash
 * but later modified for Nagios to produce better results on our
 * typical data.
 */
#define PRIME 509
static inline unsigned int hash(register const unsigned char *k)
{
	register unsigned int h = 0x123; /* magic */

	while (*k)
		h = *k++ + PRIME * h;

	return h;
}

static dkhash_bucket *dkhash_get_bucket(dkhash_table *t, const char *key, unsigned int slot)
{
	dkhash_bucket *bkt;

	for (bkt = t->buckets[slot]; bkt; bkt = bkt->next) {
		if (!strcmp(key, bkt->key))
			return bkt;
	}

	return NULL;
}

static dkhash_bucket *dkhash_get_bucket2(dkhash_table *t, const char *k1, const char *k2, unsigned int slot)
{
	dkhash_bucket *bkt;

	for (bkt = t->buckets[slot]; bkt; bkt = bkt->next) {
		if (!strcmp(k1, bkt->key) && bkt->key2 && !strcmp(k2, bkt->key2))
			return bkt;
		}

	return NULL;
}

int dkhash_insert(dkhash_table *t, const char *k1, const char *k2, void *data)
{
	unsigned int slot;
	dkhash_bucket *bkt;

	if (!t || !k1)
		return DKHASH_EINVAL;

	if (k2) {
		slot = dkhash_func2(k1, k2) & (t->num_buckets - 1);
		bkt = dkhash_get_bucket2(t, k1, k2, slot);
	}
	else {
		slot = dkhash_func(k1) & (t->num_buckets - 1);
		bkt = dkhash_get_bucket(t, k1, slot);
	}

	if (bkt)
		return DKHASH_EDUPE;

	if (!(bkt = malloc(sizeof(*bkt))))
		return DKHASH_ENOMEM;

	if (t->buckets[slot])
		t->collisions++; /* "soft" collision */

	t->added++;
	bkt->data = data;
	bkt->key = k1;
	bkt->key2 = k2;
	bkt->next = t->buckets[slot];
	t->buckets[slot] = bkt;

	if (++t->entries > t->max_entries)
		t->max_entries = t->entries;

	return DKHASH_OK;
}

void *dkhash_get(dkhash_table *t, const char *k1, const char *k2)
{
	dkhash_bucket *bkt;
	unsigned int slot;

	if (!t || !k1)
		return NULL;

	if (k2) {
		slot = dkhash_func2(k1, k2) & (t->num_buckets - 1);
		bkt = dkhash_get_bucket2(t, k1, k2, slot);
	}
	else {
		slot = dkhash_func(k1) & (t->num_buckets - 1);
		bkt = dkhash_get_bucket(t, k1, slot);
	}

	return bkt ? bkt->data : NULL;
}

dkhash_table *dkhash_create(unsigned int size)
{
	dkhash_table *t;
	unsigned int po2;

	if (!size)
		return NULL;

	t = calloc(1, sizeof(*t));

	/* we'd like size to be a power of 2, so we round up */
	po2 = rup2pof2(size);
	if (po2 < 16)
		po2 = 16;

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

int dkhash_destroy(dkhash_table *t)
{
	unsigned int i;

	if (!t)
		return DKHASH_EINVAL;

	for (i = 0; i < t->num_buckets; i++) {
		dkhash_bucket *b, *next;
		for (b = t->buckets[i]; b; b = next) {
			next = b->next;
			free(b);
		}
	}
	free(t->buckets);
	free(t);
	return DKHASH_OK;
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
	unsigned int slot;
	dkhash_bucket *bkt, *prev;

	if (!t || !k1)
		return NULL;

	slot = (k2 ? dkhash_func2(k1, k2) : dkhash_func(k1)) & (t->num_buckets - 1);
	if (!(bkt = t->buckets[slot]))
		return NULL;

	prev = bkt; /* pay attention */
	for (; bkt; bkt = bkt->next) {
		if (strcmp(k1, bkt->key))
			continue;
		if ((!k2 && !bkt->key2) || !strcmp(k2, bkt->key2)) {
			if (prev == bkt) {
				/* first entry deleted */
				t->buckets[slot] = bkt->next;
			}
			else {
				prev->next = bkt->next;
			}
			t->entries--;
			t->removed++;
			return dkhash_destroy_bucket(bkt);
		}
	}

	return NULL;
}

void dkhash_walk_data(dkhash_table *t, int (*walker)(void *)) {
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
