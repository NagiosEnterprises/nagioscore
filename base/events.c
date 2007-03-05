/*****************************************************************************
 *
 * EVENTS.C - Timed event functions for Nagios
 *
 * Copyright (c) 1999-2007 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   03-05-2007
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
#include "../include/comments.h"
#include "../include/downtime.h"
#include "../include/statusdata.h"
#include "../include/nagios.h"
#include "../include/broker.h"
#include "../include/sretention.h"


extern char	*config_file;

extern time_t   program_start;
extern time_t   last_command_check;

extern int      sigshutdown;
extern int      sigrestart;

extern double   sleep_time;
extern int      interval_length;
extern int      service_inter_check_delay_method;
extern int      host_inter_check_delay_method;
extern int      service_interleave_factor_method;
extern int      max_host_check_spread;
extern int      max_service_check_spread;

extern int      command_check_interval;
extern int      service_check_reaper_interval;
extern int      service_freshness_check_interval;
extern int      host_freshness_check_interval;
extern int      auto_rescheduling_interval;
extern int      auto_rescheduling_window;

extern int      check_external_commands;
extern int      check_orphaned_services;
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

extern int      service_check_timeout;

extern int      execute_service_checks;
extern int      execute_host_checks;

extern int      time_change_threshold;

extern int      non_parallelized_check_running;

timed_event *event_list_low=NULL;
timed_event *event_list_high=NULL;

extern host     *host_list;
extern service  *service_list;

sched_info scheduling_info;

extern unsigned long   check_result_buffer_slots;


/******************************************************************/
/************ EVENT SCHEDULING/HANDLING FUNCTIONS *****************/
/******************************************************************/

/* initialize the event timing loop before we start monitoring */
void init_timing_loop(void){
	host *temp_host;
	service *temp_service;
	time_t current_time;
	unsigned long interval_to_use;
	int total_interleave_blocks=0;
	int current_interleave_block=1;
	int interleave_block_index=0;
	int mult_factor;
	int is_valid_time;
	time_t next_valid_time;

	int schedule_check;
	double max_inter_check_delay=0.0;

#ifdef DEBUG0
	printf("init_timing_loop() start\n");
#endif

	/* get the time right now */
	time(&current_time);


	/******** GET BASIC HOST/SERVICE INFO  ********/

	scheduling_info.total_services=0;
	scheduling_info.total_scheduled_services=0;
	scheduling_info.total_hosts=0;
	scheduling_info.total_scheduled_hosts=0;
	scheduling_info.average_services_per_host=0.0;
	scheduling_info.average_scheduled_services_per_host=0.0;
	scheduling_info.service_check_interval_total=0;
	scheduling_info.average_service_inter_check_delay=0.0;
	scheduling_info.host_check_interval_total=0;
	scheduling_info.average_host_inter_check_delay=0.0;
		
	/* get info on service checks to be scheduled */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		schedule_check=TRUE;

		/* service has no check interval */
		if(temp_service->check_interval==0)
			schedule_check=FALSE;

		/* active checks are disabled */
		if(temp_service->checks_enabled==FALSE)
			schedule_check=FALSE;

		/* are there any valid times this service can be checked? */
		is_valid_time=check_time_against_period(current_time,temp_service->check_period);
		if(is_valid_time==ERROR){
			get_next_valid_time(current_time,&next_valid_time,temp_service->check_period);
			if(current_time==next_valid_time)
				schedule_check=FALSE;
		        }

		if(schedule_check==TRUE){

			scheduling_info.total_scheduled_services++;

			/* used later in inter-check delay calculations */
			scheduling_info.service_check_interval_total+=temp_service->check_interval;
		        }
		else{
			temp_service->should_be_scheduled=FALSE;
#ifdef DEBUG1
			printf("Service '%s' on host '%s' should not be scheduled.\n",temp_service->description,temp_service->host_name);
#endif
		        }

		scheduling_info.total_services++;
	        }

	/* get info on host checks to be scheduled */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		schedule_check=TRUE;

		/* host has no check interval */
		if(temp_host->check_interval==0)
			schedule_check=FALSE;

		/* active checks are disabled */
		if(temp_host->checks_enabled==FALSE)
			schedule_check=FALSE;

		/* are there any valid times this host can be checked? */
		is_valid_time=check_time_against_period(current_time,temp_host->check_period);
		if(is_valid_time==ERROR){
			get_next_valid_time(current_time,&next_valid_time,temp_host->check_period);
			if(current_time==next_valid_time)
				schedule_check=FALSE;
		        }

		if(schedule_check==TRUE){

			scheduling_info.total_scheduled_hosts++;

			/* this is used later in inter-check delay calculations */
			scheduling_info.host_check_interval_total+=temp_host->check_interval;
		        }
		else{
			temp_host->should_be_scheduled=FALSE;
#ifdef DEBUG1
			printf("Host '%s' should not be scheduled\n",temp_host->name);
#endif
		        }

		scheduling_info.total_hosts++;
	        }

	scheduling_info.average_services_per_host=(double)((double)scheduling_info.total_services/(double)scheduling_info.total_hosts);
	scheduling_info.average_scheduled_services_per_host=(double)((double)scheduling_info.total_scheduled_services/(double)scheduling_info.total_hosts);


	/******** DETERMINE SERVICE SCHEDULING PARAMS  ********/

	/* default max service check spread (in minutes) */
	scheduling_info.max_service_check_spread=max_service_check_spread;

	/* how should we determine the service inter-check delay to use? */
	switch(service_inter_check_delay_method){

	case ICD_NONE:

		/* don't spread checks out - useful for testing parallelization code */
		scheduling_info.service_inter_check_delay=0.0;
		break;

	case ICD_DUMB:

		/* be dumb and just schedule checks 1 second apart */
		scheduling_info.service_inter_check_delay=1.0;
		break;
		
	case ICD_USER:

		/* the user specified a delay, so don't try to calculate one */
		break;

	case ICD_SMART:
	default:

		/* be smart and calculate the best delay to use to minimize local load... */
		if(scheduling_info.total_scheduled_services>0 && scheduling_info.service_check_interval_total>0){

			/* adjust the check interval total to correspond to the interval length */
			scheduling_info.service_check_interval_total=(scheduling_info.service_check_interval_total*interval_length);

			/* calculate the average check interval for services */
			scheduling_info.average_service_check_interval=(double)((double)scheduling_info.service_check_interval_total/(double)scheduling_info.total_scheduled_services);

			/* calculate the average inter check delay (in seconds) needed to evenly space the service checks out */
			scheduling_info.average_service_inter_check_delay=(double)(scheduling_info.average_service_check_interval/(double)scheduling_info.total_scheduled_services);

			/* set the global inter check delay value */
			scheduling_info.service_inter_check_delay=scheduling_info.average_service_inter_check_delay;

			/* calculate max inter check delay and see if we should use that instead */
			max_inter_check_delay=(double)((scheduling_info.max_service_check_spread*60.0)/(double)scheduling_info.total_scheduled_services);
			if(scheduling_info.service_inter_check_delay>max_inter_check_delay)
				scheduling_info.service_inter_check_delay=max_inter_check_delay;
		        }
		else
			scheduling_info.service_inter_check_delay=0.0;

#ifdef DEBUG1
		printf("\tTotal scheduled service checks:  %d\n",scheduling_info.total_scheduled_services);
		printf("\tService check interval total:    %lu\n",scheduling_info.service_check_interval_total);
		printf("\tAverage service check interval:  %0.2f sec\n",scheduling_info.average_service_check_interval);
		printf("\tService inter-check delay:       %0.2f sec\n",scheduling_info.service_inter_check_delay);
#endif
	        }

	/* how should we determine the service interleave factor? */
	switch(service_interleave_factor_method){

	case ILF_USER:

		/* the user supplied a value, so don't do any calculation */
		break;

	case ILF_SMART:
	default:

		/* protect against a divide by zero problem - shouldn't happen, but just in case... */
		if(scheduling_info.total_hosts==0)
			scheduling_info.total_hosts=1;

		scheduling_info.service_interleave_factor=(int)(ceil(scheduling_info.average_scheduled_services_per_host));

#ifdef DEBUG1
		printf("\tTotal scheduled service checks: %d\n",scheduling_info.total_scheduled_services);
		printf("\tTotal hosts:                    %d\n",scheduling_info.total_hosts);
		printf("\tService Interleave factor:      %d\n",scheduling_info.service_interleave_factor);
#endif
	        }

	/* calculate number of service interleave blocks */
	if(scheduling_info.service_interleave_factor==0)
		total_interleave_blocks=scheduling_info.total_scheduled_services;
	else
		total_interleave_blocks=(int)ceil((double)scheduling_info.total_scheduled_services/(double)scheduling_info.service_interleave_factor);

	scheduling_info.first_service_check=(time_t)0L;
	scheduling_info.last_service_check=(time_t)0L;

#ifdef DEBUG1
	printf("Total scheduled services: %d\n",scheduling_info.total_scheduled_services);
	printf("Service Interleave factor: %d\n",scheduling_info.service_interleave_factor);
	printf("Total service interleave blocks: %d\n",total_interleave_blocks);
	printf("Service inter-check delay: %2.1f\n",scheduling_info.service_inter_check_delay);
#endif


	/******** SCHEDULE SERVICE CHECKS  ********/

	/* determine check times for service checks (with interleaving to minimize remote load) */
	current_interleave_block=0;
	for(temp_service=service_list;temp_service!=NULL && scheduling_info.service_interleave_factor>0;){

#ifdef DEBUG1
		printf("\tCurrent Interleave Block: %d\n",current_interleave_block);
#endif

		for(interleave_block_index=0;interleave_block_index<scheduling_info.service_interleave_factor && temp_service!=NULL;temp_service=temp_service->next){

			/* skip this service if it shouldn't be scheduled */
			if(temp_service->should_be_scheduled==FALSE)
				continue;

			/* skip services that are already scheduled (from retention data) */
			if(temp_service->next_check!=(time_t)0)
				continue;

			/* interleave block index should only be increased when we find a schedulable service */
			/* moved from for() loop 11/05/05 EG */
			interleave_block_index++;

			mult_factor=current_interleave_block+(interleave_block_index*total_interleave_blocks);

#ifdef DEBUG1
			printf("\t\tService '%s' on host '%s'\n",temp_service->description,temp_service->host_name);
			printf("\t\t\tCIB: %d, IBI: %d, TIB: %d, SIF: %d\n",current_interleave_block,interleave_block_index,total_interleave_blocks,scheduling_info.service_interleave_factor);
			printf("\t\t\tMult factor: %d\n",mult_factor);
#endif

			/* set the preferred next check time for the service */
			temp_service->next_check=(time_t)(current_time+(mult_factor*scheduling_info.service_inter_check_delay));

#ifdef DEBUG1
			printf("\t\t\tPreferred Check Time: %lu --> %s",(unsigned long)temp_service->next_check,ctime(&temp_service->next_check));
#endif

			/* make sure the service can actually be scheduled when we want */
			is_valid_time=check_time_against_period(temp_service->next_check,temp_service->check_period);
			if(is_valid_time==ERROR){
				get_next_valid_time(temp_service->next_check,&next_valid_time,temp_service->check_period);
				temp_service->next_check=next_valid_time;
			        }

#ifdef DEBUG1
			printf("\t\t\tActual Check Time: %lu --> %s",(unsigned long)temp_service->next_check,ctime(&temp_service->next_check));
#endif

			if(scheduling_info.first_service_check==(time_t)0 || (temp_service->next_check<scheduling_info.first_service_check))
				scheduling_info.first_service_check=temp_service->next_check;
			if(temp_service->next_check > scheduling_info.last_service_check)
				scheduling_info.last_service_check=temp_service->next_check;
		        }

		current_interleave_block++;
	        }

	/* add scheduled service checks to event queue */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		/* skip services that shouldn't be scheduled */
		if(temp_service->should_be_scheduled==FALSE)
			continue;

		/* create a new service check event */
		schedule_new_event(EVENT_SERVICE_CHECK,FALSE,temp_service->next_check,FALSE,0,NULL,TRUE,(void *)temp_service,NULL);
	        }


	/******** DETERMINE HOST SCHEDULING PARAMS  ********/

	scheduling_info.first_host_check=(time_t)0L;
	scheduling_info.last_host_check=(time_t)0L;

	/* default max host check spread (in minutes) */
	scheduling_info.max_host_check_spread=max_host_check_spread;

	/* how should we determine the host inter-check delay to use? */
	switch(host_inter_check_delay_method){

	case ICD_NONE:

		/* don't spread checks out */
		scheduling_info.host_inter_check_delay=0.0;
		break;

	case ICD_DUMB:

		/* be dumb and just schedule checks 1 second apart */
		scheduling_info.host_inter_check_delay=1.0;
		break;
		
	case ICD_USER:

		/* the user specified a delay, so don't try to calculate one */
		break;

	case ICD_SMART:
	default:

		/* be smart and calculate the best delay to use to minimize local load... */
		if(scheduling_info.total_scheduled_hosts>0 && scheduling_info.host_check_interval_total>0){

			/* adjust the check interval total to correspond to the interval length */
			scheduling_info.host_check_interval_total=(scheduling_info.host_check_interval_total*interval_length);

			/* calculate the average check interval for hosts */
			scheduling_info.average_host_check_interval=(double)((double)scheduling_info.host_check_interval_total/(double)scheduling_info.total_scheduled_hosts);

			/* calculate the average inter check delay (in seconds) needed to evenly space the host checks out */
			scheduling_info.average_host_inter_check_delay=(double)(scheduling_info.average_host_check_interval/(double)scheduling_info.total_scheduled_hosts);

			/* set the global inter check delay value */
			scheduling_info.host_inter_check_delay=scheduling_info.average_host_inter_check_delay;

			/* calculate max inter check delay and see if we should use that instead */
			max_inter_check_delay=(double)((scheduling_info.max_host_check_spread*60.0)/(double)scheduling_info.total_scheduled_hosts);
			if(scheduling_info.host_inter_check_delay>max_inter_check_delay)
				scheduling_info.host_inter_check_delay=max_inter_check_delay;
		        }
		else
			scheduling_info.host_inter_check_delay=0.0;

#ifdef DEBUG1
		printf("\tTotal scheduled host checks:  %d\n",scheduling_info.total_scheduled_hosts);
		printf("\tHost check interval total:    %lu\n",scheduling_info.host_check_interval_total);
		printf("\tAverage host check interval:  %0.2f sec\n",scheduling_info.average_host_check_interval);
		printf("\tHost inter-check delay:       %0.2f sec\n",scheduling_info.host_inter_check_delay);
#endif
	        }


	/******** SCHEDULE HOST CHECKS  ********/

	/* determine check times for host checks */
	mult_factor=0;
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* skip hosts that shouldn't be scheduled */
		if(temp_host->should_be_scheduled==FALSE)
			continue;

		/* skip hosts that are already scheduled (from retention data) */
		if(temp_host->next_check!=(time_t)0)
			continue;

#ifdef DEBUG1
		printf("\t\tHost '%s'\n",temp_host->name);
#endif

		/* calculate preferred host check time */
		temp_host->next_check=(time_t)(current_time+(mult_factor*scheduling_info.host_inter_check_delay));

#ifdef DEBUG1
		printf("\t\t\tPreferred Check Time: %lu --> %s",(unsigned long)temp_host->next_check,ctime(&temp_host->next_check));
#endif

		/* make sure the host can actually be scheduled at this time */
		is_valid_time=check_time_against_period(temp_host->next_check,temp_host->check_period);
		if(is_valid_time==ERROR){
			get_next_valid_time(temp_host->next_check,&next_valid_time,temp_host->check_period);
			temp_host->next_check=next_valid_time;
		        }

#ifdef DEBUG1
		printf("\t\t\tActual Check Time: %lu --> %s",(unsigned long)temp_host->next_check,ctime(&temp_host->next_check));
#endif

		if(scheduling_info.first_host_check==(time_t)0 || (temp_host->next_check<scheduling_info.first_host_check))
			scheduling_info.first_host_check=temp_host->next_check;
		if(temp_host->next_check > scheduling_info.last_host_check)
			scheduling_info.last_host_check=temp_host->next_check;

		mult_factor++;
	        }
	
	/* add scheduled host checks to event queue */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* skip hosts that shouldn't be scheduled */
		if(temp_host->should_be_scheduled==FALSE)
			continue;

		/* schedule a new host check event */
		schedule_new_event(EVENT_HOST_CHECK,FALSE,temp_host->next_check,FALSE,0,NULL,TRUE,(void *)temp_host,NULL);
	        }


	/******** SCHEDULE MISC EVENTS ********/

	/* add a host and service check rescheduling event */
	if(auto_reschedule_checks==TRUE)
		schedule_new_event(EVENT_RESCHEDULE_CHECKS,TRUE,current_time+auto_rescheduling_interval,TRUE,auto_rescheduling_interval,NULL,TRUE,NULL,NULL);

	/* add a service check reaper event */
	schedule_new_event(EVENT_SERVICE_REAPER,TRUE,current_time+service_check_reaper_interval,TRUE,service_check_reaper_interval,NULL,TRUE,NULL,NULL);

	/* add an orphaned service check event */
	if(check_orphaned_services==TRUE)
		schedule_new_event(EVENT_ORPHAN_CHECK,TRUE,current_time+(service_check_timeout*2),TRUE,(service_check_timeout*2),NULL,TRUE,NULL,NULL);

	/* add a service result "freshness" check event */
	if(check_service_freshness==TRUE)
		schedule_new_event(EVENT_SFRESHNESS_CHECK,TRUE,current_time+service_freshness_check_interval,TRUE,service_freshness_check_interval,NULL,TRUE,NULL,NULL);

	/* add a host result "freshness" check event */
	if(check_host_freshness==TRUE)
		schedule_new_event(EVENT_HFRESHNESS_CHECK,TRUE,current_time+host_freshness_check_interval,TRUE,host_freshness_check_interval,NULL,TRUE,NULL,NULL);

	/* add a status save event */
	if(aggregate_status_updates==TRUE)
		schedule_new_event(EVENT_STATUS_SAVE,TRUE,current_time+status_update_interval,TRUE,status_update_interval,NULL,TRUE,NULL,NULL);

	/* add an external command check event if needed */
	if(check_external_commands==TRUE){
		if(command_check_interval==-1)
			interval_to_use=(unsigned long)60;
		else
			interval_to_use=(unsigned long)command_check_interval;
		schedule_new_event(EVENT_COMMAND_CHECK,TRUE,current_time+interval_to_use,TRUE,interval_to_use,NULL,TRUE,NULL,NULL);
	        }

	/* add a log rotation event if necessary */
	if(log_rotation_method!=LOG_ROTATION_NONE)
		schedule_new_event(EVENT_LOG_ROTATION,TRUE,get_next_log_rotation_time(),TRUE,0,get_next_log_rotation_time,TRUE,NULL,NULL);

	/* add a retention data save event if needed */
	if(retain_state_information==TRUE && retention_update_interval>0)
		schedule_new_event(EVENT_RETENTION_SAVE,TRUE,current_time+(retention_update_interval*60),TRUE,(retention_update_interval*60),NULL,TRUE,NULL,NULL);

#ifdef DEBUG0
	printf("init_timing_loop() end\n");
#endif

	return;
        }



/* displays service check scheduling information */
void display_scheduling_info(void){
	float minimum_concurrent_checks;
	float max_reaper_interval;
	int suggestions=0;

	printf("Projected scheduling information for host and service\n");
	printf("checks is listed below.  This information assumes that\n");
	printf("you are going to start running Nagios with your current\n");
	printf("config files.\n\n");

	printf("HOST SCHEDULING INFORMATION\n");
	printf("---------------------------\n");
	printf("Total hosts:                     %d\n",scheduling_info.total_hosts);
	printf("Total scheduled hosts:           %d\n",scheduling_info.total_scheduled_hosts);

	printf("Host inter-check delay method:   ");
	if(host_inter_check_delay_method==ICD_NONE)
		printf("NONE\n");
	else if(host_inter_check_delay_method==ICD_DUMB)
		printf("DUMB\n");
	else if(host_inter_check_delay_method==ICD_SMART){
		printf("SMART\n");
		printf("Average host check interval:     %.2f sec\n",scheduling_info.average_host_check_interval);
	        }
	else
		printf("USER-SUPPLIED VALUE\n");
	printf("Host inter-check delay:          %.2f sec\n",scheduling_info.host_inter_check_delay);
	printf("Max host check spread:           %d min\n",scheduling_info.max_host_check_spread);
	printf("First scheduled check:           %s",(scheduling_info.total_scheduled_hosts==0)?"N/A\n":ctime(&scheduling_info.first_host_check));
	printf("Last scheduled check:            %s",(scheduling_info.total_scheduled_hosts==0)?"N/A\n":ctime(&scheduling_info.last_host_check));
	printf("\n\n");

	printf("SERVICE SCHEDULING INFORMATION\n");
	printf("-------------------------------\n");
	printf("Total services:                     %d\n",scheduling_info.total_services);
	printf("Total scheduled services:           %d\n",scheduling_info.total_scheduled_services);

	printf("Service inter-check delay method:   ");
	if(service_inter_check_delay_method==ICD_NONE)
		printf("NONE\n");
	else if(service_inter_check_delay_method==ICD_DUMB)
		printf("DUMB\n");
	else if(service_inter_check_delay_method==ICD_SMART){
		printf("SMART\n");
		printf("Average service check interval:     %.2f sec\n",scheduling_info.average_service_check_interval);
	        }
	else
		printf("USER-SUPPLIED VALUE\n");
	printf("Inter-check delay:                  %.2f sec\n",scheduling_info.service_inter_check_delay);

	printf("Interleave factor method:           %s\n",(service_interleave_factor_method==ILF_USER)?"USER-SUPPLIED VALUE":"SMART");
	if(service_interleave_factor_method==ILF_SMART)
		printf("Average services per host:          %.2f\n",scheduling_info.average_services_per_host);
	printf("Service interleave factor:          %d\n",scheduling_info.service_interleave_factor);

	printf("Max service check spread:           %d min\n",scheduling_info.max_service_check_spread);
	printf("First scheduled check:              %s",ctime(&scheduling_info.first_service_check));
	printf("Last scheduled check:               %s",ctime(&scheduling_info.last_service_check));
	printf("\n\n");

	printf("CHECK PROCESSING INFORMATION\n");
	printf("----------------------------\n");
	printf("Service check reaper interval:      %d sec\n",service_check_reaper_interval);
	printf("Max concurrent service checks:      ");
	if(max_parallel_service_checks==0)
		printf("Unlimited\n");
	else
		printf("%d\n",max_parallel_service_checks);
	printf("\n\n");

	printf("PERFORMANCE SUGGESTIONS\n");
	printf("-----------------------\n");

	/* check sanity of host inter-check delay */
	if(scheduling_info.host_inter_check_delay<=10.0 && scheduling_info.total_scheduled_hosts>0){
		printf("* Host checks might be scheduled too closely together - consider increasing 'check_interval' option for your hosts\n");
		suggestions++;
	        }

	/* assume a 100% (2x) service check burst for max concurrent checks */
	if(scheduling_info.service_inter_check_delay==0.0)
		minimum_concurrent_checks=ceil(service_check_reaper_interval*2.0);
	minimum_concurrent_checks=ceil((service_check_reaper_interval*2.0)/scheduling_info.service_inter_check_delay);
	if(((int)minimum_concurrent_checks > max_parallel_service_checks) && max_parallel_service_checks!=0){
		printf("* Value for 'max_concurrent_checks' option should >= %d\n",(int)minimum_concurrent_checks);
		suggestions++;
	        }

	/* assume a 100% (2x) service check burst for service check reaper */
	max_reaper_interval=floor((double)check_result_buffer_slots/scheduling_info.service_inter_check_delay);
	if(max_reaper_interval<2.0)
		max_reaper_interval=2.0;
	if(max_reaper_interval>30.0)
		max_reaper_interval=30.0;
	if((int)max_reaper_interval<service_check_reaper_interval){
		printf("* Value for 'service_reaper_frequency' should be <= %d seconds\n",(int)max_reaper_interval);
		suggestions++;
	        }
	if(service_check_reaper_interval<2){
		printf("* Value for 'service_reaper_frequency' should be >= 2 seconds\n");
		suggestions++;
	        }

	if(suggestions==0)
		printf("I have no suggestions - things look okay.\n");

	printf("\n");

	return;
        }


/* schedule a new timed event */
int schedule_new_event(int event_type, int high_priority, time_t run_time, int recurring, unsigned long event_interval, void *timing_func, int compensate_for_time_change, void *event_data, void *event_args){
	timed_event **event_list;
	timed_event *new_event;

#ifdef DEBUG0
	printf("schedule_new_event() start\n");
#endif

	if(high_priority==TRUE)
		event_list=&event_list_high;
	else
		event_list=&event_list_low;

	new_event=(timed_event *)malloc(sizeof(timed_event));
	if(new_event!=NULL){
		new_event->event_type=event_type;
		new_event->event_data=event_data;
		new_event->event_args=event_args;
		new_event->run_time=run_time;
		new_event->recurring=recurring;
		new_event->event_interval=event_interval;
		new_event->timing_func=timing_func;
		new_event->compensate_for_time_change=compensate_for_time_change;
	        }
	else
		return ERROR;

	/* add the event to the event list */
	add_event(new_event,event_list);

#ifdef DEBUG0
	printf("schedule_new_event() end\n");
#endif

	return OK;
        }


/* reschedule an event in order of execution time */
void reschedule_event(timed_event *event,timed_event **event_list){
	time_t current_time;
	time_t (*timingfunc)(void);

#ifdef DEBUG0
	printf("reschedule_event() start\n");
#endif

	/*printf("INITIAL TIME: %s",ctime(&event->run_time));*/

	/* reschedule recurring events... */
	if(event->recurring==TRUE){

		/* use custom timing function */
		if(event->timing_func!=NULL){
			timingfunc=event->timing_func;
			event->run_time=(*timingfunc)();
		        }

		/* normal recurring events */
		else{
			event->run_time=event->run_time+event->event_interval;
			time(&current_time);
			if(event->run_time<current_time)
				event->run_time=current_time;
		        }
	        }

	/*printf("RESCHEDULED TIME: %s",ctime(&event->run_time));*/

	/* add the event to the event list */
	add_event(event,event_list);

#ifdef DEBUG0
	printf("reschedule_event() end\n");
#endif

	return;
        }


/* remove event from schedule */
int deschedule_event(int event_type, int high_priority, void *event_data, void *event_args){
	timed_event **event_list;
	timed_event *temp_event;
	int found=FALSE;
	
#ifdef DEBUG0
	printf("deschedule_event() start\n");
#endif

	if(high_priority==TRUE)
		event_list=&event_list_high;
	else
		event_list=&event_list_low;

	for(temp_event=*event_list;temp_event!=NULL;temp_event=temp_event->next){
		if(temp_event->event_type==event_type && temp_event->event_data==event_data && temp_event->event_args==event_args){
			found=TRUE;
			break;
			}
		}

	/* remove the event from the event list */
	if (found){
		remove_event(temp_event,event_list);
		free(temp_event);
		}
	else{
		return ERROR; 
		}

#ifdef DEBUG0
		printf("deschedule_event() end\n");
#endif

	return OK;
	}


/* add an event to list ordered by execution time */
void add_event(timed_event *event,timed_event **event_list){
	timed_event *temp_event;
	timed_event *first_event;

#ifdef DEBUG0
	printf("add_event() start\n");
#endif

	event->next=NULL;

	first_event=*event_list;

	/* add the event to the head of the list if there are no other events */
	if(*event_list==NULL)
		*event_list=event;

        /* add event to head of the list if it should be executed first */
	else if(event->run_time < first_event->run_time){
	        event->next=*event_list;
		*event_list=event;
	        }

	/* else place the event according to next execution time */
	else{

		temp_event=*event_list;
		while(temp_event!=NULL){

			if(temp_event->next==NULL){
				temp_event->next=event;
				break;
			        }
			
			if(event->run_time < temp_event->next->run_time){
				event->next=temp_event->next;
				temp_event->next=event;
				break;
			        }

			temp_event=temp_event->next;
		        }
	        }

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_timed_event(NEBTYPE_TIMEDEVENT_ADD,NEBFLAG_NONE,NEBATTR_NONE,event,NULL);
#endif

#ifdef DEBUG0
	printf("add_event() end\n");
#endif

	return;
        }



/* remove an event from the queue */
void remove_event(timed_event *event,timed_event **event_list){
	timed_event *temp_event;

#ifdef DEBUG0
	printf("remove_event() start\n");
#endif

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_timed_event(NEBTYPE_TIMEDEVENT_REMOVE,NEBFLAG_NONE,NEBATTR_NONE,event,NULL);
#endif

	if(*event_list==NULL)
		return;

	if(*event_list==event)
		*event_list=event->next;

	else{

		for(temp_event=*event_list;temp_event!=NULL;temp_event=temp_event->next){
			if(temp_event->next==event){
				temp_event->next=temp_event->next->next;
				event->next=NULL;
				break;
			        }
		        }
	        }

#ifdef DEBUG0
	printf("remove_event() end\n");
#endif

	return;
        }



/* this is the main event handler loop */
int event_execution_loop(void){
	timed_event *temp_event;
	timed_event sleep_event;
	time_t last_time;
	time_t current_time;
	int run_event=TRUE;
	host *temp_host;
	service *temp_service;
	struct timespec delay;
	struct timeval tv;

#ifdef DEBUG0
	printf("event_execution_loop() start\n");
#endif

	time(&last_time);

	/* initialize fake "sleep" event */
	sleep_event.event_type=EVENT_SLEEP;
	sleep_event.run_time=last_time;
	sleep_event.recurring=FALSE;
	sleep_event.event_interval=0L;
	sleep_event.compensate_for_time_change=FALSE;
	sleep_event.timing_func=FALSE;
	sleep_event.event_data=FALSE;
	sleep_event.event_args=FALSE;

	while(1){

		/* see if we should exit or restart (a signal was encountered) */
		if(sigshutdown==TRUE || sigrestart==TRUE)
			break;

		/* if we don't have any events to handle, exit */
		if(event_list_high==NULL && event_list_low==NULL){
#ifdef DEBUG1
			printf("There aren't any events that need to be handled!\n");
#endif
			break;
	                }

		/* get the current time */
		time(&current_time);

		/* hey, wait a second...  we traveled back in time! */
		if(current_time<last_time)
			compensate_for_system_time_change((unsigned long)last_time,(unsigned long)current_time);

		/* else if the time advanced over the specified threshold, try and compensate... */
		else if((current_time-last_time)>=time_change_threshold)
			compensate_for_system_time_change((unsigned long)last_time,(unsigned long)current_time);

		/* keep track of the last time */
		last_time=current_time;

#ifdef DEBUG3
		printf("\n");
		printf("*** Event Check Loop ***\n");
		printf("\tCurrent time: %s",ctime(&current_time));
		if(event_list_high!=NULL)
			printf("\tNext High Priority Event Time: %s",ctime(&event_list_high->run_time));
		else
			printf("\tNo high priority events are scheduled...\n");
		if(event_list_low!=NULL)
			printf("\tNext Low Priority Event Time:  %s",ctime(&event_list_low->run_time));
		else
			printf("\tNo low priority events are scheduled...\n");
		printf("Current/Max Outstanding Service Checks: %d/%d\n",currently_running_service_checks,max_parallel_service_checks);
#endif

		/* handle high priority events */
		if(event_list_high!=NULL && (current_time>=event_list_high->run_time)){

			/* remove the first event from the timing loop */
			temp_event=event_list_high;
			event_list_high=event_list_high->next;

			/* handle the event */
			handle_timed_event(temp_event);

			/* reschedule the event if necessary */
			if(temp_event->recurring==TRUE)
				reschedule_event(temp_event,&event_list_high);

			/* else free memory associated with the event */
			else
				free(temp_event);
		        }

		/* handle low priority events */
		else if(event_list_low!=NULL && (current_time>=event_list_low->run_time)){

			/* default action is to execute the event */
			run_event=TRUE;

			/* run a few checks before executing a service check... */
			if(event_list_low->event_type==EVENT_SERVICE_CHECK){

				temp_service=(service *)event_list_low->event_data;

				/* update service check latency */
				gettimeofday(&tv,NULL);
				temp_service->latency=(double)((double)(tv.tv_sec-event_list_low->run_time)+(double)(tv.tv_usec/1000)/1000.0);

				/* don't run a service check if we're not supposed to right now */
				if(execute_service_checks==FALSE && !(temp_service->check_options & CHECK_OPTION_FORCE_EXECUTION)){
#ifdef DEBUG3
					printf("\tWe're not executing service checks right now...\n");
#endif

					/* remove the service check from the event queue and reschedule it for a later time */
					/* 12/20/05 since event was not executed, it needs to be remove()'ed to maintain sync with event broker modules */
					temp_event=event_list_low;
					remove_event(temp_event,&event_list_low);
					/*
					event_list_low=event_list_low->next;
					*/
					if(temp_service->state_type==SOFT_STATE && temp_service->current_state!=STATE_OK)
						temp_service->next_check=(time_t)(temp_service->next_check+(temp_service->retry_interval*interval_length));
					else
						temp_service->next_check=(time_t)(temp_service->next_check+(temp_service->check_interval*interval_length));
					temp_event->run_time=temp_service->next_check;
					reschedule_event(temp_event,&event_list_low);
					update_service_status(temp_service,FALSE);

					run_event=FALSE;
				        }

				/* don't run a service check if we're already maxed out on the number of parallel service checks...  */
				if(max_parallel_service_checks!=0 && (currently_running_service_checks >= max_parallel_service_checks)){
#ifdef DEBUG3
					printf("\tMax concurrent service checks (%d) has been reached.  Delaying further checks until previous checks are complete...\n",max_parallel_service_checks);
#endif
					run_event=FALSE;
				        }

				/* don't run a service check that can't be parallized if there are other checks out there... */
				if(temp_service->parallelize==FALSE && currently_running_service_checks>0){
#ifdef DEBUG3
					printf("\tA non-parallelizable check is queued for execution, but there are still other checks executing.  We'll wait...\n");
#endif
					run_event=FALSE;
				        }

				/* a service check that shouldn't be parallized with other checks is currently running, so don't execute another check */
				if(non_parallelized_check_running==TRUE){
#ifdef DEBUG3
					printf("\tA non-parallelizable check is currently running, so we have to wait before executing other checks...\n");
#endif
					run_event=FALSE;
				        }
			        }

			/* run a few checks before executing a host check... */
			if(event_list_low->event_type==EVENT_HOST_CHECK){

				temp_host=(host *)event_list_low->event_data;

				/* update host check latency */
				gettimeofday(&tv,NULL);
				temp_host->latency=(double)((double)(tv.tv_sec-event_list_low->run_time)+(double)(tv.tv_usec/1000)/1000.0);

				/* don't run a host check if we're not supposed to right now */
				if(execute_host_checks==FALSE && !(temp_host->check_options & CHECK_OPTION_FORCE_EXECUTION)){
#ifdef DEBUG3
					printf("\tWe're not executing host checks right now...\n");
#endif

					/* remove the host check from the event queue and reschedule it for a later time */
					/* 12/20/05 since event was not executed, it needs to be remove()'ed to maintain sync with event broker modules */
					temp_event=event_list_low;
					remove_event(temp_event,&event_list_low);
					/*
					event_list_low=event_list_low->next;
					*/
					temp_host->next_check=(time_t)(temp_host->next_check+(temp_host->check_interval*interval_length));
					temp_event->run_time=temp_host->next_check;
					reschedule_event(temp_event,&event_list_low);
					update_host_status(temp_host,FALSE);

					run_event=FALSE;
				        }
			        }

			/* run the event except... */
			if(run_event==TRUE){

				/* remove the first event from the timing loop */
				temp_event=event_list_low;
				event_list_low=event_list_low->next;

				/* handle the event */
				handle_timed_event(temp_event);

				/* reschedule the event if necessary */
				if(temp_event->recurring==TRUE)
					reschedule_event(temp_event,&event_list_low);

				/* else free memory associated with the event */
				else
					free(temp_event);
			        }

			/* wait a while so we don't hog the CPU... */
			else{
#ifdef USE_NANOSLEEP
				delay.tv_sec=(time_t)sleep_time;
				delay.tv_nsec=(long)((sleep_time-(double)delay.tv_sec)*1000000000);
				nanosleep(&delay,NULL);
#else
				delay.tv_sec=(time_t)sleep_time;
				if(delay.tv_sec==0L)
					delay.tv_sec=1;
				delay.tv_nsec=0L;
				sleep((unsigned int)delay.tv_sec);
#endif
			        }
		        }

		/* we don't have anything to do at this moment in time... */
		else if((event_list_high==NULL || (current_time<event_list_high->run_time)) && (event_list_low==NULL || (current_time<event_list_low->run_time))){

			/* check for external commands if we're supposed to check as often as possible */
			if(command_check_interval==-1)
				check_for_external_commands();

			/* set time to sleep so we don't hog the CPU... */
#ifdef USE_NANOSLEEP
			delay.tv_sec=(time_t)sleep_time;
			delay.tv_nsec=(long)((sleep_time-(double)delay.tv_sec)*1000000000);
#else
			delay.tv_sec=(time_t)sleep_time;
			if(delay.tv_sec==0L)
				delay.tv_sec=1;
			delay.tv_nsec=0L;
#endif

#ifdef USE_EVENT_BROKER
			/* populate fake "sleep" event */
			sleep_event.run_time=current_time;
			sleep_event.event_data=(void *)&delay;

			/* send event data to broker */
			broker_timed_event(NEBTYPE_TIMEDEVENT_SLEEP,NEBFLAG_NONE,NEBATTR_NONE,&sleep_event,NULL);
#endif

			/* wait a while so we don't hog the CPU... */
#ifdef USE_NANOSLEEP
			nanosleep(&delay,NULL);
#else
			sleep((unsigned int)delay.tv_sec);
#endif
		        }

	        }

#ifdef DEBUG0
	printf("event_execution_loop() end\n");
#endif

	return OK;
	}



/* handles a timed event */
int handle_timed_event(timed_event *event){
	host *temp_host=NULL;
	service *temp_service=NULL;
	char temp_buffer[MAX_INPUT_BUFFER];
	void (*userfunc)(void *);

#ifdef DEBUG0
	printf("handle_timed_event() start\n");
#endif

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_timed_event(NEBTYPE_TIMEDEVENT_EXECUTE,NEBFLAG_NONE,NEBATTR_NONE,event,NULL);
#endif

#ifdef DEBUG3
	printf("*** Event Details ***\n");
	printf("\tEvent time: %s",ctime(&event->run_time));
	printf("\tEvent type: %d ",event->event_type);
#endif
		
	/* how should we handle the event? */
	switch(event->event_type){

	case EVENT_SERVICE_CHECK:
#ifdef DEBUG3
		printf("(service check)\n");
		temp_service=(service *)event->event_data;
		printf("\t\tService Description: %s\n",temp_service->description);
		printf("\t\tAssociated Host:     %s\n",temp_service->host_name);
#endif

		/* run  a service check */
		temp_service=(service *)event->event_data;
		run_service_check(temp_service);
		break;

	case EVENT_HOST_CHECK:
#ifdef DEBUG3
		printf("(host check)\n");
		temp_host=(host *)event->event_data;
		printf("\t\tHost:     %s\n",temp_host->name);
#endif

		/* run a host check */
		temp_host=(host *)event->event_data;
		run_scheduled_host_check(temp_host);
		break;

	case EVENT_COMMAND_CHECK:
#ifdef DEBUG3
		printf("(external command check)\n");
#endif

		/* check for external commands */
		check_for_external_commands();
		break;

	case EVENT_LOG_ROTATION:
#ifdef DEBUG3
		printf("(log file rotation)\n");
#endif

		/* rotate the log file */
		rotate_log_file(event->run_time);
		break;

	case EVENT_PROGRAM_SHUTDOWN:
#ifdef DEBUG3
		printf("(program shutdown)\n");
#endif
		
		/* set the shutdown flag */
		sigshutdown=TRUE;

		/* log the shutdown */
		snprintf(temp_buffer,sizeof(temp_buffer),"PROGRAM_SHUTDOWN event encountered, shutting down...\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO,TRUE);
		break;

	case EVENT_PROGRAM_RESTART:
#ifdef DEBUG3
		printf("(program restart)\n");
#endif

		/* set the restart flag */
		sigrestart=TRUE;

		/* log the restart */
		snprintf(temp_buffer,sizeof(temp_buffer),"PROGRAM_RESTART event encountered, restarting...\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO,TRUE);
		break;

	case EVENT_SERVICE_REAPER:
#ifdef DEBUG3
		printf("(service check reaper)\n");
#endif

		/* reap service check results */
		reap_service_checks();
		break;

	case EVENT_ORPHAN_CHECK:
#ifdef DEBUG3
		printf("(orphaned service check)\n");
#endif

		/* check for orphaned services */
		check_for_orphaned_services();
		break;

	case EVENT_RETENTION_SAVE:
#ifdef DEBUG3
		printf("(retention save)\n");
#endif

		/* save state retention data */
		save_state_information(config_file,TRUE);
		break;

	case EVENT_STATUS_SAVE:
#ifdef DEBUG3
		printf("(status save)\n");
#endif

		/* save all status data (program, host, and service) */
		update_all_status_data();
		break;

	case EVENT_SCHEDULED_DOWNTIME:
#ifdef DEBUG3
		printf("(scheduled downtime)\n");
#endif

		/* process scheduled downtime info */
		if(event->event_data){
			handle_scheduled_downtime_by_id(*(unsigned long *)event->event_data);
			free(event->event_data);
			event->event_data=NULL;
			}
		break;

	case EVENT_SFRESHNESS_CHECK:
#ifdef DEBUG3
		printf("(service result freshness check)\n");
#endif
		
		/* check service result freshness */
		check_service_result_freshness();
		break;

	case EVENT_HFRESHNESS_CHECK:
		
#ifdef DEBUG3
		printf("(host result freshness check)\n");
#endif
		/* check host result freshness */
		check_host_result_freshness();
		break;

	case EVENT_EXPIRE_DOWNTIME:
#ifdef DEBUG3
		printf("(expire downtime)\n");
#endif

		/* check for expired scheduled downtime entries */
		check_for_expired_downtime();
		break;

	case EVENT_RESCHEDULE_CHECKS:
#ifdef DEBUG3
		printf("(reschedule checks)\n");
#endif

		/* adjust scheduling of host and service checks */
		adjust_check_scheduling();
		break;

	case EVENT_EXPIRE_COMMENT:
#ifdef DEBUG3
		printf("(expire comment)\n");
#endif

		/* check for expired comment */
		check_for_expired_comment((unsigned long)event->event_data);
		break;

	case EVENT_USER_FUNCTION:
#ifdef DEBUG3
		printf("(user function)\n");
#endif

		/* run a user-defined function */
		if(event->event_data!=NULL){
			userfunc=event->event_data;
			(*userfunc)(event->event_args);
		        }
		break;

	default:

		break;
	        }

#ifdef DEBUG0
	printf("handle_timed_event() end\n");
#endif

	return OK;
        }



/* adjusts scheduling of host and service checks */
void adjust_check_scheduling(void){
	timed_event *temp_event;
	service *temp_service=NULL;
	host *temp_host=NULL;
	double projected_host_check_overhead=0.9;
	double projected_service_check_overhead=0.1;
	time_t current_time;
	time_t first_window_time=0L;
	time_t last_window_time=0L;
	time_t last_check_time=0L;
	time_t new_run_time=0L;
	int total_checks=0;
	int current_check=0;
	double inter_check_delay=0.0;
	double current_icd_offset=0.0;
	double total_check_exec_time=0.0;
	double last_check_exec_time=0.0;
	int adjust_scheduling=FALSE;
	double exec_time_factor=0.0;
	double current_exec_time=0.0;
	double current_exec_time_offset=0.0;
	double new_run_time_offset=0.0;

#ifdef DEBUG0
	printf("adjust_check_scheduling() start\n");
#endif

	/* TODO:
	   - Track host check overhead on a per-host bases
	   - Figure out how to calculate service check overhead 
	*/

	/* determine our adjustment window */
	time(&current_time);
	first_window_time=current_time;
	last_window_time=first_window_time+auto_rescheduling_window;

	/* get current scheduling data */
	for(temp_event=event_list_low;temp_event!=NULL;temp_event=temp_event->next){

		/* skip events outside of our current window */
		if(temp_event->run_time<=first_window_time)
			continue;
		if(temp_event->run_time>last_window_time)
			break;

		if(temp_event->event_type==EVENT_HOST_CHECK){

			temp_host=(host *)temp_event->event_data;
			if(temp_host==NULL)
				continue;

			/* ignore forced checks */
			if(temp_host->check_options & CHECK_OPTION_FORCE_EXECUTION)
				continue;

			/* does the last check "bump" into this one? */
			if((unsigned long)(last_check_time+last_check_exec_time)>temp_event->run_time)
				adjust_scheduling=TRUE;
	
			last_check_time=temp_event->run_time;

			/* calculate time needed to perform check */
			last_check_exec_time=temp_host->execution_time+projected_host_check_overhead;
			total_check_exec_time+=last_check_exec_time;
		        }

		else if(temp_event->event_type==EVENT_SERVICE_CHECK){

			temp_service=(service *)temp_event->event_data;
			if(temp_service==NULL)
				continue;

			/* ignore forced checks */
			if(temp_service->check_options & CHECK_OPTION_FORCE_EXECUTION)
				continue;

			/* does the last check "bump" into this one? */
			if((unsigned long)(last_check_time+last_check_exec_time)>temp_event->run_time)
				adjust_scheduling=TRUE;
	
			last_check_time=temp_event->run_time;

			/* calculate time needed to perform check */
			/* NOTE: service check execution time is not taken into account, as service checks are run in parallel */
			last_check_exec_time=projected_service_check_overhead;
			total_check_exec_time+=last_check_exec_time;
		        }

		else
			continue;

		total_checks++;
	        }


	/* nothing to do... */
	if(total_checks==0 || adjust_scheduling==FALSE){

		/*
		printf("\n\n");
		printf("NOTHING TO DO!\n");
		printf("# CHECKS:    %d\n",total_checks);
		printf("WINDOW TIME: %d\n",auto_rescheduling_window);
		printf("EXEC TIME:   %.3f\n",total_check_exec_time);
		*/

		return;
	        }

	if((unsigned long)total_check_exec_time>auto_rescheduling_window){
		inter_check_delay=0.0;
		exec_time_factor=(double)((double)auto_rescheduling_window/total_check_exec_time);
	        }
	else{
		inter_check_delay=(double)((((double)auto_rescheduling_window)-total_check_exec_time)/(double)(total_checks*1.0));
		exec_time_factor=1.0;
	        }

	/*
	printf("\n\n");
	printf("TOTAL CHECKS: %d\n",total_checks);
	printf("WINDOW TIME:  %d\n",auto_rescheduling_window);
	printf("EXEC TIME:    %.3f\n",total_check_exec_time);
	printf("ICD:          %.3f\n",inter_check_delay);
	printf("EXEC FACTOR:  %.3f\n",exec_time_factor);
	*/

	/* adjust check scheduling */
	current_icd_offset=(inter_check_delay/2.0);
	for(temp_event=event_list_low;temp_event!=NULL;temp_event=temp_event->next){

		/* skip events outside of our current window */
		if(temp_event->run_time<=first_window_time)
			continue;
		if(temp_event->run_time>last_window_time)
			break;

		if(temp_event->event_type==EVENT_HOST_CHECK){

			temp_host=(host *)temp_event->event_data;
			if(temp_host==NULL)
				continue;

			/* ignore forced checks */
			if(temp_host->check_options & CHECK_OPTION_FORCE_EXECUTION)
				continue;

			current_exec_time=((temp_host->execution_time+projected_host_check_overhead)*exec_time_factor);
		        }

		else if(temp_event->event_type==EVENT_SERVICE_CHECK){

			temp_service=(service *)temp_event->event_data;
			if(temp_service==NULL)
				continue;

			/* ignore forced checks */
			if(temp_service->check_options & CHECK_OPTION_FORCE_EXECUTION)
				continue;

			/* NOTE: service check execution time is not taken into account, as service checks are run in parallel */
			current_exec_time=(projected_service_check_overhead*exec_time_factor);
		        }

		else
			continue;

		current_check++;
		new_run_time_offset=current_exec_time_offset+current_icd_offset;
		new_run_time=(time_t)(first_window_time+(unsigned long)new_run_time_offset);

		/*
		printf("  CURRENT CHECK #:      %d\n",current_check);
		printf("  CURRENT ICD OFFSET:   %.3f\n",current_icd_offset);
		printf("  CURRENT EXEC TIME:    %.3f\n",current_exec_time);
		printf("  CURRENT EXEC OFFSET:  %.3f\n",current_exec_time_offset);
		printf("  NEW RUN TIME:         %lu\n",new_run_time);
		*/

		if(temp_event->event_type==EVENT_HOST_CHECK){
			temp_event->run_time=new_run_time;
			temp_host->next_check=new_run_time;
			update_host_status(temp_host,FALSE);
		        }
		else{
			temp_event->run_time=new_run_time;
			temp_service->next_check=new_run_time;
			update_service_status(temp_service,FALSE);
		        }

		current_icd_offset+=inter_check_delay;
		current_exec_time_offset+=current_exec_time;
	        }

	/* resort event list (some events may be out of order at this point) */
	resort_event_list(&event_list_low);

#ifdef DEBUG0
	printf("adjust_check_scheduling() end\n");
#endif

	return;
        }



/* attempts to compensate for a change in the system time */
void compensate_for_system_time_change(unsigned long last_time,unsigned long current_time){
	char buffer[MAX_INPUT_BUFFER];
	unsigned long time_difference;
	timed_event *temp_event;
	service *temp_service;
	host *temp_host;
	time_t (*timingfunc)(void);

#ifdef DEBUG0
	printf("compensate_for_system_time_change() start\n");
#endif

	/* we moved back in time... */
	if(last_time>current_time)
		time_difference=last_time-current_time;

	/* we moved into the future... */
	else
		time_difference=current_time-last_time;

	/* log the time change */
	snprintf(buffer,sizeof(buffer),"Warning: A system time change of %lu seconds (%s in time) has been detected.  Compensating...\n",time_difference,(last_time>current_time)?"backwards":"forwards");
	buffer[sizeof(buffer)-1]='\x0';
	write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO | NSLOG_RUNTIME_WARNING,TRUE);

	/* adjust the next run time for all high priority timed events */
	for(temp_event=event_list_high;temp_event!=NULL;temp_event=temp_event->next){

		/* skip special events that occur at specific times... */
		if(temp_event->compensate_for_time_change==FALSE)
			continue;

		/* use custom timing function */
		if(temp_event->timing_func!=NULL){
			timingfunc=temp_event->timing_func;
			temp_event->run_time=(*timingfunc)();
		        }

		/* else use standard adjustment */
		else
			adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_event->run_time);
	        }

	/* resort event list (some events may be out of order at this point) */
	resort_event_list(&event_list_high);

	/* adjust the next run time for all low priority timed events */
	for(temp_event=event_list_low;temp_event!=NULL;temp_event=temp_event->next){

		/* skip special events that occur at specific times... */
		if(temp_event->compensate_for_time_change==FALSE)
			continue;

		/* use custom timing function */
		if(temp_event->timing_func!=NULL){
			timingfunc=temp_event->timing_func;
			temp_event->run_time=(*timingfunc)();
		        }

		/* else use standard adjustment */
		else
			adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_event->run_time);
	        }

	/* resort event list (some events may be out of order at this point) */
	resort_event_list(&event_list_low);

	/* adjust service timestamps */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_service->last_notification);
		adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_service->last_check);
		adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_service->next_check);
		adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_service->last_state_change);
		adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_service->last_hard_state_change);

		/* recalculate next re-notification time */
		temp_service->next_notification=get_next_service_notification_time(temp_service,temp_service->last_notification);

		/* update the status data */
		update_service_status(temp_service,FALSE);
	        }

	/* adjust host timestamps */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_host->last_host_notification);
		adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_host->last_check);
		adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_host->next_check);
		adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_host->last_state_change);
		adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_host->last_hard_state_change);
		adjust_timestamp_for_time_change(last_time,current_time,time_difference,&temp_host->last_state_history_update);

		/* recalculate next re-notification time */
		temp_host->next_host_notification=get_next_host_notification_time(temp_host,temp_host->last_host_notification);

		/* update the status data */
		update_host_status(temp_host,FALSE);
	        }

	/* adjust program timestamps */
	adjust_timestamp_for_time_change(last_time,current_time,time_difference,&program_start);
	adjust_timestamp_for_time_change(last_time,current_time,time_difference,&last_command_check);

	/* update the status data */
	update_program_status(FALSE);


#ifdef DEBUG0
	printf("compensate_for_system_time_change() end\n");
#endif

	return;
        }



/* resorts an event list by event execution time - needed when compensating for system time changes */
void resort_event_list(timed_event **event_list){
	timed_event *temp_event_list;
	timed_event *temp_event;
	timed_event *next_event;

	/* move current event list to temp list */
	temp_event_list=*event_list;
	*event_list=NULL;

	/* move all events to the new event list */
	for(temp_event=temp_event_list;temp_event!=NULL;temp_event=next_event){
		next_event=temp_event->next;

		/* add the event to the newly created event list so it will be resorted */
		temp_event->next=NULL;
		add_event(temp_event,event_list);
	        }

	return;
        }



/* adjusts a timestamp variable in accordance with a system time change */
void adjust_timestamp_for_time_change(time_t last_time, time_t current_time, unsigned long time_difference, time_t *ts){

	/* we shouldn't do anything with epoch values */
	if(*ts==(time_t)0)
		return;

	/* we moved back in time... */
	if(last_time>current_time){

		/* we can't precede the UNIX epoch */
		if(time_difference>(unsigned long)*ts)
			*ts=(time_t)0;
		else
			*ts=(time_t)(*ts-time_difference);
	        }

	/* we moved into the future... */
	else
		*ts=(time_t)(*ts+time_difference);

	return;
        }
