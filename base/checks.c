/*****************************************************************************
 *
 * CHECKS.C - Service and host check functions for Nagios
 *
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
#include "../include/workers.h"

/*#define DEBUG_CHECKS*/
/*#define DEBUG_HOST_CHECKS 1*/

#define replace_semicolons(output, ptr) do { ptr = output; while ((ptr = strchr(ptr, ';')) != NULL) { * ptr = ':'; } } while (0)

#ifdef USE_EVENT_BROKER
#include "../include/neberrors.h"
#endif

/******************************************************************/
/********************** CHECK REAPER FUNCTIONS ********************/
/******************************************************************/

/* reaps host and service check results */
int reap_check_results(void)
{
	int reaped_checks = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "reap_check_results() start\n");

	/* process files in the check result queue */
	reaped_checks = process_check_result_queue(check_result_path);

	log_debug_info(DEBUGL_FUNCTIONS, 0, "reap_check_results() reaped %d checks end\n", reaped_checks);

	return OK;
}




/******************************************************************/
/****************** SERVICE MONITORING FUNCTIONS ******************/
/******************************************************************/

/* executes a scheduled service check */
int run_scheduled_service_check(service *svc, int check_options, double latency)
{
	int result = OK;
	time_t current_time = 0L;
	time_t preferred_time = 0L;
	time_t next_valid_time = 0L;
	int time_is_valid = TRUE;

	if (svc == NULL) {
		return ERROR;
	}

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
	if (result == ERROR) {

		log_debug_info(DEBUGL_CHECKS, 1, "Unable to run scheduled service check at this time\n");

		/* only attempt to (re)schedule checks that should get checked... */
		if (svc->should_be_scheduled == TRUE) {

			/* get current time */
			time(&current_time);

			/* determine next time we should check the service if needed */
			/* if service has no check interval, schedule it again for 5 minutes from now */
			if (current_time >= preferred_time) {
				preferred_time = current_time + ((svc->check_interval <= 0) ? 300 : (svc->check_interval * interval_length));
			}

			/* make sure we rescheduled the next service check at a valid time */
			get_next_valid_time(preferred_time, &next_valid_time, svc->check_period_ptr);

			/*
			 * If we really can't reschedule the service properly, we
			 * just push the check to preferred_time plus some reasonable
			 * random value and try again then.
			 */
			if (time_is_valid == FALSE &&  check_time_against_period(next_valid_time, svc->check_period_ptr) == ERROR) {

				svc->next_check = preferred_time + ranged_urand(0, check_window(svc));

				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check of service '%s' on host '%s' could not be rescheduled properly.  Scheduling check for %s...\n", svc->description, svc->host_name, ctime(&preferred_time));

				log_debug_info(DEBUGL_CHECKS, 1, "Unable to find any valid times to reschedule the next service check!\n");
			}

			/* this service could be rescheduled... */
			else {
				svc->next_check = next_valid_time;
				if (next_valid_time > preferred_time) {
					/* Next valid time is further in the future because of
					 * timeperiod constraints. Add a random amount so we
					 * don't get all checks subject to that timeperiod
					 * constraint scheduled at the same time
					 */
					svc->next_check = reschedule_within_timeperiod(next_valid_time, svc->check_period_ptr, check_window(svc));
				}
				svc->should_be_scheduled = TRUE;

				log_debug_info(DEBUGL_CHECKS, 1, "Rescheduled next service check for %s", ctime(&next_valid_time));
			}
		}

		/*
		 * reschedule the next service check - unless we couldn't
		 * find a valid next check time, but keep original options
		 */
		if (svc->should_be_scheduled == TRUE) {
			schedule_service_check(svc, svc->next_check, check_options);
		}

		/* update the status log */
		update_service_status(svc, FALSE);

		return ERROR;
	}

	return OK;
}


/* forks a child process to run a service check, but does not wait for the service check result */
int run_async_service_check(service *svc, int check_options, double latency, int scheduled_check, int reschedule_check, int *time_is_valid, time_t *preferred_time)
{
	nagios_macros mac;
	char *raw_command = NULL;
	char *processed_command = NULL;
	struct timeval start_time, end_time;
	host *temp_host = NULL;
	double old_latency = 0.0;
	check_result *cr;
	int runchk_result = OK;
	int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;
#ifdef USE_EVENT_BROKER
	int neb_result = OK;
#endif

	log_debug_info(DEBUGL_FUNCTIONS, 0, "run_async_service_check()\n");

	/* make sure we have something */
	if (svc == NULL) {
		return ERROR;
	}

	/* is the service check viable at this time? */
	if (check_service_check_viability(svc, check_options, time_is_valid, preferred_time) == ERROR) {
		return ERROR;
	}

	/* find the host associated with this service */
	if ((temp_host = svc->host_ptr) == NULL) {
		return ERROR;
	}

	/******** GOOD TO GO FOR A REAL SERVICE CHECK AT THIS POINT ********/

#ifdef USE_EVENT_BROKER
	/* initialize start/end times */
	start_time.tv_sec = 0L;
	start_time.tv_usec = 0L;
	end_time.tv_sec = 0L;
	end_time.tv_usec = 0L;

	/* send data to event broker */
	neb_result = broker_service_check(NEBTYPE_SERVICECHECK_ASYNC_PRECHECK, NEBFLAG_NONE, NEBATTR_NONE, svc, CHECK_TYPE_ACTIVE, start_time, end_time, svc->check_command, svc->latency, 0.0, 0, FALSE, 0, NULL, NULL, NULL);

	/* neb module wants to cancel the service check - the check will be rescheduled for a later time by the scheduling logic */
	if (neb_result == NEBERROR_CALLBACKCANCEL) {

		log_debug_info(DEBUGL_CHECKS, 0, "Check of service '%s' on host '%s' (id=%u) was cancelled by a module\n", svc->description, svc->host_name, svc->id);

		if (preferred_time) {
			*preferred_time += (svc->check_interval * interval_length);
		}
		return ERROR;
	}

	/* neb module wants to override (or cancel) the service check - perhaps it will check the service itself */
	/* NOTE: if a module does this, it has to do a lot of the stuff found below to make sure things don't get whacked out of shape! */
	/* NOTE: if would be easier for modules to override checks when the NEBTYPE_SERVICECHECK_INITIATE event is called (later) */
	if (neb_result == NEBERROR_CALLBACKOVERRIDE) {

		log_debug_info(DEBUGL_CHECKS, 0, "Check of service '%s' on host '%s' (id=%u) was overridden by a module\n", svc->description, svc->host_name, svc->id);
		return OK;
	}
#endif


	log_debug_info(DEBUGL_CHECKS, 0, "Checking service '%s' on host '%s'...\n", svc->description, svc->host_name);

	/* clear check options - we don't want old check options retained */
	/* only clear check options for scheduled checks - ondemand checks shouldn't affected retained check options */
	if (scheduled_check == TRUE) {
		svc->check_options = CHECK_OPTION_NONE;
	}

	/* update latency for macros, event broker, save old value for later */
	old_latency = svc->latency;
	svc->latency = latency;

	/* grab the host and service macro variables */
	memset(&mac, 0, sizeof(mac));
	grab_host_macros_r(&mac, temp_host);
	grab_service_macros_r(&mac, svc);

	/* get the raw command line */
	get_raw_command_line_r(&mac, svc->check_command_ptr, svc->check_command, &raw_command, macro_options);
	if (raw_command == NULL) {
		clear_volatile_macros_r(&mac);
		log_debug_info(DEBUGL_CHECKS, 0, "Raw check command for service '%s' on host '%s' was NULL - aborting.\n", svc->description, svc->host_name);
		if (preferred_time) {
			*preferred_time += (svc->check_interval * interval_length);
		}
		svc->latency = old_latency;
		return ERROR;
	}

	/* process any macros contained in the argument */
	process_macros_r(&mac, raw_command, &processed_command, macro_options);
	my_free(raw_command);
	if (processed_command == NULL) {
		clear_volatile_macros_r(&mac);
		log_debug_info(DEBUGL_CHECKS, 0, "Processed check command for service '%s' on host '%s' was NULL - aborting.\n", svc->description, svc->host_name);
		if (preferred_time) {
			*preferred_time += (svc->check_interval * interval_length);
		}
		svc->latency = old_latency;
		return ERROR;
	}

	/* get the command start time */
	gettimeofday(&start_time, NULL);

	cr = calloc(1, sizeof(*cr));
	if (!cr) {
		clear_volatile_macros_r(&mac);
		svc->latency = old_latency;
		my_free(processed_command);
		return ERROR;
	}
	init_check_result(cr);

	/* save check info */
	cr->object_check_type = SERVICE_CHECK;
	cr->check_type = CHECK_TYPE_ACTIVE;
	cr->check_options = check_options;
	cr->scheduled_check = scheduled_check;
	cr->reschedule_check = reschedule_check;
	cr->latency = latency;
	cr->start_time = start_time;
	cr->finish_time = start_time;
	cr->early_timeout = FALSE;
	cr->exited_ok = TRUE;
	cr->return_code = STATE_OK;
	cr->output = NULL;
	cr->host_name = (char *)strdup(svc->host_name);
	cr->service_description = (char *)strdup(svc->description);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	neb_result = broker_service_check(NEBTYPE_SERVICECHECK_INITIATE, NEBFLAG_NONE, NEBATTR_NONE, svc, CHECK_TYPE_ACTIVE, start_time, end_time, svc->check_command, svc->latency, 0.0, service_check_timeout, FALSE, 0, processed_command, NULL, cr);

	/* neb module wants to override the service check - perhaps it will check the service itself */
	if (neb_result == NEBERROR_CALLBACKOVERRIDE) {
		clear_volatile_macros_r(&mac);
		svc->latency = old_latency;
		free_check_result(cr);
		my_free(cr);
		my_free(processed_command);
		return OK;
	}
#endif

	/* reset latency (permanent value will be set later) */
	svc->latency = old_latency;

	/* paw off the check to a worker to run */
	runchk_result = wproc_run_check(cr, processed_command, &mac);
	if (runchk_result == ERROR) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Unable to run check for service '%s' on host '%s'\n", svc->description, svc->host_name);
	}
	else {
		/* do the book-keeping */
		currently_running_service_checks++;
		svc->is_executing = TRUE;
		update_check_stats((scheduled_check == TRUE) ? ACTIVE_SCHEDULED_SERVICE_CHECK_STATS : ACTIVE_ONDEMAND_SERVICE_CHECK_STATS, start_time.tv_sec);
	}

	/* free memory */
	my_free(processed_command);
	clear_volatile_macros_r(&mac);

	return runchk_result;
}

/* Start of inline helper functions for handle async host/service check result functions 
   BH 03 Dec 2017
   The giant monolithic async helper functions were becoming difficult to maintain 
   So I broke them out into smaller manageable chunks where a side-by-side comparison
   of the two functions is a bit more reasonable. This would have been a lot easier with
   dynamic type casting, or even some macro magic - but at least this way it isn't as 
   messy as it would have been with macros.

   When introducing new inline helper functions, the goal is so that both of the functions
   can sit on the same screen at the same time - for obvious reasons (I hope).
   Try to keep them concise */

/* Bit of logic for determining an adequate return code */
int get_service_check_return_code(service *svc, check_result *cr)
{
	int rc;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "get_service_check_return_code()\n");

	if (NULL == svc || NULL == cr) {
		return STATE_UNKNOWN;
	}

	/* return now if it's a passive check */
	if (cr->check_type != CHECK_TYPE_ACTIVE) {
		return cr->return_code;
	}

	/* grab the return code */
	rc = cr->return_code;

	/* did the check result have an early timeout? */
	if (cr->early_timeout == TRUE) {

		my_free(svc->plugin_output);
		my_free(svc->long_plugin_output);
		my_free(svc->perf_data);

		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check of service '%s' on host '%s' timed out after %.3fs!\n", svc->description, svc->host_name, svc->execution_time);
		asprintf(&svc->plugin_output, "(Service check timed out after %.2lf seconds)", svc->execution_time);

		rc = service_check_timeout_state;
	}

	/* if there was some error running the command, just skip it (this shouldn't be happening) */
	else if (cr->exited_ok == FALSE) {
		
		my_free(svc->plugin_output);
		my_free(svc->long_plugin_output);
		my_free(svc->perf_data);

		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning:  Check of service '%s' on host '%s' did not exit properly!\n", svc->description, svc->host_name);
		svc->plugin_output = (char *)strdup("(Service check did not exit properly)");

		rc = STATE_CRITICAL;
	}

	/* 126 is a return code for non-executable */
	else if (cr->return_code == 126) {
		
		my_free(svc->plugin_output);
		my_free(svc->long_plugin_output);
		my_free(svc->perf_data);

		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Return code of 126 for service '%s' on host '%s' may indicate a non-executable plugin.\n",
			svc->description, svc->host_name);
		svc->plugin_output = strdup("(Return code of 126 is out of bounds. Check if plugin is executable)");
		rc = STATE_CRITICAL;
	}

	/* 127 is a return code for non-existent */
	else if (cr->return_code == 127) {
		
		my_free(svc->plugin_output);
		my_free(svc->long_plugin_output);
		my_free(svc->perf_data);

		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Return code of 127 for service '%s' on host '%s' may indicate this plugin doesn't exist.\n",
			svc->description, svc->host_name);
		svc->plugin_output = strdup("(Return code of 127 is out of bounds. Check if plugin exists)");
		rc = STATE_CRITICAL;
	}

	/* make sure the return code is within bounds */
	else if (cr->return_code < 0 || cr->return_code > 3) {

		logit(NSLOG_RUNTIME_WARNING, TRUE, 
			"Warning: Return code of %d for check of service '%s' on host '%s' was out of bounds.\n", 
			cr->return_code,
			svc->description,
			svc->host_name);

		asprintf(&svc->plugin_output, "(Return code of %d for service '%s' on host '%s' was out of bounds)",
			cr->return_code,
			svc->description,
			svc->host_name);

		rc = STATE_CRITICAL;
	}

	return rc;
}

/* Bit of logic for determining an adequate return code */
int get_host_check_return_code(host *hst, check_result *cr)
{
	int rc;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "get_host_check_return_code()\n");

	if (hst == NULL || cr == NULL) {
		return HOST_UNREACHABLE;
	}

	/* return now if it's a passive check */
	if (cr->check_type != CHECK_TYPE_ACTIVE) {
		return cr->return_code;
	}

	/* get the unprocessed return code */
	rc = cr->return_code;

	/* did the check result have an early timeout? */
	if (cr->early_timeout) {

		my_free(hst->plugin_output);
		my_free(hst->long_plugin_output);
		my_free(hst->perf_data);

		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check of host '%s' timed out after %.2lf seconds\n", hst->name, hst->execution_time);
		asprintf(&hst->plugin_output, "(Host check timed out after %.2lf seconds)", hst->execution_time);

		rc = HOST_UNREACHABLE;
	}

	/* if there was some error running the command, just skip it (this shouldn't be happening) */
	else if (cr->exited_ok == FALSE) {

		my_free(hst->plugin_output);
		my_free(hst->long_plugin_output);
		my_free(hst->perf_data);

		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning:  Check of host '%s' did not exit properly!\n", hst->name);
		hst->plugin_output = (char *)strdup("(Host check did not exit properly)");

		rc = HOST_UNREACHABLE;
	}

	/* 126 is a return code for non-executable */
	else if (cr->return_code == 126) {

		my_free(hst->plugin_output);
		my_free(hst->long_plugin_output);
		my_free(hst->perf_data);

		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Return code of 126 for host '%s' may indicate a non-executable plugin.\n",
			hst->name);
		hst->plugin_output = strdup("(Return code of 126 is out of bounds. Check if plugin is executable)");
		rc = HOST_UNREACHABLE;
	}

	/* 127 is a return code for non-existent */
	else if (cr->return_code == 127) {

		my_free(hst->plugin_output);
		my_free(hst->long_plugin_output);
		my_free(hst->perf_data);

		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Return code of 127 for host '%s' may indicate this plugin doesn't exist.\n",
			hst->name);
		hst->plugin_output = strdup("(Return code of 127 is out of bounds. Check if plugin exists)");
		rc = HOST_UNREACHABLE;
	}

	/* make sure the return code is within bounds */
	else if (cr->return_code < 0 || cr->return_code > 3) {

		my_free(hst->plugin_output);
		my_free(hst->long_plugin_output);
		my_free(hst->perf_data);

		logit(NSLOG_RUNTIME_WARNING, TRUE, 
			"Warning: Return code of %d for check of host '%s' was out of bounds.\n", 
			cr->return_code,
			hst->name);

		asprintf(&hst->plugin_output, "(Return code of %d for host '%s' was out of bounds)",
			cr->return_code,
			hst->name);

		rc = HOST_UNREACHABLE;
	}

	/* a NULL host check command means we should assume the host is UP */
	if (hst->check_command == NULL) {
		my_free(hst->plugin_output);
		hst->plugin_output = (char *)strdup("(Host assumed to be UP)");
		rc = HOST_UP;
	}

	/* if we're not doing aggressive host checking, let WARNING states indicate the host is up (fake the result to be HOST_UP) */
	else if (use_aggressive_host_checking == FALSE && rc == STATE_WARNING) {
		rc = HOST_UP;
	}

	/* any problem state indicates the host is not UP */
	else if (rc != HOST_UP) {
		rc = HOST_DOWN;
	}
		
	return rc;
}

/******************************************************************************
 ******* Calculate check result exec time
 *****************************************************************************/
static inline double calculate_check_result_execution_time(check_result *cr)
{
	double execution_time = 0.0;

	if (cr != NULL) {
		
		double start_s = cr->start_time.tv_sec;
		double start_us = cr->start_time.tv_usec;
		double finish_s = cr->finish_time.tv_sec;
		double finish_us = cr->finish_time.tv_usec;

		execution_time = ((finish_s - start_s) + ((finish_us - start_us) / 1000.0) / 1000.0);
		if (execution_time < 0.0) {
			execution_time = 0.0;
		}
	}

	return execution_time;
}

/******************************************************************************
 ******* Last state ended
 *****************************************************************************/
static inline void record_last_service_state_ended(service * svc) 
{
	switch(svc->last_state) {
	case STATE_OK:
		svc->last_time_ok = svc->last_check;
		break;
	case STATE_WARNING:
		svc->last_time_warning = svc->last_check;
		break;
	case STATE_UNKNOWN:
		svc->last_time_unknown = svc->last_check;
		break;
	case STATE_CRITICAL:
		svc->last_time_critical = svc->last_check;
		break;
	default:
		break;
	}
}
/*****************************************************************************/
static inline void record_last_host_state_ended(host * hst)
{
	switch(hst->last_state) {
	case HOST_UP:
		hst->last_time_up = hst->last_check;
		break;
	case HOST_DOWN:
		hst->last_time_down = hst->last_check;
		break;
	case HOST_UNREACHABLE:
		hst->last_time_unreachable = hst->last_check;
		break;
	default:
		break;
	}
}

/******************************************************************************
 ******* Logic chunks for when an object is passive
 *****************************************************************************/
static inline int service_is_passive(service *svc, check_result *cr)
{
	if (accept_passive_service_checks == FALSE) {
		log_debug_info(DEBUGL_CHECKS, 0, "Discarding passive service check result because passive service checks are disabled globally.\n");
		return FALSE;
	}

	if (svc->accept_passive_checks == FALSE) {
		log_debug_info(DEBUGL_CHECKS, 0, "Discarding passive service check result because passive checks are disabled for this service.\n");
		return FALSE;
	}

	svc->check_type = CHECK_TYPE_PASSIVE;

	/* update check statistics for passive checks */
	update_check_stats(PASSIVE_SERVICE_CHECK_STATS, cr->start_time.tv_sec);

	/* log passive checks - we need to do this here, as some may bypass external commands by getting dropped in checkresults dir */
	if (log_passive_checks == TRUE) {
		logit(NSLOG_PASSIVE_CHECK, FALSE, "PASSIVE SERVICE CHECK: %s;%s;%d;%s\n", 
			svc->host_name, 
			svc->description, 
			svc->current_state, 
			svc->plugin_output);
	}

	return TRUE;
}
/*****************************************************************************/
static inline int host_is_passive(host *hst, check_result *cr)
{
	if (accept_passive_host_checks == FALSE) {
		log_debug_info(DEBUGL_CHECKS, 0, "Discarding passive host check result because passive host checks are disabled globally.\n");
		return FALSE;
	}
	if (hst->accept_passive_checks == FALSE) {
		log_debug_info(DEBUGL_CHECKS, 0, "Discarding passive host check result because passive checks are disabled for this host.\n");
		return FALSE;
	}

	hst->check_type = CHECK_TYPE_PASSIVE;

	/* update check stats for passive checks */
	update_check_stats(PASSIVE_HOST_CHECK_STATS, cr->start_time.tv_sec);

	/* log passive checks - we need to do this here as some may bypass external commands by getting dropped in checkresults dir */
	/* todo - check if current_state is right - i don't think it is! */
	if (log_passive_checks == TRUE) {
		logit(NSLOG_PASSIVE_CHECK, FALSE, "PASSIVE HOST CHECK: %s;%d;%s\n", 
			hst->name, 
			hst->current_state, 
			hst->plugin_output);
	}

	return TRUE;
}

/******************************************************************************
 ******* Logic chunks for when an object is active
 *****************************************************************************/
static inline void service_is_active(service *svc)
{
	svc->check_type = CHECK_TYPE_ACTIVE;

	/* decrement the number of service checks still out there... */
	if (currently_running_service_checks > 0) {
		currently_running_service_checks--;
	}

	svc->is_executing = FALSE;
}
/*****************************************************************************/
static inline void host_is_active(host *hst)
{
	hst->check_type = CHECK_TYPE_ACTIVE;

	/* decrement the number of host checks still out there... */
	if (currently_running_host_checks > 0) {
		currently_running_host_checks--;
	}

	hst->is_executing = FALSE;
}

/******************************************************************************
 ******* Generic debugging functions
 *****************************************************************************/
static inline void debug_async_service(service *svc, check_result *cr)
{
	log_debug_info(DEBUGL_CHECKS, 0, "** Handling %s async check result for service '%s' on host '%s' from '%s'... current state %d last_hard_state %d \n", 
		(cr->check_type == CHECK_TYPE_ACTIVE) ? "ACTIVE" : "PASSIVE",
		svc->description, 
		svc->host_name, 
		check_result_source(cr),
		svc->current_state,
		svc->last_hard_state);

	log_debug_info(DEBUGL_CHECKS, 1, 
		" * OPTIONS: %d, SCHEDULED: %d, RESCHEDULE: %d, EXITED OK: %d, RETURN CODE: %d, OUTPUT:\n%s\n", 
		cr->check_options, 
		cr->scheduled_check,
		cr->reschedule_check,
		cr->exited_ok,
		cr->return_code,
		(cr == NULL) ? "NULL" : cr->output);
}
/*****************************************************************************/
static inline void debug_async_host(host *hst, check_result *cr)
{
	log_debug_info(DEBUGL_CHECKS, 0, "** Handling %s async check result for host '%s' from '%s'...\n", 
		(cr->check_type == CHECK_TYPE_ACTIVE) ? "ACTIVE" : "PASSIVE",
		hst->name, 
		check_result_source(cr));

	log_debug_info(DEBUGL_CHECKS, 1, 
		" * OPTIONS: %d, SCHEDULED: %d, RESCHEDULE: %d, EXITED OK: %d, RETURN CODE: %d, OUTPUT:\n%s\n", 
		cr->check_options,
		cr->scheduled_check,
		cr->reschedule_check,
		cr->exited_ok,
		cr->return_code,
		(cr == NULL) ? "NULL" : cr->output);
}

/******************************************************************************
 ******* Logic chunks for checking object freshness
 *****************************************************************************/
static inline void service_fresh_check(service *svc, check_result *cr, time_t current_time)
{
	/* check freshness */
	if (cr->check_options & CHECK_OPTION_FRESHNESS_CHECK) {

		/* DISCARD INVALID FRESHNESS CHECK RESULTS */
		/* If a services goes stale, Nagios will initiate a forced check in order to freshen it.  There is a race condition whereby a passive check
		   could arrive between the 1) initiation of the forced check and 2) the time when the forced check result is processed here.  This would
		   make the service fresh again, so we do a quick check to make sure the service is still stale before we accept the check result. */
		if (is_service_result_fresh(svc, current_time, FALSE) == TRUE) {
			log_debug_info(DEBUGL_CHECKS, 0, "Discarding service freshness check result because the service is currently fresh (race condition avoided).\n");
			return;
		}

		/* clear the freshening flag (it would have been set if this service was determined to be stale) */	
		svc->is_being_freshened = FALSE;
	}
}
/*****************************************************************************/
static inline void host_fresh_check(host *hst, check_result *cr, time_t current_time)
{
	/* check freshness */
	if (cr->check_options & CHECK_OPTION_FRESHNESS_CHECK) {

		/* DISCARD INVALID FRESHNESS CHECK RESULTS */
		/* If a host goes stale, Nagios will initiate a forced check in order to freshen it.  There is a race condition whereby a passive check
		   could arrive between the 1) initiation of the forced check and 2) the time when the forced check result is processed here.  This would
		   make the host fresh again, so we do a quick check to make sure the host is still stale before we accept the check result. */
		if (is_host_result_fresh(hst, current_time, FALSE) == TRUE) {
			log_debug_info(DEBUGL_CHECKS, 0, "Discarding host freshness check result because the host is currently fresh (race condition avoided).\n");
			return;
		}

		/* clear the freshening flag (it would have been set if this host was determined to be stale) */	
		hst->is_being_freshened = FALSE;
	}
}

/******************************************************************************
 ******* Logic chunks for setting some of the initial flags, etc.
 *****************************************************************************/
static inline void service_initial_handling(service *svc, check_result *cr, char **old_plugin_output)
{
	char * temp_ptr = NULL;

	/* save old plugin output */
	if (svc->plugin_output) {
		* old_plugin_output = (char *)strdup(svc->plugin_output);
	}

	my_free(svc->plugin_output);
	my_free(svc->long_plugin_output);
	my_free(svc->perf_data);

	/* parse check output to get: (1) short output, (2) long output, (3) perf data */
	parse_check_output(cr->output, &svc->plugin_output, &svc->long_plugin_output, &svc->perf_data, TRUE, FALSE);

	/* make sure the plugin output isn't null */
	if (svc->plugin_output == NULL) {
		svc->plugin_output = (char *)strdup("(No output returned from plugin)");
	}
	/* otherwise replace the semicolons with colons */
	else {
		replace_semicolons(svc->plugin_output, temp_ptr);
	}

	log_debug_info(DEBUGL_CHECKS, 2, 
		"Parsing check output...\n"
		"Short Output: %s\n"
		"Long Output: %s\n"
		"Perf Data:	%s\n",
		svc->plugin_output,
		(svc->long_plugin_output == NULL) ? "NULL" : svc->long_plugin_output,
		(svc->perf_data == NULL) ? "NULL" : svc->perf_data);

	svc->latency = cr->latency;
	svc->execution_time = calculate_check_result_execution_time(cr);
	svc->last_check = cr->start_time.tv_sec;
	svc->should_be_scheduled = cr->reschedule_check;

	svc->last_state = svc->current_state;
	svc->current_state = get_service_check_return_code(svc, cr);
}
/*****************************************************************************/
static inline void host_initial_handling(host *hst, check_result *cr, char **old_plugin_output)
{
	char * temp_ptr = NULL;

	/* save old plugin output */
	if (hst->plugin_output) {
		* old_plugin_output = (char *)strdup(hst->plugin_output);
	}

	my_free(hst->plugin_output);
	my_free(hst->long_plugin_output);
	my_free(hst->perf_data);

	/* parse check output to get: (1) short output, (2) long output, (3) perf data */
	parse_check_output(cr->output, &hst->plugin_output, &hst->long_plugin_output, &hst->perf_data, TRUE, FALSE);

	/* make sure the plugin output isn't null */
	if (hst->plugin_output == NULL) {
		hst->plugin_output = (char *)strdup("(No output returned from host check)");
	}
	/* otherwise replace the semicolons with colons */
	else {
		replace_semicolons(hst->plugin_output, temp_ptr);
	}

	log_debug_info(DEBUGL_CHECKS, 2, 
		"Parsing check output...\n"
		"Short Output: %s\n"
		"Long Output: %s\n"
		"Perf Data:	%s\n",
		hst->plugin_output,
		(hst->long_plugin_output == NULL) ? "NULL" : hst->long_plugin_output,
		(hst->perf_data == NULL) ? "NULL" : hst->perf_data);

	hst->latency = cr->latency;
	hst->execution_time = calculate_check_result_execution_time(cr);
	hst->last_check = cr->start_time.tv_sec;
	hst->should_be_scheduled = cr->reschedule_check;

	hst->last_state = hst->current_state;
	hst->current_state = get_host_check_return_code(hst, cr);
}

/******************************************************************************
 ******* Logic for when an object has a notable change
 ******* Removes acknowledgement, advances event_id, etc.
 *****************************************************************************/
static inline void service_state_or_hard_state_type_change(service * svc, int state_change, int hard_state_change, int * log_event, int * handle_event)
{
	int state_or_type_change = FALSE;

	/* update the event and problem ids */
	if (state_change == TRUE) {

		svc->last_state_change = svc->last_check;

		/* always update the event id on a state change */
		svc->last_event_id = svc->current_event_id;
		svc->current_event_id = next_event_id;
		next_event_id++;

		/* update the problem id when transitioning to a problem state */
		if (svc->last_state == STATE_OK) {
			/* don't reset last problem id, or it will be zero the next time a problem is encountered */
			svc->current_problem_id = next_problem_id;
			next_problem_id++;
		}

		svc->state_type = SOFT_STATE;

		state_or_type_change = TRUE;
	}

	if (hard_state_change == TRUE) {

		svc->last_hard_state_change = svc->last_check;
		svc->last_state_change = svc->last_check;
		svc->last_hard_state = svc->current_state;
		svc->state_type = HARD_STATE;

		state_or_type_change = TRUE;
	}

	if (state_or_type_change) {

		/* check if service should go into downtime from flexible downtime */
		if (svc->pending_flex_downtime > 0) {
			check_pending_flex_service_downtime(svc);
		}

		/* reset notification times and suppression option */
		svc->last_notification = (time_t)0;
		svc->next_notification = (time_t)0;
		svc->no_more_notifications = FALSE;

		if ((svc->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL && (state_change == TRUE || hard_state_change == FALSE))
			|| (svc->acknowledgement_type == ACKNOWLEDGEMENT_STICKY && svc->current_state == STATE_OK)) {

			/* remove any non-persistant comments associated with the ack */
			svc->problem_has_been_acknowledged = FALSE;
			svc->acknowledgement_type = ACKNOWLEDGEMENT_NONE;
			delete_service_acknowledgement_comments(svc);
		}

		svc->should_be_scheduled = TRUE;

		*log_event = TRUE;
		*handle_event = TRUE;
	}
}
/*****************************************************************************/
static inline void host_state_or_hard_state_type_change(host * hst, int state_change, int hard_state_change, int * log_event, int * handle_event, int * send_notification)
{
	int state_or_type_change = FALSE;

	/* check if we simulate a hard state change */
	if (hst->check_type == CHECK_TYPE_PASSIVE && passive_host_checks_are_soft == FALSE) {

		log_debug_info(DEBUGL_CHECKS, 2, "Check type passive and passive host checks aren't false\n");
		
		if (state_change == TRUE) {
            hst->current_attempt = 1;
            hard_state_change = TRUE;
        }
        
		hst->state_type = HARD_STATE;
	}

	/* update event and problem ids */
	if (state_change == TRUE) {

		hst->last_state_change = hst->last_check;

		/* always update the event id on a state change */
		hst->last_event_id = hst->current_event_id;
		hst->current_event_id = next_event_id;
		next_event_id++;

		/* update the problem id when transitioning to a problem state */
		if (hst->last_state == HOST_UP) {
			/* don't reset last problem id, or it will be zero the next time a problem is encountered */
			hst->current_problem_id = next_problem_id;
			next_problem_id++;
		}

		/* clear the problem id when transitioning from a problem state to an OK state */
		else {
			hst->last_problem_id = hst->current_problem_id;
			hst->current_problem_id = 0L;
		}

		hst->state_type = SOFT_STATE;

		state_or_type_change = TRUE;
	}

	if (hard_state_change == TRUE) {

		hst->last_hard_state_change = hst->last_check;
		hst->last_hard_state = hst->current_state;
		hst->state_type = HARD_STATE;

		state_or_type_change = TRUE;

		/* this is in the host func, but not the service
		   because it can easily be missed if a passive check
		   comes in and passive_host_checks_are_soft == FALSE */
		*send_notification = TRUE;
	}

	if (state_or_type_change) {

		/* check if host should go into downtime from flexible downtime */
		check_pending_flex_host_downtime(hst);

		/* reset notification times and suppression option */
		hst->last_notification = (time_t)0;
		hst->next_notification = (time_t)0;
		hst->no_more_notifications = FALSE;

		if ((hst->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL && (state_change == TRUE || hard_state_change == FALSE))
			|| (hst->acknowledgement_type == ACKNOWLEDGEMENT_STICKY && hst->current_state == STATE_OK)) {

			/* remove any non-persistant comments associated with the ack */
			hst->problem_has_been_acknowledged = FALSE;
			hst->acknowledgement_type = ACKNOWLEDGEMENT_NONE;
			delete_host_acknowledgement_comments(hst);
		}

		hst->should_be_scheduled = TRUE;

		*log_event = TRUE;
		*handle_event = TRUE;
	}
}

/******************************************************************************
 ******* Logic for setting default state change times, etc.
 *****************************************************************************/
static inline void initialize_last_service_state_change_times(service * svc, host * hst)
{
	/* initialize the last host and service state change times if necessary */
	if (svc->last_state_change == (time_t)0) {
		svc->last_state_change = svc->last_check;
	}
	if (svc->last_hard_state_change == (time_t)0) {
		svc->last_hard_state_change = svc->last_check;
	}
	if (hst->last_state_change == (time_t)0) {
		hst->last_state_change = svc->last_check;
	}
	if (hst->last_hard_state_change == (time_t)0) {
		hst->last_hard_state_change = svc->last_check;
	}
}
/*****************************************************************************/
static inline void initialize_last_host_state_change_times(host * hst)
{
	/* initialize last host state change times if necessary */
	if (hst->last_state_change == (time_t)0) {
		hst->last_state_change = hst->last_check;
	}
	if (hst->last_hard_state_change == (time_t)0) {
		hst->last_hard_state_change = hst->last_check;
	}
}


/******************************************************************************
 ******* Logic chunks propagating checks to host parents/children
 *****************************************************************************/
static inline void host_propagate_checks_to_immediate_parents(host * hst, int parent_host_up, time_t current_time)
{
	hostsmember *temp_hostsmember = NULL;
	host *parent_host = NULL;

	log_debug_info(DEBUGL_CHECKS, 1, "Propagating checks to parent host(s)...\n");
	for(temp_hostsmember = hst->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
		parent_host = temp_hostsmember->host_ptr;
		if ((parent_host_up == TRUE  && parent_host->current_state == HOST_UP) 
			|| ((parent_host_up == FALSE && parent_host->current_state != HOST_UP))) {

			log_debug_info(DEBUGL_CHECKS, 1, "Check of parent host '%s' queued.\n", parent_host->name);
			schedule_host_check(parent_host, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
			}
		}
}
static inline void host_propagate_checks_to_immediate_children(host * hst, int children_none_up, int children_none_unreachable, time_t current_time)
{
	hostsmember *temp_hostsmember = NULL;
	host *child_host = NULL;

	log_debug_info(DEBUGL_CHECKS, 1, "Propagating checks to child host(s)...\n");
	for(temp_hostsmember = hst->child_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
		child_host = temp_hostsmember->host_ptr;
		if (            (children_none_up == TRUE && child_host->current_state != HOST_UP)
			|| (children_none_unreachable == TRUE && child_host->current_state != HOST_UNREACHABLE)) {

			log_debug_info(DEBUGL_CHECKS, 1, "Check of child host '%s' queued.\n", child_host->name);
			schedule_host_check(child_host, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
		}
	}
}

/******************************************************************************
 ******* Logic chunks propagating dependency checks
 *****************************************************************************/
static inline void service_propagate_dependency_checks(service * svc, time_t current_time)
{
	if (svc->current_attempt == (svc->max_attempts - 1) 
		&& execute_service_checks == TRUE
		&& enable_predictive_service_dependency_checks == TRUE) {

		servicedependency *temp_dependency = NULL;
		service *master_service = NULL;
		objectlist *list;

		log_debug_info(DEBUGL_CHECKS, 1, "Propagating predictive dependency checks to services this one depends on...\n");

		/* check services that THIS ONE depends on for notification AND execution */
		/* we do this because we might be sending out a notification soon and we want the dependency logic to be accurate */
		for(list = svc->exec_deps; list; list = list->next) {
			temp_dependency = (servicedependency *)list->object_ptr;
			if (temp_dependency->dependent_service_ptr == svc && temp_dependency->master_service_ptr != NULL) {

				master_service = (service *)temp_dependency->master_service_ptr;

				log_debug_info(DEBUGL_CHECKS, 2, "Predictive check of service '%s' on host '%s' queued.\n", master_service->description, master_service->host_name);
				schedule_service_check(master_service, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
			}
		}

		for(list = svc->notify_deps; list; list = list->next) {
			temp_dependency = (servicedependency *)list->object_ptr;
			if (temp_dependency->dependent_service_ptr == svc && temp_dependency->master_service_ptr != NULL) {

				master_service = (service *)temp_dependency->master_service_ptr;

				log_debug_info(DEBUGL_CHECKS, 2, "Predictive check of service '%s' on host '%s' queued.\n", master_service->description, master_service->host_name);
				schedule_service_check(master_service, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
			}
		}
	}
}
/*****************************************************************************/
static inline void host_propagate_dependency_checks(host * hst, time_t current_time)
{
	/* we do to help ensure that the dependency checks are accurate before it comes time to notify */
	if (hst->current_attempt == (hst->max_attempts - 1) 
		&& execute_host_checks == TRUE
		&& enable_predictive_host_dependency_checks == TRUE) {

		objectlist *list;
		hostdependency *dep = NULL;
		host *master_host = NULL;
		
		log_debug_info(DEBUGL_CHECKS, 1, "Propagating predictive dependency checks to hosts this one depends on...\n");

		for(list = hst->notify_deps; list; list = list->next) {
			dep = (hostdependency *)list->object_ptr;
			if (dep->dependent_host_ptr == hst && dep->master_host_ptr != NULL) {

				master_host = (host *)dep->master_host_ptr;

				log_debug_info(DEBUGL_CHECKS, 1, "Check of host '%s' queued.\n", master_host->name);
				schedule_host_check(master_host, current_time, CHECK_OPTION_NONE);
			}
		}

		for(list = hst->exec_deps; list; list = list->next) {
			dep = (hostdependency *)list->object_ptr;
			if (dep->dependent_host_ptr == hst && dep->master_host_ptr != NULL) {

				master_host = (host *)dep->master_host_ptr;

				log_debug_info(DEBUGL_CHECKS, 1, "Check of host '%s' queued.\n", master_host->name);
				schedule_host_check(master_host, current_time, CHECK_OPTION_NONE);
			}
		}
	}
}

/******************************************************************************
 ******* One stop shop for determining if check_result data is valid
 *****************************************************************************/
static inline int is_valid_check_result_data(host * hst, check_result * cr)
{
	if (hst == NULL) {
		log_debug_info(DEBUGL_CHECKS, 2, "No host associated with service, bailing!\n");
		return FALSE;
	}

	if (cr == NULL) {
		log_debug_info(DEBUGL_CHECKS, 2, "No check result specified, bailing!\n");
		return FALSE;
	}

	return TRUE;
}
/******************************************************************************
 **********  Fin.  ************************************************************
 *****************************************************************************/

/* handles asynchronous service check results */
int handle_async_service_check_result(service *svc, check_result *cr)
{
	time_t current_time            = 0L;
	time_t next_check              = 0L;
	time_t preferred_time          = 0L;
	time_t next_valid_time         = 0L;

	int state_change               = FALSE;
	int hard_state_change          = FALSE;
	int send_notification          = FALSE;
	int handle_event               = FALSE;
	int log_event                  = FALSE;
	int check_host                 = FALSE;
	int update_host_stats          = FALSE;

	char * old_plugin_output       = NULL;

	host * hst                     = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "handle_async_service_check_result()\n");

	if (svc == NULL) {
		log_debug_info(DEBUGL_CHECKS, 2, "No service specified, bailing!\n");
		return ERROR;
	}
	hst = svc->host_ptr;
	if (is_valid_check_result_data(hst, cr) == FALSE) {
		return ERROR;
	}

	int new_last_hard_state	       = svc->last_hard_state;

	if (cr->check_type == CHECK_TYPE_PASSIVE) {
		if (service_is_passive(svc, cr) == FALSE) {
			return ERROR;
		}
	}
	else {
		service_is_active(svc);
	}

	time(&current_time);
	initialize_last_service_state_change_times(svc, hst);

	debug_async_service(svc, cr);
	service_fresh_check(svc, cr, current_time);
	service_initial_handling(svc, cr, &old_plugin_output);

	/* reschedule the next check at the regular interval - may be overridden */
	next_check = (time_t)(svc->last_check + (svc->check_interval * interval_length));

	/***********************************************/
	/********** SCHEDULE SERVICE CHECK LOGIC **********/
	/***********************************************/
	if (svc->current_state == STATE_OK) {

		log_debug_info(DEBUGL_CHECKS, 1, "Service is OK\n");

		if (hst->has_been_checked == FALSE) {

			log_debug_info(DEBUGL_CHECKS, 1, "Host has not been checked yet\n");

			if (hst->next_check == 0L 
				|| hst->initial_state != HOST_UP 
				|| hst->next_check < hst->check_interval * interval_length + current_time) {

				log_debug_info(DEBUGL_CHECKS, 2, "Service ok, but host hasn't been checked recently, scheduling host check\n");

				check_host = TRUE;
			}
		}

		else if (hst->current_state != HOST_UP) {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is NOT UP, so we'll check it to see if it recovered...\n");
				
			if (svc->last_state == STATE_OK 
				&& hst->has_been_checked == TRUE 
				&& current_time - hst->last_check < cached_host_check_horizon) {

				log_debug_info(DEBUGL_CHECKS, 2, "Service ok, but host isn't up (and has been checked). Using cached host data.\n");

				update_host_stats = TRUE;

			} else {

				log_debug_info(DEBUGL_CHECKS, 2, "Service ok, but host isn't up and cached data isn't valid here, scheduling host check\n");

				check_host = TRUE;
			}

			svc->host_problem_at_last_check = TRUE;
		}
        
	}
	else {

		log_debug_info(DEBUGL_CHECKS, 1, "Service is in a non-OK state!\n");

		if (hst->current_state == HOST_UP) {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is currently UP, so we'll recheck its state to make sure...\n");

			if (execute_host_checks == TRUE
				&& svc->last_state != svc->current_state
				&& hst->last_check + cached_host_check_horizon < current_time) {

				log_debug_info(DEBUGL_CHECKS, 2, "Service not ok, host is up but cached data isn't valid, scheduling host check\n");

				check_host = TRUE;

			} else {

				log_debug_info(DEBUGL_CHECKS, 2, "Service not ok, host is up, using cached host data\n");

				update_host_stats = TRUE;
			}

			/* give the service a chance to recover */
			if (svc->host_problem_at_last_check == TRUE
				&& svc->state_type == SOFT_STATE) {

				log_debug_info(DEBUGL_CHECKS, 2, "Service had a host problem at last check and is SOFT, so we'll reset current_attempt to 1 to give it a chance\n");

				svc->current_attempt = 1;
			}

			svc->host_problem_at_last_check = FALSE;
		}
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is currently not UP...\n");

			if (execute_host_checks == FALSE || svc->current_state == svc->last_state) {

				log_debug_info(DEBUGL_CHECKS, 2, "Host checks aren't enabled, so send a notification\n");

				/* fake a host check */
				if (hst->has_been_checked == FALSE) {

					log_debug_info(DEBUGL_CHECKS, 2, "Host has never been checked, fake a host check\n");

					hst->has_been_checked = TRUE;
					hst->last_check = svc->last_check;
				}

				/* possibly re-send host notifications... */
				host_notification(hst, NOTIFICATION_NORMAL, NULL, NULL, NOTIFICATION_OPTION_NONE);
			}

			svc->host_problem_at_last_check = TRUE;
		}
	}

	/* service hard state change, because if host is down/unreachable
	   the docs say we have a hard state change (but no notification) */
	if (hst->current_state != HOST_UP && new_last_hard_state != svc->current_state) {

		log_debug_info(DEBUGL_CHECKS, 2, "Host is down or unreachable, forcing service hard state change\n");

		hard_state_change = TRUE;
		svc->state_type = HARD_STATE;
		new_last_hard_state = svc->current_state;
		svc->current_attempt = svc->max_attempts;
    }

	if (check_host == TRUE) {
		schedule_host_check(hst, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
	}

	if (update_host_stats == TRUE) {
		update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, current_time);
		update_check_stats(ACTIVE_CACHED_HOST_CHECK_STATS, current_time);
	}

	/**************************************/
	/******* SERVICE CHECK OK LOGIC *******/
	/**************************************/
	if (svc->last_state == STATE_OK) {

		log_debug_info(DEBUGL_CHECKS, 1, "Service was OK at last check.\n");

		/***** SERVICE IS STILL OK *****/
		if (svc->current_state == STATE_OK) {

			log_debug_info(DEBUGL_CHECKS, 1, "Service is still OK.\n");

			svc->state_type = HARD_STATE;
			svc->current_attempt = 1;
		}

		/***** SERVICE IS NOW IN PROBLEM STATE *****/
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Service is a non-OK state (%s)!\n", service_state_name(svc->current_state));

			/* skip this service check if host is down/unreachable and state change happened */
			if (svc->host_problem_at_last_check == FALSE && hard_state_change == FALSE) {
				svc->state_type = SOFT_STATE;
			}

			svc->current_attempt = 1;

			handle_event = TRUE;
		}
	}

	/*******************************************/
	/******* SERVICE CHECK PROBLEM LOGIC *******/
	/*******************************************/
	else {

		log_debug_info(DEBUGL_CHECKS, 1, "Service was NOT OK at last check (%s).\n", service_state_name(svc->last_state));

		/***** SERVICE IS NOW OK *****/
		if (svc->current_state == STATE_OK) {

			handle_event = TRUE;

			if (svc->state_type == HARD_STATE) {

				log_debug_info(DEBUGL_CHECKS, 1, "Service experienced a HARD recovery.\n");

				send_notification = TRUE;
				hard_state_change = TRUE;

				svc->current_attempt = 1;
			}
			else {

				log_debug_info(DEBUGL_CHECKS, 1, "Service experienced a SOFT recovery.\n");				
			}

            /* there was a state change, soft or hard */
            state_change = TRUE;
		}

		/***** SERVICE IS STILL IN PROBLEM STATE *****/
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Service is still in a non-OK state (%s)!\n", service_state_name(svc->current_state));

			if (svc->state_type == SOFT_STATE) {

				log_debug_info(DEBUGL_CHECKS, 2, "Service state type is soft, using retry_interval\n");

				handle_event = TRUE;
				next_check = (unsigned long) (current_time + svc->retry_interval * interval_length);
			}

			/* check if host is down/unreachable and don't send notifications */
			else if (svc->host_problem_at_last_check == TRUE) {

				log_debug_info(DEBUGL_CHECKS, 2, "Service state type is hard, but host is down or unreachable, not sending notification\n");

			} else {

				log_debug_info(DEBUGL_CHECKS, 2, "Service state type is hard, sending a notification\n");

				send_notification = TRUE;
			}
		}
	}
    
    /* soft states should be using retry_interval */
    if (svc->state_type == SOFT_STATE) {
        
		log_debug_info(DEBUGL_CHECKS, 2, "Service state type is soft, using retry_interval\n");

		next_check = (unsigned long) (current_time + svc->retry_interval * interval_length);
    }

	/* check for a state change */
	if (svc->current_state != svc->last_state || (svc->current_state == STATE_OK && svc->state_type == SOFT_STATE)) {

		log_debug_info(DEBUGL_CHECKS, 2, "Service experienced a state change\n");

		state_change = TRUE;
	}

	/* adjust the current attempt */
	if (svc->state_type == SOFT_STATE) {

		/* this has to be first so we don't reset every time a new non-ok state comes
		   in (and triggers the state_change == TRUE) */
		if (svc->last_state != STATE_OK && svc->current_attempt < svc->max_attempts) {

			svc->current_attempt++;
		}

		/* historically, a soft recovery would actually get up to 2 attempts
		   and then immediately reset once the next check result came in */
		else if (state_change == TRUE && svc->current_state != STATE_OK) {

			svc->current_attempt = 1;
		}

		/* otherwise, just increase the attempt */
	 	else if (svc->current_attempt < svc->max_attempts) {

	 		svc->current_attempt++;
	 	}
	}

	if (svc->current_attempt >= svc->max_attempts &&
		(svc->current_state != new_last_hard_state || (svc->state_type == SOFT_STATE && svc->current_state != STATE_OK))) {

		log_debug_info(DEBUGL_CHECKS, 2, "Service had a HARD STATE CHANGE!!\n");
        
        next_check = (unsigned long)(current_time + (svc->check_interval * interval_length));

        /* set both states changed, this may have been missed... */
		hard_state_change = TRUE;

		/* this is missed earlier */
		send_notification = TRUE;
	}

	/* handle some acknowledgement things and update last_state_change */
	/* This is a temporary fix that lets us avoid changing any function boundaries in a bugfix release */
	/* @fixme 4.5.0 - refactor so that each specific struct member is only modified in */
	/* service_state_or_hard_state_type_change() or handle_async_service_check_result(), not both.*/
	int original_last_hard_state = svc->last_hard_state;
	service_state_or_hard_state_type_change(svc, state_change, hard_state_change, &log_event, &handle_event);
	if (original_last_hard_state != svc->last_hard_state) {

		/* svc->last_hard_state now gets written only after the service status is brokered */
		new_last_hard_state = svc->last_hard_state;
		svc->last_hard_state = original_last_hard_state;
	}

	/* fix edge cases where log_event wouldn't have been set or won't be */
	if (svc->current_state != STATE_OK && svc->state_type == SOFT_STATE) {
		log_event = TRUE;
	}

	record_last_service_state_ended(svc);

	check_for_service_flapping(svc, TRUE, TRUE);
	check_for_host_flapping(hst, TRUE, FALSE, TRUE);

	/* service with active checks disabled do not get rescheduled */
	if (svc->checks_enabled == FALSE) {
		svc->should_be_scheduled = FALSE;
	}

	/* hosts with non-recurring intervals do not get rescheduled if we're in a HARD or OK state */
	else if (svc->check_interval == 0 && (svc->state_type == HARD_STATE || svc->current_state == STATE_OK)) {
		svc->should_be_scheduled = FALSE;
	}

	/* schedule a non-forced check if we can */
	else if (svc->should_be_scheduled == TRUE) {

		log_debug_info(DEBUGL_CHECKS, 1, "Rescheduling next check of service at %s", ctime(&next_check));

		/* next check time was calculated above */
		/* make sure we don't get ourselves into too much trouble... */
		if (current_time > next_check) {
			svc->next_check = current_time;
		} else {
			svc->next_check = next_check;   
		}

		/* make sure we rescheduled the next service check at a valid time */
		preferred_time = svc->next_check;
		get_next_valid_time(preferred_time, &next_valid_time, svc->check_period_ptr);
		svc->next_check = next_valid_time;

		/* Next valid time is further in the future because of timeperiod
		   constraints. Add a random amount so we don't get all checks
		   subject to that timeperiod constraint scheduled at the same time */
		if (next_valid_time > preferred_time) {
			svc->next_check = reschedule_within_timeperiod(next_valid_time, svc->check_period_ptr, check_window(svc));
		}

		schedule_service_check(svc, svc->next_check, CHECK_OPTION_NONE);
	}

	if (svc->current_state == STATE_OK && state_change == TRUE) {

		/* Reset problem ID. It's important to have this happen before
		 * notification-sending logic */
		svc->last_problem_id = svc->current_problem_id;
		svc->current_problem_id = 0L;
	}

	/* volatile service gets everything in non-ok hard state */
	if ((svc->current_state != STATE_OK) 
		&& (svc->state_type == HARD_STATE) 
		&& (svc->is_volatile == TRUE)) {

		log_debug_info(DEBUGL_CHECKS, 2, "Service is volatile, and we're in a non-ok hard state..\n");

		send_notification = TRUE;
		log_event = TRUE;
		handle_event = TRUE;
	}

	if (send_notification == TRUE) {

		/* send notification */
		if (service_notification(svc, NOTIFICATION_NORMAL, NULL, NULL, NOTIFICATION_OPTION_NONE) == OK) {

			/* log state due to notification event when stalking_options N is set */
			if (should_stalk_notifications(svc)) {
				log_event = TRUE;
			}
		}
	}

	/* the service recovered, so reset the current notification number and state flags (after the recovery notification has gone out) */
	if(svc->current_state == STATE_OK && svc->state_type == HARD_STATE && hard_state_change == TRUE) {
		svc->current_notification_number = 0;
		svc->notified_on = 0;
		}

	if (obsess_over_services == TRUE) {
		obsessive_compulsive_service_check_processor(svc);
	}

	/* if we're stalking this state type AND the plugin output changed since last check, log it now.. */
	if (should_stalk(svc) && compare_strings(old_plugin_output, svc->plugin_output)) {

		log_debug_info(DEBUGL_CHECKS, 2, "Logging due to state stalking, old: [%s], new: [%s]\n", old_plugin_output, svc->plugin_output);
		log_event = TRUE;
	}

	if (log_event == TRUE) {
		log_service_event(svc);
	}

	if (handle_event == TRUE) {
		handle_service_event(svc);
	}

	/* Update OK states since they send out a soft alert but then they
	   switch into a HARD state and reset the attempts */
	if (svc->current_state == STATE_OK && state_change == TRUE) {

		/* Reset attempts */
		if (hard_state_change == TRUE) {
			svc->current_notification_number = 0;
			svc->host_problem_at_last_check = FALSE;
		}

		svc->last_hard_state_change = svc->last_check;
		new_last_hard_state = svc->current_state;

		/* Set OK to a hard state */
		svc->current_attempt = 1;
		svc->state_type = HARD_STATE;
	}

	log_debug_info(DEBUGL_CHECKS, 2, 
		"STATE: %d, TYPE: %s, CUR: %d, MAX: %d, LAST_STATE: %d, LAST_HARD: %d, NOTIFY: %d, LOGGED: %d, HANDLED: %d\n",
		svc->current_state, 
		(svc->state_type == SOFT_STATE) ? "SOFT" : "HARD", 
		svc->current_attempt, 
		svc->max_attempts, 
		svc->last_state, 
		svc->last_hard_state,
		send_notification,
		log_event,
		handle_event);

#ifdef USE_EVENT_BROKER
	broker_service_check(NEBTYPE_SERVICECHECK_PROCESSED, NEBFLAG_NONE, NEBATTR_NONE, svc, svc->check_type, cr->start_time, cr->finish_time, NULL, svc->latency, svc->execution_time, service_check_timeout, cr->early_timeout, cr->return_code, NULL, NULL, cr);
#endif


	svc->has_been_checked = TRUE;
	update_service_status(svc, FALSE);
	update_service_performance_data(svc);

	/* last_hard_state cleanup
	 * This occurs after being brokered so that last_hard_state refers to the previous logged hard state, 
	 * rather than the current hard state 
	 */
	svc->last_hard_state = new_last_hard_state;

	my_free(old_plugin_output);

	return OK;
}

/* schedules an immediate or delayed service check */
inline void schedule_service_check(service *svc, time_t check_time, int options)
{
	timed_event *temp_event = NULL;
	int use_original_event = TRUE;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "schedule_service_check()\n");

	if (svc == NULL)
		return;

	log_debug_info(DEBUGL_CHECKS, 0, "Scheduling a %s, active check of service '%s' on host '%s' @ %s", (options & CHECK_OPTION_FORCE_EXECUTION) ? "forced" : "non-forced", svc->description, svc->host_name, ctime(&check_time));

	/* don't schedule a check if active checks of this service are disabled */
	if (svc->checks_enabled == FALSE && !(options & CHECK_OPTION_FORCE_EXECUTION)) {
		log_debug_info(DEBUGL_CHECKS, 0, "Active checks of this service are disabled.\n");
		return;
	}

	/* we may have to nudge this check a bit */
	if (options == CHECK_OPTION_DEPENDENCY_CHECK) {
		if (svc->last_check + cached_service_check_horizon > check_time) {
			log_debug_info(DEBUGL_CHECKS, 0, "Last check result is recent enough (%s)", ctime(&svc->last_check));
			return;
		}
	}

	/* default is to use the new event */
	use_original_event = FALSE;

	temp_event = (timed_event *)svc->next_check_event;

	/*
	 * If the service already has a check scheduled,
	 * we need to decide which of the events to use
	 */
	if (temp_event != NULL) {

		log_debug_info(DEBUGL_CHECKS, 2, "Found another service check event for this service @ %s", ctime(&temp_event->run_time));

		/* use the originally scheduled check unless we decide otherwise */
		use_original_event = TRUE;

		/* the original event is a forced check... */
		if ((temp_event->event_options & CHECK_OPTION_FORCE_EXECUTION)) {

			/* the new event is also forced and its execution time is earlier than the original, so use it instead */
			if ((options & CHECK_OPTION_FORCE_EXECUTION) && (check_time < temp_event->run_time)) {
				use_original_event = FALSE;
				log_debug_info(DEBUGL_CHECKS, 2, "New service check event is forced and occurs before the existing event, so the new event will be used instead.\n");
			}
		}

		/* the original event is not a forced check... */
		else {

			/* the new event is a forced check, so use it instead */
			if ((options & CHECK_OPTION_FORCE_EXECUTION)) {
				use_original_event = FALSE;
				log_debug_info(DEBUGL_CHECKS, 2, "New service check event is forced, so it will be used instead of the existing event.\n");
			}

			/* the new event is not forced either and its execution time is earlier than the original, so use it instead */
			else if (check_time < temp_event->run_time) {
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
	if (use_original_event == FALSE) {
		/* make sure we remove the old event from the queue */
		if (temp_event) {
			remove_event(nagios_squeue, temp_event);
		}
		else {
			/* allocate memory for a new event item */
			temp_event = (timed_event *)calloc(1, sizeof(timed_event));
			if (temp_event == NULL) {
				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Could not reschedule check of service '%s' on host '%s'!\n", svc->description, svc->host_name);
				return;
			}
		}

		log_debug_info(DEBUGL_CHECKS, 2, "Scheduling new service check event.\n");

		/* set the next service check event and time */
		svc->next_check_event = temp_event;
		svc->next_check = check_time;

		/* save check options for retention purposes */
		svc->check_options = options;

		/* place the new event in the event queue */
		temp_event->event_type = EVENT_SERVICE_CHECK;
		temp_event->event_data = (void *)svc;
		temp_event->event_args = (void *)NULL;
		temp_event->event_options = options;
		temp_event->run_time = svc->next_check;
		temp_event->recurring = FALSE;
		temp_event->event_interval = 0L;
		temp_event->timing_func = NULL;
		temp_event->compensate_for_time_change = TRUE;
		add_event(nagios_squeue, temp_event);
	}

	else {
		/* reset the next check time (it may be out of sync) */
		if (temp_event != NULL) {
			svc->next_check = temp_event->run_time;
		}

		log_debug_info(DEBUGL_CHECKS, 2, "Keeping original service check event (ignoring the new one).\n");
	}


	/* update the status log */
	update_service_status(svc, FALSE);
}



/* checks viability of performing a service check */
inline int check_service_check_viability(service *svc, int check_options, int *time_is_valid, time_t *new_time)
{
	int perform_check = TRUE;
	time_t current_time = 0L;
	time_t preferred_time = 0L;
	int check_interval = 0;
	host *temp_host = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_service_check_viability()\n");

	/* make sure we have a service */
	if (svc == NULL) {
		return ERROR;
	}

	/* get the check interval to use if we need to reschedule the check */
	if (svc->state_type == SOFT_STATE && svc->current_state != STATE_OK) {
		check_interval = (svc->retry_interval * interval_length);
	}
	else {
		check_interval = (svc->check_interval * interval_length);
	}

	/* get the current time */
	time(&current_time);

	/* initialize the next preferred check time */
	preferred_time = current_time;

	/* can we check the host right now? */
	if (!(check_options & CHECK_OPTION_FORCE_EXECUTION)) {

		/* if checks of the service are currently disabled... */
		if (svc->checks_enabled == FALSE) {
			preferred_time = current_time + check_interval;
			perform_check = FALSE;

			log_debug_info(DEBUGL_CHECKS, 2, "Active checks of the service are currently disabled.\n");
		}

		/* make sure this is a valid time to check the service */
		if (check_time_against_period((unsigned long)current_time, svc->check_period_ptr) == ERROR) {
			preferred_time = current_time;
			if (time_is_valid) {
				*time_is_valid = FALSE;
			}
			perform_check = FALSE;

			log_debug_info(DEBUGL_CHECKS, 2, "This is not a valid time for this service to be actively checked.\n");
		}

		/* check service dependencies for execution */
		if (check_service_dependencies(svc, EXECUTION_DEPENDENCY) == DEPENDENCIES_FAILED) {
			preferred_time = current_time + check_interval;
			perform_check = FALSE;
			if (service_skip_check_dependency_status >= 0) {
				svc->current_state = service_skip_check_dependency_status;
			}
			log_debug_info(DEBUGL_CHECKS, 2, "Execution dependencies for this service failed, so it will not be actively checked.\n");
		}
	}

	/* check if parent service is OK */
	if (check_service_parents(svc) == DEPENDENCIES_FAILED) {
		preferred_time = current_time + check_interval;
		perform_check = FALSE;
		if (service_skip_check_parent_status >= 0) {
			svc->current_state = service_skip_check_parent_status;
		}
		log_debug_info(DEBUGL_CHECKS, 2, "Execution parents for this service failed, so it will not be actively checked.\n");
	}

	/* check if host is up - if not, do not perform check */
	if (host_down_disable_service_checks) {
		if ((temp_host = svc->host_ptr) == NULL) {
			log_debug_info(DEBUGL_CHECKS, 2, "Host pointer NULL in check_service_check_viability().\n");
			return ERROR;
		}
		else {
			if (temp_host->current_state != HOST_UP) {
				log_debug_info(DEBUGL_CHECKS, 2, "Host state not UP, so service check will not be performed - will be rescheduled as normal.\n");
				perform_check = FALSE;
				if (service_skip_check_host_down_status >= 0) {
					svc->current_state = service_skip_check_host_down_status;
				}
			}
		}
	}

	/* pass back the next viable check time */
	if (new_time) {
		*new_time = preferred_time;
	}

	if (perform_check == TRUE) {
		return OK;
	}
	return ERROR;
}



/* checks service parents */
int check_service_parents(service *svc)
{
	servicesmember *temp_servicesmember = NULL;
	int state = STATE_OK;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_service_parents()\n");

	/* check all parents... */
	for(temp_servicesmember = svc->parents; temp_servicesmember; temp_servicesmember = temp_servicesmember->next) {
		service *parent_service;

		/* find the service we depend on... */
		if ((parent_service = temp_servicesmember->service_ptr) == NULL) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: service '%s' on host '%s' is NULL ptr\n",
				  temp_servicesmember->service_description, temp_servicesmember->host_name);
			continue;
		}

		state = parent_service->last_hard_state;

		/* is the service we depend on in a state that fails the dependency tests? */
		if ((state == STATE_CRITICAL) || (state == STATE_UNKNOWN))
			return DEPENDENCIES_FAILED;

		if (check_service_parents(parent_service) != DEPENDENCIES_OK)
			return DEPENDENCIES_FAILED;
	}

	return DEPENDENCIES_OK;
}



/* checks service dependencies */
int check_service_dependencies(service *svc, int dependency_type)
{
	objectlist *list;
	int state = STATE_OK;
	time_t current_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_service_dependencies()\n");

	/* only check dependencies of the desired type */
	if (dependency_type == NOTIFICATION_DEPENDENCY)
		list = svc->notify_deps;
	else
		list = svc->exec_deps;

	/* check all dependencies of the desired type... */
	for(; list; list = list->next) {
		service *temp_service;
		servicedependency *temp_dependency = (servicedependency *)list->object_ptr;

		/* find the service we depend on... */
		if ((temp_service = temp_dependency->master_service_ptr) == NULL) {
			continue;
		}

		/* skip this dependency if it has a timeperiod and the current time isn't valid */
		time(&current_time);
		if (temp_dependency->dependency_period != NULL 
			&& (check_time_against_period(current_time, temp_dependency->dependency_period_ptr) == ERROR)) {

			return FALSE;
		}

		/* get the status to use (use last hard state if its currently in a soft state) */
		if (temp_service->state_type == SOFT_STATE && soft_state_dependencies == FALSE) {
			state = temp_service->last_hard_state;
		}
		else {
			state = temp_service->current_state;
		}

		/* is the service we depend on in state that fails the dependency tests? */
		if (flag_isset(temp_dependency->failure_options, 1 << state)) {
			return DEPENDENCIES_FAILED;
		}

		/* immediate dependencies ok at this point - check parent dependencies if necessary */
		if (temp_dependency->inherits_parent == TRUE) {
			if (check_service_dependencies(temp_service, dependency_type) != DEPENDENCIES_OK) {
				return DEPENDENCIES_FAILED;
			}
		}
	}

	return DEPENDENCIES_OK;
}



/* check for services that never returned from a check... */
void check_for_orphaned_services(void)
{
	service *temp_service = NULL;
	time_t current_time = 0L;
	time_t expected_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_for_orphaned_services()\n");

	/* get the current time */
	time(&current_time);

	/* check all services... */
	for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

		/* skip services that are not currently executing */
		if (temp_service->is_executing == FALSE) {
			continue;
		}

		/* determine the time at which the check results should have come in (allow 10 minutes slack time) */
		expected_time = (time_t)(temp_service->next_check + temp_service->latency + service_check_timeout + check_reaper_interval + 600);

		/* this service was supposed to have executed a while ago, but for some reason the results haven't come back in... */
		if (expected_time < current_time) {

			/* log a warning */
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: The check of service '%s' on host '%s' looks like it was orphaned (results never came back; last_check=%lu; next_check=%lu).  I'm scheduling an immediate check of the service...\n", temp_service->description, temp_service->host_name, temp_service->last_check, temp_service->next_check);

			log_debug_info(DEBUGL_CHECKS, 1, "Service '%s' on host '%s' was orphaned, so we're scheduling an immediate check...\n", temp_service->description, temp_service->host_name);
			log_debug_info(DEBUGL_CHECKS, 1, "  next_check=%lu (%s); last_check=%lu (%s);\n",
						   temp_service->next_check, ctime(&temp_service->next_check),
						   temp_service->last_check, ctime(&temp_service->last_check));

			/* decrement the number of running service checks */
			if (currently_running_service_checks > 0) {
				currently_running_service_checks--;
			}

			/* disable the executing flag */
			temp_service->is_executing = FALSE;

			/* schedule an immediate check of the service */
			schedule_service_check(temp_service, current_time, CHECK_OPTION_ORPHAN_CHECK);
		}

	}

	return;
}



/* check freshness of service results */
void check_service_result_freshness(void)
{
	service *temp_service = NULL;
	time_t current_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_service_result_freshness()\n");
	log_debug_info(DEBUGL_CHECKS, 1, "Checking the freshness of service check results...\n");

	/* bail out if we're not supposed to be checking freshness */
	if (check_service_freshness == FALSE) {
		log_debug_info(DEBUGL_CHECKS, 1, "Service freshness checking is disabled.\n");
		return;
	}

	/* get the current time */
	time(&current_time);

	/* check all services... */
	for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

		/* skip services we shouldn't be checking for freshness */
		if (temp_service->check_freshness == FALSE) {
			continue;
		}

		/* skip services that are currently executing (problems here will be caught by orphaned service check) */
		if (temp_service->is_executing == TRUE) {
			continue;
		}

		/* skip services that have both active and passive checks disabled */
		if (temp_service->checks_enabled == FALSE && temp_service->accept_passive_checks == FALSE) {
			continue;
		}

		/* skip services that are already being freshened */
		if (temp_service->is_being_freshened == TRUE) {
			continue;
		}

		/* see if the time is right... */
		if (check_time_against_period(current_time, temp_service->check_period_ptr) == ERROR) {
			continue;
		}

		/* EXCEPTION */
		/* don't check freshness of services without regular check intervals if we're using auto-freshness threshold */
		if ((temp_service->check_interval == 0) && (temp_service->freshness_threshold == 0)) {
			continue;
		}

		/* the results for the last check of this service are stale! */
		if (is_service_result_fresh(temp_service, current_time, TRUE) == FALSE) {

			/* set the freshen flag */
			temp_service->is_being_freshened = TRUE;

			/* schedule an immediate forced check of the service */
			schedule_service_check(temp_service, current_time, CHECK_OPTION_FORCE_EXECUTION | CHECK_OPTION_FRESHNESS_CHECK);
		}

	}

	return;
}



/* tests whether or not a service's check results are fresh */
int is_service_result_fresh(service *temp_service, time_t current_time, int log_this)
{
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
	if (temp_service->freshness_threshold == 0) {
		if (temp_service->state_type == HARD_STATE || temp_service->current_state == STATE_OK) {
			freshness_threshold = (temp_service->check_interval * interval_length) + temp_service->latency + additional_freshness_latency;
		}
		else {
			freshness_threshold = (temp_service->retry_interval * interval_length) + temp_service->latency + additional_freshness_latency;
		}
	}
	else {
		freshness_threshold = temp_service->freshness_threshold;
	}

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
	if (temp_service->has_been_checked == FALSE) {
		expiration_time = (time_t)(event_start + freshness_threshold);
	}
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
	else if ((temp_service->checks_enabled == TRUE) 
		&& (event_start > temp_service->last_check) 
		&& (temp_service->freshness_threshold == 0)) {

		expiration_time = (time_t)(event_start + freshness_threshold + (max_service_check_spread * interval_length));
	}
	else {
		expiration_time = (time_t)(temp_service->last_check + freshness_threshold);
	}

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
	if ((temp_service->check_type == CHECK_TYPE_PASSIVE)
		&& (temp_service->last_check < event_start)
		&& (event_start - last_program_stop > freshness_threshold * 0.618)) {

		expiration_time = event_start + freshness_threshold;
	}
	log_debug_info(DEBUGL_CHECKS, 2, "HBC: %d, PS: %lu, ES: %lu, LC: %lu, CT: %lu, ET: %lu\n", temp_service->has_been_checked, (unsigned long)program_start, (unsigned long)event_start, (unsigned long)temp_service->last_check, (unsigned long)current_time, (unsigned long)expiration_time);

	/* the results for the last check of this service are stale */
	if (expiration_time < current_time) {

		get_time_breakdown((current_time - expiration_time), &days, &hours, &minutes, &seconds);
		get_time_breakdown(freshness_threshold, &tdays, &thours, &tminutes, &tseconds);

		/* log a warning */
		if (log_this == TRUE) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: The results of service '%s' on host '%s' are stale by %dd %dh %dm %ds (threshold=%dd %dh %dm %ds).  I'm forcing an immediate check of the service.\n", temp_service->description, temp_service->host_name, days, hours, minutes, seconds, tdays, thours, tminutes, tseconds);
		}

		log_debug_info(DEBUGL_CHECKS, 1, "Check results for service '%s' on host '%s' are stale by %dd %dh %dm %ds (threshold=%dd %dh %dm %ds).  Forcing an immediate check of the service...\n", temp_service->description, temp_service->host_name, days, hours, minutes, seconds, tdays, thours, tminutes, tseconds);

		return FALSE;
	}

	log_debug_info(DEBUGL_CHECKS, 1, "Check results for service '%s' on host '%s' are fresh.\n", temp_service->description, temp_service->host_name);

	return TRUE;
}


int handle_async_host_check_result(host *hst, check_result *cr)
{
	time_t current_time      = 0L;
	time_t next_check        = 0L;
	time_t preferred_time    = 0L;
	time_t next_valid_time   = 0L;

	int state_change         = FALSE;
	int hard_state_change    = FALSE;
	int send_notification    = FALSE;
	int handle_event         = FALSE;
	int log_event            = FALSE;

	char * old_plugin_output = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "handle_async_host_check_result()\n");

	if (is_valid_check_result_data(hst, cr) == FALSE) {
		return ERROR;
	}

	int new_last_hard_state	 = hst->last_hard_state;

	if (cr->check_type == CHECK_TYPE_PASSIVE) {
		if (host_is_passive(hst, cr) == FALSE) {
			return ERROR;
		}
	}
	else  {
		host_is_active(hst);
	}
	time(&current_time);
	initialize_last_host_state_change_times(hst);

	debug_async_host(hst, cr);
	host_fresh_check(hst, cr, current_time);
	host_initial_handling(hst, cr, &old_plugin_output);

	/* reschedule the next check at the regular interval - may be overridden */
	next_check = (time_t)(hst->last_check + (hst->check_interval * interval_length));

	/**************************************/
	/********* HOST CHECK OK LOGIC ********/
	/**************************************/
	if (hst->last_state == HOST_UP) {

		log_debug_info(DEBUGL_CHECKS, 1, "Host was UP.\n");

		/***** HOST IS STILL UP *****/
		if (hst->current_state == HOST_UP) {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is still UP.\n");

			hst->state_type = HARD_STATE;
			hst->current_attempt = 1;
		}

		/***** HOST IS NOW DOWN/UNREACHABLE *****/
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is no longer UP (%s)!\n", host_state_name(hst->current_state));
            
            hst->state_type = SOFT_STATE;
            hst->current_attempt = 1;

			/* propagate checks to immediate parents if they are UP */
			host_propagate_checks_to_immediate_parents(hst, FALSE, current_time);

			/* propagate checks to immediate children if they are not UNREACHABLE */
			host_propagate_checks_to_immediate_children(hst, FALSE, TRUE, current_time);

			/* propagate checks to hosts that THIS ONE depends on for notifications AND execution */
			host_propagate_dependency_checks(hst, current_time);

			/* we need to handle this event */
			handle_event = TRUE;
		}
	}

	/**************************************/
	/****** HOST CHECK PROBLEM LOGIC ******/
	/**************************************/
	else {

		log_debug_info(DEBUGL_CHECKS, 1, "Host was not UP last time.\n");

		/***** HOST IS NOW UP *****/
		if (hst->current_state == HOST_UP) {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is UP now.\n");

			/* propagate checks to immediate parents if they are not UP */
			host_propagate_checks_to_immediate_parents(hst, TRUE, current_time);

			/* propagate checks to immediate children if they are not UP */
			host_propagate_checks_to_immediate_children(hst, TRUE, FALSE, current_time);

			/* we need to handle this event */
			handle_event = TRUE;

			/* but a soft recovery is not something we notify for */
			if (hst->state_type == HARD_STATE) {

				log_debug_info(DEBUGL_CHECKS, 1, "Host experienced a HARD recovery.\n");

				send_notification = TRUE;
				hard_state_change = TRUE;
				
				hst->current_attempt = 1;
			}
			else {

				log_debug_info(DEBUGL_CHECKS, 1, "Host experienced a SOFT recovery.\n");
			}
		}

		/***** HOST IS STILL DOWN/UNREACHABLE *****/
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is still not UP (%s)!\n", host_state_name(hst->current_state));

			if (hst->state_type == SOFT_STATE) {

				log_debug_info(DEBUGL_CHECKS, 2, "Host state type is soft, using retry_interval\n");

				handle_event = TRUE;
				next_check = (unsigned long) (current_time + hst->retry_interval * interval_length);
			}

			/* if the state_type is hard, then send a notification */
			else {

				log_debug_info(DEBUGL_CHECKS, 2, "Host state type is hard, sending a notification\n");

				send_notification = TRUE;
			}
		}
	}

	/* translate host state between DOWN/UNREACHABLE (only for passive checks if enabled) */
	if (hst->current_state != HOST_UP && (hst->check_type == CHECK_TYPE_ACTIVE || translate_passive_host_checks == TRUE)) {

		hst->current_state = determine_host_reachability(hst);
		if (hst->state_type == SOFT_STATE)
            next_check = (unsigned long)(current_time + (hst->retry_interval * interval_length));
        
	}

	/* check for state change */
	if (hst->current_state != hst->last_state || (hst->current_state == HOST_UP && hst->state_type == SOFT_STATE)) {

		log_debug_info(DEBUGL_CHECKS, 2, "Host experienced a state change\n");

		state_change = TRUE;
	}

	/* adjust the current attempt */
	if (hst->state_type == SOFT_STATE) {

		/* this is an edge case for non-up states, it needs to be checked first */
		if (hst->last_state != HOST_UP && hst->current_state != HOST_UP && hst->current_attempt < hst->max_attempts) {
			hst->current_attempt++;
		}

		/* reset it to 1 */
		else if (state_change == TRUE || hst->current_state == HOST_UP) {
			hst->current_attempt = 1;
		}

		/* or increment if we can */
		else if (hst->current_attempt < hst->max_attempts) {
			hst->current_attempt++;
		}
	}

	if (hst->current_attempt >= hst->max_attempts && hst->current_state != new_last_hard_state) {

		log_debug_info(DEBUGL_CHECKS, 2, "Host had a HARD STATE CHANGE!!\n");

		next_check = (unsigned long)(current_time + (hst->check_interval * interval_length));
        
        hard_state_change = TRUE;
		send_notification = TRUE;
	}

	/* handle some acknowledgement things and update last_state_change */
	/* @fixme 4.5.0 - See similar comment in handle_async_service_check_result() */
	int original_last_hard_state = hst->last_hard_state;
	host_state_or_hard_state_type_change(hst, state_change, hard_state_change, &log_event, &handle_event, &send_notification);
	if (original_last_hard_state != hst->last_hard_state) {

		/* svc->last_hard_state now gets written only after the service status is brokered */
		new_last_hard_state = hst->last_hard_state;
		hst->last_hard_state = original_last_hard_state;
	}

	record_last_host_state_ended(hst);

	check_for_host_flapping(hst, TRUE, TRUE, TRUE);

	/* host with active checks disabled do not get rescheduled */
	if (hst->checks_enabled == FALSE) {
		hst->should_be_scheduled = FALSE;
	}

	/* hosts with non-recurring intervals do not get rescheduled if we're in a HARD or UP state */
	else if (hst->check_interval == 0 && (hst->state_type == HARD_STATE || hst->current_state == HOST_UP)) {
		hst->should_be_scheduled = FALSE;
	}

	/* schedule a non-forced check if we can */
	else if (hst->should_be_scheduled == TRUE) {

		log_debug_info(DEBUGL_CHECKS, 1, "Rescheduling next check of host at %s", ctime(&next_check));

		/* next check time was calculated above */
		/* make sure we don't get ourselves into too much trouble... */
		if (current_time > next_check) {
			hst->next_check = current_time;
		} else {
			hst->next_check = next_check;   
		}

		/* make sure we rescheduled the next service check at a valid time */
		preferred_time = hst->next_check;
		get_next_valid_time(preferred_time, &next_valid_time, hst->check_period_ptr);
		hst->next_check = next_valid_time;

		/* Next valid time is further in the future because of timeperiod
		   constraints. Add a random amount so we don't get all checks
		   subject to that timeperiod constraint scheduled at the same time */
		if (next_valid_time > preferred_time) {
			hst->next_check = reschedule_within_timeperiod(next_valid_time, hst->check_period_ptr, check_window(hst));
		}

		schedule_host_check(hst, hst->next_check, CHECK_OPTION_NONE);
	}

	if (hst->current_attempt == HOST_UP) {
		hst->current_attempt = 1;
	}

	if (send_notification == TRUE) {

		/* send notifications */
		if (host_notification(hst, NOTIFICATION_NORMAL, NULL, NULL, NOTIFICATION_OPTION_NONE) == OK) {

			/* log state due to notification event when stalking_options N is set */
			if (should_stalk_notifications(hst)) {
				log_event = TRUE;
			}
		}
	}

    /* the host recovered, so reset the current notification number and state flags (after the recovery notification has gone out) */
    if(hst->current_state == HOST_UP && hst->state_type == HARD_STATE && hard_state_change == TRUE) {
        hst->current_notification_number = 0;
        hst->notified_on = 0;
        }
        
	if (obsess_over_hosts == TRUE) {
		obsessive_compulsive_host_check_processor(hst);
	}
	
	/* if we're stalking this state type AND the plugin output changed since last check, log it now.. */
	if (should_stalk(hst) && compare_strings(old_plugin_output, hst->plugin_output)) {
		log_event = TRUE;
	}

	/* if log_host_retries is set to true, we have to log soft states too */
	if (hst->state_type == SOFT_STATE && log_host_retries == TRUE) {
		log_event = TRUE;
	}

	if (log_event == TRUE) {
		log_host_event(hst);
	}

	if (handle_event == TRUE) {
		handle_host_event(hst);
	}

	log_debug_info(DEBUGL_CHECKS, 2, 
		"STATE: %d, TYPE: %s, CUR: %d, MAX: %d, LAST_STATE: %d, LAST_HARD: %d, NOTIFY: %d, LOGGED: %d, HANDLED: %d\n",
		hst->current_state, 
		(hst->state_type == SOFT_STATE) ? "SOFT" : "HARD", 
		hst->current_attempt, 
		hst->max_attempts, 
		hst->last_state, 
		hst->last_hard_state,
		send_notification,
		log_event,
		handle_event);

#ifdef USE_EVENT_BROKER
	broker_host_check(NEBTYPE_HOSTCHECK_PROCESSED, NEBFLAG_NONE, NEBATTR_NONE, hst, hst->check_type, hst->current_state, hst->state_type, cr->start_time, cr->finish_time, hst->check_command, hst->latency, hst->execution_time, host_check_timeout, cr->early_timeout, cr->return_code, NULL, hst->plugin_output, hst->long_plugin_output, hst->perf_data, NULL, cr);
#endif

	hst->has_been_checked = TRUE;
	update_host_status(hst, FALSE);
	update_host_performance_data(hst);

	/* last_hard_state cleanup
	 * This occurs after being brokered so that last_hard_state refers to the previous logged hard state, 
	 * rather than the current hard state 
	 */
	hst->last_hard_state = new_last_hard_state;

	/* free memory */
	my_free(old_plugin_output);

	return OK;
}

/******************************************************************/
/*************** COMMON ROUTE/HOST CHECK FUNCTIONS ****************/
/******************************************************************/

/* schedules an immediate or delayed host check */
inline void schedule_host_check(host *hst, time_t check_time, int options)
{
	timed_event *temp_event = NULL;

	/* use the originally scheduled check unless we decide otherwise */
	int use_original_event = TRUE;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "schedule_host_check()\n");


	if (hst == NULL) {
		return;
	}

	log_debug_info(DEBUGL_CHECKS, 0, "Scheduling a %s, active check of host '%s' @ %s", 
		(options & CHECK_OPTION_FORCE_EXECUTION) ? "forced" : "non-forced", 
		hst->name, 
		ctime(&check_time));

	/* don't schedule a check if active checks of this host are disabled */
	if (hst->checks_enabled == FALSE && !(options & CHECK_OPTION_FORCE_EXECUTION)) {
		log_debug_info(DEBUGL_CHECKS, 0, "Active checks are disabled for this host.\n");
		return;
	}

	if ((options == CHECK_OPTION_DEPENDENCY_CHECK)
		&& (hst->last_check + cached_host_check_horizon > check_time)) {

		log_debug_info(DEBUGL_CHECKS, 0, "Last check result is recent enough (%s)\n", ctime(&hst->last_check));
		return;
	}

	temp_event = (timed_event *)hst->next_check_event;

	if (temp_event == NULL) {
		use_original_event = FALSE;
	}

	/*
	 * If the host already had a check scheduled we need
	 * to decide which check event to use
	 */
	else {

		log_debug_info(DEBUGL_CHECKS, 2, "Found another host check event for this host @ %s", ctime(&temp_event->run_time));

		/* the original event is a forced check... */
		if ((temp_event->event_options & CHECK_OPTION_FORCE_EXECUTION)) {

			/* the new event is also forced and its execution time is earlier than the original, so use it instead */
			if ((options & CHECK_OPTION_FORCE_EXECUTION) && (check_time < temp_event->run_time)) {
				log_debug_info(DEBUGL_CHECKS, 2, "New host check event is forced and occurs before the existing event, so the new event be used instead.\n");
				use_original_event = FALSE;
			}
		}

		/* the original event is not a forced check... */
		else {

			/* the new event is a forced check, so use it instead */
			if ((options & CHECK_OPTION_FORCE_EXECUTION)) {
				log_debug_info(DEBUGL_CHECKS, 2, "New host check event is forced, so it will be used instead of the existing event.\n");
				use_original_event = FALSE;
			}

			/* the new event is not forced either and its execution time is earlier than the original, so use it instead */
			else if (check_time < temp_event->run_time) {
				log_debug_info(DEBUGL_CHECKS, 2, "New host check event occurs before the existing (older) event, so it will be used instead.\n");
				use_original_event = FALSE;
			}

			/* the new event is older, so override the existing one */
			else {
				log_debug_info(DEBUGL_CHECKS, 2, "New host check event occurs after the existing event, so we'll ignore it.\n");
			}
		}
	}

	/* use the new event */
	if (use_original_event == FALSE) {

		log_debug_info(DEBUGL_CHECKS, 2, "Scheduling new host check event.\n");

		/* possibly allocate memory for a new event item */
		if (temp_event) {
			remove_event(nagios_squeue, temp_event);
		}
		else if ((temp_event = (timed_event *)calloc(1, sizeof(timed_event))) == NULL) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Could not reschedule check of host '%s'!\n", hst->name);
			return;
		}

		/* set the next host check event and time */
		hst->next_check_event = temp_event;
		hst->next_check = check_time;

		/* save check options for retention purposes */
		hst->check_options = options;

		/* place the new event in the event queue */
		temp_event->event_type = EVENT_HOST_CHECK;
		temp_event->event_data = (void *)hst;
		temp_event->event_args = (void *)NULL;
		temp_event->event_options = options;
		temp_event->run_time = hst->next_check;
		temp_event->recurring = FALSE;
		temp_event->event_interval = 0L;
		temp_event->timing_func = NULL;
		temp_event->compensate_for_time_change = TRUE;
		add_event(nagios_squeue, temp_event);
	}

	else {
		/* reset the next check time (it may be out of sync) */
		if (temp_event != NULL) {
			hst->next_check = temp_event->run_time;
		}

		log_debug_info(DEBUGL_CHECKS, 2, "Keeping original host check event (ignoring the new one).\n");
	}

	/* update the status log */
	update_host_status(hst, FALSE);
}



/* checks host dependencies */
int check_host_dependencies(host *hst, int dependency_type)
{
	hostdependency *temp_dependency = NULL;
	objectlist *list;
	host *temp_host = NULL;
	int state = HOST_UP;
	time_t current_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_host_dependencies()\n");

	if (dependency_type == NOTIFICATION_DEPENDENCY) {
		list = hst->notify_deps;
	}
	else {
		list = hst->exec_deps;
	}

	/* check all dependencies... */
	for(; list; list = list->next) {
		temp_dependency = (hostdependency *)list->object_ptr;

		/* find the host we depend on... */
		if ((temp_host = temp_dependency->master_host_ptr) == NULL) {
			continue;
		}

		/* skip this dependency if it has a timeperiod and the current time isn't valid */
		time(&current_time);
		if ((temp_dependency->dependency_period != NULL) 
			&& (check_time_against_period(current_time, temp_dependency->dependency_period_ptr) == ERROR)) {

			return FALSE;
		}

		/* get the status to use (use last hard state if its currently in a soft state) */
		if (temp_host->state_type == SOFT_STATE && soft_state_dependencies == FALSE) {
			state = temp_host->last_hard_state;
		}
		else {
			state = temp_host->current_state;
		}

		/* is the host we depend on in state that fails the dependency tests? */
		if (flag_isset(temp_dependency->failure_options, 1 << state)) {
			return DEPENDENCIES_FAILED;
		}

		/* immediate dependencies ok at this point - check parent dependencies if necessary */
		if ((temp_dependency->inherits_parent == TRUE)
			&& (check_host_dependencies(temp_host, dependency_type) != DEPENDENCIES_OK)) {

			return DEPENDENCIES_FAILED;
		}
	}

	return DEPENDENCIES_OK;
}



/* check for hosts that never returned from a check... */
void check_for_orphaned_hosts(void)
{
	host *temp_host = NULL;
	time_t current_time = 0L;
	time_t expected_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_for_orphaned_hosts()\n");

	/* get the current time */
	time(&current_time);

	/* check all hosts... */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* skip hosts that don't have a set check interval (on-demand checks are missed by the orphan logic) */
		if (temp_host->next_check == (time_t)0L) {
			continue;
		}

		/* skip hosts that are not currently executing */
		if (temp_host->is_executing == FALSE) {
			continue;
		}

		/* determine the time at which the check results should have come in (allow 10 minutes slack time) */
		expected_time = (time_t)(temp_host->next_check + temp_host->latency + host_check_timeout + check_reaper_interval + 600);

		/* this host was supposed to have executed a while ago, but for some reason the results haven't come back in... */
		if (expected_time < current_time) {

			/* log a warning */
			logit(NSLOG_RUNTIME_WARNING, TRUE, 
				"Warning: The check of host '%s' looks like it was orphaned (results never came back).  I'm scheduling an immediate check of the host...\n", 
				temp_host->name);

			log_debug_info(DEBUGL_CHECKS, 1, 
				"Host '%s' was orphaned, so we're scheduling an immediate check...\n", 
				temp_host->name);

			/* decrement the number of running host checks */
			if (currently_running_host_checks > 0) {
				currently_running_host_checks--;
			}

			/* disable the executing flag */
			temp_host->is_executing = FALSE;

			/* schedule an immediate check of the host */
			schedule_host_check(temp_host, current_time, CHECK_OPTION_ORPHAN_CHECK);
		}
	}
}



/* check freshness of host results */
void check_host_result_freshness(void)
{
	host *temp_host = NULL;
	time_t current_time = 0L;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_host_result_freshness()\n");
	log_debug_info(DEBUGL_CHECKS, 2, "Attempting to check the freshness of host check results...\n");

	/* bail out if we're not supposed to be checking freshness */
	if (check_host_freshness == FALSE) {
		log_debug_info(DEBUGL_CHECKS, 2, "Host freshness checking is disabled.\n");
		return;
	}

	/* get the current time */
	time(&current_time);

	/* check all hosts... */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* skip hosts we shouldn't be checking for freshness */
		if (temp_host->check_freshness == FALSE) {
			continue;
		}

		/* skip hosts that have both active and passive checks disabled */
		if (temp_host->checks_enabled == FALSE && temp_host->accept_passive_checks == FALSE) {
			continue;
		}

		/* skip hosts that are currently executing (problems here will be caught by orphaned host check) */
		if (temp_host->is_executing == TRUE) {
			continue;
		}

		/* skip hosts that are already being freshened */
		if (temp_host->is_being_freshened == TRUE) {
			continue;
		}

		/* see if the time is right... */
		if (check_time_against_period(current_time, temp_host->check_period_ptr) == ERROR) {
			continue;
		}

		/* the results for the last check of this host are stale */
		if (is_host_result_fresh(temp_host, current_time, TRUE) == FALSE) {

			/* set the freshen flag */
			temp_host->is_being_freshened = TRUE;

			/* schedule an immediate forced check of the host */
			schedule_host_check(temp_host, current_time, CHECK_OPTION_FORCE_EXECUTION | CHECK_OPTION_FRESHNESS_CHECK);
		}
	}
}



/* checks to see if a hosts's check results are fresh */
int is_host_result_fresh(host *temp_host, time_t current_time, int log_this)
{
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
	if (temp_host->freshness_threshold == 0) {

		if (temp_host->state_type == HARD_STATE || temp_host->current_state == STATE_OK) {
			interval = temp_host->check_interval;
		}
		else {
			interval = temp_host->retry_interval;
		}

		freshness_threshold = (interval * interval_length) + temp_host->latency + additional_freshness_latency;
	}
	else {
		freshness_threshold = temp_host->freshness_threshold;
	}

	log_debug_info(DEBUGL_CHECKS, 2, "Freshness thresholds: host=%d, use=%d\n", temp_host->freshness_threshold, freshness_threshold);

	/* calculate expiration time */
	/*
	 * CHANGED 11/10/05 EG:
	 * program start is only used in expiration time calculation
	 * if > last check AND active checks are enabled, so active checks
	 * can become stale immediately upon program startup
	 */
	if (temp_host->has_been_checked == FALSE) {
		expiration_time = (time_t)(event_start + freshness_threshold);
	}
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
	else if ((temp_host->checks_enabled == TRUE) 
		  && (event_start > temp_host->last_check) 
		  && (temp_host->freshness_threshold == 0)) {

		expiration_time = (time_t)(event_start + freshness_threshold + (max_host_check_spread * interval_length));
	}
	else {
		expiration_time = (time_t)(temp_host->last_check + freshness_threshold);
	}

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
	if ((temp_host->check_type == CHECK_TYPE_PASSIVE)
		&& (temp_host->last_check < event_start) 
		&& (event_start - last_program_stop > freshness_threshold * 0.618)) {

		expiration_time = event_start + freshness_threshold;
	}

	log_debug_info(DEBUGL_CHECKS, 2, 
		"HBC: %d, PS: %lu, ES: %lu, LC: %lu, CT: %lu, ET: %lu\n", 
		temp_host->has_been_checked, 
		(unsigned long)program_start, 
		(unsigned long)event_start, 
		(unsigned long)temp_host->last_check, 
		(unsigned long)current_time, 
		(unsigned long)expiration_time);

	/* the results for the last check of this host are stale */
	if (expiration_time < current_time) {

		get_time_breakdown((current_time - expiration_time), &days, &hours, &minutes, &seconds);
		get_time_breakdown(freshness_threshold, &tdays, &thours, &tminutes, &tseconds);

		/* log a warning */
		if (log_this == TRUE) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, 
				"Warning: The results of host '%s' are stale by %dd %dh %dm %ds (threshold=%dd %dh %dm %ds).  I'm forcing an immediate check of the host.\n", 
				temp_host->name, 
				days, 
				hours, 
				minutes, 
				seconds, 
				tdays, 
				thours, 
				tminutes, 
				tseconds);
		}

		log_debug_info(DEBUGL_CHECKS, 1, 
			"Check results for host '%s' are stale by %dd %dh %dm %ds (threshold=%dd %dh %dm %ds).  Forcing an immediate check of the host...\n", 
			temp_host->name, 
			days,
			hours, 
			minutes, 
			seconds, 
			tdays, 
			thours, 
			tminutes, 
			tseconds);

		return FALSE;
	}

	log_debug_info(DEBUGL_CHECKS, 1, "Check results for host '%s' are fresh.\n", temp_host->name);
	return TRUE;
}


/* run a scheduled host check asynchronously */
int run_scheduled_host_check(host *hst, int check_options, double latency)
{
	int result = OK;
	time_t current_time = 0L;
	time_t preferred_time = 0L;
	time_t next_valid_time = 0L;
	int time_is_valid = TRUE;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "run_scheduled_host_check()\n");

	if (hst == NULL) {
		return ERROR;
	}

	log_debug_info(DEBUGL_CHECKS, 0, "Attempting to run scheduled check of host '%s': check options=%d, latency=%lf\n", hst->name, check_options, latency);

	/*
	 * reset the next_check_event so we know this host
	 * check is no longer in the scheduling queue
	 */
	hst->next_check_event = NULL;

	/* attempt to run the check */
	result = run_async_host_check(hst, check_options, latency, TRUE, TRUE, &time_is_valid, &preferred_time);

	/* an error occurred, so reschedule the check */
	if (result == ERROR) {

		log_debug_info(DEBUGL_CHECKS, 1, "Unable to run scheduled host check at this time\n");

		/* only attempt to (re)schedule checks that should get checked... */
		if (hst->should_be_scheduled == TRUE) {

			/* get current time */
			time(&current_time);

			/* determine next time we should check the host if needed */
			/* if host has no check interval, schedule it again for 5 minutes from now */
			if (current_time >= preferred_time) {
				preferred_time = current_time + ((hst->check_interval <= 0) ? 300 : (hst->check_interval * interval_length));
			}

			/* make sure we rescheduled the next host check at a valid time */
			get_next_valid_time(preferred_time, &next_valid_time, hst->check_period_ptr);

			/*
			 * If the host really can't be rescheduled properly we
			 * set next check time to preferred_time and try again then
			 */
			if ((time_is_valid == FALSE) 
				&& (check_time_against_period(next_valid_time, hst->check_period_ptr) == ERROR)) {

				hst->next_check = reschedule_within_timeperiod(next_valid_time, hst->check_period_ptr, check_window(hst));

				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check of host '%s' could not be rescheduled properly.  Scheduling check for %s...\n", hst->name, ctime(&preferred_time));

				log_debug_info(DEBUGL_CHECKS, 1, "Unable to find any valid times to reschedule the next host check!\n");
			}

			/* this service could be rescheduled... */
			else {
				hst->next_check = next_valid_time;
				if (next_valid_time > preferred_time) {
					/* Next valid time is further in the future because of
					 * timeperiod constraints. Add a random amount so we
					 * don't get all checks subject to that timeperiod
					 * constraint scheduled at the same time
					 */
					hst->next_check = reschedule_within_timeperiod(next_valid_time, hst->check_period_ptr, check_window(hst));
				}
				hst->should_be_scheduled = TRUE;

				log_debug_info(DEBUGL_CHECKS, 1, "Rescheduled next host check for %s", ctime(&next_valid_time));
			}
		}

		/* update the status log */
		update_host_status(hst, FALSE);

		/* reschedule the next host check - unless we couldn't find a valid next check time */
		/* 10/19/07 EG - keep original check options */
		if (hst->should_be_scheduled == TRUE) {
			schedule_host_check(hst, hst->next_check, check_options);
		}

		return ERROR;
	}

	return OK;
}



/* perform an asynchronous check of a host */
/* scheduled host checks will use this, as will some checks that result from on-demand checks... */
int run_async_host_check(host *hst, int check_options, double latency, int scheduled_check, int reschedule_check, int *time_is_valid, time_t *preferred_time)
{
	nagios_macros mac;
	char *raw_command = NULL;
	char *processed_command = NULL;
	struct timeval start_time, end_time;
	double old_latency = 0.0;
	check_result *cr;
	int runchk_result = OK;
	int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;
#ifdef USE_EVENT_BROKER
	int neb_result = OK;
#endif

	log_debug_info(DEBUGL_FUNCTIONS, 0, "run_async_host_check(%s ...)\n", hst ? hst->name : "(NULL host!)");

	/* make sure we have a host */
	if (hst == NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS, 0, "** Running async check of host '%s'...\n", hst->name);

	/* abort if check is already running or was recently completed */
	if (!(check_options & CHECK_OPTION_FORCE_EXECUTION)) {
		if (hst->is_executing == TRUE) {
			log_debug_info(DEBUGL_CHECKS, 1, "A check of this host is already being executed, so we'll pass for the moment...\n");
			return ERROR;
		}

		if (hst->last_check + cached_host_check_horizon > time(NULL)) {
			log_debug_info(DEBUGL_CHECKS, 0, "Host '%s' was last checked within its cache horizon. Aborting check\n", hst->name);
			return ERROR;
		}
	}

	log_debug_info(DEBUGL_CHECKS, 0, "Host '%s' passed first hurdle (caching/execution)\n", hst->name);
	/* is the host check viable at this time? */
	if (check_host_check_viability(hst, check_options, time_is_valid, preferred_time) == ERROR) {
		log_debug_info(DEBUGL_CHECKS, 0, "Host check isn't viable at this point.\n");
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
	neb_result = broker_host_check(NEBTYPE_HOSTCHECK_ASYNC_PRECHECK, NEBFLAG_NONE, NEBATTR_NONE, hst, CHECK_TYPE_ACTIVE, hst->current_state, hst->state_type, start_time, end_time, hst->check_command, hst->latency, 0.0, host_check_timeout, FALSE, 0, NULL, NULL, NULL, NULL, NULL, NULL);

	/* neb module wants to cancel the host check - the check will be rescheduled for a later time by the scheduling logic */
	if (neb_result == NEBERROR_CALLBACKCANCEL) {

		log_debug_info(DEBUGL_CHECKS, 0, "Check of host '%s' (id=%u) was cancelled by a module\n", hst->name, hst->id);

		if (preferred_time) {
			*preferred_time += check_window(hst);
		}
		return ERROR;
	}

	/* neb module wants to override the host check - perhaps it will check the host itself */
	/* NOTE: if a module does this, it has to do a lot of the stuff found below to make sure things don't get whacked out of shape! */
	/* NOTE: if would be easier for modules to override checks when the NEBTYPE_SERVICECHECK_INITIATE event is called (later) */
	if (neb_result == NEBERROR_CALLBACKOVERRIDE) {

		log_debug_info(DEBUGL_CHECKS, 0, "Check of host '%s' (id=%u) was overridden by a module\n", hst->name, hst->id);
		return OK;
	}
#endif

	log_debug_info(DEBUGL_CHECKS, 0, "Checking host '%s'...\n", hst->name);

	/* clear check options - we don't want old check options retained */
	/* only clear options if this was a scheduled check - on demand check options shouldn't affect retained info */
	if (scheduled_check == TRUE) {
		hst->check_options = CHECK_OPTION_NONE;
	}

	/* set latency (temporarily) for macros and event broker */
	old_latency = hst->latency;
	hst->latency = latency;

	/* grab the host macro variables */
	memset(&mac, 0, sizeof(mac));
	grab_host_macros_r(&mac, hst);

	/* get the raw command line */
	get_raw_command_line_r(&mac, hst->check_command_ptr, hst->check_command, &raw_command, macro_options);
	if (raw_command == NULL) {
		clear_volatile_macros_r(&mac);
		log_debug_info(DEBUGL_CHECKS, 0, "Raw check command for host '%s' was NULL - aborting.\n", hst->name);
		return ERROR;
	}

	/* process any macros contained in the argument */
	process_macros_r(&mac, raw_command, &processed_command, macro_options);
	my_free(raw_command);
	if (processed_command == NULL) {
		clear_volatile_macros_r(&mac);
		log_debug_info(DEBUGL_CHECKS, 0, "Processed check command for host '%s' was NULL - aborting.\n", hst->name);
		return ERROR;
	}

	/* get the command start time */
	gettimeofday(&start_time, NULL);

	cr = calloc(1, sizeof(*cr));
	if (!cr) {
		log_debug_info(DEBUGL_CHECKS, 0, "Failed to allocate checkresult struct\n");
		clear_volatile_macros_r(&mac);
		clear_host_macros_r(&mac);
		return ERROR;
	}
	init_check_result(cr);

	/* save check info */
	cr->object_check_type = HOST_CHECK;
	cr->host_name = (char *)strdup(hst->name);
	cr->service_description = NULL;
	cr->check_type = CHECK_TYPE_ACTIVE;
	cr->check_options = check_options;
	cr->scheduled_check = scheduled_check;
	cr->reschedule_check = reschedule_check;
	cr->latency = latency;
	cr->start_time = start_time;
	cr->finish_time = start_time;
	cr->early_timeout = FALSE;
	cr->exited_ok = TRUE;
	cr->return_code = STATE_OK;
	cr->output = NULL;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	neb_result = broker_host_check(NEBTYPE_HOSTCHECK_INITIATE, NEBFLAG_NONE, NEBATTR_NONE, hst, CHECK_TYPE_ACTIVE, hst->current_state, hst->state_type, start_time, end_time, hst->check_command, hst->latency, 0.0, host_check_timeout, FALSE, 0, processed_command, NULL, NULL, NULL, NULL, cr);

	/* neb module wants to override the service check - perhaps it will check the service itself */
	if (neb_result == NEBERROR_CALLBACKOVERRIDE) {
		clear_volatile_macros_r(&mac);
		hst->latency = old_latency;
		free_check_result(cr);
		my_free(cr);
		my_free(processed_command);
		return OK;
	}
#endif

	/* reset latency (permanent value for this check will get set later) */
	hst->latency = old_latency;

	runchk_result = wproc_run_check(cr, processed_command, &mac);
	if (runchk_result == ERROR) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Unable to send check for host '%s' to worker (ret=%d)\n", hst->name, runchk_result);
	}
	else {
		/* do the book-keeping */
		currently_running_host_checks++;
		hst->is_executing = TRUE;
		update_check_stats((scheduled_check == TRUE) ? ACTIVE_SCHEDULED_HOST_CHECK_STATS : ACTIVE_ONDEMAND_HOST_CHECK_STATS, start_time.tv_sec);
		update_check_stats(PARALLEL_HOST_CHECK_STATS, start_time.tv_sec);
	}


	/* free memory */
	clear_volatile_macros_r(&mac);
	my_free(processed_command);

	return runchk_result;
}

/* process results of an asynchronous host check */


/* checks viability of performing a host check */
int check_host_check_viability(host *hst, int check_options, int *time_is_valid, time_t *new_time)
{
	int perform_check = TRUE;
	time_t current_time = 0L;
	time_t preferred_time = 0L;
	int check_interval = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_host_check_viability()\n");

	/* make sure we have a host */
	if (hst == NULL) {
		return ERROR;
	}

	/* get the check interval to use if we need to reschedule the check */
	if (hst->state_type == SOFT_STATE && hst->current_state != HOST_UP) {
		check_interval = (hst->retry_interval * interval_length);
	}
	else {
		check_interval = (hst->check_interval * interval_length);
	}

	/* make sure check interval is positive - otherwise use 5 minutes out for next check */
	if (check_interval <= 0) {
		check_interval = 300;
	}

	/* get the current time */
	time(&current_time);

	/* initialize the next preferred check time */
	preferred_time = current_time;

	/* can we check the host right now? */
	if (!(check_options & CHECK_OPTION_FORCE_EXECUTION)) {

		/* if checks of the host are currently disabled... */
		if (hst->checks_enabled == FALSE) {
			preferred_time = current_time + check_interval;
			perform_check = FALSE;
		}

		/* make sure this is a valid time to check the host */
		if (check_time_against_period((unsigned long)current_time, hst->check_period_ptr) == ERROR) {
			log_debug_info(DEBUGL_CHECKS, 0, "Timeperiod check failed\n");
			preferred_time = current_time;
			if (time_is_valid) {
				*time_is_valid = FALSE;
			}
			perform_check = FALSE;
		}

		/* check host dependencies for execution */
		if (check_host_dependencies(hst, EXECUTION_DEPENDENCY) == DEPENDENCIES_FAILED) {
			log_debug_info(DEBUGL_CHECKS, 0, "Host check dependencies failed\n");
			preferred_time = current_time + check_interval;
			perform_check = FALSE;
			if (host_skip_check_dependency_status >= 0) {
				hst->current_state = host_skip_check_dependency_status;
			}
		}
	}

	/* pass back the next viable check time */
	if (new_time) {
		*new_time = preferred_time;
	}

	if (perform_check == TRUE) {
		return OK;
	}

	return ERROR;
}



/* determination of the host's state based on route availability*/
/* used only to determine difference between DOWN and UNREACHABLE states */
inline int determine_host_reachability(host *hst)
{
	host *parent_host = NULL;
	hostsmember *temp_hostsmember = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "determine_host_reachability(host=%s)\n", hst ? hst->name : "(NULL host!)");

	if (hst == NULL)
		return HOST_DOWN;

	log_debug_info(DEBUGL_CHECKS, 2, "Determining state of host '%s': current state=%d (%s)\n", hst->name, hst->current_state, host_state_name(hst->current_state));

	/* host is UP - no translation needed */
	if (hst->current_state == HOST_UP) {
		log_debug_info(DEBUGL_CHECKS, 2, "Host is UP, no state translation needed.\n");
		return HOST_UP;
	}

	/* host has no parents, so it is DOWN */
	if (hst->check_type == CHECK_TYPE_PASSIVE && hst->current_state == HOST_UNREACHABLE) {
		log_debug_info(DEBUGL_CHECKS, 2, "Passive check so keep it UNREACHABLE.\n");
		return HOST_UNREACHABLE;
	}
	else if (hst->parent_hosts == NULL) {
		log_debug_info(DEBUGL_CHECKS, 2, "Host has no parents, so it is DOWN.\n");
		return HOST_DOWN;
	}

	/* check all parent hosts to see if we're DOWN or UNREACHABLE */
	else {
		for(temp_hostsmember = hst->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
			parent_host = temp_hostsmember->host_ptr;
			log_debug_info(DEBUGL_CHECKS, 2, "   Parent '%s' is %s\n", parent_host->name, host_state_name(parent_host->current_state));
			/* bail out as soon as we find one parent host that is UP */
			if (parent_host->current_state == HOST_UP) {
				/* set the current state */
				log_debug_info(DEBUGL_CHECKS, 2, "At least one parent (%s) is up, so host is DOWN.\n", parent_host->name);
				return HOST_DOWN;
			}
		}
	}

	log_debug_info(DEBUGL_CHECKS, 2, "No parents were up, so host is UNREACHABLE.\n");
	return HOST_UNREACHABLE;
}



/******************************************************************/
/****************** HOST STATE HANDLER FUNCTIONS ******************/
/******************************************************************/



/* Parses raw plugin output and returns: short and long output, perf data. */
int parse_check_output(char *buf, char **short_output, char **long_output, char **perf_data, int escape_newlines_please, int newlines_are_escaped)
{
	int current_line = 0;
	int eof = FALSE;
	int in_perf_data = FALSE;
	const int dbuf_chunk = 1024;
	dbuf long_text;
	dbuf perf_text;
	char *ptr = NULL;
	int x = 0;
	int y = 0;

	/* Initialize output values. */
	if (short_output) {
		*short_output = NULL;
	}
	if (long_output) {
		*long_output = NULL;
	}
	if (perf_data) {
		*perf_data = NULL;
	}

	/* No input provided or no output requested, nothing to do. */
	if (!buf 
		|| !*buf 
		|| (!short_output && !long_output && !perf_data)) {

		return OK;
	}

	/* Initialize dynamic buffers (1KB chunk size). */
	dbuf_init(&long_text, dbuf_chunk);
	dbuf_init(&perf_text, dbuf_chunk);

	/* We should never need to worry about unescaping here again. We assume a
	 * common internal plugin output format that is newline delimited. */
	if (newlines_are_escaped) {
		for (x = 0, y = 0; buf[x]; x++) {
			if (buf[x] == '\\' && buf[x + 1] == '\\') {
				x++;
				buf[y++] = buf[x];
			}
			else if (buf[x] == '\\' && buf[x + 1] == 'n') {
				x++;
				buf[y++] = '\n';
			}
			else {
				buf[y++] = buf[x];
			}
		}
		buf[y] = '\0';
	}

	/* Process each line of input. */
	for (x = 0; !eof && buf[0]; x++) {

		/* Continue on until we reach the end of a line (or input). */
		if (buf[x] == '\n') {
			buf[x] = '\0';
		}
		else if (buf[x] == '\0') {
			eof = TRUE;
		}
		else {
			continue;
		}

		/* Handle this line of input. */
		current_line++;

		/* The first line contains short plugin output and optional perf data. */
		if (current_line == 1) {

			/* Get the short plugin output. If buf[0] is '|', strtok() will
			 * return buf+1 or NULL if buf[1] is '\0'. We use my_strtok_with_free()
			 * instead which returns a pointer to '\0' in this case. */
			ptr =  my_strtok_with_free(buf, "|", FALSE);
			if (ptr != NULL) {

				if (short_output) {

					/* Remove leading and trailing whitespace. */
					strip(ptr);
					*short_output = strdup(ptr);
				}

				/* Get the optional perf data. */
				ptr = my_strtok_with_free(NULL, "\n", FALSE);
				if (ptr != NULL) {
					dbuf_strcat(&perf_text, ptr);
				}

				/* free anything we've allocated */
				my_strtok_with_free(NULL, NULL, TRUE);
			}
		}

		/* Additional lines contain long plugin output and optional perf data.
		 * Once we've hit perf data, the rest of the output is perf data. */
		else if (in_perf_data) {
			if (perf_text.buf && *perf_text.buf) {
				dbuf_strcat(&perf_text, " ");
			}
			dbuf_strcat(&perf_text, buf);
		}

		/* Look for the perf data separator. */
		else if (strchr(buf, '|')) {
			in_perf_data = TRUE;

			ptr = my_strtok_with_free(buf, "|", FALSE);
			if (ptr != NULL) {

				/* Get the remaining long plugin output. */
				if (current_line > 2) {
					dbuf_strcat(&long_text, "\n");
				}
				dbuf_strcat(&long_text, ptr);

				/* Get the perf data. */
				ptr = my_strtok_with_free(NULL, "\n", FALSE);
				if (ptr != NULL) {
					if (perf_text.buf && *perf_text.buf) {
						dbuf_strcat(&perf_text, " ");
					}
					dbuf_strcat(&perf_text, ptr);
				}

				/* free anything we've allocated */
				my_strtok_with_free(NULL, NULL, TRUE);
			}
		}

		/* Otherwise it's still just long output. */
		else {
			if (current_line > 2) {
				dbuf_strcat(&long_text, "\n");
			}
			dbuf_strcat(&long_text, buf);
		}

		/* Point buf to the start of the next line. *(buf+x+1) will be a valid
		 * memory reference on our next iteration or we are at the end of input
		 * (eof == TRUE) and *(buf+x+1) will never be referenced. */
		buf += x + 1;

		/* x will be incremented to 0 by the loop update. */
		x = -1;
	}

	/* Save long output. */
	if (long_output && long_text.buf && *long_text.buf) {

		/* Escape newlines (and backslashes) in long output if requested. */
		if (escape_newlines_please) {
			*long_output = escape_newlines(long_text.buf);
		}
		else {
			*long_output = strdup(long_text.buf);
		}
	}

	/* Save perf data. */
	if (perf_data && perf_text.buf && *perf_text.buf) {

		/* Remove leading and trailing whitespace. */
		strip(perf_text.buf); 
		*perf_data = strdup(perf_text.buf);
	}

	/* free dynamic buffers */
	dbuf_free(&long_text);
	dbuf_free(&perf_text);

	return OK;
}
