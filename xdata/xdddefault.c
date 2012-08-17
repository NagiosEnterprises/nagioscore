/*****************************************************************************
 *
 * XDDDEFAULT.C - Default scheduled downtime data routines for Nagios
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
#include "../include/locations.h"
#include "../include/downtime.h"
#include "../include/macros.h"

#ifdef NSCORE
#include "../include/objects.h"
#include "../include/nagios.h"
#endif

#ifdef NSCGI
#include "../include/cgiutils.h"
#endif


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/
#include "xdddefault.h"



#ifdef NSCORE
extern unsigned long next_downtime_id;
extern scheduled_downtime *scheduled_downtime_list;
#endif




#ifdef NSCORE


/******************************************************************/
/*********** DOWNTIME INITIALIZATION/CLEANUP FUNCTIONS ************/
/******************************************************************/


/* initialize downtime data */
int xdddefault_initialize_downtime_data(char *main_config_file) {
	scheduled_downtime *temp_downtime = NULL;

	/* clean up the old downtime data */
	xdddefault_validate_downtime_data();

	/* find the new starting index for downtime id if its missing*/
	if(next_downtime_id == 0L) {
		for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; temp_downtime = temp_downtime->next) {
			if(temp_downtime->downtime_id >= next_downtime_id)
				next_downtime_id = temp_downtime->downtime_id + 1;
			}
		}

	/* initialize next downtime id if necessary */
	if(next_downtime_id == 0L)
		next_downtime_id = 1;

	return OK;
	}



/* removes invalid and old downtime entries from the downtime file */
int xdddefault_validate_downtime_data(void) {
	scheduled_downtime *temp_downtime;
	scheduled_downtime *next_downtime;
	int save = TRUE;

	/* remove stale downtimes */
	for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; temp_downtime = next_downtime) {

		next_downtime = temp_downtime->next;
		save = TRUE;

		/* delete downtimes with invalid host names */
		if(find_host(temp_downtime->host_name) == NULL)
			save = FALSE;

		/* delete downtimes with invalid service descriptions */
		if(temp_downtime->type == SERVICE_DOWNTIME && find_service(temp_downtime->host_name, temp_downtime->service_description) == NULL)
			save = FALSE;

		/* delete downtimes that have expired */
		if(temp_downtime->end_time < time(NULL))
			save = FALSE;

		/* delete the downtime */
		if(save == FALSE) {
			delete_downtime(temp_downtime->type, temp_downtime->downtime_id);
			}
		}

	/* remove triggered downtimes without valid parents */
	for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; temp_downtime = next_downtime) {

		next_downtime = temp_downtime->next;
		save = TRUE;

		if(temp_downtime->triggered_by == 0)
			continue;

		if(find_host_downtime(temp_downtime->triggered_by) == NULL && find_service_downtime(temp_downtime->triggered_by) == NULL)
			save = FALSE;

		/* delete the downtime */
		if(save == FALSE) {
			delete_downtime(temp_downtime->type, temp_downtime->downtime_id);
			}
		}

	return OK;
	}


/******************************************************************/
/************************ SAVE FUNCTIONS **************************/
/******************************************************************/

/* adds a new scheduled host downtime entry */
int xdddefault_add_new_host_downtime(char *host_name, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id, int is_in_effect){

	/* find the next valid downtime id */
	while(find_host_downtime(next_downtime_id) != NULL)
		next_downtime_id++;

	/* add downtime to list in memory */
	add_host_downtime(host_name, entry_time, author, comment, start_time, end_time, fixed, triggered_by, duration, next_downtime_id, is_in_effect);

	/* return the id for the downtime we are about to add (this happens in the main code) */
	if(downtime_id != NULL)
		*downtime_id = next_downtime_id;

	/* increment the downtime id */
	next_downtime_id++;

	return OK;
	}



/* adds a new scheduled service downtime entry */
int xdddefault_add_new_service_downtime(char *host_name, char *service_description, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id, int is_in_effect){

	/* find the next valid downtime id */
	while(find_service_downtime(next_downtime_id) != NULL)
		next_downtime_id++;

	/* add downtime to list in memory */
	add_service_downtime(host_name, service_description, entry_time, author, comment, start_time, end_time, fixed, triggered_by, duration, next_downtime_id, is_in_effect);

	/* return the id for the downtime we are about to add (this happens in the main code) */
	if(downtime_id != NULL)
		*downtime_id = next_downtime_id;

	/* increment the downtime id */
	next_downtime_id++;

	return OK;
	}
#endif


