/*****************************************************************************
 *
 * HELLOWORLD.C - Example of a simple NEB module
 *
 * Copyright (c) 2003 Ethan Galstad (nagios@nagios.org)
 *
 * Last Modified:   08-15-2003
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


/* this function gets called when the module is loaded by the event broker*/
int nebmodule_init(int flags, char *args, nebmodule *handle){
	char temp_buffer[1024];

	/* log module info to the Nagios log file */
	write_to_logs_and_console("helloworld: Copyright (c) 2003 Ethan Galstad (nagios@nagios.org)",NSLOG_INFO_MESSAGE,FALSE);
	
	/* log a message to the Nagios log file */
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"helloworld: Hello world! (flags=%d, args=%s)\n",flags,(args==NULL)?"NULL":args);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_INFO_MESSAGE,FALSE);

	return 0;
        }


/* this function gets called when the module is unoaded by the event broker */
int nebmodule_deinit(int flags, int reason){
	char temp_buffer[1024];
	
	/* log a message to the Nagios log file */
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"helloworld: Goodbye world! (flags=%d, reason=%d)\n",flags,reason);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_INFO_MESSAGE,FALSE);

	return 0;
        }
