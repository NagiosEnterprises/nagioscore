/*****************************************************************************
 *
 * PERFDATA.C - Performance data routines for Nagios
 *
 * Copyright (c) 2000-2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   06-12-2001
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


/*********** COMMON HEADER FILES ***********/

#include "../common/config.h"
#include "../common/common.h"
#include "../common/objects.h"
#include "perfdata.h"


/***** IMPLEMENTATION-SPECIFIC HEADER FILES *****/

#ifdef USE_XPDDEFAULT
#include "../xdata/xpddefault.h"
#endif
#ifdef USE_XPDFILE
#include "../xdata/xpdfile.h"
#endif


extern int   process_performance_data;



/******************************************************************/
/************** INITIALIZATION & CLEANUP FUNCTIONS ****************/
/******************************************************************/

/* initializes performance data */
int initialize_performance_data(char *config_file){

#ifdef USE_XPDDEFAULT
	xpddefault_initialize_performance_data(config_file);
#endif

#ifdef USE_XPDFILE
	xpdfile_initialize_performance_data(config_file);
#endif

	return OK;
        }



/* cleans up performance data */
int cleanup_performance_data(char *config_file){

#ifdef USE_XPDDEFAULT
	xpddefault_cleanup_performance_data(config_file);
#endif

#ifdef USE_XPDFILE
	xpdfile_cleanup_performance_data(config_file);
#endif

	return OK;
        }



/******************************************************************/
/****************** PERFORMANCE DATA FUNCTIONS ********************/
/******************************************************************/


/* updates service performance data */
int update_service_performance_data(service *svc){

	/* should we be processing performance data for anything? */
	if(process_performance_data==FALSE)
		return OK;

	/* should we process performance data for this service? */
	if(svc->process_performance_data==FALSE)
		return OK;

	/* process the performance data! */
#ifdef USE_XPDDEFAULT
	xpddefault_update_service_performance_data(svc);
#endif
#ifdef USE_XPDFILE
	xpdfile_update_service_performance_data(svc);
#endif

	return OK;
        }



/* updates host performance data */
int update_host_performance_data(host *hst){

	/* should we be processing performance data for anything? */
	if(process_performance_data==FALSE)
		return OK;

	/* should we process performance data for this host? */
	if(hst->process_performance_data==FALSE)
		return OK;

	/* process the performance data! */
#ifdef USE_XPDDEFAULT
	xpddefault_update_host_performance_data(hst);
#endif
#ifdef USE_XPDFILE
	xpdfile_update_host_performance_data(hst);
#endif

	return OK;
        }
