/*****************************************************************************
 *
 * SRETENTION.C - State retention routines for Nagios
 *
 * Copyright (c) 1999-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   08-24-2003
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

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"
#include "../include/nagios.h"
#include "../include/sretention.h"

extern int            retain_state_information;



/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/
#ifdef USE_XRDDEFAULT
#include "../xdata/xrddefault.h"		/* default routines */
#endif






/******************************************************************/
/************* TOP-LEVEL STATE INFORMATION FUNCTIONS **************/
/******************************************************************/


/* save all host and service state information */
int save_state_information(char *main_config_file, int autosave){
	char buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("save_state_information() start\n");
#endif

	if(retain_state_information==FALSE)
		return OK;

	/********* IMPLEMENTATION-SPECIFIC OUTPUT FUNCTION ********/
#ifdef USE_XRDDEFAULT
	if(xrddefault_save_state_information(main_config_file)==ERROR)
		return ERROR;
#endif

	if(autosave==TRUE){
		snprintf(buffer,sizeof(buffer),"Auto-save of retention data completed successfully.\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_all_logs(buffer,NSLOG_PROCESS_INFO);
	        }

#ifdef DEBUG0
	printf("save_state_information() end\n");
#endif

	return OK;
        }




/* reads in initial host and state information */
int read_initial_state_information(char *main_config_file){

#ifdef DEBUG0
	printf("read_initial_state_information() start\n");
#endif

	if(retain_state_information==FALSE)
		return OK;

	/********* IMPLEMENTATION-SPECIFIC INPUT FUNCTION ********/
#ifdef USE_XRDDEFAULT
	if(xrddefault_read_state_information(main_config_file)==ERROR)
		return ERROR;
#endif

#ifdef DEBUG0
	printf("read_initial_state_information() end\n");
#endif

	return OK;
        }



