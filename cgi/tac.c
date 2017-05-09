/***********************************************************************
 *
 * TAC.C - Nagios Tactical Monitoring Overview CGI
 *
 *
 * This CGI program will display the contents of the Nagios
 * log file.
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
 ***********************************************************************/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"

#include "../include/getcgi.h"
#include "../include/cgiutils.h"
#include "../include/cgiauth.h"


#define HEALTH_WARNING_PERCENTAGE       90
#define HEALTH_CRITICAL_PERCENTAGE      75


/* HOSTOUTAGE structure */
typedef struct hostoutage_struct {
	host *hst;
	int  affected_child_hosts;
	struct hostoutage_struct *next;
	} hostoutage;


extern char   main_config_file[MAX_FILENAME_LENGTH];
extern char   url_html_path[MAX_FILENAME_LENGTH];
extern char   url_images_path[MAX_FILENAME_LENGTH];
extern char   url_stylesheets_path[MAX_FILENAME_LENGTH];
extern char   url_media_path[MAX_FILENAME_LENGTH];
extern char   url_js_path[MAX_FILENAME_LENGTH];

extern int    refresh_rate;
extern int    tac_cgi_hard_only;

extern char *service_critical_sound;
extern char *service_warning_sound;
extern char *service_unknown_sound;
extern char *host_down_sound;
extern char *host_unreachable_sound;
extern char *normal_sound;

extern hoststatus *hoststatus_list;
extern servicestatus *servicestatus_list;

extern int nagios_process_state;




void analyze_status_data(void);
void display_tac_overview(void);

void find_hosts_causing_outages(void);
void calculate_outage_effect_of_host(host *, int *);
int is_route_to_host_blocked(host *);
int number_of_host_services(host *);
void add_hostoutage(host *);
void free_hostoutage_list(void);

void document_header(int);
void document_footer(void);
int process_cgivars(void);

authdata current_authdata;

int embedded = FALSE;
int display_header = FALSE;

hostoutage *hostoutage_list = NULL;

int total_blocking_outages = 0;
int total_nonblocking_outages = 0;

int total_service_health = 0;
int total_host_health = 0;
int potential_service_health = 0;
int potential_host_health = 0;
double percent_service_health = 0.0;
double percent_host_health = 0.0;

int total_hosts = 0;
int total_services = 0;

int total_active_service_checks = 0;
int total_active_host_checks = 0;
int total_passive_service_checks = 0;
int total_passive_host_checks = 0;

double min_service_execution_time = -1.0;
double max_service_execution_time = -1.0;
double total_service_execution_time = 0.0;
double average_service_execution_time = -1.0;
double min_host_execution_time = -1.0;
double max_host_execution_time = -1.0;
double total_host_execution_time = 0.0;
double average_host_execution_time = -1.0;
double min_service_latency = -1.0;
double max_service_latency = -1.0;
double total_service_latency = 0.0;
double average_service_latency = -1.0;
double min_host_latency = -1.0;
double max_host_latency = -1.0;
double total_host_latency = 0.0;
double average_host_latency = -1.0;

int flapping_services = 0;
int flapping_hosts = 0;
int flap_disabled_services = 0;
int flap_disabled_hosts = 0;
int notification_disabled_services = 0;
int notification_disabled_hosts = 0;
int event_handler_disabled_services = 0;
int event_handler_disabled_hosts = 0;
int active_checks_disabled_services = 0;
int active_checks_disabled_hosts = 0;
int passive_checks_disabled_services = 0;
int passive_checks_disabled_hosts = 0;

int hosts_pending = 0;
int hosts_pending_disabled = 0;
int hosts_up_disabled = 0;
int hosts_up_unacknowledged = 0;
int hosts_up = 0;
int hosts_down_scheduled = 0;
int hosts_down_acknowledged = 0;
int hosts_down_disabled = 0;
int hosts_down_unacknowledged = 0;
int hosts_down = 0;
int hosts_unreachable_scheduled = 0;
int hosts_unreachable_acknowledged = 0;
int hosts_unreachable_disabled = 0;
int hosts_unreachable_unacknowledged = 0;
int hosts_unreachable = 0;

int services_pending = 0;
int services_pending_disabled = 0;
int services_ok_disabled = 0;
int services_ok_unacknowledged = 0;
int services_ok = 0;
int services_warning_host_problem = 0;
int services_warning_scheduled = 0;
int services_warning_acknowledged = 0;
int services_warning_disabled = 0;
int services_warning_unacknowledged = 0;
int services_warning = 0;
int services_unknown_host_problem = 0;
int services_unknown_scheduled = 0;
int services_unknown_acknowledged = 0;
int services_unknown_disabled = 0;
int services_unknown_unacknowledged = 0;
int services_unknown = 0;
int services_critical_host_problem = 0;
int services_critical_scheduled = 0;
int services_critical_acknowledged = 0;
int services_critical_disabled = 0;
int services_critical_unacknowledged = 0;
int services_critical = 0;


/*efine DEBUG 1*/

int main(void) {
	char *sound = NULL;

	/* get the CGI variables passed in the URL */
	process_cgivars();

	/* reset internal variables */
	reset_cgi_vars();

	cgi_init(document_header, document_footer, READ_ALL_OBJECT_DATA, READ_ALL_STATUS_DATA);

	/* get authentication information */
	get_authentication_information(&current_authdata);

	document_header(TRUE);

	if(display_header == TRUE) {

		/* begin top table */
		printf("<table border=0 width=100%% cellpadding=0 cellspacing=0>\n");
		printf("<tr>\n");

		/* left column of top table - info box */
		printf("<td align=left valign=top width=33%%>\n");
		display_info_table("Tactical Status Overview", TRUE, &current_authdata);
		printf("</td>\n");

		/* middle column of top table - log file navigation options */
		printf("<td align=center valign=top width=33%%>\n");
		printf("</td>\n");

		/* right hand column of top row */
		printf("<td align=right valign=top width=33%%>\n");
		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");
		printf("</p>\n");

		}

	/* analyze current host and service status data for tac overview */
	analyze_status_data();

	/* find all hosts that are causing network outages */
	find_hosts_causing_outages();

	/* embed sound tag if necessary... */
	if(hosts_unreachable_unacknowledged > 0 && host_unreachable_sound != NULL)
		sound = host_unreachable_sound;
	else if(hosts_down_unacknowledged > 0 && host_down_sound != NULL)
		sound = host_down_sound;
	else if(services_critical_unacknowledged > 0 && service_critical_sound != NULL)
		sound = service_critical_sound;
	else if(services_warning_unacknowledged > 0 && service_warning_sound != NULL)
		sound = service_warning_sound;
	else if(services_unknown_unacknowledged == 0 && services_warning_unacknowledged == 0 && services_critical_unacknowledged == 0 && hosts_down_unacknowledged == 0 && hosts_unreachable_unacknowledged == 0 && normal_sound != NULL)
		sound = normal_sound;
	if(sound != NULL) {
		printf("<object type=\"audio/x-wav\" data=\"%s%s\" height=\"1\" width=\"1\">", url_media_path, sound);
		printf("<param name=\"filename\" value=\"%s%s\">", url_media_path, sound);
		printf("<param name=\"autostart\" value=\"true\">");
		printf("<param name=\"playcount\" value=\"1\">");
		printf("</object>");
		}


	/**** display main tac screen ****/
	display_tac_overview();

	document_footer();

	/* free memory allocated to the host outage list */
	free_hostoutage_list();

	/* free allocated memory */
	free_memory();

	return OK;
	}




void document_header(int use_stylesheet) {
	char date_time[MAX_DATETIME_LENGTH];
	time_t current_time;
	time_t expire_time;

	printf("Cache-Control: no-store\r\n");
	printf("Pragma: no-cache\r\n");
	printf("Refresh: %d\r\n", refresh_rate);

	time(&current_time);
	get_time_string(&current_time, date_time, (int)sizeof(date_time), HTTP_DATE_TIME);
	printf("Last-Modified: %s\r\n", date_time);

	expire_time = (time_t)0L;
	get_time_string(&expire_time, date_time, (int)sizeof(date_time), HTTP_DATE_TIME);
	printf("Expires: %s\r\n", date_time);

	printf("Content-type: text/html; charset=utf-8\r\n\r\n");

	if(embedded == TRUE)
		return;

	printf("<HTML>\n");
	printf("<HEAD>\n");
	printf("<link rel=\"shortcut icon\" href=\"%sfavicon.ico\" type=\"image/ico\">\n", url_images_path);
	printf("<TITLE>\n");
	printf("Nagios Tactical Monitoring Overview\n");
	printf("</TITLE>\n");

	if(use_stylesheet == TRUE) {
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, COMMON_CSS);
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, TAC_CSS);
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, NAGFUNCS_CSS);
		}

	printf("<script type='text/javascript' src='%s%s'></script>\n", url_js_path, JQUERY_JS);
	printf("<script type='text/javascript' src='%s%s'></script>\n", url_js_path, NAGFUNCS_JS);

	printf("<script type='text/javascript'>\nvar vbox, vBoxId='tac', "
			"vboxText = '<a href=https://www.nagios.com/tours target=_blank>"
			"Click here to watch the entire Nagios Core 4 Tour!</a>';\n");
	printf("$(document).ready(function() {\n"
			"var user = '%s';\nvBoxId += ';' + user;", current_authdata.username);
	printf("vbox = new vidbox({pos:'lr',"
			"vidurl:'https://www.youtube.com/embed/l20YRDhbOfA',text:vboxText,"
			"vidid:vBoxId});");
	printf("\n});\n</script>\n");

	printf("</HEAD>\n");
	printf("<BODY CLASS='tac' marginwidth=2 marginheight=2 topmargin=0 leftmargin=0 rightmargin=0>\n");

	/* include user SSI header */
	include_ssi_files(TAC_CGI, SSI_HEADER);

	return;
	}


void document_footer(void) {

	if(embedded == TRUE)
		return;

	/* include user SSI footer */
	include_ssi_files(TAC_CGI, SSI_FOOTER);

	printf("</BODY>\n");
	printf("</HTML>\n");

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

		/* we found the embed option */
		else if(!strcmp(variables[x], "embedded"))
			embedded = TRUE;

		/* we found the noheader option */
		else if(!strcmp(variables[x], "noheader"))
			display_header = FALSE;

		/* we received an invalid argument */
		else
			error = TRUE;

		}

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return error;
	}



void analyze_status_data(void) {
	servicestatus *temp_servicestatus;
	service *temp_service;
	hoststatus *temp_hoststatus;
	host *temp_host;
	int problem = TRUE;


	/* check all services */
	for(temp_servicestatus = servicestatus_list; temp_servicestatus != NULL; temp_servicestatus = temp_servicestatus->next) {

		/* see if user is authorized to view this service */
		temp_service = find_service(temp_servicestatus->host_name, temp_servicestatus->description);
		if(is_authorized_for_service(temp_service, &current_authdata) == FALSE)
			continue;

		/******** CHECK FEATURES *******/

		/* check flapping */
		if(temp_servicestatus->flap_detection_enabled == FALSE)
			flap_disabled_services++;
		else if(temp_servicestatus->is_flapping == TRUE)
			flapping_services++;

		/* check notifications */
		if(temp_servicestatus->notifications_enabled == FALSE)
			notification_disabled_services++;

		/* check event handler */
		if(temp_servicestatus->event_handler_enabled == FALSE)
			event_handler_disabled_services++;

		/* active check execution */
		if(temp_servicestatus->checks_enabled == FALSE)
			active_checks_disabled_services++;

		/* passive check acceptance */
		if(temp_servicestatus->accept_passive_checks == FALSE)
			passive_checks_disabled_services++;


		/********* CHECK STATUS ********/

		problem = TRUE;

		if(temp_servicestatus->status == SERVICE_OK) {
			if(temp_servicestatus->checks_enabled == FALSE)
				services_ok_disabled++;
			else
				services_ok_unacknowledged++;
			services_ok++;
			}

		else if(temp_servicestatus->status == SERVICE_WARNING) {
			temp_hoststatus = find_hoststatus(temp_servicestatus->host_name);
			if(temp_hoststatus != NULL && (temp_hoststatus->status == SD_HOST_DOWN || temp_hoststatus->status == SD_HOST_UNREACHABLE)) {
				services_warning_host_problem++;
				problem = FALSE;
				}
			if(temp_servicestatus->scheduled_downtime_depth > 0) {
				services_warning_scheduled++;
				problem = FALSE;
				}
			if(temp_servicestatus->problem_has_been_acknowledged == TRUE) {
				services_warning_acknowledged++;
				problem = FALSE;
				}
			if(temp_servicestatus->checks_enabled == FALSE) {
				services_warning_disabled++;
				problem = FALSE;
				}
			if(problem == TRUE) {
				if (temp_servicestatus->state_type == HARD_STATE || tac_cgi_hard_only == FALSE)
					services_warning_unacknowledged++;
			}
			services_warning++;
			}

		else if(temp_servicestatus->status == SERVICE_UNKNOWN) {
			temp_hoststatus = find_hoststatus(temp_servicestatus->host_name);
			if(temp_hoststatus != NULL && (temp_hoststatus->status == SD_HOST_DOWN || temp_hoststatus->status == SD_HOST_UNREACHABLE)) {
				services_unknown_host_problem++;
				problem = FALSE;
				}
			if(temp_servicestatus->scheduled_downtime_depth > 0) {
				services_unknown_scheduled++;
				problem = FALSE;
				}
			if(temp_servicestatus->problem_has_been_acknowledged == TRUE) {
				services_unknown_acknowledged++;
				problem = FALSE;
				}
			if(temp_servicestatus->checks_enabled == FALSE) {
				services_unknown_disabled++;
				problem = FALSE;
				}
			if(problem == TRUE) {
				if (temp_servicestatus->state_type == HARD_STATE || tac_cgi_hard_only == FALSE)
					services_unknown_unacknowledged++;
				}
			services_unknown++;
			}

		else if(temp_servicestatus->status == SERVICE_CRITICAL) {
			temp_hoststatus = find_hoststatus(temp_servicestatus->host_name);
			if(temp_hoststatus != NULL && (temp_hoststatus->status == SD_HOST_DOWN || temp_hoststatus->status == SD_HOST_UNREACHABLE)) {
				services_critical_host_problem++;
				problem = FALSE;
				}
			if(temp_servicestatus->scheduled_downtime_depth > 0) {
				services_critical_scheduled++;
				problem = FALSE;
				}
			if(temp_servicestatus->problem_has_been_acknowledged == TRUE) {
				services_critical_acknowledged++;
				problem = FALSE;
				}
			if(temp_servicestatus->checks_enabled == FALSE) {
				services_critical_disabled++;
				problem = FALSE;
				}
			if(problem == TRUE) {
				if (temp_servicestatus->state_type == HARD_STATE || tac_cgi_hard_only == FALSE)
					services_critical_unacknowledged++;
				}
			services_critical++;
			}

		else if(temp_servicestatus->status == SERVICE_PENDING) {
			if(temp_servicestatus->checks_enabled == FALSE)
				services_pending_disabled++;
			services_pending++;
			}


		/* get health stats */
		if(temp_servicestatus->status == SERVICE_OK)
			total_service_health += 2;

		else if(temp_servicestatus->status == SERVICE_WARNING || temp_servicestatus->status == SERVICE_UNKNOWN)
			total_service_health++;

		if(temp_servicestatus->status != SERVICE_PENDING)
			potential_service_health += 2;


		/* calculate execution time and latency stats */
		if(temp_servicestatus->check_type == CHECK_TYPE_ACTIVE) {

			total_active_service_checks++;

			if(min_service_latency == -1.0 || temp_servicestatus->latency < min_service_latency)
				min_service_latency = temp_servicestatus->latency;
			if(max_service_latency == -1.0 || temp_servicestatus->latency > max_service_latency)
				max_service_latency = temp_servicestatus->latency;

			if(min_service_execution_time == -1.0 || temp_servicestatus->execution_time < min_service_execution_time)
				min_service_execution_time = temp_servicestatus->execution_time;
			if(max_service_execution_time == -1.0 || temp_servicestatus->execution_time > max_service_execution_time)
				max_service_execution_time = temp_servicestatus->execution_time;

			total_service_latency += temp_servicestatus->latency;
			total_service_execution_time += temp_servicestatus->execution_time;
			}
		else
			total_passive_service_checks++;


		total_services++;
		}



	/* check all hosts */
	for(temp_hoststatus = hoststatus_list; temp_hoststatus != NULL; temp_hoststatus = temp_hoststatus->next) {

		/* see if user is authorized to view this host */
		temp_host = find_host(temp_hoststatus->host_name);
		if(is_authorized_for_host(temp_host, &current_authdata) == FALSE)
			continue;

		/******** CHECK FEATURES *******/

		/* check flapping */
		if(temp_hoststatus->flap_detection_enabled == FALSE)
			flap_disabled_hosts++;
		else if(temp_hoststatus->is_flapping == TRUE)
			flapping_hosts++;

		/* check notifications */
		if(temp_hoststatus->notifications_enabled == FALSE)
			notification_disabled_hosts++;

		/* check event handler */
		if(temp_hoststatus->event_handler_enabled == FALSE)
			event_handler_disabled_hosts++;

		/* active check execution */
		if(temp_hoststatus->checks_enabled == FALSE)
			active_checks_disabled_hosts++;

		/* passive check acceptance */
		if(temp_hoststatus->accept_passive_checks == FALSE)
			passive_checks_disabled_hosts++;


		/********* CHECK STATUS ********/

		problem = TRUE;

		if(temp_hoststatus->status == SD_HOST_UP) {
			if(temp_hoststatus->checks_enabled == FALSE)
				hosts_up_disabled++;
			else
				hosts_up_unacknowledged++;
			hosts_up++;
			}

		else if(temp_hoststatus->status == SD_HOST_DOWN) {
			if(temp_hoststatus->scheduled_downtime_depth > 0) {
				hosts_down_scheduled++;
				problem = FALSE;
				}
			if(temp_hoststatus->problem_has_been_acknowledged == TRUE) {
				hosts_down_acknowledged++;
				problem = FALSE;
				}
			if(temp_hoststatus->checks_enabled == FALSE) {
				hosts_down_disabled++;
				problem = FALSE;
				}
			if(problem == TRUE) {
				if (temp_hoststatus->state_type == HARD_STATE || tac_cgi_hard_only == FALSE)
					hosts_down_unacknowledged++;
				}
			hosts_down++;
			}

		else if(temp_hoststatus->status == SD_HOST_UNREACHABLE) {
			if(temp_hoststatus->scheduled_downtime_depth > 0) {
				hosts_unreachable_scheduled++;
				problem = FALSE;
				}
			if(temp_hoststatus->problem_has_been_acknowledged == TRUE) {
				hosts_unreachable_acknowledged++;
				problem = FALSE;
				}
			if(temp_hoststatus->checks_enabled == FALSE) {
				hosts_unreachable_disabled++;
				problem = FALSE;
				}
			if(problem == TRUE) {
				if (temp_hoststatus->state_type == HARD_STATE || tac_cgi_hard_only == FALSE)
					hosts_unreachable_unacknowledged++;
			}
			hosts_unreachable++;
			}

		else if(temp_hoststatus->status == HOST_PENDING) {
			if(temp_hoststatus->checks_enabled == FALSE)
				hosts_pending_disabled++;
			hosts_pending++;
			}

		/* get health stats */
		if(temp_hoststatus->status == SD_HOST_UP)
			total_host_health++;

		if(temp_hoststatus->status != HOST_PENDING)
			potential_host_health++;

		/* check type stats */
		if(temp_hoststatus->check_type == CHECK_TYPE_ACTIVE) {

			total_active_host_checks++;

			if(min_host_latency == -1.0 || temp_hoststatus->latency < min_host_latency)
				min_host_latency = temp_hoststatus->latency;
			if(max_host_latency == -1.0 || temp_hoststatus->latency > max_host_latency)
				max_host_latency = temp_hoststatus->latency;

			if(min_host_execution_time == -1.0 || temp_hoststatus->execution_time < min_host_execution_time)
				min_host_execution_time = temp_hoststatus->execution_time;
			if(max_host_execution_time == -1.0 || temp_hoststatus->execution_time > max_host_execution_time)
				max_host_execution_time = temp_hoststatus->execution_time;

			total_host_latency += temp_hoststatus->latency;
			total_host_execution_time += temp_hoststatus->execution_time;
			}
		else
			total_passive_host_checks++;

		total_hosts++;
		}


	/* calculate service health */
	if(potential_service_health == 0)
		percent_service_health = 0.0;
	else
		percent_service_health = ((double)total_service_health / (double)potential_service_health) * 100.0;

	/* calculate host health */
	if(potential_host_health == 0)
		percent_host_health = 0.0;
	else
		percent_host_health = ((double)total_host_health / (double)potential_host_health) * 100.0;

	/* calculate service latency */
	if(total_service_latency == 0L)
		average_service_latency = 0.0;
	else
		average_service_latency = ((double)total_service_latency / (double)total_active_service_checks);

	/* calculate host latency */
	if(total_host_latency == 0L)
		average_host_latency = 0.0;
	else
		average_host_latency = ((double)total_host_latency / (double)total_active_host_checks);

	/* calculate service execution time */
	if(total_service_execution_time == 0.0)
		average_service_execution_time = 0.0;
	else
		average_service_execution_time = ((double)total_service_execution_time / (double)total_active_service_checks);

	/* calculate host execution time */
	if(total_host_execution_time == 0.0)
		average_host_execution_time = 0.0;
	else
		average_host_execution_time = ((double)total_host_execution_time / (double)total_active_host_checks);

	return;
	}




/* determine what hosts are causing network outages */
void find_hosts_causing_outages(void) {
	hoststatus *temp_hoststatus;
	hostoutage *temp_hostoutage;
	host *temp_host;

	/* user must be authorized for all hosts in order to see outages */
	if(is_authorized_for_all_hosts(&current_authdata) == FALSE)
		return;

	/* check all hosts */
	for(temp_hoststatus = hoststatus_list; temp_hoststatus != NULL; temp_hoststatus = temp_hoststatus->next) {

		/* check only hosts that are not up and not pending */
		if(temp_hoststatus->status != SD_HOST_UP && temp_hoststatus->status != HOST_PENDING) {

			/* find the host entry */
			temp_host = find_host(temp_hoststatus->host_name);

			if(temp_host == NULL)
				continue;

			/* if the route to this host is not blocked, it is a causing an outage */
			if(is_route_to_host_blocked(temp_host) == FALSE)
				add_hostoutage(temp_host);
			}
		}


	/* check all hosts that are causing problems and calculate the extent of the problem */
	for(temp_hostoutage = hostoutage_list; temp_hostoutage != NULL; temp_hostoutage = temp_hostoutage->next) {

		/* calculate the outage effect of this particular hosts */
		calculate_outage_effect_of_host(temp_hostoutage->hst, &temp_hostoutage->affected_child_hosts);

		if(temp_hostoutage->affected_child_hosts > 1)
			total_blocking_outages++;
		else
			total_nonblocking_outages++;
		}

	return;
	}





/* adds a host outage entry */
void add_hostoutage(host *hst) {
	hostoutage *new_hostoutage;

	/* allocate memory for a new structure */
	new_hostoutage = (hostoutage *)malloc(sizeof(hostoutage));

	if(new_hostoutage == NULL)
		return;

	new_hostoutage->hst = hst;
	new_hostoutage->affected_child_hosts = 0;

	/* add the structure to the head of the list in memory */
	new_hostoutage->next = hostoutage_list;
	hostoutage_list = new_hostoutage;

	return;
	}




/* frees all memory allocated to the host outage list */
void free_hostoutage_list(void) {
	hostoutage *this_hostoutage;
	hostoutage *next_hostoutage;

	for(this_hostoutage = hostoutage_list; this_hostoutage != NULL; this_hostoutage = next_hostoutage) {
		next_hostoutage = this_hostoutage->next;
		free(this_hostoutage);
		}

	return;
	}



/* calculates network outage effect of a particular host being down or unreachable */
void calculate_outage_effect_of_host(host *hst, int *affected_hosts) {
	int total_child_hosts_affected = 0;
	int temp_child_hosts_affected = 0;
	host *temp_host;


	/* find all child hosts of this host */
	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		/* skip this host if it is not a child */
		if(is_host_immediate_child_of_host(hst, temp_host) == FALSE)
			continue;

		/* calculate the outage effect of the child */
		calculate_outage_effect_of_host(temp_host, &temp_child_hosts_affected);

		/* keep a running total of outage effects */
		total_child_hosts_affected += temp_child_hosts_affected;
		}

	*affected_hosts = total_child_hosts_affected + 1;

	return;
	}



/* tests whether or not a host is "blocked" by upstream parents (host is already assumed to be down or unreachable) */
int is_route_to_host_blocked(host *hst) {
	hostsmember *temp_hostsmember;
	hoststatus *temp_hoststatus;

	/* if the host has no parents, it is not being blocked by anyone */
	if(hst->parent_hosts == NULL)
		return FALSE;

	/* check all parent hosts */
	for(temp_hostsmember = hst->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {

		/* find the parent host's status */
		temp_hoststatus = find_hoststatus(temp_hostsmember->host_name);

		if(temp_hoststatus == NULL)
			continue;

		/* at least one parent it up (or pending), so this host is not blocked */
		if(temp_hoststatus->status == SD_HOST_UP || temp_hoststatus->status == HOST_PENDING)
			return FALSE;
		}

	return TRUE;
	}






void display_tac_overview(void) {
	char host_health_image[16];
	char service_health_image[16];


	printf("<p align=left>\n");

	printf("<table border=0 align=left width=100%% cellspacing=4 cellpadding=0>\n");
	printf("<tr>\n");

	/* left column */
	printf("<td align=left valign=top width=50%%>\n");

	display_info_table("Tactical Monitoring Overview", TRUE, &current_authdata);

	printf("</td>\n");


	/* right column */
	printf("<td align=right valign=bottom width=50%%>\n");

	printf("<table border=0 cellspacing=0 cellpadding=0>\n");

	printf("<tr>\n");

	printf("<td valign=bottom align=right>\n");

	/* display context-sensitive help */
	display_context_help(CONTEXTHELP_TAC);

	printf("</td>\n");

	printf("<td>\n");

	printf("<table border=0 cellspacing=4 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td class='perfTitle'>&nbsp;<a href='%s?type=%d' class='perfTitle'>Monitoring Performance</a></td>\n", EXTINFO_CGI, DISPLAY_PERFORMANCE);
	printf("</tr>\n");

	printf("<tr>\n");
	printf("<td>\n");

	printf("<table border=0 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td class='perfBox'>\n");
	printf("<table border=0 cellspacing=4 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td align=left valign=center class='perfItem'><a href='%s?type=%d' class='perfItem'>Service Check Execution Time:</a></td>", EXTINFO_CGI, DISPLAY_PERFORMANCE);
	printf("<td valign=top class='perfValue' nowrap><a href='%s?type=%d' class='perfValue'>%.2f / %.2f / %.3f sec</a></td>\n", EXTINFO_CGI, DISPLAY_PERFORMANCE, min_service_execution_time, max_service_execution_time, average_service_execution_time);
	printf("</tr>\n");
	printf("<tr>\n");
	printf("<td align=left valign=center class='perfItem'><a href='%s?type=%d' class='perfItem'>Service Check Latency:</a></td>", EXTINFO_CGI, DISPLAY_PERFORMANCE);
	printf("<td valign=top class='perfValue' nowrap><a href='%s?type=%d' class='perfValue'>%.2f / %.2f / %.3f sec</a></td>\n", EXTINFO_CGI, DISPLAY_PERFORMANCE, min_service_latency, max_service_latency, average_service_latency);
	printf("</tr>\n");
	printf("<tr>\n");
	printf("<td align=left valign=center class='perfItem'><a href='%s?type=%d' class='perfItem'>Host Check Execution Time:</a></td>", EXTINFO_CGI, DISPLAY_PERFORMANCE);
	printf("<td valign=top class='perfValue' nowrap><a href='%s?type=%d' class='perfValue'>%.2f / %.2f / %.3f sec</a></td>\n", EXTINFO_CGI, DISPLAY_PERFORMANCE, min_host_execution_time, max_host_execution_time, average_host_execution_time);
	printf("</tr>\n");
	printf("<tr>\n");
	printf("<td align=left valign=center class='perfItem'><a href='%s?type=%d' class='perfItem'>Host Check Latency:</a></td>", EXTINFO_CGI, DISPLAY_PERFORMANCE);
	printf("<td valign=top class='perfValue' nowrap><a href='%s?type=%d' class='perfValue'>%.2f / %.2f / %2.3f sec</a></td>\n", EXTINFO_CGI, DISPLAY_PERFORMANCE, min_host_latency, max_host_latency, average_host_latency);
	printf("</tr>\n");
	printf("<tr>\n");
	printf("<td align=left valign=center class='perfItem'><a href='%s?host=all&serviceprops=%d' class='perfItem'># Active Host / Service Checks:</a></td>", STATUS_CGI, SERVICE_ACTIVE_CHECK);
	printf("<td valign=top class='perfValue' nowrap><a href='%s?hostgroup=all&hostprops=%d&style=hostdetail' class='perfValue'>%d</a> / <a href='%s?host=all&serviceprops=%d' class='perfValue'>%d</a></td>\n", STATUS_CGI, HOST_ACTIVE_CHECK, total_active_host_checks, STATUS_CGI, SERVICE_ACTIVE_CHECK, total_active_service_checks);
	printf("</tr>\n");
	printf("<tr>\n");
	printf("<td align=left valign=center class='perfItem'><a href='%s?host=all&serviceprops=%d' class='perfItem'># Passive Host / Service Checks:</a></td>", STATUS_CGI, SERVICE_PASSIVE_CHECK);
	printf("<td valign=top class='perfValue' nowrap><a href='%s?hostgroup=all&hostprops=%d&style=hostdetail' class='perfValue'>%d</a> / <a href='%s?host=all&serviceprops=%d' class='perfValue'>%d</a></td>\n", STATUS_CGI, HOST_PASSIVE_CHECK, total_passive_host_checks, STATUS_CGI, SERVICE_PASSIVE_CHECK, total_passive_service_checks);
	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");

	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");

	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");

	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");
	printf("</p>\n");

	printf("<br clear=all>\n");
	printf("<br>\n");




	printf("<table border=0 cellspacing=0 cellpadding=0 width=100%%>\n");
	printf("<tr>\n");
	printf("<td valign=top align=left width=50%%>\n");


	/******* OUTAGES ********/

	printf("<p>\n");

	printf("<table class='tac' width=125 cellspacing=4 cellpadding=0 border=0>\n");

	printf("<tr><td colspan=1 height=20 class='outageTitle'>&nbsp;Network Outages</td></tr>\n");

	printf("<tr>\n");
	printf("<td class='outageHeader' width=125><a href='%s' class='outageHeader'>", OUTAGES_CGI);
	if(is_authorized_for_all_hosts(&current_authdata) == FALSE)
		printf("N/A");
	else
		printf("%d Outages", total_blocking_outages);
	printf("</a></td>\n");
	printf("</tr>\n");

	printf("<tr>\n");

	printf("<td valign=top>\n");
	printf("<table border=0 width=125 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=bottom width=25>&nbsp;&nbsp;&nbsp;</td>\n");
	printf("<Td width=10>&nbsp;</td>\n");

	printf("<Td valign=top width=100%%>\n");
	printf("<table border=0 width=100%%>\n");

	if(total_blocking_outages > 0)
		printf("<tr><td width=100%% class='outageImportantProblem'><a href='%s'>%d Blocking Outages</a></td></tr>\n", OUTAGES_CGI, total_blocking_outages);

	/*
	if(total_nonblocking_outages>0)
		printf("<tr><td width=100%% class='outageUnimportantProblem'><a href='%s'>%d Nonblocking Outages</a></td></tr>\n",OUTAGES_CGI,total_nonblocking_outages);
	*/

	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");

	printf("</p>\n");

	printf("</td>\n");



	/* right column */
	printf("<td valign=top align=right width=50%%>\n");

	if(percent_host_health < HEALTH_CRITICAL_PERCENTAGE)
		strncpy(host_health_image, THERM_CRITICAL_IMAGE, sizeof(host_health_image));
	else if(percent_host_health < HEALTH_WARNING_PERCENTAGE)
		strncpy(host_health_image, THERM_WARNING_IMAGE, sizeof(host_health_image));
	else
		strncpy(host_health_image, THERM_OK_IMAGE, sizeof(host_health_image));
	host_health_image[sizeof(host_health_image) - 1] = '\x0';

	if(percent_service_health < HEALTH_CRITICAL_PERCENTAGE)
		strncpy(service_health_image, THERM_CRITICAL_IMAGE, sizeof(service_health_image));
	else if(percent_service_health < HEALTH_WARNING_PERCENTAGE)
		strncpy(service_health_image, THERM_WARNING_IMAGE, sizeof(service_health_image));
	else
		strncpy(service_health_image, THERM_OK_IMAGE, sizeof(service_health_image));
	service_health_image[sizeof(service_health_image) - 1] = '\x0';

	printf("<table border=0 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td>\n");

	printf("<table border=0 cellspacing=4 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td class='healthTitle'>&nbsp;Network Health</td>\n");
	printf("</tr>\n");

	printf("<tr>\n");
	printf("<td>\n");

	printf("<table border=0 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td class='healthBox'>\n");
	printf("<table border=0 cellspacing=4 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td align=left valign=center class='healthItem'>Host Health:</td>");
	printf("<td valign=top width=100 class='healthBar'><img src='%s%s' border=0 width=%d height=20 alt='%2.1f%% Health' title='%2.1f%% Health'></td>\n", url_images_path, host_health_image, (percent_host_health < 5.0) ? 5 : (int)percent_host_health, percent_host_health, percent_host_health);
	printf("</tr>\n");
	printf("<tr>\n");
	printf("<td align=left valign=center class='healthItem'>Service Health:</td>");
	printf("<td valign=top width=100 class='healthBar'><img src='%s%s' border=0 width=%d height=20 alt='%2.1f%% Health' title='%2.1f%% Health'></td>\n", url_images_path, service_health_image, (percent_service_health < 5.0) ? 5 : (int)percent_service_health, percent_service_health, percent_service_health);
	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");

	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");

	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");

	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");






	/******* HOSTS ********/

	printf("<p>\n");

	printf("<table class='tac' width=516 cellspacing=4 cellpadding=0 border=0>\n");

	printf("<tr><td colspan=4 height=20 class='hostTitle'>&nbsp;Hosts</td></tr>\n");

	printf("<tr>\n");
	printf("<td class='hostHeader' width=125><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d' class='hostHeader'>%d Down</a></td>\n", STATUS_CGI, SD_HOST_DOWN, hosts_down);
	printf("<td class='hostHeader' width=125><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d' class='hostHeader'>%d Unreachable</a></td>\n", STATUS_CGI, SD_HOST_UNREACHABLE, hosts_unreachable);
	printf("<td class='hostHeader' width=125><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d' class='hostHeader'>%d Up</a></td>\n", STATUS_CGI, SD_HOST_UP, hosts_up);
	printf("<td class='hostHeader' width=125><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d' class='hostHeader'>%d Pending</a></td>\n", STATUS_CGI, HOST_PENDING, hosts_pending);
	printf("</tr>\n");

	printf("<tr>\n");


	printf("<td valign=top>\n");
	printf("<table border=0 width=125 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=bottom width=25>&nbsp;&nbsp;&nbsp;</td>\n");
	printf("<Td width=10>&nbsp;</td>\n");

	printf("<Td valign=top width=100%%>\n");
	printf("<table border=0 width=100%%>\n");

	if(hosts_down_unacknowledged > 0)
		printf("<tr><td width=100%% class='hostImportantProblem'><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d&hostprops=%d'>%d Unhandled Problems</a></td></tr>\n", STATUS_CGI, SD_HOST_DOWN, HOST_NO_SCHEDULED_DOWNTIME | HOST_STATE_UNACKNOWLEDGED | HOST_CHECKS_ENABLED, hosts_down_unacknowledged);

	if(hosts_down_scheduled > 0)
		printf("<tr><td width=100%% class='hostUnimportantProblem'><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d&hostprops=%d'>%d Scheduled</a></td></tr>\n", STATUS_CGI, SD_HOST_DOWN, HOST_SCHEDULED_DOWNTIME, hosts_down_scheduled);

	if(hosts_down_acknowledged > 0)
		printf("<tr><td width=100%% class='hostUnimportantProblem'><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d&hostprops=%d'>%d Acknowledged</a></td></tr>\n", STATUS_CGI, SD_HOST_DOWN, HOST_STATE_ACKNOWLEDGED, hosts_down_acknowledged);

	if(hosts_down_disabled > 0)
		printf("<tr><td width=100%% class='hostUnimportantProblem'><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d&hostprops=%d'>%d Disabled</a></td></tr>\n", STATUS_CGI, SD_HOST_DOWN, HOST_CHECKS_DISABLED, hosts_down_disabled);

	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");




	printf("<td valign=top>\n");
	printf("<table border=0 width=125 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=bottom width=25>&nbsp;</td>\n");
	printf("<Td width=10>&nbsp;</td>\n");

	printf("<Td valign=top width=100%%>\n");
	printf("<table border=0 width=100%%>\n");

	if(hosts_unreachable_unacknowledged > 0)
		printf("<tr><td width=100%% class='hostImportantProblem'><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d&hostprops=%d'>%d Unhandled Problems</a></td></tr>\n", STATUS_CGI, SD_HOST_UNREACHABLE, HOST_NO_SCHEDULED_DOWNTIME | HOST_STATE_UNACKNOWLEDGED | HOST_CHECKS_ENABLED, hosts_unreachable_unacknowledged);

	if(hosts_unreachable_scheduled > 0)
		printf("<tr><td width=100%% class='hostUnimportantProblem'><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d&hostprops=%d'>%d Scheduled</a></td></tr>\n", STATUS_CGI, SD_HOST_UNREACHABLE, HOST_SCHEDULED_DOWNTIME, hosts_unreachable_scheduled);

	if(hosts_unreachable_acknowledged > 0)
		printf("<tr><td width=100%% class='hostUnimportantProblem'><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d&hostprops=%d'>%d Acknowledged</a></td></tr>\n", STATUS_CGI, SD_HOST_UNREACHABLE, HOST_STATE_ACKNOWLEDGED, hosts_unreachable_acknowledged);

	if(hosts_unreachable_disabled > 0)
		printf("<tr><td width=100%% class='hostUnimportantProblem'><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d&hostprops=%d'>%d Disabled</a></td></tr>\n", STATUS_CGI, SD_HOST_UNREACHABLE, HOST_CHECKS_DISABLED, hosts_unreachable_disabled);

	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");




	printf("<td valign=top>\n");
	printf("<table border=0 width=125 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=bottom width=25>&nbsp;</td>\n");
	printf("<Td width=10>&nbsp;</td>\n");

	printf("<Td valign=top width=100%%>\n");
	printf("<table border=0 width=100%%>\n");

	if(hosts_up_disabled > 0)
		printf("<tr><td width=100%% class='hostUnimportantProblem'><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d&hostprops=%d'>%d Disabled</a></td></tr>\n", STATUS_CGI, SD_HOST_UP, HOST_CHECKS_DISABLED, hosts_up_disabled);

	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");




	printf("<td valign=top>\n");
	printf("<table border=0 width=125 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=bottom width=25>&nbsp;</td>\n");
	printf("<Td width=10>&nbsp;</td>\n");

	printf("<Td valign=top width=100%%>\n");
	printf("<table border=0 width=100%%>\n");

	if(hosts_pending_disabled > 0)
		printf("<tr><td width=100%% class='hostUnimportantProblem'><a href='%s?hostgroup=all&style=hostdetail&hoststatustypes=%d&hostprops=%d'>%d Disabled</a></td></tr>\n", STATUS_CGI, HOST_PENDING, HOST_CHECKS_DISABLED, hosts_pending_disabled);

	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");




	printf("</tr>\n");
	printf("</table>\n");

	/*
	printf("</tr>\n");
	printf("</table>\n");
	*/

	printf("</p>\n");




	/*printf("<br clear=all>\n");*/




	/******* SERVICES ********/

	printf("<p>\n");

	printf("<table class='tac' width=641 cellspacing=4 cellpadding=0 border=0>\n");

	printf("<tr><td colspan=5 height=20 class='serviceTitle'>&nbsp;Services</td></tr>\n");

	printf("<tr>\n");
	printf("<td class='serviceHeader' width=125><a href='%s?host=all&style=detail&servicestatustypes=%d' class='serviceHeader'>%d Critical</a></td>\n", STATUS_CGI, SERVICE_CRITICAL, services_critical);
	printf("<td class='serviceHeader' width=125><a href='%s?host=all&style=detail&servicestatustypes=%d' class='serviceHeader'>%d Warning</a></td>\n", STATUS_CGI, SERVICE_WARNING, services_warning);
	printf("<td class='serviceHeader' width=125><a href='%s?host=all&style=detail&servicestatustypes=%d' class='serviceHeader'>%d Unknown</a></td>\n", STATUS_CGI, SERVICE_UNKNOWN, services_unknown);
	printf("<td class='serviceHeader' width=125><a href='%s?host=all&style=detail&servicestatustypes=%d' class='serviceHeader'>%d Ok</a></td>\n", STATUS_CGI, SERVICE_OK, services_ok);
	printf("<td class='serviceHeader' width=125><a href='%s?host=all&style=detail&servicestatustypes=%d' class='serviceHeader'>%d Pending</a></td>\n", STATUS_CGI, SERVICE_PENDING, services_pending);
	printf("</tr>\n");

	printf("<tr>\n");


	printf("<td valign=top>\n");
	printf("<table border=0 width=125 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=bottom width=25>&nbsp;&nbsp;&nbsp;</td>\n");
	printf("<Td width=10>&nbsp;</td>\n");

	printf("<Td valign=top width=100%%>\n");
	printf("<table border=0 width=100%%>\n");

	if(services_critical_unacknowledged > 0)
		printf("<tr><td width=100%% class='serviceImportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%d'>%d Unhandled Problems</a></td></tr>\n", STATUS_CGI, SERVICE_CRITICAL, SD_HOST_UP | HOST_PENDING, SERVICE_NO_SCHEDULED_DOWNTIME | SERVICE_STATE_UNACKNOWLEDGED | SERVICE_CHECKS_ENABLED, services_critical_unacknowledged);

	if(services_critical_host_problem > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&hoststatustypes=%d'>%d on Problem Hosts</a></td></tr>\n", STATUS_CGI, SERVICE_CRITICAL, SD_HOST_DOWN | SD_HOST_UNREACHABLE, services_critical_host_problem);

	if(services_critical_scheduled > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&serviceprops=%d'>%d Scheduled</a></td></tr>\n", STATUS_CGI, SERVICE_CRITICAL, SERVICE_SCHEDULED_DOWNTIME, services_critical_scheduled);

	if(services_critical_acknowledged > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&serviceprops=%d'>%d Acknowledged</a></td></tr>\n", STATUS_CGI, SERVICE_CRITICAL, SERVICE_STATE_ACKNOWLEDGED, services_critical_acknowledged);

	if(services_critical_disabled > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&serviceprops=%d'>%d Disabled</a></td></tr>\n", STATUS_CGI, SERVICE_CRITICAL, SERVICE_CHECKS_DISABLED, services_critical_disabled);

	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");





	printf("<td valign=top>\n");
	printf("<table border=0 width=125 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=bottom width=25>&nbsp;</td>\n");
	printf("<Td width=10>&nbsp;</td>\n");

	printf("<Td valign=top width=100%%>\n");
	printf("<table border=0 width=100%%>\n");

	if(services_warning_unacknowledged > 0)
		printf("<tr><td width=100%% class='serviceImportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%d'>%d Unhandled Problems</a></td></tr>\n", STATUS_CGI, SERVICE_WARNING, SD_HOST_UP | HOST_PENDING, SERVICE_NO_SCHEDULED_DOWNTIME | SERVICE_STATE_UNACKNOWLEDGED | SERVICE_CHECKS_ENABLED, services_warning_unacknowledged);

	if(services_warning_host_problem > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&hoststatustypes=%d'>%d on Problem Hosts</a></td></tr>\n", STATUS_CGI, SERVICE_WARNING, SD_HOST_DOWN | SD_HOST_UNREACHABLE, services_warning_host_problem);

	if(services_warning_scheduled > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&serviceprops=%d'>%d Scheduled</a></td></tr>\n", STATUS_CGI, SERVICE_WARNING, SERVICE_SCHEDULED_DOWNTIME, services_warning_scheduled);

	if(services_warning_acknowledged > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&serviceprops=%d'>%d Acknowledged</a></td></tr>\n", STATUS_CGI, SERVICE_WARNING, SERVICE_STATE_ACKNOWLEDGED, services_warning_acknowledged);

	if(services_warning_disabled > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&serviceprops=%d'>%d Disabled</a></td></tr>\n", STATUS_CGI, SERVICE_WARNING, SERVICE_CHECKS_DISABLED, services_warning_disabled);

	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");





	printf("<td valign=top>\n");
	printf("<table border=0 width=125 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=bottom width=25>&nbsp;</td>\n");
	printf("<Td width=10>&nbsp;</td>\n");

	printf("<Td valign=top width=100%%>\n");
	printf("<table border=0 width=100%%>\n");

	if(services_unknown_unacknowledged > 0)
		printf("<tr><td width=100%% class='serviceImportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%d'>%d Unhandled Problems</a></td></tr>\n", STATUS_CGI, SERVICE_UNKNOWN, SD_HOST_UP | HOST_PENDING, SERVICE_NO_SCHEDULED_DOWNTIME | SERVICE_STATE_UNACKNOWLEDGED | SERVICE_CHECKS_ENABLED, services_unknown_unacknowledged);

	if(services_unknown_host_problem > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&hoststatustypes=%d'>%d on Problem Hosts</a></td></tr>\n", STATUS_CGI, SERVICE_UNKNOWN, SD_HOST_DOWN | SD_HOST_UNREACHABLE, services_unknown_host_problem);

	if(services_unknown_scheduled > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&serviceprops=%d'>%d Scheduled</a></td></tr>\n", STATUS_CGI, SERVICE_UNKNOWN, SERVICE_SCHEDULED_DOWNTIME, services_unknown_scheduled);

	if(services_unknown_acknowledged > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&serviceprops=%d'>%d Acknowledged</a></td></tr>\n", STATUS_CGI, SERVICE_UNKNOWN, SERVICE_STATE_ACKNOWLEDGED, services_unknown_acknowledged);

	if(services_unknown_disabled > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&serviceprops=%d'>%d Disabled</a></td></tr>\n", STATUS_CGI, SERVICE_UNKNOWN, SERVICE_CHECKS_DISABLED, services_unknown_disabled);

	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");




	printf("<td valign=top>\n");
	printf("<table border=0 width=125 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=bottom width=25>&nbsp;</td>\n");
	printf("<Td width=10>&nbsp;</td>\n");

	printf("<Td valign=top width=100%%>\n");
	printf("<table border=0 width=100%%>\n");

	if(services_ok_disabled > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&serviceprops=%d'>%d Disabled</a></td></tr>\n", STATUS_CGI, SERVICE_OK, SERVICE_CHECKS_DISABLED, services_ok_disabled);

	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");



	printf("<td valign=top>\n");
	printf("<table border=0 width=125 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=bottom width=25>&nbsp;</td>\n");
	printf("<Td width=10>&nbsp;</td>\n");

	printf("<td valign=top width=100%%>\n");
	printf("<table border=0 width=100%%>\n");

	if(services_pending_disabled > 0)
		printf("<tr><td width=100%% class='serviceUnimportantProblem'><a href='%s?host=all&type=detail&servicestatustypes=%d&serviceprops=%d'>%d Disabled</a></td></tr>\n", STATUS_CGI, SERVICE_PENDING, SERVICE_CHECKS_DISABLED, services_pending_disabled);

	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");



	printf("</tr>\n");
	printf("</table>\n");

	printf("</p>\n");




	/*printf("<br clear=all>\n");*/





	/******* MONITORING FEATURES ********/

	printf("<p>\n");

	printf("<table class='tac' cellspacing=4 cellpadding=0 border=0>\n");

	printf("<tr><td colspan=5 height=20 class='featureTitle'>&nbsp;Monitoring Features</td></tr>\n");

	printf("<tr>\n");
	printf("<td class='featureHeader' width=135>Flap Detection</td>\n");
	printf("<td class='featureHeader' width=135>Notifications</td>\n");
	printf("<td class='featureHeader' width=135>Event Handlers</td>\n");
	printf("<td class='featureHeader' width=135>Active Checks</td>\n");
	printf("<td class='featureHeader' width=135>Passive Checks</td>\n");
	printf("</tr>\n");

	printf("<tr>\n");

	printf("<td valign=top>\n");
	printf("<table border=0 width=135 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=top><a href='%s?cmd_typ=%d'><img src='%s%s' border=0 alt='Flap Detection %s' title='Flap Detection %s'></a></td>\n", COMMAND_CGI, (enable_flap_detection == TRUE) ? CMD_DISABLE_FLAP_DETECTION : CMD_ENABLE_FLAP_DETECTION, url_images_path, (enable_flap_detection == TRUE) ? TAC_ENABLED_ICON : TAC_DISABLED_ICON, (enable_flap_detection == TRUE) ? "Enabled" : "Disabled", (enable_flap_detection == TRUE) ? "Enabled" : "Disabled");
	printf("<Td width=10>&nbsp;</td>\n");
	if(enable_flap_detection == TRUE) {
		printf("<Td valign=top width=100%% class='featureEnabledFlapDetection'>\n");
		printf("<table border=0 width=100%%>\n");

		if(flap_disabled_services > 0)
			printf("<tr><td width=100%% class='featureItemDisabledServiceFlapDetection'><a href='%s?host=all&type=detail&serviceprops=%d'>%d Service%s Disabled</a></td></tr>\n", STATUS_CGI, SERVICE_FLAP_DETECTION_DISABLED, flap_disabled_services, (flap_disabled_services == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemEnabledServiceFlapDetection'>All Services Enabled</td></tr>\n");

		if(flapping_services > 0)
			printf("<tr><td width=100%% class='featureItemServicesFlapping'><a href='%s?host=all&type=detail&serviceprops=%d'>%d Service%s Flapping</a></td></tr>\n", STATUS_CGI, SERVICE_IS_FLAPPING, flapping_services, (flapping_services == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemServicesNotFlapping'>No Services Flapping</td></tr>\n");

		if(flap_disabled_hosts > 0)
			printf("<tr><td width=100%% class='featureItemDisabledHostFlapDetection'><a href='%s?hostgroup=all&style=hostdetail&hostprops=%d'>%d Host%s Disabled</a></td></tr>\n", STATUS_CGI, HOST_FLAP_DETECTION_DISABLED, flap_disabled_hosts, (flap_disabled_hosts == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemEnabledHostFlapDetection'>All Hosts Enabled</td></tr>\n");

		if(flapping_hosts > 0)
			printf("<tr><td width=100%% class='featureItemHostsFlapping'><a href='%s?hostgroup=all&style=hostdetail&hostprops=%d'>%d Host%s Flapping</a></td></tr>\n", STATUS_CGI, HOST_IS_FLAPPING, flapping_hosts, (flapping_hosts == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemHostsNotFlapping'>No Hosts Flapping</td></tr>\n");

		printf("</table>\n");
		printf("</td>\n");
		}
	else
		printf("<Td valign=center width=100%% class='featureDisabledFlapDetection'>N/A</td>\n");
	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");




	printf("<td valign=top>\n");
	printf("<table border=0 width=135 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=top><a href='%s?cmd_typ=%d'><img src='%s%s' border=0 alt='Notifications %s' title='Notifications %s'></a></td>\n", COMMAND_CGI, (enable_notifications == TRUE) ? CMD_DISABLE_NOTIFICATIONS : CMD_ENABLE_NOTIFICATIONS, url_images_path, (enable_notifications == TRUE) ? TAC_ENABLED_ICON : TAC_DISABLED_ICON, (enable_notifications == TRUE) ? "Enabled" : "Disabled", (enable_notifications == TRUE) ? "Enabled" : "Disabled");
	printf("<Td width=10>&nbsp;</td>\n");
	if(enable_notifications == TRUE) {
		printf("<Td valign=top width=100%% class='featureEnabledNotifications'>\n");
		printf("<table border=0 width=100%%>\n");

		if(notification_disabled_services > 0)
			printf("<tr><td width=100%% class='featureItemDisabledServiceNotifications'><a href='%s?host=all&type=detail&serviceprops=%d'>%d Service%s Disabled</a></td></tr>\n", STATUS_CGI, SERVICE_NOTIFICATIONS_DISABLED, notification_disabled_services, (notification_disabled_services == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemEnabledServiceNotifications'>All Services Enabled</td></tr>\n");

		if(notification_disabled_hosts > 0)
			printf("<tr><td width=100%% class='featureItemDisabledHostNotifications'><a href='%s?hostgroup=all&style=hostdetail&hostprops=%d'>%d Host%s Disabled</a></td></tr>\n", STATUS_CGI, HOST_NOTIFICATIONS_DISABLED, notification_disabled_hosts, (notification_disabled_hosts == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemEnabledHostNotifications'>All Hosts Enabled</td></tr>\n");

		printf("</table>\n");
		printf("</td>\n");
		}
	else
		printf("<Td valign=center width=100%% class='featureDisabledNotifications'>N/A</td>\n");
	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");





	printf("<td valign=top>\n");
	printf("<table border=0 width=135 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=top><a href='%s?cmd_typ=%d'><img src='%s%s' border=0 alt='Event Handlers %s' title='Event Handlers %s'></a></td>\n", COMMAND_CGI, (enable_event_handlers == TRUE) ? CMD_DISABLE_EVENT_HANDLERS : CMD_ENABLE_EVENT_HANDLERS, url_images_path, (enable_event_handlers == TRUE) ? TAC_ENABLED_ICON : TAC_DISABLED_ICON, (enable_event_handlers == TRUE) ? "Enabled" : "Disabled", (enable_event_handlers == TRUE) ? "Enabled" : "Disabled");
	printf("<Td width=10>&nbsp;</td>\n");
	if(enable_event_handlers == TRUE) {
		printf("<Td valign=top width=100%% class='featureEnabledHandlers'>\n");
		printf("<table border=0 width=100%%>\n");

		if(event_handler_disabled_services > 0)
			printf("<tr><td width=100%% class='featureItemDisabledServiceHandlers'><a href='%s?host=all&type=detail&serviceprops=%d'>%d Service%s Disabled</a></td></tr>\n", STATUS_CGI, SERVICE_EVENT_HANDLER_DISABLED, event_handler_disabled_services, (event_handler_disabled_services == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemEnabledServiceHandlers'>All Services Enabled</td></tr>\n");

		if(event_handler_disabled_hosts > 0)
			printf("<tr><td width=100%% class='featureItemDisabledHostHandlers'><a href='%s?hostgroup=all&style=hostdetail&hostprops=%d'>%d Host%s Disabled</a></td></tr>\n", STATUS_CGI, HOST_EVENT_HANDLER_DISABLED, event_handler_disabled_hosts, (event_handler_disabled_hosts == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemEnabledHostHandlers'>All Hosts Enabled</td></tr>\n");

		printf("</table>\n");
		printf("</td>\n");
		}
	else
		printf("<Td valign=center width=100%% class='featureDisabledHandlers'>N/A</td>\n");
	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");





	printf("<td valign=top>\n");
	printf("<table border=0 width=135 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=top><a href='%s?type=%d'><img src='%s%s' border='0' alt='Active Checks %s' title='Active Checks %s'></a></td>\n", EXTINFO_CGI, DISPLAY_PROCESS_INFO, url_images_path, (execute_service_checks == TRUE) ? TAC_ENABLED_ICON : TAC_DISABLED_ICON, (execute_service_checks == TRUE) ? "Enabled" : "Disabled", (execute_service_checks == TRUE) ? "Enabled" : "Disabled");
	printf("<Td width=10>&nbsp;</td>\n");
	if(execute_service_checks == TRUE) {
		printf("<Td valign=top width=100%% class='featureEnabledActiveChecks'>\n");
		printf("<table border=0 width=100%%>\n");

		if(active_checks_disabled_services > 0)
			printf("<tr><td width=100%% class='featureItemDisabledActiveServiceChecks'><a href='%s?host=all&type=detail&serviceprops=%d'>%d Service%s Disabled</a></td></tr>\n", STATUS_CGI, SERVICE_CHECKS_DISABLED, active_checks_disabled_services, (active_checks_disabled_services == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemEnabledActiveServiceChecks'>All Services Enabled</td></tr>\n");

		if(active_checks_disabled_hosts > 0)
			printf("<tr><td width=100%% class='featureItemDisabledActiveHostChecks'><a href='%s?hostgroup=all&style=hostdetail&hostprops=%d'>%d Host%s Disabled</a></td></tr>\n", STATUS_CGI, HOST_CHECKS_DISABLED, active_checks_disabled_hosts, (active_checks_disabled_hosts == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemEnabledActiveHostChecks'>All Hosts Enabled</td></tr>\n");

		printf("</table>\n");
		printf("</td>\n");
		}
	else
		printf("<Td valign=center width=100%% class='featureDisabledActiveChecks'>N/A</td>\n");
	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");





	printf("<td valign=top>\n");
	printf("<table border=0 width=135 cellspacing=0 cellpadding=0>\n");
	printf("<tr>\n");
	printf("<td valign=top><a href='%s?type=%d'><img src='%s%s' border='0' alt='Passive Checks %s' title='Passive Checks %s'></a></td>\n", EXTINFO_CGI, DISPLAY_PROCESS_INFO, url_images_path, (accept_passive_service_checks == TRUE) ? TAC_ENABLED_ICON : TAC_DISABLED_ICON, (accept_passive_service_checks == TRUE) ? "Enabled" : "Disabled", (accept_passive_service_checks == TRUE) ? "Enabled" : "Disabled");
	printf("<Td width=10>&nbsp;</td>\n");
	if(accept_passive_service_checks == TRUE) {

		printf("<Td valign=top width=100%% class='featureEnabledPassiveChecks'>\n");
		printf("<table border=0 width=100%%>\n");

		if(passive_checks_disabled_services > 0)
			printf("<tr><td width=100%% class='featureItemDisabledPassiveServiceChecks'><a href='%s?host=all&type=detail&serviceprops=%d'>%d Service%s Disabled</a></td></tr>\n", STATUS_CGI, SERVICE_PASSIVE_CHECKS_DISABLED, passive_checks_disabled_services, (passive_checks_disabled_services == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemEnabledPassiveServiceChecks'>All Services Enabled</td></tr>\n");

		if(passive_checks_disabled_hosts > 0)
			printf("<tr><td width=100%% class='featureItemDisabledPassiveHostChecks'><a href='%s?hostgroup=all&style=hostdetail&hostprops=%d'>%d Host%s Disabled</a></td></tr>\n", STATUS_CGI, HOST_PASSIVE_CHECKS_DISABLED, passive_checks_disabled_hosts, (passive_checks_disabled_hosts == 1) ? "" : "s");
		else
			printf("<tr><td width=100%% class='featureItemEnabledPassiveHostChecks'>All Hosts Enabled</td></tr>\n");

		printf("</table>\n");
		printf("</td>\n");
		}
	else
		printf("<Td valign=center width=100%% class='featureDisabledPassiveChecks'>N/A</td>\n");
	printf("</tr>\n");
	printf("</table>\n");
	printf("</td>\n");

	printf("</tr>\n");

	printf("</table>\n");

	printf("</p>\n");


	return;
	}
