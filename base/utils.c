/*****************************************************************************
 *
 * UTILS.C - Miscellaneous utility functions for Nagios
 *
 * Copyright (c) 1999-2007 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   04-30-2007
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
#include "../include/statusdata.h"
#include "../include/comments.h"
#include "../include/nagios.h"
#include "../include/broker.h"
#include "../include/nebmods.h"
#include "../include/nebmodules.h"


#ifdef EMBEDDEDPERL
#include "../include/epn_nagios.h"
static PerlInterpreter *my_perl = NULL;
int use_embedded_perl=TRUE;
#endif

char            *my_strtok_buffer=NULL;
char            *original_my_strtok_buffer=NULL;


extern char	*config_file;
extern char	*log_file;
extern char     *command_file;
extern char     *temp_file;
extern char     *lock_file;
extern char	*log_archive_path;
extern char     *auth_file;
extern char	*p1_file;

extern char     *nagios_user;
extern char     *nagios_group;

extern char     *macro_x[MACRO_X_COUNT];
extern char     *macro_x_names[MACRO_X_COUNT];
extern char     *macro_argv[MAX_COMMAND_ARGUMENTS];
extern char     *macro_user[MAX_USER_MACROS];
extern char     *macro_contactaddress[MAX_CONTACT_ADDRESSES];
extern char     *macro_ondemand;

extern char     *global_host_event_handler;
extern char     *global_service_event_handler;

extern char     *ocsp_command;
extern char     *ochp_command;

extern char     *illegal_object_chars;
extern char     *illegal_output_chars;

extern int      use_regexp_matches;
extern int      use_true_regexp_matching;

extern int      sigshutdown;
extern int      sigrestart;
extern char     *sigs[35];
extern int      caught_signal;
extern int      sig_id;

extern int      daemon_mode;
extern int      daemon_dumps_core;

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
extern int      ochp_timeout;

extern int      log_initial_states;

extern double   sleep_time;
extern int      interval_length;
extern int      service_inter_check_delay_method;
extern int      host_inter_check_delay_method;
extern int      service_interleave_factor_method;
extern int      max_host_check_spread;
extern int      max_service_check_spread;

extern int      command_check_interval;
extern int      service_check_reaper_interval;
extern int      service_freshness_check_interval;
extern int      host_freshness_check_interval;
extern int      auto_rescheduling_interval;
extern int      auto_rescheduling_window;

extern int      check_external_commands;
extern int      check_orphaned_services;
extern int      check_service_freshness;
extern int      check_host_freshness;
extern int      auto_reschedule_checks;

extern int      use_aggressive_host_checking;

extern int      soft_state_dependencies;

extern int      retain_state_information;
extern int      retention_update_interval;
extern int      use_retained_program_state;
extern int      use_retained_scheduling_info;
extern int      retention_scheduling_horizon;
extern unsigned long modified_host_process_attributes;
extern unsigned long modified_service_process_attributes;

extern int      log_rotation_method;

extern time_t   last_command_check;
extern time_t	last_command_status_update;
extern time_t   last_log_rotation;

extern int      verify_config;
extern int      test_scheduling;

extern service_message svc_msg;
extern int      ipc_pipe[2];

extern int      max_parallel_service_checks;
extern int      currently_running_service_checks;

extern int      enable_notifications;
extern int      execute_service_checks;
extern int      accept_passive_service_checks;
extern int      execute_host_checks;
extern int      accept_passive_host_checks;
extern int      enable_event_handlers;
extern int      obsess_over_services;
extern int      obsess_over_hosts;
extern int      enable_failure_prediction;
extern int      process_performance_data;

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

extern int      date_format;

extern contact		*contact_list;
extern contactgroup	*contactgroup_list;
extern host             *host_list;
extern hostgroup	*hostgroup_list;
extern service          *service_list;
extern servicegroup     *servicegroup_list;
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
#ifdef CYGWIN
extern char     *_tzname[2] __declspec(dllimport);
#else
extern char     *tzname[2];
#endif
#endif

extern service_message svc_msg;

extern pthread_t       worker_threads[TOTAL_WORKER_THREADS];
extern circular_buffer external_command_buffer;
extern circular_buffer service_result_buffer;
extern circular_buffer event_broker_buffer;
extern int             external_command_buffer_slots;
extern int             check_result_buffer_slots;

/* from GNU defines errno as a macro, since it's a per-thread variable */
#ifndef errno
extern int errno;
#endif



/******************************************************************/
/************************ MACRO FUNCTIONS *************************/
/******************************************************************/

/* replace macros in notification commands with their values */
int process_macros(char *input_buffer, char *output_buffer, int buffer_length, int options){
	char *temp_buffer;
	int in_macro;
	int x;
	int arg_index=0;
	int user_index=0;
	int address_index=0;
	char *selected_macro=NULL;
	int clean_macro=FALSE;
	int found_macro_x=FALSE;

#ifdef DEBUG0
	printf("process_macros() start\n");
#endif

	if(output_buffer==NULL || buffer_length<=0)
		return ERROR;

	strcpy(output_buffer,"");

	if(input_buffer==NULL)
		return ERROR;

	in_macro=FALSE;

/*#define TEST_MACROS 1*/
#ifdef TEST_MACROS
	printf("**** BEGIN MACRO PROCESSING ***********\n");
	printf("Processing: '%s'\n",input_buffer);
	printf("Buffer length: %d\n",buffer_length);
#endif

	for(temp_buffer=my_strtok(input_buffer,"$");temp_buffer!=NULL;temp_buffer=my_strtok(NULL,"$")){

#ifdef TEST_MACROS
		printf("  Processing part: '%s'\n",temp_buffer);
#endif

		selected_macro=NULL;
		found_macro_x=FALSE;
		clean_macro=FALSE;

		if(in_macro==FALSE){
			if(strlen(output_buffer)+strlen(temp_buffer)<buffer_length-1){
				strncat(output_buffer,temp_buffer,buffer_length-strlen(output_buffer)-1);
				output_buffer[buffer_length-1]='\x0';
			        }
#ifdef TEST_MACROS
			printf("    Not currently in macro.  Running output (%d): '%s'\n",strlen(output_buffer),output_buffer);
#endif
			in_macro=TRUE;
			}
		else{

			if(strlen(output_buffer)+strlen(temp_buffer)<buffer_length-1){

				/* general macros */
				for(x=0;x<MACRO_X_COUNT;x++){
					if(macro_x_names[x]==NULL)
						continue;
					if(!strcmp(temp_buffer,macro_x_names[x])){

						selected_macro=macro_x[x];
						found_macro_x=TRUE;
						
						/* host/service output/perfdata macros get cleaned */
						if(x>=16 && x<=19){
							clean_macro=TRUE;
							options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
						        }

						break;
						}
				        }

				/* we already have a macro... */
				if(found_macro_x==TRUE)
					x=0;

				/* on-demand host macros */
				else if(strstr(temp_buffer,"HOST") && strstr(temp_buffer,":")){

					grab_on_demand_macro(temp_buffer);
					selected_macro=macro_ondemand;

					/* output/perfdata macros get cleaned */
					if(strstr(temp_buffer,"HOSTOUTPUT:")==temp_buffer || strstr(temp_buffer,"HOSTPERFDATA:")){
						clean_macro=TRUE;
						options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
					        }
				        }

				/* on-demand service macros */
				else if(strstr(temp_buffer,"SERVICE") && strstr(temp_buffer,":")){

					grab_on_demand_macro(temp_buffer);
					selected_macro=macro_ondemand;

					/* output/perfdata macros get cleaned */
					if(strstr(temp_buffer,"SERVICEOUTPUT:")==temp_buffer || strstr(temp_buffer,"SERVICEPERFDATA:")){
						clean_macro=TRUE;
						options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
					        }
				        }

				/* argv macros */
				else if(strstr(temp_buffer,"ARG")==temp_buffer){
					arg_index=atoi(temp_buffer+3);
					if(arg_index>=1 && arg_index<=MAX_COMMAND_ARGUMENTS)
						selected_macro=macro_argv[arg_index-1];
					else
						selected_macro=NULL;
				        }

				/* user macros */
				else if(strstr(temp_buffer,"USER")==temp_buffer){
					user_index=atoi(temp_buffer+4);
					if(user_index>=1 && user_index<=MAX_USER_MACROS)
						selected_macro=macro_user[user_index-1];
					else
						selected_macro=NULL;
				        }

				/* contact address macros */
				else if(strstr(temp_buffer,"CONTACTADDRESS")==temp_buffer){
					address_index=atoi(temp_buffer+14);
					if(address_index>=1 && address_index<=MAX_CONTACT_ADDRESSES)
						selected_macro=macro_contactaddress[address_index-1];
					else
						selected_macro=NULL;
				        }

				/* an escaped $ is done by specifying two $$ next to each other */
				else if(!strcmp(temp_buffer,"")){
#ifdef TEST_MACROS
					printf("    Escaped $.  Running output (%d): '%s'\n",strlen(output_buffer),output_buffer);
#endif
					strncat(output_buffer,"$",buffer_length-strlen(output_buffer)-1);
				        }

				/* a non-macro, just some user-defined string between two $s */
				else{
#ifdef TEST_MACROS
					printf("    Non-macro.  Running output (%d): '%s'\n",strlen(output_buffer),output_buffer);
#endif
					strncat(output_buffer,"$",buffer_length-strlen(output_buffer)-1);
					output_buffer[buffer_length-1]='\x0';
					strncat(output_buffer,temp_buffer,buffer_length-strlen(output_buffer)-1);
					output_buffer[buffer_length-1]='\x0';
					strncat(output_buffer,"$",buffer_length-strlen(output_buffer)-1);
				        }

				/* insert macro */
				if(selected_macro!=NULL){

					/* URL encode the macro */
					if(options & URL_ENCODE_MACRO_CHARS)
						selected_macro=get_url_encoded_string(selected_macro);
				
					/* some macros are cleaned... */
					if(clean_macro==TRUE || ((options & STRIP_ILLEGAL_MACRO_CHARS) || (options & ESCAPE_MACRO_CHARS)))
						strncat(output_buffer,(selected_macro==NULL)?"":clean_macro_chars(selected_macro,options),buffer_length-strlen(output_buffer)-1);

					/* others are not cleaned */
					else
						strncat(output_buffer,(selected_macro==NULL)?"":selected_macro,buffer_length-strlen(output_buffer)-1);

					/* free memory if necessary */
					if(options & URL_ENCODE_MACRO_CHARS)
						free(selected_macro);
#ifdef TEST_MACROS
					printf("    Just finished macro.  Running output (%d): '%s'\n",strlen(output_buffer),output_buffer);
#endif
				        }

				output_buffer[buffer_length-1]='\x0';
				}

			in_macro=FALSE;
			}
		}

#ifdef TEST_MACROS
	printf("Done.  Final output: '%s'\n",output_buffer);
	printf("**** END MACRO PROCESSING *************\n");
#endif

#ifdef DEBUG0
	printf("process_macros() end\n");
#endif

	return OK;
	}


/* grab macros that are specific to a particular service */
int grab_service_macros(service *svc){
	servicegroup *temp_servicegroup;
	serviceextinfo *temp_serviceextinfo;
	time_t current_time;
	unsigned long duration;
	int days;
	int hours;
	int minutes;
	int seconds;
	char temp_buffer[MAX_INPUT_BUFFER];
	
#ifdef DEBUG0
	printf("grab_service_macros() start\n");
#endif

	/* get the service description */
	if(macro_x[MACRO_SERVICEDESC]!=NULL)
		free(macro_x[MACRO_SERVICEDESC]);
	macro_x[MACRO_SERVICEDESC]=strdup(svc->description);

	/* get the plugin output */
	if(macro_x[MACRO_SERVICEOUTPUT]!=NULL)
		free(macro_x[MACRO_SERVICEOUTPUT]);
	if(svc->plugin_output==NULL)
		macro_x[MACRO_SERVICEOUTPUT]=NULL;
	else
		macro_x[MACRO_SERVICEOUTPUT]=strdup(svc->plugin_output);

	/* get the performance data */
	if(macro_x[MACRO_SERVICEPERFDATA]!=NULL)
		free(macro_x[MACRO_SERVICEPERFDATA]);
	if(svc->perf_data==NULL)
		macro_x[MACRO_SERVICEPERFDATA]=NULL;
	else
		macro_x[MACRO_SERVICEPERFDATA]=strdup(svc->perf_data);

	/* get the service check command */
	if(macro_x[MACRO_SERVICECHECKCOMMAND]!=NULL)
		free(macro_x[MACRO_SERVICECHECKCOMMAND]);
	if(svc->service_check_command==NULL)
		macro_x[MACRO_SERVICECHECKCOMMAND]=NULL;
	else
		macro_x[MACRO_SERVICECHECKCOMMAND]=strdup(svc->service_check_command);

	/* grab the service check type */
	if(macro_x[MACRO_SERVICECHECKTYPE]!=NULL)
		free(macro_x[MACRO_SERVICECHECKTYPE]);
	macro_x[MACRO_SERVICECHECKTYPE]=(char *)malloc(MAX_CHECKTYPE_LENGTH);
	if(macro_x[MACRO_SERVICECHECKTYPE]!=NULL)
		strcpy(macro_x[MACRO_SERVICECHECKTYPE],(svc->check_type==SERVICE_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");

	/* grab the service state type macro (this is usually overridden later on) */
	if(macro_x[MACRO_SERVICESTATETYPE]!=NULL)
		free(macro_x[MACRO_SERVICESTATETYPE]);
	macro_x[MACRO_SERVICESTATETYPE]=(char *)malloc(MAX_STATETYPE_LENGTH);
	if(macro_x[MACRO_SERVICESTATETYPE]!=NULL)
		strcpy(macro_x[MACRO_SERVICESTATETYPE],(svc->state_type==HARD_STATE)?"HARD":"SOFT");

	/* get the service state */
	if(macro_x[MACRO_SERVICESTATE]!=NULL)
		free(macro_x[MACRO_SERVICESTATE]);
	macro_x[MACRO_SERVICESTATE]=(char *)malloc(MAX_STATE_LENGTH);
	if(macro_x[MACRO_SERVICESTATE]!=NULL){
		if(svc->current_state==STATE_OK)
			strcpy(macro_x[MACRO_SERVICESTATE],"OK");
		else if(svc->current_state==STATE_WARNING)
			strcpy(macro_x[MACRO_SERVICESTATE],"WARNING");
		else if(svc->current_state==STATE_CRITICAL)
			strcpy(macro_x[MACRO_SERVICESTATE],"CRITICAL");
		else
			strcpy(macro_x[MACRO_SERVICESTATE],"UNKNOWN");
	        }

	/* get the service state id */
	if(macro_x[MACRO_SERVICESTATEID]!=NULL)
		free(macro_x[MACRO_SERVICESTATEID]);
	macro_x[MACRO_SERVICESTATEID]=(char *)malloc(MAX_STATEID_LENGTH);
	if(macro_x[MACRO_SERVICESTATEID]!=NULL){
		snprintf(macro_x[MACRO_SERVICESTATEID],MAX_STATEID_LENGTH,"%d",svc->current_state);
		macro_x[MACRO_SERVICESTATEID][MAX_STATEID_LENGTH-1]='\x0';
	        }

	/* get the current service check attempt macro */
	if(macro_x[MACRO_SERVICEATTEMPT]!=NULL)
		free(macro_x[MACRO_SERVICEATTEMPT]);
	macro_x[MACRO_SERVICEATTEMPT]=(char *)malloc(MAX_ATTEMPT_LENGTH);
	if(macro_x[MACRO_SERVICEATTEMPT]!=NULL){
		snprintf(macro_x[MACRO_SERVICEATTEMPT],MAX_ATTEMPT_LENGTH,"%d",svc->current_attempt);
		macro_x[MACRO_SERVICEATTEMPT][MAX_ATTEMPT_LENGTH-1]='\x0';
	        }

	/* get the execution time macro */
	if(macro_x[MACRO_SERVICEEXECUTIONTIME]!=NULL)
		free(macro_x[MACRO_SERVICEEXECUTIONTIME]);
	macro_x[MACRO_SERVICEEXECUTIONTIME]=(char *)malloc(MAX_EXECUTIONTIME_LENGTH);
	if(macro_x[MACRO_SERVICEEXECUTIONTIME]!=NULL){
		snprintf(macro_x[MACRO_SERVICEEXECUTIONTIME],MAX_EXECUTIONTIME_LENGTH,"%.3f",svc->execution_time);
		macro_x[MACRO_SERVICEEXECUTIONTIME][MAX_EXECUTIONTIME_LENGTH-1]='\x0';
	        }

	/* get the latency macro */
	if(macro_x[MACRO_SERVICELATENCY]!=NULL)
		free(macro_x[MACRO_SERVICELATENCY]);
	macro_x[MACRO_SERVICELATENCY]=(char *)malloc(MAX_LATENCY_LENGTH);
	if(macro_x[MACRO_SERVICELATENCY]!=NULL){
		snprintf(macro_x[MACRO_SERVICELATENCY],MAX_LATENCY_LENGTH,"%.3f",svc->latency);
		macro_x[MACRO_SERVICELATENCY][MAX_LATENCY_LENGTH-1]='\x0';
	        }

	/* get the last check time macro */
	if(macro_x[MACRO_LASTSERVICECHECK]!=NULL)
		free(macro_x[MACRO_LASTSERVICECHECK]);
	macro_x[MACRO_LASTSERVICECHECK]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTSERVICECHECK]!=NULL){
		snprintf(macro_x[MACRO_LASTSERVICECHECK],MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_check);
		macro_x[MACRO_LASTSERVICECHECK][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last state change time macro */
	if(macro_x[MACRO_LASTSERVICESTATECHANGE]!=NULL)
		free(macro_x[MACRO_LASTSERVICESTATECHANGE]);
	macro_x[MACRO_LASTSERVICESTATECHANGE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTSERVICESTATECHANGE]!=NULL){
		snprintf(macro_x[MACRO_LASTSERVICESTATECHANGE],MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_state_change);
		macro_x[MACRO_LASTSERVICESTATECHANGE][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last time ok macro */
	if(macro_x[MACRO_LASTSERVICEOK]!=NULL)
		free(macro_x[MACRO_LASTSERVICEOK]);
	macro_x[MACRO_LASTSERVICEOK]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTSERVICEOK]!=NULL){
		snprintf(macro_x[MACRO_LASTSERVICEOK],MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_time_ok);
		macro_x[MACRO_LASTSERVICEOK][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last time warning macro */
	if(macro_x[MACRO_LASTSERVICEWARNING]!=NULL)
		free(macro_x[MACRO_LASTSERVICEWARNING]);
	macro_x[MACRO_LASTSERVICEWARNING]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTSERVICEWARNING]!=NULL){
		snprintf(macro_x[MACRO_LASTSERVICEWARNING],MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_time_warning);
		macro_x[MACRO_LASTSERVICEWARNING][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last time unknown macro */
	if(macro_x[MACRO_LASTSERVICEUNKNOWN]!=NULL)
		free(macro_x[MACRO_LASTSERVICEUNKNOWN]);
	macro_x[MACRO_LASTSERVICEUNKNOWN]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTSERVICEUNKNOWN]!=NULL){
		snprintf(macro_x[MACRO_LASTSERVICEUNKNOWN],MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_time_unknown);
		macro_x[MACRO_LASTSERVICEUNKNOWN][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last time critical macro */
	if(macro_x[MACRO_LASTSERVICECRITICAL]!=NULL)
		free(macro_x[MACRO_LASTSERVICECRITICAL]);
	macro_x[MACRO_LASTSERVICECRITICAL]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTSERVICECRITICAL]!=NULL){
		snprintf(macro_x[MACRO_LASTSERVICECRITICAL],MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_time_critical);
		macro_x[MACRO_LASTSERVICECRITICAL][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the service downtime depth */
	if(macro_x[MACRO_SERVICEDOWNTIME]!=NULL)
		free(macro_x[MACRO_SERVICEDOWNTIME]);
	macro_x[MACRO_SERVICEDOWNTIME]=(char *)malloc(MAX_DOWNTIME_LENGTH);
	if(macro_x[MACRO_SERVICEDOWNTIME]!=NULL){
		snprintf(macro_x[MACRO_SERVICEDOWNTIME],MAX_DOWNTIME_LENGTH,"%d",svc->scheduled_downtime_depth);
		macro_x[MACRO_SERVICEDOWNTIME][MAX_DOWNTIME_LENGTH-1]='\x0';
	        }

	/* get the percent state change */
	if(macro_x[MACRO_SERVICEPERCENTCHANGE]!=NULL)
		free(macro_x[MACRO_SERVICEPERCENTCHANGE]);
	macro_x[MACRO_SERVICEPERCENTCHANGE]=(char *)malloc(MAX_PERCENTCHANGE_LENGTH);
	if(macro_x[MACRO_SERVICEPERCENTCHANGE]!=NULL){
		snprintf(macro_x[MACRO_SERVICEPERCENTCHANGE],MAX_PERCENTCHANGE_LENGTH,"%.2f",svc->percent_state_change);
		macro_x[MACRO_SERVICEPERCENTCHANGE][MAX_PERCENTCHANGE_LENGTH-1]='\x0';
	        }

	time(&current_time);
	duration=(unsigned long)(current_time-svc->last_state_change);

	/* get the state duration in seconds */
	if(macro_x[MACRO_SERVICEDURATIONSEC]!=NULL)
		free(macro_x[MACRO_SERVICEDURATIONSEC]);
	macro_x[MACRO_SERVICEDURATIONSEC]=(char *)malloc(MAX_DURATION_LENGTH);
	if(macro_x[MACRO_SERVICEDURATIONSEC]!=NULL){
		snprintf(macro_x[MACRO_SERVICEDURATIONSEC],MAX_DURATION_LENGTH,"%lu",duration);
		macro_x[MACRO_SERVICEDURATIONSEC][MAX_DURATION_LENGTH-1]='\x0';
	        }

	/* get the state duration */
	if(macro_x[MACRO_SERVICEDURATION]!=NULL)
		free(macro_x[MACRO_SERVICEDURATION]);
	macro_x[MACRO_SERVICEDURATION]=(char *)malloc(MAX_DURATION_LENGTH);
	if(macro_x[MACRO_SERVICEDURATION]!=NULL){
		days=duration/86400;
		duration-=(days*86400);
		hours=duration/3600;
		duration-=(hours*3600);
		minutes=duration/60;
		duration-=(minutes*60);
		seconds=duration;
		snprintf(macro_x[MACRO_SERVICEDURATION],MAX_DURATION_LENGTH,"%dd %dh %dm %ds",days,hours,minutes,seconds);
		macro_x[MACRO_SERVICEDURATION][MAX_DURATION_LENGTH-1]='\x0';
	        }

	/* find one servicegroup (there may be none or several) this service is associated with */
	for(temp_servicegroup=servicegroup_list;temp_servicegroup!=NULL;temp_servicegroup=temp_servicegroup->next){
		if(is_service_member_of_servicegroup(temp_servicegroup,svc)==TRUE)
			break;
	        }

	/* get the servicegroup name */
	if(macro_x[MACRO_SERVICEGROUPNAME]!=NULL)
		free(macro_x[MACRO_SERVICEGROUPNAME]);
	macro_x[MACRO_SERVICEGROUPNAME]=NULL;
	if(temp_servicegroup!=NULL)
		macro_x[MACRO_SERVICEGROUPNAME]=strdup(temp_servicegroup->group_name);
	
	/* get the servicegroup alias */
	if(macro_x[MACRO_SERVICEGROUPALIAS]!=NULL)
		free(macro_x[MACRO_SERVICEGROUPALIAS]);
	macro_x[MACRO_SERVICEGROUPALIAS]=NULL;
	if(temp_servicegroup!=NULL)
		macro_x[MACRO_SERVICEGROUPALIAS]=strdup(temp_servicegroup->alias);

	if((temp_serviceextinfo=find_serviceextinfo(svc->host_name,svc->description))){

		/* get the action url */
		if(macro_x[MACRO_SERVICEACTIONURL]!=NULL)
			free(macro_x[MACRO_SERVICEACTIONURL]);
		if(temp_serviceextinfo->action_url==NULL)
			macro_x[MACRO_SERVICEACTIONURL]=NULL;
		else
			macro_x[MACRO_SERVICEACTIONURL]=strdup(temp_serviceextinfo->action_url);

		/* get the notes url */
		if(macro_x[MACRO_SERVICENOTESURL]!=NULL)
			free(macro_x[MACRO_SERVICENOTESURL]);
		if(temp_serviceextinfo->notes_url==NULL)
			macro_x[MACRO_SERVICENOTESURL]=NULL;
		else
			macro_x[MACRO_SERVICENOTESURL]=strdup(temp_serviceextinfo->notes_url);

		/* get the notes */
		if(macro_x[MACRO_SERVICENOTES]!=NULL)
			free(macro_x[MACRO_SERVICENOTES]);
		if(temp_serviceextinfo->notes==NULL)
			macro_x[MACRO_SERVICENOTES]=NULL;
		else
			macro_x[MACRO_SERVICENOTES]=strdup(temp_serviceextinfo->notes);
	        }

	/* get the date/time macros */
	grab_datetime_macros();

	strip(macro_x[MACRO_SERVICEOUTPUT]);
	strip(macro_x[MACRO_SERVICEPERFDATA]);
	strip(macro_x[MACRO_SERVICECHECKCOMMAND]);
	strip(macro_x[MACRO_SERVICENOTES]);

	/* notes and action URL macros may themselves contain macros, so process them... */
	if(macro_x[MACRO_SERVICEACTIONURL]!=NULL){
		process_macros(macro_x[MACRO_SERVICEACTIONURL],temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
		free(macro_x[MACRO_SERVICEACTIONURL]);
		macro_x[MACRO_SERVICEACTIONURL]=strdup(temp_buffer);
	        }
	if(macro_x[MACRO_SERVICENOTESURL]!=NULL){
		process_macros(macro_x[MACRO_SERVICENOTESURL],temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
		free(macro_x[MACRO_SERVICENOTESURL]);
		macro_x[MACRO_SERVICENOTESURL]=strdup(temp_buffer);
	        }

#ifdef DEBUG0
	printf("grab_service_macros() end\n");
#endif

	return OK;
        }


/* grab macros that are specific to a particular host */
int grab_host_macros(host *hst){
	hostgroup *temp_hostgroup;
	hostextinfo *temp_hostextinfo;
	time_t current_time;
	unsigned long duration;
	int days;
	int hours;
	int minutes;
	int seconds;
	char temp_buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("grab_host_macros() start\n");
#endif

	/* get the host name */
	if(macro_x[MACRO_HOSTNAME]!=NULL)
		free(macro_x[MACRO_HOSTNAME]);
	macro_x[MACRO_HOSTNAME]=strdup(hst->name);
	
	/* get the host alias */
	if(macro_x[MACRO_HOSTALIAS]!=NULL)
		free(macro_x[MACRO_HOSTALIAS]);
	macro_x[MACRO_HOSTALIAS]=strdup(hst->alias);

	/* get the host address */
	if(macro_x[MACRO_HOSTADDRESS]!=NULL)
		free(macro_x[MACRO_HOSTADDRESS]);
	macro_x[MACRO_HOSTADDRESS]=strdup(hst->address);

	/* get the host state */
	if(macro_x[MACRO_HOSTSTATE]!=NULL)
		free(macro_x[MACRO_HOSTSTATE]);
	macro_x[MACRO_HOSTSTATE]=(char *)malloc(MAX_STATE_LENGTH);
	if(macro_x[MACRO_HOSTSTATE]!=NULL){
		if(hst->current_state==HOST_DOWN)
			strcpy(macro_x[MACRO_HOSTSTATE],"DOWN");
		else if(hst->current_state==HOST_UNREACHABLE)
			strcpy(macro_x[MACRO_HOSTSTATE],"UNREACHABLE");
		else
			strcpy(macro_x[MACRO_HOSTSTATE],"UP");
	        }

	/* get the host state id */
	if(macro_x[MACRO_HOSTSTATEID]!=NULL)
		free(macro_x[MACRO_HOSTSTATEID]);
	macro_x[MACRO_HOSTSTATEID]=(char *)malloc(MAX_STATEID_LENGTH);
	if(macro_x[MACRO_HOSTSTATEID]!=NULL){
		snprintf(macro_x[MACRO_HOSTSTATEID],MAX_STATEID_LENGTH,"%d",hst->current_state);
		macro_x[MACRO_HOSTSTATEID][MAX_STATEID_LENGTH-1]='\x0';
	        }

	/* grab the host check type */
	if(macro_x[MACRO_HOSTCHECKTYPE]!=NULL)
		free(macro_x[MACRO_HOSTCHECKTYPE]);
	macro_x[MACRO_HOSTCHECKTYPE]=(char *)malloc(MAX_CHECKTYPE_LENGTH);
	if(macro_x[MACRO_HOSTCHECKTYPE]!=NULL)
		strcpy(macro_x[MACRO_HOSTCHECKTYPE],(hst->check_type==HOST_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");

	/* get the host state type macro */
	if(macro_x[MACRO_HOSTSTATETYPE]!=NULL)
		free(macro_x[MACRO_HOSTSTATETYPE]);
	macro_x[MACRO_HOSTSTATETYPE]=(char *)malloc(MAX_STATETYPE_LENGTH);
	if(macro_x[MACRO_HOSTSTATETYPE]!=NULL)
		strcpy(macro_x[MACRO_HOSTSTATETYPE],(hst->state_type==HARD_STATE)?"HARD":"SOFT");

	/* get the plugin output */
	if(macro_x[MACRO_HOSTOUTPUT]!=NULL)
		free(macro_x[MACRO_HOSTOUTPUT]);
	if(hst->plugin_output==NULL)
		macro_x[MACRO_HOSTOUTPUT]=NULL;
	else
		macro_x[MACRO_HOSTOUTPUT]=strdup(hst->plugin_output);

	/* get the performance data */
	if(macro_x[MACRO_HOSTPERFDATA]!=NULL)
		free(macro_x[MACRO_HOSTPERFDATA]);
	if(hst->perf_data==NULL)
		macro_x[MACRO_HOSTPERFDATA]=NULL;
	else
		macro_x[MACRO_HOSTPERFDATA]=strdup(hst->perf_data);

	/* get the host check command */
	if(macro_x[MACRO_HOSTCHECKCOMMAND]!=NULL)
		free(macro_x[MACRO_HOSTCHECKCOMMAND]);
	if(hst->host_check_command==NULL)
		macro_x[MACRO_HOSTCHECKCOMMAND]=NULL;
	else
		macro_x[MACRO_HOSTCHECKCOMMAND]=strdup(hst->host_check_command);

	/* get the host current attempt */
	if(macro_x[MACRO_HOSTATTEMPT]!=NULL)
		free(macro_x[MACRO_HOSTATTEMPT]);
	macro_x[MACRO_HOSTATTEMPT]=(char *)malloc(MAX_ATTEMPT_LENGTH);
	if(macro_x[MACRO_HOSTATTEMPT]!=NULL){
		snprintf(macro_x[MACRO_HOSTATTEMPT],MAX_ATTEMPT_LENGTH,"%d",hst->current_attempt);
		macro_x[MACRO_HOSTATTEMPT][MAX_ATTEMPT_LENGTH-1]='\x0';
	        }

	/* get the host downtime depth */
	if(macro_x[MACRO_HOSTDOWNTIME]!=NULL)
		free(macro_x[MACRO_HOSTDOWNTIME]);
	macro_x[MACRO_HOSTDOWNTIME]=(char *)malloc(MAX_DOWNTIME_LENGTH);
	if(macro_x[MACRO_HOSTDOWNTIME]!=NULL){
		snprintf(macro_x[MACRO_HOSTDOWNTIME],MAX_DOWNTIME_LENGTH,"%d",hst->scheduled_downtime_depth);
		macro_x[MACRO_HOSTDOWNTIME][MAX_DOWNTIME_LENGTH-1]='\x0';
	        }

	/* get the percent state change */
	if(macro_x[MACRO_HOSTPERCENTCHANGE]!=NULL)
		free(macro_x[MACRO_HOSTPERCENTCHANGE]);
	macro_x[MACRO_HOSTPERCENTCHANGE]=(char *)malloc(MAX_PERCENTCHANGE_LENGTH);
	if(macro_x[MACRO_HOSTPERCENTCHANGE]!=NULL){
		snprintf(macro_x[MACRO_HOSTPERCENTCHANGE],MAX_PERCENTCHANGE_LENGTH,"%.2f",hst->percent_state_change);
		macro_x[MACRO_HOSTPERCENTCHANGE][MAX_PERCENTCHANGE_LENGTH-1]='\x0';
	        }

	time(&current_time);
	duration=(unsigned long)(current_time-hst->last_state_change);

	/* get the state duration in seconds */
	if(macro_x[MACRO_HOSTDURATIONSEC]!=NULL)
		free(macro_x[MACRO_HOSTDURATIONSEC]);
	macro_x[MACRO_HOSTDURATIONSEC]=(char *)malloc(MAX_DURATION_LENGTH);
	if(macro_x[MACRO_HOSTDURATIONSEC]!=NULL){
		snprintf(macro_x[MACRO_HOSTDURATIONSEC],MAX_DURATION_LENGTH,"%lu",duration);
		macro_x[MACRO_HOSTDURATIONSEC][MAX_DURATION_LENGTH-1]='\x0';
	        }

	/* get the state duration */
	if(macro_x[MACRO_HOSTDURATION]!=NULL)
		free(macro_x[MACRO_HOSTDURATION]);
	macro_x[MACRO_HOSTDURATION]=(char *)malloc(MAX_DURATION_LENGTH);
	if(macro_x[MACRO_HOSTDURATION]!=NULL){
		days=duration/86400;
		duration-=(days*86400);
		hours=duration/3600;
		duration-=(hours*3600);
		minutes=duration/60;
		duration-=(minutes*60);
		seconds=duration;
		snprintf(macro_x[MACRO_HOSTDURATION],MAX_DURATION_LENGTH,"%dd %dh %dm %ds",days,hours,minutes,seconds);
		macro_x[MACRO_HOSTDURATION][MAX_DURATION_LENGTH-1]='\x0';
	        }

	/* get the execution time macro */
	if(macro_x[MACRO_HOSTEXECUTIONTIME]!=NULL)
		free(macro_x[MACRO_HOSTEXECUTIONTIME]);
	macro_x[MACRO_HOSTEXECUTIONTIME]=(char *)malloc(MAX_EXECUTIONTIME_LENGTH);
	if(macro_x[MACRO_HOSTEXECUTIONTIME]!=NULL){
		snprintf(macro_x[MACRO_HOSTEXECUTIONTIME],MAX_EXECUTIONTIME_LENGTH,"%.3f",hst->execution_time);
		macro_x[MACRO_HOSTEXECUTIONTIME][MAX_EXECUTIONTIME_LENGTH-1]='\x0';
	        }

	/* get the latency macro */
	if(macro_x[MACRO_HOSTLATENCY]!=NULL)
		free(macro_x[MACRO_HOSTLATENCY]);
	macro_x[MACRO_HOSTLATENCY]=(char *)malloc(MAX_LATENCY_LENGTH);
	if(macro_x[MACRO_HOSTLATENCY]!=NULL){
		snprintf(macro_x[MACRO_HOSTLATENCY],MAX_LATENCY_LENGTH,"%.3f",hst->latency);
		macro_x[MACRO_HOSTLATENCY][MAX_LATENCY_LENGTH-1]='\x0';
	        }

	/* get the last check time macro */
	if(macro_x[MACRO_LASTHOSTCHECK]!=NULL)
		free(macro_x[MACRO_LASTHOSTCHECK]);
	macro_x[MACRO_LASTHOSTCHECK]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTHOSTCHECK]!=NULL){
		snprintf(macro_x[MACRO_LASTHOSTCHECK],MAX_DATETIME_LENGTH,"%lu",(unsigned long)hst->last_check);
		macro_x[MACRO_LASTHOSTCHECK][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last state change time macro */
	if(macro_x[MACRO_LASTHOSTSTATECHANGE]!=NULL)
		free(macro_x[MACRO_LASTHOSTSTATECHANGE]);
	macro_x[MACRO_LASTHOSTSTATECHANGE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTHOSTSTATECHANGE]!=NULL){
		snprintf(macro_x[MACRO_LASTHOSTSTATECHANGE],MAX_DATETIME_LENGTH,"%lu",(unsigned long)hst->last_state_change);
		macro_x[MACRO_LASTHOSTSTATECHANGE][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last time up macro */
	if(macro_x[MACRO_LASTHOSTUP]!=NULL)
		free(macro_x[MACRO_LASTHOSTUP]);
	macro_x[MACRO_LASTHOSTUP]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTHOSTUP]!=NULL){
		snprintf(macro_x[MACRO_LASTHOSTUP],MAX_DATETIME_LENGTH,"%lu",(unsigned long)hst->last_time_up);
		macro_x[MACRO_LASTHOSTUP][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last time down macro */
	if(macro_x[MACRO_LASTHOSTDOWN]!=NULL)
		free(macro_x[MACRO_LASTHOSTDOWN]);
	macro_x[MACRO_LASTHOSTDOWN]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTHOSTDOWN]!=NULL){
		snprintf(macro_x[MACRO_LASTHOSTDOWN],MAX_DATETIME_LENGTH,"%lu",(unsigned long)hst->last_time_down);
		macro_x[MACRO_LASTHOSTDOWN][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last time unreachable macro */
	if(macro_x[MACRO_LASTHOSTUNREACHABLE]!=NULL)
		free(macro_x[MACRO_LASTHOSTUNREACHABLE]);
	macro_x[MACRO_LASTHOSTUNREACHABLE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTHOSTUNREACHABLE]!=NULL){
		snprintf(macro_x[MACRO_LASTHOSTUNREACHABLE],MAX_DATETIME_LENGTH,"%lu",(unsigned long)hst->last_time_unreachable);
		macro_x[MACRO_LASTHOSTUNREACHABLE][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* find one hostgroup (there may be none or several) this host is associated with */
	for(temp_hostgroup=hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
		if(is_host_member_of_hostgroup(temp_hostgroup,hst)==TRUE)
			break;
	        }

	/* get the hostgroup name */
	if(macro_x[MACRO_HOSTGROUPNAME]!=NULL)
		free(macro_x[MACRO_HOSTGROUPNAME]);
	macro_x[MACRO_HOSTGROUPNAME]=NULL;
	if(temp_hostgroup!=NULL)
		macro_x[MACRO_HOSTGROUPNAME]=strdup(temp_hostgroup->group_name);
	
	/* get the hostgroup alias */
	if(macro_x[MACRO_HOSTGROUPALIAS]!=NULL)
		free(macro_x[MACRO_HOSTGROUPALIAS]);
	macro_x[MACRO_HOSTGROUPALIAS]=NULL;
	if(temp_hostgroup!=NULL)
		macro_x[MACRO_HOSTGROUPALIAS]=strdup(temp_hostgroup->alias);

	if((temp_hostextinfo=find_hostextinfo(hst->name))){

		/* get the action url */
		if(macro_x[MACRO_HOSTACTIONURL]!=NULL)
			free(macro_x[MACRO_HOSTACTIONURL]);
		if(temp_hostextinfo->action_url==NULL)
			macro_x[MACRO_HOSTACTIONURL]=NULL;
		else
			macro_x[MACRO_HOSTACTIONURL]=strdup(temp_hostextinfo->action_url);

		/* get the notes url */
		if(macro_x[MACRO_HOSTNOTESURL]!=NULL)
			free(macro_x[MACRO_HOSTNOTESURL]);
		if(temp_hostextinfo->notes_url==NULL)
			macro_x[MACRO_HOSTNOTESURL]=NULL;
		else
			macro_x[MACRO_HOSTNOTESURL]=strdup(temp_hostextinfo->notes_url);

		/* get the notes */
		if(macro_x[MACRO_HOSTNOTES]!=NULL)
			free(macro_x[MACRO_HOSTNOTES]);
		if(temp_hostextinfo->notes==NULL)
			macro_x[MACRO_HOSTNOTES]=NULL;
		else
			macro_x[MACRO_HOSTNOTES]=strdup(temp_hostextinfo->notes);
	        }

	/* get the date/time macros */
	grab_datetime_macros();

	strip(macro_x[MACRO_HOSTOUTPUT]);
	strip(macro_x[MACRO_HOSTPERFDATA]);
	strip(macro_x[MACRO_HOSTCHECKCOMMAND]);
	strip(macro_x[MACRO_HOSTNOTES]);

	/* notes and action URL macros may themselves contain macros, so process them... */
	if(macro_x[MACRO_HOSTACTIONURL]!=NULL){
		process_macros(macro_x[MACRO_HOSTACTIONURL],temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
		free(macro_x[MACRO_HOSTACTIONURL]);
		macro_x[MACRO_HOSTACTIONURL]=strdup(temp_buffer);
	        }
	if(macro_x[MACRO_HOSTNOTESURL]!=NULL){
		process_macros(macro_x[MACRO_HOSTNOTESURL],temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
		free(macro_x[MACRO_HOSTNOTESURL]);
		macro_x[MACRO_HOSTNOTESURL]=strdup(temp_buffer);
	        }

#ifdef DEBUG0
	printf("grab_host_macros() end\n");
#endif

	return OK;
        }


/* grab an on-demand host or service macro */
int grab_on_demand_macro(char *str){
	char *macro=NULL;
	char *first_arg=NULL;
	char *second_arg=NULL;
	char result_buffer[MAX_INPUT_BUFFER];
	int result_buffer_len, delimiter_len;
	host *temp_host;
	hostgroup *temp_hostgroup;
	hostgroupmember *temp_hostgroupmember;
	service *temp_service;
	servicegroup *temp_servicegroup;
	servicegroupmember *temp_servicegroupmember;
	char *ptr;
	int return_val=ERROR;

#ifdef DEBUG0
	printf("grab_on_demand_macro() start\n");
#endif

	/* clear the on-demand macro */
	if(macro_ondemand!=NULL){
		free(macro_ondemand);
		macro_ondemand=NULL;
	        }

	/* get the first argument */
	macro=strdup(str);
	if(macro==NULL)
		return ERROR;

	/* get the host name */
	ptr=strchr(macro,':');
	if(ptr==NULL){
		free(macro);
		return ERROR;
	        }
	/* terminate the macro name at the first arg's delimiter */
	ptr[0]='\x0';
	first_arg=ptr+1;

	/* get the second argument (if present) */
	ptr=strchr(first_arg,':');
	if(ptr!=NULL){
		/* terminate the first arg at the second arg's delimiter */
		ptr[0]='\x0';
		second_arg=ptr+1;
	        }

	/* process the macro */
	if(strstr(macro,"HOST")){

		/* process a host macro */
		if(second_arg==NULL){
			temp_host=find_host(first_arg);
			return_val=grab_on_demand_host_macro(temp_host,macro);
	                }

		/* process a host macro containing a hostgroup */
		else{
			temp_hostgroup=find_hostgroup(first_arg);
			if(temp_hostgroup==NULL){
				free(macro);
				return ERROR;
				}

			return_val=OK;  /* start off assuming there's no error */
			result_buffer[0]='\0';
			result_buffer[sizeof(result_buffer)-1]='\0';
			result_buffer_len=0;
			delimiter_len=strlen(second_arg);

			/* process each host in the hostgroup */
			temp_hostgroupmember=temp_hostgroup->members;
			if(temp_hostgroupmember==NULL){
				macro_ondemand=strdup("");
				free(macro);
				return OK;
				}
			while(1){
				temp_host=find_host(temp_hostgroupmember->host_name);
				if(grab_on_demand_host_macro(temp_host,macro)==OK){
					strncat(result_buffer,macro_ondemand,sizeof(result_buffer)-result_buffer_len-1);
					result_buffer_len+=strlen(macro_ondemand);
					if(result_buffer_len>sizeof(result_buffer)-1){
						return_val=ERROR;
						break;
						}
					temp_hostgroupmember=temp_hostgroupmember->next;
					if(temp_hostgroupmember==NULL)
						break;
					strncat(result_buffer,second_arg,sizeof(result_buffer)-result_buffer_len-1);
					result_buffer_len+=delimiter_len;
					if(result_buffer_len>sizeof(result_buffer)-1){
						return_val=ERROR;
						break;
						}
					}
				else{
					return_val=ERROR;
					temp_hostgroupmember=temp_hostgroupmember->next;
					if(temp_hostgroupmember==NULL)
						break;
					}

				free(macro_ondemand);
				macro_ondemand=NULL;
				}

			free(macro_ondemand);
			macro_ondemand=strdup(result_buffer);
			}
	        }

	else if(strstr(macro,"SERVICE")){

		/* second args will either be service description or delimiter */
		if(second_arg==NULL){
			free(macro);
			return ERROR;
	                }

		/* process a service macro */
		temp_service=find_service(first_arg, second_arg);
		if(temp_service!=NULL)
			return_val=grab_on_demand_service_macro(temp_service,macro);

		/* process a service macro containing a servicegroup */
		else{
			temp_servicegroup=find_servicegroup(first_arg);
			if(temp_servicegroup==NULL){
				free(macro);
				return ERROR;
				}

			return_val=OK;  /* start off assuming there's no error */
			result_buffer[0]='\0';
			result_buffer[sizeof(result_buffer)-1]='\0';
			result_buffer_len=0;
			delimiter_len=strlen(second_arg);

			/* process each service in the servicegroup */
			temp_servicegroupmember=temp_servicegroup->members;
			if(temp_servicegroupmember==NULL){
				macro_ondemand=strdup("");
				free(macro);
				return OK;
				}
			while(1){
				temp_service=find_service(temp_servicegroupmember->host_name,temp_servicegroupmember->service_description);
				if(grab_on_demand_service_macro(temp_service,macro)==OK){
					strncat(result_buffer,macro_ondemand,sizeof(result_buffer)-result_buffer_len-1);
					result_buffer_len+=strlen(macro_ondemand);
					if(result_buffer_len>sizeof(result_buffer)-1){
						return_val=ERROR;
						break;
						}
					temp_servicegroupmember=temp_servicegroupmember->next;
					if(temp_servicegroupmember==NULL)
						break;
					strncat(result_buffer,second_arg,sizeof(result_buffer)-result_buffer_len-1);
					result_buffer_len+=delimiter_len;
					if(result_buffer_len>sizeof(result_buffer)-1){
						return_val=ERROR;
						break;
						}
					}
				else{
					return_val=ERROR;
					temp_servicegroupmember=temp_servicegroupmember->next;
					if(temp_servicegroupmember==NULL)
						break;
					}

				free(macro_ondemand);
				macro_ondemand=NULL;
				}

			free(macro_ondemand);
			macro_ondemand=strdup(result_buffer);
			}
	        }

	else
		return_val=ERROR;

	free(macro);

#ifdef DEBUG0
	printf("grab_on_demand_macro() end\n");
#endif

	return return_val;
        }


/* grab an on-demand host macro */
int grab_on_demand_host_macro(host *hst, char *macro){
	hostgroup *temp_hostgroup;
	hostextinfo *temp_hostextinfo;
	char temp_buffer[MAX_INPUT_BUFFER];
	time_t current_time;
	unsigned long duration;
	int days;
	int hours;
	int minutes;
	int seconds;

#ifdef DEBUG0
	printf("grab_on_demand_host_macro() start\n");
#endif

	if(hst==NULL || macro==NULL)
		return ERROR;

	/* initialize the macro */
	macro_ondemand=NULL;

	time(&current_time);
	duration=(unsigned long)(current_time-hst->last_state_change);

	/* find one hostgroup (there may be none or several) this host is associated with */
	for(temp_hostgroup=hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
		if(is_host_member_of_hostgroup(temp_hostgroup,hst)==TRUE)
			break;
	        }

	/* get the host alias */
	if(!strcmp(macro,"HOSTALIAS"))
		macro_ondemand=strdup(hst->alias);

	/* get the host address */
	else if(!strcmp(macro,"HOSTADDRESS"))
		macro_ondemand=strdup(hst->address);

	/* get the host state */
	else if(!strcmp(macro,"HOSTSTATE")){
		macro_ondemand=(char *)malloc(MAX_STATE_LENGTH);
		if(macro_ondemand!=NULL){
			if(hst->current_state==HOST_DOWN)
				strcpy(macro_ondemand,"DOWN");
			else if(hst->current_state==HOST_UNREACHABLE)
				strcpy(macro_ondemand,"UNREACHABLE");
			else
				strcpy(macro_ondemand,"UP");
		        }
	        }

	/* get the host state id */
	else if(!strcmp(macro,"HOSTSTATEID")){
		macro_ondemand=(char *)malloc(MAX_STATEID_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_STATEID_LENGTH,"%d",hst->current_state);
			macro_ondemand[MAX_STATEID_LENGTH-1]='\x0';
		        }
	        }

	/* grab the host check type */
	else if(!strcmp(macro,"HOSTCHECKTYPE")){
		macro_ondemand=(char *)malloc(MAX_CHECKTYPE_LENGTH);
		if(macro_ondemand!=NULL)
			strcpy(macro_ondemand,(hst->check_type==HOST_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");
		}

	/* get the host state type macro */
	else if(!strcmp(macro,"HOSTSTATETYPE")){
		macro_ondemand=(char *)malloc(MAX_STATETYPE_LENGTH);
		if(macro_ondemand!=NULL)
			strcpy(macro_ondemand,(hst->state_type==HARD_STATE)?"HARD":"SOFT");
	        }

	/* get the plugin output */
	else if(!strcmp(macro,"HOSTOUTPUT")){
		if(hst->plugin_output==NULL)
			macro_ondemand=NULL;
		else{
			macro_ondemand=strdup(hst->plugin_output);
			strip(macro_ondemand);
		        }
	        }

	/* get the performance data */
	else if(!strcmp(macro,"HOSTPERFDATA")){
		if(hst->perf_data==NULL)
			macro_ondemand=NULL;
		else{
			macro_ondemand=strdup(hst->perf_data);
			strip(macro_ondemand);
		        }
	        }

	/* get the host current attempt */
	else if(!strcmp(macro,"HOSTATTEMPT")){
		macro_ondemand=(char *)malloc(MAX_ATTEMPT_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_ATTEMPT_LENGTH,"%d",hst->current_attempt);
			macro_ondemand[MAX_ATTEMPT_LENGTH-1]='\x0';
		        }
	        }

	/* get the host downtime depth */
	else if(!strcmp(macro,"HOSTDOWNTIME")){
		macro_ondemand=(char *)malloc(MAX_DOWNTIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DOWNTIME_LENGTH,"%d",hst->scheduled_downtime_depth);
			macro_ondemand[MAX_ATTEMPT_LENGTH-1]='\x0';
		        }
	        }

	/* get the percent state change */
	else if(!strcmp(macro,"HOSTPERCENTCHANGE")){
		macro_ondemand=(char *)malloc(MAX_PERCENTCHANGE_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_PERCENTCHANGE_LENGTH,"%.2f",hst->percent_state_change);
			macro_ondemand[MAX_PERCENTCHANGE_LENGTH-1]='\x0';
		        }
	        }

	/* get the state duration in seconds */
	else if(!strcmp(macro,"HOSTDURATIONSEC")){
		macro_ondemand=(char *)malloc(MAX_DURATION_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DURATION_LENGTH,"%lu",duration);
			macro_ondemand[MAX_DURATION_LENGTH-1]='\x0';
		        }
	        }

	/* get the state duration */
	else if(!strcmp(macro,"HOSTDURATION")){
		macro_ondemand=(char *)malloc(MAX_DURATION_LENGTH);
		if(macro_ondemand!=NULL){
			days=duration/86400;
			duration-=(days*86400);
			hours=duration/3600;
			duration-=(hours*3600);
			minutes=duration/60;
			duration-=(minutes*60);
			seconds=duration;
			snprintf(macro_ondemand,MAX_DURATION_LENGTH,"%dd %dh %dm %ds",days,hours,minutes,seconds);
			macro_ondemand[MAX_DURATION_LENGTH-1]='\x0';
		        }
	        }

	/* get the execution time macro */
	else if(!strcmp(macro,"HOSTEXECUTIONTIME")){
		macro_ondemand=(char *)malloc(MAX_EXECUTIONTIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_EXECUTIONTIME_LENGTH,"%.3f",hst->execution_time);
			macro_ondemand[MAX_EXECUTIONTIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the latency macro */
	else if(!strcmp(macro,"HOSTLATENCY")){
		macro_ondemand=(char *)malloc(MAX_LATENCY_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_LATENCY_LENGTH,"%.3f",hst->latency);
			macro_ondemand[MAX_LATENCY_LENGTH-1]='\x0';
		        }
	        }

	/* get the last check time macro */
	else if(!strcmp(macro,"LASTHOSTCHECK")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH,"%lu",(unsigned long)hst->last_check);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the last state change time macro */
	else if(!strcmp(macro,"LASTHOSTSTATECHANGE")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH,"%lu",(unsigned long)hst->last_state_change);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the last time up macro */
	else if(!strcmp(macro,"LASTHOSTUP")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH,"%lu",(unsigned long)hst->last_time_up);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the last time down macro */
	else if(!strcmp(macro,"LASTHOSTDOWN")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH,"%lu",(unsigned long)hst->last_time_down);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the last time unreachable macro */
	else if(!strcmp(macro,"LASTHOSTUNREACHABLE")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH,"%lu",(unsigned long)hst->last_time_unreachable);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the hostgroup name */
	else if(!strcmp(macro,"HOSTGROUPNAME")){
		if(temp_hostgroup!=NULL)
			macro_ondemand=strdup(temp_hostgroup->group_name);
	        }
	
	/* get the hostgroup alias */
	else if(!strcmp(macro,"HOSTGROUPALIAS")){
		if(temp_hostgroup!=NULL)
			macro_ondemand=strdup(temp_hostgroup->alias);
	        }

	/* extended info */
	else if(!strcmp(macro,"HOSTACTIONURL") || !strcmp(macro,"HOSTNOTESURL") || !strcmp(macro,"HOSTNOTES")){
		
		/* find the extended info entry */
		if((temp_hostextinfo=find_hostextinfo(hst->name))){

			/* action url */
			if(!strcmp(macro,"HOSTACTIONURL")){

				if(temp_hostextinfo->action_url)
					macro_ondemand=strdup(temp_hostextinfo->action_url);

				/* action URL macros may themselves contain macros, so process them... */
				if(macro_ondemand!=NULL){
					process_macros(macro_ondemand,temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
					free(macro_ondemand);
					macro_ondemand=strdup(temp_buffer);
				        }
			        }

			/* notes url */
			if(!strcmp(macro,"HOSTNOTESURL")){

				if(temp_hostextinfo->notes_url)
					macro_ondemand=strdup(temp_hostextinfo->notes_url);

				/* action URL macros may themselves contain macros, so process them... */
				if(macro_ondemand!=NULL){
					process_macros(macro_ondemand,temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
					free(macro_ondemand);
					macro_ondemand=strdup(temp_buffer);
				        }
			        }

			/* notes */
			if(!strcmp(macro,"HOSTNOTES")){
				if(temp_hostextinfo->notes)
					macro_ondemand=strdup(temp_hostextinfo->notes);
			        }
		        }
	        }

	else
		return ERROR;

#ifdef DEBUG0
	printf("grab_on_demand_host_macro() end\n");
#endif

	return OK;
        }


/* grab an on-demand service macro */
int grab_on_demand_service_macro(service *svc, char *macro){
	servicegroup *temp_servicegroup;
	serviceextinfo *temp_serviceextinfo;
	char temp_buffer[MAX_INPUT_BUFFER];
	time_t current_time;
	unsigned long duration;
	int days;
	int hours;
	int minutes;
	int seconds;

#ifdef DEBUG0
	printf("grab_on_demand_service_macro() start\n");
#endif

	if(svc==NULL || macro==NULL)
		return ERROR;

	/* initialize the macro */
	macro_ondemand=NULL;

	time(&current_time);
	duration=(unsigned long)(current_time-svc->last_state_change);

	/* find one servicegroup (there may be none or several) this service is associated with */
	for(temp_servicegroup=servicegroup_list;temp_servicegroup!=NULL;temp_servicegroup=temp_servicegroup->next){
		if(is_service_member_of_servicegroup(temp_servicegroup,svc)==TRUE)
			break;
	        }

	/* get the plugin output */
	if(!strcmp(macro,"SERVICEOUTPUT")){
		if(svc->plugin_output==NULL)
			macro_ondemand=NULL;
		else{
			macro_ondemand=strdup(svc->plugin_output);
			strip(macro_ondemand);
		        }
	        }

	/* get the performance data */
	else if(!strcmp(macro,"SERVICEPERFDATA")){
		if(svc->perf_data==NULL)
			macro_ondemand=NULL;
		else{
			macro_ondemand=strdup(svc->perf_data);
			strip(macro_ondemand);
		        }
	        }

	/* grab the servuce check type */
	else if(!strcmp(macro,"SERVICECHECKTYPE")){
		macro_ondemand=(char *)malloc(MAX_CHECKTYPE_LENGTH);
		if(macro_ondemand!=NULL)
			strcpy(macro_ondemand,(svc->check_type==SERVICE_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");
		}

	/* grab the service state type macro (this is usually overridden later on) */
	else if(!strcmp(macro,"SERVICESTATETYPE")){
		macro_ondemand=(char *)malloc(MAX_STATETYPE_LENGTH);
		if(macro_ondemand!=NULL)
			strcpy(macro_ondemand,(svc->state_type==HARD_STATE)?"HARD":"SOFT");
	        }

	/* get the service state */
	else if(!strcmp(macro,"SERVICESTATE")){
		macro_ondemand=(char *)malloc(MAX_STATE_LENGTH);
		if(macro_ondemand!=NULL){
			if(svc->current_state==STATE_OK)
				strcpy(macro_ondemand,"OK");
			else if(svc->current_state==STATE_WARNING)
				strcpy(macro_ondemand,"WARNING");
			else if(svc->current_state==STATE_CRITICAL)
				strcpy(macro_ondemand,"CRITICAL");
			else
				strcpy(macro_ondemand,"UNKNOWN");
		        }
	        }

	/* get the service state id */
	else if(!strcmp(macro,"SERVICESTATEID")){
		macro_ondemand=(char *)malloc(MAX_STATEID_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_STATEID_LENGTH,"%d",svc->current_state);
			macro_ondemand[MAX_STATEID_LENGTH-1]='\x0';
		        }
	        }

	/* get the current service check attempt macro */
	else if(!strcmp(macro,"SERVICEATTEMPT")){
		macro_ondemand=(char *)malloc(MAX_ATTEMPT_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_ATTEMPT_LENGTH,"%d",svc->current_attempt);
			macro_ondemand[MAX_ATTEMPT_LENGTH-1]='\x0';
		        }
	        }

	/* get the execution time macro */
	else if(!strcmp(macro,"SERVICEEXECUTIONTIME")){
		macro_ondemand=(char *)malloc(MAX_EXECUTIONTIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_EXECUTIONTIME_LENGTH,"%.3f",svc->execution_time);
			macro_ondemand[MAX_EXECUTIONTIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the latency macro */
	else if(!strcmp(macro,"SERVICELATENCY")){
		macro_ondemand=(char *)malloc(MAX_LATENCY_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_LATENCY_LENGTH,"%.3f",svc->latency);
			macro_ondemand[MAX_LATENCY_LENGTH-1]='\x0';
		        }
	        }

	/* get the last check time macro */
	else if(!strcmp(macro,"LASTSERVICECHECK")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_check);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the last state change time macro */
	else if(!strcmp(macro,"LASTSERVICESTATECHANGE")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_state_change);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the last time ok macro */
	else if(!strcmp(macro,"LASTSERVICEOK")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_time_ok);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the last time warning macro */
	else if(!strcmp(macro,"LASTSERVICEWARNING")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_time_warning);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the last time unknown macro */
	else if(!strcmp(macro,"LASTSERVICEUNKNOWN")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_time_unknown);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the last time critical macro */
	else if(!strcmp(macro,"LASTSERVICECRITICAL")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH,"%lu",(unsigned long)svc->last_time_critical);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the service downtime depth */
	else if(!strcmp(macro,"SERVICEDOWNTIME")){
		macro_ondemand=(char *)malloc(MAX_DOWNTIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DOWNTIME_LENGTH,"%d",svc->scheduled_downtime_depth);
			macro_ondemand[MAX_DOWNTIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the percent state change */
	else if(!strcmp(macro,"SERVICEPERCENTCHANGE")){
		macro_ondemand=(char *)malloc(MAX_PERCENTCHANGE_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_PERCENTCHANGE_LENGTH,"%.2f",svc->percent_state_change);
			macro_ondemand[MAX_PERCENTCHANGE_LENGTH-1]='\x0';
		        }
	        }

	/* get the state duration in seconds */
	else if(!strcmp(macro,"SERVICEDURATIONSEC")){
		macro_ondemand=(char *)malloc(MAX_DURATION_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DURATION_LENGTH,"%lu",duration);
			macro_ondemand[MAX_DURATION_LENGTH-1]='\x0';
		        }
	        }

	/* get the state duration */
	else if(!strcmp(macro,"SERVICEDURATION")){
		macro_ondemand=(char *)malloc(MAX_DURATION_LENGTH);
		if(macro_ondemand!=NULL){
			days=duration/86400;
			duration-=(days*86400);
			hours=duration/3600;
			duration-=(hours*3600);
			minutes=duration/60;
			duration-=(minutes*60);
			seconds=duration;
			snprintf(macro_ondemand,MAX_DURATION_LENGTH,"%dd %dh %dm %ds",days,hours,minutes,seconds);
			macro_ondemand[MAX_DURATION_LENGTH-1]='\x0';
		        }
	        }

	/* get the servicegroup name */
	else if(!strcmp(macro,"SERVICEGROUPNAME")){
		if(temp_servicegroup!=NULL)
			macro_ondemand=strdup(temp_servicegroup->group_name);
	        }
	
	/* get the servicegroup alias */
	else if(!strcmp(macro,"SERVICEGROUPALIAS")){
		if(temp_servicegroup!=NULL)
			macro_ondemand=strdup(temp_servicegroup->alias);
	        }

	/* extended info */
	else if(!strcmp(macro,"SERVICEACTIONURL") || !strcmp(macro,"SERVICENOTESURL") || !strcmp(macro,"SERVICENOTES")){
		
		/* find the extended info entry */
		if((temp_serviceextinfo=find_serviceextinfo(svc->host_name,svc->description))){

			/* action url */
			if(!strcmp(macro,"SERVICEACTIONURL")){

				if(temp_serviceextinfo->action_url)
					macro_ondemand=strdup(temp_serviceextinfo->action_url);

				/* action URL macros may themselves contain macros, so process them... */
				if(macro_ondemand!=NULL){
					process_macros(macro_ondemand,temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
					free(macro_ondemand);
					macro_ondemand=strdup(temp_buffer);
				        }
			        }

			/* notes url */
			if(!strcmp(macro,"SERVICENOTESURL")){

				if(temp_serviceextinfo->notes_url)
					macro_ondemand=strdup(temp_serviceextinfo->notes_url);

				/* action URL macros may themselves contain macros, so process them... */
				if(macro_ondemand!=NULL){
					process_macros(macro_ondemand,temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
					free(macro_ondemand);
					macro_ondemand=strdup(temp_buffer);
				        }
			        }

			/* notes */
			if(!strcmp(macro,"SERVICENOTES")){
				if(temp_serviceextinfo->notes)
					macro_ondemand=strdup(temp_serviceextinfo->notes);
			        }
		        }
	        }

	else
		return ERROR;

#ifdef DEBUG0
	printf("grab_on_demand_service_macro() end\n");
#endif

	return OK;
        }


/* grab macros that are specific to a particular contact */
int grab_contact_macros(contact *cntct){
	int x;

#ifdef DEBUG0
	printf("grab_contact_macros() start\n");
#endif

	/* get the name */
	if(macro_x[MACRO_CONTACTNAME]!=NULL)
		free(macro_x[MACRO_CONTACTNAME]);
	macro_x[MACRO_CONTACTNAME]=strdup(cntct->name);

	/* get the alias */
	if(macro_x[MACRO_CONTACTALIAS]!=NULL)
		free(macro_x[MACRO_CONTACTALIAS]);
	macro_x[MACRO_CONTACTALIAS]=strdup(cntct->alias);

	/* get the email address */
	if(macro_x[MACRO_CONTACTEMAIL]!=NULL)
		free(macro_x[MACRO_CONTACTEMAIL]);
	if(cntct->email==NULL)
		macro_x[MACRO_CONTACTEMAIL]=NULL;
	else
		macro_x[MACRO_CONTACTEMAIL]=strdup(cntct->email);

	/* get the pager number */
	if(macro_x[MACRO_CONTACTPAGER]!=NULL)
		free(macro_x[MACRO_CONTACTPAGER]);
	if(cntct->pager==NULL)
		macro_x[MACRO_CONTACTPAGER]=NULL;
	else
		macro_x[MACRO_CONTACTPAGER]=strdup(cntct->pager);

	/* get misc contact addresses */
	for(x=0;x<MAX_CONTACT_ADDRESSES;x++){
		if(macro_contactaddress[x]!=NULL)
			free(macro_contactaddress[x]);
		if(cntct->address[x]==NULL)
			macro_contactaddress[x]=NULL;
		else
			macro_contactaddress[x]=strdup(cntct->address[x]);
	        }

	/* get the date/time macros */
	grab_datetime_macros();

	strip(macro_x[MACRO_CONTACTNAME]);
	strip(macro_x[MACRO_CONTACTALIAS]);
	strip(macro_x[MACRO_CONTACTEMAIL]);
	strip(macro_x[MACRO_CONTACTPAGER]);

	for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
		strip(macro_contactaddress[x]);
			

#ifdef DEBUG0
	printf("grab_contact_macros() end\n");
#endif

	return OK;
        }



/* grab summary macros (filtered for a specific contact) */
int grab_summary_macros(contact *temp_contact){
	host *temp_host;
	service  *temp_service;
	int authorized=TRUE;
	int problem=TRUE;

	int hosts_up=0;
	int hosts_down=0;
	int hosts_unreachable=0;
	int hosts_down_unhandled=0;
	int hosts_unreachable_unhandled=0;
	int host_problems=0;
	int host_problems_unhandled=0;

	int services_ok=0;
	int services_warning=0;
	int services_unknown=0;
	int services_critical=0;
	int services_warning_unhandled=0;
	int services_unknown_unhandled=0;
	int services_critical_unhandled=0;
	int service_problems=0;
	int service_problems_unhandled=0;

#ifdef DEBUG0
	printf("grab_summary_macros() start\n");
#endif

	/* get host totals */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* filter totals based on contact if necessary */
		if(temp_contact!=NULL)
			authorized=is_contact_for_host(temp_host,temp_contact);

		if(authorized==TRUE){
			problem=TRUE;
			
			if(temp_host->current_state==HOST_UP && temp_host->has_been_checked==TRUE)
				hosts_up++;
			else if(temp_host->current_state==HOST_DOWN){
				if(temp_host->scheduled_downtime_depth>0)
					problem=FALSE;
				if(temp_host->problem_has_been_acknowledged==TRUE)
					problem=FALSE;
				if(temp_host->checks_enabled==FALSE)
					problem=FALSE;
				if(problem==TRUE)
					hosts_down_unhandled++;
				hosts_down++;
				}
			else if(temp_host->current_state==HOST_UNREACHABLE){
				if(temp_host->scheduled_downtime_depth>0)
					problem=FALSE;
				if(temp_host->problem_has_been_acknowledged==TRUE)
					problem=FALSE;
				if(temp_host->checks_enabled==FALSE)
					problem=FALSE;
				if(problem==TRUE)
					hosts_down_unhandled++;
				hosts_unreachable++;
				}
			}
		}

	host_problems=hosts_down+hosts_unreachable;
	host_problems_unhandled=hosts_down_unhandled+hosts_unreachable_unhandled;

	/* get service totals */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		/* filter totals based on contact if necessary */
		if(temp_contact!=NULL)
			authorized=is_contact_for_service(temp_service,temp_contact);

		if(authorized==TRUE){
			problem=TRUE;
			
			if(temp_service->current_state==STATE_OK && temp_service->has_been_checked==TRUE)
				services_ok++;
			else if(temp_service->current_state==STATE_WARNING){
				temp_host=find_host(temp_service->host_name);
				if(temp_host!=NULL && (temp_host->current_state==HOST_DOWN || temp_host->current_state==HOST_UNREACHABLE))
					problem=FALSE;
				if(temp_service->scheduled_downtime_depth>0)
					problem=FALSE;
				if(temp_service->problem_has_been_acknowledged==TRUE)
					problem=FALSE;
				if(temp_service->checks_enabled==FALSE)
					problem=FALSE;
				if(problem==TRUE)
					services_warning_unhandled++;
				services_warning++;
		        	}
			else if(temp_service->current_state==STATE_UNKNOWN){
				temp_host=find_host(temp_service->host_name);
				if(temp_host!=NULL && (temp_host->current_state==HOST_DOWN || temp_host->current_state==HOST_UNREACHABLE))
					problem=FALSE;
				if(temp_service->scheduled_downtime_depth>0)
					problem=FALSE;
				if(temp_service->problem_has_been_acknowledged==TRUE)
					problem=FALSE;
				if(temp_service->checks_enabled==FALSE)
					problem=FALSE;
				if(problem==TRUE)
					services_unknown_unhandled++;
				services_unknown++;
				}
			else if(temp_service->current_state==STATE_CRITICAL){
				temp_host=find_host(temp_service->host_name);
				if(temp_host!=NULL && (temp_host->current_state==HOST_DOWN || temp_host->current_state==HOST_UNREACHABLE))
					problem=FALSE;
				if(temp_service->scheduled_downtime_depth>0)
					problem=FALSE;
				if(temp_service->problem_has_been_acknowledged==TRUE)
					problem=FALSE;
				if(temp_service->checks_enabled==FALSE)
					problem=FALSE;
				if(problem==TRUE)
					services_critical_unhandled++;
				services_critical++;
				}
			}
		}

	service_problems=services_warning+services_critical+services_unknown;
	service_problems_unhandled=services_warning_unhandled+services_critical_unhandled+services_unknown_unhandled;


	/* get total hosts up */
	if(macro_x[MACRO_TOTALHOSTSUP]!=NULL)
		free(macro_x[MACRO_TOTALHOSTSUP]);
	macro_x[MACRO_TOTALHOSTSUP]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALHOSTSUP]!=NULL){
		snprintf(macro_x[MACRO_TOTALHOSTSUP],MAX_TOTALS_LENGTH,"%d",hosts_up);
		macro_x[MACRO_TOTALHOSTSUP][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total hosts down */
	if(macro_x[MACRO_TOTALHOSTSDOWN]!=NULL)
		free(macro_x[MACRO_TOTALHOSTSDOWN]);
	macro_x[MACRO_TOTALHOSTSDOWN]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALHOSTSDOWN]!=NULL){
		snprintf(macro_x[MACRO_TOTALHOSTSDOWN],MAX_TOTALS_LENGTH,"%d",hosts_down);
		macro_x[MACRO_TOTALHOSTSDOWN][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total hosts unreachable */
	if(macro_x[MACRO_TOTALHOSTSUNREACHABLE]!=NULL)
		free(macro_x[MACRO_TOTALHOSTSUNREACHABLE]);
	macro_x[MACRO_TOTALHOSTSUNREACHABLE]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALHOSTSUNREACHABLE]!=NULL){
		snprintf(macro_x[MACRO_TOTALHOSTSUNREACHABLE],MAX_TOTALS_LENGTH,"%d",hosts_unreachable);
		macro_x[MACRO_TOTALHOSTSUNREACHABLE][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total unhandled hosts down */
	if(macro_x[MACRO_TOTALHOSTSDOWNUNHANDLED]!=NULL)
		free(macro_x[MACRO_TOTALHOSTSDOWNUNHANDLED]);
	macro_x[MACRO_TOTALHOSTSDOWNUNHANDLED]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALHOSTSDOWNUNHANDLED]!=NULL){
		snprintf(macro_x[MACRO_TOTALHOSTSDOWNUNHANDLED],MAX_TOTALS_LENGTH,"%d",hosts_down_unhandled);
		macro_x[MACRO_TOTALHOSTSDOWNUNHANDLED][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total unhandled hosts unreachable */
	if(macro_x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED]!=NULL)
		free(macro_x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED]);
	macro_x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED]!=NULL){
		snprintf(macro_x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED],MAX_TOTALS_LENGTH,"%d",hosts_unreachable_unhandled);
		macro_x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total host problems */
	if(macro_x[MACRO_TOTALHOSTPROBLEMS]!=NULL)
		free(macro_x[MACRO_TOTALHOSTPROBLEMS]);
	macro_x[MACRO_TOTALHOSTPROBLEMS]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALHOSTPROBLEMS]!=NULL){
		snprintf(macro_x[MACRO_TOTALHOSTPROBLEMS],MAX_TOTALS_LENGTH,"%d",host_problems);
		macro_x[MACRO_TOTALHOSTPROBLEMS][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total unhandled host problems */
	if(macro_x[MACRO_TOTALHOSTPROBLEMSUNHANDLED]!=NULL)
		free(macro_x[MACRO_TOTALHOSTPROBLEMSUNHANDLED]);
	macro_x[MACRO_TOTALHOSTPROBLEMSUNHANDLED]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALHOSTPROBLEMSUNHANDLED]!=NULL){
		snprintf(macro_x[MACRO_TOTALHOSTPROBLEMSUNHANDLED],MAX_TOTALS_LENGTH,"%d",host_problems_unhandled);
		macro_x[MACRO_TOTALHOSTPROBLEMSUNHANDLED][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total services ok */
	if(macro_x[MACRO_TOTALSERVICESOK]!=NULL)
		free(macro_x[MACRO_TOTALSERVICESOK]);
	macro_x[MACRO_TOTALSERVICESOK]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALSERVICESOK]!=NULL){
		snprintf(macro_x[MACRO_TOTALSERVICESOK],MAX_TOTALS_LENGTH,"%d",services_ok);
		macro_x[MACRO_TOTALSERVICESOK][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total services warning */
	if(macro_x[MACRO_TOTALSERVICESWARNING]!=NULL)
		free(macro_x[MACRO_TOTALSERVICESWARNING]);
	macro_x[MACRO_TOTALSERVICESWARNING]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALSERVICESWARNING]!=NULL){
		snprintf(macro_x[MACRO_TOTALSERVICESWARNING],MAX_TOTALS_LENGTH,"%d",services_warning);
		macro_x[MACRO_TOTALSERVICESWARNING][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total services critical */
	if(macro_x[MACRO_TOTALSERVICESCRITICAL]!=NULL)
		free(macro_x[MACRO_TOTALSERVICESCRITICAL]);
	macro_x[MACRO_TOTALSERVICESCRITICAL]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALSERVICESCRITICAL]!=NULL){
		snprintf(macro_x[MACRO_TOTALSERVICESCRITICAL],MAX_TOTALS_LENGTH,"%d",services_critical);
		macro_x[MACRO_TOTALSERVICESCRITICAL][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total services unknown */
	if(macro_x[MACRO_TOTALSERVICESUNKNOWN]!=NULL)
		free(macro_x[MACRO_TOTALSERVICESUNKNOWN]);
	macro_x[MACRO_TOTALSERVICESUNKNOWN]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALSERVICESUNKNOWN]!=NULL){
		snprintf(macro_x[MACRO_TOTALSERVICESUNKNOWN],MAX_TOTALS_LENGTH,"%d",services_unknown);
		macro_x[MACRO_TOTALSERVICESUNKNOWN][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total unhandled services warning */
	if(macro_x[MACRO_TOTALSERVICESWARNINGUNHANDLED]!=NULL)
		free(macro_x[MACRO_TOTALSERVICESWARNINGUNHANDLED]);
	macro_x[MACRO_TOTALSERVICESWARNINGUNHANDLED]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALSERVICESWARNINGUNHANDLED]!=NULL){
		snprintf(macro_x[MACRO_TOTALSERVICESWARNINGUNHANDLED],MAX_TOTALS_LENGTH,"%d",services_warning_unhandled);
		macro_x[MACRO_TOTALSERVICESWARNINGUNHANDLED][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total unhandled services critical */
	if(macro_x[MACRO_TOTALSERVICESCRITICALUNHANDLED]!=NULL)
		free(macro_x[MACRO_TOTALSERVICESCRITICALUNHANDLED]);
	macro_x[MACRO_TOTALSERVICESCRITICALUNHANDLED]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALSERVICESCRITICALUNHANDLED]!=NULL){
		snprintf(macro_x[MACRO_TOTALSERVICESCRITICALUNHANDLED],MAX_TOTALS_LENGTH,"%d",services_critical_unhandled);
		macro_x[MACRO_TOTALSERVICESCRITICALUNHANDLED][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total unhandled services unknown */
	if(macro_x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED]!=NULL)
		free(macro_x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED]);
	macro_x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED]!=NULL){
		snprintf(macro_x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED],MAX_TOTALS_LENGTH,"%d",services_unknown_unhandled);
		macro_x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total service problems */
	if(macro_x[MACRO_TOTALSERVICEPROBLEMS]!=NULL)
		free(macro_x[MACRO_TOTALSERVICEPROBLEMS]);
	macro_x[MACRO_TOTALSERVICEPROBLEMS]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALSERVICEPROBLEMS]!=NULL){
		snprintf(macro_x[MACRO_TOTALSERVICEPROBLEMS],MAX_TOTALS_LENGTH,"%d",service_problems);
		macro_x[MACRO_TOTALSERVICEPROBLEMS][MAX_TOTALS_LENGTH-1]='\x0';
		}

	/* get total unhandled service problems */
	if(macro_x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED]!=NULL)
		free(macro_x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED]);
	macro_x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED]=(char *)malloc(MAX_TOTALS_LENGTH);
	if(macro_x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED]!=NULL){
		snprintf(macro_x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED],MAX_TOTALS_LENGTH,"%d",service_problems_unhandled);
		macro_x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED][MAX_TOTALS_LENGTH-1]='\x0';
		}

#ifdef DEBUG0
	printf("grab_summary_macros() end\n");
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
	if(macro_x[MACRO_LONGDATETIME]==NULL)
		macro_x[MACRO_LONGDATETIME]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LONGDATETIME]!=NULL)
		get_datetime_string(&t,macro_x[MACRO_LONGDATETIME],MAX_DATETIME_LENGTH,LONG_DATE_TIME);

	/* get the current date/time (short format macro) */
	if(macro_x[MACRO_SHORTDATETIME]==NULL)
		macro_x[MACRO_SHORTDATETIME]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_SHORTDATETIME]!=NULL)
		get_datetime_string(&t,macro_x[MACRO_SHORTDATETIME],MAX_DATETIME_LENGTH,SHORT_DATE_TIME);

	/* get the short format date macro */
	if(macro_x[MACRO_DATE]==NULL)
		macro_x[MACRO_DATE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_DATE]!=NULL)
		get_datetime_string(&t,macro_x[MACRO_DATE],MAX_DATETIME_LENGTH,SHORT_DATE);

	/* get the short format time macro */
	if(macro_x[MACRO_TIME]==NULL)
		macro_x[MACRO_TIME]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_TIME]!=NULL)
		get_datetime_string(&t,macro_x[MACRO_TIME],MAX_DATETIME_LENGTH,SHORT_TIME);

	/* get the time_t format time macro */
	if(macro_x[MACRO_TIMET]==NULL)
		macro_x[MACRO_TIMET]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_TIMET]!=NULL){
		snprintf(macro_x[MACRO_TIMET],MAX_DATETIME_LENGTH,"%lu",(unsigned long)t);
		macro_x[MACRO_TIMET][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	strip(macro_x[MACRO_LONGDATETIME]);
	strip(macro_x[MACRO_SHORTDATETIME]);
	strip(macro_x[MACRO_DATE]);
	strip(macro_x[MACRO_TIME]);
	strip(macro_x[MACRO_TIMET]);

#ifdef DEBUG0
	printf("grab_datetime_macros() end\n");
#endif

	return OK;
        }



/* clear argv macros - used in commands */
int clear_argv_macros(void){
	int x;


#ifdef DEBUG0
	printf("clear_argv_macros() start\n");
#endif

	/* command argument macros */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){
		if(macro_argv[x]!=NULL){
			free(macro_argv[x]);
			macro_argv[x]=NULL;
		        }
	        }

#ifdef DEBUG0
	printf("clear_argv_macros() end\n");
#endif

	return OK;
        }



/* clear all macros that are not "constant" (i.e. they change throughout the course of monitoring) */
int clear_volatile_macros(void){
	int x;

#ifdef DEBUG0
	printf("clear_volatile_macros() start\n");
#endif

	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_ADMINEMAIL:
		case MACRO_ADMINPAGER:
		case MACRO_MAINCONFIGFILE:
		case MACRO_STATUSDATAFILE:
		case MACRO_COMMENTDATAFILE:
		case MACRO_DOWNTIMEDATAFILE:
		case MACRO_RETENTIONDATAFILE:
		case MACRO_OBJECTCACHEFILE:
		case MACRO_TEMPFILE:
		case MACRO_LOGFILE:
		case MACRO_RESOURCEFILE:
		case MACRO_COMMANDFILE:
		case MACRO_HOSTPERFDATAFILE:
		case MACRO_SERVICEPERFDATAFILE:
		case MACRO_PROCESSSTARTTIME:
			break;
		default:
			if(macro_x[x]!=NULL){
				free(macro_x[x]);
				macro_x[x]=NULL;
			        }
			break;
		        }
	        }

	/* command argument macros */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){
		if(macro_argv[x]!=NULL){
			free(macro_argv[x]);
			macro_argv[x]=NULL;
		        }
	        }

	/* contact address macros */
	for(x=0;x<MAX_CONTACT_ADDRESSES;x++){
		if(macro_contactaddress[x]!=NULL){
			free(macro_contactaddress[x]);
			macro_contactaddress[x]=NULL;
		        }
	        }

	/* clear on-demand macro */
	if(macro_ondemand!=NULL){
		free(macro_ondemand);
		macro_ondemand=NULL;
	        }

	clear_argv_macros();

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
	
	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_ADMINEMAIL:
		case MACRO_ADMINPAGER:
		case MACRO_MAINCONFIGFILE:
		case MACRO_STATUSDATAFILE:
		case MACRO_COMMENTDATAFILE:
		case MACRO_DOWNTIMEDATAFILE:
		case MACRO_RETENTIONDATAFILE:
		case MACRO_OBJECTCACHEFILE:
		case MACRO_TEMPFILE:
		case MACRO_LOGFILE:
		case MACRO_RESOURCEFILE:
		case MACRO_COMMANDFILE:
		case MACRO_HOSTPERFDATAFILE:
		case MACRO_SERVICEPERFDATAFILE:
		case MACRO_PROCESSSTARTTIME:
			if(macro_x[x]!=NULL){
				free(macro_x[x]);
				macro_x[x]=NULL;
			        }
			break;
		default:
			break;
		        }
	        }

#ifdef DEBUG0
	printf("clear_nonvolatile_macros() end\n");
#endif

	return OK;
        }


/* initializes the names of macros */
int init_macrox_names(void){
	int x;

#ifdef DEBUG0
	printf("init_macrox_names() start\n");
#endif

	/* initialize macro names */
	for(x=0;x<MACRO_X_COUNT;x++)
		macro_x_names[x]=NULL;

	/* initialize each macro name */
	add_macrox_name(MACRO_HOSTNAME,"HOSTNAME");
	add_macrox_name(MACRO_HOSTALIAS,"HOSTALIAS");
	add_macrox_name(MACRO_HOSTADDRESS,"HOSTADDRESS");
	add_macrox_name(MACRO_SERVICEDESC,"SERVICEDESC");
	add_macrox_name(MACRO_SERVICESTATE,"SERVICESTATE");
	add_macrox_name(MACRO_SERVICESTATEID,"SERVICESTATEID");
	add_macrox_name(MACRO_SERVICEATTEMPT,"SERVICEATTEMPT");
	add_macrox_name(MACRO_LONGDATETIME,"LONGDATETIME");
	add_macrox_name(MACRO_SHORTDATETIME,"SHORTDATETIME");
	add_macrox_name(MACRO_DATE,"DATE");
	add_macrox_name(MACRO_TIME,"TIME");
	add_macrox_name(MACRO_TIMET,"TIMET");
	add_macrox_name(MACRO_LASTHOSTCHECK,"LASTHOSTCHECK");
	add_macrox_name(MACRO_LASTSERVICECHECK,"LASTSERVICECHECK");
	add_macrox_name(MACRO_LASTHOSTSTATECHANGE,"LASTHOSTSTATECHANGE");
	add_macrox_name(MACRO_LASTSERVICESTATECHANGE,"LASTSERVICESTATECHANGE");
	add_macrox_name(MACRO_HOSTOUTPUT,"HOSTOUTPUT");
	add_macrox_name(MACRO_SERVICEOUTPUT,"SERVICEOUTPUT");
	add_macrox_name(MACRO_HOSTPERFDATA,"HOSTPERFDATA");
	add_macrox_name(MACRO_SERVICEPERFDATA,"SERVICEPERFDATA");
	add_macrox_name(MACRO_CONTACTNAME,"CONTACTNAME");
	add_macrox_name(MACRO_CONTACTALIAS,"CONTACTALIAS");
	add_macrox_name(MACRO_CONTACTEMAIL,"CONTACTEMAIL");
	add_macrox_name(MACRO_CONTACTPAGER,"CONTACTPAGER");
	add_macrox_name(MACRO_ADMINEMAIL,"ADMINEMAIL");
	add_macrox_name(MACRO_ADMINPAGER,"ADMINPAGER");
	add_macrox_name(MACRO_HOSTSTATE,"HOSTSTATE");
	add_macrox_name(MACRO_HOSTSTATEID,"HOSTSTATEID");
	add_macrox_name(MACRO_HOSTATTEMPT,"HOSTATTEMPT");
	add_macrox_name(MACRO_NOTIFICATIONTYPE,"NOTIFICATIONTYPE");
	add_macrox_name(MACRO_NOTIFICATIONNUMBER,"NOTIFICATIONNUMBER");
	add_macrox_name(MACRO_HOSTEXECUTIONTIME,"HOSTEXECUTIONTIME");
	add_macrox_name(MACRO_SERVICEEXECUTIONTIME,"SERVICEEXECUTIONTIME");
	add_macrox_name(MACRO_HOSTLATENCY,"HOSTLATENCY");
	add_macrox_name(MACRO_SERVICELATENCY,"SERVICELATENCY");
	add_macrox_name(MACRO_HOSTDURATION,"HOSTDURATION");
	add_macrox_name(MACRO_SERVICEDURATION,"SERVICEDURATION");
	add_macrox_name(MACRO_HOSTDURATIONSEC,"HOSTDURATIONSEC");
	add_macrox_name(MACRO_SERVICEDURATIONSEC,"SERVICEDURATIONSEC");
	add_macrox_name(MACRO_HOSTDOWNTIME,"HOSTDOWNTIME");
	add_macrox_name(MACRO_SERVICEDOWNTIME,"SERVICEDOWNTIME");
	add_macrox_name(MACRO_HOSTSTATETYPE,"HOSTSTATETYPE");
	add_macrox_name(MACRO_SERVICESTATETYPE,"SERVICESTATETYPE");
	add_macrox_name(MACRO_HOSTPERCENTCHANGE,"HOSTPERCENTCHANGE");
	add_macrox_name(MACRO_SERVICEPERCENTCHANGE,"SERVICEPERCENTCHANGE");
	add_macrox_name(MACRO_HOSTGROUPNAME,"HOSTGROUPNAME");
	add_macrox_name(MACRO_HOSTGROUPALIAS,"HOSTGROUPALIAS");
	add_macrox_name(MACRO_SERVICEGROUPNAME,"SERVICEGROUPNAME");
	add_macrox_name(MACRO_SERVICEGROUPALIAS,"SERVICEGROUPALIAS");
	add_macrox_name(MACRO_HOSTACKAUTHOR,"HOSTACKAUTHOR");
	add_macrox_name(MACRO_HOSTACKCOMMENT,"HOSTACKCOMMENT");
	add_macrox_name(MACRO_SERVICEACKAUTHOR,"SERVICEACKAUTHOR");
	add_macrox_name(MACRO_SERVICEACKCOMMENT,"SERVICEACKCOMMENT");
	add_macrox_name(MACRO_LASTSERVICEOK,"LASTSERVICEOK");
	add_macrox_name(MACRO_LASTSERVICEWARNING,"LASTSERVICEWARNING");
	add_macrox_name(MACRO_LASTSERVICEUNKNOWN,"LASTSERVICEUNKNOWN");
	add_macrox_name(MACRO_LASTSERVICECRITICAL,"LASTSERVICECRITICAL");
	add_macrox_name(MACRO_LASTHOSTUP,"LASTHOSTUP");
	add_macrox_name(MACRO_LASTHOSTDOWN,"LASTHOSTDOWN");
	add_macrox_name(MACRO_LASTHOSTUNREACHABLE,"LASTHOSTUNREACHABLE");
	add_macrox_name(MACRO_SERVICECHECKCOMMAND,"SERVICECHECKCOMMAND");
	add_macrox_name(MACRO_HOSTCHECKCOMMAND,"HOSTCHECKCOMMAND");
	add_macrox_name(MACRO_MAINCONFIGFILE,"MAINCONFIGFILE");
	add_macrox_name(MACRO_STATUSDATAFILE,"STATUSDATAFILE");
	add_macrox_name(MACRO_COMMENTDATAFILE,"COMMENTDATAFILE");
	add_macrox_name(MACRO_DOWNTIMEDATAFILE,"DOWNTIMEDATAFILE");
	add_macrox_name(MACRO_RETENTIONDATAFILE,"RETENTIONDATAFILE");
	add_macrox_name(MACRO_OBJECTCACHEFILE,"OBJECTCACHEFILE");
	add_macrox_name(MACRO_TEMPFILE,"TEMPFILE");
	add_macrox_name(MACRO_LOGFILE,"LOGFILE");
	add_macrox_name(MACRO_RESOURCEFILE,"RESOURCEFILE");
	add_macrox_name(MACRO_COMMANDFILE,"COMMANDFILE");
	add_macrox_name(MACRO_HOSTPERFDATAFILE,"HOSTPERFDATAFILE");
	add_macrox_name(MACRO_SERVICEPERFDATAFILE,"SERVICEPERFDATAFILE");
	add_macrox_name(MACRO_HOSTACTIONURL,"HOSTACTIONURL");
	add_macrox_name(MACRO_HOSTNOTESURL,"HOSTNOTESURL");
	add_macrox_name(MACRO_HOSTNOTES,"HOSTNOTES");
	add_macrox_name(MACRO_SERVICEACTIONURL,"SERVICEACTIONURL");
	add_macrox_name(MACRO_SERVICENOTESURL,"SERVICENOTESURL");
	add_macrox_name(MACRO_SERVICENOTES,"SERVICENOTES");
	add_macrox_name(MACRO_TOTALHOSTSUP,"TOTALHOSTSUP");
	add_macrox_name(MACRO_TOTALHOSTSDOWN,"TOTALHOSTSDOWN");
	add_macrox_name(MACRO_TOTALHOSTSUNREACHABLE,"TOTALHOSTSUNREACHABLE");
	add_macrox_name(MACRO_TOTALHOSTSDOWNUNHANDLED,"TOTALHOSTSDOWNUNHANDLED");
	add_macrox_name(MACRO_TOTALHOSTSUNREACHABLEUNHANDLED,"TOTALHOSTSUNREACHABLEUNHANDLED");
	add_macrox_name(MACRO_TOTALHOSTPROBLEMS,"TOTALHOSTPROBLEMS");
	add_macrox_name(MACRO_TOTALHOSTPROBLEMSUNHANDLED,"TOTALHOSTPROBLEMSUNHANDLED");
	add_macrox_name(MACRO_TOTALSERVICESOK,"TOTALSERVICESOK");
	add_macrox_name(MACRO_TOTALSERVICESWARNING,"TOTALSERVICESWARNING");
	add_macrox_name(MACRO_TOTALSERVICESCRITICAL,"TOTALSERVICESCRITICAL");
	add_macrox_name(MACRO_TOTALSERVICESUNKNOWN,"TOTALSERVICESUNKNOWN");
	add_macrox_name(MACRO_TOTALSERVICESWARNINGUNHANDLED,"TOTALSERVICESWARNINGUNHANDLED");
	add_macrox_name(MACRO_TOTALSERVICESCRITICALUNHANDLED,"TOTALSERVICESCRITICALUNHANDLED");
	add_macrox_name(MACRO_TOTALSERVICESUNKNOWNUNHANDLED,"TOTALSERVICESUNKNOWNUNHANDLED");
	add_macrox_name(MACRO_TOTALSERVICEPROBLEMS,"TOTALSERVICEPROBLEMS");
	add_macrox_name(MACRO_TOTALSERVICEPROBLEMSUNHANDLED,"TOTALSERVICEPROBLEMSUNHANDLED");
	add_macrox_name(MACRO_PROCESSSTARTTIME,"PROCESSSTARTTIME");
	add_macrox_name(MACRO_HOSTCHECKTYPE,"HOSTCHECKTYPE");
	add_macrox_name(MACRO_SERVICECHECKTYPE,"SERVICECHECKTYPE");

#ifdef DEBUG0
	printf("init_macrox_names() end\n");
#endif

	return OK;
        }


/* saves the name of a macro */
int add_macrox_name(int i, char *name){

	/* dup the macro name */
	macro_x_names[i]=strdup(name);

	return OK;
        }


/* free memory associated with the macrox names */
int free_macrox_names(void){
	int x;

#ifdef DEBUG0
	printf("free_macrox_names() start\n");
#endif

	/* free each macro name */
	for(x=0;x<MACRO_X_COUNT;x++){
		free(macro_x_names[x]);
		macro_x_names[x]=NULL;
	        }

#ifdef DEBUG0
	printf("free_macrox_names() end\n");
#endif

	return OK;
        }


/* sets or unsets all macro environment variables */
int set_all_macro_environment_vars(int set){

#ifdef DEBUG0
	printf("set_all_macro_environment_vars() start\n");
#endif

	set_macrox_environment_vars(set);
	set_argv_macro_environment_vars(set);

#ifdef DEBUG0
	printf("set_all_macro_environment_vars() start\n");
#endif

	return OK;
        }


/* sets or unsets macrox environment variables */
int set_macrox_environment_vars(int set){
	int x;

#ifdef DEBUG0
	printf("set_macrox_environment_vars() start\n");
#endif

	/* set each of the macrox environment variables */
	for(x=0;x<MACRO_X_COUNT;x++){

		/* host/service output/perfdata macros get cleaned */
		if(x>=16 && x<=19)
			set_macro_environment_var(macro_x_names[x],clean_macro_chars(macro_x[x],STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS),set);

		/* others don't */
		else
			set_macro_environment_var(macro_x_names[x],macro_x[x],set);
	        }

#ifdef DEBUG0
	printf("set_macrox_environment_vars() end\n");
#endif

	return OK;
        }


/* sets or unsets argv macro environment variables */
int set_argv_macro_environment_vars(int set){
	char macro_name[MAX_INPUT_BUFFER];
	int x;

#ifdef DEBUG0
	printf("set_argv_macro_environment_vars() start\n");
#endif

	/* set each of the argv macro environment variables */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){

		snprintf(macro_name,sizeof(macro_name),"ARG%d",x+1);
		macro_name[sizeof(macro_name)-1]='\x0';

		set_macro_environment_var(macro_name,macro_argv[x],set);
	        }

#ifdef DEBUG0
	printf("set_argv_macro_environment_vars() end\n");
#endif

	return OK;
        }


/* sets or unsets a macro environment variable */
int set_macro_environment_var(char *name, char *value, int set){
	char *env_macro_name=NULL;
#ifndef HAVE_SETENV
	char *env_macro_string=NULL;
#endif

#ifdef DEBUG0
	printf("set_macro_environment_var() start\n");
#endif

	/* we won't mess with null variable names */
	if(name==NULL)
		return ERROR;

	/* allocate memory */
	if((env_macro_name=(char *)malloc(strlen(MACRO_ENV_VAR_PREFIX)+strlen(name)+1))==NULL)
		return ERROR;

	/* create the name */
	strcpy(env_macro_name,"");
	strcpy(env_macro_name,MACRO_ENV_VAR_PREFIX);
	strcat(env_macro_name,name);

	/* set or unset the environment variable */
	if(set==TRUE){
#ifdef HAVE_SETENV
		setenv(env_macro_name,(value==NULL)?"":value,1);
#else
		/* needed for Solaris and systems that don't have setenv() */
		/* this will leak memory, but in a "controlled" way, since lost memory should be freed when the child process exits */
		env_macro_string=(char *)malloc(strlen(env_macro_name)+strlen((value==NULL)?"":value)+2);
		if(env_macro_string!=NULL){
			sprintf(env_macro_string,"%s=%s",env_macro_name,(value==NULL)?"":value);
			putenv(env_macro_string);
		        }
#endif
	        }
	else{
#ifdef HAVE_UNSETENV
		unsetenv(env_macro_name);
#endif
	        }

	/* free allocated memory */
	free(env_macro_name);

#ifdef DEBUG0
	printf("set_macro_environment_var() end\n");
#endif

	return OK;
        }




/******************************************************************/
/******************** SYSTEM COMMAND FUNCTIONS ********************/
/******************************************************************/


/* executes a system command - used for service checks and notifications */
int my_system(char *cmd,int timeout,int *early_timeout,double *exectime,char *output,int output_length){
        pid_t pid;
	int status;
	int result=0;
	char buffer[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	int fd[2];
	FILE *fp=NULL;
	int bytes_read=0;
	struct timeval start_time,end_time;
#ifdef EMBEDDEDPERL
	char fname[512];
	char *args[5] = {"",DO_CLEAN, "", "", NULL };
	int isperl;
	SV *plugin_hndlr_cr;
	STRLEN n_a ;
#ifdef aTHX
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
	*exectime=0.0;

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

	isperl=FALSE;

	if(strstr(buffer,"/bin/perl")!=NULL){

		isperl = TRUE;
		args[0] = fname;
		args[2] = "";

		if(strchr(cmd,' ')==NULL)
			args[3]="";
		else
			args[3]=cmd+strlen(fname)+1;

		/* call our perl interpreter to compile and optionally cache the compiled script. */

		ENTER;
		SAVETMPS;
		PUSHMARK(SP); 

		XPUSHs(sv_2mortal(newSVpv(args[0],0)));
		XPUSHs(sv_2mortal(newSVpv(args[1],0)));
		XPUSHs(sv_2mortal(newSVpv(args[2],0)));
		XPUSHs(sv_2mortal(newSVpv(args[3],0)));

		PUTBACK;

		call_pv("Embed::Persistent::eval_file", G_EVAL);

		SPAGAIN;

		if ( SvTRUE(ERRSV) ) {
							/*
							 * XXXX need pipe open to send the compilation failure message back to Nag ?
							 */
			(void) POPs ;

			snprintf(buffer,sizeof(buffer)-1,"%s", SvPVX(ERRSV));
			buffer[sizeof(buffer)-1]='\x0';
			strip(buffer);

#ifdef DEBUG1
			printf("embedded perl failed to  compile %s, compile error %s\n",fname,buffer);
#endif
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);

			return STATE_UNKNOWN;

			}
		else{
			plugin_hndlr_cr=newSVsv(POPs);
#ifdef DEBUG1
			printf("embedded perl successfully compiled  %s and returned plugin handler (Perl subroutine code ref)\n",fname);
#endif

			PUTBACK ;
			FREETMPS ;
			LEAVE ;

			}
		}
#endif 

	/* create a pipe */
	pipe(fd);

	/* make the pipe non-blocking */
	fcntl(fd[0],F_SETFL,O_NONBLOCK);
	fcntl(fd[1],F_SETFL,O_NONBLOCK);

	/* get the command start time */
	gettimeofday(&start_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_system_command(NEBTYPE_SYSTEM_COMMAND_START,NEBFLAG_NONE,NEBATTR_NONE,start_time,end_time,*exectime,timeout,*early_timeout,result,cmd,NULL,NULL);
#endif

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

		/* set environment variables */
		set_all_macro_environment_vars(TRUE);

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

			char *perl_output ;
			int count ;

			/* execute our previously compiled script - by call_pv("Embed::Persistent::eval_file",..) */
			ENTER;
			SAVETMPS;
			PUSHMARK(SP);

			XPUSHs(sv_2mortal(newSVpv(args[0],0)));
			XPUSHs(sv_2mortal(newSVpv(args[1],0)));
			XPUSHs(plugin_hndlr_cr);
			XPUSHs(sv_2mortal(newSVpv(args[3],0)));

			PUTBACK;

			count=call_pv("Embed::Persistent::run_package", G_ARRAY);
			/* count is a debug hook. It should always be two (2), because the persistence framework tries to return two (2) args */

			SPAGAIN;

			perl_output=POPpx ;
			strip(perl_output);
			strncpy(buffer, perl_output, sizeof(buffer));
			buffer[sizeof(buffer)-1]='\x0';
			status=POPi ;

			PUTBACK;
			FREETMPS;
			LEAVE;                                    

#ifdef DEBUG0
			printf("embedded perl ran command %s with output %d, %s\n",fname, status, buffer);
#endif

			/* write the output back to the parent process */
			write(fd[1],buffer,strlen(buffer)+1);

			/* close pipe for writing */
			close(fd[1]);

			/* reset the alarm */
			alarm(0);

			_exit(status);
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

			/* ADDED 01/04/2004 */
			/* ignore any additional lines of output */
			while(fgets(temp_buffer,sizeof(temp_buffer)-1,fp));

			/* close the command and get termination status */
			status=pclose(fp);
			
			/* report an error if we couldn't close the command */
			if(status==-1)
				result=STATE_CRITICAL;
			else {
				result=WEXITSTATUS(status);
				if(result==0 && WIFSIGNALED(status)){
					/* like bash */
					result=128+WTERMSIG(status);
					snprintf(buffer,sizeof(buffer)-1,"(Command received signal %d!)\n",WTERMSIG(status));
					buffer[sizeof(buffer)-1]='\x0';
					}
				}

			/* write the output back to the parent process */
			write(fd[1],buffer,strlen(buffer)+1);
		        }

		/* close pipe for writing */
		close(fd[1]);

		/* reset the alarm */
		alarm(0);
		
		/* clear environment variables */
		set_all_macro_environment_vars(FALSE);

		_exit(result);
	        }

	/* parent waits for child to finish executing command */
	else{
		
		/* close pipe for writing */
		close(fd[1]);

		/* wait for child to exit */
		waitpid(pid,&status,0);

		/* get the end time for running the command */
		gettimeofday(&end_time,NULL);

		/* return execution time in milliseconds */
		*exectime=(double)((double)(end_time.tv_sec-start_time.tv_sec)+(double)((end_time.tv_usec-start_time.tv_usec)/1000)/1000.0);
#ifdef REMOVED_050803
		*exectime=(double)((double)(end_time.time-start_time.time)+(double)((end_time.millitm-start_time.millitm)/1000.0));
#endif

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
		do{
			bytes_read=read(fd[0],buffer,sizeof(buffer)-1);
	                }while(bytes_read==-1 && errno==EINTR);
		buffer[sizeof(buffer)-1]='\x0';
		if(output!=NULL){
			strncpy(output,buffer,output_length);
			output[output_length-1]='\x0';
		        }

		if(bytes_read==-1 && output!=NULL)
			strcpy(output,"");

		/* if there was a critical return code and no output AND the command time exceeded the timeout thresholds, assume a timeout */
		if(result==STATE_CRITICAL && bytes_read==-1 && (end_time.tv_sec-start_time.tv_sec)>=timeout){

			/* set the early timeout flag */
			*early_timeout=TRUE;

			/* try to kill the command that timed out by sending termination signal to child process group */
			kill((pid_t)(-pid),SIGTERM);
			sleep(1);
			kill((pid_t)(-pid),SIGKILL);
		        }

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_system_command(NEBTYPE_SYSTEM_COMMAND_END,NEBFLAG_NONE,NEBATTR_NONE,start_time,end_time,*exectime,timeout,*early_timeout,result,cmd,output,NULL);
#endif

		/* close the pipe for reading */
		close(fd[0]);
	        }

#ifdef DEBUG1
	printf("my_system() end\n");
#endif

	return result;
        }



/* given a "raw" command, return the "expanded" or "whole" command line */
void get_raw_command_line(char *cmd, char *raw_command, int buffer_length, int macro_options){
	char temp_arg[MAX_COMMAND_BUFFER];
	char arg_buffer[MAX_COMMAND_BUFFER];
	command *temp_command;
	int x,y;
	int arg_index;

#ifdef DEBUG0
	printf("get_raw_command_line() start\n");
#endif
#ifdef DEBUG1
	printf("\tInput: %s\n",cmd);
#endif

	/* clear the argv macros */
	clear_argv_macros();

	/* make sure we've got all the requirements */
	if(cmd==NULL || raw_command==NULL || buffer_length<=0){
#ifdef DEBUG1
		printf("\tWe don't have enough data to get the raw command line!\n");
#endif
		return;
	        }

	/* initialize the command */
	strcpy(raw_command,"");

	/* clear the old command arguments */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){
		if(macro_argv[x]!=NULL)
			free(macro_argv[x]);
		macro_argv[x]=NULL;
	        }

	/* lookup the command... */

	/* get the command name */
	for(x=0,y=0;y<buffer_length-1;x++){
		if(cmd[x]=='!' || cmd[x]=='\x0')
			break;
		raw_command[y]=cmd[x];
		y++;
	        }
	raw_command[y]='\x0';

	/* find the command used to check this service */
	temp_command=find_command(raw_command);

	/* error if we couldn't find the command */
	if(temp_command==NULL)
		return;

	strncpy(raw_command,temp_command->command_line,buffer_length);
	raw_command[buffer_length-1]='\x0';
	strip(raw_command);

	/* skip the command name (we're about to get the arguments)... */
	for(arg_index=0;;arg_index++){
		if(cmd[arg_index]=='!' || cmd[arg_index]=='\x0')
			break;
	        }

	/* get the command arguments */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){

		/* we reached the end of the arguments... */
		if(cmd[arg_index]=='\x0')
			break;

		/* get the next argument */
		/* can't use strtok(), as that's used in process_macros... */
		for(arg_index++,y=0;y<sizeof(temp_arg)-1;arg_index++){
			if(cmd[arg_index]=='!' || cmd[arg_index]=='\x0')
				break;
			temp_arg[y]=cmd[arg_index];
			y++;
		        }
		temp_arg[y]='\x0';

		/* ADDED 01/29/04 EG */
		/* process any macros we find in the argument */
		process_macros(temp_arg,arg_buffer,sizeof(arg_buffer),macro_options);

		strip(arg_buffer);
		macro_argv[x]=strdup(arg_buffer);
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
	temp_period=find_timeperiod(period_name);
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
		temp_period=find_timeperiod(period_name);
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

#ifdef HAVE_TM_ZONE
# define tzone tm_ptr->tm_zone
#else
# define tzone (tm_ptr->tm_isdst)?tzname[1]:tzname[0]
#endif

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

	/* don't mess up other functions that might want to call a variable 'tzone' */
#undef tzone

	return;
        }



/* get the next time to schedule a log rotation */
time_t get_next_log_rotation_time(void){
	time_t current_time;
	struct tm *t;
	int is_dst_now=FALSE;
	time_t run_time;

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
		run_time=mktime(t);
		break;
	case LOG_ROTATION_DAILY:
		t->tm_mday++;
		t->tm_hour=0;
		run_time=mktime(t);
		break;
	case LOG_ROTATION_WEEKLY:
		t->tm_mday+=(7-t->tm_wday);
		t->tm_hour=0;
		run_time=mktime(t);
		break;
	case LOG_ROTATION_MONTHLY:
	default:
		t->tm_mon++;
		t->tm_mday=1;
		t->tm_hour=0;
		run_time=mktime(t);
		break;
	        }

	if(is_dst_now==TRUE && t->tm_isdst==0)
		run_time+=3600;
	else if(is_dst_now==FALSE && t->tm_isdst>0)
		run_time-=3600;

#ifdef DEBUG1
	printf("\tNext Log Rotation Time: %s",ctime(&run_time));
#endif

#ifdef DEBUG0
	printf("get_next_log_rotation_time() end\n");
#endif

	return run_time;
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
	signal(SIGPIPE,SIG_IGN);
	signal(SIGQUIT,sighandler);
	signal(SIGTERM,sighandler);
	signal(SIGHUP,sighandler);
#if !defined(DEBUG0) && !defined(DEBUG1) && !defined(DEBUG2) && !defined(DEBUG3) && !defined(DEBUG4) && !defined(DEBUG5)
	if(daemon_dumps_core==FALSE || daemon_mode==FALSE)
		signal(SIGSEGV,sighandler);
#endif

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
	signal(SIGPIPE,SIG_DFL);

#ifdef DEBUG0
	printf("reset_sighandler() end\n");
#endif

	return;
        }


/* handle signals */
void sighandler(int sig){
	char buffer[MAX_INPUT_BUFFER];
	int x=0;

	/* if shutdown is already true, we're in a signal trap loop! */
	/* changed 09/07/06 to only exit on segfaults */
	if(sigshutdown==TRUE && sig==SIGSEGV)
		exit(ERROR);

	caught_signal=TRUE;

	if(sig<0)
		sig=-sig;

	for(x=0;sigs[x]!=(char *)NULL;x++);
	sig%=x;

	sig_id=sig;

	/* log errors about segfaults now, as we might not get a chance to later */
	/* all other signals are logged at a later point in main() to prevent problems with NPTL */
	if(sig==SIGSEGV){
		snprintf(buffer,sizeof(buffer),"Caught SIG%s, shutting down...\n",sigs[sig]);
		buffer[sizeof(buffer)-1]='\x0';

#ifdef DEBUG2
		printf("%s\n",buffer);
#endif

		write_to_all_logs(buffer,NSLOG_PROCESS_INFO);
		}

	/* we received a SIGHUP, so restart... */
	if(sig==SIGHUP)
		sigrestart=TRUE;

	/* else begin shutting down... */
	else if(sig<16)
		sigshutdown=TRUE;

	return;
        }


/* handle timeouts when executing service checks */
void service_check_sighandler(int sig){
	struct timeval end_time;

	/* get the current time */
	gettimeofday(&end_time,NULL);

	/* write plugin check results to message queue */
	strncpy(svc_msg.output,"(Service Check Timed Out)",sizeof(svc_msg.output)-1);
	svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
#ifdef SERVICE_CHECK_TIMEOUTS_RETURN_UNKNOWN
	svc_msg.return_code=STATE_UNKNOWN;
#else
	svc_msg.return_code=STATE_CRITICAL;
#endif
	svc_msg.exited_ok=TRUE;
	svc_msg.check_type=SERVICE_CHECK_ACTIVE;
	svc_msg.finish_time=end_time;
	svc_msg.early_timeout=TRUE;
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
	char *homedir=NULL;

#ifdef RLIMIT_CORE
	struct rlimit limit;
#endif

	/* change working directory. scuttle home if we're dumping core */
	homedir=getenv("HOME");
	if(daemon_dumps_core==TRUE && homedir!=NULL)
		chdir(homedir);
	else
		chdir("/");

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
	else if((int)pid!=0)
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
		if(errno==EACCES || errno==EAGAIN){
			fcntl(lockfile,F_GETLK,&lock);
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Lockfile '%s' is held by PID %d.  Bailing out...",lock_file,(int)lock.l_pid);
		        }
		else
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Cannot lock lockfile '%s': %s. Bailing out...",lock_file,strerror(errno));
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
		cleanup();
		exit(ERROR);
	        }

	/* prevent daemon from dumping a core file... */
#ifdef RLIMIT_CORE
	if(daemon_dumps_core==FALSE){
		getrlimit(RLIMIT_CORE,&limit);
		limit.rlim_cur=0;
		setrlimit(RLIMIT_CORE,&limit);
	        }
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

#ifdef USE_EVENT_BROKER
	/* send program data to broker */
	broker_program_state(NEBTYPE_PROCESS_DAEMONIZE,NEBFLAG_NONE,NEBATTR_NONE,NULL);
#endif

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
	int result=OK;

#ifdef DEBUG0
	printf("drop_privileges() start\n");
#endif

#ifdef DEBUG1
	printf("Original UID/GID: %d/%d\n",(int)getuid(),(int)getgid());
#endif

	/* only drop privileges if we're running as root, so we don't interfere with being debugged while running as some random user */
	if(getuid()!=0)
		return OK;

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
				result=ERROR;
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
			result=ERROR;
		        }
	        }

#ifdef DEBUG1
	printf("New UID/GID: %d/%d\n",(int)getuid(),(int)getgid());
#endif

#ifdef DEBUG0
	printf("drop_privileges() end\n");
#endif

	return result;
        }




/******************************************************************/
/************************* IPC FUNCTIONS **************************/
/******************************************************************/



/* reads a service message from the circular buffer */
int read_svc_message(service_message *message){
	char buffer[MAX_INPUT_BUFFER];
	int result;

#ifdef DEBUG0
	printf("read_svc_message() start\n");
#endif

	if(message==NULL)
		return -1;

	/* clear the message buffer */
	memset((void *)message,0,sizeof(service_message));

	/* get a lock on the buffer */
	pthread_mutex_lock(&service_result_buffer.buffer_lock);

	/* handle detected overflows */
	if(service_result_buffer.overflow>0){

		/* log the warning */
		snprintf(buffer,sizeof(buffer)-1,"Warning: Overflow detected in service check result buffer - %lu message(s) lost.\n",service_result_buffer.overflow);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);

		/* reset overflow counter */
		service_result_buffer.overflow=0;
	        }

	/* there are no items in the buffer */
	if(service_result_buffer.items==0)
		result=-1;

	/* return the message from the tail of the buffer */
	else{
		
		/* copy message to user-supplied structure */
		memcpy(message,((service_message **)service_result_buffer.buffer)[service_result_buffer.tail],sizeof(service_message));

		/* free memory allocated for buffer slot */
		free(((service_message **)service_result_buffer.buffer)[service_result_buffer.tail]);
		service_result_buffer.buffer[service_result_buffer.tail]=NULL;

		/* adjust tail counter and number of items */
		service_result_buffer.tail=(service_result_buffer.tail + 1) % check_result_buffer_slots;
		service_result_buffer.items--;

		result=0;
	        }

	/* release the lock on the buffer */
	pthread_mutex_unlock(&service_result_buffer.buffer_lock);

#ifdef DEBUG0
	printf("read_svc_message() end\n");
#endif

	return result;
	}



/* writes a service message to the message pipe */
int write_svc_message(service_message *message){
	struct timeval tv;
	int write_result;

#ifdef DEBUG0
	printf("write_svc_message() start\n");
#endif

	if(message==NULL)
		return 0;

	while(1){

		write_result=write(ipc_pipe[1],message,sizeof(service_message));

		if(write_result==-1){

			/* pipe is full - wait a bit and retry */
			if(errno==EAGAIN){
				tv.tv_sec=0;
				tv.tv_usec=250;
				select(0,NULL,NULL,NULL,&tv);
				continue;
			        }

			/* an interrupt occurred - retry */
			if(errno==EINTR)
				continue;

			/* some other error occurred - bail out */
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
	struct stat st;
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

	/* use existing FIFO if possible */
	if(!(stat(command_file,&st)!=-1 && (st.st_mode & S_IFIFO))){

		/* create the external command file as a named pipe (FIFO) */
		if((result=mkfifo(command_file,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))!=0){

			snprintf(buffer,sizeof(buffer)-1,"Error: Could not create external command file '%s' as named pipe: (%d) -> %s.  If this file already exists and you are sure that another copy of Nagios is not running, you should delete this file.\n",command_file,errno,strerror(errno));
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

			return ERROR;
		        }
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

	/* initialize worker thread */
	if(init_command_file_worker_thread()==ERROR){

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not initialize command file worker thread.\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		/* close the command file */
		fclose(command_file_fp);
	
		/* delete the named pipe */
		unlink(command_file);

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

	/* the command file wasn't created or was already cleaned up */
	if(command_file_created==FALSE)
		return OK;

	/* reset our flag */
	command_file_created=FALSE;

	/* shutdown the worker thread */
	shutdown_command_file_worker_thread();

	/* close the command file */
	fclose(command_file_fp);
	
	/* delete the named pipe */
	/*
	if(unlink(command_file)!=0)
		return ERROR;
	*/

#ifdef DEBUG0
	printf("close_command_file() end\n");
#endif

	return OK;
        }




/******************************************************************/
/************************ STRING FUNCTIONS ************************/
/******************************************************************/

/* strip newline, carriage return, and tab characters from beginning and end of a string */
void strip(char *buffer){
	register int x;
	register int y;
	register int z;

	if(buffer==NULL || buffer[0]=='\x0')
		return;

	/* strip end of string */
	y=(int)strlen(buffer);
	for(x=y-1;x>=0;x--){
		if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
			buffer[x]='\x0';
		else
			break;
	        }

	/* strip beginning of string (by shifting) */
	y=(int)strlen(buffer);
	for(x=0;x<y;x++){
		if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
			continue;
		else
			break;
	        }
	if(x>0){
		for(z=x;z<y;z++)
			buffer[z-x]=buffer[z];
		buffer[y-x]='\x0';
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

		/* REMOVED 3/11/05 to allow for non-english spellings, etc. */
		/* illegal extended ASCII characters */
		/*
		if(ch>=166)
			return TRUE;
		*/

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

			/* REMOVED 3/11/05 to allow for non-english spellings, etc. */
			/* illegal extended ASCII characters */
			/*
			if(ch>=166)
				continue;
			*/

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
	
	token_position=strchr(my_strtok_buffer,tokens[0]);

	if(token_position==NULL){
		my_strtok_buffer=strchr(my_strtok_buffer,'\x0');
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


/* encodes a string in proper URL format */
char *get_url_encoded_string(char *input){
	register int x,y;
	char *encoded_url_string=NULL;
	char temp_expansion[4];


	/* bail if no input */
	if(input==NULL)
		return NULL;

	/* allocate enough memory to escape all characters if necessary */
	if((encoded_url_string=(char *)malloc((strlen(input)*3)+1))==NULL)
		return NULL;

	/* check/encode all characters */
	for(x=0,y=0;input[x]!=(char)'\x0';x++){

		/* alpha-numeric characters and a few other characters don't get encoded */
		if(((char)input[x]>='0' && (char)input[x]<='9') || ((char)input[x]>='A' && (char)input[x]<='Z') || ((char)input[x]>=(char)'a' && (char)input[x]<=(char)'z') || (char)input[x]==(char)'.' || (char)input[x]==(char)'-' || (char)input[x]==(char)'_'){
			encoded_url_string[y]=input[x];
			y++;
		        }

		/* spaces are pluses */
		else if((char)input[x]<=(char)' '){
			encoded_url_string[y]='+';
			y++;
		        }

		/* anything else gets represented by its hex value */
		else{
			encoded_url_string[y]='\x0';
			sprintf(temp_expansion,"%%%02X",(unsigned int)input[x]);
			strcat(encoded_url_string,temp_expansion);
			y+=3;
		        }
	        }

	/* terminate encoded string */
	encoded_url_string[y]='\x0';

	return encoded_url_string;
        }



/******************************************************************/
/************************* HASH FUNCTIONS *************************/
/******************************************************************/

/* single hash function */
int hashfunc1(const char *name1,int hashslots){
	unsigned int i,result;

	result=0;

	if(name1)
		for(i=0;i<strlen(name1);i++)
			result+=name1[i];

	result=result%hashslots;

	return result;
        }


/* dual hash function */
int hashfunc2(const char *name1,const char *name2,int hashslots){
	unsigned int i,result;

	result=0;
	if(name1)
		for(i=0;i<strlen(name1);i++)
			result+=name1[i];

	if(name2)
		for(i=0;i<strlen(name2);i++)
			result+=name2[i];

	result=result%hashslots;

	return result;
        }


/* single hash data comparison */
int compare_hashdata1(const char *val1, const char *val2){
	
	return strcmp(val1,val2);
        }


/* dual hash data comparison */
int compare_hashdata2(const char *val1a, const char *val1b, const char *val2a, const char *val2b){
	int result;

	result=strcmp(val1a,val2a);
	if(result>0)
		return 1;
	else if(result<0)
		return -1;
	else
		return strcmp(val1b,val2b);
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

				/* reset result since we successfully copied file */
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


/* open a file read-only via mmap() */
mmapfile *mmap_fopen(char *filename){
	mmapfile *new_mmapfile;
	int fd;
	void *mmap_buf;
	struct stat statbuf;
	int mode=O_RDONLY;

	/* allocate memory */
	if((new_mmapfile=(mmapfile *)malloc(sizeof(mmapfile)))==NULL)
		return NULL;

	/* open the file */
	if((fd=open(filename,mode))==-1){
		free(new_mmapfile);
		return NULL;
	        }

	/* get file info */
	if((fstat(fd,&statbuf))==-1){
		close(fd);
		free(new_mmapfile);
		return NULL;
	        }

	/* mmap() the file  - allocate one extra byte for processing zero-byte files */
	if((mmap_buf=(void *)mmap(0,statbuf.st_size+1,PROT_READ,MAP_PRIVATE,fd,0))==MAP_FAILED){
		close(fd);
		free(new_mmapfile);
		return NULL;
	        }

	/* populate struct info for later use */
	/*new_mmapfile->path=strdup(filename);*/
	new_mmapfile->path=NULL;
	new_mmapfile->fd=fd;
	new_mmapfile->file_size=(unsigned long)(statbuf.st_size);
	new_mmapfile->current_position=0L;
	new_mmapfile->current_line=0L;
	new_mmapfile->mmap_buf=mmap_buf;

	return new_mmapfile;
        }


/* close a file originally opened via mmap() */
int mmap_fclose(mmapfile *temp_mmapfile){

	if(temp_mmapfile==NULL)
		return ERROR;

	/* un-mmap() the file */
	munmap(temp_mmapfile->mmap_buf,temp_mmapfile->file_size);

	/* close the file */
	close(temp_mmapfile->fd);

	/* free memory */
	if(temp_mmapfile->path!=NULL)
		free(temp_mmapfile->path);
	free(temp_mmapfile);
	
	return OK;
        }


/* gets one line of input from an mmap()'ed file */
char *mmap_fgets(mmapfile *temp_mmapfile){
	char *buf=NULL;
	unsigned long x=0L;
	int len=0;

	if(temp_mmapfile==NULL)
		return NULL;

	/* we've reached the end of the file */
	if(temp_mmapfile->current_position>=temp_mmapfile->file_size)
		return NULL;

	/* find the end of the string (or buffer) */
	for(x=temp_mmapfile->current_position;x<temp_mmapfile->file_size;x++){
		if(*((char *)(temp_mmapfile->mmap_buf)+x)=='\n'){
			x++;
			break;
			}
	        }

	/* calculate length of line we just read */
	len=(int)(x-temp_mmapfile->current_position);

	/* allocate memory for the new line */
	if((buf=(char *)malloc(len+1))==NULL)
		return NULL;

	/* copy string to newly allocated memory and terminate the string */
	memcpy(buf,((char *)(temp_mmapfile->mmap_buf)+temp_mmapfile->current_position),len);
	buf[len]='\x0';

	/* update the current position */
	temp_mmapfile->current_position=x;

	/* increment the current line */
	temp_mmapfile->current_line++;

	return buf;
        }



/* gets one line of input from an mmap()'ed file (may be contained on more than one line in the source file) */
char *mmap_fgets_multiline(mmapfile *temp_mmapfile){
	char *buf=NULL;
	char *tempbuf=NULL;
	int len;
	int len2;

	if(temp_mmapfile==NULL)
		return NULL;

	while(1){

		free(tempbuf);

		if((tempbuf=mmap_fgets(temp_mmapfile))==NULL)
			break;

		if(buf==NULL){
			len=strlen(tempbuf);
			if((buf=(char *)malloc(len+1))==NULL)
				break;
			memcpy(buf,tempbuf,len);
			buf[len]='\x0';
		        }
		else{
			len=strlen(tempbuf);
			len2=strlen(buf);
			if((buf=(char *)realloc(buf,len+len2+1))==NULL)
				break;
			strcat(buf,tempbuf);
			len+=len2;
			buf[len]='\x0';
		        }

		/* we shouldn't continue to the next line... */
		if(!(len>0 && buf[len-1]=='\\' && (len==1 || buf[len-2]!='\\')))
			break;
	        }

	free(tempbuf);

	return buf;
        }



/******************************************************************/
/******************** EMBEDDED PERL FUNCTIONS *********************/
/******************************************************************/

/* initializes embedded perl interpreter */
int init_embedded_perl(char **env){
#ifdef EMBEDDEDPERL
	char *embedding[] = { "", "" };
	int exitstatus = 0;
	char buffer[MAX_INPUT_BUFFER];
	int argc = 2;
	struct stat stat_buf;

	/* make sure the P1 file exists... */
	if(p1_file==NULL || stat(p1_file,&stat_buf)!=0){

		use_embedded_perl=FALSE;

		snprintf(buffer,sizeof(buffer),"Error: p1.pl file required for embedded Perl interpreter is missing!\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
		}

	else{

		embedding[1]=p1_file;

		use_embedded_perl=TRUE;

		PERL_SYS_INIT3(&argc,&embedding,&env);

		if((my_perl=perl_alloc())==NULL){
			use_embedded_perl=FALSE;
			snprintf(buffer,sizeof(buffer),"Error: Could not allocate memory for embedded Perl interpreter!\n");
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
			}
		}

	/* a fatal error occurred... */
	if(use_embedded_perl==FALSE){

		snprintf(buffer,sizeof(buffer),"Bailing out due to errors encountered while initializing the embedded Perl interpreter. (PID=%d)\n",(int)getpid());
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR,TRUE);

		cleanup();
		exit(ERROR);
		}

	perl_construct(my_perl);
	exitstatus=perl_parse(my_perl,xs_init,2,embedding,env);
	if(!exitstatus)
		exitstatus=perl_run(my_perl);

#endif
	return OK;
        }


/* closes embedded perl interpreter */
int deinit_embedded_perl(void){
#ifdef EMBEDDEDPERL

	PL_perl_destruct_level=0;
	perl_destruct(my_perl);
	perl_free(my_perl);
	PERL_SYS_TERM();

#endif
	return OK;
        }




/******************************************************************/
/************************ THREAD FUNCTIONS ************************/
/******************************************************************/

/* initializes service result worker thread */
int init_service_result_worker_thread(void){
	int result;
	sigset_t newmask;

	/* initialize circular buffer */
	service_result_buffer.head=0;
	service_result_buffer.tail=0;
	service_result_buffer.items=0;
	service_result_buffer.high=0;
	service_result_buffer.overflow=0L;
	service_result_buffer.buffer=(void **)malloc(check_result_buffer_slots*sizeof(service_message **));
	if(service_result_buffer.buffer==NULL)
		return ERROR;

	/* initialize mutex */
	pthread_mutex_init(&service_result_buffer.buffer_lock,NULL);

	/* new thread should block all signals */
	sigfillset(&newmask);
	pthread_sigmask(SIG_BLOCK,&newmask,NULL);

	/* create worker thread */
	result=pthread_create(&worker_threads[SERVICE_WORKER_THREAD],NULL,service_result_worker_thread,NULL);

	/* main thread should unblock all signals */
	pthread_sigmask(SIG_UNBLOCK,&newmask,NULL);

#ifdef DEBUG1
	printf("SERVICE CHECK THREAD: %lu\n",(unsigned long)worker_threads[SERVICE_WORKER_THREAD]);
#endif

	if(result)
		return ERROR;

	return OK;
        }


/* shutdown the service result worker thread */
int shutdown_service_result_worker_thread(void){

	/* tell the worker thread to exit */
	pthread_cancel(worker_threads[SERVICE_WORKER_THREAD]);

	/* wait for the worker thread to exit */
	pthread_join(worker_threads[SERVICE_WORKER_THREAD],NULL);

	return OK;
        }


/* clean up resources used by service result worker thread */
void cleanup_service_result_worker_thread(void *arg){
	int x;

	/* release memory allocated to circular buffer */
	for(x=service_result_buffer.tail;x!=service_result_buffer.head;x=(x+1) % check_result_buffer_slots){
		free(((service_message **)service_result_buffer.buffer)[x]);
		((service_message **)service_result_buffer.buffer)[x]=NULL;
	        }
	free(service_result_buffer.buffer);

	return;
        }


/* initializes command file worker thread */
int init_command_file_worker_thread(void){
	int result;
	sigset_t newmask;

	/* initialize circular buffer */
	external_command_buffer.head=0;
	external_command_buffer.tail=0;
	external_command_buffer.items=0;
	external_command_buffer.high=0;
	external_command_buffer.overflow=0L;
	external_command_buffer.buffer=(void **)malloc(external_command_buffer_slots*sizeof(char **));
	if(external_command_buffer.buffer==NULL)
		return ERROR;

	/* initialize mutex */
	pthread_mutex_init(&external_command_buffer.buffer_lock,NULL);

	/* new thread should block all signals */
	sigfillset(&newmask);
	pthread_sigmask(SIG_BLOCK,&newmask,NULL);

	/* create worker thread */
	result=pthread_create(&worker_threads[COMMAND_WORKER_THREAD],NULL,command_file_worker_thread,NULL);

	/* main thread should unblock all signals */
	pthread_sigmask(SIG_UNBLOCK,&newmask,NULL);

#ifdef DEBUG1
	printf("COMMAND FILE THREAD: %lu\n",(unsigned long)worker_threads[COMMAND_WORKER_THREAD]);
#endif

	if(result)
		return ERROR;

	return OK;
        }


/* shutdown command file worker thread */
int shutdown_command_file_worker_thread(void){

	/* tell the worker thread to exit */
	pthread_cancel(worker_threads[COMMAND_WORKER_THREAD]);

	/* wait for the worker thread to exit */
	pthread_join(worker_threads[COMMAND_WORKER_THREAD],NULL);

	return OK;
        }


/* clean up resources used by command file worker thread */
void cleanup_command_file_worker_thread(void *arg){
	int x;

	/* release memory allocated to circular buffer */
	for(x=external_command_buffer.tail;x!=external_command_buffer.head;x=(x+1) % external_command_buffer_slots){
		free(((char **)external_command_buffer.buffer)[x]);
		((char **)external_command_buffer.buffer)[x]=NULL;
	        }
	free(external_command_buffer.buffer);

	return;
        }


/* service worker thread - artificially increases buffer of IPC pipe */
void * service_result_worker_thread(void *arg){	
	struct pollfd pfd;
	int pollval;
	struct timeval tv;
	int read_result;
	int bytes_to_read;
	int write_offset;
	int buffer_items;
	service_message message;
	service_message *new_message;

	/* specify cleanup routine */
	pthread_cleanup_push(cleanup_service_result_worker_thread,NULL);

	/* set cancellation info */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);

	while(1){

		/* should we shutdown? */
		pthread_testcancel();

		/* wait for data to arrive */
		/* select seems to not work, so we have to use poll instead */
		pfd.fd=ipc_pipe[0];
		pfd.events=POLLIN;
		pollval=poll(&pfd,1,500);

		/* loop if no data */
		if(pollval==0)
			continue;

		/* check for errors */
		if(pollval==-1){

			switch(errno){
			case EBADF:
				write_to_log("service_result_worker_thread(): poll(): EBADF",logging_options,NULL);
				break;
			case ENOMEM:
				write_to_log("service_result_worker_thread(): poll(): ENOMEM",logging_options,NULL);
				break;
			case EFAULT:
				write_to_log("service_result_worker_thread(): poll(): EFAULT",logging_options,NULL);
				break;
			case EINTR:
				/* this can happen when running under a debugger like gdb */
				/*
				write_to_log("service_result_worker_thread(): poll(): EINTR (impossible)",logging_options,NULL);
				*/
				break;
			default:
				write_to_log("service_result_worker_thread(): poll(): Unknown errno value.",logging_options,NULL);
				break;
			        }

			continue;
		        }

		/* should we shutdown? */
		pthread_testcancel();

		/* get number of items in the buffer */
		pthread_mutex_lock(&service_result_buffer.buffer_lock);
		buffer_items=service_result_buffer.items;
		pthread_mutex_unlock(&service_result_buffer.buffer_lock);

		/* process data in the pipe (one message max) if there's some free space in the circular buffer */
		if(buffer_items<check_result_buffer_slots){

			/* clear the message buffer */
			memset((void *)&message,0,sizeof(service_message));

			/* initialize the number of bytes to read */
			bytes_to_read=sizeof(service_message);

			/* read a single message from the pipe, taking into consideration that we may have had an interrupt */
			while(1){

				write_offset=sizeof(service_message)-bytes_to_read;

				/* try and read a (full or partial) message */
				read_result=read(ipc_pipe[0],((char *)&message)+write_offset,bytes_to_read);

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

				/* did we read fewer bytes than we were supposed to?  if so, try and get the rest of the message... */
				else if(read_result<bytes_to_read){

					bytes_to_read-=read_result;

					/* should we shutdown? */
					pthread_testcancel();

					continue;
				        }

				/* we read the entire message... */
				else
					break;
			        }

			/* the read was good, so save it */
			if(read_result>=0){
				
				/* allocate memory for the message */
				new_message=(service_message *)malloc(sizeof(service_message));
				if(new_message==NULL)
					break;

				/* copy the data we read to the message buffer */
				memcpy(new_message,&message,sizeof(service_message));

				/* obtain a lock for writing to the buffer */
				pthread_mutex_lock(&service_result_buffer.buffer_lock);

				/* handle overflow conditions */
				/* NOTE: This should never happen (see check above) - child processes will instead block trying to write messages to the pipe... */
				if(service_result_buffer.items==check_result_buffer_slots){

					/* record overflow */
					service_result_buffer.overflow++;

					/* update tail pointer */
					service_result_buffer.tail=(service_result_buffer.tail + 1) % check_result_buffer_slots;
				        }

				/* save the data to the buffer */
				((service_message **)service_result_buffer.buffer)[service_result_buffer.head]=new_message;

				/* increment the head counter and items */
				service_result_buffer.head=(service_result_buffer.head + 1) % check_result_buffer_slots;
				if(service_result_buffer.items<check_result_buffer_slots)
					service_result_buffer.items++;
				if(service_result_buffer.items>service_result_buffer.high)
					service_result_buffer.high=service_result_buffer.items;

				/* release lock on buffer */
				pthread_mutex_unlock(&service_result_buffer.buffer_lock);
			        }
                        }

		/* no free space in the buffer, so wait a bit */
		else{
			tv.tv_sec=0;
			tv.tv_usec=500;
			select(0,NULL,NULL,NULL,&tv);
		        }
	        }

	/* removes cleanup handler - should never be reached */
	pthread_cleanup_pop(0);

	return NULL;
        }



/* worker thread - artificially increases buffer of named pipe */
void * command_file_worker_thread(void *arg){
	char input_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;
	int buffer_items;
	int result;

	/* specify cleanup routine */
	pthread_cleanup_push(cleanup_command_file_worker_thread,NULL);

	/* set cancellation info */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);

	while(1){

		/* should we shutdown? */
		pthread_testcancel();

		/**** POLL() AND SELECT() DON'T SEEM TO WORK ****/
		/* wait a bit */
		tv.tv_sec=0;
		tv.tv_usec=500000;
		select(0,NULL,NULL,NULL,&tv);

		/* should we shutdown? */
		pthread_testcancel();

		/* get number of items in the buffer */
		pthread_mutex_lock(&external_command_buffer.buffer_lock);
		buffer_items=external_command_buffer.items;
		pthread_mutex_unlock(&external_command_buffer.buffer_lock);

#ifdef DEBUG_CFWT
		printf("(CFWT) BUFFER ITEMS: %d/%d\n",buffer_items,external_command_buffer_slots);
#endif

		/* process all commands in the file (named pipe) if there's some space in the buffer */
		if(buffer_items<external_command_buffer_slots){

			/* clear EOF condition from prior run (FreeBSD fix) */
			clearerr(command_file_fp);

			/* read and process the next command in the file */
			while(fgets(input_buffer,(int)(sizeof(input_buffer)-1),command_file_fp)!=NULL){

#ifdef DEBUG_CFWT
				printf("(CFWT) READ: %s",input_buffer);
#endif

				/* submit the external command for processing (retry if buffer is full) */
				while((result=submit_external_command(input_buffer,&buffer_items))==ERROR && buffer_items==external_command_buffer_slots){

					/* wait a bit */
					tv.tv_sec=0;
					tv.tv_usec=250000;
					select(0,NULL,NULL,NULL,&tv);

					/* should we shutdown? */
					pthread_testcancel();
				        }

#ifdef DEBUG_CFWT
				printf("(CFWT) RES: %d, BUFFER_ITEMS: %d/%d\n",result,buffer_items,external_command_buffer_slots);
#endif

				/* bail if the circular buffer is full */
				if(buffer_items==external_command_buffer_slots)
					break;

				/* should we shutdown? */
				pthread_testcancel();
	                        }

#ifdef REMOVED_09032003
			/* rewind file pointer (fix for FreeBSD, may already be taken care of due to clearerr() call before reading begins) */
			rewind(command_file_fp);
#endif
		        }
	        }

	/* removes cleanup handler - this should never be reached */
	pthread_cleanup_pop(0);

	return NULL;
        }



/* submits an external command for processing */
int submit_external_command(char *cmd, int *buffer_items){
	int result=OK;

	if(cmd==NULL || external_command_buffer.buffer==NULL){
		if(buffer_items!=NULL)
			*buffer_items=-1;
		return ERROR;
	        }

	/* obtain a lock for writing to the buffer */
	pthread_mutex_lock(&external_command_buffer.buffer_lock);

	if(external_command_buffer.items<external_command_buffer_slots){

		/* save the line in the buffer */
		((char **)external_command_buffer.buffer)[external_command_buffer.head]=strdup(cmd);

		/* increment the head counter and items */
		external_command_buffer.head=(external_command_buffer.head + 1) % external_command_buffer_slots;
		external_command_buffer.items++;
		if(external_command_buffer.items>external_command_buffer.high)
			external_command_buffer.high=external_command_buffer.items;
	        }

	/* buffer was full */
	else
		result=ERROR;

	/* return number of items now in buffer */
	if(buffer_items!=NULL)
		*buffer_items=external_command_buffer.items;

	/* release lock on buffer */
	pthread_mutex_unlock(&external_command_buffer.buffer_lock);

	return result;
        }



/* submits a raw external command (without timestamp) for processing */
int submit_raw_external_command(char *cmd, time_t *ts, int *buffer_items){
	char *newcmd=NULL;
	int length=0;
	int result=OK;
	time_t timestamp;

	if(cmd==NULL)
		return ERROR;

	/* allocate memory for the command string */
	length=strlen(cmd)+16;
	newcmd=(char *)malloc(length);
	if(newcmd==NULL)
		return ERROR;

	/* get the time */
	if(ts!=NULL)
		timestamp=*ts;
	else
		time(&timestamp);

	/* create the command string */
	snprintf(newcmd,length-1,"[%lu] %s",(unsigned long)timestamp,cmd);
	newcmd[length-1]='\x0';

	/* submit the command */
	result=submit_external_command(newcmd,buffer_items);

	/* free allocated memory */
	free(newcmd);

	return result;
        }



/******************************************************************/
/************************* MISC FUNCTIONS *************************/
/******************************************************************/

/* returns Nagios version */
char *get_program_version(void){

	return (char *)PROGRAM_VERSION;
        }


/* returns Nagios modification date */
char *get_program_modification_date(void){

	return (char *)PROGRAM_MODIFICATION_DATE;
        }



/******************************************************************/
/*********************** CLEANUP FUNCTIONS ************************/
/******************************************************************/

/* do some cleanup before we exit */
void cleanup(void){

#ifdef DEBUG0
	printf("cleanup() start\n");
#endif

#ifdef USE_EVENT_BROKER
	/* unload modules */
	if(test_scheduling==FALSE && verify_config==FALSE){
		neb_free_callback_list();
		neb_unload_all_modules(NEBMODULE_FORCE_UNLOAD,(sigshutdown==TRUE)?NEBMODULE_NEB_SHUTDOWN:NEBMODULE_NEB_RESTART);
		neb_free_module_list();
		neb_deinit_modules();
	        }
#endif

	/* free all allocated memory - including macros */
	free_memory();

#ifdef DEBUG0
	printf("cleanup() end\n");
#endif

	return;
	}


/* free the memory allocated to the linked lists */
void free_memory(void){
	timed_event *this_event=NULL;
	timed_event *next_event=NULL;
	int x;

#ifdef DEBUG0
	printf("free_memory() start\n");
#endif

	/* free all allocated memory for the object definitions */
	free_object_data();

	/* free memory allocated to comments */
	free_comment_data();

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

	/* free obsessive compulsive commands */
	if(ocsp_command!=NULL){
		free(ocsp_command);
		ocsp_command=NULL;
	        }
	if(ochp_command!=NULL){
		free(ochp_command);
		ochp_command=NULL;
	        }

	/* free memory associated with macros */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){
		if(macro_argv[x]!=NULL){
			free(macro_argv[x]);
			macro_argv[x]=NULL;
		        }
	        }

	for(x=0;x<MAX_USER_MACROS;x++){
		if(macro_user[x]!=NULL){
			free(macro_user[x]);
			macro_user[x]=NULL;
		        }
	        }

	for(x=0;x<MACRO_X_COUNT;x++){
		if(macro_x[x]!=NULL){
			free(macro_x[x]);
			macro_x[x]=NULL;
		        }
	        }

	free_macrox_names();


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

	/* free file/path variables */
	if(log_file!=NULL){
		free(log_file);
		log_file=NULL;
	        }
	if(temp_file!=NULL){
		free(temp_file);
		temp_file=NULL;
	        }
	if(command_file!=NULL){
		free(command_file);
		command_file=NULL;
	        }
	if(lock_file!=NULL){
		free(lock_file);
		lock_file=NULL;
	        }
	if(auth_file!=NULL){
		free(auth_file);
		auth_file=NULL;
	        }
	if(p1_file!=NULL){
		free(p1_file);
		p1_file=NULL;
	        }
	if(log_archive_path!=NULL){
		free(log_archive_path);
		log_archive_path=NULL;
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

	log_file=(char *)strdup(DEFAULT_LOG_FILE);
	temp_file=(char *)strdup(DEFAULT_TEMP_FILE);
	command_file=(char *)strdup(DEFAULT_COMMAND_FILE);
	lock_file=(char *)strdup(DEFAULT_LOCK_FILE);
	auth_file=(char *)strdup(DEFAULT_AUTH_FILE);
	p1_file=(char *)strdup(DEFAULT_P1_FILE);
	log_archive_path=(char *)strdup(DEFAULT_LOG_ARCHIVE_PATH);

	nagios_user=(char *)strdup(DEFAULT_NAGIOS_USER);
	nagios_group=(char *)strdup(DEFAULT_NAGIOS_GROUP);

	use_regexp_matches=FALSE;
	use_true_regexp_matching=FALSE;

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
	ochp_timeout=DEFAULT_OCHP_TIMEOUT;

	sleep_time=DEFAULT_SLEEP_TIME;
	interval_length=DEFAULT_INTERVAL_LENGTH;
	service_inter_check_delay_method=ICD_SMART;
	host_inter_check_delay_method=ICD_SMART;
	service_interleave_factor_method=ILF_SMART;
	max_service_check_spread=DEFAULT_SERVICE_CHECK_SPREAD;
	max_host_check_spread=DEFAULT_HOST_CHECK_SPREAD;

	use_aggressive_host_checking=DEFAULT_AGGRESSIVE_HOST_CHECKING;

	soft_state_dependencies=FALSE;

	retain_state_information=FALSE;
	retention_update_interval=DEFAULT_RETENTION_UPDATE_INTERVAL;
	use_retained_program_state=TRUE;
	use_retained_scheduling_info=FALSE;
	retention_scheduling_horizon=DEFAULT_RETENTION_SCHEDULING_HORIZON;
	modified_host_process_attributes=MODATTR_NONE;
	modified_service_process_attributes=MODATTR_NONE;

	command_check_interval=DEFAULT_COMMAND_CHECK_INTERVAL;
	service_check_reaper_interval=DEFAULT_SERVICE_REAPER_INTERVAL;
	service_freshness_check_interval=DEFAULT_FRESHNESS_CHECK_INTERVAL;
	host_freshness_check_interval=DEFAULT_FRESHNESS_CHECK_INTERVAL;
	auto_rescheduling_interval=DEFAULT_AUTO_RESCHEDULING_INTERVAL;
	auto_rescheduling_window=DEFAULT_AUTO_RESCHEDULING_WINDOW;

	check_external_commands=DEFAULT_CHECK_EXTERNAL_COMMANDS;
	check_orphaned_services=DEFAULT_CHECK_ORPHANED_SERVICES;
	check_service_freshness=DEFAULT_CHECK_SERVICE_FRESHNESS;
	check_host_freshness=DEFAULT_CHECK_HOST_FRESHNESS;
	auto_reschedule_checks=DEFAULT_AUTO_RESCHEDULE_CHECKS;

	log_rotation_method=LOG_ROTATION_NONE;

	last_command_check=0L;
	last_command_status_update=0L;
	last_log_rotation=0L;

        max_parallel_service_checks=DEFAULT_MAX_PARALLEL_SERVICE_CHECKS;
        currently_running_service_checks=0;

	enable_notifications=TRUE;
	execute_service_checks=TRUE;
	accept_passive_service_checks=TRUE;
	execute_host_checks=TRUE;
	accept_passive_service_checks=TRUE;
	enable_event_handlers=TRUE;
	obsess_over_services=FALSE;
	obsess_over_hosts=FALSE;
	enable_failure_prediction=TRUE;

	aggregate_status_updates=TRUE;
	status_update_interval=DEFAULT_STATUS_UPDATE_INTERVAL;

	event_broker_options=BROKER_NOTHING;

	time_change_threshold=DEFAULT_TIME_CHANGE_THRESHOLD;

	enable_flap_detection=DEFAULT_ENABLE_FLAP_DETECTION;
	low_service_flap_threshold=DEFAULT_LOW_SERVICE_FLAP_THRESHOLD;
	high_service_flap_threshold=DEFAULT_HIGH_SERVICE_FLAP_THRESHOLD;
	low_host_flap_threshold=DEFAULT_LOW_HOST_FLAP_THRESHOLD;
	high_host_flap_threshold=DEFAULT_HIGH_HOST_FLAP_THRESHOLD;

	process_performance_data=DEFAULT_PROCESS_PERFORMANCE_DATA;

	external_command_buffer_slots=DEFAULT_EXTERNAL_COMMAND_BUFFER_SLOTS;
	check_result_buffer_slots=DEFAULT_CHECK_RESULT_BUFFER_SLOTS;

	date_format=DATE_FORMAT_US;

	for(x=0;x<MACRO_X_COUNT;x++)
		macro_x[x]=NULL;

	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++)
		macro_argv[x]=NULL;

	for(x=0;x<MAX_USER_MACROS;x++)
		macro_user[x]=NULL;

	for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
		macro_contactaddress[x]=NULL;

	macro_ondemand=NULL;

	init_macrox_names();

	global_host_event_handler=NULL;
	global_service_event_handler=NULL;

	ocsp_command=NULL;
	ochp_command=NULL;

	/* reset umask */
	umask(S_IWGRP|S_IWOTH);

#ifdef DEBUG0
	printf("reset_variables() end\n");
#endif

	return OK;
        }
