/*****************************************************************************
 *
 * PERFDATA.C - Performance data routines for Nagios
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
#include "../include/perfdata.h"
#include "../include/macros.h"
#include "../xdata/xpddefault.h"


/******************************************************************/
/************** INITIALIZATION & CLEANUP FUNCTIONS ****************/
/******************************************************************/

/* initializes performance data */
int initialize_performance_data(const char *cfgfile) {
	return xpddefault_initialize_performance_data(cfgfile);
	}



/* cleans up performance data */
int cleanup_performance_data(void) {
	return xpddefault_cleanup_performance_data();
	}



/******************************************************************/
/****************** PERFORMANCE DATA FUNCTIONS ********************/
/******************************************************************/


/* updates service performance data */
int update_service_performance_data(service *svc) {

	/* should we be processing performance data for anything? */
	if(process_performance_data == FALSE)
		return OK;

	/* should we process performance data for this service? */
	if(svc->process_performance_data == FALSE)
		return OK;

	/* process the performance data! */
	xpddefault_update_service_performance_data(svc);

	return OK;
	}



/* updates host performance data */
int update_host_performance_data(host *hst) {

	/* should we be processing performance data for anything? */
	if(process_performance_data == FALSE)
		return OK;

	/* should we process performance data for this host? */
	if(hst->process_performance_data == FALSE)
		return OK;

	/* process the performance data! */
	xpddefault_update_host_performance_data(hst);

	return OK;
	}
