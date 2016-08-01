#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include "snprintf.h"
#include "lnag-utils.h"
#include "nsock.h"
#include <limits.h>

const char *nsock_strerror(int code)
{
	switch (code) {
	case NSOCK_EBIND: return "bind() failed";
	case NSOCK_ELISTEN: return "listen() failed";
	case NSOCK_ESOCKET: return "socket() failed";
	case NSOCK_EUNLINK: return "unlink() failed";
	case NSOCK_ECONNECT: return "connect() failed";
	case NSOCK_EFCNTL: return "fcntl() failed";
	case NSOCK_EINVAL: return "Invalid arguments";
	}

	return "Unknown error";
}

int nsock_unix(const char *path, unsigned int flags)
{
    static int listen_backlog = INT_MAX;
	struct sockaddr_un saun;
	struct sockaddr *sa;
	int sock = 0, mode;
	socklen_t slen;

	if(!path)
		return NSOCK_EINVAL;

	if(flags & NSOCK_TCP)
		mode = SOCK_STREAM;
	else if(flags & NSOCK_UDP)
		mode = SOCK_DGRAM;
	else
		return NSOCK_EINVAL;

	if((sock = socket(AF_UNIX, mode, 0)) < 0) {
		return NSOCK_ESOCKET;
	}

	/* set up the sockaddr_un struct and the socklen_t */
	sa = (struct sockaddr *)&saun;
	memset(&saun, 0, sizeof(saun));
	saun.sun_family = AF_UNIX;
	slen = strlen(path);
	memcpy(&saun.sun_path, path, slen);
	slen += offsetof(struct sockaddr_un, sun_path);

	/* unlink if we're supposed to, but not if we're connecting */
	if(flags & NSOCK_UNLINK && !(flags & NSOCK_CONNECT)) {
		if(unlink(path) < 0 && errno != ENOENT)
			return NSOCK_EUNLINK;
	}

	if(flags & NSOCK_CONNECT) {
		if(connect(sock, sa, slen) < 0) {
			close(sock);
			return NSOCK_ECONNECT;
		}
		return sock;
	} else {
		if(bind(sock, sa, slen) < 0) {
			close(sock);
			return NSOCK_EBIND;
		}
	}

	if(!(flags & NSOCK_BLOCK) && fcntl(sock, F_SETFL, O_NONBLOCK) < 0)
		return NSOCK_EFCNTL;

	if(flags & NSOCK_UDP)
		return sock;

    /* Default the backlog number on listen() to INT_MAX. If INT_MAX fails,
     * try using SOMAXCONN (usually 127) and if that fails, return an error */
    for (;;) {
        if (listen(sock, listen_backlog)) {
            if (listen_backlog == SOMAXCONN) {
                close(sock);
                return NSOCK_ELISTEN;
            } else
                listen_backlog = SOMAXCONN;
        }
        break;
    }

	return sock;
}

static inline int nsock_vprintf(int sd, const char *fmt, va_list ap, int plus)
{
	va_list ap_dup;
	char stack_buf[4096];
	int len;

	va_copy(ap_dup, ap);
	len = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap_dup);
	va_end(ap_dup);
	if (len < 0)
		return len;

	if (len < sizeof(stack_buf)) {
		stack_buf[len] = 0;
		if (plus) ++len; /* Include the nul byte if requested. */
		return write(sd, stack_buf, len);

	} else {
		char *heap_buf = NULL;

		len = vasprintf(&heap_buf, fmt, ap);
		if (len < 0)
			return len;

		if (plus) ++len; /* Include the nul byte if requested. */
		len = write(sd, heap_buf, len);
		free(heap_buf);
		return len;
	}
}

int nsock_printf_nul(int sd, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = nsock_vprintf(sd, fmt, ap, 1);
	va_end(ap);
	return ret;
}

int nsock_printf(int sd, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = nsock_vprintf(sd, fmt, ap, 0);
	va_end(ap);
	return ret;
}
