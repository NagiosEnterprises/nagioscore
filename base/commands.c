/*****************************************************************************
 *
 * COMMANDS.C - External command functions for Nagios
 *
 * Copyright (c) 1999-2007 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   10-20-2007
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
#include "../include/comments.h"
#include "../include/downtime.h"
#include "../include/statusdata.h"
#include "../include/perfdata.h"
#include "../include/sretention.h"
#include "../include/broker.h"
#include "../include/nagios.h"

extern char     *config_file;
extern char	*log_file;
extern char     *command_file;
extern char     *temp_file;

extern int      sigshutdown;
extern int      sigrestart;

extern int      check_external_commands;

extern int      ipc_pipe[2];

extern time_t   last_command_check;
extern time_t   last_command_status_update;

extern int      command_check_interval;

extern int      enable_notifications;
extern int      execute_service_checks;
extern int      accept_passive_service_checks;
extern int      execute_host_checks;
extern int      accept_passive_host_checks;
extern int      enable_event_handlers;
extern int      obsess_over_services;
extern int      obsess_over_hosts;
extern int      check_service_freshness;
extern int      check_host_freshness;
extern int      enable_failure_prediction;
extern int      process_performance_data;

extern int      log_external_commands;
extern int      log_passive_checks;

extern unsigned long    modified_host_process_attributes;
extern unsigned long    modified_service_process_attributes;

extern char     *global_host_event_handler;
extern char     *global_service_event_handler;

extern timed_event      *event_list_high;
extern timed_event      *event_list_low;

extern host     *host_list;
extern service  *service_list;

extern FILE     *command_file_fp;
extern int      command_file_fd;

passive_check_result    *passive_check_result_list=NULL;
passive_check_result    *passive_check_result_list_tail=NULL;

extern pthread_t       worker_threads[TOTAL_WORKER_THREADS];
extern circular_buffer external_command_buffer;
extern int             external_command_buffer_slots;



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
	int update_status=FALSE;

#ifdef DEBUG0
	printf("check_for_external_commands() start\n");
#endif


	/* bail out if we shouldn't be checking for external commands */
	if(check_external_commands==FALSE)
		return;

	/* update last command check time */
	last_command_check=time(NULL);

	/* update the status log with new program information */
	/* go easy on the frequency of this if we're checking often - only update program status every 10 seconds.... */
	if(last_command_check<(last_command_status_update+10))
		update_status=FALSE;
	else
		update_status=TRUE;
	if(update_status==TRUE){
		last_command_status_update=last_command_check;
		update_program_status(FALSE);
	        }

	/* reset passive check result list pointers */
	passive_check_result_list=NULL;
	passive_check_result_list_tail=NULL;

	/* process all commands found in the buffer */
	while(1){

		/* get a lock on the buffer */
		pthread_mutex_lock(&external_command_buffer.buffer_lock);

		/* if no items present, bail out */
		if(external_command_buffer.items<=0){
			pthread_mutex_unlock(&external_command_buffer.buffer_lock);
			break;
		        }

		if(external_command_buffer.buffer[external_command_buffer.tail]){
			strncpy(buffer,((char **)external_command_buffer.buffer)[external_command_buffer.tail],sizeof(buffer)-1);
			buffer[sizeof(buffer)-1]='\x0';
		        }
		else
			strcpy(buffer,"");

		/* free memory allocated for buffer slot */
		free(((char **)external_command_buffer.buffer)[external_command_buffer.tail]);

		/* adjust tail counter and number of items */
		external_command_buffer.tail=(external_command_buffer.tail + 1) % external_command_buffer_slots;
		external_command_buffer.items--;

		/* release the lock on the buffer */
		pthread_mutex_unlock(&external_command_buffer.buffer_lock);

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

		/* decide what type of command this is... */

		/**************************/
		/**** PROCESS COMMANDS ****/
		/**************************/

		if(!strcmp(command_id,"ENTER_STANDBY_MODE") || !strcmp(command_id,"DISABLE_NOTIFICATIONS"))
			command_type=CMD_DISABLE_NOTIFICATIONS;
		else if(!strcmp(command_id,"ENTER_ACTIVE_MODE") || !strcmp(command_id,"ENABLE_NOTIFICATIONS"))
			command_type=CMD_ENABLE_NOTIFICATIONS;

		else if(!strcmp(command_id,"SHUTDOWN_PROGRAM"))
			command_type=CMD_SHUTDOWN_PROCESS;
		else if(!strcmp(command_id,"RESTART_PROGRAM"))
			command_type=CMD_RESTART_PROCESS;

		else if(!strcmp(command_id,"SAVE_STATE_INFORMATION"))
			command_type=CMD_SAVE_STATE_INFORMATION;
		else if(!strcmp(command_id,"READ_STATE_INFORMATION"))
			command_type=CMD_READ_STATE_INFORMATION;

		else if(!strcmp(command_id,"ENABLE_EVENT_HANDLERS"))
			command_type=CMD_ENABLE_EVENT_HANDLERS;
		else if(!strcmp(command_id,"DISABLE_EVENT_HANDLERS"))
			command_type=CMD_DISABLE_EVENT_HANDLERS;

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

		else if(!strcmp(command_id,"START_EXECUTING_HOST_CHECKS"))
			command_type=CMD_START_EXECUTING_HOST_CHECKS;
		else if(!strcmp(command_id,"STOP_EXECUTING_HOST_CHECKS"))
			command_type=CMD_STOP_EXECUTING_HOST_CHECKS;

		else if(!strcmp(command_id,"START_EXECUTING_SVC_CHECKS"))
			command_type=CMD_START_EXECUTING_SVC_CHECKS;
		else if(!strcmp(command_id,"STOP_EXECUTING_SVC_CHECKS"))
			command_type=CMD_STOP_EXECUTING_SVC_CHECKS;

		else if(!strcmp(command_id,"START_ACCEPTING_PASSIVE_HOST_CHECKS"))
			command_type=CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS;
		else if(!strcmp(command_id,"STOP_ACCEPTING_PASSIVE_HOST_CHECKS"))
			command_type=CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS;

		else if(!strcmp(command_id,"START_ACCEPTING_PASSIVE_SVC_CHECKS"))
			command_type=CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS;
		else if(!strcmp(command_id,"STOP_ACCEPTING_PASSIVE_SVC_CHECKS"))
			command_type=CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS;

		else if(!strcmp(command_id,"START_OBSESSING_OVER_HOST_CHECKS"))
			command_type=CMD_START_OBSESSING_OVER_HOST_CHECKS;
		else if(!strcmp(command_id,"STOP_OBSESSING_OVER_HOST_CHECKS"))
			command_type=CMD_STOP_OBSESSING_OVER_HOST_CHECKS;

		else if(!strcmp(command_id,"START_OBSESSING_OVER_SVC_CHECKS"))
			command_type=CMD_START_OBSESSING_OVER_SVC_CHECKS;
		else if(!strcmp(command_id,"STOP_OBSESSING_OVER_SVC_CHECKS"))
			command_type=CMD_STOP_OBSESSING_OVER_SVC_CHECKS;

		else if(!strcmp(command_id,"ENABLE_FLAP_DETECTION"))
			command_type=CMD_ENABLE_FLAP_DETECTION;
		else if(!strcmp(command_id,"DISABLE_FLAP_DETECTION"))
			command_type=CMD_DISABLE_FLAP_DETECTION;

		else if(!strcmp(command_id,"CHANGE_GLOBAL_HOST_EVENT_HANDLER"))
			command_type=CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER;
		else if(!strcmp(command_id,"CHANGE_GLOBAL_SVC_EVENT_HANDLER"))
			command_type=CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER;

		else if(!strcmp(command_id,"ENABLE_SERVICE_FRESHNESS_CHECKS"))
			command_type=CMD_ENABLE_SERVICE_FRESHNESS_CHECKS;
		else if(!strcmp(command_id,"DISABLE_SERVICE_FRESHNESS_CHECKS"))
			command_type=CMD_DISABLE_SERVICE_FRESHNESS_CHECKS;

		else if(!strcmp(command_id,"ENABLE_HOST_FRESHNESS_CHECKS"))
			command_type=CMD_ENABLE_HOST_FRESHNESS_CHECKS;
		else if(!strcmp(command_id,"DISABLE_HOST_FRESHNESS_CHECKS"))
			command_type=CMD_DISABLE_HOST_FRESHNESS_CHECKS;


		/*******************************/
		/**** HOST-RELATED COMMANDS ****/
		/*******************************/

		else if(!strcmp(command_id,"ADD_HOST_COMMENT"))
			command_type=CMD_ADD_HOST_COMMENT;
		else if(!strcmp(command_id,"DEL_HOST_COMMENT"))
			command_type=CMD_DEL_HOST_COMMENT;
		else if(!strcmp(command_id,"DEL_ALL_HOST_COMMENTS"))
			command_type=CMD_DEL_ALL_HOST_COMMENTS;

		else if(!strcmp(command_id,"DELAY_HOST_NOTIFICATION"))
			command_type=CMD_DELAY_HOST_NOTIFICATION;

		else if(!strcmp(command_id,"ENABLE_HOST_NOTIFICATIONS"))
			command_type=CMD_ENABLE_HOST_NOTIFICATIONS;
		else if(!strcmp(command_id,"DISABLE_HOST_NOTIFICATIONS"))
			command_type=CMD_DISABLE_HOST_NOTIFICATIONS;

		else if(!strcmp(command_id,"ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST"))
			command_type=CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST;
		else if(!strcmp(command_id,"DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST"))
			command_type=CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST;

		else if(!strcmp(command_id,"ENABLE_HOST_AND_CHILD_NOTIFICATIONS"))
			command_type=CMD_ENABLE_HOST_AND_CHILD_NOTIFICATIONS;
		else if(!strcmp(command_id,"DISABLE_HOST_AND_CHILD_NOTIFICATIONS"))
			command_type=CMD_DISABLE_HOST_AND_CHILD_NOTIFICATIONS;

		else if(!strcmp(command_id,"ENABLE_HOST_SVC_NOTIFICATIONS"))
			command_type=CMD_ENABLE_HOST_SVC_NOTIFICATIONS;
		else if(!strcmp(command_id,"DISABLE_HOST_SVC_NOTIFICATIONS"))
			command_type=CMD_DISABLE_HOST_SVC_NOTIFICATIONS;

		else if(!strcmp(command_id,"ENABLE_HOST_SVC_CHECKS"))
			command_type=CMD_ENABLE_HOST_SVC_CHECKS;
		else if(!strcmp(command_id,"DISABLE_HOST_SVC_CHECKS"))
			command_type=CMD_DISABLE_HOST_SVC_CHECKS;

		else if(!strcmp(command_id,"ENABLE_PASSIVE_HOST_CHECKS"))
			command_type=CMD_ENABLE_PASSIVE_HOST_CHECKS;
		else if(!strcmp(command_id,"DISABLE_PASSIVE_HOST_CHECKS"))
			command_type=CMD_DISABLE_PASSIVE_HOST_CHECKS;

		else if(!strcmp(command_id,"SCHEDULE_HOST_SVC_CHECKS"))
			command_type=CMD_SCHEDULE_HOST_SVC_CHECKS;
		else if(!strcmp(command_id,"SCHEDULE_FORCED_HOST_SVC_CHECKS"))
			command_type=CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS;

		else if(!strcmp(command_id,"ACKNOWLEDGE_HOST_PROBLEM"))
			command_type=CMD_ACKNOWLEDGE_HOST_PROBLEM;
		else if(!strcmp(command_id,"REMOVE_HOST_ACKNOWLEDGEMENT"))
			command_type=CMD_REMOVE_HOST_ACKNOWLEDGEMENT;

		else if(!strcmp(command_id,"ENABLE_HOST_EVENT_HANDLER"))
			command_type=CMD_ENABLE_HOST_EVENT_HANDLER;
		else if(!strcmp(command_id,"DISABLE_HOST_EVENT_HANDLER"))
			command_type=CMD_DISABLE_HOST_EVENT_HANDLER;

		else if(!strcmp(command_id,"ENABLE_HOST_CHECK"))
			command_type=CMD_ENABLE_HOST_CHECK;
		else if(!strcmp(command_id,"DISABLE_HOST_CHECK"))
			command_type=CMD_DISABLE_HOST_CHECK;

		else if(!strcmp(command_id,"SCHEDULE_HOST_CHECK"))
			command_type=CMD_SCHEDULE_HOST_CHECK;
		else if(!strcmp(command_id,"SCHEDULE_FORCED_HOST_CHECK"))
			command_type=CMD_SCHEDULE_FORCED_HOST_CHECK;

		else if(!strcmp(command_id,"SCHEDULE_HOST_DOWNTIME"))
			command_type=CMD_SCHEDULE_HOST_DOWNTIME;
		else if(!strcmp(command_id,"SCHEDULE_HOST_SVC_DOWNTIME"))
			command_type=CMD_SCHEDULE_HOST_SVC_DOWNTIME;
		else if(!strcmp(command_id,"DEL_HOST_DOWNTIME"))
			command_type=CMD_DEL_HOST_DOWNTIME;

		else if(!strcmp(command_id,"ENABLE_HOST_FLAP_DETECTION"))
			command_type=CMD_ENABLE_HOST_FLAP_DETECTION;
		else if(!strcmp(command_id,"DISABLE_HOST_FLAP_DETECTION"))
			command_type=CMD_DISABLE_HOST_FLAP_DETECTION;

		else if(!strcmp(command_id,"START_OBSESSING_OVER_HOST"))
			command_type=CMD_START_OBSESSING_OVER_HOST;
		else if(!strcmp(command_id,"STOP_OBSESSING_OVER_HOST"))
			command_type=CMD_STOP_OBSESSING_OVER_HOST;

		else if(!strcmp(command_id,"CHANGE_HOST_EVENT_HANDLER"))
			command_type=CMD_CHANGE_HOST_EVENT_HANDLER;
		else if(!strcmp(command_id,"CHANGE_HOST_CHECK_COMMAND"))
			command_type=CMD_CHANGE_HOST_CHECK_COMMAND;

		else if(!strcmp(command_id,"CHANGE_NORMAL_HOST_CHECK_INTERVAL"))
			command_type=CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL;

		else if(!strcmp(command_id,"CHANGE_MAX_HOST_CHECK_ATTEMPTS"))
			command_type=CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS;

		else if(!strcmp(command_id,"SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME"))
			command_type=CMD_SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME;

		else if(!strcmp(command_id,"SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME"))
			command_type=CMD_SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME;

		else if(!strcmp(command_id,"SET_HOST_NOTIFICATION_NUMBER"))
			command_type=CMD_SET_HOST_NOTIFICATION_NUMBER;


		/************************************/
		/**** HOSTGROUP-RELATED COMMANDS ****/
		/************************************/

		else if(!strcmp(command_id,"ENABLE_HOSTGROUP_HOST_NOTIFICATIONS"))
			command_type=CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS;
		else if(!strcmp(command_id,"DISABLE_HOSTGROUP_HOST_NOTIFICATIONS"))
			command_type=CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS;

		else if(!strcmp(command_id,"ENABLE_HOSTGROUP_SVC_NOTIFICATIONS"))
			command_type=CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS;
		else if(!strcmp(command_id,"DISABLE_HOSTGROUP_SVC_NOTIFICATIONS"))
			command_type=CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS;

		else if(!strcmp(command_id,"ENABLE_HOSTGROUP_HOST_CHECKS"))
			command_type=CMD_ENABLE_HOSTGROUP_HOST_CHECKS;
		else if(!strcmp(command_id,"DISABLE_HOSTGROUP_HOST_CHECKS"))
			command_type=CMD_DISABLE_HOSTGROUP_HOST_CHECKS;

		else if(!strcmp(command_id,"ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS"))
			command_type=CMD_ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS;
		else if(!strcmp(command_id,"DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS"))
			command_type=CMD_DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS;

		else if(!strcmp(command_id,"ENABLE_HOSTGROUP_SVC_CHECKS"))
			command_type=CMD_ENABLE_HOSTGROUP_SVC_CHECKS;
		else if(!strcmp(command_id,"DISABLE_HOSTGROUP_SVC_CHECKS"))
			command_type=CMD_DISABLE_HOSTGROUP_SVC_CHECKS;

		else if(!strcmp(command_id,"ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS"))
			command_type=CMD_ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS;
		else if(!strcmp(command_id,"DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS"))
			command_type=CMD_DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS;

		else if(!strcmp(command_id,"SCHEDULE_HOSTGROUP_HOST_DOWNTIME"))
			command_type=CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME;
		else if(!strcmp(command_id,"SCHEDULE_HOSTGROUP_SVC_DOWNTIME"))
			command_type=CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME;


		/**********************************/
		/**** SERVICE-RELATED COMMANDS ****/
		/**********************************/

		else if(!strcmp(command_id,"ADD_SVC_COMMENT"))
			command_type=CMD_ADD_SVC_COMMENT;
		else if(!strcmp(command_id,"DEL_SVC_COMMENT"))
			command_type=CMD_DEL_SVC_COMMENT;
		else if(!strcmp(command_id,"DEL_ALL_SVC_COMMENTS"))
			command_type=CMD_DEL_ALL_SVC_COMMENTS;

		else if(!strcmp(command_id,"SCHEDULE_SVC_CHECK"))
			command_type=CMD_SCHEDULE_SVC_CHECK;
		else if(!strcmp(command_id,"SCHEDULE_FORCED_SVC_CHECK"))
			command_type=CMD_SCHEDULE_FORCED_SVC_CHECK;

		else if(!strcmp(command_id,"ENABLE_SVC_CHECK"))
			command_type=CMD_ENABLE_SVC_CHECK;
		else if(!strcmp(command_id,"DISABLE_SVC_CHECK"))
			command_type=CMD_DISABLE_SVC_CHECK;

		else if(!strcmp(command_id,"ENABLE_PASSIVE_SVC_CHECKS"))
			command_type=CMD_ENABLE_PASSIVE_SVC_CHECKS;
		else if(!strcmp(command_id,"DISABLE_PASSIVE_SVC_CHECKS"))
			command_type=CMD_DISABLE_PASSIVE_SVC_CHECKS;

		else if(!strcmp(command_id,"DELAY_SVC_NOTIFICATION"))
			command_type=CMD_DELAY_SVC_NOTIFICATION;
		else if(!strcmp(command_id,"ENABLE_SVC_NOTIFICATIONS"))
			command_type=CMD_ENABLE_SVC_NOTIFICATIONS;
		else if(!strcmp(command_id,"DISABLE_SVC_NOTIFICATIONS"))
			command_type=CMD_DISABLE_SVC_NOTIFICATIONS;

		else if(!strcmp(command_id,"PROCESS_SERVICE_CHECK_RESULT"))
			command_type=CMD_PROCESS_SERVICE_CHECK_RESULT;
		else if(!strcmp(command_id,"PROCESS_HOST_CHECK_RESULT"))
			command_type=CMD_PROCESS_HOST_CHECK_RESULT;

		else if(!strcmp(command_id,"ENABLE_SVC_EVENT_HANDLER"))
			command_type=CMD_ENABLE_SVC_EVENT_HANDLER;
		else if(!strcmp(command_id,"DISABLE_SVC_EVENT_HANDLER"))
			command_type=CMD_DISABLE_SVC_EVENT_HANDLER;

		else if(!strcmp(command_id,"ENABLE_SVC_FLAP_DETECTION"))
			command_type=CMD_ENABLE_SVC_FLAP_DETECTION;
		else if(!strcmp(command_id,"DISABLE_SVC_FLAP_DETECTION"))
			command_type=CMD_DISABLE_SVC_FLAP_DETECTION;

		else if(!strcmp(command_id,"SCHEDULE_SVC_DOWNTIME"))
			command_type=CMD_SCHEDULE_SVC_DOWNTIME;
		else if(!strcmp(command_id,"DEL_SVC_DOWNTIME"))
			command_type=CMD_DEL_SVC_DOWNTIME;

		else if(!strcmp(command_id,"ACKNOWLEDGE_SVC_PROBLEM"))
			command_type=CMD_ACKNOWLEDGE_SVC_PROBLEM;
		else if(!strcmp(command_id,"REMOVE_SVC_ACKNOWLEDGEMENT"))
			command_type=CMD_REMOVE_SVC_ACKNOWLEDGEMENT;

		else if(!strcmp(command_id,"START_OBSESSING_OVER_SVC"))
			command_type=CMD_START_OBSESSING_OVER_SVC;
		else if(!strcmp(command_id,"STOP_OBSESSING_OVER_SVC"))
			command_type=CMD_STOP_OBSESSING_OVER_SVC;

		else if(!strcmp(command_id,"CHANGE_SVC_EVENT_HANDLER"))
			command_type=CMD_CHANGE_SVC_EVENT_HANDLER;
		else if(!strcmp(command_id,"CHANGE_SVC_CHECK_COMMAND"))
			command_type=CMD_CHANGE_SVC_CHECK_COMMAND;

		else if(!strcmp(command_id,"CHANGE_NORMAL_SVC_CHECK_INTERVAL"))
			command_type=CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL;
		else if(!strcmp(command_id,"CHANGE_RETRY_SVC_CHECK_INTERVAL"))
			command_type=CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL;

		else if(!strcmp(command_id,"CHANGE_MAX_SVC_CHECK_ATTEMPTS"))
			command_type=CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS;

		else if(!strcmp(command_id,"SET_SVC_NOTIFICATION_NUMBER"))
			command_type=CMD_SET_SVC_NOTIFICATION_NUMBER;


		/***************************************/
		/**** SERVICEGROUP-RELATED COMMANDS ****/
		/***************************************/

		else if(!strcmp(command_id,"ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS"))
			command_type=CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS;
		else if(!strcmp(command_id,"DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS"))
			command_type=CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS;

		else if(!strcmp(command_id,"ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS"))
			command_type=CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS;
		else if(!strcmp(command_id,"DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS"))
			command_type=CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS;

		else if(!strcmp(command_id,"ENABLE_SERVICEGROUP_HOST_CHECKS"))
			command_type=CMD_ENABLE_SERVICEGROUP_HOST_CHECKS;
		else if(!strcmp(command_id,"DISABLE_SERVICEGROUP_HOST_CHECKS"))
			command_type=CMD_DISABLE_SERVICEGROUP_HOST_CHECKS;

		else if(!strcmp(command_id,"ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS"))
			command_type=CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS;
		else if(!strcmp(command_id,"DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS"))
			command_type=CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS;

		else if(!strcmp(command_id,"ENABLE_SERVICEGROUP_SVC_CHECKS"))
			command_type=CMD_ENABLE_SERVICEGROUP_SVC_CHECKS;
		else if(!strcmp(command_id,"DISABLE_SERVICEGROUP_SVC_CHECKS"))
			command_type=CMD_DISABLE_SERVICEGROUP_SVC_CHECKS;

		else if(!strcmp(command_id,"ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS"))
			command_type=CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS;
		else if(!strcmp(command_id,"DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS"))
			command_type=CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS;

		else if(!strcmp(command_id,"SCHEDULE_SERVICEGROUP_HOST_DOWNTIME"))
			command_type=CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME;
		else if(!strcmp(command_id,"SCHEDULE_SERVICEGROUP_SVC_DOWNTIME"))
			command_type=CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME;


		/**** UNKNOWN COMMAND ****/
		else{
			/* log the bad external command */
			snprintf(buffer,sizeof(buffer),"Warning: Unrecognized external command -> %s;%s\n",command_id,args);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_all_logs(buffer,NSLOG_EXTERNAL_COMMAND | NSLOG_RUNTIME_WARNING);

			continue;
		        }

		/* log the external command */
		snprintf(buffer,sizeof(buffer),"EXTERNAL COMMAND: %s;%s\n",command_id,args);
		buffer[sizeof(buffer)-1]='\x0';
		if(command_type==CMD_PROCESS_SERVICE_CHECK_RESULT || command_type==CMD_PROCESS_HOST_CHECK_RESULT){
			if(log_passive_checks==TRUE)
				write_to_all_logs(buffer,NSLOG_PASSIVE_CHECK);
		        }
		else{
			if(log_external_commands==TRUE)
				write_to_all_logs(buffer,NSLOG_EXTERNAL_COMMAND);
		        }

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_external_command(NEBTYPE_EXTERNALCOMMAND_START,NEBFLAG_NONE,NEBATTR_NONE,command_type,entry_time,command_id,args,NULL);
#endif

		/* process the command if its not a passive check */
		process_external_command(command_type,entry_time,args);

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_external_command(NEBTYPE_EXTERNALCOMMAND_END,NEBFLAG_NONE,NEBATTR_NONE,command_type,entry_time,command_id,args,NULL);
#endif

	        }

	/**** PROCESS ALL PASSIVE SERVICE CHECK RESULTS AT ONE TIME ****/
	if(passive_check_result_list!=NULL)
		process_passive_service_checks();


#ifdef DEBUG0
	printf("check_for_external_commands() end\n");
#endif

	return;
        }



/* top-level processor for a single external command */
void process_external_command(int cmd, time_t entry_time, char *args){

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

		/***************************/
		/***** SYSTEM COMMANDS *****/
		/***************************/

	case CMD_SHUTDOWN_PROCESS:
	case CMD_RESTART_PROCESS:
		cmd_signal_process(cmd,args);
		break;

	case CMD_SAVE_STATE_INFORMATION:
		save_state_information(config_file,FALSE);
		break;

	case CMD_READ_STATE_INFORMATION:
		read_initial_state_information(config_file);
		break;

	case CMD_ENABLE_NOTIFICATIONS:
		enable_all_notifications();
		break;

	case CMD_DISABLE_NOTIFICATIONS:
		disable_all_notifications();
		break;

	case CMD_START_EXECUTING_SVC_CHECKS:
		start_executing_service_checks();
		break;

	case CMD_STOP_EXECUTING_SVC_CHECKS:
		stop_executing_service_checks();
		break;

	case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
		start_accepting_passive_service_checks();
		break;

	case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
		stop_accepting_passive_service_checks();
		break;

	case CMD_START_OBSESSING_OVER_SVC_CHECKS:
		start_obsessing_over_service_checks();
		break;

	case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
		stop_obsessing_over_service_checks();
		break;

	case CMD_START_EXECUTING_HOST_CHECKS:
		start_executing_host_checks();
		break;

	case CMD_STOP_EXECUTING_HOST_CHECKS:
		stop_executing_host_checks();
		break;

	case CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS:
		start_accepting_passive_host_checks();
		break;

	case CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS:
		stop_accepting_passive_host_checks();
		break;

	case CMD_START_OBSESSING_OVER_HOST_CHECKS:
		start_obsessing_over_host_checks();
		break;

	case CMD_STOP_OBSESSING_OVER_HOST_CHECKS:
		stop_obsessing_over_host_checks();
		break;

	case CMD_ENABLE_EVENT_HANDLERS:
		start_using_event_handlers();
		break;

	case CMD_DISABLE_EVENT_HANDLERS:
		stop_using_event_handlers();
		break;

	case CMD_ENABLE_FLAP_DETECTION:
		enable_flap_detection_routines();
		break;

	case CMD_DISABLE_FLAP_DETECTION:
		disable_flap_detection_routines();
		break;

	case CMD_ENABLE_SERVICE_FRESHNESS_CHECKS:
		enable_service_freshness_checks();
		break;
	
	case CMD_DISABLE_SERVICE_FRESHNESS_CHECKS:
		disable_service_freshness_checks();
		break;
	
	case CMD_ENABLE_HOST_FRESHNESS_CHECKS:
		enable_host_freshness_checks();
		break;
	
	case CMD_DISABLE_HOST_FRESHNESS_CHECKS:
		disable_host_freshness_checks();
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


		/***************************/
		/*****  HOST COMMANDS  *****/
		/***************************/

	case CMD_ENABLE_HOST_CHECK:
	case CMD_DISABLE_HOST_CHECK:
	case CMD_ENABLE_PASSIVE_HOST_CHECKS:
	case CMD_DISABLE_PASSIVE_HOST_CHECKS:
	case CMD_ENABLE_HOST_SVC_CHECKS:
	case CMD_DISABLE_HOST_SVC_CHECKS:
	case CMD_ENABLE_HOST_NOTIFICATIONS:
	case CMD_DISABLE_HOST_NOTIFICATIONS:
	case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
	case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
	case CMD_ENABLE_HOST_AND_CHILD_NOTIFICATIONS:
	case CMD_DISABLE_HOST_AND_CHILD_NOTIFICATIONS:
	case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
	case CMD_ENABLE_HOST_FLAP_DETECTION:
	case CMD_DISABLE_HOST_FLAP_DETECTION:
	case CMD_ENABLE_HOST_EVENT_HANDLER:
	case CMD_DISABLE_HOST_EVENT_HANDLER:
	case CMD_START_OBSESSING_OVER_HOST:
	case CMD_STOP_OBSESSING_OVER_HOST:
	case CMD_SET_HOST_NOTIFICATION_NUMBER:
		process_host_command(cmd,entry_time,args);
		break;


		/*****************************/
		/***** HOSTGROUP COMMANDS ****/
		/*****************************/

	case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
	case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
	case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
	case CMD_ENABLE_HOSTGROUP_HOST_CHECKS:
	case CMD_DISABLE_HOSTGROUP_HOST_CHECKS:
	case CMD_ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
	case CMD_DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
	case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
	case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
	case CMD_ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
	case CMD_DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
		process_hostgroup_command(cmd,entry_time,args);
		break;


		/***************************/
		/***** SERVICE COMMANDS ****/
		/***************************/

	case CMD_ENABLE_SVC_CHECK:
	case CMD_DISABLE_SVC_CHECK:
	case CMD_ENABLE_PASSIVE_SVC_CHECKS:
	case CMD_DISABLE_PASSIVE_SVC_CHECKS:
	case CMD_ENABLE_SVC_NOTIFICATIONS:
	case CMD_DISABLE_SVC_NOTIFICATIONS:
	case CMD_ENABLE_SVC_FLAP_DETECTION:
	case CMD_DISABLE_SVC_FLAP_DETECTION:
	case CMD_ENABLE_SVC_EVENT_HANDLER:
	case CMD_DISABLE_SVC_EVENT_HANDLER:
	case CMD_START_OBSESSING_OVER_SVC:
	case CMD_STOP_OBSESSING_OVER_SVC:
	case CMD_SET_SVC_NOTIFICATION_NUMBER:
		process_service_command(cmd,entry_time,args);
		break;


		/********************************/
		/***** SERVICEGROUP COMMANDS ****/
		/********************************/

	case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
	case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
	case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
	case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
	case CMD_ENABLE_SERVICEGROUP_HOST_CHECKS:
	case CMD_DISABLE_SERVICEGROUP_HOST_CHECKS:
	case CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
	case CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
	case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
	case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
	case CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
	case CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
		process_servicegroup_command(cmd,entry_time,args);
		break;


		/***************************/
		/**** UNSORTED COMMANDS ****/
		/***************************/


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

	case CMD_SCHEDULE_SVC_CHECK:
	case CMD_SCHEDULE_FORCED_SVC_CHECK:
		cmd_schedule_check(cmd,args);
		break;

	case CMD_SCHEDULE_HOST_SVC_CHECKS:
	case CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS:
		cmd_schedule_check(cmd,args);
		break;

	case CMD_DEL_ALL_HOST_COMMENTS:
	case CMD_DEL_ALL_SVC_COMMENTS:
		cmd_delete_all_comments(cmd,args);
		break;

	case CMD_PROCESS_SERVICE_CHECK_RESULT:
		cmd_process_service_check_result(cmd,entry_time,args);
		break;

	case CMD_PROCESS_HOST_CHECK_RESULT:
		cmd_process_host_check_result(cmd,entry_time,args);
		break;

	case CMD_ACKNOWLEDGE_HOST_PROBLEM:
	case CMD_ACKNOWLEDGE_SVC_PROBLEM:
		cmd_acknowledge_problem(cmd,args);
		break;

	case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
	case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
		cmd_remove_acknowledgement(cmd,args);
		break;

	case CMD_SCHEDULE_HOST_DOWNTIME:
	case CMD_SCHEDULE_SVC_DOWNTIME:
	case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
	case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
	case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
	case CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME:
	case CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME:
	case CMD_SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME:
	case CMD_SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME:
		cmd_schedule_downtime(cmd,entry_time,args);
		break;

	case CMD_DEL_HOST_DOWNTIME:
	case CMD_DEL_SVC_DOWNTIME:
		cmd_delete_downtime(cmd,args);
		break;

	case CMD_CANCEL_ACTIVE_HOST_SVC_DOWNTIME:
	case CMD_CANCEL_PENDING_HOST_SVC_DOWNTIME:
		break;

	case CMD_SCHEDULE_HOST_CHECK:
	case CMD_SCHEDULE_FORCED_HOST_CHECK:
		cmd_schedule_check(cmd,args);
		break;

	case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
	case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:
	case CMD_CHANGE_HOST_EVENT_HANDLER:
	case CMD_CHANGE_SVC_EVENT_HANDLER:
	case CMD_CHANGE_HOST_CHECK_COMMAND:
	case CMD_CHANGE_SVC_CHECK_COMMAND:
		cmd_change_command(cmd,args);
		break;

	case CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL:
	case CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL:
	case CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL:
		cmd_change_check_interval(cmd,args);
		break;

	case CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS:
	case CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS:
		cmd_change_max_attempts(cmd,args);
		break;

	default:
		break;
	        }

#ifdef DEBUG0
	printf("process_external_command() end\n");
#endif

	return;
        }


/* processes an external host command */
int process_host_command(int cmd, time_t entry_time, char *args){
	char *host_name=NULL;
	host *temp_host=NULL;
	service *temp_service;
	char *str=NULL;
	int intval;

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* find the host */
	temp_host=find_host(host_name);
	if(temp_host==NULL)
		return ERROR;

	switch(cmd){

	case CMD_ENABLE_HOST_NOTIFICATIONS:
		enable_host_notifications(temp_host);
		break;

	case CMD_DISABLE_HOST_NOTIFICATIONS:
		disable_host_notifications(temp_host);
		break;

	case CMD_ENABLE_HOST_AND_CHILD_NOTIFICATIONS:
		enable_and_propagate_notifications(temp_host,0,TRUE,TRUE,FALSE);
		break;

	case CMD_DISABLE_HOST_AND_CHILD_NOTIFICATIONS:
		disable_and_propagate_notifications(temp_host,0,TRUE,TRUE,FALSE);
		break;

	case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		enable_and_propagate_notifications(temp_host,0,FALSE,TRUE,TRUE);

	case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		disable_and_propagate_notifications(temp_host,0,FALSE,TRUE,TRUE);

	case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
		for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
			if(!strcmp(temp_service->host_name,host_name)){
				if(cmd==CMD_ENABLE_HOST_SVC_NOTIFICATIONS)
					enable_service_notifications(temp_service);
				else
					disable_service_notifications(temp_service);
		                }
		        }
		break;

	case CMD_ENABLE_HOST_SVC_CHECKS:
	case CMD_DISABLE_HOST_SVC_CHECKS:
		for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
			if(!strcmp(temp_service->host_name,host_name)){
				if(cmd==CMD_ENABLE_HOST_SVC_CHECKS)
					enable_service_checks(temp_service);
				else
					disable_service_checks(temp_service);
		                }
	                } 
		break;

	case CMD_ENABLE_HOST_CHECK:
		enable_host_checks(temp_host);
		break;

	case CMD_DISABLE_HOST_CHECK:
		disable_host_checks(temp_host);
		break;

	case CMD_ENABLE_HOST_EVENT_HANDLER:
		enable_host_event_handler(temp_host);
		break;

	case CMD_DISABLE_HOST_EVENT_HANDLER:
		disable_host_event_handler(temp_host);
		break;

	case CMD_ENABLE_HOST_FLAP_DETECTION:
		enable_host_flap_detection(temp_host);
		break;

	case CMD_DISABLE_HOST_FLAP_DETECTION:
		disable_host_flap_detection(temp_host);
		break;

	case CMD_ENABLE_PASSIVE_HOST_CHECKS:
		enable_passive_host_checks(temp_host);
		break;

	case CMD_DISABLE_PASSIVE_HOST_CHECKS:
		disable_passive_host_checks(temp_host);
		break;

	case CMD_START_OBSESSING_OVER_HOST:
		start_obsessing_over_host(temp_host);
		break;

	case CMD_STOP_OBSESSING_OVER_HOST:
		stop_obsessing_over_host(temp_host);
		break;

	case CMD_SET_HOST_NOTIFICATION_NUMBER:
		if((str=my_strtok(NULL,";"))){
			intval=atoi(str);
			set_host_notification_number(temp_host,intval);
		        }
		break;

	default:
		break;
	        }

	return OK;
        }


/* processes an external hostgroup command */
int process_hostgroup_command(int cmd, time_t entry_time, char *args){
	char *hostgroup_name=NULL;
	hostgroup *temp_hostgroup=NULL;
	hostgroupmember *temp_member=NULL;
	host *temp_host=NULL;
	service *temp_service=NULL;

	/* get the hostgroup name */
	hostgroup_name=my_strtok(args,";");
	if(hostgroup_name==NULL)
		return ERROR;

	/* find the hostgroup */
	temp_hostgroup=find_hostgroup(hostgroup_name);
	if(temp_hostgroup==NULL)
		return ERROR;

	/* loop through all hosts in the hostgroup */
	for(temp_member=temp_hostgroup->members;temp_member!=NULL;temp_member=temp_member->next){

		temp_host=find_host(temp_member->host_name);
		if(temp_host==NULL)
			continue;

		switch(cmd){

		case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
			enable_host_notifications(temp_host);
			break;

		case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
			disable_host_notifications(temp_host);
			break;

		case CMD_ENABLE_HOSTGROUP_HOST_CHECKS:
			enable_host_checks(temp_host);
			break;

		case CMD_DISABLE_HOSTGROUP_HOST_CHECKS:
			disable_host_checks(temp_host);
			break;

		case CMD_ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
			enable_passive_host_checks(temp_host);
			break;

		case CMD_DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
			disable_passive_host_checks(temp_host);
			break;

		default:

			/* loop through all services on the host */
			for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
				if(!strcmp(temp_service->host_name,temp_host->name)){
					
					switch(cmd){

					case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
						enable_service_notifications(temp_service);
						break;

					case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
						disable_service_notifications(temp_service);
						break;

					case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
						enable_service_checks(temp_service);
						break;

					case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
						disable_service_checks(temp_service);
						break;

					case CMD_ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
						enable_passive_service_checks(temp_service);
						break;

					case CMD_DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
						disable_passive_service_checks(temp_service);
						break;

					default:
						break;
					        }
				        }
			        }
			
			break;
		        }

	        }

	return OK;
        }



/* processes an external service command */
int process_service_command(int cmd, time_t entry_time, char *args){
	char *host_name=NULL;
	char *svc_description=NULL;
	service *temp_service=NULL;
	char *str=NULL;
	int intval;

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* get the service description */
	svc_description=my_strtok(NULL,";");
	if(svc_description==NULL)
		return ERROR;

	/* find the service */
	temp_service=find_service(host_name,svc_description);
	if(temp_service==NULL)
		return ERROR;

	switch(cmd){

	case CMD_ENABLE_SVC_NOTIFICATIONS:
		enable_service_notifications(temp_service);
		break;

	case CMD_DISABLE_SVC_NOTIFICATIONS:
		disable_service_notifications(temp_service);
		break;

	case CMD_ENABLE_SVC_CHECK:
		enable_service_checks(temp_service);
		break;

	case CMD_DISABLE_SVC_CHECK:
		disable_service_checks(temp_service);
		break;

	case CMD_ENABLE_SVC_EVENT_HANDLER:
		enable_service_event_handler(temp_service);
		break;

	case CMD_DISABLE_SVC_EVENT_HANDLER:
		disable_service_event_handler(temp_service);
		break;

	case CMD_ENABLE_SVC_FLAP_DETECTION:
		enable_service_flap_detection(temp_service);
		break;

	case CMD_DISABLE_SVC_FLAP_DETECTION:
		disable_service_flap_detection(temp_service);
		break;

	case CMD_ENABLE_PASSIVE_SVC_CHECKS:
		enable_passive_service_checks(temp_service);
		break;

	case CMD_DISABLE_PASSIVE_SVC_CHECKS:
		disable_passive_service_checks(temp_service);
		break;

	case CMD_START_OBSESSING_OVER_SVC:
		start_obsessing_over_service(temp_service);
		break;

	case CMD_STOP_OBSESSING_OVER_SVC:
		stop_obsessing_over_service(temp_service);
		break;

	case CMD_SET_SVC_NOTIFICATION_NUMBER:
		if((str=my_strtok(NULL,";"))){
			intval=atoi(str);
			set_service_notification_number(temp_service,intval);
		        }
		break;

	default:
		break;
	        }

	return OK;
        }


/* processes an external servicegroup command */
int process_servicegroup_command(int cmd, time_t entry_time, char *args){
	char *servicegroup_name=NULL;
	servicegroup *temp_servicegroup=NULL;
	servicegroupmember *temp_member=NULL;
	host *temp_host=NULL;
	host *last_host=NULL;
	service *temp_service=NULL;

	/* get the servicegroup name */
	servicegroup_name=my_strtok(args,";");
	if(servicegroup_name==NULL)
		return ERROR;

	/* find the servicegroup */
	temp_servicegroup=find_servicegroup(servicegroup_name);
	if(temp_servicegroup==NULL)
		return ERROR;

	switch(cmd){

	case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
	case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
	case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
	case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
	case CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
	case CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:

		/* loop through all servicegroup members */
		for(temp_member=temp_servicegroup->members;temp_member!=NULL;temp_member=temp_member->next){

			temp_service=find_service(temp_member->host_name,temp_member->service_description);
			if(temp_service==NULL)
				continue;

			switch(cmd){

			case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
				enable_service_notifications(temp_service);
				break;

			case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
				disable_service_notifications(temp_service);
				break;

			case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
				enable_service_checks(temp_service);
				break;

			case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
				disable_service_checks(temp_service);
				break;

			case CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
				enable_passive_service_checks(temp_service);
				break;

			case CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
				disable_passive_service_checks(temp_service);
				break;

			default:
				break;
			        }
		        }

		break;

	case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
	case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
	case CMD_ENABLE_SERVICEGROUP_HOST_CHECKS:
	case CMD_DISABLE_SERVICEGROUP_HOST_CHECKS:
	case CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
	case CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:

		/* loop through all hosts that have services belonging to the servicegroup */
		last_host=NULL;
		for(temp_member=temp_servicegroup->members;temp_member!=NULL;temp_member=temp_member->next){

			temp_host=find_host(temp_member->host_name);
			if(temp_host==NULL)
				continue;

			if(temp_host==last_host)
				continue;

			switch(cmd){

			case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
				enable_host_notifications(temp_host);
				break;

			case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
				disable_host_notifications(temp_host);
				break;

			case CMD_ENABLE_SERVICEGROUP_HOST_CHECKS:
				enable_host_checks(temp_host);
				break;

			case CMD_DISABLE_SERVICEGROUP_HOST_CHECKS:
				disable_host_checks(temp_host);
				break;

			case CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
				enable_passive_host_checks(temp_host);
				break;

			case CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
				disable_passive_host_checks(temp_host);
				break;

			default:
				break;
			        }

			last_host=temp_host;
		        }

		break;

	default:
		break;
	        }

	return OK;
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
	char *comment_data;
	int persistent=0;
	int result;

#ifdef DEBUG0
	printf("cmd_add_comment() start\n");
#endif

	/* get the host name */
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
		temp_service=find_service(host_name,svc_description);
		if(temp_service==NULL)
			return ERROR;
	        }

	/* else verify that the host is valid */
	temp_host=find_host(host_name);
	if(temp_host==NULL)
		return ERROR;

	/* get the persistent flag */
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
	comment_data=my_strtok(NULL,"\n");
	if(comment_data==NULL)
		return ERROR;

	/* add the comment */
	result=add_new_comment((cmd==CMD_ADD_HOST_COMMENT)?HOST_COMMENT:SERVICE_COMMENT,USER_COMMENT,host_name,svc_description,entry_time,user,comment_data,persistent,COMMENTSOURCE_EXTERNAL,FALSE,(time_t)0,NULL);

	if(result<0)
		return ERROR;

#ifdef DEBUG0
	printf("cmd_add_comment() end\n");
#endif
	return OK;
        }



/* removes a host or service comment from the status log */
int cmd_delete_comment(int cmd,char *args){
	unsigned long comment_id;

#ifdef DEBUG0
	printf("cmd_del_comment() start\n");
#endif

	/* get the comment id we should delete */
	comment_id=strtoul(args,NULL,10);
	if(comment_id==0)
		return ERROR;

	/* delete the specified comment */
	if(cmd==CMD_DEL_HOST_COMMENT)
		delete_host_comment(comment_id);
	else
		delete_service_comment(comment_id);

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
	
	/* get the host name */
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
		temp_service=find_service(host_name,svc_description);
		if(temp_service==NULL)
			return ERROR;
	        }

	/* else verify that the host is valid */
	temp_host=find_host(host_name);
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

	/* get the host name */
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
		temp_service=find_service(host_name,svc_description);
		if(temp_service==NULL)
			return ERROR;
	        }

	/* else verify that the host is valid */
	else{

		temp_host=find_host(host_name);
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
		temp_host->next_host_notification=delay_time;
	else
		temp_service->next_notification=delay_time;
	
#ifdef DEBUG0
	printf("cmd_delay_notification() end\n");
#endif
	return OK;
        }



/* schedules a host check at a particular time */
int cmd_schedule_check(int cmd,char *args){
	char *temp_ptr;
	host *temp_host=NULL;
	service *temp_service=NULL;
	char *host_name="";
	char *svc_description="";
	time_t delay_time=0L;

#ifdef DEBUG0
	printf("cmd_schedule_check() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	if(cmd==CMD_SCHEDULE_HOST_CHECK || cmd==CMD_SCHEDULE_FORCED_HOST_CHECK || cmd==CMD_SCHEDULE_HOST_SVC_CHECKS || cmd==CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS){

		/* verify that the host is valid */
		temp_host=find_host(host_name);
		if(temp_host==NULL)
			return ERROR;
	        }
	
	else{

		/* get the service description */
		svc_description=my_strtok(NULL,";");
		if(svc_description==NULL)
			return ERROR;

		/* verify that the service is valid */
		temp_service=find_service(host_name,svc_description);
		if(temp_service==NULL)
			return ERROR;
	        }

	/* get the next check time */
	temp_ptr=my_strtok(NULL,"\n");
	if(temp_ptr==NULL)
		return ERROR;
	delay_time=strtoul(temp_ptr,NULL,10);

	/* schedule the check */
	if(cmd==CMD_SCHEDULE_HOST_CHECK || cmd==CMD_SCHEDULE_FORCED_HOST_CHECK)
		schedule_host_check(temp_host,delay_time,(cmd==CMD_SCHEDULE_FORCED_HOST_CHECK)?TRUE:FALSE);
	else if(cmd==CMD_SCHEDULE_HOST_SVC_CHECKS || cmd==CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS){
		for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
			if(!strcmp(temp_service->host_name,host_name))
				schedule_service_check(temp_service,delay_time,(cmd==CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS)?TRUE:FALSE);
		        }
	        }
	else
		schedule_service_check(temp_service,delay_time,(cmd==CMD_SCHEDULE_FORCED_SVC_CHECK)?TRUE:FALSE);

#ifdef DEBUG0
	printf("cmd_schedule_check() end\n");
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
	temp_host=find_host(host_name);
	if(temp_host==NULL)
		return ERROR;

	/* get the next check time */
	temp_ptr=my_strtok(NULL,"\n");
	if(temp_ptr==NULL)
		return ERROR;
	delay_time=strtoul(temp_ptr,NULL,10);

	/* reschedule all services on the specified host */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
		if(!strcmp(temp_service->host_name,host_name))
			schedule_service_check(temp_service,delay_time,force);
	        }

#ifdef DEBUG0
	printf("cmd_schedule_host_service_checks() end\n");
#endif
	return OK;
        }




/* schedules a program shutdown or restart */
int cmd_signal_process(int cmd, char *args){
	time_t scheduled_time;
	char *temp_ptr;
	int result;

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
	result=schedule_new_event((cmd==CMD_SHUTDOWN_PROCESS)?EVENT_PROGRAM_SHUTDOWN:EVENT_PROGRAM_RESTART,TRUE,scheduled_time,FALSE,0,NULL,FALSE,NULL,NULL);

#ifdef DEBUG0
	printf("cmd_signal_process() end\n");
#endif

	return result;
        }



/* processes results of an external service check */
int cmd_process_service_check_result(int cmd,time_t check_time,char *args){
	char *temp_ptr;
	char *host_name;
	char *svc_description;
	int return_code;
	char *output;
	int result;

#ifdef DEBUG0
	printf("cmd_process_service_check_result() start\n");
#endif

	/* get the host name */
	temp_ptr=my_strtok(args,";");
	if(temp_ptr==NULL)
		return ERROR;
	host_name=strdup(temp_ptr);

	/* get the service description */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL){
		free(host_name);
		return ERROR;
	        }
	svc_description=strdup(temp_ptr);

	/* get the service check return code */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL){
		free(host_name);
		free(svc_description);
		return ERROR;
	        }
	return_code=atoi(temp_ptr);

	/* get the plugin output (may be empty) */
	temp_ptr=my_strtok(NULL,"\n");
	if(temp_ptr==NULL)
		output=strdup("");
	else
		output=strdup(temp_ptr);

	/* submit the passive check result */
	result=process_passive_service_check(check_time,host_name,svc_description,return_code,output);

	/* free memory */
	free(host_name);
	free(svc_description);
	free(output);

#ifdef DEBUG0
	printf("cmd_process_service_check_result() end\n");
#endif

	return result;
        }



/* submits a passive service check result for later processing */
int process_passive_service_check(time_t check_time, char *host_name, char *svc_description, int return_code, char *output){
	passive_check_result *new_pcr=NULL;
	host *temp_host=NULL;
	service *temp_service=NULL;
	char *real_host_name=NULL;
	char temp_buffer[MAX_INPUT_BUFFER]="";

#ifdef DEBUG0
	printf("process_passive_service_check() start\n");
#endif

	/* skip this service check result if we aren't accepting passive service checks */
	if(accept_passive_service_checks==FALSE)
		return ERROR;

	/* make sure we have all required data */
	if(host_name==NULL || svc_description==NULL || output==NULL)
		return ERROR;

	/* find the host by its name or address */
	if(find_host(host_name)!=NULL)
		real_host_name=host_name;
	else{
		for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
			if(!strcmp(host_name,temp_host->address)){
				real_host_name=temp_host->name;
				break;
			        }
		        }
	        }

	/* we couldn't find the host */
	if(real_host_name==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer),"Warning:  Passive check result was received for service '%s' on host '%s', but the host could not be found!\n",svc_description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);

		return ERROR;
		}

	/* make sure the service exists */
	if((temp_service=find_service(real_host_name,svc_description))==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer),"Warning:  Passive check result was received for service '%s' on host '%s', but the service could not be found!\n",svc_description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);

		return ERROR;
		}

	/* skip this is we aren't accepting passive checks for this service */
	if(temp_service->accept_passive_service_checks==FALSE)
		return ERROR;

	/* allocate memory for the passive check result */
	new_pcr=(passive_check_result *)malloc(sizeof(passive_check_result));
	if(new_pcr==NULL)
		return ERROR;

	/* save the host name */
	new_pcr->host_name=strdup(real_host_name);
	if(new_pcr->host_name==NULL){
		free(new_pcr);
		return ERROR;
	        }

	/* save the service description */
	new_pcr->svc_description=strdup(svc_description);
	if(new_pcr->svc_description==NULL){
		free(new_pcr->host_name);
		free(new_pcr);
		return ERROR;
	        }

	/* save the return code */
	new_pcr->return_code=return_code;

	/* make sure the return code is within bounds */
	if(new_pcr->return_code<0 || new_pcr->return_code>3)
		new_pcr->return_code=STATE_UNKNOWN;

	/* save the output */
	new_pcr->output=strdup(output);
	if(new_pcr->output==NULL){
		free(new_pcr->svc_description);
		free(new_pcr->host_name);
		free(new_pcr);
		return ERROR;
	        }

	new_pcr->check_time=check_time;

	new_pcr->next=NULL;

	/* add the passive check result to the end of the list in memory */
	if(passive_check_result_list==NULL)
		passive_check_result_list=new_pcr;
	else
		passive_check_result_list_tail->next=new_pcr;
	passive_check_result_list_tail=new_pcr;

#ifdef DEBUG0
	printf("process_passive_service_check() end\n");
#endif

	return OK;
        }



/* process passive host check result */
int cmd_process_host_check_result(int cmd,time_t check_time,char *args){
	char *temp_ptr;
	char *host_name;
	int return_code;
	char *output;
	int result;

#ifdef DEBUG0
	printf("cmd_process_host_check_result() start\n");
#endif

	/* get the host name */
	temp_ptr=my_strtok(args,";");
	if(temp_ptr==NULL)
		return ERROR;
	host_name=strdup(temp_ptr);

	/* get the host check return code */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL){
		free(host_name);
		return ERROR;
	        }
	return_code=atoi(temp_ptr);

	/* get the plugin output (may be empty) */
	temp_ptr=my_strtok(NULL,"\n");
	if(temp_ptr==NULL)
		output=strdup("");
	else
		output=strdup(temp_ptr);

	/* submit the check result */
	result=process_passive_host_check(check_time,host_name,return_code,output);

	/* free memory */
	free(host_name);
	free(output);

#ifdef DEBUG0
	printf("cmd_process_host_check_result() end\n");
#endif

	return result;
        }


/* process passive host check result */
/* this function is a bit more involved than for passive service checks, as we need to replicate most functions performed by check_route_to_host() */
int process_passive_host_check(time_t check_time, char *host_name, int return_code, char *output){
	host *temp_host=NULL;
	char *real_host_name="";
	struct timeval tv;
	char temp_plugin_output[MAX_PLUGINOUTPUT_LENGTH]="";
	char old_plugin_output[MAX_PLUGINOUTPUT_LENGTH]="";
	char *temp_ptr=NULL;
	char temp_buffer[MAX_INPUT_BUFFER]="";
	char *output_ptr=NULL;
	char *perf_ptr=NULL;

#ifdef DEBUG0
	printf("process_passive_host_check() start\n");
#endif

	/* skip this host check result if we aren't accepting passive host check results */
	if(accept_passive_host_checks==FALSE)
		return ERROR;

	/* make sure we have all required data */
	if(host_name==NULL || output==NULL)
		return ERROR;

	/* find the host by its name or address */
	if((temp_host=find_host(host_name))!=NULL)
		real_host_name=host_name;
	else{
		for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
			if(!strcmp(host_name,temp_host->address)){
				real_host_name=temp_host->name;
				break;
			        }
		        }

		temp_host=find_host(real_host_name);
	        }

	/* we couldn't find the host */
	if(temp_host==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer),"Warning:  Passive check result was received for host '%s', but the host could not be found!\n",host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);

		return ERROR;
	        }

	/* skip this is we aren't accepting passive checks for this host */
	if(temp_host->accept_passive_host_checks==FALSE)
		return ERROR;

	/* make sure the return code is within bounds */
	if(return_code<0 || return_code>2)
		return ERROR;

	/* save the plugin output */
	strncpy(temp_plugin_output,output,MAX_PLUGINOUTPUT_LENGTH-1);
	temp_plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';

	

	/********* LET'S DO IT (SUBSET OF NORMAL HOST CHECK OPERATIONS) ********/

	/* set the checked flag */
	temp_host->has_been_checked=TRUE;

	/* save old state */
	temp_host->last_state=temp_host->current_state;
	if(temp_host->state_type==HARD_STATE)
		temp_host->last_hard_state=temp_host->current_state;

	/* NOTE TO SELF: Host state should be adjusted to reflect current host topology... */
	/* record new state */
	temp_host->current_state=return_code;

	/* record check type */
	temp_host->check_type=HOST_CHECK_PASSIVE;

	/* record state type */
	temp_host->state_type=HARD_STATE;

	/* set the current attempt - should this be set to max_attempts instead? */
	temp_host->current_attempt=1;

	/* save the old plugin output and host state for use with state stalking routines */
	strncpy(old_plugin_output,(temp_host->plugin_output==NULL)?"":temp_host->plugin_output,sizeof(old_plugin_output)-1);
	old_plugin_output[sizeof(old_plugin_output)-1]='\x0';

	/* set the last host check time */
	temp_host->last_check=check_time;

	/* clear plugin output and performance data buffers */
	strcpy(temp_host->plugin_output,"");
	strcpy(temp_host->perf_data,"");

	/* passive checks have ZERO execution time! */
	temp_host->execution_time=0.0;

	/* calculate latency */
	gettimeofday(&tv,NULL);
	temp_host->latency=(double)((double)(tv.tv_sec-check_time)+(double)(tv.tv_usec/1000)/1000.0);
	if(temp_host->latency<0.0)
		temp_host->latency=0.0;

	/* first part of plugin output (up to pipe) is status info, perfdata comes after pipe */
	/* don't use strtok() as it can cause problems if external commands are called recursively */
	/* adaptation of patch submitted by Altinity 9/17/07 */
	output_ptr=temp_plugin_output;
	perf_ptr=strchr(temp_plugin_output,'|');
	if(perf_ptr){
		perf_ptr[0]='\x0';
		perf_ptr++;
		}

	/* save plugin output */
	strip(output_ptr);
	strncpy(temp_host->plugin_output,(!strcmp(output_ptr,""))?"(No output returned from host check)":output_ptr,MAX_PLUGINOUTPUT_LENGTH-1);
	temp_host->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';

	/* save perfdata if present */
	if(perf_ptr){
		strip(perf_ptr);
		strncpy(temp_host->perf_data,perf_ptr,MAX_PLUGINOUTPUT_LENGTH-1);
		temp_host->perf_data[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
		}

	/* replace semicolons in plugin output (but not performance data) with colons */
	temp_ptr=temp_host->plugin_output;
	while((temp_ptr=strchr(temp_ptr,';')))
	      *temp_ptr=':';


	/***** HANDLE THE HOST STATE *****/
	handle_host_state(temp_host);


	/***** UPDATE HOST STATUS *****/
	update_host_status(temp_host,FALSE);


	/****** STALKING STUFF *****/
	/* if the host didn't change state and the plugin output differs from the last time it was checked, log the current state/output if state stalking is enabled */
	if(temp_host->last_state==temp_host->current_state && strcmp(old_plugin_output,temp_host->plugin_output)){

		if(temp_host->current_state==HOST_UP && temp_host->stalk_on_up==TRUE)
			log_host_event(temp_host);

		else if(temp_host->current_state==HOST_DOWN && temp_host->stalk_on_down==TRUE)
			log_host_event(temp_host);

		else if(temp_host->current_state==HOST_UNREACHABLE && temp_host->stalk_on_unreachable==TRUE)
			log_host_event(temp_host);
	        }

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_host_check(NEBTYPE_HOSTCHECK_PROCESSED,NEBFLAG_NONE,NEBATTR_NONE,temp_host,HOST_CHECK_PASSIVE,temp_host->current_state,temp_host->state_type,tv,tv,NULL,0.0,temp_host->execution_time,0,FALSE,return_code,NULL,temp_host->plugin_output,temp_host->perf_data,NULL);
#endif

	/***** CHECK FOR FLAPPING *****/
	check_for_host_flapping(temp_host,TRUE);


#ifdef DEBUG0
	printf("process_passive_host_check() end\n");
#endif

	return OK;
        }



/* acknowledges a host or service problem */
int cmd_acknowledge_problem(int cmd,char *args){
	service *temp_service=NULL;
	host *temp_host=NULL;
	char *host_name="";
	char *svc_description="";
	char *ack_author;
	char *ack_data;
	char *temp_ptr;
	int type=ACKNOWLEDGEMENT_NORMAL;
	int notify=TRUE;
	int persistent=TRUE;

#ifdef DEBUG0
	printf("cmd_acknowledge_problem() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	/* verify that the host is valid */
	temp_host=find_host(host_name);
	if(temp_host==NULL)
		return ERROR;

	/* this is a service acknowledgement */
	if(cmd==CMD_ACKNOWLEDGE_SVC_PROBLEM){

		/* get the service name */
		svc_description=my_strtok(NULL,";");
		if(svc_description==NULL)
			return ERROR;

		/* verify that the service is valid */
		temp_service=find_service(temp_host->name,svc_description);
		if(temp_service==NULL)
			return ERROR;
	        }

	/* get the type */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	type=atoi(temp_ptr);

	/* get the notification option */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	notify=(atoi(temp_ptr)>0)?TRUE:FALSE;

	/* get the persistent option */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	persistent=(atoi(temp_ptr)>0)?TRUE:FALSE;

	/* get the acknowledgement author */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	ack_author=strdup(temp_ptr);
	
	/* get the acknowledgement data */
	temp_ptr=my_strtok(NULL,"\n");
	if(temp_ptr==NULL)
		return ERROR;
	ack_data=strdup(temp_ptr);
	
	/* acknowledge the host problem */
	if(cmd==CMD_ACKNOWLEDGE_HOST_PROBLEM)
		acknowledge_host_problem(temp_host,ack_author,ack_data,type,notify,persistent);

	/* acknowledge the service problem */
	else
		acknowledge_service_problem(temp_service,ack_author,ack_data,type,notify,persistent);

	/* free memory */
	free(ack_author);
	free(ack_data);

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
	temp_host=find_host(host_name);
	if(temp_host==NULL)
		return ERROR;

	/* we are removing a service acknowledgement */
	if(cmd==CMD_REMOVE_SVC_ACKNOWLEDGEMENT){

		/* get the service name */
		svc_description=my_strtok(NULL,";");
		if(svc_description==NULL)
			return ERROR;

		/* verify that the service is valid */
		temp_service=find_service(temp_host->name,svc_description);
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



/* schedules downtime for a specific host or service */
int cmd_schedule_downtime(int cmd, time_t entry_time, char *args){
	service *temp_service=NULL;
	host *temp_host=NULL;
	host *last_host=NULL;
	hostgroup *temp_hostgroup=NULL;
	hostgroupmember *temp_hgmember=NULL;
	servicegroup *temp_servicegroup=NULL;
	servicegroupmember *temp_sgmember=NULL;
	char *host_name="";
	char *hostgroup_name="";
	char *servicegroup_name="";
	char *svc_description="";
	char *temp_ptr;
	time_t start_time;
	time_t end_time;
	int fixed;
	unsigned long triggered_by;
	unsigned long duration;
	char *author="";
	char *comment_data="";
	unsigned long downtime_id;

#ifdef DEBUG0
	printf("cmd_schedule_downtime() start\n");
#endif

	if(cmd==CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME || cmd==CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME){
		
		/* get the hostgroup name */
		hostgroup_name=my_strtok(args,";");
		if(hostgroup_name==NULL)
			return ERROR;

		/* verify that the hostgroup is valid */
		temp_hostgroup=find_hostgroup(hostgroup_name);
		if(temp_hostgroup==NULL)
			return ERROR;
	        }

	else if(cmd==CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME || cmd==CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME){

		/* get the servicegroup name */
		servicegroup_name=my_strtok(args,";");
		if(servicegroup_name==NULL)
			return ERROR;

		/* verify that the servicegroup is valid */
		temp_servicegroup=find_servicegroup(servicegroup_name);
		if(temp_servicegroup==NULL)
			return ERROR;
	        }

	else{

		/* get the host name */
		host_name=my_strtok(args,";");
		if(host_name==NULL)
			return ERROR;

		/* verify that the host is valid */
		temp_host=find_host(host_name);
		if(temp_host==NULL)
			return ERROR;

		/* this is a service downtime */
		if(cmd==CMD_SCHEDULE_SVC_DOWNTIME){

			/* get the service name */
			svc_description=my_strtok(NULL,";");
			if(svc_description==NULL)
				return ERROR;

			/* verify that the service is valid */
			temp_service=find_service(temp_host->name,svc_description);
			if(temp_service==NULL)
				return ERROR;
		        }
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

	/* get the trigger id */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;
	triggered_by=strtoul(temp_ptr,NULL,10);

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
	comment_data=my_strtok(NULL,";");
	if(comment_data==NULL)
		return ERROR;

	/* duration should be auto-calculated, not user-specified */
	if(fixed>0)
		duration=(unsigned long)(end_time-start_time);

	/* schedule downtime */
	switch(cmd){

	case CMD_SCHEDULE_HOST_DOWNTIME:
		schedule_downtime(HOST_DOWNTIME,host_name,NULL,entry_time,author,comment_data,start_time,end_time,fixed,triggered_by,duration,&downtime_id);
		break;

	case CMD_SCHEDULE_SVC_DOWNTIME:
		schedule_downtime(SERVICE_DOWNTIME,host_name,svc_description,entry_time,author,comment_data,start_time,end_time,fixed,triggered_by,duration,&downtime_id);
		break;

	case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
		for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
			if(!strcmp(temp_service->host_name,host_name))
				schedule_downtime(SERVICE_DOWNTIME,host_name,temp_service->description,entry_time,author,comment_data,start_time,end_time,fixed,triggered_by,duration,&downtime_id);
	                }
		break;

	case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
		for(temp_hgmember=temp_hostgroup->members;temp_hgmember!=NULL;temp_hgmember=temp_hgmember->next)
			schedule_downtime(HOST_DOWNTIME,temp_hgmember->host_name,NULL,entry_time,author,comment_data,start_time,end_time,fixed,triggered_by,duration,&downtime_id);
		break;

	case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
		for(temp_hgmember=temp_hostgroup->members;temp_hgmember!=NULL;temp_hgmember=temp_hgmember->next){
			for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
				if(!strcmp(temp_service->host_name,temp_hgmember->host_name))
					schedule_downtime(SERVICE_DOWNTIME,temp_service->host_name,temp_service->description,entry_time,author,comment_data,start_time,end_time,fixed,triggered_by,duration,&downtime_id);
		                }
		        }
		break;

	case CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME:
		last_host=NULL;
		for(temp_sgmember=temp_servicegroup->members;temp_sgmember!=NULL;temp_sgmember=temp_sgmember->next){
			temp_host=find_host(temp_sgmember->host_name);
			if(temp_host==NULL)
				continue;
			if(last_host==temp_host)
				continue;
			schedule_downtime(HOST_DOWNTIME,temp_sgmember->host_name,NULL,entry_time,author,comment_data,start_time,end_time,fixed,triggered_by,duration,&downtime_id);
			last_host=temp_host;
		        }
		break;

	case CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME:
		for(temp_sgmember=temp_servicegroup->members;temp_sgmember!=NULL;temp_sgmember=temp_sgmember->next)
			schedule_downtime(SERVICE_DOWNTIME,temp_sgmember->host_name,temp_sgmember->service_description,entry_time,author,comment_data,start_time,end_time,fixed,triggered_by,duration,&downtime_id);
		break;

	case CMD_SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME:

		/* schedule downtime for "parent" host */
		schedule_downtime(HOST_DOWNTIME,host_name,NULL,entry_time,author,comment_data,start_time,end_time,fixed,triggered_by,duration,&downtime_id);

		/* schedule (non-triggered) downtime for all child hosts */
		schedule_and_propagate_downtime(temp_host,entry_time,author,comment_data,start_time,end_time,fixed,0,duration);
		break;

	case CMD_SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME:

		/* schedule downtime for "parent" host */
		schedule_downtime(HOST_DOWNTIME,host_name,NULL,entry_time,author,comment_data,start_time,end_time,fixed,triggered_by,duration,&downtime_id);

		/* schedule triggered downtime for all child hosts */
		schedule_and_propagate_downtime(temp_host,entry_time,author,comment_data,start_time,end_time,fixed,downtime_id,duration);
		break;

	default:
		break;
	        }

#ifdef DEBUG0
	printf("cmd_schedule_downtime() end\n");
#endif
	return OK;
        }



/* deletes scheduled host or service downtime */
int cmd_delete_downtime(int cmd, char *args){
	unsigned long downtime_id;
	char *temp_ptr;

#ifdef DEBUG0
	printf("cmd_delete_downtime() start\n");
#endif

	/* get the id of the downtime to delete */
	temp_ptr=my_strtok(args,"\n");
	if(temp_ptr==NULL)
		return ERROR;
	downtime_id=strtoul(temp_ptr,NULL,10);

	if(cmd==CMD_DEL_HOST_DOWNTIME)
		unschedule_downtime(HOST_DOWNTIME,downtime_id);
	else
		unschedule_downtime(SERVICE_DOWNTIME,downtime_id);

#ifdef DEBUG0
	printf("cmd_delete_downtime() end\n");
#endif

	return OK;
        }

        
/* changes a host or service command */
int cmd_change_command(int cmd,char *args){
	service *temp_service=NULL;
	host *temp_host=NULL;
	command *temp_command=NULL;
	char *host_name="";
	char *svc_description="";
	char *command_name="";
	char *temp_ptr;
	char *temp_ptr2;
	unsigned long attr=MODATTR_NONE;

#ifdef DEBUG0
	printf("cmd_change_command() start\n");
#endif

	if(cmd!=CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER && cmd!=CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER){

		/* get the host name */
		host_name=my_strtok(args,";");
		if(host_name==NULL)
			return ERROR;

		if(cmd==CMD_CHANGE_SVC_EVENT_HANDLER || cmd==CMD_CHANGE_SVC_CHECK_COMMAND){

			/* get the service name */
			svc_description=my_strtok(NULL,";");
			if(svc_description==NULL)
				return ERROR;

			/* verify that the service is valid */
			temp_service=find_service(host_name,svc_description);
			if(temp_service==NULL)
				return ERROR;
	                }
		else{

			/* verify that the host is valid */
			temp_host=find_host(host_name);
			if(temp_host==NULL)
				return ERROR;
		        }

		command_name=my_strtok(NULL,";");
	        }

	else
		command_name=my_strtok(args,";");

	if(command_name==NULL)
		return ERROR;
	temp_ptr=strdup(command_name);
	if(temp_ptr==NULL)
		return ERROR;

	/* make sure the command exists */
	temp_ptr2=my_strtok(temp_ptr,"!");
	temp_command=find_command(temp_ptr2);
	if(temp_command==NULL)
		return ERROR;

	free(temp_ptr);
	temp_ptr=strdup(command_name);
	if(temp_ptr==NULL)
		return ERROR;

	switch(cmd){

	case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
	case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:

		if(cmd==CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER){
			free(global_host_event_handler);
			global_host_event_handler=temp_ptr;
			attr=MODATTR_EVENT_HANDLER_COMMAND;
			modified_host_process_attributes|=attr;

#ifdef USE_EVENT_BROKER
			/* send data to event broker */
			broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE,NEBFLAG_NONE,NEBATTR_NONE,cmd,attr,modified_host_process_attributes,MODATTR_NONE,modified_service_process_attributes,global_host_event_handler,global_service_event_handler,NULL);
#endif
		        }
		else{
			free(global_service_event_handler);
			global_service_event_handler=temp_ptr;
			attr=MODATTR_EVENT_HANDLER_COMMAND;
			modified_service_process_attributes|=attr;

#ifdef USE_EVENT_BROKER
			/* send data to event broker */
			broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE,NEBFLAG_NONE,NEBATTR_NONE,cmd,MODATTR_NONE,modified_host_process_attributes,attr,modified_service_process_attributes,global_host_event_handler,global_service_event_handler,NULL);
#endif
		        }

		/* update program status */
		update_program_status(FALSE);
		break;

	case CMD_CHANGE_HOST_EVENT_HANDLER:
	case CMD_CHANGE_HOST_CHECK_COMMAND:

		if(cmd==CMD_CHANGE_HOST_EVENT_HANDLER){
			free(temp_host->event_handler);
			temp_host->event_handler=temp_ptr;
			attr=MODATTR_EVENT_HANDLER_COMMAND;
		        }
		else{
			free(temp_host->host_check_command);
			temp_host->host_check_command=temp_ptr;
			attr=MODATTR_CHECK_COMMAND;
		        }

		temp_host->modified_attributes|=attr;

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE,NEBFLAG_NONE,NEBATTR_NONE,temp_host,cmd,attr,temp_host->modified_attributes,NULL);
#endif

		/* update host status */
		update_host_status(temp_host,FALSE);
		break;

	case CMD_CHANGE_SVC_EVENT_HANDLER:
	case CMD_CHANGE_SVC_CHECK_COMMAND:

		if(cmd==CMD_CHANGE_SVC_EVENT_HANDLER){
			free(temp_service->event_handler);
			temp_service->event_handler=temp_ptr;
			attr=MODATTR_EVENT_HANDLER_COMMAND;
		        }
		else{
			free(temp_service->service_check_command);
			temp_service->service_check_command=temp_ptr;
			attr=MODATTR_CHECK_COMMAND;
		        }

		temp_service->modified_attributes|=attr;

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,NEBFLAG_NONE,NEBATTR_NONE,temp_service,cmd,attr,temp_service->modified_attributes,NULL);
#endif

		/* update service status */
		update_service_status(temp_service,FALSE);
		break;

	default:
		break;
	        }

#ifdef DEBUG0
	printf("cmd_change_command() start\n");
#endif

	return OK;
        }
	

        
/* changes a host or service check interval */
int cmd_change_check_interval(int cmd,char *args){
	service *temp_service=NULL;
	host *temp_host=NULL;
	char *host_name="";
	char *svc_description="";
	char *temp_ptr;
	int interval;
	int old_interval;
	time_t preferred_time;
	time_t next_valid_time;
	unsigned long attr;

#ifdef DEBUG0
	printf("cmd_change_check_interval() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	if(cmd==CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL || cmd==CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL){

		/* get the service name */
		svc_description=my_strtok(NULL,";");
		if(svc_description==NULL)
			return ERROR;

		/* verify that the service is valid */
		temp_service=find_service(host_name,svc_description);
		if(temp_service==NULL)
			return ERROR;
	        }
	else{

		/* verify that the host is valid */
		temp_host=find_host(host_name);
		if(temp_host==NULL)
			return ERROR;
	        }

	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;

	interval=atoi(temp_ptr);
	if(interval<0)
		return ERROR;

	switch(cmd){

	case CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL:

		/* save the old check interval */
		old_interval=temp_host->check_interval;

		/* modify the check interval */
		temp_host->check_interval=interval;
		attr=MODATTR_NORMAL_CHECK_INTERVAL;
		temp_host->modified_attributes|=attr;

		/* schedule a host check if previous interval was 0 (checks were not regularly scheduled) */
		if(old_interval==0 && temp_host->checks_enabled==TRUE){

			/* set the host check flag */
			temp_host->should_be_scheduled=TRUE;

			/* schedule a check for right now (or as soon as possible) */
			time(&preferred_time);
			if(check_time_against_period(preferred_time,temp_host->check_period)==ERROR){
				get_next_valid_time(preferred_time,&next_valid_time,temp_host->check_period);
				temp_host->next_check=next_valid_time;
			        }
			else
				temp_host->next_check=preferred_time;

			/* schedule a check if we should */
			if(temp_host->should_be_scheduled==TRUE)
				schedule_host_check(temp_host,temp_host->next_check,FALSE);
		        }

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE,NEBFLAG_NONE,NEBATTR_NONE,temp_host,cmd,attr,temp_host->modified_attributes,NULL);
#endif

		/* update the status log with the host info */
		update_host_status(temp_host,FALSE);
		break;

	case CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL:

		/* save the old check interval */
		old_interval=temp_service->check_interval;

		/* modify the check interval */
		temp_service->check_interval=interval;
		attr=MODATTR_NORMAL_CHECK_INTERVAL;
		temp_service->modified_attributes|=attr;

		/* schedule a service check if previous interval was 0 (checks were not regularly scheduled) */
		if(old_interval==0 && temp_service->checks_enabled==TRUE && temp_service->check_interval!=0){

			/* set the service check flag */
			temp_service->should_be_scheduled=TRUE;

			/* schedule a check for right now (or as soon as possible) */
			time(&preferred_time);
			if(check_time_against_period(preferred_time,temp_service->check_period)==ERROR){
				get_next_valid_time(preferred_time,&next_valid_time,temp_service->check_period);
				temp_service->next_check=next_valid_time;
			        }
			else
				temp_service->next_check=preferred_time;

			/* schedule a check if we should */
			if(temp_service->should_be_scheduled==TRUE)
				schedule_service_check(temp_service,temp_service->next_check,FALSE);
		        }

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,NEBFLAG_NONE,NEBATTR_NONE,temp_service,cmd,attr,temp_service->modified_attributes,NULL);
#endif

		/* update the status log with the service info */
		update_service_status(temp_service,FALSE);
		break;

	case CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL:

		temp_service->retry_interval=interval;
		attr=MODATTR_RETRY_CHECK_INTERVAL;
		temp_service->modified_attributes|=attr;

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,NEBFLAG_NONE,NEBATTR_NONE,temp_service,cmd,attr,temp_service->modified_attributes,NULL);
#endif

		/* update the status log with the service info */
		update_service_status(temp_service,FALSE);
		break;

	default:
		break;
	        }

#ifdef DEBUG0
	printf("cmd_change_check_interval() start\n");
#endif

	return OK;
        }
	
        
/* changes a host or service max check attempts */
int cmd_change_max_attempts(int cmd,char *args){
	service *temp_service=NULL;
	host *temp_host=NULL;
	char *host_name="";
	char *svc_description="";
	char *temp_ptr;
	int max_attempts;
	unsigned long attr;

#ifdef DEBUG0
	printf("cmd_change_max_attempts() start\n");
#endif

	/* get the host name */
	host_name=my_strtok(args,";");
	if(host_name==NULL)
		return ERROR;

	if(cmd==CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS){

		/* get the service name */
		svc_description=my_strtok(NULL,";");
		if(svc_description==NULL)
			return ERROR;

		/* verify that the service is valid */
		temp_service=find_service(host_name,svc_description);
		if(temp_service==NULL)
			return ERROR;
	        }
	else{

		/* verify that the host is valid */
		temp_host=find_host(host_name);
		if(temp_host==NULL)
			return ERROR;
	        }

	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL)
		return ERROR;

	max_attempts=atoi(temp_ptr);
	if(max_attempts<1)
		return ERROR;

	switch(cmd){

	case CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS:

		temp_host->max_attempts=max_attempts;
		attr=MODATTR_MAX_CHECK_ATTEMPTS;
		temp_host->modified_attributes|=attr;

		/* adjust current attempt number if in a hard state */
		if(temp_host->state_type==HARD_STATE && temp_host->current_state!=HOST_UP && temp_host->current_attempt>1)
			temp_host->current_attempt=temp_host->max_attempts;

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE,NEBFLAG_NONE,NEBATTR_NONE,temp_host,cmd,attr,temp_host->modified_attributes,NULL);
#endif

		/* update host status info */
		update_host_status(temp_host,FALSE);
		break;

	case CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS:

		temp_service->max_attempts=max_attempts;
		attr=MODATTR_MAX_CHECK_ATTEMPTS;
		temp_service->modified_attributes|=attr;

		/* adjust current attempt number if in a hard state */
		if(temp_service->state_type==HARD_STATE && temp_service->current_state!=STATE_OK && temp_service->current_attempt>1)
			temp_service->current_attempt=temp_service->max_attempts;

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,NEBFLAG_NONE,NEBATTR_NONE,temp_service,cmd,attr,temp_service->modified_attributes,NULL);
#endif

		/* update service status info */
		update_service_status(temp_service,FALSE);
		break;

	default:
		break;
	        }

#ifdef DEBUG0
	printf("cmd_change_max_attempts() start\n");
#endif

	return OK;
        }
	



/******************************************************************/
/*************** INTERNAL COMMAND IMPLEMENTATIONS  ****************/
/******************************************************************/

/* temporarily disables a service check */
void disable_service_checks(service *svc){
	timed_event *temp_event;

#ifdef DEBUG0
	printf("disable_service_checks() start\n");
#endif

	/* set the attribute modified flag */
	svc->modified_attributes|=MODATTR_ACTIVE_CHECKS_ENABLED;

	/* checks are already disabled */
	if(svc->checks_enabled==FALSE)
		return;

	/* disable the service check... */
	svc->checks_enabled=FALSE;
	svc->should_be_scheduled=FALSE;

	/* remove scheduled checks of this service from the event queue */
	for(temp_event=event_list_low;temp_event!=NULL;temp_event=temp_event->next){
		if(temp_event->event_type==EVENT_SERVICE_CHECK && svc==(service *)temp_event->event_data)
			break;
	        }

	/* we found a check event to remove */
	if(temp_event!=NULL){
		remove_event(temp_event,&event_list_low);
		free(temp_event);
	        }

	/* update the status log to reflect the new service state */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("disable_service_checks() end\n");
#endif

	return;
        }



/* enables a service check */
void enable_service_checks(service *svc){
	time_t preferred_time;
	time_t next_valid_time;

#ifdef DEBUG0
	printf("enable_service_checks() start\n");
#endif

	/* set the attribute modified flag */
	svc->modified_attributes|=MODATTR_ACTIVE_CHECKS_ENABLED;

	/* checks are already enabled */
	if(svc->checks_enabled==TRUE)
		return;

	/* enable the service check... */
	svc->checks_enabled=TRUE;
	svc->should_be_scheduled=TRUE;

	/* services with no check intervals don't get checked */
	if(svc->check_interval==0)
		svc->should_be_scheduled=FALSE;

	/* schedule a check for right now (or as soon as possible) */
	time(&preferred_time);
	if(check_time_against_period(preferred_time,svc->check_period)==ERROR){
		get_next_valid_time(preferred_time,&next_valid_time,svc->check_period);
		svc->next_check=next_valid_time;
	        }
	else
		svc->next_check=preferred_time;

	/* schedule a check if we should */
	if(svc->should_be_scheduled==TRUE)
		schedule_service_check(svc,svc->next_check,FALSE);

	/* update the status log to reflect the new service state */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("enable_service_checks() end\n");
#endif

	return;
        }



/* enable notifications on a program-wide basis */
void enable_all_notifications(void){

#ifdef DEBUG0
	printf("enable_all_notifications() start\n");
#endif

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_NOTIFICATIONS_ENABLED;
	modified_service_process_attributes|=MODATTR_NOTIFICATIONS_ENABLED;

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

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_NOTIFICATIONS_ENABLED;
	modified_service_process_attributes|=MODATTR_NOTIFICATIONS_ENABLED;

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

	/* set the attribute modified flag */
	svc->modified_attributes|=MODATTR_NOTIFICATIONS_ENABLED;

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

	/* set the attribute modified flag */
	svc->modified_attributes|=MODATTR_NOTIFICATIONS_ENABLED;

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

	/* set the attribute modified flag */
	hst->modified_attributes|=MODATTR_NOTIFICATIONS_ENABLED;

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

	/* set the attribute modified flag */
	hst->modified_attributes|=MODATTR_NOTIFICATIONS_ENABLED;

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
void enable_and_propagate_notifications(host *hst, int level, int affect_top_host, int affect_hosts, int affect_services){
	host *temp_host;
	service *temp_service;

#ifdef DEBUG0
	printf("enable_and_propagate_notifications() start\n");
#endif

	/* enable notification for top level host */
	if(affect_top_host==TRUE && level==0)
		enable_host_notifications(hst);

	/* check all child hosts... */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		if(is_host_immediate_child_of_host(hst,temp_host)==TRUE){

			/* recurse... */
			enable_and_propagate_notifications(temp_host,level+1,affect_top_host,affect_hosts,affect_services);

			/* enable notifications for this host */
			if(affect_hosts==TRUE)
				enable_host_notifications(temp_host);

			/* enable notifications for all services on this host... */
			if(affect_services==TRUE){
				for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
					if(!strcmp(temp_service->host_name,temp_host->name))
						enable_service_notifications(temp_service);
				        }
		                }
	                }
	        }

#ifdef DEBUG0
	printf("enable_and_propagate_notifications() end\n");
#endif

	return;
        }


/* disables notifications for all hosts and services "beyond" a given host */
void disable_and_propagate_notifications(host *hst, int level, int affect_top_host, int affect_hosts, int affect_services){
	host *temp_host;
	service *temp_service;

#ifdef DEBUG0
	printf("disable_and_propagate_notifications() start\n");
#endif

	if(hst==NULL)
		return;

	/* disable notifications for top host */
	if(affect_top_host==TRUE && level==0)
		disable_host_notifications(hst);

	/* check all child hosts... */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		if(is_host_immediate_child_of_host(hst,temp_host)==TRUE){

			/* recurse... */
			disable_and_propagate_notifications(temp_host,level+1,affect_top_host,affect_hosts,affect_services);

			/* disable notifications for this host */
			if(affect_hosts==TRUE)
				disable_host_notifications(temp_host);

			/* disable notifications for all services on this host... */
			if(affect_services==TRUE){
				for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
					if(!strcmp(temp_service->host_name,temp_host->name))
						disable_service_notifications(temp_service);
				        }
	                        }
	                }
	        }

#ifdef DEBUG0
	printf("disable_and_propagate_notifications() end\n");
#endif

	return;
        }


/* schedules downtime for all hosts "beyond" a given host */
void schedule_and_propagate_downtime(host *temp_host, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration){
	host *this_host;

#ifdef DEBUG0
	printf("schedule_and_propagate_downtime() start\n");
#endif

	/* check all child hosts... */
	for(this_host=host_list;this_host!=NULL;this_host=this_host->next){

		if(is_host_immediate_child_of_host(temp_host,this_host)==TRUE){

			/* recurse... */
			schedule_and_propagate_downtime(this_host,entry_time,author,comment_data,start_time,end_time,fixed,triggered_by,duration);

			/* schedule downtime for this host */
			schedule_downtime(HOST_DOWNTIME,this_host->name,NULL,entry_time,author,comment_data,start_time,end_time,fixed,triggered_by,duration,NULL);
	                }
	        }

#ifdef DEBUG0
	printf("schedule_and_propagate_downtime() end\n");
#endif

	return;
        }


/* acknowledges a host problem */
void acknowledge_host_problem(host *hst, char *ack_author, char *ack_data, int type, int notify, int persistent){
	time_t current_time;

#ifdef DEBUG0
	printf("acknowledge_host_problem() start\n");
#endif

	/* cannot acknowledge a non-existent problem */
	if(hst->current_state==HOST_UP)
		return;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_acknowledgement_data(NEBTYPE_ACKNOWLEDGEMENT_ADD,NEBFLAG_NONE,NEBATTR_NONE,HOST_ACKNOWLEDGEMENT,(void *)hst,ack_author,ack_data,type,notify,persistent,NULL);
#endif

	/* send out an acknowledgement notification */
	if(notify==TRUE)
		host_notification(hst,NOTIFICATION_ACKNOWLEDGEMENT,ack_author,ack_data);

	/* set the acknowledgement flag */
	hst->problem_has_been_acknowledged=TRUE;

	/* set the acknowledgement type */
	hst->acknowledgement_type=(type==ACKNOWLEDGEMENT_STICKY)?ACKNOWLEDGEMENT_STICKY:ACKNOWLEDGEMENT_NORMAL;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

	/* add a comment for the acknowledgement */
	time(&current_time);
	add_new_host_comment(ACKNOWLEDGEMENT_COMMENT,hst->name,current_time,ack_author,ack_data,persistent,COMMENTSOURCE_INTERNAL,FALSE,(time_t)0,NULL);

#ifdef DEBUG0
	printf("acknowledge_host_problem() end\n");
#endif

	return;
        }


/* acknowledges a service problem */
void acknowledge_service_problem(service *svc, char *ack_author, char *ack_data, int type, int notify, int persistent){
	time_t current_time;

#ifdef DEBUG0
	printf("acknowledge_service_problem() start\n");
#endif

	/* cannot acknowledge a non-existent problem */
	if(svc->current_state==STATE_OK)
		return;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_acknowledgement_data(NEBTYPE_ACKNOWLEDGEMENT_ADD,NEBFLAG_NONE,NEBATTR_NONE,SERVICE_ACKNOWLEDGEMENT,(void *)svc,ack_author,ack_data,type,notify,persistent,NULL);
#endif

	/* send out an acknowledgement notification */
	if(notify==TRUE)
		service_notification(svc,NOTIFICATION_ACKNOWLEDGEMENT,ack_author,ack_data);

	/* set the acknowledgement flag */
	svc->problem_has_been_acknowledged=TRUE;

	/* set the acknowledgement type */
	svc->acknowledgement_type=(type==ACKNOWLEDGEMENT_STICKY)?ACKNOWLEDGEMENT_STICKY:ACKNOWLEDGEMENT_NORMAL;

	/* update the status log with the service info */
	update_service_status(svc,FALSE);

	/* add a comment for the acknowledgement */
	time(&current_time);
	add_new_service_comment(ACKNOWLEDGEMENT_COMMENT,svc->host_name,svc->description,current_time,ack_author,ack_data,persistent,COMMENTSOURCE_INTERNAL,FALSE,(time_t)0,NULL);

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

	/* set the attribute modified flag */
	modified_service_process_attributes|=MODATTR_ACTIVE_CHECKS_ENABLED;

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

	/* set the attribute modified flag */
	modified_service_process_attributes|=MODATTR_ACTIVE_CHECKS_ENABLED;

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

	/* set the attribute modified flag */
	modified_service_process_attributes|=MODATTR_PASSIVE_CHECKS_ENABLED;

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

	/* set the attribute modified flag */
	modified_service_process_attributes|=MODATTR_PASSIVE_CHECKS_ENABLED;

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

	/* set the attribute modified flag */
	svc->modified_attributes|=MODATTR_PASSIVE_CHECKS_ENABLED;

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

	/* set the attribute modified flag */
	svc->modified_attributes|=MODATTR_PASSIVE_CHECKS_ENABLED;

	/* set the passive check flag */
	svc->accept_passive_service_checks=FALSE;

	/* update the status log with the service info */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("disable_passive_service_checks() end\n");
#endif

	return;
        }



/* starts executing host checks */
void start_executing_host_checks(void){

#ifdef DEBUG0
	printf("start_executing_host_checks() start\n");
#endif

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_ACTIVE_CHECKS_ENABLED;

	/* bail out if we're already executing hosts */
	if(execute_host_checks==TRUE)
		return;

	/* set the host check execution flag */
	execute_host_checks=TRUE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("start_executing_host_checks() end\n");
#endif

	return;
        }




/* stops executing host checks */
void stop_executing_host_checks(void){

#ifdef DEBUG0
	printf("stop_executing_host_checks() start\n");
#endif

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_ACTIVE_CHECKS_ENABLED;

	/* bail out if we're already not executing hosts */
	if(execute_host_checks==FALSE)
		return;

	/* set the host check execution flag */
	execute_host_checks=FALSE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("stop_executing_host_checks() end\n");
#endif

	return;
        }



/* starts accepting passive host checks */
void start_accepting_passive_host_checks(void){

#ifdef DEBUG0
	printf("start_accepting_passive_host_checks() start\n");
#endif

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_PASSIVE_CHECKS_ENABLED;

	/* bail out if we're already accepting passive hosts */
	if(accept_passive_host_checks==TRUE)
		return;

	/* set the host check flag */
	accept_passive_host_checks=TRUE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("start_accepting_passive_host_checks() end\n");
#endif

	return;
        }



/* stops accepting passive host checks */
void stop_accepting_passive_host_checks(void){

#ifdef DEBUG0
	printf("stop_accepting_passive_host_checks() start\n");
#endif

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_PASSIVE_CHECKS_ENABLED;

	/* bail out if we're already not accepting passive hosts */
	if(accept_passive_host_checks==FALSE)
		return;

	/* set the host check flag */
	accept_passive_host_checks=FALSE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("stop_accepting_passive_host_checks() end\n");
#endif

	return;
        }



/* enables passive host checks for a particular host */
void enable_passive_host_checks(host *hst){

#ifdef DEBUG0
	printf("enable_passive_host_checks() start\n");
#endif

	/* set the attribute modified flag */
	hst->modified_attributes|=MODATTR_PASSIVE_CHECKS_ENABLED;

	/* set the passive check flag */
	hst->accept_passive_host_checks=TRUE;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("enable_passive_host_checks() end\n");
#endif

	return;
        }



/* disables passive host checks for a particular host */
void disable_passive_host_checks(host *hst){

#ifdef DEBUG0
	printf("disable_passive_host_checks() start\n");
#endif

	/* set the attribute modified flag */
	hst->modified_attributes|=MODATTR_PASSIVE_CHECKS_ENABLED;

	/* set the passive check flag */
	hst->accept_passive_host_checks=FALSE;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("disable_passive_host_checks() end\n");
#endif

	return;
        }


/* enables event handlers on a program-wide basis */
void start_using_event_handlers(void){

#ifdef DEBUG0
	printf("start_using_event_handlers() start\n");
#endif

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_EVENT_HANDLER_ENABLED;
	modified_service_process_attributes|=MODATTR_EVENT_HANDLER_ENABLED;

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

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_EVENT_HANDLER_ENABLED;
	modified_service_process_attributes|=MODATTR_EVENT_HANDLER_ENABLED;

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

	/* set the attribute modified flag */
	svc->modified_attributes|=MODATTR_EVENT_HANDLER_ENABLED;

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

	/* set the attribute modified flag */
	svc->modified_attributes|=MODATTR_EVENT_HANDLER_ENABLED;

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

	/* set the attribute modified flag */
	hst->modified_attributes|=MODATTR_EVENT_HANDLER_ENABLED;

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

	/* set the attribute modified flag */
	hst->modified_attributes|=MODATTR_EVENT_HANDLER_ENABLED;

	/* set the event handler flag */
	hst->event_handler_enabled=FALSE;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("disable_host_event_handler() end\n");
#endif

	return;
        }


/* disables checks of a particular host */
void disable_host_checks(host *hst){
	timed_event *temp_event;

#ifdef DEBUG0
	printf("disable_host_checks() start\n");
#endif

	/* set the attribute modified flag */
	hst->modified_attributes|=MODATTR_ACTIVE_CHECKS_ENABLED;

	/* checks are already disabled */
	if(hst->checks_enabled==FALSE)
		return;

	/* set the host check flag */
	hst->checks_enabled=FALSE;
	hst->should_be_scheduled=FALSE;

	/* remove scheduled checks of this host from the event queue */
	for(temp_event=event_list_low;temp_event!=NULL;temp_event=temp_event->next){
		if(temp_event->event_type==EVENT_HOST_CHECK && hst==(host *)temp_event->event_data)
			break;
	        }

	/* we found a check event to remove */
	if(temp_event!=NULL){
		remove_event(temp_event,&event_list_low);
		free(temp_event);
	        }

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("disable_host_checks() end\n");
#endif

	return;
        }


/* enables checks of a particular host */
void enable_host_checks(host *hst){
	time_t preferred_time;
	time_t next_valid_time;

#ifdef DEBUG0
	printf("enable_host_checks() start\n");
#endif

	/* set the attribute modified flag */
	hst->modified_attributes|=MODATTR_ACTIVE_CHECKS_ENABLED;

	/* checks are already enabled */
	if(hst->checks_enabled==TRUE)
		return;

	/* set the host check flag */
	hst->checks_enabled=TRUE;
	hst->should_be_scheduled=TRUE;

	/* hosts with no check intervals don't get checked */
	if(hst->check_interval==0)
		hst->should_be_scheduled=FALSE;

	/* schedule a check for right now (or as soon as possible) */
	time(&preferred_time);
	if(check_time_against_period(preferred_time,hst->check_period)==ERROR){
		get_next_valid_time(preferred_time,&next_valid_time,hst->check_period);
		hst->next_check=next_valid_time;
	        }
	else
		hst->next_check=preferred_time;

	/* schedule a check if we should */
	if(hst->should_be_scheduled==TRUE)
		schedule_host_check(hst,hst->next_check,FALSE);

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("enable_host_checks() end\n");
#endif

	return;
        }



/* start obsessing over service check results */
void start_obsessing_over_service_checks(void){

#ifdef DEBUG0
        printf("start_obsessing_over_service_checks() start\n");
#endif

	/* set the attribute modified flag */
	modified_service_process_attributes|=MODATTR_OBSESSIVE_HANDLER_ENABLED;

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

	/* set the attribute modified flag */
	modified_service_process_attributes|=MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* set the service obsession flag */
	obsess_over_services=FALSE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("stop_obsessing_over_service_checks() end\n");
#endif

	return;
        }



/* start obsessing over host check results */
void start_obsessing_over_host_checks(void){

#ifdef DEBUG0
        printf("start_obsessing_over_host_checks() start\n");
#endif

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* set the host obsession flag */
	obsess_over_hosts=TRUE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("start_obsessing_over_host_checks() end\n");
#endif

	return;
        }



/* stop obsessing over host check results */
void stop_obsessing_over_host_checks(void){

#ifdef DEBUG0
        printf("stop_obsessing_over_host_checks() start\n");
#endif

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* set the host obsession flag */
	obsess_over_hosts=FALSE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("stop_obsessing_over_host_checks() end\n");
#endif

	return;
        }



/* enables service freshness checking */
void enable_service_freshness_checks(void){

#ifdef DEBUG0
	printf("enable_service_freshness_checks() start\n");
#endif

	/* set the attribute modified flag */
	modified_service_process_attributes|=MODATTR_FRESHNESS_CHECKS_ENABLED;

	/* set the freshness check flag */
	check_service_freshness=TRUE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("enable_service_freshness_checks() end\n");
#endif

	return;
        }


/* disables service freshness checking */
void disable_service_freshness_checks(void){

#ifdef DEBUG0
	printf("disable_service_freshness_checks() start\n");
#endif

	/* set the attribute modified flag */
	modified_service_process_attributes|=MODATTR_FRESHNESS_CHECKS_ENABLED;

	/* set the freshness check flag */
	check_service_freshness=FALSE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("disable_service_freshness_checks() end\n");
#endif

	return;
        }


/* enables host freshness checking */
void enable_host_freshness_checks(void){

#ifdef DEBUG0
	printf("enable_host_freshness_checks() start\n");
#endif

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_FRESHNESS_CHECKS_ENABLED;

	/* set the freshness check flag */
	check_host_freshness=TRUE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("enable_host_freshness_checks() end\n");
#endif

	return;
        }


/* disables host freshness checking */
void disable_host_freshness_checks(void){

#ifdef DEBUG0
	printf("disable_host_freshness_checks() start\n");
#endif

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_FRESHNESS_CHECKS_ENABLED;

	/* set the freshness check flag */
	check_host_freshness=FALSE;

	/* update the status log with the program info */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("disable_host_freshness_checks() end\n");
#endif

	return;
        }


/* enable failure prediction on a program-wide basis */
void enable_all_failure_prediction(void){

#ifdef DEBUG0
	printf("enable_all_failure_prediction() start\n");
#endif

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_FAILURE_PREDICTION_ENABLED;
	modified_service_process_attributes|=MODATTR_FAILURE_PREDICTION_ENABLED;

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

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_FAILURE_PREDICTION_ENABLED;
	modified_service_process_attributes|=MODATTR_FAILURE_PREDICTION_ENABLED;

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

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_PERFORMANCE_DATA_ENABLED;
	modified_service_process_attributes|=MODATTR_PERFORMANCE_DATA_ENABLED;

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

	/* set the attribute modified flag */
	modified_host_process_attributes|=MODATTR_PERFORMANCE_DATA_ENABLED;
	modified_service_process_attributes|=MODATTR_PERFORMANCE_DATA_ENABLED;

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


/* start obsessing over a particular service */
void start_obsessing_over_service(service *svc){

#ifdef DEBUG0
	printf("start_obsessing_over_service() start\n");
#endif

	/* set the attribute modified flag */
	svc->modified_attributes|=MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* set the obsess over service flag */
	svc->obsess_over_service=TRUE;

	/* update the status log with the service info */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("start_obsessing_over_service() end\n");
#endif

	return;
        }


/* stop obsessing over a particular service */
void stop_obsessing_over_service(service *svc){

#ifdef DEBUG0
	printf("stop_obsessing_over_service() start\n");
#endif

	/* set the attribute modified flag */
	svc->modified_attributes|=MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* set the obsess over service flag */
	svc->obsess_over_service=FALSE;

	/* update the status log with the service info */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("stop_obsessing_over_service() end\n");
#endif

	return;
        }


/* start obsessing over a particular host */
void start_obsessing_over_host(host *hst){

#ifdef DEBUG0
	printf("start_obsessing_over_host() start\n");
#endif

	/* set the attribute modified flag */
	hst->modified_attributes|=MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* set the obsess over host flag */
	hst->obsess_over_host=TRUE;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("start_obsessing_over_host() end\n");
#endif

	return;
        }


/* stop obsessing over a particular host */
void stop_obsessing_over_host(host *hst){

#ifdef DEBUG0
	printf("stop_obsessing_over_host() start\n");
#endif

	/* set the attribute modified flag */
	hst->modified_attributes|=MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* set the obsess over host flag */
	hst->obsess_over_host=FALSE;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("stop_obsessing_over_host() end\n");
#endif

	return;
        }


/* sets the current notification number for a specific host */
void set_host_notification_number(host *hst, int num){

#ifdef DEBUG0
	printf("set_host_notification_number() start\n");
#endif

	/* set the notification number */
	hst->current_notification_number=num;

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("set_host_notification_number() end\n");
#endif

	return;
        }


/* sets the current notification number for a specific service */
void set_service_notification_number(service *svc, int num){

#ifdef DEBUG0
	printf("set_service_notification_number() start\n");
#endif

	/* set the notification number */
	svc->current_notification_number=num;

	/* update the status log with the service info */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("set_service_notification_number() end\n");
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
				svc_msg.start_time.tv_sec=temp_pcr->check_time;
				svc_msg.start_time.tv_usec=0;
				svc_msg.finish_time=svc_msg.start_time;

				/* write the service check results... */
				write_svc_message(&svc_msg);
	                        }
		        }

		/* free memory for the passive service check result list */
		this_pcr=passive_check_result_list;
		while(this_pcr!=NULL){
			next_pcr=this_pcr->next;
			free(this_pcr->host_name);
			free(this_pcr->svc_description);
			free(this_pcr->output);
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
		free(this_pcr->host_name);
		free(this_pcr->svc_description);
		free(this_pcr->output);
		free(this_pcr);
		this_pcr=next_pcr;
	        }
	passive_check_result_list=NULL;
	passive_check_result_list_tail=NULL;

#ifdef DEBUG0
	printf("process_passive_service_checks() end\n");
#endif

	return;
        }

