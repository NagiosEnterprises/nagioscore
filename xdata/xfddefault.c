/*****************************************************************************
 *
 * XFDDEFAULT.C - Default failure prediction routines for Nagios
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

#include "../common/config.h"
#include "../common/common.h"
#include "../common/objects.h"
#include "../base/nagios.h"


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/
#include "xfddefault.h"





/******************************************************************/
/***************** COMMON CONFIG INITIALIZATION  ******************/
/******************************************************************/

/* grab configuration information from appropriate config file(s) */
int xfddefault_grab_config_info(char *config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	FILE *fp;

	/*** CORE PASSES IN MAIN CONFIG FILE! ***/

	/* open the config file for reading */
	fp=fopen(config_file,"r");
	if(fp==NULL)
		return ERROR;

	/* read in all lines from the config file */
	for(fgets(input_buffer,sizeof(input_buffer)-1,fp);!feof(fp);fgets(input_buffer,sizeof(input_buffer)-1,fp)){

		/* skip blank lines and comments */
		if(input_buffer[0]=='#' || input_buffer[0]=='\x0' || input_buffer[0]=='\n' || input_buffer[0]=='\r')
			continue;

		strip(input_buffer);

		/* read variables directly from the main config file */
		xfddefault_grab_config_directives(input_buffer);
	        }

	fclose(fp);

	return OK;
        }



/* process configuration directives */
void xfddefault_grab_config_directives(char *input_buffer){

	return;	
        }






/******************************************************************/
/********** PREDICTION INITIALIZATION/CLEANUP FUNCTIONS ***********/
/******************************************************************/


/* initialize failure prediction data */
int xfddefault_initialize_prediction_data(char *main_config_file){

	/* grab configuration information */
	if(xfddefault_grab_config_info(main_config_file)==ERROR)
		return ERROR;

	return OK;
        }


/* cleans up failure prediction data */
int xfddefault_cleanup_prediction_data(char *main_config_file){

	return OK;
        }





/******************************************************************/
/**************** PREDICTION SUBMISSION FUNCTIONS *****************/
/******************************************************************/

/* submit service check data to failure prediction logic */
int xfddefault_submit_service_prediction_data(service *svc){

	return OK;
        }



/* submit host check data to failure prediction logic */
int xfddefault_submit_host_prediction_data(host *hst){

	return OK;
        }

