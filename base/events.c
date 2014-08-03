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
#include "../include/workers.h"
#include "../lib/squeue.h"

/* the event we're currently processing */
static timed_event *current_event;

static unsigned int event_count[EVENT_USER_FUNCTION + 1];

/******************************************************************/
/************ EVENT SCHEDULING/HANDLING FUNCTIONS *****************/
/******************************************************************/

int dump_event_stats(int sd)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(event_count); i++) {
		nsock_printf(sd, "%s=%u;", EVENT_TYPE_STR(i), event_count[i]);
		/*
		 * VERSIONFIX: Make EVENT_SLEEP and EVENT_USER_FUNCTION
		 * appear in linear order in include/nagios.h when we go
		 * from 4.0 -> 4.1, so we can remove this junk.
		 */
		if (i == 16)
			i = 97;
		}
	nsock_printf_nul(sd, "SQUEUE_ENTRIES=%u", squeue_size(nagios_squeue));

	return OK;
	}

static void track_events(unsigned int type, int add)
{
	/*
	 * remove_event() calls track_events() with add being -1.
	 * add_event() calls us with add being 1
	 */
	if (type < ARRAY_SIZE(event_count))
		event_count[type] += add;
	}

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
	struct timeval now;
	unsigned int fixed_hosts = 0, fixed_services = 0;
	int check_delay = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "init_timing_loop() start\n");

	/* get the time and seed the prng */
	gettimeofday(&now, NULL);
	current_time = now.tv_sec;
	srand((now.tv_sec << 10) ^ now.tv_usec);


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
			double exec_time;

			/* get real exec time, or make a pessimistic guess */
			exec_time = temp_service->execution_time ? temp_service->execution_time : 2.0;

			scheduling_info.total_scheduled_services++;

			/* used later in inter-check delay calculations */
			scheduling_info.service_check_interval_total += temp_service->check_interval;

			/* calculate rolling average execution time (available from retained state information) */
			scheduling_info.average_service_execution_time = (double)(((scheduling_info.average_service_execution_time * (scheduling_info.total_scheduled_services - 1)) + exec_time) / (double)scheduling_info.total_scheduled_services);
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

	log_debug_info(DEBUGL_EVENTS, 2, "Determining service scheduling parameters...\n");

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

	log_debug_info(DEBUGL_EVENTS, 2, "Scheduling service checks...\n");

	/* determine check times for service checks (with interleaving to minimize remote load) */
	current_interleave_block = 0;
	for(temp_service = service_list; temp_service != NULL && scheduling_info.service_interleave_factor > 0;) {

		log_debug_info(DEBUGL_EVENTS, 2, "Current Interleave Block: %d\n", current_interleave_block);

		for(interleave_block_index = 0; interleave_block_index < scheduling_info.service_interleave_factor && temp_service != NULL; temp_service = temp_service->next) {
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
			if(check_delay > 0 && check_delay < check_window(temp_service)) {
				log_debug_info(DEBUGL_EVENTS, 2, "Service is already scheduled to be checked in the future: %s\n", ctime(&temp_service->next_check));
				continue;
				}

			/* interleave block index should only be increased when we find a schedulable service */
			/* moved from for() loop 11/05/05 EG */
			interleave_block_index++;

			mult_factor = current_interleave_block + (interleave_block_index * total_interleave_blocks);

			log_debug_info(DEBUGL_EVENTS, 2, "CIB: %d, IBI: %d, TIB: %d, SIF: %d\n", current_interleave_block, interleave_block_index, total_interleave_blocks, scheduling_info.service_interleave_factor);
			log_debug_info(DEBUGL_EVENTS, 2, "Mult factor: %d\n", mult_factor);

			/*
			 * set the preferred next check time for the service
			 * If we end up too far into the future, grab a random
			 * time within the service's window instead.
			 */
			check_delay =
					mult_factor * scheduling_info.service_inter_check_delay;
			if(check_delay > check_window(temp_service)) {
				log_debug_info(DEBUGL_EVENTS, 0,
						"  Fixing check time %lu secs too far away\n",
						check_delay - check_window(temp_service));
				fixed_services++;
				check_delay = ranged_urand(0, check_window(temp_service));
				log_debug_info(DEBUGL_EVENTS, 0, "  New check offset: %d\n",
						check_delay);
			}
			temp_service->next_check = (time_t)(current_time + check_delay);

			log_debug_info(DEBUGL_EVENTS, 2, "Preferred Check Time: %lu --> %s\n", (unsigned long)temp_service->next_check, ctime(&temp_service->next_check));


			/* make sure the service can actually be scheduled when we want */
			is_valid_time = check_time_against_period(temp_service->next_check, temp_service->check_period_ptr);
			if(is_valid_time == ERROR) {
				log_debug_info(DEBUGL_EVENTS, 2, "Preferred Time is Invalid In Timeperiod '%s': %lu --> %s\n", temp_service->check_period_ptr->name, (unsigned long)temp_service->next_check, ctime(&temp_service->next_check));
				get_next_valid_time(temp_service->next_check, &next_valid_time, temp_service->check_period_ptr);
				temp_service->next_check =
						(time_t)(next_valid_time + check_delay);
				}

			log_debug_info(DEBUGL_EVENTS, 2, "Actual Check Time: %lu --> %s\n", (unsigned long)temp_service->next_check, ctime(&temp_service->next_check));

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
		temp_service->next_check_event = schedule_new_event(EVENT_SERVICE_CHECK, FALSE, temp_service->next_check, FALSE, 0, NULL, TRUE, (void *)temp_service, NULL, temp_service->check_options);
		}


	if(test_scheduling == TRUE)
		gettimeofday(&tv[5], NULL);

	/******** DETERMINE HOST SCHEDULING PARAMS  ********/

	log_debug_info(DEBUGL_EVENTS, 2, "Determining host scheduling parameters...\n");

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

	log_debug_info(DEBUGL_EVENTS, 2, "Scheduling host checks...\n");

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

		/*
		 * calculate preferred host check time.
		 * If it's too far into the future, we grab a random time
		 * within this host's max check window instead
		 */
		check_delay = mult_factor * scheduling_info.host_inter_check_delay;
		if(check_delay > check_window(temp_host)) {
			log_debug_info(DEBUGL_EVENTS, 1, "Fixing check time (off by %lu)\n",
					check_delay - check_window(temp_host));
			fixed_hosts++;
			check_delay = ranged_urand(0, check_window(temp_host));
			}
		temp_host->next_check = (time_t)(current_time + check_delay);

		log_debug_info(DEBUGL_EVENTS, 2, "Preferred Check Time: %lu --> %s", (unsigned long)temp_host->next_check, ctime(&temp_host->next_check));

		/* make sure the host can actually be scheduled at this time */
		is_valid_time = check_time_against_period(temp_host->next_check, temp_host->check_period_ptr);
		if(is_valid_time == ERROR) {
			get_next_valid_time(temp_host->next_check, &next_valid_time, temp_host->check_period_ptr);
			temp_host->next_check = (time_t)(next_valid_time | check_delay);
			}

		log_debug_info(DEBUGL_EVENTS, 2, "Actual Check Time: %lu --> %s\n", (unsigned long)temp_host->next_check, ctime(&temp_host->next_check));

		if(scheduling_info.first_host_check == (time_t)0 || (temp_host->next_check < scheduling_info.first_host_check))
			scheduling_info.first_host_check = temp_host->next_check;
		if(temp_host->next_check > scheduling_info.last_host_check)
			scheduling_info.last_host_check = temp_host->next_check;

		mult_factor++;
		}

	log_debug_info(DEBUGL_EVENTS, 0, "Fixed scheduling for %u hosts and %u services\n", fixed_hosts, fixed_services);

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
		temp_host->next_check_event = schedule_new_event(EVENT_HOST_CHECK, FALSE, temp_host->next_check, FALSE, 0, NULL, TRUE, (void *)temp_host, NULL, temp_host->check_options);
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
	float minimum_concurrent_checks = 0.0;
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

	/***** MINIMUM CONCURRENT CHECKS RECOMMENDATION *****/
	minimum_concurrent_checks = ceil((((scheduling_info.total_scheduled_services / scheduling_info.average_service_check_interval)
									   + (scheduling_info.total_scheduled_hosts / scheduling_info.average_host_check_interval))
									  * 1.4 * scheduling_info.average_service_execution_time));

	printf("CHECK PROCESSING INFORMATION\n");
	printf("----------------------------\n");
	printf("Average check execution time:    %.2fs%s",
		   scheduling_info.average_service_execution_time,
		   scheduling_info.average_service_execution_time == 2.0 ? " (pessimistic guesstimate)\n" : "\n");
	printf("Estimated concurrent checks:     %.0f (%.2f per cpu core)\n",
		   minimum_concurrent_checks, (float)minimum_concurrent_checks / (float)online_cpus());
	printf("Max concurrent service checks:   ");
	if(max_parallel_service_checks == 0)
		printf("Unlimited\n");
	else
		printf("%d\n", max_parallel_service_checks);
	printf("\n\n");

	printf("PERFORMANCE SUGGESTIONS\n");
	printf("-----------------------\n");


	/* compare with configured value */
	if(((int)minimum_concurrent_checks > max_parallel_service_checks) && max_parallel_service_checks != 0) {
		printf("* Value for 'max_concurrent_checks' option should be >= %d\n", (int)minimum_concurrent_checks);
		suggestions++;
		}
	if(loadctl.nofile_limit * 0.4 < minimum_concurrent_checks) {
		printf("* Increase the \"open files\" ulimit for user '%s'\n", nagios_user);
		printf(" - You can do this by adding\n      %s hard nofiles %d\n   to /etc/security/limits.conf\n",
			   nagios_user, rup2pof2(minimum_concurrent_checks * 2));
		suggestions++;
		}
	if(loadctl.nproc_limit * 0.75 < minimum_concurrent_checks) {
		printf("* Increase the \"max user processes\" ulimit for user '%s'\n", nagios_user);
		printf(" - You can do this by adding\n      %s hard nproc %d\n   to /etc/security/limits.conf\n",
			   nagios_user, rup2pof2(minimum_concurrent_checks));
		suggestions++;
		}

	if(minimum_concurrent_checks > online_cpus() * 75) {
		printf("* Aim for a max of 50 concurrent checks / cpu core (current: %.2f)\n",
			   (float)minimum_concurrent_checks / (float)online_cpus());
		suggestions++;
		}

	if(suggestions) {
		printf("\nNOTE: These are just guidelines and *not* hard numbers.\n\n");
		printf("Ultimately, only testing will tell if your settings and hardware are\n");
		printf("suitable for the types and number of checks you're planning to run.\n");
		}
	else {
		printf("I have no suggestions - things look okay.\n");
		}

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

	size = num_objects.hosts + num_objects.services;
	if(size < 4096)
		size = 4096;

	nagios_squeue = squeue_create(size);
	return 0;
}

/* schedule a new timed event */
timed_event *schedule_new_event(int event_type, int high_priority, time_t run_time, int recurring, unsigned long event_interval, void *timing_func, int compensate_for_time_change, void *event_data, void *event_args, int event_options) {
	timed_event *new_event;
 	char run_time_string[MAX_DATETIME_LENGTH] = "";

	log_debug_info(DEBUGL_FUNCTIONS, 0, "schedule_new_event()\n");

	get_datetime_string(&run_time, run_time_string, MAX_DATETIME_LENGTH,
			SHORT_DATE_TIME);
	log_debug_info(DEBUGL_EVENTS, 0, "New Event Details:\n");
	log_debug_info(DEBUGL_EVENTS, 0, " Type:                       EVENT_%s\n",
			EVENT_TYPE_STR(event_type));
	log_debug_info(DEBUGL_EVENTS, 0, " High Priority:              %s\n",
			( high_priority ? "Yes" : "No"));
	log_debug_info(DEBUGL_EVENTS, 0, " Run Time:                   %s\n",
			run_time_string);
	log_debug_info(DEBUGL_EVENTS, 0, " Recurring:                  %s\n",
			( recurring ? "Yes" : "No"));
	log_debug_info(DEBUGL_EVENTS, 0, " Event Interval:             %lu\n",
			event_interval);
	log_debug_info(DEBUGL_EVENTS, 0, " Compensate for Time Change: %s\n",
			( compensate_for_time_change ? "Yes" : "No"));
	log_debug_info(DEBUGL_EVENTS, 0, " Event Options:              %d\n",
			event_options);

	new_event = (timed_event *)calloc(1, sizeof(timed_event));
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

	log_debug_info(DEBUGL_EVENTS, 0, " Event ID:                   %p\n", new_event);

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

	if(event->sq_event) {
		logit(NSLOG_RUNTIME_ERROR, TRUE,
		      "Error: Adding %s event that seems to already be scheduled\n",
		      EVENT_TYPE_STR(event->event_type));
		remove_event(sq, event);
	}

	if(event->priority) {
		event->sq_event = squeue_add_usec(sq, event->run_time, event->priority - 1, event);
		}
	else {
		event->sq_event = squeue_add(sq, event->run_time, event);
		}
	if(!event->sq_event) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Failed to add event to squeue '%p' with prio %u: %s\n",
			  sq, event->priority, strerror(errno));
		}

	if(sq == nagios_squeue)
		track_events(event->event_type, +1);

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
	if(!event || !event->sq_event)
		return;

	if (sq)
		squeue_remove(sq, event->sq_event);
	else
		logit(NSLOG_RUNTIME_ERROR, TRUE,
		      "Error: remove_event() called for %s event with NULL sq parameter\n",
		      EVENT_TYPE_STR(event->event_type));

	if(sq == nagios_squeue)
		track_events(event->event_type, -1);

	event->sq_event = NULL; /* mark this event as unscheduled */

	/*
	 * if we catch an event from the queue which gets removed when
	 * we go polling for input (as might happen with f.e. downtime
	 * events that we get "cancel" commands for just as they are
	 * about to start or expire), we must make sure we mark the
	 * current event as no longer scheduled, or we'll run into
	 * segfaults and memory corruptions for sure.
	 */
	if (event == current_event) {
		current_event = NULL;
		}
	}


static int should_run_event(timed_event *temp_event)
{
	int run_event = TRUE;	/* default action is to execute the event */
	int nudge_seconds = 0;

	/* we only care about jobs that cause processes to run */
	if (temp_event->event_type != EVENT_HOST_CHECK &&
	    temp_event->event_type != EVENT_SERVICE_CHECK)
	{
		return TRUE;
	}

	/* if we can't spawn any more jobs, don't bother */
	if (!wproc_can_spawn(&loadctl)) {
		wproc_reap(1, 1); /* Try to reap one job for one msec. */
		return FALSE;
	}

	/* run a few checks before executing a service check... */
	if(temp_event->event_type == EVENT_SERVICE_CHECK) {
		service *temp_service = (service *)temp_event->event_data;

		/* forced checks override normal check logic */
		if((temp_service->check_options & CHECK_OPTION_FORCE_EXECUTION))
			return TRUE;

		/* don't run a service check if we're already maxed out on the number of parallel service checks...  */
		if(max_parallel_service_checks != 0 && (currently_running_service_checks >= max_parallel_service_checks)) {
			nudge_seconds = ranged_urand(NUDGE_MIN, NUDGE_MAX);
			logit(NSLOG_RUNTIME_WARNING, TRUE, "\tMax concurrent service checks (%d) has been reached.  Nudging %s:%s by %d seconds...\n", max_parallel_service_checks, temp_service->host_name, temp_service->description, nudge_seconds);
			run_event = FALSE;
		}

		/* don't run a service check if active checks are disabled */
		if(execute_service_checks == FALSE) {
			log_debug_info(DEBUGL_EVENTS | DEBUGL_CHECKS, 1, "We're not executing service checks right now, so we'll skip check event for service '%s;%s'.\n", temp_service->host_name, temp_service->description);
			run_event = FALSE;
		}

		/* reschedule the check if we can't run it now */
		if(run_event == FALSE) {
			remove_event(nagios_squeue, temp_event);

			if(nudge_seconds) {
				/* We nudge the next check time when it is due to too many concurrent service checks */
				temp_service->next_check = (time_t)(temp_service->next_check + nudge_seconds);
			}
			else {
				temp_service->next_check += check_window(temp_service);
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

		/* forced checks override normal check logic */
		if((temp_host->check_options & CHECK_OPTION_FORCE_EXECUTION))
			return TRUE;

		/* don't run a host check if active checks are disabled */
		if(execute_host_checks == FALSE) {
			log_debug_info(DEBUGL_EVENTS | DEBUGL_CHECKS, 1, "We're not executing host checks right now, so we'll skip host check event for host '%s'.\n", temp_host->name);
			run_event = FALSE;
		}

		/* reschedule the host check if we can't run it right now */
		if(run_event == FALSE) {
			remove_event(nagios_squeue, temp_event);
			temp_host->next_check += check_window(temp_host);
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
	timed_event *temp_event, *last_event = NULL;
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
		current_event = temp_event = (timed_event *)squeue_peek(nagios_squeue);

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
		if (inputs < 0 && errno != EINTR) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Polling for input on %p failed: %s", nagios_iobs, iobroker_strerror(inputs));
			break;
		}

		log_debug_info(DEBUGL_IPC, 2, "## %d descriptors had input\n", inputs);

		/*
		 * if the event we peaked was removed from the queue from
		 * one of the I/O operations, we must take care not to
		 * try to run at, as we're (almost) sure to access free'd
		 * or invalid memory if we do.
		 */
		if (!current_event) {
			log_debug_info(DEBUGL_EVENTS, 0, "Event was cancelled by iobroker input\n");
			continue;
			}

		/* 100 milliseconds allowance for firing off events early */
		gettimeofday(&now, NULL);
		if (tv_delta_msec(&now, event_runtime) > 100)
			continue;

		/* move on if we shouldn't run this event */
		if(should_run_event(temp_event) == FALSE)
			continue;

		/* handle the event */
		handle_timed_event(temp_event);

		/*
		 * we must remove the entry we've peeked, or
		 * we'll keep getting the same one over and over.
		 * This also maintains sync with broker modules.
		 */
		remove_event(nagios_squeue, temp_event);

		/* reschedule the event if necessary */
		if(temp_event->recurring == TRUE)
			reschedule_event(nagios_squeue, temp_event);

		/* else free memory associated with the event */
		else
			my_free(temp_event);
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

	log_debug_info(DEBUGL_EVENTS, 0, "** Timed Event ** Type: EVENT_%s, Run Time: %s", EVENT_TYPE_STR(event->event_type), ctime(&event->run_time));

	/* get event latency */
	gettimeofday(&tv, NULL);
	event_runtime = squeue_event_runtime(event->sq_event);
	latency = (double)(tv_delta_f(event_runtime, &tv));
	if (latency < 0.0) /* events may run up to 0.1 seconds early */
		latency = 0.0;

	/* how should we handle the event? */
	switch(event->event_type) {

		case EVENT_SERVICE_CHECK:

			temp_service = (service *)event->event_data;

			log_debug_info(DEBUGL_EVENTS, 0, "** Service Check Event ==> Host: '%s', Service: '%s', Options: %d, Latency: %f sec\n", temp_service->host_name, temp_service->description, event->event_options, latency);

			/* run the service check */
			run_scheduled_service_check(temp_service, event->event_options, latency);
			break;

		case EVENT_HOST_CHECK:

			temp_host = (host *)event->event_data;

			log_debug_info(DEBUGL_EVENTS, 0, "** Host Check Event ==> Host: '%s', Options: %d, Latency: %f sec\n", temp_host->name, event->event_options, latency);

			/* run the host check */
			run_scheduled_host_check(temp_host, event->event_options, latency);
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



/* The squeue internal event type, declared again here so we can manipulate the
 * scheduling queue without a malloc/free for each add/remove.
 * @todo: Refactor this to not depend so heavily on the event queue
 * implementation, doing so efficiently may require a different sheduling queue
 * data structure. */
struct squeue_event {
	unsigned int pos;
	pqueue_pri_t pri;
	struct timeval when;
	void *data;
	};

/*
 * Adjusts scheduling of active, non-forced host and service checks.
 */
void adjust_check_scheduling(void) {
	pqueue_t *temp_pqueue; /* squeue_t is a typedef of pqueue_t. */
	struct squeue_event *sq_event;
	struct squeue_event **events_to_reschedule;

	timed_event *temp_event;
	service *temp_service = NULL;
	host *temp_host = NULL;

	const double INTER_CHECK_RESCHEDULE_THRESHOLD = scheduling_info.service_inter_check_delay * 0.25;
	double inter_check_delay = 0.0;
	double new_run_time_offset = 0.0;

	time_t first_window_time;
	time_t last_window_time;

	struct timeval last_check_tv = { (time_t)0, (suseconds_t)0 };

	int adjust_scheduling = FALSE;
	int total_checks = 0;
	int i;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "adjust_check_scheduling() start\n");


	/* Determine our adjustment window. */
	first_window_time = time(NULL);
	last_window_time = first_window_time + auto_rescheduling_window;

	/* Nothing to do if the first event is after the reschedule window. */
	sq_event = pqueue_peek(nagios_squeue);
	temp_event = sq_event ? sq_event->data : NULL;
	if (!temp_event || temp_event->run_time > last_window_time)
		return;


	/* Get a sorted array of all check events to reschedule. First we need a
	 * duplicate of nagios_squeue so we can get the events in-order without
	 * having to remove them from the original queue. We will use
	 * pqueue_change_priority() to move the check events in the original queue.
	 * @note: This is horribly dependent on implementation details of squeue
	 * and pqueue, but we don't have much choice to avoid a free/malloc of each
	 * squeue_event from the head to last_window_time, or avoid paying the full
	 * O(n lg n) penalty twice to drain and rebuild the queue. */
	temp_pqueue = malloc(sizeof(*temp_pqueue));
	if (!temp_pqueue) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Failed to allocate queue needed to adjust check scheduling.\n");
		return;
		}
	*temp_pqueue = *nagios_squeue;

	/* We need a separate copy of the underlying queue array. */
	temp_pqueue->d = malloc(temp_pqueue->size * sizeof(void*));
	if (!temp_pqueue->d) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Failed to allocate queue data needed to adjust check scheduling.\n");
		free(temp_pqueue);
		return;
		}
	memcpy(temp_pqueue->d, nagios_squeue->d, temp_pqueue->size * sizeof(void*));
	temp_pqueue->avail = temp_pqueue->size;

	/* Now allocate space for a sorted array of check events. We shouldn't need
	 * space for all events, but we can't really calculate how many we'll need
	 * without looking at all events. */
	events_to_reschedule = malloc((temp_pqueue->size - 1) * sizeof(void*));
	if (!events_to_reschedule) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Failed to allocate memory needed to adjust check scheduling.\n");
		pqueue_free(temp_pqueue); /* pqueue_free() to keep the events. */
		return;
		}

	/* Now we get the events to reschedule and collect some scheduling info. */
	while ((sq_event = pqueue_pop(temp_pqueue))) {

		/* We need a timed_event and event data. */
		temp_event = sq_event->data;
		if (!temp_event || !temp_event->event_data)
			continue;

		/* Skip events before our current window. */
		if (temp_event->run_time < first_window_time)
			continue;
		/* We're done once past the end of the window. */
		if (temp_event->run_time > last_window_time)
			break;

		switch (temp_event->event_type) {
			case EVENT_HOST_CHECK:
				temp_host = temp_event->event_data;
				/* Leave forced checks. */
				if (temp_host->check_options & CHECK_OPTION_FORCE_EXECUTION)
					continue;
				break;

			case EVENT_SERVICE_CHECK:
				temp_service = temp_event->event_data;
				/* Leave forced checks. */
				if (temp_service->check_options & CHECK_OPTION_FORCE_EXECUTION)
					continue;
				break;

			default:
				continue;
			}

		/* Reschedule if the last check overlap into this one. */
		if (last_check_tv.tv_sec > 0 && tv_delta_msec(&last_check_tv, &sq_event->when) < INTER_CHECK_RESCHEDULE_THRESHOLD * 1000) {
/*			log_debug_info(DEBUGL_SCHEDULING, 2, "Rescheduling event %d: %.3fs delay.\n", total_checks, tv_delta_f(&last_check_tv, &sq_event->when));
*/			adjust_scheduling = TRUE;
			}

		last_check_tv = sq_event->when;
		events_to_reschedule[total_checks++] = sq_event;
		}

	/* Removing squeue_events from temp_pqueue invalidates the positions of
	 * those events in nagios_squeue, so we need to fix that up before we
	 * return or change their priorities. Start at i=1 since i=0 is unused. */
	for (i = 1; i < (int)nagios_squeue->size; ++i) {
		if ((sq_event = nagios_squeue->d[i]))
			sq_event->pos = i;
		}

	/* No checks to reschedule, nothing to do... */
	if (total_checks < 2 || !adjust_scheduling) {
		log_debug_info(DEBUGL_SCHEDULING, 0, "No events need to be rescheduled (%d checks in %ds window).\n", total_checks, auto_rescheduling_window);

		pqueue_free(temp_pqueue);
		free(events_to_reschedule);
		return;
		}


	inter_check_delay = auto_rescheduling_window / (double)total_checks;

	log_debug_info(DEBUGL_SCHEDULING, 0, "Rescheduling events: %d checks in %ds window, ICD: %.3fs.\n", total_checks, auto_rescheduling_window, inter_check_delay);


	/* Now smooth out the schedule. */
	new_run_time_offset = inter_check_delay * 0.5;
	for (i = 0; i < total_checks; ++i, new_run_time_offset += inter_check_delay) {
		struct timeval new_run_time;

		/* All events_to_reschedule are valid squeue_events with data pointers
		 * to timed_events for non-forced host or service checks. */
		sq_event = events_to_reschedule[i];
		temp_event = sq_event->data;

		/* Calculate and apply a new queue 'when' time. */
		new_run_time.tv_sec = first_window_time + (time_t)floor(new_run_time_offset);
		new_run_time.tv_usec = (suseconds_t)(fmod(new_run_time_offset, 1.0) * 1E6);

/*		log_debug_info(DEBUGL_SCHEDULING, 2, "Check %d: offset %.3fs, new run time %lu.%06ld.\n", i, new_run_time_offset, (unsigned long)new_run_time.tv_sec, (long)new_run_time.tv_usec);
*/
		squeue_change_priority_tv(nagios_squeue, sq_event, &new_run_time);


		if (temp_event->run_time != new_run_time.tv_sec)
			temp_event->run_time = new_run_time.tv_sec;

		switch (temp_event->event_type) {
			case EVENT_HOST_CHECK:
				temp_host = temp_event->event_data;
				if (temp_host->next_check != new_run_time.tv_sec) {
					temp_host->next_check = new_run_time.tv_sec;
					update_host_status(temp_host, FALSE);
					}
				break;
			case EVENT_SERVICE_CHECK:
				temp_service = temp_event->event_data;
				if (temp_service->next_check != new_run_time.tv_sec) {
					temp_service->next_check = new_run_time.tv_sec;
					update_service_status(temp_service, FALSE);
					}
				break;
			default:
				break;
			}
		}


	log_debug_info(DEBUGL_FUNCTIONS, 0, "adjust_check_scheduling() end\n");

	pqueue_free(temp_pqueue);
	free(events_to_reschedule);
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

		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->last_notification);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->last_check);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->next_check);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->last_state_change);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->last_hard_state_change);
		adjust_timestamp_for_time_change(last_time, current_time, time_difference, &temp_host->last_state_history_update);

		/* recalculate next re-notification time */
		temp_host->next_notification = get_next_host_notification_time(temp_host, temp_host->last_notification);

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
