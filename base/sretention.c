/*****************************************************************************
 *
 * SRETENTION.C - State retention routines for Nagios
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
#include "../include/nagios.h"
#include "../include/sretention.h"
#include "../include/broker.h"
#include "../xdata/xrddefault.h"		/* default routines */


/******************************************************************/
/************* TOP-LEVEL STATE INFORMATION FUNCTIONS **************/
/******************************************************************/


/* initializes retention data at program start */
int initialize_retention_data(const char *cfgfile) {
	return xrddefault_initialize_retention_data(cfgfile);
	}



/* cleans up retention data before program termination */
int cleanup_retention_data(void) {
	return xrddefault_cleanup_retention_data();
	}



/* save all host and service state information */
int save_state_information(int autosave) {
	int result = OK;

	if(retain_state_information == FALSE)
		return OK;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_retention_data(NEBTYPE_RETENTIONDATA_STARTSAVE, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

	result = xrddefault_save_state_information();

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_retention_data(NEBTYPE_RETENTIONDATA_ENDSAVE, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

	if(result == ERROR)
		return ERROR;

	if(autosave == TRUE)
		logit(NSLOG_PROCESS_INFO, FALSE, "Auto-save of retention data completed successfully.\n");

	return OK;
	}



/* reads in initial host and state information */
int read_initial_state_information(void) {
	int result = OK;

	if(retain_state_information == FALSE)
		return OK;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_retention_data(NEBTYPE_RETENTIONDATA_STARTLOAD, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

	result = xrddefault_read_state_information();

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_retention_data(NEBTYPE_RETENTIONDATA_ENDLOAD, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

	if(result == ERROR)
		return ERROR;

	return OK;
	}
