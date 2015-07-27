/**************************************************************************
 *
 * OBJECTJSON.C -  Nagios CGI for returning JSON-formatted object data
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
		Add code to display customvariables
		Add sort criteria for *list queries
		Add core3 flag for backward compatible display of flags that were
			combined into a single variable in core4
		Implement internationalized day of week, month names, and formats
			for timeperiod details
		Implement raw numbers for day of week, month for timeperiod details
*/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"

#include "../include/cgiutils.h"
#include "../include/getcgi.h"
#include "../include/cgiauth.h"
#include "../include/jsonutils.h"
#include "../include/objectjson.h"

#define THISCGI "objectjson.cgi"

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char *status_file;

extern host *host_list;
extern hostgroup *hostgroup_list;
extern service *service_list;
extern servicegroup *servicegroup_list;
extern contact *contact_list;
extern contactgroup *contactgroup_list;
extern timeperiod *timeperiod_list;
extern command *command_list;
extern servicedependency *servicedependency_list;
extern serviceescalation *serviceescalation_list;
extern hostdependency *hostdependency_list;
extern hostescalation *hostescalation_list;

void document_header(int);
void document_footer(void);
void init_cgi_data(object_json_cgi_data *);
int process_cgivars(json_object *, object_json_cgi_data *, time_t);
void free_cgi_data(object_json_cgi_data *);
int validate_arguments(json_object *, object_json_cgi_data *, time_t);

authdata current_authdata;

const string_value_mapping valid_queries[] = {
	{ "hostcount", OBJECT_QUERY_HOSTCOUNT, 
		"Return the number of hosts" },
	{ "hostlist", OBJECT_QUERY_HOSTLIST, 
		"Return a list of hosts" },
	{ "host", OBJECT_QUERY_HOST, 
		"Return the configuration for a single host" },
	{ "hostgroupcount", OBJECT_QUERY_HOSTGROUPCOUNT, 
		"Return the number of host groups" },
	{ "hostgrouplist", OBJECT_QUERY_HOSTGROUPLIST, 
		"Return a list of host groups" },
	{ "hostgroup", OBJECT_QUERY_HOSTGROUP, 
		"Return the configuration for a single hostgroup" },
	{ "servicecount", OBJECT_QUERY_SERVICECOUNT, 
		"Return a list of services" },
	{ "servicelist", OBJECT_QUERY_SERVICELIST, 
		"Return a list of services" },
	{ "service", OBJECT_QUERY_SERVICE, 
		"Return the configuration for a single service" },
	{ "servicegroupcount", OBJECT_QUERY_SERVICEGROUPCOUNT, 
		"Return the number of service groups" },
	{ "servicegrouplist", OBJECT_QUERY_SERVICEGROUPLIST, 
		"Return a list of service groups" },
	{ "servicegroup", OBJECT_QUERY_SERVICEGROUP,
		"Return the configuration for a single servicegroup" },
	{ "contactcount", OBJECT_QUERY_CONTACTCOUNT, 
		"Return the number of contacts" },
	{ "contactlist", OBJECT_QUERY_CONTACTLIST, 
		"Return a list of contacts" },
	{ "contact", OBJECT_QUERY_CONTACT, 
		"Return the configuration for a single contact" },
	{ "contactgroupcount", OBJECT_QUERY_CONTACTGROUPCOUNT,
		"Return the number of contact groups" },
	{ "contactgrouplist", OBJECT_QUERY_CONTACTGROUPLIST,
		"Return a list of contact groups" },
	{ "contactgroup", OBJECT_QUERY_CONTACTGROUP,
		"Return the configuration for a single contactgroup" },
	{ "timeperiodcount", OBJECT_QUERY_TIMEPERIODCOUNT,
		"Return the number of time periods" },
	{ "timeperiodlist", OBJECT_QUERY_TIMEPERIODLIST,
		"Return a list of time periods" },
	{ "timeperiod", OBJECT_QUERY_TIMEPERIOD,
		"Return the configuration for a single timeperiod" },
	{ "commandcount", OBJECT_QUERY_COMMANDCOUNT,
		"Return the number of commands" },
	{ "commandlist", OBJECT_QUERY_COMMANDLIST,
		"Return a list of commands" },
	{ "command", OBJECT_QUERY_COMMAND,
		"Return the configuration for a single command" },
	{ "servicedependencycount", OBJECT_QUERY_SERVICEDEPENDENCYCOUNT,
		"Return the number of service dependencies" },
	{ "servicedependencylist", OBJECT_QUERY_SERVICEDEPENDENCYLIST,
		"Return a list of service dependencies" },
	{ "serviceescalationcount", OBJECT_QUERY_SERVICEESCALATIONCOUNT,
		"Return the number of service escalations" },
	{ "serviceescalationlist", OBJECT_QUERY_SERVICEESCALATIONLIST,
		"Return a list of service escalations" },
	{ "hostdependencycount", OBJECT_QUERY_HOSTDEPENDENCYCOUNT,
		"Return the number of host dependencies" },
	{ "hostdependencylist", OBJECT_QUERY_HOSTDEPENDENCYLIST,
		"Return a list of host dependencies" },
	{ "hostescalationcount", OBJECT_QUERY_HOSTESCALATIONCOUNT,
		"Return the number of host escalations" },
	{ "hostescalationlist", OBJECT_QUERY_HOSTESCALATIONLIST,
		"Return a list of host escalations" },
	{ "help", OBJECT_QUERY_HELP, 
		"Display help for this CGI" },
	{ NULL, -1, NULL },
	};

static const int query_status[][2] = {
	{ OBJECT_QUERY_HOSTCOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_HOSTLIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_HOST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_HOSTGROUPCOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_HOSTGROUPLIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_HOSTGROUP, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_SERVICECOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_SERVICELIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_SERVICE, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_SERVICEGROUPCOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_SERVICEGROUPLIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_SERVICEGROUP, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_CONTACTCOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_CONTACTLIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_CONTACT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_CONTACTGROUPCOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_CONTACTGROUPLIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_CONTACTGROUP, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_TIMEPERIODCOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_TIMEPERIODLIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_TIMEPERIOD, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_COMMANDCOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_COMMANDLIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_COMMAND, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_SERVICEDEPENDENCYCOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_SERVICEDEPENDENCYLIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_SERVICEESCALATIONCOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_SERVICEESCALATIONLIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_HOSTDEPENDENCYCOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_HOSTDEPENDENCYLIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_HOSTESCALATIONCOUNT, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_HOSTESCALATIONLIST, QUERY_STATUS_RELEASED },
	{ OBJECT_QUERY_HELP, QUERY_STATUS_RELEASED },
	{ -1, -1 },
};

option_help object_json_help[] = {
	{ 
		"query",
		"Query",
		"enumeration",
		{ "all", NULL },
		{ NULL },
		NULL,
		"Specifies the type of query to be exeuted.",
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
		{ "hostlist", "hostgrouplist", "servicelist", "servicegrouplist", "contactlist", "contactgrouplist", "timeperiodlist", "commandlist", "servicedependencylist", "serviceescalationlist", "hostdependencylist", "hostescalationlist", NULL },
		NULL,
		"Specifies the index (zero-based) of the first object in the list to be returned.",
		NULL
		},
	{ 
		"count",
		"Count",
		"integer",
		{ NULL },
		{ "hostlist", "hostgrouplist", "servicelist", "servicegrouplist", "contactlist", "contactgrouplist", "timeperiodlist", "commandlist", "servicedependencylist", "serviceescalationlist", "hostdependencylist", "hostescalationlist", NULL },
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
		{ "hostlist", "hostgrouplist", "servicelist", "servicegrouplist", "contactlist", "contactgrouplist", "timeperiodlist", "commandlist", NULL },
		NULL,
		"If true, return the details for all entities in the list.",
		NULL
		},
	{ 
		"hostname",
		"Host Name",
		"nagios:objectjson/hostlist",
		{ "host", "service", NULL },
		{ "servicecount", "servicelist", "hostescalationlist", "serviceescalationlist", NULL },
		NULL,
		"Name for the host requested.",
		NULL
		},
	{ 
		"hostgroupmember",
		"Host Group Member",
		"nagios:objectjson/hostlist",
		{ NULL },
		{ "hostgroupcount", "hostgrouplist", NULL },
		NULL,
		"Limits the hostgroups returned to those containing the hostgroupmember.",
		NULL
		},
	{ 
		"hostgroup",
		"Host Group",
		"nagios:objectjson/hostgrouplist",
		{ "hostgroup", NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", "hostescalationcount", "hostescalationlist", "serviceescalationcount", "serviceescalationlist", NULL },
		NULL,
		"Returns information applicable to the hostgroup or the hosts in the hostgroup depending on the query.",
		NULL
		},
	{ 
		"servicegroup",
		"Service Group",
		"nagios:objectjson/servicegrouplist",
		{ "servicegroup", NULL },
		{ "servicecount", "servicelist", "serviceescalationcount", "serviceescalationlist", NULL },
		NULL,
		"Returns information applicable to the servicegroup or the services in the servicegroup depending on the query.",
		NULL
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
		{ "hostcount", "hostlist", "servicecount", "servicelist", "contactcount", "contactlist", "serviceescalationcount", "serviceescalationlist", "hostescalationcount", "hostescalationlist", NULL },
		NULL,
		"Returns information applicable to the contactgroup or the contacts in the contactgroup depending on the query.",
		NULL
		},
	{ 
		"servicedescription",
		"Service Description",
		"nagios:objectjson/servicelist",
		{ "service", NULL },
		{ "servicecount", "servicelist", "serviceescalationcount", "serviceescalationlist", NULL },
		"hostname",
		"Description for the service requested.",
		NULL
		},
	{ 
		"servicegroupmemberhost",
		"Service Group Member Host",
		"nagios:objectjson/hostlist",
		{ NULL },
		{ "servicegroupcount", "servicegrouplist", NULL },
		NULL,
		"Limits the servicegroups returned to those containing the servicegroupmemberhost (and servicegroupmemberservice).",
		NULL
		},
	{ 
		"servicegroupmemberservice",
		"Service Group Member Service",
		"nagios:objectjson/servicelist",
		{ NULL },
		{ "servicegroupcount", "servicegrouplist", NULL },
		"servicegroupmemberhost",
		"Limits the servicegroups returned to those containing the servicegroupmemberservice (and servicegroupmemberhost).",
		NULL
		},
	{ 
		"contactname",
		"Contact Name",
		"nagios:objectjson/contactlist",
		{ "contact", NULL },
		{ "hostcount", "hostlist", "servicecount", "servicelist", "serviceescalationcount", "serviceescalationlist", "hostescalationcount", "hostescalationlist", NULL },
		NULL,
		"Name for the contact requested.",
		NULL
		},
	{ 
		"contactgroupmember",
		"Contact Group Member",
		"nagios:objectjson/contactlist",
		{ NULL },
		{ "contactgroupcount", "contactgrouplist", NULL },
		NULL,
		"Limits the contactgroups returned to those containing the contactgroupmember.",
		NULL
		},
	{ 
		"timeperiod",
		"Timeperiod Name",
		"nagios:objectjson/timeperiodlist",
		{ "timeperiod", NULL },
		{ NULL },
		NULL,
		"Name for the timeperiod requested.",
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
		{ "hostcount","hostlist", "contactcount", "contactlist", NULL },
		NULL,
		"Name of a host notification timeperiod to be used as selection criteria.",
		NULL
		},
	{
		"servicenotificationtimeperiod",
		"Service Notification Timeperiod Name",
		"nagios:objectjson/timeperiodlist",
		{ NULL },
		{ "servicecount", "servicelist", "contactcount", "contactlist", NULL },
		NULL,
		"Name of a service notification timeperiod to be used as selection criteria.",
		NULL
		},
	{
		"command",
		"Command Name",
		"nagios:objectjson/commandlist",
		{ "command", NULL },
		{ NULL },
		NULL,
		"Name for the command requested.",
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
		"masterhostname",
		"Master Host Name",
		"nagios:objectjson/hostlist",
		{ NULL },
		{ "hostdependencycount", "hostdependencylist", "servicedependencycount", "servicedependencylist", NULL },
		NULL,
		"Name for a master host to be used as a selector.",
		NULL
		},
	{
		"masterhostgroupname",
		"Master Hostgroup Name",
		"nagios:objectjson/hostgrouplist",
		{ NULL },
		{ "hostdependencycount", "hostdependencylist", "servicedependencycount", "servicedependencylist", NULL },
		NULL,
		"Name for a master hostgroup to be used as a selector.",
		NULL
		},
	{
		"masterservicedescription",
		"Master Service Description",
		"nagios:objectjson/servicelist",
		{ NULL },
		{ "servicedependencycount", "servicedependencylist", NULL },
		"masterhostname",
		"Description for a master service to be used as a selector.",
		NULL
		},
	{
		"masterservicegroupname",
		"Master Servicegroup Name",
		"nagios:objectjson/servicegrouplist",
		{ NULL },
		{ "servicedependencycount", "servicedependencylist", NULL },
		NULL,
		"Name for a master servicegroup to be used as a selector.",
		NULL
		},
	{
		"dependenthostname",
		"Dependent Host Name",
		"nagios:objectjson/hostlist",
		{ NULL },
		{ "hostdependencycount", "hostdependencylist", "servicedependencycount", "servicedependencylist", NULL },
		NULL,
		"Name for a dependent host to be used as a selector.",
		NULL
		},
	{
		"dependenthostgroupname",
		"Dependent Hostgroup Name",
		"nagios:objectjson/hostgrouplist",
		{ NULL },
		{ "hostdependencycount", "hostdependencylist", "servicedependencycount", "servicedependencylist", NULL },
		NULL,
		"Name for a dependent hostgroup to be used as a selector.",
		NULL
		},
	{
		"dependentservicedescription",
		"Dependent Service Description",
		"nagios:objectjson/servicelist",
		{ NULL },
		{ "servicedependencycount", "servicedependencylist", NULL },
		"dependenthostname",
		"Description for a dependent service to be used as a selector.",
		NULL
		},
	{
		"dependentservicegroupname",
		"Dependent Servicegroup Name",
		"nagios:objectjson/servicegrouplist",
		{ NULL },
		{ "servicedependencycount", "servicedependencylist", NULL },
		NULL,
		"Name for a dependent servicegroup to be used as a selector.",
		NULL
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

int json_object_host_passes_selection(host *, int, host *, int, host *, 
		hostgroup *, contact *, contactgroup *, timeperiod *, timeperiod *,
		command *, command *);
json_object *json_object_host_selectors(int, int, int, host *, int, host *, 
		hostgroup *, contact *, contactgroup *, timeperiod *, timeperiod *,
		command *, command *);

int json_object_hostgroup_passes_selection(hostgroup *, host *);
json_object *json_object_hostgroup_selectors(int, int, host *);

int json_object_service_passes_host_selection(host *, int, host *, int, host *, 
		hostgroup *, host *);
int json_object_service_passes_service_selection(service *, servicegroup *, 
		contact *, char *, char *, char *, contactgroup *, timeperiod *,
		timeperiod *, command *, command *);
json_object *json_object_service_selectors(int, int, int, host *, int, host *, 
		hostgroup *, host *, servicegroup *, contact *, char *, char *, char *,
		contactgroup *, timeperiod *, timeperiod *, command *, command *);

int json_object_servicegroup_passes_selection(servicegroup *, service *);
json_object *json_object_servicegroup_display_selectors(int, int, service *);

int json_object_contact_passes_selection(contact *, contactgroup *,
		timeperiod *, timeperiod *);
json_object *json_object_contact_selectors(int, int, contactgroup *,
		timeperiod *, timeperiod *);

int json_object_contactgroup_passes_selection(contactgroup *, contact *);
json_object *json_object_contactgroup_display_selectors(int, int, contact *);

json_object *json_object_timeperiod_selectors(int, int);

json_object *json_object_command_selectors(int, int);

int json_object_servicedependency_passes_selection(servicedependency *, host *,
		hostgroup *, char *, servicegroup *, host *, hostgroup *, char *,
		servicegroup *);
json_object *json_object_servicedependency_selectors(int, int, host *,
		hostgroup *, char *, servicegroup *, host *, hostgroup *, char *,
		servicegroup *);

int json_object_serviceescalation_passes_selection(serviceescalation *, host *,
		char *, hostgroup *, servicegroup *, contact *, contactgroup *);
json_object *json_object_serviceescalation_selectors(int, int, host *, char *,
		hostgroup *, servicegroup *, contact *, contactgroup *);

int json_object_hostdependency_passes_selection(hostdependency *, host *,
		hostgroup *, host *, hostgroup *);
json_object *json_object_hostdependency_selectors(int, int, host *,
		hostgroup *, host *, hostgroup *);

int json_object_hostescalation_passes_selection(hostescalation *, host *,
		hostgroup *, contact *, contactgroup *);
json_object *json_object_hostescalation_selectors(int, int, host *,
		hostgroup *, contact *, contactgroup *);

int main(void) {
	int result = OK;
	time_t query_time;
	object_json_cgi_data	cgi_data;
	json_object *json_root;
	struct stat ocstat;
	time_t	last_object_cache_update = (time_t)0;

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
		json_object_append_object(json_root, "data", 
				json_help(object_json_help));
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
		json_object_append_object(json_root, "data", json_help(object_json_help));
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
		json_object_append_object(json_root, "data", json_help(object_json_help));
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
		json_object_append_object(json_root, "data", json_help(object_json_help));
		document_footer();
		return ERROR;
		}

	/* Get the update time on the object cache file */
	if(stat(object_cache_file, &ocstat) < 0) {
		json_object_append_object(json_root, "result",
				json_result(query_time, THISCGI,
				svm_get_string_from_value(cgi_data.query, valid_queries),
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, NULL, RESULT_FILE_OPEN_READ_ERROR,
				"Error: Could not obtain object cache file status: %s!",
				strerror(errno)));
		json_object_append_object(json_root, "data", json_help(object_json_help));
		document_footer();
		return ERROR;
		}
	last_object_cache_update = ocstat.st_mtime;

	/* read all status data */
	result = read_all_status_data(status_file, READ_ALL_STATUS_DATA);
	if(result == ERROR) {
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, NULL, RESULT_FILE_OPEN_READ_ERROR,
				"Error: Could not read host and service status information!"));
		json_object_append_object(json_root, "data", json_help(object_json_help));

		document_footer();
		return ERROR;
		}

	/* validate arguments in URL */
	result = validate_arguments(json_root, &cgi_data, query_time);
	if(result != RESULT_SUCCESS) {
		json_object_append_object(json_root, "data", json_help(object_json_help));
		json_object_print(json_root, 0, 1, cgi_data.strftime_format,
				cgi_data.format_options);
		document_footer();
		return ERROR;
		}

	/* get authentication information */
	get_authentication_information(&current_authdata);

	/* Return something to the user */
	switch( cgi_data.query) {
	case OBJECT_QUERY_HOSTCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_hostcount(cgi_data.use_parent_host, 
				cgi_data.parent_host, cgi_data.use_child_host, 
				cgi_data.child_host, cgi_data.hostgroup, cgi_data.contact,
				cgi_data.contactgroup, cgi_data.check_timeperiod,
				cgi_data.host_notification_timeperiod, cgi_data.check_command,
				cgi_data.event_handler));
		break;
	case OBJECT_QUERY_HOSTLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_hostlist(cgi_data.format_options, cgi_data.start, 
				cgi_data.count, cgi_data.details, cgi_data.use_parent_host, 
				cgi_data.parent_host, cgi_data.use_child_host, 
				cgi_data.child_host, cgi_data.hostgroup, cgi_data.contact,
				cgi_data.contactgroup, cgi_data.check_timeperiod,
				cgi_data.host_notification_timeperiod, cgi_data.check_command,
				cgi_data.event_handler));
		break;
	case OBJECT_QUERY_HOST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_host(cgi_data.format_options, cgi_data.host));
		break;
	case OBJECT_QUERY_HOSTGROUPCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_hostgroupcount(cgi_data.format_options, 
				cgi_data.hostgroup_member));
		break;
	case OBJECT_QUERY_HOSTGROUPLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data",
				json_object_hostgrouplist(cgi_data.format_options, 
				cgi_data.start, cgi_data.count, cgi_data.details, 
				cgi_data.hostgroup_member));
		break;
	case OBJECT_QUERY_HOSTGROUP:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_hostgroup(cgi_data.format_options, 
				cgi_data.hostgroup));
		break;
	case OBJECT_QUERY_SERVICECOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_servicecount(cgi_data.host, 
				cgi_data.use_parent_host, cgi_data.parent_host, 
				cgi_data.use_child_host, cgi_data.child_host, 
				cgi_data.hostgroup, cgi_data.servicegroup, cgi_data.contact, 
				cgi_data.service_description, cgi_data.parent_service_name,
				cgi_data.child_service_name, cgi_data.contactgroup,
				cgi_data.check_timeperiod,
				cgi_data.service_notification_timeperiod,
				cgi_data.check_command, cgi_data.event_handler));
		break;
	case OBJECT_QUERY_SERVICELIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_servicelist(cgi_data.format_options, cgi_data.start,
				cgi_data.count, cgi_data.details, cgi_data.host, 
				cgi_data.use_parent_host, cgi_data.parent_host, 
				cgi_data.use_child_host, cgi_data.child_host, 
				cgi_data.hostgroup, cgi_data.servicegroup, cgi_data.contact, 
				cgi_data.service_description, cgi_data.parent_service_name,
				cgi_data.child_service_name, cgi_data.contactgroup,
				cgi_data.check_timeperiod,
				cgi_data.service_notification_timeperiod,
				cgi_data.check_command, cgi_data.event_handler));
		break;
	case OBJECT_QUERY_SERVICE:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_service(cgi_data.format_options, cgi_data.service));
		break;
	case OBJECT_QUERY_SERVICEGROUPCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_servicegroupcount(cgi_data.servicegroup_member));
		break;
	case OBJECT_QUERY_SERVICEGROUPLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data",
				json_object_servicegrouplist(cgi_data.format_options, 
				cgi_data.start, cgi_data.count, cgi_data.details, 
				cgi_data.servicegroup_member));
		break;
	case OBJECT_QUERY_SERVICEGROUP:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_servicegroup(cgi_data.format_options, 
				cgi_data.servicegroup));
		break;
	case OBJECT_QUERY_CONTACTCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_contactcount(cgi_data.contactgroup,
				cgi_data.host_notification_timeperiod,
				cgi_data.service_notification_timeperiod));
		break;
	case OBJECT_QUERY_CONTACTLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_contactlist(cgi_data.format_options, cgi_data.start, 
				cgi_data.count, cgi_data.details, cgi_data.contactgroup,
				cgi_data.host_notification_timeperiod,
				cgi_data.service_notification_timeperiod));
		break;
	case OBJECT_QUERY_CONTACT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_contact(cgi_data.format_options, cgi_data.contact));
		break;
	case OBJECT_QUERY_CONTACTGROUPCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_contactgroupcount(cgi_data.contactgroup_member));
		break;
	case OBJECT_QUERY_CONTACTGROUPLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data",
				json_object_contactgrouplist(cgi_data.format_options, 
				cgi_data.start, cgi_data.count, cgi_data.details, 
				cgi_data.contactgroup_member));
		break;
	case OBJECT_QUERY_CONTACTGROUP:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_contactgroup(cgi_data.format_options, 
				cgi_data.contactgroup));
		break;
	case OBJECT_QUERY_TIMEPERIODCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_timeperiodcount());
		break;
	case OBJECT_QUERY_TIMEPERIODLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_timeperiodlist(cgi_data.format_options, 
				cgi_data.start, cgi_data.count, cgi_data.details));
		break;
	case OBJECT_QUERY_TIMEPERIOD:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_timeperiod(cgi_data.format_options, 
				cgi_data.timeperiod));
		break;
	case OBJECT_QUERY_COMMANDCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", json_object_commandcount());
		break;
	case OBJECT_QUERY_COMMANDLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_commandlist(cgi_data.format_options, cgi_data.start, 
				cgi_data.count, cgi_data.details));
		break;
	case OBJECT_QUERY_COMMAND:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_command(cgi_data.format_options, cgi_data.command));
		break;
	case OBJECT_QUERY_SERVICEDEPENDENCYCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_servicedependencycount(cgi_data.master_host, 
				cgi_data.master_hostgroup, cgi_data.master_service_description,
				cgi_data.master_servicegroup, cgi_data.dependent_host,
				cgi_data.dependent_hostgroup,
				cgi_data.dependent_service_description,
				cgi_data.dependent_servicegroup));
		break;
	case OBJECT_QUERY_SERVICEDEPENDENCYLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_servicedependencylist(cgi_data.format_options, 
				cgi_data.start, cgi_data.count, cgi_data.master_host,
				cgi_data.master_hostgroup, cgi_data.master_service_description,
				cgi_data.master_servicegroup, cgi_data.dependent_host,
				cgi_data.dependent_hostgroup,
				cgi_data.dependent_service_description,
				cgi_data.dependent_servicegroup));
		break;
	case OBJECT_QUERY_SERVICEESCALATIONCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_serviceescalationcount(cgi_data.host,
				cgi_data.service_description, cgi_data.hostgroup,
				cgi_data.servicegroup, cgi_data.contact,
				cgi_data.contactgroup));
		break;
	case OBJECT_QUERY_SERVICEESCALATIONLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_serviceescalationlist(cgi_data.format_options, 
				cgi_data.start, cgi_data.count, cgi_data.host,
				cgi_data.service_description, cgi_data.hostgroup,
				cgi_data.servicegroup, cgi_data.contact,
				cgi_data.contactgroup));
		break;
	case OBJECT_QUERY_HOSTDEPENDENCYCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_hostdependencycount(cgi_data.master_host,
				cgi_data.master_hostgroup, cgi_data.dependent_host,
				cgi_data.dependent_hostgroup));
		break;
	case OBJECT_QUERY_HOSTDEPENDENCYLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_hostdependencylist(cgi_data.format_options, 
				cgi_data.start, cgi_data.count, cgi_data.master_host,
				cgi_data.master_hostgroup, cgi_data.dependent_host,
				cgi_data.dependent_hostgroup));
		break;
	case OBJECT_QUERY_HOSTESCALATIONCOUNT:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_hostescalationcount(cgi_data.host,
				cgi_data.hostgroup, cgi_data.contact, cgi_data.contactgroup));
		break;
	case OBJECT_QUERY_HOSTESCALATIONLIST:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				last_object_cache_update, &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", 
				json_object_hostescalationlist(cgi_data.format_options, 
				cgi_data.start, cgi_data.count, cgi_data.host,
				cgi_data.hostgroup, cgi_data.contact, cgi_data.contactgroup));
		break;
	case OBJECT_QUERY_HELP:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				compile_time(__DATE__, __TIME__), &current_authdata,
				RESULT_SUCCESS, ""));
		json_object_append_object(json_root, "data", json_help(object_json_help));
		break;
	default:
		json_object_append_object(json_root, "result", 
				json_result(query_time, THISCGI, 
				svm_get_string_from_value(cgi_data.query, valid_queries), 
				get_query_status(query_status, cgi_data.query),
				(time_t)-1, &current_authdata, RESULT_OPTION_MISSING,
				"Error: Object Type not specified. See data for help."));
		json_object_append_object(json_root, "data", json_help(object_json_help));
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


void init_cgi_data(object_json_cgi_data *cgi_data) {
	cgi_data->format_options = 0;
	cgi_data->query = OBJECT_QUERY_INVALID;
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
	cgi_data->hostgroup_member_name = NULL;
	cgi_data->hostgroup_member = NULL;
	cgi_data->hostgroup_name = NULL;
	cgi_data->hostgroup = NULL;
	cgi_data->servicegroup_name = NULL;
	cgi_data->servicegroup = NULL;
	cgi_data->service_description = NULL;
	cgi_data->service = NULL;
	cgi_data->servicegroup_member_host_name = NULL;
	cgi_data->servicegroup_member_service_description = NULL;
	cgi_data->servicegroup_member = NULL;
	cgi_data->parent_service_name = NULL;
	cgi_data->child_service_name = NULL;
	cgi_data->contactgroup_name = NULL;
	cgi_data->contactgroup = NULL;
	cgi_data->contact_name = NULL;
	cgi_data->contact = NULL;
	cgi_data->contactgroup_member_name = NULL;
	cgi_data->contactgroup_member = NULL;
	cgi_data->timeperiod_name = NULL;
	cgi_data->timeperiod = NULL;
	cgi_data->check_timeperiod_name = NULL;
	cgi_data->check_timeperiod = NULL;
	cgi_data->host_notification_timeperiod_name = NULL;
	cgi_data->host_notification_timeperiod = NULL;
	cgi_data->service_notification_timeperiod_name = NULL;
	cgi_data->service_notification_timeperiod = NULL;
	cgi_data->command_name = NULL;
	cgi_data->command = NULL;
	cgi_data->check_command_name = NULL;
	cgi_data->check_command = NULL;
	cgi_data->event_handler_name = NULL;
	cgi_data->event_handler = NULL;
	cgi_data->master_host_name = NULL;
	cgi_data->master_host = NULL;
	cgi_data->master_hostgroup_name = NULL;
	cgi_data->master_hostgroup = NULL;
	cgi_data->master_service_description = NULL;
	cgi_data->master_service = NULL;
	cgi_data->master_servicegroup_name = NULL;
	cgi_data->master_servicegroup = NULL;
	cgi_data->dependent_host_name = NULL;
	cgi_data->dependent_host = NULL;
	cgi_data->dependent_hostgroup_name = NULL;
	cgi_data->dependent_hostgroup = NULL;
	cgi_data->dependent_service_description = NULL;
	cgi_data->dependent_service = NULL;
	cgi_data->dependent_servicegroup_name = NULL;
	cgi_data->dependent_servicegroup = NULL;
}

void free_cgi_data( object_json_cgi_data *cgi_data) {
	if( NULL != cgi_data->strftime_format) free( cgi_data->strftime_format);
	if( NULL != cgi_data->parent_host_name) free( cgi_data->parent_host_name);
	if( NULL != cgi_data->child_host_name) free( cgi_data->child_host_name);
	if( NULL != cgi_data->host_name) free( cgi_data->host_name);
	if( NULL != cgi_data->hostgroup_member_name) free( cgi_data->hostgroup_member_name);
	if( NULL != cgi_data->hostgroup_name) free( cgi_data->hostgroup_name);
	if( NULL != cgi_data->servicegroup_name) free(cgi_data->servicegroup_name);
	if( NULL != cgi_data->service_description) free(cgi_data->service_description);
	if( NULL != cgi_data->servicegroup_member_host_name) free(cgi_data->servicegroup_member_host_name);
	if( NULL != cgi_data->servicegroup_member_service_description) free(cgi_data->servicegroup_member_service_description);
	if( NULL != cgi_data->parent_service_name) free( cgi_data->parent_service_name);
	if( NULL != cgi_data->child_service_name) free( cgi_data->child_service_name);
	if( NULL != cgi_data->contactgroup_name) free(cgi_data->contactgroup_name);
	if( NULL != cgi_data->contact_name) free(cgi_data->contact_name);
	if( NULL != cgi_data->contactgroup_member_name) free(cgi_data->contactgroup_member_name);
	if( NULL != cgi_data->timeperiod_name) free(cgi_data->timeperiod_name);
	if( NULL != cgi_data->check_timeperiod_name) free(cgi_data->check_timeperiod_name);
	if( NULL != cgi_data->host_notification_timeperiod_name) free(cgi_data->host_notification_timeperiod_name);
	if( NULL != cgi_data->service_notification_timeperiod_name) free(cgi_data->service_notification_timeperiod_name);
	if( NULL != cgi_data->command_name) free(cgi_data->command_name);
	if( NULL != cgi_data->check_command_name) free(cgi_data->check_command_name);
	if( NULL != cgi_data->event_handler_name) free(cgi_data->event_handler_name);
	if( NULL != cgi_data->master_host_name) free(cgi_data->master_host_name);
	if( NULL != cgi_data->master_hostgroup_name) free(cgi_data->master_hostgroup_name);
	if( NULL != cgi_data->master_service_description) free(cgi_data->master_service_description);
	if( NULL != cgi_data->master_servicegroup_name) free(cgi_data->master_servicegroup_name);
	if( NULL != cgi_data->dependent_host_name) free(cgi_data->dependent_host_name);
	if( NULL != cgi_data->dependent_hostgroup_name) free(cgi_data->dependent_hostgroup_name);
	if( NULL != cgi_data->dependent_service_description) free(cgi_data->dependent_service_description);
	if( NULL != cgi_data->dependent_servicegroup_name) free(cgi_data->dependent_servicegroup_name);
	}


int process_cgivars(json_object *json_root, object_json_cgi_data *cgi_data,
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

		else if(!strcmp(variables[x], "hostgroupmember")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->hostgroup_member_name)))
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

		else if(!strcmp(variables[x], "servicegroupmemberhost")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->servicegroup_member_host_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "servicegroupmemberservice")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1],
					&(cgi_data->servicegroup_member_service_description)))
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

		else if(!strcmp(variables[x], "contactgroupmember")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->contactgroup_member_name)))
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

		else if(!strcmp(variables[x], "command")) {
			if((result = parse_string_cgivar(THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->command_name)))
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

		else if(!strcmp(variables[x], "masterhostname")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->master_host_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "masterhostgroupname")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->master_hostgroup_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "masterservicedescription")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->master_service_description)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "masterservicegroupname")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->master_servicegroup_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "dependenthostname")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->dependent_host_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "dependenthostgroupname")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->dependent_hostgroup_name)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "dependentservicedescription")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->dependent_service_description)))
					!= RESULT_SUCCESS) {
				break;
				}
			x++;
			}

		else if(!strcmp(variables[x], "dependentservicegroupname")) {
			if((result = parse_string_cgivar(THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					json_root, query_time, authinfo, variables[x],
					variables[x+1], &(cgi_data->dependent_servicegroup_name)))
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

int validate_arguments(json_object *json_root, object_json_cgi_data *cgi_data,
		time_t query_time) {
	int result = RESULT_SUCCESS;
	host *temp_host = NULL;
	hostgroup *temp_hostgroup = NULL;
	servicegroup *temp_servicegroup = NULL;
	service *temp_service = NULL;
	contactgroup *temp_contactgroup = NULL;
	contact *temp_contact = NULL;
	timeperiod *temp_timeperiod = NULL;
	command *temp_command = NULL;
	authdata *authinfo = NULL; /* Currently always NULL because
									get_authentication_information() hasn't
									been called yet, but in case we want to
									use it in the future... */

	/* Validate that required parameters were supplied */
	switch(cgi_data->query) {
	case OBJECT_QUERY_HOSTCOUNT:
		break;
	case OBJECT_QUERY_HOSTLIST:
		break;
	case OBJECT_QUERY_HOST:
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
	case OBJECT_QUERY_HOSTGROUPCOUNT:
		break;
	case OBJECT_QUERY_HOSTGROUPLIST:
		break;
	case OBJECT_QUERY_HOSTGROUP:
		if( NULL == cgi_data->hostgroup_name) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Hostgroup information requested, but no hostgroup name specified."));
			}
		break;
	case OBJECT_QUERY_SERVICECOUNT:
		break;
	case OBJECT_QUERY_SERVICELIST:
		break;
	case OBJECT_QUERY_SERVICE:
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
	case OBJECT_QUERY_SERVICEGROUPCOUNT:
		break;
	case OBJECT_QUERY_SERVICEGROUPLIST:
		break;
	case OBJECT_QUERY_SERVICEGROUP:
		if( NULL == cgi_data->servicegroup_name) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Service group information requested, but no service group name specified."));
			}
		break;
	case OBJECT_QUERY_CONTACTCOUNT:
		break;
	case OBJECT_QUERY_CONTACTLIST:
		break;
	case OBJECT_QUERY_CONTACT:
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
	case OBJECT_QUERY_CONTACTGROUPCOUNT:
		break;
	case OBJECT_QUERY_CONTACTGROUPLIST:
		break;
	case OBJECT_QUERY_CONTACTGROUP:
		if( NULL == cgi_data->contactgroup_name) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Contactgroup information requested, but no contactgroup name specified."));
			}
		break;
	case OBJECT_QUERY_TIMEPERIODCOUNT:
		break;
	case OBJECT_QUERY_TIMEPERIODLIST:
		break;
	case OBJECT_QUERY_TIMEPERIOD:
		if( NULL == cgi_data->timeperiod_name) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Timeperiod information requested, but no timeperiod name specified."));
			}
		break;
	case OBJECT_QUERY_COMMANDCOUNT:
		break;
	case OBJECT_QUERY_COMMANDLIST:
		break;
	case OBJECT_QUERY_COMMAND:
		if( NULL == cgi_data->command_name) {
			result = RESULT_OPTION_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"Command information requested, but no command name specified."));
			}
		break;
	case OBJECT_QUERY_SERVICEDEPENDENCYCOUNT:
		break;
	case OBJECT_QUERY_SERVICEDEPENDENCYLIST:
		break;
	case OBJECT_QUERY_SERVICEESCALATIONCOUNT:
		break;
	case OBJECT_QUERY_SERVICEESCALATIONLIST:
		break;
	case OBJECT_QUERY_HOSTDEPENDENCYCOUNT:
		break;
	case OBJECT_QUERY_HOSTDEPENDENCYLIST:
		break;
	case OBJECT_QUERY_HOSTESCALATIONCOUNT:
		break;
	case OBJECT_QUERY_HOSTESCALATIONLIST:
		break;
	case OBJECT_QUERY_HELP:
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

	/* Validate the requested hostgroup member */
	if( NULL != cgi_data->hostgroup_member_name) {
		temp_host = find_host(cgi_data->hostgroup_member_name);
		if( NULL == temp_host) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The hostgroup member '%s' could not be found.", 
					cgi_data->hostgroup_member_name));
			}
		else {
			cgi_data->hostgroup_member = temp_host;
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

	/* Validate the requested service. Note that the host name is not a
		required parameter for all queries and so it may not make sense to 
		try to obtain the service associated with a service description */
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

	/* Validate the requested servicegroup member host name and service 
		description */
	if( NULL != cgi_data->servicegroup_member_host_name || 
			NULL != cgi_data->servicegroup_member_service_description) {
		if( NULL == cgi_data->servicegroup_member_host_name || 
				NULL == cgi_data->servicegroup_member_service_description) {
			result = RESULT_OPTION_VALUE_MISSING;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"If either the servicegroupmemberhost or servicegroupmemberservice is specified, both must be specified."));
			}
		else {
			temp_service = find_service( cgi_data->servicegroup_member_host_name,
					cgi_data->servicegroup_member_service_description);
			if( NULL == temp_service) {
				result = RESULT_OPTION_VALUE_INVALID;
				json_object_append_object(json_root, "result", 
						json_result(query_time, THISCGI, 
						svm_get_string_from_value(cgi_data->query,
						valid_queries),
						get_query_status(query_status, cgi_data->query),
						(time_t)-1, authinfo, result,
						"The servicegroup member service '%s' on host '%s' could not be found.", 
						cgi_data->servicegroup_member_service_description,
						cgi_data->servicegroup_member_host_name));
				}
			else {
				cgi_data->servicegroup_member = temp_service;
				}
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
					(time_t)-1, authinfo, result,
					"The contact '%s' could not be found.",
					cgi_data->contact_name));
			}
		else {
			cgi_data->contact = temp_contact;
			}
		}

	/* Validate the requested contactgroup member */
	if( NULL != cgi_data->contactgroup_member_name) {
		temp_contact = find_contact(cgi_data->contactgroup_member_name);
		if( NULL == temp_contact) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The contactgroup member '%s' could not be found.", 
					cgi_data->contactgroup_member_name));
			}
		else {
			cgi_data->contactgroup_member = temp_contact;
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

	/* Validate the requested command */
	if( NULL != cgi_data->command_name) {
		temp_command = find_command(cgi_data->command_name);
		if( NULL == temp_command) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result", 
					json_result(query_time, THISCGI, 
					svm_get_string_from_value(cgi_data->query, valid_queries), 
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result, "The command '%s' could not be found.", 
					cgi_data->command_name));
			}
		else {
			cgi_data->command = temp_command;
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

	/* Validate the requested event handler */
	if( NULL != cgi_data->event_handler_name) {
		temp_command = find_command(cgi_data->event_handler_name);
		if( NULL == temp_command) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The event handler '%s' could not be found.",
					cgi_data->event_handler_name));
			}
		else {
			cgi_data->event_handler = temp_command;
			}
		}

	/* Validate the requested master host */
	if( NULL != cgi_data->master_host_name) {
		temp_host = find_host(cgi_data->master_host_name);
		if( NULL == temp_host) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The master host '%s' could not be found.",
					cgi_data->master_host_name));
			}
		else {
			cgi_data->master_host = temp_host;
			}
		}

	/* Validate the requested master hostgroup */
	if( NULL != cgi_data->master_hostgroup_name) {
		temp_hostgroup = find_hostgroup(cgi_data->master_hostgroup_name);
		if( NULL == temp_hostgroup) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The master hostgroup '%s' could not be found.",
					cgi_data->master_hostgroup_name));
			}
		else {
			cgi_data->master_hostgroup = temp_hostgroup;
			}
		}

	/* Validate the requested master servicegroup */
	if( NULL != cgi_data->master_servicegroup_name) {
		temp_servicegroup =
				find_servicegroup(cgi_data->master_servicegroup_name);
		if( NULL == temp_servicegroup) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The master servicegroup '%s' could not be found.",
					cgi_data->master_servicegroup_name));
			}
		else {
			cgi_data->master_servicegroup = temp_servicegroup;
			}
		}

	/* Validate the requested dependent host */
	if( NULL != cgi_data->dependent_host_name) {
		temp_host = find_host(cgi_data->dependent_host_name);
		if( NULL == temp_host) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The dependent host '%s' could not be found.",
					cgi_data->dependent_host_name));
			}
		else {
			cgi_data->dependent_host = temp_host;
			}
		}

	/* Validate the requested dependent hostgroup */
	if( NULL != cgi_data->dependent_hostgroup_name) {
		temp_hostgroup = find_hostgroup(cgi_data->dependent_hostgroup_name);
		if( NULL == temp_hostgroup) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The dependent hostgroup '%s' could not be found.",
					cgi_data->dependent_hostgroup_name));
			}
		else {
			cgi_data->dependent_hostgroup = temp_hostgroup;
			}
		}

	/* Validate the requested dependent servicegroup */
	if( NULL != cgi_data->dependent_servicegroup_name) {
		temp_servicegroup =
				find_servicegroup(cgi_data->dependent_servicegroup_name);
		if( NULL == temp_servicegroup) {
			result = RESULT_OPTION_VALUE_INVALID;
			json_object_append_object(json_root, "result",
					json_result(query_time, THISCGI,
					svm_get_string_from_value(cgi_data->query, valid_queries),
					get_query_status(query_status, cgi_data->query),
					(time_t)-1, authinfo, result,
					"The dependent servicegroup '%s' could not be found.",
					cgi_data->dependent_servicegroup_name));
			}
		else {
			cgi_data->dependent_servicegroup = temp_servicegroup;
			}
		}

	return result;
	}

json_object * json_object_custom_variables(struct customvariablesmember *custom_variables) {

	json_object *json_custom_variables;
	customvariablesmember *temp_custom_variablesmember;

	json_custom_variables = json_new_object();

	for(temp_custom_variablesmember = custom_variables; 
			temp_custom_variablesmember != NULL; 
			temp_custom_variablesmember = temp_custom_variablesmember->next) {
		json_object_append_string(json_custom_variables,
				temp_custom_variablesmember->variable_name, &percent_escapes,
				temp_custom_variablesmember->variable_value);
		}

	return json_custom_variables;
	}

int json_object_host_passes_selection(host *temp_host, int use_parent_host,
		host *parent_host, int use_child_host, host *child_host,
		hostgroup *temp_hostgroup, contact *temp_contact,
		contactgroup *temp_contactgroup, timeperiod *check_timeperiod,
		timeperiod *notification_timeperiod, command *check_command,
		command *event_handler) {

	host *temp_host2;

	/* Skip if user is not authorized for this host */
	if(FALSE == is_authorized_for_host(temp_host, &current_authdata)) {
		return 0;
		}

	/* If the host parent was specified, skip this host if it's parent is 
		not the parent host specified */
	if( 1 == use_parent_host && 
			FALSE == is_host_immediate_child_of_host(parent_host, temp_host)) {
		return 0;
		}

	/* If the hostgroup was specified, skip this host if it is not a member
		of the hostgroup specified */
	if( NULL != temp_hostgroup && 
			( FALSE == is_host_member_of_hostgroup(temp_hostgroup, 
					temp_host))) {
		return 0;
		}

	/* If the contact was specified, skip this host if it does not have
		the contact specified */
	if( NULL != temp_contact && 
			( FALSE == is_contact_for_host(temp_host, temp_contact))) {
		return 0;
		}

	/* If a contactgroup was specified, skip this host if it does not have
		the contactgroup specified */
	if(NULL != temp_contactgroup &&
			(FALSE == is_contactgroup_for_host(temp_host, temp_contactgroup))) {
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

json_object * json_object_host_selectors(int start, int count, 
		int use_parent_host, host *parent_host, int use_child_host,
		host *child_host, hostgroup *temp_hostgroup, contact *temp_contact,
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
	if( NULL != temp_contact) {
		json_object_append_string(json_selectors, "contact", &percent_escapes,
				temp_contact->name);
		}
	if( NULL != temp_contactgroup) {
		json_object_append_string(json_selectors, "contactgroup",
				&percent_escapes, temp_contactgroup->group_name);
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

json_object * json_object_hostcount(int use_parent_host, host *parent_host, 
		int use_child_host, host *child_host, hostgroup *temp_hostgroup, 
		contact *temp_contact, contactgroup *temp_contactgroup,
		timeperiod *check_timeperiod, timeperiod *notification_timeperiod,
		command *check_command, command *event_handler) {

	json_object *json_data;
	host *temp_host;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_host_selectors(0, 0, use_parent_host, parent_host, 
			use_child_host, child_host, temp_hostgroup, temp_contact,
			temp_contactgroup, check_timeperiod, notification_timeperiod,
			check_command, event_handler));

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		if(json_object_host_passes_selection(temp_host, use_parent_host,
				parent_host, use_child_host, child_host, temp_hostgroup, 
				temp_contact, temp_contactgroup, check_timeperiod,
				notification_timeperiod, check_command, event_handler) == 0) {
			continue;
			}

		count++;
		}
	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object * json_object_hostlist(unsigned format_options, int start,
		int count, int details, int use_parent_host, host *parent_host,
		int use_child_host, host *child_host, hostgroup *temp_hostgroup,
		contact *temp_contact, contactgroup *temp_contactgroup,
		timeperiod *check_timeperiod, timeperiod *notification_timeperiod,
		command *check_command, command *event_handler) {

	json_object *json_data;
	json_object *json_hostlist_object = NULL;
	json_array *json_hostlist_array = NULL;
	json_object *json_host_details;
	host *temp_host;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_host_selectors(start, count, use_parent_host, 
			parent_host, use_child_host, child_host, temp_hostgroup, 
			temp_contact, temp_contactgroup, check_timeperiod,
			notification_timeperiod, check_command, event_handler));

	if(details > 0) {
		json_hostlist_object = json_new_object();
		}
	else {
		json_hostlist_array = json_new_array();
		}

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		if(json_object_host_passes_selection(temp_host, use_parent_host,
				parent_host, use_child_host, child_host, temp_hostgroup, 
				temp_contact, temp_contactgroup, check_timeperiod,
				notification_timeperiod, check_command, event_handler) == 0) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			if( details > 0) {
				json_host_details = json_new_object();
				json_object_host_details(json_host_details, format_options, 
						temp_host);
				json_object_append_object(json_hostlist_object, temp_host->name, 
						json_host_details);
				}
			else {
				json_array_append_string(json_hostlist_array, &percent_escapes,
						temp_host->name);
				}
			counted++;
			}
		current++; 
		}

	if(details > 0) {
		json_object_append_object(json_data, "hostlist", json_hostlist_object);
		}
	else {
		json_object_append_array(json_data, "hostlist", json_hostlist_array);
		}

	return json_data;
	}

json_object *json_object_host(unsigned format_options, host *temp_host) {

	json_object *json_host = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_string(json_details, "name", &percent_escapes,
			temp_host->name);
	json_object_host_details(json_details, format_options, temp_host);
	json_object_append_object(json_host, "host", json_details);

	return json_host;
}

void json_object_host_details(json_object *json_details, unsigned format_options, 
		host *temp_host) {

	json_array *json_parent_hosts;
	json_array *json_child_hosts;
	json_array *json_services;
	json_array *json_contactgroups;
	json_array *json_contacts;
	hostsmember *temp_hostsmember;
	servicesmember *temp_servicesmember;
	contactgroupsmember *temp_contact_groupsmember;
#ifdef NSCORE
	contactsmember *temp_contactsmember;
#else
	contact *temp_contact;
#endif

	json_object_append_string(json_details, "name", &percent_escapes,
			temp_host->name);
	json_object_append_string(json_details, "display_name", &percent_escapes,
			temp_host->display_name);
	json_object_append_string(json_details, "alias", &percent_escapes,
			temp_host->alias);
	json_object_append_string(json_details, "address", &percent_escapes,
			temp_host->address);

	json_parent_hosts = json_new_array();
	for(temp_hostsmember = temp_host->parent_hosts; temp_hostsmember != NULL; 
			temp_hostsmember = temp_hostsmember->next) {
		json_array_append_string(json_parent_hosts, &percent_escapes,
				temp_hostsmember->host_name);
		}
	json_object_append_array(json_details, "parent_hosts", json_parent_hosts);

	json_child_hosts = json_new_array();
	for(temp_hostsmember = temp_host->child_hosts; temp_hostsmember != NULL; 
			temp_hostsmember = temp_hostsmember->next) {
		json_array_append_string(json_child_hosts, &percent_escapes,
				temp_hostsmember->host_name);
		}
	json_object_append_array(json_details, "child_hosts", json_child_hosts);

	json_services = json_new_array();
	for(temp_servicesmember = temp_host->services; temp_servicesmember != NULL; 
			temp_servicesmember = temp_servicesmember->next) {
		json_array_append_string(json_services, &percent_escapes,
				temp_servicesmember->service_description);
		}
	json_object_append_array(json_details, "services", json_services);

#ifdef JSON_NAGIOS_4X
	json_object_append_string(json_details, "check_command", &percent_escapes,
			temp_host->check_command);
#else
	json_object_append_string(json_details, "host_check_command", 
			&percent_escapes, temp_host->host_check_command);
#endif

	json_enumeration(json_details, format_options, "initial_state", 
			temp_host->initial_state, svm_host_states);
	json_object_append_real(json_details, "check_interval", 
			temp_host->check_interval);
	json_object_append_real(json_details, "retry_interval", 
			temp_host->retry_interval);
	json_object_append_integer(json_details, "max_attempts", 
			temp_host->max_attempts);
	json_object_append_string(json_details, "event_handler", &percent_escapes,
			temp_host->event_handler);

	json_contactgroups = json_new_array();
	for(temp_contact_groupsmember = temp_host->contact_groups; 
			temp_contact_groupsmember != NULL; 
			temp_contact_groupsmember = temp_contact_groupsmember->next) {
		json_array_append_string(json_contactgroups, &percent_escapes,
				temp_contact_groupsmember->group_name);
		}
	json_object_append_array(json_details, "contact_groups", json_contactgroups);

	json_contacts = json_new_array();
#ifdef NSCORE
	for(temp_contactsmember = temp_host->contacts; 
			temp_contactsmember != NULL; 
			temp_contactsmember = temp_contactsmember->next) {
		json_array_append_string(json_contacts, &percent_escapes,
				temp_contactsmember->contact_name);
		}
#else
	for(temp_contact = contact_list; temp_contact != NULL; 
			temp_contact = temp_contact->next) {
		if(TRUE == is_contact_for_host(temp_host, temp_contact)) {
			json_array_append_string(json_contacts, &percent_escapes,
					temp_contact->name);
			}
		}
#endif
	json_object_append_array(json_details, "contacts", json_contacts);

	json_object_append_real(json_details, "notification_interval", 
			temp_host->notification_interval);
	json_object_append_real(json_details, "first_notification_delay", 
			temp_host->first_notification_delay);
#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "notify_on_down", 
				flag_isset(temp_host->notification_options, OPT_DOWN));
		json_object_append_boolean(json_details, "notify_on_unreachable", 
				flag_isset(temp_host->notification_options, OPT_UNREACHABLE));
		json_object_append_boolean(json_details, "notify_on_recovery", 
				flag_isset(temp_host->notification_options, OPT_RECOVERY));
		json_object_append_boolean(json_details, "notify_on_flapping", 
				flag_isset(temp_host->notification_options, OPT_FLAPPING));
		json_object_append_boolean(json_details, "notify_on_downtime", 
				flag_isset(temp_host->notification_options, OPT_DOWNTIME));
		}
	else {
#endif
		json_bitmask(json_details, format_options, "notifications_options",
				temp_host->notification_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "notify_on_down", 
			temp_host->notify_on_down);
	json_object_append_boolean(json_details, "notify_on_unreachable", 
			temp_host->notify_on_unreachable);
	json_object_append_boolean(json_details, "notify_on_recovery", 
			temp_host->notify_on_recovery);
	json_object_append_boolean(json_details, "notify_on_flapping", 
			temp_host->notify_on_flapping);
	json_object_append_boolean(json_details, "notify_on_downtime", 
			temp_host->notify_on_downtime);
#endif
	json_object_append_string(json_details, "notification_period", 
			&percent_escapes, temp_host->notification_period);
	json_object_append_string(json_details, "check_period", &percent_escapes,
			temp_host->check_period);
	json_object_append_boolean(json_details, "flap_detection_enabled", 
			temp_host->flap_detection_enabled);
	json_object_append_real(json_details, "low_flap_threshold", 
			temp_host->low_flap_threshold);
	json_object_append_real(json_details, "high_flap_threshold", 
			temp_host->high_flap_threshold);
#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details "flap_detection_on_up", 
				flag_isset(temp_host->flap_detection_options, OPT_UP));
		json_object_append_boolean(json_details "flap_detection_on_down", 
				flag_isset(temp_host->flap_detection_options, OPT_DOWN));
		json_object_append_boolean(json_details "flap_detection_on_unreachable", 
				flag_isset(temp_host->flap_detection_options, OPT_UNREACHABLE));
		}
	else {
#endif
		json_bitmask(json_details, format_options, "flap_detection_options",
				temp_host->flap_detection_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "flap_detection_on_up", 
			temp_host->flap_detection_on_up);
	json_object_append_boolean(json_details, "flap_detection_on_down", 
			temp_host->flap_detection_on_down);
	json_object_append_boolean(json_details, "flap_detection_on_unreachable", 
			temp_host->flap_detection_on_unreachable);
#endif

#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "stalk_on_up", 
				flag_isset(temp_host->stalking_options, OPT_UP));
		json_object_append_boolean(json_details, "stalk_on_down", 
				flag_isset(temp_host->stalking_options, OPT_DOWN));
		json_object_append_boolean(json_details, "stalk_on_unreachable", 
				flag_isset(temp_host->stalking_options, OPT_UNREACHABLE));
		}
	else {
#endif
		json_bitmask(json_details, format_options, "stalking_options",
				temp_host->stalking_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "stalk_on_up", 
			temp_host->stalk_on_up);
	json_object_append_boolean(json_details, "stalk_on_down", 
			temp_host->stalk_on_down);
	json_object_append_boolean(json_details, "stalk_on_unreachable", 
			temp_host->stalk_on_unreachable);
#endif

	json_object_append_boolean(json_details, "check_freshness", 
			temp_host->check_freshness);
	json_object_append_integer(json_details, "freshness_threshold", 
			temp_host->freshness_threshold);
	json_object_append_boolean(json_details, "process_performance_data", 
			temp_host->process_performance_data);
	json_object_append_boolean(json_details, "checks_enabled", 
			temp_host->checks_enabled);
#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "accept_passive_host_checks", 
				temp_host->accept_passive_checks);
		}
	else {
#endif
		json_object_append_boolean(json_details, "accept_passive_checks", 
				temp_host->accept_passive_checks);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "accept_passive_host_checks", 
			temp_host->accept_passive_host_checks);
#endif
	json_object_append_boolean(json_details, "event_handler_enabled", 
			temp_host->event_handler_enabled);
	json_object_append_boolean(json_details, "retain_status_information", 
			temp_host->retain_status_information);
	json_object_append_boolean(json_details, "retain_nonstatus_information", 
			temp_host->retain_nonstatus_information);
#ifndef JSON_NAGIOS_4X
	json_object_append_boolean(json_details, "failure_prediction_enabled", 
			temp_host->failure_prediction_enabled);
	json_object_append_string(json_details, "failure_prediction_options", 
			NULL, temp_host->failure_prediction_options);
#endif
#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "obsess_over_host", 
				temp_host->obsess);
		}
	else {
#endif
		json_object_append_boolean(json_details, "obsess", temp_host->obsess);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "obsess_over_host", 
			temp_host->obsess_over_host);
#endif
#ifdef JSON_NAGIOS_4X
	json_object_append_boolean(json_details, "hourly_value", 
			temp_host->hourly_value);
#endif
	json_object_append_string(json_details, "notes", &percent_escapes,
			temp_host->notes);
	json_object_append_string(json_details, "notes_url", &percent_escapes,
			temp_host->notes_url);
	json_object_append_string(json_details, "action_url", &percent_escapes,
			temp_host->action_url);
	json_object_append_string(json_details, "icon_image", &percent_escapes,
			temp_host->icon_image);
	json_object_append_string(json_details, "icon_image_alt", &percent_escapes,
			temp_host->icon_image_alt);
	json_object_append_string(json_details, "vrml_image", &percent_escapes,
			temp_host->vrml_image);
	json_object_append_string(json_details, "statusmap_image", &percent_escapes,
			temp_host->statusmap_image);
	json_object_append_boolean(json_details, "have_2d_coords", 
			temp_host->have_2d_coords);
	json_object_append_integer(json_details, "x_2d", temp_host->x_2d);
	json_object_append_integer(json_details, "y_2d", temp_host->y_2d);
	json_object_append_boolean(json_details, "have_3d_coords", 
			temp_host->have_3d_coords);
	json_object_append_real(json_details, "x_3d", temp_host->x_3d);
	json_object_append_real(json_details, "y_3d", temp_host->y_3d);
	json_object_append_real(json_details, "z_3d", temp_host->z_3d);
	json_object_append_boolean(json_details, "should_be_drawn", 
			temp_host->should_be_drawn);
	json_object_append_object(json_details, "custom_variables", 
			json_object_custom_variables(temp_host->custom_variables));
	}

int json_object_hostgroup_passes_selection(hostgroup *temp_hostgroup, 
		host *temp_hostgroup_member) {

	/* Skip if user is not authorized for this hostgroup */
	if(FALSE == is_authorized_for_hostgroup(temp_hostgroup, 
			&current_authdata)) {
		return 0;
		}

	/* Skip if a hostgroup member is specified and the hostgroup member
		host is not a member of the hostgroup */
	if( NULL != temp_hostgroup_member && 
			( FALSE == is_host_member_of_hostgroup(temp_hostgroup, 
					temp_hostgroup_member))) {
		return 0;
		}

	return 1;
	}

json_object *json_object_hostgroup_selectors(int start, int count, 
		host *temp_hostgroup_member) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if( NULL != temp_hostgroup_member) {
		json_object_append_string(json_selectors, "hostgroupmember", 
				&percent_escapes, temp_hostgroup_member->name);
		}

	return json_selectors;
	}

json_object *json_object_hostgroupcount(unsigned format_options, 
		host *temp_hostgroup_member) {

	json_object *json_data;
	hostgroup *temp_hostgroup;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_hostgroup_selectors(0, 0, temp_hostgroup_member));

	for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; 
			temp_hostgroup = temp_hostgroup->next) {

		if(json_object_hostgroup_passes_selection(temp_hostgroup, 
				temp_hostgroup_member) == 0) {
			continue;
			}

		count++; 
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_object_hostgrouplist(unsigned format_options, int start, 
		int count, int details, host *temp_hostgroup_member) {

	json_object *json_data;
	json_object *json_hostgrouplist_object = NULL;
	json_array *json_hostgrouplist_array = NULL;
	json_object *json_hostgroup_details;
	hostgroup *temp_hostgroup;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_hostgroup_selectors(start, count, 
			temp_hostgroup_member));

	if(details > 0) {
		json_hostgrouplist_object = json_new_object();
		}
	else {
		json_hostgrouplist_array = json_new_array();
		}

	for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; 
			temp_hostgroup = temp_hostgroup->next) {

		if(json_object_hostgroup_passes_selection(temp_hostgroup, 
				temp_hostgroup_member) == 0) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			if( details > 0) {
				json_hostgroup_details = json_new_object();
				json_object_hostgroup_details(json_hostgroup_details, 
						format_options, temp_hostgroup);
				json_object_append_object(json_hostgrouplist_object, 
						temp_hostgroup->group_name, json_hostgroup_details);
				}
			else {
				json_array_append_string(json_hostgrouplist_array, 
						&percent_escapes, temp_hostgroup->group_name);
				}
			counted++;
			}
		current++; 
		}

	if(details > 0) {
		json_object_append_object(json_data, "hostgrouplist", 
				json_hostgrouplist_object);
		}
	else {
		json_object_append_array(json_data, "hostgrouplist", 
				json_hostgrouplist_array);
		}

	return json_data;
	}

json_object *json_object_hostgroup(unsigned format_options,
		hostgroup *temp_hostgroup) {

	json_object *json_hostgroup = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_string(json_details, "group_name", &percent_escapes,
			temp_hostgroup->group_name);
	json_object_hostgroup_details(json_details, format_options, temp_hostgroup);
	json_object_append_object(json_hostgroup, "hostgroup", json_details);

	return json_hostgroup;
}

void json_object_hostgroup_details(json_object *json_details, 
		unsigned format_options, hostgroup *temp_hostgroup) {

	json_array *json_members;
	hostsmember *temp_member;

	json_object_append_string(json_details, "group_name", &percent_escapes,
			temp_hostgroup->group_name);
	json_object_append_string(json_details, "alias", &percent_escapes,
			temp_hostgroup->alias);

	json_members = json_new_array();
	for(temp_member = temp_hostgroup->members; temp_member != NULL; 
			temp_member = temp_member->next) {
		json_array_append_string(json_members, &percent_escapes,
				temp_member->host_name);
		}
	json_object_append_array(json_details, "members", json_members);

	json_object_append_string(json_details, "notes", &percent_escapes,
			temp_hostgroup->notes);
	json_object_append_string(json_details, "notes_url", &percent_escapes,
			temp_hostgroup->notes_url);
	json_object_append_string(json_details, "action_url", &percent_escapes,
			temp_hostgroup->action_url);
	}

int json_object_service_passes_host_selection(host *temp_host, 
		int use_parent_host, host *parent_host, int use_child_host, 
		host *child_host, hostgroup *temp_hostgroup, host *match_host) {

	host *temp_host2;

	/* Skip if user is not authorized for this host */
	if(FALSE == is_authorized_for_host(temp_host, &current_authdata)) {
		return 0;
	}

	/* If the host parent was specified, skip the services whose host is 
		not a child of the parent host specified */
	if( 1 == use_parent_host && NULL != temp_host && 
			FALSE == is_host_immediate_child_of_host(parent_host, temp_host)) {
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
	if( NULL != match_host && NULL != temp_host && temp_host != match_host) {
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

int json_object_service_passes_service_selection(service *temp_service, 
		servicegroup *temp_servicegroup, contact *temp_contact, 
		char *service_description, char *parent_service_name,
		char *child_service_name, contactgroup *temp_contactgroup,
		timeperiod *check_timeperiod, timeperiod *notification_timeperiod,
		command *check_command, command *event_handler) {

	servicesmember *temp_servicesmember;

	/* Skip if user is not authorized for this service */
	if(FALSE == is_authorized_for_service(temp_service, &current_authdata)) {
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

	/* If the service description was supplied, skip the services that do not
		have this service description */
	if((NULL != service_description) && 
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

	/* If an event handler was specified, skip this service if it does not
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

json_object *json_object_service_selectors(int start, int count, 
		int use_parent_host, host *parent_host, int use_child_host,
		host *child_host, hostgroup *temp_hostgroup, host *match_host, 
		servicegroup *temp_servicegroup, contact *temp_contact, 
		char *service_description, char *parent_service_name,
		char *child_service_name, contactgroup *temp_contactgroup,
		timeperiod *check_timeperiod, timeperiod *notification_timeperiod,
		command *check_command, command *event_handler) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if( NULL != match_host) {
		json_object_append_string(json_selectors, "host", &percent_escapes,
				match_host->name);
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
		json_object_append_string(json_selectors, "servicegroup", &percent_escapes,
				temp_servicegroup->group_name);
		}
	if(NULL != temp_contact) {
		json_object_append_string(json_selectors, "contact",&percent_escapes,
				temp_contact->name);
		}
	if(NULL != temp_contactgroup) {
		json_object_append_string(json_selectors, "contactgroup",
				&percent_escapes, temp_contactgroup->group_name);
		}
	if( NULL != service_description) {
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

json_object *json_object_servicecount(host *match_host, int use_parent_host, 
		host *parent_host, int use_child_host, host *child_host, 
		hostgroup *temp_hostgroup, servicegroup *temp_servicegroup, 
		contact *temp_contact, char *service_description,
		char *parent_service_name, char *child_service_name,
		contactgroup *temp_contactgroup, timeperiod *check_timeperiod,
		timeperiod *notification_timeperiod, command *check_command,
		command *event_handler) {

	json_object *json_data;
	host *temp_host;
	service *temp_service;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_service_selectors(0, 0, use_parent_host, parent_host, 
			use_child_host, child_host, temp_hostgroup, match_host, 
			temp_servicegroup, temp_contact, service_description,
			parent_service_name, child_service_name, temp_contactgroup,
			check_timeperiod, notification_timeperiod, check_command,
			event_handler));

	for(temp_service = service_list; temp_service != NULL; 
			temp_service = temp_service->next) {

		temp_host = find_host(temp_service->host_name);
		if(NULL == temp_host) {
			continue;
			}

		if(json_object_service_passes_host_selection(temp_host, 
				use_parent_host, parent_host, use_child_host, child_host, 
				temp_hostgroup, match_host) == 0) {
			continue;
			}

		if(json_object_service_passes_service_selection(temp_service, 
				temp_servicegroup, temp_contact, service_description,
				parent_service_name, child_service_name,
				temp_contactgroup, check_timeperiod,
				notification_timeperiod, check_command, event_handler) == 0) {
			continue;
			}

		count++; 
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_object_servicelist(unsigned format_options, int start, 
		int count, int details, host *match_host, int use_parent_host, 
		host *parent_host, int use_child_host, host *child_host, 
		hostgroup *temp_hostgroup, servicegroup *temp_servicegroup, 
		contact *temp_contact, char *service_description,
		char *parent_service_name, char *child_service_name,
		contactgroup *temp_contactgroup, timeperiod *check_timeperiod,
		timeperiod *notification_timeperiod, command *check_command,
		command *event_handler) {

	json_object *json_data;
	json_object *json_hostlist;
	json_object *json_servicelist_object = NULL;
	json_array *json_servicelist_array = NULL;
	json_object *json_service_details;
	host *temp_host;
	service *temp_service;
	int current = 0;
	int counted = 0;
	int service_count;
	char *buf;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_service_selectors(start, count, use_parent_host, 
			parent_host, use_child_host, child_host, temp_hostgroup, match_host,
			temp_servicegroup, temp_contact, service_description,
			parent_service_name, child_service_name, temp_contactgroup,
			check_timeperiod, notification_timeperiod, check_command,
			event_handler));

	json_hostlist = json_new_object();

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
		if(json_object_service_passes_host_selection(temp_host, use_parent_host,
					parent_host, use_child_host, child_host, temp_hostgroup, 
					match_host) == 0) {
				continue;
				}

		service_count = 0;
		if(details > 0) {
			json_servicelist_object = json_new_object();
			}
		else {
			json_servicelist_array = json_new_array();
			}

		for(temp_service = service_list; temp_service != NULL; 
				temp_service = temp_service->next) {
	
			if(json_object_service_passes_service_selection(temp_service,
					temp_servicegroup, temp_contact, 
					service_description, parent_service_name,
					child_service_name, temp_contactgroup,
					check_timeperiod, notification_timeperiod,
					check_command, event_handler) == 0) {
				continue;
				}

			/* If this service isn't on the host we're currently working with, 
					skip it */
			if( strcmp( temp_host->name, temp_service->host_name)) continue;

			/* If the current item passes the start and limit tests, display it */
			if( passes_start_and_count_limits(start, count, current, counted)) {
				if( details > 0) {
					json_service_details = json_new_object();
					json_object_service_details(json_service_details, 
							format_options, temp_service);
					asprintf(&buf, "%s", 
							temp_service->description);
					json_object_append_object(json_servicelist_object, buf, 
							json_service_details);
					}
				else {
					json_array_append_string(json_servicelist_array,
							&percent_escapes, temp_service->description);
					}
				counted++;
				service_count++;
				}
			current++; 
			}

		if(service_count > 0) {
			if(details > 0) {
				json_object_append_object(json_hostlist, temp_host->name, 
						json_servicelist_object);
				}
			else {
				json_object_append_array(json_hostlist, temp_host->name, 
						json_servicelist_array);
				}
			}
		}

	json_object_append_object(json_data, "servicelist", json_hostlist);
	return json_data;
	}

json_object *json_object_service(unsigned format_options, service *temp_service) {

	json_object *json_service = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_string(json_details, "host_name", &percent_escapes,
			temp_service->host_name);
	json_object_append_string(json_details, "description", &percent_escapes,
			temp_service->description);
	json_object_service_details(json_details, format_options, temp_service);
	json_object_append_object(json_service, "service", json_details);

	return json_service;
}

void json_object_service_details(json_object *json_details, 
		unsigned format_options, service *temp_service) {

	json_array *json_contactgroups;
	json_array *json_contacts;
	contactgroupsmember *temp_contact_groupsmember;
#ifdef NSCORE
	contactsmember *temp_contactsmember;
#else
	contact *temp_contact;
#endif
#ifdef JSON_NAGIOS_4X
	servicesmember *temp_servicesmember;
	json_object *json_parent_service;
	json_array *json_parent_services;
	json_object *json_child_service;
	json_array *json_child_services;
#endif

	json_object_append_string(json_details, "host_name", &percent_escapes,
			temp_service->host_name);
	json_object_append_string(json_details, "description", &percent_escapes,
			temp_service->description);
	json_object_append_string(json_details, "display_name", &percent_escapes,
			temp_service->display_name);
#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_string(json_details, "service_check_command",
				&percent_escapes, temp_service->check_command);
		}
	else {
#endif
		json_object_append_string(json_details, "check_command",
				&percent_escapes, temp_service->check_command);
#if 0
		}
#endif
#else
	json_object_append_string(json_details, "service_check_command",
			&percent_escapes, temp_service->service_check_command);
#endif
	json_object_append_string(json_details, "event_handler", &percent_escapes,
			temp_service->event_handler);

	json_enumeration(json_details, format_options, "initial_state", 
			temp_service->initial_state, svm_service_states);

	json_object_append_real(json_details, "check_interval", 
			temp_service->check_interval);
	json_object_append_real(json_details, "retry_interval", 
			temp_service->retry_interval);
	json_object_append_integer(json_details, "max_attempts", 
			temp_service->max_attempts);
	json_object_append_boolean(json_details, "parallelize",  
			temp_service->parallelize);

	json_contactgroups = json_new_array();
	for(temp_contact_groupsmember = temp_service->contact_groups; 
			temp_contact_groupsmember != NULL; 
			temp_contact_groupsmember = temp_contact_groupsmember->next) {
		json_array_append_string(json_contactgroups, &percent_escapes,
				temp_contact_groupsmember->group_name);
		}
	json_object_append_array(json_details, "contact_groups", json_contactgroups);

	json_contacts = json_new_array();
#ifdef NSCORE
	for(temp_contactsmember = temp_service->contacts; 
			temp_contactsmember != NULL; 
			temp_contactsmember = temp_contactsmember->next) {
		json_array_append_string(json_contacts, &percent_escapes,
				temp_contactsmember->contact_name);
		}
#else
	for(temp_contact = contact_list; temp_contact != NULL; 
			temp_contact = temp_contact->next) {
		if(TRUE == is_contact_for_service(temp_service, temp_contact)) {
			json_array_append_string(json_contacts, &percent_escapes,
					temp_contact->name);
			}
		}
#endif
	json_object_append_array(json_details, "contacts", json_contacts);

	json_object_append_real(json_details, "notification_interval", 
			temp_service->notification_interval);
	json_object_append_real(json_details, "first_notification_delay", 
			temp_service->first_notification_delay);

#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "notify_on_unknown", 
				flag_isset(temp_service->notification_options, OPT_UNKNOWN));
		json_object_append_boolean(json_details, "notify_on_warning", 
				flag_isset(temp_service->notification_options, OPT_WARNING));
		json_object_append_boolean(json_details, "notify_on_critical", 
				flag_isset(temp_service->notification_options, OPT_CRITICAL));
		json_object_append_boolean(json_details, "notify_on_recovery", 
				flag_isset(temp_service->notification_options, OPT_RECOVERY));
		json_object_append_boolean(json_details, "notify_on_flapping", 
				flag_isset(temp_service->notification_options, OPT_FLAPPING));
		json_object_append_boolean(json_details, "notify_on_downtime", 
				flag_isset(temp_service->notification_options, OPT_DOWNTIME));
		}
	else {
#endif
		json_bitmask(json_details, format_options, "notifications_options",
				temp_service->notification_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "notify_on_unknown", 
			temp_service->notify_on_unknown);
	json_object_append_boolean(json_details, "notify_on_warning", 
			temp_service->notify_on_warning);
	json_object_append_boolean(json_details, "notify_on_critical", 
			temp_service->notify_on_critical);
	json_object_append_boolean(json_details, "notify_on_recovery", 
			temp_service->notify_on_recovery);
	json_object_append_boolean(json_details, "notify_on_flapping", 
			temp_service->notify_on_flapping);
	json_object_append_boolean(json_details, "notify_on_downtime", 
			temp_service->notify_on_downtime);
#endif

#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "stalk_on_ok", 
				flag_isset(temp_service->stalking_options, OPT_OK));
		json_object_append_boolean(json_details, "stalk_on_warning", 
				flag_isset(temp_service->stalking_options, OPT_WARNING));
		json_object_append_boolean(json_details, "stalk_on_unknown", 
				flag_isset(temp_service->stalking_options, OPT_UNKNOWN));
		json_object_append_boolean(json_details, "stalk_on_critical", 
				flag_isset(temp_service->stalking_options, OPT_CRITICAL));
		}
	else {
#endif
		json_bitmask(json_details, format_options, "stalking_options",
				temp_service->stalking_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "stalk_on_ok", 
			temp_service->stalk_on_ok);
	json_object_append_boolean(json_details, "stalk_on_warning", 
			temp_service->stalk_on_warning);
	json_object_append_boolean(json_details, "stalk_on_unknown", 
			temp_service->stalk_on_unknown);
	json_object_append_boolean(json_details, "stalk_on_critical", 
			temp_service->stalk_on_critical);
#endif

	json_object_append_boolean(json_details, "is_volatile", 
			temp_service->is_volatile);
	json_object_append_string(json_details, "notification_period",
			&percent_escapes, temp_service->notification_period);
	json_object_append_string(json_details, "check_period", &percent_escapes,
			temp_service->check_period);
	json_object_append_boolean(json_details, "flap_detection_enabled", 
			temp_service->flap_detection_enabled);
	json_object_append_real(json_details, "low_flap_threshold", 
			temp_service->low_flap_threshold);
	json_object_append_real(json_details, "high_flap_threshold", 
			temp_service->high_flap_threshold);

#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "flap_dectetion_on_ok", 
				flag_isset(temp_service->flap_detection_options, OPT_OK));
		json_object_append_boolean(json_details, "flap_dectetion_on_warning", 
				flag_isset(temp_service->flap_detection_options, OPT_WARNING));
		json_object_append_boolean(json_details, "flap_dectetion_on_unknown", 
				flag_isset(temp_service->flap_detection_options, OPT_UNKNOWN));
		json_object_append_boolean(json_details, "flap_detection_on_critical", 
				flag_isset(temp_service->flap_detection_options, OPT_CRITICAL));
		}
	else {
#endif
		json_bitmask(json_details, format_options, "flap_detection_options",
				temp_service->flap_detection_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "flap_dectetion_on_ok", 
			temp_service->flap_detection_on_ok);
	json_object_append_boolean(json_details, "flap_dectetion_on_warning", 
			temp_service->flap_detection_on_warning);
	json_object_append_boolean(json_details, "flap_dectetion_on_unknown", 
			temp_service->flap_detection_on_unknown);
	json_object_append_boolean(json_details, "flap_detection_on_critical", 
			temp_service->flap_detection_on_critical);
#endif

	json_object_append_boolean(json_details, "process_performance_data", 
			temp_service->process_performance_data);
	json_object_append_boolean(json_details, "check_freshness", 
			temp_service->check_freshness);
	json_object_append_integer(json_details, "freshness_threshold", 
			temp_service->freshness_threshold);
#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, 
				"accept_passive_service_checks",
				temp_service->accept_passive_checks);
		}
	else {
#endif
		json_object_append_boolean(json_details, "accept_passive_checks", 
				temp_service->accept_passive_checks);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "accept_passive_service_checks", 
			temp_service->accept_passive_service_checks);
#endif
	json_object_append_boolean(json_details, "event_handler_enabled", 
			temp_service->event_handler_enabled);
	json_object_append_boolean(json_details, "checks_enabled", 
			temp_service->checks_enabled);
	json_object_append_boolean(json_details, "retain_status_information", 
			temp_service->retain_status_information);
	json_object_append_boolean(json_details, "retain_nonstatus_information", 
			temp_service->retain_nonstatus_information);
	json_object_append_boolean(json_details, "notifications_enabled", 
			temp_service->notifications_enabled);
#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "obsess_over_service", 
				temp_service->obsess);
		}
	else {
#endif
		json_object_append_boolean(json_details, "obsess", temp_service->obsess);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "obsess_over_service", 
			temp_service->obsess_over_service);
#endif
#ifndef JSON_NAGIOS_4X
	json_object_append_boolean(json_details, "failure_prediction_enabled", 
			temp_service->failure_prediction_enabled);
	json_object_append_string(json_details, "failure_prediction_options", 
			NULL, temp_service->failure_prediction_options);
#endif
#ifdef JSON_NAGIOS_4X
	json_object_append_integer(json_details, "hourly_value", 
			temp_service->hourly_value);
#endif

#ifdef JSON_NAGIOS_4X
	json_parent_services = json_new_array();
	for(temp_servicesmember = temp_service->parents; 
			temp_servicesmember != NULL; 
			temp_servicesmember = temp_servicesmember->next) {
		json_parent_service = json_new_object();
		json_object_append_string(json_parent_service, "host_name",
				&percent_escapes, temp_servicesmember->host_name);
		json_object_append_string(json_parent_service, "service_description",
				&percent_escapes, temp_servicesmember->service_description);
		json_array_append_object(json_parent_services, json_parent_service);
		}
	json_object_append_array(json_details, "parents", json_parent_services);

	json_child_services = json_new_array();
	for(temp_servicesmember = temp_service->children;
			temp_servicesmember != NULL;
			temp_servicesmember = temp_servicesmember->next) {
		json_child_service = json_new_object();
		json_object_append_string(json_child_service, "host_name",
				&percent_escapes, temp_servicesmember->host_name);
		json_object_append_string(json_child_service, "service_description",
				&percent_escapes, temp_servicesmember->service_description);
		json_array_append_object(json_child_services, json_child_service);
		}
	json_object_append_array(json_details, "children", json_child_services);
#endif

	json_object_append_string(json_details, "notes", &percent_escapes,
			temp_service->notes);
	json_object_append_string(json_details, "notes_url", &percent_escapes,
			temp_service->notes_url);
	json_object_append_string(json_details, "action_url", &percent_escapes,
			temp_service->action_url);
	json_object_append_string(json_details, "icon_image", &percent_escapes,
			temp_service->icon_image);
	json_object_append_string(json_details, "icon_image_alt", &percent_escapes,
			temp_service->icon_image_alt);
	json_object_append_object(json_details, "custom_variables", 
			json_object_custom_variables(temp_service->custom_variables));
	}

int json_object_servicegroup_passes_selection(servicegroup *temp_servicegroup,
		service *temp_servicegroup_member) {

	/* Skip if user is not authorized for this hostgroup */
	if(FALSE == is_authorized_for_servicegroup(temp_servicegroup, 
			&current_authdata)) {
		return 0;
		}

	/* Skip if a servicegroup member is specified and the servicegroup 
		member service is not a member of the servicegroup */
	if( NULL != temp_servicegroup_member && 
			( FALSE == is_service_member_of_servicegroup(temp_servicegroup, 
					temp_servicegroup_member))) {
		return 0;
		}

	return 1;
	}

json_object * json_object_servicegroup_selectors(int start, int count, 
		service *temp_servicegroup_member) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if( NULL != temp_servicegroup_member) {
		json_object_append_string(json_selectors, "servicegroupmemberhost", 
				&percent_escapes, temp_servicegroup_member->host_name);
		json_object_append_string(json_selectors, "servicegroupmemberservice", 
				&percent_escapes, temp_servicegroup_member->description);
		}

	return json_selectors;
	}

json_object *json_object_servicegroupcount(service *temp_servicegroup_member) {

	json_object *json_data;
	servicegroup *temp_servicegroup;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_servicegroup_selectors(0, 0, 
			temp_servicegroup_member));

	for(temp_servicegroup = servicegroup_list; temp_servicegroup != NULL; 
			temp_servicegroup = temp_servicegroup->next) {

		if(json_object_servicegroup_passes_selection(temp_servicegroup,
				temp_servicegroup_member) == 0) {
			continue;
			}

		count++; 
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_object_servicegrouplist(unsigned format_options, int start, 
		int count, int details, service *temp_servicegroup_member) {

	json_object *json_data;
	json_object *json_servicegrouplist_object = NULL;
	json_array *json_servicegrouplist_array = NULL;
	json_object *json_servicegroup_details;
	servicegroup *temp_servicegroup;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_servicegroup_selectors(start, count, 
			temp_servicegroup_member));

	if(details > 0) {
		json_servicegrouplist_object = json_new_object();
		}
	else {
		json_servicegrouplist_array = json_new_array();
		}

	for(temp_servicegroup = servicegroup_list; temp_servicegroup != NULL; 
			temp_servicegroup = temp_servicegroup->next) {

		if(json_object_servicegroup_passes_selection(temp_servicegroup,
				temp_servicegroup_member) == 0) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			if( details > 0) {
				json_servicegroup_details = json_new_object();
				json_object_servicegroup_details(json_servicegroup_details, 
						format_options, temp_servicegroup);
				json_object_append_object(json_servicegrouplist_object, 
						temp_servicegroup->group_name,
						json_servicegroup_details);
				}
			else {
				json_object_append_string(json_servicegrouplist_array, NULL, 
						&percent_escapes, temp_servicegroup->group_name);
				}
			counted++;
			}
		current++; 
		}

	if(details > 0) {
		json_object_append_object(json_data, "servicegrouplist", 
				json_servicegrouplist_object);
		}
	else {
		json_object_append_array(json_data, "servicegrouplist", 
				json_servicegrouplist_array);
		}

	return json_data;
	}

json_object * json_object_servicegroup(unsigned format_options, 
		servicegroup *temp_servicegroup) {

	json_object *json_servicegroup = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_string(json_details, "group_name", 
			&percent_escapes, temp_servicegroup->group_name);
	json_object_servicegroup_details(json_details, format_options, 
			temp_servicegroup);
	json_object_append_object(json_servicegroup, "servicegroup", json_details);

	return json_servicegroup;
}

void json_object_servicegroup_details(json_object *json_details, 
		unsigned format_options, servicegroup *temp_servicegroup) {

	json_array *json_members;
	servicesmember *temp_member;
	json_object *json_member;

	json_object_append_string(json_details, "group_name", &percent_escapes,
			temp_servicegroup->group_name);
	json_object_append_string(json_details, "alias", &percent_escapes,
			temp_servicegroup->alias);

	json_members = json_new_array();
	for(temp_member = temp_servicegroup->members; temp_member != NULL; 
			temp_member = temp_member->next) {
		json_member = json_new_object();
		json_object_append_string(json_member, "host_name", &percent_escapes,
				temp_member->host_name);
		json_object_append_string(json_member, "service_description",
				&percent_escapes, temp_member->service_description);
		json_array_append_object(json_members, json_member);
		}
	json_object_append_array(json_details, "members", json_members);

	json_object_append_string(json_details, "notes", &percent_escapes,
			temp_servicegroup->notes);
	json_object_append_string(json_details, "notes_url", &percent_escapes,
			temp_servicegroup->notes_url);
	json_object_append_string(json_details, "action_url", &percent_escapes,
			temp_servicegroup->action_url);
	}

int json_object_contact_passes_selection(contact *temp_contact,
		contactgroup *temp_contactgroup,
		timeperiod *host_notification_timeperiod,
		timeperiod *service_notification_timeperiod) {

	/* If the contactgroup was specified, skip the contacts that are not 
		members of the contactgroup specified */
	if( NULL != temp_contactgroup && 
			( FALSE == is_contact_member_of_contactgroup(temp_contactgroup, 
					temp_contact))) {
		return 0;
		}

	/* If a host notification timeperiod was specified, skip this contact
		if it does not have the host notification timeperiod specified */
	if(NULL != host_notification_timeperiod && (host_notification_timeperiod !=
			temp_contact->host_notification_period_ptr)) {
		return 0;
		}

	/* If a service notification timeperiod was specified, skip this contact
		if it does not have the service notification timeperiod specified */
	if(NULL != service_notification_timeperiod &&
			(service_notification_timeperiod !=
			temp_contact->service_notification_period_ptr)) {
		return 0;
		}

	return 1;
	}

json_object *json_object_contact_display_selectors(int start, int count, 
		contactgroup *temp_contactgroup,
		timeperiod *host_notification_timeperiod,
		timeperiod *service_notification_timeperiod) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if( NULL != temp_contactgroup) {
		json_object_append_string(json_selectors, "contactgroup",
				&percent_escapes, temp_contactgroup->group_name);
		}
	if( NULL != host_notification_timeperiod) {
		json_object_append_string(json_selectors, "hostnotificationtimeperiod",
				&percent_escapes, host_notification_timeperiod->name);
		}
	if( NULL != service_notification_timeperiod) {
		json_object_append_string(json_selectors,
				"servicenotificationtimeperiod", &percent_escapes,
				service_notification_timeperiod->name);
		}

	return json_selectors;
	}

json_object *json_object_contactcount(contactgroup *temp_contactgroup,
		timeperiod *host_notification_timeperiod,
		timeperiod *service_notification_timeperiod) {

	json_object *json_data;
	contact *temp_contact;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_contact_display_selectors(0, 0, temp_contactgroup,
			host_notification_timeperiod, service_notification_timeperiod));

	for(temp_contact = contact_list; temp_contact != NULL; 
			temp_contact = temp_contact->next) {

		if(json_object_contact_passes_selection(temp_contact, 
				temp_contactgroup, host_notification_timeperiod,
				service_notification_timeperiod) == 0) {
			continue;
			}

		count++; 
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_object_contactlist(unsigned format_options, int start, 
		int count, int details, contactgroup *temp_contactgroup,
		timeperiod *host_notification_timeperiod,
		timeperiod *service_notification_timeperiod) {

	json_object *json_data;
	json_object *json_contactlist_object = NULL;
	json_array *json_contactlist_array = NULL;
	json_object *json_contact_details;
	contact *temp_contact;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_contact_display_selectors(start, count,
			temp_contactgroup, host_notification_timeperiod,
			service_notification_timeperiod));

	if(details > 0) {
		json_contactlist_object = json_new_object();
		}
	else {
		json_contactlist_array = json_new_array();
		}

	for(temp_contact = contact_list; temp_contact != NULL; 
			temp_contact = temp_contact->next) {

		if(json_object_contact_passes_selection(temp_contact, 
				temp_contactgroup, host_notification_timeperiod,
				service_notification_timeperiod) == 0) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			if( details > 0) {
				json_contact_details = json_new_object();
				json_object_contact_details(json_contact_details, 
						format_options, temp_contact);
				json_object_append_object(json_contactlist_object, 
						temp_contact->name, json_contact_details);
				}
			else {
				json_array_append_string(json_contactlist_array, 
						&percent_escapes, temp_contact->name);
				}
			counted++;
			}
		current++; 
		}

	if(details > 0) {
		json_object_append_object(json_data, "contactlist", 
				json_contactlist_object);
		}
	else {
		json_object_append_array(json_data, "contactlist", 
				json_contactlist_array);
		}

	return json_data;
	}

json_object *json_object_contact(unsigned format_options, contact *temp_contact) {

	json_object *json_contact = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_string(json_details, "name", &percent_escapes,
			temp_contact->name);
	json_object_contact_details(json_details, format_options, temp_contact);
	json_object_append_object(json_contact, "contact", json_details);

	return json_contact;
}

void json_object_contact_details(json_object *json_details, 
		unsigned format_options, contact *temp_contact) {

	json_array *json_addresses;
	json_array *json_host_commands;
	json_array *json_service_commands;
	int x;
	commandsmember *temp_commandsmember;

	json_object_append_string(json_details, "name", &percent_escapes,
			temp_contact->name);
	json_object_append_string(json_details, "alias", &percent_escapes,
			temp_contact->alias);
	json_object_append_string(json_details, "email", &percent_escapes,
			temp_contact->email);
	json_object_append_string(json_details, "pager", &percent_escapes,
			temp_contact->pager);

	json_addresses = json_new_array();
	for( x = 0; x < MAX_CONTACT_ADDRESSES; x++) {
		if( NULL != temp_contact->address[ x]) {
			json_array_append_string(json_addresses, &percent_escapes,
					temp_contact->address[ x]);
			}
		}
	json_object_append_array(json_details, "addresses", json_addresses);

	json_host_commands = json_new_array();
	for(temp_commandsmember = temp_contact->host_notification_commands; 
			temp_commandsmember != NULL; 
			temp_commandsmember = temp_commandsmember->next) {
		json_array_append_string(json_host_commands, &percent_escapes,
				temp_commandsmember->command);
		}
	json_object_append_array(json_details, "host_notification_commands", 
			json_host_commands);

	json_service_commands = json_new_array();
	for(temp_commandsmember = temp_contact->service_notification_commands; 
			temp_commandsmember != NULL; 
			temp_commandsmember = temp_commandsmember->next) {
		json_array_append_string(json_service_commands, &percent_escapes,
				temp_commandsmember->command);
		}
	json_object_append_array(json_details, "service_notification_commands", 
			json_service_commands);

#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "notify_on_service_unknown", 
				flag_isset(temp_contact->service_notification_options, 
				OPT_UNKNOWN));
		json_object_append_boolean(json_details, "notify_on_service_warning", 
				flag_isset(temp_contact->service_notification_options, 
				OPT_WARNING));
		json_object_append_boolean(json_details, "notify_on_service_critical", 
				flag_isset(temp_contact->service_notification_options, 
				OPT_CRITICAL));
		json_object_append_boolean(json_details, "notify_on_service_recovery", 
				flag_isset(temp_contact->service_notification_options, 
				OPT_RECOVERY));
		json_object_append_boolean(json_details, "notify_on_service_flapping", 
				flag_isset(temp_contact->service_notification_options, 
				OPT_FLAPPING));
		json_object_append_boolean(json_details, "notify_on_service_downtime", 
				flag_isset(temp_contact->service_notification_options, 
				OPT_DOWNTIME));
		}
	else {
#endif
		json_bitmask(json_details, format_options, 
				"service_notification_options",
				temp_contact->service_notification_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "notify_on_service_unknown", 
			temp_contact->notify_on_service_unknown);
	json_object_append_boolean(json_details, "notify_on_service_warning", 
			temp_contact->notify_on_service_warning);
	json_object_append_boolean(json_details, "notify_on_service_critical", 
			temp_contact->notify_on_service_critical);
	json_object_append_boolean(json_details, "notify_on_service_recovery", 
			temp_contact->notify_on_service_recovery);
	json_object_append_boolean(json_details, "notify_on_service_flapping", 
			temp_contact->notify_on_service_flapping);
	json_object_append_boolean(json_details, "notify_on_service_downtime", 
			temp_contact->notify_on_service_downtime);
#endif

#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "notify_on_host_down", 
				flag_isset(temp_contact->host_notification_options, OPT_DOWN));
		json_object_append_boolean(json_details, "notify_on_host_unreachable", 
				flag_isset(temp_contact->host_notification_options, 
				OPT_UNREACHABLE));
		json_object_append_boolean(json_details, "notify_on_host_recovery", 
				flag_isset(temp_contact->host_notification_options, 
				OPT_RECOVERY));
		json_object_append_boolean(json_details, "notify_on_host_flapping", 
				flag_isset(temp_contact->host_notification_options, 
				OPT_FLAPPING));
		json_object_append_boolean(json_details, "notify_on_host_downtime", 
				flag_isset(temp_contact->host_notification_options, 
				OPT_DOWNTIME));
		}
	else {
#endif
		json_bitmask(json_details, format_options, "host_notification_options",
				temp_contact->host_notification_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "notify_on_host_down", 
			temp_contact->notify_on_host_down);
	json_object_append_boolean(json_details, "notify_on_host_unreachable", 
			temp_contact->notify_on_host_unreachable);
	json_object_append_boolean(json_details, "notify_on_host_recovery", 
			temp_contact->notify_on_host_recovery);
	json_object_append_boolean(json_details, "notify_on_host_flapping", 
			temp_contact->notify_on_host_flapping);
	json_object_append_boolean(json_details, "notify_on_host_downtime", 
			temp_contact->notify_on_host_downtime);
#endif

	json_object_append_string(json_details, "host_notification_period", 
			&percent_escapes, temp_contact->host_notification_period);
	json_object_append_string(json_details, "service_notification_period", 
			&percent_escapes, temp_contact->service_notification_period);
	json_object_append_boolean(json_details, "host_notifications_enabled", 
			temp_contact->host_notifications_enabled);
	json_object_append_boolean(json_details, "service_notifications_enabled", 
			temp_contact->service_notifications_enabled);
	json_object_append_boolean(json_details, "can_submit_commands", 
			temp_contact->can_submit_commands);
	json_object_append_boolean(json_details, "retain_status_information", 
			temp_contact->retain_status_information);
	json_object_append_boolean(json_details, "retain_nonstatus_information", 
			temp_contact->retain_nonstatus_information);
#ifdef JSON_NAGIOS_4X
	json_object_append_integer(json_details, "minimum_value", 
			temp_contact->minimum_value);
#endif
	json_object_append_object(json_details, "custom_variables", 
			json_object_custom_variables(temp_contact->custom_variables));
	}

int json_object_contactgroup_passes_selection(contactgroup *temp_contactgroup,
		contact *temp_contactgroup_member) {

	/* Skip if a contactgroup member is specified and the contactgroup 
		member contact is not a member of the contactgroup */
	if( NULL != temp_contactgroup_member && 
			( FALSE == is_contact_member_of_contactgroup(temp_contactgroup, 
					temp_contactgroup_member))) {
		return 0;
		}

	return 1;
	}

json_object *json_object_contactgroup_display_selectors( int start, int count, 
		contact *temp_contactgroup_member) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if( NULL != temp_contactgroup_member) {
		json_object_append_string(json_selectors, "contactgroupmember", 
				&percent_escapes, temp_contactgroup_member->name);
		}

	return json_selectors;
	}

json_object *json_object_contactgroupcount(contact * temp_contactgroup_member) {

	json_object *json_data;
	contactgroup *temp_contactgroup;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_contactgroup_display_selectors(0, 0, 
			temp_contactgroup_member));

	for(temp_contactgroup = contactgroup_list; temp_contactgroup != NULL; 
			temp_contactgroup = temp_contactgroup->next) {

		if(json_object_contactgroup_passes_selection(temp_contactgroup,
				temp_contactgroup_member) == 0) {
			continue;
			}

		count++; 
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_object_contactgrouplist(unsigned format_options, int start, 
		int count, int details, contact * temp_contactgroup_member) {

	json_object *json_data;
	json_object *json_contactgrouplist_object = NULL;
	json_array *json_contactgrouplist_array = NULL;
	json_object *json_contactgroup_details;
	contactgroup *temp_contactgroup;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_contactgroup_display_selectors(start, count, 
			temp_contactgroup_member));

	if(details > 0) {
		json_contactgrouplist_object = json_new_object();
		}
	else {
		json_contactgrouplist_array = json_new_array();
		}

	for(temp_contactgroup = contactgroup_list; temp_contactgroup != NULL; 
			temp_contactgroup = temp_contactgroup->next) {

		if(json_object_contactgroup_passes_selection(temp_contactgroup,
				temp_contactgroup_member) == 0) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			if( details > 0) {
				json_contactgroup_details = json_new_object();
				json_object_contactgroup_details(json_contactgroup_details, 
						format_options, temp_contactgroup);
				json_object_append_object(json_contactgrouplist_object, 
						temp_contactgroup->group_name, json_contactgroup_details);
				}
			else {
				json_array_append_string(json_contactgrouplist_array,
						&percent_escapes, temp_contactgroup->group_name);
				}
			counted++;
			}
		current++; 
		}

	if(details > 0) {
		json_object_append_object(json_data, "contactgrouplist", 
				json_contactgrouplist_object);
		}
	else {
		json_object_append_array(json_data, "contactgrouplist", 
				json_contactgrouplist_array);
		}

	return json_data;
	}

json_object *json_object_contactgroup(unsigned format_options, 
		contactgroup *temp_contactgroup) {

	json_object *json_contactgroup = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_string(json_details, "group_name", &percent_escapes,
			temp_contactgroup->group_name);
	json_object_contactgroup_details(json_details, format_options, 
			temp_contactgroup);
	json_object_append_object(json_contactgroup, "contactgroup", json_details);

	return json_contactgroup;
}

void json_object_contactgroup_details(json_object *json_details, 
		unsigned format_options, contactgroup *temp_contactgroup) {

	json_array *json_members;
	contactsmember *temp_member;

	json_object_append_string(json_details, "group_name", &percent_escapes,
			temp_contactgroup->group_name);
	json_object_append_string(json_details, "alias", &percent_escapes,
			temp_contactgroup->alias);

	json_members = json_new_array();
	for(temp_member = temp_contactgroup->members; temp_member != NULL; 
			temp_member = temp_member->next) {
		json_array_append_string(json_members, &percent_escapes,
				temp_member->contact_name);
		}
	json_object_append_array(json_details, "members", json_members);
	}

json_object *json_object_timeperiod_selectors(int start, int count) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}

	return json_selectors;
	}

json_object *json_object_timeperiodcount(void) {

	json_object *json_data;
	timeperiod *temp_timeperiod;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_timeperiod_selectors(0, 0));

	for(temp_timeperiod = timeperiod_list; temp_timeperiod != NULL; 
			temp_timeperiod = temp_timeperiod->next) {

		count++; 
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_object_timeperiodlist(unsigned format_options, int start, 
		int count, int details) {

	json_object *json_data;
	json_object *json_timeperiodlist_object = NULL;
	json_array *json_timeperiodlist_array = NULL;
	json_object *json_timeperiod_details;
	timeperiod *temp_timeperiod;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_timeperiod_selectors(start, count));

	if(details > 0) {
		json_timeperiodlist_object = json_new_object();
		}
	else {
		json_timeperiodlist_array = json_new_array();
		}

	for(temp_timeperiod = timeperiod_list; temp_timeperiod != NULL; 
			temp_timeperiod = temp_timeperiod->next) {

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			if( details > 0) {
				json_timeperiod_details = json_new_object();
				json_object_timeperiod_details(json_timeperiod_details, 
						format_options, temp_timeperiod);
				json_object_append_object(json_timeperiodlist_object, 
						temp_timeperiod->name, json_timeperiod_details);
				}
			else {
				json_array_append_string(json_timeperiodlist_array, 
						&percent_escapes, temp_timeperiod->name);
				}
			counted++;
			}
		current++; 
		}

	if(details > 0) {
		json_object_append_object(json_data, "timeperiodlist", 
				json_timeperiodlist_object);
		}
	else {
		json_object_append_array(json_data, "timeperiodlist", 
				json_timeperiodlist_array);
		}

	return json_data;
	}

json_object *json_object_timeperiod(unsigned format_options, 
		timeperiod *temp_timeperiod) {

	json_object *json_timeperiod = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_string(json_details, "name", &percent_escapes,
			temp_timeperiod->name);
	json_object_timeperiod_details(json_details, format_options, temp_timeperiod);
	json_object_append_object(json_timeperiod, "timeperiod", json_details);

	return json_timeperiod;
}

void json_object_timeperiod_details(json_object *json_details, 
		unsigned format_options, timeperiod *temp_timeperiod) {

	json_object *json_days;
	int x;
	json_array *json_timeranges;
	json_array *json_exceptions;
	json_array *json_exclusions;
	daterange *temp_daterange;
	timeperiodexclusion *temp_timeperiodexclusion;

	json_object_append_string(json_details, "name", &percent_escapes,
			temp_timeperiod->name);
	json_object_append_string(json_details, "alias", &percent_escapes,
			temp_timeperiod->alias);

	json_days = json_new_object();
	for(x = 0; x < sizeof(temp_timeperiod->days) / sizeof(temp_timeperiod->days[0]); x++) {
		if( NULL != temp_timeperiod->days[x]) {
			json_timeranges = json_new_array();
			json_object_timerange(json_timeranges, format_options, 
					temp_timeperiod->days[x]);
			json_object_append_array(json_days, (char *)dayofweek[x], 
					json_timeranges);
			}
		}
	json_object_append_object(json_details, "days", json_days);

	json_exceptions = json_new_array();
	for(x = 0; x < DATERANGE_TYPES; x++) {
		for(temp_daterange = temp_timeperiod->exceptions[x]; 
				temp_daterange != NULL; temp_daterange = temp_daterange->next) {
			json_array_append_object(json_exceptions, 
					json_object_daterange(format_options, temp_daterange, x));
			}
		}
	json_object_append_array(json_details, "exceptions", json_exceptions);

	json_exclusions = json_new_array();
	for(temp_timeperiodexclusion = temp_timeperiod->exclusions; 
			temp_timeperiodexclusion != NULL; 
			temp_timeperiodexclusion = temp_timeperiodexclusion->next) {
		json_array_append_string(json_exclusions, &percent_escapes,
				temp_timeperiodexclusion->timeperiod_name);
		}
	json_object_append_array(json_details, "exclusions", json_exclusions);
	}

json_object * json_object_daterange(unsigned format_options, 
	daterange *temp_daterange, int daterange_type) {

	json_object *json_daterange;
	json_array *json_timeranges;

	json_daterange = json_new_object();

	switch(daterange_type) {
	case DATERANGE_CALENDAR_DATE:
		json_object_append_string(json_daterange, "type", NULL,
				"DATERANGE_CALENDAR_DATE");
		if(temp_daterange->syear == temp_daterange->eyear &&
				temp_daterange->smon == temp_daterange->emon &&
				temp_daterange->smday == temp_daterange->emday) {
			json_object_append_string(json_daterange, "date", NULL,
					"%4d-%02d-%02d", temp_daterange->syear,
					temp_daterange->smon+1, temp_daterange->smday);
			}
		else {
			json_object_append_string(json_daterange, "startdate", NULL,
					"%4d-%02d-%02d", temp_daterange->syear, 
					temp_daterange->smon+1, temp_daterange->smday);
			json_object_append_string(json_daterange, "enddate", NULL,
					"%4d-%02d-%02d", temp_daterange->eyear,
					temp_daterange->emon+1, temp_daterange->emday);
			}
		if( temp_daterange->skip_interval > 0) {
			json_object_append_integer(json_daterange, "skip_interval", 
					temp_daterange->skip_interval);
			}
		json_timeranges = json_new_array();
		json_object_timerange(json_timeranges, format_options, 
				temp_daterange->times);
		json_object_append_array(json_daterange, "times", json_timeranges);
		break;
	case DATERANGE_MONTH_DATE:
		json_object_append_string(json_daterange, "type", NULL,
				"DATERANGE_MONTH_DATE");
		if(temp_daterange->smon == temp_daterange->emon &&
				temp_daterange->smday == temp_daterange->emday) {
			json_object_append_string(json_daterange, "month", &percent_escapes,
					(char *)month[temp_daterange->smon]);
			json_object_append_integer(json_daterange, "day",
					temp_daterange->smday);
			}
		else {
			json_object_append_string(json_daterange, "startmonth",
					&percent_escapes, (char *)month[temp_daterange->smon]);
			json_object_append_integer(json_daterange, "startday",
					temp_daterange->smday);
			json_object_append_string(json_daterange, "endmonth",
					&percent_escapes, (char *)month[temp_daterange->emon]);
			json_object_append_integer(json_daterange, "endday",
					temp_daterange->emday);
			}
		if( temp_daterange->skip_interval > 0) {
			json_object_append_integer(json_daterange, "skip_interval", 
					temp_daterange->skip_interval);
			}
		json_timeranges = json_new_array();
		json_object_timerange(json_timeranges, format_options, 
					temp_daterange->times);
		json_object_append_array(json_daterange, "times", json_timeranges);
		break;
	case DATERANGE_MONTH_DAY:
		json_object_append_string(json_daterange, "type", NULL,
				"DATERANGE_MONTH_DAY");
		if(temp_daterange->smday == temp_daterange->emday) {
			json_object_append_integer(json_daterange, "day", 
					temp_daterange->smday);
			}
		else {
			json_object_append_integer(json_daterange, "startday", 
					temp_daterange->smday);
			json_object_append_integer(json_daterange, "endday", 
					temp_daterange->emday);
			}
		if( temp_daterange->skip_interval > 0) {
			json_object_append_integer(json_daterange, "skip_interval", 
					temp_daterange->skip_interval);
			}
		json_timeranges = json_new_array();
		json_object_timerange(json_timeranges, format_options, 
				temp_daterange->times);
		json_object_append_array(json_daterange, "times", json_timeranges);
		break;
	case DATERANGE_MONTH_WEEK_DAY:
		json_object_append_string(json_daterange, "type", NULL,
				"DATERANGE_MONTH_WEEK_DAY");
		if(temp_daterange->smon == temp_daterange->emon &&
				temp_daterange->swday == temp_daterange->ewday &&
				temp_daterange->swday_offset == temp_daterange->ewday_offset) {
			json_object_append_string(json_daterange, "month", &percent_escapes,
					(char *)month[temp_daterange->smon]);
			json_object_append_string(json_daterange, "weekday",
					&percent_escapes, (char *)dayofweek[temp_daterange->swday]);
			json_object_append_integer(json_daterange, "weekdayoffset",
					temp_daterange->swday_offset);
			}
		else {
			json_object_append_string(json_daterange, "startmonth",
					&percent_escapes, (char *)month[temp_daterange->smon]);
			json_object_append_string(json_daterange, "startweekday",
					&percent_escapes, (char *)dayofweek[temp_daterange->swday]);
			json_object_append_integer(json_daterange, "startweekdayoffset",
					temp_daterange->swday_offset);
			json_object_append_string(json_daterange, "endmonth",
					&percent_escapes, (char *)month[temp_daterange->emon]);
			json_object_append_string(json_daterange, "endweekday",
					&percent_escapes, (char *)dayofweek[temp_daterange->ewday]);
			json_object_append_integer(json_daterange, "endweekdayoffset",
					temp_daterange->ewday_offset);
			}
		if( temp_daterange->skip_interval > 0) {
			json_object_append_integer(json_daterange, "skip_interval", 
					temp_daterange->skip_interval);
			}
		json_timeranges = json_new_array();
		json_object_timerange(json_timeranges, format_options, 
				temp_daterange->times);
		json_object_append_array(json_daterange, "times", json_timeranges);
		break;
	case DATERANGE_WEEK_DAY:
		json_object_append_string(json_daterange, "type", NULL,
				"DATERANGE_WEEK_DAY");
		if(temp_daterange->swday == temp_daterange->ewday &&
				temp_daterange->swday_offset == temp_daterange->ewday_offset) {
			json_object_append_string(json_daterange, "weekday",
					&percent_escapes, (char *)dayofweek[temp_daterange->swday]);
			json_object_append_integer(json_daterange, "weekdayoffset",
					temp_daterange->swday_offset);
			}
		else {
			json_object_append_string(json_daterange, "startweekday",
					&percent_escapes, (char *)dayofweek[temp_daterange->swday]);
			json_object_append_integer(json_daterange, "startweekdayoffset", 
					temp_daterange->swday_offset);
			json_object_append_string(json_daterange, "endweekday",
					&percent_escapes, (char *)dayofweek[temp_daterange->ewday]);
			json_object_append_integer(json_daterange, "endweekdayoffset", 
					temp_daterange->ewday_offset);
			}
		if( temp_daterange->skip_interval > 0) {
			json_object_append_integer(json_daterange, "skip_interval", 
					temp_daterange->skip_interval);
			}
		json_timeranges = json_new_array();
		json_object_timerange(json_timeranges, format_options, 
				temp_daterange->times);
		json_object_append_array(json_daterange, "times", json_timeranges);
		break;
	default:
		json_object_append_string(json_daterange, "type", NULL,
				"Unknown daterange type: %u", daterange_type);
		break;
		}

	return json_daterange;
	}

void json_object_timerange(json_array *json_parent, unsigned format_options, 
		timerange *temp_timerange) {

	json_object *json_timerange;

	for(; temp_timerange != NULL; temp_timerange = temp_timerange->next) {
		json_timerange = json_new_object();
		json_object_append_time(json_timerange, "range_start", 
				temp_timerange->range_start);
		json_object_append_time(json_timerange, "range_end", 
				temp_timerange->range_end);
		json_array_append_object(json_parent, json_timerange);
		}
	}

json_object *json_object_command_selectors(int start, int count) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}

	return json_selectors;
	}

json_object *json_object_commandcount(void) {

	json_object *json_data;
	command *temp_command;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_command_selectors(0, 0));

	for(temp_command = command_list; temp_command != NULL; 
			temp_command = temp_command->next) {

		count++; 
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_object_commandlist(unsigned format_options, int start, 
		int count, int details) {

	json_object *json_data;
	json_object *json_commandlist_object = NULL;
	json_array *json_commandlist_array = NULL;
	json_object *json_command_details;
	command *temp_command;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_command_selectors(start, count));

	if(details > 0) {
		json_commandlist_object = json_new_object();
		}
	else {
		json_commandlist_array = json_new_array();
		}

	for(temp_command = command_list; temp_command != NULL; 
			temp_command = temp_command->next) {

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			if( details > 0) {
				json_command_details = json_new_object();
				json_object_command_details(json_command_details, format_options, 
						temp_command);
				json_object_append_object(json_commandlist_object, 
						temp_command->name, json_command_details);
				}
			else {
				json_array_append_string(json_commandlist_array,
						&percent_escapes, temp_command->name);
				}
			counted++;
			}
		current++; 
		}

	if(details > 0) {
		json_object_append_object(json_data, "commandlist", 
				json_commandlist_object);
		}
	else {
		json_object_append_array(json_data, "commandlist", 
				json_commandlist_array);
		}

	return json_data;
	}

json_object *json_object_command(unsigned format_options, command *temp_command) {

	json_object *json_command = json_new_object();
	json_object *json_details = json_new_object();

	json_object_append_string(json_details, "name", &percent_escapes,
			temp_command->name);
	json_object_command_details(json_details, format_options, temp_command);
	json_object_append_object(json_command, "command", json_details);

	return json_command;
}

void json_object_command_details(json_object *json_details, 
		unsigned format_options, command *temp_command) {
	json_object_append_string(json_details, "name", &percent_escapes,
			temp_command->name);
	json_object_append_string(json_details, "command_line", &percent_escapes,
			temp_command->command_line);
	}

int json_object_servicedependency_passes_selection(
		servicedependency *temp_servicedependency, host *master_host,
		hostgroup *master_hostgroup, char *master_service_description,
		servicegroup *master_servicegroup, host *dependent_host,
		hostgroup *dependent_hostgroup, char * dependent_service_description,
		servicegroup *dependent_servicegroup) {

	host *temp_host = NULL;
	service *temp_service = NULL;

	/* Skip if the servicedependency does not have the specified master host */
	if(NULL != master_host &&
			strcmp(temp_servicedependency->host_name, master_host->name)) {
		return 0;
		}

	/* Skip if the servicedependency does not have a master host in the
		specified hostgroup */
	if(NULL != master_hostgroup) {
		temp_host = find_host(temp_servicedependency->host_name);
		if((NULL != temp_host) && (FALSE ==
				is_host_member_of_hostgroup(master_hostgroup, temp_host))) {
			return 0;
			}
		}

	/* Skip if the servicedependency does not have the specified master
		service */
	if(NULL != master_service_description &&
			strcmp(temp_servicedependency->service_description,
			master_service_description)) {
		return 0;
		}

	/* Skip if the servicedependency does not have a master service in the
		specified servicegroup */
	if(NULL != master_servicegroup) {
		temp_service = find_service(temp_servicedependency->host_name,
				temp_servicedependency->service_description);
		if((NULL != temp_service) && (FALSE ==
				is_service_member_of_servicegroup(master_servicegroup,
				temp_service))) {
			return 0;
			}
		}

	/* Skip if the servicedependency does not have the specified dependent
		host */
	if(NULL != dependent_host &&
			strcmp(temp_servicedependency->dependent_host_name,
			dependent_host->name)) {
		return 0;
		}

	/* Skip if the servicedependency does not have a dependent host in the
		specified hostgroup */
	if(NULL != dependent_hostgroup) {
		temp_host = find_host(temp_servicedependency->dependent_host_name);
		if((NULL != temp_host) && (FALSE ==
				is_host_member_of_hostgroup(dependent_hostgroup, temp_host))) {
			return 0;
			}
		}

	/* Skip if the servicedependency does not have the specified dependent
		service */
	if(NULL != dependent_service_description &&
			strcmp(temp_servicedependency->dependent_service_description,
			dependent_service_description)) {
		return 0;
		}

	/* Skip if the servicedependency does not have a dependent service in the
		specified servicegroup */
	if(NULL != dependent_servicegroup) {
		temp_service = find_service(temp_servicedependency->dependent_host_name,
				temp_servicedependency->dependent_service_description);
		if((NULL != temp_service) && (FALSE ==
				is_service_member_of_servicegroup(dependent_servicegroup,
				temp_service))) {
			return 0;
			}
		}

	return 1;
	}

json_object *json_object_servicedependency_selectors(int start, int count,
		host *master_host, hostgroup *master_hostgroup,
		char *master_service_description, servicegroup *master_servicegroup,
		host *dependent_host, hostgroup *dependent_hostgroup,
		char * dependent_service_description,
		servicegroup *dependent_servicegroup) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if(NULL != master_host) {
		json_object_append_string(json_selectors, "masterhostname",
				&percent_escapes, master_host->name);
		}
	if(NULL != master_hostgroup) {
		json_object_append_string(json_selectors, "masterhostgroupname",
				&percent_escapes, master_hostgroup->group_name);
		}
	if(NULL != master_service_description) {
		json_object_append_string(json_selectors, "masterservicedescription",
				&percent_escapes, master_service_description);
		}
	if(NULL != master_servicegroup) {
		json_object_append_string(json_selectors, "masterservicegroupname",
				&percent_escapes, master_servicegroup->group_name);
		}
	if(NULL != dependent_host) {
		json_object_append_string(json_selectors, "dependenthostname",
				&percent_escapes, dependent_host->name);
		}
	if(NULL != dependent_hostgroup) {
		json_object_append_string(json_selectors, "dependenthostgroupname",
				&percent_escapes, dependent_hostgroup->group_name);
		}
	if(NULL != dependent_service_description) {
		json_object_append_string(json_selectors, "dependentservicedescription",
				&percent_escapes, dependent_service_description);
		}
	if(NULL != dependent_servicegroup) {
		json_object_append_string(json_selectors, "dependentservicegroupname",
				&percent_escapes, dependent_servicegroup->group_name);
		}

	return json_selectors;
	}

json_object *json_object_servicedependencycount(host *master_host,
		hostgroup *master_hostgroup, char *master_service_description,
		servicegroup *master_servicegroup, host *dependent_host,
		hostgroup *dependent_hostgroup, char * dependent_service_description,
		servicegroup *dependent_servicegroup) {

	json_object *json_data;
#ifdef JSON_NAGIOS_4X
	int x;
#endif
	servicedependency *temp_servicedependency;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_servicedependency_selectors(0, 0, master_host,
			master_hostgroup, master_service_description, master_servicegroup,
			dependent_host, dependent_hostgroup, dependent_service_description,
			dependent_servicegroup));

#ifdef JSON_NAGIOS_4X
	for(x = 0; x < num_objects.servicedependencies; x++) {
		temp_servicedependency = servicedependency_ary[ x];
#else
	for(temp_servicedependency = servicedependency_list; 
			temp_servicedependency != NULL; 
			temp_servicedependency = temp_servicedependency->next) {
#endif

		if(json_object_servicedependency_passes_selection(temp_servicedependency,
				master_host, master_hostgroup, master_service_description,
				master_servicegroup, dependent_host, dependent_hostgroup,
				dependent_service_description, dependent_servicegroup)) {
			count++; 
			}
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_object_servicedependencylist(unsigned format_options,
		int start, int count, host *master_host, hostgroup *master_hostgroup,
		char *master_service_description, servicegroup *master_servicegroup,
		host *dependent_host, hostgroup *dependent_hostgroup,
		char * dependent_service_description,
		servicegroup *dependent_servicegroup) {

	json_object *json_data;
	json_array *json_servicedependencylist;
	json_object *json_servicedependency_details;
#ifdef JSON_NAGIOS_4X
	int x;
#endif
	servicedependency *temp_servicedependency;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_servicedependency_selectors(start, count,
			master_host, master_hostgroup, master_service_description,
			master_servicegroup, dependent_host, dependent_hostgroup,
			dependent_service_description, dependent_servicegroup));

	json_servicedependencylist = json_new_array();

#ifdef JSON_NAGIOS_4X
	for(x = 0; x < num_objects.servicedependencies; x++) {
		temp_servicedependency = servicedependency_ary[ x];
#else
	for(temp_servicedependency = servicedependency_list; 
			temp_servicedependency != NULL; 
			temp_servicedependency = temp_servicedependency->next) {
#endif

		if(!json_object_servicedependency_passes_selection(temp_servicedependency,
				master_host, master_hostgroup, master_service_description,
				master_servicegroup, dependent_host, dependent_hostgroup,
				dependent_service_description, dependent_servicegroup)) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, 
				current, counted)) {
			json_servicedependency_details = json_new_object();
			json_object_servicedependency_details(json_servicedependency_details, 
					format_options, temp_servicedependency);
			json_array_append_object(json_servicedependencylist, 
					json_servicedependency_details);
			}
			counted++;
		current++; 
		}

	json_object_append_array(json_data, "servicedependencylist", 
			json_servicedependencylist);

	return json_data;
	}

void json_object_servicedependency_details(json_object *json_details, 
		unsigned format_options, servicedependency *temp_servicedependency) {

	json_object_append_integer(json_details, "dependency_type", 
			temp_servicedependency->dependency_type);
	json_object_append_string(json_details, "dependent_host_name", 
			&percent_escapes, temp_servicedependency->dependent_host_name);
	json_object_append_string(json_details, "dependent_service_description", 
			&percent_escapes,
			temp_servicedependency->dependent_service_description);
	json_object_append_string(json_details, "host_name", 
			&percent_escapes, temp_servicedependency->host_name);
	json_object_append_string(json_details, "service_description", 
			&percent_escapes, temp_servicedependency->service_description);
	json_object_append_string(json_details, "dependency_period", 
			&percent_escapes, temp_servicedependency->dependency_period);
	json_object_append_boolean(json_details, "inherits_parent", 
			temp_servicedependency->inherits_parent);

#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "fail_on_ok",
				flag_isset(temp_servicedependency->failure_options, OPT_OK));
		json_object_append_boolean(json_details, "fail_on_warning", 
				flag_isset(temp_servicedependency->failure_options, 
				OPT_WARNING));
		json_object_append_boolean(json_details, "fail_on_unknown", 
				flag_isset(temp_servicedependency->failure_options, 
				OPT_UNKNOWN));
		json_object_append_boolean(json_details, "fail_on_critical", 
				flag_isset(temp_servicedependency->failure_options, 
				OPT_CRITICAL));
		json_object_append_boolean(json_details, "fail_on_pending", 
				flag_isset(temp_servicedependency->failure_options, 
				OPT_PENDING));
		}
	else {
#endif
		json_bitmask(json_details, format_options, "failure_options",
				temp_servicedependency->failure_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "fail_on_ok",
			temp_servicedependency->fail_on_ok);
	json_object_append_boolean(json_details, "fail_on_warning", 
			temp_servicedependency->fail_on_warning);
	json_object_append_boolean(json_details, "fail_on_unknown", 
			temp_servicedependency->fail_on_unknown);
	json_object_append_boolean(json_details, "fail_on_critical", 
			temp_servicedependency->fail_on_critical);
	json_object_append_boolean(json_details, "fail_on_pending", 
			temp_servicedependency->fail_on_pending);
#endif
	}

int json_object_serviceescalation_passes_selection(serviceescalation *temp_serviceescalation, 
		host *match_host, char *service_description,
		hostgroup *match_hostgroup, servicegroup *match_servicegroup,
		contact *match_contact, contactgroup *match_contactgroup) {

	int found;
	hostsmember *temp_hostsmember;
	servicesmember *temp_servicesmember;

	/* Skip if the serviceescalation is not for the specified host */
	if( NULL != match_host && 
			strcmp( temp_serviceescalation->host_name, match_host->name)) {
		return 0;
		}

	if((NULL != service_description) &&
			strcmp(temp_serviceescalation->description, service_description)) {
		return 0;
		}

	if(NULL != match_hostgroup) {
		found = 0;
		for(temp_hostsmember = match_hostgroup->members;
				temp_hostsmember != NULL;
				temp_hostsmember = temp_hostsmember->next) {
			if(!strcmp(temp_hostsmember->host_name,
					temp_serviceescalation->host_name)) {
				found = 1;
				break;
				}
			}
			if(0 == found) {
				return 0;
				}
		}

	if(NULL != match_servicegroup) {
		found = 0;
		for(temp_servicesmember = match_servicegroup->members;
				temp_servicesmember != NULL;
				temp_servicesmember = temp_servicesmember->next) {
			if(!strcmp(temp_servicesmember->host_name,
					temp_serviceescalation->host_name) &&
					!strcmp(temp_servicesmember->service_description,
					temp_serviceescalation->description)) {
				found = 1;
				break;
				}
			}
			if(0 == found) {
				return 0;
				}
		}

	/* If a contact was specified, skip this service escalation if it does
		not have the contact specified */
	if( NULL != match_contact &&
			( FALSE == is_contact_for_service_escalation(temp_serviceescalation,
			match_contact))) {
		return 0;
		}

	/* If a contactgroup was specified, skip this service escalation if
		it does not have the contactgroup specified */
	if(NULL != match_contactgroup && (FALSE ==
			is_contactgroup_for_service_escalation(temp_serviceescalation,
			 match_contactgroup))) {
		return 0;
		}

	return 1;
	}

json_object *json_object_serviceescalation_selectors(int start, int count,
		host *match_host, char *service_description,
		hostgroup *match_hostgroup, servicegroup *match_servicegroup,
		contact *match_contact, contactgroup *match_contactgroup) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if(start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if(count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if(NULL != match_host) {
		json_object_append_string(json_selectors, "hostname", &percent_escapes,
				match_host->name);
		}
	if(NULL != service_description) {
		json_object_append_string(json_selectors, "servicedescription",
				&percent_escapes, service_description);
		}
	if(NULL != match_hostgroup) {
		json_object_append_string(json_selectors, "hostgroup",
				&percent_escapes, match_hostgroup->group_name);
		}
	if(NULL != match_servicegroup) {
		json_object_append_string(json_selectors, "servicegroup",
				&percent_escapes, match_servicegroup->group_name);
		}
	if( NULL != match_contact) {
		json_object_append_string(json_selectors, "contact",
				&percent_escapes, match_contact->name);
		}
	if( NULL != match_contactgroup) {
		json_object_append_string(json_selectors, "contactgroup",
				&percent_escapes, match_contactgroup->group_name);
		}

	return json_selectors;
	}

json_object *json_object_serviceescalationcount(host *match_host,
		char *service_description, hostgroup *match_hostgroup,
		servicegroup *match_servicegroup, contact *match_contact,
		contactgroup *match_contactgroup) {

	json_object *json_data;
#ifdef JSON_NAGIOS_4X
	int x;
#endif
	serviceescalation *temp_serviceescalation;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_serviceescalation_selectors(0, 0, match_host,
			service_description, match_hostgroup, match_servicegroup,
			match_contact, match_contactgroup));

#ifdef JSON_NAGIOS_4X
	for(x = 0; x < num_objects.serviceescalations; x++) {
		temp_serviceescalation=serviceescalation_ary[ x];
#else
	for(temp_serviceescalation = serviceescalation_list; 
			temp_serviceescalation != NULL; 
			temp_serviceescalation = temp_serviceescalation->next) {
#endif
		if(json_object_serviceescalation_passes_selection(temp_serviceescalation,
				match_host, service_description, match_hostgroup,
				match_servicegroup, match_contact, match_contactgroup) == 0) {
			continue;
			}


		count++; 
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_object_serviceescalationlist(unsigned format_options, 
		int start, int count, host *match_host, char *service_description,
		hostgroup *match_hostgroup, servicegroup *match_servicegroup,
		contact *match_contact, contactgroup *match_contactgroup) {

	json_object *json_data;
	json_array *json_serviceescalationlist;
	json_object *json_serviceescalation_details;
#ifdef JSON_NAGIOS_4X
	int x;
#endif
	serviceescalation *temp_serviceescalation;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_serviceescalation_selectors(start, count, match_host,
			service_description, match_hostgroup, match_servicegroup,
			match_contact, match_contactgroup));

	json_serviceescalationlist = json_new_array();

#ifdef JSON_NAGIOS_4X
	for(x = 0; x < num_objects.serviceescalations; x++) {
		temp_serviceescalation=serviceescalation_ary[ x];
#else
	for(temp_serviceescalation = serviceescalation_list; 
			temp_serviceescalation != NULL; 
			temp_serviceescalation = temp_serviceescalation->next) {
#endif
		if(json_object_serviceescalation_passes_selection(temp_serviceescalation,
				match_host, service_description, match_hostgroup,
				match_servicegroup, match_contact, match_contactgroup) == 0) {
			continue;
			}


		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			json_serviceescalation_details = json_new_object();
			json_object_serviceescalation_details(json_serviceescalation_details, 
					format_options, temp_serviceescalation);
			json_array_append_object(json_serviceescalationlist, 
					json_serviceescalation_details);
			}
			counted++;
		current++; 
		}

	json_object_append_array(json_data, "serviceescalationlist", 
			json_serviceescalationlist);

	return json_data;
	}

void json_object_serviceescalation_details(json_object *json_details, 
		unsigned format_options, serviceescalation *temp_serviceescalation) {

	json_array *json_contactgroups;
	json_array *json_contacts;
	contactgroupsmember *temp_contact_groupsmember;
	contactsmember *temp_contactsmember;

	json_object_append_string(json_details, "host_name", &percent_escapes,
			temp_serviceescalation->host_name);
	json_object_append_string(json_details, "description", &percent_escapes,
			temp_serviceescalation->description);
	json_object_append_integer(json_details, "first_notification", 
			temp_serviceescalation->first_notification);
	json_object_append_integer(json_details, "last_notification", 
			temp_serviceescalation->last_notification);
	json_object_append_real(json_details, "notification_interval", 
			temp_serviceescalation->notification_interval);
	json_object_append_string(json_details, "escalation_period",
			&percent_escapes, temp_serviceescalation->escalation_period);

#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "escalate_on_recovery", 
				flag_isset(temp_serviceescalation->escalation_options, 
				OPT_RECOVERY));
		json_object_append_boolean(json_details, "escalate_on_warning", 
				flag_isset(temp_serviceescalation->escalation_options, 
				OPT_WARNING));
		json_object_append_boolean(json_details, "escalate_on_unknown", 
				flag_isset(temp_serviceescalation->escalation_options, 
				OPT_UNKNOWN)));
		json_object_append_boolean(json_details, "escalate_on_critical", 
				flag_isset(temp_serviceescalation->escalation_options, 
				OPT_CRITICAL));
		}
	else {
#endif
		json_bitmask(json_details, format_options, "escalation_options",
				temp_serviceescalation->escalation_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "escalate_on_recovery", 
			temp_serviceescalation->escalate_on_recovery);
	json_object_append_boolean(json_details, "escalate_on_warning", 
			temp_serviceescalation->escalate_on_warning);
	json_object_append_boolean(json_details, "escalate_on_unknown", 
			temp_serviceescalation->escalate_on_unknown);
	json_object_append_boolean(json_details, "escalate_on_critical", 
			temp_serviceescalation->escalate_on_critical);
#endif

	json_contactgroups = json_new_object();
	for(temp_contact_groupsmember = temp_serviceescalation->contact_groups; 
			temp_contact_groupsmember != NULL; 
			temp_contact_groupsmember = temp_contact_groupsmember->next) {
		json_array_append_string(json_contactgroups, &percent_escapes,
				temp_contact_groupsmember->group_name);
		}
	json_object_append_array(json_details, "contact_groups", json_contactgroups);

	json_contacts = json_new_object();
	for(temp_contactsmember = temp_serviceescalation->contacts; 
			temp_contactsmember != NULL; 
			temp_contactsmember = temp_contactsmember->next) {
		json_array_append_string(json_contacts, &percent_escapes,
				temp_contactsmember->contact_name);
		}
	json_object_append_array(json_details, "contacts", json_contacts);
	}

int json_object_hostdependency_passes_selection(
		hostdependency *temp_hostdependency, host *master_host,
		hostgroup *master_hostgroup, host *dependent_host,
		hostgroup *dependent_hostgroup) {

	host *temp_host = NULL;

	/* Skip if the hostdependency does not have the specified master host */
	if(NULL != master_host &&
			strcmp(temp_hostdependency->host_name, master_host->name)) {
		return 0;
		}

	/* Skip if the hostdependency does not have a master host in the
		specified hostgroup*/
	if(NULL != master_hostgroup) {
		temp_host = find_host(temp_hostdependency->host_name);
		if((NULL != temp_host) && (FALSE ==
				is_host_member_of_hostgroup(master_hostgroup, temp_host))) {
			return 0;
			}
		}

	/* Skip if the hostdependency does not have the specified dependent host */
	if(NULL != dependent_host &&
			strcmp(temp_hostdependency->dependent_host_name,
			dependent_host->name)) {
		return 0;
		}

	/* Skip if the hostdependency does not have a dependent host in the
		specified hostgroup*/
	if(NULL != dependent_hostgroup) {
		temp_host = find_host(temp_hostdependency->dependent_host_name);
		if((NULL != temp_host) && (FALSE ==
				is_host_member_of_hostgroup(dependent_hostgroup, temp_host))) {
			return 0;
			}
		}

	return 1;
	}

json_object *json_object_hostdependency_selectors(int start, int count,
		host *master_host, hostgroup *master_hostgroup, host *dependent_host,
		hostgroup *dependent_hostgroup) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if(NULL != master_host) {
		json_object_append_string(json_selectors, "masterhostname",
				&percent_escapes, master_host->name);
		}
	if(NULL != master_hostgroup) {
		json_object_append_string(json_selectors, "masterhostgroupname",
				&percent_escapes, master_hostgroup->group_name);
		}
	if(NULL != dependent_host) {
		json_object_append_string(json_selectors, "dependenthostname",
				&percent_escapes, dependent_host->name);
		}
	if(NULL != dependent_hostgroup) {
		json_object_append_string(json_selectors, "dependenthostgroupname",
				&percent_escapes, dependent_hostgroup->group_name);
		}

	return json_selectors;
	}

json_object *json_object_hostdependencycount(host *master_host,
		hostgroup *master_hostgroup, host *dependent_host,
		hostgroup *dependent_hostgroup) {

	json_object *json_data;
#ifdef JSON_NAGIOS_4X
	int x;
#endif
	hostdependency *temp_hostdependency;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_hostdependency_selectors(0, 0, master_host,
			master_hostgroup, dependent_host, dependent_hostgroup));

#ifdef JSON_NAGIOS_4X
	for(x = 0; x < num_objects.hostdependencies; x++) {
		temp_hostdependency = hostdependency_ary[ x];
#else
	for(temp_hostdependency = hostdependency_list; 
			temp_hostdependency != NULL; 
			temp_hostdependency = temp_hostdependency->next) {
#endif

		if(json_object_hostdependency_passes_selection(
				temp_hostdependency, master_host, master_hostgroup,
				dependent_host, dependent_hostgroup)) {
			count++;
			}
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_object_hostdependencylist(unsigned format_options, int start,
		int count, host *master_host, hostgroup *master_hostgroup,
		host *dependent_host, hostgroup *dependent_hostgroup) {

	json_object *json_data;
	json_array *json_hostdependencylist;
#ifdef JSON_NAGIOS_4X
	int x;
#endif
	json_object *json_hostdependency_details;
	hostdependency *temp_hostdependency;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_hostdependency_selectors(start, count,
			master_host, master_hostgroup, dependent_host,
			dependent_hostgroup));

	json_hostdependencylist = json_new_array();

#ifdef JSON_NAGIOS_4X
	for(x = 0; x < num_objects.hostdependencies; x++) {
		temp_hostdependency = hostdependency_ary[ x];
#else
	for(temp_hostdependency = hostdependency_list; 
			temp_hostdependency != NULL; 
			temp_hostdependency = temp_hostdependency->next) {
#endif

		if(!json_object_hostdependency_passes_selection(
				temp_hostdependency, master_host, master_hostgroup,
				dependent_host, dependent_hostgroup)) {
			continue;
			}

		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			json_hostdependency_details = json_new_object();
			json_object_hostdependency_details(json_hostdependency_details, 
					format_options, temp_hostdependency);
			json_array_append_object(json_hostdependencylist, 
					json_hostdependency_details);	
			}
			counted++;
		current++; 
		}

	json_object_append_array(json_data, "hostdependencylist", 
			json_hostdependencylist);

	return json_data;
	}

void json_object_hostdependency_details(json_object *json_details,
		unsigned format_options, hostdependency *temp_hostdependency) {

	json_object_append_integer(json_details, "dependency_type", 
			temp_hostdependency->dependency_type);
	json_object_append_string(json_details, "dependent_host_name", 
			&percent_escapes, temp_hostdependency->dependent_host_name);
	json_object_append_string(json_details, "host_name", 
			&percent_escapes, temp_hostdependency->host_name);
	json_object_append_string(json_details, "dependency_period", 
			&percent_escapes, temp_hostdependency->dependency_period);
	json_object_append_boolean(json_details, "inherits_parent", 
			temp_hostdependency->inherits_parent);

#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "fail_on_up", 
				flag_isset(temp_hostdependency->failure_options, OPT_UP));
		json_object_append_boolean(json_details, "fail_on_down", 
				flag_isset(temp_hostdependency->failure_options, OPT_DOWN));
		json_object_append_boolean(json_details, "fail_on_unreachable", 
				flag_isset(temp_hostdependency->failure_options, 
				OPT_UNREACHABLE));
		json_object_append_boolean(json_details, "fail_on_pending", 
				flag_isset(temp_hostdependency->failure_options, OPT_PENDING));
		}
	else {
#endif
		json_bitmask(json_details, format_options, "failure_options",
				temp_hostdependency->failure_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "fail_on_up", 
			temp_hostdependency->fail_on_up);
	json_object_append_boolean(json_details, "fail_on_down", 
			temp_hostdependency->fail_on_down);
	json_object_append_boolean(json_details, "fail_on_unreachable", 
			temp_hostdependency->fail_on_unreachable);
	json_object_append_boolean(json_details, "fail_on_pending", 
			temp_hostdependency->fail_on_pending);
#endif
	}

int json_object_hostescalation_passes_selection(hostescalation *temp_hostescalation, 
		host *match_host, hostgroup *match_hostgroup, contact *match_contact,
		contactgroup *match_contactgroup) {

	int found;
	hostsmember *temp_hostsmember;

	/* Skip if the hostescalation is not for the specified host */
	if( NULL != match_host && 
			strcmp( temp_hostescalation->host_name, match_host->name)) {
		return 0;
		}

	if(NULL != match_hostgroup) {
		found = 0;
		for(temp_hostsmember = match_hostgroup->members;
				temp_hostsmember != NULL;
				temp_hostsmember = temp_hostsmember->next) {
			if(!strcmp(temp_hostsmember->host_name,
					temp_hostescalation->host_name)) {
				found = 1;
				break;
				}
			}
			if(0 == found) {
				return 0;
				}
		}

	/* If a contact was specified, skip this host escalation if it does
		not have the contact specified */
	if( NULL != match_contact &&
			( FALSE == is_contact_for_host_escalation(temp_hostescalation,
			match_contact))) {
		return 0;
		}

	/* If a contactgroup was specified, skip this host escalation if
		it does not have the contactgroup specified */
	if(NULL != match_contactgroup && (FALSE ==
			is_contactgroup_for_host_escalation(temp_hostescalation,
			 match_contactgroup))) {
		return 0;
		}

	return 1;
	}

json_object *json_object_hostescalation_selectors(int start, int count, 
		host *match_host, hostgroup *match_hostgroup, contact *match_contact,
		contactgroup *match_contactgroup) {

	json_object *json_selectors;

	json_selectors = json_new_object();

	if( start > 0) {
		json_object_append_integer(json_selectors, "start", start);
		}
	if( count > 0) {
		json_object_append_integer(json_selectors, "count", count);
		}
	if( NULL != match_host) {
		json_object_append_string(json_selectors, "hostname", &percent_escapes,
				match_host->name);
		}
	if(NULL != match_hostgroup) {
		json_object_append_string(json_selectors, "hostgroup",
				&percent_escapes, match_hostgroup->group_name);
		}
	if( NULL != match_contact) {
		json_object_append_string(json_selectors, "contact",
				&percent_escapes, match_contact->name);
		}
	if( NULL != match_contactgroup) {
		json_object_append_string(json_selectors, "contactgroup",
				&percent_escapes, match_contactgroup->group_name);
		}

	return json_selectors;
	}

json_object *json_object_hostescalationcount(host *match_host,
		hostgroup *match_hostgroup, contact *match_contact,
		contactgroup *match_contactgroup) {

	json_object *json_data;
#ifdef JSON_NAGIOS_4X
	int x;
#endif
	hostescalation *temp_hostescalation;
	int count = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_hostescalation_selectors(0, 0, match_host,
			match_hostgroup, match_contact, match_contactgroup));

#ifdef JSON_NAGIOS_4X
	for(x = 0; x < num_objects.hostescalations; x++) {
		temp_hostescalation = hostescalation_ary[ x];
#else
	for(temp_hostescalation = hostescalation_list; 
			temp_hostescalation != NULL; 
			temp_hostescalation = temp_hostescalation->next) {
#endif
		if(json_object_hostescalation_passes_selection(temp_hostescalation,
				match_host, match_hostgroup, match_contact,
				match_contactgroup) == 0) {
			continue;
			}

		count++; 
		}

	json_object_append_integer(json_data, "count", count);

	return json_data;
	}

json_object *json_object_hostescalationlist(unsigned format_options, int start, 
		int count, host *match_host, hostgroup *match_hostgroup,
		contact *match_contact, contactgroup *match_contactgroup) {

	json_object *json_data;
	json_array *json_hostescalationlist;
	json_object *json_hostescalation_details;
#ifdef JSON_NAGIOS_4X
	int x;
#endif
	hostescalation *temp_hostescalation;
	int current = 0;
	int counted = 0;

	json_data = json_new_object();
	json_object_append_object(json_data, "selectors", 
			json_object_hostescalation_selectors(start, count, match_host,
			match_hostgroup, match_contact, match_contactgroup));

	json_hostescalationlist = json_new_array();

#ifdef JSON_NAGIOS_4X
	for(x = 0; x < num_objects.hostescalations; x++) {
		temp_hostescalation = hostescalation_ary[ x];
#else
	for(temp_hostescalation = hostescalation_list; 
			temp_hostescalation != NULL; 
			temp_hostescalation = temp_hostescalation->next) {
#endif
		if(json_object_hostescalation_passes_selection(temp_hostescalation,
				match_host, match_hostgroup, match_contact,
				match_contactgroup) == 0) {
			continue;
			}


		/* If the current item passes the start and limit tests, display it */
		if( passes_start_and_count_limits(start, count, current, counted)) {
			json_hostescalation_details = json_new_object();
			json_object_hostescalation_details(json_hostescalation_details, 
					format_options, temp_hostescalation);
			json_array_append_object(json_hostescalationlist, 
					json_hostescalation_details);
			}
			counted++;
		current++; 
		}

	json_object_append_array(json_data, "hostescalationlist", 
			json_hostescalationlist);

	return json_data;
	}

void json_object_hostescalation_details(json_object *json_details,
		unsigned format_options, hostescalation *temp_hostescalation) {

	json_array *json_contactgroups;
	json_array *json_contacts;
	contactgroupsmember *temp_contact_groupsmember;
	contactsmember *temp_contactsmember;

	json_object_append_string(json_details, "host_name", &percent_escapes,
			temp_hostescalation->host_name);
	json_object_append_integer(json_details, "first_notification", 
			temp_hostescalation->first_notification);
	json_object_append_integer(json_details, "last_notification", 
			temp_hostescalation->last_notification);
	json_object_append_real(json_details, "notification_interval", 
			temp_hostescalation->notification_interval);
	json_object_append_string(json_details, "escalation_period",
			&percent_escapes, temp_hostescalation->escalation_period);

#ifdef JSON_NAGIOS_4X
#if 0
	if( CORE3_COMPATIBLE) {
		json_object_append_boolean(json_details, "escalate_on_recovery", 
				flag_isset(temp_hostescalation->escalation_options, 
				OPT_RECOVERY));
		json_object_append_boolean(json_details, "escalate_on_down", 
				flag_isset(temp_hostescalation->escalation_options, OPT_DOWN));
		json_object_append_boolean(json_details, "escalate_on_unreachable", 
				flag_isset(temp_hostescalation->escalation_options, 
				OPT_UNREACHABLE));
		}
	else {
#endif
		json_bitmask(json_details, format_options, "escalation_options",
				temp_hostescalation->escalation_options, svm_option_types);
#if 0
		}
#endif
#else
	json_object_append_boolean(json_details, "escalate_on_recovery", 
			temp_hostescalation->escalate_on_recovery);
	json_object_append_boolean(json_details, "escalate_on_down", 
			temp_hostescalation->escalate_on_down);
	json_object_append_boolean(json_details, "escalate_on_unreachable", 
			temp_hostescalation->escalate_on_unreachable);
#endif

	json_contactgroups = json_new_array();
	for(temp_contact_groupsmember = temp_hostescalation->contact_groups; 
			temp_contact_groupsmember != NULL; 
			temp_contact_groupsmember = temp_contact_groupsmember->next) {
		json_array_append_string(json_contactgroups, &percent_escapes,
				temp_contact_groupsmember->group_name);
		}
	json_object_append_array(json_details, "contact_groups", json_contactgroups);

	json_contacts = json_new_array();
	for(temp_contactsmember = temp_hostescalation->contacts; 
			temp_contactsmember != NULL; 
			temp_contactsmember = temp_contactsmember->next) {
		json_array_append_string(json_contacts, &percent_escapes,
				temp_contactsmember->contact_name);
		}
	json_object_append_array(json_details, "contacts", json_contacts);
	}
