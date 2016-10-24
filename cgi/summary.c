/**************************************************************************
 *
 * SUMMARY.C -  Nagios Alert Summary CGI
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
#include "../include/getcgi.h"
#include "../include/cgiauth.h"


extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_html_path[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char url_stylesheets_path[MAX_FILENAME_LENGTH];

/* output types */
#define HTML_OUTPUT             0
#define CSV_OUTPUT              1

/* custom report types */
#define REPORT_NONE                    0
#define REPORT_RECENT_ALERTS           1
#define REPORT_ALERT_TOTALS            2
#define REPORT_TOP_ALERTS              3
#define REPORT_HOSTGROUP_ALERT_TOTALS  4
#define REPORT_HOST_ALERT_TOTALS       5
#define REPORT_SERVICE_ALERT_TOTALS    6
#define REPORT_SERVICEGROUP_ALERT_TOTALS 7

/* standard report types */
#define SREPORT_NONE                   0
#define SREPORT_RECENT_ALERTS          1
#define SREPORT_RECENT_HOST_ALERTS     2
#define SREPORT_RECENT_SERVICE_ALERTS  3
#define SREPORT_TOP_HOST_ALERTS        4
#define SREPORT_TOP_SERVICE_ALERTS     5

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

#define AE_SOFT_STATE           1
#define AE_HARD_STATE           2

#define AE_HOST_ALERT           1
#define AE_SERVICE_ALERT        2

#define AE_HOST_PRODUCER        1
#define AE_SERVICE_PRODUCER     2

#define AE_HOST_DOWN            1
#define AE_HOST_UNREACHABLE     2
#define AE_HOST_UP              4
#define AE_SERVICE_WARNING      8
#define AE_SERVICE_UNKNOWN      16
#define AE_SERVICE_CRITICAL     32
#define AE_SERVICE_OK           64

typedef struct archived_event_struct {
	time_t  time_stamp;
	int     event_type;
	int     entry_type;
	char    *host_name;
	char    *service_description;
	int     state;
	int     state_type;
	char    *event_info;
	struct archived_event_struct *next;
	} archived_event;

typedef struct alert_producer_struct {
	int     producer_type;
	char    *host_name;
	char    *service_description;
	int     total_alerts;
	struct alert_producer_struct *next;
	} alert_producer;


void read_archived_event_data(void);
void scan_log_file_for_archived_event_data(char *);
void convert_timeperiod_to_times(int);
void compute_report_times(void);
void determine_standard_report_options(void);
void add_archived_event(int, time_t, int, int, char *, char *, char *);
alert_producer *find_producer(int, char *, char *);
alert_producer *add_producer(int, char *, char *);
void free_event_list(void);
void free_producer_list(void);

void display_report(void);
void display_recent_alerts(void);
void display_alert_totals(void);
void display_hostgroup_alert_totals(void);
void display_specific_hostgroup_alert_totals(hostgroup *);
void display_servicegroup_alert_totals(void);
void display_specific_servicegroup_alert_totals(servicegroup *);
void display_host_alert_totals(void);
void display_specific_host_alert_totals(host *);
void display_service_alert_totals(void);
void display_specific_service_alert_totals(service *);
void display_top_alerts(void);

void document_header(int);
void document_footer(void);
int process_cgivars(void);


archived_event *event_list = NULL;
alert_producer *producer_list = NULL;

authdata current_authdata;

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

int compute_time_from_parts = FALSE;
int timeperiod_type = TIMEPERIOD_CUSTOM;

int state_types = AE_HARD_STATE + AE_SOFT_STATE;
int alert_types = AE_HOST_ALERT + AE_SERVICE_ALERT;
int host_states = AE_HOST_UP + AE_HOST_DOWN + AE_HOST_UNREACHABLE;
int service_states = AE_SERVICE_OK + AE_SERVICE_WARNING + AE_SERVICE_UNKNOWN + AE_SERVICE_CRITICAL;

int show_all_hostgroups = TRUE;
int show_all_servicegroups = TRUE;
int show_all_hosts = TRUE;

char *target_hostgroup_name = "";
char *target_servicegroup_name = "";
char *target_host_name = "";
hostgroup *target_hostgroup = NULL;
servicegroup *target_servicegroup = NULL;
host *target_host = NULL;

int earliest_archive = 0;
int item_limit = 25;
int total_items = 0;

int embedded = FALSE;
int display_header = TRUE;

int output_format = HTML_OUTPUT;
int display_type = REPORT_RECENT_ALERTS;
int standard_report = SREPORT_NONE;
int generate_report = FALSE;



int main(int argc, char **argv) {
	char temp_buffer[MAX_INPUT_BUFFER];
	char start_timestring[MAX_DATETIME_LENGTH];
	char end_timestring[MAX_DATETIME_LENGTH];
	host *temp_host;
	int days, hours, minutes, seconds;
	hostgroup *temp_hostgroup;
	servicegroup *temp_servicegroup;
	time_t t3;
	time_t current_time;
	struct tm *t;
	int x;

	/* reset internal CGI variables */
	reset_cgi_vars();

	cgi_init(document_header, document_footer, READ_ALL_OBJECT_DATA, 0);

	/* initialize report time period to last 24 hours */
	time(&t2);
	t1 = (time_t)(t2 - (60 * 60 * 24));

	/* get the arguments passed in the URL */
	process_cgivars();

	document_header(TRUE);

	/* get authentication information */
	get_authentication_information(&current_authdata);

	if(standard_report != SREPORT_NONE)
		determine_standard_report_options();

	if(compute_time_from_parts == TRUE)
		compute_report_times();

	/* make sure times are sane, otherwise swap them */
	if(t2 < t1) {
		t3 = t2;
		t2 = t1;
		t1 = t3;
		}

	if(display_header == TRUE) {

		/* begin top table */
		printf("<table border=0 width=100%% cellspacing=0 cellpadding=0>\n");
		printf("<tr>\n");

		/* left column of the first row */
		printf("<td align=left valign=top width=33%%>\n");

		snprintf(temp_buffer, sizeof(temp_buffer) - 1, "Alert Summary Report");
		temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
		display_info_table(temp_buffer, FALSE, &current_authdata);

		printf("</td>\n");

		/* center column of top row */
		printf("<td align=center valign=top width=33%%>\n");

		if(generate_report == TRUE) {

			printf("<DIV ALIGN=CENTER CLASS='dataTitle'>\n");
			if(display_type == REPORT_TOP_ALERTS)
				printf("Top Alert Producers");
			else if(display_type == REPORT_ALERT_TOTALS || display_type == REPORT_HOSTGROUP_ALERT_TOTALS || display_type == REPORT_SERVICEGROUP_ALERT_TOTALS || display_type == REPORT_HOST_ALERT_TOTALS || display_type == REPORT_SERVICE_ALERT_TOTALS)
				printf("Alert Totals");
			else
				printf("Most Recent Alerts");

			if(show_all_hostgroups == FALSE)
				printf(" For Hostgroup '%s'", target_hostgroup_name);
			else if(show_all_servicegroups == FALSE)
				printf(" For Servicegroup '%s'", target_servicegroup_name);
			else if(show_all_hosts == FALSE)
				printf(" For Host '%s'", target_host_name);

			printf("</DIV>\n");

			printf("<BR>\n");

			get_time_string(&t1, start_timestring, sizeof(start_timestring) - 1, SHORT_DATE_TIME);
			get_time_string(&t2, end_timestring, sizeof(end_timestring) - 1, SHORT_DATE_TIME);
			printf("<div align=center class='reportRange'>%s to %s</div>\n", start_timestring, end_timestring);

			get_time_breakdown((time_t)(t2 - t1), &days, &hours, &minutes, &seconds);
			printf("<div align=center class='reportDuration'>Duration: %dd %dh %dm %ds</div>\n", days, hours, minutes, seconds);
			}

		printf("</td>\n");

		/* right hand column of top row */
		printf("<td align=right valign=bottom width=33%%>\n");

		if(generate_report == TRUE) {

			printf("<table border=0>\n");

			printf("<tr>\n");
			printf("<td valign=top align=left class='optBoxTitle' colspan=2>Report Options Summary:</td>\n");
			printf("</tr>\n");

			printf("<tr>\n");
			printf("<td valign=top align=left class='optBoxItem'>Alert Types:</td>\n");
			printf("<td valign=top align=left class='optBoxValue'>\n");
			if(alert_types & AE_HOST_ALERT)
				printf("Host");
			if(alert_types & AE_SERVICE_ALERT)
				printf("%sService", (alert_types & AE_HOST_ALERT) ? " &amp; " : "");
			printf(" Alerts</td>\n");
			printf("</tr>\n");

			printf("<tr>\n");
			printf("<td valign=top align=left class='optBoxItem'>State Types:</td>\n");
			printf("<td valign=top align=left class='optBoxValue'>");
			if(state_types & AE_SOFT_STATE)
				printf("Soft");
			if(state_types & AE_HARD_STATE)
				printf("%sHard", (state_types & AE_SOFT_STATE) ? " &amp; " : "");
			printf(" States</td>\n");
			printf("</tr>\n");

			printf("<tr>\n");
			printf("<td valign=top align=left class='optBoxItem'>Host States:</td>\n");
			printf("<td valign=top align=left class='optBoxValue'>");
			x = 0;
			if(host_states & AE_HOST_UP) {
				printf("Up");
				x = 1;
				}
			if(host_states & AE_HOST_DOWN) {
				printf("%sDown", (x == 1) ? ", " : "");
				x = 1;
				}
			if(host_states & AE_HOST_UNREACHABLE)
				printf("%sUnreachable", (x == 1) ? ", " : "");
			if(x == 0)
				printf("None");
			printf("</td>\n");
			printf("</tr>\n");

			printf("<tr>\n");
			printf("<td valign=top align=left class='optBoxItem'>Service States:</td>\n");
			printf("<td valign=top align=left class='optBoxValue'>");
			x = 0;
			if(service_states & AE_SERVICE_OK) {
				printf("Ok");
				x = 1;
				}
			if(service_states & AE_SERVICE_WARNING) {
				printf("%sWarning", (x == 1) ? ", " : "");
				x = 1;
				}
			if(service_states & AE_SERVICE_UNKNOWN) {
				printf("%sUnknown", (x == 1) ? ", " : "");
				x = 1;
				}
			if(service_states & AE_SERVICE_CRITICAL)
				printf("%sCritical", (x == 1) ? ", " : "");
			if(x == 0)
				printf("None");
			printf("</td>\n");
			printf("</tr>\n");

			printf("<tr>\n");
			printf("<td valign=top align=left colspan=2 class='optBoxItem'>\n");
			printf("<form action='%s' method='GET'>\n", SUMMARY_CGI);
			printf("<input type='submit' name='btnSubmit' value='Generate New Report'>\n");
			printf("</form>\n");
			printf("</td>\n");
			printf("</tr>\n");

			/* display context-sensitive help */
			printf("<tr><td></td><td align=right valign=bottom>\n");
			if(display_type == REPORT_TOP_ALERTS)
				display_context_help(CONTEXTHELP_SUMMARY_ALERT_PRODUCERS);
			else if(display_type == REPORT_ALERT_TOTALS)
				display_context_help(CONTEXTHELP_SUMMARY_ALERT_TOTALS);
			else if(display_type == REPORT_HOSTGROUP_ALERT_TOTALS)
				display_context_help(CONTEXTHELP_SUMMARY_HOSTGROUP_ALERT_TOTALS);
			else if(display_type == REPORT_HOST_ALERT_TOTALS)
				display_context_help(CONTEXTHELP_SUMMARY_HOST_ALERT_TOTALS);
			else if(display_type == REPORT_SERVICE_ALERT_TOTALS)
				display_context_help(CONTEXTHELP_SUMMARY_SERVICE_ALERT_TOTALS);
			else if(display_type == REPORT_SERVICEGROUP_ALERT_TOTALS)
				display_context_help(CONTEXTHELP_SUMMARY_SERVICEGROUP_ALERT_TOTALS);
			else
				display_context_help(CONTEXTHELP_SUMMARY_RECENT_ALERTS);
			printf("</td></tr>\n");

			printf("</table>\n");
			}

		else {
			printf("<table border=0>\n");

			printf("<tr><td></td><td align=right valign=bottom>\n");
			display_context_help(CONTEXTHELP_SUMMARY_MENU);
			printf("</td></tr>\n");

			printf("</table>\n");
			}

		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");
		}


	/*********************************/
	/****** GENERATE THE REPORT ******/
	/*********************************/

	if(generate_report == TRUE) {
		read_archived_event_data();
		display_report();
		}

	/* ask user for report options */
	else {

		time(&current_time);
		t = localtime(&current_time);

		start_day = 1;
		start_year = t->tm_year + 1900;
		end_day = t->tm_mday;
		end_year = t->tm_year + 1900;

		printf("<DIV ALIGN=CENTER CLASS='dateSelectTitle'>Standard Reports:</DIV>\n");
		printf("<DIV ALIGN=CENTER>\n");
		printf("<form method=\"get\" action=\"%s\">\n", SUMMARY_CGI);

		printf("<input type='hidden' name='report' value='1'>\n");

		printf("<table border=0 cellpadding=5>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Report Type:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='standardreport'>\n");
		printf("<option value=%d>25 Most Recent Hard Alerts\n", SREPORT_RECENT_ALERTS);
		printf("<option value=%d>25 Most Recent Hard Host Alerts\n", SREPORT_RECENT_HOST_ALERTS);
		printf("<option value=%d>25 Most Recent Hard Service Alerts\n", SREPORT_RECENT_SERVICE_ALERTS);
		printf("<option value=%d>Top 25 Hard Host Alert Producers\n", SREPORT_TOP_HOST_ALERTS);
		printf("<option value=%d>Top 25 Hard Service Alert Producers\n", SREPORT_TOP_SERVICE_ALERTS);
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td></td><td align=left class='dateSelectItem'><input type='submit' value='Create Summary Report!'></td></tr>\n");

		printf("</table>\n");

		printf("</form>\n");
		printf("</DIV>\n");

		printf("<DIV ALIGN=CENTER CLASS='dateSelectTitle'>Custom Report Options:</DIV>\n");
		printf("<DIV ALIGN=CENTER>\n");
		printf("<form method=\"get\" action=\"%s\">\n", SUMMARY_CGI);

		printf("<input type='hidden' name='report' value='1'>\n");

		printf("<table border=0 cellpadding=5>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Report Type:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='displaytype'>\n");
		printf("<option value=%d>Most Recent Alerts\n", REPORT_RECENT_ALERTS);
		printf("<option value=%d>Alert Totals\n", REPORT_ALERT_TOTALS);
		printf("<option value=%d>Alert Totals By Hostgroup\n", REPORT_HOSTGROUP_ALERT_TOTALS);
		printf("<option value=%d>Alert Totals By Host\n", REPORT_HOST_ALERT_TOTALS);
		printf("<option value=%d>Alert Totals By Servicegroup\n", REPORT_SERVICEGROUP_ALERT_TOTALS);
		printf("<option value=%d>Alert Totals By Service\n", REPORT_SERVICE_ALERT_TOTALS);
		printf("<option value=%d>Top Alert Producers\n", REPORT_TOP_ALERTS);
		printf("</select>\n");
		printf("</td></tr>\n");

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

		printf("<tr><td class='reportSelectSubTitle' valign=center>Limit To Hostgroup:</td><td align=left valign=center class='reportSelectItem'>\n");
		printf("<select name='hostgroup'>\n");
		printf("<option value='all'>** ALL HOSTGROUPS **\n");
		for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {
			if(is_authorized_for_hostgroup(temp_hostgroup, &current_authdata) == TRUE)
				printf("<option value='%s'>%s\n", escape_string(temp_hostgroup->group_name), temp_hostgroup->group_name);
			}
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' valign=center>Limit To Servicegroup:</td><td align=left valign=center class='reportSelectItem'>\n");
		printf("<select name='servicegroup'>\n");
		printf("<option value='all'>** ALL SERVICEGROUPS **\n");
		for(temp_servicegroup = servicegroup_list; temp_servicegroup != NULL; temp_servicegroup = temp_servicegroup->next) {
			if(is_authorized_for_servicegroup(temp_servicegroup, &current_authdata) == TRUE)
				printf("<option value='%s'>%s\n", escape_string(temp_servicegroup->group_name), temp_servicegroup->group_name);
			}
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' valign=center>Limit To Host:</td><td align=left valign=center class='reportSelectItem'>\n");
		printf("<select name='host'>\n");
		printf("<option value='all'>** ALL HOSTS **\n");

		for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
			if(is_authorized_for_host(temp_host, &current_authdata) == TRUE)
				printf("<option value='%s'>%s\n", escape_string(temp_host->name), temp_host->name);
			}
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Alert Types:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='alerttypes'>\n");
		printf("<option value=%d %s>Host and Service Alerts\n", AE_HOST_ALERT + AE_SERVICE_ALERT, (alert_types == AE_HOST_ALERT + AE_SERVICE_ALERT) ? "SELECTED" : "");
		printf("<option value=%d %s>Host Alerts\n", AE_HOST_ALERT, (alert_types == AE_HOST_ALERT) ? "SELECTED" : "");
		printf("<option value=%d %s>Service Alerts\n", AE_SERVICE_ALERT, (alert_types == AE_SERVICE_ALERT) ? "SELECTED" : "");
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>State Types:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='statetypes'>\n");
		printf("<option value=%d %s>Hard and Soft States\n", AE_HARD_STATE + AE_SOFT_STATE, (state_types == AE_HARD_STATE + AE_SOFT_STATE) ? "SELECTED" : "");
		printf("<option value=%d %s>Hard States\n", AE_HARD_STATE, (state_types == AE_HARD_STATE) ? "SELECTED" : "");
		printf("<option value=%d %s>Soft States\n", AE_SOFT_STATE, (state_types == AE_SOFT_STATE) ? "SELECTED" : "");
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Host States:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='hoststates'>\n");
		printf("<option value=%d>All Host States\n", AE_HOST_UP + AE_HOST_DOWN + AE_HOST_UNREACHABLE);
		printf("<option value=%d>Host Problem States\n", AE_HOST_DOWN + AE_HOST_UNREACHABLE);
		printf("<option value=%d>Host Up States\n", AE_HOST_UP);
		printf("<option value=%d>Host Down States\n", AE_HOST_DOWN);
		printf("<option value=%d>Host Unreachable States\n", AE_HOST_UNREACHABLE);
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Service States:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<select name='servicestates'>\n");
		printf("<option value=%d>All Service States\n", AE_SERVICE_OK + AE_SERVICE_WARNING + AE_SERVICE_UNKNOWN + AE_SERVICE_CRITICAL);
		printf("<option value=%d>Service Problem States\n", AE_SERVICE_WARNING + AE_SERVICE_UNKNOWN + AE_SERVICE_CRITICAL);
		printf("<option value=%d>Service Ok States\n", AE_SERVICE_OK);
		printf("<option value=%d>Service Warning States\n", AE_SERVICE_WARNING);
		printf("<option value=%d>Service Unknown States\n", AE_SERVICE_UNKNOWN);
		printf("<option value=%d>Service Critical States\n", AE_SERVICE_CRITICAL);
		printf("</select>\n");
		printf("</td></tr>\n");

		printf("<tr><td class='reportSelectSubTitle' align=right>Max List Items:</td>\n");
		printf("<td class='reportSelectItem'>\n");
		printf("<input type='text' name='limit' size='3' maxlength='3' value='%d'>\n", item_limit);
		printf("</td></tr>\n");

		printf("<tr><td></td><td align=left class='dateSelectItem'><input type='submit' value='Create Summary Report!'></td></tr>\n");

		printf("</table>\n");

		printf("</form>\n");
		printf("</DIV>\n");
		}


	document_footer();

	/* free all other allocated memory */
	free_memory();
	free_event_list();
	free_producer_list();

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
		printf("Content-type: text/html; charset=utf-8\r\n\r\n");
	else {
		printf("Content-type: text/plain\r\n\r\n");
		return;
		}

	if(embedded == TRUE || output_format == CSV_OUTPUT)
		return;

	printf("<html>\n");
	printf("<head>\n");
	printf("<link rel=\"shortcut icon\" href=\"%sfavicon.ico\" type=\"image/ico\">\n", url_images_path);
	printf("<title>\n");
	printf("Nagios Event Summary\n");
	printf("</title>\n");

	if(use_stylesheet == TRUE) {
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, COMMON_CSS);
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n", url_stylesheets_path, SUMMARY_CSS);
		}

	printf("</head>\n");

	printf("<BODY CLASS='summary'>\n");

	/* include user SSI header */
	include_ssi_files(SUMMARY_CGI, SSI_HEADER);

	return;
	}



void document_footer(void) {

	if(output_format != HTML_OUTPUT)
		return;

	if(embedded == TRUE)
		return;

	/* include user SSI footer */
	include_ssi_files(SUMMARY_CGI, SSI_FOOTER);

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

		/* we found the item limit argument */
		else if(!strcmp(variables[x], "limit")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			item_limit = atoi(variables[x]);
			}

		/* we found the state types argument */
		else if(!strcmp(variables[x], "statetypes")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			state_types = atoi(variables[x]);
			}

		/* we found the alert types argument */
		else if(!strcmp(variables[x], "alerttypes")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			alert_types = atoi(variables[x]);
			}

		/* we found the host states argument */
		else if(!strcmp(variables[x], "hoststates")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			host_states = atoi(variables[x]);
			}

		/* we found the service states argument */
		else if(!strcmp(variables[x], "servicestates")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			service_states = atoi(variables[x]);
			}

		/* we found the generate report argument */
		else if(!strcmp(variables[x], "report")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			generate_report = (atoi(variables[x]) > 0) ? TRUE : FALSE;
			}


		/* we found the display type argument */
		else if(!strcmp(variables[x], "displaytype")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			display_type = atoi(variables[x]);
			}

		/* we found the standard report argument */
		else if(!strcmp(variables[x], "standardreport")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			standard_report = atoi(variables[x]);
			}

		/* we found the hostgroup argument */
		else if(!strcmp(variables[x], "hostgroup")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((target_hostgroup_name = (char *)strdup(variables[x])) == NULL)
				target_hostgroup_name = "";
			strip_html_brackets(target_hostgroup_name);

			if(!strcmp(target_hostgroup_name, "all"))
				show_all_hostgroups = TRUE;
			else {
				show_all_hostgroups = FALSE;
				target_hostgroup = find_hostgroup(target_hostgroup_name);
				}
			}

		/* we found the servicegroup argument */
		else if(!strcmp(variables[x], "servicegroup")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((target_servicegroup_name = (char *)strdup(variables[x])) == NULL)
				target_servicegroup_name = "";
			strip_html_brackets(target_servicegroup_name);

			if(!strcmp(target_servicegroup_name, "all"))
				show_all_servicegroups = TRUE;
			else {
				show_all_servicegroups = FALSE;
				target_servicegroup = find_servicegroup(target_servicegroup_name);
				}
			}

		/* we found the host argument */
		else if(!strcmp(variables[x], "host")) {
			x++;
			if(variables[x] == NULL) {
				error = TRUE;
				break;
				}

			if((target_host_name = (char *)strdup(variables[x])) == NULL)
				target_host_name = "";
			strip_html_brackets(target_host_name);

			if(!strcmp(target_host_name, "all"))
				show_all_hosts = TRUE;
			else {
				show_all_hosts = FALSE;
				target_host = find_host(target_host_name);
				}
			}
		}

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return error;
	}



/* reads log files for archived event data */
void read_archived_event_data(void) {
	char filename[MAX_FILENAME_LENGTH];
	int oldest_archive = 0;
	int newest_archive = 0;
	int current_archive = 0;

	/* determine oldest archive to use when scanning for data */
	oldest_archive = determine_archive_to_use_from_time(t1);

	/* determine most recent archive to use when scanning for data */
	newest_archive = determine_archive_to_use_from_time(t2);

	if(oldest_archive < newest_archive)
		oldest_archive = newest_archive;

	/* read in all the necessary archived logs (from most recent to earliest) */
	for(current_archive = newest_archive; current_archive <= oldest_archive; current_archive++) {

		/* get the name of the log file that contains this archive */
		get_log_archive_to_use(current_archive, filename, sizeof(filename) - 1);

		/* scan the log file for archived state data */
		scan_log_file_for_archived_event_data(filename);
		}

	return;
	}



/* grabs archived event data from a log file */
void scan_log_file_for_archived_event_data(char *filename) {
	char *input = NULL;
	char *input2 = NULL;
	char entry_host_name[MAX_INPUT_BUFFER];
	char entry_svc_description[MAX_INPUT_BUFFER];
	int state;
	int state_type;
	char *temp_buffer;
	char *plugin_output;
	time_t time_stamp;
	mmapfile *thefile;


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

		/* get the timestamp */
		temp_buffer = my_strtok(input2, "]");
		time_stamp = (temp_buffer == NULL) ? (time_t)0 : (time_t)strtoul(temp_buffer + 1, NULL, 10);
		if(time_stamp < t1 || time_stamp > t2)
			continue;

		/* host alerts */
		if(strstr(input, "HOST ALERT:")) {

			/* get host name */
			temp_buffer = my_strtok(NULL, ":");
			temp_buffer = my_strtok(NULL, ";");
			strncpy(entry_host_name, (temp_buffer == NULL) ? "" : temp_buffer + 1, sizeof(entry_host_name));
			entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

			/* state type */
			if(strstr(input, ";SOFT;"))
				state_type = AE_SOFT_STATE;
			else
				state_type = AE_HARD_STATE;

			/* get the plugin output */
			temp_buffer = my_strtok(NULL, ";");
			temp_buffer = my_strtok(NULL, ";");
			temp_buffer = my_strtok(NULL, ";");
			plugin_output = my_strtok(NULL, "\n");

			/* state */
			if(strstr(input, ";DOWN;"))
				state = AE_HOST_DOWN;
			else if(strstr(input, ";UNREACHABLE;"))
				state = AE_HOST_UNREACHABLE;
			else if(strstr(input, ";RECOVERY") || strstr(input, ";UP;"))
				state = AE_HOST_UP;
			else
				continue;

			add_archived_event(AE_HOST_ALERT, time_stamp, state, state_type, entry_host_name, NULL, plugin_output);
			}

		/* service alerts */
		if(strstr(input, "SERVICE ALERT:")) {

			/* get host name */
			temp_buffer = my_strtok(NULL, ":");
			temp_buffer = my_strtok(NULL, ";");
			strncpy(entry_host_name, (temp_buffer == NULL) ? "" : temp_buffer + 1, sizeof(entry_host_name));
			entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

			/* get service description */
			temp_buffer = my_strtok(NULL, ";");
			strncpy(entry_svc_description, (temp_buffer == NULL) ? "" : temp_buffer, sizeof(entry_svc_description));
			entry_svc_description[sizeof(entry_svc_description) - 1] = '\x0';

			/* state type */
			if(strstr(input, ";SOFT;"))
				state_type = AE_SOFT_STATE;
			else
				state_type = AE_HARD_STATE;

			/* get the plugin output */
			temp_buffer = my_strtok(NULL, ";");
			temp_buffer = my_strtok(NULL, ";");
			temp_buffer = my_strtok(NULL, ";");
			plugin_output = my_strtok(NULL, "\n");

			/* state */
			if(strstr(input, ";WARNING;"))
				state = AE_SERVICE_WARNING;
			else if(strstr(input, ";UNKNOWN;"))
				state = AE_SERVICE_UNKNOWN;
			else if(strstr(input, ";CRITICAL;"))
				state = AE_SERVICE_CRITICAL;
			else if(strstr(input, ";RECOVERY") || strstr(input, ";OK;"))
				state = AE_SERVICE_OK;
			else
				continue;

			add_archived_event(AE_SERVICE_ALERT, time_stamp, state, state_type, entry_host_name, entry_svc_description, plugin_output);
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



void free_event_list(void) {
	archived_event *this_event = NULL;
	archived_event *next_event = NULL;

	for(this_event = event_list; this_event != NULL;) {
		next_event = this_event->next;
		if(this_event->host_name != NULL)
			free(this_event->host_name);
		if(this_event->service_description != NULL)
			free(this_event->service_description);
		if(this_event->event_info != NULL)
			free(this_event->event_info);
		free(this_event);
		this_event = next_event;
		}

	event_list = NULL;

	return;
	}


/* adds an archived event entry to the list in memory */
void add_archived_event(int event_type, time_t time_stamp, int entry_type, int state_type, char *host_name, char *svc_description, char *event_info) {
	archived_event *last_event = NULL;
	archived_event *temp_event = NULL;
	archived_event *new_event = NULL;
	service *temp_service = NULL;
	host *temp_host;


	/* check timestamp sanity */
	if(time_stamp < t1 || time_stamp > t2)
		return;

	/* check alert type (host or service alert) */
	if(!(alert_types & event_type))
		return;

	/* check state type (soft or hard state) */
	if(!(state_types & state_type))
		return;

	/* check state (host or service state) */
	if(event_type == AE_HOST_ALERT) {
		if(!(host_states & entry_type))
			return;
		}
	else {
		if(!(service_states & entry_type))
			return;
		}

	/* find the host this entry is associated with */
	temp_host = find_host(host_name);

	/* check hostgroup match (valid filter for all reports) */
	if(show_all_hostgroups == FALSE && is_host_member_of_hostgroup(target_hostgroup, temp_host) == FALSE)
		return;

	/* check host match (valid filter for some reports) */
	if(show_all_hosts == FALSE && (display_type == REPORT_RECENT_ALERTS || display_type == REPORT_HOST_ALERT_TOTALS || display_type == REPORT_SERVICE_ALERT_TOTALS)) {
		if(target_host == NULL || temp_host == NULL)
			return;
		if(strcmp(target_host->name, temp_host->name))
			return;
		}

	/* check servicegroup math (valid filter for all reports) */
	if(event_type == AE_SERVICE_ALERT) {
		temp_service = find_service(host_name, svc_description);
		if(show_all_servicegroups == FALSE && is_service_member_of_servicegroup(target_servicegroup, temp_service) == FALSE)
			return;
		}
	else {
		if(show_all_servicegroups == FALSE && is_host_member_of_servicegroup(target_servicegroup, temp_host) == FALSE)
			return;
		}

	/* check authorization */
	if(event_type == AE_SERVICE_ALERT) {
		if(is_authorized_for_service(temp_service, &current_authdata) == FALSE)
			return;
		}
	else {
		if(is_authorized_for_host(temp_host, &current_authdata) == FALSE)
			return;
		}

#ifdef DEBUG
	if(event_type == AE_HOST_ALERT)
		printf("Adding host alert (%s) @ %llu<BR>\n", host_name, (unsigned long long)time_stamp);
	else
		printf("Adding service alert (%s/%s) @ %llu<BR>\n", host_name, svc_description, (unsigned long long)time_stamp);
#endif

	/* allocate memory for the new entry */
	new_event = (archived_event *)malloc(sizeof(archived_event));
	if(new_event == NULL)
		return;

	/* allocate memory for the host name */
	if(host_name != NULL) {
		new_event->host_name = (char *)malloc(strlen(host_name) + 1);
		if(new_event->host_name != NULL)
			strcpy(new_event->host_name, host_name);
		}
	else
		new_event->host_name = NULL;

	/* allocate memory for the service description */
	if(svc_description != NULL) {
		new_event->service_description = (char *)malloc(strlen(svc_description) + 1);
		if(new_event->service_description != NULL)
			strcpy(new_event->service_description, svc_description);
		}
	else
		new_event->service_description = NULL;

	/* allocate memory for the event info */
	if(event_info != NULL) {
		new_event->event_info = (char *)malloc(strlen(event_info) + 1);
		if(new_event->event_info != NULL)
			strcpy(new_event->event_info, event_info);
		}
	else
		new_event->event_info = NULL;

	new_event->event_type = event_type;
	new_event->time_stamp = time_stamp;
	new_event->entry_type = entry_type;
	new_event->state_type = state_type;


	/* add the new entry to the list in memory, sorted by time */
	last_event = event_list;
	for(temp_event = event_list; temp_event != NULL; temp_event = temp_event->next) {
		if(new_event->time_stamp >= temp_event->time_stamp) {
			new_event->next = temp_event;
			if(temp_event == event_list)
				event_list = new_event;
			else
				last_event->next = new_event;
			break;
			}
		else
			last_event = temp_event;
		}
	if(event_list == NULL) {
		new_event->next = NULL;
		event_list = new_event;
		}
	else if(temp_event == NULL) {
		new_event->next = NULL;
		last_event->next = new_event;
		}

	total_items++;

	return;
	}



/* determines standard report options */
void determine_standard_report_options(void) {

	/* report over last 7 days */
	convert_timeperiod_to_times(TIMEPERIOD_LAST7DAYS);
	compute_time_from_parts = FALSE;

	/* common options */
	state_types = AE_HARD_STATE;
	item_limit = 25;

	/* report-specific options */
	switch(standard_report) {

		case SREPORT_RECENT_ALERTS:
			display_type = REPORT_RECENT_ALERTS;
			alert_types = AE_HOST_ALERT + AE_SERVICE_ALERT;
			host_states = AE_HOST_UP + AE_HOST_DOWN + AE_HOST_UNREACHABLE;
			service_states = AE_SERVICE_OK + AE_SERVICE_WARNING + AE_SERVICE_UNKNOWN + AE_SERVICE_CRITICAL;
			break;

		case SREPORT_RECENT_HOST_ALERTS:
			display_type = REPORT_RECENT_ALERTS;
			alert_types = AE_HOST_ALERT;
			host_states = AE_HOST_UP + AE_HOST_DOWN + AE_HOST_UNREACHABLE;
			break;

		case SREPORT_RECENT_SERVICE_ALERTS:
			display_type = REPORT_RECENT_ALERTS;
			alert_types = AE_SERVICE_ALERT;
			service_states = AE_SERVICE_OK + AE_SERVICE_WARNING + AE_SERVICE_UNKNOWN + AE_SERVICE_CRITICAL;
			break;

		case SREPORT_TOP_HOST_ALERTS:
			display_type = REPORT_TOP_ALERTS;
			alert_types = AE_HOST_ALERT;
			host_states = AE_HOST_UP + AE_HOST_DOWN + AE_HOST_UNREACHABLE;
			break;

		case SREPORT_TOP_SERVICE_ALERTS:
			display_type = REPORT_TOP_ALERTS;
			alert_types = AE_SERVICE_ALERT;
			service_states = AE_SERVICE_OK + AE_SERVICE_WARNING + AE_SERVICE_UNKNOWN + AE_SERVICE_CRITICAL;
			break;

		default:
			break;
		}

	return;
	}



/* displays report */
void display_report(void) {

	switch(display_type) {

		case REPORT_ALERT_TOTALS:
			display_alert_totals();
			break;

		case REPORT_HOSTGROUP_ALERT_TOTALS:
			display_hostgroup_alert_totals();
			break;

		case REPORT_HOST_ALERT_TOTALS:
			display_host_alert_totals();
			break;

		case REPORT_SERVICEGROUP_ALERT_TOTALS:
			display_servicegroup_alert_totals();
			break;

		case REPORT_SERVICE_ALERT_TOTALS:
			display_service_alert_totals();
			break;

		case REPORT_TOP_ALERTS:
			display_top_alerts();
			break;

		default:
			display_recent_alerts();
			break;
		}

	return;
	}


/* displays recent alerts */
void display_recent_alerts(void) {
	archived_event *temp_event;
	int current_item = 0;
	int odd = 0;
	const char *bgclass = "";
	const char *status_bgclass = "";
	const char *status = "";
	char date_time[MAX_DATETIME_LENGTH];



	printf("<BR>\n");

	if(item_limit <= 0 || total_items <= item_limit || total_items == 0)
		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Displaying all %d matching alerts\n", total_items);
	else
		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Displaying most recent %d of %d total matching alerts\n", item_limit, total_items);

	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CLASS='data'>\n");
	printf("<TR><TH CLASS='data'>Time</TH><TH CLASS='data'>Alert Type</TH><TH CLASS='data'>Host</TH><TH CLASS='data'>Service</TH><TH CLASS='data'>State</TH><TH CLASS='data'>State Type</TH><TH CLASS='data'>Information</TH></TR>\n");


	for(temp_event = event_list; temp_event != NULL; temp_event = temp_event->next, current_item++) {

		if(current_item >= item_limit && item_limit > 0)
			break;

		if(odd) {
			odd = 0;
			bgclass = "Odd";
			}
		else {
			odd = 1;
			bgclass = "Even";
			}

		printf("<tr CLASS='data%s'>", bgclass);

		get_time_string(&temp_event->time_stamp, date_time, (int)sizeof(date_time), SHORT_DATE_TIME);
		printf("<td CLASS='data%s'>%s</td>", bgclass, date_time);

		printf("<td CLASS='data%s'>%s</td>", bgclass, (temp_event->event_type == AE_HOST_ALERT) ? "Host Alert" : "Service Alert");

		printf("<td CLASS='data%s'><a href='%s?type=%d&host=%s'>%s</a></td>", bgclass, EXTINFO_CGI, DISPLAY_HOST_INFO, url_encode(temp_event->host_name), temp_event->host_name);

		if(temp_event->event_type == AE_HOST_ALERT)
			printf("<td CLASS='data%s'>N/A</td>", bgclass);
		else {
			printf("<td CLASS='data%s'><a href='%s?type=%d&host=%s", bgclass, EXTINFO_CGI, DISPLAY_SERVICE_INFO, url_encode(temp_event->host_name));
			printf("&service=%s'>%s</a></td>", url_encode(temp_event->service_description), temp_event->service_description);
			}

		switch(temp_event->entry_type) {
			case AE_HOST_UP:
				status_bgclass = "hostUP";
				status = "UP";
				break;
			case AE_HOST_DOWN:
				status_bgclass = "hostDOWN";
				status = "DOWN";
				break;
			case AE_HOST_UNREACHABLE:
				status_bgclass = "hostUNREACHABLE";
				status = "UNREACHABLE";
				break;
			case AE_SERVICE_OK:
				status_bgclass = "serviceOK";
				status = "OK";
				break;
			case AE_SERVICE_WARNING:
				status_bgclass = "serviceWARNING";
				status = "WARNING";
				break;
			case AE_SERVICE_UNKNOWN:
				status_bgclass = "serviceUNKNOWN";
				status = "UNKNOWN";
				break;
			case AE_SERVICE_CRITICAL:
				status_bgclass = "serviceCRITICAL";
				status = "CRITICAL";
				break;
			default:
				status_bgclass = bgclass;
				status = "???";
				break;
			}

		printf("<td CLASS='%s'>%s</td>", status_bgclass, status);

		printf("<td CLASS='data%s'>%s</td>", bgclass, (temp_event->state_type == AE_SOFT_STATE) ? "SOFT" : "HARD");

		printf("<td CLASS='data%s'>%s</td>", bgclass, temp_event->event_info);

		printf("</tr>\n");
		}

	printf("</TABLE>\n");
	printf("</DIV>\n");

	return;
	}



/* displays alerts totals */
void display_alert_totals(void) {
	int hard_host_up_alerts = 0;
	int soft_host_up_alerts = 0;
	int hard_host_down_alerts = 0;
	int soft_host_down_alerts = 0;
	int hard_host_unreachable_alerts = 0;
	int soft_host_unreachable_alerts = 0;
	int hard_service_ok_alerts = 0;
	int soft_service_ok_alerts = 0;
	int hard_service_warning_alerts = 0;
	int soft_service_warning_alerts = 0;
	int hard_service_unknown_alerts = 0;
	int soft_service_unknown_alerts = 0;
	int hard_service_critical_alerts = 0;
	int soft_service_critical_alerts = 0;
	archived_event *temp_event;


	/************************/
	/**** OVERALL TOTALS ****/
	/************************/

	/* process all events */
	for(temp_event = event_list; temp_event != NULL; temp_event = temp_event->next) {

		/* host alerts */
		if(temp_event->event_type == AE_HOST_ALERT) {
			if(temp_event->state_type == AE_SOFT_STATE) {
				if(temp_event->entry_type == AE_HOST_UP)
					soft_host_up_alerts++;
				else if(temp_event->entry_type == AE_HOST_DOWN)
					soft_host_down_alerts++;
				else if(temp_event->entry_type == AE_HOST_UNREACHABLE)
					soft_host_unreachable_alerts++;
				}
			else {
				if(temp_event->entry_type == AE_HOST_UP)
					hard_host_up_alerts++;
				else if(temp_event->entry_type == AE_HOST_DOWN)
					hard_host_down_alerts++;
				else if(temp_event->entry_type == AE_HOST_UNREACHABLE)
					hard_host_unreachable_alerts++;
				}
			}

		/* service alerts */
		else {
			if(temp_event->state_type == AE_SOFT_STATE) {
				if(temp_event->entry_type == AE_SERVICE_OK)
					soft_service_ok_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_WARNING)
					soft_service_warning_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_UNKNOWN)
					soft_service_unknown_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_CRITICAL)
					soft_service_critical_alerts++;
				}
			else {
				if(temp_event->entry_type == AE_SERVICE_OK)
					hard_service_ok_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_WARNING)
					hard_service_warning_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_UNKNOWN)
					hard_service_unknown_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_CRITICAL)
					hard_service_critical_alerts++;
				}
			}
		}

	printf("<BR>\n");

	printf("<DIV ALIGN=CENTER>\n");
	printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Overall Totals</DIV>\n");
	printf("<BR>\n");
	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 CLASS='reportDataOdd'><TR><TD>\n");
	printf("<TABLE BORDER=0>\n");
	printf("<TR>\n");

	if(alert_types & AE_HOST_ALERT) {

		printf("<TD ALIGN=CENTER VALIGN=TOP>\n");

		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Host Alerts</DIV>\n");

		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>State</TH><TH CLASS='data'>Soft Alerts</TH><TH CLASS='data'>Hard Alerts</TH><TH CLASS='data'>Total Alerts</TH></TR>\n");

		printf("<TR CLASS='dataOdd'><TD CLASS='hostUP'>UP</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_host_up_alerts, hard_host_up_alerts, soft_host_up_alerts + hard_host_up_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='hostDOWN'>DOWN</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_host_down_alerts, hard_host_down_alerts, soft_host_down_alerts + hard_host_down_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='hostUNREACHABLE'>UNREACHABLE</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_host_unreachable_alerts, hard_host_unreachable_alerts, soft_host_unreachable_alerts + hard_host_unreachable_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='dataEven'>All States</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'><B>%d</B></TD></TR>\n", soft_host_up_alerts + soft_host_down_alerts + soft_host_unreachable_alerts, hard_host_up_alerts + hard_host_down_alerts + hard_host_unreachable_alerts, soft_host_up_alerts + hard_host_up_alerts + soft_host_down_alerts + hard_host_down_alerts + soft_host_unreachable_alerts + hard_host_unreachable_alerts);

		printf("</TABLE>\n");
		printf("</DIV>\n");

		printf("</TD>\n");
		}

	if(alert_types & AE_SERVICE_ALERT) {

		printf("<TD ALIGN=CENTER VALIGN=TOP>\n");

		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Service Alerts</DIV>\n");

		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>State</TH><TH CLASS='data'>Soft Alerts</TH><TH CLASS='data'>Hard Alerts</TH><TH CLASS='data'>Total Alerts</TH></TR>\n");

		printf("<TR CLASS='dataOdd'><TD CLASS='serviceOK'>OK</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_service_ok_alerts, hard_service_ok_alerts, soft_service_ok_alerts + hard_service_ok_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='serviceWARNING'>WARNING</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_service_warning_alerts, hard_service_warning_alerts, soft_service_warning_alerts + hard_service_warning_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='serviceUNKNOWN'>UNKNOWN</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_service_unknown_alerts, hard_service_unknown_alerts, soft_service_unknown_alerts + hard_service_unknown_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='serviceCRITICAL'>CRITICAL</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_service_critical_alerts, hard_service_critical_alerts, soft_service_critical_alerts + hard_service_critical_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='dataOdd'>All States</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'><B>%d</B></TD></TR>\n", soft_service_ok_alerts + soft_service_warning_alerts + soft_service_unknown_alerts + soft_service_critical_alerts, hard_service_ok_alerts + hard_service_warning_alerts + hard_service_unknown_alerts + hard_service_critical_alerts, soft_service_ok_alerts + soft_service_warning_alerts + soft_service_unknown_alerts + soft_service_critical_alerts + hard_service_ok_alerts + hard_service_warning_alerts + hard_service_unknown_alerts + hard_service_critical_alerts);

		printf("</TABLE>\n");
		printf("</DIV>\n");

		printf("</TD>\n");
		}

	printf("</TR>\n");
	printf("</TABLE>\n");
	printf("</TD></TR></TABLE>\n");
	printf("</DIV>\n");

	return;
	}



/* displays hostgroup alert totals  */
void display_hostgroup_alert_totals(void) {
	hostgroup *temp_hostgroup;

	/**************************/
	/**** HOSTGROUP TOTALS ****/
	/**************************/

	printf("<BR>\n");

	printf("<DIV ALIGN=CENTER>\n");
	printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Totals By Hostgroup</DIV>\n");

	if(show_all_hostgroups == FALSE)
		display_specific_hostgroup_alert_totals(target_hostgroup);
	else {
		for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next)
			display_specific_hostgroup_alert_totals(temp_hostgroup);
		}

	printf("</DIV>\n");

	return;
	}


/* displays alert totals for a specific hostgroup */
void display_specific_hostgroup_alert_totals(hostgroup *grp) {
	int hard_host_up_alerts = 0;
	int soft_host_up_alerts = 0;
	int hard_host_down_alerts = 0;
	int soft_host_down_alerts = 0;
	int hard_host_unreachable_alerts = 0;
	int soft_host_unreachable_alerts = 0;
	int hard_service_ok_alerts = 0;
	int soft_service_ok_alerts = 0;
	int hard_service_warning_alerts = 0;
	int soft_service_warning_alerts = 0;
	int hard_service_unknown_alerts = 0;
	int soft_service_unknown_alerts = 0;
	int hard_service_critical_alerts = 0;
	int soft_service_critical_alerts = 0;
	archived_event *temp_event;
	host *temp_host;

	if(grp == NULL)
		return;

	/* make sure the user is authorized to view this hostgroup */
	if(is_authorized_for_hostgroup(grp, &current_authdata) == FALSE)
		return;

	/* process all events */
	for(temp_event = event_list; temp_event != NULL; temp_event = temp_event->next) {

		temp_host = find_host(temp_event->host_name);
		if(is_host_member_of_hostgroup(grp, temp_host) == FALSE)
			continue;

		/* host alerts */
		if(temp_event->event_type == AE_HOST_ALERT) {
			if(temp_event->state_type == AE_SOFT_STATE) {
				if(temp_event->entry_type == AE_HOST_UP)
					soft_host_up_alerts++;
				else if(temp_event->entry_type == AE_HOST_DOWN)
					soft_host_down_alerts++;
				else if(temp_event->entry_type == AE_HOST_UNREACHABLE)
					soft_host_unreachable_alerts++;
				}
			else {
				if(temp_event->entry_type == AE_HOST_UP)
					hard_host_up_alerts++;
				else if(temp_event->entry_type == AE_HOST_DOWN)
					hard_host_down_alerts++;
				else if(temp_event->entry_type == AE_HOST_UNREACHABLE)
					hard_host_unreachable_alerts++;
				}
			}

		/* service alerts */
		else {
			if(temp_event->state_type == AE_SOFT_STATE) {
				if(temp_event->entry_type == AE_SERVICE_OK)
					soft_service_ok_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_WARNING)
					soft_service_warning_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_UNKNOWN)
					soft_service_unknown_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_CRITICAL)
					soft_service_critical_alerts++;
				}
			else {
				if(temp_event->entry_type == AE_SERVICE_OK)
					hard_service_ok_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_WARNING)
					hard_service_warning_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_UNKNOWN)
					hard_service_unknown_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_CRITICAL)
					hard_service_critical_alerts++;
				}
			}
		}


	printf("<BR>\n");
	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 CLASS='reportDataEven'><TR><TD>\n");
	printf("<TABLE BORDER=0>\n");

	printf("<TR><TD COLSPAN=2 ALIGN=CENTER CLASS='dataSubTitle'>Hostgroup '%s' (%s)</TD></TR>\n", grp->group_name, grp->alias);

	printf("<TR>\n");

	if(alert_types & AE_HOST_ALERT) {

		printf("<TD ALIGN=CENTER VALIGN=TOP>\n");

		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Host Alerts</DIV>\n");

		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>State</TH><TH CLASS='data'>Soft Alerts</TH><TH CLASS='data'>Hard Alerts</TH><TH CLASS='data'>Total Alerts</TH></TR>\n");

		printf("<TR CLASS='dataOdd'><TD CLASS='hostUP'>UP</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_host_up_alerts, hard_host_up_alerts, soft_host_up_alerts + hard_host_up_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='hostDOWN'>DOWN</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_host_down_alerts, hard_host_down_alerts, soft_host_down_alerts + hard_host_down_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='hostUNREACHABLE'>UNREACHABLE</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_host_unreachable_alerts, hard_host_unreachable_alerts, soft_host_unreachable_alerts + hard_host_unreachable_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='dataEven'>All States</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'><B>%d</B></TD></TR>\n", soft_host_up_alerts + soft_host_down_alerts + soft_host_unreachable_alerts, hard_host_up_alerts + hard_host_down_alerts + hard_host_unreachable_alerts, soft_host_up_alerts + hard_host_up_alerts + soft_host_down_alerts + hard_host_down_alerts + soft_host_unreachable_alerts + hard_host_unreachable_alerts);

		printf("</TABLE>\n");
		printf("</DIV>\n");

		printf("</TD>\n");
		}

	if(alert_types & AE_SERVICE_ALERT) {

		printf("<TD ALIGN=CENTER VALIGN=TOP>\n");

		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Service Alerts</DIV>\n");

		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>State</TH><TH CLASS='data'>Soft Alerts</TH><TH CLASS='data'>Hard Alerts</TH><TH CLASS='data'>Total Alerts</TH></TR>\n");

		printf("<TR CLASS='dataOdd'><TD CLASS='serviceOK'>OK</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_service_ok_alerts, hard_service_ok_alerts, soft_service_ok_alerts + hard_service_ok_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='serviceWARNING'>WARNING</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_service_warning_alerts, hard_service_warning_alerts, soft_service_warning_alerts + hard_service_warning_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='serviceUNKNOWN'>UNKNOWN</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_service_unknown_alerts, hard_service_unknown_alerts, soft_service_unknown_alerts + hard_service_unknown_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='serviceCRITICAL'>CRITICAL</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_service_critical_alerts, hard_service_critical_alerts, soft_service_critical_alerts + hard_service_critical_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='dataOdd'>All States</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'><B>%d</B></TD></TR>\n", soft_service_ok_alerts + soft_service_warning_alerts + soft_service_unknown_alerts + soft_service_critical_alerts, hard_service_ok_alerts + hard_service_warning_alerts + hard_service_unknown_alerts + hard_service_critical_alerts, soft_service_ok_alerts + soft_service_warning_alerts + soft_service_unknown_alerts + soft_service_critical_alerts + hard_service_ok_alerts + hard_service_warning_alerts + hard_service_unknown_alerts + hard_service_critical_alerts);

		printf("</TABLE>\n");
		printf("</DIV>\n");

		printf("</TD>\n");
		}

	printf("</TR>\n");

	printf("</TABLE>\n");
	printf("</TD></TR></TABLE>\n");

	return;
	}



/* displays host alert totals  */
void display_host_alert_totals(void) {
	host *temp_host;

	/*********************/
	/**** HOST TOTALS ****/
	/*********************/

	printf("<BR>\n");

	printf("<DIV ALIGN=CENTER>\n");
	printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Totals By Host</DIV>\n");

	if(show_all_hosts == FALSE)
		display_specific_host_alert_totals(target_host);
	else {
		for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next)
			display_specific_host_alert_totals(temp_host);
		}

	printf("</DIV>\n");

	return;
	}


/* displays alert totals for a specific host */
void display_specific_host_alert_totals(host *hst) {
	int hard_host_up_alerts = 0;
	int soft_host_up_alerts = 0;
	int hard_host_down_alerts = 0;
	int soft_host_down_alerts = 0;
	int hard_host_unreachable_alerts = 0;
	int soft_host_unreachable_alerts = 0;
	int hard_service_ok_alerts = 0;
	int soft_service_ok_alerts = 0;
	int hard_service_warning_alerts = 0;
	int soft_service_warning_alerts = 0;
	int hard_service_unknown_alerts = 0;
	int soft_service_unknown_alerts = 0;
	int hard_service_critical_alerts = 0;
	int soft_service_critical_alerts = 0;
	archived_event *temp_event;

	if(hst == NULL)
		return;

	/* make sure the user is authorized to view this host */
	if(is_authorized_for_host(hst, &current_authdata) == FALSE)
		return;

	if(show_all_hostgroups == FALSE && target_hostgroup != NULL) {
		if(is_host_member_of_hostgroup(target_hostgroup, hst) == FALSE)
			return;
		}

	/* process all events */
	for(temp_event = event_list; temp_event != NULL; temp_event = temp_event->next) {

		if(strcmp(temp_event->host_name, hst->name))
			continue;

		/* host alerts */
		if(temp_event->event_type == AE_HOST_ALERT) {
			if(temp_event->state_type == AE_SOFT_STATE) {
				if(temp_event->entry_type == AE_HOST_UP)
					soft_host_up_alerts++;
				else if(temp_event->entry_type == AE_HOST_DOWN)
					soft_host_down_alerts++;
				else if(temp_event->entry_type == AE_HOST_UNREACHABLE)
					soft_host_unreachable_alerts++;
				}
			else {
				if(temp_event->entry_type == AE_HOST_UP)
					hard_host_up_alerts++;
				else if(temp_event->entry_type == AE_HOST_DOWN)
					hard_host_down_alerts++;
				else if(temp_event->entry_type == AE_HOST_UNREACHABLE)
					hard_host_unreachable_alerts++;
				}
			}

		/* service alerts */
		else {
			if(temp_event->state_type == AE_SOFT_STATE) {
				if(temp_event->entry_type == AE_SERVICE_OK)
					soft_service_ok_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_WARNING)
					soft_service_warning_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_UNKNOWN)
					soft_service_unknown_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_CRITICAL)
					soft_service_critical_alerts++;
				}
			else {
				if(temp_event->entry_type == AE_SERVICE_OK)
					hard_service_ok_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_WARNING)
					hard_service_warning_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_UNKNOWN)
					hard_service_unknown_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_CRITICAL)
					hard_service_critical_alerts++;
				}
			}
		}


	printf("<BR>\n");
	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 CLASS='reportDataEven'><TR><TD>\n");
	printf("<TABLE BORDER=0>\n");

	printf("<TR><TD COLSPAN=2 ALIGN=CENTER CLASS='dataSubTitle'>Host '%s' (%s)</TD></TR>\n", hst->name, hst->alias);

	printf("<TR>\n");

	if(alert_types & AE_HOST_ALERT) {

		printf("<TD ALIGN=CENTER VALIGN=TOP>\n");

		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Host Alerts</DIV>\n");

		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>State</TH><TH CLASS='data'>Soft Alerts</TH><TH CLASS='data'>Hard Alerts</TH><TH CLASS='data'>Total Alerts</TH></TR>\n");

		printf("<TR CLASS='dataOdd'><TD CLASS='hostUP'>UP</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_host_up_alerts, hard_host_up_alerts, soft_host_up_alerts + hard_host_up_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='hostDOWN'>DOWN</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_host_down_alerts, hard_host_down_alerts, soft_host_down_alerts + hard_host_down_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='hostUNREACHABLE'>UNREACHABLE</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_host_unreachable_alerts, hard_host_unreachable_alerts, soft_host_unreachable_alerts + hard_host_unreachable_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='dataEven'>All States</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'><B>%d</B></TD></TR>\n", soft_host_up_alerts + soft_host_down_alerts + soft_host_unreachable_alerts, hard_host_up_alerts + hard_host_down_alerts + hard_host_unreachable_alerts, soft_host_up_alerts + hard_host_up_alerts + soft_host_down_alerts + hard_host_down_alerts + soft_host_unreachable_alerts + hard_host_unreachable_alerts);

		printf("</TABLE>\n");
		printf("</DIV>\n");

		printf("</TD>\n");
		}

	if(alert_types & AE_SERVICE_ALERT) {

		printf("<TD ALIGN=CENTER VALIGN=TOP>\n");

		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Service Alerts</DIV>\n");

		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>State</TH><TH CLASS='data'>Soft Alerts</TH><TH CLASS='data'>Hard Alerts</TH><TH CLASS='data'>Total Alerts</TH></TR>\n");

		printf("<TR CLASS='dataOdd'><TD CLASS='serviceOK'>OK</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_service_ok_alerts, hard_service_ok_alerts, soft_service_ok_alerts + hard_service_ok_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='serviceWARNING'>WARNING</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_service_warning_alerts, hard_service_warning_alerts, soft_service_warning_alerts + hard_service_warning_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='serviceUNKNOWN'>UNKNOWN</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_service_unknown_alerts, hard_service_unknown_alerts, soft_service_unknown_alerts + hard_service_unknown_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='serviceCRITICAL'>CRITICAL</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_service_critical_alerts, hard_service_critical_alerts, soft_service_critical_alerts + hard_service_critical_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='dataOdd'>All States</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'><B>%d</B></TD></TR>\n", soft_service_ok_alerts + soft_service_warning_alerts + soft_service_unknown_alerts + soft_service_critical_alerts, hard_service_ok_alerts + hard_service_warning_alerts + hard_service_unknown_alerts + hard_service_critical_alerts, soft_service_ok_alerts + soft_service_warning_alerts + soft_service_unknown_alerts + soft_service_critical_alerts + hard_service_ok_alerts + hard_service_warning_alerts + hard_service_unknown_alerts + hard_service_critical_alerts);

		printf("</TABLE>\n");
		printf("</DIV>\n");

		printf("</TD>\n");
		}

	printf("</TR>\n");

	printf("</TABLE>\n");
	printf("</TD></TR></TABLE>\n");

	return;
	}


/* displays servicegroup alert totals  */
void display_servicegroup_alert_totals(void) {
	servicegroup *temp_servicegroup;

	/**************************/
	/**** SERVICEGROUP TOTALS ****/
	/**************************/

	printf("<BR>\n");

	printf("<DIV ALIGN=CENTER>\n");
	printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Totals By Servicegroup</DIV>\n");

	if(show_all_servicegroups == FALSE)
		display_specific_servicegroup_alert_totals(target_servicegroup);
	else {
		for(temp_servicegroup = servicegroup_list; temp_servicegroup != NULL; temp_servicegroup = temp_servicegroup->next)
			display_specific_servicegroup_alert_totals(temp_servicegroup);
		}

	printf("</DIV>\n");

	return;
	}


/* displays alert totals for a specific servicegroup */
void display_specific_servicegroup_alert_totals(servicegroup *grp) {
	int hard_host_up_alerts = 0;
	int soft_host_up_alerts = 0;
	int hard_host_down_alerts = 0;
	int soft_host_down_alerts = 0;
	int hard_host_unreachable_alerts = 0;
	int soft_host_unreachable_alerts = 0;
	int hard_service_ok_alerts = 0;
	int soft_service_ok_alerts = 0;
	int hard_service_warning_alerts = 0;
	int soft_service_warning_alerts = 0;
	int hard_service_unknown_alerts = 0;
	int soft_service_unknown_alerts = 0;
	int hard_service_critical_alerts = 0;
	int soft_service_critical_alerts = 0;
	archived_event *temp_event;
	host *temp_host;
	service *temp_service;

	if(grp == NULL)
		return;

	/* make sure the user is authorized to view this servicegroup */
	if(is_authorized_for_servicegroup(grp, &current_authdata) == FALSE)
		return;

	/* process all events */
	for(temp_event = event_list; temp_event != NULL; temp_event = temp_event->next) {

		if(temp_event->event_type == AE_HOST_ALERT) {

			temp_host = find_host(temp_event->host_name);
			if(is_host_member_of_servicegroup(grp, temp_host) == FALSE)
				continue;
			}
		else {

			temp_service = find_service(temp_event->host_name, temp_event->service_description);
			if(is_service_member_of_servicegroup(grp, temp_service) == FALSE)
				continue;
			}

		/* host alerts */
		if(temp_event->event_type == AE_HOST_ALERT) {
			if(temp_event->state_type == AE_SOFT_STATE) {
				if(temp_event->entry_type == AE_HOST_UP)
					soft_host_up_alerts++;
				else if(temp_event->entry_type == AE_HOST_DOWN)
					soft_host_down_alerts++;
				else if(temp_event->entry_type == AE_HOST_UNREACHABLE)
					soft_host_unreachable_alerts++;
				}
			else {
				if(temp_event->entry_type == AE_HOST_UP)
					hard_host_up_alerts++;
				else if(temp_event->entry_type == AE_HOST_DOWN)
					hard_host_down_alerts++;
				else if(temp_event->entry_type == AE_HOST_UNREACHABLE)
					hard_host_unreachable_alerts++;
				}
			}

		/* service alerts */
		else {
			if(temp_event->state_type == AE_SOFT_STATE) {
				if(temp_event->entry_type == AE_SERVICE_OK)
					soft_service_ok_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_WARNING)
					soft_service_warning_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_UNKNOWN)
					soft_service_unknown_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_CRITICAL)
					soft_service_critical_alerts++;
				}
			else {
				if(temp_event->entry_type == AE_SERVICE_OK)
					hard_service_ok_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_WARNING)
					hard_service_warning_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_UNKNOWN)
					hard_service_unknown_alerts++;
				else if(temp_event->entry_type == AE_SERVICE_CRITICAL)
					hard_service_critical_alerts++;
				}
			}
		}


	printf("<BR>\n");
	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 CLASS='reportDataEven'><TR><TD>\n");
	printf("<TABLE BORDER=0>\n");

	printf("<TR><TD COLSPAN=2 ALIGN=CENTER CLASS='dataSubTitle'>Servicegroup '%s' (%s)</TD></TR>\n", grp->group_name, grp->alias);

	printf("<TR>\n");

	if(alert_types & AE_HOST_ALERT) {

		printf("<TD ALIGN=CENTER VALIGN=TOP>\n");

		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Host Alerts</DIV>\n");

		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>State</TH><TH CLASS='data'>Soft Alerts</TH><TH CLASS='data'>Hard Alerts</TH><TH CLASS='data'>Total Alerts</TH></TR>\n");

		printf("<TR CLASS='dataOdd'><TD CLASS='hostUP'>UP</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_host_up_alerts, hard_host_up_alerts, soft_host_up_alerts + hard_host_up_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='hostDOWN'>DOWN</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_host_down_alerts, hard_host_down_alerts, soft_host_down_alerts + hard_host_down_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='hostUNREACHABLE'>UNREACHABLE</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_host_unreachable_alerts, hard_host_unreachable_alerts, soft_host_unreachable_alerts + hard_host_unreachable_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='dataEven'>All States</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'><B>%d</B></TD></TR>\n", soft_host_up_alerts + soft_host_down_alerts + soft_host_unreachable_alerts, hard_host_up_alerts + hard_host_down_alerts + hard_host_unreachable_alerts, soft_host_up_alerts + hard_host_up_alerts + soft_host_down_alerts + hard_host_down_alerts + soft_host_unreachable_alerts + hard_host_unreachable_alerts);

		printf("</TABLE>\n");
		printf("</DIV>\n");

		printf("</TD>\n");
		}

	if(alert_types & AE_SERVICE_ALERT) {

		printf("<TD ALIGN=CENTER VALIGN=TOP>\n");

		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Service Alerts</DIV>\n");

		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>State</TH><TH CLASS='data'>Soft Alerts</TH><TH CLASS='data'>Hard Alerts</TH><TH CLASS='data'>Total Alerts</TH></TR>\n");

		printf("<TR CLASS='dataOdd'><TD CLASS='serviceOK'>OK</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_service_ok_alerts, hard_service_ok_alerts, soft_service_ok_alerts + hard_service_ok_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='serviceWARNING'>WARNING</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_service_warning_alerts, hard_service_warning_alerts, soft_service_warning_alerts + hard_service_warning_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='serviceUNKNOWN'>UNKNOWN</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_service_unknown_alerts, hard_service_unknown_alerts, soft_service_unknown_alerts + hard_service_unknown_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='serviceCRITICAL'>CRITICAL</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_service_critical_alerts, hard_service_critical_alerts, soft_service_critical_alerts + hard_service_critical_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='dataOdd'>All States</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'><B>%d</B></TD></TR>\n", soft_service_ok_alerts + soft_service_warning_alerts + soft_service_unknown_alerts + soft_service_critical_alerts, hard_service_ok_alerts + hard_service_warning_alerts + hard_service_unknown_alerts + hard_service_critical_alerts, soft_service_ok_alerts + soft_service_warning_alerts + soft_service_unknown_alerts + soft_service_critical_alerts + hard_service_ok_alerts + hard_service_warning_alerts + hard_service_unknown_alerts + hard_service_critical_alerts);

		printf("</TABLE>\n");
		printf("</DIV>\n");

		printf("</TD>\n");
		}

	printf("</TR>\n");

	printf("</TABLE>\n");
	printf("</TD></TR></TABLE>\n");

	return;
	}



/* displays service alert totals  */
void display_service_alert_totals(void) {
	service *temp_service;

	/************************/
	/**** SERVICE TOTALS ****/
	/************************/

	printf("<BR>\n");

	printf("<DIV ALIGN=CENTER>\n");
	printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Totals By Service</DIV>\n");

	for(temp_service = service_list; temp_service != NULL; temp_service = temp_service->next)
		display_specific_service_alert_totals(temp_service);

	printf("</DIV>\n");

	return;
	}


/* displays alert totals for a specific service */
void display_specific_service_alert_totals(service *svc) {
	int hard_service_ok_alerts = 0;
	int soft_service_ok_alerts = 0;
	int hard_service_warning_alerts = 0;
	int soft_service_warning_alerts = 0;
	int hard_service_unknown_alerts = 0;
	int soft_service_unknown_alerts = 0;
	int hard_service_critical_alerts = 0;
	int soft_service_critical_alerts = 0;
	archived_event *temp_event;
	host *temp_host;

	if(svc == NULL)
		return;

	/* make sure the user is authorized to view this service */
	if(is_authorized_for_service(svc, &current_authdata) == FALSE)
		return;

	if(show_all_hostgroups == FALSE && target_hostgroup != NULL) {
		temp_host = find_host(svc->host_name);
		if(is_host_member_of_hostgroup(target_hostgroup, temp_host) == FALSE)
			return;
		}

	if(show_all_hosts == FALSE && target_host != NULL) {
		if(strcmp(target_host->name, svc->host_name))
			return;
		}

	/* process all events */
	for(temp_event = event_list; temp_event != NULL; temp_event = temp_event->next) {

		if(temp_event->event_type != AE_SERVICE_ALERT)
			continue;

		if(strcmp(temp_event->host_name, svc->host_name) || strcmp(temp_event->service_description, svc->description))
			continue;

		/* service alerts */
		if(temp_event->state_type == AE_SOFT_STATE) {
			if(temp_event->entry_type == AE_SERVICE_OK)
				soft_service_ok_alerts++;
			else if(temp_event->entry_type == AE_SERVICE_WARNING)
				soft_service_warning_alerts++;
			else if(temp_event->entry_type == AE_SERVICE_UNKNOWN)
				soft_service_unknown_alerts++;
			else if(temp_event->entry_type == AE_SERVICE_CRITICAL)
				soft_service_critical_alerts++;
			}
		else {
			if(temp_event->entry_type == AE_SERVICE_OK)
				hard_service_ok_alerts++;
			else if(temp_event->entry_type == AE_SERVICE_WARNING)
				hard_service_warning_alerts++;
			else if(temp_event->entry_type == AE_SERVICE_UNKNOWN)
				hard_service_unknown_alerts++;
			else if(temp_event->entry_type == AE_SERVICE_CRITICAL)
				hard_service_critical_alerts++;
			}
		}


	printf("<BR>\n");
	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 CLASS='reportDataEven'><TR><TD>\n");
	printf("<TABLE BORDER=0>\n");

	printf("<TR><TD COLSPAN=2 ALIGN=CENTER CLASS='dataSubTitle'>Service '%s' on Host '%s'</TD></TR>\n", svc->description, svc->host_name);

	printf("<TR>\n");

	if(alert_types & AE_SERVICE_ALERT) {

		printf("<TD ALIGN=CENTER VALIGN=TOP>\n");

		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Service Alerts</DIV>\n");

		printf("<DIV ALIGN=CENTER>\n");
		printf("<TABLE BORDER=0 CLASS='data'>\n");
		printf("<TR><TH CLASS='data'>State</TH><TH CLASS='data'>Soft Alerts</TH><TH CLASS='data'>Hard Alerts</TH><TH CLASS='data'>Total Alerts</TH></TR>\n");

		printf("<TR CLASS='dataOdd'><TD CLASS='serviceOK'>OK</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_service_ok_alerts, hard_service_ok_alerts, soft_service_ok_alerts + hard_service_ok_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='serviceWARNING'>WARNING</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_service_warning_alerts, hard_service_warning_alerts, soft_service_warning_alerts + hard_service_warning_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='serviceUNKNOWN'>UNKNOWN</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD></TR>\n", soft_service_unknown_alerts, hard_service_unknown_alerts, soft_service_unknown_alerts + hard_service_unknown_alerts);
		printf("<TR CLASS='dataEven'><TD CLASS='serviceCRITICAL'>CRITICAL</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD><TD CLASS='dataEven'>%d</TD></TR>\n", soft_service_critical_alerts, hard_service_critical_alerts, soft_service_critical_alerts + hard_service_critical_alerts);
		printf("<TR CLASS='dataOdd'><TD CLASS='dataOdd'>All States</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'>%d</TD><TD CLASS='dataOdd'><B>%d</B></TD></TR>\n", soft_service_ok_alerts + soft_service_warning_alerts + soft_service_unknown_alerts + soft_service_critical_alerts, hard_service_ok_alerts + hard_service_warning_alerts + hard_service_unknown_alerts + hard_service_critical_alerts, soft_service_ok_alerts + soft_service_warning_alerts + soft_service_unknown_alerts + soft_service_critical_alerts + hard_service_ok_alerts + hard_service_warning_alerts + hard_service_unknown_alerts + hard_service_critical_alerts);

		printf("</TABLE>\n");
		printf("</DIV>\n");

		printf("</TD>\n");
		}

	printf("</TR>\n");

	printf("</TABLE>\n");
	printf("</TD></TR></TABLE>\n");

	return;
	}


/* find a specific alert producer */
alert_producer *find_producer(int type, char *hname, char *sdesc) {
	alert_producer *temp_producer;

	for(temp_producer = producer_list; temp_producer != NULL; temp_producer = temp_producer->next) {

		if(temp_producer->producer_type != type)
			continue;
		if(hname != NULL && strcmp(hname, temp_producer->host_name))
			continue;
		if(sdesc != NULL && strcmp(sdesc, temp_producer->service_description))
			continue;

		return temp_producer;
		}

	return NULL;
	}


/* adds a new producer to the list in memory */
alert_producer *add_producer(int producer_type, char *host_name, char *service_description) {
	alert_producer *new_producer = NULL;

	/* allocate memory for the new entry */
	new_producer = (alert_producer *)malloc(sizeof(alert_producer));
	if(new_producer == NULL)
		return NULL;

	/* allocate memory for the host name */
	if(host_name != NULL) {
		new_producer->host_name = (char *)malloc(strlen(host_name) + 1);
		if(new_producer->host_name != NULL)
			strcpy(new_producer->host_name, host_name);
		}
	else
		new_producer->host_name = NULL;

	/* allocate memory for the service description */
	if(service_description != NULL) {
		new_producer->service_description = (char *)malloc(strlen(service_description) + 1);
		if(new_producer->service_description != NULL)
			strcpy(new_producer->service_description, service_description);
		}
	else
		new_producer->service_description = NULL;

	new_producer->producer_type = producer_type;
	new_producer->total_alerts = 0;

	/* add the new entry to the list in memory, sorted by time */
	new_producer->next = producer_list;
	producer_list = new_producer;

	return new_producer;
	}



void free_producer_list(void) {
	alert_producer *this_producer = NULL;
	alert_producer *next_producer = NULL;

	for(this_producer = producer_list; this_producer != NULL;) {
		next_producer = this_producer->next;
		if(this_producer->host_name != NULL)
			free(this_producer->host_name);
		if(this_producer->service_description != NULL)
			free(this_producer->service_description);
		free(this_producer);
		this_producer = next_producer;
		}

	producer_list = NULL;

	return;
	}



/* displays top alerts */
void display_top_alerts(void) {
	archived_event *temp_event = NULL;
	alert_producer *temp_producer = NULL;
	alert_producer *next_producer = NULL;
	alert_producer *last_producer = NULL;
	alert_producer *new_producer = NULL;
	alert_producer *temp_list = NULL;
	int producer_type = AE_HOST_PRODUCER;
	int current_item = 0;
	int odd = 0;
	const char *bgclass = "";

	/* process all events */
	for(temp_event = event_list; temp_event != NULL; temp_event = temp_event->next) {

		producer_type = (temp_event->event_type == AE_HOST_ALERT) ? AE_HOST_PRODUCER : AE_SERVICE_PRODUCER;

		/* see if we already have a record for the producer */
		temp_producer = find_producer(producer_type, temp_event->host_name, temp_event->service_description);

		/* if not, add a record */
		if(temp_producer == NULL)
			temp_producer = add_producer(producer_type, temp_event->host_name, temp_event->service_description);

		/* producer record could not be added */
		if(temp_producer == NULL)
			continue;

		/* update stats for producer */
		temp_producer->total_alerts++;
		}


	/* sort the producer list by total alerts (descending) */
	total_items = 0;
	temp_list = NULL;
	for(new_producer = producer_list; new_producer != NULL;) {
		next_producer = new_producer->next;

		last_producer = temp_list;
		for(temp_producer = temp_list; temp_producer != NULL; temp_producer = temp_producer->next) {
			if(new_producer->total_alerts >= temp_producer->total_alerts) {
				new_producer->next = temp_producer;
				if(temp_producer == temp_list)
					temp_list = new_producer;
				else
					last_producer->next = new_producer;
				break;
				}
			else
				last_producer = temp_producer;
			}
		if(temp_list == NULL) {
			new_producer->next = NULL;
			temp_list = new_producer;
			}
		else if(temp_producer == NULL) {
			new_producer->next = NULL;
			last_producer->next = new_producer;
			}

		new_producer = next_producer;
		total_items++;
		}
	producer_list = temp_list;


	printf("<BR>\n");

	if(item_limit <= 0 || total_items <= item_limit || total_items == 0)
		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Displaying all %d matching alert producers\n", total_items);
	else
		printf("<DIV ALIGN=CENTER CLASS='dataSubTitle'>Displaying top %d of %d total matching alert producers\n", item_limit, total_items);

	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CLASS='data'>\n");
	printf("<TR><TH CLASS='data'>Rank</TH><TH CLASS='data'>Producer Type</TH><TH CLASS='data'>Host</TH><TH CLASS='data'>Service</TH><TH CLASS='data'>Total Alerts</TH></TR>\n");



	/* display top producers */
	for(temp_producer = producer_list; temp_producer != NULL; temp_producer = temp_producer->next) {

		if(current_item >= item_limit && item_limit > 0)
			break;

		current_item++;

		if(odd) {
			odd = 0;
			bgclass = "Odd";
			}
		else {
			odd = 1;
			bgclass = "Even";
			}

		printf("<tr CLASS='data%s'>", bgclass);

		printf("<td CLASS='data%s'>#%d</td>", bgclass, current_item);

		printf("<td CLASS='data%s'>%s</td>", bgclass, (temp_producer->producer_type == AE_HOST_PRODUCER) ? "Host" : "Service");

		printf("<td CLASS='data%s'><a href='%s?type=%d&host=%s'>%s</a></td>", bgclass, EXTINFO_CGI, DISPLAY_HOST_INFO, url_encode(temp_producer->host_name), temp_producer->host_name);

		if(temp_producer->producer_type == AE_HOST_PRODUCER)
			printf("<td CLASS='data%s'>N/A</td>", bgclass);
		else {
			printf("<td CLASS='data%s'><a href='%s?type=%d&host=%s", bgclass, EXTINFO_CGI, DISPLAY_SERVICE_INFO, url_encode(temp_producer->host_name));
			printf("&service=%s'>%s</a></td>", url_encode(temp_producer->service_description), temp_producer->service_description);
			}

		printf("<td CLASS='data%s'>%d</td>", bgclass, temp_producer->total_alerts);

		printf("</tr>\n");
		}

	printf("</TABLE>\n");
	printf("</DIV>\n");

	return;
	}
