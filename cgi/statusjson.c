/**************************************************************************
 *
 * STATUSJSON.C -  Nagios CGI for returning JSON-formatted status data
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

/*
	TODO:
		Return an error when the authenticated user is not authorized
			for the requested host or service
		Consolidate criteria functions for *count and *list queries
		Move static programstatus information to objectjson.c?
*/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"
#include "../include/comments.h"
#include "../include/downtime.h"

#include "../include/cgiutils.h"
#include "../include/getcgi.h"
#include "../include/cgiauth.h"
#include "../include/jsonutils.h"
#include "../include/objectjson.h"
#include "../include/statusjson.h"

#define THISCGI "statusjson.cgi"

#ifdef JSON_NAGIOS_4X
#define HOST_STATUS_ALL (SD_HOST_UP | SD_HOST_DOWN | SD_HOST_UNREACHABLE | HOST_PENDING)
#define SERVICE_STATUS_ALL (SERVICE_OK | SERVICE_WARNING | SERVICE_CRITICAL | SERVICE_UNKNOWN | SERVICE_PENDING)
#else
#define HOST_STATUS_ALL (HOST_UP | HOST_DOWN | HOST_UNREACHABLE | HOST_PENDING)
#define SERVICE_STATUS_ALL (SERVICE_OK | SERVICE_WARNING | SERVICE_CRITICAL | SERVICE_UNKNOWN | SERVICE_PENDING)
#endif

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char *status_file;

extern host *host_list;
extern hoststatus *hoststatus_list;
extern service *service_list;
#if 0
extern contact *contact_list;
#endif
extern servicestatus *servicestatus_list;
extern nagios_comment *comment_list;
extern scheduled_downtime *scheduled_downtime_list;

/* Program status variables */
extern unsigned long  modified_host_process_attributes;
extern unsigned long  modified_service_process_attributes;
extern int nagios_pid;
extern int daemon_mode;
extern time_t program_start;
#ifndef JSON_NAGIOS_4X
extern time_t last_command_check;
#endif
extern time_t last_log_rotation;
extern int enable_notifications;
extern int execute_service_checks;
extern int accept_passive_service_checks;
extern int execute_host_checks;
extern int accept_passive_host_checks;
extern int enable_event_handlers;
extern int obsess_over_services;
extern int obsess_over_hosts;
extern int check_service_freshness;
extern int check_host_freshness;
extern int enable_flap_detection;
#ifndef JSON_NAGIOS_4X
extern int enable_failure_prediction;
#endif
extern int process_performance_data;
#if 0
extern char *global_host_event_handler;
extern char *global_service_event_handler;
extern unsigned long next_comment_id;
extern unsigned long next_downtime_id;
extern unsigned long next_event_id;
extern unsigned long next_problem_id;
extern unsigned long next_notification_id;
extern int external_command_buffer_slots;
extern check_stats check_statistics[MAX_CHECK_STATS_TYPES];
#endif

/* Performance data variables */
extern int program_stats[MAX_CHECK_STATS_TYPES][3];
extern int buffer_stats[1][3];

void document_header(int);
void document_footer(void);
void init_cgi_data(status_json_cgi_data *);
int process_cgivars(json_object *, status_json_cgi_data *, time_t);
void free_cgi_data(status_json_cgi_data *);
int validate_arguments(json_object *, status_json_cgi_data *, time_t);

authdata current_authdata;

const string_value_mapping valid_queries[] = {
	{ "hostcount", STATUS_QUERY_HOSTCOUNT,
		"Return the number of hosts in each state" },
	{ "hostlist", STATUS_QUERY_HOSTLIST,
		"Return a list of hosts and their current status" },
	{ "host", STATUS_QUERY_HOST, 
		"Return the status of a single host." },
	{ "servicecount", STATUS_QUERY_SERVICECOUNT,
		"Return the number of services in each state" },
	{ "servicelist", STATUS_QUERY_SERVICELIST,
		"Return a list of services and their current status" },
	{ "service", STATUS_QUERY_SERVICE,
		"Return the status of a single service" },
#if 0
	{ "contactcount", STATUS_QUERY_CONTACTCOUNT,
		"Return the number of contacts" },
	{ "contactlist", STATUS_QUERY_CONTACTLIST,
		"Return a list of of contacts and their current status" },
	{ "contact", STATUS_QUERY_CONTACT,
		"Return a single contact" },
#endif
	{ "commentcount", STATUS_QUERY_COMMENTCOUNT,
		"Return the number of comments" },
	{ "commentlist", STATUS_QUERY_COMMENTLIST,
		"Return a list of comments" },
	{ "comment", STATUS_QUERY_COMMENT,
		"Return a single comment" },
	{ "downtimecount", STATUS_QUERY_DOWNTIMECOUNT,
		"Return the number of downtimes" },
	{ "downtimelist", STATUS_QUERY_DOWNTIMELIST,
		"Return a list of downtimes" },
	{ "downtime", STATUS_QUERY_DOWNTIME,
		"Return a single downtime" },
	{ "programstatus", STATUS_QUERY_PROGRAMSTATUS,
		"Return the Nagios Core program status" },
	{ "performancedata", STATUS_QUERY_PERFORMANCEDATA,
		"Return the Nagios Core performance data" },
	{ "help", STATUS_QUERY_HELP, 
		"Display help for this CGI" },
	{ NULL, -1, NULL},
	};

static const int query_status[][2] = {
	{ STATUS_QUERY_HOSTCOUNT, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_HOSTLIST, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_HOST, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_SERVICECOUNT, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_SERVICELIST, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_SERVICE, QUERY_STATUS_RELEASED },
#if 0
	{ STATUS_QUERY_CONTACTCOUNT, QUERY_STATUS_BETA },
	{ STATUS_QUERY_CONTACTLIST, QUERY_STATUS_BETA },
	{ STATUS_QUERY_CONTACT, QUERY_STATUS_BETA },
#endif
	{ STATUS_QUERY_COMMENTCOUNT, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_COMMENTLIST, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_COMMENT, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_DOWNTIMECOUNT, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_DOWNTIMELIST, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_DOWNTIME, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_PROGRAMSTATUS, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_PERFORMANCEDATA, QUERY_STATUS_RELEASED },
	{ STATUS_QUERY_HELP, QUERY_STATUS_RELEASED },
	{ -1, -1},
	};

const string_value_mapping svm_host_time_fields[] = {
	{ "lastupdate", STATUS_TIME_LAST_UPDATE, "Last Update" },
	{ "lastcheck", STATUS_TIME_LAST_CHECK, "Last Check" },
	{ "nextcheck", STATUS_TIME_NEXT_CHECK, "Next Check" },
	{ "laststatechange", STATUS_TIME_LAST_STATE_CHANGE, "Last State Change" },
	{ "lasthardstatechange", STATUS_TIME_LAST_HARD_STATE_CHANGE, 
			"Last Hard State Change" },
	{ "lasttimeup", STATUS_TIME_LAST_TIME_UP, "Last Time Up" },
	{ "lasttimedown", STATUS_TIME_LAST_TIME_DOWN, "Last Time Down" },
	{ "lasttimeunreachable", STATUS_TIME_LAST_TIME_UNREACHABLE, 
			"Last Time Unreachable" },
	{ "lastnotification", STATUS_TIME_LAST_NOTIFICATION, "Last Notification" },
	{ "nextnotification", STATUS_TIME_NEXT_NOTIFICATION, "Next Notification" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_service_time_fields[] = {
	{ "lastupdate", STATUS_TIME_LAST_UPDATE, "Last Update" },
	{ "lastcheck", STATUS_TIME_LAST_CHECK, "Last Check" },
	{ "nextcheck", STATUS_TIME_NEXT_CHECK, "Next Check" },
	{ "laststatechange", STATUS_TIME_LAST_STATE_CHANGE, "Last State Change" },
	{ "lasthardstatechange", STATUS_TIME_LAST_HARD_STATE_CHANGE, 
			"Last Hard State Change" },
	{ "lasttimeok", STATUS_TIME_LAST_TIME_OK, "Last Time OK" },
	{ "lasttimewarning", STATUS_TIME_LAST_TIME_WARNING, "Last Time Warning" },
	{ "lasttimecritical", STATUS_TIME_LAST_TIME_CRITICAL, 
			"Last Time Critical" },
	{ "lasttimeunknown", STATUS_TIME_LAST_TIME_UNKNOWN, "Last Time Unknown" },
	{ "lastnotification", STATUS_TIME_LAST_NOTIFICATION, "Last Notification" },
	{ "nextnotification", STATUS_TIME_NEXT_NOTIFICATION, "Next Notification" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_valid_comment_types[] = {
	{ "host", COMMENT_TYPE_HOST, "Host Comment" },
	{ "service", COMMENT_TYPE_SERVICE, "Service Comment" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_valid_comment_entry_types[] = {
	{ "user", COMMENT_ENTRY_USER, "User Comment" },
	{ "downtime", COMMENT_ENTRY_DOWNTIME, "Downtime Comment" },
	{ "flapping", COMMENT_ENTRY_FLAPPING, "Flapping Comment" },
	{ "acknowledgement", COMMENT_ENTRY_ACKNOWLEDGEMENT, 
			"Acknowledgement Comment" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_valid_persistence[] = {
	{ "yes", BOOLEAN_TRUE, "Persistent Comment" },
	{ "no", BOOLEAN_FALSE, "Non-Persistent Comment" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_valid_expiration[] = {
	{ "yes", BOOLEAN_TRUE, "Comment Expires" },
	{ "no", BOOLEAN_FALSE, "Comment Does Not Expire" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_comment_time_fields[] = {
	{ "entrytime", STATUS_TIME_ENTRY_TIME, "Entry Time" },
	{ "expiretime", STATUS_TIME_EXPIRE_TIME, "Expiration Time" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_downtime_time_fields[] = {
	{ "entrytime", STATUS_TIME_ENTRY_TIME, "Entry Time" },
	{ "starttime", STATUS_TIME_START_TIME, "Start Time" },
	{ "flexdowntimestart", STATUS_TIME_FLEX_DOWNTIME_START, 
			"Flex Downtime Start" },
	{ "endtime", STATUS_TIME_END_TIME, "End Time" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_valid_downtime_object_types[] = {
	{ "host", DOWNTIME_OBJECT_TYPE_HOST, "Host Downtime" },
	{ "service", DOWNTIME_OBJECT_TYPE_SERVICE, "Service Downtime" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_valid_downtime_types[] = {
	{ "fixed", DOWNTIME_TYPE_FIXED, "Fixed Downtime" },
	{ "flexible", DOWNTIME_TYPE_FLEXIBLE, "Flexible Downtime" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_valid_triggered_status[] = {
	{ "yes", BOOLEAN_TRUE, "Downtime Triggered" },
	{ "no", BOOLEAN_FALSE, "Downtime Not Triggered" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_valid_in_effect_status[] = {
	{ "yes", BOOLEAN_TRUE, "Downtime In Effect" },
	{ "no", BOOLEAN_FALSE, "Downtime Not In Effect" },
	{ NULL, -1, NULL },
	};

option_help status_json_help[] = {
	{ 
		"query",
		"Query",
		"enumeration",
		{ "all", NULL },
		{ NULL },
		NULL,
		"Specifies the type of query to be executed.",
		valid_queries
		},
	{ 
		"formatoptions",
		"Format Options",
		"list",
		{ NULL },
		{ "all", NULL },
		NULL,
		"Specifies the formatting options to be used when displaying the results. Multiple options are allowed and are separated by a plus (+) sign..",
		svm_format_options
		},
	{ 
		"start",
		"Start",
		"integer",
		{ NULL },
		{ "hostlist", "servicelist", NULL },
		NULL,
		"Specifies the index (zero-based) of the first object in the list to be returned.",
		NULL
		},
	{ 
		"count",
		"Count",
		"integer",
		{ NULL },
		{ "hostlist", "servicelist", NULL },
		NULL,
		"Specifies the number of objects in the list to be returned.",
		NULL
		},
	{ 
		"parenthost",
		"Parent Host",
		"nagios:objectjson/hostlist",
		{ NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", NULL },
		NULL,
		"Limits the hosts or serivces returned to those whose host parent is specified. A value of 'none' returns all hosts or services reachable directly by the Nagios core host.",
		parent_host_extras
		},
	{ 
		"childhost",
		"Child Host",
		"nagios:objectjson/hostlist",
		{ NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", NULL },
		NULL,
		"Limits the hosts or serivces returned to those whose having the host specified as a child host. A value of 'none' returns all hosts or services with no child hosts.",
		child_host_extras
		},
	{ 
		"details",
		"Show Details",
		"boolean",
		{ NULL },
		{ "hostlist", "servicelist", "commentlist", "downtimelist", NULL },
		NULL,
		"Returns the details for all entities in the list.",
		NULL
		},
	{ 
		"dateformat",
		"Date Format",
		"string",
		{ NULL },
		{ "all", NULL },
		NULL,
		"strftime format string for values of type time_t. In the absence of a format, the Javascript default format of the number of milliseconds since the beginning of the Unix epoch is used. Because of URL encoding, percent signs must be encoded as %25 and a space must be encoded as a plus (+) sign.",
		NULL
		},
	{ 
		"hostname",
		"Host Name",
		"nagios:objectjson/hostlist",
		{ "host", "service", NULL },
		{ "servicecount", "servicelist", "commentcount", "commentlist", "downtimecount", "downtimelist", NULL },
		NULL,
		"Name for the host requested.",
		NULL
		},
	{ 
		"hostgroup",
		"Host Group",
		"nagios:objectjson/hostgrouplist",
		{ "hostgroup", NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", NULL },
		NULL,
		"Returns information applicable to the hostgroup or the hosts in the hostgroup depending on the query.",
		NULL
		},
	{ 
		"hoststatus",
		"Host Status",
		"list",
		{ NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", NULL },
		NULL,
		"Limits returned information to those hosts whose status matches this list. Host statuses are space separated.",
		svm_host_statuses
		},
	{ 
		"servicegroup",
		"Service Group",
		"nagios:objectjson/servicegrouplist",
		{ "servicegroup", NULL },
		{ "servicecount", "servicelist", NULL },
		NULL,
		"Returns information applicable to the servicegroup or the services in the servicegroup depending on the query.",
		NULL
		},
	{ 
		"servicestatus",
		"Service Status",
		"list",
		{ NULL },
		{ "servicecount", "servicelist" },
		NULL,
		"Limits returned information to those services whose status matches this list. Service statuses are space separated.",
		svm_service_statuses
		},
	{
		"parentservice",
		"Parent Service",
		"nagios:objectjson/servicelist",
		{ NULL },
		{ "servicecount", "servicelist", NULL },
		NULL,
		"Limits the serivces returned to those whose service parent has the name specified. A value of 'none' returns all services with no service parent.",
		parent_service_extras
		},
	{
		"childservice",
		"Child Service",
		"nagios:objectjson/servicelist",
		{ NULL },
		{ "servicecount", "servicelist", NULL },
		NULL,
		"Limits the serivces returned to those whose having the named service as a child service. A value of 'none' returns all services with no child services.",
		child_service_extras
		},
	{ 
		"contactgroup",
		"Contact Group",
		"nagios:objectjson/contactgrouplist",
		{ "contactgroup", NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", NULL },
		NULL,
		"Returns information applicable to the contactgroup or the contacts in the contactgroup depending on the query.",
		NULL
		},
	{ 
		"servicedescription",
		"Service Description",
		"nagios:objectjson/servicelist",
		/* "for query: 'service'",*/
		{ "service", NULL },
		{ "servicecount", "servicelist", "commentcount", "commentlist", "downtimecount", "downtimelist", NULL },
		"hostname",
		"Description for the service requested.",
		NULL
		},
	{ 
		"checktimeperiod",
		"Check Timeperiod Name",
		"nagios:objectjson/timeperiodlist",
		{ NULL },
		{ "hostcount","hostlist", "servicecount", "servicelist", NULL },
		NULL,
		"Name of a check timeperiod to be used as selection criteria.",
		NULL
		},
	{
		"hostnotificationtimeperiod",
		"Host Notification Timeperiod Name",
		"nagios:objectjson/timeperiodlist",
		{ NULL },
		{ "hostcount","hostlist", NULL },
		NULL,
		"Name of a host notification timeperiod to be used as selection criteria.",
		NULL
		},
	{
		"servicenotificationtimeperiod",
		"Service Notification Timeperiod Name",
		"nagios:objectjson/timeperiodlist",
		{ NULL },
		{ "servicecount", "servicelist", NULL },
		NULL,
		"Name of a service notification timeperiod to be used as selection criteria.",
		NULL
		},
	{
		"checkcommand",
		"Check Command Name",
		"nagios:objectjson/commandlist",
		{ NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", NULL },
		NULL,
		"Name of a check command to be be used as a selector.",
		NULL
		},
	{
		"eventhandler",
		"Event Handler Name",
		"nagios:objectjson/commandlist",
		{ NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", NULL },
		NULL,
		"Name of an event handler to be be used as a selector.",
		NULL
		},
	{
		"commenttypes",
		"Comment Type",
		"list",
		{ NULL },
		{ "commentcount", "commentlist", NULL },
		NULL,
		"Comment type for the comment requested.",
		svm_valid_comment_types
		},
	{ 
		"entrytypes",
		"Entry Type",
		"list",
		{ NULL },
		{ "commentcount", "commentlist", NULL },
		NULL,
		"Entry type for the comment requested.",
		svm_valid_comment_entry_types
		},
	{ 
		"persistence",
		"Comment Persistence",
		"list",
		{ NULL },
		{ "commentcount", "commentlist", NULL },
		NULL,
		"Persistence for the comment requested.",
		svm_valid_persistence
		},
	{ 
		"expiring",
		"Comment Expiration",
		"list",
		{ NULL },
		{ "commentcount", "commentlist", NULL },
		NULL,
		"Whether or not the comment expires.",
		svm_valid_expiration
		},
	{ 
		"downtimeobjecttypes",
		"Downtime Object Type",
		"list",
		{ NULL },
		{ "downtimecount", "downtimelist", NULL },
		NULL,
		"The type of object to which the downtime applies.",
		svm_valid_downtime_object_types
		},
	{ 
		"downtimetypes",
		"Downtime Type",
		"list",
		{ NULL },
		{ "downtimecount", "downtimelist", NULL },
		NULL,
		"The type of the downtime.",
		svm_valid_downtime_types
		},
	{ 
		"triggered",
		"Downtime Triggered",
		"list",
		{ NULL },
		{ "downtimecount", "downtimelist", NULL },
		NULL,
		"Whether or not the downtime is triggered.",
		svm_valid_triggered_status
		},
	{ 
		"triggeredby",
		"Triggered By",
		"nagios:statusjson/downtimelist",
		{ NULL },
		{ "downtimecount", "downtimelist", NULL },
		NULL,
		"ID of the downtime which triggers other downtimes.",
		NULL
		},
	{ 
		"ineffect",
		"Downtime In Effect",
		"list",
		{ NULL },
		{ "downtimecount", "downtimelist", NULL },
		NULL,
		"Whether or not the downtime is in effect.",
		svm_valid_in_effect_status
		},
	{ 
		"commentid",
		"Comment ID",
		"nagios:statusjson/commentlist",
		{ "comment", NULL },
		{ NULL },
		NULL,
		"Comment ID for the comment requested.",
		NULL
		},
	{ 
		"downtimeid",
		"Downtime ID",
		"nagios:statusjson/downtimelist",
		{ "downtime", NULL },
		{ NULL },
		NULL,
		"Downtime ID for the downtime requested.",
		NULL
		},
	{ 
		"contactname",
		"Contact Name",
		"nagios:objectjson/contactlist",
		{ NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", NULL },
		NULL,
		"Name for the contact requested.",
		NULL
		},
	{ 
		"hosttimefield",
		"Host Time Field",
		"enumeration",
		{ NULL },
		{ "hostcount", "hostlist", NULL },
		NULL,
		"Field to use when comparing times on a hostlist query.",
		svm_host_time_fields
		},
	{ 
		"servicetimefield",
		"Service Time Field",
		"enumeration",
		{ NULL },
		{ "servicecount", "servicelist", NULL },
		NULL,
		"Field to use when comparing times on a servicelist query.",
		svm_service_time_fields
		},
	{ 
		"commenttimefield",
		"Comment Time Field",
		"enumeration",
		{ NULL },
		{ "commentcount", "commentlist", NULL },
		NULL,
		"Field to use when comparing times on a commentlist query.",
		svm_comment_time_fields
		},
	{ 
		"downtimetimefield",
		"Downtime Time Field",
		"enumeration",
		{ NULL },
		{ "downtimecount", "downtimelist", NULL },
		NULL,
		"Field to use when comparing times on a downtimelist query.",
		svm_downtime_time_fields
		},
	{ 
		"starttime",
		"Start Time",
		"integer",
		{ NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", "commentcount", 
				"commentlist", "downtimecount", "downtimelist", NULL },
		NULL,
		"Starting time to use when querying based on a time range. Not specifying a start time implies all entries since the beginning of time. Supplying a plus or minus sign means times relative to the query time.",
		NULL,
		},
	{ 
		"endtime",
		"End Time",
		"integer",
		{ NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", "commentcount", 
				"commentlist", "downtimecount", "downtimelist", NULL },
		NULL,
		"Ending time to use when querying based on a time range. Not specifying an end time implies all entries until the time of the query. Specifying plus or minus sign means times relative to the query time.",
		NULL,
		},
	{ /* The last entry must contain all NULL entries */
		NULL,
		NULL,
		NULL,
		{ NULL },
		{ NULL },
		NULL,
		NULL,
		NULL
		},
	};

extern const json_escape percent_escapes;

int json_status_host_passes_selection(host *, int, host *, int, host *, 
		hostgroup *, contact *, hoststatus *, int, time_t, time_t,
		contactgroup *, timeperiod *, timeperiod *, command *, command *);
json_object *json_status_host_display_selectors(unsigned, int, int, int, host *,
		int, host *, hostgroup *, int, contact *, int, time_t, time_t,
		contactgroup *, timeperiod *, timeperiod *, command *, command *);

int json_status_service_passes_host_selection(host *, int, host *, int, host *, 
		hostgroup *, host *, int);
int json_status_service_passes_service_selection(service *, servicegroup *, 
		contact *, servicestatus *, int, time_t, time_t, char *, char *,
		char *, contactgroup *, timeperiod *, timeperiod *, command *,
		command *);
json_object *json_status_service_selectors(unsigned, int, int, int, host *, int,
		host *, hostgroup *, host *, servicegroup *, int, int, contact *, int, 
		time_t, time_t, char *, char *, char *, contactgroup *, timeperiod *,
		timeperiod *, command *, command *);

int json_status_comment_passes_selection(nagios_comment *, int, time_t, time_t,
		unsigned, unsigned, unsigned, unsigned, char *, char *);
json_object *json_status_comment_selectors(unsigned, int, int, int, time_t, 
		time_t, unsigned, unsigned, unsigned, unsigned, char *, char *);

int json_status_downtime_passes_selection(scheduled_downtime *, int, time_t, 
		time_t, unsigned, unsigned, unsigned, int, unsigned, char *, char *);
json_object *json_status_downtime_selectors(unsigned, int, int, int, time_t, 
		time_t, unsigned, unsigned, unsigned, int, unsigned, char *, char *);

int main(void) {
	int result = OK;
	time_t query_time;
	status_json_cgi_data	cgi_data;
	json_object *json_root;
	struct stat sdstat;
	time_t	last_status_data_update = (time_t)0;
	hoststatus *temp_hoststatus = NULL;
	servicestatus *temp_servicestatus = NULL;

	/* The official time of the query */
	time(&query_time);

	json_root = json_new_object();
	if(NULL == json_root) {
		printf( "Failed to create new json object\n");
		exit( 1);
		}
	json_object_append_integer(json_root, "format_version", 
			OUTPUT_FORMAT_VERSION);

	/* Initialize shared configuration variables */                             
	init_shared_cfg_vars(1);

	init_cgi_data(&cgi_data);

	document_header(cgi_data.format_options & JSON_FORMAT_WHITESPACE);

	/* get the arguments passed in the URL */
	result = process_cgivars(json_root, &cgi_data, query_time);
	if(result != RESULT_SUCCESS) {
		json_object_append_object(json_root, "data", json_help(status_json_help));
		json_object_print(json_root, 0, 1, cgi_data.strftime_format, 
				cgi_data.format_options);
		document_footer();
		return ERROR;
		}

	/* reset internal variables */
	reset_cgi_vars();

	/* read the CGI configuration file */
	result = read_cgi_config_file(get_cgi_config_location());
	if(result == ERROR) {
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, NULL, RESULT_FILE_OPEN_READ_ERROR,
				"Error: Could not open CGI configuration file '%s' for reading!", 
				get_cgi_config_location()));
		json_object_append_object(json_root, "data", json_help(status_json_help));
		json_object_print(json_root, 0, 1, cgi_data.strftime_format, 
				cgi_data.format_options);
		document_footer();
		return ERROR;
		}

	/* read the main configuration file */
	result = read_main_config_file(main_config_file);
	if(result == ERROR) {
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, NULL, RESULT_FILE_OPEN_READ_ERROR,
				"Error: Could not open main configuration file '%s' for reading!", 
				main_config_file));
		json_object_append_object(json_root, "data", json_help(status_json_help));
		json_object_print(json_root, 0, 1, cgi_data.strftime_format, 
				cgi_data.format_options);
		document_footer();
		return ERROR;
		}

	/* read all object configuration data */
	result = read_all_object_configuration_data(main_config_file, 
			READ_ALL_OBJECT_DATA);
	if(result == ERROR) {
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, NULL, RESULT_FILE_OPEN_READ_ERROR,
				"Error: Could not read some or all object configuration data!"));
		json_object_append_object(json_root, "data", json_help(status_json_help));
		json_object_print(json_root, 0, 1, cgi_data.strftime_format, 
				cgi_data.format_options);
		document_footer();
		return ERROR;
		}

	/* Get the update time on the status data file. This needs to occur before
		the status data is read because the read_all_status_data() function
		clears the name of the status file */
	if(stat(status_file, &sdstat) < 0) {
		json_object_append_object(json_root, "result",
				json_result(query_time, THISCGI,
				svm_get_string_from_value(cgi_data.query, valid_queries),
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, NULL, RESULT_FILE_OPEN_READ_ERROR,
				"Error: Could not obtain status data file status: %s!",
				strerror(errno)));
		json_object_append_object(json_root, "data", json_help(status_json_help));
		document_footer();
		return ERROR;
		}
	last_status_data_update = sdstat.st_mtime;

	/* read all status data */
	result = read_all_status_data(status_file, READ_ALL_STATUS_DATA);
	if(result == ERROR) {
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, NULL, RESULT_FILE_OPEN_READ_ERROR,
				"Error: Could not read host and service status information!"));
		json_object_append_object(json_root, "data", json_help(status_json_help));
		json_object_print(json_root, 0, 1, cgi_data.strftime_format, 
				cgi_data.format_options);
		document_footer();
		return ERROR;
		}

	/* validate arguments in URL */
	result = validate_arguments(json_root, &cgi_data, query_time);
	if(result != RESULT_SUCCESS) {
		json_object_append_object(json_root, "data", json_help(status_json_help));
		json_object_print(json_root, 0, 1, cgi_data.strftime_format, 
				cgi_data.format_options);
		document_footer();
		return ERROR;
		}

	/* get authentication information */
	get_authentication_information(&current_authdata);

	/* Return something to the user */
	switch( cgi_data.query) {
	case STATUS_QUERY_HOSTCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_status_hostcount(cgi_data.format_options, 
				cgi_data.use_parent_host, cgi_data.parent_host, 
				cgi_data.use_child_host, cgi_data.child_host, 
				cgi_data.hostgroup, cgi_data.host_statuses, cgi_data.contact,
				cgi_data.host_time_field, cgi_data.start_time, 
				cgi_data.end_time, cgi_data.contactgroup,
				cgi_data.check_timeperiod,
				cgi_data.host_notification_timeperiod, cgi_data.check_command,
				cgi_data.event_handler));
		break;
	case STATUS_QUERY_HOSTLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_status_hostlist(cgi_data.format_options, cgi_data.start, 
				cgi_data.count, cgi_data.details, cgi_data.use_parent_host, 
				cgi_data.parent_host, cgi_data.use_child_host, 
				cgi_data.child_host, cgi_data.hostgroup, cgi_data.host_statuses,
				cgi_data.contact, cgi_data.host_time_field, cgi_data.start_time,
				cgi_data.end_time, cgi_data.contactgroup,
				cgi_data.check_timeperiod,
				cgi_data.host_notification_timeperiod, cgi_data.check_command,
				cgi_data.event_handler));
		break;
	case STATUS_QUERY_HOST:
		temp_hoststatus = find_hoststatus(cgi_data.host_name);
		if( NULL == temp_hoststatus) {
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data.query, valid_queries), 
					get_query_status(query_status, cgi_data.query),
					(time_t)-1, &current_authdata, RESULT_OPTION_VALUE_INVALID,
					"The status for host '%s' could not be found.", 
					cgi_data.host_name));
			result = ERROR;
			}
		else {
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data.query, valid_queries), 
					get_query_status(query_status, cgi_data.query),
					last_status_data_update, &current_authdata,
					RESULT_SUCCESS, ""));
			json_object_append_object(json_root, "data", 
					json_status_host(cgi_data.format_options, cgi_data.host, 
					temp_hoststatus));
			}
		break;
	case STATUS_QUERY_SERVICECOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_status_servicecount(cgi_data.format_options, cgi_data.host, 
				cgi_data.use_parent_host, cgi_data.parent_host, 
				cgi_data.use_child_host, cgi_data.child_host, 
				cgi_data.hostgroup, cgi_data.servicegroup, 
				cgi_data.host_statuses, cgi_data.service_statuses, 
				cgi_data.contact, cgi_data.service_time_field, 
				cgi_data.start_time, cgi_data.end_time, 
				cgi_data.service_description, cgi_data.parent_service_name,
				cgi_data.child_service_name, cgi_data.contactgroup,
				cgi_data.check_timeperiod,
				cgi_data.service_notification_timeperiod,
				cgi_data.check_command, cgi_data.event_handler));
		break;
	case STATUS_QUERY_SERVICELIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_status_servicelist(cgi_data.format_options, cgi_data.start, 
				cgi_data.count, cgi_data.details, cgi_data.host, 
				cgi_data.use_parent_host, cgi_data.parent_host, 
				cgi_data.use_child_host, cgi_data.child_host, 
				cgi_data.hostgroup, cgi_data.servicegroup,
				cgi_data.host_statuses, cgi_data.service_statuses, 
				cgi_data.contact, cgi_data.service_time_field, 
				cgi_data.start_time, cgi_data.end_time, 
				cgi_data.service_description, cgi_data.parent_service_name,
				cgi_data.child_service_name, cgi_data.contactgroup,
				cgi_data.check_timeperiod,
				cgi_data.service_notification_timeperiod,
				cgi_data.check_command, cgi_data.event_handler));
		break;
	case STATUS_QUERY_SERVICE:
		temp_servicestatus = find_servicestatus(cgi_data.host_name, 
				cgi_data.service_description);
		if( NULL == temp_servicestatus) {
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data.query, valid_queries), 
					get_query_status(query_status, cgi_data.query),
					(time_t)-1, &current_authdata, RESULT_OPTION_VALUE_INVALID,
					"The status for service '%s' on host '%s' could not be found.", 
					cgi_data.service_description, cgi_data.host_name));
			result = ERROR;
			}
		else {
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data.query, valid_queries), 
					get_query_status(query_status, cgi_data.query),
					last_status_data_update, &current_authdata,
					RESULT_SUCCESS, ""));
			json_object_append_object(json_root, "data", 
					json_status_service(cgi_data.format_options, 
					cgi_data.service, temp_servicestatus));
			}
		break;
#if 0
	case STATUS_QUERY_CONTACTCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_status_contactcount(1, cgi_data.format_options, cgi_data.start, 
				cgi_data.count, cgi_data.details, cgi_data.contactgroup);
		break;
	case STATUS_QUERY_CONTACTLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_status_contactlist(1, cgi_data.format_options, cgi_data.start, 
				cgi_data.count, cgi_data.details, cgi_data.contactgroup);
		break;
	case STATUS_QUERY_CONTACT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_status_contact(1, cgi_data.format_options, cgi_data.contact);
		break;
#endif
	case STATUS_QUERY_COMMENTCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_status_commentcount(cgi_data.format_options, 
				cgi_data.comment_time_field, cgi_data.start_time, 
				cgi_data.end_time, cgi_data.comment_types,
				cgi_data.entry_types, cgi_data.persistence,
				cgi_data.expiring, cgi_data.host_name,
				cgi_data.service_description));
		break;
	case STATUS_QUERY_COMMENTLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_status_commentlist(cgi_data.format_options, cgi_data.start,
				cgi_data.count, cgi_data.details, cgi_data.comment_time_field, 
				cgi_data.start_time, cgi_data.end_time, 
				cgi_data.comment_types, cgi_data.entry_types, 
				cgi_data.persistence, cgi_data.expiring, cgi_data.host_name,
				cgi_data.service_description));
		break;
	case STATUS_QUERY_COMMENT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_status_comment(cgi_data.format_options, cgi_data.comment));
		break;
	case STATUS_QUERY_DOWNTIMECOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_status_downtimecount(cgi_data.format_options, 
				cgi_data.downtime_time_field, cgi_data.start_time, 
				cgi_data.end_time, cgi_data.downtime_object_types, 
				cgi_data.downtime_types, cgi_data.triggered,
				cgi_data.triggered_by, cgi_data.in_effect, cgi_data.host_name,
				cgi_data.service_description));
		break;
	case STATUS_QUERY_DOWNTIMELIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_status_downtimelist(cgi_data.format_options, 
				cgi_data.start, cgi_data.count, cgi_data.details,
				cgi_data.downtime_time_field, cgi_data.start_time, 
				cgi_data.end_time, cgi_data.downtime_object_types, 
				cgi_data.downtime_types, cgi_data.triggered,
				cgi_data.triggered_by, cgi_data.in_effect, cgi_data.host_name,
				cgi_data.service_description));
		break;
	case STATUS_QUERY_DOWNTIME:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_status_downtime(cgi_data.format_options, cgi_data.downtime));
		break;
	case STATUS_QUERY_PROGRAMSTATUS:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_status_program(cgi_data.format_options));
		break;
	case STATUS_QUERY_PERFORMANCEDATA:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_status_data_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", json_status_performance());
		break;
	case STATUS_QUERY_HELP:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				compile_time(__DATE__, __TIME__), &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", json_help(status_json_help));
		break;
	default:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, &current_authdata, RESULT_OPTION_MISSING,
				"Error: Object Type not specified. See data for help."));
		json_object_append_object(json_root, "data", json_help(status_json_help));
		break;
		}

	json_object_print(json_root, 0, 1, cgi_data.strftime_format, 
			cgi_data.format_options);
	document_footer();

	/* free all allocated memory */
	free_cgi_data( &cgi_data);
	json_free_object(json_root, 1);
	free_memory();

	return OK;
	}

void document_header(int whitespace) {
	char date_time[MAX_DATETIME_LENGTH];
	time_t expire_time;
	time_t current_time;

	time(&current_time);

	printf("Cache-Control: no-store\r\n");
	printf("Pragma: no-cache\r\n");

	get_time_string(&current_time, date_time, (int)sizeof(date_time), HTTP_DATE_TIME);
	printf("Last-Modified: %s\r\n", date_time);

	expire_time = (time_t)0L;
	get_time_string(&expire_time, date_time, (int)sizeof(date_time), HTTP_DATE_TIME);
	printf("Expires: %s\r\n", date_time);

	printf("Content-type: application/json; charset=utf-8\r\n\r\n");

	return;
	}


void document_footer(void) {

	printf( "\n");

	return;
	}


void init_cgi_data(status_json_cgi_data *cgi_data) {
	cgi_data->format_options = 0;
	cgi_data->query = STATUS_QUERY_INVALID;
	cgi_data->start = 0;
	cgi_data->count = 0;
	cgi_data->details = 0;
	cgi_data->strftime_format = NULL;
	cgi_data->parent_host_name = NULL;
	cgi_data->use_parent_host = 0;
	cgi_data->parent_host = NULL;
	cgi_data->child_host_name = NULL;
	cgi_data->use_child_host = 0;
	cgi_data->child_host = NULL;
	cgi_data->host_name = NULL;
	cgi_data->host = NULL;
	cgi_data->host_statuses = HOST_STATUS_ALL;
	cgi_data->hostgroup_name = NULL;
	cgi_data->hostgroup = NULL;
	cgi_data->servicegroup_name = NULL;
	cgi_data->servicegroup = NULL;
	cgi_data->service_description = NULL;
	cgi_data->service = NULL;
	cgi_data->service_statuses = SERVICE_STATUS_ALL;
	cgi_data->parent_service_name = NULL;
	cgi_data->child_service_name = NULL;
	cgi_data->contactgroup_name = NULL;
	cgi_data->contactgroup = NULL;
	cgi_data->contact_name = NULL;
	cgi_data->contact = NULL;
	cgi_data->check_timeperiod_name = NULL;
	cgi_data->check_timeperiod = NULL;
	cgi_data->host_notification_timeperiod_name = NULL;
	cgi_data->host_notification_timeperiod = NULL;
	cgi_data->service_notification_timeperiod_name = NULL;
	cgi_data->service_notification_timeperiod = NULL;
	cgi_data->check_command_name = NULL;
	cgi_data->check_command = NULL;
	cgi_data->event_handler_name = NULL;
	cgi_data->event_handler = NULL;
	cgi_data->comment_types = COMMENT_TYPE_ALL;
	cgi_data->entry_types = COMMENT_ENTRY_ALL;
	cgi_data->persistence = BOOLEAN_EITHER;
	cgi_data->expiring = BOOLEAN_EITHER;
	cgi_data->comment_id = -1;
	cgi_data->comment = NULL;
	cgi_data->downtime_id = -1;
	cgi_data->downtime = NULL;
	cgi_data->start_time = (time_t)0;
	cgi_data->end_time = (time_t)0;
	cgi_data->host_time_field = STATUS_TIME_INVALID;
	cgi_data->service_time_field = STATUS_TIME_INVALID;
	cgi_data->comment_time_field = STATUS_TIME_INVALID;
	cgi_data->downtime_time_field = STATUS_TIME_INVALID;
	cgi_data->downtime_object_types = DOWNTIME_OBJECT_TYPE_ALL;
	cgi_data->downtime_types = DOWNTIME_TYPE_ALL;
	cgi_data->triggered = BOOLEAN_EITHER;
	cgi_data->triggered_by = -1;
	cgi_data->in_effect = BOOLEAN_EITHER;
}

void free_cgi_data( status_json_cgi_data *cgi_data) {
	if( NULL != cgi_data->parent_host_name) free( cgi_data->parent_host_name);
	if( NULL != cgi_data->child_host_name) free( cgi_data->child_host_name);
	if( NULL != cgi_data->host_name) free( cgi_data->host_name);
	if( NULL != cgi_data->hostgroup_name) free( cgi_data->hostgroup_name);
	if( NULL != cgi_data->servicegroup_name) free(cgi_data->servicegroup_name);
	if( NULL != cgi_data->service_description) free(cgi_data->service_description);
	if( NULL != cgi_data->parent_service_name) free(cgi_data->parent_service_name);
	if( NULL != cgi_data->child_service_name) free(cgi_data->child_service_name);
	if( NULL != cgi_data->contactgroup_name) free(cgi_data->contactgroup_name);
	if( NULL != cgi_data->contact_name) free(cgi_data->contact_name);
	if( NULL != cgi_data->check_timeperiod_name) free(cgi_data->check_timeperiod_name);
	if( NULL != cgi_data->host_notification_timeperiod_name) free(cgi_data->host_notification_timeperiod_name);
	if( NULL != cgi_data->service_notification_timeperiod_name) free(cgi_data->service_notification_timeperiod_name);
	if( NULL != cgi_data->check_command_name) free(cgi_data->check_command_name);
	if( NULL != cgi_data->event_handler_name) free(cgi_data->event_handler_name);
	}


int process_cgivars(json_object *json_root, status_json_cgi_data *cgi_data,
		time_t query_time) {
	char **variables;
	int result = RESULT_SUCCESS;
	int x;
	authdata *authinfo = NULL; /* Currently always NULL because
									get_authentication_information() hasn't
									been called yet, but in case we want to
									use it in the future... */

	variables = getcgivars();

	for(x = 0; variables[x] != NULL; x++) {
		/* We set these each iteration because they could change with each
			iteration */

		/* we found the query argument */
		if(!strcmp(variables[x], "query")) {
			if((result = parse_enumeration_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], valid_queries, &(cgi_data->query)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "formatoptions")) {
			cgi_data->format_options = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_format_options,
					&(cgi_data->format_options))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "start")) {
			if((result = parse_int_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->start))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "count")) {
			if((result = parse_int_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->count))) != RESULT_SUCCESS) {
				break;
				}

			if(cgi_data->count == 0) {
				json_object_append_object(json_root, "result", 
						json_result(query_time, THISCGI, 
						svm_get_string_from_value(cgi_data->query,
						valid_queries),
						get_query_status(query_status, cgi_data->query),
						(time_t)-1, authinfo, RESULT_OPTION_VALUE_INVALID,
						"The count option value is invalid. "
						"It must be an integer greater than zero"));
				result = RESULT_OPTION_VALUE_INVALID;
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "parenthost")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->parent_host_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "childhost")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->child_host_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "hostname")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->host_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "hostgroup")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->hostgroup_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "hoststatus")) {
			cgi_data->host_statuses = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_host_statuses,
					&(cgi_data->host_statuses))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "servicegroup")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->servicegroup_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "servicestatus")) {
			cgi_data->service_statuses = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_service_statuses,
					&(cgi_data->service_statuses))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "parentservice")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->parent_service_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "childservice")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->child_service_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "checktimeperiod")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->check_timeperiod_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "hostnotificationtimeperiod")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1],
					&(cgi_data->host_notification_timeperiod_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "servicenotificationtimeperiod")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1],
					&(cgi_data->service_notification_timeperiod_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "checkcommand")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->check_command_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "eventhandler")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->event_handler_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "commenttypes")) {
			cgi_data->comment_types = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_valid_comment_types,
					&(cgi_data->comment_types))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "entrytypes")) {
			cgi_data->entry_types = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_valid_comment_entry_types,
					&(cgi_data->entry_types))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "persistence")) {
			cgi_data->persistence = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_valid_persistence,
					&(cgi_data->persistence))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "expiring")) {
			cgi_data->expiring = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_valid_expiration,
					&(cgi_data->expiring))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "commentid")) {
			if((result = parse_int_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->comment_id)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "downtimeid")) {
			if((result = parse_int_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->downtime_id)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "downtimeobjecttypes")) {
			cgi_data->downtime_object_types = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_valid_downtime_object_types,
					&(cgi_data->downtime_object_types))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "downtimetypes")) {
			cgi_data->downtime_types = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_valid_downtime_types,
					&(cgi_data->downtime_types))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "triggered")) {
			cgi_data->triggered = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_valid_triggered_status,
					&(cgi_data->triggered))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "triggeredby")) {
			if((result = parse_int_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->triggered_by)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "ineffect")) {
			cgi_data->in_effect = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_valid_in_effect_status,
					&(cgi_data->in_effect))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "contactgroup")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->contactgroup_name)))
					!= RESULT_SUCCESS) {
				result = ERROR;
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "servicedescription")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->service_description)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "contactname")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->contact_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "details")) {
			if((result = parse_boolean_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->details))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "dateformat")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->strftime_format)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "hosttimefield")) {
			if((result = parse_enumeration_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_host_time_fields,
					&(cgi_data->host_time_field))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "servicetimefield")) {
			if((result = parse_enumeration_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_service_time_fields,
					&(cgi_data->service_time_field))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "commenttimefield")) {
			if((result = parse_enumeration_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_comment_time_fields,
					&(cgi_data->comment_time_field))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "downtimetimefield")) {
			if((result = parse_enumeration_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], svm_downtime_time_fields,
					&(cgi_data->downtime_time_field))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "starttime")) {
			if((result = parse_time_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->start_time)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "endtime")) {
			if((result = parse_time_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->end_time))) != RESULT_SUCCESS) {
				result = ERROR;
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], ""));

		else {
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, RESULT_OPTION_INVALID,
					"Invalid option: '%s'.", variables[x]));
			result = RESULT_OPTION_INVALID;
			break;
			}
		}

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return result;
	}

int validate_arguments(json_object *json_root, status_json_cgi_data *cgi_data,
		time_t query_time) {
	int result = RESULT_SUCCESS;
	host *temp_host = NULL;
	hostgroup *temp_hostgroup = NULL;
	servicegroup *temp_servicegroup = NULL;
	service *temp_service = NULL;
	contactgroup *temp_contactgroup = NULL;
	timeperiod *temp_timeperiod = NULL;
	command *temp_command = NULL;
	contact *temp_contact = NULL;
	nagios_comment *temp_comment = NULL;
	scheduled_downtime *temp_downtime = NULL;
	authdata *authinfo = NULL; /* Currently always NULL because
									get_authentication_information() hasn't
									been called yet, but in case we want to
									use it in the future... */

	/* Validate that required parameters were supplied */
	switch(cgi_data->query) {
	case STATUS_QUERY_HOSTCOUNT:
		break;
	case STATUS_QUERY_HOSTLIST:
		if(((cgi_data->start_time > 0) || (cgi_data->end_time > 0)) && 
				(STATUS_TIME_INVALID == cgi_data->host_time_field)) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Start and/or end time supplied, but time field specified."));
			}
		break;
	case STATUS_QUERY_HOST:
		if( NULL == cgi_data->host_name) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Host information requested, but no host name specified."));
			}
		break;
	case STATUS_QUERY_SERVICECOUNT:
		break;
	case STATUS_QUERY_SERVICELIST:
		if(((cgi_data->start_time > 0) || (cgi_data->end_time > 0)) && 
				(STATUS_TIME_INVALID == cgi_data->service_time_field)) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Start and/or end time supplied, but time field specified."));
			}
		break;
	case STATUS_QUERY_SERVICE:
		if( NULL == cgi_data->host_name) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Service information requested, but no host name specified."));
			}
		if( NULL == cgi_data->service_description) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Service information requested, but no service description specified."));
			}
		break;
	case STATUS_QUERY_COMMENTCOUNT:
		break;
	case STATUS_QUERY_COMMENTLIST:
		if(((cgi_data->start_time > 0) || (cgi_data->end_time > 0)) && 
				(STATUS_TIME_INVALID == cgi_data->comment_time_field)) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Start and/or end time supplied, but time field specified."));
			}
		break;
	case STATUS_QUERY_COMMENT:
		if( -1 == cgi_data->comment_id) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Comment information requested, but no id specified."));
			}
		break;
	case STATUS_QUERY_DOWNTIMECOUNT:
		break;
	case STATUS_QUERY_DOWNTIMELIST:
		if(((cgi_data->start_time > 0) || (cgi_data->end_time > 0)) && 
				(STATUS_TIME_INVALID == cgi_data->downtime_time_field)) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Start and/or end time supplied, but time field specified."));
			}
		break;
	case STATUS_QUERY_DOWNTIME:
		if( -1 == cgi_data->downtime_id) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Downtime information requested, but no id specified."));
			}
		break;
	case STATUS_QUERY_PROGRAMSTATUS:
		break;
	case STATUS_QUERY_PERFORMANCEDATA:
		break;
#if 0
	case STATUS_QUERY_CONTACTCOUNT:
		break;
	case STATUS_QUERY_CONTACTLIST:
		break;
	case STATUS_QUERY_CONTACT:
		if( NULL == cgi_data->contact_name) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Contact information requested, but no contact name specified."));
			}
		break;
#endif
	case STATUS_QUERY_HELP:
		break;
	default:
		result = RESULT_OPTION_MISSING;
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data->query, valid_queries), 
				get_query_status(query_status, cgi_data->query),
				(time_t)-1, authinfo, result, "Missing validation for object type %u.", 
				cgi_data->query));
		break;
		}

	/* Validate the requested parent host */
	if( NULL != cgi_data->parent_host_name) {
		cgi_data->use_parent_host = 1;
		cgi_data->parent_host = NULL;
		if(strcmp(cgi_data->parent_host_name, "none")) {
			temp_host = find_host(cgi_data->parent_host_name);
			if( NULL == temp_host) {
				cgi_data->use_parent_host = 0;
				result = RESULT_OPTION_VALUE_INVALID;
				json_object_append_object(json_root, "result", 
						json_result(query_time, THISCGI, 
						svm_get_string_from_value(cgi_data->query,
						valid_queries),
						get_query_status(query_status, cgi_data->query),
						(time_t)-1, authinfo, result,
						"The parenthost '%s' could not be found.", 
						cgi_data->parent_host_name));
				}
			else {
				cgi_data->parent_host = temp_host;
				}
			}
		}

	/* Validate the requested child host */
	if( NULL != cgi_data->child_host_name) {
		cgi_data->use_child_host = 1;
		cgi_data->child_host = NULL;
		if(strcmp(cgi_data->child_host_name, "none")) {
			temp_host = find_host(cgi_data->child_host_name);
			if( NULL == temp_host) {
				cgi_data->use_child_host = 0;
				result = RESULT_OPTION_VALUE_INVALID;
				json_object_append_object(json_root, "result", 
						json_result(query_time, THISCGI, 
						svm_get_string_from_value(cgi_data->query,
						valid_queries),
						get_query_status(query_status, cgi_data->query),
						(time_t)-1, authinfo, result,
						"The childhost '%s' could not be found.", 
						cgi_data->child_host_name));
				}
			else {
				cgi_data->child_host = temp_host;
				}
			}
		}

	/* Validate the requested host */
	if( NULL != cgi_data->host_name) {
		temp_host = find_host(cgi_data->host_name);
		if( NULL == temp_host) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The host '%s' could not be found.", cgi_data->host_name));
			}
		else {
			cgi_data->host = temp_host;
			}
		}

	/* Validate the requested hostgroup */
	if( NULL != cgi_data->hostgroup_name) {
		temp_hostgroup = find_hostgroup(cgi_data->hostgroup_name);
		if( NULL == temp_hostgroup) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The hostgroup '%s' could not be found.", 
					cgi_data->hostgroup_name));
			}
		else {
			cgi_data->hostgroup = temp_hostgroup;
			}
		}

	/* Validate the requested servicegroup */
	if( NULL != cgi_data->servicegroup_name) {
		temp_servicegroup = find_servicegroup(cgi_data->servicegroup_name);
		if( NULL == temp_servicegroup) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The servicegroup '%s' could not be found.", 
					cgi_data->servicegroup_name));
			}
		else {
			cgi_data->servicegroup = temp_servicegroup;
			}
		}

	/* Validate the requested contactgroup */
	if( NULL != cgi_data->contactgroup_name) {
		temp_contactgroup = find_contactgroup(cgi_data->contactgroup_name);
		if( NULL == temp_contactgroup) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The contactgroup '%s' could not be found.", 
					cgi_data->contactgroup_name));
			}
		else {
			cgi_data->contactgroup = temp_contactgroup;
			}
		}

	/* Validate the requested service. Because a service description may be
		requested without specifying a host name, it may not make sense
		to look for a specific service object */
	if((NULL != cgi_data->service_description) && 
			(NULL != cgi_data->host_name)) {
		temp_service = find_service(cgi_data->host_name, 
				cgi_data->service_description);
		if( NULL == temp_service) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The service '%s' on host '%s' could not be found.",
					cgi_data->service_description, cgi_data->host_name));
			}
		else {
			cgi_data->service = temp_service;
			}
		}

	/* Validate the requested check timeperiod */
	if( NULL != cgi_data->check_timeperiod_name) {
		temp_timeperiod = find_timeperiod(cgi_data->check_timeperiod_name);
		if( NULL == temp_timeperiod) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The check timeperiod '%s' could not be found.",
					cgi_data->check_timeperiod_name));
			}
		else {
			cgi_data->check_timeperiod = temp_timeperiod;
			}
		}

	/* Validate the requested host notification timeperiod */
	if( NULL != cgi_data->host_notification_timeperiod_name) {
		temp_timeperiod =
				find_timeperiod(cgi_data->host_notification_timeperiod_name);
		if( NULL == temp_timeperiod) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The host notification timeperiod '%s' could not be found.",
					cgi_data->host_notification_timeperiod_name));
			}
		else {
			cgi_data->host_notification_timeperiod = temp_timeperiod;
			}
		}

	/* Validate the requested service notification timeperiod */
	if( NULL != cgi_data->service_notification_timeperiod_name) {
		temp_timeperiod =
				find_timeperiod(cgi_data->service_notification_timeperiod_name);
		if( NULL == temp_timeperiod) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The service notification timeperiod '%s' could not be found.",
					cgi_data->service_notification_timeperiod_name));
			}
		else {
			cgi_data->service_notification_timeperiod = temp_timeperiod;
			}
		}

	/* Validate the requested check command */
	if( NULL != cgi_data->check_command_name) {
		temp_command = find_command(cgi_data->check_command_name);
		if( NULL == temp_command) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The check command '%s' could not be found.",
					cgi_data->check_command_name));
			}
		else {
			cgi_data->check_command = temp_command;
			}
		}

	/* Validate the requested event_handler */
	if( NULL != cgi_data->event_handler_name) {
		temp_command = find_command(cgi_data->event_handler_name);
		if( NULL == temp_command) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The event_handler '%s' could not be found.",
					cgi_data->event_handler_name));
			}
		else {
			cgi_data->event_handler = temp_command;
			}
		}

	/* Validate the requested comment */
	if(-1 != cgi_data->comment_id) {
		temp_comment = find_host_comment(cgi_data->comment_id);
		if(NULL == temp_comment) {
			temp_comment = find_service_comment(cgi_data->comment_id);
			}
		if(NULL == temp_comment) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The comment with ID '%d' could not be found.", 
					cgi_data->comment_id));
			}
		else {
			cgi_data->comment = temp_comment;
			}
		}

	/* Validate the requested downtime */
	if(-1 != cgi_data->downtime_id) {
		temp_downtime = find_downtime(ANY_DOWNTIME, cgi_data->downtime_id);
		if(NULL == temp_downtime) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The downtime with ID '%d' could not be found.", 
					cgi_data->downtime_id));
			}
		else {
			cgi_data->downtime = temp_downtime;
			}
		}

	/* Validate the requested triggered-by downtime */
	if(-1 != cgi_data->triggered_by) {
		temp_downtime = find_downtime(ANY_DOWNTIME, cgi_data->triggered_by);
		if(NULL == temp_downtime) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The triggering downtime with ID '%d' could not be found.", 
					cgi_data->triggered_by));
			}
		}

	/* Validate the requested contact */
	if( NULL != cgi_data->contact_name) {
		temp_contact = find_contact(cgi_data->contact_name);
		if( NULL == temp_contact) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result, "The contact '%s' could not be found.", 
					cgi_data->contact_name));
			}
		else {
			cgi_data->contact = temp_contact;
			}
		}

	/* Validate the requested start time is before the requested end time */
	if((cgi_data->start_time > 0) && (cgi_data->end_time > 0) && (
			cgi_data->start_time >= cgi_data->end_time)) {
		result = RESULT_OPTION_VALUE_INVALID;
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data->query, valid_queries), 
				get_query_status(query_status, cgi_data->query),
				(time_t)-1, authinfo, result,
				"The requested start time must be before the end time."));
		}

	return result;
	}

int json_status_host_passes_selection(host *temp_host, int use_parent_host,
		host *parent_host, int use_child_host, host *child_host, 
		hostgroup *temp_hostgroup, contact *temp_contact, 
		hoststatus *temp_hoststatus, int time_field, time_t start_time, 
		time_t end_time, contactgroup *temp_contactgroup,
		timeperiod *check_timeperiod, timeperiod *notification_timeperiod,
		command *check_command, command *event_handler) {

	host *temp_host2;

	/* Skip if user is not authorized for this host */
	if(FALSE == is_authorized_for_host(temp_host, &current_authdata)) {
		return 0;
	}

	/* If the host parent was specified, skip the hosts whose parent is 
		not the parent host specified */
	if( 1 == use_parent_host && 
			FALSE == is_host_immediate_child_of_host(parent_host, temp_host)) {
		return 0;
		}

	/* If the hostgroup was specified, skip the hosts that are not members
		of the hostgroup specified */
	if( NULL != temp_hostgroup && 
			( FALSE == is_host_member_of_hostgroup(temp_hostgroup, 
			temp_host))) {
		return 0;
		}

	/* If the contact was specified, skip the hosts that do not have
		the contact specified */
	if( NULL != temp_contact && 
			( FALSE == is_contact_for_host(temp_host, temp_contact))) {
		return 0;
		}

	/* If a contactgroup was specified, skip the hosts that do not have
		the contactgroup specified */
	if(NULL != temp_contactgroup &&
			(FALSE == is_contactgroup_for_host(temp_host, temp_contactgroup))) {
		return 0;
		}

	switch(time_field) {
	case STATUS_TIME_INVALID: 
		break;
	case STATUS_TIME_LAST_UPDATE:
		if((start_time > 0) && (temp_hoststatus->last_update < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_hoststatus->last_update > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_CHECK:
		if((start_time > 0) && (temp_hoststatus->last_check < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_hoststatus->last_check > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_NEXT_CHECK:
		if((start_time > 0) && (temp_hoststatus->next_check < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_hoststatus->next_check > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_STATE_CHANGE:
		if((start_time > 0) && (temp_hoststatus->last_state_change < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_hoststatus->last_state_change > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_HARD_STATE_CHANGE:
		if((start_time > 0) && (temp_hoststatus->last_hard_state_change < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_hoststatus->last_hard_state_change > 
				end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_TIME_UP:
		if((start_time > 0) && (temp_hoststatus->last_time_up < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_hoststatus->last_time_up > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_TIME_DOWN:
		if((start_time > 0) && (temp_hoststatus->last_time_down < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_hoststatus->last_time_down > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_TIME_UNREACHABLE:
		if((start_time > 0) && (temp_hoststatus->last_time_unreachable < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_hoststatus->last_time_unreachable > 
				end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_NOTIFICATION:
		if((start_time > 0) && (temp_hoststatus->last_notification < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_hoststatus->last_notification > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_NEXT_NOTIFICATION:
		if((start_time > 0) && (temp_hoststatus->next_notification < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_hoststatus->next_notification > end_time)) {
			return 0;
			}
		break;
	default:
		return 0;
		}

	/* If a check timeperiod was specified, skip this host if it does not have
		the check timeperiod specified */
	if(NULL != check_timeperiod &&
			(check_timeperiod != temp_host->check_period_ptr)) {
		return 0;
		}

	/* If a notification timeperiod was specified, skip this host if it
		does not have the notification timeperiod specified */
	if(NULL != notification_timeperiod &&
			(notification_timeperiod != temp_host->notification_period_ptr)) {
		return 0;
		}

	/* If a check command was specified, skip this host if it does not
		have the check command specified */
	if(NULL != check_command &&
			(check_command != temp_host->check_command_ptr)) {
		return 0;
		}

	/* If an event handler was specified, skip this host if it does not
		have the event handler specified */
	if(NULL != event_handler &&
			(event_handler != temp_host->event_handler_ptr)) {
		return 0;
		}

	/* If a child host was specified... (leave this for last since it is the
		most expensive check) */
	if(1 == use_child_host) { 
		/* If the child host is "none", skip this host if it has children */
		if(NULL == child_host) {
			for(temp_host2 = host_list; temp_host2 != NULL; 
					temp_host2 = temp_host2->next) {
				if(TRUE == is_host_immediate_child_of_host(temp_host, 
						temp_host2)) {
					return 0;
					}
				}
			}
		/* Otherwise, skip this host if it does not have the specified host
			as a child */
		else if(FALSE == is_host_immediate_child_of_host(temp_host, child_host)) {
			return 0;
			}
		}

	return 1;

	}

json_object *json_status_host_selectors(unsigned format_options, int start, 
		int count, int use_parent_host, host *parent_host, int use_child_host, 
		host *child_host, hostgroup *temp_hostgroup, int host_statuses, 
		contact *temp_contact, int time_field, time_t start_time, 
		time_t end_time, contactgroup *temp_contactgroup,
		timeperiod *check_timeperiod, timeperiod *notification_timeperiod,
		command *check_command, command *event_handler) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if(start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if(count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if(1 == use_parent_host) {
		json_object_append_string(json_selectors, "parenthost",
				&percent_escapes,
				( NULL == parent_host ? "none" : parent_host->name));
		}
	if(1 == use_child_host) {
		json_object_append_string(json_selectors, "childhost", &percent_escapes,
				( NULL == child_host ? "none" : child_host->name));
		}
	if(NULL != temp_hostgroup) {
		json_object_append_string(json_selectors, "hostgroup", &percent_escapes,
				temp_hostgroup->group_name);
		}
	if(HOST_STATUS_ALL != host_statuses) {
		json_bitmask(json_selectors, format_options, "hoststatus", 
				host_statuses, svm_host_statuses);
		}
	if(NULL != temp_contact) {
		json_object_append_string(json_selectors, "contact",&percent_escapes,
				temp_contact->name);
		}
	if(NULL != temp_contactgroup) {
		json_object_append_string(json_selectors, "contactgroup",
				&percent_escapes, temp_contactgroup->group_name);
		}
	if(time_field > 0) {
		json_enumeration(json_selectors, format_options, "hosttimefield", 
				time_field, svm_host_time_fields);
		}
	if(start_time > 0) {
		json_object_append_time_t(json_selectors, "starttime", start_time);
		}
	if(end_time > 0) {
		json_object_append_time_t(json_selectors, "endtime", end_time);
		}
	if( NULL != check_timeperiod) {
		json_object_append_string(json_selectors, "checktimeperiod",
				&percent_escapes, check_timeperiod->name);
		}
	if( NULL != notification_timeperiod) {
		json_object_append_string(json_selectors, "hostnotificationtimeperiod",
				&percent_escapes, notification_timeperiod->name);
		}
	if( NULL != check_command) {
		json_object_append_string(json_selectors, "checkcommand",
				&percent_escapes, check_command->name);
		}
	if( NULL != event_handler) {
		json_object_append_string(json_selectors, "eventhandler",
				&percent_escapes, event_handler->name);
		}

	return json_selectors;
	}

json_object *json_status_hostcount(unsigned format_options, int use_parent_host,
		host *parent_host, int use_child_host, host *child_host, 
		hostgroup *temp_hostgroup, int host_statuses, contact *temp_contact,
		int time_field, time_t start_time, time_t end_time,
		contactgroup *temp_contactgroup, timeperiod *check_timeperiod,
		timeperiod *notification_timeperiod, command *check_command,
		command *event_handler) {

	json_object *json_data;
	json_object *json_count;
	host *temp_host;
	hoststatus *temp_hoststatus;
	int up = 0;
	int down = 0;
	int unreachable = 0;
	int pending = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_status_host_selectors(format_options, 0, 0, use_parent_host, 
			parent_host, use_child_host, child_host, temp_hostgroup, 
			host_statuses, temp_contact, time_field, start_time, end_time,
			temp_contactgroup, check_timeperiod, notification_timeperiod,
			check_command, event_handler));

	json_count = json_new_object();

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* If we cannot get the status of the host, skip it. This should 
			probably return an error and doing so is in the todo list. */
		temp_hoststatus = find_hoststatus(temp_host->name);
		if( NULL == temp_hoststatus) {
			continue;
			}

		if(json_status_host_passes_selection(temp_host, use_parent_host, 
				parent_host, use_child_host, child_host, temp_hostgroup, 
				temp_contact, temp_hoststatus, time_field, start_time, 
				end_time, temp_contactgroup, check_timeperiod,
				notification_timeperiod, check_command, event_handler) == 0) {
			continue;
			}

		/* Count the hosts in each state */
		switch(temp_hoststatus->status) {
		case HOST_PENDING:
			pending++;
			break;
#ifdef JSON_NAGIOS_4X
		case SD_HOST_UP:
#else
		case HOST_UP:
#endif
			up++;
			break;
#ifdef JSON_NAGIOS_4X
		case SD_HOST_UNREACHABLE:
#else
		case HOST_UNREACHABLE:
#endif
			unreachable++;
			break;
#ifdef JSON_NAGIOS_4X
		case SD_HOST_DOWN:
#else
		case HOST_DOWN:
#endif
			down++;
			break;
			}
		}

#ifdef JSON_NAGIOS_4X
	if( host_statuses & SD_HOST_UP)
#else
	if( host_statuses & HOST_UP)
#endif
		json_object_append_integer(json_count, "up", up);
#ifdef JSON_NAGIOS_4X
	if( host_statuses & SD_HOST_DOWN)
#else
	if( host_statuses & HOST_DOWN)
#endif
		json_object_append_integer(json_count, "down", down);
#ifdef JSON_NAGIOS_4X
	if( host_statuses & SD_HOST_UNREACHABLE)
#else
	if( host_statuses & HOST_UNREACHABLE)
#endif
		json_object_append_integer(json_count, "unreachable", unreachable);
	if( host_statuses & HOST_PENDING)
		json_object_append_integer(json_count, "pending", pending);

	json_object_append_object(json_data, "count", json_count);

	return json_data;
	}

json_object *json_status_hostlist(unsigned format_options, int start, int count,
		int details, int use_parent_host, host *parent_host, int use_child_host,
		host *child_host, hostgroup *temp_hostgroup, int host_statuses, 
		contact *temp_contact, int time_field, time_t start_time, 
		time_t end_time, contactgroup *temp_contactgroup,
		timeperiod *check_timeperiod, timeperiod *notification_timeperiod,
		command *check_command, command *event_handler) {

	json_object *json_data;
	json_object *json_hostlist;
	json_object *json_host_details;
	host *temp_host;
	hoststatus *temp_hoststatus;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_status_host_selectors(format_options, start, count, 
			use_parent_host, parent_host, use_child_host, child_host, 
			temp_hostgroup, host_statuses, temp_contact, time_field, start_time,
			end_time, temp_contactgroup, check_timeperiod,
			notification_timeperiod, check_command, event_handler));

	json_hostlist = json_new_object();

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* If we cannot get the status of the host, skip it. This should 
			probably return an error and doing so is in the todo list. */
		temp_hoststatus = find_hoststatus(temp_host->name);
		if( NULL == temp_hoststatus) {
			continue;
			}

		if(json_status_host_passes_selection(temp_host, use_parent_host, 
				parent_host, use_child_host, child_host, temp_hostgroup, 
				temp_contact, temp_hoststatus, time_field, start_time, 
				end_time, temp_contactgroup, check_timeperiod,
				notification_timeperiod, check_command, event_handler) == 0) {
			continue;
			}

		/* If the status of the host does not match one of the status the
			user requested, skip the host */
		if(!(temp_hoststatus->status & host_statuses)) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			if( details > 0) {
				json_host_details = json_new_object();
				json_status_host_details(json_host_details, format_options, 
						temp_host, temp_hoststatus);
				json_object_append_object(json_hostlist, temp_host->name, 
						json_host_details);
				}
			else {
				json_enumeration(json_hostlist, format_options, temp_host->name,
						temp_hoststatus->status, svm_host_statuses);
				}
			counted++;
			}
		current++; 
		}

	json_object_append_object(json_data, "hostlist", json_hostlist);

	return json_data;
	}

json_object *json_status_host(unsigned format_options, host *temp_host, 
		hoststatus *temp_hoststatus) {

	json_object *json_host = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_string(json_details, "name", &percent_escapes,
			temp_host->name);
	json_status_host_details(json_details, format_options, temp_host, 
			temp_hoststatus);
	json_object_append_object(json_host, "host", json_details);

	return json_host;
	}

void json_status_host_details(json_object *json_details, unsigned format_options, 
		host *temp_host, hoststatus *temp_hoststatus) {

	json_object_append_string(json_details, "name", &percent_escapes,
			temp_host->name);
	json_object_append_string(json_details, "plugin_output", 
			&percent_escapes, temp_hoststatus->plugin_output);
	json_object_append_string(json_details, "long_plugin_output", 
			&percent_escapes, temp_hoststatus->long_plugin_output);
	json_object_append_string(json_details, "perf_data", 
			&percent_escapes, temp_hoststatus->perf_data);
	json_enumeration(json_details, format_options, "status", 
			temp_hoststatus->status, svm_host_statuses);
	json_object_append_time_t(json_details, "last_update", 
			temp_hoststatus->last_update);
	json_object_append_boolean(json_details, "has_been_checked", 
			temp_hoststatus->has_been_checked);
	json_object_append_boolean(json_details, "should_be_scheduled", 
			temp_hoststatus->should_be_scheduled);
	json_object_append_integer(json_details, "current_attempt", 
			temp_hoststatus->current_attempt);
	json_object_append_integer(json_details, "max_attempts", 
			temp_hoststatus->max_attempts);
	json_object_append_time_t(json_details, "last_check", 
			temp_hoststatus->last_check);
	json_object_append_time_t(json_details, "next_check", 
			temp_hoststatus->next_check);
	json_bitmask(json_details, format_options, "check_options",
			temp_hoststatus->check_options, svm_check_options);
	json_enumeration(json_details, format_options, "check_type",
			temp_hoststatus->check_type, svm_host_check_types);
	json_object_append_time_t(json_details, "last_state_change", 
			temp_hoststatus->last_state_change);
	json_object_append_time_t(json_details, "last_hard_state_change", 
			temp_hoststatus->last_hard_state_change);
	json_enumeration(json_details, format_options, "last_hard_state", 
			temp_hoststatus->last_hard_state, svm_host_states);
	json_object_append_time_t(json_details, "last_time_up", 
			temp_hoststatus->last_time_up);
	json_object_append_time_t(json_details, "last_time_down",
			temp_hoststatus->last_time_down);
	json_object_append_time_t(json_details, "last_time_unreachable", 
			temp_hoststatus->last_time_unreachable);
	json_enumeration(json_details, format_options, "state_type", 
			temp_hoststatus->state_type, svm_state_types);
	json_object_append_time_t(json_details, "last_notification", 
			temp_hoststatus->last_notification);
	json_object_append_time_t(json_details, "next_notification", 
			temp_hoststatus->next_notification);
	json_object_append_boolean(json_details, "no_more_notifications", 
			temp_hoststatus->no_more_notifications);
	json_object_append_boolean(json_details, "notifications_enabled", 
			temp_hoststatus->notifications_enabled);
	json_object_append_boolean(json_details, "problem_has_been_acknowledged", 
			temp_hoststatus->problem_has_been_acknowledged);
	json_enumeration(json_details, format_options, "acknowledgement_type", 
			temp_hoststatus->acknowledgement_type, svm_acknowledgement_types);
	json_object_append_integer(json_details, "current_notification_number", 
			temp_hoststatus->current_notification_number);
#ifdef JSON_NAGIOS_4X
	json_object_append_boolean(json_details, "accept_passive_checks", 
			temp_hoststatus->accept_passive_checks);
#else
	json_object_append_boolean(json_details, "accept_passive_host_checks", 
			temp_hoststatus->accept_passive_host_checks);
#endif
	json_object_append_boolean(json_details, "event_handler_enabled", 
			temp_hoststatus->event_handler_enabled);
	json_object_append_boolean(json_details, "checks_enabled", 
			temp_hoststatus->checks_enabled);
	json_object_append_boolean(json_details, "flap_detection_enabled", 
			temp_hoststatus->flap_detection_enabled);
	json_object_append_boolean(json_details, "is_flapping", 
			temp_hoststatus->is_flapping);
	json_object_append_real(json_details, "percent_state_change", 
			temp_hoststatus->percent_state_change);
	json_object_append_real(json_details, "latency", temp_hoststatus->latency);
	json_object_append_real(json_details, "execution_time", 
			temp_hoststatus->execution_time);
	json_object_append_integer(json_details, "scheduled_downtime_depth", 
			temp_hoststatus->scheduled_downtime_depth);
#ifndef JSON_NAGIOS_4X
	json_object_append_boolean(json_details, "failure_prediction_enabled", 
			temp_hoststatus->failure_prediction_enabled);
#endif
	json_object_append_boolean(json_details, "process_performance_data", 
			temp_hoststatus->process_performance_data);
#ifdef JSON_NAGIOS_4X
	json_object_append_boolean(json_details, "obsess", temp_hoststatus->obsess);
#else
	json_object_append_boolean(json_details, "obsess_over_host", 
			temp_hoststatus->obsess_over_host);
#endif
	}

int json_status_service_passes_host_selection(host *temp_host, 
		int use_parent_host, host *parent_host, int use_child_host, 
		host *child_host, hostgroup *temp_hostgroup, host *match_host, 
		int host_statuses) {

	hoststatus *temp_hoststatus;
	host *temp_host2;

	/* Skip if user is not authorized for this service */
	if(FALSE == is_authorized_for_host(temp_host, &current_authdata)) {
		return 0;
	}

	/* If the host parent was specified, skip the services whose host is 
		not a child of the parent host specified */
	if( 1 == use_parent_host && NULL != temp_host && 
			FALSE == is_host_immediate_child_of_host(parent_host, 
			temp_host)) {
		return 0;
		}

	/* If the hostgroup was specified, skip the services on hosts that 
		are not members of the hostgroup specified */
	if( NULL != temp_hostgroup && NULL != temp_host &&
			( FALSE == is_host_member_of_hostgroup(temp_hostgroup, 
					temp_host))) {
		return 0;
		}

	/* If the host was specified, skip the services not on the host
		specified */
	if( NULL != match_host && NULL != temp_host &&
			temp_host != match_host) {
		return 0;
		}

	/* If we cannot get the status of the host, skip it. This should 
		probably return an error and doing so is in the todo list. */
	temp_hoststatus = find_hoststatus(temp_host->name);
	if( NULL == temp_hoststatus) {
		return 0;
		}

	/* If a child host was specified... */
	if(1 == use_child_host) { 
		/* If the child host is "none", skip this host if it has children */
		if(NULL == child_host) {
			for(temp_host2 = host_list; temp_host2 != NULL; 
					temp_host2 = temp_host2->next) {
				if(TRUE == is_host_immediate_child_of_host(temp_host, 
						temp_host2)) {
					return 0;
					}
				}
			}
		/* Otherwise, skip this host if it does not have the specified host
			as a child */
		else if(FALSE == is_host_immediate_child_of_host(temp_host, child_host)) {
			return 0;
			}
		}

	return 1;
	}

int json_status_service_passes_service_selection(service *temp_service, 
		servicegroup *temp_servicegroup, contact *temp_contact, 
		servicestatus *temp_servicestatus, int time_field, time_t start_time, 
		time_t end_time, char *service_description, char *parent_service_name,
		char *child_service_name, contactgroup *temp_contactgroup,
		timeperiod *check_timeperiod, timeperiod *notification_timeperiod,
		command *check_command, command *event_handler) {

	servicesmember *temp_servicesmember;

	/* Skip if user is not authorized for this service */
	if(FALSE == is_authorized_for_service(temp_service, 
			&current_authdata)) {
		return 0;
	}

	/* If the servicegroup was specified, skip the services that are not 
		members of the servicegroup specified */
	if( NULL != temp_servicegroup && 
			( FALSE == is_service_member_of_servicegroup(temp_servicegroup, 
			temp_service))) {
		return 0;
		}

	/* If the contact was specified, skip the services that do not have
		the contact specified */
	if( NULL != temp_contact && 
			( FALSE == is_contact_for_service(temp_service, temp_contact))) {
		return 0;
		}

	/* If a contactgroup was specified, skip the services that do not have
		the contactgroup specified */
	if(NULL != temp_contactgroup &&
			(FALSE == is_contactgroup_for_service(temp_service,
			temp_contactgroup))) {
		return 0;
		}

	switch(time_field) {
	case STATUS_TIME_INVALID: 
		break;
	case STATUS_TIME_LAST_UPDATE:
		if((start_time > 0) && (temp_servicestatus->last_update < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_servicestatus->last_update > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_CHECK:
		if((start_time > 0) && (temp_servicestatus->last_check < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_servicestatus->last_check > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_NEXT_CHECK:
		if((start_time > 0) && (temp_servicestatus->next_check < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_servicestatus->next_check > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_STATE_CHANGE:
		if((start_time > 0) && (temp_servicestatus->last_state_change < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_servicestatus->last_state_change > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_HARD_STATE_CHANGE:
		if((start_time > 0) && (temp_servicestatus->last_hard_state_change < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_servicestatus->last_hard_state_change > 
				end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_TIME_OK:
		if((start_time > 0) && (temp_servicestatus->last_time_ok < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_servicestatus->last_time_ok > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_TIME_WARNING:
		if((start_time > 0) && (temp_servicestatus->last_time_warning < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_servicestatus->last_time_warning > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_TIME_CRITICAL:
		if((start_time > 0) && (temp_servicestatus->last_time_critical < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_servicestatus->last_time_critical > 
				end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_TIME_UNKNOWN:
		if((start_time > 0) && (temp_servicestatus->last_time_unknown < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_servicestatus->last_time_unknown > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_LAST_NOTIFICATION:
		if((start_time > 0) && (temp_servicestatus->last_notification < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_servicestatus->last_notification > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_NEXT_NOTIFICATION:
		if((start_time > 0) && (temp_servicestatus->next_notification < 
				start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_servicestatus->next_notification > end_time)) {
			return 0;
			}
		break;
	default:
		return 0;
		}

	/* If a service description was specified, skip the services that do not 
		have the service description specified */
	if(NULL != service_description && 
			strcmp(temp_service->description, service_description)) {
		return 0;
		}

	/* If a check timeperiod was specified, skip this service if it does
		not have the check timeperiod specified */
	if(NULL != check_timeperiod &&
			(check_timeperiod != temp_service->check_period_ptr)) {
		return 0;
		}

	/* If a notification timeperiod was specified, skip this service if it does
		not have the notification timeperiod specified */
	if(NULL != notification_timeperiod && (notification_timeperiod !=
			temp_service->notification_period_ptr)) {
		return 0;
		}

	/* If a check command was specified, skip this service if it does not
		have the check command specified */
	if(NULL != check_command &&
			(check_command != temp_service->check_command_ptr)) {
		return 0;
		}

	/* If a event handler was specified, skip this service if it does not
		have the event handler specified */
	if(NULL != event_handler &&
			(event_handler != temp_service->event_handler_ptr)) {
		return 0;
		}

	/* If a parent service was specified... */
	if(NULL != parent_service_name) {
		/* If the parent service is "none", skip this service if it has
			parentren */
		if(!strcmp(parent_service_name,"none")) {
			if(NULL != temp_service->parents) {
				return 0;
				}
			}
		/* Otherwise, skip this service if it does not have the specified
			service as a parent */
		else {
			int found = 0;
			for(temp_servicesmember = temp_service->parents;
					temp_servicesmember != NULL;
					temp_servicesmember = temp_servicesmember->next) {
				if(!strcmp(temp_servicesmember->service_description,
						parent_service_name)) {
					found = 1;
					}
				}
			if(0 == found) {
				return 0;
				}
			}
		}

	/* If a child service was specified... */
	if(NULL != child_service_name) {
		/* If the child service is "none", skip this service if it has
			children */
		if(!strcmp(child_service_name,"none")) {
			if(NULL != temp_service->children) {
				return 0;
				}
			}
		/* Otherwise, skip this service if it does not have the specified
			service as a child */
		else {
			int found = 0;
			for(temp_servicesmember = temp_service->children;
					temp_servicesmember != NULL;
					temp_servicesmember = temp_servicesmember->next) {
				if(!strcmp(temp_servicesmember->service_description,
						child_service_name)) {
					found = 1;
					}
				}
			if(0 == found) {
				return 0;
				}
			}
		}

	return 1;
	}

json_object *json_status_service_selectors(unsigned format_options, 
		int start, int count, int use_parent_host, host *parent_host, 
		int use_child_host, host *child_host, hostgroup *temp_hostgroup, 
		host *match_host, servicegroup *temp_servicegroup, int host_statuses, 
		int service_statuses, contact *temp_contact, int time_field, 
		time_t start_time, time_t end_time, char *service_description,
		char *parent_service_name, char *child_service_name,
		contactgroup *temp_contactgroup, timeperiod *check_timeperiod,
		timeperiod *notification_timeperiod, command *check_command,
		command *event_handler) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if( 1 == use_parent_host) {
		json_object_append_string(json_selectors, "parenthost",
				&percent_escapes,
				( NULL == parent_host ? "none" : parent_host->name));
		}
	if( 1 == use_child_host) {
		json_object_append_string(json_selectors, "childhost", &percent_escapes,
				( NULL == child_host ? "none" : child_host->name));
		}
	if( NULL != temp_hostgroup) {
		json_object_append_string(json_selectors, "hostgroup", &percent_escapes,
				temp_hostgroup->group_name);
		}
	if( NULL != temp_servicegroup) {
		json_object_append_string(json_selectors, "servicegroup",
				&percent_escapes, temp_servicegroup->group_name);
		}
	if(HOST_STATUS_ALL != host_statuses) {
		json_bitmask(json_selectors, format_options, "host_status", 
				host_statuses, svm_host_statuses);
		}
	if(SERVICE_STATUS_ALL != service_statuses) {
		json_bitmask(json_selectors, format_options, "service_status", 
				service_statuses, svm_service_statuses);
		}
	if(NULL != temp_contact) {
		json_object_append_string(json_selectors, "contact",&percent_escapes,
				temp_contact->name);
		}
	if(NULL != temp_contactgroup) {
		json_object_append_string(json_selectors, "contactgroup",
				&percent_escapes, temp_contactgroup->group_name);
		}
	if(time_field > 0) {
		json_enumeration(json_selectors, format_options, "servicetimefield", 
				time_field, svm_service_time_fields);
		}
	if(start_time > 0) {
		json_object_append_time_t(json_selectors, "starttime", start_time);
		}
	if(end_time > 0) {
		json_object_append_time_t(json_selectors, "endtime", end_time);
		}
	if(NULL != service_description) {
		json_object_append_string(json_selectors, "servicedescription",
				&percent_escapes, service_description);
		}
	if( NULL != parent_service_name) {
		json_object_append_string(json_selectors, "parentservice",
				&percent_escapes, parent_service_name);
		}
	if( NULL != child_service_name) {
		json_object_append_string(json_selectors, "childservice",
				&percent_escapes, child_service_name);
		}
	if( NULL != check_timeperiod) {
		json_object_append_string(json_selectors, "checktimeperiod",
				&percent_escapes, check_timeperiod->name);
		}
	if( NULL != notification_timeperiod) {
		json_object_append_string(json_selectors,
				"servicenotificationtimeperiod", &percent_escapes,
				notification_timeperiod->name);
		}
	if( NULL != check_command) {
		json_object_append_string(json_selectors, "checkcommand",
				&percent_escapes, check_command->name);
		}
	if( NULL != event_handler) {
		json_object_append_string(json_selectors, "eventhandler",
				&percent_escapes, event_handler->name);
		}

	return json_selectors;
	}

json_object *json_status_servicecount(unsigned format_options, host *match_host,
		int use_parent_host, host *parent_host, int use_child_host, 
		host *child_host, hostgroup *temp_hostgroup, 
		servicegroup *temp_servicegroup, int host_statuses, 
		int service_statuses, contact *temp_contact, int time_field, 
		time_t start_time, time_t end_time, char *service_description,
		char *parent_service_name, char *child_service_name,
		contactgroup *temp_contactgroup, timeperiod *check_timeperiod,
		timeperiod *notification_timeperiod, command *check_command,
		command *event_handler) {

	json_object *json_data;
	json_object *json_count;
	host *temp_host;
	service *temp_service;
	servicestatus *temp_servicestatus;
	int ok = 0;
	int warning = 0;
	int critical = 0;
	int unknown = 0;
	int pending = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_status_service_selectors(format_options, 0, 0, use_parent_host,
			parent_host, use_child_host, child_host, temp_hostgroup, match_host,
			temp_servicegroup, host_statuses, service_statuses, temp_contact, 
			time_field, start_time, end_time, service_description,
			parent_service_name, child_service_name, temp_contactgroup,
			check_timeperiod, notification_timeperiod, check_command,
			event_handler));

	json_count = json_new_object();

	for(temp_service = service_list; temp_service != NULL; 
			temp_service = temp_service->next) {

		temp_host = find_host(temp_service->host_name);
		if(NULL == temp_host) {
			continue;
			}

		if(!json_status_service_passes_host_selection(temp_host, 
				use_parent_host, parent_host, use_child_host, child_host, 
				temp_hostgroup, match_host, host_statuses)) {
			continue;
			}

		/* Get the service status. If we cannot get the status of 
			the service, skip it. This should probably return an 
			error and doing so is in the todo list. */
		temp_servicestatus = find_servicestatus(temp_service->host_name, 
				temp_service->description);
		if( NULL == temp_servicestatus) {
			continue;
			}

		if(!json_status_service_passes_service_selection(temp_service, 
				temp_servicegroup, temp_contact, temp_servicestatus, 
				time_field, start_time, end_time, service_description,
				parent_service_name, child_service_name, temp_contactgroup,
				check_timeperiod, notification_timeperiod, check_command,
				event_handler)) {
			continue;
			}

		/* Count the services in each state */
		switch(temp_servicestatus->status) {
		case SERVICE_PENDING:
			pending++;
			break;
		case SERVICE_OK:
			ok++;
			break;
		case SERVICE_WARNING:
			warning++;
			break;
		case SERVICE_CRITICAL:
			critical++;
			break;
		case SERVICE_UNKNOWN:
			unknown++;
			break;
			}
		}

	if( service_statuses & SERVICE_OK)
		json_object_append_integer(json_count, "ok", ok);
	if( service_statuses & SERVICE_WARNING)
		json_object_append_integer(json_count, "warning", warning);
	if( service_statuses & SERVICE_CRITICAL)
		json_object_append_integer(json_count, "critical", critical);
	if( service_statuses & SERVICE_UNKNOWN)
		json_object_append_integer(json_count, "unknown", unknown);
	if( service_statuses & SERVICE_PENDING)
		json_object_append_integer(json_count, "pending", pending);

	json_object_append_object(json_data, "count", json_count);

	return json_data;
	}

json_object *json_status_servicelist(unsigned format_options, int start, 
		int count, int details, host *match_host, int use_parent_host, 
		host *parent_host, int use_child_host, host *child_host, 
		hostgroup *temp_hostgroup, servicegroup *temp_servicegroup, 
		int host_statuses, int service_statuses, contact *temp_contact,
		int time_field, time_t start_time, time_t end_time, 
		char *service_description, char *parent_service_name,
		char *child_service_name, contactgroup *temp_contactgroup,
		timeperiod *check_timeperiod, timeperiod *notification_timeperiod,
		command *check_command, command *event_handler) {

	json_object *json_data;
	json_object *json_hostlist;
	json_object *json_servicelist;
	json_object *json_service_details;
	host *temp_host;
	service *temp_service;
	servicestatus *temp_servicestatus;
	int current = 0;
	int counted = 0;
	int service_count; /* number of services on a host */

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_status_service_selectors(format_options, start, count, 
			use_parent_host, parent_host, use_child_host, child_host, 
			temp_hostgroup, match_host, temp_servicegroup, host_statuses, 
			service_statuses, temp_contact, time_field, start_time, end_time, 
			service_description, parent_service_name, child_service_name,
			temp_contactgroup, check_timeperiod, notification_timeperiod,
			check_command, event_handler));

	json_hostlist = json_new_object();

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		if(json_status_service_passes_host_selection(temp_host, 
				use_parent_host, parent_host, use_child_host, child_host, 
				temp_hostgroup, match_host, host_statuses) == 0) {
			continue;
			}

		json_servicelist = json_new_object();

		service_count = 0;

		for(temp_service = service_list; temp_service != NULL; 
				temp_service = temp_service->next) {

			/* If this service isn't on the host we're currently working with, 
					skip it */
			if( strcmp( temp_host->name, temp_service->host_name)) continue;

			/* Get the service status. If we cannot get the status of 
				the service, skip it. This should probably return an 
				error and doing so is in the todo list. */
			temp_servicestatus = find_servicestatus(temp_service->host_name, 
					temp_service->description);
			if( NULL == temp_servicestatus) {
				continue;
				}

			if(json_status_service_passes_service_selection(temp_service, 
					temp_servicegroup, temp_contact, temp_servicestatus, 
					time_field, start_time, end_time, 
					service_description, parent_service_name,
					child_service_name, temp_contactgroup,
					check_timeperiod, notification_timeperiod,
					check_command, event_handler) == 0) {
				continue;
				}

			/* If the status of the service does not match one of the status the
				user requested, skip the service */
			if(!(temp_servicestatus->status & service_statuses)) {
				continue;
				}

			service_count++;

			/* If the current item passes the start and limit tests, display it */
			if( passes_start_and_count_limits(start, count, current, counted)) {
				if( details > 0) {
					json_service_details = json_new_object();
					json_status_service_details(json_service_details, 
							format_options, 
							temp_service, temp_servicestatus);
					json_object_append_object(json_servicelist, 
							temp_service->description, 
							json_service_details);
					}
				else {
					json_enumeration(json_servicelist, format_options, 
							temp_service->description,
							temp_servicestatus->status, 
							svm_service_statuses);
					}
				counted++;
				}
			current++; 
			}

		if( service_count > 0) {
			json_object_append_object(json_hostlist, temp_host->name, 
					json_servicelist);
			}
		}

	json_object_append_object(json_data, "servicelist", json_hostlist);

	return json_data;
	}

json_object *json_status_service(unsigned format_options, service *temp_service, 
		servicestatus *temp_servicestatus) {

	json_object *json_service = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_string(json_details, "host_name", &percent_escapes,
			temp_service->host_name);
	json_object_append_string(json_details, "description", &percent_escapes,
			temp_service->description);
	json_status_service_details(json_details, format_options, temp_service, 
			temp_servicestatus);
	json_object_append_object(json_service, "service", json_details);

	return json_service;
	}

void json_status_service_details(json_object *json_details, 
		unsigned format_options, service *temp_service, 
		servicestatus *temp_servicestatus) {

	json_object_append_string(json_details, "host_name", &percent_escapes,
			temp_service->host_name);
	json_object_append_string(json_details, "description", &percent_escapes,
			temp_service->description);
	json_object_append_string(json_details, "plugin_output", &percent_escapes,
			temp_servicestatus->plugin_output);
	json_object_append_string(json_details, "long_plugin_output",
			&percent_escapes, temp_servicestatus->long_plugin_output);
	json_object_append_string(json_details, "perf_data", &percent_escapes,
			temp_servicestatus->perf_data);
	json_object_append_integer(json_details, "max_attempts", 
			temp_servicestatus->max_attempts);
	json_object_append_integer(json_details, "current_attempt", 
			temp_servicestatus->current_attempt);
	json_enumeration(json_details, format_options, "status", 
			temp_servicestatus->status, svm_service_statuses);
	json_object_append_time_t(json_details, "last_update", 
			temp_servicestatus->last_update);
	json_object_append_boolean(json_details, "has_been_checked", 
			temp_servicestatus->has_been_checked);
	json_object_append_boolean(json_details, "should_be_scheduled", 
			temp_servicestatus->should_be_scheduled);
	json_object_append_time_t(json_details, "last_check", 
			temp_servicestatus->last_check);
	json_bitmask(json_details, format_options, "check_options",
			temp_servicestatus->check_options, svm_check_options);
	json_enumeration(json_details, format_options, "check_type",
			temp_servicestatus->check_type, svm_service_check_types);
	json_object_append_boolean(json_details, "checks_enabled", 
			temp_servicestatus->checks_enabled);
	json_object_append_time_t(json_details, "last_state_change", 
			temp_servicestatus->last_state_change);
	json_object_append_time_t(json_details, "last_hard_state_change", 
			temp_servicestatus->last_hard_state_change);
	json_enumeration(json_details, format_options, "last_hard_state", 
			temp_servicestatus->last_hard_state, svm_service_states);
	json_object_append_time_t(json_details, "last_time_ok", 
			temp_servicestatus->last_time_ok);
	json_object_append_time_t(json_details, "last_time_warning", 
			temp_servicestatus->last_time_warning);
	json_object_append_time_t(json_details, "last_time_unknown", 
			temp_servicestatus->last_time_unknown);
	json_object_append_time_t(json_details, "last_time_critical", 
			temp_servicestatus->last_time_critical);
	json_enumeration(json_details, format_options, "state_type", 
			temp_servicestatus->state_type, svm_state_types);
	json_object_append_time_t(json_details, "last_notification", 
			temp_servicestatus->last_notification);
	json_object_append_time_t(json_details, "next_notification", 
			temp_servicestatus->next_notification);
	json_object_append_time_t(json_details, "next_check", 
			temp_servicestatus->next_check);
	json_object_append_boolean(json_details, "no_more_notifications", 
			temp_servicestatus->no_more_notifications);
	json_object_append_boolean(json_details, "notifications_enabled", 
			temp_servicestatus->notifications_enabled);
	json_object_append_boolean(json_details, "problem_has_been_acknowledged", 
			temp_servicestatus->problem_has_been_acknowledged);
	json_enumeration(json_details, format_options, "acknowledgement_type", 
			temp_servicestatus->acknowledgement_type, 
			svm_acknowledgement_types);
	json_object_append_integer(json_details, "current_notification_number", 
			temp_servicestatus->current_notification_number);
#ifdef JSON_NAGIOS_4X
	json_object_append_boolean(json_details, "accept_passive_checks", 
			temp_servicestatus->accept_passive_checks);
#else
	json_object_append_boolean(json_details, "accept_passive_service_checks", 
			temp_servicestatus->accept_passive_service_checks);
#endif
	json_object_append_boolean(json_details, "event_handler_enabled", 
			temp_servicestatus->event_handler_enabled);
	json_object_append_boolean(json_details, "flap_detection_enabled", 
			temp_servicestatus->flap_detection_enabled);
	json_object_append_boolean(json_details, "is_flapping", 
			temp_servicestatus->is_flapping);
	json_object_append_real(json_details, "percent_state_change", 
			temp_servicestatus->percent_state_change);
	json_object_append_real(json_details, "latency", temp_servicestatus->latency);
	json_object_append_real(json_details, "execution_time", 
			temp_servicestatus->execution_time);
	json_object_append_integer(json_details, "scheduled_downtime_depth", 
			temp_servicestatus->scheduled_downtime_depth);
#ifndef JSON_NAGIOS_4X
	json_object_append_boolean(json_details, "failure_prediction_enabled", 
			temp_servicestatus->failure_prediction_enabled);
#endif
	json_object_append_boolean(json_details, "process_performance_data", 
			temp_servicestatus->process_performance_data);
#ifdef JSON_NAGIOS_4X
	json_object_append_boolean(json_details, "obsess", 
			temp_servicestatus->obsess);
#else
	json_object_append_boolean(json_details, "obsess_over_service", 
			temp_servicestatus->obsess_over_service);
#endif
	}

int json_status_comment_passes_selection(nagios_comment *temp_comment,
		int time_field, time_t start_time, time_t end_time,
		unsigned comment_types, unsigned entry_types, unsigned persistence,
		unsigned expiring, char *host_name, char *service_description) {

	switch(time_field) {
	case STATUS_TIME_INVALID: 
		break;
	case STATUS_TIME_ENTRY_TIME:
		if((start_time > 0) && (temp_comment->entry_time < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_comment->entry_time > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_EXPIRE_TIME:
		if((start_time > 0) && (temp_comment->expire_time < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_comment->expire_time > end_time)) {
			return 0;
			}
		break;
	default:
		return 0;
		}

	if(comment_types != COMMENT_TYPE_ALL) {
		switch(temp_comment->comment_type) {
		case HOST_COMMENT:
			if(!(comment_types & COMMENT_TYPE_HOST)) {
				return 0;
				}
			break;
		case SERVICE_COMMENT:
			if(!(comment_types & COMMENT_TYPE_SERVICE)) {
				return 0;
				}
			break;
			}
		}

	if(entry_types != COMMENT_ENTRY_ALL) {
		switch(temp_comment->entry_type) {
		case USER_COMMENT:
			if(!(entry_types & COMMENT_ENTRY_USER)) {
				return 0;
				}
			break;
		case DOWNTIME_COMMENT:
			if(!(entry_types & COMMENT_ENTRY_DOWNTIME)) {
				return 0;
				}
			break;
		case FLAPPING_COMMENT:
			if(!(entry_types & COMMENT_ENTRY_FLAPPING)) {
				return 0;
				}
			break;
		case ACKNOWLEDGEMENT_COMMENT:
			if(!(entry_types & COMMENT_ENTRY_ACKNOWLEDGEMENT)) {
				return 0;
				}
			break;
			}
		}

	if(persistence != BOOLEAN_EITHER) {
		switch(temp_comment->persistent) {
		case 0: /* false */
			if(!(persistence & BOOLEAN_FALSE)) {
				return 0;
				}
			break;
		case 1: /* true */
			if(!(persistence & BOOLEAN_TRUE)) {
				return 0;
				}
			break;
			}
		}

	if(expiring != BOOLEAN_EITHER) {
		switch(temp_comment->expires) {
		case 0: /* false */
			if(!(expiring & BOOLEAN_FALSE)) {
				return 0;
				}
			break;
		case 1: /* true */
			if(!(expiring & BOOLEAN_TRUE)) {
				return 0;
				}
			break;
			}
		}

	if(NULL != host_name) {
		if((NULL == temp_comment->host_name) ||
				strcmp(temp_comment->host_name, host_name)) {
			return 0;
			}
		}

	if(NULL != service_description) {
		if((NULL == temp_comment->service_description) ||
				strcmp(temp_comment->service_description,
				service_description)) {
			return 0;
			}
		}

	return 1;
	}

json_object *json_status_comment_selectors(unsigned format_options, int start, 
		int count, int time_field, time_t start_time, time_t end_time,
		unsigned comment_types, unsigned entry_types, unsigned persistence,
		unsigned expiring, char *host_name, char *service_description) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if(time_field > 0) {
		json_enumeration(json_selectors, format_options, "commenttimefield", 
				time_field, svm_comment_time_fields);
		}
	if(start_time > 0) {
		json_object_append_time_t(json_selectors, "starttime", start_time);
		}
	if(end_time > 0) {
		json_object_append_time_t(json_selectors, "endtime", end_time);
		}
	if(comment_types != COMMENT_TYPE_ALL) {
		json_bitmask(json_selectors, format_options, "commenttypes", 
				comment_types, svm_valid_comment_types);
		}
	if(entry_types != COMMENT_ENTRY_ALL) {
		json_bitmask(json_selectors, format_options, "entrytypes", 
				entry_types, svm_valid_comment_entry_types);
		}
	if(persistence != BOOLEAN_EITHER) {
		json_bitmask(json_selectors, format_options, "persistence", 
				persistence, svm_valid_persistence);
		}
	if(expiring != BOOLEAN_EITHER) {
		json_bitmask(json_selectors, format_options, "expiring", 
				expiring, svm_valid_expiration);
		}
	if(NULL != host_name) {
		json_object_append_string(json_selectors, "hostname", &percent_escapes,
				host_name);
		}
	if(NULL != service_description) {
		json_object_append_string(json_selectors, "servicedescription", 
				&percent_escapes, service_description);
		}

	return json_selectors;
	}

json_object *json_status_commentcount(unsigned format_options, int time_field, 
		time_t start_time, time_t end_time, unsigned comment_types,
		unsigned entry_types, unsigned persistence, unsigned expiring,
		char *host_name, char *service_description) {

	json_object *json_data;
	nagios_comment *temp_comment;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_status_comment_selectors(format_options, 0, 0, time_field, 
			start_time, end_time, comment_types, entry_types, persistence,
			expiring, host_name, service_description));

	for(temp_comment = comment_list; temp_comment != NULL; 
			temp_comment = temp_comment->next) {
		if(json_status_comment_passes_selection(temp_comment, time_field, 
				start_time, end_time, comment_types, entry_types,
				persistence, expiring, host_name, service_description) == 0) {
			continue;
			}
		count++;
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_status_commentlist(unsigned format_options, int start, 
		int count, int details, int time_field, time_t start_time, 
		time_t end_time, unsigned comment_types, unsigned entry_types,
		unsigned persistence, unsigned expiring, char *host_name,
		char *service_description) {

	json_object *json_data;
	json_object *json_commentlist_object = NULL;
	json_object *json_commentlist_array = NULL;
	json_object *json_comment_details;
	nagios_comment *temp_comment;
	int current = 0;
	int counted = 0;
	char *buf;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_status_comment_selectors(format_options, start, count, 
			time_field, start_time, end_time, comment_types, entry_types,
			persistence, expiring, host_name, service_description));

	if(details > 0) {
		json_commentlist_object = json_new_object();
		}
	else {
		json_commentlist_array = json_new_array();
		}

	for(temp_comment = comment_list; temp_comment != NULL; 
			temp_comment = temp_comment->next) {

		if(json_status_comment_passes_selection(temp_comment, time_field, 
				start_time, end_time, comment_types, entry_types,
				persistence, expiring, host_name, service_description) == 0) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			if( details > 0) {
				asprintf(&buf, "%lu", temp_comment->comment_id);
				json_comment_details = json_new_object();
				json_status_comment_details(json_comment_details, format_options, 
						temp_comment);
				json_object_append_object(json_commentlist_object, buf,
						json_comment_details);
				}
			else {
				json_array_append_integer(json_commentlist_array, 
						temp_comment->comment_id); 
				}
			counted++;
			}
		current++; 
		}

	if(details > 0) {
		json_object_append_object(json_data, "commentlist", 
				json_commentlist_object);
		}
	else {
		json_object_append_array(json_data, "commentlist", 
				json_commentlist_array);
		}

	return json_data;
	}

json_object *json_status_comment(unsigned format_options,
		nagios_comment *temp_comment) {

	json_object *json_comment = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_integer(json_details, "comment_id", 
			temp_comment->comment_id);
	json_status_comment_details(json_details, format_options, temp_comment); 
	json_object_append_object(json_comment, "comment", json_details);

	return json_comment;
	}

void json_status_comment_details(json_object *json_details, 
		unsigned format_options, nagios_comment *temp_comment) {

	json_object_append_integer(json_details, "comment_id", 
			temp_comment->comment_id);
	json_enumeration(json_details, format_options, "comment_type",
			temp_comment->comment_type, svm_comment_types);
	json_enumeration(json_details, format_options, "entry_type",
			temp_comment->entry_type, svm_comment_entry_types);
	json_object_append_integer(json_details,  "source", temp_comment->source);
	json_object_append_boolean(json_details, "persistent", 
			temp_comment->persistent);
	json_object_append_time_t(json_details, "entry_time", 
			temp_comment->entry_time);
	json_object_append_boolean(json_details, "expires", temp_comment->expires);
	json_object_append_time_t(json_details, "expire_time", 
			temp_comment->expire_time);
	json_object_append_string(json_details, "host_name", &percent_escapes,
			temp_comment->host_name);
	if(SERVICE_COMMENT == temp_comment->comment_type) {
		json_object_append_string(json_details,  "service_description", 
				&percent_escapes, temp_comment->service_description);
		}
	json_object_append_string(json_details, "author", &percent_escapes,
			temp_comment->author);
	json_object_append_string(json_details, "comment_data", &percent_escapes,
			temp_comment->comment_data);
	}

int json_status_downtime_passes_selection(scheduled_downtime *temp_downtime, 
		int time_field, time_t start_time, time_t end_time, 
		unsigned object_types, unsigned downtime_types, unsigned triggered,
		int triggered_by, unsigned in_effect, char *host_name,
		char *service_description) {

	switch(time_field) {
	case STATUS_TIME_INVALID: 
		break;
	case STATUS_TIME_ENTRY_TIME:
		if((start_time > 0) && (temp_downtime->entry_time < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_downtime->entry_time > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_START_TIME:
		if((start_time > 0) && (temp_downtime->start_time < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_downtime->start_time > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_FLEX_DOWNTIME_START:
		if((start_time > 0) && (temp_downtime->flex_downtime_start < 
					start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_downtime->flex_downtime_start > end_time)) {
			return 0;
			}
		break;
	case STATUS_TIME_END_TIME:
		if((start_time > 0) && (temp_downtime->end_time < start_time)) {
			return 0;
			}
		if((end_time > 0) && (temp_downtime->end_time > end_time)) {
			return 0;
			}
		break;
	default:
		return 0;
		}

	if(object_types != DOWNTIME_OBJECT_TYPE_ALL) {
		switch(temp_downtime->type) {
		case HOST_DOWNTIME:
			if(!(object_types & DOWNTIME_OBJECT_TYPE_HOST)) {
				return 0;
				}
			break;
		case SERVICE_DOWNTIME:
			if(!(object_types & DOWNTIME_OBJECT_TYPE_SERVICE)) {
				return 0;
				}
			break;
			}
		}

	if(downtime_types != DOWNTIME_TYPE_ALL) {
		if(temp_downtime->fixed) {
			if(!(downtime_types & DOWNTIME_TYPE_FIXED)) {
				return 0;
				}
			}
		else {
			if(!(downtime_types & DOWNTIME_TYPE_FLEXIBLE)) {
				return 0;
				}
			}
		}

	if(triggered != BOOLEAN_EITHER) {
		if(0 == temp_downtime->triggered_by) {
			if(!(triggered & BOOLEAN_FALSE)) {
				return 0;
				}
			}
		else {
			if(!(triggered & BOOLEAN_TRUE)) {
				return 0;
				}
			}
		}

	if(triggered_by != -1) {
		if(temp_downtime->triggered_by != (unsigned long)triggered_by) {
			return 0;
			}
		}

	if(in_effect != BOOLEAN_EITHER) {
		if(0 == temp_downtime->is_in_effect) {
			if(!(in_effect & BOOLEAN_FALSE)) {
				return 0;
				}
			}
		else {
			if(!(in_effect & BOOLEAN_TRUE)) {
				return 0;
				}
			}
		}

	if(NULL != host_name) {
		if((NULL == temp_downtime->host_name) ||
				strcmp(temp_downtime->host_name, host_name)) {
			return 0;
			}
		}

	if(NULL != service_description) {
		if((NULL == temp_downtime->service_description) ||
				strcmp(temp_downtime->service_description,
				service_description)) {
			return 0;
			}
		}

	return 1;
	}

json_object *json_status_downtime_selectors(unsigned format_options, int start, 
		int count, int time_field, time_t start_time, time_t end_time,
		unsigned object_types, unsigned downtime_types, unsigned triggered,
		int triggered_by, unsigned in_effect, char *host_name,
		char *service_description) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if(time_field > 0) {
		json_enumeration(json_selectors, format_options, "downtimetimefield", 
				time_field, svm_downtime_time_fields);
		}
	if(start_time > 0) {
		json_object_append_time_t(json_selectors, "starttime", start_time);
		}
	if(end_time > 0) {
		json_object_append_time_t(json_selectors, "endtime", end_time);
		}
	if(object_types != DOWNTIME_OBJECT_TYPE_ALL) {
		json_bitmask(json_selectors, format_options, "downtimeobjecttypes", 
				object_types, svm_valid_downtime_object_types);
		}
	if(downtime_types != DOWNTIME_TYPE_ALL) {
		json_bitmask(json_selectors, format_options, "downtimetypes", 
				downtime_types, svm_valid_downtime_types);
		}
	if(triggered != BOOLEAN_EITHER) {
		json_bitmask(json_selectors, format_options, "triggered", 
				triggered, svm_valid_triggered_status);
		}
	if(triggered_by != -1) {
		json_object_append_integer(json_selectors, "triggeredby", triggered_by);
		}
	if(in_effect != BOOLEAN_EITHER) {
		json_bitmask(json_selectors, format_options, "ineffect", 
				in_effect, svm_valid_in_effect_status);
		}
	if(NULL != host_name) {
		json_object_append_string(json_selectors, "hostname", &percent_escapes,
				host_name);
		}
	if(NULL != service_description) {
		json_object_append_string(json_selectors, "servicedescription",
				&percent_escapes, service_description);
		}

	return json_selectors;
	}

json_object *json_status_downtimecount(unsigned format_options, int time_field, 
		time_t start_time, time_t end_time, unsigned object_types,
		unsigned downtime_types, unsigned triggered, int triggered_by,
		unsigned in_effect, char *host_name, char *service_description) {

	json_object *json_data;
	scheduled_downtime *temp_downtime;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_status_downtime_selectors(format_options, 0, 0, time_field, 
			start_time, end_time, object_types, downtime_types, triggered,
			triggered_by, in_effect, host_name, service_description));

	for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; 
			temp_downtime = temp_downtime->next) {
		if(!json_status_downtime_passes_selection(temp_downtime, time_field, 
				start_time, end_time, object_types, downtime_types,
				triggered, triggered_by, in_effect, host_name,
				service_description)) {
			continue;
			}
		count++;
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_status_downtimelist(unsigned format_options, int start, 
		int count, int details, int time_field, time_t start_time, 
		time_t end_time, unsigned object_types, unsigned downtime_types,
		unsigned triggered, int triggered_by, unsigned in_effect,
		char *host_name, char *service_description) {

	json_object *json_data;
	json_object *json_downtimelist_object = NULL;
	json_object *json_downtimelist_array = NULL;
	json_object *json_downtime_details;
	scheduled_downtime *temp_downtime;
	int current = 0;
	int counted = 0;
	char *buf;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_status_downtime_selectors(format_options, start, count, 
			time_field, start_time, end_time, object_types, downtime_types,
			triggered, triggered_by, in_effect, host_name,
			service_description));

	if(details > 0) {
		json_downtimelist_object = json_new_object();
		}
	else {
		json_downtimelist_array = json_new_array();
		}

	for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; 
			temp_downtime = temp_downtime->next) {

		if(!json_status_downtime_passes_selection(temp_downtime, time_field, 
				start_time, end_time, object_types, downtime_types,
				triggered, triggered_by, in_effect, host_name,
				service_description)) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			if( details > 0) {
				asprintf(&buf, "%lu", temp_downtime->downtime_id);
				json_downtime_details = json_new_object();
				json_status_downtime_details(json_downtime_details, 
						format_options, temp_downtime);
				json_object_append_object(json_downtimelist_object, buf, 
						json_downtime_details);
				}
			else {
				json_array_append_integer(json_downtimelist_array, 
						temp_downtime->downtime_id); 
				}
			counted++;
			}
		current++; 
		}

	if(details > 0) {
		json_object_append_object(json_data, "downtimelist", 
				json_downtimelist_object);
		}
	else {
		json_object_append_array(json_data, "downtimelist", 
				json_downtimelist_array);
		}

	return json_data;
	}

json_object *json_status_downtime(unsigned format_options, 
		scheduled_downtime *temp_downtime) {

	json_object *json_downtime = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_integer(json_details, "downtime_id", 
			temp_downtime->downtime_id);
	json_status_downtime_details(json_details, format_options, temp_downtime); 
	json_object_append_object(json_downtime, "downtime", json_details);

	return json_downtime;
	}

void json_status_downtime_details(json_object *json_details, 
		unsigned format_options, scheduled_downtime *temp_downtime) {

	json_object_append_integer(json_details, "downtime_id", 
			temp_downtime->downtime_id);
	json_enumeration(json_details, format_options, "type", temp_downtime->type, 
			svm_downtime_types);
	json_object_append_string(json_details, "host_name", &percent_escapes,
			temp_downtime->host_name);
	if(SERVICE_DOWNTIME == temp_downtime->type) {
		json_object_append_string(json_details, "service_description",
				&percent_escapes, temp_downtime->service_description);
		}
	json_object_append_time_t(json_details, "entry_time", 
			temp_downtime->entry_time);
	json_object_append_time_t(json_details, "start_time", 
			temp_downtime->start_time);
	json_object_append_time_t(json_details, "flex_downtime_start", 
			temp_downtime->flex_downtime_start);
	json_object_append_time_t(json_details, "end_time", temp_downtime->end_time);
	json_object_append_boolean(json_details, "fixed", temp_downtime->fixed);
	json_object_append_integer(json_details, "triggered_by", 
			(int)temp_downtime->triggered_by);
	json_object_append_integer(json_details, "duration", 
			(int)temp_downtime->duration);
	json_object_append_boolean(json_details, "is_in_effect", 
			temp_downtime->is_in_effect);
	json_object_append_boolean(json_details, "start_notification_sent", 
			temp_downtime->start_notification_sent);
	json_object_append_string(json_details, "author", &percent_escapes,
			temp_downtime->author);
	json_object_append_string(json_details, "comment", &percent_escapes,
			temp_downtime->comment);
	}

json_object *json_status_program(unsigned format_options) {

	json_object *json_data;
	json_object *json_status;

	json_data = json_new_object();

	json_status = json_new_object();

#if 0
	json_object_append_unsigned(json_status, "modified_host_attributes", 
			(unsigned long long)modified_host_process_attributes);
	json_object_append_unsigned(json_status, 
			"modified_service_process_attributes", 
			(unsigned long long)modified_service_process_attributes);
#endif
	json_object_append_string(json_status, "version", NULL, PROGRAM_VERSION);
	json_object_append_integer(json_status, "nagios_pid", nagios_pid);
	json_object_append_boolean(json_status, "daemon_mode", daemon_mode);
	json_object_append_time_t(json_status, "program_start", program_start);
#ifndef JSON_NAGIOS_4X
	json_object_append_time_t(json_status, "last_command_check", 
			last_command_check);
#endif
	json_object_append_time_t(json_status, "last_log_rotation", 
			last_log_rotation);
	json_object_append_boolean(json_status, "enable_notifications", 
			enable_notifications);
	json_object_append_boolean(json_status, "execute_service_checks", 
			execute_service_checks);
	json_object_append_boolean(json_status, "accept_passive_service_checks", 
			accept_passive_service_checks);
	json_object_append_boolean(json_status, "execute_host_checks", 
			execute_host_checks);
	json_object_append_boolean(json_status, "accept_passive_host_checks", 
			accept_passive_host_checks);
	json_object_append_boolean(json_status, "enable_event_handlers", 
			enable_event_handlers);
	json_object_append_boolean(json_status, "obsess_over_services", 
			obsess_over_services);
	json_object_append_boolean(json_status, "obsess_over_hosts", 
			obsess_over_hosts);
	json_object_append_boolean(json_status, "check_service_freshness", 
			check_service_freshness);
	json_object_append_boolean(json_status, "check_host_freshness", 
			check_host_freshness);
	json_object_append_boolean(json_status, "enable_flap_detection", 
			enable_flap_detection);
#ifndef JSON_NAGIOS_4X
	json_object_append_boolean(json_status, "enable_failure_prediction", 
			enable_failure_prediction);
#endif
	json_object_append_boolean(json_status, "process_performance_data", 
			process_performance_data);
#if 0
	json_object_append_string(json_status, "global_host_event_handler", 
			&percent_escapes, global_host_event_handler);
	json_object_append_string(json_status, "global_service_event_handler", 
			&percent_escapes, global_service_event_handler);
	json_object_append_unsigned(json_status, "next_comment_id", 
			(unsigned long long)next_comment_id);
	json_object_append_unsigned(json_status, "next_downtime_id", 
			(unsigned long long)next_downtime_id);
	json_object_append_unsigned(json_status, "next_event_id", 
			(unsigned long long)next_event_id);
	json_object_append_unsigned(json_status, "next_problem_id", 
			(unsigned long long)next_problem_id);
	json_object_append_unsigned(json_status, "next_notification_id", 
			(unsigned long long)next_notification_id);
	json_object_append_unsigned(json_status, 
			"total_external_command_buffer_slots",
			(unsigned long long)external_command_buffer_slots);
	json_object_append_unsigned(json_status, "used_external_command_buffer_slots",
			(unsigned long long)used_external_command_buffer_slots);
	json_object_append_unsigned(json_status, "high_external_command_buffer_slots",
			(unsigned long long)high_external_command_buffer_slots);
	json_object_append_string(json_status, "active_scheduled_host_check_stats", 
			NULL, "%d,%d,%d", 
			check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[0],
			check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[1],
			check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[2]);
	json_object_append_string(json_status, "active_ondemand_host_check_stats", 
			NULL, "%d,%d,%d", 
			check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[0],
			check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[1],
			check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[2]);
	json_object_append_string(json_status, "passive_host_check_stats", NULL,
			"%d,%d,%d",
			check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[0],
			check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[1],
			check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[2]);
	json_object_append_string(json_status,
			"active_scheduled_service_check_stats", NULL, "%d,%d,%d",
			check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[0],
			check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[1],
			check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[2]);
	json_object_append_string(json_status, "active_ondemand_service_check_stats", 
			NULL, "%d,%d,%d",
			check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[0],
			check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[1],
			check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[2]);
	json_object_append_string(json_status, "passive_service_check_stats", 
			NULL, "%d,%d,%d",
			check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[0],
			check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[1],
			check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[2]);
	json_object_append_string(json_status, "cached_host_check_stats", NULL,
			"%d,%d,%d",
			check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[0],
			check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[1],
			check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[2]);
	json_object_append_string(json_status, "cached_service_check_stats", 
			NULL, "%d,%d,%d",
			check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[0],
			check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[1],
			check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[2]);
	json_object_append_string(json_status, "external_command_stats", NULL,
			"%d,%d,%d",
			check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[0],
			check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[1],
			check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[2]);
	json_object_append_string(json_status, "parallel_host_check_stats", 
			NULL, "%d,%d,%d",
			check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[0],
			check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[1],
			check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[2]);
	json_object_append_string(json_status, "serial_host_check_stats", NULL,
			"%d,%d,%d",
			check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[0],
			check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[1],
			check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[2]);
#endif

	json_object_append_object(json_data, "programstatus", json_status);

	return json_data;
	}

json_object *json_status_performance(void) { 

	service *temp_service = NULL;
	servicestatus *temp_servicestatus = NULL;
	host *temp_host = NULL;
	hoststatus *temp_hoststatus = NULL;
	int total_active_service_checks = 0;
	int total_passive_service_checks = 0;
	double min_service_execution_time = 0.0;
	double max_service_execution_time = 0.0;
	double total_service_execution_time = 0.0;
	int have_min_service_execution_time = FALSE;
	int have_max_service_execution_time = FALSE;
	double min_service_latency = 0.0;
	double max_service_latency = 0.0;
	double long total_service_latency = 0.0;
	int have_min_service_latency = FALSE;
	int have_max_service_latency = FALSE;
	double min_host_latency = 0.0;
	double max_host_latency = 0.0;
	double total_host_latency = 0.0;
	int have_min_host_latency = FALSE;
	int have_max_host_latency = FALSE;
	double min_service_percent_change_a = 0.0;
	double max_service_percent_change_a = 0.0;
	double total_service_percent_change_a = 0.0;
	int have_min_service_percent_change_a = FALSE;
	int have_max_service_percent_change_a = FALSE;
	double min_service_percent_change_b = 0.0;
	double max_service_percent_change_b = 0.0;
	double total_service_percent_change_b = 0.0;
	int have_min_service_percent_change_b = FALSE;
	int have_max_service_percent_change_b = FALSE;
	int active_service_checks_1min = 0;
	int active_service_checks_5min = 0;
	int active_service_checks_15min = 0;
	int active_service_checks_1hour = 0;
	int active_service_checks_start = 0;
	int active_service_checks_ever = 0;
	int passive_service_checks_1min = 0;
	int passive_service_checks_5min = 0;
	int passive_service_checks_15min = 0;
	int passive_service_checks_1hour = 0;
	int passive_service_checks_start = 0;
	int passive_service_checks_ever = 0;
	int total_active_host_checks = 0;
	int total_passive_host_checks = 0;
	double min_host_execution_time = 0.0;
	double max_host_execution_time = 0.0;
	double total_host_execution_time = 0.0;
	int have_min_host_execution_time = FALSE;
	int have_max_host_execution_time = FALSE;
	double min_host_percent_change_a = 0.0;
	double max_host_percent_change_a = 0.0;
	double total_host_percent_change_a = 0.0;
	int have_min_host_percent_change_a = FALSE;
	int have_max_host_percent_change_a = FALSE;
	double min_host_percent_change_b = 0.0;
	double max_host_percent_change_b = 0.0;
	double total_host_percent_change_b = 0.0;
	int have_min_host_percent_change_b = FALSE;
	int have_max_host_percent_change_b = FALSE;
	int active_host_checks_1min = 0;
	int active_host_checks_5min = 0;
	int active_host_checks_15min = 0;
	int active_host_checks_1hour = 0;
	int active_host_checks_start = 0;
	int active_host_checks_ever = 0;
	int passive_host_checks_1min = 0;
	int passive_host_checks_5min = 0;
	int passive_host_checks_15min = 0;
	int passive_host_checks_1hour = 0;
	int passive_host_checks_start = 0;
	int passive_host_checks_ever = 0;
	time_t current_time;

	json_object *json_data;
	json_object *json_programstatus;
	json_object *json_service_checks;
	json_object *json_host_checks;
	json_object *json_check_statistics;
	json_object *json_buffer_usage;
	json_object *json_active;
	json_object *json_passive;
	json_object *json_checks;
	json_object *json_metrics;
	json_object *json_temp;

	time(&current_time);

	/* check all services */
	for(temp_servicestatus = servicestatus_list; temp_servicestatus != NULL; 
			temp_servicestatus = temp_servicestatus->next) {

		/* find the service */
		temp_service = find_service(temp_servicestatus->host_name, 
				temp_servicestatus->description);
		if(NULL == temp_service) {
			continue;
			}

		/* make sure the user has rights to view service information */
		if(FALSE == is_authorized_for_service(temp_service, 
				&current_authdata)) {
			continue;
			}

		/* is this an active or passive check? */
		if(SERVICE_CHECK_ACTIVE == temp_servicestatus->check_type) {

			total_active_service_checks++;

			total_service_execution_time += temp_servicestatus->execution_time;
			if(have_min_service_execution_time == FALSE || 
					temp_servicestatus->execution_time < 
					min_service_execution_time) {
				have_min_service_execution_time = TRUE;
				min_service_execution_time = temp_servicestatus->execution_time;
				}
			if(have_max_service_execution_time == FALSE || 
					temp_servicestatus->execution_time > 
					max_service_execution_time) {
				have_max_service_execution_time = TRUE;
				max_service_execution_time = temp_servicestatus->execution_time;
				}

			total_service_percent_change_a += 
					temp_servicestatus->percent_state_change;
			if(have_min_service_percent_change_a == FALSE || 
					temp_servicestatus->percent_state_change < 
					min_service_percent_change_a) {
				have_min_service_percent_change_a = TRUE;
				min_service_percent_change_a = 
						temp_servicestatus->percent_state_change;
				}
			if(have_max_service_percent_change_a == FALSE || 
					temp_servicestatus->percent_state_change > 
					max_service_percent_change_a) {
				have_max_service_percent_change_a = TRUE;
				max_service_percent_change_a = 
						temp_servicestatus->percent_state_change;
				}

			total_service_latency += temp_servicestatus->latency;
			if(have_min_service_latency == FALSE || 
					temp_servicestatus->latency < min_service_latency) {
				have_min_service_latency = TRUE;
				min_service_latency = temp_servicestatus->latency;
				}
			if(have_max_service_latency == FALSE || 
					temp_servicestatus->latency > max_service_latency) {
				have_max_service_latency = TRUE;
				max_service_latency = temp_servicestatus->latency;
				}

			if(temp_servicestatus->last_check >= (current_time - 60))
				active_service_checks_1min++;
			if(temp_servicestatus->last_check >= (current_time - 300))
				active_service_checks_5min++;
			if(temp_servicestatus->last_check >= (current_time - 900))
				active_service_checks_15min++;
			if(temp_servicestatus->last_check >= (current_time - 3600))
				active_service_checks_1hour++;
			if(temp_servicestatus->last_check >= program_start)
				active_service_checks_start++;
			if(temp_servicestatus->last_check != (time_t)0)
				active_service_checks_ever++;
			}

		else {
			total_passive_service_checks++;

			total_service_percent_change_b += 
					temp_servicestatus->percent_state_change;
			if(have_min_service_percent_change_b == FALSE || 
					temp_servicestatus->percent_state_change < 
					min_service_percent_change_b) {
				have_min_service_percent_change_b = TRUE;
				min_service_percent_change_b = 
						temp_servicestatus->percent_state_change;
				}
			if(have_max_service_percent_change_b == FALSE || 
					temp_servicestatus->percent_state_change > 
					max_service_percent_change_b) {
				have_max_service_percent_change_b = TRUE;
				max_service_percent_change_b = 
						temp_servicestatus->percent_state_change;
				}

			if(temp_servicestatus->last_check >= (current_time - 60))
				passive_service_checks_1min++;
			if(temp_servicestatus->last_check >= (current_time - 300))
				passive_service_checks_5min++;
			if(temp_servicestatus->last_check >= (current_time - 900))
				passive_service_checks_15min++;
			if(temp_servicestatus->last_check >= (current_time - 3600))
				passive_service_checks_1hour++;
			if(temp_servicestatus->last_check >= program_start)
				passive_service_checks_start++;
			if(temp_servicestatus->last_check != (time_t)0)
				passive_service_checks_ever++;
			}
		}

	/* check all hosts */
	for(temp_hoststatus = hoststatus_list; temp_hoststatus != NULL; 
			temp_hoststatus = temp_hoststatus->next) {

		/* find the host */
		temp_host = find_host(temp_hoststatus->host_name);
		if(NULL == temp_host) {
			continue;
			}

		/* make sure the user has rights to view host information */
		if(FALSE == is_authorized_for_host(temp_host, &current_authdata)) {
			continue;
			}

		/* is this an active or passive check? */
		if(temp_hoststatus->check_type == HOST_CHECK_ACTIVE) {

			total_active_host_checks++;

			total_host_execution_time += temp_hoststatus->execution_time;
			if(have_min_host_execution_time == FALSE || 
					temp_hoststatus->execution_time < min_host_execution_time) {
				have_min_host_execution_time = TRUE;
				min_host_execution_time = temp_hoststatus->execution_time;
				}
			if(have_max_host_execution_time == FALSE || 
					temp_hoststatus->execution_time > max_host_execution_time) {
				have_max_host_execution_time = TRUE;
				max_host_execution_time = temp_hoststatus->execution_time;
				}

			total_host_percent_change_a += 
					temp_hoststatus->percent_state_change;
			if(have_min_host_percent_change_a == FALSE || 
					temp_hoststatus->percent_state_change < 
					min_host_percent_change_a) {
				have_min_host_percent_change_a = TRUE;
				min_host_percent_change_a = 
						temp_hoststatus->percent_state_change;
				}
			if(have_max_host_percent_change_a == FALSE || 
					temp_hoststatus->percent_state_change > 
					max_host_percent_change_a) {
				have_max_host_percent_change_a = TRUE;
				max_host_percent_change_a = 
						temp_hoststatus->percent_state_change;
				}

			total_host_latency += temp_hoststatus->latency;
			if(have_min_host_latency == FALSE || 
					temp_hoststatus->latency < min_host_latency) {
				have_min_host_latency = TRUE;
				min_host_latency = temp_hoststatus->latency;
				}
			if(have_max_host_latency == FALSE || 
					temp_hoststatus->latency > max_host_latency) {
				have_max_host_latency = TRUE;
				max_host_latency = temp_hoststatus->latency;
				}

			if(temp_hoststatus->last_check >= (current_time - 60))
				active_host_checks_1min++;
			if(temp_hoststatus->last_check >= (current_time - 300))
				active_host_checks_5min++;
			if(temp_hoststatus->last_check >= (current_time - 900))
				active_host_checks_15min++;
			if(temp_hoststatus->last_check >= (current_time - 3600))
				active_host_checks_1hour++;
			if(temp_hoststatus->last_check >= program_start)
				active_host_checks_start++;
			if(temp_hoststatus->last_check != (time_t)0)
				active_host_checks_ever++;
			}

		else {
			total_passive_host_checks++;

			total_host_percent_change_b += temp_hoststatus->percent_state_change;
			if(have_min_host_percent_change_b == FALSE || 
					temp_hoststatus->percent_state_change < 
					min_host_percent_change_b) {
				have_min_host_percent_change_b = TRUE;
				min_host_percent_change_b = 
						temp_hoststatus->percent_state_change;
				}
			if(have_max_host_percent_change_b == FALSE || 
					temp_hoststatus->percent_state_change > 
					max_host_percent_change_b) {
				have_max_host_percent_change_b = TRUE;
				max_host_percent_change_b = 
						temp_hoststatus->percent_state_change;
				}

			if(temp_hoststatus->last_check >= (current_time - 60))
				passive_host_checks_1min++;
			if(temp_hoststatus->last_check >= (current_time - 300))
				passive_host_checks_5min++;
			if(temp_hoststatus->last_check >= (current_time - 900))
				passive_host_checks_15min++;
			if(temp_hoststatus->last_check >= (current_time - 3600))
				passive_host_checks_1hour++;
			if(temp_hoststatus->last_check >= program_start)
				passive_host_checks_start++;
			if(temp_hoststatus->last_check != (time_t)0)
				passive_host_checks_ever++;
			}
		}

	/* Avoid divide by zero errors */
	if(0 == total_active_service_checks) {
		total_active_service_checks = 1;
		}
	if(0 == total_passive_service_checks) {
		total_passive_service_checks = 1;
		}
	if(0 == total_active_host_checks) {
		total_active_host_checks = 1;
		}
	if(0 == total_passive_host_checks) {
		total_passive_host_checks = 1;
		}

	json_data = json_new_object();

	json_programstatus = json_new_object();

	json_service_checks = json_new_object();

	/* Active Service Checks */

	json_active = json_new_object();

	json_checks = json_new_object();

	json_object_append_integer(json_checks, "1min", active_service_checks_1min);
	json_object_append_integer(json_checks, "5min", active_service_checks_5min);
	json_object_append_integer(json_checks, "15min", active_service_checks_15min);
	json_object_append_integer(json_checks, "1hour", active_service_checks_1hour);
	json_object_append_integer(json_checks, "start", active_service_checks_start);

	json_object_append_object(json_active, "checks", json_checks);

	json_metrics = json_new_object();

	json_temp = json_new_object();
	json_object_append_real(json_temp, "min", min_service_execution_time);
	json_object_append_real(json_temp, "max", max_service_execution_time);
	json_object_append_real(json_temp, "average", 
			(double)((double)total_service_execution_time / (double)total_active_service_checks));
	json_object_append_object(json_metrics, "check_execution_time", json_temp);

	json_temp = json_new_object();
	json_object_append_real(json_temp, "min", min_service_latency);
	json_object_append_real(json_temp, "max", max_service_latency);
	json_object_append_real(json_temp, "average", 
			(double)((double)total_service_latency / (double)total_active_service_checks));
	json_object_append_object(json_metrics, "check_latency", json_temp);

	json_temp = json_new_object();
	json_object_append_real(json_temp, "min", min_service_percent_change_a);
	json_object_append_real(json_temp, "max", max_service_percent_change_a);
	json_object_append_real(json_temp, "average", 
			(double)((double)total_service_percent_change_a / (double)total_active_service_checks));
	json_object_append_object(json_metrics, "percent_state_change", json_temp);

	json_object_append_object(json_active, "metrics", json_metrics);

	json_object_append_object(json_service_checks, "active", json_active);

	/* Passive Service Checks */

	json_passive = json_new_object();

	json_checks = json_new_object();
	json_object_append_integer(json_checks, "1min", passive_service_checks_1min);
	json_object_append_integer(json_checks, "5min", passive_service_checks_5min);
	json_object_append_integer(json_checks, "15min", 
			passive_service_checks_15min);
	json_object_append_integer(json_checks, "1hour", 
			passive_service_checks_1hour);
	json_object_append_integer(json_checks, "start", 
			passive_service_checks_start);
	json_object_append_object(json_passive, "checks", json_checks);

	json_metrics = json_new_object();

	json_temp = json_new_object();
	json_object_append_real(json_temp, "min", min_service_percent_change_b);
	json_object_append_real(json_temp, "max", max_service_percent_change_b);
	json_object_append_real(json_temp, "average", 
			(double)((double)total_service_percent_change_b / (double)total_active_service_checks));
	json_object_append_object(json_metrics, "percent_state_change", json_temp);

	json_object_append_object(json_passive, "metrics", json_metrics);

	json_object_append_object(json_service_checks, "passive", json_passive);

	json_object_append_object(json_programstatus, "service_checks", 
			json_service_checks);

	json_host_checks = json_new_object();

	/* Active Host Checks */

	json_active = json_new_object();

	json_checks = json_new_object();
	json_object_append_integer(json_checks, "1min", active_host_checks_1min);
	json_object_append_integer(json_checks, "5min", active_host_checks_5min);
	json_object_append_integer(json_checks, "15min", active_host_checks_15min);
	json_object_append_integer(json_checks, "1hour", active_host_checks_1hour);
	json_object_append_integer(json_checks, "start", active_host_checks_start);
	json_object_append_object(json_active, "checks", json_checks);

	json_metrics = json_new_object();

	json_temp = json_new_object();
	json_object_append_real(json_temp, "min", min_host_execution_time);
	json_object_append_real(json_temp, "max", max_host_execution_time);
	json_object_append_real(json_temp, "average", 
			(double)((double)total_host_execution_time / (double)total_active_host_checks));
	json_object_append_object(json_metrics, "check_execution_time", json_temp);

	json_temp = json_new_object();
	json_object_append_real(json_temp, "min", min_host_latency);
	json_object_append_real(json_temp, "max", max_host_latency);
	json_object_append_real(json_temp, "average", 
			(double)((double)total_host_latency / (double)total_active_host_checks));
	json_object_append_object(json_metrics, "check_latency", json_temp);

	json_temp = json_new_object();
	json_object_append_real(json_temp, "min", min_host_percent_change_a);
	json_object_append_real(json_temp, "max", max_host_percent_change_a);
	json_object_append_real(json_temp, "average", 
			(double)((double)total_host_percent_change_a / (double)total_active_host_checks));
	json_object_append_object(json_metrics, "percent_state_change", json_temp);

	json_object_append_object(json_active, "metrics", json_metrics);

	json_object_append_object(json_host_checks, "active", json_active);

	/* Passive Host Checks */

	json_passive = json_new_object();

	json_checks = json_new_object();
	json_object_append_integer(json_checks, "1min", passive_host_checks_1min);
	json_object_append_integer(json_checks, "5min", passive_host_checks_5min);
	json_object_append_integer(json_checks, "15min", passive_host_checks_15min);
	json_object_append_integer(json_checks, "1hour", passive_host_checks_1hour);
	json_object_append_integer(json_checks, "start", passive_host_checks_start);
	json_object_append_object(json_passive, "checks", json_checks);

	json_metrics = json_new_object();

	json_temp = json_new_object();
	json_object_append_real(json_temp, "min", min_host_percent_change_b);
	json_object_append_real(json_temp, "max", max_host_percent_change_b);
	json_object_append_real(json_temp, "average", 
			(double)((double)total_host_percent_change_b / (double)total_active_host_checks));
	json_object_append_object(json_metrics, "percent_state_change", json_temp);

	json_object_append_object(json_passive, "metrics", json_metrics);

	json_object_append_object(json_host_checks, "passive", json_passive);

	json_object_append_object(json_programstatus, "host_checks", 
			json_host_checks);

	/* Check Stats */

	json_check_statistics = json_new_object();

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "1min", 
			program_stats[ACTIVE_SCHEDULED_HOST_CHECK_STATS][0]);
	json_object_append_integer(json_temp, "5min", 
			program_stats[ACTIVE_SCHEDULED_HOST_CHECK_STATS][1]);
	json_object_append_integer(json_temp, "15min", 
			program_stats[ACTIVE_SCHEDULED_HOST_CHECK_STATS][2]);
	json_object_append_object(json_check_statistics, 
			"active_scheduled_host_checks", json_temp);

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "1min", 
			program_stats[ACTIVE_ONDEMAND_HOST_CHECK_STATS][0]);
	json_object_append_integer(json_temp, "5min", 
			program_stats[ACTIVE_ONDEMAND_HOST_CHECK_STATS][1]);
	json_object_append_integer(json_temp, "15min", 
			program_stats[ACTIVE_ONDEMAND_HOST_CHECK_STATS][2]);
	json_object_append_object(json_check_statistics, 
			"active_ondemand_host_checks", json_temp);

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "1min", 
			program_stats[PARALLEL_HOST_CHECK_STATS][0]);
	json_object_append_integer(json_temp, "5min", 
			program_stats[PARALLEL_HOST_CHECK_STATS][1]);
	json_object_append_integer(json_temp, "15min", 
			program_stats[PARALLEL_HOST_CHECK_STATS][2]);
	json_object_append_object(json_check_statistics, 
			"parallel_host_checks", json_temp);

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "1min", 
			program_stats[SERIAL_HOST_CHECK_STATS][0]);
	json_object_append_integer(json_temp, "5min", 
			program_stats[SERIAL_HOST_CHECK_STATS][1]);
	json_object_append_integer(json_temp, "15min", 
			program_stats[SERIAL_HOST_CHECK_STATS][2]);
	json_object_append_object(json_check_statistics, 
			"serial_host_checks", json_temp);

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "1min", 
			program_stats[ACTIVE_CACHED_HOST_CHECK_STATS][0]);
	json_object_append_integer(json_temp, "5min", 
			program_stats[ACTIVE_CACHED_HOST_CHECK_STATS][1]);
	json_object_append_integer(json_temp, "15min", 
			program_stats[ACTIVE_CACHED_HOST_CHECK_STATS][2]);
	json_object_append_object(json_check_statistics, 
			"cached_host_checks", json_temp);

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "1min", 
			program_stats[PASSIVE_HOST_CHECK_STATS][0]);
	json_object_append_integer(json_temp, "5min", 
			program_stats[PASSIVE_HOST_CHECK_STATS][1]);
	json_object_append_integer(json_temp, "15min", 
			program_stats[PASSIVE_HOST_CHECK_STATS][2]);
	json_object_append_object(json_check_statistics, 
			"passive_host_checks", json_temp);

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "1min", 
			program_stats[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS][0]);
	json_object_append_integer(json_temp, "5min", 
			program_stats[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS][1]);
	json_object_append_integer(json_temp, "15min", 
			program_stats[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS][2]);
	json_object_append_object(json_check_statistics, 
			"active_scheduled_service_checks", json_temp);

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "1min", 
			program_stats[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS][0]);
	json_object_append_integer(json_temp, "5min", 
			program_stats[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS][1]);
	json_object_append_integer(json_temp, "15min", 
			program_stats[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS][2]);
	json_object_append_object(json_check_statistics, 
			"active_ondemand_service_checks", json_temp);

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "1min", 
			program_stats[ACTIVE_CACHED_SERVICE_CHECK_STATS][0]);
	json_object_append_integer(json_temp, "5min", 
			program_stats[ACTIVE_CACHED_SERVICE_CHECK_STATS][1]);
	json_object_append_integer(json_temp, "15min", 
			program_stats[ACTIVE_CACHED_SERVICE_CHECK_STATS][2]);
	json_object_append_object(json_check_statistics, 
			"cached_service_checks", json_temp);

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "1min", 
			program_stats[PASSIVE_SERVICE_CHECK_STATS][0]);
	json_object_append_integer(json_temp, "5min", 
			program_stats[PASSIVE_SERVICE_CHECK_STATS][1]);
	json_object_append_integer(json_temp, "15min", 
			program_stats[PASSIVE_SERVICE_CHECK_STATS][2]);
	json_object_append_object(json_check_statistics, 
			"passive_service_checks", json_temp);

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "1min", 
			program_stats[EXTERNAL_COMMAND_STATS][0]);
	json_object_append_integer(json_temp, "5min", 
			program_stats[EXTERNAL_COMMAND_STATS][1]);
	json_object_append_integer(json_temp, "15min", 
			program_stats[EXTERNAL_COMMAND_STATS][2]);
	json_object_append_object(json_check_statistics, 
			"external_commands", json_temp);

	json_object_append_object(json_programstatus, "check_statistics", 
			json_check_statistics);

	/* Buffer Stats */

	json_buffer_usage = json_new_object();

	json_temp = json_new_object();
	json_object_append_integer(json_temp, "in_use", buffer_stats[0][1]);
	json_object_append_integer(json_temp, "max_used", buffer_stats[0][2]);
	json_object_append_integer(json_temp, "total_available", buffer_stats[0][0]);
	json_object_append_object(json_buffer_usage, "external_commands", json_temp);

	json_object_append_object(json_programstatus, "buffer_usage", 
			json_buffer_usage);

	json_object_append_object(json_data, "programstatus", json_programstatus);

	return json_data;
	}
