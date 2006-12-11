/*****************************************************************************
 *
 * CONFIG.C - Configuration input and verification routines for Nagios
 *
 * Copyright (c) 1999-2006 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 10-27-2006
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
#include "../include/objects.h"
#include "../include/nagios.h"
#include "../include/broker.h"
#include "../include/nebmods.h"
#include "../include/nebmodules.h"


extern char	*log_file;
extern char     *command_file;
extern char     *temp_file;
extern char     *temp_path;
extern char     *lock_file;
extern char	*log_archive_path;
extern char     *auth_file;
extern char	*p1_file;

extern char     *nagios_user;
extern char     *nagios_group;

extern char     *macro_x[MACRO_X_COUNT];
extern char     *macro_user[MAX_USER_MACROS];

extern char     *global_host_event_handler;
extern char     *global_service_event_handler;
extern command  *global_host_event_handler_ptr;
extern command  *global_service_event_handler_ptr;

extern char     *ocsp_command;
extern char     *ochp_command;
extern command  *ocsp_command_ptr;
extern command  *ochp_command_ptr;

extern char     *illegal_object_chars;
extern char     *illegal_output_chars;

extern int      use_regexp_matches;
extern int      use_true_regexp_matching;

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
extern int      ochp_timeout;

extern int      log_initial_states;

extern int      daemon_mode;
extern int      daemon_dumps_core;

extern int      verify_config;
extern int      verify_object_relationships;
extern int      verify_circular_paths;
extern int      test_scheduling;
extern int      precache_objects;

extern double   sleep_time;
extern int      interval_length;
extern int      service_inter_check_delay_method;
extern int      host_inter_check_delay_method;
extern int      service_interleave_factor_method;
extern int      max_host_check_spread;
extern int      max_service_check_spread;

extern sched_info scheduling_info;

extern int      max_child_process_time;

extern int      max_parallel_service_checks;

extern int      command_check_interval;
extern int      check_reaper_interval;
extern int      service_freshness_check_interval;
extern int      host_freshness_check_interval;
extern int      auto_rescheduling_interval;
extern int      auto_rescheduling_window;

extern int      check_external_commands;
extern int      check_orphaned_services;
extern int      check_orphaned_hosts;
extern int      check_service_freshness;
extern int      check_host_freshness;
extern int      auto_reschedule_checks;

extern int      use_aggressive_host_checking;
extern int      use_old_host_check_logic;
extern unsigned long cached_host_check_horizon;
extern unsigned long cached_service_check_horizon;
extern int      enable_predictive_host_dependency_checks;
extern int      enable_predictive_service_dependency_checks;

extern int      soft_state_dependencies;

extern int      retain_state_information;
extern int      retention_update_interval;
extern int      use_retained_program_state;
extern int      use_retained_scheduling_info;
extern int      retention_scheduling_horizon;

extern int      log_rotation_method;

extern int      enable_notifications;
extern int      execute_service_checks;
extern int      accept_passive_service_checks;
extern int      execute_host_checks;
extern int      accept_passive_host_checks;
extern int      enable_event_handlers;
extern int      obsess_over_services;
extern int      obsess_over_hosts;
extern int      enable_failure_prediction;

extern int      aggregate_status_updates;
extern int      status_update_interval;

extern int      time_change_threshold;

extern unsigned long event_broker_options;

extern int      process_performance_data;

extern int      enable_flap_detection;

extern double   low_service_flap_threshold;
extern double   high_service_flap_threshold;
extern double   low_host_flap_threshold;
extern double   high_host_flap_threshold;

extern int      use_large_installation_tweaks;

extern int      enable_embedded_perl;
extern int      use_embedded_perl_implicitly;

extern int      date_format;

extern contact		*contact_list;
extern contactgroup	*contactgroup_list;
extern host             *host_list;
extern hostgroup	*hostgroup_list;
extern service          *service_list;
extern servicegroup     *servicegroup_list;
extern notification     *notification_list;
extern command          *command_list;
extern timeperiod       *timeperiod_list;
extern serviceescalation *serviceescalation_list;
extern servicedependency *servicedependency_list;
extern hostdependency   *hostdependency_list;
extern hostescalation   *hostescalation_list;

extern host		**host_hashlist;
extern service		**service_hashlist;




/******************************************************************/
/************** CONFIGURATION INPUT FUNCTIONS *********************/
/******************************************************************/

/* read all configuration data */
int read_all_object_data(char *main_config_file){
	int result=OK;
	int options=0;
	int cache=FALSE;
	int precache=FALSE;

#ifdef DEBUG0
	printf("read_all_config_data() start\n");
#endif

	options=READ_ALL_OBJECT_DATA;

	/* cache object definitions if we're up and running */
	if(verify_config==FALSE && test_scheduling==FALSE)
		cache=TRUE;

	/* precache object definitions */
	if(precache_objects==TRUE && (verify_config==TRUE || test_scheduling==TRUE))
		precache=TRUE;

	/* read in all host configuration data from external sources */
	result=read_object_config_data(main_config_file,options,cache,precache);
	if(result!=OK)
		return ERROR;

#ifdef DEBUG0
	printf("read_all_config_data() end\n");
#endif

	return OK;
        }


/* process the main configuration file */
int read_main_config_file(char *main_config_file){
	char *input=NULL;
	char *variable=NULL;
	char *value=NULL;
	char *temp_buffer=NULL;
	char error_message[MAX_INPUT_BUFFER];
	char *temp_ptr=NULL;
	mmapfile *thefile=NULL;
	int current_line=0;
	int error=FALSE;
	int command_check_interval_is_seconds=FALSE;
	char *modptr=NULL;
	char *argptr=NULL;

#ifdef DEBUG0
	printf("read_main_config_file() start\n");
#endif
#ifdef DEBUG1
	printf("\tConfig file: %s\n",main_config_file);
#endif

	/* open the config file for reading */
	if((thefile=mmap_fopen(main_config_file))==NULL){
		asprintf(&temp_buffer,"Error: Cannot open main configuration file '%s' for reading!",main_config_file);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
		return ERROR;
		}

	/* save the main config file macro */
	my_free((void **)&macro_x[MACRO_MAINCONFIGFILE]);
	if((macro_x[MACRO_MAINCONFIGFILE]=(char *)strdup(main_config_file)))
		strip(macro_x[MACRO_MAINCONFIGFILE]);

	/* process all lines in the config file */
	while(1){

		/* free memory */
		my_free((void **)&input);
		my_free((void **)&variable);
		my_free((void **)&value);

		/* read the next line */
		if((input=mmap_fgets(thefile))==NULL)
			break;

		current_line=thefile->current_line;

		/* skip blank lines */
		if(input[0]=='\x0' || input[0]=='\n' || input[0]=='\r')
			continue;

		strip(input);

		/* skip comments */
		if(input[0]=='#' || input[0]==';')
			continue;

#ifdef DEBUG1
		printf("\tLine %d = '%s'\n",current_line,input);
#endif

		/* get the variable name */
		if((temp_ptr=my_strtok(input,"="))==NULL){
			strcpy(error_message,"NULL variable");
			error=TRUE;
			break;
			}
		if((variable=(char *)strdup(temp_ptr))==NULL){
			strcpy(error_message,"malloc() error");
			error=TRUE;
			break;
		        }

		/* get the value */
		if((temp_ptr=my_strtok(NULL,"\n"))==NULL){
			strcpy(error_message,"NULL value");
			error=TRUE;
			break;
			}
		if((value=(char *)strdup(temp_ptr))==NULL){
			strcpy(error_message,"malloc() error");
			error=TRUE;
			break;
		        }
		strip(variable);
		strip(value);

		/* process the variable/value */

		if(!strcmp(variable,"resource_file")){

			/* save the macro */
			my_free((void **)&macro_x[MACRO_RESOURCEFILE]);
			macro_x[MACRO_RESOURCEFILE]=(char *)strdup(value);

#ifdef DEBUG1
			printf("\t\tprocessing resource file '%s'\n",value);
#endif

			/* process the resource file */
			read_resource_file(value);
		        }

		else if(!strcmp(variable,"log_file")){

			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"Log file is too long");
				error=TRUE;
				break;
				}

			my_free((void **)&log_file);
			log_file=(char *)strdup(value);

			/* save the macro */
			my_free((void **)&macro_x[MACRO_LOGFILE]);
			macro_x[MACRO_LOGFILE]=(char *)strdup(log_file);

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

			my_free((void **)&command_file);
			command_file=(char *)strdup(value);

			/* save the macro */
			my_free((void **)&macro_x[MACRO_COMMANDFILE]);
			macro_x[MACRO_COMMANDFILE]=(char *)strdup(value);

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

			my_free((void **)&temp_file);
			temp_file=(char *)strdup(value);

			/* save the macro */
			my_free((void **)&macro_x[MACRO_TEMPFILE]);
			macro_x[MACRO_TEMPFILE]=(char *)strdup(temp_file);

#ifdef DEBUG1
			printf("\t\ttemp_file set to '%s'\n",temp_file);
#endif
			}

		else if(!strcmp(variable,"temp_path")){

			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"Temp path is too long");
				error=TRUE;
				break;
				}

			my_free((void **)&temp_path);
			if((temp_path=(char *)strdup(value))){
				strip(temp_path);
				/* make sure we don't have a trailing slash */
				if(temp_path[strlen(temp_path)-1]=='/')
					temp_path[strlen(temp_path)-1]='\x0';
			        }

			/* save the macro */
			my_free((void **)&macro_x[MACRO_TEMPPATH]);
			macro_x[MACRO_TEMPPATH]=(char *)strdup(temp_path);

#ifdef DEBUG1
			printf("\t\ttemp_path set to '%s'\n",temp_path);
#endif
			}

		else if(!strcmp(variable,"lock_file")){

			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"Lock file is too long");
				error=TRUE;
				break;
				}

			my_free((void **)&lock_file);
			lock_file=(char *)strdup(value);

#ifdef DEBUG1
			printf("\t\tlock_file set to '%s'\n",lock_file);
#endif
			}

		else if(!strcmp(variable,"global_host_event_handler")){
			my_free((void **)&global_host_event_handler);
			global_host_event_handler=(char *)strdup(value);
#ifdef DEBUG1
			printf("\t\tglobal_host_event_handler set to '%s'\n",global_host_event_handler);
#endif
		        }

		else if(!strcmp(variable,"global_service_event_handler")){
			my_free((void **)&global_service_event_handler);
			global_service_event_handler=(char *)strdup(value);
#ifdef DEBUG1
			printf("\t\tglobal_service_event_handler set to '%s'\n",global_service_event_handler);
#endif
		        }

		else if(!strcmp(variable,"ocsp_command")){
			my_free((void **)&ocsp_command);
			ocsp_command=(char *)strdup(value);
#ifdef DEBUG1
			printf("\t\tocsp_command set to '%s'\n",ocsp_command);
#endif
		        }

		else if(!strcmp(variable,"ochp_command")){
			my_free((void **)&ochp_command);
			ochp_command=(char *)strdup(value);
#ifdef DEBUG1
			printf("\t\tochp_command set to '%s'\n",ochp_command);
#endif
		        }

		else if(!strcmp(variable,"nagios_user")){
			my_free((void **)&nagios_user);
			nagios_user=(char *)strdup(value);
#ifdef DEBUG1
			printf("\t\tnagios_user set to '%s'\n",nagios_user);
#endif
		        }

		else if(!strcmp(variable,"nagios_group")){
			my_free((void **)&nagios_group);
			nagios_group=(char *)strdup(value);
#ifdef DEBUG1
			printf("\t\tnagios_group set to '%s'\n",nagios_group);
#endif
		        }

		else if(!strcmp(variable,"admin_email")){

			/* save the macro */
			my_free((void **)&macro_x[MACRO_ADMINEMAIL]);
			macro_x[MACRO_ADMINEMAIL]=(char *)strdup(value);

#ifdef DEBUG1
			printf("\t\tmacro_admin_email set to '%s'\n",macro_x[MACRO_ADMINEMAIL]);
#endif
		        }

		else if(!strcmp(variable,"admin_pager")){

			/* save the macro */
			my_free((void **)&macro_x[MACRO_ADMINPAGER]);
			macro_x[MACRO_ADMINPAGER]=(char *)strdup(value);

#ifdef DEBUG1
			printf("\t\tmacro_admin_pager set to '%s'\n",macro_x[MACRO_ADMINPAGER]);
#endif
		        }

		else if(!strcmp(variable,"use_syslog")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for use_syslog");
				error=TRUE;
				break;
				}

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

			log_external_commands=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tlog_external_commands set to %s\n",(log_external_commands==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"log_passive_checks")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for log_passive_checks");
				error=TRUE;
				break;
			        }

			log_passive_checks=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tlog_passive_checks set to %s\n",(log_passive_checks==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"log_initial_states")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for log_initial_states");
				error=TRUE;
				break;
			        }

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

			retain_state_information=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tretain_state_information set to %s\n",(retain_state_information==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"retention_update_interval")){

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

			use_retained_program_state=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tuse_retained_program_state set to %s\n",(use_retained_program_state==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"use_retained_scheduling_info")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for use_retained_scheduling_info");
				error=TRUE;
				break;
			        }

			use_retained_scheduling_info=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tuse_retained_scheduling_info set to %s\n",(use_retained_scheduling_info==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"retention_scheduling_horizon")){

			retention_scheduling_horizon=atoi(value);

			if(retention_scheduling_horizon<=0){
				strcpy(error_message,"Illegal value for retention_scheduling_horizon");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tretention_scheduling_horizon set to %d\n",retention_scheduling_horizon);
#endif
		        }

		else if(!strcmp(variable,"obsess_over_services")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for obsess_over_services");
				error=TRUE;
				break;
			        }

			obsess_over_services=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tobsess_over_services set to %s\n",(obsess_over_services==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"obsess_over_hosts")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for obsess_over_hosts");
				error=TRUE;
				break;
			        }

			obsess_over_hosts=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tobsess_over_hosts set to %s\n",(obsess_over_hosts==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"service_check_timeout")){

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

		else if(!strcmp(variable,"ochp_timeout")){

			ochp_timeout=atoi(value);

			if(ochp_timeout<=0){
				strcpy(error_message,"Illegal value for ochp_timeout");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tochp_timeout set to %d\n",ochp_timeout);
#endif
		        }

		else if(!strcmp(variable,"use_agressive_host_checking") || !strcmp(variable,"use_aggressive_host_checking")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for use_aggressive_host_checking");
				error=TRUE;
				break;
			        }

			use_aggressive_host_checking=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tuse_aggressive_host_checking set to %s\n",(use_aggressive_host_checking==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"use_old_host_check_logic")){
			use_old_host_check_logic=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tuse_old_host_check_logic set to %s\n",(use_old_host_check_logic==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"cached_host_check_horizon")){
			cached_host_check_horizon=strtoul(value,NULL,0);
#ifdef DEBUG1
			printf("\t\tcached_host_check_horizon set to %lu\n",cached_host_check_horizon);
#endif
		        }

		else if(!strcmp(variable,"enable_predictive_host_dependency_checks")){
			enable_predictive_host_dependency_checks=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tenable_predictive_host_dependency_checks set to %s\n",(enable_predictive_host_dependency_checks==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"cached_service_check_horizon")){
			cached_service_check_horizon=strtoul(value,NULL,0);
#ifdef DEBUG1
			printf("\t\tcached_service_check_horizon set to %lu\n",cached_service_check_horizon);
#endif
		        }

		else if(!strcmp(variable,"enable_predictive_service_dependency_checks")){
			enable_predictive_service_dependency_checks=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tenable_predictive_service_dependency_checks set to %s\n",(enable_predictive_service_dependency_checks==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"soft_state_dependencies")){
			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for soft_state_dependencies");
				error=TRUE;
				break;
			        }

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

			my_free((void **)&log_archive_path);
			log_archive_path=(char *)strdup(value);

#ifdef DEBUG1
			printf("\t\tlog_archive_path set to '%s'\n",log_archive_path);
#endif
			}

		else if(!strcmp(variable,"enable_event_handlers")){
			enable_event_handlers=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tenable_event_handlers set to %s\n",(enable_event_handlers==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"enable_notifications")){
			enable_notifications=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tenable_notifications set to %s\n",(enable_notifications==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"execute_service_checks")){
			execute_service_checks=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\texecute_service_checks set to %s\n",(execute_service_checks==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"accept_passive_service_checks")){
			accept_passive_service_checks=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\taccept_passive_service_checks set to %s\n",(accept_passive_service_checks==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"execute_host_checks")){
			execute_host_checks=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\texecute_host_checks set to %s\n",(execute_host_checks==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"accept_passive_host_checks")){
			accept_passive_host_checks=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\taccept_passive_host_checks set to %s\n",(accept_passive_host_checks==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"service_inter_check_delay_method")){
			if(!strcmp(value,"n"))
				service_inter_check_delay_method=ICD_NONE;
			else if(!strcmp(value,"d"))
				service_inter_check_delay_method=ICD_DUMB;
			else if(!strcmp(value,"s"))
				service_inter_check_delay_method=ICD_SMART;
			else{
				service_inter_check_delay_method=ICD_USER;
				scheduling_info.service_inter_check_delay=strtod(value,NULL);
				if(scheduling_info.service_inter_check_delay<=0.0){
					strcpy(error_message,"Illegal value for service_inter_check_delay_method");
					error=TRUE;
					break;
				        }
			        }
#ifdef DEBUG1
			printf("\t\tservice_inter_check_delay_method set to %d\n",service_inter_check_delay_method);
#endif
		        }

		else if(!strcmp(variable,"max_service_check_spread")){
			strip(value);
			max_service_check_spread=atoi(value);
			if(max_service_check_spread<1){
				strcpy(error_message,"Illegal value for max_service_check_spread");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tmax_service_check_spread set to %d\n",max_service_check_spread);
#endif
		        }

		else if(!strcmp(variable,"host_inter_check_delay_method")){

			if(!strcmp(value,"n"))
				host_inter_check_delay_method=ICD_NONE;
			else if(!strcmp(value,"d"))
				host_inter_check_delay_method=ICD_DUMB;
			else if(!strcmp(value,"s"))
				host_inter_check_delay_method=ICD_SMART;
			else{
				host_inter_check_delay_method=ICD_USER;
				scheduling_info.host_inter_check_delay=strtod(value,NULL);
				if(scheduling_info.host_inter_check_delay<=0.0){
					strcpy(error_message,"Illegal value for host_inter_check_delay_method");
					error=TRUE;
					break;
				        }
			        }
#ifdef DEBUG1
			printf("\t\thost_inter_check_delay_method set to %d\n",host_inter_check_delay_method);
#endif
		        }

		else if(!strcmp(variable,"max_host_check_spread")){

			max_host_check_spread=atoi(value);
			if(max_host_check_spread<1){
				strcpy(error_message,"Illegal value for max_host_check_spread");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tmax_host_check_spread set to %d\n",max_host_check_spread);
#endif
		        }

		else if(!strcmp(variable,"service_interleave_factor")){
			if(!strcmp(value,"s"))
				service_interleave_factor_method=ILF_SMART;
			else{
				service_interleave_factor_method=ILF_USER;
				scheduling_info.service_interleave_factor=atoi(value);
				if(scheduling_info.service_interleave_factor<1)
					scheduling_info.service_interleave_factor=1;
			        }
		        }

		else if(!strcmp(variable,"max_concurrent_checks")){

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

		else if(!strcmp(variable,"check_result_reaper_frequency") || !strcmp(variable,"service_reaper_frequency")){

			check_reaper_interval=atoi(value);
			if(check_reaper_interval<1){
				strcpy(error_message,"Illegal value for check_result_reaper_frequency");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tcheck_result_reaper_interval set to %d\n",check_reaper_interval);
#endif
		        }

		else if(!strcmp(variable,"sleep_time")){

			sleep_time=atof(value);
			if(sleep_time<=0.0){
				strcpy(error_message,"Illegal value for sleep_time");
				error=TRUE;
				break;
			        }
#ifdef DEBUG1
			printf("\t\tsleep_time set to %f\n",sleep_time);
#endif
		        }

		else if(!strcmp(variable,"interval_length")){

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

			check_external_commands=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tcheck_external_commands set to %s\n",(check_external_commands==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"command_check_interval")){

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

			check_orphaned_services=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tcheck_orphaned_services set to %s\n",(check_orphaned_services==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"check_for_orphaned_hosts")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for check_for_orphaned_hosts");
				error=TRUE;
				break;
			        }

			check_orphaned_hosts=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tcheck_orphaned_hosts set to %s\n",(check_orphaned_host==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"check_service_freshness")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for check_service_freshness");
				error=TRUE;
				break;
			        }

			check_service_freshness=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tcheck_service_freshness set to %s\n",(check_service_freshness==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"check_host_freshness")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for check_host_freshness");
				error=TRUE;
				break;
			        }

			check_host_freshness=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tcheck_host_freshness set to %s\n",(check_host_freshness==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"service_freshness_check_interval") || !strcmp(variable,"freshness_check_interval")){

			service_freshness_check_interval=atoi(value);
			if(service_freshness_check_interval<=0){
				strcpy(error_message,"Illegal value for service_freshness_check_interval");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tservice_freshness_check_interval set to %d\n",service_freshness_check_interval);
#endif
		        }

		else if(!strcmp(variable,"host_freshness_check_interval")){

			host_freshness_check_interval=atoi(value);
			if(host_freshness_check_interval<=0){
				strcpy(error_message,"Illegal value for host_freshness_check_interval");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\thost_freshness_check_interval set to %d\n",host_freshness_check_interval);
#endif
		        }
		else if(!strcmp(variable,"auto_reschedule_checks")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for auto_reschedule_checks");
				error=TRUE;
				break;
			        }

			auto_reschedule_checks=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tauto_reschedule_checks set to %s\n",(auto_reschedule_checks==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"auto_rescheduling_interval")){

			auto_rescheduling_interval=atoi(value);
			if(auto_rescheduling_interval<=0){
				strcpy(error_message,"Illegal value for auto_rescheduling_interval");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tauto_rescheduling_interval set to %d\n",auto_rescheduling_interval);
#endif
		        }

		else if(!strcmp(variable,"auto_rescheduling_window")){

			auto_rescheduling_window=atoi(value);
			if(auto_rescheduling_window<=0){
				strcpy(error_message,"Illegal value for auto_rescheduling_window");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tauto_rescheduling_window set to %d\n",auto_rescheduling_window);
#endif
		        }

		else if(!strcmp(variable,"aggregate_status_updates")){

			aggregate_status_updates=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\taggregate_status_updates to %s\n",(aggregate_status_updates==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"status_update_interval")){

			status_update_interval=atoi(value);
			if(status_update_interval<=1){
				strcpy(error_message,"Illegal value for status_update_interval");
				error=TRUE;
				break;
			        }

#ifdef DEBUG1
			printf("\t\tstatus_update_interval set to %d\n",status_update_interval);
#endif
		        }

		else if(!strcmp(variable,"time_change_threshold")){

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

			process_performance_data=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tprocess_performance_data set to %s\n",(process_performance_data==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"enable_flap_detection")){

			enable_flap_detection=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tenable_flap_detection set to %s\n",(enable_flap_detection==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"enable_failure_prediction")){

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
			printf("\t\tlow_service_flap_threshold set to %f\n",low_service_flap_threshold);
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
			printf("\t\thigh_service_flap_threshold set to %f\n",high_service_flap_threshold);
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
			printf("\t\tlow_host_flap_threshold set to %f\n",low_host_flap_threshold);
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
			printf("\t\thigh_host_flap_threshold set to %f\n",high_host_flap_threshold);
#endif
		        }

		else if(!strcmp(variable,"date_format")){

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

		else if(!strcmp(variable,"p1_file")){

			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"P1 file is too long");
				error=TRUE;
				break;
				}

			my_free((void **)&p1_file);
			p1_file=(char *)strdup(value);

#ifdef DEBUG1
			printf("\t\tp1_file set to '%s'\n",p1_file);
#endif
			}

		else if(!strcmp(variable,"event_broker_options")){

			if(!strcmp(value,"-1"))
				event_broker_options=BROKER_EVERYTHING;
			else
				event_broker_options=strtoul(value,NULL,0);
#ifdef DEBUG1
			printf("\t\tevent_broker_options set to %d\n",event_broker_options);
#endif
		        }

		else if(!strcmp(variable,"illegal_object_name_chars")){
			illegal_object_chars=(char *)strdup(value);
#ifdef DEBUG1
			printf("\t\tillegal_object_name_chars set to '%s'\n",illegal_object_chars);
#endif
		        }

		else if(!strcmp(variable,"illegal_macro_output_chars")){
			illegal_output_chars=(char *)strdup(value);
#ifdef DEBUG1
			printf("\t\tillegal_macro_output_chars set to '%s'\n",illegal_output_chars);
#endif
		        }

		else if(!strcmp(variable,"broker_module")){
			modptr=strtok(value," \n");
			argptr=strtok(NULL,"\n");
#ifdef USE_EVENT_BROKER
                        neb_add_module(modptr,argptr,TRUE);
#endif
		        }

		else if(!strcmp(variable,"use_regexp_matching")){

			use_regexp_matches=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tuse_regexp_matches to %s\n",(use_regexp_matches==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"use_true_regexp_matching")){

			use_true_regexp_matching=(atoi(value)>0)?TRUE:FALSE;
#ifdef DEBUG1
			printf("\t\tuse_true_regexp_matching to %s\n",(use_true_regexp_matching==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"daemon_dumps_core")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for daemon_dumps_core");
				error=TRUE;
				break;
				}

			daemon_dumps_core=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tdaemon_dumps_core set to %s\n",(daemon_dumps_core==TRUE)?"TRUE":"FALSE");
#endif
			}

		else if(!strcmp(variable,"use_large_installation_tweaks")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for use_large_installation_tweaks ");
				error=TRUE;
				break;
			        }

			use_large_installation_tweaks=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tuse_large_installation_tweaks set to %s\n",(use_large_installation_tweaks==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"enable_embedded_perl")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for enable_embedded_perl");
				error=TRUE;
				break;
			        }

			enable_embedded_perl=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tenable_embedded_perl set to %s\n",(enable_embedded_perl==TRUE)?"TRUE":"FALSE");
#endif
		        }

		else if(!strcmp(variable,"use_embedded_perl_implicitly")){

			if(strlen(value)!=1||value[0]<'0'||value[0]>'1'){
				strcpy(error_message,"Illegal value for use_embedded_perl_implicitly");
				error=TRUE;
				break;
			        }

			use_embedded_perl_implicitly=(atoi(value)>0)?TRUE:FALSE;

#ifdef DEBUG1
			printf("\t\tuse_embedded_perl_implicitly set to %s\n",(use_embedded_perl_implicitly==TRUE)?"TRUE":"FALSE");
#endif
		        }

		/*** AUTH_FILE VARIABLE USED BY EMBEDDED PERL INTERPRETER ***/
		else if(!strcmp(variable,"auth_file")){

			if(strlen(value)>MAX_FILENAME_LENGTH-1){
				strcpy(error_message,"Auth file is too long");
				error=TRUE;
				break;
			        }

			my_free((void **)&auth_file);
			auth_file=(char *)strdup(value);
#ifdef DEBUG1
			printf("\t\tauth_file set to '%s'\n",auth_file);
#endif
		        }

		/* warn about old variables */
		else if(!strcmp(variable,"comment_file") || !strcmp(variable,"xcddefault_comment_file")){
			asprintf(&temp_buffer,"Warning: comment_file variable ignored.  Comments are now stored in the status and retention files.");
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
			my_free((void **)&temp_buffer);
		        }
		else if(!strcmp(variable,"downtime_file") || !strcmp(variable,"xdddefault_downtime_file")){
			asprintf(&temp_buffer,"Warning: downtime_file variable ignored.  Downtime entries are now stored in the status and retention files.");
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
			my_free((void **)&temp_buffer);
		        }

		/* skip external data directives */
		else if(strstr(input,"x")==input)
			continue;

		/* ignore external variables */
		else if(!strcmp(variable,"status_file"))
			continue;
		else if(!strcmp(variable,"perfdata_timeout"))
			continue;
		else if(strstr(variable,"host_perfdata")==variable)
			continue;
		else if(strstr(variable,"service_perfdata")==variable)
			continue;
		else if(strstr(input,"cfg_file=")==input || strstr(input,"cfg_dir=")==input)
			continue;
		else if(strstr(input,"state_retention_file=")==input)
			continue;
		else if(strstr(input,"object_cache_file=")==input)
			continue;
		else if(strstr(input,"precached_object_file=")==input)
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
		asprintf(&temp_buffer,"Error in configuration file '%s' - Line %d (%s)",main_config_file,current_line,error_message);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
		return ERROR;
	        }

	/* free leftover memory and close the file */
	my_free((void **)&input);
	mmap_fclose(thefile);

	/* free memory */
	my_free((void **)&input);
	my_free((void **)&variable);
	my_free((void **)&value);

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
	char *temp_buffer=NULL;
	char *input=NULL;
	char *variable=NULL;
	char *value=NULL;
	char *temp_ptr=NULL;
	mmapfile *thefile=NULL;
	int current_line=1;
	int error=FALSE;
	int user_index=0;

#ifdef DEBUG0
	printf("read_resource_file() start\n");
#endif

#ifdef DEBUG1
	printf("processing resource file '%s'\n",resource_file);
#endif

	if((thefile=mmap_fopen(resource_file))==NULL){
		asprintf(&temp_buffer,"Error: Cannot open resource file '%s' for reading!",resource_file);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
		return ERROR;
		}

	/* process all lines in the resource file */
	while(1){

		/* free memory */
		my_free((void **)&input);
		my_free((void **)&variable);
		my_free((void **)&value);

		/* read the next line */
		if((input=mmap_fgets(thefile))==NULL)
			break;

		current_line=thefile->current_line;

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]=='\x0' || input[0]=='\n' || input[0]=='\r')
			continue;

		strip(input);

		/* get the variable name */
		if((temp_ptr=my_strtok(input,"="))==NULL){
			asprintf(&temp_buffer,"Error: NULL variable - Line %d of resource file '%s'",current_line,resource_file);
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			error=TRUE;
			break;
			}
		if((variable=(char *)strdup(temp_ptr))==NULL){
			error=TRUE;
			break;
		        }

		/* get the value */
		if((temp_ptr=my_strtok(NULL,"\n"))==NULL){
			asprintf(&temp_buffer,"Error: NULL variable value - Line %d of resource file '%s'",current_line,resource_file);
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			error=TRUE;
			break;
			}
		if((value=(char *)strdup(temp_ptr))==NULL){
			error=TRUE;
			break;
		        }

		/* what should we do with the variable/value pair? */

		/* check for macro declarations */
		if(variable[0]=='$' && variable[strlen(variable)-1]=='$'){
			
			/* $USERx$ macro declarations */
			if(strstr(variable,"$USER")==variable  && strlen(variable)>5){
				user_index=atoi(variable+5)-1;
				if(user_index>=0 && user_index<MAX_USER_MACROS){
					my_free((void **)&macro_user[user_index]);
					macro_user[user_index]=(char *)strdup(value);
#ifdef DEBUG1
					printf("\t\t$USER%d$ set to '%s'\n",user_index+1,macro_user[user_index]);
#endif
				        }
			        }
		        }
	        }

	/* free leftover memory and close the file */
	my_free((void **)&input);
	mmap_fclose(thefile);

	/* free memory */
	my_free((void **)&variable);
	my_free((void **)&value);

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

/* do a pre-flight check to make sure object relationships, etc. make sense */
int pre_flight_check(void){
	char *temp_buffer=NULL;
	host *temp_host=NULL;
	char *buf=NULL;
	service *temp_service=NULL;
	command *temp_command=NULL;
	char *temp_command_name="";
	int warnings=0;
	int errors=0;
	struct timeval tv[4];
	double runtime[4];

#ifdef DEBUG0
	printf("pre_flight_check() start\n");
#endif

	if(test_scheduling==TRUE)
		gettimeofday(&tv[0],NULL);

	/********************************************/
	/* check object relationships               */
	/********************************************/
	pre_flight_object_check(&warnings,&errors);
	if(test_scheduling==TRUE)
		gettimeofday(&tv[1],NULL);

 
	/********************************************/
	/* check for circular paths between hosts   */
	/********************************************/
	pre_flight_circular_check(&warnings,&errors);
	if(test_scheduling==TRUE)
		gettimeofday(&tv[2],NULL);

 
	/********************************************/
	/* check global event handler commands...   */
	/********************************************/
	if(verify_config==TRUE)
		printf("Checking global event handlers...\n");
	if(global_host_event_handler!=NULL){

		/* check the event handler command */
		buf=(char *)strdup(global_host_event_handler);

		/* get the command name, leave any arguments behind */
		temp_command_name=my_strtok(buf,"!");

		temp_command=find_command(temp_command_name);
		if(temp_command==NULL){
			asprintf(&temp_buffer,"Error: Global host event handler command '%s' is not defined anywhere!",temp_command_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* save the pointer to the command for later */
		global_host_event_handler_ptr=temp_command;

		my_free((void **)&buf);
	        }
	if(global_service_event_handler!=NULL){

		/* check the event handler command */
		buf=(char *)strdup(global_service_event_handler);

		/* get the command name, leave any arguments behind */
		temp_command_name=my_strtok(buf,"!");

		temp_command=find_command(temp_command_name);
		if(temp_command==NULL){
			asprintf(&temp_buffer,"Error: Global service event handler command '%s' is not defined anywhere!",temp_command_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* save the pointer to the command for later */
		global_service_event_handler_ptr=temp_command;

		my_free((void **)&buf);
	        }

#ifdef DEBUG1
	printf("\tCompleted global event handler command checks\n");
#endif

	/**************************************************/
	/* check obsessive processor commands...          */
	/**************************************************/
	if(verify_config==TRUE)
		printf("Checking obsessive compulsive processor commands...\n");
	if(ocsp_command!=NULL){

		buf=(char *)strdup(ocsp_command);

		/* get the command name, leave any arguments behind */
		temp_command_name=my_strtok(buf,"!");

	        temp_command=find_command(temp_command_name);
		if(temp_command==NULL){
			asprintf(&temp_buffer,"Error: Obsessive compulsive service processor command '%s' is not defined anywhere!",temp_command_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* save the pointer to the command for later */
		ocsp_command_ptr=temp_command;

		my_free((void **)&buf);
	        }
	if(ochp_command!=NULL){

		buf=(char *)strdup(ochp_command);

		/* get the command name, leave any arguments behind */
		temp_command_name=my_strtok(buf,"!");

	        temp_command=find_command(temp_command_name);
		if(temp_command==NULL){
			asprintf(&temp_buffer,"Error: Obsessive compulsive host processor command '%s' is not defined anywhere!",temp_command_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* save the pointer to the command for later */
		ochp_command_ptr=temp_command;

		my_free((void **)&buf);
	        }

#ifdef DEBUG1
	printf("\tCompleted obsessive compulsive processor command checks\n");
#endif

	/**************************************************/
	/* check various settings...                      */
	/**************************************************/
	if(verify_config==TRUE)
		printf("Checking misc settings...\n");

	/* warn if user didn't specify any illegal macro output chars */
	if(illegal_output_chars==NULL){
		asprintf(&temp_buffer,"%s","Warning: Nothing specified for illegal_macro_output_chars variable!\n");
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
		my_free((void **)&temp_buffer);
		warnings++;
	        }

	/* count number of services associated with each host (we need this for flap detection)... */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
		if((temp_host=find_host(temp_service->host_name))){
			temp_host->total_services++;
			temp_host->total_service_check_interval+=temp_service->check_interval;
		        }
	        }

	if(verify_config==TRUE){
		printf("\n");
		printf("Total Warnings: %d\n",warnings);
		printf("Total Errors:   %d\n",errors);
	        }

	if(test_scheduling==TRUE)
		gettimeofday(&tv[3],NULL);

	if(test_scheduling==TRUE){

		if(verify_object_relationships==TRUE)
			runtime[0]=(double)((double)(tv[1].tv_sec-tv[0].tv_sec)+(double)((tv[1].tv_usec-tv[0].tv_usec)/1000.0)/1000.0);
		else
			runtime[0]=0.0;
		if(verify_circular_paths==TRUE)
			runtime[1]=(double)((double)(tv[2].tv_sec-tv[1].tv_sec)+(double)((tv[2].tv_usec-tv[1].tv_usec)/1000.0)/1000.0);
		else
			runtime[1]=0.0;
		runtime[2]=(double)((double)(tv[3].tv_sec-tv[2].tv_sec)+(double)((tv[3].tv_usec-tv[2].tv_usec)/1000.0)/1000.0);
		runtime[3]=runtime[0]+runtime[1]+runtime[2];

		printf("Timing information on configuration verification is listed below.\n\n");

		printf("CONFIG VERIFICATION TIMES\n");
		printf("----------------------------------\n");
		printf("Object Relationships: %.6lf sec\n",runtime[0]);
		printf("Circular Paths:       %.6lf sec\n",runtime[1]);
		printf("Misc:                 %.6lf sec\n",runtime[2]);
		printf("                      ============\n");
		printf("TOTAL:                %.6lf sec\n",runtime[3]);
		printf("\n\n");
	        }

#ifdef DEBUG0
	printf("pre_flight_check() end\n");
#endif

	return (errors>0)?ERROR:OK;
	}



/* do a pre-flight check to make sure object relationships make sense */
int pre_flight_object_check(int *w, int *e){
	contact *temp_contact=NULL;
	commandsmember *temp_commandsmember=NULL;
	contactgroup *temp_contactgroup=NULL;
	contactgroupmember *temp_contactgroupmember=NULL;
	contactgroupsmember *temp_contactgroupsmember=NULL;
	contactsmember *temp_contactsmember=NULL;
	host *temp_host=NULL;
	host *temp_host2=NULL;
	hostsmember *temp_hostsmember=NULL;
	hostgroup *temp_hostgroup=NULL;
	hostgroupmember *temp_hostgroupmember=NULL;
	servicegroup *temp_servicegroup=NULL;
	servicegroupmember *temp_servicegroupmember=NULL;
	service *temp_service=NULL;
	service *temp_service2=NULL;
	command *temp_command=NULL;
	timeperiod *temp_timeperiod=NULL;
	serviceescalation *temp_se=NULL;
	hostescalation *temp_he=NULL;
	servicedependency *temp_sd=NULL;
	hostdependency *temp_hd=NULL;
	char *temp_buffer=NULL;
	char *buf=NULL;
	char *temp_command_name="";
	int found=FALSE;
	int total_objects=0;
	int warnings=0;
	int errors=0;

#ifdef DEBUG0
	printf("pre_flight_object_check() start\n");
#endif


	/* bail out if we aren't supposed to verify object relationships */
	if(verify_object_relationships==FALSE)
		return OK;


	/*****************************************/
	/* check each service...                 */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking services...\n");
	if(service_hashlist==NULL){
		asprintf(&temp_buffer,"Error: There are no services defined!");
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
		my_free((void **)&temp_buffer);
		errors++;
	        }
	total_objects=0;
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		total_objects++;
		found=FALSE;

		/* check for a valid host */
		temp_host=find_host(temp_service->host_name);

		/* we couldn't find an associated host! */
		if(!temp_host){
			asprintf(&temp_buffer,"Error: Host '%s' specified in service '%s' not defined anywhere!",temp_service->host_name,temp_service->description);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
			}

		/* save the host pointer for later */
		temp_service->host_ptr=temp_host;

		/* check the event handler command */
		if(temp_service->event_handler!=NULL){

			/* check the event handler command */
			buf=(char *)strdup(temp_service->event_handler);

			/* get the command name, leave any arguments behind */
			temp_command_name=my_strtok(buf,"!");

			temp_command=find_command(temp_command_name);
			if(temp_command==NULL){
				asprintf(&temp_buffer,"Error: Event handler command '%s' specified in service '%s' for host '%s' not defined anywhere",temp_command_name,temp_service->description,temp_service->host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			my_free((void **)&buf);

			/* save the pointer to the event handler for later */
			temp_service->event_handler_ptr=temp_command;
		        }

		/* check the service check_command */
		buf=(char *)strdup(temp_service->service_check_command);

		/* get the command name, leave any arguments behind */
		temp_command_name=my_strtok(buf,"!");

		temp_command=find_command(temp_command_name);
		if(temp_command==NULL){
			asprintf(&temp_buffer,"Error: Service check command '%s' specified in service '%s' for host '%s' not defined anywhere!",temp_command_name,temp_service->description,temp_service->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		my_free((void **)&buf);

		/* save the pointer to the check command for later */
		temp_service->check_command_ptr=temp_command;

		/* check for sane recovery options */
		if(temp_service->notify_on_recovery==TRUE && temp_service->notify_on_warning==FALSE && temp_service->notify_on_critical==FALSE){
			asprintf(&temp_buffer,"Warning: Recovery notification option in service '%s' for host '%s' doesn't make any sense - specify warning and/or critical options as well",temp_service->description,temp_service->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }

		/* reset the found flag */
		found=FALSE;

#ifdef REMOVED_07182006
		/* check for valid contactgroups */
		for(temp_contactgroupsmember=temp_service->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){

			temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name);

			if(temp_contactgroup==NULL){
				asprintf(&temp_buffer,"Error: Contact group '%s' specified in service '%s' for host '%s' is not defined anywhere!",temp_contactgroupsmember->group_name,temp_service->description,temp_service->host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the group pointer for later */
			temp_contactgroupsmember->group_ptr=temp_contactgroup;
			}

		/* check to see if there is at least one contact group */
		if(temp_service->contact_groups==NULL){
			asprintf(&temp_buffer,"Warning: Service '%s' on host '%s'  has no default contact group(s) defined!",temp_service->description,temp_service->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }
#endif

		/* check for valid contacts */
		for(temp_contactsmember=temp_service->contacts;temp_contactsmember!=NULL;temp_contactsmember=temp_contactsmember->next){

			temp_contact=find_contact(temp_contactsmember->contact_name);

			if(temp_contact==NULL){
				asprintf(&temp_buffer,"Error: Contact '%s' specified in service '%s' for host '%s' is not defined anywhere!",temp_contactsmember->contact_name,temp_service->description,temp_service->host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the contact pointer for later */
			temp_contactsmember->contact_ptr=temp_contact;
			}

		/* check to see if there is at least one contact */
		if(temp_service->contacts==NULL){
			asprintf(&temp_buffer,"Warning: Service '%s' on host '%s' has no default contact(s) defined!",temp_service->description,temp_service->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }

		/* verify service check timeperiod */
		if(temp_service->check_period==NULL){
			asprintf(&temp_buffer,"Warning: Service '%s' on host '%s' has no check time period defined!",temp_service->description,temp_service->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }
		else{
		        temp_timeperiod=find_timeperiod(temp_service->check_period);
			if(temp_timeperiod==NULL){
				asprintf(&temp_buffer,"Error: Check period '%s' specified for service '%s' on host '%s' is not defined anywhere!",temp_service->check_period,temp_service->description,temp_service->host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the pointer to the check timeperiod for later */
			temp_service->check_period_ptr=temp_timeperiod;
		        }

		/* check service notification timeperiod */
		if(temp_service->notification_period==NULL){
			asprintf(&temp_buffer,"Warning: Service '%s' on host '%s' has no notification time period defined!",temp_service->description,temp_service->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }

		else{
		        temp_timeperiod=find_timeperiod(temp_service->notification_period);
			if(temp_timeperiod==NULL){
				asprintf(&temp_buffer,"Error: Notification period '%s' specified for service '%s' on host '%s' is not defined anywhere!",temp_service->notification_period,temp_service->description,temp_service->host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the pointer to the notification timeperiod for later */
			temp_service->notification_period_ptr=temp_timeperiod;
		        }

		/* see if the notification interval is less than the check interval */
		if(temp_service->notification_interval<temp_service->check_interval && temp_service->notification_interval!=0){
			asprintf(&temp_buffer,"Warning: Service '%s' on host '%s'  has a notification interval less than its check interval!  Notifications are only re-sent after checks are made, so the effective notification interval will be that of the check interval.",temp_service->description,temp_service->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }

		/* check for illegal characters in service description */
		if(contains_illegal_object_chars(temp_service->description)==TRUE){
			asprintf(&temp_buffer,"Error: The description string for service '%s' on host '%s' contains one or more illegal characters.",temp_service->description,temp_service->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
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
	if(host_hashlist==NULL){
		asprintf(&temp_buffer,"Error: There are no hosts defined!");
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
		my_free((void **)&temp_buffer);
		errors++;
	        }
	total_objects=0;
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		total_objects++;
		found=FALSE;

		/* make sure each host has at least one service associated with it */
		for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
			if(!strcmp(temp_service->host_name,temp_host->name)){
				found=TRUE;
				break;
			        }
			}

		/* we couldn't find a service associated with this host! */
		if(found==FALSE){
			asprintf(&temp_buffer,"Warning: Host '%s' has no services associated with it!",temp_host->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
			}

		found=FALSE;

		/* check the event handler command */
		if(temp_host->event_handler!=NULL){

			/* check the event handler command */
			buf=(char *)strdup(temp_host->event_handler);

			/* get the command name, leave any arguments behind */
			temp_command_name=my_strtok(buf,"!");

			temp_command=find_command(temp_command_name);
			if(temp_command==NULL){
				asprintf(&temp_buffer,"Error: Event handler command '%s' specified for host '%s' not defined anywhere",temp_command_name,temp_host->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			my_free((void **)&buf);

			/* save the pointer to the event handler command for later */
			temp_host->event_handler_ptr=temp_command;
		        }

		/* hosts that don't have check commands defined shouldn't ever be checked... */
		if(temp_host->host_check_command!=NULL){

			/* check the host check_command */
			buf=(char *)strdup(temp_host->host_check_command);

			/* get the command name, leave any arguments behind */
			temp_command_name=my_strtok(buf,"!");

			temp_command=find_command(temp_command_name);
			if(temp_command==NULL){
				asprintf(&temp_buffer,"Error: Host check command '%s' specified for host '%s' is not defined anywhere!",temp_command_name,temp_host->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
		                }

			/* save the pointer to the check command for later */
			temp_host->check_command_ptr=temp_command;

			my_free((void **)&buf);
		        }

		/* check host check timeperiod */
		if(temp_host->check_period!=NULL){
		        temp_timeperiod=find_timeperiod(temp_host->check_period);
			if(temp_timeperiod==NULL){
				asprintf(&temp_buffer,"Error: Check period '%s' specified for host '%s' is not defined anywhere!",temp_host->check_period,temp_host->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the pointer to the check timeperiod for later */
			temp_host->check_period_ptr=temp_timeperiod;
		        }

#ifdef REMOVED_07182006
		/* check all contact groups */
		for(temp_contactgroupsmember=temp_host->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){

			temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name);

			if(temp_contactgroup==NULL){
				asprintf(&temp_buffer,"Error: Contact group '%s' specified in host '%s' is not defined anywhere!",temp_contactgroupsmember->group_name,temp_host->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the group pointer for later */
			temp_contactgroupsmember->group_ptr=temp_contactgroup;
			}

		/* check to see if there is at least one contact group */
		if(temp_host->contact_groups==NULL){
			asprintf(&temp_buffer,"Warning: Host '%s' has no default contact group(s) defined!",temp_host->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }
#endif

		/* check all contacts */
		for(temp_contactsmember=temp_host->contacts;temp_contactsmember!=NULL;temp_contactsmember=temp_contactsmember->next){

			temp_contact=find_contact(temp_contactsmember->contact_name);

			if(temp_contact==NULL){
				asprintf(&temp_buffer,"Error: Contact '%s' specified in host '%s' is not defined anywhere!",temp_contactsmember->contact_name,temp_host->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the contact pointer for later */
			temp_contactsmember->contact_ptr=temp_contact;
			}

		/* check to see if there is at least one contact */
		if(temp_host->contacts==NULL){
			asprintf(&temp_buffer,"Warning: Host '%s' has no default contact(s) defined!",temp_host->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }

		/* check notification timeperiod */
		if(temp_host->notification_period!=NULL){
		        temp_timeperiod=find_timeperiod(temp_host->notification_period);
			if(temp_timeperiod==NULL){
				asprintf(&temp_buffer,"Error: Notification period '%s' specified for host '%s' is not defined anywhere!",temp_host->notification_period,temp_host->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the pointer to the notification timeperiod for later */
			temp_host->notification_period_ptr=temp_timeperiod;
		        }

		/* check all parent parent host */
		for(temp_hostsmember=temp_host->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

			if((temp_host2=find_host(temp_hostsmember->host_name))==NULL){
				asprintf(&temp_buffer,"Error: '%s' is not a valid parent for host '%s'!",temp_hostsmember->host_name,temp_host->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
		                }

			/* save the parent host pointer for later */
			temp_hostsmember->host_ptr=temp_host2;
		        }

		/* check for sane recovery options */
		if(temp_host->notify_on_recovery==TRUE && temp_host->notify_on_down==FALSE && temp_host->notify_on_unreachable==FALSE){
			asprintf(&temp_buffer,"Warning: Recovery notification option in host '%s' definition doesn't make any sense - specify down and/or unreachable options as well",temp_host->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }

		/* check for illegal characters in host name */
		if(contains_illegal_object_chars(temp_host->name)==TRUE){
			asprintf(&temp_buffer,"Error: The name of host '%s' contains one or more illegal characters.",temp_host->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
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
#ifdef REMOVED_12112006
	if(hostgroup_list==NULL){
		asprintf(&temp_buffer,"Error: There are no host groups defined!");
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
		my_free((void **)&temp_buffer);
		errors++;
	        }
#endif
	for(temp_hostgroup=hostgroup_list,total_objects=0;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next,total_objects++){

		/* check all group members */
		for(temp_hostgroupmember=temp_hostgroup->members;temp_hostgroupmember!=NULL;temp_hostgroupmember=temp_hostgroupmember->next){

			temp_host=find_host(temp_hostgroupmember->host_name);
			if(temp_host==NULL){
				asprintf(&temp_buffer,"Error: Host '%s' specified in host group '%s' is not defined anywhere!",temp_hostgroupmember->host_name,temp_hostgroup->group_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save host pointer for later */
			temp_hostgroupmember->host_ptr=temp_host;
		        }

		/* check for illegal characters in hostgroup name */
		if(contains_illegal_object_chars(temp_hostgroup->group_name)==TRUE){
			asprintf(&temp_buffer,"Error: The name of hostgroup '%s' contains one or more illegal characters.",temp_hostgroup->group_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }
		}

	if(verify_config==TRUE)
		printf("\tChecked %d host groups.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted hostgroup verification checks\n");
#endif


	/*****************************************/
	/* check each service group...           */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking service groups...\n");
	for(temp_servicegroup=servicegroup_list,total_objects=0;temp_servicegroup!=NULL;temp_servicegroup=temp_servicegroup->next,total_objects++){

		/* check all group members */
		for(temp_servicegroupmember=temp_servicegroup->members;temp_servicegroupmember!=NULL;temp_servicegroupmember=temp_servicegroupmember->next){

			temp_service=find_service(temp_servicegroupmember->host_name,temp_servicegroupmember->service_description);
			if(temp_service==NULL){
				asprintf(&temp_buffer,"Error: Service '%s' on host '%s' specified in service group '%s' is not defined anywhere!",temp_servicegroupmember->service_description,temp_servicegroupmember->host_name,temp_servicegroup->group_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save service pointer for later */
			temp_servicegroupmember->service_ptr=temp_service;
		        }

		/* check for illegal characters in servicegroup name */
		if(contains_illegal_object_chars(temp_servicegroup->group_name)==TRUE){
			asprintf(&temp_buffer,"Error: The name of servicegroup '%s' contains one or more illegal characters.",temp_servicegroup->group_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }
		}

	if(verify_config==TRUE)
		printf("\tChecked %d service groups.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted servicegroup verification checks\n");
#endif


	/*****************************************/
	/* check all contacts...                 */
	/*****************************************/
	if(verify_config==TRUE)
		printf("Checking contacts...\n");
	if(contact_list==NULL){
		asprintf(&temp_buffer,"Error: There are no contacts defined!");
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
		my_free((void **)&temp_buffer);
		errors++;
	        }
	for(temp_contact=contact_list,total_objects=0;temp_contact!=NULL;temp_contact=temp_contact->next,total_objects++){

		/* check service notification commands */
		if(temp_contact->service_notification_commands==NULL){
			asprintf(&temp_buffer,"Error: Contact '%s' has no service notification commands defined!",temp_contact->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }
		else for(temp_commandsmember=temp_contact->service_notification_commands;temp_commandsmember!=NULL;temp_commandsmember=temp_commandsmember->next){

			/* check the host notification command */
			buf=(char *)strdup(temp_commandsmember->command);

			/* get the command name, leave any arguments behind */
			temp_command_name=my_strtok(buf,"!");

			temp_command=find_command(temp_command_name);
			if(temp_command==NULL){
				asprintf(&temp_buffer,"Error: Service notification command '%s' specified for contact '%s' is not defined anywhere!",temp_command_name,temp_contact->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save pointer to the command for later */
			temp_commandsmember->command_ptr=temp_command;

			my_free((void **)&buf);
		        }

		/* check host notification commands */
		if(temp_contact->host_notification_commands==NULL){
			asprintf(&temp_buffer,"Error: Contact '%s' has no host notification commands defined!",temp_contact->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }
		else for(temp_commandsmember=temp_contact->host_notification_commands;temp_commandsmember!=NULL;temp_commandsmember=temp_commandsmember->next){

			/* check the host notification command */
			buf=(char *)strdup(temp_commandsmember->command);

			/* get the command name, leave any arguments behind */
			temp_command_name=my_strtok(buf,"!");

			temp_command=find_command(temp_command_name);
			if(temp_command==NULL){
				asprintf(&temp_buffer,"Error: Host notification command '%s' specified for contact '%s' is not defined anywhere!",temp_command_name,temp_contact->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save pointer to the command for later */
			temp_commandsmember->command_ptr=temp_command;

			my_free((void **)&buf);
	                }

		/* check service notification timeperiod */
		if(temp_contact->service_notification_period==NULL){
			asprintf(&temp_buffer,"Warning: Contact '%s' has no service notification time period defined!",temp_contact->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }

		else{
		        temp_timeperiod=find_timeperiod(temp_contact->service_notification_period);
			if(temp_timeperiod==NULL){
				asprintf(&temp_buffer,"Error: Service notification period '%s' specified for contact '%s' is not defined anywhere!",temp_contact->service_notification_period,temp_contact->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the pointer to the service notification timeperiod for later */
			temp_contact->service_notification_period_ptr=temp_timeperiod;
		        }

		/* check host notification timeperiod */
		if(temp_contact->host_notification_period==NULL){
			asprintf(&temp_buffer,"Warning: Contact '%s' has no host notification time period defined!",temp_contact->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }

		else{
		        temp_timeperiod=find_timeperiod(temp_contact->host_notification_period);
			if(temp_timeperiod==NULL){
				asprintf(&temp_buffer,"Error: Host notification period '%s' specified for contact '%s' is not defined anywhere!",temp_contact->host_notification_period,temp_contact->name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the pointer to the host notification timeperiod for later */
			temp_contact->host_notification_period_ptr=temp_timeperiod;
		        }

#ifdef REMOVED_07182006
		found=FALSE;

		/* make sure the contact belongs to at least one contact group */
		for(temp_contactgroup=contactgroup_list;temp_contactgroup!=NULL;temp_contactgroup=temp_contactgroup->next){
			temp_contactgroupmember=find_contactgroupmember(temp_contact->name,temp_contactgroup);
			if(temp_contactgroupmember!=NULL){
				found=TRUE;
				break;
			        }
		        }

		/* we couldn't find the contact in any contact groups */
		if(found==FALSE){
			asprintf(&temp_buffer,"Warning: Contact '%s' is not a member of any contact groups!",temp_contact->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }
#endif
	
		/* check for sane host recovery options */
		if(temp_contact->notify_on_host_recovery==TRUE && temp_contact->notify_on_host_down==FALSE && temp_contact->notify_on_host_unreachable==FALSE){
			asprintf(&temp_buffer,"Warning: Host recovery notification option for contact '%s' doesn't make any sense - specify down and/or unreachable options as well",temp_contact->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }

		/* check for sane service recovery options */
		if(temp_contact->notify_on_service_recovery==TRUE && temp_contact->notify_on_service_critical==FALSE && temp_contact->notify_on_service_warning==FALSE){
			asprintf(&temp_buffer,"Warning: Service recovery notification option for contact '%s' doesn't make any sense - specify critical and/or warning options as well",temp_contact->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
		        }

		/* check for illegal characters in contact name */
		if(contains_illegal_object_chars(temp_contact->name)==TRUE){
			asprintf(&temp_buffer,"Error: The name of contact '%s' contains one or more illegal characters.",temp_contact->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
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
		asprintf(&temp_buffer,"Error: There are no contact groups defined!\n");
		write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
		my_free((void **)&temp_buffer);
		errors++;
	        }
	for(temp_contactgroup=contactgroup_list,total_objects=0;temp_contactgroup!=NULL;temp_contactgroup=temp_contactgroup->next,total_objects++){

		found=FALSE;

#ifdef REMOVED_07182006
		/* make sure each contactgroup is used in at least one host or service definition or escalation */
		for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
			for(temp_contactgroupsmember=temp_host->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
				if(!strcmp(temp_contactgroup->group_name,temp_contactgroupsmember->group_name)){
					found=TRUE;
					break;
			                }
		                 }
			if(found==TRUE)
				break;
		        }
		if(found==FALSE){
			for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){
				for(temp_contactgroupsmember=temp_service->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
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
			asprintf(&temp_buffer,"Warning: Contact group '%s' is not used in any host/service definitions or host/service escalations!",temp_contactgroup->group_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_WARNING,TRUE);
			my_free((void **)&temp_buffer);
			warnings++;
			}
#endif

		/* check all the group members */
		for(temp_contactgroupmember=temp_contactgroup->members;temp_contactgroupmember!=NULL;temp_contactgroupmember=temp_contactgroupmember->next){

			temp_contact=find_contact(temp_contactgroupmember->contact_name);
			if(temp_contact==NULL){
				asprintf(&temp_buffer,"Error: Contact '%s' specified in contact group '%s' is not defined anywhere!",temp_contactgroupmember->contact_name,temp_contactgroup->group_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the contact pointer for later */
			temp_contactgroupmember->contact_ptr=temp_contact;
		        }

		/* check for illegal characters in contactgroup name */
		if(contains_illegal_object_chars(temp_contactgroup->group_name)==TRUE){
			asprintf(&temp_buffer,"Error: The name of contact group '%s' contains one or more illegal characters.",temp_contactgroup->group_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
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
			asprintf(&temp_buffer,"Error: Service '%s' on host '%s' specified in service escalation is not defined anywhere!",temp_se->description,temp_se->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* save the service pointer for later */
		temp_se->service_ptr=temp_service;

		/* find the timeperiod */
		if(temp_se->escalation_period!=NULL){
		        temp_timeperiod=find_timeperiod(temp_se->escalation_period);
			if(temp_timeperiod==NULL){
				asprintf(&temp_buffer,"Error: Escalation period '%s' specified in service escalation for service '%s' on host '%s' is not defined anywhere!",temp_se->escalation_period,temp_se->description,temp_se->host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the timeperiod pointer for later */
			temp_se->escalation_period_ptr=temp_timeperiod;
		        }

#ifdef REMOVED_07182006
		/* find the contact groups */
		for(temp_contactgroupsmember=temp_se->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
			
			/* find the contact group */
			temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name);
			if(temp_contactgroup==NULL){
				asprintf(&temp_buffer,"Error: Contact group '%s' specified in service escalation for service '%s' on host '%s' is not defined anywhere!",temp_contactgroupsmember->group_name,temp_se->description,temp_se->host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the group pointer for later */
			temp_contactgroupsmember->group_ptr=temp_contactgroup;
		        }
#endif

		/* find the contacts */
		for(temp_contactsmember=temp_se->contacts;temp_contactsmember!=NULL;temp_contactsmember=temp_contactsmember->next){
			
			/* find the contact */
			temp_contact=find_contact(temp_contactsmember->contact_name);
			if(temp_contact==NULL){
				asprintf(&temp_buffer,"Error: Contact '%s' specified in service escalation for service '%s' on host '%s' is not defined anywhere!",temp_contactsmember->contact_name,temp_se->description,temp_se->host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the contact pointer for later */
			temp_contactsmember->contact_ptr=temp_contact;
		        }
	        }

	if(verify_config==TRUE)
		printf("\tChecked %d service escalations.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted service escalation checks\n");
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
			asprintf(&temp_buffer,"Error: Dependent service '%s' on host '%s' specified in service dependency for service '%s' on host '%s' is not defined anywhere!",temp_sd->dependent_service_description,temp_sd->dependent_host_name,temp_sd->service_description,temp_sd->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* save pointer for later */
		temp_sd->dependent_service_ptr=temp_service;

		/* find the service we're depending on */
		temp_service2=find_service(temp_sd->host_name,temp_sd->service_description);
		if(temp_service2==NULL){
			asprintf(&temp_buffer,"Error: Service '%s' on host '%s' specified in service dependency for service '%s' on host '%s' is not defined anywhere!",temp_sd->service_description,temp_sd->host_name,temp_sd->dependent_service_description,temp_sd->dependent_host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* save pointer for later */
		temp_sd->master_service_ptr=temp_service2;

		/* make sure they're not the same service */
		if(temp_service==temp_service2){
			asprintf(&temp_buffer,"Error: Service dependency definition for service '%s' on host '%s' is circular (it depends on itself)!",temp_sd->dependent_service_description,temp_sd->dependent_host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* find the timeperiod */
		if(temp_sd->dependency_period!=NULL){
		        temp_timeperiod=find_timeperiod(temp_sd->dependency_period);
			if(temp_timeperiod==NULL){
				asprintf(&temp_buffer,"Error: Dependency period '%s' specified in service dependency for service '%s' on host '%s' is not defined anywhere!",temp_sd->dependency_period,temp_sd->dependent_service_description,temp_sd->dependent_host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the timeperiod pointer for later */
			temp_sd->dependency_period_ptr=temp_timeperiod;
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
			asprintf(&temp_buffer,"Error: Host '%s' specified in host escalation is not defined anywhere!",temp_he->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* save the host pointer for later */
		temp_he->host_ptr=temp_host;

		/* find the timeperiod */
		if(temp_he->escalation_period!=NULL){
		        temp_timeperiod=find_timeperiod(temp_he->escalation_period);
			if(temp_timeperiod==NULL){
				asprintf(&temp_buffer,"Error: Escalation period '%s' specified in host escalation for host '%s' is not defined anywhere!",temp_he->escalation_period,temp_he->host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the timeperiod pointer for later */
			temp_he->escalation_period_ptr=temp_timeperiod;
		        }

#ifdef REMOVED_07182006
		/* find the contact groups */
		for(temp_contactgroupsmember=temp_he->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){
			
			/* find the contact group */
			temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name);
			if(temp_contactgroup==NULL){
				asprintf(&temp_buffer,"Error: Contact group '%s' specified in host escalation for host '%s' is not defined anywhere!",temp_contactgroupsmember->group_name,temp_he->host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the group pointer for later */
			temp_contactgroupsmember->group_ptr=temp_contactgroup;
		        }
#endif

		/* find the contacts */
		for(temp_contactsmember=temp_he->contacts;temp_contactsmember!=NULL;temp_contactsmember=temp_contactsmember->next){
			
			/* find the contact*/
			temp_contact=find_contact(temp_contactsmember->contact_name);
			if(temp_contact==NULL){
				asprintf(&temp_buffer,"Error: Contact '%s' specified in host escalation for host '%s' is not defined anywhere!",temp_contactsmember->contact_name,temp_he->host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the contact pointer for later */
			temp_contactsmember->contact_ptr=temp_contact;
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
			asprintf(&temp_buffer,"Error: Dependent host specified in host dependency for host '%s' is not defined anywhere!",temp_hd->dependent_host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* save pointer for later */
		temp_hd->dependent_host_ptr=temp_host2;

		/* find the host we're depending on */
		temp_host2=find_host(temp_hd->host_name);
		if(temp_host2==NULL){
			asprintf(&temp_buffer,"Error: Host specified in host dependency for host '%s' is not defined anywhere!",temp_hd->dependent_host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* save pointer for later */
		temp_hd->master_host_ptr=temp_host2;

		/* make sure they're not the same host */
		if(temp_host==temp_host2){
			asprintf(&temp_buffer,"Error: Host dependency definition for host '%s' is circular (it depends on itself)!",temp_hd->dependent_host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }

		/* find the timeperiod */
		if(temp_hd->dependency_period!=NULL){
		        temp_timeperiod=find_timeperiod(temp_hd->dependency_period);
			if(temp_timeperiod==NULL){
				asprintf(&temp_buffer,"Error: Dependency period '%s' specified in host dependency for host '%s' is not defined anywhere!",temp_hd->dependency_period,temp_hd->dependent_host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
				my_free((void **)&temp_buffer);
				errors++;
			        }

			/* save the timeperiod pointer for later */
			temp_hd->dependency_period_ptr=temp_timeperiod;
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
			asprintf(&temp_buffer,"Error: The name of command '%s' contains one or more illegal characters.",temp_command->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
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
			asprintf(&temp_buffer,"Error: The name of time period '%s' contains one or more illegal characters.",temp_timeperiod->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }
	        }

	if(verify_config==TRUE)
		printf("\tChecked %d time periods.\n",total_objects);

#ifdef DEBUG1
	printf("\tCompleted command checks\n");
#endif


       /* update warning and error count */
	*w+=warnings;
	*e+=errors;
 
#ifdef DEBUG0
	printf("pre_flight_object_check() end\n");
#endif

	return (errors>0)?ERROR:OK;
	}


/* check for circular paths and dependencies */
int pre_flight_circular_check(int *w, int *e){
	host *temp_host=NULL;
	host *temp_host2=NULL;
	servicedependency *temp_sd=NULL;
	servicedependency *temp_sd2=NULL;
	hostdependency *temp_hd=NULL;
	hostdependency *temp_hd2=NULL;
	char *temp_buffer=NULL;
	int found=FALSE;
	int result=OK;
	int warnings=0;
	int errors=0;

#ifdef DEBUG0
	printf("pre_flight_circular_check() start\n");
#endif


	/* bail out if we aren't supposed to verify circular paths */
	if(verify_circular_paths==FALSE)
		return OK;


	/********************************************/
	/* check for circular paths between hosts   */
	/********************************************/
	if(verify_config==TRUE)
		printf("Checking for circular paths between hosts...\n");

	/* check routes between all hosts */
	found=FALSE;
	result=OK;
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* clear checked flag for all hosts */
		for(temp_host2=host_list;temp_host2!=NULL;temp_host2=temp_host2->next)
			temp_host2->circular_path_checked=FALSE;

		found=check_for_circular_host_path(temp_host,temp_host);
		if(found==TRUE){
			asprintf(&temp_buffer,"Error: There is a circular parent/child path that exists for host '%s'!",temp_host->name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
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
		printf("Checking for circular host and service dependencies...\n");

	/* check execution dependencies between all services */
	for(temp_sd=servicedependency_list;temp_sd!=NULL;temp_sd=temp_sd->next){

		/* clear checked flag for all dependencies */
		for(temp_sd2=servicedependency_list;temp_sd2!=NULL;temp_sd2=temp_sd2->next)
			temp_sd2->circular_path_checked=FALSE;

		found=check_for_circular_servicedependency_path(temp_sd,temp_sd,EXECUTION_DEPENDENCY);
		if(found==TRUE){
			asprintf(&temp_buffer,"Error: A circular execution dependency (which could result in a deadlock) exists for service '%s' on host '%s'!",temp_sd->service_description,temp_sd->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }
	        }

	/* check notification dependencies between all services */
	for(temp_sd=servicedependency_list;temp_sd!=NULL;temp_sd=temp_sd->next){

		/* clear checked flag for all dependencies */
		for(temp_sd2=servicedependency_list;temp_sd2!=NULL;temp_sd2=temp_sd2->next)
			temp_sd2->circular_path_checked=FALSE;

		found=check_for_circular_servicedependency_path(temp_sd,temp_sd,NOTIFICATION_DEPENDENCY);
		if(found==TRUE){
			asprintf(&temp_buffer,"Error: A circular notification dependency (which could result in a deadlock) exists for service '%s' on host '%s'!",temp_sd->service_description,temp_sd->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }
	        }

	/* check execution dependencies between all hosts */
	for(temp_hd=hostdependency_list;temp_hd!=NULL;temp_hd=temp_hd->next){

		/* clear checked flag for all dependencies */
		for(temp_hd2=hostdependency_list;temp_hd2!=NULL;temp_hd2=temp_hd2->next)
			temp_hd2->circular_path_checked=FALSE;

		found=check_for_circular_hostdependency_path(temp_hd,temp_hd,EXECUTION_DEPENDENCY);
		if(found==TRUE){
			asprintf(&temp_buffer,"Error: A circular execution dependency (which could result in a deadlock) exists for host '%s'!",temp_hd->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }
	        }

	/* check notification dependencies between all hosts */
	for(temp_hd=hostdependency_list;temp_hd!=NULL;temp_hd=temp_hd->next){

		/* clear checked flag for all dependencies */
		for(temp_hd2=hostdependency_list;temp_hd2!=NULL;temp_hd2=temp_hd2->next)
			temp_hd2->circular_path_checked=FALSE;

		found=check_for_circular_hostdependency_path(temp_hd,temp_hd,NOTIFICATION_DEPENDENCY);
		if(found==TRUE){
			asprintf(&temp_buffer,"Error: A circular notification dependency (which could result in a deadlock) exists for host '%s'!",temp_hd->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_VERIFICATION_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			errors++;
		        }
	        }

#ifdef DEBUG1
	printf("\tCompleted circular host and service dependency checks\n");
#endif

        /* update warning and error count */
	*w+=warnings;
	*e+=errors;
 
#ifdef DEBUG0
	printf("pre_flight_check() end\n");
#endif

	return (errors>0)?ERROR:OK;
	}

