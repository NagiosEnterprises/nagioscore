/*****************************************************************************
 *
 * UTILS.C - Miscellaneous utility functions for Nagios
 *
 * Copyright (c) 1999-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   08-27-2003
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

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"
#include "../include/comments.h"
#include "../include/nagios.h"
#include "../include/broker.h"
#include "../include/nebmods.h"
#include "../include/nebmodules.h"

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
 
#ifdef HAVE_GRP_H
#include <grp.h>
#endif

#ifdef EMBEDDEDPERL
#include <EXTERN.h>
#include <perl.h>
static PerlInterpreter *my_perl;
#include <fcntl.h>
/* In perl.h (or friends) there is a macro that defines sighandler as Perl_sighandler, so we must #undef it so we can use our sighandler() function */
#undef sighandler
/* and we don't need perl's reentrant versions */
#undef localtime
#undef getpwnam
#undef getgrnam
#undef strerror
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
extern int      ochp_timeout;

extern int      log_initial_states;

extern double   sleep_time;
extern int      interval_length;
extern int      service_inter_check_delay_method;
extern int      host_inter_check_delay_method;
extern int      service_interleave_factor_method;

extern int      command_check_interval;
extern int      service_check_reaper_interval;
extern int      service_freshness_check_interval;
extern int      host_freshness_check_interval;

extern int      check_external_commands;
extern int      check_orphaned_services;
extern int      check_service_freshness;
extern int      check_host_freshness;

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

extern int      event_broker_options;

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
extern hostgroup	*hostgroup_list;
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

extern int errno;



/******** BEGIN EMBEDDED PERL INTERPRETER DECLARATIONS ********/

#ifdef EMBEDDEDPERL 
#include <EXTERN.h>
#include <perl.h>
static PerlInterpreter *my_perl;
#include <fcntl.h>
#undef ctime   /* don't need perl's threaded version */
#undef printf   /* can't use perl's printf until initialized */

/* include PERL xs_init code for module and C library support */

#if defined(__cplusplus) && !defined(PERL_OBJECT)
#define is_cplusplus
#endif

#ifdef is_cplusplus
extern "C" {
#endif

#ifdef PERL_OBJECT
#define NO_XSLOCKS
#include <XSUB.h>
#include "win32iop.h"
#include <perlhost.h>
#endif
#ifdef is_cplusplus
}
#  ifndef EXTERN_C
#    define EXTERN_C extern "C"
#  endif
#else
#  ifndef EXTERN_C
#    define EXTERN_C extern
#  endif
#endif

#ifdef THREADEDPERL
EXTERN_C void xs_init _((pTHX));
EXTERN_C void boot_DynaLoader _((pTHX_ CV* cv));
#else
EXTERN_C void xs_init _((void));
EXTERN_C void boot_DynaLoader _((CV* cv));
#endif

#ifdef THREADEDPERL
EXTERN_C void xs_init(pTHX)
#else
EXTERN_C void xs_init(void)
#endif
{
	char *file = __FILE__;
	dXSUB_SYS;
	/* DynaLoader is a special case */
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
        }

static PerlInterpreter *perl=NULL;

int embedded_perl_calls=0;
int use_embedded_perl=TRUE;
#endif

/******** END EMBEDDED PERL INTERPRETER DECLARATIONS ********/




/******************************************************************/
/************************ MACRO FUNCTIONS *************************/
/******************************************************************/

/* replace macros in notification commands with their values */
int process_macros(char *input_buffer,char *output_buffer,int buffer_length,int options){
	char *temp_buffer;
	int in_macro;
	int arg_index=0;
	int user_index=0;
	int address_index=0;
	char *selected_macro=NULL;
	int clean_macro=FALSE;

#ifdef DEBUG0
	printf("process_macros() start\n");
#endif

	strcpy(output_buffer,"");

	in_macro=FALSE;

	for(temp_buffer=my_strtok(input_buffer,"$");temp_buffer!=NULL;temp_buffer=my_strtok(NULL,"$")){

		selected_macro=NULL;
		clean_macro=FALSE;

		if(in_macro==FALSE){
			if(strlen(output_buffer)+strlen(temp_buffer)<buffer_length-1){
				strncat(output_buffer,temp_buffer,buffer_length-strlen(output_buffer)-1);
				output_buffer[buffer_length-1]='\x0';
			        }
			in_macro=TRUE;
			}
		else{

			if(strlen(output_buffer)+strlen(temp_buffer)<buffer_length-1){

				/*******************/
				/*** HOST MACROS ***/
				/*******************/

				if(!strcmp(temp_buffer,"HOSTNAME"))
					selected_macro=macro_x[MACRO_HOSTNAME];

				else if(!strcmp(temp_buffer,"HOSTALIAS"))
					selected_macro=macro_x[MACRO_HOSTALIAS];

				else if(!strcmp(temp_buffer,"HOSTADDRESS"))
					selected_macro=macro_x[MACRO_HOSTADDRESS];

				else if(!strcmp(temp_buffer,"LASTHOSTCHECK"))
					selected_macro=macro_x[MACRO_LASTHOSTCHECK];

				else if(!strcmp(temp_buffer,"LASTHOSTSTATECHANGE"))
					selected_macro=macro_x[MACRO_LASTHOSTSTATECHANGE];

				else if(!strcmp(temp_buffer,"HOSTOUTPUT")){
					selected_macro=macro_x[MACRO_HOSTOUTPUT];
					clean_macro=TRUE;
				        }

				else if(!strcmp(temp_buffer,"HOSTPERFDATA")){
					selected_macro=macro_x[MACRO_HOSTPERFDATA];
					clean_macro=TRUE;
				        }

				else if(!strcmp(temp_buffer,"HOSTACKAUTHOR"))
					selected_macro=macro_x[MACRO_HOSTACKAUTHOR];

				else if(!strcmp(temp_buffer,"HOSTACKCOMMENT"))
					selected_macro=macro_x[MACRO_HOSTACKCOMMENT];

				else if(!strcmp(temp_buffer,"HOSTSTATE"))
					selected_macro=macro_x[MACRO_HOSTSTATE];

				else if(!strcmp(temp_buffer,"HOSTSTATEID"))
					selected_macro=macro_x[MACRO_HOSTSTATEID];

				else if(!strcmp(temp_buffer,"HOSTATTEMPT"))
					selected_macro=macro_x[MACRO_HOSTATTEMPT];

				else if(!strcmp(temp_buffer,"HOSTSTATETYPE"))
					selected_macro=macro_x[MACRO_HOSTSTATETYPE];

				else if(!strcmp(temp_buffer,"HOSTLATENCY"))
					selected_macro=macro_x[MACRO_HOSTLATENCY];

				else if(!strcmp(temp_buffer,"HOSTDURATION"))
					selected_macro=macro_x[MACRO_HOSTDURATION];

				else if(!strcmp(temp_buffer,"HOSTEXECUTIONTIME"))
					selected_macro=macro_x[MACRO_HOSTEXECUTIONTIME];

				else if(!strcmp(temp_buffer,"HOSTDURATIONSEC"))
					selected_macro=macro_x[MACRO_HOSTDURATIONSEC];

				else if(!strcmp(temp_buffer,"HOSTDOWNTIME"))
					selected_macro=macro_x[MACRO_HOSTDOWNTIME];

				else if(!strcmp(temp_buffer,"HOSTPERCENTCHANGE"))
					selected_macro=macro_x[MACRO_HOSTPERCENTCHANGE];

				else if(!strcmp(temp_buffer,"HOSTGROUPNAME"))
					selected_macro=macro_x[MACRO_HOSTGROUPNAME];

				else if(!strcmp(temp_buffer,"HOSTGROUPALIAS"))
					selected_macro=macro_x[MACRO_HOSTGROUPALIAS];

				/* on-demand host macros */
				else if(strstr(temp_buffer,"HOST") && strstr(temp_buffer,":")){
					grab_on_demand_macro(temp_buffer);
					selected_macro=macro_ondemand;
				        }

				/**********************/
				/*** SERVICE MACROS ***/
				/**********************/

				else if(!strcmp(temp_buffer,"SERVICEDESC"))
					selected_macro=macro_x[MACRO_SERVICEDESC];

				else if(!strcmp(temp_buffer,"SERVICESTATE"))
					selected_macro=macro_x[MACRO_SERVICESTATE];

				else if(!strcmp(temp_buffer,"SERVICESTATEID"))
					selected_macro=macro_x[MACRO_SERVICESTATEID];

				else if(!strcmp(temp_buffer,"SERVICEATTEMPT"))
					selected_macro=macro_x[MACRO_SERVICEATTEMPT];

				else if(!strcmp(temp_buffer,"LASTSERVICECHECK"))
					selected_macro=macro_x[MACRO_LASTSERVICECHECK];

				else if(!strcmp(temp_buffer,"LASTSERVICESTATECHANGE"))
					selected_macro=macro_x[MACRO_LASTSERVICESTATECHANGE];

				else if(!strcmp(temp_buffer,"SERVICEOUTPUT")){
					selected_macro=macro_x[MACRO_SERVICEOUTPUT];
					clean_macro=TRUE;
				        }

				else if(!strcmp(temp_buffer,"SERVICEPERFDATA")){
					selected_macro=macro_x[MACRO_SERVICEPERFDATA];
					clean_macro=TRUE;
				        }

				else if(!strcmp(temp_buffer,"SERVICEACKAUTHOR"))
					selected_macro=macro_x[MACRO_SERVICEACKAUTHOR];

				else if(!strcmp(temp_buffer,"SERVICEACKCOMMENT"))
					selected_macro=macro_x[MACRO_SERVICEACKCOMMENT];

				else if(!strcmp(temp_buffer,"SERVICESTATETYPE"))
					selected_macro=macro_x[MACRO_SERVICESTATETYPE];

				else if(!strcmp(temp_buffer,"SERVICEEXECUTIONTIME"))
					selected_macro=macro_x[MACRO_SERVICEEXECUTIONTIME];

				else if(!strcmp(temp_buffer,"SERVICELATENCY"))
					selected_macro=macro_x[MACRO_SERVICELATENCY];

				else if(!strcmp(temp_buffer,"SERVICEDURATION"))
					selected_macro=macro_x[MACRO_SERVICEDURATION];

				else if(!strcmp(temp_buffer,"SERVICEDURATIONSEC"))
					selected_macro=macro_x[MACRO_SERVICEDURATIONSEC];

				else if(!strcmp(temp_buffer,"SERVICEPERCENTCHANGE"))
					selected_macro=macro_x[MACRO_SERVICEPERCENTCHANGE];

				else if(!strcmp(temp_buffer,"SERVICEGROUPNAME"))
					selected_macro=macro_x[MACRO_SERVICEGROUPNAME];

				else if(!strcmp(temp_buffer,"SERVICEGROUPALIAS"))
					selected_macro=macro_x[MACRO_SERVICEGROUPALIAS];

				else if(!strcmp(temp_buffer,"SERVICEDOWNTIME"))
					selected_macro=macro_x[MACRO_SERVICEDOWNTIME];

				/* on-demand service macros */
				else if(strstr(temp_buffer,"SERVICE") && strstr(temp_buffer,":")){
					grab_on_demand_macro(temp_buffer);
					selected_macro=macro_ondemand;
				        }

				/**********************/
				/*** CONTACT MACROS ***/
				/**********************/

				else if(!strcmp(temp_buffer,"CONTACTNAME"))
					selected_macro=macro_x[MACRO_CONTACTNAME];

				else if(!strcmp(temp_buffer,"CONTACTALIAS"))
					selected_macro=macro_x[MACRO_CONTACTALIAS];

				else if(!strcmp(temp_buffer,"CONTACTEMAIL"))
					selected_macro=macro_x[MACRO_CONTACTEMAIL];

				else if(!strcmp(temp_buffer,"CONTACTPAGER"))
					selected_macro=macro_x[MACRO_CONTACTPAGER];

				else if(strstr(temp_buffer,"CONTACTADDRESS")==temp_buffer){
					address_index=atoi(temp_buffer+14);
					if(address_index>=1 && address_index<=MAX_CONTACT_ADDRESSES)
						selected_macro=macro_contactaddress[address_index-1];
					else
						selected_macro=NULL;
				        }
			
				/***************************/
				/*** NOTIFICATION MACROS ***/
				/***************************/

				else if(!strcmp(temp_buffer,"NOTIFICATIONTYPE"))
					selected_macro=macro_x[MACRO_NOTIFICATIONTYPE];

				else if(!strcmp(temp_buffer,"NOTIFICATIONNUMBER"))
					selected_macro=macro_x[MACRO_NOTIFICATIONNUMBER];

				/**********************/
				/*** GENERIC MACROS ***/
				/**********************/

				else if(!strcmp(temp_buffer,"DATETIME") || !strcmp(temp_buffer,"LONGDATETIME"))
					selected_macro=macro_x[MACRO_LONGDATETIME];

				else if(!strcmp(temp_buffer,"SHORTDATETIME"))
					selected_macro=macro_x[MACRO_SHORTDATETIME];

				else if(!strcmp(temp_buffer,"DATE"))
					selected_macro=macro_x[MACRO_DATE];

				else if(!strcmp(temp_buffer,"TIME"))
					selected_macro=macro_x[MACRO_TIME];

				else if(!strcmp(temp_buffer,"TIMET"))
					selected_macro=macro_x[MACRO_TIMET];

				else if(!strcmp(temp_buffer,"ADMINEMAIL"))
					selected_macro=macro_x[MACRO_ADMINEMAIL];

				else if(!strcmp(temp_buffer,"ADMINPAGER"))
					selected_macro=macro_x[MACRO_ADMINPAGER];

				else if(strstr(temp_buffer,"ARG")==temp_buffer){
					arg_index=atoi(temp_buffer+3);
					if(arg_index>=1 && arg_index<=MAX_COMMAND_ARGUMENTS)
						selected_macro=macro_argv[arg_index-1];
					else
						selected_macro=NULL;
				        }

				else if(strstr(temp_buffer,"USER")==temp_buffer){
					user_index=atoi(temp_buffer+4);
					if(user_index>=1 && user_index<=MAX_USER_MACROS)
						selected_macro=macro_user[user_index-1];
					else
						selected_macro=NULL;
				        }

				/* an escaped $ is done by specifying two $$ next to each other */
				else if(!strcmp(temp_buffer,"")){
					strncat(output_buffer,"$",buffer_length-strlen(output_buffer)-1);
				        }

				/* a non-macro, just some user-defined string between two $s */
				else{
					strncat(output_buffer,"$",buffer_length-strlen(output_buffer)-1);
					output_buffer[buffer_length-1]='\x0';
					strncat(output_buffer,temp_buffer,buffer_length-strlen(output_buffer)-1);
					output_buffer[buffer_length-1]='\x0';
					strncat(output_buffer,"$",buffer_length-strlen(output_buffer)-1);
				        }
				
				/* insert macro */
				if(selected_macro!=NULL){

					/* $xOUTPUT$ and $xPERFDATA$ macros are cleaned */
					if(clean_macro==TRUE)
						strncat(output_buffer,(selected_macro==NULL)?"":clean_macro_chars(selected_macro,options),buffer_length-strlen(output_buffer)-1);

					/* normal macros as not cleaned */
					else
						strncat(output_buffer,(selected_macro==NULL)?"":clean_macro_chars(selected_macro,options),buffer_length-strlen(output_buffer)-1);
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
	servicegroup *temp_servicegroup;
	time_t current_time;
	unsigned long duration;
	int hours;
	int minutes;
	int seconds;
	
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
		snprintf(macro_x[MACRO_SERVICESTATEID],MAX_STATEID_LENGTH-1,"%d",svc->current_state);
		macro_x[MACRO_SERVICESTATEID][MAX_STATEID_LENGTH-1]='\x0';
	        }

	/* get the current service check attempt macro */
	if(macro_x[MACRO_SERVICEATTEMPT]!=NULL)
		free(macro_x[MACRO_SERVICEATTEMPT]);
	macro_x[MACRO_SERVICEATTEMPT]=(char *)malloc(MAX_ATTEMPT_LENGTH);
	if(macro_x[MACRO_SERVICEATTEMPT]!=NULL){
		snprintf(macro_x[MACRO_SERVICEATTEMPT],MAX_ATTEMPT_LENGTH-1,"%d",svc->current_attempt);
		macro_x[MACRO_SERVICEATTEMPT][MAX_ATTEMPT_LENGTH-1]='\x0';
	        }

	/* get the execution time macro */
	if(macro_x[MACRO_SERVICEEXECUTIONTIME]!=NULL)
		free(macro_x[MACRO_SERVICEEXECUTIONTIME]);
	macro_x[MACRO_SERVICEEXECUTIONTIME]=(char *)malloc(MAX_EXECUTIONTIME_LENGTH);
	if(macro_x[MACRO_SERVICEEXECUTIONTIME]!=NULL){
		snprintf(macro_x[MACRO_SERVICEEXECUTIONTIME],MAX_EXECUTIONTIME_LENGTH-1,"%lf",svc->execution_time);
		macro_x[MACRO_SERVICEEXECUTIONTIME][MAX_EXECUTIONTIME_LENGTH-1]='\x0';
	        }

	/* get the latency macro */
	if(macro_x[MACRO_SERVICELATENCY]!=NULL)
		free(macro_x[MACRO_SERVICELATENCY]);
	macro_x[MACRO_SERVICELATENCY]=(char *)malloc(MAX_LATENCY_LENGTH);
	if(macro_x[MACRO_SERVICELATENCY]!=NULL){
		snprintf(macro_x[MACRO_SERVICELATENCY],MAX_LATENCY_LENGTH-1,"%lf",svc->latency);
		macro_x[MACRO_SERVICELATENCY][MAX_LATENCY_LENGTH-1]='\x0';
	        }

	/* get the last check time macro */
	if(macro_x[MACRO_LASTSERVICECHECK]!=NULL)
		free(macro_x[MACRO_LASTSERVICECHECK]);
	macro_x[MACRO_LASTSERVICECHECK]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTSERVICECHECK]!=NULL){
		snprintf(macro_x[MACRO_LASTSERVICECHECK],MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)svc->last_check);
		macro_x[MACRO_LASTSERVICECHECK][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last state change time macro */
	if(macro_x[MACRO_LASTSERVICESTATECHANGE]!=NULL)
		free(macro_x[MACRO_LASTSERVICESTATECHANGE]);
	macro_x[MACRO_LASTSERVICESTATECHANGE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTSERVICESTATECHANGE]!=NULL){
		snprintf(macro_x[MACRO_LASTSERVICESTATECHANGE],MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)svc->last_state_change);
		macro_x[MACRO_LASTSERVICESTATECHANGE][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the service downtime depth */
	if(macro_x[MACRO_SERVICEDOWNTIME]!=NULL)
		free(macro_x[MACRO_SERVICEDOWNTIME]);
	macro_x[MACRO_SERVICEDOWNTIME]=(char *)malloc(MAX_DOWNTIME_LENGTH);
	if(macro_x[MACRO_SERVICEDOWNTIME]!=NULL){
		snprintf(macro_x[MACRO_SERVICEDOWNTIME],MAX_DOWNTIME_LENGTH-1,"%d",svc->scheduled_downtime_depth);
		macro_x[MACRO_SERVICEDOWNTIME][MAX_DOWNTIME_LENGTH-1]='\x0';
	        }

	/* get the percent state change */
	if(macro_x[MACRO_SERVICEPERCENTCHANGE]!=NULL)
		free(macro_x[MACRO_SERVICEPERCENTCHANGE]);
	macro_x[MACRO_SERVICEPERCENTCHANGE]=(char *)malloc(MAX_PERCENTCHANGE_LENGTH);
	if(macro_x[MACRO_SERVICEPERCENTCHANGE]!=NULL){
		snprintf(macro_x[MACRO_SERVICEPERCENTCHANGE],MAX_PERCENTCHANGE_LENGTH-1,"%.2f",svc->percent_state_change);
		macro_x[MACRO_SERVICEPERCENTCHANGE][MAX_PERCENTCHANGE_LENGTH-1]='\x0';
	        }

	time(&current_time);
	duration=(unsigned long)(current_time-svc->last_state_change);

	/* get the state duration in seconds */
	if(macro_x[MACRO_SERVICEDURATIONSEC]!=NULL)
		free(macro_x[MACRO_SERVICEDURATIONSEC]);
	macro_x[MACRO_SERVICEDURATIONSEC]=(char *)malloc(MAX_DURATION_LENGTH);
	if(macro_x[MACRO_SERVICEDURATIONSEC]!=NULL){
		snprintf(macro_x[MACRO_SERVICEDURATIONSEC],MAX_DURATION_LENGTH-1,"%lu",duration);
		macro_x[MACRO_SERVICEDURATIONSEC][MAX_DURATION_LENGTH-1]='\x0';
	        }

	/* get the state duration */
	if(macro_x[MACRO_SERVICEDURATION]!=NULL)
		free(macro_x[MACRO_SERVICEDURATION]);
	macro_x[MACRO_SERVICEDURATION]=(char *)malloc(MAX_DURATION_LENGTH);
	if(macro_x[MACRO_SERVICEDURATION]!=NULL){
		hours=duration/3600;
		duration-=(hours*3600);
		minutes=duration/60;
		duration-=(minutes*60);
		seconds=duration;
		snprintf(macro_x[MACRO_SERVICEDURATION],MAX_DURATION_LENGTH-1,"%dh %dm %ds",hours,minutes,seconds);
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

	/* get the date/time macros */
	grab_datetime_macros();

	strip(macro_x[MACRO_SERVICEOUTPUT]);
	strip(macro_x[MACRO_SERVICEPERFDATA]);

#ifdef DEBUG0
	printf("grab_service_macros() end\n");
#endif

	return OK;
        }


/* grab macros that are specific to a particular host */
int grab_host_macros(host *hst){
	hostgroup *temp_hostgroup;
	time_t current_time;
	unsigned long duration;
	int hours;
	int minutes;
	int seconds;

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
		snprintf(macro_x[MACRO_HOSTSTATEID],MAX_STATEID_LENGTH-1,"%d",hst->current_state);
		macro_x[MACRO_HOSTSTATEID][MAX_STATEID_LENGTH-1]='\x0';
	        }

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

	/* get the host current attempt */
	if(macro_x[MACRO_HOSTATTEMPT]!=NULL)
		free(macro_x[MACRO_HOSTATTEMPT]);
	macro_x[MACRO_HOSTATTEMPT]=(char *)malloc(MAX_ATTEMPT_LENGTH);
	if(macro_x[MACRO_HOSTATTEMPT]!=NULL){
		snprintf(macro_x[MACRO_HOSTATTEMPT],MAX_ATTEMPT_LENGTH-1,"%d",hst->current_attempt);
		macro_x[MACRO_HOSTATTEMPT][MAX_ATTEMPT_LENGTH-1]='\x0';
	        }

	/* get the host downtime depth */
	if(macro_x[MACRO_HOSTDOWNTIME]!=NULL)
		free(macro_x[MACRO_HOSTDOWNTIME]);
	macro_x[MACRO_HOSTDOWNTIME]=(char *)malloc(MAX_DOWNTIME_LENGTH);
	if(macro_x[MACRO_HOSTDOWNTIME]!=NULL){
		snprintf(macro_x[MACRO_HOSTDOWNTIME],MAX_DOWNTIME_LENGTH-1,"%d",hst->scheduled_downtime_depth);
		macro_x[MACRO_HOSTDOWNTIME][MAX_DOWNTIME_LENGTH-1]='\x0';
	        }

	/* get the percent state change */
	if(macro_x[MACRO_HOSTPERCENTCHANGE]!=NULL)
		free(macro_x[MACRO_HOSTPERCENTCHANGE]);
	macro_x[MACRO_HOSTPERCENTCHANGE]=(char *)malloc(MAX_PERCENTCHANGE_LENGTH);
	if(macro_x[MACRO_HOSTPERCENTCHANGE]!=NULL){
		snprintf(macro_x[MACRO_HOSTPERCENTCHANGE],MAX_PERCENTCHANGE_LENGTH-1,"%.2f",hst->percent_state_change);
		macro_x[MACRO_HOSTPERCENTCHANGE][MAX_PERCENTCHANGE_LENGTH-1]='\x0';
	        }

	time(&current_time);
	duration=(unsigned long)(current_time-hst->last_state_change);

	/* get the state duration in seconds */
	if(macro_x[MACRO_HOSTDURATIONSEC]!=NULL)
		free(macro_x[MACRO_HOSTDURATIONSEC]);
	macro_x[MACRO_HOSTDURATIONSEC]=(char *)malloc(MAX_DURATION_LENGTH);
	if(macro_x[MACRO_HOSTDURATIONSEC]!=NULL){
		snprintf(macro_x[MACRO_HOSTDURATIONSEC],MAX_DURATION_LENGTH-1,"%lu",duration);
		macro_x[MACRO_HOSTDURATIONSEC][MAX_DURATION_LENGTH-1]='\x0';
	        }

	/* get the state duration */
	if(macro_x[MACRO_HOSTDURATION]!=NULL)
		free(macro_x[MACRO_HOSTDURATION]);
	macro_x[MACRO_HOSTDURATION]=(char *)malloc(MAX_DURATION_LENGTH);
	if(macro_x[MACRO_HOSTDURATION]!=NULL){
		hours=duration/3600;
		duration-=(hours*3600);
		minutes=duration/60;
		duration-=(minutes*60);
		seconds=duration;
		snprintf(macro_x[MACRO_HOSTDURATION],MAX_DURATION_LENGTH-1,"%dh %dm %ds",hours,minutes,seconds);
		macro_x[MACRO_HOSTDURATION][MAX_DURATION_LENGTH-1]='\x0';
	        }

	/* get the execution time macro */
	if(macro_x[MACRO_HOSTEXECUTIONTIME]!=NULL)
		free(macro_x[MACRO_HOSTEXECUTIONTIME]);
	macro_x[MACRO_HOSTEXECUTIONTIME]=(char *)malloc(MAX_EXECUTIONTIME_LENGTH);
	if(macro_x[MACRO_HOSTEXECUTIONTIME]!=NULL){
		snprintf(macro_x[MACRO_HOSTEXECUTIONTIME],MAX_EXECUTIONTIME_LENGTH-1,"%lf",hst->execution_time);
		macro_x[MACRO_HOSTEXECUTIONTIME][MAX_EXECUTIONTIME_LENGTH-1]='\x0';
	        }

	/* get the latency macro */
	if(macro_x[MACRO_HOSTLATENCY]!=NULL)
		free(macro_x[MACRO_HOSTLATENCY]);
	macro_x[MACRO_HOSTLATENCY]=(char *)malloc(MAX_LATENCY_LENGTH);
	if(macro_x[MACRO_HOSTLATENCY]!=NULL){
		snprintf(macro_x[MACRO_HOSTLATENCY],MAX_LATENCY_LENGTH-1,"%lf",hst->latency);
		macro_x[MACRO_HOSTLATENCY][MAX_LATENCY_LENGTH-1]='\x0';
	        }

	/* get the last check time macro */
	if(macro_x[MACRO_LASTHOSTCHECK]!=NULL)
		free(macro_x[MACRO_LASTHOSTCHECK]);
	macro_x[MACRO_LASTHOSTCHECK]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTHOSTCHECK]!=NULL){
		snprintf(macro_x[MACRO_LASTHOSTCHECK],MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)hst->last_state_change);
		macro_x[MACRO_LASTHOSTCHECK][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/* get the last state change time macro */
	if(macro_x[MACRO_LASTHOSTSTATECHANGE]!=NULL)
		free(macro_x[MACRO_LASTHOSTSTATECHANGE]);
	macro_x[MACRO_LASTHOSTSTATECHANGE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LASTHOSTSTATECHANGE]!=NULL){
		snprintf(macro_x[MACRO_LASTHOSTSTATECHANGE],MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)hst->last_state_change);
		macro_x[MACRO_LASTHOSTSTATECHANGE][MAX_DATETIME_LENGTH-1]='\x0';
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

	/* get the date/time macros */
	grab_datetime_macros();

	strip(macro_x[MACRO_HOSTOUTPUT]);
	strip(macro_x[MACRO_HOSTPERFDATA]);

#ifdef DEBUG0
	printf("grab_host_macros() end\n");
#endif

	return OK;
        }


/* grab an on-demand host or service macro */
int grab_on_demand_macro(char *str){
	char *macro=NULL;
	char *host_name=NULL;
	char *service_description=NULL;
	host *temp_host;
	service *temp_service;
	char *ptr;

#ifdef DEBUG0
	printf("grab_on_demand_macro() start\n");
#endif

	/* clear the on-demand macro */
	if(macro_ondemand!=NULL){
		free(macro_ondemand);
		macro_ondemand=NULL;
	        }

	/* get the macro name */
	macro=strdup(str);
	if(macro==NULL)
		return ERROR;

	/* get the host name */
	ptr=strchr(macro,':');
	if(ptr==NULL)
		return ERROR;
	ptr[0]='\x0';
	host_name=strdup(ptr+1);
	if(host_name==NULL){
		free(macro);
		return ERROR;
	        }

	/* get the service description (if applicable) */
	ptr=strchr(host_name,':');
	if(ptr!=NULL){
		ptr[0]='\x0';
		service_description=strdup(ptr+1);
		if(service_description==NULL){
			free(macro);
			free(host_name);
			return ERROR;
		        }
	        }

	/* process the macro */
	if(strstr(macro,"HOST")){
		temp_host=find_host(host_name);
		grab_on_demand_host_macro(temp_host,macro);
	        }
	else if(strstr(macro,"SERVICE")){
		temp_service=find_service(host_name,service_description);
		grab_on_demand_service_macro(temp_service,macro);
	        }

	free(macro);
	free(host_name);
	free(service_description);

#ifdef DEBUG0
	printf("grab_on_demand_macro() end\n");
#endif

	return OK;
        }


/* grab an on-demand host macro */
int grab_on_demand_host_macro(host *hst, char *macro){
	hostgroup *temp_hostgroup;
	time_t current_time;
	unsigned long duration;
	int hours;
	int minutes;
	int seconds;

#ifdef DEBUG0
	printf("grab_on_demand_host_macro() start\n");
#endif

	if(hst==NULL || macro==NULL)
		return ERROR;

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
			snprintf(macro_ondemand,MAX_STATEID_LENGTH-1,"%d",hst->current_state);
			macro_ondemand[MAX_STATEID_LENGTH-1]='\x0';
		        }
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
			snprintf(macro_ondemand,MAX_ATTEMPT_LENGTH-1,"%d",hst->current_attempt);
			macro_ondemand='\x0';
		        }
	        }

	/* get the host downtime depth */
	else if(!strcmp(macro,"HOSTDOWNTIME")){
		macro_ondemand=(char *)malloc(MAX_DOWNTIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DOWNTIME_LENGTH-1,"%d",hst->scheduled_downtime_depth);
			macro_ondemand='\x0';
		        }
	        }

	/* get the percent state change */
	else if(!strcmp(macro,"HOSTPERCENTCHANGE")){
		macro_ondemand=(char *)malloc(MAX_PERCENTCHANGE_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_PERCENTCHANGE_LENGTH-1,"%.2f",hst->percent_state_change);
			macro_ondemand[MAX_PERCENTCHANGE_LENGTH-1]='\x0';
		        }
	        }

	/* get the state duration in seconds */
	else if(!strcmp(macro,"HOSTDURATIONSEC")){
		macro_ondemand=(char *)malloc(MAX_DURATION_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DURATION_LENGTH-1,"%lu",duration);
			macro_ondemand[MAX_DURATION_LENGTH-1]='\x0';
		        }
	        }

	/* get the state duration */
	else if(!strcmp(macro,"HOSTDURATION")){
		macro_ondemand=(char *)malloc(MAX_DURATION_LENGTH);
		if(macro_ondemand!=NULL){
			hours=duration/3600;
			duration-=(hours*3600);
			minutes=duration/60;
			duration-=(minutes*60);
			seconds=duration;
			snprintf(macro_ondemand,MAX_DURATION_LENGTH-1,"%dh %dm %ds",hours,minutes,seconds);
			macro_ondemand[MAX_DURATION_LENGTH-1]='\x0';
		        }
	        }

	/* get the execution time macro */
	else if(!strcmp(macro,"HOSTEXECUTIONTIME")){
		macro_ondemand=(char *)malloc(MAX_EXECUTIONTIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_EXECUTIONTIME_LENGTH-1,"%lf",hst->execution_time);
			macro_ondemand[MAX_EXECUTIONTIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the latency macro */
	else if(!strcmp(macro,"HOSTLATENCY")){
		macro_ondemand=(char *)malloc(MAX_LATENCY_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_LATENCY_LENGTH-1,"%lf",hst->latency);
			macro_ondemand[MAX_LATENCY_LENGTH-1]='\x0';
		        }
	        }

	/* get the last check time macro */
	else if(!strcmp(macro,"LASTHOSTCHECK")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)hst->last_state_change);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the last state change time macro */
	else if(!strcmp(macro,"LASTHOSTSTATECHANGE")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)hst->last_state_change);
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

#ifdef DEBUG0
	printf("grab_on_demand_host_macro() end\n");
#endif

	return OK;
        }


/* grab an on-demand service macro */
int grab_on_demand_service_macro(service *svc, char *macro){
	servicegroup *temp_servicegroup;
	time_t current_time;
	unsigned long duration;
	int hours;
	int minutes;
	int seconds;

#ifdef DEBUG0
	printf("grab_on_demand_service_macro() start\n");
#endif

	if(svc==NULL || macro==NULL)
		return ERROR;

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
			snprintf(macro_ondemand,MAX_STATEID_LENGTH-1,"%d",svc->current_state);
			macro_ondemand[MAX_STATEID_LENGTH-1]='\x0';
		        }
	        }

	/* get the current service check attempt macro */
	else if(!strcmp(macro,"SERVICEATTEMPT")){
		macro_ondemand=(char *)malloc(MAX_ATTEMPT_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_ATTEMPT_LENGTH-1,"%d",svc->current_attempt);
			macro_ondemand[MAX_ATTEMPT_LENGTH-1]='\x0';
		        }
	        }

	/* get the execution time macro */
	else if(!strcmp(macro,"SERVICEEXECUTIONTIME")){
		macro_ondemand=(char *)malloc(MAX_EXECUTIONTIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_EXECUTIONTIME_LENGTH-1,"%lf",svc->execution_time);
			macro_ondemand[MAX_EXECUTIONTIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the latency macro */
	else if(!strcmp(macro,"SERVICELATENCY")){
		macro_ondemand=(char *)malloc(MAX_LATENCY_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_LATENCY_LENGTH-1,"%lf",svc->latency);
			macro_ondemand[MAX_LATENCY_LENGTH-1]='\x0';
		        }
	        }

	/* get the last check time macro */
	else if(!strcmp(macro,"LASTSERVICECHECK")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)svc->last_check);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the last state change time macro */
	else if(!strcmp(macro,"LASTSERVICESTATECHANGE")){
		macro_ondemand=(char *)malloc(MAX_DATETIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)svc->last_state_change);
			macro_ondemand[MAX_DATETIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the service downtime depth */
	else if(!strcmp(macro,"SERVICEDOWNTIME")){
		macro_ondemand=(char *)malloc(MAX_DOWNTIME_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DOWNTIME_LENGTH-1,"%d",svc->scheduled_downtime_depth);
			macro_ondemand[MAX_DOWNTIME_LENGTH-1]='\x0';
		        }
	        }

	/* get the percent state change */
	else if(!strcmp(macro,"SERVICEPERCENTCHANGE")){
		macro_ondemand=(char *)malloc(MAX_PERCENTCHANGE_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_PERCENTCHANGE_LENGTH-1,"%.2f",svc->percent_state_change);
			macro_ondemand[MAX_PERCENTCHANGE_LENGTH-1]='\x0';
		        }
	        }

	/* get the state duration in seconds */
	else if(!strcmp(macro,"SERVICEDURATIONSEC")){
		macro_ondemand=(char *)malloc(MAX_DURATION_LENGTH);
		if(macro_ondemand!=NULL){
			snprintf(macro_ondemand,MAX_DURATION_LENGTH-1,"%lu",duration);
			macro_ondemand[MAX_DURATION_LENGTH-1]='\x0';
		        }
	        }

	/* get the state duration */
	else if(!strcmp(macro,"SERVICEDURATION")){
		macro_ondemand=(char *)malloc(MAX_DURATION_LENGTH);
		if(macro_ondemand!=NULL){
			hours=duration/3600;
			duration-=(hours*3600);
			minutes=duration/60;
			duration-=(minutes*60);
			seconds=duration;
			snprintf(macro_ondemand,MAX_DURATION_LENGTH-1,"%dh %dm %ds",hours,minutes,seconds);
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
		snprintf(macro_x[MACRO_TIMET],MAX_DATETIME_LENGTH-1,"%lu",(unsigned long)t);
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




/******************************************************************/
/******************** SYSTEM COMMAND FUNCTIONS ********************/
/******************************************************************/


/* executes a system command - used for service checks and notifications */
int my_system(char *cmd,int timeout,int *early_timeout,double *exectime,char *output,int output_length){
        pid_t pid;
	int status;
	int result;
	char buffer[MAX_INPUT_BUFFER];
	int fd[2];
	FILE *fp=NULL;
	int bytes_read=0;
	struct timeval start_time,end_time;
	int attr=NEBATTR_NONE;
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

	isperl=FALSE;

	if(strstr(buffer,"/bin/perl")!=NULL){

		isperl = TRUE;
		args[0] = fname;
		args[2] = tmpfname;

		if(strchr(cmd,' ')==NULL)
			args[3]="";
		else
			args[3]=cmd+strlen(fname)+1;

		/* reinialize embedded perl if necessary */
		if(use_embedded_perl==TRUE && max_embedded_perl_calls>0 && embedded_perl_calls>max_embedded_perl_calls)
			reinit_embedded_perl();

		embedded_perl_calls++;

		/* call our perl interpreter to compile and optionally cache the compiled script. */
		if(use_embedded_perl==TRUE)
			perl_call_argv("Embed::Persistent::eval_file", G_DISCARD | G_EVAL, args);
	        }
#endif 

	/* create a pipe */
	pipe(fd);

	/* make the pipe non-blocking */
	fcntl(fd[0],F_SETFL,O_NONBLOCK);
	fcntl(fd[1],F_SETFL,O_NONBLOCK);

	/* get the command start time */
	gettimeofday(&start_time,NULL);

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
		if(isperl==TRUE && use_embedded_perl==TRUE){

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
			attr+=NEBATTR_EARLY_COMMAND_TIMEOUT;

			/* try to kill the command that timed out by sending termination signal to child process group */
			kill((pid_t)(-pid),SIGTERM);
			sleep(1);
			kill((pid_t)(-pid),SIGKILL);
		        }

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_system_command(NEBTYPE_SYSTEM_COMMAND,NEBFLAG_NONE,attr,*exectime,timeout,*early_timeout,result,cmd,output,NULL);
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

	/* lookup the command... */

	/* get the command name */
	strcpy(temp_buffer,cmd);
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
	temp_command=find_command(raw_command);

	/* error if we couldn't find the command */
	if(temp_command==NULL)
		return;

	strncpy(raw_command,temp_command->command_line,buffer_length);
	raw_command[buffer_length-1]='\x0';
	strip(raw_command);

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
		t->tm_mon++;
		t->tm_mday=1;
		t->tm_hour=0;
		run_time=mktime(t);
		break;
	default:
		break;
	        }

	if(is_dst_now==TRUE && t->tm_isdst==0)
		run_time+=3600;
	else if(is_dst_now==FALSE && t->tm_isdst>0)
		run_time-=3600;

#ifdef DEBUG1
	printf("\tNext Log Rotation Time: %s",ctime(run_time));
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
		write_to_all_logs(temp_buffer,NSLOG_PROCESS_INFO);
#ifdef DEBUG2
		printf("%s\n",temp_buffer);
#endif
	        }

	/* else begin shutting down... */
	else if(sig<16){

		sigshutdown=TRUE;

		sprintf(temp_buffer,"Caught SIG%s, shutting down...\n",sigs[sig]);
		write_to_all_logs(temp_buffer,NSLOG_PROCESS_INFO);

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
	struct timeval end_time;

	/* get the current time */
	gettimeofday(&end_time,NULL);

	/* write plugin check results to message queue */
	strncpy(svc_msg.output,"(Service Check Timed Out)",sizeof(svc_msg.output)-1);
	svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
	svc_msg.return_code=STATE_CRITICAL;
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

#ifdef USE_EVENT_BROKER
	/* send program data to broker */
	broker_program_state(NEBTYPE_PROCESS_DAEMON,NEBFLAG_NONE,NEBATTR_NONE,NULL);
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
	bzero((void *)message,sizeof(service_message));

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
		service_result_buffer.tail=(service_result_buffer.tail + 1) % SERVICE_BUFFER_SLOTS;
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
	int write_result;

#ifdef DEBUG0
	printf("write_svc_message() start\n");
#endif

	if(message==NULL)
		return 0;

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
/******************** EMBEDDED PERL FUNCTIONS *********************/
/******************************************************************/

/* initializes embedded perl interpreter */
int init_embedded_perl(void){
#ifdef EMBEDDEDPERL
	char *embedding[] = { "", "" };
	int exitstatus = 0;
	char buffer[MAX_INPUT_BUFFER];

	embedding[1]=p1_file;

	embedded_perl_calls=0;
	use_embedded_perl=TRUE;

	if((perl=perl_alloc())==NULL){
		use_embedded_perl=FALSE;
		snprintf(buffer,sizeof(buffer),"Error: Could not allocate memory for embedded Perl interpreter!\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
		return ERROR;
                }

	perl_construct(perl);
	exitstatus=perl_parse(perl,xs_init,2,embedding,NULL);
	if(!exitstatus)
		exitstatus=perl_run(perl);

#endif
	return OK;
        }


/* closes embedded perl interpreter */
int deinit_embedded_perl(void){
#ifdef EMBEDDEDPERL

	PL_perl_destruct_level=0;
	perl_destruct(perl);
	perl_free(perl);

#endif
	return OK;
        }


/* reinitialized embedded perl interpreter */
int reinit_embedded_perl(void){
#ifdef EMBEDDEDPERL
	char buffer[MAX_INPUT_BUFFER];

	snprintf(buffer,sizeof(buffer),"Re-initializing embedded Perl interpreter after %d uses...\n",embedded_perl_calls);
	buffer[sizeof(buffer)-1]='\x0';
	write_to_logs_and_console(buffer,NSLOG_INFO_MESSAGE,TRUE);

	deinit_embedded_perl();
	
	if(init_embedded_perl()==ERROR){
		snprintf(buffer,sizeof(buffer),"Error: Could not re-initialize embedded Perl interpreter!  Perl scripts will be interpreted normally.\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
		return ERROR;
                }

	snprintf(buffer,sizeof(buffer),"Embedded Perl interpreter re-initialized ok.\n");
	buffer[sizeof(buffer)-1]='\x0';
	write_to_logs_and_console(buffer,NSLOG_INFO_MESSAGE,TRUE);

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
	service_result_buffer.overflow=0L;
	service_result_buffer.buffer=(void **)malloc(SERVICE_BUFFER_SLOTS*sizeof(service_message **));
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
	for(x=service_result_buffer.tail;x!=service_result_buffer.head;x=(x+1) % SERVICE_BUFFER_SLOTS){
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
	external_command_buffer.overflow=0L;
	external_command_buffer.buffer=(void **)malloc(COMMAND_BUFFER_SLOTS*sizeof(char **));
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
	for(x=external_command_buffer.tail;x!=external_command_buffer.head;x=(x+1) % COMMAND_BUFFER_SLOTS){
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
	int read_result;
	int bytes_to_read;
	int write_offset;
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
		if(pollval<=0)
			continue;

		/* should we shutdown? */
		pthread_testcancel();

		/* obtain a lock for writing to the buffer */
		pthread_mutex_lock(&service_result_buffer.buffer_lock);

		/* process all data in the buffer if there's some space in the buffer */
		if(service_result_buffer.items<SERVICE_BUFFER_SLOTS){

			/* clear the message buffer */
			bzero((void *)&message,sizeof(service_message));

			/* initialize the number of bytes to read */
			bytes_to_read=sizeof(service_message);

			/* read a single message from the pipe, taking into consideration that we may have had an interrupt */
			while(1){

				write_offset=sizeof(service_message)-bytes_to_read;

				/* try and read a (full or partial) message */
				read_result=read(ipc_pipe[0],((void *)&message)+write_offset,bytes_to_read);

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
					continue;
				        }

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

				/* handle overflow conditions */
				if(service_result_buffer.items==SERVICE_BUFFER_SLOTS){

					/* record overflow */
					service_result_buffer.overflow++;

					/* update tail pointer */
					service_result_buffer.tail=(service_result_buffer.tail + 1) % SERVICE_BUFFER_SLOTS;
				        }

				/* save the data to the buffer */
				((service_message **)service_result_buffer.buffer)[service_result_buffer.head]=new_message;

				/* increment the head counter and items */
				service_result_buffer.head=(service_result_buffer.head + 1) % SERVICE_BUFFER_SLOTS;
				if(service_result_buffer.items<SERVICE_BUFFER_SLOTS)
					service_result_buffer.items++;
			        }

			/* bail out if the buffer is now full */
			if(service_result_buffer.items==SERVICE_BUFFER_SLOTS)
				break;

			/* should we shutdown? */
			pthread_testcancel();
                        }

		/* release lock on buffer */
		pthread_mutex_unlock(&service_result_buffer.buffer_lock);
	        }

	/* removes cleanup handler */
	pthread_cleanup_pop(0);

	return NULL;
        }



/* worker thread - artificially increases buffer of named pipe */
void * command_file_worker_thread(void *arg){
	char input_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	/* specify cleanup routine */
	pthread_cleanup_push(cleanup_command_file_worker_thread,NULL);

	/* set cancellation info */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);

	while(1){

		/* should we shutdown? */
		pthread_testcancel();

		/**** REPLACE THIS WITH A CALL TO POLL(), AS SELECT() DOESN'T WORK ****/
		/* wait a bit */
		tv.tv_sec=0;
		tv.tv_usec=500000;
		select(0,NULL,NULL,NULL,&tv);

		/* should we shutdown? */
		pthread_testcancel();

		/* obtain a lock for writing to the buffer */
		pthread_mutex_lock(&external_command_buffer.buffer_lock);

		/* process all commands in the file (named pipe) if there's some space in the buffer */
		if(external_command_buffer.items<COMMAND_BUFFER_SLOTS){

			/* clear EOF condition from prior run (FreeBSD fix) */
			clearerr(command_file_fp);

			while(fgets(input_buffer,(int)(sizeof(input_buffer)-1),command_file_fp)!=NULL){

				/* save the line in the buffer */
				((char **)external_command_buffer.buffer)[external_command_buffer.head]=strdup(input_buffer);

				/* increment the head counter and items */
				external_command_buffer.head=(external_command_buffer.head + 1) % COMMAND_BUFFER_SLOTS;
				external_command_buffer.items++;

				/* bail out if the buffer is now full */
				if(external_command_buffer.items==COMMAND_BUFFER_SLOTS)
					break;

				/* should we shutdown? */
				pthread_testcancel();
	                        }

			/* rewind file pointer (fix for FreeBSD, may already be taken care of due to clearerr() call before reading begins) */
			rewind(command_file_fp);
		        }

		/* release lock on buffer */
		pthread_mutex_unlock(&external_command_buffer.buffer_lock);
	        }

	/* removes cleanup handler */
	pthread_cleanup_pop(0);

	return NULL;
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
		neb_unload_all_modules(NEBMODULE_FORCE_UNLOAD,(sigshutdown==TRUE)?NEBMODULE_NEB_SHUTDOWN:NEBMODULE_NEB_RESTART);
		neb_free_module_list();
		neb_free_callback_list();
		neb_deinit_modules();
	        }
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

	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++)
		macro_argv[x]=NULL;

	for(x=0;x<MAX_USER_MACROS;x++)
		macro_user[x]=NULL;
	

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

	check_external_commands=DEFAULT_CHECK_EXTERNAL_COMMANDS;
	check_orphaned_services=DEFAULT_CHECK_ORPHANED_SERVICES;
	check_service_freshness=DEFAULT_CHECK_SERVICE_FRESHNESS;
	check_host_freshness=DEFAULT_CHECK_HOST_FRESHNESS;

	log_rotation_method=LOG_ROTATION_NONE;

	last_command_check=0L;
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

	date_format=DATE_FORMAT_US;

	max_embedded_perl_calls=DEFAULT_MAX_EMBEDDED_PERL_CALLS;

	for(x=0;x<=MACRO_X_COUNT;x++)
		macro_x[x]=NULL;

	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++)
		macro_argv[x]=NULL;

	for(x=0;x<MAX_USER_MACROS;x++)
		macro_user[x]=NULL;

	for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
		macro_contactaddress[x]=NULL;

	macro_ondemand=NULL;

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
