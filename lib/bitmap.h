#ifndef LIBNAGIOS_bitmap_h__
#define LIBNAGIOS_bitmap_h__
/**
 * @file bitmap.h
 * @brief Bit vector API
 * @{
 */
struct bitmap;
typedef struct bitmap bitmap;

/**
 * Create a bitmaptor of size 'size'
 * @param size Desired storage capacity
 * @return A bitmap pointer on success, NULL on errors
 */
extern bitmap *bitmap_create(unsigned long size);

/**
 * Destroy a bitmaptor by freeing all the memory it uses
 * @param bm The bitmaptor to destroy
 */
extern void bitmap_destroy(bitmap *bm);

/**
 * Copy a bitmaptor
 * @param bm The bitmaptor to copy
 * @return Pointer to an identical bitmap on success, NULL on errors
 */
extern bitmap *bitmap_copy(const bitmap *bm);

/**
 * Set a bit in the vector
 * @param bm The bitmaptor to operate on
 * @param pos Position of the bit to set
 * @return 0 on success, -1 on errors
 */
extern int bitmap_set(bitmap *bm, unsigned long pos);

/**
 * Check if a particular bit is set in the vector
 * @param bm The bitmaptor to check
 * @param pos Position of the bit to check
 * @return 1 if set, otherwise 0
 */
extern int bitmap_isset(const bitmap *bm, unsigned long pos);

/**
 * Unset a particular bit in the vector
 * @param bm The bitmaptor to operate on
 * @param pos Position of the bit to unset
 */
extern int bitmap_unset(bitmap *bm, unsigned long pos);

/**
 * Obtain cardinality (max number of elements) of the bitmaptor
 * @param bm The bitmaptor to check
 * @return The cardinality of the bitmaptor
 */
extern unsigned long bitmap_cardinality(const bitmap *bm);
#define bitmap_size bitmap_cardinality

/**
 * Count set bits in vector. Completed in O(n/8) time.
 * @param bm The bitmaptor to count bits in
 * @return The number of set bits
 */
extern unsigned long bitmap_count_set_bits(const bitmap *bm);

/**
 * Count unset bits in vector. Completed in O(n/8) time.
 * @param bm The bitmaptor to count bits in
 * @return The number of set bits
 */
extern unsigned long bitmap_count_unset_bits(const bitmap *bm);

/**
 * Unset all bits in a bitmaptor
 * @param bm The bitmaptor to clear
 */
extern bitmap *bitmap_clear(bitmap *bm);

/**
 * Calculate intersection of two bitmaptors
 * The intersection is defined as all bits that are members of
 * both A and B. It's equivalent to bitwise AND.
 * This function completes in O(n/sizeof(long)) operations.
 * @param a The first bitmaptor
 * @param b The second bitmaptor
 * @return NULL on errors; A newly created bitmaptor on success.
 */
extern bitmap *bitmap_intersect(const bitmap *a, const bitmap *b);

/**
 * Calculate union of two bitmaptors
 * The union is defined as all bits that are members of
 * A or B or both A and B. It's equivalent to bitwise OR.
 * This function completes in O(n/sizeof(long)) operations.
 * @param a The first bitmaptor
 * @param b The second bitmaptor
 * @return NULL on errors; A newly created bitmaptor on success.
 */
extern bitmap *bitmap_union(const bitmap *a, const bitmap *b);

/**
 * Calculate set difference between two bitmaptors
 * The set difference of A / B is defined as all members of A
 * that isn't members of B. Note that parameter ordering matters
 * for this function.
 * This function completes in O(n/sizeof(long)) operations.
 * @param a The first bitmaptor (numerator)
 * @param b The first bitmaptor (denominator)
 * @return NULL on errors; A newly created bitmaptor on success.
 */
extern bitmap *bitmap_diff(const bitmap *a, const bitmap *b);

/**
 * Calculate symmetric difference between two bitmaptors
 * The symmetric difference between A and B is the set that
 * contains all elements in either set but not in both.
 * This function completes in O(n/sizeof(long)) operations.
 * @param a The first bitmaptor
 * @param b The second bitmaptor
 */
extern bitmap *bitmap_symdiff(const bitmap *a, const bitmap *b);

/**
 * Compare two bitmaptors for equality
 * @param a The first bitmaptor
 * @param b The other bitmaptor
 * @return Similar to memcmp(), with tiebreaks determined by cardinality
 */
extern int bitmap_cmp(const bitmap *a, const bitmap *b);
/** @} */
#endif /* LIBNAGIOS_bitmap_h__ */
