/*****************************************************************************
 *
 * HELLOWORLD.C - Example of a simple NEB module
 *
 * Copyright (c) 2003-2004 Ethan Galstad (nagios@nagios.org)
 *
 * Last Modified: 11-05-2004
 *
 * Description:
 *
 * This is an example of a very basic module.  It does nothing useful other
 * than printing a message when it is initialized (loaded) and closed
 * (unloaded).  I would not call that useful...
 *
 * Instructions:
 * 
 * Compile with the following command:
 *
 *     gcc -shared -o helloworld.o helloworld.c
 *
 *****************************************************************************/

/* include (minimum required) event broker header files */
#include "../include/nebmodules.h"
#include "../include/nebcallbacks.h"

/* include some Nagios stuff as well */
#include "../include/config.h"
#include "../include/common.h"
#include "../include/nagios.h"

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION);

void reminder_message(char *);


/* this function gets called when the module is loaded by the event broker */
int nebmodule_init(int flags, char *args, nebmodule *handle){
	char temp_buffer[1024];
	time_t current_time;
	unsigned long interval;

	/* log module info to the Nagios log file */
	write_to_all_logs("helloworld: Copyright (c) 2003 Ethan Galstad (nagios@nagios.org)",NSLOG_INFO_MESSAGE);
	
	/* log a message to the Nagios log file */
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"helloworld: Hello world!\n");
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_all_logs(temp_buffer,NSLOG_INFO_MESSAGE);

	/* log a reminder message every 15 minutes (how's that for annoying? :-)) */
	time(&current_time);
	interval=900;
	schedule_new_event(EVENT_USER_FUNCTION,TRUE,current_time+interval,TRUE,interval,NULL,TRUE,reminder_message,"How about you?");

	return 0;
        }


/* this function gets called when the module is unloaded by the event broker */
int nebmodule_deinit(int flags, int reason){
	char temp_buffer[1024];
	
	/* log a message to the Nagios log file */
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"helloworld: Goodbye world!\n");
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_all_logs(temp_buffer,NSLOG_INFO_MESSAGE);

	return 0;
        }


/* gets called every X minutes by an event in the scheduling queue */
void reminder_message(char *message){
	char temp_buffer[1024];
	
	/* log a message to the Nagios log file */
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"helloworld: I'm still here! %s",message);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_all_logs(temp_buffer,NSLOG_INFO_MESSAGE);

	return;
        }

