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

extern int service_check_timeout, host_check_timeout;
extern int notification_timeout;

iobroker_set *nagios_iobs = NULL;
static worker_process **workers;
static unsigned int num_workers;
static unsigned int worker_index;

typedef struct wproc_object_job {
	char *contact_name;
	char *host_name;
	char *service_description;
	struct squeue_event *sq_evt;
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

#define tv2float(tv) ((float)((tv)->tv_sec) + ((float)(tv)->tv_usec) / 1000000.0)

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
	wproc_object_job *oj;

	if (!job)
		return;

	oj = (wproc_object_job *)job->arg;

	switch (job->type) {
	case WPJOB_CHECK:
		free_check_result(job->arg);
		free(job->arg);
		break;
	case WPJOB_NOTIFY:
		free(oj->contact_name);
		/* fallthrough */
	case WPJOB_OCSP:
	case WPJOB_OCHP:
		free(oj->host_name);
		if (oj->service_description) {
			free(oj->service_description);
		}
		free(job->arg);
		break;

	case WPJOB_GLOBAL_SVC_EVTHANDLER:
	case WPJOB_SVC_EVTHANDLER:
	case WPJOB_GLOBAL_HOST_EVTHANDLER:
	case WPJOB_HOST_EVTHANDLER:
		/* these require nothing special */
		break;
	default:
		logit(NSLOG_RUNTIME_WARNING, TRUE, "Workers: Unknown job type: %d\n", job->type);
		break;
	}

	my_free(job->command);

	wp->jobs[job->id % wp->max_jobs] = NULL;
	free(job);
}

static void free_wproc_memory(worker_process *wp)
{
	int i = 0, destroyed = 0;

	if (!wp)
		return;

	iocache_destroy(wp->ioc);
	wp->ioc = NULL;

	for (i = 0; i < wp->max_jobs; i++) {
		if (!wp->jobs[i])
			continue;

		destroy_job(wp, wp->jobs[i]);
		/* we can (often) break out early */
		if (++destroyed >= wp->jobs_running)
			break;
	}

	free(wp->jobs);
}

/*
 * This gets called from both parent and worker process, so
 * we must take care not to blindly shut down everything here
 */
void free_worker_memory(void)
{
	unsigned int i;

	for (i = 0; i < num_workers; i++) {
		if (!workers[i])
			continue;

		/* workers die when master socket close()s */
		iobroker_close(nagios_iobs, workers[i]->sd);
		free_wproc_memory(workers[i]);
		my_free(workers[i]);
	}
	iobroker_destroy(nagios_iobs, 0);
	nagios_iobs = NULL;
	workers = NULL;
	num_workers = 0;
	worker_index = 0;
}

/*
 * function workers call as soon as they come alive
 */
static void worker_init_func(void *arg)
{
	/*
	 * we pass 'arg' here to safeguard against
	 * changes in it since the worker spawned
	 */
	free_memory((nagios_macros *)arg);
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

	cr->output_file = NULL;
	cr->output_file_fp = NULL;
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

	if (wpres->outstd) {
		cr->output = strdup(wpres->outstd);
	} else if (wpres->outerr) {
		asprintf(&cr->output, "(No output on stdout) stderr: %s", wpres->outerr);
	} else {
		cr->output = NULL;
	}

	cr->early_timeout = wpres->early_timeout;
	cr->exited_ok = wpres->exited_ok;

	if (cr->service_description) {
		service *svc = find_service(cr->host_name, cr->service_description);
		if (svc)
			result = handle_async_service_check_result(svc, cr);
		else
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Worker: Failed to locate service '%s' on host '%s'. Result from job %d (%s) will be dropped.\n",
				  cr->service_description, cr->host_name, job->id, job->command);
	} else {
		host *hst = find_host(cr->host_name);
		if (hst)
			result = handle_async_host_check_result_3x(hst, cr);
		else
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Worker: Failed to locate host '%s'. Result from job %d (%s) will be dropped.\n",
				  cr->host_name, job->id, job->command);
	}
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
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Unrecognized worker result variable: (i=%d) %s=%s\n", i, key, value);
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
		case WPRES_ru_nswap:
			wpres->rusage.ru_nswap = atoi(value);
			break;
		case WPRES_ru_inblock:
			wpres->rusage.ru_inblock = atoi(value);
			break;
		case WPRES_ru_oublock:
			wpres->rusage.ru_oublock = atoi(value);
			break;
		case WPRES_ru_nsignals:
			wpres->rusage.ru_nsignals = atoi(value);
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

		default:
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Recognized but unhandled worker result variable: %s=%s\n", key, value);
			break;
		}
	}
	return 0;
}

static int handle_worker_result(int sd, int events, void *arg)
{
	worker_process *wp = (worker_process *)arg;
	wproc_object_job *oj;
	char *buf;
	unsigned long size;
	int ret;

	ret = iocache_read(wp->ioc, wp->sd);
	if (ret < 0) {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "iocache_read() from worker %d returned %d: %s\n",
			  wp->pid, ret, strerror(errno));
		return 0;
	} else if (ret == 0) {
		/*
		 * XXX FIXME worker exited. spawn a new on to replace it
		 * and distribute all unfinished jobs from this one to others
		 */
		return 0;
	}

	while ((buf = iocache_use_delim(wp->ioc, MSG_DELIM, MSG_DELIM_LEN, &size))) {
		struct kvvec *kvv;
		int job_id = -1;
		char *key, *value;
		worker_job *job;
		wproc_result wpres;

		kvv = buf2kvvec(buf, size, '=', '\0');
		if (!kvv) {
			/* XXX FIXME log an error */
			continue;
		}

		key = kvv->kv[0].key;
		value = kvv->kv[0].value;

		/* log messages are handled first */
		if (kvv->kv_pairs == 1 && !strcmp(key, "log")) {
			logit(NSLOG_INFO_MESSAGE, TRUE, "worker %d: %s\n", wp->pid, value);
			kvvec_destroy(kvv, KVVEC_FREE_ALL);
			continue;
		}

		memset(&wpres, 0, sizeof(wpres));
		wpres.job_id = -1;
		wpres.type = -1;
		wpres.response = kvv;
		parse_worker_result(&wpres, kvv);

		job = get_job(wp, wpres.job_id);
		if (!job) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Worker job with id '%d' doesn't exist on worker %d.\n",
				  job_id, wp->pid);
			kvvec_destroy(kvv, KVVEC_FREE_ALL);
			continue;
		}
		if (wpres.type != job->type) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Worker %d claims job %d is type %d, but we think it's type %d\n",
				  wp->pid, job->id, wpres.type, job->type);
			kvvec_destroy(kvv, KVVEC_FREE_ALL);
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
		kvvec_destroy(kvv, KVVEC_FREE_ALL);
		destroy_job(wp, job);
		wp->jobs_running--;
	}

	return 0;
}

static int init_iobroker(void)
{
	if (!nagios_iobs)
		nagios_iobs = iobroker_create();

	if (nagios_iobs)
		return 0;
	return -1;
}

int init_workers(int desired_workers)
{
	worker_process **wps;
	int i;

	if (desired_workers <= 0) {
		desired_workers = 4;
	}

	init_iobroker();

	/* can't shrink the number of workers (yet) */
	if (desired_workers < num_workers)
		return -1;

	wps = calloc(desired_workers, sizeof(worker_process *));
	if (!wps)
		return -1;

	if (workers) {
		if (num_workers < desired_workers) {
			for (i = 0; i < num_workers; i++) {
				wps[i] = workers[i];
			}
		}

		free(workers);
	}

	workers = wps;
	for (; num_workers < desired_workers; num_workers++) {
		worker_process *wp;

		wp = spawn_worker(worker_init_func, (void *)get_global_macros());
		if (!wp) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Failed to spawn worker: %s\n", strerror(errno));
			free_worker_memory();
			return ERROR;
		}

		wps[num_workers] = wp;
		iobroker_register(nagios_iobs, wp->sd, wp, handle_worker_result);
	}

	return 0;
}

static worker_process *get_worker(worker_job *job)
{
	worker_process *wp = NULL;
	int i;

	if (!workers) {
		return NULL;
	}

	wp = workers[worker_index++ % num_workers];
	job->id = get_job_id(wp);

	if (job->id < 0) {
		/* XXX FIXME Fiddle with finding a new, less busy, worker here */
	}
	wp->jobs[job->id % wp->max_jobs] = job;
	return wp;

	/* dead code below. for now */
	for (i = 0; i < num_workers; i++) {
		wp = workers[worker_index++ % num_workers];
		if (wp) {
			/*
			 * XXX: check worker flags so we don't ship checks to a
			 * worker that's about to reincarnate.
			 */
			return wp;
		}

		worker_index++;
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
	struct kvvec *kvv;
	worker_process *wp;

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
	kvv = kvvec_init(4); /* job_id, type, command and timeout */
	if (!kvv)
		return ERROR;

	kvvec_addkv(kvv, "job_id", (char *)mkstr("%d", job->id));
	kvvec_addkv(kvv, "type", (char *)mkstr("%d", job->type));
	kvvec_addkv(kvv, "command", job->command);
	kvvec_addkv(kvv, "timeout", (char *)mkstr("%u", job->timeout));
	send_kvvec(wp->sd, kvv);
	kvvec_destroy(kvv, 0);
	wp->jobs_running++;
	wp->jobs_started++;

	return 0;
}

static wproc_object_job *create_object_job(char *cname, char *hname, char *sdesc)
{
	wproc_object_job *oj;

	oj = calloc(1, sizeof(*oj));
	if (oj) {
		oj->host_name = strdup(hname);
		if (cname)
			oj->contact_name = strdup(cname);
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
