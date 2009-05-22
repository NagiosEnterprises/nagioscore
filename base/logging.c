/*****************************************************************************
 *
 * LOGGING.C - Log file functions for use with Nagios
 *
 * Copyright (c) 1999-2007 Ethan Galstad (egalstad@nagios.org)
 * Last Modified: 10-28-2007
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
#include "../include/macros.h"
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

extern char     *debug_file;
extern int      debug_level;
extern int      debug_verbosity;
extern unsigned long max_debug_file_size;
FILE            *debug_file_fp=NULL;




/******************************************************************/
/************************ LOGGING FUNCTIONS ***********************/
/******************************************************************/

/* This needs to be a function rather than a macro. C99 introduces
 * variadic macros, but we need to support compilers that aren't
 * C99 compliant in that area, so a function it is. Hopefully most
 * compilers will just optimize this call away, as it's easily
 * recognizable as not doing anything at all */
void logit(int data_type, int display, const char *fmt, ...){
	int len;
	va_list ap;
	char *buffer=NULL;

	va_start(ap,fmt);
	if((len=vasprintf(&buffer,fmt,ap))>0){
		write_to_logs_and_console(buffer,data_type,display);
		free(buffer);
		}
	va_end(ap);

	return;
	}


/* write something to the log file, syslog, and possibly the console */
int write_to_logs_and_console(char *buffer, unsigned long data_type, int display){
	register int len=0;
	register int x=0;

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
	if(display==TRUE){

		/* don't display warnings if we're just testing scheduling */
		if(test_scheduling==TRUE && data_type==NSLOG_VERIFICATION_WARNING)
			return OK;

		write_to_console(buffer);
	        }

	return OK;
        }


/* write something to the console */
int write_to_console(char *buffer){

	/* should we print to the console? */
	if(daemon_mode==FALSE)
		printf("%s\n",buffer);

	return OK;
        }


/* write something to the log file and syslog facility */
int write_to_all_logs(char *buffer, unsigned long data_type){

	/* write to syslog */
	write_to_syslog(buffer,data_type);

	/* write to main log */
	write_to_log(buffer,data_type,NULL);

	return OK;
        }


/* write something to the log file and syslog facility */
int write_to_all_logs_with_timestamp(char *buffer, unsigned long data_type, time_t *timestamp){

	/* write to syslog */
	write_to_syslog(buffer,data_type);

	/* write to main log */
	write_to_log(buffer,data_type,timestamp);

	return OK;
        }


/* write something to the nagios log file */
int write_to_log(char *buffer, unsigned long data_type, time_t *timestamp){
	FILE *fp=NULL;
	time_t log_time=0L;

	if(buffer==NULL)
		return ERROR;

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

	return OK;
	}


/* write something to the syslog facility */
int write_to_syslog(char *buffer, unsigned long data_type){

	if(buffer==NULL)
		return ERROR;

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

	return OK;
	}


/* write a service problem/recovery to the nagios log file */
int log_service_event(service *svc){
	char *temp_buffer=NULL;
	char *processed_buffer=NULL;
	unsigned long log_options=0L;
	host *temp_host=NULL;

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
	if((temp_host=svc->host_ptr)==NULL)
		return ERROR;

	/* grab service macros */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);

	asprintf(&temp_buffer,"SERVICE ALERT: %s;%s;$SERVICESTATE$;$SERVICESTATETYPE$;$SERVICEATTEMPT$;%s\n",svc->host_name,svc->description,(svc->plugin_output==NULL)?"":svc->plugin_output);
	process_macros(temp_buffer,&processed_buffer,0);

	write_to_all_logs(processed_buffer,log_options);

	my_free(temp_buffer);
	my_free(processed_buffer);

	return OK;
	}


/* write a host problem/recovery to the log file */
int log_host_event(host *hst){
	char *temp_buffer=NULL;
	char *processed_buffer=NULL;
	unsigned long log_options=0L;

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


	asprintf(&temp_buffer,"HOST ALERT: %s;$HOSTSTATE$;$HOSTSTATETYPE$;$HOSTATTEMPT$;%s\n",hst->name,(hst->plugin_output==NULL)?"":hst->plugin_output);
	process_macros(temp_buffer,&processed_buffer,0);

	write_to_all_logs(processed_buffer,log_options);

	my_free(temp_buffer);
	my_free(processed_buffer);

	return OK;
        }


/* logs host states */
int log_host_states(int type, time_t *timestamp){
	char *temp_buffer=NULL;
	char *processed_buffer=NULL;
	host *temp_host=NULL;;

	/* bail if we shouldn't be logging initial states */
	if(type==INITIAL_STATES && log_initial_states==FALSE)
		return OK;

	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* grab the host macros */
		clear_volatile_macros();
		grab_host_macros(temp_host);

		asprintf(&temp_buffer,"%s HOST STATE: %s;$HOSTSTATE$;$HOSTSTATETYPE$;$HOSTATTEMPT$;%s\n",(type==INITIAL_STATES)?"INITIAL":"CURRENT",temp_host->name,(temp_host->plugin_output==NULL)?"":temp_host->plugin_output);
		process_macros(temp_buffer,&processed_buffer,0);
		
		write_to_all_logs_with_timestamp(processed_buffer,NSLOG_INFO_MESSAGE,timestamp);

		my_free(temp_buffer);
		my_free(processed_buffer);
	        }

	return OK;
        }


/* logs service states */
int log_service_states(int type, time_t *timestamp){
	char *temp_buffer=NULL;
	char *processed_buffer=NULL;
	service *temp_service=NULL;
	host *temp_host=NULL;;

	/* bail if we shouldn't be logging initial states */
	if(type==INITIAL_STATES && log_initial_states==FALSE)
		return OK;

	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		/* find the associated host */
		if((temp_host=temp_service->host_ptr)==NULL)
			continue;

		/* grab service macros */
		clear_volatile_macros();
		grab_host_macros(temp_host);
		grab_service_macros(temp_service);

		asprintf(&temp_buffer,"%s SERVICE STATE: %s;%s;$SERVICESTATE$;$SERVICESTATETYPE$;$SERVICEATTEMPT$;%s\n",(type==INITIAL_STATES)?"INITIAL":"CURRENT",temp_service->host_name,temp_service->description,temp_service->plugin_output);
		process_macros(temp_buffer,&processed_buffer,0);

		write_to_all_logs_with_timestamp(processed_buffer,NSLOG_INFO_MESSAGE,timestamp);

		my_free(temp_buffer);
		my_free(processed_buffer);
	        }

	return OK;
        }


/* rotates the main log file */
int rotate_log_file(time_t rotation_time){
	char *temp_buffer=NULL;
	char method_string[16]="";
	char *log_archive=NULL;
	struct tm *t;
	int rename_result=0;
	int stat_result=-1;
	struct stat log_file_stat;

	if(log_rotation_method==LOG_ROTATION_NONE){
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

	stat_result = stat(log_file, &log_file_stat);

	/* get the archived filename to use */
	asprintf(&log_archive,"%s%snagios-%02d-%02d-%d-%02d.log",log_archive_path,(log_archive_path[strlen(log_archive_path)-1]=='/')?"":"/",t->tm_mon+1,t->tm_mday,t->tm_year+1900,t->tm_hour);

	/* rotate the log file */
	rename_result=my_rename(log_file,log_archive);

	if(rename_result){
		my_free(log_archive);
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
	asprintf(&temp_buffer,"LOG ROTATION: %s\n",method_string);
	write_to_all_logs_with_timestamp(temp_buffer,NSLOG_PROCESS_INFO,&rotation_time);
	my_free(temp_buffer);

	/* record log file version format */
	write_log_file_info(&rotation_time);

	if(stat_result==0){
		chmod(log_file, log_file_stat.st_mode);
		chown(log_file, log_file_stat.st_uid, log_file_stat.st_gid);
		}

	/* log current host and service state */
	log_host_states(CURRENT_STATES,&rotation_time);
	log_service_states(CURRENT_STATES,&rotation_time);

	/* free memory */
	my_free(log_archive);

	return OK;
        }


/* record log file version/info */
int write_log_file_info(time_t *timestamp){
	char *temp_buffer=NULL;

	/* write log version */
	asprintf(&temp_buffer,"LOG VERSION: %s\n",LOG_VERSION_2);
	write_to_all_logs_with_timestamp(temp_buffer,NSLOG_PROCESS_INFO,timestamp);
	my_free(temp_buffer);

	return OK;
        }


/* opens the debug log for writing */
int open_debug_log(void){

	/* don't do anything if we're not actually running... */
	if(verify_config==TRUE || test_scheduling==TRUE)
		return OK;

	/* don't do anything if we're not debugging */
	if(debug_level==DEBUGL_NONE)
		return OK;

	if((debug_file_fp=fopen(debug_file,"a+"))==NULL)
		return ERROR;

	return OK;
	}


/* closes the debug log */
int close_debug_log(void){

	if(debug_file_fp!=NULL)
		fclose(debug_file_fp);
	
	debug_file_fp=NULL;

	return OK;
	}


/* write to the debug log */
int log_debug_info(int level, int verbosity, const char *fmt, ...){
	va_list ap;
	char *temp_path=NULL;
	struct timeval current_time;

	if(!(debug_level==DEBUGL_ALL || (level & debug_level)))
		return OK;

	if(verbosity>debug_verbosity)
		return OK;

	if(debug_file_fp==NULL)
		return ERROR;

	/* write the timestamp */
	gettimeofday(&current_time,NULL);
	fprintf(debug_file_fp,"[%lu.%06lu] [%03d.%d] [pid=%lu] ",current_time.tv_sec,current_time.tv_usec,level,verbosity,(unsigned long)getpid());

	/* write the data */
	va_start(ap,fmt);
	vfprintf(debug_file_fp,fmt,ap);
	va_end(ap);

	/* flush, so we don't have problems tailing or when fork()ing */
	fflush(debug_file_fp);

	/* if file has grown beyond max, rotate it */
	if((unsigned long)ftell(debug_file_fp)>max_debug_file_size && max_debug_file_size>0L){

		/* close the file */
		close_debug_log();
		
		/* rotate the log file */
		asprintf(&temp_path,"%s.old",debug_file);
		if(temp_path){

			/* unlink the old debug file */
			unlink(temp_path);

			/* rotate the debug file */
			my_rename(debug_file,temp_path);

			/* free memory */
			my_free(temp_path);
			}

		/* open a new file */
		open_debug_log();
		}

	return OK;
	}

