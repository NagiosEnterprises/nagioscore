/*
 * This file holds all nagios<->libnagios integration stuff, so that
 * libnagios itself is usable as a standalone library for addon
 * writers to use as they see fit.
 *
 * This means apis inside libnagios can be tested without compiling
 * all of Nagios into it, and that they can remain general-purpose
 * code that can be reused for other things later.
 */
#include "../include/config.h"
#include <string.h>
#include "../include/nagios.h"
#include "../include/workers.h"

/* perfect hash function for wproc response codes */
#include "wpres-phash.h"

struct wproc_worker;

struct wproc_job {
	unsigned int id;
	unsigned int type;
	unsigned int timeout;
	char *command;
	void *arg;
	struct wproc_worker *wp;
};

struct wproc_list;

struct wproc_worker {
	char *name; /**< check-source name of this worker */
	int sd;     /**< communication socket */
	pid_t pid;  /**< pid */
	int max_jobs; /**< Max number of jobs the worker can handle */
	int jobs_running; /**< jobs running */
	int jobs_started; /**< jobs started */
	int job_index; /**< round-robin slot allocator (this wraps) */
	iocache *ioc;  /**< iocache for reading from worker */
	fanout_table *jobs; /**< array of jobs */
	struct wproc_list *wp_list;
};

struct wproc_list {
	unsigned int len;
	unsigned int idx;
	struct wproc_worker **wps;
};

static struct wproc_list workers = {0, 0, NULL};

static dkhash_table *specialized_workers;
static struct wproc_list *to_remove = NULL;

typedef struct wproc_callback_job {
	void *data;
	void (*callback)(struct wproc_result *, void *, int);
} wproc_callback_job;

typedef struct wproc_object_job {
	char *contact_name;
	char *host_name;
	char *service_description;
} wproc_object_job;

unsigned int wproc_num_workers_online = 0, wproc_num_workers_desired = 0;
unsigned int wproc_num_workers_spawned = 0;

extern struct kvvec * macros_to_kvv(nagios_macros *);

#define tv2float(tv) ((float)((tv)->tv_sec) + ((float)(tv)->tv_usec) / 1000000.0)

static const char *wpjob_type_name(unsigned int type)
{
	switch (type) {
	case WPJOB_CHECK:
		return "CHECK";

	case WPJOB_NOTIFY:
		return "NOTIFY";

	case WPJOB_OCSP:
		return "OCSP";

	case WPJOB_OCHP:
		return "OCHP";

	case WPJOB_GLOBAL_SVC_EVTHANDLER:
		return "GLOBAL SERVICE EVENTHANDLER";

	case WPJOB_SVC_EVTHANDLER:
		return "SERVICE EVENTHANDLER";

	case WPJOB_GLOBAL_HOST_EVTHANDLER:
		return "GLOBAL HOST EVENTHANDLER";

	case WPJOB_HOST_EVTHANDLER:
		return "HOST EVENTHANDLER";

	case WPJOB_CALLBACK:
		return "CALLBACK";

	case WPJOB_HOST_PERFDATA:
		return "HOST PERFDATA";

	case WPJOB_SVC_PERFDATA:
		return "SERVICE PERFDATA";
	}
	return "UNKNOWN";
}

static void wproc_logdump_buffer(int level, int show, const char *prefix, char *buf)
{
	char *ptr, *eol;
	unsigned int line = 1;

	if (!buf || !*buf) {
		return;
	}

	for (ptr = buf; ptr && *ptr; ptr = eol ? eol + 1 : NULL) {
		if ((eol = strchr(ptr, '\n'))) {
			*eol = 0;
		}
		logit(level, show, "%s line %.02d: %s\n", prefix, line++, ptr);
		if (eol) {
			*eol = '\n';
		}
		else {
			break;
		}
	}
}

/* Try to reap 'jobs' jobs for 'msecs' milliseconds. Return early on error. */
void wproc_reap(int jobs, int msecs)
{
	struct timeval start;
	gettimeofday(&start, NULL);

	while (jobs > 0 && msecs > 0) {

		int inputs = iobroker_poll(nagios_iobs, msecs);
		if (inputs < 0) {
			return;
		}

		jobs -= inputs; /* One input is roughly equivalent to one job. */

		struct timeval now;
		gettimeofday(&now, NULL);

		msecs -= tv_delta_msec(&start, &now);
		start = now;
	}
}

int wproc_can_spawn(struct load_control *lc)
{
	unsigned int old = 0;
	time_t now;

	/* if no load control is enabled, we can safely run this job */
	if (!(lc->options & LOADCTL_ENABLED)) {
		return 1;
	}

	now = time(NULL);
	if (lc->last_check + lc->check_interval > now) {

		lc->last_check = now;

		if (getloadavg(lc->load, 3) < 0) {
			return lc->jobs_limit > lc->jobs_running;
		}

		if (lc->load[0] > lc->backoff_limit) {
			old = lc->jobs_limit;
			lc->jobs_limit -= lc->backoff_change;
		}

		else if (lc->load[0] < lc->rampup_limit) {
			old = lc->jobs_limit;
			lc->jobs_limit += lc->rampup_change;
		}

		if (lc->jobs_limit > lc->jobs_max) {
			lc->jobs_limit = lc->jobs_max;
		}

		else if (lc->jobs_limit < lc->jobs_min) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Tried to set jobs_limit to %u, below jobs_min (%u)\n",
				  lc->jobs_limit, lc->jobs_min);
			lc->jobs_limit = lc->jobs_min;
		}

		if (old && old != lc->jobs_limit) {
			if (lc->jobs_limit < old) {
				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: loadctl.jobs_limit changed from %u to %u\n", old, lc->jobs_limit);
			} else {
				logit(NSLOG_INFO_MESSAGE, FALSE, "wproc: loadctl.jobs_limit changed from %u to %u\n", old, lc->jobs_limit);
			}
		}
	}

	return lc->jobs_limit > lc->jobs_running;
}

static int get_job_id(struct wproc_worker *wp)
{
	return wp->job_index++;
}

static struct wproc_job *get_job(struct wproc_worker *wp, int job_id)
{
	return fanout_remove(wp->jobs, job_id);
}


static struct wproc_list *get_wproc_list(const char *cmd)
{
	struct wproc_list *wp_list = NULL;
	char *cmd_name             = NULL;
	char *slash                = NULL;
	char *space                = NULL;

	if (!specialized_workers) {
		return &workers;
	}

	/* first, look for a specialized worker for this command */
	space = strchr(cmd, ' ');
	if (space != NULL) {

		int namelen = (unsigned long) space - (unsigned long) cmd;

		cmd_name = calloc(1, namelen + 1);

		/* not exactly optimal, but what the hells */
		if (!cmd_name) {
			return &workers;
		}

		memcpy(cmd_name, cmd, namelen);
		slash = strrchr(cmd_name, '/');
	}

	wp_list = dkhash_get(specialized_workers, cmd_name ? cmd_name : cmd, NULL);
	if (!wp_list && slash) {
		wp_list = dkhash_get(specialized_workers, ++slash, NULL);
	}
	if (wp_list != NULL) {
		log_debug_info(DEBUGL_CHECKS, 1, "Found specialized worker(s) for '%s'", (slash && *slash != '/') ? slash : cmd_name);
	}
	if (cmd_name) {
		free(cmd_name);
	}

	if (wp_list) {
		return wp_list;
	}

	return &workers;
}

static struct wproc_worker *get_worker(const char *cmd)
{
	struct wproc_list *wp_list = NULL;

	log_debug_info(DEBUGL_WORKERS, 0, "get_worker()\n");

	if (!cmd) {
		log_debug_info(DEBUGL_WORKERS, 1, " * cmd is null, bailing\n");
		return NULL;
	}

	wp_list = get_wproc_list(cmd);
	if (!wp_list) {
		log_debug_info(DEBUGL_WORKERS, 1, " * wp_list is null, bailing\n");
		return NULL;
	}

	if (!wp_list->wps) {
		log_debug_info(DEBUGL_WORKERS, 1, " * wp_list->wps is null, bailing\n");
		return NULL;
	}

	if (!wp_list->len) {
		log_debug_info(DEBUGL_WORKERS, 2, " * wp_list->len is <= 0, bailing\n");
		return NULL;
	}

	return wp_list->wps[wp_list->idx++ % wp_list->len];
}

static struct wproc_job *create_job(int type, void *arg, time_t timeout, const char *cmd)
{
	struct wproc_job *job   = NULL;
	struct wproc_worker *wp = NULL;
	int result              = 0;

	log_debug_info(DEBUGL_WORKERS, 0, "create_job()\n");	

	wp = get_worker(cmd);
	if (wp == NULL) {
		return NULL;
	}

	job = calloc(1, sizeof(*job));
	if (job == NULL) {

		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to allocate memory for worker job: %s\n", strerror(errno));
		return NULL;
	}

	job->wp      = wp;
	job->id      = get_job_id(wp);
	job->type    = type;
	job->arg     = arg;
	job->timeout = timeout;

	result = fanout_add(wp->jobs, job->id, job);
	if (result < 0) {

		log_debug_info(DEBUGL_WORKERS, 1, " * Can't add job to wp->jobs, bailing\n");
		free(job);
		return NULL;
	}

	job->command = strdup(cmd);
	if (job->command == NULL) {

		log_debug_info(DEBUGL_WORKERS, 1, " * job command can't be null, bailing\n");
		free(job);
		return NULL;
	}

	return job;
}

static void run_job_callback(struct wproc_job *job, struct wproc_result *wpres, int val)
{
	wproc_callback_job *cj;

	log_debug_info(DEBUGL_WORKERS, 0, "run_job_callback()\n");

	if (job == NULL) {

		log_debug_info(DEBUGL_WORKERS, 1, " * job is null, bailing\n");
		return;
	}

	if (job->arg == NULL) {

		log_debug_info(DEBUGL_WORKERS, 1, " * job arg is null, bailing\n");
		return;
	}

	cj = (struct wproc_callback_job *)job->arg;

	if (cj->callback == NULL) {

		log_debug_info(DEBUGL_WORKERS, 1, " * callback_job callback is null, bailing\n");
		return;
	}

	cj->callback(wpres, cj->data, val);
	cj->callback = NULL;
}

static void destroy_job(struct wproc_job *job)
{
	if (job == NULL) {

		log_debug_info(DEBUGL_WORKERS, 1, " * destroy_job -> job is null, bailing\n");
		return;
	}

	log_debug_info(DEBUGL_WORKERS, 0, "destroy_job(%d=%s)\n", job->type, wpjob_type_name(job->type));

	switch (job->type) {
	case WPJOB_CHECK:
		free_check_result(job->arg);
		free(job->arg);
		break;

	case WPJOB_NOTIFY:
	case WPJOB_OCSP:
	case WPJOB_OCHP:
		free(job->arg);
		break;

	case WPJOB_GLOBAL_SVC_EVTHANDLER:
	case WPJOB_SVC_EVTHANDLER:
	case WPJOB_GLOBAL_HOST_EVTHANDLER:
	case WPJOB_HOST_EVTHANDLER:
	case WPJOB_HOST_PERFDATA:
	case WPJOB_SVC_PERFDATA:
		/* these require nothing special */
		break;

	case WPJOB_CALLBACK:
		/* call with NULL result to make callback clean things up */
		run_job_callback(job, NULL, 0);
		break;

	default:
		logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: Unknown job type: %d\n", job->type);
		break;
	}

	my_free(job->command);
	if (job->wp != NULL) {

		log_debug_info(DEBUGL_WORKERS, 1, " * removing job->wp\n");
		fanout_remove(job->wp->jobs, job->id);
		job->wp->jobs_running--;
	}

	loadctl.jobs_running--;

	free(job);
}

static void fo_destroy_job(void *job)
{
	destroy_job((struct wproc_job *)job);
}

static int wproc_is_alive(struct wproc_worker *wp)
{
	if (wp == NULL || wp->pid <= 0) {
		return 0;
	}

	if (kill(wp->pid, 0) == 0 && iobroker_is_registered(nagios_iobs, wp->sd))
		return 1;
	return 0;
}

static int wproc_destroy(struct wproc_worker *wp, int flags)
{
	int i     = 0;
	int force = 0;
	int self  = 0;

	if (!wp) {
		return 0;
	}

	force = !!(flags & WPROC_FORCE);

	self = getpid();

	/* master retains workers through restarts */
	if (self == nagios_pid && !force) {
		return 0;
	}

	/* free all memory when either forcing or a worker called us */
	iocache_destroy(wp->ioc);
	wp->ioc = NULL;
	my_free(wp->name);
	fanout_destroy(wp->jobs, fo_destroy_job);
	wp->jobs = NULL;

	/* workers must never control other workers, so they return early */
	if (self != nagios_pid) {
		return 0;
	}

	/* kill(0, SIGKILL) equals suicide, so we avoid it */
	if (wp->pid) {
		kill(wp->pid, SIGKILL);
	}

	iobroker_close(nagios_iobs, wp->sd);

	/* reap our possibly lost children */
	while (waitpid(-1, &i, WNOHANG) > 0)
		; /* do nothing */

	free(wp);

	return 0;
}

/* remove the worker list pointed to by to_remove */
static int remove_specialized(void *data)
{
	if (data == to_remove) {
		return DKHASH_WALK_REMOVE;
	}

	else if (to_remove == NULL) {

		/* remove all specialised workers and their lists */
		struct wproc_list *h = data;
		int i;

		for (i = 0; i < h->len; i++) {

			/* not sure what WPROC_FORCE is actually for.
			 * Nagios does *not* retain workers across
			 * restarts, as stated in wproc_destroy?
			 */
			wproc_destroy(h->wps[i], WPROC_FORCE);
		}

		h->len = 0;
		free(h->wps);
		free(h);

		return DKHASH_WALK_REMOVE;
	}

	return 0;
}

/* remove worker from job assignment list */
static void remove_worker(struct wproc_worker *worker)
{
	unsigned int i         = 0;
	unsigned int j         = 0;
	struct wproc_list *wpl = worker->wp_list;

	for (i = 0; i < wpl->len; i++) {

		if (wpl->wps[i] == worker) {
			continue;
		}

		wpl->wps[j++] = wpl->wps[i];
	}

	wpl->len = j;

	if (specialized_workers == NULL || wpl->len > 0) {
		return;
	}

	to_remove = wpl;
	dkhash_walk_data(specialized_workers, remove_specialized);
}


/*
 * This gets called from both parent and worker process, so
 * we must take care not to blindly shut down everything here
 */
void free_worker_memory(int flags)
{
	if (workers.wps) {
		unsigned int i;

		for (i = 0; i < workers.len; i++) {
			if (!workers.wps[i])
				continue;

			wproc_destroy(workers.wps[i], flags);
			workers.wps[i] = NULL;
		}

		my_free(workers.wps);
	}
	workers.len = 0;
	workers.idx = 0;

	to_remove = NULL;
	dkhash_walk_data(specialized_workers, remove_specialized);
	dkhash_destroy(specialized_workers);

	/* Don't leave pointers to freed memory. */
	specialized_workers = NULL;
}

static int str2timeval(char *str, struct timeval *tv)
{
	char *ptr  = NULL;
	char *ptr2 = NULL;

	tv->tv_sec = strtoul(str, &ptr, 10);

	if (ptr == str) {
		tv->tv_sec = tv->tv_usec = 0;
		return -1;
	}

	if (*ptr == '.' || *ptr == ',') {
		ptr2 = ptr + 1;
		tv->tv_usec = strtoul(ptr2, &ptr, 10);
	}

	return 0;
}

static int handle_worker_check(wproc_result *wpres, struct wproc_worker *wp, struct wproc_job *job)
{
	int result = ERROR;
	check_result *cr = (check_result *)job->arg;

	cr->start_time.tv_sec   = wpres->start.tv_sec;
	cr->start_time.tv_usec  = wpres->start.tv_usec;
	cr->finish_time.tv_sec  = wpres->stop.tv_sec;
	cr->finish_time.tv_usec = wpres->stop.tv_usec;

	if (WIFEXITED(wpres->wait_status)) {
		cr->return_code = WEXITSTATUS(wpres->wait_status);
	}
	else {
		cr->return_code = STATE_UNKNOWN;
	}

	if (wpres->outstd && *wpres->outstd) {
		cr->output = strdup(wpres->outstd);
	}
	else if (wpres->outerr) {
		asprintf(&cr->output, "(No output on stdout) stderr: %s", wpres->outerr);
	}
	else {
		cr->output = NULL;
	}

	cr->early_timeout = wpres->early_timeout;
	cr->exited_ok     = wpres->exited_ok;
	cr->engine        = NULL;
	cr->source        = wp->name;

	process_check_result(cr);
	free_check_result(cr);

	return result;
}

/*
 * parses a worker result. We do no strdup()'s here, so when
 * kvv is destroyed, all references to strings will become
 * invalid
 */
static int parse_worker_result(wproc_result *wpres, struct kvvec *kvv)
{
	int i;

	for (i = 0; i < kvv->kv_pairs; i++) {
		struct wpres_key *k;
		char *key, *value;
		key = kvv->kv[i].key;
		value = kvv->kv[i].value;

		k = wpres_get_key(key, kvv->kv[i].key_len);
		if (!k) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: Unrecognized result variable: (i=%d) %s=%s\n", i, key, value);
			continue;
		}
		switch (k->code) {
		case WPRES_job_id:
			wpres->job_id = atoi(value);
			break;
		case WPRES_type:
			wpres->type = atoi(value);
			break;
		case WPRES_command:
			wpres->command = value;
			break;
		case WPRES_timeout:
			wpres->timeout = atoi(value);
			break;
		case WPRES_wait_status:
			wpres->wait_status = atoi(value);
			break;
		case WPRES_start:
			str2timeval(value, &wpres->start);
			break;
		case WPRES_stop:
			str2timeval(value, &wpres->stop);
			break;
		case WPRES_outstd:
			wpres->outstd = value;
			break;
		case WPRES_outerr:
			wpres->outerr = value;
			break;
		case WPRES_exited_ok:
			wpres->exited_ok = atoi(value);
			break;
		case WPRES_error_msg:
			wpres->exited_ok = FALSE;
			wpres->error_msg = value;
			break;
		case WPRES_error_code:
			wpres->exited_ok = FALSE;
			wpres->error_code = atoi(value);
			break;
		case WPRES_runtime:
			/* ignored */
			break;

		default:
			logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: Recognized but unhandled result variable: %s=%s\n", key, value);
			break;
		}
	}
	return 0;
}

static int wproc_run_job(struct wproc_job *job, nagios_macros *mac);
static void fo_reassign_wproc_job(void *job_)
{
	struct wproc_job *job = (struct wproc_job *)job_;
	job->wp = get_worker(job->command);
	if (job->wp != NULL) {
		job->id = get_job_id(job->wp);
		/* macros aren't used right now anyways */
		wproc_run_job(job, NULL);
	} else {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: Error: can't get_worker() in fo_reassign_wproc_job\n");
	}
}

static int handle_worker_result(int sd, int events, void *arg)
{
	wproc_object_job *oj = NULL;
	char *buf, *error_reason = NULL;
	unsigned long size;
	int ret;
	static struct kvvec kvv = KVVEC_INITIALIZER;
	struct wproc_worker *wp = (struct wproc_worker *)arg;

	if((ret = iocache_capacity(wp->ioc)) < 0) {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: iocache_capacity() is %d for worker %s.\n", ret, wp->name);
	}

	ret = iocache_read(wp->ioc, wp->sd);

	if (ret < 0) {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: iocache_read() from %s returned %d: %s\n",
			  wp->name, ret, strerror(errno));
		return 0;
	} else if (ret == 0) {
		logit(NSLOG_INFO_MESSAGE, TRUE, "wproc: Socket to worker %s broken, removing", wp->name);
		wproc_num_workers_online--;
		iobroker_unregister(nagios_iobs, sd);
		if (workers.len <= 0) {
			/* there aren't global workers left, we can't run any more checks
			 * we should try respawning a few of the standard ones
			 */
			logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc: All our workers are dead, we can't do anything!");
		}
		remove_worker(wp);
		fanout_destroy(wp->jobs, fo_reassign_wproc_job);
		wp->jobs = NULL;
		wproc_destroy(wp, WPROC_FORCE);
		return 0;
	}
	while ((buf = worker_ioc2msg(wp->ioc, &size, 0))) {
		struct wproc_job *job;
		wproc_result wpres;

		/* log messages are handled first */
		if (size > 5 && !memcmp(buf, "log=", 4)) {
			logit(NSLOG_INFO_MESSAGE, TRUE, "wproc: %s: %s\n", wp->name, buf + 4);
			continue;
		}

		/* for everything else we need to actually parse */
		if (buf2kvvec_prealloc(&kvv, buf, size, '=', '\0', KVVEC_ASSIGN) <= 0) {
			logit(NSLOG_RUNTIME_ERROR, TRUE,
				  "wproc: Failed to parse key/value vector from worker response with len %lu. First kv=%s",
				  size, buf ? buf : "(NULL)");
			continue;
		}

		memset(&wpres, 0, sizeof(wpres));
		wpres.job_id = -1;
		wpres.type = -1;
		wpres.response = &kvv;
		parse_worker_result(&wpres, &kvv);

		job = get_job(wp, wpres.job_id);
		if (!job) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: Job with id '%d' doesn't exist on %s.\n",
				  wpres.job_id, wp->name);
			continue;
		}
		if (wpres.type != job->type) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: %s claims job %d is type %d, but we think it's type %d\n",
				  wp->name, job->id, wpres.type, job->type);
			break;
		}
		oj = (wproc_object_job *)job->arg;

		/*
		 * ETIME ("Timer expired") doesn't really happen
		 * on any modern systems, so we reuse it to mean
		 * "program timed out"
		 */
		if (wpres.error_code == ETIME) {
			wpres.early_timeout = TRUE;
		}
		if (wpres.early_timeout) {
			asprintf(&error_reason, "timed out after %.2fs", tv_delta_f(&wpres.start, &wpres.stop));
		}
		else if (WIFSIGNALED(wpres.wait_status)) {
			asprintf(&error_reason, "died by signal %d%s after %.2f seconds",
			         WTERMSIG(wpres.wait_status),
			         WCOREDUMP(wpres.wait_status) ? " (core dumped)" : "",
			         tv_delta_f(&wpres.start, &wpres.stop));
		}
		else if (job->type != WPJOB_CHECK && WEXITSTATUS(wpres.wait_status) != 0) {
			asprintf(&error_reason, "is a non-check helper but exited with return code %d",
			         WEXITSTATUS(wpres.wait_status));
		}
		if (error_reason) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc: %s job %d from worker %s %s",
			      wpjob_type_name(job->type), job->id, wp->name, error_reason);
#ifdef DEBUG
			/* The log below could leak sensitive information, such as 
				passwords, so only enable it if you really need it */
			logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc:   command: %s\n", job->command);
#endif
			if (job->type != WPJOB_CHECK && oj) {
				logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc:   host=%s; service=%s; contact=%s\n",
				      oj->host_name ? oj->host_name : "(none)",
				      oj->service_description ? oj->service_description : "(none)",
				      oj->contact_name ? oj->contact_name : "(none)");
			} else if (oj) {
				struct check_result *cr = (struct check_result *)job->arg;
				logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc:   host=%s; service=%s;\n",
				      cr->host_name, cr->service_description);
			}
			logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc:   early_timeout=%d; exited_ok=%d; wait_status=%d; error_code=%d;\n",
			      wpres.early_timeout, wpres.exited_ok, wpres.wait_status, wpres.error_code);
			wproc_logdump_buffer(NSLOG_RUNTIME_ERROR, TRUE, "wproc:   stderr", wpres.outerr);
			wproc_logdump_buffer(NSLOG_RUNTIME_ERROR, TRUE, "wproc:   stdout", wpres.outstd);
		}
		my_free(error_reason);

		switch (job->type) {
		case WPJOB_CHECK:
			ret = handle_worker_check(&wpres, wp, job);
			break;
		case WPJOB_NOTIFY:
			if (wpres.early_timeout) {
				if (oj->service_description) {
					logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Notifying contact '%s' of service '%s' on host '%s' by command '%s' timed out after %.2f seconds\n",
						  oj->contact_name, oj->service_description,
						  oj->host_name, job->command,
						  tv2float(&wpres.runtime));
				} else {
					logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Notifying contact '%s' of host '%s' by command '%s' timed out after %.2f seconds\n",
						  oj->contact_name, oj->host_name,
						  job->command, tv2float(&wpres.runtime));
				}
			}
			break;
		case WPJOB_OCSP:
			if (wpres.early_timeout) {
				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: OCSP command '%s' for service '%s' on host '%s' timed out after %.2f seconds\n",
					  job->command, oj->service_description, oj->host_name,
					  tv2float(&wpres.runtime));
			}
			break;
		case WPJOB_OCHP:
			if (wpres.early_timeout) {
				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: OCHP command '%s' for host '%s' timed out after %.2f seconds\n",
					  job->command, oj->host_name, tv2float(&wpres.runtime));
			}
			break;
		case WPJOB_GLOBAL_SVC_EVTHANDLER:
			if (wpres.early_timeout) {
				logit(NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING, TRUE,
					  "Warning: Global service event handler command '%s' timed out after %.2f seconds\n",
					  job->command, tv2float(&wpres.runtime));
			}
			break;
		case WPJOB_SVC_EVTHANDLER:
			if (wpres.early_timeout) {
				logit(NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING, TRUE,
					  "Warning: Service event handler command '%s' timed out after %.2f seconds\n",
					  job->command, tv2float(&wpres.runtime));
			}
			break;
		case WPJOB_GLOBAL_HOST_EVTHANDLER:
			if (wpres.early_timeout) {
				logit(NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING, TRUE,
					  "Warning: Global host event handler command '%s' timed out after %.2f seconds\n",
					  job->command, tv2float(&wpres.runtime));
			}
			break;
		case WPJOB_HOST_EVTHANDLER:
			if (wpres.early_timeout) {
				logit(NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING, TRUE,
					  "Warning: Host event handler command '%s' timed out after %.2f seconds\n",
					  job->command, tv2float(&wpres.runtime));
			}
			break;

		case WPJOB_CALLBACK:
			run_job_callback(job, &wpres, 0);
			break;

		case WPJOB_HOST_PERFDATA:
		case WPJOB_SVC_PERFDATA:
			/* these require nothing special */
			break;

		default:
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Worker %ld: Unknown jobtype: %d\n", (long)wp->pid, job->type);
			break;
		}
		destroy_job(job);
	}

	return 0;
}

int workers_alive(void)
{
	unsigned int i;
	int alive = 0;

	for (i = 0; i < workers.len; i++) {
		if (wproc_is_alive(workers.wps[i]))
			alive++;
	}

	return alive;
}

/* a service for registering workers */
static int register_worker(int sd, char *buf, unsigned int len)
{
	int i, is_global = 1;
	struct kvvec *info;
	struct wproc_worker *worker;

	logit(NSLOG_INFO_MESSAGE, TRUE, "wproc: Registry request: %s\n", buf);
	if (!(worker = calloc(1, sizeof(*worker)))) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc: Failed to allocate worker: %s\n", strerror(errno));
		return 500;
	}

	info = buf2kvvec(buf, len, '=', ';', 0);
	if (info == NULL) {
		free(worker);
		logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc: Failed to parse registration request\n");
		return 500;
	}

	worker->sd = sd;
	worker->ioc = iocache_create(1 * 1024 * 1024);

	iobroker_unregister(nagios_iobs, sd);
	iobroker_register(nagios_iobs, sd, worker, handle_worker_result);

	for(i = 0; i < info->kv_pairs; i++) {
		struct key_value *kv = &info->kv[i];
		if (!strcmp(kv->key, "name")) {
			worker->name = strdup(kv->value);
		}
		else if (!strcmp(kv->key, "pid")) {
			worker->pid = atoi(kv->value);
		}
		else if (!strcmp(kv->key, "max_jobs")) {
			worker->max_jobs = atoi(kv->value);
		}
		else if (!strcmp(kv->key, "plugin")) {
			struct wproc_list *command_handlers;
			is_global = 0;
			if (!(command_handlers = dkhash_get(specialized_workers, kv->value, NULL))) {
				command_handlers = calloc(1, sizeof(struct wproc_list));
				command_handlers->wps = calloc(1, sizeof(struct wproc_worker**));
				command_handlers->len = 1;
				command_handlers->wps[0] = worker;
				dkhash_insert(specialized_workers, strdup(kv->value), NULL, command_handlers);
			}
			else {
				command_handlers->len++;
				command_handlers->wps = realloc(command_handlers->wps, command_handlers->len * sizeof(struct wproc_worker**));
				command_handlers->wps[command_handlers->len - 1] = worker;
			}
			worker->wp_list = command_handlers;
		}
	}

	if (!worker->max_jobs) {
		/*
		 * each worker uses two filedescriptors per job, one to
		 * connect to the master and about 13 to handle libraries
		 * and memory allocation, so this guesstimate shouldn't
		 * be too far off (for local workers, at least).
		 */
		worker->max_jobs = (iobroker_max_usable_fds() / 2) - 50;
	}
	worker->jobs = fanout_create(worker->max_jobs);

	if (is_global) {
		workers.len++;
		workers.wps = realloc(workers.wps, workers.len * sizeof(struct wproc_worker *));
		workers.wps[workers.len - 1] = worker;
		worker->wp_list = &workers;
	}
	wproc_num_workers_online++;
	kvvec_destroy(info, 0);
	nsock_printf_nul(sd, "OK");

	/* signal query handler to release its iocache for this one */
	return QH_TAKEOVER;
}

static int wproc_query_handler(int sd, char *buf, unsigned int len)
{
	char *space, *rbuf = NULL;

	if (!*buf || !strcmp(buf, "help")) {
		nsock_printf_nul(sd, "Control worker processes.\n"
			"Valid commands:\n"
			"  wpstats              Print general job information\n"
			"  register <options>   Register a new worker\n"
			"                       <options> can be name, pid, max_jobs and/or plugin.\n"
			"                       There can be many plugin args.");
		return 0;
	}

	if ((space = memchr(buf, ' ', len)) != NULL)
		*space = 0;

	rbuf = space ? space + 1 : buf;
	len -= (unsigned long)rbuf - (unsigned long)buf;

	if (!strcmp(buf, "register"))
		return register_worker(sd, rbuf, len);
	if (!strcmp(buf, "wpstats")) {
		unsigned int i;

		for (i = 0; i < workers.len; i++) {
			struct wproc_worker *wp = workers.wps[i];
			nsock_printf(sd, "name=%s;pid=%ld;jobs_running=%u;jobs_started=%u\n",
					wp->name, (long)wp->pid,
					wp->jobs_running, wp->jobs_started);
		}
		return 0;
	}

	return 400;
}

static int spawn_core_worker(void)
{
	char *argvec[] = {nagios_binary_path, "--worker", qh_socket_path ? qh_socket_path : DEFAULT_QUERY_SOCKET, NULL};
	int ret;

	if ((ret = spawn_helper(argvec)) < 0)
		logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc: Failed to launch core worker: %s\n", strerror(errno));
	else
		wproc_num_workers_spawned++;

	return ret;
}

int get_desired_workers(int desired_workers) {

	if (desired_workers <= 0) {
		int cpus = online_cpus(); /* Always at least 1 CPU. */

		if (desired_workers < 0) {
			/* @note This is an undocumented case of questionable utility, and
			 * should be removed. Users reading this note have been warned. */
			desired_workers = cpus - desired_workers;
			/* desired_workers is now > 0. */
		} else {
			desired_workers = cpus * 1.5;

			if (desired_workers < 4) {
				/* Use at least 4 workers when autocalculating so we have some
				 * level of parallelism. */
				desired_workers = 4;
			} else if (desired_workers > 48) {
				/* Limit the autocalculated workers so we don't spawn too many
				 * on systems with many schedulable cores (>32). */
				desired_workers = 48;
			}
		}
	}

	return desired_workers;
}

/* if this function is updated, the function rlimit_problem_detection()
   must be updated as well */
int init_workers(int desired_workers)
{
	specialized_workers = dkhash_create(512);
	if (!specialized_workers) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc: Failed to allocate specialized worker table.\n");
		return -1;
	}

	/* Register our query handler before launching workers, so other workers
	 * can join us whenever they're ready. */
	if (!qh_register_handler("wproc", "Worker process management and info", 0, wproc_query_handler))
		logit(NSLOG_INFO_MESSAGE, TRUE, "wproc: Successfully registered manager as @wproc with query handler\n");
	else
		logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc: Failed to register manager with query handler\n");

	desired_workers = get_desired_workers(desired_workers);
	wproc_num_workers_desired = desired_workers;

	if (workers_alive() == desired_workers)
		return 0;

	/* can't shrink the number of workers (yet) */
	if (desired_workers < (int)workers.len)
		return -1;

	while (desired_workers-- > 0) {
		int worker_pid = spawn_core_worker();
		log_debug_info(DEBUGL_WORKERS, 2, "Spawned new worker with pid: (%d)\n", worker_pid);
	}

	return 0;
}

/*
 * Handles adding the command and macros to the kvvec,
 * as well as shipping the command off to a designated
 * worker
 */
static int wproc_run_job(struct wproc_job *job, nagios_macros *mac)
{
	static struct kvvec kvv    = KVVEC_INITIALIZER;
	struct kvvec_buf *kvvb     = NULL;
	struct kvvec *env_kvvp     = NULL;
	struct kvvec_buf *env_kvvb = NULL;
	struct wproc_worker *wp    = NULL;
	int ret                    = OK;
	int result                 = OK;
	ssize_t written            = 0;

	log_debug_info(DEBUGL_WORKERS, 1, "wproc_run_job()\n");

	if (job == NULL) {
		log_debug_info(DEBUGL_WORKERS, 1, " * failed because job was null\n");
		return ERROR;
	}

	if (job->wp == NULL) {
		log_debug_info(DEBUGL_WORKERS, 1, " * failed because job worker process was null\n");
		return ERROR;
	}

	wp = job->wp;

	/* job_id, type, command and timeout */
	if (!kvvec_init(&kvv, 4)) {
		return ERROR;
	}

	kvvec_addkv(&kvv, "job_id", (char *)mkstr("%d", job->id));
	kvvec_addkv(&kvv, "type", (char *)mkstr("%d", job->type));
	kvvec_addkv(&kvv, "command", job->command);
	kvvec_addkv(&kvv, "timeout", (char *)mkstr("%u", job->timeout));

	/* Add the macro environment variables */
	if (mac != NULL) {

		env_kvvp = macros_to_kvv(mac);

		if (env_kvvp != NULL) {

			env_kvvb = kvvec2buf(env_kvvp, '=', '\n', 0);

			if (env_kvvb == NULL) {
				kvvec_destroy(env_kvvp, KVVEC_FREE_KEYS);
			}
			else {
				/* no reason to call strlen("env") 
				  when we know it's 3 characters */
				kvvec_addkv_wlen(&kvv, "env", 3, env_kvvb->buf, env_kvvb->buflen);
			}
		}
	}

	kvvb = build_kvvec_buf(&kvv);

	/* ret = write(wp->sd, kvvb->buf, kvvb->bufsize); */
	ret = nwrite(wp->sd, kvvb->buf, kvvb->bufsize, &written);

	if (ret != (int) kvvb->bufsize) {

		logit(NSLOG_RUNTIME_ERROR, TRUE, 
			"wproc: '%s' seems to be choked. ret = %d; bufsize = %lu: written = %lu; errno = %d (%s)\n",
			wp->name, ret, kvvb->bufsize, (long unsigned int) written, errno, strerror(errno));
		destroy_job(job);
		result = ERROR;

	} else {
		wp->jobs_running++;
		wp->jobs_started++;
		loadctl.jobs_running++;
	}

	if (env_kvvp != NULL) {
		kvvec_destroy(env_kvvp, KVVEC_FREE_KEYS);
	}

	if (env_kvvb != NULL) {
		free(env_kvvb->buf);
		free(env_kvvb);
	}

	free(kvvb->buf);
	free(kvvb);

	return result;
}

static wproc_object_job *create_object_job(char *cname, char *hname, char *sdesc)
{
	wproc_object_job *oj;

	oj = calloc(1, sizeof(*oj));
	if (oj) {
		oj->host_name = hname;
		if (cname)
			oj->contact_name = cname;
		if (sdesc)
			oj->service_description = sdesc;
	}

	return oj;
}

int wproc_notify(char *cname, char *hname, char *sdesc, char *cmd, nagios_macros *mac)
{
	struct wproc_job *job;
	wproc_object_job *oj;

	if (!(oj = create_object_job(cname, hname, sdesc)))
		return ERROR;

	job = create_job(WPJOB_NOTIFY, oj, notification_timeout, cmd);

	return wproc_run_job(job, mac);
}

int wproc_run_service_job(int jtype, int timeout, service *svc, char *cmd, nagios_macros *mac)
{
	struct wproc_job *job;
	wproc_object_job *oj;

	if (!(oj = create_object_job(NULL, svc->host_name, svc->description)))
		return ERROR;

	job = create_job(jtype, oj, timeout, cmd);

	return wproc_run_job(job, mac);
}

int wproc_run_host_job(int jtype, int timeout, host *hst, char *cmd, nagios_macros *mac)
{
	struct wproc_job *job;
	wproc_object_job *oj;

	if (!(oj = create_object_job(NULL, hst->name, NULL)))
		return ERROR;

	job = create_job(jtype, oj, timeout, cmd);

	return wproc_run_job(job, mac);
}

int wproc_run_check(check_result *cr, char *cmd, nagios_macros *mac)
{
	struct wproc_job *job;
	int timeout;

	if (cr->service_description)
		timeout = service_check_timeout;
	else
		timeout = host_check_timeout;

	job = create_job(WPJOB_CHECK, cr, timeout, cmd);
	return wproc_run_job(job, mac);
}

int wproc_run(int jtype, char *cmd, int timeout, nagios_macros *mac)
{
	struct wproc_job *job;

	job = create_job(jtype, NULL, timeout, cmd);
	return wproc_run_job(job, mac);
}

int wproc_run_callback(char *cmd, int timeout,
		void (*cb)(struct wproc_result *, void *, int), void *data,
		nagios_macros *mac)
{
	struct wproc_job *job;
	struct wproc_callback_job *cj;
	if (!(cj = calloc(1, sizeof(*cj))))
		return ERROR;
	cj->callback = cb;
	cj->data = data;

	job = create_job(WPJOB_CALLBACK, cj, timeout, cmd);
	return wproc_run_job(job, mac);
}
