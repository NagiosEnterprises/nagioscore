/*
 * $Id: runcmd.c,v 1.3 2005/08/01 23:51:34 exon Exp $
 *
 * A simple interface to executing programs from other programs, using an
 * optimized and safe popen()-like implementation. It is considered safe
 * in that no shell needs to be spawned and the environment passed to the
 * execve()'d program is essentially empty.
 *
 *
 * The code in this file is a derivative of popen.c which in turn was taken
 * from "Advanced Programming for the Unix Environment" by W. Richard Stevens.
 *
 * Care has been taken to make sure the functions are async-safe. The one
 * function which isn't is runcmd_init() which it doesn't make sense to
 * call twice anyway, so the api as a whole should be considered async-safe.
 *
 */

#define NAGIOSPLUG_API_C 1

/* includes **/
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include "runcmd.h"


/** macros **/
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif

#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

/* 4.3BSD Reno <signal.h> doesn't define SIG_ERR */
#if defined(SIG_IGN) && !defined(SIG_ERR)
# define SIG_ERR ((Sigfunc *)-1)
#endif

/* This variable must be global, since there's no way the caller
 * can forcibly slay a dead or ungainly running program otherwise.
 * Multithreading apps and plugins can initialize it (via runcmd_init())
 * in an async safe manner PRIOR to calling runcmd() for the first time.
 *
 * The check for initialized values is atomic and can
 * occur in any number of threads simultaneously. */
static pid_t *pids = NULL;

/* If OPEN_MAX isn't defined, we try the sysconf syscall first.
 * If that fails, we fall back to an educated guess which is accurate
 * on Linux and some other systems. There's no guarantee that our guess is
 * adequate and the program will die with SIGSEGV if it isn't and the
 * upper boundary is breached. */
#ifdef OPEN_MAX
# define maxfd OPEN_MAX
#else
# ifndef _SC_OPEN_MAX /* sysconf macro unavailable, so guess */
#  define maxfd 256
# else
static int maxfd = 0;
# endif /* _SC_OPEN_MAX */
#endif /* OPEN_MAX */


const char *runcmd_strerror(int code)
{
	switch (code) {
	case RUNCMD_EFD:
		return "pipe() or open() failed";
	case RUNCMD_EALLOC:
		return "memory allocation failed";
	case RUNCMD_ECMD:
		return "command too complicated";
	case RUNCMD_EFORK:
		return "failed to fork()";
	case RUNCMD_EINVAL:
		return "invalid parameters";
	case RUNCMD_EWAIT:
		return "wait() failed";
	}
	return "unknown";
}

/* yield the pid belonging to a particular file descriptor */
pid_t runcmd_pid(int fd)
{
	if(!pids || fd >= maxfd || fd < 0)
		return 0;

	return pids[fd];
}

/*
 * Simple command parser which is still tolerably accurate for our
 * simple needs. It might serve as a useful example on how to program
 * a state-machine though.
 *
 * It's up to the caller to handle output redirection, job control,
 * conditional statements, variable substitution, nested commands and
 * function execution. We do mark such occasions with the return code
 * though, which is to be interpreted as a bitfield with potentially
 * multiple flags set.
 */
#define STATE_NONE  0
#define STATE_WHITE (1 << 0)
#define STATE_INARG (1 << 1)
#define STATE_INSQ  (1 << 2)
#define STATE_INDQ  (1 << 3)
#define STATE_SPECIAL (1 << 4)
#define in_quotes (state & (STATE_INSQ | STATE_INDQ))
#define is_state(s) (state == s)
#define set_state(s) (state = s)
#define have_state(s) ((state & s) == s)
#define add_state(s) (state |= s)
#define del_state(s) (state &= ~s)
#define add_ret(r) (ret |= r)
int runcmd_cmd2strv(const char *str, int *out_argc, char **out_argv)
{
	int arg = 0, i, a = 0;
	int state, ret = 0;
	size_t len;
	char *argz;

	set_state(STATE_NONE);
	len = strlen(str);

	argz = malloc(len + 10);
	for (i = 0; i < len; i++) {
		const char *p = &str[i];

		switch (*p) {
		case 0:
			return ret;

		case ' ': case '\t': case '\r': case '\n':
			if (is_state(STATE_INARG)) {
				set_state(STATE_NONE);
				argz[a++] = 0;
				continue;
			}
			if (!in_quotes)
				continue;

			break;

		case '\\':
			i++;
			break;

		case '\'':
			if (have_state(STATE_INDQ))
				break;
			if (have_state(STATE_INSQ)) {
				del_state(STATE_INSQ);
				continue;
			}

			/*
			 * quotes can come inside arguments or
			 * at the start of them
			 */
			if (is_state(STATE_NONE) || is_state(STATE_INARG)) {
				if (is_state(STATE_NONE)) {
					/* starting a new argument */
					out_argv[arg++] = &argz[a];
				}
				set_state(STATE_INSQ | STATE_INARG);
				continue;
			}
		case '"':
			if (have_state(STATE_INSQ))
				break;
			if (have_state(STATE_INDQ)) {
				del_state(STATE_INDQ);
				continue;
			}
			if (is_state(STATE_NONE) || is_state(STATE_INARG)) {
				if (is_state(STATE_NONE)) {
					out_argv[arg++] = &argz[a];
				}
				set_state(STATE_INDQ | STATE_INARG);
				continue;
			}
			break;

		case '|':
			if (!in_quotes) {
				add_ret(CMD_HAS_REDIR);
			}
			break;
		case '&': case ';':
			if (!in_quotes) {
				set_state(STATE_SPECIAL);
				add_ret(CMD_HAS_JOBCONTROL);
				if (i && str[i - 1] != *p) {
					argz[a++] = 0;
					out_argv[arg++] = &argz[a];
				}
			}
			break;

		case '`':
			if (!in_quotes) {
				add_ret(CMD_HAS_SUBCOMMAND);
			}
			break;

		case '(':
			if (!in_quotes) {
				add_ret(CMD_HAS_PAREN);
			}
			break;

		case '*': case '?':
			if (!in_quotes) {
				add_ret(CMD_HAS_WILDCARD);
			}

			/* fallthrough */

		default:
			break;
		}

		if (is_state(STATE_NONE)) {
			set_state(STATE_INARG);
			out_argv[arg++] = &argz[a];
		}

		/* by default we simply copy the byte */
		argz[a++] = str[i];
	}

	/* make sure we nul-terminate the last argument */
	argz[a++] = 0;

	if (have_state(STATE_INSQ))
		add_ret(CMD_HAS_UBSQ);
	if (have_state(STATE_INDQ))
		add_ret(CMD_HAS_UBDQ);

	*out_argc = arg;

	return ret;
}


/* this function is NOT async-safe. It is exported so multithreaded
 * plugins (or other apps) can call it prior to running any commands
 * through this api and thus achieve async-safeness throughout the api */
void runcmd_init(void)
{
#if defined(RLIMIT_NOFILE)
	if (!maxfd) {
		struct rlimit rlim;
		getrlimit(RLIMIT_NOFILE, &rlim);
		maxfd = rlim.rlim_cur;
	}
#elif !defined(OPEN_MAX) && !defined(IOV_MAX) && defined(_SC_OPEN_MAX)
	if(!maxfd) {
		if((maxfd = sysconf(_SC_OPEN_MAX)) < 0) {
			/* possibly log or emit a warning here, since there's no
			 * guarantee that our guess at maxfd will be adequate */
			maxfd = 256;
		}
	}
#endif

	if (!pids)
		pids = calloc(maxfd, sizeof(pid_t));
}


/* Start running a command */
int runcmd_open(const char *cmd, int *pfd, int *pfderr, char **env)
{
	char **argv = NULL;
	int cmd2strv_errors, argc = 0;
	size_t cmdlen;
	pid_t pid;

	int i = 0;

	if(!pids)
		runcmd_init();

	/* if no command was passed, return with no error */
	if (!cmd || !*cmd)
		return RUNCMD_EINVAL;

	cmdlen = strlen(cmd);
	argv = calloc((cmdlen / 2) + 5, sizeof(char *));
	if (!argv)
		return RUNCMD_EALLOC;

	cmd2strv_errors = runcmd_cmd2strv(cmd, &argc, argv);
	if (cmd2strv_errors) {
		/*
		 * if there are complications, we fall back to running
		 * the command via the shell
		 */
		free(argv[0]);
		argv[0] = "/bin/sh";
		argv[1] = "-c";
		argv[2] = strdup(cmd);
		if (!argv[2]) {
			free(argv);
			return RUNCMD_EALLOC;
		}
		argv[3] = NULL;
	}

	if (pipe(pfd) < 0) {
		if (!cmd2strv_errors)
			free(argv[0]);
		else
			free(argv[2]);
		free(argv);
		return RUNCMD_ECMD;
	}
	if (pipe(pfderr) < 0) {
		if (!cmd2strv_errors)
			free(argv[0]);
		else
			free(argv[2]);
		free(argv);
		close(pfd[0]);
		close(pfd[1]);
		return RUNCMD_EFD;
	}
	pid = fork();
	if (pid < 0) {
		if (!cmd2strv_errors)
			free(argv[0]);
		else
			free(argv[2]);
		free(argv);
		close(pfd[0]);
		close(pfd[1]);
		close(pfderr[0]);
		close(pfderr[1]);
		return RUNCMD_EFORK; /* errno set by the failing function */
	}

	/* child runs excevp() and _exit. */
	if (pid == 0) {
		close (pfd[0]);
		if (pfd[1] != STDOUT_FILENO) {
			dup2 (pfd[1], STDOUT_FILENO);
			close (pfd[1]);
		}
		close (pfderr[0]);
		if (pfderr[1] != STDERR_FILENO) {
			dup2 (pfderr[1], STDERR_FILENO);
			close (pfderr[1]);
		}

		/* close all descriptors in pids[]
		 * This is executed in a separate address space (pure child),
		 * so we don't have to worry about async safety */
		for (i = 0; i < maxfd; i++)
			if(pids[i] > 0)
				close (i);

		i = execvp(argv[0], argv);
		fprintf(stderr, "execvp(%s, ...) failed. errno is %d: %s\n", argv[0], errno, strerror(errno));
		if (!cmd2strv_errors)
			free(argv[0]);
		else
			free(argv[2]);
		_exit (errno);
	}

	/* parent picks up execution here */
	/*
	 * close childs file descriptors in our address space and
	 * release the memory we used that won't get passed to the
	 * caller.
	 */
	close(pfd[1]);
	close(pfderr[1]);
	if (!cmd2strv_errors)
		free(argv[0]);
	else
		free(argv[2]);
	free(argv);

	/* tag our file's entry in the pid-list and return it */
	pids[pfd[0]] = pid;

	return pfd[0];
}


int runcmd_close(int fd)
{
	int status;
	pid_t pid;

	/* make sure this fd was opened by runcmd_open() */
	if(fd < 0 || fd > maxfd || !pids || (pid = pids[fd]) == 0)
		return RUNCMD_EINVAL;

	pids[fd] = 0;
	if (close(fd) == -1)
		return -1;

	/* EINTR is ok (sort of), everything else is bad */
	while (waitpid(pid, &status, 0) < 0)
		if (errno != EINTR)
			return RUNCMD_EWAIT;

	/* return child's termination status */
	return (WIFEXITED(status)) ? WEXITSTATUS(status) : -1;
}


int runcmd_try_close(int fd, int *status, int sig)
{
	pid_t pid;
	int result;

	/* make sure this fd was opened by popen() */
	if(fd < 0 || fd > maxfd || !pids || !pids[fd])
		return RUNCMD_EINVAL;

	pid = pids[fd];
	while((result = waitpid(pid, status, WNOHANG)) != pid) {
		if(!result) return 0;
		if(result == -1) {
			switch(errno) {
			case EINTR:
				continue;
			case EINVAL:
				return -1;
			case ECHILD:
				if(sig) {
					result = kill(pid, sig);
					sig = 0;
					continue;
				}
				else return -1;
			} /* switch */
		}
	}

	pids[fd] = 0;
	close(fd);
	return result;
}
