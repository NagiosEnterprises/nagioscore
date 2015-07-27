/**************************************************************************
 *
 * STATUSJSON.H -  Nagios CGI for returning JSON-formatted status data
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

#ifndef STATUSJSON_H_INCLUDED
#define STATUSJSON_H_INCLUDED

/* Structure containing CGI query string options and values */
typedef struct status_json_cgi_data_struct {
	/* Format options for JSON output */
	unsigned	format_options;
	/* Query being requested */
	int			query;
	/* Index of starting object returned for list requests */
	int			start;
	/* Number of objects returned for list requests */
	int			count;
	/* Return details for each entity in a list request */
	int			details;
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
	/* The host status selector values */
	unsigned	host_statuses;
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
	/* The service status selector values */
	unsigned	service_statuses;
	/* Name of service whose children should be returned if parentservice is
		specified */
	char *		parent_service_name;
	/* Name of service whose parents should be returned if childservice is
		specified */
	char *		child_service_name;
	/* Name of contactgroup for which details should be returned */
	char *		contactgroup_name;
	/* Contactgroup whose name is contactgroup_name */
	contactgroup *	contactgroup;
	/* Name of contact for which details should be returned */
	char *		contact_name;
	/* Contact whose contact name is contact_name */
	contact *	contact;
	/* Name of check timeperiod for which details should be returned */
	char *		check_timeperiod_name;
	/* Timeperiod whose timeperiod name is check_timeperiod_name */
	timeperiod *check_timeperiod;
	/* Name of host notification timeperiod for which details should
			be returned */
	char *		host_notification_timeperiod_name;
	/* Timeperiod whose timeperiod name is host_notification_timeperiod_name */
	timeperiod *host_notification_timeperiod;
	/* Name of service notification timeperiod for which details should
		be returned */
	char *		service_notification_timeperiod_name;
	/* Timeperiod whose timeperiod name is
		service_notification_timeperiod_name */
	timeperiod *service_notification_timeperiod;
	/* Name of check command to be used as a selector */
	char *		check_command_name;
	/* Command whose command name is check_command_name */
	command *	check_command;
	/* Name of event handle to be used as a selector */
	char *		event_handler_name;
	/* Command whose command name is event_handler_name */
	command *	event_handler;
	/* Type(s) of comments to include in comment count and list results */
	unsigned	comment_types;
	/* Entry type(s) of comments to include in comment count and list results */
	unsigned	entry_types;
	/* Persistence(s) of comments to include in comment count and list 
 		results */
	unsigned	persistence;
	/* Whether comments that are expiring, non-expiring, or both are included 
 		in comment count and list results */
	unsigned	expiring;
	/* ID of comment for which details should be returned */
	int			comment_id;
	/* Comment whose id is comment_id */
	nagios_comment *comment;
	/* ID of downtime for which details should be returned */
	int		 	downtime_id;
	/* Downtime whose id is downtime_id */
	scheduled_downtime * downtime;
	/* Start time for time-based queries */
	time_t		start_time;
	/* End time for time-based queries */
	time_t		end_time;
	/* Field on which to base time for hostcount and hostlist queries */
	int			host_time_field;
	/* Field on which to base time for servicecount and servicelist queries */
	int			service_time_field;
	/* Field on which to base time for commentcount and commentlist queries */
	int			comment_time_field;
	/* Field on which to base time for downtimecount and downtimelist queries */
	int			downtime_time_field;
	/* Object type to use for downtimecount and downtimelist queries */
	unsigned	downtime_object_types;
	/* Downtime type to use for downtimecount and downtimelist queries */
	unsigned	downtime_types;
	/* Whether downtimes that are triggered, non-triggered, or both are 
		included in downtime count and list results */
	unsigned	triggered;
	/* ID of a triggering downtime */
	int			triggered_by;
	/* Whether downtimes that are or are not in effect or both are included 
		in downtime count and list results */
	unsigned	in_effect;
	} status_json_cgi_data;

/* Status Type Information */
#define STATUS_QUERY_INVALID				0
#define STATUS_QUERY_HOSTCOUNT				1
#define STATUS_QUERY_HOSTLIST				2
#define STATUS_QUERY_HOST					3
#define STATUS_QUERY_SERVICECOUNT			4
#define STATUS_QUERY_SERVICELIST			5
#define STATUS_QUERY_SERVICE				6
#if 0
#define STATUS_QUERY_CONTACTCOUNT			7
#define STATUS_QUERY_CONTACTLIST			8
#define STATUS_QUERY_CONTACT				9
#endif
#define STATUS_QUERY_COMMENTCOUNT			10
#define STATUS_QUERY_COMMENTLIST			11
#define STATUS_QUERY_COMMENT				12
#define STATUS_QUERY_DOWNTIMECOUNT			13
#define STATUS_QUERY_DOWNTIMELIST			14
#define STATUS_QUERY_DOWNTIME				16
#define STATUS_QUERY_PROGRAMSTATUS			17
#define STATUS_QUERY_PERFORMANCEDATA		18
#define STATUS_QUERY_HELP					19

/* Status Time Fields */
#define STATUS_TIME_INVALID					0
#define STATUS_TIME_LAST_UPDATE				1	/* host, service */
#define STATUS_TIME_LAST_CHECK				2	/* host, service */
#define STATUS_TIME_NEXT_CHECK				3	/* host, service */
#define STATUS_TIME_LAST_STATE_CHANGE		4	/* host, service */
#define STATUS_TIME_LAST_HARD_STATE_CHANGE	5	/* host, service */
#define STATUS_TIME_LAST_TIME_UP			6	/* host */
#define STATUS_TIME_LAST_TIME_DOWN			7	/* host */
#define STATUS_TIME_LAST_TIME_UNREACHABLE	8	/* host */
#define STATUS_TIME_LAST_TIME_OK			9	/* service */
#define STATUS_TIME_LAST_TIME_WARNING		10	/* service */
#define STATUS_TIME_LAST_TIME_CRITICAL		11	/* service */
#define STATUS_TIME_LAST_TIME_UNKNOWN		12	/* service */
#define STATUS_TIME_LAST_NOTIFICATION		13	/* host, service */
#define STATUS_TIME_NEXT_NOTIFICATION		14	/* host, service */
#define STATUS_TIME_ENTRY_TIME				15	/* comment, downtime */
#define STATUS_TIME_EXPIRE_TIME				16	/* comment */
#define STATUS_TIME_START_TIME				17	/* downtime */
#define STATUS_TIME_FLEX_DOWNTIME_START		18	/* downtime */
#define STATUS_TIME_END_TIME				19	/* downtime */

/* Comment Types */
#define COMMENT_TYPE_HOST		1
#define COMMENT_TYPE_SERVICE	2
#define COMMENT_TYPE_ALL 		(COMMENT_TYPE_HOST | COMMENT_TYPE_SERVICE)

/* Comment Entry Types */
#define COMMENT_ENTRY_USER				1
#define COMMENT_ENTRY_DOWNTIME			2
#define COMMENT_ENTRY_FLAPPING			4
#define COMMENT_ENTRY_ACKNOWLEDGEMENT	8
#define COMMENT_ENTRY_ALL				(COMMENT_ENTRY_USER |\
											COMMENT_ENTRY_DOWNTIME |\
											COMMENT_ENTRY_FLAPPING |\
											COMMENT_ENTRY_ACKNOWLEDGEMENT)

/* Downtime Object Types */
#define DOWNTIME_OBJECT_TYPE_HOST		1
#define DOWNTIME_OBJECT_TYPE_SERVICE	2
#define DOWNTIME_OBJECT_TYPE_ALL 		(DOWNTIME_OBJECT_TYPE_HOST |\
											DOWNTIME_OBJECT_TYPE_SERVICE)

/* Downtime Types */
#define DOWNTIME_TYPE_FIXED		1
#define DOWNTIME_TYPE_FLEXIBLE	2
#define DOWNTIME_TYPE_ALL 		(DOWNTIME_TYPE_FIXED | DOWNTIME_TYPE_FLEXIBLE)

extern json_object *json_status_hostcount(unsigned, int, host *, int, host *, 
		hostgroup *, int, contact *, int, time_t, time_t, contactgroup *,
		timeperiod *, timeperiod *, command *, command *);
extern json_object *json_status_hostlist(unsigned, int, int, int, int, host *, 
		int, host *, hostgroup *, int, contact *, int, time_t, time_t,
		contactgroup *, timeperiod *, timeperiod *, command *, command *);
extern json_object *json_status_host(unsigned, host *, hoststatus *);
extern void json_status_host_details(json_object *, unsigned, host *, 
		hoststatus *);

extern json_object *json_status_servicecount(unsigned, host *, int, host *, 
		int, host *, hostgroup *, servicegroup *, int, int, contact *, int, 
		time_t, time_t, char *, char *, char *, contactgroup *, timeperiod *,
		timeperiod *, command *, command *);
extern json_object *json_status_servicelist(unsigned, int, int, int, host *, 
		int, host *, int, host *, hostgroup *, servicegroup *, int, int, 
		contact *, int, time_t, time_t, char *, char *, char *, contactgroup *,
		timeperiod *, timeperiod *, command *, command *);
extern json_object *json_status_service(unsigned, service *, servicestatus *);
extern void json_status_service_details(json_object *, unsigned, service *,
		servicestatus *);

#if 0
extern void json_status_contactlist(unsigned, unsigned, unsigned, unsigned, 
		unsigned, contactgroup *);
extern void json_status_contactlist(unsigned, unsigned, unsigned, unsigned, 
		unsigned, contactgroup *);
extern void json_status_contact(unsigned, unsigned, contact *);
extern void json_status_contact_details(unsigned, unsigned, contact *);
#endif

extern json_object *json_status_commentcount(unsigned, int, time_t, time_t,
		unsigned, unsigned, unsigned, unsigned, char *, char *);
extern json_object *json_status_commentlist(unsigned, int, int, int, int, 
		time_t, time_t, unsigned, unsigned, unsigned, unsigned, char *, char *);
extern json_object *json_status_comment(unsigned, nagios_comment *);
extern void json_status_comment_details(json_object *, unsigned,
		nagios_comment *);

extern json_object *json_status_downtimecount(unsigned, int, time_t, time_t, 
		unsigned, unsigned, unsigned, int, unsigned, char *, char *);
extern json_object *json_status_downtimelist(unsigned, int, int, int, int, 
		time_t, time_t, unsigned, unsigned, unsigned, int, unsigned, char *,
		char *);
extern json_object *json_status_downtime(unsigned, scheduled_downtime *);
extern void json_status_downtime_details(json_object *, unsigned, 
		scheduled_downtime *);

extern json_object *json_status_program(unsigned);

extern json_object *json_status_performance(void);

#endif
