/**************************************************************************
 *
 * HISTOGRAM.C -  Nagios Alert Histogram CGI
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

#include <gd.h>			/* Boutell's GD library function */
#include <gdfonts.h>		/* GD library small font definition */


/*#define DEBUG			1*/


#define HISTOGRAM_IMAGE         "histogram.png"

/* archived state types */
#define AS_NO_DATA              0
#define AS_PROGRAM_START        1
#define AS_PROGRAM_END          2
#define AS_HOST_UP		3
#define AS_HOST_DOWN		4
#define AS_HOST_UNREACHABLE	5
#define AS_SVC_OK		6
#define AS_SVC_UNKNOWN		7
#define AS_SVC_WARNING		8
#define AS_SVC_CRITICAL		9


/* display types */
#define DISPLAY_HOST_HISTOGRAM	        0
#define DISPLAY_SERVICE_HISTOGRAM	1
#define DISPLAY_NO_HISTOGRAM    	2

/* input types */
#define GET_INPUT_NONE                  0
#define GET_INPUT_TARGET_TYPE           1
#define GET_INPUT_HOST_TARGET           2
#define GET_INPUT_SERVICE_TARGET        3
#define GET_INPUT_OPTIONS               4

/* breakdown types */
#define BREAKDOWN_MONTHLY       0
#define BREAKDOWN_DAY_OF_MONTH  1
#define BREAKDOWN_DAY_OF_WEEK   2
#define BREAKDOWN_HOURLY        3

/* modes */
#define CREATE_HTML		0
#define CREATE_IMAGE		1

/* standard report times */
#define TIMEPERIOD_CUSTOM	0
#define TIMEPERIOD_TODAY	1
#define TIMEPERIOD_YESTERDAY	2
#define TIMEPERIOD_THISWEEK	3
#define TIMEPERIOD_LASTWEEK	4
#define TIMEPERIOD_THISMONTH	5
#define TIMEPERIOD_LASTMONTH	6
#define TIMEPERIOD_THISQUARTER	7
#define TIMEPERIOD_LASTQUARTER	8
#define TIMEPERIOD_THISYEAR	9
#define TIMEPERIOD_LASTYEAR	10
#define TIMEPERIOD_LAST24HOURS	11
#define TIMEPERIOD_LAST7DAYS	12
#define TIMEPERIOD_LAST31DAYS   13


#define MAX_ARCHIVE_SPREAD	65
#define MAX_ARCHIVE		65
#define MAX_ARCHIVE_BACKTRACKS	60

#define DRAWING_WIDTH	        550
#define DRAWING_HEIGHT	        195
#define DRAWING_X_OFFSET	60
#define DRAWING_Y_OFFSET        235

#define GRAPH_HOST_UP                   1
#define GRAPH_HOST_DOWN                 2
#define GRAPH_HOST_UNREACHABLE          4
#define GRAPH_SERVICE_OK                8
#define GRAPH_SERVICE_WARNING           16
#define GRAPH_SERVICE_UNKNOWN           32
#define GRAPH_SERVICE_CRITICAL          64

#define GRAPH_HOST_PROBLEMS             6
#define GRAPH_HOST_ALL                  7

#define GRAPH_SERVICE_PROBLEMS          112
#define GRAPH_SERVICE_ALL               120

#define GRAPH_EVERYTHING                255


#define GRAPH_SOFT_STATETYPES           1
#define GRAPH_HARD_STATETYPES           2
#define GRAPH_ALL_STATETYPES            3




extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_html_path[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char url_stylesheets_path[MAX_FILENAME_LENGTH];
extern char physical_images_path[MAX_FILENAME_LENGTH];
extern char *status_file;

authdata current_authdata;


typedef struct timeslice_data_struct {
	unsigned long    service_ok;
	unsigned long    host_up;
	unsigned long    service_critical;
	unsigned long    host_down;
	unsigned long    service_unknown;
	unsigned long    host_unreachable;
	unsigned long    service_warning;
	} timeslice_data;


timeslice_data *tsdata;


void convert_timeperiod_to_times(int);
void compute_report_times(void);
void graph_all_histogram_data(void);
void add_archived_state(int, time_t);
void read_archived_state_data(void);
void scan_log_file_for_archived_state_data(char *);
void draw_line(int, int, int, int, int);
void draw_dashed_line(int, int, int, int, int);

void document_header(int);
void document_footer(void);
int process_cgivars(void);


time_t t1;
time_t t2;

int start_second = 0;
int start_minute = 0;
int start_hour = 0;
int start_day = 1;
int start_month = 1;
int start_year = 2000;
int end_second = 0;
int end_minute = 0;
int end_hour = 24;
int end_day = 1;
int end_month = 1;
int end_year = 2000;

int display_type = DISPLAY_NO_HISTOGRAM;
int mode = CREATE_HTML;
int input_type = GET_INPUT_NONE;
int timeperiod_type = TIMEPERIOD_LAST24HOURS;
int breakdown_type = BREAKDOWN_HOURLY;
int compute_time_from_parts = FALSE;

int initial_states_logged = FALSE;
int assume_state_retention = TRUE;
int new_states_only = FALSE;

int last_state = AS_NO_DATA;
int program_restart_has_occurred = FALSE;

int graph_events = GRAPH_EVERYTHING;
int graph_statetypes = GRAPH_HARD_STATETYPES;

int embedded = FALSE;
int display_header = TRUE;

char *host_name = "";
char *svc_description = "";

gdImagePtr histogram_image = 0;
int color_white = 0;
int color_black = 0;
int color_red = 0;
int color_darkred = 0;
int color_green = 0;
int color_yellow = 0;
int color_orange = 0;
int color_lightgray = 0;
FILE *image_file = NULL;

int backtrack_archives = 0;
int earliest_archive = 0;
time_t earliest_time;
time_t latest_time;

int image_width = 900;
int image_height = 320;

int total_buckets = 96;



int main(int argc, char **argv) {
	int result = OK;
	char temp_buffer[MAX_INPUT_BUFFER];
	char image_template[MAX_INPUT_BUFFER];
	char start_timestring[MAX_INPUT_BUFFER];
	char end_timestring[MAX_INPUT_BUFFER];
	host *temp_host;
	service *temp_service;
	int is_authorized = TRUE;
	int found = FALSE;
	int days, hours, minutes, seconds;
	char *first_service = NULL;
	int x;
	time_t t3;
	time_t current_time;
	struct tm *t;

	/* initialize time period to last 24 hours */
	time(&t2);
	t1 = (time_t)(t2 - (60 * 60 * 24));

	/* get the arguments passed in the URL */
	process_cgivars();

	/* reset internal CGI variables */
	reset_cgi_vars();

	/* Initialize shared configuration variables */                             
	init_shared_cfg_vars(1);

	/* read the CGI configuration file */
	result = read_cgi_config_file(get_cgi_config_location());
	if(result == ERROR) {
		if(mode == CREATE_HTML) {
			document_header(FALSE);
			cgi_config_file_error(get_cgi_config_location());
			document_footer();
			}
		return ERROR;
		}

	/* read the main configuration file */
	result = read_main_config_file(main_config_file);
	if(result == ERROR) {
		if(mode == CREATE_HTML) {
			document_header(FALSE);
			main_config_file_error(main_config_file);
			document_footer();
			}
		return ERROR;
		}

	/* read all object configuration data */
	result = read_all_object_configuration_data(main_config_file, READ_ALL_OBJECT_DATA);
	if(result == ERROR) {
		if(mode == CREATE_HTML) {
			document_header(FALSE);
			object_data_error();
			document_footer();
			}
		return ERROR;
		}

	/* read all status data */
	result = read_all_status_data(status_file, READ_ALL_STATUS_DATA);
	if(result == ERROR) {
		if(mode == CREATE_HTML) {
			document_header(FALSE);
			status_data_error();
			document_footer();
			}
		free_memory();
		return ERROR;
		}

	document_header(TRUE);

	/* get authentication information */
	get_authentication_information(&current_authdata);

	if(compute_time_from_parts == TRUE)
		compute_report_times();

	/* make sure times are sane, otherwise swap them */
	if(t2 < t1) {
		t3 = t2;
		t2 = t1;
		t1 = t3;
		}


	if(mode == CREATE_HTML && display_header == TRUE) {

		/* begin top table */
		printf("<table border=0 width=100%% cellspacing=0 cellpadding=0>\n");
		printf("<tr>\n");

		/* left column of the first row */
		printf("<td align=left valign=top width=33%%>\n");

		if(display_type == DISPLAY_HOST_HISTOGRAM)
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Host Alert Histogram");
		else if(display_type == DISPLAY_SERVICE_HISTOGRAM)
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Service Alert Histogram");
		else
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Host and Service Alert Histogram");
		temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
		display_info_table(temp_buffer, FALSE, &current_authdata);

		if(display_type != DISPLAY_NO_HISTOGRAM && input_type == GET_INPUT_NONE) {

			printf("<TABLE BORDER=1 CELLPADDING=0 CELLSPACING=0 CLASS='linkBox'>\n");
			printf("<TR><TD CLASS='linkBox'>\n");

			if(display_type == DISPLAY_HOST_HISTOGRAM) {
#ifdef USE_TRENDS
				printf("<a href='%s?host=%s&t1=%lu&t2=%lu&assumestateretention=%s'>View Trends For This Host</a><BR>\n", TRENDS_CGI, url_encode(host_name), t1, t2, (assume_state_retention == TRUE) ? "yes" : "no");
#endif
				printf("<a href='%s?host=%s&t1=%lu&t2=%lu&assumestateretention=%s&show_log_entries'>View Availability Report For This Host</a><BR>\n", AVAIL_CGI, url_encode(host_name), t1, t2, (assume_state_retention == TRUE) ? "yes" : "no");
				printf("<a href='%s?host=%s'>View Status Detail For This Host</a><BR>\n", STATUS_CGI, url_encode(host_name));
				printf("<a href='%s?host=%s'>View History For This Host</a><BR>\n", HISTORY_CGI, url_encode(host_name));
				printf("<a href='%s?host=%s'>View Notifications For This Host</a><BR>\n", NOTIFICATIONS_CGI, url_encode(host_name));
				}
			else {
#ifdef USE_TRENDS
				printf("<a href='%s?host=%s", TRENDS_CGI, url_encode(host_name));
#endif
				printf("&service=%s&t1=%lu&t2=%lu&assumestateretention=%s'>View Trends For This Service</a><BR>\n", url_encode(svc_description), t1, t2, (assume_state_retention == TRUE) ? "yes" : "no");
				printf("<a href='%s?host=%s", AVAIL_CGI, url_encode(host_name));
				printf("&service=%s&t1=%lu&t2=%lu&assumestateretention=%s&show_log_entries'>View Availability Report For This Service</a><BR>\n", url_encode(svc_description), t1, t2, (assume_state_retention == TRUE) ? "yes" : "no");
				printf("<A HREF='%s?host=%s&", HISTORY_CGI, url_encode(host_name));
				printf("service=%s'>View History For This Service</A><BR>\n", url_encode(svc_description));
				printf("<A HREF='%s?host=%s&", NOTIFICATIONS_CGI, url_encode(host_name));
				printf("service=%s'>View Notifications For This Service</A><BR>\n", url_encode(svc_description));
				}

			printf("</TD></TR>\n");
			printf("</TABLE>\n");
			}

		printf("</td>\n");

		/* center column of top row */
		printf("<td align=center valign=top width=33%%>\n");

		if(display_type != DISPLAY_NO_HISTOGRAM && input_type == GET_INPUT_NONE) {

			printf("<DIV ALIGN=CENTER CLASS='dataTitle'>\n");
			if(display_type == DISPLAY_HOST_HISTOGRAM)
				printf("Host '%s'", host_name);
			else if(display_type == DISPLAY_SERVICE_HISTOGRAM)
				printf("Service '%s' On Host '%s'", svc_description, host_name);
			printf("</DIV>\n");

			printf("<BR>\n");

			printf("<IMG SRC='%s%s' BORDER=0 ALT='%s Event Histogram' TITLE='%s Event Histogram'>\n", url_images_path, TRENDS_ICON, (display_type == DISPLAY_HOST_HISTOGRAM) ? "Host" : "Service", (display_type == DISPLAY_HOST_HISTOGRAM) ? "Host" : "Service");

			printf("<BR CLEAR=ALL>\n");

			get_time_string(&t1, start_timestring, sizeof(start_timestring) - 1, SHORT_DATE_TIME);
			get_time_string(&t2, end_timestring, sizeof(end_timestring) - 1, SHORT_DATE_TIME);
			printf("<div align=center class='reportRange'>%s to %s</div>\n", start_timestring, end_timestring);

			get_time_breakdown((time_t)(t2 - t1), &days, &hours, &minutes, &seconds);
			printf("<div align=center class='reportDuration'>Duration: %dd %dh %dm %ds</div>\n", days, hours, minutes, seconds);
			}

		printf("</td>\n");

		/* right hand column of top row */
		printf("<td align=right valign=bottom width=33%%>\n");

#ifdef LEGACY_GRAPHICAL_CGIS
		printf("<form method=\"GET\" action=\"%s\">\n", HISTOGRAM_CGI);
#else
		printf("<form method=\"GET\" action=\"%s\">\n", LEGACY_HISTOGRAM_CGI);
#endif
		printf("<table border=0 CLASS='optBox'>\n");

		if(display_type != DISPLAY_NO_HISTOGRAM && input_type == GET_INPUT_NONE) {

			printf("<tr><td CLASS='optBoxItem' valign=top align=left>Report period:</td><td CLASS='optBoxItem' valign=top align=left>Assume state retention:</td></tr>\n");
			printf("<tr><td CLASS='optBoxItem' valign=top align=left>\n");

			printf("<input type='hidden' name='t1' value='%lu'>\n", (unsigned long)t1);
			printf("<input type='hidden' name='t2' value='%lu'>\n", (unsigned long)t2);
			printf("<input type='hidden' name='host' value='%s'>\n", escape_string(host_name));
			if(display_type == DISPLAY_SERVICE_HISTOGRAM)
				printf("<input type='hidden' name='service' value='%s'>\n", escape_string(svc_description));

			printf("<select name='timeperiod'>\n");
			printf("<option value=custom>[ Current time range ]\n");
			printf("<option value=today %s>Today\n", (timeperiod_type == TIMEPERIOD_TODAY) ? "SELECTED" : "");
			printf("<option value=last24hours %s>Last 24 Hours\n", (timeperiod_type == TIMEPERIOD_LAST24HOURS) ? "SELECTED" : "");
			printf("<option value=yesterday %s>Yesterday\n", (timeperiod_type == TIMEPERIOD_YESTERDAY) ? "SELECTED" : "");
			printf("<option value=thisweek %s>This Week\n", (timeperiod_type == TIMEPERIOD_THISWEEK) ? "SELECTED" : "");
			printf("<option value=last7days %s>Last 7 Days\n", (timeperiod_type == TIMEPERIOD_LAST7DAYS) ? "SELECTED" : "");
			printf("<option value=lastweek %s>Last Week\n", (timeperiod_type == TIMEPERIOD_LASTWEEK) ? "SELECTED" : "");
			printf("<option value=thismonth %s>This Month\n", (timeperiod_type == TIMEPERIOD_THISMONTH) ? "SELECTED" : "");
			printf("<option value=last31days %s>Last 31 Days\n", (timeperiod_type == TIMEPERIOD_LAST31DAYS) ? "SELECTED" : "");
			printf("<option value=lastmonth %s>Last Month\n", (timeperiod_type == TIMEPERIOD_LASTMONTH) ? "SELECTED" : "");
			printf("<option value=thisyear %s>This Year\n", (timeperiod_type == TIMEPERIOD_THISYEAR) ? "SELECTED" : "");
			printf("<option value=lastyear %s>Last Year\n", (timeperiod_type == TIMEPERIOD_LASTYEAR) ? "SELECTED" : "");
			printf("</select>\n");
			printf("</td><td CLASS='optBoxItem' valign=top align=left>\n");
			printf("<select name='assumestateretention'>\n");
			printf("<option value=yes %s>yes\n", (assume_state_retention == TRUE) ? "SELECTED" : "");
			printf("<option value=no %s>no\n", (assume_state_retention == TRUE) ? "" : "SELECTED");
			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td CLASS='optBoxItem' valign=top align=left>Breakdown type:</td><td CLASS='optBoxItem' valign=top align=left>Initial states logged:</td></tr>\n");
			printf("<tr><td CLASS='optBoxItem' valign=top align=left>\n");
			printf("<select name='breakdown'>\n");
			printf("<option value=monthly %s>Month\n", (breakdown_type == BREAKDOWN_MONTHLY) ? "SELECTED" : "");
			printf("<option value=dayofmonth %s>Day of the Month\n", (breakdown_type == BREAKDOWN_DAY_OF_MONTH) ? "SELECTED" : "");
			printf("<option value=dayofweek %s>Day of the Week\n", (breakdown_type == BREAKDOWN_DAY_OF_WEEK) ? "SELECTED" : "");
			printf("<option value=hourly %s>Hour of the Day\n", (breakdown_type == BREAKDOWN_HOURLY) ? "SELECTED" : "");
			printf("</select>\n");
			printf("</td><td CLASS='optBoxItem' valign=top align=left>\n");
			printf("<select name='initialstateslogged'>\n");
			printf("<option value=yes %s>yes\n", (initial_states_logged == TRUE) ? "SELECTED" : "");
			printf("<option value=no %s>no\n", (initial_states_logged == TRUE) ? "" : "SELECTED");
			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td CLASS='optBoxItem' valign=top align=left>Events to graph:</td><td CLASS='optBoxItem' valign=top align=left>Ignore repeated states:</td></tr>\n");
			printf("<tr><td CLASS='optBoxItem' valign=top align=left>\n");
			printf("<select name='graphevents'>\n");
			if(display_type == DISPLAY_HOST_HISTOGRAM) {
				printf("<option value=%d %s>All host events\n", GRAPH_HOST_ALL, (graph_events == GRAPH_HOST_ALL) ? "SELECTED" : "");
				printf("<option value=%d %s>Host problem events\n", GRAPH_HOST_PROBLEMS, (graph_events == GRAPH_HOST_PROBLEMS) ? "SELECTED" : "");
				printf("<option value=%d %s>Host up events\n", GRAPH_HOST_UP, (graph_events == GRAPH_HOST_UP) ? "SELECTED" : "");
				printf("<option value=%d %s>Host down events\n", GRAPH_HOST_DOWN, (graph_events == GRAPH_HOST_DOWN) ? "SELECTED" : "");
				printf("<option value=%d %s>Host unreachable events\n", GRAPH_HOST_UNREACHABLE, (graph_events == GRAPH_HOST_UNREACHABLE) ? "SELECTED" : "");
				}
			else {
				printf("<option value=%d %s>All service events\n", GRAPH_SERVICE_ALL, (graph_events == GRAPH_SERVICE_ALL) ? "SELECTED" : "");
				printf("<option value=%d %s>Service problem events\n", GRAPH_SERVICE_PROBLEMS, (graph_events == GRAPH_SERVICE_PROBLEMS) ? "SELECTED" : "");
				printf("<option value=%d %s>Service ok events\n", GRAPH_SERVICE_OK, (graph_events == GRAPH_SERVICE_OK) ? "SELECTED" : "");
				printf("<option value=%d %s>Service warning events\n", GRAPH_SERVICE_WARNING, (graph_events == GRAPH_SERVICE_WARNING) ? "SELECTED" : "");
				printf("<option value=%d %s>Service unknown events\n", GRAPH_SERVICE_UNKNOWN, (graph_events == GRAPH_SERVICE_UNKNOWN) ? "SELECTED" : "");
				printf("<option value=%d %s>Service critical events\n", GRAPH_SERVICE_CRITICAL, (graph_events == GRAPH_SERVICE_CRITICAL) ? "SELECTED" : "");
				}
			printf("</select>\n");
			printf("</td><td CLASS='optBoxItem' valign=top align=left>\n");
			printf("<select name='newstatesonly'>\n");
			printf("<option value=yes %s>yes\n", (new_states_only == TRUE) ? "SELECTED" : "");
			printf("<option value=no %s>no\n", (new_states_only == TRUE) ? "" : "SELECTED");
			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td CLASS='optBoxItem' valign=top align=left>State types to graph:</td><td CLASS='optBoxItem' valign=top align=left></td></tr>\n");
			printf("<tr><td CLASS='optBoxItem' valign=top align=left>\n");
			printf("<select name='graphstatetypes'>\n");
			printf("<option value=%d %s>Hard states\n", GRAPH_HARD_STATETYPES, (graph_statetypes == GRAPH_HARD_STATETYPES) ? "SELECTED" : "");
			printf("<option value=%d %s>Soft states\n", GRAPH_SOFT_STATETYPES, (graph_statetypes == GRAPH_SOFT_STATETYPES) ? "SELECTED" : "");
			printf("<option value=%d %s>Hard and soft states\n", GRAPH_ALL_STATETYPES, (graph_statetypes == GRAPH_ALL_STATETYPES) ? "SELECTED" : "");
			printf("</select>\n");
			printf("</td><td CLASS='optBoxItem' valign=top align=left>\n");
			printf("<input type='submit' value='Update'>\n");
			printf("</td></tr>\n");
			}

		/* display context-sensitive help */
		printf("<tr><td></td><td align=right valign=bottom>\n");
		if(display_type != DISPLAY_NO_HISTOGRAM && input_type == GET_INPUT_NONE) {
			if(display_type == DISPLAY_HOST_HISTOGRAM)
				display_context_help(CONTEXTHELP_HISTOGRAM_HOST);
			else
				display_context_help(CONTEXTHELP_HISTOGRAM_SERVICE);
			}
		else if(display_type == DISPLAY_NO_HISTOGRAM || input_type != GET_INPUT_NONE) {
			if(input_type == GET_INPUT_NONE)
				display_context_help(CONTEXTHELP_HISTOGRAM_MENU1);
			else if(input_type == GET_INPUT_TARGET_TYPE)
				display_context_help(CONTEXTHELP_HISTOGRAM_MENU1);
			else if(input_type == GET_INPUT_HOST_TARGET)
				display_context_help(CONTEXTHELP_HISTOGRAM_MENU2);
			else if(input_type == GET_INPUT_SERVICE_TARGET)
				display_context_help(CONTEXTHELP_HISTOGRAM_MENU3);
			else if(input_type == GET_INPUT_OPTIONS)
				display_context_help(CONTEXTHELP_HISTOGRAM_MENU4);
			}
		printf("</td></tr>\n");

		printf("</table>\n");
		printf("</form>\n");

		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");
		}

	/* check authorization... */
	if(display_type == DISPLAY_HOST_HISTOGRAM) {
		temp_host = find_host(host_name);
		if(temp_host == NULL || is_authorized_for_host(temp_host, &current_authdata) == FALSE)
			is_authorized = FALSE;
		}
	else if(display_type == DISPLAY_SERVICE_HISTOGRAM) {
		temp_service = find_service(host_name, svc_description);
		if(temp_service == NULL || is_authorized_for_service(temp_service, &current_authdata) == FALSE)
			is_authorized = FALSE;
		}
	if(is_authorized == FALSE) {

		if(mode == CREATE_HTML)
			printf("<P><DIV ALIGN=CENTER CLASS='errorMessage'>It appears as though you are not authorized to view information for the specified %s...</DIV></P>\n", (display_type == DISPLAY_HOST_HISTOGRAM) ? "host" : "service");

		document_footer();
		free_memory();
		return ERROR;
		}

	if(display_type != DISPLAY_NO_HISTOGRAM && input_type == GET_INPUT_NONE) {

		/* print URL to image */
		if(mode == CREATE_HTML) {

			printf("<BR><BR>\n");
			printf("<DIV ALIGN=CENTER>\n");
#ifdef LEGACY_GRAPHICAL_CGIS
			printf("<IMG SRC='%s?createimage&t1=%lu&t2=%lu", HISTOGRAM_CGI, (unsigned long)t1, (unsigned long)t2);
#else
			printf("<IMG SRC='%s?createimage&t1=%lu&t2=%lu", LEGACY_HISTOGRAM_CGI, (unsigned long)t1, (unsigned long)t2);
#endif
			printf("&host=%s", url_encode(host_name));
			if(display_type == DISPLAY_SERVICE_HISTOGRAM)
				printf("&service=%s", url_encode(svc_description));
			printf("&breakdown=");
			if(breakdown_type == BREAKDOWN_MONTHLY)
				printf("monthly");
			else if(breakdown_type == BREAKDOWN_DAY_OF_MONTH)
				printf("dayofmonth");
			else if(breakdown_type == BREAKDOWN_DAY_OF_WEEK)
				printf("dayofweek");
			else
				printf("hourly");
			printf("&assumestateretention=%s", (assume_state_retention == TRUE) ? "yes" : "no");
			printf("&initialstateslogged=%s", (initial_states_logged == TRUE) ? "yes" : "no");
			printf("&newstatesonly=%s", (new_states_only == TRUE) ? "yes" : "no");
			printf("&graphevents=%d", graph_events);
			printf("&graphstatetypes=%d", graph_statetypes);
			printf("' BORDER=0 name='histogramimage'>\n");
			printf("</DIV>\n");
			}

		/* read and process state data */
		else {

			/* allocate memory */
			tsdata = NULL;
			if(breakdown_type == BREAKDOWN_MONTHLY)
				total_buckets = 12;
			else if(breakdown_type == BREAKDOWN_DAY_OF_MONTH)
				total_buckets = 31;
			else if(breakdown_type == BREAKDOWN_DAY_OF_WEEK)
				total_buckets = 7;
			else
				total_buckets = 96;

			tsdata = (timeslice_data *)malloc(sizeof(timeslice_data) * total_buckets);
			if(tsdata == NULL)
				return ERROR;

			for(x = 0; x < total_buckets; x++) {
				tsdata[x].service_ok = 0L;
				tsdata[x].service_unknown = 0L;
				tsdata[x].service_warning = 0L;
				tsdata[x].service_critical = 0L;
				tsdata[x].host_up = 0L;
				tsdata[x].host_down = 0L;
				tsdata[x].host_unreachable = 0L;
				}

			/* read in all necessary archived state data */
			read_archived_state_data();

#ifdef DEBUG
			printf("Done reading archived state data.\n");
#endif

			/* location of image template */
			snprintf(image_template, sizeof(image_template) - 1, "%s/%s", physical_images_path, HISTOGRAM_IMAGE);
			image_template[sizeof(image_template) - 1] = '\x0';

			/* allocate buffer for storing image */
			image_file = fopen(image_template, "r");
			if(image_file != NULL) {
				histogram_image = gdImageCreateFromPng(image_file);
				fclose(image_file);
				}
			if(histogram_image == NULL)
				histogram_image = gdImageCreate(image_width, image_height);
			if(histogram_image == NULL) {
#ifdef DEBUG
				printf("Error: Could not allocate memory for image\n");
#endif
				return ERROR;
				}

			/* allocate colors used for drawing */
			color_white = gdImageColorAllocate(histogram_image, 255, 255, 255);
			color_black = gdImageColorAllocate(histogram_image, 0, 0, 0);
			color_red = gdImageColorAllocate(histogram_image, 255, 0, 0);
			color_darkred = gdImageColorAllocate(histogram_image, 128, 0, 0);
			color_green = gdImageColorAllocate(histogram_image, 0, 128, 0);
			color_yellow = gdImageColorAllocate(histogram_image, 176, 178, 20);
			color_orange = gdImageColorAllocate(histogram_image, 255, 100, 25);
			color_lightgray = gdImageColorAllocate(histogram_image, 192, 192, 192);

			/* set transparency index */
			gdImageColorTransparent(histogram_image, color_white);

			/* make sure the graphic is interlaced */
			gdImageInterlace(histogram_image, 1);

#ifdef DEBUG
			printf("Starting to graph data...\n");
#endif

			/* graph archived state histogram data */
			graph_all_histogram_data();

#ifdef DEBUG
			printf("Done graphing data.\n");
#endif

			/* use STDOUT for writing the image data... */
			image_file = stdout;

#ifdef DEBUG
			image_file = fopen("/tmp/histogram.png", "w");
#endif

			/* write the image to to STDOUT */
			gdImagePng(histogram_image, image_file);

#ifdef DEBUG
			fclose(image_file);
#endif

			/* free memory allocated to image */
			gdImageDestroy(histogram_image);

			/* free memory allocated for data */
			free(tsdata);
			}
		}


	/* show user a selection of hosts and services to choose from... */
	if(display_type == DISPLAY_NO_HISTOGRAM || input_type != GET_INPUT_NONE) {

		/* ask the user for what host they want a report for */
		if(input_type == GET_INPUT_HOST_TARGET) {

			printf("<P><DIV ALIGN=CENTER>\n");
			printf("<DIV CLASS='reportSelectTitle'>Step 2: Select Host</DIV>\n");
			printf("</DIV></P>\n");

			printf("<P><DIV ALIGN=CENTER>\n");

#ifdef LEGACY_GRAPHICAL_CGIS
			printf("<form method=\"GET\" action=\"%s\">\n", HISTOGRAM_CGI);
#else
			printf("<form method=\"GET\" action=\"%s\">\n", LEGACY_HISTOGRAM_CGI);
#endif
			printf("<input type='hidden' name='input' value='getoptions'>\n");

			printf("<TABLE BORDER=0 cellspacing=0 cellpadding=10>\n");
			printf("<tr><td class='reportSelectSubTitle' valign=center>Host:</td>\n");
			printf("<td class='reportSelectItem' valign=center>\n");
			printf("<select name='host'>\n");

			for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
				if(is_authorized_for_host(temp_host, &current_authdata) == TRUE)
					printf("<option value='%s'>%s\n", escape_string(temp_host->name), temp_host->name);
				}

			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td></td><td class='reportSelectItem'>\n");
			printf("<input type='submit' value='Continue to Step 3'>\n");
			printf("</td></tr>\n");

			printf("</TABLE>\n");
			printf("</form>\n");

			printf("</DIV></P>\n");
			}

		/* ask the user for what service they want a report for */
		else if(input_type == GET_INPUT_SERVICE_TARGET) {

			printf("<SCRIPT LANGUAGE='JavaScript'>\n");
			printf("function gethostname(hostindex){\n");
			printf("hostnames=[");

			for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {
				if(is_authorized_for_service(temp_service, &current_authdata) == TRUE) {
					if(found == TRUE)
						printf(",");
					else
						first_service = temp_service->host_name;
					printf(" \"%s\"", temp_service->host_name);
					found = TRUE;
					}
				}

			printf(" ]\n");
			printf("return hostnames[hostindex];\n");
			printf("}\n");
			printf("</SCRIPT>\n");


			printf("<P><DIV ALIGN=CENTER>\n");
			printf("<DIV CLASS='reportSelectTitle'>Step 2: Select Service</DIV>\n");
			printf("</DIV></P>\n");

			printf("<P><DIV ALIGN=CENTER>\n");

#ifdef LEGACY_GRAPHICAL_CGIS
			printf("<form method=\"GET\" action=\"%s\" name=\"serviceform\">\n", HISTOGRAM_CGI);
#else
			printf("<form method=\"GET\" action=\"%s\" name=\"serviceform\">\n", LEGACY_HISTOGRAM_CGI);
#endif
			printf("<input type='hidden' name='input' value='getoptions'>\n");
			printf("<input type='hidden' name='host' value='%s'>\n", (first_service == NULL) ? "unknown" : (char *)escape_string(first_service));

			printf("<TABLE BORDER=0 cellpadding=5>\n");
			printf("<tr><td class='reportSelectSubTitle'>Service:</td>\n");
			printf("<td class='reportSelectItem'>\n");
			printf("<select name='service' onFocus='document.serviceform.host.value=gethostname(this.selectedIndex);' onChange='document.serviceform.host.value=gethostname(this.selectedIndex);'>\n");

			for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {
				if(is_authorized_for_service(temp_service, &current_authdata) == TRUE)
					printf("<option value='%s'>%s;%s\n", escape_string(temp_service->description), temp_service->host_name, temp_service->description);
				}

			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td></td><td class='reportSelectItem'>\n");
			printf("<input type='submit' value='Continue to Step 3'>\n");
			printf("</td></tr>\n");

			printf("</TABLE>\n");
			printf("</form>\n");

			printf("</DIV></P>\n");
			}

		/* ask the user for report range and options */
		else if(input_type == GET_INPUT_OPTIONS) {

			time(&current_time);
			t = localtime(&current_time);

			start_day = 1;
			start_year = t->tm_year + 1900;
			end_day = t->tm_mday;
			end_year = t->tm_year + 1900;

			printf("<P><DIV ALIGN=CENTER>\n");
			printf("<DIV CLASS='reportSelectTitle'>Step 3: Select Report Options</DIV>\n");
			printf("</DIV></P>\n");

			printf("<P><DIV ALIGN=CENTER>\n");

#ifdef LEGACY_GRAPHICAL_CGIS
			printf("<form method=\"GET\" action=\"%s\">\n", HISTOGRAM_CGI);
#else
			printf("<form method=\"GET\" action=\"%s\">\n", LEGACY_HISTOGRAM_CGI);
#endif
			printf("<input type='hidden' name='host' value='%s'>\n", escape_string(host_name));
			if(display_type == DISPLAY_SERVICE_HISTOGRAM)
				printf("<input type='hidden' name='service' value='%s'>\n", escape_string(svc_description));

			printf("<TABLE BORDER=0 cellpadding=5>\n");
			printf("<tr><td class='reportSelectSubTitle' align=right>Report Period:</td>\n");
			printf("<td class='reportSelectItem'>\n");
			printf("<select name='timeperiod'>\n");
			printf("<option value=today>Today\n");
			printf("<option value=last24hours>Last 24 Hours\n");
			printf("<option value=yesterday>Yesterday\n");
			printf("<option value=thisweek>This Week\n");
			printf("<option value=last7days SELECTED>Last 7 Days\n");
			printf("<option value=lastweek>Last Week\n");
			printf("<option value=thismonth>This Month\n");
			printf("<option value=last31days>Last 31 Days\n");
			printf("<option value=lastmonth>Last Month\n");
			printf("<option value=thisyear>This Year\n");
			printf("<option value=lastyear>Last Year\n");
			printf("<option value=custom>* CUSTOM REPORT PERIOD *\n");
			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td valign=top class='reportSelectSubTitle'>If Custom Report Period...</td></tr>\n");

			printf("<tr>");
			printf("<td valign=top class='reportSelectSubTitle'>Start Date (Inclusive):</td>\n");
			printf("<td align=left valign=top class='reportSelectItem'>");
			printf("<select name='smon'>\n");
			printf("<option value='1' %s>January\n", (t->tm_mon == 0) ? "SELECTED" : "");
			printf("<option value='2' %s>February\n", (t->tm_mon == 1) ? "SELECTED" : "");
			printf("<option value='3' %s>March\n", (t->tm_mon == 2) ? "SELECTED" : "");
			printf("<option value='4' %s>April\n", (t->tm_mon == 3) ? "SELECTED" : "");
			printf("<option value='5' %s>May\n", (t->tm_mon == 4) ? "SELECTED" : "");
			printf("<option value='6' %s>June\n", (t->tm_mon == 5) ? "SELECTED" : "");
			printf("<option value='7' %s>July\n", (t->tm_mon == 6) ? "SELECTED" : "");
			printf("<option value='8' %s>August\n", (t->tm_mon == 7) ? "SELECTED" : "");
			printf("<option value='9' %s>September\n", (t->tm_mon == 8) ? "SELECTED" : "");
			printf("<option value='10' %s>October\n", (t->tm_mon == 9) ? "SELECTED" : "");
			printf("<option value='11' %s>November\n", (t->tm_mon == 10) ? "SELECTED" : "");
			printf("<option value='12' %s>December\n", (t->tm_mon == 11) ? "SELECTED" : "");
			printf("</select>\n ");
			printf("<input type='text' size='2' maxlength='2' name='sday' value='%d'> ", start_day);
			printf("<input type='text' size='4' maxlength='4' name='syear' value='%d'>", start_year);
			printf("<input type='hidden' name='shour' value='0'>\n");
			printf("<input type='hidden' name='smin' value='0'>\n");
			printf("<input type='hidden' name='ssec' value='0'>\n");
			printf("</td>\n");
			printf("</tr>\n");

			printf("<tr>");
			printf("<td valign=top class='reportSelectSubTitle'>End Date (Inclusive):</td>\n");
			printf("<td align=left valign=top class='reportSelectItem'>");
			printf("<select name='emon'>\n");
			printf("<option value='1' %s>January\n", (t->tm_mon == 0) ? "SELECTED" : "");
			printf("<option value='2' %s>February\n", (t->tm_mon == 1) ? "SELECTED" : "");
			printf("<option value='3' %s>March\n", (t->tm_mon == 2) ? "SELECTED" : "");
			printf("<option value='4' %s>April\n", (t->tm_mon == 3) ? "SELECTED" : "");
			printf("<option value='5' %s>May\n", (t->tm_mon == 4) ? "SELECTED" : "");
			printf("<option value='6' %s>June\n", (t->tm_mon == 5) ? "SELECTED" : "");
			printf("<option value='7' %s>July\n", (t->tm_mon == 6) ? "SELECTED" : "");
			printf("<option value='8' %s>August\n", (t->tm_mon == 7) ? "SELECTED" : "");
			printf("<option value='9' %s>September\n", (t->tm_mon == 8) ? "SELECTED" : "");
			printf("<option value='10' %s>October\n", (t->tm_mon == 9) ? "SELECTED" : "");
			printf("<option value='11' %s>November\n", (t->tm_mon == 10) ? "SELECTED" : "");
			printf("<option value='12' %s>December\n", (t->tm_mon == 11) ? "SELECTED" : "");
			printf("</select>\n ");
			printf("<input type='text' size='2' maxlength='2' name='eday' value='%d'> ", end_day);
			printf("<input type='text' size='4' maxlength='4' name='eyear' value='%d'>", end_year);
			printf("<input type='hidden' name='ehour' value='24'>\n");
			printf("<input type='hidden' name='emin' value='0'>\n");
			printf("<input type='hidden' name='esec' value='0'>\n");
			printf("</td>\n");
			printf("</tr>\n");

			printf("<tr><td colspan=2><br></td></tr>\n");

			printf("<tr><td class='reportSelectSubTitle' align=right>Statistics Breakdown:</td>\n");
			printf("<td class='reportSelectItem'>\n");
			printf("<select name='breakdown'>\n");
			printf("<option value=monthly>Month\n");
			printf("<option value=dayofmonth SELECTED>Day of the Month\n");
			printf("<option value=dayofweek>Day of the Week\n");
			printf("<option value=hourly>Hour of the Day\n");
			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td class='reportSelectSubTitle' align=right>Events To Graph:</td>\n");
			printf("<td class='reportSelectItem'>\n");
			printf("<select name='graphevents'>\n");
			if(display_type == DISPLAY_HOST_HISTOGRAM) {
				printf("<option value=%d %s>All host events\n", GRAPH_HOST_ALL, (graph_events == GRAPH_HOST_ALL) ? "SELECTED" : "");
				printf("<option value=%d %s>Host problem events\n", GRAPH_HOST_PROBLEMS, (graph_events == GRAPH_HOST_PROBLEMS) ? "SELECTED" : "");
				printf("<option value=%d %s>Host up events\n", GRAPH_HOST_UP, (graph_events == GRAPH_HOST_UP) ? "SELECTED" : "");
				printf("<option value=%d %s>Host down events\n", GRAPH_HOST_DOWN, (graph_events == GRAPH_HOST_DOWN) ? "SELECTED" : "");
				printf("<option value=%d %s>Host unreachable events\n", GRAPH_HOST_UNREACHABLE, (graph_events == GRAPH_HOST_UNREACHABLE) ? "SELECTED" : "");
				}
			else {
				printf("<option value=%d %s>All service events\n", GRAPH_SERVICE_ALL, (graph_events == GRAPH_SERVICE_ALL) ? "SELECTED" : "");
				printf("<option value=%d %s>Service problem events\n", GRAPH_SERVICE_PROBLEMS, (graph_events == GRAPH_SERVICE_PROBLEMS) ? "SELECTED" : "");
				printf("<option value=%d %s>Service ok events\n", GRAPH_SERVICE_OK, (graph_events == GRAPH_SERVICE_OK) ? "SELECTED" : "");
				printf("<option value=%d %s>Service warning events\n", GRAPH_SERVICE_WARNING, (graph_events == GRAPH_SERVICE_WARNING) ? "SELECTED" : "");
				printf("<option value=%d %s>Service unknown events\n", GRAPH_SERVICE_UNKNOWN, (graph_events == GRAPH_SERVICE_UNKNOWN) ? "SELECTED" : "");
				printf("<option value=%d %s>Service critical events\n", GRAPH_SERVICE_CRITICAL, (graph_events == GRAPH_SERVICE_CRITICAL) ? "SELECTED" : "");
				}
			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td class='reportSelectSubTitle' align=right>State Types To Graph:</td>\n");
			printf("<td class='reportSelectItem'>\n");
			printf("<select name='graphstatetypes'>\n");
			printf("<option value=%d>Hard states\n", GRAPH_HARD_STATETYPES);
			printf("<option value=%d>Soft states\n", GRAPH_SOFT_STATETYPES);
			printf("<option value=%d SELECTED>Hard and soft states\n", GRAPH_ALL_STATETYPES);
			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td class='reportSelectSubTitle' align=right>Assume State Retention:</td>\n");
			printf("<td class='reportSelectItem'>\n");
			printf("<select name='assumestateretention'>\n");
			printf("<option value='yes'>Yes\n");
			printf("<option value='no'>No\n");
			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td class='reportSelectSubTitle' align=right>Initial States Logged:</td>\n");
			printf("<td class='reportSelectItem'>\n");
			printf("<select name='initialstateslogged'>\n");
			printf("<option value='yes'>Yes\n");
			printf("<option value='no' SELECTED>No\n");
			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td class='reportSelectSubTitle' align=right>Ignore Repeated States:</td>\n");
			printf("<td class='reportSelectItem'>\n");
			printf("<select name='newstatesonly'>\n");
			printf("<option value='yes'>Yes\n");
			printf("<option value='no' SELECTED>No\n");
			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td></td><td class='reportSelectItem'><input type='submit' value='Create Report'></td></tr>\n");

			printf("</TABLE>\n");
			printf("</form>\n");

			printf("</DIV></P>\n");
			}

		/* as the user whether they want a graph for a host or service */
		else {
			printf("<P><DIV ALIGN=CENTER>\n");
			printf("<DIV CLASS='reportSelectTitle'>Step 1: Select Report Type</DIV>\n");
			printf("</DIV></P>\n");

			printf("<P><DIV ALIGN=CENTER>\n");

#ifdef LEGACY_GRAPHICAL_CGIS
			printf("<form method=\"GET\" action=\"%s\">\n", HISTOGRAM_CGI);
#else
			printf("<form method=\"GET\" action=\"%s\">\n", LEGACY_HISTOGRAM_CGI);
#endif
			printf("<TABLE BORDER=0 cellpadding=5>\n");
			printf("<tr><td class='reportSelectSubTitle' align=right>Type:</td>\n");
			printf("<td class='reportSelectItem'>\n");
			printf("<select name='input'>\n");
			printf("<option value=gethost>Host\n");
			printf("<option value=getservice>Service\n");
			printf("</select>\n");
			printf("</td></tr>\n");

			printf("<tr><td></td><td class='reportSelectItem'>\n");
			printf("<input type='submit' value='Continue to Step 2'>\n");
			printf("</td></tr>\n");

			printf("</TABLE>\n");
			printf("</form>\n");

			printf("</DIV></P>\n");
			}

		}

	document_footer();

	/* free all other allocated memory */
	free_memory();

	return OK;
	}


void document_header(int use_stylesheet) {
	char date_time[MAX_DATETIME_LENGTH];
	time_t current_time;
	time_t expire_time;

	if(mode == CREATE_HTML) {
		printf("Cache-Control: no-store\r\n");
		printf("Pragma: no-cache\r\n");

		time(&current_time);
		get_time_string(&current_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
		printf("Last-Modified: %s\r\n", date_time);

		expire_time = (time_t)0;
		get_time_string(&expire_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
		printf("Expires: %s\r\n", date_time);

		printf("Content-type: text/html; charset=utf-8\r\n\r\n");

		if(embedded == TRUE)
			return;

		printf("<html>\n");
		printf("<head>\n");
		printf("<link rel=\"shortcut icon\" href=\"%sfavicon.ico\" type=\"image/ico\">\n", url_images_path);
		printf("<title>\n");
		printf("Nagios Histogram\n");
		printf("</title>\n");

		if(use_stylesheet == TRUE) {
			printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, COMMON_CSS);
			printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, HISTOGRAM_CSS);
			}

		printf("</head>\n");

		printf("<BODY CLASS='histogram'>\n");

		/* include user SSI header */
#ifdef LEGACY_GRAPHICAL_CGIS
		include_ssi_files(HISTOGRAM_CGI, SSI_HEADER);
#else
		include_ssi_files(LEGACY_HISTOGRAM_CGI, SSI_HEADER);
#endif

		printf("<div id=\"popup\" style=\"position:absolute; z-index:1; visibility: hidden\"></div>\n");
		}

	else {
		printf("Cache-Control: no-store\r\n");
		printf("Pragma: no-cache\r\n");

		time(&current_time);
		get_time_string(&current_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
		printf("Last-Modified: %s\r\n", date_time);

		expire_time = (time_t)0L;
		get_time_string(&expire_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
		printf("Expires: %s\r\n", date_time);

		printf("Content-Type: image/png\r\n\r\n");
		}

	return;
	}



void document_footer(void) {

	if(embedded == TRUE)
		return;

	if(mode == CREATE_HTML) {

		/* include user SSI footer */
#ifdef LEGACY_GRAPHICAL_CGIS
		include_ssi_files(HISTOGRAM_CGI, SSI_FOOTER);
#else
		include_ssi_files(LEGACY_HISTOGRAM_CGI, SSI_FOOTER);
#endif

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

		/* we found the host argument */
		else if(!strcmp(variables[x], "host")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((host_name = (char *)strdup(variables[x])) == NULL)
				host_name = "";
			strip_html_brackets(host_name);

			display_type = DISPLAY_HOST_HISTOGRAM;
			}

		/* we found the node width argument */
		else if(!strcmp(variables[x], "service")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((svc_description = (char *)strdup(variables[x])) == NULL)
				svc_description = "";
			strip_html_brackets(svc_description);

			display_type = DISPLAY_SERVICE_HISTOGRAM;
			}

		/* we found first time argument */
		else if(!strcmp(variables[x], "t1")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			t1 = (time_t)strtoul(variables[x], NULL, 10);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			}

		/* we found first time argument */
		else if(!strcmp(variables[x], "t2")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			t2 = (time_t)strtoul(variables[x], NULL, 10);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			}

		/* we found the image creation option */
		else if(!strcmp(variables[x], "createimage")) {
			mode = CREATE_IMAGE;
			}

		/* we found the backtrack archives argument */
		else if(!strcmp(variables[x], "backtrack")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			backtrack_archives = atoi(variables[x]);
			if(backtrack_archives < 0)
				backtrack_archives = 0;
			if(backtrack_archives > MAX_ARCHIVE_BACKTRACKS)
				backtrack_archives = MAX_ARCHIVE_BACKTRACKS;
			}

		/* we found the standard timeperiod argument */
		else if(!strcmp(variables[x], "timeperiod")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "today"))
				timeperiod_type = TIMEPERIOD_TODAY;
			else if(!strcmp(variables[x], "yesterday"))
				timeperiod_type = TIMEPERIOD_YESTERDAY;
			else if(!strcmp(variables[x], "thisweek"))
				timeperiod_type = TIMEPERIOD_THISWEEK;
			else if(!strcmp(variables[x], "lastweek"))
				timeperiod_type = TIMEPERIOD_LASTWEEK;
			else if(!strcmp(variables[x], "thismonth"))
				timeperiod_type = TIMEPERIOD_THISMONTH;
			else if(!strcmp(variables[x], "lastmonth"))
				timeperiod_type = TIMEPERIOD_LASTMONTH;
			else if(!strcmp(variables[x], "thisquarter"))
				timeperiod_type = TIMEPERIOD_THISQUARTER;
			else if(!strcmp(variables[x], "lastquarter"))
				timeperiod_type = TIMEPERIOD_LASTQUARTER;
			else if(!strcmp(variables[x], "thisyear"))
				timeperiod_type = TIMEPERIOD_THISYEAR;
			else if(!strcmp(variables[x], "lastyear"))
				timeperiod_type = TIMEPERIOD_LASTYEAR;
			else if(!strcmp(variables[x], "last24hours"))
				timeperiod_type = TIMEPERIOD_LAST24HOURS;
			else if(!strcmp(variables[x], "last7days"))
				timeperiod_type = TIMEPERIOD_LAST7DAYS;
			else if(!strcmp(variables[x], "last31days"))
				timeperiod_type = TIMEPERIOD_LAST31DAYS;
			else if(!strcmp(variables[x], "custom"))
				timeperiod_type = TIMEPERIOD_CUSTOM;
			else
				timeperiod_type = TIMEPERIOD_TODAY;


			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				convert_timeperiod_to_times(timeperiod_type);
			}

		/* we found time argument */
		else if(!strcmp(variables[x], "smon")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			start_month = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}

		/* we found time argument */
		else if(!strcmp(variables[x], "sday")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			start_day = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}

		/* we found time argument */
		else if(!strcmp(variables[x], "syear")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			start_year = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}

		/* we found time argument */
		else if(!strcmp(variables[x], "smin")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			start_minute = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}

		/* we found time argument */
		else if(!strcmp(variables[x], "ssec")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			start_second = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}

		/* we found time argument */
		else if(!strcmp(variables[x], "shour")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			start_hour = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}


		/* we found time argument */
		else if(!strcmp(variables[x], "emon")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			end_month = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}

		/* we found time argument */
		else if(!strcmp(variables[x], "eday")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			end_day = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}

		/* we found time argument */
		else if(!strcmp(variables[x], "eyear")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			end_year = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}

		/* we found time argument */
		else if(!strcmp(variables[x], "emin")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			end_minute = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}

		/* we found time argument */
		else if(!strcmp(variables[x], "esec")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			end_second = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}

		/* we found time argument */
		else if(!strcmp(variables[x], "ehour")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(timeperiod_type != TIMEPERIOD_CUSTOM)
				continue;

			end_hour = atoi(variables[x]);
			timeperiod_type = TIMEPERIOD_CUSTOM;
			compute_time_from_parts = TRUE;
			}

		/* we found the embed option */
		else if(!strcmp(variables[x], "embedded"))
			embedded = TRUE;

		/* we found the noheader option */
		else if(!strcmp(variables[x], "noheader"))
			display_header = FALSE;

		/* we found the input option */
		else if(!strcmp(variables[x], "input")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "gethost"))
				input_type = GET_INPUT_HOST_TARGET;
			else if(!strcmp(variables[x], "getservice"))
				input_type = GET_INPUT_SERVICE_TARGET;
			else if(!strcmp(variables[x], "getoptions"))
				input_type = GET_INPUT_OPTIONS;
			else
				input_type = GET_INPUT_TARGET_TYPE;
			}

		/* we found the graph states option */
		else if(!strcmp(variables[x], "graphevents")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			graph_events = atoi(variables[x]);
			}

		/* we found the graph state types option */
		else if(!strcmp(variables[x], "graphstatetypes")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			graph_statetypes = atoi(variables[x]);
			}

		/* we found the breakdown option */
		else if(!strcmp(variables[x], "breakdown")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "monthly"))
				breakdown_type = BREAKDOWN_MONTHLY;
			else if(!strcmp(variables[x], "dayofmonth"))
				breakdown_type = BREAKDOWN_DAY_OF_MONTH;
			else if(!strcmp(variables[x], "dayofweek"))
				breakdown_type = BREAKDOWN_DAY_OF_WEEK;
			else
				breakdown_type = BREAKDOWN_HOURLY;
			}

		/* we found the assume state retention option */
		else if(!strcmp(variables[x], "assumestateretention")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "yes"))
				assume_state_retention = TRUE;
			else
				assume_state_retention = FALSE;
			}

		/* we found the initial states logged option */
		else if(!strcmp(variables[x], "initialstateslogged")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "yes"))
				initial_states_logged = TRUE;
			else
				initial_states_logged = FALSE;

			}

		/* we found the new states only option */
		else if(!strcmp(variables[x], "newstatesonly")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "yes"))
				new_states_only = TRUE;
			else
				new_states_only = FALSE;

			}
		}

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return error;
	}



/* graphs histogram data */
void graph_all_histogram_data(void) {
	int pixel_x;
	int pixel_y;
	int last_pixel_y;
	int current_bucket;
	int actual_bucket;
	unsigned long max_value;
	double x_scaling_factor;
	double y_scaling_factor;
	double x_units;
	double y_units;
	int current_unit;
	int actual_unit;
	char temp_buffer[MAX_INPUT_BUFFER];
	int string_width;
	int string_height;
	char *days[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
	char *months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
	char start_time[MAX_INPUT_BUFFER];
	char end_time[MAX_INPUT_BUFFER];

	unsigned long state1_max = 0L;
	unsigned long state1_min = 0L;
	int have_state1_min = FALSE;
	unsigned long state1_sum = 0L;
	double state1_avg = 0.0;
	unsigned long state2_max = 0L;
	unsigned long state2_min = 0L;
	int have_state2_min = FALSE;
	unsigned long state2_sum = 0L;
	double state2_avg = 0.0;
	unsigned long state3_max = 0L;
	unsigned long state3_min = 0L;
	int have_state3_min = FALSE;
	unsigned long state3_sum = 0L;
	double state3_avg = 0.0;
	unsigned long state4_max = 0L;
	unsigned long state4_min = 0L;
	int have_state4_min = FALSE;
	unsigned long state4_sum = 0L;
	double state4_avg = 0.0;


#ifdef DEBUG
	printf("Total Buckets: %d\n", total_buckets);
#endif

	/* determine max value in the buckets (for scaling) */
	max_value = 0L;
	for(current_bucket = 0; current_bucket < total_buckets; current_bucket++) {
		if(tsdata[current_bucket].service_ok > max_value)
			max_value = tsdata[current_bucket].service_ok;
		if(tsdata[current_bucket].service_warning > max_value)
			max_value = tsdata[current_bucket].service_warning;
		if(tsdata[current_bucket].service_unknown > max_value)
			max_value = tsdata[current_bucket].service_unknown;
		if(tsdata[current_bucket].service_critical > max_value)
			max_value = tsdata[current_bucket].service_critical;
		if(tsdata[current_bucket].host_up > max_value)
			max_value = tsdata[current_bucket].host_up;
		if(tsdata[current_bucket].host_down > max_value)
			max_value = tsdata[current_bucket].host_down;
		if(tsdata[current_bucket].host_unreachable > max_value)
			max_value = tsdata[current_bucket].host_unreachable;
		}

#ifdef DEBUG
	printf("Done determining max bucket values\n");
	printf("MAX_VALUE=%lu\n", max_value);
	printf("DRAWING_HEIGHT=%lu\n", DRAWING_HEIGHT);
#endif

	/* min number of values to graph */
	if(max_value < 10)
		max_value = 10;
#ifdef DEBUG
	printf("ADJUSTED MAX_VALUE=%lu\n", max_value);
#endif

	/* determine y scaling factor */
	/*y_scaling_factor=floor((double)DRAWING_HEIGHT/(double)max_value);*/
	y_scaling_factor = (double)((double)DRAWING_HEIGHT / (double)max_value);

	/* determine x scaling factor */
	x_scaling_factor = (double)((double)DRAWING_WIDTH / (double)total_buckets);

	/* determine y units resolution - we want a max of about 10 y grid lines */
	/*
	y_units=(double)((double)DRAWING_HEIGHT/19.0);
	y_units=ceil(y_units/y_scaling_factor)*y_scaling_factor;
	*/
	y_units = ceil(19.0 / y_scaling_factor);

	/* determine x units resolution */
	if(breakdown_type == BREAKDOWN_HOURLY)
		x_units = (double)((double)DRAWING_WIDTH / (double)(total_buckets / 4.0));
	else
		x_units = x_scaling_factor;

#ifdef DEBUG
	printf("DRAWING_WIDTH: %d\n", DRAWING_WIDTH);
	printf("DRAWING_HEIGHT: %d\n", DRAWING_HEIGHT);
	printf("max_value: %lu\n", max_value);
	printf("x_scaling_factor: %.3f\n", x_scaling_factor);
	printf("y_scaling_factor: %.3f\n", y_scaling_factor);
	printf("x_units: %.3f\n", x_units);
	printf("y_units: %.3f\n", y_units);
	printf("y units to draw: %.3f\n", ((double)max_value / y_units));
#endif

	string_height = gdFontSmall->h;

#ifdef DEBUG
	printf("Starting to draw Y grid lines...\n");
#endif

	/* draw y grid lines */
	if(max_value > 0) {
		for(current_unit = 1; (current_unit * y_units * y_scaling_factor) <= DRAWING_HEIGHT; current_unit++) {
			draw_dashed_line(DRAWING_X_OFFSET, DRAWING_Y_OFFSET - (current_unit * y_units * y_scaling_factor), DRAWING_X_OFFSET + DRAWING_WIDTH, DRAWING_Y_OFFSET - (current_unit * y_units * y_scaling_factor), color_lightgray);
#ifdef DEBUG
			printf("  Drawing Y unit #%d @ %d\n", current_unit, (int)(current_unit * y_units * y_scaling_factor));
#endif
			}
		}

#ifdef DEBUG
	printf("Starting to draw X grid lines...\n");
#endif

	/* draw x grid lines */
	for(current_unit = 1; (int)(current_unit * x_units) <= DRAWING_WIDTH; current_unit++)
		draw_dashed_line(DRAWING_X_OFFSET + (int)(current_unit * x_units), DRAWING_Y_OFFSET, DRAWING_X_OFFSET + (int)(current_unit * x_units), DRAWING_Y_OFFSET - DRAWING_HEIGHT, color_lightgray);

#ifdef DEBUG
	printf("Starting to draw grid units...\n");
#endif

	/* draw y units */
	if(max_value > 0) {
		for(current_unit = 0; (current_unit * y_units * y_scaling_factor) <= DRAWING_HEIGHT; current_unit++) {
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%d", (int)(current_unit * y_units));
			temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
			string_width = gdFontSmall->w * strlen(temp_buffer);
			gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET - string_width - 5, DRAWING_Y_OFFSET - (current_unit * y_units * y_scaling_factor) - (string_height / 2), (unsigned char *)temp_buffer, color_black);
			}
		}

	/* draw x units */
	for(current_unit = 0, actual_unit = 0; (int)(current_unit * x_units) <= DRAWING_WIDTH; current_unit++, actual_unit++) {

		if(actual_unit >= total_buckets)
			actual_unit = 0;

		if(breakdown_type == BREAKDOWN_MONTHLY)
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%s", months[actual_unit]);
		else if(breakdown_type == BREAKDOWN_DAY_OF_MONTH)
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%d", actual_unit + 1);
		else if(breakdown_type == BREAKDOWN_DAY_OF_WEEK)
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%s", days[actual_unit]);
		else
			snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%02d:00", (actual_unit == 24) ? 0 : actual_unit);

		temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
		string_width = gdFontSmall->w * strlen(temp_buffer);

		gdImageStringUp(histogram_image, gdFontSmall, DRAWING_X_OFFSET + (current_unit * x_units) - (string_height / 2), DRAWING_Y_OFFSET + 5 + string_width, (unsigned char *)temp_buffer, color_black);
		}

	/* draw y unit measure */
	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Number of Events");
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageStringUp(histogram_image, gdFontSmall, 0, DRAWING_Y_OFFSET - (DRAWING_HEIGHT / 2) + (string_width / 2), (unsigned char *)temp_buffer, color_black);

	/* draw x unit measure */
	if(breakdown_type == BREAKDOWN_MONTHLY)
		snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Month");
	else if(breakdown_type == BREAKDOWN_DAY_OF_MONTH)
		snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Day of the Month");
	else if(breakdown_type == BREAKDOWN_DAY_OF_WEEK)
		snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Day of the Week");
	else
		snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Hour of the Day (15 minute increments)");
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + (DRAWING_WIDTH / 2) - (string_width / 2), DRAWING_Y_OFFSET + 70, (unsigned char *)temp_buffer, color_black);

	/* draw title */
	snprintf(start_time, sizeof(start_time) - 1, "%s", ctime(&t1));
	start_time[sizeof(start_time) - 1] = '\x0';
	start_time[strlen(start_time) - 1] = '\x0';
	snprintf(end_time, sizeof(end_time) - 1, "%s", ctime(&t2));
	end_time[sizeof(end_time) - 1] = '\x0';
	end_time[strlen(end_time) - 1] = '\x0';

	if(display_type == DISPLAY_HOST_HISTOGRAM)
		snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Event History For Host '%s'", host_name);
	else
		snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Event History For Service '%s' On Host '%s'", svc_description, host_name);
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + (DRAWING_WIDTH / 2) - (string_width / 2), 0, (unsigned char *)temp_buffer, color_black);

	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%s to %s", start_time, end_time);
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + (DRAWING_WIDTH / 2) - (string_width / 2), string_height + 5, (unsigned char *)temp_buffer, color_black);


#ifdef DEBUG
	printf("About to starting graphing (total_buckets=%d)...\n", total_buckets);
#endif


	/* graph service states */
	if(display_type == DISPLAY_HOST_HISTOGRAM) {

		/* graph host recoveries */
		if(graph_events & GRAPH_HOST_UP) {

			last_pixel_y = 0;
			for(current_bucket = 0, actual_bucket = 0; current_bucket <= total_buckets; current_bucket++, actual_bucket++) {

				if(actual_bucket >= total_buckets)
					actual_bucket = 0;

				pixel_x = (int)(current_bucket * x_scaling_factor);

				pixel_y = (int)(tsdata[actual_bucket].host_up * y_scaling_factor);

				if(current_bucket > 0 && !(last_pixel_y == 0 && pixel_y == 0))
					draw_line(DRAWING_X_OFFSET + pixel_x - (int)x_scaling_factor, DRAWING_Y_OFFSET - last_pixel_y, DRAWING_X_OFFSET + pixel_x, DRAWING_Y_OFFSET - pixel_y, color_green);

				last_pixel_y = pixel_y;

				if(current_bucket < total_buckets) {
					if(have_state1_min == FALSE || tsdata[actual_bucket].host_up < state1_min) {
						state1_min = tsdata[actual_bucket].host_up;
						have_state1_min = TRUE;
						}
					if(state1_max == 0 || tsdata[actual_bucket].host_up > state1_max)
						state1_max = tsdata[actual_bucket].host_up;
					state1_sum += tsdata[actual_bucket].host_up;
					}
				}
			}

#ifdef DEBUG
		printf("Done graphing HOST UP states...\n");
#endif

		/* graph host down states */
		if(graph_events & GRAPH_HOST_DOWN) {

			last_pixel_y = 0;
			for(current_bucket = 0, actual_bucket = 0; current_bucket <= total_buckets; current_bucket++, actual_bucket++) {

				if(actual_bucket >= total_buckets)
					actual_bucket = 0;

				pixel_x = (int)(current_bucket * x_scaling_factor);

				pixel_y = (int)(tsdata[actual_bucket].host_down * y_scaling_factor);

				if(current_bucket > 0 && !(last_pixel_y == 0 && pixel_y == 0))
					draw_line(DRAWING_X_OFFSET + pixel_x - (int)x_scaling_factor, DRAWING_Y_OFFSET - last_pixel_y, DRAWING_X_OFFSET + pixel_x, DRAWING_Y_OFFSET - pixel_y, color_red);

				last_pixel_y = pixel_y;

				if(current_bucket < total_buckets) {
					if(have_state2_min == FALSE || tsdata[actual_bucket].host_down < state2_min) {
						state2_min = tsdata[actual_bucket].host_down;
						have_state2_min = TRUE;
						}
					if(state2_max == 0 || tsdata[actual_bucket].host_down > state2_max)
						state2_max = tsdata[actual_bucket].host_down;
					state2_sum += tsdata[actual_bucket].host_down;
					}
				}
			}

#ifdef DEBUG
		printf("Done graphing HOST DOWN states...\n");
#endif

		/* graph host unreachable states */
		if(graph_events & GRAPH_HOST_UNREACHABLE) {

			last_pixel_y = 0;
			for(current_bucket = 0, actual_bucket = 0; current_bucket <= total_buckets; current_bucket++, actual_bucket++) {

				if(actual_bucket >= total_buckets)
					actual_bucket = 0;

				pixel_x = (int)(current_bucket * x_scaling_factor);

				pixel_y = (int)(tsdata[actual_bucket].host_unreachable * y_scaling_factor);

				if(current_bucket > 0 && !(last_pixel_y == 0 && pixel_y == 0))
					draw_line(DRAWING_X_OFFSET + pixel_x - (int)x_scaling_factor, DRAWING_Y_OFFSET - last_pixel_y, DRAWING_X_OFFSET + pixel_x, DRAWING_Y_OFFSET - pixel_y, color_darkred);

				last_pixel_y = pixel_y;

				if(current_bucket < total_buckets) {
					if(have_state3_min == FALSE || tsdata[actual_bucket].host_unreachable < state3_min) {
						state3_min = tsdata[actual_bucket].host_unreachable;
						have_state3_min = TRUE;
						}
					if(state3_max == 0 || tsdata[actual_bucket].host_unreachable > state3_max)
						state3_max = tsdata[actual_bucket].host_unreachable;
					state3_sum += tsdata[actual_bucket].host_unreachable;
					}
				}
			}

#ifdef DEBUG
		printf("Done graphing HOST UNREACHABLE states...\n");
#endif

		}

	/* graph service states */
	else {

		/* graph service recoveries */
		if(graph_events & GRAPH_SERVICE_OK) {

			last_pixel_y = 0;
			for(current_bucket = 0, actual_bucket = 0; current_bucket <= total_buckets; current_bucket++, actual_bucket++) {

				if(actual_bucket >= total_buckets)
					actual_bucket = 0;

				pixel_x = (int)(current_bucket * x_scaling_factor);

				pixel_y = (int)(tsdata[actual_bucket].service_ok * y_scaling_factor);

				if(current_bucket > 0 && !(last_pixel_y == 0 && pixel_y == 0))
					draw_line(DRAWING_X_OFFSET + pixel_x - (int)x_scaling_factor, DRAWING_Y_OFFSET - last_pixel_y, DRAWING_X_OFFSET + pixel_x, DRAWING_Y_OFFSET - pixel_y, color_green);

				last_pixel_y = pixel_y;

				if(current_bucket < total_buckets) {
					if(have_state1_min == FALSE || tsdata[actual_bucket].service_ok < state1_min) {
						state1_min = tsdata[actual_bucket].service_ok;
						have_state1_min = TRUE;
						}
					if(state1_max == 0 || tsdata[actual_bucket].service_ok > state1_max)
						state1_max = tsdata[actual_bucket].service_ok;
					state1_sum += tsdata[actual_bucket].service_ok;
					}
				}
			}

		/* graph service warning states */
		if(graph_events & GRAPH_SERVICE_WARNING) {

			last_pixel_y = 0;
			for(current_bucket = 0, actual_bucket = 0; current_bucket <= total_buckets; current_bucket++, actual_bucket++) {

				if(actual_bucket >= total_buckets)
					actual_bucket = 0;

				pixel_x = (int)(current_bucket * x_scaling_factor);

				pixel_y = (int)(tsdata[actual_bucket].service_warning * y_scaling_factor);

				if(current_bucket > 0 && !(last_pixel_y == 0 && pixel_y == 0))
					draw_line(DRAWING_X_OFFSET + pixel_x - (int)x_scaling_factor, DRAWING_Y_OFFSET - last_pixel_y, DRAWING_X_OFFSET + pixel_x, DRAWING_Y_OFFSET - pixel_y, color_yellow);

				last_pixel_y = pixel_y;

				if(current_bucket < total_buckets) {
					if(have_state2_min == FALSE || tsdata[actual_bucket].service_warning < state2_min) {
						state2_min = tsdata[actual_bucket].service_warning;
						have_state2_min = TRUE;
						}
					if(state2_max == 0 || tsdata[actual_bucket].service_warning > state2_max)
						state2_max = tsdata[actual_bucket].service_warning;
					state2_sum += tsdata[actual_bucket].service_warning;
					}
				}
			}

		/* graph service unknown states */
		if(graph_events & GRAPH_SERVICE_UNKNOWN) {

			last_pixel_y = 0;
			for(current_bucket = 0, actual_bucket = 0; current_bucket <= total_buckets; current_bucket++, actual_bucket++) {

				if(actual_bucket >= total_buckets)
					actual_bucket = 0;

				pixel_x = (int)(current_bucket * x_scaling_factor);

				pixel_y = (int)(tsdata[actual_bucket].service_unknown * y_scaling_factor);

				if(current_bucket > 0 && !(last_pixel_y == 0 && pixel_y == 0))
					draw_line(DRAWING_X_OFFSET + pixel_x - (int)x_scaling_factor, DRAWING_Y_OFFSET - last_pixel_y, DRAWING_X_OFFSET + pixel_x, DRAWING_Y_OFFSET - pixel_y, color_orange);

				last_pixel_y = pixel_y;

				if(current_bucket < total_buckets) {
					if(have_state3_min == FALSE || tsdata[actual_bucket].service_unknown < state3_min) {
						state3_min = tsdata[actual_bucket].service_unknown;
						have_state3_min = TRUE;
						}
					if(state3_max == 0 || tsdata[actual_bucket].service_unknown > state3_max)
						state3_max = tsdata[actual_bucket].service_unknown;
					state3_sum += tsdata[actual_bucket].service_unknown;
					}
				}
			}

		/* graph service critical states */
		if(graph_events & GRAPH_SERVICE_CRITICAL) {

			last_pixel_y = 0;
			for(current_bucket = 0, actual_bucket = 0; current_bucket <= total_buckets; current_bucket++, actual_bucket++) {

				if(actual_bucket >= total_buckets)
					actual_bucket = 0;

				pixel_x = (int)(current_bucket * x_scaling_factor);

				pixel_y = (int)(tsdata[actual_bucket].service_critical * y_scaling_factor);

				if(current_bucket > 0 && !(last_pixel_y == 0 && pixel_y == 0))
					draw_line(DRAWING_X_OFFSET + pixel_x - (int)x_scaling_factor, DRAWING_Y_OFFSET - last_pixel_y, DRAWING_X_OFFSET + pixel_x, DRAWING_Y_OFFSET - pixel_y, color_red);

				last_pixel_y = pixel_y;

				if(current_bucket < total_buckets) {
					if(have_state4_min == FALSE || tsdata[actual_bucket].service_critical < state4_min) {
						state4_min = tsdata[actual_bucket].service_critical;
						have_state4_min = TRUE;
						}
					if(state4_max == 0 || tsdata[actual_bucket].service_critical > state4_max)
						state4_max = tsdata[actual_bucket].service_critical;
					state4_sum += tsdata[actual_bucket].service_critical;
					}
				}
			}
		}

#ifdef DEBUG
	printf("Done graphing states...\n");
#endif

	/* draw graph boundaries */
	draw_line(DRAWING_X_OFFSET, DRAWING_Y_OFFSET, DRAWING_X_OFFSET, DRAWING_Y_OFFSET - DRAWING_HEIGHT, color_black);
	draw_line(DRAWING_X_OFFSET + DRAWING_WIDTH, DRAWING_Y_OFFSET, DRAWING_X_OFFSET + DRAWING_WIDTH, DRAWING_Y_OFFSET - DRAWING_HEIGHT, color_black);
	draw_line(DRAWING_X_OFFSET, DRAWING_Y_OFFSET, DRAWING_X_OFFSET + DRAWING_WIDTH, DRAWING_Y_OFFSET, color_black);


	/* graph stats */
	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "EVENT TYPE");
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + DRAWING_WIDTH + 15, DRAWING_Y_OFFSET - DRAWING_HEIGHT, (unsigned char *)temp_buffer, color_black);

	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "  MIN   MAX   SUM   AVG");
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + DRAWING_WIDTH + 115, DRAWING_Y_OFFSET - DRAWING_HEIGHT, (unsigned char *)temp_buffer, color_black);

	draw_line(DRAWING_X_OFFSET + DRAWING_WIDTH + 15, DRAWING_Y_OFFSET - DRAWING_HEIGHT + string_height + 2, DRAWING_X_OFFSET + DRAWING_WIDTH + 275, DRAWING_Y_OFFSET - DRAWING_HEIGHT + string_height + 2, color_black);

	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Recovery (%s):", (display_type == DISPLAY_SERVICE_HISTOGRAM) ? "Ok" : "Up");
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + DRAWING_WIDTH + 15, DRAWING_Y_OFFSET - DRAWING_HEIGHT + ((string_height + 5) * 1), (unsigned char *)temp_buffer, color_green);

	state1_avg = (double)((double)state1_sum / (double)total_buckets);
	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%5lu %5lu %5lu   %.2f", state1_min, state1_max, state1_sum, state1_avg);
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + DRAWING_WIDTH + 115, DRAWING_Y_OFFSET - DRAWING_HEIGHT + ((string_height + 5) * 1), (unsigned char *)temp_buffer, color_black);

	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%s:", (display_type == DISPLAY_SERVICE_HISTOGRAM) ? "Warning" : "Down");
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + DRAWING_WIDTH + 15, DRAWING_Y_OFFSET - DRAWING_HEIGHT + ((string_height + 5) * 2), (unsigned char *)temp_buffer, (display_type == DISPLAY_SERVICE_HISTOGRAM) ? color_yellow : color_red);

	state2_avg = (double)((double)state2_sum / (double)total_buckets);
	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%5lu %5lu %5lu   %.2f", state2_min, state2_max, state2_sum, state2_avg);
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + DRAWING_WIDTH + 115, DRAWING_Y_OFFSET - DRAWING_HEIGHT + ((string_height + 5) * 2), (unsigned char *)temp_buffer, color_black);

	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%s:", (display_type == DISPLAY_SERVICE_HISTOGRAM) ? "Unknown" : "Unreachable");
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + DRAWING_WIDTH + 15, DRAWING_Y_OFFSET - DRAWING_HEIGHT + ((string_height + 5) * 3), (unsigned char *)temp_buffer, (display_type == DISPLAY_SERVICE_HISTOGRAM) ? color_orange : color_darkred);

	state3_avg = (double)((double)state3_sum / (double)total_buckets);
	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%5lu %5lu %5lu   %.2f", state3_min, state3_max, state3_sum, state3_avg);
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	string_width = gdFontSmall->w * strlen(temp_buffer);
	gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + DRAWING_WIDTH + 115, DRAWING_Y_OFFSET - DRAWING_HEIGHT + ((string_height + 5) * 3), (unsigned char *)temp_buffer, color_black);

	if(display_type == DISPLAY_SERVICE_HISTOGRAM) {

		snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Critical:");
		temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
		string_width = gdFontSmall->w * strlen(temp_buffer);
		gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + DRAWING_WIDTH + 15, DRAWING_Y_OFFSET - DRAWING_HEIGHT + ((string_height + 5) * 4), (unsigned char *)temp_buffer, color_red);

		state4_avg = (double)((double)state4_sum / (double)total_buckets);
		snprintf(temp_buffer, sizeof(temp_buffer) - 1, "%5lu %5lu %5lu   %.2f", state4_min, state4_max, state4_sum, state4_avg);
		temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
		string_width = gdFontSmall->w * strlen(temp_buffer);
		gdImageString(histogram_image, gdFontSmall, DRAWING_X_OFFSET + DRAWING_WIDTH + 115, DRAWING_Y_OFFSET - DRAWING_HEIGHT + ((string_height + 5) * 4), (unsigned char *)temp_buffer, color_black);
		}

	return;
	}


/* adds an archived state entry */
void add_archived_state(int state_type, time_t time_stamp) {
	struct tm *our_time;
	int bucket;
	int skip_state = FALSE;

#ifdef DEBUG2
	printf("NEW ENTRY: last=%d this=%d\n", last_state, state_type);
#endif

	/* don't record program starts/stops, just make a note that one occurred */
	if(state_type == AS_PROGRAM_START || state_type == AS_PROGRAM_END) {
#ifdef DEBUG2
		printf("Recording a program start: %d\n", state_type);
#endif
		program_restart_has_occurred = TRUE;
		return;
		}

	/* see if we should even take into account this event */
	if(program_restart_has_occurred == TRUE) {

#ifdef DEBUG2
		printf("program_restart_has_occurred: last=%d this=%d\n", last_state, state_type);
#endif

		if(initial_states_logged == TRUE) {
			if(state_type == AS_SVC_OK && last_state == AS_SVC_OK)
				skip_state = TRUE;
			if(state_type == AS_HOST_UP && last_state == AS_HOST_UP)
				skip_state = TRUE;
			}

		if(assume_state_retention == TRUE && initial_states_logged == TRUE) {
			if(state_type == AS_SVC_WARNING && last_state == AS_SVC_WARNING)
				skip_state = TRUE;
			if(state_type == AS_SVC_UNKNOWN && last_state == AS_SVC_UNKNOWN)
				skip_state = TRUE;
			if(state_type == AS_SVC_CRITICAL && last_state == AS_SVC_CRITICAL)
				skip_state = TRUE;
			if(state_type == AS_HOST_DOWN && last_state == AS_HOST_DOWN)
				skip_state = TRUE;
			if(state_type == AS_HOST_UNREACHABLE && last_state == AS_HOST_UNREACHABLE)
				skip_state = TRUE;
			}

		if(skip_state == TRUE) {
			program_restart_has_occurred = FALSE;
#ifdef DEBUG2
			printf("Skipping state...\n");
#endif
			return;
			}
		}

	/* reset program restart variable */
	program_restart_has_occurred = FALSE;

	/* are we only processing new states */
	if(new_states_only == TRUE && state_type == last_state) {
#ifdef DEBUG2
		printf("Skipping state (not a new state)...\n");
#endif
		return;
		}

#ifdef DEBUG2
	printf("GOODSTATE: %d @ %lu\n", state_type, (unsigned long)time_stamp);
#endif



	our_time = localtime(&time_stamp);

	/* calculate the correct bucket to dump the data into */
	if(breakdown_type == BREAKDOWN_MONTHLY)
		bucket = our_time->tm_mon;

	else if(breakdown_type == BREAKDOWN_DAY_OF_MONTH)
		bucket = our_time->tm_mday - 1;

	else if(breakdown_type == BREAKDOWN_DAY_OF_WEEK)
		bucket = our_time->tm_wday;

	else
		bucket = (our_time->tm_hour * 4) + (our_time->tm_min / 15);

#ifdef DEBUG2
	printf("\tBucket=%d\n", bucket);
#endif

	/* save the data in the correct bucket */
	if(state_type == AS_SVC_OK)
		tsdata[bucket].service_ok++;
	else if(state_type == AS_SVC_UNKNOWN)
		tsdata[bucket].service_unknown++;
	else if(state_type == AS_SVC_WARNING)
		tsdata[bucket].service_warning++;
	else if(state_type == AS_SVC_CRITICAL)
		tsdata[bucket].service_critical++;
	else if(state_type == AS_HOST_UP)
		tsdata[bucket].host_up++;
	else if(state_type == AS_HOST_DOWN)
		tsdata[bucket].host_down++;
	else if(state_type == AS_HOST_UNREACHABLE)
		tsdata[bucket].host_unreachable++;

	/* record last state type */
	last_state = state_type;

	return;
	}



/* reads log files for archived state data */
void read_archived_state_data(void) {
	char filename[MAX_FILENAME_LENGTH];
	int newest_archive = 0;
	int oldest_archive = 0;
	int current_archive;

#ifdef DEBUG2
	printf("Determining archives to use...\n");
#endif

	/* determine earliest archive to use */
	oldest_archive = determine_archive_to_use_from_time(t1);
	if(log_rotation_method != LOG_ROTATION_NONE)
		oldest_archive += backtrack_archives;

	/* determine most recent archive to use */
	newest_archive = determine_archive_to_use_from_time(t2);

	if(oldest_archive < newest_archive)
		oldest_archive = newest_archive;

#ifdef DEBUG2
	printf("Oldest archive: %d\n", oldest_archive);
	printf("Newest archive: %d\n", newest_archive);
#endif

	/* read in all the necessary archived logs */
	for(current_archive = newest_archive; current_archive <= oldest_archive; current_archive++) {

		/* get the name of the log file that contains this archive */
		get_log_archive_to_use(current_archive, filename, sizeof(filename) - 1);

#ifdef DEBUG2
		printf("\tCurrent archive: %d (%s)\n", current_archive, filename);
#endif

		/* scan the log file for archived state data */
		scan_log_file_for_archived_state_data(filename);
		}

	return;
	}



/* grabs archives state data from a log file */
void scan_log_file_for_archived_state_data(char *filename) {
	char *input = NULL;
	char *input2 = NULL;
	char entry_host_name[MAX_INPUT_BUFFER];
	char entry_svc_description[MAX_INPUT_BUFFER];
	char *temp_buffer;
	time_t time_stamp;
	mmapfile *thefile;

	/* print something so browser doesn't time out */
	if(mode == CREATE_HTML) {
		printf(" ");
		fflush(NULL);
		}

	if((thefile = mmap_fopen(filename)) == NULL) {
#ifdef DEBUG2
		printf("Could not open file '%s' for reading.\n", filename);
#endif
		return;
		}

#ifdef DEBUG2
	printf("Scanning log file '%s' for archived state data...\n", filename);
#endif

	while(1) {

		/* free memory */
		free(input);
		free(input2);
		input = NULL;
		input2 = NULL;

		/* read the next line */
		if((input = mmap_fgets(thefile)) == NULL)
			break;

		strip(input);

		if((input2 = strdup(input)) == NULL)
			continue;

		temp_buffer = my_strtok(input2, "]");
		time_stamp = (temp_buffer == NULL) ? (time_t)0 : (time_t)strtoul(temp_buffer + 1, NULL, 10);

		/* program starts/restarts */
		if(strstr(input, " starting..."))
			add_archived_state(AS_PROGRAM_START, time_stamp);
		if(strstr(input, " restarting..."))
			add_archived_state(AS_PROGRAM_START, time_stamp);

		/* program stops */
		if(strstr(input, " shutting down..."))
			add_archived_state(AS_PROGRAM_END, time_stamp);
		if(strstr(input, "Bailing out"))
			add_archived_state(AS_PROGRAM_END, time_stamp);

		if(display_type == DISPLAY_HOST_HISTOGRAM) {
			if(strstr(input, "HOST ALERT:")) {

				/* get host name */
				temp_buffer = my_strtok(NULL, ":");
				temp_buffer = my_strtok(NULL, ";");
				strncpy(entry_host_name, (temp_buffer == NULL) ? "" : temp_buffer + 1, sizeof(entry_host_name));
				entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

				if(strcmp(host_name, entry_host_name))
					continue;

				/* skip soft states if necessary */
				if(!(graph_statetypes & GRAPH_SOFT_STATETYPES) && strstr(input, ";SOFT;"))
					continue;

				/* skip hard states if necessary */
				if(!(graph_statetypes & GRAPH_HARD_STATETYPES) && strstr(input, ";HARD;"))
					continue;

				if(strstr(input, ";DOWN;"))
					add_archived_state(AS_HOST_DOWN, time_stamp);
				else if(strstr(input, ";UNREACHABLE;"))
					add_archived_state(AS_HOST_UNREACHABLE, time_stamp);
				else if(strstr(input, ";RECOVERY") || strstr(input, ";UP;"))
					add_archived_state(AS_HOST_UP, time_stamp);
				}
			}
		if(display_type == DISPLAY_SERVICE_HISTOGRAM) {
			if(strstr(input, "SERVICE ALERT:")) {

				/* get host name */
				temp_buffer = my_strtok(NULL, ":");
				temp_buffer = my_strtok(NULL, ";");
				strncpy(entry_host_name, (temp_buffer == NULL) ? "" : temp_buffer + 1, sizeof(entry_host_name));
				entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

				if(strcmp(host_name, entry_host_name))
					continue;

				/* get service description */
				temp_buffer = my_strtok(NULL, ";");
				strncpy(entry_svc_description, (temp_buffer == NULL) ? "" : temp_buffer, sizeof(entry_svc_description));
				entry_svc_description[sizeof(entry_svc_description) - 1] = '\x0';

				if(strcmp(svc_description, entry_svc_description))
					continue;

				/* skip soft states if necessary */
				if(!(graph_statetypes & GRAPH_SOFT_STATETYPES) && strstr(input, ";SOFT;"))
					continue;

				/* skip hard states if necessary */
				if(!(graph_statetypes & GRAPH_HARD_STATETYPES) && strstr(input, ";HARD;"))
					continue;

				if(strstr(input, ";CRITICAL;"))
					add_archived_state(AS_SVC_CRITICAL, time_stamp);
				else if(strstr(input, ";WARNING;"))
					add_archived_state(AS_SVC_WARNING, time_stamp);
				else if(strstr(input, ";UNKNOWN;"))
					add_archived_state(AS_SVC_UNKNOWN, time_stamp);
				else if(strstr(input, ";RECOVERY;") || strstr(input, ";OK;"))
					add_archived_state(AS_SVC_OK, time_stamp);
				}
			}

		}

	/* free memory and close the file */
	free(input);
	free(input2);
	mmap_fclose(thefile);

	return;
	}




void convert_timeperiod_to_times(int type) {
	time_t current_time;
	struct tm *t;

	/* get the current time */
	time(&current_time);

	t = localtime(&current_time);

	t->tm_sec = 0;
	t->tm_min = 0;
	t->tm_hour = 0;
	t->tm_isdst = -1;

	switch(type) {
		case TIMEPERIOD_LAST24HOURS:
			t1 = current_time - (60 * 60 * 24);
			t2 = current_time;
			break;
		case TIMEPERIOD_TODAY:
			t1 = mktime(t);
			t2 = current_time;
			break;
		case TIMEPERIOD_YESTERDAY:
			t1 = (time_t)(mktime(t) - (60 * 60 * 24));
			t2 = (time_t)mktime(t);
			break;
		case TIMEPERIOD_THISWEEK:
			t1 = (time_t)(mktime(t) - (60 * 60 * 24 * t->tm_wday));
			t2 = current_time;
			break;
		case TIMEPERIOD_LASTWEEK:
			t1 = (time_t)(mktime(t) - (60 * 60 * 24 * t->tm_wday) - (60 * 60 * 24 * 7));
			t2 = (time_t)(mktime(t) - (60 * 60 * 24 * t->tm_wday));
			break;
		case TIMEPERIOD_THISMONTH:
			t->tm_mday = 1;
			t1 = mktime(t);
			t2 = current_time;
			break;
		case TIMEPERIOD_LASTMONTH:
			t->tm_mday = 1;
			t2 = mktime(t);
			if(t->tm_mon == 0) {
				t->tm_mon = 11;
				t->tm_year--;
				}
			else
				t->tm_mon--;
			t1 = mktime(t);
			break;
		case TIMEPERIOD_THISQUARTER:
			break;
		case TIMEPERIOD_LASTQUARTER:
			break;
		case TIMEPERIOD_THISYEAR:
			t->tm_mon = 0;
			t->tm_mday = 1;
			t1 = mktime(t);
			t2 = current_time;
			break;
		case TIMEPERIOD_LASTYEAR:
			t->tm_mon = 0;
			t->tm_mday = 1;
			t2 = mktime(t);
			t->tm_year--;
			t1 = mktime(t);
			break;
		case TIMEPERIOD_LAST7DAYS:
			t2 = current_time;
			t1 = current_time - (7 * 24 * 60 * 60);
			break;
		case TIMEPERIOD_LAST31DAYS:
			t2 = current_time;
			t1 = current_time - (31 * 24 * 60 * 60);
			break;
		default:
			break;
		}

	return;
	}



void compute_report_times(void) {
	time_t current_time;
	struct tm *st;
	struct tm *et;

	/* get the current time */
	time(&current_time);

	st = localtime(&current_time);

	st->tm_sec = start_second;
	st->tm_min = start_minute;
	st->tm_hour = start_hour;
	st->tm_mday = start_day;
	st->tm_mon = start_month - 1;
	st->tm_year = start_year - 1900;
	st->tm_isdst = -1;

	t1 = mktime(st);

	et = localtime(&current_time);

	et->tm_sec = end_second;
	et->tm_min = end_minute;
	et->tm_hour = end_hour;
	et->tm_mday = end_day;
	et->tm_mon = end_month - 1;
	et->tm_year = end_year - 1900;
	et->tm_isdst = -1;

	t2 = mktime(et);
	}



/* draws a solid line */
void draw_line(int x1, int y1, int x2, int y2, int color) {
	int styleSolid[1];

	styleSolid[0] = color;

	/* sets current style to a solid line */
	gdImageSetStyle(histogram_image, styleSolid, 1);

	/* draws a line (dashed) */
	gdImageLine(histogram_image, x1, y1, x2, y2, gdStyled);

	return;
	}


/* draws a dashed line */
void draw_dashed_line(int x1, int y1, int x2, int y2, int color) {
	int styleDashed[6];

	styleDashed[0] = color;
	styleDashed[1] = color;
	styleDashed[2] = gdTransparent;
	styleDashed[3] = gdTransparent;
	styleDashed[4] = gdTransparent;
	styleDashed[5] = gdTransparent;

	/* sets current style to a solid line */
	gdImageSetStyle(histogram_image, styleDashed, 6);

	/* draws a line (dashed) */
	gdImageLine(histogram_image, x1, y1, x2, y2, gdStyled);

	return;
	}
