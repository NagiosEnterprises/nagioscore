/******************************************************************************
 *
 * worker-ping.c - Nagios Core 4 worker to handle ping checke
 *
 * Program: Nagios Core
 * License: GPL
 *
 * First Written:   01-03-2013 (start of development)
 *
 * Description:
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/

#include "config.h"
#include "workers.h"

/* Local macro definitions */
#define PING_WORKER_VERSION "0.2"
#define PING_WORKER_MODIFICATION_DATE "02-Feb-13"

#define PW_OK		0
#define PW_ERROR	1

/* Local function declarations */
int			daemon_init( void);
int			drop_privileges( char *, char *);
void		print_license( void);
void		print_usage( char *);
void		print_version( void);
void		parse_worker_command_line( int, char **, char **, int *, char **,
					char **, char **);
static int	worker( const char *);

/* Everything starts here */
main( int argc, char **argv, char **env) {
	int daemon_mode = FALSE;
	char *worker_socket;
	char *worker_user = ( char *)0;
	char *worker_group = ( char *)0;

	if( FALSE == daemon_mode) {
		printf( "Greetings from the ping worker.\n");
	}

	parse_worker_command_line( argc, argv, env, &daemon_mode, &worker_socket,
			&worker_user, &worker_group);

	if( FALSE == daemon_mode) {
		print_version();
		printf( "Worker socket is %s.\n", worker_socket);
	}

	/* drop privileges */
	if( drop_privileges( worker_user, worker_group) == PW_ERROR) {
		fprintf( stderr, "Failed to drop privileges. Aborting.\n");
		exit( 1);
	}

	if( TRUE == daemon_mode) {
		if( daemon_init() == ERROR) {
			fprintf( stderr,
					"Bailing out due to failure to daemonize. (PID=%d)\n",
					( int)getpid());
			exit( 1);
		}
	}

	/* Enter the worker code */
	if( worker( worker_socket)) {
		exit( 1);
	}
}

void parse_worker_command_line( int argc, char **argv, char **env,
		int *daemon_mode, char **worker_socket, char **worker_user,
		char **worker_group) {
	int c = 0;
	int display_usage = FALSE;
	int display_license = FALSE;

#ifdef HAVE_GETOPT_H
	int option_index = 0;
	static struct option long_options[] = {
		{ "help", no_argument, 0, 'h'},
		{ "version", no_argument, 0, 'V'},
		{ "license", no_argument, 0, 'V'},
		{ "daemon", no_argument, 0, 'd'},
		{ "worker", required_argument, 0, 'W'},
		{ "user", required_argument, 0, 'u'},
		{ "group", required_argument, 0, 'g'},
		{ 0, 0, 0, 0}
	};
#define getopt( argc, argv, o) getopt_long( argc, argv, o, long_options, &option_index)
#endif

	/* get all command line arguments */
	while( 1) {
		c = getopt( argc, argv, "+:hVdW:u:g:");

		if( -1 == c || EOF == c) break;

		switch( c) {

			case '?': /* usage */
			case 'h':
				display_usage = TRUE;
				break;

			case 'V': /* version */
				display_license = TRUE;
				break;

			case 'd': /* daemon mode */
				*daemon_mode = TRUE;
				break;

			case 'W':
				*worker_socket = optarg;
				break;

			case 'u':
				*worker_user = optarg;
				break;

			case 'g':
				*worker_group = optarg;
				break;

			case ':':
				printf( "Missing argument for command line option '%c'.\n\n",
						optopt);
				print_usage( argv[ 0]);
				exit( 1);
				break;

			default:
				printf( "Unknown command line option '%c'.\n\n", c);
				print_usage( argv[ 0]);
				exit( 1);
				break;
		}
	}

	if( TRUE == display_license) {
		print_version();
		print_license();
		exit( 0);
	}

	if( TRUE == display_usage) {
		print_usage( argv[ 0]);
		exit( 0);
	}

}

void print_license( void) {
	printf( "\nThis program is free software; you can redistribute it and/or modify\n");
	printf( "it under the terms of the GNU General Public License version 2 as\n");
	printf( "published by the Free Software Foundation.\n\n");
	printf( "This program is distributed in the hope that it will be useful,\n");
	printf( "but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
	printf( "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
	printf( "GNU General Public License for more details.\n\n");
	printf( "You should have received a copy of the GNU General Public License\n");
	printf( "along with this program; if not, write to the Free Software\n");
	printf( "Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n\n");
}

void print_usage( char *program_name) {
	printf( "\nUsage: %s [-?|-h|--help] [-V|--version] [-d]\n", program_name);
	printf( "        -W /path/to/socket -u user -g group\n");
	printf( "\n");
	printf( "Options:\n");
	printf( "\n");
	printf( "  -d, --daemon                 Starts the ping worker in daemon mode,\n");
	printf( "                               instead of as a foreground process\n");
	printf( "  -W, --worker /path/to/socket Act as a worker for an already running daemon\n");
	printf( "  -u, --user user              Drop priviliges to the specified user\n");
	printf( "  -g, --group group            Drop priviliges to the specified user\n");
	printf( "\n");
	printf( "Visit the Nagios website at https://www.nagios.org/ for bug fixes, new\n");
	printf( "releases, online documentation, FAQs, information on subscribing to\n");
	printf( "the mailing lists, and commercial support options for Nagios.\n");
	printf( "\n");
}

void print_version( void) {
	printf( "\nNagios Core 4 Ping Worker %s\n", PING_WORKER_VERSION);
	printf( "Copyright (c) 2013-present Nagios Core Development Team\n");
	printf( "        and Community Contributors\n");
	printf( "Last Modified: %s\n", PING_WORKER_MODIFICATION_DATE);
	printf( "License: GPL\n");
	printf( "Website: https://www.nagios.org\n");
}

static int worker( const char *path)
{
	int sd, ret;
	char response[128];

	/*set_loadctl_defaults();*/

	sd = nsock_unix( path, NSOCK_TCP | NSOCK_CONNECT);
	if( sd < 0) {
		printf( "Failed to connect to query socket '%s': %s: %s\n",
				path, nsock_strerror( sd), strerror( errno));
		return 1;
	}

	ret = nsock_printf_nul( sd, "@wproc register name=Core Ping Worker %d;pid=%d;plugin=check_ping", getpid(), getpid());
	if( ret < 0) {
		printf( "Failed to register as worker.\n");
		return 1;
	}

	ret = read( sd, response, 3);
	if( ret != 3) {
		printf( "Failed to read response from wproc manager\n");
		return 1;
	}
	if( memcmp( response, "OK", 3)) {
		read( sd, response + 3, sizeof(response) - 4);
		response[ sizeof( response) - 2] = 0;
		printf( "Failed to register with wproc manager: %s\n", response);
		return 1;
	}

	enter_worker( sd, start_cmd);
	return 0;
}

int drop_privileges( char *user, char *group) {
	uid_t uid = -1;
	gid_t gid = -1;
	struct group *grp = NULL;
	struct passwd *pw = NULL;
	int result = PW_OK;

	/* only drop privileges if we're running as root, so we don't
		interfere with being debugged while running as some random user */
	if( getuid() != 0) {
		return PW_OK;
	}

	/* set effective group ID */
	if( NULL != group) {

		/* see if this is a group name */
		if( strspn( group, "0123456789") < strlen( group)) {
			grp = ( struct group *)getgrnam( group);
			if( NULL != grp) {
				gid = ( gid_t)(grp->gr_gid);
			}
			else {
				fprintf( stderr,
					"Warning: Could not get group entry for '%s'\n", group);
			}
		}

		/* else we were passed the GID */
		else {
			gid = ( gid_t)atoi( group);
		}

		/* set effective group ID if other than current EGID */
		if( gid != getegid()) {
			if( setgid( gid) == -1) {
				fprintf( stderr, "Warning: Could not set effective GID=%d\n",
						( int)gid);
				result = PW_ERROR;
			}
		}
	}

	/* set effective user ID */
	if( NULL != user) {

		/* see if this is a user name */
		if( strspn( user, "0123456789") < strlen( user)) {
			pw = ( struct passwd *)getpwnam( user);
			if( NULL != pw) {
				uid = ( uid_t)(pw->pw_uid);
			}
			else {
				fprintf( stderr,
						"Warning: Could not get passwd entry for '%s'\n", user);
			}
		}

		/* else we were passed the UID */
		else {
			uid = ( uid_t)atoi( user);
		}

#ifdef HAVE_INITGROUPS

		if( uid != geteuid()) {

			/* initialize supplementary groups */
			if( initgroups( user, gid) == -1) {
				if( EPERM == errno) {
					fprintf( stderr, "Warning: Unable to change supplementary "
							"groups using initgroups() -- I hope you know what "
							"you're doing\n");
				}
				else {
					fprintf( stderr, "Warning: Possibly root user failed "
							"dropping privileges with initgroups()\n");
					return PW_ERROR;
				}
			}
		}
#endif
		if( setuid( uid) == -1) {
			fprintf( stderr, "Warning: Could not set effective UID=%d\n",
					( int)uid);
			result = PW_ERROR;
		}
	}

	return result;
}

int daemon_init( void) {
	pid_t pid = -1;
	int pidno = 0;
	int lockfile = 0;
	int val = 0;
	char buf[256];
	struct flock lock;
	char *homedir = NULL;

#ifdef RLIMIT_CORE
	struct rlimit limit;
#endif

	/* change working directory. scuttle home if we're dumping core */
	homedir = getenv( "HOME");
#ifdef DAEMON_DUMPS_CORE
	if( NULL != homedir) {
		chdir( homedir);
	}
	else {
#endif
		chdir( "/");
#ifdef DAEMON_DUMPS_CORE
	}
#endif

	umask( S_IWGRP | S_IWOTH);

	/* close existing stdin, stdout, stderr */
	close( 0);
	close( 1);
	close( 2);

	/* THIS HAS TO BE DONE TO AVOID PROBLEMS WITH STDERR BEING REDIRECTED TO SERVICE MESSAGE PIPE! */
	/* re-open stdin, stdout, stderr with known values */
	open( "/dev/null", O_RDONLY);
	open( "/dev/null", O_WRONLY);
	open( "/dev/null", O_WRONLY);

#ifdef USE_LOCKFILE
	lockfile = open( lock_file,
			O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);

	if( lockfile < 0) {
		logit( NSLOG_RUNTIME_ERROR, TRUE, "Failed to obtain lock on file %s: %s\n", lock_file, strerror(errno));
		logit( NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR, TRUE, "Bailing out due to errors encountered while attempting to daemonize... (PID=%d)", (int)getpid());

		exit( 1);
	}

	/* see if we can read the contents of the lockfile */
	if(( val = read( lockfile, buf, ( size_t)10)) < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Lockfile exists but cannot be read");
		exit( 1);
	}

	/* we read something - check the PID */
	if( val > 0) {
		if(( val = sscanf( buf, "%d", &pidno)) < 1) {
			logit( NSLOG_RUNTIME_ERROR, TRUE, "Lockfile '%s' does not contain a valid PID (%s)", lock_file, buf);
			exit( 1);
		}
	}

	/* check for SIGHUP */
	if( val == 1 && ( pid = ( pid_t)pidno) == getpid()) {
		close( lockfile);
		return OK;
	}
#endif

	/* exit on errors... */
	if(( pid = fork()) < 0) {
		return( PW_ERROR);
	}

	/* parent process goes away.. */
	else if((int)pid != 0) {
		exit( 0);
	}

	/* child continues... */

	/* child becomes session leader... */
	setsid();

#ifdef USE_LOCKFILE
	/* place a file lock on the lock file */
	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;
	if( fcntl( lockfile, F_SETLK, &lock) < 0) {
		if( EACCES == errno || EAGAIN == errno) {
			fcntl( lockfile, F_GETLK, &lock);
			logit( NSLOG_RUNTIME_ERROR, TRUE, "Lockfile '%s' looks like its already held by another instance of Nagios (PID %d).  Bailing out...", lock_file, (int)lock.l_pid);
			}
		else {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Cannot lock lockfile '%s': %s. Bailing out...", lock_file, strerror(errno));

		}
		exit( 1);
	}
#endif

	/* prevent daemon from dumping a core file... */
#if defined( RLIMIT_CORE) && defined( DAEMON_DUMPS_CORE)
	getrlimit( RLIMIT_CORE, &limit);
	limit.rlim_cur = 0;
	setrlimit( RLIMIT_CORE, &limit);
#endif

#ifdef USE_LOCKFILE
	/* write PID to lockfile... */
	lseek( lockfile, 0, SEEK_SET);
	ftruncate( lockfile, 0);
	sprintf( buf, "%d\n", ( int)getpid());
	write( lockfile, buf, strlen( buf));

	/* make sure lock file stays open while program is executing... */
	val = fcntl( lockfile, F_GETFD, 0);
	val |= FD_CLOEXEC;
	fcntl( lockfile, F_SETFD, val);
#endif

	return PW_OK;
}
