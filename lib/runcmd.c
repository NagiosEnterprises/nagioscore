/*
 * A simple interface to executing programs from other programs, using an
 * optimized and safer popen()-like implementation. It is considered safer in
 * that no shell needs to be spawned for simple commands, and the environment
 * passed to the execve()'d program is essentially empty.
 *
 * This code is based on popen.c, which in turn was taken from
 * "Advanced Programming in the UNIX Environment" by W. Richard Stevens.
 *
 * Care has been taken to make sure the functions are async-safe. The exception
 * is runcmd_init() which multithreaded applications or plugins must call in a
 * non-reentrant manner before calling any other runcmd function.
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

/* Determine whether we have setenv()/unsetenv() (see setenv(3) on Linux) */
#if _BSD_SOURCE || _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
# define HAVE_SETENV
#endif

/*
 * This variable must be global, since there's no way the caller
 * can forcibly slay a dead or ungainly running program otherwise.
 *
 * The check for initialized values and allocation is not atomic, and can
 * potentially occur in any number of threads simultaneously.
 *
 * Multithreaded apps and plugins must initialize it (via runcmd_init())
 * in an async safe manner  before calling any other runcmd function.
 */
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
#define STATE_BSLASH (1 << 5)
#define in_quotes (state & (STATE_INSQ | STATE_INDQ))
#define is_state(s) (state == s)
#define set_state(s) (state = s)
#define have_state(s) ((state & s) == s)
#define add_state(s) (state |= s)
#define del_state(s) (state &= ~s)
#define add_ret(r) (ret |= r)
int runcmd_cmd2strv(const char *str, int *out_argc, char **out_argv)
{
	int arg = 0;
	int a = 0;
	unsigned int i;
	int state;
	int ret = 0;
	size_t len;
	char *argz;

	set_state(STATE_NONE);

	if (!str || !*str || !out_argc || !out_argv)
		return RUNCMD_EINVAL;

	len = strlen(str);
	argz = malloc(len + 1);
	if (!argz)
		return RUNCMD_EALLOC;

	/* Point argv[0] at the parsed argument string argz so we don't leak. */
	out_argv[0] = argz;
	out_argv[1] = NULL;

	for (i = 0; i < len; i++) {
		const char *p = &str[i];

		switch (*p) {
		case 0:
			if (arg == 0) free(out_argv[0]);
			out_argv[arg] = NULL;
			*out_argc = arg;
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
			/* single-quoted strings never interpolate backslashes */
			if (have_state(STATE_INSQ) || have_state(STATE_BSLASH)) {
				break;
			}
			/*
			 * double-quoted strings let backslashes escape
			 * a few, but not all, shell specials
			 */
			if (have_state(STATE_INDQ)) {
				const char next = str[i + 1];
				switch (next) {
				case '"': case '\\': case '$': case '`':
					add_state(STATE_BSLASH);
					continue;
				}
				break;
			}
			/*
			 * unquoted strings remove unescaped backslashes,
			 * but backslashes escape anything and everything
			 */
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
		case '<':
		case '>':
			if (!in_quotes) {
				add_ret(RUNCMD_HAS_REDIR);
			}
			break;
		case '&': case ';':
			if (!in_quotes) {
				set_state(STATE_SPECIAL);
				add_ret(RUNCMD_HAS_JOBCONTROL);
			}
			break;

		case '`':
			if (!have_state(STATE_INSQ) && !have_state(STATE_BSLASH)) {
				add_ret(RUNCMD_HAS_SUBCOMMAND);
			}
			break;

		case '(': case ')':
			if (!in_quotes) {
				add_ret(RUNCMD_HAS_PAREN);
			}
			break;

		case '$':
			if (!have_state(STATE_INSQ) && !have_state(STATE_BSLASH)) {
				if (p[1] == '(')
					add_ret(RUNCMD_HAS_SUBCOMMAND);
				else
					add_ret(RUNCMD_HAS_SHVAR);
			}
			break;

		case '*': case '?':
			if (!in_quotes) {
				add_ret(RUNCMD_HAS_WILDCARD);
			}
			break;

		default:
			break;
		}

		/* here, we're limited to escaped backslashes, so remove STATE_BSLASH */
		del_state(STATE_BSLASH);

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
		add_ret(RUNCMD_HAS_UBSQ);
	if (have_state(STATE_INDQ))
		add_ret(RUNCMD_HAS_UBDQ);

	out_argv[arg] = NULL;
	*out_argc = arg;

	return ret;
}


/* This function is NOT async-safe. It is exported so multithreaded
 * plugins (or other apps) can call it prior to running any commands
 * through this API and thus achieve async-safeness throughout the API. */
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


static int runcmd_setenv(const char *name, const char *value);
int update_environment(char *name, char *value, int set);

/* Start running a command */
int runcmd_open(const char *cmd, int *pfd, int *pfderr, char **env,
		void (*iobreg)(int, int, void *), void *iobregarg)
{
	char **argv = NULL;
	int argc = 0;
	int cmd2strv_errors;
	size_t cmdlen;
	pid_t pid;

	int i = 0;

	if(!pids)
		runcmd_init();

	/* We can't do anything without a command, or FD arrays. */
	if (!cmd || !*cmd || !pfd || !pfderr)
		return RUNCMD_EINVAL;

	cmdlen = strlen(cmd);
	argv = calloc((cmdlen / 2) + 5, sizeof(char *));
	if (!argv)
		return RUNCMD_EALLOC;

	cmd2strv_errors = runcmd_cmd2strv(cmd, &argc, argv);

	if (cmd2strv_errors == RUNCMD_EALLOC) {
		/* We couldn't allocate the parsed argument array. */
		free(argv);
		return RUNCMD_EALLOC;
	}

	if (cmd2strv_errors) {
		/* Run complex commands via the shell. */
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
		free(!cmd2strv_errors ? argv[0] : argv[2]);
		free(argv);
		return RUNCMD_EFD;
	}
	if (pipe(pfderr) < 0) {
		free(!cmd2strv_errors ? argv[0] : argv[2]);
		free(argv);
		close(pfd[0]);
		close(pfd[1]);
		return RUNCMD_EFD;
	}

	if (iobreg) iobreg(pfd[0], pfderr[0], iobregarg);

	pid = fork();
	if (pid < 0) {
		free(!cmd2strv_errors ? argv[0] : argv[2]);
		free(argv);
		close(pfd[0]);
		close(pfd[1]);
		close(pfderr[0]);
		close(pfderr[1]);
		return RUNCMD_EFORK; /* errno set by the failing function */
	}

	/* Child runs excevp() and _exit(). */
	if (pid == 0) {
		int exit_status = EXIT_SUCCESS; /* To preserve errno when _exit()ing. */

		/* Make our children their own process group leaders so they are killable
		 * by their parent (word of the day: filicide / prolicide). */
		if (setpgid(getpid(), getpid()) == -1) {
			exit_status = errno;
			fprintf(stderr, "setpgid(...) errno %d: %s\n", errno, strerror(errno));
			goto child_error_exit;
		}

		close (pfd[0]);
		if (pfd[1] != STDOUT_FILENO) {
			if (dup2(pfd[1], STDOUT_FILENO) == -1) {
				exit_status = errno;
				fprintf(stderr, "dup2(pfd[1], STDOUT_FILENO) errno %d: %s\n", errno, strerror(errno));
				goto child_error_exit;
			}
			close(pfd[1]);
		}

		close (pfderr[0]);
		if (pfderr[1] != STDERR_FILENO) {
			if (dup2(pfderr[1], STDERR_FILENO) == -1) {
				exit_status = errno;
				fprintf(stderr, "dup2(pfderr[1], STDERR_FILENO) errno %d: %s\n", errno, strerror(errno));
				goto child_error_exit;
			}
			close(pfderr[1]);
		}

		/* Close all descriptors in pids[], the child shouldn't see these. */
		for (i = 0; i < maxfd; i++) {
			if (pids[i] > 0)
				close(i);
		}

		/* Export the environment. */
		if (env) {
			for (; env[0] && env[1]; env += 2) {
				if (runcmd_setenv(env[0], env[1]) == -1) {
					exit_status = errno;
					fprintf(stderr, "runcmd_setenv(%s, ...) errno %d: %s\n",
						env[0], errno, strerror(errno)
					);
					goto child_error_exit;
				}
			}
		}

		/* Add VAR=value arguments from simple commands to the environment. */
		i = 0;
		if (!cmd2strv_errors) {
			char *ev;
			for (; i < argc && (ev = strchr(argv[i], '=')); ++i) {
				if (*ev) *ev++ = '\0';
				if (runcmd_setenv(argv[i], ev) == -1) {
					exit_status = errno;
					fprintf(stderr, "runcmd_setenv(%s, ev) errno %d: %s\n",
						argv[i], errno, strerror(errno)
					);
					goto child_error_exit;
				}
			}
			if (i == argc) {
				exit_status = EXIT_FAILURE;
				fprintf(stderr, "No command after variables.\n");
				goto child_error_exit;
			}
		}

		execvp(argv[i], argv + i);

		exit_status = errno;
		fprintf(stderr, "execvp(%s, ...) failed. errno is %d: %s\n", argv[i], errno, strerror(errno));

child_error_exit:
		/* Free argv memory before exiting so valgrind doesn't see it as a leak. */
		free(!cmd2strv_errors ? argv[0] : argv[2]);
		free(argv);
		_exit(exit_status);
	}

	/* parent picks up execution here */
	/*
	 * close childs file descriptors in our address space and
	 * release the memory we used that won't get passed to the
	 * caller.
	 */
	close(pfd[1]);
	close(pfderr[1]);
	free(!cmd2strv_errors ? argv[0] : argv[2]);
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

/**
 * Sets an environment variable.
 * This is for runcmd_open() to set the child environment.
 * @param naem Variable name to set.
 * @param value Value to set.
 * @return 0 on success, -1 on error with errno set to: EINVAL if name is NULL;
 * or the value set by setenv() or asprintf()/putenv().
 * @note If setenv() is unavailable (e.g. Solaris), a "name=vale" string is
 * allocated to pass to putenv(), which is retained by the environment. This
 * 'leaked' memory will be on the heap at program exit.
 */
static int runcmd_setenv(const char *name, const char *value) {
#ifndef HAVE_SETENV
	char *env_string = NULL;
#endif

	/* We won't mess with null variable names or values. */
	if (!name || !value) {
		errno = EINVAL;
		return -1;
	}

	errno = 0;
#ifdef HAVE_SETENV
	return setenv(name, value, 1);
#else
	/* For Solaris and systems that don't have setenv().
	 * This will leak memory, but in a "controlled" way, since the memory
	 * should be freed when the child process exits. */
	if (asprintf(&env_string, "%s=%s", name, value) == -1) return -1;
	if (!env_string) {
		errno = ENOMEM;
		return -1;
	}
	return putenv(env_string);
#endif
}

/* sets or unsets an environment variable */
int update_environment(char *name, char *value, int set) {
#ifndef HAVE_SETENV
	char *env_string = NULL;
#endif

	/* we won't mess with null variable names */
	if(name == NULL) return -1;

	/* set the environment variable */
	if(set == 1) {

#ifdef HAVE_SETENV
		setenv(name, (value == NULL) ? "" : value, 1);
#else
		/* needed for Solaris and systems that don't have setenv() */
		/* this will leak memory, but in a "controlled" way, since lost memory should be freed when the child process exits */
		asprintf(&env_string, "%s=%s", name, (value == NULL) ? "" : value);
		if(env_string) putenv(env_string);
#endif
	}
	/* clear the variable */
	else {
#ifdef HAVE_UNSETENV
		unsetenv(name);
#endif
	}

	return 0;
}

/**
 * This will free pids if non-null
 * Useful for external applications that rely on libnagios to
 * keep a lid on potential memory leaks
 */
void runcmd_free_pids(void) {
	if (pids)
		free(pids);
}

