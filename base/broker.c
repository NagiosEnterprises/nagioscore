/*****************************************************************************
 *
 * BROKER.C - Event broker routines for Nagios
 *
 * Copyright (c) 2002-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   08-14-2003
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

#include "../include/nagios.h"
#include "../include/broker.h"

extern pthread_t       worker_threads[TOTAL_WORKER_THREADS];

extern circular_buffer event_broker_buffer;
extern char            *event_broker_file;
extern int             event_broker_options;

extern int             sigrestart;
extern int             sigshutdown;

int broker_file_open;
int event_broker_socket=-1;


#ifdef USE_EVENT_BROKER



/******************************************************************/
/************************* EVENT FUNCTIONS ************************/
/******************************************************************/


/* sends program data (starts, restarts, stops, etc.) to broker */
void broker_program_state(int type, int flags, int attr, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_PROGRAM_STATE))
		return;

	tv=get_broker_timestamp(timestamp);

	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,(int)getpid());
	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }


/* send timed event data to broker */
void broker_timed_event(int type, int flags, int attr, timed_event *event, void *data, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;
	service *temp_service;
	host *temp_host;

	if(!(event_broker_options & BROKER_TIMED_EVENTS))
		return;

	tv=get_broker_timestamp(timestamp);

	switch(type){
	case NEBTYPE_TIMEDEVENT_SLEEP:
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lf;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,*((double *)data));
		break;
	default:
		if(event==NULL)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr);
		else if(event->event_type==EVENT_SERVICE_CHECK){
			temp_service=(service *)event->event_data;
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%lu;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,event->event_type,event->run_time,temp_service->host_name,temp_service->description);
		        }
		else if(event->event_type==EVENT_HOST_CHECK){
			temp_host=(host *)event->event_data;
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%lu;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,event->event_type,event->run_time,temp_host->name);
		        }
		else
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%lu;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,event->event_type,event->run_time);
		break;
	        }
	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }



/* send log data to broker */
void broker_log_data(int type, int flags, int attr, char *data, unsigned long data_type, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_LOGGED_DATA))
		return;

	tv=get_broker_timestamp(timestamp);

	strip(data);
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lu;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,data_type,data);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }



/* send system command data to broker */
void broker_system_command(int type, int flags, int attr, double exectime, int timeout, int early_timeout, int retcode, char *cmd, char *output, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_SYSTEM_COMMANDS))
		return;
	
	if(cmd==NULL)
		return;

	tv=get_broker_timestamp(timestamp);

	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lf;%d;%d;%d;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,exectime,timeout,early_timeout,retcode,cmd,(output==NULL)?"":output);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }



/* send event handler data to broker */
void broker_event_handler(int type, int flags, int attr, void *data, int state, int state_type, double exectime, int timeout, int early_timeout, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;
	service *temp_service=NULL;
	host *temp_host=NULL;

	if(!(event_broker_options & BROKER_EVENT_HANDLERS))
		return;
	
	if(data==NULL)
		return;

	tv=get_broker_timestamp(timestamp);

	if(type==NEBTYPE_EVENTHANDLER_SERVICE || type==NEBTYPE_EVENTHANDLER_GLOBAL_SERVICE)
		temp_service=(service *)data;
	else
		temp_host=(host *)data;

	if(type==NEBTYPE_EVENTHANDLER_SERVICE || type==NEBTYPE_EVENTHANDLER_GLOBAL_SERVICE)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%d;%lf;%d;%d;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,state_type,exectime,timeout,early_timeout,temp_service->host_name,temp_service->description);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%d;%lf;%d;%d;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,state_type,exectime,timeout,early_timeout,temp_host->name);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }




/* send obsessive compulsive host/service  data to broker */
void broker_ocp_data(int type, int flags, int attr, void *data, int state, int state_type, double exectime, int timeout, int early_timeout, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;
	service *temp_service=NULL;
	host *temp_host=NULL;

	if(!(event_broker_options & BROKER_OCP_DATA))
		return;
	
	if(data==NULL)
		return;

	tv=get_broker_timestamp(timestamp);

	if(type==NEBTYPE_OCP_SERVICE)
		temp_service=(service *)data;
	else
		temp_host=(host *)data;

	if(type==NEBTYPE_OCP_SERVICE)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%d;%lf;%d;%d;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,state_type,exectime,timeout,early_timeout,temp_service->host_name,temp_service->description);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%d;%lf;%d;%d;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,state_type,exectime,timeout,early_timeout,temp_host->name);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }



/* send host check data to broker */
void broker_host_check(int type, int flags, int attr, host *hst, int state, double exectime, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_HOST_CHECKS))
		return;
	
	if(hst==NULL)
		return;

	tv=get_broker_timestamp(timestamp);

	if(type==NEBTYPE_HOSTCHECK_INITIATE)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lf;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,hst->latency,hst->name);
	else if(type==NEBTYPE_HOSTCHECK_PROCESSED)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%s;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,hst->name,hst->plugin_output,hst->perf_data);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%lf;%d;%d;%s;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,state,exectime,hst->current_attempt,hst->max_attempts,hst->name,hst->plugin_output,hst->perf_data);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }



/* send service check data to broker */
void broker_service_check(int type, int flags, int attr, service *svc, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_SERVICE_CHECKS))
		return;
	
	if(svc==NULL)
		return;

	tv=get_broker_timestamp(timestamp);

	if(type==NEBTYPE_SERVICECHECK_INITIATE)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lf;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,svc->latency,svc->host_name,svc->description);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%d;%lf;%d;%d;%s;%s;%s;%s;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,svc->current_state,svc->state_type,svc->execution_time,svc->current_attempt,svc->max_attempts,svc->host_name,svc->description,svc->plugin_output,svc->perf_data);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }



/* send comment data to broker */
void broker_comment_data(int type, int flags, int attr, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, unsigned long comment_id, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_COMMENT_DATA))
		return;
	
	tv=get_broker_timestamp(timestamp);

	if(attr==NEBATTR_HOST_COMMENT)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%lu;%s;%s;%d;%d;%lu;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,host_name,entry_time,author_name,comment_data,persistent,source,comment_id);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%s;%lu;%s;%s;%d;%d;%lu;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,host_name,svc_description,entry_time,author_name,comment_data,persistent,source,comment_id);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }



/* send downtime data to broker */
void broker_downtime_data(int type, int flags, int attr, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;

	if(!(event_broker_options & BROKER_DOWNTIME_DATA))
		return;
	
	tv=get_broker_timestamp(timestamp);

	if((attr & NEBATTR_HOST_DOWNTIME))
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%lu;%s;%s;%lu;%lu;%d;%lu;%lu;%lu;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,host_name,entry_time,author_name,comment_data,start_time,end_time,fixed,triggered_by,duration,downtime_id);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%s;%lu;%s;%s;%lu;%lu;%d;%lu;%lu;%lu;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,host_name,svc_description,entry_time,author_name,comment_data,start_time,end_time,fixed,triggered_by,duration,downtime_id);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }



/* send flapping data to broker */
void broker_flapping_data(int type, int flags, int attr, void *data, double percent_change, double threshold, struct timeval *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	service *temp_service;
	host *temp_host;
	struct timeval tv;

	if(!(event_broker_options & BROKER_FLAPPING_DATA))
		return;

	if(data==NULL)
		return;

	if(attr & NEBATTR_SERVICE_FLAPPING)
		temp_service=(service *)data;
	else
		temp_host=(host *)data;
		
	tv=get_broker_timestamp(timestamp);

	if(attr & NEBATTR_SERVICE_FLAPPING)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%s;%lf;%lf;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,temp_service->host_name,temp_service->description,percent_change,threshold);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s;%lf;%lf;\n",tv.tv_sec,(tv.tv_usec/1000),type,flags,attr,temp_host->name,percent_change,threshold);

	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }



/******************************************************************/
/************************* OUTPUT FUNCTIONS ***********************/
/******************************************************************/

/* sends data to the event broker worker thread */
int send_event_data_to_broker(char *data){
#ifdef BROKER_BLOCKS
	struct timeval tv;
#endif

	if(event_broker_options==BROKER_NOTHING)
		return OK;

	/* bail if we weren't given data */
	if(data==NULL)
		return ERROR;

	/* bail if the buffer is unallocated */
	if(event_broker_buffer.buffer==NULL){
		printf("BUFFER WAS NULL: %s\n",data);
		return ERROR;
	        }

	/* obtain a lock for writing to the buffer */
	pthread_mutex_lock(&event_broker_buffer.buffer_lock);

#ifdef BROKER_BLOCKS
	/* if the buffer is full and we're blocking , we'll wait... */
	while(event_broker_buffer.items==EVENT_BUFFER_SLOTS){

		/* if we're shutting down or restarting, don't block */
		if(sigrestart==TRUE || sigshutdown==TRUE)
			break;
		
		/* unlock the buffer */
		pthread_mutex_unlock(&event_broker_buffer.buffer_lock);

		/* wait a bit... */
		tv.tv_sec=0;
		tv.tv_usec=250000;
		select(0,NULL,NULL,NULL,&tv);

		/* lock the buffer for writing */
		pthread_mutex_lock(&event_broker_buffer.buffer_lock);
	        }
#else
	/* is the buffer full?  if it is and we're not blocking, we're going to lose some older data... */
	if(event_broker_buffer.items==EVENT_BUFFER_SLOTS){

		/* increment the overflow counter */
		event_broker_buffer.overflow++;

		/* free memory allocated to oldest data item */
		free(((char **)event_broker_buffer.buffer)[event_broker_buffer.head]);

		/* adjust tail pointer for overflow */
		event_broker_buffer.tail=(event_broker_buffer.tail + 1) % EVENT_BUFFER_SLOTS;

		printf("OVERFLOW: %s\n",data);
	        }
#endif

	/* save the data to the buffer */
	((char **)event_broker_buffer.buffer)[event_broker_buffer.head]=strdup(data);

	/* increment the index counters and number of items */
	event_broker_buffer.head=(event_broker_buffer.head + 1) % EVENT_BUFFER_SLOTS;
	if(event_broker_buffer.items<EVENT_BUFFER_SLOTS)
		event_broker_buffer.items++;

	/* unlock the buffer */
	pthread_mutex_unlock(&event_broker_buffer.buffer_lock);

	return OK;
        }



/* gets timestamp for use by broker */
struct timeval get_broker_timestamp(struct timeval *timestamp){
	struct timeval tv;

	if(timestamp==NULL)
		gettimeofday(&tv,NULL);
	else
		tv=*timestamp;

	return tv;
        }




/******************************************************************/
/************************ THREAD FUNCTIONS ************************/
/******************************************************************/

/* initializes event broker worker thread */
int init_event_broker_worker_thread(void){

	if(event_broker_options==BROKER_NOTHING)
		return OK;

	/* initialize socket */
	event_broker_socket=-1;

	/* initialize circular buffer */
	event_broker_buffer.head=0;
	event_broker_buffer.tail=0;
	event_broker_buffer.items=0;
	event_broker_buffer.overflow=0L;
	event_broker_buffer.buffer=(void **)malloc(EVENT_BUFFER_SLOTS*sizeof(char **));
	if(event_broker_buffer.buffer==NULL)
		return ERROR;

	/* initialize mutex */
	pthread_mutex_init(&event_broker_buffer.buffer_lock,NULL);

	return OK;
        }



/* starts event broker worker thread */
int start_event_broker_worker_thread(void){
	int result;
	sigset_t newmask;

	if(event_broker_options==BROKER_NOTHING)
		return OK;

	/* new thread should block all signals */
	sigfillset(&newmask);
	pthread_sigmask(SIG_BLOCK,&newmask,NULL);

	/* create worker thread */
	result=pthread_create(&worker_threads[EVENT_WORKER_THREAD],NULL,event_broker_worker_thread,NULL);

	/* main thread should unblock all signals */
	pthread_sigmask(SIG_UNBLOCK,&newmask,NULL);

#ifdef DEBUG1
	printf("EVENT BROKER THREAD: %lu\n",(unsigned long)worker_threads[EVENT_WORKER_THREAD]);
#endif

	if(result)
		return ERROR;

	return OK;
        }



/* shutdown event broker worker thread */
int shutdown_event_broker_worker_thread(void){

	if(event_broker_options==BROKER_NOTHING)
		return OK;

	/* tell the worker thread to exit */
	pthread_cancel(worker_threads[EVENT_WORKER_THREAD]);

	/* wait for the worker thread to exit */
	pthread_join(worker_threads[EVENT_WORKER_THREAD],NULL);

	return OK;
        }


/* clean up resources used by event broker worker thread */
void cleanup_event_broker_worker_thread(void *arg){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;
	int len;

	/* lock the buffer */
	pthread_mutex_lock(&event_broker_buffer.buffer_lock);

	/* report any overflows */
	if(event_broker_buffer.overflow>0){

		if(event_broker_socket>=0){
			gettimeofday(&tv,NULL);
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lu\n",tv.tv_sec,(tv.tv_usec/1000),NEBTYPE_INFO,NEBFLAG_NONE,NEBATTR_BUFFER_OVERFLOW,event_broker_buffer.overflow);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			len=strlen(temp_buffer);
			write_event_broker_data(event_broker_socket,temp_buffer,&len);
		        }

		/* reset overflow counter */
		event_broker_buffer.overflow=0L;
	        }

	/* flush remaining data in the buffer */
	while(event_broker_buffer.items>0){

		if(event_broker_socket>=0){
			len=strlen(((char **)event_broker_buffer.buffer)[event_broker_buffer.tail]);
			write_event_broker_data(event_broker_socket,((char **)event_broker_buffer.buffer)[event_broker_buffer.tail],&len);
		        }
				
#ifdef DEBUGBROKER
		printf("FLUSH: %s",((char **)event_broker_buffer.buffer)[event_broker_buffer.tail]);
#endif

		/* free buffer data */
		free(((char **)event_broker_buffer.buffer)[event_broker_buffer.tail]);

		/* update the tail counter and items */
		event_broker_buffer.tail=(event_broker_buffer.tail + 1) % EVENT_BUFFER_SLOTS;
		event_broker_buffer.items--;
	        }

	/* release memory allocated to circular buffer */
	free(event_broker_buffer.buffer);
	event_broker_buffer.buffer=NULL;

	/* release lock on buffer */
	pthread_mutex_unlock(&event_broker_buffer.buffer_lock);

	if(event_broker_socket>=0){

		/* say goodbye */
		gettimeofday(&tv,NULL);
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s\n",tv.tv_sec,(tv.tv_usec/1000),NEBTYPE_GOODBYE,NEBFLAG_NONE,NEBATTR_NONE,PROGRAM_VERSION);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		len=strlen(temp_buffer);
		write_event_broker_data(event_broker_socket,temp_buffer,&len);

		/* close socket */
		fsync(event_broker_socket);
		close(event_broker_socket);
	        }

	return;
        }


/* event broker thread - passes event data to external daemon */
void * event_broker_worker_thread(void *arg){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;
	struct timeval start_time;
	int write_result;
	int hello_attr;
	int x;
	int broker_socket_open;
	struct sockaddr_un sa;
	int len;

	/* specify cleanup routine */
	pthread_cleanup_push(cleanup_event_broker_worker_thread,NULL);

	/* set cancellation info */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);

	gettimeofday(&start_time,NULL);

	/***** MAKE THE CONNECTION (OR RECONNECT) TO BROKER SOCKET *****/

	/* you have to love GOTO statements... */
        brokerconnect:

#ifdef DEBUGBROKER
	printf("Attempting to (re)connect to socket...\n");
#endif

	/* initialize socket */
	while((event_broker_socket=socket(AF_UNIX,SOCK_STREAM,0))==-1){

#ifdef DEBUGBROKER
		printf("We couldn't initialize socket: %s\n",strerror(errno));
#endif

		for(x=0;x<10;x++){

			/* should we shutdown? */
			pthread_testcancel();
				
			/* wait a bit */
			tv.tv_sec=0;
			tv.tv_usec=500000;
			select(0,NULL,NULL,NULL,&tv);
		        }
	        }

	/* initialize socket address */
	sa.sun_family=AF_UNIX;
	strncpy(sa.sun_path,event_broker_file,108);
	sa.sun_path[107]='\x0';
	len=sizeof(sa.sun_family)+strlen(sa.sun_path);

	hello_attr=NEBATTR_NONE;

	/* connect to the socket */
	while(connect(event_broker_socket,(struct sockaddr *)&sa,len)==-1){

#ifdef DEBUGBROKER
		printf("We couldn't connect to broker socket: %s\n",strerror(errno));
#endif

		/* flag this as a potential error */
		if(!(hello_attr & NEBATTR_BROKERFILE_ERROR))
			hello_attr+=NEBATTR_BROKERFILE_ERROR;

		for(x=0;x<10;x++){

			/* should we shutdown? */
			pthread_testcancel();
				
			/* wait a bit */
			tv.tv_sec=0;
			tv.tv_usec=500000;
			select(0,NULL,NULL,NULL,&tv);
		        }
	        }

#ifdef DEBUGBROKER
	printf("We CONNECTED!\n");
#endif

	/* socket should be nonblocking */
	fcntl(event_broker_socket,F_SETFL,O_NONBLOCK);

	/* flag the socket as just having been opened */
	broker_socket_open=FALSE;

	while(1){

		/* should we shutdown? */
		pthread_testcancel();

		/* wait a bit */
		tv.tv_sec=0;
		tv.tv_usec=500000;
		select(0,NULL,NULL,NULL,&tv);

		/* should we shutdown? */
		pthread_testcancel();

		/* broker socket was just opened - say hello... */
		if(broker_socket_open==FALSE){

			gettimeofday(&tv,NULL);
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lu.%d;%s\n",tv.tv_sec,(tv.tv_usec/1000),NEBTYPE_HELLO,NEBFLAG_NONE,hello_attr,start_time.tv_sec,(start_time.tv_usec/1000),PROGRAM_VERSION);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			len=strlen(temp_buffer);
			write_result=write_event_broker_data(event_broker_socket,temp_buffer,&len);

			/* handle disconnects... */
			if(write_result==-1 && (errno!=EAGAIN && errno!=EWOULDBLOCK)){
				close(event_broker_socket);
				goto brokerconnect;
			        }

#ifdef DEBUGBROKER
			printf("SAID HELLO: %s\n",temp_buffer);
#endif

			broker_socket_open=TRUE;
		        }

		/* release lock on buffer */
		pthread_mutex_unlock(&event_broker_buffer.buffer_lock);

		/* process all data in the buffer */
		while(event_broker_buffer.items>0){

			/* obtain a lock for reading from the buffer */
			pthread_mutex_lock(&event_broker_buffer.buffer_lock);

			/* report the amount data lost due to overflows */
			if(event_broker_buffer.overflow>0){

				gettimeofday(&tv,NULL);
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lu\n",tv.tv_sec,(tv.tv_usec/1000),NEBTYPE_INFO,NEBFLAG_NONE,NEBATTR_BUFFER_OVERFLOW,event_broker_buffer.overflow);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				len=strlen(temp_buffer);
				write_result=write_event_broker_data(event_broker_socket,temp_buffer,&len);

				/* handle disconnects... */
				if(write_result==-1 && (errno!=EAGAIN && errno!=EWOULDBLOCK)){
					close(event_broker_socket);
					pthread_mutex_unlock(&event_broker_buffer.buffer_lock);
					goto brokerconnect;
			                }

				/* reset overflow counter */
				event_broker_buffer.overflow=0L;
		                }

			/* write this buffer item to the broker socket */
			len=strlen(((char **)event_broker_buffer.buffer)[event_broker_buffer.tail]);
			write_result=write_event_broker_data(event_broker_socket,((char **)event_broker_buffer.buffer)[event_broker_buffer.tail],&len);

#ifdef DEBUGBROKER
			printf("SENT: %s",((char **)event_broker_buffer.buffer)[event_broker_buffer.tail]);
#endif

			/* handle disconnects... */
			if(write_result==-1 && (errno!=EAGAIN && errno!=EWOULDBLOCK)){
				close(event_broker_socket);
				pthread_mutex_unlock(&event_broker_buffer.buffer_lock);
				goto brokerconnect;
			        }

			/* the write was okay */
			else{

				/* free buffer data */
				free(((char **)event_broker_buffer.buffer)[event_broker_buffer.tail]);

				/* update the tail counter and items */
				event_broker_buffer.tail=(event_broker_buffer.tail + 1) % EVENT_BUFFER_SLOTS;
				event_broker_buffer.items--;
			        }

			/* release lock on buffer */
			pthread_mutex_unlock(&event_broker_buffer.buffer_lock);

			/* should we shutdown? */
			pthread_testcancel();
                        }
	        }

	/* removes cleanup handler */
	pthread_cleanup_pop(0);

	return NULL;
        }


/* sends data to event broker worker socket - modelled after Beej's sendall() function */
int write_event_broker_data(int sock, char *buf, int *len){
	int total=0;
	int bytesleft=*len;
	int n=0;
	struct pollfd pfd;
	int pollval;

	/* check for null string */
	if(buf==NULL){
		*len=0;
		return 0;
	        }

	while(total<*len){

		/* try and send whatever is left */
		n=send(sock,buf+total,bytesleft,MSG_NOSIGNAL);

		/* a critical error occurred, so bail */
		if(n==-1 && errno!=EAGAIN && errno!=EWOULDBLOCK)
			break;

		/* wait a bit before we retry the send */
		if(n==-1){
#ifdef DEBUGBROKER
			printf("Send Error: %s\n",(errno==EAGAIN)?"EAGAIN":"EWOULDBLOCK");
#endif
			pfd.fd=sock;
			pfd.events=POLLOUT;
			pollval=poll(&pfd,1,100);

			/* bail if there are critical errors */
			if(pollval>0 && ((pfd.revents & POLLHUP) || (pfd.revents & POLLERR) || (pfd.revents & POLLNVAL)))
				break;
		        }

		/* we were able to send some data, so update counters */
		if(n>=0){
			total+=n;
			bytesleft-=n;
		        }
	        }

	/* return number of bytes actually sent here */
	*len=total;

	/* return -1 on failure, 0 on success */
	return (n==-1)?-1:0;
        }
#endif

