/*****************************************************************************
 *
 * LOGGING.C - Log file functions for use with Nagios
 *
 * Copyright (c) 1999-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   02-18-2003
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
#include "../common/statusdata.h"
#include "nagios.h"
#include "broker.h"


extern char	*log_file;
extern char     *temp_file;
extern char	*log_archive_path;

extern char     *macro_x[MACRO_X_COUNT];

extern int	use_syslog;
extern int      log_service_retries;

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

#ifdef DEBUG0
	printf("write_to_logs_and_console() start\n");
#endif

	/* write messages to the log unless we are just verifying the data or testing scheduling... */
	if(verify_config==FALSE && test_scheduling==FALSE){

		/* write to syslog */
		write_to_syslog(buffer,data_type);

		/* write to main log */
		write_to_log(buffer,data_type);
	        }

	/* should we print to the console? */
	if(display==TRUE && daemon_mode==FALSE)
		printf("%s\n",buffer);

#ifdef DEBUG0
	printf("write_to_logs_and_console() end\n");
#endif

	return OK;
        }



/* write something to the nagios log file */
int write_to_log(char *buffer, unsigned long data_type){
	FILE *fp;
	time_t t;


#ifdef DEBUG0
	printf("write_to_log() start\n");
#endif

	/* make sure we can log this type of entry */
	if(!(data_type & logging_options))
		return OK;

	fp=fopen(log_file,"a+");
	if(fp==NULL){
		printf("Warning: Cannot open log file '%s' for writing\n",log_file);
		return ERROR;
		}

	/* get the current time */
	time(&t);

	/* strip any newlines from the end of the buffer */
	strip(buffer);

	/* write the buffer to the log file */
	fprintf(fp,"[%lu] %s\n",t,buffer);

	fclose(fp);

#ifdef USE_EVENT_BROKER
	/* send data to the event broker */
	broker_log_data(NEBTYPE_LOG_DATA,NEBFLAG_NONE,NEBATTR_NONE,buffer,data_type,NULL);
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
int log_service_event(service *svc,int state_type){
	char temp_buffer[MAX_INPUT_BUFFER];
	unsigned long log_options;
	host *temp_host;

#ifdef DEBUG0
	printf("log_service_event() start\n");
#endif

	/* don't log soft errors if the user doesn't want to */
	if(state_type==SOFT_STATE && !log_service_retries)
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

	snprintf(temp_buffer,sizeof(temp_buffer),"SERVICE ALERT: %s;%s;%s;%s;%s;%s\n",svc->host_name,svc->description,macro_x[MACRO_SERVICESTATE],(state_type==SOFT_STATE)?"SOFT":"HARD",macro_x[MACRO_SERVICEATTEMPT],svc->plugin_output);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,log_options,FALSE);


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

	/* get the log options */
	if(hst->current_state==HOST_DOWN)
		log_options=NSLOG_HOST_DOWN;
	else if(hst->current_state==HOST_UNREACHABLE)
		log_options=NSLOG_HOST_UNREACHABLE;
	else
		log_options=NSLOG_HOST_UP;


	snprintf(temp_buffer,sizeof(temp_buffer),"HOST ALERT: %s;%s;%s;%s;%s\n",hst->name,macro_x[MACRO_HOSTSTATE],macro_x[MACRO_HOSTSTATETYPE],macro_x[MACRO_HOSTATTEMPT],hst->plugin_output);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,log_options,FALSE);

#ifdef DEBUG0
	printf("log_host_event() start\n");
#endif
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

	/* record the log rotation after it has been done... */
	snprintf(temp_buffer,sizeof(temp_buffer),"LOG ROTATION: %s\n",method_string);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO,FALSE);

	/* record log file version format */
	write_log_file_info();

#ifdef USE_EVENT_BROKER
	/* send data to the event broker */
	broker_log_data(NEBTYPE_LOG_ROTATION,NEBFLAG_NONE,NEBATTR_NONE,log_archive,log_rotation_method,NULL);
#endif

#ifdef DEBUG3
	printf("\tRotated main log file to '%s'\n",log_archive);
#endif

#ifdef DEBUG0
	printf("rotate_log_file() end\n");
#endif

	return OK;
        }


/* record log file version/info */
int write_log_file_info(void){
	char temp_buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("write_log_file_info() start\n");
#endif

	/* write log version */
	snprintf(temp_buffer,sizeof(temp_buffer),"LOG VERSION: %s\n",LOG_VERSION_2);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO,FALSE);

#ifdef DEBUG0
	printf("write_log_file_info() end\n");
#endif

	return OK;
        }
