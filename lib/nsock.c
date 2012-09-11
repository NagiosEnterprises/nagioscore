#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include "lnag-utils.h"
#include "nsock.h"

int nsock_unix(const char *path, unsigned int mask, unsigned int flags)
{
	struct sockaddr_un saun;
	struct sockaddr *sa;
	int old_mask, sock = 0, mode;
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
	old_mask = umask(mask);
	sa = (struct sockaddr *)&saun;
	memset(&saun, 0, sizeof(saun));
	saun.sun_family = AF_UNIX;
	umask(old_mask);
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

	if(flags & NSOCK_UDP)
		return sock;

	if(listen(sock, 3) < 0) {
		close(sock);
		return NSOCK_ELISTEN;
	}

	return sock;
}
