/*****************************************************************************
 *
 * NEBSTRUCTS.H - Event broker includes for Nagios
 *
 * Copyright (c) 2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 08-28-2003
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
	char            *data;
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
	int             check_type;
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
	int             check_type;
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

	int             comment_type;
	char            *host_name;
	char            *service_description;
	time_t          entry_time;
	char            *author_name;
	char            *comment_data;
	int             persistent;
	int             source;
	int             entry_type;
	int             expires;
	time_t          expire_time;
	unsigned long   comment_id;
        }nebstruct_comment_data;


/* downtime data structure */
typedef struct nebstruct_downtime_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	int             downtime_type;
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

	int             flapping_type;
	char            *host_name;
	char            *service_description;
	double          percent_change;
	double          threshold;
	unsigned long   comment_id;
        }nebstruct_flapping_data;


/* program status structure */
typedef struct nebstruct_program_status_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	time_t          program_start;
	int             pid;
	int             daemon_mode;
	time_t          last_command_check;
	time_t          last_log_rotation;
	int             notifications_enabled;
	int             active_service_checks_enabled;
	int             passive_service_checks_enabled;
	int             active_host_checks_enabled;
	int             passive_host_checks_enabled;
	int             event_handlers_enabled;
	int             flap_detection_enabled;
	int             failure_prediction_enabled;
	int             process_performance_data;
	int             obsess_over_hosts;
	int             obsess_over_services;
        }nebstruct_program_status_data;


/* host status structure */
typedef struct nebstruct_host_status_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	void            *object_ptr;
        }nebstruct_host_status_data;


/* service status structure */
typedef struct nebstruct_service_status_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	void            *object_ptr;
        }nebstruct_service_status_data;


/* notification data structure */
typedef struct nebstruct_notification_struct{
	int             type;
	int             flags;
	int             attr;
	struct timeval  timestamp;

	int             notification_type;
	char            *host_name;
	char            *service_description;
	int             reason_type;
	int             state;
	char            *output;
	char            *ack_author;
	char            *ack_data;
	int             contacts_notified;
        }nebstruct_notification_data;


#endif
