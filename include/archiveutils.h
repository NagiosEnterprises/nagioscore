/**************************************************************************
 *
 * ARCHIVEUTILS.H -  Utility information for Nagios CGI that read archives
 *
 * Copyright (c) 2013 Nagios Enterprises, LLC
 * Last Modified: 06-30-2013
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
 *************************************************************************/

#ifndef ARCHIVEUTILS_H_INCLUDED
#define ARCHIVEUTILS_H_INCLUDED

#include "../include/jsonutils.h"

/* Archive utilities object types */
#define AU_OBJTYPE_NONE			0
#define AU_OBJTYPE_HOST			(1<<0)
#define AU_OBJTYPE_SERVICE		(1<<1)
#define AU_OBJTYPE_HOSTGROUP	(1<<2)
#define AU_OBJTYPE_SERVICEGROUP	(1<<3)
#define AU_OBJTYPE_ALL			(AU_OBJTYPE_HOST | AU_OBJTYPE_SERVICE)

/* Archive utilities state types */
#define AU_STATETYPE_HARD		(1<<0)
#define AU_STATETYPE_SOFT		(1<<1)
#define AU_STATETYPE_NO_DATA	(1<<2)
#define AU_STATETYPE_ALL		(AU_STATETYPE_HARD | AU_STATETYPE_SOFT)

/* Archive utilities states */
#define AU_STATE_NO_DATA			0
#define AU_STATE_HOST_UP			(1<<0)
#define AU_STATE_HOST_DOWN			(1<<1)
#define AU_STATE_HOST_UNREACHABLE	(1<<2)
#define AU_STATE_SERVICE_OK			(1<<3)
#define AU_STATE_SERVICE_WARNING	(1<<4)
#define AU_STATE_SERVICE_CRITICAL	(1<<5)
#define AU_STATE_SERVICE_UNKNOWN	(1<<6)
#define AU_STATE_PROGRAM_START		(1<<7)	/* Nagios program start */
#define AU_STATE_PROGRAM_END		(1<<8)	/* Nagios program end */
#define AU_STATE_DOWNTIME_START		(1<<9)	/* Downtime start */
#define AU_STATE_DOWNTIME_END		(1<<10)	/* Downtime end */
#define AU_STATE_CURRENT_STATE		(1<<11)	/* Host or service current state */

#define AU_STATE_HOST_ALL			(AU_STATE_HOST_UP | \
									AU_STATE_HOST_DOWN | \
									AU_STATE_HOST_UNREACHABLE)
#define AU_STATE_SERVICE_ALL		(AU_STATE_SERVICE_OK | \
									AU_STATE_SERVICE_WARNING | \
									AU_STATE_SERVICE_CRITICAL | \
									AU_STATE_SERVICE_UNKNOWN)
#define AU_STATE_ALL				(AU_STATE_HOST_ALL | AU_STATE_SERVICE_ALL)

/* Archive utilities log types */
#define AU_LOGTYPE_ALERT			(1<<0)
#define AU_LOGTYPE_STATE_INITIAL	(1<<1)
#define AU_LOGTYPE_STATE_CURRENT	(1<<2)
#define AU_LOGTYPE_NOTIFICATION		(1<<3)
#define AU_LOGTYPE_DOWNTIME			(1<<4)
#define AU_LOGTYPE_NAGIOS			(1<<5)
#define AU_LOGTYPE_STATE			(AU_LOGTYPE_STATE_INITIAL | \
									AU_LOGTYPE_STATE_CURRENT)
#define AU_LOGTYPE_ALL				(AU_LOGTYPE_ALERT | \
									AU_LOGTYPE_STATE | \
									AU_LOGTYPE_NOTIFICATION | \
									AU_LOGTYPE_DOWNTIME | \
									AU_LOGTYPE_NAGIOS)

/* Archive utilities notification types */
#define AU_NOTIFICATION_NO_DATA						0
#define AU_NOTIFICATION_HOST_DOWN					(1<<0)
#define AU_NOTIFICATION_HOST_UNREACHABLE			(1<<1)
#define AU_NOTIFICATION_HOST_RECOVERY				(1<<2)
#define AU_NOTIFICATION_HOST_CUSTOM					(1<<3)
#define AU_NOTIFICATION_HOST_ACK					(1<<4)
#define AU_NOTIFICATION_HOST_FLAPPING_START			(1<<5)
#define AU_NOTIFICATION_HOST_FLAPPING_STOP			(1<<6)
#define AU_NOTIFICATION_SERVICE_CRITICAL			(1<<7)
#define AU_NOTIFICATION_SERVICE_WARNING				(1<<8)
#define AU_NOTIFICATION_SERVICE_RECOVERY			(1<<9)
#define AU_NOTIFICATION_SERVICE_CUSTOM				(1<<10)
#define AU_NOTIFICATION_SERVICE_ACK					(1<<11)
#define AU_NOTIFICATION_SERVICE_FLAPPING_START		(1<<12)
#define AU_NOTIFICATION_SERVICE_FLAPPING_STOP		(1<<13)
#define AU_NOTIFICATION_SERVICE_UNKNOWN				(1<<14)

#define AU_NOTIFICATION_HOST_ALL	(AU_NOTIFICATION_HOST_DOWN | \
									AU_NOTIFICATION_HOST_UNREACHABLE | \
									AU_NOTIFICATION_HOST_RECOVERY | \
									AU_NOTIFICATION_HOST_CUSTOM | \
									AU_NOTIFICATION_HOST_ACK | \
									AU_NOTIFICATION_HOST_FLAPPING_START | \
									AU_NOTIFICATION_HOST_FLAPPING_STOP)

#define AU_NOTIFICATION_SERVICE_ALL	(AU_NOTIFICATION_SERVICE_CRITICAL | \
									AU_NOTIFICATION_SERVICE_WARNING | \
									AU_NOTIFICATION_SERVICE_RECOVERY | \
									AU_NOTIFICATION_SERVICE_CUSTOM | \
									AU_NOTIFICATION_SERVICE_ACK | \
									AU_NOTIFICATION_SERVICE_FLAPPING_START | \
									AU_NOTIFICATION_SERVICE_FLAPPING_STOP | \
									AU_NOTIFICATION_SERVICE_UNKNOWN)

#define AU_NOTFICATION_ALL	(AU_NOTFICATION_HOST_ALL | \
							AU_NOTIFICATION_SERVICE_ALL)

typedef struct au_array_struct {
	char 	*label;
	int		size;
	int		count;
	void	**members;
	int		new;
	} au_array;

typedef struct au_node_struct {
	void	*data;
	struct au_node_struct *next;
	} au_node;

typedef struct au_linked_list_struct {
	char	*label;
	au_node	*head;
	au_node	*last_new;
	} au_linked_list;

struct au_log_entry_struct;

/* au_availability keeps the availability information for a given host or
	service */
typedef struct au_availability_struct {
	unsigned long time_up;
	unsigned long time_down;
	unsigned long time_unreachable;
	unsigned long time_ok;
	unsigned long time_warning;
	unsigned long time_unknown;
	unsigned long time_critical;

	unsigned long scheduled_time_up;
	unsigned long scheduled_time_down;
	unsigned long scheduled_time_unreachable;
	unsigned long scheduled_time_ok;
	unsigned long scheduled_time_warning;
	unsigned long scheduled_time_unknown;
	unsigned long scheduled_time_critical;
	unsigned long scheduled_time_indeterminate;

	unsigned long time_indeterminate_nodata;
	unsigned long time_indeterminate_notrunning;
	} au_availability;

/* au_host keeps information about a single host and all log entries that
	pertain to that host, including global events such as Nagios starts and
	stops */
typedef struct au_host_struct {
	char		*name;
	host		*hostp;
	au_linked_list	*log_entries;
	au_availability *availability;
	} au_host;

/* au_service keeps information about a single service and all log entries
	that pertain to that service, including global events such as Nagios 
	starts and stops */
typedef struct au_service_struct {
	char		*host_name;
	char		*description;
	service 	*servicep;
	au_linked_list	*log_entries;
	au_availability *availability;
	} au_service;

typedef struct au_contact_struct {
	char		*name;
	contact		*contactp;
	} au_contact;

/* au_log_alert keeps information about alert and state type logs */
typedef struct au_log_alert_struct {
	int			obj_type;		/* AU_OBJTYPE_HOST or AU_OBJTYPE_SERVICE */	
	void		*object;		/* au_host or au_service */
	int			state_type;		/* hard, soft, or no data */
	int			state;			/* any host or service state, or no data */
	char		*plugin_output;
	} au_log_alert;

/* au_log_notification keeps information about notification logs */
typedef struct au_log_notification_struct {
	int			obj_type;		/* AU_OBJTYPE_HOST or AU_OBJTYPE_SERVICE */	
	void		*object;		/* au_host or au_service */
	au_contact	*contact;		/* notification contact */
	int			notification_type;
	char		*method;
	char		*message;		/* informational method */
	} au_log_notification;

/* au_log_downtime keeps information about downtime logs */
typedef struct au_log_downtime_struct {
	int			obj_type;		/* AU_OBJTYPE_HOST or AU_OBJTYPE_SERVICE */	
	void		*object;		/* au_host or au_service */
	int			downtime_type;	/* AU_STATE_DOWNTIME_START or 
									AU_STATE_DOWNTIME_END */
	} au_log_downtime;

/* au_log_nagios keeps information about Nagios starts and stops */
typedef struct au_log_nagios_struct {
	int			type;			/* AU_STATE_NAGIOS_START or 
									AU_STATE_NAGIOS_STOP */
	char		*description;
	} au_log_nagios;

/* au_log_entry keeps information about each log entry */
typedef struct au_log_entry_struct {
	time_t		timestamp;
	int			entry_type;		/* AU_LOGTYPE_* */
	void		*entry;			/* au_log_alert *, au_log_notification *, 
									au_log_downtime *, or au_log_nagios * */
	} au_log_entry;

typedef struct au_log_struct {
	au_array		*host_subjects;		/* hosts to parse when specified
											in the query */
	au_array		*service_subjects;	/* services to parse when specified
											in the query */
	au_linked_list	*entry_list;		/* linked list of log entries */
	au_array		*hosts;				/* list of hosts and their log entries
											discovered during parsing */
	au_array		*services;			/* list of services and the log
											entries discovered during parsing */
	au_array		*contacts;			/* list of contacts associated with
											notification logs */
	} au_log;

/* External functions */
extern au_log *au_init_log(void);
extern int read_archived_data(time_t, time_t, int, unsigned, unsigned, 
		unsigned, au_log *, time_t *);
extern int	au_cmp_log_entries(const void *, const void *);
extern void au_free_log(au_log *);

extern au_node *au_list_add_node(au_linked_list *, void *, 
		int(*)(const void *, const void *));

extern int au_add_alert_or_state_log(au_log *, time_t, int, int, void *, int, 
		int, char *);
extern au_log_alert *au_create_alert_or_state_log(int, void *, int, int, 
		char *);
extern void au_free_alert_log(au_log_alert *);

extern au_host *au_add_host(au_array *, char *);
extern au_host *au_find_host(au_array *, char *);
extern au_service *au_add_service(au_array *, char *, char *);
extern au_service *au_find_service(au_array *, char *, char *);
extern au_array *au_init_array(char *);
extern void au_free_array(au_array *, void(*)(void *));
extern int au_array_append_member(au_array *, void *);

/* External variables */
extern const string_value_mapping svm_au_object_types[];
extern const string_value_mapping svm_au_state_types[];
extern const string_value_mapping svm_au_states[];
extern const string_value_mapping svm_au_log_types[];
extern const string_value_mapping svm_au_notification_types[];

#endif
