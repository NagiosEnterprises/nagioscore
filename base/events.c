/*****************************************************************************
 *
 * EVENTS.C - Timed event functions for Nagios
 *
 * Copyright (c) 1999-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   04-02-2003
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "../common/config.h"
#include "../common/common.h"
#include "../common/downtime.h"
#include "../common/statusdata.h"
#include "nagios.h"
#include "broker.h"


extern char	*config_file;

extern time_t   program_start;

extern int      sigshutdown;
extern int      sigrestart;

extern double   sleep_time;
extern int      interval_length;
extern int      service_inter_check_delay_method;
extern int      host_inter_check_delay_method;
extern int      service_interleave_factor_method;

extern int      command_check_interval;
extern int      service_check_reaper_interval;
extern int      freshness_check_interval;

extern int      check_external_commands;
extern int      check_orphaned_services;
extern int      check_service_freshness;

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

sched_info scheduling_info;



/******************************************************************/
/************ EVENT SCHEDULING/HANDLING FUNCTIONS *****************/
/******************************************************************/

/* initialize the event timing loop before we start monitoring */
void init_timing_loop(void){
	host *temp_host;
	void *host_cursor;
	service *temp_service;
	time_t current_time;
	timed_event *new_event;
	time_t run_time;
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
	move_first_service();
	while(temp_service=get_next_service()){

		schedule_check=TRUE;

		/* service has no check interval */
		if(temp_service->check_interval==0)
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
	move_first_host();
	while(temp_host=get_next_host()){

		schedule_check=TRUE;

		/* host has no check interval */
		if(temp_host->check_interval==0)
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
	scheduling_info.max_service_check_spread=DEFAULT_SERVICE_CHECK_SPREAD;

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

	/* add service checks as separate events (with interleaving to minimize remote load) */
	current_interleave_block=0;
	move_first_service();
	temp_service=get_next_service();
	while(temp_service!=NULL){

#ifdef DEBUG1
		printf("\tCurrent Interleave Block: %d\n",current_interleave_block);
#endif

		for(interleave_block_index=0;interleave_block_index<scheduling_info.service_interleave_factor && temp_service!=NULL;temp_service=get_next_service(),interleave_block_index++){

			/* skip this service if it shouldn't be scheduled */
			if(temp_service->should_be_scheduled==FALSE)
				continue;

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

			/* create a new service check event */
			new_event=malloc(sizeof(timed_event));
			if(new_event!=NULL){
				new_event->event_type=EVENT_SERVICE_CHECK;
				new_event->event_data=(void *)temp_service;
				new_event->run_time=temp_service->next_check;

			        /* service checks really are recurring, but are rescheduled manually - not automatically */
				new_event->recurring=FALSE;

				schedule_event(new_event,&event_list_low);
			        }

		        }

		current_interleave_block++;
	        }


	/******** DETERMINE HOST SCHEDULING PARAMS  ********/

	scheduling_info.first_host_check=(time_t)0L;
	scheduling_info.last_host_check=(time_t)0L;

	/* default max host check spread (in minutes) */
	scheduling_info.max_host_check_spread=DEFAULT_HOST_CHECK_SPREAD;

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

	/* add host checks as seperate events */
	mult_factor=0;
	move_first_host();
	while(temp_host=get_next_host()){

		/* skip hosts that shouldn't be scheduled */
		if(temp_host->should_be_scheduled==FALSE)
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

		/* create a new host check event */
		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_HOST_CHECK;
			new_event->event_data=(void *)temp_host;
			new_event->run_time=temp_host->next_check;

			/* host checks really are recurring, but are rescheduled manually */
			new_event->recurring=FALSE;

			schedule_event(new_event,&event_list_low);
		        }

		mult_factor++;
	        }
	

	/******** SCHEDULE MISC EVENTS ********/

	/* add a service check reaper event */
	new_event=malloc(sizeof(timed_event));
	if(new_event!=NULL){
		new_event->event_type=EVENT_SERVICE_REAPER;
		new_event->event_data=(void *)NULL;
		new_event->run_time=current_time;
		new_event->recurring=TRUE;
		schedule_event(new_event,&event_list_high);
	        }

	/* add an orphaned service check event */
	if(check_orphaned_services==TRUE){
		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_ORPHAN_CHECK;
			new_event->event_data=(void *)NULL;
			new_event->run_time=current_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
	                }
	        }

	/* add a service result "freshness" check event */
	if(check_service_freshness==TRUE){
		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_FRESHNESS_CHECK;
			new_event->event_data=(void *)NULL;
			new_event->run_time=current_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
	                }
	        }

	/* add a status save event */
	if(aggregate_status_updates==TRUE){
		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_STATUS_SAVE;
			new_event->event_data=(void *)NULL;
			new_event->run_time=current_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
	                }
	        }

	/* add an external command check event if needed */
	if(check_external_commands==TRUE){
		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_COMMAND_CHECK;
			new_event->event_data=(void *)NULL;
			new_event->run_time=current_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
		        }
	        }

	/* add a log rotation event if necessary */
	if(log_rotation_method!=LOG_ROTATION_NONE){

		/* get the next time to run the log rotation */
		get_next_log_rotation_time(&run_time);

		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_LOG_ROTATION;
			new_event->event_data=(void *)NULL;
			new_event->run_time=run_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
		        }
	        }

	/* add a retention data save event if needed */
	if(retain_state_information==TRUE && retention_update_interval>0){
		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_RETENTION_SAVE;
			new_event->event_data=(void *)NULL;
			new_event->run_time=current_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
		        }
	        }

#ifdef DEBUG0
	printf("init_timing_loop() end\n");
#endif

	return;
        }



/* displays service check scheduling information */
void display_scheduling_info(void){
	float minimum_concurrent_checks;

	printf("Projected scheduling information for host and service\n");
	printf("checks is listed below.  This information assumes that\n");
	printf("you are going to start running Nagios with your current\n");
	printf("config files.\n\n");

	printf("HOST SCHEDULING INFORMATION\n");
	printf("---------------------------\n");
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
	printf("Total hosts:                        %d\n",scheduling_info.total_hosts);

	printf("Service Inter-check delay method:   ");
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
	printf("Max concurrent service checks:      %d\n",max_parallel_service_checks);
	printf("\n\n");

	printf("PERFORMANCE SUGGESTIONS\n");
	printf("-----------------------\n");

	/* assume a 100% service check burst for max concurrent checks */
	if(scheduling_info.service_inter_check_delay==0.0)
		minimum_concurrent_checks=ceil(service_check_reaper_interval*2.0);
	minimum_concurrent_checks=ceil((service_check_reaper_interval*2.0)/scheduling_info.service_inter_check_delay);
	printf("Minimum value for 'max_concurrent_checks':   %d\n",(int)minimum_concurrent_checks);
	printf("\n");

	return;
        }


/* schedule an event in order of execution time */
void schedule_event(timed_event *event,timed_event **event_list){
	timed_event *temp_event;
	timed_event *first_event;
	host *temp_host;
	service *temp_service;
	time_t run_time;

#ifdef DEBUG0
	printf("schedule_event() start\n");
#endif

	/* if this event is a service check... */
	if(event->event_type==EVENT_SERVICE_CHECK){
		temp_service=(service *)event->event_data;
		event->run_time=temp_service->next_check;
	        }

	/* if this event is a host check... */
	else if(event->event_type==EVENT_HOST_CHECK){
		temp_host=(host *)event->event_data;
		event->run_time=temp_host->next_check;
	        }

	/* if this is an external command check event ... */
	else if(event->event_type==EVENT_COMMAND_CHECK){

		/* we're supposed to check as often as possible, so we schedule regular checks at 60 second intervals */
		if(command_check_interval==-1)
			event->run_time=event->run_time+60;

		/* else schedule external command checks at user-specified intervals */
		else
			event->run_time=event->run_time+(command_check_interval);
	        }

	/* if this is a log rotation event... */
	else if(event->event_type==EVENT_LOG_ROTATION){
		get_next_log_rotation_time(&run_time);
		event->run_time=run_time;
	        }

	/* if this is a service check reaper event... */
	else if(event->event_type==EVENT_SERVICE_REAPER)
		event->run_time=event->run_time+service_check_reaper_interval;

	/* if this is a orphaned service check event... */
	else if(event->event_type==EVENT_ORPHAN_CHECK)
		event->run_time=event->run_time+(service_check_timeout*2);

	/* if this is a service result freshness check */
	else if(event->event_type==EVENT_FRESHNESS_CHECK)
		event->run_time=event->run_time+freshness_check_interval;

	/* if this is a state retention save event... */
	else if(event->event_type==EVENT_RETENTION_SAVE)
		event->run_time=event->run_time+(retention_update_interval*60);

	/* if this is a status save event... */
	else if(event->event_type==EVENT_STATUS_SAVE)
		event->run_time=event->run_time+(status_update_interval);

	/* if this is a scheduled program shutdown or restart, we already know the time... */

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
	broker_timed_event(NEBTYPE_TIMEDEVENT_ADD,NEBFLAG_NONE,NEBATTR_NONE,event,NULL,NULL);
#endif

#ifdef DEBUG0
	printf("schedule_event() end\n");
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
	broker_timed_event(NEBTYPE_TIMEDEVENT_REMOVE,NEBFLAG_NONE,NEBATTR_NONE,event,NULL,NULL);
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
	time_t last_time;
	time_t current_time;
	int run_event=TRUE;
	host *temp_host;
	service *temp_service;
	struct timespec delay;

#ifdef DEBUG0
	printf("event_execution_loop() start\n");
#endif

	time(&last_time);

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
				schedule_event(temp_event,&event_list_high);

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
				temp_service->latency=(unsigned long)(current_time-event_list_low->run_time);

				/* don't run a service check if we're not supposed to right now */
				if(execute_service_checks==FALSE && !(temp_service->check_options & CHECK_OPTION_FORCE_EXECUTION)){
#ifdef DEBUG3
					printf("\tWe're not executing service checks right now...\n");
#endif

					/* remove the service check from the event queue and reschedule it for a later time */
					temp_event=event_list_low;
					event_list_low=event_list_low->next;
					if(temp_service->state_type==SOFT_STATE && temp_service->current_state!=STATE_OK)
						temp_service->next_check=(time_t)(temp_service->next_check+(temp_service->retry_interval*interval_length));
					else
						temp_service->next_check=(time_t)(temp_service->next_check+(temp_service->check_interval*interval_length));
					schedule_event(temp_event,&event_list_low);
					update_service_status(temp_service,FALSE);

					run_event=FALSE;
				        }

				/* dont' run a service check if we're already maxed out on the number of parallel service checks...  */
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

			/* run a few checks before executing ahost check... */
			if(event_list_low->event_type==EVENT_HOST_CHECK){

				temp_host=(host *)event_list_low->event_data;

				/* update host check latency */
				temp_host->latency=(unsigned long)(current_time-event_list_low->run_time);

				/* don't run a host check if we're not supposed to right now */
				if(execute_host_checks==FALSE && !(temp_host->check_options & CHECK_OPTION_FORCE_EXECUTION)){
#ifdef DEBUG3
					printf("\tWe're not executing host checks right now...\n");
#endif

					/* remove the host check from the event queue and reschedule it for a later time */
					temp_event=event_list_low;
					event_list_low=event_list_low->next;
					temp_host->next_check=(time_t)(temp_host->next_check+(temp_host->check_interval*interval_length));
					schedule_event(temp_event,&event_list_low);
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
					schedule_event(temp_event,&event_list_low);

				/* else free memory associated with the event */
				else
					free(temp_event);
			        }

			/* wait a second so we don't hog the CPU... */
			else{
#ifdef USE_EVENT_BROKER
#ifdef INSANE_BROKERING
				/* send event data to broker */
				broker_timed_event(NEBTYPE_TIMEDEVENT_DELAY,NEBFLAG_NONE,NEBATTR_NONE,temp_event,NULL,NULL);
#endif
#endif
				delay.tv_sec=(time_t)sleep_time;
				delay.tv_nsec=(long)((sleep_time-(double)delay.tv_sec)*1000000000);
				nanosleep(&delay,NULL);
			        }
		        }

		/* we don't have anything to do at this moment in time */
		else{

			/* check for external commands if we're supposed to check as often as possible */
			if(command_check_interval==-1)
				check_for_external_commands();

			/* wait a second so we don't hog the CPU... */
#ifdef USE_EVENT_BROKER
#ifdef INSANE_BROKERING
			/* send event data to broker */
			broker_timed_event(NEBTYPE_TIMEDEVENT_SLEEP,NEBFLAG_NONE,NEBATTR_NONE,NULL,(void *)&sleep_time,NULL);
#endif
#endif
			delay.tv_sec=(time_t)sleep_time;
			delay.tv_nsec=(long)((sleep_time-(double)delay.tv_sec)*1000000000);
			nanosleep(&delay,NULL);
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

#ifdef DEBUG0
	printf("handle_timed_event() start\n");
#endif

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_timed_event(NEBTYPE_TIMEDEVENT_EXECUTE,NEBFLAG_NONE,NEBATTR_NONE,event,NULL,NULL);
#endif

#ifdef DEBUG3
	printf("*** Event Details ***\n");
	printf("\tEvent type: %d ",event->event_type);

	if(event->event_type==EVENT_SERVICE_CHECK){
		printf("(service check)\n");
		temp_service=(service *)event->event_data;
		printf("\t\tService Description: %s\n",temp_service->description);
		printf("\t\tAssociated Host:     %s\n",temp_service->host_name);
	        }

	else if(event->event_type==EVENT_HOST_CHECK){
		printf("(host check)\n");
		temp_host=(host *)event->event_data;
		printf("\t\tHost:     %s\n",temp_host->name);
	        }

	else if(event->event_type==EVENT_COMMAND_CHECK)
		printf("(external command check)\n");

	else if(event->event_type==EVENT_LOG_ROTATION)
		printf("(log file rotation)\n");

	else if(event->event_type==EVENT_PROGRAM_SHUTDOWN)
		printf("(program shutdown)\n");

	else if(event->event_type==EVENT_PROGRAM_RESTART)
		printf("(program restart)\n");

	else if(event->event_type==EVENT_SERVICE_REAPER)
		printf("(service check reaper)\n");

	else if(event->event_type==EVENT_ORPHAN_CHECK)
		printf("(orphaned service check)\n");

	else if(event->event_type==EVENT_RETENTION_SAVE)
		printf("(retention save)\n");

	else if(event->event_type==EVENT_STATUS_SAVE)
		printf("(status save)\n");

	else if(event->event_type==EVENT_SCHEDULED_DOWNTIME)
		printf("(scheduled downtime)\n");

	else if(event->event_type==EVENT_FRESHNESS_CHECK)
		printf("(service result freshness check)\n");

	else if(event->event_type==EVENT_EXPIRE_DOWNTIME)
		printf("(expire downtime)\n");

	printf("\tEvent time: %s",ctime(&event->run_time));
#endif
		
	/* how should we handle the event? */
	switch(event->event_type){

	case EVENT_SERVICE_CHECK:

		/* run  a service check */
		temp_service=(service *)event->event_data;
		run_service_check(temp_service);
		break;

	case EVENT_HOST_CHECK:

		/* run a host check */
		temp_host=(host *)event->event_data;
		run_scheduled_host_check(temp_host);
		break;

	case EVENT_COMMAND_CHECK:

		/* check for external commands */
		check_for_external_commands();
		break;

	case EVENT_LOG_ROTATION:

		/* rotate the log file */
		rotate_log_file(event->run_time);
		break;

	case EVENT_PROGRAM_SHUTDOWN:
		
		/* set the shutdown flag */
		sigshutdown=TRUE;

		/* log the shutdown */
		snprintf(temp_buffer,sizeof(temp_buffer),"PROGRAM_SHUTDOWN event encountered, shutting down...\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO,TRUE);
		break;

	case EVENT_PROGRAM_RESTART:

		/* set the restart flag */
		sigrestart=TRUE;

		/* log the restart */
		snprintf(temp_buffer,sizeof(temp_buffer),"PROGRAM_RESTART event encountered, restarting...\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO,TRUE);
		break;

	case EVENT_SERVICE_REAPER:

		/* reap service check results */
		reap_service_checks();
		break;

	case EVENT_ORPHAN_CHECK:

		/* check for orphaned services */
		check_for_orphaned_services();
		break;

	case EVENT_RETENTION_SAVE:

		/* save state retention data */
		save_state_information(config_file,TRUE);
		break;

	case EVENT_STATUS_SAVE:

		/* save all status data (program, host, and service) */
		update_all_status_data();
		break;

	case EVENT_SCHEDULED_DOWNTIME:

		/* process scheduled downtime info */
		handle_scheduled_downtime((scheduled_downtime *)event->event_data);
		break;

	case EVENT_FRESHNESS_CHECK:
		
		/* check service result freshness */
		check_service_result_freshness();
		break;

	case EVENT_EXPIRE_DOWNTIME:

		/* check for expired scheduled downtime entries */
		check_for_expired_downtime();

	default:

		break;
	        }

#ifdef DEBUG0
	printf("handle_timed_event() end\n");
#endif

	return OK;
        }



/* attempts to compensate for a change in the system time */
void compensate_for_system_time_change(unsigned long last_time,unsigned long current_time){
	char buffer[MAX_INPUT_BUFFER];
	unsigned long time_difference;
	timed_event *temp_event;
	service *temp_service;
	host *temp_host;
	void *host_cursor;

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

		/* we moved back in time... */
		if(last_time>current_time){

			/* we can't precede the UNIX epoch */
			if(time_difference>(unsigned long)temp_event->run_time)
				temp_event->run_time=(time_t)0;
			else
				temp_event->run_time=(time_t)(temp_event->run_time-(time_t)time_difference);
		        }

		/* we moved into the future... */
		else
			temp_event->run_time=(time_t)(temp_event->run_time+(time_t)time_difference);
	        }

	/* adjust the next run time for all low priority timed events */
	for(temp_event=event_list_low;temp_event!=NULL;temp_event=temp_event->next){

		/* we moved back in time... */
		if(last_time>current_time){

			/* we can't precede the UNIX epoch */
			if(time_difference>(unsigned long)temp_event->run_time)
				temp_event->run_time=(time_t)0;
			else
				temp_event->run_time=(time_t)(temp_event->run_time-(time_t)time_difference);
		        }

		/* we moved into the future... */
		else
			temp_event->run_time=(time_t)(temp_event->run_time+(time_t)time_difference);
	        }

	/* adjust the last notification time for all services */
	move_first_service();
	while(temp_service=get_next_service()){

		if(temp_service->last_notification==(time_t)0)
			continue;

		/* we moved back in time... */
		if(last_time>current_time){

			/* we can't precede the UNIX epoch */
			if(time_difference>(unsigned long)temp_service->last_notification)
				temp_service->last_notification=(time_t)0;
			else
				temp_service->last_notification=(time_t)(temp_service->last_notification-(time_t)time_difference);
		        }

		/* we moved into the future... */
		else
			temp_service->last_notification=(time_t)(temp_service->last_notification+(time_t)time_difference);

		/* update the status data */
		update_service_status(temp_service,FALSE);
	        }

	/* adjust the next check time for all services */
	move_first_service();
	while(temp_service=get_next_service()){

		if(temp_service->next_check==(time_t)0)
			continue;

		/* we moved back in time... */
		if(last_time>current_time){

			/* we can't precede the UNIX epoch */
			if(time_difference>(unsigned long)temp_service->next_check)
				temp_service->next_check=(time_t)0;
			else
				temp_service->next_check=(time_t)(temp_service->next_check-(time_t)time_difference);
		        }

		/* we moved into the future... */
		else
			temp_service->next_check=(time_t)(temp_service->next_check+(time_t)time_difference);

		/* update the status data */
		update_service_status(temp_service,FALSE);
	        }

	/* adjust the last notification time for all hosts */
	host_cursor=get_host_cursor();
	while(temp_host=get_next_host_cursor(host_cursor)){

		if(temp_host->last_host_notification==(time_t)0)
			continue;

		/* we moved back in time... */
		if(last_time>current_time){

			/* we can't precede the UNIX epoch */
			if(time_difference>(unsigned long)temp_host->last_host_notification)
				temp_host->last_host_notification=(time_t)0;
			else
				temp_host->last_host_notification=(time_t)(temp_host->last_host_notification-(time_t)time_difference);
		        }

		/* we moved into the future... */
		else
			temp_host->last_host_notification=(time_t)(temp_host->last_host_notification+(time_t)time_difference);

		/* update the status data */
		update_host_status(temp_host,FALSE);
	        }
	free_host_cursor(host_cursor);

	/* adjust the next check time for all hosts */
	host_cursor=get_host_cursor();
	while(temp_host=get_next_host_cursor(host_cursor)){

		if(temp_host->next_check==(time_t)0)
			continue;

		/* we moved back in time... */
		if(last_time>current_time){

			/* we can't precede the UNIX epoch */
			if(time_difference>(unsigned long)temp_host->next_check)
				temp_host->next_check=(time_t)0;
			else
				temp_host->next_check=(time_t)(temp_host->next_check-(time_t)time_difference);
		        }

		/* we moved into the future... */
		else
			temp_host->next_check=(time_t)(temp_host->next_check+(time_t)time_difference);

		/* update the status data */
		update_host_status(temp_host,FALSE);
	        }

	/* adjust program start time (necessary for state stats calculations) */
	/* we moved back in time... */
	if(last_time>current_time){

		/* we can't precede the UNIX epoch */
		if(time_difference>(unsigned long)program_start)
			program_start=(time_t)0;
		else
			program_start=(time_t)(program_start-(time_t)time_difference);
	        }

	/* we moved into the future... */
	else
		program_start=(time_t)(program_start+(time_t)time_difference);

	/* update status data */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("compensate_for_system_time_change() end\n");
#endif

	return;
        }
