#ifndef INCLUDE_iocache_h__
#define INCLUDE_iocache_h__
#include <stdlib.h>
#include <limits.h>

/**
 * @file iocache.h
 * @brief I/O cache function declarations
 *
 * The I/O cache library is useful for reading large chunks of data
 * from sockets and utilizing parts of that data based on either
 * size or a magic delimiter.
 *
 * @{
 */

/** opaque type for iocache operations */
struct iocache;
typedef struct iocache iocache;

/**
 * Destroys an iocache object, freeing all memory allocated to it.
 * @param ioc The iocache object to destroy
 */
extern void iocache_destroy(iocache *ioc);

/**
 * Resets an iocache struct, discarding all data in it without free()'ing
 * any memory.
 *
 * @param[in] ioc The iocache struct to reset
 */
extern void iocache_reset(iocache *ioc);

/**
 * Resizes the buffer in an io cache
 * @param ioc The io cache to resize
 * @param new_size The new size of the io cache
 * @return 0 on success, -1 on errors
 */
extern int iocache_resize(iocache *ioc, unsigned long new_size);

/**
 * Grows an iocache object
 * This uses iocache_resize() internally
 * @param[in] ioc The iocache to grow
 * @param[in] increment How much to increase it
 * @return 0 on success, -1 on errors
 */
extern int iocache_grow(iocache *ioc, unsigned long increment);

/**
 * Returns the total size of the io cache
 * @param[in] ioc The iocache to inspect
 * @return The size of the io cache. If ioc is null, 0 is returned
 */
extern unsigned long iocache_size(iocache *ioc);

/**
 * Returns remaining read capacity of the io cache
 * @param ioc The io cache to operate on
 * @return The number of bytes available to read
 */
extern unsigned long iocache_capacity(iocache *ioc);

/**
 * Return the amount of unread but stored data in the io cache
 * @param ioc The io cache to operate on
 * @return Number of bytes available to read
 */
extern unsigned long iocache_available(iocache *ioc);

/**
 * Use a chunk of data from iocache based on size. The caller
 * must take care not to write beyond the end of the requested
 * buffer, or Bad Things(tm) will happen.
 *
 * @param ioc The io cache we should use data from
 * @param size The size of the data we want returned
 * @return NULL on errors (insufficient data, fe). pointer on success
 */
extern char *iocache_use_size(iocache *ioc, unsigned long size);

/**
 * Use a chunk of data from iocache based on delimiter. The
 * caller must take care not to write beyond the end of the
 * requested buffer, if any is returned, or Bad Things(tm) will
 * happen.
 *
 * @param ioc The io cache to use data from
 * @param delim The delimiter
 * @param delim_len Length of the delimiter
 * @param size Length of the returned buffer
 * @return NULL on errors (delimiter not found, insufficient data). pointer on success
 */
extern char *iocache_use_delim(iocache *ioc, const char *delim, size_t delim_len, unsigned long *size);

/**
 * Creates the iocache object, initializing it with the given size
 * @param size Initial size of the iocache buffer
 * @return Pointer to a valid iocache object
 */
extern iocache *iocache_create(unsigned long size);

/**
 * Read data into the iocache buffer
 * @param ioc The io cache we should read into
 * @param fd The filedescriptor we should read from
 * @return The number of bytes read on success. < 0 on errors
 */
extern int iocache_read(iocache *ioc, int fd);

#endif /* INCLUDE_iocache_h__ */
/** @} */
