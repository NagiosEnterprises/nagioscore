/**************************************************************************
 *
 * ARCHIVEJSON.H -  Nagios CGI for returning JSON-formatted archive data
 *
 * Copyright (c) 2013 Nagios Enterprises, LLC
 * Last Modified: 04-13-2013
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

#ifndef ARCHIVEJSON_H_INCLUDED
#define ARCHIVEJSON_H_INCLUDED

/* Structure containing CGI query string options and values */
typedef struct archive_json_cgi_data_struct {
	/* Format options for JSON output */
	unsigned	format_options;
	/* Query being requested */
	int			query;
	/* Index of starting object returned for list requests */
	int			start;
	/* Number of objects returned for list requests */
	int			count;
	/* Type(s) of object to be queried (host and/or service) */
	unsigned	object_types;
	/* Type of object to be queried (host and/or service) */
	int			object_type;
	/* State types to include in query results */
	unsigned	state_types;
	/* Host states to include in query results */
	unsigned	host_states;
	/* Service states to include in query results */
	unsigned	service_states;
	/* Host notification types to include in query results */
	unsigned	host_notification_types;
	/* Service notification types to include in query results */
	unsigned	service_notification_types;
	/* strftime format string for time_t values */
	char *		strftime_format;
	/* Name of host whose children should be returned if parenthost is 
		specified */
	char *		parent_host_name;
	/* Use the parent host field or search all hosts*/
	int			use_parent_host;
	/* Host whose children should be returned if use_parent_host is non-zero */
	host *		parent_host;
	/* Name of host whose parents should be returned if childhost is 
		specified */
	char *		child_host_name;
	/* Use the child host field or search all hosts*/
	int			use_child_host;
	/* Host whose parents should be returned if use_child_host is non-zero */
	host *		child_host;
	/* Name of host for which details should be returned */
	char *		host_name;
	/* Host whose host name is host_name */
	host *		host;
	/* Name of hostgroup for which details should be returned */
	char *		hostgroup_name;
	/* Hostgroup whose name is hostgroup_name */
	hostgroup *	hostgroup;
	/* Name of servicegroup for which details should be returned */
	char *		servicegroup_name;
	/* Servicegroup whose name is servicegroup_name */
	servicegroup *	servicegroup;
	/* Name of service for which details should be returned */
	char *		service_description;
	/* Service whose host name is host_name and whose description is 
		service_description*/
	service *	service;
	/* Name of contact for which details should be returned */
	char *		contact_name;
	/* Contact whose contact name is contact_name */
	contact *	contact;
	/* Name of contactgroup for which details should be returned */
	char *		contactgroup_name;
	/* Contactgroup whose name is contactgroup_name */
	contactgroup *	contactgroup;
	/* Notification method */
	char *		notification_method;
	/* Report timeperiod name */
	char *		timeperiod_name;
	/* Timeperiod whose name is timeperiod_name */
	timeperiod *timeperiod;
	/* Assumed initial host state */
	int			assumed_initial_host_state;
	/* Assumed initial service state */
	int			assumed_initial_service_state;
	/* Assume initial state for host(s) or service(s) */
	int			assume_initial_state;
	/* Assume states are retained */
	int			assume_state_retention;
	/* Assume states during Nagios downtime */
	int			assume_states_during_nagios_downtime;
	/* Start time */
	time_t		start_time;
	/* End time */
	time_t		end_time;
	/* Number of backtrack archives to read */
	int			backtracked_archives;
	} archive_json_cgi_data;

/* Object Type Information */
#define ARCHIVE_QUERY_INVALID				0
#define ARCHIVE_QUERY_HELP					1
#define ARCHIVE_QUERY_ALERTCOUNT			2
#define ARCHIVE_QUERY_ALERTLIST				3
#define ARCHIVE_QUERY_NOTIFICATIONCOUNT		4
#define ARCHIVE_QUERY_NOTIFICATIONLIST		5
#define ARCHIVE_QUERY_STATECHANGELIST		6
#define ARCHIVE_QUERY_AVAILABILITY			7

extern json_object *json_archive_alertcount(unsigned, time_t, time_t, int, 
		char *, char *, int, host *, int, host *, hostgroup *, servicegroup *, 
		contact *, contactgroup *, unsigned, unsigned, unsigned, au_log *);
extern json_object *json_archive_alertlist(unsigned, int, int, time_t, time_t, 
		int, char *, char *, int, host *, int, host *, hostgroup *, 
		servicegroup *, contact *, contactgroup *, unsigned, unsigned, 
		unsigned, au_log *);
extern void json_archive_alert_details(json_object *, unsigned, time_t, 
		au_log_alert *);

extern json_object * json_archive_notificationcount(unsigned, time_t, time_t, 
		int, char *, char *, int, host *, int, host *, hostgroup *, 
		servicegroup *, char *, contactgroup *, unsigned, unsigned, char *, 
		au_log *);
extern json_object * json_archive_notificationlist(unsigned, int, int, time_t, 
		time_t, int, char *, char *, int, host *, int, host *, hostgroup *, 
		servicegroup *, char *, contactgroup *, unsigned, unsigned, char *, 
		au_log *);
extern void json_archive_notification_details(json_object *, unsigned, time_t, 
		au_log_notification *);

extern json_object *json_archive_statechangelist(unsigned, int, int, time_t, 
		time_t, int, char *, char *, int, int, unsigned, au_log *);

extern json_object *json_archive_availability(unsigned, time_t, time_t, time_t, 
		int, char *, char *, hostgroup *, servicegroup *, timeperiod *, int, 
		int, int, int, int, unsigned, au_log *);
#endif
