/*****************************************************************************
 *
 * CHECKS.C - Service and host check functions for Nagios
 *
 * Copyright (c) 2011 Nagios Core Development Team
 * Copyright (c) 1999-2010 Ethan Galstad (egalstad@nagios.org)
 * Last Modified: 01-20-2011
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
#include "../include/comments.h"
#include "../include/common.h"
#include "../include/statusdata.h"
#include "../include/downtime.h"
#include "../include/macros.h"
#include "../include/nagios.h"
#include "../include/broker.h"
#include "../include/perfdata.h"

/*#define DEBUG_CHECKS*/
/*#define DEBUG_HOST_CHECKS 1*/


#ifdef EMBEDDEDPERL
#include "../include/epn_nagios.h"
#endif

#ifdef USE_EVENT_BROKER
#include "../include/neberrors.h"
#endif

extern int      sigshutdown;
extern int      sigrestart;

extern char     *temp_file;
extern char     *temp_path;
extern char     *check_result_path;

extern int      interval_length;

extern int      command_check_interval;

extern int      log_initial_states;
extern int      log_passive_checks;
extern int      log_host_retries;

extern int      service_check_timeout;
extern int      host_check_timeout;

extern int      check_reaper_interval;
extern int      max_check_reaper_time;

extern int      use_aggressive_host_checking;
extern unsigned long cached_host_check_horizon;
extern unsigned long cached_service_check_horizon;
extern int      enable_predictive_host_dependency_checks;
extern int      enable_predictive_service_dependency_checks;

extern int      soft_state_dependencies;

extern int      currently_running_service_checks;
extern int      currently_running_host_checks;

extern int      accept_passive_service_checks;
extern int      execute_service_checks;
extern int      accept_passive_host_checks;
extern int      execute_host_checks;
extern int      obsess_over_services;
extern int      obsess_over_hosts;

extern int      translate_passive_host_checks;
extern int      passive_host_checks_are_soft;

extern int      check_service_freshness;
extern int      check_host_freshness;
extern int      additional_freshness_latency;

extern int      max_host_check_spread;
extern int      max_service_check_spread;

extern int      use_large_installation_tweaks;
extern int      free_child_process_memory;
extern int      child_processes_fork_twice;

extern time_t   last_program_stop;
extern time_t   program_start;
extern time_t   event_start;

extern timed_event       *event_list_low;
extern timed_event       *event_list_low_tail;

extern host              *host_list;
extern service           *service_list;
extern servicedependency *servicedependency_list;
extern hostdependency    *hostdependency_list;

extern unsigned long   next_event_id;
extern unsigned long   next_problem_id;

extern check_result    check_result_info;
extern check_result    *check_result_list;

extern pthread_t       worker_threads[TOTAL_WORKER_THREADS];

extern unsigned long max_debug_file_size;

#ifdef EMBEDDEDPERL
extern int      use_embedded_perl;
#endif





/******************************************************************/
/********************** CHECK REAPER FUNCTIONS ********************/
/******************************************************************/

/* reaps host and service check results */
int reap_check_results(void) {
	check_result *queued_check_result = NULL;
	service *temp_service = NULL;
	host *temp_host = NULL;
	time_t current_time = 0L;
	time_t reaper_start_time = 0L;
	int reaped_checks = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "reap_check_results() start\n");
	log_debug_info(DEBUGL_CHECKS, 0, "Starting to reap check results.\n");

	/* get the start time */
	time(&reaper_start_time);

	/* process files in the check result queue */
	process_check_result_queue(check_result_path);

	/* read all check results that have come in... */
	while((queued_check_result = read_check_result(&check_result_list))) {

		reaped_checks++;

		log_debug_info(DEBUGL_CHECKS, 2, "Found a check result (#%d) to handle...\n", reaped_checks);

		/* service check */
		if(queued_check_result->object_check_type == SERVICE_CHECK) {

			/* make sure the service exists */
			if((temp_service = find_service(queued_check_result->host_name, queued_check_result->service_description)) == NULL) {

				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check result queue contained results for service '%s' on host '%s', but the service could not be found!  Perhaps you forgot to define the service in your config files?\n", queued_check_result->service_description, queued_check_result->host_name);

				/* free memory */
				free_check_result(queued_check_result);
				my_free(queued_check_result);

				/* TODO - add new service definition automatically */

				continue;
				}

			log_debug_info(DEBUGL_CHECKS, 1, "Handling check result for service '%s' on host '%s'...\n", temp_service->description, temp_service->host_name);

			/* process the check result */
			handle_async_service_check_result(temp_service, queued_check_result);
			}

		/* host check */
		else {
			if((temp_host = find_host(queued_check_result->host_name)) == NULL) {

				/* make sure the host exists */
				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check result queue contained results for host '%s', but the host could not be found!  Perhaps you forgot to define the host in your config files?\n", queued_check_result->host_name);

				/* free memory */
				free_check_result(queued_check_result);
				my_free(queued_check_result);

				/* TODO - add new host definition automatically */

				continue;
				}

			log_debug_info(DEBUGL_CHECKS, 1, "Handling check result for host '%s'...\n", temp_host->name);

			/* process the check result */
			handle_async_host_check_result_3x(temp_host, queued_check_result);
			}

		log_debug_info(DEBUGL_CHECKS | DEBUGL_IPC, 1, "Deleted check result file '%s'\n", queued_check_result->output_file);

		/* free allocated memory */
		free_check_result(queued_check_result);
		my_free(queued_check_result);

		/* break out if we've been here too long (max_check_reaper_time seconds) */
		time(&current_time);
		if((int)(current_time - reaper_start_time) > max_check_reaper_time) {
			log_debug_info(DEBUGL_CHECKS, 0, "Breaking out of check result reaper: max reaper time exceeded\n");
			break;
			}

		/* bail out if we encountered a signal */
		if(sigshutdown == TRUE || sigrestart == TRUE) {
			log_debug_info(DEBUGL_CHECKS, 0, "Breaking out of check result reaper: signal encountered\n");
			break;
			}
		}

	log_debug_info(DEBUGL_CHECKS, 0, "Finished reaping %d check results\n", reaped_checks);
	log_debug_info(DEBUGL_FUNCTIONS, 0, "reap_check_results() end\n");

	return OK;
	}




/******************************************************************/
/****************** SERVICE MONITORING FUNCTIONS ******************/
/******************************************************************/

/* executes a scheduled service check */
int run_scheduled_service_check(service *svc, int check_options, double latency) {
	int result = OK;
	time_t current_time = 0L;
	time_t preferred_time = 0L;
	time_t next_valid_time = 0L;
	int time_is_valid = TRUE;

	if(svc == NULL)
		return ERROR;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "run_scheduled_service_check() start\n");
	log_debug_info(DEBUGL_CHECKS, 0, "Attempting to run scheduled check of service '%s' on host '%s': check options=%d, latency=%lf\n", svc->description, svc->host_name, check_options, latency);

	/*
	 * reset the next_check_event so we know it's
	 * no longer in the scheduling queue
	 */
	svc->next_check_event = NULL;

	/* attempt to run the check */
	result = run_async_service_check(svc, check_options, latency, TRUE, TRUE, &time_is_valid, &preferred_time);

	/* an error occurred, so reschedule the check */
	if(result == ERROR) {

		log_debug_info(DEBUGL_CHECKS, 1, "Unable to run scheduled service check at this time\n");

		/* only attempt to (re)schedule checks that should get checked... */
		if(svc->should_be_scheduled == TRUE) {

			/* get current time */
			time(&current_time);

			/* determine next time we should check the service if needed */
			/* if service has no check interval, schedule it again for 5 minutes from now */
			if(current_time >= preferred_time)
				preferred_time = current_time + ((svc->check_interval <= 0) ? 300 : (svc->check_interval * interval_length));

			/* make sure we rescheduled the next service check at a valid time */
			get_next_valid_time(preferred_time, &next_valid_time, svc->check_period_ptr);

			/*
			logit(NSLOG_RUNTIME_WARNING,TRUE,"Warning: Service '%s' on host '%s' timeperiod check failed...\n",svc->description,svc->host_name);
			logit(NSLOG_RUNTIME_WARNING,TRUE,"Current time: %s",ctime(&current_time));
			logit(NSLOG_RUNTIME_WARNING,TRUE,"Preferred time: %s",ctime(&preferred_time));
			logit(NSLOG_RUNTIME_WARNING,TRUE,"Next valid time: %s",ctime(&next_valid_time));
			*/

			/* the service could not be rescheduled properly - set the next check time for next week */
			/*if(time_is_valid==FALSE && next_valid_time==preferred_time){*/
			/* UPDATED 08/12/09 EG to reflect proper timeperod check logic */
			if(time_is_valid == FALSE &&  check_time_against_period(next_valid_time, svc->check_period_ptr) == ERROR) {

				/*
				svc->next_check=(time_t)(next_valid_time+(60*60*24*365));
				svc->should_be_scheduled=FALSE;
				*/

				svc->next_check = (time_t)(next_valid_time + (60 * 60 * 24 * 7));

				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check of service '%s' on host '%s' could not be rescheduled properly.  Scheduling check for next week...\n", svc->description, svc->host_name);

				log_debug_info(DEBUGL_CHECKS, 1, "Unable to find any valid times to reschedule the next service check!\n");
				}

			/* this service could be rescheduled... */
			else {
				svc->next_check = next_valid_time;
				svc->should_be_scheduled = TRUE;

				log_debug_info(DEBUGL_CHECKS, 1, "Rescheduled next service check for %s", ctime(&next_valid_time));
				}
			}

		/* reschedule the next service check - unless we couldn't find a valid next check time */
		/* 10/19/07 EG - keep original check options */
		if(svc->should_be_scheduled == TRUE)
			schedule_service_check(svc, svc->next_check, check_options);

		/* update the status log */
		update_service_status(svc, FALSE);

		return ERROR;
		}

	return OK;
	}


/* forks a child process to run a service check, but does not wait for the service check result */
int run_async_service_check(service *svc, int check_options, double latency, int scheduled_check, int reschedule_check, int *time_is_valid, time_t *preferred_time) {
	nagios_macros mac;
	char *raw_command = NULL;
	char *processed_command = NULL;
	char output_buffer[MAX_INPUT_BUFFER] = "";
	char *temp_buffer = NULL;
	struct timeval start_time, end_time;
	pid_t pid = 0;
	int fork_error = FALSE;
	int wait_result = 0;
	host *temp_host = NULL;
	FILE *fp = NULL;
	int pclose_result = 0;
	mode_t new_umask = 077;
	mode_t old_umask;
	char *output_file = NULL;
	double old_latency = 0.0;
	dbuf checkresult_dbuf;
	int dbuf_chunk = 1024;
#ifdef USE_EVENT_BROKER
	int neb_result = OK;
#endif
#ifdef EMBEDDEDPERL
	char fname[512] = "";
	char *args[5] = {"", DO_CLEAN, "", "", NULL };
	char *perl_plugin_output = NULL;
	SV *plugin_hndlr_cr = NULL;
	int count ;
	int use_epn = FALSE;
#ifdef aTHX
	dTHX;
#endif
	dSP;
#endif

	log_debug_info(DEBUGL_FUNCTIONS, 0, "run_async_service_check()\n");

	/* make sure we have something */
	if(svc == NULL)
		return ERROR;

	/* is the service check viable at this time? */
	if(check_service_check_viability(svc, check_options, time_is_valid, preferred_time) == ERROR)
		return ERROR;

	/* find the host associated with this service */
	if((temp_host = svc->host_ptr) == NULL)
		return ERROR;

	/******** GOOD TO GO FOR A REAL SERVICE CHECK AT THIS POINT ********/

#ifdef USE_EVENT_BROKER
	/* initialize start/end times */
	start_time.tv_sec = 0L;
	start_time.tv_usec = 0L;
	end_time.tv_sec = 0L;
	end_time.tv_usec = 0L;

	/* send data to event broker */
	neb_result = broker_service_check(NEBTYPE_SERVICECHECK_ASYNC_PRECHECK, NEBFLAG_NONE, NEBATTR_NONE, svc, SERVICE_CHECK_ACTIVE, start_time, end_time, svc->service_check_command, svc->latency, 0.0, 0, FALSE, 0, NULL, NULL);

	/* neb module wants to cancel the service check - the check will be rescheduled for a later time by the scheduling logic */
	if(neb_result == NEBERROR_CALLBACKCANCEL) {
		if(preferred_time)
			*preferred_time += (svc->check_interval * interval_length);
		return ERROR;
		}

	/* neb module wants to override (or cancel) the service check - perhaps it will check the service itself */
	/* NOTE: if a module does this, it has to do a lot of the stuff found below to make sure things don't get whacked out of shape! */
	/* NOTE: if would be easier for modules to override checks when the NEBTYPE_SERVICECHECK_INITIATE event is called (later) */
	if(neb_result == NEBERROR_CALLBACKOVERRIDE)
		return OK;
#endif


	log_debug_info(DEBUGL_CHECKS, 0, "Checking service '%s' on host '%s'...\n", svc->description, svc->host_name);

	/* clear check options - we don't want old check options retained */
	/* only clear check options for scheduled checks - ondemand checks shouldn't affected retained check options */
	if(scheduled_check == TRUE)
		svc->check_options = CHECK_OPTION_NONE;

	/* update latency for macros, event broker, save old value for later */
	old_latency = svc->latency;
	svc->latency = latency;

	/* grab the host and service macro variables */
	memset(&mac, 0, sizeof(mac));
	grab_host_macros_r(&mac, temp_host);
	grab_service_macros_r(&mac, svc);

	/* get the raw command line */
	get_raw_command_line_r(&mac, svc->check_command_ptr, svc->service_check_command, &raw_command, 0);
	if(raw_command == NULL) {
		clear_volatile_macros_r(&mac);
		log_debug_info(DEBUGL_CHECKS, 0, "Raw check command for service '%s' on host '%s' was NULL - aborting.\n", svc->description, svc->host_name);
		if(preferred_time)
			*preferred_time += (svc->check_interval * interval_length);
		svc->latency = old_latency;
		return ERROR;
		}

	/* process any macros contained in the argument */
	process_macros_r(&mac, raw_command, &processed_command, 0);
	my_free(raw_command);
	if(processed_command == NULL) {
		clear_volatile_macros_r(&mac);
		log_debug_info(DEBUGL_CHECKS, 0, "Processed check command for service '%s' on host '%s' was NULL - aborting.\n", svc->description, svc->host_name);
		if(preferred_time)
			*preferred_time += (svc->check_interval * interval_length);
		svc->latency = old_latency;
		return ERROR;
		}

	/* get the command start time */
	gettimeofday(&start_time, NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	neb_result = broker_service_check(NEBTYPE_SERVICECHECK_INITIATE, NEBFLAG_NONE, NEBATTR_NONE, svc, SERVICE_CHECK_ACTIVE, start_time, end_time, svc->service_check_command, svc->latency, 0.0, service_check_timeout, FALSE, 0, processed_command, NULL);

	/* neb module wants to override the service check - perhaps it will check the service itself */
	if(neb_result == NEBERROR_CALLBACKOVERRIDE) {
		clear_volatile_macros_r(&mac);
		svc->latency = old_latency;
		my_free(processed_command);
		return OK;
		}
#endif

	/* increment number of service checks that are currently running... */
	currently_running_service_checks++;

	/* set the execution flag */
	svc->is_executing = TRUE;

	/* start save check info */
	check_result_info.object_check_type = SERVICE_CHECK;
	check_result_info.check_type = SERVICE_CHECK_ACTIVE;
	check_result_info.check_options = check_options;
	check_result_info.scheduled_check = scheduled_check;
	check_result_info.reschedule_check = reschedule_check;
	check_result_info.start_time = start_time;
	check_result_info.finish_time = start_time;
	check_result_info.early_timeout = FALSE;
	check_result_info.exited_ok = TRUE;
	check_result_info.return_code = STATE_OK;
	check_result_info.output = NULL;

	/* open a temp file for storing check output */
	old_umask = umask(new_umask);
	asprintf(&output_file, "%s/checkXXXXXX", temp_path);
	check_result_info.output_file_fd = mkstemp(output_file);
	if(check_result_info.output_file_fd >= 0)
		check_result_info.output_file_fp = fdopen(check_result_info.output_file_fd, "w");
	else {
		check_result_info.output_file_fp = NULL;
		check_result_info.output_file_fd = -1;
		}
	umask(old_umask);

	log_debug_info(DEBUGL_CHECKS | DEBUGL_IPC, 1, "Check result output will be written to '%s' (fd=%d)\n", output_file, check_result_info.output_file_fd);


	/* finish save check info */
	check_result_info.host_name = (char *)strdup(svc->host_name);
	check_result_info.service_description = (char *)strdup(svc->description);
	check_result_info.output_file = (check_result_info.output_file_fd < 0 || output_file == NULL) ? NULL : strdup(output_file);

	/* free memory */
	my_free(output_file);

	/* write start of check result file */
	/* if things go really bad later on down the line, the user will at least have a partial file to help debug missing output results */
	if(check_result_info.output_file_fp) {

		fprintf(check_result_info.output_file_fp, "### Active Check Result File ###\n");
		fprintf(check_result_info.output_file_fp, "file_time=%lu\n", (unsigned long)check_result_info.start_time.tv_sec);
		fprintf(check_result_info.output_file_fp, "\n");

		fprintf(check_result_info.output_file_fp, "### Nagios Service Check Result ###\n");
		fprintf(check_result_info.output_file_fp, "# Time: %s", ctime(&check_result_info.start_time.tv_sec));
		fprintf(check_result_info.output_file_fp, "host_name=%s\n", check_result_info.host_name);
		fprintf(check_result_info.output_file_fp, "service_description=%s\n", check_result_info.service_description);
		fprintf(check_result_info.output_file_fp, "check_type=%d\n", check_result_info.check_type);
		fprintf(check_result_info.output_file_fp, "check_options=%d\n", check_result_info.check_options);
		fprintf(check_result_info.output_file_fp, "scheduled_check=%d\n", check_result_info.scheduled_check);
		fprintf(check_result_info.output_file_fp, "reschedule_check=%d\n", check_result_info.reschedule_check);
		fprintf(check_result_info.output_file_fp, "latency=%f\n", svc->latency);
		fprintf(check_result_info.output_file_fp, "start_time=%lu.%lu\n", check_result_info.start_time.tv_sec, check_result_info.start_time.tv_usec);

		/* flush output or it'll get written again when we fork() */
		fflush(check_result_info.output_file_fp);
		}

	/* initialize dynamic buffer for storing plugin output */
	dbuf_init(&checkresult_dbuf, dbuf_chunk);


	/* reset latency (permanent value will be set later) */
	svc->latency = old_latency;

	/* update check statistics */
	update_check_stats((scheduled_check == TRUE) ? ACTIVE_SCHEDULED_SERVICE_CHECK_STATS : ACTIVE_ONDEMAND_SERVICE_CHECK_STATS, start_time.tv_sec);

#ifdef EMBEDDEDPERL

	/* get"filename" component of command */
	strncpy(fname, processed_command, strcspn(processed_command, " "));
	fname[strcspn(processed_command, " ")] = '\x0';

	/* should we use the embedded Perl interpreter to run this script? */
	use_epn = file_uses_embedded_perl(fname);

	/* if yes, do some initialization */
	if(use_epn == TRUE) {

		log_debug_info(DEBUGL_CHECKS, 1, "** Using Embedded Perl interpreter to run service check...\n");

		args[0] = fname;
		args[2] = "";

		if(strchr(processed_command, ' ') == NULL)
			args[3] = "";
		else
			args[3] = processed_command + strlen(fname) + 1;

		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		XPUSHs(sv_2mortal(newSVpv(args[0], 0)));
		XPUSHs(sv_2mortal(newSVpv(args[1], 0)));
		XPUSHs(sv_2mortal(newSVpv(args[2], 0)));
		XPUSHs(sv_2mortal(newSVpv(args[3], 0)));
		PUTBACK;

		/* call our perl interpreter to compile and optionally cache the command */

		call_pv("Embed::Persistent::eval_file", G_SCALAR | G_EVAL);

		SPAGAIN ;

		if(SvTRUE(ERRSV)) {

			/*
			 * if SvTRUE(ERRSV)
			 * 	write failure to IPC pipe
			 *	return
			 */

			/* remove the top element of the Perl stack (undef) */
			(void) POPs ;

			pclose_result = STATE_UNKNOWN;
			perl_plugin_output = SvPVX(ERRSV);

			log_debug_info(DEBUGL_CHECKS, 0, "Embedded Perl failed to compile %s, compile error %s - skipping plugin\n", fname, perl_plugin_output);

			/* save plugin output */
			if(perl_plugin_output != NULL) {
				temp_buffer = escape_newlines(perl_plugin_output);
				dbuf_strcat(&checkresult_dbuf, temp_buffer);
				my_free(temp_buffer);
				}

			/* get the check finish time */
			gettimeofday(&end_time, NULL);

			/* record check result info */
			check_result_info.exited_ok = FALSE;
			check_result_info.return_code = pclose_result;
			check_result_info.finish_time = end_time;

			/* write check result to file */
			if(check_result_info.output_file_fp) {

				fprintf(check_result_info.output_file_fp, "finish_time=%lu.%lu\n", check_result_info.finish_time.tv_sec, check_result_info.finish_time.tv_usec);
				fprintf(check_result_info.output_file_fp, "early_timeout=%d\n", check_result_info.early_timeout);
				fprintf(check_result_info.output_file_fp, "exited_ok=%d\n", check_result_info.exited_ok);
				fprintf(check_result_info.output_file_fp, "return_code=%d\n", check_result_info.return_code);
				fprintf(check_result_info.output_file_fp, "output=%s\n", (checkresult_dbuf.buf == NULL) ? "(null)" : checkresult_dbuf.buf);

				/* close the temp file */
				fclose(check_result_info.output_file_fp);

				/* move check result to queue directory */
				move_check_result_to_queue(check_result_info.output_file);
				}

			/* free memory */
			dbuf_free(&checkresult_dbuf);

			/* free check result memory */
			free_check_result(&check_result_info);

			return OK;
			}
		else {

			plugin_hndlr_cr = newSVsv(POPs);

			log_debug_info(DEBUGL_CHECKS, 1, "Embedded Perl successfully compiled %s and returned code ref to plugin handler\n", fname);

			PUTBACK ;
			FREETMPS ;
			LEAVE ;
			}
		}
#endif

	/* plugin is a C plugin or a Perl plugin _without_ compilation errors */

	/* fork a child process */
	pid = fork();

	/* an error occurred while trying to fork */
	if(pid == -1) {

		fork_error = TRUE;

		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: The check of service '%s' on host '%s' could not be performed due to a fork() error: '%s'.  The check will be rescheduled.\n", svc->description, svc->host_name, strerror(errno));

		log_debug_info(DEBUGL_CHECKS, 0, "Check of service '%s' on host '%s' could not be performed due to a fork() error: '%s'!\n", svc->description, svc->host_name, strerror(errno));
		}

	/* if we are in the child process... */
	else if(pid == 0) {

		/* set environment variables */
		set_all_macro_environment_vars_r(&mac, TRUE);

		/* ADDED 11/12/07 EG */
		/* close external command file and shut down worker thread */
		close_command_file();

		/* fork again if we're not in a large installation */
		if(child_processes_fork_twice == TRUE) {

			/* fork again... */
			pid = fork();

			/* an error occurred while trying to fork again */
			if(pid == -1)
				exit(STATE_UNKNOWN);
			}

		/* the grandchild (or child if large install tweaks are enabled) process should run the service check... */
		if(pid == 0 || child_processes_fork_twice == FALSE) {

			/* reset signal handling */
			reset_sighandler();

			/* become the process group leader */
			setpgid(0, 0);

			/* exit on term signals at this process level */
			signal(SIGTERM, SIG_DFL);

			/* catch plugins that don't finish in a timely manner */
			signal(SIGALRM, service_check_sighandler);
			alarm(service_check_timeout);

			/* disable rotation of the debug file */
			max_debug_file_size = 0L;

			/******** BEGIN EMBEDDED PERL INTERPRETER EXECUTION ********/
#ifdef EMBEDDEDPERL
			if(use_epn == TRUE) {

				/* execute our previously compiled script - from call_pv("Embed::Persistent::eval_file",..) */
				/* NB. args[2] is _now_ a code ref (to the Perl subroutine corresp to the plugin) returned by eval_file() */

				ENTER;
				SAVETMPS;
				PUSHMARK(SP);

				XPUSHs(sv_2mortal(newSVpv(args[0], 0)));
				XPUSHs(sv_2mortal(newSVpv(args[1], 0)));
				XPUSHs(plugin_hndlr_cr);
				XPUSHs(sv_2mortal(newSVpv(args[3], 0)));

				PUTBACK;

				count = call_pv("Embed::Persistent::run_package", G_ARRAY);

				SPAGAIN;

				perl_plugin_output = POPpx ;
				pclose_result = POPi ;

				/* NOTE: 07/16/07 This has to be done before FREETMPS statement below, or the POPpx pointer will be invalid (Hendrik B.) */
				/* get perl plugin output - escape newlines */
				if(perl_plugin_output != NULL) {
					temp_buffer = escape_newlines(perl_plugin_output);
					dbuf_strcat(&checkresult_dbuf, temp_buffer);
					my_free(temp_buffer);
					}

				PUTBACK;
				FREETMPS;
				LEAVE;

				log_debug_info(DEBUGL_CHECKS, 1, "Embedded Perl ran %s: return code=%d, plugin output=%s\n", fname, pclose_result, (perl_plugin_output == NULL) ? "NULL" : checkresult_dbuf.buf);

				/* reset the alarm */
				alarm(0);

				/* get the check finish time */
				gettimeofday(&end_time, NULL);

				/* record check result info */
				check_result_info.return_code = pclose_result;
				check_result_info.finish_time = end_time;

				/* write check result to file */
				if(check_result_info.output_file_fp) {

					fprintf(check_result_info.output_file_fp, "finish_time=%lu.%lu\n", check_result_info.finish_time.tv_sec, check_result_info.finish_time.tv_usec);
					fprintf(check_result_info.output_file_fp, "early_timeout=%d\n", check_result_info.early_timeout);
					fprintf(check_result_info.output_file_fp, "exited_ok=%d\n", check_result_info.exited_ok);
					fprintf(check_result_info.output_file_fp, "return_code=%d\n", check_result_info.return_code);
					fprintf(check_result_info.output_file_fp, "output=%s\n", (checkresult_dbuf.buf == NULL) ? "(null)" : checkresult_dbuf.buf);

					/* close the temp file */
					fclose(check_result_info.output_file_fp);

					/* move check result to queue directory */
					move_check_result_to_queue(check_result_info.output_file);
					}

				/* free memory */
				dbuf_free(&checkresult_dbuf);

				/* free check result memory */
				free_check_result(&check_result_info);

				/* return with plugin exit status - not really necessary... */
				_exit(pclose_result);
				}
#endif
			/******** END EMBEDDED PERL INTERPRETER EXECUTION ********/


			/* run the plugin check command */
			fp = popen(processed_command, "r");
			if(fp == NULL)
				_exit(STATE_UNKNOWN);

			/* initialize buffer */
			strcpy(output_buffer, "");

			/* get all lines of plugin output - escape newlines */
			while(fgets(output_buffer, sizeof(output_buffer) - 1, fp)) {
				temp_buffer = escape_newlines(output_buffer);
				dbuf_strcat(&checkresult_dbuf, temp_buffer);
				my_free(temp_buffer);
				}

			/* close the process */
			pclose_result = pclose(fp);

			/* reset the alarm and ignore SIGALRM */
			signal(SIGALRM, SIG_IGN);
			alarm(0);

			/* get the check finish time */
			gettimeofday(&end_time, NULL);

			/* record check result info */
			check_result_info.finish_time = end_time;
			check_result_info.early_timeout = FALSE;

			/* test for execution error */
			if(pclose_result == -1) {
				pclose_result = STATE_UNKNOWN;
				check_result_info.return_code = STATE_CRITICAL;
				check_result_info.exited_ok = FALSE;
				}
			else {
				if(WEXITSTATUS(pclose_result) == 0 && WIFSIGNALED(pclose_result))
					check_result_info.return_code = 128 + WTERMSIG(pclose_result);
				else
					check_result_info.return_code = WEXITSTATUS(pclose_result);
				}

			/* write check result to file */
			if(check_result_info.output_file_fp) {
				FILE *fp;

				/* avoid races with signal handling */
				fp = check_result_info.output_file_fp;
				check_result_info.output_file_fp = NULL;

				fprintf(fp, "finish_time=%lu.%lu\n", check_result_info.finish_time.tv_sec, check_result_info.finish_time.tv_usec);
				fprintf(fp, "early_timeout=%d\n", check_result_info.early_timeout);
				fprintf(fp, "exited_ok=%d\n", check_result_info.exited_ok);
				fprintf(fp, "return_code=%d\n", check_result_info.return_code);
				fprintf(fp, "output=%s\n", (checkresult_dbuf.buf == NULL) ? "(null)" : checkresult_dbuf.buf);

				/* close the temp file */
				fclose(fp);

				/* move check result to queue directory */
				move_check_result_to_queue(check_result_info.output_file);
				}

			/* free memory */
			dbuf_free(&checkresult_dbuf);
			my_free(processed_command);

			/* free check result memory */
			free_check_result(&check_result_info);

			/* return with plugin exit status - not really necessary... */
			_exit(pclose_result);
			}

		/* NOTE: this code is never reached if large install tweaks are enabled... */

		/* unset environment variables */
		set_all_macro_environment_vars_r(&mac, FALSE);

		/* free allocated memory */
		/* this needs to be done last, so we don't free memory for variables before they're used above */
		if(free_child_process_memory == TRUE)
			free_memory(&mac);

		/* parent exits immediately - grandchild process is inherited by the INIT process, so we have no zombie problem... */
		_exit(STATE_OK);
		}

	/* else the parent should wait for the first child to return... */
	else if(pid > 0) {
		clear_volatile_macros_r(&mac);

		log_debug_info(DEBUGL_CHECKS, 2, "Service check is executing in child process (pid=%lu)\n", (unsigned long)pid);

		/* parent should close output file */
		if(check_result_info.output_file_fp)
			fclose(check_result_info.output_file_fp);

		/* should this be done in first child process (after spawning grandchild) as well? */
		/* free memory allocated for IPC functionality */
		free_check_result(&check_result_info);

		/* free memory */
		my_free(processed_command);

		/* wait for the first child to return */
		/* don't do this if large install tweaks are enabled - we'll clean up children in event loop */
		if(child_processes_fork_twice == TRUE)
			wait_result = waitpid(pid, NULL, 0);
		}

	/* see if we were able to run the check... */
	if(fork_error == TRUE)
		return ERROR;

	return OK;
	}



/* handles asynchronous service check results */
int handle_async_service_check_result(service *temp_service, check_result *queued_check_result) {
	host *temp_host = NULL;
	time_t next_service_check = 0L;
	time_t preferred_time = 0L;
	time_t next_valid_time = 0L;
	int reschedule_check = FALSE;
	int state_change = FALSE;
	int hard_state_change = FALSE;
	int first_host_check_initiated = FALSE;
	int route_result = HOST_UP;
	time_t current_time = 0L;
	int state_was_logged = FALSE;
	char *old_plugin_output = NULL;
	char *temp_plugin_output = NULL;
	char *temp_ptr = NULL;
	servicedependency *temp_dependency = NULL;
	objectlist *check_servicelist = NULL;
	objectlist *servicelist_item = NULL;
	service *master_service = NULL;
	int run_async_check = TRUE;
	int state_changes_use_cached_state = TRUE; /* TODO - 09/23/07 move this to a global variable */
	int flapping_check_done = FALSE;
	void *ptr = NULL;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "handle_async_service_check_result()\n");

	/* make sure we have what we need */
	if(temp_service == NULL || queued_check_result == NULL)
		return ERROR;

	/* get the current time */
	time(&current_time);

	log_debug_info(DEBUGL_CHECKS, 0, "** Handling check result for service '%s' on host '%s'...\n", temp_service->description, temp_service->host_name);
	log_debug_info(DEBUGL_CHECKS, 1, "HOST: %s, SERVICE: %s, CHECK TYPE: %s, OPTIONS: %d, SCHEDULED: %s, RESCHEDULE: %s, EXITED OK: %s, RETURN CODE: %d, OUTPUT: %s\n", temp_service->host_name, temp_service->description, (queued_check_result->check_type == SERVICE_CHECK_ACTIVE) ? "Active" : "Passive", queued_check_result->check_options, (queued_check_result->scheduled_check == TRUE) ? "Yes" : "No", (queued_check_result->reschedule_check == TRUE) ? "Yes" : "No", (queued_check_result->exited_ok == TRUE) ? "Yes" : "No", queued_check_result->return_code, queued_check_result->output);

	/* decrement the number of service checks still out there... */
	if(queued_check_result->check_type == SERVICE_CHECK_ACTIVE && currently_running_service_checks > 0)
		currently_running_service_checks--;

	/* skip this service check results if its passive and we aren't accepting passive check results */
	if(queued_check_result->check_type == SERVICE_CHECK_PASSIVE) {
		if(accept_passive_service_checks == FALSE) {
			log_debug_info(DEBUGL_CHECKS, 0, "Discarding passive service check result because passive service checks are disabled globally.\n");
			return ERROR;
			}
		if(temp_service->accept_passive_service_checks == FALSE) {
			log_debug_info(DEBUGL_CHECKS, 0, "Discarding passive service check result because passive checks are disabled for this service.\n");
			return ERROR;
			}
		}

	/* clear the freshening flag (it would have been set if this service was determined to be stale) */
	if(queued_check_result->check_options & CHECK_OPTION_FRESHNESS_CHECK)
		temp_service->is_being_freshened = FALSE;

	/* clear the execution flag if this was an active check */
	if(queued_check_result->check_type == SERVICE_CHECK_ACTIVE)
		temp_service->is_executing = FALSE;

	/* DISCARD INVALID FRESHNESS CHECK RESULTS */
	/* If a services goes stale, Nagios will initiate a forced check in order to freshen it.  There is a race condition whereby a passive check
	   could arrive between the 1) initiation of the forced check and 2) the time when the forced check result is processed here.  This would
	   make the service fresh again, so we do a quick check to make sure the service is still stale before we accept the check result. */
	if((queued_check_result->check_options & CHECK_OPTION_FRESHNESS_CHECK) && is_service_result_fresh(temp_service, current_time, FALSE) == TRUE) {
		log_debug_info(DEBUGL_CHECKS, 0, "Discarding service freshness check result because the service is currently fresh (race condition avoided).\n");
		return OK;
		}

	/* check latency is passed to us */
	temp_service->latency = queued_check_result->latency;

	/* update the execution time for this check (millisecond resolution) */
	temp_service->execution_time = (double)((double)(queued_check_result->finish_time.tv_sec - queued_check_result->start_time.tv_sec) + (double)((queued_check_result->finish_time.tv_usec - queued_check_result->start_time.tv_usec) / 1000.0) / 1000.0);
	if(temp_service->execution_time < 0.0)
		temp_service->execution_time = 0.0;

	/* get the last check time */
	temp_service->last_check = queued_check_result->start_time.tv_sec;

	/* was this check passive or active? */
	temp_service->check_type = (queued_check_result->check_type == SERVICE_CHECK_ACTIVE) ? SERVICE_CHECK_ACTIVE : SERVICE_CHECK_PASSIVE;

	/* update check statistics for passive checks */
	if(queued_check_result->check_type == SERVICE_CHECK_PASSIVE)
		update_check_stats(PASSIVE_SERVICE_CHECK_STATS, queued_check_result->start_time.tv_sec);

	/* should we reschedule the next service check? NOTE: This may be overridden later... */
	reschedule_check = queued_check_result->reschedule_check;

	/* save the old service status info */
	temp_service->last_state = temp_service->current_state;

	/* save old plugin output */
	if(temp_service->plugin_output)
		old_plugin_output = (char *)strdup(temp_service->plugin_output);

	/* clear the old plugin output and perf data buffers */
	my_free(temp_service->plugin_output);
	my_free(temp_service->long_plugin_output);
	my_free(temp_service->perf_data);

	/* if there was some error running the command, just skip it (this shouldn't be happening) */
	if(queued_check_result->exited_ok == FALSE) {

		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning:  Check of service '%s' on host '%s' did not exit properly!\n", temp_service->description, temp_service->host_name);

		temp_service->plugin_output = (char *)strdup("(Service check did not exit properly)");

		temp_service->current_state = STATE_CRITICAL;
		}

	/* make sure the return code is within bounds */
	else if(queued_check_result->return_code < 0 || queued_check_result->return_code > 3) {

		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Return code of %d for check of service '%s' on host '%s' was out of bounds.%s\n", queued_check_result->return_code, temp_service->description, temp_service->host_name, (queued_check_result->return_code == 126 ? "Make sure the plugin you're trying to run is executable." : (queued_check_result->return_code == 127 ? " Make sure the plugin you're trying to run actually exists." : "")));

		asprintf(&temp_plugin_output, "\x73\x6f\x69\x67\x61\x6e\x20\x74\x68\x67\x69\x72\x79\x70\x6f\x63\x20\x6e\x61\x68\x74\x65\x20\x64\x61\x74\x73\x6c\x61\x67");
		my_free(temp_plugin_output);
		asprintf(&temp_service->plugin_output, "(Return code of %d is out of bounds%s)", queued_check_result->return_code, (queued_check_result->return_code == 126 ? " - plugin may not be executable" : (queued_check_result->return_code == 127 ? " - plugin may be missing" : "")));

		temp_service->current_state = STATE_CRITICAL;
		}

	/* else the return code is okay... */
	else {

		/* parse check output to get: (1) short output, (2) long output, (3) perf data */
		parse_check_output(queued_check_result->output, &temp_service->plugin_output, &temp_service->long_plugin_output, &temp_service->perf_data, TRUE, TRUE);

		/* make sure the plugin output isn't null */
		if(temp_service->plugin_output == NULL)
			temp_service->plugin_output = (char *)strdup("(No output returned from plugin)");

		/* replace semicolons in plugin output (but not performance data) with colons */
		else if((temp_ptr = temp_service->plugin_output)) {
			while((temp_ptr = strchr(temp_ptr, ';')))
				* temp_ptr = ':';
			}

		log_debug_info(DEBUGL_CHECKS, 2, "Parsing check output...\n");
		log_debug_info(DEBUGL_CHECKS, 2, "Short Output: %s\n", (temp_service->plugin_output == NULL) ? "NULL" : temp_service->plugin_output);
		log_debug_info(DEBUGL_CHECKS, 2, "Long Output:  %s\n", (temp_service->long_plugin_output == NULL) ? "NULL" : temp_service->long_plugin_output);
		log_debug_info(DEBUGL_CHECKS, 2, "Perf Data:    %s\n", (temp_service->perf_data == NULL) ? "NULL" : temp_service->perf_data);

		/* grab the return code */
		temp_service->current_state = queued_check_result->return_code;
		}


	/* record the last state time */
	switch(temp_service->current_state) {
		case STATE_OK:
			temp_service->last_time_ok = temp_service->last_check;
			break;
		case STATE_WARNING:
			temp_service->last_time_warning = temp_service->last_check;
			break;
		case STATE_UNKNOWN:
			temp_service->last_time_unknown = temp_service->last_check;
			break;
		case STATE_CRITICAL:
			temp_service->last_time_critical = temp_service->last_check;
			break;
		default:
			break;
		}

	/* log passive checks - we need to do this here, as some my bypass external commands by getting dropped in checkresults dir */
	if(temp_service->check_type == SERVICE_CHECK_PASSIVE) {
		if(log_passive_checks == TRUE)
			logit(NSLOG_PASSIVE_CHECK, FALSE, "PASSIVE SERVICE CHECK: %s;%s;%d;%s\n", temp_service->host_name, temp_service->description, temp_service->current_state, temp_service->plugin_output);
		}

	/* get the host that this service runs on */
	temp_host = (host *)temp_service->host_ptr;

	/* if the service check was okay... */
	if(temp_service->current_state == STATE_OK) {

		/* if the host has never been checked before, verify its status */
		/* only do this if 1) the initial state was set to non-UP or 2) the host is not scheduled to be checked soon (next 5 minutes) */
		if(temp_host->has_been_checked == FALSE && (temp_host->initial_state != HOST_UP || (unsigned long)temp_host->next_check == 0L || (unsigned long)(temp_host->next_check - current_time) > 300)) {

			/* set a flag to remember that we launched a check */
			first_host_check_initiated = TRUE;

			/* 08/04/07 EG launch an async (parallel) host check unless aggressive host checking is enabled */
			/* previous logic was to simply run a sync (serial) host check */
			/* do NOT allow cached check results to happen here - we need the host to be checked for real... */
			if(use_aggressive_host_checking == TRUE)
				perform_on_demand_host_check(temp_host, NULL, CHECK_OPTION_NONE, FALSE, 0L);
			else
				run_async_host_check_3x(temp_host, CHECK_OPTION_NONE, 0.0, FALSE, FALSE, NULL, NULL);
			}
		}


	/**** NOTE - THIS WAS MOVED UP FROM LINE 1049 BELOW TO FIX PROBLEMS WHERE CURRENT ATTEMPT VALUE WAS ACTUALLY "LEADING" REAL VALUE ****/
	/* increment the current attempt number if this is a soft state (service was rechecked) */
	if(temp_service->state_type == SOFT_STATE && (temp_service->current_attempt < temp_service->max_attempts))
		temp_service->current_attempt = temp_service->current_attempt + 1;


	log_debug_info(DEBUGL_CHECKS, 2, "ST: %s  CA: %d  MA: %d  CS: %d  LS: %d  LHS: %d\n", (temp_service->state_type == SOFT_STATE) ? "SOFT" : "HARD", temp_service->current_attempt, temp_service->max_attempts, temp_service->current_state, temp_service->last_state, temp_service->last_hard_state);

	/* check for a state change (either soft or hard) */
	if(temp_service->current_state != temp_service->last_state) {
		log_debug_info(DEBUGL_CHECKS, 2, "Service has changed state since last check!\n");
		state_change = TRUE;
		}

	/* checks for a hard state change where host was down at last service check */
	/* this occurs in the case where host goes down and service current attempt gets reset to 1 */
	/* if this check is not made, the service recovery looks like a soft recovery instead of a hard one */
	if(temp_service->host_problem_at_last_check == TRUE && temp_service->current_state == STATE_OK) {
		log_debug_info(DEBUGL_CHECKS, 2, "Service had a HARD STATE CHANGE!!\n");
		hard_state_change = TRUE;
		}

	/* check for a "normal" hard state change where max check attempts is reached */
	if(temp_service->current_attempt >= temp_service->max_attempts && temp_service->current_state != temp_service->last_hard_state) {
		log_debug_info(DEBUGL_CHECKS, 2, "Service had a HARD STATE CHANGE!!\n");
		hard_state_change = TRUE;
		}

	/* a state change occurred... */
	/* reset last and next notification times and acknowledgement flag if necessary, misc other stuff */
	if(state_change == TRUE || hard_state_change == TRUE) {

		/* reschedule the service check */
		reschedule_check = TRUE;

		/* reset notification times */
		temp_service->last_notification = (time_t)0;
		temp_service->next_notification = (time_t)0;

		/* reset notification suppression option */
		temp_service->no_more_notifications = FALSE;

		if(temp_service->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL && (state_change == TRUE || hard_state_change == FALSE)) {

			temp_service->problem_has_been_acknowledged = FALSE;
			temp_service->acknowledgement_type = ACKNOWLEDGEMENT_NONE;

			/* remove any non-persistant comments associated with the ack */
			delete_service_acknowledgement_comments(temp_service);
			}
		else if(temp_service->acknowledgement_type == ACKNOWLEDGEMENT_STICKY && temp_service->current_state == STATE_OK) {

			temp_service->problem_has_been_acknowledged = FALSE;
			temp_service->acknowledgement_type = ACKNOWLEDGEMENT_NONE;

			/* remove any non-persistant comments associated with the ack */
			delete_service_acknowledgement_comments(temp_service);
			}

		/* do NOT reset current notification number!!! */
		/* hard changes between non-OK states should continue to be escalated, so don't reset current notification number */
		/*temp_service->current_notification_number=0;*/
		}

	/* initialize the last host and service state change times if necessary */
	if(temp_service->last_state_change == (time_t)0)
		temp_service->last_state_change = temp_service->last_check;
	if(temp_service->last_hard_state_change == (time_t)0)
		temp_service->last_hard_state_change = temp_service->last_check;
	if(temp_host->last_state_change == (time_t)0)
		temp_host->last_state_change = temp_service->last_check;
	if(temp_host->last_hard_state_change == (time_t)0)
		temp_host->last_hard_state_change = temp_service->last_check;

	/* update last service state change times */
	if(state_change == TRUE)
		temp_service->last_state_change = temp_service->last_check;
	if(hard_state_change == TRUE)
		temp_service->last_hard_state_change = temp_service->last_check;

	/* update the event and problem ids */
	if(state_change == TRUE) {

		/* always update the event id on a state change */
		temp_service->last_event_id = temp_service->current_event_id;
		temp_service->current_event_id = next_event_id;
		next_event_id++;

		/* update the problem id when transitioning to a problem state */
		if(temp_service->last_state == STATE_OK) {
			/* don't reset last problem id, or it will be zero the next time a problem is encountered */
			/* temp_service->last_problem_id=temp_service->current_problem_id;*/
			temp_service->current_problem_id = next_problem_id;
			next_problem_id++;
			}

		/* clear the problem id when transitioning from a problem state to an OK state */
		if(temp_service->current_state == STATE_OK) {
			temp_service->last_problem_id = temp_service->current_problem_id;
			temp_service->current_problem_id = 0L;
			}
		}


	/**************************************/
	/******* SERVICE CHECK OK LOGIC *******/
	/**************************************/

	/* if the service is up and running OK... */
	if(temp_service->current_state == STATE_OK) {

		log_debug_info(DEBUGL_CHECKS, 1, "Service is OK.\n");

		/* reset the acknowledgement flag (this should already have been done, but just in case...) */
		temp_service->problem_has_been_acknowledged = FALSE;
		temp_service->acknowledgement_type = ACKNOWLEDGEMENT_NONE;

		/* verify the route to the host and send out host recovery notifications */
		if(temp_host->current_state != HOST_UP) {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is NOT UP, so we'll check it to see if it recovered...\n");

			/* 08/04/07 EG launch an async (parallel) host check (possibly cached) unless aggressive host checking is enabled */
			/* previous logic was to simply run a sync (serial) host check */
			if(use_aggressive_host_checking == TRUE)
				perform_on_demand_host_check(temp_host, NULL, CHECK_OPTION_NONE, TRUE, cached_host_check_horizon);
			/* 09/23/07 EG don't launch a new host check if we already did so earlier */
			else if(first_host_check_initiated == TRUE)
				log_debug_info(DEBUGL_CHECKS, 1, "First host check was already initiated, so we'll skip a new host check.\n");
			else {
				/* can we use the last cached host state? */
				/* usually only use cached host state if no service state change has occurred */
				if((state_change == FALSE || state_changes_use_cached_state == TRUE) && temp_host->has_been_checked == TRUE && ((current_time - temp_host->last_check) <= cached_host_check_horizon)) {
					log_debug_info(DEBUGL_CHECKS, 1, "* Using cached host state: %d\n", temp_host->current_state);
					update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, current_time);
					update_check_stats(ACTIVE_CACHED_HOST_CHECK_STATS, current_time);
					}

				/* else launch an async (parallel) check of the host */
				else
					run_async_host_check_3x(temp_host, CHECK_OPTION_NONE, 0.0, FALSE, FALSE, NULL, NULL);
				}
			}

		/* if a hard service recovery has occurred... */
		if(hard_state_change == TRUE) {

			log_debug_info(DEBUGL_CHECKS, 1, "Service experienced a HARD RECOVERY.\n");

			/* set the state type macro */
			temp_service->state_type = HARD_STATE;

			/* log the service recovery */
			log_service_event(temp_service);
			state_was_logged = TRUE;

			/* 10/04/07 check to see if the service and/or associate host is flapping */
			/* this should be done before a notification is sent out to ensure the host didn't just start flapping */
			check_for_service_flapping(temp_service, TRUE, TRUE);
			check_for_host_flapping(temp_host, TRUE, FALSE, TRUE);
			flapping_check_done = TRUE;

			/* notify contacts about the service recovery */
			service_notification(temp_service, NOTIFICATION_NORMAL, NULL, NULL, NOTIFICATION_OPTION_NONE);

			/* run the service event handler to handle the hard state change */
			handle_service_event(temp_service);
			}

		/* else if a soft service recovery has occurred... */
		else if(state_change == TRUE) {

			log_debug_info(DEBUGL_CHECKS, 1, "Service experienced a SOFT RECOVERY.\n");

			/* this is a soft recovery */
			temp_service->state_type = SOFT_STATE;

			/* log the soft recovery */
			log_service_event(temp_service);
			state_was_logged = TRUE;

			/* run the service event handler to handle the soft state change */
			handle_service_event(temp_service);
			}

		/* else no service state change has occurred... */
		else {
			log_debug_info(DEBUGL_CHECKS, 1, "Service did not change state.\n");
			}

		/* should we obsessive over service checks? */
		if(obsess_over_services == TRUE)
			obsessive_compulsive_service_check_processor(temp_service);

		/* reset all service variables because its okay now... */
		temp_service->host_problem_at_last_check = FALSE;
		temp_service->current_attempt = 1;
		temp_service->state_type = HARD_STATE;
		temp_service->last_hard_state = STATE_OK;
		temp_service->last_notification = (time_t)0;
		temp_service->next_notification = (time_t)0;
		temp_service->current_notification_number = 0;
		temp_service->problem_has_been_acknowledged = FALSE;
		temp_service->acknowledgement_type = ACKNOWLEDGEMENT_NONE;
		temp_service->notified_on_unknown = FALSE;
		temp_service->notified_on_warning = FALSE;
		temp_service->notified_on_critical = FALSE;
		temp_service->no_more_notifications = FALSE;

		if(reschedule_check == TRUE)
			next_service_check = (time_t)(temp_service->last_check + (temp_service->check_interval * interval_length));
		}


	/*******************************************/
	/******* SERVICE CHECK PROBLEM LOGIC *******/
	/*******************************************/

	/* hey, something's not working quite like it should... */
	else {

		log_debug_info(DEBUGL_CHECKS, 1, "Service is in a non-OK state!\n");

		/* check the route to the host if its up right now... */
		if(temp_host->current_state == HOST_UP) {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is currently UP, so we'll recheck its state to make sure...\n");

			/* 08/04/07 EG launch an async (parallel) host check (possibly cached) unless aggressive host checking is enabled */
			/* previous logic was to simply run a sync (serial) host check */
			if(use_aggressive_host_checking == TRUE)
				perform_on_demand_host_check(temp_host, &route_result, CHECK_OPTION_NONE, TRUE, cached_host_check_horizon);
			else {
				/* can we use the last cached host state? */
				/* only use cached host state if no service state change has occurred */
				if((state_change == FALSE || state_changes_use_cached_state == TRUE) && temp_host->has_been_checked == TRUE && ((current_time - temp_host->last_check) <= cached_host_check_horizon)) {
					/* use current host state as route result */
					route_result = temp_host->current_state;
					log_debug_info(DEBUGL_CHECKS, 1, "* Using cached host state: %d\n", temp_host->current_state);
					update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, current_time);
					update_check_stats(ACTIVE_CACHED_HOST_CHECK_STATS, current_time);
					}

				/* else launch an async (parallel) check of the host */
				/* CHANGED 02/15/08 only if service changed state since service was last checked */
				else if(state_change == TRUE) {
					/* use current host state as route result */
					route_result = temp_host->current_state;
					run_async_host_check_3x(temp_host, CHECK_OPTION_NONE, 0.0, FALSE, FALSE, NULL, NULL);
					}

				/* ADDED 02/15/08 */
				/* else assume same host state */
				else {
					route_result = temp_host->current_state;
					log_debug_info(DEBUGL_CHECKS, 1, "* Using last known host state: %d\n", temp_host->current_state);
					update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, current_time);
					update_check_stats(ACTIVE_CACHED_HOST_CHECK_STATS, current_time);
					}
				}
			}

		/* else the host is either down or unreachable, so recheck it if necessary */
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is currently DOWN/UNREACHABLE.\n");

			/* we're using aggressive host checking, so really do recheck the host... */
			if(use_aggressive_host_checking == TRUE) {
				log_debug_info(DEBUGL_CHECKS, 1, "Agressive host checking is enabled, so we'll recheck the host state...\n");
				perform_on_demand_host_check(temp_host, &route_result, CHECK_OPTION_NONE, TRUE, cached_host_check_horizon);
				}

			/* the service wobbled between non-OK states, so check the host... */
			else if((state_change == TRUE && state_changes_use_cached_state == FALSE) && temp_service->last_hard_state != STATE_OK) {
				log_debug_info(DEBUGL_CHECKS, 1, "Service wobbled between non-OK states, so we'll recheck the host state...\n");
				/* 08/04/07 EG launch an async (parallel) host check unless aggressive host checking is enabled */
				/* previous logic was to simply run a sync (serial) host check */
				/* use current host state as route result */
				route_result = temp_host->current_state;
				run_async_host_check_3x(temp_host, CHECK_OPTION_NONE, 0.0, FALSE, FALSE, NULL, NULL);
				/*perform_on_demand_host_check(temp_host,&route_result,CHECK_OPTION_NONE,TRUE,cached_host_check_horizon);*/
				}

			/* else fake the host check, but (possibly) resend host notifications to contacts... */
			else {

				log_debug_info(DEBUGL_CHECKS, 1, "Assuming host is in same state as before...\n");

				/* if the host has never been checked before, set the checked flag and last check time */
				/* 03/11/06 EG Note: This probably never evaluates to FALSE, present for historical reasons only, can probably be removed in the future */
				if(temp_host->has_been_checked == FALSE) {
					temp_host->has_been_checked = TRUE;
					temp_host->last_check = temp_service->last_check;
					}

				/* fake the route check result */
				route_result = temp_host->current_state;

				/* possibly re-send host notifications... */
				host_notification(temp_host, NOTIFICATION_NORMAL, NULL, NULL, NOTIFICATION_OPTION_NONE);
				}
			}

		/* if the host is down or unreachable ... */
		/* 05/29/2007 NOTE: The host might be in a SOFT problem state due to host check retries/caching.  Not sure if we should take that into account and do something different or not... */
		if(route_result != HOST_UP) {

			log_debug_info(DEBUGL_CHECKS, 2, "Host is not UP, so we mark state changes if appropriate\n");

			/* "fake" a hard state change for the service - well, its not really fake, but it didn't get caught earlier... */
			if(temp_service->last_hard_state != temp_service->current_state)
				hard_state_change = TRUE;

			/* update last state change times */
			if(state_change == TRUE || hard_state_change == TRUE)
				temp_service->last_state_change = temp_service->last_check;
			if(hard_state_change == TRUE) {
				temp_service->last_hard_state_change = temp_service->last_check;
				temp_service->state_type = HARD_STATE;
				temp_service->last_hard_state = temp_service->current_state;
				}

			/* put service into a hard state without attempting check retries and don't send out notifications about it */
			temp_service->host_problem_at_last_check = TRUE;
			/* Below removed 08/04/2010 EG - http://tracker.nagios.org/view.php?id=128 */
			/*
			temp_service->state_type=HARD_STATE;
			temp_service->last_hard_state=temp_service->current_state;
			temp_service->current_attempt=1;
			*/
			}

		/* the host is up - it recovered since the last time the service was checked... */
		else if(temp_service->host_problem_at_last_check == TRUE) {

			/* next time the service is checked we shouldn't get into this same case... */
			temp_service->host_problem_at_last_check = FALSE;

			/* reset the current check counter, so we give the service a chance */
			/* this helps prevent the case where service has N max check attempts, N-1 of which have already occurred. */
			/* if we didn't do this, the next check might fail and result in a hard problem - we should really give it more time */
			/* ADDED IF STATEMENT 01-17-05 EG */
			/* 01-17-05: Services in hard problem states before hosts went down would sometimes come back as soft problem states after */
			/* the hosts recovered.  This caused problems, so hopefully this will fix it */
			if(temp_service->state_type == SOFT_STATE)
				temp_service->current_attempt = 1;
			}

		log_debug_info(DEBUGL_CHECKS, 1, "Current/Max Attempt(s): %d/%d\n", temp_service->current_attempt, temp_service->max_attempts);

		/* if we should retry the service check, do so (except it the host is down or unreachable!) */
		if(temp_service->current_attempt < temp_service->max_attempts) {

			/* the host is down or unreachable, so don't attempt to retry the service check */
			if(route_result != HOST_UP) {

				log_debug_info(DEBUGL_CHECKS, 1, "Host isn't UP, so we won't retry the service check...\n");

				/* the host is not up, so reschedule the next service check at regular interval */
				if(reschedule_check == TRUE)
					next_service_check = (time_t)(temp_service->last_check + (temp_service->check_interval * interval_length));

				/* log the problem as a hard state if the host just went down */
				if(hard_state_change == TRUE) {
					log_service_event(temp_service);
					state_was_logged = TRUE;

					/* run the service event handler to handle the hard state */
					handle_service_event(temp_service);
					}
				}

			/* the host is up, so continue to retry the service check */
			else {

				log_debug_info(DEBUGL_CHECKS, 1, "Host is UP, so we'll retry the service check...\n");

				/* this is a soft state */
				temp_service->state_type = SOFT_STATE;

				/* log the service check retry */
				log_service_event(temp_service);
				state_was_logged = TRUE;

				/* run the service event handler to handle the soft state */
				handle_service_event(temp_service);

				if(reschedule_check == TRUE)
					next_service_check = (time_t)(temp_service->last_check + (temp_service->retry_interval * interval_length));
				}

			/* perform dependency checks on the second to last check of the service */
			if(enable_predictive_service_dependency_checks == TRUE && temp_service->current_attempt == (temp_service->max_attempts - 1)) {

				log_debug_info(DEBUGL_CHECKS, 1, "Looking for services to check for predictive dependency checks...\n");

				/* check services that THIS ONE depends on for notification AND execution */
				/* we do this because we might be sending out a notification soon and we want the dependency logic to be accurate */
				for(temp_dependency = get_first_servicedependency_by_dependent_service(temp_service->host_name, temp_service->description, &ptr); temp_dependency != NULL; temp_dependency = get_next_servicedependency_by_dependent_service(temp_service->host_name, temp_service->description, &ptr)) {
					if(temp_dependency->dependent_service_ptr == temp_service && temp_dependency->master_service_ptr != NULL) {
						master_service = (service *)temp_dependency->master_service_ptr;
						log_debug_info(DEBUGL_CHECKS, 2, "Predictive check of service '%s' on host '%s' queued.\n", master_service->description, master_service->host_name);
						add_object_to_objectlist(&check_servicelist, (void *)master_service);
						}
					}
				}
			}


		/* we've reached the maximum number of service rechecks, so handle the error */
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Service has reached max number of rechecks, so we'll handle the error...\n");

			/* this is a hard state */
			temp_service->state_type = HARD_STATE;

			/* if we've hard a hard state change... */
			if(hard_state_change == TRUE) {

				/* log the service problem (even if host is not up, which is new in 0.0.5) */
				log_service_event(temp_service);
				state_was_logged = TRUE;
				}

			/* else log the problem (again) if this service is flagged as being volatile */
			else if(temp_service->is_volatile == TRUE) {
				log_service_event(temp_service);
				state_was_logged = TRUE;
				}

			/* check for start of flexible (non-fixed) scheduled downtime if we just had a hard error */
			/* we need to check for both, state_change (SOFT) and hard_state_change (HARD) values */
			if((hard_state_change == TRUE || state_change == TRUE) && temp_service->pending_flex_downtime > 0)
				check_pending_flex_service_downtime(temp_service);

			/* 10/04/07 check to see if the service and/or associate host is flapping */
			/* this should be done before a notification is sent out to ensure the host didn't just start flapping */
			check_for_service_flapping(temp_service, TRUE, TRUE);
			check_for_host_flapping(temp_host, TRUE, FALSE, TRUE);
			flapping_check_done = TRUE;

			/* (re)send notifications out about this service problem if the host is up (and was at last check also) and the dependencies were okay... */
			service_notification(temp_service, NOTIFICATION_NORMAL, NULL, NULL, NOTIFICATION_OPTION_NONE);

			/* run the service event handler if we changed state from the last hard state or if this service is flagged as being volatile */
			if(hard_state_change == TRUE || temp_service->is_volatile == TRUE)
				handle_service_event(temp_service);

			/* save the last hard state */
			temp_service->last_hard_state = temp_service->current_state;

			/* reschedule the next check at the regular interval */
			if(reschedule_check == TRUE)
				next_service_check = (time_t)(temp_service->last_check + (temp_service->check_interval * interval_length));
			}


		/* should we obsessive over service checks? */
		if(obsess_over_services == TRUE)
			obsessive_compulsive_service_check_processor(temp_service);
		}

	/* reschedule the next service check ONLY for active, scheduled checks */
	if(reschedule_check == TRUE) {

		log_debug_info(DEBUGL_CHECKS, 1, "Rescheduling next check of service at %s", ctime(&next_service_check));

		/* default is to reschedule service check unless a test below fails... */
		temp_service->should_be_scheduled = TRUE;

		/* next check time was calculated above */
		temp_service->next_check = next_service_check;

		/* make sure we don't get ourselves into too much trouble... */
		if(current_time > temp_service->next_check)
			temp_service->next_check = current_time;

		/* make sure we rescheduled the next service check at a valid time */
		preferred_time = temp_service->next_check;
		get_next_valid_time(preferred_time, &next_valid_time, temp_service->check_period_ptr);
		temp_service->next_check = next_valid_time;

		/* services with non-recurring intervals do not get rescheduled */
		if(temp_service->check_interval == 0)
			temp_service->should_be_scheduled = FALSE;

		/* services with active checks disabled do not get rescheduled */
		if(temp_service->checks_enabled == FALSE)
			temp_service->should_be_scheduled = FALSE;

		/* schedule a non-forced check if we can */
		if(temp_service->should_be_scheduled == TRUE)
			schedule_service_check(temp_service, temp_service->next_check, CHECK_OPTION_NONE);
		}

	/* if we're stalking this state type and state was not already logged AND the plugin output changed since last check, log it now.. */
	if(temp_service->state_type == HARD_STATE && state_change == FALSE && state_was_logged == FALSE && compare_strings(old_plugin_output, temp_service->plugin_output)) {

		if((temp_service->current_state == STATE_OK && temp_service->stalk_on_ok == TRUE))
			log_service_event(temp_service);

		else if((temp_service->current_state == STATE_WARNING && temp_service->stalk_on_warning == TRUE))
			log_service_event(temp_service);

		else if((temp_service->current_state == STATE_UNKNOWN && temp_service->stalk_on_unknown == TRUE))
			log_service_event(temp_service);

		else if((temp_service->current_state == STATE_CRITICAL && temp_service->stalk_on_critical == TRUE))
			log_service_event(temp_service);
		}

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_service_check(NEBTYPE_SERVICECHECK_PROCESSED, NEBFLAG_NONE, NEBATTR_NONE, temp_service, temp_service->check_type, queued_check_result->start_time, queued_check_result->finish_time, NULL, temp_service->latency, temp_service->execution_time, service_check_timeout, queued_check_result->early_timeout, queued_check_result->return_code, NULL, NULL);
#endif

	/* set the checked flag */
	temp_service->has_been_checked = TRUE;

	/* update the current service status log */
	update_service_status(temp_service, FALSE);

	/* check to see if the service and/or associate host is flapping */
	if(flapping_check_done == FALSE) {
		check_for_service_flapping(temp_service, TRUE, TRUE);
		check_for_host_flapping(temp_host, TRUE, FALSE, TRUE);
		}

	/* update service performance info */
	update_service_performance_data(temp_service);

	/* free allocated memory */
	my_free(temp_plugin_output);
	my_free(old_plugin_output);


	/* run async checks of all services we added above */
	/* don't run a check if one is already executing or we can get by with a cached state */
	for(servicelist_item = check_servicelist; servicelist_item != NULL; servicelist_item = servicelist_item->next) {
		run_async_check = TRUE;
		temp_service = (service *)servicelist_item->object_ptr;

		/* we can get by with a cached state, so don't check the service */
		if((current_time - temp_service->last_check) <= cached_service_check_horizon) {
			run_async_check = FALSE;

			/* update check statistics */
			update_check_stats(ACTIVE_CACHED_SERVICE_CHECK_STATS, current_time);
			}

		if(temp_service->is_executing == TRUE)
			run_async_check = FALSE;

		if(run_async_check == TRUE)
			run_async_service_check(temp_service, CHECK_OPTION_NONE, 0.0, FALSE, FALSE, NULL, NULL);
		}
	free_objectlist(&check_servicelist);

	return OK;
	}



/* schedules an immediate or delayed service check */
void schedule_service_check(service *svc, time_t check_time, int options) {
	timed_event *temp_event = NULL;
	timed_event *new_event = NULL;
	int use_original_event = TRUE;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "schedule_service_check()\n");

	if(svc == NULL)
		return;

	log_debug_info(DEBUGL_CHECKS, 0, "Scheduling a %s, active check of service '%s' on host '%s' @ %s", (options & CHECK_OPTION_FORCE_EXECUTION) ? "forced" : "non-forced", svc->description, svc->host_name, ctime(&check_time));

	/* don't schedule a check if active checks of this service are disabled */
	if(svc->checks_enabled == FALSE && !(options & CHECK_OPTION_FORCE_EXECUTION)) {
		log_debug_info(DEBUGL_CHECKS, 0, "Active checks of this service are disabled.\n");
		return;
		}

	/* default is to use the new event */
	use_original_event = FALSE;

	temp_event = (timed_event *)svc->next_check_event;

	/*
	 * If the service already has a check scheduled,
	 * we need to decide which of the events to use
	 */
	if(temp_event != NULL) {

		log_debug_info(DEBUGL_CHECKS, 2, "Found another service check event for this service @ %s", ctime(&temp_event->run_time));

		/* use the originally scheduled check unless we decide otherwise */
		use_original_event = TRUE;

		/* the original event is a forced check... */
		if((temp_event->event_options & CHECK_OPTION_FORCE_EXECUTION)) {

			/* the new event is also forced and its execution time is earlier than the original, so use it instead */
			if((options & CHECK_OPTION_FORCE_EXECUTION) && (check_time < temp_event->run_time)) {
				use_original_event = FALSE;
				log_debug_info(DEBUGL_CHECKS, 2, "New service check event is forced and occurs before the existing event, so the new event will be used instead.\n");
				}
			}

		/* the original event is not a forced check... */
		else {

			/* the new event is a forced check, so use it instead */
			if((options & CHECK_OPTION_FORCE_EXECUTION)) {
				use_original_event = FALSE;
				log_debug_info(DEBUGL_CHECKS, 2, "New service check event is forced, so it will be used instead of the existing event.\n");
				}

			/* the new event is not forced either and its execution time is earlier than the original, so use it instead */
			else if(check_time < temp_event->run_time) {
				use_original_event = FALSE;
				log_debug_info(DEBUGL_CHECKS, 2, "New service check event occurs before the existing (older) event, so it will be used instead.\n");
				}

			/* the new event is older, so override the existing one */
			else {
				log_debug_info(DEBUGL_CHECKS, 2, "New service check event occurs after the existing event, so we'll ignore it.\n");
				}
			}
		}

	/* schedule a new event */
	if(use_original_event == FALSE) {

		/* allocate memory for a new event item */
		new_event = (timed_event *)malloc(sizeof(timed_event));
		if(new_event == NULL) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Could not reschedule check of service '%s' on host '%s'!\n", svc->description, svc->host_name);
			return;
			}

		/* make sure we kill off the old event */
		if(temp_event) {
			remove_event(temp_event, &event_list_low, &event_list_low_tail);
			my_free(temp_event);
			}
		log_debug_info(DEBUGL_CHECKS, 2, "Scheduling new service check event.\n");

		/* set the next service check event and time */
		svc->next_check_event = new_event;
		svc->next_check = check_time;

		/* save check options for retention purposes */
		svc->check_options = options;

		/* place the new event in the event queue */
		new_event->event_type = EVENT_SERVICE_CHECK;
		new_event->event_data = (void *)svc;
		new_event->event_args = (void *)NULL;
		new_event->event_options = options;
		new_event->run_time = svc->next_check;
		new_event->recurring = FALSE;
		new_event->event_interval = 0L;
		new_event->timing_func = NULL;
		new_event->compensate_for_time_change = TRUE;
		reschedule_event(new_event, &event_list_low, &event_list_low_tail);
		}

	else {
		/* reset the next check time (it may be out of sync) */
		if(temp_event != NULL)
			svc->next_check = temp_event->run_time;

		log_debug_info(DEBUGL_CHECKS, 2, "Keeping original service check event (ignoring the new one).\n");
		}


	/* update the status log */
	update_service_status(svc, FALSE);

	return;
	}



/* checks viability of performing a service check */
int check_service_check_viability(service *svc, int check_options, int *time_is_valid, time_t *new_time) {
	int result = OK;
	int perform_check = TRUE;
	time_t current_time = 0L;
	time_t preferred_time = 0L;
	int check_interval = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_service_check_viability()\n");

	/* make sure we have a service */
	if(svc == NULL)
		return ERROR;

	/* get the check interval to use if we need to reschedule the check */
	if(svc->state_type == SOFT_STATE && svc->current_state != STATE_OK)
		check_interval = (svc->retry_interval * interval_length);
	else
		check_interval = (svc->check_interval * interval_length);

	/* get the current time */
	time(&current_time);

	/* initialize the next preferred check time */
	preferred_time = current_time;

	/* can we check the host right now? */
	if(!(check_options & CHECK_OPTION_FORCE_EXECUTION)) {

		/* if checks of the service are currently disabled... */
		if(svc->checks_enabled == FALSE) {
			preferred_time = current_time + check_interval;
			perform_check = FALSE;

			log_debug_info(DEBUGL_CHECKS, 2, "Active checks of the service are currently disabled.\n");
			}

		/* make sure this is a valid time to check the service */
		if(check_time_against_period((unsigned long)current_time, svc->check_period_ptr) == ERROR) {
			preferred_time = current_time;
			if(time_is_valid)
				*time_is_valid = FALSE;
			perform_check = FALSE;

			log_debug_info(DEBUGL_CHECKS, 2, "This is not a valid time for this service to be actively checked.\n");
			}

		/* check service dependencies for execution */
		if(check_service_dependencies(svc, EXECUTION_DEPENDENCY) == DEPENDENCIES_FAILED) {
			preferred_time = current_time + check_interval;
			perform_check = FALSE;

			log_debug_info(DEBUGL_CHECKS, 2, "Execution dependencies for this service failed, so it will not be actively checked.\n");
			}
		}

	/* pass back the next viable check time */
	if(new_time)
		*new_time = preferred_time;

	result = (perform_check == TRUE) ? OK : ERROR;

	return result;
	}



/* checks service dependencies */
int check_service_dependencies(service *svc, int dependency_type) {
	servicedependency *temp_dependency = NULL;
	service *temp_service = NULL;
	int state = STATE_OK;
	time_t current_time = 0L;
	void *ptr = NULL;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_service_dependencies()\n");

	/* check all dependencies... */
	for(temp_dependency = get_first_servicedependency_by_dependent_service(svc->host_name, svc->description, &ptr); temp_dependency != NULL; temp_dependency = get_next_servicedependency_by_dependent_service(svc->host_name, svc->description, &ptr)) {

		/* only check dependencies of the desired type (notification or execution) */
		if(temp_dependency->dependency_type != dependency_type)
			continue;

		/* find the service we depend on... */
		if((temp_service = temp_dependency->master_service_ptr) == NULL)
			continue;

		/* skip this dependency if it has a timeperiod and the current time isn't valid */
		time(&current_time);
		if(temp_dependency->dependency_period != NULL && check_time_against_period(current_time, temp_dependency->dependency_period_ptr) == ERROR)
			return FALSE;

		/* get the status to use (use last hard state if its currently in a soft state) */
		if(temp_service->state_type == SOFT_STATE && soft_state_dependencies == FALSE)
			state = temp_service->last_hard_state;
		else
			state = temp_service->current_state;

		/* is the service we depend on in state that fails the dependency tests? */
		if(state == STATE_OK && temp_dependency->fail_on_ok == TRUE)
			return DEPENDENCIES_FAILED;
		if(state == STATE_WARNING && temp_dependency->fail_on_warning == TRUE)
			return DEPENDENCIES_FAILED;
		if(state == STATE_UNKNOWN && temp_dependency->fail_on_unknown == TRUE)
			return DEPENDENCIES_FAILED;
		if(state == STATE_CRITICAL && temp_dependency->fail_on_critical == TRUE)
			return DEPENDENCIES_FAILED;
		if((state == STATE_OK && temp_service->has_been_checked == FALSE) && temp_dependency->fail_on_pending == TRUE)
			return DEPENDENCIES_FAILED;

		/* immediate dependencies ok at this point - check parent dependencies if necessary */
		if(temp_dependency->inherits_parent == TRUE) {
			if(check_service_dependencies(temp_service, dependency_type) != DEPENDENCIES_OK)
				return DEPENDENCIES_FAILED;
			}
		}

	return DEPENDENCIES_OK;
	}



/* check for services that never returned from a check... */
void check_for_orphaned_services(void) {
	service *temp_service = NULL;
	time_t current_time = 0L;
	time_t expected_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_for_orphaned_services()\n");

	/* get the current time */
	time(&current_time);

	/* check all services... */
	for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

		/* skip services that are not currently executing */
		if(temp_service->is_executing == FALSE)
			continue;

		/* determine the time at which the check results should have come in (allow 10 minutes slack time) */
		expected_time = (time_t)(temp_service->next_check + temp_service->latency + service_check_timeout + check_reaper_interval + 600);

		/* this service was supposed to have executed a while ago, but for some reason the results haven't come back in... */
		if(expected_time < current_time) {

			/* log a warning */
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: The check of service '%s' on host '%s' looks like it was orphaned (results never came back).  I'm scheduling an immediate check of the service...\n", temp_service->description, temp_service->host_name);

			log_debug_info(DEBUGL_CHECKS, 1, "Service '%s' on host '%s' was orphaned, so we're scheduling an immediate check...\n", temp_service->description, temp_service->host_name);

			/* decrement the number of running service checks */
			if(currently_running_service_checks > 0)
				currently_running_service_checks--;

			/* disable the executing flag */
			temp_service->is_executing = FALSE;

			/* schedule an immediate check of the service */
			schedule_service_check(temp_service, current_time, CHECK_OPTION_ORPHAN_CHECK);
			}

		}

	return;
	}



/* check freshness of service results */
void check_service_result_freshness(void) {
	service *temp_service = NULL;
	time_t current_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_service_result_freshness()\n");
	log_debug_info(DEBUGL_CHECKS, 1, "Checking the freshness of service check results...\n");

	/* bail out if we're not supposed to be checking freshness */
	if(check_service_freshness == FALSE) {
		log_debug_info(DEBUGL_CHECKS, 1, "Service freshness checking is disabled.\n");
		return;
		}

	/* get the current time */
	time(&current_time);

	/* check all services... */
	for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

		/* skip services we shouldn't be checking for freshness */
		if(temp_service->check_freshness == FALSE)
			continue;

		/* skip services that are currently executing (problems here will be caught by orphaned service check) */
		if(temp_service->is_executing == TRUE)
			continue;

		/* skip services that have both active and passive checks disabled */
		if(temp_service->checks_enabled == FALSE && temp_service->accept_passive_service_checks == FALSE)
			continue;

		/* skip services that are already being freshened */
		if(temp_service->is_being_freshened == TRUE)
			continue;

		/* see if the time is right... */
		if(check_time_against_period(current_time, temp_service->check_period_ptr) == ERROR)
			continue;

		/* EXCEPTION */
		/* don't check freshness of services without regular check intervals if we're using auto-freshness threshold */
		if(temp_service->check_interval == 0 && temp_service->freshness_threshold == 0)
			continue;

		/* the results for the last check of this service are stale! */
		if(is_service_result_fresh(temp_service, current_time, TRUE) == FALSE) {

			/* set the freshen flag */
			temp_service->is_being_freshened = TRUE;

			/* schedule an immediate forced check of the service */
			schedule_service_check(temp_service, current_time, CHECK_OPTION_FORCE_EXECUTION | CHECK_OPTION_FRESHNESS_CHECK);
			}

		}

	return;
	}



/* tests whether or not a service's check results are fresh */
int is_service_result_fresh(service *temp_service, time_t current_time, int log_this) {
	int freshness_threshold = 0;
	time_t expiration_time = 0L;
	int days = 0;
	int hours = 0;
	int minutes = 0;
	int seconds = 0;
	int tdays = 0;
	int thours = 0;
	int tminutes = 0;
	int tseconds = 0;

	log_debug_info(DEBUGL_CHECKS, 2, "Checking freshness of service '%s' on host '%s'...\n", temp_service->description, temp_service->host_name);

	/* use user-supplied freshness threshold or auto-calculate a freshness threshold to use? */
	if(temp_service->freshness_threshold == 0) {
		if(temp_service->state_type == HARD_STATE || temp_service->current_state == STATE_OK)
			freshness_threshold = (temp_service->check_interval * interval_length) + temp_service->latency + additional_freshness_latency;
		else
			freshness_threshold = (temp_service->retry_interval * interval_length) + temp_service->latency + additional_freshness_latency;
		}
	else
		freshness_threshold = temp_service->freshness_threshold;

	log_debug_info(DEBUGL_CHECKS, 2, "Freshness thresholds: service=%d, use=%d\n", temp_service->freshness_threshold, freshness_threshold);

	/* calculate expiration time */
	/*
	 * CHANGED 11/10/05 EG -
	 * program start is only used in expiration time calculation
	 * if > last check AND active checks are enabled, so active checks
	 * can become stale immediately upon program startup
	 */
	/*
	 * CHANGED 02/25/06 SG -
	 * passive checks also become stale, so remove dependence on active
	 * check logic
	 */
	if(temp_service->has_been_checked == FALSE)
		expiration_time = (time_t)(event_start + freshness_threshold);
	/*
	 * CHANGED 06/19/07 EG -
	 * Per Ton's suggestion (and user requests), only use program start
	 * time over last check if no specific threshold has been set by user.
	 * Problems can occur if Nagios is restarted more frequently that
	 * freshness threshold intervals (services never go stale).
	 */
	/*
	 * CHANGED 10/07/07 EG:
	 * Only match next condition for services that
	 * have active checks enabled...
	 */
	/*
	 * CHANGED 10/07/07 EG:
	 * Added max_service_check_spread to expiration time as suggested
	 * by Altinity
	 */
	else if(temp_service->checks_enabled == TRUE && event_start > temp_service->last_check && temp_service->freshness_threshold == 0)
		expiration_time = (time_t)(event_start + freshness_threshold + (max_service_check_spread * interval_length));
	else
		expiration_time = (time_t)(temp_service->last_check + freshness_threshold);

	/*
	 * If the check was last done passively, we assume it's going
	 * to continue that way and we need to handle the fact that
	 * Nagios might have been shut off for quite a long time. If so,
	 * we mustn't spam freshness notifications but use event_start
	 * instead of last_check to determine freshness expiration time.
	 * The threshold for "long time" is determined as 61.8% of the normal
	 * freshness threshold based on vast heuristical research (ie, "some
	 * guy once told me the golden ratio is good for loads of stuff").
	 */
	if(temp_service->check_type == SERVICE_CHECK_PASSIVE) {
		if(temp_service->last_check < event_start &&
		        event_start - last_program_stop < freshness_threshold * 0.618) {
			expiration_time = event_start + freshness_threshold;
			}
		}
	log_debug_info(DEBUGL_CHECKS, 2, "HBC: %d, PS: %lu, ES: %lu, LC: %lu, CT: %lu, ET: %lu\n", temp_service->has_been_checked, (unsigned long)program_start, (unsigned long)event_start, (unsigned long)temp_service->last_check, (unsigned long)current_time, (unsigned long)expiration_time);

	/* the results for the last check of this service are stale */
	if(expiration_time < current_time) {

		get_time_breakdown((current_time - expiration_time), &days, &hours, &minutes, &seconds);
		get_time_breakdown(freshness_threshold, &tdays, &thours, &tminutes, &tseconds);

		/* log a warning */
		if(log_this == TRUE)
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: The results of service '%s' on host '%s' are stale by %dd %dh %dm %ds (threshold=%dd %dh %dm %ds).  I'm forcing an immediate check of the service.\n", temp_service->description, temp_service->host_name, days, hours, minutes, seconds, tdays, thours, tminutes, tseconds);

		log_debug_info(DEBUGL_CHECKS, 1, "Check results for service '%s' on host '%s' are stale by %dd %dh %dm %ds (threshold=%dd %dh %dm %ds).  Forcing an immediate check of the service...\n", temp_service->description, temp_service->host_name, days, hours, minutes, seconds, tdays, thours, tminutes, tseconds);

		return FALSE;
		}

	log_debug_info(DEBUGL_CHECKS, 1, "Check results for service '%s' on host '%s' are fresh.\n", temp_service->description, temp_service->host_name);

	return TRUE;
	}




/******************************************************************/
/*************** COMMON ROUTE/HOST CHECK FUNCTIONS ****************/
/******************************************************************/

/* execute an on-demand check  */
int perform_on_demand_host_check(host *hst, int *check_return_code, int check_options, int use_cached_result, unsigned long check_timestamp_horizon) {

	log_debug_info(DEBUGL_FUNCTIONS, 0, "perform_on_demand_host_check()\n");

	perform_on_demand_host_check_3x(hst, check_return_code, check_options, use_cached_result, check_timestamp_horizon);

	return OK;
	}



/* execute a scheduled host check using either the 2.x or 3.x logic */
int perform_scheduled_host_check(host *hst, int check_options, double latency) {

	log_debug_info(DEBUGL_FUNCTIONS, 0, "perform_scheduled_host_check()\n");

	run_scheduled_host_check_3x(hst, check_options, latency);

	return OK;
	}



/* schedules an immediate or delayed host check */
void schedule_host_check(host *hst, time_t check_time, int options) {
	timed_event *temp_event = NULL;
	timed_event *new_event = NULL;
	int use_original_event = TRUE;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "schedule_host_check()\n");

	if(hst == NULL)
		return;

	log_debug_info(DEBUGL_CHECKS, 0, "Scheduling a %s, active check of host '%s' @ %s", (options & CHECK_OPTION_FORCE_EXECUTION) ? "forced" : "non-forced", hst->name, ctime(&check_time));

	/* don't schedule a check if active checks of this host are disabled */
	if(hst->checks_enabled == FALSE && !(options & CHECK_OPTION_FORCE_EXECUTION)) {
		log_debug_info(DEBUGL_CHECKS, 0, "Active checks are disabled for this host.\n");
		return;
		}

	/* default is to use the new event */
	use_original_event = FALSE;

	temp_event = (timed_event *)hst->next_check_event;

	/*
	 * If the host already had a check scheduled we need
	 * to decide which check event to use
	 */
	if(temp_event != NULL) {

		log_debug_info(DEBUGL_CHECKS, 2, "Found another host check event for this host @ %s", ctime(&temp_event->run_time));

		/* use the originally scheduled check unless we decide otherwise */
		use_original_event = TRUE;

		/* the original event is a forced check... */
		if((temp_event->event_options & CHECK_OPTION_FORCE_EXECUTION)) {

			/* the new event is also forced and its execution time is earlier than the original, so use it instead */
			if((options & CHECK_OPTION_FORCE_EXECUTION) && (check_time < temp_event->run_time)) {
				log_debug_info(DEBUGL_CHECKS, 2, "New host check event is forced and occurs before the existing event, so the new event be used instead.\n");
				use_original_event = FALSE;
				}
			}

		/* the original event is not a forced check... */
		else {

			/* the new event is a forced check, so use it instead */
			if((options & CHECK_OPTION_FORCE_EXECUTION)) {
				use_original_event = FALSE;
				log_debug_info(DEBUGL_CHECKS, 2, "New host check event is forced, so it will be used instead of the existing event.\n");
				}

			/* the new event is not forced either and its execution time is earlier than the original, so use it instead */
			else if(check_time < temp_event->run_time) {
				use_original_event = FALSE;
				log_debug_info(DEBUGL_CHECKS, 2, "New host check event occurs before the existing (older) event, so it will be used instead.\n");
				}

			/* the new event is older, so override the existing one */
			else {
				log_debug_info(DEBUGL_CHECKS, 2, "New host check event occurs after the existing event, so we'll ignore it.\n");
				}
			}
		}

	/* use the new event */
	if(use_original_event == FALSE) {

		log_debug_info(DEBUGL_CHECKS, 2, "Scheduling new host check event.\n");

		/* allocate memory for a new event item */
		if((new_event = (timed_event *)malloc(sizeof(timed_event))) == NULL) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Could not reschedule check of host '%s'!\n", hst->name);
			return;
			}

		if(temp_event) {
			remove_event(temp_event, &event_list_low, &event_list_low_tail);
			my_free(temp_event);
			}

		/* set the next host check event and time */
		hst->next_check_event = new_event;
		hst->next_check = check_time;

		/* save check options for retention purposes */
		hst->check_options = options;

		/* place the new event in the event queue */
		new_event->event_type = EVENT_HOST_CHECK;
		new_event->event_data = (void *)hst;
		new_event->event_args = (void *)NULL;
		new_event->event_options = options;
		new_event->run_time = hst->next_check;
		new_event->recurring = FALSE;
		new_event->event_interval = 0L;
		new_event->timing_func = NULL;
		new_event->compensate_for_time_change = TRUE;
		reschedule_event(new_event, &event_list_low, &event_list_low_tail);
		}

	else {
		/* reset the next check time (it may be out of sync) */
		if(temp_event != NULL)
			hst->next_check = temp_event->run_time;

		log_debug_info(DEBUGL_CHECKS, 2, "Keeping original host check event (ignoring the new one).\n");
		}

	/* update the status log */
	update_host_status(hst, FALSE);

	return;
	}



/* checks host dependencies */
int check_host_dependencies(host *hst, int dependency_type) {
	hostdependency *temp_dependency = NULL;
	host *temp_host = NULL;
	int state = HOST_UP;
	time_t current_time = 0L;
	void *ptr = NULL;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_host_dependencies()\n");

	/* check all dependencies... */
	for(temp_dependency = get_first_hostdependency_by_dependent_host(hst->name, &ptr); temp_dependency != NULL; temp_dependency = get_next_hostdependency_by_dependent_host(hst->name, &ptr)) {

		/* only check dependencies of the desired type (notification or execution) */
		if(temp_dependency->dependency_type != dependency_type)
			continue;

		/* find the host we depend on... */
		if((temp_host = temp_dependency->master_host_ptr) == NULL)
			continue;

		/* skip this dependency if it has a timeperiod and the current time isn't valid */
		time(&current_time);
		if(temp_dependency->dependency_period != NULL && check_time_against_period(current_time, temp_dependency->dependency_period_ptr) == ERROR)
			return FALSE;

		/* get the status to use (use last hard state if its currently in a soft state) */
		if(temp_host->state_type == SOFT_STATE && soft_state_dependencies == FALSE)
			state = temp_host->last_hard_state;
		else
			state = temp_host->current_state;

		/* is the host we depend on in state that fails the dependency tests? */
		if(state == HOST_UP && temp_dependency->fail_on_up == TRUE)
			return DEPENDENCIES_FAILED;
		if(state == HOST_DOWN && temp_dependency->fail_on_down == TRUE)
			return DEPENDENCIES_FAILED;
		if(state == HOST_UNREACHABLE && temp_dependency->fail_on_unreachable == TRUE)
			return DEPENDENCIES_FAILED;
		if((state == HOST_UP && temp_host->has_been_checked == FALSE) && temp_dependency->fail_on_pending == TRUE)
			return DEPENDENCIES_FAILED;

		/* immediate dependencies ok at this point - check parent dependencies if necessary */
		if(temp_dependency->inherits_parent == TRUE) {
			if(check_host_dependencies(temp_host, dependency_type) != DEPENDENCIES_OK)
				return DEPENDENCIES_FAILED;
			}
		}

	return DEPENDENCIES_OK;
	}



/* check for hosts that never returned from a check... */
void check_for_orphaned_hosts(void) {
	host *temp_host = NULL;
	time_t current_time = 0L;
	time_t expected_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_for_orphaned_hosts()\n");

	/* get the current time */
	time(&current_time);

	/* check all hosts... */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* skip hosts that don't have a set check interval (on-demand checks are missed by the orphan logic) */
		if(temp_host->next_check == (time_t)0L)
			continue;

		/* skip hosts that are not currently executing */
		if(temp_host->is_executing == FALSE)
			continue;

		/* determine the time at which the check results should have come in (allow 10 minutes slack time) */
		expected_time = (time_t)(temp_host->next_check + temp_host->latency + host_check_timeout + check_reaper_interval + 600);

		/* this host was supposed to have executed a while ago, but for some reason the results haven't come back in... */
		if(expected_time < current_time) {

			/* log a warning */
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: The check of host '%s' looks like it was orphaned (results never came back).  I'm scheduling an immediate check of the host...\n", temp_host->name);

			log_debug_info(DEBUGL_CHECKS, 1, "Host '%s' was orphaned, so we're scheduling an immediate check...\n", temp_host->name);

			/* decrement the number of running host checks */
			if(currently_running_host_checks > 0)
				currently_running_host_checks--;

			/* disable the executing flag */
			temp_host->is_executing = FALSE;

			/* schedule an immediate check of the host */
			schedule_host_check(temp_host, current_time, CHECK_OPTION_ORPHAN_CHECK);
			}

		}

	return;
	}



/* check freshness of host results */
void check_host_result_freshness(void) {
	host *temp_host = NULL;
	time_t current_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_host_result_freshness()\n");
	log_debug_info(DEBUGL_CHECKS, 2, "Attempting to check the freshness of host check results...\n");

	/* bail out if we're not supposed to be checking freshness */
	if(check_host_freshness == FALSE) {
		log_debug_info(DEBUGL_CHECKS, 2, "Host freshness checking is disabled.\n");
		return;
		}

	/* get the current time */
	time(&current_time);

	/* check all hosts... */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* skip hosts we shouldn't be checking for freshness */
		if(temp_host->check_freshness == FALSE)
			continue;

		/* skip hosts that have both active and passive checks disabled */
		if(temp_host->checks_enabled == FALSE && temp_host->accept_passive_host_checks == FALSE)
			continue;

		/* skip hosts that are currently executing (problems here will be caught by orphaned host check) */
		if(temp_host->is_executing == TRUE)
			continue;

		/* skip hosts that are already being freshened */
		if(temp_host->is_being_freshened == TRUE)
			continue;

		/* see if the time is right... */
		if(check_time_against_period(current_time, temp_host->check_period_ptr) == ERROR)
			continue;

		/* the results for the last check of this host are stale */
		if(is_host_result_fresh(temp_host, current_time, TRUE) == FALSE) {

			/* set the freshen flag */
			temp_host->is_being_freshened = TRUE;

			/* schedule an immediate forced check of the host */
			schedule_host_check(temp_host, current_time, CHECK_OPTION_FORCE_EXECUTION | CHECK_OPTION_FRESHNESS_CHECK);
			}
		}

	return;
	}



/* checks to see if a hosts's check results are fresh */
int is_host_result_fresh(host *temp_host, time_t current_time, int log_this) {
	time_t expiration_time = 0L;
	int freshness_threshold = 0;
	int days = 0;
	int hours = 0;
	int minutes = 0;
	int seconds = 0;
	int tdays = 0;
	int thours = 0;
	int tminutes = 0;
	int tseconds = 0;
	double interval = 0;

	log_debug_info(DEBUGL_CHECKS, 2, "Checking freshness of host '%s'...\n", temp_host->name);

	/* use user-supplied freshness threshold or auto-calculate a freshness threshold to use? */
	if(temp_host->freshness_threshold == 0) {
		if(temp_host->state_type == HARD_STATE || temp_host->current_state == STATE_OK) {
			interval = temp_host->check_interval;
			}
		else {
			interval = temp_host->retry_interval;
			}
		freshness_threshold = (interval * interval_length) + temp_host->latency + additional_freshness_latency;
		}
	else
		freshness_threshold = temp_host->freshness_threshold;

	log_debug_info(DEBUGL_CHECKS, 2, "Freshness thresholds: host=%d, use=%d\n", temp_host->freshness_threshold, freshness_threshold);

	/* calculate expiration time */
	/*
	 * CHANGED 11/10/05 EG:
	 * program start is only used in expiration time calculation
	 * if > last check AND active checks are enabled, so active checks
	 * can become stale immediately upon program startup
	 */
	if(temp_host->has_been_checked == FALSE)
		expiration_time = (time_t)(event_start + freshness_threshold);
	/*
	 * CHANGED 06/19/07 EG:
	 * Per Ton's suggestion (and user requests), only use program start
	 * time over last check if no specific threshold has been set by user.
	 * Problems can occur if Nagios is restarted more frequently that
	 * freshness threshold intervals (hosts never go stale).
	 */
	/*
	 * CHANGED 10/07/07 EG:
	 * Added max_host_check_spread to expiration time as suggested by
	 * Altinity
	 */
	else if(temp_host->checks_enabled == TRUE && event_start > temp_host->last_check && temp_host->freshness_threshold == 0)
		expiration_time = (time_t)(event_start + freshness_threshold + (max_host_check_spread * interval_length));
	else
		expiration_time = (time_t)(temp_host->last_check + freshness_threshold);

	/*
	 * If the check was last done passively, we assume it's going
	 * to continue that way and we need to handle the fact that
	 * Nagios might have been shut off for quite a long time. If so,
	 * we mustn't spam freshness notifications but use event_start
	 * instead of last_check to determine freshness expiration time.
	 * The threshold for "long time" is determined as 61.8% of the normal
	 * freshness threshold based on vast heuristical research (ie, "some
	 * guy once told me the golden ratio is good for loads of stuff").
	 */
	if(temp_host->check_type == HOST_CHECK_PASSIVE) {
		if(temp_host->last_check < event_start &&
		        event_start - last_program_stop > freshness_threshold * 0.618) {
			expiration_time = event_start + freshness_threshold;
			}
		}

	log_debug_info(DEBUGL_CHECKS, 2, "HBC: %d, PS: %lu, ES: %lu, LC: %lu, CT: %lu, ET: %lu\n", temp_host->has_been_checked, (unsigned long)program_start, (unsigned long)event_start, (unsigned long)temp_host->last_check, (unsigned long)current_time, (unsigned long)expiration_time);

	/* the results for the last check of this host are stale */
	if(expiration_time < current_time) {

		get_time_breakdown((current_time - expiration_time), &days, &hours, &minutes, &seconds);
		get_time_breakdown(freshness_threshold, &tdays, &thours, &tminutes, &tseconds);

		/* log a warning */
		if(log_this == TRUE)
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: The results of host '%s' are stale by %dd %dh %dm %ds (threshold=%dd %dh %dm %ds).  I'm forcing an immediate check of the host.\n", temp_host->name, days, hours, minutes, seconds, tdays, thours, tminutes, tseconds);

		log_debug_info(DEBUGL_CHECKS, 1, "Check results for host '%s' are stale by %dd %dh %dm %ds (threshold=%dd %dh %dm %ds).  Forcing an immediate check of the host...\n", temp_host->name, days, hours, minutes, seconds, tdays, thours, tminutes, tseconds);

		return FALSE;
		}
	else
		log_debug_info(DEBUGL_CHECKS, 1, "Check results for host '%s' are fresh.\n", temp_host->name);

	return TRUE;
	}



/******************************************************************/
/************* NAGIOS 3.X ROUTE/HOST CHECK FUNCTIONS **************/
/******************************************************************/


/*** ON-DEMAND HOST CHECKS USE THIS FUNCTION ***/
/* check to see if we can reach the host */
int perform_on_demand_host_check_3x(host *hst, int *check_result_code, int check_options, int use_cached_result, unsigned long check_timestamp_horizon) {
	int result = OK;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "perform_on_demand_host_check_3x()\n");

	/* make sure we have a host */
	if(hst == NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS, 0, "** On-demand check for host '%s'...\n", hst->name);

	/* check the status of the host */
	result = run_sync_host_check_3x(hst, check_result_code, check_options, use_cached_result, check_timestamp_horizon);

	return result;
	}



/* perform a synchronous check of a host */
/* on-demand host checks will use this... */
int run_sync_host_check_3x(host *hst, int *check_result_code, int check_options, int use_cached_result, unsigned long check_timestamp_horizon) {
	int result = OK;
	time_t current_time = 0L;
	int host_result = HOST_UP;
	char *old_plugin_output = NULL;
	struct timeval start_time;
	struct timeval end_time;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "run_sync_host_check_3x()\n");

	/* make sure we have a host */
	if(hst == NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS, 0, "** Run sync check of host '%s'...\n", hst->name);

	/* is the host check viable at this time? */
	/* if not, return current state and bail out */
	if(check_host_check_viability_3x(hst, check_options, NULL, NULL) == ERROR) {
		if(check_result_code)
			*check_result_code = hst->current_state;
		log_debug_info(DEBUGL_CHECKS, 0, "Host check is not viable at this time.\n");
		return OK;
		}

	/* get the current time */
	time(&current_time);

	/* high resolution start time for event broker */
	gettimeofday(&start_time, NULL);

	/* can we use the last cached host state? */
	if(use_cached_result == TRUE && !(check_options & CHECK_OPTION_FORCE_EXECUTION)) {

		/* we can used the cached result, so return it and get out of here... */
		if(hst->has_been_checked == TRUE && ((current_time - hst->last_check) <= check_timestamp_horizon)) {
			if(check_result_code)
				*check_result_code = hst->current_state;

			log_debug_info(DEBUGL_CHECKS, 1, "* Using cached host state: %d\n", hst->current_state);

			/* update check statistics */
			update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, current_time);
			update_check_stats(ACTIVE_CACHED_HOST_CHECK_STATS, current_time);

			return OK;
			}
		}


	log_debug_info(DEBUGL_CHECKS, 1, "* Running actual host check: old state=%d\n", hst->current_state);


	/******** GOOD TO GO FOR A REAL HOST CHECK AT THIS POINT ********/

	/* update check statistics */
	update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, current_time);
	update_check_stats(SERIAL_HOST_CHECK_STATS, start_time.tv_sec);

	/* reset host check latency, since on-demand checks have none */
	hst->latency = 0.0;

	/* adjust host check attempt */
	adjust_host_check_attempt_3x(hst, TRUE);

	/* save old host state */
	hst->last_state = hst->current_state;
	if(hst->state_type == HARD_STATE)
		hst->last_hard_state = hst->current_state;

	/* save old plugin output for state stalking */
	if(hst->plugin_output)
		old_plugin_output = (char *)strdup(hst->plugin_output);

	/* set the checked flag */
	hst->has_been_checked = TRUE;

	/* clear the freshness flag */
	hst->is_being_freshened = FALSE;

	/* clear check options - we don't want old check options retained */
	hst->check_options = CHECK_OPTION_NONE;

	/* set the check type */
	hst->check_type = HOST_CHECK_ACTIVE;


	/*********** EXECUTE THE CHECK AND PROCESS THE RESULTS **********/

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec = 0L;
	end_time.tv_usec = 0L;
	broker_host_check(NEBTYPE_HOSTCHECK_INITIATE, NEBFLAG_NONE, NEBATTR_NONE, hst, HOST_CHECK_ACTIVE, hst->current_state, hst->state_type, start_time, end_time, hst->host_check_command, hst->latency, 0.0, host_check_timeout, FALSE, 0, NULL, NULL, NULL, NULL, NULL);
#endif

	/* execute the host check */
	host_result = execute_sync_host_check_3x(hst);

	/* process the host check result */
	process_host_check_result_3x(hst, host_result, old_plugin_output, check_options, FALSE, use_cached_result, check_timestamp_horizon);

	/* free memory */
	my_free(old_plugin_output);

	log_debug_info(DEBUGL_CHECKS, 1, "* Sync host check done: new state=%d\n", hst->current_state);

	/* high resolution end time for event broker */
	gettimeofday(&end_time, NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_host_check(NEBTYPE_HOSTCHECK_PROCESSED, NEBFLAG_NONE, NEBATTR_NONE, hst, HOST_CHECK_ACTIVE, hst->current_state, hst->state_type, start_time, end_time, hst->host_check_command, hst->latency, hst->execution_time, host_check_timeout, FALSE, hst->current_state, NULL, hst->plugin_output, hst->long_plugin_output, hst->perf_data, NULL);
#endif

	return result;
	}



/* run an "alive" check on a host */
/* on-demand host checks will use this... */
int execute_sync_host_check_3x(host *hst) {
	nagios_macros mac;
	int result = STATE_OK;
	int return_result = HOST_UP;
	char *processed_command = NULL;
	char *raw_command = NULL;
	struct timeval start_time;
	struct timeval end_time;
	char *temp_ptr;
	int early_timeout = FALSE;
	double exectime;
	char *temp_plugin_output = NULL;
#ifdef USE_EVENT_BROKER
	int neb_result = OK;
#endif


	log_debug_info(DEBUGL_FUNCTIONS, 0, "execute_sync_host_check_3x()\n");

	if(hst == NULL)
		return HOST_DOWN;

	log_debug_info(DEBUGL_CHECKS, 0, "** Executing sync check of host '%s'...\n", hst->name);

#ifdef USE_EVENT_BROKER
	/* initialize start/end times */
	start_time.tv_sec = 0L;
	start_time.tv_usec = 0L;
	end_time.tv_sec = 0L;
	end_time.tv_usec = 0L;

	/* send data to event broker */
	neb_result = broker_host_check(NEBTYPE_HOSTCHECK_SYNC_PRECHECK, NEBFLAG_NONE, NEBATTR_NONE, hst, HOST_CHECK_ACTIVE, hst->current_state, hst->state_type, start_time, end_time, hst->host_check_command, hst->latency, 0.0, host_check_timeout, FALSE, 0, NULL, NULL, NULL, NULL, NULL);

	/* neb module wants to cancel the host check - return the current state of the host */
	if(neb_result == NEBERROR_CALLBACKCANCEL)
		return hst->current_state;

	/* neb module wants to override the host check - perhaps it will check the host itself */
	/* NOTE: if a module does this, it must check the status of the host and populate the data structures BEFORE it returns from the callback! */
	if(neb_result == NEBERROR_CALLBACKOVERRIDE)
		return hst->current_state;
#endif

	/* grab the host macros */
	memset(&mac, 0, sizeof(mac));
	grab_host_macros_r(&mac, hst);

	/* high resolution start time for event broker */
	gettimeofday(&start_time, NULL);

	/* get the last host check time */
	time(&hst->last_check);

	/* get the raw command line */
	get_raw_command_line_r(&mac, hst->check_command_ptr, hst->host_check_command, &raw_command, 0);
	if(raw_command == NULL) {
		clear_volatile_macros_r(&mac);
		return ERROR;
		}

	/* process any macros contained in the argument */
	process_macros_r(&mac, raw_command, &processed_command, 0);
	if(processed_command == NULL) {
		my_free(raw_command);
		clear_volatile_macros_r(&mac);
		return ERROR;
		}

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec = 0L;
	end_time.tv_usec = 0L;
	broker_host_check(NEBTYPE_HOSTCHECK_RAW_START, NEBFLAG_NONE, NEBATTR_NONE, hst, HOST_CHECK_ACTIVE, return_result, hst->state_type, start_time, end_time, hst->host_check_command, 0.0, 0.0, host_check_timeout, early_timeout, result, processed_command, hst->plugin_output, hst->long_plugin_output, hst->perf_data, NULL);
#endif

	log_debug_info(DEBUGL_COMMANDS, 1, "Raw host check command: %s\n", raw_command);
	log_debug_info(DEBUGL_COMMANDS, 0, "Processed host check ommand: %s\n", processed_command);
	my_free(raw_command);

	/* clear plugin output and performance data buffers */
	my_free(hst->plugin_output);
	my_free(hst->long_plugin_output);
	my_free(hst->perf_data);

	/* run the host check command */
	result = my_system_r(&mac, processed_command, host_check_timeout, &early_timeout, &exectime, &temp_plugin_output, MAX_PLUGIN_OUTPUT_LENGTH);
	clear_volatile_macros_r(&mac);

	/* if the check timed out, report an error */
	if(early_timeout == TRUE) {

		my_free(temp_plugin_output);
		asprintf(&temp_plugin_output, "Host check timed out after %d seconds\n", host_check_timeout);

		/* log the timeout */
		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Host check command '%s' for host '%s' timed out after %d seconds\n", processed_command, hst->name, host_check_timeout);
		}

	/* calculate total execution time */
	hst->execution_time = exectime;

	/* record check type */
	hst->check_type = HOST_CHECK_ACTIVE;

	/* parse the output: short and long output, and perf data */
	parse_check_output(temp_plugin_output, &hst->plugin_output, &hst->long_plugin_output, &hst->perf_data, TRUE, TRUE);

	/* free memory */
	my_free(temp_plugin_output);
	my_free(processed_command);

	/* a NULL host check command means we should assume the host is UP */
	if(hst->host_check_command == NULL) {
		my_free(hst->plugin_output);
		hst->plugin_output = (char *)strdup("(Host assumed to be UP)");
		result = STATE_OK;
		}

	/* make sure we have some data */
	if(hst->plugin_output == NULL || !strcmp(hst->plugin_output, "")) {
		my_free(hst->plugin_output);
		hst->plugin_output = (char *)strdup("(No output returned from host check)");
		}

	/* replace semicolons in plugin output (but not performance data) with colons */
	if((temp_ptr = hst->plugin_output)) {
		while((temp_ptr = strchr(temp_ptr, ';')))
			* temp_ptr = ':';
		}

	/* if we're not doing aggressive host checking, let WARNING states indicate the host is up (fake the result to be STATE_OK) */
	if(use_aggressive_host_checking == FALSE && result == STATE_WARNING)
		result = STATE_OK;


	if(result == STATE_OK)
		return_result = HOST_UP;
	else
		return_result = HOST_DOWN;

	/* high resolution end time for event broker */
	gettimeofday(&end_time, NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_host_check(NEBTYPE_HOSTCHECK_RAW_END, NEBFLAG_NONE, NEBATTR_NONE, hst, HOST_CHECK_ACTIVE, return_result, hst->state_type, start_time, end_time, hst->host_check_command, 0.0, exectime, host_check_timeout, early_timeout, result, processed_command, hst->plugin_output, hst->long_plugin_output, hst->perf_data, NULL);
#endif

	log_debug_info(DEBUGL_CHECKS, 0, "** Sync host check done: state=%d\n", return_result);

	return return_result;
	}



/* run a scheduled host check asynchronously */
int run_scheduled_host_check_3x(host *hst, int check_options, double latency) {
	int result = OK;
	time_t current_time = 0L;
	time_t preferred_time = 0L;
	time_t next_valid_time = 0L;
	int time_is_valid = TRUE;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "run_scheduled_host_check_3x()\n");

	if(hst == NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS, 0, "Attempting to run scheduled check of host '%s': check options=%d, latency=%lf\n", hst->name, check_options, latency);

	/*
	 * reset the next_check_event so we know this host
	 * check is no longer in the scheduling queue
	 */
	hst->next_check_event = NULL;

	/* attempt to run the check */
	result = run_async_host_check_3x(hst, check_options, latency, TRUE, TRUE, &time_is_valid, &preferred_time);

	/* an error occurred, so reschedule the check */
	if(result == ERROR) {

		log_debug_info(DEBUGL_CHECKS, 1, "Unable to run scheduled host check at this time\n");

		/* only attempt to (re)schedule checks that should get checked... */
		if(hst->should_be_scheduled == TRUE) {

			/* get current time */
			time(&current_time);

			/* determine next time we should check the host if needed */
			/* if host has no check interval, schedule it again for 5 minutes from now */
			if(current_time >= preferred_time)
				preferred_time = current_time + ((hst->check_interval <= 0) ? 300 : (hst->check_interval * interval_length));

			/* make sure we rescheduled the next host check at a valid time */
			get_next_valid_time(preferred_time, &next_valid_time, hst->check_period_ptr);

			/* the host could not be rescheduled properly - set the next check time for next week */
			if(time_is_valid == FALSE && next_valid_time == preferred_time) {

				/*
				hst->next_check=(time_t)(next_valid_time+(60*60*24*365));
				hst->should_be_scheduled=FALSE;
				*/

				hst->next_check = (time_t)(next_valid_time + (60 * 60 * 24 * 7));

				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check of host '%s' could not be rescheduled properly.  Scheduling check for next week...\n", hst->name);

				log_debug_info(DEBUGL_CHECKS, 1, "Unable to find any valid times to reschedule the next host check!\n");
				}

			/* this service could be rescheduled... */
			else {
				hst->next_check = next_valid_time;
				hst->should_be_scheduled = TRUE;

				log_debug_info(DEBUGL_CHECKS, 1, "Rescheduled next host check for %s", ctime(&next_valid_time));
				}
			}

		/* update the status log */
		update_host_status(hst, FALSE);

		/* reschedule the next host check - unless we couldn't find a valid next check time */
		/* 10/19/07 EG - keep original check options */
		if(hst->should_be_scheduled == TRUE)
			schedule_host_check(hst, hst->next_check, check_options);

		return ERROR;
		}

	return OK;
	}



/* perform an asynchronous check of a host */
/* scheduled host checks will use this, as will some checks that result from on-demand checks... */
int run_async_host_check_3x(host *hst, int check_options, double latency, int scheduled_check, int reschedule_check, int *time_is_valid, time_t *preferred_time) {
	nagios_macros mac;
	char *raw_command = NULL;
	char *processed_command = NULL;
	char output_buffer[MAX_INPUT_BUFFER] = "";
	char *temp_buffer = NULL;
	struct timeval start_time, end_time;
	pid_t pid = 0;
	int fork_error = FALSE;
	int wait_result = 0;
	FILE *fp = NULL;
	int pclose_result = 0;
	mode_t new_umask = 077;
	mode_t old_umask;
	char *output_file = NULL;
	double old_latency = 0.0;
	dbuf checkresult_dbuf;
	int dbuf_chunk = 1024;
#ifdef USE_EVENT_BROKER
	int neb_result = OK;
#endif

	log_debug_info(DEBUGL_FUNCTIONS, 0, "run_async_host_check_3x()\n");

	/* make sure we have a host */
	if(hst == NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS, 0, "** Running async check of host '%s'...\n", hst->name);

	/* is the host check viable at this time? */
	if(check_host_check_viability_3x(hst, check_options, time_is_valid, preferred_time) == ERROR)
		return ERROR;

	/* 08/04/07 EG don't execute a new host check if one is already running */
	if(hst->is_executing == TRUE && !(check_options & CHECK_OPTION_FORCE_EXECUTION)) {
		log_debug_info(DEBUGL_CHECKS, 1, "A check of this host is already being executed, so we'll pass for the moment...\n");
		return ERROR;
		}

	/******** GOOD TO GO FOR A REAL HOST CHECK AT THIS POINT ********/

#ifdef USE_EVENT_BROKER
	/* initialize start/end times */
	start_time.tv_sec = 0L;
	start_time.tv_usec = 0L;
	end_time.tv_sec = 0L;
	end_time.tv_usec = 0L;

	/* send data to event broker */
	neb_result = broker_host_check(NEBTYPE_HOSTCHECK_ASYNC_PRECHECK, NEBFLAG_NONE, NEBATTR_NONE, hst, HOST_CHECK_ACTIVE, hst->current_state, hst->state_type, start_time, end_time, hst->host_check_command, hst->latency, 0.0, host_check_timeout, FALSE, 0, NULL, NULL, NULL, NULL, NULL);

	/* neb module wants to cancel the host check - the check will be rescheduled for a later time by the scheduling logic */
	if(neb_result == NEBERROR_CALLBACKCANCEL)
		return ERROR;

	/* neb module wants to override the host check - perhaps it will check the host itself */
	/* NOTE: if a module does this, it has to do a lot of the stuff found below to make sure things don't get whacked out of shape! */
	if(neb_result == NEBERROR_CALLBACKOVERRIDE)
		return OK;
#endif

	log_debug_info(DEBUGL_CHECKS, 0, "Checking host '%s'...\n", hst->name);

	/* clear check options - we don't want old check options retained */
	/* only clear options if this was a scheduled check - on demand check options shouldn't affect retained info */
	if(scheduled_check == TRUE)
		hst->check_options = CHECK_OPTION_NONE;

	/* adjust host check attempt */
	adjust_host_check_attempt_3x(hst, TRUE);

	/* set latency (temporarily) for macros and event broker */
	old_latency = hst->latency;
	hst->latency = latency;

	/* grab the host macro variables */
	memset(&mac, 0, sizeof(mac));
	grab_host_macros_r(&mac, hst);

	/* get the raw command line */
	get_raw_command_line_r(&mac, hst->check_command_ptr, hst->host_check_command, &raw_command, 0);
	if(raw_command == NULL) {
		clear_volatile_macros_r(&mac);
		log_debug_info(DEBUGL_CHECKS, 0, "Raw check command for host '%s' was NULL - aborting.\n", hst->name);
		return ERROR;
		}

	/* process any macros contained in the argument */
	process_macros_r(&mac, raw_command, &processed_command, 0);
	my_free(raw_command);
	if(processed_command == NULL) {
		clear_volatile_macros_r(&mac);
		log_debug_info(DEBUGL_CHECKS, 0, "Processed check command for host '%s' was NULL - aborting.\n", hst->name);
		return ERROR;
		}

	/* get the command start time */
	gettimeofday(&start_time, NULL);

	/* set check time for on-demand checks, so they're not incorrectly detected as being orphaned - Luke Ross 5/16/08 */
	/* NOTE: 06/23/08 EG not sure if there will be side effects to this or not.... */
	if(scheduled_check == FALSE)
		hst->next_check = start_time.tv_sec;

	/* increment number of host checks that are currently running... */
	currently_running_host_checks++;

	/* set the execution flag */
	hst->is_executing = TRUE;

	/* open a temp file for storing check output */
	old_umask = umask(new_umask);
	asprintf(&output_file, "%s/checkXXXXXX", temp_path);
	check_result_info.output_file_fd = mkstemp(output_file);
	if(check_result_info.output_file_fd >= 0)
		check_result_info.output_file_fp = fdopen(check_result_info.output_file_fd, "w");
	else {
		check_result_info.output_file_fp = NULL;
		check_result_info.output_file_fd = -1;
		}
	umask(old_umask);

	log_debug_info(DEBUGL_CHECKS | DEBUGL_IPC, 1, "Check result output will be written to '%s' (fd=%d)\n", output_file, check_result_info.output_file_fd);

	/* save check info */
	check_result_info.object_check_type = HOST_CHECK;
	check_result_info.host_name = (char *)strdup(hst->name);
	check_result_info.service_description = NULL;
	check_result_info.check_type = HOST_CHECK_ACTIVE;
	check_result_info.check_options = check_options;
	check_result_info.scheduled_check = scheduled_check;
	check_result_info.reschedule_check = reschedule_check;
	check_result_info.output_file = (check_result_info.output_file_fd < 0 || output_file == NULL) ? NULL : strdup(output_file);
	check_result_info.latency = latency;
	check_result_info.start_time = start_time;
	check_result_info.finish_time = start_time;
	check_result_info.early_timeout = FALSE;
	check_result_info.exited_ok = TRUE;
	check_result_info.return_code = STATE_OK;
	check_result_info.output = NULL;

	/* free memory */
	my_free(output_file);

	/* write initial check info to file */
	/* if things go bad later on, the user will at least have something to go on when debugging... */
	if(check_result_info.output_file_fp) {

		fprintf(check_result_info.output_file_fp, "### Active Check Result File ###\n");
		fprintf(check_result_info.output_file_fp, "file_time=%lu\n", (unsigned long)check_result_info.start_time.tv_sec);
		fprintf(check_result_info.output_file_fp, "\n");

		fprintf(check_result_info.output_file_fp, "### Nagios Host Check Result ###\n");
		fprintf(check_result_info.output_file_fp, "# Time: %s", ctime(&check_result_info.start_time.tv_sec));
		fprintf(check_result_info.output_file_fp, "host_name=%s\n", check_result_info.host_name);
		fprintf(check_result_info.output_file_fp, "check_type=%d\n", check_result_info.check_type);
		fprintf(check_result_info.output_file_fp, "check_options=%d\n", check_result_info.check_options);
		fprintf(check_result_info.output_file_fp, "scheduled_check=%d\n", check_result_info.scheduled_check);
		fprintf(check_result_info.output_file_fp, "reschedule_check=%d\n", check_result_info.reschedule_check);
		fprintf(check_result_info.output_file_fp, "latency=%f\n", hst->latency);
		fprintf(check_result_info.output_file_fp, "start_time=%lu.%lu\n", check_result_info.start_time.tv_sec, check_result_info.start_time.tv_usec);

		/* flush buffer or we'll end up writing twice when we fork() */
		fflush(check_result_info.output_file_fp);
		}

	/* initialize dynamic buffer for storing plugin output */
	dbuf_init(&checkresult_dbuf, dbuf_chunk);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_host_check(NEBTYPE_HOSTCHECK_INITIATE, NEBFLAG_NONE, NEBATTR_NONE, hst, HOST_CHECK_ACTIVE, hst->current_state, hst->state_type, start_time, end_time, hst->host_check_command, hst->latency, 0.0, host_check_timeout, FALSE, 0, processed_command, NULL, NULL, NULL, NULL);
#endif

	/* reset latency (permanent value for this check will get set later) */
	hst->latency = old_latency;

	/* update check statistics */
	update_check_stats((scheduled_check == TRUE) ? ACTIVE_SCHEDULED_HOST_CHECK_STATS : ACTIVE_ONDEMAND_HOST_CHECK_STATS, start_time.tv_sec);
	update_check_stats(PARALLEL_HOST_CHECK_STATS, start_time.tv_sec);

	/* fork a child process */
	pid = fork();

	/* an error occurred while trying to fork */
	if(pid == -1) {

		fork_error = TRUE;

		/* log an error */
		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: The check of host '%s' could not be performed due to a fork() error: '%s'.\n", hst->name, strerror(errno));

		log_debug_info(DEBUGL_CHECKS, 0, "Check of host '%s' could not be performed due to a fork() error: '%s'!\n", hst->name, strerror(errno));
		}

	/* if we are in the child process... */
	else if(pid == 0) {

		/* set environment variables */
		set_all_macro_environment_vars_r(&mac, TRUE);

		/* ADDED 11/12/07 EG */
		/* close external command file and shut down worker thread */
		close_command_file();

		/* fork again if we're not in a large installation */
		if(child_processes_fork_twice == TRUE) {

			/* fork again... */
			pid = fork();

			/* an error occurred while trying to fork again */
			if(pid == -1)
				exit(STATE_UNKNOWN);
			}

		/* the grandchild (or child if large install tweaks are enabled) process should run the host check... */
		if(pid == 0 || child_processes_fork_twice == FALSE) {

			/* reset signal handling */
			reset_sighandler();

			/* become the process group leader */
			setpgid(0, 0);

			/* exit on term signals at this process level */
			signal(SIGTERM, SIG_DFL);

			/* catch plugins that don't finish in a timely manner */
			signal(SIGALRM, host_check_sighandler);
			alarm(host_check_timeout);

			/* disable rotation of the debug file */
			max_debug_file_size = 0L;

			/* run the plugin check command */
			fp = popen(processed_command, "r");
			if(fp == NULL)
				_exit(STATE_UNKNOWN);

			/* initialize buffer */
			strcpy(output_buffer, "");

			/* get all lines of plugin output - escape newlines */
			while(fgets(output_buffer, sizeof(output_buffer) - 1, fp)) {
				temp_buffer = escape_newlines(output_buffer);
				dbuf_strcat(&checkresult_dbuf, temp_buffer);
				my_free(temp_buffer);
				}

			/* close the process */
			pclose_result = pclose(fp);

			/* reset the alarm and signal handling here */
			signal(SIGALRM, SIG_IGN);
			alarm(0);

			/* get the check finish time */
			gettimeofday(&end_time, NULL);

			/* record check result info */
			check_result_info.finish_time = end_time;
			check_result_info.early_timeout = FALSE;

			/* test for execution error */
			if(pclose_result == -1) {
				pclose_result = STATE_UNKNOWN;
				check_result_info.return_code = STATE_CRITICAL;
				check_result_info.exited_ok = FALSE;
				}
			else {
				if(WEXITSTATUS(pclose_result) == 0 && WIFSIGNALED(pclose_result))
					check_result_info.return_code = 128 + WTERMSIG(pclose_result);
				else
					check_result_info.return_code = WEXITSTATUS(pclose_result);
				}

			/* write check result to file */
			if(check_result_info.output_file_fp) {
				FILE *fp;

				/* protect against signal races */
				fp = check_result_info.output_file_fp;
				check_result_info.output_file_fp = NULL;

				fprintf(fp, "finish_time=%lu.%lu\n", check_result_info.finish_time.tv_sec, check_result_info.finish_time.tv_usec);
				fprintf(fp, "early_timeout=%d\n", check_result_info.early_timeout);
				fprintf(fp, "exited_ok=%d\n", check_result_info.exited_ok);
				fprintf(fp, "return_code=%d\n", check_result_info.return_code);
				fprintf(fp, "output=%s\n", (checkresult_dbuf.buf == NULL) ? "(null)" : checkresult_dbuf.buf);

				/* close the temp file */
				fclose(fp);

				/* move check result to queue directory */
				move_check_result_to_queue(check_result_info.output_file);
				}

			/* free memory */
			dbuf_free(&checkresult_dbuf);
			my_free(processed_command);

			/* free check result memory */
			free_check_result(&check_result_info);

			/* return with plugin exit status - not really necessary... */
			_exit(pclose_result);
			}

		/* NOTE: this code is never reached if large install tweaks are enabled... */

		/* unset environment variables */
		set_all_macro_environment_vars_r(&mac, FALSE);

		/* free allocated memory */
		/* this needs to be done last, so we don't free memory for variables before they're used above */
		if(free_child_process_memory == TRUE)
			free_memory(&mac);

		/* parent exits immediately - grandchild process is inherited by the INIT process, so we have no zombie problem... */
		_exit(STATE_OK);
		}

	/* else the parent should wait for the first child to return... */
	else if(pid > 0) {
		clear_volatile_macros_r(&mac);

		log_debug_info(DEBUGL_CHECKS, 2, "Host check is executing in child process (pid=%lu)\n", (unsigned long)pid);

		/* parent should close output file */
		if(check_result_info.output_file_fp)
			fclose(check_result_info.output_file_fp);

		/* should this be done in first child process (after spawning grandchild) as well? */
		/* free memory allocated for IPC functionality */
		free_check_result(&check_result_info);

		/* free memory */
		my_free(processed_command);

		/* wait for the first child to return */
		/* if large install tweaks are enabled, we'll clean up the zombie process later */
		if(child_processes_fork_twice == TRUE)
			wait_result = waitpid(pid, NULL, 0);
		}

	/* see if we were able to run the check... */
	if(fork_error == TRUE)
		return ERROR;

	return OK;
	}



/* process results of an asynchronous host check */
int handle_async_host_check_result_3x(host *temp_host, check_result *queued_check_result) {
	time_t current_time;
	int result = STATE_OK;
	int reschedule_check = FALSE;
	char *old_plugin_output = NULL;
	char *temp_ptr = NULL;
	struct timeval start_time_hires;
	struct timeval end_time_hires;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "handle_async_host_check_result_3x()\n");

	/* make sure we have what we need */
	if(temp_host == NULL || queued_check_result == NULL)
		return ERROR;

	time(&current_time);

	log_debug_info(DEBUGL_CHECKS, 1, "** Handling async check result for host '%s'...\n", temp_host->name);

	log_debug_info(DEBUGL_CHECKS, 2, "\tCheck Type:         %s\n", (queued_check_result->check_type == HOST_CHECK_ACTIVE) ? "Active" : "Passive");
	log_debug_info(DEBUGL_CHECKS, 2, "\tCheck Options:      %d\n", queued_check_result->check_options);
	log_debug_info(DEBUGL_CHECKS, 2, "\tScheduled Check?:   %s\n", (queued_check_result->scheduled_check == TRUE) ? "Yes" : "No");
	log_debug_info(DEBUGL_CHECKS, 2, "\tReschedule Check?:  %s\n", (queued_check_result->reschedule_check == TRUE) ? "Yes" : "No");
	log_debug_info(DEBUGL_CHECKS, 2, "\tExited OK?:         %s\n", (queued_check_result->exited_ok == TRUE) ? "Yes" : "No");
	log_debug_info(DEBUGL_CHECKS, 2, "\tExec Time:          %.3f\n", temp_host->execution_time);
	log_debug_info(DEBUGL_CHECKS, 2, "\tLatency:            %.3f\n", temp_host->latency);
	log_debug_info(DEBUGL_CHECKS, 2, "\tReturn Status:      %d\n", queued_check_result->return_code);
	log_debug_info(DEBUGL_CHECKS, 2, "\tOutput:             %s\n", (queued_check_result == NULL) ? "NULL" : queued_check_result->output);

	/* decrement the number of host checks still out there... */
	if(queued_check_result->check_type == HOST_CHECK_ACTIVE && currently_running_host_checks > 0)
		currently_running_host_checks--;

	/* skip this host check results if its passive and we aren't accepting passive check results */
	if(queued_check_result->check_type == HOST_CHECK_PASSIVE) {
		if(accept_passive_host_checks == FALSE) {
			log_debug_info(DEBUGL_CHECKS, 0, "Discarding passive host check result because passive host checks are disabled globally.\n");
			return ERROR;
			}
		if(temp_host->accept_passive_host_checks == FALSE) {
			log_debug_info(DEBUGL_CHECKS, 0, "Discarding passive host check result because passive checks are disabled for this host.\n");
			return ERROR;
			}
		}

	/* clear the freshening flag (it would have been set if this host was determined to be stale) */
	if(queued_check_result->check_options & CHECK_OPTION_FRESHNESS_CHECK)
		temp_host->is_being_freshened = FALSE;

	/* DISCARD INVALID FRESHNESS CHECK RESULTS */
	/* If a host goes stale, Nagios will initiate a forced check in order to freshen it.  There is a race condition whereby a passive check
	   could arrive between the 1) initiation of the forced check and 2) the time when the forced check result is processed here.  This would
	   make the host fresh again, so we do a quick check to make sure the host is still stale before we accept the check result. */
	if((queued_check_result->check_options & CHECK_OPTION_FRESHNESS_CHECK) && is_host_result_fresh(temp_host, current_time, FALSE) == TRUE) {
		log_debug_info(DEBUGL_CHECKS, 0, "Discarding host freshness check result because the host is currently fresh (race condition avoided).\n");
		return OK;
		}

	/* was this check passive or active? */
	temp_host->check_type = (queued_check_result->check_type == HOST_CHECK_ACTIVE) ? HOST_CHECK_ACTIVE : HOST_CHECK_PASSIVE;

	/* update check statistics for passive results */
	if(queued_check_result->check_type == HOST_CHECK_PASSIVE)
		update_check_stats(PASSIVE_HOST_CHECK_STATS, queued_check_result->start_time.tv_sec);

	/* should we reschedule the next check of the host? NOTE: this might be overridden later... */
	reschedule_check = queued_check_result->reschedule_check;

	/* check latency is passed to us for both active and passive checks */
	temp_host->latency = queued_check_result->latency;

	/* update the execution time for this check (millisecond resolution) */
	temp_host->execution_time = (double)((double)(queued_check_result->finish_time.tv_sec - queued_check_result->start_time.tv_sec) + (double)((queued_check_result->finish_time.tv_usec - queued_check_result->start_time.tv_usec) / 1000.0) / 1000.0);
	if(temp_host->execution_time < 0.0)
		temp_host->execution_time = 0.0;

	/* set the checked flag */
	temp_host->has_been_checked = TRUE;

	/* clear the execution flag if this was an active check */
	if(queued_check_result->check_type == HOST_CHECK_ACTIVE)
		temp_host->is_executing = FALSE;

	/* get the last check time */
	temp_host->last_check = queued_check_result->start_time.tv_sec;

	/* was this check passive or active? */
	temp_host->check_type = (queued_check_result->check_type == HOST_CHECK_ACTIVE) ? HOST_CHECK_ACTIVE : HOST_CHECK_PASSIVE;

	/* save the old host state */
	temp_host->last_state = temp_host->current_state;
	if(temp_host->state_type == HARD_STATE)
		temp_host->last_hard_state = temp_host->current_state;

	/* save old plugin output */
	if(temp_host->plugin_output)
		old_plugin_output = (char *)strdup(temp_host->plugin_output);

	/* clear the old plugin output and perf data buffers */
	my_free(temp_host->plugin_output);
	my_free(temp_host->long_plugin_output);
	my_free(temp_host->perf_data);

	/* parse check output to get: (1) short output, (2) long output, (3) perf data */
	parse_check_output(queued_check_result->output, &temp_host->plugin_output, &temp_host->long_plugin_output, &temp_host->perf_data, TRUE, TRUE);

	/* make sure we have some data */
	if(temp_host->plugin_output == NULL || !strcmp(temp_host->plugin_output, "")) {
		my_free(temp_host->plugin_output);
		temp_host->plugin_output = (char *)strdup("(No output returned from host check)");
		}

	/* replace semicolons in plugin output (but not performance data) with colons */
	if((temp_ptr = temp_host->plugin_output)) {
		while((temp_ptr = strchr(temp_ptr, ';')))
			* temp_ptr = ':';
		}

	log_debug_info(DEBUGL_CHECKS, 2, "Parsing check output...\n");
	log_debug_info(DEBUGL_CHECKS, 2, "Short Output: %s\n", (temp_host->plugin_output == NULL) ? "NULL" : temp_host->plugin_output);
	log_debug_info(DEBUGL_CHECKS, 2, "Long Output:  %s\n", (temp_host->long_plugin_output == NULL) ? "NULL" : temp_host->long_plugin_output);
	log_debug_info(DEBUGL_CHECKS, 2, "Perf Data:    %s\n", (temp_host->perf_data == NULL) ? "NULL" : temp_host->perf_data);

	/* get the unprocessed return code */
	/* NOTE: for passive checks, this is the final/processed state */
	result = queued_check_result->return_code;

	/* adjust return code (active checks only) */
	if(queued_check_result->check_type == HOST_CHECK_ACTIVE) {

		/* if there was some error running the command, just skip it (this shouldn't be happening) */
		if(queued_check_result->exited_ok == FALSE) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning:  Check of host '%s' did not exit properly!\n", temp_host->name);

			my_free(temp_host->plugin_output);
			my_free(temp_host->long_plugin_output);
			my_free(temp_host->perf_data);

			temp_host->plugin_output = (char *)strdup("(Host check did not exit properly)");

			result = STATE_CRITICAL;
			}

		/* make sure the return code is within bounds */
		else if(queued_check_result->return_code < 0 || queued_check_result->return_code > 3) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Return code of %d for check of host '%s' was out of bounds.%s\n", queued_check_result->return_code, temp_host->name, (queued_check_result->return_code == 126 || queued_check_result->return_code == 127) ? " Make sure the plugin you're trying to run actually exists." : "");

			my_free(temp_host->plugin_output);
			my_free(temp_host->long_plugin_output);
			my_free(temp_host->perf_data);

			asprintf(&temp_host->plugin_output, "(Return code of %d is out of bounds%s)", queued_check_result->return_code, (queued_check_result->return_code == 126 || queued_check_result->return_code == 127) ? " - plugin may be missing" : "");

			result = STATE_CRITICAL;
			}

		/* a NULL host check command means we should assume the host is UP */
		if(temp_host->host_check_command == NULL) {
			my_free(temp_host->plugin_output);
			temp_host->plugin_output = (char *)strdup("(Host assumed to be UP)");
			result = STATE_OK;
			}
		}

	/* translate return code to basic UP/DOWN state - the DOWN/UNREACHABLE state determination is made later */
	/* NOTE: only do this for active checks - passive check results already have the final state */
	if(queued_check_result->check_type == HOST_CHECK_ACTIVE) {

		/* if we're not doing aggressive host checking, let WARNING states indicate the host is up (fake the result to be STATE_OK) */
		if(use_aggressive_host_checking == FALSE && result == STATE_WARNING)
			result = STATE_OK;

		/* OK states means the host is UP */
		if(result == STATE_OK)
			result = HOST_UP;

		/* any problem state indicates the host is not UP */
		else
			result = HOST_DOWN;
		}


	/******************* PROCESS THE CHECK RESULTS ******************/

	/* process the host check result */
	process_host_check_result_3x(temp_host, result, old_plugin_output, CHECK_OPTION_NONE, reschedule_check, TRUE, cached_host_check_horizon);

	/* free memory */
	my_free(old_plugin_output);

	log_debug_info(DEBUGL_CHECKS, 1, "** Async check result for host '%s' handled: new state=%d\n", temp_host->name, temp_host->current_state);

	/* high resolution start time for event broker */
	start_time_hires = queued_check_result->start_time;

	/* high resolution end time for event broker */
	gettimeofday(&end_time_hires, NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_host_check(NEBTYPE_HOSTCHECK_PROCESSED, NEBFLAG_NONE, NEBATTR_NONE, temp_host, temp_host->check_type, temp_host->current_state, temp_host->state_type, start_time_hires, end_time_hires, temp_host->host_check_command, temp_host->latency, temp_host->execution_time, host_check_timeout, queued_check_result->early_timeout, queued_check_result->return_code, NULL, temp_host->plugin_output, temp_host->long_plugin_output, temp_host->perf_data, NULL);
#endif

	return OK;
	}



/* processes the result of a synchronous or asynchronous host check */
int process_host_check_result_3x(host *hst, int new_state, char *old_plugin_output, int check_options, int reschedule_check, int use_cached_result, unsigned long check_timestamp_horizon) {
	hostsmember *temp_hostsmember = NULL;
	host *child_host = NULL;
	host *parent_host = NULL;
	host *master_host = NULL;
	host *temp_host = NULL;
	hostdependency *temp_dependency = NULL;
	objectlist *check_hostlist = NULL;
	objectlist *hostlist_item = NULL;
	int parent_state = HOST_UP;
	time_t current_time = 0L;
	time_t next_check = 0L;
	time_t preferred_time = 0L;
	time_t next_valid_time = 0L;
	int run_async_check = TRUE;
	void *ptr = NULL;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "process_host_check_result_3x()\n");

	log_debug_info(DEBUGL_CHECKS, 1, "HOST: %s, ATTEMPT=%d/%d, CHECK TYPE=%s, STATE TYPE=%s, OLD STATE=%d, NEW STATE=%d\n", hst->name, hst->current_attempt, hst->max_attempts, (hst->check_type == HOST_CHECK_ACTIVE) ? "ACTIVE" : "PASSIVE", (hst->state_type == HARD_STATE) ? "HARD" : "SOFT", hst->current_state, new_state);

	/* get the current time */
	time(&current_time);

	/* default next check time */
	next_check = (unsigned long)(current_time + (hst->check_interval * interval_length));

	/* we have to adjust current attempt # for passive checks, as it isn't done elsewhere */
	if(hst->check_type == HOST_CHECK_PASSIVE && passive_host_checks_are_soft == TRUE)
		adjust_host_check_attempt_3x(hst, FALSE);

	/* log passive checks - we need to do this here, as some my bypass external commands by getting dropped in checkresults dir */
	if(hst->check_type == HOST_CHECK_PASSIVE) {
		if(log_passive_checks == TRUE)
			logit(NSLOG_PASSIVE_CHECK, FALSE, "PASSIVE HOST CHECK: %s;%d;%s\n", hst->name, new_state, hst->plugin_output);
		}


	/******* HOST WAS DOWN/UNREACHABLE INITIALLY *******/
	if(hst->current_state != HOST_UP) {

		log_debug_info(DEBUGL_CHECKS, 1, "Host was DOWN/UNREACHABLE.\n");

		/***** HOST IS NOW UP *****/
		/* the host just recovered! */
		if(new_state == HOST_UP) {

			/* set the current state */
			hst->current_state = HOST_UP;

			/* set the state type */
			/* set state type to HARD for passive checks and active checks that were previously in a HARD STATE */
			if(hst->state_type == HARD_STATE || (hst->check_type == HOST_CHECK_PASSIVE && passive_host_checks_are_soft == FALSE))
				hst->state_type = HARD_STATE;
			else
				hst->state_type = SOFT_STATE;

			log_debug_info(DEBUGL_CHECKS, 1, "Host experienced a %s recovery (it's now UP).\n", (hst->state_type == HARD_STATE) ? "HARD" : "SOFT");

			/* reschedule the next check of the host at the normal interval */
			reschedule_check = TRUE;
			next_check = (unsigned long)(current_time + (hst->check_interval * interval_length));

			/* propagate checks to immediate parents if they are not already UP */
			/* we do this because a parent host (or grandparent) may have recovered somewhere and we should catch the recovery as soon as possible */
			log_debug_info(DEBUGL_CHECKS, 1, "Propagating checks to parent host(s)...\n");

			for(temp_hostsmember = hst->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
				if((parent_host = temp_hostsmember->host_ptr) == NULL)
					continue;
				if(parent_host->current_state != HOST_UP) {
					log_debug_info(DEBUGL_CHECKS, 1, "Check of parent host '%s' queued.\n", parent_host->name);
					add_object_to_objectlist(&check_hostlist, (void *)parent_host);
					}
				}

			/* propagate checks to immediate children if they are not already UP */
			/* we do this because children may currently be UNREACHABLE, but may (as a result of this recovery) switch to UP or DOWN states */
			log_debug_info(DEBUGL_CHECKS, 1, "Propagating checks to child host(s)...\n");

			for(temp_hostsmember = hst->child_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
				if((child_host = temp_hostsmember->host_ptr) == NULL)
					continue;
				if(child_host->current_state != HOST_UP) {
					log_debug_info(DEBUGL_CHECKS, 1, "Check of child host '%s' queued.\n", child_host->name);
					add_object_to_objectlist(&check_hostlist, (void *)child_host);
					}
				}
			}

		/***** HOST IS STILL DOWN/UNREACHABLE *****/
		/* we're still in a problem state... */
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is still DOWN/UNREACHABLE.\n");

			/* passive checks are treated as HARD states by default... */
			if(hst->check_type == HOST_CHECK_PASSIVE && passive_host_checks_are_soft == FALSE) {

				/* set the state type */
				hst->state_type = HARD_STATE;

				/* reset the current attempt */
				hst->current_attempt = 1;
				}

			/* active checks and passive checks (treated as SOFT states) */
			else {

				/* set the state type */
				/* we've maxed out on the retries */
				if(hst->current_attempt == hst->max_attempts)
					hst->state_type = HARD_STATE;
				/* the host was in a hard problem state before, so it still is now */
				else if(hst->current_attempt == 1)
					hst->state_type = HARD_STATE;
				/* the host is in a soft state and the check will be retried */
				else
					hst->state_type = SOFT_STATE;
				}

			/* make a determination of the host's state */
			/* translate host state between DOWN/UNREACHABLE (only for passive checks if enabled) */
			hst->current_state = new_state;
			if(hst->check_type == HOST_CHECK_ACTIVE || translate_passive_host_checks == TRUE)
				hst->current_state = determine_host_reachability(hst);

			/* reschedule the next check if the host state changed */
			if(hst->last_state != hst->current_state || hst->last_hard_state != hst->current_state) {

				reschedule_check = TRUE;

				/* schedule a re-check of the host at the retry interval because we can't determine its final state yet... */
				if(hst->state_type == SOFT_STATE)
					next_check = (unsigned long)(current_time + (hst->retry_interval * interval_length));

				/* host has maxed out on retries (or was previously in a hard problem state), so reschedule the next check at the normal interval */
				else
					next_check = (unsigned long)(current_time + (hst->check_interval * interval_length));
				}

			}

		}

	/******* HOST WAS UP INITIALLY *******/
	else {

		log_debug_info(DEBUGL_CHECKS, 1, "Host was UP.\n");

		/***** HOST IS STILL UP *****/
		/* either the host never went down since last check */
		if(new_state == HOST_UP) {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is still UP.\n");

			/* set the current state */
			hst->current_state = HOST_UP;

			/* set the state type */
			hst->state_type = HARD_STATE;

			/* reschedule the next check at the normal interval */
			if(reschedule_check == TRUE)
				next_check = (unsigned long)(current_time + (hst->check_interval * interval_length));
			}

		/***** HOST IS NOW DOWN/UNREACHABLE *****/
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is now DOWN/UNREACHABLE.\n");

			/***** SPECIAL CASE FOR HOSTS WITH MAX_ATTEMPTS==1 *****/
			if(hst->max_attempts == 1) {

				log_debug_info(DEBUGL_CHECKS, 1, "Max attempts = 1!.\n");

				/* set the state type */
				hst->state_type = HARD_STATE;

				/* host has maxed out on retries, so reschedule the next check at the normal interval */
				reschedule_check = TRUE;
				next_check = (unsigned long)(current_time + (hst->check_interval * interval_length));

				/* we need to run SYNCHRONOUS checks of all parent hosts to accurately determine the state of this host */
				/* this is extremely inefficient (reminiscent of Nagios 2.x logic), but there's no other good way around it */
				/* check all parent hosts to see if we're DOWN or UNREACHABLE */
				/* only do this for ACTIVE checks, as PASSIVE checks contain a pre-determined state */
				if(hst->check_type == HOST_CHECK_ACTIVE) {

					log_debug_info(DEBUGL_CHECKS, 1, "** WARNING: Max attempts = 1, so we have to run serial checks of all parent hosts!\n");

					for(temp_hostsmember = hst->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {

						if((parent_host = temp_hostsmember->host_ptr) == NULL)
							continue;

						log_debug_info(DEBUGL_CHECKS, 1, "Running serial check parent host '%s'...\n", parent_host->name);

						/* run an immediate check of the parent host */
						run_sync_host_check_3x(parent_host, &parent_state, check_options, use_cached_result, check_timestamp_horizon);

						/* bail out as soon as we find one parent host that is UP */
						if(parent_state == HOST_UP) {

							log_debug_info(DEBUGL_CHECKS, 1, "Parent host is UP, so this one is DOWN.\n");

							/* set the current state */
							hst->current_state = HOST_DOWN;
							break;
							}
						}

					if(temp_hostsmember == NULL) {
						/* host has no parents, so its up */
						if(hst->parent_hosts == NULL) {
							log_debug_info(DEBUGL_CHECKS, 1, "Host has no parents, so it's DOWN.\n");
							hst->current_state = HOST_DOWN;
							}
						else {
							/* no parents were up, so this host is UNREACHABLE */
							log_debug_info(DEBUGL_CHECKS, 1, "No parents were UP, so this host is UNREACHABLE.\n");
							hst->current_state = HOST_UNREACHABLE;
							}
						}
					}

				/* set the host state for passive checks */
				else {
					/* set the state */
					hst->current_state = new_state;

					/* translate host state between DOWN/UNREACHABLE for passive checks (if enabled) */
					/* make a determination of the host's state */
					if(translate_passive_host_checks == TRUE)
						hst->current_state = determine_host_reachability(hst);

					}

				/* propagate checks to immediate children if they are not UNREACHABLE */
				/* we do this because we may now be blocking the route to child hosts */
				log_debug_info(DEBUGL_CHECKS, 1, "Propagating check to immediate non-UNREACHABLE child hosts...\n");

				for(temp_hostsmember = hst->child_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
					if((child_host = temp_hostsmember->host_ptr) == NULL)
						continue;
					if(child_host->current_state != HOST_UNREACHABLE) {
						log_debug_info(DEBUGL_CHECKS, 1, "Check of child host '%s' queued.\n", child_host->name);
						add_object_to_objectlist(&check_hostlist, (void *)child_host);
						}
					}
				}

			/***** MAX ATTEMPTS > 1 *****/
			else {

				/* active and (in some cases) passive check results are treated as SOFT states */
				if(hst->check_type == HOST_CHECK_ACTIVE || passive_host_checks_are_soft == TRUE) {

					/* set the state type */
					hst->state_type = SOFT_STATE;
					}

				/* by default, passive check results are treated as HARD states */
				else {

					/* set the state type */
					hst->state_type = HARD_STATE;

					/* reset the current attempt */
					hst->current_attempt = 1;
					}

				/* make a (in some cases) preliminary determination of the host's state */
				/* translate host state between DOWN/UNREACHABLE (for passive checks only if enabled) */
				hst->current_state = new_state;
				if(hst->check_type == HOST_CHECK_ACTIVE || translate_passive_host_checks == TRUE)
					hst->current_state = determine_host_reachability(hst);

				/* reschedule a check of the host */
				reschedule_check = TRUE;

				/* schedule a re-check of the host at the retry interval because we can't determine its final state yet... */
				if(hst->check_type == HOST_CHECK_ACTIVE || passive_host_checks_are_soft == TRUE)
					next_check = (unsigned long)(current_time + (hst->retry_interval * interval_length));

				/* schedule a re-check of the host at the normal interval */
				else
					next_check = (unsigned long)(current_time + (hst->check_interval * interval_length));

				/* propagate checks to immediate parents if they are UP */
				/* we do this because a parent host (or grandparent) may have gone down and blocked our route */
				/* checking the parents ASAP will allow us to better determine the final state (DOWN/UNREACHABLE) of this host later */
				log_debug_info(DEBUGL_CHECKS, 1, "Propagating checks to immediate parent hosts that are UP...\n");

				for(temp_hostsmember = hst->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
					if((parent_host = temp_hostsmember->host_ptr) == NULL)
						continue;
					if(parent_host->current_state == HOST_UP) {
						add_object_to_objectlist(&check_hostlist, (void *)parent_host);
						log_debug_info(DEBUGL_CHECKS, 1, "Check of host '%s' queued.\n", parent_host->name);
						}
					}

				/* propagate checks to immediate children if they are not UNREACHABLE */
				/* we do this because we may now be blocking the route to child hosts */
				log_debug_info(DEBUGL_CHECKS, 1, "Propagating checks to immediate non-UNREACHABLE child hosts...\n");

				for(temp_hostsmember = hst->child_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
					if((child_host = temp_hostsmember->host_ptr) == NULL)
						continue;
					if(child_host->current_state != HOST_UNREACHABLE) {
						log_debug_info(DEBUGL_CHECKS, 1, "Check of child host '%s' queued.\n", child_host->name);
						add_object_to_objectlist(&check_hostlist, (void *)child_host);
						}
					}

				/* check dependencies on second to last host check */
				if(enable_predictive_host_dependency_checks == TRUE && hst->current_attempt == (hst->max_attempts - 1)) {

					/* propagate checks to hosts that THIS ONE depends on for notifications AND execution */
					/* we do to help ensure that the dependency checks are accurate before it comes time to notify */
					log_debug_info(DEBUGL_CHECKS, 1, "Propagating predictive dependency checks to hosts this one depends on...\n");

					for(temp_dependency = get_first_hostdependency_by_dependent_host(hst->name, &ptr); temp_dependency != NULL; temp_dependency = get_next_hostdependency_by_dependent_host(hst->name, &ptr)) {
						if(temp_dependency->dependent_host_ptr == hst && temp_dependency->master_host_ptr != NULL) {
							master_host = (host *)temp_dependency->master_host_ptr;
							log_debug_info(DEBUGL_CHECKS, 1, "Check of host '%s' queued.\n", master_host->name);
							add_object_to_objectlist(&check_hostlist, (void *)master_host);
							}
						}
					}
				}
			}
		}

	log_debug_info(DEBUGL_CHECKS, 1, "Pre-handle_host_state() Host: %s, Attempt=%d/%d, Type=%s, Final State=%d\n", hst->name, hst->current_attempt, hst->max_attempts, (hst->state_type == HARD_STATE) ? "HARD" : "SOFT", hst->current_state);

	/* handle the host state */
	handle_host_state(hst);

	log_debug_info(DEBUGL_CHECKS, 1, "Post-handle_host_state() Host: %s, Attempt=%d/%d, Type=%s, Final State=%d\n", hst->name, hst->current_attempt, hst->max_attempts, (hst->state_type == HARD_STATE) ? "HARD" : "SOFT", hst->current_state);


	/******************** POST-PROCESSING STUFF *********************/

	/* if the plugin output differs from previous check and no state change, log the current state/output if state stalking is enabled */
	if(hst->last_state == hst->current_state && compare_strings(old_plugin_output, hst->plugin_output)) {

		if(hst->current_state == HOST_UP && hst->stalk_on_up == TRUE)
			log_host_event(hst);

		else if(hst->current_state == HOST_DOWN && hst->stalk_on_down == TRUE)
			log_host_event(hst);

		else if(hst->current_state == HOST_UNREACHABLE && hst->stalk_on_unreachable == TRUE)
			log_host_event(hst);
		}

	/* check to see if the associated host is flapping */
	check_for_host_flapping(hst, TRUE, TRUE, TRUE);

	/* reschedule the next check of the host (usually ONLY for scheduled, active checks, unless overridden above) */
	if(reschedule_check == TRUE) {

		log_debug_info(DEBUGL_CHECKS, 1, "Rescheduling next check of host at %s", ctime(&next_check));

		/* default is to reschedule host check unless a test below fails... */
		hst->should_be_scheduled = TRUE;

		/* get the new current time */
		time(&current_time);

		/* make sure we don't get ourselves into too much trouble... */
		if(current_time > next_check)
			hst->next_check = current_time;
		else
			hst->next_check = next_check;

		/* make sure we rescheduled the next service check at a valid time */
		preferred_time = hst->next_check;
		get_next_valid_time(preferred_time, &next_valid_time, hst->check_period_ptr);
		hst->next_check = next_valid_time;

		/* hosts with non-recurring intervals do not get rescheduled if we're in a HARD or UP state */
		if(hst->check_interval == 0 && (hst->state_type == HARD_STATE || hst->current_state == HOST_UP))
			hst->should_be_scheduled = FALSE;

		/* host with active checks disabled do not get rescheduled */
		if(hst->checks_enabled == FALSE)
			hst->should_be_scheduled = FALSE;

		/* schedule a non-forced check if we can */
		if(hst->should_be_scheduled == TRUE) {
			schedule_host_check(hst, hst->next_check, CHECK_OPTION_NONE);
			}
		}

	/* update host status - for both active (scheduled) and passive (non-scheduled) hosts */
	update_host_status(hst, FALSE);

	/* run async checks of all hosts we added above */
	/* don't run a check if one is already executing or we can get by with a cached state */
	for(hostlist_item = check_hostlist; hostlist_item != NULL; hostlist_item = hostlist_item->next) {
		run_async_check = TRUE;
		temp_host = (host *)hostlist_item->object_ptr;

		log_debug_info(DEBUGL_CHECKS, 2, "ASYNC CHECK OF HOST: %s, CURRENTTIME: %lu, LASTHOSTCHECK: %lu, CACHEDTIMEHORIZON: %lu, USECACHEDRESULT: %d, ISEXECUTING: %d\n", temp_host->name, current_time, temp_host->last_check, check_timestamp_horizon, use_cached_result, temp_host->is_executing);

		if(use_cached_result == TRUE && ((current_time - temp_host->last_check) <= check_timestamp_horizon))
			run_async_check = FALSE;
		if(temp_host->is_executing == TRUE)
			run_async_check = FALSE;
		if(run_async_check == TRUE)
			run_async_host_check_3x(temp_host, CHECK_OPTION_NONE, 0.0, FALSE, FALSE, NULL, NULL);
		}
	free_objectlist(&check_hostlist);

	return OK;
	}



/* checks viability of performing a host check */
int check_host_check_viability_3x(host *hst, int check_options, int *time_is_valid, time_t *new_time) {
	int result = OK;
	int perform_check = TRUE;
	time_t current_time = 0L;
	time_t preferred_time = 0L;
	int check_interval = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_host_check_viability_3x()\n");

	/* make sure we have a host */
	if(hst == NULL)
		return ERROR;

	/* get the check interval to use if we need to reschedule the check */
	if(hst->state_type == SOFT_STATE && hst->current_state != HOST_UP)
		check_interval = (hst->retry_interval * interval_length);
	else
		check_interval = (hst->check_interval * interval_length);

	/* make sure check interval is positive - otherwise use 5 minutes out for next check */
	if(check_interval <= 0)
		check_interval = 300;

	/* get the current time */
	time(&current_time);

	/* initialize the next preferred check time */
	preferred_time = current_time;

	/* can we check the host right now? */
	if(!(check_options & CHECK_OPTION_FORCE_EXECUTION)) {

		/* if checks of the host are currently disabled... */
		if(hst->checks_enabled == FALSE) {
			preferred_time = current_time + check_interval;
			perform_check = FALSE;
			}

		/* make sure this is a valid time to check the host */
		if(check_time_against_period((unsigned long)current_time, hst->check_period_ptr) == ERROR) {
			preferred_time = current_time;
			if(time_is_valid)
				*time_is_valid = FALSE;
			perform_check = FALSE;
			}

		/* check host dependencies for execution */
		if(check_host_dependencies(hst, EXECUTION_DEPENDENCY) == DEPENDENCIES_FAILED) {
			preferred_time = current_time + check_interval;
			perform_check = FALSE;
			}
		}

	/* pass back the next viable check time */
	if(new_time)
		*new_time = preferred_time;

	result = (perform_check == TRUE) ? OK : ERROR;

	return result;
	}



/* adjusts current host check attempt before a new check is performed */
int adjust_host_check_attempt_3x(host *hst, int is_active) {

	log_debug_info(DEBUGL_FUNCTIONS, 0, "adjust_host_check_attempt_3x()\n");

	if(hst == NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS, 2, "Adjusting check attempt number for host '%s': current attempt=%d/%d, state=%d, state type=%d\n", hst->name, hst->current_attempt, hst->max_attempts, hst->current_state, hst->state_type);

	/* if host is in a hard state, reset current attempt number */
	if(hst->state_type == HARD_STATE)
		hst->current_attempt = 1;

	/* if host is in a soft UP state, reset current attempt number (active checks only) */
	else if(is_active == TRUE && hst->state_type == SOFT_STATE && hst->current_state == HOST_UP)
		hst->current_attempt = 1;

	/* increment current attempt number */
	else if(hst->current_attempt < hst->max_attempts)
		hst->current_attempt++;

	log_debug_info(DEBUGL_CHECKS, 2, "New check attempt number = %d\n", hst->current_attempt);

	return OK;
	}



/* determination of the host's state based on route availability*/
/* used only to determine difference between DOWN and UNREACHABLE states */
int determine_host_reachability(host *hst) {
	int state = HOST_DOWN;
	host *parent_host = NULL;
	hostsmember *temp_hostsmember = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "determine_host_reachability()\n");

	if(hst == NULL)
		return HOST_DOWN;

	log_debug_info(DEBUGL_CHECKS, 2, "Determining state of host '%s': current state=%d\n", hst->name, hst->current_state);

	/* host is UP - no translation needed */
	if(hst->current_state == HOST_UP) {
		state = HOST_UP;
		log_debug_info(DEBUGL_CHECKS, 2, "Host is UP, no state translation needed.\n");
		}

	/* host has no parents, so it is DOWN */
	else if(hst->parent_hosts == NULL) {
		state = HOST_DOWN;
		log_debug_info(DEBUGL_CHECKS, 2, "Host has no parents, so it is DOWN.\n");
		}

	/* check all parent hosts to see if we're DOWN or UNREACHABLE */
	else {

		for(temp_hostsmember = hst->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {

			if((parent_host = temp_hostsmember->host_ptr) == NULL)
				continue;

			/* bail out as soon as we find one parent host that is UP */
			if(parent_host->current_state == HOST_UP) {
				/* set the current state */
				state = HOST_DOWN;
				log_debug_info(DEBUGL_CHECKS, 2, "At least one parent (%s) is up, so host is DOWN.\n", parent_host->name);
				break;
				}
			}
		/* no parents were up, so this host is UNREACHABLE */
		if(temp_hostsmember == NULL) {
			state = HOST_UNREACHABLE;
			log_debug_info(DEBUGL_CHECKS, 2, "No parents were up, so host is UNREACHABLE.\n");
			}
		}

	return state;
	}



/******************************************************************/
/****************** HOST STATE HANDLER FUNCTIONS ******************/
/******************************************************************/


/* top level host state handler - occurs after every host check (soft/hard and active/passive) */
int handle_host_state(host *hst) {
	int state_change = FALSE;
	int hard_state_change = FALSE;
	time_t current_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "handle_host_state()\n");

	/* get current time */
	time(&current_time);

	/* obsess over this host check */
	obsessive_compulsive_host_check_processor(hst);

	/* update performance data */
	update_host_performance_data(hst);

	/* record latest time for current state */
	switch(hst->current_state) {
		case HOST_UP:
			hst->last_time_up = current_time;
			break;
		case HOST_DOWN:
			hst->last_time_down = current_time;
			break;
		case HOST_UNREACHABLE:
			hst->last_time_unreachable = current_time;
			break;
		default:
			break;
		}

	/* has the host state changed? */
	if(hst->last_state != hst->current_state || (hst->current_state == HOST_UP && hst->state_type == SOFT_STATE))
		state_change = TRUE;

	if(hst->current_attempt >= hst->max_attempts && hst->last_hard_state != hst->current_state)
		hard_state_change = TRUE;

	/* if the host state has changed... */
	if(state_change == TRUE || hard_state_change == TRUE) {

		/* reset the next and last notification times */
		hst->last_host_notification = (time_t)0;
		hst->next_host_notification = (time_t)0;

		/* reset notification suppression option */
		hst->no_more_notifications = FALSE;

		/* reset the acknowledgement flag if necessary */
		if(hst->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL && (state_change == TRUE || hard_state_change == FALSE)) {

			hst->problem_has_been_acknowledged = FALSE;
			hst->acknowledgement_type = ACKNOWLEDGEMENT_NONE;

			/* remove any non-persistant comments associated with the ack */
			delete_host_acknowledgement_comments(hst);
			}
		else if(hst->acknowledgement_type == ACKNOWLEDGEMENT_STICKY && hst->current_state == HOST_UP) {

			hst->problem_has_been_acknowledged = FALSE;
			hst->acknowledgement_type = ACKNOWLEDGEMENT_NONE;

			/* remove any non-persistant comments associated with the ack */
			delete_host_acknowledgement_comments(hst);
			}

		}

	/* Not sure about this, but is old behaviour */
	if(hst->last_hard_state != hst->current_state)
		hard_state_change = TRUE;

	if(state_change == TRUE || hard_state_change == TRUE) {

		/* update last state change times */
		hst->last_state_change = current_time;
		if(hst->state_type == HARD_STATE)
			hst->last_hard_state_change = current_time;

		/* update the event id */
		hst->last_event_id = hst->current_event_id;
		hst->current_event_id = next_event_id;
		next_event_id++;

		/* update the problem id when transitioning to a problem state */
		if(hst->last_state == HOST_UP) {
			/* don't reset last problem id, or it will be zero the next time a problem is encountered */
			/*hst->last_problem_id=hst->current_problem_id;*/
			hst->current_problem_id = next_problem_id;
			next_problem_id++;
			}

		/* clear the problem id when transitioning from a problem state to an UP state */
		if(hst->current_state == HOST_UP) {
			hst->last_problem_id = hst->current_problem_id;
			hst->current_problem_id = 0L;
			}

		/* write the host state change to the main log file */
		if(hst->state_type == HARD_STATE || (hst->state_type == SOFT_STATE && log_host_retries == TRUE))
			log_host_event(hst);

		/* check for start of flexible (non-fixed) scheduled downtime */
		/* CHANGED 08-05-2010 EG flex downtime can now start on soft states */
		/*if(hst->state_type==HARD_STATE)*/
		check_pending_flex_host_downtime(hst);

		/* notify contacts about the recovery or problem if its a "hard" state */
		if(hst->state_type == HARD_STATE)
			host_notification(hst, NOTIFICATION_NORMAL, NULL, NULL, NOTIFICATION_OPTION_NONE);

		/* handle the host state change */
		handle_host_event(hst);

		/* the host just recovered, so reset the current host attempt */
		if(hst->current_state == HOST_UP)
			hst->current_attempt = 1;

		/* the host recovered, so reset the current notification number and state flags (after the recovery notification has gone out) */
		if(hst->current_state == HOST_UP) {
			hst->current_notification_number = 0;
			hst->notified_on_down = FALSE;
			hst->notified_on_unreachable = FALSE;
			}
		}

	/* else the host state has not changed */
	else {

		/* notify contacts if host is still down or unreachable */
		if(hst->current_state != HOST_UP && hst->state_type == HARD_STATE)
			host_notification(hst, NOTIFICATION_NORMAL, NULL, NULL, NOTIFICATION_OPTION_NONE);

		/* if we're in a soft state and we should log host retries, do so now... */
		if(hst->state_type == SOFT_STATE && log_host_retries == TRUE)
			log_host_event(hst);
		}

	return OK;
	}


/* parse raw plugin output and return: short and long output, perf data */
int parse_check_output(char *buf, char **short_output, char **long_output, char **perf_data, int escape_newlines_please, int newlines_are_escaped) {
	int current_line = 0;
	int found_newline = FALSE;
	int eof = FALSE;
	int used_buf = 0;
	int dbuf_chunk = 1024;
	dbuf db1;
	dbuf db2;
	char *ptr = NULL;
	int in_perf_data = FALSE;
	char *tempbuf = NULL;
	register int x = 0;
	register int y = 0;

	/* initialize values */
	if(short_output)
		*short_output = NULL;
	if(long_output)
		*long_output = NULL;
	if(perf_data)
		*perf_data = NULL;

	/* nothing to do */
	if(buf == NULL || !strcmp(buf, ""))
		return OK;

	used_buf = strlen(buf) + 1;

	/* initialize dynamic buffers (1KB chunk size) */
	dbuf_init(&db1, dbuf_chunk);
	dbuf_init(&db2, dbuf_chunk);

	/* unescape newlines and escaped backslashes first */
	if(newlines_are_escaped == TRUE) {
		for(x = 0, y = 0; buf[x] != '\x0'; x++) {
			if(buf[x] == '\\' && buf[x + 1] == '\\') {
				x++;
				buf[y++] = buf[x];
				}
			else if(buf[x] == '\\' && buf[x + 1] == 'n') {
				x++;
				buf[y++] = '\n';
				}
			else
				buf[y++] = buf[x];
			}
		buf[y] = '\x0';
		}

	/* process each line of input */
	for(x = 0; eof == FALSE; x++) {

		/* we found the end of a line */
		if(buf[x] == '\n')
			found_newline = TRUE;
		else if(buf[x] == '\\' && buf[x + 1] == 'n' && newlines_are_escaped == TRUE) {
			found_newline = TRUE;
			buf[x] = '\x0';
			x++;
			}
		else if(buf[x] == '\x0') {
			found_newline = TRUE;
			eof = TRUE;
			}
		else
			found_newline = FALSE;

		if(found_newline == TRUE) {

			current_line++;

			/* handle this line of input */
			buf[x] = '\x0';
			if((tempbuf = (char *)strdup(buf))) {

				/* first line contains short plugin output and optional perf data */
				if(current_line == 1) {

					/* get the short plugin output */
					if((ptr = strtok(tempbuf, "|"))) {
						if(short_output)
							*short_output = (char *)strdup(ptr);

						/* get the optional perf data */
						if((ptr = strtok(NULL, "\n")))
							dbuf_strcat(&db2, ptr);
						}
					}

				/* additional lines contain long plugin output and optional perf data */
				else {

					/* rest of the output is perf data */
					if(in_perf_data == TRUE) {
						dbuf_strcat(&db2, tempbuf);
						dbuf_strcat(&db2, " ");
						}

					/* we're still in long output */
					else {

						/* perf data separator has been found */
						if(strstr(tempbuf, "|")) {

							/* NOTE: strtok() causes problems if first character of tempbuf='|', so use my_strtok() instead */
							/* get the remaining long plugin output */
							if((ptr = my_strtok(tempbuf, "|"))) {

								if(current_line > 2)
									dbuf_strcat(&db1, "\n");
								dbuf_strcat(&db1, ptr);

								/* get the perf data */
								if((ptr = my_strtok(NULL, "\n"))) {
									dbuf_strcat(&db2, ptr);
									dbuf_strcat(&db2, " ");
									}
								}

							/* set the perf data flag */
							in_perf_data = TRUE;
							}

						/* just long output */
						else {
							if(current_line > 2)
								dbuf_strcat(&db1, "\n");
							dbuf_strcat(&db1, tempbuf);
							}
						}
					}

				my_free(tempbuf);
				tempbuf = NULL;
				}


			/* shift data back to front of buffer and adjust counters */
			memmove((void *)&buf[0], (void *)&buf[x + 1], (size_t)((int)used_buf - x - 1));
			used_buf -= (x + 1);
			buf[used_buf] = '\x0';
			x = -1;
			}
		}

	/* save long output */
	if(long_output && (db1.buf && strcmp(db1.buf, ""))) {

		if(escape_newlines_please == FALSE)
			*long_output = (char *)strdup(db1.buf);

		else {

			/* escape newlines (and backslashes) in long output */
			if((tempbuf = (char *)malloc((strlen(db1.buf) * 2) + 1))) {

				for(x = 0, y = 0; db1.buf[x] != '\x0'; x++) {

					if(db1.buf[x] == '\n') {
						tempbuf[y++] = '\\';
						tempbuf[y++] = 'n';
						}
					else if(db1.buf[x] == '\\') {
						tempbuf[y++] = '\\';
						tempbuf[y++] = '\\';
						}
					else
						tempbuf[y++] = db1.buf[x];
					}

				tempbuf[y] = '\x0';
				*long_output = (char *)strdup(tempbuf);
				my_free(tempbuf);
				}
			}
		}

	/* save perf data */
	if(perf_data && (db2.buf && strcmp(db2.buf, "")))
		*perf_data = (char *)strdup(db2.buf);

	/* strip short output and perf data */
	if(short_output)
		strip(*short_output);
	if(perf_data)
		strip(*perf_data);

	/* free dynamic buffers */
	dbuf_free(&db1);
	dbuf_free(&db2);

	return OK;
	}


