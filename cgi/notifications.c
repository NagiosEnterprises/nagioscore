/************************************************************************
 *
 * NOTIFICATIONS.C - Nagios Notifications CGI
 *
 *
 * This CGI program will display the notification events for
 * a given host or contact or for all contacts/hosts.
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

#include "../include/getcgi.h"
#include "../include/cgiutils.h"
#include "../include/cgiauth.h"

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_html_path[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char url_docs_path[MAX_FILENAME_LENGTH];
extern char url_stylesheets_path[MAX_FILENAME_LENGTH];


#define FIND_HOST		1
#define FIND_CONTACT		2
#define FIND_SERVICE		3

#define MAX_QUERYNAME_LENGTH	256

#define SERVICE_NOTIFICATION	0
#define HOST_NOTIFICATION	1

#define SERVICE_NOTIFICATION_STRING 	"] SERVICE NOTIFICATION:"
#define HOST_NOTIFICATION_STRING	"] HOST NOTIFICATION:"

void display_notifications(void);

void document_header(int);
void document_footer(void);
int process_cgivars(void);

authdata current_authdata;

char log_file_to_use[MAX_FILENAME_LENGTH];
int log_archive = 0;

int query_type = FIND_HOST;
int find_all = TRUE;
char *query_contact_name = "";
char *query_host_name = "";
char *query_svc_description = "";

int notification_options = NOTIFICATION_ALL;
int use_lifo = TRUE;

int embedded = FALSE;
int display_header = TRUE;


int main(void) {
	char temp_buffer[MAX_INPUT_BUFFER];
	char temp_buffer2[MAX_INPUT_BUFFER];


	/* get the arguments passed in the URL */
	process_cgivars();

	/* reset internal variables */
	reset_cgi_vars();

	cgi_init(document_header, document_footer, READ_ALL_OBJECT_DATA, 0);

	document_header(TRUE);

	/* get authentication information */
	get_authentication_information(&current_authdata);

	/* determine what log file we should use */
	get_log_archive_to_use(log_archive, log_file_to_use, (int)sizeof(log_file_to_use));


	if(display_header == TRUE) {

		/* begin top table */
		printf("<table border=0 width=100%%>\n");
		printf("<tr>\n");

		/* left column of top row */
		printf("<td align=left valign=top width=33%%>\n");

		if(query_type == FIND_SERVICE)
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Service Notifications");
		else if(query_type == FIND_HOST) {
			if(find_all == TRUE)
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Notifications");
			else
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Host Notifications");
			}
		else
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Contact Notifications");
		display_info_table(temp_buffer, FALSE, &current_authdata);

		if(query_type == FIND_HOST || query_type == FIND_SERVICE) {
			printf("<TABLE BORDER=1 CELLPADDING=0 CELLSPACING=0 CLASS='linkBox'>\n");
			printf("<TR><TD CLASS='linkBox'>\n");
			if(query_type == FIND_HOST) {
				printf("<A HREF='%s?host=%s'>View Status Detail For %s</A><BR>\n", STATUS_CGI, (find_all == TRUE) ? "all" : url_encode(query_host_name), (find_all == TRUE) ? "All Hosts" : "This Host");
				printf("<A HREF='%s?host=%s'>View History For %s</A><BR>\n", HISTORY_CGI, (find_all == TRUE) ? "all" : url_encode(query_host_name), (find_all == TRUE) ? "All Hosts" : "This Host");
#ifdef USE_TRENDS
				if(find_all == FALSE)
					printf("<A HREF='%s?host=%s'>View Trends For This Host</A><BR>\n", TRENDS_CGI, url_encode(query_host_name));
#endif
				}
			else if(query_type == FIND_SERVICE) {
				printf("<A HREF='%s?host=%s&", HISTORY_CGI, (find_all == TRUE) ? "all" : url_encode(query_host_name));
				printf("service=%s'>View History For This Service</A><BR>\n", url_encode(query_svc_description));
#ifdef USE_TRENDS
				printf("<A HREF='%s?host=%s&", TRENDS_CGI, (find_all == TRUE) ? "all" : url_encode(query_host_name));
				printf("service=%s'>View Trends For This Service</A><BR>\n", url_encode(query_svc_description));
#endif
				}
			printf("</TD></TR>\n");
			printf("</TABLE>\n");
			}

		printf("</td>\n");


		/* middle column of top row */
		printf("<td align=center valign=top width=33%%>\n");

		printf("<DIV ALIGN=CENTER CLASS='dataTitle'>\n");
		if(query_type == FIND_SERVICE)
			printf("Service '%s' On Host '%s'", query_svc_description, query_host_name);
		else if(query_type == FIND_HOST) {
			if(find_all == TRUE)
				printf("All Hosts and Services");
			else
				printf("Host '%s'", query_host_name);
			}
		else {
			if(find_all == TRUE)
				printf("All Contacts");
			else
				printf("Contact '%s'", query_contact_name);
			}
		printf("</DIV>\n");
		printf("<BR>\n");

		if(query_type == FIND_SERVICE) {
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%s?%shost=%s&", NOTIFICATIONS_CGI, (use_lifo == FALSE) ? "oldestfirst&" : "", url_encode(query_host_name));
			snprintf(temp_buffer2, sizeof(temp_buffer2) - 1, "service=%s&type=%d&", url_encode(query_svc_description), notification_options);
			strncat(temp_buffer, temp_buffer2, sizeof(temp_buffer) - strlen(temp_buffer) - 1);
			}
		else
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%s?%s%s=%s&type=%d&", NOTIFICATIONS_CGI, (use_lifo == FALSE) ? "oldestfirst&" : "", (query_type == FIND_HOST) ? "host" : "contact", (query_type == FIND_HOST) ? url_encode(query_host_name) : url_encode(query_contact_name), notification_options);
		temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
		display_nav_table(temp_buffer, log_archive);

		printf("</td>\n");


		/* right hand column of top row */
		printf("<td align=right valign=top width=33%%>\n");

		printf("<form method='GET' action='%s'>\n", NOTIFICATIONS_CGI);
		if(query_type == FIND_SERVICE) {
			printf("<input type='hidden' name='host' value='%s'>\n", escape_string(query_host_name));
			printf("<input type='hidden' name='service' value='%s'>\n", escape_string(query_svc_description));
			}
		else
			printf("<input type='hidden' name='%s' value='%s'>\n", (query_type == FIND_HOST) ? "host" : "contact", (query_type == FIND_HOST) ? escape_string(query_host_name) : escape_string(query_contact_name));
		printf("<input type='hidden' name='archive' value='%d'>\n", log_archive);
		printf("<table border=0 CLASS='optBox'>\n");
		printf("<tr>\n");
		if(query_type == FIND_SERVICE)
			printf("<td align=left colspan=2 CLASS='optBoxItem'>Notification detail level for this service:</td>");
		else
			printf("<td align=left colspan=2 CLASS='optBoxItem'>Notification detail level for %s %s%s:</td>", (find_all == TRUE) ? "all" : "this", (query_type == FIND_HOST) ? "host" : "contact", (find_all == TRUE) ? "s" : "");
		printf("</tr>\n");
		printf("<tr>\n");
		printf("<td align=left colspan=2 CLASS='optBoxItem'><select name='type'>\n");
		printf("<option value=%d %s>All notifications\n", NOTIFICATION_ALL, (notification_options == NOTIFICATION_ALL) ? "selected" : "");
		if(query_type != FIND_SERVICE) {
			printf("<option value=%d %s>All service notifications\n", NOTIFICATION_SERVICE_ALL, (notification_options == NOTIFICATION_SERVICE_ALL) ? "selected" : "");
			printf("<option value=%d %s>All host notifications\n", NOTIFICATION_HOST_ALL, (notification_options == NOTIFICATION_HOST_ALL) ? "selected" : "");
			}
		printf("<option value=%d %s>Service custom\n", NOTIFICATION_SERVICE_CUSTOM, (notification_options == NOTIFICATION_SERVICE_CUSTOM) ? "selected" : "");
		printf("<option value=%d %s>Service acknowledgements\n", NOTIFICATION_SERVICE_ACK, (notification_options == NOTIFICATION_SERVICE_ACK) ? "selected" : "");
		printf("<option value=%d %s>Service warning\n", NOTIFICATION_SERVICE_WARNING, (notification_options == NOTIFICATION_SERVICE_WARNING) ? "selected" : "");
		printf("<option value=%d %s>Service unknown\n", NOTIFICATION_SERVICE_UNKNOWN, (notification_options == NOTIFICATION_SERVICE_UNKNOWN) ? "selected" : "");
		printf("<option value=%d %s>Service critical\n", NOTIFICATION_SERVICE_CRITICAL, (notification_options == NOTIFICATION_SERVICE_CRITICAL) ? "selected" : "");
		printf("<option value=%d %s>Service recovery\n", NOTIFICATION_SERVICE_RECOVERY, (notification_options == NOTIFICATION_SERVICE_RECOVERY) ? "selected" : "");
		printf("<option value=%d %s>Service flapping\n", NOTIFICATION_SERVICE_FLAP, (notification_options == NOTIFICATION_SERVICE_FLAP) ? "selected" : "");
		printf("<option value=%d %s>Service downtime\n", NOTIFICATION_SERVICE_DOWNTIME, (notification_options == NOTIFICATION_SERVICE_DOWNTIME) ? "selected" : "");
		if(query_type != FIND_SERVICE) {
			printf("<option value=%d %s>Host custom\n", NOTIFICATION_HOST_CUSTOM, (notification_options == NOTIFICATION_HOST_CUSTOM) ? "selected" : "");
			printf("<option value=%d %s>Host acknowledgements\n", NOTIFICATION_HOST_ACK, (notification_options == NOTIFICATION_HOST_ACK) ? "selected" : "");
			printf("<option value=%d %s>Host down\n", NOTIFICATION_HOST_DOWN, (notification_options == NOTIFICATION_HOST_DOWN) ? "selected" : "");
			printf("<option value=%d %s>Host unreachable\n", NOTIFICATION_HOST_UNREACHABLE, (notification_options == NOTIFICATION_HOST_UNREACHABLE) ? "selected" : "");
			printf("<option value=%d %s>Host recovery\n", NOTIFICATION_HOST_RECOVERY, (notification_options == NOTIFICATION_HOST_RECOVERY) ? "selected" : "");
			printf("<option value=%d %s>Host flapping\n", NOTIFICATION_HOST_FLAP, (notification_options == NOTIFICATION_HOST_FLAP) ? "selected" : "");
			printf("<option value=%d %s>Host downtime\n", NOTIFICATION_HOST_DOWNTIME, (notification_options == NOTIFICATION_HOST_DOWNTIME) ? "selected" : "");
			}
		printf("</select></td>\n");
		printf("</tr>\n");
		printf("<tr>\n");
		printf("<td align=left CLASS='optBoxItem'>Older Entries First:</td>\n");
		printf("<td></td>\n");
		printf("</tr>\n");
		printf("<tr>\n");
		printf("<td align=left valign=bottom CLASS='optBoxItem'><input type='checkbox' name='oldestfirst' %s></td>", (use_lifo == FALSE) ? "checked" : "");
		printf("<td align=right CLASS='optBoxItem'><input type='submit' value='Update'></td>\n");
		printf("</tr>\n");

		/* display context-sensitive help */
		printf("<tr><td></td><td align=right valign=bottom>\n");
		display_context_help(CONTEXTHELP_NOTIFICATIONS);
		printf("</td></tr>\n");

		printf("</table>\n");
		printf("</form>\n");

		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");

		}


	/* display notifications */
	display_notifications();

	document_footer();

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

	time(&current_time);
	get_time_string(&current_time, date_time, (int)sizeof(date_time), HTTP_DATE_TIME);
	printf("Last-Modified: %s\r\n", date_time);

	expire_time = (time_t)0L;
	get_time_string(&expire_time, date_time, (int)sizeof(date_time), HTTP_DATE_TIME);
	printf("Expires: %s\r\n", date_time);

	printf("Content-type: text/html; charset=utf-8\r\n\r\n");

	if(embedded == TRUE)
		return;

	printf("<html>\n");
	printf("<head>\n");
	printf("<link rel=\"shortcut icon\" href=\"%sfavicon.ico\" type=\"image/ico\">\n", url_images_path);
	printf("<title>\n");
	printf("Alert Notifications\n");
	printf("</title>\n");

	if(use_stylesheet == TRUE) {
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, COMMON_CSS);
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, NOTIFICATIONS_CSS);
		}

	printf("</head>\n");

	printf("<body CLASS='notifications'>\n");

	/* include user SSI header */
	include_ssi_files(NOTIFICATIONS_CGI, SSI_HEADER);

	return;
	}



void document_footer(void) {

	if(embedded == TRUE)
		return;

	/* include user SSI footer */
	include_ssi_files(NOTIFICATIONS_CGI, SSI_FOOTER);

	printf("</body>\n");
	printf("</html>\n");

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

		/* we found the host argument */
		else if(!strcmp(variables[x], "host")) {
			query_type = FIND_HOST;
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((query_host_name = strdup(variables[x])) == NULL)
				query_host_name = "";
			strip_html_brackets(query_host_name);

			if(!strcmp(query_host_name, "all"))
				find_all = TRUE;
			else
				find_all = FALSE;
			}

		/* we found the contact argument */
		else if(!strcmp(variables[x], "contact")) {
			query_type = FIND_CONTACT;
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((query_contact_name = strdup(variables[x])) == NULL)
				query_contact_name = "";
			strip_html_brackets(query_contact_name);

			if(!strcmp(query_contact_name, "all"))
				find_all = TRUE;
			else
				find_all = FALSE;
			}

		/* we found the service argument */
		else if(!strcmp(variables[x], "service")) {
			query_type = FIND_SERVICE;
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			if((query_svc_description = strdup(variables[x])) == NULL)
				query_svc_description = "";
			strip_html_brackets(query_svc_description);
			}

		/* we found the notification type argument */
		else if(!strcmp(variables[x], "type")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			notification_options = atoi(variables[x]);
			}

		/* we found the log archive argument */
		else if(!strcmp(variables[x], "archive")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			log_archive = atoi(variables[x]);
			if(log_archive < 0)
				log_archive = 0;
			}

		/* we found the order argument */
		else if(!strcmp(variables[x], "oldestfirst")) {
			use_lifo = FALSE;
			}

		/* we found the embed option */
		else if(!strcmp(variables[x], "embedded"))
			embedded = TRUE;

		/* we found the noheader option */
		else if(!strcmp(variables[x], "noheader"))
			display_header = FALSE;
		}

	/*
	 * Set some default values if not already set.
	 * Done here as they won't be set if variable
	 * not provided via cgi parameters
	 * Only required for hosts & contacts, not services
	 * as there is no service_name=all option
	 */
	if(query_type == FIND_HOST && strlen(query_host_name) == 0) {
		query_host_name = "all";
		find_all = TRUE;
		}
	if(query_type == FIND_CONTACT && strlen(query_contact_name) == 0) {
		query_contact_name = "all";
		find_all = TRUE;
		}

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return error;
	}



void display_notifications(void) {
	mmapfile *thefile = NULL;
	char *input = NULL;
	char *temp_buffer;
	char date_time[MAX_DATETIME_LENGTH];
	char alert_level[MAX_INPUT_BUFFER];
	char alert_level_class[MAX_INPUT_BUFFER];
	char contact_name[MAX_INPUT_BUFFER];
	char service_name[MAX_INPUT_BUFFER];
	char host_name[MAX_INPUT_BUFFER];
	char method_name[MAX_INPUT_BUFFER];
	int show_entry;
	int total_notifications;
	int notification_type = SERVICE_NOTIFICATION;
	int notification_detail_type = NOTIFICATION_SERVICE_CRITICAL;
	int odd = 0;
	time_t t;
	host *temp_host;
	service *temp_service;
	int result;

	if(use_lifo == TRUE) {
		result = read_file_into_lifo(log_file_to_use);
		if(result != LIFO_OK) {
			if(result == LIFO_ERROR_MEMORY) {
				printf("<P><DIV CLASS='warningMessage'>Not enough memory to reverse log file - displaying notifications in natural order...</DIV></P>");
				}
			else if(result == LIFO_ERROR_FILE) {
				printf("<P><DIV CLASS='errorMessage'>Error: Cannot open log file '%s' for reading!</DIV></P>", log_file_to_use);
				return;
				}
			use_lifo = FALSE;
			}
		}

	if(use_lifo == FALSE) {

		if((thefile = mmap_fopen(log_file_to_use)) == NULL) {
			printf("<P><DIV CLASS='errorMessage'>Error: Cannot open log file '%s' for reading!</DIV></P>", log_file_to_use);
			return;
			}
		}

	printf("<p>\n");
	printf("<div align='center'>\n");

	printf("<table border=0 CLASS='notifications'>\n");
	printf("<tr>\n");
	printf("<th CLASS='notifications'>Host</th>\n");
	printf("<th CLASS='notifications'>Service</th>\n");
	printf("<th CLASS='notifications'>Type</th>\n");
	printf("<th CLASS='notifications'>Time</th>\n");
	printf("<th CLASS='notifications'>Contact</th>\n");
	printf("<th CLASS='notifications'>Notification Command</th>\n");
	printf("<th CLASS='notifications'>Information</th>\n");
	printf("</tr>\n");

	total_notifications = 0;

	while(1) {

		free(input);

		if(use_lifo == TRUE) {
			if((input = pop_lifo()) == NULL)
				break;
			}
		else {
			if((input = mmap_fgets(thefile)) == NULL)
				break;
			}

		strip(input);

		/* see if this line contains the notification event string */
		if(strstr(input, HOST_NOTIFICATION_STRING) || strstr(input, SERVICE_NOTIFICATION_STRING)) {

			if(strstr(input, HOST_NOTIFICATION_STRING))
				notification_type = HOST_NOTIFICATION;
			else
				notification_type = SERVICE_NOTIFICATION;

			/* get the date/time */
			temp_buffer = (char *)strtok(input, "]");
			t = (time_t)(temp_buffer == NULL) ? 0L : strtoul(temp_buffer + 1, NULL, 10);
			get_time_string(&t, date_time, (int)sizeof(date_time), SHORT_DATE_TIME);
			strip(date_time);

			/* get the contact name */
			temp_buffer = (char *)strtok(NULL, ":");
			temp_buffer = (char *)strtok(NULL, ";");
			snprintf(contact_name, sizeof(contact_name), "%s", (temp_buffer == NULL) ? "" : temp_buffer + 1);
			contact_name[sizeof(contact_name) - 1] = '\x0';

			/* get the host name */
			temp_buffer = (char *)strtok(NULL, ";");
			snprintf(host_name, sizeof(host_name), "%s", (temp_buffer == NULL) ? "" : temp_buffer);
			host_name[sizeof(host_name) - 1] = '\x0';

			/* get the service name */
			if(notification_type == SERVICE_NOTIFICATION) {
				temp_buffer = (char *)strtok(NULL, ";");
				snprintf(service_name, sizeof(service_name), "%s", (temp_buffer == NULL) ? "" : temp_buffer);
				service_name[sizeof(service_name) - 1] = '\x0';
				}

			/* get the alert level */
			temp_buffer = (char *)strtok(NULL, ";");
			snprintf(alert_level, sizeof(alert_level), "%s", (temp_buffer == NULL) ? "" : temp_buffer);
			alert_level[sizeof(alert_level) - 1] = '\x0';

			if(notification_type == SERVICE_NOTIFICATION) {

				if(!strcmp(alert_level, "CRITICAL")) {
					notification_detail_type = NOTIFICATION_SERVICE_CRITICAL;
					strcpy(alert_level_class, "CRITICAL");
					}
				else if(!strcmp(alert_level, "WARNING")) {
					notification_detail_type = NOTIFICATION_SERVICE_WARNING;
					strcpy(alert_level_class, "WARNING");
					}
				else if(!strcmp(alert_level, "RECOVERY") || !strcmp(alert_level, "OK")) {
					strcpy(alert_level, "OK");
					notification_detail_type = NOTIFICATION_SERVICE_RECOVERY;
					strcpy(alert_level_class, "OK");
					}
				else if(strstr(alert_level, "CUSTOM (")) {
					notification_detail_type = NOTIFICATION_SERVICE_CUSTOM;
					strcpy(alert_level_class, "CUSTOM");
					}
				else if(strstr(alert_level, "ACKNOWLEDGEMENT (")) {
					notification_detail_type = NOTIFICATION_SERVICE_ACK;
					strcpy(alert_level_class, "ACKNOWLEDGEMENT");
					}
				else if(strstr(alert_level, "FLAPPINGSTART (")) {
					strcpy(alert_level, "FLAPPING START");
					notification_detail_type = NOTIFICATION_SERVICE_FLAP;
					strcpy(alert_level_class, "UNKNOWN");
					}
				else if(strstr(alert_level, "FLAPPINGSTOP (")) {
					strcpy(alert_level, "FLAPPING STOP");
					notification_detail_type = NOTIFICATION_SERVICE_FLAP;
					strcpy(alert_level_class, "UNKNOWN");
					}
				else if(strstr(alert_level, "DOWNTIME")) {
					notification_detail_type = NOTIFICATION_SERVICE_DOWNTIME;
					strcpy(alert_level_class, "DOWNTIME");
					}
				else {
					strcpy(alert_level, "UNKNOWN");
					notification_detail_type = NOTIFICATION_SERVICE_UNKNOWN;
					strcpy(alert_level_class, "UNKNOWN");
					}
				}

			else {

				if(!strcmp(alert_level, "DOWN")) {
					strncpy(alert_level, "HOST DOWN", sizeof(alert_level));
					strcpy(alert_level_class, "HOSTDOWN");
					notification_detail_type = NOTIFICATION_HOST_DOWN;
					}
				else if(!strcmp(alert_level, "UNREACHABLE")) {
					strncpy(alert_level, "HOST UNREACHABLE", sizeof(alert_level));
					strcpy(alert_level_class, "HOSTUNREACHABLE");
					notification_detail_type = NOTIFICATION_HOST_UNREACHABLE;
					}
				else if(!strcmp(alert_level, "RECOVERY") || !strcmp(alert_level, "UP")) {
					strncpy(alert_level, "HOST UP", sizeof(alert_level));
					strcpy(alert_level_class, "HOSTUP");
					notification_detail_type = NOTIFICATION_HOST_RECOVERY;
					}
				else if(strstr(alert_level, "CUSTOM (")) {
					strcpy(alert_level_class, "HOSTCUSTOM");
					notification_detail_type = NOTIFICATION_HOST_CUSTOM;
					}
				else if(strstr(alert_level, "ACKNOWLEDGEMENT (")) {
					strcpy(alert_level_class, "HOSTACKNOWLEDGEMENT");
					notification_detail_type = NOTIFICATION_HOST_ACK;
					}
				else if(strstr(alert_level, "FLAPPINGSTART (")) {
					strcpy(alert_level, "FLAPPING START");
					strcpy(alert_level_class, "UNKNOWN");
					notification_detail_type = NOTIFICATION_HOST_FLAP;
					}
				else if(strstr(alert_level, "FLAPPINGSTOP (")) {
					strcpy(alert_level, "FLAPPING STOP");
					strcpy(alert_level_class, "UNKNOWN");
					notification_detail_type = NOTIFICATION_HOST_FLAP;
					}
				else if(strstr(alert_level, "DOWNTIME")) {
					strcpy(alert_level_class, "DOWNTIME");
					notification_detail_type = NOTIFICATION_HOST_DOWNTIME;
					}
				}

			/* get the method name */
			temp_buffer = (char *)strtok(NULL, ";");
			snprintf(method_name, sizeof(method_name), "%s", (temp_buffer == NULL) ? "" : temp_buffer);
			method_name[sizeof(method_name) - 1] = '\x0';

			/* move to the informational message */
			temp_buffer = strtok(NULL, ";");

			show_entry = FALSE;

			/* if we're searching by contact, filter out unwanted contact */
			if(query_type == FIND_CONTACT) {
				if(find_all == TRUE)
					show_entry = TRUE;
				else if(!strcmp(query_contact_name, contact_name))
					show_entry = TRUE;
				}

			else if(query_type == FIND_HOST) {
				if(find_all == TRUE)
					show_entry = TRUE;
				else if(!strcmp(query_host_name, host_name))
					show_entry = TRUE;
				}

			else if(query_type == FIND_SERVICE) {
				if(!strcmp(query_host_name, host_name) && !strcmp(query_svc_description, service_name))
					show_entry = TRUE;
				}

			if(show_entry == TRUE) {
				if(notification_options == NOTIFICATION_ALL)
					show_entry = TRUE;
				else if(notification_options == NOTIFICATION_HOST_ALL && notification_type == HOST_NOTIFICATION)
					show_entry = TRUE;
				else if(notification_options == NOTIFICATION_SERVICE_ALL && notification_type == SERVICE_NOTIFICATION)
					show_entry = TRUE;
				else if(notification_detail_type & notification_options)
					show_entry = TRUE;
				else
					show_entry = FALSE;
				}

			/* make sure user has authorization to view this notification */
			if(notification_type == HOST_NOTIFICATION) {
				temp_host = find_host(host_name);
				if(is_authorized_for_host(temp_host, &current_authdata) == FALSE)
					show_entry = FALSE;
				}
			else {
				temp_service = find_service(host_name, service_name);
				if(is_authorized_for_service(temp_service, &current_authdata) == FALSE)
					show_entry = FALSE;
				}

			if(show_entry == TRUE) {

				total_notifications++;

				if(odd) {
					odd = 0;
					printf("<tr CLASS='notificationsOdd'>\n");
					}
				else {
					odd = 1;
					printf("<tr CLASS='notificationsEven'>\n");
					}
				printf("<td CLASS='notifications%s'><a href='%s?type=%d&host=%s'>%s</a></td>\n", (odd) ? "Even" : "Odd", EXTINFO_CGI, DISPLAY_HOST_INFO, url_encode(host_name), host_name);
				if(notification_type == SERVICE_NOTIFICATION) {
					printf("<td CLASS='notifications%s'><a href='%s?type=%d&host=%s", (odd) ? "Even" : "Odd", EXTINFO_CGI, DISPLAY_SERVICE_INFO, url_encode(host_name));
					printf("&service=%s'>%s</a></td>\n", url_encode(service_name), service_name);
					}
				else
					printf("<td CLASS='notifications%s'>N/A</td>\n", (odd) ? "Even" : "Odd");
				printf("<td CLASS='notifications%s'>%s</td>\n", alert_level_class, alert_level);
				printf("<td CLASS='notifications%s'>%s</td>\n", (odd) ? "Even" : "Odd", date_time);
				printf("<td CLASS='notifications%s'><a href='%s?type=contacts#%s'>%s</a></td>\n", (odd) ? "Even" : "Odd", CONFIG_CGI, url_encode(contact_name), contact_name);
				printf("<td CLASS='notifications%s'><a href='%s?type=commands#%s'>%s</a></td>\n", (odd) ? "Even" : "Odd", CONFIG_CGI, url_encode(method_name), method_name);
				printf("<td CLASS='notifications%s'>%s</td>\n", (odd) ? "Even" : "Odd", html_encode(temp_buffer, FALSE));
				printf("</tr>\n");
				}
			}
		}


	printf("</table>\n");

	printf("</div>\n");
	printf("</p>\n");

	if(total_notifications == 0) {
		printf("<P><DIV CLASS='errorMessage'>No notifications have been recorded");
		if(find_all == FALSE) {
			if(query_type == FIND_SERVICE)
				printf(" for this service");
			else if(query_type == FIND_CONTACT)
				printf(" for this contact");
			else
				printf(" for this host");
			}
		printf(" in %s log file</DIV></P>", (log_archive == 0) ? "the current" : "this archived");
		}

	free(input);

	if(use_lifo == TRUE)
		free_lifo_memory();
	else
		mmap_fclose(thefile);

	return;
	}
