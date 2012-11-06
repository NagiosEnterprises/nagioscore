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
#include "wp-phash.c"

struct wproc_list {
	int len;
	unsigned int idx;
	worker_process **wps;
};

static struct wproc_list workers = {0, 0, NULL};

static dkhash_table *specialized_workers;

typedef struct wproc_object_job {
	char *contact_name;
	char *host_name;
	char *service_description;
} wproc_object_job;

typedef struct wproc_result {
	int job_id;
	int type;
	time_t timeout;
	struct timeval start;
	struct timeval stop;
	struct timeval runtime;
	int wait_status;
	char *command;
	char *outstd;
	char *outerr;
	char *error_msg;
	int error_code;
	int exited_ok;
	int errcode;
	int early_timeout;
	struct rusage rusage;
	struct kvvec *response;
} wproc_result;

unsigned int wproc_num_workers_online = 0, wproc_num_workers_desired = 0;
unsigned int wproc_num_workers_spawned = 0;

#define tv2float(tv) ((float)((tv)->tv_sec) + ((float)(tv)->tv_usec) / 1000000.0)

/* reap 'jobs' jobs or 'secs' seconds, whichever comes first */
void wproc_reap(int jobs, int msecs)
{
	time_t start, now;
	start = time(NULL);

	/* one input equals one job (or close enough to it anyway) */
	do {
		int inputs;

		now = time(NULL);
		inputs = iobroker_poll(nagios_iobs, (now - start) * 1000);
		jobs -= inputs;
	} while (jobs > 0 && start + (msecs * 1000) <= now);
}


int wproc_can_spawn(struct load_control *lc)
{
	unsigned int old = 0;
	time_t now;

	/* if no load control is enabled, we can safely run this job */
	if (!(lc->options & LOADCTL_ENABLED))
		return 1;

	now = time(NULL);
	if (lc->last_check + lc->check_interval > now) {
		lc->last_check = now;

		if (getloadavg(lc->load, 3) < 0)
			return lc->jobs_limit < lc->jobs_running;

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

	return lc->jobs_limit < lc->jobs_running;
}

static worker_job *create_job(int type, void *arg, time_t timeout, const char *command)
{
	worker_job *job;

	job = calloc(1, sizeof(*job));
	if (!job)
		return NULL;

	job->type = type;
	job->arg = arg;
	job->timeout = timeout;
	job->command = strdup(command);

	return job;
}

static int get_job_id(worker_process *wp)
{
	int i;

	/* if there can't be any jobs, we break out early */
	if (wp->jobs_running == wp->max_jobs)
		return -1;

	/*
	 * Locate a free job_id by checking oldest slots first.
	 * This should result in us getting a slot fairly quickly
	 */
	for (i = wp->job_index; i < wp->job_index + wp->max_jobs; i++) {
		if (!wp->jobs[i % wp->max_jobs]) {
			wp->job_index = i % wp->max_jobs;
			return wp->job_index;
		}
	}

	return -1;
}

static worker_job *get_job(worker_process *wp, int job_id)
{
	/*
	 * XXX FIXME check job->id against job_id and do something if
	 * they don't match
	 */
	return wp->jobs[job_id % wp->max_jobs];
}

static void destroy_job(worker_process *wp, worker_job *job)
{
	if (!job)
		return;

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
		/* these require nothing special */
		break;
	default:
		logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: Unknown job type: %d\n", job->type);
		break;
	}

	my_free(job->command);

	wp->jobs[job->id % wp->max_jobs] = NULL;
	wp->jobs_running--;
	loadctl.jobs_running--;

	free(job);
}

static int wproc_is_alive(worker_process *wp)
{
	if (!wp || !wp->pid)
		return 0;
	if (kill(wp->pid, 0) == 0 && iobroker_is_registered(nagios_iobs, wp->sd))
		return 1;
	return 0;
}

int wproc_destroy(worker_process *wp, int flags)
{
	int i = 0, force = 0, self;

	if (!wp)
		return 0;

	force = !!(flags & WPROC_FORCE);

	self = getpid();

	/* master retains workers through restarts */
	if (self == nagios_pid && !force)
		return 0;

	/* free all memory when either forcing or a worker called us */
	iocache_destroy(wp->ioc);
	wp->ioc = NULL;
	my_free(wp->source_name);
	if (wp->jobs) {
		for (i = 0; i < wp->max_jobs && wp->jobs_running; i++) {
			if (!wp->jobs[i])
				continue;

			destroy_job(wp, wp->jobs[i]);
		}

		free(wp->jobs);
		wp->jobs = NULL;
	}

	/* workers must never control other workers, so they return early */
	if (self != nagios_pid)
		return 0;

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

static worker_process *to_remove = NULL;
/* remove the worker pointed to by to_remove
 * if to_remove is null, remove everything */
static int remove_specialized(void *data)
{
	int i;
	struct wproc_list *list = (struct wproc_list *)data;
	for (i = 0; i < list->len; i++) {
		if (to_remove != NULL && list->wps[i] != to_remove)
			continue;

		list->len--;
		if (list->len <= 0) {
			free(list->wps);
			list->wps = NULL;
			if(list != &workers)
				free(list);
			return DKHASH_WALK_REMOVE;
		}
		else {
			list->wps[i] = list->wps[list->len];
			i--;
		}
	}
	return 0;
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

		free(workers.wps);
	}
	to_remove = NULL;
	dkhash_walk_data(specialized_workers, remove_specialized);
	dkhash_destroy(specialized_workers);
	workers.wps = NULL;
	workers.len = 0;
	workers.idx = 0;
}

static int str2timeval(char *str, struct timeval *tv)
{
	char *ptr, *ptr2;

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

static int handle_worker_check(wproc_result *wpres, worker_process *wp, worker_job *job)
{
	int result = ERROR;
	check_result *cr = (check_result *)job->arg;

	memcpy(&cr->rusage, &wpres->rusage, sizeof(wpres->rusage));
	cr->start_time.tv_sec = wpres->start.tv_sec;
	cr->start_time.tv_usec = wpres->start.tv_usec;
	cr->finish_time.tv_sec = wpres->stop.tv_sec;
	cr->finish_time.tv_usec = wpres->stop.tv_usec;
	if (WIFEXITED(wpres->wait_status)) {
		cr->return_code = WEXITSTATUS(wpres->wait_status);
	} else {
		cr->return_code = STATE_UNKNOWN;
	}

	if (wpres->outstd && *wpres->outstd) {
		cr->output = strdup(wpres->outstd);
	} else if (wpres->outerr) {
		asprintf(&cr->output, "(No output on stdout) stderr: %s", wpres->outerr);
	} else {
		cr->output = NULL;
	}

	cr->early_timeout = wpres->early_timeout;
	cr->exited_ok = wpres->exited_ok;
	cr->engine = &nagios_check_engine;
	cr->source = wp;

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
		char *key, *value;
		int code;
		key = kvv->kv[i].key;
		value = kvv->kv[i].value;

		code = wp_phash(key, kvv->kv[i].key_len);
		switch (code) {
		case -1:
			logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: Unrecognized result variable: (i=%d) %s=%s\n", i, key, value);
			break;

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
		case WPRES_runtime:
			/* ignored */
			break;
		case WPRES_ru_utime:
			str2timeval(value, &wpres->rusage.ru_utime);
			break;
		case WPRES_ru_stime:
			str2timeval(value, &wpres->rusage.ru_stime);
			break;
		case WPRES_ru_minflt:
			wpres->rusage.ru_minflt = atoi(value);
			break;
		case WPRES_ru_majflt:
			wpres->rusage.ru_majflt = atoi(value);
			break;
		case WPRES_ru_inblock:
			wpres->rusage.ru_inblock = atoi(value);
			break;
		case WPRES_ru_oublock:
			wpres->rusage.ru_oublock = atoi(value);
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
		case WPRES_ru_nsignals:
		case WPRES_ru_nswap:
			break;

		default:
			logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: Recognized but unhandled result variable: %s=%s\n", key, value);
			break;
		}
	}
	return 0;
}

static int handle_worker_result(int sd, int events, void *arg)
{
	wproc_object_job *oj;
	char *buf;
	unsigned long size;
	int ret;
	static struct kvvec kvv = KVVEC_INITIALIZER;
	worker_process *wp = (worker_process *)arg;

	if(iocache_capacity(wp->ioc) == 0) {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: iocache_capacity() is 0 for worker %s.\n", wp->source_name);
	}

	ret = iocache_read(wp->ioc, wp->sd);

	if (ret < 0) {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: iocache_read() from %s returned %d: %s\n",
			  wp->source_name, ret, strerror(errno));
		return 0;
	} else if (ret == 0) {
		logit(NSLOG_INFO_MESSAGE, TRUE, "wproc: Socket to worker %s broken, removing", wp->source_name);
		wproc_num_workers_online--;
		iobroker_unregister(nagios_iobs, sd);
		to_remove = wp;
		dkhash_walk_data(specialized_workers, remove_specialized);
		if (remove_specialized((void *)&workers) == DKHASH_WALK_REMOVE) {
			/* there aren't global workers left, we can't run any more checks
			 * we should try respawning a few of the standard ones
			 */
			logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc: All our workers are dead, we can't do anything!");
		}
		if (wp->jobs) {
			int i, rescheduled = 0;
			for (i = 0; i < wp->max_jobs; i++) {
				if (!wp->jobs[i])
					continue;

				create_job(wp->jobs[i]->type, wp->jobs[i]->arg, wp->jobs[i]->timeout, wp->jobs[i]->command);

				if (++rescheduled >= wp->jobs_running)
					break;
			}
		}

		wproc_destroy(wp, 0);
		return 0;
	}
	while ((buf = iocache_use_delim(wp->ioc, MSG_DELIM, MSG_DELIM_LEN, &size))) {
		int job_id = -1;
		worker_job *job;
		wproc_result wpres;

		/* log messages are handled first */
		if (size > 5 && !memcmp(buf, "log=", 4)) {
			logit(NSLOG_INFO_MESSAGE, TRUE, "wproc: %s: %s\n", wp->source_name, buf + 4);
			continue;
		}

		/* for everything else we need to actually parse */
		if (buf2kvvec_prealloc(&kvv, buf, size, '=', '\0', KVVEC_ASSIGN) <= 0) {
			/* XXX FIXME log an error */
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
				  job_id, wp->source_name);
			continue;
		}
		if (wpres.type != job->type) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "wproc: %s claims job %d is type %d, but we think it's type %d\n",
				  wp->source_name, job->id, wpres.type, job->type);
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

		default:
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Worker %d: Unknown jobtype: %d\n", wp->pid, job->type);
			break;
		}
		destroy_job(wp, job);
	}

	return 0;
}

int workers_alive(void)
{
	int i, alive = 0;

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
	worker_process *worker;

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
			worker->source_name = strdup(kv->value);
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
				command_handlers->wps = calloc(1, sizeof(worker_process**));
				command_handlers->len = 1;
				command_handlers->wps[0] = worker;
				dkhash_insert(specialized_workers, strdup(kv->value), NULL, command_handlers);
			}
			else {
				command_handlers->len++;
				command_handlers->wps = realloc(command_handlers->wps, command_handlers->len * sizeof(worker_process**));
				command_handlers->wps[command_handlers->len - 1] = worker;
			}
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
	worker->jobs = calloc(worker->max_jobs, sizeof(worker_job *));

	if (is_global) {
		workers.len++;
		workers.wps = realloc(workers.wps, workers.len * sizeof(worker_process *));
		workers.wps[workers.len - 1] = worker;
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

	if ((space = memchr(buf, ' ', len)) != NULL)
		*space = 0;

	rbuf = space ? space + 1 : buf;
	len -= (unsigned long)rbuf - (unsigned long)buf;

	if (!strcmp(buf, "register"))
		return register_worker(sd, rbuf, len);
	if (!strcmp(buf, "wpstats")) {
		int i;

		for (i = 0; i < workers.len; i++) {
			worker_process *wp = workers.wps[i];
			nsock_printf(sd, "name=%s;pid=%d;jobs_running=%u;jobs_started=%u\n",
						 wp->source_name, wp->pid,
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


int init_workers(int desired_workers)
{
	int i;

	/*
	 * we register our query handler before launching workers,
	 * so other workers can join us whenever they're ready
	 */
	specialized_workers = dkhash_create(512);
	if(!qh_register_handler("wproc", 0, wproc_query_handler))
		logit(NSLOG_INFO_MESSAGE, TRUE, "wproc: Successfully registered manager as @wproc with query handler\n");
	else
		logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc: Failed to register manager with query handler\n");

	i = desired_workers;
	if (desired_workers <= 0) {
		int cpus = online_cpus();

		if(!desired_workers) {
			desired_workers = cpus * 1.5;
			/* min 4 workers, as it's tested and known to work */
			if(desired_workers < 4)
				desired_workers = 4;
		}
		else {
			/* desired workers is a negative number */
			desired_workers = cpus - desired_workers;
		}
	}
	wproc_num_workers_desired = desired_workers;

	if (workers_alive() == desired_workers)
		return 0;

	/* can't shrink the number of workers (yet) */
	if (desired_workers < workers.len)
		return -1;

	for (i = 0; i < desired_workers; i++)
		spawn_core_worker();

	return 0;
}

static worker_process *get_worker(worker_job *job)
{
	worker_process *wp = NULL;
	struct wproc_list *wp_list;
	int i;
	char *cmd_name, *space, *slash = NULL;

	/* first, look for a specialized worker for this command */
	cmd_name = job->command;
	if ((space = strchr(cmd_name, ' ')) != NULL) {
		*space = '\0';
		slash = strrchr(cmd_name, '/');
	}

	wp_list = dkhash_get(specialized_workers, cmd_name, NULL);
	if (!wp_list && slash) {
		wp_list = dkhash_get(specialized_workers, ++slash, NULL);
	}
	if (wp_list != NULL) {
		log_debug_info(DEBUGL_CHECKS, 1, "Found specialized worker(s) for '%s'", (slash && *slash != '/') ? slash : cmd_name);
	}
	else {
		if (!workers.wps)
			return NULL;
		wp_list = &workers;
	}
	if (space != NULL)
		*space = ' ';


	wp = wp_list->wps[wp_list->idx++ % wp_list->len];

	job->id = get_job_id(wp);

	if (job->id < 0) {
		/* XXX FIXME Fiddle with finding a new, less busy, worker here */
		return NULL;
	}
	wp->jobs[job->id] = job;
	job->wp = wp;
	return wp;

	/* dead code below. for now */
	for (i = 0; i < workers.len; i++) {
		wp = workers.wps[workers.idx++ % workers.len];
		if (wp) {
			/*
			 * XXX: check worker flags so we don't ship checks to a
			 * worker that's about to reincarnate.
			 */
			return wp;
		}

		workers.idx++;
		if (wp)
			return wp;
	}

	return NULL;
}

/*
 * Handles adding the command and macros to the kvvec,
 * as well as shipping the command off to a designated
 * worker
 */
static int wproc_run_job(worker_job *job, nagios_macros *mac)
{
	static struct kvvec kvv = KVVEC_INITIALIZER;
	struct kvvec_buf *kvvb;
	worker_process *wp;
	int ret;

	/*
	 * get_worker() also adds job to the workers list
	 * and sets job_id
	 */
	wp = get_worker(job);
	if (!wp || job->id < 0)
		return ERROR;

	/*
	 * XXX FIXME: add environment macros as
	 *  kvvec_addkv(kvv, "env", "NAGIOS_LALAMACRO=VALUE");
	 *  kvvec_addkv(kvv, "env", "NAGIOS_LALAMACRO2=VALUE");
	 * so workers know to add them to environment. For now,
	 * we don't support that though.
	 */
	if (!kvvec_init(&kvv, 4))	/* job_id, type, command and timeout */
		return ERROR;

	kvvec_addkv(&kvv, "job_id", (char *)mkstr("%d", job->id));
	kvvec_addkv(&kvv, "type", (char *)mkstr("%d", job->type));
	kvvec_addkv(&kvv, "command", job->command);
	kvvec_addkv(&kvv, "timeout", (char *)mkstr("%u", job->timeout));
	kvvb = build_kvvec_buf(&kvv);
	ret = write(wp->sd, kvvb->buf, kvvb->bufsize);
	wp->jobs_running++;
	wp->jobs_started++;
	loadctl.jobs_running++;
	if (ret != kvvb->bufsize) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "wproc: '%s' seems to be choked. ret = %d; bufsize = %lu: errno = %d (%s)\n",
			  wp->source_name, ret, kvvb->bufsize, errno, strerror(errno));
		destroy_job(wp, job);
	}
	free(kvvb->buf);
	free(kvvb);

	return ret;
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
	worker_job *job;
	wproc_object_job *oj;

	oj = create_object_job(cname, hname, sdesc);
	job = create_job(WPJOB_NOTIFY, oj, notification_timeout, cmd);

	return wproc_run_job(job, mac);
}

int wproc_run_service_job(int jtype, int timeout, service *svc, char *cmd, nagios_macros *mac)
{
	worker_job *job;
	wproc_object_job *oj;

	oj = create_object_job(NULL, svc->host_name, svc->description);
	job = create_job(jtype, oj, timeout, cmd);

	return wproc_run_job(job, mac);
}

int wproc_run_host_job(int jtype, int timeout, host *hst, char *cmd, nagios_macros *mac)
{
	worker_job *job;
	wproc_object_job *oj;

	oj = create_object_job(NULL, hst->name, NULL);
	job = create_job(jtype, oj, timeout, cmd);

	return wproc_run_job(job, mac);
}

int wproc_run_check(check_result *cr, char *cmd, nagios_macros *mac)
{
	worker_job *job;
	time_t timeout;

	if (cr->service_description)
		timeout = service_check_timeout;
	else
		timeout = host_check_timeout;

	job = create_job(WPJOB_CHECK, cr, timeout, cmd);
	return wproc_run_job(job, mac);
}

int wproc_run(int jtype, char *cmd, int timeout, nagios_macros *mac)
{
	worker_job *job;
	time_t real_timeout = timeout + time(NULL);

	job = create_job(jtype, NULL, real_timeout, cmd);
	return wproc_run_job(job, mac);
}
