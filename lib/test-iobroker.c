#include <signal.h>
#include <stdio.h>
#include <malloc.h>
#include <netdb.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include "iobroker.c"
#include "t-utils.h"

static iobroker_set *iobs;

static char *msg[] = {
	"Welcome to the echo service!\n",
	"test msg nr 2",
	"random piece of string\nwith\nlots\nof\nnewlines\n\n\n\n\0",
	"another totally random message\n",
	"and this one's from emma. She's alright, really",
	NULL,
};

static int echo_service(int fd, int events, void *arg)
{
	char buf[1024];
	int len;

	len = read(fd, buf, sizeof(buf));
	if (len < 0) {
		perror("read");
		iobroker_close(iobs, fd);
		ok_int(iobroker_is_registered(iobs, fd), 0, "Closing must deregister");
		return 0;
	}
	/* zero read means we're disconnected */
	if (!len) {
		iobroker_close(iobs, fd);
		ok_int(iobroker_is_registered(iobs, fd), 0, "Closing must deregister");
		return 0;
	}

	write(fd, buf, len);

	return 0;
}

static int connected_handler(int fd, int events, void *arg)
{
	int *counter = (int *)arg;
	int i;

	i = *counter;

	if (events == IOBROKER_POLLIN) {
		char buf[1024];
		int len = read(fd, buf, sizeof(buf));

		buf[len] = 0;

		test(len == (int)strlen(msg[i]), "len match for message %d", i);
		test(!memcmp(buf, msg[i], len), "data match for message %d", i);
	}

	i++;

	if (i < 0 || i >= (int)ARRAY_SIZE(msg)) {
		fprintf(stderr, "i = %d in connected_handler(). What's up with that?\n", i);
		return 0;
	}

	if (!msg[i]) {
		iobroker_close(iobs, fd);
		return 0;
	}

	write(fd, msg[i], strlen(msg[i]));
	*counter = i;

	return 0;
}

static int listen_handler(int fd, int events, void *arg)
{
	int sock;
	struct sockaddr_in sain;
	socklen_t addrlen;

	if (!arg || arg != iobs) {
		printf("Argument passing seems to fail spectacularly\n");
	}

	addrlen = sizeof(sain);
	//printf("listen_handler(%d, %d, %p) called\n", fd, events, arg);
	sock = accept(fd, (struct sockaddr *)&sain, &addrlen);
	if (sock < 0) {
		perror("accept");
		return -1;
	}

	write(sock, msg[0], strlen(msg[0]));
	iobroker_register(iobs, sock, iobs, echo_service);
	ok_int(iobroker_is_registered(iobs, sock), 1, "is_registered must be true");
	return 0;
}

void sighandler(int sig)
{
	/* test failed */
	t_fail("Caught signal %d", sig);
	exit(t_end());
}


#define NUM_PROCS 500
static int proc_counter[NUM_PROCS];
static int conn_spam(struct sockaddr_in *sain)
{
	int i;
#ifdef HAVE_SIGACTION
	struct sigaction sig_action;

	sig_action.sa_sigaction = NULL;
	sig_action.sa_handler = SIG_IGN;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;
	sigaction(SIGPIPE, &sig_action, NULL);
	sig_action.sa_handler = sighandler;
	sigfillset(&sig_action.sa_mask);
	sig_action.sa_flags = SA_NODEFER|SA_RESTART;
	sigaction(SIGQUIT, &sig_action, NULL);
	sigaction(SIGINT, &sig_action, NULL);
#else /* HAVE_SIGACTION */
	signal(SIGALRM, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGPIPE, SIG_IGN);
#endif /* HAVE_SIGACTION */

	alarm(20);

	for (i = 0; i < NUM_PROCS; i++) {
		int fd, sockopt = 1;

		fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		(void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));
		proc_counter[i] = 0;
		iobroker_register(iobs, fd, (void *)&proc_counter[i], connected_handler);
		if (connect(fd, (struct sockaddr *)sain, sizeof(*sain))) {
			perror("connect");
		}
		iobroker_poll(iobs, -1);
	}
	return 0;
}

int main(int argc, char **argv)
{
	int listen_fd, flags, sockopt = 1;
	struct sockaddr_in sain;
	int error;
	const char *err_msg;

	t_set_colors(0);
	t_start("iobroker ipc test");

	error = iobroker_get_max_fds(NULL);
	ok_int(error, IOBROKER_ENOSET, "test errors when passing null");

	err_msg = iobroker_strerror(error);
	test(err_msg && !strcmp(err_msg, iobroker_errors[(~error) + 1].string), "iobroker_strerror() returns the right string");

	iobs = iobroker_create();
	error = iobroker_get_max_fds(iobs);
	test(iobs && error >= 0, "max fd's for real iobroker set must be > 0");

	listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	flags = fcntl(listen_fd, F_GETFD);
	flags |= FD_CLOEXEC;
	fcntl(listen_fd, F_SETFD, flags);

	(void)setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));

	memset(&sain, 0, sizeof(sain));
	sain.sin_port = ntohs(9123);
	sain.sin_family = AF_INET;
	bind(listen_fd, (struct sockaddr *)&sain, sizeof(sain));
	listen(listen_fd, 128);
	iobroker_register(iobs, listen_fd, iobs, listen_handler);

	if (argc == 1)
		conn_spam(&sain);

	for (;;) {
		iobroker_poll(iobs, -1);
		if (iobroker_get_num_fds(iobs) <= 1) {
			break;
		}
	}

	iobroker_close(iobs, listen_fd);
	iobroker_destroy(iobs, 0);

	t_end();
	return 0;
}
