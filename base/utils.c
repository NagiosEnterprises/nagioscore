/*****************************************************************************
 *
 * UTILS.C - Miscellaneous utility functions for Nagios
 *
 * Copyright (c) 1999-2007 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   02-04-2007
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
static PerlInterpreter *my_perl=NULL;
int use_embedded_perl=TRUE;
#endif

char            *my_strtok_buffer=NULL;
char            *original_my_strtok_buffer=NULL;

extern char	*config_file;
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
extern char     *macro_x_names[MACRO_X_COUNT];
extern char     *macro_argv[MAX_COMMAND_ARGUMENTS];
extern char     *macro_user[MAX_USER_MACROS];
extern char     *macro_contactaddress[MAX_CONTACT_ADDRESSES];
extern char     *macro_ondemand;
customvariablesmember *macro_custom_host_vars;
customvariablesmember *macro_custom_service_vars;
customvariablesmember *macro_custom_contact_vars;

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

extern int      sigshutdown;
extern int      sigrestart;

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
extern unsigned long modified_host_process_attributes;
extern unsigned long modified_service_process_attributes;
extern unsigned long retained_host_attribute_mask;
extern unsigned long retained_service_attribute_mask;
extern unsigned long retained_contact_host_attribute_mask;
extern unsigned long retained_contact_service_attribute_mask;
extern unsigned long retained_process_host_attribute_mask;
extern unsigned long retained_process_service_attribute_mask;

extern unsigned long next_comment_id;
extern unsigned long next_downtime_id;
extern unsigned long next_event_id;
extern unsigned long next_notification_id;

extern int      log_rotation_method;

extern time_t   program_start;

extern time_t   last_command_check;
extern time_t	last_command_status_update;
extern time_t   last_log_rotation;

extern int      verify_config;
extern int      test_scheduling;

extern check_result check_result_info;
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
extern timed_event      *event_list_high;
extern timed_event      *event_list_low;
extern notification     *notification_list;
extern command          *command_list;
extern timeperiod       *timeperiod_list;

extern int      command_file_fd;
extern FILE     *command_file_fp;
extern int      command_file_created;

#ifdef HAVE_TZNAME
#ifdef CYGWIN
extern char     *_tzname[2] __declspec(dllimport);
#else
extern char     *tzname[2];
#endif
#endif

extern dbuf     check_result_dbuf;

extern pthread_t       worker_threads[TOTAL_WORKER_THREADS];
extern circular_buffer external_command_buffer;
extern circular_buffer check_result_buffer;
extern circular_buffer event_broker_buffer;
extern int             external_command_buffer_slots;
extern int             check_result_buffer_slots;

extern check_stats    check_statistics[MAX_CHECK_STATS_TYPES];

/* from GNU defines errno as a macro, since it's a per-thread variable */
#ifndef errno
extern int errno;
#endif



/******************************************************************/
/************************ MACRO FUNCTIONS *************************/
/******************************************************************/

/* replace macros in notification commands with their values */
int process_macros(char *input_buffer, char *output_buffer, int buffer_length, int options){
	customvariablesmember *temp_customvariablesmember=NULL;
	char *temp_buffer=NULL;
	int in_macro=FALSE;
	int x=0;
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
						if((x>=16 && x<=19) || (x>=99 && x<=100)){
							clean_macro=TRUE;
							options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
						        }

						break;
						}
				        }

				/* we already have a macro... */
				if(found_macro_x==TRUE)
					x=0;

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

				/* custom host variable macros */
				else if(strstr(temp_buffer,"_HOST")==temp_buffer){
					selected_macro=NULL;
					for(temp_customvariablesmember=macro_custom_host_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
						if(!strcmp(temp_buffer,temp_customvariablesmember->variable_name)){
							selected_macro=temp_customvariablesmember->variable_value;
							clean_macro=TRUE;
							options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
							break;
						        }
					        }
				         }

				/* custom service variable macros */
				else if(strstr(temp_buffer,"_SERVICE")==temp_buffer){
					selected_macro=NULL;
					for(temp_customvariablesmember=macro_custom_service_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
						if(!strcmp(temp_buffer,temp_customvariablesmember->variable_name)){
							selected_macro=temp_customvariablesmember->variable_value;
							clean_macro=TRUE;
							options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
							break;
						        }
					        }
				         }

				/* custom contact variable macros */
				else if(strstr(temp_buffer,"_CONTACT")==temp_buffer){
					selected_macro=NULL;
					for(temp_customvariablesmember=macro_custom_contact_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
						if(!strcmp(temp_buffer,temp_customvariablesmember->variable_name)){
							selected_macro=temp_customvariablesmember->variable_value;
							clean_macro=TRUE;
							options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
							break;
						        }
					        }
				         }

				/* contact address macros */
				else if(strstr(temp_buffer,"CONTACTADDRESS")==temp_buffer){
					address_index=atoi(temp_buffer+14);
					if(address_index>=1 && address_index<=MAX_CONTACT_ADDRESSES)
						selected_macro=macro_contactaddress[address_index-1];
					else
						selected_macro=NULL;
				        }

				/* on-demand host macros */
				else if(strstr(temp_buffer,"HOST") && strstr(temp_buffer,":")){

					grab_on_demand_macro(temp_buffer);
					selected_macro=macro_ondemand;

					/* output/perfdata macros get cleaned */
					if(strstr(temp_buffer,"HOSTOUTPUT:")==temp_buffer || strstr(temp_buffer,"LONGHOSTOUTPUT:")==temp_buffer  || strstr(temp_buffer,"HOSTPERFDATA:")){
						clean_macro=TRUE;
						options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
					        }
				        }

				/* on-demand service macros */
				else if(strstr(temp_buffer,"SERVICE") && strstr(temp_buffer,":")){

					grab_on_demand_macro(temp_buffer);
					selected_macro=macro_ondemand;

					/* output/perfdata macros get cleaned */
					if(strstr(temp_buffer,"SERVICEOUTPUT:")==temp_buffer || strstr(temp_buffer,"LONGSERVICEOUTPUT:")==temp_buffer || strstr(temp_buffer,"SERVICEPERFDATA:")){
						clean_macro=TRUE;
						options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
					        }
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
						my_free((void **)&selected_macro);
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
	servicegroup *temp_servicegroup=NULL;
	customvariablesmember *temp_customvariablesmember=NULL;
	objectlist *temp_objectlist=NULL;
	char *customvarname=NULL;
	time_t current_time=0L;
	unsigned long duration=0L;
	int days=0;
	int hours=0;
	int minutes=0;
	int seconds=0;
	char temp_buffer[MAX_INPUT_BUFFER]="";
	char *buf1=NULL;
	char *buf2=NULL;
	
#ifdef DEBUG0
	printf("grab_service_macros() start\n");
#endif

	/* get the service description */
	my_free((void **)&macro_x[MACRO_SERVICEDESC]);
	macro_x[MACRO_SERVICEDESC]=(char *)strdup(svc->description);

	/* get the service display name */
	my_free((void **)&macro_x[MACRO_SERVICEDISPLAYNAME]);
	if(svc->display_name)
		macro_x[MACRO_SERVICEDISPLAYNAME]=(char *)strdup(svc->display_name);

	/* get the plugin output */
	my_free((void **)&macro_x[MACRO_SERVICEOUTPUT]);
	if(svc->plugin_output)
		macro_x[MACRO_SERVICEOUTPUT]=(char *)strdup(svc->plugin_output);

	/* get the long plugin output */
	my_free((void **)&macro_x[MACRO_LONGSERVICEOUTPUT]);
	if(svc->long_plugin_output)
		macro_x[MACRO_LONGSERVICEOUTPUT]=(char *)strdup(svc->long_plugin_output);

	/* get the performance data */
	my_free((void **)&macro_x[MACRO_SERVICEPERFDATA]);
	if(svc->perf_data)
		macro_x[MACRO_SERVICEPERFDATA]=(char *)strdup(svc->perf_data);

	/* get the service check command */
	my_free((void **)&macro_x[MACRO_SERVICECHECKCOMMAND]);
	if(svc->service_check_command)
		macro_x[MACRO_SERVICECHECKCOMMAND]=(char *)strdup(svc->service_check_command);

	/* grab the service check type */
	my_free((void **)&macro_x[MACRO_SERVICECHECKTYPE]);
	macro_x[MACRO_SERVICECHECKTYPE]=(char *)strdup((svc->check_type==SERVICE_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");

	/* grab the service state type macro (this is usually overridden later on) */
	my_free((void **)&macro_x[MACRO_SERVICESTATETYPE]);
	macro_x[MACRO_SERVICESTATETYPE]=(char *)strdup((svc->state_type==HARD_STATE)?"HARD":"SOFT");

	/* get the service state */
	my_free((void **)&macro_x[MACRO_SERVICESTATE]);
	if(svc->current_state==STATE_OK)
		macro_x[MACRO_SERVICESTATE]=(char *)strdup("OK");
	else if(svc->current_state==STATE_WARNING)
		macro_x[MACRO_SERVICESTATE]=(char *)strdup("WARNING");
	else if(svc->current_state==STATE_CRITICAL)
		macro_x[MACRO_SERVICESTATE]=(char *)strdup("CRITICAL");
	else
		macro_x[MACRO_SERVICESTATE]=(char *)strdup("UNKNOWN");

	/* get the service state id */
	my_free((void **)&macro_x[MACRO_SERVICESTATEID]);
	asprintf(&macro_x[MACRO_SERVICESTATEID],"%d",svc->current_state);

	/* get the current service check attempt macro */
	my_free((void **)&macro_x[MACRO_SERVICEATTEMPT]);
	asprintf(&macro_x[MACRO_SERVICEATTEMPT],"%d",svc->current_attempt);

	/* get the execution time macro */
	my_free((void **)&macro_x[MACRO_SERVICEEXECUTIONTIME]);
	asprintf(&macro_x[MACRO_SERVICEEXECUTIONTIME],"%.3f",svc->execution_time);

	/* get the latency macro */
	my_free((void **)&macro_x[MACRO_SERVICELATENCY]);
	asprintf(&macro_x[MACRO_SERVICELATENCY],"%.3f",svc->latency);

	/* get the last check time macro */
	my_free((void **)&macro_x[MACRO_LASTSERVICECHECK]);
	asprintf(&macro_x[MACRO_LASTSERVICECHECK],"%lu",(unsigned long)svc->last_check);

	/* get the last state change time macro */
	my_free((void **)&macro_x[MACRO_LASTSERVICESTATECHANGE]);
	asprintf(&macro_x[MACRO_LASTSERVICESTATECHANGE],"%lu",(unsigned long)svc->last_state_change);

	/* get the last time ok macro */
	my_free((void **)&macro_x[MACRO_LASTSERVICEOK]);
	asprintf(&macro_x[MACRO_LASTSERVICEOK],"%lu",(unsigned long)svc->last_time_ok);

	/* get the last time warning macro */
	my_free((void **)&macro_x[MACRO_LASTSERVICEWARNING]);
	asprintf(&macro_x[MACRO_LASTSERVICEWARNING],"%lu",(unsigned long)svc->last_time_warning);

	/* get the last time unknown macro */
	my_free((void **)&macro_x[MACRO_LASTSERVICEUNKNOWN]);
	asprintf(&macro_x[MACRO_LASTSERVICEUNKNOWN],"%lu",(unsigned long)svc->last_time_unknown);

	/* get the last time critical macro */
	my_free((void **)&macro_x[MACRO_LASTSERVICECRITICAL]);
	asprintf(&macro_x[MACRO_LASTSERVICECRITICAL],"%lu",(unsigned long)svc->last_time_critical);

	/* get the service downtime depth */
	my_free((void **)&macro_x[MACRO_SERVICEDOWNTIME]);
	asprintf(&macro_x[MACRO_SERVICEDOWNTIME],"%d",svc->scheduled_downtime_depth);

	/* get the percent state change */
	my_free((void **)&macro_x[MACRO_SERVICEPERCENTCHANGE]);
	asprintf(&macro_x[MACRO_SERVICEPERCENTCHANGE],"%.2f",svc->percent_state_change);

	time(&current_time);
	duration=(unsigned long)(current_time-svc->last_state_change);

	/* get the state duration in seconds */
	my_free((void **)&macro_x[MACRO_SERVICEDURATIONSEC]);
	asprintf(&macro_x[MACRO_SERVICEDURATIONSEC],"%lu",duration);

	/* get the state duration */
	my_free((void **)&macro_x[MACRO_SERVICEDURATION]);
	days=duration/86400;
	duration-=(days*86400);
	hours=duration/3600;
	duration-=(hours*3600);
	minutes=duration/60;
	duration-=(minutes*60);
	seconds=duration;
	asprintf(&macro_x[MACRO_SERVICEDURATION],"%dd %dh %dm %ds",days,hours,minutes,seconds);

	/* get the notification number macro */
	my_free((void **)&macro_x[MACRO_SERVICENOTIFICATIONNUMBER]);
	asprintf(&macro_x[MACRO_SERVICENOTIFICATIONNUMBER],"%d",svc->current_notification_number);

	/* get the notification id macro */
	my_free((void **)&macro_x[MACRO_SERVICENOTIFICATIONID]);
	asprintf(&macro_x[MACRO_SERVICENOTIFICATIONID],"%lu",svc->current_notification_id);

	/* get the event id macro */
	my_free((void **)&macro_x[MACRO_SERVICEEVENTID]);
	asprintf(&macro_x[MACRO_SERVICEEVENTID],"%lu",svc->current_event_id);

	/* get the last event id macro */
	my_free((void **)&macro_x[MACRO_LASTSERVICEEVENTID]);
	asprintf(&macro_x[MACRO_LASTSERVICEEVENTID],"%lu",svc->last_event_id);

	/* find all servicegroups this service is associated with */
	for(temp_objectlist=svc->servicegroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

		if((temp_servicegroup=(servicegroup *)temp_objectlist->object_ptr)==NULL)
			continue;

		asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_servicegroup->group_name);
		my_free((void **)&buf2);
		buf2=buf1;
		}
	my_free((void **)&macro_x[MACRO_SERVICEGROUPNAMES]);
	if(buf2){
		macro_x[MACRO_SERVICEGROUPNAMES]=(char *)strdup(buf2);
		my_free((void **)&buf2);
		}

	/* get the first/primary servicegroup name */
	my_free((void **)&macro_x[MACRO_SERVICEGROUPNAME]);
	if(svc->servicegroups_ptr)
		macro_x[MACRO_SERVICEGROUPNAME]=(char *)strdup(((servicegroup *)svc->servicegroups_ptr->object_ptr)->group_name);
	
	/* get the servicegroup alias */
	my_free((void **)&macro_x[MACRO_SERVICEGROUPALIAS]);
	if(svc->servicegroups_ptr)
		macro_x[MACRO_SERVICEGROUPALIAS]=(char *)strdup(((servicegroup *)svc->servicegroups_ptr->object_ptr)->alias);

	/* get the action url */
	my_free((void **)&macro_x[MACRO_SERVICEACTIONURL]);
	if(svc->action_url)
		macro_x[MACRO_SERVICEACTIONURL]=(char *)strdup(svc->action_url);

	/* get the notes url */
	my_free((void **)&macro_x[MACRO_SERVICENOTESURL]);
	if(svc->notes_url)
		macro_x[MACRO_SERVICENOTESURL]=(char *)strdup(svc->notes_url);

	/* get the notes */
	my_free((void **)&macro_x[MACRO_SERVICENOTES]);
	if(svc->notes)
		macro_x[MACRO_SERVICENOTES]=(char *)strdup(svc->notes);

	/* get custom variables */
	for(temp_customvariablesmember=svc->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
		asprintf(&customvarname,"_SERVICE%s",temp_customvariablesmember->variable_name);
		add_custom_variable_to_object(&macro_custom_service_vars,customvarname,temp_customvariablesmember->variable_value);
		my_free((void **)&customvarname);
	        }

	/*
	strip(macro_x[MACRO_SERVICEOUTPUT]);
	strip(macro_x[MACRO_SERVICEPERFDATA]);
	strip(macro_x[MACRO_SERVICECHECKCOMMAND]);
	strip(macro_x[MACRO_SERVICENOTES]);
	*/

	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	if(macro_x[MACRO_SERVICEACTIONURL]!=NULL){
		process_macros(macro_x[MACRO_SERVICEACTIONURL],temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
		my_free((void **)&macro_x[MACRO_SERVICEACTIONURL]);
		macro_x[MACRO_SERVICEACTIONURL]=(char *)strdup(temp_buffer);
	        }
	if(macro_x[MACRO_SERVICENOTESURL]!=NULL){
		process_macros(macro_x[MACRO_SERVICENOTESURL],temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
		my_free((void **)&macro_x[MACRO_SERVICENOTESURL]);
		macro_x[MACRO_SERVICENOTESURL]=(char *)strdup(temp_buffer);
	        }
	if(macro_x[MACRO_SERVICENOTES]!=NULL){
		process_macros(macro_x[MACRO_SERVICENOTES],temp_buffer,sizeof(temp_buffer),0);
		my_free((void **)&macro_x[MACRO_SERVICENOTES]);
		macro_x[MACRO_SERVICENOTES]=(char *)strdup(temp_buffer);
	        }

#ifdef DEBUG0
	printf("grab_service_macros() end\n");
#endif

	return OK;
        }


/* grab macros that are specific to a particular host */
int grab_host_macros(host *hst){
	hostgroup *temp_hostgroup=NULL;
	customvariablesmember *temp_customvariablesmember=NULL;
	objectlist *temp_objectlist=NULL;
	char *customvarname=NULL;
	time_t current_time=0L;
	unsigned long duration=0L;
	int days=0;
	int hours=0;
	int minutes=0;
	int seconds=0;
	char temp_buffer[MAX_INPUT_BUFFER]="";
	char *buf1=NULL;
	char *buf2=NULL;

#ifdef DEBUG0
	printf("grab_host_macros() start\n");
#endif

	/* get the host name */
	my_free((void **)&macro_x[MACRO_HOSTNAME]);
	macro_x[MACRO_HOSTNAME]=(char *)strdup(hst->name);
	
	/* get the host display name */
	my_free((void **)&macro_x[MACRO_HOSTDISPLAYNAME]);
	if(hst->display_name)
		macro_x[MACRO_HOSTDISPLAYNAME]=(char *)strdup(hst->display_name);

	/* get the host alias */
	my_free((void **)&macro_x[MACRO_HOSTALIAS]);
	macro_x[MACRO_HOSTALIAS]=(char *)strdup(hst->alias);

	/* get the host address */
	my_free((void **)&macro_x[MACRO_HOSTADDRESS]);
	macro_x[MACRO_HOSTADDRESS]=(char *)strdup(hst->address);

	/* get the host state */
	my_free((void **)&macro_x[MACRO_HOSTSTATE]);
	if(hst->current_state==HOST_DOWN)
		macro_x[MACRO_HOSTSTATE]=(char *)strdup("DOWN");
	else if(hst->current_state==HOST_UNREACHABLE)
		macro_x[MACRO_HOSTSTATE]=(char *)strdup("UNREACHABLE");
	else
		macro_x[MACRO_HOSTSTATE]=(char *)strdup("UP");

	/* get the host state id */
	my_free((void **)&macro_x[MACRO_HOSTSTATEID]);
	asprintf(&macro_x[MACRO_HOSTSTATEID],"%d",hst->current_state);

	/* grab the host check type */
	my_free((void **)&macro_x[MACRO_HOSTCHECKTYPE]);
	asprintf(&macro_x[MACRO_HOSTCHECKTYPE],"%s",(hst->check_type==HOST_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");

	/* get the host state type macro */
	my_free((void **)&macro_x[MACRO_HOSTSTATETYPE]);
	asprintf(&macro_x[MACRO_HOSTSTATETYPE],"%s",(hst->state_type==HARD_STATE)?"HARD":"SOFT");

	/* get the plugin output */
	my_free((void **)&macro_x[MACRO_HOSTOUTPUT]);
	if(hst->plugin_output)
		macro_x[MACRO_HOSTOUTPUT]=(char *)strdup(hst->plugin_output);

	/* get the long plugin output */
	my_free((void **)&macro_x[MACRO_LONGHOSTOUTPUT]);
	if(hst->long_plugin_output)
		macro_x[MACRO_LONGHOSTOUTPUT]=(char *)strdup(hst->long_plugin_output);

	/* get the performance data */
	my_free((void **)&macro_x[MACRO_HOSTPERFDATA]);
	if(hst->perf_data)
		macro_x[MACRO_HOSTPERFDATA]=(char *)strdup(hst->perf_data);

	/* get the host check command */
	my_free((void **)&macro_x[MACRO_HOSTCHECKCOMMAND]);
	if(hst->host_check_command)
		macro_x[MACRO_HOSTCHECKCOMMAND]=(char *)strdup(hst->host_check_command);

	/* get the host current attempt */
	my_free((void **)&macro_x[MACRO_HOSTATTEMPT]);
	asprintf(&macro_x[MACRO_HOSTATTEMPT],"%d",hst->current_attempt);

	/* get the host downtime depth */
	my_free((void **)&macro_x[MACRO_HOSTDOWNTIME]);
	asprintf(&macro_x[MACRO_HOSTDOWNTIME],"%d",hst->scheduled_downtime_depth);

	/* get the percent state change */
	my_free((void **)&macro_x[MACRO_HOSTPERCENTCHANGE]);
	asprintf(&macro_x[MACRO_HOSTPERCENTCHANGE],"%.2f",hst->percent_state_change);

	time(&current_time);
	duration=(unsigned long)(current_time-hst->last_state_change);

	/* get the state duration in seconds */
	my_free((void **)&macro_x[MACRO_HOSTDURATIONSEC]);
	asprintf(&macro_x[MACRO_HOSTDURATIONSEC],"%lu",duration);

	/* get the state duration */
	my_free((void **)&macro_x[MACRO_HOSTDURATION]);
	days=duration/86400;
	duration-=(days*86400);
	hours=duration/3600;
	duration-=(hours*3600);
	minutes=duration/60;
	duration-=(minutes*60);
	seconds=duration;
	asprintf(&macro_x[MACRO_HOSTDURATION],"%dd %dh %dm %ds",days,hours,minutes,seconds);

	/* get the execution time macro */
	my_free((void **)&macro_x[MACRO_HOSTEXECUTIONTIME]);
	asprintf(&macro_x[MACRO_HOSTEXECUTIONTIME],"%.3f",hst->execution_time);

	/* get the latency macro */
	my_free((void **)&macro_x[MACRO_HOSTLATENCY]);
	asprintf(&macro_x[MACRO_HOSTLATENCY],"%.3f",hst->latency);

	/* get the last check time macro */
	my_free((void **)&macro_x[MACRO_LASTHOSTCHECK]);
	asprintf(&macro_x[MACRO_LASTHOSTCHECK],"%lu",(unsigned long)hst->last_check);

	/* get the last state change time macro */
	my_free((void **)&macro_x[MACRO_LASTHOSTSTATECHANGE]);
	asprintf(&macro_x[MACRO_LASTHOSTSTATECHANGE],"%lu",(unsigned long)hst->last_state_change);

	/* get the last time up macro */
	my_free((void **)&macro_x[MACRO_LASTHOSTUP]);
	asprintf(&macro_x[MACRO_LASTHOSTUP],"%lu",(unsigned long)hst->last_time_up);

	/* get the last time down macro */
	my_free((void **)&macro_x[MACRO_LASTHOSTDOWN]);
	asprintf(&macro_x[MACRO_LASTHOSTDOWN],"%lu",(unsigned long)hst->last_time_down);

	/* get the last time unreachable macro */
	my_free((void **)&macro_x[MACRO_LASTHOSTUNREACHABLE]);
	asprintf(&macro_x[MACRO_LASTHOSTUNREACHABLE],"%lu",(unsigned long)hst->last_time_unreachable);

	/* get the notification number macro */
	my_free((void **)&macro_x[MACRO_HOSTNOTIFICATIONNUMBER]);
	asprintf(&macro_x[MACRO_HOSTNOTIFICATIONNUMBER],"%d",hst->current_notification_number);

	/* get the notification id macro */
	my_free((void **)&macro_x[MACRO_HOSTNOTIFICATIONID]);
	asprintf(&macro_x[MACRO_HOSTNOTIFICATIONID],"%lu",hst->current_notification_id);

	/* get the event id macro */
	my_free((void **)&macro_x[MACRO_HOSTEVENTID]);
	asprintf(&macro_x[MACRO_HOSTEVENTID],"%lu",hst->current_event_id);

	/* get the last event id macro */
	my_free((void **)&macro_x[MACRO_LASTHOSTEVENTID]);
	asprintf(&macro_x[MACRO_LASTHOSTEVENTID],"%lu",hst->last_event_id);

	/* find all hostgroups this host is associated with */
	for(temp_objectlist=hst->hostgroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

		if((temp_hostgroup=(hostgroup *)temp_objectlist->object_ptr)==NULL)
			continue;

		asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_hostgroup->group_name);
		my_free((void **)&buf2);
		buf2=buf1;
		}
	my_free((void **)&macro_x[MACRO_HOSTGROUPNAMES]);
	if(buf2){
		macro_x[MACRO_HOSTGROUPNAMES]=(char *)strdup(buf2);
		my_free((void **)&buf2);
		}

	/* get the hostgroup name */
	my_free((void **)&macro_x[MACRO_HOSTGROUPNAME]);
	if(hst->hostgroups_ptr)
		macro_x[MACRO_HOSTGROUPNAME]=(char *)strdup(((hostgroup *)hst->hostgroups_ptr->object_ptr)->group_name);
	
	/* get the hostgroup alias */
	my_free((void **)&macro_x[MACRO_HOSTGROUPALIAS]);
	if(hst->hostgroups_ptr)
		macro_x[MACRO_HOSTGROUPALIAS]=(char *)strdup(((hostgroup *)hst->hostgroups_ptr->object_ptr)->alias);

	/* get the action url */
	my_free((void **)&macro_x[MACRO_HOSTACTIONURL]);
	if(hst->action_url)
		macro_x[MACRO_HOSTACTIONURL]=(char *)strdup(hst->action_url);

	/* get the notes url */
	my_free((void **)&macro_x[MACRO_HOSTNOTESURL]);
	if(hst->notes_url)
		macro_x[MACRO_HOSTNOTESURL]=(char *)strdup(hst->notes_url);

	/* get the notes */
	my_free((void **)&macro_x[MACRO_HOSTNOTES]);
	if(hst->notes)
		macro_x[MACRO_HOSTNOTES]=(char *)strdup(hst->notes);

	/* get custom variables */
	for(temp_customvariablesmember=hst->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
		asprintf(&customvarname,"_HOST%s",temp_customvariablesmember->variable_name);
		add_custom_variable_to_object(&macro_custom_host_vars,customvarname,temp_customvariablesmember->variable_value);
		my_free((void **)&customvarname);
	        }

	/*
	strip(macro_x[MACRO_HOSTOUTPUT]);
	strip(macro_x[MACRO_HOSTPERFDATA]);
	strip(macro_x[MACRO_HOSTCHECKCOMMAND]);
	strip(macro_x[MACRO_HOSTNOTES]);
	*/

	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	if(macro_x[MACRO_HOSTACTIONURL]!=NULL){
		process_macros(macro_x[MACRO_HOSTACTIONURL],temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
		my_free((void **)&macro_x[MACRO_HOSTACTIONURL]);
		macro_x[MACRO_HOSTACTIONURL]=(char *)strdup(temp_buffer);
	        }
	if(macro_x[MACRO_HOSTNOTESURL]!=NULL){
		process_macros(macro_x[MACRO_HOSTNOTESURL],temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
		my_free((void **)&macro_x[MACRO_HOSTNOTESURL]);
		macro_x[MACRO_HOSTNOTESURL]=(char *)strdup(temp_buffer);
	        }
	if(macro_x[MACRO_HOSTNOTES]!=NULL){
		process_macros(macro_x[MACRO_HOSTNOTES],temp_buffer,sizeof(temp_buffer),0);
		my_free((void **)&macro_x[MACRO_HOSTNOTES]);
		macro_x[MACRO_HOSTNOTES]=(char *)strdup(temp_buffer);
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
	char result_buffer[MAX_INPUT_BUFFER]="";
	int result_buffer_len=0;
	int delimiter_len=0;
	host *temp_host=NULL;
	hostgroup *temp_hostgroup=NULL;
	hostgroupmember *temp_hostgroupmember=NULL;
	service *temp_service=NULL;
	servicegroup *temp_servicegroup=NULL;
	servicegroupmember *temp_servicegroupmember=NULL;
	char *ptr=NULL;
	char *host_name=NULL;
	int return_val=ERROR;

#ifdef DEBUG0
	printf("grab_on_demand_macro() start\n");
#endif

	/* clear the on-demand macro */
	my_free((void **)&macro_ondemand);

	/* save a copy of the macro */
	if((macro=(char *)strdup(str))==NULL)
		return ERROR;

	/* get the first argument (host name) */
	if((ptr=strchr(macro,':'))==NULL){
		my_free((void **)&macro);
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
			if((temp_hostgroup=find_hostgroup(first_arg))==NULL){
				my_free((void **)&macro);
				return ERROR;
				}

			return_val=OK;  /* start off assuming there's no error */
			result_buffer[0]='\0';
			result_buffer[sizeof(result_buffer)-1]='\0';
			result_buffer_len=0;
			delimiter_len=strlen(second_arg);

			/* process each host in the hostgroup */
			if((temp_hostgroupmember=temp_hostgroup->members)==NULL){
				macro_ondemand=(char *)strdup("");
				my_free((void **)&macro);
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
					if((temp_hostgroupmember=temp_hostgroupmember->next)==NULL)
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
					if((temp_hostgroupmember=temp_hostgroupmember->next)==NULL)
						break;
					}

				my_free((void **)&macro_ondemand);
				}

			my_free((void **)&macro_ondemand);
			macro_ondemand=(char *)strdup(result_buffer);
			}
	        }

	else if(strstr(macro,"SERVICE")){

		/* second args will either be service description or delimiter */
		if(second_arg==NULL){
			my_free((void **)&macro);
			return ERROR;
	                }

		/* if first arg (host name) is blank, it means refer to the "current" host, so look to other macros for help... */
		if(!strcmp(first_arg,""))
			host_name=macro_x[MACRO_HOSTNAME];
		else
			host_name=first_arg;

		/* process a service macro */
		temp_service=find_service(host_name,second_arg);
		if(temp_service!=NULL)
			return_val=grab_on_demand_service_macro(temp_service,macro);

		/* process a service macro containing a servicegroup */
		else{
			if((temp_servicegroup=find_servicegroup(first_arg))==NULL){
				my_free((void **)&macro);
				return ERROR;
				}

			return_val=OK;  /* start off assuming there's no error */
			result_buffer[0]='\0';
			result_buffer[sizeof(result_buffer)-1]='\0';
			result_buffer_len=0;
			delimiter_len=strlen(second_arg);

			/* process each service in the servicegroup */
			if((temp_servicegroupmember=temp_servicegroup->members)==NULL){
				macro_ondemand=(char *)strdup("");
				my_free((void **)&macro);
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
					if((temp_servicegroupmember=temp_servicegroupmember->next)==NULL)
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
					if((temp_servicegroupmember=temp_servicegroupmember->next)==NULL)
						break;
					}

				my_free((void **)&macro_ondemand);
				}

			my_free((void **)&macro_ondemand);
			macro_ondemand=(char *)strdup(result_buffer);
			}
	        }

	else
		return_val=ERROR;

	my_free((void **)&macro);

#ifdef DEBUG0
	printf("grab_on_demand_macro() end\n");
#endif

	return return_val;
        }


/* grab an on-demand host macro */
int grab_on_demand_host_macro(host *hst, char *macro){
	hostgroup *temp_hostgroup=NULL;
	customvariablesmember *temp_customvariablesmember=NULL;
	objectlist *temp_objectlist=NULL;
	char *customvarname=NULL;
	char temp_buffer[MAX_INPUT_BUFFER]="";
	char *buf1=NULL;
	char *buf2=NULL;
	time_t current_time=0L;
	unsigned long duration=0L;
	int days=0;
	int hours=0;
	int minutes=0;
	int seconds=0;

#ifdef DEBUG0
	printf("grab_on_demand_host_macro() start\n");
#endif

	if(hst==NULL || macro==NULL)
		return ERROR;

	/* initialize the macro */
	macro_ondemand=NULL;

	time(&current_time);
	duration=(unsigned long)(current_time-hst->last_state_change);

	/* get the host display name */
	if(!strcmp(macro,"HOSTDISPLAYNAME")){
		if(hst->display_name)
			macro_ondemand=(char *)strdup(hst->display_name);
	        }

	/* get the host alias */
	if(!strcmp(macro,"HOSTALIAS"))
		macro_ondemand=(char *)strdup(hst->alias);

	/* get the host address */
	else if(!strcmp(macro,"HOSTADDRESS"))
		macro_ondemand=(char *)strdup(hst->address);

	/* get the host state */
	else if(!strcmp(macro,"HOSTSTATE")){
		if(hst->current_state==HOST_DOWN)
			macro_ondemand=(char *)strdup("DOWN");
		else if(hst->current_state==HOST_UNREACHABLE)
			macro_ondemand=(char *)strdup("UNREACHABLE");
		else
			macro_ondemand=(char *)strdup("UP");
	        }

	/* get the host state id */
	else if(!strcmp(macro,"HOSTSTATEID"))
		asprintf(&macro_ondemand,"%d",hst->current_state);

	/* grab the host check type */
	else if(!strcmp(macro,"HOSTCHECKTYPE"))
		asprintf(&macro_ondemand,"%s",(hst->check_type==HOST_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");

	/* get the host state type macro */
	else if(!strcmp(macro,"HOSTSTATETYPE"))
		asprintf(&macro_ondemand,"%s",(hst->state_type==HARD_STATE)?"HARD":"SOFT");

	/* get the plugin output */
	else if(!strcmp(macro,"HOSTOUTPUT")){
		if(hst->plugin_output){
			macro_ondemand=(char *)strdup(hst->plugin_output);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* get the long plugin output */
	else if(!strcmp(macro,"LONGHOSTOUTPUT")){
		if(hst->long_plugin_output==NULL)
			macro_ondemand=NULL;
		else{
			macro_ondemand=(char *)strdup(hst->long_plugin_output);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* get the performance data */
	else if(!strcmp(macro,"HOSTPERFDATA")){
		if(hst->perf_data){
			macro_ondemand=(char *)strdup(hst->perf_data);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* get the host current attempt */
	else if(!strcmp(macro,"HOSTATTEMPT"))
		asprintf(&macro_ondemand,"%d",hst->current_attempt);

	/* get the host downtime depth */
	else if(!strcmp(macro,"HOSTDOWNTIME"))
		asprintf(&macro_ondemand,"%d",hst->scheduled_downtime_depth);

	/* get the percent state change */
	else if(!strcmp(macro,"HOSTPERCENTCHANGE"))
		asprintf(&macro_ondemand,"%.2f",hst->percent_state_change);

	/* get the state duration in seconds */
	else if(!strcmp(macro,"HOSTDURATIONSEC"))
		asprintf(&macro_ondemand,"%lu",duration);

	/* get the state duration */
	else if(!strcmp(macro,"HOSTDURATION")){
		days=duration/86400;
		duration-=(days*86400);
		hours=duration/3600;
		duration-=(hours*3600);
		minutes=duration/60;
		duration-=(minutes*60);
		seconds=duration;
		asprintf(&macro_ondemand,"%dd %dh %dm %ds",days,hours,minutes,seconds);
	        }

	/* get the execution time macro */
	else if(!strcmp(macro,"HOSTEXECUTIONTIME"))
		asprintf(&macro_ondemand,"%.3f",hst->execution_time);

	/* get the latency macro */
	else if(!strcmp(macro,"HOSTLATENCY"))
		asprintf(&macro_ondemand,"%.3f",hst->latency);

	/* get the last check time macro */
	else if(!strcmp(macro,"LASTHOSTCHECK"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)hst->last_check);

	/* get the last state change time macro */
	else if(!strcmp(macro,"LASTHOSTSTATECHANGE"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)hst->last_state_change);

	/* get the last time up macro */
	else if(!strcmp(macro,"LASTHOSTUP"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)hst->last_time_up);

	/* get the last time down macro */
	else if(!strcmp(macro,"LASTHOSTDOWN"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)hst->last_time_down);

	/* get the last time unreachable macro */
	else if(!strcmp(macro,"LASTHOSTUNREACHABLE"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)hst->last_time_unreachable);

	/* get the notification number macro */
	else if(!strcmp(macro,"HOSTNOTIFICATIONNUMBER"))
		asprintf(&macro_ondemand,"%d",hst->current_notification_number);

	/* get the notification id macro */
	else if(!strcmp(macro,"HOSTNOTIFICATIONID"))
		asprintf(&macro_ondemand,"%lu",hst->current_notification_id);

	/* get the event id macro */
	else if(!strcmp(macro,"HOSTEVENTID"))
		asprintf(&macro_ondemand,"%lu",hst->current_event_id);

	/* get the last event id macro */
	else if(!strcmp(macro,"LASTHOSTEVENTID"))
		asprintf(&macro_ondemand,"%lu",hst->last_event_id);

	/* get the hostgroup names */
	else if(!strcmp(macro,"HOSTGROUPNAMES")){

		/* find all hostgroups this host is associated with */
		for(temp_objectlist=hst->hostgroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

			if((temp_hostgroup=(hostgroup *)temp_objectlist->object_ptr)==NULL)
				continue;

			asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_hostgroup->group_name);
			my_free((void **)&buf2);
			buf2=buf1;
			}
		if(buf2){
			macro_ondemand=(char *)strdup(buf2);
			my_free((void **)&buf2);
			}
		}

	/* get the hostgroup name */
	else if(!strcmp(macro,"HOSTGROUPNAME")){
		if(hst->hostgroups_ptr)
			macro_ondemand=(char *)strdup(((hostgroup *)hst->hostgroups_ptr->object_ptr)->group_name);
	        }
	
	/* get the hostgroup alias */
	else if(!strcmp(macro,"HOSTGROUPALIAS")){
		if(hst->hostgroups_ptr)
			macro_ondemand=(char *)strdup(((hostgroup *)hst->hostgroups_ptr->object_ptr)->alias);
	        }

	/* action url */
	if(!strcmp(macro,"HOSTACTIONURL")){

		if(hst->action_url)
			macro_ondemand=(char *)strdup(hst->action_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
			my_free((void **)&macro_ondemand);
			macro_ondemand=(char *)strdup(temp_buffer);
		        }
	        }

	/* notes url */
	if(!strcmp(macro,"HOSTNOTESURL")){

		if(hst->notes_url)
			macro_ondemand=(char *)strdup(hst->notes_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
			my_free((void **)&macro_ondemand);
			macro_ondemand=(char *)strdup(temp_buffer);
		        }
	        }

	/* notes */
	if(!strcmp(macro,"HOSTNOTES")){
		if(hst->notes)
			macro_ondemand=(char *)strdup(hst->notes);

		/* notes macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,temp_buffer,sizeof(temp_buffer),0);
			my_free((void **)&macro_ondemand);
			macro_ondemand=(char *)strdup(temp_buffer);
		        }
	        }

	/* custom variables */
	else if(strstr(macro,"_HOST")==macro){
		
		/* get the variable name */
		if((customvarname=(char *)strdup(macro+5))){

			for(temp_customvariablesmember=hst->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){

				if(!strcmp(customvarname,temp_customvariablesmember->variable_name)){
					macro_ondemand=(char *)strdup(temp_customvariablesmember->variable_value);
					break;
				        }
			        }

			/* free memory */
			my_free((void **)&customvarname);
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
	servicegroup *temp_servicegroup=NULL;
	customvariablesmember *temp_customvariablesmember=NULL;
	objectlist *temp_objectlist=NULL;
	char *customvarname=NULL;
	char temp_buffer[MAX_INPUT_BUFFER]="";
	char *buf1=NULL;
	char *buf2=NULL;
	time_t current_time=0L;
	unsigned long duration=0L;
	int days=0;
	int hours=0;
	int minutes=0;
	int seconds=0;

#ifdef DEBUG0
	printf("grab_on_demand_service_macro() start\n");
#endif

	if(svc==NULL || macro==NULL)
		return ERROR;

	/* initialize the macro */
	macro_ondemand=NULL;

	time(&current_time);
	duration=(unsigned long)(current_time-svc->last_state_change);

	/* get the service display name */
	if(!strcmp(macro,"SERVICEDISPLAYNAME")){
		if(svc->display_name)
			macro_ondemand=(char *)strdup(svc->display_name);
	        }

	/* get the plugin output */
	if(!strcmp(macro,"SERVICEOUTPUT")){
		if(svc->plugin_output){
			macro_ondemand=(char *)strdup(svc->plugin_output);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* get the long plugin output */
	if(!strcmp(macro,"LONGSERVICEOUTPUT")){
		if(svc->long_plugin_output){
			macro_ondemand=(char *)strdup(svc->long_plugin_output);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* get the performance data */
	else if(!strcmp(macro,"SERVICEPERFDATA")){
		if(svc->perf_data==NULL){
			macro_ondemand=(char *)strdup(svc->perf_data);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* grab the servuce check type */
	else if(!strcmp(macro,"SERVICECHECKTYPE"))
		macro_ondemand=(char *)strdup((svc->check_type==SERVICE_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");

	/* grab the service state type macro (this is usually overridden later on) */
	else if(!strcmp(macro,"SERVICESTATETYPE"))
		macro_ondemand=(char *)strdup((svc->state_type==HARD_STATE)?"HARD":"SOFT");

	/* get the service state */
	else if(!strcmp(macro,"SERVICESTATE")){
		if(svc->current_state==STATE_OK)
			macro_ondemand=(char *)strdup("OK");
		else if(svc->current_state==STATE_WARNING)
			macro_ondemand=(char *)strdup("WARNING");
		else if(svc->current_state==STATE_CRITICAL)
			macro_ondemand=(char *)strdup("CRITICAL");
		else
			macro_ondemand=(char *)strdup("UNKNOWN");
	        }

	/* get the service state id */
	else if(!strcmp(macro,"SERVICESTATEID"))
		asprintf(&macro_ondemand,"%d",svc->current_state);

	/* get the current service check attempt macro */
	else if(!strcmp(macro,"SERVICEATTEMPT"))
		asprintf(&macro_ondemand,"%d",svc->current_attempt);


	/* get the execution time macro */
	else if(!strcmp(macro,"SERVICEEXECUTIONTIME"))
		asprintf(&macro_ondemand,"%.3f",svc->execution_time);

	/* get the latency macro */
	else if(!strcmp(macro,"SERVICELATENCY"))
		asprintf(&macro_ondemand,"%.3f",svc->latency);

	/* get the last check time macro */
	else if(!strcmp(macro,"LASTSERVICECHECK"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_check);

	/* get the last state change time macro */
	else if(!strcmp(macro,"LASTSERVICESTATECHANGE"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_state_change);

	/* get the last time ok macro */
	else if(!strcmp(macro,"LASTSERVICEOK"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_time_ok);

	/* get the last time warning macro */
	else if(!strcmp(macro,"LASTSERVICEWARNING"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_time_warning);

	/* get the last time unknown macro */
	else if(!strcmp(macro,"LASTSERVICEUNKNOWN"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_time_unknown);

	/* get the last time critical macro */
	else if(!strcmp(macro,"LASTSERVICECRITICAL"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_time_critical);

	/* get the service downtime depth */
	else if(!strcmp(macro,"SERVICEDOWNTIME"))
		asprintf(&macro_ondemand,"%d",svc->scheduled_downtime_depth);

	/* get the percent state change */
	else if(!strcmp(macro,"SERVICEPERCENTCHANGE"))
		asprintf(&macro_ondemand,"%.2f",svc->percent_state_change);

	/* get the state duration in seconds */
	else if(!strcmp(macro,"SERVICEDURATIONSEC"))
		asprintf(&macro_ondemand,"%lu",duration);

	/* get the state duration */
	else if(!strcmp(macro,"SERVICEDURATION")){
		days=duration/86400;
		duration-=(days*86400);
		hours=duration/3600;
		duration-=(hours*3600);
		minutes=duration/60;
		duration-=(minutes*60);
		seconds=duration;
		asprintf(&macro_ondemand,"%dd %dh %dm %ds",days,hours,minutes,seconds);
	        }

	/* get the notification number macro */
	else if(!strcmp(macro,"SERVICENOTIFICATIONNUMBER"))
		asprintf(&macro_ondemand,"%d",svc->current_notification_number);

	/* get the notification id macro */
	else if(!strcmp(macro,"SERVICENOTIFICATIONID"))
		asprintf(&macro_ondemand,"%lu",svc->current_notification_id);

	/* get the event id macro */
	else if(!strcmp(macro,"SERVICEEVENTID"))
		asprintf(&macro_ondemand,"%lu",svc->current_event_id);

	/* get the event id macro */
	else if(!strcmp(macro,"LASTSERVICEEVENTID"))
		asprintf(&macro_ondemand,"%lu",svc->last_event_id);

	/* get the servicegroup names */
	else if(!strcmp(macro,"SERVICEGROUPNAMES")){

		/* find all servicegroups this service is associated with */
		for(temp_objectlist=svc->servicegroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

			if((temp_servicegroup=(servicegroup *)temp_objectlist->object_ptr)==NULL)
				continue;

			asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_servicegroup->group_name);
			my_free((void **)&buf2);
			buf2=buf1;
			}
		if(buf2){
			macro_ondemand=(char *)strdup(buf2);
			my_free((void **)&buf2);
			}
		}

	/* get the servicegroup name */
	else if(!strcmp(macro,"SERVICEGROUPNAME")){
		if(svc->servicegroups_ptr)
			macro_ondemand=(char *)strdup(((servicegroup *)svc->servicegroups_ptr->object_ptr)->group_name);
	        }
	
	/* get the servicegroup alias */
	else if(!strcmp(macro,"SERVICEGROUPALIAS")){
		if(svc->servicegroups_ptr)
			macro_ondemand=(char *)strdup(((servicegroup *)svc->servicegroups_ptr->object_ptr)->alias);
	        }

	/* action url */
	else if(!strcmp(macro,"SERVICEACTIONURL")){
		if(svc->action_url)
			macro_ondemand=(char *)strdup(svc->action_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
			my_free((void **)&macro_ondemand);
			macro_ondemand=(char *)strdup(temp_buffer);
		        }
	        }

	/* notes url */
	else if(!strcmp(macro,"SERVICENOTESURL")){

		if(svc->notes_url)
			macro_ondemand=(char *)strdup(svc->notes_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,temp_buffer,sizeof(temp_buffer),URL_ENCODE_MACRO_CHARS);
			my_free((void **)&macro_ondemand);
			macro_ondemand=(char *)strdup(temp_buffer);
		        }
	        }

	/* notes */
	else if(!strcmp(macro,"SERVICENOTES")){
		if(svc->notes)
			macro_ondemand=(char *)strdup(svc->notes);

		/* notes macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,temp_buffer,sizeof(temp_buffer),0);
			my_free((void **)&macro_ondemand);
			macro_ondemand=(char *)strdup(temp_buffer);
		        }
	        }

	/* custom variables */
	else if(strstr(macro,"_SERVICE")==macro){
		
		/* get the variable name */
		if((customvarname=(char *)strdup(macro+8))){

			for(temp_customvariablesmember=svc->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){

				if(!strcmp(customvarname,temp_customvariablesmember->variable_name)){
					macro_ondemand=(char *)strdup(temp_customvariablesmember->variable_value);
					break;
				        }
			        }

			/* free memory */
			my_free((void **)&customvarname);
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
	customvariablesmember *temp_customvariablesmember=NULL;
	char *customvarname=NULL;
	int x=0;

#ifdef DEBUG0
	printf("grab_contact_macros() start\n");
#endif

	/* get the name */
	my_free((void **)&macro_x[MACRO_CONTACTNAME]);
	macro_x[MACRO_CONTACTNAME]=(char *)strdup(cntct->name);

	/* get the alias */
	my_free((void **)&macro_x[MACRO_CONTACTALIAS]);
	macro_x[MACRO_CONTACTALIAS]=(char *)strdup(cntct->alias);

	/* get the email address */
	my_free((void **)&macro_x[MACRO_CONTACTEMAIL]);
	if(cntct->email)
		macro_x[MACRO_CONTACTEMAIL]=(char *)strdup(cntct->email);

	/* get the pager number */
	my_free((void **)&macro_x[MACRO_CONTACTPAGER]);
	if(cntct->pager)
		macro_x[MACRO_CONTACTPAGER]=(char *)strdup(cntct->pager);

	/* get misc contact addresses */
	for(x=0;x<MAX_CONTACT_ADDRESSES;x++){
		my_free((void **)&macro_contactaddress[x]);
		if(cntct->address[x]){
			macro_contactaddress[x]=(char *)strdup(cntct->address[x]);
			/*strip(macro_contactaddress[x]);*/
		        }
	        }

	/* get custom variables */
	for(temp_customvariablesmember=cntct->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
		asprintf(&customvarname,"_CONTACT%s",temp_customvariablesmember->variable_name);
		add_custom_variable_to_object(&macro_custom_contact_vars,customvarname,temp_customvariablesmember->variable_value);
		my_free((void **)&customvarname);
	        }

	/*
	strip(macro_x[MACRO_CONTACTNAME]);
	strip(macro_x[MACRO_CONTACTALIAS]);
	strip(macro_x[MACRO_CONTACTEMAIL]);
	strip(macro_x[MACRO_CONTACTPAGER]);
	*/

#ifdef DEBUG0
	printf("grab_contact_macros() end\n");
#endif

	return OK;
        }



/* grab summary macros (filtered for a specific contact) */
int grab_summary_macros(contact *temp_contact){
	host *temp_host=NULL;
	service  *temp_service=NULL;
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

	/* this func seems to take up quite a bit of CPU, so skip it if we have a large install... */
	if(use_large_installation_tweaks==TRUE)
		return OK;

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
	my_free((void **)&macro_x[MACRO_TOTALHOSTSUP]);
	asprintf(&macro_x[MACRO_TOTALHOSTSUP],"%d",hosts_up);

	/* get total hosts down */
	my_free((void **)&macro_x[MACRO_TOTALHOSTSDOWN]);
	asprintf(&macro_x[MACRO_TOTALHOSTSDOWN],"%d",hosts_down);

	/* get total hosts unreachable */
	my_free((void **)&macro_x[MACRO_TOTALHOSTSUNREACHABLE]);
	asprintf(&macro_x[MACRO_TOTALHOSTSUNREACHABLE],"%d",hosts_unreachable);

	/* get total unhandled hosts down */
	my_free((void **)&macro_x[MACRO_TOTALHOSTSDOWNUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALHOSTSDOWNUNHANDLED],"%d",hosts_down_unhandled);

	/* get total unhandled hosts unreachable */
	my_free((void **)&macro_x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED],"%d",hosts_unreachable_unhandled);

	/* get total host problems */
	my_free((void **)&macro_x[MACRO_TOTALHOSTPROBLEMS]);
	asprintf(&macro_x[MACRO_TOTALHOSTPROBLEMS],"%d",host_problems);

	/* get total unhandled host problems */
	my_free((void **)&macro_x[MACRO_TOTALHOSTPROBLEMSUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALHOSTPROBLEMSUNHANDLED],"%d",host_problems_unhandled);

	/* get total services ok */
	my_free((void **)&macro_x[MACRO_TOTALSERVICESOK]);
	asprintf(&macro_x[MACRO_TOTALSERVICESOK],"%d",services_ok);

	/* get total services warning */
	my_free((void **)&macro_x[MACRO_TOTALSERVICESWARNING]);
	asprintf(&macro_x[MACRO_TOTALSERVICESWARNING],"%d",services_warning);

	/* get total services critical */
	my_free((void **)&macro_x[MACRO_TOTALSERVICESCRITICAL]);
	asprintf(&macro_x[MACRO_TOTALSERVICESCRITICAL],"%d",services_critical);

	/* get total services unknown */
	my_free((void **)&macro_x[MACRO_TOTALSERVICESUNKNOWN]);
	asprintf(&macro_x[MACRO_TOTALSERVICESUNKNOWN],"%d",services_unknown);

	/* get total unhandled services warning */
	my_free((void **)&macro_x[MACRO_TOTALSERVICESWARNINGUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALSERVICESWARNINGUNHANDLED],"%d",services_warning_unhandled);

	/* get total unhandled services critical */
	my_free((void **)&macro_x[MACRO_TOTALSERVICESCRITICALUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALSERVICESCRITICALUNHANDLED],"%d",services_critical_unhandled);

	/* get total unhandled services unknown */
	my_free((void **)&macro_x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED],"%d",services_unknown_unhandled);

	/* get total service problems */
	my_free((void **)&macro_x[MACRO_TOTALSERVICEPROBLEMS]);
	asprintf(&macro_x[MACRO_TOTALSERVICEPROBLEMS],"%d",service_problems);

	/* get total unhandled service problems */
	my_free((void **)&macro_x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED],"%d",service_problems_unhandled);

#ifdef DEBUG0
	printf("grab_summary_macros() end\n");
#endif

	return OK;
        }



/* updates date/time macros */
int grab_datetime_macros(void){
	time_t t=0L;

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

	/*
	strip(macro_x[MACRO_LONGDATETIME]);
	strip(macro_x[MACRO_SHORTDATETIME]);
	strip(macro_x[MACRO_DATE]);
	strip(macro_x[MACRO_TIME]);
	strip(macro_x[MACRO_TIMET]);
	*/

#ifdef DEBUG0
	printf("grab_datetime_macros() end\n");
#endif

	return OK;
        }



/* clear argv macros - used in commands */
int clear_argv_macros(void){
	register int x=0;


#ifdef DEBUG0
	printf("clear_argv_macros() start\n");
#endif

	/* command argument macros */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++)
		my_free((void **)&macro_argv[x]);

#ifdef DEBUG0
	printf("clear_argv_macros() end\n");
#endif

	return OK;
        }



/* clear all macros that are not "constant" (i.e. they change throughout the course of monitoring) */
int clear_volatile_macros(void){
	customvariablesmember *this_customvariablesmember=NULL;
	customvariablesmember *next_customvariablesmember=NULL;
	register int x=0;

#ifdef DEBUG0
	printf("clear_volatile_macros() start\n");
#endif

	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_ADMINEMAIL:
		case MACRO_ADMINPAGER:
		case MACRO_MAINCONFIGFILE:
		case MACRO_STATUSDATAFILE:
		case MACRO_RETENTIONDATAFILE:
		case MACRO_OBJECTCACHEFILE:
		case MACRO_TEMPFILE:
		case MACRO_LOGFILE:
		case MACRO_RESOURCEFILE:
		case MACRO_COMMANDFILE:
		case MACRO_HOSTPERFDATAFILE:
		case MACRO_SERVICEPERFDATAFILE:
		case MACRO_PROCESSSTARTTIME:
		case MACRO_TEMPPATH:
			break;
		default:
			my_free((void **)&macro_x[x]);
			break;
		        }
	        }

	/* contact address macros */
	for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
		my_free((void **)&macro_contactaddress[x]);

	/* clear on-demand macro */
	my_free((void **)&macro_ondemand);

	/* clear ARGx macros */
	clear_argv_macros();

	/* clear custom host variables */
	for(this_customvariablesmember=macro_custom_host_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free((void **)&this_customvariablesmember->variable_name);
		my_free((void **)&this_customvariablesmember->variable_value);
		my_free((void **)&this_customvariablesmember);
	        }
	macro_custom_host_vars=NULL;

	/* clear custom service variables */
	for(this_customvariablesmember=macro_custom_service_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free((void **)&this_customvariablesmember->variable_name);
		my_free((void **)&this_customvariablesmember->variable_value);
		my_free((void **)&this_customvariablesmember);
	        }
	macro_custom_service_vars=NULL;

	/* clear custom contact variables */
	for(this_customvariablesmember=macro_custom_contact_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free((void **)&this_customvariablesmember->variable_name);
		my_free((void **)&this_customvariablesmember->variable_value);
		my_free((void **)&this_customvariablesmember);
	        }
	macro_custom_contact_vars=NULL;

#ifdef DEBUG0
	printf("clear_volatile_macros() end\n");
#endif

	return OK;
        }


/* clear macros that are constant (i.e. they do NOT change during monitoring) */
int clear_nonvolatile_macros(void){
	register int x=0;

#ifdef DEBUG0
	printf("clear_nonvolatile_macros() start\n");
#endif
	
	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_ADMINEMAIL:
		case MACRO_ADMINPAGER:
		case MACRO_MAINCONFIGFILE:
		case MACRO_STATUSDATAFILE:
		case MACRO_RETENTIONDATAFILE:
		case MACRO_OBJECTCACHEFILE:
		case MACRO_TEMPFILE:
		case MACRO_LOGFILE:
		case MACRO_RESOURCEFILE:
		case MACRO_COMMANDFILE:
		case MACRO_HOSTPERFDATAFILE:
		case MACRO_SERVICEPERFDATAFILE:
		case MACRO_PROCESSSTARTTIME:
		case MACRO_TEMPPATH:
			my_free((void **)&macro_x[x]);
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
	register int x=0;

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
	add_macrox_name(MACRO_HOSTDISPLAYNAME,"HOSTDISPLAYNAME");
	add_macrox_name(MACRO_SERVICEDISPLAYNAME,"SERVICEDISPLAYNAME");
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
	add_macrox_name(MACRO_LONGHOSTOUTPUT,"LONGHOSTOUTPUT");
	add_macrox_name(MACRO_LONGSERVICEOUTPUT,"LONGSERVICEOUTPUT");
	add_macrox_name(MACRO_TEMPPATH,"TEMPPATH");
	add_macrox_name(MACRO_HOSTNOTIFICATIONNUMBER,"HOSTNOTIFICATIONNUMBER");
	add_macrox_name(MACRO_SERVICENOTIFICATIONNUMBER,"SERVICENOTIFICATIONNUMBER");
	add_macrox_name(MACRO_HOSTNOTIFICATIONID,"HOSTNOTIFICATIONID");
	add_macrox_name(MACRO_SERVICENOTIFICATIONID,"SERVICENOTIFICATIONID");
	add_macrox_name(MACRO_HOSTEVENTID,"HOSTEVENTID");
	add_macrox_name(MACRO_LASTHOSTEVENTID,"LASTHOSTEVENTID");
	add_macrox_name(MACRO_SERVICEEVENTID,"SERVICEEVENTID");
	add_macrox_name(MACRO_LASTSERVICEEVENTID,"LASTSERVICEEVENTID");
	add_macrox_name(MACRO_HOSTGROUPNAMES,"HOSTGROUPNAMES");
	add_macrox_name(MACRO_SERVICEGROUPNAMES,"SERVICEGROUPNAMES");
	add_macrox_name(MACRO_HOSTACKAUTHORNAME,"HOSTACKAUTHORNAME");
	add_macrox_name(MACRO_HOSTACKAUTHORALIAS,"HOSTACKAUTHORALIAS");
	add_macrox_name(MACRO_SERVICEACKAUTHORNAME,"SERVICEACKAUTHORNAME");
	add_macrox_name(MACRO_SERVICEACKAUTHORALIAS,"SERVICEACKAUTHORALIAS");

#ifdef DEBUG0
	printf("init_macrox_names() end\n");
#endif

	return OK;
        }


/* saves the name of a macro */
int add_macrox_name(int i, char *name){

	/* dup the macro name */
	macro_x_names[i]=(char *)strdup(name);

	return OK;
        }


/* free memory associated with the macrox names */
int free_macrox_names(void){
	register int x=0;

#ifdef DEBUG0
	printf("free_macrox_names() start\n");
#endif

	/* free each macro name */
	for(x=0;x<MACRO_X_COUNT;x++)
		my_free((void **)&macro_x_names[x]);

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
	set_custom_macro_environment_vars(set);

#ifdef DEBUG0
	printf("set_all_macro_environment_vars() start\n");
#endif

	return OK;
        }


/* sets or unsets macrox environment variables */
int set_macrox_environment_vars(int set){
	register int x=0;

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
	char *macro_name=NULL;
	register int x=0;

#ifdef DEBUG0
	printf("set_argv_macro_environment_vars() start\n");
#endif

	/* set each of the argv macro environment variables */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){
		asprintf(&macro_name,"ARG%d",x+1);
		set_macro_environment_var(macro_name,macro_argv[x],set);
		my_free((void **)&macro_name);
	        }

#ifdef DEBUG0
	printf("set_argv_macro_environment_vars() end\n");
#endif

	return OK;
        }


/* sets or unsets custom host/service/contact macro environment variables */
int set_custom_macro_environment_vars(int set){
	customvariablesmember *temp_customvariablesmember=NULL;

#ifdef DEBUG0
	printf("set_custom_macro_environment_vars() start\n");
#endif

	/* set each of the custom host environment variables */
	for(temp_customvariablesmember=macro_custom_host_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
		set_macro_environment_var(temp_customvariablesmember->variable_name,clean_macro_chars(temp_customvariablesmember->variable_value,STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS),set);
		}

	/* set each of the custom service environment variables */
	for(temp_customvariablesmember=macro_custom_service_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next)
		set_macro_environment_var(temp_customvariablesmember->variable_name,clean_macro_chars(temp_customvariablesmember->variable_value,STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS),set);

	/* set each of the custom contact environment variables */
	for(temp_customvariablesmember=macro_custom_contact_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next)
		set_macro_environment_var(temp_customvariablesmember->variable_name,clean_macro_chars(temp_customvariablesmember->variable_value,STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS),set);

#ifdef DEBUG0
	printf("set_custom_macro_environment_vars() end\n");
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
	my_free((void **)&env_macro_name);

#ifdef DEBUG0
	printf("set_macro_environment_var() end\n");
#endif

	return OK;
        }




/******************************************************************/
/******************** SYSTEM COMMAND FUNCTIONS ********************/
/******************************************************************/


/* executes a system command - used for notifications, event handlers, etc. */
int my_system(char *cmd,int timeout,int *early_timeout,double *exectime,char **output,int max_output_length){
        pid_t pid=0;
	int status=0;
	int result=0;
	char buffer[MAX_INPUT_BUFFER]="";
	char *temp_buffer=NULL;
	int fd[2];
	FILE *fp=NULL;
	int bytes_read=0;
	struct timeval start_time,end_time;
	dbuf output_dbuf;
	int dbuf_chunk=1024;
#ifdef EMBEDDEDPERL
	char fname[512];
	char *args[5]={"",DO_CLEAN, "", "", NULL };
	SV *plugin_hndlr_cr;
	STRLEN n_a ;
	char *perl_output;
	int count;
	int use_epn=FALSE;
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
		*output=NULL;
	*early_timeout=FALSE;
	*exectime=0.0;

	/* if no command was passed, return with no error */
	if(cmd==NULL)
	        return STATE_OK;

#ifdef EMBEDDEDPERL

	/* get"filename" component of command */
	strncpy(fname,cmd,strcspn(cmd," "));
	fname[strcspn(cmd," ")]='\x0';

	/* should we use the embedded Perl interpreter to run this script? */
	use_epn=file_uses_embedded_perl(fname);

	/* if yes, do some initialization */
	if(use_epn==TRUE){

		args[0]=fname;
		args[2]="";

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

		if( SvTRUE(ERRSV) ){
			/*
			 * XXXX need pipe open to send the compilation failure message back to Nagios ?
			 */
			(void) POPs ;

			asprintf(&temp_buffer,"%s", SvPVX(ERRSV));
#ifdef DEBUG1
			printf("embedded perl failed to compile %s, compile error %s\n",fname,temp_buffer);
#endif
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
			my_free((void **)&temp_buffer);

			return STATE_UNKNOWN;
			}
		else{
			plugin_hndlr_cr=newSVsv(POPs);
#ifdef DEBUG1
			printf("embedded perl successfully compiled %s and returned plugin handler (Perl subroutine code ref)\n",fname);
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

		asprintf(&temp_buffer,"Warning: fork() in my_system() failed for command \"%s\"\n",cmd);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);

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

		/* reset signal handling */
		reset_sighandler();

		/* close pipe for reading */
		close(fd[0]);

		/* trap commands that timeout */
		signal(SIGALRM,my_system_sighandler);
		alarm(timeout);


		/******** BEGIN EMBEDDED PERL CODE EXECUTION ********/

#ifdef EMBEDDEDPERL
		if(use_epn==TRUE){

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
			strncpy(buffer,perl_output,sizeof(buffer));
			buffer[sizeof(buffer)-1]='\x0';
			status=POPi ;

			PUTBACK;
			FREETMPS;
			LEAVE;                                    

#ifdef DEBUG0
			printf("embedded perl ran command %s with output %d, %s\n",fname,status,buffer);
#endif

			/* write the output back to the parent process */
			write(fd[1],buffer,strlen(buffer)+1);

			/* close pipe for writing */
			close(fd[1]);

			/* reset the alarm */
			alarm(0);

			_exit(status);
		        }
#endif  
		/******** END EMBEDDED PERL CODE EXECUTION ********/


		/* run the command */
		fp=(FILE *)popen(cmd,"r");
		
		/* report an error if we couldn't run the command */
		if(fp==NULL){

			strncpy(buffer,"(Error: Could not execute command)\n",sizeof(buffer)-1);
			buffer[sizeof(buffer)-1]='\x0';

			/* write the error back to the parent process */
			write(fd[1],buffer,strlen(buffer)+1);

			result=STATE_CRITICAL;
		        }
		else{

			/* write all the lines of output back to the parent process */
			while(fgets(buffer,sizeof(buffer)-1,fp))
				write(fd[1],buffer,strlen(buffer));

			/* close the command and get termination status */
			status=pclose(fp);
			
			/* report an error if we couldn't close the command */
			if(status==-1)
				result=STATE_CRITICAL;
			else
				result=WEXITSTATUS(status);
		        }

		/* close pipe for writing */
		close(fd[1]);

		/* reset the alarm */
		alarm(0);
		
		/* clear environment variables */
		set_all_macro_environment_vars(FALSE);

#ifndef DONT_USE_MEMORY_PERFORMANCE_TWEAKS
		/* free allocated memory */
		/* this needs to be done last, so we don't free memory for variables before they're used above */
		if(use_large_installation_tweaks==FALSE)
			free_memory();
#endif

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
		if(*exectime<0.0)
			*exectime=0.0;

		/* get the exit code returned from the program */
		result=WEXITSTATUS(status);

		/* check for possibly missing scripts/binaries/etc */
		if(result==126 || result==127){
			temp_buffer="\163\157\151\147\141\156\040\145\144\151\163\156\151";
			asprintf(&temp_buffer,"Warning: Attempting to execute the command \"%s\" resulted in a return code of %d.  Make sure the script or binary you are trying to execute actually exists...\n",cmd,result);
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
			my_free((void **)&temp_buffer);
		        }

		/* check bounds on the return value */
		if(result<-1 || result>3)
			result=STATE_UNKNOWN;

		/* initialize output */
		strcpy(buffer,"");

		/* initialize dynamic buffer */
		dbuf_init(&output_dbuf,dbuf_chunk);

		/* try and read the results from the command output (retry if we encountered a signal) */
		do{
			bytes_read=read(fd[0],buffer,sizeof(buffer)-1);

			/* append data we just read to dynamic buffer */
			if(bytes_read>0){
				buffer[bytes_read]='\x0';
				dbuf_strcat(&output_dbuf,buffer);
			        }

			/* handle errors */
			if(bytes_read==-1){
				/* we encountered a recoverable error, so try again */
				if(errno==EINTR || errno==EAGAIN)
					continue;
				else
					break;
			        }

			/* we're done */
			if(bytes_read==0)
				break;

  		        }while(1);

		/* cap output length - this isn't necessary, but it keeps runaway plugin output from causing problems */
		if(max_output_length>0  && output_dbuf.used_size>max_output_length)
			output_dbuf.buf[max_output_length]='\x0';

		if(output!=NULL && output_dbuf.buf)
			*output=(char *)strdup(output_dbuf.buf);

		/* free memory */
		dbuf_free(&output_dbuf);

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
		broker_system_command(NEBTYPE_SYSTEM_COMMAND_END,NEBFLAG_NONE,NEBATTR_NONE,start_time,end_time,*exectime,timeout,*early_timeout,result,cmd,(output==NULL)?NULL:*output,NULL);
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
int get_raw_command_line(command *cmd_ptr, char *cmd, char *full_command, int buffer_length, int macro_options){
	char temp_arg[MAX_COMMAND_BUFFER]="";
	char arg_buffer[MAX_COMMAND_BUFFER]="";
	register int x=0;
	register int y=0;
	register int arg_index=0;
	register int escaped=FALSE;

#ifdef DEBUG0
	printf("get_raw_command_line() start\n");
#endif
#ifdef DEBUG1
	printf("\tInput: %s\n",cmd);
#endif

	/* clear the argv macros */
	clear_argv_macros();

	/* initialize the full command */
	if(full_command)
		full_command[0]='\x0';

	/* make sure we've got all the requirements */
	if(cmd_ptr==NULL || full_command==NULL){
#ifdef DEBUG1
		printf("\tWe don't have enough data to get the expanded command line!\n");
#endif
		return ERROR;
	        }

	/* get the full command line */
	if(cmd_ptr->command_line!=NULL){
		strncpy(full_command,cmd_ptr->command_line,buffer_length);
		full_command[buffer_length-1]='\x0';
		}

	/* get the command arguments */
	if(cmd!=NULL){

		/* skip the command name (we're about to get the arguments)... */
		for(arg_index=0;;arg_index++){
			if(cmd[arg_index]=='!' || cmd[arg_index]=='\x0')
				break;
			}

		/* get each command argument */
		for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){

			/* we reached the end of the arguments... */
			if(cmd[arg_index]=='\x0')
				break;

			/* get the next argument */
			/* can't use strtok(), as that's used in process_macros... */
			for(arg_index++,y=0;y<sizeof(temp_arg)-1;arg_index++){
				
				/* backslashes escape */
				if(cmd[arg_index]=='\\' && escaped==FALSE){
					escaped=TRUE;
					continue;
					}

				/* end of argument */
				if((cmd[arg_index]=='!' && escaped==FALSE) || cmd[arg_index]=='\x0')
					break;

				/* normal of escaped char */
				temp_arg[y]=cmd[arg_index];
				y++;

				/* clear escaped flag */
				escaped=FALSE;
				}
			temp_arg[y]='\x0';

			/* ADDED 01/29/04 EG */
			/* process any macros we find in the argument */
			process_macros(temp_arg,arg_buffer,sizeof(arg_buffer),macro_options);

			macro_argv[x]=(char *)strdup(arg_buffer);
			}
		}

#ifdef DEBUG1
	printf("\tOutput: %s\n",full_command);
#endif
#ifdef DEBUG0
	printf("get_raw_command_line() end\n");
#endif

	return OK;
        }



/* given a "raw" command, return the "expanded" or "whole" command line */
int get_raw_command_line2(command *cmd_ptr, char *cmd_args, char **full_command, int macro_options){
	char temp_arg[MAX_COMMAND_BUFFER]="";
	char arg_buffer[MAX_COMMAND_BUFFER]="";
	register int x=0;
	register int y=0;
	register int arg_index=0;

#ifdef DEBUG0
	printf("get_raw_command_line() start\n");
#endif
#ifdef DEBUG1
	printf("\tInput: %s\n",cmd);
#endif

	/* clear the argv macros */
	clear_argv_macros();

	/* initialize the full command */
	if(full_command)
		*full_command=NULL;

	/* make sure we've got all the requirements */
	if(cmd_ptr==NULL || full_command==NULL){
#ifdef DEBUG1
		printf("\tWe don't have enough data to get the expanded command line!\n");
#endif
		return ERROR;
	        }

	/* get the full command line */
	if(cmd_ptr->command_line!=NULL)
		*full_command=(char *)strdup(cmd_ptr->command_line);

	/* get the command arguments */
	if(cmd_args!=NULL){

		/* skip the command name (we're about to get the arguments)... */
		/* use the previous value from above */
		arg_index=0;

		/* get each command argument */
		for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){

			/* we reached the end of the arguments... */
			if(cmd_args[arg_index]=='\x0')
				break;

			/* get the next argument */
			/* can't use strtok(), as that's used in process_macros... */
			for(arg_index++,y=0;y<sizeof(temp_arg)-1;arg_index++){
				if(cmd_args[arg_index]=='!' || cmd_args[arg_index]=='\x0')
					break;
				temp_arg[y]=cmd_args[arg_index];
				y++;
				}
			temp_arg[y]='\x0';

			/* ADDED 01/29/04 EG */
			/* process any macros we find in the argument */
			process_macros(temp_arg,arg_buffer,sizeof(arg_buffer),macro_options);

			macro_argv[x]=(char *)strdup(arg_buffer);
			}
		}

#ifdef DEBUG1
	printf("\tOutput: %s\n",full_command);
#endif
#ifdef DEBUG0
	printf("get_raw_command_line() end\n");
#endif

	return OK;
        }





/******************************************************************/
/************************* TIME FUNCTIONS *************************/
/******************************************************************/


/* see if the specified time falls into a valid time range in the given time period */
int check_time_against_period(time_t check_time, timeperiod *tperiod){
	timerange *temp_range;
	unsigned long midnight_today;
	struct tm *t;

#ifdef DEBUG0
	printf("check_time_against_period() start\n");
#endif
	
	/* if no period was specified, assume the time is good */
	if(tperiod==NULL)
		return OK;

	t=localtime((time_t *)&check_time);

	/* calculate the start of the day (midnight, 00:00 hours) */
	t->tm_sec=0;
	t->tm_min=0;
	t->tm_hour=0;
	midnight_today=(unsigned long)mktime(t);

	for(temp_range=tperiod->days[t->tm_wday];temp_range!=NULL;temp_range=temp_range->next){

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
void get_next_valid_time(time_t preferred_time,time_t *valid_time, timeperiod *tperiod){
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
	if(check_time_against_period(preferred_time,tperiod)==OK)
		*valid_time=preferred_time;

	/* else find the next available time */
	else{

		/* find the time period - if we can't find it, go with the preferred time */
		if(tperiod==NULL){
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
			for(temp_timerange=tperiod->days[weekday];temp_timerange!=NULL;temp_timerange=temp_timerange->next){

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
	static char *sigs[]={"EXIT","HUP","INT","QUIT","ILL","TRAP","ABRT","BUS","FPE","KILL","USR1","SEGV","USR2","PIPE","ALRM","TERM","STKFLT","CHLD","CONT","STOP","TSTP","TTIN","TTOU","URG","XCPU","XFSZ","VTALRM","PROF","WINCH","IO","PWR","UNUSED","ZERR","DEBUG",(char *)NULL};
	int i=0;
	char *temp_buffer=NULL;


	/* if shutdown is already true, we're in a signal trap loop! */
	/* changed 09/07/06 to only exit on segfaults */
	if(sigshutdown==TRUE && sig==SIGSEGV)
		exit(ERROR);

	if(sig<0)
		sig=-sig;

	for(i=0;sigs[i]!=(char *)NULL;i++);

	sig%=i;

	/* we received a SIGHUP, so restart... */
	if(sig==SIGHUP){

		sigrestart=TRUE;

		asprintf(&temp_buffer,"Caught SIGHUP, restarting...\n");
		write_to_all_logs(temp_buffer,NSLOG_PROCESS_INFO);
#ifdef DEBUG2
		printf("%s\n",temp_buffer);
#endif
		my_free((void **)&temp_buffer);
	        }

	/* else begin shutting down... */
	else if(sig<16){

		sigshutdown=TRUE;

		asprintf(&temp_buffer,"Caught SIG%s, shutting down...\n",sigs[sig]);
		write_to_all_logs(temp_buffer,NSLOG_PROCESS_INFO);
#ifdef DEBUG2
		printf("%s\n",temp_buffer);
#endif
		my_free((void **)&temp_buffer);
	        }

	return;
        }


/* handle timeouts when executing service checks */
void service_check_sighandler(int sig){
	struct timeval end_time;

	/* get the current time */
	gettimeofday(&end_time,NULL);

	/* write plugin check results to temp file */
	if(check_result_info.output_file_fp){
		fputs("(Service Check Timed Out)",check_result_info.output_file_fp);
		fclose(check_result_info.output_file_fp);
	        }
	if(check_result_info.output_file_fd>0)
		close(check_result_info.output_file_fd);

#ifdef SERVICE_CHECK_TIMEOUTS_RETURN_UNKNOWN
	check_result_info.return_code=STATE_UNKNOWN;
#else
	check_result_info.return_code=STATE_CRITICAL;
#endif
	check_result_info.finish_time=end_time;
	check_result_info.early_timeout=TRUE;

	/* write check result to message queue */
	write_check_result(&check_result_info);

	/* close write end of IPC pipe */
	close(ipc_pipe[1]);

	/* try to kill the command that timed out by sending termination signal to our process group */
	/* we also kill ourselves while doing this... */
	kill((pid_t)0,SIGKILL);
	
	/* force the child process (service check) to exit... */
	_exit(STATE_CRITICAL);
        }


/* handle timeouts when executing host checks */
void host_check_sighandler(int sig){
	struct timeval end_time;

	/* get the current time */
	gettimeofday(&end_time,NULL);

	/* write plugin check results to temp file */
	if(check_result_info.output_file_fp){
		fputs("(Host Check Timed Out)",check_result_info.output_file_fp);
		fclose(check_result_info.output_file_fp);
	        }
	if(check_result_info.output_file_fd>0)
		close(check_result_info.output_file_fd);

	check_result_info.return_code=STATE_CRITICAL;
	check_result_info.finish_time=end_time;
	check_result_info.early_timeout=TRUE;

	/* write check result to message queue */
	write_check_result(&check_result_info);

	/* close write end of IPC pipe */
	close(ipc_pipe[1]);

	/* try to kill the command that timed out by sending termination signal to our process group */
	/* we also kill ourselves while doing this... */
	kill((pid_t)0,SIGKILL);
	
	/* force the child process (service check) to exit... */
	_exit(STATE_CRITICAL);
        }


/* handle timeouts when executing commands via my_system() */
void my_system_sighandler(int sig){

	/* force the child process to exit... */
	_exit(STATE_CRITICAL);
        }




/******************************************************************/
/************************ DAEMON FUNCTIONS ************************/
/******************************************************************/

int daemon_init(void){
	pid_t pid=-1;
	int pidno=0;
	int lockfile=0;
	int val=0;
	char buf[256];
	struct flock lock;
	char *temp_buffer=NULL;
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
			asprintf(&temp_buffer,"%s is a directory\n",lock_file);
		else if(errno==EACCES)
			asprintf(&temp_buffer,"You do not have permission to write to %s\n",lock_file);
		else if(errno==ENAMETOOLONG)
			asprintf(&temp_buffer,"The filename is too long: %s\n",lock_file);
		else if(errno==ENOENT)
			asprintf(&temp_buffer,"%s does not exist (ENOENT)\n",lock_file);
		else if(errno==ENOTDIR)
			asprintf(&temp_buffer,"%s does not exist (ENOTDIR)\n",lock_file);
		else if(errno==ENXIO)
			asprintf(&temp_buffer,"Cannot write to special file\n");
		else if(errno==ENODEV)
			asprintf(&temp_buffer,"Cannot write to device\n");
		else if(errno==EROFS)
			asprintf(&temp_buffer,"%s is on a read-only file system\n",lock_file);
		else if(errno==ETXTBSY)
			asprintf(&temp_buffer,"%s is a currently running program\n",lock_file);
		else if(errno==EFAULT)
			asprintf(&temp_buffer,"%s is outside address space\n",lock_file);
		else if(errno==ELOOP)
			asprintf(&temp_buffer,"Too many symbolic links\n");
		else if(errno==ENOSPC)
			asprintf(&temp_buffer,"No space on device\n");
		else if(errno==ENOMEM)
			asprintf(&temp_buffer,"Insufficient kernel memory\n");
		else if(errno==EMFILE)
			asprintf(&temp_buffer,"Too many files open in process\n");
		else if(errno==ENFILE)
			asprintf(&temp_buffer,"Too many files open on system\n");

		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
		my_free((void **)&temp_buffer);

		asprintf(&temp_buffer,"Bailing out due to errors encountered while attempting to daemonize... (PID=%d)",(int)getpid());
		write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR,TRUE);
		my_free((void **)&temp_buffer);

		cleanup();
		exit(ERROR);
	        }

	/* see if we can read the contents of the lockfile */
	if((val=read(lockfile,buf,(size_t)10))<0){
		asprintf(&temp_buffer,"Lockfile exists but cannot be read");
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
		my_free((void **)&temp_buffer);
		cleanup();
		exit(ERROR);
	        }

	/* we read something - check the PID */
	if(val>0){
		if((val=sscanf(buf,"%d",&pidno))<1){
			asprintf(&temp_buffer,"Lockfile '%s' does not contain a valid PID (%s)",lock_file,buf);
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
			my_free((void **)&temp_buffer);
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
			asprintf(&temp_buffer,"Lockfile '%s' is held by PID %d.  Bailing out...",lock_file,(int)lock.l_pid);
		        }
		else
			asprintf(&temp_buffer,"Cannot lock lockfile '%s': %s. Bailing out...",lock_file,strerror(errno));
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
		my_free((void **)&temp_buffer);
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
	char *temp_buffer=NULL;
	uid_t uid=-1;
	gid_t gid=-1;
	struct group *grp=NULL;
	struct passwd *pw=NULL;
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
				asprintf(&temp_buffer,"Warning: Could not get group entry for '%s'",group);
				write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
				my_free((void **)&temp_buffer);
		                }
		        }

		/* else we were passed the GID */
		else
			gid=(gid_t)atoi(group);

		/* set effective group ID if other than current EGID */
		if(gid!=getegid()){

			if(setgid(gid)==-1){
				asprintf(&temp_buffer,"Warning: Could not set effective GID=%d",(int)gid);
				write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
				my_free((void **)&temp_buffer);
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
				asprintf(&temp_buffer,"Warning: Could not get passwd entry for '%s'",user);
				write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
				my_free((void **)&temp_buffer);
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
					asprintf(&temp_buffer,"Warning: Unable to change supplementary groups using initgroups() -- I hope you know what you're doing");
					write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
					my_free((void **)&temp_buffer);
		                        }
				else{
					asprintf(&temp_buffer,"Warning: Possibly root user failed dropping privileges with initgroups()");
					write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
					my_free((void **)&temp_buffer);
					return ERROR;
			                }
	                        }
		        }
#endif
		if(setuid(uid)==-1){
			asprintf(&temp_buffer,"Warning: Could not set effective UID=%d",(int)uid);
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
			my_free((void **)&temp_buffer);
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

/* reads a host/service check result from the circular buffer */
int read_check_result(check_result *info){
	char *temp_buffer=NULL;
	int result=0;
	check_result *buffered_result=NULL;

#ifdef DEBUG0
	printf("read_check_result() start\n");
#endif

	if(info==NULL)
		return -1;

	/* clear the message buffer */
	memset((void *)info,0,sizeof(check_result));

	/* get a lock on the buffer */
	pthread_mutex_lock(&check_result_buffer.buffer_lock);

	/* handle detected overflows */
	if(check_result_buffer.overflow>0){

		/* log the warning */
		asprintf(&temp_buffer,"Warning: Overflow detected in check result buffer - %lu check result(s) lost.\n",check_result_buffer.overflow);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);

		/* reset overflow counter */
		check_result_buffer.overflow=0;
	        }

	/* there are no items in the buffer */
	if(check_result_buffer.items==0)
		result=0;

	/* return the result from the tail of the buffer */
	else{

		buffered_result=((check_result **)check_result_buffer.buffer)[check_result_buffer.tail];
		
		/* copy result to user-supplied structure */
		memcpy(info,buffered_result,sizeof(check_result));
		info->host_name=NULL;
		if(buffered_result->host_name)
			info->host_name=(char *)strdup(buffered_result->host_name);
		info->service_description=NULL;
		if(buffered_result->service_description)
			info->service_description=(char *)strdup(buffered_result->service_description);
		info->output_file=NULL;
		if(buffered_result->output_file)
			info->output_file=(char *)strdup(buffered_result->output_file);

		/* free memory */
		free_check_result(buffered_result);

		/* free memory allocated for buffer slot */
		my_free((void **)&check_result_buffer.buffer[check_result_buffer.tail]);
		check_result_buffer.buffer[check_result_buffer.tail]=NULL;

		/* adjust tail counter and number of items */
		check_result_buffer.tail=(check_result_buffer.tail + 1) % check_result_buffer_slots;
		check_result_buffer.items--;

		result=1;
	        }

	/* release the lock on the buffer */
	pthread_mutex_unlock(&check_result_buffer.buffer_lock);

#ifdef DEBUG0
	printf("read_check_result() end\n");
#endif

	return result;
	}


/* writes host/service check result info to the message pipe */
int write_check_result(check_result *info){
	char *buf=NULL;
	int tbytes=0;
	int buflen=0;
	struct timeval tv;
	int write_result=0;

#ifdef DEBUG0
	printf("write_check_result() start\n");
#endif

	if(info==NULL)
		return 0;

	asprintf(&buf,
		 "%d=%d\n%d=%s\n%d=%s\n%d=%d\n%d=%d\n%d=%d\n%d=%s\n%d=%f\n%d=%lu.%lu\n%d=%lu.%lu\n%d=%d\n%d=%d\n%d=%d\n\n"
		 ,1,info->object_check_type
		 ,2,(info->host_name==NULL)?"":info->host_name
		 ,3,(info->service_description==NULL)?"":info->service_description
		 ,4,info->check_type
		 ,5,info->scheduled_check
		 ,6,info->reschedule_check
		 ,7,(info->output_file==NULL)?"":info->output_file
		 ,8,info->latency
		 ,9,info->start_time.tv_sec,info->start_time.tv_usec
		 ,10,info->finish_time.tv_sec,info->finish_time.tv_usec
		 ,11,info->early_timeout
		 ,12,info->exited_ok
		 ,13,info->return_code
		);

	if(buf==NULL)
		return 0;

#ifdef DEBUG_CHECK_IPC
	printf("WRITING CHECK RESULT FOR: %s/%s\n",info->host_name,info->service_description);
	printf("%s",buf);
#endif

	buflen=strlen(buf);

	while(tbytes<buflen){

		/* try to write everything we have left */
		write_result=write(ipc_pipe[1],buf+tbytes,buflen-tbytes);

		/* some kind of error occurred */
		if(write_result==-1){

			/* pipe is full - wait a bit and retry */
			if(errno==EAGAIN){
				tv.tv_sec=0;
				tv.tv_usec=250;
				select(0,NULL,NULL,NULL,&tv);
				continue;
			        }

			/* unless we encountered a recoverable error, bail out */
			if(errno!=EAGAIN && errno!=EINTR){
				my_free((void **)&buf);
				return -1;
			        }
		        }

		/* update the number of bytes we've written */
		tbytes+=write_result;
	        }

	/* free memory */
	my_free((void **)&buf);

#ifdef DEBUG0
	printf("write_check_result() end\n");
#endif

	return tbytes;
        }


/* frees memory associated with a host/service check result */
int free_check_result(check_result *info){
	
	if(info==NULL)
		return OK;

	my_free((void **)&info->host_name);
	my_free((void **)&info->service_description);
	my_free((void **)&info->output_file);

	return OK;
        }



/* grabs plugin output and perfdata from a file */
int read_check_output_from_file(char *fname, char **short_output, char **long_output, char **perf_data, int escape_newlines){
	mmapfile *thefile=NULL;
	char *input=NULL;
	int dbuf_chunk=1024;
	dbuf db;

	/* initialize values */
	if(short_output)
		*short_output=NULL;
	if(long_output)
		*long_output=NULL;
	if(perf_data)
		*perf_data=NULL;

	/* no file name */
	if(fname==NULL || !strcmp(fname,"")){
		if(short_output)
			*short_output=(char *)strdup("(Check result file missing - no plugin output!)");
		return ERROR;
	        }

	/* open the file for reading */
	if((thefile=mmap_fopen(fname))==NULL){

		if(short_output)
			*short_output=(char *)strdup("(Cannot read check result file (file may be empty) - no plugin output!)");

		/* try removing the file - zero length files can't be mmap()'ed, so it might exist */
		unlink(fname);

		return ERROR;
	        }

	/* initialize dynamic buffer (1KB chunk size) */
	dbuf_init(&db,dbuf_chunk);

	/* read in all lines from the config file */
	while(1){

		/* free memory */
		my_free((void **)&input);

		/* read the next line */
		if((input=mmap_fgets(thefile))==NULL)
			break;

		/* save the new input */
		dbuf_strcat(&db,input);
	        }

	/* cap output length - this isn't necessary, but it keeps runaway plugin output from causing problems */
	if(db.used_size>MAX_PLUGIN_OUTPUT_LENGTH)
		db.buf[MAX_PLUGIN_OUTPUT_LENGTH]='\x0';

	/* parse the output: short and long output, and perf data */
	parse_check_output(db.buf,short_output,long_output,perf_data,escape_newlines,FALSE);

	/* free dynamic buffer */
	dbuf_free(&db);

	/* free memory and close file */
	my_free((void **)&input);
	mmap_fclose(thefile);

#ifndef DEBUG_CHECK_IPC
	/* remove the file */
	unlink(fname);
#endif

	return OK;
        }



/* parse raw plugin output and return: short and long output, perf data */
int parse_check_output(char *buf, char **short_output, char **long_output, char **perf_data, int escape_newlines, int newlines_are_escaped){
	int current_line=0;
	int found_newline=FALSE;
	int eof=FALSE;
	int used_buf=0;
	int dbuf_chunk=1024;
	dbuf db1;
	dbuf db2;
	char *ptr=NULL;
	int in_perf_data=FALSE;
	char *tempbuf=NULL;
	register int x=0;
	register int y=0;

	/* initialize values */
	if(short_output)
		*short_output=NULL;
	if(long_output)
		*long_output=NULL;
	if(perf_data)
		*perf_data=NULL;

	/* nothing to do */
	if(buf==NULL || !strcmp(buf,""))
		return OK;

	used_buf=strlen(buf)+1;

	/* initialize dynamic buffers (1KB chunk size) */
	dbuf_init(&db1,dbuf_chunk);
	dbuf_init(&db2,dbuf_chunk);

	/* process each line of input */
	for(x=0;eof==FALSE;x++){

		/* we found the end of a line */
		if(buf[x]=='\n')
			found_newline=TRUE;
		else if(buf[x]=='\\' && buf[x+1]=='n' && newlines_are_escaped==TRUE){
			found_newline=TRUE;
			buf[x]='\x0';
			x++;
		        }
		else if(buf[x]=='\x0'){
			found_newline=TRUE;
			eof=TRUE;
		        }
		else
			found_newline=FALSE;

		if(found_newline==TRUE){
	
			current_line++;

			/* handle this line of input */
			buf[x]='\x0';
			if((tempbuf=(char *)strdup(buf))){

				/* first line contains short plugin output and optional perf data */
				if(current_line==1){

					/* get the short plugin output */
					if((ptr=strtok(tempbuf,"|"))){
						if(short_output)
							*short_output=(char *)strdup(ptr);

						/* get the optional perf data */
						if((ptr=strtok(NULL,"\n")))
							dbuf_strcat(&db2,ptr);
					        }
				        }

				/* additional lines contain long plugin output and optional perf data */
				else{

					/* rest of the output is perf data */
					if(in_perf_data==TRUE)
						dbuf_strcat(&db2,tempbuf);

					/* we're still in long output */
					else{

						/* perf data separator has been found */
						if(strstr(tempbuf,"|")){

							/* NOTE: strtok() causes problems if first character of tempbuf='|', so use my_strtok() instead */
							/* get the remaining long plugin output */
							if((ptr=my_strtok(tempbuf,"|"))){

								if(current_line>2)
									dbuf_strcat(&db1,"\n");
								dbuf_strcat(&db1,ptr);

								/* get the perf data */
								if((ptr=my_strtok(NULL,"\n")))
									dbuf_strcat(&db2,ptr);
							        }

							/* set the perf data flag */
							in_perf_data=TRUE;
						        }

						/* just long output */
						else{
							if(current_line>2)
								dbuf_strcat(&db1,"\n");
							dbuf_strcat(&db1,tempbuf);
						        }
					        }
				        }

				my_free((void **)&tempbuf);
				tempbuf=NULL;
			        }
		

			/* shift data back to front of buffer and adjust counters */
			memmove((void *)&buf[0],(void *)&buf[x+1],(size_t)((int)used_buf-x-1));
			used_buf-=(x+1);
			buf[used_buf]='\x0';
			x=-1;
		        }
	        }

	/* save long output */
	if(long_output && (db1.buf && strcmp(db1.buf,""))){

		if(escape_newlines==FALSE)
			*long_output=(char *)strdup(db1.buf);

		else{

			/* escape newlines (and backslashes) in long output */
			if((tempbuf=(char *)malloc((strlen(db1.buf)*2)+1))){

				for(x=0,y=0;db1.buf[x]!='\x0';x++){

					if(db1.buf[x]=='\n'){
						tempbuf[y++]='\\';
						tempbuf[y++]='n';
					        }
					else if(db1.buf[x]=='\\'){
						tempbuf[y++]='\\';
						tempbuf[y++]='\\';
					        }
					else
						tempbuf[y++]=db1.buf[x];
				        }

				tempbuf[y]='\x0';
				*long_output=(char *)strdup(tempbuf);
				my_free((void **)&tempbuf);
			        }
		        }
	        }

	/* save perf data */
	if(perf_data && (db2.buf && strcmp(db2.buf,"")))
		*perf_data=(char *)strdup(db2.buf);

	/* strip short output and perf data */
	if(short_output)
		strip(*short_output);
	if(perf_data)
		strip(*perf_data);

	/* free dynamic buffers */
	dbuf_free(&db1);
	dbuf_free(&db2);

	return OK;
        }



/* creates external command file as a named pipe (FIFO) and opens it for reading (non-blocked mode) */
int open_command_file(void){
	char *temp_buffer=NULL;
	struct stat st;
 	int result=0;

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

			asprintf(&temp_buffer,"Error: Could not create external command file '%s' as named pipe: (%d) -> %s.  If this file already exists and you are sure that another copy of Nagios is not running, you should delete this file.\n",command_file,errno,strerror(errno));
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
			my_free((void **)&temp_buffer);

			return ERROR;
		        }
	        }

	/* open the command file for reading (non-blocked) - O_TRUNC flag cannot be used due to errors on some systems */
	if((command_file_fd=open(command_file,O_RDONLY | O_NONBLOCK))<0){

		asprintf(&temp_buffer,"Error: Could not open external command file for reading via open(): (%d) -> %s\n",errno,strerror(errno));
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
		my_free((void **)&temp_buffer);

		return ERROR;
	        }

	/* re-open the FIFO for use with fgets() */
	if((command_file_fp=(FILE *)fdopen(command_file_fd,"r"))==NULL){

		asprintf(&temp_buffer,"Error: Could not open external command file for reading via fdopen(): (%d) -> %s\n",errno,strerror(errno));
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
		my_free((void **)&temp_buffer);

		return ERROR;
	        }

	/* initialize worker thread */
	if(init_command_file_worker_thread()==ERROR){

		asprintf(&temp_buffer,"Error: Could not initialize command file worker thread.\n");
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
		my_free((void **)&temp_buffer);

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
	register int x=0;
	register int y=0;
	register int z=0;

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
	/* save last position for later... */
	z=x;

	/* strip beginning of string (by shifting) */
	for(x=0;;x++){
		if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
			continue;
		else
			break;
	        }
	if(x>0){
		/* new length of the string after we stripped the end */
		y=z+1;
		
		/* shift chars towards beginning of string to remove leading whitespace */
		for(z=x;z<y;z++)
			buffer[z-x]=buffer[z];
		buffer[y-x]='\x0';
	        }

	return;
	}



/* determines whether or not an object name (host, service, etc) contains illegal characters */
int contains_illegal_object_chars(char *name){
	register int x=0;
	register int y=0;
	register int ch=0;

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
	register int x=0;
	register int y=0;
	register int z=0;
	register int ch=0;
	register int len=0;
	register int illegal_char=0;

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
	char *token_position=NULL;
	char *sequence_head=NULL;

	if(buffer!=NULL){
		my_free((void **)&original_my_strtok_buffer);
		if((my_strtok_buffer=(char *)strdup(buffer))==NULL)
			return NULL;
		original_my_strtok_buffer=my_strtok_buffer;
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
	char *begin=NULL;
	char *end=NULL;
	char ch='\x0';

	begin=*stringp;
	if(begin==NULL)
		return NULL;

	/* a frequent case is when the delimiter string contains only one character.  Here we don't need to call the expensive `strpbrk' function and instead work using `strchr'.  */
	if(delim[0]=='\0' || delim[1]=='\0'){

		ch=delim[0];

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
		/* find the end of the token.  */
		end=strpbrk(begin,delim);

	if(end){

		/* terminate the token and set *STRINGP past NUL character.  */
		*end++='\0';
		*stringp=end;
		}
	else
		/* no more delimiters; this is the last token.  */
		*stringp=NULL;

	return begin;
	}



/* my wrapper for free() */
int my_free(void **ptr){

	if(ptr==NULL)
		return ERROR;

	/* I hate calling free() and then resetting the pointer to NULL, so lets do it together */
	if(*ptr){
		free(*ptr);
		*ptr=NULL;
	        }

	return OK;
        }


/* encodes a string in proper URL format */
char *get_url_encoded_string(char *input){
	register int x=0;
	register int y=0;
	char *encoded_url_string=NULL;
	char temp_expansion[4]="";


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



/* compares strings */
int compare_strings(char *val1a, char *val2a){

	/* use the compare_hashdata() function */
	return compare_hashdata(val1a,NULL,val2a,NULL);
        }



/******************************************************************/
/************************* HASH FUNCTIONS *************************/
/******************************************************************/

/* dual hash function */
int hashfunc(const char *name1,const char *name2,int hashslots){
	unsigned int i=0;
	unsigned int result=0;

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


/* dual hash data comparison */
int compare_hashdata(const char *val1a, const char *val1b, const char *val2a, const char *val2b){
	int result=0;

	/* NOTE: If hash calculation changes, update the compare_strings() function! */

	/* check first name */
	if(val1a==NULL && val2a==NULL)
		result=0;
	else if(val1a==NULL)
		result=1;
	else if(val2a==NULL)
		result=-1;
	else
		result=strcmp(val1a,val2a);

	/* check second name if necessary */
	if(result==0){
		if(val1b==NULL && val2b==NULL)
			result=0;
		else if(val1b==NULL)
			result=1;
		else if(val2b==NULL)
			result=-1;
		else
			result=strcmp(val1b,val2b);
	        }

	return result;
        }



/******************************************************************/
/************************* FILE FUNCTIONS *************************/
/******************************************************************/

/* renames a file - works across filesystems (Mike Wiacek) */
int my_rename(char *source, char *dest){
	char buffer[MAX_INPUT_BUFFER]={0};
	int rename_result=0;
	int source_fd=-1;
	int dest_fd=-1;
	int bytes_read=0;


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
	mmapfile *new_mmapfile=NULL;
	int fd=0;
	void *mmap_buf=NULL;
	struct stat statbuf;
	int mode=O_RDONLY;
	unsigned long file_size=0L;

	if(filename==NULL)
		return NULL;

	/* allocate memory */
	if((new_mmapfile=(mmapfile *)malloc(sizeof(mmapfile)))==NULL)
		return NULL;

	/* open the file */
	if((fd=open(filename,mode))==-1){
		my_free((void **)&new_mmapfile);
		return NULL;
	        }

	/* get file info */
	if((fstat(fd,&statbuf))==-1){
		close(fd);
		my_free((void **)&new_mmapfile);
		return NULL;
	        }

	/* get file size */
	file_size=(unsigned long)statbuf.st_size;

	/* only mmap() if we have a file greater than 0 bytes */
	if(file_size>0){

		/* mmap() the file - allocate one extra byte for processing zero-byte files */
		if((mmap_buf=(void *)mmap(0,file_size,PROT_READ,MAP_PRIVATE,fd,0))==MAP_FAILED){
			close(fd);
			my_free((void **)&new_mmapfile);
			return NULL;
			}
		}
	else
		mmap_buf=NULL;

	/* populate struct info for later use */
	new_mmapfile->path=(char *)strdup(filename);
	new_mmapfile->fd=fd;
	new_mmapfile->file_size=(unsigned long)file_size;
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
	if(temp_mmapfile->file_size>0L)
		munmap(temp_mmapfile->mmap_buf,temp_mmapfile->file_size);

	/* close the file */
	close(temp_mmapfile->fd);

	/* free memory */
	my_free((void **)&temp_mmapfile->path);
	my_free((void **)&temp_mmapfile);
	
	return OK;
        }


/* gets one line of input from an mmap()'ed file */
char *mmap_fgets(mmapfile *temp_mmapfile){
	char *buf=NULL;
	unsigned long x=0L;
	int len=0;

	if(temp_mmapfile==NULL)
		return NULL;

	/* size of file is 0 bytes */
	if(temp_mmapfile->file_size==0L)
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
	int len=0;
	int len2=0;

	if(temp_mmapfile==NULL)
		return NULL;

	while(1){

		my_free((void **)&tempbuf);

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

	my_free((void **)&tempbuf);

	return buf;
        }




/******************************************************************/
/******************** DYNAMIC BUFFER FUNCTIONS ********************/
/******************************************************************/

/* initializes a dynamic buffer */
int dbuf_init(dbuf *db, int chunk_size){

	if(db==NULL)
		return ERROR;

	db->buf=NULL;
	db->used_size=0L;
	db->allocated_size=0L;
	db->chunk_size=chunk_size;

	return OK;
        }


/* frees a dynamic buffer */
int dbuf_free(dbuf *db){

	if(db==NULL)
		return ERROR;

	if(db->buf!=NULL)
		my_free((void **)&db->buf);
	db->buf=NULL;
	db->used_size=0L;
	db->allocated_size=0L;

	return OK;
        }


/* dynamically expands a string */
int dbuf_strcat(dbuf *db, char *buf){
	char *newbuf=NULL;
	unsigned long buflen=0L;
	unsigned long new_size=0L;
	unsigned long memory_needed=0L;

	if(db==NULL || buf==NULL)
		return ERROR;

	/* how much memory should we allocate (if any)? */
	buflen=strlen(buf);
	new_size=db->used_size+buflen+1;

	/* we need more memory */
	if(db->allocated_size<new_size){

		memory_needed=((ceil(new_size/db->chunk_size)+1)*db->chunk_size);

		/* allocate memory to store old and new string */
		if((newbuf=(char *)realloc((void *)db->buf,(size_t)memory_needed))==NULL)
			return ERROR;

		/* update buffer pointer */
		db->buf=newbuf;

		/* update allocated size */
		db->allocated_size=memory_needed;

		/* terminate buffer */
		db->buf[db->used_size]='\x0';
	        }

	/* append the new string */
	strcat(db->buf,buf);

	/* update size allocated */
	db->used_size+=buflen;

	return OK;
        }



/******************************************************************/
/******************** EMBEDDED PERL FUNCTIONS *********************/
/******************************************************************/

/* initializes embedded perl interpreter */
int init_embedded_perl(char **env){
#ifdef EMBEDDEDPERL
	char *embedding[]={ "", "" };
	int exitstatus=0;
	char *temp_buffer=NULL;
	int argc=2;
	struct stat stat_buf;

	/* make sure the P1 file exists... */
	if(p1_file==NULL || stat(p1_file,&stat_buf)!=0){

		use_embedded_perl=FALSE;

		asprintf(&temp_buffer,"Error: p1.pl file required for embedded Perl interpreter is missing!\n");
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
		my_free((void **)&temp_buffer);
		}

	else{

		embedding[1]=p1_file;

		use_embedded_perl=TRUE;

		PERL_SYS_INIT3(&argc,&embedding,&env);

		if((my_perl=perl_alloc())==NULL){
			use_embedded_perl=FALSE;
			asprintf(&temp_buffer,"Error: Could not allocate memory for embedded Perl interpreter!\n");
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_ERROR,TRUE);
			my_free((void **)&temp_buffer);
			}
		}

	/* a fatal error occurred... */
	if(use_embedded_perl==FALSE){

		asprintf(&temp_buffer,"Bailing out due to errors encountered while initializing the embedded Perl interpreter. (PID=%d)\n",(int)getpid());
		write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR,TRUE);
		my_free((void **)&temp_buffer);

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


/* checks to see if we should run a script using the embedded Perl interpreter */
int file_uses_embedded_perl(char *fname){
	int use_epn=FALSE;
#ifdef EMBEDDEDPERL
	FILE *fp=NULL;
	char line1[80]="";
	char linen[80]="";
	int line=0;
	char *ptr=NULL;
	int found_epn_directive=FALSE;

	if(enable_embedded_perl==TRUE){

		/* open the file, check if its a Perl script and see if we can use epn  */
		fp=fopen(fname,"r");
		if(fp!=NULL){

			/* grab the first line - we should see Perl */
			fgets(line1,80,fp);

			/* yep, its a Perl script... */
			if(strstr(line1,"/bin/perl")!=NULL){

				/* epn directives must be found in first ten lines of plugin */
				for(line=1;line<10;line++){

					if(fgets(linen,80,fp)){

						/* line contains Nagios directives */
						if(strstr(linen,"# nagios:")){

							ptr=strtok(linen,":");

							/* process each directive */
							for(ptr=strtok(NULL,",");ptr!=NULL;ptr=strtok(NULL,",")){

								if(!strcmp(ptr,"+epn")){
									use_epn=TRUE;
									found_epn_directive=TRUE;
									}
								else if(!strcmp(ptr,"-epn")){
									use_epn=FALSE;
									found_epn_directive=TRUE;
									}
								}
							}

						if(found_epn_directive==TRUE)
							break;
						}

					/* EOF */
					else
						break;
					}
					
				/* if the plugin didn't tell us whether or not to use embedded Perl, use implicit value */
				if(found_epn_directive==FALSE)
					use_epn=(use_embedded_perl_implicitly==TRUE)?TRUE:FALSE;
				}

			fclose(fp);
			}
		}
#endif

	return use_epn;
	}





/******************************************************************/
/************************ THREAD FUNCTIONS ************************/
/******************************************************************/

/* initializes service result worker thread */
int init_check_result_worker_thread(void){
	int result=0;
	sigset_t newmask;

	/* initialize circular buffer */
	check_result_buffer.head=0;
	check_result_buffer.tail=0;
	check_result_buffer.items=0;
	check_result_buffer.high=0;
	check_result_buffer.overflow=0L;
	check_result_buffer.buffer=(void **)malloc(check_result_buffer_slots*sizeof(check_result **));
	if(check_result_buffer.buffer==NULL)
		return ERROR;

	/* initialize mutex */
	pthread_mutex_init(&check_result_buffer.buffer_lock,NULL);

	/* new thread should block all signals */
	sigfillset(&newmask);
	pthread_sigmask(SIG_BLOCK,&newmask,NULL);

	/* create worker thread */
	result=pthread_create(&worker_threads[CHECK_RESULT_WORKER_THREAD],NULL,check_result_worker_thread,NULL);

	/* main thread should unblock all signals */
	pthread_sigmask(SIG_UNBLOCK,&newmask,NULL);

#ifdef DEBUG1
	printf("SERVICE CHECK THREAD: %lu\n",(unsigned long)worker_threads[CHECK_RESULT_WORKER_THREAD]);
#endif

	if(result)
		return ERROR;

	return OK;
        }


/* shutdown the service result worker thread */
int shutdown_check_result_worker_thread(void){

	/* tell the worker thread to exit */
	pthread_cancel(worker_threads[CHECK_RESULT_WORKER_THREAD]);

	/* wait for the worker thread to exit */
	pthread_join(worker_threads[CHECK_RESULT_WORKER_THREAD],NULL);

	return OK;
        }


/* clean up resources used by service result worker thread */
void cleanup_check_result_worker_thread(void *arg){
	register int x=0;

	/* release memory allocated to circular buffer */
	for(x=check_result_buffer.tail;x!=check_result_buffer.head;x=(x+1) % check_result_buffer_slots){
		my_free((void **)&((check_result **)check_result_buffer.buffer)[x]);
		((check_result **)check_result_buffer.buffer)[x]=NULL;
	        }
	my_free((void **)&check_result_buffer.buffer);

	/* free memory allocated to dynamic buffer */
	dbuf_free(&check_result_dbuf);

	return;
        }


/* initializes command file worker thread */
int init_command_file_worker_thread(void){
	int result=0;
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
	register int x=0;

	/* release memory allocated to circular buffer */
	for(x=external_command_buffer.tail;x!=external_command_buffer.head;x=(x+1) % external_command_buffer_slots){
		my_free((void **)&((char **)external_command_buffer.buffer)[x]);
		((char **)external_command_buffer.buffer)[x]=NULL;
	        }
	my_free((void **)&external_command_buffer.buffer);

	return;
        }


/* check result worker thread - artificially increases buffer of IPC pipe */
void * check_result_worker_thread(void *arg){	
	struct pollfd pfd;
	int pollval=0;
	struct timeval tv;
	int buffer_items=0;
	char buf[512];
	int dbuf_chunk=2048;
	int result=0;
	check_result info;

	/* specify cleanup routine */
	pthread_cleanup_push(cleanup_check_result_worker_thread,NULL);

	/* set cancellation info */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);

	/* initialize dynamic buffer (2KB chunk size) */
	dbuf_init(&check_result_dbuf,dbuf_chunk);

	/* clear the check result structure */
	memset((void *)&info,0,sizeof(check_result));

	/* initialize check result structure */
	info.object_check_type=SERVICE_CHECK;
	info.host_name=NULL;
	info.service_description=NULL;
	info.check_type=SERVICE_CHECK_ACTIVE;
	info.output_file=NULL;
	info.latency=0.0;
	info.start_time.tv_sec=0;
	info.start_time.tv_usec=0;
	info.finish_time.tv_sec=0;
	info.finish_time.tv_usec=0;
	info.early_timeout=FALSE;
	info.exited_ok=TRUE;
	info.return_code=STATE_OK;

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
				write_to_log("check_result_worker_thread(): poll(): EBADF",logging_options,NULL);
				break;
			case ENOMEM:
				write_to_log("check_result_worker_thread(): poll(): ENOMEM",logging_options,NULL);
				break;
			case EFAULT:
				write_to_log("check_result_worker_thread(): poll(): EFAULT",logging_options,NULL);
				break;
			case EINTR:
				/* this can happen when we're running Nagios under a debugger like gdb */
				/* REMOVED 03/11/2006 EG */
				/*
				write_to_log("check_result_worker_thread(): poll(): EINTR (impossible). Perhaps we're running under gdb?",logging_options,NULL);
				*/
				break;
			default:
				write_to_log("check_result_worker_thread(): poll(): Unknown errno value.",logging_options,NULL);
				break;
			        }

			continue;
		        }

		/* should we shutdown? */
		pthread_testcancel();

		/* get number of items in the buffer */
		pthread_mutex_lock(&check_result_buffer.buffer_lock);
		buffer_items=check_result_buffer.items;
		pthread_mutex_unlock(&check_result_buffer.buffer_lock);

		/* process data in the pipe (one message max) if there's some free space in the circular buffer */
		if(buffer_items<check_result_buffer_slots){

			/* read all data from client */
			while(1){

				result=read(ipc_pipe[0],buf,sizeof(buf)-1);

				/* handle errors */
				if(result==-1){

#ifdef DEBUG_CHECK_IPC
					switch(errno){
					case EAGAIN:
						printf("EAGAIN: %d\n",errno);
						break;
					case EINTR:
						printf("EINTR\n");
						break;
					case EIO:
						printf("EIO\n");
						break;
					case EINVAL:
						printf("EINVAL\n");
						break;
					case EBADF:
						printf("EBADF\n");
						break;
					default:
						printf("UNKNOWN: %d\n",errno);
						break;
					        }
#endif

					/* bail out on hard errors */
					if(errno!=EINTR)
						break;
					else
						continue;
				        }

				/* zero bytes read means we lost the connection */
				if(result==0)
					break;

				/* append data we just read to dynamic buffer */
				buf[result]='\x0';
				dbuf_strcat(&check_result_dbuf,buf);

				/* check for completed lines of input */
				handle_check_result_input1(&info,&check_result_dbuf);
                               }
		        }

		/* no free space in the buffer, so wait a bit */
		else{
			tv.tv_sec=0;
			tv.tv_usec=500;
			select(0,NULL,NULL,NULL,&tv);
		        }
	        }

	/* should never be reached... */

	/* removes cleanup handler */
	pthread_cleanup_pop(0);

	return NULL;
        }



/* top-level check result input handler */
int handle_check_result_input1(check_result *info, dbuf *db){
	char *buf=NULL;
	register int x=0;
	

	if(db==NULL)
		return OK;
	if(db->buf==NULL)
		return OK;

	/* search for complete lines of input */
	for(x=0;db->buf[x]!='\x0';x++){

		/* we found the end of a line */
		if(db->buf[x]=='\n'){

			/* handle this line of input */
			db->buf[x]='\x0';
			if((buf=(char *)strdup(db->buf))){

				handle_check_result_input2(info,buf);

				my_free((void **)&buf);
				buf=NULL;
			        }
		
			/* shift data back to front of buffer and adjust counters */
			memmove((void *)&db->buf[0],(void *)&db->buf[x+1],(size_t)((int)db->used_size-x-1));
			db->used_size-=(x+1);
			db->buf[db->used_size]='\x0';
			x=-1;
		        }
	        }

	return OK;
        }



/* low-level check result input handler */
int handle_check_result_input2(check_result *info, char *buf){
	char *var=NULL;
	char *val=NULL;
	char *ptr1=NULL;
	char *ptr2=NULL;

	if(info==NULL || buf==NULL)
		return ERROR;

	strip(buf);

	/* input is finished, so buffer this check result */
	if(!strcmp(buf,"")){

		/* buffer check result */
		buffer_check_result(info);

		/* reinitialize check result structure */
		info->object_check_type=SERVICE_CHECK;
		info->check_type=SERVICE_CHECK_ACTIVE;
		info->scheduled_check=FALSE;
		info->reschedule_check=FALSE;
		info->start_time.tv_sec=0;
		info->start_time.tv_usec=0;
		info->latency=0.0;
		info->finish_time.tv_sec=0;
		info->finish_time.tv_usec=0;
		info->early_timeout=FALSE;
		info->exited_ok=TRUE;
		info->return_code=STATE_OK;

		/* free check result memory */
		free_check_result(info);

		return OK;
	        }

	/* get var/val pair */
	if((var=strtok(buf,"="))==NULL)
		return ERROR;
	if((val=strtok(NULL,"\n"))==NULL)
		return ERROR;

	/* handle the data */
	switch(atoi(var)){
	case 1:
		info->object_check_type=atoi(val);
		break;
	case 2:
		info->host_name=(char *)strdup(val);
		break;
	case 3:
		info->service_description=(char *)strdup(val);
		break;
	case 4:
		info->check_type=atoi(val);
		break;
	case 5:
		info->scheduled_check=atoi(val);
		break;
	case 6:
		info->reschedule_check=atoi(val);
		break;
	case 7:
		info->output_file=(char *)strdup(val);
		break;
	case 8:
		info->latency=strtod(val,NULL);
		break;
	case 9:
		if((ptr1=strtok(val,"."))==NULL)
			return ERROR;
		if((ptr2=strtok(NULL,"\n"))==NULL)
			return ERROR;
		info->start_time.tv_sec=strtoul(ptr1,NULL,0);
		info->start_time.tv_usec=strtoul(ptr2,NULL,0);
		break;
	case 10:
		if((ptr1=strtok(val,"."))==NULL)
			return ERROR;
		if((ptr2=strtok(NULL,"\n"))==NULL)
			return ERROR;
		info->finish_time.tv_sec=strtoul(ptr1,NULL,0);
		info->finish_time.tv_usec=strtoul(ptr2,NULL,0);
		break;
	case 11:
		info->early_timeout=atoi(val);
		break;
	case 12:
		info->exited_ok=atoi(val);
		break;
	case 13:
		info->return_code=atoi(val);
		break;
	default:
		return ERROR;
	        }

	return OK;
        }



/* saves a host/service check result info structure to the circular buffer */
int buffer_check_result(check_result *info){
	check_result *new_info=NULL;

	if(info==NULL)
		return ERROR;

#ifdef DEBUG_CHECK_IPC
	printf("BUFFERING CHECK RESULT FOR: HOST=%s, SVC=%s\n",(info->host_name==NULL)?"":info->host_name,(info->service_description==NULL)?"":info->service_description);
#endif

	/* allocate memory for the result */
	if((new_info=(check_result *)malloc(sizeof(check_result)))==NULL)
		return ERROR;
	
	/* copy the original check result info */
	memcpy(new_info,info,sizeof(check_result));
	new_info->host_name=NULL;
	if(info->host_name)
		new_info->host_name=(char *)strdup(info->host_name);
	new_info->service_description=NULL;
	if(info->service_description)
		new_info->service_description=(char *)strdup(info->service_description);
	new_info->output_file=NULL;
	if(info->output_file)
		new_info->output_file=(char *)strdup(info->output_file);

	/* obtain a lock for writing to the buffer */
	pthread_mutex_lock(&check_result_buffer.buffer_lock);

	/* handle overflow conditions */
	/* NOTE: This should never happen - child processes will instead block trying to write messages to the pipe... */
	if(check_result_buffer.items==check_result_buffer_slots){

		/* record overflow */
		check_result_buffer.overflow++;

		/* update tail pointer */
		check_result_buffer.tail=(check_result_buffer.tail + 1) % check_result_buffer_slots;
	        }

	/* save the data to the buffer */
	((check_result **)check_result_buffer.buffer)[check_result_buffer.head]=new_info;

	/* increment the head counter and items */
	check_result_buffer.head=(check_result_buffer.head + 1) % check_result_buffer_slots;
	if(check_result_buffer.items<check_result_buffer_slots)
		check_result_buffer.items++;
	if(check_result_buffer.items>check_result_buffer.high)
		check_result_buffer.high=check_result_buffer.items;
	
#ifdef DEBUG_CHECK_IPC
	printf("BUFFER OK.  TOTAL ITEMS=%d\n",check_result_buffer.items);
#endif

	/* release lock on buffer */
	pthread_mutex_unlock(&check_result_buffer.buffer_lock);

	return OK;
        }



/* worker thread - artificially increases buffer of named pipe */
void * command_file_worker_thread(void *arg){
	char input_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;
	int buffer_items=0;
	int result=0;

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
				printf("(CFWT) RES: %d, BUFFER_ITEMS: %d/%d\n",result,buffer_items,external_comand_buffer_slots);
#endif

				/* bail if the circular buffer is full */
				if(buffer_items==external_command_buffer_slots)
					break;

				/* should we shutdown? */
				pthread_testcancel();
	                        }
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
		((char **)external_command_buffer.buffer)[external_command_buffer.head]=(char *)strdup(cmd);

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
	int result=OK;
	time_t timestamp;

	if(cmd==NULL)
		return ERROR;

	/* get the time */
	if(ts!=NULL)
		timestamp=*ts;
	else
		time(&timestamp);

	/* create the command string */
	asprintf(&newcmd,"[%lu] %s",(unsigned long)timestamp,cmd);

	/* submit the command */
	result=submit_external_command(newcmd,buffer_items);

	/* free allocated memory */
	my_free((void **)&newcmd);

	return result;
        }



/******************************************************************/
/********************** CHECK STATS FUNCTIONS *********************/
/******************************************************************/

/* initialize check statistics data structures */
int init_check_stats(void){
	int x=0;
	int y=0;

	for(x=0;x<MAX_CHECK_STATS_TYPES;x++){
		check_statistics[x].current_bucket=0;
		for(y=0;y<CHECK_STATS_BUCKETS;y++)
			check_statistics[x].bucket[y]=0;
		check_statistics[x].overflow_bucket=0;
		for(y=0;y<3;y++)
			check_statistics[x].minute_stats[y]=0;
		check_statistics[x].last_update=(time_t)0L;
		}

	return OK;
	}


/* records stats for a given type of check */
int update_check_stats(int check_type, time_t check_time){
	time_t current_time;
	unsigned long minutes=0L;
	int new_current_bucket=0;
	int this_bucket=0;
	int x=0;
	
	if(check_type<0 || check_type>=MAX_CHECK_STATS_TYPES)
		return ERROR;

	time(&current_time);

	if((unsigned long)check_time==0L){
#ifdef DEBUG_CHECK_STATS
		printf("TYPE[%d] CHECK TIME==0!\n",check_type);
#endif
		check_time=current_time;
		}

	/* do some sanity checks on the age of the stats data before we start... */
	/* get the new current bucket number */
	minutes=((unsigned long)check_time-(unsigned long)program_start) / 60;
	new_current_bucket=minutes % CHECK_STATS_BUCKETS;

	/* its been more than 15 minutes since stats were updated, so clear the stats */
	if((((unsigned long)current_time - (unsigned long)check_statistics[check_type].last_update) / 60) > CHECK_STATS_BUCKETS){
		for(x=0;x<CHECK_STATS_BUCKETS;x++)
			check_statistics[check_type].bucket[x]=0;
		check_statistics[check_type].overflow_bucket=0;
#ifdef DEBUG_CHECK_STATS
		printf("CLEARING ALL: TYPE[%d], CURRENT=%lu, LASTUPDATE=%lu\n",check_type,(unsigned long)current_time,(unsigned long)check_statistics[check_type].last_update);
#endif
		}

	/* different current bucket number than last time */
	else if(new_current_bucket!=check_statistics[check_type].current_bucket){

		/* clear stats in buckets between last current bucket and new current bucket - stats haven't been updated in a while */
		for(x=check_statistics[check_type].current_bucket;x<(CHECK_STATS_BUCKETS * 2);x++){

			this_bucket=(x + CHECK_STATS_BUCKETS + 1) % CHECK_STATS_BUCKETS;
			
			if(this_bucket==new_current_bucket)
				break;
	
#ifdef DEBUG_CHECK_STATS			
			printf("CLEARING BUCKET %d, (NEW=%d, OLD=%d)\n",this_bucket,new_current_bucket,check_statistics[check_type].current_bucket);
#endif

			/* clear old bucket value */
			check_statistics[check_type].bucket[this_bucket]=0;
			}

		/* update the current bucket number, push old value to overflow bucket */
		check_statistics[check_type].overflow_bucket=check_statistics[check_type].bucket[new_current_bucket];
		check_statistics[check_type].current_bucket=new_current_bucket;
		check_statistics[check_type].bucket[new_current_bucket]=0;
		}
#ifdef DEBUG_CHECK_STATS
	else
		printf("NO CLEARING NEEDED\n");
#endif


	/* increment the value of the current bucket */
	check_statistics[check_type].bucket[new_current_bucket]++;

#ifdef DEBUG_CHECK_STATS
	printf("TYPE[%d].BUCKET[%d]=%d\n",check_type,new_current_bucket,check_statistics[check_type].bucket[new_current_bucket]);
	printf("   ");
	for(x=0;x<CHECK_STATS_BUCKETS;x++)
		printf("[%d] ",check_statistics[check_type].bucket[x]);
	printf(" (%d)\n",check_statistics[check_type].overflow_bucket);
#endif

	/* record last update time */
	check_statistics[check_type].last_update=current_time;

	return OK;
	}


/* generate 1/5/15 minute stats for a given type of check */
int generate_check_stats(void){
	time_t current_time;
	int x=0;
	int new_current_bucket=0;
	int this_bucket=0;
	int last_bucket=0;
	int this_bucket_value=0;
	int last_bucket_value=0;
	int bucket_value=0;
	int seconds=0;
	int minutes=0;
	int check_type=0;
	float this_bucket_weight=0.0;
	float last_bucket_weight=0.0;
	int left_value=0;
	int right_value=0;


	time(&current_time);

	/* do some sanity checks on the age of the stats data before we start... */
	/* get the new current bucket number */
	minutes=((unsigned long)current_time-(unsigned long)program_start) / 60;
	new_current_bucket=minutes % CHECK_STATS_BUCKETS;
	for(check_type=0;check_type<MAX_CHECK_STATS_TYPES;check_type++){

		/* its been more than 15 minutes since stats were updated, so clear the stats */
		if((((unsigned long)current_time - (unsigned long)check_statistics[check_type].last_update) / 60) > CHECK_STATS_BUCKETS){
			for(x=0;x<CHECK_STATS_BUCKETS;x++)
				check_statistics[check_type].bucket[x]=0;
			check_statistics[check_type].overflow_bucket=0;
#ifdef DEBUG_CHECK_STATS
			printf("GEN CLEARING ALL: TYPE[%d], CURRENT=%lu, LASTUPDATE=%lu\n",check_type,(unsigned long)current_time,(unsigned long)check_statistics[check_type].last_update);
#endif
			}

		/* different current bucket number than last time */
		else if(new_current_bucket!=check_statistics[check_type].current_bucket){

			/* clear stats in buckets between last current bucket and new current bucket - stats haven't been updated in a while */
			for(x=check_statistics[check_type].current_bucket;x<(CHECK_STATS_BUCKETS*2);x++){

				this_bucket=(x + CHECK_STATS_BUCKETS + 1) % CHECK_STATS_BUCKETS;

				if(this_bucket==new_current_bucket)
					break;
				
#ifdef DEBUG_CHECK_STATS			
				printf("GEN CLEARING BUCKET %d, (NEW=%d, OLD=%d), CURRENT=%lu, LASTUPDATE=%lu\n",this_bucket,new_current_bucket,check_statistics[check_type].current_bucket,(unsigned long)current_time,(unsigned long)check_statistics[check_type].last_update);
#endif

				/* clear old bucket value */
				check_statistics[check_type].bucket[this_bucket]=0;
				}

			/* update the current bucket number, push old value to overflow bucket */
			check_statistics[check_type].overflow_bucket=check_statistics[check_type].bucket[new_current_bucket];
			check_statistics[check_type].current_bucket=new_current_bucket;
			check_statistics[check_type].bucket[new_current_bucket]=0;
			}
#ifdef DEBUG_CHECK_STATS
		else
			printf("GEN NO CLEARING NEEDED: TYPE[%d], CURRENT=%lu, LASTUPDATE=%lu\n",check_type,(unsigned long)current_time,(unsigned long)check_statistics[check_type].last_update);
#endif

		/* update last check time */
		check_statistics[check_type].last_update=current_time;
		}

	/* determine weights to use for this/last buckets */
	seconds=((unsigned long)current_time-(unsigned long)program_start) % 60;
	this_bucket_weight=(seconds/60.0);
	last_bucket_weight=((60-seconds)/60.0);

	/* update statistics for all check types */
	for(check_type=0;check_type<MAX_CHECK_STATS_TYPES;check_type++){

		/* clear the old statistics */
		for(x=0;x<3;x++)
			check_statistics[check_type].minute_stats[x]=0;

		/* loop through each bucket */
		for(x=0;x<CHECK_STATS_BUCKETS;x++){
			
			/* which buckets should we use for this/last bucket? */
			this_bucket=(check_statistics[check_type].current_bucket + CHECK_STATS_BUCKETS - x) % CHECK_STATS_BUCKETS;
			last_bucket=(this_bucket + CHECK_STATS_BUCKETS - 1) % CHECK_STATS_BUCKETS;

			/* raw/unweighted value for this bucket */
			this_bucket_value=check_statistics[check_type].bucket[this_bucket];

			/* raw/unweighted value for last bucket - use overflow bucket if last bucket is current bucket */
			if(last_bucket==check_statistics[check_type].current_bucket)
				last_bucket_value=check_statistics[check_type].overflow_bucket;
			else
				last_bucket_value=check_statistics[check_type].bucket[last_bucket];

			/* determine value by weighting this/last buckets... */
			/* if this is the current bucket, use its full value + weighted % of last bucket */
			if(x==0){
				right_value=this_bucket_value;
				left_value=(int)round(last_bucket_value * last_bucket_weight);
				bucket_value=(int)(this_bucket_value + round(last_bucket_value * last_bucket_weight));
				}
			/* otherwise use weighted % of this and last bucket */
			else{
				right_value=(int)round(this_bucket_value * this_bucket_weight);
				left_value=(int)round(last_bucket_value * last_bucket_weight);
				bucket_value=(int)(round(this_bucket_value * this_bucket_weight) + round(last_bucket_value * last_bucket_weight));
				}

			/* 1 minute stats */
			if(x==0)
				check_statistics[check_type].minute_stats[0]=bucket_value;
		
			/* 5 minute stats */
			if(x<5)
				check_statistics[check_type].minute_stats[1]+=bucket_value;

			/* 15 minute stats */
			if(x<15)
				check_statistics[check_type].minute_stats[2]+=bucket_value;

#ifdef DEBUG_CHECK_STATS2
			printf("X=%d, THIS[%d]=%d, LAST[%d]=%d, 1/5/15=%d,%d,%d  L=%d R=%d\n",x,this_bucket,this_bucket_value,last_bucket,last_bucket_value,check_statistics[check_type].minute_stats[0],check_statistics[check_type].minute_stats[1],check_statistics[check_type].minute_stats[2],left_value,right_value);
#endif
			/* record last update time */
			check_statistics[check_type].last_update=current_time;
			}

#ifdef DEBUG_CHECK_STATS
		printf("TYPE[%d]   1/5/15 = %d, %d, %d (seconds=%d, this_weight=%f, last_weight=%f)\n",check_type,check_statistics[check_type].minute_stats[0],check_statistics[check_type].minute_stats[1],check_statistics[check_type].minute_stats[2],seconds,this_bucket_weight,last_bucket_weight);
#endif
		}

	return OK;
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
	register int x=0;

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
		my_free((void **)&this_event);
		this_event=next_event;
	        }

	/* reset the event pointer */
	event_list_high=NULL;

	/* free memory for the low priority event list */
	this_event=event_list_low;
	while(this_event!=NULL){
		next_event=this_event->next;
		my_free((void **)&this_event);
		this_event=next_event;
	        }

	/* reset the event pointer */
	event_list_low=NULL;

	/* free memory used by my_strtok() function and reset the my_strtok() buffers */
	my_free((void **)&original_my_strtok_buffer);
	my_strtok_buffer=NULL;

	/* free memory for global event handlers */
	my_free((void **)&global_host_event_handler);
	my_free((void **)&global_service_event_handler);

	/* free any notification list that may have been overlooked */
	free_notification_list();

	/* free obsessive compulsive commands */
	my_free((void **)&ocsp_command);
	my_free((void **)&ochp_command);

	/* free memory associated with macros */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++)
		my_free((void **)&macro_argv[x]);

	for(x=0;x<MAX_USER_MACROS;x++)
		my_free((void **)&macro_user[x]);

	for(x=0;x<MACRO_X_COUNT;x++)
		my_free((void **)&macro_x[x]);

	free_macrox_names();

	/* free illegal char strings */
	my_free((void **)&illegal_object_chars);
	my_free((void **)&illegal_output_chars);

	/* free nagios user and group */
	my_free((void **)&nagios_user);
	my_free((void **)&nagios_group);

	/* free file/path variables */
	my_free((void **)&log_file);
	my_free((void **)&temp_file);
	my_free((void **)&temp_path);
	my_free((void **)&command_file);
	my_free((void **)&lock_file);
	my_free((void **)&auth_file);
	my_free((void **)&p1_file);
	my_free((void **)&log_archive_path);

#ifdef DEBUG0
	printf("free_memory() end\n");
#endif

	return;
	}


/* free a notification list that was created */
void free_notification_list(void){
	notification *temp_notification=NULL;
	notification *next_notification=NULL;

#ifdef DEBUG0
	printf("free_notification_list() start\n");
#endif

	temp_notification=notification_list;
	while(temp_notification!=NULL){
		next_notification=temp_notification->next;
		my_free((void **)&temp_notification);
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
	register int x=0;

#ifdef DEBUG0
	printf("reset_variables() start\n");
#endif

	log_file=(char *)strdup(DEFAULT_LOG_FILE);
	temp_file=(char *)strdup(DEFAULT_TEMP_FILE);
	temp_path=(char *)strdup(DEFAULT_TEMP_PATH);
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
	use_old_host_check_logic=DEFAULT_USE_OLD_HOST_CHECK_LOGIC;
	cached_host_check_horizon=DEFAULT_CACHED_HOST_CHECK_HORIZON;
	cached_service_check_horizon=DEFAULT_CACHED_SERVICE_CHECK_HORIZON;
	enable_predictive_host_dependency_checks=DEFAULT_ENABLE_PREDICTIVE_HOST_DEPENDENCY_CHECKS;
	enable_predictive_service_dependency_checks=DEFAULT_ENABLE_PREDICTIVE_SERVICE_DEPENDENCY_CHECKS;

	soft_state_dependencies=FALSE;

	retain_state_information=FALSE;
	retention_update_interval=DEFAULT_RETENTION_UPDATE_INTERVAL;
	use_retained_program_state=TRUE;
	use_retained_scheduling_info=FALSE;
	retention_scheduling_horizon=DEFAULT_RETENTION_SCHEDULING_HORIZON;
	modified_host_process_attributes=MODATTR_NONE;
	modified_service_process_attributes=MODATTR_NONE;
	retained_host_attribute_mask=0L;
	retained_service_attribute_mask=0L;
	retained_process_host_attribute_mask=0L;
	retained_process_service_attribute_mask=0L;
	retained_contact_host_attribute_mask=0L;
	retained_contact_service_attribute_mask=0L;

	command_check_interval=DEFAULT_COMMAND_CHECK_INTERVAL;
	check_reaper_interval=DEFAULT_CHECK_REAPER_INTERVAL;
	service_freshness_check_interval=DEFAULT_FRESHNESS_CHECK_INTERVAL;
	host_freshness_check_interval=DEFAULT_FRESHNESS_CHECK_INTERVAL;
	auto_rescheduling_interval=DEFAULT_AUTO_RESCHEDULING_INTERVAL;
	auto_rescheduling_window=DEFAULT_AUTO_RESCHEDULING_WINDOW;

	check_external_commands=DEFAULT_CHECK_EXTERNAL_COMMANDS;
	check_orphaned_services=DEFAULT_CHECK_ORPHANED_SERVICES;
	check_orphaned_hosts=DEFAULT_CHECK_ORPHANED_HOSTS;
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

	next_comment_id=0L;  /* comment and downtime id get initialized to nonzero elsewhere */
	next_downtime_id=0L;
	next_event_id=1;
	next_notification_id=1;

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

	use_large_installation_tweaks=DEFAULT_USE_LARGE_INSTALLATION_TWEAKS;

        enable_embedded_perl=DEFAULT_ENABLE_EMBEDDED_PERL;
	use_embedded_perl_implicitly=DEFAULT_USE_EMBEDDED_PERL_IMPLICITLY;

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
	global_host_event_handler_ptr=NULL;
	global_service_event_handler_ptr=NULL;

	ocsp_command=NULL;
	ochp_command=NULL;
	ocsp_command_ptr=NULL;
	ochp_command_ptr=NULL;

	/* reset umask */
	umask(S_IWGRP|S_IWOTH);

#ifdef DEBUG0
	printf("reset_variables() end\n");
#endif

	return OK;
        }
