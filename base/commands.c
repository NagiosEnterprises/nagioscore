/*****************************************************************************
 *
 * COMMANDS.C - External command functions for Nagios
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   01-21-2002
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
#include "../common/comments.h"
#include "../common/statusdata.h"
#include "sretention.h"
#include "nagios.h"

extern char     config_file[MAX_FILENAME_LENGTH];
extern char	log_file[MAX_FILENAME_LENGTH];
extern char     command_file[MAX_FILENAME_LENGTH];
extern char     temp_file[MAX_FILENAME_LENGTH];
extern char     comment_file[MAX_FILENAME_LENGTH];

extern int      sigshutdown;
extern int      sigrestart;

extern int      check_external_commands;

extern int      ipc_pipe[2];

extern time_t   last_command_check;

extern int      enable_notifications;
extern int      execute_service_checks;
extern int      accept_passive_service_checks;
extern int      enable_event_handlers;
extern int      obsess_over_services;
extern int      enable_failure_prediction;
extern int      process_performance_data;

extern int      log_external_commands;
extern int      log_passive_checks;

extern timed_event      *event_list_high;
extern timed_event      *event_list_low;
extern service          *service_list;
extern host             *host_list;

extern FILE     *command_file_fp;

passive_check_result    *passive_check_result_list;

int             flush_pending_commands=FALSE;


/******************************************************************/
/****************** EXTERNAL COMMAND PROCESSING *******************/
/******************************************************************/

/* checks for the existence of the external command file and processes all
   commands found in it */
void check_for_external_commands(void){
	char buffer[MAX_INPUT_BUFFER];
	char command_id[MAX_INPUT_BUFFER];
	char args[MAX_INPUT_BUFFER];
	time_t entry_time;
	int command_type=CMD_NONE;
	char *temp_ptr;

#ifdef DEBUG0
	printf("check_for_external_commands() start\n");
#endif

	/* bail out if we shouldn't be checking for external commands */
	if(check_external_commands==FALSE)
		return;

	/* update last command check time */
	last_command_check=time(NULL);

	/* update the status log with new program information */
	update_program_status(FALSE);

	/* reset passive check result list pointer */
	passive_check_result_list=NULL;

	/* reset flush flag */
	flush_pending_commands=FALSE;

	/* clear EOF condition from prior run (FreeBSD fix) */
	clearerr(command_file_fp);

	/* process all commands in the file (now implemented as a named pipe) */
	while(fgets(buffer,(int)(sizeof(buffer)-1),command_file_fp)!=NULL){

		/* we're flushing all remaining commands from the file */
		if(flush_pending_commands==TRUE)
			continue;

		/* strip the buffer of newlines and carriage returns */
		strip(buffer);

#ifdef DEBUG3
		printf("\tRaw command entry: %s\n",buffer);
#endif

		/* get the command entry time */
		temp_ptr=my_strtok(buffer,"[");
		if(temp_ptr==NULL)
			continue;
		temp_ptr=my_strtok(NULL,"]");
		if(temp_ptr==NULL)
			continue;
		entry_time=(time_t)strtoul(temp_ptr,NULL,10);

		/* get the command identifier */
		temp_ptr=my_strtok(NULL,";");
		if(temp_ptr==NULL)
			continue;
		strncpy(command_id,temp_ptr+1,sizeof(command_id)-1);
		command_id[sizeof(command_id)-1]='\x0';

		/* get the command arguments */
		temp_ptr=my_strtok(NULL,"\n");
		if(temp_ptr==NULL)
			strcpy(args,"");
		else{
			strncpy(args,temp_ptr,sizeof(args)-1);
			args[sizeof(args)-1]='\x0';
		        }

		/* decide what type of command this is */
		if(!strcmp(command_id,"ADD_HOST_COMMENT"))
			command_type=CMD_ADD_HOST_COMMENT;
		else if(!strcmp(command_id,"ADD_SVC_COMMENT"))
			command_type=CMD_ADD_SVC_COMMENT;

		else if(!strcmp(command_id,"DEL_HOST_COMMENT"))
			command_type=CMD_DEL_HOST_COMMENT;
		else if(!strcmp(command_id,"DEL_SVC_COMMENT"))
			command_type=CMD_DEL_SVC_COMMENT;

		else if(!strcmp(command_id,"DELAY_HOST_NOTIFICATION"))
			command_type=CMD_DELAY_HOST_NOTIFICATION;
		else if(!strcmp(command_id,"DELAY_SVC_NOTIFICATION"))
			command_type=CMD_DELAY_SVC_NOTIFICATION;

		/* this is used for immediate and delayed service checks... */
		else if(!strcmp(command_id,"SCHEDULE_SVC_CHECK"))
			command_type=CMD_DELAY_SVC_CHECK;
		else if(!strcmp(command_id,"SCHEDULE_FORCED_SVC_CHECK"))
			command_type=CMD_FORCE_DELAY_SVC_CHECK;

		else if(!strcmp(command_id,"ENABLE_SVC_CHECK"))
			command_type=CMD_ENABLE_SVC_CHECK;
		else if(!strcmp(command_id,"DISABLE_SVC_CHECK"))
			command_type=CMD_DISABLE_SVC_CHECK;

		/* enable/disable notifications */
		else if(!strcmp(command_id,"ENTER_STANDBY_MODE") || !strcmp(command_id,"DISABLE_NOTIFICATIONS"))
			command_type=CMD_DISABLE_NOTIFICATIONS;
		else if(!strcmp(command_id,"ENTER_ACTIVE_MODE") || !strcmp(command_id,"ENABLE_NOTIFICATIONS"))
			command_type=CMD_ENABLE_NOTIFICATIONS;

		else if(!strcmp(command_id,"SHUTDOWN_PROGRAM"))
			command_type=CMD_SHUTDOWN_PROCESS;
		else if(!strcmp(command_id,"RESTART_PROGRAM"))
			command_type=CMD_RESTART_PROCESS;

		else if(!strcmp(command_id,"ENABLE_HOST_SVC_CHECKS"))
			command_type=CMD_ENABLE_HOST_SVC_CHECKS;
		else if(!strcmp(command_id,"DISABLE_HOST_SVC_CHECKS"))
			command_type=CMD_DISABLE_HOST_SVC_CHECKS;

		/* this is used for immediate and delayed service checks... */
		else if(!strcmp(command_id,"SCHEDULE_HOST_SVC_CHECKS"))
			command_type=CMD_DELAY_HOST_SVC_CHECKS;
		else if(!strcmp(command_id,"SCHEDULE_FORCED_HOST_SVC_CHECKS"))
			command_type=CMD_FORCE_DELAY_HOST_SVC_CHECKS;

		else if(!strcmp(command_id,"DEL_ALL_HOST_COMMENTS"))
			command_type=CMD_DEL_ALL_HOST_COMMENTS;
		else if(!strcmp(command_id,"DEL_ALL_SVC_COMMENTS"))
			command_type=CMD_DEL_ALL_SVC_COMMENTS;

		else if(!strcmp(command_id,"ENABLE_SVC_NOTIFICATIONS"))
			command_type=CMD_ENABLE_SVC_NOTIFICATIONS;
		else if(!strcmp(command_id,"DISABLE_SVC_NOTIFICATIONS"))
			command_type=CMD_DISABLE_SVC_NOTIFICATIONS;

		else if(!strcmp(command_id,"ENABLE_HOST_NOTIFICATIONS"))
			command_type=CMD_ENABLE_HOST_NOTIFICATIONS;
		else if(!strcmp(command_id,"DISABLE_HOST_NOTIFICATIONS"))
			command_type=CMD_DISABLE_HOST_NOTIFICATIONS;

		else if(!strcmp(command_id,"ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST"))
			command_type=CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST;
		else if(!strcmp(command_id,"DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST"))
			command_type=CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST;

		else if(!strcmp(command_id,"ENABLE_HOST_SVC_NOTIFICATIONS"))
			command_type=CMD_ENABLE_HOST_SVC_NOTIFICATIONS;
		else if(!strcmp(command_id,"DISABLE_HOST_SVC_NOTIFICATIONS"))
			command_type=CMD_DISABLE_HOST_SVC_NOTIFICATIONS;

		else if(!strcmp(command_id,"PROCESS_SERVICE_CHECK_RESULT"))
			command_type=CMD_PROCESS_SERVICE_CHECK_RESULT;

		else if(!strcmp(command_id,"SAVE_STATE_INFORMATION"))
			command_type=CMD_SAVE_STATE_INFORMATION;
		else if(!strcmp(command_id,"READ_STATE_INFORMATION"))
			command_type=CMD_READ_STATE_INFORMATION;

		else if(!strcmp(command_id,"ACKNOWLEDGE_HOST_PROBLEM"))
			command_type=CMD_ACKNOWLEDGE_HOST_PROBLEM;
		else if(!strcmp(command_id,"ACKNOWLEDGE_SVC_PROBLEM"))
			command_type=CMD_ACKNOWLEDGE_SVC_PROBLEM;

		else if(!strcmp(command_id,"START_EXECUTING_SVC_CHECKS"))
			command_type=CMD_START_EXECUTING_SVC_CHECKS;
		else if(!strcmp(command_id,"STOP_EXECUTING_SVC_CHECKS"))
			command_type=CMD_STOP_EXECUTING_SVC_CHECKS;

		else if(!strcmp(command_id,"START_ACCEPTING_PASSIVE_SVC_CHECKS"))
			command_type=CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS;
		else if(!strcmp(command_id,"STOP_ACCEPTING_PASSIVE_SVC_CHECKS"))
			command_type=CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS;

		else if(!strcmp(command_id,"ENABLE_PASSIVE_SVC_CHECKS"))
			command_type=CMD_ENABLE_PASSIVE_SVC_CHECKS;
		else if(!strcmp(command_id,"DISABLE_PASSIVE_SVC_CHECKS"))
			command_type=CMD_DISABLE_PASSIVE_SVC_CHECKS;

		else if(!strcmp(command_id,"ENABLE_EVENT_HANDLERS"))
			command_type=CMD_ENABLE_EVENT_HANDLERS;
		else if(!strcmp(command_id,"DISABLE_EVENT_HANDLERS"))
			command_type=CMD_DISABLE_EVENT_HANDLERS;

		else if(!strcmp(command_id,"ENABLE_SVC_EVENT_HANDLER"))
			command_type=CMD_ENABLE_SVC_EVENT_HANDLER;
		else if(!strcmp(command_id,"DISABLE_SVC_EVENT_HANDLER"))
			command_type=CMD_DISABLE_SVC_EVENT_HANDLER;
		
		else if(!strcmp(command_id,"ENABLE_HOST_EVENT_HANDLER"))
			command_type=CMD_ENABLE_HOST_EVENT_HANDLER;
		else if(!strcmp(command_id,"DISABLE_HOST_EVENT_HANDLER"))
			command_type=CMD_DISABLE_HOST_EVENT_HANDLER;

		else if(!strcmp(command_id,"ENABLE_HOST_CHECK"))
			command_type=CMD_ENABLE_HOST_CHECK;
		else if(!strcmp(command_id,"DISABLE_HOST_CHECK"))
			command_type=CMD_DISABLE_HOST_CHECK;

		else if(!strcmp(command_id,"START_OBSESSING_OVER_SVC_CHECKS"))
			command_type=CMD_START_OBSESSING_OVER_SVC_CHECKS;
		else if(!strcmp(command_id,"STOP_OBSESSING_OVER_SVC_CHECKS"))
			command_type=CMD_STOP_OBSESSING_OVER_SVC_CHECKS;

		else if(!strcmp(command_id,"REMOVE_HOST_ACKNOWLEDGEMENT"))
			command_type=CMD_REMOVE_HOST_ACKNOWLEDGEMENT;
		else if(!strcmp(command_id,"REMOVE_SVC_ACKNOWLEDGEMENT"))
			command_type=CMD_REMOVE_SVC_ACKNOWLEDGEMENT;

		else if(!strcmp(command_id,"SCHEDULE_HOST_DOWNTIME"))
			command_type=CMD_SCHEDULE_HOST_DOWNTIME;
		else if(!strcmp(command_id,"SCHEDULE_SVC_DOWNTIME"))
			command_type=CMD_SCHEDULE_SVC_DOWNTIME;

		else if(!strcmp(command_id,"ENABLE_FLAP_DETECTION"))
			command_type=CMD_ENABLE_FLAP_DETECTION;
		else if(!strcmp(command_id,"DISABLE_FLAP_DETECTION"))
			command_type=CMD_DISABLE_FLAP_DETECTION;

		else if(!strcmp(command_id,"ENABLE_HOST_FLAP_DETECTION"))
			command_type=CMD_ENABLE_HOST_FLAP_DETECTION;
		else if(!strcmp(command_id,"DISABLE_HOST_FLAP_DETECTION"))
			command_type=CMD_DISABLE_HOST_FLAP_DETECTION;

		else if(!strcmp(command_id,"ENABLE_SVC_FLAP_DETECTION"))
			command_type=CMD_ENABLE_SVC_FLAP_DETECTION;
		else if(!strcmp(command_id,"DISABLE_SVC_FLAP_DETECTION"))
			command_type=CMD_DISABLE_SVC_FLAP_DETECTION;

		else if(!strcmp(command_id,"DEL_HOST_DOWNTIME"))
			command_type=CMD_DEL_HOST_DOWNTIME;
		else if(!strcmp(command_id,"DEL_SVC_DOWNTIME"))
			command_type=CMD_DEL_SVC_DOWNTIME;

		else if(!strcmp(command_id,"FLUSH_PENDING_COMMANDS"))
			command_type=CMD_FLUSH_PENDING_COMMANDS;

		else if(!strcmp(command_id,"ENABLE_FAILURE_PREDICTION"))
			command_type=CMD_ENABLE_FAILURE_PREDICTION;
		else if(!strcmp(command_id,"DISABLE_FAILURE_PREDICTION"))
			command_type=CMD_DISABLE_FAILURE_PREDICTION;

		else if(!strcmp(command_id,"ENABLE_PERFORMANCE_DATA"))
			command_type=CMD_ENABLE_PERFORMANCE_DATA;
		else if(!strcmp(command_id,"DISABLE_PERFORMANCE_DATA"))
			command_type=CMD_DISABLE_PERFORMANCE_DATA;

		else if(!strcmp(command_id,"SCHEDULE_HOST_SVC_DOWNTIME"))
			command_type=CMD_SCHEDULE_HOST_SVC_DOWNTIME;

		else{
			/* log the bad external command */
			snprintf(buffer,sizeof(buffer),"Warning: Unrecognized external command -> %s;%s\n",command_id,args);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_EXTERNAL_COMMAND | NSLOG_RUNTIME_WARNING,FALSE);

			continue;
		        }

		/* log the external command */
		snprintf(buffer,sizeof(buffer),"EXTERNAL COMMAND: %s;%s\n",command_id,args);
		buffer[sizeof(buffer)-1]='\x0';
		if(command_type==CMD_PROCESS_SERVICE_CHECK_RESULT){
			if(log_passive_checks==TRUE)
				write_to_logs_and_console(buffer,NSLOG_PASSIVE_CHECK,FALSE);
		        }
		else{
			if(log_external_commands==TRUE)
				write_to_logs_and_console(buffer,NSLOG_EXTERNAL_COMMAND,FALSE);
		        }

		/* process the command if its not a passive check */
		process_external_command(command_type,entry_time,args);
	        }

	/* rewind file pointer (fix for FreeBSD, may already be taken care of due to clearerr() call before reading begins) */
	rewind(command_file_fp);


	/**** PROCESS ALL PASSIVE CHECK RESULTS AT ONE TIME ****/
	if(passive_check_result_list!=NULL)
		process_passive_service_checks();


#ifdef DEBUG0
	printf("check_for_external_commands() end\n");
#endif

	return;
        }



/* top-level processor for a single external command */
void process_external_command(int cmd,time_t entry_time,char *args){

#ifdef DEBUG0
	printf("process_external_command() start\n");
#endif

#ifdef DEBUG3
	printf("\tExternal Command Type: %d\n",cmd);
	printf("\tCommand Entry Time: %lu\n",(unsigned long)entry_time);
	printf("\tCommand Arguments: %s\n",args);
#endif

	/* how shall we execute the command? */
	switch(cmd){

	case CMD_ADD_HOST_COMMENT:
	case CMD_ADD_SVC_COMMENT:
		cmd_add_comment(cmd,entry_time,args);
		break;

	case CMD_DEL_HOST_COMMENT:
	case CMD_DEL_SVC_COMMENT:
		cmd_delete_comment(cmd,args);
		break;

	case CMD_DELAY_HOST_NOTIFICATION:
	case CMD_DELAY_SVC_NOTIFICATION:
		cmd_delay_notification(cmd,args);
		break;

	case CMD_DELAY_SVC_CHECK:
	case CMD_FORCE_DELAY_SVC_CHECK:
	case CMD_IMMEDIATE_SVC_CHECK:
		cmd_schedule_service_check(cmd,args,(cmd==CMD_FORCE_DELAY_SVC_CHECK)?TRUE:FALSE);
		break;

	case CMD_ENABLE_SVC_CHECK:
	case CMD_DISABLE_SVC_CHECK:
		cmd_enable_disable_service_check(cmd,args);
		break;

	case CMD_ENABLE_NOTIFICATIONS:
	case CMD_DISABLE_NOTIFICATIONS:
		cmd_enable_disable_notifications(cmd,args);
		break;

	case CMD_SHUTDOWN_PROCESS:
	case CMD_RESTART_PROCESS:
		cmd_signal_process(cmd,args);
		break;

	case CMD_ENABLE_HOST_SVC_CHECKS:
	case CMD_DISABLE_HOST_SVC_CHECKS:
		cmd_enable_disable_host_service_checks(cmd,args);
		break;

	case CMD_DELAY_HOST_SVC_CHECKS:
	case CMD_FORCE_DELAY_HOST_SVC_CHECKS:
	case CMD_IMMEDIATE_HOST_SVC_CHECKS:
		cmd_schedule_host_service_checks(cmd,args,(cmd==CMD_FORCE_DELAY_HOST_SVC_CHECKS)?TRUE:FALSE);
		break;

	case CMD_DEL_ALL_HOST_COMMENTS:
	case CMD_DEL_ALL_SVC_COMMENTS:
		cmd_delete_all_comments(cmd,args);
		break;

	case CMD_ENABLE_SVC_NOTIFICATIONS:
	case CMD_DISABLE_SVC_NOTIFICATIONS:
		cmd_enable_disable_service_notifications(cmd,args);
		break;

	case CMD_ENABLE_HOST_NOTIFICATIONS:
	case CMD_DISABLE_HOST_NOTIFICATIONS:
		cmd_enable_disable_host_notifications(cmd,args);
		break;

	case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
	case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		cmd_enable_disable_and_propagate_notifications(cmd,args);
		break;

	case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
		cmd_enable_disable_host_service_notifications(cmd,args);
		break;

	case CMD_PROCESS_SERVICE_CHECK_RESULT:
		cmd_process_service_check_result(cmd,entry_time,args);
		break;

	case CMD_SAVE_STATE_INFORMATION:
		save_state_information(config_file,FALSE);
		break;

	case CMD_READ_STATE_INFORMATION:
		read_initial_state_information(config_file);
		break;

	case CMD_ACKNOWLEDGE_HOST_PROBLEM:
	case CMD_ACKNOWLEDGE_SVC_PROBLEM:
		cmd_acknowledge_problem(cmd,args);
		break;

	case CMD_START_EXECUTING_SVC_CHECKS:
	case CMD_STOP_EXECUTING_SVC_CHECKS:
		cmd_start_stop_executing_service_checks(cmd);
		break;

	case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
	case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
		cmd_start_stop_accepting_passive_checks(cmd);
		break;

	case CMD_ENABLE_PASSIVE_SVC_CHECKS:
	case CMD_DISABLE_PASSIVE_SVC_CHECKS:
		cmd_enable_disable_passive_service_checks(cmd,args);
		break;

	case CMD_ENABLE_EVENT_HANDLERS:
		start_using_event_handlers();
		break;

	case CMD_DISABLE_EVENT_HANDLERS:
		stop_using_event_handlers();
		break;

	case CMD_ENABLE_SVC_EVENT_HANDLER:
	case CMD_DISABLE_SVC_EVENT_HANDLER:
		cmd_enable_disable_service_event_handler(cmd,args);
		break;

	case CMD_ENABLE_HOST_EVENT_HANDLER:
	case CMD_DISABLE_HOST_EVENT_HANDLER:
		cmd_enable_disable_host_event_handler(cmd,args);
		break;

	case CMD_ENABLE_HOST_CHECK:
	case CMD_DISABLE_HOST_CHECK:
		cmd_enable_disable_host_check(cmd,args);
		break;

	case CMD_START_OBSESSING_OVER_SVC_CHECKS:
	case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
		cmd_start_stop_obsessing_over_service_checks(cmd);
		break;

	case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
	case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
		cmd_remove_acknowledgement(cmd,args);
		break;

	case CMD_SCHEDULE_HOST_DOWNTIME:
	case CMD_SCHEDULE_SVC_DOWNTIME:
		cmd_schedule_downtime(cmd,entry_time,args);
		break;

	case CMD_ENABLE_FLAP_DETECTION:
		enable_flap_detection_routines();
		break;

	case CMD_DISABLE_FLAP_DETECTION:
		disable_flap_detection_routines();
		break;
	
	case CMD_ENABLE_HOST_FLAP_DETECTION:
	case CMD_DISABLE_HOST_FLAP_DETECTION:
		cmd_enable_disable_host_flap_detection(cmd,args);
		break;

	case CMD_ENABLE_SVC_FLAP_DETECTION:
	case CMD_DISABLE_SVC_FLAP_DETECTION:
		cmd_enable_disable_service_flap_detection(cmd,args);
		break;

	case CMD_DEL_HOST_DOWNTIME:
	case CMD_DEL_SVC_DOWNTIME:
		cmd_delete_downtime(cmd,args);
		break;

	case CMD_CANCEL_ACTIVE_HOST_SVC_DOWNTIME:
	case CMD_CANCEL_PENDING_HOST_SVC_DOWNTIME:
		break;

	case CMD_FLUSH_PENDING_COMMANDS:
		flush_pending_commands=TRUE;
		break;

	case CMD_ENABLE_FAILURE_PREDICTION:
		enable_all_failure_prediction();
		break;
		
	case CMD_DISABLE_FAILURE_PREDICTION:
		disable_all_failure_prediction();
		break;

	case CMD_ENABLE_PERFORMANCE_DATA:
		enable_performance_data();
		break;

	case CMD_DISABLE_PERFORMANCE_DATA:
		disable_performance_data();
		break;

	case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
		cmd_schedule_host_service_downtime(cmd,entry_time,args);
		break;

	default:
		break;
	        }

#ifdef DEBUG0
	printf("process_external_command() end\n");
#endif

	return;
        }



/******************************************************************/
/*************** EXTERNAL COMMAND IMPLEMENTATIONS  ****************/
/******************************************************************/

/* adds a host or service comment to the status log */
int cmd_add_comment(int cmd,time_t entry_time,char *args){
	char *temp_ptr;
	host *temp_host;
	service *temp_service;
	char *host_name="";
	char *svc_description="";
	char *user;
	char *comment;
	int persistent=0;
	int result;

#ifdef DEBUG0
	printf("cmd_add_comment() start\n");
#endif

	/* get the host nmae */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* if we're adding a service comment...  */
	if(cmd==CMD_ADD_SVC_COMMENT){

		/* get the service description */
		svc_description=my_strtok(NULL,";");
		if(svc_description==NULL)
			return ERROR;

		/* verify that the service is valid */
		temp_service=find_service(host_name,svc_description,NULL);
		if(temp_service==NULL)
			return ERROR;
	        }

	/* else verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* get the persistant flag */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	persistent=atoi(temp_ptr);
	if(persistent>1)
		persistent=1;
	else if(persistent<0)
		persistent=0;

	/* get the name of the user who entered the comment */
	user=my_strtok(NULL,";");
	if(user==NULL)
		return ERROR;

	/* get the comment */
	comment=my_strtok(NULL,"\n");
	if(comment==NULL)
		return ERROR;

	/* add the comment */
	result=save_comment((cmd==CMD_ADD_HOST_COMMENT)?HOST_COMMENT:SERVICE_COMMENT,host_name,svc_description,entry_time,user,comment,persistent,NULL);

	if(result<0)
		return ERROR;

#ifdef DEBUG0
	printf("cmd_add_comment() end\n");
#endif
	return OK;
        }



/* removes a host or service comment from the status log */
int cmd_delete_comment(int cmd,char *args){
	int comment_id;

#ifdef DEBUG0
	printf("cmd_del_comment() start\n");
#endif
	
	/* get the comment id we should delete */
	comment_id=atoi(args);
	if(comment_id<=0)
		return ERROR;

	/* delete the specified comment */
	delete_comment((cmd==CMD_DEL_HOST_COMMENT)?HOST_COMMENT:SERVICE_COMMENT,comment_id);

#ifdef DEBUG0
	printf("cmd_del_comment() end\n");
#endif
	return OK;
        }



/* removes all comments associated with a host or service from the status log */
int cmd_delete_all_comments(int cmd,char *args){
	service *temp_service=NULL;
	host *temp_host=NULL;
	char *host_name="";
	char *svc_description="";

#ifdef DEBUG0
	printf("cmd_del_all_comments() start\n");
#endif
	
	/* get the host nmae */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* if we're deleting service comments...  */
	if(cmd==CMD_DEL_ALL_SVC_COMMENTS){

		/* get the service description */
		svc_description=my_strtok(NULL,";");
		if(svc_description==NULL)
			return ERROR;

		/* verify that the service is valid */
		temp_service=find_service(host_name,svc_description,NULL);
		if(temp_service==NULL)
			return ERROR;
	        }

	/* else verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* delete comments */
	delete_all_comments((cmd==CMD_DEL_ALL_HOST_COMMENTS)?HOST_COMMENT:SERVICE_COMMENT,host_name,svc_description);

#ifdef DEBUG0
	printf("cmd_del_all_comments() end\n");
#endif
	return OK;
        }



/* delays a host or service notification for given number of minutes */
int cmd_delay_notification(int cmd,char *args){
	char *temp_ptr;
	host *temp_host=NULL;
	service *temp_service=NULL;
	char *host_name="";
	char *svc_description="";
	time_t delay_time;

#ifdef DEBUG0
	printf("cmd_delay_notification() start\n");
#endif

	/* get the host nmae */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* if this is a service notification delay...  */
	if(cmd==CMD_DELAY_SVC_NOTIFICATION){

		/* get the service description */
		svc_description=my_strtok(NULL,";");
		if(svc_description==NULL)
			return ERROR;

		/* verify that the service is valid */
		temp_service=find_service(host_name,svc_description,NULL);
		if(temp_service==NULL)
			return ERROR;
	        }

	/* else verify that the host is valid */
	else{

		temp_host=find_host(host_name,NULL);
		if(temp_host==NULL)
			return ERROR;
	        }

	/* get the time that we should delay until... */
	temp_ptr=my_strtok(NULL,"\n");
	if(temp_ptr==NULL)
		return ERROR;
	delay_time=strtoul(temp_ptr,NULL,10);

	/* delay the next notification... */
	if(cmd==CMD_DELAY_HOST_NOTIFICATION)
		temp_host->last_host_notification=delay_time;
	else
		temp_service->last_notification=delay_time;
	

#ifdef DEBUG0
	printf("cmd_delay_notification() end\n");
#endif
	return OK;
        }



/* schedules a service check at a particular time */
int cmd_schedule_service_check(int cmd,char *args, int force){
	char *temp_ptr;
	service *temp_service=NULL;
	char *host_name="";
	char *svc_description="";
	time_t delay_time=0L;

#ifdef DEBUG0
	printf("cmd_schedule_service_check() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* get the service description */
	svc_description=my_strtok(NULL,";");
	if(svc_description==NULL)
		return ERROR;

	/* verify that the service is valid */
	temp_service=find_service(host_name,svc_description,NULL);
	if(temp_service==NULL)
		return ERROR;

	/* get the next check time */
	temp_ptr=my_strtok(NULL,"\n");
	if(temp_ptr==NULL)
		return ERROR;
	delay_time=strtoul(temp_ptr,NULL,10);

	/* schedule a delayed service check */
	schedule_service_check(temp_service,delay_time,force);

#ifdef DEBUG0
	printf("cmd_schedule_service_check() end\n");
#endif
	return OK;
        }



/* schedules all service checks on a host for a particular time */
int cmd_schedule_host_service_checks(int cmd,char *args, int force){
	char *temp_ptr;
	host *temp_host=NULL;
	service *temp_service=NULL;
	char *host_name="";
	time_t delay_time=0L;

#ifdef DEBUG0
	printf("cmd_schedule_host_service_checks() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* get the next check time */
	temp_ptr=my_strtok(NULL,"\n");
	if(temp_ptr==NULL)
		return ERROR;
	delay_time=strtoul(temp_ptr,NULL,10);

	/* reschedule all services on the specified host */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		/* schedule a delayed service check */
		if(!strcmp(host_name,temp_service->host_name))
			schedule_service_check(temp_service,delay_time,force);
	        }


#ifdef DEBUG0
	printf("cmd_schedule_host_service_checks() end\n");
#endif
	return OK;
        }



/* enables or disables a service check */
int cmd_enable_disable_service_check(int cmd,char *args){
	service *temp_service=NULL;
	char *host_name="";
	char *svc_description="";

#ifdef DEBUG0
	printf("cmd_enable_disable_service_check() start\n");
#endif

	/* get the host nmae */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* get the service description */
	svc_description=my_strtok(NULL,";");
	if(svc_description==NULL)
		return ERROR;

	/* verify that the service is valid */
	temp_service=find_service(host_name,svc_description,NULL);
	if(temp_service==NULL)
		return ERROR;

	if(cmd==CMD_ENABLE_SVC_CHECK)
		enable_service_check(temp_service);
	else
		disable_service_check(temp_service);

#ifdef DEBUG0
	printf("cmd_enable_disable_service_check() end\n");
#endif
	return OK;
        }



/* enables/disables all service checks for a host */
int cmd_enable_disable_host_service_checks(int cmd,char *args){
	host *temp_host=NULL;
	service *temp_service=NULL;
	char *host_name="";

#ifdef DEBUG0
	printf("cmd_enable_disable_host_service_checks() start\n");
#endif

	/* get the host nmae */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* disable all services associated with this host... */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		if(!strcmp(temp_service->host_name,temp_host->name)){
			if(cmd==CMD_ENABLE_HOST_SVC_CHECKS)
				enable_service_check(temp_service);
			else
				disable_service_check(temp_service);
		        }
	        }

#ifdef DEBUG0
	printf("cmd_enable_disable_host_service_checks() end\n");
#endif
	return OK;
        }



/* schedules a program shutdown or restart */
int cmd_signal_process(int cmd, char *args){
	timed_event *new_event=NULL;
	time_t scheduled_time;
	char *temp_ptr;

#ifdef DEBUG0
	printf("cmd_signal_process() start\n");
#endif

	/* get the time to schedule the event */
	temp_ptr=my_strtok(args,"\n");
	if(temp_ptr==NULL)
		scheduled_time=0L;
	else
		scheduled_time=strtoul(temp_ptr,NULL,10);

	/* add a scheduled program shutdown or restart to the event list */
	new_event=malloc(sizeof(timed_event));
	if(new_event!=NULL){
		new_event->event_type=(cmd==CMD_SHUTDOWN_PROCESS)?EVENT_PROGRAM_SHUTDOWN:EVENT_PROGRAM_RESTART;
		new_event->event_data=(void *)NULL;
		new_event->run_time=scheduled_time;
		new_event->recurring=FALSE;
		schedule_event(new_event,&event_list_high);
	        }
	else
		return ERROR;

#ifdef DEBUG0
	printf("cmd_signal_process() end\n");
#endif

	return OK;
        }


int cmd_enable_disable_notifications(int cmd, char *args){
	timed_event *new_event=NULL;
	time_t scheduled_time;
	char *temp_ptr;

#ifdef DEBUG0
	printf("cmd_enable_disable_notifications() start\n");
#endif

	/* get the time to schedule the event */
	temp_ptr=my_strtok(args,"\n");
	if(temp_ptr==NULL)
		scheduled_time=0L;
	else
		scheduled_time=strtoul(temp_ptr,NULL,10);

	/* no need to schedule a future change, just do it now */
	if(scheduled_time==0L){
		if(cmd==CMD_ENABLE_NOTIFICATIONS)
			enable_all_notifications();
		else
			disable_all_notifications();
	        }

	/* else add a scheduled notification change */
	else{

		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=(cmd==CMD_ENABLE_NOTIFICATIONS)?EVENT_ENABLE_NOTIFICATIONS:EVENT_DISABLE_NOTIFICATIONS;
			new_event->event_data=(void *)NULL;
			new_event->run_time=scheduled_time;
			new_event->recurring=FALSE;
			schedule_event(new_event,&event_list_high);
	                }
		else
			return ERROR;
	        }

#ifdef DEBUG0
	printf("cmd_enable_disable_notifications() end\n");
#endif

	return OK;
        }


/* enables/disables notifications for a service */
int cmd_enable_disable_service_notifications(int cmd,char *args){
	service *temp_service=NULL;
	char *host_name="";
	char *svc_description="";

#ifdef DEBUG0
	printf("cmd_enable_disable_service_notifications() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* get the service description */
	svc_description=my_strtok(NULL,";");
	if(svc_description==NULL)
		return ERROR;

	/* verify that the service is valid */
	temp_service=find_service(host_name,svc_description,NULL);
	if(temp_service==NULL)
		return ERROR;

	if(cmd==CMD_ENABLE_SVC_NOTIFICATIONS)
		enable_service_notifications(temp_service);
	else
		disable_service_notifications(temp_service);

#ifdef DEBUG0
	printf("cmd_enable_disable_service_notifications() end\n");
#endif
	return OK;
        }


/* enables/disables notifications for a host */
int cmd_enable_disable_host_notifications(int cmd,char *args){
	host *temp_host=NULL;
	char *host_name="";

#ifdef DEBUG0
	printf("cmd_enable_disable_host_notifications() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	if(cmd==CMD_ENABLE_HOST_NOTIFICATIONS)
		enable_host_notifications(temp_host);
	else
		disable_host_notifications(temp_host);

#ifdef DEBUG0
	printf("cmd_enable_disable_host_notifications() end\n");
#endif
	return OK;
        }



/* enables/disables notifications for all services on a host  */
int cmd_enable_disable_host_service_notifications(int cmd,char *args){
	service *temp_service=NULL;
	host *temp_host=NULL;
	char *host_name="";

#ifdef DEBUG0
	printf("cmd_enable_disable_host_service_notifications() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* find all services for this host... */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		if(!strcmp(temp_service->host_name,host_name)){
			if(cmd==CMD_ENABLE_HOST_SVC_NOTIFICATIONS)
				enable_service_notifications(temp_service);
			else
				disable_service_notifications(temp_service);
		        }
	        }

#ifdef DEBUG0
	printf("cmd_enable_disable_host_service_notifications() end\n");
#endif
	return OK;
        }


/* enables/disables notifications for all hosts and services "beyond" a given host */
int cmd_enable_disable_and_propagate_notifications(int cmd,char *args){
	host *temp_host=NULL;
	char *host_name="";

#ifdef DEBUG0
	printf("cmd_enable_disable_and_propagate_notifications() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	if(cmd==CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST)
		enable_and_propagate_notifications(temp_host);
	else
		disable_and_propagate_notifications(temp_host);

#ifdef DEBUG0
	printf("cmd_enable_disable_and_propagate_notifications() end\n");
#endif
	return OK;
        }



/* processes results of an external service check */
int cmd_process_service_check_result(int cmd,time_t check_time,char *args){
	char *temp_ptr;
	passive_check_result *new_pcr;
	passive_check_result *temp_pcr;
	host *temp_host;

#ifdef DEBUG0
	printf("cmd_process_service_check_result() start\n");
#endif

	new_pcr=(passive_check_result *)malloc(sizeof(passive_check_result));
	if(new_pcr==NULL)
		return ERROR;

	/* get the host name */
	temp_ptr=my_strtok(args,";");

	/* if this isn't a host name, mabye its a host address */
	if(find_host(temp_ptr,NULL)==NULL){
		for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
			if(!strcmp(temp_ptr,temp_host->address)){
				temp_ptr=temp_host->name;
				break;
			        }
		        }
	        }

	if(temp_ptr==NULL){
		free(new_pcr);
		return ERROR;
	        }
	new_pcr->host_name=(char *)malloc(strlen(temp_ptr)+1);
	if(new_pcr->host_name==NULL){
		free(new_pcr);
		return ERROR;
	        }
	strcpy(new_pcr->host_name,temp_ptr);

	/* get the service description */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL){
		free(new_pcr->host_name);
		free(new_pcr);
		return ERROR;
	        }
	new_pcr->svc_description=(char *)malloc(strlen(temp_ptr)+1);
	if(new_pcr->svc_description==NULL){
		free(new_pcr->host_name);
		free(new_pcr);
		return ERROR;
	        }
	strcpy(new_pcr->svc_description,temp_ptr);

	/* get the service check return code */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL){
		free(new_pcr->svc_description);
		free(new_pcr->host_name);
		free(new_pcr);
		return ERROR;
	        }
	new_pcr->return_code=atoi(temp_ptr);

	/* make sure the return code is within bounds */
	if(new_pcr->return_code<-1 || new_pcr->return_code>2)
		new_pcr->return_code=STATE_UNKNOWN;

	/* get the plugin output */
	temp_ptr=my_strtok(NULL,"\n");
	if(temp_ptr==NULL){
		free(new_pcr->svc_description);
		free(new_pcr->host_name);
		free(new_pcr);
		return ERROR;
	        }
	new_pcr->output=(char *)malloc(strlen(temp_ptr)+1);
	if(new_pcr->output==NULL){
		free(new_pcr->svc_description);
		free(new_pcr->host_name);
		free(new_pcr);
		return ERROR;
	        }
	strcpy(new_pcr->output,temp_ptr);

	new_pcr->check_time=check_time;

	new_pcr->next=NULL;

	/* add the passive check result to the end of the list in memory */
	if(passive_check_result_list==NULL)
		passive_check_result_list=new_pcr;
	else{
		for(temp_pcr=passive_check_result_list;temp_pcr->next!=NULL;temp_pcr=temp_pcr->next);
		temp_pcr->next=new_pcr;
	        }

#ifdef DEBUG0
	printf("cmd_process_service_check_result() end\n");
#endif
	return OK;
        }


/* acknowledges a host or service problem */
int cmd_acknowledge_problem(int cmd,char *args){
	service *temp_service=NULL;
	host *temp_host=NULL;
	char *host_name="";
	char *svc_description="";
	char *ack_data;
	char *temp_ptr;
	int notify=TRUE;

#ifdef DEBUG0
	printf("cmd_acknowledge_problem() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* this is a service acknowledgement */
	if(cmd==CMD_ACKNOWLEDGE_SVC_PROBLEM){

		/* get the service name */
		svc_description=my_strtok(NULL,";");
		if(svc_description==NULL)
			return ERROR;

		/* verify that the service is valid */
		temp_service=find_service(temp_host->name,svc_description,NULL);
		if(temp_service==NULL)
			return ERROR;
	        }

	/* get the notification option */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	notify=(atoi(temp_ptr)>0)?TRUE:FALSE;

	/* get the acknowledgement data */
	ack_data=my_strtok(NULL,"\n");
	if(ack_data==NULL)
		return ERROR;

	/* acknowledge the host problem */
	if(cmd==CMD_ACKNOWLEDGE_HOST_PROBLEM)
		acknowledge_host_problem(temp_host,ack_data,notify);

	/* acknowledge the service problem */
	else
		acknowledge_service_problem(temp_service,ack_data,notify);


#ifdef DEBUG0
	printf("cmd_acknowledge_problem() end\n");
#endif
	return OK;
        }



/* removes a host or service acknowledgement */
int cmd_remove_acknowledgement(int cmd,char *args){
	service *temp_service=NULL;
	host *temp_host=NULL;
	char *host_name="";
	char *svc_description="";

#ifdef DEBUG0
	printf("cmd_remove_acknowledgement() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* we are removing a service acknowledgement */
	if(cmd==CMD_REMOVE_SVC_ACKNOWLEDGEMENT){

		/* get the service name */
		svc_description=my_strtok(NULL,";");
		if(svc_description==NULL)
			return ERROR;

		/* verify that the service is valid */
		temp_service=find_service(temp_host->name,svc_description,NULL);
		if(temp_service==NULL)
			return ERROR;
	        }

	/* acknowledge the host problem */
	if(cmd==CMD_REMOVE_HOST_ACKNOWLEDGEMENT)
		remove_host_acknowledgement(temp_host);

	/* acknowledge the service problem */
	else
		remove_service_acknowledgement(temp_service);

#ifdef DEBUG0
	printf("cmd_remove_acknowledgement() end\n");
#endif
	return OK;
        }



/* starts or stops executing service checks */
int cmd_start_stop_executing_service_checks(int cmd){

#ifdef DEBUG0
	printf("cmd_start_stop_executing_service_checks() start\n");
#endif

	if(cmd==CMD_START_EXECUTING_SVC_CHECKS)
		start_executing_service_checks();
	else
		stop_executing_service_checks();

#ifdef DEBUG0
	printf("cmd_start_stop_executing_service_checks() end\n");
#endif

	return OK;
        }



/* starts or stops accepting passive service checks */
int cmd_start_stop_accepting_passive_checks(int cmd){

#ifdef DEBUG0
	printf("cmd_start_stop_accepting_passive_checks() start\n");
#endif

	if(cmd==CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS)
		start_accepting_passive_service_checks();
	else
		stop_accepting_passive_service_checks();

#ifdef DEBUG0
	printf("cmd_start_stop_accepting_passive_checks() end\n");
#endif

	return OK;
        }


/* enables/disables passive checks for a particular service */
int cmd_enable_disable_passive_service_checks(int cmd,char *args){
	service *temp_service=NULL;
	char *host_name="";
	char *svc_description="";

#ifdef DEBUG0
	printf("cmd_enable_disable__passive_service_checks() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* get the service description */
	svc_description=my_strtok(NULL,";");
	if(svc_description==NULL)
		return ERROR;

	/* verify that the service is valid */
	temp_service=find_service(host_name,svc_description,NULL);
	if(temp_service==NULL)
		return ERROR;

	/* enable or disable passive checks for this service */
	if(cmd==CMD_ENABLE_PASSIVE_SVC_CHECKS)
		enable_passive_service_checks(temp_service);
	else
		disable_passive_service_checks(temp_service);

#ifdef DEBUG0
	printf("cmd_enable_disable__passive_service_checks() end\n");
#endif

	return OK;
        }


/* enables/disables the event handler for a particular service */
int cmd_enable_disable_service_event_handler(int cmd,char *args){
	host *temp_host=NULL;
	service *temp_service=NULL;
	char *host_name="";
	char *svc_description="";

#ifdef DEBUG0
	printf("cmd_enable_disable_service_event_handler() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* get the service description */
	svc_description=my_strtok(NULL,"\n");
	if(svc_description==NULL)
		return ERROR;

	/* verify that the service is valid */
	temp_service=find_service(host_name,svc_description,NULL);
	if(temp_service==NULL)
		return ERROR;

	/* enable or disable the host event handler */
	if(cmd==CMD_ENABLE_SVC_EVENT_HANDLER)
		enable_service_event_handler(temp_service);
	else
		disable_service_event_handler(temp_service);

#ifdef DEBUG0
	printf("cmd_enable_disable_service_event_handler() end\n");
#endif

	return OK;
        }
        


/* enables/disables the event handler for a particular host */
int cmd_enable_disable_host_event_handler(int cmd,char *args){
	host *temp_host=NULL;
	char *host_name="";

#ifdef DEBUG0
	printf("cmd_enable_disable_host_event_handler() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* enable or disable the host event handler */
	if(cmd==CMD_ENABLE_HOST_EVENT_HANDLER)
		enable_host_event_handler(temp_host);
	else
		disable_host_event_handler(temp_host);

#ifdef DEBUG0
	printf("cmd_enable_disable_host_event_handler() end\n");
#endif

	return OK;
        }
        

/* enables/disables checks of a particular host */
int cmd_enable_disable_host_check(int cmd,char *args){
	host *temp_host=NULL;
	char *host_name="";

#ifdef DEBUG0
	printf("cmd_enable_disable_host_check() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* enable or disable the host check */
	if(cmd==CMD_ENABLE_HOST_CHECK)
		enable_host_check(temp_host);
	else
		disable_host_check(temp_host);

#ifdef DEBUG0
	printf("cmd_enable_disable_host_check() end\n");
#endif

	return OK;
        }



/* start or stop obsessing over service checks */
int cmd_start_stop_obsessing_over_service_checks(int cmd){

#ifdef DEBUG0
	printf("cmd_start_stop_obsessing_over_service_checks() start\n");
#endif

	if(cmd==CMD_START_OBSESSING_OVER_SVC_CHECKS)
		start_obsessing_over_service_checks();
	else
		stop_obsessing_over_service_checks();

#ifdef DEBUG0
	printf("cmd_start_stop_obsessing_over_service_checks() end\n");
#endif

	return OK;
        }


/* enables/disables flap detection for a particular host */
int cmd_enable_disable_host_flap_detection(int cmd,char *args){
	host *temp_host=NULL;
	char *host_name="";

#ifdef DEBUG0
	printf("cmd_enable_disable_host_flap_detection() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* enable or disable the host check */
	if(cmd==CMD_ENABLE_HOST_FLAP_DETECTION)
		enable_host_flap_detection(temp_host);
	else
		disable_host_flap_detection(temp_host);

#ifdef DEBUG0
	printf("cmd_enable_disable_host_flap_detection() end\n");
#endif

	return OK;
        }


/* enables/disables flap detection for a particular service */
int cmd_enable_disable_service_flap_detection(int cmd,char *args){
	host *temp_host=NULL;
	service *temp_service=NULL;
	char *host_name="";
	char *svc_description="";

#ifdef DEBUG0
	printf("cmd_enable_disable_service_flap_detection() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* get the service description */
	svc_description=my_strtok(NULL,"\n");
	if(svc_description==NULL)
		return ERROR;

	/* verify that the service is valid */
	temp_service=find_service(host_name,svc_description,NULL);
	if(temp_service==NULL)
		return ERROR;

	/* enable or disable the host event handler */
	if(cmd==CMD_ENABLE_SVC_FLAP_DETECTION)
		enable_service_flap_detection(temp_service);
	else
		disable_service_flap_detection(temp_service);

#ifdef DEBUG0
	printf("cmd_enable_disable_service_flap_detection() end\n");
#endif

	return OK;
        }
        


/* schedules downtime for a specific host or service */
int cmd_schedule_downtime(int cmd, time_t entry_time, char *args){
	service *temp_service=NULL;
	host *temp_host=NULL;
	char *host_name="";
	char *svc_description="";
	char *temp_ptr;
	time_t start_time;
	time_t end_time;
	int fixed;
	unsigned long duration;
	char *author="";
	char *comment="";

#ifdef DEBUG0
	printf("cmd_schedule_downtime() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* this is a service downtime */
	if(cmd==CMD_SCHEDULE_SVC_DOWNTIME){

		/* get the service name */
		svc_description=my_strtok(NULL,";");
		if(svc_description==NULL)
			return ERROR;

		/* verify that the service is valid */
		temp_service=find_service(temp_host->name,svc_description,NULL);
		if(temp_service==NULL)
			return ERROR;
	        }

	/* get the start time */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	start_time=(time_t)strtoul(temp_ptr,NULL,10);

	/* get the end time */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	end_time=(time_t)strtoul(temp_ptr,NULL,10);

	/* get the fixed flag */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	fixed=atoi(temp_ptr);

	/* get the duration */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	duration=strtoul(temp_ptr,NULL,10);

	/* get the author */
	author=my_strtok(NULL,";");
	if(author==NULL)
		return ERROR;

	/* get the comment */
	comment=my_strtok(NULL,";");
	if(comment==NULL)
		return ERROR;

	/* duration should be auto-calculated, not user-specified */
	if(fixed>0)
		duration=(unsigned long)(end_time-start_time);

	/* schedule downtime */
	if(cmd==CMD_SCHEDULE_HOST_DOWNTIME)
		schedule_downtime(HOST_DOWNTIME,host_name,NULL,entry_time,author,comment,start_time,end_time,fixed,duration);
	else
		schedule_downtime(SERVICE_DOWNTIME,host_name,svc_description,entry_time,author,comment,start_time,end_time,fixed,duration);

#ifdef DEBUG0
	printf("cmd_schedule_downtime() end\n");
#endif
	return OK;
        }



/* deletes scheduled host or service downtime */
int cmd_delete_downtime(int cmd, char *args){
	int downtime_id;
	char *temp_ptr;

#ifdef DEBUG0
	printf("cmd_delete_downtime() start\n");
#endif

	/* get the id of the downtime to delete */
	temp_ptr=my_strtok(args,"\n");
	if(temp_ptr==NULL)
		return ERROR;
	downtime_id=atoi(temp_ptr);

	if(cmd==CMD_DEL_HOST_DOWNTIME)
		unschedule_downtime(HOST_DOWNTIME,downtime_id);
	else
		unschedule_downtime(SERVICE_DOWNTIME,downtime_id);

#ifdef DEBUG0
	printf("cmd_delete_downtime() end\n");
#endif

	return OK;
        }

        

/* schedules downtime for all services on a specific host */
int cmd_schedule_host_service_downtime(int cmd, time_t entry_time, char *args){
	service *temp_service=NULL;
	host *temp_host=NULL;
	char *host_name="";
	char *temp_ptr;
	time_t start_time;
	time_t end_time;
	int fixed;
	unsigned long duration;
	char *author="";
	char *comment="";

#ifdef DEBUG0
	printf("cmd_schedule_host_service_downtime() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name,NULL);
	if(temp_host==NULL)
		return ERROR;

	/* get the start time */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	start_time=(time_t)strtoul(temp_ptr,NULL,10);

	/* get the end time */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	end_time=(time_t)strtoul(temp_ptr,NULL,10);

	/* get the fixed flag */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	fixed=atoi(temp_ptr);

	/* get the duration */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	duration=strtoul(temp_ptr,NULL,10);

	/* get the author */
	author=my_strtok(NULL,";");
	if(author==NULL)
		return ERROR;

	/* get the comment */
	comment=my_strtok(NULL,";");
	if(comment==NULL)
		return ERROR;

	/* duration should be auto-calculated, not user-specified */
	if(fixed>0)
		duration=(unsigned long)(end_time-start_time);

	/* schedule downtime */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
		if(!strcmp(temp_service->host_name,host_name))
			schedule_downtime(SERVICE_DOWNTIME,host_name,temp_service->description,entry_time,author,comment,start_time,end_time,fixed,duration);
	        }

#ifdef DEBUG0
	printf("cmd_schedule_host_service_downtime() end\n");
#endif
	return OK;
        }




/******************************************************************/
/*************** INTERNAL COMMAND IMPLEMENTATIONS  ****************/
/******************************************************************/

/* temporarily disables a service check */
void disable_service_check(service *svc){

#ifdef DEBUG0
	printf("disable_service_check() start\n");
#endif
	/* disable the service check... */
	svc->checks_enabled=FALSE;

	/* update the status log to reflect the new service state */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("disable_service_check() end\n");
#endif

	return;
        }



/* enables a service check */
void enable_service_check(service *svc){

#ifdef DEBUG0
	printf("enable_service_check() start\n");
#endif

	/* enable the service check... */
	svc->checks_enabled=TRUE;

	/* update the status log to reflect the new service state */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("enable_service_check() end\n");
#endif

	return;
        }



/* schedules an immediate or delayed service check */
void schedule_service_check(service *svc,time_t check_time,int forced){
	timed_event *temp_event;
	timed_event *new_event;
	int found=FALSE;
	char temp_buffer[MAX_INPUT_BUFFER];
	int use_original_event=TRUE;

#ifdef DEBUG0
	printf("schedule_service_check() start\n");
#endif

	/* allocate memory for a new event item */
	new_event=(timed_event *)malloc(sizeof(timed_event));
	if(new_event==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Could not reschedule check of service '%s' on host '%s'!\n",svc->description,svc->host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);

		return;
	        }

	/* see if there are any other scheduled checks of this service in the queue */
	for(temp_event=event_list_low;temp_event!=NULL;temp_event=temp_event->next){

		if(temp_event->event_type==EVENT_SERVICE_CHECK && svc==(service *)temp_event->event_data){
			found=TRUE;
			break;
		        }
	        }

	/* we found another service check event for this service in the queue - what should we do? */
	if(found==TRUE && temp_event!=NULL){

		/* use the originally scheduled check unless we decide otherwise */
		use_original_event=TRUE;

		/* the original event is a forced check... */
		if(svc->check_options & CHECK_OPTION_FORCE_EXECUTION){
			
			/* the new event is also forced and its execution time is earlier than the original, so use it instead */
			if(forced==TRUE && check_time < svc->next_check)
				use_original_event=FALSE;
		        }

		/* the original event is not a forced check... */
		else{

			/* the new event is a forced check, so use it instead */
			if(forced==TRUE)
				use_original_event=FALSE;

			/* the new event is not forced either and its execution time is earlier than the original, so use it instead */
			else if(check_time < svc->next_check)
				use_original_event=FALSE;
		        }

		/* the originally queued event won the battle, so keep it and exit */
		if(use_original_event==TRUE){
			free(new_event);
			return;
		        }

		remove_event(temp_event,&event_list_low);
		free(temp_event);
	        }

	/* set the next service check time */
	svc->next_check=check_time;

	/* set the force service check option */
	if(forced==TRUE)
		svc->check_options|=CHECK_OPTION_FORCE_EXECUTION;

	/* place the new event in the event queue */
	new_event->event_type=EVENT_SERVICE_CHECK;
	new_event->event_data=(void *)svc;
	new_event->run_time=svc->next_check;
	new_event->recurring=FALSE;
	schedule_event(new_event,&event_list_low);

	/* update the status log */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("schedule_service_check() end\n");
#endif

	return;
        }


/* enable notifications on a program-wide basis */
void enable_all_notifications(void){

#ifdef DEBUG0
	printf("enable_all_notifications() start\n");
#endif

	/* bail out if we're already set... */
	if(enable_notifications==TRUE)
		return;

	/* update notification status */
	enable_notifications=TRUE;

	/* update the status log */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("enable_all_notifications() end\n");
#endif

	return;
        }


/* disable notifications on a program-wide basis */
void disable_all_notifications(void){

#ifdef DEBUG0
	printf("disable_all_notifications() start\n");
#endif

	/* bail out if we're already set... */
	if(enable_notifications==FALSE)
		return;

	/* update notification status */
	enable_notifications=FALSE;

	/* update the status log */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("disable_all_notifications() end\n");
#endif

	return;
        }


/* enables notifications for a service */
void enable_service_notifications(service *svc){

#ifdef DEBUG0
	printf("enable_service_notifications() start\n");
#endif

	/* enable the service notifications... */
	svc->notifications_enabled=TRUE;

	/* update the status log to reflect the new service state */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("enable_service_notifications() end\n");
#endif

	return;
        }


/* disables notifications for a service */
void disable_service_notifications(service *svc){

#ifdef DEBUG0
	printf("disable_service_notifications() start\n");
#endif

	/* disable the service notifications... */
	svc->notifications_enabled=FALSE;

	/* update the status log to reflect the new service state */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("disable_service_notifications() end\n");
#endif

	return;
        }


/* enables notifications for a host */
void enable_host_notifications(host *hst){

#ifdef DEBUG0
	printf("enable_host_notifications() start\n");
#endif

	/* enable the host notifications... */
	hst->notifications_enabled=TRUE;

	/* update the status log to reflect the new host state */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("enable_host_notifications() end\n");
#endif

	return;
        }


/* disables notifications for a host */
void disable_host_notifications(host *hst){

#ifdef DEBUG0
	printf("disable_host_notifications() start\n");
#endif

	/* disable the host notifications... */
	hst->notifications_enabled=FALSE;

	/* update the status log to reflect the new host state */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("disable_host_notifications() end\n");
#endif

	return;
        }


/* enables notifications for all hosts and services "beyond" a given host */
void enable_and_propagate_notifications(host *hst){
	host *temp_host;
	service *temp_service;

#ifdef DEBUG0
	printf("enable_and_propagate_notifications() start\n");
#endif

	/* check all child hosts... */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		if(is_host_immediate_child_of_host(hst,temp_host)==TRUE){

			/* recurse... */
			enable_and_propagate_notifications(temp_host);

			/* enable notifications for this host */
			enable_host_notifications(temp_host);

			/* enable notifications for all services on this host... */
			for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
				if(!strcmp(temp_host->name,temp_service->host_name))
					enable_service_notifications(temp_service);
			        }
		        }

	        }

#ifdef DEBUG0
	printf("enable_and_propagate_notifications() end\n");
#endif

	return;
        }


/* disables notifications for all hosts and services "beyond" a given host */
void disable_and_propagate_notifications(host *hst){
	host *temp_host;
	service *temp_service;

#ifdef DEBUG0
	printf("disable_and_propagate_notifications() start\n");
#endif

	/* check all child hosts... */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		if(is_host_immediate_child_of_host(hst,temp_host)==TRUE){

			/* recurse... */
			disable_and_propagate_notifications(temp_host);

			/* disable notifications for this host */
			disable_host_notifications(temp_host);

			/* disable notifications for all services on this host... */
			for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
				if(!strcmp(temp_host->name,temp_service->host_name))
					disable_service_notifications(temp_service);
			        }
		        }

	        }

#ifdef DEBUG0
	printf("disable_and_propagate_notifications() end\n");
#endif

	return;
        }


/* acknowledges a host problem */
void acknowledge_host_problem(host *hst, char *ack_data, int notify){

#ifdef DEBUG0
	printf("acknowledge_host_problem() start\n");
#endif

	/* send out an acknowledgement notification */
	if(notify==TRUE)
		host_notification(hst,hst->status,ack_data);

	/* set the acknowledgement flag */
	hst->problem_has_been_acknowledged=TRUE;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("acknowledge_host_problem() end\n");
#endif

	return;
        }


/* acknowledges a service problem */
void acknowledge_service_problem(service *svc, char *ack_data, int notify){

#ifdef DEBUG0
	printf("acknowledge_service_problem() start\n");
#endif

	/* send out an acknowledgement notification */
	if(notify==TRUE)
		service_notification(svc,ack_data);

	/* set the acknowledgement flag */
	svc->problem_has_been_acknowledged=TRUE;

	/* update the status log with the service info */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("acknowledge_service_problem() end\n");
#endif

	return;
        }


/* removes a host acknowledgement */
void remove_host_acknowledgement(host *hst){

#ifdef DEBUG0
	printf("remove_host_acknowledgement() start\n");
#endif

	/* set the acknowledgement flag */
	hst->problem_has_been_acknowledged=FALSE;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("remove_host_acknowledgement() end\n");
#endif

	return;
        }


/* removes a service acknowledgement */
void remove_service_acknowledgement(service *svc){

#ifdef DEBUG0
	printf("remove_service_acknowledgement() start\n");
#endif

	/* set the acknowledgement flag */
	svc->problem_has_been_acknowledged=FALSE;

	/* update the status log with the service info */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("remove_service_acknowledgement() end\n");
#endif

	return;
        }


/* starts executing service checks */
void start_executing_service_checks(void){

#ifdef DEBUG0
	printf("start_executing_service_checks() start\n");
#endif

	/* bail out if we're already executing services */
	if(execute_service_checks==TRUE)
		return;

	/* set the service check execution flag */
	execute_service_checks=TRUE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("start_executing_service_checks() end\n");
#endif

	return;
        }




/* stops executing service checks */
void stop_executing_service_checks(void){

#ifdef DEBUG0
	printf("stop_executing_service_checks() start\n");
#endif

	/* bail out if we're already not executing services */
	if(execute_service_checks==FALSE)
		return;

	/* set the service check execution flag */
	execute_service_checks=FALSE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("stop_executing_service_checks() end\n");
#endif

	return;
        }



/* starts accepting passive service checks */
void start_accepting_passive_service_checks(void){

#ifdef DEBUG0
	printf("start_accepting_passive_service_checks() start\n");
#endif

	/* bail out if we're already accepting passive services */
	if(accept_passive_service_checks==TRUE)
		return;

	/* set the service check flag */
	accept_passive_service_checks=TRUE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("start_accepting_passive_service_checks() end\n");
#endif

	return;
        }



/* stops accepting passive service checks */
void stop_accepting_passive_service_checks(void){

#ifdef DEBUG0
	printf("stop_accepting_passive_service_checks() start\n");
#endif

	/* bail out if we're already not accepting passive services */
	if(accept_passive_service_checks==FALSE)
		return;

	/* set the service check flag */
	accept_passive_service_checks=FALSE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("stop_accepting_passive_service_checks() end\n");
#endif

	return;
        }



/* enables passive service checks for a particular service */
void enable_passive_service_checks(service *svc){

#ifdef DEBUG0
	printf("enable_passive_service_checks() start\n");
#endif

	/* set the passive check flag */
	svc->accept_passive_service_checks=TRUE;

	/* update the status log with the service info */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("enable_passive_service_checks() end\n");
#endif

	return;
        }



/* disables passive service checks for a particular service */
void disable_passive_service_checks(service *svc){

#ifdef DEBUG0
	printf("disable_passive_service_checks() start\n");
#endif

	/* set the passive check flag */
	svc->accept_passive_service_checks=FALSE;

	/* update the status log with the service info */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("disable_passive_service_checks() end\n");
#endif

	return;
        }


/* enables event handlers on a program-wide basis */
void start_using_event_handlers(void){

#ifdef DEBUG0
	printf("start_using_event_handlers() start\n");
#endif

	/* set the event handler flag */
	enable_event_handlers=TRUE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("start_using_event_handlers() end\n");
#endif

	return;
        }


/* disables event handlers on a program-wide basis */
void stop_using_event_handlers(void){

#ifdef DEBUG0
	printf("stop_using_event_handlers() start\n");
#endif

	/* set the event handler flag */
	enable_event_handlers=FALSE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("stop_using_event_handlers() end\n");
#endif

	return;
        }


/* enables the event handler for a particular service */
void enable_service_event_handler(service *svc){

#ifdef DEBUG0
	printf("enable_service_event_handler() start\n");
#endif

	/* set the event handler flag */
	svc->event_handler_enabled=TRUE;

	/* update the status log with the service info */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("enable_service_event_handler() end\n");
#endif

	return;
        }



/* disables the event handler for a particular service */
void disable_service_event_handler(service *svc){

#ifdef DEBUG0
	printf("disable_service_event_handler() start\n");
#endif

	/* set the event handler flag */
	svc->event_handler_enabled=FALSE;

	/* update the status log with the service info */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("disable_service_event_handler() end\n");
#endif

	return;
        }


/* enables the event handler for a particular host */
void enable_host_event_handler(host *hst){

#ifdef DEBUG0
	printf("enable_host_event_handler() start\n");
#endif

	/* set the event handler flag */
	hst->event_handler_enabled=TRUE;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("enable_host_event_handler() end\n");
#endif

	return;
        }


/* disables the event handler for a particular host */
void disable_host_event_handler(host *hst){

#ifdef DEBUG0
	printf("disable_host_event_handler() start\n");
#endif

	/* set the event handler flag */
	hst->event_handler_enabled=FALSE;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("disable_host_event_handler() end\n");
#endif

	return;
        }


/* enables checks of a particular host */
void enable_host_check(host *hst){

#ifdef DEBUG0
	printf("enable_host_check() start\n");
#endif

	/* set the host check flag */
	hst->checks_enabled=TRUE;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("enable_host_check() end\n");
#endif

	return;
        }


/* disables checks of a particular host */
void disable_host_check(host *hst){

#ifdef DEBUG0
	printf("disable_host_check() start\n");
#endif

	/* set the host check flag */
	hst->checks_enabled=FALSE;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("disable_host_check() end\n");
#endif

	return;
        }



/* start obsessing over service check results */
void start_obsessing_over_service_checks(void){

#ifdef DEBUG0
        printf("start_obsessing_over_service_checks() start\n");
#endif

	/* set the service obsession flag */
	obsess_over_services=TRUE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("start_obsessing_over_service_checks() end\n");
#endif

	return;
        }



/* stop obsessing over service check results */
void stop_obsessing_over_service_checks(void){

#ifdef DEBUG0
        printf("stop_obsessing_over_service_checks() start\n");
#endif

	/* set the service obsession flag */
	obsess_over_services=FALSE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("stop_obsessing_over_service_checks() end\n");
#endif

	return;
        }



/* enable failure prediction on a program-wide basis */
void enable_all_failure_prediction(void){

#ifdef DEBUG0
	printf("enable_all_failure_prediction() start\n");
#endif

	/* bail out if we're already set... */
	if(enable_failure_prediction==TRUE)
		return;

	enable_failure_prediction=TRUE;

	/* update the status log */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("enable_all_failure_prediction() end\n");
#endif

	return;
        }


/* disable failure prediction on a program-wide basis */
void disable_all_failure_prediction(void){

#ifdef DEBUG0
	printf("disable_all_failure_prediction() start\n");
#endif

	/* bail out if we're already set... */
	if(enable_failure_prediction==FALSE)
		return;

	enable_failure_prediction=FALSE;

	/* update the status log */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("enable_all_failure_prediction() end\n");
#endif

	return;
        }


/* enable performance data on a program-wide basis */
void enable_performance_data(void){

#ifdef DEBUG0
	printf("enable_performance_data() start\n");
#endif

	/* bail out if we're already set... */
	if(process_performance_data==TRUE)
		return;

	process_performance_data=TRUE;

	/* update the status log */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("enable_performance_data() end\n");
#endif

	return;
        }


/* disable performance data on a program-wide basis */
void disable_performance_data(void){

#ifdef DEBUG0
	printf("disable_performance_data() start\n");
#endif

	/* bail out if we're already set... */
	if(process_performance_data==FALSE)
		return;

	process_performance_data=FALSE;

	/* update the status log */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("disable_performance_data() end\n");
#endif

	return;
        }



/* process all passive service checks found in a given file */
void process_passive_service_checks(void){
	service_message svc_msg;
	pid_t pid;
	passive_check_result *temp_pcr;
	passive_check_result *this_pcr;
	passive_check_result *next_pcr;


#ifdef DEBUG0
	printf("process_passive_service_checks() start\n");
#endif

	/* fork... */
	pid=fork();

	/* an error occurred while trying to fork */
	if(pid==-1)
		return;

	/* if we are in the child process... */
	if(pid==0){

		/* become the process group leader */
		setpgid(0,0);

		/* free allocated memory */
		free_memory();

		/* close read end of IPC pipe */
		close(ipc_pipe[0]);

		/* reset signal handling */
		reset_sighandler();

		/* fork again... */
		pid=fork();

		/* the grandchild process should submit the service check result... */
		if(pid==0){

			/* write all service checks to the IPC pipe for later processing by the grandparent */
			for(temp_pcr=passive_check_result_list;temp_pcr!=NULL;temp_pcr=temp_pcr->next){

				strncpy(svc_msg.host_name,temp_pcr->host_name,sizeof(svc_msg.host_name)-1);
				svc_msg.host_name[sizeof(svc_msg.host_name)-1]='\x0';
				strncpy(svc_msg.description,temp_pcr->svc_description,sizeof(svc_msg.description)-1);
				svc_msg.description[sizeof(svc_msg.description)-1]='\x0';
				snprintf(svc_msg.output,sizeof(svc_msg.output)-1,"%s",temp_pcr->output);
				svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
				svc_msg.return_code=temp_pcr->return_code;
				svc_msg.parallelized=FALSE;
				svc_msg.exited_ok=TRUE;
				svc_msg.check_type=SERVICE_CHECK_PASSIVE;
				svc_msg.check_time=temp_pcr->check_time;
				svc_msg.finish_time=time(NULL);

				/* write the service check results... */
				write_svc_message(&svc_msg);
	                        }
		        }

		/* free memory for the passive service check result list */
		this_pcr=passive_check_result_list;
		while(this_pcr!=NULL){
			next_pcr=this_pcr->next;
			free(this_pcr);
			this_pcr=next_pcr;
	                }

		exit(OK);
	        }

	/* else the parent should wait for the first child to return... */
	else if(pid>0)
		waitpid(pid,NULL,0);

	/* free memory for the passive service check result list */
	this_pcr=passive_check_result_list;
	while(this_pcr!=NULL){
		next_pcr=this_pcr->next;
		free(this_pcr);
		this_pcr=next_pcr;
	        }
	passive_check_result_list=NULL;

#ifdef DEBUG0
	printf("process_passive_service_checks() end\n");
#endif

	return;
        }

