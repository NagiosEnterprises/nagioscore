/*****************************************************************************
 *
 * XCDDEFAULT.C - Default external comment data routines for Nagios
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
#include "../include/comments.h"
#include "../include/macros.h"
#include "../include/objects.h"
#include "../include/nagios.h"
#include "xcddefault.h"


/******************************************************************/
/************ COMMENT INITIALIZATION/CLEANUP FUNCTIONS ************/
/******************************************************************/


/* initialize comment data */
int xcddefault_initialize_comment_data(void) {
	nagios_comment *temp_comment = NULL;

	/* find the new starting index for comment id if its missing*/
	if(next_comment_id == 0L) {
		for(temp_comment = comment_list; temp_comment != NULL; temp_comment = temp_comment->next) {
			if(temp_comment->comment_id >= next_comment_id)
				next_comment_id = temp_comment->comment_id + 1;
			}
		}

	/* initialize next comment id if necessary */
	if(next_comment_id == 0L)
		next_comment_id = 1;

	return OK;
	}


/******************************************************************/
/***************** DEFAULT DATA OUTPUT FUNCTIONS ******************/
/******************************************************************/


/* adds a new host comment */
int xcddefault_add_new_host_comment(int entry_type, char *host_name, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, int expires, time_t expire_time, unsigned long *comment_id) {

	/* find the next valid comment id */
	while(find_host_comment(next_comment_id) != NULL)
		next_comment_id++;

	/* add comment to list in memory */
	add_host_comment(entry_type, host_name, entry_time, author_name, comment_data, next_comment_id, persistent, expires, expire_time, source);

	/* return the id for the comment we are about to add (this happens in the main code) */
	if(comment_id != NULL)
		*comment_id = next_comment_id;

	/* increment the comment id */
	next_comment_id++;

	return OK;
	}


/* adds a new service comment */
int xcddefault_add_new_service_comment(int entry_type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, int expires, time_t expire_time, unsigned long *comment_id) {

	/* find the next valid comment id */
	while(find_service_comment(next_comment_id) != NULL)
		next_comment_id++;

	/* add comment to list in memory */
	add_service_comment(entry_type, host_name, svc_description, entry_time, author_name, comment_data, next_comment_id, persistent, expires, expire_time, source);

	/* return the id for the comment we are about to add (this happens in the main code) */
	if(comment_id != NULL)
		*comment_id = next_comment_id;

	/* increment the comment id */
	next_comment_id++;

	return OK;
	}
