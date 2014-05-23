/*****************************************************************************
 *
 * STATUSDATA.C - External status data for Nagios CGIs
 *
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

/*********** COMMON HEADER FILES ***********/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"
#include "../xdata/xsddefault.h"		/* default routines */

#ifdef NSCGI
#include "../include/cgiutils.h"
#else
#include "../include/nagios.h"
#include "../include/broker.h"
#endif


#ifdef NSCGI
hoststatus      *hoststatus_list = NULL;
hoststatus      *hoststatus_list_tail = NULL;
servicestatus   *servicestatus_list = NULL;
servicestatus   *servicestatus_list_tail = NULL;

hoststatus      **hoststatus_hashlist = NULL;
servicestatus   **servicestatus_hashlist = NULL;

extern int      use_pending_states;
#endif



#ifdef NSCORE

/******************************************************************/
/****************** TOP-LEVEL OUTPUT FUNCTIONS ********************/
/******************************************************************/

/* initializes status data at program start */
int initialize_status_data(const char *cfgfile) {
	return xsddefault_initialize_status_data(cfgfile);
	}


/* update all status data (aggregated dump) */
int update_all_status_data(void) {
	int result = OK;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_aggregated_status_data(NEBTYPE_AGGREGATEDSTATUS_STARTDUMP, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

	result = xsddefault_save_status_data();

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_aggregated_status_data(NEBTYPE_AGGREGATEDSTATUS_ENDDUMP, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif
	return result;
	}


/* cleans up status data before program termination */
int cleanup_status_data(int delete_status_data) {
	return xsddefault_cleanup_status_data(delete_status_data);
	}



/* updates program status info */
int update_program_status(int aggregated_dump) {

#ifdef USE_EVENT_BROKER
	/* send data to event broker (non-aggregated dumps only) */
	if(aggregated_dump == FALSE)
		broker_program_status(NEBTYPE_PROGRAMSTATUS_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

	return OK;
	}



/* updates host status info */
int update_host_status(host *hst, int aggregated_dump) {

#ifdef USE_EVENT_BROKER
	/* send data to event broker (non-aggregated dumps only) */
	if(aggregated_dump == FALSE)
		broker_host_status(NEBTYPE_HOSTSTATUS_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, hst, NULL);
#endif

	return OK;
	}



/* updates service status info */
int update_service_status(service *svc, int aggregated_dump) {

#ifdef USE_EVENT_BROKER
	/* send data to event broker (non-aggregated dumps only) */
	if(aggregated_dump == FALSE)
		broker_service_status(NEBTYPE_SERVICESTATUS_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, svc, NULL);
#endif

	return OK;
	}



/* updates contact status info */
int update_contact_status(contact *cntct, int aggregated_dump) {

#ifdef USE_EVENT_BROKER
	/* send data to event broker (non-aggregated dumps only) */
	if(aggregated_dump == FALSE)
		broker_contact_status(NEBTYPE_CONTACTSTATUS_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, cntct, NULL);
#endif

	return OK;
	}
#endif





#ifdef NSCGI

/******************************************************************/
/******************* TOP-LEVEL INPUT FUNCTIONS ********************/
/******************************************************************/


/* reads in all status data */
int read_status_data(const char *status_file_name, int options) {
	return xsddefault_read_status_data(status_file_name, options);
	}



/******************************************************************/
/****************** CHAINED HASH FUNCTIONS ************************/
/******************************************************************/

/* adds hoststatus to hash list in memory */
int add_hoststatus_to_hashlist(hoststatus *new_hoststatus) {
	hoststatus *temp_hoststatus = NULL;
	hoststatus *lastpointer = NULL;
	int hashslot = 0;
	int i = 0;

	/* initialize hash list */
	if(hoststatus_hashlist == NULL) {

		hoststatus_hashlist = (hoststatus **)malloc(sizeof(hoststatus *) * HOSTSTATUS_HASHSLOTS);
		if(hoststatus_hashlist == NULL)
			return 0;

		for(i = 0; i < HOSTSTATUS_HASHSLOTS; i++)
			hoststatus_hashlist[i] = NULL;
		}

	if(!new_hoststatus)
		return 0;

	hashslot = hashfunc(new_hoststatus->host_name, NULL, HOSTSTATUS_HASHSLOTS);
	lastpointer = NULL;
	for(temp_hoststatus = hoststatus_hashlist[hashslot]; temp_hoststatus && compare_hashdata(temp_hoststatus->host_name, NULL, new_hoststatus->host_name, NULL) < 0; temp_hoststatus = temp_hoststatus->nexthash)
		lastpointer = temp_hoststatus;

	if(!temp_hoststatus || (compare_hashdata(temp_hoststatus->host_name, NULL, new_hoststatus->host_name, NULL) != 0)) {
		if(lastpointer)
			lastpointer->nexthash = new_hoststatus;
		else
			hoststatus_hashlist[hashslot] = new_hoststatus;
		new_hoststatus->nexthash = temp_hoststatus;

		return 1;
		}

	/* else already exists */
	return 0;
	}


int add_servicestatus_to_hashlist(servicestatus *new_servicestatus) {
	servicestatus *temp_servicestatus = NULL, *lastpointer = NULL;
	int hashslot = 0;
	int i = 0;

	/* initialize hash list */
	if(servicestatus_hashlist == NULL) {

		servicestatus_hashlist = (servicestatus **)malloc(sizeof(servicestatus *) * SERVICESTATUS_HASHSLOTS);
		if(servicestatus_hashlist == NULL)
			return 0;

		for(i = 0; i < SERVICESTATUS_HASHSLOTS; i++)
			servicestatus_hashlist[i] = NULL;
		}

	if(!new_servicestatus)
		return 0;

	hashslot = hashfunc(new_servicestatus->host_name, new_servicestatus->description, SERVICESTATUS_HASHSLOTS);
	lastpointer = NULL;
	for(temp_servicestatus = servicestatus_hashlist[hashslot]; temp_servicestatus && compare_hashdata(temp_servicestatus->host_name, temp_servicestatus->description, new_servicestatus->host_name, new_servicestatus->description) < 0; temp_servicestatus = temp_servicestatus->nexthash)
		lastpointer = temp_servicestatus;

	if(!temp_servicestatus || (compare_hashdata(temp_servicestatus->host_name, temp_servicestatus->description, new_servicestatus->host_name, new_servicestatus->description) != 0)) {
		if(lastpointer)
			lastpointer->nexthash = new_servicestatus;
		else
			servicestatus_hashlist[hashslot] = new_servicestatus;
		new_servicestatus->nexthash = temp_servicestatus;


		return 1;
		}

	/* else already exists */
	return 0;
	}



/******************************************************************/
/********************** ADDITION FUNCTIONS ************************/
/******************************************************************/


/* adds a host status entry to the list in memory */
int add_host_status(hoststatus *new_hoststatus) {
	char date_string[MAX_DATETIME_LENGTH];

	/* make sure we have what we need */
	if(new_hoststatus == NULL)
		return ERROR;
	if(new_hoststatus->host_name == NULL)
		return ERROR;

	/* massage host status a bit */
	if(new_hoststatus != NULL) {
		switch(new_hoststatus->status) {
			case 0:
				new_hoststatus->status = SD_HOST_UP;
				break;
			case 1:
				new_hoststatus->status = SD_HOST_DOWN;
				break;
			case 2:
				new_hoststatus->status = SD_HOST_UNREACHABLE;
				break;
			default:
				new_hoststatus->status = SD_HOST_UP;
				break;
			}
		if(new_hoststatus->has_been_checked == FALSE) {
			if(use_pending_states == TRUE)
				new_hoststatus->status = HOST_PENDING;
			my_free(new_hoststatus->plugin_output);
			if(new_hoststatus->should_be_scheduled == TRUE) {
				get_time_string(&new_hoststatus->next_check, date_string, sizeof(date_string), LONG_DATE_TIME);
				asprintf(&new_hoststatus->plugin_output, "Host check scheduled for %s", date_string);
				}
			else {
				/* passive-only hosts that have just been scheduled for a forced check */
				if(new_hoststatus->checks_enabled == FALSE && new_hoststatus->next_check != (time_t)0L && (new_hoststatus->check_options & CHECK_OPTION_FORCE_EXECUTION)) {
					get_time_string(&new_hoststatus->next_check, date_string, sizeof(date_string), LONG_DATE_TIME);
					asprintf(&new_hoststatus->plugin_output, "Forced host check scheduled for %s", date_string);
					}
				/* passive-only hosts not scheduled to be checked */
				else
					new_hoststatus->plugin_output = (char *)strdup("Host is not scheduled to be checked...");
				}
			}
		}

	new_hoststatus->next = NULL;
	new_hoststatus->nexthash = NULL;

	/* add new hoststatus to hoststatus chained hash list */
	if(!add_hoststatus_to_hashlist(new_hoststatus))
		return ERROR;

	/* object cache file is already sorted, so just add new items to end of list */
	if(hoststatus_list == NULL) {
		hoststatus_list = new_hoststatus;
		hoststatus_list_tail = new_hoststatus;
		}
	else {
		hoststatus_list_tail->next = new_hoststatus;
		hoststatus_list_tail = new_hoststatus;
		}

	return OK;
	}


/* adds a service status entry to the list in memory */
int add_service_status(servicestatus *new_svcstatus) {
	char date_string[MAX_DATETIME_LENGTH];

	/* make sure we have what we need */
	if(new_svcstatus == NULL)
		return ERROR;
	if(new_svcstatus->host_name == NULL || new_svcstatus->description == NULL)
		return ERROR;


	/* massage service status a bit */
	if(new_svcstatus != NULL) {
		switch(new_svcstatus->status) {
			case 0:
				new_svcstatus->status = SERVICE_OK;
				break;
			case 1:
				new_svcstatus->status = SERVICE_WARNING;
				break;
			case 2:
				new_svcstatus->status = SERVICE_CRITICAL;
				break;
			case 3:
				new_svcstatus->status = SERVICE_UNKNOWN;
				break;
			default:
				new_svcstatus->status = SERVICE_OK;
				break;
			}
		if(new_svcstatus->has_been_checked == FALSE) {
			if(use_pending_states == TRUE)
				new_svcstatus->status = SERVICE_PENDING;
			my_free(new_svcstatus->plugin_output);
			if(new_svcstatus->should_be_scheduled == TRUE) {
				get_time_string(&new_svcstatus->next_check, date_string, sizeof(date_string), LONG_DATE_TIME);
				asprintf(&new_svcstatus->plugin_output, "Service check scheduled for %s", date_string);
				}
			else {
				/* passive-only services that have just been scheduled for a forced check */
				if(new_svcstatus->checks_enabled == FALSE && new_svcstatus->next_check != (time_t)0L && (new_svcstatus->check_options & CHECK_OPTION_FORCE_EXECUTION)) {
					get_time_string(&new_svcstatus->next_check, date_string, sizeof(date_string), LONG_DATE_TIME);
					asprintf(&new_svcstatus->plugin_output, "Forced service check scheduled for %s", date_string);
					}
				/* passive-only services not scheduled to be checked */
				else
					new_svcstatus->plugin_output = (char *)strdup("Service is not scheduled to be checked...");
				}
			}
		}

	new_svcstatus->next = NULL;
	new_svcstatus->nexthash = NULL;

	/* add new servicestatus to servicestatus chained hash list */
	if(!add_servicestatus_to_hashlist(new_svcstatus))
		return ERROR;

	/* object cache file is already sorted, so just add new items to end of list */
	if(servicestatus_list == NULL) {
		servicestatus_list = new_svcstatus;
		servicestatus_list_tail = new_svcstatus;
		}
	else {
		servicestatus_list_tail->next = new_svcstatus;
		servicestatus_list_tail = new_svcstatus;
		}

	return OK;
	}





/******************************************************************/
/*********************** CLEANUP FUNCTIONS ************************/
/******************************************************************/


/* free all memory for status data */
void free_status_data(void) {
	hoststatus *this_hoststatus = NULL;
	hoststatus *next_hoststatus = NULL;
	servicestatus *this_svcstatus = NULL;
	servicestatus *next_svcstatus = NULL;

	/* free memory for the host status list */
	for(this_hoststatus = hoststatus_list; this_hoststatus != NULL; this_hoststatus = next_hoststatus) {
		next_hoststatus = this_hoststatus->next;
		my_free(this_hoststatus->host_name);
		my_free(this_hoststatus->plugin_output);
		my_free(this_hoststatus->long_plugin_output);
		my_free(this_hoststatus->perf_data);
		my_free(this_hoststatus);
		}

	/* free memory for the service status list */
	for(this_svcstatus = servicestatus_list; this_svcstatus != NULL; this_svcstatus = next_svcstatus) {
		next_svcstatus = this_svcstatus->next;
		my_free(this_svcstatus->host_name);
		my_free(this_svcstatus->description);
		my_free(this_svcstatus->plugin_output);
		my_free(this_svcstatus->long_plugin_output);
		my_free(this_svcstatus->perf_data);
		my_free(this_svcstatus);
		}

	/* free hash lists reset list pointers */
	my_free(hoststatus_hashlist);
	my_free(servicestatus_hashlist);
	hoststatus_list = NULL;
	servicestatus_list = NULL;

	return;
	}




/******************************************************************/
/************************ SEARCH FUNCTIONS ************************/
/******************************************************************/


/* find a host status entry */
hoststatus *find_hoststatus(char *host_name) {
	hoststatus *temp_hoststatus = NULL;

	if(host_name == NULL || hoststatus_hashlist == NULL)
		return NULL;

	for(temp_hoststatus = hoststatus_hashlist[hashfunc(host_name, NULL, HOSTSTATUS_HASHSLOTS)]; temp_hoststatus && compare_hashdata(temp_hoststatus->host_name, NULL, host_name, NULL) < 0; temp_hoststatus = temp_hoststatus->nexthash);

	if(temp_hoststatus && (compare_hashdata(temp_hoststatus->host_name, NULL, host_name, NULL) == 0))
		return temp_hoststatus;

	return NULL;
	}


/* find a service status entry */
servicestatus *find_servicestatus(char *host_name, char *svc_desc) {
	servicestatus *temp_servicestatus = NULL;

	if(host_name == NULL || svc_desc == NULL || servicestatus_hashlist == NULL)
		return NULL;

	for(temp_servicestatus = servicestatus_hashlist[hashfunc(host_name, svc_desc, SERVICESTATUS_HASHSLOTS)]; temp_servicestatus && compare_hashdata(temp_servicestatus->host_name, temp_servicestatus->description, host_name, svc_desc) < 0; temp_servicestatus = temp_servicestatus->nexthash);

	if(temp_servicestatus && (compare_hashdata(temp_servicestatus->host_name, temp_servicestatus->description, host_name, svc_desc) == 0))
		return temp_servicestatus;

	return NULL;
	}




/******************************************************************/
/*********************** UTILITY FUNCTIONS ************************/
/******************************************************************/


/* gets the total number of services of a certain state for a specific host */
int get_servicestatus_count(char *host_name, int type) {
	servicestatus *temp_status = NULL;
	int count = 0;

	if(host_name == NULL)
		return 0;

	for(temp_status = servicestatus_list; temp_status != NULL; temp_status = temp_status->next) {
		if(temp_status->status & type) {
			if(!strcmp(host_name, temp_status->host_name))
				count++;
			}
		}

	return count;
	}

#endif
