/*****************************************************************************
 *
 * PREDICT.C - Failure prediction routines for Nagios
 *
 * Copyright (c) 2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   07-26-2001
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

#include "nagios.h"
#include "predict.h"


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/
#ifdef USE_XFDDEFAULT
#include "../xdata/xfddefault.h"		/* default routines */
#endif



/******************************************************************/
/************** INITIALIZATION & CLEANUP FUNCTIONS ****************/
/******************************************************************/

/* initialize failure prediction data */
int initialize_prediction_data(char *main_config_file){
	int result=OK;

#ifdef DEBUG0
	printf("initialize_prediction_data() start\n");
#endif

	/*** IMPLEMENTATION-SPECIFIC FUNCTIONS ***/
#ifdef USE_XFDDEFAULT
	result=xfddefault_initialize_prediction_data(main_config_file);
#endif

#ifdef DEBUG0
	printf("initialize_prediction_data() end\n");
#endif

	return result;
        }


/* cleanup failure prediction data */
int cleanup_prediction_data(char *main_config_file){
	int result=OK;

#ifdef DEBUG0
	printf("cleanup_prediction_data() start\n");
#endif

	/*** IMPLEMENTATION-SPECIFIC FUNCTIONS ***/
#ifdef USE_XFDDEFAULT
	result=xfddefault_cleanup_prediction_data(main_config_file);
#endif

#ifdef DEBUG0
	printf("cleanup_prediction_data() end\n");
#endif

	return result;
        }



/******************************************************************/
/****************** FAILURE PREDICTION FUNCTIONS ******************/
/******************************************************************/


/* submit service check results to failure prediction logic */
void submit_service_prediction_data(service *svc){
	int result=OK;

#ifdef DEBUG0
	printf("submit_service_prediction_data() start\n");
#endif

	/*** IMPLEMENTATION-SPECIFIC FUNCTIONS ***/
#ifdef USE_XFDDEFAULT
	result=xfddefault_submit_service_prediction_data(svc);
#endif

#ifdef DEBUG0
	printf("submit_service_prediction_data() end\n");
#endif

	return;
        }



/* submit host check results to failure prediction logic */
void submit_host_prediction_data(host *hst){
	int result=OK;

#ifdef DEBUG0
	printf("submit_host_prediction_data() start\n");
#endif

	/*** IMPLEMENTATION-SPECIFIC FUNCTIONS ***/
#ifdef USE_XFDDEFAULT
	result=xfddefault_submit_host_prediction_data(hst);
#endif

#ifdef DEBUG0
	printf("submit_host_prediction_data() end\n");
#endif

	return;
        }

