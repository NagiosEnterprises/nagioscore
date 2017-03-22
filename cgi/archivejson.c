/**************************************************************************
 *
 * ARCHIVEJSON.C -  Nagios CGI for returning JSON-formatted archive data
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

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"

#include "../include/cgiutils.h"
#include "../include/getcgi.h"
#include "../include/cgiauth.h"
#include "../include/jsonutils.h"
#include "../include/archiveutils.h"
#include "../include/archivejson.h"

#define THISCGI "archivejson.cgi"

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

extern char main_config_file[MAX_FILENAME_LENGTH];
extern int log_rotation_method;
extern char *status_file;

extern host *host_list;
extern hostgroup *hostgroup_list;
extern service *service_list;
extern servicegroup *servicegroup_list;
#if 0
extern contact *contact_list;
extern contactgroup *contactgroup_list;
extern timeperiod *timeperiod_list;
extern command *command_list;
extern servicedependency *servicedependency_list;
extern serviceescalation *serviceescalation_list;
extern hostdependency *hostdependency_list;
extern hostescalation *hostescalation_list;
#endif

void document_header();
void document_footer(void);
void init_cgi_data(archive_json_cgi_data *);
int process_cgivars(json_object *, archive_json_cgi_data *, time_t);
void free_cgi_data(archive_json_cgi_data *);
int validate_arguments(json_object *, archive_json_cgi_data *, time_t);

authdata current_authdata;

const string_value_mapping valid_queries[] = {
	{ "help", ARCHIVE_QUERY_HELP, 
		"Display help for this CGI" },
	{ "alertcount", ARCHIVE_QUERY_ALERTCOUNT, 
		"Return the number of alerts" },
	{ "alertlist", ARCHIVE_QUERY_ALERTLIST, 
		"Return a list of alerts" },
	{ "notificationcount", ARCHIVE_QUERY_NOTIFICATIONCOUNT, 
		"Return the number of notifications" },
	{ "notificationlist", ARCHIVE_QUERY_NOTIFICATIONLIST, 
		"Return a list of notifications" },
	{ "statechangelist", ARCHIVE_QUERY_STATECHANGELIST, 
		"Return a list of state changes" },
	{ "availability", ARCHIVE_QUERY_AVAILABILITY, 
		"Return an availability report" },
	{ NULL, -1, NULL },
	};

static const int query_status[][2] = {
	{ ARCHIVE_QUERY_HELP, QUERY_STATUS_RELEASED },
	{ ARCHIVE_QUERY_ALERTCOUNT, QUERY_STATUS_RELEASED },
	{ ARCHIVE_QUERY_ALERTLIST, QUERY_STATUS_RELEASED },
	{ ARCHIVE_QUERY_NOTIFICATIONCOUNT, QUERY_STATUS_RELEASED },
	{ ARCHIVE_QUERY_NOTIFICATIONLIST, QUERY_STATUS_RELEASED },
	{ ARCHIVE_QUERY_STATECHANGELIST, QUERY_STATUS_RELEASED },
	{ ARCHIVE_QUERY_AVAILABILITY, QUERY_STATUS_RELEASED },
	{ -1, -1 },
	};

const string_value_mapping valid_object_types[] = {
	{ "host", AU_OBJTYPE_HOST, "Host" },
	{ "service", AU_OBJTYPE_SERVICE, "Service" },
	{ NULL, -1, NULL },
	};

const string_value_mapping valid_availability_object_types[] = {
	{ "hosts", AU_OBJTYPE_HOST, "Hosts" },
	{ "hostgroups", AU_OBJTYPE_HOSTGROUP, "Hostgroups" },
	{ "services", AU_OBJTYPE_SERVICE, "Services" },
	{ "servicegroups", AU_OBJTYPE_SERVICEGROUP, "Servicegroups" },
	{ NULL, -1, NULL },
	};

const string_value_mapping valid_state_types[] = {
	{ "hard", AU_STATETYPE_HARD, "Hard" },
	{ "soft", AU_STATETYPE_SOFT, "Soft" },
	{ NULL, -1, NULL },
	};

const string_value_mapping valid_states[] = {
	{ "no_data", AU_STATE_NO_DATA, "No Data" },
	{ "host_up", AU_STATE_HOST_UP, "Host Up" },
	{ "host_down", AU_STATE_HOST_DOWN, "Host Down" },
	{ "host_unreachable", AU_STATE_HOST_UNREACHABLE, "Host Unreachable" },
	{ "service_ok", AU_STATE_SERVICE_OK, "Service OK" },
	{ "service_warning", AU_STATE_SERVICE_WARNING, "Service Warning" },
	{ "service_critical", AU_STATE_SERVICE_CRITICAL, "Service Critical" },
	{ "service_unknown", AU_STATE_SERVICE_UNKNOWN, "Service Unknown" },
	{ "program_start", AU_STATE_PROGRAM_START, "Program Start" },
	{ "program_end", AU_STATE_PROGRAM_END, "Program End" },
	{ "downtime_start", AU_STATE_DOWNTIME_START, "Downtime Start" },
	{ "downtime_end", AU_STATE_DOWNTIME_END, "Downtime End" },
};

const string_value_mapping valid_host_states[] = {
	{ "up", AU_STATE_HOST_UP, "Up" },
	{ "down", AU_STATE_HOST_DOWN, "Down" },
	{ "unreachable", AU_STATE_HOST_UNREACHABLE, "Unreachable" },
	{ NULL, -1, NULL },
	};

const string_value_mapping valid_initial_host_states[] = {
	{ "up", AU_STATE_HOST_UP, "Up" },
	{ "down", AU_STATE_HOST_DOWN, "Down" },
	{ "unreachable", AU_STATE_HOST_UNREACHABLE, "Unreachable" },
	{ "current", AU_STATE_CURRENT_STATE, "Current State" },
	{ NULL, -1, NULL },
	};

const string_value_mapping valid_service_states[] = {
	{ "ok", AU_STATE_SERVICE_OK, "Ok" },
	{ "warning", AU_STATE_SERVICE_WARNING, "Warning" },
	{ "critical", AU_STATE_SERVICE_CRITICAL, "Critical" },
	{ "unknown", AU_STATE_SERVICE_UNKNOWN, "Unknown" },
	{ NULL, -1, NULL },
	};

const string_value_mapping valid_initial_service_states[] = {
	{ "ok", AU_STATE_SERVICE_OK, "Ok" },
	{ "warning", AU_STATE_SERVICE_WARNING, "Warning" },
	{ "critical", AU_STATE_SERVICE_CRITICAL, "Critical" },
	{ "unknown", AU_STATE_SERVICE_UNKNOWN, "Unknown" },
	{ "current", AU_STATE_CURRENT_STATE, "Current State" },
	{ NULL, -1, NULL },
	};

const string_value_mapping valid_host_notification_types[] = {
	{ "nodata", AU_NOTIFICATION_NO_DATA, 
			"No Data" },
	{ "down", AU_NOTIFICATION_HOST_DOWN, 
			"Host Down" },
	{ "unreachable", AU_NOTIFICATION_HOST_UNREACHABLE, 
			"Host Unreachable" },
	{ "recovery", AU_NOTIFICATION_HOST_RECOVERY, 
			"Host Recovery" },
	{ "hostcustom", AU_NOTIFICATION_HOST_CUSTOM, 
			"Host Custom" },
	{ "hostack", AU_NOTIFICATION_HOST_ACK, 
			"Host Acknowledgement" },
	{ "hostflapstart", AU_NOTIFICATION_HOST_FLAPPING_START, 
			"Host Flapping Start" },
	{ "hostflapstop", AU_NOTIFICATION_HOST_FLAPPING_STOP, 
			"Host Flapping Stop" },
	{ NULL, -1, NULL },
	};

const string_value_mapping valid_service_notification_types[] = {
	{ "nodata", AU_NOTIFICATION_NO_DATA, 
			"No Data" },
	{ "critical", AU_NOTIFICATION_SERVICE_CRITICAL, 
			"Service Critical" },
	{ "warning", AU_NOTIFICATION_SERVICE_WARNING, 
			"Service Warning" },
	{ "recovery", AU_NOTIFICATION_SERVICE_RECOVERY, 
			"Service Recovery" },
	{ "custom", AU_NOTIFICATION_SERVICE_CUSTOM, 
			"Service Custom" },
	{ "serviceack", AU_NOTIFICATION_SERVICE_ACK, 
			"Service Acknowledgement" },
	{ "serviceflapstart", AU_NOTIFICATION_SERVICE_FLAPPING_START, 
			"Service Flapping Start" },
	{ "serviceflapstop", AU_NOTIFICATION_SERVICE_FLAPPING_STOP, 
			"Service Flapping Stop" },
	{ "unknown", AU_NOTIFICATION_SERVICE_UNKNOWN, 
			"Service Unknown" },
	{ NULL, -1, NULL },
	};

option_help archive_json_help[] = {
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
		{ "alertlist", "notificationlist", "statechangelist", NULL },
		NULL,
		"Specifies the index (zero-based) of the first object in the list to be returned.",
		NULL
		},
	{ 
		"count",
		"Count",
		"integer",
		{ NULL },
		{ "alertlist", "notificationlist", "statechangelist", NULL },
		NULL,
		"Specifies the number of objects in the list to be returned.",
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
		"objecttypes",
		"Object Types",
		"list",
		{ NULL },
		{ "alertcount", "alertlist", "notificationcount", "notificationlist", 
				NULL },
		NULL,
		"Type(s) of object to be included in query results.",
		valid_object_types
		},
	{ 
		"objecttype",
		"Object Type",
		"enumeration",
		{ "statechangelist", NULL },
		{ NULL },
		NULL,
		"Type of object to be included in query results.",
		valid_object_types
		},
	{ 
		"availabilityobjecttype",
		"Availability Object Type",
		"enumeration",
		{ "availability", NULL },
		{ NULL },
		NULL,
		"Type of object to be included in query results.",
		valid_availability_object_types
		},
	{ 
		"statetypes",
		"State Types",
		"list",
		{ NULL },
		{ "alertcount", "alertlist", "statechangelist", "availability", NULL },
		NULL,
		"Type(s) of states to be included in query results.",
		valid_state_types
		},
	{ 
		"hoststates",
		"Host States",
		"list",
		{ NULL },
		{ "alertcount", "alertlist", NULL },
		NULL,
		"Host states to be included in query results.",
		valid_host_states
		},
	{ 
		"servicestates",
		"Service States",
		"list",
		{ NULL },
		{ "alertcount", "alertlist", NULL },
		NULL,
		"Service states to be included in query results.",
		valid_service_states
		},
	{ 
		"hostnotificationtypes",
		"Host Notification Types",
		"list",
		{ NULL },
		{ "notificationcount", "notificationlist", NULL },
		NULL,
		"Host notification types to be included in query results.",
		valid_host_notification_types
		},
	{ 
		"servicenotificationtypes",
		"Service Notification Types",
		"list",
		{ NULL },
		{ "notificationcount", "notificationlist", NULL },
		NULL,
		"Service notification types to be included in query results.",
		valid_service_notification_types
		},
	{ 
		"parenthost",
		"Parent Host",
		"nagios:objectjson/hostlist",
		{ NULL },
		{ "alertcount", "alertlist", "notificationcount", "notificationlist", 
				NULL },
		NULL,
		"Limits the hosts or services returned to those whose host parent is specified. A value of 'none' returns all hosts or services reachable directly by the Nagios core host.",
		parent_host_extras
		},
	{ 
		"childhost",
		"Child Host",
		"nagios:objectjson/hostlist",
		{ NULL },
		{ "alertcount", "alertlist", "notificationcount", "notificationlist", 
				NULL },
		NULL,
		"Limits the hosts or services returned to those whose having the host specified as a child host. A value of 'none' returns all hosts or services with no child hosts.",
		child_host_extras
		},
	{ 
		"hostname",
		"Host Name",
		"nagios:objectjson/hostlist",
		{ "statechangelist", NULL },
		{ "alertcount", "alertlist", "notificationcount", "notificationlist", 
				"availability", NULL },
		NULL,
		"Name for the host requested. For availability reports if the "
		"availability object type is hosts and the hostname is not "
		"specified, the report will be generated for all hosts. Likewise, "
		"if the availability object type is services and the hostname is not "
		"specified, the report will be generated for all services or all "
		"services with the same description, depending on the value for "
		"service description.",
		NULL
		},
	{ 
		"hostgroup",
		"Host Group",
		"nagios:objectjson/hostgrouplist",
		{ NULL },
		{ "alertcount", "alertlist", "notificationcount", "notificationlist", 
				"availability", NULL },
		NULL,
		"Returns information applicable to the hosts in the hostgroup.",
		NULL
		},
	{ 
		"servicegroup",
		"Service Group",
		"nagios:objectjson/servicegrouplist",
		{ NULL },
		{ "alertcount", "alertlist", "notificationcount", "notificationlist", 
				"availability", NULL },
		NULL,
		"Returns information applicable to the services in the servicegroup.",
		NULL
		},
	{ 
		"servicedescription",
		"Service Description",
		"nagios:objectjson/servicelist",
		{ NULL },
		{ "alertcount", "alertlist", "notificationcount", "notificationlist", 
				"statechangelist", "availability", NULL },
		"hostname",
		"Description for the service requested. For availability reports, "
		"if the availability object type is services and the "
		"servicedescription is not specified, the report will be generated "
		"either for all services or for all services on the specified host, "
		"depending on the value specified for hostname",
		NULL
		},
	{ 
		"contactname",
		"Contact Name",
		"nagios:objectjson/contactlist",
		{ NULL },
		{ "alertcount", "alertlist", "notificationcount", "notificationlist", 
				NULL },
		NULL,
		"Name for the contact requested.",
		NULL
		},
	{ 
		"contactgroup",
		"Contact Group",
		"nagios:objectjson/contactgrouplist",
		{ NULL },
		{ "alertcount", "alertlist", "notificationcount", "notificationlist",
				NULL },
		NULL,
		"Returns information applicable to the contacts in the contactgroup.",
		NULL
		},
	{ 
		"notificationmethod",
		"Notification Method",
		"nagios:objectjson/commandlist",
		{ NULL },
		{ "notificationcount", "notificationlist", NULL },
		NULL,
		"Returns objects that match the notification method.",
		NULL
		},
	{ 
		"timeperiod",
		"Report Time Period",
		"nagios:objectjson/timeperiodlist",
		{ NULL },
		{ "availability", NULL },
		NULL,
		"Timeperiod to use for the report.",
		NULL
		},
	{ 
		"assumeinitialstate",
		"Assume Initial State",
		"boolean",
		{ NULL },
		{ "availability", NULL },
		NULL,
		"Assume the initial state for the host(s) or service(s). Note that if true, assuming the initial state will be done only if the initial state could not be discovered by looking through the backtracked logs.",
		NULL
		},
	{ 
		"assumestateretention",
		"Assume State Retention",
		"boolean",
		{ NULL },
		{ "availability", NULL },
		NULL,
		"Assume states are retained.",
		NULL
		},
	{ 
		"assumestatesduringnagiosdowntime",
		"Assume States During Nagios Downtime",
		"boolean",
		{ NULL },
		{ "availability", NULL },
		NULL,
		"Assume states are retained during Nagios downtime.",
		NULL
		},
	{ 
		"assumedinitialhoststate",
		"Assumed Initial Host State",
		"enumeration",
		{ NULL },
		{ "availability", NULL },
		NULL,
		"Host state assumed when it is not possible to determine the initial host state.",
		valid_initial_host_states
		},
	{ 
		"assumedinitialservicestate",
		"Assumed Initial Service State",
		"enumeration",
		{ NULL },
		{ "availability", NULL },
		NULL,
		"Service state assumed when it is not possible to determine the initial service state.",
		valid_initial_service_states
		},
	{ 
		"backtrackedarchives",
		"Backtracked Archives",
		"integer",
		{ NULL },
		{ "alertcount", "alertlist", "notificationcount", "notificationlist", 
				"statechangelist", "availability", NULL },
		NULL,
		"Number of backtracked archives to read in an attempt to find initial states.",
		NULL,
		},
	{ 
		"starttime",
		"Start Time",
		"integer",
		{ "alertcount", "alertlist", "notificationcount", "notificationlist", 
				"statechangelist", "availability", NULL },
		{ NULL },
		NULL,
		"Starting time to use. Supplying a plus or minus sign means times relative to the query time.",
		NULL,
		},
	{ 
		"endtime",
		"End Time",
		"integer",
		{ "alertcount", "alertlist", "notificationcount", "notificationlist", 
				"statechangelist", "availability", NULL },
		{ NULL },
		NULL,
		"Ending time to use. Specifying plus or minus sign means times relative to the query time.",
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

int json_archive_alert_passes_selection(time_t, time_t, time_t, int, int, 
		au_host *, char *, au_service *, char *, int, host *, int, host *,
		hostgroup *, servicegroup *, contact *, contactgroup *, unsigned, 
		unsigned, unsigned, unsigned, unsigned);
json_object *json_archive_alert_selectors(unsigned, int, int, time_t, time_t, 
		int, char *, char *, int, host *, int, host *, hostgroup *, 
		servicegroup *, contact *, contactgroup *, unsigned, unsigned, 
		unsigned);

int json_archive_notification_passes_selection(time_t, time_t, time_t, int, 
		int, au_host *, char *, au_service *, char *, int, host *, int, host *, 
		hostgroup *, servicegroup *, au_contact *, char *, contactgroup *, 
		unsigned, unsigned, unsigned, char *, char *);
json_object *json_archive_notification_selectors(unsigned, int, int, time_t, 
		time_t, int, char *, char *, int, host *, int, host *, hostgroup *, 
		servicegroup *, char *, contactgroup *, unsigned, unsigned, char *);

int json_archive_statechange_passes_selection(time_t, time_t, int, int, 
		au_host *, char *, au_service *, char *, unsigned, unsigned);
json_object *json_archive_statechange_selectors(unsigned, int, int, time_t, 
		time_t, int, char *, char *, unsigned);

int get_initial_nagios_state(au_linked_list *, time_t, time_t);
int get_initial_downtime_state(au_linked_list *, time_t, time_t);
int get_initial_subject_state(au_linked_list *, time_t, time_t);
int get_log_entry_state(au_log_entry *);
int have_data(au_linked_list *, int);
void compute_window_availability(time_t, time_t, int, int, int, timeperiod *, 
		au_availability *, int, int, int);
static void compute_availability(au_linked_list *, time_t, time_t, time_t, 
		timeperiod *, au_availability *, int, int, int, int);
void compute_host_availability(time_t, time_t, time_t, au_log *, au_host *, 
		timeperiod *, int, int, int, int);
void compute_service_availability(time_t, time_t, time_t, au_log *, 
		au_service *, timeperiod *, int, int, int, int);
json_object *json_archive_single_host_availability(unsigned, time_t, time_t, 
		time_t, char *, timeperiod *, int, int, int, int, unsigned, au_log *);
json_object *json_archive_single_service_availability(unsigned, time_t, time_t, 
		time_t, char *, char *, timeperiod *, int, int, int, int, unsigned, 
		au_log *);
json_object *json_archive_host_availability(unsigned, char *, 
		au_availability *);
json_object *json_archive_service_availability(unsigned, char *, char *,
		au_availability *);

int main(void) {
	int result = OK;
	time_t query_time;
	archive_json_cgi_data	cgi_data;
	json_object *json_root;
	au_log *log;
	time_t last_archive_data_update = (time_t)0;
	json_object_member *romp = NULL;

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

	document_header();

	/* get the arguments passed in the URL */
	result = process_cgivars(json_root, &cgi_data, query_time);
	if(result != RESULT_SUCCESS) {
		json_object_append_object(json_root, "data", 
				json_help(archive_json_help));
		json_object_print(json_root, 0, 1, cgi_data.strftime_format, 
				cgi_data.format_options);
		document_footer();
		return result;
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
		json_object_append_object(json_root, "data", 
				json_help(archive_json_help));
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
		json_object_append_object(json_root, "data", 
				json_help(archive_json_help));
		document_footer();
		return ERROR;
		}

	/* Set the number of backtracked archives if it wasn't specified by the 
		user */
	if(0 == cgi_data.backtracked_archives) {
		switch(log_rotation_method) {
		case LOG_ROTATION_MONTHLY:
			cgi_data.backtracked_archives = 1;
			break;
		case LOG_ROTATION_WEEKLY:
			cgi_data.backtracked_archives = 2;
			break;
		case LOG_ROTATION_DAILY:
			cgi_data.backtracked_archives = 4;
			break;
		case LOG_ROTATION_HOURLY:
			cgi_data.backtracked_archives = 8;
			break;
		default:
			cgi_data.backtracked_archives = 2;
			break;
			}
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
		json_object_append_object(json_root, "data", 
				json_help(archive_json_help));
		document_footer();
		return ERROR;
		}

	/* read all status data */
	result = read_all_status_data(status_file, READ_ALL_STATUS_DATA);
	if(result == ERROR) {
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, NULL, RESULT_FILE_OPEN_READ_ERROR,
				"Error: Could not read host and service status information!"));
		json_object_append_object(json_root, "data", 
				json_help(archive_json_help));

		document_footer();
		return ERROR;
		}

	/* validate arguments in URL */
	result = validate_arguments(json_root, &cgi_data, query_time);
	if((result != RESULT_SUCCESS) && (result != RESULT_OPTION_IGNORED)) {
		json_object_append_object(json_root, "data", 
				json_help(archive_json_help));
		json_object_print(json_root, 0, 1, cgi_data.strftime_format, 
				cgi_data.format_options);
		document_footer();
		return ERROR;
		}

	/* get authentication information */
	get_authentication_information(&current_authdata);

	/* Allocate the structure for the logs */
	if((log = au_init_log()) == NULL) {
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, &current_authdata, RESULT_MEMORY_ALLOCATION_ERROR,
				"Unable to allocate memory for log data."));
		json_object_append_object(json_root, "data", 
				json_help(archive_json_help));

		document_footer();
		return ERROR;
		}

	/* Return something to the user */
	switch( cgi_data.query) {
	case ARCHIVE_QUERY_HELP:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				compile_time(__DATE__, __TIME__), &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_help(archive_json_help));
		break;
	case ARCHIVE_QUERY_ALERTCOUNT:
		read_archived_data(cgi_data.start_time, cgi_data.end_time, 
				cgi_data.backtracked_archives, AU_OBJTYPE_ALL, 
				AU_STATETYPE_ALL, AU_LOGTYPE_ALERT | AU_LOGTYPE_STATE, log,
				&last_archive_data_update);
		if(result != RESULT_OPTION_IGNORED) {
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data.query, valid_queries), 
					get_query_status(query_status, cgi_data.query),
					last_archive_data_update, &current_authdata,
					RESULT_SUCCESS, ""));
			}
		else {
			romp = json_get_object_member(json_root, "result");
			json_object_append_time_t(romp->value.object, "last_data_update",
					last_archive_data_update);
			}
		json_object_append_object(json_root, "data", 
				json_archive_alertcount(cgi_data.format_options, 
				cgi_data.start_time, cgi_data.end_time, cgi_data.object_types, 
				cgi_data.host_name, cgi_data.service_description, 
				cgi_data.use_parent_host, cgi_data.parent_host, 
				cgi_data.use_child_host, cgi_data.child_host, 
				cgi_data.hostgroup, cgi_data.servicegroup, cgi_data.contact, 
				cgi_data.contactgroup, cgi_data.state_types, 
				cgi_data.host_states, cgi_data.service_states, log));
		break;
	case ARCHIVE_QUERY_ALERTLIST:
		read_archived_data(cgi_data.start_time, cgi_data.end_time, 
				cgi_data.backtracked_archives, AU_OBJTYPE_ALL, 
				AU_STATETYPE_ALL, AU_LOGTYPE_ALERT | AU_LOGTYPE_STATE, log,
				&last_archive_data_update);
		if(result != RESULT_OPTION_IGNORED) {
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data.query, valid_queries), 
					get_query_status(query_status, cgi_data.query),
					last_archive_data_update, &current_authdata,
					RESULT_SUCCESS, ""));
			}
		else {
			romp = json_get_object_member(json_root, "result");
			json_object_append_time_t(romp->value.object, "last_data_update",
					last_archive_data_update);
			}
		json_object_append_object(json_root, "data", 
				json_archive_alertlist(cgi_data.format_options, cgi_data.start,
				cgi_data.count, cgi_data.start_time, cgi_data.end_time, 
				cgi_data.object_types, cgi_data.host_name, 
				cgi_data.service_description, cgi_data.use_parent_host, 
				cgi_data.parent_host, cgi_data.use_child_host, 
				cgi_data.child_host, cgi_data.hostgroup, cgi_data.servicegroup, 
				cgi_data.contact, cgi_data.contactgroup, cgi_data.state_types, 
				cgi_data.host_states, cgi_data.service_states, log));
		break;
	case ARCHIVE_QUERY_NOTIFICATIONCOUNT:
		read_archived_data(cgi_data.start_time, cgi_data.end_time, 
				cgi_data.backtracked_archives, AU_OBJTYPE_ALL, 
				AU_STATETYPE_ALL, AU_LOGTYPE_NOTIFICATION, log,
				&last_archive_data_update);
		if(result != RESULT_OPTION_IGNORED) {
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data.query, valid_queries), 
					get_query_status(query_status, cgi_data.query),
					last_archive_data_update, &current_authdata,
					RESULT_SUCCESS, ""));
			}
		else {
			romp = json_get_object_member(json_root, "result");
			json_object_append_time_t(romp->value.object, "last_data_update",
					last_archive_data_update);
			}
		json_object_append_object(json_root, "data", 
				json_archive_notificationcount(cgi_data.format_options, 
				cgi_data.start_time, cgi_data.end_time, cgi_data.object_types, 
				cgi_data.host_name, cgi_data.service_description, 
				cgi_data.use_parent_host, cgi_data.parent_host, 
				cgi_data.use_child_host, cgi_data.child_host, 
				cgi_data.hostgroup, cgi_data.servicegroup, 
				cgi_data.contact_name, cgi_data.contactgroup, 
				cgi_data.host_notification_types, 
				cgi_data.service_notification_types, 
				cgi_data.notification_method, log));
		break;
	case ARCHIVE_QUERY_NOTIFICATIONLIST:
		read_archived_data(cgi_data.start_time, cgi_data.end_time, 
				cgi_data.backtracked_archives, AU_OBJTYPE_ALL, 
				AU_STATETYPE_ALL, AU_LOGTYPE_NOTIFICATION, log,
				&last_archive_data_update);
		if(result != RESULT_OPTION_IGNORED) {
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data.query, valid_queries), 
					get_query_status(query_status, cgi_data.query),
					last_archive_data_update, &current_authdata,
					RESULT_SUCCESS, ""));
			}
		else {
			romp = json_get_object_member(json_root, "result");
			json_object_append_time_t(romp->value.object, "last_data_update",
					last_archive_data_update);
			}
		json_object_append_object(json_root, "data", 
				json_archive_notificationlist(cgi_data.format_options, 
				cgi_data.start, cgi_data.count, cgi_data.start_time, 
				cgi_data.end_time, cgi_data.object_types, cgi_data.host_name, 
				cgi_data.service_description, cgi_data.use_parent_host, 
				cgi_data.parent_host, cgi_data.use_child_host, 
				cgi_data.child_host, cgi_data.hostgroup, cgi_data.servicegroup, 
				cgi_data.contact_name, cgi_data.contactgroup, 
				cgi_data.host_notification_types, 
				cgi_data.service_notification_types, 
				cgi_data.notification_method, log));
		break;
	case ARCHIVE_QUERY_STATECHANGELIST:
		read_archived_data(cgi_data.start_time, cgi_data.end_time, 
				cgi_data.backtracked_archives, cgi_data.object_type, 
				AU_STATETYPE_ALL, (AU_LOGTYPE_ALERT | AU_LOGTYPE_STATE), log,
				&last_archive_data_update);
		if(result != RESULT_OPTION_IGNORED) {
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data.query, valid_queries), 
					get_query_status(query_status, cgi_data.query),
					last_archive_data_update, &current_authdata,
					RESULT_SUCCESS, ""));
			}
		else {
			romp = json_get_object_member(json_root, "result");
			json_object_append_time_t(romp->value.object, "last_data_update",
					last_archive_data_update);
			}
		json_object_append_object(json_root, "data", 
				json_archive_statechangelist(cgi_data.format_options, 
				cgi_data.start, cgi_data.count, cgi_data.start_time, 
				cgi_data.end_time, cgi_data.object_type, cgi_data.host_name, 
				cgi_data.service_description, 
				cgi_data.assumed_initial_host_state, 
				cgi_data.assumed_initial_service_state, cgi_data.state_types, 
				log));
		break;
	case ARCHIVE_QUERY_AVAILABILITY:
		switch(cgi_data.object_type) {
		case AU_OBJTYPE_HOST:
		case AU_OBJTYPE_HOSTGROUP:
			read_archived_data(cgi_data.start_time, cgi_data.end_time, 
					cgi_data.backtracked_archives, AU_OBJTYPE_HOST, 
					cgi_data.state_types, (AU_LOGTYPE_NAGIOS | AU_LOGTYPE_ALERT 
					| AU_LOGTYPE_STATE | AU_LOGTYPE_DOWNTIME), log,
					&last_archive_data_update);
			break;
		case AU_OBJTYPE_SERVICE:
		case AU_OBJTYPE_SERVICEGROUP:
			read_archived_data(cgi_data.start_time, cgi_data.end_time, 
					cgi_data.backtracked_archives, AU_OBJTYPE_SERVICE, 
					cgi_data.state_types, (AU_LOGTYPE_NAGIOS | AU_LOGTYPE_ALERT 
					| AU_LOGTYPE_STATE | AU_LOGTYPE_DOWNTIME), log,
					&last_archive_data_update);
			break;
		}
		if(result != RESULT_OPTION_IGNORED) {
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data.query, valid_queries), 
					get_query_status(query_status, cgi_data.query),
					last_archive_data_update, &current_authdata,
					RESULT_SUCCESS, ""));
			}
		else {
			romp = json_get_object_member(json_root, "result");
			json_object_append_time_t(romp->value.object, "last_data_update",
					last_archive_data_update);
			}
		json_object_append_object(json_root, "data", 
				json_archive_availability(cgi_data.format_options, query_time,
				cgi_data.start_time, cgi_data.end_time, 
				cgi_data.object_type, cgi_data.host_name, 
				cgi_data.service_description, cgi_data.hostgroup, 
				cgi_data.servicegroup, cgi_data.timeperiod, 
				cgi_data.assume_initial_state, cgi_data.assume_state_retention, 
				cgi_data.assume_states_during_nagios_downtime,
				cgi_data.assumed_initial_host_state, 
				cgi_data.assumed_initial_service_state, cgi_data.state_types, 
				log));
		break;
	default:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, &current_authdata, RESULT_OPTION_MISSING,
				"Error: Object Type not specified. See data for help."));
		json_object_append_object(json_root, "data", 
				json_help(archive_json_help));
		break;
		}

	json_object_print(json_root, 0, 1, cgi_data.strftime_format, 
			cgi_data.format_options);
	document_footer();

	/* free all allocated memory */
	free_cgi_data( &cgi_data);
	json_free_object(json_root, 1);
	au_free_log(log);
	free_memory();

	return OK;
	}

void document_header() {
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


void init_cgi_data(archive_json_cgi_data *cgi_data) {
	cgi_data->format_options = 0;
	cgi_data->query = ARCHIVE_QUERY_INVALID;
	cgi_data->start = 0;
	cgi_data->count = 0;
	cgi_data->object_types = AU_OBJTYPE_ALL;
	cgi_data->object_type = 0;
	cgi_data->state_types = AU_STATETYPE_ALL;
	cgi_data->host_states = AU_STATE_HOST_ALL;
	cgi_data->service_states = AU_STATE_SERVICE_ALL;
	cgi_data->host_notification_types = AU_NOTIFICATION_HOST_ALL;
	cgi_data->service_notification_types = AU_NOTIFICATION_SERVICE_ALL;
#if 0
	cgi_data->details = 0;
#endif
	cgi_data->strftime_format = NULL;
	cgi_data->start_time = (time_t)0;
	cgi_data->end_time = (time_t)0;
	cgi_data->parent_host_name = NULL;
	cgi_data->use_parent_host = 0;
	cgi_data->parent_host = NULL;
	cgi_data->child_host_name = NULL;
	cgi_data->use_child_host = 0;
	cgi_data->child_host = NULL;
	cgi_data->host_name = NULL;
	cgi_data->host = NULL;
	cgi_data->hostgroup_name = NULL;
	cgi_data->hostgroup = NULL;
	cgi_data->servicegroup_name = NULL;
	cgi_data->servicegroup = NULL;
	cgi_data->service_description = NULL;
	cgi_data->service = NULL;
	cgi_data->contact_name = NULL;
	cgi_data->contact = NULL;
	cgi_data->contactgroup_name = NULL;
	cgi_data->contactgroup = NULL;
	cgi_data->notification_method = NULL;
	cgi_data->timeperiod_name = NULL;
	cgi_data->timeperiod = NULL;
	cgi_data->assumed_initial_host_state = AU_STATE_NO_DATA;
	cgi_data->assumed_initial_service_state = AU_STATE_NO_DATA;
	cgi_data->assume_initial_state = 1;
	cgi_data->assume_state_retention = 1;
	cgi_data->assume_states_during_nagios_downtime = 1;
	cgi_data->backtracked_archives = 0;
}

void free_cgi_data(archive_json_cgi_data *cgi_data) {
	if(NULL != cgi_data->strftime_format) free( cgi_data->strftime_format);
	if(NULL != cgi_data->parent_host_name) free( cgi_data->parent_host_name);
	if(NULL != cgi_data->child_host_name) free( cgi_data->child_host_name);
	if(NULL != cgi_data->host_name) free( cgi_data->host_name);
	if(NULL != cgi_data->hostgroup_name) free( cgi_data->hostgroup_name);
	if(NULL != cgi_data->servicegroup_name) free(cgi_data->servicegroup_name);
	if(NULL != cgi_data->service_description) free(cgi_data->service_description);
	if(NULL != cgi_data->contact_name) free(cgi_data->contact_name);
	if(NULL != cgi_data->contactgroup_name) free(cgi_data->contactgroup_name);
	if(NULL != cgi_data->notification_method) free(cgi_data->notification_method);
	if(NULL != cgi_data->timeperiod_name) free(cgi_data->timeperiod_name);
	}


int process_cgivars(json_object *json_root, archive_json_cgi_data *cgi_data,
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

		else if(!strcmp(variables[x], "backtrackedarchives")) {
			if((result = parse_int_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->backtracked_archives)))
					!= RESULT_SUCCESS) {
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

		else if(!strcmp(variables[x], "objecttype")) {
			if((result = parse_enumeration_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], valid_object_types,
					&(cgi_data->object_type))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "objecttypes")) {
			cgi_data->object_types = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], valid_object_types,
					&(cgi_data->object_types))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "availabilityobjecttype")) {
			if((result = parse_enumeration_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], valid_availability_object_types,
					&(cgi_data->object_type))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "statetypes")) {
			cgi_data->state_types = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], valid_state_types,
					&(cgi_data->state_types))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "hoststates")) {
			cgi_data->host_states = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], valid_host_states,
					&(cgi_data->host_states))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "servicestates")) {
			cgi_data->service_states = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], valid_service_states,
					&(cgi_data->service_states))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "hostnotificationtypes")) {
			cgi_data->host_notification_types = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], valid_host_notification_types,
					&(cgi_data->host_notification_types))) != RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "servicenotificationtypes")) {
			cgi_data->service_notification_types = 0;
			if((result = parse_bitmask_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], valid_service_notification_types,
					&(cgi_data->service_notification_types))) 
					!= RESULT_SUCCESS) {
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

		else if(!strcmp(variables[x], "contactgroup")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->contactgroup_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "notificationmethod")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->notification_method)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "timeperiod")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->timeperiod_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "assumeinitialstate")) {
			if((result = parse_boolean_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->assume_initial_state)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "assumestateretention")) {
			if((result = parse_boolean_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->assume_state_retention)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "assumestatesduringnagiosdowntime")) {
			if((result = parse_boolean_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1],
					&(cgi_data->assume_states_during_nagios_downtime)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "assumedinitialhoststate")) {
			if((result = parse_enumeration_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], valid_initial_host_states,
					&(cgi_data->assumed_initial_host_state)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "assumedinitialservicestate")) {
			if((result = parse_enumeration_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], valid_initial_service_states,
					&(cgi_data->assumed_initial_service_state)))
					!= RESULT_SUCCESS) {
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
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "NagFormId"))
			++x;

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

int validate_arguments(json_object *json_root, archive_json_cgi_data *cgi_data,
		time_t query_time) {

	int result = RESULT_SUCCESS;
	host *temp_host = NULL;
	hostgroup *temp_hostgroup = NULL;
	servicegroup *temp_servicegroup = NULL;
	contactgroup *temp_contactgroup = NULL;
	timeperiod *temp_timeperiod = NULL;
#if 0
	service *temp_service = NULL;
	contact *temp_contact = NULL;
	command *temp_command = NULL;
#endif
	authdata *authinfo = NULL; /* Currently always NULL because
									get_authentication_information() hasn't
									been called yet, but in case we want to
									use it in the future... */

	/* Validate that required parameters were supplied */
	switch(cgi_data->query) {
	case ARCHIVE_QUERY_HELP:
		break;
	case ARCHIVE_QUERY_ALERTCOUNT:
	case ARCHIVE_QUERY_ALERTLIST:
		if((0 == cgi_data->start_time) && (0 == cgi_data->end_time)) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Start and/or end time must be supplied."));
			}

		if(!(cgi_data->object_types & (AU_OBJTYPE_HOST | AU_OBJTYPE_SERVICE))) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"At least one object type must be supplied."));
			}
		break;
	case ARCHIVE_QUERY_NOTIFICATIONCOUNT:
	case ARCHIVE_QUERY_NOTIFICATIONLIST:
		if((0 == cgi_data->start_time) && (0 == cgi_data->end_time)) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Start and/or end time must be supplied."));
			}

		if(!(cgi_data->object_types & (AU_OBJTYPE_HOST | AU_OBJTYPE_SERVICE))) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"At least one object type must be supplied."));
			}
		break;
	case ARCHIVE_QUERY_STATECHANGELIST:
		if((0 == cgi_data->start_time) && (0 == cgi_data->end_time)) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Start and/or end time must be supplied."));
			}

		if(0 == cgi_data->object_type) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Object type must be supplied."));
			}

		if(NULL == cgi_data->host_name) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Host name must be supplied."));
			}

		if((AU_OBJTYPE_SERVICE == cgi_data->object_type) && 
				(NULL == cgi_data->service_description)) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Service description must be supplied."));
			}

		break;
	case ARCHIVE_QUERY_AVAILABILITY:
		if((0 == cgi_data->start_time) && (0 == cgi_data->end_time)) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Start and/or end time must be supplied."));
			}

		if(0 == cgi_data->object_type) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Availability object type must be supplied."));
			}

		break;
	default:
		result = RESULT_OPTION_MISSING;
		json_object_append_object(json_root, "result", json_result(query_time,
				THISCGI,
				svm_get_string_from_value(cgi_data->query, valid_queries),
				get_query_status(query_status, cgi_data->query),
				(time_t)-1, authinfo, result,
				"Missing validation for object type %u.", cgi_data->query));
		break;
		}

	/* Attempt to find the host object associated with host_name. Because
		we're looking at historical data, the host may no longer 
		exist. */
	if(NULL != cgi_data->host_name) {
		cgi_data->host = find_host(cgi_data->host_name);
		}

	/* Attempt to find the service object associated with host_name and
		service_description. Because we're looking at historical data,
		the service may no longer exist. */
	if((NULL != cgi_data->host_name) && 
			(NULL != cgi_data->service_description)) {
		cgi_data->service = find_service(cgi_data->host_name, 
				cgi_data->service_description);
		}

	/* Validate the requested parent host */
	if( NULL != cgi_data->parent_host_name) {
		if(strcmp(cgi_data->parent_host_name, "none")) {
			temp_host = find_host(cgi_data->parent_host_name);
			if( NULL == temp_host) {
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
				cgi_data->use_parent_host = 1;
				cgi_data->parent_host = temp_host;
				}
			}
		else {
			cgi_data->use_parent_host = 1;
			cgi_data->parent_host = NULL;
			}
		}

	/* Validate the requested child host */
	if( NULL != cgi_data->child_host_name) {
		if(strcmp(cgi_data->child_host_name, "none")) {
			temp_host = find_host(cgi_data->child_host_name);
			if( NULL == temp_host) {
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
				cgi_data->use_child_host = 1;
				cgi_data->child_host = temp_host;
				}
			}
		else {
			cgi_data->use_child_host = 1;
			cgi_data->child_host = NULL;
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

	/* Attempt to find the contact object associated with contact_name. 
		Because we're looking at historical data, the contact may 
		no longer exist. */
	if(NULL != cgi_data->contact_name) {
		cgi_data->contact = find_contact(cgi_data->contact_name);
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

	/* Validate the requested timeperiod */
	if( NULL != cgi_data->timeperiod_name) {
		temp_timeperiod = find_timeperiod(cgi_data->timeperiod_name);
		if( NULL == temp_timeperiod) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The timeperiod '%s' could not be found.", 
					cgi_data->timeperiod_name));
			}
		else {
			cgi_data->timeperiod = temp_timeperiod;
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

	/* If one or more host states were selected, but host objects were not, 
			notify the user, but continue */
	if((cgi_data->host_states != AU_STATE_HOST_ALL) && 
			(!(cgi_data->object_types & AU_OBJTYPE_HOST))) {
		result = RESULT_OPTION_IGNORED;
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data->query, valid_queries), 
				get_query_status(query_status, cgi_data->query),
				(time_t)-1, authinfo, result, "The requested host states "
				"were ignored because host objects were not selected."));
		}
	/* If one or more service states were selected, but service objects 
			were not, notify the user but continue */
	else if((cgi_data->service_states != AU_STATE_SERVICE_ALL) && 
			(!(cgi_data->object_types & AU_OBJTYPE_SERVICE))) {
		result = RESULT_OPTION_IGNORED;
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data->query, valid_queries), 
				get_query_status(query_status, cgi_data->query),
				(time_t)-1, authinfo, result, "The requested service states "
				"were ignored because service objects were not selected."));
		}

	/* If one or more host notification types were selected, but host 
			objects were not, notify the user, but continue */
	if((cgi_data->host_notification_types != AU_NOTIFICATION_HOST_ALL) && 
			(!(cgi_data->object_types & AU_OBJTYPE_HOST))) {
		result = RESULT_OPTION_IGNORED;
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data->query, valid_queries), 
				get_query_status(query_status, cgi_data->query),
				(time_t)-1, authinfo, result,
				"The requested host notification types were ignored because host objects were not selected."));
		}
	/* If one or more service notification types were selected, but 
			service objects were not, notify the user but continue */
	else if((cgi_data->service_notification_types != 
			AU_NOTIFICATION_SERVICE_ALL) && 
			(!(cgi_data->object_types & AU_OBJTYPE_SERVICE))) {
		result = RESULT_OPTION_IGNORED;
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data->query, valid_queries), 
				get_query_status(query_status, cgi_data->query),
				(time_t)-1, authinfo, result,
				"The requested service notification types were ignored because service objects were not selected."));
		}

	return result;
	}

int json_archive_alert_passes_selection(time_t timestamp, time_t start_time, 
		time_t end_time, int current_object_type, int match_object_types,
		au_host *current_host, char *match_host, au_service *current_service, 
		char *match_service, int use_parent_host, host *parent_host, 
		int use_child_host, host *child_host, hostgroup *match_hostgroup, 
		servicegroup *match_servicegroup, contact *match_contact, 
		contactgroup *match_contactgroup, unsigned current_state_type, 
		unsigned match_state_types, unsigned current_state, 
		unsigned match_host_states, unsigned match_service_states) {

	host *temp_host;
	host *temp_host2;
	service *temp_service;
	contactgroupsmember *temp_contact_groupsmember;
	int found;

	if((start_time > 0) && (timestamp < start_time)) {
		return 0;
		}

	if((end_time > 0) && (timestamp > end_time)) {
		return 0;
		}

	/* Skip if we're not interested in the current state type */
	if(!(current_state_type & match_state_types)) {
		return 0;
		}

	switch(current_object_type) {
	case AU_OBJTYPE_HOST:
		/* Skip if we're not interested in hosts */
		if(!(match_object_types & AU_OBJTYPE_HOST)) {
			return 0;
			}

		/* Skip if user is not authorized for this host */
		if((NULL != current_host->hostp) && (FALSE == 
				is_authorized_for_host(current_host->hostp, 
				&current_authdata))) {
			return 0;
			}

		/* Skip if we're not interested in the current host state */
		if(!(current_state & match_host_states)) {
			return 0;
			}

		/* If a specific host name was specified, skip this host if it's 
			name does not match the one specified */
		if((NULL != match_host) && strcmp(current_host->name, match_host)) {
			return 0;
			}

		/* If a host parent was specified, skip this host if it's parent is 
			not the parent host specified */
		if((1 == use_parent_host) && (NULL != current_host->hostp) && 
				FALSE == is_host_immediate_child_of_host(parent_host, 
				current_host->hostp)) {
			return 0;
			}

		/* If a hostgroup was specified, skip this host if it is not a member
			of the hostgroup specified */
		if((NULL != match_hostgroup) && (NULL != current_host->hostp) &&
				( FALSE == is_host_member_of_hostgroup(match_hostgroup, 
						current_host->hostp))) {
			return 0;
			}

		/* If the contact was specified, skip this host if it does not have
			the contact specified */
		if((NULL != match_contact) && (NULL != current_host->hostp) && 
				(FALSE == is_contact_for_host(current_host->hostp, 
				match_contact))) {
			return 0;
			}

		/* If the contact group was specified, skip this host if it does not 
			have the contact group specified */
		if((NULL != match_contactgroup) && (NULL != current_host->hostp)) {
			found = 0;
			for(temp_contact_groupsmember = 
					current_host->hostp->contact_groups; 
					temp_contact_groupsmember != NULL; 
					temp_contact_groupsmember = 
					temp_contact_groupsmember->next) {
				if(!strcmp(temp_contact_groupsmember->group_name, 
						match_contactgroup->group_name)) {
					found = 1;
					break;
					}
				}
			if(0 == found) return 0;
			}

		/* If a child host was specified... */
		if((1 == use_child_host) && (NULL != current_host->hostp)) { 
			/* If the child host is "none", skip this host if it has children */
			if(NULL == child_host) {
				for(temp_host2 = host_list; temp_host2 != NULL; 
						temp_host2 = temp_host2->next) {
					if(TRUE == 
							is_host_immediate_child_of_host(current_host->hostp,
							temp_host2)) {
						return 0;
						}
					}
				}
			/* Otherwise, skip this host if it does not have the specified host
				as a child */
			else if(FALSE == 
					is_host_immediate_child_of_host(current_host->hostp, 
					child_host)) {
				return 0;
				}
			}
		break;
	case AU_OBJTYPE_SERVICE:
		/* Skip if we're not interested in services */
		if(!(match_object_types & AU_OBJTYPE_SERVICE)) {
			return 0;
			}

		/* Skip if user is not authorized for this service */
		if((NULL != current_service->servicep) && (FALSE == 
				is_authorized_for_service(current_service->servicep, 
				&current_authdata))) {
			return 0;
			}

		/* Skip if we're not interested in the current service state */
		if(!(current_state & match_service_states)) {
			return 0;
			}

		/* If a specific host name was specified, skip this service if it's 
			host name does not match the one specified */
		if((NULL != match_host) && strcmp(current_service->host_name, 
				match_host)) {
			return 0;
			}

		/* If a specific service description was specified, skip this service 
			if it's description does not match the one specified */
		if((NULL != match_service) && strcmp(current_service->description, 
				match_service)) {
			return 0;
			}

		/* If a host parent was specified, skip this service if the parent 
			of the host associated with it is not the parent host specified */
		if((1 == use_parent_host) && (NULL != current_service->servicep)) {
			temp_host = find_host(current_service->servicep->host_name);
			if((NULL != temp_host) && (FALSE == 
					is_host_immediate_child_of_host(parent_host, temp_host))) {
				return 0;
				}
			}

		/* If a hostgroup was specified, skip this service if it's host is not 
			a member of the hostgroup specified */
		if((NULL != match_hostgroup) && (NULL != current_service->servicep)) {
			temp_host = find_host(current_service->servicep->host_name);
			if((NULL != temp_host) && (FALSE == 
					is_host_member_of_hostgroup(match_hostgroup, temp_host))) {
				return 0;
				}
			}

		/* If a servicegroup was specified, skip this service if it is not 
			a member of the servicegroup specified */
		if((NULL != match_servicegroup) && 
			(NULL != current_service->servicep)) {
			temp_service = find_service(current_service->servicep->host_name, 
					current_service->description);
			if((NULL != temp_service) && (FALSE == 
					is_service_member_of_servicegroup(match_servicegroup, 
					temp_service))) {
				return 0;
				}
			}

		/* If the contact was specified, skip this service if it does not have
			the contact specified */
		if((NULL != match_contact) && (NULL != current_service->servicep) && 
				(FALSE == is_contact_for_service(current_service->servicep, 
				match_contact))) {
			return 0;
			}

		/* If the contact group was specified, skip this service if it does not 
			have the contact group specified */
		if((NULL != match_contactgroup) && 
				(NULL != current_service->servicep)) {
			found = 0;
			for(temp_contact_groupsmember = 
					current_service->servicep->contact_groups; 
					temp_contact_groupsmember != NULL; 
					temp_contact_groupsmember = 
					temp_contact_groupsmember->next) {
				if(!strcmp(temp_contact_groupsmember->group_name, 
						match_contactgroup->group_name)) {
					found = 1;
					break;
					}
				}
			if(0 == found) return 0;
			}

		/* If a child host was specified... */
		if((1 == use_child_host) && (NULL != current_service->servicep)) { 
			temp_host = find_host(current_service->servicep->host_name);
			if(NULL != temp_host) {
				/* If the child host is "none", skip this service if it's 
					host has children */
				if(NULL == child_host) {
					for(temp_host2 = host_list; temp_host2 != NULL; 
							temp_host2 = temp_host2->next) {
						if(TRUE == is_host_immediate_child_of_host(temp_host,
								temp_host2)) {
							return 0;
							}
						}
					}
				/* Otherwise, skip this service if it's host does not have the 
					specified host as a child */
				else if(FALSE == is_host_immediate_child_of_host(temp_host,
						child_host)) {
					return 0;
					}
				}
			}
		break;
		}


	return 1;
	}

json_object * json_archive_alert_selectors(unsigned format_options, int start, 
		int count, time_t start_time, time_t end_time, int object_types, 
		char *match_host, char *match_service, int use_parent_host, 
		host *parent_host, int use_child_host, host *child_host, 
		hostgroup *match_hostgroup, servicegroup *match_servicegroup, 
		contact *match_contact, contactgroup *match_contactgroup, 
		unsigned match_state_types, unsigned match_host_states, 
		unsigned match_service_states) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}

	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}

	if(start_time > 0) {
		json_object_append_time_t(json_selectors, "starttime", start_time);
		}

	if(end_time > 0) {
		json_object_append_time_t(json_selectors, "endtime", end_time);
		}

	if(object_types != AU_OBJTYPE_ALL) {
		json_bitmask(json_selectors, format_options, "objecttypes", 
				object_types, valid_object_types);
		}

	if(match_state_types != AU_STATETYPE_ALL) {
		json_bitmask(json_selectors, format_options, "statetypes", 
				match_state_types, valid_state_types);
		}

	if((object_types & AU_OBJTYPE_HOST) && 
			(match_host_states != AU_STATE_HOST_ALL)) {
		json_bitmask(json_selectors, format_options, "hoststates", 
				match_host_states, valid_host_states);
		}

	if((object_types & AU_OBJTYPE_SERVICE) && 
			(match_service_states != AU_STATE_SERVICE_ALL)) {
		json_bitmask(json_selectors, format_options, "servicestates", 
				match_service_states, valid_service_states);
		}

	if(NULL != match_host) {
		json_object_append_string(json_selectors, "hostname", &percent_escapes,
				match_host);
		}

	if(NULL != match_service) {
		json_object_append_string(json_selectors, "servicedescription", 
				&percent_escapes, match_service);
		}

	if(1 == use_parent_host) {
		json_object_append_string(json_selectors, "parenthost", 
				&percent_escapes,
				( NULL == parent_host ? "none" : parent_host->name));
		}

	if( 1 == use_child_host) {
		json_object_append_string(json_selectors, "childhost", &percent_escapes,
				( NULL == child_host ? "none" : child_host->name));
		}

	if(NULL != match_hostgroup) {
		json_object_append_string(json_selectors, "hostgroup", &percent_escapes,
				match_hostgroup->group_name);
		}

	if((object_types & AU_OBJTYPE_SERVICE) && (NULL != match_servicegroup)) {
		json_object_append_string(json_selectors, "servicegroup",
				&percent_escapes, match_servicegroup->group_name);
		}

	if(NULL != match_contact) {
		json_object_append_string(json_selectors, "contact", &percent_escapes,
				match_contact->name);
		}

	if(NULL != match_contactgroup) {
		json_object_append_string(json_selectors, "contactgroup",
				&percent_escapes, match_contactgroup->group_name);
		}

	return json_selectors;
	}

json_object * json_archive_alertcount(unsigned format_options, 
		time_t start_time, time_t end_time, int object_types, 
		char *match_host, char *match_service, int use_parent_host, 
		host *parent_host, int use_child_host, host *child_host, 
		hostgroup *match_hostgroup, servicegroup *match_servicegroup, 
		contact *match_contact, contactgroup *match_contactgroup, 
		unsigned match_state_types, unsigned match_host_states, 
		unsigned match_service_states, au_log *log) {

	json_object *json_data;
	au_node *temp_node;
	au_log_entry *temp_entry;
	au_log_alert *temp_alert_log;
	au_host *temp_host = NULL;
	au_service *temp_service = NULL;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_archive_alert_selectors(format_options, 0, 0, start_time, 
			end_time, object_types, match_host, match_service, use_parent_host, 
			parent_host, use_child_host, child_host, match_hostgroup, 
			match_servicegroup, match_contact, match_contactgroup, 
			match_state_types, match_host_states, match_service_states));

	for(temp_node = log->entry_list->head; temp_node != NULL; 
			temp_node = temp_node->next) {

		temp_entry = (au_log_entry *)temp_node->data;

		/* Skip all but alert type messages */
		if(AU_LOGTYPE_ALERT != temp_entry->entry_type) continue;

		/* Get the host/service object */
		temp_alert_log = (au_log_alert *)temp_entry->entry;
		switch(temp_alert_log->obj_type) {
		case AU_OBJTYPE_HOST:
			temp_host = (au_host *)temp_alert_log->object;
			temp_service = NULL;
			break;
		case AU_OBJTYPE_SERVICE:
			temp_host = NULL;
			temp_service = (au_service *)temp_alert_log->object;
			break;
			}

		/* Skip anything that does not pass the alert selection criteria */
		if(json_archive_alert_passes_selection(temp_entry->timestamp, 
				start_time, end_time, temp_alert_log->obj_type, 
				object_types, temp_host, match_host, temp_service, 
				match_service, use_parent_host, parent_host, use_child_host, 
				child_host, match_hostgroup, match_servicegroup, 
				match_contact, match_contactgroup, temp_alert_log->state_type, 
				match_state_types, temp_alert_log->state, match_host_states, 
				match_service_states) == 0) {
			continue;
			}

		count++;
		}
	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object * json_archive_alertlist(unsigned format_options, int start, 
		int count, time_t start_time, time_t end_time, int object_types, 
		char *match_host, char *match_service, int use_parent_host, 
		host *parent_host, int use_child_host, host *child_host, 
		hostgroup *match_hostgroup, servicegroup *match_servicegroup, 
		contact *match_contact, contactgroup *match_contactgroup, 
		unsigned match_state_types, unsigned match_host_states, 
		unsigned match_service_states, au_log *log) {

	json_object *json_data;
	json_array *json_alertlist;
	json_object *json_alert_details;
	au_node *temp_node;
	au_log_entry *temp_entry;
	au_log_alert *temp_alert_log;
	au_host *temp_host = NULL;
	au_service *temp_service = NULL;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_archive_alert_selectors(format_options, start, count, 
			start_time, end_time, object_types, match_host, match_service, 
			use_parent_host, parent_host, use_child_host, child_host, 
			match_hostgroup, match_servicegroup, match_contact, 
			match_contactgroup, match_state_types, match_host_states, 
			match_service_states));

	json_alertlist = json_new_array();

	for(temp_node = log->entry_list->head; temp_node != NULL; 
			temp_node = temp_node->next) {

		temp_entry = (au_log_entry *)temp_node->data;

		/* Skip all but alert type messages */
		if(AU_LOGTYPE_ALERT != temp_entry->entry_type) continue;

		/* Get the host/service object */
		temp_alert_log = (au_log_alert *)temp_entry->entry;
		switch(temp_alert_log->obj_type) {
		case AU_OBJTYPE_HOST:
			temp_host = (au_host *)temp_alert_log->object;
			temp_service = NULL;
			break;
		case AU_OBJTYPE_SERVICE:
			temp_host = NULL;
			temp_service = (au_service *)temp_alert_log->object;
			break;
			}

		/* Skip anything that does not pass the alert selection criteria */
		if(json_archive_alert_passes_selection(temp_entry->timestamp, 
				start_time, end_time, temp_alert_log->obj_type, 
				object_types, temp_host, match_host, temp_service, 
				match_service, use_parent_host, parent_host, use_child_host, 
				child_host, match_hostgroup, match_servicegroup, 
				match_contact, match_contactgroup, temp_alert_log->state_type, 
				match_state_types, temp_alert_log->state, match_host_states, 
				match_service_states) == 0) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			json_alert_details = json_new_object();
			json_archive_alert_details(json_alert_details, format_options, 
					temp_entry->timestamp, (au_log_alert *)temp_entry->entry);
			json_array_append_object(json_alertlist, json_alert_details);
			counted++;
			}
		current++; 
		}

	json_object_append_array(json_data, "alertlist", json_alertlist);

	return json_data;
	}

void json_archive_alert_details(json_object *json_details, 
		unsigned format_options, time_t timestamp, au_log_alert *temp_alert) {

	au_host *temp_host;
	au_service *temp_service;

	json_object_append_time_t(json_details, "timestamp", timestamp);

	json_enumeration(json_details, format_options, "object_type", 
			temp_alert->obj_type, svm_au_object_types);

	switch(temp_alert->obj_type) {
	case AU_OBJTYPE_HOST:
		temp_host = (au_host *)temp_alert->object;
		json_object_append_string(json_details, "name", &percent_escapes,
				temp_host->name);
		break;
	case AU_OBJTYPE_SERVICE:
		temp_service = (au_service *)temp_alert->object;
		json_object_append_string(json_details, "host_name", &percent_escapes,
				temp_service->host_name);
		json_object_append_string(json_details, "description", &percent_escapes,
				temp_service->description);
		break;
		}

	json_enumeration(json_details, format_options, "state_type", 
			temp_alert->state_type, svm_au_state_types);
	json_enumeration(json_details, format_options, "state", temp_alert->state, 
			svm_au_states);
	json_object_append_string(json_details, "plugin_output", &percent_escapes,
			temp_alert->plugin_output);
	}

int json_archive_notification_passes_selection(time_t timestamp, 
		time_t start_time, time_t end_time, int current_object_type, 
		int match_object_types, au_host *current_host, char *match_host,
		au_service *current_service, char *match_service, int use_parent_host, 
		host *parent_host, int use_child_host, host *child_host, 
		hostgroup *match_hostgroup, servicegroup *match_servicegroup, 
		au_contact *current_contact, char *match_contact, 
		contactgroup *match_contactgroup, 
		unsigned current_notification_type, 
		unsigned match_host_notification_types, 
		unsigned match_service_notification_types, 
		char *current_notification_method, char *match_notification_method) {

	host *temp_host;
	host *temp_host2;
	service *temp_service;
	contactgroupsmember *temp_contact_groupsmember;
	int found;

	if((start_time > 0) && (timestamp < start_time)) {
		return 0;
		}

	if((end_time > 0) && (timestamp > end_time)) {
		return 0;
		}

	/* If the contact was specified, skip this notification if it does not have
		the contact specified */
	if((NULL != match_contact) && (NULL != current_contact) && 
			strcmp(current_contact->name, match_contact)) {
		return 0;
		}

	/* If the notification method was specified, skip this notification if 
		it does not have the method specified */
	if((NULL != match_notification_method) && 
			(NULL != current_notification_method) && 
			strcmp(current_notification_method, match_notification_method)) {
		return 0;
		}

	switch(current_object_type) {
	case AU_OBJTYPE_HOST:
		/* Skip if we're not interested in hosts */
		if(!(match_object_types & AU_OBJTYPE_HOST)) {
			return 0;
			}

		/* Skip if user is not authorized for this host */
		if((NULL != current_host->hostp) && (FALSE == 
				is_authorized_for_host(current_host->hostp, 
				&current_authdata))) {
			return 0;
			}

		/* Skip if we're not interested in the current host notification type */
		if(!(current_notification_type & match_host_notification_types)) {
			return 0;
			}

		/* If a specific host name was specified, skip this host if it's 
			name does not match the one specified */
		if((NULL != match_host) && strcmp(current_host->name, match_host)) {
			return 0;
			}

		/* If a host parent was specified, skip this host if it's parent is 
			not the parent host specified */
		if((1 == use_parent_host) && (NULL != current_host->hostp) && 
				FALSE == is_host_immediate_child_of_host(parent_host, 
				current_host->hostp)) {
			return 0;
			}

		/* If a hostgroup was specified, skip this host if it is not a member
			of the hostgroup specified */
		if((NULL != match_hostgroup) && (NULL != current_host->hostp) &&
				( FALSE == is_host_member_of_hostgroup(match_hostgroup, 
						current_host->hostp))) {
			return 0;
			}

		/* If the contact group was specified, skip this host if it does not 
			have the contact group specified */
		if((NULL != match_contactgroup) && (NULL != current_host->hostp)) {
			found = 0;
			for(temp_contact_groupsmember = 
					current_host->hostp->contact_groups; 
					temp_contact_groupsmember != NULL; 
					temp_contact_groupsmember = 
					temp_contact_groupsmember->next) {
				if(!strcmp(temp_contact_groupsmember->group_name, 
						match_contactgroup->group_name)) {
					found = 1;
					break;
					}
				}
			if(0 == found) return 0;
			}

		/* If a child host was specified... */
		if((1 == use_child_host) && (NULL != current_host->hostp)) { 
			/* If the child host is "none", skip this host if it has children */
			if(NULL == child_host) {
				for(temp_host2 = host_list; temp_host2 != NULL; 
						temp_host2 = temp_host2->next) {
					if(TRUE == 
							is_host_immediate_child_of_host(current_host->hostp,
							temp_host2)) {
						return 0;
						}
					}
				}
			/* Otherwise, skip this host if it does not have the specified host
				as a child */
			else if(FALSE == 
					is_host_immediate_child_of_host(current_host->hostp, 
					child_host)) {
				return 0;
				}
			}
		break;
	case AU_OBJTYPE_SERVICE:
		/* Skip if we're not interested in services */
		if(!(match_object_types & AU_OBJTYPE_SERVICE)) {
			return 0;
			}

		/* Skip if user is not authorized for this service */
		if((NULL != current_service->servicep) && (FALSE == 
				is_authorized_for_service(current_service->servicep, 
				&current_authdata))) {
			return 0;
			}

		/* Skip if we're not interested in the current service notification 
			type */
		if(!(current_notification_type & match_service_notification_types)) {
			return 0;
			}

		/* If a specific host name was specified, skip this service if it's 
			host name does not match the one specified */
		if((NULL != match_host) && strcmp(current_service->host_name, 
				match_host)) {
			return 0;
			}

		/* If a specific service description was specified, skip this service 
			if it's description does not match the one specified */
		if((NULL != match_service) && strcmp(current_service->description, 
				match_service)) {
			return 0;
			}

		/* If a host parent was specified, skip this service if the parent 
			of the host associated with it is not the parent host specified */
		if((1 == use_parent_host) && (NULL != current_service->servicep)) {
			temp_host = find_host(current_service->servicep->host_name);
			if((NULL != temp_host) && (FALSE == 
					is_host_immediate_child_of_host(parent_host, temp_host))) {
				return 0;
				}
			}

		/* If a hostgroup was specified, skip this service if it's host is not 
			a member of the hostgroup specified */
		if((NULL != match_hostgroup) && (NULL != current_service->servicep)) {
			temp_host = find_host(current_service->servicep->host_name);
			if((NULL != temp_host) && (FALSE == 
					is_host_member_of_hostgroup(match_hostgroup, temp_host))) {
				return 0;
				}
			}

		/* If a servicegroup was specified, skip this service if it is not 
			a member of the servicegroup specified */
		if((NULL != match_servicegroup) && 
			(NULL != current_service->servicep)) {
			temp_service = find_service(current_service->servicep->host_name, 
					current_service->description);
			if((NULL != temp_service) && (FALSE == 
					is_service_member_of_servicegroup(match_servicegroup, 
					temp_service))) {
				return 0;
				}
			}

		/* If the contact group was specified, skip this service if it does not 
			have the contact group specified */
		if((NULL != match_contactgroup) && 
				(NULL != current_service->servicep)) {
			found = 0;
			for(temp_contact_groupsmember = 
					current_service->servicep->contact_groups; 
					temp_contact_groupsmember != NULL; 
					temp_contact_groupsmember = 
					temp_contact_groupsmember->next) {
				if(!strcmp(temp_contact_groupsmember->group_name, 
						match_contactgroup->group_name)) {
					found = 1;
					break;
					}
				}
			if(0 == found) return 0;
			}

		/* If a child host was specified... */
		if((1 == use_child_host) && (NULL != current_service->servicep)) { 
			temp_host = find_host(current_service->servicep->host_name);
			if(NULL != temp_host) {
				/* If the child host is "none", skip this service if it's 
					host has children */
				if(NULL == child_host) {
					for(temp_host2 = host_list; temp_host2 != NULL; 
							temp_host2 = temp_host2->next) {
						if(TRUE == is_host_immediate_child_of_host(temp_host,
								temp_host2)) {
							return 0;
							}
						}
					}
				/* Otherwise, skip this service if it's host does not have the 
					specified host as a child */
				else if(FALSE == is_host_immediate_child_of_host(temp_host,
						child_host)) {
					return 0;
					}
				}
			}
		break;
		}

	return 1;
	}

json_object * json_archive_notification_selectors(unsigned format_options, 
		int start, int count, time_t start_time, time_t end_time, 
		int match_object_types, char *match_host, char *match_service, 
		int use_parent_host, host *parent_host, int use_child_host, 
		host *child_host, hostgroup *match_hostgroup, 
		servicegroup *match_servicegroup, char *match_contact, 
		contactgroup *match_contactgroup, 
		unsigned match_host_notification_types, 
		unsigned match_service_notification_types,
		char *match_notification_method) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}

	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}

	if(start_time > 0) {
		json_object_append_time_t(json_selectors, "starttime", start_time);
		}

	if(end_time > 0) {
		json_object_append_time_t(json_selectors, "endtime", end_time);
		}

	if(match_object_types != AU_OBJTYPE_ALL) {
		json_bitmask(json_selectors, format_options, "objecttypes", 
				match_object_types, valid_object_types);
		}

	if((match_object_types & AU_OBJTYPE_HOST) && 
			(match_host_notification_types != AU_NOTIFICATION_HOST_ALL)) {
		json_bitmask(json_selectors, format_options, "hostnotificationtypes", 
				match_host_notification_types, 
				valid_host_notification_types);
		}

	if((match_object_types & AU_OBJTYPE_SERVICE) && 
			(match_service_notification_types != AU_NOTIFICATION_SERVICE_ALL)) {
		json_bitmask(json_selectors, format_options, "servicenotificationtypes",
				match_service_notification_types, 
				valid_service_notification_types);
		}

	if(NULL != match_host) {
		json_object_append_string(json_selectors, "hostname", &percent_escapes,
				match_host);
		}

	if(NULL != match_service) {
		json_object_append_string(json_selectors, "servicedescription", 
				&percent_escapes, match_service);
		}

	if(1 == use_parent_host) {
		json_object_append_string(json_selectors, "parenthost", 
				&percent_escapes,
				( NULL == parent_host ? "none" : parent_host->name));
		}

	if( 1 == use_child_host) {
		json_object_append_string(json_selectors, "childhost", 
				&percent_escapes,
				( NULL == child_host ? "none" : child_host->name));
		}

	if(NULL != match_hostgroup) {
		json_object_append_string(json_selectors, "hostgroup", &percent_escapes,
				match_hostgroup->group_name);
		}

	if((match_object_types & AU_OBJTYPE_SERVICE) && 
			(NULL != match_servicegroup)) {
		json_object_append_string(json_selectors, "servicegroup",
				&percent_escapes, match_servicegroup->group_name);
		}

	if(NULL != match_contact) {
		json_object_append_string(json_selectors, "contact", &percent_escapes,
				match_contact);
		}

	if(NULL != match_contactgroup) {
		json_object_append_string(json_selectors, "contactgroup",
				&percent_escapes, match_contactgroup->group_name);
		}

	if(NULL != match_notification_method) {
		json_object_append_string(json_selectors, "notificationmethod",
				&percent_escapes, match_notification_method);
		}

	return json_selectors;
	}

json_object * json_archive_notificationcount(unsigned format_options, 
		time_t start_time, time_t end_time, int match_object_types, 
		char *match_host, char *match_service, int use_parent_host, 
		host *parent_host, int use_child_host, host *child_host, 
		hostgroup *match_hostgroup, servicegroup *match_servicegroup, 
		char *match_contact, contactgroup *match_contactgroup, 
		unsigned match_host_notification_types, 
		unsigned match_service_notification_types, 
		char *match_notification_method, au_log *log) {

	json_object *json_data;
	au_node *temp_node;
	au_log_entry *temp_entry;
	au_log_notification *temp_notification_log;
	au_host *temp_host = NULL;
	au_service *temp_service = NULL;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_archive_notification_selectors(format_options, 0, 0, 
			start_time, end_time, match_object_types, match_host, 
			match_service, use_parent_host, parent_host, use_child_host, 
			child_host, match_hostgroup, match_servicegroup, match_contact, 
			match_contactgroup, match_host_notification_types, 
			match_service_notification_types, match_notification_method));

	for(temp_node = log->entry_list->head; temp_node != NULL; 
			temp_node = temp_node->next) {

		temp_entry = (au_log_entry *)temp_node->data;

		/* Skip all but notification type messages */
		if(AU_LOGTYPE_NOTIFICATION != temp_entry->entry_type) continue;

		/* Get the host/service object */
		temp_notification_log = (au_log_notification *)temp_entry->entry;
		switch(temp_notification_log->obj_type) {
		case AU_OBJTYPE_HOST:
			temp_host = (au_host *)temp_notification_log->object;
			temp_service = NULL;
			break;
		case AU_OBJTYPE_SERVICE:
			temp_host = NULL;
			temp_service = (au_service *)temp_notification_log->object;
			break;
			}

		/* Skip anything that does not pass the notification selection 
			criteria */
		if(json_archive_notification_passes_selection(temp_entry->timestamp, 
				start_time, end_time, temp_notification_log->obj_type,
				match_object_types, temp_host, match_host, temp_service,
				match_service, use_parent_host, parent_host, use_child_host, 
				child_host, match_hostgroup, match_servicegroup, 
				temp_notification_log->contact, match_contact, 
				match_contactgroup, temp_notification_log->notification_type, 
				match_host_notification_types, 
				match_service_notification_types, temp_notification_log->method,				match_notification_method) == 0) {
			continue;
			}

		count++;
		}
	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object * json_archive_notificationlist(unsigned format_options, int start, 
		int count, time_t start_time, time_t end_time, int match_object_types, 
		char *match_host, char *match_service, int use_parent_host, 
		host *parent_host, int use_child_host, host *child_host, 
		hostgroup *match_hostgroup, servicegroup *match_servicegroup, 
		char *match_contact, contactgroup *match_contactgroup, 
		unsigned match_host_notification_types, 
		unsigned match_service_notification_types, 
		char *match_notification_method, au_log *log) {

	json_object *json_data;
	json_array *json_notificationlist;
	json_object *json_notification_details;
	au_node *temp_node;
	au_log_entry *temp_entry;
	au_log_notification *temp_notification_log;
	au_host *temp_host = NULL;
	au_service *temp_service = NULL;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_archive_notification_selectors(format_options, start, count, 
			start_time, end_time, match_object_types, match_host, 
			match_service, use_parent_host, parent_host, use_child_host, 
			child_host, match_hostgroup, match_servicegroup, match_contact, 
			match_contactgroup, match_host_notification_types, 
			match_service_notification_types, match_notification_method));

	json_notificationlist = json_new_array();

	for(temp_node = log->entry_list->head; temp_node != NULL; 
			temp_node = temp_node->next) {

		temp_entry = (au_log_entry *)temp_node->data;

		/* Skip all but notification type messages */
		if(AU_LOGTYPE_NOTIFICATION != temp_entry->entry_type) continue;

		/* Get the host/service object */
		temp_notification_log = (au_log_notification *)temp_entry->entry;
		switch(temp_notification_log->obj_type) {
		case AU_OBJTYPE_HOST:
			temp_host = (au_host *)temp_notification_log->object;
			temp_service = NULL;
			break;
		case AU_OBJTYPE_SERVICE:
			temp_host = NULL;
			temp_service = (au_service *)temp_notification_log->object;
			break;
			}

		/* Skip anything that does not pass the notification selection 
				criteria */
		if(json_archive_notification_passes_selection(temp_entry->timestamp, 
				start_time, end_time, temp_notification_log->obj_type, 
				match_object_types, temp_host, match_host, temp_service, 
				match_service, use_parent_host, parent_host, use_child_host, 
				child_host, match_hostgroup, match_servicegroup, 
				temp_notification_log->contact, match_contact, 
				match_contactgroup, temp_notification_log->notification_type, 
				match_host_notification_types, 
				match_service_notification_types, temp_notification_log->method,
				match_notification_method) == 0) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			json_notification_details = json_new_object();
			json_archive_notification_details(json_notification_details, 
					format_options, temp_entry->timestamp, 
					(au_log_notification *)temp_entry->entry);
			json_array_append_object(json_notificationlist, 
					json_notification_details);
			counted++;
			}
		current++; 
		}

	json_object_append_array(json_data, "notificationlist", 
			json_notificationlist);

	return json_data;
	}

void json_archive_notification_details(json_object *json_details, 
		unsigned format_options, time_t timestamp, 
		au_log_notification *temp_notification) {

	au_host *temp_host;
	au_service *temp_service;

	json_object_append_time_t(json_details, "timestamp", timestamp);

	json_enumeration(json_details, format_options, "object_type", 
			temp_notification->obj_type, 
			svm_au_object_types);

	switch(temp_notification->obj_type) {
	case AU_OBJTYPE_HOST:
		temp_host = (au_host *)temp_notification->object;
		json_object_append_string(json_details, "name", &percent_escapes,
				temp_host->name);
		break;
	case AU_OBJTYPE_SERVICE:
		temp_service = (au_service *)temp_notification->object;
		json_object_append_string(json_details, "host_name", &percent_escapes,
				temp_service->host_name);
		json_object_append_string(json_details, "description", &percent_escapes,
				temp_service->description);
		break;
		}

	json_object_append_string(json_details, "contact", &percent_escapes,
			temp_notification->contact->name);
	json_enumeration(json_details, format_options, "notification_type",
			temp_notification->notification_type, svm_au_notification_types);
	json_object_append_string(json_details, "method", &percent_escapes,
			temp_notification->method);
	json_object_append_string(json_details, "message", &percent_escapes,
			temp_notification->message);
	}

int json_archive_statechange_passes_selection(time_t timestamp,
		time_t end_time, int current_object_type, 
		int match_object_type, au_host *current_host, char *match_host, 
		au_service *current_service, char *match_service, 
		unsigned current_state_type, unsigned match_state_types) {

	if((end_time > 0) && (timestamp > end_time)) {
		return 0;
		}

	/* Skip if we're not interested in the current state type */
	if(!(current_state_type & match_state_types)) {
		return 0;
		}

	switch(current_object_type) {
	case AU_OBJTYPE_HOST:
		/* Skip if we're not interested in hosts */
		if(match_object_type != AU_OBJTYPE_HOST) {
			return 0;
			}

		/* Skip if user is not authorized for this host */
		if((NULL != current_host->hostp) && (FALSE == 
				is_authorized_for_host(current_host->hostp, 
				&current_authdata))) {
			return 0;
			}

		/* If a specific host name was specified, skip this host if it's 
			name does not match the one specified */
		if((NULL != match_host) && (NULL != current_host) && 
				strcmp(current_host->name, match_host)) {
			return 0;
			}

		break;

	case AU_OBJTYPE_SERVICE:
		/* Skip if we're not interested in services */
		if(match_object_type != AU_OBJTYPE_SERVICE) {
			return 0;
			}

		/* Skip if user is not authorized for this service */
		if((NULL != current_service->servicep) && (FALSE == 
				is_authorized_for_service(current_service->servicep, 
				&current_authdata))) {
			return 0;
			}

		/* If a specific service was specified, skip this service if it's 
			host name and service description do not match the one specified */
		if((NULL != match_host) && (NULL != match_service) && 
				(NULL != current_service) && 
				( strcmp(current_service->host_name, match_host) ||
				strcmp(current_service->description, match_service))) {
			return 0;
			}

		break;
		}

	return 1;
	}

json_object *json_archive_statechange_selectors(unsigned format_options, 
		int start, int count, time_t start_time, time_t end_time, 
		int object_type, char *host_name, char *service_description,
		unsigned state_types) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}

	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}

	if(start_time > 0) {
		json_object_append_time_t(json_selectors, "starttime", start_time);
		}

	if(end_time > 0) {
		json_object_append_time_t(json_selectors, "endtime", end_time);
		}

	if(NULL != host_name) {
		json_object_append_string(json_selectors, "hostname", &percent_escapes,
				host_name);
		}

	if(NULL != service_description) {
		json_object_append_string(json_selectors, "servicedescription", 
				&percent_escapes, service_description);
		}

	if(state_types != AU_STATETYPE_ALL) {
		json_bitmask(json_selectors, format_options, "statetypes", state_types, 
				valid_state_types);
		}

	return json_selectors;
	}

json_object *json_archive_statechangelist(unsigned format_options, 
		int start, int count, time_t start_time, time_t end_time, 
		int object_type, char *host_name, char *service_description, 
		int assumed_initial_host_state, int assumed_initial_service_state, 
		unsigned state_types, au_log *log) {

	json_object *json_data;
	json_array *json_statechangelist;
	json_object *json_statechange_details;
	au_node *temp_node;
	int initial_host_state = AU_STATE_NO_DATA;
	int	initial_service_state = AU_STATE_NO_DATA;
	au_log_entry *temp_entry = NULL;
	au_log_alert *temp_state_log = NULL;
	au_log_alert *start_log = NULL;
	au_log_alert *end_log = NULL;
	au_host *temp_host = NULL;
	au_service *temp_service = NULL;
	int have_seen_first_entry = 0;
	int current = 0;
	int counted = 0;

	if(assumed_initial_host_state != AU_STATE_NO_DATA)
		initial_host_state = assumed_initial_host_state;
	if(assumed_initial_service_state != AU_STATE_NO_DATA)
		initial_service_state = assumed_initial_service_state;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_archive_statechange_selectors(format_options, start, count, 
			start_time, end_time, object_type, host_name, service_description,
			state_types));

	json_statechangelist = json_new_array();

	for(temp_node = log->entry_list->head; temp_node != NULL; 
			temp_node = temp_node->next) {

		/* Skip all but notification type messages */
		temp_entry = (au_log_entry *)temp_node->data;
		if(!(temp_entry->entry_type & (AU_LOGTYPE_STATE | AU_LOGTYPE_ALERT))) {
			continue;
			}

		/* Get the host/service object */
		temp_state_log = (au_log_alert *)temp_entry->entry;
		switch(temp_state_log->obj_type) {
		case AU_OBJTYPE_HOST:
			temp_host = (au_host *)temp_state_log->object;
			temp_service = NULL;
			break;
		case AU_OBJTYPE_SERVICE:
			temp_host = NULL;
			temp_service = (au_service *)temp_state_log->object;
			break;
			}

		/* Skip any entries not passing the selectors */
		if(json_archive_statechange_passes_selection(temp_entry->timestamp, 
				end_time, temp_state_log->obj_type, 
				object_type, temp_host, host_name, temp_service, 
				service_description, temp_state_log->state_type, 
				state_types) == 0) {
			continue;
			}

		if((start_time > 0) && (temp_entry->timestamp < start_time)) {
			/* If we're before the start time and no initial assumed state
				was provided, record the state to be used as the initial
				state */
			switch(temp_state_log->obj_type) {
			case AU_OBJTYPE_HOST:
				if(AU_STATE_NO_DATA == assumed_initial_host_state)
					initial_host_state = temp_state_log->state;
				break;
			case AU_OBJTYPE_SERVICE:
				if(AU_STATE_NO_DATA == assumed_initial_service_state)
					initial_service_state = temp_state_log->state;
				break;
				}
			continue;
			}
		else if(0 == have_seen_first_entry) {
			/* When we get to the first entry after the start, inject a 
				pseudo entry with the initial state */
			switch(object_type) {
			case AU_OBJTYPE_HOST:
				start_log = au_create_alert_or_state_log(object_type, 
						temp_host, AU_STATETYPE_HARD, initial_host_state, 
						"Initial Host Pseudo-State");
				break;
			case AU_OBJTYPE_SERVICE:
				start_log = au_create_alert_or_state_log(object_type, 
						temp_service, AU_STATETYPE_HARD, initial_service_state, 
						"Initial Service Pseudo-State");
				break;
				}
			if(start_log != NULL) {
				json_statechange_details = json_new_object();
				json_archive_alert_details(json_statechange_details, 
						format_options, start_time, start_log);
				json_array_append_object(json_statechangelist, 
						json_statechange_details);
				au_free_alert_log(start_log);
				}
			have_seen_first_entry = 1;
			}
#if 0
		else {
			/* Ignore ALERT logs if their state is not different from the
				previous state */
			switch(object_type) {
			case AU_OBJTYPE_HOST:
//				if((temp_entry->entry_type & (AU_LOGTYPE_ALERT | 
//						AU_LOGTYPE_STATE_INITIAL)) && 
				if((temp_entry->entry_type & AU_LOGTYPE_ALERT) &&
						(last_host_state == temp_state_log->state)) {
					continue;
					}
				last_host_state = temp_state_log->state;
				break;
			case AU_OBJTYPE_SERVICE:
//				if((temp_entry->entry_type & (AU_LOGTYPE_ALERT | 
//						AU_LOGTYPE_STATE_INITIAL)) && 
				if((temp_entry->entry_type & AU_LOGTYPE_ALERT) &&
						(last_service_state == temp_state_log->state)) {
					continue;
					}
				last_service_state = temp_state_log->state;
				break;
				}
			}
#endif

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			json_statechange_details = json_new_object();
			json_archive_alert_details(json_statechange_details, 
					format_options, temp_entry->timestamp, temp_state_log);
			json_array_append_object(json_statechangelist, 
					json_statechange_details);
			counted++;
			}
		current++; 
		}

	/* Inject a pseudo entry with the final state */
	switch(object_type) {
	case AU_OBJTYPE_HOST:
		temp_host = au_find_host(log->hosts, host_name);
		if(NULL != temp_host) {
			end_log = au_create_alert_or_state_log(object_type,
					temp_host, AU_STATETYPE_HARD, temp_state_log->state,
					"Final Host Pseudo-State");
			}
		break;
	case AU_OBJTYPE_SERVICE:
		temp_service = au_find_service(log->hosts, host_name,
				service_description);
		if(NULL != temp_service) {
			end_log = au_create_alert_or_state_log(object_type,
					temp_service, AU_STATETYPE_HARD, temp_state_log->state,
					"Final Service Pseudo-State");
			}
		break;
		}
	if(end_log != NULL) {
		json_statechange_details = json_new_object();
		json_archive_alert_details(json_statechange_details, format_options, 
				end_time, end_log);
		json_array_append_object(json_statechangelist, 
				json_statechange_details);
		au_free_alert_log(end_log);
		}

	json_object_append_array(json_data, "statechangelist", 
			json_statechangelist);

	return json_data;
	}

/*
	Return the initial state of Nagios itself.
 */
int get_initial_nagios_state(au_linked_list *log_entries, time_t start_time, 
		time_t end_time) {

	au_node *temp_node;
	au_log_entry *current_log_entry;
	au_log_nagios *temp_nagios_log;
	int initial_state = AU_STATE_NO_DATA;

	for(temp_node = log_entries->head; temp_node != NULL; 
			temp_node = temp_node->next) {
		current_log_entry = (au_log_entry *)temp_node->data;
		if(current_log_entry->timestamp < start_time) {
			/* Any log entries prior to the start time tell us something
				about the initial state of Nagios so look at all of them */
			switch(current_log_entry->entry_type) {
			case AU_LOGTYPE_NAGIOS:
				/* If the log is a Nagios start or stop log, that is the
					current program state */
				temp_nagios_log = (au_log_nagios *)current_log_entry->entry;
				initial_state = temp_nagios_log->type;
				break;
			default:
				/* Any other log indicates that Nagios is running */
				initial_state = AU_STATE_PROGRAM_START;
				break;
				}
			}
		else {
			if(AU_STATE_NO_DATA != initial_state) {
				/* Once we cross the threshold of the start time, if we have
					an initial state, that is THE initial state */
				return initial_state;
				}
			else {
				/* Otherwise the first log encountered tells us the initial
					state of Nagios */
				switch(current_log_entry->entry_type) {
				case AU_LOGTYPE_NAGIOS:
					/* If the log is a Nagios start or stop log, that is 
						opposite the initial state */
					temp_nagios_log = (au_log_nagios *)current_log_entry->entry;
					switch(temp_nagios_log->type) {
					case AU_STATE_PROGRAM_START:
						return AU_STATE_PROGRAM_END;
						break;
					case AU_STATE_PROGRAM_END:
						return AU_STATE_PROGRAM_START;
						break;
						}
					break;
				default:
					/* Any other log indicates that Nagios was running at
						the start time */
					return AU_STATE_PROGRAM_START;
					break;
					}
				}
			}
		}
		/* If we reach this point, we are in bad shape because we had nothing
			in the logs and the initial state should still be AU_STATE_NO_DATA,
			which we return as an error indication */
		return initial_state;
	}

/*
	Return the initial downtime state of the host or service.
 */
int get_initial_downtime_state(au_linked_list *log_entries, time_t start_time, 
		time_t end_time) {

	au_node *temp_node;
	au_log_entry *current_log_entry;
	au_log_downtime *temp_downtime_log;
	int initial_state = AU_STATE_NO_DATA;

	for(temp_node = log_entries->head; temp_node != NULL; 
			temp_node = temp_node->next) {
		current_log_entry = (au_log_entry *)temp_node->data;
		if(current_log_entry->timestamp < start_time) {
			/* Any downtime log prior to the start time may indicate the 
				initial downtime state so look at all of them */
			if(AU_LOGTYPE_DOWNTIME == current_log_entry->entry_type) {
				temp_downtime_log = (au_log_downtime *)current_log_entry->entry;
				initial_state = temp_downtime_log->downtime_type;
				}
			}
		else if(current_log_entry->timestamp <= end_time) {
			if(AU_STATE_NO_DATA != initial_state) {
				/* Once we cross the start time, if we have a downtime state,
					we have THE initial downtime state */
				return initial_state;
				}
			else {
				/* If we dont' have a downtime state yet, the first downtime
					state we encounter will be opposite the initial downtime
					state */
				if(AU_LOGTYPE_NOTIFICATION == current_log_entry->entry_type) {
					temp_downtime_log = (au_log_downtime *)current_log_entry->entry;
					switch(temp_downtime_log->downtime_type) {
					case AU_STATE_DOWNTIME_START:
						return AU_STATE_DOWNTIME_END;
						break;
					case AU_STATE_DOWNTIME_END:
						return AU_STATE_DOWNTIME_START;
						break;
						}
					}
				}
			}
		}
		/* If we haven't encountered any indication of downtime yet, assume
			we were not initially in downtime */
		if(AU_STATE_NO_DATA == initial_state) {
			initial_state = AU_STATE_DOWNTIME_END;
			}

		return initial_state;
	}

/*
	Return the initial state of the host or service.
 */
int get_initial_subject_state(au_linked_list *log_entries, time_t start_time, 
		time_t end_time) {

	au_node *temp_node;
	au_log_entry *current_log_entry;
	int initial_state = AU_STATE_NO_DATA;

	for(temp_node = log_entries->head; temp_node != NULL; 
			temp_node = temp_node->next) {
		current_log_entry = (au_log_entry *)temp_node->data;
		if(current_log_entry->timestamp < start_time) {
			/* Any state log prior to the start time may indicate the
				initial state of the subject so look at all of them */
			switch(current_log_entry->entry_type) {
			case AU_LOGTYPE_ALERT:
			case AU_LOGTYPE_STATE_INITIAL:
			case AU_LOGTYPE_STATE_CURRENT:
				initial_state = get_log_entry_state(current_log_entry);
				break;
				}
			}
		}
		return initial_state;
	}

int get_log_entry_state(au_log_entry *log_entry) {

	switch(log_entry->entry_type) {
	case AU_LOGTYPE_ALERT:
	case AU_LOGTYPE_STATE_INITIAL:
	case AU_LOGTYPE_STATE_CURRENT:
		return ((au_log_alert *)log_entry->entry)->state;
		break;
	case AU_LOGTYPE_DOWNTIME:
		return ((au_log_downtime *)log_entry->entry)->downtime_type;
		break;
	case AU_LOGTYPE_NAGIOS:
		return ((au_log_nagios *)log_entry->entry)->type;
		break;
	case AU_LOGTYPE_NOTIFICATION:
		return ((au_log_notification *)log_entry->entry)->notification_type;
		break;
	default:
		return AU_STATE_NO_DATA;
		break;
		}
	}

#define have_real_data(a)	have_data((a), \
	~(AU_STATE_NO_DATA | AU_STATE_PROGRAM_START | AU_STATE_PROGRAM_END))
#define have_state_data(a)	have_data((a), AU_STATE_ALL)

int have_data(au_linked_list *log_entries, int mask) {

	au_node *temp_node;
	int state;

	for(temp_node = log_entries->head; temp_node != NULL; 
			temp_node = temp_node->next) {
		state = get_log_entry_state((au_log_entry *)temp_node->data);
		if(state & mask) return TRUE;
		}
	return FALSE;
	}

unsigned long calculate_window_duration(time_t start_time, time_t end_time, 
		timeperiod *report_timeperiod) {

	struct tm *t;
	unsigned long state_duration;
	time_t midnight;
	int weekday;
	timerange *temp_timerange;
	unsigned long temp_duration;
	unsigned long temp_end;
	unsigned long temp_start;
	unsigned long start;
	unsigned long end;

	/* MickeM - attempt to handle the the report time_period (if any) */
	if(report_timeperiod != NULL) {
		t = localtime((time_t *)&start_time);
		state_duration = 0;

		/* calculate the start of the day (midnight, 00:00 hours) */
		t->tm_sec = 0;
		t->tm_min = 0;
		t->tm_hour = 0;
		t->tm_isdst = -1;
		midnight = mktime(t);
		weekday = t->tm_wday;

		while(midnight < end_time) {
			temp_duration = 0;
			temp_end = min(86400, end_time - midnight);
			temp_start = 0;
			if(start_time > midnight) {
				temp_start = start_time - midnight;
				}
#ifdef DEBUG
			printf("Matching: %ld -> %ld. (%ld -> %ld)\n", temp_start, 
					temp_end, midnight + temp_start, midnight + temp_end);
#endif
			/* check all time ranges for this day of the week */
			for(temp_timerange = report_timeperiod->days[weekday]; 
					temp_timerange != NULL; 
					temp_timerange = temp_timerange->next) {
#ifdef DEBUG
				printf("Matching in timerange[%d]: %lu -> %lu (%lu -> %lu)\n", 
						weekday, temp_timerange->range_start, 
						temp_timerange->range_end, temp_start, temp_end);
#endif

				start = max(temp_timerange->range_start, temp_start);
				end = min(temp_timerange->range_end, temp_end);

				if(start < end) {
					temp_duration += end - start;
#ifdef DEBUG
					printf("Matched time: %ld -> %ld = %lu\n", start, end, 
							temp_duration);
#endif
					}
#ifdef DEBUG
				else {
					printf("Ignored time: %ld -> %ld\n", start, end);
					}
#endif
				}
			state_duration += temp_duration;
			temp_start = 0;
			midnight += 86400;
			if(++weekday > 6) weekday = 0;
			}
		}

	/* No report timeperiod was requested; assume 24x7 */
	else {
		/* Calculate time in this state */
		state_duration = (unsigned long)(end_time - start_time);
		}

	return state_duration;
	}

#ifdef DEBUG
void print_availability(au_availability *availability, const char *padding) {

	printf("%sAll::           up:          %10lu\n", padding,
			availability->time_up);
	printf("%s                down:        %10lu\n", padding,
			availability->time_down);
	printf("%s                unreachable: %10lu\n", padding,
			availability->time_unreachable);
	printf("%s                ok:          %10lu\n", padding,
			availability->time_ok);
	printf("%s                warning:     %10lu\n", padding,
			availability->time_warning);
	printf("%s                critical:    %10lu\n", padding,
			availability->time_critical);
	printf("%s                unknown:     %10lu\n", padding,
			availability->time_unknown); 
	printf("%sScheduled::     up:          %10lu\n", padding,
			availability->scheduled_time_up);
	printf("%s                down:        %10lu\n", padding,
			availability->scheduled_time_down);
	printf("%s                unreachable: %10lu\n", padding,
			availability->scheduled_time_unreachable);
	printf("%s                ok:          %10lu\n", padding,
			availability->scheduled_time_ok);
	printf("%s                warning:     %10lu\n", padding,
			availability->scheduled_time_warning);
	printf("%s                critical:    %10lu\n", padding,
			availability->scheduled_time_critical);
	printf("%s                unknown:     %10lu\n", padding,
			availability->scheduled_time_unknown); 
	printf("%sIndeterminate:: scheduled:   %10lu\n", padding,
			availability->scheduled_time_indeterminate);
	printf("%s                nodata:      %10lu\n", padding,
			availability->time_indeterminate_nodata);
	printf("%s                notrunning:  %10lu\n", padding,
			availability->time_indeterminate_notrunning); 
	}
#endif

void compute_window_availability(time_t start_time, time_t end_time, 
		int last_subject_state, int last_downtime_state, int last_nagios_state,
		timeperiod *report_timeperiod, au_availability *availability, 
		int assume_state_during_nagios_downtime, int object_type, 
		int assume_state_retention) {

	unsigned long state_duration;

#ifdef DEBUG
printf( "    %lu to %lu (%lus): %s/%s/%s\n", start_time, end_time, 
		(end_time - start_time),
		svm_get_string_from_value(last_subject_state, svm_au_states),
		svm_get_string_from_value(last_downtime_state, svm_au_states),
		svm_get_string_from_value(last_nagios_state, svm_au_states));
#endif

	/* Get the duration of this window as adjusted for the report timeperiod */
	state_duration = calculate_window_duration(start_time, end_time, 
			report_timeperiod);

	/* Determine the appropriate state */
	switch(last_nagios_state) {
	case AU_STATE_PROGRAM_START:
		switch(last_downtime_state) {
		case AU_STATE_DOWNTIME_START:
			switch(last_subject_state) {
			case AU_STATE_NO_DATA:
				availability->time_indeterminate_nodata += state_duration;
				break;
			case AU_STATE_HOST_UP:
				availability->scheduled_time_up += state_duration;
				break;
			case AU_STATE_HOST_DOWN:
				availability->scheduled_time_down += state_duration;
				break;
			case AU_STATE_HOST_UNREACHABLE:
				availability->scheduled_time_unreachable += state_duration;
				break;
			case AU_STATE_SERVICE_OK:
				availability->scheduled_time_ok += state_duration;
				break;
			case AU_STATE_SERVICE_WARNING:
				availability->scheduled_time_warning += state_duration;
				break;
			case AU_STATE_SERVICE_UNKNOWN:
				availability->scheduled_time_unknown += state_duration;
				break;
			case AU_STATE_SERVICE_CRITICAL:
				availability->scheduled_time_critical += state_duration;
				break;
				}
			break;
		case AU_STATE_DOWNTIME_END:
			switch(last_subject_state) {
			case AU_STATE_NO_DATA:
				availability->time_indeterminate_nodata += state_duration;
				break;
			case AU_STATE_HOST_UP:
				availability->time_up += state_duration;
				break;
			case AU_STATE_HOST_DOWN:
				availability->time_down += state_duration;
				break;
			case AU_STATE_HOST_UNREACHABLE:
				availability->time_unreachable += state_duration;
				break;
			case AU_STATE_SERVICE_OK:
				availability->time_ok += state_duration;
				break;
			case AU_STATE_SERVICE_WARNING:
				availability->time_warning += state_duration;
				break;
			case AU_STATE_SERVICE_UNKNOWN:
				availability->time_unknown += state_duration;
				break;
			case AU_STATE_SERVICE_CRITICAL:
				availability->time_critical += state_duration;
				break;
				}
			break;
			}
		break;
	case AU_STATE_PROGRAM_END:
		if(assume_state_during_nagios_downtime) {
			switch(last_downtime_state) {
			case AU_STATE_DOWNTIME_START:
				switch(last_subject_state) {
				case AU_STATE_NO_DATA:
					availability->time_indeterminate_nodata += state_duration;
					break;
				case AU_STATE_HOST_UP:
					availability->scheduled_time_up += state_duration;
					break;
				case AU_STATE_HOST_DOWN:
					availability->scheduled_time_down += state_duration;
					break;
				case AU_STATE_HOST_UNREACHABLE:
					availability->scheduled_time_unreachable += state_duration;
					break;
				case AU_STATE_SERVICE_OK:
					availability->scheduled_time_ok += state_duration;
					break;
				case AU_STATE_SERVICE_WARNING:
					availability->scheduled_time_warning += state_duration;
					break;
				case AU_STATE_SERVICE_UNKNOWN:
					availability->scheduled_time_unknown += state_duration;
					break;
				case AU_STATE_SERVICE_CRITICAL:
					availability->scheduled_time_critical += state_duration;
					break;
					}
				break;
			case AU_STATE_DOWNTIME_END:
				switch(last_subject_state) {
				case AU_STATE_NO_DATA:
					availability->time_indeterminate_nodata += state_duration;
					break;
				case AU_STATE_HOST_UP:
					availability->time_up += state_duration;
					break;
				case AU_STATE_HOST_DOWN:
					availability->time_down += state_duration;
					break;
				case AU_STATE_HOST_UNREACHABLE:
					availability->time_unreachable += state_duration;
					break;
				case AU_STATE_SERVICE_OK:
					availability->time_ok += state_duration;
					break;
				case AU_STATE_SERVICE_WARNING:
					availability->time_warning += state_duration;
					break;
				case AU_STATE_SERVICE_UNKNOWN:
					availability->time_unknown += state_duration;
					break;
				case AU_STATE_SERVICE_CRITICAL:
					availability->time_critical += state_duration;
					break;
					}
				break;
				}
			}
		else {
			availability->time_indeterminate_notrunning += state_duration;
			}
		break;
		}

#ifdef DEBUG
	print_availability(availability, "    ");
#endif

	return;
	}

static void compute_availability(au_linked_list *log_entries, time_t query_time,
		time_t start_time, time_t end_time, timeperiod *report_timeperiod, 
		au_availability *availability, int initial_subject_state,
		int assume_state_during_nagios_downtime, int object_type,
		int assume_state_retention) {

	int current_nagios_state;
	int last_nagios_state;
	int current_downtime_state;
	int last_downtime_state;
	int current_subject_state = initial_subject_state;
	int last_subject_state = AU_STATE_NO_DATA;
	au_node *temp_node;
	au_log_entry *current_log_entry;
	au_log_entry *last_log_entry = NULL;
	au_log_alert *temp_alert_log;
	au_log_downtime *temp_downtime_log;
	au_log_nagios *temp_nagios_log;
	time_t last_time = start_time;
	time_t current_time;

#ifdef DEBUG
	printf("  initial state: %s\n",
			svm_get_string_from_value(initial_subject_state, valid_states));
#endif

	/* Determine the initial Nagios state */
	if((last_nagios_state = current_nagios_state = 
			get_initial_nagios_state(log_entries, start_time, end_time)) 
			== AU_STATE_NO_DATA) {
		/* This is bad; I'm giving up. */
		return;
		}

#ifdef DEBUG
	printf("  initial nagios state: %s\n",
			svm_get_string_from_value(last_nagios_state, valid_states));
#endif

	/* Determine the initial downtime state */
	if((last_downtime_state = current_downtime_state = 
			get_initial_downtime_state(log_entries, start_time, end_time)) 
			== AU_STATE_NO_DATA) {
		/* This is bad; I'm giving up. */
		return;
		}

#ifdef DEBUG
	printf("  initial downtime state: %s\n",
			svm_get_string_from_value(last_downtime_state, valid_states));
#endif


	/* Process all entry pairs */
	for(temp_node = log_entries->head; temp_node != NULL; 
			temp_node = temp_node->next) {
		current_log_entry = (au_log_entry *)temp_node->data;

		/* Skip everything before the start of the requested query window */
		if(current_log_entry->timestamp < start_time) continue;

#ifdef DEBUG
		printf("  Got log of type \"%s\" at %lu\n", 
				svm_get_string_from_value(current_log_entry->entry_type, 
				svm_au_log_types), current_log_entry->timestamp);
#endif

		/* Update states */
		last_subject_state = current_subject_state;
		last_downtime_state = current_downtime_state;
		last_nagios_state = current_nagios_state;

		switch(current_log_entry->entry_type) {
		case AU_LOGTYPE_ALERT:
		case AU_LOGTYPE_STATE_INITIAL:
		case AU_LOGTYPE_STATE_CURRENT:
			temp_alert_log = (au_log_alert *)current_log_entry->entry;
			current_subject_state = temp_alert_log->state;
#ifdef DEBUG
			printf("  Current alert state: \"%s\"\n", 
					svm_get_string_from_value(current_subject_state, 
					valid_states));
#endif
			break;
		case AU_LOGTYPE_DOWNTIME:
			temp_downtime_log = (au_log_downtime *)current_log_entry->entry;
			current_downtime_state = temp_downtime_log->downtime_type;
#ifdef DEBUG
			printf("  Current downtime state: \"%s\"\n", 
					svm_get_string_from_value(current_downtime_state, 
					valid_states));
#endif
			break;
		case AU_LOGTYPE_NAGIOS:
			temp_nagios_log = (au_log_nagios *)current_log_entry->entry;
			current_nagios_state = temp_nagios_log->type;
#ifdef DEBUG
			printf("  Current nagios state: \"%s\"\n", 
					svm_get_string_from_value(current_nagios_state, 
					valid_states));
#endif
			break;
		default:
			continue;
			break;
			}

		/* Record the time ranges for the current log pair */
		if(NULL != last_log_entry) last_time = last_log_entry->timestamp;
		current_time = current_log_entry->timestamp;

		/* Bail out if we've already passed the end of the requested time
			range*/
		if(last_time > end_time) break;

		/* Clip last time if it extends beyond the end of the requested
			time range */
		if(current_time > end_time) current_time = end_time;

		/* Clip first time if it precedes the start of the requested 
			time range */
		if(last_time < start_time) last_time = start_time;

		/* compute availability times for this chunk */
		compute_window_availability(last_time, current_time, last_subject_state,
				last_downtime_state, last_nagios_state, report_timeperiod, 
				availability, assume_state_during_nagios_downtime, object_type, 
				assume_state_retention);

		/* Return if we've reached the end of the requested time 
			range */
		if(current_time >= end_time) {
			last_log_entry = current_log_entry;
			break;
			}

		/* Keep track of the last item */
		last_log_entry = current_log_entry;
		}

	/* Process the last entry */
	if(last_log_entry != NULL) {
		/* Update states */
		last_subject_state = current_subject_state;
		last_downtime_state = current_downtime_state;
		last_nagios_state = current_nagios_state;

		/* Don't process an entry that is beyond the end of the requested
			time range */
		if(last_log_entry->timestamp < end_time) {
			current_time = query_time;
			if(current_time > end_time) current_time = end_time;

			last_time = last_log_entry->timestamp;
			if(last_time < start_time) last_time = start_time;

			/* compute availability times for last state */
			compute_window_availability(last_time, current_time, 
					last_subject_state, last_downtime_state, last_nagios_state, 
					report_timeperiod, availability, 
					assume_state_during_nagios_downtime, object_type, 
					assume_state_retention);
			}
		}
	else {
		/* There were no log entries for the entire queried time, therefore
			the whole query window was the same state */
		compute_window_availability(start_time, end_time, initial_subject_state,
				last_downtime_state, last_nagios_state, report_timeperiod,
				availability, assume_state_during_nagios_downtime, object_type, 
				assume_state_retention);
		}
	}


void compute_host_availability(time_t query_time, time_t start_time, 
		time_t end_time, au_log *log, au_host *host, 
		timeperiod *report_timeperiod, int assume_initial_state, 
		int assumed_initial_host_state, 
		int assume_state_during_nagios_downtime, int assume_state_retention) {

	hoststatus *host_status = NULL;
	int	last_known_state = AU_STATE_NO_DATA;
	int	initial_host_state = AU_STATE_NO_DATA;

#ifdef DEBUG
	printf("Computing availability for host %s\n    from %lu to %lu\n",
			host->name, start_time, end_time);
#endif

	/* If the start time is in the future, we can't compute anything */
	if(start_time > query_time) return;

	/* Get current host status if possible */
	host_status = find_hoststatus(host->name);

	/* If we don't have any data, the query time falls within the requested
		query window, and we have the current host status, insert the 
		current state as the pseudo state at the start of the requested
		query window. */
	if((have_state_data(host->log_entries) == FALSE) && 
			(query_time > start_time) && (query_time <= end_time) && 
			(host_status != NULL)) {
		switch(host_status->status) {
		case HOST_DOWN:
			last_known_state = AU_STATE_HOST_DOWN;
			break;
		case HOST_UNREACHABLE:
			last_known_state = AU_STATE_HOST_UNREACHABLE;
			break;
		case HOST_UP:
			last_known_state = AU_STATE_HOST_UP;
			break;
		default:
			last_known_state = AU_STATE_NO_DATA;
			break;
			}

		if(last_known_state != AU_STATE_NO_DATA) {
			/* Add a dummy current state item */
#ifdef DEBUG
			printf("  Inserting %s as current service pseudo-state\n",
					svm_get_string_from_value(last_known_state, valid_states));
#endif
			(void)au_add_alert_or_state_log(log, start_time, 
					AU_LOGTYPE_STATE_CURRENT, AU_OBJTYPE_HOST, host, 
					AU_STATETYPE_HARD, last_known_state, 
					"Current Host Pseudo-State");
			}
		}

	/* Next determine the initial state of the host */
	initial_host_state = get_initial_subject_state(host->log_entries, 
			start_time, end_time);
#ifdef DEBUG
	printf("  Initial state from logs: %s\n",
			svm_get_string_from_value(initial_host_state, valid_states));
#endif

	if((AU_STATE_NO_DATA == initial_host_state) && 
			(1 == assume_initial_state)) {
		switch(assumed_initial_host_state) {
		case AU_STATE_HOST_UP:
		case AU_STATE_HOST_DOWN:
		case AU_STATE_HOST_UNREACHABLE:
			initial_host_state = assumed_initial_host_state;
			break;
		case AU_STATE_CURRENT_STATE:
			if(host_status != NULL) {
				switch(host_status->status) {
				case HOST_DOWN:
					initial_host_state = AU_STATE_HOST_DOWN;
					break;
				case HOST_UNREACHABLE:
					initial_host_state = AU_STATE_HOST_UNREACHABLE;
					break;
				case HOST_UP:
					initial_host_state = AU_STATE_HOST_UP;
					break;
					}
				}
			break;
			}
		}

	/* At this point, if we still don't have any real data, we can't compute
		anything, so return */
	if((have_real_data(host->log_entries) != TRUE) && 
			(AU_STATE_NO_DATA == initial_host_state)) return;

	/* Compute the availability for every entry in the host's list of log
		entries */
	compute_availability(host->log_entries, query_time, start_time, end_time, 
			report_timeperiod, host->availability, initial_host_state,
			assume_state_during_nagios_downtime, AU_OBJTYPE_HOST,
			assume_state_retention);

	}

void compute_service_availability(time_t query_time, time_t start_time, 
		time_t end_time, au_log *log, au_service *service, 
		timeperiod *report_timeperiod, int assume_initial_state, 
		int assumed_initial_service_state, 
		int assume_state_during_nagios_downtime, int assume_state_retention) {

	servicestatus *service_status = NULL;
	int	last_known_state = AU_STATE_NO_DATA;
	int	initial_service_state = AU_STATE_NO_DATA;

#ifdef DEBUG
	printf("Computing availability for service %s:%s\n    from %lu to %lu\n",
			service->host_name, service->description, start_time, end_time);
#endif

	/* If the start time is in the future, we can't compute anything */
	if(start_time > query_time) return;

	/* Get current service status if possible */
	service_status = find_servicestatus(service->host_name, 
			service->description);

	/* If we don't have any data, the query time falls within the requested
		query window, and we have the current service status, insert the 
		current state as the pseudo state at the start of the requested
		query window. */
	if((have_state_data(service->log_entries) == FALSE) && 
			(query_time > start_time) && (query_time <= end_time) && 
			(service_status != NULL)) {
		switch(service_status->status) {
		case SERVICE_OK:
			last_known_state = AU_STATE_SERVICE_OK;
			break;
		case SERVICE_WARNING:
			last_known_state = AU_STATE_SERVICE_WARNING;
			break;
		case SERVICE_CRITICAL:
			last_known_state = AU_STATE_SERVICE_CRITICAL;
			break;
		case SERVICE_UNKNOWN:
			last_known_state = AU_STATE_SERVICE_UNKNOWN;
			break;
		default:
			last_known_state = AU_STATE_NO_DATA;
			break;
			}

		if(last_known_state != AU_STATE_NO_DATA) {
			/* Add a dummy current state item */
#ifdef DEBUG
			printf("  Inserting %s as current service pseudo-state\n",
					svm_get_string_from_value(last_known_state, valid_states));
#endif
			(void)au_add_alert_or_state_log(log, start_time, 
					AU_LOGTYPE_STATE_CURRENT, AU_OBJTYPE_SERVICE, service, 
					AU_STATETYPE_HARD, last_known_state, 
					"Current Service Pseudo-State");
			}
		}

	/* Next determine the initial state of the service */
	initial_service_state = get_initial_subject_state(service->log_entries, 
			start_time, end_time);
#ifdef DEBUG
	printf("  Initial state from logs: %s\n",
			svm_get_string_from_value(initial_service_state, valid_states));
#endif
	if((AU_STATE_NO_DATA == initial_service_state) && 
			(1 == assume_initial_state)) {
		switch(assumed_initial_service_state) {
		case AU_STATE_SERVICE_OK:
		case AU_STATE_SERVICE_WARNING:
		case AU_STATE_SERVICE_UNKNOWN:
		case AU_STATE_SERVICE_CRITICAL:
			initial_service_state = assumed_initial_service_state;
			break;
		case AU_STATE_CURRENT_STATE:
			if(service_status == NULL) {
				switch(service_status->status) {
				case SERVICE_OK:
					initial_service_state = AU_STATE_SERVICE_OK;
					break;
				case SERVICE_WARNING:
					initial_service_state = AU_STATE_SERVICE_WARNING;
					break;
				case SERVICE_UNKNOWN:
					initial_service_state = AU_STATE_SERVICE_UNKNOWN;
					break;
				case SERVICE_CRITICAL:
					initial_service_state = AU_STATE_SERVICE_CRITICAL;
					break;
					}
				}
			break;
			}
		}

	/* At this point, if we still don't have any real data, we can't compute
		anything, so return */
	if((have_real_data(service->log_entries) != TRUE) && 
			(AU_STATE_NO_DATA == initial_service_state)) return;

	/* Compute the availability for every entry in the service's list of log
		entries */
	compute_availability(service->log_entries, query_time, start_time, end_time,
			report_timeperiod, service->availability, initial_service_state,
			assume_state_during_nagios_downtime, AU_OBJTYPE_SERVICE,
			assume_state_retention);

	}

json_object *json_archive_availability_selectors(unsigned format_options, 
		time_t start_time, time_t end_time, int availability_object_type, 
		char *host_name, char *service_description, hostgroup *hostgroup,
		servicegroup *servicegroup, timeperiod *report_timeperiod,
		int assume_initial_state, int assume_state_retention, 
		int assume_state_during_nagios_downtime, 
		int assumed_initial_host_state, int assumed_initial_service_state, 
		unsigned state_types) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	json_enumeration(json_selectors, format_options, "availabilityobjecttype", 
			availability_object_type, valid_availability_object_types);

	if(start_time > 0) {
		json_object_append_time_t(json_selectors, "starttime", start_time);
		}

	if(end_time > 0) {
		json_object_append_time_t(json_selectors, "endtime", end_time);
		}

	if(NULL != host_name) {
		json_object_append_string(json_selectors, "hostname", &percent_escapes,
				host_name);
		}

	if(NULL != service_description) {
		json_object_append_string(json_selectors, "servicedescription", 
				&percent_escapes, service_description);
		}

	if(NULL != hostgroup) {
		json_object_append_string(json_selectors, "hostgroup", &percent_escapes,
				hostgroup->group_name);
		}

	if(NULL != servicegroup) {
		json_object_append_string(json_selectors, "servicegroup",
				&percent_escapes, servicegroup->group_name);
		}

	if(NULL != report_timeperiod) {
		json_object_append_string(json_selectors, "timeperiod",
				&percent_escapes, report_timeperiod->name);
		}

	json_object_append_boolean(json_selectors, "assumeinitialstate", 
			assume_initial_state);
	json_object_append_boolean(json_selectors, "assumestateretention", 
			assume_state_retention);
	json_object_append_boolean(json_selectors, 
			"assumestateduringnagiosdowntime", 
			assume_state_during_nagios_downtime); 
	if(assumed_initial_host_state != AU_STATE_NO_DATA) {
		json_enumeration(json_selectors, format_options, 
			"assumedinitialhoststate", assumed_initial_host_state, 
			valid_initial_host_states);
		}

	if(assumed_initial_service_state != AU_STATE_NO_DATA) {
		json_enumeration(json_selectors, format_options, 
			"assumedinitialservicestate", assumed_initial_service_state, 
			valid_initial_service_states);
		}

	if(state_types != AU_STATETYPE_ALL) {
		json_bitmask(json_selectors, format_options, "statetypes", state_types, 
				valid_state_types);
		}

	return json_selectors;
	}

json_object *json_archive_availability(unsigned format_options, 
		time_t query_time, time_t start_time, time_t end_time, 
		int availability_object_type, char *host_name, 
		char *service_description, hostgroup *hostgroup_selector, 
		servicegroup *servicegroup_selector, timeperiod *report_timeperiod, 
		int assume_initial_state, int assume_state_retention, 
		int assume_state_during_nagios_downtime, 
		int assumed_initial_host_state, int assumed_initial_service_state, 
		unsigned state_types, au_log *log) {

	json_object *json_data;

	host *temp_host;
	json_array *json_host_list;
	json_object *json_host_object;

	service *temp_service;
	json_array *json_service_list;
	json_object *json_service_object;

	hostgroup *temp_hostgroup;
	json_object *json_hostgroup_object;
	json_array *json_hostgroup_list;
	hostsmember *temp_hostgroup_member;

	servicegroup *temp_servicegroup;
	json_object *json_servicegroup_object;
	json_array *json_servicegroup_list;
	servicesmember *temp_servicegroup_member;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_archive_availability_selectors(format_options, start_time, 
			end_time, availability_object_type, host_name, service_description, 
			hostgroup_selector, servicegroup_selector, report_timeperiod, 
			assume_initial_state, assume_state_retention, 
			assume_state_during_nagios_downtime, assumed_initial_host_state, 
			assumed_initial_service_state, state_types));

	/* First compute the availability */
	switch(availability_object_type) {
		case AU_OBJTYPE_HOST:
			if(NULL == host_name) {		/* compute for all hosts */
				json_host_list = json_new_array();
				for(temp_host = host_list; temp_host != NULL; 
						temp_host = temp_host->next) {
					if(FALSE == is_authorized_for_host(temp_host, 
							&current_authdata)) {
						continue;
						}
					json_host_object = 
							json_archive_single_host_availability(
							format_options, query_time, start_time, end_time, 
							temp_host->name, report_timeperiod, 
							assume_initial_state, assume_state_retention, 
							assume_state_during_nagios_downtime, 
							assumed_initial_host_state, state_types, log);
					if(NULL != json_host_object) {
						json_array_append_object(json_host_list, 
								json_host_object);
						}
					}
				json_object_append_array(json_data, "hosts", json_host_list);
				}
			else {
				temp_host = find_host(host_name);
				if((NULL != temp_host) && 
						(TRUE == is_authorized_for_host(temp_host, 
						&current_authdata))) {
					json_host_object = 
							json_archive_single_host_availability(format_options, 
							query_time, start_time, end_time, host_name, 
							report_timeperiod, assume_initial_state, 
							assume_state_retention, 
							assume_state_during_nagios_downtime, 
							assumed_initial_host_state, state_types, log);
					if(NULL != json_host_object) {
						json_object_append_object(json_data, "host", 
								json_host_object);
						}
					}
				}
			break;
		case AU_OBJTYPE_SERVICE:
			if((NULL == host_name) && (NULL == service_description)) {	
				/* compute for all services on all hosts */
				json_service_list = json_new_array();
				for(temp_service = service_list; temp_service != NULL; 
						temp_service = temp_service->next) {
					if(FALSE == is_authorized_for_service(temp_service,
							&current_authdata)) {
						continue;
						}
					json_service_object = 
							json_archive_single_service_availability(
							format_options, query_time, start_time, end_time, 
							temp_service->host_name, temp_service->description, 
							report_timeperiod, assume_initial_state, 
							assume_state_retention, 
							assume_state_during_nagios_downtime, 
							assumed_initial_service_state, state_types, log);
					if(NULL != json_service_object) {
						json_array_append_object(json_service_list, 
								json_service_object);
						}
					}
				json_object_append_array(json_data, "services", 
						json_service_list);
				}
			else if(NULL == service_description) {	
				/* compute for all services on the specified host */
				json_service_list = json_new_array();
				for(temp_service = service_list; temp_service != NULL; 
						temp_service = temp_service->next) {
					if(!strcmp(temp_service->host_name, host_name) &&
							(TRUE == is_authorized_for_service(temp_service,
							&current_authdata))) {
						json_service_object = 
								json_archive_single_service_availability(
								format_options, query_time, start_time, 
								end_time, temp_service->host_name, 
								temp_service->description, report_timeperiod, 
								assume_initial_state, assume_state_retention, 
								assume_state_during_nagios_downtime, 
								assumed_initial_service_state, state_types, 
								log);
						if(NULL != json_service_object) {
							json_array_append_object(json_service_list, 
									json_service_object);
							}
						}
					}
				json_object_append_array(json_data, "services", 
						json_service_list);
				}
			else if(NULL == host_name) {	
				/* compute for all services on the specified host */
				json_service_list = json_new_array();
				for(temp_service = service_list; temp_service != NULL; 
						temp_service = temp_service->next) {
					if(!strcmp(temp_service->description, 
							service_description) &&
							(TRUE == is_authorized_for_service(temp_service,
							&current_authdata))) {
						json_service_object = 
								json_archive_single_service_availability(
								format_options, query_time, start_time, 
								end_time, temp_service->host_name, 
								temp_service->description, report_timeperiod, 
								assume_initial_state, assume_state_retention, 
								assume_state_during_nagios_downtime, 
								assumed_initial_service_state, state_types, 
								log);
						if(NULL != json_service_object) {
							json_array_append_object(json_service_list, 
									json_service_object);
							}
						}
					}
				json_object_append_array(json_data, "services", 
						json_service_list);
				}
			else {
				temp_service = find_service(host_name, service_description);
				if((NULL != temp_service) && 
						(TRUE == is_authorized_for_service(temp_service,
						&current_authdata))) {
					json_service_object = 
							json_archive_single_service_availability(format_options,
							query_time, start_time, end_time, host_name, 
							service_description, report_timeperiod, 
							assume_initial_state, assume_state_retention, 
							assume_state_during_nagios_downtime, 
							assumed_initial_service_state, state_types, log);
					if(NULL != json_service_object) {
						json_object_append_object(json_data, "service", 
								json_service_object);
						}
					}
				}
			break;
		case AU_OBJTYPE_HOSTGROUP:
			if(NULL == hostgroup_selector) {
				/* compute for all hosts in all hostgroups */
				json_hostgroup_list = json_new_array();
				for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; 
						temp_hostgroup = temp_hostgroup->next) {
					json_host_list = json_new_array();
					for(temp_hostgroup_member = temp_hostgroup->members; 
							temp_hostgroup_member != NULL; 
							temp_hostgroup_member = 
							temp_hostgroup_member->next) {
						if(FALSE == is_authorized_for_host(temp_hostgroup_member->host_ptr, 
								&current_authdata)) {
							continue;
							}
						json_host_object = 
								json_archive_single_host_availability(
								format_options, query_time, start_time, 
								end_time, temp_hostgroup_member->host_name, 
								report_timeperiod, assume_initial_state, 
								assume_state_retention, 
								assume_state_during_nagios_downtime, 
								assumed_initial_host_state, state_types, log);
						if(NULL != json_host_object) {
							json_array_append_object(json_host_list, 
									json_host_object);
							}
						}
					json_hostgroup_object = json_new_object();
					json_object_append_string(json_hostgroup_object, "name",
							&percent_escapes, temp_hostgroup->group_name);
					json_object_append_array(json_hostgroup_object, "hosts", 
							json_host_list);
					json_array_append_object(json_hostgroup_list, 
							json_hostgroup_object);
					}
				json_object_append_array(json_data, "hostgroups", 
						json_hostgroup_list);
				}
			else {
				/* compute for all hosts in the specified hostgroup */
				json_host_list = json_new_array();
				for(temp_hostgroup_member = hostgroup_selector->members; 
						temp_hostgroup_member != NULL; 
						temp_hostgroup_member = temp_hostgroup_member->next) {
					if(FALSE == is_authorized_for_host(temp_hostgroup_member->host_ptr, 
							&current_authdata)) {
						continue;
						}
					json_host_object = 
							json_archive_single_host_availability(
							format_options, query_time, start_time, end_time, 
							temp_hostgroup_member->host_name, report_timeperiod, 
							assume_initial_state, assume_state_retention, 
							assume_state_during_nagios_downtime, 
							assumed_initial_host_state, state_types, log);
					if(NULL != json_host_object) {
						json_array_append_object(json_host_list, 
								json_host_object);
						}
					}
				json_hostgroup_object = json_new_object();
				json_object_append_string(json_hostgroup_object, "name",
						&percent_escapes, hostgroup_selector->group_name);
				json_object_append_array(json_hostgroup_object, "hosts", 
						json_host_list);
				json_object_append_object(json_data, "hostgroup", 
						json_hostgroup_object);
				}
			break;
		case AU_OBJTYPE_SERVICEGROUP:
			if(NULL == servicegroup_selector) {
				/* compute for all hosts in all hostgroups */
				json_servicegroup_list = json_new_array();
				for(temp_servicegroup = servicegroup_list; 
						temp_servicegroup != NULL; 
						temp_servicegroup = temp_servicegroup->next) {
					json_service_list = json_new_array();
					for(temp_servicegroup_member = temp_servicegroup->members; 
							temp_servicegroup_member != NULL; 
							temp_servicegroup_member = 
							temp_servicegroup_member->next) {
						if(FALSE == is_authorized_for_service(temp_servicegroup_member->service_ptr,
								&current_authdata)) {
							continue;
							}
						json_service_object = 
								json_archive_single_service_availability(
								format_options, query_time, start_time, 
								end_time, temp_servicegroup_member->host_name, 
								temp_servicegroup_member->service_description, 
								report_timeperiod, assume_initial_state, 
								assume_state_retention, 
								assume_state_during_nagios_downtime, 
								assumed_initial_service_state, state_types, 
								log);
						if(NULL != json_service_object) {
							json_array_append_object(json_service_list, 
									json_service_object);
							}
						}
					json_servicegroup_object = json_new_object();
					json_object_append_string(json_servicegroup_object, "name",
							&percent_escapes, temp_servicegroup->group_name);
					json_object_append_array(json_servicegroup_object, "hosts", 
							json_service_list);
					json_array_append_object(json_servicegroup_list, 
							json_servicegroup_object);
					}
				json_object_append_array(json_data, "servicegroups", 
						json_servicegroup_list);
				}
			else {
				/* compute for all services in the specified servicegroup */
				json_service_list = json_new_array();
				for(temp_servicegroup_member = servicegroup_selector->members; 
						temp_servicegroup_member != NULL; 
						temp_servicegroup_member = 
						temp_servicegroup_member->next) {
					if(FALSE == is_authorized_for_service(temp_servicegroup_member->service_ptr,
							&current_authdata)) {
						continue;
						}
					json_service_object = 
							json_archive_single_service_availability(
							format_options, query_time, start_time, end_time, 
							temp_servicegroup_member->host_name, 
							temp_servicegroup_member->service_description, 
							report_timeperiod, assume_initial_state, 
							assume_state_retention, 
							assume_state_during_nagios_downtime, 
							assumed_initial_service_state, state_types, log);
					if(NULL != json_service_object) {
						json_array_append_object(json_service_list, 
								json_service_object);
						}
					}
				json_servicegroup_object = json_new_object();
				json_object_append_string(json_servicegroup_object, "name",
						&percent_escapes, servicegroup_selector->group_name);
				json_object_append_array(json_servicegroup_object, "services", 
						json_service_list);
				json_object_append_object(json_data, "servicegroup", 
						json_servicegroup_object);
				}
			break;
		default:
			break;
		}

	return json_data;
	}

json_object *json_archive_single_host_availability(unsigned format_options, 
		time_t query_time, time_t start_time, time_t end_time, char *host_name, 
		timeperiod *report_timeperiod, int assume_initial_state, 
		int assume_state_retention, int assume_state_during_nagios_downtime, 
		int assumed_initial_host_state, unsigned state_types, au_log *log) {

	au_host *host = NULL;
	au_node *temp_entry = NULL;
	au_host *global_host = NULL;
	json_object	*json_host_availability = NULL;

	host = au_find_host(log->hosts, host_name);
	if(NULL == host) { 
		/* host has no log entries, so create the host */
		host = au_add_host(log->hosts, host_name);
		/* Add global events to this new host */
		global_host = au_find_host(log->hosts, "*");
		if(NULL != global_host) {
			for(temp_entry = global_host->log_entries->head; NULL != temp_entry;
					temp_entry = temp_entry->next) {
				if(au_list_add_node(host->log_entries, temp_entry->data,
						au_cmp_log_entries) == 0) {
					break;
					}
				}
			}
		}

	if((host->availability = calloc(1, sizeof(au_availability))) != NULL) {
		compute_host_availability(query_time, start_time, end_time, log, host, 
				report_timeperiod, assume_initial_state, 
				assumed_initial_host_state, 
				assume_state_during_nagios_downtime, assume_state_retention);
		json_host_availability = json_archive_host_availability(format_options, 
				host->name, host->availability);
		}

	return json_host_availability;
	}

json_object *json_archive_single_service_availability(unsigned format_options, 
		time_t query_time, time_t start_time, time_t end_time, char *host_name, 
		char *service_description, timeperiod *report_timeperiod, 
		int assume_initial_state, int assume_state_retention, 
		int assume_state_during_nagios_downtime, 
		int assumed_initial_service_state, unsigned state_types, au_log *log) {

	au_service *service = NULL;
	au_node *temp_entry = NULL;
	au_host *global_host = NULL;
	json_object	*json_service_availability = NULL;

	service = au_find_service(log->services, host_name, service_description);
	if(NULL == service) {
		/* service has no log entries, so create the service */
		service = au_add_service(log->services, host_name, service_description);
		/* Add global events to this new service */
		global_host = au_find_host(log->hosts, "*");
		if(NULL != global_host) {
			for(temp_entry = global_host->log_entries->head; NULL != temp_entry;
					temp_entry = temp_entry->next) {
				if(au_list_add_node(service->log_entries, temp_entry->data,
						au_cmp_log_entries) == 0) {
					break;
					}
				}
			}
		}

	if((service->availability = calloc(1, sizeof(au_availability))) != NULL) {
		compute_service_availability(query_time, start_time, end_time, log, 
				service, report_timeperiod, assume_initial_state, 
				assumed_initial_service_state, 
				assume_state_during_nagios_downtime, assume_state_retention);
		json_service_availability = 
				json_archive_service_availability(format_options, 
						service->host_name, service->description, 
						service->availability);
		}

	return json_service_availability;
	}

json_object *json_archive_host_availability(unsigned format_options, 
		char *name, au_availability *availability) {

	json_object *json_host_availability;

	json_host_availability = json_new_object();

	if(name != NULL) {
		json_object_append_string(json_host_availability, "name",
				&percent_escapes, name);
	}

	json_object_append_duration(json_host_availability, "time_up", 
			availability->time_up);
	json_object_append_duration(json_host_availability, "time_down", 
			availability->time_down);
	json_object_append_duration(json_host_availability, "time_unreachable", 
			availability->time_unreachable);
	json_object_append_duration(json_host_availability, "scheduled_time_up", 
			availability->scheduled_time_up);
	json_object_append_duration(json_host_availability, "scheduled_time_down", 
			availability->scheduled_time_down);
	json_object_append_duration(json_host_availability, 
			"scheduled_time_unreachable", 
			availability->scheduled_time_unreachable);
	json_object_append_duration(json_host_availability, 
			"time_indeterminate_nodata", 
			availability->time_indeterminate_nodata);
	json_object_append_duration(json_host_availability, 
			"time_indeterminate_notrunning", 
			availability->time_indeterminate_notrunning);

	return json_host_availability;
	}

json_object *json_archive_service_availability(unsigned format_options, 
		char *host_name, char *description, au_availability *availability) {

	json_object *json_service_availability;

	json_service_availability = json_new_object();

	if(host_name != NULL) {
		json_object_append_string(json_service_availability, "host_name", 
				&percent_escapes, host_name);
	}
	if(description != NULL) {
		json_object_append_string(json_service_availability, "description", 
				&percent_escapes, description);
	}

	json_object_append_duration(json_service_availability, "time_ok", 
			availability->time_ok);
	json_object_append_duration(json_service_availability, "time_warning", 
			availability->time_warning);
	json_object_append_duration(json_service_availability, "time_critical", 
			availability->time_critical);
	json_object_append_duration(json_service_availability, "time_unknown", 
			availability->time_unknown);
	json_object_append_duration(json_service_availability, "scheduled_time_ok", 
			availability->scheduled_time_ok);
	json_object_append_duration(json_service_availability, 
			"scheduled_time_warning", availability->scheduled_time_warning);
	json_object_append_duration(json_service_availability, 
			"scheduled_time_critical", availability->scheduled_time_critical);
	json_object_append_duration(json_service_availability, 
			"scheduled_time_unknown", availability->scheduled_time_unknown);
	json_object_append_duration(json_service_availability, 
			"time_indeterminate_nodata", 
			availability->time_indeterminate_nodata);
	json_object_append_duration(json_service_availability, 
			"time_indeterminate_notrunning", 
			availability->time_indeterminate_notrunning);

	return json_service_availability;
	}
