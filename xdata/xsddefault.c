/*****************************************************************************
 *
 * XSDDEFAULT.C - Default external status data input routines for Nagios
 *
 * Copyright (c) 2000-2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-13-2001
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
#include "../common/locations.h"
#include "../common/statusdata.h"

#ifdef NSCORE
#include "../base/nagios.h"
#endif

#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/
#include "xsddefault.h"


char xsddefault_status_log[MAX_FILENAME_LENGTH]="";
char xsddefault_temp_file[MAX_FILENAME_LENGTH]="";

#ifdef NSCORE
FILE *xsddefault_aggregate_fp=NULL;
int xsddefault_aggregate_fd=0;
char xsddefault_aggregate_temp_file[MAX_INPUT_BUFFER];
#endif



/******************************************************************/
/***************** COMMON CONFIG INITIALIZATION  ******************/
/******************************************************************/

/* grab configuration information */
int xsddefault_grab_config_info(char *config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	FILE *fp;
#ifdef NSCGI
	FILE *fp2;
	char *temp_buffer;
#endif


	/*** CORE PASSES IN MAIN CONFIG FILE, CGIS PASS IN CGI CONFIG FILE! ***/

	/* initialize the location of the status log */
	strncpy(xsddefault_status_log,DEFAULT_STATUS_FILE,sizeof(xsddefault_status_log)-1);
	strncpy(xsddefault_temp_file,DEFAULT_TEMP_FILE,sizeof(xsddefault_temp_file)-1);
	xsddefault_status_log[sizeof(xsddefault_status_log)-1]='\x0';
	xsddefault_temp_file[sizeof(xsddefault_temp_file)-1]='\x0';

	/* open the config file for reading */
	fp=fopen(config_file,"r");
	if(fp==NULL)
		return ERROR;

	/* read in all lines from the main config file */
	for(fgets(input_buffer,sizeof(input_buffer)-1,fp);!feof(fp);fgets(input_buffer,sizeof(input_buffer)-1,fp)){

		/* skip blank lines and comments */
		if(input_buffer[0]=='#' || input_buffer[0]=='\x0' || input_buffer[0]=='\n' || input_buffer[0]=='\r')
			continue;

		strip(input_buffer);

#ifdef NSCGI
		/* CGI needs to find and read contents of main config file, since it was passed the name of the CGI config file */
		if(strstr(input_buffer,"main_config_file")==input_buffer){

			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			
			fp2=fopen(temp_buffer,"r");
			if(fp2==NULL)
				continue;

			/* read in all lines from the main config file */
			for(fgets(input_buffer,sizeof(input_buffer)-1,fp2);!feof(fp2);fgets(input_buffer,sizeof(input_buffer)-1,fp2)){

				/* skip blank lines and comments */
				if(input_buffer[0]=='#' || input_buffer[0]=='\x0' || input_buffer[0]=='\n' || input_buffer[0]=='\r')
					continue;

				strip(input_buffer);

				xsddefault_grab_config_directives(input_buffer);
			        }

			fclose(fp2);
		        }
#endif

#ifdef NSCORE
		/* core reads variables directly from the main config file */
		xsddefault_grab_config_directives(input_buffer);
#endif
	        }

	fclose(fp);

	/* we didn't find the status log name */
	if(!strcmp(xsddefault_status_log,""))
		return ERROR;

	/* we didn't find the temp file */
	if(!strcmp(xsddefault_temp_file,""))
		return ERROR;

	return OK;
        }


void xsddefault_grab_config_directives(char *input_buffer){
	char *temp_buffer;

	/* status log definition */
	if((strstr(input_buffer,"status_file")==input_buffer) || (strstr(input_buffer,"xsddefault_status_log")==input_buffer)){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xsddefault_status_log,temp_buffer,sizeof(xsddefault_status_log)-1);
		xsddefault_status_log[sizeof(xsddefault_status_log)-1]='\x0';
	        }


	/* temp file definition */
	if((strstr(input_buffer,"temp_file")==input_buffer) || (strstr(input_buffer,"xsddefault_temp_file")==input_buffer)){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xsddefault_temp_file,temp_buffer,sizeof(xsddefault_temp_file)-1);
		xsddefault_temp_file[sizeof(xsddefault_temp_file)-1]='\x0';
	        }

	return;
        }



#ifdef NSCORE

/******************************************************************/
/***************** DEFAULT DATA OUTPUT FUNCTIONS ******************/
/******************************************************************/


/* initialize status data */
int xsddefault_initialize_status_data(char *config_file){
	int result;

	/* grab configuration data */
	result=xsddefault_grab_config_info(config_file);
	if(result==ERROR)
		return ERROR;

	/* delete the old status log (it might not exist) */
	unlink(xsddefault_status_log);

	return OK;
        }


/* cleanup status data before terminating */
int xsddefault_cleanup_status_data(char *config_file, int delete_status_data){

	/* delete the status log */
	if(delete_status_data==TRUE){
		if(unlink(xsddefault_status_log))
			return ERROR;
	        }

	return OK;
        }


/* start aggregated dump */
int xsddefault_begin_aggregated_dump(void){
	char buffer[MAX_INPUT_BUFFER];

	/* open a safe temp file for output */
	snprintf(xsddefault_aggregate_temp_file,sizeof(xsddefault_aggregate_temp_file)-1,"%sXXXXXX",xsddefault_temp_file);
	xsddefault_aggregate_temp_file[sizeof(xsddefault_aggregate_temp_file)-1]='\x0';
	if((xsddefault_aggregate_fd=mkstemp(xsddefault_aggregate_temp_file))==-1)
		return ERROR;
	xsddefault_aggregate_fp=fdopen(xsddefault_aggregate_fd,"w");
	if(xsddefault_aggregate_fp==NULL){
		close(xsddefault_aggregate_fd);
		unlink(xsddefault_aggregate_temp_file);
		return ERROR;
	        }

	/* write version info to status file */
	snprintf(buffer,sizeof(buffer)-1,"# Nagios %s Status File\n",PROGRAM_VERSION);
	buffer[sizeof(buffer)-1]='\x0';
	fputs(buffer,xsddefault_aggregate_fp);

	return OK;
        }


/* finish aggregated dump */
int xsddefault_end_aggregated_dump(void){

	/* reset file permissions */
	fchmod(xsddefault_aggregate_fd,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* close the temp file */
	fclose(xsddefault_aggregate_fp);

	/* move the temp file to the status log (overwrite the old status log) */
	if(my_rename(xsddefault_aggregate_temp_file,xsddefault_status_log))
		return ERROR;

	return OK;
        }


/* updates program status data */
int xsddefault_update_program_status(time_t _program_start, int _nagios_pid, int _daemon_mode, time_t _last_command_check, time_t _last_log_rotation, int _enable_notifications, int _execute_service_checks, int _accept_passive_service_checks, int _enable_event_handlers, int _obsess_over_services, int _enable_flap_detection, int _enable_failure_prediction, int _process_performance_data, int aggregated_dump){
	FILE *fpin,*fpout;
	char input_buffer[MAX_INPUT_BUFFER];
	char program_string[MAX_INPUT_BUFFER];
	char new_program_string[MAX_INPUT_BUFFER];
	time_t current_time;
	int found_entry=FALSE;
	char temp_file[MAX_INPUT_BUFFER];
	int tempfd;

	time(&current_time);

	/* create a string to identify previous entries in the log file for the program info */
	snprintf(program_string,sizeof(program_string),"] PROGRAM;");
	program_string[sizeof(program_string)-1]='\x0';

	snprintf(new_program_string,sizeof(program_string)-1,"[%lu] PROGRAM;%lu;%d;%d;%lu;%lu;%d;%d;%d;%d;%d;%d;%d;%d\n",(unsigned long)current_time,(unsigned long)_program_start,_nagios_pid,_daemon_mode,(unsigned long)_last_command_check,(unsigned long)_last_log_rotation,_enable_notifications,_execute_service_checks,_accept_passive_service_checks,_enable_event_handlers,_obsess_over_services,_enable_flap_detection,_enable_failure_prediction,_process_performance_data);
	new_program_string[sizeof(new_program_string)-1]='\x0';

	/* we're doing an aggregated dump */
	if(aggregated_dump==TRUE){
		fputs(new_program_string,xsddefault_aggregate_fp);
		return OK;
	        }

	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xsddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((tempfd=mkstemp(temp_file))==-1)
		return ERROR;
	fpout=fdopen(tempfd,"w");
	if(fpout==NULL){
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* open the existing status log */
	fpin=fopen(xsddefault_status_log,"r");
	if(fpin==NULL){
		fclose(fpout);
		unlink(temp_file);
		return ERROR;
	        }

	/* read all lines from the original status log */
	while(fgets(input_buffer,MAX_INPUT_BUFFER-1,fpin)){

		/* if we haven't found the program entry yet, search for a match... */
		if(found_entry==FALSE){

			/* if this is the old program string, write the new program info instead */
			if(strstr(input_buffer,program_string)){
				fprintf(fpout,"%s",new_program_string);
				found_entry=TRUE;
			        }

			/* if this is not a duplicate entry, write it to the new log file */
			else
				fputs(input_buffer,fpout);
		        }

		/* we already found the program entry, so just write this line to the new log file */
		else
			fputs(input_buffer,fpout);
	        }

	/* we didn't find the program entry, so write one */
	if(found_entry==FALSE)
		fprintf(fpout,"%s",new_program_string);

	/* reset file permissions */
	fchmod(tempfd,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* close files */
	fclose(fpout);
	fclose(fpin);

	/* move the temp file to the status log (overwrite the old status log) */
	if(my_rename(temp_file,xsddefault_status_log))
		return ERROR;

	return OK;
        }


/* updates host status data */
int xsddefault_update_host_status(char *host_name, char *status, time_t last_update, time_t last_check, time_t last_state_change, int problem_has_been_acknowledged, unsigned long time_up, unsigned long time_down, unsigned long time_unreachable, time_t last_notification, int current_notification_number, int notifications_enabled, int event_handler_enabled, int checks_enabled, int flap_detection_enabled, int is_flapping, double percent_state_change, int scheduled_downtime_depth, int failure_prediction_enabled, int process_performance_data, char *plugin_output, int aggregated_dump){
	FILE *fpin,*fpout;
	char input_buffer[MAX_INPUT_BUFFER];
	char host_string[MAX_INPUT_BUFFER];
	char new_host_string[MAX_INPUT_BUFFER];
	int found_entry=FALSE;
	char temp_file[MAX_INPUT_BUFFER];
	int tempfd;


	/* create a host string to identify previous entries in the log file for this host */
	snprintf(host_string,sizeof(host_string),"] HOST;%s;",host_name);
	host_string[sizeof(host_string)-1]='\x0';

	if(last_check==(time_t)0)
		snprintf(new_host_string,sizeof(new_host_string)-1,"[%lu] HOST;%s;PENDING;0;0;0;0;0;0;0;0;%d;%d;%d;%d;0;0.0;0;%d;%d;(Not enough data to determine host status yet)\n",(unsigned long)last_update,host_name,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,failure_prediction_enabled,process_performance_data);
	else
		snprintf(new_host_string,sizeof(new_host_string)-1,"[%lu] HOST;%s;%s;%lu;%lu;%d;%lu;%lu;%lu;%lu;%d;%d;%d;%d;%d;%d;%3.2f;%d;%d;%d;%s\n",(unsigned long)last_update,host_name,status,(unsigned long)last_check,(unsigned long)last_state_change,problem_has_been_acknowledged,time_up,time_down,time_unreachable,(unsigned long)last_notification,current_notification_number,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,plugin_output);
	new_host_string[sizeof(new_host_string)-1]='\x0';

	/* we're doing an aggregated dump of the status data */
	if(aggregated_dump==TRUE){
		fputs(new_host_string,xsddefault_aggregate_fp);
		return OK;
	        }

	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xsddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((tempfd=mkstemp(temp_file))==-1)
		return ERROR;
	fpout=fdopen(tempfd,"w");
	if(fpout==NULL){
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* open the existing status log */
	fpin=fopen(xsddefault_status_log,"r");
	if(fpin==NULL){
		fclose(fpout);
		unlink(temp_file);
		return ERROR;
	        }

	/* read all lines from the original status log */
	while(fgets(input_buffer,MAX_INPUT_BUFFER-1,fpin)){

		/* if we haven't found the host entry yet, scan for it */
		if(found_entry==FALSE){

			/* if this is the old host string, write the new host info instead */
			if(strstr(input_buffer,host_string)){
				fprintf(fpout,"%s",new_host_string);
				found_entry=TRUE;
			        }

			/* this was not a match, so just write it to the new log file */
			else
				fputs(input_buffer,fpout);
		        }

		/* a matching host entry was already found, so just write this to the new log file */
		else
			fputs(input_buffer,fpout);
	        }

	/* we didn't find the host entry, so write one */
	if(found_entry==FALSE)
		fprintf(fpout,"%s",new_host_string);

	/* reset file permissions */
	fchmod(tempfd,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* close files */
	fclose(fpout);
	fclose(fpin);

	/* move the temp file to the status log (overwrite the old status log) */
	if(my_rename(temp_file,xsddefault_status_log))
		return ERROR;

	return OK;
        }


/* updates service status data */
int xsddefault_update_service_status(char *host_name, char *description, char *status, time_t last_update, int current_attempt, int max_attempts, int state_type, time_t last_check, time_t next_check, int should_be_scheduled, int check_type, int checks_enabled, int accept_passive_service_checks, int event_handler_enabled, time_t last_state_change, int problem_has_been_acknowledged, char *last_hard_state, unsigned long time_ok, unsigned long time_warning, unsigned long time_unknown, unsigned long time_critical, time_t last_notification, int current_notification_number, int notifications_enabled, int latency, int execution_time, int flap_detection_enabled, int is_flapping, double percent_state_change, int scheduled_downtime_depth, int failure_prediction_enabled, int process_performance_data, int obsess_over_service, char *plugin_output, int aggregated_dump){
	FILE *fpin,*fpout;
	char input_buffer[MAX_INPUT_BUFFER];
	char service_string[MAX_INPUT_BUFFER];
	char new_service_string[MAX_INPUT_BUFFER];
	int found_entry=FALSE;
	char temp_file[MAX_INPUT_BUFFER];
	int tempfd;


	/* create a service string to identify previous entries in the log for this service/host pair */
	snprintf(service_string,sizeof(service_string),"] SERVICE;%s;%s;",host_name,description);
	service_string[sizeof(service_string)-1]='\x0';

	/* create the new service status string */
	if(last_check==(time_t)0)
		snprintf(new_service_string,sizeof(new_service_string)-1,"[%lu] SERVICE;%s;%s;PENDING;0/%d;HARD;0;%lu;ACTIVE;%d;%d;%d;0;0;OK;0;0;0;0;0;0;%d;%d;%d;%d;0;0.0;0;%d;%d;%d;%s%s",(unsigned long)last_update,host_name,description,max_attempts,(should_be_scheduled==TRUE)?(unsigned long)next_check:0L,checks_enabled,accept_passive_service_checks,event_handler_enabled,notifications_enabled,latency,execution_time,flap_detection_enabled,failure_prediction_enabled,process_performance_data,obsess_over_service,(should_be_scheduled==TRUE)?"Service check scheduled for ":"Service check is not scheduled for execution...\n",(should_be_scheduled==TRUE)?ctime(&next_check):"");

	else
		snprintf(new_service_string,sizeof(new_service_string)-1,"[%lu] SERVICE;%s;%s;%s;%d/%d;%s;%lu;%lu;%s;%d;%d;%d;%lu;%d;%s;%lu;%lu;%lu;%lu;%lu;%d;%d;%d;%d;%d;%d;%3.2f;%d;%d;%d;%d;%s\n",(unsigned long)last_update,host_name,description,status,(state_type==SOFT_STATE)?current_attempt-1:current_attempt,max_attempts,(state_type==SOFT_STATE)?"SOFT":"HARD",(unsigned long)last_check,(should_be_scheduled==TRUE)?(unsigned long)next_check:0L,(check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE",checks_enabled,accept_passive_service_checks,event_handler_enabled,(unsigned long)last_state_change,problem_has_been_acknowledged,last_hard_state,time_ok,time_unknown,time_warning,time_critical,(unsigned long)last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,plugin_output);

	new_service_string[sizeof(new_service_string)-1]='\x0';

	/* we're doing an aggregated dump of the status data */
	if(aggregated_dump==TRUE){
		fputs(new_service_string,xsddefault_aggregate_fp);
		return OK;
	        }

	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xsddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((tempfd=mkstemp(temp_file))==-1)
		return ERROR;
	fpout=fdopen(tempfd,"w");
	if(fpout==NULL){
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* open the existing status log */
	fpin=fopen(xsddefault_status_log,"r");
	if(fpin==NULL){
		fclose(fpout);
		unlink(temp_file);
		return ERROR;
	        }

	/* read all lines from the original status log */
	while(fgets(input_buffer,MAX_INPUT_BUFFER-1,fpin)){

		/* if we haven't found the entry yet, search for it */
		if(found_entry==FALSE){

			/* if this is the old service string, write the new service info instead */
			if(strstr(input_buffer,service_string)){
				fprintf(fpout,"%s",new_service_string);
				found_entry=TRUE;
			        }

			/* this is not a matching entry, so just write it to the new log file */
			else
				fputs(input_buffer,fpout);
		        }

		/* the service entry has already been found, so just write this to the new log file */
		else
			fputs(input_buffer,fpout);
	        }

	/* we didn't find the service entry, so write one */
	if(found_entry==FALSE)
		fprintf(fpout,"%s",new_service_string);

	/* reset file permissions */
	fchmod(tempfd,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* close files */
	fclose(fpout);
	fclose(fpin);

	/* move the temp file to the status log (overwrite the old status log) */
	if(my_rename(temp_file,xsddefault_status_log))
		return ERROR;


	return OK;
        }

#endif



#ifdef NSCGI

/******************************************************************/
/****************** DEFAULT DATA INPUT FUNCTIONS ******************/
/******************************************************************/

/* read all program, host, and service status information */
int xsddefault_read_status_data(char *config_file,int options){
	char input_buffer[MAX_INPUT_BUFFER];
	FILE *fp;
	int result;

	/* grab configuration data */
	result=xsddefault_grab_config_info(config_file);
	if(result==ERROR)
		return ERROR;

	/* opent the status log for reading */
	fp=fopen(xsddefault_status_log,"r");
	if(fp==NULL)
		return ERROR;

	/* read in all lines from the status log */
	for(fgets(input_buffer,sizeof(input_buffer)-1,fp);!feof(fp);fgets(input_buffer,sizeof(input_buffer)-1,fp)){

		/* skip blank lines and comments */
		if(input_buffer[0]=='#' || input_buffer[0]=='\x0' || input_buffer[0]=='\n' || input_buffer[0]=='\r')
			continue;

		strip(input_buffer);

		if(strstr(input_buffer,"] PROGRAM;") && (options & READ_PROGRAM_STATUS))
			result=xsddefault_add_program_status(input_buffer);

		else if(strstr(input_buffer,"] HOST;") && (options & READ_HOST_STATUS))
			result=xsddefault_add_host_status(input_buffer);

		else if(strstr(input_buffer,"] SERVICE;") && (options & READ_SERVICE_STATUS))
			result=xsddefault_add_service_status(input_buffer);

		if(result!=OK){
			/*printf("Error: %s\n",input_buffer);*/
			fclose(fp);
			return ERROR;
		        }
	        }

	fclose(fp);

	return OK;
        }



/* adds program status information */
int xsddefault_add_program_status(char *input_buffer){
	char *temp_buffer;
	time_t _program_start;
	int _nagios_pid;
	int _daemon_mode;
	time_t _last_command_check;
	time_t _last_log_rotation;
	int _enable_notifications;
	int _execute_service_checks;
	int _accept_passive_service_checks;
	int _enable_event_handlers;
	int _obsess_over_services;
	int _enable_flap_detection;
	int _enable_failure_prediction;
	int _process_performance_data;
	int result;


	if(input_buffer==NULL)
		return ERROR;

	/* skip the identifier string */
	temp_buffer=my_strtok(input_buffer,";");
	if(temp_buffer==NULL)
		return ERROR;

	/* program start time */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_program_start=(time_t)strtoul(temp_buffer,NULL,10);

	/* PID */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_nagios_pid=(temp_buffer==NULL)?0:atoi(temp_buffer);

	/* daemon mode flag */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_daemon_mode=(temp_buffer==NULL)?0:atoi(temp_buffer);

	/* last command check */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_last_command_check=(time_t)strtoul(temp_buffer,NULL,10);

	/* last log rotation */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_last_log_rotation=(time_t)strtoul(temp_buffer,NULL,10);

	/* enable notifications option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_enable_notifications=atoi(temp_buffer);

	/* execute service checks option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_execute_service_checks=atoi(temp_buffer);

	/* accept passive service checks option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_accept_passive_service_checks=atoi(temp_buffer);

	/* enable event handlers option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_enable_event_handlers=atoi(temp_buffer);

	/* obsess over services option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_obsess_over_services=atoi(temp_buffer);

	/* enable flap detection option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_enable_flap_detection=atoi(temp_buffer);

	/* enable failure prediction option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	_enable_failure_prediction=atoi(temp_buffer);

	/* process performance data option */
	temp_buffer=my_strtok(NULL,"\n");
	if(temp_buffer==NULL)
		return ERROR;
	_process_performance_data=atoi(temp_buffer);

	/* add program status information */
	result=add_program_status(_program_start,_nagios_pid,_daemon_mode,_last_command_check,_last_log_rotation,_enable_notifications,_execute_service_checks,_accept_passive_service_checks,_enable_event_handlers,_obsess_over_services,_enable_flap_detection,_enable_failure_prediction,_process_performance_data);
	if(result!=OK)
		return ERROR;


	return OK;
        }



/* adds host status information */
int xsddefault_add_host_status(char *input_buffer){
	char *temp_buffer;
	int result;
	time_t last_update;
	char *host_name="";
	char status[16];
	time_t last_check;
	time_t last_state_change;
	int problem_has_been_acknowledged;
	unsigned long time_up;
	unsigned long time_down;
	unsigned long time_unreachable;
	time_t last_notification;
	int current_notification_number;
	int notifications_enabled;
	int event_handler_enabled;
	int checks_enabled;
	int flap_detection_enabled;
	int is_flapping;
	double percent_state_change;
	int scheduled_downtime_depth;
	int failure_prediction_enabled;
	int process_performance_data;


	/* get the last host update time */
	temp_buffer=my_strtok(input_buffer,"]");
	if(temp_buffer==NULL)
		return ERROR;
	last_update=(time_t)strtoul(temp_buffer+1,NULL,10);

	/* get the host name */
	temp_buffer=my_strtok(NULL,";");
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	host_name=(char *)malloc(strlen(temp_buffer)+1);
	if(host_name==NULL)
		return ERROR;
	strcpy(host_name,temp_buffer);

	/* get the host status */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	strncpy(status,temp_buffer,sizeof(status)-1);
	status[sizeof(status)-1]='\x0';

	/* get the last check time */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	last_check=(time_t)strtoul(temp_buffer,NULL,10);

	/* get the last state change time */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	last_state_change=(time_t)strtoul(temp_buffer,NULL,10);

	/* get the acknowledgement option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	problem_has_been_acknowledged=atoi(temp_buffer);

	/* get the time up */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	time_up=strtoul(temp_buffer,NULL,10);

	/* get the time down */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	time_down=strtoul(temp_buffer,NULL,10);

	/* get the time unreachable */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	time_unreachable=strtoul(temp_buffer,NULL,10);

	/* get the last notification time */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	last_notification=(time_t)strtoul(temp_buffer,NULL,10);

	/* get the current notification number */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	current_notification_number=atoi(temp_buffer);

	/* get the notifications enabled option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	notifications_enabled=atoi(temp_buffer);

	/* get the event handler enabled option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	event_handler_enabled=atoi(temp_buffer);

	/* get the check enabled option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	checks_enabled=atoi(temp_buffer);

	/* get the flap detection enabled option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	flap_detection_enabled=atoi(temp_buffer);

	/* get the flapping indicator */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	is_flapping=atoi(temp_buffer);

	/* get the percent state change */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	percent_state_change=strtod(temp_buffer,NULL);

	/* get the scheduled downtime depth */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	scheduled_downtime_depth=atoi(temp_buffer);

	/* get the failure prediction enabled option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	failure_prediction_enabled=atoi(temp_buffer);

	/* get the performance data processing option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	process_performance_data=atoi(temp_buffer);

	/* get the plugin output */
	temp_buffer=my_strtok(NULL,"\n");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }


	/* add this host status */
	result=add_host_status(host_name,status,last_update,last_check,last_state_change,problem_has_been_acknowledged,time_up,time_down,time_unreachable,last_notification,current_notification_number,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,temp_buffer);
	if(result!=OK)
		return ERROR;

	return OK;
        }



/* adds service status information */
int xsddefault_add_service_status(char *input_buffer){
	char *temp_buffer;
	int result;
	time_t last_update;
	char *host_name="";
	char *description="";
	char status[16];
	int current_attempt;
	int max_attempts;
	int state_type;
	time_t last_check;
	time_t next_check;
	int check_type;
	int checks_enabled;
	int accept_passive_service_checks;
	int event_handler_enabled;
	time_t last_state_change;
	int problem_has_been_acknowledged;
	char last_hard_state[16];
	unsigned long time_ok;
	unsigned long time_warning;
	unsigned long time_unknown;
	unsigned long time_critical;
	time_t last_notification;
	int current_notification_number;
	int notifications_enabled;
	int latency;
	int execution_time;
	int flap_detection_enabled;
	int is_flapping;
	double percent_state_change;
	int scheduled_downtime_depth;
	int failure_prediction_enabled;
	int process_performance_data;
	int obsess_over_service;


	/* get the last update time */
	temp_buffer=my_strtok(input_buffer,"]");
	if(temp_buffer==NULL)
		return ERROR;
	last_update=(time_t)strtoul(temp_buffer+1,NULL,10);

	/* get the host name */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL)
		return ERROR;
	host_name=(char *)malloc(strlen(temp_buffer)+1);
	if(host_name==NULL)
		return ERROR;
	strcpy(host_name,temp_buffer);

	/* get the service description */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	description=(char *)malloc(strlen(temp_buffer)+1);
	if(description==NULL){
		free(host_name);
		return ERROR;
	        }
	strcpy(description,temp_buffer);

	/* get the service status */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	strncpy(status,temp_buffer,sizeof(status)-1);
	status[sizeof(status)-1]='\x0';

	/* get the current attempt */
	temp_buffer=my_strtok(NULL,"/");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	current_attempt=atoi(temp_buffer);

	/* get the max attempts */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	max_attempts=atoi(temp_buffer);

	/* get the state type */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	state_type=(!strcmp(temp_buffer,"SOFT"))?SOFT_STATE:HARD_STATE;

	/* get the last check time */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	last_check=(time_t)strtoul(temp_buffer,NULL,10);

	/* get the next check time */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	next_check=(time_t)strtoul(temp_buffer,NULL,10);

	/* get the check type */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	check_type=(!strcmp(temp_buffer,"ACTIVE"))?SERVICE_CHECK_ACTIVE:SERVICE_CHECK_PASSIVE;


	/* get the check enabled argument */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	checks_enabled=atoi(temp_buffer);

	/* get the passive check argument */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	accept_passive_service_checks=atoi(temp_buffer);

	/* get the event handler enabled argument */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	event_handler_enabled=atoi(temp_buffer);

	/* get the last state change time */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	last_state_change=(time_t)strtoul(temp_buffer,NULL,10);

	/* get the acknowledgement option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	problem_has_been_acknowledged=atoi(temp_buffer);

	/* get the last hard state */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	strncpy(last_hard_state,temp_buffer,sizeof(last_hard_state)-1);
	last_hard_state[sizeof(last_hard_state)-1]='\x0';

	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	time_ok=strtoul(temp_buffer,NULL,10);

	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	time_unknown=strtoul(temp_buffer,NULL,10);

	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	time_warning=strtoul(temp_buffer,NULL,10);

	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	time_critical=strtoul(temp_buffer,NULL,10);

	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	last_notification=(time_t)strtoul(temp_buffer,NULL,10);

	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	current_notification_number=atoi(temp_buffer);

	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	notifications_enabled=atoi(temp_buffer);

	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	latency=atoi(temp_buffer);

	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	execution_time=atoi(temp_buffer);

	/* get the flap detection enabled option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	flap_detection_enabled=atoi(temp_buffer);

	/* get the flapping indicator */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	is_flapping=atoi(temp_buffer);

	/* get the percent state change */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	percent_state_change=strtod(temp_buffer,NULL);

	/* get the scheduled downtime depth */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }
	scheduled_downtime_depth=atoi(temp_buffer);

	/* get the failure prediction enabled option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	failure_prediction_enabled=atoi(temp_buffer);

	/* get the performance data processing option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	process_performance_data=atoi(temp_buffer);

	/* get the obsess over service option */
	temp_buffer=my_strtok(NULL,";");
	if(temp_buffer==NULL){
		free(host_name);
		return ERROR;
	        }
	obsess_over_service=atoi(temp_buffer);

	/* get the plugin output */
	temp_buffer=my_strtok(NULL,"\n");
	if(temp_buffer==NULL){
		free(host_name);
		free(description);
		return ERROR;
	        }

	/* add this service status */
	result=add_service_status(host_name,description,status,last_update,current_attempt,max_attempts,state_type,last_check,next_check,check_type,checks_enabled,accept_passive_service_checks,event_handler_enabled,last_state_change,problem_has_been_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,temp_buffer);
	if(result!=OK)
		return ERROR;

	return OK;
        }


#endif

