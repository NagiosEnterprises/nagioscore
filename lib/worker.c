#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include "libnagios.h"

#define MSG_DELIM "\1\0\0" /**< message limiter - note this ends up being
			     \1\0\0\0 on the wire as "" strings null-terminate */
#define MSG_DELIM_LEN (sizeof(MSG_DELIM)) /**< message delimiter length - 4, not 3 */
#define PAIR_SEP 0 /**< pair separator for buf2kvvec() and kvvec2buf() */
#define KV_SEP '=' /**< key/value separator for buf2kvvec() and kvvec2buf() */

struct execution_information {
	squeue_event *sq_event;
	pid_t pid;
	int state;
	struct timeval start;
	struct timeval stop;
	float runtime;
	struct rusage rusage;
};

static iobroker_set *iobs;
static squeue_t *sq;
static unsigned int started, running_jobs, timeouts, reapable;
static int master_sd;
static int parent_pid;
static fanout_table *ptab;

static void exit_worker(int code, const char *msg)
{
	child_process *cp;
	int discard;
#ifdef HAVE_SIGACTION
	struct sigaction sig_action;
#endif

	if (msg) {
		perror(msg);
	}

	/*
	 * We must kill our children, so let's embark on that
	 * large scale filicide. Each process should be in a
	 * process group of its own, so we can signal not only
	 * the plugin but also all of its children.
	 */
#ifdef HAVE_SIGACTION
	sig_action.sa_sigaction = NULL;
	sig_action.sa_handler = SIG_IGN;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;
	sigaction(SIGTERM, &sig_action, NULL);
	sigaction(SIGSEGV, &sig_action, NULL);
#else
	signal(SIGTERM, SIG_IGN);
	signal(SIGSEGV, SIG_IGN);
#endif
	kill(0, SIGTERM);
	while (waitpid(-1, &discard, WNOHANG) > 0)
		; /* do nothing */
	sleep(1);
	while ((cp = (child_process *)squeue_pop(sq))) {
		/* kill all processes in the child's process group */
		(void)kill(-cp->ei->pid, SIGKILL);
	}
	sleep(1);
	while (waitpid(-1, &discard, WNOHANG) > 0)
		; /* do nothing */

	exit(code);
}

/*
 * write a log message to master.
 * Note that this will break if we change delimiters someday,
 * but avoids doing several extra malloc()+free() for this
 * pretty simple case.
 */
__attribute__((__format__(__printf__, 1, 2)))
static void wlog(const char *fmt, ...)
{
#define LOG_KEY_LEN 4
	static char lmsg[8192] = "log=";
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(lmsg + LOG_KEY_LEN, sizeof(lmsg) - LOG_KEY_LEN - MSG_DELIM_LEN, fmt, ap);
	va_end(ap);
	if (len < 0) {
		/* We can't send what we can't print. */
		return;
	}

	len += LOG_KEY_LEN; /* log= */
	if (len > sizeof(lmsg) - MSG_DELIM_LEN - 1) {
		/* A truncated log is better than no log or buffer overflows. */
		len = sizeof(lmsg) - MSG_DELIM_LEN - 1;
	}

	/* Add the kv pair separator and the message delimiter. */
	lmsg[len] = 0;
	len++;
	memcpy(lmsg + len, MSG_DELIM, MSG_DELIM_LEN);
	len += MSG_DELIM_LEN;

	if (write(master_sd, lmsg, len) < 0 && errno == EPIPE) {
		/* Master has died or abandoned us, so exit. */
		exit_worker(1, "Failed to write() to master");
	}
}

__attribute__((__format__(__printf__, 3, 4)))
static void job_error(child_process *cp, struct kvvec *kvv, const char *fmt, ...)
{
	char msg[4096];
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	if (len < 0) {
		/* We can't send what we can't print. */
		kvvec_destroy(kvv, 0);
		return;
	}

	if (len > sizeof(msg) - 1) {
		/* A truncated log is better than no log or buffer overflows. */
		len = sizeof(msg) - 1;
	}
	msg[len] = 0;

	if (cp) {
		kvvec_addkv(kvv, "job_id", mkstr("%d", cp->id));
	}
	kvvec_addkv_wlen(kvv, "error_msg", 9, msg, len);

	if (worker_send_kvvec(master_sd, kvv) < 0 && errno == EPIPE) {
		/* Master has died or abandoned us, so exit. */
		exit_worker(1, "Failed to send job error key/value vector to master");
	}

	kvvec_destroy(kvv, 0);
}

struct kvvec_buf *build_kvvec_buf(struct kvvec *kvv)
{
	struct kvvec_buf *kvvb;

	/*
	 * key=value, separated by PAIR_SEP and messages
	 * delimited by MSG_DELIM
	 */
	kvvb = kvvec2buf(kvv, KV_SEP, PAIR_SEP, MSG_DELIM_LEN);
	if (!kvvb) {
		return NULL;
	}
	memcpy(kvvb->buf + (kvvb->bufsize - MSG_DELIM_LEN), MSG_DELIM, MSG_DELIM_LEN);

	return kvvb;
}

int worker_send_kvvec(int sd, struct kvvec *kvv)
{
	int ret;
	struct kvvec_buf *kvvb;

	kvvb = build_kvvec_buf(kvv);
	if (!kvvb)
		return -1;

	/* bufsize, not buflen, as it gets us the delimiter */
	/* ret = write(sd, kvvb->buf, kvvb->bufsize); */
    ret = nwrite(sd, kvvb->buf, kvvb->bufsize,NULL);
	free(kvvb->buf);
	free(kvvb);

	return ret;
}

int send_kvvec(int sd, struct kvvec *kvv)
{
	return worker_send_kvvec(sd, kvv);
}

char *worker_ioc2msg(iocache *ioc, unsigned long *size, int flags)
{
	return iocache_use_delim(ioc, MSG_DELIM, MSG_DELIM_LEN, size);
}

int worker_buf2kvvec_prealloc(struct kvvec *kvv, char *buf, unsigned long len, int kvv_flags)
{
	return buf2kvvec_prealloc(kvv, buf, len, KV_SEP, PAIR_SEP, kvv_flags);
}

#define kvvec_add_long(kvv, key, value) \
	do { \
		const char *buf = mkstr("%ld", value); \
		kvvec_addkv_wlen(kvv, key, sizeof(key) - 1, buf, strlen(buf)); \
	} while (0)

#define kvvec_add_tv(kvv, key, value) \
	do { \
		const char *buf = mkstr("%ld.%06ld", value.tv_sec, value.tv_usec); \
		kvvec_addkv_wlen(kvv, key, sizeof(key) - 1, buf, strlen(buf)); \
	} while (0)

/* forward declaration */
static void gather_output(child_process *cp, iobuf *io, int final);

static void destroy_job(child_process *cp)
{
	/*
	 * we must remove the job's timeout ticker,
	 * or we'll end up accessing an already free()'d
	 * pointer, or the pointer to a different child.
	 */
	squeue_remove(sq, cp->ei->sq_event);
	running_jobs--;
	fanout_remove(ptab, cp->ei->pid);

	if (cp->outstd.buf) {
		free(cp->outstd.buf);
		cp->outstd.buf = NULL;
	}
	if (cp->outerr.buf) {
		free(cp->outerr.buf);
		cp->outerr.buf = NULL;
	}

	if(NULL != cp->env) kvvec_destroy(cp->env, KVVEC_FREE_ALL);
	kvvec_destroy(cp->request, KVVEC_FREE_ALL);
	free(cp->cmd);

	free(cp->ei);
	free(cp);
}

#define strip_nul_bytes(io) \
	do { \
		char *nul; \
		if (!io.buf || !*io.buf) \
			io.len = 0; \
		else if ((nul = memchr(io.buf, 0, io.len))) { \
			io.len = (unsigned long)nul - (unsigned long)io.buf; \
		} \
	} while (0)

int finish_job(child_process *cp, int reason)
{
	static struct kvvec resp = KVVEC_INITIALIZER;
	struct rusage *ru = &cp->ei->rusage;
	int i, ret;

	/* get rid of still open filedescriptors */
	if (cp->outstd.fd != -1) {
		gather_output(cp, &cp->outstd, 1);
		iobroker_close(iobs, cp->outstd.fd);
	}
	if (cp->outerr.fd != -1) {
		gather_output(cp, &cp->outerr, 1);
		iobroker_close(iobs, cp->outerr.fd);
	}

	/* Make sure network-supplied data doesn't contain nul bytes */
	strip_nul_bytes(cp->outstd);
	strip_nul_bytes(cp->outerr);

	/* how many key/value pairs do we need? */
	if (kvvec_init(&resp, 12 + cp->request->kv_pairs) == NULL) {
		/* what the hell do we do now? */
		exit_worker(1, "Failed to init response key/value vector");
	}

	gettimeofday(&cp->ei->stop, NULL);

	if (running_jobs != squeue_size(sq)) {
		wlog("running_jobs(%d) != squeue_size(sq) (%d)\n",
			 running_jobs, squeue_size(sq));
		wlog("started: %d; running: %d; finished: %d\n",
			 started, running_jobs, started - running_jobs);
	}

	cp->ei->runtime = tv_delta_f(&cp->ei->start, &cp->ei->stop);

	/*
	 * Now build the return message.
	 * First comes the request, minus environment variables
	 */
	for (i = 0; i < cp->request->kv_pairs; i++) {
		struct key_value *kv = &cp->request->kv[i];
		/* skip environment macros */
		if (kv->key_len == 3 && !strcmp(kv->key, "env")) {
			continue;
		}
		kvvec_addkv_wlen(&resp, kv->key, kv->key_len, kv->value, kv->value_len);
	}
	kvvec_addkv(&resp, "wait_status", mkstr("%d", cp->ret));
	kvvec_add_tv(&resp, "start", cp->ei->start);
	kvvec_add_tv(&resp, "stop", cp->ei->stop);
	kvvec_addkv(&resp, "runtime", mkstr("%f", cp->ei->runtime));
	if (!reason) {
		/* child exited nicely (or with a signal, so check wait_status) */
		kvvec_addkv(&resp, "exited_ok", "1");
		kvvec_add_tv(&resp, "ru_utime", ru->ru_utime);
		kvvec_add_tv(&resp, "ru_stime", ru->ru_stime);
		kvvec_add_long(&resp, "ru_minflt", ru->ru_minflt);
		kvvec_add_long(&resp, "ru_majflt", ru->ru_majflt);
		kvvec_add_long(&resp, "ru_inblock", ru->ru_inblock);
		kvvec_add_long(&resp, "ru_oublock", ru->ru_oublock);
	} else {
		/* some error happened */
		kvvec_addkv(&resp, "exited_ok", "0");
		kvvec_addkv(&resp, "error_code", mkstr("%d", reason));
	}
	kvvec_addkv_wlen(&resp, "outerr", 6, cp->outerr.buf, cp->outerr.len);
	kvvec_addkv_wlen(&resp, "outstd", 6, cp->outstd.buf, cp->outstd.len);
	ret = worker_send_kvvec(master_sd, &resp);
	if (ret < 0 && errno == EPIPE)
		exit_worker(1, "Failed to send kvvec struct to master");

	return 0;
}

static int check_completion(child_process *cp, int flags)
{
	int result, status;

	if (!cp || !cp->ei->pid) {
		return 0;
	}

	/*
	 * we mustn't let EINTR interrupt us, since it could well
	 * be a SIGCHLD from the properly exiting process doing it
	 */
	do {
		errno = 0;
		result = wait4(cp->ei->pid, &status, flags, &cp->ei->rusage);
	} while (result < 0 && errno == EINTR);

	if (result == cp->ei->pid || (result < 0 && errno == ECHILD)) {
		cp->ret = status;
		finish_job(cp, 0);
		destroy_job(cp);
		return 0;
	}

	if (!result)
		return -1;

	return -errno;
}

/*
 * "What can the harvest hope for, if not for the care
 * of the Reaper Man?"
 *   -- Terry Pratchett, Reaper Man
 *
 * We end up here no matter if the job is stale (ie, the child is
 * stuck in uninterruptable sleep) or if it's the first time we try
 * to kill it.
 * A job is considered reaped once we reap our direct child, in
 * which case init will become parent of our grandchildren.
 * It's also considered fully reaped if kill() results in ESRCH or
 * EPERM, or if wait()ing for the process group results in ECHILD.
 */
static void kill_job(child_process *cp, int reason)
{
	int ret, status, reaped = 0;
	int pid = cp ? cp->ei->pid : 0;

	/*
	 * first attempt at reaping, so see if we just failed to
	 * notice that things were going wrong her
	 */
	if (reason == ETIME && !check_completion(cp, WNOHANG)) {
		timeouts++;
		wlog("job %d with pid %d reaped at timeout. timeouts=%u; started=%u", cp->id, pid, timeouts, started);
		return;
	}

	/* brutal but efficient */
	if (kill(-cp->ei->pid, SIGKILL) < 0) {
		if (errno == ESRCH) {
			reaped = 1;
		} else {
			wlog("kill(-%ld, SIGKILL) failed: %s\n", (long)cp->ei->pid, strerror(errno));
		}
	}

	/*
	 * we must iterate at least once, in case kill() returns
	 * ESRCH when there's zombies
	 */
	do {
		ret = waitpid(cp->ei->pid, &status, WNOHANG);
		if (ret < 0 && errno == EINTR)
			continue;

		if (ret == cp->ei->pid || (ret < 0 && errno == ECHILD)) {
			reaped = 1;
			break;
		}
		if (!ret) {
			struct timeval tv;

			gettimeofday(&tv, NULL);
			/*
			 * stale process (signal may not have been delivered, or
			 * the child can be stuck in uninterruptible sleep). We
			 * can't hang around forever, so just reschedule a new
			 * reap attempt later.
			 */
			if (reason == ESTALE) {
				wlog("tv.tv_sec is currently %llu", (unsigned long long)tv.tv_sec);
				tv.tv_sec += 5;
				wlog("Failed to reap child with pid %ld. Next attempt @ %llu.%lu", (long)cp->ei->pid, (unsigned long long)tv.tv_sec, (unsigned long)tv.tv_usec);
			} else {
				tv.tv_usec = 250000;
				if (tv.tv_usec > 1000000) {
					tv.tv_usec -= 1000000;
					tv.tv_sec += 1;
				}
				cp->ei->state = ESTALE;
				finish_job(cp, reason);
			}
			squeue_remove(sq, cp->ei->sq_event);
			cp->ei->sq_event = squeue_add_tv(sq, &tv, cp);
			return;
		}
	} while (!reaped);

	if (cp->ei->state != ESTALE)
		finish_job(cp, reason);
	else
		wlog("job %d (pid=%ld): Dormant child reaped", cp->id, (long)cp->ei->pid);
	destroy_job(cp);
}

static void gather_output(child_process *cp, iobuf *io, int final)
{
	int retry = 5;

	for (;;) {
		char buf[4096];
		int rd;

		rd = read(io->fd, buf, sizeof(buf));
		if (rd < 0) {
			if (errno == EAGAIN && !final)
				break;
			if (errno == EINTR || errno == EAGAIN) {
				char	buf[1024];
				if (--retry == 0) {
					sprintf(buf, "job %d (pid=%ld): Failed to read(): %s", cp->id, (long)cp->ei->pid, strerror(errno));
					break;
				}
				sprintf(buf, "job %d (pid=%ld): read() returned error %d", cp->id, (long)cp->ei->pid, errno);
				syslog(LOG_NOTICE, "%s", buf);
				sleep(1);
				continue;
			}
			if (!final && errno != EAGAIN)
				wlog("job %d (pid=%ld): Failed to read(): %s", cp->id, (long)cp->ei->pid, strerror(errno));
		}

		if (rd > 0) {
			/* we read some data */
			io->buf = realloc(io->buf, rd + io->len + 1);
			memcpy(&io->buf[io->len], buf, rd);
			io->len += rd;
			io->buf[io->len] = '\0';
		}

		/*
		 * Close down on bad, zero and final reads (we don't get
		 * EAGAIN, so all errors are really unfixable)
		 */
		if (rd <= 0 || final) {
			iobroker_close(iobs, io->fd);
			io->fd = -1;
			if (!final)
				check_completion(cp, WNOHANG);
			break;
		}

		break;
	}
}


static int stderr_handler(int fd, int events, void *cp_)
{
	child_process *cp = (child_process *)cp_;
	gather_output(cp, &cp->outerr, 0);
	return 0;
}

static int stdout_handler(int fd, int events, void *cp_)
{
	child_process *cp = (child_process *)cp_;
	gather_output(cp, &cp->outstd, 0);
	return 0;
}

static void sigchld_handler(int sig)
{
	reapable++;
}

static void reap_jobs(void)
{
	int reaped = 0;
	do {
		int pid, status;
		struct rusage ru;
		pid = wait3(&status, WNOHANG, &ru);
		if (pid > 0) {
			struct child_process *cp;

			reapable--;
			if (!(cp = fanout_get(ptab, pid))) {
				/* we reaped a lost child. Odd that */
				continue;
			}
			cp->ret = status;
			memcpy(&cp->ei->rusage, &ru, sizeof(ru));
			reaped++;
			if (cp->ei->state != ESTALE)
				finish_job(cp, cp->ei->state);
			destroy_job(cp);
		}
		else if (!pid || (pid < 0 && errno == ECHILD)) {
			reapable = 0;
		}
	} while (reapable);
}

void cmd_iobroker_register(int fdout, int fderr, void *arg) {
	/* We must never block, even if plugins issue '_exit()' */
	fcntl(fdout, F_SETFL, O_NONBLOCK);
	fcntl(fderr, F_SETFL, O_NONBLOCK);
	if( iobroker_register(iobs, fdout, arg, stdout_handler) < 0) {
		wlog("Failed to register iobroker for stdout");
	}
	if( iobroker_register(iobs, fderr, arg, stderr_handler) < 0) {
		wlog("Failed to register iobroker for stderr");
	}
}

char **env_from_kvvec(struct kvvec *kvv_env) {

	int i;
	char **env;

	if(NULL == kvv_env) return NULL;

	env = calloc(kvv_env->kv_pairs*2+1, sizeof(char *));
	for (i = 0; i < kvv_env->kv_pairs; i++) {
		struct key_value *kv = &kvv_env->kv[i];
		env[i*2] = kv->key;
		env[i*2+1] = kv->value;
	}
	env[i*2] = NULL;

	return env;
}

int start_cmd(child_process *cp)
{
	int pfd[2] = {-1, -1}, pfderr[2] = {-1, -1};

	char **env = env_from_kvvec(cp->env);

	cp->outstd.fd = runcmd_open(cp->cmd, pfd, pfderr, env, 
			cmd_iobroker_register, cp);
	my_free(env);
	if (cp->outstd.fd < 0) {
		return -1;
	}

	cp->outerr.fd = pfderr[0];
	cp->ei->pid = runcmd_pid(cp->outstd.fd);
	/* no pid means we somehow failed */
	if (!cp->ei->pid) {
		return -1;
	}

	fanout_add(ptab, cp->ei->pid, cp);

	return 0;
}

static iocache *ioc;

static child_process *parse_command_kvvec(struct kvvec *kvv)
{
	int i;
	child_process *cp;

	/* get this command's struct and insert it at the top of the list */
	cp = calloc(1, sizeof(*cp));
	if (!cp) {
		wlog("Failed to calloc() a child_process struct");
		return NULL;
	}
	cp->ei = calloc(1, sizeof(*cp->ei));
	if (!cp->ei) {
		wlog("Failed to calloc() a execution_information struct");
		return NULL;
	}

	/*
	 * we must copy from the vector, since it points to data
	 * found in the iocache where we read the command, which will
	 * be overwritten when we receive one next
	 */
	for (i = 0; i < kvv->kv_pairs; i++) {
		struct key_value *kv = &kvv->kv[i];
		char *key = kv->key;
		char *value = kv->value;
		char *endptr;

		if (!strcmp(key, "command")) {
			cp->cmd = strdup(value);
			continue;
		}
		if (!strcmp(key, "job_id")) {
			cp->id = (unsigned int)strtoul(value, &endptr, 0);
			continue;
		}
		if (!strcmp(key, "timeout")) {
			cp->timeout = (unsigned int)strtoul(value, &endptr, 0);
			continue;
		}
		if (!strcmp(key, "env")) {
			cp->env = buf2kvvec(value, strlen(value), '=', '\n', KVVEC_COPY);
			continue;
		}
	}

	/* jobs without a timeout get a default of 60 seconds. */
	if (!cp->timeout) {
		cp->timeout = 60;
	}

	return cp;
}

static void spawn_job(struct kvvec *kvv, int(*cb)(child_process *))
{
	int result;
	child_process *cp;

	if (!kvv) {
		wlog("Received NULL command key/value vector. Bug in iocache.c or kvvec.c?");
		return;
	}

	cp = parse_command_kvvec(kvv);
	if (!cp) {
		job_error(NULL, kvv, "Failed to parse worker-command");
		return;
	}
	if (!cp->cmd) {
		job_error(cp, kvv, "Failed to parse commandline. Ignoring job %u", cp->id);
		return;
	}

	gettimeofday(&cp->ei->start, NULL);
	cp->request = kvv;
	cp->ei->sq_event = squeue_add(sq, cp->timeout + time(NULL), cp);
	started++;
	running_jobs++;
	result = cb(cp);
	if (result < 0) {
		job_error(cp, kvv, "Failed to start child: %s: %s", runcmd_strerror(result), strerror(errno));
		squeue_remove(sq, cp->ei->sq_event);
		running_jobs--;
		return;
	}
}

static int receive_command(int sd, int events, void *arg)
{
	int ioc_ret;
	char *buf;
	unsigned long size;

	if (!ioc) {
		ioc = iocache_create(512 * 1024);
	}
	ioc_ret = iocache_read(ioc, sd);

	/* master closed the connection, so we exit */
	if (ioc_ret == 0) {
		iobroker_close(iobs, sd);
		exit_worker(0, NULL);
	}
	if (ioc_ret < 0) {
		/* XXX: handle this somehow */
	}

#if 0
	/* debug-volley */
	buf = iocache_use_size(ioc, ioc_ret);
	write(master_sd, buf, ioc_ret);
	return 0;
#endif
	/*
	 * now loop over all inbound messages in the iocache.
	 * Since KV_TERMINATOR is a nul-byte, they're separated by 3 nuls
	 */
	while ((buf = iocache_use_delim(ioc, MSG_DELIM, MSG_DELIM_LEN, &size))) {
		struct kvvec *kvv;
		/* we must copy vars here, as we preserve them for the response */
		kvv = buf2kvvec(buf, (unsigned int)size, KV_SEP, PAIR_SEP, KVVEC_COPY);
		if (kvv)
			spawn_job(kvv, arg);
	}

	return 0;
}

int worker_set_sockopts(int sd, int bufsize)
{
	int ret;

	ret = fcntl(sd, F_SETFD, FD_CLOEXEC);
	ret |= fcntl(sd, F_SETFL, O_NONBLOCK);

	if (!bufsize)
		return ret;
	ret |= setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(int));
	ret |= setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(int));

	return ret;
}

/* XXX: deprecated */
int set_socket_options(int sd, int bufsize)
{
	return worker_set_sockopts(sd, bufsize);
}

void enter_worker(int sd, int (*cb)(child_process*))
{
#ifdef HAVE_SIGACTION
	struct sigaction sig_action;
#endif

	/* created with socketpair(), usually */
	master_sd = sd;
	parent_pid = getppid();
	(void)chdir("/tmp");
	(void)chdir("nagios-workers");

	ptab = fanout_create(4096);

	if (setpgid(0, 0)) {
		/* XXX: handle error somehow, or maybe just ignore it */
	}

	/* we need to catch child signals to mark jobs as reapable */
#ifdef HAVE_SIGACTION
	sig_action.sa_sigaction = NULL;
	sig_action.sa_handler = sigchld_handler;
	sigfillset(&sig_action.sa_mask);
	sig_action.sa_flags=SA_NOCLDSTOP;
	sigaction(SIGCHLD, &sig_action, NULL);
#else
	signal(SIGCHLD, sigchld_handler);
#endif

	fcntl(fileno(stdout), F_SETFD, FD_CLOEXEC);
	fcntl(fileno(stderr), F_SETFD, FD_CLOEXEC);
	fcntl(master_sd, F_SETFD, FD_CLOEXEC);
	iobs = iobroker_create();
	if (!iobs) {
		/* XXX: handle this a bit better */
		exit_worker(EXIT_FAILURE, "Worker failed to create io broker socket set");
	}

	/*
	 * Create a modest scheduling queue that will be
	 * more than enough for our needs
	 */
	sq = squeue_create(1024);
	worker_set_sockopts(master_sd, 256 * 1024);

	iobroker_register(iobs, master_sd, cb, receive_command);
	while (iobroker_get_num_fds(iobs) > 0) {
		int poll_time = -1;

		/* check for timed out jobs */
		while (running_jobs) {
			child_process *cp;
			struct timeval now, tmo;

			/* stop when scheduling queue is empty */
			cp = (child_process *)squeue_peek(sq);
			if (!cp)
				break;

			tmo.tv_usec = cp->ei->start.tv_usec;
			tmo.tv_sec = cp->ei->start.tv_sec + cp->timeout;
			gettimeofday(&now, NULL);
			poll_time = tv_delta_msec(&now, &tmo);
			/*
			 * A little extra takes care of rounding errors and
			 * ensures we never kill a job before it times out.
			 * 5 milliseconds is enough to take care of that.
			 */
			poll_time += 5;
			if (poll_time > 0)
				break;

			if (cp->ei->state == ESTALE) {
				if(cp->ei->sq_event &&
						squeue_evt_when_is_after(cp->ei->sq_event, &now)) {
					/* If the state is already stale, there is a already
						a job to kill the child on the queue so don't
						add another one here. */
					kill_job(cp, ESTALE);
				}
			} else {
				/* this job timed out, so kill it */
				wlog("job %d (pid=%ld) timed out. Killing it", cp->id, (long)cp->ei->pid);
				kill_job(cp, ETIME);
			}
		}

		iobroker_poll(iobs, poll_time);

		if (reapable)
			reap_jobs();
	}

	/* we exit when the master shuts us down */
	exit(EXIT_SUCCESS);
}

int spawn_named_helper(char *path, char **argv)
{
	int ret, pid;

	pid = fork();
	if (pid < 0) {
		return -1;
	}

	/* parent leaves early */
	if (pid)
		return pid;

	ret = execvp(path, argv);
	/* if execvp() fails, there's really nothing we can do */
	exit(ret);
}

int spawn_helper(char **argv)
{
	return spawn_named_helper(argv[0], argv);
}
