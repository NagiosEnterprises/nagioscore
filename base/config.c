/*****************************************************************************
 *
 * CONFIG.C - Configuration input and verification routines for Nagios
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-03-2002
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
#include "../common/objects.h"
#include "nagios.h"


extern char	*log_file;
extern char     *command_file;
extern char     *temp_file;
extern char     *lock_file;
extern char	*log_archive_path;
extern char     *auth_file;
extern char	*p1_file;

extern char     *nagios_user;
extern char     *nagios_group;

extern char     *macro_admin_email;
extern char     *macro_admin_pager;
extern char     *macro_user[MAX_USER_MACROS];

extern char     *global_host_event_handler;
extern char     *global_service_event_handler;

extern char     *ocsp_command;

extern char     *illegal_object_chars;
extern char     *illegal_output_chars;

extern int	use_syslog;
extern int      log_notifications;
extern int      log_service_retries;
extern int      log_host_retries;
extern int      log_event_handlers;
extern int      log_external_commands;
extern int      log_passive_checks;

extern int      service_check_timeout;
extern int      host_check_timeout;
extern int      event_handler_timeout;
extern int      notification_timeout;
extern int      ocsp_timeout;

extern int      log_initial_states;

extern int      daemon_mode;
extern int      verify_config;

extern int      sleep_time;
extern int      interval_length;
extern int      inter_check_delay_method;
extern int      interleave_factor_method;

extern sched_info scheduling_info;

extern int      max_child_process_time;

extern int      max_parallel_service_checks;

extern int      command_check_interval;
extern int      service_check_reaper_interval;
extern int      freshness_check_interval;

extern int      check_external_commands;
extern int      check_orphaned_services;
extern int      check_service_freshness;

extern int      use_aggressive_host_checking;

extern int      soft_state_dependencies;

extern int      retain_state_information;
extern int      retention_update_interval;
extern int      use_retained_program_state;

extern int      log_rotation_method;

extern int      enable_notifications;
extern int      execute_service_checks;
extern int      accept_passive_service_checks;
extern int      enable_event_handlers;
extern int      obsess_over_services;
extern int      enable_failure_prediction;

extern int      aggregate_status_updates;
extern int      status_update_interval;

extern int      time_change_threshold;

extern int      process_performance_data;

extern int      enable_flap_detection;

extern double   low_service_flap_threshold;
extern double   high_service_flap_threshold;
extern double   low_host_flap_threshold;
extern double   high_host_flap_threshold;

extern int      date_format;

extern int      max_embedded_perl_calls;

extern contact		*contact_list;
extern contactgroup	*contactgroup_list;
extern host		**host_list;
extern hostgroup	*hostgroup_list;
extern service		**service_list;
extern notification     *notification_list;
extern command          *command_list;
extern timeperiod       *timeperiod_list;
extern serviceescalation *serviceescalation_list;
extern hostgroupescalation *hostgroupescalation_list;
extern servicedependency *servicedependency_list;
extern hostdependency   *hostdependency_list;
extern hostescalation   *hostescalation_list;





/******************************************************************/
/************** CONFIGURATION INPUT FUNCTIONS *********************/
/******************************************************************/

/* read all configuration data */
int read_all_config_data(char *main_config_file){
	int result=OK;

#ifdef DEBUG0
	printf("read_all_config_data() start\n");
#endif

	/* read the main config file */
	result=read_main_config_file(main_config_file);
	if(result!=OK)
		return ERROR;

	/* read in all host configuration data from external sources */
	result=read_object_config_data(main_config_file,READ_TIMEPERIODS|READ_HOSTS|READ_HOSTGROUPS|READ_CONTACTS|READ_CONTACTGROUPS|READ_SERVICES|READ_COMMANDS|READ_SERVICEESCALATIONS|READ_HOSTGROUPESCALATIONS|READ_SERVICEDEPENDENCIES|READ_HOSTDEPENDENCIES|READ_HOSTESCALATIONS);
	if(result!=OK)
		return ERROR;

#ifdef DEBUG0
	printf("read_all_config_data() end\n");
#endif

	return OK;
        }


/* process the main configuration file */
int read_main_config_file(char *main_config_file){
	char input[MAX_INPUT_BUFFER];
	char variable[MAX_INPUT_BUFFER];
	char value[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	char error_message[MAX_INPUT_BUFFER];
	char *temp;
	FILE *fp;
	int current_line=0;
	int error=FALSE;
	int command_check_interval_is_seconds=FALSE;

#ifdef DEBUG0
	printf("read_main_config_file() start\n");
#endif
#ifdef DEBUG1
	printf("\tConfig file: %s\n",main_config_file);
#endif

	/* open the config file for reading */
	fp=fopen(main_config_file,"r");
	if(fp==NULL){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Cannot open main configuration file '%s' for reading!",main_config_file);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		return ERROR;
		}

	/* process all lines in the config file */
	for(current_line=1,fgets(input,MAX_INPUT_BUFFER-1,fp);!feof(fp);current_line++,fgets(input,MAX_INPUT_BUFFER-1,fp)){

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]=='\x0' || input[0]=='\n' || input[0]=='\r')
			continue;

		strip(input);

		/* skip external data directives */
		if(strstr(input,"x")==input)
			continue;

#ifdef DEBUG1
		printf("\tLine %d = '%s'\n",current_line,input);
#endif

		/* get the variable name */
		temp=my_strtok(input,"=");

		/* if there is no variable name, return error */
		if(temp==NULL){
			strcpy(error_message,"NULL variable");
			error=TRUE;
			break;
			}

		/* else the variable is good */
		strncpy(variable,temp,sizeof(variable));
		variable[sizeof(variable)-1]='\x0';

		/* get the value */
		temp=my_strtok(NULL,"=");

		/* if no value exists, return error */
		if(temp==NULL){
			strcpy(error_message,"NULL value");
			error=TRUE;
			break;
			}

		/* else the value is good */
		strncpy(value,temp,sizeof(value));
		value[sizeof(value)-1]='\x0';
		strip(value);

		/* process the variable/value */

		if(!strcmp(variable,"resource_file")){
#ifdef DEBUG1
			printf("\t\tprocessing resource file '%s'\n");
#endif
			read_resource_file(value);
		        }
		else if(!strcmp(variable,"log_file")){
			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"Log file is too long");
				error=TRUE;
				break;
				}

			if(log_file!=NULL)
				free(log_file);
			log_file=(char *)strdup(value);
			strip(log_file);

#ifdef DEBUG1
			printf("\t\tlog_file set to '%s'\n",log_file);
#endif
			}
		else if(!strcmp(variable,"command_file")){
			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"Command file is too long");
				error=TRUE;
				break;
				}

			if(command_file!=NULL)
				free(command_file);
			command_file=(char *)strdup(value);
			strip(command_file);

#ifdef DEBUG1
			printf("\t\tcommand_file set to '%s'\n",command_file);
#endif
			}
		else if(!strcmp(variable,"temp_file")){
			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"Temp file is too long");
				error=TRUE;
				break;
				}

			if(temp_file!=NULL)
				free(temp_file);
			temp_file=(char *)strdup(value);
			strip(temp_file);

#ifdef DEBUG1
			printf("\t\ttemp_file set to '%s'\n",temp_file);
#endif
			}
		else if(!strcmp(variable,"lock_file")){
			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"Lock file is too long");
				error=TRUE;
				break;
				}

			if(lock_file!=NULL)
				free(lock_file);
			lock_file=(char *)strdup(value);
			strip(lock_file);

#ifdef DEBUG1
			printf("\t\tlock_file set to '%s'\n",lock_file);
#endif
			}
		else if(!strcmp(variable,"global_host_event_handler")){
			if(global_host_event_handler!=NULL)
				free(global_host_event_handler);
			global_host_event_handler=(char *)strdup(value);
			if(global_host_event_handler==NULL){
				strcpy(error_message,"Could not allocate memory for global host event handler");
				error=TRUE;
				break;
			        }
			strip(global_host_event_handler);

#ifdef DEBUG1
			printf("\t\tglobal_host_event_handler set to '%s'\n",global_host_event_handler);
#endif
		        }
		else if(!strcmp(variable,"global_service_event_handler")){
			if(global_service_event_handler!=NULL)
				free(global_service_event_handler);
			global_service_event_handler=(char *)strdup(value);
			if(global_service_event_handler==NULL){
				strcpy(error_message,"Could not allocate memory for global service event handler");
				error=TRUE;
				break;
			        }

			strip(global_service_event_handler);

#ifdef DEBUG1
			printf("\t\tglobal_service_event_handler set to '%s'\n",global_service_event_handler);
#endif
		        }
		else if(!strcmp(variable,"ocsp_command")){
			if(ocsp_command!=NULL)
				free(ocsp_command);
			ocsp_command=(char *)strdup(value);
			if(ocsp_command==NULL){
				strcpy(error_message,"Could not allocate memory for obsessive compulsive service processor command");
				error=TRUE;
				break;
			        }

			strip(ocsp_command);

#ifdef DEBUG1
			printf("\t\tocsp_command set to '%s'\n",ocsp_command);
#endif
		        }
		else if(!strcmp(variable,"nagios_user")){
			if(nagios_user!=NULL)
				free(nagios_user);
			nagios_user=(char *)strdup(value);
			if(nagios_user==NULL){
				strcpy(error_message,"Could not allocate memory for nagios user");
				error=TRUE;
				break;
			        }

			strip(nagios_user);

#ifdef DEBUG1
			printf("\t\tnagios_user set to '%s'\n",nagios_user);
#endif
		        }
		else if(!strcmp(variable,"nagios_group")){
			if(nagios_group!=NULL)
				free(nagios_group);
			nagios_group=(char *)strdup(value);
			if(nagios_group==NULL){
				strcpy(error_message,"Could not allocate memory for nagios group");
				error=TRUE;
				break;
			        }

			strip(nagios_group);

#ifdef DEBUG1
			printf("\t\tnagios_group set to '%s'\n",nagios_group);
#endif
		        }
		else if(!strcmp(variable,"admin_email")){
			if(macro_admin_email!=NULL)
				free(macro_admin_email);
			macro_admin_email=(char *)strdup(value);
			if(macro_admin_email==NULL){
				strcpy(error_message,"Could not allocate memory for admin email address");
				error=TRUE;
				break;
			        }

			strip(macro_admin_email);

#ifdef DEBUG1
			printf("\t\tmacro_admin_email set to '%s'\n",macro_admin_email);
#endif
		        }
		else if(!strcmp(variable,"admin_pager")){
			if(macro_admin_pager!=NULL)
				free(macro_admin_pager);
			macro_admin_pager=(char *)strdup(value);
			if(macro_admin_pager==NULL){
				strcpy(error_message,"Could not allocate memory for admin pager");
				error=TRUE;
				break;
			        }

			strip(macro_admin_pager);

#ifdef DEBUG1
			printf("\t\tmacro_admin_pager set to '%s'\n",macro_admin_pager);
#endif
		        }
		else if(!strcmp(variable,"use_syslog")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for use_syslog");
				error=TRUE;
				break;
				}

			strip(value);
			use_syslog=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tuse_syslog set to %s\n",(use_syslog==TRUE)?"TRUE":"FALSE");
#endif
			}
		else if(!strcmp(variable,"log_notifications")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for log_notifications");
				error=TRUE;
				break;
			        }

			strip(value);
			log_notifications=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tlog_notifications set to %s\n",(log_notifications==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"log_service_retries")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for log_service_retries");
				error=TRUE;
				break;
			        }

			strip(value);
			log_service_retries=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tlog_service_retries set to %s\n",(log_service_retries==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"log_host_retries")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for log_host_retries");
				error=TRUE;
				break;
			        }

			strip(value);
			log_host_retries=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tlog_host_retries set to %s\n",(log_host_retries==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"log_event_handlers")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for log_event_handlers");
				error=TRUE;
				break;
			        }

			strip(value);
			log_event_handlers=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tlog_event_handlers set to %s\n",(log_event_handlers==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"log_external_commands")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for log_external_commands");
				error=TRUE;
				break;
			        }

			strip(value);
			log_external_commands=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tlog_external_commands set to %s\n",(log_external_commands==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"log_passive_service_checks")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for log_passive_service_checks");
				error=TRUE;
				break;
			        }

			strip(value);
			log_passive_checks=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tlog_passive_service_checks set to %s\n",(log_passive_checks==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"log_initial_states")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for log_initial_states");
				error=TRUE;
				break;
			        }

			strip(value);
			log_initial_states=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tlog_initial_states set to %s\n",(log_initial_states==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"retain_state_information")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for retain_state_information");
				error=TRUE;
				break;
			        }

			strip(value);
			retain_state_information=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tretain_state_information set to %s\n",(retain_state_information==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"retention_update_interval")){
			strip(value);
			retention_update_interval=atoi(value);
			if(retention_update_interval<0){
				strcpy(error_message,"Illegal value for retention_update_interval");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tretention_update_interval set to %d\n",retention_update_interval);
#endif
		        }
		else if(!strcmp(variable,"use_retained_program_state")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for use_retained_program_state");
				error=TRUE;
				break;
			        }

			strip(value);
			use_retained_program_state=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tuse_retained_program_state set to %s\n",(use_retained_program_state==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"obsess_over_services")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for obsess_over_services");
				error=TRUE;
				break;
			        }

			strip(value);
			obsess_over_services=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tobsess_over_services set to %s\n",(obsess_over_services==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"service_check_timeout")){
			strip(value);
			service_check_timeout=atoi(value);

			if(service_check_timeout<=0){
				strcpy(error_message,"Illegal value for service_check_timeout");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tservice_check_timeout set to %d\n",service_check_timeout);
#endif
		        }
		else if(!strcmp(variable,"host_check_timeout")){
			strip(value);
			host_check_timeout=atoi(value);

			if(host_check_timeout<=0){
				strcpy(error_message,"Illegal value for host_check_timeout");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\thost_check_timeout set to %d\n",host_check_timeout);
#endif
		        }
		else if(!strcmp(variable,"event_handler_timeout")){
			strip(value);
			event_handler_timeout=atoi(value);

			if(event_handler_timeout<=0){
				strcpy(error_message,"Illegal value for event_handler_timeout");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tevent_handler_timeout set to %d\n",event_handler_timeout);
#endif
		        }
		else if(!strcmp(variable,"notification_timeout")){
			strip(value);
			notification_timeout=atoi(value);

			if(notification_timeout<=0){
				strcpy(error_message,"Illegal value for notification_timeout");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tnotification_timeout set to %d\n",notification_timeout);
#endif
		        }
		else if(!strcmp(variable,"ocsp_timeout")){
			strip(value);
			ocsp_timeout=atoi(value);

			if(ocsp_timeout<=0){
				strcpy(error_message,"Illegal value for ocsp_timeout");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tocsp_timeout set to %d\n",ocsp_timeout);
#endif
		        }
		else if(!strcmp(variable,"use_agressive_host_checking") || !strcmp(variable,"use_aggressive_host_checking")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for use_aggressive_host_checking");
				error=TRUE;
				break;
			        }

			strip(value);
			use_aggressive_host_checking=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tuse_aggressive_host_checking set to %s\n",(use_aggressive_host_checking==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"soft_state_dependencies")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for soft_state_dependencies");
				error=TRUE;
				break;
			        }

			strip(value);
			soft_state_dependencies=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tsoft_state_dependencies set to %s\n",(soft_state_dependencies==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"log_rotation_method")){
			if(!strcmp(value,"n"))
				log_rotation_method=LOG_ROTATION_NONE;
			else if(!strcmp(value,"h"))
				log_rotation_method=LOG_ROTATION_HOURLY;
			else if(!strcmp(value,"d"))
				log_rotation_method=LOG_ROTATION_DAILY;
			else if(!strcmp(value,"w"))
				log_rotation_method=LOG_ROTATION_WEEKLY;
			else if(!strcmp(value,"m"))
				log_rotation_method=LOG_ROTATION_MONTHLY;
			else{
				strcpy(error_message,"Illegal value for log_rotation_method");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tlog_rotation_method set to %d\n",log_rotation_method);
#endif
		        }
		else if(!strcmp(variable,"log_archive_path")){
			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"Log archive path too long");
				error=TRUE;
				break;
				}

			log_archive_path=(char *)strdup(value);
			strip(log_archive_path);

#ifdef DEBUG1
			printf("\t\tlog_archive_path set to '%s'\n",log_archive_path);
#endif
			}
		else if(!strcmp(variable,"enable_event_handlers")){
			strip(value);
			enable_event_handlers=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tenable_event_handlers set to %s\n",(enable_event_handlers==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"enable_notifications")){
			strip(value);
			enable_notifications=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tenable_notifications set to %s\n",(enable_notifications==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"execute_service_checks")){
			strip(value);
			execute_service_checks=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\texecute_service_checks set to %s\n",(execute_service_checks==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"accept_passive_service_checks")){
			strip(value);
			accept_passive_service_checks=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\taccept_passive_service_checks set to %s\n",(accept_passive_service_checks==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"inter_check_delay_method")){
			if(!strcmp(value,"n"))
				inter_check_delay_method=ICD_NONE;
			else if(!strcmp(value,"d"))
				inter_check_delay_method=ICD_DUMB;
			else if(!strcmp(value,"s"))
				inter_check_delay_method=ICD_SMART;
			else{
				inter_check_delay_method=ICD_USER;
				scheduling_info.inter_check_delay=strtod(value,NULL);
				if(scheduling_info.inter_check_delay<=0.0){
					strcpy(error_message,"Illegal value for inter_check_delay_method");
					error=TRUE;
					break;
				        }
			        }
#ifdef DEBUG1
			printf("\t\tinter_check_delay_method set to %d\n",inter_check_delay_method);
#endif
		        }
		else if(!strcmp(variable,"service_interleave_factor")){
			if(!strcmp(value,"s"))
				interleave_factor_method=ILF_SMART;
			else{
				interleave_factor_method=ILF_USER;
				scheduling_info.interleave_factor=atoi(value);
				if(scheduling_info.interleave_factor<1)
					scheduling_info.interleave_factor=1;
			        }
		        }
		else if(!strcmp(variable,"max_concurrent_checks")){
			strip(value);
			max_parallel_service_checks=atoi(value);
			if(max_parallel_service_checks<0){
				strcpy(error_message,"Illegal value for max_concurrent_checks");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tmax_parallel_service_checks set to %d\n",max_parallel_service_checks);
#endif
		        }
		else if(!strcmp(variable,"service_reaper_frequency")){
			strip(value);
			service_check_reaper_interval=atoi(value);
			if(service_check_reaper_interval<1){
				strcpy(error_message,"Illegal value for service_reaper_frequency");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tservice_check_reaper_interval set to %d\n",service_check_reaper_interval);
#endif
		        }
		else if(!strcmp(variable,"sleep_time")){
			strip(value);
			sleep_time=atoi(value);
			if(sleep_time<1){
				strcpy(error_message,"Illegal value for sleep_time");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tsleep_time set to %d\n",sleep_time);
#endif
		        }
		else if(!strcmp(variable,"interval_length")){
			strip(value);
			interval_length=atoi(value);
			if(interval_length<1){
				strcpy(error_message,"Illegal value for interval_length");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tinterval_length set to %d\n",interval_length);
#endif
		        }
		else if(!strcmp(variable,"check_external_commands")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for check_external_commands");
				error=TRUE;
				break;
			        }

			strip(value);
			check_external_commands=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tcheck_external_commands set to %s\n",(check_external_commands==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"command_check_interval")){
			strip(value);
			command_check_interval_is_seconds=(strstr(value,"s"))?TRUE:FALSE;
			command_check_interval=atoi(value);
			if(command_check_interval<-1 || command_check_interval==0){
				strcpy(error_message,"Illegal value for command_check_interval");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tcommand_check_interval set to %d\n",command_check_interval);
#endif
		        }
		else if(!strcmp(variable,"check_for_orphaned_services")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for check_for_orphaned_services");
				error=TRUE;
				break;
			        }

			strip(value);
			check_orphaned_services=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tcheck_orphaned_services set to %s\n",(check_orphaned_services==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"check_service_freshness")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for check_service_freshness");
				error=TRUE;
				break;
			        }

			strip(value);
			check_service_freshness=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tcheck_service_freshness set to %s\n",(check_service_freshness==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"freshness_check_interval")){
			strip(value);
			freshness_check_interval=atoi(value);
			if(freshness_check_interval<=0){
				strcpy(error_message,"Illegal value for freshness_check_interval");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tfreshness_check_interval set to %d\n",freshness_check_interval);
#endif
		        }
		else if(!strcmp(variable,"aggregate_status_updates")){
			strip(value);
			aggregate_status_updates=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\taggregate_status_updates to %s\n",(aggregate_status_updates==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"status_update_interval")){
			strip(value);
			status_update_interval=atoi(value);
			if(status_update_interval<5){
				strcpy(error_message,"Illegal value for status_update_interval");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tstatus_update_interval set to %d\n",status_update_interval);
#endif
		        }

		else if(!strcmp(variable,"time_change_threshold")){
			strip(value);
			time_change_threshold=atoi(value);

			if(time_change_threshold<=5){
				strcpy(error_message,"Illegal value for time_change_threshold");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\ttime_change_threshold set to %d\n",time_change_threshold);
#endif
		        }
		else if(!strcmp(variable,"process_performance_data")){
			strip(value);
			process_performance_data=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tprocess_performance_data set to %s\n",(process_performance_data==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"enable_flap_detection")){
			strip(value);
			enable_flap_detection=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tenable_flap_detection set to %s\n",(enable_flap_detection==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"enable_failure_prediction")){
			strip(value);
			enable_failure_prediction=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tenable_failure_prediction set to %s\n",(enable_failure_prediction==TRUE)?"TRUE":"FALSE");
#endif
		        }
		else if(!strcmp(variable,"low_service_flap_threshold")){
			low_service_flap_threshold=strtod(value,NULL);
			if(low_service_flap_threshold<=0.0 || low_service_flap_threshold>=100.0){
				strcpy(error_message,"Illegal value for low_service_flap_threshold");
				error=TRUE;
				break;
			        }
#ifdef DEBUG1
			printf("\t\tlow_service_flap_threshold set to %lf\n",low_service_flap_threshold);
#endif
		        }
		else if(!strcmp(variable,"high_service_flap_threshold")){
			high_service_flap_threshold=strtod(value,NULL);
			if(high_service_flap_threshold<=0.0 ||  high_service_flap_threshold>100.0){
				strcpy(error_message,"Illegal value for high_service_flap_threshold");
				error=TRUE;
				break;
			        }
#ifdef DEBUG1
			printf("\t\thigh_service_flap_threshold set to %lf\n",high_service_flap_threshold);
#endif
		        }
		else if(!strcmp(variable,"low_host_flap_threshold")){
			low_host_flap_threshold=strtod(value,NULL);
			if(low_host_flap_threshold<=0.0 || low_host_flap_threshold>=100.0){
				strcpy(error_message,"Illegal value for low_host_flap_threshold");
				error=TRUE;
				break;
			        }
#ifdef DEBUG1
			printf("\t\tlow_host_flap_threshold set to %lf\n",low_host_flap_threshold);
#endif
		        }
		else if(!strcmp(variable,"high_host_flap_threshold")){
			high_host_flap_threshold=strtod(value,NULL);
			if(high_host_flap_threshold<=0.0 || high_host_flap_threshold>100.0){
				strcpy(error_message,"Illegal value for high_host_flap_threshold");
				error=TRUE;
				break;
			        }
#ifdef DEBUG1
			printf("\t\thigh_host_flap_threshold set to %lf\n",high_host_flap_threshold);
#endif
		        }
		else if(!strcmp(variable,"date_format")){
			strip(value);
			if(!strcmp(value,"euro"))
				date_format=DATE_FORMAT_EURO;
			else if(!strcmp(value,"iso8601"))
				date_format=DATE_FORMAT_ISO8601;
			else if(!strcmp(value,"strict-iso8601"))
				date_format=DATE_FORMAT_STRICT_ISO8601;
			else
				date_format=DATE_FORMAT_US;
#ifdef DEBUG1
			printf("\t\tdate_format set to %d\n",date_format);
#endif
		        }
		else if(!strcmp(variable,"max_embedded_perl_calls")){
			strip(value);
			max_embedded_perl_calls=atoi(value);

			if(max_embedded_perl_calls<=0){
				strcpy(error_message,"Illegal value for max_embedded_perl_calls");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tmax_embedded_perl_calls set to %d\n",max_embedded_perl_calls);
#endif
		        }
		else if(!strcmp(variable,"p1_file")){
			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"P1 file is too long");
				error=TRUE;
				break;
				}

			if(p1_file!=NULL)
				free(p1_file);
			p1_file=(char *)strdup(value);
			strip(p1_file);

#ifdef DEBUG1
			printf("\t\tp1_file set to '%s'\n",p1_file);
#endif
			}
		else if(!strcmp(variable,"illegal_object_name_chars")){
			illegal_object_chars=strdup(value);
#ifdef DEBUG1
			printf("\t\tillegal_object_name_chars set to '%s'\n",illegal_object_chars);
#endif
		        }
		else if(!strcmp(variable,"illegal_macro_output_chars")){
			illegal_output_chars=strdup(value);
#ifdef DEBUG1
			printf("\t\tillegal_macro_output_chars set to '%s'\n",illegal_output_chars);
#endif
		        }

		/*** AUTH_FILE VARIABLE USED BY EMBEDDED PERL INTERPRETER ***/
		else if(!strcmp(variable,"auth_file")){
			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"Auth file is too long");
				error=TRUE;
				break;
			        }

			if(auth_file!=NULL)
				free(auth_file);
			auth_file=(char *)strdup(value);
			strip(auth_file);
#ifdef DEBUG1
			printf("\t\tauth_file set to '%s'\n",auth_file);
#endif
		        }

		/* ignore old variables */
		else if(!strcmp(variable,"status_file"))
			continue;
		else if(!strcmp(variable,"comment_file"))
			continue;
		else if(!strcmp(variable,"downtime_file"))
			continue;
		else if(!strcmp(variable,"perfdata_timeout"))
			continue;
		else if(!strcmp(variable,"host_perfdata_command"))
			continue;
		else if(!strcmp(variable,"service_perfdata_command"))
			continue;
		else if(strstr(input,"cfg_file=")==input || strstr(input,"cfg_dir=")==input)
			continue;
		else if(strstr(input,"state_retention_file=")==input)
			continue;

		/* we don't know what this variable is... */
		else{
			strcpy(error_message,"UNKNOWN VARIABLE");
			error=TRUE;
			break;
			}

		}

	/* adjust values */
	if(command_check_interval_is_seconds==FALSE && command_check_interval!=-1)
		command_check_interval*=interval_length;

	if(error==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in configuration file '%s' - Line %d (%s)",main_config_file,current_line,error_message);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		return ERROR;
	        }

	fclose(fp);

	/* make sure a log file has been specified */
	strip(log_file);
	if(!strcmp(log_file,"")){
		if(daemon_mode==FALSE)
			printf("Error: Log file is not specified anywhere in main config file '%s'!\n",main_config_file);
		return ERROR;
		}

#ifdef DEBUG0
	printf("read_main_config_file() end\n");
#endif

	return OK;
	}



/* processes macros in resource file */
int read_resource_file(char *resource_file){
	char temp_buffer[MAX_INPUT_BUFFER];
	char input[MAX_INPUT_BUFFER];
	char variable[MAX_INPUT_BUFFER];
	char value[MAX_INPUT_BUFFER];
	char *temp_ptr;
	FILE *fp;
	int current_line=1;
	int error=FALSE;
	int user_index=0;

#ifdef DEBUG0
	printf("read_resource_file() start\n");
#endif

#ifdef DEBUG1
	printf("processing resource file '%s'\n",resource_file);
#endif

	fp=fopen(resource_file,"r");
	if(fp==NULL){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Cannot open resource file '%s' for reading!",resource_file);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		return ERROR;
		}

	/* process all lines in the resource file */
	for(current_line=1,fgets(input,MAX_INPUT_BUFFER-1,fp);!feof(fp);current_line++,fgets(input,MAX_INPUT_BUFFER-1,fp)){

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]=='\x0' || input[0]=='\n' || input[0]=='\r')
			continue;

		strip(input);

		/* get the variable name */
		temp_ptr=my_strtok(input,"=");

		/* if there is no variable name, return error */
		if(temp_ptr==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: NULL variable - Line %d of resource file '%s'",current_line,resource_file);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
			error=TRUE;
			break;
			}

		/* else the variable is good */
		strncpy(variable,temp_ptr,sizeof(variable));
		variable[sizeof(variable)-1]='\x0';

		/* get the value */
		temp_ptr=my_strtok(NULL,"=");

		/* if no value exists, return error */
		if(temp_ptr==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: NULL variable value - Line %d of resource file '%s'",current_line,resource_file);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
			error=TRUE;
			break;
			}

		/* else the value is good */
		strncpy(value,temp_ptr,sizeof(value));
		value[sizeof(value)-1]='\x0';
		strip(value);

		/* what should we do with the variable/value pair? */

		/* check for macro declarations */
		if(variable[0]=='$' && variable[strlen(variable)-1]=='$'){
			
			/* $USERx$ macro declarations */
			if(strstr(variable,"$USER")==variable  && strlen(variable)>5){
				user_index=atoi(variable+5)-1;
				if(user_index>=0 && user_index<MAX_USER_MACROS){
					if(macro_user[user_index]!=NULL)
						free(macro_user[user_index]);
					macro_user[user_index]=(char *)malloc(strlen(value)+1);
					if(macro_user[user_index]!=NULL){
						strcpy(macro_user[user_index],value);
#ifdef DEBUG1
						printf("\t\t$USER%d$ set to '%s'\n",user_index+1,macro_user[user_index]);
#endif
					        }
				        }
			        }
		        }
	        }

	fclose(fp);

	if(error==TRUE)
		return ERROR;

#ifdef DEBUG0
	printf("read_resource_file() end\n");
#endif

	return OK;
        }






/****************************************************************/
/**************** CONFIG VERIFICATION FUNCTIONS *****************/
/****************************************************************/

/* do a pre-flight check to make sure object relationships make sense */
int pre_flight_check(void){
	contact *temp_contact;
	commandsmember *temp_commandsmember;
	contactgroup *temp_contactgroup;
	contactgroupmember *temp_contactgroupmember;
	contactgroupsmember *temp_contactgroupsmember;
	host *temp_host;
	host *temp_host2;
	hostsmember *temp_hostsmember;
	hostgroup *temp_hostgroup;
	hostgroupmember *temp_hostgroupmember;
	service *temp_service;
	service *temp_service2;
	command *temp_command;
	timeperiod *temp_timeperiod;
	serviceescalation *temp_se;
	hostescalation *temp_he;
	hostgroupescalation *temp_hge;
	servicedependency *temp_sd;
	servicedependency *temp_sd2;
	hostdependency *temp_hd;
	char temp_buffer[MAX_INPUT_BUFFER];
	char *temp_command_name="";
	int found;
	int result=OK;
	int total_objects=0;
	int warnings=0;
	int errors=0;

#ifdef DEBUG0
	printf("pre_flight_check() start\n");
#endif



	/*****************************************/
	/* check each service...                 */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking services...\n");
	if(service_list==NULL){
		snprintf(temp_buffer,sizeof(temp_buffer),"Error: There are no services defined!");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
		errors++;
	        }
	total_objects=0;
	move_first_service();
	while(temp_service=get_next_service()) {
		total_objects++;
		found=FALSE;

		/* check for a valid host */
		temp_host = find_host(temp_service->host_name);

		/* we couldn't find an associated host! */
		if(!temp_host){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Host '%s' specified in service '%s' not defined anywhere!",temp_service->host_name,temp_service->description);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
			}

		/* check the event handler command */
		if(temp_service->event_handler!=NULL){
			temp_command=find_command(temp_service->event_handler,NULL);
			if(temp_command==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Event handler command '%s' specified in service '%s' for host '%s' not defined anywhere",temp_service->event_handler,temp_service->description,temp_service->host_name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
		        }

		/* check the service check_command */
		strncpy(temp_buffer,temp_service->service_check_command,sizeof(temp_buffer));
		temp_buffer[sizeof(temp_buffer)-1]='\x0';

		/* raw command lines shouldn't be checked */
		if(temp_buffer[0]=='"')
			continue;

		/* get the command name, leave any arguments behind */
		temp_command_name=my_strtok(temp_buffer,"!");

		temp_command=find_command(temp_command_name,NULL);
		if(temp_command==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Service check command '%s' specified in service '%s' for host '%s' not defined anywhere!",temp_command_name,temp_service->description,temp_service->host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }		

		/* check for sane recovery options */
		if(temp_service->notify_on_recovery==TRUE && temp_service->notify_on_warning==FALSE && temp_service->notify_on_critical==FALSE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Recovery notification option in service '%s' for host '%s' doesn't make any sense - specify warning and/or critical options as well",temp_service->description,temp_service->host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }

		/* reset the found flag */
		found=FALSE;

		/* check for valid contactgroups */
		for(temp_contactgroupsmember=temp_service->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){

			temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name,NULL);

			if(temp_contactgroup==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Contact group '%s' specified in service '%s' for host '%s' is not defined anywhere!",temp_contactgroupsmember->group_name,temp_service->description,temp_service->host_name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
			}

		/* check to see if there is at least one contact group */
		if(temp_service->contact_groups==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Service '%s' on host '%s'  has no default contact group(s) defined!",temp_service->description,temp_service->host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }

		/* verify service check timeperiod */
		if(temp_service->check_period==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Service '%s' on host '%s'  has no check time period defined!",temp_service->description,temp_service->host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }
		else{
		        temp_timeperiod=find_timeperiod(temp_service->check_period,NULL);
			if(temp_timeperiod==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Check period '%s' specified for service '%s' on host '%s' is not defined anywhere!",temp_service->check_period,temp_service->description,temp_service->host_name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
		        }

		/* check service notification timeperiod */
		if(temp_service->notification_period==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Service '%s' on host '%s' has no notification time period defined!",temp_service->description,temp_service->host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }

		else{
		        temp_timeperiod=find_timeperiod(temp_service->notification_period,NULL);
			if(temp_timeperiod==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Notification period '%s' specified for service '%s' on host '%s' is not defined anywhere!",temp_service->notification_period,temp_service->description,temp_service->host_name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
		        }

		/* see if the notification interval is less than the check interval */
		if(temp_service->notification_interval<temp_service->check_interval && temp_service->notification_interval!=0){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Service '%s' on host '%s'  has a notification interval less than its check interval!  Notifications are only re-sent after checks are made, so the effective notification interval will be that of the check interval.",temp_service->description,temp_service->host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }

		/* check for illegal characters in service description */
		if(contains_illegal_object_chars(temp_service->description)==TRUE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: The description string for service '%s' on host '%s' contains one or more illegal characters.",temp_service->description,temp_service->host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
	        }

	if(verify_config==TRUE)
		printf("\tChecked %d services.\n",total_objects);


#ifdef DEBUG1
	printf("\tCompleted service verification checks\n");
#endif



	/*****************************************/
	/* check all hosts...                    */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking hosts...\n");
	if(host_list==NULL){
		snprintf(temp_buffer,sizeof(temp_buffer),"Error: There are no hosts defined!");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
		errors++;
	        }
	total_objects=0;
	move_first_host();
	while(temp_host = get_next_host()) {
		total_objects++;

		found=FALSE;

		/* make sure each host has at least one service associated with it */
		if(find_all_services_by_host(temp_host->name)) {
			if(get_next_service_by_host()) {
				found=TRUE;
				}
			}

		/* we couldn't find a service associated with this host! */
		if(found==FALSE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Host '%s' has no services associated with it!",temp_host->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
			}

		found=FALSE;

		/* make sure each host is a member of at least one hostgroup */
		for(temp_hostgroup=hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
			temp_hostgroupmember=find_hostgroupmember(temp_host->name,temp_hostgroup,NULL);
			if(temp_hostgroupmember!=NULL){
				found=TRUE;
				break;
			        }
		        }

		/* we couldn't find the host in any host groups */
		if(found==FALSE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Host '%s' is not a member of any host groups!",temp_host->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }

		/* check the event handler command */
		if(temp_host->event_handler!=NULL){
			temp_command=find_command(temp_host->event_handler,NULL);
			if(temp_command==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Event handler command '%s' specified for host '%s' not defined anywhere",temp_host->event_handler,temp_host->name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
		        }

		/* hosts that don't have check commands defined shouldn't ever be checked... */
		if(temp_host->host_check_command!=NULL){

			temp_command=find_command(temp_host->host_check_command,NULL);
			if(temp_command==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Host check command '%s' specified for host '%s' is not defined anywhere!",temp_host->host_check_command,temp_host->name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
		                }
		        }

		/* check notification timeperiod */
		if(temp_host->notification_period!=NULL){
		        temp_timeperiod=find_timeperiod(temp_host->notification_period,NULL);
			if(temp_timeperiod==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Notification period '%s' specified for host '%s' is not defined anywhere!",temp_host->notification_period,temp_host->name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
		        }

		/* check all parent parent host */
		for(temp_hostsmember=temp_host->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

			if(find_host(temp_hostsmember->host_name)==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: '%s' is not a valid parent for host '%s'!",temp_hostsmember->host_name,temp_host->name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
		                }
		        }

		/* check for sane recovery options */
		if(temp_host->notify_on_recovery==TRUE && temp_host->notify_on_down==FALSE && temp_host->notify_on_unreachable==FALSE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Recovery notification option in host '%s' definition doesn't make any sense - specify down and/or unreachable options as well",temp_host->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }

		/* check for illegal characters in host name */
		if(contains_illegal_object_chars(temp_host->name)==TRUE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: The name of host '%s' contains one or more illegal characters.",temp_host->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
	        }


	if(verify_config==TRUE)
		printf("\tChecked %d hosts.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted host verification checks\n");
#endif


	/*****************************************/
	/* check each host group...              */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking host groups...\n");
	if(hostgroup_list==NULL){
		snprintf(temp_buffer,sizeof(temp_buffer),"Error: There are no host groups defined!");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
		errors++;
	        }
	for(temp_hostgroup=hostgroup_list,total_objects=0;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next,total_objects++){

		/* check all group members */
		for(temp_hostgroupmember=temp_hostgroup->members;temp_hostgroupmember!=NULL;temp_hostgroupmember=temp_hostgroupmember->next){

			temp_host=find_host(temp_hostgroupmember->host_name);
			if(temp_host==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Host '%s' specified in host group '%s' is not defined anywhere!",temp_hostgroupmember->host_name,temp_hostgroup->group_name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }

		        }

		found=FALSE;

		/* check all contact groups */
		for(temp_contactgroupsmember=temp_hostgroup->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){

			temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name,NULL);

			if(temp_contactgroup==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Contact group '%s' specified in host group '%s' is not defined anywhere!",temp_contactgroupsmember->group_name,temp_hostgroup->group_name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
			}
		/* check to see if there is at least one contact group */
		if(temp_hostgroup->contact_groups==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Host group '%s'  has no default contact group(s) defined!",temp_hostgroup->group_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }

		/* check for illegal characters in hostgroup name */
		if(contains_illegal_object_chars(temp_hostgroup->group_name)==TRUE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: The name of hostgroup '%s' contains one or more illegal characters.",temp_hostgroup->group_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
		}

	if(verify_config==TRUE)
		printf("\tChecked %d host groups.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted hostgroup verification checks\n");
#endif


	/*****************************************/
	/* check all contacts...                 */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking contacts...\n");
	if(contact_list==NULL){
		snprintf(temp_buffer,sizeof(temp_buffer),"Error: There are no contacts defined!");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
		errors++;
	        }
	for(temp_contact=contact_list,total_objects=0;temp_contact!=NULL;temp_contact=temp_contact->next,total_objects++){

		/* check service notification commands */
		if(temp_contact->service_notification_commands==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Contact '%s' has no service notification commands defined!",temp_contact->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
		else for(temp_commandsmember=temp_contact->service_notification_commands;temp_commandsmember!=NULL;temp_commandsmember=temp_commandsmember->next){
		        temp_command=find_command(temp_commandsmember->command,NULL);
			if(temp_command==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Service notification command '%s' specified for contact '%s' is not defined anywhere!",temp_commandsmember->command,temp_contact->name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
		        }

		/* check host notification commands */
		if(temp_contact->host_notification_commands==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Contact '%s' has no host notification commands defined!",temp_contact->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
		else for(temp_commandsmember=temp_contact->host_notification_commands;temp_commandsmember!=NULL;temp_commandsmember=temp_commandsmember->next){
			temp_command=find_command(temp_commandsmember->command,NULL);
			if(temp_command==NULL){
				sprintf(temp_buffer,"Error: Host notification command '%s' specified for contact '%s' is not defined anywhere!",temp_commandsmember->command,temp_contact->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
	                }

		/* check service notification timeperiod */
		if(temp_contact->service_notification_period==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Contact '%s' has no service notification time period defined!",temp_contact->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }

		else{
		        temp_timeperiod=find_timeperiod(temp_contact->service_notification_period,NULL);
			if(temp_timeperiod==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Service notification period '%s' specified for contact '%s' is not defined anywhere!",temp_contact->service_notification_period,temp_contact->name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
		        }

		/* check host notification timeperiod */
		if(temp_contact->host_notification_period==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Contact '%s' has no host notification time period defined!",temp_contact->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }

		else{
		        temp_timeperiod=find_timeperiod(temp_contact->host_notification_period,NULL);
			if(temp_timeperiod==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Host notification period '%s' specified for contact '%s' is not defined anywhere!",temp_contact->host_notification_period,temp_contact->name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
		        }

		found=FALSE;

		/* make sure the contact belongs to at least one contact group */
		for(temp_contactgroup=contactgroup_list;temp_contactgroup!=NULL;temp_contactgroup=temp_contactgroup->next){
			temp_contactgroupmember=find_contactgroupmember(temp_contact->name,temp_contactgroup,NULL);
			if(temp_contactgroupmember!=NULL){
				found=TRUE;
				break;
			        }
		        }

		/* we couldn't find the contact in any contact groups */
		if(found==FALSE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Contact '%s' is not a member of any contact groups!",temp_contact->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }
	
		/* check for sane host recovery options */
		if(temp_contact->notify_on_host_recovery==TRUE && temp_contact->notify_on_host_down==FALSE && temp_contact->notify_on_host_unreachable==FALSE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Host recovery notification option for contact '%s' doesn't make any sense - specify down and/or unreachable options as well",temp_contact->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }

		/* check for sane service recovery options */
		if(temp_contact->notify_on_service_recovery==TRUE && temp_contact->notify_on_service_critical==FALSE && temp_contact->notify_on_service_warning==FALSE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Service recovery notification option for contact '%s' doesn't make any sense - specify critical and/or warning options as well",temp_contact->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }

		/* check for illegal characters in contact name */
		if(contains_illegal_object_chars(temp_contact->name)==TRUE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: The name of contact '%s' contains one or more illegal characters.",temp_contact->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
	        }

	if(verify_config==TRUE)
		printf("\tChecked %d contacts.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted contact verification checks\n");
#endif

 

	/*****************************************/
	/* check each contact group...           */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking contact groups...\n");
	if(contactgroup_list==NULL){
		snprintf(temp_buffer,sizeof(temp_buffer),"Error: There are no contact groups defined!\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
		errors++;
	        }
	for(temp_contactgroup=contactgroup_list,total_objects=0;temp_contactgroup!=NULL;temp_contactgroup=temp_contactgroup->next,total_objects++){

		found=FALSE;

		/* make sure each contactgroup is used in at least one hostgroup or service definition or escalation */
		for(temp_hostgroup=hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
			for(temp_contactgroupsmember=temp_hostgroup->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
				if(!strcmp(temp_contactgroup->group_name,temp_contactgroupsmember->group_name)){
					found=TRUE;
					break;
				        }
			        }
			if(found==TRUE)
				break;
			}
		if(found==FALSE){
			move_first_service();
			while((temp_service=get_next_service()) && !found) {
				for(temp_contactgroupsmember=temp_service->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
					if(!strcmp(temp_contactgroup->group_name,temp_contactgroupsmember->group_name)){
						found=TRUE;
						break;
				                }
			                 }
			        }
		        }
		if(found==FALSE){
			for(temp_se=serviceescalation_list;temp_se!=NULL;temp_se=temp_se->next){
				for(temp_contactgroupsmember=temp_se->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
					if(!strcmp(temp_contactgroup->group_name,temp_contactgroupsmember->group_name)){
						found=TRUE;
						break;
				                }
			                 }
				if(found==TRUE)
					break;
			        }
		        }
		if(found==FALSE){
			for(temp_hge=hostgroupescalation_list;temp_hge!=NULL;temp_hge=temp_hge->next){
				for(temp_contactgroupsmember=temp_hge->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
					if(!strcmp(temp_contactgroup->group_name,temp_contactgroupsmember->group_name)){
						found=TRUE;
						break;
				                }
			                 }
				if(found==TRUE)
					break;
			        }
		        }
		if(found==FALSE){
			for(temp_he=hostescalation_list;temp_he!=NULL;temp_he=temp_he->next){
				for(temp_contactgroupsmember=temp_he->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
					if(!strcmp(temp_contactgroup->group_name,temp_contactgroupsmember->group_name)){
						found=TRUE;
						break;
				                }
			                 }
				if(found==TRUE)
					break;
			        }
		        }

		/* we couldn't find a hostgroup or service */
		if(found==FALSE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Contact group '%s' is not used in any hostgroup/service definitions or host/hostgroup/service escalations!",temp_contactgroup->group_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
			}

		/* check all the group members */
		for(temp_contactgroupmember=temp_contactgroup->members;temp_contactgroupmember!=NULL;temp_contactgroupmember=temp_contactgroupmember->next){

			temp_contact=find_contact(temp_contactgroupmember->contact_name,NULL);
			if(temp_contact==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Contact '%s' specified in contact group '%s' is not defined anywhere!",temp_contactgroupmember->contact_name,temp_contactgroup->group_name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }

		        }

		/* check for illegal characters in contactgroup name */
		if(contains_illegal_object_chars(temp_contactgroup->group_name)==TRUE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: The name of contact group '%s' contains one or more illegal characters.",temp_contactgroup->group_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
		}

	if(verify_config==TRUE)
		printf("\tChecked %d contact groups.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted contact group verification checks\n");
#endif



	/*****************************************/
	/* check all service escalations...     */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking service escalations...\n");

	for(temp_se=serviceescalation_list,total_objects=0;temp_se!=NULL;temp_se=temp_se->next,total_objects++){

		/* find the service */
		temp_service=find_service(temp_se->host_name,temp_se->description);
		if(temp_service==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Service escalation for service '%s' on host '%s' is not defined anywhere!",temp_se->description,temp_se->host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }

		/* find the contact groups */
		for(temp_contactgroupsmember=temp_se->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
			
			/* find the contact group */
			temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name,NULL);
			if(temp_contactgroup==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Contact group '%s' specified in service escalation for service '%s' on host '%s' is not defined anywhere!",temp_contactgroupsmember->group_name,temp_se->description,temp_se->host_name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
		        }
	        }

	if(verify_config==TRUE)
		printf("\tChecked %d service escalations.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted service escalation checks\n");
#endif



 
	/*****************************************/
	/* check all hostgroup escalations...    */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking host group escalations...\n");

	for(temp_hge=hostgroupescalation_list,total_objects=0;temp_hge!=NULL;temp_hge=temp_hge->next,total_objects++){

		/* find the hostgroup */
		temp_hostgroup=find_hostgroup(temp_hge->group_name,NULL);
		if(temp_hostgroup==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Hostgroup escalation for hostgroup '%s' is invalid because the hostgroup is not defined anywhere!",temp_hge->group_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }

		/* find the contact groups */
		for(temp_contactgroupsmember=temp_hge->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
			
			/* find the contact group */
			temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name,NULL);
			if(temp_contactgroup==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Contact group '%s' specified in hostgroup escalation for hostgroup '%s' is not defined anywhere!",temp_contactgroupsmember->group_name,temp_hge->group_name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
		        }
	        }

	if(verify_config==TRUE)
		printf("\tChecked %d host group escalations.\n",total_objects);
 
#ifdef DEBUG1
	printf("\tCompleted hostgroup escalation checks\n");
#endif


	/*****************************************/
	/* check all service dependencies...     */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking service dependencies...\n");

	for(temp_sd=servicedependency_list,total_objects=0;temp_sd!=NULL;temp_sd=temp_sd->next,total_objects++){

		/* find the dependent service */
		temp_service=find_service(temp_sd->dependent_host_name,temp_sd->dependent_service_description);
		if(temp_service==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Dependent service specified in service dependency for service '%s' on host '%s' is not defined anywhere!",temp_sd->dependent_service_description,temp_sd->dependent_host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }

		/* find the service we're depending on */
		temp_service2=find_service(temp_sd->host_name,temp_sd->service_description);
		if(temp_service2==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Service specified in service dependency for service '%s' on host '%s' is not defined anywhere!",temp_sd->dependent_service_description,temp_sd->dependent_host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }

		/* make sure they're not the same service */
		if(temp_service==temp_service2){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Service dependency definition for service '%s' on host '%s' is circular (it depends on itself)!",temp_sd->dependent_service_description,temp_sd->dependent_host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
	        }

	if(verify_config==TRUE)
		printf("\tChecked %d service dependencies.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted service dependency checks\n");
#endif



	/*****************************************/
	/* check all host escalations...     */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking host escalations...\n");

	for(temp_he=hostescalation_list,total_objects=0;temp_he!=NULL;temp_he=temp_he->next,total_objects++){

		/* find the host */
		temp_host=find_host(temp_he->host_name);
		if(temp_host==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Host escalation for host '%s' is not defined anywhere!",temp_he->host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }

		/* find the contact groups */
		for(temp_contactgroupsmember=temp_he->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
			
			/* find the contact group */
			temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name,NULL);
			if(temp_contactgroup==NULL){
				snprintf(temp_buffer,sizeof(temp_buffer),"Error: Contact group '%s' specified in host escalation for host '%s' is not defined anywhere!",temp_contactgroupsmember->group_name,temp_he->host_name);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				errors++;
			        }
		        }
	        }

	if(verify_config==TRUE)
		printf("\tChecked %d host escalations.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted host escalation checks\n");
#endif


	/*****************************************/
	/* check all host dependencies...     */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking host dependencies...\n");

	for(temp_hd=hostdependency_list,total_objects=0;temp_hd!=NULL;temp_hd=temp_hd->next,total_objects++){

		/* find the dependent host */
		temp_host=find_host(temp_hd->dependent_host_name);
		if(temp_host==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Dependent host specified in host dependency for host '%s' is not defined anywhere!",temp_hd->dependent_host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }

		/* find the host we're depending on */
		temp_host2=find_host(temp_hd->host_name);
		if(temp_host2==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Host specified in host dependency for host '%s' is not defined anywhere!",temp_hd->dependent_host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }

		/* make sure they're not the same host */
		if(temp_host==temp_host2){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Host dependency definition for host '%s' is circular (it depends on itself)!",temp_hd->dependent_host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
	        }

	if(verify_config==TRUE)
		printf("\tChecked %d host dependencies.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted host dependency checks\n");
#endif


	/*****************************************/
	/* check all commands...                 */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking commands...\n");

	for(temp_command=command_list,total_objects=0;temp_command!=NULL;temp_command=temp_command->next,total_objects++){

		/* check for illegal characters in command name */
		if(contains_illegal_object_chars(temp_command->name)==TRUE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: The name of command '%s' contains one or more illegal characters.",temp_command->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
	        }

	if(verify_config==TRUE)
		printf("\tChecked %d commands.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted command checks\n");
#endif


	/*****************************************/
	/* check all timeperiods...              */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking time periods...\n");

	for(temp_timeperiod=timeperiod_list,total_objects=0;temp_timeperiod!=NULL;temp_timeperiod=temp_timeperiod->next,total_objects++){

		/* check for illegal characters in timeperiod name */
		if(contains_illegal_object_chars(temp_timeperiod->name)==TRUE){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: The name of time period '%s' contains one or more illegal characters.",temp_timeperiod->name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
	        }

	if(verify_config==TRUE)
		printf("\tChecked %d time periods.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted command checks\n");
#endif


	/********************************************/
	/* check for circular paths between hosts   */
	/********************************************/
	if(verify_config==TRUE)
		printf("Checking for circular paths between hosts...\n");

	/* check routes between all hosts */
	found=FALSE;
	result=OK;
	move_first_host();
	while(temp_host = get_next_host()) {
		found=check_for_circular_path(temp_host,temp_host);
		if(found==TRUE){
			sprintf(temp_buffer,"Error: There is a circular parent/child path that exists for host '%s'!",temp_host->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			result=ERROR;
		        }
	        }
	if(result==ERROR)
		errors++;

#ifdef DEBUG1
	printf("\tCompleted circular path checks\n");
#endif



	/********************************************/
	/* check for circular dependencies         */
	/********************************************/
	if(verify_config==TRUE)
		printf("Checking for circular service execution dependencies...\n");

	/* check dependencies between all services */
	for(temp_sd=servicedependency_list;temp_sd!=NULL;temp_sd=temp_sd->next){

		/* clear checked flag for all dependencies */
		for(temp_sd2=servicedependency_list;temp_sd2!=NULL;temp_sd2=temp_sd2->next)
			temp_sd2->has_been_checked=FALSE;

		found=check_for_circular_dependency(temp_sd,temp_sd);
		if(found==TRUE){
			sprintf(temp_buffer,"Warning: A circular execution dependency (which could result in a deadlock) exists for service '%s' on host '%s'!",temp_sd->service_description,temp_sd->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			warnings++;
		        }
	        }

#ifdef DEBUG1
	printf("\tCompleted circular service dependency checks\n");
#endif

 
	/********************************************/
	/* check global event handler commands...   */
	/********************************************/
	if(verify_config==TRUE)
		printf("Checking global event handlers...\n");
	if(global_host_event_handler!=NULL){

	        temp_command=find_command(global_host_event_handler,NULL);
		if(temp_command==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Global host event handler command '%s' is not defined anywhere!",global_host_event_handler);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
	        }
	if(global_service_event_handler!=NULL){

	        temp_command=find_command(global_service_event_handler,NULL);
		if(temp_command==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Global service event handler command '%s' is not defined anywhere!",global_service_event_handler);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
	        }

#ifdef DEBUG1
	printf("\tCompleted global event handler command checks\n");
#endif

	/**************************************************/
	/* check obsessive service processor command...   */
	/**************************************************/
	if(verify_config==TRUE)
		printf("Checking obsessive compulsive service processor command...\n");
	if(ocsp_command!=NULL){
	        temp_command=find_command(ocsp_command,NULL);
		if(temp_command==NULL){
			snprintf(temp_buffer,sizeof(temp_buffer),"Error: Obsessive compulsive service processor command '%s' is not defined anywhere!",ocsp_command);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			errors++;
		        }
	        }

#ifdef DEBUG1
	printf("\tCompleted obsessive compulsive service processor command check\n");
#endif

	/**************************************************/
	/* check various settings...                      */
	/**************************************************/
	if(verify_config==TRUE)
		printf("Checking misc settings...\n");

	/* warn if user didn't specify any illegal macro output chars */
	if(illegal_output_chars==NULL){
		sprintf(temp_buffer,"Warning: Nothing specified for illegal_macro_output_chars variable!\n");
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
		warnings++;
	        }

	/* count number of services associated with each host (we need this for flap detection)... */
	move_first_service();
	while(temp_service = get_next_service()) {
		if(temp_host=find_host(temp_service->host_name)) {
			temp_host->total_services++;
			temp_host->total_service_check_interval+=temp_service->check_interval;
		        }
	        }

	if(verify_config==TRUE){
		printf("\n");
		printf("Total Warnings: %d\n",warnings);
		printf("Total Errors:   %d\n",errors);
	        }

#ifdef DEBUG0
	printf("pre_flight_check() end\n");
#endif

	return (errors>0)?ERROR:OK;
	}

