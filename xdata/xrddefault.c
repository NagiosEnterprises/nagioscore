/*****************************************************************************
 *
 * XRDDEFAULT.C - Default external state retention routines for Nagios
 *
 * Copyright (c) 1999-2004 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   02-02-2004
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
#include "../base/nagios.h"
#include "../base/sretention.h"


/**** STATE INFORMATION SPECIFIC HEADER FILES ****/

#include "xrddefault.h"

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
	char *temp_ptr;
	time_t current_time;
	int result=OK;
	FILE *fp;
	host *temp_host=NULL;
	service *temp_service=NULL;
	char *host_name;
	char *service_description;
	char *plugin_output;
	int state;
	unsigned long time_ok;
	unsigned long time_warning;
	unsigned long time_unknown;
	unsigned long time_critical;
	unsigned long time_up;
	unsigned long time_down;
	unsigned long time_unreachable;
	unsigned long last_notification;
	unsigned long last_check;
	int check_type;
	int notifications_enabled;
	int checks_enabled;
	int problem_has_been_acknowledged;
	int current_notification_number;
	int accept_passive_checks;
	int event_handler_enabled;
	int enable_notifications;
	int execute_service_checks;
	int accept_passive_service_checks;
	int enable_event_handlers;
	int obsess_over_services;
	int enable_flap_detection;
	int flap_detection_enabled;
	int enable_failure_prediction;
	int failure_prediction_enabled;
	int process_performance_data;
	int obsess_over_service;
	unsigned long last_state_change;


#ifdef DEBUG0
	printf("xrddefault_save_state_information() start\n");
#endif

	/* grab config info */
	if(xrddefault_grab_config_info(main_config_file)==ERROR){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Failed to grab configuration information for retention data\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* open the retention file for writing */
	fp=fopen(xrddefault_retention_file,"w");
	if(fp==NULL){
#ifdef DEBUG1
		printf("Error: Cannot open state retention file '%s' for writing!\n",xrddefault_retention_file);
#endif
		return ERROR;
	        }

	/* write version info to status file */
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"# Nagios %s Retention File\n",PROGRAM_VERSION);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	fputs(temp_buffer,fp);

	time(&current_time);
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"CREATED: %lu\n",current_time);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	fputs(temp_buffer,fp);

	/* save program state information */
	get_program_state_information(&enable_notifications,&execute_service_checks,&accept_passive_service_checks,&enable_event_handlers,&obsess_over_services,&enable_flap_detection,&enable_failure_prediction,&process_performance_data);
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"PROGRAM: %d;%d;%d;%d;%d;%d;%d;%d\n",enable_notifications,accept_passive_service_checks,execute_service_checks,enable_event_handlers,obsess_over_services,enable_flap_detection,enable_failure_prediction,process_performance_data);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	fputs(temp_buffer,fp);

	/* save host state information */
	temp_host=NULL;
	while((temp_host=get_host_state_information(temp_host,&host_name,&state,&plugin_output,&last_check,&checks_enabled,&time_up,&time_down,&time_unreachable,&last_notification,&current_notification_number,&notifications_enabled,&event_handler_enabled,&problem_has_been_acknowledged,&flap_detection_enabled,&failure_prediction_enabled,&process_performance_data,&last_state_change))!=NULL){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"HOST: %s;%d;%lu;%d;%lu;%lu;%lu;%lu;%d;%d;%d;%d;%d;%d;%d;%lu;%s\n",host_name,state,last_check,checks_enabled,time_up,time_down,time_unreachable,last_notification,current_notification_number,notifications_enabled,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,last_state_change,plugin_output);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';

		fputs(temp_buffer,fp);
	        }

	/* save service state information */
	temp_service=NULL;
	while((temp_service=get_service_state_information(temp_service,&host_name,&service_description,&state,&plugin_output,&last_check,&check_type,&time_ok,&time_warning,&time_unknown,&time_critical,&last_notification,&current_notification_number,&notifications_enabled,&checks_enabled,&accept_passive_checks,&event_handler_enabled,&problem_has_been_acknowledged,&flap_detection_enabled,&failure_prediction_enabled,&process_performance_data,&obsess_over_service,&last_state_change))!=NULL){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"SERVICE: %s;%s;%d;%lu;%d;%lu;%lu;%lu;%lu;%lu;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%lu;%s\n",host_name,service_description,state,last_check,check_type,time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification_number,notifications_enabled,checks_enabled,accept_passive_checks,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,obsess_over_service,last_state_change,plugin_output);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';

		fputs(temp_buffer,fp);
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
	time_t current_time;
	time_t time_created;
	FILE *fp;
	char *host_name;
	char *service_description;
	char *plugin_output;
	int state;
	unsigned long time_ok;
	unsigned long time_warning;
	unsigned long time_unknown;
	unsigned long time_critical;
	unsigned long time_up;
	unsigned long time_down;
	unsigned long time_unreachable;
	unsigned long last_notification;
	unsigned long last_check;
	int check_type;
	int notifications_enabled;
	int checks_enabled;
	int problem_has_been_acknowledged;
	int current_notification_number;
	int accept_passive_checks;
	int event_handler_enabled;
	int enable_notifications;
	int execute_service_checks;
	int accept_passive_service_checks;
	int enable_event_handlers;
	int obsess_over_services;
	int enable_flap_detection;
	int flap_detection_enabled;
	int enable_failure_prediction;
	int failure_prediction_enabled;
	int process_performance_data;
	int obsess_over_service;
	unsigned long last_state_change;
	int result=OK;

#ifdef DEBUG0
	printf("xrddefault_read_state_information() start\n");
#endif

	/* grab config info */
	if(xrddefault_grab_config_info(main_config_file)==ERROR){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Failed to grab configuration information for retention data\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* open the retention file for reading */
	fp=fopen(xrddefault_retention_file,"r");
	if(fp==NULL){
#ifdef DEBUG1
		printf("Error: Cannot open state retention file '%s' for reading!\n",xrddefault_retention_file);
#endif
		return ERROR;
	        }

	time(&current_time);
	time_created=current_time;


	/* read all lines in the retention file */
	while(fgets(temp_buffer,sizeof(temp_buffer)-1,fp)){

		if(feof(fp))
			break;

		/* skip blank lines */
		if(temp_buffer[0]=='\x0' || temp_buffer[0]=='\n' || temp_buffer[0]=='\r')
			continue;

		strip(temp_buffer);

		/* this is the creation timestamp */
		if(strstr(temp_buffer,"CREATED:")==temp_buffer){

			temp_ptr=strtok(temp_buffer,":");

			temp_ptr=strtok(NULL,"\n");
			time_created=(time_t)strtoul(temp_ptr,NULL,10);
		        }

		/* this is the program entry */
		else if(strstr(temp_buffer,"PROGRAM:")==temp_buffer){

			temp_ptr=strtok(temp_buffer,":");
			if(temp_ptr==NULL)
				continue;

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			enable_notifications=atoi(temp_ptr+1);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			execute_service_checks=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			accept_passive_service_checks=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			enable_event_handlers=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			obsess_over_services=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			enable_flap_detection=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			enable_failure_prediction=atoi(temp_ptr);

			temp_ptr=strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;
			process_performance_data=atoi(temp_ptr);

			/* set the program state information */
			set_program_state_information(enable_notifications,execute_service_checks,accept_passive_service_checks,enable_event_handlers,obsess_over_services,enable_flap_detection,enable_failure_prediction,process_performance_data);
		        }

		/* this is a host entry */
		else if(strstr(temp_buffer,"HOST:")==temp_buffer){

			temp_ptr=strtok(temp_buffer,":");

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			host_name=temp_ptr+1;

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			state=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			last_check=strtoul(temp_ptr,NULL,10);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			checks_enabled=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			time_up=strtoul(temp_ptr,NULL,10);
			if(state==HOST_UP)
				time_up+=(current_time-time_created);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			time_down=strtoul(temp_ptr,NULL,10);
			if(state==HOST_DOWN)
				time_down+=(current_time-time_created);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			time_unreachable=strtoul(temp_ptr,NULL,10);
			if(state==HOST_UNREACHABLE)
				time_unreachable+=(current_time-time_created);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			last_notification=strtoul(temp_ptr,NULL,10);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			current_notification_number=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			notifications_enabled=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			event_handler_enabled=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			problem_has_been_acknowledged=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			flap_detection_enabled=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			failure_prediction_enabled=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			process_performance_data=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			last_state_change=strtoul(temp_ptr,NULL,10);

			temp_ptr=strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;
			plugin_output=temp_ptr;

			/* set the host state */
			result=set_host_state_information(host_name,state,plugin_output,last_check,checks_enabled,time_up,time_down,time_unreachable,last_notification,current_notification_number,notifications_enabled,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,last_state_change);
		        }

		/* this is a service entry */
		else if(strstr(temp_buffer,"SERVICE:")==temp_buffer){

			temp_ptr=strtok(temp_buffer,":");

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			host_name=temp_ptr+1;

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			service_description=temp_ptr;

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			state=atoi(temp_ptr);
			
			/* convert old STATE_UNKNOWN type */
			if(state==-1)
				state=STATE_UNKNOWN;

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			last_check=strtoul(temp_ptr,NULL,10);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			check_type=atoi(temp_ptr);
			
			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			time_ok=strtoul(temp_ptr,NULL,10);
			if(state==STATE_OK)
				time_ok+=(current_time-time_created);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			time_warning=strtoul(temp_ptr,NULL,10);
			if(state==STATE_WARNING)
				time_warning+=(current_time-time_created);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			time_unknown=strtoul(temp_ptr,NULL,10);
			if(state==STATE_UNKNOWN)
				time_unknown+=(current_time-time_created);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			time_critical=strtoul(temp_ptr,NULL,10);
			if(state==STATE_CRITICAL)
				time_critical+=(current_time-time_created);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
		        last_notification=strtoul(temp_ptr,NULL,10);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			current_notification_number=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			notifications_enabled=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			checks_enabled=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			accept_passive_checks=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			event_handler_enabled=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			problem_has_been_acknowledged=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			flap_detection_enabled=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			failure_prediction_enabled=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			process_performance_data=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			obsess_over_service=atoi(temp_ptr);

			temp_ptr=strtok(NULL,";");
			if(temp_ptr==NULL)
				continue;
			last_state_change=strtoul(temp_ptr,NULL,10);

			temp_ptr=strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;
			plugin_output=temp_ptr;

			/* set the service state */
			result=set_service_state_information(host_name,service_description,state,plugin_output,last_check,check_type,time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification_number,notifications_enabled,checks_enabled,accept_passive_checks,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,obsess_over_service,last_state_change);
		        }
	        }


	fclose(fp);


#ifdef DEBUG0
	printf("xrddefault_read_state_information() end\n");
#endif

	return result;
        }
