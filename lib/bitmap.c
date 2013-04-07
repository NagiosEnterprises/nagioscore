#include <assert.h>
#include "bitmap.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifndef CHAR_BIT
# define CHAR_BIT 8
#endif

typedef unsigned int bmap;

#define MAPSIZE (sizeof(bmap) * CHAR_BIT)
#define MAPMASK (MAPSIZE - 1) /* bits - 1, so 63 for 64-bit machines */
#define SHIFTOUT (MAPSIZE == 64 ? 6 : 5) /* log2(bits) */

struct bitmap {
	bmap *vector;
	unsigned long alloc;
};

void bitmap_clear(bitmap *bm)
{
	if (bm)
		memset(bm->vector, 0, bm->alloc * sizeof(bmap));
}

int bitmap_resize(bitmap *bm, unsigned long size)
{
	unsigned long ralloc;
	bmap *nvec;

	if (!bm)
		return -1;

	/* be tight on space */
	ralloc = (size >> SHIFTOUT) + !!(size & MAPMASK);

	if (!bm->vector) {
		bm->vector = calloc(1, ralloc * sizeof(bmap));
		if (!bm->vector)
			return -1;
		bm->alloc = ralloc;
		return 0;
	}

	nvec = realloc(bm->vector, ralloc * sizeof(bmap));
	if (!nvec) {
		return -1;
	}
	bm->vector = nvec;
	bm->alloc = ralloc;
	return 0;
}

static bitmap *bitmap_init(bitmap *bm, unsigned long size)
{
	int ret;

	if (!bm)
		return NULL;

	ret = bitmap_resize(bm, size);
	if (ret < 0)
		return NULL;

	return bm;
}

bitmap *bitmap_create(unsigned long size)
{
	bitmap *bm;

	if (!(bm = calloc(1, sizeof(bitmap))))
		return NULL;

	if (bitmap_init(bm, size) == bm)
		return bm;

	free(bm);

	return NULL;
}

void bitmap_destroy(bitmap *bm)
{
	if (!bm)
		return;
	if (bm->vector)
		free(bm->vector);
	free(bm);
}

bitmap *bitmap_copy(const bitmap *bm)
{
	bitmap *ret;

	if (!bm)
		return NULL;
	ret = bitmap_create(bitmap_cardinality(bm));
	if (!ret)
		return NULL;

	memcpy(ret->vector, bm->vector, bitmap_size(bm));
	return ret;
}

static inline unsigned int l_bits(bmap map)
{
	unsigned int i, tot_bits = 0;

	/*
	 * bits per byte. A 16k entry table would be slightly faster
	 * on most archs but a damn sight uglier on all of them
	 */
	const unsigned char bpb[256] = {
		0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
	};

	for (i = 0; i < sizeof(bmap); i++) {
		const unsigned char ch = (map >> (i * CHAR_BIT)) & 0xff;
		const unsigned char cbits = bpb[ch];
		tot_bits += cbits;
	}

	return tot_bits;
}


int bitmap_set(bitmap *bm, unsigned long pos)
{
	const bmap l = pos >> SHIFTOUT;
	const unsigned int bit = pos & MAPMASK;

	if (!bm)
		return 0;
	if (l > bm->alloc)
		return -1;

	bm->vector[l] |= (1 << bit);
	return 0;
}

int bitmap_isset(const bitmap *bm, unsigned long pos)
{
	const bmap l = pos >> SHIFTOUT;
	const int bit = pos & MAPMASK;
	int set;

	if (!bm || l > bm->alloc)
		return 0;

	set = !!(bm->vector[l] & (1 << bit));
	return set;
}

int bitmap_unset(bitmap *bm, unsigned long pos)
{
	const bmap l = pos >> SHIFTOUT;
	const int bit = pos & MAPMASK;
	const int val = bitmap_isset(bm, pos);

	bm->vector[l] &= ~(1 << bit);
	return val;
}

unsigned long bitmap_cardinality(const bitmap *bm)
{
	if (!bm)
		return 0;

	return bm->alloc * MAPSIZE;
}

/*
 * count set bits in alloc * (mapsize / 8) ops
 */
unsigned long bitmap_count_set_bits(const bitmap *bm)
{
	unsigned long i, set_bits = 0;

	if (!bm)
		return 0;

	for (i = 0; i < bm->alloc; i++) {
		set_bits += l_bits(bm->vector[i]);
	}

	return set_bits;
}

unsigned long bitmap_count_unset_bits(const bitmap *bm)
{
	return bitmap_cardinality(bm) - bitmap_count_set_bits(bm);
}

#define BITMAP_MATH(a, b) \
	unsigned int i; \
	bitmap *bm; \
	/* a->alloc has to be smallest */ \
	if (a->alloc > b->alloc) { \
		const bitmap *temp = b; \
		b = a; \
		a = temp; \
	} \
	bm = bitmap_create(bitmap_cardinality(b)); \
	if (!bm) \
		return NULL; \
	for (i = 0; i < a->alloc; i++)

bitmap *bitmap_intersect(const bitmap *a, const bitmap *b)
{
	BITMAP_MATH(a, b) {
		bm->vector[i] = a->vector[i] & b->vector[i];
	}

	return bm;
}


bitmap *bitmap_union(const bitmap *a, const bitmap *b)
{
	if(!a)
		return bitmap_copy(b);
	if(!b)
		return bitmap_copy(a);
	do {
		BITMAP_MATH(a, b) {
			bm->vector[i] = a->vector[i] | b->vector[i];
		}
		return bm;
	} while(0);
}

bitmap *bitmap_unite(bitmap *res, const bitmap *addme)
{
	unsigned int i;

	if(!addme || !res)
		return res;

	if (bitmap_size(addme) > bitmap_size(res)) {
		bitmap_resize(res, bitmap_size(addme));
	}

	for (i = 0; i < addme->alloc; i++) {
		res->vector[i] |= addme->vector[i];
	}
	return res;
}

/*
 * set difference gets everything in A that isn't also in B. A is the
 * numerator, so if it's larger we must include any overflow in the
 * resulting set.
 */
bitmap *bitmap_diff(const bitmap *a, const bitmap *b)
{
	const bitmap *a_ = a, *b_ = b;

	BITMAP_MATH(a, b) {
		bm->vector[i] = a->vector[i] & ~(b->vector[i]);
	}
	if (a_->alloc > b_->alloc) {
		memcpy(&bm->vector[i], &b->vector[i], (b->alloc - a->alloc) * MAPSIZE);
	}
	return bm;
}

/*
 * symmetric set difference lists all items only present in one set
 */
bitmap *bitmap_symdiff(const bitmap *a, const bitmap *b)
{
	BITMAP_MATH(a, b) {
		bm->vector[i] = (a->vector[i] | b->vector[i]) ^ (a->vector[i] & b->vector[i]);
	}
	if (b->alloc > a->alloc) {
		memcpy(&bm->vector[i], &b->vector[i], (b->alloc - a->alloc) * MAPSIZE);
	}
	return bm;
}

#define min(a, b) (a > b ? b : a)
int bitmap_cmp(const bitmap *a, const bitmap *b)
{
	int ret;

	ret = memcmp(a->vector, b->vector, min(a->alloc, b->alloc) * MAPSIZE);
	if (ret || a->alloc == b->alloc) {
		return ret;
	}
	if (a->alloc > b->alloc)
		return 1;
	return -1;
}
