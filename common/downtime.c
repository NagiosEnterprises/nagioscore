/*****************************************************************************
 *
 * DOWNTIME.C - Scheduled downtime functions for Nagios
 *
 * Copyright (c) 2000-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   08-05-2003
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

#include "config.h"
#include "common.h"
#include "comments.h"
#include "downtime.h"
#include "objects.h"
#include "statusdata.h"

/***** IMPLEMENTATION-SPECIFIC INCLUDES *****/

#ifdef USE_XDDDEFAULT
#include "../xdata/xdddefault.h"
#endif

#ifdef NSCORE
#include "../base/nagios.h"
#include "../base/broker.h"
#endif

#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif


scheduled_downtime *scheduled_downtime_list=NULL;

#ifdef NSCORE
extern timed_event *event_list_high;
#endif



#ifdef NSCORE

/******************************************************************/
/**************** INITIALIZATION/CLEANUP FUNCTIONS ****************/
/******************************************************************/


/* initalizes scheduled downtime data */
int initialize_downtime_data(char *config_file){
	int result;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XDDDEFAULT
	result=xdddefault_initialize_downtime_data(config_file);
#endif
#ifdef USE_XDDDB
	result=xdddb_initialize_downtime_data(config_file);
#endif

	return result;
        }


/* cleans up scheduled downtime data */
int cleanup_downtime_data(char *config_file){
	int result;
	
	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XDDDEFAULT
	result=xdddefault_cleanup_downtime_data(config_file);
#endif
#ifdef USE_XDDDB
	result=xdddb_cleanup_downtime_data(config_file);
#endif

	/* free memory allocated to downtime data */
	free_downtime_data();

	return result;
        }


/******************************************************************/
/********************** SCHEDULING FUNCTIONS **********************/
/******************************************************************/

/* schedules a host or service downtime */
int schedule_downtime(int type, char *host_name, char *service_description, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *new_downtime_id){
	unsigned long downtime_id;

	/* don't add old or invalid downtimes */
	if(start_time>=end_time || end_time<=time(NULL))
		return ERROR;

	/* add a new downtime entry */
	add_new_downtime(type,host_name,service_description,entry_time,author,comment,start_time,end_time,fixed,triggered_by,duration,&downtime_id);

	/* register the scheduled downtime */
	register_downtime(type,downtime_id);

	/* return downtime id */
	if(new_downtime_id!=NULL)
		*new_downtime_id=downtime_id;

	return OK;
        }


/* unschedules a host or service downtime */
int unschedule_downtime(int type,unsigned long downtime_id){
	scheduled_downtime *temp_downtime;
	scheduled_downtime *event_downtime;
	host *hst=NULL;
	service *svc=NULL;
	timed_event *temp_event;
	char temp_buffer[MAX_INPUT_BUFFER];
#ifdef USE_EVENT_BROKER
	int attr;
#endif
	

	/* find the downtime entry in the list in memory */
	temp_downtime=find_downtime(type,downtime_id);
	if(temp_downtime==NULL)
		return ERROR;

	/* find the host or service associated with this downtime */
	if(temp_downtime->type==HOST_DOWNTIME){
		hst=find_host(temp_downtime->host_name);
		if(hst==NULL)
			return ERROR;
	        }
	else{
		svc=find_service(temp_downtime->host_name,temp_downtime->service_description);
		if(svc==NULL)
			return ERROR;
	        }

	/* decrement pending flex downtime if necessary ... */
	if(temp_downtime->fixed==FALSE && temp_downtime->incremented_pending_downtime==TRUE){
		if(temp_downtime->type==HOST_DOWNTIME)
			hst->pending_flex_downtime--;
		else
			svc->pending_flex_downtime--;
	        }

	/* decrement the downtime depth variable and update status data if necessary */
	if(temp_downtime->is_in_effect==TRUE){

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		attr=(temp_downtime->type==HOST_DOWNTIME)?NEBATTR_HOST_DOWNTIME:NEBATTR_SERVICE_DOWNTIME;
		attr|=NEBATTR_DOWNTIME_STOP_CANCELLED;
		broker_downtime_data(NEBTYPE_DOWNTIME_STOP,NEBFLAG_NONE,attr,temp_downtime->host_name,temp_downtime->service_description,temp_downtime->entry_time,temp_downtime->author,temp_downtime->comment,temp_downtime->start_time,temp_downtime->end_time,temp_downtime->fixed,temp_downtime->duration,temp_downtime->downtime_id,NULL);
#endif

		if(temp_downtime->type==HOST_DOWNTIME){

			hst->scheduled_downtime_depth--;
			update_host_status(hst,FALSE);

			/* log a notice - this is parsed by the history CGI */
			if(hst->scheduled_downtime_depth==0){
				snprintf(temp_buffer,sizeof(temp_buffer),"HOST DOWNTIME ALERT: %s;CANCELLED; Scheduled downtime for host has been cancelled.\n",hst->name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_INFO_MESSAGE,FALSE);
			        }
		        }

		else{

			svc->scheduled_downtime_depth--;
			update_service_status(svc,FALSE);

			/* log a notice - this is parsed by the history CGI */
			if(svc->scheduled_downtime_depth==0){
				snprintf(temp_buffer,sizeof(temp_buffer),"SERVICE DOWNTIME ALERT: %s;%s;CANCELLED; Scheduled downtime for service has been cancelled.\n",svc->host_name,svc->description);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_INFO_MESSAGE,FALSE);
			        }
		        }
	        }

	/* remove scheduled entry from event queue */
	for(temp_event=event_list_high;temp_event!=NULL;temp_event=temp_event->next){
		if(temp_event->event_type!=EVENT_SCHEDULED_DOWNTIME)
			continue;
		event_downtime=(scheduled_downtime *)temp_event->event_data;
		if(event_downtime->type!=temp_downtime->type)
			continue;
		if(event_downtime->downtime_id==downtime_id)
			break;
	        }
	if(temp_event!=NULL)
		remove_event(temp_event,&event_list_high);

	/* delete downtime entry */
	if(temp_downtime->type==HOST_DOWNTIME)
		delete_host_downtime(downtime_id);
	else
		delete_service_downtime(downtime_id);

	/* unschedule all downtime entries that were triggered by this one */
	for(temp_downtime=scheduled_downtime_list;temp_downtime!=NULL;temp_downtime=temp_downtime->next){
		if(temp_downtime->triggered_by==downtime_id)
			unschedule_downtime(ANY_DOWNTIME,downtime_id);
	        }

	return OK;
        }



/* registers scheduled downtime (schedules it, adds comments, etc.) */
int register_downtime(int type, unsigned long downtime_id){
 	char temp_buffer[MAX_INPUT_BUFFER];
	char start_time_string[MAX_DATETIME_LENGTH];
	char end_time_string[MAX_DATETIME_LENGTH];
	scheduled_downtime *temp_downtime;
	timed_event *new_event;
	host *hst=NULL;
	service *svc=NULL;
	char *type_string="";
	int hours;
	int minutes;

	/* find the downtime entry in memory */
	temp_downtime=find_downtime(type,downtime_id);
	if(temp_downtime==NULL)
		return ERROR;

	/* find the host or service associated with this downtime */
	if(temp_downtime->type==HOST_DOWNTIME){
		hst=find_host(temp_downtime->host_name);
		if(hst==NULL)
			return ERROR;
	        }
	else{
		svc=find_service(temp_downtime->host_name,temp_downtime->service_description);
		if(svc==NULL)
			return ERROR;
	        }

	/* create the comment */
	get_datetime_string(&(temp_downtime->start_time),start_time_string,MAX_DATETIME_LENGTH,SHORT_DATE_TIME);
	get_datetime_string(&(temp_downtime->end_time),end_time_string,MAX_DATETIME_LENGTH,SHORT_DATE_TIME);
	hours=temp_downtime->duration/3600;
	minutes=((temp_downtime->duration-(hours*3600))/60);
	if(temp_downtime->type==HOST_DOWNTIME)
		type_string="host";
	else
		type_string="service";
	if(temp_downtime->fixed==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"This %s has been scheduled for fixed downtime from %s to %s.  Notifications for the %s will not be sent out during that time period.",type_string,start_time_string,end_time_string,type_string);
	        }
	else{
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"This %s has been scheduled for flexible downtime starting between %s and %s and lasting for a period of %d hours and %d minutes.  Notifications for the %s will not be sent out during that time period.",type_string,start_time_string,end_time_string,hours,minutes,type_string);
	        }
	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	/* add a non-persistent comment to the host or service regarding the scheduled outage */
	if(temp_downtime->type==SERVICE_DOWNTIME)
		add_new_comment(SERVICE_COMMENT,svc->host_name,svc->description,time(NULL),"(Nagios Process)",temp_buffer,0,COMMENTSOURCE_INTERNAL,&(temp_downtime->comment_id));
	else
		add_new_comment(HOST_COMMENT,hst->name,NULL,time(NULL),"(Nagios Process)",temp_buffer,0,COMMENTSOURCE_INTERNAL,&(temp_downtime->comment_id));


	/*** SCHEDULE DOWNTIME - FLEXIBLE (NON-FIXED) DOWNTIME IS HANDLED AT A LATER POINT ***/

	/* only non-triggered downtime is scheduled... */
	if(temp_downtime->triggered_by==0){

		/* allocate memory for a new event queue item */
		new_event=(timed_event *)malloc(sizeof(timed_event));
		if(new_event==NULL)
			return ERROR;

		/* place the new event in the event queue */
		new_event->event_type=EVENT_SCHEDULED_DOWNTIME;
		new_event->event_data=(void *)temp_downtime;
		new_event->run_time=temp_downtime->start_time;
		new_event->recurring=FALSE;
		schedule_event(new_event,&event_list_high);
	        }

	return OK;
        }


/* handles scheduled host or service downtime */
int handle_scheduled_downtime(scheduled_downtime *temp_downtime){
 	char buffer[MAX_INPUT_BUFFER];
	scheduled_downtime *this_downtime;
	timed_event *new_event;
	host *hst=NULL;
	service *svc=NULL;
#ifdef USE_EVENT_BROKER
	int attr;
#endif

#ifdef DEBUG0
        printf("handle_scheduled_downtime() start\n");
#endif

	if(temp_downtime==NULL)
		return ERROR;

	/* find the host or service associated with this downtime */
	if(temp_downtime->type==HOST_DOWNTIME){
		hst=find_host(temp_downtime->host_name);
		if(hst==NULL)
			return ERROR;
	        }
	else{
		svc=find_service(temp_downtime->host_name,temp_downtime->service_description);
		if(svc==NULL)
			return ERROR;
	        }

	/* if downtime if flexible and host/svc is in an ok state, don't do anything right now (wait for event handler to kill it off) */
	/* start_flex_downtime variable is set to TRUE by event handler functions */
	if(temp_downtime->fixed==FALSE){

		/* we're not supposed to force a start of flex downtime... */
		if(temp_downtime->start_flex_downtime==FALSE){

			/* host is up or service is ok, so we don't really do anything right now */
			if((temp_downtime->type==HOST_DOWNTIME && hst->current_state==HOST_UP) || (temp_downtime->type==SERVICE_DOWNTIME && svc->current_state==STATE_OK)){

				/* increment pending flex downtime counter */
				if(temp_downtime->type==HOST_DOWNTIME)
					hst->pending_flex_downtime++;
				else
					svc->pending_flex_downtime++;
				temp_downtime->incremented_pending_downtime=TRUE;

				/*** SINCE THE FLEX DOWNTIME MAY NEVER START, WE HAVE TO PROVIDE A WAY OF EXPIRING UNUSED DOWNTIME... ***/

				/* allocate memory for a new event queue item */
				new_event=(timed_event *)malloc(sizeof(timed_event));
				if(new_event==NULL)
					return ERROR;

				/* place the new event in the event queue */
				new_event->event_type=EVENT_EXPIRE_DOWNTIME;
				new_event->event_data=NULL;
				new_event->run_time=(time_t)(temp_downtime->end_time+1);
				new_event->recurring=FALSE;
				schedule_event(new_event,&event_list_high);

				return OK;
			        }
		        }
	        }

	/* have we come to the end of the scheduled downtime? */
	if(temp_downtime->is_in_effect==TRUE){

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		attr=(temp_downtime->type==HOST_DOWNTIME)?NEBATTR_HOST_DOWNTIME:NEBATTR_SERVICE_DOWNTIME;
		attr|=NEBATTR_DOWNTIME_STOP_NORMAL;
		broker_downtime_data(NEBTYPE_DOWNTIME_STOP,NEBFLAG_NONE,attr,temp_downtime->host_name,temp_downtime->service_description,temp_downtime->entry_time,temp_downtime->author,temp_downtime->comment,temp_downtime->start_time,temp_downtime->end_time,temp_downtime->fixed,temp_downtime->duration,temp_downtime->downtime_id,NULL);
#endif

		/* decrement the downtime depth variable */
		if(temp_downtime->type==HOST_DOWNTIME)
			hst->scheduled_downtime_depth--;
		else
			svc->scheduled_downtime_depth--;

		if(temp_downtime->type==HOST_DOWNTIME && hst->scheduled_downtime_depth==0){

			/* log a notice - this one is parsed by the history CGI */
			snprintf(buffer,sizeof(buffer),"HOST DOWNTIME ALERT: %s;STOPPED; Host has exited from a period of scheduled downtime",hst->name);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_INFO_MESSAGE,FALSE);
		        }

		else if(temp_downtime->type==SERVICE_DOWNTIME && svc->scheduled_downtime_depth==0){

			/* log a notice - this one is parsed by the history CGI */
			snprintf(buffer,sizeof(buffer),"SERVICE DOWNTIME ALERT: %s;%s;STOPPED; Service has exited from a period of scheduled downtime",svc->host_name,svc->description);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_INFO_MESSAGE,FALSE);
		        }


		/* update the status data */
		if(temp_downtime->type==HOST_DOWNTIME)
			update_host_status(hst,FALSE);
		else
			update_service_status(svc,FALSE);

		/* decrement pending flex downtime if necessary */
		if(temp_downtime->fixed==FALSE && temp_downtime->incremented_pending_downtime==TRUE){
			if(temp_downtime->type==HOST_DOWNTIME){
				if(hst->pending_flex_downtime>0)
					hst->pending_flex_downtime--;
			        }
			else{
				if(svc->pending_flex_downtime>0)
					svc->pending_flex_downtime--;
			        }
		        }

		/* delete downtime entry from the log */
		if(temp_downtime->type==HOST_DOWNTIME)
			delete_host_downtime(temp_downtime->downtime_id);
		else
			delete_service_downtime(temp_downtime->downtime_id);

		/* handle (stop) downtime that is triggered by this one */
		for(this_downtime=scheduled_downtime_list;this_downtime!=NULL;this_downtime=this_downtime->next){
			if(this_downtime->triggered_by==temp_downtime->downtime_id)
				handle_scheduled_downtime(this_downtime);
		        }
	        }

	/* else we are just starting the scheduled downtime */
	else{

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_downtime_data(NEBTYPE_DOWNTIME_START,NEBFLAG_NONE,(temp_downtime->type==HOST_DOWNTIME)?NEBATTR_HOST_DOWNTIME:NEBATTR_SERVICE_DOWNTIME,temp_downtime->host_name,temp_downtime->service_description,temp_downtime->entry_time,temp_downtime->author,temp_downtime->comment,temp_downtime->start_time,temp_downtime->end_time,temp_downtime->fixed,temp_downtime->duration,temp_downtime->downtime_id,NULL);
#endif

		if(temp_downtime->type==HOST_DOWNTIME && hst->scheduled_downtime_depth==0){

			/* log a notice - this one is parsed by the history CGI */
			snprintf(buffer,sizeof(buffer),"HOST DOWNTIME ALERT: %s;STARTED; Host has entered a period of scheduled downtime",hst->name);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_INFO_MESSAGE,FALSE);
		        }

		else if(temp_downtime->type==SERVICE_DOWNTIME && svc->scheduled_downtime_depth==0){

			/* log a notice - this one is parsed by the history CGI */
			snprintf(buffer,sizeof(buffer),"SERVICE DOWNTIME ALERT: %s;%s;STARTED; Service has entered a period of scheduled downtime",svc->host_name,svc->description);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_INFO_MESSAGE,FALSE);
		        }

		/* increment the downtime depth variable */
		if(temp_downtime->type==HOST_DOWNTIME)
			hst->scheduled_downtime_depth++;
		else
			svc->scheduled_downtime_depth++;

		/* set the in effect flag */
		temp_downtime->is_in_effect=TRUE;

		/* update the status data */
		if(temp_downtime->type==HOST_DOWNTIME)
			update_host_status(hst,FALSE);
		else
			update_service_status(svc,FALSE);

		/* allocate memory for a new event queue item */
		new_event=(timed_event *)malloc(sizeof(timed_event));
		if(new_event==NULL)
			return ERROR;

		/* place the new event in the event queue */
		new_event->event_type=EVENT_SCHEDULED_DOWNTIME;
		new_event->event_data=(void *)temp_downtime;
		if(temp_downtime->fixed==FALSE)
			new_event->run_time=(time_t)((unsigned long)time(NULL)+temp_downtime->duration);
		else
			new_event->run_time=temp_downtime->end_time;
		new_event->recurring=FALSE;
		schedule_event(new_event,&event_list_high);

		/* handle (start) downtime that is triggered by this one */
		for(this_downtime=scheduled_downtime_list;this_downtime!=NULL;this_downtime=this_downtime->next){
			if(this_downtime->triggered_by==temp_downtime->downtime_id)
				handle_scheduled_downtime(this_downtime);
		        }
	        }
	
#ifdef DEBUG0
	printf("handle_scheduled_downtime() end\n");
#endif
	
	return OK;
        }



/* checks for flexible (non-fixed) host downtime that should start now */
int check_pending_flex_host_downtime(host *hst){
	scheduled_downtime *temp_downtime;
	time_t current_time;

#ifdef DEBUG0
	printf("check_pending_flex_host_downtime() start\n");
#endif

	if(hst==NULL)
		return ERROR;

	time(&current_time);

	/* if host is currently up, nothing to do */
	if(hst->current_state==HOST_UP)
		return OK;

	/* check all downtime entries */
	for(temp_downtime=scheduled_downtime_list;temp_downtime!=NULL;temp_downtime=temp_downtime->next){

		if(temp_downtime->type!=HOST_DOWNTIME)
			continue;

		if(temp_downtime->fixed==TRUE)
			continue;

		if(temp_downtime->is_in_effect==TRUE)
			continue;

		/* triggered downtime entries should be ignored here */
		if(temp_downtime->triggered_by!=0)
			continue;

		/* this entry matches our host! */
		if(find_host(temp_downtime->host_name)==hst){
			
			/* if the time boundaries are okay, start this scheduled downtime */
			if(temp_downtime->start_time<=current_time && current_time<=temp_downtime->end_time){
				temp_downtime->start_flex_downtime=TRUE;
				handle_scheduled_downtime(temp_downtime);
			        }
		        }
	        }

#ifdef DEBUG0
	printf("check_pending_flex_host_downtime() end\n");
#endif

	return OK;
        }


/* checks for flexible (non-fixed) service downtime that should start now */
int check_pending_flex_service_downtime(service *svc){
	scheduled_downtime *temp_downtime;
	time_t current_time;

#ifdef DEBUG0
	printf("check_pending_flex_service_downtime() start\n");
#endif

	if(svc==NULL)
		return ERROR;

	time(&current_time);

	/* if service is currently ok, nothing to do */
	if(svc->current_state==STATE_OK)
		return OK;

	/* check all downtime entries */
	for(temp_downtime=scheduled_downtime_list;temp_downtime!=NULL;temp_downtime=temp_downtime->next){

		if(temp_downtime->type!=SERVICE_DOWNTIME)
			continue;

		if(temp_downtime->fixed==TRUE)
			continue;

		if(temp_downtime->is_in_effect==TRUE)
			continue;

		/* triggered downtime entries should be ignored here */
		if(temp_downtime->triggered_by!=0)
			continue;

		/* this entry matches our service! */
		if(find_service(temp_downtime->host_name,temp_downtime->service_description)==svc){

			/* if the time boundaries are okay, start this scheduled downtime */
			if(temp_downtime->start_time<=current_time && current_time<=temp_downtime->end_time){
				temp_downtime->start_flex_downtime=TRUE;
				handle_scheduled_downtime(temp_downtime);
			        }
		        }
	        }

#ifdef DEBUG0
	printf("check_pending_flex_service_downtime() end\n");
#endif

	return OK;
        }


/* checks for (and removes) expired downtime entries */
int check_for_expired_downtime(void){
	scheduled_downtime *temp_downtime;
	scheduled_downtime *next_downtime;
	time_t current_time;

#ifdef DEBUG0
	printf("check_for_expired_downtime() start\n");
#endif

	time(&current_time);

	/* check all downtime entries... */
	for(temp_downtime=scheduled_downtime_list;temp_downtime!=NULL;temp_downtime=next_downtime){

		next_downtime=temp_downtime->next;

		/* this entry should be removed */
		if(temp_downtime->is_in_effect==FALSE && temp_downtime->end_time<current_time){

			/* delete the downtime entry */
			if(temp_downtime->type==HOST_DOWNTIME)
				delete_host_downtime(temp_downtime->downtime_id);
			else
				delete_service_downtime(temp_downtime->downtime_id);
		        }
	        }

#ifdef DEBUG0
	printf("check_for_expired_downtime() end\n");
#endif

	return OK;
        }



/******************************************************************/
/************************* SAVE FUNCTIONS *************************/
/******************************************************************/


/* save a host or service downtime */
int add_new_downtime(int type, char *host_name, char *service_description, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id){
	int result;

	if(type==HOST_DOWNTIME)
		result=add_new_host_downtime(host_name,entry_time,author,comment,start_time,end_time,fixed,triggered_by,duration,downtime_id);
	else
		result=add_new_service_downtime(host_name,service_description,entry_time,author,comment,start_time,end_time,fixed,triggered_by,duration,downtime_id);

	return result;
        }


/* saves a host downtime entry */
int add_new_host_downtime(char *host_name, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id){
	int result;
	unsigned long new_downtime_id;

	if(host_name==NULL)
		return ERROR;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XDDDEFAULT
	result=xdddefault_add_new_host_downtime(host_name,entry_time,author,comment,start_time,end_time,fixed,triggered_by,duration,&new_downtime_id);
#endif
#ifdef USE_XDDDB
	result=xdddb_add_new_host_downtime(host_name,entry_time,author,comment,start_time,end_time,fixed,triggered_by,duration,&new_downtime_id);
#endif

	/* save downtime id */
	if(downtime_id!=NULL)
		*downtime_id=new_downtime_id;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_downtime_data(NEBTYPE_DOWNTIME_ADD,NEBFLAG_NONE,NEBATTR_HOST_DOWNTIME,host_name,NULL,entry_time,author,comment,start_time,end_time,fixed,trigerred_by,duration,new_downtime_id,NULL);
#endif

	return result;
        }


/* saves a service downtime entry */
int add_new_service_downtime(char *host_name, char *service_description, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id){
	int result;
	unsigned long new_downtime_id;

	if(host_name==NULL || service_description==NULL)
		return ERROR;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XDDDEFAULT
	result=xdddefault_add_new_service_downtime(host_name,service_description,entry_time,author,comment,start_time,end_time,fixed,triggered_by,duration,&new_downtime_id);
#endif
#ifdef USE_XDDDB
	result=xdddb_add_new_service_downtime(host_name,service_description,entry_time,author,comment,start_time,end_time,fixed,triggered_by,duration,&new_downtime_id);
#endif

	/* save downtime id */
	if(downtime_id!=NULL)
		*downtime_id=new_downtime_id;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_downtime_data(NEBTYPE_DOWNTIME_ADD,NEBFLAG_NONE,NEBATTR_SERVICE_DOWNTIME,host_name,service_description,entry_time,author,comment,start_time,end_time,fixed,triggered_by,duration,new_downtime_id,NULL);
#endif

	return result;
        }



/******************************************************************/
/*********************** DELETION FUNCTIONS ***********************/
/******************************************************************/


/* deletes a scheduled host or service downtime entry from the list in memory */
int delete_downtime(int type,unsigned long downtime_id){
	int result;
	scheduled_downtime *this_downtime=NULL;
	scheduled_downtime *last_downtime=NULL;
	scheduled_downtime *next_downtime=NULL;

	/* find the downtime we should remove */
	for(this_downtime=scheduled_downtime_list,last_downtime=scheduled_downtime_list;this_downtime!=NULL;this_downtime=next_downtime){
		next_downtime=this_downtime->next;

		/* we found the downtime we should delete */
		if(this_downtime->downtime_id==downtime_id && this_downtime->type==type)
			break;

		last_downtime=this_downtime;
	        }

	/* remove the downtime from the list in memory */
	if(this_downtime!=NULL){

		/* first remove the comment associated with this downtime */
		if(this_downtime->type==HOST_DOWNTIME)
			delete_host_comment(this_downtime->comment_id);
		else
			delete_service_comment(this_downtime->comment_id);

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_downtime_data(NEBTYPE_DOWNTIME_DELETE,NEBFLAG_NONE,(type==HOST_DOWNTIME)?NEBATTR_HOST_DOWNTIME:NEBATTR_SERVICE_DOWNTIME,this_downtime->host_name,this_downtime->service_description,this_downtime->entry_time,this_downtime->author,this_downtime->comment,this_downtime->start_time,this_downtime->end_time,this_downtime->fixed,this_downtime->trigerred_by,this_downtime->duration,downtime_id,NULL);
#endif

		if(scheduled_downtime_list==this_downtime)
			scheduled_downtime_list=this_downtime->next;
		else
			last_downtime->next=next_downtime;
		
		/* free memory */
		free(this_downtime->host_name);
		free(this_downtime->service_description);
		free(this_downtime->author);
		free(this_downtime->comment);
		free(this_downtime);

		result=OK;
	        }
	else
		result=ERROR;
	
	return result;
        }


/* deletes a scheduled host downtime entry */
int delete_host_downtime(unsigned long downtime_id){
	int result;

	/* delete the downtime from memory */
	delete_downtime(HOST_DOWNTIME,downtime_id);
	
	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XDDDEFAULT
	result=xdddefault_delete_host_downtime(downtime_id);
#endif
#ifdef USE_XDDDB
	result=xdddb_delete_host_downtime(downtime_id,FALSE);
#endif

	return result;
        }


/* deletes a scheduled service downtime entry */
int delete_service_downtime(unsigned long downtime_id){
	int result;

	/* delete the downtime from memory */
	delete_downtime(HOST_DOWNTIME,downtime_id);
	
	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XDDDEFAULT
	result=xdddefault_delete_service_downtime(downtime_id);
#endif
#ifdef USE_XDDDB
	result=xdddb_delete_service_downtime(downtime_id,FALSE);
#endif

	return result;
        }

#endif



#ifdef NSCGI

/******************************************************************/
/************************ INPUT FUNCTIONS *************************/
/******************************************************************/

/* reads all downtime data */
int read_downtime_data(char *main_config_file){
	int result;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XDDDEFAULT
	result=xdddefault_read_downtime_data(main_config_file);
#endif
#ifdef USE_XDDDB
	result=xdddb_read_downtime_data(main_config_file);
#endif

	return result;
        }

#endif



/******************************************************************/
/******************** ADDITION FUNCTIONS **************************/
/******************************************************************/

/* adds a host downtime entry to the list in memory */
int add_host_downtime(char *host_name, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id){
	int result;

	result=add_downtime(HOST_DOWNTIME,host_name,NULL,entry_time,author,comment,start_time,end_time,fixed,triggered_by,duration,downtime_id);

	return result;
        }


/* adds a service downtime entry to the list in memory */
int add_service_downtime(char *host_name, char *svc_description, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id){
	int result;

	result=add_downtime(SERVICE_DOWNTIME,host_name,svc_description,entry_time,author,comment,start_time,end_time,fixed,triggered_by,duration,downtime_id);

	return result;
        }


/* adds a host or service downtime entry to the list in memory */
int add_downtime(int downtime_type, char *host_name, char *svc_description, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id){
	scheduled_downtime *new_downtime=NULL;
	scheduled_downtime *last_downtime=NULL;
	scheduled_downtime *temp_downtime=NULL;

	/* don't add triggered downtimes that don't have a valid parent */
	if(triggered_by>0  && find_downtime(ANY_DOWNTIME,triggered_by)==NULL)
		return ERROR;

	/* allocate memory for the downtime */
	new_downtime=(scheduled_downtime *)malloc(sizeof(scheduled_downtime));
	if(new_downtime==NULL)
		return ERROR;

	new_downtime->host_name=strdup(host_name);
	if(new_downtime->host_name==NULL){
		free(new_downtime);
		return ERROR;
	        }

	if(downtime_type==SERVICE_DOWNTIME){
		new_downtime->service_description=strdup(svc_description);
		if(new_downtime->service_description==NULL){
			free(new_downtime->host_name);
			free(new_downtime);
			return ERROR;
		        }
	        }
	else
		new_downtime->service_description=NULL;

	if(author==NULL)
		new_downtime->author=NULL;
	else
		new_downtime->author=strdup(author);

	if(comment==NULL)
		new_downtime->comment=NULL;
	else
		new_downtime->comment=strdup(comment);

	new_downtime->type=downtime_type;
	new_downtime->entry_time=entry_time;
	new_downtime->start_time=start_time;
	new_downtime->end_time=end_time;
	new_downtime->fixed=(fixed>0)?TRUE:FALSE;
	new_downtime->triggered_by=triggered_by;
	new_downtime->duration=duration;
	new_downtime->downtime_id=downtime_id;
#ifdef NSCORE
	new_downtime->comment_id=0;
	new_downtime->is_in_effect=FALSE;
	new_downtime->start_flex_downtime=FALSE;
	new_downtime->incremented_pending_downtime=FALSE;
#endif


	/* add new downtime to downtime list, sorted by start time */
	last_downtime=scheduled_downtime_list;
	for(temp_downtime=scheduled_downtime_list;temp_downtime!=NULL;temp_downtime=temp_downtime->next){
		if(new_downtime->start_time<temp_downtime->start_time){
			new_downtime->next=temp_downtime;
			if(temp_downtime==scheduled_downtime_list)
				scheduled_downtime_list=new_downtime;
			else
				last_downtime->next=new_downtime;
			break;
		        }
		else
			last_downtime=temp_downtime;
	        }
	if(scheduled_downtime_list==NULL){
		new_downtime->next=NULL;
		scheduled_downtime_list=new_downtime;
	        }
	else if(temp_downtime==NULL){
		new_downtime->next=NULL;
		last_downtime->next=new_downtime;
	        }

#ifdef NSCORE
#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_downtime_data(NEBTYPE_DOWNTIME_LOAD,NEBFLAG_NONE,(downtime_type==HOST_DOWNTIME)?NEBATTR_HOST_DOWNTIME:NEBATTR_SERVICE_DOWNTIME,host_name,svc_description,entry_time,author,comment,start_time,end_time,fixed,duration,downtime_id,NULL);
#endif
#endif

	return OK;
        }




/******************************************************************/
/************************ SEARCH FUNCTIONS ************************/
/******************************************************************/

/* finds a specific downtime entry */
scheduled_downtime *find_downtime(int type, unsigned long downtime_id){
	scheduled_downtime *temp_downtime;

	for(temp_downtime=scheduled_downtime_list;temp_downtime!=NULL;temp_downtime=temp_downtime->next){
		if(type!=ANY_DOWNTIME && temp_downtime->type!=type)
			continue;
		if(temp_downtime->downtime_id==downtime_id)
			return temp_downtime;
	        }

	return NULL;
        }


/* finds a specific host downtime entry */
scheduled_downtime *find_host_downtime(unsigned long downtime_id){

	return find_downtime(HOST_DOWNTIME,downtime_id);
        }


/* finds a specific service downtime entry */
scheduled_downtime *find_service_downtime(unsigned long downtime_id){

	return find_downtime(SERVICE_DOWNTIME,downtime_id);
        }



/******************************************************************/
/********************* CLEANUP FUNCTIONS **************************/
/******************************************************************/

/* frees memory allocated for the scheduled downtime data */
void free_downtime_data(void){
	scheduled_downtime *this_downtime;
	scheduled_downtime *next_downtime;

	/* free memory for the scheduled_downtime list */
	for(this_downtime=scheduled_downtime_list;this_downtime!=NULL;this_downtime=next_downtime){
		next_downtime=this_downtime->next;
		free(this_downtime->host_name);
		free(this_downtime->service_description);
		free(this_downtime->author);
		free(this_downtime->comment);
		free(this_downtime);
	        }

	/* reset list pointer */
	scheduled_downtime_list=NULL;

	return;
        }


