/*****************************************************************************
 *
 * BROKER.C - Event broker routines for Nagios
 *
 * Copyright (c) 2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-17-2002
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

#include "nagios.h"
#include "broker.h"

extern circular_buffer event_broker_buffer;
extern char            *event_broker_file;
extern int             event_broker_options;

int event_broker_fd=-1;
int broker_file_open;



/******************************************************************/
/************************* EVENT FUNCTIONS ************************/
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
	if(event_broker_buffer.buffer==NULL)
		return ERROR;

	/* obtain a lock for writing to the buffer */
	pthread_mutex_lock(&event_broker_buffer.buffer_lock);

#ifdef BROKER_BLOCKS
	/* if the buffer is full and we're blocking , we'll wait... */
	while(event_broker_buffer.items==EVENT_BUFFER_SLOTS){
		
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
		free(((char **)event_broker_buffer.buffer)[event_broker_buffer.tail]);
	        }
#endif

	/* save the data to the buffer */
	((char **)event_broker_buffer.buffer)[event_broker_buffer.tail]=strdup(data);

	/* increment the index counters and number of items */
	event_broker_buffer.tail=(event_broker_buffer.tail + 1) % EVENT_BUFFER_SLOTS;
	if(event_broker_buffer.items<EVENT_BUFFER_SLOTS)
		event_broker_buffer.items++;

	/* unlock the buffer */
	pthread_mutex_unlock(&event_broker_buffer.buffer_lock);

	return OK;
        }



/* sends program data (starts, restarts, stops, etc.) to broker */
void broker_program_state(int type, int flags, int attr, struct timeb *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeb tb;

	if(!(event_broker_options & BROKER_PROGRAM_STATE))
		return;

	if(timestamp==NULL)
		ftime(&tb);
	else
		tb=*timestamp;

	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d\n",(unsigned long)tb.time,tb.millitm,type,flags,attr,(int)getpid());
	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }


/* send timed event data to broker */
void broker_timed_event(int type, int flags, int attr, timed_event *event, void *data, struct timeb *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeb tb;
	service *temp_service;

	if(!(event_broker_options & BROKER_TIMED_EVENTS))
		return;

	if(timestamp==NULL)
		ftime(&tb);
	else
		tb=*timestamp;

	switch(type){
	case NEBTYPE_TIMEDEVENT_SLEEP:
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lf\n",(unsigned long)tb.time,tb.millitm,type,flags,attr,*((double *)data));
		break;
	default:
		if(event==NULL)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;\n",(unsigned long)tb.time,tb.millitm,type,flags,attr);
		else if(event->event_type==EVENT_SERVICE_CHECK){
			temp_service=(service *)event->event_data;
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%lu;%s;%s;\n",(unsigned long)tb.time,tb.millitm,type,flags,attr,event->event_type,event->run_time,temp_service->host_name,temp_service->description);
		        }
		else
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%d;%lu;\n",(unsigned long)tb.time,tb.millitm,type,flags,attr,event->event_type,event->run_time);
		break;
	        }
	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }



/* send log data to broker */
void broker_logged_data(int type, int flags, int attr, char *data, unsigned long data_type, struct timeb *timestamp){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeb tb;
	service *temp_service;

	if(!(event_broker_options & BROKER_LOGGED_DATA))
		return;

	if(timestamp==NULL)
		ftime(&tb);
	else
		tb=*timestamp;

	strip(data);
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lu;%s\n",(unsigned long)tb.time,tb.millitm,type,flags,attr,data_type,data);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	send_event_data_to_broker(temp_buffer);

	return;
        }




/******************************************************************/
/************************ THREAD FUNCTIONS ************************/
/******************************************************************/

/* event broker thread - passed event data to external daemon */
void * event_broker_worker_thread(void *arg){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeval tv;
	struct timeb start_time;
	struct timeb tb;
	sigset_t newmask;
	int write_result;
	int open_failed;
	int hello_attr;
	int x;

	/* this thread should block all signals */
	sigfillset(&newmask);
	pthread_sigmask(SIG_BLOCK,&newmask,NULL);

	/* specify cleanup routine */
	pthread_cleanup_push(cleanup_event_broker_worker_thread,NULL);

	/* set cancellation info */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);

	broker_file_open=FALSE;
	open_failed=FALSE;
	event_broker_fd=-1;

	hello_attr=NEBATTR_NONE;

	ftime(&start_time);

	while(1){

		/* open the event broker file for writing (non-blocked) */
		if(event_broker_fd<0){

			/* open the named pipe for writing */
			event_broker_fd=open(event_broker_file,O_WRONLY | O_NONBLOCK | O_APPEND,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

			/* wait a while if we couldn't open the file */
			if(event_broker_fd<0){

				/* flag this as a potential error */
				if(open_failed==FALSE)
					hello_attr+=NEBATTR_BROKERFILE_ERROR;
				open_failed=TRUE;

				for(x=0;x<10;x++){

					/* should we shutdown? */
					pthread_testcancel();
				
					/* wait a bit */
					tv.tv_sec=0;
					tv.tv_usec=500000;
					select(0,NULL,NULL,NULL,&tv);
				        }

				continue;
			        }
		        }

		/* should we shutdown? */
		pthread_testcancel();

		/* wait a bit */
		tv.tv_sec=0;
		tv.tv_usec=500000;
		select(0,NULL,NULL,NULL,&tv);

		/* should we shutdown? */
		pthread_testcancel();

		/* broker file was just opened - say hello... */
		if(broker_file_open==FALSE){
			ftime(&tb);
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lu.%d;%s\n",(unsigned long)tb.time,tb.millitm,NEBTYPE_HELLO,NEBFLAG_NONE,hello_attr,(unsigned long)start_time.time,start_time.millitm,PROGRAM_VERSION);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write(event_broker_fd,temp_buffer,strlen(temp_buffer));
			broker_file_open=TRUE;
		        }

		/* obtain a lock for reading from the buffer */
		pthread_mutex_lock(&event_broker_buffer.buffer_lock);

		/* report the amount data lost due to overflows */
		if(event_broker_buffer.overflow>0){
			ftime(&tb);
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%lu\n",(unsigned long)tb.time,tb.millitm,NEBTYPE_INFO,NEBFLAG_NONE,NEBATTR_BUFFER_OVERFLOW,event_broker_buffer.overflow);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write(event_broker_fd,temp_buffer,strlen(temp_buffer));
			event_broker_buffer.overflow=0L;
		        }

		/* process all data in the buffer */
		if(event_broker_buffer.items>0){

			write(event_broker_fd,((char **)event_broker_buffer.buffer)[event_broker_buffer.head],strlen(((char **)event_broker_buffer.buffer)[event_broker_buffer.head]));

			/* free buffer data */
			free(((char **)event_broker_buffer.buffer)[event_broker_buffer.head]);

			/* update the head counter and items */
			event_broker_buffer.head=(event_broker_buffer.head + 1) % EVENT_BUFFER_SLOTS;
			event_broker_buffer.items--;

			/* should we shutdown? */
			pthread_testcancel();
                        }

		/* release lock on buffer */
		pthread_mutex_unlock(&event_broker_buffer.buffer_lock);
	        }

	/* removes cleanup handler */
	pthread_cleanup_pop(0);

	return NULL;
        }



/* clean up resources used by event broker worker thread */
void * cleanup_event_broker_worker_thread(void *arg){
	char temp_buffer[MAX_INPUT_BUFFER];
	struct timeb tb;
	int x;

	/* lock the buffer */
	pthread_mutex_lock(&event_broker_buffer.buffer_lock);

	/* flush remaining data in the buffer */
	while(event_broker_buffer.items>0){

		if(broker_file_open==TRUE)
			write(event_broker_fd,((char **)event_broker_buffer.buffer)[event_broker_buffer.head],strlen(((char **)event_broker_buffer.buffer)[event_broker_buffer.head]));
				
		/* free buffer data */
		free(((char **)event_broker_buffer.buffer)[event_broker_buffer.head]);

		/* update the head counter and items */
		event_broker_buffer.head=(event_broker_buffer.head + 1) % EVENT_BUFFER_SLOTS;
		event_broker_buffer.items--;
	        }

	/* release memory allocated to circular buffer */
	for(x=event_broker_buffer.head;x!=event_broker_buffer.tail;x=(x+1) % EVENT_BUFFER_SLOTS)
		free(((char **)event_broker_buffer.buffer)[x]);
	free(event_broker_buffer.buffer);
	event_broker_buffer.buffer=NULL;

	/* release lock on buffer */
	pthread_mutex_unlock(&event_broker_buffer.buffer_lock);

	if(broker_file_open==TRUE){

		/* say goodbye */
		ftime(&tb);
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu.%d] %d;%d;%d;%s\n",(unsigned long)tb.time,tb.millitm,NEBTYPE_GOODBYE,NEBFLAG_NONE,NEBATTR_NONE,PROGRAM_VERSION);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write(event_broker_fd,temp_buffer,strlen(temp_buffer));

		/* close file */
		fsync(event_broker_fd);
		close(event_broker_fd);
	        }

	return NULL;
        }

