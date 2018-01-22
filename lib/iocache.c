#include "iocache.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct iocache {
	char *ioc_buf; /* the data */
	unsigned long ioc_offset; /* where we're reading in the buffer */
	unsigned long ioc_buflen; /* the amount of data read into the buffer */
	unsigned long ioc_bufsize; /* size of the buffer */
};

void iocache_destroy(iocache *ioc)
{
	if (!ioc)
		return;

	if (ioc->ioc_buf)
		free(ioc->ioc_buf);

	free(ioc);
}

/**
 * Attempts to move data from the end to the beginning
 * of the ioc_buf, expelling old data to make more room
 * for new reads.
 */
static inline void iocache_move_data(iocache *ioc)
{
	unsigned long available;

	if (!ioc->ioc_offset)
		return; /* nothing to do */

	/* if we're fully read, we only have to reset the counters */
	if (ioc->ioc_buflen <= ioc->ioc_offset) {
		iocache_reset(ioc);
		return;
	}
	available = ioc->ioc_buflen - ioc->ioc_offset;
	memmove(ioc->ioc_buf, ioc->ioc_buf + ioc->ioc_offset, available);
	ioc->ioc_offset = 0;
	ioc->ioc_buflen = available;
}

void iocache_reset(iocache *ioc)
{
	if (ioc)
		ioc->ioc_offset = ioc->ioc_buflen = 0;
}

int iocache_resize(iocache *ioc, unsigned long new_size)
{
	char *buf;

	if (!ioc)
		return -1;

	iocache_move_data(ioc);

	buf = realloc(ioc->ioc_buf, new_size);
	if (!buf)
		return -1;
	ioc->ioc_buf = buf;
	ioc->ioc_bufsize = new_size;
	return 0;
}

int iocache_grow(iocache *ioc, unsigned long increment)
{
	return iocache_resize(ioc, iocache_size(ioc) + increment);
}

unsigned long iocache_size(iocache *ioc)
{
	return ioc ? ioc->ioc_bufsize : 0;
}

unsigned long iocache_capacity(iocache *ioc)
{
	if (!ioc || !ioc->ioc_buf || !ioc->ioc_bufsize)
		return 0;

	iocache_move_data(ioc);

	return ioc->ioc_bufsize - ioc->ioc_buflen;
}

unsigned long iocache_available(iocache *ioc)
{
	if (!ioc || !ioc->ioc_buf || !ioc->ioc_bufsize || !ioc->ioc_buflen)
		return 0;

	return ioc->ioc_buflen - ioc->ioc_offset;
}

char *iocache_use_size(iocache *ioc, unsigned long size)
{
	char *ret;

	if (!ioc || !ioc->ioc_buf)
		return NULL;
	if (ioc->ioc_bufsize < size || iocache_available(ioc) < size)
		return NULL;

	ret = ioc->ioc_buf + ioc->ioc_offset;
	ioc->ioc_offset += size;

	return ret;
}

int iocache_unuse_size(iocache *ioc, unsigned long size)
{
	if (!ioc || !ioc->ioc_buf)
		return -1;
	if (size > ioc->ioc_offset)
		return -1;

	ioc->ioc_offset -= size;

	return 0;
}


char *iocache_use_delim(iocache *ioc, const char *delim, size_t delim_len, unsigned long *size)
{
	char *ptr = NULL;
	char *buf;
	unsigned long remains;

	if (!ioc || !ioc->ioc_buf || !ioc->ioc_bufsize || !ioc->ioc_buflen)
		return NULL;

	*size = 0;
	if (ioc->ioc_offset >= ioc->ioc_buflen) {
		iocache_move_data(ioc);
		return NULL;
	}

	buf = &ioc->ioc_buf[ioc->ioc_offset];
	remains = iocache_available(ioc);
	while (remains >= delim_len) {
		unsigned long jump;
		ptr = memchr(buf, *delim, remains - (delim_len - 1));
		if (!ptr) {
			return NULL;
		}
		if (delim_len == 1 || !memcmp(ptr, delim, delim_len)) {
			unsigned long ioc_start;

			ioc_start = (unsigned long)ioc->ioc_buf + ioc->ioc_offset;
			*size = (unsigned long)ptr - ioc_start;

			/* make sure we use up all of the delimiter as well */
			return iocache_use_size(ioc, delim_len + *size);
		}
		jump = 1 + (unsigned long)ptr - (unsigned long)buf;
		remains -= jump;
		buf += jump;
	}
	return NULL;
}

iocache *iocache_create(unsigned long size)
{
	iocache *ioc;

	ioc = calloc(1, sizeof(*ioc));
	if (ioc && size) {
		ioc->ioc_buf = calloc(1, size);
		if (!ioc->ioc_buf) {
			free(ioc);
			return NULL;
		}
		ioc->ioc_bufsize = size;
	}

	return ioc;
}

int iocache_read(iocache *ioc, int fd)
{
	int to_read, bytes_read;

	if (!ioc || !ioc->ioc_buf || fd < 0)
		return -1;

	/* we make sure we've got as much room as possible */
	iocache_move_data(ioc);

	/* calculate the size we should read */
	to_read = ioc->ioc_bufsize - ioc->ioc_buflen;

	bytes_read = read(fd, ioc->ioc_buf + ioc->ioc_buflen, to_read);
	if (bytes_read > 0) {
		ioc->ioc_buflen += bytes_read;
	}

	return bytes_read;
}


int iocache_add(iocache *ioc, char *buf, unsigned int len)
{
	if (!ioc || iocache_capacity(ioc) < len)
		return -1;

	memcpy(ioc->ioc_buf + ioc->ioc_offset, buf, len);
	ioc->ioc_buflen += len;
	return ioc->ioc_buflen - ioc->ioc_offset;
}

/*
 * Three cases to handle:
 *  - buf has data, iocache doesn't.
 *  - iocache has data, buf doesn't.
 *  - both buf and iocache has data.
 */
int iocache_sendto(iocache *ioc, int fd, char *buf, unsigned int len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
	int sent;

	errno = 0;
	if (!ioc)
		return -1;

	if (!ioc->ioc_buflen && !len)
		return 0;

	if (ioc->ioc_buf && iocache_available(ioc)) {
		if (buf && len) {
			/* copy buf and len to iocache buffer to use just one write */
			if (iocache_capacity(ioc) < len) {
				if (iocache_grow(ioc, iocache_size(ioc)) < 0)
					return -1;
			}
			if (iocache_add(ioc, buf, len) < 0)
				return -1;
		}
		buf = ioc->ioc_buf;
		len = iocache_available(ioc);
	}

	sent = sendto(fd, buf, len, flags, dest_addr, addrlen);
	if (sent < 1)
		return -errno;

	if (iocache_available(ioc))
		iocache_use_size(ioc, sent);

	return sent;
}
