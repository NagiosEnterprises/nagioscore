#ifndef LIBNAGIOS_nsock_h__
#define LIBNAGIOS_nsock_h__
#include <errno.h>

#define NSOCK_EBIND    (-1)     /**< failed to bind() */
#define NSOCK_ELISTEN  (-2)     /**< failed to listen() */
#define NSOCK_ESOCKET  (-3)     /**< failed to socket() */
#define NSOCK_EUNLINK  (-4)     /**< failed to unlink() */
#define NSOCK_ECONNECT (-5)     /**< failed to connect() */
#define NSOCK_EINVAL (-EINVAL) /**< -22, normally */

/** flags for the various create calls */
#define NSOCK_TCP     (1 << 0)  /**< use tcp mode */
#define NSOCK_UDP     (1 << 1)  /**< use udp mode */
#define NSOCK_UNLINK  (1 << 2)  /**< unlink existing path (only nsock_unix) */
#define NSOCK_REUSE   (1 << 2)  /**< reuse existing address */
#define NSOCK_CONNECT (1 << 3)  /**< connect rather than create */

/**
 * Create or connect to a unix socket
 *
 * @param path The path to connect to or create
 * @param mask The umask to use when creating the new socket
 * @param flags Various options controlling the mode of the socket
 * @return An NSOCK_E macro on errors, the created socket on succes
 */
extern int nsock_unix(const char *path, unsigned int mask, unsigned int flags);
#endif /* LIBNAGIOS_nsock_h__ */
