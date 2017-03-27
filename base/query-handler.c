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
	const char *description; /* short description of this handler */
	unsigned int options;
	qh_handler handler;
	struct query_handler *prev_qh, *next_qh;
};

static struct query_handler *qhandlers;
static int qh_listen_sock = -1; /* the listening socket */
static unsigned int qh_running;
unsigned int qh_max_running = 0; /* defaults to unlimited */
static dkhash_table *qh_table;

/* the echo service. stupid, but useful for testing */
static int qh_echo(int sd, char *buf, unsigned int len)
{
	if (!strcmp(buf, "help")) {
		nsock_printf_nul(sd,
			"Query handler that simply echoes back what you send it.");
		return 0;
	}
	(void)write(sd, buf, len);
	return 0;
}

static struct query_handler *qh_find_handler(const char *name)
{
	return (struct query_handler *)dkhash_get(qh_table, name, NULL);
}

/* subset of http error codes */
const char *qh_strerror(int code)
{
	if (code < 0)
		return "Low-level system error";

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
	case 401: return "Unauthorized";
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
		worker_set_sockopts(nsd, 0);
		qh_running++;
		return 0;
	}
	else {
		int result;
		unsigned long len;
		unsigned int query_len = 0;
		char *buf, *space;
		struct query_handler *qh;
		char *handler = NULL, *query = NULL;

		result = iocache_read(ioc, sd);
		/* disconnect? */
		if(result == 0 || (result < 0 && errno == EPIPE)) {
			iocache_destroy(ioc);
			iobroker_close(nagios_iobs, sd);
			qh_running--;
			return 0;
		}

		/*
		 * A request looks like this: '[@|#]<qh>[<SP>][<query>]\0'.
		 * That is, optional '#' (oneshot) or '@' (keepalive),
		 * followed by the name of a registered handler, followed by
		 * an optional space and an optional query. If the handler
		 * has no "default" handler, a query is required or an error
		 * will be thrown.
		 */

		/* Use data up to the first nul byte */
		buf = iocache_use_delim(ioc, "\0", 1, &len);
		if(!buf)
			return 0;

		/* Identify handler part and any magic query bytes */
		if (*buf == '@' || *buf == '#') {
			handler = buf + 1;
		}

		/* Locate query (if any) */
		if((space = strchr(buf, ' '))) {
			*space = 0;
			query = space + 1;
			query_len = len - ((unsigned long)query - (unsigned long)buf);
		} else {
			query = "";
			query_len = 0;
		}

		/* locate the handler */
		if(!(qh = qh_find_handler(handler))) {
			/* not found. that's a 404 */
			nsock_printf(sd, "404: %s: No such handler", handler);
			iobroker_close(nagios_iobs, sd);
			iocache_destroy(ioc);
			return 0;
		}

		/* strip trailing newlines */
		while (query_len > 0 && (query[query_len - 1] == 0 || query[query_len - 1] == '\n'))
			query[--query_len] = 0;

		/* now pass the query to the handler */
		if ((result = qh->handler(sd, query, query_len)) >= 100) {
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
	struct query_handler *qh, *next, *prev;

	if (!(qh = dkhash_remove(qh_table, name, NULL)))
		return 0;

	next = qh->next_qh;
	prev = qh->prev_qh;
	if (next)
		next->prev_qh = prev;
	if (prev)
		prev->next_qh = next;
	else
		qhandlers = next;

	free(qh);

	return 0;
}

int qh_register_handler(const char *name, const char *description, unsigned int options, qh_handler handler)
{
	struct query_handler *qh;
	int result;

	if(!name)
		return -1;

	if(!handler) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "qh: Failed to register handler '%s': No handler function specified\n", name);
		return -1;
	}

	if(strlen(name) > 128) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "qh: Failed to register handler '%s': Name too long\n", name);
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
	qh->description = description;
	qh->handler = handler;
	qh->options = options;
	qh->next_qh = qhandlers;
	if (qhandlers)
		qhandlers->prev_qh = qh;
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
		qh_deregister_handler(qh->name);
	}
	dkhash_destroy(qh_table);
	qh_table = NULL;
	qhandlers = NULL;

	if(!path)
		return;

	unlink(path);
}

static int qh_help(int sd, char *buf, unsigned int len)
{
	struct query_handler *qh;

	if (!*buf || !strcmp(buf, "help")) {
		nsock_printf_nul(sd,
			"  help <name>   show help for handler <name>\n"
			"  help list     list registered handlers\n");
		return 0;
	}

	if (!strcmp(buf, "list")) {
		for (qh = qhandlers; qh; qh = qh->next_qh) {
			nsock_printf(sd, "%-10s %s\n", qh->name, qh->description ? qh->description : "(No description available)");
		}
		nsock_printf(sd, "%c", 0);
		return 0;
	}

	if (!(qh = qh_find_handler(buf))) {
		nsock_printf_nul(sd, "No handler named '%s' is registered\n", buf);
	} else if (qh->handler(sd, "help", 4) > 200) {
		nsock_printf_nul(sd, "The handler %s doesn't have any help yet.", buf);
	}

	return 0;
}

static int qh_core(int sd, char *buf, unsigned int len)
{
	char *space;

	if (!*buf || !strcmp(buf, "help")) {
		nsock_printf_nul(sd, "Query handler for manipulating nagios core.\n"
			"Available commands:\n"
			"  loadctl           Print information about current load control settings\n"
			"  loadctl <options> Configure nagios load control.\n"
			"                    The options are the same parameters and format as\n"
			"                    returned above.\n"
			"  squeuestats       scheduling queue statistics\n"
		);
		return 0;
	}
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

	if (!space && !strcmp(buf, "squeuestats"))
		return dump_event_stats(sd);

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
		logit(NSLOG_RUNTIME_ERROR, TRUE, "qh: query_socket is NULL. What voodoo is this?\n");
		return ERROR;
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
		logit(NSLOG_RUNTIME_ERROR, TRUE, "qh: Failed to register socket with io broker: %s; errno=%d: %s\n", iobroker_strerror(result), errno, strerror(errno));
		return ERROR;
	}

	logit(NSLOG_INFO_MESSAGE, FALSE, "qh: Socket '%s' successfully initialized\n", path);

	/* now register our the in-core handlers */
	if(!qh_register_handler("core", "Nagios Core control and info", 0, qh_core))
		logit(NSLOG_INFO_MESSAGE, FALSE, "qh: core query handler registered\n");
	qh_register_handler("echo", "The Echo Service - What You Put Is What You Get", 0, qh_echo);
	qh_register_handler("help", "Help for the query handler", 0, qh_help);

	return 0;
}
