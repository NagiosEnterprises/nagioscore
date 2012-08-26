/*****************************************************************************
 *
 * EVENTS.C - Timed event functions for Nagios
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
#include "../include/common.h"
#include "../include/downtime.h"
#include "../include/comments.h"
#include "../include/statusdata.h"
#include "../include/nagios.h"
#include "../include/broker.h"
#include "../include/sretention.h"
#include "../lib/squeue.h"


extern int      test_scheduling;

extern time_t   program_start;
extern time_t   event_start;

extern int      sigshutdown;
extern int      sigrestart;

extern int      interval_length;
extern int      service_inter_check_delay_method;
extern int      host_inter_check_delay_method;
extern int      service_interleave_factor_method;
extern int      max_host_check_spread;
extern int      max_service_check_spread;

extern int      check_reaper_interval;
extern int      service_freshness_check_interval;
extern int      host_freshness_check_interval;
extern int      auto_rescheduling_interval;

extern int      check_orphaned_services;
extern int      check_orphaned_hosts;
extern int      check_service_freshness;
extern int      check_host_freshness;
extern int      auto_reschedule_checks;

extern int      retain_state_information;
extern int      retention_update_interval;

extern int      max_parallel_service_checks;
extern int      currently_running_service_checks;

extern int      aggregate_status_updates;
extern int      status_update_interval;

extern int      log_rotation_method;

extern int      execute_service_checks;
extern int      execute_host_checks;

extern int      time_change_threshold;

squeue_t *nagios_squeue = NULL; /* our scheduling queue */

sched_info scheduling_info;
extern iobroker_set *nagios_iobs;



/******************************************************************/
/************ EVENT SCHEDULING/HANDLING FUNCTIONS *****************/
/******************************************************************/


/* initialize the event timing loop before we start monitoring */
void init_timing_loop(void) {
	host *temp_host = NULL;
	service *temp_service = NULL;
	time_t current_time = 0L;
	int total_interleave_blocks = 0;
	int current_interleave_block = 1;
	int interleave_block_index = 0;
	int mult_factor = 0;
	int is_valid_time = 0;
	time_t next_valid_time = 0L;
	int schedule_check = 0;
	double max_inter_check_delay = 0.0;
	struct timeval tv[9];
	double runtime[9];

	log_debug_info(DEBUGL_FUNCTIONS, 0, "init_timing_loop() start\n");

	/* get the time right now */
	time(&current_time);


	/******** GET BASIC HOST/SERVICE INFO  ********/

	scheduling_info.total_services = 0;
	scheduling_info.total_scheduled_services = 0;
	scheduling_info.total_hosts = 0;
	scheduling_info.total_scheduled_hosts = 0;
	scheduling_info.average_services_per_host = 0.0;
	scheduling_info.average_scheduled_services_per_host = 0.0;
	scheduling_info.average_service_execution_time = 0.0;
	scheduling_info.service_check_interval_total = 0;
	scheduling_info.average_service_inter_check_delay = 0.0;
	scheduling_info.host_check_interval_total = 0;
	scheduling_info.average_host_inter_check_delay = 0.0;

	if(test_scheduling == TRUE)
		gettimeofday(&tv[0], NULL);

	/* get info on service checks to be scheduled */
	for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

		schedule_check = TRUE;

		/* service has no check interval */
		if(temp_service->check_interval == 0)
			schedule_check = FALSE;

		/* active checks are disabled */
		if(temp_service->checks_enabled == FALSE)
			schedule_check = FALSE;

		/* are there any valid times this service can be checked? */
		is_valid_time = check_time_against_period(current_time, temp_service->check_period_ptr);
		if(is_valid_time == ERROR) {
			get_next_valid_time(current_time, &next_valid_time, temp_service->check_period_ptr);
			if(current_time == next_valid_time)
				schedule_check = FALSE;
			}

		if(schedule_check == TRUE) {

			scheduling_info.total_scheduled_services++;

			/* used later in inter-check delay calculations */
			scheduling_info.service_check_interval_total += temp_service->check_interval;

			/* calculate rolling average execution time (available from retained state information) */
			scheduling_info.average_service_execution_time = (double)(((scheduling_info.average_service_execution_time * (scheduling_info.total_scheduled_services - 1)) + temp_service->execution_time) / (double)scheduling_info.total_scheduled_services);
			}
		else {
			temp_service->should_be_scheduled = FALSE;

			log_debug_info(DEBUGL_EVENTS, 1, "Service '%s' on host '%s' should not be scheduled.\n", temp_service->description, temp_service->host_name);
			}

		scheduling_info.total_services++;
		}

	if(test_scheduling == TRUE)
		gettimeofday(&tv[1], NULL);

	/* get info on host checks to be scheduled */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		schedule_check = TRUE;

		/* host has no check interval */
		if(temp_host->check_interval == 0)
			schedule_check = FALSE;

		/* active checks are disabled */
		if(temp_host->checks_enabled == FALSE)
			schedule_check = FALSE;

		/* are there any valid times this host can be checked? */
		is_valid_time = check_time_against_period(current_time, temp_host->check_period_ptr);
		if(is_valid_time == ERROR) {
			get_next_valid_time(current_time, &next_valid_time, temp_host->check_period_ptr);
			if(current_time == next_valid_time)
				schedule_check = FALSE;
			}

		if(schedule_check == TRUE) {

			scheduling_info.total_scheduled_hosts++;

			/* this is used later in inter-check delay calculations */
			scheduling_info.host_check_interval_total += temp_host->check_interval;
			}
		else {
			temp_host->should_be_scheduled = FALSE;

			log_debug_info(DEBUGL_EVENTS, 1, "Host '%s' should not be scheduled.\n", temp_host->name);
			}

		scheduling_info.total_hosts++;
		}

	if(test_scheduling == TRUE)
		gettimeofday(&tv[2], NULL);

	scheduling_info.average_services_per_host = (double)((double)scheduling_info.total_services / (double)scheduling_info.total_hosts);
	scheduling_info.average_scheduled_services_per_host = (double)((double)scheduling_info.total_scheduled_services / (double)scheduling_info.total_hosts);

	/* adjust the check interval total to correspond to the interval length */
	scheduling_info.service_check_interval_total = (scheduling_info.service_check_interval_total * interval_length);

	/* calculate the average check interval for services */
	scheduling_info.average_service_check_interval = (double)((double)scheduling_info.service_check_interval_total / (double)scheduling_info.total_scheduled_services);


	/******** DETERMINE SERVICE SCHEDULING PARAMS  ********/

	log_debug_info(DEBUGL_EVENTS, 2, "Determining service scheduling parameters...");

	/* default max service check spread (in minutes) */
	scheduling_info.max_service_check_spread = max_service_check_spread;

	/* how should we determine the service inter-check delay to use? */
	switch(service_inter_check_delay_method) {

		case ICD_NONE:

			/* don't spread checks out - useful for testing parallelization code */
			scheduling_info.service_inter_check_delay = 0.0;
			break;

		case ICD_DUMB:

			/* be dumb and just schedule checks 1 second apart */
			scheduling_info.service_inter_check_delay = 1.0;
			break;

		case ICD_USER:

			/* the user specified a delay, so don't try to calculate one */
			break;

		case ICD_SMART:
		default:

			/* be smart and calculate the best delay to use to minimize local load... */
			if(scheduling_info.total_scheduled_services > 0 && scheduling_info.service_check_interval_total > 0) {

				/* calculate the average inter check delay (in seconds) needed to evenly space the service checks out */
				scheduling_info.average_service_inter_check_delay = (double)(scheduling_info.average_service_check_interval / (double)scheduling_info.total_scheduled_services);

				/* set the global inter check delay value */
				scheduling_info.service_inter_check_delay = scheduling_info.average_service_inter_check_delay;

				/* calculate max inter check delay and see if we should use that instead */
				max_inter_check_delay = (double)((scheduling_info.max_service_check_spread * 60.0) / (double)scheduling_info.total_scheduled_services);
				if(scheduling_info.service_inter_check_delay > max_inter_check_delay)
					scheduling_info.service_inter_check_delay = max_inter_check_delay;
				}
			else
				scheduling_info.service_inter_check_delay = 0.0;

			log_debug_info(DEBUGL_EVENTS, 1, "Total scheduled service checks:  %d\n", scheduling_info.total_scheduled_services);
			log_debug_info(DEBUGL_EVENTS, 1, "Average service check interval:  %0.2f sec\n", scheduling_info.average_service_check_interval);
			log_debug_info(DEBUGL_EVENTS, 1, "Service inter-check delay:       %0.2f sec\n", scheduling_info.service_inter_check_delay);
		}

	/* how should we determine the service interleave factor? */
	switch(service_interleave_factor_method) {

		case ILF_USER:

			/* the user supplied a value, so don't do any calculation */
			break;

		case ILF_SMART:
		default:

			/* protect against a divide by zero problem - shouldn't happen, but just in case... */
			if(scheduling_info.total_hosts == 0)
				scheduling_info.total_hosts = 1;

			scheduling_info.service_interleave_factor = (int)(ceil(scheduling_info.average_scheduled_services_per_host));

			log_debug_info(DEBUGL_EVENTS, 1, "Total scheduled service checks: %d\n", scheduling_info.total_scheduled_services);
			log_debug_info(DEBUGL_EVENTS, 1, "Total hosts:                    %d\n", scheduling_info.total_hosts);
			log_debug_info(DEBUGL_EVENTS, 1, "Service Interleave factor:      %d\n", scheduling_info.service_interleave_factor);
		}

	/* calculate number of service interleave blocks */
	if(scheduling_info.service_interleave_factor == 0)
		total_interleave_blocks = scheduling_info.total_scheduled_services;
	else
		total_interleave_blocks = (int)ceil((double)scheduling_info.total_scheduled_services / (double)scheduling_info.service_interleave_factor);

	scheduling_info.first_service_check = (time_t)0L;
	scheduling_info.last_service_check = (time_t)0L;

	log_debug_info(DEBUGL_EVENTS, 1, "Total scheduled services: %d\n", scheduling_info.total_scheduled_services);
	log_debug_info(DEBUGL_EVENTS, 1, "Service Interleave factor: %d\n", scheduling_info.service_interleave_factor);
	log_debug_info(DEBUGL_EVENTS, 1, "Total service interleave blocks: %d\n", total_interleave_blocks);
	log_debug_info(DEBUGL_EVENTS, 1, "Service inter-check delay: %2.1f\n", scheduling_info.service_inter_check_delay);


	if(test_scheduling == TRUE)
		gettimeofday(&tv[3], NULL);

	/******** SCHEDULE SERVICE CHECKS  ********/

	log_debug_info(DEBUGL_EVENTS, 2, "Scheduling service checks...");

	/* determine check times for service checks (with interleaving to minimize remote load) */
	current_interleave_block = 0;
	for(temp_service = service_list; temp_service != NULL && scheduling_info.service_interleave_factor > 0;) {

		log_debug_info(DEBUGL_EVENTS, 2, "Current Interleave Block: %d\n", current_interleave_block);

		for(interleave_block_index = 0; interleave_block_index < scheduling_info.service_interleave_factor && temp_service != NULL; temp_service = temp_service->next) {
			int check_delay = 0;

			log_debug_info(DEBUGL_EVENTS, 2, "Service '%s' on host '%s'\n", temp_service->description, temp_service->host_name);
			/* skip this service if it shouldn't be scheduled */
			if(temp_service->should_be_scheduled == FALSE) {
				log_debug_info(DEBUGL_EVENTS, 2, "Service check should not be scheduled.\n");
				continue;
				}

			/*
			 * skip services that are already scheduled for the (near)
			 * future from retention data, but reschedule ones that
			 * were supposed to happen while we weren't running...
			 * We check to make sure the check isn't scheduled to run
			 * far in the future to make sure checks who've hade their
			 * timeperiods changed during the restart aren't left
			 * hanging too long without being run.
			 */
			check_delay = temp_service->next_check - current_time;
			if(check_delay > 0 && check_delay < (temp_service->check_interval * interval_length)) {
				log_debug_info(DEBUGL_EVENTS, 2, "Service is already scheduled to be checked in the future: %s\n", ctime(&temp_service->next_check));
				continue;
				}

			/* interleave block index should only be increased when we find a schedulable service */
			/* moved from for() loop 11/05/05 EG */
			interleave_block_index++;

			mult_factor = current_interleave_block + (interleave_block_index * total_interleave_blocks);

			log_debug_info(DEBUGL_EVENTS, 2, "CIB: %d, IBI: %d, TIB: %d, SIF: %d\n", current_interleave_block, interleave_block_index, total_interleave_blocks, scheduling_info.service_interleave_factor);
			log_debug_info(DEBUGL_EVENTS, 2, "Mult factor: %d\n", mult_factor);

			/* set the preferred next check time for the service */
			temp_service->next_check = (time_t)(current_time + (mult_factor * scheduling_info.service_inter_check_delay));

			log_debug_info(DEBUGL_EVENTS, 2, "Preferred Check Time: %lu --> %s", (unsigned long)temp_service->next_check, ctime(&temp_service->next_check));


			/* make sure the service can actually be scheduled when we want */
			is_valid_time = check_time_against_period(temp_service->next_check, temp_service->check_period_ptr);
			if(is_valid_time == ERROR) {
				log_debug_info(DEBUGL_EVENTS, 2, "Preferred Time is Invalid In Timeperiod '%s': %lu --> %s", temp_service->check_period_ptr->name, (unsigned long)temp_service->next_check, ctime(&temp_service->next_check));
				get_next_valid_time(temp_service->next_check, &next_valid_time, temp_service->check_period_ptr);
				temp_service->next_check = next_valid_time;
				}

			log_debug_info(DEBUGL_EVENTS, 2, "Actual Check Time: %lu --> %s", (unsigned long)temp_service->next_check, ctime(&temp_service->next_check));

			if(scheduling_info.first_service_check == (time_t)0 || (temp_service->next_check < scheduling_info.first_service_check))
				scheduling_info.first_service_check = temp_service->next_check;
			if(temp_service->next_check > scheduling_info.last_service_check)
				scheduling_info.last_service_check = temp_service->next_check;
			}

		current_interleave_block++;
		}

	if(test_scheduling == TRUE)
		gettimeofday(&tv[4], NULL);

	/* add scheduled service checks to event queue */
	for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

		/* Nagios XI/NDOUtils MOD */
		/* update status of all services (scheduled or not) */
		update_service_status(temp_service, FALSE);

		/* skip most services that shouldn't be scheduled */
		if(temp_service->should_be_scheduled == FALSE) {

			/* passive checks are an exception if a forced check was scheduled before Nagios was restarted */
			if(!(temp_service->checks_enabled == FALSE && temp_service->next_check != (time_t)0L && (temp_service->check_options & CHECK_OPTION_FORCE_EXECUTION)))
				continue;
			}

		/* create a new service check event */
		schedule_new_event(EVENT_SERVICE_CHECK, FALSE, temp_service->next_check, FALSE, 0, NULL, TRUE, (void *)temp_service, NULL, temp_service->check_options);
		}


	if(test_scheduling == TRUE)
		gettimeofday(&tv[5], NULL);

	/******** DETERMINE HOST SCHEDULING PARAMS  ********/

	log_debug_info(DEBUGL_EVENTS, 2, "Determining host scheduling parameters...");

	scheduling_info.first_host_check = (time_t)0L;
	scheduling_info.last_host_check = (time_t)0L;

	/* default max host check spread (in minutes) */
	scheduling_info.max_host_check_spread = max_host_check_spread;

	/* how should we determine the host inter-check delay to use? */
	switch(host_inter_check_delay_method) {

		case ICD_NONE:

			/* don't spread checks out */
			scheduling_info.host_inter_check_delay = 0.0;
			break;

		case ICD_DUMB:

			/* be dumb and just schedule checks 1 second apart */
			scheduling_info.host_inter_check_delay = 1.0;
			break;

		case ICD_USER:

			/* the user specified a delay, so don't try to calculate one */
			break;

		case ICD_SMART:
		default:

			/* be smart and calculate the best delay to use to minimize local load... */
			if(scheduling_info.total_scheduled_hosts > 0 && scheduling_info.host_check_interval_total > 0) {

				/* adjust the check interval total to correspond to the interval length */
				scheduling_info.host_check_interval_total = (scheduling_info.host_check_interval_total * interval_length);

				/* calculate the average check interval for hosts */
				scheduling_info.average_host_check_interval = (double)((double)scheduling_info.host_check_interval_total / (double)scheduling_info.total_scheduled_hosts);

				/* calculate the average inter check delay (in seconds) needed to evenly space the host checks out */
				scheduling_info.average_host_inter_check_delay = (double)(scheduling_info.average_host_check_interval / (double)scheduling_info.total_scheduled_hosts);

				/* set the global inter check delay value */
				scheduling_info.host_inter_check_delay = scheduling_info.average_host_inter_check_delay;

				/* calculate max inter check delay and see if we should use that instead */
				max_inter_check_delay = (double)((scheduling_info.max_host_check_spread * 60.0) / (double)scheduling_info.total_scheduled_hosts);
				if(scheduling_info.host_inter_check_delay > max_inter_check_delay)
					scheduling_info.host_inter_check_delay = max_inter_check_delay;
				}
			else
				scheduling_info.host_inter_check_delay = 0.0;

			log_debug_info(DEBUGL_EVENTS, 2, "Total scheduled host checks:  %d\n", scheduling_info.total_scheduled_hosts);
			log_debug_info(DEBUGL_EVENTS, 2, "Host check interval total:    %lu\n", scheduling_info.host_check_interval_total);
			log_debug_info(DEBUGL_EVENTS, 2, "Average host check interval:  %0.2f sec\n", scheduling_info.average_host_check_interval);
			log_debug_info(DEBUGL_EVENTS, 2, "Host inter-check delay:       %0.2f sec\n", scheduling_info.host_inter_check_delay);
		}

	if(test_scheduling == TRUE)
		gettimeofday(&tv[6], NULL);


	/******** SCHEDULE HOST CHECKS  ********/

	log_debug_info(DEBUGL_EVENTS, 2, "Scheduling host checks...");

	/* determine check times for host checks */
	mult_factor = 0;
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		log_debug_info(DEBUGL_EVENTS, 2, "Host '%s'\n", temp_host->name);

		/* skip hosts that shouldn't be scheduled */
		if(temp_host->should_be_scheduled == FALSE) {
			log_debug_info(DEBUGL_EVENTS, 2, "Host check should not be scheduled.\n");
			continue;
			}

		/* skip hosts that are already scheduled for the future (from retention data), but reschedule ones that were supposed to be checked before we started */
		if(temp_host->next_check > current_time) {
			log_debug_info(DEBUGL_EVENTS, 2, "Host is already scheduled to be checked in the future: %s\n", ctime(&temp_host->next_check));
			continue;
			}

		/* calculate preferred host check time */
		temp_host->next_check = (time_t)(current_time + (mult_factor * scheduling_info.host_inter_check_delay));

		log_debug_info(DEBUGL_EVENTS, 2, "Preferred Check Time: %lu --> %s", (unsigned long)temp_host->next_check, ctime(&temp_host->next_check));

		/* make sure the host can actually be scheduled at this time */
		is_valid_time = check_time_against_period(temp_host->next_check, temp_host->check_period_ptr);
		if(is_valid_time == ERROR) {
			get_next_valid_time(temp_host->next_check, &next_valid_time, temp_host->check_period_ptr);
			temp_host->next_check = next_valid_time;
			}

		log_debug_info(DEBUGL_EVENTS, 2, "Actual Check Time: %lu --> %s", (unsigned long)temp_host->next_check, ctime(&temp_host->next_check));

		if(scheduling_info.first_host_check == (time_t)0 || (temp_host->next_check < scheduling_info.first_host_check))
			scheduling_info.first_host_check = temp_host->next_check;
		if(temp_host->next_check > scheduling_info.last_host_check)
			scheduling_info.last_host_check = temp_host->next_check;

		mult_factor++;
		}

	if(test_scheduling == TRUE)
		gettimeofday(&tv[7], NULL);

	/* add scheduled host checks to event queue */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* Nagios XI/NDOUtils Mod */
		/* update status of all hosts (scheduled or not) */
		update_host_status(temp_host, FALSE);

		/* skip most hosts that shouldn't be scheduled */
		if(temp_host->should_be_scheduled == FALSE) {

			/* passive checks are an exception if a forced check was scheduled before Nagios was restarted */
			if(!(temp_host->checks_enabled == FALSE && temp_host->next_check != (time_t)0L && (temp_host->check_options & CHECK_OPTION_FORCE_EXECUTION)))
				continue;
			}

		/* schedule a new host check event */
		schedule_new_event(EVENT_HOST_CHECK, FALSE, temp_host->next_check, FALSE, 0, NULL, TRUE, (void *)temp_host, NULL, temp_host->check_options);
		}

	if(test_scheduling == TRUE)
		gettimeofday(&tv[8], NULL);


	/******** SCHEDULE MISC EVENTS ********/

	/* add a host and service check rescheduling event */
	if(auto_reschedule_checks == TRUE)
		schedule_new_event(EVENT_RESCHEDULE_CHECKS, TRUE, current_time + auto_rescheduling_interval, TRUE, auto_rescheduling_interval, NULL, TRUE, NULL, NULL, 0);

	/* add a check result reaper event */
	schedule_new_event(EVENT_CHECK_REAPER, TRUE, current_time + check_reaper_interval, TRUE, check_reaper_interval, NULL, TRUE, NULL, NULL, 0);

	/* add an orphaned check event */
	if(check_orphaned_services == TRUE || check_orphaned_hosts == TRUE)
		schedule_new_event(EVENT_ORPHAN_CHECK, TRUE, current_time + DEFAULT_ORPHAN_CHECK_INTERVAL, TRUE, DEFAULT_ORPHAN_CHECK_INTERVAL, NULL, TRUE, NULL, NULL, 0);

	/* add a service result "freshness" check event */
	if(check_service_freshness == TRUE)
		schedule_new_event(EVENT_SFRESHNESS_CHECK, TRUE, current_time + service_freshness_check_interval, TRUE, service_freshness_check_interval, NULL, TRUE, NULL, NULL, 0);

	/* add a host result "freshness" check event */
	if(check_host_freshness == TRUE)
		schedule_new_event(EVENT_HFRESHNESS_CHECK, TRUE, current_time + host_freshness_check_interval, TRUE, host_freshness_check_interval, NULL, TRUE, NULL, NULL, 0);

	/* add a status save event */
	if(aggregate_status_updates == TRUE)
		schedule_new_event(EVENT_STATUS_SAVE, TRUE, current_time + status_update_interval, TRUE, status_update_interval, NULL, TRUE, NULL, NULL, 0);

	/* add a log rotation event if necessary */
	if(log_rotation_method != LOG_ROTATION_NONE)
		schedule_new_event(EVENT_LOG_ROTATION, TRUE, get_next_log_rotation_time(), TRUE, 0, (void *)get_next_log_rotation_time, TRUE, NULL, NULL, 0);

	/* add a retention data save event if needed */
	if(retain_state_information == TRUE && retention_update_interval > 0)
		schedule_new_event(EVENT_RETENTION_SAVE, TRUE, current_time + (retention_update_interval * 60), TRUE, (retention_update_interval * 60), NULL, TRUE, NULL, NULL, 0);

	if(test_scheduling == TRUE) {

		runtime[0] = (double)((double)(tv[1].tv_sec - tv[0].tv_sec) + (double)((tv[1].tv_usec - tv[0].tv_usec) / 1000.0) / 1000.0);
		runtime[1] = (double)((double)(tv[2].tv_sec - tv[1].tv_sec) + (double)((tv[2].tv_usec - tv[1].tv_usec) / 1000.0) / 1000.0);
		runtime[2] = (double)((double)(tv[3].tv_sec - tv[2].tv_sec) + (double)((tv[3].tv_usec - tv[2].tv_usec) / 1000.0) / 1000.0);
		runtime[3] = (double)((double)(tv[4].tv_sec - tv[3].tv_sec) + (double)((tv[4].tv_usec - tv[3].tv_usec) / 1000.0) / 1000.0);
		runtime[4] = (double)((double)(tv[5].tv_sec - tv[4].tv_sec) + (double)((tv[5].tv_usec - tv[4].tv_usec) / 1000.0) / 1000.0);
		runtime[5] = (double)((double)(tv[6].tv_sec - tv[5].tv_sec) + (double)((tv[6].tv_usec - tv[5].tv_usec) / 1000.0) / 1000.0);
		runtime[6] = (double)((double)(tv[7].tv_sec - tv[6].tv_sec) + (double)((tv[7].tv_usec - tv[6].tv_usec) / 1000.0) / 1000.0);
		runtime[7] = (double)((double)(tv[8].tv_sec - tv[7].tv_sec) + (double)((tv[8].tv_usec - tv[7].tv_usec) / 1000.0) / 1000.0);

		runtime[8] = (double)((double)(tv[8].tv_sec - tv[0].tv_sec) + (double)((tv[8].tv_usec - tv[0].tv_usec) / 1000.0) / 1000.0);

		printf("EVENT SCHEDULING TIMES\n");
		printf("-------------------------------------\n");
		printf("Get service info:        %.6lf sec\n", runtime[0]);
		printf("Get host info info:      %.6lf sec\n", runtime[1]);
		printf("Get service params:      %.6lf sec\n", runtime[2]);
		printf("Schedule service times:  %.6lf sec\n", runtime[3]);
		printf("Schedule service events: %.6lf sec\n", runtime[4]);
		printf("Get host params:         %.6lf sec\n", runtime[5]);
		printf("Schedule host times:     %.6lf sec\n", runtime[6]);
		printf("Schedule host events:    %.6lf sec\n", runtime[7]);
		printf("                         ============\n");
		printf("TOTAL:                   %.6lf sec\n", runtime[8]);
		printf("\n\n");
		}

	log_debug_info(DEBUGL_FUNCTIONS, 0, "init_timing_loop() end\n");

	return;
	}



/* displays service check scheduling information */
void display_scheduling_info(void) {
	float minimum_concurrent_checks1 = 0.0;
	float minimum_concurrent_checks2 = 0.0;
	float minimum_concurrent_checks = 0.0;
	float max_reaper_interval = 0.0;
	int suggestions = 0;

	printf("Projected scheduling information for host and service checks\n");
	printf("is listed below.  This information assumes that you are going\n");
	printf("to start running Nagios with your current config files.\n\n");

	printf("HOST SCHEDULING INFORMATION\n");
	printf("---------------------------\n");
	printf("Total hosts:                     %d\n", scheduling_info.total_hosts);
	printf("Total scheduled hosts:           %d\n", scheduling_info.total_scheduled_hosts);

	printf("Host inter-check delay method:   ");
	if(host_inter_check_delay_method == ICD_NONE)
		printf("NONE\n");
	else if(host_inter_check_delay_method == ICD_DUMB)
		printf("DUMB\n");
	else if(host_inter_check_delay_method == ICD_SMART) {
		printf("SMART\n");
		printf("Average host check interval:     %.2f sec\n", scheduling_info.average_host_check_interval);
		}
	else
		printf("USER-SUPPLIED VALUE\n");
	printf("Host inter-check delay:          %.2f sec\n", scheduling_info.host_inter_check_delay);
	printf("Max host check spread:           %d min\n", scheduling_info.max_host_check_spread);
	printf("First scheduled check:           %s", (scheduling_info.total_scheduled_hosts == 0) ? "N/A\n" : ctime(&scheduling_info.first_host_check));
	printf("Last scheduled check:            %s", (scheduling_info.total_scheduled_hosts == 0) ? "N/A\n" : ctime(&scheduling_info.last_host_check));
	printf("\n\n");

	printf("SERVICE SCHEDULING INFORMATION\n");
	printf("-------------------------------\n");
	printf("Total services:                     %d\n", scheduling_info.total_services);
	printf("Total scheduled services:           %d\n", scheduling_info.total_scheduled_services);

	printf("Service inter-check delay method:   ");
	if(service_inter_check_delay_method == ICD_NONE)
		printf("NONE\n");
	else if(service_inter_check_delay_method == ICD_DUMB)
		printf("DUMB\n");
	else if(service_inter_check_delay_method == ICD_SMART) {
		printf("SMART\n");
		printf("Average service check interval:     %.2f sec\n", scheduling_info.average_service_check_interval);
		}
	else
		printf("USER-SUPPLIED VALUE\n");
	printf("Inter-check delay:                  %.2f sec\n", scheduling_info.service_inter_check_delay);

	printf("Interleave factor method:           %s\n", (service_interleave_factor_method == ILF_USER) ? "USER-SUPPLIED VALUE" : "SMART");
	if(service_interleave_factor_method == ILF_SMART)
		printf("Average services per host:          %.2f\n", scheduling_info.average_services_per_host);
	printf("Service interleave factor:          %d\n", scheduling_info.service_interleave_factor);

	printf("Max service check spread:           %d min\n", scheduling_info.max_service_check_spread);
	printf("First scheduled check:              %s", ctime(&scheduling_info.first_service_check));
	printf("Last scheduled check:               %s", ctime(&scheduling_info.last_service_check));
	printf("\n\n");

	printf("CHECK PROCESSING INFORMATION\n");
	printf("----------------------------\n");
	printf("Check result reaper interval:       %d sec\n", check_reaper_interval);
	printf("Max concurrent service checks:      ");
	if(max_parallel_service_checks == 0)
		printf("Unlimited\n");
	else
		printf("%d\n", max_parallel_service_checks);
	printf("\n\n");

	printf("PERFORMANCE SUGGESTIONS\n");
	printf("-----------------------\n");


	/***** MAX REAPER INTERVAL RECOMMENDATION *****/

	/* assume a 100% (2x) check burst for check reaper */
	/* assume we want a max of 2k files in the result queue at any given time */
	max_reaper_interval = floor(2000 * scheduling_info.service_inter_check_delay);
	if(max_reaper_interval < 2.0)
		max_reaper_interval = 2.0;
	if(max_reaper_interval > 30.0)
		max_reaper_interval = 30.0;
	if((int)max_reaper_interval < check_reaper_interval) {
		printf("* Value for 'check_result_reaper_frequency' should be <= %d seconds\n", (int)max_reaper_interval);
		suggestions++;
		}
	if(check_reaper_interval < 2) {
		printf("* Value for 'check_result_reaper_frequency' should be >= 2 seconds\n");
		suggestions++;
		}

	/***** MINIMUM CONCURRENT CHECKS RECOMMENDATION *****/

	/* first method (old) - assume a 100% (2x) service check burst for max concurrent checks */
	if(scheduling_info.service_inter_check_delay == 0.0)
		minimum_concurrent_checks1 = ceil(check_reaper_interval * 2.0);
	else
		minimum_concurrent_checks1 = ceil((check_reaper_interval * 2.0) / scheduling_info.service_inter_check_delay);

	/* second method (new) - assume a 25% (1.25x) service check burst for max concurrent checks */
	minimum_concurrent_checks2 = ceil((((double)scheduling_info.total_scheduled_services) / scheduling_info.average_service_check_interval) * 1.25 * check_reaper_interval * scheduling_info.average_service_execution_time);

	/* use max of computed values */
	if(minimum_concurrent_checks1 > minimum_concurrent_checks2)
		minimum_concurrent_checks = minimum_concurrent_checks1;
	else
		minimum_concurrent_checks = minimum_concurrent_checks2;

	/* compare with configured value */
	if(((int)minimum_concurrent_checks > max_parallel_service_checks) && max_parallel_service_checks != 0) {
		printf("* Value for 'max_concurrent_checks' option should be >= %d\n", (int)minimum_concurrent_checks);
		suggestions++;
		}

	if(suggestions == 0)
		printf("I have no suggestions - things look okay.\n");

	printf("\n");

	return;
	}


/*
 * Create the event queue
 * We oversize it somewhat to avoid unnecessary growing
 */
int init_event_queue(void)
{
	unsigned int size;

	size = scheduling_info.total_hosts + scheduling_info.total_services;
	size += 4096 + (size / 100);

	nagios_squeue = squeue_create(size);
	return 0;
}

/* schedule a new timed event */
timed_event *schedule_new_event(int event_type, int high_priority, time_t run_time, int recurring, unsigned long event_interval, void *timing_func, int compensate_for_time_change, void *event_data, void *event_args, int event_options) {
	timed_event *new_event;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "schedule_new_event()\n");

	new_event = (timed_event *)malloc(sizeof(timed_event));
	if(new_event != NULL) {
		new_event->event_type = event_type;
		new_event->event_data = event_data;
		new_event->event_args = event_args;
		new_event->event_options = event_options;
		new_event->run_time = run_time;
		new_event->recurring = recurring;
		new_event->event_interval = event_interval;
		new_event->timing_func = timing_func;
		new_event->compensate_for_time_change = compensate_for_time_change;
		new_event->priority = high_priority;
		}
	else
		return NULL;

	/* add the event to the event list */
	add_event(nagios_squeue, new_event);

	return new_event;
	}


/* reschedule an event in order of execution time */
void reschedule_event(squeue_t *sq, timed_event *event) {
	time_t current_time = 0L;
	time_t (*timingfunc)(void);

	log_debug_info(DEBUGL_FUNCTIONS, 0, "reschedule_event()\n");

	/* reschedule recurring events... */
	if(event->recurring == TRUE) {

		/* use custom timing function */
		if(event->timing_func != NULL) {
			timingfunc = event->timing_func;
			event->run_time = (*timingfunc)();
			}

		/* normal recurring events */
		else {
			event->run_time = event->run_time + event->event_interval;
			time(&current_time);
			if(event->run_time < current_time)
				event->run_time = current_time;
			}
		}

	/* add the event to the event list */
	add_event(sq, event);

	return;
	}


/* add an event to list ordered by execution time */
void add_event(squeue_t *sq, timed_event *event) {

	log_debug_info(DEBUGL_FUNCTIONS, 0, "add_event()\n");

	if(event->priority) {
		event->sq_event = squeue_add_usec(sq, event->run_time, event->priority - 1, event);
		}
	else {
		event->sq_event = squeue_add(sq, event->run_time, event);
		}
	if(!event->sq_event) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Failed to add event to squeue '%p' with prio %u: %s\n",
			  sq, event->priority, strerror(errno));
		}

#ifdef USE_EVENT_BROKER
	else {
		/* send event data to broker */
		broker_timed_event(NEBTYPE_TIMEDEVENT_ADD, NEBFLAG_NONE, NEBATTR_NONE, event, NULL);
		}
#endif

	return;
	}



/* remove an event from the queue */
void remove_event(squeue_t *sq, timed_event *event) {
#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_timed_event(NEBTYPE_TIMEDEVENT_REMOVE, NEBFLAG_NONE, NEBATTR_NONE, event, NULL);
#endif
	if(sq && event)
		squeue_remove(sq, event->sq_event);
	}


static int should_run_event(timed_event *temp_event)
{
	int run_event = TRUE;	/* default action is to execute the event */
	int nudge_seconds = 5 + (rand() % 10);

	/* run a few checks before executing a service check... */
	if(temp_event->event_type == EVENT_SERVICE_CHECK) {
		service *temp_service = (service *)temp_event->event_data;

		/* don't run a service check if we're already maxed out on the number of parallel service checks...  */
		if(max_parallel_service_checks != 0 && (currently_running_service_checks >= max_parallel_service_checks)) {
			temp_service->next_check += nudge_seconds;
			temp_event->run_time += nudge_seconds;
			logit(NSLOG_RUNTIME_WARNING, TRUE, "\tMax concurrent service checks (%d) has been reached.  Nudging %s:%s by %d seconds...\n", max_parallel_service_checks, temp_service->host_name, temp_service->description, nudge_seconds);
			reschedule_event(nagios_squeue, temp_event);
			return 0;
		}

		/* don't run a service check if active checks are disabled */
		if(execute_service_checks == FALSE) {
			temp_service->next_check = temp_service->last_check + (temp_service->check_interval * interval_length) + (rand() % 7);
			temp_event->run_time = temp_service->next_check;
			log_debug_info(DEBUGL_EVENTS | DEBUGL_CHECKS, 1, "We're not executing service checks right now, so we'll skip this event.\n");
			reschedule_event(nagios_squeue, temp_event);
			run_event = FALSE;
		}

		/* forced checks override normal check logic */
		if((temp_service->check_options & CHECK_OPTION_FORCE_EXECUTION))
			run_event = TRUE;

		/* reschedule the check if we can't run it now */
		if(run_event == FALSE) {

			if(nudge_seconds) {
				/* We nudge the next check time when it is due to too many concurrent service checks */
				temp_service->next_check = (time_t)(temp_service->next_check + nudge_seconds);
			}
			else {
				/* Otherwise reschedule (TODO: This should be smarter as it doesn't consider its timeperiod) */
				if(temp_service->state_type == SOFT_STATE && temp_service->current_state != STATE_OK)
					temp_service->next_check = (time_t)(temp_service->next_check + (temp_service->retry_interval * interval_length));
				else
					temp_service->next_check = (time_t)(temp_service->next_check + (temp_service->check_interval * interval_length));
			}

			temp_event->run_time = temp_service->next_check;
			reschedule_event(nagios_squeue, temp_event);
			update_service_status(temp_service, FALSE);

			run_event = FALSE;
		}
	}
	/* run a few checks before executing a host check... */
	else if(temp_event->event_type == EVENT_HOST_CHECK) {
		host *temp_host = (host *)temp_event->event_data;

		temp_host = (host *)temp_event->event_data;

		/* don't run a host check if active checks are disabled */
		if(execute_host_checks == FALSE) {
			log_debug_info(DEBUGL_EVENTS | DEBUGL_CHECKS, 1, "We're not executing host checks right now, so we'll skip this event.\n");
			run_event = FALSE;
		}

		/* forced checks override normal check logic */
		if((temp_host->check_options & CHECK_OPTION_FORCE_EXECUTION))
			run_event = TRUE;

		/* reschedule the host check if we can't run it right now */
		if(run_event == FALSE) {
			if(temp_host->state_type == SOFT_STATE && temp_host->current_state != STATE_OK)
				temp_host->next_check = (time_t)(temp_host->next_check + (temp_host->retry_interval * interval_length));
			else
				temp_host->next_check = (time_t)(temp_host->next_check + (temp_host->check_interval * interval_length));

			temp_event->run_time = temp_host->next_check;
			reschedule_event(nagios_squeue, temp_event);
			update_host_status(temp_host, FALSE);
			run_event = FALSE;
		}
	}

	return run_event;
}

/* this is the main event handler loop */
int event_execution_loop(void) {
	timed_event *temp_event = NULL, *last_event = NULL;
	time_t last_time = 0L;
	time_t current_time = 0L;
	time_t last_status_update = 0L;
	int poll_time_ms;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "event_execution_loop() start\n");

	time(&last_time);

	while(1) {
		struct timeval now;
		const struct timeval *event_runtime;
		int inputs;

		/* super-priority (hardcoded) events come first */

		/* see if we should exit or restart (a signal was encountered) */
		if(sigshutdown == TRUE || sigrestart == TRUE)
			break;

		/* get the current time */
		time(&current_time);

		/* hey, wait a second...  we traveled back in time! */
		if(current_time < last_time)
			compensate_for_system_time_change((unsigned long)last_time, (unsigned long)current_time);

		/* else if the time advanced over the specified threshold, try and compensate... */
		else if((current_time - last_time) >= time_change_threshold)
			compensate_for_system_time_change((unsigned long)last_time, (unsigned long)current_time);

		/* get next scheduled event */
		temp_event = (timed_event *)squeue_peek(nagios_squeue);

		/* if we don't have any events to handle, exit */
		if(!temp_event) {
			log_debug_info(DEBUGL_EVENTS, 0, "There aren't any events that need to be handled! Exiting...\n");
			break;
		}

		/* keep track of the last time */
		last_time = current_time;

		/* update status information occassionally - NagVis watches the NDOUtils DB to see if Nagios is alive */
		if((unsigned long)(current_time - last_status_update) > 5) {
			last_status_update = current_time;
			update_program_status(FALSE);
			}

		event_runtime = squeue_event_runtime(temp_event->sq_event);
		if (temp_event != last_event) {
			log_debug_info(DEBUGL_EVENTS, 1, "** Event Check Loop\n");
			log_debug_info(DEBUGL_EVENTS, 1, "Next Event Time: %s", ctime(&temp_event->run_time));
			log_debug_info(DEBUGL_EVENTS, 1, "Current/Max Service Checks: %d/%d (%.3lf%% saturation)\n",
						   currently_running_service_checks, max_parallel_service_checks,
						   ((float)currently_running_service_checks / (float)max_parallel_service_checks) * 100);
		}

		last_event = temp_event;

		gettimeofday(&now, NULL);
		poll_time_ms = tv_delta_msec(&now, event_runtime) - 25;
		if (poll_time_ms < 0)
			poll_time_ms = 0;
		else if(poll_time_ms >= 1500)
			poll_time_ms = 1500;

		log_debug_info(DEBUGL_SCHEDULING | DEBUGL_IPC, 1, "## Polling %dms; sockets=%d; events=%u; iobs=%p\n",
		               poll_time_ms, iobroker_get_num_fds(nagios_iobs),
		               squeue_size(nagios_squeue), nagios_iobs);
		inputs = iobroker_poll(nagios_iobs, poll_time_ms);
		log_debug_info(DEBUGL_IPC, 2, "## %d descriptors had input\n", inputs);

		/* 100 milliseconds allowance for firing off events early */
		gettimeofday(&now, NULL);
		if (tv_delta_msec(&now, event_runtime) > 100)
			continue;

		/*
		 * we must remove the entry we've peeked, or
		 * we'll keep getting the same one over and over.
		 * This also maintains sync with broker modules.
		 */
		remove_event(nagios_squeue, temp_event);

		/* handle high priority events */
		if(temp_event->priority || should_run_event(temp_event)) {
			/* handle the event */
			handle_timed_event(temp_event);

			/* reschedule the event if necessary */
			if(temp_event->recurring == TRUE)
				reschedule_event(nagios_squeue, temp_event);

			/* else free memory associated with the event */
			else
				my_free(temp_event);
		}
	}

	log_debug_info(DEBUGL_FUNCTIONS, 0, "event_execution_loop() end\n");

	return OK;
	}



/* handles a timed event */
int handle_timed_event(timed_event *event) {
	host *temp_host = NULL;
	service *temp_service = NULL;
	void (*userfunc)(void *);
	struct timeval tv;
	const struct timeval *event_runtime;
	double latency;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "handle_timed_event() start\n");

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_timed_event(NEBTYPE_TIMEDEVENT_EXECUTE, NEBFLAG_NONE, NEBATTR_NONE, event, NULL);
#endif

	log_debug_info(DEBUGL_EVENTS, 0, "** Timed Event ** Type: %d, Run Time: %s", event->event_type, ctime(&event->run_time));

	/* get event latency */
	gettimeofday(&tv, NULL);
	event_runtime = squeue_event_runtime(event->sq_event);
	latency = (double)(tv_delta_f(&tv, event_runtime));
	if (latency < 0.0) /* events may run up to 0.1 seconds early */
		latency = 0.0;

	/* how should we handle the event? */
	switch(event->event_type) {

		case EVENT_SERVICE_CHECK:

			temp_service = (service *)event->event_data;

			log_debug_info(DEBUGL_EVENTS, 0, "** Service Check Event ==> Host: '%s', Service: '%s', Options: %d, Latency: %f sec\n", temp_service->host_name, temp_service->description, event->event_options, latency);

			/* run the service check */
			temp_service = (service *)event->event_data;
			run_scheduled_service_check(temp_service, event->event_options, latency);
			break;

		case EVENT_HOST_CHECK:

			temp_host = (host *)event->event_data;

			log_debug_info(DEBUGL_EVENTS, 0, "** Host Check Event ==> Host: '%s', Options: %d, Latency: %f sec\n", temp_host->name, event->event_options, latency);

			/* run the host check */
			temp_host = (host *)event->event_data;
			perform_scheduled_host_check(temp_host, event->event_options, latency);
			break;

		case EVENT_LOG_ROTATION:

			log_debug_info(DEBUGL_EVENTS, 0, "** Log File Rotation Event. Latency: %.3fs\n", latency);

			/* rotate the log file */
			rotate_log_file(event->run_time);
			break;

		case EVENT_PROGRAM_SHUTDOWN:

			log_debug_info(DEBUGL_EVENTS, 0, "** Program Shutdown Event. Latency: %.3fs\n", latency);

			/* set the shutdown flag */
			sigshutdown = TRUE;

			/* log the shutdown */
			logit(NSLOG_PROCESS_INFO, TRUE, "PROGRAM_SHUTDOWN event encountered, shutting down...\n");
			break;

		case EVENT_PROGRAM_RESTART:

			log_debug_info(DEBUGL_EVENTS, 0, "** Program Restart Event. Latency: %.3fs\n", latency);

			/* set the restart flag */
			sigrestart = TRUE;

			/* log the restart */
			logit(NSLOG_PROCESS_INFO, TRUE, "PROGRAM_RESTART event encountered, restarting...\n");
			break;

		case EVENT_CHECK_REAPER:

			log_debug_info(DEBUGL_EVENTS, 0, "** Check Result Reaper. Latency: %.3fs\n", latency);

			/* reap host and service check results */
			reap_check_results();
			break;

		case EVENT_ORPHAN_CHECK:

			log_debug_info(DEBUGL_EVENTS, 0, "** Orphaned Host and Service Check Event. Latency: %.3fs\n", latency);

			/* check for orphaned hosts and services */
			if(check_orphaned_hosts == TRUE)
				check_for_orphaned_hosts();
			if(check_orphaned_services == TRUE)
				check_for_orphaned_services();
			break;

		case EVENT_RETENTION_SAVE:

			log_debug_info(DEBUGL_EVENTS, 0, "** Retention Data Save Event. Latency: %.3fs\n", latency);

			/* save state retention data */
			save_state_information(TRUE);
			break;

		case EVENT_STATUS_SAVE:

			log_debug_info(DEBUGL_EVENTS, 0, "** Status Data Save Event. Latency: %.3fs\n", latency);

			/* save all status data (program, host, and service) */
			update_all_status_data();
			break;

		case EVENT_SCHEDULED_DOWNTIME:

			log_debug_info(DEBUGL_EVENTS, 0, "** Scheduled Downtime Event. Latency: %.3fs\n", latency);

			/* process scheduled downtime info */
			if(event->event_data) {
				handle_scheduled_downtime_by_id(*(unsigned long *)event->event_data);
				free(event->event_data);
				event->event_data = NULL;
				}
			break;

		case EVENT_SFRESHNESS_CHECK:

			log_debug_info(DEBUGL_EVENTS, 0, "** Service Result Freshness Check Event. Latency: %.3fs\n", latency);

			/* check service result freshness */
			check_service_result_freshness();
			break;

		case EVENT_HFRESHNESS_CHECK:

			log_debug_info(DEBUGL_EVENTS, 0, "** Host Result Freshness Check Event. Latency: %.3fs\n", latency);

			/* check host result freshness */
			check_host_result_freshness();
			break;

		case EVENT_EXPIRE_DOWNTIME:

			log_debug_info(DEBUGL_EVENTS, 0, "** Expire Downtime Event. Latency: %.3fs\n", latency);

			/* check for expired scheduled downtime entries */
			check_for_expired_downtime();
			break;

		case EVENT_RESCHEDULE_CHECKS:

			/* adjust scheduling of host and service checks */
			log_debug_info(DEBUGL_EVENTS, 0, "** Reschedule Checks Event. Latency: %.3fs\n", latency);

			adjust_check_scheduling();
			break;

		case EVENT_EXPIRE_COMMENT:

			log_debug_info(DEBUGL_EVENTS, 0, "** Expire Comment Event. Latency: %.3fs\n", latency);

			/* check for expired comment */
			check_for_expired_comment((unsigned long)event->event_data);
			break;

		case EVENT_CHECK_PROGRAM_UPDATE:

			log_debug_info(DEBUGL_EVENTS, 0, "** Check For Program Update. Latency: %.3fs\n", latency);

			/* check for new versions of Nagios */
			check_for_nagios_updates(FALSE, TRUE);
			break;

		case EVENT_USER_FUNCTION:

			log_debug_info(DEBUGL_EVENTS, 0, "** User Function Event. Latency: %.3fs\n", latency);

			/* run a user-defined function */
			if(event->event_data != NULL) {
				userfunc = event->event_data;
				(*userfunc)(event->event_args);
				}
			break;

		default:

			break;
		}

	log_debug_info(DEBUGL_FUNCTIONS, 0, "handle_timed_event() end\n");

	return OK;
	}



/*
 * adjusts scheduling of host and service checks
 * @TODO: Replace the earlier O(n lg n) behaviour of this algorithm
 * and instead add a (small) random component to each new scheduled
 * check. It will work better and in O(1) time.
 */
void adjust_check_scheduling(void) {
	return;
	}

static void adjust_squeue_for_time_change(squeue_t **q, int delta) {
	timed_event *event;
	squeue_t *sq_new;

	/*
	 * this is pretty inefficient in terms of free() + malloc(),
	 * but it should be pretty rare that we have to adjust times
	 * so we go with the well-tested codepath.
	 */
	sq_new = squeue_create(squeue_size(*q));
	while ((event = squeue_pop(*q))) {
		if (event->compensate_for_time_change == TRUE) {
			if (event->timing_func) {
				time_t (*timingfunc)(void);
				timingfunc = event->timing_func;
				event->run_time = timingfunc();
				}
			else {
				event->run_time += delta;
				}
			}
		if(event->priority) {
			event->sq_event = squeue_add_usec(sq_new, event->run_time, event->priority - 1, event);
			}
		else {
			event->sq_event = squeue_add(sq_new, event->run_time, event);
			}
		}
	squeue_destroy(*q, 0);
	*q = sq_new;
	}

/* attempts to compensate for a change in the system time */
void compensate_for_system_time_change(unsigned long last_time, unsigned long current_time) {
	unsigned long time_difference = 0L;
	service *temp_service = NULL;
	host *temp_host = NULL;
	int days = 0;
	int hours = 0;
	int minutes = 0;
	int seconds = 0;
	int delta = 0;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "compensate_for_system_time_change() start\n");

	/*
	 * if current_time < last_time, delta will be negative so we can
	 * still use addition to all effected timestamps
	 */
	delta = current_time - last_time;

	/* we moved back in time... */
	if(last_time > current_time) {
		time_difference = last_time - current_time;
		get_time_breakdown(time_difference, &days, &hours, &minutes, &seconds);
		log_debug_info(DEBUGL_EVENTS, 0, "Detected a backwards time change of %dd %dh %dm %ds.\n", days, hours, minutes, seconds);
		}

	/* we moved into the future... */
	else {
		time_difference = current_time - last_time;
		get_time_breakdown(time_difference, &days, &hours, &minutes, &seconds);
		log_debug_info(DEBUGL_EVENTS, 0, "Detected a forwards time change of %dd %dh %dm %ds.\n", days, hours, minutes, seconds);
		}

	/* log the time change */
	logit(NSLOG_PROCESS_INFO | NSLOG_RUNTIME_WARNING, TRUE, "Warning: A system time change of %d seconds (%dd %dh %dm %ds %s in time) has been detected.  Compensating...\n",
	      delta, days, hours, minutes, seconds,
	      (last_time > current_time) ? "backwards" : "forwards");

	adjust_squeue_for_time_change(&nagios_squeue, delta);

	/* adjust service timestamps */
	for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_service->last_notification);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_service->last_check);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_service->next_check);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_service->last_state_change);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_service->last_hard_state_change);

		/* recalculate next re-notification time */
		temp_service->next_notification = get_next_service_notification_time(temp_service, temp_service->last_notification);

		/* update the status data */
		update_service_status(temp_service, FALSE);
		}

	/* adjust host timestamps */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->last_host_notification);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->last_check);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->next_check);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->last_state_change);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->last_hard_state_change);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->last_state_history_update);

		/* recalculate next re-notification time */
		temp_host->next_host_notification = get_next_host_notification_time(temp_host, temp_host->last_host_notification);

		/* update the status data */
		update_host_status(temp_host, FALSE);
		}

	/* adjust program timestamps */
	adjust_timestamp_for_time_change(last_time, current_time, time_difference, &program_start);
	adjust_timestamp_for_time_change(last_time, current_time, time_difference, &event_start);

	/* update the status data */
	update_program_status(FALSE);

	return;
	}



/* adjusts a timestamp variable in accordance with a system time change */
void adjust_timestamp_for_time_change(time_t last_time, time_t current_time, unsigned long time_difference, time_t *ts) {

	log_debug_info(DEBUGL_FUNCTIONS, 0, "adjust_timestamp_for_time_change()\n");

	/* we shouldn't do anything with epoch values */
	if(*ts == (time_t)0)
		return;

	/* we moved back in time... */
	if(last_time > current_time) {

		/* we can't precede the UNIX epoch */
		if(time_difference > (unsigned long)*ts)
			*ts = (time_t)0;
		else
			*ts = (time_t)(*ts - time_difference);
		}

	/* we moved into the future... */
	else
		*ts = (time_t)(*ts + time_difference);

	return;
	}
