/*
 * Simple test-program to try multiplexing running other programs
 * through the worker process layer.
 */

#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include "worker.h"


typedef struct simple_worker {
	int pid, sd;
	unsigned int job_index;
	iocache *ioc;
} simple_worker;

/* we can't handle packets larger than 64MiB */
#define MAX_IOCACHE_SIZE (64 * 1024 * 1024)
static int sigreceived;
static iobroker_set *iobs;

static simple_worker *spawn_worker(void (*init_func)(void *), void *init_arg)
{
	int sv[2];
	int pid;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
		return NULL;

	pid = fork();
	if (pid < 0) {
		close(sv[0]);
		close(sv[1]);
		return NULL;
	}

	/* parent leaves the child */
	if (pid) {
		simple_worker *worker = calloc(1, sizeof(simple_worker));
		close(sv[1]);
		if (!worker) {
			kill(SIGKILL, pid);
			close(sv[0]);
			return NULL;
		}
		worker->sd = sv[0];
		worker->pid = pid;
		worker->ioc = iocache_create(1 * 1024 * 1024);
		return worker;
	}

	/* child closes parent's end of socket and gets busy */
	close(sv[0]);

	if (init_func) {
		init_func(init_arg);
	}
	enter_worker(sv[1], start_cmd);

	/* not reached, ever */
	exit(EXIT_FAILURE);
}

static void die(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

static void sighandler(int sig)
{
	sigreceived = sig;
	printf("%d: caught sig %d\n", getpid(), sig);
}

static void child_exited(int sig)
{
	struct rusage ru;
	int status, result;

	result = wait3(&status, 0, &ru);
	printf("wait3() status: %d; return %d: %s\n",
		 status, result, strerror(errno));
	if (WIFEXITED(status)) {
		printf("Child with pid %d exited normally\n", result);
	}
	if (WIFSIGNALED(status)) {
		printf("Child caught signal %d\n", WTERMSIG(status));
		printf("Child did%s produce a core dump\n", WCOREDUMP(status) ? "" : " not");
	}
	exit(1);
}

static int print_input(int sd, int events, void *wp_)
{
	int ret, pkt = 0;
	simple_worker *wp = (simple_worker *)wp_;
	struct kvvec kvv = KVVEC_INITIALIZER;
	char *buf;
	unsigned long tot_bytes = 0, size;

	/*
	 * if some command filled the buffer, we grow it and read some
	 * more until we hit the limit
	 * @todo Define a limit :p
	 */
	size = iocache_size(wp->ioc);
	if (!iocache_capacity(wp->ioc)) {
		if (iocache_size(wp->ioc) < MAX_IOCACHE_SIZE) {
			/* double the size */
			iocache_grow(wp->ioc, iocache_size(wp->ioc));
			printf("Growing iocache for worker %d. sizes old/new %lu/%lu\n",
				   wp->pid, size, iocache_size(wp->ioc));
		} else {
			printf("iocache_size() for worker %d is already at max\n", wp->pid);
		}
	}

	ret = iocache_read(wp->ioc, sd);
	if (!ret) {
		printf("Worker with pid %d seems to have crashed. Exiting\n", wp->pid);
		exit(1);
	}
	if (ret < 0) {
		printf("iocache_read() from worker %d returned %d: %m\n", wp->pid, ret);
		return 0;
	}
	printf("read %d bytes from worker with pid %d::\n", ret, wp->pid);
	while ((buf = worker_ioc2msg(wp->ioc, &size, 0))) {
		int i, ret;
		tot_bytes += size;
		ret = worker_buf2kvvec_prealloc(&kvv, buf, (unsigned int)size, KVVEC_ASSIGN);
		if (!ret < 0) {
			printf("main: Failed to parse buffer of size %lu to key/value vector\n", size);
			continue;
		}
		for (i = 0; i < kvv.kv_pairs; i++) {
			struct key_value *kv = &kvv.kv[i];
			if (!i && memcmp(kv->key, buf, kv->key_len)) {
				printf("### kv[0]->key doesn't match buf. error in kvvec?\n");
			}
			printf("main: %2d.%02d: %s=%s\n", pkt, i, kv->key, kv->value);
		}
		pkt++;
	}

	printf("iocache: available: %lu; size: %lu; capacity: %lu\n",
		   iocache_available(wp->ioc), iocache_size(wp->ioc), iocache_capacity(wp->ioc));
	printf("Got %d packets in %ld bytes (ret: %d)\n", pkt, tot_bytes, ret);

	return 0;
}

#define NWPS 3
static simple_worker *wps[NWPS];
static int wp_index;

static int send_command(int sd, int events, void *discard)
{
	char buf[8192];
	int ret;
	simple_worker *wp;
	struct kvvec *kvv;

	ret = read(sd, buf, sizeof(buf));
	if (ret == 0) {
		iobroker_close(iobs, sd);
		return 0;
	}
	if (ret < 0) {
		printf("main: Failed to read() from fd %d: %s",
			   sd, strerror(errno));
	}

	/* this happens when we're reading from stdin */
	buf[--ret] = 0;

	kvv = kvvec_create(5);
	wp = wps[wp_index++ % NWPS];
	kvvec_addkv(kvv, "job_id", (char *)mkstr("%d", wp->job_index++));
	kvvec_addkv_wlen(kvv, "command", sizeof("command") - 1, buf, ret);
	kvvec_addkv(kvv, "timeout", (char *)mkstr("%d", 10));
	printf("Sending kvvec with %d pairs to worker %d\n", kvv->kv_pairs, wp->pid);
	worker_send_kvvec(wp->sd, kvv);
	kvvec_destroy(kvv, 0);
	return 0;
}

void print_some_crap(void *arg)
{
	char *str = (char *)arg;

	printf("%d: Argument passed: %s\n", getpid(), str);
}

int main(int argc, char **argv)
{
	simple_worker *wp;
	int i;
#ifdef HAVE_SIGACTION
	struct sigaction sig_action;

	sig_action.sa_sigaction = NULL;
	sigfillset(&sig_action.sa_mask);
	sig_action.sa_flags=SA_NOCLDSTOP;
	sig_action.sa_handler = child_exited;
	sigaction(SIGCHLD, &sig_action, NULL);

	sig_action.sa_flags = SA_NODEFER|SA_RESTART;
	sig_action.sa_handler = sighandler;
	sigfillset(&sig_action.sa_mask);
	sigaction(SIGINT, &sig_action, NULL);
	sigaction(SIGPIPE, &sig_action, NULL);
#else /* HAVE_SIGACTION */

	signal(SIGINT, sighandler);
	signal(SIGPIPE, sighandler);
	signal(SIGCHLD, child_exited);
#endif /* HAVE_SIGACTION */

	iobs = iobroker_create();
	if (!iobs)
		die("Failed to create io broker set");

	for (i = 0; i < NWPS; i++) {
		wp = spawn_worker(print_some_crap, "lalala");
		if (!wp) {
			die("Failed to spawn worker(s)\n");
		}
		wps[i] = wp;
		printf("Registering worker sd %d with io broker\n", wp->sd);
		iobroker_register(iobs, wp->sd, wp, print_input);
	}

	iobroker_register(iobs, fileno(stdin), NULL, send_command);

	/* get to work */
	while (!sigreceived && iobroker_get_num_fds(iobs)) {
		iobroker_poll(iobs, -1);
	}

	for (i = 0; i < NWPS; i++) {
		kill(wps[i]->pid, SIGKILL);
	}

	return 0;
}
