/*****************************************************************************
 *
 * SEHANDLERS.C - Service and host event and state handlers for Nagios
 *
 * Copyright (c) 1999-2006 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-12-2006
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
#include "../include/statusdata.h"
#include "../include/downtime.h"
#include "../include/nagios.h"
#include "../include/perfdata.h"
#include "../include/broker.h"


extern int             enable_event_handlers;
extern int             obsess_over_services;
extern int             obsess_over_hosts;

extern int             log_event_handlers;
extern int             log_host_retries;

extern int             event_handler_timeout;
extern int             ocsp_timeout;
extern int             ochp_timeout;

extern char            *macro_x[MACRO_X_COUNT];

extern char            *global_host_event_handler;
extern char            *global_service_event_handler;

extern char            *ocsp_command;
extern char            *ochp_command;

extern time_t          program_start;



/******************************************************************/
/************* OBSESSIVE COMPULSIVE HANDLER FUNCTIONS *************/
/******************************************************************/


/* handles service check results in an obsessive compulsive manner... */
int obsessive_compulsive_service_check_processor(service *svc){
	char raw_command_line[MAX_COMMAND_BUFFER];
	char processed_command_line[MAX_COMMAND_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	host *temp_host;
	int early_timeout=FALSE;
	double exectime;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;

#ifdef DEBUG0
	printf("obsessive_compulsive_service_check_processor() start\n");
#endif

	/* bail out if we shouldn't be obsessing */
	if(obsess_over_services==FALSE)
		return OK;
	if(svc->obsess_over_service==FALSE)
		return OK;

	/* if there is no valid command, exit */
	if(ocsp_command==NULL)
		return ERROR;

	/* find the associated host */
	temp_host=find_host(svc->host_name);

	/* update service macros */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);
	grab_summary_macros(NULL);

	/* get the raw command line */
	get_raw_command_line(ocsp_command,raw_command_line,sizeof(raw_command_line),macro_options);
	strip(raw_command_line);

#ifdef DEBUG3
	printf("\tRaw obsessive compulsive service processor command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),macro_options);

#ifdef DEBUG3
	printf("\tProcessed obsessive compulsive service processor command line: %s\n",processed_command_line);
#endif

	/* run the command */
	my_system(processed_command_line,ocsp_timeout,&early_timeout,&exectime,NULL,0);

	/* check to see if the command timed out */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: OCSP command '%s' for service '%s' on host '%s' timed out after %d seconds\n",processed_command_line,svc->description,svc->host_name,ocsp_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }
	
#ifdef DEBUG0
	printf("obsessive_compulsive_service_check_processor() end\n");
#endif

	return OK;
        }



/* handles host check results in an obsessive compulsive manner... */
int obsessive_compulsive_host_check_processor(host *hst){
	char raw_command_line[MAX_COMMAND_BUFFER];
	char processed_command_line[MAX_COMMAND_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	int early_timeout=FALSE;
	double exectime;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;

#ifdef DEBUG0
	printf("obsessive_compulsive_host_check_processor() start\n");
#endif

	/* bail out if we shouldn't be obsessing */
	if(obsess_over_hosts==FALSE)
		return OK;
	if(hst->obsess_over_host==FALSE)
		return OK;

	/* if there is no valid command, exit */
	if(ochp_command==NULL)
		return ERROR;

	/* update macros */
	clear_volatile_macros();
	grab_host_macros(hst);
	grab_summary_macros(NULL);

	/* get the raw command line */
	get_raw_command_line(ochp_command,raw_command_line,sizeof(raw_command_line),macro_options);
	strip(raw_command_line);

#ifdef DEBUG3
	printf("\tRaw obsessive compulsive host processor command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),macro_options);

#ifdef DEBUG3
	printf("\tProcessed obsessive compulsive host processor command line: %s\n",processed_command_line);
#endif

	/* run the command */
	my_system(processed_command_line,ochp_timeout,&early_timeout,&exectime,NULL,0);

	/* check to see if the command timed out */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: OCHP command '%s' for host '%s' timed out after %d seconds\n",processed_command_line,hst->name,ochp_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }

#ifdef DEBUG0
	printf("obsessive_compulsive_host_check_processor() end\n");
#endif

	return OK;
        }




/******************************************************************/
/**************** SERVICE EVENT HANDLER FUNCTIONS *****************/
/******************************************************************/


/* handles changes in the state of a service */
int handle_service_event(service *svc){
	host *temp_host;

#ifdef DEBUG0
	printf("handle_service_event() start\n");
#endif

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_statechange_data(NEBTYPE_STATECHANGE_END,NEBFLAG_NONE,NEBATTR_NONE,SERVICE_STATECHANGE,(void *)svc,svc->current_state,svc->state_type,svc->current_attempt,svc->max_attempts,NULL);
#endif

	/* bail out if we shouldn't be running event handlers */
	if(enable_event_handlers==FALSE)
		return OK;
	if(svc->event_handler_enabled==FALSE)
		return OK;

	/* find the host */
	temp_host=find_host(svc->host_name);

	/* update service macros */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);
	grab_summary_macros(NULL);

	/* run the global service event handler */
	run_global_service_event_handler(svc);

	/* run the event handler command if there is one */
	if(svc->event_handler!=NULL)
		run_service_event_handler(svc);

	/* check for external commands - the event handler may have given us some directives... */
	check_for_external_commands();

#ifdef DEBUG0
	printf("handle_service_event() end\n");
#endif

	return OK;
        }



/* runs the global service event handler */
int run_global_service_event_handler(service *svc){
	char raw_command_line[MAX_COMMAND_BUFFER];
	char processed_command_line[MAX_COMMAND_BUFFER];
	char command_output[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	int early_timeout=FALSE;
	double exectime=0.0;
	int result=0;
	struct timeval start_time;
	struct timeval end_time;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;

#ifdef DEBUG0
	printf("run_global_service_event_handler() start\n");
#endif

	/* bail out if we shouldn't be running event handlers */
	if(enable_event_handlers==FALSE)
		return OK;

	/* a global service event handler command has not been defined */
	if(global_service_event_handler==NULL)
		return ERROR;

	/* get start time */
	gettimeofday(&start_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_event_handler(NEBTYPE_EVENTHANDLER_START,NEBFLAG_NONE,NEBATTR_NONE,GLOBAL_SERVICE_EVENTHANDLER,(void *)svc,svc->current_state,svc->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,global_service_event_handler,NULL,NULL,NULL);
#endif

	/* get the raw command line */
	get_raw_command_line(global_service_event_handler,raw_command_line,sizeof(raw_command_line),macro_options);
	strip(raw_command_line);

#ifdef DEBUG3
	printf("\tRaw global service event handler command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),macro_options);

#ifdef DEBUG3
	printf("\tProcessed global service event handler command line: %s\n",processed_command_line);
#endif

	if(log_event_handlers==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"GLOBAL SERVICE EVENT HANDLER: %s;%s;%s;%s;%s;%s\n",svc->host_name,svc->description,macro_x[MACRO_SERVICESTATE],macro_x[MACRO_SERVICESTATETYPE],macro_x[MACRO_SERVICEATTEMPT],global_service_event_handler);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_all_logs(temp_buffer,NSLOG_EVENT_HANDLER);
	        }

	/* run the command */
	result=my_system(processed_command_line,event_handler_timeout,&early_timeout,&exectime,command_output,sizeof(command_output)-1);

	/* check to see if the event handler timed out */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Global service event handler command '%s' timed out after %d seconds\n",processed_command_line,event_handler_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* get end time */
	gettimeofday(&end_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_event_handler(NEBTYPE_EVENTHANDLER_END,NEBFLAG_NONE,NEBATTR_NONE,GLOBAL_SERVICE_EVENTHANDLER,(void *)svc,svc->current_state,svc->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,global_service_event_handler,processed_command_line,command_output,NULL);
#endif

#ifdef DEBUG0
	printf("run_global_service_event_handler() end\n");
#endif

	return OK;
        }



/* runs a service event handler command */
int run_service_event_handler(service *svc){
	char raw_command_line[MAX_COMMAND_BUFFER];
	char processed_command_line[MAX_COMMAND_BUFFER];
	char command_output[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	int early_timeout=FALSE;
	double exectime=0.0;
	int result=0;
	struct timeval start_time;
	struct timeval end_time;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;

#ifdef DEBUG0
	printf("run_service_event_handler() start\n");
#endif

	/* bail if there's no command */
	if(svc->event_handler==NULL)
		return ERROR;

	/* get start time */
	gettimeofday(&start_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_event_handler(NEBTYPE_EVENTHANDLER_START,NEBFLAG_NONE,NEBATTR_NONE,SERVICE_EVENTHANDLER,(void *)svc,svc->current_state,svc->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,svc->event_handler,NULL,NULL,NULL);
#endif

	/* get the raw command line */
	get_raw_command_line(svc->event_handler,raw_command_line,sizeof(raw_command_line),macro_options);
	strip(raw_command_line);

#ifdef DEBUG3
	printf("\tRaw service event handler command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),macro_options);

#ifdef DEBUG3
	printf("\tProcessed service event handler command line: %s\n",processed_command_line);
#endif

	if(log_event_handlers==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"SERVICE EVENT HANDLER: %s;%s;%s;%s;%s;%s\n",svc->host_name,svc->description,macro_x[MACRO_SERVICESTATE],macro_x[MACRO_SERVICESTATETYPE],macro_x[MACRO_SERVICEATTEMPT],svc->event_handler);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_all_logs(temp_buffer,NSLOG_EVENT_HANDLER);
	        }

	/* run the command */
	result=my_system(processed_command_line,event_handler_timeout,&early_timeout,&exectime,command_output,sizeof(command_output)-1);

	/* check to see if the event handler timed out */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Service event handler command '%s' timed out after %d seconds\n",processed_command_line,event_handler_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* get end time */
	gettimeofday(&end_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_event_handler(NEBTYPE_EVENTHANDLER_END,NEBFLAG_NONE,NEBATTR_NONE,SERVICE_EVENTHANDLER,(void *)svc,svc->current_state,svc->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,svc->event_handler,processed_command_line,command_output,NULL);
#endif

#ifdef DEBUG0
	printf("run_service_event_handler() end\n");
#endif

	return OK;
        }




/******************************************************************/
/****************** HOST EVENT HANDLER FUNCTIONS ******************/
/******************************************************************/


/* handles a change in the status of a host */
int handle_host_event(host *hst){

#ifdef DEBUG0
	printf("handle_host_event() start\n");
#endif

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_statechange_data(NEBTYPE_STATECHANGE_END,NEBFLAG_NONE,NEBATTR_NONE,HOST_STATECHANGE,(void *)hst,hst->current_state,hst->state_type,hst->current_attempt,hst->max_attempts,NULL);
#endif

	/* bail out if we shouldn't be running event handlers */
	if(enable_event_handlers==FALSE)
		return OK;
	if(hst->event_handler_enabled==FALSE)
		return OK;

	/* update host macros */
	clear_volatile_macros();
	grab_host_macros(hst);
	grab_summary_macros(NULL);

	/* run the global host event handler */
	run_global_host_event_handler(hst);

	/* run the event handler command if there is one */
	if(hst->event_handler!=NULL)
		run_host_event_handler(hst);

	/* check for external commands - the event handler may have given us some directives... */
	check_for_external_commands();

#ifdef DEBUG0
	printf("handle_host_event() end\n");
#endif

	return OK;
        }


/* runs the global host event handler */
int run_global_host_event_handler(host *hst){
	char raw_command_line[MAX_COMMAND_BUFFER];
	char processed_command_line[MAX_COMMAND_BUFFER];
	char command_output[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	int early_timeout=FALSE;
	double exectime=0.0;
	int result=0;
	struct timeval start_time;
	struct timeval end_time;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;

#ifdef DEBUG0
	printf("run_global_host_event_handler() start\n");
#endif

	/* bail out if we shouldn't be running event handlers */
	if(enable_event_handlers==FALSE)
		return OK;

	/* no global host event handler command is defined */
	if(global_host_event_handler==NULL)
		return ERROR;

	/* get start time */
	gettimeofday(&start_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_event_handler(NEBTYPE_EVENTHANDLER_START,NEBFLAG_NONE,NEBATTR_NONE,GLOBAL_HOST_EVENTHANDLER,(void *)hst,hst->current_state,hst->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,global_host_event_handler,NULL,NULL,NULL);
#endif

	/* get the raw command line */
	get_raw_command_line(global_host_event_handler,raw_command_line,sizeof(raw_command_line),macro_options);
	strip(raw_command_line);

#ifdef DEBUG3
	printf("\tRaw global host event handler command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),macro_options);

#ifdef DEBUG3
	printf("\tProcessed global host event handler command line: %s\n",processed_command_line);
#endif

	if(log_event_handlers==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"GLOBAL HOST EVENT HANDLER: %s;%s;%s;%s;%s\n",hst->name,macro_x[MACRO_HOSTSTATE],macro_x[MACRO_HOSTSTATETYPE],macro_x[MACRO_HOSTATTEMPT],global_host_event_handler);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_all_logs(temp_buffer,NSLOG_EVENT_HANDLER);
	        }

	/* run the command */
	result=my_system(processed_command_line,event_handler_timeout,&early_timeout,&exectime,command_output,sizeof(command_output)-1);

	/* check for a timeout in the execution of the event handler command */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Global host event handler command '%s' timed out after %d seconds\n",processed_command_line,event_handler_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* get end time */
	gettimeofday(&end_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_event_handler(NEBTYPE_EVENTHANDLER_END,NEBFLAG_NONE,NEBATTR_NONE,GLOBAL_HOST_EVENTHANDLER,(void *)hst,hst->current_state,hst->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,global_host_event_handler,processed_command_line,command_output,NULL);
#endif

#ifdef DEBUG0
	printf("run_global_host_event_handler() end\n");
#endif

	return OK;
        }


/* runs a host event handler command */
int run_host_event_handler(host *hst){
	char raw_command_line[MAX_COMMAND_BUFFER];
	char processed_command_line[MAX_COMMAND_BUFFER];
	char command_output[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	int early_timeout=FALSE;
	double exectime=0.0;
	int result=0;
	struct timeval start_time;
	struct timeval end_time;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;

#ifdef DEBUG0
	printf("run_host_event_handler() start\n");
#endif

	/* bail if there's no command */
	if(hst->event_handler==NULL)
		return ERROR;

	/* get start time */
	gettimeofday(&start_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_event_handler(NEBTYPE_EVENTHANDLER_START,NEBFLAG_NONE,NEBATTR_NONE,HOST_EVENTHANDLER,(void *)hst,hst->current_state,hst->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,hst->event_handler,NULL,NULL,NULL);
#endif

	/* get the raw command line */
	get_raw_command_line(hst->event_handler,raw_command_line,sizeof(raw_command_line),macro_options);
	strip(raw_command_line);

#ifdef DEBUG3
	printf("\tRaw host event handler command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),macro_options);

#ifdef DEBUG3
	printf("\tProcessed host event handler command line: %s\n",processed_command_line);
#endif

	if(log_event_handlers==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"HOST EVENT HANDLER: %s;%s;%s;%s;%s\n",hst->name,macro_x[MACRO_HOSTSTATE],macro_x[MACRO_HOSTSTATETYPE],macro_x[MACRO_HOSTATTEMPT],hst->event_handler);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_all_logs(temp_buffer,NSLOG_EVENT_HANDLER);
	        }

	/* run the command */
	result=my_system(processed_command_line,event_handler_timeout,&early_timeout,&exectime,command_output,sizeof(command_output)-1);

	/* check to see if the event handler timed out */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Host event handler command '%s' timed out after %d seconds\n",processed_command_line,event_handler_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* get end time */
	gettimeofday(&end_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send event data to broker */
	broker_event_handler(NEBTYPE_EVENTHANDLER_END,NEBFLAG_NONE,NEBATTR_NONE,HOST_EVENTHANDLER,(void *)hst,hst->current_state,hst->state_type,start_time,end_time,exectime,event_handler_timeout,early_timeout,result,hst->event_handler,processed_command_line,command_output,NULL);
#endif

#ifdef DEBUG0
	printf("run_host_event_handler() end\n");
#endif

	return OK;
        }




/******************************************************************/
/****************** HOST STATE HANDLER FUNCTIONS ******************/
/******************************************************************/


/* top level host state handler - occurs after every host check (soft/hard and active/passive) */
int handle_host_state(host *hst){
	int state_change=FALSE;
	time_t current_time;

#ifdef DEBUG0
	printf("handle_host_state() start\n");
#endif

	/* get current time */
	time(&current_time);

	/* obsess over this host check */
	obsessive_compulsive_host_check_processor(hst);

	/* update performance data */
	update_host_performance_data(hst);

	/* record latest time for current state */
	switch(hst->current_state){
	case HOST_UP:
		hst->last_time_up=current_time;
		break;
	case HOST_DOWN:
		hst->last_time_down=current_time;
		break;
	case HOST_UNREACHABLE:
		hst->last_time_unreachable=current_time;
		break;
	default:
		break;
	        }

	/* has the host state changed? */
	if(hst->last_state!=hst->current_state || (hst->current_state==HOST_UP && hst->state_type==SOFT_STATE))
		state_change=TRUE;

	/* if the host state has changed... */
	if(state_change==TRUE){

		/* update last state change times */
		hst->last_state_change=current_time;
		if(hst->state_type==HARD_STATE)
			hst->last_hard_state_change=current_time;

		/* reset the acknowledgement flag if necessary */
		if(hst->acknowledgement_type==ACKNOWLEDGEMENT_NORMAL){
			hst->problem_has_been_acknowledged=FALSE;
			hst->acknowledgement_type=ACKNOWLEDGEMENT_NONE;
		        }
		else if(hst->acknowledgement_type==ACKNOWLEDGEMENT_STICKY && hst->current_state==HOST_UP){
			hst->problem_has_been_acknowledged=FALSE;
			hst->acknowledgement_type=ACKNOWLEDGEMENT_NONE;
		        }

		/* reset the next and last notification times */
		hst->last_host_notification=(time_t)0;
		hst->next_host_notification=(time_t)0;

		/* reset notification suppression option */
		hst->no_more_notifications=FALSE;

		/* the host just recovered, so reset the current host attempt */
		/* 11/11/05 EG - moved below */
		/*
		if(hst->current_state==HOST_UP)
			hst->current_attempt=1;
		*/

		/* write the host state change to the main log file */
		if(hst->state_type==HARD_STATE || (hst->state_type==SOFT_STATE && log_host_retries==TRUE))
			log_host_event(hst);

		/* check for start of flexible (non-fixed) scheduled downtime */
		if(hst->state_type==HARD_STATE)
			check_pending_flex_host_downtime(hst);

		/* notify contacts about the recovery or problem if its a "hard" state */
		if(hst->state_type==HARD_STATE)
			host_notification(hst,NOTIFICATION_NORMAL,NULL,NULL);

		/* handle the host state change */
		handle_host_event(hst);

		/* the host just recovered, so reset the current host attempt */
		if(hst->current_state==HOST_UP)
			hst->current_attempt=1;

		/* the host recovered, so reset the current notification number and state flags (after the recovery notification has gone out) */
		if(hst->current_state==HOST_UP){
			hst->current_notification_number=0;
			hst->notified_on_down=FALSE;
			hst->notified_on_unreachable=FALSE;
		        }
	        }

	/* else the host state has not changed */
	else{

		/* notify contacts if host is still down or unreachable */
		if(hst->current_state!=HOST_UP && hst->state_type==HARD_STATE)
			host_notification(hst,NOTIFICATION_NORMAL,NULL,NULL);

		/* if we're in a soft state and we should log host retries, do so now... */
		if(hst->state_type==SOFT_STATE && log_host_retries==TRUE)
			log_host_event(hst);
	        }

#ifdef DEBUG0
	printf("handle_host_state() end\n");
#endif

	return OK;
        }


