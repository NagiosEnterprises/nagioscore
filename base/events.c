/*****************************************************************************
 *
 * EVENTS.C - Timed event functions for Nagios
 *
 * Copyright (c) 1999-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   01-01-2003
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
extern int      inter_check_delay_method;
extern int      interleave_factor_method;

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

extern int      time_change_threshold;

extern int      non_parallelized_check_running;

timed_event *event_list_low=NULL;
timed_event *event_list_high=NULL;

sched_info scheduling_info;



/******************************************************************/
/************ EVENT SCHEDULING/HANDLING FUNCTIONS *****************/
/******************************************************************/


/* calculate the inter check delay to use when initially scheduling service checks */
void calculate_inter_check_delay(void){
	service *temp_service;

#ifdef DEBUG0
	printf("calculate_inter_check_delay() start\n");
#endif

	/* how should we determine the inter-check delay to use? */
	switch(inter_check_delay_method){

	case ICD_NONE:

		/* don't spread checks out - useful for testing parallelization code */
		scheduling_info.inter_check_delay=0.0;
		break;

	case ICD_DUMB:

		/* be dumb and just schedule checks 1 second apart */
		scheduling_info.inter_check_delay=1.0;
		break;
		
	case ICD_USER:

		/* the user specified a delay, so don't try to calculate one */
		break;

	case ICD_SMART:
	default:

		/* be smart and calculate the best delay to use to minimize CPU load... */

		scheduling_info.inter_check_delay=0.0;
		scheduling_info.check_interval_total=0L;

		scheduling_info.total_services=0;
		move_first_service();
		while(temp_service=get_next_service()){
			scheduling_info.total_services++;
			scheduling_info.check_interval_total+=temp_service->check_interval;
		        }

		if(scheduling_info.total_services==0 || scheduling_info.check_interval_total==0)
			return;

		/* adjust the check interval total to correspond to the interval length */
		scheduling_info.check_interval_total=(scheduling_info.check_interval_total*interval_length);

		/* calculate the average check interval for services */
		scheduling_info.average_check_interval=(double)((double)scheduling_info.check_interval_total/(double)scheduling_info.total_services);

		/* calculate the average inter check delay (in seconds) needed to evenly space the service checks out */
		scheduling_info.average_inter_check_delay=(double)(scheduling_info.average_check_interval/(double)scheduling_info.total_services);

		/* set the global inter check delay value */
		scheduling_info.inter_check_delay=scheduling_info.average_inter_check_delay;

#ifdef DEBUG1
		printf("\tTotal service checks:    %d\n",scheduling_info.total_services);
		printf("\tCheck interval total:    %lu\n",scheduling_info.check_interval_total);
		printf("\tAverage check interval:  %0.2f sec\n",scheduling_info.average_check_interval);
		printf("\tInter-check delay:       %0.2f sec\n",scheduling_info.inter_check_delay);
#endif
	        }

#ifdef DEBUG0
	printf("calculate_inter_check_delay() end\n");
#endif

	return;
        }



/* calculate the interleave factor for spreading out service checks */
void calculate_interleave_factor(void){

#ifdef DEBUG0
	printf("calculate_interleave_factor() start\n");
#endif

	/* how should we determine the interleave factor? */
	switch(interleave_factor_method){

	case ILF_USER:

		/* the user supplied a value, so don't do any calculation */
		break;


	case ILF_SMART:
	default:


		/* count the number of service we have */
		scheduling_info.total_services=0;
		move_first_service();
		while(get_next_service()){
			scheduling_info.total_services++;
		        }

		/* count the number of hosts we have */
		scheduling_info.total_hosts=0;
		for(move_first_host();get_next_host();scheduling_info.total_hosts++)
			;

		/* protect against a divide by zero problem - shouldn't happen, but just in case... */
		if(scheduling_info.total_hosts==0)
			scheduling_info.total_hosts=1;

		scheduling_info.average_services_per_host=(double)((double)scheduling_info.total_services/(double)scheduling_info.total_hosts);
		scheduling_info.interleave_factor=(int)(ceil(scheduling_info.average_services_per_host));

		/* temporary hack for Alpha systems, not sure why calculation results in a 0... */
		if(scheduling_info.interleave_factor==0)
			scheduling_info.interleave_factor=(int)(scheduling_info.total_services/scheduling_info.total_hosts);

#ifdef DEBUG1
		printf("\tTotal service checks:    %d\n",scheduling_info.total_services);
		printf("\tTotal hosts:             %d\n",scheduling_info.total_hosts);
		printf("\tInterleave factor:       %d\n",scheduling_info.interleave_factor);
#endif
	        }

#ifdef DEBUG0
	printf("calculate_interleave_factor() end\n");
#endif

	return;
        }



/* initialize the event timing loop before we start monitoring */
void init_timing_loop(void){
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

#ifdef DEBUG0
	printf("init_timing_loop() start\n");
#endif

	/* get the time right now */
	time(&current_time);

	/* calculate number of interleave blocks */
	if(scheduling_info.interleave_factor==0)
		total_interleave_blocks=scheduling_info.total_services;
	else
		total_interleave_blocks=(int)ceil((double)scheduling_info.total_services/(double)scheduling_info.interleave_factor);

	scheduling_info.first_service_check=(time_t)0L;
	scheduling_info.last_service_check=(time_t)0L;

#ifdef DEBUG1
	printf("Total services: %d\n",scheduling_info.total_services);
	printf("Interleave factor: %d\n",scheduling_info.interleave_factor);
	printf("Total interleave blocks: %d\n",total_interleave_blocks);
	printf("Inter-check delay: %2.1f\n",scheduling_info.inter_check_delay);
#endif

	/* add all service checks as separate events (with interleaving) */
	current_interleave_block=1;
	move_first_service();

	temp_service=get_next_service();
	while(temp_service) {
		current_interleave_block++;

#ifdef DEBUG1
		printf("\tCurrent Interleave Block: %d\n",current_interleave_block);
#endif

		for(interleave_block_index=0; interleave_block_index<scheduling_info.interleave_factor; interleave_block_index++, temp_service=get_next_service()) {
			if(temp_service==NULL)
				break;

			mult_factor=current_interleave_block+(interleave_block_index*total_interleave_blocks);

#ifdef DEBUG1
			printf("\t\tInterleave Block Index: %d\n",interleave_block_index);
			printf("\t\tMult factor: %d\n",mult_factor);
#endif

			/* set the next check time for the service */
			temp_service->next_check=(time_t)(current_time+(mult_factor*scheduling_info.inter_check_delay));

			/* make sure the service can actually be scheduled */
			is_valid_time=check_time_against_period(temp_service->next_check,temp_service->check_period);
			if(is_valid_time==ERROR){

				/* the initial time didn't work out, so try and find a next valid time for running the check */
				get_next_valid_time(temp_service->next_check,&next_valid_time,temp_service->check_period);

				/* whoa!  we couldn't find a next valid time, so don't schedule the check at all */
				if(temp_service->next_check==next_valid_time)
					temp_service->should_be_scheduled=FALSE;
			        }

#ifdef DEBUG1
			printf("\t\tService '%s' on host '%s'\n",temp_service->description,temp_service->host_name);
			if(temp_service->should_be_scheduled==TRUE)
				printf("\t\tNext Check: %lu --> %s",(unsigned long)temp_service->next_check,ctime(&temp_service->next_check));
			else
				printf("\t\tService check should *not* be scheduled!\n");
#endif

			if(scheduling_info.first_service_check==(time_t)0 || (temp_service->next_check<scheduling_info.first_service_check))
				scheduling_info.first_service_check=temp_service->next_check;
			if(temp_service->next_check > scheduling_info.last_service_check)
				scheduling_info.last_service_check=temp_service->next_check;

			/* create a new service check event if we should schedule this check */
			if(temp_service->should_be_scheduled==TRUE){

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
		        }

	        }

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

	printf("\n\tSERVICE SCHEDULING INFORMATION\n");
	printf("\t-------------------------------\n");
	printf("\tTotal services:             %d\n",scheduling_info.total_services);
	printf("\tTotal hosts:                %d\n",scheduling_info.total_hosts);
	printf("\n");

	printf("\tCommand check interval:     %d sec\n",command_check_interval);
	printf("\tCheck reaper interval:      %d sec\n",service_check_reaper_interval);
	printf("\n");

	printf("\tInter-check delay method:   ");
	if(inter_check_delay_method==ICD_NONE)
		printf("NONE\n");
	else if(inter_check_delay_method==ICD_DUMB)
		printf("DUMB\n");
	else if(inter_check_delay_method==ICD_SMART){
		printf("SMART\n");
		printf("\tAverage check interval:     %2.3f sec\n",scheduling_info.average_check_interval);
	        }
	else
		printf("USER-SUPPLIED VALUE\n");
	printf("\tInter-check delay:          %2.3f sec\n",scheduling_info.inter_check_delay);
	printf("\n");

	printf("\tInterleave factor method:   %s\n",(interleave_factor_method==ILF_USER)?"USER-SUPPLIED VALUE":"SMART");
	if(interleave_factor_method==ILF_SMART)
		printf("\tAverage services per host:  %2.3f\n",scheduling_info.average_services_per_host);
	printf("\tService interleave factor:  %d\n",scheduling_info.interleave_factor);
	printf("\n");

	printf("\tInitial service check scheduling info:\n");
	printf("\t--------------------------------------\n");
	printf("\tFirst scheduled check:      %s",ctime(&scheduling_info.first_service_check));
	printf("\tLast scheduled check:       %s",ctime(&scheduling_info.last_service_check));
	printf("\n");
	printf("\tRough guidelines for max_concurrent_checks value:\n");
	printf("\t-------------------------------------------------\n");

	minimum_concurrent_checks=ceil((float)service_check_reaper_interval/scheduling_info.inter_check_delay);

	printf("\tAbsolute minimum value:     %d\n",(int)minimum_concurrent_checks);
	printf("\tRecommend value:            %d\n",(int)(minimum_concurrent_checks*3.0));
	printf("\n");
	printf("\tNotes:\n");
	printf("\tThe recommendations for the max_concurrent_checks value\n");
	printf("\tassume that the average execution time for service\n");
	printf("\tchecks is less than the service check reaper interval.\n");
	printf("\tThe minimum value also reflects best case scenarios\n");
	printf("\twhere there are no problems on your network.  You will\n");
	printf("\thave to tweak this value as necessary after testing.\n");
	printf("\tHigh latency values for checks are often indicative of\n");
	printf("\tthe max_concurrent_checks value being set too low and/or\n");
	printf("\tthe service_reaper_frequency being set too high.\n");
	printf("\tIt is important to note that the values displayed above\n");
	printf("\tdo not reflect current performance information for any\n");
	printf("\tNagios process that may currently be running.  They are\n");
	printf("\tprovided solely to project expected and recommended\n");
	printf("\tvalues based on the current data in the config files.\n");
	printf("\n");

	return;
        }


/* schedule an event in order of execution time */
void schedule_event(timed_event *event,timed_event **event_list){
	timed_event *temp_event;
	timed_event *first_event;
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
		printf("Current/Max Outstanding Checks: %d/%d\n",currently_running_service_checks,max_parallel_service_checks);
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

			/* run the event except if it is a service check and we already are maxed out on number or parallel service checks... */
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
				sleep((unsigned int)sleep_time);
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
	service *temp_service;
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
