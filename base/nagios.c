/*****************************************************************************
 *
 * NAGIOS.C - Core Program Code For Nagios
 *
 * Program: Nagios Core
 * Version: 3.5.0
 * License: GPL
 * Copyright (c) 2009-2010 Nagios Core Development Team and Community Contributors
 * Copyright (c) 1999-2009 Ethan Galstad
 *
 * First Written:   01-28-1999 (start of development)
 * Last Modified:
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

/*#define DEBUG_MEMORY 1*/
#ifdef DEBUG_MEMORY
#include <mcheck.h>
#endif


char		*config_file = NULL;
char		*log_file = NULL;
char            *command_file = NULL;
char            *temp_file = NULL;
char            *temp_path = NULL;
char            *check_result_path = NULL;
char            *lock_file = NULL;
char            *log_archive_path = NULL;
char            *p1_file = NULL;  /**** EMBEDDED PERL ****/
char            *auth_file = NULL; /**** EMBEDDED PERL INTERPRETER AUTH FILE ****/
char            *nagios_user = NULL;
char            *nagios_group = NULL;

char            *global_host_event_handler = NULL;
char            *global_service_event_handler = NULL;
command         *global_host_event_handler_ptr = NULL;
command         *global_service_event_handler_ptr = NULL;

char            *ocsp_command = NULL;
char            *ochp_command = NULL;
command         *ocsp_command_ptr = NULL;
command         *ochp_command_ptr = NULL;

char            *illegal_object_chars = NULL;
char            *illegal_output_chars = NULL;

int             use_regexp_matches = FALSE;
int             use_true_regexp_matching = FALSE;

int		use_syslog = DEFAULT_USE_SYSLOG;
int             log_notifications = DEFAULT_NOTIFICATION_LOGGING;
int             log_service_retries = DEFAULT_LOG_SERVICE_RETRIES;
int             log_host_retries = DEFAULT_LOG_HOST_RETRIES;
int             log_event_handlers = DEFAULT_LOG_EVENT_HANDLERS;
int             log_initial_states = DEFAULT_LOG_INITIAL_STATES;
int             log_external_commands = DEFAULT_LOG_EXTERNAL_COMMANDS;
int             log_passive_checks = DEFAULT_LOG_PASSIVE_CHECKS;

unsigned long   logging_options = 0;
unsigned long   syslog_options = 0;

int             service_check_timeout = DEFAULT_SERVICE_CHECK_TIMEOUT;
int             service_check_timeout_state = STATE_CRITICAL;
int             host_check_timeout = DEFAULT_HOST_CHECK_TIMEOUT;
int             event_handler_timeout = DEFAULT_EVENT_HANDLER_TIMEOUT;
int             notification_timeout = DEFAULT_NOTIFICATION_TIMEOUT;
int             ocsp_timeout = DEFAULT_OCSP_TIMEOUT;
int             ochp_timeout = DEFAULT_OCHP_TIMEOUT;

double          sleep_time = DEFAULT_SLEEP_TIME;
int             interval_length = DEFAULT_INTERVAL_LENGTH;
int             service_inter_check_delay_method = ICD_SMART;
int             host_inter_check_delay_method = ICD_SMART;
int             service_interleave_factor_method = ILF_SMART;
int             max_host_check_spread = DEFAULT_HOST_CHECK_SPREAD;
int             max_service_check_spread = DEFAULT_SERVICE_CHECK_SPREAD;

int             command_check_interval = DEFAULT_COMMAND_CHECK_INTERVAL;
int             check_reaper_interval = DEFAULT_CHECK_REAPER_INTERVAL;
int             max_check_reaper_time = DEFAULT_MAX_REAPER_TIME;
int             service_freshness_check_interval = DEFAULT_FRESHNESS_CHECK_INTERVAL;
int             host_freshness_check_interval = DEFAULT_FRESHNESS_CHECK_INTERVAL;
int             auto_rescheduling_interval = DEFAULT_AUTO_RESCHEDULING_INTERVAL;

int             check_external_commands = DEFAULT_CHECK_EXTERNAL_COMMANDS;
int             check_orphaned_services = DEFAULT_CHECK_ORPHANED_SERVICES;
int             check_orphaned_hosts = DEFAULT_CHECK_ORPHANED_HOSTS;
int             check_service_freshness = DEFAULT_CHECK_SERVICE_FRESHNESS;
int             check_host_freshness = DEFAULT_CHECK_HOST_FRESHNESS;
int             auto_reschedule_checks = DEFAULT_AUTO_RESCHEDULE_CHECKS;
int             auto_rescheduling_window = DEFAULT_AUTO_RESCHEDULING_WINDOW;

int             additional_freshness_latency = DEFAULT_ADDITIONAL_FRESHNESS_LATENCY;

int             check_for_updates = DEFAULT_CHECK_FOR_UPDATES;
int             bare_update_check = DEFAULT_BARE_UPDATE_CHECK;
time_t          last_update_check = 0L;
unsigned long   update_uid = 0L;
int             update_available = FALSE;
char            *last_program_version = NULL;
char            *new_program_version = NULL;

time_t          last_command_check = 0L;
time_t          last_command_status_update = 0L;
time_t          last_log_rotation = 0L;
time_t          last_program_stop = 0L;

int             use_aggressive_host_checking = DEFAULT_AGGRESSIVE_HOST_CHECKING;
unsigned long   cached_host_check_horizon = DEFAULT_CACHED_HOST_CHECK_HORIZON;
unsigned long   cached_service_check_horizon = DEFAULT_CACHED_SERVICE_CHECK_HORIZON;
int             enable_predictive_host_dependency_checks = DEFAULT_ENABLE_PREDICTIVE_HOST_DEPENDENCY_CHECKS;
int             enable_predictive_service_dependency_checks = DEFAULT_ENABLE_PREDICTIVE_SERVICE_DEPENDENCY_CHECKS;

int             soft_state_dependencies = FALSE;

int             retain_state_information = FALSE;
int             retention_update_interval = DEFAULT_RETENTION_UPDATE_INTERVAL;
int             use_retained_program_state = TRUE;
int             use_retained_scheduling_info = FALSE;
int             retention_scheduling_horizon = DEFAULT_RETENTION_SCHEDULING_HORIZON;
unsigned long   modified_host_process_attributes = MODATTR_NONE;
unsigned long   modified_service_process_attributes = MODATTR_NONE;
unsigned long   retained_host_attribute_mask = 0L;
unsigned long   retained_service_attribute_mask = 0L;
unsigned long   retained_contact_host_attribute_mask = 0L;
unsigned long   retained_contact_service_attribute_mask = 0L;
unsigned long   retained_process_host_attribute_mask = 0L;
unsigned long   retained_process_service_attribute_mask = 0L;

unsigned long   next_comment_id = 0L;
unsigned long   next_downtime_id = 0L;
unsigned long   next_event_id = 0L;
unsigned long   next_problem_id = 0L;
unsigned long   next_notification_id = 0L;

int             log_rotation_method = LOG_ROTATION_NONE;

int             sigshutdown = FALSE;
int             sigrestart = FALSE;
char            *sigs[35] = {"EXIT", "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT", "BUS", "FPE", "KILL", "USR1", "SEGV", "USR2", "PIPE", "ALRM", "TERM", "STKFLT", "CHLD", "CONT", "STOP", "TSTP", "TTIN", "TTOU", "URG", "XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "IO", "PWR", "UNUSED", "ZERR", "DEBUG", (char *)NULL};
int             caught_signal = FALSE;
int             sig_id = 0;

int             restarting = FALSE;

int             verify_config = FALSE;
int             verify_object_relationships = TRUE;
int             verify_circular_paths = TRUE;
int             test_scheduling = FALSE;
int             precache_objects = FALSE;
int             use_precached_objects = FALSE;

int             daemon_mode = FALSE;
int             daemon_dumps_core = TRUE;

int             max_parallel_service_checks = DEFAULT_MAX_PARALLEL_SERVICE_CHECKS;
int             currently_running_service_checks = 0;
int             currently_running_host_checks = 0;

time_t          program_start = 0L;
time_t          event_start = 0L;
int             nagios_pid = 0;
int             enable_notifications = TRUE;
int             execute_service_checks = TRUE;
int             accept_passive_service_checks = TRUE;
int             execute_host_checks = TRUE;
int             accept_passive_host_checks = TRUE;
int             enable_event_handlers = TRUE;
int             obsess_over_services = FALSE;
int             obsess_over_hosts = FALSE;
int             enable_failure_prediction = TRUE;

int             translate_passive_host_checks = DEFAULT_TRANSLATE_PASSIVE_HOST_CHECKS;
int             passive_host_checks_are_soft = DEFAULT_PASSIVE_HOST_CHECKS_SOFT;

int             aggregate_status_updates = TRUE;
int             status_update_interval = DEFAULT_STATUS_UPDATE_INTERVAL;

int             time_change_threshold = DEFAULT_TIME_CHANGE_THRESHOLD;

unsigned long   event_broker_options = BROKER_NOTHING;

int             process_performance_data = DEFAULT_PROCESS_PERFORMANCE_DATA;

int             enable_flap_detection = DEFAULT_ENABLE_FLAP_DETECTION;

double          low_service_flap_threshold = DEFAULT_LOW_SERVICE_FLAP_THRESHOLD;
double          high_service_flap_threshold = DEFAULT_HIGH_SERVICE_FLAP_THRESHOLD;
double          low_host_flap_threshold = DEFAULT_LOW_HOST_FLAP_THRESHOLD;
double          high_host_flap_threshold = DEFAULT_HIGH_HOST_FLAP_THRESHOLD;

int             use_large_installation_tweaks = DEFAULT_USE_LARGE_INSTALLATION_TWEAKS;
int             enable_environment_macros = TRUE;
int             free_child_process_memory = -1;
int             child_processes_fork_twice = -1;

int             enable_embedded_perl = DEFAULT_ENABLE_EMBEDDED_PERL;
int             use_embedded_perl_implicitly = DEFAULT_USE_EMBEDDED_PERL_IMPLICITLY;
int             embedded_perl_initialized = FALSE;

int             date_format = DATE_FORMAT_US;
char            *use_timezone = NULL;

int             allow_empty_hostgroup_assignment = DEFAULT_ALLOW_EMPTY_HOSTGROUP_ASSIGNMENT;

int             command_file_fd;
FILE            *command_file_fp;
int             command_file_created = FALSE;


extern contact	       *contact_list;
extern contactgroup    *contactgroup_list;
extern hostgroup       *hostgroup_list;
extern command         *command_list;
extern timeperiod      *timeperiod_list;
extern serviceescalation *serviceescalation_list;

notification    *notification_list;

check_result    check_result_info;
check_result    *check_result_list = NULL;
unsigned long	max_check_result_file_age = DEFAULT_MAX_CHECK_RESULT_AGE;

dbuf            check_result_dbuf;

circular_buffer external_command_buffer;
circular_buffer check_result_buffer;
pthread_t       worker_threads[TOTAL_WORKER_THREADS];
int             external_command_buffer_slots = DEFAULT_EXTERNAL_COMMAND_BUFFER_SLOTS;

check_stats     check_statistics[MAX_CHECK_STATS_TYPES];

char            *debug_file;
int             debug_level = DEFAULT_DEBUG_LEVEL;
int             debug_verbosity = DEFAULT_DEBUG_VERBOSITY;
unsigned long   max_debug_file_size = DEFAULT_MAX_DEBUG_FILE_SIZE;




/* Following main() declaration required by older versions of Perl ut 5.00503 */
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

	mac = get_global_macros();



#ifdef HAVE_GETOPT_H
	int option_index = 0;
	static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"version", no_argument, 0, 'V'},
			{"license", no_argument, 0, 'V'},
			{"verify-config", no_argument, 0, 'v'},
			{"daemon", no_argument, 0, 'd'},
			{"test-scheduling", no_argument, 0, 's'},
			{"dont-verify-objects", no_argument, 0, 'o'},
			{"dont-verify-paths", no_argument, 0, 'x'},
			{"precache-objects", no_argument, 0, 'p'},
			{"use-precached-objects", no_argument, 0, 'u'},
			{0, 0, 0, 0}
		};
#endif

	/* make sure we have the correct number of command line arguments */
	if(argc < 2)
		error = TRUE;


	/* get all command line arguments */
	while(1) {

#ifdef HAVE_GETOPT_H
		c = getopt_long(argc, argv, "+hVvdsoxpu", long_options, &option_index);
#else
		c = getopt(argc, argv, "+hVvdsoxpu");
#endif

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
				verify_config = TRUE;
				break;

			case 's': /* scheduling check */
				test_scheduling = TRUE;
				break;

			case 'd': /* daemon mode */
				daemon_mode = TRUE;
				break;

			case 'o': /* don't verify objects */
				/*
				verify_object_relationships=FALSE;
				*/
				break;

			case 'x': /* don't verify circular paths */
				verify_circular_paths = FALSE;
				break;

			case 'p': /* precache object config */
				precache_objects = TRUE;
				break;

			case 'u': /* use precached object config */
				use_precached_objects = TRUE;
				break;

			default:
				break;
			}

		}

	/* make sure we have the right combination of arguments */
	if(precache_objects == TRUE && (test_scheduling == FALSE && verify_config == FALSE)) {
		error = TRUE;
		display_help = TRUE;
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
		printf("  -v, --verify-config          Verify all configuration data\n");
		printf("  -s, --test-scheduling        Shows projected/recommended check scheduling and other\n");
		printf("                               diagnostic info based on the current configuration files.\n");
		/*printf("  -o, --dont-verify-objects    Don't verify object relationships - USE WITH CAUTION!\n");*/
		printf("  -x, --dont-verify-paths      Don't check for circular object paths - USE WITH CAUTION!\n");
		printf("  -p, --precache-objects       Precache object configuration - use with -v or -s options\n");
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


	/* we're just verifying the configuration... */
	if(verify_config == TRUE) {

		/* reset program variables */
		reset_variables();

		printf("Reading configuration data...\n");

		/* read in the configuration files (main config file, resource and object config files) */
		if((result = read_main_config_file(config_file)) == OK) {

			printf("   Read main config file okay...\n");

			/* drop privileges */
			if((result = drop_privileges(nagios_user, nagios_group)) == ERROR)
				printf("   Failed to drop privileges.  Aborting.");
			else {
				/* read object config files */
				if((result = read_all_object_data(config_file)) == OK)
					printf("   Read object config files okay...\n");
				else
					printf("   Error processing object config files!\n");
				}
			}
		else
			printf("   Error processing main config file!\n\n");

		printf("\n");

		/* there was a problem reading the config files */
		if(result != OK) {

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
			}

		/* the config files were okay, so run the pre-flight check */
		else {

			printf("Running pre-flight check on configuration data...\n\n");

			/* run the pre-flight check to make sure things look okay... */
			result = pre_flight_check();

			if(result == OK)
				printf("\nThings look okay - No serious problems were detected during the pre-flight check\n");
			else {
				printf("\n***> One or more problems was encountered while running the pre-flight check...\n");
				printf("\n");
				printf("     Check your configuration file(s) to ensure that they contain valid\n");
				printf("     directives and data defintions.  If you are upgrading from a previous\n");
				printf("     version of Nagios, you should be aware that some variables/definitions\n");
				printf("     may have been removed or modified in this version.  Make sure to read\n");
				printf("     the HTML documentation regarding the config files, as well as the\n");
				printf("     'Whats New' section to find out what has changed.\n\n");
				}
			}

		/* clean up after ourselves */
		cleanup();

		/* free config_file */
		my_free(config_file);

		/* exit */
		exit(result);
		}


	/* we're just testing scheduling... */
	else if(test_scheduling == TRUE) {

		/* reset program variables */
		reset_variables();

		/* read in the configuration files (main config file and all host config files) */
		result = read_main_config_file(config_file);

		/* drop privileges */
		if(result == OK)
			if((result = drop_privileges(nagios_user, nagios_group)) == ERROR)
				printf("Failed to drop privileges.  Aborting.");

		/* read object config files */
		if(result == OK)
			result = read_all_object_data(config_file);

		/* read initial service and host state information */
		if(result == OK) {
			initialize_retention_data(config_file);
			read_initial_state_information();
			}

		if(result != OK)
			printf("***> One or more problems was encountered while reading configuration data...\n");

		/* run the pre-flight check to make sure everything looks okay */
		if(result == OK) {
			if((result = pre_flight_check()) != OK)
				printf("***> One or more problems was encountered while running the pre-flight check...\n");
			}

		if(result == OK) {

			/* initialize the event timing loop */
			init_timing_loop();

			/* display scheduling information */
			display_scheduling_info();

			if(precache_objects == TRUE) {
				printf("\n");
				printf("OBJECT PRECACHING\n");
				printf("-----------------\n");
				printf("Object config files were precached.\n");
				}
			}

#undef TEST_TIMEPERIODS
#ifdef TEST_TIMEPERIODS
		/* DO SOME TIMEPERIOD TESTING - ADDED 08/11/2009 */
		time_t now, pref_time, valid_time;
		timeperiod *tp;
		tp = find_timeperiod("247_exclusion");
		time(&now);
		pref_time = now;
		get_next_valid_time(pref_time, &valid_time, tp);
		printf("=====\n");
		printf("CURRENT:   %lu = %s", (unsigned long)now, ctime(&now));
		printf("PREFERRED: %lu = %s", (unsigned long)pref_time, ctime(&pref_time));
		printf("NEXT:      %lu = %s", (unsigned long)valid_time, ctime(&valid_time));
		printf("=====\n");
#endif

		/* clean up after ourselves */
		cleanup();

		/* exit */
		exit(result);
		}


	/* else start to monitor things... */
	else {

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

					/* shutdown the external command worker thread */
					shutdown_command_file_worker_thread();

					/* close and delete the external command file FIFO */
					close_command_file();

					/* cleanup embedded perl interpreter */
					if(embedded_perl_initialized == TRUE)
						deinit_embedded_perl();
					}

#ifdef USE_EVENT_BROKER
				/* send program data to broker */
				broker_program_state(NEBTYPE_PROCESS_SHUTDOWN, NEBFLAG_PROCESS_INITIATED, NEBATTR_SHUTDOWN_ABNORMAL, NULL);
#endif
				cleanup();
				exit(ERROR);
				}



			/* initialize embedded Perl interpreter */
			/* NOTE 02/15/08 embedded Perl must be initialized if compiled in, regardless of whether or not its enabled in the config file */
			/* It compiled it, but not initialized, Nagios will segfault in readdir() calls, as libperl takes this function over */
			if(embedded_perl_initialized == FALSE) {
				/*				if(enable_embedded_perl==TRUE){*/
#ifdef EMBEDDEDPERL
				init_embedded_perl(env);
#else
				init_embedded_perl(NULL);
#endif
				embedded_perl_initialized = TRUE;
				/*					}*/
				}

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

			/* open the command file (named pipe) for reading */
			result = open_command_file();
			if(result != OK) {

				logit(NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR, TRUE, "Bailing out due to errors encountered while trying to initialize the external command file... (PID=%d)\n", (int)getpid());

#ifdef USE_EVENT_BROKER
				/* send program data to broker */
				broker_program_state(NEBTYPE_PROCESS_SHUTDOWN, NEBFLAG_PROCESS_INITIATED, NEBATTR_SHUTDOWN_ABNORMAL, NULL);
#endif
				cleanup();
				exit(ERROR);
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

			/* Determine which checks are still executing so they are not
				scheduled when the timing loop is initialized */
			find_executing_checks(check_result_path);

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
					asprintf(&buffer, "Caught SIGHUP, restarting...\n");
				else if(sig_id != SIGSEGV)
					asprintf(&buffer, "Caught SIG%s, shutting down...\n", sigs[sig_id]);

				write_to_all_logs(buffer, NSLOG_PROCESS_INFO);
				my_free(buffer);
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

			/* clean up the comment data */
			cleanup_comment_data(config_file);

			/* clean up the status data unless we're restarting */
			if(sigrestart == FALSE)
				cleanup_status_data(config_file, TRUE);

			/* close and delete the external command file FIFO unless we're restarting */
			if(sigrestart == FALSE) {
				shutdown_command_file_worker_thread();
				close_command_file();
				}

			/* cleanup embedded perl interpreter */
			if(sigrestart == FALSE)
				deinit_embedded_perl();

			/* shutdown stuff... */
			if(sigshutdown == TRUE) {

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


