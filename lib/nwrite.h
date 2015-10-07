#ifndef LIBNAGIOS_NWRITE_H_INCLUDED
#define LIBNAGIOS_NWRITE_H_INCLUDED

/**
 * @file nwrite.h
 * @brief Functions that properly handle incomplete write()'s
 *
 * Some functions simply use write() to send data through a socket.
 * These calls are sometimes interrupted, especially in the case of
 * an overly large buffer. Even though the write() _could_ finish,
 * the incomplete write is treated as an error. The functions here
 * properly handle those cases.
 *
 * @{
 */

/**
 * Send data through a socket
 * This function will send data through a socket and return
 * the number of bytes written.
 * @param sock The socket to write to
 * @param data The data to write
 * @param lth The length of the data
 * @param sent The number of bytes written (can be NULL)
 * @return The number of bytes written or -1 if error
 */
static inline ssize_t nwrite(int fd, const void *buf, size_t count, ssize_t *written)
{
	ssize_t	out, tot = 0;

	if (!buf || count == 0)
		return 0;

	while (tot < count) {
		out = write(fd, buf + tot, count - tot);
		if (out > 0)
			tot += out;
		else if(errno == EAGAIN || errno == EINTR)
			continue;
		else {
			if (written)
				*written = tot;
			return out;
		}
	}
	if (written)
		*written = tot;
	return tot;
}

/** @} */
#endif /* LIBNAGIOS_NWRITE_H_INCLUDED */
