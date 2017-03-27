/**************************************************************************
 *
 * CMD.C -  Nagios Command CGI
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
#include "../include/comments.h"
#include "../include/downtime.h"

#include "../include/cgiutils.h"
#include "../include/cgiauth.h"
#include "../include/getcgi.h"

extern const char *extcmd_get_name(int id);

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_html_path[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char command_file[MAX_FILENAME_LENGTH];

extern char url_stylesheets_path[MAX_FILENAME_LENGTH];

extern int  nagios_process_state;

extern int  use_authentication;

extern int  lock_author_names;

extern int ack_no_sticky;
extern int ack_no_send;

#define MAX_AUTHOR_LENGTH	64
#define MAX_COMMENT_LENGTH	1024

#define HTML_CONTENT   0
#define WML_CONTENT    1


char *host_name = "";
char *hostgroup_name = "";
char *servicegroup_name = "";
char *service_desc = "";
char *comment_author = "";
char *comment_data = "";
char *start_time_string = "";
char *end_time_string = "";
char *cookie_form_id = NULL, *form_id = NULL;

unsigned long comment_id = 0;
unsigned long downtime_id = 0;
int notification_delay = 0;
int schedule_delay = 0;
int persistent_comment = FALSE;
int sticky_ack = FALSE;
int send_notification = FALSE;
int force_check = FALSE;
int plugin_state = STATE_OK;
char plugin_output[MAX_INPUT_BUFFER] = "";
char performance_data[MAX_INPUT_BUFFER] = "";
time_t start_time = 0L;
time_t end_time = 0L;
int affect_host_and_services = FALSE;
int propagate_to_children = FALSE;
int fixed = FALSE;
unsigned long duration = 0L;
unsigned long triggered_by = 0L;
int child_options = 0;
int force_notification = 0;
int broadcast_notification = 0;

int command_type = CMD_NONE;
int command_mode = CMDMODE_REQUEST;

int content_type = HTML_CONTENT;

int display_header = TRUE;

authdata current_authdata;

void show_command_help(int);
void request_command_data(int);
void commit_command_data(int);
int commit_command(int);
int write_command_to_file(char *);
void clean_comment_data(char *);

void document_header(int);
void document_footer(void);
int process_cgivars(void);

int string_to_time(char *, time_t *);



int main(void) {
	int result = OK;
	int formid_ok = OK;

	/* Initialize shared configuration variables */                             
	init_shared_cfg_vars(1);

	/* get the arguments passed in the URL */
	process_cgivars();

	/* reset internal variables */
	reset_cgi_vars();

	/* read the CGI configuration file */
	result = read_cgi_config_file(get_cgi_config_location());
	if(result == ERROR) {
		document_header(FALSE);
		if(content_type == WML_CONTENT)
			printf("<p>Error: Could not open CGI config file!</p>\n");
		else
			cgi_config_file_error(get_cgi_config_location());
		document_footer();
		return ERROR;
		}

	/* read the main configuration file */
	result = read_main_config_file(main_config_file);
	if(result == ERROR) {
		document_header(FALSE);
		if(content_type == WML_CONTENT)
			printf("<p>Error: Could not open main config file!</p>\n");
		else
			main_config_file_error(main_config_file);
		document_footer();
		return ERROR;
		}

	/* This requires the date_format parameter in the main config file */
	if(strcmp(start_time_string, ""))
		string_to_time(start_time_string, &start_time);

	if(strcmp(end_time_string, ""))
		string_to_time(end_time_string, &end_time);


	/* read all object configuration data */
	result = read_all_object_configuration_data(main_config_file, READ_ALL_OBJECT_DATA);
	if(result == ERROR) {
		document_header(FALSE);
		if(content_type == WML_CONTENT)
			printf("<p>Error: Could not read object config data!</p>\n");
		else
			object_data_error();
		document_footer();
		return ERROR;
		}

	document_header(TRUE);

	/* get authentication information */
	get_authentication_information(&current_authdata);

	if(display_header == TRUE) {

		/* begin top table */
		printf("<table border=0 width=100%%>\n");
		printf("<tr>\n");

		/* left column of the first row */
		printf("<td align=left valign=top width=33%%>\n");
		display_info_table("External Command Interface", FALSE, &current_authdata);
		printf("</td>\n");

		/* center column of the first row */
		printf("<td align=center valign=top width=33%%>\n");
		printf("</td>\n");

		/* right column of the first row */
		printf("<td align=right valign=bottom width=33%%>\n");

		/* display context-sensitive help */
		if(command_mode == CMDMODE_COMMIT)
			display_context_help(CONTEXTHELP_CMD_COMMIT);
		else
			display_context_help(CONTEXTHELP_CMD_INPUT);

		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");
		}

	/* authorized_for_read_only should take priority */
	if(is_authorized_for_read_only(&current_authdata) == TRUE) {
		printf("<P><DIV CLASS='errorMessage'>It appears as though you do not have permission to submit the command you requested...</DIV></P>\n");
		printf("<P><DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
		printf("and check the authorization options in your CGI configuration file.</DIV></P>\n");

		document_footer();

		/* free allocated memory */
		free_memory();
		free_object_data();

		return OK;
        }

	if (cookie_form_id && *cookie_form_id) {
		formid_ok = ERROR;
		if (form_id && *form_id) {
			if (!strcmp(form_id, cookie_form_id))
				formid_ok = OK;
		}
	}

	/* if no command was specified... */
	if(command_type == CMD_NONE) {
		if(content_type == WML_CONTENT)
			printf("<p>Error: No command specified!</p>\n");
		else
			printf("<P><DIV CLASS='errorMessage'>Error: No command was specified</DIV></P>\n");
		}

	/* if this is the first request for a command, present option */
	else if(command_mode == CMDMODE_REQUEST)
		request_command_data(command_type);

	/* the user wants to commit the command */
	else if(command_mode == CMDMODE_COMMIT) {
		if (formid_ok == ERROR)	/* we're expecting an id but it wasn't there... */
			printf("<p>Error: Invalid form id!</p>\n");
		else
			commit_command_data(command_type);
	}

	document_footer();

	/* free allocated memory */
	free_memory();
	free_object_data();

	return OK;
	}



void document_header(int use_stylesheet) {

	if(content_type == WML_CONTENT) {

		printf("Content-type: text/vnd.wap.wml\r\n\r\n");

		printf("<?xml version=\"1.0\"?>\n");
		printf("<!DOCTYPE wml PUBLIC \"-//WAPFORUM//DTD WML 1.1//EN\" \"http://www.wapforum.org/DTD/wml_1.1.xml\">\n");

		printf("<wml>\n");

		printf("<card id='card1' title='Command Results'>\n");
		}

	else {

		printf("Content-type: text/html; charset=utf-8\r\n\r\n");

		printf("<html>\n");
		printf("<head>\n");
		printf("<link rel=\"shortcut icon\" href=\"%sfavicon.ico\" type=\"image/ico\">\n", url_images_path);
		printf("<title>\n");
		printf("External Command Interface\n");
		printf("</title>\n");

		if(use_stylesheet == TRUE) {
			printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, COMMON_CSS);
			printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, COMMAND_CSS);
			}

		printf("</head>\n");

		printf("<body CLASS='cmd'>\n");

		/* include user SSI header */
		include_ssi_files(COMMAND_CGI, SSI_HEADER);
		}

	return;
	}


void document_footer(void) {

	if(content_type == WML_CONTENT) {
		printf("</card>\n");
		printf("</wml>\n");
		}

	else {

		/* include user SSI footer */
		include_ssi_files(COMMAND_CGI, SSI_FOOTER);

		printf("</body>\n");
		printf("</html>\n");
		}

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

		/* we found the command type */
		else if(!strcmp(variables[x], "cmd_typ")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			command_type = atoi(variables[x]);
			}

		/* we found the command mode */
		else if(!strcmp(variables[x], "cmd_mod")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			command_mode = atoi(variables[x]);
			}

		/* we found the comment id */
		else if(!strcmp(variables[x], "com_id")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			comment_id = strtoul(variables[x], NULL, 10);
			}

		/* we found the downtime id */
		else if(!strcmp(variables[x], "down_id")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			downtime_id = strtoul(variables[x], NULL, 10);
			}

		/* we found the notification delay */
		else if(!strcmp(variables[x], "not_dly")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			notification_delay = atoi(variables[x]);
			}

		/* we found the schedule delay */
		else if(!strcmp(variables[x], "sched_dly")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			schedule_delay = atoi(variables[x]);
			}

		/* we found the comment author */
		else if(!strcmp(variables[x], "com_author")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((comment_author = (char *)strdup(variables[x])) == NULL)
				comment_author = "";
			strip_html_brackets(comment_author);
			}

		/* we found the comment data */
		else if(!strcmp(variables[x], "com_data")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((comment_data = (char *)strdup(variables[x])) == NULL)
				comment_data = "";
			strip_html_brackets(comment_data);
			}

		/* we found the host name */
		else if(!strcmp(variables[x], "host")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((host_name = (char *)strdup(variables[x])) == NULL)
				host_name = "";
			strip_html_brackets(host_name);
			}

		/* we found the hostgroup name */
		else if(!strcmp(variables[x], "hostgroup")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((hostgroup_name = (char *)strdup(variables[x])) == NULL)
				hostgroup_name = "";
			strip_html_brackets(hostgroup_name);
			}

		/* we found the service name */
		else if(!strcmp(variables[x], "service")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((service_desc = (char *)strdup(variables[x])) == NULL)
				service_desc = "";
			strip_html_brackets(service_desc);
			}

		/* we found the servicegroup name */
		else if(!strcmp(variables[x], "servicegroup")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((servicegroup_name = (char *)strdup(variables[x])) == NULL)
				servicegroup_name = "";
			strip_html_brackets(servicegroup_name);
			}

		/* we got the persistence option for a comment */
		else if(!strcmp(variables[x], "persistent"))
			persistent_comment = TRUE;

		/* we got the notification option for an acknowledgement */
		else if(!strcmp(variables[x], "send_notification"))
			send_notification = TRUE;

		/* we got the acknowledgement type */
		else if(!strcmp(variables[x], "sticky_ack"))
			sticky_ack = TRUE;

		/* we got the service check force option */
		else if(!strcmp(variables[x], "force_check"))
			force_check = TRUE;

		/* we got the option to affect host and all its services */
		else if(!strcmp(variables[x], "ahas"))
			affect_host_and_services = TRUE;

		/* we got the option to propagate to child hosts */
		else if(!strcmp(variables[x], "ptc"))
			propagate_to_children = TRUE;

		/* we got the option for fixed downtime */
		else if(!strcmp(variables[x], "fixed")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			fixed = (atoi(variables[x]) > 0) ? TRUE : FALSE;
			}

		/* we got the triggered by downtime option */
		else if(!strcmp(variables[x], "trigger")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			triggered_by = strtoul(variables[x], NULL, 10);
			}

		/* we got the child options */
		else if(!strcmp(variables[x], "childoptions")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			child_options = atoi(variables[x]);
			}

		/* we found the plugin output */
		else if(!strcmp(variables[x], "plugin_output")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			/* protect against buffer overflows */
			if(strlen(variables[x]) >= MAX_INPUT_BUFFER - 1) {
				error = TRUE;
				break;
				}
			else
				strcpy(plugin_output, variables[x]);
			}

		/* we found the performance data */
		else if(!strcmp(variables[x], "performance_data")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			/* protect against buffer overflows */
			if(strlen(variables[x]) >= MAX_INPUT_BUFFER - 1) {
				error = TRUE;
				break;
				}
			else
				strcpy(performance_data, variables[x]);
			}

		/* we found the plugin state */
		else if(!strcmp(variables[x], "plugin_state")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			plugin_state = atoi(variables[x]);
			}

		/* we found the hour duration */
		else if(!strcmp(variables[x], "hours")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(atoi(variables[x]) < 0) {
				error = TRUE;
				break;
				}
			duration += (unsigned long)(atoi(variables[x]) * 3600);
			}

		/* we found the minute duration */
		else if(!strcmp(variables[x], "minutes")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(atoi(variables[x]) < 0) {
				error = TRUE;
				break;
				}
			duration += (unsigned long)(atoi(variables[x]) * 60);
			}

		/* we found the start time */
		else if(!strcmp(variables[x], "start_time")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			start_time_string = (char *)malloc(strlen(variables[x]) + 1);
			if(start_time_string == NULL)
				start_time_string = "";
			else
				strcpy(start_time_string, variables[x]);
			}

		/* we found the end time */
		else if(!strcmp(variables[x], "end_time")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			end_time_string = (char *)malloc(strlen(variables[x]) + 1);
			if(end_time_string == NULL)
				end_time_string = "";
			else
				strcpy(end_time_string, variables[x]);
			}

		/* we found the content type argument */
		else if(!strcmp(variables[x], "content")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			if(!strcmp(variables[x], "wml")) {
				content_type = WML_CONTENT;
				display_header = FALSE;
				}
			else
				content_type = HTML_CONTENT;
			}

		/* we found the forced notification option */
		else if(!strcmp(variables[x], "force_notification"))
			force_notification = NOTIFICATION_OPTION_FORCED;

		/* we found the broadcast notification option */
		else if(!strcmp(variables[x], "broadcast_notification"))
			broadcast_notification = NOTIFICATION_OPTION_BROADCAST;

		/* we found the cookie form id */
		else if (!strcmp(variables[x], "NagFormId")) {
			x++;
			if (variables[x] == NULL) {
				error = TRUE;
				break;
			}

			cookie_form_id = (char*)strdup(variables[x]);
		}

		/* we found the form id on the form */
		else if (!strcmp(variables[x], "nagFormId")) {
			x++;
			if (variables[x] == NULL) {
				error = TRUE;
				break;
			}

			form_id = (char*)strdup(variables[x]);
		}

	}

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return error;
	}



void request_command_data(int cmd) {
	time_t t;
	char start_time_str[MAX_DATETIME_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	contact *temp_contact;
	scheduled_downtime *temp_downtime;


	/* get default name to use for comment author */
	temp_contact = find_contact(current_authdata.username);
	if(temp_contact != NULL && temp_contact->alias != NULL)
		comment_author = temp_contact->alias;
	else
		comment_author = current_authdata.username;


	printf("<P><DIV ALIGN=CENTER CLASS='cmdType'>You are requesting to ");

	switch(cmd) {

		case CMD_ADD_HOST_COMMENT:
		case CMD_ADD_SVC_COMMENT:
			printf("add a %s comment", (cmd == CMD_ADD_HOST_COMMENT) ? "host" : "service");
			break;

		case CMD_DEL_HOST_COMMENT:
		case CMD_DEL_SVC_COMMENT:
			printf("delete a %s comment", (cmd == CMD_DEL_HOST_COMMENT) ? "host" : "service");
			break;

		case CMD_DELAY_HOST_NOTIFICATION:
		case CMD_DELAY_SVC_NOTIFICATION:
			printf("delay a %s notification", (cmd == CMD_DELAY_HOST_NOTIFICATION) ? "host" : "service");
			break;

		case CMD_SCHEDULE_SVC_CHECK:
			printf("schedule a service check");
			break;

		case CMD_ENABLE_SVC_CHECK:
		case CMD_DISABLE_SVC_CHECK:
			printf("%s active checks of a particular service", (cmd == CMD_ENABLE_SVC_CHECK) ? "enable" : "disable");
			break;

		case CMD_ENABLE_NOTIFICATIONS:
		case CMD_DISABLE_NOTIFICATIONS:
			printf("%s notifications", (cmd == CMD_ENABLE_NOTIFICATIONS) ? "enable" : "disable");
			break;

		case CMD_SHUTDOWN_PROCESS:
		case CMD_RESTART_PROCESS:
			printf("%s the Nagios process", (cmd == CMD_SHUTDOWN_PROCESS) ? "shutdown" : "restart");
			break;

		case CMD_ENABLE_HOST_SVC_CHECKS:
		case CMD_DISABLE_HOST_SVC_CHECKS:
			printf("%s active checks of all services on a host", (cmd == CMD_ENABLE_HOST_SVC_CHECKS) ? "enable" : "disable");
			break;

		case CMD_SCHEDULE_HOST_SVC_CHECKS:
			printf("schedule a check of all services for a host");
			break;

		case CMD_DEL_ALL_HOST_COMMENTS:
		case CMD_DEL_ALL_SVC_COMMENTS:
			printf("delete all comments for a %s", (cmd == CMD_DEL_ALL_HOST_COMMENTS) ? "host" : "service");
			break;

		case CMD_ENABLE_SVC_NOTIFICATIONS:
		case CMD_DISABLE_SVC_NOTIFICATIONS:
			printf("%s notifications for a service", (cmd == CMD_ENABLE_SVC_NOTIFICATIONS) ? "enable" : "disable");
			break;

		case CMD_ENABLE_HOST_NOTIFICATIONS:
		case CMD_DISABLE_HOST_NOTIFICATIONS:
			printf("%s notifications for a host", (cmd == CMD_ENABLE_HOST_NOTIFICATIONS) ? "enable" : "disable");
			break;

		case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
			printf("%s notifications for all hosts and services beyond a host", (cmd == CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST) ? "enable" : "disable");
			break;

		case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
			printf("%s notifications for all services on a host", (cmd == CMD_ENABLE_HOST_SVC_NOTIFICATIONS) ? "enable" : "disable");
			break;

		case CMD_ACKNOWLEDGE_HOST_PROBLEM:
		case CMD_ACKNOWLEDGE_SVC_PROBLEM:
			printf("acknowledge a %s problem", (cmd == CMD_ACKNOWLEDGE_HOST_PROBLEM) ? "host" : "service");
			break;

		case CMD_START_EXECUTING_SVC_CHECKS:
		case CMD_STOP_EXECUTING_SVC_CHECKS:
			printf("%s executing active service checks", (cmd == CMD_START_EXECUTING_SVC_CHECKS) ? "start" : "stop");
			break;

		case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
		case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
			printf("%s accepting passive service checks", (cmd == CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS) ? "start" : "stop");
			break;

		case CMD_ENABLE_PASSIVE_SVC_CHECKS:
		case CMD_DISABLE_PASSIVE_SVC_CHECKS:
			printf("%s accepting passive service checks for a particular service", (cmd == CMD_ENABLE_PASSIVE_SVC_CHECKS) ? "start" : "stop");
			break;

		case CMD_ENABLE_EVENT_HANDLERS:
		case CMD_DISABLE_EVENT_HANDLERS:
			printf("%s event handlers", (cmd == CMD_ENABLE_EVENT_HANDLERS) ? "enable" : "disable");
			break;

		case CMD_ENABLE_HOST_EVENT_HANDLER:
		case CMD_DISABLE_HOST_EVENT_HANDLER:
			printf("%s the event handler for a particular host", (cmd == CMD_ENABLE_HOST_EVENT_HANDLER) ? "enable" : "disable");
			break;

		case CMD_ENABLE_SVC_EVENT_HANDLER:
		case CMD_DISABLE_SVC_EVENT_HANDLER:
			printf("%s the event handler for a particular service", (cmd == CMD_ENABLE_SVC_EVENT_HANDLER) ? "enable" : "disable");
			break;

		case CMD_ENABLE_HOST_CHECK:
		case CMD_DISABLE_HOST_CHECK:
			printf("%s active checks of a particular host", (cmd == CMD_ENABLE_HOST_CHECK) ? "enable" : "disable");
			break;

		case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
		case CMD_START_OBSESSING_OVER_SVC_CHECKS:
			printf("%s obsessing over service checks", (cmd == CMD_STOP_OBSESSING_OVER_SVC_CHECKS) ? "stop" : "start");
			break;

		case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
		case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
			printf("remove a %s acknowledgement", (cmd == CMD_REMOVE_HOST_ACKNOWLEDGEMENT) ? "host" : "service");
			break;

		case CMD_SCHEDULE_HOST_DOWNTIME:
		case CMD_SCHEDULE_SVC_DOWNTIME:
			printf("schedule downtime for a particular %s", (cmd == CMD_SCHEDULE_HOST_DOWNTIME) ? "host" : "service");
			break;

		case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
			printf("schedule downtime for all services for a particular host");
			break;

		case CMD_PROCESS_HOST_CHECK_RESULT:
		case CMD_PROCESS_SERVICE_CHECK_RESULT:
			printf("submit a passive check result for a particular %s", (cmd == CMD_PROCESS_HOST_CHECK_RESULT) ? "host" : "service");
			break;

		case CMD_ENABLE_HOST_FLAP_DETECTION:
		case CMD_DISABLE_HOST_FLAP_DETECTION:
			printf("%s flap detection for a particular host", (cmd == CMD_ENABLE_HOST_FLAP_DETECTION) ? "enable" : "disable");
			break;

		case CMD_ENABLE_SVC_FLAP_DETECTION:
		case CMD_DISABLE_SVC_FLAP_DETECTION:
			printf("%s flap detection for a particular service", (cmd == CMD_ENABLE_SVC_FLAP_DETECTION) ? "enable" : "disable");
			break;

		case CMD_ENABLE_FLAP_DETECTION:
		case CMD_DISABLE_FLAP_DETECTION:
			printf("%s flap detection for hosts and services", (cmd == CMD_ENABLE_FLAP_DETECTION) ? "enable" : "disable");
			break;

		case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
			printf("%s notifications for all services in a particular hostgroup", (cmd == CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS) ? "enable" : "disable");
			break;

		case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
			printf("%s notifications for all hosts in a particular hostgroup", (cmd == CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS) ? "enable" : "disable");
			break;

		case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
		case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
			printf("%s active checks of all services in a particular hostgroup", (cmd == CMD_ENABLE_HOSTGROUP_SVC_CHECKS) ? "enable" : "disable");
			break;

		case CMD_DEL_HOST_DOWNTIME:
		case CMD_DEL_SVC_DOWNTIME:
			printf("cancel scheduled downtime for a particular %s", (cmd == CMD_DEL_HOST_DOWNTIME) ? "host" : "service");
			break;

		case CMD_ENABLE_PERFORMANCE_DATA:
		case CMD_DISABLE_PERFORMANCE_DATA:
			printf("%s performance data processing for hosts and services", (cmd == CMD_ENABLE_PERFORMANCE_DATA) ? "enable" : "disable");
			break;

		case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
			printf("schedule downtime for all hosts in a particular hostgroup");
			break;

		case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
			printf("schedule downtime for all services in a particular hostgroup");
			break;

		case CMD_START_EXECUTING_HOST_CHECKS:
		case CMD_STOP_EXECUTING_HOST_CHECKS:
			printf("%s executing host checks", (cmd == CMD_START_EXECUTING_HOST_CHECKS) ? "start" : "stop");
			break;

		case CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS:
		case CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS:
			printf("%s accepting passive host checks", (cmd == CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS) ? "start" : "stop");
			break;

		case CMD_ENABLE_PASSIVE_HOST_CHECKS:
		case CMD_DISABLE_PASSIVE_HOST_CHECKS:
			printf("%s accepting passive checks for a particular host", (cmd == CMD_ENABLE_PASSIVE_HOST_CHECKS) ? "start" : "stop");
			break;

		case CMD_START_OBSESSING_OVER_HOST_CHECKS:
		case CMD_STOP_OBSESSING_OVER_HOST_CHECKS:
			printf("%s obsessing over host checks", (cmd == CMD_START_OBSESSING_OVER_HOST_CHECKS) ? "start" : "stop");
			break;

		case CMD_SCHEDULE_HOST_CHECK:
			printf("schedule a host check");
			break;

		case CMD_START_OBSESSING_OVER_SVC:
		case CMD_STOP_OBSESSING_OVER_SVC:
			printf("%s obsessing over a particular service", (cmd == CMD_START_OBSESSING_OVER_SVC) ? "start" : "stop");
			break;

		case CMD_START_OBSESSING_OVER_HOST:
		case CMD_STOP_OBSESSING_OVER_HOST:
			printf("%s obsessing over a particular host", (cmd == CMD_START_OBSESSING_OVER_HOST) ? "start" : "stop");
			break;

		case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
			printf("%s notifications for all services in a particular servicegroup", (cmd == CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS) ? "enable" : "disable");
			break;

		case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
			printf("%s notifications for all hosts in a particular servicegroup", (cmd == CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS) ? "enable" : "disable");
			break;

		case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
			printf("%s active checks of all services in a particular servicegroup", (cmd == CMD_ENABLE_SERVICEGROUP_SVC_CHECKS) ? "enable" : "disable");
			break;

		case CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME:
			printf("schedule downtime for all hosts in a particular servicegroup");
			break;

		case CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME:
			printf("schedule downtime for all services in a particular servicegroup");
			break;

		case CMD_CLEAR_HOST_FLAPPING_STATE:
		case CMD_CLEAR_SVC_FLAPPING_STATE:
			printf("clear flapping state for a %s", (cmd == CMD_CLEAR_HOST_FLAPPING_STATE) ? "host" : "service");
			break;

		case CMD_SEND_CUSTOM_HOST_NOTIFICATION:
		case CMD_SEND_CUSTOM_SVC_NOTIFICATION:
			printf("send a custom %s notification", (cmd == CMD_SEND_CUSTOM_HOST_NOTIFICATION) ? "host" : "service");
			break;

		default:
			printf("execute an unknown command.  Shame on you!</DIV>");
			return;
		}

	printf("</DIV></p>\n");

	printf("<p>\n");
	printf("<div align='center'>\n");

	printf("<table border=0 width=90%%>\n");
	printf("<tr>\n");
	printf("<td align=center valign=top>\n");

	printf("<DIV ALIGN=CENTER CLASS='optBoxTitle'>Command Options</DIV>\n");

	printf("<TABLE CELLSPACING=0 CELLPADDING=0 BORDER=1 CLASS='optBox'>\n");
	printf("<TR><TD CLASS='optBoxItem'>\n");
	printf("<form method='post' action='%s'>\n", COMMAND_CGI);
	if (cookie_form_id && *cookie_form_id)
		printf("<INPUT TYPE='hidden' NAME='nagFormId' VALUE='%s'\n", cookie_form_id);
	printf("<TABLE CELLSPACING=0 CELLPADDING=0 CLASS='optBox'>\n");

	printf("<tr><td><INPUT TYPE='HIDDEN' NAME='cmd_typ' VALUE='%d'><INPUT TYPE='HIDDEN' NAME='cmd_mod' VALUE='%d'></td></tr>\n", cmd, CMDMODE_COMMIT);

	switch(cmd) {

		case CMD_ADD_HOST_COMMENT:
		case CMD_ACKNOWLEDGE_HOST_PROBLEM:
			printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>", escape_string(host_name));
			printf("</b></td></tr>\n");
			if(cmd == CMD_ACKNOWLEDGE_HOST_PROBLEM) {
				printf("<tr><td CLASS='optBoxItem'>Sticky Acknowledgement:</td><td><b>");
				printf("<INPUT TYPE='checkbox' NAME='sticky_ack' %s>", (ack_no_sticky == TRUE) ? "" : "CHECKED");
				printf("</b></td></tr>\n");
				printf("<tr><td CLASS='optBoxItem'>Send Notification:</td><td><b>");
				printf("<INPUT TYPE='checkbox' NAME='send_notification' %s>", (ack_no_send == TRUE) ? "" : "CHECKED");
				printf("</b></td></tr>\n");
				}
			printf("<tr><td CLASS='optBoxItem'>Persistent%s:</td><td><b>", (cmd == CMD_ACKNOWLEDGE_HOST_PROBLEM) ? " Comment" : "");
			printf("<INPUT TYPE='checkbox' NAME='persistent' %s>", (cmd == CMD_ACKNOWLEDGE_HOST_PROBLEM) ? "" : "CHECKED");
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Author (Your Name):</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='com_author' VALUE='%s' %s>", escape_string(comment_author), (lock_author_names == TRUE) ? "READONLY DISABLED" : "");
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Comment:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='com_data' VALUE='%s' SIZE=40>", escape_string(comment_data));
			printf("</b></td></tr>\n");
			break;

		case CMD_ADD_SVC_COMMENT:
		case CMD_ACKNOWLEDGE_SVC_PROBLEM:
			printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>", escape_string(host_name));
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>", escape_string(service_desc));
			if(cmd == CMD_ACKNOWLEDGE_SVC_PROBLEM) {
				printf("<tr><td CLASS='optBoxItem'>Sticky Acknowledgement:</td><td><b>");
				printf("<INPUT TYPE='checkbox' NAME='sticky_ack' %s>", (ack_no_sticky == TRUE) ? "" : "CHECKED");
				printf("</b></td></tr>\n");
				printf("<tr><td CLASS='optBoxItem'>Send Notification:</td><td><b>");
				printf("<INPUT TYPE='checkbox' NAME='send_notification' %s>", (ack_no_send == TRUE) ? "" : "CHECKED");
				printf("</b></td></tr>\n");
				}
			printf("<tr><td CLASS='optBoxItem'>Persistent%s:</td><td><b>", (cmd == CMD_ACKNOWLEDGE_SVC_PROBLEM) ? " Comment" : "");
			printf("<INPUT TYPE='checkbox' NAME='persistent' %s>", (cmd == CMD_ACKNOWLEDGE_SVC_PROBLEM) ? "" : "CHECKED");
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Author (Your Name):</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='com_author' VALUE='%s' %s>", escape_string(comment_author), (lock_author_names == TRUE) ? "READONLY DISABLED" : "");
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Comment:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='com_data' VALUE='%s' SIZE=40>", escape_string(comment_data));
			printf("</b></td></tr>\n");
			break;

		case CMD_DEL_HOST_COMMENT:
		case CMD_DEL_SVC_COMMENT:
			printf("<tr><td CLASS='optBoxRequiredItem'>Comment ID:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='com_id' VALUE='%lu'>", comment_id);
			printf("</b></td></tr>\n");
			break;

		case CMD_DELAY_HOST_NOTIFICATION:
			printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>", escape_string(host_name));
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Notification Delay (minutes from now):</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='not_dly' VALUE='%d'>", notification_delay);
			printf("</b></td></tr>\n");
			break;

		case CMD_DELAY_SVC_NOTIFICATION:
			printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>", escape_string(host_name));
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>", escape_string(service_desc));
			printf("<tr><td CLASS='optBoxRequiredItem'>Notification Delay (minutes from now):</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='not_dly' VALUE='%d'>", notification_delay);
			printf("</b></td></tr>\n");
			break;

		case CMD_SCHEDULE_SVC_CHECK:
		case CMD_SCHEDULE_HOST_CHECK:
		case CMD_SCHEDULE_HOST_SVC_CHECKS:
			printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>", escape_string(host_name));
			printf("</b></td></tr>\n");
			if(cmd == CMD_SCHEDULE_SVC_CHECK) {
				printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
				printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>", escape_string(service_desc));
				printf("</b></td></tr>\n");
				}
			time(&t);
			get_time_string(&t, buffer, sizeof(buffer) - 1, SHORT_DATE_TIME);
			printf("<tr><td CLASS='optBoxRequiredItem'>Check Time:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='start_time' VALUE='%s'>", buffer);
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxItem'>Force Check:</td><td><b>");
			printf("<INPUT TYPE='checkbox' NAME='force_check' %s>", (force_check == TRUE) ? "CHECKED" : "");
			printf("</b></td></tr>\n");
			break;

		case CMD_ENABLE_SVC_CHECK:
		case CMD_DISABLE_SVC_CHECK:
		case CMD_DEL_ALL_SVC_COMMENTS:
		case CMD_ENABLE_SVC_NOTIFICATIONS:
		case CMD_DISABLE_SVC_NOTIFICATIONS:
		case CMD_ENABLE_PASSIVE_SVC_CHECKS:
		case CMD_DISABLE_PASSIVE_SVC_CHECKS:
		case CMD_ENABLE_SVC_EVENT_HANDLER:
		case CMD_DISABLE_SVC_EVENT_HANDLER:
		case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
		case CMD_ENABLE_SVC_FLAP_DETECTION:
		case CMD_DISABLE_SVC_FLAP_DETECTION:
		case CMD_START_OBSESSING_OVER_SVC:
		case CMD_STOP_OBSESSING_OVER_SVC:
		case CMD_CLEAR_SVC_FLAPPING_STATE:
			printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>", escape_string(host_name));
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>", escape_string(service_desc));
			printf("</b></td></tr>\n");
			break;

		case CMD_ENABLE_HOST_SVC_CHECKS:
		case CMD_DISABLE_HOST_SVC_CHECKS:
		case CMD_DEL_ALL_HOST_COMMENTS:
		case CMD_ENABLE_HOST_NOTIFICATIONS:
		case CMD_DISABLE_HOST_NOTIFICATIONS:
		case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
		case CMD_ENABLE_HOST_EVENT_HANDLER:
		case CMD_DISABLE_HOST_EVENT_HANDLER:
		case CMD_ENABLE_HOST_CHECK:
		case CMD_DISABLE_HOST_CHECK:
		case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
		case CMD_ENABLE_HOST_FLAP_DETECTION:
		case CMD_DISABLE_HOST_FLAP_DETECTION:
		case CMD_ENABLE_PASSIVE_HOST_CHECKS:
		case CMD_DISABLE_PASSIVE_HOST_CHECKS:
		case CMD_START_OBSESSING_OVER_HOST:
		case CMD_STOP_OBSESSING_OVER_HOST:
		case CMD_CLEAR_HOST_FLAPPING_STATE:
			printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>", escape_string(host_name));
			printf("</b></td></tr>\n");
			if(cmd == CMD_ENABLE_HOST_SVC_CHECKS || cmd == CMD_DISABLE_HOST_SVC_CHECKS || cmd == CMD_ENABLE_HOST_SVC_NOTIFICATIONS || cmd == CMD_DISABLE_HOST_SVC_NOTIFICATIONS) {
				printf("<tr><td CLASS='optBoxItem'>%s For Host Too:</td><td><b>", (cmd == CMD_ENABLE_HOST_SVC_CHECKS || cmd == CMD_ENABLE_HOST_SVC_NOTIFICATIONS) ? "Enable" : "Disable");
				printf("<INPUT TYPE='checkbox' NAME='ahas'>");
				printf("</b></td></tr>\n");
				}
			if(cmd == CMD_ENABLE_HOST_NOTIFICATIONS || cmd == CMD_DISABLE_HOST_NOTIFICATIONS) {
				printf("<tr><td CLASS='optBoxItem'>%s Notifications For Child Hosts Too:</td><td><b>", (cmd == CMD_ENABLE_HOST_NOTIFICATIONS) ? "Enable" : "Disable");
				printf("<INPUT TYPE='checkbox' NAME='ptc'>");
				printf("</b></td></tr>\n");
				}
			break;

		case CMD_ENABLE_NOTIFICATIONS:
		case CMD_DISABLE_NOTIFICATIONS:
		case CMD_SHUTDOWN_PROCESS:
		case CMD_RESTART_PROCESS:
		case CMD_START_EXECUTING_SVC_CHECKS:
		case CMD_STOP_EXECUTING_SVC_CHECKS:
		case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
		case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
		case CMD_ENABLE_EVENT_HANDLERS:
		case CMD_DISABLE_EVENT_HANDLERS:
		case CMD_START_OBSESSING_OVER_SVC_CHECKS:
		case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
		case CMD_ENABLE_FLAP_DETECTION:
		case CMD_DISABLE_FLAP_DETECTION:
		case CMD_ENABLE_PERFORMANCE_DATA:
		case CMD_DISABLE_PERFORMANCE_DATA:
		case CMD_START_EXECUTING_HOST_CHECKS:
		case CMD_STOP_EXECUTING_HOST_CHECKS:
		case CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS:
		case CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS:
		case CMD_START_OBSESSING_OVER_HOST_CHECKS:
		case CMD_STOP_OBSESSING_OVER_HOST_CHECKS:
			printf("<tr><td CLASS='optBoxItem' colspan=2>There are no options for this command.<br>Click the 'Commit' button to submit the command.</td></tr>");
			break;

		case CMD_PROCESS_HOST_CHECK_RESULT:
		case CMD_PROCESS_SERVICE_CHECK_RESULT:
			printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>", escape_string(host_name));
			printf("</b></td></tr>\n");
			if(cmd == CMD_PROCESS_SERVICE_CHECK_RESULT) {
				printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
				printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>", escape_string(service_desc));
				printf("</b></td></tr>\n");
				}
			printf("<tr><td CLASS='optBoxRequiredItem'>Check Result:</td><td><b>");
			printf("<SELECT NAME='plugin_state'>");
			if(cmd == CMD_PROCESS_SERVICE_CHECK_RESULT) {
				printf("<OPTION VALUE=%d SELECTED>OK\n", STATE_OK);
				printf("<OPTION VALUE=%d>WARNING\n", STATE_WARNING);
				printf("<OPTION VALUE=%d>UNKNOWN\n", STATE_UNKNOWN);
				printf("<OPTION VALUE=%d>CRITICAL\n", STATE_CRITICAL);
				}
			else {
				printf("<OPTION VALUE=0 SELECTED>UP\n");
				printf("<OPTION VALUE=1>DOWN\n");
				printf("<OPTION VALUE=2>UNREACHABLE\n");
				}
			printf("</SELECT>\n");
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Check Output:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='plugin_output' VALUE=''>");
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxItem'>Performance Data:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='performance_data' VALUE=''>");
			printf("</b></td></tr>\n");
			break;

		case CMD_SCHEDULE_HOST_DOWNTIME:
		case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
		case CMD_SCHEDULE_SVC_DOWNTIME:

			printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>", escape_string(host_name));
			printf("</b></td></tr>\n");
			if(cmd == CMD_SCHEDULE_SVC_DOWNTIME) {
				printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
				printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>", escape_string(service_desc));
				}
			printf("<tr><td CLASS='optBoxRequiredItem'>Author (Your Name):</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='com_author' VALUE='%s' %s>", escape_string(comment_author), (lock_author_names == TRUE) ? "READONLY DISABLED" : "");
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Comment:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='com_data' VALUE='%s' SIZE=40>", escape_string(comment_data));
			printf("</b></td></tr>\n");

			printf("<tr><td CLASS='optBoxItem'><br></td></tr>\n");

			printf("<tr><td CLASS='optBoxItem'>Triggered By:</td><td>\n");
			printf("<select name='trigger'>\n");
			printf("<option value='0'>N/A\n");

			for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; temp_downtime = temp_downtime->next) {
				if(temp_downtime->type != HOST_DOWNTIME)
					continue;
				printf("<option value='%lu'>", temp_downtime->downtime_id);
				get_time_string(&temp_downtime->start_time, start_time_str, sizeof(start_time_str), SHORT_DATE_TIME);
				printf("ID: %lu, Host '%s' starting @ %s\n", temp_downtime->downtime_id, temp_downtime->host_name, start_time_str);
				}
			for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; temp_downtime = temp_downtime->next) {
				if(temp_downtime->type != SERVICE_DOWNTIME)
					continue;
				printf("<option value='%lu'>", temp_downtime->downtime_id);
				get_time_string(&temp_downtime->start_time, start_time_str, sizeof(start_time_str), SHORT_DATE_TIME);
				printf("ID: %lu, Service '%s' on host '%s' starting @ %s \n", temp_downtime->downtime_id, temp_downtime->service_description, temp_downtime->host_name, start_time_str);
				}

			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td CLASS='optBoxItem'><br></td></tr>\n");

			time(&t);
			get_time_string(&t, buffer, sizeof(buffer) - 1, SHORT_DATE_TIME);
			printf("<tr><td CLASS='optBoxRequiredItem'>Start Time:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='start_time' VALUE='%s'>", buffer);
			printf("</b></td></tr>\n");
			t += (unsigned long)7200;
			get_time_string(&t, buffer, sizeof(buffer) - 1, SHORT_DATE_TIME);
			printf("<tr><td CLASS='optBoxRequiredItem'>End Time:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='end_time' VALUE='%s'>", buffer);
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxItem'>Type:</td><td><b>");
			printf("<SELECT NAME='fixed'>");
			printf("<OPTION VALUE=1>Fixed\n");
			printf("<OPTION VALUE=0>Flexible\n");
			printf("</SELECT>\n");
			printf("</b></td></tr>\n");

			printf("<tr><td CLASS='optBoxItem'>If Flexible, Duration:</td><td>");
			printf("<table border=0><tr>\n");
			printf("<td align=right><INPUT TYPE='TEXT' NAME='hours' VALUE='2' SIZE=2 MAXLENGTH=2></td>\n");
			printf("<td align=left>Hours</td>\n");
			printf("<td align=right><INPUT TYPE='TEXT' NAME='minutes' VALUE='0' SIZE=2 MAXLENGTH=2></td>\n");
			printf("<td align=left>Minutes</td>\n");
			printf("</tr></table>\n");
			printf("</td></tr>\n");

			printf("<tr><td CLASS='optBoxItem'><br></td></tr>\n");

			if(cmd == CMD_SCHEDULE_HOST_DOWNTIME) {
				printf("<tr><td CLASS='optBoxItem'>Child Hosts:</td><td><b>");
				printf("<SELECT name='childoptions'>");
				printf("<option value='0'>Do nothing with child hosts\n");
				printf("<option value='1'>Schedule triggered downtime for all child hosts\n");
				printf("<option value='2'>Schedule non-triggered downtime for all child hosts\n");
				printf("</SELECT>\n");
				printf("</b></td></tr>\n");
				}

			printf("<tr><td CLASS='optBoxItem'><br></td></tr>\n");

			break;

		case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
		case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
			printf("<tr><td CLASS='optBoxRequiredItem'>Hostgroup Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='hostgroup' VALUE='%s'>", escape_string(hostgroup_name));
			printf("</b></td></tr>\n");
			if(cmd == CMD_ENABLE_HOSTGROUP_SVC_CHECKS || cmd == CMD_DISABLE_HOSTGROUP_SVC_CHECKS || cmd == CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS || cmd == CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS) {
				printf("<tr><td CLASS='optBoxItem'>%s For Hosts Too:</td><td><b>", (cmd == CMD_ENABLE_HOSTGROUP_SVC_CHECKS || cmd == CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS) ? "Enable" : "Disable");
				printf("<INPUT TYPE='checkbox' NAME='ahas'>");
				printf("</b></td></tr>\n");
				}
			break;

		case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
		case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
		case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
			printf("<tr><td CLASS='optBoxRequiredItem'>Servicegroup Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='servicegroup' VALUE='%s'>", escape_string(servicegroup_name));
			printf("</b></td></tr>\n");
			if(cmd == CMD_ENABLE_SERVICEGROUP_SVC_CHECKS || cmd == CMD_DISABLE_SERVICEGROUP_SVC_CHECKS || cmd == CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS || cmd == CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS) {
				printf("<tr><td CLASS='optBoxItem'>%s For Hosts Too:</td><td><b>", (cmd == CMD_ENABLE_SERVICEGROUP_SVC_CHECKS || cmd == CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS) ? "Enable" : "Disable");
				printf("<INPUT TYPE='checkbox' NAME='ahas'>");
				printf("</b></td></tr>\n");
				}
			break;

		case CMD_DEL_HOST_DOWNTIME:
		case CMD_DEL_SVC_DOWNTIME:
			printf("<tr><td CLASS='optBoxRequiredItem'>Scheduled Downtime ID:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='down_id' VALUE='%lu'>", downtime_id);
			printf("</b></td></tr>\n");
			break;


		case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
		case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
		case CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME:
		case CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME:

			if(cmd == CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME || cmd == CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME) {
				printf("<tr><td CLASS='optBoxRequiredItem'>Hostgroup Name:</td><td><b>");
				printf("<INPUT TYPE='TEXT' NAME='hostgroup' VALUE='%s'>", escape_string(hostgroup_name));
				printf("</b></td></tr>\n");
				}
			else {
				printf("<tr><td CLASS='optBoxRequiredItem'>Servicegroup Name:</td><td><b>");
				printf("<INPUT TYPE='TEXT' NAME='servicegroup' VALUE='%s'>", escape_string(servicegroup_name));
				printf("</b></td></tr>\n");
				}
			printf("<tr><td CLASS='optBoxRequiredItem'>Author (Your Name):</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='com_author' VALUE='%s' %s>", escape_string(comment_author), (lock_author_names == TRUE) ? "READONLY DISABLED" : "");
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Comment:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='com_data' VALUE='%s' SIZE=40>", escape_string(comment_data));
			printf("</b></td></tr>\n");
			time(&t);
			get_time_string(&t, buffer, sizeof(buffer) - 1, SHORT_DATE_TIME);
			printf("<tr><td CLASS='optBoxRequiredItem'>Start Time:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='start_time' VALUE='%s'>", buffer);
			printf("</b></td></tr>\n");
			t += (unsigned long)7200;
			get_time_string(&t, buffer, sizeof(buffer) - 1, SHORT_DATE_TIME);
			printf("<tr><td CLASS='optBoxRequiredItem'>End Time:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='end_time' VALUE='%s'>", buffer);
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxItem'>Type:</td><td><b>");
			printf("<SELECT NAME='fixed'>");
			printf("<OPTION VALUE=1>Fixed\n");
			printf("<OPTION VALUE=0>Flexible\n");
			printf("</SELECT>\n");
			printf("</b></td></tr>\n");

			printf("<tr><td CLASS='optBoxItem'>If Flexible, Duration:</td><td>");
			printf("<table border=0><tr>\n");
			printf("<td align=right><INPUT TYPE='TEXT' NAME='hours' VALUE='2' SIZE=2 MAXLENGTH=2></td>\n");
			printf("<td align=left>Hours</td>\n");
			printf("<td align=right><INPUT TYPE='TEXT' NAME='minutes' VALUE='0' SIZE=2 MAXLENGTH=2></td>\n");
			printf("<td align=left>Minutes</td>\n");
			printf("</tr></table>\n");
			printf("</td></tr>\n");
			if(cmd == CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME || cmd == CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME) {
				printf("<tr><td CLASS='optBoxItem'>Schedule Downtime For Hosts Too:</td><td><b>");
				printf("<INPUT TYPE='checkbox' NAME='ahas'>");
				printf("</b></td></tr>\n");
				}
			break;

		case CMD_SEND_CUSTOM_HOST_NOTIFICATION:
		case CMD_SEND_CUSTOM_SVC_NOTIFICATION:
			printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>", escape_string(host_name));
			printf("</b></td></tr>\n");

			if(cmd == CMD_SEND_CUSTOM_SVC_NOTIFICATION) {
				printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
				printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>", escape_string(service_desc));
				printf("</b></td></tr>\n");
				}

			printf("<tr><td CLASS='optBoxItem'>Forced:</td><td><b>");
			printf("<INPUT TYPE='checkbox' NAME='force_notification' ");
			printf("</b></td></tr>\n");

			printf("<tr><td CLASS='optBoxItem'>Broadcast:</td><td><b>");
			printf("<INPUT TYPE='checkbox' NAME='broadcast_notification' ");
			printf("</b></td></tr>\n");

			printf("<tr><td CLASS='optBoxRequiredItem'>Author (Your Name):</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='com_author' VALUE='%s' %s>", escape_string(comment_author), (lock_author_names == TRUE) ? "READONLY DISABLED" : "");
			printf("</b></td></tr>\n");
			printf("<tr><td CLASS='optBoxRequiredItem'>Comment:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='com_data' VALUE='%s' SIZE=40>", escape_string(comment_data));
			printf("</b></td></tr>\n");
			break;

		default:
			printf("<tr><td CLASS='optBoxItem'>This should not be happening... :-(</td><td></td></tr>\n");
		}


	printf("<tr><td CLASS='optBoxItem' COLSPAN=2></td></tr>\n");
	printf("<tr><td CLASS='optBoxItem'></td><td CLASS='optBoxItem'><INPUT TYPE='submit' NAME='btnSubmit' VALUE='Commit'> <INPUT TYPE='reset' VALUE='Reset'></td></tr>\n");

	printf("</table>\n");
	printf("</form>\n");
	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");

	printf("</td>\n");
	printf("<td align=center valign=top width=50%%>\n");

	/* show information about the command... */
	show_command_help(cmd);

	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");

	printf("</div>\n");
	printf("</p>\n");

	printf("<P><DIV CLASS='infoMessage'>Please enter all required information before committing the command.<br>Required fields are marked in red.<br>Failure to supply all required values will result in an error.</DIV></P>");

	return;
	}


void commit_command_data(int cmd) {
	char *error_string = NULL;
	int result = OK;
	int authorized = FALSE;
	service *temp_service;
	host *temp_host;
	hostgroup *temp_hostgroup;
	nagios_comment *temp_comment;
	scheduled_downtime *temp_downtime;
	servicegroup *temp_servicegroup = NULL;
	contact *temp_contact = NULL;


	/* get authentication information */
	get_authentication_information(&current_authdata);

	/* get name to use for author */
	if(lock_author_names == TRUE) {
		temp_contact = find_contact(current_authdata.username);
		if(temp_contact != NULL && temp_contact->alias != NULL)
			comment_author = temp_contact->alias;
		else
			comment_author = current_authdata.username;
		}

	switch(cmd) {
		case CMD_ADD_HOST_COMMENT:
		case CMD_ACKNOWLEDGE_HOST_PROBLEM:

			/* make sure we have author name, and comment data... */
			if(!strcmp(comment_author, "")) {
				if(!error_string)
					error_string = strdup("Author was not entered");
				}
			if(!strcmp(comment_data, "")) {
				if(!error_string)
					error_string = strdup("Comment was not entered");
				}

			/* clean up the comment data */
			clean_comment_data(comment_author);
			clean_comment_data(comment_data);

			/* see if the user is authorized to issue a command... */
			temp_host = find_host(host_name);
			if(is_authorized_for_host_commands(temp_host, &current_authdata) == TRUE)
				authorized = TRUE;
			break;

		case CMD_ADD_SVC_COMMENT:
		case CMD_ACKNOWLEDGE_SVC_PROBLEM:

			/* make sure we have author name, and comment data... */
			if(!strcmp(comment_author, "")) {
				if(!error_string)
					error_string = strdup("Author was not entered");
				}
			if(!strcmp(comment_data, "")) {
				if(!error_string)
					error_string = strdup("Comment was not entered");
				}

			/* clean up the comment data */
			clean_comment_data(comment_author);
			clean_comment_data(comment_data);

			/* see if the user is authorized to issue a command... */
			temp_service = find_service(host_name, service_desc);
			if(is_authorized_for_service_commands(temp_service, &current_authdata) == TRUE)
				authorized = TRUE;
			break;

		case CMD_DEL_HOST_COMMENT:
		case CMD_DEL_SVC_COMMENT:

			/* check the sanity of the comment id */
			if(comment_id == 0) {
				if(!error_string)
					error_string = strdup("Comment id cannot be 0");
				}

			/* find the comment */
			if(cmd == CMD_DEL_HOST_COMMENT)
				temp_comment = find_host_comment(comment_id);
			else
				temp_comment = find_service_comment(comment_id);

			/* see if the user is authorized to issue a command... */
			if(cmd == CMD_DEL_HOST_COMMENT && temp_comment != NULL) {
				temp_host = find_host(temp_comment->host_name);
				if(is_authorized_for_host_commands(temp_host, &current_authdata) == TRUE)
					authorized = TRUE;
				}
			if(cmd == CMD_DEL_SVC_COMMENT && temp_comment != NULL) {
				temp_service = find_service(temp_comment->host_name, temp_comment->service_description);
				if(is_authorized_for_service_commands(temp_service, &current_authdata) == TRUE)
					authorized = TRUE;
				}

			/* free comment data */
			free_comment_data();

			break;

		case CMD_DEL_HOST_DOWNTIME:
		case CMD_DEL_SVC_DOWNTIME:

			/* check the sanity of the downtime id */
			if(downtime_id == 0) {
				if(!error_string)
					error_string = strdup("Downtime id cannot be 0");
				}

			/* find the downtime entry */
			if(cmd == CMD_DEL_HOST_DOWNTIME)
				temp_downtime = find_host_downtime(downtime_id);
			else
				temp_downtime = find_service_downtime(downtime_id);

			/* see if the user is authorized to issue a command... */
			if(cmd == CMD_DEL_HOST_DOWNTIME && temp_downtime != NULL) {
				temp_host = find_host(temp_downtime->host_name);
				if(is_authorized_for_host_commands(temp_host, &current_authdata) == TRUE)
					authorized = TRUE;
				}
			if(cmd == CMD_DEL_SVC_DOWNTIME && temp_downtime != NULL) {
				temp_service = find_service(temp_downtime->host_name, temp_downtime->service_description);
				if(is_authorized_for_service_commands(temp_service, &current_authdata) == TRUE)
					authorized = TRUE;
				}

			/* free downtime data */
			free_downtime_data();

			break;

		case CMD_SCHEDULE_SVC_CHECK:
		case CMD_ENABLE_SVC_CHECK:
		case CMD_DISABLE_SVC_CHECK:
		case CMD_DEL_ALL_SVC_COMMENTS:
		case CMD_ENABLE_SVC_NOTIFICATIONS:
		case CMD_DISABLE_SVC_NOTIFICATIONS:
		case CMD_ENABLE_PASSIVE_SVC_CHECKS:
		case CMD_DISABLE_PASSIVE_SVC_CHECKS:
		case CMD_ENABLE_SVC_EVENT_HANDLER:
		case CMD_DISABLE_SVC_EVENT_HANDLER:
		case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
		case CMD_PROCESS_SERVICE_CHECK_RESULT:
		case CMD_SCHEDULE_SVC_DOWNTIME:
		case CMD_DELAY_SVC_NOTIFICATION:
		case CMD_ENABLE_SVC_FLAP_DETECTION:
		case CMD_DISABLE_SVC_FLAP_DETECTION:
		case CMD_START_OBSESSING_OVER_SVC:
		case CMD_STOP_OBSESSING_OVER_SVC:
		case CMD_CLEAR_SVC_FLAPPING_STATE:

			/* make sure we have author name and comment data... */
			if(cmd == CMD_SCHEDULE_SVC_DOWNTIME) {
				if(!strcmp(comment_data, "")) {
					if(!error_string)
						error_string = strdup("Comment was not entered");
					}
				else if(!strcmp(comment_author, "")) {
					if(!error_string)
						error_string = strdup("Author was not entered");
					}
				}

			/* see if the user is authorized to issue a command... */
			temp_service = find_service(host_name, service_desc);
			if(is_authorized_for_service_commands(temp_service, &current_authdata) == TRUE)
				authorized = TRUE;

			/* make sure we have passive check info (if necessary) */
			if(cmd == CMD_PROCESS_SERVICE_CHECK_RESULT && !strcmp(plugin_output, "")) {
				if(!error_string)
					error_string = strdup("Plugin output cannot be blank");
				}

			/* make sure we have a notification delay (if necessary) */
			if(cmd == CMD_DELAY_SVC_NOTIFICATION && notification_delay <= 0) {
				if(!error_string)
					error_string = strdup("Notification delay must be greater than 0");
				}

			/* clean up the comment data if scheduling downtime */
			if(cmd == CMD_SCHEDULE_SVC_DOWNTIME) {
				clean_comment_data(comment_author);
				clean_comment_data(comment_data);
				}

			/* make sure we have check time (if necessary) */
			if(cmd == CMD_SCHEDULE_SVC_CHECK && start_time == (time_t)0) {
				if(!error_string)
					error_string = strdup("Start time must be non-zero or bad format has been submitted.");
				}

			/* make sure we have start/end times for downtime (if necessary) */
			if(cmd == CMD_SCHEDULE_SVC_DOWNTIME && (start_time == (time_t)0 || end_time == (time_t)0 || end_time < start_time)) {
				if(!error_string)
					error_string = strdup("Start or end time not valid");
				}

			break;

		case CMD_ENABLE_NOTIFICATIONS:
		case CMD_DISABLE_NOTIFICATIONS:
		case CMD_SHUTDOWN_PROCESS:
		case CMD_RESTART_PROCESS:
		case CMD_START_EXECUTING_SVC_CHECKS:
		case CMD_STOP_EXECUTING_SVC_CHECKS:
		case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
		case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
		case CMD_ENABLE_EVENT_HANDLERS:
		case CMD_DISABLE_EVENT_HANDLERS:
		case CMD_START_OBSESSING_OVER_SVC_CHECKS:
		case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
		case CMD_ENABLE_FLAP_DETECTION:
		case CMD_DISABLE_FLAP_DETECTION:
		case CMD_ENABLE_PERFORMANCE_DATA:
		case CMD_DISABLE_PERFORMANCE_DATA:
		case CMD_START_EXECUTING_HOST_CHECKS:
		case CMD_STOP_EXECUTING_HOST_CHECKS:
		case CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS:
		case CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS:
		case CMD_START_OBSESSING_OVER_HOST_CHECKS:
		case CMD_STOP_OBSESSING_OVER_HOST_CHECKS:

			/* see if the user is authorized to issue a command... */
			if(is_authorized_for_system_commands(&current_authdata) == TRUE)
				authorized = TRUE;
			break;

		case CMD_ENABLE_HOST_SVC_CHECKS:
		case CMD_DISABLE_HOST_SVC_CHECKS:
		case CMD_DEL_ALL_HOST_COMMENTS:
		case CMD_SCHEDULE_HOST_SVC_CHECKS:
		case CMD_ENABLE_HOST_NOTIFICATIONS:
		case CMD_DISABLE_HOST_NOTIFICATIONS:
		case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
		case CMD_ENABLE_HOST_EVENT_HANDLER:
		case CMD_DISABLE_HOST_EVENT_HANDLER:
		case CMD_ENABLE_HOST_CHECK:
		case CMD_DISABLE_HOST_CHECK:
		case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
		case CMD_SCHEDULE_HOST_DOWNTIME:
		case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
		case CMD_DELAY_HOST_NOTIFICATION:
		case CMD_ENABLE_HOST_FLAP_DETECTION:
		case CMD_DISABLE_HOST_FLAP_DETECTION:
		case CMD_PROCESS_HOST_CHECK_RESULT:
		case CMD_ENABLE_PASSIVE_HOST_CHECKS:
		case CMD_DISABLE_PASSIVE_HOST_CHECKS:
		case CMD_SCHEDULE_HOST_CHECK:
		case CMD_START_OBSESSING_OVER_HOST:
		case CMD_STOP_OBSESSING_OVER_HOST:
		case CMD_CLEAR_HOST_FLAPPING_STATE:

			/* make sure we have author name and comment data... */
			if(cmd == CMD_SCHEDULE_HOST_DOWNTIME || cmd == CMD_SCHEDULE_HOST_SVC_DOWNTIME) {
				if(!strcmp(comment_data, "")) {
					if(!error_string)
						error_string = strdup("Comment was not entered");
					}
				else if(!strcmp(comment_author, "")) {
					if(!error_string)
						error_string = strdup("Author was not entered");
					}
				}

			/* see if the user is authorized to issue a command... */
			temp_host = find_host(host_name);
			if(is_authorized_for_host_commands(temp_host, &current_authdata) == TRUE)
				authorized = TRUE;

			/* clean up the comment data if scheduling downtime */
			if(cmd == CMD_SCHEDULE_HOST_DOWNTIME || cmd == CMD_SCHEDULE_HOST_SVC_DOWNTIME) {
				clean_comment_data(comment_author);
				clean_comment_data(comment_data);
				}

			/* make sure we have a notification delay (if necessary) */
			if(cmd == CMD_DELAY_HOST_NOTIFICATION && notification_delay <= 0) {
				if(!error_string)
					error_string = strdup("Notification delay must be greater than 0");
				}

			/* make sure we have start/end times for downtime (if necessary) */
			if((cmd == CMD_SCHEDULE_HOST_DOWNTIME || cmd == CMD_SCHEDULE_HOST_SVC_DOWNTIME) && (start_time == (time_t)0 || end_time == (time_t)0 || start_time > end_time)) {
				if(!error_string)
					error_string = strdup("Start or end time not valid");
				}

			/* make sure we have check time (if necessary) */
			if((cmd == CMD_SCHEDULE_HOST_CHECK || cmd == CMD_SCHEDULE_HOST_SVC_CHECKS) && start_time == (time_t)0) {
				if(!error_string)
					error_string = strdup("Start time must be non-zero or bad format has been submitted.");
				}

			/* make sure we have passive check info (if necessary) */
			if(cmd == CMD_PROCESS_HOST_CHECK_RESULT && !strcmp(plugin_output, "")) {
				if(!error_string)
					error_string = strdup("Plugin output cannot be blank");
				}

			break;

		case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
		case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
		case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
		case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:

			/* make sure we have author and comment data */
			if(cmd == CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME || cmd == CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME) {
				if(!strcmp(comment_data, "")) {
					if(!error_string)
						error_string = strdup("Comment was not entered");
					}
				else if(!strcmp(comment_author, "")) {
					if(!error_string)
						error_string = strdup("Author was not entered");
					}
				}

			/* make sure we have start/end times for downtime */
			if((cmd == CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME || cmd == CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME) && (start_time == (time_t)0 || end_time == (time_t)0 || start_time > end_time)) {
				if(!error_string)
					error_string = strdup("Start or end time not valid");
				}

			/* see if the user is authorized to issue a command... */
			temp_hostgroup = find_hostgroup(hostgroup_name);
			if(is_authorized_for_hostgroup_commands(temp_hostgroup, &current_authdata) == TRUE)
				authorized = TRUE;

			/* clean up the comment data if scheduling downtime */
			if(cmd == CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME || cmd == CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME) {
				clean_comment_data(comment_author);
				clean_comment_data(comment_data);
				}

			break;

		case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
		case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
		case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
		case CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME:
		case CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME:

			/* make sure we have author and comment data */
			if(cmd == CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME || cmd == CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME) {
				if(!strcmp(comment_data, "")) {
					if(!error_string)
						error_string = strdup("Comment was not entered");
					}
				else if(!strcmp(comment_author, "")) {
					if(!error_string)
						error_string = strdup("Author was not entered");
					}
				}

			/* make sure we have start/end times for downtime */
			if((cmd == CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME || cmd == CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME) && (start_time == (time_t)0 || end_time == (time_t)0 || start_time > end_time)) {
				if(!error_string)
					error_string = strdup("Start or end time not valid");
				}

			/* see if the user is authorized to issue a command... */

			temp_servicegroup = find_servicegroup(servicegroup_name);
			if(is_authorized_for_servicegroup_commands(temp_servicegroup, &current_authdata) == TRUE)
				authorized = TRUE;

			break;

		case CMD_SEND_CUSTOM_HOST_NOTIFICATION:
		case CMD_SEND_CUSTOM_SVC_NOTIFICATION:

			/* make sure we have author and comment data */
			if(!strcmp(comment_data, "")) {
				if(!error_string)
					error_string = strdup("Comment was not entered");
				}
			else if(!strcmp(comment_author, "")) {
				if(!error_string)
					error_string = strdup("Author was not entered");
				}

			/* see if the user is authorized to issue a command... */
			if(cmd == CMD_SEND_CUSTOM_HOST_NOTIFICATION) {
				temp_host = find_host(host_name);
				if(is_authorized_for_host_commands(temp_host, &current_authdata) == TRUE)
					authorized = TRUE;
				}
			else {
				temp_service = find_service(host_name, service_desc);
				if(is_authorized_for_service_commands(temp_service, &current_authdata) == TRUE)
					authorized = TRUE;
				}
			break;

		default:
			if(!error_string) error_string = strdup("An error occurred while processing your command!");
		}


	/* to be safe, we are going to REQUIRE that the authentication functionality is enabled... */
	if(use_authentication == FALSE) {
		if(content_type == WML_CONTENT)
			printf("<p>Error: Authentication is not enabled!</p>\n");
		else {
			printf("<P>\n");
			printf("<DIV CLASS='errorMessage'>Sorry Dave, I can't let you do that...</DIV><br>");
			printf("<DIV CLASS='errorDescription'>");
			printf("It seems that you have chosen to not use the authentication functionality of the CGIs.<br><br>");
			printf("I don't want to be personally responsible for what may happen as a result of allowing unauthorized users to issue commands to Nagios,");
			printf("so you'll have to disable this safeguard if you are really stubborn and want to invite trouble.<br><br>");
			printf("<strong>Read the section on CGI authentication in the HTML documentation to learn how you can enable authentication and why you should want to.</strong>\n");
			printf("</DIV>\n");
			printf("</P>\n");
			}
		}

	/* the user is not authorized to issue the given command */
	else if(authorized == FALSE) {
		if(content_type == WML_CONTENT)
			printf("<p>Error: You're not authorized to commit that command!</p>\n");
		else {
			printf("<P><DIV CLASS='errorMessage'>Sorry, but you are not authorized to commit the specified command.</DIV></P>\n");
			printf("<P><DIV CLASS='errorDescription'>Read the section of the documentation that deals with authentication and authorization in the CGIs for more information.<BR><BR>\n");
			printf("<A HREF='javascript:window.history.go(-2)'>Return from whence you came</A></DIV></P>\n");
			}
		}

	/* some error occurred (data was probably missing) */
	else if(error_string) {
		if(content_type == WML_CONTENT)
			printf("<p>%s</p>\n", error_string);
		else {
			printf("<P><DIV CLASS='errorMessage'>%s</DIV></P>\n", error_string);
			free(error_string);
			printf("<P><DIV CLASS='errorDescription'>Go <A HREF='javascript:window.history.go(-1)'>back</A> and verify that you entered all required information correctly.<BR>\n");
			printf("<A HREF='javascript:window.history.go(-2)'>Return from whence you came</A></DIV></P>\n");
			}
		}

	/* if Nagios isn't checking external commands, don't do anything... */
	else if(check_external_commands == FALSE) {
		if(content_type == WML_CONTENT)
			printf("<p>Error: Nagios is not checking external commands!</p>\n");
		else {
			printf("<P><DIV CLASS='errorMessage'>Sorry, but Nagios is currently not checking for external commands, so your command will not be committed!</DIV></P>\n");
			printf("<P><DIV CLASS='errorDescription'>Read the documentation for information on how to enable external commands...<BR><BR>\n");
			printf("<A HREF='javascript:window.history.go(-2)'>Return from whence you came</A></DIV></P>\n");
			}
		}

	/* everything looks okay, so let's go ahead and commit the command... */
	else {

		/* commit the command */
		result = commit_command(cmd);

		if(result == OK) {
			if(content_type == WML_CONTENT)
				printf("<p>Your command was submitted successfully...</p>\n");
			else {
				printf("<P><DIV CLASS='infoMessage'>Your command request was successfully submitted to Nagios for processing.<BR><BR>\n");
				printf("Note: It may take a while before the command is actually processed.<BR><BR>\n");
				printf("<A HREF='javascript:window.history.go(-2)'>Done</A></DIV></P>");
				}
			}
		else {
			if(content_type == WML_CONTENT)
				printf("<p>An error occurred while committing your command!</p>\n");
			else {
				printf("<P><DIV CLASS='errorMessage'>An error occurred while attempting to commit your command for processing.<BR><BR>\n");
				printf("<A HREF='javascript:window.history.go(-2)'>Return from whence you came</A></DIV></P>\n");
				}
			}
		}

	return;
	}

__attribute__((format(printf, 2, 3)))
static int cmd_submitf(int id, const char *fmt, ...) {
	char cmd[MAX_EXTERNAL_COMMAND_LENGTH];
	const char *command_name;
	int len;
	int len2;
	va_list ap;

	command_name = extcmd_get_name(id);
	/*
	 * We disallow sending 'CHANGE' commands from the cgi's
	 * until we do proper session handling to prevent cross-site
	 * request forgery
	 */
	if(!command_name || (strlen(command_name) > 6 && !memcmp("CHANGE", command_name, 6)))
		return ERROR;

	len = snprintf(cmd, sizeof(cmd), "[%lu] %s;", time(NULL), command_name);
	if(len < 0 || len >= sizeof(cmd))
		return ERROR;

	if(fmt) {
		va_start(ap, fmt);
		len2 = vsnprintf(cmd + len, sizeof(cmd) - len, fmt, ap);
		va_end(ap);
		len += len2;
		if(len2 < 0 || len >= sizeof(cmd))
			return ERROR;
		}

	cmd[len] = 0; /* 0 <= len < sizeof(cmd) */
	return write_command_to_file(cmd);
	}



/* commits a command for processing */
int commit_command(int cmd) {
	time_t current_time;
	time_t scheduled_time;
	time_t notification_time;
	int result;

	/* get the current time */
	time(&current_time);

	/* get the scheduled time */
	scheduled_time = current_time + (schedule_delay * 60);

	/* get the notification time */
	notification_time = current_time + (notification_delay * 60);

	/*
	 * these are supposed to be implanted inside the
	 * completed commands shipped off to nagios and
	 * must therefore never contain ';'
	 */
	if(host_name && strchr(host_name, ';'))
		return ERROR;
	if(service_desc && strchr(service_desc, ';'))
		return ERROR;
	if(comment_author && strchr(comment_author, ';'))
		return ERROR;
	if(hostgroup_name && strchr(hostgroup_name, ';'))
		return ERROR;
	if(servicegroup_name && strchr(servicegroup_name, ';'))
		return ERROR;

	/* decide how to form the command line... */
	switch(cmd) {

			/* commands without arguments */
		case CMD_START_EXECUTING_SVC_CHECKS:
		case CMD_STOP_EXECUTING_SVC_CHECKS:
		case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
		case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
		case CMD_ENABLE_EVENT_HANDLERS:
		case CMD_DISABLE_EVENT_HANDLERS:
		case CMD_START_OBSESSING_OVER_SVC_CHECKS:
		case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
		case CMD_ENABLE_FLAP_DETECTION:
		case CMD_DISABLE_FLAP_DETECTION:
		case CMD_ENABLE_PERFORMANCE_DATA:
		case CMD_DISABLE_PERFORMANCE_DATA:
		case CMD_START_EXECUTING_HOST_CHECKS:
		case CMD_STOP_EXECUTING_HOST_CHECKS:
		case CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS:
		case CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS:
		case CMD_START_OBSESSING_OVER_HOST_CHECKS:
		case CMD_STOP_OBSESSING_OVER_HOST_CHECKS:
			result = cmd_submitf(cmd, NULL);
			break;

			/** simple host commands **/
		case CMD_ENABLE_HOST_FLAP_DETECTION:
		case CMD_DISABLE_HOST_FLAP_DETECTION:
		case CMD_ENABLE_PASSIVE_HOST_CHECKS:
		case CMD_DISABLE_PASSIVE_HOST_CHECKS:
		case CMD_START_OBSESSING_OVER_HOST:
		case CMD_STOP_OBSESSING_OVER_HOST:
		case CMD_DEL_ALL_HOST_COMMENTS:
		case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		case CMD_ENABLE_HOST_EVENT_HANDLER:
		case CMD_DISABLE_HOST_EVENT_HANDLER:
		case CMD_ENABLE_HOST_CHECK:
		case CMD_DISABLE_HOST_CHECK:
		case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
		case CMD_CLEAR_HOST_FLAPPING_STATE:
			result = cmd_submitf(cmd, "%s", host_name);
			break;

			/** simple service commands **/
		case CMD_ENABLE_SVC_FLAP_DETECTION:
		case CMD_DISABLE_SVC_FLAP_DETECTION:
		case CMD_ENABLE_PASSIVE_SVC_CHECKS:
		case CMD_DISABLE_PASSIVE_SVC_CHECKS:
		case CMD_START_OBSESSING_OVER_SVC:
		case CMD_STOP_OBSESSING_OVER_SVC:
		case CMD_DEL_ALL_SVC_COMMENTS:
		case CMD_ENABLE_SVC_NOTIFICATIONS:
		case CMD_DISABLE_SVC_NOTIFICATIONS:
		case CMD_ENABLE_SVC_EVENT_HANDLER:
		case CMD_DISABLE_SVC_EVENT_HANDLER:
		case CMD_ENABLE_SVC_CHECK:
		case CMD_DISABLE_SVC_CHECK:
		case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
		case CMD_CLEAR_SVC_FLAPPING_STATE:
			result = cmd_submitf(cmd, "%s;%s", host_name, service_desc);
			break;

		case CMD_ADD_HOST_COMMENT:
			result = cmd_submitf(cmd, "%s;%d;%s;%s", host_name, persistent_comment, comment_author, comment_data);
			break;

		case CMD_ADD_SVC_COMMENT:
			result = cmd_submitf(cmd, "%s;%s;%d;%s;%s", host_name, service_desc, persistent_comment, comment_author, comment_data);
			break;

		case CMD_DEL_HOST_COMMENT:
		case CMD_DEL_SVC_COMMENT:
			result = cmd_submitf(cmd, "%lu", comment_id);
			break;

		case CMD_DELAY_HOST_NOTIFICATION:
			result = cmd_submitf(cmd, "%s;%lu", host_name, notification_time);
			break;

		case CMD_DELAY_SVC_NOTIFICATION:
			result = cmd_submitf(cmd, "%s;%s;%lu", host_name, service_desc, notification_time);
			break;

		case CMD_SCHEDULE_SVC_CHECK:
		case CMD_SCHEDULE_FORCED_SVC_CHECK:
			if(force_check == TRUE)
				cmd = CMD_SCHEDULE_FORCED_SVC_CHECK;
			result = cmd_submitf(cmd, "%s;%s;%lu", host_name, service_desc, start_time);
			break;

		case CMD_DISABLE_NOTIFICATIONS:
		case CMD_ENABLE_NOTIFICATIONS:
		case CMD_SHUTDOWN_PROCESS:
		case CMD_RESTART_PROCESS:
			result = cmd_submitf(cmd, "%lu", scheduled_time);
			break;

		case CMD_ENABLE_HOST_SVC_CHECKS:
		case CMD_DISABLE_HOST_SVC_CHECKS:
			result = cmd_submitf(cmd, "%s", host_name);
			if(affect_host_and_services == TRUE) {
				cmd = (cmd == CMD_ENABLE_HOST_SVC_CHECKS) ? CMD_ENABLE_HOST_CHECK : CMD_DISABLE_HOST_CHECK;
				result |= cmd_submitf(cmd, "%s", host_name);
				}
			break;

		case CMD_SCHEDULE_HOST_SVC_CHECKS:
			if(force_check == TRUE)
				cmd = CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS;
			result = cmd_submitf(cmd, "%s;%lu", host_name, scheduled_time);
			break;

		case CMD_ENABLE_HOST_NOTIFICATIONS:
		case CMD_DISABLE_HOST_NOTIFICATIONS:
			if(propagate_to_children == TRUE)
				cmd = (cmd == CMD_ENABLE_HOST_NOTIFICATIONS) ? CMD_ENABLE_HOST_AND_CHILD_NOTIFICATIONS : CMD_DISABLE_HOST_AND_CHILD_NOTIFICATIONS;
			result = cmd_submitf(cmd, "%s", host_name);
			break;

		case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
			result = cmd_submitf(cmd, "%s", host_name);
			if(affect_host_and_services == TRUE) {
				cmd = (cmd == CMD_ENABLE_HOST_SVC_NOTIFICATIONS) ? CMD_ENABLE_HOST_NOTIFICATIONS : CMD_DISABLE_HOST_NOTIFICATIONS;
				result |= cmd_submitf(cmd, "%s", host_name);
				}
			break;

		case CMD_ACKNOWLEDGE_HOST_PROBLEM:
			result = cmd_submitf(cmd, "%s;%d;%d;%d;%s;%s", host_name, (sticky_ack == TRUE) ? ACKNOWLEDGEMENT_STICKY : ACKNOWLEDGEMENT_NORMAL, send_notification, persistent_comment, comment_author, comment_data);
			break;

		case CMD_ACKNOWLEDGE_SVC_PROBLEM:
			result = cmd_submitf(cmd, "%s;%s;%d;%d;%d;%s;%s", host_name, service_desc, (sticky_ack == TRUE) ? ACKNOWLEDGEMENT_STICKY : ACKNOWLEDGEMENT_NORMAL, send_notification, persistent_comment, comment_author, comment_data);
			break;

		case CMD_PROCESS_SERVICE_CHECK_RESULT:
			result = cmd_submitf(cmd, "%s;%s;%d;%s|%s", host_name, service_desc, plugin_state, plugin_output, performance_data);
			break;

		case CMD_PROCESS_HOST_CHECK_RESULT:
			result = cmd_submitf(cmd, "%s;%d;%s|%s", host_name, plugin_state, plugin_output, performance_data);
			break;

		case CMD_SCHEDULE_HOST_DOWNTIME:
			if(child_options == 1)
				cmd = CMD_SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME;
			else if(child_options == 2)
				cmd = CMD_SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME;

			result = cmd_submitf(cmd, "%s;%lu;%lu;%d;%lu;%lu;%s;%s", host_name, start_time, end_time, fixed, triggered_by, duration, comment_author, comment_data);
			break;

		case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
			result = cmd_submitf(cmd, "%s;%lu;%lu;%d;%lu;%lu;%s;%s", host_name, start_time, end_time, fixed, triggered_by, duration, comment_author, comment_data);
			break;

		case CMD_SCHEDULE_SVC_DOWNTIME:
			result = cmd_submitf(cmd, "%s;%s;%lu;%lu;%d;%lu;%lu;%s;%s", host_name, service_desc, start_time, end_time, fixed, triggered_by, duration, comment_author, comment_data);
			break;

		case CMD_DEL_HOST_DOWNTIME:
		case CMD_DEL_SVC_DOWNTIME:
			result = cmd_submitf(cmd, "%lu", downtime_id);
			break;

		case CMD_SCHEDULE_HOST_CHECK:
			if(force_check == TRUE)
				cmd = CMD_SCHEDULE_FORCED_HOST_CHECK;
			result = cmd_submitf(cmd, "%s;%lu", host_name, start_time);
			break;

		case CMD_SEND_CUSTOM_HOST_NOTIFICATION:
			result = cmd_submitf(cmd, "%s;%d;%s;%s", host_name, (force_notification | broadcast_notification), comment_author, comment_data);
			break;

		case CMD_SEND_CUSTOM_SVC_NOTIFICATION:
			result = cmd_submitf(cmd, "%s;%s;%d;%s;%s", host_name, service_desc, (force_notification | broadcast_notification), comment_author, comment_data);
			break;


			/***** HOSTGROUP COMMANDS *****/

		case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
			result = cmd_submitf(cmd, "%s", hostgroup_name);
			if(affect_host_and_services == TRUE) {
				cmd = (cmd == CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS) ? CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS : CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS;
				result |= cmd_submitf(cmd, "%s", hostgroup_name);
				}
			break;

		case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
			result = cmd_submitf(cmd, "%s", hostgroup_name);
			break;

		case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
		case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
			result = cmd_submitf(cmd, "%s", hostgroup_name);
			if(affect_host_and_services == TRUE) {
				cmd = (cmd == CMD_ENABLE_HOSTGROUP_SVC_CHECKS) ? CMD_ENABLE_HOSTGROUP_HOST_CHECKS : CMD_DISABLE_HOSTGROUP_HOST_CHECKS;
				result |= cmd_submitf(cmd, "%s", hostgroup_name);
				}
			break;

		case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
			result = cmd_submitf(cmd, "%s;%lu;%lu;%d;0;%lu;%s;%s", hostgroup_name, start_time, end_time, fixed, duration, comment_author, comment_data);
			break;

		case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
			result = cmd_submitf(cmd, "%s;%lu;%lu;%d;0;%lu;%s;%s", hostgroup_name, start_time, end_time, fixed, duration, comment_author, comment_data);
			if(affect_host_and_services == TRUE)
				result |= cmd_submitf(CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME, "%s;%lu;%lu;%d;0;%lu;%s;%s", hostgroup_name, start_time, end_time, fixed, duration, comment_author, comment_data);
			break;


			/***** SERVICEGROUP COMMANDS *****/

		case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
			result = cmd_submitf(cmd, "%s", servicegroup_name);
			if(affect_host_and_services == TRUE) {
				cmd = (cmd == CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS) ? CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS : CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS;
				result |= cmd_submitf(cmd, "%s", servicegroup_name);
				}
			break;

		case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
			result = cmd_submitf(cmd, "%s", servicegroup_name);
			break;

		case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
			result = cmd_submitf(cmd, "%s", servicegroup_name);
			if(affect_host_and_services == TRUE) {
				cmd = (cmd == CMD_ENABLE_SERVICEGROUP_SVC_CHECKS) ? CMD_ENABLE_SERVICEGROUP_HOST_CHECKS : CMD_DISABLE_SERVICEGROUP_HOST_CHECKS;
				result |= cmd_submitf(cmd, "%s", servicegroup_name);
				}
			break;

		case CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME:
			result = cmd_submitf(cmd, "%s;%lu;%lu;%d;0;%lu;%s;%s", servicegroup_name, start_time, end_time, fixed, duration, comment_author, comment_data);
			break;

		case CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME:
			result = cmd_submitf(cmd, "%s;%lu;%lu;%d;0;%lu;%s;%s", servicegroup_name, start_time, end_time, fixed, duration, comment_author, comment_data);
			if(affect_host_and_services == TRUE)
				result |= cmd_submitf(CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME, "%s;%lu;%lu;%d;0;%lu;%s;%s", servicegroup_name, start_time, end_time, fixed, duration, comment_author, comment_data);
			break;

		default:
			return ERROR;
			break;
		}

	return result;
	}



/* write a command entry to the command file */
int write_command_to_file(char *cmd) {
	FILE *fp;
	struct stat statbuf;

	/*
	 * Commands are not allowed to have newlines in them, as
	 * that allows malicious users to hand-craft requests that
	 * bypass the access-restrictions.
	 */
	if(!cmd || !*cmd || strchr(cmd, '\n'))
		return ERROR;

	/* bail out if the external command file doesn't exist */
	if(stat(command_file, &statbuf)) {

		if(content_type == WML_CONTENT)
			printf("<p>Error: Could not stat() external command file!</p>\n");
		else {
			printf("<P><DIV CLASS='errorMessage'>Error: Could not stat() command file '%s'!</DIV></P>\n", command_file);
			printf("<P><DIV CLASS='errorDescription'>");
			printf("The external command file may be missing, Nagios may not be running, and/or Nagios may not be checking external commands.\n");
			printf("</DIV></P>\n");
			}

		return ERROR;
		}

	/* open the command for writing (since this is a pipe, it will really be appended) */
	fp = fopen(command_file, "w");
	if(fp == NULL) {

		if(content_type == WML_CONTENT)
			printf("<p>Error: Could not open command file for update!</p>\n");
		else {
			printf("<P><DIV CLASS='errorMessage'>Error: Could not open command file '%s' for update!</DIV></P>\n", command_file);
			printf("<P><DIV CLASS='errorDescription'>");
			printf("The permissions on the external command file and/or directory may be incorrect.  Read the FAQs on how to setup proper permissions.\n");
			printf("</DIV></P>\n");
			}

		return ERROR;
		}

	/* write the command to file */
	fprintf(fp, "%s\n", cmd);

	/* flush buffer */
	fflush(fp);

	fclose(fp);

	return OK;
	}


/* strips out semicolons from comment data */
void clean_comment_data(char *buffer) {
	int x;
	int y;

	y = (int)strlen(buffer);

	for(x = 0; x < y; x++) {
		if(buffer[x] == ';')
			buffer[x] = ' ';
		}

	return;
	}


/* display information about a command */
void show_command_help(int cmd) {

	printf("<DIV ALIGN=CENTER CLASS='descriptionTitle'>Command Description</DIV>\n");
	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 CLASS='commandDescription'>\n");
	printf("<TR><TD CLASS='commandDescription'>\n");

	/* decide what information to print out... */
	switch(cmd) {

		case CMD_ADD_HOST_COMMENT:
			printf("This command is used to add a comment for the specified host.  If you work with other administrators, you may find it useful to share information about a host\n");
			printf("that is having problems if more than one of you may be working on it.  If you do not check the 'persistent' option, the comment will be automatically be deleted\n");
			printf("the next time Nagios is restarted.\n");
			break;

		case CMD_ADD_SVC_COMMENT:
			printf("This command is used to add a comment for the specified service.  If you work with other administrators, you may find it useful to share information about a host\n");
			printf("or service that is having problems if more than one of you may be working on it.  If you do not check the 'persistent' option, the comment will automatically be\n");
			printf("deleted the next time Nagios is restarted.\n");
			break;

		case CMD_DEL_HOST_COMMENT:
			printf("This command is used to delete a specific host comment.\n");
			break;

		case CMD_DEL_SVC_COMMENT:
			printf("This command is used to delete a specific service comment.\n");
			break;

		case CMD_DELAY_HOST_NOTIFICATION:
			printf("This command is used to delay the next problem notification that is sent out for the specified host.  The notification delay will be disregarded if\n");
			printf("the host changes state before the next notification is scheduled to be sent out.  This command has no effect if the host is currently UP.\n");
			break;

		case CMD_DELAY_SVC_NOTIFICATION:
			printf("This command is used to delay the next problem notification that is sent out for the specified service.  The notification delay will be disregarded if\n");
			printf("the service changes state before the next notification is scheduled to be sent out.  This command has no effect if the service is currently in an OK state.\n");
			break;

		case CMD_SCHEDULE_SVC_CHECK:
			printf("This command is used to schedule the next check of a particular service.  Nagios will re-queue the service to be checked at the time you specify.\n");
			printf("If you select the <i>force check</i> option, Nagios will force a check of the service regardless of both what time the scheduled check occurs and whether or not checks are enabled for the service.\n");
			break;

		case CMD_ENABLE_SVC_CHECK:
			printf("This command is used to enable active checks of a service.\n");
			break;

		case CMD_DISABLE_SVC_CHECK:
			printf("This command is used to disable active checks of a service.\n");
			break;

		case CMD_DISABLE_NOTIFICATIONS:
			printf("This command is used to disable host and service notifications on a program-wide basis.\n");
			break;

		case CMD_ENABLE_NOTIFICATIONS:
			printf("This command is used to enable host and service notifications on a program-wide basis.\n");
			break;

		case CMD_SHUTDOWN_PROCESS:
			printf("This command is used to shutdown the Nagios process. Note: Once the Nagios has been shutdown, it cannot be restarted via the web interface!\n");
			break;

		case CMD_RESTART_PROCESS:
			printf("This command is used to restart the Nagios process.   Executing a restart command is equivalent to sending the process a HUP signal.\n");
			printf("All information will be flushed from memory, the configuration files will be re-read, and Nagios will start monitoring with the new configuration information.\n");
			break;

		case CMD_ENABLE_HOST_SVC_CHECKS:
			printf("This command is used to enable active checks of all services associated with the specified host.  This <i>does not</i> enable checks of the host unless you check the 'Enable for host too' option.\n");
			break;

		case CMD_DISABLE_HOST_SVC_CHECKS:
			printf("This command is used to disable active checks of all services associated with the specified host.  When a service is disabled Nagios will not monitor the service.  Doing this will prevent any notifications being sent out for\n");
			printf("the specified service while it is disabled.  In order to have Nagios check the service in the future you will have to re-enable the service.\n");
			printf("Note that disabling service checks may not necessarily prevent notifications from being sent out about the host which those services are associated with.  This <i>does not</i> disable checks of the host unless you check the 'Disable for host too' option.\n");
			break;

		case CMD_SCHEDULE_HOST_SVC_CHECKS:
			printf("This command is used to scheduled the next check of all services on the specified host.  If you select the <i>force check</i> option, Nagios will force a check of all services on the host regardless of both what time the scheduled checks occur and whether or not checks are enabled for those services.\n");
			break;

		case CMD_DEL_ALL_HOST_COMMENTS:
			printf("This command is used to delete all comments associated with the specified host.\n");
			break;

		case CMD_DEL_ALL_SVC_COMMENTS:
			printf("This command is used to delete all comments associated with the specified service.\n");
			break;

		case CMD_ENABLE_SVC_NOTIFICATIONS:
			printf("This command is used to enable notifications for the specified service.  Notifications will only be sent out for the\n");
			printf("service state types you defined in your service definition.\n");
			break;

		case CMD_DISABLE_SVC_NOTIFICATIONS:
			printf("This command is used to prevent notifications from being sent out for the specified service.  You will have to re-enable notifications\n");
			printf("for this service before any alerts can be sent out in the future.\n");
			break;

		case CMD_ENABLE_HOST_NOTIFICATIONS:
			printf("This command is used to enable notifications for the specified host.  Notifications will only be sent out for the\n");
			printf("host state types you defined in your host definition.  Note that this command <i>does not</i> enable notifications\n");
			printf("for services associated with this host.\n");
			break;

		case CMD_DISABLE_HOST_NOTIFICATIONS:
			printf("This command is used to prevent notifications from being sent out for the specified host.  You will have to re-enable notifications for this host\n");
			printf("before any alerts can be sent out in the future.  Note that this command <i>does not</i> disable notifications for services associated with this host.\n");
			break;

		case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
			printf("This command is used to enable notifications for all hosts and services that lie \"beyond\" the specified host\n");
			printf("(from the view of Nagios).\n");
			break;

		case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
			printf("This command is used to temporarily prevent notifications from being sent out for all hosts and services that lie\n");
			printf("\"beyond\" the specified host (from the view of Nagios).\n");
			break;

		case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
			printf("This command is used to enable notifications for all services on the specified host.  Notifications will only be sent out for the\n");
			printf("service state types you defined in your service definition.  This <i>does not</i> enable notifications for the host unless you check the 'Enable for host too' option.\n");
			break;

		case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
			printf("This command is used to prevent notifications from being sent out for all services on the specified host.  You will have to re-enable notifications for\n");
			printf("all services associated with this host before any alerts can be sent out in the future.  This <i>does not</i> prevent notifications from being sent out about the host unless you check the 'Disable for host too' option.\n");
			break;

		case CMD_ACKNOWLEDGE_HOST_PROBLEM:
			printf("This command is used to acknowledge a host problem.  When a host problem is acknowledged, future notifications about problems are temporarily disabled until the host changes from its current state.\n");
			printf("If you want acknowledgement to disable notifications until the host recovers, check the 'Sticky Acknowledgement' checkbox.\n");
			printf("Contacts for this host will receive a notification about the acknowledgement, so they are aware that someone is working on the problem.  Additionally, a comment will also be added to the host.\n");
			printf("Make sure to enter your name and fill in a brief description of what you are doing in the comment field.  If you would like the host comment to remain once the acknowledgement is removed, check\n");
			printf("the 'Persistent Comment' checkbox.  If you do not want an acknowledgement notification sent out to the appropriate contacts, uncheck the 'Send Notification' checkbox.\n");
			break;

		case CMD_ACKNOWLEDGE_SVC_PROBLEM:
			printf("This command is used to acknowledge a service problem.  When a service problem is acknowledged, future notifications about problems are temporarily disabled until the service changes from its current state.\n");
			printf("If you want acknowledgement to disable notifications until the service recovers, check the 'Sticky Acknowledgement' checkbox.\n");
			printf("Contacts for this service will receive a notification about the acknowledgement, so they are aware that someone is working on the problem.  Additionally, a comment will also be added to the service.\n");
			printf("Make sure to enter your name and fill in a brief description of what you are doing in the comment field.  If you would like the service comment to remain once the acknowledgement is removed, check\n");
			printf("the 'Persistent Comment' checkbox.  If you do not want an acknowledgement notification sent out to the appropriate contacts, uncheck the 'Send Notification' checkbox.\n");
			break;

		case CMD_START_EXECUTING_SVC_CHECKS:
			printf("This command is used to resume execution of active service checks on a program-wide basis.  Individual services which are disabled will still not be checked.\n");
			break;

		case CMD_STOP_EXECUTING_SVC_CHECKS:
			printf("This command is used to temporarily stop Nagios from actively executing any service checks.  This will have the side effect of preventing any notifications from being sent out (for any and all services and hosts).\n");
			printf("Service checks will not be executed again until you issue a command to resume service check execution.\n");
			break;

		case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
			printf("This command is used to make Nagios start accepting passive service check results that it finds in the external command file\n");
			break;

		case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
			printf("This command is use to make Nagios stop accepting passive service check results that it finds in the external command file.  All passive check results that are found will be ignored.\n");
			break;

		case CMD_ENABLE_PASSIVE_SVC_CHECKS:
			printf("This command is used to allow Nagios to accept passive service check results that it finds in the external command file for this particular service.\n");
			break;

		case CMD_DISABLE_PASSIVE_SVC_CHECKS:
			printf("This command is used to stop Nagios accepting passive service check results that it finds in the external command file for this particular service.  All passive check results that are found for this service will be ignored.\n");
			break;

		case CMD_ENABLE_EVENT_HANDLERS:
			printf("This command is used to allow Nagios to run host and service event handlers.\n");
			break;

		case CMD_DISABLE_EVENT_HANDLERS:
			printf("This command is used to temporarily prevent Nagios from running any host or service event handlers.\n");
			break;

		case CMD_ENABLE_SVC_EVENT_HANDLER:
			printf("This command is used to allow Nagios to run the service event handler for a particular service when necessary (if one is defined).\n");
			break;

		case CMD_DISABLE_SVC_EVENT_HANDLER:
			printf("This command is used to temporarily prevent Nagios from running the service event handler for a particular service.\n");
			break;

		case CMD_ENABLE_HOST_EVENT_HANDLER:
			printf("This command is used to allow Nagios to run the host event handler for a particular service when necessary (if one is defined).\n");
			break;

		case CMD_DISABLE_HOST_EVENT_HANDLER:
			printf("This command is used to temporarily prevent Nagios from running the host event handler for a particular host.\n");
			break;

		case CMD_ENABLE_HOST_CHECK:
			printf("This command is used to enable active checks of this host.\n");
			break;

		case CMD_DISABLE_HOST_CHECK:
			printf("This command is used to temporarily prevent Nagios from actively checking the status of a particular host.  If Nagios needs to check the status of this host, it will assume that it is in the same state that it was in before checks were disabled.\n");
			break;

		case CMD_START_OBSESSING_OVER_SVC_CHECKS:
			printf("This command is used to have Nagios start obsessing over service checks.  Read the documentation on distributed monitoring for more information on this.\n");
			break;

		case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
			printf("This command is used stop Nagios from obsessing over service checks.\n");
			break;

		case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
			printf("This command is used to remove an acknowledgement for a particular host problem.  Once the acknowledgement is removed, notifications may start being\n");
			printf("sent out about the host problem. \n");
			break;

		case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
			printf("This command is used to remove an acknowledgement for a particular service problem.  Once the acknowledgement is removed, notifications may start being\n");
			printf("sent out about the service problem.\n");
			break;

		case CMD_PROCESS_SERVICE_CHECK_RESULT:
			printf("This command is used to submit a passive check result for a particular service.  It is particularly useful for resetting security-related services to OK states once they have been dealt with.\n");
			break;

		case CMD_PROCESS_HOST_CHECK_RESULT:
			printf("This command is used to submit a passive check result for a particular host.\n");
			break;

		case CMD_SCHEDULE_HOST_DOWNTIME:
			printf("This command is used to schedule downtime for a particular host.  During the specified downtime, Nagios will not send notifications out about the host.\n");
			printf("When the scheduled downtime expires, Nagios will send out notifications for this host as it normally would.  Scheduled downtimes are preserved\n");
			printf("across program shutdowns and restarts.  Both the start and end times should be specified in the following format:  <b>mm/dd/yyyy hh:mm:ss</b>.\n");
			printf("If you select the <i>fixed</i> option, the downtime will be in effect between the start and end times you specify.  If you do not select the <i>fixed</i>\n");
			printf("option, Nagios will treat this as \"flexible\" downtime.  Flexible downtime starts when the host goes down or becomes unreachable (sometime between the\n");
			printf("start and end times you specified) and lasts as long as the duration of time you enter.  The duration fields do not apply for fixed downtime.\n");
			break;

		case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
			printf("This command is used to schedule downtime for all services on a particular host.  During the specified downtime, Nagios will not send notifications out about the host.\n");
			printf("Normally, a host in downtime will not send alerts about any services in a failed state. This option will explicitly set downtime for all services for this host.\n");
			printf("When the scheduled downtime expires, Nagios will send out notifications for this host as it normally would.  Scheduled downtimes are preserved\n");
			printf("across program shutdowns and restarts.  Both the start and end times should be specified in the following format:  <b>mm/dd/yyyy hh:mm:ss</b>.\n");
			printf("If you select the <i>fixed</i> option, the downtime will be in effect between the start and end times you specify.  If you do not select the <i>fixed</i>\n");
			printf("option, Nagios will treat this as \"flexible\" downtime.  Flexible downtime starts when the host goes down or becomes unreachable (sometime between the\n");
			printf("start and end times you specified) and lasts as long as the duration of time you enter.  The duration fields do not apply for fixed downtime.\n");
			break;

		case CMD_SCHEDULE_SVC_DOWNTIME:
			printf("This command is used to schedule downtime for a particular service.  During the specified downtime, Nagios will not send notifications out about the service.\n");
			printf("When the scheduled downtime expires, Nagios will send out notifications for this service as it normally would.  Scheduled downtimes are preserved\n");
			printf("across program shutdowns and restarts.  Both the start and end times should be specified in the following format:  <b>mm/dd/yyyy hh:mm:ss</b>.\n");
			printf("If you select the <i>fixed</i> option, the downtime will be in effect between the start and end times you specify.  If you do not select the <i>fixed</i>\n");
			printf("option, Nagios will treat this as \"flexible\" downtime.  Flexible downtime starts when the service enters a non-OK state (sometime between the\n");
			printf("start and end times you specified) and lasts as long as the duration of time you enter.  The duration fields do not apply for fixed downtime.\n");
			break;

		case CMD_ENABLE_HOST_FLAP_DETECTION:
			printf("This command is used to enable flap detection for a specific host.  If flap detection is disabled on a program-wide basis, this will have no effect,\n");
			break;

		case CMD_DISABLE_HOST_FLAP_DETECTION:
			printf("This command is used to disable flap detection for a specific host.\n");
			break;

		case CMD_ENABLE_SVC_FLAP_DETECTION:
			printf("This command is used to enable flap detection for a specific service.  If flap detection is disabled on a program-wide basis, this will have no effect,\n");
			break;

		case CMD_DISABLE_SVC_FLAP_DETECTION:
			printf("This command is used to disable flap detection for a specific service.\n");
			break;

		case CMD_ENABLE_FLAP_DETECTION:
			printf("This command is used to enable flap detection for hosts and services on a program-wide basis.  Individual hosts and services may have flap detection disabled.\n");
			break;

		case CMD_DISABLE_FLAP_DETECTION:
			printf("This command is used to disable flap detection for hosts and services on a program-wide basis.\n");
			break;

		case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
			printf("This command is used to enable notifications for all services in the specified hostgroup.  Notifications will only be sent out for the\n");
			printf("service state types you defined in your service definitions.  This <i>does not</i> enable notifications for the hosts in this hostgroup unless you check the 'Enable for hosts too' option.\n");
			break;

		case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
			printf("This command is used to prevent notifications from being sent out for all services in the specified hostgroup.  You will have to re-enable notifications for\n");
			printf("all services in this hostgroup before any alerts can be sent out in the future.  This <i>does not</i> prevent notifications from being sent out about the hosts in this hostgroup unless you check the 'Disable for hosts too' option.\n");
			break;

		case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
			printf("This command is used to enable notifications for all hosts in the specified hostgroup.  Notifications will only be sent out for the\n");
			printf("host state types you defined in your host definitions.\n");
			break;

		case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
			printf("This command is used to prevent notifications from being sent out for all hosts in the specified hostgroup.  You will have to re-enable notifications for\n");
			printf("all hosts in this hostgroup before any alerts can be sent out in the future.\n");
			break;

		case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
			printf("This command is used to enable active checks of all services in the specified hostgroup.  This <i>does not</i> enable active checks of the hosts in the hostgroup unless you check the 'Enable for hosts too' option.\n");
			break;

		case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
			printf("This command is used to disable active checks of all services in the specified hostgroup.  This <i>does not</i> disable checks of the hosts in the hostgroup unless you check the 'Disable for hosts too' option.\n");
			break;

		case CMD_DEL_HOST_DOWNTIME:
			printf("This command is used to cancel active or pending scheduled downtime for the specified host.\n");
			break;

		case CMD_DEL_SVC_DOWNTIME:
			printf("This command is used to cancel active or pending scheduled downtime for the specified service.\n");
			break;

		case CMD_ENABLE_PERFORMANCE_DATA:
			printf("This command is used to enable the processing of performance data for hosts and services on a program-wide basis.  Individual hosts and services may have performance data processing disabled.\n");
			break;

		case CMD_DISABLE_PERFORMANCE_DATA:
			printf("This command is used to disable the processing of performance data for hosts and services on a program-wide basis.\n");
			break;

		case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
			printf("This command is used to schedule downtime for all hosts in a particular hostgroup.  During the specified downtime, Nagios will not send notifications out about the hosts.\n");
			printf("When the scheduled downtime expires, Nagios will send out notifications for the hosts as it normally would.  Scheduled downtimes are preserved\n");
			printf("across program shutdowns and restarts.  Both the start and end times should be specified in the following format:  <b>mm/dd/yyyy hh:mm:ss</b>.\n");
			printf("If you select the <i>fixed</i> option, the downtime will be in effect between the start and end times you specify.  If you do not select the <i>fixed</i>\n");
			printf("option, Nagios will treat this as \"flexible\" downtime.  Flexible downtime starts when a host goes down or becomes unreachable (sometime between the\n");
			printf("start and end times you specified) and lasts as long as the duration of time you enter.  The duration fields do not apply for fixed downtime.\n");
			break;

		case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
			printf("This command is used to schedule downtime for all services in a particular hostgroup.  During the specified downtime, Nagios will not send notifications out about the services.\n");
			printf("When the scheduled downtime expires, Nagios will send out notifications for the services as it normally would.  Scheduled downtimes are preserved\n");
			printf("across program shutdowns and restarts.  Both the start and end times should be specified in the following format:  <b>mm/dd/yyyy hh:mm:ss</b>.\n");
			printf("If you select the <i>fixed</i> option, the downtime will be in effect between the start and end times you specify.  If you do not select the <i>fixed</i>\n");
			printf("option, Nagios will treat this as \"flexible\" downtime.  Flexible downtime starts when a service enters a non-OK state (sometime between the\n");
			printf("start and end times you specified) and lasts as long as the duration of time you enter.  The duration fields do not apply for fixed downtime.\n");
			printf("Note that scheduling downtime for services does not automatically schedule downtime for the hosts those services are associated with.  If you want to also schedule downtime for all hosts in the hostgroup, check the 'Schedule downtime for hosts too' option.\n");
			break;

		case CMD_START_EXECUTING_HOST_CHECKS:
			printf("This command is used to enable active host checks on a program-wide basis.\n");
			break;

		case CMD_STOP_EXECUTING_HOST_CHECKS:
			printf("This command is used to disable active host checks on a program-wide basis.\n");
			break;

		case CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS:
			printf("This command is used to have Nagios start obsessing over host checks.  Read the documentation on distributed monitoring for more information on this.\n");
			break;

		case CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS:
			printf("This command is used to stop Nagios from obsessing over host checks.\n");
			break;

		case CMD_ENABLE_PASSIVE_HOST_CHECKS:
			printf("This command is used to allow Nagios to accept passive host check results that it finds in the external command file for a particular host.\n");
			break;

		case CMD_DISABLE_PASSIVE_HOST_CHECKS:
			printf("This command is used to stop Nagios from accepting passive host check results that it finds in the external command file for a particular host.  All passive check results that are found for this host will be ignored.\n");
			break;

		case CMD_START_OBSESSING_OVER_HOST_CHECKS:
			printf("This command is used to have Nagios start obsessing over host checks.  Read the documentation on distributed monitoring for more information on this.\n");
			break;

		case CMD_STOP_OBSESSING_OVER_HOST_CHECKS:
			printf("This command is used to stop Nagios from obsessing over host checks.\n");
			break;

		case CMD_SCHEDULE_HOST_CHECK:
			printf("This command is used to schedule the next check of a particular host.  Nagios will re-queue the host to be checked at the time you specify.\n");
			printf("If you select the <i>force check</i> option, Nagios will force a check of the host regardless of both what time the scheduled check occurs and whether or not checks are enabled for the host.\n");
			break;

		case CMD_START_OBSESSING_OVER_SVC:
			printf("This command is used to have Nagios start obsessing over a particular service.\n");
			break;

		case CMD_STOP_OBSESSING_OVER_SVC:
			printf("This command is used to stop Nagios from obsessing over a particular service.\n");
			break;

		case CMD_START_OBSESSING_OVER_HOST:
			printf("This command is used to have Nagios start obsessing over a particular host.\n");
			break;

		case CMD_STOP_OBSESSING_OVER_HOST:
			printf("This command is used to stop Nagios from obsessing over a particular host.\n");
			break;

		case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
			printf("This command is used to enable notifications for all services in the specified servicegroup.  Notifications will only be sent out for the\n");
			printf("service state types you defined in your service definitions.  This <i>does not</i> enable notifications for the hosts in this servicegroup unless you check the 'Enable for hosts too' option.\n");
			break;

		case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
			printf("This command is used to prevent notifications from being sent out for all services in the specified servicegroup.  You will have to re-enable notifications for\n");
			printf("all services in this servicegroup before any alerts can be sent out in the future.  This <i>does not</i> prevent notifications from being sent out about the hosts in this servicegroup unless you check the 'Disable for hosts too' option.\n");
			break;

		case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
			printf("This command is used to enable notifications for all hosts in the specified servicegroup.  Notifications will only be sent out for the\n");
			printf("host state types you defined in your host definitions.\n");
			break;

		case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
			printf("This command is used to prevent notifications from being sent out for all hosts in the specified servicegroup.  You will have to re-enable notifications for\n");
			printf("all hosts in this servicegroup before any alerts can be sent out in the future.\n");
			break;

		case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
			printf("This command is used to enable active checks of all services in the specified servicegroup.  This <i>does not</i> enable active checks of the hosts in the servicegroup unless you check the 'Enable for hosts too' option.\n");
			break;

		case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
			printf("This command is used to disable active checks of all services in the specified servicegroup.  This <i>does not</i> disable checks of the hosts in the servicegroup unless you check the 'Disable for hosts too' option.\n");
			break;

		case CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME:
			printf("This command is used to schedule downtime for all hosts in a particular servicegroup.  During the specified downtime, Nagios will not send notifications out about the hosts.\n");
			printf("When the scheduled downtime expires, Nagios will send out notifications for the hosts as it normally would.  Scheduled downtimes are preserved\n");
			printf("across program shutdowns and restarts.  Both the start and end times should be specified in the following format:  <b>mm/dd/yyyy hh:mm:ss</b>.\n");
			printf("If you select the <i>fixed</i> option, the downtime will be in effect between the start and end times you specify.  If you do not select the <i>fixed</i>\n");
			printf("option, Nagios will treat this as \"flexible\" downtime.  Flexible downtime starts when a host goes down or becomes unreachable (sometime between the\n");
			printf("start and end times you specified) and lasts as long as the duration of time you enter.  The duration fields do not apply for fixed downtime.\n");
			break;

		case CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME:
			printf("This command is used to schedule downtime for all services in a particular servicegroup.  During the specified downtime, Nagios will not send notifications out about the services.\n");
			printf("When the scheduled downtime expires, Nagios will send out notifications for the services as it normally would.  Scheduled downtimes are preserved\n");
			printf("across program shutdowns and restarts.  Both the start and end times should be specified in the following format:  <b>mm/dd/yyyy hh:mm:ss</b>.\n");
			printf("If you select the <i>fixed</i> option, the downtime will be in effect between the start and end times you specify.  If you do not select the <i>fixed</i>\n");
			printf("option, Nagios will treat this as \"flexible\" downtime.  Flexible downtime starts when a service enters a non-OK state (sometime between the\n");
			printf("start and end times you specified) and lasts as long as the duration of time you enter.  The duration fields do not apply for fixed downtime.\n");
			printf("Note that scheduling downtime for services does not automatically schedule downtime for the hosts those services are associated with.  If you want to also schedule downtime for all hosts in the servicegroup, check the 'Schedule downtime for hosts too' option.\n");
			break;

		case CMD_CLEAR_HOST_FLAPPING_STATE:
		case CMD_CLEAR_SVC_FLAPPING_STATE:
			printf("This command is used to reset the flapping state for the specified %s.\n",
				(cmd == CMD_CLEAR_HOST_FLAPPING_STATE) ? "host" : "service");
			printf("All state history for the specified %s will be cleared.\n",
				(cmd == CMD_CLEAR_HOST_FLAPPING_STATE) ? "host" : "service");
			break;

		case CMD_SEND_CUSTOM_HOST_NOTIFICATION:
		case CMD_SEND_CUSTOM_SVC_NOTIFICATION:
			printf("This command is used to send a custom notification about the specified %s.  Useful in emergencies when you need to notify admins of an issue regarding a monitored system or service.\n", (cmd == CMD_SEND_CUSTOM_HOST_NOTIFICATION) ? "host" : "service");
			printf("Custom notifications normally follow the regular notification logic in Nagios.  Selecting the <i>Forced</i> option will force the notification to be sent out, regardless of the time restrictions, whether or not notifications are enabled, etc.  Selecting the <i>Broadcast</i> option causes the notification to be sent out to all normal (non-escalated) and escalated contacts.  These options allow you to override the normal notification logic if you need to get an important message out.\n");
			break;

		default:
			printf("Sorry, but no information is available for this command.");
		}

	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	return;
	}



/* converts a time string to a UNIX timestamp, respecting the date_format option */
int string_to_time(char *buffer, time_t *t) {
	struct tm lt;
	int ret = 0;


	/* Initialize some variables just in case they don't get parsed
	   by the sscanf() call.  A better solution is to also check the
	   CGI input for validity, but this should suffice to prevent
	   strange problems if the input is not valid.
	   Jan 15 2003  Steve Bonds */
	lt.tm_mon = 0;
	lt.tm_mday = 1;
	lt.tm_year = 1900;
	lt.tm_hour = 0;
	lt.tm_min = 0;
	lt.tm_sec = 0;
	lt.tm_wday = 0;
	lt.tm_yday = 0;


	if(date_format == DATE_FORMAT_EURO)
		ret = sscanf(buffer, "%02d-%02d-%04d %02d:%02d:%02d", &lt.tm_mday, &lt.tm_mon, &lt.tm_year, &lt.tm_hour, &lt.tm_min, &lt.tm_sec);
	else if(date_format == DATE_FORMAT_ISO8601 || date_format == DATE_FORMAT_STRICT_ISO8601)
		ret = sscanf(buffer, "%04d-%02d-%02d%*[ T]%02d:%02d:%02d", &lt.tm_year, &lt.tm_mon, &lt.tm_mday, &lt.tm_hour, &lt.tm_min, &lt.tm_sec);
	else
		ret = sscanf(buffer, "%02d-%02d-%04d %02d:%02d:%02d", &lt.tm_mon, &lt.tm_mday, &lt.tm_year, &lt.tm_hour, &lt.tm_min, &lt.tm_sec);

	if(ret != 6)
		return ERROR;

	lt.tm_mon--;
	lt.tm_year -= 1900;

	/* tell mktime() to try and compute DST automatically */
	lt.tm_isdst = -1;

	*t = mktime(&lt);

	return OK;
	}
