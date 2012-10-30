#include "include/config.h"
#include "include/nagios.h"
#include "lib/libnagios.h"
#include "lib/nsock.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

/* A registered handler */
struct query_handler {
	const char *name; /* also "address" of this handler. Must be unique */
	unsigned int options;
	qh_handler handler;
	struct query_handler *next_qh;
};

static struct query_handler *qhandlers;
static int qh_listen_sock = -1; /* the listening socket */
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

/* subset of http error codes */
static const char *qh_strerror(int code)
{
	if (code == 100)
		return "Continue";
	if (code == 101)
		return "Switching protocols";

	if (code < 300)
		return "OK";

	if (code < 400)
		return "Redirected (possibly deprecated address)";

	switch (code) {
		/* client errors */
	case 400: return "Bad request";
	case 401: return "Unathorized";
	case 403: return "Forbidden (disabled by config)";
	case 404: return "Not found";
	case 405: return "Method not allowed";
	case 406: return "Not acceptable";
	case 407: return "Proxy authentication required";
	case 408: return "Request timed out";
	case 409: return "Conflict";
	case 410: return "Gone";
	case 411: return "Length required";
	case 412: return "Precondition failed";
	case 413: return "Request too large";
	case 414: return "Request-URI too long";

		/* server errors */
	case 500: return "Internal server error";
	case 501: return "Not implemented";
	case 502: return "Bad gateway";
	case 503: return "Service unavailable";
	case 504: return "Gateway timeout";
	case 505: return "Version not supported";
	}
	return "Unknown error";
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
			logit(NSLOG_RUNTIME_ERROR, TRUE, "qh: Failed to create iocache for inbound request\n");
			nsock_printf(nsd, "500: Internal server error");
			close(nsd);
			return 0;
		}

		/*
		 * @todo: Stash the iocache and the socket in some
		 * addressable list so we can release them on deinit
		 */
		if(iobroker_register(nagios_iobs, nsd, ioc, qh_input) < 0) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "qh: Failed to register input socket %d with I/O broker: %s\n", nsd, strerror(errno));
			iocache_destroy(ioc);
			close(nsd);
			return 0;
		}

		/* make it non-blocking, but leave kernel buffers unchanged */
		set_socket_options(nsd, 0);
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
		 * to the handler, after cutting any trailing newlines
		 */
		buf = iocache_use_delim(ioc, "\0", 1, &len);
		if(!buf)
			return 0;

		if((*buf != '@' && *buf != '#') || !(space = strchr(buf, ' '))) {
			/* bad request, so nuke the socket */
			nsock_printf_nul(sd, "400: Bad request");
			iobroker_close(nagios_iobs, sd);
			iocache_destroy(ioc);
			return 0;
		}

		*(space++) = 0;

		if(!(qh = qh_find_handler(buf + 1))) {
			/* no handler. that's a 404 */
			nsock_printf(sd, "404: No such handler");
			iobroker_close(nagios_iobs, sd);
			iocache_destroy(ioc);
			return 0;
		}
		len -= strlen(buf);

		/* strip trailing newlines */
		while (space[len - 1] == 0 || space[len - 1] == '\n')
			space[--len] = 0;

		if ((result = qh->handler(sd, space, len)) >= 100) {
			nsock_printf_nul(sd, "%d: %s", result, qh_strerror(result));
		}

		if(result >= 300 || *buf == '#') {
			/* error code or one-shot query */
			iobroker_close(nagios_iobs, sd);
			iocache_destroy(ioc);
			return 0;
		}

		/* check for magic handler codes */
		switch (result) {
		case QH_CLOSE: /* oneshot handler */
		case -1:       /* general error */
			iobroker_close(nagios_iobs, sd);
			/* fallthrough */
		case QH_TAKEOVER: /* handler takes over */
		case 101:         /* switch protocol (takeover + message) */
			iocache_destroy(ioc);
			break;
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
	int result;

	if(!name || !handler)
		return -1;

	if(strlen(name) > 128) {
		return -ENAMETOOLONG;
	}

	/* names must be unique */
	if(qh_find_handler(name)) {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "qh: Handler '%s' registered more than once\n", name);
		return -1;
	}

	if (!(qh = calloc(1, sizeof(*qh)))) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "qh: Failed to allocate memory for handler '%s'\n", name);
		return -errno;
	}

	qh->name = name;
	qh->handler = handler;
	qh->options = options;
	qh->next_qh = qhandlers;
	qhandlers = qh;

	result = dkhash_insert(qh_table, qh->name, NULL, qh);
	if(result < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE,
			  "qh: Failed to insert query handler '%s' (%p) into hash table %p (%d): %s\n",
			  name, qh, qh_table, result, strerror(errno));
		free(qh);
		return result;
	}

	return 0;
}

void qh_deinit(const char *path)
{
	struct query_handler *qh, *next;

	for(qh = qhandlers; qh; qh = next) {
		next = qh->next_qh;
		free(qh);
	}

	if(!path)
		return;

	unlink(path);
}

static int qh_core(int sd, char *buf, unsigned int len)
{
	char *space;

	if ((space = memchr(buf, ' ', len)))
		*(space++) = 0;
	if (!space && !strcmp(buf, "loadctl")) {
		nsock_printf_nul
			(sd, "jobs_max=%u;jobs_min=%u;"
				"jobs_running=%u;jobs_limit=%u;"
				"load=%.2f;"
				"backoff_limit=%.2f;backoff_change=%u;"
				"rampup_limit=%.2f;rampup_change=%u;"
				"nproc_limit=%u;nofile_limit=%u;"
				"options=%u;changes=%u;",
				loadctl.jobs_max, loadctl.jobs_min,
				loadctl.jobs_running, loadctl.jobs_limit,
				loadctl.load[0],
				loadctl.backoff_limit, loadctl.backoff_change,
				loadctl.rampup_limit, loadctl.rampup_change,
				loadctl.nproc_limit, loadctl.nofile_limit,
				loadctl.options, loadctl.changes);
		return 0;
	}

	if (space) {
		len -= (unsigned long)space - (unsigned long)buf;
		if (!strcmp(buf, "loadctl")) {
			return set_loadctl_options(space, len) == OK ? 200 : 400;
		}
	}

	/* No matching command found */
	return 404;
}

int qh_init(const char *path)
{
	int result, old_umask;

	if(qh_listen_sock >= 0)
		iobroker_close(nagios_iobs, qh_listen_sock);

	if(!path) {
		/* not configured, so do nothing */
		logit(NSLOG_INFO_MESSAGE, TRUE, "qh: Query socket not enabled. Set 'query_socket=</path/to/query-socket>' in config (and stop whining, Robin).\n");
		return 0;
	}

	old_umask = umask(0117);
	errno = 0;
	qh_listen_sock = nsock_unix(path, NSOCK_TCP | NSOCK_UNLINK);
	umask(old_umask);
	if(qh_listen_sock < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "qh: Failed to init socket '%s'. %s: %s\n",
			  path, nsock_strerror(qh_listen_sock), strerror(errno));
		return ERROR;
	}

	/* plugins shouldn't have this socket */
	(void)fcntl(qh_listen_sock, F_SETFD, FD_CLOEXEC);

	/* most likely overkill, but it's small, so... */
	if(!(qh_table = dkhash_create(1024))) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "qh: Failed to create hash table\n");
		close(qh_listen_sock);
		return ERROR;
	}

	errno = 0;
	result = iobroker_register(nagios_iobs, qh_listen_sock, NULL, qh_input);
	if(result < 0) {
		dkhash_destroy(qh_table);
		close(qh_listen_sock);
		logit(NSLOG_RUNTIME_ERROR, TRUE, "qh: Failed to register socket with io broker: %s\n", iobroker_strerror(result));
		return ERROR;
	}

	logit(NSLOG_INFO_MESSAGE, FALSE, "qh: Socket '%s' successfully initialized\n", path);
	if(!qh_register_handler("echo", 0, qh_echo))
		logit(NSLOG_INFO_MESSAGE, FALSE, "qh: echo services successfully registered\n");

	if(!qh_register_handler("core", 0, qh_core))
		logit(NSLOG_INFO_MESSAGE, FALSE, "qh: core query handler registered\n");

	return 0;
}
