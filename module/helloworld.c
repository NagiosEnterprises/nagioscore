/*****************************************************************************
 *
 * HELLOWORLD.C - Example of a simple NEB module
 *
 * Copyright (c) 2003-2007 Ethan Galstad (https://www.nagios.org)
 *
 * Description:
 *
 * This is an example of a very basic module.  It does nothing useful other
 * than logging some messages to the main Nagios log file when it is initialized
 * (loaded), when it is closed (unloaded), and when aggregated status updates
 * occur.  I would not call that too useful, but hopefully it will serve as a
 * very basic example of how to write a NEB module...
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

/* include other event broker header files that we need for our work */
#include "../include/nebstructs.h"
#include "../include/broker.h"

/* include some Nagios stuff as well */
#include "../include/config.h"
#include "../include/common.h"
#include "../include/nagios.h"

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION);


void *helloworld_module_handle = NULL;

void helloworld_reminder_message(char *);
int helloworld_handle_data(int, void *);


/* this function gets called when the module is loaded by the event broker */
int nebmodule_init(int flags, char *args, nebmodule *handle) {
	char temp_buffer[1024];
	time_t current_time;
	unsigned long interval;

	/* save our handle */
	helloworld_module_handle = handle;

	/* set some info - this is completely optional, as Nagios doesn't do anything with this data */
	neb_set_module_info(helloworld_module_handle, NEBMODULE_MODINFO_TITLE, "helloworld");
	neb_set_module_info(helloworld_module_handle, NEBMODULE_MODINFO_AUTHOR, "Ethan Galstad");
	neb_set_module_info(helloworld_module_handle, NEBMODULE_MODINFO_TITLE, "Copyright (c) 2003-2007 Ethan Galstad");
	neb_set_module_info(helloworld_module_handle, NEBMODULE_MODINFO_VERSION, "noversion");
	neb_set_module_info(helloworld_module_handle, NEBMODULE_MODINFO_LICENSE, "GPL v2");
	neb_set_module_info(helloworld_module_handle, NEBMODULE_MODINFO_DESC, "A simple example to get you started with Nagios Event Broker (NEB) modules.");

	/* log module info to the Nagios log file */
	write_to_all_logs("helloworld: Copyright (c) 2003-2007 Ethan Galstad (egalstad@nagios.org)", NSLOG_INFO_MESSAGE);

	/* log a message to the Nagios log file */
	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "helloworld: Hello world!\n");
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

	/* log a reminder message every 15 minutes (how's that for annoying? :-)) */
	time(&current_time);
	interval = 900;
	schedule_new_event(EVENT_USER_FUNCTION, TRUE, current_time + interval, TRUE, interval, NULL, TRUE, (void *)helloworld_reminder_message, "How about you?", 0);

	/* register to be notified of certain events... */
	neb_register_callback(NEBCALLBACK_AGGREGATED_STATUS_DATA, helloworld_module_handle, 0, helloworld_handle_data);

	return 0;
	}


/* this function gets called when the module is unloaded by the event broker */
int nebmodule_deinit(int flags, int reason) {
	char temp_buffer[1024];

	/* deregister for all events we previously registered for... */
	neb_deregister_callback(NEBCALLBACK_AGGREGATED_STATUS_DATA, helloworld_handle_data);

	/* log a message to the Nagios log file */
	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "helloworld: Goodbye world!\n");
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

	return 0;
	}


/* gets called every X minutes by an event in the scheduling queue */
void helloworld_reminder_message(char *message) {
	char temp_buffer[1024];

	/* log a message to the Nagios log file */
	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "helloworld: I'm still here! %s", message);
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

	return;
	}


/* handle data from Nagios daemon */
int helloworld_handle_data(int event_type, void *data) {
	nebstruct_aggregated_status_data *agsdata = NULL;
	char temp_buffer[1024];

	/* what type of event/data do we have? */
	switch(event_type) {

		case NEBCALLBACK_AGGREGATED_STATUS_DATA:

			/* an aggregated status data dump just started or ended... */
			if((agsdata = (nebstruct_aggregated_status_data *)data)) {

				/* log a message to the Nagios log file */
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "helloworld: An aggregated status update just %s.", (agsdata->type == NEBTYPE_AGGREGATEDSTATUS_STARTDUMP) ? "started" : "finished");
				temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
				write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
				}

			break;

		default:
			break;
		}

	return 0;
	}

