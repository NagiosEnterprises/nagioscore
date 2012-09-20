/*****************************************************************************
 *
 * NAGIOS.C - Core Program Code For Nagios
 *
 * Program: Nagios Core
 * License: GPL
 *
 * First Written:   01-28-1999 (start of development)
 *
 * Description:
 *
 * Nagios is a network monitoring tool that will check hosts and services
 * that you specify.  It has the ability to notify contacts via email, pager,
 * or other user-defined methods when a service or host goes down and
 * recovers.  Service and host monitoring is done through the use of external
 * plugins which can be developed independently of Nagios.
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

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/comments.h"
#include "../include/downtime.h"
#include "../include/statusdata.h"
#include "../include/macros.h"
#include "../include/nagios.h"
#include "../include/sretention.h"
#include "../include/perfdata.h"
#include "../include/broker.h"
#include "../include/nebmods.h"
#include "../include/nebmodules.h"
#include "../include/workers.h"

/*#define DEBUG_MEMORY 1*/
#ifdef DEBUG_MEMORY
#include <mcheck.h>
#endif


int main(int argc, char **argv, char **env) {
	int result;
	int error = FALSE;
	char *buffer = NULL;
	int display_license = FALSE;
	int display_help = FALSE;
	int c = 0;
	struct tm *tm, tm_s;
	time_t now;
	char datestring[256];
	nagios_macros *mac;

#ifdef HAVE_GETOPT_H
	int option_index = 0;
	static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"version", no_argument, 0, 'V'},
			{"license", no_argument, 0, 'V'},
			{"verify-config", no_argument, 0, 'v'},
			{"daemon", no_argument, 0, 'd'},
			{"test-scheduling", no_argument, 0, 's'},
			{"precache-objects", no_argument, 0, 'p'},
			{"use-precached-objects", no_argument, 0, 'u'},
			{"enable-timing-point", no_argument, 0, 'T'},
			{0, 0, 0, 0}
		};
#define getopt(argc, argv, o) getopt_long(argc, argv, o, long_options, &option_index)
#endif

	mac = get_global_macros();

	/* make sure we have the correct number of command line arguments */
	if(argc < 2)
		error = TRUE;

	/* get all command line arguments */
	while(1) {
		c = getopt(argc, argv, "+hVvdspuxT");

		if(c == -1 || c == EOF)
			break;

		switch(c) {

			case '?': /* usage */
			case 'h':
				display_help = TRUE;
				break;

			case 'V': /* version */
				display_license = TRUE;
				break;

			case 'v': /* verify */
				verify_config++;
				break;

			case 's': /* scheduling check */
				test_scheduling = TRUE;
				break;

			case 'd': /* daemon mode */
				daemon_mode = TRUE;
				break;

			case 'p': /* precache object config */
				precache_objects = TRUE;
				break;

			case 'u': /* use precached object config */
				use_precached_objects = TRUE;
				break;
			case 'T':
				enable_timing_point = TRUE;
				break;

			case 'x':
				printf("Warning: -x is deprecated and will be removed\n");
				break;

			default:
				break;
			}

		}

#ifdef DEBUG_MEMORY
	mtrace();
#endif

	if(daemon_mode == FALSE) {
		printf("\nNagios Core %s\n", PROGRAM_VERSION);
		printf("Copyright (c) 2009-2011 Nagios Core Development Team and Community Contributors\n");
		printf("Copyright (c) 1999-2009 Ethan Galstad\n");
		printf("Last Modified: %s\n", PROGRAM_MODIFICATION_DATE);
		printf("License: GPL\n\n");
		printf("Website: http://www.nagios.org\n");
		}

	/* just display the license */
	if(display_license == TRUE) {

		printf("This program is free software; you can redistribute it and/or modify\n");
		printf("it under the terms of the GNU General Public License version 2 as\n");
		printf("published by the Free Software Foundation.\n\n");
		printf("This program is distributed in the hope that it will be useful,\n");
		printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
		printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
		printf("GNU General Public License for more details.\n\n");
		printf("You should have received a copy of the GNU General Public License\n");
		printf("along with this program; if not, write to the Free Software\n");
		printf("Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n\n");

		exit(OK);
		}

	/* make sure we got the main config file on the command line... */
	if(optind >= argc)
		error = TRUE;

	/* if there are no command line options (or if we encountered an error), print usage */
	if(error == TRUE || display_help == TRUE) {

		printf("Usage: %s [options] <main_config_file>\n", argv[0]);
		printf("\n");
		printf("Options:\n");
		printf("\n");
		printf("  -v, --verify-config          Verify all configuration data (-v -v for more info)\n");
		printf("  -s, --test-scheduling        Shows projected/recommended check scheduling and other\n");
		printf("                               diagnostic info based on the current configuration files.\n");
		printf("  -T, --enable-timing-point    Enable timed commentary on initialization\n");
		/*printf("  -o, --dont-verify-objects    Don't verify object relationships - USE WITH CAUTION!\n");*/
		printf("  -x, --dont-verify-paths      Don't check for circular object paths - USE WITH CAUTION!\n");
		printf("  -p, --precache-objects       Precache object configuration\n");
		printf("  -u, --use-precached-objects  Use precached object config file\n");
		printf("  -d, --daemon                 Starts Nagios in daemon mode, instead of as a foreground process\n");
		printf("\n");
		printf("Visit the Nagios website at http://www.nagios.org/ for bug fixes, new\n");
		printf("releases, online documentation, FAQs, information on subscribing to\n");
		printf("the mailing lists, and commercial support options for Nagios.\n");
		printf("\n");

		exit(ERROR);
		}


	/* config file is last argument specified */
	config_file = (char *)strdup(argv[optind]);
	if(config_file == NULL) {
		printf("Error allocating memory.\n");
		exit(ERROR);
		}

	/* make sure the config file uses an absolute path */
	if(config_file[0] != '/') {

		/* save the name of the config file */
		buffer = (char *)strdup(config_file);

		/* reallocate a larger chunk of memory */
		config_file = (char *)realloc(config_file, MAX_FILENAME_LENGTH);
		if(config_file == NULL) {
			printf("Error allocating memory.\n");
			exit(ERROR);
			}

		/* get absolute path of current working directory */
		getcwd(config_file, MAX_FILENAME_LENGTH);

		/* append a forward slash */
		strncat(config_file, "/", 1);
		config_file[MAX_FILENAME_LENGTH - 1] = '\x0';

		/* append the config file to the path */
		strncat(config_file, buffer, MAX_FILENAME_LENGTH - strlen(config_file) - 1);
		config_file[MAX_FILENAME_LENGTH - 1] = '\x0';

		my_free(buffer);
		}

	/*
	 * let's go to town. We'll be noisy if we're verifying config
	 * or running scheduling tests.
	 */
	if(verify_config || test_scheduling || precache_objects) {
		reset_variables();

		if(verify_config)
			printf("Reading configuration data...\n");

		/* read our config file */
		result = read_main_config_file(config_file);
		if(result != OK) {
			printf("   Error processing main config file!\n\n");
			exit(EXIT_FAILURE);
			}

		if(verify_config)
			printf("   Read main config file okay...\n");

		/* drop privileges */
		if((result = drop_privileges(nagios_user, nagios_group)) == ERROR) {
			printf("   Failed to drop privileges.  Aborting.");
			exit(EXIT_FAILURE);
			}

		/* read object config files */
		result = read_all_object_data(config_file);
		if(result != OK) {
			printf("   Error processing object config files!\n\n");
			/* if the config filename looks fishy, warn the user */
			if(!strstr(config_file, "nagios.cfg")) {
				printf("\n***> The name of the main configuration file looks suspicious...\n");
				printf("\n");
				printf("     Make sure you are specifying the name of the MAIN configuration file on\n");
				printf("     the command line and not the name of another configuration file.  The\n");
				printf("     main configuration file is typically '/usr/local/nagios/etc/nagios.cfg'\n");
				}

			printf("\n***> One or more problems was encountered while processing the config files...\n");
			printf("\n");
			printf("     Check your configuration file(s) to ensure that they contain valid\n");
			printf("     directives and data defintions.  If you are upgrading from a previous\n");
			printf("     version of Nagios, you should be aware that some variables/definitions\n");
			printf("     may have been removed or modified in this version.  Make sure to read\n");
			printf("     the HTML documentation regarding the config files, as well as the\n");
			printf("     'Whats New' section to find out what has changed.\n\n");
			exit(EXIT_FAILURE);
			}

		if(verify_config) {
			printf("   Read object config files okay...\n\n");
			printf("Running pre-flight check on configuration data...\n\n");
			}

		/* run the pre-flight check to make sure things look okay... */
		result = pre_flight_check();

		if(result != OK) {
			printf("\n***> One or more problems was encountered while running the pre-flight check...\n");
			printf("\n");
			printf("     Check your configuration file(s) to ensure that they contain valid\n");
			printf("     directives and data defintions.  If you are upgrading from a previous\n");
			printf("     version of Nagios, you should be aware that some variables/definitions\n");
			printf("     may have been removed or modified in this version.  Make sure to read\n");
			printf("     the HTML documentation regarding the config files, as well as the\n");
			printf("     'Whats New' section to find out what has changed.\n\n");
			exit(EXIT_FAILURE);
			}

		if(verify_config) {
			printf("\nThings look okay - No serious problems were detected during the pre-flight check\n");
			}

		/* scheduling tests need a bit more than config verifications */
		if(test_scheduling == TRUE) {

			/* we'll need the event queue here so we can time insertions */
			init_event_queue();
			timing_point("Done initializing event queue\n");

			/* read initial service and host state information */
			initialize_retention_data(config_file);
			read_initial_state_information();
			timing_point("Retention data and initial state parsed\n");

			/* initialize the event timing loop */
			init_timing_loop();
			timing_point("Timing loop initialized\n");

			/* display scheduling information */
			display_scheduling_info();
			}

		if(precache_objects) {
			result = fcache_objects(object_precache_file);
			timing_point("Done precaching objects\n");
			if(result == OK) {
				printf("Object precache file created:\n%s\n", object_precache_file);
				}
			else {
				printf("Failed to precache objects to '%s': %s\n", object_precache_file, strerror(errno));
				}
			}

		/* clean up after ourselves */
		cleanup();

		/* exit */
		timing_point("Exiting\n");
		exit(result);
		}


	/* else start to monitor things... */
	else {

		nagios_iobs = iobroker_create();

		/* keep monitoring things until we get a shutdown command */
		do {

			/* reset program variables */
			reset_variables();

			/* get PID */
			nagios_pid = (int)getpid();

			/* read in the configuration files (main and resource config files) */
			result = read_main_config_file(config_file);

			/* NOTE 11/06/07 EG moved to after we read config files, as user may have overridden timezone offset */
			/* get program (re)start time and save as macro */
			program_start = time(NULL);
			my_free(mac->x[MACRO_PROCESSSTARTTIME]);
			asprintf(&mac->x[MACRO_PROCESSSTARTTIME], "%lu", (unsigned long)program_start);

			/* open debug log */
			open_debug_log();

			/* drop privileges */
			if(drop_privileges(nagios_user, nagios_group) == ERROR) {

				logit(NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR | NSLOG_CONFIG_ERROR, TRUE, "Failed to drop privileges.  Aborting.");

				cleanup();
				exit(ERROR);
				}

#ifdef USE_EVENT_BROKER
			/* initialize modules */
			neb_init_modules();
			neb_init_callback_list();
#endif

			/* this must be logged after we read config data, as user may have changed location of main log file */
			logit(NSLOG_PROCESS_INFO, TRUE, "Nagios %s starting... (PID=%d)\n", PROGRAM_VERSION, (int)getpid());

			/* log the local time - may be different than clock time due to timezone offset */
			now = time(NULL);
			tm = localtime_r(&now, &tm_s);
			strftime(datestring, sizeof(datestring), "%a %b %d %H:%M:%S %Z %Y", tm);
			logit(NSLOG_PROCESS_INFO, TRUE, "Local time is %s", datestring);

			/* write log version/info */
			write_log_file_info(NULL);

			/*
			 * Initialize query handler and event subscription service.
			 * This must be done before modules are initialized, so
			 * the modules can use our in-core stuff properly
			 */
			qh_init(qh_socket_path);
			nerd_init();

#ifdef USE_EVENT_BROKER
			/* load modules */
			neb_load_all_modules();

			/* send program data to broker */
			broker_program_state(NEBTYPE_PROCESS_PRELAUNCH, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

			/* read in all object config data */
			if(result == OK)
				result = read_all_object_data(config_file);

			/* there was a problem reading the config files */
			if(result != OK)
				logit(NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR | NSLOG_CONFIG_ERROR, TRUE, "Bailing out due to one or more errors encountered in the configuration files. Run Nagios from the command line with the -v option to verify your config before restarting. (PID=%d)", (int)getpid());

			else {

				/* run the pre-flight check to make sure everything looks okay*/
				if((result = pre_flight_check()) != OK)
					logit(NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR | NSLOG_VERIFICATION_ERROR, TRUE, "Bailing out due to errors encountered while running the pre-flight check.  Run Nagios from the command line with the -v option to verify your config before restarting. (PID=%d)\n", (int)getpid());
				}

			/* an error occurred that prevented us from (re)starting */
			if(result != OK) {

				/* if we were restarting, we need to cleanup from the previous run */
				if(sigrestart == TRUE) {

					/* clean up the status data */
					cleanup_status_data(config_file, TRUE);
					}

#ifdef USE_EVENT_BROKER
				/* send program data to broker */
				broker_program_state(NEBTYPE_PROCESS_SHUTDOWN, NEBFLAG_PROCESS_INITIATED, NEBATTR_SHUTDOWN_ABNORMAL, NULL);
#endif
				cleanup();
				exit(ERROR);
				}

			init_event_queue();

			/* write the objects.cache file */
			fcache_objects(object_cache_file);

			/* handle signals (interrupts) */
			setup_sighandler();


#ifdef USE_EVENT_BROKER
			/* send program data to broker */
			broker_program_state(NEBTYPE_PROCESS_START, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

			/* enter daemon mode (unless we're restarting...) */
			if(daemon_mode == TRUE && sigrestart == FALSE) {

				result = daemon_init();

				/* we had an error daemonizing, so bail... */
				if(result == ERROR) {
					logit(NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR, TRUE, "Bailing out due to failure to daemonize. (PID=%d)", (int)getpid());

#ifdef USE_EVENT_BROKER
					/* send program data to broker */
					broker_program_state(NEBTYPE_PROCESS_SHUTDOWN, NEBFLAG_PROCESS_INITIATED, NEBATTR_SHUTDOWN_ABNORMAL, NULL);
#endif
					cleanup();
					exit(ERROR);
					}

				asprintf(&buffer, "Finished daemonizing... (New PID=%d)\n", (int)getpid());
				write_to_all_logs(buffer, NSLOG_PROCESS_INFO);
				my_free(buffer);

				/* get new PID */
				nagios_pid = (int)getpid();
				}

			/* initialize status data unless we're starting */
			if(sigrestart == FALSE)
				initialize_status_data(config_file);

			/* read initial service and host state information  */
			initialize_retention_data(config_file);
			read_initial_state_information();

			/* initialize comment data */
			initialize_comment_data(config_file);

			/* initialize scheduled downtime data */
			initialize_downtime_data(config_file);

			/* initialize performance data */
			initialize_performance_data(config_file);

			/* initialize the event timing loop */
			init_timing_loop();

			/* initialize check statistics */
			init_check_stats();

			/* check for updates */
			check_for_nagios_updates(FALSE, TRUE);

			/* update all status data (with retained information) */
			update_all_status_data();

			/* log initial host and service state */
			log_host_states(INITIAL_STATES, NULL);
			log_service_states(INITIAL_STATES, NULL);

			/* reset the restart flag */
			sigrestart = FALSE;

			/* fire up command file worker */
			launch_command_file_worker();

			/* initialize check workers */
			init_workers(num_check_workers);

#ifdef USE_EVENT_BROKER
			/* send program data to broker */
			broker_program_state(NEBTYPE_PROCESS_EVENTLOOPSTART, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

			/* get event start time and save as macro */
			event_start = time(NULL);
			my_free(mac->x[MACRO_EVENTSTARTTIME]);
			asprintf(&mac->x[MACRO_EVENTSTARTTIME], "%lu", (unsigned long)event_start);

			/***** start monitoring all services *****/
			/* (doesn't return until a restart or shutdown signal is encountered) */
			event_execution_loop();

			/* 03/01/2007 EG Moved from sighandler() to prevent FUTEX locking problems under NPTL */
			/* 03/21/2007 EG SIGSEGV signals are still logged in sighandler() so we don't loose them */
			/* did we catch a signal? */
			if(caught_signal == TRUE) {

				if(sig_id == SIGHUP)
					logit(NSLOG_PROCESS_INFO, TRUE, "Caught SIGHUP, restarting...\n");

				}

#ifdef USE_EVENT_BROKER
			/* send program data to broker */
			broker_program_state(NEBTYPE_PROCESS_EVENTLOOPEND, NEBFLAG_NONE, NEBATTR_NONE, NULL);
			if(sigshutdown == TRUE)
				broker_program_state(NEBTYPE_PROCESS_SHUTDOWN, NEBFLAG_USER_INITIATED, NEBATTR_SHUTDOWN_NORMAL, NULL);
			else if(sigrestart == TRUE)
				broker_program_state(NEBTYPE_PROCESS_RESTART, NEBFLAG_USER_INITIATED, NEBATTR_RESTART_NORMAL, NULL);
#endif

			/* save service and host state information */
			save_state_information(FALSE);
			cleanup_retention_data(config_file);

			/* clean up performance data */
			cleanup_performance_data(config_file);

			/* clean up the scheduled downtime data */
			cleanup_downtime_data(config_file);

			/* clean up the status data unless we're restarting */
			if(sigrestart == FALSE) {
				cleanup_status_data(config_file, TRUE);
				}

			/* shutdown stuff... */
			if(sigshutdown == TRUE) {
				free_worker_memory(WPROC_FORCE);
				iobroker_destroy(nagios_iobs, IOBROKER_CLOSE_SOCKETS);

				/* make sure lock file has been removed - it may not have been if we received a shutdown command */
				if(daemon_mode == TRUE)
					unlink(lock_file);

				/* log a shutdown message */
				logit(NSLOG_PROCESS_INFO, TRUE, "Successfully shutdown... (PID=%d)\n", (int)getpid());
				}

			/* clean up after ourselves */
			cleanup();

			/* close debug log */
			close_debug_log();

			}
		while(sigrestart == TRUE && sigshutdown == FALSE);

		/* free misc memory */
		my_free(config_file);
		}

	return OK;
	}


