/**************************************************************************
 *
 * STATUSWML.C -  Nagios Status CGI for WAP-enabled devices
 *
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

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char *status_file;

extern hoststatus *hoststatus_list;
extern servicestatus *servicestatus_list;

extern int	use_ssl_authentication;
extern int      nagios_process_state;

extern char     *ping_syntax;

#define DISPLAY_HOST		        0
#define DISPLAY_SERVICE                 1
#define DISPLAY_HOSTGROUP               2
#define DISPLAY_INDEX                   3
#define DISPLAY_PING                    4
#define DISPLAY_TRACEROUTE              5
#define DISPLAY_QUICKSTATS              6
#define DISPLAY_PROCESS                 7
#define DISPLAY_ALL_PROBLEMS            8
#define DISPLAY_UNHANDLED_PROBLEMS      9

#define DISPLAY_HOSTGROUP_SUMMARY       0
#define DISPLAY_HOSTGROUP_OVERVIEW      1

#define DISPLAY_HOST_SUMMARY            0
#define DISPLAY_HOST_SERVICES           1

void document_header(void);
void document_footer(void);
int process_cgivars(void);
int validate_arguments(void);
int is_valid_hostip(char *hostip);

int display_type = DISPLAY_INDEX;
int hostgroup_style = DISPLAY_HOSTGROUP_SUMMARY;
int host_style = DISPLAY_HOST_SUMMARY;

void display_index(void);
void display_host(void);
void display_host_services(void);
void display_service(void);
void display_hostgroup_summary(void);
void display_hostgroup_overview(void);
void display_ping(void);
void display_traceroute(void);
void display_quick_stats(void);
void display_process(void);
void display_problems(void);

char *host_name = "";
char *hostgroup_name = "";
char *service_desc = "";
char *ping_address = "";
char *traceroute_address = "";

int show_all_hostgroups = TRUE;


authdata current_authdata;



int main(void) {
	int result = OK;

	/* get the arguments passed in the URL */
	process_cgivars();

	/* reset internal variables */
	reset_cgi_vars();

	/* Initialize shared configuration variables */                             
	init_shared_cfg_vars(1);

	document_header();

	/* validate arguments in URL */
	result = validate_arguments();
	if(result == ERROR) {
		document_footer();
		return ERROR;
		}

	/* read the CGI configuration file */
	result = read_cgi_config_file(get_cgi_config_location());
	if(result == ERROR) {
		printf("<P>Error: Could not open CGI configuration file '%s' for reading!</P>\n", get_cgi_config_location());
		document_footer();
		return ERROR;
		}

	/* read the main configuration file */
	result = read_main_config_file(main_config_file);
	if(result == ERROR) {
		printf("<P>Error: Could not open main configuration file '%s' for reading!</P>\n", main_config_file);
		document_footer();
		return ERROR;
		}

	/* read all object configuration data */
	result = read_all_object_configuration_data(main_config_file, READ_ALL_OBJECT_DATA);
	if(result == ERROR) {
		printf("<P>Error: Could not read some or all object configuration data!</P>\n");
		document_footer();
		return ERROR;
		}

	/* read all status data */
	result = read_all_status_data(status_file, READ_ALL_STATUS_DATA);
	if(result == ERROR) {
		printf("<P>Error: Could not read host and service status information!</P>\n");
		document_footer();
		free_memory();
		return ERROR;
		}

	/* get authentication information */
	get_authentication_information(&current_authdata);

	/* decide what to display to the user */
	if(display_type == DISPLAY_HOST && host_style == DISPLAY_HOST_SERVICES)
		display_host_services();
	else if(display_type == DISPLAY_HOST)
		display_host();
	else if(display_type == DISPLAY_SERVICE)
		display_service();
	else if(display_type == DISPLAY_HOSTGROUP && hostgroup_style == DISPLAY_HOSTGROUP_OVERVIEW)
		display_hostgroup_overview();
	else if(display_type == DISPLAY_HOSTGROUP && hostgroup_style == DISPLAY_HOSTGROUP_SUMMARY)
		display_hostgroup_summary();
	else if(display_type == DISPLAY_PING)
		display_ping();
	else if(display_type == DISPLAY_TRACEROUTE)
		display_traceroute();
	else if(display_type == DISPLAY_QUICKSTATS)
		display_quick_stats();
	else if(display_type == DISPLAY_PROCESS)
		display_process();
	else if(display_type == DISPLAY_ALL_PROBLEMS || display_type == DISPLAY_UNHANDLED_PROBLEMS)
		display_problems();
	else
		display_index();

	document_footer();

	/* free all allocated memory */
	free_memory();

	return OK;
	}


void document_header(void) {
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

	printf("Content-type: text/vnd.wap.wml\r\n\r\n");

	printf("<?xml version=\"1.0\"?>\n");
	printf("<!DOCTYPE wml PUBLIC \"-//WAPFORUM//DTD WML 1.1//EN\" \"http://www.wapforum.org/DTD/wml_1.1.xml\">\n");

	printf("<wml>\n");

	printf("<head>\n");
	printf("<meta forua=\"true\" http-equiv=\"Cache-Control\" content=\"max-age=0\"/>\n");
	printf("</head>\n");

	return;
	}


void document_footer(void) {

	printf("</wml>\n");

	return;
	}


int process_cgivars(void) {
	char **variables;
	int error = FALSE;
	int x;

	variables = getcgivars();

	for(x = 0; variables[x] != NULL; x++) {

		/* do some basic length checking on the variable identifier to prevent buffer overflows */
		if(strlen(variables[x]) >= MAX_INPUT_BUFFER - 1) {
			continue;
			}

		/* we found the hostgroup argument */
		else if(!strcmp(variables[x], "hostgroup")) {
			display_type = DISPLAY_HOSTGROUP;
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((hostgroup_name = (char *)strdup(variables[x])) == NULL)
				hostgroup_name = "";
			strip_html_brackets(hostgroup_name);

			if(!strcmp(hostgroup_name, "all"))
				show_all_hostgroups = TRUE;
			else
				show_all_hostgroups = FALSE;
			}

		/* we found the host argument */
		else if(!strcmp(variables[x], "host")) {
			display_type = DISPLAY_HOST;
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((host_name = (char *)strdup(variables[x])) == NULL)
				host_name = "";
			strip_html_brackets(host_name);
			}

		/* we found the service argument */
		else if(!strcmp(variables[x], "service")) {
			display_type = DISPLAY_SERVICE;
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((service_desc = (char *)strdup(variables[x])) == NULL)
				service_desc = "";
			strip_html_brackets(service_desc);
			}


		/* we found the hostgroup style argument */
		else if(!strcmp(variables[x], "style")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "overview"))
				hostgroup_style = DISPLAY_HOSTGROUP_OVERVIEW;
			else if(!strcmp(variables[x], "summary"))
				hostgroup_style = DISPLAY_HOSTGROUP_SUMMARY;
			else if(!strcmp(variables[x], "servicedetail"))
				host_style = DISPLAY_HOST_SERVICES;
			else if(!strcmp(variables[x], "processinfo"))
				display_type = DISPLAY_PROCESS;
			else if(!strcmp(variables[x], "aprobs"))
				display_type = DISPLAY_ALL_PROBLEMS;
			else if(!strcmp(variables[x], "uprobs"))
				display_type = DISPLAY_UNHANDLED_PROBLEMS;
			else
				display_type = DISPLAY_QUICKSTATS;
			}

		/* we found the ping argument */
		else if(!strcmp(variables[x], "ping")) {
			display_type = DISPLAY_PING;
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((ping_address = (char *)strdup(variables[x])) == NULL)
				ping_address = "";
			strip_html_brackets(ping_address);
			}

		/* we found the traceroute argument */
		else if(!strcmp(variables[x], "traceroute")) {
			display_type = DISPLAY_TRACEROUTE;
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((traceroute_address = (char *)strdup(variables[x])) == NULL)
				traceroute_address = "";
			strip_html_brackets(traceroute_address);
			}

		}

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return error;
	}

int validate_arguments(void) {
	int result = OK;
	if((strcmp(ping_address, "")) && !is_valid_hostip(ping_address)) {
		printf("<p>Invalid host name/ip</p>\n");
		result = ERROR;
		}
	if(strcmp(traceroute_address, "") && !is_valid_hostip(traceroute_address)) {
		printf("<p>Invalid host name/ip</p>\n");
		result = ERROR;
		}
	return result;
	}

int is_valid_hostip(char *hostip) {
	char *valid_domain_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-";
	if(strcmp(hostip, "") && strlen(hostip) == strspn(hostip, valid_domain_chars) && hostip[0] != '-' && hostip[strlen(hostip) - 1] != '-')
		return TRUE;
	return FALSE;
	}

/* main intro screen */
void display_index(void) {


	/**** MAIN MENU SCREEN (CARD 1) ****/
	printf("<card id='card1' title='Nagios WAP Interface'>\n");
	printf("<p align='center' mode='nowrap'>\n");

	printf("<b>Nagios</b><br/><b>WAP Interface</b><br/>\n");

	printf("<b><anchor title='Quick Stats'>Quick Stats<go href='%s'><postfield name='style' value='quickstats'/></go></anchor></b><br/>\n", STATUSWML_CGI);

	printf("<b><anchor title='Status Summary'>Status Summary<go href='%s'><postfield name='hostgroup' value='all'/><postfield name='style' value='summary'/></go></anchor></b><br/>\n", STATUSWML_CGI);

	printf("<b><anchor title='Status Overview'>Status Overview<go href='%s'><postfield name='hostgroup' value='all'/><postfield name='style' value='overview'/></go></anchor></b><br/>\n", STATUSWML_CGI);

	printf("<b><anchor title='All Problems'>All Problems<go href='%s'><postfield name='style' value='aprobs'/></go></anchor></b><br/>\n", STATUSWML_CGI);

	printf("<b><anchor title='Unhandled Problems'>Unhandled Problems<go href='%s'><postfield name='style' value='uprobs'/></go></anchor></b><br/>\n", STATUSWML_CGI);

	printf("<b><anchor title='Process Info'>Process Info<go href='%s'><postfield name='style' value='processinfo'/></go></anchor></b><br/>\n", STATUSWML_CGI);

	printf("<b><anchor title='Network Tools'>Tools<go href='#card2'/></anchor></b><br/>\n");

	printf("<b><anchor title='About'>About<go href='#card3'/></anchor></b><br/>\n");

	printf("</p>\n");
	printf("</card>\n");


	/**** TOOLS SCREEN (CARD 2) ****/
	printf("<card id='card2' title='Network Tools'>\n");
	printf("<p align='center' mode='nowrap'>\n");

	printf("<b>Network Tools:</b><br/>\n");

	printf("<b><anchor title='Ping Host'>Ping<go href='%s'><postfield name='ping' value=''/></go></anchor></b><br/>\n", STATUSWML_CGI);
	printf("<b><anchor title='Traceroute'>Traceroute<go href='%s'><postfield name='traceroute' value=''/></go></anchor></b><br/>\n", STATUSWML_CGI);
	printf("<b><anchor title='View Host'>View Host<go href='#card4'/></anchor></b><br/>\n");
	printf("<b><anchor title='View Hostgroup'>View Hostgroup<go href='#card5'/></anchor></b><br/>\n");

	printf("</p>\n");
	printf("</card>\n");


	/**** ABOUT SCREEN (CARD 3) ****/
	printf("<card id='card3' title='About'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>About</b><br/>\n");
	printf("</p>\n");

	printf("<p align='center' mode='wrap'>\n");
	printf("<b>Nagios %s</b><br/><b>WAP Interface</b><br/>\n", PROGRAM_VERSION);
	printf("Copyright (C) 2001 Ethan Galstad<br/>\n");
	printf("egalstad@nagios.org<br/><br/>\n");
	printf("License: <b>GPL</b><br/><br/>\n");
	printf("Based in part on features found in AskAround's Wireless Network Tools<br/>\n");
	printf("<b>www.askaround.com</b><br/>\n");
	printf("</p>\n");

	printf("</card>\n");



	/**** VIEW HOST SCREEN (CARD 4) ****/
	printf("<card id='card4' title='View Host'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>View Host</b><br/>\n");
	printf("</p>\n");

	printf("<p align='center' mode='wrap'>\n");
	printf("<b>Host Name:</b><br/>\n");
	printf("<input name='hname'/>\n");
	printf("<do type='accept'>\n");
	printf("<go href='%s' method='post'><postfield name='host' value='$(hname)'/></go>\n", STATUSWML_CGI);
	printf("</do>\n");
	printf("</p>\n");

	printf("</card>\n");



	/**** VIEW HOSTGROUP SCREEN (CARD 5) ****/
	printf("<card id='card5' title='View Hostgroup'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>View Hostgroup</b><br/>\n");
	printf("</p>\n");

	printf("<p align='center' mode='wrap'>\n");
	printf("<b>Hostgroup Name:</b><br/>\n");
	printf("<input name='gname'/>\n");
	printf("<do type='accept'>\n");
	printf("<go href='%s' method='post'><postfield name='hostgroup' value='$(gname)'/><postfield name='style' value='overview'/></go>\n", STATUSWML_CGI);
	printf("</do>\n");
	printf("</p>\n");

	printf("</card>\n");


	return;
	}


/* displays process info */
void display_process(void) {


	/**** MAIN SCREEN (CARD 1) ****/
	printf("<card id='card1' title='Process Info'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>Process Info</b><br/><br/>\n");

	/* check authorization */
	if(is_authorized_for_system_information(&current_authdata) == FALSE) {

		printf("<b>Error: Not authorized for process info!</b>\n");
		printf("</p>\n");
		printf("</card>\n");
		return;
		}

	if(nagios_process_state == STATE_OK)
		printf("Nagios process is running<br/>\n");
	else
		printf("<b>Nagios process may not be running</b><br/>\n");

	if(enable_notifications == TRUE)
		printf("Notifications are enabled<br/>\n");
	else
		printf("<b>Notifications are disabled</b><br/>\n");

	if(execute_service_checks == TRUE)
		printf("Check execution is enabled<br/>\n");
	else
		printf("<b>Check execution is disabled</b><br/>\n");

	printf("<br/>\n");
	printf("<b><anchor title='Process Commands'>Process Commands<go href='#card2'/></anchor></b>\n");
	printf("</p>\n");

	printf("</card>\n");


	/**** COMMANDS SCREEN (CARD 2) ****/
	printf("<card id='card2' title='Process Commands'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>Process Commands</b><br/>\n");

	if(enable_notifications == FALSE)
		printf("<b><anchor title='Enable Notifications'>Enable Notifications<go href='%s' method='post'><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, CMD_ENABLE_NOTIFICATIONS, CMDMODE_COMMIT);
	else
		printf("<b><anchor title='Disable Notifications'>Disable Notifications<go href='%s' method='post'><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, CMD_DISABLE_NOTIFICATIONS, CMDMODE_COMMIT);

	if(execute_service_checks == FALSE)
		printf("<b><anchor title='Enable Check Execution'>Enable Check Execution<go href='%s' method='post'><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, CMD_START_EXECUTING_SVC_CHECKS, CMDMODE_COMMIT);
	else
		printf("<b><anchor title='Disable Check Execution'>Disable Check Execution<go href='%s' method='post'><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, CMD_STOP_EXECUTING_SVC_CHECKS, CMDMODE_COMMIT);

	printf("</p>\n");

	printf("</card>\n");


	return;
	}



/* displays quick stats */
void display_quick_stats(void) {
	host *temp_host;
	hoststatus *temp_hoststatus;
	service *temp_service;
	servicestatus *temp_servicestatus;
	int hosts_unreachable = 0;
	int hosts_down = 0;
	int hosts_up = 0;
	int hosts_pending = 0;
	int services_critical = 0;
	int services_unknown = 0;
	int services_warning = 0;
	int services_ok = 0;
	int services_pending = 0;


	/**** MAIN SCREEN (CARD 1) ****/
	printf("<card id='card1' title='Quick Stats'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>Quick Stats</b><br/>\n");
	printf("</p>\n");

	/* check all hosts */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		if(is_authorized_for_host(temp_host, &current_authdata) == FALSE)
			continue;

		temp_hoststatus = find_hoststatus(temp_host->name);
		if(temp_hoststatus == NULL)
			continue;

		if(temp_hoststatus->status == SD_HOST_UNREACHABLE)
			hosts_unreachable++;
		else if(temp_hoststatus->status == SD_HOST_DOWN)
			hosts_down++;
		else if(temp_hoststatus->status == HOST_PENDING)
			hosts_pending++;
		else
			hosts_up++;
		}

	/* check all services */
	for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

		if(is_authorized_for_service(temp_service, &current_authdata) == FALSE)
			continue;

		temp_servicestatus = find_servicestatus(temp_service->host_name, temp_service->description);
		if(temp_servicestatus == NULL)
			continue;

		if(temp_servicestatus->status == SERVICE_CRITICAL)
			services_critical++;
		else if(temp_servicestatus->status == SERVICE_UNKNOWN)
			services_unknown++;
		else if(temp_servicestatus->status == SERVICE_WARNING)
			services_warning++;
		else if(temp_servicestatus->status == SERVICE_PENDING)
			services_pending++;
		else
			services_ok++;
		}

	printf("<p align='left' mode='nowrap'>\n");

	printf("<b>Host Totals</b>:<br/>\n");
	printf("%d UP<br/>\n", hosts_up);
	printf("%d DOWN<br/>\n", hosts_down);
	printf("%d UNREACHABLE<br/>\n", hosts_unreachable);
	printf("%d PENDING<br/>\n", hosts_pending);

	printf("<br/>\n");

	printf("<b>Service Totals:</b><br/>\n");
	printf("%d OK<br/>\n", services_ok);
	printf("%d WARNING<br/>\n", services_warning);
	printf("%d UNKNOWN<br/>\n", services_unknown);
	printf("%d CRITICAL<br/>\n", services_critical);
	printf("%d PENDING<br/>\n", services_pending);

	printf("</p>\n");

	printf("</card>\n");

	return;
	}



/* displays hostgroup status overview */
void display_hostgroup_overview(void) {
	hostgroup *temp_hostgroup;
	hostsmember *temp_member;
	host *temp_host;
	hoststatus *temp_hoststatus;


	/**** MAIN SCREEN (CARD 1) ****/
	printf("<card id='card1' title='Status Overview'>\n");
	printf("<p align='center' mode='nowrap'>\n");

	printf("<b><anchor title='Status Overview'>Status Overview<go href='%s' method='post'><postfield name='hostgroup' value='%s'/><postfield name='style' value='summary'/></go></anchor></b><br/><br/>\n", STATUSWML_CGI, escape_string(hostgroup_name));

	/* check all hostgroups */
	for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {

		if(show_all_hostgroups == FALSE && strcmp(temp_hostgroup->group_name, hostgroup_name))
			continue;

		if(is_authorized_for_hostgroup(temp_hostgroup, &current_authdata) == FALSE)
			continue;

		printf("<b>%s</b>\n", temp_hostgroup->alias);

		printf("<table columns='2' align='LL'>\n");

		/* check all hosts in this hostgroup */
		for(temp_member = temp_hostgroup->members; temp_member != NULL; temp_member = temp_member->next) {

			temp_host = find_host(temp_member->host_name);
			if(temp_host == NULL)
				continue;

			if(is_host_member_of_hostgroup(temp_hostgroup, temp_host) == FALSE)
				continue;

			temp_hoststatus = find_hoststatus(temp_host->name);
			if(temp_hoststatus == NULL)
				continue;

			printf("<tr><td><anchor title='%s'>", temp_host->name);
			if(temp_hoststatus->status == SD_HOST_UP)
				printf("UP");
			else if(temp_hoststatus->status == HOST_PENDING)
				printf("PND");
			else if(temp_hoststatus->status == SD_HOST_DOWN)
				printf("DWN");
			else if(temp_hoststatus->status == SD_HOST_UNREACHABLE)
				printf("UNR");
			else
				printf("???");
			printf("<go href='%s' method='post'><postfield name='host' value='%s'/></go></anchor></td>", STATUSWML_CGI, temp_host->name);
			printf("<td>%s</td></tr>\n", temp_host->name);
			}

		printf("</table>\n");

		printf("<br/>\n");
		}

	if(show_all_hostgroups == FALSE)
		printf("<b><anchor title='View All Hostgroups'>View All Hostgroups<go href='%s' method='post'><postfield name='hostgroup' value='all'/><postfield name='style' value='overview'/></go></anchor></b>\n", STATUSWML_CGI);

	printf("</p>\n");
	printf("</card>\n");

	return;
	}


/* displays hostgroup status summary */
void display_hostgroup_summary(void) {
	hostgroup *temp_hostgroup;
	hostsmember *temp_member;
	host *temp_host;
	hoststatus *temp_hoststatus;
	service *temp_service;
	servicestatus *temp_servicestatus;
	int hosts_unreachable = 0;
	int hosts_down = 0;
	int hosts_up = 0;
	int hosts_pending = 0;
	int services_critical = 0;
	int services_unknown = 0;
	int services_warning = 0;
	int services_ok = 0;
	int services_pending = 0;
	int found = 0;


	/**** MAIN SCREEN (CARD 1) ****/
	printf("<card id='card1' title='Status Summary'>\n");
	printf("<p align='center' mode='nowrap'>\n");

	printf("<b><anchor title='Status Summary'>Status Summary<go href='%s' method='post'><postfield name='hostgroup' value='%s'/><postfield name='style' value='overview'/></go></anchor></b><br/><br/>\n", STATUSWML_CGI, escape_string(hostgroup_name));

	/* check all hostgroups */
	for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {

		if(show_all_hostgroups == FALSE && strcmp(temp_hostgroup->group_name, hostgroup_name))
			continue;

		if(is_authorized_for_hostgroup(temp_hostgroup, &current_authdata) == FALSE)
			continue;

		printf("<b><anchor title='%s'>%s<go href='%s' method='post'><postfield name='hostgroup' value='%s'/><postfield name='style' value='overview'/></go></anchor></b>\n", temp_hostgroup->group_name, temp_hostgroup->alias, STATUSWML_CGI, temp_hostgroup->group_name);

		printf("<table columns='2' align='LL'>\n");

		hosts_up = 0;
		hosts_pending = 0;
		hosts_down = 0;
		hosts_unreachable = 0;

		services_ok = 0;
		services_pending = 0;
		services_warning = 0;
		services_unknown = 0;
		services_critical = 0;

		/* check all hosts in this hostgroup */
		for(temp_member = temp_hostgroup->members; temp_member != NULL; temp_member = temp_member->next) {

			temp_host = find_host(temp_member->host_name);
			if(temp_host == NULL)
				continue;

			if(is_host_member_of_hostgroup(temp_hostgroup, temp_host) == FALSE)
				continue;

			temp_hoststatus = find_hoststatus(temp_host->name);
			if(temp_hoststatus == NULL)
				continue;

			if(temp_hoststatus->status == SD_HOST_UNREACHABLE)
				hosts_unreachable++;
			else if(temp_hoststatus->status == SD_HOST_DOWN)
				hosts_down++;
			else if(temp_hoststatus->status == HOST_PENDING)
				hosts_pending++;
			else
				hosts_up++;

			/* check all services on this host */
			for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

				if(strcmp(temp_service->host_name, temp_host->name))
					continue;

				if(is_authorized_for_service(temp_service, &current_authdata) == FALSE)
					continue;

				temp_servicestatus = find_servicestatus(temp_service->host_name, temp_service->description);
				if(temp_servicestatus == NULL)
					continue;

				if(temp_servicestatus->status == SERVICE_CRITICAL)
					services_critical++;
				else if(temp_servicestatus->status == SERVICE_UNKNOWN)
					services_unknown++;
				else if(temp_servicestatus->status == SERVICE_WARNING)
					services_warning++;
				else if(temp_servicestatus->status == SERVICE_PENDING)
					services_pending++;
				else
					services_ok++;
				}
			}

		printf("<tr><td>Hosts:</td><td>");
		found = 0;
		if(hosts_unreachable > 0) {
			printf("%d UNR", hosts_unreachable);
			found = 1;
			}
		if(hosts_down > 0) {
			printf("%s%d DWN", (found == 1) ? ", " : "", hosts_down);
			found = 1;
			}
		if(hosts_pending > 0) {
			printf("%s%d PND", (found == 1) ? ", " : "", hosts_pending);
			found = 1;
			}
		printf("%s%d UP", (found == 1) ? ", " : "", hosts_up);
		printf("</td></tr>\n");
		printf("<tr><td>Services:</td><td>");
		found = 0;
		if(services_critical > 0) {
			printf("%d CRI", services_critical);
			found = 1;
			}
		if(services_warning > 0) {
			printf("%s%d WRN", (found == 1) ? ", " : "", services_warning);
			found = 1;
			}
		if(services_unknown > 0) {
			printf("%s%d UNK", (found == 1) ? ", " : "", services_unknown);
			found = 1;
			}
		if(services_pending > 0) {
			printf("%s%d PND", (found == 1) ? ", " : "", services_pending);
			found = 1;
			}
		printf("%s%d OK", (found == 1) ? ", " : "", services_ok);
		printf("</td></tr>\n");

		printf("</table>\n");

		printf("<br/>\n");
		}

	if(show_all_hostgroups == FALSE)
		printf("<b><anchor title='View All Hostgroups'>View All Hostgroups<go href='%s' method='post'><postfield name='hostgroup' value='all'/><postfield name='style' value='summary'/></go></anchor></b>\n", STATUSWML_CGI);

	printf("</p>\n");

	printf("</card>\n");

	return;
	}



/* displays host status */
void display_host(void) {
	host *temp_host;
	hoststatus *temp_hoststatus;
	char last_check[MAX_DATETIME_LENGTH];
	int days;
	int hours;
	int minutes;
	int seconds;
	time_t current_time;
	time_t t;
	char state_duration[48];
	int found;

	/**** MAIN SCREEN (CARD 1) ****/
	printf("<card id='card1' title='Host Status'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>Host '%s'</b><br/>\n", host_name);

	/* find the host */
	temp_host = find_host(host_name);
	temp_hoststatus = find_hoststatus(host_name);
	if(temp_host == NULL || temp_hoststatus == NULL) {

		printf("<b>Error: Could not find host!</b>\n");
		printf("</p>\n");
		printf("</card>\n");
		return;
		}

	/* check authorization */
	if(is_authorized_for_host(temp_host, &current_authdata) == FALSE) {

		printf("<b>Error: Not authorized for host!</b>\n");
		printf("</p>\n");
		printf("</card>\n");
		return;
		}


	printf("<table columns='2' align='LL'>\n");

	printf("<tr><td>Status:</td><td>");
	if(temp_hoststatus->status == SD_HOST_UP)
		printf("UP");
	else if(temp_hoststatus->status == HOST_PENDING)
		printf("PENDING");
	else if(temp_hoststatus->status == SD_HOST_DOWN)
		printf("DOWN");
	else if(temp_hoststatus->status == SD_HOST_UNREACHABLE)
		printf("UNREACHABLE");
	else
		printf("?");
	printf("</td></tr>\n");

	printf("<tr><td>Info:</td><td>%s</td></tr>\n", temp_hoststatus->plugin_output);

	get_time_string(&temp_hoststatus->last_check, last_check, sizeof(last_check) - 1, SHORT_DATE_TIME);
	printf("<tr><td>Last Check:</td><td>%s</td></tr>\n", last_check);

	current_time = time(NULL);
	if(temp_hoststatus->last_state_change == (time_t)0)
		t = current_time - program_start;
	else
		t = current_time - temp_hoststatus->last_state_change;
	get_time_breakdown((unsigned long)t, &days, &hours, &minutes, &seconds);
	snprintf(state_duration, sizeof(state_duration) - 1, "%2dd %2dh %2dm %2ds%s", days, hours, minutes, seconds, (temp_hoststatus->last_state_change == (time_t)0) ? "+" : "");
	printf("<tr><td>Duration:</td><td>%s</td></tr>\n", state_duration);

	printf("<tr><td>Properties:</td><td>");
	found = 0;
	if(temp_hoststatus->checks_enabled == FALSE) {
		printf("%sChecks disabled", (found == 1) ? ", " : "");
		found = 1;
		}
	if(temp_hoststatus->notifications_enabled == FALSE) {
		printf("%sNotifications disabled", (found == 1) ? ", " : "");
		found = 1;
		}
	if(temp_hoststatus->problem_has_been_acknowledged == TRUE) {
		printf("%sProblem acknowledged", (found == 1) ? ", " : "");
		found = 1;
		}
	if(temp_hoststatus->scheduled_downtime_depth > 0) {
		printf("%sIn scheduled downtime", (found == 1) ? ", " : "");
		found = 1;
		}
	if(found == 0)
		printf("N/A");
	printf("</td></tr>\n");

	printf("</table>\n");
	printf("<br/>\n");
	printf("<b><anchor title='View Services'>View Services<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='style' value='servicedetail'/></go></anchor></b>\n", STATUSWML_CGI, escape_string(host_name));
	printf("<b><anchor title='Host Commands'>Host Commands<go href='#card2'/></anchor></b>\n");
	printf("</p>\n");

	printf("</card>\n");


	/**** COMMANDS SCREEN (CARD 2) ****/
	printf("<card id='card2' title='Host Commands'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>Host Commands</b><br/>\n");

	printf("<b><anchor title='Ping Host'>Ping Host<go href='%s' method='post'><postfield name='ping' value='%s'/></go></anchor></b>\n", STATUSWML_CGI, temp_host->address);
	printf("<b><anchor title='Traceroute'>Traceroute<go href='%s' method='post'><postfield name='traceroute' value='%s'/></go></anchor></b>\n", STATUSWML_CGI, temp_host->address);

	if(temp_hoststatus->status != SD_HOST_UP && temp_hoststatus->status != HOST_PENDING)
		printf("<b><anchor title='Acknowledge Problem'>Acknowledge Problem<go href='#card3'/></anchor></b>\n");

	if(temp_hoststatus->checks_enabled == FALSE)
		printf("<b><anchor title='Enable Host Checks'>Enable Host Checks<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, escape_string(host_name), CMD_ENABLE_HOST_CHECK, CMDMODE_COMMIT);
	else
		printf("<b><anchor title='Disable Host Checks'>Disable Host Checks<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, escape_string(host_name), CMD_DISABLE_HOST_CHECK, CMDMODE_COMMIT);

	if(temp_hoststatus->notifications_enabled == FALSE)
		printf("<b><anchor title='Enable Host Notifications'>Enable Host Notifications<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, escape_string(host_name), CMD_ENABLE_HOST_NOTIFICATIONS, CMDMODE_COMMIT);
	else
		printf("<b><anchor title='Disable Host Notifications'>Disable Host Notifications<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, escape_string(host_name), CMD_DISABLE_HOST_NOTIFICATIONS, CMDMODE_COMMIT);


	printf("<b><anchor title='Enable All Service Checks'>Enable All Service Checks<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, escape_string(host_name), CMD_ENABLE_HOST_SVC_CHECKS, CMDMODE_COMMIT);

	printf("<b><anchor title='Disable All Service Checks'>Disable All Service Checks<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, escape_string(host_name), CMD_DISABLE_HOST_SVC_CHECKS, CMDMODE_COMMIT);

	printf("<b><anchor title='Enable All Service Notifications'>Enable All Service Notifications<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, escape_string(host_name), CMD_ENABLE_HOST_SVC_NOTIFICATIONS, CMDMODE_COMMIT);

	printf("<b><anchor title='Disable All Service Notifications'>Disable All Service Notifications<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", COMMAND_CGI, escape_string(host_name), CMD_DISABLE_HOST_SVC_NOTIFICATIONS, CMDMODE_COMMIT);

	printf("</p>\n");

	printf("</card>\n");


	/**** ACKNOWLEDGEMENT SCREEN (CARD 3) ****/
	printf("<card id='card3' title='Acknowledge Problem'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>Acknowledge Problem</b><br/>\n");
	printf("</p>\n");

	printf("<p align='center' mode='wrap'>\n");
	printf("<b>Your Name:</b><br/>\n");
	printf("<input name='name' value='%s' /><br/>\n", ((use_ssl_authentication) ? (getenv("SSL_CLIENT_S_DN_CN")) : (getenv("REMOTE_USER"))));
	printf("<b>Comment:</b><br/>\n");
	printf("<input name='comment' value='acknowledged by WAP'/>\n");

	printf("<do type='accept'>\n");
	printf("<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='com_author' value='$(name)'/><postfield name='com_data' value='$(comment)'/><postfield name='persistent' value=''/><postfield name='send_notification' value=''/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go>\n", COMMAND_CGI, escape_string(host_name), CMD_ACKNOWLEDGE_HOST_PROBLEM, CMDMODE_COMMIT);
	printf("</do>\n");

	printf("</p>\n");

	printf("</card>\n");

	return;
	}



/* displays services on a host */
void display_host_services(void) {
	service *temp_service;
	servicestatus *temp_servicestatus;

	/**** MAIN SCREEN (CARD 1) ****/
	printf("<card id='card1' title='Host Services'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>Host <anchor title='%s'>", url_encode(host_name));
	printf("'%s'<go href='%s' method='post'><postfield name='host' value='%s'/></go></anchor> Services</b><br/>\n", host_name, STATUSWML_CGI, escape_string(host_name));

	printf("<table columns='2' align='LL'>\n");

	/* check all services */
	for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

		if(strcmp(temp_service->host_name, host_name))
			continue;

		if(is_authorized_for_service(temp_service, &current_authdata) == FALSE)
			continue;

		temp_servicestatus = find_servicestatus(temp_service->host_name, temp_service->description);
		if(temp_servicestatus == NULL)
			continue;

		printf("<tr><td><anchor title='%s'>", temp_service->description);
		if(temp_servicestatus->status == SERVICE_OK)
			printf("OK");
		else if(temp_servicestatus->status == SERVICE_PENDING)
			printf("PND");
		else if(temp_servicestatus->status == SERVICE_WARNING)
			printf("WRN");
		else if(temp_servicestatus->status == SERVICE_UNKNOWN)
			printf("UNK");
		else if(temp_servicestatus->status == SERVICE_CRITICAL)
			printf("CRI");
		else
			printf("???");

		printf("<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='service' value='%s'/></go></anchor></td>", STATUSWML_CGI, temp_service->host_name, temp_service->description);
		printf("<td>%s</td></tr>\n", temp_service->description);
		}

	printf("</table>\n");

	printf("</p>\n");

	printf("</card>\n");

	return;
	}



/* displays service status */
void display_service(void) {
	service *temp_service;
	servicestatus *temp_servicestatus;
	char last_check[MAX_DATETIME_LENGTH];
	int days;
	int hours;
	int minutes;
	int seconds;
	time_t current_time;
	time_t t;
	char state_duration[48];
	int found;

	/**** MAIN SCREEN (CARD 1) ****/
	printf("<card id='card1' title='Service Status'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>Service '%s' on host '%s'</b><br/>\n", service_desc, host_name);

	/* find the service */
	temp_service = find_service(host_name, service_desc);
	temp_servicestatus = find_servicestatus(host_name, service_desc);
	if(temp_service == NULL || temp_servicestatus == NULL) {

		printf("<b>Error: Could not find service!</b>\n");
		printf("</p>\n");
		printf("</card>\n");
		return;
		}

	/* check authorization */
	if(is_authorized_for_service(temp_service, &current_authdata) == FALSE) {

		printf("<b>Error: Not authorized for service!</b>\n");
		printf("</p>\n");
		printf("</card>\n");
		return;
		}


	printf("<table columns='2' align='LL'>\n");

	printf("<tr><td>Status:</td><td>");
	if(temp_servicestatus->status == SERVICE_OK)
		printf("OK");
	else if(temp_servicestatus->status == SERVICE_PENDING)
		printf("PENDING");
	else if(temp_servicestatus->status == SERVICE_WARNING)
		printf("WARNING");
	else if(temp_servicestatus->status == SERVICE_UNKNOWN)
		printf("UNKNOWN");
	else if(temp_servicestatus->status == SERVICE_CRITICAL)
		printf("CRITICAL");
	else
		printf("?");
	printf("</td></tr>\n");

	printf("<tr><td>Info:</td><td>%s</td></tr>\n", temp_servicestatus->plugin_output);

	get_time_string(&temp_servicestatus->last_check, last_check, sizeof(last_check) - 1, SHORT_DATE_TIME);
	printf("<tr><td>Last Check:</td><td>%s</td></tr>\n", last_check);

	current_time = time(NULL);
	if(temp_servicestatus->last_state_change == (time_t)0)
		t = current_time - program_start;
	else
		t = current_time - temp_servicestatus->last_state_change;
	get_time_breakdown((unsigned long)t, &days, &hours, &minutes, &seconds);
	snprintf(state_duration, sizeof(state_duration) - 1, "%2dd %2dh %2dm %2ds%s", days, hours, minutes, seconds, (temp_servicestatus->last_state_change == (time_t)0) ? "+" : "");
	printf("<tr><td>Duration:</td><td>%s</td></tr>\n", state_duration);

	printf("<tr><td>Properties:</td><td>");
	found = 0;
	if(temp_servicestatus->checks_enabled == FALSE) {
		printf("%sChecks disabled", (found == 1) ? ", " : "");
		found = 1;
		}
	if(temp_servicestatus->notifications_enabled == FALSE) {
		printf("%sNotifications disabled", (found == 1) ? ", " : "");
		found = 1;
		}
	if(temp_servicestatus->problem_has_been_acknowledged == TRUE) {
		printf("%sProblem acknowledged", (found == 1) ? ", " : "");
		found = 1;
		}
	if(temp_servicestatus->scheduled_downtime_depth > 0) {
		printf("%sIn scheduled downtime", (found == 1) ? ", " : "");
		found = 1;
		}
	if(found == 0)
		printf("N/A");
	printf("</td></tr>\n");

	printf("</table>\n");
	printf("<br/>\n");
	printf("<b><anchor title='View Host'>View Host<go href='%s' method='post'><postfield name='host' value='%s'/></go></anchor></b>\n", STATUSWML_CGI, escape_string(host_name));
	printf("<b><anchor title='Service Commands'>Svc. Commands<go href='#card2'/></anchor></b>\n");
	printf("</p>\n");

	printf("</card>\n");


	/**** COMMANDS SCREEN (CARD 2) ****/
	printf("<card id='card2' title='Service Commands'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>Service Commands</b><br/>\n");

	if(temp_servicestatus->status != SERVICE_OK && temp_servicestatus->status != SERVICE_PENDING)
		printf("<b><anchor title='Acknowledge Problem'>Acknowledge Problem<go href='#card3'/></anchor></b>\n");

	if(temp_servicestatus->checks_enabled == FALSE) {
		printf("<b><anchor title='Enable Checks'>Enable Checks<go href='%s' method='post'><postfield name='host' value='%s'/>", COMMAND_CGI, escape_string(host_name));
		printf("<postfield name='service' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", escape_string(service_desc), CMD_ENABLE_SVC_CHECK, CMDMODE_COMMIT);
		}
	else {
		printf("<b><anchor title='Disable Checks'>Disable Checks<go href='%s' method='post'><postfield name='host' value='%s'/>", COMMAND_CGI, escape_string(host_name));
		printf("<postfield name='service' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", escape_string(service_desc), CMD_DISABLE_SVC_CHECK, CMDMODE_COMMIT);

		printf("<b><anchor title='Schedule Immediate Check'>Schedule Immediate Check<go href='%s' method='post'><postfield name='host' value='%s'/>", COMMAND_CGI, escape_string(host_name));
		printf("<postfield name='service' value='%s'/><postfield name='start_time' value='%lu'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", escape_string(service_desc), (unsigned long)current_time, CMD_SCHEDULE_SVC_CHECK, CMDMODE_COMMIT);
		}

	if(temp_servicestatus->notifications_enabled == FALSE) {
		printf("<b><anchor title='Enable Notifications'>Enable Notifications<go href='%s' method='post'><postfield name='host' value='%s'/>", COMMAND_CGI, escape_string(host_name));
		printf("<postfield name='service' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", escape_string(service_desc), CMD_ENABLE_SVC_NOTIFICATIONS, CMDMODE_COMMIT);
		}
	else {
		printf("<b><anchor title='Disable Notifications'>Disable Notifications<go href='%s' method='post'><postfield name='host' value='%s'/>", COMMAND_CGI, escape_string(host_name));
		printf("<postfield name='service' value='%s'/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go></anchor></b><br/>\n", escape_string(service_desc), CMD_DISABLE_SVC_NOTIFICATIONS, CMDMODE_COMMIT);
		}

	printf("</p>\n");

	printf("</card>\n");


	/**** ACKNOWLEDGEMENT SCREEN (CARD 3) ****/
	printf("<card id='card3' title='Acknowledge Problem'>\n");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>Acknowledge Problem</b><br/>\n");
	printf("</p>\n");

	printf("<p align='center' mode='wrap'>\n");
	printf("<b>Your Name:</b><br/>\n");
	printf("<input name='name' value='%s' /><br/>\n", ((use_ssl_authentication) ? (getenv("SSL_CLIENT_S_DN_CN")) : (getenv("REMOTE_USER"))));
	printf("<b>Comment:</b><br/>\n");
	printf("<input name='comment' value='acknowledged by WAP'/>\n");

	printf("<do type='accept'>\n");
	printf("<go href='%s' method='post'><postfield name='host' value='%s'/>", COMMAND_CGI, escape_string(host_name));
	printf("<postfield name='service' value='%s'/><postfield name='com_author' value='$(name)'/><postfield name='com_data' value='$(comment)'/><postfield name='persistent' value=''/><postfield name='send_notification' value=''/><postfield name='cmd_typ' value='%d'/><postfield name='cmd_mod' value='%d'/><postfield name='content' value='wml'/></go>\n", escape_string(service_desc), CMD_ACKNOWLEDGE_SVC_PROBLEM, CMDMODE_COMMIT);
	printf("</do>\n");

	printf("</p>\n");

	printf("</card>\n");

	return;
	}


/* displays ping results */
void display_ping(void) {
	char input_buffer[MAX_INPUT_BUFFER];
	char buffer[MAX_INPUT_BUFFER];
	char *temp_ptr;
	FILE *fp;
	int odd = 0;
	int in_macro = FALSE;

	/**** MAIN SCREEN (CARD 1) ****/
	printf("<card id='card1' title='Ping'>\n");

	if(!strcmp(ping_address, "")) {

		printf("<p align='center' mode='nowrap'>\n");
		printf("<b>Ping Host</b><br/>\n");
		printf("</p>\n");

		printf("<p align='center' mode='wrap'>\n");
		printf("<b>Host Name/Address:</b><br/>\n");
		printf("<input name='address'/>\n");
		printf("<do type='accept'>\n");
		printf("<go href='%s'><postfield name='ping' value='$(address)'/></go>\n", STATUSWML_CGI);
		printf("</do>\n");
		printf("</p>\n");
		}

	else {

		printf("<p align='center' mode='nowrap'>\n");
		printf("<b>Results For Ping Of %s:</b><br/>\n", ping_address);
		printf("</p>\n");

		printf("<p mode='nowrap'>\n");

		if(ping_syntax == NULL)
			printf("ping_syntax in CGI config file is NULL!\n");

		else {

			/* process macros in the ping syntax */
			strcpy(buffer, "");
			strncpy(input_buffer, ping_syntax, sizeof(input_buffer) - 1);
			input_buffer[strlen(ping_syntax) - 1] = '\x0';
			for(temp_ptr = my_strtok(input_buffer, "$"); temp_ptr != NULL; temp_ptr = my_strtok(NULL, "$")) {

				if(in_macro == FALSE) {
					if(strlen(buffer) + strlen(temp_ptr) < sizeof(buffer) - 1) {
						strncat(buffer, temp_ptr, sizeof(buffer) - strlen(buffer) - 1);
						buffer[sizeof(buffer) - 1] = '\x0';
						}
					in_macro = TRUE;
					}
				else {

					if(strlen(buffer) + strlen(temp_ptr) < sizeof(buffer) - 1) {

						if(!strcmp(temp_ptr, "HOSTADDRESS"))
							strncat(buffer, ping_address, sizeof(buffer) - strlen(buffer) - 1);
						}

					in_macro = FALSE;
					}
				}

			/* run the ping command */
			fp = popen(buffer, "r");
			if(fp) {
				while(1) {
					fgets(buffer, sizeof(buffer) - 1, fp);
					if(feof(fp))
						break;

					strip(buffer);

					if(odd) {
						odd = 0;
						printf("%s<br/>\n", buffer);
						}
					else {
						odd = 1;
						printf("<b>%s</b><br/>\n", buffer);
						}
					}
				}
			else
				printf("Error executing ping!\n");

			pclose(fp);
			}

		printf("</p>\n");
		}

	printf("</card>\n");

	return;
	}


/* displays traceroute results */
void display_traceroute(void) {
	char buffer[MAX_INPUT_BUFFER];
	FILE *fp;
	int odd = 0;

	/**** MAIN SCREEN (CARD 1) ****/
	printf("<card id='card1' title='Traceroute'>\n");

	if(!strcmp(traceroute_address, "")) {

		printf("<p align='center' mode='nowrap'>\n");
		printf("<b>Traceroute</b><br/>\n");
		printf("</p>\n");

		printf("<p align='center' mode='wrap'>\n");
		printf("<b>Host Name/Address:</b><br/>\n");
		printf("<input name='address'/>\n");
		printf("<do type='accept'>\n");
		printf("<go href='%s'><postfield name='traceroute' value='$(address)'/></go>\n", STATUSWML_CGI);
		printf("</do>\n");
		printf("</p>\n");
		}

	else {

		printf("<p align='center' mode='nowrap'>\n");
		printf("<b>Results For Traceroute To %s:</b><br/>\n", traceroute_address);
		printf("</p>\n");

		printf("<p mode='nowrap'>\n");

		snprintf(buffer, sizeof(buffer) - 1, "%s %s", TRACEROUTE_COMMAND, traceroute_address);
		buffer[sizeof(buffer) - 1] = '\x0';

		fp = popen(buffer, "r");
		if(fp) {
			while(1) {
				fgets(buffer, sizeof(buffer) - 1, fp);
				if(feof(fp))
					break;

				strip(buffer);

				if(odd) {
					odd = 0;
					printf("%s<br/>\n", buffer);
					}
				else {
					odd = 1;
					printf("<b>%s</b><br/>\n", buffer);
					}
				}
			}
		else
			printf("Error executing traceroute!\n");

		pclose(fp);

		printf("</p>\n");
		}

	printf("</card>\n");

	return;
	}



/* displays problems */
void display_problems(void) {
	host *temp_host;
	service *temp_service;
	hoststatus *temp_hoststatus;
	int total_host_problems = 0;
	servicestatus *temp_servicestatus;
	int total_service_problems = 0;

	/**** MAIN SCREEN (CARD 1) ****/
	printf("<card id='card1' title='%s Problems'>\n", (display_type == DISPLAY_ALL_PROBLEMS) ? "All" : "Unhandled");
	printf("<p align='center' mode='nowrap'>\n");
	printf("<b>%s Problems</b><br/><br/>\n", (display_type == DISPLAY_ALL_PROBLEMS) ? "All" : "Unhandled");

	printf("<b>Host Problems:</b>\n");

	printf("<table columns='2' align='LL'>\n");

	/* check all hosts */
	for(temp_hoststatus = hoststatus_list; temp_hoststatus != NULL; temp_hoststatus = temp_hoststatus->next) {

		temp_host = find_host(temp_hoststatus->host_name);
		if(temp_host == NULL)
			continue;

		if(is_authorized_for_host(temp_host, &current_authdata) == FALSE)
			continue;

		if(temp_hoststatus->status == SD_HOST_UP || temp_hoststatus->status == HOST_PENDING)
			continue;

		if(display_type == DISPLAY_UNHANDLED_PROBLEMS) {
			if(temp_hoststatus->problem_has_been_acknowledged == TRUE)
				continue;
			if(temp_hoststatus->notifications_enabled == FALSE)
				continue;
			if(temp_hoststatus->scheduled_downtime_depth > 0)
				continue;
			}

		total_host_problems++;

		printf("<tr><td><anchor title='%s'>", temp_host->name);
		if(temp_hoststatus->status == SD_HOST_DOWN)
			printf("DWN");
		else if(temp_hoststatus->status == SD_HOST_UNREACHABLE)
			printf("UNR");
		else
			printf("???");
		printf("<go href='%s' method='post'><postfield name='host' value='%s'/></go></anchor></td>", STATUSWML_CGI, temp_host->name);
		printf("<td>%s</td></tr>\n", temp_host->name);
		}

	if(total_host_problems == 0)
		printf("<tr><td>No problems</td></tr>\n");

	printf("</table>\n");

	printf("<br/>\n");


	printf("<b>Svc Problems:</b>\n");

	printf("<table columns='2' align='LL'>\n");

	/* check all services */
	for(temp_servicestatus = servicestatus_list; temp_servicestatus != NULL; temp_servicestatus = temp_servicestatus->next) {

		temp_service = find_service(temp_servicestatus->host_name, temp_servicestatus->description);
		if(temp_service == NULL)
			continue;

		if(is_authorized_for_service(temp_service, &current_authdata) == FALSE)
			continue;

		if(temp_servicestatus->status == SERVICE_OK || temp_servicestatus->status == SERVICE_PENDING)
			continue;

		if(display_type == DISPLAY_UNHANDLED_PROBLEMS) {
			if(temp_servicestatus->problem_has_been_acknowledged == TRUE)
				continue;
			if(temp_servicestatus->notifications_enabled == FALSE)
				continue;
			if(temp_servicestatus->scheduled_downtime_depth > 0)
				continue;
			if((temp_hoststatus = find_hoststatus(temp_service->host_name))) {
				if(temp_hoststatus->scheduled_downtime_depth > 0)
					continue;
				if(temp_hoststatus->problem_has_been_acknowledged == TRUE)
					continue;
				}
			}

		total_service_problems++;

		printf("<tr><td><anchor title='%s'>", temp_servicestatus->description);
		if(temp_servicestatus->status == SERVICE_CRITICAL)
			printf("CRI");
		else if(temp_servicestatus->status == SERVICE_WARNING)
			printf("WRN");
		else if(temp_servicestatus->status == SERVICE_UNKNOWN)
			printf("UNK");
		else
			printf("???");
		printf("<go href='%s' method='post'><postfield name='host' value='%s'/><postfield name='service' value='%s'/></go></anchor></td>", STATUSWML_CGI, temp_service->host_name, temp_service->description);
		printf("<td>%s/%s</td></tr>\n", temp_service->host_name, temp_service->description);
		}

	if(total_service_problems == 0)
		printf("<tr><td>No problems</td></tr>\n");

	printf("</table>\n");

	printf("</p>\n");

	printf("</card>\n");

	return;
	}
