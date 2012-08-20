#include "t-utils.h"
#include "lnag-utils.h"
#include "bitmap.c"

#define PRIME 2089
int main(int argc, char **argv)
{
	bitmap *a = NULL, *b, *r_union, *r_diff, *r_symdiff, *r_intersect;
	int i;
	int sa[] = {    2, 3, 4, 1783, 1784, 1785 };
	int sb[] = { 1, 2, 3,          1784, 1785, 1786, 1790, 1791, 1792 };
	/*
	 * intersect:   2, 3,          1784, 1785
	 * union:    1, 2, 3, 4, 1783, 1784, 1785, 1786, 1790, 1791, 1792
	 * diff A/B:          4, 1783
	 * diff B/A: 1,                            1786, 1790, 1791, 1792
	 * symdiff:  1,          1783,             1786, 1790, 1791, 1792
	 */

	ok_int(bitmap_count_set_bits(a), 0, "counting bits in null vector a");
	a = bitmap_create(PRIME);
	b = bitmap_create(PRIME);
	ok_int(bitmap_count_set_bits(b), 0, "counting bits in empty vector b");

	t_set_colors(0);
	ok_int(bitmap_cardinality(a) > PRIME, 1, "bitmap cardinality test");
	for (i = 0; i < veclen(sa); i++) {
		bitmap_set(a, sa[i]);
	}
	ok_int(bitmap_count_set_bits(a), veclen(sa), "counting set bits for a");

	for (i = 0; i < veclen(sb); i++) {
		ok_int(0, bitmap_isset(b, sb[i]), "checking unset bit");
		bitmap_set(b, sb[i]);
		if (!ok_int(1, bitmap_isset(b, sb[i]), "set and isset should work"))
			printf("sb[i]: %d\n", sb[i]);
	}
	if (!ok_int(bitmap_count_set_bits(b), veclen(sb), "counting set bits for b")) {
		for (i = 0; i < PRIME; i++) {
			if (bitmap_isset(b, i)) {
				;
			}
		}
	}

	r_union = bitmap_union(a, b);
	ok_int(bitmap_count_set_bits(r_union), 11, "bitmap union sets the right amount of bits");
	for (i = 0; i < veclen(sa); i++) {
		ok_int(1, bitmap_isset(r_union, sa[i]), "union should have bits from a");
	}
	for (i = 0; i < veclen(sb); i++) {
		ok_int(1, bitmap_isset(r_union, sb[i]), "union should have bits from b");
	}

	r_diff = bitmap_diff(a, b);
	ok_int(bitmap_count_set_bits(r_diff), 2, "diff must set right amount of bits");

	r_symdiff = bitmap_symdiff(a, b);
	ok_int(bitmap_count_set_bits(r_symdiff), 7, "symdiff must set right amount of bits");

	r_intersect = bitmap_intersect(a, b);
	ok_int(bitmap_count_set_bits(r_intersect), 4, "intersect must set right amount of bits");

	t_end();
	return 0;
}
