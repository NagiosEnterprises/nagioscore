/*****************************************************************************
 *
 * BROKER.C - Event broker routines for Nagios
 *
 * Copyright (c) 2002-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   08-15-2003
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

#include "../include/config.h"
#include "../include/common.h"

#include "../include/nagios.h"
#include "../include/broker.h"

extern int             event_broker_options;


#ifdef USE_EVENT_BROKER



/******************************************************************/
/************************* EVENT FUNCTIONS ************************/
/******************************************************************/


/* sends program data (starts, restarts, stops, etc.) to broker */
void broker_program_state(int type, int flags, int attr, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_PROGRAM_STATE))
		return;

	tv=get_broker_timestamp(timestamp);

	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,(int)getpid());
	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	return;
        }


/* send timed event data to broker */
void broker_timed_event(int type, int flags, int attr, timed_event *event, void *data, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;
	service *temp_service;
	host *temp_host;

	if(!(event_broker_options & BROKER_TIMED_EVENTS))
		return;

	tv=get_broker_timestamp(timestamp);

	switch(type){
	case NEBTYPE_TIMEDEVENT_SLEEP:
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lf;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,*((double *)data));
		break;
	default:
		if(event==NULL)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr);
		else if(event->event_type==EVENT_SERVICE_CHECK){
			temp_service=(service *)event->event_data;
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%lu;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,event->event_type,event->run_time,temp_service->host_name,temp_service->description);
		        }
		else if(event->event_type==EVENT_HOST_CHECK){
			temp_host=(host *)event->event_data;
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%lu;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,event->event_type,event->run_time,temp_host->name);
		        }
		else
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%lu;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,event->event_type,event->run_time);
		break;
	        }
	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	return;
        }



/* send log data to broker */
void broker_log_data(int type, int flags, int attr, char *data, unsigned long data_type, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_LOGGED_DATA))
		return;

	tv=get_broker_timestamp(timestamp);

	strip(data);
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lu;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,data_type,data);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	return;
        }



/* send system command data to broker */
void broker_system_command(int type, int flags, int attr, double exectime, int timeout, int early_timeout, int retcode, char *cmd, char *output, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_SYSTEM_COMMANDS))
		return;
	
	if(cmd==NULL)
		return;

	tv=get_broker_timestamp(timestamp);

	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lf;%d;%d;%d;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,exectime,timeout,early_timeout,retcode,cmd,(output==NULL)?"":output);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	return;
        }



/* send event handler data to broker */
void broker_event_handler(int type, int flags, int attr, void *data, int state, int state_type, double exectime, int timeout, int early_timeout, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;
	service *temp_service=NULL;
	host *temp_host=NULL;

	if(!(event_broker_options & BROKER_EVENT_HANDLERS))
		return;
	
	if(data==NULL)
		return;

	tv=get_broker_timestamp(timestamp);

	if(type==NEBTYPE_EVENTHANDLER_SERVICE || type==NEBTYPE_EVENTHANDLER_GLOBAL_SERVICE)
		temp_service=(service *)data;
	else
		temp_host=(host *)data;

	if(type==NEBTYPE_EVENTHANDLER_SERVICE || type==NEBTYPE_EVENTHANDLER_GLOBAL_SERVICE)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%d;%lf;%d;%d;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,state_type,exectime,timeout,early_timeout,temp_service->host_name,temp_service->description);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%d;%lf;%d;%d;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,state_type,exectime,timeout,early_timeout,temp_host->name);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	return;
        }




/* send obsessive compulsive host/service  data to broker */
void broker_ocp_data(int type, int flags, int attr, void *data, int state, int state_type, double exectime, int timeout, int early_timeout, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;
	service *temp_service=NULL;
	host *temp_host=NULL;

	if(!(event_broker_options & BROKER_OCP_DATA))
		return;
	
	if(data==NULL)
		return;

	tv=get_broker_timestamp(timestamp);

	if(type==NEBTYPE_OCP_SERVICE)
		temp_service=(service *)data;
	else
		temp_host=(host *)data;

	if(type==NEBTYPE_OCP_SERVICE)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%d;%lf;%d;%d;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,state_type,exectime,timeout,early_timeout,temp_service->host_name,temp_service->description);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%d;%lf;%d;%d;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,state_type,exectime,timeout,early_timeout,temp_host->name);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	return;
        }



/* send host check data to broker */
void broker_host_check(int type, int flags, int attr, host *hst, int state, double exectime, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_HOST_CHECKS))
		return;
	
	if(hst==NULL)
		return;

	tv=get_broker_timestamp(timestamp);

	if(type==NEBTYPE_HOSTCHECK_INITIATE)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lf;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,hst->latency,hst->name);
	else if(type==NEBTYPE_HOSTCHECK_PROCESSED)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%s;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,hst->name,hst->plugin_output,hst->perf_data);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%lf;%d;%d;%s;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,exectime,hst->current_attempt,hst->max_attempts,hst->name,hst->plugin_output,hst->perf_data);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	return;
        }



/* send service check data to broker */
void broker_service_check(int type, int flags, int attr, service *svc, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_SERVICE_CHECKS))
		return;
	
	if(svc==NULL)
		return;

	tv=get_broker_timestamp(timestamp);

	if(type==NEBTYPE_SERVICECHECK_INITIATE)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lf;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,svc->latency,svc->host_name,svc->description);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%d;%lf;%d;%d;%s;%s;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,svc->current_state,svc->state_type,svc->execution_time,svc->current_attempt,svc->max_attempts,svc->host_name,svc->description,svc->plugin_output,svc->perf_data);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	return;
        }



/* send comment data to broker */
void broker_comment_data(int type, int flags, int attr, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, unsigned long comment_id, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_COMMENT_DATA))
		return;
	
	tv=get_broker_timestamp(timestamp);

	if(attr==NEBATTR_HOST_COMMENT)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%lu;%s;%s;%d;%d;%lu;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,host_name,entry_time,author_name,comment_data,persistent,source,comment_id);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%s;%lu;%s;%s;%d;%d;%lu;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,host_name,svc_description,entry_time,author_name,comment_data,persistent,source,comment_id);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	return;
        }



/* send downtime data to broker */
void broker_downtime_data(int type, int flags, int attr, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_DOWNTIME_DATA))
		return;
	
	tv=get_broker_timestamp(timestamp);

	if((attr & NEBATTR_HOST_DOWNTIME))
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%lu;%s;%s;%lu;%lu;%d;%lu;%lu;%lu;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,host_name,entry_time,author_name,comment_data,start_time,end_time,fixed,triggered_by,duration,downtime_id);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%s;%lu;%s;%s;%lu;%lu;%d;%lu;%lu;%lu;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,host_name,svc_description,entry_time,author_name,comment_data,start_time,end_time,fixed,triggered_by,duration,downtime_id);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	return;
        }



/* send flapping data to broker */
void broker_flapping_data(int type, int flags, int attr, void *data, double percent_change, double threshold, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	service *temp_service;
	host *temp_host;
	struct timeval tv;

	if(!(event_broker_options & BROKER_FLAPPING_DATA))
		return;

	if(data==NULL)
		return;

	if(attr & NEBATTR_SERVICE_FLAPPING)
		temp_service=(service *)data;
	else
		temp_host=(host *)data;
		
	tv=get_broker_timestamp(timestamp);

	if(attr & NEBATTR_SERVICE_FLAPPING)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%s;%lf;%lf;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,temp_service->host_name,temp_service->description,percent_change,threshold);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%lf;%lf;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,temp_host->name,percent_change,threshold);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';


	return;
        }



/******************************************************************/
/************************* OUTPUT FUNCTIONS ***********************/
/******************************************************************/

/* gets timestamp for use by broker */
struct timeval get_broker_timestamp(struct timeval *timestamp){
	struct timeval tv;

	if(timestamp==NULL)
		gettimeofday(&tv,NULL);
	else
		tv=*timestamp;

	return tv;
        }



#endif

