/*****************************************************************************
 *
 * SEHANDLERS.C - Service and host event and state handlers for Nagios
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   10-16-2002
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
#include "../common/statusdata.h"
#include "../common/downtime.h"
#include "nagios.h"


extern int             enable_event_handlers;
extern int             obsess_over_services;

extern int             log_event_handlers;
extern int             log_host_retries;

extern int             event_handler_timeout;
extern int             ocsp_timeout;

extern char            *macro_current_service_attempt;
extern char            *macro_current_host_attempt;
extern char            *macro_host_state;
extern char	       *macro_service_state;
extern char            *macro_state_type;

extern char            *global_host_event_handler;
extern char            *global_service_event_handler;

extern char            *ocsp_command;

extern time_t          program_start;



/******************************************************************/
/******** OBSESSIVE COMPULSIVE SERVICE HANDLER FUNCTIONS **********/
/******************************************************************/


/* handles service check results in an obsessive compulsive manner... */
int obsessive_compulsive_service_check_processor(service *svc,int state_type){
	char raw_command_line[MAX_INPUT_BUFFER];
	char processed_command_line[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	command *temp_command;
	host *temp_host;
	int early_timeout=FALSE;

#ifdef DEBUG0
	printf("obsessive_compulsive_service_check_processor() start\n");
#endif

	/* bail out if we shouldn't be obsessing */
	if(obsess_over_services==FALSE)
		return OK;
	if(svc->obsess_over_service==FALSE)
		return OK;

	/* find the associated host */
	temp_host=find_host(svc->host_name);

	/* update service macros */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);

	/* grab the service state type macro */
	if(macro_state_type!=NULL)
		free(macro_state_type);
	macro_state_type=(char *)malloc(MAX_STATETYPE_LENGTH);
	if(macro_state_type!=NULL)
		strcpy(macro_state_type,(state_type==HARD_STATE)?"HARD":"SOFT");

	/* grab the current service check number macro */
	if(macro_current_service_attempt!=NULL)
		free(macro_current_service_attempt);
	macro_current_service_attempt=(char *)malloc(MAX_ATTEMPT_LENGTH);
	if(macro_current_service_attempt!=NULL)
		sprintf(macro_current_service_attempt,"%d",svc->current_attempt);

	/* find the service processor command */
	temp_command=find_command(ocsp_command,NULL);

	/* if there is no valid command, exit */
	if(temp_command==NULL)
		return ERROR;

	/* get the raw command line to execute */
	strncpy(raw_command_line,temp_command->command_line,sizeof(raw_command_line));
	raw_command_line[sizeof(raw_command_line)-1]='\x0';

#ifdef DEBUG3
	printf("\tRaw obsessive compulsive service processor command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS);

#ifdef DEBUG3
	printf("\tProcessed obsessive compulsive service processor command line: %s\n",processed_command_line);
#endif

	/* run the command */
	my_system(processed_command_line,ocsp_timeout,&early_timeout,NULL,0);

	/* check to see if the command timed out */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: OCSP command '%s' for service '%s' on host '%s' timed out after %d seconds\n",ocsp_command,svc->description,svc->host_name,ocsp_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }
	
#ifdef DEBUG0
	printf("obsessive_compulsive_service_check_processor() end\n");
#endif

	return OK;
        }





/******************************************************************/
/**************** SERVICE EVENT HANDLER FUNCTIONS *****************/
/******************************************************************/


/* handles changes in the state of a service */
int handle_service_event(service *svc,int state_type){
	host *temp_host;

#ifdef DEBUG0
	printf("handle_service_event() start\n");
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

	/* grab the service state type macro */
	if(macro_state_type!=NULL)
		free(macro_state_type);
	macro_state_type=(char *)malloc(MAX_STATETYPE_LENGTH);
	if(macro_state_type!=NULL)
		strcpy(macro_state_type,(state_type==HARD_STATE)?"HARD":"SOFT");

	/* grab the current service check number macro */
	if(macro_current_service_attempt!=NULL)
		free(macro_current_service_attempt);
	macro_current_service_attempt=(char *)malloc(MAX_ATTEMPT_LENGTH);
	if(macro_current_service_attempt!=NULL)
		sprintf(macro_current_service_attempt,"%d",svc->current_attempt);

	/* run the global service event handler */
	run_global_service_event_handler(svc,state_type);

	/* run the event handler command if there is one */
	if(svc->event_handler!=NULL)
		run_service_event_handler(svc,state_type);

	/* check for external commands - the event handler may have given us some directives... */
	check_for_external_commands();

#ifdef DEBUG0
	printf("handle_service_event() end\n");
#endif

	return OK;
        }



/* runs the global service event handler */
int run_global_service_event_handler(service *svc,int state_type){
	char raw_command_line[MAX_INPUT_BUFFER];
	char processed_command_line[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	command *temp_command;
	int early_timeout=FALSE;

#ifdef DEBUG0
	printf("run_global_service_event_handler() start\n");
#endif

	/* bail out if we shouldn't be running event handlers */
	if(enable_event_handlers==FALSE)
		return OK;

	/* a global service event handler command has not been defined */
	if(global_service_event_handler==NULL)
		return ERROR;

	/* get the raw command line */
	get_raw_command_line(global_service_event_handler,raw_command_line,sizeof(raw_command_line));
	strip(raw_command_line);

#ifdef DEBUG3
	printf("\tRaw global service event handler command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS);

#ifdef DEBUG3
	printf("\tProcessed global service event handler command line: %s\n",processed_command_line);
#endif

	if(log_event_handlers==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"GLOBAL SERVICE EVENT HANDLER: %s;%s;%s;%s;%s;%s\n",svc->host_name,svc->description,macro_service_state,macro_state_type,macro_current_service_attempt,global_service_event_handler);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER,FALSE);
	        }

	/* run the command */
	my_system(processed_command_line,event_handler_timeout,&early_timeout,NULL,0);

	/* check to see if the event handler timed out */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Global service event handler command '%s' timed out after %d seconds\n",global_service_event_handler,event_handler_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
	        }

#ifdef DEBUG0
	printf("run_global_service_event_handler() end\n");
#endif

	return OK;
        }



/* runs a service event handler command */
int run_service_event_handler(service *svc,int state_type){
	char raw_command_line[MAX_INPUT_BUFFER];
	char processed_command_line[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	int early_timeout=FALSE;

#ifdef DEBUG0
	printf("run_service_event_handler() start\n");
#endif

	/* get the raw command line */
	get_raw_command_line(svc->event_handler,raw_command_line,sizeof(raw_command_line));
	strip(raw_command_line);

#ifdef DEBUG3
	printf("\tRaw service event handler command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS);

#ifdef DEBUG3
	printf("\tProcessed service event handler command line: %s\n",processed_command_line);
#endif

	if(log_event_handlers==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"SERVICE EVENT HANDLER: %s;%s;%s;%s;%s;%s\n",svc->host_name,svc->description,macro_service_state,macro_state_type,macro_current_service_attempt,svc->event_handler);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER,FALSE);
	        }

	/* run the command */
	my_system(processed_command_line,event_handler_timeout,&early_timeout,NULL,0);

	/* check to see if the event handler timed out */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Service event handler command '%s' timed out after %d seconds\n",svc->event_handler,event_handler_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
	        }
#ifdef DEBUG0
	printf("run_service_event_handler() end\n");
#endif

	return OK;
        }




/******************************************************************/
/****************** HOST EVENT HANDLER FUNCTIONS ******************/
/******************************************************************/


/* handles a change in the status of a host */
int handle_host_event(host *hst,int state,int state_type){

#ifdef DEBUG0
	printf("handle_host_event() start\n");
#endif

	/* bail out if we shouldn't be running event handlers */
	if(enable_event_handlers==FALSE)
		return OK;
	if(hst->event_handler_enabled==FALSE)
		return OK;

	/* update host macros */
	grab_host_macros(hst);

	/* grab the host state type macro */
	if(macro_state_type!=NULL)
		free(macro_state_type);
	macro_state_type=(char *)malloc(MAX_STATETYPE_LENGTH);
	if(macro_state_type!=NULL)
		strcpy(macro_state_type,(state_type==HARD_STATE)?"HARD":"SOFT");

	/* make sure the host state macro is correct */
	if(macro_host_state!=NULL)
		free(macro_host_state);
	macro_host_state=(char *)malloc(MAX_STATE_LENGTH);
	if(macro_host_state!=NULL){
		if(state==HOST_DOWN)
			strcpy(macro_host_state,"DOWN");
		else if(state==HOST_UNREACHABLE)
			strcpy(macro_host_state,"UNREACHABLE");
		else
			strcpy(macro_host_state,"UP");
	        }

	/* run the global host event handler */
	run_global_host_event_handler(hst,state,state_type);

	/* run the event handler command if there is one */
	if(hst->event_handler!=NULL)
		run_host_event_handler(hst,state,state_type);

	/* check for external commands - the event handler may have given us some directives... */
	check_for_external_commands();

#ifdef DEBUG0
	printf("handle_host_event() end\n");
#endif

	return OK;
        }


/* runs the global host event handler */
int run_global_host_event_handler(host *hst,int state,int state_type){
	char raw_command_line[MAX_INPUT_BUFFER];
	char processed_command_line[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	int early_timeout=FALSE;

#ifdef DEBUG0
	printf("run_global_host_event_handler() start\n");
#endif

	/* bail out if we shouldn't be running event handlers */
	if(enable_event_handlers==FALSE)
		return OK;

	/* no global host event handler command is defined */
	if(global_host_event_handler==NULL)
		return ERROR;

	/* get the raw command line */
	get_raw_command_line(global_host_event_handler,raw_command_line,sizeof(raw_command_line));
	strip(raw_command_line);

#ifdef DEBUG3
	printf("\tRaw global host event handler command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS);

#ifdef DEBUG3
	printf("\tProcessed global host event handler command line: %s\n",processed_command_line);
#endif

	if(log_event_handlers==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"GLOBAL HOST EVENT HANDLER: %s;%s;%s;%s;%s\n",hst->name,macro_host_state,macro_state_type,macro_current_host_attempt,global_host_event_handler);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER,FALSE);
	        }

	/* run the command */
	my_system(processed_command_line,event_handler_timeout,&early_timeout,NULL,0);

	/* check for a timeout in the execution of the event handler command */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Global host event handler command '%s' timed out after %d seconds\n",global_host_event_handler,event_handler_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
	        }

#ifdef DEBUG0
	printf("run_global_host_event_handler() end\n");
#endif

	return OK;
        }


/* runs a host event handler command */
int run_host_event_handler(host *hst,int state,int state_type){
	char raw_command_line[MAX_INPUT_BUFFER];
	char processed_command_line[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	int early_timeout=FALSE;

#ifdef DEBUG0
	printf("run_host_event_handler() start\n");
#endif

	/* get the raw command line */
	get_raw_command_line(hst->event_handler,raw_command_line,sizeof(raw_command_line));
	strip(raw_command_line);

#ifdef DEBUG3
	printf("\tRaw host event handler command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS);

#ifdef DEBUG3
	printf("\tProcessed host event handler command line: %s\n",processed_command_line);
#endif

	if(log_event_handlers==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"HOST EVENT HANDLER: %s;%s;%s;%s;%s\n",hst->name,macro_host_state,macro_state_type,macro_current_host_attempt,hst->event_handler);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER,FALSE);
	        }

	/* run the command */
	my_system(processed_command_line,event_handler_timeout,&early_timeout,NULL,0);

	/* check to see if the event handler timed out */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Host event handler command '%s' timed out after %d seconds\n",hst->event_handler,event_handler_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_EVENT_HANDLER | NSLOG_RUNTIME_WARNING,TRUE);
	        }
#ifdef DEBUG0
	printf("run_host_event_handler() end\n");
#endif

	return OK;
        }




/******************************************************************/
/**************** SERVICE STATE HANDLER FUNCTIONS *****************/
/******************************************************************/


/* updates service state times */
void update_service_state_times(service *svc){
	unsigned long time_difference;
	time_t current_time;

#ifdef DEBUG0
	printf("update_service_state_times() start\n");
#endif

	/* calculate the time since the last service state change */
	time(&current_time);

	/* if this is NOT the first time we've had a service check/state change.. */
	if(svc->has_been_checked==TRUE){

		if(svc->last_state_change<program_start)
			time_difference=(unsigned long)current_time-program_start;
		else
			time_difference=(unsigned long)current_time-svc->last_state_change;

		/* use last hard state... */
		if(svc->last_hard_state==STATE_UNKNOWN)
			svc->time_unknown+=time_difference;
		else if(svc->last_hard_state==STATE_WARNING)
			svc->time_warning+=time_difference;
		else if(svc->last_hard_state==STATE_CRITICAL)
			svc->time_critical+=time_difference;
		else
			svc->time_ok+=time_difference;
	        }


	/* update the last service state change time */
	svc->last_state_change=current_time;

	/* update status log with service information */
	update_service_status(svc,FALSE);


#ifdef DEBUG0
	printf("update_service_state_times() end\n");
#endif
	
	return;
        }




/******************************************************************/
/****************** HOST STATE HANDLER FUNCTIONS ******************/
/******************************************************************/


/* top level host state handler */
int handle_host_state(host *hst,int state,int state_type){
	int state_change=FALSE;

#ifdef DEBUG0
	printf("handle_host_state() start\n");
#endif


	/* has the host state changed? */
	if(hst->status!=state)
		state_change=TRUE;

	/* if the host state has changed... */
	if(state_change==TRUE){

		/* reset the acknowledgement flag */
		hst->problem_has_been_acknowledged=FALSE;

		/* reset the next and last notification times */
		hst->last_host_notification=(time_t)0;
		hst->next_host_notification=(time_t)0;

		/* set the state flags in case we "float" between down and unreachable states before a recovery */
		if(state_type==HARD_STATE){
			if(state==HOST_DOWN)
				hst->has_been_down=TRUE;
			else if(state==HOST_UNREACHABLE)
				hst->has_been_unreachable=TRUE;
		        }

		/* update the host state times */
		if(state_type==HARD_STATE)
			update_host_state_times(hst);

		/* the host just recovered, so reset the current host attempt number */
		if(state==HOST_UP)
			hst->current_attempt=1;

		/* write the host state change to the main log file */
		if(state_type==HARD_STATE || (state_type==SOFT_STATE && log_host_retries==TRUE))
			log_host_event(hst,state,state_type);

		/* check for start of flexible (non-fixed) scheduled downtime */
		/*if(state_type==HARD_STATE && hst->pending_flex_downtime>0)*/
		if(state_type==HARD_STATE)
			check_pending_flex_host_downtime(hst,state);

		/* notify contacts about the recovery or problem if its a "hard" state */
		if(state_type==HARD_STATE)
			host_notification(hst,state,NULL);

		/* handle the host state change */
		handle_host_event(hst,state,state_type);

		/* the host recovered, so reset the current notification number and state flags (after the recovery notification has gone out) */
		if(state==HOST_UP){
			hst->current_notification_number=0;
			hst->has_been_down=FALSE;
			hst->has_been_unreachable=FALSE;
		        }

		/* set the host state flag and update the status log if this is a hard state */
		if(state_type==HARD_STATE){
			hst->status=state;
			update_host_status(hst,FALSE);
		        }
	        }

	/* else the host state has not changed */
	else{

		/* notify contacts if host is still down */
		if(state!=HOST_UP && state_type==HARD_STATE)
			host_notification(hst,state,NULL);

		/* if we're in a soft state and we should log host retries, do so now... */
		if(state_type==SOFT_STATE && log_host_retries==TRUE)
			log_host_event(hst,state,state_type);

		/* update the status log if this is a hard state */
		if(state_type==HARD_STATE)
			update_host_status(hst,FALSE);

	        }

#ifdef DEBUG0
	printf("handle_host_state() end\n");
#endif

	return OK;
        }




/* updates host state times */
void update_host_state_times(host *hst){
	unsigned long time_difference;
	time_t current_time;

#ifdef DEBUG0
	printf("update_host_state_times() start\n");
#endif

	/* get the current time */
	time(&current_time);

	/* if this is NOT the first time we've had a host check/state change... */
	if(hst->has_been_checked==TRUE){

		if(hst->last_state_change<program_start)
			time_difference=(unsigned long)current_time-program_start;
		else
			time_difference=(unsigned long)current_time-hst->last_state_change;

		if(hst->status==HOST_DOWN)
			hst->time_down+=time_difference;
		else if(hst->status==HOST_UNREACHABLE)
			hst->time_unreachable+=time_difference;
		else
			hst->time_up+=time_difference;
	        }

	/* update the last host state change time */
	hst->last_state_change=current_time;

	/* update status log with host information */
	update_host_status(hst,FALSE);


#ifdef DEBUG0
	printf("update_host_state_times() end\n");
#endif
	
	return;
        }

