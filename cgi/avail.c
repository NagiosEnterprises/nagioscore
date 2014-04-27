/**************************************************************************
 *
 * AVAIL.C -  Nagios Availability CGI
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
#include "../include/statusdata.h"

#include "../include/cgiutils.h"
#include "../include/cgiauth.h"
#include "../include/getcgi.h"


extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_html_path[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char url_stylesheets_path[MAX_FILENAME_LENGTH];

#ifndef max
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif



/* output types */
#define HTML_OUTPUT             0
#define CSV_OUTPUT              1


/* archived state types */
#define AS_CURRENT_STATE        -1   /* special case for initial assumed state */
#define AS_NO_DATA		0
#define AS_PROGRAM_END		1
#define AS_PROGRAM_START	2
#define AS_HOST_UP		3
#define AS_HOST_DOWN		4
#define AS_HOST_UNREACHABLE	5
#define AS_SVC_OK		6
#define AS_SVC_UNKNOWN		7
#define AS_SVC_WARNING		8
#define AS_SVC_CRITICAL		9

#define AS_SVC_DOWNTIME_START   10
#define AS_SVC_DOWNTIME_END     11
#define AS_HOST_DOWNTIME_START  12
#define AS_HOST_DOWNTIME_END    13

#define AS_SOFT_STATE           1
#define AS_HARD_STATE           2


/* display types */
#define DISPLAY_NO_AVAIL        0
#define DISPLAY_HOSTGROUP_AVAIL 1
#define DISPLAY_HOST_AVAIL      2
#define DISPLAY_SERVICE_AVAIL   3
#define DISPLAY_SERVICEGROUP_AVAIL 4

/* subject types */
#define HOST_SUBJECT            0
#define SERVICE_SUBJECT         1


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
#define TIMEPERIOD_LAST31DAYS	13

#define MIN_TIMESTAMP_SPACING	10

#define MAX_ARCHIVE_SPREAD	65
#define MAX_ARCHIVE		65
#define MAX_ARCHIVE_BACKTRACKS	60

authdata current_authdata;

typedef struct archived_state_struct {
	time_t  time_stamp;
	int     entry_type;
	int     state_type;
	char    *state_info;
	int     processed_state;
	struct archived_state_struct *misc_ptr;
	struct archived_state_struct *next;
	} archived_state;

typedef struct avail_subject_struct {
	int type;
	char *host_name;
	char *service_description;
	archived_state *as_list;        /* archived state list */
	archived_state *as_list_tail;
	archived_state *sd_list;        /* scheduled downtime list */
	int last_known_state;
	time_t earliest_time;
	time_t latest_time;
	int earliest_state;
	int latest_state;

	unsigned long time_up;
	unsigned long time_down;
	unsigned long time_unreachable;
	unsigned long time_ok;
	unsigned long time_warning;
	unsigned long time_unknown;
	unsigned long time_critical;

	unsigned long scheduled_time_up;
	unsigned long scheduled_time_down;
	unsigned long scheduled_time_unreachable;
	unsigned long scheduled_time_ok;
	unsigned long scheduled_time_warning;
	unsigned long scheduled_time_unknown;
	unsigned long scheduled_time_critical;
	unsigned long scheduled_time_indeterminate;

	unsigned long time_indeterminate_nodata;
	unsigned long time_indeterminate_notrunning;

	struct avail_subject_struct *next;
	} avail_subject;

avail_subject *subject_list = NULL;

time_t t1;
time_t t2;

int display_type = DISPLAY_NO_AVAIL;
int timeperiod_type = TIMEPERIOD_LAST24HOURS;
int show_log_entries = FALSE;
int full_log_entries = FALSE;
int show_scheduled_downtime = TRUE;

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

int get_date_parts = FALSE;
int select_hostgroups = FALSE;
int select_hosts = FALSE;
int select_servicegroups = FALSE;
int select_services = FALSE;
int select_output_format = FALSE;

int compute_time_from_parts = FALSE;

int show_all_hostgroups = FALSE;
int show_all_hosts = FALSE;
int show_all_servicegroups = FALSE;
int show_all_services = FALSE;

int assume_initial_states = TRUE;
int assume_state_retention = TRUE;
int assume_states_during_notrunning = TRUE;
int initial_assumed_host_state = AS_NO_DATA;
int initial_assumed_service_state = AS_NO_DATA;
int include_soft_states = FALSE;

char *hostgroup_name = "";
char *host_name = "";
char *servicegroup_name = "";
char *svc_description = "";

void create_subject_list(void);
void add_subject(int, char *, char *);
avail_subject *find_subject(int, char *, char *);
void compute_availability(void);
void compute_subject_availability(avail_subject *, time_t);
void compute_subject_availability_times(int, int, time_t, time_t, time_t, avail_subject *, archived_state *);
void compute_subject_downtime(avail_subject *, time_t);
void compute_subject_downtime_times(time_t, time_t, avail_subject *, archived_state *);
void compute_subject_downtime_part_times(time_t, time_t, int, avail_subject *);
void display_hostgroup_availability(void);
void display_specific_hostgroup_availability(hostgroup *);
void display_servicegroup_availability(void);
void display_specific_servicegroup_availability(servicegroup *);
void display_host_availability(void);
void display_service_availability(void);
void write_log_entries(avail_subject *);

void get_running_average(double *, double, int);

void host_report_url(const char *, const char *);
void service_report_url(const char *, const char *, const char *);
void compute_report_times(void);

int convert_host_state_to_archived_state(int);
int convert_service_state_to_archived_state(int);
void add_global_archived_state(int, int, time_t, const char *);
void add_archived_state(int, int, time_t, const char *, avail_subject *);
void add_scheduled_downtime(int, time_t, avail_subject *);
void free_availability_data(void);
void free_archived_state_list(archived_state *);
void read_archived_state_data(void);
void scan_log_file_for_archived_state_data(char *);
void convert_timeperiod_to_times(int);
unsigned long calculate_total_time(time_t, time_t);

void document_header(int);
void document_footer(void);
int process_cgivars(void);



int backtrack_archives = 2;
int earliest_archive = 0;

int embedded = FALSE;
int display_header = TRUE;

timeperiod *current_timeperiod = NULL;

int output_format = HTML_OUTPUT;



int main(int argc, char **argv) {
	char temp_buffer[MAX_INPUT_BUFFER];
	char start_timestring[MAX_DATETIME_LENGTH];
	char end_timestring[MAX_DATETIME_LENGTH];
	host *temp_host;
	service *temp_service;
	int is_authorized = TRUE;
	time_t report_start_time;
	time_t report_end_time;
	int days, hours, minutes, seconds;
	hostgroup *temp_hostgroup;
	servicegroup *temp_servicegroup;
	timeperiod *temp_timeperiod;
	time_t t3;
	time_t current_time;
	struct tm *t;
	char *firsthostpointer;

	/* reset internal CGI variables */
	reset_cgi_vars();

	cgi_init(document_header, document_footer, READ_ALL_OBJECT_DATA, READ_ALL_STATUS_DATA);

	/* initialize time period to last 24 hours */
	time(&current_time);
	t2 = current_time;
	t1 = (time_t)(current_time - (60 * 60 * 24));

	/* default number of backtracked archives */
	switch(log_rotation_method) {
		case LOG_ROTATION_MONTHLY:
			backtrack_archives = 1;
			break;
		case LOG_ROTATION_WEEKLY:
			backtrack_archives = 2;
			break;
		case LOG_ROTATION_DAILY:
			backtrack_archives = 4;
			break;
		case LOG_ROTATION_HOURLY:
			backtrack_archives = 8;
			break;
		default:
			backtrack_archives = 2;
			break;
		}

	/* get the arguments passed in the URL */
	process_cgivars();

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

	/* don't let user create reports in the future */
	if(t2 > current_time) {
		t2 = current_time;
		if(t1 > t2)
			t1 = t2 - (60 * 60 * 24);
		}

	if(display_header == TRUE) {

		/* begin top table */
		printf("<table border=0 width=100%% cellspacing=0 cellpadding=0>\n");
		printf("<tr>\n");

		/* left column of the first row */
		printf("<td align=left valign=top width=33%%>\n");

		switch(display_type) {
			case DISPLAY_HOST_AVAIL:
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Host Availability Report");
				break;
			case DISPLAY_SERVICE_AVAIL:
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Service Availability Report");
				break;
			case DISPLAY_HOSTGROUP_AVAIL:
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Hostgroup Availability Report");
				break;
			case DISPLAY_SERVICEGROUP_AVAIL:
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Servicegroup Availability Report");
				break;
			default:
				snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Availability Report");
				break;
			}
		temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
		display_info_table(temp_buffer, FALSE, &current_authdata);

		if(((display_type == DISPLAY_HOST_AVAIL && show_all_hosts == FALSE) || (display_type == DISPLAY_SERVICE_AVAIL && show_all_services == FALSE)) && get_date_parts == FALSE) {

			printf("<TABLE BORDER=1 CELLPADDING=0 CELLSPACING=0 CLASS='linkBox'>\n");
			printf("<TR><TD CLASS='linkBox'>\n");

			if(display_type == DISPLAY_HOST_AVAIL && show_all_hosts == FALSE) {
				host_report_url("all", "View Availability Report For All Hosts");
				printf("<BR>\n");
#ifdef USE_TRENDS
				printf("<a href='%s?host=%s&t1=%lu&t2=%lu&assumestateretention=%s&assumeinitialstates=%s&includesoftstates=%s&assumestatesduringnotrunning=%s&initialassumedhoststate=%d&backtrack=%d'>View Trends For This Host</a><BR>\n", TRENDS_CGI, url_encode(host_name), t1, t2, (include_soft_states == TRUE) ? "yes" : "no", (assume_state_retention == TRUE) ? "yes" : "no", (assume_initial_states == TRUE) ? "yes" : "no", (assume_states_during_notrunning == TRUE) ? "yes" : "no", initial_assumed_host_state, backtrack_archives);
#endif
#ifdef USE_HISTOGRAM
				printf("<a href='%s?host=%s&t1=%lu&t2=%lu&assumestateretention=%s'>View Alert Histogram For This Host</a><BR>\n", HISTOGRAM_CGI, url_encode(host_name), t1, t2, (assume_state_retention == TRUE) ? "yes" : "no");
#endif
				printf("<a href='%s?host=%s'>View Status Detail For This Host</a><BR>\n", STATUS_CGI, url_encode(host_name));
				printf("<a href='%s?host=%s'>View Alert History For This Host</a><BR>\n", HISTORY_CGI, url_encode(host_name));
				printf("<a href='%s?host=%s'>View Notifications For This Host</a><BR>\n", NOTIFICATIONS_CGI, url_encode(host_name));
				}
			else if(display_type == DISPLAY_SERVICE_AVAIL && show_all_services == FALSE) {
				host_report_url(host_name, "View Availability Report For This Host");
				printf("<BR>\n");
				service_report_url("null", "all", "View Availability Report For All Services");
				printf("<BR>\n");
#ifdef USE_TRENDS
				printf("<a href='%s?host=%s", TRENDS_CGI, url_encode(host_name));
				printf("&service=%s&t1=%lu&t2=%lu&assumestateretention=%s&includesoftstates=%s&assumeinitialstates=%s&assumestatesduringnotrunning=%s&initialassumedservicestate=%d&backtrack=%d'>View Trends For This Service</a><BR>\n", url_encode(svc_description), t1, t2, (include_soft_states == TRUE) ? "yes" : "no", (assume_state_retention == TRUE) ? "yes" : "no", (assume_initial_states == TRUE) ? "yes" : "no", (assume_states_during_notrunning == TRUE) ? "yes" : "no", initial_assumed_service_state, backtrack_archives);
#endif
#ifdef USE_HISTOGRAM
				printf("<a href='%s?host=%s", HISTOGRAM_CGI, url_encode(host_name));
				printf("&service=%s&t1=%lu&t2=%lu&assumestateretention=%s'>View Alert Histogram For This Service</a><BR>\n", url_encode(svc_description), t1, t2, (assume_state_retention == TRUE) ? "yes" : "no");
#endif
				printf("<A HREF='%s?host=%s&", HISTORY_CGI, url_encode(host_name));
				printf("service=%s'>View Alert History For This Service</A><BR>\n", url_encode(svc_description));
				printf("<A HREF='%s?host=%s&", NOTIFICATIONS_CGI, url_encode(host_name));
				printf("service=%s'>View Notifications For This Service</A><BR>\n", url_encode(svc_description));
				}

			printf("</TD></TR>\n");
			printf("</TABLE>\n");
			}

		printf("</td>\n");

		/* center column of top row */
		printf("<td align=center valign=top width=33%%>\n");

		if(display_type != DISPLAY_NO_AVAIL && get_date_parts == FALSE) {

			printf("<DIV ALIGN=CENTER CLASS='dataTitle'>\n");
			if(display_type == DISPLAY_HOST_AVAIL) {
				if(show_all_hosts == TRUE)
					printf("All Hosts");
				else
					printf("Host '%s'", host_name);
				}
			else if(display_type == DISPLAY_SERVICE_AVAIL) {
				if(show_all_services == TRUE)
					printf("All Services");
				else
					printf("Service '%s' On Host '%s'", svc_description, host_name);
				}
			else if(display_type == DISPLAY_HOSTGROUP_AVAIL) {
				if(show_all_hostgroups == TRUE)
					printf("All Hostgroups");
				else
					printf("Hostgroup '%s'", hostgroup_name);
				}
			else if(display_type == DISPLAY_SERVICEGROUP_AVAIL) {
				if(show_all_servicegroups == TRUE)
					printf("All Servicegroups");
				else
					printf("Servicegroup '%s'", servicegroup_name);
				}
			printf("</DIV>\n");

			printf("<BR>\n");

			printf("<IMG SRC='%s%s' BORDER=0 ALT='Availability Report' TITLE='Availability Report'>\n", url_images_path, TRENDS_ICON);

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

		printf("<form method=\"GET\" action=\"%s\">\n", AVAIL_CGI);
		printf("<table border=0 CLASS='optBox'>\n");

		if(display_type != DISPLAY_NO_AVAIL && get_date_parts == FALSE) {

			printf("<tr><td valign=top align=left class='optBoxItem'>First assumed %s state:</td><td valign=top align=left class='optBoxItem'>%s</td></tr>\n", (display_type == DISPLAY_SERVICE_AVAIL) ? "service" : "host", (display_type == DISPLAY_HOST_AVAIL || display_type == DISPLAY_HOSTGROUP_AVAIL || display_type == DISPLAY_SERVICEGROUP_AVAIL) ? "First assumed service state" : "");
			printf("<tr>\n");
			printf("<td valign=top align=left class='optBoxItem'>\n");

			printf("<input type='hidden' name='t1' value='%lu'>\n", (unsigned long)t1);
			printf("<input type='hidden' name='t2' value='%lu'>\n", (unsigned long)t2);
			if(show_log_entries == TRUE)
				printf("<input type='hidden' name='show_log_entries' value=''>\n");
			if(full_log_entries == TRUE)
				printf("<input type='hidden' name='full_log_entries' value=''>\n");
			if(display_type == DISPLAY_HOSTGROUP_AVAIL)
				printf("<input type='hidden' name='hostgroup' value='%s'>\n", escape_string(hostgroup_name));
			if(display_type == DISPLAY_HOST_AVAIL || display_type == DISPLAY_SERVICE_AVAIL)
				printf("<input type='hidden' name='host' value='%s'>\n", escape_string(host_name));
			if(display_type == DISPLAY_SERVICE_AVAIL)
				printf("<input type='hidden' name='service' value='%s'>\n", escape_string(svc_description));
			if(display_type == DISPLAY_SERVICEGROUP_AVAIL)
				printf("<input type='hidden' name='servicegroup' value='%s'>\n", escape_string(servicegroup_name));

			printf("<input type='hidden' name='assumeinitialstates' value='%s'>\n", (assume_initial_states == TRUE) ? "yes" : "no");
			printf("<input type='hidden' name='assumestateretention' value='%s'>\n", (assume_state_retention == TRUE) ? "yes" : "no");
			printf("<input type='hidden' name='assumestatesduringnotrunning' value='%s'>\n", (assume_states_during_notrunning == TRUE) ? "yes" : "no");
			printf("<input type='hidden' name='includesoftstates' value='%s'>\n", (include_soft_states == TRUE) ? "yes" : "no");

			if(display_type == DISPLAY_HOST_AVAIL || display_type == DISPLAY_HOSTGROUP_AVAIL || display_type == DISPLAY_SERVICEGROUP_AVAIL) {
				printf("<select name='initialassumedhoststate'>\n");
				printf("<option value=%d %s>Unspecified\n", AS_NO_DATA, (initial_assumed_host_state == AS_NO_DATA) ? "SELECTED" : "");
				printf("<option value=%d %s>Current State\n", AS_CURRENT_STATE, (initial_assumed_host_state == AS_CURRENT_STATE) ? "SELECTED" : "");
				printf("<option value=%d %s>Host Up\n", AS_HOST_UP, (initial_assumed_host_state == AS_HOST_UP) ? "SELECTED" : "");
				printf("<option value=%d %s>Host Down\n", AS_HOST_DOWN, (initial_assumed_host_state == AS_HOST_DOWN) ? "SELECTED" : "");
				printf("<option value=%d %s>Host Unreachable\n", AS_HOST_UNREACHABLE, (initial_assumed_host_state == AS_HOST_UNREACHABLE) ? "SELECTED" : "");
				printf("</select>\n");
				}
			else {
				printf("<input type='hidden' name='initialassumedhoststate' value='%d'>", initial_assumed_host_state);
				printf("<select name='initialassumedservicestate'>\n");
				printf("<option value=%d %s>Unspecified\n", AS_NO_DATA, (initial_assumed_service_state == AS_NO_DATA) ? "SELECTED" : "");
				printf("<option value=%d %s>Current State\n", AS_CURRENT_STATE, (initial_assumed_service_state == AS_CURRENT_STATE) ? "SELECTED" : "");
				printf("<option value=%d %s>Service Ok\n", AS_SVC_OK, (initial_assumed_service_state == AS_SVC_OK) ? "SELECTED" : "");
				printf("<option value=%d %s>Service Warning\n", AS_SVC_WARNING, (initial_assumed_service_state == AS_SVC_WARNING) ? "SELECTED" : "");
				printf("<option value=%d %s>Service Unknown\n", AS_SVC_UNKNOWN, (initial_assumed_service_state == AS_SVC_UNKNOWN) ? "SELECTED" : "");
				printf("<option value=%d %s>Service Critical\n", AS_SVC_CRITICAL, (initial_assumed_service_state == AS_SVC_CRITICAL) ? "SELECTED" : "");
				printf("</select>\n");
				}
			printf("</td>\n");
			printf("<td CLASS='optBoxItem'>\n");
			if(display_type == DISPLAY_HOST_AVAIL || display_type == DISPLAY_HOSTGROUP_AVAIL || display_type == DISPLAY_SERVICEGROUP_AVAIL) {
				printf("<select name='initialassumedservicestate'>\n");
				printf("<option value=%d %s>Unspecified\n", AS_NO_DATA, (initial_assumed_service_state == AS_NO_DATA) ? "SELECTED" : "");
				printf("<option value=%d %s>Current State\n", AS_CURRENT_STATE, (initial_assumed_service_state == AS_CURRENT_STATE) ? "SELECTED" : "");
				printf("<option value=%d %s>Service Ok\n", AS_SVC_OK, (initial_assumed_service_state == AS_SVC_OK) ? "SELECTED" : "");
				printf("<option value=%d %s>Service Warning\n", AS_SVC_WARNING, (initial_assumed_service_state == AS_SVC_WARNING) ? "SELECTED" : "");
				printf("<option value=%d %s>Service Unknown\n", AS_SVC_UNKNOWN, (initial_assumed_service_state == AS_SVC_UNKNOWN) ? "SELECTED" : "");
				printf("<option value=%d %s>Service Critical\n", AS_SVC_CRITICAL, (initial_assumed_service_state == AS_SVC_CRITICAL) ? "SELECTED" : "");
				printf("</select>\n");
				}
			printf("</td>\n");
			printf("</tr>\n");

			printf("<tr><td valign=top align=left class='optBoxItem'>Report period:</td><td valign=top align=left class='optBoxItem'>Backtracked archives:</td></tr>\n");
			printf("<tr>\n");
			printf("<td valign=top align=left class='optBoxItem'>\n");
			printf("<select name='timeperiod'>\n");
			printf("<option SELECTED>[ Current time range ]\n");
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
			printf("</td>\n");
			printf("<td valign=top align=left CLASS='optBoxItem'>\n");
			printf("<input type='text' size='2' maxlength='2' name='backtrack' value='%d'>\n", backtrack_archives);
			printf("</td>\n");
			printf("</tr>\n");

			printf("<tr><td valign=top align=left></td>\n");
			printf("<td valign=top align=left CLASS='optBoxItem'>\n");
			printf("<input type='submit' value='Update'>\n");
			printf("</td>\n");
			printf("</tr>\n");
			}

		/* display context-sensitive help */
		printf("<tr><td></td><td align=right valign=bottom>\n");
		if(get_date_parts == TRUE)
			display_context_help(CONTEXTHELP_AVAIL_MENU5);
		else if(select_hostgroups == TRUE)
			display_context_help(CONTEXTHELP_AVAIL_MENU2);
		else if(select_hosts == TRUE)
			display_context_help(CONTEXTHELP_AVAIL_MENU3);
		else if(select_services == TRUE)
			display_context_help(CONTEXTHELP_AVAIL_MENU4);
		else if(display_type == DISPLAY_HOSTGROUP_AVAIL)
			display_context_help(CONTEXTHELP_AVAIL_HOSTGROUP);
		else if(display_type == DISPLAY_HOST_AVAIL)
			display_context_help(CONTEXTHELP_AVAIL_HOST);
		else if(display_type == DISPLAY_SERVICE_AVAIL)
			display_context_help(CONTEXTHELP_AVAIL_SERVICE);
		else if(display_type == DISPLAY_SERVICEGROUP_AVAIL)
			display_context_help(CONTEXTHELP_AVAIL_SERVICEGROUP);
		else
			display_context_help(CONTEXTHELP_AVAIL_MENU1);
		printf("</td></tr>\n");

		printf("</table>\n");
		printf("</form>\n");

		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");
		}




	/* step 3 - ask user for report date range */
	if(get_date_parts == TRUE) {

		time(&current_time);
		t = localtime(&current_time);

		start_day = 1;
		start_year = t->tm_year + 1900;
		end_day = t->tm_mday;
		end_year = t->tm_year + 1900;

		printf("<P><DIV ALIGN=CENTER CLASS='dateSelectTitle'>Step 3: Select Report Options</DIV></p>\n");

		printf("<P><DIV ALIGN=CENTER>\n");

		printf("<form method=\"get\" action=\"%s\">\n", AVAIL_CGI);
		printf("<input type='hidden' name='show_log_entries' value=''>\n");
		if(display_type == DISPLAY_HOSTGROUP_AVAIL)
			printf("<input type='hidden' name='hostgroup' value='%s'>\n", escape_string(hostgroup_name));
		if(display_type == DISPLAY_HOST_AVAIL || display_type == DISPLAY_SERVICE_AVAIL)
			printf("<input type='hidden' name='host' value='%s'>\n", escape_string(host_name));
		if(display_type == DISPLAY_SERVICE_AVAIL)
			printf("<input type='hidden' name='service' value='%s'>\n", escape_string(svc_description));
		if(display_type == DISPLAY_SERVICEGROUP_AVAIL)
			printf("<input type='hidden' name='servicegroup' value='%s'>\n", escape_string(servicegroup_name));

		printf("<table border=0 cellpadding=5>\n");

		printf("<tr>");
		printf("<td valign=top class='reportSelectSubTitle'>Report Period:</td>\n");
		printf("<td valign=top align=left class='optBoxItem'>\n");
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
		printf("</td>\n");
		printf("</tr>\n");

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

		printf("<tr>");
		printf("<td valign=top class='reportSelectSubTitle'>Report time Period:</td>\n");
		printf("<td valign=top align=left class='optBoxItem'>\n");
		printf("<select name='rpttimeperiod'>\n");
		printf("<option value=\"\">None\n");
		/* check all the time periods... */
		for(temp_timeperiod = timeperiod_list; temp_timeperiod != NULL; temp_timeperiod = temp_timeperiod->next)
			printf("<option value=%s>%s\n", escape_string(temp_timeperiod->name), temp_timeperiod->name);
		printf("</select>\n");
		printf("</td>\n");
		printf("</tr>\n");
		printf("<tr><td colspan=2><br></td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Assume Initial States:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='assumeinitialstates'>\n");
		printf("<option value=yes>Yes\n");
		printf("<option value=no>No\n");
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Assume State Retention:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='assumestateretention'>\n");
		printf("<option value=yes>Yes\n");
		printf("<option value=no>No\n");
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Assume States During Program Downtime:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='assumestatesduringnotrunning'>\n");
		printf("<option value=yes>Yes\n");
		printf("<option value=no>No\n");
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Include Soft States:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='includesoftstates'>\n");
		printf("<option value=yes>Yes\n");
		printf("<option value=no SELECTED>No\n");
		printf("</select>\n");
		printf("</td></tr>\n");

		if(display_type != DISPLAY_SERVICE_AVAIL) {
			printf("<tr><td class='reportSelectSubTitle' align=right>First Assumed Host State:</td>\n");
			printf("<td class='reportSelectItem'>\n");
			printf("<select name='initialassumedhoststate'>\n");
			printf("<option value=%d>Unspecified\n", AS_NO_DATA);
			printf("<option value=%d>Current State\n", AS_CURRENT_STATE);
			printf("<option value=%d>Host Up\n", AS_HOST_UP);
			printf("<option value=%d>Host Down\n", AS_HOST_DOWN);
			printf("<option value=%d>Host Unreachable\n", AS_HOST_UNREACHABLE);
			printf("</select>\n");
			printf("</td></tr>\n");
			}

		printf("<tr><td class='reportSelectSubTitle' align=right>First Assumed Service State:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='initialassumedservicestate'>\n");
		printf("<option value=%d>Unspecified\n", AS_NO_DATA);
		printf("<option value=%d>Current State\n", AS_CURRENT_STATE);
		printf("<option value=%d>Service Ok\n", AS_SVC_OK);
		printf("<option value=%d>Service Warning\n", AS_SVC_WARNING);
		printf("<option value=%d>Service Unknown\n", AS_SVC_UNKNOWN);
		printf("<option value=%d>Service Critical\n", AS_SVC_CRITICAL);
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Backtracked Archives (To Scan For Initial States):</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<input type='text' name='backtrack' size='2' maxlength='2' value='%d'>\n", backtrack_archives);
		printf("</td></tr>\n");

		if((display_type == DISPLAY_HOST_AVAIL && show_all_hosts == TRUE) || (display_type == DISPLAY_SERVICE_AVAIL && show_all_services == TRUE)) {
			printf("<tr>");
			printf("<td valign=top class='reportSelectSubTitle'>Output in CSV Format:</td>\n");
			printf("<td valign=top class='reportSelectItem'>");
			printf("<input type='checkbox' name='csvoutput' value=''>\n");
			printf("</td>\n");
			printf("</tr>\n");
			}

		printf("<tr><td></td><td align=left class='dateSelectItem'><input type='submit' value='Create Availability Report!'></td></tr>\n");

		printf("</table>\n");

		printf("</form>\n");
		printf("</DIV></P>\n");
		}


	/* step 2 - the user wants to select a hostgroup */
	else if(select_hostgroups == TRUE) {
		printf("<p><div align=center class='reportSelectTitle'>Step 2: Select Hostgroup</div></p>\n");

		printf("<p><div align=center>\n");

		printf("<form method=\"get\" action=\"%s\">\n", AVAIL_CGI);
		printf("<input type='hidden' name='get_date_parts'>\n");

		printf("<table border=0 cellpadding=5>\n");

		printf("<tr><td class='reportSelectSubTitle' valign=center>Hostgroup(s):</td><td align=left valign=center class='reportSelectItem'>\n");
		printf("<select name='hostgroup'>\n");
		printf("<option value='all'>** ALL HOSTGROUPS **\n");
		for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {
			if(is_authorized_for_hostgroup(temp_hostgroup, &current_authdata) == TRUE)
				printf("<option value='%s'>%s\n", escape_string(temp_hostgroup->group_name), temp_hostgroup->group_name);
			}
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td></td><td align=left class='dateSelectItem'><input type='submit' value='Continue to Step 3'></td></tr>\n");

		printf("</table>\n");

		printf("</form>\n");

		printf("</div></p>\n");
		}

	/* step 2 - the user wants to select a host */
	else if(select_hosts == TRUE) {
		printf("<p><div align=center class='reportSelectTitle'>Step 2: Select Host</div></p>\n");

		printf("<p><div align=center>\n");

		printf("<form method=\"get\" action=\"%s\">\n", AVAIL_CGI);
		printf("<input type='hidden' name='get_date_parts'>\n");

		printf("<table border=0 cellpadding=5>\n");

		printf("<tr><td class='reportSelectSubTitle' valign=center>Host(s):</td><td align=left valign=center class='reportSelectItem'>\n");
		printf("<select name='host'>\n");
		printf("<option value='all'>** ALL HOSTS **\n");
		for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
			if(is_authorized_for_host(temp_host, &current_authdata) == TRUE)
				printf("<option value='%s'>%s\n", escape_string(temp_host->name), temp_host->name);
			}
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td></td><td align=left class='dateSelectItem'><input type='submit' value='Continue to Step 3'></td></tr>\n");

		printf("</table>\n");

		printf("</form>\n");

		printf("</div></p>\n");

		printf("<div align=center class='helpfulHint'>Tip: If you want to have the option of getting the availability data in CSV format, select '<b>** ALL HOSTS **</b>' from the pull-down menu.\n");
		}

	/* step 2 - the user wants to select a servicegroup */
	else if(select_servicegroups == TRUE) {
		printf("<p><div align=center class='reportSelectTitle'>Step 2: Select Servicegroup</div></p>\n");

		printf("<p><div align=center>\n");

		printf("<form method=\"get\" action=\"%s\">\n", AVAIL_CGI);
		printf("<input type='hidden' name='get_date_parts'>\n");

		printf("<table border=0 cellpadding=5>\n");

		printf("<tr><td class='reportSelectSubTitle' valign=center>Servicegroup(s):</td><td align=left valign=center class='reportSelectItem'>\n");
		printf("<select name='servicegroup'>\n");
		printf("<option value='all'>** ALL SERVICEGROUPS **\n");
		for(temp_servicegroup = servicegroup_list; temp_servicegroup != NULL; temp_servicegroup = temp_servicegroup->next) {
			if(is_authorized_for_servicegroup(temp_servicegroup, &current_authdata) == TRUE)
				printf("<option value='%s'>%s\n", escape_string(temp_servicegroup->group_name), temp_servicegroup->group_name);
			}
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td></td><td align=left class='dateSelectItem'><input type='submit' value='Continue to Step 3'></td></tr>\n");

		printf("</table>\n");

		printf("</form>\n");

		printf("</div></p>\n");
		}

	/* step 2 - the user wants to select a service */
	else if(select_services == TRUE) {

		printf("<SCRIPT LANGUAGE='JavaScript'>\n");
		printf("function gethostname(hostindex){\n");
		printf("hostnames=[\"all\"");

		firsthostpointer = NULL;
		for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {
			if(is_authorized_for_service(temp_service, &current_authdata) == TRUE) {
				if(!firsthostpointer)
					firsthostpointer = temp_service->host_name;
				printf(", \"%s\"", temp_service->host_name);
				}
			}

		printf(" ]\n");
		printf("return hostnames[hostindex];\n");
		printf("}\n");
		printf("</SCRIPT>\n");

		printf("<p><div align=center class='reportSelectTitle'>Step 2: Select Service</div></p>\n");

		printf("<p><div align=center>\n");

		printf("<form method=\"get\" action=\"%s\" name='serviceform'>\n", AVAIL_CGI);
		printf("<input type='hidden' name='get_date_parts'>\n");
		printf("<input type='hidden' name='host' value='%s'>\n", (firsthostpointer == NULL) ? "unknown" : (char *)escape_string(firsthostpointer));

		printf("<table border=0 cellpadding=5>\n");

		printf("<tr><td class='reportSelectSubTitle' valign=center>Service(s):</td><td align=left valign=center class='reportSelectItem'>\n");
		printf("<select name='service' onFocus='document.serviceform.host.value=gethostname(this.selectedIndex);' onChange='document.serviceform.host.value=gethostname(this.selectedIndex);'>\n");
		printf("<option value='all'>** ALL SERVICES **\n");
		for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {
			if(is_authorized_for_service(temp_service, &current_authdata) == TRUE)
				printf("<option value='%s'>%s;%s\n", escape_string(temp_service->description), temp_service->host_name, temp_service->description);
			}

		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td></td><td align=left class='dateSelectItem'><input type='submit' value='Continue to Step 3'></td></tr>\n");

		printf("</table>\n");

		printf("</form>\n");

		printf("</div></p>\n");

		printf("<div align=center class='helpfulHint'>Tip: If you want to have the option of getting the availability data in CSV format, select '<b>** ALL SERVICES **</b>' from the pull-down menu.\n");
		}


	/* generate availability report */
	else if(display_type != DISPLAY_NO_AVAIL) {

		/* check authorization */
		is_authorized = TRUE;
		if((display_type == DISPLAY_HOST_AVAIL && show_all_hosts == FALSE) || (display_type == DISPLAY_SERVICE_AVAIL && show_all_services == FALSE)) {

			if(display_type == DISPLAY_HOST_AVAIL && show_all_hosts == FALSE)
				is_authorized = is_authorized_for_host(find_host(host_name), &current_authdata);
			else
				is_authorized = is_authorized_for_service(find_service(host_name, svc_description), &current_authdata);
			}

		if(is_authorized == FALSE)
			printf("<P><DIV ALIGN=CENTER CLASS='errorMessage'>It appears as though you are not authorized to view information for the specified %s...</DIV></P>\n", (display_type == DISPLAY_HOST_AVAIL) ? "host" : "service");

		else {

			time(&report_start_time);

			/* create list of subjects to collect availability data for */
			create_subject_list();

			/* read in all necessary archived state data */
			read_archived_state_data();

			/* compute availability data */
			compute_availability();

			time(&report_end_time);

			if(output_format == HTML_OUTPUT) {
				get_time_breakdown((time_t)(report_end_time - report_start_time), &days, &hours, &minutes, &seconds);
				printf("<div align=center class='reportTime'>[ Availability report completed in %d min %d sec ]</div>\n", minutes, seconds);
				printf("<BR><BR>\n");
				}

			/* display availability data */
			if(display_type == DISPLAY_HOST_AVAIL)
				display_host_availability();
			else if(display_type == DISPLAY_SERVICE_AVAIL)
				display_service_availability();
			else if(display_type == DISPLAY_HOSTGROUP_AVAIL)
				display_hostgroup_availability();
			else if(display_type == DISPLAY_SERVICEGROUP_AVAIL)
				display_servicegroup_availability();

			/* free memory allocated to availability data */
			free_availability_data();
			}
		}


	/* step 1 - ask the user what kind of report they want */
	else {

		printf("<p><div align=center class='reportSelectTitle'>Step 1: Select Report Type</div></p>\n");

		printf("<p><div align=center>\n");

		printf("<form method=\"get\" action=\"%s\">\n", AVAIL_CGI);

		printf("<table border=0 cellpadding=5>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Type:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='report_type'>\n");
		printf("<option value=hostgroups>Hostgroup(s)\n");
		printf("<option value=hosts>Host(s)\n");
		printf("<option value=servicegroups>Servicegroup(s)\n");
		printf("<option value=services>Service(s)\n");
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td></td><td align=left class='dateSelectItem'><input type='submit' value='Continue to Step 2'></td></tr>\n");

		printf("</table>\n");

		printf("</form>\n");

		printf("</div></p>\n");
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

	printf("Cache-Control: no-store\r\n");
	printf("Pragma: no-cache\r\n");

	time(&current_time);
	get_time_string(&current_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
	printf("Last-Modified: %s\r\n", date_time);

	expire_time = (time_t)0;
	get_time_string(&expire_time, date_time, sizeof(date_time), HTTP_DATE_TIME);
	printf("Expires: %s\r\n", date_time);

	if(output_format == HTML_OUTPUT)
		printf("Content-type: text/html\r\n\r\n");
	else {
		printf("Content-type: text/csv\r\n\r\n");
		return;
		}

	if(embedded == TRUE || output_format == CSV_OUTPUT)
		return;

	printf("<html>\n");
	printf("<head>\n");
	printf("<link rel=\"shortcut icon\" href=\"%sfavicon.ico\" type=\"image/ico\">\n", url_images_path);
	printf("<title>\n");
	printf("Nagios Availability\n");
	printf("</title>\n");

	if(use_stylesheet == TRUE) {
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, COMMON_CSS);
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, AVAIL_CSS);
		}

	printf("</head>\n");

	printf("<BODY CLASS='avail'>\n");

	/* include user SSI header */
	include_ssi_files(AVAIL_CGI, SSI_HEADER);

	return;
	}



void document_footer(void) {

	if(output_format != HTML_OUTPUT)
		return;

	if(embedded == TRUE)
		return;

	/* include user SSI footer */
	include_ssi_files(AVAIL_CGI, SSI_FOOTER);

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

		/* we found the hostgroup argument */
		else if(!strcmp(variables[x], "hostgroup")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((hostgroup_name = (char *)strdup(variables[x])) == NULL)
				hostgroup_name = "";
			strip_html_brackets(hostgroup_name);

			display_type = DISPLAY_HOSTGROUP_AVAIL;
			show_all_hostgroups = (strcmp(hostgroup_name, "all")) ? FALSE : TRUE;
			}

		/* we found the servicegroup argument */
		else if(!strcmp(variables[x], "servicegroup")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((servicegroup_name = (char *)strdup(variables[x])) == NULL)
				servicegroup_name = "";
			strip_html_brackets(servicegroup_name);

			display_type = DISPLAY_SERVICEGROUP_AVAIL;
			show_all_servicegroups = (strcmp(servicegroup_name, "all")) ? FALSE : TRUE;
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

			display_type = DISPLAY_HOST_AVAIL;
			show_all_hosts = (strcmp(host_name, "all")) ? FALSE : TRUE;
			}

		/* we found the service description argument */
		else if(!strcmp(variables[x], "service")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((svc_description = (char *)strdup(variables[x])) == NULL)
				svc_description = "";
			strip_html_brackets(svc_description);

			display_type = DISPLAY_SERVICE_AVAIL;
			show_all_services = (strcmp(svc_description, "all")) ? FALSE : TRUE;
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
			compute_time_from_parts = FALSE;
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
			compute_time_from_parts = FALSE;
			}

		/* we found the assume initial states option */
		else if(!strcmp(variables[x], "assumeinitialstates")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "yes"))
				assume_initial_states = TRUE;
			else
				assume_initial_states = FALSE;
			}

		/* we found the assume state during program not running option */
		else if(!strcmp(variables[x], "assumestatesduringnotrunning")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "yes"))
				assume_states_during_notrunning = TRUE;
			else
				assume_states_during_notrunning = FALSE;
			}

		/* we found the initial assumed host state option */
		else if(!strcmp(variables[x], "initialassumedhoststate")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			initial_assumed_host_state = atoi(variables[x]);
			}

		/* we found the initial assumed service state option */
		else if(!strcmp(variables[x], "initialassumedservicestate")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			initial_assumed_service_state = atoi(variables[x]);
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

		/* we found the include soft states option */
		else if(!strcmp(variables[x], "includesoftstates")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "yes"))
				include_soft_states = TRUE;
			else
				include_soft_states = FALSE;
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

#ifdef DEBUG
			printf("BACKTRACK ARCHIVES: %d\n", backtrack_archives);
#endif
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
				continue;

			convert_timeperiod_to_times(timeperiod_type);
			compute_time_from_parts = FALSE;
			}

		/* we found the embed option */
		else if(!strcmp(variables[x], "embedded"))
			embedded = TRUE;

		/* we found the noheader option */
		else if(!strcmp(variables[x], "noheader"))
			display_header = FALSE;

		/* we found the CSV output option */
		else if(!strcmp(variables[x], "csvoutput")) {
			display_header = FALSE;
			output_format = CSV_OUTPUT;
			}

		/* we found the log entries option  */
		else if(!strcmp(variables[x], "show_log_entries"))
			show_log_entries = TRUE;

		/* we found the full log entries option */
		else if(!strcmp(variables[x], "full_log_entries"))
			full_log_entries = TRUE;

		/* we found the get date parts option */
		else if(!strcmp(variables[x], "get_date_parts"))
			get_date_parts = TRUE;

		/* we found the report type selection option */
		else if(!strcmp(variables[x], "report_type")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}
			if(!strcmp(variables[x], "hostgroups"))
				select_hostgroups = TRUE;
			else if(!strcmp(variables[x], "servicegroups"))
				select_servicegroups = TRUE;
			else if(!strcmp(variables[x], "hosts"))
				select_hosts = TRUE;
			else
				select_services = TRUE;
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

		/* we found the show scheduled downtime option */
		else if(!strcmp(variables[x], "showscheduleddowntime")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if(!strcmp(variables[x], "yes"))
				show_scheduled_downtime = TRUE;
			else
				show_scheduled_downtime = FALSE;
			}

		/* we found the report timeperiod option */
		else if(!strcmp(variables[x], "rpttimeperiod")) {
			timeperiod *temp_timeperiod;
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			for(temp_timeperiod = timeperiod_list; temp_timeperiod != NULL; temp_timeperiod = temp_timeperiod->next) {
				if(!strcmp(url_encode(temp_timeperiod->name), variables[x])) {
					current_timeperiod = temp_timeperiod;
					break;
					}
				}
			}

		}

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return error;
	}



/* computes availability data for all subjects */
void compute_availability(void) {
	avail_subject *temp_subject;
	time_t current_time;

	time(&current_time);

	for(temp_subject = subject_list; temp_subject != NULL; temp_subject = temp_subject->next) {
		compute_subject_availability(temp_subject, current_time);
		compute_subject_downtime(temp_subject, current_time);
		}

	return;
	}



/* computes availability data for a given subject */
void compute_subject_availability(avail_subject *subject, time_t current_time) {
	archived_state *temp_as;
	archived_state *last_as;
	time_t a;
	time_t b;
	int current_state = AS_NO_DATA;
	int have_some_real_data = FALSE;
	hoststatus *hststatus = NULL;
	servicestatus *svcstatus = NULL;
	time_t initial_assumed_time;
	int initial_assumed_state = AS_NO_DATA;
	int error;


	/* if left hand of graph is after current time, we can't do anything at all.... */
	if(t1 > current_time)
		return;

	/* get current state of host or service if possible */
	if(subject->type == HOST_SUBJECT)
		hststatus = find_hoststatus(subject->host_name);
	else
		svcstatus = find_servicestatus(subject->host_name, subject->service_description);


	/************************************/
	/* INSERT CURRENT STATE (IF WE CAN) */
	/************************************/

	/* if current time DOES NOT fall within graph bounds, so we can't do anything as far as assuming current state */

	/* if we don't have any data, assume current state (if possible) */
	if(subject->as_list == NULL && current_time > t1 && current_time <= t2) {

		/* we don't have any historical information, but the current time falls within the reporting period, so use */
		/* the current status of the host/service as the starting data */
		if(subject->type == HOST_SUBJECT) {
			if(hststatus != NULL) {

				if(hststatus->status == SD_HOST_DOWN)
					subject->last_known_state = AS_HOST_DOWN;
				else if(hststatus->status == SD_HOST_UNREACHABLE)
					subject->last_known_state = AS_HOST_UNREACHABLE;
				else if(hststatus->status == SD_HOST_UP)
					subject->last_known_state = AS_HOST_UP;
				else
					subject->last_known_state = AS_NO_DATA;

				if(subject->last_known_state != AS_NO_DATA) {

					/* add a dummy archived state item, so something can get graphed */
					add_archived_state(subject->last_known_state, AS_HARD_STATE, t1, "Current Host State Assumed (Faked Log Entry)", subject);
					}
				}
			}
		else {
			if(svcstatus != NULL) {

				if(svcstatus->status == SERVICE_OK)
					subject->last_known_state = AS_SVC_OK;
				else if(svcstatus->status == SERVICE_WARNING)
					subject->last_known_state = AS_SVC_WARNING;
				else if(svcstatus->status == SERVICE_CRITICAL)
					subject->last_known_state = AS_SVC_CRITICAL;
				else if(svcstatus->status == SERVICE_UNKNOWN)
					subject->last_known_state = AS_SVC_UNKNOWN;
				else
					subject->last_known_state = AS_NO_DATA;

				if(subject->last_known_state != AS_NO_DATA) {

					/* add a dummy archived state item, so something can get graphed */
					add_archived_state(subject->last_known_state, AS_HARD_STATE, t1, "Current Service State Assumed (Faked Log Entry)", subject);
					}
				}
			}
		}



	/******************************************/
	/* INSERT FIRST ASSUMED STATE (IF WE CAN) */
	/******************************************/

	if((subject->type == HOST_SUBJECT && initial_assumed_host_state != AS_NO_DATA) || (subject->type == SERVICE_SUBJECT && initial_assumed_service_state != AS_NO_DATA)) {

		/* see if its okay to assume initial state for this subject */
		error = FALSE;
		if(subject->type == SERVICE_SUBJECT) {
			if(initial_assumed_service_state != AS_SVC_OK && initial_assumed_service_state != AS_SVC_WARNING && initial_assumed_service_state != AS_SVC_UNKNOWN && initial_assumed_service_state != AS_SVC_CRITICAL && initial_assumed_service_state != AS_CURRENT_STATE)
				error = TRUE;
			else
				initial_assumed_state = initial_assumed_service_state;
			if(initial_assumed_service_state == AS_CURRENT_STATE && svcstatus == NULL)
				error = TRUE;
			}
		else {
			if(initial_assumed_host_state != AS_HOST_UP && initial_assumed_host_state != AS_HOST_DOWN && initial_assumed_host_state != AS_HOST_UNREACHABLE && initial_assumed_host_state != AS_CURRENT_STATE)
				error = TRUE;
			else
				initial_assumed_state = initial_assumed_host_state;
			if(initial_assumed_host_state == AS_CURRENT_STATE && hststatus == NULL)
				error = TRUE;
			}

		/* get the current state if applicable */
		if(((subject->type == HOST_SUBJECT && initial_assumed_host_state == AS_CURRENT_STATE) || (subject->type == SERVICE_SUBJECT && initial_assumed_service_state == AS_CURRENT_STATE)) && error == FALSE) {
			if(subject->type == SERVICE_SUBJECT) {
				switch(svcstatus->status) {
					case SERVICE_OK:
						initial_assumed_state = AS_SVC_OK;
						break;
					case SERVICE_WARNING:
						initial_assumed_state = AS_SVC_WARNING;
						break;
					case SERVICE_UNKNOWN:
						initial_assumed_state = AS_SVC_UNKNOWN;
						break;
					case SERVICE_CRITICAL:
						initial_assumed_state = AS_SVC_CRITICAL;
						break;
					default:
						error = TRUE;
						break;
					}
				}
			else {
				switch(hststatus->status) {
					case SD_HOST_DOWN:
						initial_assumed_state = AS_HOST_DOWN;
						break;
					case SD_HOST_UNREACHABLE:
						initial_assumed_state = AS_HOST_UNREACHABLE;
						break;
					case SD_HOST_UP:
						initial_assumed_state = AS_HOST_UP;
						break;
					default:
						error = TRUE;
						break;
					}
				}
			}

		if(error == FALSE) {

			/* add this assumed state entry before any entries in the list and <= t1 */
			if(subject->as_list == NULL)
				initial_assumed_time = t1;
			else if(subject->as_list->time_stamp > t1)
				initial_assumed_time = t1;
			else
				initial_assumed_time = subject->as_list->time_stamp - 1;

			if(subject->type == HOST_SUBJECT)
				add_archived_state(initial_assumed_state, AS_HARD_STATE, initial_assumed_time, "First Host State Assumed (Faked Log Entry)", subject);
			else
				add_archived_state(initial_assumed_state, AS_HARD_STATE, initial_assumed_time, "First Service State Assumed (Faked Log Entry)", subject);
			}
		}




	/**************************************/
	/* BAIL OUT IF WE DON'T HAVE ANYTHING */
	/**************************************/

	have_some_real_data = FALSE;
	for(temp_as = subject->as_list; temp_as != NULL; temp_as = temp_as->next) {
		if(temp_as->entry_type != AS_NO_DATA && temp_as->entry_type != AS_PROGRAM_START && temp_as->entry_type != AS_PROGRAM_END) {
			have_some_real_data = TRUE;
			break;
			}
		}
	if(have_some_real_data == FALSE)
		return;




	last_as = NULL;
	subject->earliest_time = t2;
	subject->latest_time = t1;


#ifdef DEBUG
	printf("--- BEGINNING/MIDDLE SECTION ---<BR>\n");
#endif

	/**********************************/
	/*    BEGINNING/MIDDLE SECTION    */
	/**********************************/

	for(temp_as = subject->as_list; temp_as != NULL; temp_as = temp_as->next) {

		/* keep this as last known state if this is the first entry or if it occurs before the starting point of the graph */
		if((temp_as->time_stamp <= t1 || temp_as == subject->as_list) && (temp_as->entry_type != AS_NO_DATA && temp_as->entry_type != AS_PROGRAM_END && temp_as->entry_type != AS_PROGRAM_START)) {
			subject->last_known_state = temp_as->entry_type;
#ifdef DEBUG
			printf("SETTING LAST KNOWN STATE=%d<br>\n", subject->last_known_state);
#endif
			}

		/* skip this entry if it occurs before the starting point of the graph */
		if(temp_as->time_stamp <= t1) {
#ifdef DEBUG
			printf("SKIPPING PRE-EVENT: %d @ %lu<br>\n", temp_as->entry_type, temp_as->time_stamp);
#endif
			last_as = temp_as;
			continue;
			}

		/* graph this span if we're not on the first item */
		if(last_as != NULL) {

			a = last_as->time_stamp;
			b = temp_as->time_stamp;

			/* we've already passed the last time displayed in the graph */
			if(a > t2)
				break;

			/* only graph this data if its on the graph */
			else if(b > t1) {

				/* clip last time if it exceeds graph limits */
				if(b > t2)
					b = t2;

				/* clip first time if it precedes graph limits */
				if(a < t1)
					a = t1;

				/* save this time if its the earliest we've graphed */
				if(a < subject->earliest_time) {
					subject->earliest_time = a;
					subject->earliest_state = last_as->entry_type;
					}

				/* save this time if its the latest we've graphed */
				if(b > subject->latest_time) {
					subject->latest_time = b;
					subject->latest_state = last_as->entry_type;
					}

				/* compute availability times for this chunk */
				compute_subject_availability_times(last_as->entry_type, temp_as->entry_type, last_as->time_stamp, a, b, subject, temp_as);

				/* return if we've reached the end of the graph limits */
				if(b >= t2) {
					last_as = temp_as;
					break;
					}
				}
			}


		/* keep track of the last item */
		last_as = temp_as;
		}


#ifdef DEBUG
	printf("--- END SECTION ---<BR>\n");
#endif

	/**********************************/
	/*           END SECTION          */
	/**********************************/

	if(last_as != NULL) {

		/* don't process an entry that is beyond the limits of the graph */
		if(last_as->time_stamp < t2) {

			time(&current_time);
			b = current_time;
			if(b > t2)
				b = t2;

			a = last_as->time_stamp;
			if(a < t1)
				a = t1;

			/* fake the current state (it doesn't really matter for graphing) */
			if(subject->type == HOST_SUBJECT)
				current_state = AS_HOST_UP;
			else
				current_state = AS_SVC_OK;

			/* compute availability times for last state */
			compute_subject_availability_times(last_as->entry_type, current_state, last_as->time_stamp, a, b, subject, last_as);
			}
		}


	return;
	}


/* computes availability times */
void compute_subject_availability_times(int first_state, int last_state, time_t real_start_time, time_t start_time, time_t end_time, avail_subject *subject, archived_state *as) {
	int start_state;
	unsigned long state_duration;
	struct tm *t;
	time_t midnight_today;
	int weekday;
	timerange *temp_timerange;
	unsigned long temp_duration;
	unsigned long temp_end;
	unsigned long temp_start;
	unsigned long start;
	unsigned long end;

#ifdef DEBUG
	if(subject->type == HOST_SUBJECT)
		printf("HOST '%s'...\n", subject->host_name);
	else
		printf("SERVICE '%s' ON HOST '%s'...\n", subject->service_description, subject->host_name);

	printf("COMPUTING %d->%d FROM %lu to %lu (%lu seconds) FOR %s<br>\n", first_state, last_state, start_time, end_time, (end_time - start_time), (subject->type == HOST_SUBJECT) ? "HOST" : "SERVICE");
#endif

	/* clip times if necessary */
	if(start_time < t1)
		start_time = t1;
	if(end_time > t2)
		end_time = t2;

	/* make sure this is a valid time */
	if(start_time > t2)
		return;
	if(end_time < t1)
		return;

	/* MickeM - attempt to handle the current time_period (if any) */
	if(current_timeperiod != NULL) {
		t = localtime((time_t *)&start_time);
		state_duration = 0;

		/* calculate the start of the day (midnight, 00:00 hours) */
		t->tm_sec = 0;
		t->tm_min = 0;
		t->tm_hour = 0;
		t->tm_isdst = -1;
		midnight_today = (unsigned long)mktime(t);
		weekday = t->tm_wday;

		while(midnight_today < end_time) {
			temp_duration = 0;
			temp_end = min(86400, end_time - midnight_today);
			temp_start = 0;
			if(start_time > midnight_today)
				temp_start = start_time - midnight_today;
#ifdef DEBUG
			printf("<b>Matching: %ld -> %ld. (%ld -> %ld)</b><br>\n", temp_start, temp_end, midnight_today + temp_start, midnight_today + temp_end);
#endif
			/* check all time ranges for this day of the week */
			for(temp_timerange = current_timeperiod->days[weekday]; temp_timerange != NULL; temp_timerange = temp_timerange->next) {

#ifdef DEBUG
				printf("<li>Matching in timerange[%d]: %d -> %d (%ld -> %ld)<br>\n", weekday, temp_timerange->range_start, temp_timerange->range_end, temp_start, temp_end);
#endif
				start = max(temp_timerange->range_start, temp_start);
				end = min(temp_timerange->range_end, temp_end);

				if(start < end) {
					temp_duration += end - start;
#ifdef DEBUG
					printf("<li>Matched time: %ld -> %ld = %d<br>\n", start, end, temp_duration);
#endif
					}
#ifdef DEBUG
				else
					printf("<li>Ignored time: %ld -> %ld<br>\n", start, end);
#endif
				}
			state_duration += temp_duration;
			temp_start = 0;
			midnight_today += 86400;
			if(++weekday > 6)
				weekday = 0;
			}
		}

	/* no report timeperiod was selected (assume 24x7) */
	else {
		/* calculate time in this state */
		state_duration = (unsigned long)(end_time - start_time);
		}

	/* can't graph if we don't have data... */
	if(first_state == AS_NO_DATA || last_state == AS_NO_DATA) {
		subject->time_indeterminate_nodata += state_duration;
		return;
		}
	if(first_state == AS_PROGRAM_START && (last_state == AS_PROGRAM_END || last_state == AS_PROGRAM_START)) {
		if(assume_initial_states == FALSE) {
			subject->time_indeterminate_nodata += state_duration;
			return;
			}
		}
	if(first_state == AS_PROGRAM_END) {

		/* added 7/24/03 */
		if(assume_states_during_notrunning == TRUE) {
			first_state = subject->last_known_state;
			}
		else {
			subject->time_indeterminate_notrunning += state_duration;
			return;
			}
		}

	/* special case if first entry was program start */
	if(first_state == AS_PROGRAM_START) {

		if(assume_initial_states == TRUE) {

			if(assume_state_retention == TRUE)
				start_state = subject->last_known_state;

			else {
				if(subject->type == HOST_SUBJECT)
					start_state = AS_HOST_UP;
				else
					start_state = AS_SVC_OK;
				}
			}
		else
			return;
		}
	else {
		start_state = first_state;
		subject->last_known_state = first_state;
		}

	/* save "processed state" info */
	as->processed_state = start_state;

#ifdef DEBUG
	printf("PASSED TIME CHECKS, CLIPPED VALUES: START=%lu, END=%lu\n", start_time, end_time);
#endif


	/* add time in this state to running totals */
	switch(start_state) {
		case AS_HOST_UP:
			subject->time_up += state_duration;
			break;
		case AS_HOST_DOWN:
			subject->time_down += state_duration;
			break;
		case AS_HOST_UNREACHABLE:
			subject->time_unreachable += state_duration;
			break;
		case AS_SVC_OK:
			subject->time_ok += state_duration;
			break;
		case AS_SVC_WARNING:
			subject->time_warning += state_duration;
			break;
		case AS_SVC_UNKNOWN:
			subject->time_unknown += state_duration;
			break;
		case AS_SVC_CRITICAL:
			subject->time_critical += state_duration;
			break;
		default:
			break;
		}

	return;
	}


/* computes downtime data for a given subject */
void compute_subject_downtime(avail_subject *subject, time_t current_time) {
	archived_state *temp_sd;
	time_t start_time;
	time_t end_time;
	int host_downtime_state = 0;
	int service_downtime_state = 0;
	int process_chunk = FALSE;

#ifdef DEBUG2
	printf("COMPUTE_SUBJECT_DOWNTIME\n");
#endif

	/* if left hand of graph is after current time, we can't do anything at all.... */
	if(t1 > current_time)
		return;

	/* no scheduled downtime data for subject... */
	if(subject->sd_list == NULL)
		return;

	/* all data we have occurs after last time on graph... */
	if(subject->sd_list->time_stamp >= t2)
		return;

	/* initialize pointer */
	temp_sd = subject->sd_list;

	/* special case if first entry is the end of scheduled downtime */
	if((temp_sd->entry_type == AS_HOST_DOWNTIME_END || temp_sd->entry_type == AS_SVC_DOWNTIME_END) && temp_sd->time_stamp > t1) {

#ifdef DEBUG2
		printf("\tSPECIAL DOWNTIME CASE\n");
#endif
		start_time = t1;
		end_time = (temp_sd->time_stamp > t2) ? t2 : temp_sd->time_stamp;
		compute_subject_downtime_times(start_time, end_time, subject, NULL);
		temp_sd = temp_sd->next;
		}

	/* process all periods of scheduled downtime */
	for(; temp_sd != NULL; temp_sd = temp_sd->next) {

		/* we've passed graph bounds... */
		if(temp_sd->time_stamp >= t2)
			break;

		if(temp_sd->entry_type == AS_HOST_DOWNTIME_START)
			host_downtime_state = 1;
		else if(temp_sd->entry_type == AS_HOST_DOWNTIME_END)
			host_downtime_state = 0;
		else if(temp_sd->entry_type == AS_SVC_DOWNTIME_START)
			service_downtime_state = 1;
		else if(temp_sd->entry_type == AS_SVC_DOWNTIME_END)
			service_downtime_state = 0;
		else
			continue;

		process_chunk = FALSE;
		if(temp_sd->entry_type == AS_HOST_DOWNTIME_START || temp_sd->entry_type == AS_SVC_DOWNTIME_START)
			process_chunk = TRUE;
		else if(subject->type == SERVICE_SUBJECT && (host_downtime_state == 1 || service_downtime_state == 1))
			process_chunk = TRUE;

		/* process this specific "chunk" of scheduled downtime */
		if(process_chunk == TRUE) {

			start_time = temp_sd->time_stamp;
			end_time = (temp_sd->next == NULL) ? current_time : temp_sd->next->time_stamp;

			/* check time sanity */
			if(end_time <= t1)
				continue;
			if(start_time >= t2)
				continue;
			if(start_time >= end_time)
				continue;

			/* clip time values */
			if(start_time < t1)
				start_time = t1;
			if(end_time > t2)
				end_time = t2;

			compute_subject_downtime_times(start_time, end_time, subject, temp_sd);
			}
		}

	return;
	}



/* computes downtime times */
void compute_subject_downtime_times(time_t start_time, time_t end_time, avail_subject *subject, archived_state *sd) {
	archived_state *temp_as = NULL;
	time_t part_subject_state = 0L;
	int saved_status = 0;
	int saved_stamp = 0;
	int count = 0;
	archived_state *temp_before = NULL;
	archived_state *last = NULL;

#ifdef DEBUG2
	printf("<P><b>ENTERING COMPUTE_SUBJECT_DOWNTIME_TIMES: start=%lu, end=%lu, t1=%lu, t2=%lu </b></P>", start_time, end_time, t1, t2);
#endif

	/* times are weird, so bail out... */
	if(start_time > end_time)
		return;
	if(start_time < t1 || end_time > t2)
		return;

	/* find starting point in archived state list */
	if(sd == NULL) {
#ifdef DEBUG2
		printf("<P>TEMP_AS=SUBJECT->AS_LIST </P>");
#endif
		temp_as = subject->as_list;
		}
	else if(sd->misc_ptr == NULL) {
#ifdef DEBUG2
		printf("<P>TEMP_AS=SUBJECT->AS_LIST</P>");
#endif
		temp_as = subject->as_list;
		}
	else if(sd->misc_ptr->next == NULL) {
#ifdef DEBUG2
		printf("<P>TEMP_AS=SD->MISC_PTR</P>");
#endif
		temp_as = sd->misc_ptr;
		}
	else {
#ifdef DEBUG2
		printf("<P>TEMP_AS=SD->MISC_PTR->NEXT</P>");
#endif
		temp_as = sd->misc_ptr->next;
		}

	/* initialize values */
	if(temp_as == NULL)
		part_subject_state = AS_NO_DATA;
	else if(temp_as->processed_state == AS_PROGRAM_START || temp_as->processed_state == AS_PROGRAM_END || temp_as->processed_state == AS_NO_DATA) {
#ifdef DEBUG2
		printf("<P>ENTRY TYPE #1: %d</P>", temp_as->entry_type);
#endif
		part_subject_state = AS_NO_DATA;
		}
	else {
#ifdef DEBUG2
		printf("<P>ENTRY TYPE #2: %d</P>", temp_as->entry_type);
#endif
		part_subject_state = temp_as->processed_state;
		}

#ifdef DEBUG2
	printf("<P>TEMP_AS=%s</P>", (temp_as == NULL) ? "NULL" : "Not NULL");
	printf("<P>SD=%s</P>", (sd == NULL) ? "NULL" : "Not NULL");
#endif

	/* temp_as now points to first event to possibly "break" this chunk */
	for(; temp_as != NULL; temp_as = temp_as->next) {
		count++;
		last = temp_as;

		if(temp_before == NULL) {
			if(last->time_stamp > start_time) {
				if(last->time_stamp > end_time)
					compute_subject_downtime_part_times(start_time, end_time, part_subject_state, subject);
				else
					compute_subject_downtime_part_times(start_time, last->time_stamp, part_subject_state, subject);
				}
			temp_before = temp_as;
			saved_status = temp_as->entry_type;
			saved_stamp = temp_as->time_stamp;

			/* check if first time is before schedule downtime */
			if(saved_stamp < start_time)
				saved_stamp = start_time;

			continue;
			}

		/* if status changed, we have to calculate */
		if(saved_status != temp_as->entry_type) {

			/* is outside schedule time, use end schdule downtime */
			if(temp_as->time_stamp > end_time) {
				if(saved_stamp < start_time)
					compute_subject_downtime_part_times(start_time, end_time, saved_status, subject);
				else
					compute_subject_downtime_part_times(saved_stamp, end_time, saved_status, subject);
				}
			else {
				if(saved_stamp < start_time)
					compute_subject_downtime_part_times(start_time, temp_as->time_stamp, saved_status, subject);
				else
					compute_subject_downtime_part_times(saved_stamp, temp_as->time_stamp, saved_status, subject);
				}
			saved_status = temp_as->entry_type;
			saved_stamp = temp_as->time_stamp;
			}
		}

	/* just one entry inside the scheduled downtime */
	if(count == 0)
		compute_subject_downtime_part_times(start_time, end_time, part_subject_state, subject);
	else {
		/* is outside scheduled time, use end schdule downtime */
		if(last->time_stamp > end_time)
			compute_subject_downtime_part_times(saved_stamp, end_time, saved_status, subject);
		else
			compute_subject_downtime_part_times(saved_stamp, last->time_stamp, saved_status, subject);
		}

	return;
	}



/* computes downtime times */
void compute_subject_downtime_part_times(time_t start_time, time_t end_time, int subject_state, avail_subject *subject) {
	unsigned long state_duration;

#ifdef DEBUG2
	printf("ENTERING COMPUTE_SUBJECT_DOWNTIME_PART_TIMES\n");
#endif

	/* times are weird */
	if(start_time > end_time)
		return;

	state_duration = (unsigned long)(end_time - start_time);

	switch(subject_state) {
		case AS_HOST_UP:
			subject->scheduled_time_up += state_duration;
			break;
		case AS_HOST_DOWN:
			subject->scheduled_time_down += state_duration;
			break;
		case AS_HOST_UNREACHABLE:
			subject->scheduled_time_unreachable += state_duration;
			break;
		case AS_SVC_OK:
			subject->scheduled_time_ok += state_duration;
			break;
		case AS_SVC_WARNING:
			subject->scheduled_time_warning += state_duration;
			break;
		case AS_SVC_UNKNOWN:
			subject->scheduled_time_unknown += state_duration;
			break;
		case AS_SVC_CRITICAL:
			subject->scheduled_time_critical += state_duration;
			break;
		default:
			subject->scheduled_time_indeterminate += state_duration;
			break;
		}

#ifdef DEBUG2
	printf("\tSUBJECT DOWNTIME: Host '%s', Service '%s', State=%d, Duration=%lu, Start=%lu\n", subject->host_name, (subject->service_description == NULL) ? "NULL" : subject->service_description, subject_state, state_duration, start_time);
#endif

	return;
	}



/* convert current host state to archived state value */
int convert_host_state_to_archived_state(int current_status) {

	if(current_status == SD_HOST_UP)
		return AS_HOST_UP;
	if(current_status == SD_HOST_DOWN)
		return AS_HOST_DOWN;
	if(current_status == SD_HOST_UNREACHABLE)
		return AS_HOST_UNREACHABLE;

	return AS_NO_DATA;
	}


/* convert current service state to archived state value */
int convert_service_state_to_archived_state(int current_status) {

	if(current_status == SERVICE_OK)
		return AS_SVC_OK;
	if(current_status == SERVICE_UNKNOWN)
		return AS_SVC_UNKNOWN;
	if(current_status == SERVICE_WARNING)
		return AS_SVC_WARNING;
	if(current_status == SERVICE_CRITICAL)
		return AS_SVC_CRITICAL;

	return AS_NO_DATA;
	}



/* create list of subjects to collect availability data for */
void create_subject_list(void) {
	hostgroup *temp_hostgroup;
	hostsmember *temp_hgmember;
	servicegroup *temp_servicegroup;
	servicesmember *temp_sgmember;
	host *temp_host;
	service *temp_service;
	const char *last_host_name = "";

	/* we're displaying one or more hosts */
	if(display_type == DISPLAY_HOST_AVAIL && host_name && strcmp(host_name, "")) {

		/* we're only displaying a specific host (and summaries for all services associated with it) */
		if(show_all_hosts == FALSE) {
			add_subject(HOST_SUBJECT, host_name, NULL);
			for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {
				if(!strcmp(temp_service->host_name, host_name))
					add_subject(SERVICE_SUBJECT, host_name, temp_service->description);
				}
			}

		/* we're displaying all hosts */
		else {
			for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next)
				add_subject(HOST_SUBJECT, temp_host->name, NULL);
			}
		}

	/* we're displaying a specific service */
	else if(display_type == DISPLAY_SERVICE_AVAIL && svc_description && strcmp(svc_description, "")) {

		/* we're only displaying a specific service */
		if(show_all_services == FALSE)
			add_subject(SERVICE_SUBJECT, host_name, svc_description);

		/* we're displaying all services */
		else {
			for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next)
				add_subject(SERVICE_SUBJECT, temp_service->host_name, temp_service->description);
			}
		}

	/* we're displaying one or more hostgroups (the host members of the groups) */
	else if(display_type == DISPLAY_HOSTGROUP_AVAIL && hostgroup_name && strcmp(hostgroup_name, "")) {

		/* we're displaying all hostgroups */
		if(show_all_hostgroups == TRUE) {
			for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {
				for(temp_hgmember = temp_hostgroup->members; temp_hgmember != NULL; temp_hgmember = temp_hgmember->next)
					add_subject(HOST_SUBJECT, temp_hgmember->host_name, NULL);
				}
			}
		/* we're only displaying a specific hostgroup */
		else {
			temp_hostgroup = find_hostgroup(hostgroup_name);
			if(temp_hostgroup != NULL) {
				for(temp_hgmember = temp_hostgroup->members; temp_hgmember != NULL; temp_hgmember = temp_hgmember->next)
					add_subject(HOST_SUBJECT, temp_hgmember->host_name, NULL);
				}
			}
		}

	/* we're displaying one or more servicegroups (the host and service members of the groups) */
	else if(display_type == DISPLAY_SERVICEGROUP_AVAIL && servicegroup_name && strcmp(servicegroup_name, "")) {

		/* we're displaying all servicegroups */
		if(show_all_servicegroups == TRUE) {
			for(temp_servicegroup = servicegroup_list; temp_servicegroup != NULL; temp_servicegroup = temp_servicegroup->next) {
				for(temp_sgmember = temp_servicegroup->members; temp_sgmember != NULL; temp_sgmember = temp_sgmember->next) {
					add_subject(SERVICE_SUBJECT, temp_sgmember->host_name, temp_sgmember->service_description);
					if(strcmp(last_host_name, temp_sgmember->host_name))
						add_subject(HOST_SUBJECT, temp_sgmember->host_name, NULL);
					last_host_name = temp_sgmember->host_name;
					}
				}
			}
		/* we're only displaying a specific servicegroup */
		else {
			temp_servicegroup = find_servicegroup(servicegroup_name);
			if(temp_servicegroup != NULL) {
				for(temp_sgmember = temp_servicegroup->members; temp_sgmember != NULL; temp_sgmember = temp_sgmember->next) {
					add_subject(SERVICE_SUBJECT, temp_sgmember->host_name, temp_sgmember->service_description);
					if(strcmp(last_host_name, temp_sgmember->host_name))
						add_subject(HOST_SUBJECT, temp_sgmember->host_name, NULL);
					last_host_name = temp_sgmember->host_name;
					}
				}
			}
		}

	return;
	}



/* adds a subject */
void add_subject(int subject_type, char *hn, char *sd) {
	avail_subject *last_subject = NULL;
	avail_subject *temp_subject = NULL;
	avail_subject *new_subject = NULL;
	int is_authorized = FALSE;

	/* bail if we've already added the subject */
	if(find_subject(subject_type, hn, sd))
		return;

	/* see if the user is authorized to see data for this host or service */
	if(subject_type == HOST_SUBJECT)
		is_authorized = is_authorized_for_host(find_host(hn), &current_authdata);
	else
		is_authorized = is_authorized_for_service(find_service(hn, sd), &current_authdata);
	if(is_authorized == FALSE)
		return;

	/* allocate memory for the new entry */
	new_subject = (avail_subject *)malloc(sizeof(avail_subject));
	if(new_subject == NULL)
		return;

	/* allocate memory for the host name */
	if(hn != NULL) {
		new_subject->host_name = (char *)malloc(strlen(hn) + 1);
		if(new_subject->host_name != NULL)
			strcpy(new_subject->host_name, hn);
		}
	else
		new_subject->host_name = NULL;

	/* allocate memory for the service description */
	if(sd != NULL) {
		new_subject->service_description = (char *)malloc(strlen(sd) + 1);
		if(new_subject->service_description != NULL)
			strcpy(new_subject->service_description, sd);
		}
	else
		new_subject->service_description = NULL;

	new_subject->type = subject_type;
	new_subject->earliest_state = AS_NO_DATA;
	new_subject->latest_state = AS_NO_DATA;
	new_subject->time_up = 0L;
	new_subject->time_down = 0L;
	new_subject->time_unreachable = 0L;
	new_subject->time_ok = 0L;
	new_subject->time_warning = 0L;
	new_subject->time_unknown = 0L;
	new_subject->time_critical = 0L;
	new_subject->scheduled_time_up = 0L;
	new_subject->scheduled_time_down = 0L;
	new_subject->scheduled_time_unreachable = 0L;
	new_subject->scheduled_time_ok = 0L;
	new_subject->scheduled_time_warning = 0L;
	new_subject->scheduled_time_unknown = 0L;
	new_subject->scheduled_time_critical = 0L;
	new_subject->scheduled_time_indeterminate = 0L;
	new_subject->time_indeterminate_nodata = 0L;
	new_subject->time_indeterminate_notrunning = 0L;
	new_subject->as_list = NULL;
	new_subject->as_list_tail = NULL;
	new_subject->sd_list = NULL;
	new_subject->last_known_state = AS_NO_DATA;

	/* add the new entry to the list in memory, sorted by host name */
	last_subject = subject_list;
	for(temp_subject = subject_list; temp_subject != NULL; temp_subject = temp_subject->next) {
		if(strcmp(new_subject->host_name, temp_subject->host_name) < 0) {
			new_subject->next = temp_subject;
			if(temp_subject == subject_list)
				subject_list = new_subject;
			else
				last_subject->next = new_subject;
			break;
			}
		else
			last_subject = temp_subject;
		}
	if(subject_list == NULL) {
		new_subject->next = NULL;
		subject_list = new_subject;
		}
	else if(temp_subject == NULL) {
		new_subject->next = NULL;
		last_subject->next = new_subject;
		}

	return;
	}



/* finds a specific subject */
avail_subject *find_subject(int type, char *hn, char *sd) {
	avail_subject *temp_subject;

	if(hn == NULL)
		return NULL;

	if(type == SERVICE_SUBJECT && sd == NULL)
		return NULL;

	for(temp_subject = subject_list; temp_subject != NULL; temp_subject = temp_subject->next) {
		if(temp_subject->type != type)
			continue;
		if(strcmp(hn, temp_subject->host_name))
			continue;
		if(type == SERVICE_SUBJECT && strcmp(sd, temp_subject->service_description))
			continue;
		return temp_subject;
		}

	return NULL;
	}



/* adds an archived state entry to all subjects */
void add_global_archived_state(int entry_type, int state_type, time_t time_stamp, const char *state_info) {
	avail_subject *temp_subject;

	for(temp_subject = subject_list; temp_subject != NULL; temp_subject = temp_subject->next)
		add_archived_state(entry_type, state_type, time_stamp, state_info, temp_subject);

	return;
	}




/* adds an archived state entry to a specific subject */
void add_archived_state(int entry_type, int state_type, time_t time_stamp, const char *state_info, avail_subject *subject) {
	archived_state *last_as = NULL;
	archived_state *temp_as = NULL;
	archived_state *new_as = NULL;

	/* allocate memory for the new entry */
	new_as = (archived_state *)malloc(sizeof(archived_state));
	if(new_as == NULL)
		return;

	/* allocate memory for the state info */
	if(state_info != NULL) {
		new_as->state_info = (char *)malloc(strlen(state_info) + 1);
		if(new_as->state_info != NULL)
			strcpy(new_as->state_info, state_info);
		}
	else new_as->state_info = NULL;

	/* initialize the "processed state" value - this gets modified later for most entries */
	if(entry_type != AS_PROGRAM_START && entry_type != AS_PROGRAM_END && entry_type != AS_NO_DATA)
		new_as->processed_state = entry_type;
	else
		new_as->processed_state = AS_NO_DATA;

	new_as->entry_type = entry_type;
	new_as->state_type = state_type;
	new_as->time_stamp = time_stamp;
	new_as->misc_ptr = NULL;

	/* add the new entry to the list in memory, sorted by time (more recent entries should appear towards end of list) */
	last_as = subject->as_list;
	for(temp_as = subject->as_list; temp_as != NULL; temp_as = temp_as->next) {
		if(new_as->time_stamp < temp_as->time_stamp) {
			new_as->next = temp_as;
			if(temp_as == subject->as_list)
				subject->as_list = new_as;
			else
				last_as->next = new_as;
			break;
			}
		else
			last_as = temp_as;
		}
	if(subject->as_list == NULL) {
		new_as->next = NULL;
		subject->as_list = new_as;
		}
	else if(temp_as == NULL) {
		new_as->next = NULL;
		last_as->next = new_as;
		}

	/* update "tail" of the list - not really the tail, just last item added */
	subject->as_list_tail = new_as;

	return;
	}


/* adds a scheduled downtime entry to a specific subject */
void add_scheduled_downtime(int state_type, time_t time_stamp, avail_subject *subject) {
	archived_state *last_sd = NULL;
	archived_state *temp_sd = NULL;
	archived_state *new_sd = NULL;

	/* allocate memory for the new entry */
	new_sd = (archived_state *)malloc(sizeof(archived_state));
	if(new_sd == NULL)
		return;

	new_sd->state_info = NULL;
	new_sd->processed_state = state_type;
	new_sd->entry_type = state_type;
	new_sd->time_stamp = time_stamp;
	new_sd->misc_ptr = subject->as_list_tail;

	/* add the new entry to the list in memory, sorted by time (more recent entries should appear towards end of list) */
	last_sd = subject->sd_list;
	for(temp_sd = subject->sd_list; temp_sd != NULL; temp_sd = temp_sd->next) {
		if(new_sd->time_stamp <= temp_sd->time_stamp) {
			new_sd->next = temp_sd;
			if(temp_sd == subject->sd_list)
				subject->sd_list = new_sd;
			else
				last_sd->next = new_sd;
			break;
			}
		else
			last_sd = temp_sd;
		}
	if(subject->sd_list == NULL) {
		new_sd->next = NULL;
		subject->sd_list = new_sd;
		}
	else if(temp_sd == NULL) {
		new_sd->next = NULL;
		last_sd->next = new_sd;
		}

	return;
	}


/* frees memory allocated to all availability data */
void free_availability_data(void) {
	avail_subject *this_subject;
	avail_subject *next_subject;

	for(this_subject = subject_list; this_subject != NULL;) {
		next_subject = this_subject->next;
		if(this_subject->host_name != NULL)
			free(this_subject->host_name);
		if(this_subject->service_description != NULL)
			free(this_subject->service_description);
		free_archived_state_list(this_subject->as_list);
		free_archived_state_list(this_subject->sd_list);
		free(this_subject);
		this_subject = next_subject;
		}

	return;
	}

/* frees memory allocated to the archived state list */
void free_archived_state_list(archived_state *as_list) {
	archived_state *this_as = NULL;
	archived_state *next_as = NULL;

	for(this_as = as_list; this_as != NULL;) {
		next_as = this_as->next;
		if(this_as->state_info != NULL)
			free(this_as->state_info);
		free(this_as);
		this_as = next_as;
		}

	as_list = NULL;

	return;
	}



/* reads log files for archived state data */
void read_archived_state_data(void) {
	char filename[MAX_FILENAME_LENGTH];
	int oldest_archive = 0;
	int newest_archive = 0;
	int current_archive = 0;

	/* determine oldest archive to use when scanning for data (include backtracked archives as well) */
	oldest_archive = determine_archive_to_use_from_time(t1);
	if(log_rotation_method != LOG_ROTATION_NONE)
		oldest_archive += backtrack_archives;

	/* determine most recent archive to use when scanning for data */
	newest_archive = determine_archive_to_use_from_time(t2);

	if(oldest_archive < newest_archive)
		oldest_archive = newest_archive;

	/* read in all the necessary archived logs (from most recent to earliest) */
	for(current_archive = newest_archive; current_archive <= oldest_archive; current_archive++) {

#ifdef DEBUG
		printf("Reading archive #%d\n", current_archive);
#endif

		/* get the name of the log file that contains this archive */
		get_log_archive_to_use(current_archive, filename, sizeof(filename) - 1);

#ifdef DEBUG
		printf("Archive name: '%s'\n", filename);
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
	char *plugin_output = NULL;
	char *temp_buffer = NULL;
	time_t time_stamp;
	mmapfile *thefile = NULL;
	avail_subject *temp_subject = NULL;
	int state_type = 0;

	if((thefile = mmap_fopen(filename)) == NULL)
		return;

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
			add_global_archived_state(AS_PROGRAM_START, AS_NO_DATA, time_stamp, "Program start");
		if(strstr(input, " restarting..."))
			add_global_archived_state(AS_PROGRAM_START, AS_NO_DATA, time_stamp, "Program restart");

		/* program stops */
		if(strstr(input, " shutting down..."))
			add_global_archived_state(AS_PROGRAM_END, AS_NO_DATA, time_stamp, "Normal program termination");
		if(strstr(input, "Bailing out"))
			add_global_archived_state(AS_PROGRAM_END, AS_NO_DATA, time_stamp, "Abnormal program termination");

		if(display_type == DISPLAY_HOST_AVAIL || display_type == DISPLAY_HOSTGROUP_AVAIL || display_type == DISPLAY_SERVICEGROUP_AVAIL) {

			/* normal host alerts and initial/current states */
			if(strstr(input, "HOST ALERT:") || strstr(input, "INITIAL HOST STATE:") || strstr(input, "CURRENT HOST STATE:")) {

				/* get host name */
				temp_buffer = my_strtok(NULL, ":");
				temp_buffer = my_strtok(NULL, ";");
				strncpy(entry_host_name, (temp_buffer == NULL) ? "" : temp_buffer + 1, sizeof(entry_host_name));
				entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

				/* see if there is a corresponding subject for this host */
				temp_subject = find_subject(HOST_SUBJECT, entry_host_name, NULL);
				if(temp_subject == NULL)
					continue;

				/* state types */
				if(strstr(input, ";SOFT;")) {
					if(include_soft_states == FALSE)
						continue;
					state_type = AS_SOFT_STATE;
					}
				if(strstr(input, ";HARD;"))
					state_type = AS_HARD_STATE;

				/* get the plugin output */
				temp_buffer = my_strtok(NULL, ";");
				temp_buffer = my_strtok(NULL, ";");
				temp_buffer = my_strtok(NULL, ";");
				plugin_output = my_strtok(NULL, "\n");

				if(strstr(input, ";DOWN;"))
					add_archived_state(AS_HOST_DOWN, state_type, time_stamp, plugin_output, temp_subject);
				else if(strstr(input, ";UNREACHABLE;"))
					add_archived_state(AS_HOST_UNREACHABLE, state_type, time_stamp, plugin_output, temp_subject);
				else if(strstr(input, ";RECOVERY") || strstr(input, ";UP;"))
					add_archived_state(AS_HOST_UP, state_type, time_stamp, plugin_output, temp_subject);
				else
					add_archived_state(AS_NO_DATA, AS_NO_DATA, time_stamp, plugin_output, temp_subject);
				}

			/* scheduled downtime notices */
			else if(strstr(input, "HOST DOWNTIME ALERT:")) {

				/* get host name */
				temp_buffer = my_strtok(NULL, ":");
				temp_buffer = my_strtok(NULL, ";");
				strncpy(entry_host_name, (temp_buffer == NULL) ? "" : temp_buffer + 1, sizeof(entry_host_name));
				entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

				/* see if there is a corresponding subject for this host */
				temp_subject = find_subject(HOST_SUBJECT, entry_host_name, NULL);
				if(temp_subject == NULL)
					continue;

				if(show_scheduled_downtime == FALSE)
					continue;

				if(strstr(input, ";STARTED;"))
					add_scheduled_downtime(AS_HOST_DOWNTIME_START, time_stamp, temp_subject);
				else
					add_scheduled_downtime(AS_HOST_DOWNTIME_END, time_stamp, temp_subject);

				}
			}

		if(display_type == DISPLAY_SERVICE_AVAIL || display_type == DISPLAY_HOST_AVAIL || display_type == DISPLAY_SERVICEGROUP_AVAIL) {

			/* normal service alerts and initial/current states */
			if(strstr(input, "SERVICE ALERT:") || strstr(input, "INITIAL SERVICE STATE:") || strstr(input, "CURRENT SERVICE STATE:")) {

				/* get host name */
				temp_buffer = my_strtok(NULL, ":");
				temp_buffer = my_strtok(NULL, ";");
				strncpy(entry_host_name, (temp_buffer == NULL) ? "" : temp_buffer + 1, sizeof(entry_host_name));
				entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

				/* get service description */
				temp_buffer = my_strtok(NULL, ";");
				strncpy(entry_svc_description, (temp_buffer == NULL) ? "" : temp_buffer, sizeof(entry_svc_description));
				entry_svc_description[sizeof(entry_svc_description) - 1] = '\x0';

				/* see if there is a corresponding subject for this service */
				temp_subject = find_subject(SERVICE_SUBJECT, entry_host_name, entry_svc_description);
				if(temp_subject == NULL)
					continue;

				/* state types */
				if(strstr(input, ";SOFT;")) {
					if(include_soft_states == FALSE)
						continue;
					state_type = AS_SOFT_STATE;
					}
				if(strstr(input, ";HARD;"))
					state_type = AS_HARD_STATE;

				/* get the plugin output */
				temp_buffer = my_strtok(NULL, ";");
				temp_buffer = my_strtok(NULL, ";");
				temp_buffer = my_strtok(NULL, ";");
				plugin_output = my_strtok(NULL, "\n");

				if(strstr(input, ";CRITICAL;"))
					add_archived_state(AS_SVC_CRITICAL, state_type, time_stamp, plugin_output, temp_subject);
				else if(strstr(input, ";WARNING;"))
					add_archived_state(AS_SVC_WARNING, state_type, time_stamp, plugin_output, temp_subject);
				else if(strstr(input, ";UNKNOWN;"))
					add_archived_state(AS_SVC_UNKNOWN, state_type, time_stamp, plugin_output, temp_subject);
				else if(strstr(input, ";RECOVERY;") || strstr(input, ";OK;"))
					add_archived_state(AS_SVC_OK, state_type, time_stamp, plugin_output, temp_subject);
				else
					add_archived_state(AS_NO_DATA, AS_NO_DATA, time_stamp, plugin_output, temp_subject);

				}

			/* scheduled service downtime notices */
			else if(strstr(input, "SERVICE DOWNTIME ALERT:")) {

				/* get host name */
				temp_buffer = my_strtok(NULL, ":");
				temp_buffer = my_strtok(NULL, ";");
				strncpy(entry_host_name, (temp_buffer == NULL) ? "" : temp_buffer + 1, sizeof(entry_host_name));
				entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

				/* get service description */
				temp_buffer = my_strtok(NULL, ";");
				strncpy(entry_svc_description, (temp_buffer == NULL) ? "" : temp_buffer, sizeof(entry_svc_description));
				entry_svc_description[sizeof(entry_svc_description) - 1] = '\x0';

				/* see if there is a corresponding subject for this service */
				temp_subject = find_subject(SERVICE_SUBJECT, entry_host_name, entry_svc_description);
				if(temp_subject == NULL)
					continue;

				if(show_scheduled_downtime == FALSE)
					continue;

				if(strstr(input, ";STARTED;"))
					add_scheduled_downtime(AS_SVC_DOWNTIME_START, time_stamp, temp_subject);
				else
					add_scheduled_downtime(AS_SVC_DOWNTIME_END, time_stamp, temp_subject);
				}

			/* scheduled host downtime notices */
			else if(strstr(input, "HOST DOWNTIME ALERT:")) {

				/* get host name */
				temp_buffer = my_strtok(NULL, ":");
				temp_buffer = my_strtok(NULL, ";");
				strncpy(entry_host_name, (temp_buffer == NULL) ? "" : temp_buffer + 1, sizeof(entry_host_name));
				entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

				/* this host downtime entry must be added to all service subjects associated with the host! */
				for(temp_subject = subject_list; temp_subject != NULL; temp_subject = temp_subject->next) {

					if(temp_subject->type != SERVICE_SUBJECT)
						continue;

					if(strcmp(temp_subject->host_name, entry_host_name))
						continue;

					if(show_scheduled_downtime == FALSE)
						continue;

					if(strstr(input, ";STARTED;"))
						add_scheduled_downtime(AS_HOST_DOWNTIME_START, time_stamp, temp_subject);
					else
						add_scheduled_downtime(AS_HOST_DOWNTIME_END, time_stamp, temp_subject);
					}
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
			/* not implemented */
			break;
		case TIMEPERIOD_LASTQUARTER:
			/* not implemented */
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


/* writes log entries to screen */
void write_log_entries(avail_subject *subject) {
	archived_state *temp_as;
	archived_state *temp_sd;
	time_t current_time;
	char start_date_time[MAX_DATETIME_LENGTH];
	char end_date_time[MAX_DATETIME_LENGTH];
	char duration[20];
	const char *bgclass = "";
	const char *ebgclass = "";
	const char *entry_type = "";
	const char *state_type = "";
	int days;
	int hours;
	int minutes;
	int seconds;
	int odd = 0;


	if(output_format != HTML_OUTPUT)
		return;

	if(show_log_entries == FALSE)
		return;

	if(subject == NULL)
		return;

	time(&current_time);

	/* inject all scheduled downtime entries into the main list for display purposes */
	for(temp_sd = subject->sd_list; temp_sd != NULL; temp_sd = temp_sd->next) {
		switch(temp_sd->entry_type) {
			case AS_SVC_DOWNTIME_START:
			case AS_HOST_DOWNTIME_START:
				entry_type = "Start of scheduled downtime";
				break;
			case AS_SVC_DOWNTIME_END:
			case AS_HOST_DOWNTIME_END:
				entry_type = "End of scheduled downtime";
				break;
			default:
				entry_type = "?";
				break;
			}
		add_archived_state(temp_sd->entry_type, AS_NO_DATA, temp_sd->time_stamp, entry_type, subject);
		}


	printf("<BR><BR>\n");

	printf("<DIV ALIGN=CENTER CLASS='dataTitle'>%s Log Entries:</DIV>\n", (subject->type == HOST_SUBJECT) ? "Host" : "Service");

	printf("<DIV ALIGN=CENTER CLASS='infoMessage'>");
	if(full_log_entries == TRUE) {
		full_log_entries = FALSE;
		if(subject->type == HOST_SUBJECT)
			host_report_url(subject->host_name, "[ View condensed log entries ]");
		else
			service_report_url(subject->host_name, subject->service_description, "[ View condensed log entries ]");
		full_log_entries = TRUE;
		}
	else {
		full_log_entries = TRUE;
		if(subject->type == HOST_SUBJECT)
			host_report_url(subject->host_name, "[ View full log entries ]");
		else
			service_report_url(subject->host_name, subject->service_description, "[ View full log entries ]");
		full_log_entries = FALSE;
		}
	printf("</DIV>\n");

	printf("<DIV ALIGN=CENTER>\n");

	printf("<table border=1 cellspacing=0 cellpadding=3 class='logEntries'>\n");
	printf("<tr><th class='logEntries'>Event Start Time</th><th class='logEntries'>Event End Time</th><th class='logEntries'>Event Duration</th><th class='logEntries'>Event/State Type</th><th class='logEntries'>Event/State Information</th></tr>\n");

	/* write all archived state entries */
	for(temp_as = subject->as_list; temp_as != NULL; temp_as = temp_as->next) {

		if(temp_as->state_type == AS_HARD_STATE)
			state_type = " (HARD)";
		else if(temp_as->state_type == AS_SOFT_STATE)
			state_type = " (SOFT)";
		else
			state_type = "";

		switch(temp_as->entry_type) {
			case AS_NO_DATA:
				if(full_log_entries == FALSE)
					continue;
				entry_type = "NO DATA";
				ebgclass = "INDETERMINATE";
				break;
			case AS_PROGRAM_END:
				if(full_log_entries == FALSE)
					continue;
				entry_type = "PROGRAM END";
				ebgclass = "INDETERMINATE";
				break;
			case AS_PROGRAM_START:
				if(full_log_entries == FALSE)
					continue;
				entry_type = "PROGRAM (RE)START";
				ebgclass = "INDETERMINATE";
				break;
			case AS_HOST_UP:
				entry_type = "HOST UP";
				ebgclass = "UP";
				break;
			case AS_HOST_DOWN:
				entry_type = "HOST DOWN";
				ebgclass = "DOWN";
				break;
			case AS_HOST_UNREACHABLE:
				entry_type = "HOST UNREACHABLE";
				ebgclass = "UNREACHABLE";
				break;
			case AS_SVC_OK:
				entry_type = "SERVICE OK";
				ebgclass = "OK";
				break;
			case AS_SVC_UNKNOWN:
				entry_type = "SERVICE UNKNOWN";
				ebgclass = "UNKNOWN";
				break;
			case AS_SVC_WARNING:
				entry_type = "SERVICE WARNING";
				ebgclass = "WARNING";
				break;
			case AS_SVC_CRITICAL:
				entry_type = "SERVICE CRITICAL";
				ebgclass = "CRITICAL";
				break;
			case AS_SVC_DOWNTIME_START:
				entry_type = "SERVICE DOWNTIME START";
				ebgclass = "INDETERMINATE";
				break;
			case AS_SVC_DOWNTIME_END:
				entry_type = "SERVICE DOWNTIME END";
				ebgclass = "INDETERMINATE";
				break;
			case AS_HOST_DOWNTIME_START:
				entry_type = "HOST DOWNTIME START";
				ebgclass = "INDETERMINATE";
				break;
			case AS_HOST_DOWNTIME_END:
				entry_type = "HOST DOWNTIME END";
				ebgclass = "INDETERMINATE";
				break;
			default:
				if(full_log_entries == FALSE)
					continue;
				entry_type = "?";
				ebgclass = "INDETERMINATE";
			}

		get_time_string(&(temp_as->time_stamp), start_date_time, sizeof(start_date_time) - 1, SHORT_DATE_TIME);
		if(temp_as->next == NULL) {
			get_time_string(&t2, end_date_time, sizeof(end_date_time) - 1, SHORT_DATE_TIME);
			get_time_breakdown((time_t)(t2 - temp_as->time_stamp), &days, &hours, &minutes, &seconds);
			snprintf(duration, sizeof(duration) - 1, "%dd %dh %dm %ds+", days, hours, minutes, seconds);
			}
		else {
			get_time_string(&(temp_as->next->time_stamp), end_date_time, sizeof(end_date_time) - 1, SHORT_DATE_TIME);
			get_time_breakdown((time_t)(temp_as->next->time_stamp - temp_as->time_stamp), &days, &hours, &minutes, &seconds);
			snprintf(duration, sizeof(duration) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
			}

		if(odd) {
			bgclass = "Odd";
			odd = 0;
			}
		else {
			bgclass = "Even";
			odd = 1;
			}

		printf("<tr class='logEntries%s'>", bgclass);
		printf("<td class='logEntries%s'>%s</td>", bgclass, start_date_time);
		printf("<td class='logEntries%s'>%s</td>", bgclass, end_date_time);
		printf("<td class='logEntries%s'>%s</td>", bgclass, duration);
		printf("<td class='logEntries%s'>%s%s</td>", ebgclass, entry_type, state_type);
		printf("<td class='logEntries%s'>%s</td>", bgclass, (temp_as->state_info == NULL) ? "" : html_encode(temp_as->state_info, FALSE));
		printf("</tr>\n");
		}

	printf("</table>\n");

	printf("</DIV>\n");

	return;
	}



/* display hostgroup availability */
void display_hostgroup_availability(void) {
	hostgroup *temp_hostgroup;

	/* display data for a specific hostgroup */
	if(show_all_hostgroups == FALSE) {
		temp_hostgroup = find_hostgroup(hostgroup_name);
		display_specific_hostgroup_availability(temp_hostgroup);
		}

	/* display data for all hostgroups */
	else {
		for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next)
			display_specific_hostgroup_availability(temp_hostgroup);
		}

	return;
	}



/* display availability for a specific hostgroup */
void display_specific_hostgroup_availability(hostgroup *hg) {
	unsigned long total_time;
	unsigned long time_determinate;
	unsigned long time_indeterminate;
	avail_subject *temp_subject;
	double percent_time_up = 0.0;
	double percent_time_down = 0.0;
	double percent_time_unreachable = 0.0;
	double percent_time_up_known = 0.0;
	double percent_time_down_known = 0.0;
	double percent_time_unreachable_known = 0.0;
	double percent_time_indeterminate = 0.0;

	double average_percent_time_up = 0.0;
	double average_percent_time_up_known = 0.0;
	double average_percent_time_down = 0.0;
	double average_percent_time_down_known = 0.0;
	double average_percent_time_unreachable = 0.0;
	double average_percent_time_unreachable_known = 0.0;
	double average_percent_time_indeterminate = 0.0;

	int current_subject = 0;

	const char *bgclass = "";
	int odd = 1;
	host *temp_host;

	if(hg == NULL)
		return;

	/* the user isn't authorized to view this hostgroup */
	if(is_authorized_for_hostgroup(hg, &current_authdata) == FALSE)
		return;

	/* calculate total time during period based on timeperiod used for reporting */
	total_time = calculate_total_time(t1, t2);

	printf("<BR><BR>\n");
	printf("<DIV ALIGN=CENTER CLASS='dataTitle'>Hostgroup '%s' Host State Breakdowns:</DIV>\n", hg->group_name);

	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CLASS='data'>\n");
	printf("<TR><TH CLASS='data'>Host</TH><TH CLASS='data'>%% Time Up</TH><TH CLASS='data'>%% Time Down</TH><TH CLASS='data'>%% Time Unreachable</TH><TH CLASS='data'>%% Time Undetermined</TH></TR>\n");

	for(temp_subject = subject_list; temp_subject != NULL; temp_subject = temp_subject->next) {

		if(temp_subject->type != HOST_SUBJECT)
			continue;

		temp_host = find_host(temp_subject->host_name);
		if(temp_host == NULL)
			continue;

		if(is_host_member_of_hostgroup(hg, temp_host) == FALSE)
			continue;

		current_subject++;

		/* reset variables */
		percent_time_up = 0.0;
		percent_time_down = 0.0;
		percent_time_unreachable = 0.0;
		percent_time_indeterminate = 0.0;
		percent_time_up_known = 0.0;
		percent_time_down_known = 0.0;
		percent_time_unreachable_known = 0.0;

		time_determinate = temp_subject->time_up + temp_subject->time_down + temp_subject->time_unreachable;
		time_indeterminate = total_time - time_determinate;

		if(total_time > 0) {
			percent_time_up = (double)(((double)temp_subject->time_up * 100.0) / (double)total_time);
			percent_time_down = (double)(((double)temp_subject->time_down * 100.0) / (double)total_time);
			percent_time_unreachable = (double)(((double)temp_subject->time_unreachable * 100.0) / (double)total_time);
			percent_time_indeterminate = (double)(((double)time_indeterminate * 100.0) / (double)total_time);
			if(time_determinate > 0) {
				percent_time_up_known = (double)(((double)temp_subject->time_up * 100.0) / (double)time_determinate);
				percent_time_down_known = (double)(((double)temp_subject->time_down * 100.0) / (double)time_determinate);
				percent_time_unreachable_known = (double)(((double)temp_subject->time_unreachable * 100.0) / (double)time_determinate);
				}
			}

		if(odd) {
			odd = 0;
			bgclass = "Odd";
			}
		else {
			odd = 1;
			bgclass = "Even";
			}

		printf("<tr CLASS='data%s'><td CLASS='data%s'>", bgclass, bgclass);
		host_report_url(temp_subject->host_name, temp_subject->host_name);
		printf("</td><td CLASS='hostUP'>%2.3f%% (%2.3f%%)</td><td CLASS='hostDOWN'>%2.3f%% (%2.3f%%)</td><td CLASS='hostUNREACHABLE'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>\n", percent_time_up, percent_time_up_known, percent_time_down, percent_time_down_known, percent_time_unreachable, percent_time_unreachable_known, bgclass, percent_time_indeterminate);

		get_running_average(&average_percent_time_up, percent_time_up, current_subject);
		get_running_average(&average_percent_time_up_known, percent_time_up_known, current_subject);
		get_running_average(&average_percent_time_down, percent_time_down, current_subject);
		get_running_average(&average_percent_time_down_known, percent_time_down_known, current_subject);
		get_running_average(&average_percent_time_unreachable, percent_time_unreachable, current_subject);
		get_running_average(&average_percent_time_unreachable_known, percent_time_unreachable_known, current_subject);
		get_running_average(&average_percent_time_indeterminate, percent_time_indeterminate, current_subject);
		}

	/* average statistics */
	if(odd) {
		odd = 0;
		bgclass = "Odd";
		}
	else {
		odd = 1;
		bgclass = "Even";
		}
	printf("<tr CLASS='data%s'><td CLASS='data%s'>Average</td><td CLASS='hostUP'>%2.3f%% (%2.3f%%)</td><td CLASS='hostDOWN'>%2.3f%% (%2.3f%%)</td><td CLASS='hostUNREACHABLE'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>", bgclass, bgclass, average_percent_time_up, average_percent_time_up_known, average_percent_time_down, average_percent_time_down_known, average_percent_time_unreachable, average_percent_time_unreachable_known, bgclass, average_percent_time_indeterminate);

	printf("</table>\n");
	printf("</DIV>\n");

	return;
	}


/* display servicegroup availability */
void display_servicegroup_availability(void) {
	servicegroup *temp_servicegroup;

	/* display data for a specific servicegroup */
	if(show_all_servicegroups == FALSE) {
		temp_servicegroup = find_servicegroup(servicegroup_name);
		display_specific_servicegroup_availability(temp_servicegroup);
		}

	/* display data for all servicegroups */
	else {
		for(temp_servicegroup = servicegroup_list; temp_servicegroup != NULL; temp_servicegroup = temp_servicegroup->next)
			display_specific_servicegroup_availability(temp_servicegroup);
		}

	return;
	}



/* display availability for a specific servicegroup */
void display_specific_servicegroup_availability(servicegroup *sg) {
	unsigned long total_time;
	unsigned long time_determinate;
	unsigned long time_indeterminate;
	avail_subject *temp_subject;
	double percent_time_up = 0.0;
	double percent_time_down = 0.0;
	double percent_time_unreachable = 0.0;
	double percent_time_up_known = 0.0;
	double percent_time_down_known = 0.0;
	double percent_time_unreachable_known = 0.0;
	double percent_time_indeterminate = 0.0;
	double percent_time_ok = 0.0;
	double percent_time_warning = 0.0;
	double percent_time_unknown = 0.0;
	double percent_time_critical = 0.0;
	double percent_time_ok_known = 0.0;
	double percent_time_warning_known = 0.0;
	double percent_time_unknown_known = 0.0;
	double percent_time_critical_known = 0.0;

	double average_percent_time_up = 0.0;
	double average_percent_time_up_known = 0.0;
	double average_percent_time_down = 0.0;
	double average_percent_time_down_known = 0.0;
	double average_percent_time_unreachable = 0.0;
	double average_percent_time_unreachable_known = 0.0;
	double average_percent_time_ok = 0.0;
	double average_percent_time_ok_known = 0.0;
	double average_percent_time_unknown = 0.0;
	double average_percent_time_unknown_known = 0.0;
	double average_percent_time_warning = 0.0;
	double average_percent_time_warning_known = 0.0;
	double average_percent_time_critical = 0.0;
	double average_percent_time_critical_known = 0.0;
	double average_percent_time_indeterminate = 0.0;

	int current_subject = 0;

	const char *bgclass = "";
	int odd = 1;
	host *temp_host;
	service *temp_service;
	char last_host[MAX_INPUT_BUFFER];

	if(sg == NULL)
		return;

	/* the user isn't authorized to view this servicegroup */
	if(is_authorized_for_servicegroup(sg, &current_authdata) == FALSE)
		return;

	/* calculate total time during period based on timeperiod used for reporting */
	total_time = calculate_total_time(t1, t2);

	printf("<BR><BR>\n");
	printf("<DIV ALIGN=CENTER CLASS='dataTitle'>Servicegroup '%s' Host State Breakdowns:</DIV>\n", sg->group_name);

	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CLASS='data'>\n");
	printf("<TR><TH CLASS='data'>Host</TH><TH CLASS='data'>%% Time Up</TH><TH CLASS='data'>%% Time Down</TH><TH CLASS='data'>%% Time Unreachable</TH><TH CLASS='data'>%% Time Undetermined</TH></TR>\n");

	for(temp_subject = subject_list; temp_subject != NULL; temp_subject = temp_subject->next) {

		if(temp_subject->type != HOST_SUBJECT)
			continue;

		temp_host = find_host(temp_subject->host_name);
		if(temp_host == NULL)
			continue;

		if(is_host_member_of_servicegroup(sg, temp_host) == FALSE)
			continue;

		current_subject++;

		/* reset variables */
		percent_time_up = 0.0;
		percent_time_down = 0.0;
		percent_time_unreachable = 0.0;
		percent_time_indeterminate = 0.0;
		percent_time_up_known = 0.0;
		percent_time_down_known = 0.0;
		percent_time_unreachable_known = 0.0;

		time_determinate = temp_subject->time_up + temp_subject->time_down + temp_subject->time_unreachable;
		time_indeterminate = total_time - time_determinate;

		if(total_time > 0) {
			percent_time_up = (double)(((double)temp_subject->time_up * 100.0) / (double)total_time);
			percent_time_down = (double)(((double)temp_subject->time_down * 100.0) / (double)total_time);
			percent_time_unreachable = (double)(((double)temp_subject->time_unreachable * 100.0) / (double)total_time);
			percent_time_indeterminate = (double)(((double)time_indeterminate * 100.0) / (double)total_time);
			if(time_determinate > 0) {
				percent_time_up_known = (double)(((double)temp_subject->time_up * 100.0) / (double)time_determinate);
				percent_time_down_known = (double)(((double)temp_subject->time_down * 100.0) / (double)time_determinate);
				percent_time_unreachable_known = (double)(((double)temp_subject->time_unreachable * 100.0) / (double)time_determinate);
				}
			}

		if(odd) {
			odd = 0;
			bgclass = "Odd";
			}
		else {
			odd = 1;
			bgclass = "Even";
			}

		printf("<tr CLASS='data%s'><td CLASS='data%s'>", bgclass, bgclass);
		host_report_url(temp_subject->host_name, temp_subject->host_name);
		printf("</td><td CLASS='hostUP'>%2.3f%% (%2.3f%%)</td><td CLASS='hostDOWN'>%2.3f%% (%2.3f%%)</td><td CLASS='hostUNREACHABLE'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>\n", percent_time_up, percent_time_up_known, percent_time_down, percent_time_down_known, percent_time_unreachable, percent_time_unreachable_known, bgclass, percent_time_indeterminate);

		get_running_average(&average_percent_time_up, percent_time_up, current_subject);
		get_running_average(&average_percent_time_up_known, percent_time_up_known, current_subject);
		get_running_average(&average_percent_time_down, percent_time_down, current_subject);
		get_running_average(&average_percent_time_down_known, percent_time_down_known, current_subject);
		get_running_average(&average_percent_time_unreachable, percent_time_unreachable, current_subject);
		get_running_average(&average_percent_time_unreachable_known, percent_time_unreachable_known, current_subject);
		get_running_average(&average_percent_time_indeterminate, percent_time_indeterminate, current_subject);
		}

	/* average statistics */
	if(odd) {
		odd = 0;
		bgclass = "Odd";
		}
	else {
		odd = 1;
		bgclass = "Even";
		}
	printf("<tr CLASS='data%s'><td CLASS='data%s'>Average</td><td CLASS='hostUP'>%2.3f%% (%2.3f%%)</td><td CLASS='hostDOWN'>%2.3f%% (%2.3f%%)</td><td CLASS='hostUNREACHABLE'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>", bgclass, bgclass, average_percent_time_up, average_percent_time_up_known, average_percent_time_down, average_percent_time_down_known, average_percent_time_unreachable, average_percent_time_unreachable_known, bgclass, average_percent_time_indeterminate);

	printf("</table>\n");
	printf("</DIV>\n");

	printf("<BR>\n");
	printf("<DIV ALIGN=CENTER CLASS='dataTitle'>Servicegroup '%s' Service State Breakdowns:</DIV>\n", sg->group_name);

	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CLASS='data'>\n");
	printf("<TR><TH CLASS='data'>Host</TH><TH CLASS='data'>Service</TH><TH CLASS='data'>%% Time OK</TH><TH CLASS='data'>%% Time Warning</TH><TH CLASS='data'>%% Time Unknown</TH><TH CLASS='data'>%% Time Critical</TH><TH CLASS='data'>%% Time Undetermined</TH></TR>\n");

	current_subject = 0;
	average_percent_time_indeterminate = 0.0;

	for(temp_subject = subject_list; temp_subject != NULL; temp_subject = temp_subject->next) {

		if(temp_subject->type != SERVICE_SUBJECT)
			continue;

		temp_service = find_service(temp_subject->host_name, temp_subject->service_description);
		if(temp_service == NULL)
			continue;

		if(is_service_member_of_servicegroup(sg, temp_service) == FALSE)
			continue;

		current_subject++;

		time_determinate = temp_subject->time_ok + temp_subject->time_warning + temp_subject->time_unknown + temp_subject->time_critical;
		time_indeterminate = total_time - time_determinate;

		/* adjust indeterminate time due to insufficient data (not all was caught) */
		temp_subject->time_indeterminate_nodata = time_indeterminate - temp_subject->time_indeterminate_notrunning;

		/* initialize values */
		percent_time_ok = 0.0;
		percent_time_warning = 0.0;
		percent_time_unknown = 0.0;
		percent_time_critical = 0.0;
		percent_time_indeterminate = 0.0;
		percent_time_ok_known = 0.0;
		percent_time_warning_known = 0.0;
		percent_time_unknown_known = 0.0;
		percent_time_critical_known = 0.0;

		if(total_time > 0) {
			percent_time_ok = (double)(((double)temp_subject->time_ok * 100.0) / (double)total_time);
			percent_time_warning = (double)(((double)temp_subject->time_warning * 100.0) / (double)total_time);
			percent_time_unknown = (double)(((double)temp_subject->time_unknown * 100.0) / (double)total_time);
			percent_time_critical = (double)(((double)temp_subject->time_critical * 100.0) / (double)total_time);
			percent_time_indeterminate = (double)(((double)time_indeterminate * 100.0) / (double)total_time);
			if(time_determinate > 0) {
				percent_time_ok_known = (double)(((double)temp_subject->time_ok * 100.0) / (double)time_determinate);
				percent_time_warning_known = (double)(((double)temp_subject->time_warning * 100.0) / (double)time_determinate);
				percent_time_unknown_known = (double)(((double)temp_subject->time_unknown * 100.0) / (double)time_determinate);
				percent_time_critical_known = (double)(((double)temp_subject->time_critical * 100.0) / (double)time_determinate);
				}
			}

		if(odd) {
			odd = 0;
			bgclass = "Odd";
			}
		else {
			odd = 1;
			bgclass = "Even";
			}

		printf("<tr CLASS='data%s'><td CLASS='data%s'>", bgclass, bgclass);
		if(strcmp(temp_subject->host_name, last_host))
			host_report_url(temp_subject->host_name, temp_subject->host_name);
		printf("</td><td CLASS='data%s'>", bgclass);
		service_report_url(temp_subject->host_name, temp_subject->service_description, temp_subject->service_description);
		printf("</td><td CLASS='serviceOK'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceWARNING'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceUNKNOWN'>%2.3f%% (%2.3f%%)</td><td class='serviceCRITICAL'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>\n", percent_time_ok, percent_time_ok_known, percent_time_warning, percent_time_warning_known, percent_time_unknown, percent_time_unknown_known, percent_time_critical, percent_time_critical_known, bgclass, percent_time_indeterminate);

		strncpy(last_host, temp_subject->host_name, sizeof(last_host) - 1);
		last_host[sizeof(last_host) - 1] = '\x0';

		get_running_average(&average_percent_time_ok, percent_time_ok, current_subject);
		get_running_average(&average_percent_time_ok_known, percent_time_ok_known, current_subject);
		get_running_average(&average_percent_time_unknown, percent_time_unknown, current_subject);
		get_running_average(&average_percent_time_unknown_known, percent_time_unknown_known, current_subject);
		get_running_average(&average_percent_time_warning, percent_time_warning, current_subject);
		get_running_average(&average_percent_time_warning_known, percent_time_warning_known, current_subject);
		get_running_average(&average_percent_time_critical, percent_time_critical, current_subject);
		get_running_average(&average_percent_time_critical_known, percent_time_critical_known, current_subject);
		get_running_average(&average_percent_time_indeterminate, percent_time_indeterminate, current_subject);
		}

	/* display average stats */
	if(odd) {
		odd = 0;
		bgclass = "Odd";
		}
	else {
		odd = 1;
		bgclass = "Even";
		}

	printf("<tr CLASS='data%s'><td CLASS='data%s' colspan='2'>Average</td><td CLASS='serviceOK'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceWARNING'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceUNKNOWN'>%2.3f%% (%2.3f%%)</td><td class='serviceCRITICAL'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>\n", bgclass, bgclass, average_percent_time_ok, average_percent_time_ok_known, average_percent_time_warning, average_percent_time_warning_known, average_percent_time_unknown, average_percent_time_unknown_known, average_percent_time_critical, average_percent_time_critical_known, bgclass, average_percent_time_indeterminate);

	printf("</table>\n");
	printf("</DIV>\n");

	return;
	}


/* display host availability */
void display_host_availability(void) {
	unsigned long total_time;
	unsigned long time_determinate;
	unsigned long time_indeterminate;
	avail_subject *temp_subject;
	host *temp_host;
	service *temp_service;
	int days, hours, minutes, seconds;
	char time_indeterminate_string[48];
	char time_determinate_string[48];
	char total_time_string[48];
	double percent_time_ok = 0.0;
	double percent_time_warning = 0.0;
	double percent_time_unknown = 0.0;
	double percent_time_critical = 0.0;
	double percent_time_indeterminate = 0.0;
	double percent_time_ok_known = 0.0;
	double percent_time_warning_known = 0.0;
	double percent_time_unknown_known = 0.0;
	double percent_time_critical_known = 0.0;
	char time_up_string[48];
	char time_down_string[48];
	char time_unreachable_string[48];
	double percent_time_up = 0.0;
	double percent_time_down = 0.0;
	double percent_time_unreachable = 0.0;
	double percent_time_up_known = 0.0;
	double percent_time_down_known = 0.0;
	double percent_time_unreachable_known = 0.0;

	double percent_time_up_scheduled = 0.0;
	double percent_time_up_unscheduled = 0.0;
	double percent_time_down_scheduled = 0.0;
	double percent_time_down_unscheduled = 0.0;
	double percent_time_unreachable_scheduled = 0.0;
	double percent_time_unreachable_unscheduled = 0.0;
	double percent_time_up_scheduled_known = 0.0;
	double percent_time_up_unscheduled_known = 0.0;
	double percent_time_down_scheduled_known = 0.0;
	double percent_time_down_unscheduled_known = 0.0;
	double percent_time_unreachable_scheduled_known = 0.0;
	double percent_time_unreachable_unscheduled_known = 0.0;
	char time_up_scheduled_string[48];
	char time_up_unscheduled_string[48];
	char time_down_scheduled_string[48];
	char time_down_unscheduled_string[48];
	char time_unreachable_scheduled_string[48];
	char time_unreachable_unscheduled_string[48];

	char time_indeterminate_scheduled_string[48];
	char time_indeterminate_unscheduled_string[48];
	char time_indeterminate_notrunning_string[48];
	char time_indeterminate_nodata_string[48];
	double percent_time_indeterminate_notrunning = 0.0;
	double percent_time_indeterminate_nodata = 0.0;

	double average_percent_time_up = 0.0;
	double average_percent_time_up_known = 0.0;
	double average_percent_time_down = 0.0;
	double average_percent_time_down_known = 0.0;
	double average_percent_time_unreachable = 0.0;
	double average_percent_time_unreachable_known = 0.0;
	double average_percent_time_indeterminate = 0.0;

	double average_percent_time_ok = 0.0;
	double average_percent_time_ok_known = 0.0;
	double average_percent_time_unknown = 0.0;
	double average_percent_time_unknown_known = 0.0;
	double average_percent_time_warning = 0.0;
	double average_percent_time_warning_known = 0.0;
	double average_percent_time_critical = 0.0;
	double average_percent_time_critical_known = 0.0;

	int current_subject = 0;

	const char *bgclass = "";
	int odd = 1;

	/* calculate total time during period based on timeperiod used for reporting */
	total_time = calculate_total_time(t1, t2);

#ifdef DEBUG
	printf("Total time: '%ld' seconds<br>\n", total_time);
#endif

	/* show data for a specific host */
	if(show_all_hosts == FALSE) {

		temp_subject = find_subject(HOST_SUBJECT, host_name, NULL);
		if(temp_subject == NULL)
			return;

		temp_host = find_host(temp_subject->host_name);
		if(temp_host == NULL)
			return;

		/* the user isn't authorized to view this host */
		if(is_authorized_for_host(temp_host, &current_authdata) == FALSE)
			return;

		time_determinate = temp_subject->time_up + temp_subject->time_down + temp_subject->time_unreachable;
		time_indeterminate = total_time - time_determinate;

		/* adjust indeterminate time due to insufficient data (not all was caught) */
		temp_subject->time_indeterminate_nodata = time_indeterminate - temp_subject->time_indeterminate_notrunning;

		/* up times */
		get_time_breakdown(temp_subject->time_up, &days, &hours, &minutes, &seconds);
		snprintf(time_up_string, sizeof(time_up_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->scheduled_time_up, &days, &hours, &minutes, &seconds);
		snprintf(time_up_scheduled_string, sizeof(time_up_scheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->time_up - temp_subject->scheduled_time_up, &days, &hours, &minutes, &seconds);
		snprintf(time_up_unscheduled_string, sizeof(time_up_unscheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		/* down times */
		get_time_breakdown(temp_subject->time_down, &days, &hours, &minutes, &seconds);
		snprintf(time_down_string, sizeof(time_down_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->scheduled_time_down, &days, &hours, &minutes, &seconds);
		snprintf(time_down_scheduled_string, sizeof(time_down_scheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->time_down - temp_subject->scheduled_time_down, &days, &hours, &minutes, &seconds);
		snprintf(time_down_unscheduled_string, sizeof(time_down_unscheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		/* unreachable times */
		get_time_breakdown(temp_subject->time_unreachable, &days, &hours, &minutes, &seconds);
		snprintf(time_unreachable_string, sizeof(time_unreachable_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->scheduled_time_unreachable, &days, &hours, &minutes, &seconds);
		snprintf(time_unreachable_scheduled_string, sizeof(time_unreachable_scheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->time_unreachable - temp_subject->scheduled_time_unreachable, &days, &hours, &minutes, &seconds);
		snprintf(time_unreachable_unscheduled_string, sizeof(time_unreachable_unscheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		/* indeterminate times */
		get_time_breakdown(time_indeterminate, &days, &hours, &minutes, &seconds);
		snprintf(time_indeterminate_string, sizeof(time_indeterminate_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->scheduled_time_indeterminate, &days, &hours, &minutes, &seconds);
		snprintf(time_indeterminate_scheduled_string, sizeof(time_indeterminate_scheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(time_indeterminate - temp_subject->scheduled_time_indeterminate, &days, &hours, &minutes, &seconds);
		snprintf(time_indeterminate_unscheduled_string, sizeof(time_indeterminate_unscheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->time_indeterminate_notrunning, &days, &hours, &minutes, &seconds);
		snprintf(time_indeterminate_notrunning_string, sizeof(time_indeterminate_notrunning_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->time_indeterminate_nodata, &days, &hours, &minutes, &seconds);
		snprintf(time_indeterminate_nodata_string, sizeof(time_indeterminate_nodata_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		get_time_breakdown(time_determinate, &days, &hours, &minutes, &seconds);
		snprintf(time_determinate_string, sizeof(time_determinate_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		get_time_breakdown(total_time, &days, &hours, &minutes, &seconds);
		snprintf(total_time_string, sizeof(total_time_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		if(total_time > 0) {
			percent_time_up = (double)(((double)temp_subject->time_up * 100.0) / (double)total_time);
			percent_time_up_scheduled = (double)(((double)temp_subject->scheduled_time_up * 100.0) / (double)total_time);
			percent_time_up_unscheduled = percent_time_up - percent_time_up_scheduled;
			percent_time_down = (double)(((double)temp_subject->time_down * 100.0) / (double)total_time);
			percent_time_down_scheduled = (double)(((double)temp_subject->scheduled_time_down * 100.0) / (double)total_time);
			percent_time_down_unscheduled = percent_time_down - percent_time_down_scheduled;
			percent_time_unreachable = (double)(((double)temp_subject->time_unreachable * 100.0) / (double)total_time);
			percent_time_unreachable_scheduled = (double)(((double)temp_subject->scheduled_time_unreachable * 100.0) / (double)total_time);
			percent_time_unreachable_unscheduled = percent_time_unreachable - percent_time_unreachable_scheduled;
			percent_time_indeterminate = (double)(((double)time_indeterminate * 100.0) / (double)total_time);
			percent_time_indeterminate_notrunning = (double)(((double)temp_subject->time_indeterminate_notrunning * 100.0) / (double)total_time);
			percent_time_indeterminate_nodata = (double)(((double)temp_subject->time_indeterminate_nodata * 100.0) / (double)total_time);
			if(time_determinate > 0) {
				percent_time_up_known = (double)(((double)temp_subject->time_up * 100.0) / (double)time_determinate);
				percent_time_up_scheduled_known = (double)(((double)temp_subject->scheduled_time_up * 100.0) / (double)time_determinate);
				percent_time_up_unscheduled_known = percent_time_up_known - percent_time_up_scheduled_known;
				percent_time_down_known = (double)(((double)temp_subject->time_down * 100.0) / (double)time_determinate);
				percent_time_down_scheduled_known = (double)(((double)temp_subject->scheduled_time_down * 100.0) / (double)time_determinate);
				percent_time_down_unscheduled_known = percent_time_down_known - percent_time_down_scheduled_known;
				percent_time_unreachable_known = (double)(((double)temp_subject->time_unreachable * 100.0) / (double)time_determinate);
				percent_time_unreachable_scheduled_known = (double)(((double)temp_subject->scheduled_time_unreachable * 100.0) / (double)time_determinate);
				percent_time_unreachable_unscheduled_known = percent_time_unreachable_known - percent_time_unreachable_scheduled_known;
				}
			}

		printf("<DIV ALIGN=CENTER CLASS='dataTitle'>Host State Breakdowns:</DIV>\n");

#ifdef USE_TRENDS
		printf("<p align='center'>\n");
		printf("<a href='%s?host=%s", TRENDS_CGI, url_encode(host_name));
		printf("&t1=%lu&t2=%lu&includesoftstates=%s&assumestateretention=%s&assumeinitialstates=%s&assumestatesduringnotrunning=%s&initialassumedhoststate=%d&backtrack=%d'>", t1, t2, (include_soft_states == TRUE) ? "yes" : "no", (assume_state_retention == TRUE) ? "yes" : "no", (assume_initial_states == TRUE) ? "yes" : "no", (assume_states_during_notrunning == TRUE) ? "yes" : "no", initial_assumed_host_state, backtrack_archives);
		printf("<img src='%s?createimage&smallimage&host=%s", TRENDS_CGI, url_encode(host_name));
		printf("&t1=%lu&t2=%lu&includesoftstates=%s&assumestateretention=%s&assumeinitialstates=%s&assumestatesduringnotrunning=%s&initialassumedhoststate=%d&backtrack=%d' border=1 alt='Host State Trends' title='Host State Trends' width='500' height='20'>", t1, t2, (include_soft_states == TRUE) ? "yes" : "no", (assume_state_retention == TRUE) ? "yes" : "no", (assume_initial_states == TRUE) ? "yes" : "no", (assume_states_during_notrunning == TRUE) ? "yes" : "no", initial_assumed_host_state, backtrack_archives);
		printf("</a><br>\n");
		printf("</p>\n");
#endif
		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>State</TH><TH CLASS='data'>Type / Reason</TH><TH CLASS='data'>Time</TH><TH CLASS='data'>%% Total Time</TH><TH CLASS='data'>%% Known Time</TH></TR>\n");

		/* up times */
		printf("<tr CLASS='dataEven'><td CLASS='hostUP' rowspan=3>UP</td>");
		printf("<td CLASS='dataEven'>Unscheduled</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>%2.3f%%</td><td class='dataEven'>%2.3f%%</td></tr>\n", time_up_unscheduled_string, percent_time_up, percent_time_up_known);
		printf("<tr CLASS='dataEven'><td CLASS='dataEven'>Scheduled</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>%2.3f%%</td><td class='dataEven'>%2.3f%%</td></tr>\n", time_up_scheduled_string, percent_time_up_scheduled, percent_time_up_scheduled_known);
		printf("<tr CLASS='hostUP'><td CLASS='hostUP'>Total</td><td CLASS='hostUP'>%s</td><td CLASS='hostUP'>%2.3f%%</td><td class='hostUP'>%2.3f%%</td></tr>\n", time_up_string, percent_time_up, percent_time_up_known);

		/* down times */
		printf("<tr CLASS='dataOdd'><td CLASS='hostDOWN' rowspan=3>DOWN</td>");
		printf("<td CLASS='dataOdd'>Unscheduled</td><td CLASS='dataOdd'>%s</td><td CLASS='dataOdd'>%2.3f%%</td><td class='dataOdd'>%2.3f%%</td></tr>\n", time_down_unscheduled_string, percent_time_down_unscheduled, percent_time_down_unscheduled_known);
		printf("<tr CLASS='dataOdd'><td CLASS='dataOdd'>Scheduled</td><td CLASS='dataOdd'>%s</td><td CLASS='dataOdd'>%2.3f%%</td><td class='dataOdd'>%2.3f%%</td></tr>\n", time_down_scheduled_string, percent_time_down_scheduled, percent_time_down_scheduled_known);
		printf("<tr CLASS='hostDOWN'><td CLASS='hostDOWN'>Total</td><td CLASS='hostDOWN'>%s</td><td CLASS='hostDOWN'>%2.3f%%</td><td class='hostDOWN'>%2.3f%%</td></tr>\n", time_down_string, percent_time_down, percent_time_down_known);

		/* unreachable times */
		printf("<tr CLASS='dataEven'><td CLASS='hostUNREACHABLE' rowspan=3>UNREACHABLE</td>");
		printf("<td CLASS='dataEven'>Unscheduled</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>%2.3f%%</td><td class='dataEven'>%2.3f%%</td></tr>\n", time_unreachable_unscheduled_string, percent_time_unreachable, percent_time_unreachable_known);
		printf("<tr CLASS='dataEven'><td CLASS='dataEven'>Scheduled</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>%2.3f%%</td><td class='dataEven'>%2.3f%%</td></tr>\n", time_unreachable_scheduled_string, percent_time_unreachable_scheduled, percent_time_unreachable_scheduled_known);
		printf("<tr CLASS='hostUNREACHABLE'><td CLASS='hostUNREACHABLE'>Total</td><td CLASS='hostUNREACHABLE'>%s</td><td CLASS='hostUNREACHABLE'>%2.3f%%</td><td class='hostUNREACHABLE'>%2.3f%%</td></tr>\n", time_unreachable_string, percent_time_unreachable, percent_time_unreachable_known);

		/* indeterminate times */
		printf("<tr CLASS='dataOdd'><td CLASS='dataOdd' rowspan=3>Undetermined</td>");
		printf("<td CLASS='dataOdd'>Nagios Not Running</td><td CLASS='dataOdd'>%s</td><td CLASS='dataOdd'>%2.3f%%</td><td CLASS='dataOdd'></td></tr>\n", time_indeterminate_notrunning_string, percent_time_indeterminate_notrunning);
		printf("<tr CLASS='dataOdd'><td CLASS='dataOdd'>Insufficient Data</td><td CLASS='dataOdd'>%s</td><td CLASS='dataOdd'>%2.3f%%</td><td CLASS='dataOdd'></td></tr>\n", time_indeterminate_nodata_string, percent_time_indeterminate_nodata);
		printf("<tr CLASS='dataOdd'><td CLASS='dataOdd'>Total</td><td CLASS='dataOdd'>%s</td><td CLASS='dataOdd'>%2.3f%%</td><td CLASS='dataOdd'></td></tr>\n", time_indeterminate_string, percent_time_indeterminate);

		printf("<tr><td colspan=3></td></tr>\n");

		printf("<tr CLASS='dataEven'><td CLASS='dataEven'>All</td><td class='dataEven'>Total</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>100.000%%</td><td CLASS='dataEven'>100.000%%</td></tr>\n", total_time_string);
		printf("</table>\n");
		printf("</DIV>\n");



		/* display state breakdowns for all services on this host */

		printf("<BR><BR>\n");
		printf("<DIV ALIGN=CENTER CLASS='dataTitle'>State Breakdowns For Host Services:</DIV>\n");

		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>Service</TH><TH CLASS='data'>%% Time OK</TH><TH CLASS='data'>%% Time Warning</TH><TH CLASS='data'>%% Time Unknown</TH><TH CLASS='data'>%% Time Critical</TH><TH CLASS='data'>%% Time Undetermined</TH></TR>\n");

		for(temp_subject = subject_list; temp_subject != NULL; temp_subject = temp_subject->next) {

			if(temp_subject->type != SERVICE_SUBJECT)
				continue;

			temp_service = find_service(temp_subject->host_name, temp_subject->service_description);
			if(temp_service == NULL)
				continue;

			/* the user isn't authorized to view this service */
			if(is_authorized_for_service(temp_service, &current_authdata) == FALSE)
				continue;

			current_subject++;

			if(odd) {
				odd = 0;
				bgclass = "Odd";
				}
			else {
				odd = 1;
				bgclass = "Even";
				}

			/* reset variables */
			percent_time_ok = 0.0;
			percent_time_warning = 0.0;
			percent_time_unknown = 0.0;
			percent_time_critical = 0.0;
			percent_time_indeterminate = 0.0;
			percent_time_ok_known = 0.0;
			percent_time_warning_known = 0.0;
			percent_time_unknown_known = 0.0;
			percent_time_critical_known = 0.0;

			time_determinate = temp_subject->time_ok + temp_subject->time_warning + temp_subject->time_unknown + temp_subject->time_critical;
			time_indeterminate = total_time - time_determinate;

			if(total_time > 0) {
				percent_time_ok = (double)(((double)temp_subject->time_ok * 100.0) / (double)total_time);
				percent_time_warning = (double)(((double)temp_subject->time_warning * 100.0) / (double)total_time);
				percent_time_unknown = (double)(((double)temp_subject->time_unknown * 100.0) / (double)total_time);
				percent_time_critical = (double)(((double)temp_subject->time_critical * 100.0) / (double)total_time);
				percent_time_indeterminate = (double)(((double)time_indeterminate * 100.0) / (double)total_time);
				if(time_determinate > 0) {
					percent_time_ok_known = (double)(((double)temp_subject->time_ok * 100.0) / (double)time_determinate);
					percent_time_warning_known = (double)(((double)temp_subject->time_warning * 100.0) / (double)time_determinate);
					percent_time_unknown_known = (double)(((double)temp_subject->time_unknown * 100.0) / (double)time_determinate);
					percent_time_critical_known = (double)(((double)temp_subject->time_critical * 100.0) / (double)time_determinate);
					}
				}

			printf("<tr CLASS='data%s'><td CLASS='data%s'>", bgclass, bgclass);
			service_report_url(temp_subject->host_name, temp_subject->service_description, temp_subject->service_description);
			printf("</td><td CLASS='serviceOK'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceWARNING'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceUNKNOWN'>%2.3f%% (%2.3f%%)</td><td class='serviceCRITICAL'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>\n", percent_time_ok, percent_time_ok_known, percent_time_warning, percent_time_warning_known, percent_time_unknown, percent_time_unknown_known, percent_time_critical, percent_time_critical_known, bgclass, percent_time_indeterminate);

			get_running_average(&average_percent_time_ok, percent_time_ok, current_subject);
			get_running_average(&average_percent_time_ok_known, percent_time_ok_known, current_subject);
			get_running_average(&average_percent_time_unknown, percent_time_unknown, current_subject);
			get_running_average(&average_percent_time_unknown_known, percent_time_unknown_known, current_subject);
			get_running_average(&average_percent_time_warning, percent_time_warning, current_subject);
			get_running_average(&average_percent_time_warning_known, percent_time_warning_known, current_subject);
			get_running_average(&average_percent_time_critical, percent_time_critical, current_subject);
			get_running_average(&average_percent_time_critical_known, percent_time_critical_known, current_subject);
			get_running_average(&average_percent_time_indeterminate, percent_time_indeterminate, current_subject);
			}

		/* display average stats */
		if(odd) {
			odd = 0;
			bgclass = "Odd";
			}
		else {
			odd = 1;
			bgclass = "Even";
			}

		printf("<tr CLASS='data%s'><td CLASS='data%s'>Average</td><td CLASS='serviceOK'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceWARNING'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceUNKNOWN'>%2.3f%% (%2.3f%%)</td><td class='serviceCRITICAL'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>\n", bgclass, bgclass, average_percent_time_ok, average_percent_time_ok_known, average_percent_time_warning, average_percent_time_warning_known, average_percent_time_unknown, average_percent_time_unknown_known, average_percent_time_critical, average_percent_time_critical_known, bgclass, average_percent_time_indeterminate);

		printf("</table>\n");
		printf("</DIV>\n");


		/* write log entries for the host */
		temp_subject = find_subject(HOST_SUBJECT, host_name, NULL);
		write_log_entries(temp_subject);
		}


	/* display data for all hosts */
	else {

		if(output_format == HTML_OUTPUT) {

			printf("<BR><BR>\n");
			printf("<DIV ALIGN=CENTER CLASS='dataTitle'>Host State Breakdowns:</DIV>\n");

			printf("<DIV ALIGN=CENTER>\n");
			printf("<TABLE BORDER=0 CLASS='data'>\n");
			printf("<TR><TH CLASS='data'>Host</TH><TH CLASS='data'>%% Time Up</TH><TH CLASS='data'>%% Time Down</TH><TH CLASS='data'>%% Time Unreachable</TH><TH CLASS='data'>%% Time Undetermined</TH></TR>\n");
			}

		else if(output_format == CSV_OUTPUT) {
			printf("HOST_NAME,");

			printf(" TIME_UP_SCHEDULED, PERCENT_TIME_UP_SCHEDULED, PERCENT_KNOWN_TIME_UP_SCHEDULED, TIME_UP_UNSCHEDULED, PERCENT_TIME_UP_UNSCHEDULED, PERCENT_KNOWN_TIME_UP_UNSCHEDULED, TOTAL_TIME_UP, PERCENT_TOTAL_TIME_UP, PERCENT_KNOWN_TIME_UP,");

			printf(" TIME_DOWN_SCHEDULED, PERCENT_TIME_DOWN_SCHEDULED, PERCENT_KNOWN_TIME_DOWN_SCHEDULED, TIME_DOWN_UNSCHEDULED, PERCENT_TIME_DOWN_UNSCHEDULED, PERCENT_KNOWN_TIME_DOWN_UNSCHEDULED, TOTAL_TIME_DOWN, PERCENT_TOTAL_TIME_DOWN, PERCENT_KNOWN_TIME_DOWN,");

			printf(" TIME_UNREACHABLE_SCHEDULED, PERCENT_TIME_UNREACHABLE_SCHEDULED, PERCENT_KNOWN_TIME_UNREACHABLE_SCHEDULED, TIME_UNREACHABLE_UNSCHEDULED, PERCENT_TIME_UNREACHABLE_UNSCHEDULED, PERCENT_KNOWN_TIME_UNREACHABLE_UNSCHEDULED, TOTAL_TIME_UNREACHABLE, PERCENT_TOTAL_TIME_UNREACHABLE, PERCENT_KNOWN_TIME_UNREACHABLE,");

			printf(" TIME_UNDETERMINED_NOT_RUNNING, PERCENT_TIME_UNDETERMINED_NOT_RUNNING, TIME_UNDETERMINED_NO_DATA, PERCENT_TIME_UNDETERMINED_NO_DATA, TOTAL_TIME_UNDETERMINED, PERCENT_TOTAL_TIME_UNDETERMINED\n");
			}


		for(temp_subject = subject_list; temp_subject != NULL; temp_subject = temp_subject->next) {

			if(temp_subject->type != HOST_SUBJECT)
				continue;

			temp_host = find_host(temp_subject->host_name);
			if(temp_host == NULL)
				continue;

			/* the user isn't authorized to view this host */
			if(is_authorized_for_host(temp_host, &current_authdata) == FALSE)
				continue;

			current_subject++;

			time_determinate = temp_subject->time_up + temp_subject->time_down + temp_subject->time_unreachable;
			time_indeterminate = total_time - time_determinate;

			/* adjust indeterminate time due to insufficient data (not all was caught) */
			temp_subject->time_indeterminate_nodata = time_indeterminate - temp_subject->time_indeterminate_notrunning;

			/* initialize values */
			percent_time_up = 0.0;
			percent_time_up_scheduled = 0.0;
			percent_time_up_unscheduled = 0.0;
			percent_time_down = 0.0;
			percent_time_down_scheduled = 0.0;
			percent_time_down_unscheduled = 0.0;
			percent_time_unreachable = 0.0;
			percent_time_unreachable_scheduled = 0.0;
			percent_time_unreachable_unscheduled = 0.0;
			percent_time_indeterminate = 0.0;
			percent_time_indeterminate_notrunning = 0.0;
			percent_time_indeterminate_nodata = 0.0;
			percent_time_up_known = 0.0;
			percent_time_up_scheduled_known = 0.0;
			percent_time_up_unscheduled_known = 0.0;
			percent_time_down_known = 0.0;
			percent_time_down_scheduled_known = 0.0;
			percent_time_down_unscheduled_known = 0.0;
			percent_time_unreachable_known = 0.0;
			percent_time_unreachable_scheduled_known = 0.0;
			percent_time_unreachable_unscheduled_known = 0.0;

			if(total_time > 0) {
				percent_time_up = (double)(((double)temp_subject->time_up * 100.0) / (double)total_time);
				percent_time_up_scheduled = (double)(((double)temp_subject->scheduled_time_up * 100.0) / (double)total_time);
				percent_time_up_unscheduled = percent_time_up - percent_time_up_scheduled;
				percent_time_down = (double)(((double)temp_subject->time_down * 100.0) / (double)total_time);
				percent_time_down_scheduled = (double)(((double)temp_subject->scheduled_time_down * 100.0) / (double)total_time);
				percent_time_down_unscheduled = percent_time_down - percent_time_down_scheduled;
				percent_time_unreachable = (double)(((double)temp_subject->time_unreachable * 100.0) / (double)total_time);
				percent_time_unreachable_scheduled = (double)(((double)temp_subject->scheduled_time_unreachable * 100.0) / (double)total_time);
				percent_time_unreachable_unscheduled = percent_time_unreachable - percent_time_unreachable_scheduled;
				percent_time_indeterminate = (double)(((double)time_indeterminate * 100.0) / (double)total_time);
				percent_time_indeterminate_notrunning = (double)(((double)temp_subject->time_indeterminate_notrunning * 100.0) / (double)total_time);
				percent_time_indeterminate_nodata = (double)(((double)temp_subject->time_indeterminate_nodata * 100.0) / (double)total_time);
				if(time_determinate > 0) {
					percent_time_up_known = (double)(((double)temp_subject->time_up * 100.0) / (double)time_determinate);
					percent_time_up_scheduled_known = (double)(((double)temp_subject->scheduled_time_up * 100.0) / (double)time_determinate);
					percent_time_up_unscheduled_known = percent_time_up_known - percent_time_up_scheduled_known;
					percent_time_down_known = (double)(((double)temp_subject->time_down * 100.0) / (double)time_determinate);
					percent_time_down_scheduled_known = (double)(((double)temp_subject->scheduled_time_down * 100.0) / (double)time_determinate);
					percent_time_down_unscheduled_known = percent_time_down_known - percent_time_down_scheduled_known;
					percent_time_unreachable_known = (double)(((double)temp_subject->time_unreachable * 100.0) / (double)time_determinate);
					percent_time_unreachable_scheduled_known = (double)(((double)temp_subject->scheduled_time_unreachable * 100.0) / (double)time_determinate);
					percent_time_unreachable_unscheduled_known = percent_time_unreachable_known - percent_time_unreachable_scheduled_known;
					}
				}

			if(output_format == HTML_OUTPUT) {

				if(odd) {
					odd = 0;
					bgclass = "Odd";
					}
				else {
					odd = 1;
					bgclass = "Even";
					}

				printf("<tr CLASS='data%s'><td CLASS='data%s'>", bgclass, bgclass);
				host_report_url(temp_subject->host_name, temp_subject->host_name);
				printf("</td><td CLASS='hostUP'>%2.3f%% (%2.3f%%)</td><td CLASS='hostDOWN'>%2.3f%% (%2.3f%%)</td><td CLASS='hostUNREACHABLE'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>\n", percent_time_up, percent_time_up_known, percent_time_down, percent_time_down_known, percent_time_unreachable, percent_time_unreachable_known, bgclass, percent_time_indeterminate);
				}
			else if(output_format == CSV_OUTPUT) {

				/* host name */
				printf("\"%s\",", temp_subject->host_name);

				/* up times */
				printf(" %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%,", temp_subject->scheduled_time_up, percent_time_up_scheduled, percent_time_up_scheduled_known, temp_subject->time_up - temp_subject->scheduled_time_up, percent_time_up_unscheduled, percent_time_up_unscheduled_known, temp_subject->time_up, percent_time_up, percent_time_up_known);

				/* down times */
				printf(" %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%,", temp_subject->scheduled_time_down, percent_time_down_scheduled, percent_time_down_scheduled_known, temp_subject->time_down - temp_subject->scheduled_time_down, percent_time_down_unscheduled, percent_time_down_unscheduled_known, temp_subject->time_down, percent_time_down, percent_time_down_known);

				/* unreachable times */
				printf(" %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%,", temp_subject->scheduled_time_unreachable, percent_time_unreachable_scheduled, percent_time_unreachable_scheduled_known, temp_subject->time_unreachable - temp_subject->scheduled_time_unreachable, percent_time_unreachable_unscheduled, percent_time_unreachable_unscheduled_known, temp_subject->time_unreachable, percent_time_unreachable, percent_time_unreachable_known);

				/* indeterminate times */
				printf(" %lu, %2.3f%%, %lu, %2.3f%%, %lu, %2.3f%%\n", temp_subject->time_indeterminate_notrunning, percent_time_indeterminate_notrunning, temp_subject->time_indeterminate_nodata, percent_time_indeterminate_nodata, time_indeterminate, percent_time_indeterminate);
				}

			get_running_average(&average_percent_time_up, percent_time_up, current_subject);
			get_running_average(&average_percent_time_up_known, percent_time_up_known, current_subject);
			get_running_average(&average_percent_time_down, percent_time_down, current_subject);
			get_running_average(&average_percent_time_down_known, percent_time_down_known, current_subject);
			get_running_average(&average_percent_time_unreachable, percent_time_unreachable, current_subject);
			get_running_average(&average_percent_time_unreachable_known, percent_time_unreachable_known, current_subject);
			get_running_average(&average_percent_time_indeterminate, percent_time_indeterminate, current_subject);
			}

		if(output_format == HTML_OUTPUT) {

			/* average statistics */
			if(odd) {
				odd = 0;
				bgclass = "Odd";
				}
			else {
				odd = 1;
				bgclass = "Even";
				}
			printf("<tr CLASS='data%s'><td CLASS='data%s'>Average</td><td CLASS='hostUP'>%2.3f%% (%2.3f%%)</td><td CLASS='hostDOWN'>%2.3f%% (%2.3f%%)</td><td CLASS='hostUNREACHABLE'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>", bgclass, bgclass, average_percent_time_up, average_percent_time_up_known, average_percent_time_down, average_percent_time_down_known, average_percent_time_unreachable, average_percent_time_unreachable_known, bgclass, average_percent_time_indeterminate);

			printf("</table>\n");
			printf("</DIV>\n");
			}
		}

	return;
	}


/* display service availability */
void display_service_availability(void) {
	unsigned long total_time;
	unsigned long time_determinate;
	unsigned long time_indeterminate;
	avail_subject *temp_subject;
	service *temp_service;
	int days, hours, minutes, seconds;
	char time_ok_string[48];
	char time_warning_string[48];
	char time_unknown_string[48];
	char time_critical_string[48];
	char time_indeterminate_string[48];
	char time_determinate_string[48];
	char total_time_string[48];
	double percent_time_ok = 0.0;
	double percent_time_warning = 0.0;
	double percent_time_unknown = 0.0;
	double percent_time_critical = 0.0;
	double percent_time_indeterminate = 0.0;
	double percent_time_ok_known = 0.0;
	double percent_time_warning_known = 0.0;
	double percent_time_unknown_known = 0.0;
	double percent_time_critical_known = 0.0;

	char time_critical_scheduled_string[48];
	char time_critical_unscheduled_string[48];
	double percent_time_critical_scheduled = 0.0;
	double percent_time_critical_unscheduled = 0.0;
	double percent_time_critical_scheduled_known = 0.0;
	double percent_time_critical_unscheduled_known = 0.0;
	char time_unknown_scheduled_string[48];
	char time_unknown_unscheduled_string[48];
	double percent_time_unknown_scheduled = 0.0;
	double percent_time_unknown_unscheduled = 0.0;
	double percent_time_unknown_scheduled_known = 0.0;
	double percent_time_unknown_unscheduled_known = 0.0;
	char time_warning_scheduled_string[48];
	char time_warning_unscheduled_string[48];
	double percent_time_warning_scheduled = 0.0;
	double percent_time_warning_unscheduled = 0.0;
	double percent_time_warning_scheduled_known = 0.0;
	double percent_time_warning_unscheduled_known = 0.0;
	char time_ok_scheduled_string[48];
	char time_ok_unscheduled_string[48];
	double percent_time_ok_scheduled = 0.0;
	double percent_time_ok_unscheduled = 0.0;
	double percent_time_ok_scheduled_known = 0.0;
	double percent_time_ok_unscheduled_known = 0.0;

	double average_percent_time_ok = 0.0;
	double average_percent_time_ok_known = 0.0;
	double average_percent_time_unknown = 0.0;
	double average_percent_time_unknown_known = 0.0;
	double average_percent_time_warning = 0.0;
	double average_percent_time_warning_known = 0.0;
	double average_percent_time_critical = 0.0;
	double average_percent_time_critical_known = 0.0;
	double average_percent_time_indeterminate = 0.0;

	int current_subject = 0;

	char time_indeterminate_scheduled_string[48];
	char time_indeterminate_unscheduled_string[48];
	char time_indeterminate_notrunning_string[48];
	char time_indeterminate_nodata_string[48];
	double percent_time_indeterminate_notrunning = 0.0;
	double percent_time_indeterminate_nodata = 0.0;

	int odd = 1;
	const char *bgclass = "";
	char last_host[128] = "";


	/* calculate total time during period based on timeperiod used for reporting */
	total_time = calculate_total_time(t1, t2);

	/* we're only getting data for one service */
	if(show_all_services == FALSE) {

		temp_subject = find_subject(SERVICE_SUBJECT, host_name, svc_description);
		if(temp_subject == NULL)
			return;

		temp_service = find_service(temp_subject->host_name, temp_subject->service_description);
		if(temp_service == NULL)
			return;

		/* the user isn't authorized to view this service */
		if(is_authorized_for_service(temp_service, &current_authdata) == FALSE)
			return;

		time_determinate = temp_subject->time_ok + temp_subject->time_warning + temp_subject->time_unknown + temp_subject->time_critical;
		time_indeterminate = total_time - time_determinate;

		/* adjust indeterminate time due to insufficient data (not all was caught) */
		temp_subject->time_indeterminate_nodata = time_indeterminate - temp_subject->time_indeterminate_notrunning;

		/* ok states */
		get_time_breakdown(temp_subject->time_ok, &days, &hours, &minutes, &seconds);
		snprintf(time_ok_string, sizeof(time_ok_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->scheduled_time_ok, &days, &hours, &minutes, &seconds);
		snprintf(time_ok_scheduled_string, sizeof(time_ok_scheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->time_ok - temp_subject->scheduled_time_ok, &days, &hours, &minutes, &seconds);
		snprintf(time_ok_unscheduled_string, sizeof(time_ok_unscheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		/* warning states */
		get_time_breakdown(temp_subject->time_warning, &days, &hours, &minutes, &seconds);
		snprintf(time_warning_string, sizeof(time_warning_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->scheduled_time_warning, &days, &hours, &minutes, &seconds);
		snprintf(time_warning_scheduled_string, sizeof(time_warning_scheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->time_warning - temp_subject->scheduled_time_warning, &days, &hours, &minutes, &seconds);
		snprintf(time_warning_unscheduled_string, sizeof(time_warning_unscheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		/* unknown states */
		get_time_breakdown(temp_subject->time_unknown, &days, &hours, &minutes, &seconds);
		snprintf(time_unknown_string, sizeof(time_unknown_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->scheduled_time_unknown, &days, &hours, &minutes, &seconds);
		snprintf(time_unknown_scheduled_string, sizeof(time_unknown_scheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->time_unknown - temp_subject->scheduled_time_unknown, &days, &hours, &minutes, &seconds);
		snprintf(time_unknown_unscheduled_string, sizeof(time_unknown_unscheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		/* critical states */
		get_time_breakdown(temp_subject->time_critical, &days, &hours, &minutes, &seconds);
		snprintf(time_critical_string, sizeof(time_critical_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->scheduled_time_critical, &days, &hours, &minutes, &seconds);
		snprintf(time_critical_scheduled_string, sizeof(time_critical_scheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->time_critical - temp_subject->scheduled_time_critical, &days, &hours, &minutes, &seconds);
		snprintf(time_critical_unscheduled_string, sizeof(time_critical_unscheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		/* indeterminate time */
		get_time_breakdown(time_indeterminate, &days, &hours, &minutes, &seconds);
		snprintf(time_indeterminate_string, sizeof(time_indeterminate_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->scheduled_time_indeterminate, &days, &hours, &minutes, &seconds);
		snprintf(time_indeterminate_scheduled_string, sizeof(time_indeterminate_scheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(time_indeterminate - temp_subject->scheduled_time_indeterminate, &days, &hours, &minutes, &seconds);
		snprintf(time_indeterminate_unscheduled_string, sizeof(time_indeterminate_unscheduled_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->time_indeterminate_notrunning, &days, &hours, &minutes, &seconds);
		snprintf(time_indeterminate_notrunning_string, sizeof(time_indeterminate_notrunning_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);
		get_time_breakdown(temp_subject->time_indeterminate_nodata, &days, &hours, &minutes, &seconds);
		snprintf(time_indeterminate_nodata_string, sizeof(time_indeterminate_nodata_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		get_time_breakdown(time_determinate, &days, &hours, &minutes, &seconds);
		snprintf(time_determinate_string, sizeof(time_determinate_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		get_time_breakdown(total_time, &days, &hours, &minutes, &seconds);
		snprintf(total_time_string, sizeof(total_time_string) - 1, "%dd %dh %dm %ds", days, hours, minutes, seconds);

		if(total_time > 0) {
			percent_time_ok = (double)(((double)temp_subject->time_ok * 100.0) / (double)total_time);
			percent_time_ok_scheduled = (double)(((double)temp_subject->scheduled_time_ok * 100.0) / (double)total_time);
			percent_time_ok_unscheduled = percent_time_ok - percent_time_ok_scheduled;
			percent_time_warning = (double)(((double)temp_subject->time_warning * 100.0) / (double)total_time);
			percent_time_warning_scheduled = (double)(((double)temp_subject->scheduled_time_unknown * 100.0) / (double)total_time);
			percent_time_warning_unscheduled = percent_time_warning - percent_time_warning_scheduled;
			percent_time_unknown = (double)(((double)temp_subject->time_unknown * 100.0) / (double)total_time);
			percent_time_unknown_scheduled = (double)(((double)temp_subject->scheduled_time_unknown * 100.0) / (double)total_time);
			percent_time_unknown_unscheduled = percent_time_unknown - percent_time_unknown_scheduled;
			percent_time_critical = (double)(((double)temp_subject->time_critical * 100.0) / (double)total_time);
			percent_time_critical_scheduled = (double)(((double)temp_subject->scheduled_time_critical * 100.0) / (double)total_time);
			percent_time_critical_unscheduled = percent_time_critical - percent_time_critical_scheduled;
			percent_time_indeterminate = (double)(((double)time_indeterminate * 100.0) / (double)total_time);
			percent_time_indeterminate_notrunning = (double)(((double)temp_subject->time_indeterminate_notrunning * 100.0) / (double)total_time);
			percent_time_indeterminate_nodata = (double)(((double)temp_subject->time_indeterminate_nodata * 100.0) / (double)total_time);
			if(time_determinate > 0) {
				percent_time_ok_known = (double)(((double)temp_subject->time_ok * 100.0) / (double)time_determinate);
				percent_time_ok_scheduled_known = (double)(((double)temp_subject->scheduled_time_ok * 100.0) / (double)time_determinate);
				percent_time_ok_unscheduled_known = percent_time_ok_known - percent_time_ok_scheduled_known;
				percent_time_warning_known = (double)(((double)temp_subject->time_warning * 100.0) / (double)time_determinate);
				percent_time_warning_scheduled_known = (double)(((double)temp_subject->scheduled_time_warning * 100.0) / (double)time_determinate);
				percent_time_warning_unscheduled_known = percent_time_warning_known - percent_time_warning_scheduled_known;
				percent_time_unknown_known = (double)(((double)temp_subject->time_unknown * 100.0) / (double)time_determinate);
				percent_time_unknown_scheduled_known = (double)(((double)temp_subject->scheduled_time_unknown * 100.0) / (double)time_determinate);
				percent_time_unknown_unscheduled_known = percent_time_unknown_known - percent_time_unknown_scheduled_known;
				percent_time_critical_known = (double)(((double)temp_subject->time_critical * 100.0) / (double)time_determinate);
				percent_time_critical_scheduled_known = (double)(((double)temp_subject->scheduled_time_critical * 100.0) / (double)time_determinate);
				percent_time_critical_unscheduled_known = percent_time_critical_known - percent_time_critical_scheduled_known;
				}
			}

		printf("<DIV ALIGN=CENTER CLASS='dataTitle'>Service State Breakdowns:</DIV>\n");
#ifdef USE_TRENDS
		printf("<p align='center'>\n");
		printf("<a href='%s?host=%s", TRENDS_CGI, url_encode(host_name));
		printf("&service=%s&t1=%lu&t2=%lu&includesoftstates=%s&assumestateretention=%s&assumeinitialstates=%s&assumestatesduringnotrunning=%s&initialassumedservicestate=%d&backtrack=%d'>", url_encode(svc_description), t1, t2, (include_soft_states == TRUE) ? "yes" : "no", (assume_state_retention == TRUE) ? "yes" : "no", (assume_initial_states == TRUE) ? "yes" : "no", (assume_states_during_notrunning == TRUE) ? "yes" : "no", initial_assumed_service_state, backtrack_archives);
		printf("<img src='%s?createimage&smallimage&host=%s", TRENDS_CGI, url_encode(host_name));
		printf("&service=%s&t1=%lu&t2=%lu&includesoftstates=%s&assumestateretention=%s&assumeinitialstates=%s&assumestatesduringnotrunning=%s&initialassumedservicestate=%d&backtrack=%d' border=1 alt='Service State Trends' title='Service State Trends' width='500' height='20'>", url_encode(svc_description), t1, t2, (include_soft_states == TRUE) ? "yes" : "no", (assume_state_retention == TRUE) ? "yes" : "no", (assume_initial_states == TRUE) ? "yes" : "no", (assume_states_during_notrunning == TRUE) ? "yes" : "no", initial_assumed_service_state, backtrack_archives);
		printf("</a><br>\n");
		printf("</p>\n");
#endif

		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>State</TH><TH CLASS='data'>Type / Reason</TH><TH CLASS='data'>Time</TH><TH CLASS='data'>%% Total Time</TH><TH CLASS='data'>%% Known Time</TH></TR>\n");

		/* ok states */
		printf("<tr CLASS='dataEven'><td CLASS='serviceOK' rowspan=3>OK</td>");
		printf("<td CLASS='dataEven'>Unscheduled</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>%2.3f%%</td><td CLASS='dataEven'>%2.3f%%</td></tr>\n", time_ok_unscheduled_string, percent_time_ok_unscheduled, percent_time_ok_unscheduled_known);
		printf("<tr CLASS='dataEven'><td CLASS='dataEven'>Scheduled</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>%2.3f%%</td><td CLASS='dataEven'>%2.3f%%</td></tr>\n", time_ok_scheduled_string, percent_time_ok_scheduled, percent_time_ok_scheduled_known);
		printf("<tr CLASS='serviceOK'><td CLASS='serviceOK'>Total</td><td CLASS='serviceOK'>%s</td><td CLASS='serviceOK'>%2.3f%%</td><td CLASS='serviceOK'>%2.3f%%</td></tr>\n", time_ok_string, percent_time_ok, percent_time_ok_known);

		/* warning states */
		printf("<tr CLASS='dataOdd'><td CLASS='serviceWARNING' rowspan=3>WARNING</td>");
		printf("<td CLASS='dataOdd'>Unscheduled</td><td CLASS='dataOdd'>%s</td><td CLASS='dataOdd'>%2.3f%%</td><td CLASS='dataOdd'>%2.3f%%</td></tr>\n", time_warning_unscheduled_string, percent_time_warning_unscheduled, percent_time_warning_unscheduled_known);
		printf("<tr CLASS='dataOdd'><td CLASS='dataOdd'>Scheduled</td><td CLASS='dataOdd'>%s</td><td CLASS='dataOdd'>%2.3f%%</td><td CLASS='dataOdd'>%2.3f%%</td></tr>\n", time_warning_scheduled_string, percent_time_warning_scheduled, percent_time_warning_scheduled_known);
		printf("<tr CLASS='serviceWARNING'><td CLASS='serviceWARNING'>Total</td><td CLASS='serviceWARNING'>%s</td><td CLASS='serviceWARNING'>%2.3f%%</td><td CLASS='serviceWARNING'>%2.3f%%</td></tr>\n", time_warning_string, percent_time_warning, percent_time_warning_known);

		/* unknown states */
		printf("<tr CLASS='dataEven'><td CLASS='serviceUNKNOWN' rowspan=3>UNKNOWN</td>");
		printf("<td CLASS='dataEven'>Unscheduled</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>%2.3f%%</td><td CLASS='dataEven'>%2.3f%%</td></tr>\n", time_unknown_unscheduled_string, percent_time_unknown_unscheduled, percent_time_unknown_unscheduled_known);
		printf("<tr CLASS='dataEven'><td CLASS='dataEven'>Scheduled</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>%2.3f%%</td><td CLASS='dataEven'>%2.3f%%</td></tr>\n", time_unknown_scheduled_string, percent_time_unknown_scheduled, percent_time_unknown_scheduled_known);
		printf("<tr CLASS='serviceUNKNOWN'><td CLASS='serviceUNKNOWN'>Total</td><td CLASS='serviceUNKNOWN'>%s</td><td CLASS='serviceUNKNOWN'>%2.3f%%</td><td CLASS='serviceUNKNOWN'>%2.3f%%</td></tr>\n", time_unknown_string, percent_time_unknown, percent_time_unknown_known);

		/* critical states */
		printf("<tr CLASS='dataOdd'><td CLASS='serviceCRITICAL' rowspan=3>CRITICAL</td>");
		printf("<td CLASS='dataOdd'>Unscheduled</td><td CLASS='dataOdd'>%s</td><td CLASS='dataOdd'>%2.3f%%</td><td CLASS='dataOdd'>%2.3f%%</td></tr>\n", time_critical_unscheduled_string, percent_time_critical_unscheduled, percent_time_critical_unscheduled_known);
		printf("<tr CLASS='dataOdd'><td CLASS='dataOdd'>Scheduled</td><td CLASS='dataOdd'>%s</td><td CLASS='dataOdd'>%2.3f%%</td><td CLASS='dataOdd'>%2.3f%%</td></tr>\n", time_critical_scheduled_string, percent_time_critical_scheduled, percent_time_critical_scheduled_known);
		printf("<tr CLASS='serviceCRITICAL'><td CLASS='serviceCRITICAL'>Total</td><td CLASS='serviceCRITICAL'>%s</td><td CLASS='serviceCRITICAL'>%2.3f%%</td><td CLASS='serviceCRITICAL'>%2.3f%%</td></tr>\n", time_critical_string, percent_time_critical, percent_time_critical_known);


		printf("<tr CLASS='dataEven'><td CLASS='dataEven' rowspan=3>Undetermined</td>");

		printf("<td CLASS='dataEven'>Nagios Not Running</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>%2.3f%%</td><td CLASS='dataEven'></td></tr>\n", time_indeterminate_notrunning_string, percent_time_indeterminate_notrunning);
		printf("<tr CLASS='dataEven'><td CLASS='dataEven'>Insufficient Data</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>%2.3f%%</td><td CLASS='dataEven'></td></tr>\n", time_indeterminate_nodata_string, percent_time_indeterminate_nodata);
		printf("<tr CLASS='dataEven'><td CLASS='dataEven'>Total</td><td CLASS='dataEven'>%s</td><td CLASS='dataEven'>%2.3f%%</td><td CLASS='dataEven'></td></tr>\n", time_indeterminate_string, percent_time_indeterminate);

		printf("<tr><td colspan=3></td></tr>\n");
		printf("<tr CLASS='dataOdd'><td CLASS='dataOdd'>All</td><td CLASS='dataOdd'>Total</td><td CLASS='dataOdd'>%s</td><td CLASS='dataOdd'>100.000%%</td><td CLASS='dataOdd'>100.000%%</td></tr>\n", total_time_string);
		printf("</table>\n");
		printf("</DIV>\n");


		write_log_entries(temp_subject);
		}


	/* display data for all services */
	else {

		if(output_format == HTML_OUTPUT) {

			printf("<DIV ALIGN=CENTER CLASS='dataTitle'>Service State Breakdowns:</DIV>\n");

			printf("<DIV ALIGN=CENTER>\n");
			printf("<TABLE BORDER=0 CLASS='data'>\n");
			printf("<TR><TH CLASS='data'>Host</TH><TH CLASS='data'>Service</TH><TH CLASS='data'>%% Time OK</TH><TH CLASS='data'>%% Time Warning</TH><TH CLASS='data'>%% Time Unknown</TH><TH CLASS='data'>%% Time Critical</TH><TH CLASS='data'>%% Time Undetermined</TH></TR>\n");
			}
		else if(output_format == CSV_OUTPUT) {
			printf("HOST_NAME, SERVICE_DESCRIPTION,");

			printf(" TIME_OK_SCHEDULED, PERCENT_TIME_OK_SCHEDULED, PERCENT_KNOWN_TIME_OK_SCHEDULED, TIME_OK_UNSCHEDULED, PERCENT_TIME_OK_UNSCHEDULED, PERCENT_KNOWN_TIME_OK_UNSCHEDULED, TOTAL_TIME_OK, PERCENT_TOTAL_TIME_OK, PERCENT_KNOWN_TIME_OK,");

			printf(" TIME_WARNING_SCHEDULED, PERCENT_TIME_WARNING_SCHEDULED, PERCENT_KNOWN_TIME_WARNING_SCHEDULED, TIME_WARNING_UNSCHEDULED, PERCENT_TIME_WARNING_UNSCHEDULED, PERCENT_KNOWN_TIME_WARNING_UNSCHEDULED, TOTAL_TIME_WARNING, PERCENT_TOTAL_TIME_WARNING, PERCENT_KNOWN_TIME_WARNING,");

			printf(" TIME_UNKNOWN_SCHEDULED, PERCENT_TIME_UNKNOWN_SCHEDULED, PERCENT_KNOWN_TIME_UNKNOWN_SCHEDULED, TIME_UNKNOWN_UNSCHEDULED, PERCENT_TIME_UNKNOWN_UNSCHEDULED, PERCENT_KNOWN_TIME_UNKNOWN_UNSCHEDULED, TOTAL_TIME_UNKNOWN, PERCENT_TOTAL_TIME_UNKNOWN, PERCENT_KNOWN_TIME_UNKNOWN,");

			printf(" TIME_CRITICAL_SCHEDULED, PERCENT_TIME_CRITICAL_SCHEDULED, PERCENT_KNOWN_TIME_CRITICAL_SCHEDULED, TIME_CRITICAL_UNSCHEDULED, PERCENT_TIME_CRITICAL_UNSCHEDULED, PERCENT_KNOWN_TIME_CRITICAL_UNSCHEDULED, TOTAL_TIME_CRITICAL, PERCENT_TOTAL_TIME_CRITICAL, PERCENT_KNOWN_TIME_CRITICAL,");

			printf(" TIME_UNDETERMINED_NOT_RUNNING, PERCENT_TIME_UNDETERMINED_NOT_RUNNING, TIME_UNDETERMINED_NO_DATA, PERCENT_TIME_UNDETERMINED_NO_DATA, TOTAL_TIME_UNDETERMINED, PERCENT_TOTAL_TIME_UNDETERMINED\n");
			}


		for(temp_subject = subject_list; temp_subject != NULL; temp_subject = temp_subject->next) {

			if(temp_subject->type != SERVICE_SUBJECT)
				continue;

			temp_service = find_service(temp_subject->host_name, temp_subject->service_description);
			if(temp_service == NULL)
				continue;

			/* the user isn't authorized to view this service */
			if(is_authorized_for_service(temp_service, &current_authdata) == FALSE)
				continue;

			current_subject++;

			time_determinate = temp_subject->time_ok + temp_subject->time_warning + temp_subject->time_unknown + temp_subject->time_critical;
			time_indeterminate = total_time - time_determinate;

			/* adjust indeterminate time due to insufficient data (not all was caught) */
			temp_subject->time_indeterminate_nodata = time_indeterminate - temp_subject->time_indeterminate_notrunning;

			/* initialize values */
			percent_time_ok = 0.0;
			percent_time_ok_scheduled = 0.0;
			percent_time_ok_unscheduled = 0.0;
			percent_time_warning = 0.0;
			percent_time_warning_scheduled = 0.0;
			percent_time_warning_unscheduled = 0.0;
			percent_time_unknown = 0.0;
			percent_time_unknown_scheduled = 0.0;
			percent_time_unknown_unscheduled = 0.0;
			percent_time_critical = 0.0;
			percent_time_critical_scheduled = 0.0;
			percent_time_critical_unscheduled = 0.0;
			percent_time_indeterminate = 0.0;
			percent_time_indeterminate_notrunning = 0.0;
			percent_time_indeterminate_nodata = 0.0;
			percent_time_ok_known = 0.0;
			percent_time_ok_scheduled_known = 0.0;
			percent_time_ok_unscheduled_known = 0.0;
			percent_time_warning_known = 0.0;
			percent_time_warning_scheduled_known = 0.0;
			percent_time_warning_unscheduled_known = 0.0;
			percent_time_unknown_known = 0.0;
			percent_time_unknown_scheduled_known = 0.0;
			percent_time_unknown_unscheduled_known = 0.0;
			percent_time_critical_known = 0.0;
			percent_time_critical_scheduled_known = 0.0;
			percent_time_critical_unscheduled_known = 0.0;

			if(total_time > 0) {
				percent_time_ok = (double)(((double)temp_subject->time_ok * 100.0) / (double)total_time);
				percent_time_ok_scheduled = (double)(((double)temp_subject->scheduled_time_ok * 100.0) / (double)total_time);
				percent_time_ok_unscheduled = percent_time_ok - percent_time_ok_scheduled;
				percent_time_warning = (double)(((double)temp_subject->time_warning * 100.0) / (double)total_time);
				percent_time_warning_scheduled = (double)(((double)temp_subject->scheduled_time_unknown * 100.0) / (double)total_time);
				percent_time_warning_unscheduled = percent_time_warning - percent_time_warning_scheduled;
				percent_time_unknown = (double)(((double)temp_subject->time_unknown * 100.0) / (double)total_time);
				percent_time_unknown_scheduled = (double)(((double)temp_subject->scheduled_time_unknown * 100.0) / (double)total_time);
				percent_time_unknown_unscheduled = percent_time_unknown - percent_time_unknown_scheduled;
				percent_time_critical = (double)(((double)temp_subject->time_critical * 100.0) / (double)total_time);
				percent_time_critical_scheduled = (double)(((double)temp_subject->scheduled_time_critical * 100.0) / (double)total_time);
				percent_time_critical_unscheduled = percent_time_critical - percent_time_critical_scheduled;
				percent_time_indeterminate = (double)(((double)time_indeterminate * 100.0) / (double)total_time);
				percent_time_indeterminate_notrunning = (double)(((double)temp_subject->time_indeterminate_notrunning * 100.0) / (double)total_time);
				percent_time_indeterminate_nodata = (double)(((double)temp_subject->time_indeterminate_nodata * 100.0) / (double)total_time);
				if(time_determinate > 0) {
					percent_time_ok_known = (double)(((double)temp_subject->time_ok * 100.0) / (double)time_determinate);
					percent_time_ok_scheduled_known = (double)(((double)temp_subject->scheduled_time_ok * 100.0) / (double)time_determinate);
					percent_time_ok_unscheduled_known = percent_time_ok_known - percent_time_ok_scheduled_known;
					percent_time_warning_known = (double)(((double)temp_subject->time_warning * 100.0) / (double)time_determinate);
					percent_time_warning_scheduled_known = (double)(((double)temp_subject->scheduled_time_warning * 100.0) / (double)time_determinate);
					percent_time_warning_unscheduled_known = percent_time_warning_known - percent_time_warning_scheduled_known;
					percent_time_unknown_known = (double)(((double)temp_subject->time_unknown * 100.0) / (double)time_determinate);
					percent_time_unknown_scheduled_known = (double)(((double)temp_subject->scheduled_time_unknown * 100.0) / (double)time_determinate);
					percent_time_unknown_unscheduled_known = percent_time_unknown_known - percent_time_unknown_scheduled_known;
					percent_time_critical_known = (double)(((double)temp_subject->time_critical * 100.0) / (double)time_determinate);
					percent_time_critical_scheduled_known = (double)(((double)temp_subject->scheduled_time_critical * 100.0) / (double)time_determinate);
					percent_time_critical_unscheduled_known = percent_time_critical_known - percent_time_critical_scheduled_known;
					}
				}

			if(output_format == HTML_OUTPUT) {

				if(odd) {
					odd = 0;
					bgclass = "Odd";
					}
				else {
					odd = 1;
					bgclass = "Even";
					}

				printf("<tr CLASS='data%s'><td CLASS='data%s'>", bgclass, bgclass);
				if(strcmp(temp_subject->host_name, last_host))
					host_report_url(temp_subject->host_name, temp_subject->host_name);
				printf("</td><td CLASS='data%s'>", bgclass);
				service_report_url(temp_subject->host_name, temp_subject->service_description, temp_subject->service_description);
				printf("</td><td CLASS='serviceOK'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceWARNING'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceUNKNOWN'>%2.3f%% (%2.3f%%)</td><td class='serviceCRITICAL'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>\n", percent_time_ok, percent_time_ok_known, percent_time_warning, percent_time_warning_known, percent_time_unknown, percent_time_unknown_known, percent_time_critical, percent_time_critical_known, bgclass, percent_time_indeterminate);
				}
			else if(output_format == CSV_OUTPUT) {

				/* host name and service description */
				printf("\"%s\", \"%s\",", temp_subject->host_name, temp_subject->service_description);

				/* ok times */
				printf(" %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%,", temp_subject->scheduled_time_ok, percent_time_ok_scheduled, percent_time_ok_scheduled_known, temp_subject->time_ok - temp_subject->scheduled_time_ok, percent_time_ok_unscheduled, percent_time_ok_unscheduled_known, temp_subject->time_ok, percent_time_ok, percent_time_ok_known);

				/* warning times */
				printf(" %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%,", temp_subject->scheduled_time_warning, percent_time_warning_scheduled, percent_time_warning_scheduled_known, temp_subject->time_warning - temp_subject->scheduled_time_warning, percent_time_warning_unscheduled, percent_time_warning_unscheduled_known, temp_subject->time_warning, percent_time_warning, percent_time_warning_known);

				/* unknown times */
				printf(" %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%,", temp_subject->scheduled_time_unknown, percent_time_unknown_scheduled, percent_time_unknown_scheduled_known, temp_subject->time_unknown - temp_subject->scheduled_time_unknown, percent_time_unknown_unscheduled, percent_time_unknown_unscheduled_known, temp_subject->time_unknown, percent_time_unknown, percent_time_unknown_known);

				/* critical times */
				printf(" %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%, %lu, %2.3f%%, %2.3f%%,", temp_subject->scheduled_time_critical, percent_time_critical_scheduled, percent_time_critical_scheduled_known, temp_subject->time_critical - temp_subject->scheduled_time_critical, percent_time_critical_unscheduled, percent_time_critical_unscheduled_known, temp_subject->time_critical, percent_time_critical, percent_time_critical_known);

				/* indeterminate times */
				printf(" %lu, %2.3f%%, %lu, %2.3f%%, %lu, %2.3f%%\n", temp_subject->time_indeterminate_notrunning, percent_time_indeterminate_notrunning, temp_subject->time_indeterminate_nodata, percent_time_indeterminate_nodata, time_indeterminate, percent_time_indeterminate);
				}

			strncpy(last_host, temp_subject->host_name, sizeof(last_host) - 1);
			last_host[sizeof(last_host) - 1] = '\x0';

			get_running_average(&average_percent_time_ok, percent_time_ok, current_subject);
			get_running_average(&average_percent_time_ok_known, percent_time_ok_known, current_subject);
			get_running_average(&average_percent_time_unknown, percent_time_unknown, current_subject);
			get_running_average(&average_percent_time_unknown_known, percent_time_unknown_known, current_subject);
			get_running_average(&average_percent_time_warning, percent_time_warning, current_subject);
			get_running_average(&average_percent_time_warning_known, percent_time_warning_known, current_subject);
			get_running_average(&average_percent_time_critical, percent_time_critical, current_subject);
			get_running_average(&average_percent_time_critical_known, percent_time_critical_known, current_subject);
			get_running_average(&average_percent_time_indeterminate, percent_time_indeterminate, current_subject);
			}

		if(output_format == HTML_OUTPUT) {

			/* average statistics */
			if(odd) {
				odd = 0;
				bgclass = "Odd";
				}
			else {
				odd = 1;
				bgclass = "Even";
				}
			printf("<tr CLASS='data%s'><td CLASS='data%s' colspan='2'>Average</td><td CLASS='serviceOK'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceWARNING'>%2.3f%% (%2.3f%%)</td><td CLASS='serviceUNKNOWN'>%2.3f%% (%2.3f%%)</td><td class='serviceCRITICAL'>%2.3f%% (%2.3f%%)</td><td class='data%s'>%2.3f%%</td></tr>\n", bgclass, bgclass, average_percent_time_ok, average_percent_time_ok_known, average_percent_time_warning, average_percent_time_warning_known, average_percent_time_unknown, average_percent_time_unknown_known, average_percent_time_critical, average_percent_time_critical_known, bgclass, average_percent_time_indeterminate);

			printf("</table>\n");
			printf("</DIV>\n");
			}
		}

	return;
	}




void host_report_url(const char *hn, const char *label) {

	printf("<a href='%s?host=%s", AVAIL_CGI, url_encode(hn));
	printf("&show_log_entries");
	printf("&t1=%lu&t2=%lu", t1, t2);
	printf("&backtrack=%d", backtrack_archives);
	printf("&assumestateretention=%s", (assume_state_retention == TRUE) ? "yes" : "no");
	printf("&assumeinitialstates=%s", (assume_initial_states == TRUE) ? "yes" : "no");
	printf("&assumestatesduringnotrunning=%s", (assume_states_during_notrunning == TRUE) ? "yes" : "no");
	printf("&initialassumedhoststate=%d", initial_assumed_host_state);
	printf("&initialassumedservicestate=%d", initial_assumed_service_state);
	if(show_log_entries == TRUE)
		printf("&show_log_entries");
	if(full_log_entries == TRUE)
		printf("&full_log_entries");
	printf("&showscheduleddowntime=%s", (show_scheduled_downtime == TRUE) ? "yes" : "no");
	if(current_timeperiod != NULL)
		printf("&rpttimeperiod=%s", url_encode(current_timeperiod->name));
	printf("'>%s</a>", label);

	return;
	}


void service_report_url(const char *hn, const char *sd, const char *label) {

	printf("<a href='%s?host=%s", AVAIL_CGI, url_encode(hn));
	printf("&service=%s", url_encode(sd));
	printf("&t1=%lu&t2=%lu", t1, t2);
	printf("&backtrack=%d", backtrack_archives);
	printf("&assumestateretention=%s", (assume_state_retention == TRUE) ? "yes" : "no");
	printf("&assumeinitialstates=%s", (assume_initial_states == TRUE) ? "yes" : "no");
	printf("&assumestatesduringnotrunning=%s", (assume_states_during_notrunning == TRUE) ? "yes" : "no");
	printf("&initialassumedhoststate=%d", initial_assumed_host_state);
	printf("&initialassumedservicestate=%d", initial_assumed_service_state);
	if(show_log_entries == TRUE)
		printf("&show_log_entries");
	if(full_log_entries == TRUE)
		printf("&full_log_entries");
	printf("&showscheduleddowntime=%s", (show_scheduled_downtime == TRUE) ? "yes" : "no");
	if(current_timeperiod != NULL)
		printf("&rpttimeperiod=%s", url_encode(current_timeperiod->name));
	printf("'>%s</a>", label);

	return;
	}


/* calculates running average */
void get_running_average(double *running_average, double new_value, int current_item) {

	*running_average = (((*running_average * ((double)current_item - 1.0)) + new_value) / (double)current_item);

	return;
	}


/* used in reports where a timeperiod is selected */
unsigned long calculate_total_time(time_t start_time, time_t end_time) {
	struct tm *t;
	time_t midnight_today;
	int weekday;
	unsigned long total_time;
	timerange *temp_timerange;
	unsigned long temp_duration;
	unsigned long temp_end;
	unsigned long temp_start;
	unsigned long start;
	unsigned long end;

	/* attempt to handle the current time_period */
	if(current_timeperiod != NULL) {

		/* "A day" is 86400 seconds */
		t = localtime(&start_time);

		/* calculate the start of the day (midnight, 00:00 hours) */
		t->tm_sec = 0;
		t->tm_min = 0;
		t->tm_hour = 0;
		t->tm_isdst = -1;
		midnight_today = mktime(t);
		weekday = t->tm_wday;

		total_time = 0;
		while(midnight_today < end_time) {
			temp_duration = 0;
			temp_end = min(86400, t2 - midnight_today);
			temp_start = 0;
			if(t1 > midnight_today)
				temp_start = t1 - midnight_today;

			/* check all time ranges for this day of the week */
			for(temp_timerange = current_timeperiod->days[weekday]; temp_timerange != NULL; temp_timerange = temp_timerange->next) {
				start = max(temp_timerange->range_start, temp_start);
				end = min(temp_timerange->range_end, temp_end);
#ifdef DEBUG
				printf("<li>Matching in timerange[%d]: %d -> %d (%ld -> %ld) %d -> %d = %ld<br>\n", weekday, temp_timerange->range_start, temp_timerange->range_end, temp_start, temp_end, start, end, end - start);
#endif
				if(end > start)
					temp_duration += end - start;
				}
			total_time += temp_duration;
			temp_start = 0;
			midnight_today += 86400;
			if(++weekday > 6)
				weekday = 0;
			}

		return total_time;
		}

	/* no timeperiod was selected */
	return end_time - start_time;
	}
