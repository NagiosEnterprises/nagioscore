/*****************************************************************************
 *
 * STATUSDATA.C - External status data for Nagios CGIs
 *
 * Copyright (c) 2000-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   08-19-2002
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

/*********** COMMON HEADER FILES ***********/

#include "config.h"
#include "common.h"
#include "objects.h"
#include "statusdata.h"

#ifdef NSCORE
#include "../base/nagios.h"
#endif
#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif

/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/

#ifdef USE_XSDDEFAULT
#include "../xdata/xsddefault.h"		/* default routines */
#endif
#ifdef USE_XSDDB
#include "../xdata/xsddb.h"                     /* database routines */
#endif



#ifdef NSCGI
hoststatus      *hoststatus_list=NULL;
servicestatus   *servicestatus_list=NULL;

time_t program_start;
int daemon_mode;
time_t last_command_check;
time_t last_log_rotation;
int enable_notifications;
int execute_service_checks;
int accept_passive_service_checks;
int enable_event_handlers;
int obsess_over_services;
int enable_flap_detection;
int enable_failure_prediction;
int process_performance_data;
int nagios_pid;
#endif

#ifdef NSCORE
extern time_t program_start;
extern int nagios_pid;
extern int daemon_mode;
extern time_t last_command_check;
extern time_t last_log_rotation;
extern int enable_notifications;
extern int execute_service_checks;
extern int accept_passive_service_checks;
extern int enable_event_handlers;
extern int obsess_over_services;
extern int enable_flap_detection;
extern int enable_failure_prediction;
extern int process_performance_data;
extern int aggregate_status_updates;
extern host *host_list;
extern service *service_list;
#endif



#ifdef NSCORE

/******************************************************************/
/****************** TOP-LEVEL OUTPUT FUNCTIONS ********************/
/******************************************************************/

/* initializes status data at program start */
int initialize_status_data(char *config_file){
	int result=OK;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_initialize_status_data(config_file);
#endif
#ifdef USE_XSDDB
	result=xsddb_initialize_status_data(config_file);
#endif

	return result;
        }


/* update all status data (aggregated dump) */
int update_all_status_data(void){
	host *temp_host;
	service *temp_service;
	int result=OK;


	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_begin_aggregated_dump();
#endif
#ifdef USE_XSDDB
	result=xsddb_begin_aggregated_dump();
#endif

	if(result!=OK)
		return ERROR;

	/* update program info */
	if(update_program_status(TRUE)==ERROR)
		return ERROR;

	/* update all host data */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(update_host_status(temp_host,TRUE)==ERROR)
			return ERROR;
	                }

	/* update all service data */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
		if(update_service_status(temp_service,TRUE)==ERROR)
			return ERROR;
	        }

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_end_aggregated_dump();
#endif
#ifdef USE_XSDDB
	result=xsddb_end_aggregated_dump();
#endif

	if(result!=OK)
		return ERROR;

	return OK;
        }


/* cleans up status data before program termination */
int cleanup_status_data(char *config_file,int delete_status_data){
	int result=OK;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_cleanup_status_data(config_file,delete_status_data);
#endif
#ifdef USE_XSDDB
	result=xsddb_cleanup_status_data(config_file,delete_status_data);
#endif

	return result;
        }



/* updates program status info */
int update_program_status(int aggregated_dump){
	int result=OK;

	/* don't update status if we're only dumping status data occasionally and this is not part of an aggregated dump */
	if(aggregate_status_updates==TRUE && aggregated_dump==FALSE)
		return OK;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_update_program_status(program_start,(int)getpid(),daemon_mode,last_command_check,last_log_rotation,enable_notifications,execute_service_checks,accept_passive_service_checks,enable_event_handlers,obsess_over_services,enable_flap_detection,enable_failure_prediction,process_performance_data,aggregated_dump);
#endif
#ifdef USE_XSDDB
	result=xsddb_update_program_status(program_start,(int)getpid(),daemon_mode,last_command_check,last_log_rotation,enable_notifications,execute_service_checks,accept_passive_service_checks,enable_event_handlers,obsess_over_services,enable_flap_detection,enable_failure_prediction,process_performance_data,aggregated_dump);
#endif

	return result;
        }



/* updates host status info */
int update_host_status(host *hst,int aggregated_dump){
	int result=OK;
	char *status_string="";
	time_t current_time;

	/* don't update status if we're only dumping status data occasionally and this is not part of an aggregated dump */
	if(aggregate_status_updates==TRUE && aggregated_dump==FALSE)
		return OK;

	time(&current_time);
	
	switch(hst->status){
	case HOST_DOWN:
		status_string="DOWN";
		break;
	case HOST_UNREACHABLE:
		status_string="UNREACHABLE";
		break;
	case HOST_UP:
		status_string="UP";
		break;
	default:
		status_string="?";
	        }

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_update_host_status(hst->name,status_string,current_time,hst->last_check,hst->last_state_change,hst->problem_has_been_acknowledged,hst->time_up,hst->time_down,hst->time_unreachable,hst->last_host_notification,hst->current_notification_number,hst->notifications_enabled,hst->event_handler_enabled,hst->checks_enabled,hst->flap_detection_enabled,hst->is_flapping,hst->percent_state_change,hst->scheduled_downtime_depth,hst->failure_prediction_enabled,hst->process_performance_data,hst->plugin_output,aggregated_dump);
#endif
#ifdef USE_XSDDB
	result=xsddb_update_host_status(hst->name,status_string,current_time,hst->last_check,hst->last_state_change,hst->problem_has_been_acknowledged,hst->time_up,hst->time_down,hst->time_unreachable,hst->last_host_notification,hst->current_notification_number,hst->notifications_enabled,hst->event_handler_enabled,hst->checks_enabled,hst->flap_detection_enabled,hst->is_flapping,hst->percent_state_change,hst->scheduled_downtime_depth,hst->failure_prediction_enabled,hst->process_performance_data,hst->plugin_output,aggregated_dump);
#endif

	return result;
        }



/* updates service status info */
int update_service_status(service *svc,int aggregated_dump){
	int result=OK;
	host *hst;
	char *status_string="";
	char *last_hard_state_string="";
	time_t current_time;

	/* don't update status if we're only dumping status data occasionally and this is not part of an aggregated dump */
	if(aggregate_status_updates==TRUE && aggregated_dump==FALSE)
		return OK;

	time(&current_time);

	/* find the host associated with this service */
	hst=find_host(svc->host_name,NULL);
	if(hst==NULL)
		return ERROR;

	/* update host status info if necessary */
	if(aggregated_dump==FALSE)
		update_host_status(hst,FALSE);

	/* get the service state string */
	if(svc->current_state==STATE_OK){
		/*
		if(svc->last_state==STATE_OK)
			status_string="OK";
		else
			status_string="RECOVERY";
		*/
		status_string="OK";
	        }
	else if(svc->current_state==STATE_CRITICAL)
		status_string="CRITICAL";
	else if(svc->current_state==STATE_WARNING)
		status_string="WARNING";
	else
		status_string="UNKNOWN";

	/* get the last hard state string */
	switch(svc->last_hard_state){
	case STATE_OK:
		last_hard_state_string="OK";
		break;
	case STATE_WARNING:
		last_hard_state_string="WARNING";
		break;
	case STATE_UNKNOWN:
		last_hard_state_string="UNKNOWN";
		break;
	case STATE_CRITICAL:
		last_hard_state_string="CRITICAL";
		break;
	default:
		last_hard_state_string="?";
	        }


	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_update_service_status(svc->host_name,svc->description,status_string,current_time,svc->current_attempt,svc->max_attempts,svc->state_type,svc->last_check,svc->next_check,svc->should_be_scheduled,svc->check_type,svc->checks_enabled,svc->accept_passive_service_checks,svc->event_handler_enabled,svc->last_state_change,svc->problem_has_been_acknowledged,last_hard_state_string,svc->time_ok,svc->time_warning,svc->time_unknown,svc->time_critical,svc->last_notification,svc->current_notification_number,svc->notifications_enabled,svc->latency,svc->execution_time,svc->flap_detection_enabled,svc->is_flapping,svc->percent_state_change,svc->scheduled_downtime_depth,svc->failure_prediction_enabled,svc->process_performance_data,svc->obsess_over_service,svc->plugin_output,aggregated_dump);
#endif
#ifdef USE_XSDDB
	result=xsddb_update_service_status(svc->host_name,svc->description,status_string,current_time,svc->current_attempt,svc->max_attempts,svc->state_type,svc->last_check,svc->next_check,svc->should_be_scheduled,svc->check_type,svc->checks_enabled,svc->accept_passive_service_checks,svc->event_handler_enabled,svc->last_state_change,svc->problem_has_been_acknowledged,last_hard_state_string,svc->time_ok,svc->time_warning,svc->time_unknown,svc->time_critical,svc->last_notification,svc->current_notification_number,svc->notifications_enabled,svc->latency,svc->execution_time,svc->flap_detection_enabled,svc->is_flapping,svc->percent_state_change,svc->scheduled_downtime_depth,svc->failure_prediction_enabled,svc->process_performance_data,svc->obsess_over_service,svc->plugin_output,aggregated_dump);
#endif


	return result;
        }

#endif





#ifdef NSCGI

/******************************************************************/
/******************* TOP-LEVEL INPUT FUNCTIONS ********************/
/******************************************************************/


/* reads in all status data */
int read_status_data(char *config_file,int options){
	int result=OK;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_read_status_data(config_file,options);
#endif
#ifdef USE_XSDDB
	result=xsddb_read_status_data(config_file,options);
#endif

	return result;
        }



/******************************************************************/
/********************** ADDITION FUNCTIONS ************************/
/******************************************************************/


/* sets program status variables */
int add_program_status(time_t _program_start, int _nagios_pid, int _daemon_mode, time_t _last_command_check, time_t _last_log_rotation, int _enable_notifications,int _execute_service_checks,int _accept_passive_service_checks,int _enable_event_handlers,int _obsess_over_services, int _enable_flap_detection, int _enable_failure_prediction, int _process_performance_data){

	/* make sure values are in range */
	if(_daemon_mode<0 || _daemon_mode>1)
		return ERROR;
	if(_enable_notifications<0 || _enable_notifications>1)
		return ERROR;
	if(_execute_service_checks<0 || _execute_service_checks>1)
		return ERROR;
	if(_accept_passive_service_checks<0 || _accept_passive_service_checks>1)
		return ERROR;
	if(_enable_event_handlers<0 || _enable_event_handlers>1)
		return ERROR;
	if(_obsess_over_services<0 || _obsess_over_services>1)
		return ERROR;
	if(_enable_flap_detection<0 || _enable_flap_detection>1)
		return ERROR;
	if(_enable_failure_prediction<0 || _enable_failure_prediction>1)
		return ERROR;
	if(_process_performance_data<0 || _process_performance_data>1)
		return ERROR;

	/* set program variables */
	program_start=_program_start;
	nagios_pid=_nagios_pid;
	daemon_mode=(_daemon_mode>0)?TRUE:FALSE;
	last_command_check=_last_command_check;
	last_log_rotation=_last_log_rotation;
	enable_notifications=(_enable_notifications>0)?TRUE:FALSE;
	execute_service_checks=(_execute_service_checks>0)?TRUE:FALSE;
	accept_passive_service_checks=(_accept_passive_service_checks>0)?TRUE:FALSE;
	enable_event_handlers=(_enable_event_handlers>0)?TRUE:FALSE;
	obsess_over_services=(_obsess_over_services>0)?TRUE:FALSE;
	enable_flap_detection=(_enable_flap_detection>0)?TRUE:FALSE;
	enable_failure_prediction=(_enable_failure_prediction>0)?TRUE:FALSE;
	process_performance_data=(_process_performance_data>0)?TRUE:FALSE;

	return OK;
        }



/* adds a host status entry to the list in memory */
int add_host_status(char *host_name,char *status_string,time_t last_update,time_t last_check,time_t last_state_change,int problem_has_been_acknowledged,unsigned long time_up, unsigned long time_down, unsigned long time_unreachable,time_t last_notification,int current_notification_number,int notifications_enabled,int event_handler_enabled,int checks_enabled,int flap_detection_enabled, int is_flapping, double percent_state_change, int scheduled_downtime_depth, int failure_prediction_enabled, int process_performance_data, char *plugin_output){
	hoststatus *new_hoststatus=NULL;
	hoststatus *last_hoststatus=NULL;
	hoststatus *temp_hoststatus=NULL;
	int status;

	/* make sure we have what we need */
	if(host_name==NULL)
		return ERROR;
	if(!strcmp(host_name,""))
		return ERROR;
	if(plugin_output==NULL)
		return ERROR;
	if(status_string==NULL)
		return ERROR;
	if(problem_has_been_acknowledged<0 || problem_has_been_acknowledged>1)
		return ERROR;
	if(current_notification_number<0)
		return ERROR;
	if(notifications_enabled<0 || notifications_enabled>1)
		return ERROR;
	if(event_handler_enabled<0 || event_handler_enabled>1)
		return ERROR;
	if(checks_enabled<0 || checks_enabled>1)
		return ERROR;
	if(flap_detection_enabled<0 || flap_detection_enabled>1)
		return ERROR;
	if(is_flapping<0 || is_flapping>1)
		return ERROR;
	if(failure_prediction_enabled<0 || failure_prediction_enabled>1)
		return ERROR;
	if(process_performance_data<0 || process_performance_data>1)
		return ERROR;

	if(!strcmp(status_string,"DOWN"))
		status=HOST_DOWN;
	else if(!strcmp(status_string,"UNREACHABLE"))
		status=HOST_UNREACHABLE;
	else if(!strcmp(status_string,"PENDING"))
		status=HOST_PENDING;
	else if(!strcmp(status_string,"UP"))
		status=HOST_UP;
	else
		return ERROR;

	/* allocate memory for new host status */
	new_hoststatus=(hoststatus *)malloc(sizeof(hoststatus));
	if(new_hoststatus==NULL)
		return ERROR;

	/* host name */
	new_hoststatus->host_name=strdup(host_name);
	if(new_hoststatus->host_name==NULL){
		free(new_hoststatus);
		return ERROR;
	        }
	strcpy(new_hoststatus->host_name,host_name);

	/* host status */
	new_hoststatus->status=status;

	/* time this status data was last updated */
	new_hoststatus->last_update=last_update;

	/* time this host was last checked */
	new_hoststatus->last_check=last_check;

	/* time this host last changed state */
	new_hoststatus->last_state_change=last_state_change;

	/* has this host problem been acknowleged? */
	new_hoststatus->problem_has_been_acknowledged=(problem_has_been_acknowledged>0)?TRUE:FALSE;

	/* time up, down and unreachable */
	new_hoststatus->time_up=time_up;
	new_hoststatus->time_down=time_down;
	new_hoststatus->time_unreachable=time_unreachable;

	/* last notification time for this host */
	new_hoststatus->last_notification=last_notification;

	/* current notification number */
	new_hoststatus->current_notification_number=current_notification_number;

	/* notifications enabled option */
	new_hoststatus->notifications_enabled=(notifications_enabled>0)?TRUE:FALSE;

	/* event handler enabled option */
	new_hoststatus->event_handler_enabled=(event_handler_enabled>0)?TRUE:FALSE;

	/* checks enabled option */
	new_hoststatus->checks_enabled=(checks_enabled>0)?TRUE:FALSE;

	/* flap detection enabled option */
	new_hoststatus->flap_detection_enabled=(flap_detection_enabled>0)?TRUE:FALSE;

	/* flapping indicator */
	new_hoststatus->is_flapping=(is_flapping>0)?TRUE:FALSE;

	/* percent state change */
	new_hoststatus->percent_state_change=percent_state_change;

	/* scheduled downtime depth */
	new_hoststatus->scheduled_downtime_depth=scheduled_downtime_depth;

	/* failure prediction enabled option */
	new_hoststatus->failure_prediction_enabled=(failure_prediction_enabled>0)?TRUE:FALSE;

	/* performance processing enabled */
	new_hoststatus->process_performance_data=(process_performance_data>0)?TRUE:FALSE;

	/* plugin output */
	new_hoststatus->information=strdup(plugin_output);
	if(new_hoststatus->information==NULL){
		free(new_hoststatus->host_name);
		free(new_hoststatus);
		return ERROR;
	        }



	/* add new host status to list, sorted by host name */
	last_hoststatus=hoststatus_list;
	for(temp_hoststatus=hoststatus_list;temp_hoststatus!=NULL;temp_hoststatus=temp_hoststatus->next){
		if(strcmp(new_hoststatus->host_name,temp_hoststatus->host_name)<0){
			new_hoststatus->next=temp_hoststatus;
			if(temp_hoststatus==hoststatus_list)
				hoststatus_list=new_hoststatus;
			else
				last_hoststatus->next=new_hoststatus;
			break;
		        }
		else
			last_hoststatus=temp_hoststatus;
	        }
	if(hoststatus_list==NULL){
		new_hoststatus->next=NULL;
		hoststatus_list=new_hoststatus;
	        }
	else if(temp_hoststatus==NULL){
		new_hoststatus->next=NULL;
		last_hoststatus->next=new_hoststatus;
	        }


	return OK;
        }


/* adds a service status entry to the list in memory */
int add_service_status(char *host_name,char *svc_description,char *status_string,time_t last_update,int current_attempt,int max_attempts,int state_type,time_t last_check,time_t next_check,int check_type,int checks_enabled,int accept_passive_checks,int event_handler_enabled,time_t last_state_change,int problem_has_been_acknowledged,char *last_hard_state_string,unsigned long time_ok,unsigned long time_warning,unsigned long time_unknown,unsigned long time_critical,time_t last_notification,int current_notification_number,int notifications_enabled, int latency, int execution_time, int flap_detection_enabled, int is_flapping, double percent_state_change, int scheduled_downtime_depth, int failure_prediction_enabled, int process_performance_data, int obsess_over_service, char *plugin_output){
	servicestatus *new_svcstatus=NULL;
	servicestatus *last_svcstatus=NULL;
	servicestatus *temp_svcstatus=NULL;
	int status;
	int last_hard_state;


	/* make sure we have what we need */
	if(host_name==NULL)
		return ERROR;
	if(!strcmp(host_name,""))
		return ERROR;
	if(svc_description==NULL)
		return ERROR;
	if(!strcmp(svc_description,""))
		return ERROR;
	if(plugin_output==NULL)
		return ERROR;
	if(status_string==NULL)
		return ERROR;
	if(last_hard_state_string==NULL)
		return ERROR;
	if(current_attempt<0)
		return ERROR;
	if(max_attempts<1)
		return ERROR;
	if(checks_enabled<0 || checks_enabled>1)
		return ERROR;
	if(accept_passive_checks<0 || accept_passive_checks>1)
		return ERROR;
	if(event_handler_enabled<0 || event_handler_enabled>1)
		return ERROR;
	if(problem_has_been_acknowledged<0 || problem_has_been_acknowledged>1)
		return ERROR;
	if(notifications_enabled<0 || notifications_enabled>1)
		return ERROR;
	if(flap_detection_enabled<0 || flap_detection_enabled>1)
		return ERROR;
	if(is_flapping<0 || is_flapping>1)
		return ERROR;
	if(failure_prediction_enabled<0 || failure_prediction_enabled>1)
		return ERROR;
	if(process_performance_data<0 || process_performance_data>1)
		return ERROR;

	if(!strcmp(status_string,"WARNING"))
		status=SERVICE_WARNING;
	else if(!strcmp(status_string,"UNKNOWN"))
		status=SERVICE_UNKNOWN;
	else if(!strcmp(status_string,"CRITICAL"))
		status=SERVICE_CRITICAL;
	else if(!strcmp(status_string,"RECOVERY"))
		status=SERVICE_RECOVERY;
	else if(!strcmp(status_string,"HOST DOWN"))
		status=SERVICE_HOST_DOWN;
	else if(!strcmp(status_string,"UNREACHABLE"))
		status=SERVICE_UNREACHABLE;
	else if(!strcmp(status_string,"PENDING"))
		status=SERVICE_PENDING;
	else if(!strcmp(status_string,"OK"))
		status=SERVICE_OK;
	else
		return ERROR;

	if(!strcmp(last_hard_state_string,"WARNING"))
		last_hard_state=STATE_WARNING;
	else if(!strcmp(last_hard_state_string,"UNKNOWN"))
		last_hard_state=STATE_UNKNOWN;
	else if(!strcmp(last_hard_state_string,"CRITICAL"))
		last_hard_state=STATE_CRITICAL;
	else if(!strcmp(last_hard_state_string,"OK"))
		last_hard_state=STATE_OK;
	else
		return ERROR;


	/* allocate memory for a new service status entry */
	new_svcstatus=(servicestatus *)malloc(sizeof(servicestatus));
	if(new_svcstatus==NULL)
		return ERROR;

	/* host name */
	new_svcstatus->host_name=strdup(host_name);
	if(new_svcstatus->host_name==NULL){
		free(new_svcstatus);
		return ERROR;
	        }

	/* service description */
	new_svcstatus->description=strdup(svc_description);
	if(new_svcstatus->description==NULL){
		free(new_svcstatus->host_name);
		free(new_svcstatus);
		return ERROR;
	        }

	/* status */
	new_svcstatus->status=status;

	/* time this service status info was last updated */
	new_svcstatus->last_update=last_update;

	/* current attempt */
	new_svcstatus->current_attempt=current_attempt;

	/* max attempts */
	new_svcstatus->max_attempts=max_attempts;

	/* state type */
	new_svcstatus->state_type=state_type;

	/* last check time */
	new_svcstatus->last_check=last_check;

	/* next check time */
	new_svcstatus->next_check=next_check;

	/* last check type */
	new_svcstatus->check_type=check_type;

	/* checks enabled */
	new_svcstatus->checks_enabled=(checks_enabled>0)?TRUE:FALSE;

	/* passive checks accepted */
	new_svcstatus->accept_passive_service_checks=(accept_passive_checks>0)?TRUE:FALSE;

	/* event handler enabled */
	new_svcstatus->event_handler_enabled=(event_handler_enabled>0)?TRUE:FALSE;

	/* last state change time */
	new_svcstatus->last_state_change=last_state_change;

	/* acknowledgement */
	new_svcstatus->problem_has_been_acknowledged=(problem_has_been_acknowledged>0)?TRUE:FALSE;

	/* last hard state */
	new_svcstatus->last_hard_state=last_hard_state;

	/* time ok, warning, unknown, and critical */
	new_svcstatus->time_ok=time_ok;
	new_svcstatus->time_warning=time_warning;
	new_svcstatus->time_unknown=time_unknown;
	new_svcstatus->time_critical=time_critical;

	/* last notification time */
	new_svcstatus->last_notification=last_notification;

	/* current notification number */
	new_svcstatus->current_notification_number=current_notification_number;

	/* notifications enabled */
	new_svcstatus->notifications_enabled=(notifications_enabled>0)?TRUE:FALSE;

	/* failure prediction enabled */
	new_svcstatus->failure_prediction_enabled=(failure_prediction_enabled>0)?TRUE:FALSE;

	/* performance data processing */
	new_svcstatus->process_performance_data=(process_performance_data>0)?TRUE:FALSE;

	/* obsess over service */
	new_svcstatus->obsess_over_service=(obsess_over_service>0)?TRUE:FALSE;

	/* plugin output */
	new_svcstatus->information=strdup(plugin_output);
	if(new_svcstatus->information==NULL){
		free(new_svcstatus->description);
		free(new_svcstatus->host_name);
		free(new_svcstatus);
		return ERROR;
	        }

	/* latency and execution time */
	new_svcstatus->latency=latency;
	new_svcstatus->execution_time=execution_time;

	/* flap detection enabled */
	new_svcstatus->flap_detection_enabled=(flap_detection_enabled>0)?TRUE:FALSE;

	/* flapping indicator */
	new_svcstatus->is_flapping=(is_flapping>0)?TRUE:FALSE;

	/* percent state change */
	new_svcstatus->percent_state_change=percent_state_change;

	/* scheduled downtime depth */
	new_svcstatus->scheduled_downtime_depth=scheduled_downtime_depth;


	/* add new service status to list, sorted by host name then description */
	last_svcstatus=servicestatus_list;
	for(temp_svcstatus=servicestatus_list;temp_svcstatus!=NULL;temp_svcstatus=temp_svcstatus->next){

		if(strcmp(new_svcstatus->host_name,temp_svcstatus->host_name)<0){
			new_svcstatus->next=temp_svcstatus;
			if(temp_svcstatus==servicestatus_list)
				servicestatus_list=new_svcstatus;
			else
				last_svcstatus->next=new_svcstatus;
			break;
		        }

		else if(strcmp(new_svcstatus->host_name,temp_svcstatus->host_name)==0 && strcmp(new_svcstatus->description,temp_svcstatus->description)<0){
			new_svcstatus->next=temp_svcstatus;
			if(temp_svcstatus==servicestatus_list)
				servicestatus_list=new_svcstatus;
			else
				last_svcstatus->next=new_svcstatus;
			break;
		        }

		else
			last_svcstatus=temp_svcstatus;
	        }
	if(servicestatus_list==NULL){
		new_svcstatus->next=NULL;
		servicestatus_list=new_svcstatus;
	        }
	else if(temp_svcstatus==NULL){
		new_svcstatus->next=NULL;
		last_svcstatus->next=new_svcstatus;
	        }


	return OK;
        }





/******************************************************************/
/*********************** CLEANUP FUNCTIONS ************************/
/******************************************************************/


/* free all memory for status data */
void free_status_data(void){
	hoststatus *this_hoststatus;
	hoststatus *next_hoststatus;
	servicestatus *this_svcstatus;
	servicestatus *next_svcstatus;

	/* free memory for the host status list */
	for(this_hoststatus=hoststatus_list;this_hoststatus!=NULL;this_hoststatus=next_hoststatus){
		next_hoststatus=this_hoststatus->next;
		free(this_hoststatus->host_name);
		free(this_hoststatus->information);
		free(this_hoststatus);
	        }

	/* free memory for the service status list */
	for(this_svcstatus=servicestatus_list;this_svcstatus!=NULL;this_svcstatus=next_svcstatus){
		next_svcstatus=this_svcstatus->next;
		free(this_svcstatus->host_name);
		free(this_svcstatus->description);
		free(this_svcstatus->information);
		free(this_svcstatus);
	        }

	/* reset list pointers */
	hoststatus_list=NULL;
	servicestatus_list=NULL;

	return;
        }




/******************************************************************/
/************************ SEARCH FUNCTIONS ************************/
/******************************************************************/


/* find a host status entry */
hoststatus *find_hoststatus(char *host_name){
	hoststatus *temp_hoststatus;

	for(temp_hoststatus=hoststatus_list;temp_hoststatus!=NULL;temp_hoststatus=temp_hoststatus->next){
		if(!strcmp(temp_hoststatus->host_name,host_name))
			return temp_hoststatus;
	        }

	return NULL;
        }


/* find a service status entry */
servicestatus *find_servicestatus(char *host_name,char *svc_desc){
	servicestatus *temp_servicestatus;

	for(temp_servicestatus=servicestatus_list;temp_servicestatus!=NULL;temp_servicestatus=temp_servicestatus->next){
		if(!strcmp(temp_servicestatus->host_name,host_name) && !strcmp(temp_servicestatus->description,svc_desc))
			return temp_servicestatus;
	        }

	return NULL;
        }




/******************************************************************/
/*********************** UTILITY FUNCTIONS ************************/
/******************************************************************/


/* gets the total number of services of a certain state for a specific host */
int get_servicestatus_count(char *host_name, int type){
	servicestatus *temp_status;
	int count=0;

	if(host_name==NULL)
		return 0;

	for(temp_status=servicestatus_list;temp_status!=NULL;temp_status=temp_status->next){
		if(temp_status->status & type){
			if(!strcmp(host_name,temp_status->host_name))
				count++;
		        }
	        }

	return count;
        }



#endif

