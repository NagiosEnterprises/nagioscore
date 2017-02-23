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


#ifdef USE_EVENT_BROKER
#include "../include/neberrors.h"
#endif

/******************************************************************/
/********************** CHECK REAPER FUNCTIONS ********************/
/******************************************************************/

/* reaps host and service check results */
int reap_check_results(void) {
	int reaped_checks = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "reap_check_results() start\n");
	log_debug_info(DEBUGL_CHECKS, 0, "Starting to reap check results.\n");

	/* process files in the check result queue */
	reaped_checks = process_check_result_queue(check_result_path);

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
			 * If we really can't reschedule the service properly, we
			 * just push the check to preferred_time plus some reasonable
			 * random value and try again then.
			 */
			if(time_is_valid == FALSE &&  check_time_against_period(next_valid_time, svc->check_period_ptr) == ERROR) {

				svc->next_check = preferred_time +
						ranged_urand(0, check_window(svc));

				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check of service '%s' on host '%s' could not be rescheduled properly.  Scheduling check for %s...\n", svc->description, svc->host_name, ctime(&preferred_time));

				log_debug_info(DEBUGL_CHECKS, 1, "Unable to find any valid times to reschedule the next service check!\n");
				}

			/* this service could be rescheduled... */
			else {
				svc->next_check = next_valid_time;
				if(next_valid_time > preferred_time) {
					/* Next valid time is further in the future because of
					 * timeperiod constraints. Add a random amount so we
					 * don't get all checks subject to that timeperiod
					 * constraint scheduled at the same time
					 */
					svc->next_check += ranged_urand(0, check_window(svc));
					}
				svc->should_be_scheduled = TRUE;

				log_debug_info(DEBUGL_CHECKS, 1, "Rescheduled next service check for %s", ctime(&next_valid_time));
				}
			}

		/*
		 * reschedule the next service check - unless we couldn't
		 * find a valid next check time, but keep original options
		 */
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
	neb_result = broker_service_check(NEBTYPE_SERVICECHECK_ASYNC_PRECHECK, NEBFLAG_NONE, NEBATTR_NONE, svc, CHECK_TYPE_ACTIVE, start_time, end_time, svc->check_command, svc->latency, 0.0, 0, FALSE, 0, NULL, NULL, NULL);

	if (neb_result == NEBERROR_CALLBACKCANCEL || neb_result == NEBERROR_CALLBACKOVERRIDE) {
		log_debug_info(DEBUGL_CHECKS, 0, "Check of service '%s' on host '%s' (id=%u) was %s by a module\n",
		               svc->description, svc->host_name, svc->id,
		               neb_result == NEBERROR_CALLBACKCANCEL ? "cancelled" : "overridden");
	}
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
	get_raw_command_line_r(&mac, svc->check_command_ptr, svc->check_command, &raw_command, macro_options);
	if(raw_command == NULL) {
		clear_volatile_macros_r(&mac);
		log_debug_info(DEBUGL_CHECKS, 0, "Raw check command for service '%s' on host '%s' was NULL - aborting.\n", svc->description, svc->host_name);
		if(preferred_time)
			*preferred_time += (svc->check_interval * interval_length);
		svc->latency = old_latency;
		return ERROR;
		}

	/* process any macros contained in the argument */
	process_macros_r(&mac, raw_command, &processed_command, macro_options);
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
	if(neb_result == NEBERROR_CALLBACKOVERRIDE) {
		clear_volatile_macros_r(&mac);
		svc->latency = old_latency;
		free_check_result(cr);
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


static int get_service_check_return_code(service *temp_service,
		check_result *queued_check_result) {

	int rc;
	char *temp_plugin_output = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "get_service_check_return_code()\n");

	if(NULL == temp_service || NULL == queued_check_result) {
		return STATE_UNKNOWN;
		}

	/* grab the return code */
	rc = queued_check_result->return_code;

	/* adjust return code (active checks only) */
	if(queued_check_result->check_type == CHECK_TYPE_ACTIVE) {
		if(queued_check_result->early_timeout == TRUE) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check of service '%s' on host '%s' timed out after %.3fs!\n", temp_service->description, temp_service->host_name, temp_service->execution_time);
			my_free(temp_service->plugin_output);
			my_free(temp_service->long_plugin_output);
			my_free(temp_service->perf_data);
			asprintf(&temp_service->plugin_output, "(Service check timed out after %.2lf seconds)", temp_service->execution_time);
			rc = service_check_timeout_state;
			}
		/* if there was some error running the command, just skip it (this shouldn't be happening) */
		else if(queued_check_result->exited_ok == FALSE) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning:  Check of service '%s' on host '%s' did not exit properly!\n", temp_service->description, temp_service->host_name);
			
			my_free(temp_service->plugin_output);
			my_free(temp_service->long_plugin_output);
			my_free(temp_service->perf_data);

			temp_service->plugin_output = (char *)strdup("(Service check did not exit properly)");

			rc = STATE_CRITICAL;
			}

		/* make sure the return code is within bounds */
		else if(queued_check_result->return_code < 0 || queued_check_result->return_code > 3) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Return code of %d for check of service '%s' on host '%s' was out of bounds.%s\n", queued_check_result->return_code, temp_service->description, temp_service->host_name, (queued_check_result->return_code == 126 ? "Make sure the plugin you're trying to run is executable." : (queued_check_result->return_code == 127 ? " Make sure the plugin you're trying to run actually exists." : "")));

			asprintf(&temp_plugin_output, "(Return code of %d is out of bounds%s : %s)", queued_check_result->return_code, (queued_check_result->return_code == 126 ? " - plugin may not be executable" : (queued_check_result->return_code == 127 ? " - plugin may be missing" : "")), temp_service->plugin_output);
			my_free(temp_service->plugin_output);

			asprintf(&temp_service->plugin_output, "%s)", temp_plugin_output);
			my_free(temp_plugin_output);
			my_free(temp_service->long_plugin_output);
			my_free(temp_service->perf_data);

			rc = STATE_CRITICAL;
			}
		}

	return rc;
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
	service *master_service = NULL;
	int state_changes_use_cached_state = TRUE; /* TODO - 09/23/07 move this to a global variable */
	int flapping_check_done = FALSE;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "handle_async_service_check_result()\n");

	/* make sure we have what we need */
	if(temp_service == NULL || queued_check_result == NULL)
		return ERROR;

	/* get the current time */
	time(&current_time);

	if (current_time < temp_service->next_check)
		next_service_check = temp_service->next_check + check_window(temp_service);
	else
		next_service_check = current_time + check_window(temp_service);

	log_debug_info(DEBUGL_CHECKS, 0, "** Handling check result for service '%s' on host '%s' from '%s'...\n", temp_service->description, temp_service->host_name, check_result_source(queued_check_result));
	log_debug_info(DEBUGL_CHECKS, 1, "HOST: %s, SERVICE: %s, CHECK TYPE: %s, OPTIONS: %d, SCHEDULED: %s, RESCHEDULE: %s, EXITED OK: %s, RETURN CODE: %d, OUTPUT: %s\n", temp_service->host_name, temp_service->description, (queued_check_result->check_type == CHECK_TYPE_ACTIVE) ? "Active" : "Passive", queued_check_result->check_options, (queued_check_result->scheduled_check == TRUE) ? "Yes" : "No", (queued_check_result->reschedule_check == TRUE) ? "Yes" : "No", (queued_check_result->exited_ok == TRUE) ? "Yes" : "No", queued_check_result->return_code, queued_check_result->output);

	/* decrement the number of service checks still out there... */
	if(queued_check_result->check_type == CHECK_TYPE_ACTIVE && currently_running_service_checks > 0)
		currently_running_service_checks--;

	/* skip this service check results if its passive and we aren't accepting passive check results */
	if(queued_check_result->check_type == CHECK_TYPE_PASSIVE) {
		if(accept_passive_service_checks == FALSE) {
			log_debug_info(DEBUGL_CHECKS, 0, "Discarding passive service check result because passive service checks are disabled globally.\n");
			return ERROR;
			}
		if(temp_service->accept_passive_checks == FALSE) {
			log_debug_info(DEBUGL_CHECKS, 0, "Discarding passive service check result because passive checks are disabled for this service.\n");
			return ERROR;
			}
		}

	/* clear the freshening flag (it would have been set if this service was determined to be stale) */
	if(queued_check_result->check_options & CHECK_OPTION_FRESHNESS_CHECK)
		temp_service->is_being_freshened = FALSE;

	/* clear the execution flag if this was an active check */
	if(queued_check_result->check_type == CHECK_TYPE_ACTIVE)
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
	temp_service->check_type = (queued_check_result->check_type == CHECK_TYPE_ACTIVE) ? CHECK_TYPE_ACTIVE : CHECK_TYPE_PASSIVE;

	/* update check statistics for passive checks */
	if(queued_check_result->check_type == CHECK_TYPE_PASSIVE)
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

	/* parse check output to get: (1) short output, (2) long output, (3) perf data */
	parse_check_output(queued_check_result->output, &temp_service->plugin_output, &temp_service->long_plugin_output, &temp_service->perf_data, TRUE, FALSE);

	/* make sure the plugin output isn't null */
	if(temp_service->plugin_output == NULL)
		temp_service->plugin_output = (char *)strdup("(No output returned from plugin)");

	/* replace semicolons in plugin output (but not performance data) with colons */
	else if((temp_ptr = temp_service->plugin_output)) {
		while((temp_ptr = strchr(temp_ptr, ';')))
			* temp_ptr = ':';
		}

	/* grab the return code */
	temp_service->current_state = get_service_check_return_code(temp_service,
			queued_check_result);

	log_debug_info(DEBUGL_CHECKS, 2, "Parsing check output...\n");
	log_debug_info(DEBUGL_CHECKS, 2, "Short Output: %s\n", (temp_service->plugin_output == NULL) ? "NULL" : temp_service->plugin_output);
	log_debug_info(DEBUGL_CHECKS, 2, "Long Output:  %s\n", (temp_service->long_plugin_output == NULL) ? "NULL" : temp_service->long_plugin_output);
	log_debug_info(DEBUGL_CHECKS, 2, "Perf Data:    %s\n", (temp_service->perf_data == NULL) ? "NULL" : temp_service->perf_data);

	/* record the time the last state ended */
	switch(temp_service->last_state) {
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
	if(temp_service->check_type == CHECK_TYPE_PASSIVE) {
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
			schedule_host_check(temp_host, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
			}
		}


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

		/*
		 * hard changes between non-OK states should continue
		 * to be escalated, so don't reset current notification number
		 */
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

			if(first_host_check_initiated == TRUE)
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
					schedule_host_check(temp_host, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
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
		temp_service->notified_on = 0;

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

			/* only run a new check if we can and have to */
			if(execute_host_checks && (state_change == TRUE && state_changes_use_cached_state == FALSE) && temp_host->last_check + cached_host_check_horizon < current_time) {
				schedule_host_check(temp_host, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
				}
			else {
				log_debug_info(DEBUGL_CHECKS, 1, "* Using cached host state: %d\n", temp_host->current_state);
				route_result = temp_host->current_state;
				update_check_stats(ACTIVE_ONDEMAND_HOST_CHECK_STATS, current_time);
				update_check_stats(ACTIVE_CACHED_HOST_CHECK_STATS, current_time);
				}
			}

		/* else the host is either down or unreachable, so recheck it if necessary */
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is currently %s.\n", host_state_name(temp_host->current_state));

			if(execute_host_checks && (state_change == TRUE && state_changes_use_cached_state == FALSE)) {
				schedule_host_check(temp_host, current_time, CHECK_OPTION_NONE);
				}
			/* else fake the host check, but (possibly) resend host notifications to contacts... */
			else {

				log_debug_info(DEBUGL_CHECKS, 1, "Assuming host is in same state as before...\n");

				/* if the host has never been checked before, set the checked flag and last check time */
				/* This probably never evaluates to FALSE, present for historical reasons only, can probably be removed in the future */
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
		if(route_result != HOST_UP) {
			if (temp_host->state_type == HARD_STATE) {
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
				}
			else if (temp_service->last_state == STATE_OK)
				temp_service->state_type = SOFT_STATE;
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

		/* if we should retry the service check, do so (except if the host is down or unreachable!) */
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
			if(execute_service_checks && enable_predictive_service_dependency_checks == TRUE && temp_service->current_attempt == (temp_service->max_attempts - 1)) {
				objectlist *list;

				log_debug_info(DEBUGL_CHECKS, 1, "Looking for services to check for predictive dependency checks...\n");

				/* check services that THIS ONE depends on for notification AND execution */
				/* we do this because we might be sending out a notification soon and we want the dependency logic to be accurate */
				for(list = temp_service->exec_deps; list; list = list->next) {
					temp_dependency = (servicedependency *)list->object_ptr;
					if(temp_dependency->dependent_service_ptr == temp_service && temp_dependency->master_service_ptr != NULL) {
						master_service = (service *)temp_dependency->master_service_ptr;
						log_debug_info(DEBUGL_CHECKS, 2, "Predictive check of service '%s' on host '%s' queued.\n", master_service->description, master_service->host_name);
						schedule_service_check(master_service, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
						}
					}
				for(list = temp_service->notify_deps; list; list = list->next) {
					temp_dependency = (servicedependency *)list->object_ptr;
					if(temp_dependency->dependent_service_ptr == temp_service && temp_dependency->master_service_ptr != NULL) {
						master_service = (service *)temp_dependency->master_service_ptr;
						log_debug_info(DEBUGL_CHECKS, 2, "Predictive check of service '%s' on host '%s' queued.\n", master_service->description, master_service->host_name);
						schedule_service_check(master_service, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
						}
					}
				}
			}


		/* we've reached the maximum number of service rechecks, so handle the error */
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Service has reached max number of rechecks, so we'll handle the error...\n");

			/* this is a hard state */
			temp_service->state_type = HARD_STATE;

			/* check for start of flexible (non-fixed) scheduled downtime if we just had a hard error */
			/* we need to check for both, state_change (SOFT) and hard_state_change (HARD) values */
			if((hard_state_change == TRUE || state_change == TRUE) && temp_service->pending_flex_downtime > 0)
				check_pending_flex_service_downtime(temp_service);

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
		if(next_valid_time > preferred_time) {
			/* Next valid time is further in the future because of timeperiod
			 * constraints. Add a random amount so we don't get all checks
			 * subject to that timeperiod constraint scheduled at the same time
			 */
			temp_service->next_check +=
					ranged_urand(0, check_window(temp_service));
		}

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

		if(should_stalk(temp_service))
			log_service_event(temp_service);

		}

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_service_check(NEBTYPE_SERVICECHECK_PROCESSED, NEBFLAG_NONE, NEBATTR_NONE, temp_service, temp_service->check_type, queued_check_result->start_time, queued_check_result->finish_time, NULL, temp_service->latency, temp_service->execution_time, service_check_timeout, queued_check_result->early_timeout, queued_check_result->return_code, NULL, NULL, queued_check_result);
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

	return OK;
	}



/* schedules an immediate or delayed service check */
void schedule_service_check(service *svc, time_t check_time, int options) {
	timed_event *temp_event = NULL;
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
		/* make sure we remove the old event from the queue */
		if(temp_event) {
			remove_event(nagios_squeue, temp_event);
			}
		else {
			/* allocate memory for a new event item */
			temp_event = (timed_event *)calloc(1, sizeof(timed_event));
			if(temp_event == NULL) {
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
	host *temp_host = NULL;

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

		/* check if parent service is OK */
		if(check_service_parents(svc) == DEPENDENCIES_FAILED) {
			preferred_time = current_time + check_interval;
			perform_check = FALSE;
			log_debug_info(DEBUGL_CHECKS, 2, "Execution parents for this service failed, so it will not be actively checked.\n");
		}

		/* check if host is up - if not, do not perform check */
		if(host_down_disable_service_checks) {
			if((temp_host = svc->host_ptr) == NULL) {
				log_debug_info(DEBUGL_CHECKS, 2, "Host pointer NULL in check_service_check_viability().\n");
				return ERROR;
			} else {
				if(temp_host->current_state != HOST_UP) {
					log_debug_info(DEBUGL_CHECKS, 2, "Host state not UP, so service check will not be performed - will be rescheduled as normal.\n");
					perform_check = FALSE;
				}
			}
		}

	/* pass back the next viable check time */
	if(new_time)
		*new_time = preferred_time;

	result = (perform_check == TRUE) ? OK : ERROR;

	return result;
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
		if((parent_service = temp_servicesmember->service_ptr) == NULL) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: service '%s' on host '%s' is NULL ptr\n",
				  temp_servicesmember->service_description, temp_servicesmember->host_name);
			continue;
		}

		state = parent_service->last_hard_state;

		/* is the service we depend on in a state that fails the dependency tests? */
		if((state == STATE_CRITICAL) || (state == STATE_UNKNOWN))
			return DEPENDENCIES_FAILED;

		if(check_service_parents(parent_service) != DEPENDENCIES_OK)
			return DEPENDENCIES_FAILED;
	}

	return DEPENDENCIES_OK;
}



/* checks service dependencies */
int check_service_dependencies(service *svc, int dependency_type) {
	objectlist *list;
	int state = STATE_OK;
	time_t current_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_service_dependencies()\n");

	/* only check dependencies of the desired type */
	if(dependency_type == NOTIFICATION_DEPENDENCY)
		list = svc->notify_deps;
	else
		list = svc->exec_deps;

	/* check all dependencies of the desired type... */
	for(; list; list = list->next) {
		service *temp_service;
		servicedependency *temp_dependency = (servicedependency *)list->object_ptr;

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
		if(flag_isset(temp_dependency->failure_options, 1 << state))
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
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: The check of service '%s' on host '%s' looks like it was orphaned (results never came back; last_check=%lu; next_check=%lu).  I'm scheduling an immediate check of the service...\n", temp_service->description, temp_service->host_name, temp_service->last_check, temp_service->next_check);

			log_debug_info(DEBUGL_CHECKS, 1, "Service '%s' on host '%s' was orphaned, so we're scheduling an immediate check...\n", temp_service->description, temp_service->host_name);
			log_debug_info(DEBUGL_CHECKS, 1, "  next_check=%lu (%s); last_check=%lu (%s);\n",
						   temp_service->next_check, ctime(&temp_service->next_check),
						   temp_service->last_check, ctime(&temp_service->last_check));

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
		if(temp_service->checks_enabled == FALSE && temp_service->accept_passive_checks == FALSE)
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
	if (temp_service->check_type == CHECK_TYPE_PASSIVE) {
		if (temp_service->last_check < event_start &&
			event_start - last_program_stop > freshness_threshold * 0.618)
		{
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

/* schedules an immediate or delayed host check */
void schedule_host_check(host *hst, time_t check_time, int options) {
	timed_event *temp_event = NULL;
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

	if (options == CHECK_OPTION_DEPENDENCY_CHECK) {
		if (hst->last_check + cached_host_check_horizon > check_time) {
			log_debug_info(DEBUGL_CHECKS, 0, "Last check result is recent enough (%s)\n", ctime(&hst->last_check));
			return;
		}
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

		/* possibly allocate memory for a new event item */
		if (temp_event) {
			remove_event(nagios_squeue, temp_event);
			}
		else if((temp_event = (timed_event *)calloc(1, sizeof(timed_event))) == NULL) {
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
	objectlist *list;
	host *temp_host = NULL;
	int state = HOST_UP;
	time_t current_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_host_dependencies()\n");

	if (dependency_type == NOTIFICATION_DEPENDENCY) {
		list = hst->notify_deps;
	} else {
		list = hst->exec_deps;
	}

	/* check all dependencies... */
	for(; list; list = list->next) {
		temp_dependency = (hostdependency *)list->object_ptr;

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
		if(flag_isset(temp_dependency->failure_options, 1 << state))
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
		if(temp_host->checks_enabled == FALSE && temp_host->accept_passive_checks == FALSE)
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
	if (temp_host->check_type == CHECK_TYPE_PASSIVE) {
		if (temp_host->last_check < event_start &&
			event_start - last_program_stop > freshness_threshold * 0.618)
		{
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


/* run a scheduled host check asynchronously */
int run_scheduled_host_check(host *hst, int check_options, double latency) {
	int result = OK;
	time_t current_time = 0L;
	time_t preferred_time = 0L;
	time_t next_valid_time = 0L;
	int time_is_valid = TRUE;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "run_scheduled_host_check()\n");

	if(hst == NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS, 0, "Attempting to run scheduled check of host '%s': check options=%d, latency=%lf\n", hst->name, check_options, latency);

	/*
	 * reset the next_check_event so we know this host
	 * check is no longer in the scheduling queue
	 */
	hst->next_check_event = NULL;

	/* attempt to run the check */
	result = run_async_host_check(hst, check_options, latency, TRUE, TRUE, &time_is_valid, &preferred_time);

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

			/*
			 * If the host really can't be rescheduled properly we
			 * set next check time to preferred_time and try again then
			 */
			if(time_is_valid == FALSE && check_time_against_period(next_valid_time, hst->check_period_ptr) == ERROR) {

				hst->next_check = preferred_time +
						ranged_urand(0, check_window(hst));

				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check of host '%s' could not be rescheduled properly.  Scheduling check for %s...\n", hst->name, ctime(&preferred_time));

				log_debug_info(DEBUGL_CHECKS, 1, "Unable to find any valid times to reschedule the next host check!\n");
				}

			/* this service could be rescheduled... */
			else {
				hst->next_check = next_valid_time;
				if(next_valid_time > preferred_time) {
					/* Next valid time is further in the future because of
					 * timeperiod constraints. Add a random amount so we
					 * don't get all checks subject to that timeperiod
					 * constraint scheduled at the same time
					 */
					hst->next_check += ranged_urand(0, check_window(hst));
					}
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
int run_async_host_check(host *hst, int check_options, double latency, int scheduled_check, int reschedule_check, int *time_is_valid, time_t *preferred_time) {
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
	if(hst == NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS, 0, "** Running async check of host '%s'...\n", hst->name);

	/* abort if check is already running or was recently completed */
	if (!(check_options & CHECK_OPTION_FORCE_EXECUTION)) {
		if(hst->is_executing == TRUE) {
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
	if(check_host_check_viability(hst, check_options, time_is_valid, preferred_time) == ERROR) {
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

	if(neb_result == NEBERROR_CALLBACKCANCEL || neb_result == NEBERROR_CALLBACKOVERRIDE) {
		log_debug_info(DEBUGL_CHECKS, 0, "Check of host '%s' (id=%u) was %s by a module\n",
		               hst->name, hst->id,
		               neb_result == NEBERROR_CALLBACKCANCEL ? "cancelled" : "overridden");
		}
	/* neb module wants to cancel the host check - the check will be rescheduled for a later time by the scheduling logic */
	if(neb_result == NEBERROR_CALLBACKCANCEL) {
		if(preferred_time)
			*preferred_time += check_window(hst);
		return ERROR;
		}

	/* neb module wants to override the host check - perhaps it will check the host itself */
	/* NOTE: if a module does this, it has to do a lot of the stuff found below to make sure things don't get whacked out of shape! */
	/* NOTE: if would be easier for modules to override checks when the NEBTYPE_SERVICECHECK_INITIATE event is called (later) */
	if(neb_result == NEBERROR_CALLBACKOVERRIDE)
		return OK;
#endif

	log_debug_info(DEBUGL_CHECKS, 0, "Checking host '%s'...\n", hst->name);

	/* clear check options - we don't want old check options retained */
	/* only clear options if this was a scheduled check - on demand check options shouldn't affect retained info */
	if(scheduled_check == TRUE)
		hst->check_options = CHECK_OPTION_NONE;

	/* adjust host check attempt */
	adjust_host_check_attempt(hst, TRUE);

	/* set latency (temporarily) for macros and event broker */
	old_latency = hst->latency;
	hst->latency = latency;

	/* grab the host macro variables */
	memset(&mac, 0, sizeof(mac));
	grab_host_macros_r(&mac, hst);

	/* get the raw command line */
	get_raw_command_line_r(&mac, hst->check_command_ptr, hst->check_command, &raw_command, macro_options);
	if(raw_command == NULL) {
		clear_volatile_macros_r(&mac);
		log_debug_info(DEBUGL_CHECKS, 0, "Raw check command for host '%s' was NULL - aborting.\n", hst->name);
		return ERROR;
		}

	/* process any macros contained in the argument */
	process_macros_r(&mac, raw_command, &processed_command, macro_options);
	my_free(raw_command);
	if(processed_command == NULL) {
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
		my_free(processed_command);
		return OK;
	}
#endif

	/* reset latency (permanent value for this check will get set later) */
	hst->latency = old_latency;

	runchk_result = wproc_run_check(cr, processed_command, &mac);
	if (runchk_result == ERROR) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Unable to send check for host '%s' to worker (ret=%d)\n", hst->name, runchk_result);
	} else {
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



static int get_host_check_return_code(host *temp_host,
		check_result *queued_check_result) {

	int rc;
	char *temp_plugin_output = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "get_host_check_return_code()\n");

	/* get the unprocessed return code */
	/* NOTE: for passive checks, this is the final/processed state */
	rc = queued_check_result->return_code;

	/* adjust return code (active checks only) */
	if(queued_check_result->check_type == CHECK_TYPE_ACTIVE) {
		if(queued_check_result->early_timeout) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Check of host '%s' timed out after %.2lf seconds\n", temp_host->name, temp_host->execution_time);
			my_free(temp_host->plugin_output);
			my_free(temp_host->long_plugin_output);
			my_free(temp_host->perf_data);
			asprintf(&temp_host->plugin_output, "(Host check timed out after %.2lf seconds)", temp_host->execution_time);
			rc = STATE_UNKNOWN;
			}

		/* if there was some error running the command, just skip it (this shouldn't be happening) */
		else if(queued_check_result->exited_ok == FALSE) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning:  Check of host '%s' did not exit properly!\n", temp_host->name);

			my_free(temp_host->plugin_output);
			my_free(temp_host->long_plugin_output);
			my_free(temp_host->perf_data);

			temp_host->plugin_output = (char *)strdup("(Host check did not exit properly)");

			rc = STATE_CRITICAL;
			}

		/* make sure the return code is within bounds */
		else if(queued_check_result->return_code < 0 || queued_check_result->return_code > 3) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Return code of %d for check of host '%s' was out of bounds.%s\n", queued_check_result->return_code, temp_host->name, (queued_check_result->return_code == 126 || queued_check_result->return_code == 127) ? " Make sure the plugin you're trying to run actually exists." : "");

			asprintf(&temp_plugin_output, "(Return code of %d is out of bounds%s : %s)", queued_check_result->return_code, (queued_check_result->return_code == 126 || queued_check_result->return_code == 127) ? " - plugin may be missing" : "", temp_host->plugin_output);
			my_free(temp_host->plugin_output);

			asprintf(&temp_host->plugin_output, "%s)", temp_plugin_output);
			my_free(temp_plugin_output);
			my_free(temp_host->long_plugin_output);
			my_free(temp_host->perf_data);

			rc = STATE_CRITICAL;
			}

		/* a NULL host check command means we should assume the host is UP */
		if(temp_host->check_command == NULL) {
			my_free(temp_host->plugin_output);
			temp_host->plugin_output = (char *)strdup("(Host assumed to be UP)");
			rc = STATE_OK;
			}
		}
	return rc;
	}

/* process results of an asynchronous host check */
int handle_async_host_check_result(host *temp_host, check_result *queued_check_result) {
	time_t current_time;
	int result = STATE_OK;
	int reschedule_check = FALSE;
	char *old_plugin_output = NULL;
	char *temp_ptr = NULL;
	struct timeval start_time_hires;
	struct timeval end_time_hires;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "handle_async_host_check_result(%s ...)\n", temp_host ? temp_host->name : "(NULL host!)");

	/* make sure we have what we need */
	if(temp_host == NULL || queued_check_result == NULL)
		return ERROR;

	time(&current_time);

	log_debug_info(DEBUGL_CHECKS, 1, "** Handling async check result for host '%s' from '%s'...\n", temp_host->name, check_result_source(queued_check_result));

	log_debug_info(DEBUGL_CHECKS, 2, "\tCheck Type:         %s\n", (queued_check_result->check_type == CHECK_TYPE_ACTIVE) ? "Active" : "Passive");
	log_debug_info(DEBUGL_CHECKS, 2, "\tCheck Options:      %d\n", queued_check_result->check_options);
	log_debug_info(DEBUGL_CHECKS, 2, "\tScheduled Check?:   %s\n", (queued_check_result->scheduled_check == TRUE) ? "Yes" : "No");
	log_debug_info(DEBUGL_CHECKS, 2, "\tReschedule Check?:  %s\n", (queued_check_result->reschedule_check == TRUE) ? "Yes" : "No");
	log_debug_info(DEBUGL_CHECKS, 2, "\tExited OK?:         %s\n", (queued_check_result->exited_ok == TRUE) ? "Yes" : "No");
	log_debug_info(DEBUGL_CHECKS, 2, "\tExec Time:          %.3f\n", temp_host->execution_time);
	log_debug_info(DEBUGL_CHECKS, 2, "\tLatency:            %.3f\n", temp_host->latency);
	log_debug_info(DEBUGL_CHECKS, 2, "\tReturn Status:      %d\n", queued_check_result->return_code);
	log_debug_info(DEBUGL_CHECKS, 2, "\tOutput:             %s\n", (queued_check_result == NULL) ? "NULL" : queued_check_result->output);

	/* decrement the number of host checks still out there... */
	if(queued_check_result->check_type == CHECK_TYPE_ACTIVE && currently_running_host_checks > 0)
		currently_running_host_checks--;

	/* skip this host check results if its passive and we aren't accepting passive check results */
	if(queued_check_result->check_type == CHECK_TYPE_PASSIVE) {
		if(accept_passive_host_checks == FALSE) {
			log_debug_info(DEBUGL_CHECKS, 0, "Discarding passive host check result because passive host checks are disabled globally.\n");
			return ERROR;
			}
		if(temp_host->accept_passive_checks == FALSE) {
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
	temp_host->check_type = (queued_check_result->check_type == CHECK_TYPE_ACTIVE) ? CHECK_TYPE_ACTIVE : CHECK_TYPE_PASSIVE;

	/* update check statistics for passive results */
	if(queued_check_result->check_type == CHECK_TYPE_PASSIVE)
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
	if(queued_check_result->check_type == CHECK_TYPE_ACTIVE)
		temp_host->is_executing = FALSE;

	/* get the last check time */
	temp_host->last_check = queued_check_result->start_time.tv_sec;

	/* was this check passive or active? */
	temp_host->check_type = (queued_check_result->check_type == CHECK_TYPE_ACTIVE) ? CHECK_TYPE_ACTIVE : CHECK_TYPE_PASSIVE;

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
	parse_check_output(queued_check_result->output, &temp_host->plugin_output, &temp_host->long_plugin_output, &temp_host->perf_data, TRUE, FALSE);

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

	/* get the check return code */
	result = get_host_check_return_code(temp_host, queued_check_result);

	/* translate return code to basic UP/DOWN state - the DOWN/UNREACHABLE state determination is made later */
	/* NOTE: only do this for active checks - passive check results already have the final state */
	if(queued_check_result->check_type == CHECK_TYPE_ACTIVE) {

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
	process_host_check_result(temp_host, result, old_plugin_output, CHECK_OPTION_NONE, reschedule_check, TRUE, cached_host_check_horizon);

	/* free memory */
	my_free(old_plugin_output);

	log_debug_info(DEBUGL_CHECKS, 1, "** Async check result for host '%s' handled: new state=%d\n", temp_host->name, temp_host->current_state);

	/* high resolution start time for event broker */
	start_time_hires = queued_check_result->start_time;

	/* high resolution end time for event broker */
	gettimeofday(&end_time_hires, NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_host_check(NEBTYPE_HOSTCHECK_PROCESSED, NEBFLAG_NONE, NEBATTR_NONE, temp_host, temp_host->check_type, temp_host->current_state, temp_host->state_type, start_time_hires, end_time_hires, temp_host->check_command, temp_host->latency, temp_host->execution_time, host_check_timeout, queued_check_result->early_timeout, queued_check_result->return_code, NULL, temp_host->plugin_output, temp_host->long_plugin_output, temp_host->perf_data, NULL, queued_check_result);
#endif

	return OK;
	}



/* processes the result of a synchronous or asynchronous host check */
int process_host_check_result(host *hst, int new_state, char *old_plugin_output, int check_options, int reschedule_check, int use_cached_result, unsigned long check_timestamp_horizon) {
	hostsmember *temp_hostsmember = NULL;
	host *child_host = NULL;
	host *parent_host = NULL;
	host *master_host = NULL;
	time_t current_time = 0L;
	time_t next_check = 0L;
	time_t preferred_time = 0L;
	time_t next_valid_time = 0L;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "process_host_check_result()\n");

	log_debug_info(DEBUGL_CHECKS, 1, "HOST: %s, ATTEMPT=%d/%d, CHECK TYPE=%s, STATE TYPE=%s, OLD STATE=%d, NEW STATE=%d\n", hst->name, hst->current_attempt, hst->max_attempts, (hst->check_type == CHECK_TYPE_ACTIVE) ? "ACTIVE" : "PASSIVE", (hst->state_type == HARD_STATE) ? "HARD" : "SOFT", hst->current_state, new_state);

	/* get the current time */
	time(&current_time);

	/* default next check time */
	next_check = current_time + normal_check_window(hst);

	/* we have to adjust current attempt # for passive checks, as it isn't done elsewhere */
	if(hst->check_type == CHECK_TYPE_PASSIVE && passive_host_checks_are_soft == TRUE)
		adjust_host_check_attempt(hst, FALSE);

	/* log passive checks - we need to do this here, as some my bypass external commands by getting dropped in checkresults dir */
	if(hst->check_type == CHECK_TYPE_PASSIVE) {
		if(log_passive_checks == TRUE)
			logit(NSLOG_PASSIVE_CHECK, FALSE, "PASSIVE HOST CHECK: %s;%d;%s\n", hst->name, new_state, hst->plugin_output);
		}


	/******* HOST WAS DOWN/UNREACHABLE INITIALLY *******/
	if(hst->current_state != HOST_UP) {

		log_debug_info(DEBUGL_CHECKS, 1, "Host was %s.\n", host_state_name(hst->current_state));

		/***** HOST IS NOW UP *****/
		/* the host just recovered! */
		if(new_state == HOST_UP) {

			/* set the current state */
			hst->current_state = HOST_UP;

			/* set the state type */
			/* set state type to HARD for passive checks and active checks that were previously in a HARD STATE */
			if(hst->state_type == HARD_STATE || (hst->check_type == CHECK_TYPE_PASSIVE && passive_host_checks_are_soft == FALSE))
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
				parent_host = temp_hostsmember->host_ptr;
				if(parent_host->current_state != HOST_UP) {
					log_debug_info(DEBUGL_CHECKS, 1, "Check of parent host '%s' queued.\n", parent_host->name);
					schedule_host_check(parent_host, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
					}
				}

			/* propagate checks to immediate children if they are not already UP */
			/* we do this because children may currently be UNREACHABLE, but may (as a result of this recovery) switch to UP or DOWN states */
			log_debug_info(DEBUGL_CHECKS, 1, "Propagating checks to child host(s)...\n");
			for(temp_hostsmember = hst->child_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
				child_host = temp_hostsmember->host_ptr;
				if(child_host->current_state != HOST_UP) {
					log_debug_info(DEBUGL_CHECKS, 1, "Check of child host '%s' queued.\n", child_host->name);
					schedule_host_check(child_host, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
					}
				}

			}

		/***** HOST IS STILL DOWN/UNREACHABLE *****/
		/* we're still in a problem state... */
		else {

			log_debug_info(DEBUGL_CHECKS, 1, "Host is still %s.\n", host_state_name(hst->current_state));

			/* passive checks are treated as HARD states by default... */
			if(hst->check_type == CHECK_TYPE_PASSIVE && passive_host_checks_are_soft == FALSE) {

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
                /* 2015-07-23 with the change adjust_host_check_attempt, this can no longer happen
                else if(hst->current_attempt == 1)
					hst->state_type = HARD_STATE;
                */
				/* the host is in a soft state and the check will be retried */
				else
					hst->state_type = SOFT_STATE;
				}

			/* make a determination of the host's state */
			/* translate host state between DOWN/UNREACHABLE (only for passive checks if enabled) */
			hst->current_state = new_state;
			if(hst->check_type == CHECK_TYPE_ACTIVE || translate_passive_host_checks == TRUE)
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

			log_debug_info(DEBUGL_CHECKS, 1, "Host is now %s.\n", host_state_name(hst->current_state));

			/* active and (in some cases) passive check results are treated as SOFT states */
			if(hst->check_type == CHECK_TYPE_ACTIVE || passive_host_checks_are_soft == TRUE) {

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
			if(hst->check_type == CHECK_TYPE_ACTIVE || translate_passive_host_checks == TRUE)
				hst->current_state = determine_host_reachability(hst);

			/* reschedule a check of the host */
			reschedule_check = TRUE;

			/* schedule a re-check of the host at the retry interval because we can't determine its final state yet... */
			if(hst->check_type == CHECK_TYPE_ACTIVE || passive_host_checks_are_soft == TRUE)
				next_check = (unsigned long)(current_time + (hst->retry_interval * interval_length));

			/* schedule a re-check of the host at the normal interval */
			else
				next_check = (unsigned long)(current_time + (hst->check_interval * interval_length));

			/* propagate checks to immediate parents if they are UP */
			/* we do this because a parent host (or grandparent) may have gone down and blocked our route */
			/* checking the parents ASAP will allow us to better determine the final state (DOWN/UNREACHABLE) of this host later */
			log_debug_info(DEBUGL_CHECKS, 1, "Propagating checks to immediate parent hosts that are UP...\n");

			for(temp_hostsmember = hst->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
				parent_host = temp_hostsmember->host_ptr;
				if(parent_host->current_state == HOST_UP) {
					schedule_host_check(parent_host, current_time, CHECK_OPTION_DEPENDENCY_CHECK);
					log_debug_info(DEBUGL_CHECKS, 1, "Check of host '%s' queued.\n", parent_host->name);
					}
				}

			/* propagate checks to immediate children if they are not UNREACHABLE */
			/* we do this because we may now be blocking the route to child hosts */
			log_debug_info(DEBUGL_CHECKS, 1, "Propagating checks to immediate non-UNREACHABLE child hosts...\n");
			for(temp_hostsmember = hst->child_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
				child_host = temp_hostsmember->host_ptr;
				if(child_host->current_state != HOST_UNREACHABLE) {
					log_debug_info(DEBUGL_CHECKS, 1, "Check of child host '%s' queued.\n", child_host->name);
					schedule_host_check(child_host, current_time, CHECK_OPTION_NONE);
					}
				}

			/* check dependencies on second to last host check */
			if(enable_predictive_host_dependency_checks == TRUE && hst->current_attempt == (hst->max_attempts - 1)) {
				objectlist *list;

				/* propagate checks to hosts that THIS ONE depends on for notifications AND execution */
				/* we do to help ensure that the dependency checks are accurate before it comes time to notify */
				log_debug_info(DEBUGL_CHECKS, 1, "Propagating predictive dependency checks to hosts this one depends on...\n");

				for(list = hst->notify_deps; list; list = list->next) {
					hostdependency *dep = (hostdependency *)list->object_ptr;
					if(dep->dependent_host_ptr == hst && dep->master_host_ptr != NULL) {
						master_host = (host *)dep->master_host_ptr;
						log_debug_info(DEBUGL_CHECKS, 1, "Check of host '%s' queued.\n", master_host->name);
						schedule_host_check(master_host, current_time, CHECK_OPTION_NONE);
						}
					}
				for(list = hst->exec_deps; list; list = list->next) {
					hostdependency *dep = (hostdependency *)list->object_ptr;
					if(dep->dependent_host_ptr == hst && dep->master_host_ptr != NULL) {
						master_host = (host *)dep->master_host_ptr;
						log_debug_info(DEBUGL_CHECKS, 1, "Check of host '%s' queued.\n", master_host->name);
						schedule_host_check(master_host, current_time, CHECK_OPTION_NONE);
						}
					}
				}
			}
		}

	log_debug_info(DEBUGL_CHECKS, 1, "Pre-handle_host_state() Host: %s, Attempt=%d/%d, Type=%s, Final State=%d (%s)\n", hst->name, hst->current_attempt, hst->max_attempts, (hst->state_type == HARD_STATE) ? "HARD" : "SOFT", hst->current_state, host_state_name(hst->current_state));

	/* handle the host state */
	handle_host_state(hst);

	log_debug_info(DEBUGL_CHECKS, 1, "Post-handle_host_state() Host: %s, Attempt=%d/%d, Type=%s, Final State=%d (%s)\n", hst->name, hst->current_attempt, hst->max_attempts, (hst->state_type == HARD_STATE) ? "HARD" : "SOFT", hst->current_state, host_state_name(hst->current_state));


	/******************** POST-PROCESSING STUFF *********************/

	/* if the plugin output differs from previous check and no state change, log the current state/output if state stalking is enabled */
	if(hst->last_state == hst->current_state && should_stalk(hst) && compare_strings(old_plugin_output, hst->plugin_output)) {
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
		if(next_valid_time > preferred_time) {
			/* Next valid time is further in the future because of timeperiod
			 * constraints. Add a random amount so we don't get all checks
			 * subject to that timeperiod constraint scheduled at the same time
			 */
			hst->next_check += ranged_urand(0, check_window(hst));
			}

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

	return OK;
	}



/* checks viability of performing a host check */
int check_host_check_viability(host *hst, int check_options, int *time_is_valid, time_t *new_time) {
	int result = OK;
	int perform_check = TRUE;
	time_t current_time = 0L;
	time_t preferred_time = 0L;
	int check_interval = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_host_check_viability()\n");

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
			log_debug_info(DEBUGL_CHECKS, 0, "Timeperiod check failed\n");
			preferred_time = current_time;
			if(time_is_valid)
				*time_is_valid = FALSE;
			perform_check = FALSE;
			}

		/* check host dependencies for execution */
		if(check_host_dependencies(hst, EXECUTION_DEPENDENCY) == DEPENDENCIES_FAILED) {
			log_debug_info(DEBUGL_CHECKS, 0, "Host check dependencies failed\n");
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
int adjust_host_check_attempt(host *hst, int is_active) {

	log_debug_info(DEBUGL_FUNCTIONS, 0, "adjust_host_check_attempt()\n");

	if(hst == NULL)
		return ERROR;

	log_debug_info(DEBUGL_CHECKS, 2, "Adjusting check attempt number for host '%s': current attempt=%d/%d, state=%d, state type=%d\n", hst->name, hst->current_attempt, hst->max_attempts, hst->current_state, hst->state_type);

	/* if host is in a hard state, reset current attempt number */
    /* 2015-07-23 only reset current_attempt if host is up */
	if(hst->state_type == HARD_STATE && hst->current_state == HOST_UP)
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
	host *parent_host = NULL;
	hostsmember *temp_hostsmember = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "determine_host_reachability(host=%s)\n", hst ? hst->name : "(NULL host!)");

	if(hst == NULL)
		return HOST_DOWN;

	log_debug_info(DEBUGL_CHECKS, 2, "Determining state of host '%s': current state=%d (%s)\n", hst->name, hst->current_state, host_state_name(hst->current_state));

	/* host is UP - no translation needed */
	if(hst->current_state == HOST_UP) {
		log_debug_info(DEBUGL_CHECKS, 2, "Host is UP, no state translation needed.\n");
		return HOST_UP;
		}

	/* host has no parents, so it is DOWN */
	if(hst->check_type == CHECK_TYPE_PASSIVE && hst->current_state == HOST_UNREACHABLE) {
		log_debug_info(DEBUGL_CHECKS, 2, "Passive check so keep it UNREACHABLE.\n");
		return HOST_UNREACHABLE;
		}
	else if(hst->parent_hosts == NULL) {
		log_debug_info(DEBUGL_CHECKS, 2, "Host has no parents, so it is DOWN.\n");
		return HOST_DOWN;
		}

	/* check all parent hosts to see if we're DOWN or UNREACHABLE */
	else {
		for(temp_hostsmember = hst->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
			parent_host = temp_hostsmember->host_ptr;
			log_debug_info(DEBUGL_CHECKS, 2, "   Parent '%s' is %s\n", parent_host->name, host_state_name(parent_host->current_state));
			/* bail out as soon as we find one parent host that is UP */
			if(parent_host->current_state == HOST_UP) {
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

	/* record the time the last state ended */
	switch(hst->last_state) {
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
		hst->last_notification = (time_t)0;
		hst->next_notification = (time_t)0;

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
		/* It can start on soft states */
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
			hst->notified_on = 0;
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


/* Parses raw plugin output and returns: short and long output, perf data. */
int parse_check_output(char *buf, char **short_output, char **long_output, char **perf_data, int escape_newlines_please, int newlines_are_escaped) {
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
	if (short_output)
		*short_output = NULL;
	if (long_output)
		*long_output = NULL;
	if (perf_data)
		*perf_data = NULL;

	/* No input provided or no output requested, nothing to do. */
	if (!buf || !*buf || (!short_output && !long_output && !perf_data))
		return OK;


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
			else
				buf[y++] = buf[x];
			}
		buf[y] = '\0';
		}

	/* Process each line of input. */
	for (x = 0; !eof && buf[0]; x++) {

		/* Continue on until we reach the end of a line (or input). */
		if (buf[x] == '\n')
			buf[x] = '\0';
		else if (buf[x] == '\0')
			eof = TRUE;
		else
			continue;

		/* Handle this line of input. */
		current_line++;

		/* The first line contains short plugin output and optional perf data. */
		if (current_line == 1) {

			/* Get the short plugin output. If buf[0] is '|', strtok() will
			 * return buf+1 or NULL if buf[1] is '\0'. We use my_strtok()
			 * instead which returns a pointer to '\0' in this case. */
			if ((ptr = my_strtok(buf, "|"))) {
				if (short_output) {
					strip(ptr); /* Remove leading and trailing whitespace. */
					*short_output = strdup(ptr);
					}

				/* Get the optional perf data. */
				if ((ptr = my_strtok(NULL, "\n")))
					dbuf_strcat(&perf_text, ptr);
				}

			}
		/* Additional lines contain long plugin output and optional perf data.
		 * Once we've hit perf data, the rest of the output is perf data. */
		else if (in_perf_data) {
			if (perf_text.buf && *perf_text.buf)
				dbuf_strcat(&perf_text, " ");
			dbuf_strcat(&perf_text, buf);

			}
		/* Look for the perf data separator. */
		else if (strchr(buf, '|')) {
			in_perf_data = TRUE;

			if ((ptr = my_strtok(buf, "|"))) {

				/* Get the remaining long plugin output. */
				if (current_line > 2)
					dbuf_strcat(&long_text, "\n");
				dbuf_strcat(&long_text, ptr);

				/* Get the perf data. */
				if ((ptr = my_strtok(NULL, "\n"))) {
					if (perf_text.buf && *perf_text.buf)
						dbuf_strcat(&perf_text, " ");
					dbuf_strcat(&perf_text, ptr);
					}
				}

			}
		/* Otherwise it's still just long output. */
		else {
			if (current_line > 2)
				dbuf_strcat(&long_text, "\n");
			dbuf_strcat(&long_text, buf);
			}

		/* Point buf to the start of the next line. *(buf+x+1) will be a valid
		 * memory reference on our next iteration or we are at the end of input
		 * (eof == TRUE) and *(buf+x+1) will never be referenced. */
		buf += x + 1;
		x = -1; /* x will be incremented to 0 by the loop update. */
		}

	/* Save long output. */
	if (long_output && long_text.buf && *long_text.buf) {
		/* Escape newlines (and backslashes) in long output if requested. */
		if (escape_newlines_please)
			*long_output = escape_newlines(long_text.buf);
		else
			*long_output = strdup(long_text.buf);
		}

	/* Save perf data. */
	if (perf_data && perf_text.buf && *perf_text.buf) {
		strip(perf_text.buf); /* Remove leading and trailing whitespace. */
		*perf_data = strdup(perf_text.buf);
		}

	/* free dynamic buffers */
	dbuf_free(&long_text);
	dbuf_free(&perf_text);

	return OK;
	}
