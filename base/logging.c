/*****************************************************************************
 *
 * LOGGING.C - Log file functions for use with Nagios
 *
 * Copyright (c) 1999-2005 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-12-2005
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
#include "../include/statusdata.h"
#include "../include/nagios.h"
#include "../include/broker.h"


extern char	*log_file;
extern char     *temp_file;
extern char	*log_archive_path;

extern char     *macro_x[MACRO_X_COUNT];

extern host     *host_list;
extern service  *service_list;

extern int	use_syslog;
extern int      log_service_retries;
extern int      log_initial_states;

extern unsigned long      logging_options;
extern unsigned long      syslog_options;

extern int      verify_config;
extern int      test_scheduling;

extern time_t   last_log_rotation;
extern int      log_rotation_method;

extern int      daemon_mode;




/******************************************************************/
/************************ LOGGING FUNCTIONS ***********************/
/******************************************************************/

/* write something to the log file, syslog, and possibly the console */
int write_to_logs_and_console(char *buffer, unsigned long data_type, int display){
	int len;
	int x;

#ifdef DEBUG0
	printf("write_to_logs_and_console() start\n");
#endif

	/* strip unnecessary newlines */
	len=strlen(buffer);
	for(x=len-1;x>=0;x--){
		if(buffer[x]=='\n')
			buffer[x]='\x0';
		else
			break;
	        }

	/* write messages to the logs */
	write_to_all_logs(buffer,data_type);

	/* write message to the console */
	if(display==TRUE)
		write_to_console(buffer);

#ifdef DEBUG0
	printf("write_to_logs_and_console() end\n");
#endif

	return OK;
        }


/* write something to the console */
int write_to_console(char *buffer){

#ifdef DEBUG0
	printf("write_to_console() start\n");
#endif

	/* should we print to the console? */
	if(daemon_mode==FALSE)
		printf("%s\n",buffer);

#ifdef DEBUG0
	printf("write_to_console() end\n");
#endif

	return OK;
        }


/* write something to the log file and syslog facility */
int write_to_all_logs(char *buffer, unsigned long data_type){

#ifdef DEBUG0
	printf("write_to_all_logs() start\n");
#endif

	/* write to syslog */
	write_to_syslog(buffer,data_type);

	/* write to main log */
	write_to_log(buffer,data_type,NULL);

#ifdef DEBUG0
	printf("write_to_all_logs() end\n");
#endif

	return OK;
        }


/* write something to the log file and syslog facility */
int write_to_all_logs_with_timestamp(char *buffer, unsigned long data_type, time_t *timestamp){

#ifdef DEBUG0
	printf("write_to_all_logs_with_timestamp() start\n");
#endif

	/* write to syslog */
	write_to_syslog(buffer,data_type);

	/* write to main log */
	write_to_log(buffer,data_type,timestamp);

#ifdef DEBUG0
	printf("write_to_all_logs_with_timestamp() end\n");
#endif

	return OK;
        }


/* write something to the nagios log file */
int write_to_log(char *buffer, unsigned long data_type, time_t *timestamp){
	FILE *fp;
	time_t log_time;


#ifdef DEBUG0
	printf("write_to_log() start\n");
#endif

	/* don't log anything if we're not actually running... */
	if(verify_config==TRUE || test_scheduling==TRUE)
		return OK;

	/* make sure we can log this type of entry */
	if(!(data_type & logging_options))
		return OK;

	fp=fopen(log_file,"a+");
	if(fp==NULL){
		if(daemon_mode==FALSE)
			printf("Warning: Cannot open log file '%s' for writing\n",log_file);
		return ERROR;
		}

	/* what timestamp should we use? */
	if(timestamp==NULL)
		time(&log_time);
	else
		log_time=*timestamp;

	/* strip any newlines from the end of the buffer */
	strip(buffer);

	/* write the buffer to the log file */
	fprintf(fp,"[%lu] %s\n",log_time,buffer);

	fclose(fp);

#ifdef USE_EVENT_BROKER
	/* send data to the event broker */
	broker_log_data(NEBTYPE_LOG_DATA,NEBFLAG_NONE,NEBATTR_NONE,buffer,data_type,log_time,NULL);
#endif

#ifdef DEBUG0
	printf("write_to_log() end\n");
#endif

	return OK;
	}


/* write something to the syslog facility */
int write_to_syslog(char *buffer, unsigned long data_type){

#ifdef DEBUG0
	printf("write_to_syslog() start\n");
#endif

	/* don't log anything if we're not actually running... */
	if(verify_config==TRUE || test_scheduling==TRUE)
		return OK;

	/* bail out if we shouldn't write to syslog */
	if(use_syslog==FALSE)
		return OK;

	/* make sure we should log this type of entry */
	if(!(data_type & syslog_options))
		return OK;

	/* write the buffer to the syslog facility */
	syslog(LOG_USER|LOG_INFO,"%s",buffer);

#ifdef DEBUG0
	printf("write_to_syslog() end\n");
#endif

	return OK;
	}


/* write a service problem/recovery to the nagios log file */
int log_service_event(service *svc){
	char temp_buffer[MAX_INPUT_BUFFER];
	unsigned long log_options;
	host *temp_host;

#ifdef DEBUG0
	printf("log_service_event() start\n");
#endif

	/* don't log soft errors if the user doesn't want to */
	if(svc->state_type==SOFT_STATE && !log_service_retries)
		return OK;

	/* get the log options */
	if(svc->current_state==STATE_UNKNOWN)
		log_options=NSLOG_SERVICE_UNKNOWN;
	else if(svc->current_state==STATE_WARNING)
		log_options=NSLOG_SERVICE_WARNING;
	else if(svc->current_state==STATE_CRITICAL)
		log_options=NSLOG_SERVICE_CRITICAL;
	else
		log_options=NSLOG_SERVICE_OK;

	/* find the associated host */
	temp_host=find_host(svc->host_name);

	/* grab service macros */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);
	grab_summary_macros(NULL);

	snprintf(temp_buffer,sizeof(temp_buffer),"SERVICE ALERT: %s;%s;%s;%s;%s;%s\n",svc->host_name,svc->description,macro_x[MACRO_SERVICESTATE],macro_x[MACRO_SERVICESTATETYPE],macro_x[MACRO_SERVICEATTEMPT],svc->plugin_output);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_all_logs(temp_buffer,log_options);


#ifdef DEBUG0
	printf("log_service_event() end\n");
#endif

	return OK;
	}


/* write a host problem/recovery to the log file */
int log_host_event(host *hst){
	char temp_buffer[MAX_INPUT_BUFFER];
	unsigned long log_options=0L;

#ifdef DEBUG0
	printf("log_host_event() start\n");
#endif

	/* grab the host macros */
	clear_volatile_macros();
	grab_host_macros(hst);
	grab_summary_macros(NULL);

	/* get the log options */
	if(hst->current_state==HOST_DOWN)
		log_options=NSLOG_HOST_DOWN;
	else if(hst->current_state==HOST_UNREACHABLE)
		log_options=NSLOG_HOST_UNREACHABLE;
	else
		log_options=NSLOG_HOST_UP;


	snprintf(temp_buffer,sizeof(temp_buffer),"HOST ALERT: %s;%s;%s;%s;%s\n",hst->name,macro_x[MACRO_HOSTSTATE],macro_x[MACRO_HOSTSTATETYPE],macro_x[MACRO_HOSTATTEMPT],hst->plugin_output);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_all_logs(temp_buffer,log_options);

#ifdef DEBUG0
	printf("log_host_event() start\n");
#endif
	return OK;
        }


/* logs host states */
int log_host_states(int type, time_t *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	host *temp_host;

	/* bail if we shouldn't be logging initial states */
	if(type==INITIAL_STATES && log_initial_states==FALSE)
		return OK;

	/* grab summary macros */
	grab_summary_macros(NULL);

	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* grab the host macros */
		clear_volatile_macros();
		grab_host_macros(temp_host);

		snprintf(temp_buffer,sizeof(temp_buffer),"%s HOST STATE: %s;%s;%s;%s;%s\n",(type==INITIAL_STATES)?"INITIAL":"CURRENT",temp_host->name,macro_x[MACRO_HOSTSTATE],macro_x[MACRO_HOSTSTATETYPE],macro_x[MACRO_HOSTATTEMPT],temp_host->plugin_output);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_all_logs_with_timestamp(temp_buffer,NSLOG_INFO_MESSAGE,timestamp);
	        }

	return OK;
        }


/* logs service states */
int log_service_states(int type, time_t *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	service *temp_service;
	host *temp_host;

	/* bail if we shouldn't be logging initial states */
	if(type==INITIAL_STATES && log_initial_states==FALSE)
		return OK;

	/* grab summary macros */
	grab_summary_macros(NULL);

	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		/* find the associated host */
		temp_host=find_host(temp_service->host_name);

		/* grab service macros */
		clear_volatile_macros();
		grab_host_macros(temp_host);
		grab_service_macros(temp_service);

		snprintf(temp_buffer,sizeof(temp_buffer),"%s SERVICE STATE: %s;%s;%s;%s;%s;%s\n",(type==INITIAL_STATES)?"INITIAL":"CURRENT",temp_service->host_name,temp_service->description,macro_x[MACRO_SERVICESTATE],macro_x[MACRO_SERVICESTATETYPE],macro_x[MACRO_SERVICEATTEMPT],temp_service->plugin_output);

		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_all_logs_with_timestamp(temp_buffer,NSLOG_INFO_MESSAGE,timestamp);
	        }

	return OK;
        }


/* rotates the main log file */
int rotate_log_file(time_t rotation_time){
	char temp_buffer[MAX_INPUT_BUFFER];
	char method_string[16];
	char log_archive[MAX_FILENAME_LENGTH];
	struct tm *t;
	int rename_result;

#ifdef DEBUG0
	printf("rotate_log_file() start\n");
#endif

	if(log_rotation_method==LOG_ROTATION_NONE){

#ifdef DEBUG1
		printf("\tWe're not supposed to be doing log rotations!\n");
#endif
		return OK;
	        }
	else if(log_rotation_method==LOG_ROTATION_HOURLY)
		strcpy(method_string,"HOURLY");
	else if(log_rotation_method==LOG_ROTATION_DAILY)
		strcpy(method_string,"DAILY");
	else if(log_rotation_method==LOG_ROTATION_WEEKLY)
		strcpy(method_string,"WEEKLY");
	else if(log_rotation_method==LOG_ROTATION_MONTHLY)
		strcpy(method_string,"MONTHLY");
	else
		return ERROR;

	/* update the last log rotation time and status log */
	last_log_rotation=time(NULL);
	update_program_status(FALSE);

	t=localtime(&rotation_time);

	/* get the archived filename to use */
	snprintf(log_archive,sizeof(log_archive),"%s%snagios-%02d-%02d-%d-%02d.log",log_archive_path,(log_archive_path[strlen(log_archive_path)-1]=='/')?"":"/",t->tm_mon+1,t->tm_mday,t->tm_year+1900,t->tm_hour);
	log_archive[sizeof(log_archive)-1]='\x0';

	/* rotate the log file */
	rename_result=my_rename(log_file,log_archive);

	if(rename_result){

#ifdef DEBUG1
		printf("\tError: Could not rotate main log file to '%s'\n",log_archive);
#endif

		return ERROR;
	        }

#ifdef USE_EVENT_BROKER
	/* REMOVED - log rotation events are already handled by NEBTYPE_TIMEDEVENT_EXECUTE... */
	/* send data to the event broker */
        /*
	broker_log_data(NEBTYPE_LOG_ROTATION,NEBFLAG_NONE,NEBATTR_NONE,log_archive,log_rotation_method,0,NULL);
	*/
#endif

	/* record the log rotation after it has been done... */
	snprintf(temp_buffer,sizeof(temp_buffer),"LOG ROTATION: %s\n",method_string);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_all_logs_with_timestamp(temp_buffer,NSLOG_PROCESS_INFO,&rotation_time);

	/* record log file version format */
	write_log_file_info(&rotation_time);

	/* log current host and service state */
	log_host_states(CURRENT_STATES,&rotation_time);
	log_service_states(CURRENT_STATES,&rotation_time);

#ifdef DEBUG3
	printf("\tRotated main log file to '%s'\n",log_archive);
#endif

#ifdef DEBUG0
	printf("rotate_log_file() end\n");
#endif

	return OK;
        }


/* record log file version/info */
int write_log_file_info(time_t *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("write_log_file_info() start\n");
#endif

	/* write log version */
	snprintf(temp_buffer,sizeof(temp_buffer),"LOG VERSION: %s\n",LOG_VERSION_2);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_all_logs_with_timestamp(temp_buffer,NSLOG_PROCESS_INFO,timestamp);

#ifdef DEBUG0
	printf("write_log_file_info() end\n");
#endif

	return OK;
        }
