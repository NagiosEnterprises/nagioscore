/*****************************************************************************
 *
 * UTILS.C - Miscellaneous utility functions for Nagios
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   11-10-2002
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
#include "../common/statusdata.h"

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
 
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
 
#include "nagios.h"

#ifdef EMBEDDEDPERL
#include <EXTERN.h>
#include <perl.h>
static PerlInterpreter *my_perl;
#include <fcntl.h>
/* In perl.h (or friends) there is a macro that defines sighandler as Perl_sighandler, so we must #undef it so we can use our sighandler() function */
#undef sighandler
#endif

char            *my_strtok_buffer=NULL;
char            *original_my_strtok_buffer=NULL;


extern char	config_file[MAX_FILENAME_LENGTH];
extern char	log_file[MAX_FILENAME_LENGTH];
extern char     command_file[MAX_FILENAME_LENGTH];
extern char     temp_file[MAX_FILENAME_LENGTH];
extern char     lock_file[MAX_FILENAME_LENGTH];
extern char	log_archive_path[MAX_FILENAME_LENGTH];

extern char     *nagios_user;
extern char     *nagios_group;

extern char     *macro_contact_name;
extern char     *macro_contact_alias;
extern char	*macro_host_name;
extern char	*macro_host_alias;
extern char	*macro_host_address;
extern char	*macro_service_description;
extern char	*macro_service_state;
extern char	*macro_date_time[7];
extern char	*macro_output;
extern char	*macro_perfdata;
extern char     *macro_contact_email;
extern char     *macro_contact_pager;
extern char     *macro_admin_email;
extern char     *macro_admin_pager;
extern char     *macro_host_state;
extern char     *macro_state_type;
extern char     *macro_execution_time;
extern char     *macro_latency;
extern char     *macro_current_service_attempt;
extern char     *macro_current_host_attempt;
extern char     *macro_notification_type;
extern char     *macro_notification_number;
extern char     *macro_argv[MAX_COMMAND_ARGUMENTS];
extern char     *macro_user[MAX_USER_MACROS];

extern char     *global_host_event_handler;
extern char     *global_service_event_handler;

extern char     *ocsp_command;

extern char     *illegal_object_chars;
extern char     *illegal_output_chars;

extern int      sigshutdown;
extern int      sigrestart;

extern int      daemon_mode;

extern int	use_syslog;
extern int      log_notifications;
extern int      log_service_retries;
extern int      log_host_retries;
extern int      log_event_handlers;
extern int      log_external_commands;
extern int      log_passive_checks;

extern unsigned long      logging_options;
extern unsigned long      syslog_options;

extern int      service_check_timeout;
extern int      host_check_timeout;
extern int      event_handler_timeout;
extern int      notification_timeout;
extern int      ocsp_timeout;

extern int      log_initial_states;

extern int      sleep_time;
extern int      interval_length;
extern int      inter_check_delay_method;
extern int      interleave_factor_method;

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

extern time_t   last_command_check;
extern time_t   last_log_rotation;

extern int      verify_config;

extern service_message svc_msg;
extern int      ipc_pipe[2];

extern int      max_parallel_service_checks;
extern int      currently_running_service_checks;

extern int      enable_notifications;
extern int      execute_service_checks;
extern int      accept_passive_service_checks;
extern int      enable_event_handlers;
extern int      obsess_over_services;
extern int      enable_failure_prediction;
extern int      process_performance_data;

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

extern contact		*contact_list;
extern contactgroup	*contactgroup_list;
extern host		*host_list;
extern hostgroup	*hostgroup_list;
extern service		*service_list;
extern timed_event      *event_list_high;
extern timed_event      *event_list_low;
extern notification     *notification_list;
extern command          *command_list;
extern timeperiod       *timeperiod_list;

extern int      command_file_fd;
extern FILE     *command_file_fp;
extern int      command_file_created;

char my_system_output[MAX_INPUT_BUFFER];

#ifdef HAVE_TZNAME
extern char     *tzname[2];
#endif

extern service_message svc_msg;

extern int errno;



/******************************************************************/
/************************ MACRO FUNCTIONS *************************/
/******************************************************************/

/* replace macros in notification commands with their values */
int process_macros(char *input_buffer,char *output_buffer,int buffer_length,int options){
	char *temp_buffer;
	int in_macro;
	int arg_index=0;
	int user_index=0;

#ifdef DEBUG0
	printf("process_macros() start\n");
#endif

	strcpy(output_buffer,"");

	in_macro=FALSE;

	for(temp_buffer=my_strtok(input_buffer,"$");temp_buffer!=NULL;temp_buffer=my_strtok(NULL,"$")){

		if(in_macro==FALSE){
			if(strlen(output_buffer)+strlen(temp_buffer)<buffer_length-1){
				strncat(output_buffer,temp_buffer,buffer_length-strlen(output_buffer)-1);
				output_buffer[buffer_length-1]='\x0';
			        }
			in_macro=TRUE;
			}
		else{

			if(strlen(output_buffer)+strlen(temp_buffer)<buffer_length-1){

				if(!strcmp(temp_buffer,"HOSTNAME"))
					strncat(output_buffer,(macro_host_name==NULL)?"":macro_host_name,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"HOSTALIAS"))
					strncat(output_buffer,(macro_host_alias==NULL)?"":macro_host_alias,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"HOSTADDRESS"))
					strncat(output_buffer,(macro_host_address==NULL)?"":macro_host_address,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"SERVICEDESC"))
					strncat(output_buffer,(macro_service_description==NULL)?"":macro_service_description,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"SERVICESTATE"))
					strncat(output_buffer,(macro_service_state==NULL)?"":macro_service_state,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"SERVICEATTEMPT"))
					strncat(output_buffer,(macro_current_service_attempt==NULL)?"":macro_current_service_attempt,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"DATETIME") || !strcmp(temp_buffer,"LONGDATETIME"))
					strncat(output_buffer,(macro_date_time[MACRO_DATETIME_LONGDATE]==NULL)?"":macro_date_time[MACRO_DATETIME_LONGDATE],buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"SHORTDATETIME"))
					strncat(output_buffer,(macro_date_time[MACRO_DATETIME_SHORTDATE]==NULL)?"":macro_date_time[MACRO_DATETIME_SHORTDATE],buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"DATE"))
					strncat(output_buffer,(macro_date_time[MACRO_DATETIME_DATE]==NULL)?"":macro_date_time[MACRO_DATETIME_DATE],buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"TIME"))
					strncat(output_buffer,(macro_date_time[MACRO_DATETIME_TIME]==NULL)?"":macro_date_time[MACRO_DATETIME_TIME],buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"TIMET"))
					strncat(output_buffer,(macro_date_time[MACRO_DATETIME_TIMET]==NULL)?"":macro_date_time[MACRO_DATETIME_TIMET],buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"LASTCHECK"))
					strncat(output_buffer,(macro_date_time[MACRO_DATETIME_LASTCHECK]==NULL)?"":macro_date_time[MACRO_DATETIME_LASTCHECK],buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"LASTSTATECHANGE"))
					strncat(output_buffer,(macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]==NULL)?"":macro_date_time[MACRO_DATETIME_LASTSTATECHANGE],buffer_length-strlen(output_buffer)-1);

				/* $OUTPUT macro is cleaned before insertion */
				else if(!strcmp(temp_buffer,"OUTPUT"))
					strncat(output_buffer,(macro_output==NULL)?"":clean_macro_chars(macro_output,options),buffer_length-strlen(output_buffer)-1);

				/* $PERFDATA macro is cleaned before insertion */
				else if(!strcmp(temp_buffer,"PERFDATA"))
					strncat(output_buffer,(macro_perfdata==NULL)?"":clean_macro_chars(macro_perfdata,options),buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"CONTACTNAME"))
					strncat(output_buffer,(macro_contact_name==NULL)?"":macro_contact_name,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"CONTACTALIAS"))
					strncat(output_buffer,(macro_contact_alias==NULL)?"":macro_contact_alias,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"CONTACTEMAIL"))
					strncat(output_buffer,(macro_contact_email==NULL)?"":macro_contact_email,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"CONTACTPAGER"))
					strncat(output_buffer,(macro_contact_pager==NULL)?"":macro_contact_pager,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"ADMINEMAIL"))
					strncat(output_buffer,(macro_admin_email==NULL)?"":macro_admin_email,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"ADMINPAGER"))
					strncat(output_buffer,(macro_admin_pager==NULL)?"":macro_admin_pager,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"HOSTSTATE"))
					strncat(output_buffer,(macro_host_state==NULL)?"":macro_host_state,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"HOSTATTEMPT"))
					strncat(output_buffer,(macro_current_host_attempt==NULL)?"":macro_current_host_attempt,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"STATETYPE"))
					strncat(output_buffer,(macro_state_type==NULL)?"":macro_state_type,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"NOTIFICATIONTYPE"))
					strncat(output_buffer,(macro_notification_type==NULL)?"":macro_notification_type,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"NOTIFICATIONNUMBER"))
					strncat(output_buffer,(macro_notification_number==NULL)?"":macro_notification_number,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"EXECUTIONTIME"))
					strncat(output_buffer,(macro_execution_time==NULL)?"":macro_execution_time,buffer_length-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"LATENCY"))
					strncat(output_buffer,(macro_latency==NULL)?"":macro_latency,buffer_length-strlen(output_buffer)-1);


				else if(strstr(temp_buffer,"ARG")==temp_buffer){
					arg_index=atoi(temp_buffer+3);
					if(arg_index>=1 && arg_index<=MAX_COMMAND_ARGUMENTS)
						strncat(output_buffer,(macro_argv[arg_index-1]==NULL)?"":macro_argv[arg_index-1],buffer_length-strlen(output_buffer)-1);
				        }

				else if(strstr(temp_buffer,"USER")==temp_buffer){
					user_index=atoi(temp_buffer+4);
					if(user_index>=1 && user_index<=MAX_USER_MACROS)
						strncat(output_buffer,(macro_user[user_index-1]==NULL)?"":macro_user[user_index-1],buffer_length-strlen(output_buffer)-1);
				        }
			
				/* an escaped $ is done by specifying two $$ next to each other */
				else if(!strcmp(temp_buffer,"")){
					strncat(output_buffer,"$",buffer_length-strlen(output_buffer)-1);
					output_buffer[buffer_length-1]='\x0';
				        }
				
				/* a non-macro, just some user-defined string between two $s */
				else{
					strncat(output_buffer,"$",buffer_length-strlen(output_buffer)-1);
					output_buffer[buffer_length-1]='\x0';
					strncat(output_buffer,temp_buffer,buffer_length-strlen(output_buffer)-1);
					output_buffer[buffer_length-1]='\x0';
					strncat(output_buffer,"$",buffer_length-strlen(output_buffer)-1);
				        }

				output_buffer[buffer_length-1]='\x0';
				}

			in_macro=FALSE;
			}
		}

#ifdef DEBUG0
	printf("process_macros() end\n");
#endif

	return OK;
	}


/* grab macros that are specific to a particular service */
int grab_service_macros(service *svc){
	
#ifdef DEBUG0
	printf("grab_service_macros() start\n");
#endif

	/* get the service description */
	if(macro_service_description!=NULL)
		free(macro_service_description);
	macro_service_description=(char *)malloc(strlen(svc->description)+1);
	if(macro_service_description!=NULL)
		strcpy(macro_service_description,svc->description);

	/* get the plugin output */
	if(macro_output!=NULL)
		free(macro_output);
	if(svc->plugin_output==NULL)
		macro_output=NULL;
	else
		macro_output=(char *)malloc(strlen(svc->plugin_output)+1);
	if(macro_output!=NULL)
		strcpy(macro_output,svc->plugin_output);

	/* get the performance data */
	if(macro_perfdata!=NULL)
		free(macro_perfdata);
	if(svc->perf_data==NULL)
		macro_perfdata=NULL;
	else
		macro_perfdata=(char *)malloc(strlen(svc->perf_data)+1);
	if(macro_perfdata!=NULL)
		strcpy(macro_perfdata,svc->perf_data);

	/* grab the service state type macro (this is usually overridden later on) */
	if(macro_state_type!=NULL)
		free(macro_state_type);
	macro_state_type=(char *)malloc(MAX_STATETYPE_LENGTH);
	if(macro_state_type!=NULL)
		strcpy(macro_state_type,(svc->state_type==HARD_STATE)?"HARD":"SOFT");

	/* get the service state */
	if(macro_service_state!=NULL)
		free(macro_service_state);
	macro_service_state=(char *)malloc(MAX_STATE_LENGTH);
	if(macro_service_state!=NULL){
		if(svc->current_state==STATE_OK)
			strcpy(macro_service_state,"OK");
		else if(svc->current_state==STATE_WARNING)
			strcpy(macro_service_state,"WARNING");
		else if(svc->current_state==STATE_CRITICAL)
			strcpy(macro_service_state,"CRITICAL");
		else
			strcpy(macro_service_state,"UNKNOWN");
	        }

	/* get the current service check attempt macro */
	if(macro_current_service_attempt!=NULL)
		free(macro_current_service_attempt);
	macro_current_service_attempt=(char *)malloc(MAX_ATTEMPT_LENGTH);
	if(macro_current_service_attempt!=NULL){
		snprintf(macro_current_service_attempt,MAX_ATTEMPT_LENGTH-1,"%d",svc->current_attempt);
		macro_current_service_attempt[MAX_ATTEMPT_LENGTH-1]='\x0';
	        }

	/* get the execution time macro */
	if(macro_execution_time!=NULL)
		free(macro_execution_time);
	macro_execution_time=(char *)malloc(MAX_EXECUTIONTIME_LENGTH);
	if(macro_execution_time!=NULL){
		snprintf(macro_execution_time,MAX_EXECUTIONTIME_LENGTH-1,"%lu",svc->execution_time);
		macro_execution_time[MAX_EXECUTIONTIME_LENGTH-1]='\x0';
	        }

	/* get the latency macro */
	if(macro_latency!=NULL)
		free(macro_latency);
	macro_latency=(char *)malloc(MAX_LATENCY_LENGTH);
	if(macro_latency!=NULL){
		snprintf(macro_latency,MAX_LATENCY_LENGTH-1,"%lu",svc->latency);
		macro_latency[MAX_LATENCY_LENGTH-1]='\x0';
	        }

	/* get the date/time macros */
	grab_datetime_macros();

	/* get the last check time macro */
	if(macro_date_time[MACRO_DATETIME_LASTCHECK]!=NULL)
		free(macro_date_time[MACRO_DATETIME_LASTCHECK]);
	macro_date_time[MACRO_DATETIME_LASTCHECK]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_date_time[MACRO_DATETIME_LASTCHECK]!=NULL){
		snprintf(macro_date_time[MACRO_DATETIME_LASTCHECK],MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)svc->last_check);
		macro_date_time[MACRO_DATETIME_LASTCHECK][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last state change time macro */
	if(macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]!=NULL)
		free(macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]);
	macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]!=NULL){
		snprintf(macro_date_time[MACRO_DATETIME_LASTSTATECHANGE],MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)svc->last_state_change);
		macro_date_time[MACRO_DATETIME_LASTSTATECHANGE][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	strip(macro_service_description);
	strip(macro_output);
	strip(macro_service_state);
	strip(macro_current_service_attempt);

#ifdef DEBUG0
	printf("grab_service_macros() end\n");
#endif

	return OK;
        }


/* grab macros that are specific to a particular host */
int grab_host_macros(host *hst){

#ifdef DEBUG0
	printf("grab_host_macros() start\n");
#endif

	/* get the host name */
	if(macro_host_name!=NULL)
		free(macro_host_name);
	macro_host_name=(char *)malloc(strlen(hst->name)+1);
	if(macro_host_name!=NULL)
		strcpy(macro_host_name,hst->name);
	
	/* get the host alias */
	if(macro_host_alias!=NULL)
		free(macro_host_alias);
	macro_host_alias=(char *)malloc(strlen(hst->alias)+1);
	if(macro_host_alias!=NULL)
		strcpy(macro_host_alias,hst->alias);

	/* get the host address */
	if(macro_host_address!=NULL)
		free(macro_host_address);
	macro_host_address=(char *)malloc(strlen(hst->address)+1);
	if(macro_host_address!=NULL)
		strcpy(macro_host_address,hst->address);

	/* get the host state */
	if(macro_host_state!=NULL)
		free(macro_host_state);
	macro_host_state=(char *)malloc(MAX_STATE_LENGTH);
	if(macro_host_state!=NULL){
		if(hst->status==HOST_DOWN)
			strcpy(macro_host_state,"DOWN");
		else if(hst->status==HOST_UNREACHABLE)
			strcpy(macro_host_state,"UNREACHABLE");
		else
			strcpy(macro_host_state,"UP");
	        }

	/* get the plugin output */
	if(macro_output!=NULL)
		free(macro_output);
	if(hst->plugin_output==NULL)
		macro_output=NULL;
	else
		macro_output=(char *)malloc(strlen(hst->plugin_output)+1);
	if(macro_output!=NULL)
		strcpy(macro_output,hst->plugin_output);

	/* get the performance data */
	if(macro_perfdata!=NULL)
		free(macro_perfdata);
	if(hst->perf_data==NULL)
		macro_perfdata=NULL;
	else
		macro_perfdata=(char *)malloc(strlen(hst->perf_data)+1);
	if(macro_perfdata!=NULL)
		strcpy(macro_perfdata,hst->perf_data);

	/* get the host current attempt */
	if(macro_current_host_attempt!=NULL)
		free(macro_current_host_attempt);
	macro_current_host_attempt=(char *)malloc(MAX_ATTEMPT_LENGTH);
	if(macro_current_host_attempt!=NULL){
		snprintf(macro_current_host_attempt,MAX_ATTEMPT_LENGTH-1,"%d",hst->current_attempt);
		macro_current_host_attempt[MAX_ATTEMPT_LENGTH-1]='\x0';
	        }

	/* get the execution time macro */
	if(macro_execution_time!=NULL)
		free(macro_execution_time);
	macro_execution_time=(char *)malloc(MAX_EXECUTIONTIME_LENGTH);
	if(macro_execution_time!=NULL){
		snprintf(macro_execution_time,MAX_EXECUTIONTIME_LENGTH-1,"%lu",hst->execution_time);
		macro_execution_time[MAX_EXECUTIONTIME_LENGTH-1]='\x0';
	        }

	/* get the date/time macros */
	grab_datetime_macros();

	/* get the last check time macro */
	if(macro_date_time[MACRO_DATETIME_LASTCHECK]!=NULL)
		free(macro_date_time[MACRO_DATETIME_LASTCHECK]);
	macro_date_time[MACRO_DATETIME_LASTCHECK]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_date_time[MACRO_DATETIME_LASTCHECK]!=NULL){
		snprintf(macro_date_time[MACRO_DATETIME_LASTCHECK],MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)hst->last_check);
		macro_date_time[MACRO_DATETIME_LASTCHECK][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last state change time macro */
	if(macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]!=NULL)
		free(macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]);
	macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]!=NULL){
		snprintf(macro_date_time[MACRO_DATETIME_LASTSTATECHANGE],MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)hst->last_state_change);
		macro_date_time[MACRO_DATETIME_LASTSTATECHANGE][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	strip(macro_host_name);
	strip(macro_host_alias);
	strip(macro_host_address);
	strip(macro_host_state);
	strip(macro_output);
	strip(macro_current_host_attempt);

#ifdef DEBUG0
	printf("grab_host_macros() end\n");
#endif

	return OK;
        }


/* grab macros that are specific to a particular contact */
int grab_contact_macros(contact *cntct){

#ifdef DEBUG0
	printf("grab_contact_macros() start\n");
#endif

	/* get the name */
	if(macro_contact_name!=NULL)
		free(macro_contact_name);
	macro_contact_name=(char *)malloc(strlen(cntct->name)+1);
	if(macro_contact_name!=NULL)
		strcpy(macro_contact_name,cntct->name);

	/* get the alias */
	if(macro_contact_alias!=NULL)
		free(macro_contact_alias);
	macro_contact_alias=(char *)malloc(strlen(cntct->alias)+1);
	if(macro_contact_alias!=NULL)
		strcpy(macro_contact_alias,cntct->alias);

	/* get the email address */
	if(macro_contact_email!=NULL)
		free(macro_contact_email);
	if(cntct->email==NULL)
		macro_contact_email=NULL;
	else
		macro_contact_email=(char *)malloc(strlen(cntct->email)+1);
	if(macro_contact_email!=NULL)
		strcpy(macro_contact_email,cntct->email);

	/* get the pager number */
	if(macro_contact_pager!=NULL)
		free(macro_contact_pager);
	if(cntct->pager==NULL)
		macro_contact_pager=NULL;
	else
		macro_contact_pager=(char *)malloc(strlen(cntct->pager)+1);
	if(macro_contact_pager!=NULL)
		strcpy(macro_contact_pager,cntct->pager);

	/* get the date/time macros */
	grab_datetime_macros();

	strip(macro_contact_name);
	strip(macro_contact_alias);
	strip(macro_contact_email);
	strip(macro_contact_pager);

#ifdef DEBUG0
	printf("grab_contact_macros() end\n");
#endif

	return OK;
        }


/* updates date/time macros */
int grab_datetime_macros(void){
	time_t t;

#ifdef DEBUG0
	printf("grab_datetime_macros() start\n");
#endif

	/* get the current time */
	time(&t);

	/* get the current date/time (long format macro) */
	if(macro_date_time[MACRO_DATETIME_LONGDATE]!=NULL)
		free(macro_date_time[MACRO_DATETIME_LONGDATE]);
	macro_date_time[MACRO_DATETIME_LONGDATE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_date_time[MACRO_DATETIME_LONGDATE]!=NULL)
		get_datetime_string(&t,macro_date_time[MACRO_DATETIME_LONGDATE],MAX_DATETIME_LENGTH,LONG_DATE_TIME);

	/* get the current date/time (short format macro) */
	if(macro_date_time[MACRO_DATETIME_SHORTDATE]!=NULL)
		free(macro_date_time[MACRO_DATETIME_SHORTDATE]);
	macro_date_time[MACRO_DATETIME_SHORTDATE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_date_time[MACRO_DATETIME_SHORTDATE]!=NULL)
		get_datetime_string(&t,macro_date_time[MACRO_DATETIME_SHORTDATE],MAX_DATETIME_LENGTH,SHORT_DATE_TIME);

	/* get the short format date macro */
	if(macro_date_time[MACRO_DATETIME_DATE]!=NULL)
		free(macro_date_time[MACRO_DATETIME_DATE]);
	macro_date_time[MACRO_DATETIME_DATE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_date_time[MACRO_DATETIME_DATE]!=NULL)
		get_datetime_string(&t,macro_date_time[MACRO_DATETIME_DATE],MAX_DATETIME_LENGTH,SHORT_DATE);

	/* get the short format time macro */
	if(macro_date_time[MACRO_DATETIME_TIME]!=NULL)
		free(macro_date_time[MACRO_DATETIME_TIME]);
	macro_date_time[MACRO_DATETIME_TIME]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_date_time[MACRO_DATETIME_TIME]!=NULL)
		get_datetime_string(&t,macro_date_time[MACRO_DATETIME_TIME],MAX_DATETIME_LENGTH,SHORT_TIME);

	/* get the time_t format time macro */
	if(macro_date_time[MACRO_DATETIME_TIMET]!=NULL)
		free(macro_date_time[MACRO_DATETIME_TIMET]);
	macro_date_time[MACRO_DATETIME_TIMET]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_date_time[MACRO_DATETIME_TIMET]!=NULL){
		snprintf(macro_date_time[MACRO_DATETIME_TIMET],MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)t);
		macro_date_time[MACRO_DATETIME_TIMET][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	strip(macro_date_time[MACRO_DATETIME_LONGDATE]);
	strip(macro_date_time[MACRO_DATETIME_SHORTDATE]);
	strip(macro_date_time[MACRO_DATETIME_DATE]);
	strip(macro_date_time[MACRO_DATETIME_TIME]);
	strip(macro_date_time[MACRO_DATETIME_TIMET]);

#ifdef DEBUG0
	printf("grab_datetime_macros() end\n");
#endif

	return OK;
        }



/* clear all macros that are not "constant" (i.e. they change throughout the course of monitoring) */
int clear_volatile_macros(void){
	int x;

#ifdef DEBUG0
	printf("clear_volatile_macros() start\n");
#endif

	/* plugin output macros */
	if(macro_output!=NULL){
		free(macro_output);
		macro_output=NULL;
	        }
	if(macro_perfdata!=NULL){
		free(macro_perfdata);
		macro_perfdata=NULL;
	        }

	/* contact macros */
	if(macro_contact_name!=NULL){
		free(macro_contact_name);
		macro_contact_name=NULL;
	        }
	if(macro_contact_alias!=NULL){
		free(macro_contact_alias);
		macro_contact_alias=NULL;
	        }
	if(macro_contact_email!=NULL){
		free(macro_contact_email);
		macro_contact_email=NULL;
	        }
	if(macro_contact_pager!=NULL){
		free(macro_contact_pager);
		macro_contact_pager=NULL;
	        }

	/* host macros */
	if(macro_host_name!=NULL){
		free(macro_host_name);
		macro_host_name=NULL;
	        }
	if(macro_host_alias!=NULL){
		free(macro_host_alias);
		macro_host_alias=NULL;
	        }
	if(macro_host_address!=NULL){
		free(macro_host_address);
		macro_host_address=NULL;
	        }
	if(macro_host_state!=NULL){
		free(macro_host_state);
		macro_host_state=NULL;
	        }
	if(macro_current_host_attempt!=NULL){
		free(macro_current_host_attempt);
		macro_current_host_attempt=NULL;
	        }

	/* service macros */
	if(macro_service_description!=NULL){
		free(macro_service_description);
		macro_service_description=NULL;
	        }
	if(macro_service_state!=NULL){
		free(macro_service_state);
		macro_service_state=NULL;
	        }
	if(macro_current_service_attempt!=NULL){
		free(macro_current_service_attempt);
		macro_current_service_attempt=NULL;
                }

	/* miscellaneous macros */
	if(macro_state_type!=NULL){
		free(macro_state_type);
		macro_state_type=NULL;
	        }
	if(macro_date_time[MACRO_DATETIME_LONGDATE]!=NULL){
		free(macro_date_time[MACRO_DATETIME_LONGDATE]);
		macro_date_time[MACRO_DATETIME_LONGDATE]=NULL;
	        }
	if(macro_date_time[MACRO_DATETIME_SHORTDATE]!=NULL){
		free(macro_date_time[MACRO_DATETIME_SHORTDATE]);
		macro_date_time[MACRO_DATETIME_SHORTDATE]=NULL;
	        }
	if(macro_date_time[MACRO_DATETIME_DATE]!=NULL){
		free(macro_date_time[MACRO_DATETIME_DATE]);
		macro_date_time[MACRO_DATETIME_DATE]=NULL;
	        }
	if(macro_date_time[MACRO_DATETIME_TIME]!=NULL){
		free(macro_date_time[MACRO_DATETIME_TIME]);
		macro_date_time[MACRO_DATETIME_TIME]=NULL;
	        }
	if(macro_date_time[MACRO_DATETIME_TIMET]!=NULL){
		free(macro_date_time[MACRO_DATETIME_TIMET]);
		macro_date_time[MACRO_DATETIME_TIMET]=NULL;
	        }
	if(macro_date_time[MACRO_DATETIME_LASTCHECK]!=NULL){
		free(macro_date_time[MACRO_DATETIME_LASTCHECK]);
		macro_date_time[MACRO_DATETIME_LASTCHECK]=NULL;
	        }
	if(macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]!=NULL){
		free(macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]);
		macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]=NULL;
	        }
	if(macro_notification_type!=NULL){
		free(macro_notification_type);
		macro_notification_type=NULL;
	        }
	if(macro_notification_number!=NULL){
		free(macro_notification_number);
		macro_notification_number=NULL;
	        }
	if(macro_execution_time!=NULL){
		free(macro_execution_time);
		macro_execution_time=NULL;
	        }
	if(macro_latency!=NULL){
		free(macro_latency);
		macro_latency=NULL;
	        }

	/* command argument macros */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){
		if(macro_argv[x]!=NULL){
			free(macro_argv[x]);
			macro_argv[x]=NULL;
		        }
	        }

#ifdef DEBUG0
	printf("clear_volatile_macros() end\n");
#endif

	return OK;
        }


/* clear macros that are constant (i.e. they do NOT change during monitoring) */
int clear_nonvolatile_macros(void){
	int x=0;

#ifdef DEBUG0
	printf("clear_nonvolatile_macros() start\n");
#endif

	/* admin email and pager */
	if(macro_admin_email!=NULL){
		free(macro_admin_email);
		macro_admin_email=NULL;
	        }
	if(macro_admin_pager!=NULL){
		free(macro_admin_pager);
		macro_admin_pager=NULL;
	        }

	/* user macros */
	for(x=0;x<MAX_USER_MACROS;x++){
		if(macro_user[x]!=NULL){
			free(macro_user[x]);
			macro_user[x]=NULL;
		        }
	        }

#ifdef DEBUG0
	printf("clear_nonvolatile_macros() end\n");
#endif

	return OK;
        }




/******************************************************************/
/******************** SYSTEM COMMAND FUNCTIONS ********************/
/******************************************************************/


/* executes a system command - used for service checks and notifications */
int my_system(char *cmd,int timeout,int *early_timeout,char *output,int output_length){
        pid_t pid;
	int status;
	int result;
	char buffer[MAX_INPUT_BUFFER];
	int fd[2];
	FILE *fp=NULL;
	int bytes_read=0;
	time_t start_time,end_time;

#ifdef EMBEDDEDPERL
	char fname[1024];
	char tmpfname[32];
	char *args[5] = {"",DO_CLEAN, "", "", NULL };
	int isperl;
	int tmpfd;
#ifdef THREADEDPERL
	dTHX;
#endif
	dSP;
#endif


#ifdef DEBUG0
	printf("my_system() start\n");
#endif

	/* initialize return variables */
	if(output!=NULL)
		strcpy(output,"");
	*early_timeout=FALSE;

	/* if no command was passed, return with no error */
	if(cmd==NULL)
	        return STATE_OK;

#ifdef EMBEDDEDPERL

	strncpy(fname,cmd,strcspn(cmd," "));
	fname[strcspn(cmd," ")] = '\x0';

	/* have "filename" component of command. Check for PERL */
	fp=fopen(fname,"r");
	if(fp==NULL)
		strcpy(buffer,"");
	else{
		fgets(buffer,80,fp);
		fclose(fp);
	        }

	isperl=0;

	if(strstr(buffer,"/bin/perl")!=NULL){

		isperl = 1;
		args[0] = fname;
		args[2] = tmpfname;

		if(strchr(cmd,' ')==NULL)
			args[3]="";
		else
			args[3]=cmd+strlen(fname)+1;

		/* call our perl interpreter to compile and optionally cache the compiled script. */
		perl_call_argv("Embed::Persistent::eval_file", G_DISCARD | G_EVAL, args);
	        }
#endif 

	/* create a pipe */
	pipe(fd);

	/* make the pipe non-blocking */
	fcntl(fd[0],F_SETFL,O_NONBLOCK);
	fcntl(fd[1],F_SETFL,O_NONBLOCK);

	/* get the command start time */
	time(&start_time);

	/* fork */
	pid=fork();

	/* return an error if we couldn't fork */
	if(pid==-1){

		snprintf(buffer,sizeof(buffer)-1,"Warning: fork() in my_system() failed for command \"%s\"\n",cmd);
		buffer[sizeof(buffer)-1]='\x0';

		if(output!=NULL){
			strncpy(output,buffer,output_length-1);
			output[output_length-1]='\x0';
		        }

		write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);

		/* close both ends of the pipe */
		close(fd[0]);
		close(fd[1]);
		
	        return STATE_UNKNOWN;  
	        }

	/* execute the command in the child process */
        if (pid==0){

		/* become process group leader */
		setpgid(0,0);

#ifndef USE_MEMORY_PERFORMANCE_TWEAKS
		/* free allocated memory */
		free_memory();
#endif

		/* reset signal handling */
		reset_sighandler();

		/* close pipe for reading */
		close(fd[0]);

		/* trap commands that timeout */
		signal(SIGALRM,my_system_sighandler);
		alarm(timeout);


		/******** BEGIN EMBEDDED PERL CODE EXECUTION ********/

#ifdef EMBEDDEDPERL
		if(isperl){

			/* generate a temporary filename to which stdout can be redirected. */
			snprintf(tmpfname,sizeof(tmpfname)-1,"/tmp/embeddedXXXXXX");
			if((tmpfd=mkstemp(tmpfname))==-1)
				_exit(STATE_UNKNOWN);

			/* execute our previously compiled script  */
			ENTER;
			SAVETMPS;
			PUSHMARK(SP);
			XPUSHs(sv_2mortal(newSVpv(args[0],0)));
			XPUSHs(sv_2mortal(newSVpv(args[1],0)));
			XPUSHs(sv_2mortal(newSVpv(args[2],0)));
			XPUSHs(sv_2mortal(newSVpv(args[3],0)));
			PUTBACK;
			perl_call_pv("Embed::Persistent::run_package", G_EVAL);
			SPAGAIN;
			status = POPi;
			PUTBACK;
			FREETMPS;
			LEAVE;                                    

			/* check return status  */
			if(SvTRUE(ERRSV)){
#ifdef DEBUG0
				printf("embedded perl ran command %s with error\n",fname);
#endif
				status=-2;
			        }

			/* read back stdout from script */
			fp=fopen(tmpfname,"r");

			/* default return string in case nothing was returned */
			strcpy(buffer,"(No output!)");

			/* grab output from plugin (which was redirected to the temp file) */
			fgets(buffer,sizeof(buffer)-1,fp);
			buffer[sizeof(buffer)-1]='\x0';
			strip(buffer);

			/* close and delete temp file */
			fclose(fp);
			close(tmpfd);
			unlink(tmpfname);

			/* report the command status */
			if(status==-2)
				result=STATE_CRITICAL;
			else
				result=status;

			/* write the output back to the parent process */
			write(fd[1],buffer,strlen(buffer)+1);

			/* close pipe for writing */
			close(fd[1]);

			/* reset the alarm */
			alarm(0);

			_exit(result);
		        }

		/* Not a perl command. Use popen... */
#endif  

		/******** END EMBEDDED PERL CODE EXECUTION ********/


		/* run the command */
		fp=popen(cmd,"r");
		
		/* report an error if we couldn't run the command */
		if(fp==NULL){

			strncpy(buffer,"(Error: Could not execute command)\n",sizeof(buffer)-1);
			buffer[sizeof(buffer)-1]='\x0';

			/* write the error back to the parent process */
			write(fd[1],buffer,strlen(buffer)+1);

			result=STATE_CRITICAL;
		        }
		else{

			/* default return string in case nothing was returned */
			strcpy(buffer,"(No output!)");

			/* read in the first line of output from the command */
			fgets(buffer,sizeof(buffer)-1,fp);

			/* close the command and get termination status */
			status=pclose(fp);
			
			/* report an error if we couldn't close the command */
			if(status==-1)
				result=STATE_CRITICAL;
			else
				result=WEXITSTATUS(status);

			/* write the output back to the parent process */
			write(fd[1],buffer,strlen(buffer)+1);
		        }

		/* close pipe for writing */
		close(fd[1]);

		/* reset the alarm */
		alarm(0);
		
		_exit(result);
	        }

	/* parent waits for child to finish executing command */
	else{
		
		/* close pipe for writing */
		close(fd[1]);

		/* wait for child to exit */
		waitpid(pid,&status,0);

		/* get the end time for running the command */
		time(&end_time);

		/* get the exit code returned from the program */
		result=WEXITSTATUS(status);

		/* check for possibly missing scripts/binaries/etc */
		if(result==126 || result==127){
			snprintf(buffer,sizeof(buffer)-1,"Warning: Attempting to execute the command \"%s\" resulted in a return code of %d.  Make sure the script or binary you are trying to execute actually exists...\n",cmd,result);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);
		        }

		/* because of my idiotic idea of having UNKNOWN states be equivalent to -1, I must hack things a bit... */
		if(result==255 || result==-1)
			result=STATE_UNKNOWN;

		/* check bounds on the return value */
		if(result<-1 || result>3)
			result=STATE_UNKNOWN;

		/* try and read the results from the command output (retry if we encountered a signal) */
		if(output!=NULL){
			do{
				bytes_read=read(fd[0],output,output_length-1);
		                }while(bytes_read==-1 && errno==EINTR);
		        }

		if(bytes_read==-1 && output!=NULL)
			strcpy(output,"");

		/* if there was a critical return code and no output AND the command time exceeded the timeout thresholds, assume a timeout */
		if(result==STATE_CRITICAL && bytes_read==-1 && (end_time-start_time)>=timeout){

			/* set the early timeout flag */
			*early_timeout=TRUE;

			/* try to kill the command that timed out by sending termination signal to child process group */
			kill((pid_t)(-pid),SIGTERM);
			sleep(1);
			kill((pid_t)(-pid),SIGKILL);
		        }

		/* close the pipe for reading */
		close(fd[0]);
	        }

#ifdef DEBUG1
	printf("my_system() end\n");
#endif

	return result;
        }



/* given a "raw" command, return the "expanded" or "whole" command line */
void get_raw_command_line(char *cmd,char *raw_command,int buffer_length){
	char temp_buffer[MAX_INPUT_BUFFER];
	char *buffer;
	command *temp_command;
	int x;

#ifdef DEBUG0
	printf("get_raw_command_line() start\n");
#endif
#ifdef DEBUG1
	printf("\tInput: %s\n",cmd);
#endif

	/* initialize the command */
	strcpy(raw_command,"");

	/* if we were handed a NULL string, throw it right back */
	if(cmd==NULL){
		
#ifdef DEBUG1
		printf("\tRaw command is NULL!\n");
#endif

		return;
	        }

	/* clear the old command arguments */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){
		if(macro_argv[x]!=NULL)
			free(macro_argv[x]);
		macro_argv[x]=NULL;
	        }

	/* if the command is enclosed in quotes, this *is* the command we should run, don't do a lookup */
	if(cmd[0]=='\"'){
		strncpy(raw_command,cmd+1,buffer_length);
		raw_command[buffer_length-1]='\x0';
		strip(raw_command);

		/* strip the trailing quote if its there... */
		if(raw_command[strlen(raw_command)-1]=='\"')
			raw_command[strlen(raw_command)-1]='\x0';
	        }

	/* else lookup the command */
	else{

		strcpy(temp_buffer,cmd);

		/* get the command name */
		buffer=my_strtok(temp_buffer,"!");

		/* the buffer should never be NULL, but just in case... */
		if(buffer==NULL){
			strcpy(raw_command,"");
			return;
		        }
		else{
			strncpy(raw_command,buffer,buffer_length);
			raw_command[buffer_length-1]='\x0';
		        }

		/* get the arguments */
		for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){
			buffer=my_strtok(NULL,"!");
			if(buffer==NULL)
				break;
			strip(buffer);
			macro_argv[x]=(char *)malloc(strlen(buffer)+1);
			if(macro_argv[x]!=NULL)
				strcpy(macro_argv[x],buffer);
		        }

		/* find the command used to check this service */
		temp_command=find_command(raw_command,NULL);

		/* error if we couldn't find the command */
		if(temp_command==NULL)
			return;

		strncpy(raw_command,temp_command->command_line,buffer_length);
		raw_command[buffer_length-1]='\x0';
		strip(raw_command);
	        }

#ifdef DEBUG1
	printf("\tOutput: %s\n",raw_command);
#endif
#ifdef DEBUG0
	printf("get_raw_command_line() end\n");
#endif

	return;
        }





/******************************************************************/
/************************* TIME FUNCTIONS *************************/
/******************************************************************/


/* see if the specified time falls into a valid time range in the given time period */
int check_time_against_period(time_t check_time,char *period_name){
	timeperiod *temp_period;
	timerange *temp_range;
	unsigned long midnight_today;
	struct tm *t;

#ifdef DEBUG0
	printf("check_time_against_period() start\n");
#endif
	
	/* if no period name was specified, assume the time is good */
	if(period_name==NULL)
		return OK;

	/* if no period name was specified, assume the time is good */
	if(!strcmp(period_name,""))
		return OK;

	/* if period could not be found, assume the time is good */
	temp_period=find_timeperiod(period_name,NULL);
	if(temp_period==NULL)
		return OK;

	t=localtime((time_t *)&check_time);

	/* calculate the start of the day (midnight, 00:00 hours) */
	t->tm_sec=0;
	t->tm_min=0;
	t->tm_hour=0;
	midnight_today=(unsigned long)mktime(t);

	for(temp_range=temp_period->days[t->tm_wday];temp_range!=NULL;temp_range=temp_range->next){

		/* if the user-specified time falls in this range, return with a positive result */
		if(check_time>=(time_t)(midnight_today+temp_range->range_start) && check_time<=(time_t)(midnight_today+temp_range->range_end))
			return OK;
	        }

#ifdef DEBUG0
	printf("check_time_against_period() end\n");
#endif

	return ERROR;
        }



/* given a preferred time, get the next valid time within a time period */ 
void get_next_valid_time(time_t preferred_time,time_t *valid_time, char *period_name){
	timeperiod *temp_period;
	timerange *temp_timerange;
	unsigned long midnight_today;
	struct tm *t;
	int today;
	int weekday;
	unsigned long earliest_next_valid_time=0L;
	time_t this_time_range_start;
	int has_looped=FALSE;
	int days_into_the_future;

#ifdef DEBUG0
	printf("get_next_valid_time() start\n");
#endif

#ifdef DEBUG1
	printf("\tPreferred Time: %lu --> %s",(unsigned long)preferred_time,ctime(&preferred_time));
#endif

	/* if the preferred time is valid today, go with it */
	if(check_time_against_period(preferred_time,period_name)==OK)
		*valid_time=preferred_time;

	/* else find the next available time */
	else{

		/* find the time period - if we can't find it, go with the preferred time */
		temp_period=find_timeperiod(period_name,NULL);
		if(temp_period==NULL){
			*valid_time=preferred_time;
			return;
		        }

 
		t=localtime((time_t *)&preferred_time);

		/* calculate the start of the day (midnight, 00:00 hours) */
		t->tm_sec=0;
		t->tm_min=0;
		t->tm_hour=0;
		midnight_today=(unsigned long)mktime(t);

		today=t->tm_wday;

		has_looped=FALSE;

		/* check a one week rotation of time */
		for(weekday=today,days_into_the_future=0;;weekday++,days_into_the_future++){

			if(weekday>=7){
				weekday-=7;
				has_looped=TRUE;
			        }

			/* check all time ranges for this day of the week */
			for(temp_timerange=temp_period->days[weekday];temp_timerange!=NULL;temp_timerange=temp_timerange->next){

				/* calculate the time for the start of this time range */
				this_time_range_start=(time_t)(midnight_today+(days_into_the_future*3600*24)+temp_timerange->range_start);

				/* we're looking for the earliest possible start time for this day */
				if((earliest_next_valid_time==(time_t)0 || (this_time_range_start < earliest_next_valid_time)) && (this_time_range_start >= (time_t)preferred_time))
					earliest_next_valid_time=this_time_range_start;
			        }

			/* break out of the loop if we have checked an entire week already */
			if(has_looped==TRUE && weekday >= today)
				break;
		        }

		/* if we couldn't find a time period (there must be none defined) */
		if(earliest_next_valid_time==(time_t)0)
			*valid_time=(time_t)preferred_time;

		/* else use the calculated time */
		else
			*valid_time=earliest_next_valid_time;
	        }

#ifdef DEBUG1
	printf("\tNext Valid Time: %lu --> %s",(unsigned long)*valid_time,ctime(valid_time));
#endif

#ifdef DEBUG0
	printf("get_next_valid_time() end\n");
#endif

	return;
        }

	
	 

/* given a date/time in time_t format, produce a corresponding date/time string, including timezone */
void get_datetime_string(time_t *raw_time,char *buffer,int buffer_length, int type){
	time_t t;
	struct tm *tm_ptr;
	int hour;
	int minute;
	int second;
	int month;
	int day;
	int year;
	char *weekdays[7]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
	char *months[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sept","Oct","Nov","Dec"};
	char *tzone="";

	if(raw_time==NULL)
		time(&t);
	else 
		t=*raw_time;

	tm_ptr=localtime(&t);

	hour=tm_ptr->tm_hour;
	minute=tm_ptr->tm_min;
	second=tm_ptr->tm_sec;
	month=tm_ptr->tm_mon+1;
	day=tm_ptr->tm_mday;
	year=tm_ptr->tm_year+1900;

#ifdef HAVE_TM_ZONE
	tzone=(char *)tm_ptr->tm_zone;
#else
	tzone=(tm_ptr->tm_isdst)?tzname[1]:tzname[0];
#endif

	/* ctime() style date/time */
	if(type==LONG_DATE_TIME)
		snprintf(buffer,buffer_length,"%s %s %d %02d:%02d:%02d %s %d",weekdays[tm_ptr->tm_wday],months[tm_ptr->tm_mon],day,hour,minute,second,tzone,year);

	/* short date/time */
	else if(type==SHORT_DATE_TIME){
		if(date_format==DATE_FORMAT_EURO)
			snprintf(buffer,buffer_length,"%02d-%02d-%04d %02d:%02d:%02d",day,month,year,hour,minute,second);
		else if(date_format==DATE_FORMAT_ISO8601 || date_format==DATE_FORMAT_STRICT_ISO8601)
			snprintf(buffer,buffer_length,"%04d-%02d-%02d%c%02d:%02d:%02d",year,month,day,(date_format==DATE_FORMAT_STRICT_ISO8601)?'T':' ',hour,minute,second);
		else
			snprintf(buffer,buffer_length,"%02d-%02d-%04d %02d:%02d:%02d",month,day,year,hour,minute,second);
	        }

	/* short date */
	else if(type==SHORT_DATE){
		if(date_format==DATE_FORMAT_EURO)
			snprintf(buffer,buffer_length,"%02d-%02d-%04d",day,month,year);
		else if(date_format==DATE_FORMAT_ISO8601 || date_format==DATE_FORMAT_STRICT_ISO8601)
			snprintf(buffer,buffer_length,"%04d-%02d-%02d",year,month,day);
		else
			snprintf(buffer,buffer_length,"%02d-%02d-%04d",month,day,year);
	        }

	/* short time */
	else
		snprintf(buffer,buffer_length,"%02d:%02d:%02d",hour,minute,second);

	buffer[buffer_length-1]='\x0';

	return;
        }



/* get the next time to schedule a log rotation */
void get_next_log_rotation_time(time_t *run_time){
	time_t current_time;
	struct tm *t;
	int is_dst_now=FALSE;

#ifdef DEBUG0
	printf("get_next_log_rotation_time() start\n");
#endif
	
	time(&current_time);
	t=localtime(&current_time);
	t->tm_min=0;
	t->tm_sec=0;
	is_dst_now=(t->tm_isdst>0)?TRUE:FALSE;

	switch(log_rotation_method){
	case LOG_ROTATION_HOURLY:
		t->tm_hour++;
		*run_time=mktime(t);
		break;
	case LOG_ROTATION_DAILY:
		t->tm_mday++;
		t->tm_hour=0;
		*run_time=mktime(t);
		break;
	case LOG_ROTATION_WEEKLY:
		t->tm_mday+=(7-t->tm_wday);
		t->tm_hour=0;
		*run_time=mktime(t);
		break;
	case LOG_ROTATION_MONTHLY:
		t->tm_mon++;
		t->tm_mday=1;
		t->tm_hour=0;
		*run_time=mktime(t);
		break;
	default:
		break;
	        }

	if(is_dst_now==TRUE && t->tm_isdst==0)
		*run_time+=3600;
	else if(is_dst_now==FALSE && t->tm_isdst>0)
		*run_time-=3600;


#ifdef DEBUG1
	printf("\tNext Log Rotation Time: %s",ctime(run_time));
#endif

#ifdef DEBUG0
	printf("get_next_log_rotation_time() end\n");
#endif

	return;
        }



/******************************************************************/
/******************** SIGNAL HANDLER FUNCTIONS ********************/
/******************************************************************/


/* trap signals so we can exit gracefully */
void setup_sighandler(void){

#ifdef DEBUG0
	printf("setup_sighandler() start\n");
#endif

	/* reset the shutdown flag */
	sigshutdown=FALSE;

	/* remove buffering from stderr, stdin, and stdout */
	setbuf(stdin,(char *)NULL);
	setbuf(stdout,(char *)NULL);
	setbuf(stderr,(char *)NULL);

	/* initialize signal handling */
	signal(SIGQUIT,sighandler);
	signal(SIGTERM,sighandler);
	signal(SIGHUP,sighandler);
	signal(SIGSEGV,sighandler);

#ifdef DEBUG0
	printf("setup_sighandler() end\n");
#endif

	return;
        }


/* reset signal handling... */
void reset_sighandler(void){

#ifdef DEBUG0
	printf("reset_sighandler() start\n");
#endif

	/* set signal handling to default actions */
	signal(SIGQUIT,SIG_DFL);
	signal(SIGTERM,SIG_DFL);
	signal(SIGHUP,SIG_DFL);
	signal(SIGSEGV,SIG_DFL);

#ifdef DEBUG0
	printf("reset_sighandler() end\n");
#endif

	return;
        }


/* handle signals */
void sighandler(int sig){
	static char *sigs[]={"EXIT","HUP","INT","QUIT","ILL","TRAP","ABRT","BUS","FPE","KILL","USR1","SEGV","USR2","PIPE","ALRM","TERM","STKFLT","CHLD","CONT","STOP","TSTP","TTIN","TTOU","URG","XCPU","XFSZ","VTALRM","PROF","WINCH","IO","PWR","UNUSED","ZERR","DEBUG",(char *)NULL};
	int i;
	char temp_buffer[MAX_INPUT_BUFFER];


	/* if shutdown is already true, we're in a signal trap loop! */
	if(sigshutdown==TRUE)
		exit(ERROR);

	if(sig<0)
		sig=-sig;

	for(i=0;sigs[i]!=(char *)NULL;i++);

	sig%=i;

	/* we received a SIGHUP, so restart... */
	if(sig==SIGHUP){

		sigrestart=TRUE;

		sprintf(temp_buffer,"Caught SIGHUP, restarting...\n");
		write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO,FALSE);
#ifdef DEBUG2
		printf("%s\n",temp_buffer);
#endif
	        }

	/* else begin shutting down... */
	else if(sig<16){

		sigshutdown=TRUE;

		sprintf(temp_buffer,"Caught SIG%s, shutting down...\n",sigs[sig]);
		write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO,FALSE);

#ifdef DEBUG2
		printf("%s\n",temp_buffer);
#endif

		/* remove the lock file if we're in daemon mode */
		if(daemon_mode==TRUE)
			unlink(lock_file);

		/* close and delete the external command file FIFO */
		close_command_file();
	        }

	return;
        }


/* handle timeouts when executing service checks */
void service_check_sighandler(int sig){

	/* write plugin check results to message queue */
	strncpy(svc_msg.output,"(Service Check Timed Out)",sizeof(svc_msg.output)-1);
	svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
	svc_msg.return_code=STATE_CRITICAL;
	svc_msg.exited_ok=TRUE;
	svc_msg.check_type=SERVICE_CHECK_ACTIVE;
	svc_msg.finish_time=time(NULL);
	write_svc_message(&svc_msg);

	/* close write end of IPC pipe */
	close(ipc_pipe[1]);

	/* try to kill the command that timed out by sending termination signal to our process group */
	/* we also kill ourselves while doing this... */
	kill((pid_t)0,SIGKILL);
	
	/* force the child process (service check) to exit... */
	exit(STATE_CRITICAL);
        }


/* handle timeouts when executing commands via my_system() */
void my_system_sighandler(int sig){

	/* force the child process to exit... */
	exit(STATE_CRITICAL);
        }




/******************************************************************/
/************************ DAEMON FUNCTIONS ************************/
/******************************************************************/

int daemon_init(void){
	pid_t pid=-1;
	int pidno;
	int lockfile;
	int val=0;
	char buf[256];
	struct flock lock;
	char temp_buffer[MAX_INPUT_BUFFER];

#ifdef RLIMIT_CORE
	struct rlimit limit;
#endif

	chdir("/");		/* change working directory */

	umask(S_IWGRP|S_IWOTH);

	lockfile=open(lock_file,O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);

	if(lockfile<0){
		strcpy(temp_buffer,"");
		if(errno==EISDIR)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"%s is a directory\n",lock_file);
		else if(errno==EACCES)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"You do not have permission to write to %s\n",lock_file);
		else if(errno==ENAMETOOLONG)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"The filename is too long: %s\n",lock_file);
		else if(errno==ENOENT)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"%s does not exist (ENOENT)\n",lock_file);
		else if(errno==ENOTDIR)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"%s does not exist (ENOTDIR)\n",lock_file);
		else if(errno==ENXIO)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Cannot write to special file\n");
		else if(errno==ENODEV)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Cannot write to device\n");
		else if(errno==EROFS)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"%s is on a read-only file system\n",lock_file);
		else if(errno==ETXTBSY)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"%s is a currently running program\n",lock_file);
		else if(errno==EFAULT)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"%s is outside address space\n",lock_file);
		else if(errno==ELOOP)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Too many symbolic links\n");
		else if(errno==ENOSPC)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"No space on device\n");
		else if(errno==ENOMEM)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Insufficient kernel memory\n");
		else if(errno==EMFILE)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Too many files open in process\n");
		else if(errno==ENFILE)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Too many files open on system\n");

		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);

		snprintf(temp_buffer,sizeof(temp_buffer),"Bailing out due to errors encountered while attempting to daemonize... (PID=%d)",(int)getpid());
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR,TRUE);

		cleanup();
		exit(ERROR);
	        }

	/* see if we can read the contents of the lockfile */
	if((val=read(lockfile,buf,(size_t)10))<0){
		write_to_logs_and_console("Lockfile exists but cannot be read",NSLOG_RUNTIME_ERROR,TRUE);
		cleanup();
		exit(ERROR);
	        }

	/* we read something - check the PID */
	if(val>0){
		if((val=sscanf(buf,"%d",&pidno))<1){
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Lockfile '%s' does not contain a valid PID (%s)",lock_file,buf);
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
			cleanup();
			exit(ERROR);
		        }
	        }

	/* check for SIGHUP */
	if(val==1 && (pid=(pid_t)pidno)==getpid()){
		close(lockfile);
		return OK;
	        }

	/* exit on errors... */
	if((pid=fork())<0)
		return(ERROR);

	/* parent process goes away.. */
	else if((int)pid != 0)
		exit(OK);

	/* child continues... */

	/* child becomes session leader... */
	setsid();

	/* place a file lock on the lock file */
	lock.l_type=F_WRLCK;
	lock.l_start=0;
	lock.l_whence=SEEK_SET;
	lock.l_len=0;
	if(fcntl(lockfile,F_SETLK,&lock)<0){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Lockfile '%s' is held by PID %d.  Bailing out...",lock_file,(int)pid);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
		cleanup();
		exit(ERROR);
	        }

	/* prevent daemon from dumping a core file... */
#ifdef RLIMIT_CORE
	getrlimit(RLIMIT_CORE,&limit);
	limit.rlim_cur=0;
	setrlimit(RLIMIT_CORE,&limit);
#endif

	/* write PID to lockfile... */
	lseek(lockfile,0,SEEK_SET);
	ftruncate(lockfile,0);
	sprintf(buf,"%d\n",(int)getpid());
	write(lockfile,buf,strlen(buf));

	/* make sure lock file stays open while program is executing... */
	val=fcntl(lockfile,F_GETFD,0);
	val|=FD_CLOEXEC;
	fcntl(lockfile,F_SETFD,val);

        /* close existing stdin, stdout, stderr */
	close(0);
	close(1);
	close(2);

	/* THIS HAS TO BE DONE TO AVOID PROBLEMS WITH STDERR BEING REDIRECTED TO SERVICE MESSAGE PIPE! */
	/* re-open stdin, stdout, stderr with known values */
	open("/dev/null",O_RDONLY);
	open("/dev/null",O_WRONLY);
	open("/dev/null",O_WRONLY);

	return OK;
	}




/******************************************************************/
/*********************** SECURITY FUNCTIONS ***********************/
/******************************************************************/

/* drops privileges */
int drop_privileges(char *user, char *group){
	char temp_buffer[MAX_INPUT_BUFFER];
	uid_t uid=-1;
	gid_t gid=-1;
	struct group *grp;
	struct passwd *pw;

#ifdef DEBUG0
	printf("drop_privileges() start\n");
#endif

#ifdef DEBUG1
	printf("Original UID/GID: %d/%d\n",(int)getuid(),(int)getgid());
#endif

	/* set effective group ID */
	if(group!=NULL){
		
		/* see if this is a group name */
		if(strspn(group,"0123456789")<strlen(group)){
			grp=(struct group *)getgrnam(group);
			if(grp!=NULL)
				gid=(gid_t)(grp->gr_gid);
			else{
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Warning: Could not get group entry for '%s'",group);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		                }
		        }

		/* else we were passed the GID */
		else
			gid=(gid_t)atoi(group);

		/* set effective group ID if other than current EGID */
		if(gid!=getegid()){

			if(setgid(gid)==-1){
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Warning: Could not set effective GID=%d",(int)gid);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
			        }
		        }
	        }


	/* set effective user ID */
	if(user!=NULL){
		
		/* see if this is a user name */
		if(strspn(user,"0123456789")<strlen(user)){
			pw=(struct passwd *)getpwnam(user);
			if(pw!=NULL)
				uid=(uid_t)(pw->pw_uid);
			else{
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Warning: Could not get passwd entry for '%s'",user);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
			        }
		        }

		/* else we were passed the UID */
		else
			uid=(uid_t)atoi(user);
			
#ifdef HAVE_INITGROUPS

		if(uid!=geteuid()){

			/* initialize supplementary groups */
			if(initgroups(user,gid)==-1){
				if(errno==EPERM){
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Warning: Unable to change supplementary groups using initgroups() -- I hope you know what you're doing");
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		                        }
				else{
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Warning: Possibly root user failed dropping privileges with initgroups()");
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
					return ERROR;
			                }
	                        }
		        }
#endif
		if(setuid(uid)==-1){
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Warning: Could not set effective UID=%d",(int)uid);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		        }
	        }

#ifdef DEBUG1
	printf("New UID/GID: %d/%d\n",(int)getuid(),(int)getgid());
#endif

#ifdef DEBUG0
	printf("drop_privileges() end\n");
#endif

	return OK;
        }




/******************************************************************/
/************************* IPC FUNCTIONS **************************/
/******************************************************************/



/* reads a service message from the message pipe */
int read_svc_message(service_message *message){
	int read_result;
	int bytes_to_read;
	int write_offset;

#ifdef DEBUG0
	printf("read_svc_message() start\n");
#endif

	/* clear the message buffer */
	bzero(message,sizeof(service_message));

	/* initialize the number of bytes to read */
	bytes_to_read=sizeof(service_message);

        /* read a single message from the pipe, taking into consideration that we may have had an interrupt */
	while(1){

		write_offset=sizeof(service_message)-bytes_to_read;

		/* try and read a (full or partial) message */
		read_result=read(ipc_pipe[0],message+write_offset,bytes_to_read);

		/* we had a failure in reading from the pipe... */
		if(read_result==-1){

			/* if the problem wasn't due to an interrupt, return with an error */
			if(errno!=EINTR)
				break;

			/* otherwise we'll try reading from the pipe again... */
		        }

		/* are we at the end of pipe? this should not happen... */
		else if(read_result==0){

			read_result=-1;
			break;
		        }

		/* did we read fewer bytes than we were supposed to?  if so, try this read and try again (so we get the rest of the message)... */
		else if(read_result<bytes_to_read){
			bytes_to_read-=read_result;
			continue;
		        }

		else
			break;
	        }

#ifdef DEBUG0
	printf("read_svc_message() end\n");
#endif

	return read_result;
	}



/* writes a service message to the message pipe */
int write_svc_message(service_message *message){
	int write_result;

#ifdef DEBUG0
	printf("write_svc_message() start\n");
#endif

	while(1){

		write_result=write(ipc_pipe[1],message,sizeof(service_message));

		if(write_result==-1){

			if(errno!=EINTR && errno!=EAGAIN)
				break;
		         }
		else 
			break;
	        }


#ifdef DEBUG0
	printf("write_svc_message() end\n");
#endif

	return write_result;
        }


/* creates external command file as a named pipe (FIFO) and opens it for reading (non-blocked mode) */
int open_command_file(void){
	char buffer[MAX_INPUT_BUFFER];
 	int result;

#ifdef DEBUG0
	printf("open_command_file() start\n");
#endif

	/* if we're not checking external commands, don't do anything */
	if(check_external_commands==FALSE)
		return OK;

	/* the command file was already created */
	if(command_file_created==TRUE)
		return OK;

	/* reset umask (group needs write permissions) */
	umask(S_IWOTH);

	/* create the external command file as a named pipe (FIFO) */
	if((result=mkfifo(command_file,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))!=0){

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not create external command file '%s' as named pipe: (%d) -> %s.  If this file already exists and you are sure that another copy of Nagios is not running, you should delete this file.\n",command_file,errno,strerror(errno));
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* open the command file for reading (non-blocked) - O_TRUNC flag cannot be used due to errors on some systems */
	if((command_file_fd=open(command_file,O_RDONLY | O_NONBLOCK))<0){

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not open external command file for reading via open(): (%d) -> %s\n",errno,strerror(errno));
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* re-open the FIFO for use with fgets() */
	if((command_file_fp=fdopen(command_file_fd,"r"))==NULL){

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not open external command file for reading via fdopen(): (%d) -> %s\n",errno,strerror(errno));
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* set a flag to remember we already created the file */
	command_file_created=TRUE;

#ifdef DEBUG0
	printf("open_command_file() end\n");
#endif

	return OK;
        }


/* closes the external command file FIFO and deletes it */
int close_command_file(void){

#ifdef DEBUG0
	printf("close_command_file() start\n");
#endif

	/* if we're not checking external commands, don't do anything */
	if(check_external_commands==FALSE)
		return OK;

	/* close the command file */
	fclose(command_file_fp);
	close(command_file_fd);
	
	/* delete the named pipe */
	if(unlink(command_file)!=0)
		return ERROR;

#ifdef DEBUG0
	printf("close_command_file() end\n");
#endif

	return OK;
        }




/******************************************************************/
/************************ STRING FUNCTIONS ************************/
/******************************************************************/

/* strip newline, carriage return, and tab characters from end of a string */
void strip(char *buffer){
	register int x;

	if(buffer==NULL)
		return;

	x=(int)strlen(buffer)-1;

	for(;x>=0;x--){
		if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
			buffer[x]='\x0';
		else
			break;
	        }

	return;
	}



/* determines whether or not an object name (host, service, etc) contains illegal characters */
int contains_illegal_object_chars(char *name){
	register int x;
	register int y;
	register int ch;

	if(name==NULL)
		return FALSE;

	x=(int)strlen(name)-1;

	for(;x>=0;x--){

		ch=(int)name[x];

		/* illegal ASCII characters */
		if(ch<32 || ch==127)
			return TRUE;

		/* illegal extended ASCII characters */
		if(ch>=166)
			return TRUE;

		/* illegal user-specified characters */
		if(illegal_object_chars!=NULL)
			for(y=0;illegal_object_chars[y];y++)
				if(name[x]==illegal_object_chars[y])
					return TRUE;
	        }

	return FALSE;
        }



/* cleans illegal characters in macros before output */
char *clean_macro_chars(char *macro,int options){
	register int x,y,z;
	register int ch;
	register int len;
	register int illegal_char;

	if(macro==NULL)
		return "";

	len=(int)strlen(macro);

	/* strip illegal characters out of macro */
	if(options & STRIP_ILLEGAL_MACRO_CHARS){

		for(y=0,x=0;x<len;x++){
			
			ch=(int)macro[x];

			/* illegal ASCII characters */
			if(ch<32 || ch==127)
				continue;

			/* illegal extended ASCII characters */
			if(ch>=166)
				continue;

			/* illegal user-specified characters */
			illegal_char=FALSE;
			if(illegal_output_chars!=NULL){
				for(z=0;illegal_output_chars[z]!='\x0';z++){
					if(ch==(int)illegal_output_chars[z]){
						illegal_char=TRUE;
						break;
					        }
				        }
			        }
				
		        if(illegal_char==FALSE)
				macro[y++]=macro[x];
		        }

		macro[y++]='\x0';
	        }

#ifdef ON_HOLD_FOR_NOW
	/* escape nasty character in macro */
	if(options & ESCAPE_MACRO_CHARS){
	        }
#endif

	return macro;
        }



/* fix the problem with strtok() skipping empty options between tokens */	
char *my_strtok(char *buffer,char *tokens){
	char *token_position;
	char *sequence_head;

	if(buffer!=NULL){
		if(original_my_strtok_buffer!=NULL)
			free(original_my_strtok_buffer);
		my_strtok_buffer=(char *)malloc(strlen(buffer)+1);
		if(my_strtok_buffer==NULL)
			return NULL;
		original_my_strtok_buffer=my_strtok_buffer;
		strcpy(my_strtok_buffer,buffer);
	        }
	
	sequence_head=my_strtok_buffer;

	if(sequence_head[0]=='\x0')
		return NULL;
	
	token_position=index(my_strtok_buffer,tokens[0]);

	if(token_position==NULL){
		my_strtok_buffer=index(my_strtok_buffer,'\x0');
		return sequence_head;
	        }

	token_position[0]='\x0';
	my_strtok_buffer=token_position+1;

	return sequence_head;
        }



/* fixes compiler problems under Solaris, since strsep() isn't included */
/* this code is taken from the glibc source */
char *my_strsep (char **stringp, const char *delim){
	char *begin, *end;

	begin = *stringp;
	if (begin == NULL)
		return NULL;

	/* A frequent case is when the delimiter string contains only one
	   character.  Here we don't need to call the expensive `strpbrk'
	   function and instead work using `strchr'.  */
	if(delim[0]=='\0' || delim[1]=='\0'){
		char ch = delim[0];

		if(ch=='\0')
			end=NULL;
		else{
			if(*begin==ch)
				end=begin;
			else
				end=strchr(begin+1,ch);
			}
		}

	else
		/* Find the end of the token.  */
		end = strpbrk (begin, delim);

	if(end){

		/* Terminate the token and set *STRINGP past NUL character.  */
		*end++='\0';
		*stringp=end;
		}
	else
		/* No more delimiters; this is the last token.  */
		*stringp=NULL;

	return begin;
	}




/******************************************************************/
/************************* FILE FUNCTIONS *************************/
/******************************************************************/

/* renames a file - works across filesystems (Mike Wiacek) */
int my_rename(char *source, char *dest){
	char buffer[MAX_INPUT_BUFFER]={0};
	int rename_result;
	int source_fd;
	int dest_fd;
	int bytes_read;


	/* make sure we have something */
	if(source==NULL || dest==NULL)
		return -1;

	/* first see if we can rename file with standard function */
	rename_result=rename(source,dest);

	/* an error occurred because the source and dest files are on different filesystems */
	if(rename_result==-1 && errno==EXDEV){

#ifdef DEBUG2
		printf("\tMoving file across file systems.\n");
#endif

		/* open destination file for writing */
		if((dest_fd=open(dest,O_WRONLY|O_TRUNC|O_CREAT|O_APPEND,0644))>0){

			/* open source file for reading */
			if((source_fd=open(source,O_RDONLY,0644))>0){

				while((bytes_read=read(source_fd,buffer,sizeof(buffer)))>0)
					write(dest_fd,buffer,bytes_read);

				close(source_fd);
				close(dest_fd);
				
				/* delete the original file */
				unlink(source);

				/* reset result since we sucessfully copied file */
				rename_result=0;
			        }

			else{
				close(dest_fd);
				return rename_result;
			        }
		        }

		else
			return rename_result;
	        }

	return rename_result;
        }



/******************************************************************/
/*********************** CLEANUP FUNCTIONS ************************/
/******************************************************************/

/* do some cleanup before we exit */
void cleanup(void){

#ifdef DEBUG0
	printf("cleanup() start\n");
#endif

	/* free all allocated memory */
	free_memory();

	/* reset global variables to default values */
	reset_variables();

	/* clear all macros */
	clear_volatile_macros();
	clear_nonvolatile_macros();

#ifdef DEBUG0
	printf("cleanup() end\n");
#endif

	return;
	}


/* free the memory allocated to the linked lists */
void free_memory(void){
	timed_event *this_event=NULL;
	timed_event *next_event=NULL;

#ifdef DEBUG0
	printf("free_memory() start\n");
#endif

	/* free all allocated memory for the object definitions */
	free_object_data();

	/* free memory for the high priority event list */
	this_event=event_list_high;
	while(this_event!=NULL){
		next_event=this_event->next;
		free(this_event);
		this_event=next_event;
	        }

	/* reset the event pointer */
	event_list_high=NULL;

	/* free memory for the low priority event list */
	this_event=event_list_low;
	while(this_event!=NULL){
		next_event=this_event->next;
		free(this_event);
		this_event=next_event;
	        }

	/* reset the event pointer */
	event_list_low=NULL;

#ifdef DEBUG1
	printf("\tevent lists freed\n");
#endif

	/* free memory used by my_strtok() function */
	if(original_my_strtok_buffer!=NULL)
		free(original_my_strtok_buffer);

	/* reset the my_strtok() buffers */
	original_my_strtok_buffer=NULL;
	my_strtok_buffer=NULL;

#ifdef DEBUG1
	printf("\tmy_strtok() buffers freed\n");
#endif

	/* free memory for global event handlers */
	if(global_host_event_handler!=NULL){
		free(global_host_event_handler);
		global_host_event_handler=NULL;
	        }
	if(global_service_event_handler!=NULL){
		free(global_service_event_handler);
		global_service_event_handler=NULL;
	        }

#ifdef DEBUG1
	printf("\tglobal event handlers freed\n");
#endif

	/* free any notification list that may have been overlooked */
	free_notification_list();

#ifdef DEBUG1
	printf("\tnotification_list freed\n");
#endif

	/* free obsessive compulsive service command */
	if(ocsp_command!=NULL){
		free(ocsp_command);
		ocsp_command=NULL;
	        }

	/* free illegal char strings */
	if(illegal_object_chars!=NULL){
		free(illegal_object_chars);
		illegal_object_chars=NULL;
	        }
	if(illegal_output_chars!=NULL){
		free(illegal_output_chars);
		illegal_output_chars=NULL;
	        }

	/* free nagios user and group */
	if(nagios_user!=NULL){
		free(nagios_user);
		nagios_user=NULL;
	        }
	if(nagios_group!=NULL){
		free(nagios_group);
		nagios_group=NULL;
	        }

#ifdef DEBUG0
	printf("free_memory() end\n");
#endif

	return;
	}


/* free a notification list that was created */
void free_notification_list(void){
	notification *temp_notification;
	notification *next_notification;

#ifdef DEBUG0
	printf("free_notification_list() start\n");
#endif

	temp_notification=notification_list;
	while(temp_notification!=NULL){
		next_notification=temp_notification->next;
		free((void *)temp_notification);
		temp_notification=next_notification;
	        }

	/* reset notification list pointer */
	notification_list=NULL;

#ifdef DEBUG0
	printf("free_notification_list() end\n");
#endif

	return;
        }


/* reset all system-wide variables, so when we've receive a SIGHUP we can restart cleanly */
int reset_variables(void){
	int x;

#ifdef DEBUG0
	printf("reset_variables() start\n");
#endif

	strncpy(log_file,DEFAULT_LOG_FILE,sizeof(log_file));
	log_file[sizeof(log_file)-1]='\x0';
	strncpy(temp_file,DEFAULT_TEMP_FILE,sizeof(temp_file));
	temp_file[sizeof(temp_file)-1]='\x0';
	strncpy(command_file,DEFAULT_COMMAND_FILE,sizeof(command_file));
	command_file[sizeof(command_file)-1]='\x0';
	if(sigrestart==FALSE){
		strncpy(lock_file,DEFAULT_LOCK_FILE,sizeof(lock_file));
		lock_file[sizeof(lock_file)-1]='\x0';
	        }
	strncpy(log_archive_path,DEFAULT_LOG_ARCHIVE_PATH,sizeof(log_archive_path));
	log_archive_path[sizeof(log_archive_path)-1]='\x0';

	nagios_user=(char *)malloc(strlen(DEFAULT_NAGIOS_USER)+1);
	if(nagios_user!=NULL)
		strcpy(nagios_user,DEFAULT_NAGIOS_USER);
	nagios_group=(char *)malloc(strlen(DEFAULT_NAGIOS_GROUP)+1);
	if(nagios_group!=NULL)
		strcpy(nagios_group,DEFAULT_NAGIOS_GROUP);

	use_syslog=DEFAULT_USE_SYSLOG;
	log_service_retries=DEFAULT_LOG_SERVICE_RETRIES;
	log_host_retries=DEFAULT_LOG_HOST_RETRIES;
	log_initial_states=DEFAULT_LOG_INITIAL_STATES;

	log_notifications=DEFAULT_NOTIFICATION_LOGGING;
	log_event_handlers=DEFAULT_LOG_EVENT_HANDLERS;
	log_external_commands=DEFAULT_LOG_EXTERNAL_COMMANDS;
	log_passive_checks=DEFAULT_LOG_PASSIVE_CHECKS;

	logging_options=NSLOG_RUNTIME_ERROR | NSLOG_RUNTIME_WARNING | NSLOG_VERIFICATION_ERROR | NSLOG_VERIFICATION_WARNING | NSLOG_CONFIG_ERROR | NSLOG_CONFIG_WARNING | NSLOG_PROCESS_INFO | NSLOG_HOST_NOTIFICATION | NSLOG_SERVICE_NOTIFICATION | NSLOG_EVENT_HANDLER | NSLOG_EXTERNAL_COMMAND | NSLOG_PASSIVE_CHECK | NSLOG_HOST_UP | NSLOG_HOST_DOWN | NSLOG_HOST_UNREACHABLE | NSLOG_SERVICE_OK | NSLOG_SERVICE_WARNING | NSLOG_SERVICE_UNKNOWN | NSLOG_SERVICE_CRITICAL | NSLOG_INFO_MESSAGE;

	syslog_options=NSLOG_RUNTIME_ERROR | NSLOG_RUNTIME_WARNING | NSLOG_VERIFICATION_ERROR | NSLOG_VERIFICATION_WARNING | NSLOG_CONFIG_ERROR | NSLOG_CONFIG_WARNING | NSLOG_PROCESS_INFO | NSLOG_HOST_NOTIFICATION | NSLOG_SERVICE_NOTIFICATION | NSLOG_EVENT_HANDLER | NSLOG_EXTERNAL_COMMAND | NSLOG_PASSIVE_CHECK | NSLOG_HOST_UP | NSLOG_HOST_DOWN | NSLOG_HOST_UNREACHABLE | NSLOG_SERVICE_OK | NSLOG_SERVICE_WARNING | NSLOG_SERVICE_UNKNOWN | NSLOG_SERVICE_CRITICAL | NSLOG_INFO_MESSAGE;

	service_check_timeout=DEFAULT_SERVICE_CHECK_TIMEOUT;
	host_check_timeout=DEFAULT_HOST_CHECK_TIMEOUT;
	event_handler_timeout=DEFAULT_EVENT_HANDLER_TIMEOUT;
	notification_timeout=DEFAULT_NOTIFICATION_TIMEOUT;
	ocsp_timeout=DEFAULT_OCSP_TIMEOUT;

	sleep_time=DEFAULT_SLEEP_TIME;
	interval_length=DEFAULT_INTERVAL_LENGTH;
	inter_check_delay_method=ICD_SMART;
	interleave_factor_method=ILF_SMART;

	use_aggressive_host_checking=DEFAULT_AGGRESSIVE_HOST_CHECKING;

	soft_state_dependencies=FALSE;

	retain_state_information=FALSE;
	retention_update_interval=DEFAULT_RETENTION_UPDATE_INTERVAL;
	use_retained_program_state=TRUE;

	command_check_interval=DEFAULT_COMMAND_CHECK_INTERVAL;
	service_check_reaper_interval=DEFAULT_SERVICE_REAPER_INTERVAL;
	freshness_check_interval=DEFAULT_FRESHNESS_CHECK_INTERVAL;

	check_external_commands=DEFAULT_CHECK_EXTERNAL_COMMANDS;
	check_orphaned_services=DEFAULT_CHECK_ORPHANED_SERVICES;
	check_service_freshness=DEFAULT_CHECK_SERVICE_FRESHNESS;

	log_rotation_method=LOG_ROTATION_NONE;

	last_command_check=0L;
	last_log_rotation=0L;

        max_parallel_service_checks=DEFAULT_MAX_PARALLEL_SERVICE_CHECKS;
        currently_running_service_checks=0;

	enable_notifications=TRUE;
	execute_service_checks=TRUE;
	accept_passive_service_checks=TRUE;
	enable_event_handlers=TRUE;
	obsess_over_services=FALSE;
	enable_failure_prediction=TRUE;

	aggregate_status_updates=TRUE;
	status_update_interval=DEFAULT_STATUS_UPDATE_INTERVAL;

	time_change_threshold=DEFAULT_TIME_CHANGE_THRESHOLD;

	enable_flap_detection=DEFAULT_ENABLE_FLAP_DETECTION;
	low_service_flap_threshold=DEFAULT_LOW_SERVICE_FLAP_THRESHOLD;
	high_service_flap_threshold=DEFAULT_HIGH_SERVICE_FLAP_THRESHOLD;
	low_host_flap_threshold=DEFAULT_LOW_HOST_FLAP_THRESHOLD;
	high_host_flap_threshold=DEFAULT_HIGH_HOST_FLAP_THRESHOLD;

	process_performance_data=DEFAULT_PROCESS_PERFORMANCE_DATA;

	date_format=DATE_FORMAT_US;

	macro_contact_name=NULL;
	macro_contact_alias=NULL;
	macro_host_name=NULL;
	macro_host_alias=NULL;
	macro_host_address=NULL;
	macro_service_description=NULL;
	macro_service_state=NULL;
	macro_output=NULL;
	macro_perfdata=NULL;
	macro_contact_email=NULL;
	macro_contact_pager=NULL;
	macro_admin_email=NULL;
	macro_admin_pager=NULL;
	macro_host_state=NULL;
	macro_state_type=NULL;
	macro_current_service_attempt=NULL;
	macro_current_host_attempt=NULL;
	macro_notification_type=NULL;
	macro_execution_time=NULL;
	macro_latency=NULL;

	macro_date_time[MACRO_DATETIME_LONGDATE]=NULL;
	macro_date_time[MACRO_DATETIME_SHORTDATE]=NULL;
	macro_date_time[MACRO_DATETIME_DATE]=NULL;
	macro_date_time[MACRO_DATETIME_TIME]=NULL;
	macro_date_time[MACRO_DATETIME_TIMET]=NULL;
	macro_date_time[MACRO_DATETIME_LASTCHECK]=NULL;
	macro_date_time[MACRO_DATETIME_LASTSTATECHANGE]=NULL;

	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++)
		macro_argv[x]=NULL;

	for(x=0;x<MAX_USER_MACROS;x++)
		macro_user[x]=NULL;

	global_host_event_handler=NULL;
	global_service_event_handler=NULL;

	ocsp_command=NULL;

	/* reset umask */
	umask(S_IWGRP|S_IWOTH);

#ifdef DEBUG0
	printf("reset_variables() end\n");
#endif

	return OK;
        }
