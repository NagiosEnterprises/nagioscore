/*****************************************************************************
 *
 * SRETENTION.C - State retention routines for Nagios
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   01-11-2002
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

#include "../common/config.h"
#include "../common/common.h"
#include "../common/objects.h"
#include "../common/statusdata.h"
#include "nagios.h"
#include "sretention.h"

extern host           *host_list;
extern service        *service_list;

extern int            interval_length;

extern int            enable_notifications;
extern int            execute_service_checks;
extern int            accept_passive_service_checks;
extern int            enable_event_handlers;
extern int            obsess_over_services;
extern int            enable_flap_detection;
extern int            enable_failure_prediction;
extern int            process_performance_data;

extern int            retain_state_information;
extern int            use_retained_program_state;

extern time_t         program_start;


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/
#ifdef USE_XRDDEFAULT
#include "../xdata/xrddefault.h"		/* default routines */
#endif
#ifdef USE_XRDDB
#include "../xdata/xrddb.h"			/* database routines */
#endif






/******************************************************************/
/************* TOP-LEVEL STATE INFORMATION FUNCTIONS **************/
/******************************************************************/


/* save all host and service state information */
int save_state_information(char *main_config_file, int autosave){
	char buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("save_state_information() start\n");
#endif

	if(retain_state_information==FALSE)
		return OK;

	/********* IMPLEMENTATION-SPECIFIC OUTPUT FUNCTION ********/
#ifdef USE_XRDDEFAULT
	if(xrddefault_save_state_information(main_config_file)==ERROR)
		return ERROR;
#endif
#ifdef USE_XRDDB
	if(xrddb_save_state_information(main_config_file)==ERROR)
		return ERROR;
#endif

	if(autosave==TRUE){
		snprintf(buffer,sizeof(buffer),"Auto-save of retention data completed successfully.\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO,FALSE);
	        }

#ifdef DEBUG0
	printf("save_state_information() end\n");
#endif

	return OK;
        }




/* reads in initial host and state information */
int read_initial_state_information(char *main_config_file){

#ifdef DEBUG0
	printf("read_initial_state_information() start\n");
#endif

	if(retain_state_information==FALSE)
		return OK;

	/********* IMPLEMENTATION-SPECIFIC INPUT FUNCTION ********/
#ifdef USE_XRDDEFAULT
	if(xrddefault_read_state_information(main_config_file)==ERROR)
		return ERROR;
#endif
#ifdef USE_XRDDB
	if(xrddb_read_state_information(main_config_file)==ERROR)
		return ERROR;
#endif

#ifdef DEBUG0
	printf("read_initial_state_information() end\n");
#endif

	return OK;
        }




/******************************************************************/
/****************** STATE INFORMATION FUNCTIONS *******************/
/******************************************************************/


/* sets initial program information */
int set_program_state_information(int notifications, int service_checks, int passive_service_checks, int event_handlers, int obsess_services, int flap_detection, int failure_prediction, int performance_data){

#ifdef DEBUG0
	printf("set_program_state_information() start\n");
#endif

	/* don't initialize program state if we're not supposed to... */
	if(use_retained_program_state==FALSE)
		return OK;

	/* check validity of data we've been given */
	if(notifications<0 || notifications>1)
		return ERROR;

	if(service_checks<0 || service_checks>1)
		return ERROR;

	if(passive_service_checks<0 || passive_service_checks>1)
		return ERROR;

	if(event_handlers<0 || event_handlers>1)
		return ERROR;

	if(obsess_services<0 || obsess_services>1)
		return ERROR;

	if(flap_detection<0 || flap_detection>1)
		return ERROR;

	if(failure_prediction<0 || failure_prediction>1)
		return ERROR;

	if(performance_data<0 || performance_data>1)
		return ERROR;

	enable_notifications=(notifications>0)?TRUE:FALSE;
	execute_service_checks=(service_checks>0)?TRUE:FALSE;
	accept_passive_service_checks=(passive_service_checks>0)?TRUE:FALSE;
	enable_event_handlers=(event_handlers>0)?TRUE:FALSE;
	obsess_over_services=(obsess_services>0)?TRUE:FALSE;
	enable_flap_detection=(flap_detection>0)?TRUE:FALSE;
	enable_failure_prediction=(failure_prediction>0)?TRUE:FALSE;
	process_performance_data=(performance_data>0)?TRUE:FALSE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("set_program_state_information() end\n");
#endif

	return OK;
        }


/* sets initial service state information */
int set_service_state_information(char *host_name, char *description, int state, char *output, unsigned long last_check, int check_type, unsigned long time_ok, unsigned long time_warning, unsigned long time_unknown, unsigned long time_critical, unsigned long last_notification, int current_notification_number, int notifications_enabled, int checks_enabled, int accept_passive_checks, int event_handler_enabled, int problem_has_been_acknowledged, int flap_detection_enabled, int failure_prediction_enabled, int process_performance_data, int obsess_over_service, unsigned long last_state_change){
	service *temp_service;
	time_t current_time;
	int x;

#ifdef DEBUG0
	printf("set_service_state_information() start\n");
#endif

	/* find the service */
	temp_service=find_service(host_name,description,NULL);
	if(temp_service==NULL)
		return ERROR;

	/* update status information if necessary */
	if(temp_service->retain_status_information==TRUE){

		if(output==NULL)
			return ERROR;

		/* do bounds checking on the state */
		if(state<-1 || state>3)
			return ERROR;

		/* convert old STATUS_UNKNOWN value */
		if(state==-1)
			state=STATE_UNKNOWN;

		if(problem_has_been_acknowledged<0 || problem_has_been_acknowledged>1)
			return ERROR;

		/* initialize plugin output */
		strncpy(temp_service->plugin_output,output,MAX_PLUGINOUTPUT_LENGTH-1);
		temp_service->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';

		/* initialize performance data */
		strcpy(temp_service->perf_data,"");

		/* get the current time */
		time(&current_time);

		/* set state information */
		temp_service->check_type=(check_type>0)?SERVICE_CHECK_ACTIVE:SERVICE_CHECK_PASSIVE;
		temp_service->current_state=state;
		temp_service->last_state=state;
		temp_service->last_hard_state=state;
		temp_service->state_type=HARD_STATE;

		if(state==STATE_OK)
			temp_service->current_attempt=1;
		else
			temp_service->current_attempt=temp_service->max_attempts;

		if(state==STATE_WARNING)
			temp_service->has_been_warning=TRUE;
		else if(state==STATE_CRITICAL)
			temp_service->has_been_critical=TRUE;
		else if(state==STATE_UNKNOWN)
			temp_service->has_been_unknown=TRUE;

		temp_service->last_state_change=(time_t)last_state_change;
		temp_service->last_check=(time_t)last_check;

		temp_service->host_problem_at_last_check=FALSE;
		temp_service->no_recovery_notification=FALSE;

		temp_service->time_ok=time_ok;
		temp_service->time_warning=time_warning;
		temp_service->time_unknown=time_unknown;
		temp_service->time_critical=time_critical;

		if(temp_service->current_state==STATE_OK)
			temp_service->last_notification=(time_t)0;
		else
			temp_service->last_notification=(time_t)last_notification;
		if(temp_service->last_notification==(time_t)0){
			temp_service->next_notification=(time_t)0;
			temp_service->current_notification_number=0;
	                }
		else{
			temp_service->next_notification=(time_t)last_notification+(temp_service->notification_interval*interval_length);
			temp_service->current_notification_number=current_notification_number;
	                }

		if(temp_service->current_state==STATE_OK)
			temp_service->problem_has_been_acknowledged=FALSE;
		else
			temp_service->problem_has_been_acknowledged=(problem_has_been_acknowledged==0)?FALSE:TRUE;

		/* initialize state history for flap detection routines */
		for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
			temp_service->state_history[x]=temp_service->current_state;
	        }

	/* update non-status information if necessary */
	if(temp_service->retain_nonstatus_information==TRUE){

		if(notifications_enabled<0 || notifications_enabled>1)
			return ERROR;
		if(checks_enabled<0 || checks_enabled>1)
			return ERROR;
		if(accept_passive_checks<0 || accept_passive_checks>1)
			return ERROR;
		if(event_handler_enabled<0 || event_handler_enabled>1)
			return ERROR;
		if(flap_detection_enabled<0 || flap_detection_enabled>1)
			return ERROR;
		if(failure_prediction_enabled<0 || failure_prediction_enabled>1)
			return ERROR;
		if(process_performance_data<0 || process_performance_data>1)
			return ERROR;
		if(obsess_over_service<0 || obsess_over_service>1)
			return ERROR;

		temp_service->notifications_enabled=(notifications_enabled>0)?TRUE:FALSE;
		temp_service->checks_enabled=(checks_enabled>0)?TRUE:FALSE;
		temp_service->accept_passive_service_checks=(accept_passive_checks>0)?TRUE:FALSE;
		temp_service->event_handler_enabled=(event_handler_enabled>0)?TRUE:FALSE;
		temp_service->flap_detection_enabled=(flap_detection_enabled>0)?TRUE:FALSE;
		temp_service->failure_prediction_enabled=(failure_prediction_enabled>0)?TRUE:FALSE;
		temp_service->process_performance_data=(process_performance_data>0)?TRUE:FALSE;
		temp_service->obsess_over_service=(obsess_over_service>0)?TRUE:FALSE;
	        }

	/* update the status log with this service */
	update_service_status(temp_service,FALSE);

#ifdef DEBUG0
	printf("set_service_state_information() end\n");
#endif

	return OK;
        }




/* sets initial host state information */
int set_host_state_information(char *host_name, int state, char *output, unsigned long last_check, int checks_enabled, unsigned long time_up, unsigned long time_down, unsigned long time_unreachable, unsigned long last_notification, int current_notification_number, int notifications_enabled, int event_handler_enabled, int problem_has_been_acknowledged, int flap_detection_enabled, int failure_prediction_enabled, int process_performance_data, unsigned long last_state_change){
	host *temp_host;
	int x;

#ifdef DEBUG0
	printf("set_host_state_information() start\n");
#endif

	/* find the host */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* update status information if necessary */
	if(temp_host->retain_status_information==TRUE){

		if(output==NULL)
			return ERROR;

		/* initialize plugin output */
		strncpy(temp_host->plugin_output,output,MAX_PLUGINOUTPUT_LENGTH-1);
		temp_host->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';

		/* initialize perf data */
		strcpy(temp_host->perf_data,"");

		/* check state bounds */
		if(state<0 || state>2)
			return ERROR;

		if(problem_has_been_acknowledged<0 || problem_has_been_acknowledged>1)
			return ERROR;

		/* set state information */
		temp_host->status=state;

		if(state==HOST_UP)
			temp_host->current_attempt=1;
		else
			temp_host->current_attempt=temp_host->max_attempts;

		temp_host->time_up=time_up;
		temp_host->time_down=time_down;
		temp_host->time_unreachable=time_unreachable;

		temp_host->last_state_change=(time_t)last_state_change;
		temp_host->last_check=(time_t)last_check;

		if(temp_host->status==HOST_UP)
			temp_host->last_host_notification=(time_t)0;
		else
			temp_host->last_host_notification=(time_t)last_notification;
		if(temp_host->last_host_notification==(time_t)0){
			temp_host->next_host_notification=(time_t)0;
			temp_host->current_notification_number=0;
	                }
		else{
			temp_host->next_host_notification=(time_t)last_notification+(temp_host->notification_interval*interval_length);
			temp_host->current_notification_number=current_notification_number;
	                }

		if(temp_host->status==HOST_UP)
			temp_host->problem_has_been_acknowledged=FALSE;
		else
			temp_host->problem_has_been_acknowledged=(problem_has_been_acknowledged==0)?FALSE:TRUE;

		if(temp_host->status==HOST_DOWN)
			temp_host->has_been_down=TRUE;
		else if(temp_host->status==HOST_UNREACHABLE)
			temp_host->has_been_unreachable=TRUE;

		/* initialize state history for flap detection routines */
		for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
			temp_host->state_history[x]=temp_host->status;
	        }

	/* update non-status information if necessary */
	if(temp_host->retain_nonstatus_information==TRUE){

		if(notifications_enabled<0 || notifications_enabled>1)
			return ERROR;
		if(checks_enabled<0 || checks_enabled>1)
			return ERROR;
		if(event_handler_enabled<0 || event_handler_enabled>1)
			return ERROR;
		if(flap_detection_enabled<0 || flap_detection_enabled>1)
			return ERROR;
		if(failure_prediction_enabled<0 || failure_prediction_enabled>1)
			return ERROR;
		if(process_performance_data<0 || process_performance_data>1)
			return ERROR;

		temp_host->notifications_enabled=(notifications_enabled>0)?TRUE:FALSE;
		temp_host->checks_enabled=(checks_enabled>0)?TRUE:FALSE;
		temp_host->event_handler_enabled=(event_handler_enabled>0)?TRUE:FALSE;
		temp_host->flap_detection_enabled=(flap_detection_enabled>0)?TRUE:FALSE;
		temp_host->failure_prediction_enabled=(failure_prediction_enabled>0)?TRUE:FALSE;
		temp_host->process_performance_data=(process_performance_data>0)?TRUE:FALSE;
	        }

	/* update the status log with this host */
	update_host_status(temp_host,FALSE);

#ifdef DEBUG0
	printf("set_host_state_information() end\n");
#endif

	return OK;
        }




/* gets program information */
int get_program_state_information(int *notifications, int *service_checks, int *passive_service_checks, int *event_handlers, int *obsess_services, int *flap_detection, int *failure_prediction, int *performance_data){

#ifdef DEBUG0
	printf("set_program_state_information() start\n");
#endif

	*notifications=enable_notifications;
	*service_checks=execute_service_checks;
	*passive_service_checks=accept_passive_service_checks;
	*event_handlers=enable_event_handlers;
	*obsess_services=obsess_over_services;
	*flap_detection=enable_flap_detection;
	*failure_prediction=enable_failure_prediction;
	*performance_data=process_performance_data;

#ifdef DEBUG0
	printf("set_program_state_information() end\n");
#endif

	return OK;
        }



/* gets service state information */
service * get_service_state_information(service *svc, char **host_name, char **service_description, int *state, char **output, unsigned long *last_check, int *check_type, unsigned long *time_ok, unsigned long *time_warning, unsigned long *time_unknown, unsigned long *time_critical, unsigned long *last_notification, int *current_notification_number, int *notifications_enabled, int *checks_enabled, int *accept_passive_checks, int *event_handler_enabled, int *problem_has_been_acknowledged, int *flap_detection_enabled, int *failure_prediction_enabled, int *process_performance_data, int *obsess_over_service, unsigned long *last_state_change){
	service *temp_service;
	time_t current_time;
	unsigned long time_difference;
	unsigned long t_ok;
	unsigned long t_warning;
	unsigned long t_unknown;
	unsigned long t_critical;
	int service_state;
	char *plugin_output;

#ifdef DEBUG0
	printf("get_service_state_information() start\n");
#endif

	/* get the service to check */
	if(svc==NULL)
		temp_service=service_list;
	else
		temp_service=svc->next;

	if(temp_service==NULL)
		return NULL;

	/* skip services that haven't been checked yet */
	while(temp_service->last_check==(time_t)0){
		temp_service=temp_service->next;
		if(temp_service==NULL)
			return NULL;
	        }

	/* get the current time */
	time(&current_time);

	t_ok=temp_service->time_ok;
	t_warning=temp_service->time_warning;
	t_unknown=temp_service->time_unknown;
	t_critical=temp_service->time_critical;

	if(temp_service->state_type==SOFT_STATE){
		service_state=temp_service->last_hard_state;
		plugin_output="No data yet (service was in a soft problem state during state retention)";
	        }
	else{
		service_state=temp_service->current_state;
		plugin_output=temp_service->plugin_output;
	        }

	/* if this is NOT the first time we've had a service check/state change... */
	if(temp_service->has_been_checked==TRUE && temp_service->last_state_change!=(time_t)0){

		if(temp_service->last_state_change<program_start)
			time_difference=(unsigned long)(program_start>current_time)?0:(current_time-program_start);
		else
			time_difference=(unsigned long)(temp_service->last_state_change>current_time)?0:(current_time-temp_service->last_state_change);

		if(service_state==STATE_WARNING)
			t_warning+=time_difference;
		else if(service_state==STATE_UNKNOWN)
			t_unknown+=time_difference;
		else if(service_state==STATE_CRITICAL)
			t_critical+=time_difference;
		else
			t_ok+=time_difference;
	        }

	*host_name=temp_service->host_name;
	*service_description=temp_service->description;
	*state=service_state;
	*output=plugin_output;
	*last_check=(unsigned long)temp_service->last_check;
	*check_type=temp_service->check_type;
	*time_ok=t_ok;
	*time_warning=t_warning;
	*time_unknown=t_unknown;
	*time_critical=t_critical;
	*current_notification_number=temp_service->current_notification_number;
	*last_notification=(unsigned long)temp_service->last_notification;
	*notifications_enabled=temp_service->notifications_enabled;
	*accept_passive_checks=temp_service->accept_passive_service_checks;
	*checks_enabled=temp_service->checks_enabled;
	*event_handler_enabled=temp_service->event_handler_enabled;
	*problem_has_been_acknowledged=temp_service->problem_has_been_acknowledged;
	*flap_detection_enabled=temp_service->flap_detection_enabled;
	*failure_prediction_enabled=temp_service->failure_prediction_enabled;
	*process_performance_data=temp_service->process_performance_data;
	*obsess_over_service=temp_service->obsess_over_service;
	*last_state_change=(unsigned long)temp_service->last_state_change;

#ifdef DEBUG0
	printf("get_service_state_information() end\n");
#endif

	return temp_service;
        }




/* gets host state information */
host * get_host_state_information(host *hst, char **host_name, int *state, char **output, unsigned long *last_check, int *checks_enabled, unsigned long *time_up, unsigned long *time_down, unsigned long *time_unreachable, unsigned long *last_notification, int *current_notification_number, int *notifications_enabled, int *event_handler_enabled, int *problem_has_been_acknowledged, int *flap_detection_enabled, int *failure_prediction_enabled, int *process_performance_data, unsigned long *last_state_change){
	host *temp_host;
	time_t current_time;
	unsigned long time_difference;
	unsigned long t_up;
	unsigned long t_down;
	unsigned long t_unreachable;

#ifdef DEBUG0
	printf("get_host_state_information() start\n");
#endif

	/* get the host to check */
	if(hst==NULL)
		temp_host=host_list;
	else
		temp_host=hst->next;

	if(temp_host==NULL)
		return NULL;

	/* skip hosts that haven't been checked yet */
	while(temp_host->last_check==(time_t)0){
		temp_host=temp_host->next;
		if(temp_host==NULL)
			return NULL;
	        }

	/* get the current time */
	time(&current_time);

	t_up=temp_host->time_up;
	t_down=temp_host->time_down;
	t_unreachable=temp_host->time_unreachable;

	/* if this is NOT the first time we've had a host check/state change... */
	if(temp_host->has_been_checked==TRUE && temp_host->last_state_change!=0L){

		if(temp_host->last_state_change<program_start)
			time_difference=(unsigned long)(program_start>current_time)?0:(current_time-program_start);
		else
			time_difference=(unsigned long)(temp_host->last_state_change>current_time)?0:(current_time-temp_host->last_state_change);

		if(temp_host->status==HOST_DOWN)
			t_down+=time_difference;
		else if(temp_host->status==HOST_UNREACHABLE)
			t_unreachable+=time_difference;
		else
			t_up+=time_difference;
	        }

	*host_name=temp_host->name;
	*state=temp_host->status;
	*output=temp_host->plugin_output;
	*last_check=(unsigned long)temp_host->last_check;
	*time_up=t_up;
	*time_down=t_down;
	*time_unreachable=t_unreachable;
	*last_notification=(unsigned long)temp_host->last_host_notification;
	*current_notification_number=temp_host->current_notification_number;
	*notifications_enabled=temp_host->notifications_enabled;
	*problem_has_been_acknowledged=temp_host->problem_has_been_acknowledged;
	*checks_enabled=temp_host->checks_enabled;
	*event_handler_enabled=temp_host->event_handler_enabled;
	*flap_detection_enabled=temp_host->flap_detection_enabled;
	*failure_prediction_enabled=temp_host->failure_prediction_enabled;
	*process_performance_data=temp_host->process_performance_data;
	*last_state_change=(unsigned long)temp_host->last_state_change;

#ifdef DEBUG0
	printf("get_host_state_information() end\n");
#endif

	return temp_host;
        }
