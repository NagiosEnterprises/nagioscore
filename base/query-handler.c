#include "include/config.h"
#include "include/nagios.h"
#include "lib/libnagios.h"
#include "lib/nsock.h"
#include <unistd.h>
#include <stdlib.h>

/* A registered handler */
struct query_handler {
	const char *name; /* also "address" of this handler. Must be unique */
	unsigned int options;
	qh_handler handler;
};

static struct query_handler *qhandlers;
static int qh_listen_sock; /* the listening socket */
static unsigned int qh_running;
unsigned int qh_max_running = 0; /* defaults to unlimited */
static dkhash_table *qh_table;

/* the echo service. stupid, but useful for testing */
static int qh_echo(int sd, char *buf, unsigned int len)
{
	(void)write(sd, buf, len);
	return 0;
}

static struct query_handler *qh_find_handler(const char *name)
{
	return (struct query_handler *)dkhash_get(qh_table, name, NULL);
}

static int qh_input(int sd, int events, void *ioc_)
{
	iocache *ioc = (iocache *)ioc_;

	/* input on main socket, so accept one */
	if(sd == qh_listen_sock) {
		struct sockaddr sa;
		socklen_t slen = 0;
		int nsd;

		memset(&sa, 0, sizeof(sa)); /* shut valgrind up */
		nsd = accept(sd, &sa, &slen);
		if(qh_max_running && qh_running >= qh_max_running) {
			nsock_printf(nsd, "503: Server full");
			close(nsd);
			return 0;
		}

		if(!(ioc = iocache_create(16384))) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to create iocache for inbound query request\n");
			nsock_printf(nsd, "500: Internal server error");
			close(nsd);
			return 0;
		}

		if(iobroker_register(nagios_iobs, nsd, ioc, qh_input) < 0) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to register query input socket %d with I/O broker: %s\n", nsd, strerror(errno));
			close(nsd);
			return 0;
		}
		qh_running++;
		return 0;
	}
	else {
		int result;
		unsigned long len;
		char *buf, *space;
		struct query_handler *qh;

		result = iocache_read(ioc, sd);
		/* disconnect? */
		if(result == 0 || (result < 0 && errno == EPIPE)) {
			iocache_destroy(ioc);
			iobroker_close(nagios_iobs, sd);
			qh_running--;
			return 0;
		}

		/*
		 * A request looks like this: '@foo<SP><handler-specific request>\0'
		 * but we only need to care about the first part ("@foo"), so
		 * locate the first space and then pass the rest of the request
		 * to the handler
		 */
		buf = iocache_use_delim(ioc, "\0", 1, &len);
		if(*buf != '@') {
			/* bad request, so nuke the socket */
			nsock_printf(sd, "400: Bad request");
			iobroker_close(nagios_iobs, sd);
			iocache_destroy(ioc);
			return 0;
		}
		if((space = strchr(buf, ' ')))
			*(space++) = 0;

		if(!(qh = qh_find_handler(buf + 1))) {
			/* no handler. that's a 404 */
			nsock_printf(sd, "404: No such handler");
			return 0;
		}
		len -= strlen(buf);
		result = qh->handler(sd, space ? space : buf, len);
		if(result >= 300) {
			/* error codes, all of them */
			nsock_printf(sd, "%d: Seems '%s' doesn't like you. Oops...", result, qh->name);
			iobroker_close(nagios_iobs, sd);
			iocache_destroy(ioc);
		}
	}
	return 0;
}

int qh_deregister_handler(const char *name)
{
	struct query_handler *qh;

	qh = dkhash_get(qh_table, name, NULL);
	if(qh) {
		free(qh);
	}

	return 0;
}

int qh_register_handler(const char *name, unsigned int options, qh_handler handler)
{
	struct query_handler *qh;

	if(!name || !handler)
		return -1;

	if(strlen(name) > 128) {
		return -ENAMETOOLONG;
	}

	/* names must be unique */
	if(qh_find_handler(name)) {
		return -1;
	}

	if (!(qh = calloc(1, sizeof(*qh)))) {
		return -errno;
	}

	qh->name = name;
	qh->handler = handler;
	qh->options = options;
	qhandlers = qh;

	if(dkhash_insert(qh_table, qh->name, NULL, qh) < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to register query handler '%s': %s\n", name, strerror(errno));
		free(qh);
	}

	return 0;
}

int qh_init(const char *path)
{
	int result;

	if(qh_listen_sock >= 0)
		iobroker_close(nagios_iobs, qh_listen_sock);

	if(!path) /* not configured, so do nothing */
		return 0;

	errno = 0;
	qh_listen_sock = nsock_unix(path, 022, NSOCK_TCP | NSOCK_UNLINK);
	if(qh_listen_sock < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Init of query socket '%s' failed. %s: %s\n",
			  path, nsock_strerror(qh_listen_sock), strerror(errno));
		return ERROR;
	}

	/* most likely overkill, but it's small, so... */
	if(!(qh_table = dkhash_create(1024))) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to create hash table for query handler\n");
		close(qh_listen_sock);
		return ERROR;
	}

	errno = 0;
	result = iobroker_register(nagios_iobs, qh_listen_sock, NULL, qh_input);
	if(result < 0) {
		dkhash_destroy(qh_table);
		close(qh_listen_sock);
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to register qh socket with io broker: %s\n", iobroker_strerror(result));
		return ERROR;
	}

	logit(NSLOG_INFO_MESSAGE, TRUE, "Query socket '%s' successfully initialized\n", path);
	if(!qh_register_handler("echo", 0, qh_echo))
		logit(NSLOG_INFO_MESSAGE, TRUE, "Successfully registered echo service with query handler\n");

	return 0;
}
