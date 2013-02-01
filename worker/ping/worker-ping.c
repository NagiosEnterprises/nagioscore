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
#define PING_WORKER_VERSION "0.1"
#define PING_WORKER_MODIFICATION_DATE "03-Jan-13"

/* Local function declarations */
void		print_license( void);
void		print_usage( char *);
void		print_version( void);
void		parse_worker_command_line( int, char **, char **, int *, char **);
static int	worker( const char *);

/* Everything starts here */
main( int argc, char **argv, char **env) {
	int daemon_mode = FALSE;
	char *worker_socket;

	printf( "Greetings from the ping worker.\n");

	parse_worker_command_line( argc, argv, env, &daemon_mode, &worker_socket);

	if( FALSE == daemon_mode) {
		print_version();
	}

	printf( "Worker socket is %s.\n", worker_socket);

	/* Enter the worker code */
	if( worker( worker_socket)) {
		exit( 1);
	}
}

void parse_worker_command_line( int argc, char **argv, char **env, 
		int *daemon_mode, char **worker_socket) {
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
		{ 0, 0, 0, 0}
	};
#define getopt( argc, argv, o) getopt_long( argc, argv, o, long_options, &option_index)
#endif

	/* get all command line arguments */
	while( 1) {
		c = getopt( argc, argv, "+:hVdW:");

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

	if( display_license == TRUE) {
		print_version();
		print_license();
		exit( 0);
	}

	if( display_usage == TRUE) {
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
	printf( "        -W /path/to/socket\n");
	printf( "\n");
	printf( "Options:\n");
	printf( "\n");
	printf( "  -d, --daemon                 Starts the ping worker in daemon mode,\n");
	printf( "                               instead of as a foreground process\n");
	printf( "  -W, --worker /path/to/socket Act as a worker for an already running daemon\n");
	printf( "\n");
	printf( "Visit the Nagios website at http://www.nagios.org/ for bug fixes, new\n");
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
	printf( "Website: http://www.nagios.org\n");
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
