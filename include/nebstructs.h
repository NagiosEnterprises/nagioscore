/*****************************************************************************
 *
 * NEBSTRUCTS.H - Event broker includes for Nagios
 *
 * Copyright (c) 2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 08-19-2003
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

#ifndef _NEBSTRUCTS_H
#define _NEBSTRUCTS_H

#include "config.h"
#include "objects.h"
#include "nagios.h"



/****** STRUCTURES *************************/

/* process data structure */
typedef struct nebstruct_process_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;
        }nebstruct_process_data;


/* timed event data structure */
typedef struct nebstruct_timed_event_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	int             event_type;
	int             recurring;
	time_t          run_time;
	void            *event_data;
        }nebstruct_timed_event_data;

/* log data structure */
typedef struct nebstruct_log_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	int             data_type;
	char            *log_entry;
        }nebstruct_log_data;


/* system command structure */
typedef struct nebstruct_system_command_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	int             timeout;
	char            *command_line;
	int             early_timeout;
	double          execution_time;
	int             return_code;
	char            *output;
        }nebstruct_system_command_data;


/* event handler structure */
typedef struct nebstruct_event_handler_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	char            *host_name;
	char            *service_description;
	int             state_type;
	int             state;
	int             timeout;
	char            *command_line;
	int             early_timeout;
	double          execution_time;
	int             return_code;
	char            *output;
        }nebstruct_event_handler_data;


/* host check structure */
typedef struct nebstruct_host_check_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	char            *host_name;
	int             current_attempt;
	int             max_attempts;
	int             state_type;
	int             state;
	int             timeout;
	char            *command_line;
	int             early_timeout;
	double          execution_time;
	double          latency;
	int             return_code;
	char            *output;
	char            *perf_data;
        }nebstruct_host_check_data;


/* service check structure */
typedef struct nebstruct_service_check_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	char            *host_name;
	char            *service_description;
	int             current_attempt;
	int             max_attempts;
	int             state_type;
	int             state;
	int             timeout;
	char            *command_line;
	int             early_timeout;
	double          execution_time;
	double          latency;
	int             return_code;
	char            *output;
	char            *perf_data;
        }nebstruct_service_check_data;


/* comment data structure */
typedef struct nebstruct_comment_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;
	
	char            *host_name;
	char            *service_description;
	time_t          entry_time;
	char            *author_name;
	char            *comment_data;
	int             persistent;
	int             source;
	unsigned long   comment_id;
        }nebstruct_comment_data;


/* downtime data structure */
typedef struct nebstruct_downtime_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;
	
	char            *host_name;
	char            *service_description;
	time_t          entry_time;
	char            *author_name;
	char            *comment_data;
	time_t          start_time;
	time_t          end_time;
	int             fixed;
	unsigned long   duration;
	unsigned long   triggered_by;
	unsigned long   downtime_id;
        }nebstruct_downtime_data;


/* flapping data structure */
typedef struct nebstruct_flapping_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	char            *host_name;
	char            *service_description;
	double          percent_change;
	double          threshold;
	unsigned long   comment_id;
        }nebstruct_flapping_data;


#endif
