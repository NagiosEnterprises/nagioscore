/*****************************************************************************
 *
 * XRDDEFAULT.C - Default external state retention routines for Nagios
 *
 * Copyright (c) 1999-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   04-18-2003
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
#include "../base/nagios.h"
#include "../base/sretention.h"


/**** STATE INFORMATION SPECIFIC HEADER FILES ****/

#include "xrddefault.h"


extern int            enable_notifications;
extern int            execute_service_checks;
extern int            accept_passive_service_checks;
extern int            execute_host_checks;
extern int            accept_passive_host_checks;
extern int            enable_event_handlers;
extern int            obsess_over_services;
extern int            obsess_over_hosts;
extern int            enable_flap_detection;
extern int            enable_failure_prediction;
extern int            process_performance_data;

extern int            use_retained_program_state;


char xrddefault_retention_file[MAX_FILENAME_LENGTH]="";




/******************************************************************/
/********************* CONFIG INITIALIZATION  *********************/
/******************************************************************/

int xrddefault_grab_config_info(char *main_config_file){
	char temp_buffer[MAX_INPUT_BUFFER];
	char *temp_ptr;
	FILE *fp;
							      

	/* initialize the location of the retention file */
	strncpy(xrddefault_retention_file,DEFAULT_RETENTION_FILE,sizeof(xrddefault_retention_file)-1);
	xrddefault_retention_file[sizeof(xrddefault_retention_file)-1]='\x0';

	/* open the main config file for reading */
	fp=fopen(main_config_file,"r");
	if(fp==NULL){
#ifdef DEBUG1
		printf("Error: Cannot open main configuration file '%s' for reading!\n",main_config_file);
#endif
		return ERROR;
	        }

	/* read in all lines from the main config file */
	while(fgets(temp_buffer,sizeof(temp_buffer)-1,fp)){

		if(feof(fp))
			break;

		/* skip blank lines and comments */
		if(temp_buffer[0]=='#' || temp_buffer[0]=='\x0' || temp_buffer[0]=='\n' || temp_buffer[0]=='\r')
			continue;

		strip(temp_buffer);

		temp_ptr=my_strtok(temp_buffer,"=");
		if(temp_ptr==NULL)
			continue;

		/* skip lines that don't specify the host config file location */
		if(strcmp(temp_ptr,"xrddefault_retention_file") && strcmp(temp_ptr,"state_retention_file"))
			continue;

		/* get the retention file name */
		temp_ptr=my_strtok(NULL,"\n");
		if(temp_ptr==NULL)
			continue;

		strncpy(xrddefault_retention_file,temp_ptr,sizeof(xrddefault_retention_file)-1);
		xrddefault_retention_file[sizeof(xrddefault_retention_file)-1]='\x0';
	        }

	fclose(fp);

	return OK;
        }


/******************************************************************/
/**************** DEFAULT STATE OUTPUT FUNCTION *******************/
/******************************************************************/

int xrddefault_save_state_information(char *main_config_file){
	char temp_buffer[MAX_INPUT_BUFFER];
	time_t current_time;
	int result=OK;
	FILE *fp;
	host *temp_host=NULL;
	service *temp_service=NULL;
	int x;

#ifdef DEBUG0
	printf("xrddefault_save_state_information() start\n");
#endif

	/* grab config info */
	if(xrddefault_grab_config_info(main_config_file)==ERROR){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Failed to grab configuration information for retention data!\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* open the retention file for writing */
	fp=fopen(xrddefault_retention_file,"w");
	if(fp==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not open state retention file '%s' for writing!\n",xrddefault_retention_file);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* write version info to status file */
	fprintf(fp,"########################################\n");
	fprintf(fp,"#      NAGIOS STATE RETENTION FILE\n");
	fprintf(fp,"#\n");
	fprintf(fp,"# THIS FILE IS AUTOMATICALLY GENERATED\n");
	fprintf(fp,"# BY NAGIOS.  DO NOT MODIFY THIS FILE!\n");
	fprintf(fp,"########################################\n\n");

	time(&current_time);

	/* write file info */
	fprintf(fp,"info {\n");
	fprintf(fp,"\tcreated=%lu\n",current_time);
	fprintf(fp,"\tversion=%s\n",PROGRAM_VERSION);
	fprintf(fp,"\t}\n\n");

	/* save program state information */
	fprintf(fp,"program {\n");
	fprintf(fp,"\tenable_notifications=%d\n",enable_notifications);
	fprintf(fp,"\tactive_service_checks_enabled=%d\n",execute_service_checks);
	fprintf(fp,"\tpassive_service_checks_enabled=%d\n",accept_passive_service_checks);
	fprintf(fp,"\tactive_host_checks_enabled=%d\n",execute_host_checks);
	fprintf(fp,"\tpassive_host_checks_enabled=%d\n",accept_passive_host_checks);
	fprintf(fp,"\tenable_event_handlers=%d\n",enable_event_handlers);
	fprintf(fp,"\tobsess_over_services=%d\n",obsess_over_services);
	fprintf(fp,"\tobsess_over_hosts=%d\n",obsess_over_hosts);
	fprintf(fp,"\tenable_flap_detection=%d\n",enable_flap_detection);
	fprintf(fp,"\tenable_failure_prediction=%d\n",enable_failure_prediction);
	fprintf(fp,"\tprocess_performance_data=%d\n",process_performance_data);
	fprintf(fp,"\t}\n\n");

	/* save host state information */
	move_first_host();
	while((temp_host=get_next_host())!=NULL){

		fprintf(fp,"host {\n");
		fprintf(fp,"\thost_name=%s\n",temp_host->name);
		fprintf(fp,"\thas_been_checked=%d\n",temp_host->has_been_checked);
		fprintf(fp,"\tcheck_execution_time=%.2f\n",temp_host->execution_time);
		fprintf(fp,"\tcheck_latency=%lu\n",temp_host->latency);
		fprintf(fp,"\tcurrent_state=%d\n",temp_host->current_state);
		fprintf(fp,"\tlast_state=%d\n",temp_host->last_state);
		fprintf(fp,"\tlast_hard_state=%d\n",temp_host->last_hard_state);
		fprintf(fp,"\tplugin_output=%s\n",(temp_host->plugin_output==NULL)?"":temp_host->plugin_output);
		fprintf(fp,"\tperformance_data=%s\n",(temp_host->perf_data==NULL)?"":temp_host->perf_data);
		fprintf(fp,"\tlast_check=%lu\n",temp_host->last_check);
		fprintf(fp,"\tcurrent_attempt=%d\n",temp_host->current_attempt);
		fprintf(fp,"\tstate_type=%d\n",temp_host->state_type);
		fprintf(fp,"\tlast_state_change=%lu\n",temp_host->last_state_change);
		fprintf(fp,"\tlast_hard_state_change=%lu\n",temp_host->last_hard_state_change);
		fprintf(fp,"\tnotified_on_down=%d\n",temp_host->notified_on_down);
		fprintf(fp,"\tnotified_on_unreachable=%d\n",temp_host->notified_on_unreachable);
		fprintf(fp,"\tlast_notification=%lu\n",temp_host->last_host_notification);
		fprintf(fp,"\tcurrent_notification_number=%d\n",temp_host->current_notification_number);
		fprintf(fp,"\tnotifications_enabled=%d\n",temp_host->notifications_enabled);
		fprintf(fp,"\tproblem_has_been_acknowledged=%d\n",temp_host->problem_has_been_acknowledged);
		fprintf(fp,"\tacknowledgement_type=%d\n",temp_host->acknowledgement_type);
		fprintf(fp,"\tactive_checks_enabled=%d\n",temp_host->checks_enabled);
		fprintf(fp,"\tpassive_checks_enabled=%d\n",temp_host->accept_passive_host_checks);
		fprintf(fp,"\tevent_handler_enabled=%d\n",temp_host->event_handler_enabled);
		fprintf(fp,"\tflap_detection_enabled=%d\n",temp_host->flap_detection_enabled);
		fprintf(fp,"\tfailure_prediction_enabled=%d\n",temp_host->failure_prediction_enabled);
		fprintf(fp,"\tprocess_performance_data=%d\n",temp_host->process_performance_data);
		fprintf(fp,"\tobsess_over_host=%d\n",temp_host->obsess_over_host);
		fprintf(fp,"\tis_flapping=%d\n",temp_host->is_flapping);
		fprintf(fp,"\tpercent_state_change=%.2f\n",temp_host->percent_state_change);

		fprintf(fp,"\tstate_history=");
		for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
			fprintf(fp,"%s%d",(x>0)?",":"",temp_host->state_history[(x+temp_host->state_history_index)%MAX_STATE_HISTORY_ENTRIES]);
		fprintf(fp,"\n");

		fprintf(fp,"\t}\n\n");
	        }

	/* save service state information */
	move_first_service();
	while((temp_service=get_next_service())!=NULL){

		fprintf(fp,"service {\n");
		fprintf(fp,"\thost_name=%s\n",temp_service->host_name);
		fprintf(fp,"\tservice_description=%s\n",temp_service->description);
		fprintf(fp,"\thas_been_checked=%d\n",temp_service->has_been_checked);
		fprintf(fp,"\tcheck_execution_time=%.2f\n",temp_service->execution_time);
		fprintf(fp,"\tcheck_latency=%lu\n",temp_service->latency);
		fprintf(fp,"\tcurrent_state=%d\n",temp_service->current_state);
		fprintf(fp,"\tlast_state=%d\n",temp_service->last_state);
		fprintf(fp,"\tlast_hard_state=%d\n",temp_service->last_hard_state);
		fprintf(fp,"\tcurrent_attempt=%d\n",temp_service->current_attempt);
		fprintf(fp,"\tstate_type=%d\n",temp_service->state_type);
		fprintf(fp,"\tlast_state_change=%lu\n",temp_service->last_state_change);
		fprintf(fp,"\tlast_hard_state_change=%lu\n",temp_service->last_hard_state_change);
		fprintf(fp,"\tplugin_output=%s\n",(temp_service->plugin_output==NULL)?"":temp_service->plugin_output);
		fprintf(fp,"\tperformance_data=%s\n",(temp_service->perf_data==NULL)?"":temp_service->perf_data);
		fprintf(fp,"\tlast_check=%lu\n",temp_service->last_check);
		fprintf(fp,"\tcheck_type=%d\n",temp_service->check_type);
		fprintf(fp,"\tnotified_on_unknown=%d\n",temp_service->notified_on_unknown);
		fprintf(fp,"\tnotified_on_warning=%d\n",temp_service->notified_on_warning);
		fprintf(fp,"\tnotified_on_critical=%d\n",temp_service->notified_on_critical);
		fprintf(fp,"\tcurrent_notification_number=%d\n",temp_service->current_notification_number);
		fprintf(fp,"\tlast_notification=%lu\n",temp_service->last_notification);
		fprintf(fp,"\tnotifications_enabled=%d\n",temp_service->notifications_enabled);
		fprintf(fp,"\tactive_checks_enabled=%d\n",temp_service->checks_enabled);
		fprintf(fp,"\tpassive_checks_enabled=%d\n",temp_service->accept_passive_service_checks);
		fprintf(fp,"\tevent_handler_enabled=%d\n",temp_service->event_handler_enabled);
		fprintf(fp,"\tproblem_has_been_acknowledged=%d\n",temp_service->problem_has_been_acknowledged);
		fprintf(fp,"\tacknowledgement_type=%d\n",temp_service->acknowledgement_type);
		fprintf(fp,"\tflap_detection_enabled=%d\n",temp_service->flap_detection_enabled);
		fprintf(fp,"\tfailure_prediction_enabled=%d\n",temp_service->failure_prediction_enabled);
		fprintf(fp,"\tprocess_performance_data=%d\n",temp_service->process_performance_data);
		fprintf(fp,"\tobsess_over_service=%d\n",temp_service->obsess_over_service);
		fprintf(fp,"\tis_flapping=%d\n",temp_service->is_flapping);
		fprintf(fp,"\tpercent_state_change=%.2f\n",temp_service->percent_state_change);

		fprintf(fp,"\tstate_history=");
		for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
			fprintf(fp,"%s%d",(x>0)?",":"",temp_service->state_history[(x+temp_service->state_history_index)%MAX_STATE_HISTORY_ENTRIES]);
		fprintf(fp,"\n");

		fprintf(fp,"\t}\n\n");
	        }

	fclose(fp);



#ifdef DEBUG0
	printf("xrddefault_save_state_information() end\n");
#endif

	return result;
        }




/******************************************************************/
/***************** DEFAULT STATE INPUT FUNCTION *******************/
/******************************************************************/

int xrddefault_read_state_information(char *main_config_file){
	char temp_buffer[MAX_INPUT_BUFFER];
	char *temp_ptr;
	FILE *fp;
	char *host_name=NULL;
	char *service_description=NULL;
	int data_type=XRDDEFAULT_NO_DATA;
	int x;
	host *temp_host=NULL;
	service *temp_service=NULL;
	char *var;
	char *val;

#ifdef DEBUG0
	printf("xrddefault_read_state_information() start\n");
#endif

	/* grab config info */
	if(xrddefault_grab_config_info(main_config_file)==ERROR){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Failed to grab configuration information for retention data!\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* open the retention file for reading */
	fp=fopen(xrddefault_retention_file,"r");
	if(fp==NULL)
		return ERROR;


	/* read all lines in the retention file */
	while(fgets(temp_buffer,sizeof(temp_buffer)-1,fp)){

		strip(temp_buffer);

		/* skip blank lines and comments */
		if(temp_buffer[0]=='#' || temp_buffer[0]=='\x0')
			continue;

		else if(!strcmp(temp_buffer,"info {"))
			data_type=XRDDEFAULT_INFO_DATA;
		else if(!strcmp(temp_buffer,"program {"))
			data_type=XRDDEFAULT_PROGRAM_DATA;
		else if(!strcmp(temp_buffer,"host {"))
			data_type=XRDDEFAULT_HOST_DATA;
		else if(!strcmp(temp_buffer,"service {"))
			data_type=XRDDEFAULT_SERVICE_DATA;

		else if(!strcmp(temp_buffer,"}")){

			switch(data_type){

			case XRDDEFAULT_INFO_DATA:
				break;

			case XRDDEFAULT_PROGRAM_DATA:
				break;

			case XRDDEFAULT_HOST_DATA:

				if(temp_host!=NULL){

					/* update host status */
					update_host_status(temp_host,FALSE);

					/* check for flapping */
					check_for_host_flapping(temp_host,FALSE);

					/* handle new vars added */
					if(temp_host->last_hard_state_change==(time_t)0)
						temp_host->last_hard_state_change=temp_host->last_state_change;
				        }

				free(host_name);
				host_name=NULL;
				temp_host=NULL;
				break;

			case XRDDEFAULT_SERVICE_DATA:

				if(temp_service!=NULL){

					/* update service status */
					update_service_status(temp_service,FALSE);

					/* check for flapping */
					check_for_service_flapping(temp_service,FALSE);
					
					/* handle new vars added */
					if(temp_service->last_hard_state_change==(time_t)0)
						temp_service->last_hard_state_change=temp_service->last_state_change;
				        }

				free(host_name);
				host_name=NULL;
				free(service_description);
				service_description=NULL;
				temp_service=NULL;
				break;

			default:
				break;
			        }

			data_type=XRDDEFAULT_NO_DATA;
		        }

		else if(data_type!=XRDDEFAULT_NO_DATA){

			var=strtok(temp_buffer,"=");
			val=strtok(NULL,"\n");
			if(val==NULL)
				continue;

			switch(data_type){

			case XRDDEFAULT_INFO_DATA:
				break;

			case XRDDEFAULT_PROGRAM_DATA:
				if(use_retained_program_state==FALSE)
					break;
				if(!strcmp(var,"enable_notifications"))
					enable_notifications=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"active_service_checks_enabled"))
					execute_service_checks=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"passive_service_checks_enabled"))
					accept_passive_service_checks=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"active_host_checks_enabled"))
					execute_host_checks=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"passive_host_checks_enabled"))
					accept_passive_host_checks=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"enable_event_handlers"))
					enable_event_handlers=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"obsess_over_services"))
					obsess_over_services=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"obsess_over_hosts"))
					obsess_over_hosts=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"enable_flap_detection"))
					enable_flap_detection=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"enable_failure_prediction"))
					enable_failure_prediction=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"process_performance_data"))
					process_performance_data=(atoi(val)>0)?TRUE:FALSE;
				break;

			case XRDDEFAULT_HOST_DATA:
				if(!strcmp(var,"host_name")){
					host_name=strdup(val);
					temp_host=find_host(host_name);
				        }
				else if(temp_host!=NULL){
					if(temp_host->retain_status_information==TRUE){
						if(!strcmp(var,"has_been_checked"))
							temp_host->has_been_checked=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"check_execution_time"))
							temp_host->execution_time=strtod(val,NULL);
						else if(!strcmp(var,"check_latency"))
							temp_host->latency=strtoul(val,NULL,10);
						else if(!strcmp(var,"current_state"))
							temp_host->current_state=atoi(val);
						else if(!strcmp(var,"last_state"))
							temp_host->last_state=atoi(val);
						else if(!strcmp(var,"last_hard_state"))
							temp_host->last_hard_state=atoi(val);
						else if(!strcmp(var,"plugin_output")){
							strncpy(temp_host->plugin_output,val,MAX_PLUGINOUTPUT_LENGTH-1);
							temp_host->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
					                }
						else if(!strcmp(var,"performance_data")){
							strncpy(temp_host->perf_data,val,MAX_PLUGINOUTPUT_LENGTH-1);
							temp_host->perf_data[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
					                }
						else if(!strcmp(var,"last_check"))
							temp_host->last_check=strtoul(val,NULL,10);
						else if(!strcmp(var,"current_attempt"))
							temp_host->current_attempt=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"state_type"))
							temp_host->state_type=atoi(val);
						else if(!strcmp(var,"last_state_change"))
							temp_host->last_state_change=strtoul(val,NULL,10);
						else if(!strcmp(var,"last_hard_state_change"))
							temp_host->last_hard_state_change=strtoul(val,NULL,10);
						else if(!strcmp(var,"notified_on_down"))
							temp_host->notified_on_down=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"notified_on_unreachable"))
							temp_host->notified_on_unreachable=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"last_notification"))
							temp_host->last_host_notification=strtoul(val,NULL,10);
						else if(!strcmp(var,"current_notification_number"))
							temp_host->current_notification_number=atoi(val);
						else if(!strcmp(var,"problem_has_been_acknowledged"))
							temp_host->problem_has_been_acknowledged=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"acknowledgement_type"))
							temp_host->acknowledgement_type=atoi(val);
						else if(!strcmp(var,"state_history")){
							temp_ptr=val;
							for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
								temp_host->state_history[x]=atoi(strsep(&temp_ptr,","));
							temp_host->state_history_index=0;
						        }
					        }
					if(temp_host->retain_nonstatus_information==TRUE){
						if(!strcmp(var,"notifications_enabled"))
							temp_host->notifications_enabled=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"active_checks_enabled"))
							temp_host->checks_enabled=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"passive_checks_enabled"))
							temp_host->accept_passive_host_checks=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"event_handler_enabled"))
							temp_host->event_handler_enabled=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"flap_detection_enabled"))
							temp_host->flap_detection_enabled=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"failure_prediction_enabled"))
							temp_host->failure_prediction_enabled=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"process_performance_data"))
							temp_host->process_performance_data=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"obsess_over_host"))
							temp_host->obsess_over_host=(atoi(val)>0)?TRUE:FALSE;
					        }
				        }
				break;

			case XRDDEFAULT_SERVICE_DATA:
				if(!strcmp(var,"host_name")){
					host_name=strdup(val);
					temp_service=find_service(host_name,service_description);
				        }
				else if(!strcmp(var,"service_description")){
					service_description=strdup(val);
					temp_service=find_service(host_name,service_description);
				        }
				else if(temp_service!=NULL){
					if(temp_service->retain_status_information==TRUE){
						if(!strcmp(var,"has_been_checked"))
							temp_service->has_been_checked=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"check_execution_time"))
							temp_service->execution_time=strtod(val,NULL);
						else if(!strcmp(var,"check_latency"))
							temp_service->latency=strtoul(val,NULL,10);
						else if(!strcmp(var,"current_state"))
							temp_service->current_state=atoi(val);
						else if(!strcmp(var,"last_state"))
							temp_service->last_state=atoi(val);
						else if(!strcmp(var,"last_hard_state"))
							temp_service->last_hard_state=atoi(val);
						else if(!strcmp(var,"current_attempt"))
							temp_service->current_attempt=atoi(val);
						else if(!strcmp(var,"state_type"))
							temp_service->state_type=atoi(val);
						else if(!strcmp(var,"last_state_change"))
							temp_service->last_state_change=strtoul(val,NULL,10);
						else if(!strcmp(var,"last_hard_state_change"))
							temp_service->last_hard_state_change=strtoul(val,NULL,10);
						else if(!strcmp(var,"plugin_output")){
							strncpy(temp_service->plugin_output,val,MAX_PLUGINOUTPUT_LENGTH-1);
							temp_service->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
					                }
						else if(!strcmp(var,"performance_data")){
							strncpy(temp_service->perf_data,val,MAX_PLUGINOUTPUT_LENGTH-1);
							temp_service->perf_data[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
					                }
						else if(!strcmp(var,"last_check"))
							temp_service->last_check=strtoul(val,NULL,10);
						else if(!strcmp(var,"check_type"))
							temp_service->check_type=atoi(val);
						else if(!strcmp(var,"notified_on_unknown"))
							temp_service->notified_on_unknown=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"notified_on_warning"))
							temp_service->notified_on_warning=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"notified_on_critical"))
							temp_service->notified_on_critical=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"current_notification_number"))
							temp_service->current_notification_number=atoi(val);
						else if(!strcmp(var,"last_notification"))
							temp_service->last_notification=strtoul(val,NULL,10);

						else if(!strcmp(var,"state_history")){
							temp_ptr=val;
							for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
								temp_service->state_history[x]=atoi(strsep(&temp_ptr,","));
							temp_service->state_history_index=0;
						        }
					        }
					if(temp_service->retain_nonstatus_information==TRUE){
						if(!strcmp(var,"notifications_enabled"))
							temp_service->notifications_enabled=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"active_checks_enabled"))
							temp_service->checks_enabled=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"passive_checks_enabled"))
							temp_service->accept_passive_service_checks=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"event_handler_enabled"))
							temp_service->event_handler_enabled=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"problem_has_been_acknowledged"))
							temp_service->problem_has_been_acknowledged=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"acknowledgement_type"))
							temp_service->acknowledgement_type=atoi(val);
						else if(!strcmp(var,"flap_detection_enabled"))
							temp_service->flap_detection_enabled=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"failure_prediction_enabled"))
							temp_service->failure_prediction_enabled=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"process_performance_data"))
							temp_service->process_performance_data=(atoi(val)>0)?TRUE:FALSE;
						else if(!strcmp(var,"obsess_over_service"))
							temp_service->obsess_over_service=(atoi(val)>0)?TRUE:FALSE;
					        }
				        }
				break;

			default:
				break;
			        }

		        }
	        }


	fclose(fp);


#ifdef DEBUG0
	printf("xrddefault_read_state_information() end\n");
#endif

	return OK;
        }
