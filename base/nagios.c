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

static int is_worker;

static void set_loadctl_defaults(void)
{
	struct rlimit rlim;

	/* Workers need to up 'em, master needs to know 'em */
	getrlimit(RLIMIT_NOFILE, &rlim);
	rlim.rlim_cur = rlim.rlim_max;
	setrlimit(RLIMIT_NOFILE, &rlim);
	loadctl.nofile_limit = rlim.rlim_max;
#ifdef RLIMIT_NPROC
	getrlimit(RLIMIT_NPROC, &rlim);
	rlim.rlim_cur = rlim.rlim_max;
	setrlimit(RLIMIT_NPROC, &rlim);
	loadctl.nproc_limit = rlim.rlim_max;
#else
	loadctl.nproc_limit = loadctl.nofile_limit / 2;
#endif

	/*
	 * things may have been configured already. Otherwise we
	 * set some sort of sane defaults here
	 */
	if (!loadctl.jobs_max) {
		loadctl.jobs_max = loadctl.nproc_limit - 100;
		if (!is_worker && loadctl.jobs_max > (loadctl.nofile_limit - 50) * wproc_num_workers_online) {
			loadctl.jobs_max = (loadctl.nofile_limit - 50) * wproc_num_workers_online;
		}
	}

	if (!loadctl.jobs_limit)
		loadctl.jobs_limit = loadctl.jobs_max;

	if (!loadctl.backoff_limit)
		loadctl.backoff_limit = online_cpus() * 2.5;
	if (!loadctl.rampup_limit)
		loadctl.rampup_limit = online_cpus() * 0.8;
	if (!loadctl.backoff_change)
		loadctl.backoff_change = loadctl.jobs_limit * 0.3;
	if (!loadctl.rampup_change)
		loadctl.rampup_change = loadctl.backoff_change * 0.25;
	if (!loadctl.check_interval)
		loadctl.check_interval = 60;
	if (!loadctl.jobs_min)
		loadctl.jobs_min = online_cpus() * 20; /* pessimistic */
}

static int test_path_access(const char *program, int mode)
{
	char *envpath, *p, *colon;
	int ret, our_errno = 1500; /* outside errno range */

	if (program[0] == '/' || !(envpath = getenv("PATH")))
		return access(program, mode);

	if (!(envpath = strdup(envpath))) {
		errno = ENOMEM;
		return -1;
	}

	for (p = envpath; p; p = colon + 1) {
		char *path;

		colon = strchr(p, ':');
		if (colon)
			*colon = 0;
		asprintf(&path, "%s/%s", p, program);
		ret = access(path, mode);
		free(path);
		if (!ret)
			break;

		if (ret < 0) {
			if (errno == ENOENT)
				continue;
			if (our_errno > errno)
				our_errno = errno;
		}
		if (!colon)
			break;
	}

	free(envpath);

	if (!ret)
		errno = 0;
	else
		errno = our_errno;

	return ret;
}

static int nagios_core_worker(const char *path)
{
	int sd, ret;
	char response[128];

	is_worker = 1;

	set_loadctl_defaults();

	sd = nsock_unix(path, NSOCK_TCP | NSOCK_CONNECT);
	if (sd < 0) {
		printf("Failed to connect to query socket '%s': %s: %s\n",
			   path, nsock_strerror(sd), strerror(errno));
		return 1;
	}

	ret = nsock_printf_nul(sd, "@wproc register name=Core Worker %ld;pid=%ld", (long)getpid(), (long)getpid());
	if (ret < 0) {
		printf("Failed to register as worker.\n");
		return 1;
	}

	ret = read(sd, response, 3);
	if (ret != 3) {
		printf("Failed to read response from wproc manager\n");
		return 1;
	}
	if (memcmp(response, "OK", 3)) {
		read(sd, response + 3, sizeof(response) - 4);
		response[sizeof(response) - 2] = 0;
		printf("Failed to register with wproc manager: %s\n", response);
		return 1;
	}

	enter_worker(sd, start_cmd);
	return 0;
}

/*
 * only handles logfile for now, which we stash in macros to
 * make sure we can log *somewhere* in case the new path is
 * completely inaccessible.
 */
static int test_configured_paths(void)
{
	FILE *fp;
	nagios_macros *mac;

	mac = get_global_macros();

	fp = fopen(log_file, "a+");
	if (!fp) {
		/*
		 * we do some variable trashing here so logit() can
		 * open the old logfile (if any), in case we got a
		 * restart command or a SIGHUP
		 */
		char *value_absolute = log_file;
		log_file = mac->x[MACRO_LOGFILE];
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to open logfile '%s' for writing: %s\n", value_absolute, strerror(errno));
		return ERROR;
		}

	fclose(fp);

	/* save the macro */
	mac->x[MACRO_LOGFILE] = log_file;
	return OK;
}

int main(int argc, char **argv) {
	int result;
	int error = FALSE;
	int display_license = FALSE;
	int display_help = FALSE;
	int c = 0;
	struct tm *tm, tm_s;
	time_t now;
	char datestring[256];
	nagios_macros *mac;
	const char *worker_socket = NULL;
	int i;
#ifdef HAVE_SIGACTION
	struct sigaction sig_action;
#endif

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
			{"worker", required_argument, 0, 'W'},
			{0, 0, 0, 0}
		};
#define getopt(argc, argv, o) getopt_long(argc, argv, o, long_options, &option_index)
#endif

	memset(&loadctl, 0, sizeof(loadctl));
	mac = get_global_macros();

	/* make sure we have the correct number of command line arguments */
	if(argc < 2)
		error = TRUE;

	/* get all command line arguments */
	while(1) {
		c = getopt(argc, argv, "+hVvdspuxTW");

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
			case 'W':
				worker_socket = optarg;
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
	/* if we're a worker we can skip everything below */
	if(worker_socket) {
		exit(nagios_core_worker(worker_socket));
	}

	/* Initialize configuration variables */                             
	init_main_cfg_vars(1);
	init_shared_cfg_vars(1);

	if(daemon_mode == FALSE) {
		printf("\nNagios Core %s\n", PROGRAM_VERSION);
		printf("Copyright (c) 2009-present Nagios Core Development Team and Community Contributors\n");
		printf("Copyright (c) 1999-2009 Ethan Galstad\n");
		printf("Last Modified: %s\n", PROGRAM_MODIFICATION_DATE);
		printf("License: GPL\n\n");
		printf("Website: https://www.nagios.org\n");
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
		printf("  -x, --dont-verify-paths      Deprecated (Don't check for circular object paths)\n");
		printf("  -p, --precache-objects       Precache object configuration\n");
		printf("  -u, --use-precached-objects  Use precached object config file\n");
		printf("  -d, --daemon                 Starts Nagios in daemon mode, instead of as a foreground process\n");
		printf("  -W, --worker /path/to/socket Act as a worker for an already running daemon\n");
		printf("\n");
		printf("Visit the Nagios website at https://www.nagios.org/ for bug fixes, new\n");
		printf("releases, online documentation, FAQs, information on subscribing to\n");
		printf("the mailing lists, and commercial support options for Nagios.\n");
		printf("\n");

		exit(ERROR);
		}


	/*
	 * config file is last argument specified.
	 * Make sure it uses an absolute path
	 */
	config_file = nspath_absolute(argv[optind], NULL);
	if(config_file == NULL) {
		printf("Error allocating memory.\n");
		exit(ERROR);
		}

	config_file_dir = nspath_absolute_dirname(config_file, NULL);

	/* 
	 * Set the signal handler for the SIGXFSZ signal here because
	 * we may encounter this signal before the other signal handlers
	 * are set.
	 */
#ifdef HAVE_SIGACTION
	sig_action.sa_sigaction = NULL;
	sig_action.sa_handler = handle_sigxfsz;
	sigfillset(&sig_action.sa_mask);
	sig_action.sa_flags = SA_NODEFER|SA_RESTART;
	sigaction(SIGXFSZ, &sig_action, NULL);
#else
	signal(SIGXFSZ, handle_sigxfsz);
#endif

	/*
	 * let's go to town. We'll be noisy if we're verifying config
	 * or running scheduling tests.
	 */
	if(verify_config || test_scheduling || precache_objects) {
		reset_variables();
		/*
		 * if we don't beef up our resource limits as much as
		 * we can, it's quite possible we'll run headlong into
		 * EAGAIN due to too many processes when we try to
		 * drop privileges later.
		 */
		set_loadctl_defaults();

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

		/*
		 * this must come after dropping privileges, so we make
		 * sure to test access permissions as the right user.
		 */
		if (!verify_config && test_configured_paths() == ERROR) {
			printf("   One or more path problems detected. Aborting.\n");
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
				printf("     main configuration file is typically '%s'\n", DEFAULT_CONFIG_FILE);
				}

			printf("\n***> One or more problems was encountered while processing the config files...\n");
			printf("\n");
			printf("     Check your configuration file(s) to ensure that they contain valid\n");
			printf("     directives and data definitions.  If you are upgrading from a previous\n");
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
			printf("     directives and data definitions.  If you are upgrading from a previous\n");
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

		/* make valgrind shut up about still reachable memory */
		neb_free_module_list();
		free(config_file_dir);
		free(config_file);

		exit(result);
		}


	/* else start to monitor things... */
	else {

		/*
		 * if we're called with a relative path we must make
		 * it absolute so we can launch our workers.
		 * If not, we needn't bother, as we're using execvp()
		 */
		if (strchr(argv[0], '/'))
			nagios_binary_path = nspath_absolute(argv[0], NULL);
		else
			nagios_binary_path = strdup(argv[0]);

		if (!nagios_binary_path) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Unable to allocate memory for nagios_binary_path\n");
			exit(EXIT_FAILURE);
			}

		if (!(nagios_iobs = iobroker_create())) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to create IO broker set: %s\n",
				  strerror(errno));
			exit(EXIT_FAILURE);
			}

		/* keep monitoring things until we get a shutdown command */
		do {
			/* reset internal book-keeping (in case we're restarting) */
			wproc_num_workers_spawned = wproc_num_workers_online = 0;
			caught_signal = sigshutdown = FALSE;
			sig_id = 0;

			/* reset program variables */
			reset_variables();
			timing_point("Variables reset\n");

			/* get PID */
			nagios_pid = (int)getpid();

			/* read in the configuration files (main and resource config files) */
			result = read_main_config_file(config_file);
			if (result != OK) {
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to process config file '%s'. Aborting\n", config_file);
				exit(EXIT_FAILURE);
				}
			timing_point("Main config file read\n");

			/* NOTE 11/06/07 EG moved to after we read config files, as user may have overridden timezone offset */
			/* get program (re)start time and save as macro */
			program_start = time(NULL);
			my_free(mac->x[MACRO_PROCESSSTARTTIME]);
			asprintf(&mac->x[MACRO_PROCESSSTARTTIME], "%llu", (unsigned long long)program_start);

			/* drop privileges */
			if(drop_privileges(nagios_user, nagios_group) == ERROR) {

				logit(NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR | NSLOG_CONFIG_ERROR, TRUE, "Failed to drop privileges.  Aborting.");

				cleanup();
				exit(ERROR);
				}

			if (test_path_access(nagios_binary_path, X_OK)) {
				logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: failed to access() %s: %s\n", nagios_binary_path, strerror(errno));
				logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Spawning workers will be impossible. Aborting.\n");
				exit(EXIT_FAILURE);
				}

			if (test_configured_paths() == ERROR) {
				/* error has already been logged */
				exit(EXIT_FAILURE);
				}
			/* enter daemon mode (unless we're restarting...) */
			if(daemon_mode == TRUE && sigrestart == FALSE) {

				result = daemon_init();

				/* we had an error daemonizing, so bail... */
				if(result == ERROR) {
					logit(NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR, TRUE, "Bailing out due to failure to daemonize. (PID=%d)", (int)getpid());
					cleanup();
					exit(EXIT_FAILURE);
					}

				/* get new PID */
				nagios_pid = (int)getpid();
				}

			/* this must be logged after we read config data, as user may have changed location of main log file */
			logit(NSLOG_PROCESS_INFO, TRUE, "Nagios %s starting... (PID=%d)\n", PROGRAM_VERSION, (int)getpid());

			/* log the local time - may be different than clock time due to timezone offset */
			now = time(NULL);
			tm = localtime_r(&now, &tm_s);
			strftime(datestring, sizeof(datestring), "%a %b %d %H:%M:%S %Z %Y", tm);
			logit(NSLOG_PROCESS_INFO, TRUE, "Local time is %s", datestring);

			/* write log version/info */
			write_log_file_info(NULL);

			/* open debug log now that we're the right user */
			open_debug_log();

#ifdef USE_EVENT_BROKER
			/* initialize modules */
			neb_init_modules();
			neb_init_callback_list();
#endif
			timing_point("NEB module API initialized\n");

			/* handle signals (interrupts) before we do any socket I/O */
			setup_sighandler();

			/*
			 * Initialize query handler and event subscription service.
			 * This must be done before modules are initialized, so
			 * the modules can use our in-core stuff properly
			 */
			if (qh_init(qh_socket_path ? qh_socket_path : DEFAULT_QUERY_SOCKET) != OK) {
				logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to initialize query handler. Aborting\n");
				exit(EXIT_FAILURE);
			}
			timing_point("Query handler initialized\n");
			nerd_init();
			timing_point("NERD initialized\n");

			/* initialize check workers */
			if(init_workers(num_check_workers) < 0) {
				logit(NSLOG_RUNTIME_ERROR, TRUE, "Failed to spawn workers. Aborting\n");
				exit(EXIT_FAILURE);
			}
			timing_point("%u workers spawned\n", wproc_num_workers_spawned);
			i = 0;
			while (i < 50 && wproc_num_workers_online < wproc_num_workers_spawned) {
				iobroker_poll(nagios_iobs, 50);
				i++;
			}
			timing_point("%u workers connected\n", wproc_num_workers_online);

			/* now that workers have arrived we can set the defaults */
			set_loadctl_defaults();

#ifdef USE_EVENT_BROKER
			/* load modules */
			if (neb_load_all_modules() != OK) {
				logit(NSLOG_CONFIG_ERROR, ERROR, "Error: Module loading failed. Aborting.\n");
				/* if we're dumping core, we must remove all dl-files */
				if (daemon_dumps_core)
					neb_unload_all_modules(NEBMODULE_FORCE_UNLOAD, NEBMODULE_NEB_SHUTDOWN);
				exit(EXIT_FAILURE);
				}
			timing_point("Modules loaded\n");

			/* send program data to broker */
			broker_program_state(NEBTYPE_PROCESS_PRELAUNCH, NEBFLAG_NONE, NEBATTR_NONE, NULL);
			timing_point("First callback made\n");
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
					cleanup_status_data(TRUE);
					}

#ifdef USE_EVENT_BROKER
				/* send program data to broker */
				broker_program_state(NEBTYPE_PROCESS_SHUTDOWN, NEBFLAG_PROCESS_INITIATED, NEBATTR_SHUTDOWN_ABNORMAL, NULL);
#endif
				cleanup();
				exit(ERROR);
				}

			timing_point("Object configuration parsed and understood\n");

			/* write the objects.cache file */
			fcache_objects(object_cache_file);
			timing_point("Objects cached\n");

			init_event_queue();
			timing_point("Event queue initialized\n");


#ifdef USE_EVENT_BROKER
			/* send program data to broker */
			broker_program_state(NEBTYPE_PROCESS_START, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

			/* initialize status data unless we're starting */
			if(sigrestart == FALSE) {
				initialize_status_data(config_file);
				timing_point("Status data initialized\n");
				}

			/* initialize scheduled downtime data */
			initialize_downtime_data();
			timing_point("Downtime data initialized\n");

			/* read initial service and host state information  */
			initialize_retention_data(config_file);
			timing_point("Retention data initialized\n");
			read_initial_state_information();
			timing_point("Initial state information read\n");

			/* initialize comment data */
			initialize_comment_data();
			timing_point("Comment data initialized\n");

			/* initialize performance data */
			initialize_performance_data(config_file);
			timing_point("Performance data initialized\n");

			/* initialize the event timing loop */
			init_timing_loop();
			timing_point("Event timing loop initialized\n");

			/* initialize check statistics */
			init_check_stats();
			timing_point("check stats initialized\n");

			/* check for updates */
			check_for_nagios_updates(FALSE, TRUE);
			timing_point("Update check concluded\n");

			/* update all status data (with retained information) */
			update_all_status_data();
			timing_point("Status data updated\n");

			/* log initial host and service state */
			log_host_states(INITIAL_STATES, NULL);
			log_service_states(INITIAL_STATES, NULL);
			timing_point("Initial states logged\n");

			/* reset the restart flag */
			sigrestart = FALSE;

			/* fire up command file worker */
			launch_command_file_worker();
			timing_point("Command file worker launched\n");

#ifdef USE_EVENT_BROKER
			/* send program data to broker */
			broker_program_state(NEBTYPE_PROCESS_EVENTLOOPSTART, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

			/* get event start time and save as macro */
			event_start = time(NULL);
			my_free(mac->x[MACRO_EVENTSTARTTIME]);
			asprintf(&mac->x[MACRO_EVENTSTARTTIME], "%llu", (unsigned long long)event_start);

			timing_point("Entering event execution loop\n");
			/***** start monitoring all services *****/
			/* (doesn't return until a restart or shutdown signal is encountered) */
			event_execution_loop();

			/*
			 * immediately deinitialize the query handler so it
			 * can remove modules that have stashed data with it
			 */
			qh_deinit(qh_socket_path ? qh_socket_path : DEFAULT_QUERY_SOCKET);

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
			cleanup_retention_data();

			/* clean up performance data */
			cleanup_performance_data();

			/* clean up the scheduled downtime data */
			cleanup_downtime_data();

			/* clean up the status data unless we're restarting */
			if(sigrestart == FALSE) {
				cleanup_status_data(TRUE);
				}

			free_worker_memory(WPROC_FORCE);
			/* shutdown stuff... */
			if(sigshutdown == TRUE) {
				iobroker_destroy(nagios_iobs, IOBROKER_CLOSE_SOCKETS);
				nagios_iobs = NULL;

				/* log a shutdown message */
				logit(NSLOG_PROCESS_INFO, TRUE, "Successfully shutdown... (PID=%d)\n", (int)getpid());
				}

			/* clean up after ourselves */
			cleanup();

			/* close debug log */
			close_debug_log();

			}
		while(sigrestart == TRUE && sigshutdown == FALSE);

		if(daemon_mode == TRUE)
			unlink(lock_file);

		/* free misc memory */
		my_free(lock_file);
		my_free(config_file);
		my_free(config_file_dir);
		my_free(nagios_binary_path);
		}

	return OK;
	}
