#ifndef INCLUDE_kvvec_h__
#define INCLUDE_kvvec_h__
struct key_value {
	char *key, *value;
	int key_len, value_len;
};

struct kvvec_buf {
	char *buf;
	unsigned long buflen;
	unsigned long bufsize;
};

struct kvvec {
	struct key_value *kv;
	int kv_alloc;
	int kv_pairs;
	int kvv_sorted;
	int combined_len; /* used for kvvec_buf to avoid one loop */
};

/** Parameters for kvvec_destroy() */
#define KVVEC_FREE_KEYS   1
#define KVVEC_FREE_VALUES 2
#define KVVEC_FREE_ALL    (KVVEC_FREE_KEYS | KVVEC_FREE_VALUES)

/**
 * Initialize a key/value vector
 * @param hint Number of key/value pairs we expect to store
 * @return Pointer to a struct kvvec, properly initialized
 */
extern struct kvvec *kvvec_init(int hint);

/**
 * Grow a key/value vector. Used internally as needed by
 * the kvvec api.
 *
 * @param kvv The key/value vector to grow
 * @param hint The new size we should grow to
 * @return 0 on success, < 0 on errors
 */
extern int kvvec_grow(struct kvvec *kvv, int hint);

/**
 * Sort a key/value vector alphabetically by key name
 * @param kvv The key/value vector to sort
 * @return 0
 */
extern int kvvec_sort(struct kvvec *kvv);

/**
 * Add a key/value pair to an existing key/value vector, with
 * lengths of strings already calculated
 * @param kvv The key/value vector to add this key/value pair to
 * @param key The key
 * @param keylen Length of the key
 * @param value The value
 * @param valuelen Length of the value
 * @return 0 on success, < 0 on errors
 */
extern int kvvec_addkv_wlen(struct kvvec *kvv, char *key, int keylen, char *value, int valuelen);

/**
 * Shortcut to kvvec_addkv_wlen() when lengths aren't known
 * @param kvv The key/value vector to add this key/value pair to
 * @param key The key
 * @param value The value
 * @return 0 on success, < 0 on errors
 */
#define kvvec_addkv(kvv, key, value) kvvec_addkv_wlen(kvv, key, 0, value, 0)

/**
 * Walk each key/value pair in a key/value vector, sending them
 * as arguments to a callback function. The callback function has
 * no control over the iteration process and must not delete or
 * modify the key/value vector it's operating on.
 * @param kvv The key/value vector to walk
 * @param arg Extra argument to the callback function
 * @param callback Callback function
 * @return 0 on success, < 0 on errors
 */
extern int kvvec_foreach(struct kvvec *kvv, void *arg, int (*callback)(struct key_value *, void *));

/**
 * Destroy a key/value vector
 * @param kvv The key/value vector to destroy
 * @param flags or'ed combination of KVVEC_FREE_{KEYS,VALUES}, or KVVEC_FREE_ALL
 * @return 0 on success, < 0 on errors
 */
extern int kvvec_destroy(struct kvvec *kvv, int flags);

/**
 * Create a linear buffer of all the key/value pairs and
 * return it as a kvvec_buf. The caller must free() all
 * pointers in the returned kvvec_buf
 * (FIXME: add kvvec_buf_destroy(), or move this and its counterpart
 * out of the kvvec api into a separate one)
 *
 * @param kvv The key/value vector to convert
 * @param kv_sep Character separating keys and their values
 * @param pair_sep Character separating key/value pairs
 * @param overalloc Integer determining how much extra data we should
 *                  allocate. The overallocated memory is filled with
 *                  nul bytes.
 * @return A pointer to a newly created kvvec_buf structure
 */
extern struct kvvec_buf *kvvec2buf(struct kvvec *kvv, char kv_sep, char pair_sep, int overalloc);

/**
 * Create a key/value vector from a pre-parsed buffer. Immensely
 * useful for ipc in combination with kvvec2buf().
 *
 * @param str The buffer to convert to a key/value vector
 * @param len Length of buffer to convert
 * @param kv_sep Character separating key and value
 * @param pair_sep Character separating key/value pairs
 */
extern struct kvvec *buf2kvvec(const char *str, unsigned int len, const char kvsep, const char pair_sep);

#endif /* INCLUDE_kvvec_h__ */
