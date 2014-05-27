/**************************************************************************
 *
 * OBJECTJSON.H -  Nagios CGI for returning JSON-formatted object data
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

#ifndef OBJECTJSON_H_INCLUDED
#define OBJECTJSON_H_INCLUDED

/* Structure containing CGI query string options and values */
typedef struct object_json_cgi_data_struct {
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
	/* Use the parent host field or search all hosts */
	int			use_parent_host;
	/* Host whose children should be returned if use_parent_host is non-zero */
	host *		parent_host;
	/* Name of host whose parents should be returned if childhost is 
		specified */
	char *		child_host_name;
	/* Use the child host field or search all hosts */
	int			use_child_host;
	/* Host whose parents should be returned if use_child_host is non-zero */
	host *		child_host;
	/* Name of host for which details should be returned */
	char *		host_name;
	/* Host whose host name is host_name */
	host *		host;
	/* Name of host whose hostgroup should be returned if hostgroupmember is 
		specified */
	char *		hostgroup_member_name;
	/* Host whose hostgroup should be returned if hostgroup_member_name is 
		specified */
	host *		hostgroup_member;
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
	/* Name of host for which servicegroup should be returned if 
		servicegroupmemberhost and servicegroupmemberservice are specified */
	char *		servicegroup_member_host_name;
	/* Name of service for which servicegroup should be returned if 
		servicegroupmemberhost and servicegroupmemberservice are specified */
	char *		servicegroup_member_service_description;
	/* Service whose servicegroup should be returned if 
		servicegroup_member_host_name and 
		servicegroup_member_service_description are specified */
	service *	servicegroup_member;
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
	/* Name of contact whose contactgroup should be returned if 
		contactgroupmember is specified */
	char *		contactgroup_member_name;
	/* Contact whose contactgroup should be returned if 
		contactgroup_member_name is specified */
	contact *	contactgroup_member;
	/* Name of timeperiod for which details should be returned */
	char *		timeperiod_name;
	/* Timeperiod whose timeperiod name is timeperiod_name */
	timeperiod *timeperiod;
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
	/* Name of command for which details should be returned */
	char *		command_name;
	/* Command whose command name is command_name */
	command *	command;
	/* Name of check command to be used as a selector */
	char *		check_command_name;
	/* Command whose command name is check_command_name */
	command *	check_command;
	/* Name of event handler to be used as a selector */
	char *		event_handler_name;
	/* Command whose command name is event_handler_name */
	command *	event_handler;
	/* Name of master host to be used as a selector for dependencies */
	char *		master_host_name;
	/* Host whose host name is master_host_name */
	host *		master_host;
	/* Name of master hostgroup to be used as a selector for dependencies */
	char *		master_hostgroup_name;
	/* Host whose hostgroup name is master_hostgroup_name */
	hostgroup *	master_hostgroup;
	/* Name of master service to be used as a selector for dependencies */
	char *		master_service_description;
	/* Service whose service name is master_service_description */
	service *	master_service;
	/* Name of master servicegroup to be used as a selector for dependencies */
	char *		master_servicegroup_name;
	/* Service whose servicegroup name is master_servicegroup_name */
	servicegroup *	master_servicegroup;
	/* Name of dependent host to be used as a selector for dependencies */
	char *		dependent_host_name;
	/* Host whose host name is dependent_host_name */
	host *		dependent_host;
	/* Name of dependent hostgroup to be used as a selector for dependencies */
	char *		dependent_hostgroup_name;
	/* Host whose hostgroup name is dependent_hostgroup_name */
	hostgroup *	dependent_hostgroup;
	/* Name of dependent service to be used as a selector for dependencies */
	char *		dependent_service_description;
	/* Service whose service name is dependent_service_description */
	service *	dependent_service;
	/* Name of dependent servicegroup to be used as a selector for
			dependencies */
	char *		dependent_servicegroup_name;
	/* Service whose servicegroup name is dependent_servicegroup_name */
	servicegroup *	dependent_servicegroup;
	} object_json_cgi_data;

/* Object Type Information */
#define OBJECT_QUERY_INVALID				0
#define OBJECT_QUERY_HOSTCOUNT				1
#define OBJECT_QUERY_HOSTLIST				2
#define OBJECT_QUERY_HOST					3
#define OBJECT_QUERY_HOSTGROUPCOUNT			4
#define OBJECT_QUERY_HOSTGROUPLIST			5
#define OBJECT_QUERY_HOSTGROUP				6
#define OBJECT_QUERY_SERVICECOUNT			7
#define OBJECT_QUERY_SERVICELIST			8
#define OBJECT_QUERY_SERVICE				9
#define OBJECT_QUERY_SERVICEGROUPCOUNT		10
#define OBJECT_QUERY_SERVICEGROUPLIST		11
#define OBJECT_QUERY_SERVICEGROUP			12
#define OBJECT_QUERY_CONTACTCOUNT			13
#define OBJECT_QUERY_CONTACTLIST			14
#define OBJECT_QUERY_CONTACT				15
#define OBJECT_QUERY_CONTACTGROUPCOUNT		16
#define OBJECT_QUERY_CONTACTGROUPLIST		17
#define OBJECT_QUERY_CONTACTGROUP			18
#define OBJECT_QUERY_TIMEPERIODCOUNT		19
#define OBJECT_QUERY_TIMEPERIODLIST			20
#define OBJECT_QUERY_TIMEPERIOD				21
#define OBJECT_QUERY_COMMANDCOUNT			22
#define OBJECT_QUERY_COMMANDLIST			23
#define OBJECT_QUERY_COMMAND				24
#define OBJECT_QUERY_SERVICEDEPENDENCYCOUNT	25
#define OBJECT_QUERY_SERVICEDEPENDENCYLIST	26
#define OBJECT_QUERY_SERVICEESCALATIONCOUNT	27
#define OBJECT_QUERY_SERVICEESCALATIONLIST	28
#define OBJECT_QUERY_HOSTDEPENDENCYCOUNT	29
#define OBJECT_QUERY_HOSTDEPENDENCYLIST		30
#define OBJECT_QUERY_HOSTESCALATIONCOUNT	31
#define OBJECT_QUERY_HOSTESCALATIONLIST		32
#define OBJECT_QUERY_HELP					33

extern json_object * json_object_custom_variables(struct customvariablesmember *);

extern json_object *json_object_hostcount(int, host *, int, host *, hostgroup *,
		contact *, contactgroup *, timeperiod *, timeperiod *, command *,
		command *);
extern json_object *json_object_hostlist(unsigned, int, int, int, int, host *, 
		int, host *, hostgroup *, contact *, contactgroup *, timeperiod *,
		timeperiod *, command *, command *);
extern json_object *json_object_host(unsigned, host *);
extern void json_object_host_details(json_object *, unsigned, host *);

extern json_object *json_object_hostgroupcount(unsigned, host *);
extern json_object *json_object_hostgrouplist(unsigned, int, int, int, host *);
extern json_object *json_object_hostgroup(unsigned, hostgroup *);
extern void json_object_hostgroup_details(json_object *, unsigned, hostgroup *);

extern json_object *json_object_servicecount(host *, int, host *, int, host *, 
		hostgroup *, servicegroup *, contact *, char *, char *, char *,
		contactgroup *, timeperiod *, timeperiod *, command *, command *);
extern json_object *json_object_servicelist(unsigned, int, int, int, host *, 
		int, host *, int, host *, hostgroup *, servicegroup *, contact *, 
		char *, char *, char *, contactgroup *, timeperiod *, timeperiod *,
		command *, command *);
extern json_object *json_object_service(unsigned, service *);
extern void json_object_service_details(json_object *, unsigned, service *);

extern json_object *json_object_servicegroupcount(service *);
extern json_object *json_object_servicegrouplist(unsigned, int, int, int, 
		service *);
extern json_object *json_object_servicegroup(unsigned, servicegroup *);
extern void json_object_servicegroup_details(json_object *, unsigned, 
		servicegroup *);

extern json_object *json_object_contactcount(contactgroup *, timeperiod *,
		timeperiod *);
extern json_object *json_object_contactlist(unsigned, int, int, int, 
		contactgroup *, timeperiod *, timeperiod *);
extern json_object *json_object_contact(unsigned, contact *);
extern void json_object_contact_details(json_object *, unsigned, contact *);

extern json_object *json_object_contactgroupcount(contact *);
extern json_object *json_object_contactgrouplist(unsigned, int, int, int, 
		contact *);
extern json_object *json_object_contactgroup(unsigned, contactgroup *);
extern void json_object_contactgroup_details(json_object *, unsigned, 
		contactgroup *);

extern json_object *json_object_timeperiodcount(void);
extern json_object *json_object_timeperiodlist(unsigned, int, int, int);
extern json_object *json_object_timeperiod(unsigned, timeperiod *);
extern void json_object_timeperiod_details(json_object *, unsigned, timeperiod *);
extern void json_object_timerange(json_array *, unsigned, timerange *);
extern json_object *json_object_daterange(unsigned, daterange *, int);

extern json_object *json_object_commandcount(void);
extern json_object *json_object_commandlist(unsigned, int, int, int);
extern json_object *json_object_command(unsigned, command *);
extern void json_object_command_details(json_object *, unsigned, command *);

extern json_object *json_object_servicedependencycount(host *, hostgroup *,
		char *, servicegroup *, host *, hostgroup *, char *, servicegroup *);
extern json_object *json_object_servicedependencylist(unsigned, int, int,
		host *, hostgroup *, char *, servicegroup *, host *, hostgroup *,
		char *, servicegroup *);
extern void json_object_servicedependency_details(json_object *, unsigned, 
		servicedependency *);

extern json_object *json_object_serviceescalationcount(host *, char *,
		hostgroup *, servicegroup *, contact *, contactgroup *);
extern json_object *json_object_serviceescalationlist(unsigned, int, int, 
		host *, char *, hostgroup *, servicegroup *, contact *, contactgroup *);
extern void json_object_serviceescalation_details(json_object *, unsigned, 
		serviceescalation *);

extern json_object *json_object_hostdependencycount(host *, hostgroup *,
		host *, hostgroup *);
extern json_object *json_object_hostdependencylist(unsigned, int, int, host *,
		hostgroup *, host *, hostgroup *);
extern void json_object_hostdependency_details(json_object *, unsigned, 
		hostdependency *);

extern json_object *json_object_hostescalationcount(host *, hostgroup *,
		contact *, contactgroup *);
extern json_object *json_object_hostescalationlist(unsigned, int, int, host *,
		hostgroup *, contact *, contactgroup *);
extern void json_object_hostescalation_details(json_object *, unsigned, 
		hostescalation *);

#endif
