/*****************************************************************************
 *
 * STATUSDATA.H - Header for external status data routines
 *
 * Copyright (c) 2000-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   04-20-2002
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

#ifndef _STATUSDATA_H
#define _STATUSDATA_H

#ifdef NSCORE
#include "objects.h"
#endif


#ifdef NSCGI

#define READ_PROGRAM_STATUS	1
#define READ_HOST_STATUS	2
#define READ_SERVICE_STATUS	4

#define READ_ALL_STATUS_DATA    READ_PROGRAM_STATUS | READ_HOST_STATUS | READ_SERVICE_STATUS


/**************************** DATA STRUCTURES ******************************/


/* HOST STATUS structure */
typedef struct hoststatus_struct{
	char    *host_name;
	char    *information;
	int     status;
	time_t  last_update;
	time_t  last_check;
	time_t	last_state_change;
	unsigned long	time_up;
	unsigned long	time_down;
	unsigned long	time_unreachable;
	time_t  last_notification;
	int     notifications_enabled;
	int     problem_has_been_acknowledged;
	int     current_notification_number;
	int     event_handler_enabled;
	int     checks_enabled;
	int     flap_detection_enabled;
	int     is_flapping;
	double  percent_state_change;
	int     scheduled_downtime_depth;
	int     failure_prediction_enabled;
	int     process_performance_data;
	struct  hoststatus_struct *next;
        }hoststatus;


/* SERVICE STATUS structure */
typedef struct servicestatus_struct{
	char    *host_name;
	char    *description;
	char    *information;
	int     max_attempts;
	int     current_attempt;
	int     status;
	time_t  last_update;
	time_t  last_check;
	time_t  next_check;
	int     check_type;
	int	checks_enabled;
	time_t	last_state_change;
	int	last_hard_state;
	int     state_type;
	unsigned long	time_ok;
	unsigned long	time_unknown;
	unsigned long	time_warning;
	unsigned long	time_critical;
	time_t  last_notification;
	int     notifications_enabled;
	int     problem_has_been_acknowledged;
	int     current_notification_number;
	int     accept_passive_service_checks;
	int     event_handler_enabled;
	int     flap_detection_enabled;
	int     is_flapping;
	double  percent_state_change;
	int     latency;
	int     execution_time;
	int     scheduled_downtime_depth;
	int     failure_prediction_enabled;
	int     process_performance_data;
	int     obsess_over_service;
	struct  servicestatus_struct *next;
        }servicestatus;


/*************************** SERVICE STATES ***************************/

#define SERVICE_PENDING			1
#define SERVICE_OK			2
#define SERVICE_RECOVERY		4
#define SERVICE_WARNING			8
#define SERVICE_UNKNOWN			16
#define SERVICE_CRITICAL		32
#define SERVICE_HOST_DOWN		64
#define SERVICE_UNREACHABLE		128


/**************************** HOST STATES ****************************/

#define HOST_PENDING			1
#define HOST_UP				2
#define HOST_DOWN			4
#define HOST_UNREACHABLE		8



/**************************** FUNCTIONS ******************************/

int read_status_data(char *,int);                       /* reads all status data */
int add_program_status(time_t,int,int,time_t,time_t,int,int,int,int,int,int,int,int);
int add_host_status(char *,char *,time_t,time_t,time_t,int,unsigned long,unsigned long,unsigned long,time_t,int,int,int,int,int,int,double,int,int,int,char *);
int add_service_status(char *,char *,char *,time_t,int,int,int,time_t,time_t,int,int,int,int,time_t,int,char *,unsigned long,unsigned long,unsigned long,unsigned long,time_t,int,int,int,int,int,int,double,int,int,int,int,char *);
void free_status_data(void);                            /* free all memory allocated to status data */
servicestatus *find_servicestatus(char *,char *);       /* finds status information for a specific service */
hoststatus *find_hoststatus(char *);                    /* finds status information for a specific host */
int get_servicestatus_count(char *,int);		/* gets total number of services of a certain type for a specific host */
#endif


#ifdef NSCORE
int initialize_status_data(char *);                     /* initializes status data at program start */
int update_all_status_data(void);                       /* updates all status data */
int cleanup_status_data(char *,int);                    /* cleans up status data at program termination */
int update_program_status(int);                         /* updates program status data */
int update_host_status(host *,int);                     /* updates host status data */
int update_service_status(service *,int);               /* updates service status data */
#endif


#endif

