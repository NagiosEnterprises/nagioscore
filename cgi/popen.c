/******************************************************************************
 * popen.c - Written by Karl DeBisschop
 *
 * A safe alternative to popen
 * 
 * Provides spopen and spclose
 *
 * Code taken with liitle modification from "Advanced Programming for the Unix
 * Environment" by W. Richard Stevens
 *
 * This is considered safe in that no shell is spawned, and the environment and
 * path passed to the exec'd program are esstially empty. (popen create a shell
 * and passes the environment to it).
 *
 ******************************************************************************/

#include "../common/common.h"
#include "../common/config.h"

#include <stdarg.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include "popen.h"

pid_t *childpid;
int *childerr;


/* 4.3BSD Reno <signal.h> doesn't define SIG_ERR */
#if defined(SIG_IGN) && !defined(SIG_ERR)
#define	SIG_ERR	((Sigfunc *)-1)
#endif

#define	min(a,b)	((a) < (b) ? (a) : (b))
#define	max(a,b)	((a) > (b) ? (a) : (b))
#define MAXLINE 2048
#define MAXARGS 50

int open_max(void);			/* {Prog openmax} */
void err_sys(const char *, ...);
char *rtrim(char *, const char *);

extern int *childerr; /* ptr to array allocated at run-time */
extern pid_t *childpid; /* ptr to array allocated at run-time */
static int maxfd; /* from our open_max(), {Prog openmax} */

FILE *
spopen(const char *cmdstring)
{
  char *environ[] = { "USER=unknown", "PATH=", NULL };
  char cmd[MAXLINE]; /* potential overflow */
  char *args[MAXARGS]; /* potential overflow */
  char *str;
  char c1[MAXARGS], c2[MAXARGS];

  int	i=0, pfd[2], pfderr[2], len;
  pid_t	pid;
  FILE *fp;

#ifdef RLIMIT_CORE
  struct rlimit limit;

  /* do not leave core files */
  getrlimit(RLIMIT_CORE,&limit);
  limit.rlim_cur = 0;
  setrlimit(RLIMIT_CORE,&limit);
#endif

  /* if no command was passed, return with no error */
  if(cmdstring==NULL || strlen(cmdstring) > MAXLINE-1)
    return(NULL);

  strncpy(cmd,cmdstring,(size_t) MAXLINE);

  /* This is not a shell, so we don't handle `???` or "???" */
  if((strstr(cmdstring,"`")) || (strstr(cmdstring,"\"")))
    return NULL;

  /* allow single quotes, but only if non-whitesapce doesn't occur on bith sides */
  if(sscanf(cmdstring,"%[^' ]'%[^' ]",c1,c2)==2)
    return NULL;

  /* get command to run */
  args[i]=strtok(cmd," ");
  if(args[0]==NULL)
    return(NULL);

  /* loop to get arguments to command */
  while ((args[++i]=strtok(NULL," "))) {
    if (i==MAXARGS-2 || args[i]==NULL) /* save space for NULL terminator */
      return(NULL);
      if ((strstr(args[i],"'"))) { /* SIMPLE quoted strings */
      str=1+strstr(args[i],"'");
      if((strstr(str,"'"))){
	str=rtrim(str,"'");
	sprintf(args[i],"%s",str);
      } else {
	sprintf(args[i],"%s",str);
	while(TRUE){
	  str=strtok(NULL," ");
	  sprintf(args[i],"%s %s",args[i],str);
	  if((strstr(str,"'"))){
	    len = strlen(args[i]);
	    sprintf(args[i]+len-1,"%s"," ");
	    break;
	  }
	}
      }
    }
  }
  args[++i]=NULL;

  if (childpid == NULL) {		/* first time through */
    maxfd = open_max(); /* allocate zeroed out array for child pids */
    if ( (childpid = calloc(maxfd, sizeof(pid_t))) == NULL)
      return(NULL);
  }

  if (childerr == NULL) {		/* first time through */
    maxfd = open_max(); /* allocate zeroed out array for child pids */
    if ( (childerr = calloc(maxfd, sizeof(int))) == NULL)
      return(NULL);
  }

  if (pipe(pfd) < 0)
    return(NULL);	/* errno set by pipe() */

  if (pipe(pfderr) < 0)
    return(NULL);	/* errno set by pipe() */

  if ( (pid = fork()) < 0)
    return(NULL);	/* errno set by fork() */
  else if (pid == 0) {						    /* child */
    close(pfd[0]);
    if (pfd[1] != STDOUT_FILENO) {
      dup2(pfd[1], STDOUT_FILENO);
      close(pfd[1]);
    }
    close(pfderr[0]);
    if (pfderr[1] != STDERR_FILENO) {
      dup2(pfderr[1], STDERR_FILENO);
      close(pfderr[1]);
    }
    /* close all descriptors in childpid[] */
    for (i = 0; i < maxfd; i++)
      if (childpid[i] > 0)
	close(i);

    execve(args[0],args,environ);
    _exit(0);
  }

  close(pfd[1]);                                                  /* parent */
  if ( (fp = fdopen(pfd[0], "r")) == NULL)
    return(NULL);
  close(pfderr[1]);

  childpid[fileno(fp)] = pid;	/* remember child pid for this fd */
  childerr[fileno(fp)] = pfderr[0];	/* remember STDERR fd for this fd */
  return(fp);
}

int
spclose(FILE *fp)
{
  int fd, status;
  pid_t	pid;

  if (childpid == NULL)
    return(-1);		/* popen() has never been called */

  fd = fileno(fp);
  if ( (pid = childpid[fd]) == 0)
    return(-1);		/* fp wasn't opened by popen() */

  childpid[fd] = 0;
  if (fclose(fp) == EOF)
    return(-1);

  while (waitpid(pid, &status, 0) < 0)
    if (errno != EINTR)
      return(-1);	/* error other than EINTR from waitpid() */

  return(status);	/* return child's termination status */
}

#ifdef	OPEN_MAX
static int	openmax = OPEN_MAX;
#else
static int	openmax = 0;
#endif

#define	OPEN_MAX_GUESS	256	/* if OPEN_MAX is indeterminate */
				/* no guarantee this is adequate */
int
open_max(void)
{
  if (openmax == 0) {		/* first time through */
    errno = 0;
    if ( (openmax = sysconf(_SC_OPEN_MAX)) < 0) {
      if (errno == 0)
	openmax = OPEN_MAX_GUESS;	/* it's indeterminate */
      else
	err_sys("sysconf error for _SC_OPEN_MAX");
    }
  }
  return(openmax);
}


static void err_doit(int, const char *, va_list);

char	*pname = NULL;		/* caller can set this from argv[0] */

/* Fatal error related to a system call.
 * Print a message and terminate. */

void
err_sys(const char *fmt, ...)
{
  va_list		ap;

  va_start(ap, fmt);
  err_doit(1, fmt, ap);
  va_end(ap);
  exit(1);
}

/* Print a message and return to caller.
 * Caller specifies "errnoflag". */

static void
err_doit(int errnoflag, const char *fmt, va_list ap)
{
  int		errno_save;
  char	buf[MAXLINE];

  errno_save = errno;		/* value caller might want printed */
  vsprintf(buf, fmt, ap);
  if (errnoflag)
    sprintf(buf+strlen(buf), ": %s", strerror(errno_save));
  strcat(buf, "\n");
  fflush(stdout);		/* in case stdout and stderr are the same */
  fputs(buf, stderr);
  fflush(NULL);		/* flushes all stdio output streams */
  return;
}

char *rtrim(char *str, const char *tok)
{
  int i=0;

  while(str!=NULL){
    if(*(str+i)==*tok){
      sprintf(str+i,"%s","\0");
      return str;
    }
    i++;
  }
  return str;  
}
