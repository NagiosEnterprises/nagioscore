/**************************************************************************
 *
 * EXTINFO.C -  Nagios Extended Information CGI
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 12-04-2002
 *
 * License:
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "../common/config.h"
#include "../common/locations.h"
#include "../common/common.h"
#include "../common/objects.h"
#include "../common/comments.h"
#include "../common/downtime.h"
#include "../common/statusdata.h"

#include "cgiutils.h"
#include "getcgi.h"
#include "auth.h"
#include "edata.h"

extern char             nagios_check_command[MAX_INPUT_BUFFER];
extern char             nagios_process_info[MAX_INPUT_BUFFER];
extern int              nagios_process_state;
extern int              refresh_rate;

extern time_t		program_start;
extern int              nagios_pid;
extern int              daemon_mode;
extern time_t           last_command_check;
extern time_t           last_log_rotation;
extern int              enable_notifications;
extern int              execute_service_checks;
extern int              accept_passive_service_checks;
extern int              enable_event_handlers;
extern int              obsess_over_services;
extern int              enable_flap_detection;
extern int              enable_failure_prediction;
extern int              process_performance_data;

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_html_path[MAX_FILENAME_LENGTH];
extern char url_stylesheets_path[MAX_FILENAME_LENGTH];
extern char url_docs_path[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char url_logo_images_path[MAX_FILENAME_LENGTH];
extern char log_file[MAX_FILENAME_LENGTH];

extern comment           *comment_list;
extern scheduled_downtime  *scheduled_downtime_list;
extern hoststatus *hoststatus_list;
extern servicestatus *servicestatus_list;


#define MAX_MESSAGE_BUFFER		4096

#define HEALTH_WARNING_PERCENTAGE       85
#define HEALTH_CRITICAL_PERCENTAGE      75

/* SERVICESORT structure */
typedef struct servicesort_struct{
	servicestatus *svcstatus;
	struct servicesort_struct *next;
        }servicesort;

void document_header(int);
void document_footer(void);
int process_cgivars(void);

void show_process_info(void);
void show_host_info(void);
void show_service_info(void);
void show_all_comments(void);
void show_performance_data(void);
void show_hostgroup_info(void);
void show_all_downtime(void);
void show_scheduling_queue(void);
void display_comments(int);

int sort_services(int,int);
int compare_servicesort_entries(int,int,servicesort *,servicesort *);
void free_servicesort_list();

authdata current_authdata;

servicesort *servicesort_list=NULL;

char *host_name="";
char *hostgroup_name="";
char *service_desc="";

int display_type=DISPLAY_PROCESS_INFO;

int sort_type=SORT_ASCENDING;
int sort_option=SORT_NEXTCHECKTIME;

int embedded=FALSE;
int display_header=TRUE;



int main(void){
	int result=OK;
	char temp_buffer[MAX_INPUT_BUFFER];
	hostextinfo *temp_hostextinfo=NULL;
	serviceextinfo *temp_serviceextinfo=NULL;
	host *temp_host=NULL;
	hostgroup *temp_hostgroup=NULL;
	

	/* get the arguments passed in the URL */
	process_cgivars();

	/* reset internal variables */
	reset_cgi_vars();

	/* read the CGI configuration file */
	result=read_cgi_config_file(get_cgi_config_location());
	if(result==ERROR){
		document_header(FALSE);
		cgi_config_file_error(main_config_file);
		document_footer();
		return ERROR;
	        }

	/* read the main configuration file */
	result=read_main_config_file(main_config_file);
	if(result==ERROR){
		document_header(FALSE);
		main_config_file_error(main_config_file);
		document_footer();
		return ERROR;
	        }

	/* read all object configuration data */
	result=read_all_object_configuration_data(main_config_file,READ_ALL_OBJECT_DATA);
	if(result==ERROR){
		document_header(FALSE);
		object_data_error();
		document_footer();
		return ERROR;
                }

	/* read all status data */
	result=read_all_status_data(get_cgi_config_location(),READ_ALL_STATUS_DATA);
	if(result==ERROR){
		document_header(FALSE);
		status_data_error();
		document_footer();
		free_memory();
		return ERROR;
                }

	document_header(TRUE);

	/* read in extended host information */
	read_extended_object_config_data(get_cgi_config_location(),READ_ALL_EXTENDED_DATA);

	/* get authentication information */
	get_authentication_information(&current_authdata);


	if(display_header==TRUE){

		/* begin top table */
		printf("<table border=0 width=100%%>\n");
		printf("<tr>\n");

		/* left column of the first row */
		printf("<td align=left valign=top width=33%%>\n");

		if(display_type==DISPLAY_HOST_INFO)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Host Information");
		else if(display_type==DISPLAY_SERVICE_INFO)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Service Information");
		else if(display_type==DISPLAY_COMMENTS)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"All Host and Service Comments");
		else if(display_type==DISPLAY_PERFORMANCE)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Performance Information");
		else if(display_type==DISPLAY_HOSTGROUP_INFO)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Hostgroup Information");
		else if(display_type==DISPLAY_DOWNTIME)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"All Host and Service Scheduled Downtime");
		else if(display_type==DISPLAY_SCHEDULING_QUEUE)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Service Check Scheduling Queue");
		else
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Nagios Process Information");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		display_info_table(temp_buffer,TRUE,&current_authdata);

		/* find the host */
		if(display_type==DISPLAY_HOST_INFO || display_type==DISPLAY_SERVICE_INFO){

			temp_host=find_host(host_name);

			/* write some Javascript helper functions */
			if(temp_host!=NULL){
				printf("<SCRIPT LANGUAGE=\"JavaScript\">\n<!--\n");
				printf("function nagios_get_host_name()\n{\n");
				printf("return \"%s\";\n",temp_host->name);
				printf("}\n");
				printf("function nagios_get_host_address()\n{\n");
				printf("return \"%s\";\n",temp_host->address);
				printf("}\n");
				printf("//-->\n</SCRIPT>\n");
			        }
		        }

		/* find the hostgroup */
		else if(display_type==DISPLAY_HOSTGROUP_INFO)
			temp_hostgroup=find_hostgroup(hostgroup_name,NULL);

		if(((display_type==DISPLAY_HOST_INFO || display_type==DISPLAY_SERVICE_INFO) && temp_host!=NULL) || (display_type==DISPLAY_HOSTGROUP_INFO && temp_hostgroup!=NULL)){
			printf("<TABLE BORDER=1 CELLPADDING=0 CELLSPACING=0 CLASS='linkBox'>\n");
			printf("<TR><TD CLASS='linkBox'>\n");
			if(display_type==DISPLAY_SERVICE_INFO)
				printf("<A HREF='%s?type=%d&host=%s'>View Information For This Host</A><br>\n",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(host_name));
			if(display_type==DISPLAY_SERVICE_INFO || display_type==DISPLAY_HOST_INFO)
				printf("<A HREF='%s?host=%s'>View Status Detail For This Host</A><BR>\n",STATUS_CGI,url_encode(host_name));
			if(display_type==DISPLAY_HOST_INFO){
				printf("<A HREF='%s?host=%s'>View Alert History For This Host</A><BR>\n",HISTORY_CGI,url_encode(host_name));
#ifdef USE_TRENDS
				printf("<A HREF='%s?host=%s'>View Trends For This Host</A><BR>\n",TRENDS_CGI,url_encode(host_name));
#endif
#ifdef USE_HISTOGRAM
				printf("<A HREF='%s?host=%s'>View Alert Histogram For This Host</A><BR>\n",HISTOGRAM_CGI,url_encode(host_name));
#endif
				printf("<A HREF='%s?host=%s&show_log_entries'>View Availability Report For This Host</A><BR>\n",AVAIL_CGI,url_encode(host_name));
				printf("<A HREF='%s?host=%s'>View Notifications This Host</A>\n",NOTIFICATIONS_CGI,url_encode(host_name));
		                }
			else if(display_type==DISPLAY_SERVICE_INFO){
				printf("<A HREF='%s?host=%s&",HISTORY_CGI,url_encode(host_name));
				printf("service=%s'>View Alert History For This Service</A><BR>\n",url_encode(service_desc));
#ifdef USE_TRENDS
				printf("<A HREF='%s?host=%s&",TRENDS_CGI,url_encode(host_name));
				printf("service=%s'>View Trends For This Service</A><BR>\n",url_encode(service_desc));
#endif
#ifdef USE_HISTOGRAM
				printf("<A HREF='%s?host=%s&",HISTOGRAM_CGI,url_encode(host_name));
				printf("service=%s'>View Alert Histogram For This Service</A><BR>\n",url_encode(service_desc));
#endif
				printf("<A HREF='%s?host=%s&",AVAIL_CGI,url_encode(host_name));
				printf("service=%s&show_log_entries'>View Availability Report For This Service</A><BR>\n",url_encode(service_desc));
				printf("<A HREF='%s?host=%s&",NOTIFICATIONS_CGI,url_encode(host_name));
				printf("service=%s'>View Notifications This Service</A>\n",url_encode(service_desc));
		                }
			else if(display_type==DISPLAY_HOSTGROUP_INFO){
				printf("<A HREF='%s?hostgroup=%s&style=detail'>View Status Detail For This Hostgroup</A><BR>\n",STATUS_CGI,url_encode(hostgroup_name));
				printf("<A HREF='%s?hostgroup=%s&style=overview'>View Status Overview For This Hostgroup</A><BR>\n",STATUS_CGI,url_encode(hostgroup_name));
				printf("<A HREF='%s?hostgroup=%s&style=grid'>View Status Grid For This Hostgroup</A><BR>\n",STATUS_CGI,url_encode(hostgroup_name));
				printf("<A HREF='%s?hostgroup=%s'>View Availability For This Hostgroup</A><BR>\n",AVAIL_CGI,url_encode(hostgroup_name));
		                }
			printf("</TD></TR>\n");
			printf("</TABLE>\n");
	                }

		printf("</td>\n");

		/* middle column of top row */
		printf("<td align=center valign=center width=33%%>\n");

		if(((display_type==DISPLAY_HOST_INFO || display_type==DISPLAY_SERVICE_INFO) && temp_host!=NULL) || (display_type==DISPLAY_HOSTGROUP_INFO && temp_hostgroup!=NULL)){

			if(display_type==DISPLAY_SERVICE_INFO)
				printf("<DIV CLASS='data'>Service</DIV><DIV CLASS='dataTitle'>%s</DIV><DIV CLASS='data'>On Host</DIV>\n",service_desc);
			if(display_type==DISPLAY_HOST_INFO)
				printf("<DIV CLASS='data'>Host</DIV>\n");
			if(display_type==DISPLAY_SERVICE_INFO || display_type==DISPLAY_HOST_INFO){
				printf("<DIV CLASS='dataTitle'>%s</DIV>\n",temp_host->alias);
				printf("<DIV CLASS='dataTitle'>(%s)</DIV><BR>\n",temp_host->name);
				printf("<DIV CLASS='data'>%s</DIV>\n",temp_host->address);
			        }
			if(display_type==DISPLAY_HOSTGROUP_INFO){
				printf("<DIV CLASS='data'>Hostgroup</DIV>\n");
				printf("<DIV CLASS='dataTitle'>%s</DIV>\n",temp_hostgroup->alias);
				printf("<DIV CLASS='dataTitle'>(%s)</DIV>\n",temp_hostgroup->group_name);
			        }

			if(display_type==DISPLAY_SERVICE_INFO){
				temp_serviceextinfo=find_serviceextinfo(host_name,service_desc);
				if(temp_serviceextinfo!=NULL){
					if(temp_serviceextinfo->icon_image!=NULL)
						printf("<img src='%s%s' border=0 alt='%s'><BR CLEAR=ALL>",url_logo_images_path,temp_serviceextinfo->icon_image,(temp_serviceextinfo->icon_image_alt==NULL)?"":temp_serviceextinfo->icon_image_alt);
					if(temp_serviceextinfo->icon_image_alt!=NULL)
						printf("<font size=-1><i>( %s )</i><font>\n",temp_serviceextinfo->icon_image_alt);
				        }
			        }

			if(display_type==DISPLAY_HOST_INFO || temp_serviceextinfo==NULL){
				temp_hostextinfo=find_hostextinfo(host_name);
				if(temp_hostextinfo!=NULL){
					if(temp_hostextinfo->icon_image!=NULL)
						printf("<img src='%s%s' border=0 alt='%s'><BR CLEAR=ALL>",url_logo_images_path,temp_hostextinfo->icon_image,(temp_hostextinfo->icon_image_alt==NULL)?"":temp_hostextinfo->icon_image_alt);
					if(temp_hostextinfo->icon_image_alt!=NULL)
						printf("<font size=-1><i>( %s )</i><font>\n",temp_hostextinfo->icon_image_alt);
				        }
		                }
 	                }

		printf("</td>\n");

		/* right column of top row */
		printf("<td align=right valign=bottom width=33%%>\n");

		if(display_type==DISPLAY_HOST_INFO){
			if(temp_hostextinfo!=NULL && temp_hostextinfo->notes_url!=NULL && strcmp(temp_hostextinfo->notes_url,"")){
				printf("<A HREF='");
				print_host_notes_url(temp_hostextinfo);
				printf("' TARGET='_blank'><img src='%s%s' border=0 alt='View Additional Notes For This Host'></A>\n",url_images_path,NOTES_ICON);
				printf("<BR CLEAR=ALL><FONT SIZE=-1><I>There Are Additional<BR>Notes For This Host...</I></FONT><BR CLEAR=ALL><BR CLEAR=ALL>\n");
		                }
	                }

		else if(display_type==DISPLAY_SERVICE_INFO){
			if(temp_serviceextinfo!=NULL && temp_serviceextinfo->notes_url!=NULL && strcmp(temp_serviceextinfo->notes_url,"")){
				printf("<A HREF='");
				print_service_notes_url(temp_serviceextinfo);
				printf("' TARGET='_blank'><img src='%s%s' border=0 alt='View Additional Notes For This Service'></A>\n",url_images_path,NOTES_ICON);
				printf("<BR CLEAR=ALL><FONT SIZE=-1><I>There Are Additional<BR>Notes For This Service...</I></FONT><BR CLEAR=ALL><BR CLEAR=ALL>\n");
		                }
	                }

		/* display context-sensitive help */
		if(display_type==DISPLAY_HOST_INFO)
			display_context_help(CONTEXTHELP_EXT_HOST);
		else if(display_type==DISPLAY_SERVICE_INFO)
			display_context_help(CONTEXTHELP_EXT_SERVICE);
		else if(display_type==DISPLAY_HOSTGROUP_INFO)
			display_context_help(CONTEXTHELP_EXT_HOSTGROUP);
		else if(display_type==DISPLAY_PROCESS_INFO)
			display_context_help(CONTEXTHELP_EXT_PROCESS);
		else if(display_type==DISPLAY_PERFORMANCE)
			display_context_help(CONTEXTHELP_EXT_PERFORMANCE);
		else if(display_type==DISPLAY_COMMENTS)
			display_context_help(CONTEXTHELP_EXT_COMMENTS);
		else if(display_type==DISPLAY_DOWNTIME)
			display_context_help(CONTEXTHELP_EXT_DOWNTIME);
		else if(display_type==DISPLAY_SCHEDULING_QUEUE)
			display_context_help(CONTEXTHELP_EXT_QUEUE);

		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");

	        }

	printf("<BR>\n");

	if(display_type==DISPLAY_HOST_INFO)
		show_host_info();
	else if(display_type==DISPLAY_SERVICE_INFO)
		show_service_info();
	else if(display_type==DISPLAY_COMMENTS)
		show_all_comments();
	else if(display_type==DISPLAY_PERFORMANCE)
		show_performance_data();
	else if(display_type==DISPLAY_HOSTGROUP_INFO)
		show_hostgroup_info();
	else if(display_type==DISPLAY_DOWNTIME)
		show_all_downtime();
	else if(display_type==DISPLAY_SCHEDULING_QUEUE)
		show_scheduling_queue();
	else
		show_process_info();

	document_footer();

	/* free all allocated memory */
	free_memory();
	free_extended_data();
	free_comment_data();
	free_downtime_data();

	return OK;
        }



void document_header(int use_stylesheet){
	char date_time[MAX_DATETIME_LENGTH];
	time_t current_time;
	time_t expire_time;

	printf("Cache-Control: no-store\n");
	printf("Pragma: no-cache\n");
	printf("Refresh: %d\n",refresh_rate);

	time(&current_time);
	get_time_string(&current_time,date_time,(int)sizeof(date_time),HTTP_DATE_TIME);
	printf("Last-Modified: %s\n",date_time);

	expire_time=(time_t)0L;
	get_time_string(&expire_time,date_time,(int)sizeof(date_time),HTTP_DATE_TIME);
	printf("Expires: %s\n",date_time);

	printf("Content-type: text/html\r\n\r\n");

	if(embedded==TRUE)
		return;

	printf("<html>\n");
	printf("<head>\n");
	printf("<title>\n");
	printf("Extended Information\n");
	printf("</title>\n");

	if(use_stylesheet==TRUE)
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>",url_stylesheets_path,EXTINFO_CSS);

	printf("</head>\n");

	printf("<body CLASS='extinfo'>\n");

	/* include user SSI header */
	include_ssi_files(EXTINFO_CGI,SSI_HEADER);

	return;
        }


void document_footer(void){

	if(embedded==TRUE)
		return;

	/* include user SSI footer */
	include_ssi_files(EXTINFO_CGI,SSI_FOOTER);

	printf("</body>\n");
	printf("</html>\n");

	return;
        }


int process_cgivars(void){
	char **variables;
	int error=FALSE;
	int temp_type;
	int x;

	variables=getcgivars();

	for(x=0;variables[x]!=NULL;x++){

		/* do some basic length checking on the variable identifier to prevent buffer overflows */
		if(strlen(variables[x])>=MAX_INPUT_BUFFER-1){
			x++;
			continue;
		        }

		/* we found the display type */
		else if(!strcmp(variables[x],"type")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
				}
			temp_type=atoi(variables[x]);
			if(temp_type==DISPLAY_HOST_INFO)
				display_type=DISPLAY_HOST_INFO;
			else if(temp_type==DISPLAY_SERVICE_INFO)
				display_type=DISPLAY_SERVICE_INFO;
			else if(temp_type==DISPLAY_COMMENTS)
				display_type=DISPLAY_COMMENTS;
			else if(temp_type==DISPLAY_PERFORMANCE)
				display_type=DISPLAY_PERFORMANCE;
			else if(temp_type==DISPLAY_HOSTGROUP_INFO)
				display_type=DISPLAY_HOSTGROUP_INFO;
			else if(temp_type==DISPLAY_DOWNTIME)
				display_type=DISPLAY_DOWNTIME;
			else if(temp_type==DISPLAY_SCHEDULING_QUEUE)
				display_type=DISPLAY_SCHEDULING_QUEUE;
			else
				display_type=DISPLAY_PROCESS_INFO;
			}

		/* we found the host name */
		else if(!strcmp(variables[x],"host")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			host_name=strdup(variables[x]);
			if(host_name==NULL)
				host_name="";
			}

		/* we found the hostgroup name */
		else if(!strcmp(variables[x],"hostgroup")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			hostgroup_name=strdup(variables[x]);
			if(hostgroup_name==NULL)
				hostgroup_name="";
			}

		/* we found the service name */
		else if(!strcmp(variables[x],"service")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			service_desc=strdup(variables[x]);
			if(service_desc==NULL)
				service_desc="";
			}

		/* we found the sort type argument */
		else if(!strcmp(variables[x],"sorttype")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			sort_type=atoi(variables[x]);
		        }

		/* we found the sort option argument */
		else if(!strcmp(variables[x],"sortoption")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			sort_option=atoi(variables[x]);
		        }

		/* we found the embed option */
		else if(!strcmp(variables[x],"embedded"))
			embedded=TRUE;

		/* we found the noheader option */
		else if(!strcmp(variables[x],"noheader"))
			display_header=FALSE;
	        }

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return error;
        }



void show_process_info(void){
	char state_string[MAX_INPUT_BUFFER];
	char *state_class="";
	char date_time[MAX_DATETIME_LENGTH];
	time_t current_time;
	unsigned long run_time;
	char run_time_string[24];
	int days=0;
	int hours=0;
	int minutes=0;
	int seconds=0;

	/* make sure the user has rights to view system information */
	if(is_authorized_for_system_information(&current_authdata)==FALSE){

		printf("<P><DIV CLASS='errorMessage'>It appears as though you do not have permission to view process information...</DIV></P>\n");
		printf("<P><DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
		printf("and check the authorization options in your CGI configuration file.</DIV></P>\n");

		return;
	        }

	printf("<P>\n");
	printf("<DIV ALIGN=CENTER>\n");

	printf("<TABLE BORDER=0 CELLPADDING=20>\n");
	printf("<TR><TD VALIGN=TOP>\n");

	printf("<DIV CLASS='dataTitle'>Process Information</DIV>\n");

	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 CLASS='data'>\n");
	printf("<TR><TD class='stateInfoTable1'>\n");
	printf("<TABLE BORDER=0>\n");

	/* program start time */
	get_time_string(&program_start,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
	printf("<TR><TD CLASS='dataVar'>Program Start Time:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",date_time);

	/* total running time */
	time(&current_time);
	run_time=(unsigned long)(current_time-program_start);
	get_time_breakdown(run_time,&days,&hours,&minutes,&seconds);
	sprintf(run_time_string,"%dd %dh %dm %ds",days,hours,minutes,seconds);
	printf("<TR><TD CLASS='dataVar'>Total Running Time:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",run_time_string);

	/* last external check */
	get_time_string(&last_command_check,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
	printf("<TR><TD CLASS='dataVar'>Last External Command Check:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(last_command_check==(time_t)0)?"N/A":date_time);

	/* last log file rotation */
	get_time_string(&last_log_rotation,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
	printf("<TR><TD CLASS='dataVar'>Last Log File Rotation:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(last_log_rotation==(time_t)0)?"N/A":date_time);

	/* PID */
	printf("<TR><TD CLASS='dataVar'>Nagios PID</TD><TD CLASS='dataval'>%d</TD></TR>\n",nagios_pid);

	/* notifications enabled */
	printf("<TR><TD CLASS='dataVar'>Notifications Enabled?</TD><TD CLASS='dataVal'><DIV CLASS='notifications%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",(enable_notifications==TRUE)?"ENABLED":"DISABLED",(enable_notifications==TRUE)?"YES":"NO");

	/* service check execution enabled */
	printf("<TR><TD CLASS='dataVar'>Service Checks Being Executed?</TD><TD CLASS='dataVal'><DIV CLASS='checks%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",(execute_service_checks==TRUE)?"ENABLED":"DISABLED",(execute_service_checks==TRUE)?"YES":"NO");

	/* passive service check acceptance */
	printf("<TR><TD CLASS='dataVar'>Passive Service Checks Being Accepted?</TD><TD CLASS='dataVal'><DIV CLASS='checks%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",(accept_passive_service_checks==TRUE)?"ENABLED":"DISABLED",(accept_passive_service_checks==TRUE)?"YES":"NO");

	/* event handlers enabled */
	printf("<TR><TD CLASS='dataVar'>Event Handlers Enabled?</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(enable_event_handlers==TRUE)?"Yes":"No");

	/* obsessing over services */
	printf("<TR><TD CLASS='dataVar'>Obsessing Over Services?</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(obsess_over_services==TRUE)?"Yes":"No");

	/* flap detection enabled */
	printf("<TR><TD CLASS='dataVar'>Flap Detection Enabled?</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(enable_flap_detection==TRUE)?"Yes":"No");

#ifdef PREDICT_FAILURES
	/* failure prediction enabled */
	printf("<TR><TD CLASS='dataVar'>Failure Prediction Enabled?</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(enable_failure_prediction==TRUE)?"Yes":"No");
#endif

	/* process performance data */
	printf("<TR><TD CLASS='dataVar'>Performance Data Being Processed?</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(process_performance_data==TRUE)?"Yes":"No");

#ifdef USE_OLDCRUD
	/* daemon mode */
	printf("<TR><TD CLASS='dataVar'>Running As A Daemon?</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(daemon_mode==TRUE)?"Yes":"No");
#endif

	printf("</TABLE>\n");
	printf("</TD></TR>\n");
	printf("</TABLE>\n");


	printf("</TD><TD VALIGN=TOP>\n");

	printf("<DIV CLASS='commandTitle'>Process Commands</DIV>\n");

	printf("<TABLE BORDER=1 CELLPADDING=0 CELLSPACING=0 CLASS='command'>\n");
	printf("<TR><TD>\n");

	if(nagios_process_state==STATE_OK){
		printf("<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 CLASS='command'>\n");

#ifndef DUMMY_INSTALL
		printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Shutdown the Nagios Process'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Shutdown the Nagios process</a></td></tr>\n",url_images_path,STOP_ICON,COMMAND_CGI,CMD_SHUTDOWN_PROCESS);
		printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Restart the Nagios Process'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Restart the Nagios process</a></td></tr>\n",url_images_path,RESTART_ICON,COMMAND_CGI,CMD_RESTART_PROCESS);
#endif

		if(enable_notifications==TRUE)
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Disable Notifications'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Disable notifications</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_NOTIFICATIONS);
		else
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Enable Notifications'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Enable notifications</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_NOTIFICATIONS);

		if(execute_service_checks==TRUE)
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Stop Executing Service Checks'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Stop executing service checks</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_STOP_EXECUTING_SVC_CHECKS);
		else
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Start Executing Service Checks'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Start executing service checks</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_START_EXECUTING_SVC_CHECKS);

		if(accept_passive_service_checks==TRUE)
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Stop Accepting Passive Service Checks'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Stop accepting passive service checks</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS);
		else
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Start Accepting Passive Service Checks'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Start accepting passive service checks</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS);
		if(enable_event_handlers==TRUE)
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Disable Event Handlers'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Disable event handlers</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_EVENT_HANDLERS);
		else
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Enable Event Handlers'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Enable event handlers</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_EVENT_HANDLERS);
		if(obsess_over_services==TRUE)
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Stop Obsessing Over Services'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Stop obsessing over services</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_STOP_OBSESSING_OVER_SVC_CHECKS);
		else
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Start Obsessing Over Services'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Start obsessing over services</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_START_OBSESSING_OVER_SVC_CHECKS);
		if(enable_flap_detection==TRUE)
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Disable Flap Detection'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Disable flap detection</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_FLAP_DETECTION);
		else
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Enable Flap Detection'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Enable flap detection</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_FLAP_DETECTION);
#ifdef PREDICT_FAILURES
		if(enable_failure_prediction==TRUE)
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Disable Failure Prediction'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Disable failure prediction</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_FAILURE_PREDICTION);
		else
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Enable Failure Prediction'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Enable failure prediction</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_FAILURE_PREDICTION);
#endif
		if(process_performance_data==TRUE)
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Disable Performance Data'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Disable performance data</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_PERFORMANCE_DATA);
		else
			printf("<TR CLASS='command'><TD><img src='%s%s' border=0 ALT='Enable Performance Data'></td><td CLASS='command'><a href='%s?cmd_typ=%d'>Enable performance data</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_PERFORMANCE_DATA);

		printf("</TABLE>\n");
	        }
	else{
		printf("<DIV ALIGN=CENTER CLASS='infoMessage'>It appears as though Nagios is not running, so commands are temporarily unavailable...\n");
		if(!strcmp(nagios_check_command,"")){
			printf("<BR><BR>\n");
			printf("Hint: It looks as though you have not defined a command for checking the process state by supplying a value for the <b>nagios_check_command</b> option in the CGI configuration file.<BR>\n");
			printf("Read the documentation for more information on checking the status of the Nagios process in the CGIs.\n");
		        }
		printf("</DIV>\n");
	        }

	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	printf("</TD></TR></TABLE>\n");


	printf("<P>");
	printf("<DIV ALIGN=CENTER>\n");

	printf("<DIV CLASS='dataTitle'>Process Status Information</DIV>\n");

	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 CLASS='data'>\n");
	printf("<TR><TD class='stateInfoTable2'>\n");
	printf("<TABLE BORDER=0>\n");

	if(nagios_process_state==STATE_OK){
		strcpy(state_string,"OK");
		state_class="processOK";
		}
	else if(nagios_process_state==STATE_WARNING){
		strcpy(state_string,"WARNING");
		state_class="processWARNING";
		}
	else if(nagios_process_state==STATE_CRITICAL){
		strcpy(state_string,"CRITICAL");
		state_class="processCRITICAL";
		}
	else{
		strcpy(state_string,"UNKNOWN");
		state_class="processUNKNOWN";
		}

	/* process state */
	printf("<TR><TD CLASS='dataVar'>Process Status:</TD><TD CLASS='dataVal'><DIV CLASS='%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",state_class,state_string);

	/* process check command result */
	printf("<TR><TD CLASS='dataVar'>Check Command Output:&nbsp;</TD><TD CLASS='dataVal'>%s&nbsp;</TD></TR>\n",nagios_process_info);

	printf("</TABLE>\n");
	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	printf("</DIV>\n");
	printf("</P>\n");



	return;
	}


void show_host_info(void){
	hoststatus *temp_hoststatus;
	host *temp_host;
	char date_time[MAX_DATETIME_LENGTH];
	char state_duration[48];
	char status_age[48];
	char state_string[MAX_INPUT_BUFFER];
	char *bg_class="";
	unsigned long total_monitored_time;
	unsigned long time_up;
	unsigned long time_down;
	unsigned long time_unreachable;
	float percent_time_up;
	float percent_time_down;
	float percent_time_unreachable;
	char time_up_string[48];
	char time_down_string[48];
	char time_unreachable_string[48];
	char total_time_string[48];
	int days;
	int hours;
	int minutes;
	int seconds;
	time_t current_time;
	time_t t;
	int duration_error=FALSE;


	/* get host info */
	temp_host=find_host(host_name);

	/* make sure the user has rights to view host information */
	if(is_authorized_for_host(temp_host,&current_authdata)==FALSE){

		printf("<P><DIV CLASS='errorMessage'>It appears as though you do not have permission to view information for this host...</DIV></P>\n");
		printf("<P><DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
		printf("and check the authorization options in your CGI configuration file.</DIV></P>\n");

		return;
	        }

	/* get host status info */
	temp_hoststatus=find_hoststatus(host_name);

	/* make sure host information exists */
	if(temp_host==NULL){
		printf("<P><DIV CLASS='errorMessage'>Error: Host Not Found!</DIV></P>>");
		return;
		}
	if(temp_hoststatus==NULL){
		printf("<P><DIV CLASS='errorMessage'>Error: Host Status Information Not Found!</DIV></P");
		return;
		}


	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CELLPADDING=0 WIDTH=100%%>\n");
	printf("<TR>\n");

	printf("<TD ALIGN=CENTER VALIGN=TOP CLASS='stateInfoPanel'>\n");
	
	printf("<DIV CLASS='dataTitle'>Host State Information</DIV>\n");

	if(temp_hoststatus->last_check==0L)
		printf("<P><DIV ALIGN=CENTER>This host has not yet been checked, so status information is not available.</DIV></P>\n");

	else{

		printf("<TABLE BORDER=0>\n");
		printf("<TR><TD>\n");

		printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0>\n");
		printf("<TR><TD class='stateInfoTable1'>\n");
		printf("<TABLE BORDER=0>\n");

		if(temp_hoststatus->status==HOST_UP){
			strcpy(state_string,"UP");
			bg_class="hostUP";
		        }
		else if(temp_hoststatus->status==HOST_DOWN){
			strcpy(state_string,"DOWN");
			bg_class="hostDOWN";
		        }
		else if(temp_hoststatus->status==HOST_UNREACHABLE){
			strcpy(state_string,"UNREACHABLE");
			bg_class="hostUNREACHABLE";
		        }

		printf("<TR><TD CLASS='dataVar'>Host Status:</td><td CLASS='dataVal'><DIV CLASS='%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></td></tr>\n",bg_class,state_string,(temp_hoststatus->problem_has_been_acknowledged==TRUE)?"(Has been acknowledged)":"");

		printf("<TR><TD CLASS='dataVar'>Status Information:</td><td CLASS='dataVal'>%s</td></tr>\n",temp_hoststatus->information);

		get_time_string(&temp_hoststatus->last_check,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TR><TD CLASS='dataVar'>Last Status Check:</td><td CLASS='dataVal'>%s</td></tr>\n",date_time);

		current_time=time(NULL);
		t=0;
		duration_error=FALSE;
		if(temp_hoststatus->last_check>current_time)
			duration_error=TRUE;
		else
			t=current_time-temp_hoststatus->last_check;
		get_time_breakdown((unsigned long)t,&days,&hours,&minutes,&seconds);
		if(duration_error==TRUE)
			snprintf(status_age,sizeof(status_age)-1,"???");
		else if(temp_hoststatus->last_check==(time_t)0)
			snprintf(status_age,sizeof(status_age)-1,"N/A");
		else
			snprintf(status_age,sizeof(status_age)-1,"%2dd %2dh %2dm %2ds",days,hours,minutes,seconds);
		status_age[sizeof(status_age)-1]='\x0';
		printf("<TR><TD CLASS='dataVar'>Status Data Age:</td><td CLASS='dataVal'>%s</td></tr>\n",status_age);

		get_time_string(&temp_hoststatus->last_state_change,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TR><TD CLASS='dataVar'>Last State Change:</td><td CLASS='dataVal'>%s</td></tr>\n",(temp_hoststatus->last_state_change==(time_t)0)?"N/A":date_time);

		t=0;
		duration_error=FALSE;
		if(temp_hoststatus->last_state_change==(time_t)0){
			if(program_start>current_time)
				duration_error=TRUE;
			else
				t=current_time-program_start;
		        }
		else{
			if(temp_hoststatus->last_state_change>current_time)
				duration_error=TRUE;
			else
				t=current_time-temp_hoststatus->last_state_change;
		        }
		get_time_breakdown((unsigned long)t,&days,&hours,&minutes,&seconds);
		if(duration_error==TRUE)
			snprintf(state_duration,sizeof(state_duration)-1,"???");
		else
			snprintf(state_duration,sizeof(state_duration)-1,"%2dd %2dh %2dm %2ds%s",days,hours,minutes,seconds,(temp_hoststatus->last_state_change==(time_t)0)?"+":"");
		state_duration[sizeof(state_duration)-1]='\x0';
		printf("<TR><TD CLASS='dataVar'>Current State Duration:</td><td CLASS='dataVal'>%s</td></tr>\n",state_duration);

		get_time_string(&temp_hoststatus->last_notification,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TR><TD CLASS='dataVar'>Last Host Notification:</td><td CLASS='dataVal'>%s</td></tr>\n",(temp_hoststatus->last_notification==(time_t)0)?"N/A":date_time);

		printf("<TR><TD CLASS='dataVar'>Current Notification Number:&nbsp;&nbsp;</td><td CLASS='dataVal'>%d&nbsp;&nbsp;</td></tr>\n",temp_hoststatus->current_notification_number);

		printf("<TR><TD CLASS='dataVar'>Is This Host Flapping?</td><td CLASS='dataVal'>");
		if(temp_hoststatus->flap_detection_enabled==FALSE || enable_flap_detection==FALSE)
			printf("N/A");
		else
			printf("<DIV CLASS='%sflapping'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV>",(temp_hoststatus->is_flapping==TRUE)?"":"not",(temp_hoststatus->is_flapping==TRUE)?"YES":"NO");
		printf("</td></tr>\n");

		printf("<TR'><TD CLASS='dataVar'>Percent State Change:</td><td CLASS='dataVal'>");
		if(temp_hoststatus->flap_detection_enabled==FALSE || enable_flap_detection==FALSE)
			printf("N/A");
		else
			printf("%3.2f%%",temp_hoststatus->percent_state_change);
		printf("</td></tr>\n");

		printf("<TR><TD CLASS='dataVar'>In Scheduled Downtime?</td><td CLASS='dataVal'><DIV CLASS='downtime%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></td></tr>\n",(temp_hoststatus->scheduled_downtime_depth>0)?"ACTIVE":"INACTIVE",(temp_hoststatus->scheduled_downtime_depth>0)?"YES":"NO");

		get_time_string(&temp_hoststatus->last_update,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TR><TD CLASS='dataVar'>Last Update:</td><td CLASS='dataVal'>%s</td></tr>\n",(temp_hoststatus->last_update==(time_t)0)?"N/A":date_time);

		printf("</TABLE>\n");
		printf("</TD></TR>\n");
		printf("</TABLE>\n");

		printf("</TD></TR>\n");
		printf("<TR><TD>\n");

		printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0>\n");
		printf("<TR><TD class='stateInfoTable2'>\n");
		printf("<TABLE BORDER=0>\n");

		printf("<TR><TD CLASS='dataVar'>Host Checks:</TD><TD CLASS='dataVal'><DIV CLASS='checks%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",(temp_hoststatus->checks_enabled==TRUE)?"ENABLED":"DISABLED",(temp_hoststatus->checks_enabled==TRUE)?"ENABLED":"DISABLED");

		printf("<TR><TD CLASS='dataVar'>Host Notifications:</td><td CLASS='dataVal'><DIV CLASS='notifications%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></td></tr>\n",(temp_hoststatus->notifications_enabled)?"ENABLED":"DISABLED",(temp_hoststatus->notifications_enabled)?"ENABLED":"DISABLED");

		printf("<TR><TD CLASS='dataVar'>Event Handler:</td><td CLASS='dataVal'><DIV CLASS='eventhandlers%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></td></tr>\n",(temp_hoststatus->event_handler_enabled)?"ENABLED":"DISABLED",(temp_hoststatus->event_handler_enabled)?"ENABLED":"DISABLED");

		printf("<TR><TD CLASS='dataVar'>Flap Detection:</td><td CLASS='dataVal'><DIV CLASS='flapdetection%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></td></tr>\n",(temp_hoststatus->flap_detection_enabled==TRUE)?"ENABLED":"DISABLED",(temp_hoststatus->flap_detection_enabled==TRUE)?"ENABLED":"DISABLED");

		printf("</TABLE>\n");
		printf("</TD></TR>\n");
		printf("</TABLE>\n");

		printf("</TD></TR>\n");
		printf("</TABLE>\n");
	        }

	printf("</TD>\n");

	printf("<TD ALIGN=CENTER VALIGN=TOP>\n");
	printf("<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0><TR>\n");

	printf("<TD ALIGN=CENTER VALIGN=TOP CLASS='commandPanel'>\n");

	printf("<DIV CLASS='commandTitle'>Host Commands</DIV>\n");

	printf("<TABLE BORDER=1 CELLPADDING=0 CELLSPACING=0 CLASS='command'><TR><TD>\n");

	if(nagios_process_state==STATE_OK){

		printf("<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=0 CLASS='command'>\n");

		if(temp_hoststatus->checks_enabled==TRUE)
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Checks Of This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Disable checks of this host</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_HOST_CHECK,url_encode(host_name));
		else
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Checks Of This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Enable checks of this host</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_HOST_CHECK,url_encode(host_name));

		if(temp_hoststatus->status==HOST_DOWN || temp_hoststatus->status==HOST_UNREACHABLE){
			if(temp_hoststatus->problem_has_been_acknowledged==FALSE)
				printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Acknowledge This Host Problem'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Acknowledge this host problem</a></td></tr>\n",url_images_path,ACKNOWLEDGEMENT_ICON,COMMAND_CGI,CMD_ACKNOWLEDGE_HOST_PROBLEM,url_encode(host_name));
			else
				printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Remove Problem Acknowledgement'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Remove problem acknowledgement</a></td></tr>\n",url_images_path,REMOVE_ACKNOWLEDGEMENT_ICON,COMMAND_CGI,CMD_REMOVE_HOST_ACKNOWLEDGEMENT,url_encode(host_name));
		        }

		if(temp_hoststatus->notifications_enabled==TRUE)
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Notifications For This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Disable notifications for this host</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_HOST_NOTIFICATIONS,url_encode(host_name));
		else
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Notifications For This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Enable notifications for this host</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_HOST_NOTIFICATIONS,url_encode(host_name));
		if(temp_hoststatus->status!=HOST_UP)
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Delay Next Host Notification'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Delay next host notification</a></td></tr>\n",url_images_path,DELAY_ICON,COMMAND_CGI,CMD_DELAY_HOST_NOTIFICATION,url_encode(host_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Schedule Downtime For This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Schedule downtime for this host</a></td></tr>\n",url_images_path,DOWNTIME_ICON,COMMAND_CGI,CMD_SCHEDULE_HOST_DOWNTIME,url_encode(host_name));

		/*
		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Cancel Scheduled Downtime For This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Cancel scheduled downtime for this host</a></td></tr>\n",url_images_path,SCHEDULED_DOWNTIME_ICON,COMMAND_CGI,CMD_CANCEL_HOST_DOWNTIME,url_encode(host_name));
		*/

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Notifications For All Services On This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Disable notifications for all services on this host</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_HOST_SVC_NOTIFICATIONS,url_encode(host_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Notifications For All Services On This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Enable notifications for all services on this host</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_HOST_SVC_NOTIFICATIONS,url_encode(host_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Schedule An Immediate Check Of All Services On This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Schedule an immediate check of all services on this host</a></td></tr>\n",url_images_path,DELAY_ICON,COMMAND_CGI,CMD_IMMEDIATE_HOST_SVC_CHECKS,url_encode(host_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Checks Of All Services On This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Disable checks of all services on this host</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_HOST_SVC_CHECKS,url_encode(host_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Checks Of All Services On This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Enable checks of all services on this host</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_HOST_SVC_CHECKS,url_encode(host_name));

		if(temp_hoststatus->event_handler_enabled==TRUE)
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Event Handler For This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Disable event handler for this host</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_HOST_EVENT_HANDLER,url_encode(host_name));
		else
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Event Handler For This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Enable event handler for this host</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_HOST_EVENT_HANDLER,url_encode(host_name));
		if(temp_hoststatus->flap_detection_enabled==TRUE)
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Flap Detection For This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Disable flap detection for this host</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_HOST_FLAP_DETECTION,url_encode(host_name));
		else
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Flap Detection For This Host'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s'>Enable flap detection for this host</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_HOST_FLAP_DETECTION,url_encode(host_name));

		printf("</TABLE>\n");
	        }
	else{
		printf("<DIV ALIGN=CENTER CLASS='infoMessage'>It appears as though Nagios is not running, so commands are temporarily unavailable...<br>\n");
		printf("Click <a href='%s?type=%d'>here</a> to view Nagios process information</DIV>\n",EXTINFO_CGI,DISPLAY_PROCESS_INFO);
	        }
	printf("</TD></TR></TABLE>\n");

	printf("</TD>\n");

	printf("</TR>\n");
	printf("</TABLE></TR>\n");

	printf("<TR>\n");

	printf("<TD COLSPAN=2 ALIGN=CENTER VALIGN=TOP CLASS='commentPanel'>\n");

	/* display comments */
	display_comments(HOST_COMMENT);

	printf("</TD>\n");

	printf("</TR>\n");
	printf("</TABLE>\n");
	printf("</DIV>\n");

	return;
	}


void show_service_info(void){
	service *temp_service;
	char date_time[MAX_DATETIME_LENGTH];
	char status_age[48];
	char state_duration[48];
	servicestatus *temp_svcstatus;
	char state_string[MAX_INPUT_BUFFER];
	char *bg_class="";
	float percent_time_ok;
	float percent_time_warning;
	float percent_time_unknown;
	float percent_time_critical;
	char time_ok_string[48];
	char time_warning_string[48];
	char time_unknown_string[48];
	char time_critical_string[48];
	char total_time_string[48];
	unsigned long time_ok;
	unsigned long time_warning;
	unsigned long time_unknown;
	unsigned long time_critical;
	unsigned long total_monitored_time;
	int days;
	int hours;
	int minutes;
	int seconds;
	time_t t;
	time_t current_time;
	int duration_error=FALSE;

	/* find the service */
	temp_service=find_service(host_name,service_desc);

	/* make sure the user has rights to view service information */
	if(is_authorized_for_service(temp_service,&current_authdata)==FALSE){

		printf("<P><DIV CLASS='errorMessage'>It appears as though you do not have permission to view information for this service...</DIV></P>\n");
		printf("<P><DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
		printf("and check the authorization options in your CGI configuration file.</DIV></P>\n");

		return;
	        }

	/* get service status info */
	temp_svcstatus=find_servicestatus(host_name,service_desc);

	/* make sure service information exists */
	if(temp_service==NULL){
		printf("<P><DIV CLASS='errorMessage'>Error: Service Not Found!</DIV></P>");
		return;
		}
	if(temp_svcstatus==NULL){
		printf("<P><DIV CLASS='errorMessage'>Error: Service Status Not Found!</DIV></P>");
		return;
		}


	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 WIDTH=100%%>\n");
	printf("<TR>\n");

	printf("<TD ALIGN=CENTER VALIGN=TOP CLASS='stateInfoPanel'>\n");
	
	printf("<DIV CLASS='dataTitle'>Service State Information</DIV>\n");

	if(temp_svcstatus->last_check==0L)
		printf("<P><DIV ALIGN=CENTER>This service has not yet been checked, so its current status information and state statistics are not available.</DIV></P>\n");

	else{

		printf("<TABLE BORDER=0>\n");

		printf("<TR><TD>\n");

		printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0>\n");
		printf("<TR><TD class='stateInfoTable1'>\n");
		printf("<TABLE BORDER=0>\n");


		if(temp_svcstatus->status==SERVICE_OK || temp_svcstatus->status==SERVICE_RECOVERY){
			strcpy(state_string,"OK");
			bg_class="serviceOK";
			}
		else if(temp_svcstatus->status==SERVICE_WARNING){
			strcpy(state_string,"WARNING");
			bg_class="serviceWARNING";
			}
		else if(temp_svcstatus->status==SERVICE_CRITICAL || temp_svcstatus->status==SERVICE_UNREACHABLE || temp_svcstatus->status==SERVICE_HOST_DOWN){
			strcpy(state_string,"CRITICAL");
			bg_class="serviceCRITICAL";
			}
		else{
			strcpy(state_string,"UNKNOWN");
			bg_class="serviceUNKNOWN";
			}
		printf("<TR><TD CLASS='dataVar'>Current Status:</TD><TD CLASS='dataVal'><DIV CLASS='%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",bg_class,state_string,(temp_svcstatus->problem_has_been_acknowledged==TRUE)?"(Has been acknowledged)":"");

		printf("<TR><TD CLASS='dataVar'>Status Information:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",temp_svcstatus->information);

		printf("<TR><TD CLASS='dataVar'>Current Attempt:</TD><TD CLASS='dataVal'>%d/%d</TD></TR>\n",temp_svcstatus->current_attempt,temp_svcstatus->max_attempts);

		printf("<TR><TD CLASS='dataVar'>State Type:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(temp_svcstatus->state_type==HARD_STATE)?"HARD":"SOFT");

		printf("<TR><TD CLASS='dataVar'>Last Check Type:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(temp_svcstatus->check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE");

		get_time_string(&temp_svcstatus->last_check,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TR><TD CLASS='dataVar'>Last Check Time:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",date_time);

		current_time=time(NULL);
		t=0;
		duration_error=FALSE;
		if(temp_svcstatus->last_check>current_time)
			duration_error=TRUE;
		else
			t=current_time-temp_svcstatus->last_check;
		get_time_breakdown((unsigned long)t,&days,&hours,&minutes,&seconds);
		if(duration_error==TRUE)
			snprintf(status_age,sizeof(status_age)-1,"???");
		else if(temp_svcstatus->last_check==(time_t)0)
			snprintf(status_age,sizeof(status_age)-1,"N/A");
		else
			snprintf(status_age,sizeof(status_age)-1,"%2dd %2dh %2dm %2ds",days,hours,minutes,seconds);
		status_age[sizeof(status_age)-1]='\x0';
		printf("<TR><TD CLASS='dataVar'>Status Data Age:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",status_age);

		get_time_string(&temp_svcstatus->next_check,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TR><TD CLASS='dataVar'>Next Scheduled Active Check:&nbsp;&nbsp;</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(temp_svcstatus->checks_enabled && temp_svcstatus->next_check!=(time_t)0)?date_time:"N/A");

		printf("<TR><TD CLASS='dataVar'>Latency:</TD><TD CLASS='dataVal'>");
		if(temp_svcstatus->check_type==SERVICE_CHECK_ACTIVE)
			printf("%s%d second%s",(temp_svcstatus->latency==0)?"&lt; ":"",(temp_svcstatus->latency==0)?1:temp_svcstatus->latency,(temp_svcstatus->latency>1)?"s":"");
		else
			printf("N/A");
		printf("</TD></TR>\n");

		printf("<TR><TD CLASS='dataVar'>Check Duration:</TD><TD CLASS='dataVal'>%s%d second%s</TD></TR>\n",(temp_svcstatus->execution_time==0)?"&lt; ":"",(temp_svcstatus->execution_time==0)?1:temp_svcstatus->execution_time,(temp_svcstatus->execution_time>1)?"s":"");

		get_time_string(&temp_svcstatus->last_state_change,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TR><TD CLASS='dataVar'>Last State Change:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(temp_svcstatus->last_state_change==(time_t)0)?"N/A":date_time);

		t=0;
		duration_error=FALSE;
		if(temp_svcstatus->last_state_change==(time_t)0){
			if(program_start>current_time)
				duration_error=TRUE;
			else
				t=current_time-program_start;
		        }
		else{
			if(temp_svcstatus->last_state_change>current_time)
				duration_error=TRUE;
			else
				t=current_time-temp_svcstatus->last_state_change;
		        }
		get_time_breakdown((unsigned long)t,&days,&hours,&minutes,&seconds);
		if(duration_error==TRUE)
			snprintf(state_duration,sizeof(state_duration)-1,"???");
		else
			snprintf(state_duration,sizeof(state_duration)-1,"%2dd %2dh %2dm %2ds%s",days,hours,minutes,seconds,(temp_svcstatus->last_state_change==(time_t)0)?"+":"");
		state_duration[sizeof(state_duration)-1]='\x0';
		printf("<TR><TD CLASS='dataVar'>Current State Duration:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",state_duration);

		get_time_string(&temp_svcstatus->last_notification,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TR><TD CLASS='dataVar'>Last Service Notification:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(temp_svcstatus->last_notification==(time_t)0)?"N/A":date_time);

		get_time_string(&temp_svcstatus->last_notification,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TR><TD CLASS='dataVar'>Current Notification Number:</TD><TD CLASS='dataVal'>%d</TD></TR>\n",temp_svcstatus->current_notification_number);

		printf("<TR><TD CLASS='dataVar'>Is This Service Flapping?</TD><TD CLASS='dataVal'>");
		if(temp_svcstatus->flap_detection_enabled==FALSE || enable_flap_detection==FALSE)
			printf("N/A");
		else
			printf("<DIV CLASS='%sflapping'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV>",(temp_svcstatus->is_flapping==TRUE)?"":"not",(temp_svcstatus->is_flapping==TRUE)?"YES":"NO");
		printf("</TD></TR>\n");

		printf("<TR><TD CLASS='dataVar'>Percent State Change:</TD><TD CLASS='dataVal'>");
		if(temp_svcstatus->flap_detection_enabled==FALSE || enable_flap_detection==FALSE)
			printf("N/A");
		else
			printf("%3.2f%%",temp_svcstatus->percent_state_change);
		printf("</TD></TR>\n");

		printf("<TR><TD CLASS='dataVar'>In Scheduled Downtime?</TD><TD CLASS='dataVal'><DIV CLASS='downtime%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",(temp_svcstatus->scheduled_downtime_depth>0)?"ACTIVE":"INACTIVE",(temp_svcstatus->scheduled_downtime_depth>0)?"YES":"NO");

		get_time_string(&temp_svcstatus->last_update,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TR><TD CLASS='dataVar'>Last Update:</TD><TD CLASS='dataVal'>%s</TD></TR>\n",(temp_svcstatus->last_update==(time_t)0)?"N/A":date_time);


		printf("</TABLE>\n");
		printf("</TD></TR>\n");
		printf("</TABLE>\n");

		printf("</TD></TR>\n");
		printf("<TR><TD>\n");

		printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0>\n");
		printf("<TR><TD class='stateInfoTable2'>\n");
		printf("<TABLE BORDER=0>\n");

		printf("<TR><TD CLASS='dataVar'>Service Checks:</TD><td CLASS='dataVal'><DIV CLASS='checks%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",(temp_svcstatus->checks_enabled)?"ENABLED":"DISABLED",(temp_svcstatus->checks_enabled)?"ENABLED":"DISABLED");

		printf("<TR><TD CLASS='dataVar'>Passive Checks:</TD><td CLASS='dataVal'><DIV CLASS='checks%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",(temp_svcstatus->accept_passive_service_checks==TRUE)?"ENABLED":"DISABLED",(temp_svcstatus->accept_passive_service_checks)?"ENABLED":"DISABLED");

		printf("<TR><td CLASS='dataVar'>Service Notifications:</TD><td CLASS='dataVal'><DIV CLASS='notifications%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",(temp_svcstatus->notifications_enabled)?"ENABLED":"DISABLED",(temp_svcstatus->notifications_enabled)?"ENABLED":"DISABLED");

		printf("<TR><TD CLASS='dataVar'>Event Handler:</TD><td CLASS='dataVal'><DIV CLASS='eventhandlers%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",(temp_svcstatus->event_handler_enabled)?"ENABLED":"DISABLED",(temp_svcstatus->event_handler_enabled)?"ENABLED":"DISABLED");

		printf("<TR><TD CLASS='dataVar'>Flap Detection:</TD><td CLASS='dataVal'><DIV CLASS='flapdetection%s'>&nbsp;&nbsp;%s&nbsp;&nbsp;</DIV></TD></TR>\n",(temp_svcstatus->flap_detection_enabled==TRUE)?"ENABLED":"DISABLED",(temp_svcstatus->flap_detection_enabled==TRUE)?"ENABLED":"DISABLED");


		printf("</TABLE>\n");
		printf("</TD></TR>\n");
		printf("</TABLE>\n");

		printf("</TD></TR>\n");
		printf("</TABLE>\n");
                }
	

	printf("</TD>\n");

	printf("<TD ALIGN=CENTER VALIGN=TOP>\n");
	printf("<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0><TR>\n");

	printf("<TD ALIGN=CENTER VALIGN=TOP CLASS='commandPanel'>\n");

	printf("<DIV CLASS='dataTitle'>Service Commands</DIV>\n");

	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 CLASS='command'>\n");
	printf("<TR><TD>\n");

	if(nagios_process_state==STATE_OK){
		printf("<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=0 CLASS='command'>\n");

		if(temp_svcstatus->checks_enabled){

			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Checks Of This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_SVC_CHECK,url_encode(host_name));
			printf("&service=%s'>Disable checks of this service</a></td></tr>\n",url_encode(service_desc));

			printf("<tr CLASS='data'><td><img src='%s%s' border=0 ALT='Re-schedule Next Service Check'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,DELAY_ICON,COMMAND_CGI,CMD_DELAY_SVC_CHECK,url_encode(host_name));
			printf("&service=%s'>Re-schedule the next check of this service</a></td></tr>\n",url_encode(service_desc));
	                }
		else{
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Checks Of This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_SVC_CHECK,url_encode(host_name));
			printf("&service=%s'>Enable checks of this service</a></td></tr>\n",url_encode(service_desc));
	                }

		if(temp_svcstatus->accept_passive_service_checks==TRUE){
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Submit Passive Check Result For This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,PASSIVE_ICON,COMMAND_CGI,CMD_PROCESS_SERVICE_CHECK_RESULT,url_encode(host_name));
			printf("&service=%s'>Submit passive check result for this service</a></td></tr>\n",url_encode(service_desc));

			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Stop Accepting Passive Checks For This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_PASSIVE_SVC_CHECKS,url_encode(host_name));
			printf("&service=%s'>Stop accepting passive checks for this service</a></td></tr>\n",url_encode(service_desc));
		        }
		else{
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Start Accepting Passive Checks For This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_PASSIVE_SVC_CHECKS,url_encode(host_name));
			printf("&service=%s'>Start accepting passive checks for this service</a></td></tr>\n",url_encode(service_desc));
		        }

		if((temp_svcstatus->status==SERVICE_WARNING || temp_svcstatus->status==SERVICE_UNKNOWN || temp_svcstatus->status==SERVICE_CRITICAL) && temp_svcstatus->state_type==HARD_STATE){
			if(temp_svcstatus->problem_has_been_acknowledged==FALSE){
				printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Acknowledge This Service Problem'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,ACKNOWLEDGEMENT_ICON,COMMAND_CGI,CMD_ACKNOWLEDGE_SVC_PROBLEM,url_encode(host_name));
				printf("&service=%s'>Acknowledge this service problem</a></td></tr>\n",url_encode(service_desc));
			        }
			else{
				printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Remove Problem Acknowledgement'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,REMOVE_ACKNOWLEDGEMENT_ICON,COMMAND_CGI,CMD_REMOVE_SVC_ACKNOWLEDGEMENT,url_encode(host_name));
				printf("&service=%s'>Remove problem acknowledgement</a></td></tr>\n",url_encode(service_desc));
			        }
		        }
		if(temp_svcstatus->notifications_enabled){
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Notifications For This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_SVC_NOTIFICATIONS,url_encode(host_name));
			printf("&service=%s'>Disable notifications for this service</a></td></tr>\n",url_encode(service_desc));
			if(temp_svcstatus->status!=SERVICE_OK){
				printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Delay Next Service Notification'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,DELAY_ICON,COMMAND_CGI,CMD_DELAY_SVC_NOTIFICATION,url_encode(host_name));
				printf("&service=%s'>Delay next service notification</a></td></tr>\n",url_encode(service_desc));
		                }
		        }
		else{
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Notifications For This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_SVC_NOTIFICATIONS,url_encode(host_name));
			printf("&service=%s'>Enable notifications for this service</a></td></tr>\n",url_encode(service_desc));
		        }

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Schedule Downtime For This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,DOWNTIME_ICON,COMMAND_CGI,CMD_SCHEDULE_SVC_DOWNTIME,url_encode(host_name));
		printf("&service=%s'>Schedule downtime for this service</a></td></tr>\n",url_encode(service_desc));

		/*
		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Cancel Scheduled Downtime For This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,SCHEDULED_DOWNTIME_ICON,COMMAND_CGI,CMD_CANCEL_SVC_DOWNTIME,url_encode(host_name));
		printf("&service=%s'>Cancel scheduled downtime for this service</a></td></tr>\n",url_encode(service_desc));
		*/

		if(temp_svcstatus->event_handler_enabled==TRUE){
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Event Handler For This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_SVC_EVENT_HANDLER,url_encode(host_name));
			printf("&service=%s'>Disable event handler for this service</a></td></tr>\n",url_encode(service_desc));
		        }
		else{
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Event Handler For This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_SVC_EVENT_HANDLER,url_encode(host_name));
			printf("&service=%s'>Enable event handler for this service</a></td></tr>\n",url_encode(service_desc));
		        }

		if(temp_svcstatus->flap_detection_enabled==TRUE){
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Flap Detection For This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_SVC_FLAP_DETECTION,url_encode(host_name));
			printf("&service=%s'>Disable flap detection for this service</a></td></tr>\n",url_encode(service_desc));
		        }
		else{
			printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Flap Detection For This Service'></td><td CLASS='command'><a href='%s?cmd_typ=%d&host=%s",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_SVC_FLAP_DETECTION,url_encode(host_name));
			printf("&service=%s'>Enable flap detection for this service</a></td></tr>\n",url_encode(service_desc));
		        }

		printf("</table>\n");
	        }
	else{
		printf("<DIV CLASS='infoMessage'>It appears as though Nagios is not running, so commands are temporarily unavailable...<br>\n");
		printf("Click <a href='%s?type=%d'>here</a> to view Nagios process information</DIV>\n",EXTINFO_CGI,DISPLAY_PROCESS_INFO);
	        }

	printf("</td></tr>\n");
	printf("</table>\n");

	printf("</TD>\n");

	printf("</TR></TABLE></TD>\n");
	printf("</TR>\n");

	printf("<TR><TD COLSPAN=2><BR></TD></TR>\n");

	printf("<TR>\n");
	printf("<TD COLSPAN=2 ALIGN=CENTER VALIGN=TOP CLASS='commentPanel'>\n");

	/* display comments */
	display_comments(SERVICE_COMMENT);

	printf("</TD>\n");
	printf("</TR>\n");

	printf("</TABLE>\n");
	printf("</DIV>\n");

	return;
	}




void show_hostgroup_info(void){
	hostgroup *temp_hostgroup;


	/* get host info */
	temp_hostgroup=find_hostgroup(hostgroup_name,NULL);

	/* make sure the user has rights to view hostgroup information */
	if(is_authorized_for_hostgroup(temp_hostgroup,&current_authdata)==FALSE){

		printf("<P><DIV CLASS='errorMessage'>It appears as though you do not have permission to view information for this hostgroup...</DIV></P>\n");
		printf("<P><DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
		printf("and check the authorization options in your CGI configuration file.</DIV></P>\n");

		return;
	        }

	/* make sure hostgroup information exists */
	if(temp_hostgroup==NULL){
		printf("<P><DIV CLASS='errorMessage'>Error: Hostgroup Not Found!</DIV></P>>");
		return;
		}


	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 WIDTH=100%%>\n");
	printf("<TR>\n");


	/* top left panel */
	printf("<TD ALIGN=CENTER VALIGN=TOP CLASS='stateInfoPanel'>\n");

	/* right top panel */
	printf("</TD><TD ALIGN=CENTER VALIGN=TOP CLASS='stateInfoPanel' ROWSPAN=2>\n");

	printf("<DIV CLASS='dataTitle'>Hostgroup Commands</DIV>\n");

	if(nagios_process_state==STATE_OK){

		printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 CLASS='command'>\n");
		printf("<TR><TD>\n");

		printf("<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=0 CLASS='command'>\n");

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Schedule Downtime For All Hosts In This Hostgroup'></td><td CLASS='command'><a href='%s?cmd_typ=%d&hostgroup=%s'>Schedule downtime for all hosts in this hostgroup</a></td></tr>\n",url_images_path,DOWNTIME_ICON,COMMAND_CGI,CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME,url_encode(hostgroup_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Schedule Downtime For All Services In This Hostgroup'></td><td CLASS='command'><a href='%s?cmd_typ=%d&hostgroup=%s'>Schedule downtime for all services in this hostgroup</a></td></tr>\n",url_images_path,DOWNTIME_ICON,COMMAND_CGI,CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME,url_encode(hostgroup_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Notifications For All Hosts In This Hostgroup'></td><td CLASS='command'><a href='%s?cmd_typ=%d&hostgroup=%s'>Enable notifications for all hosts in this hostgroup</a></td></tr>\n",url_images_path,NOTIFICATION_ICON,COMMAND_CGI,CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS,url_encode(hostgroup_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Notifications For All Hosts In This Hostgroup'></td><td CLASS='command'><a href='%s?cmd_typ=%d&hostgroup=%s'>Disable notifications for all hosts in this hostgroup</a></td></tr>\n",url_images_path,NOTIFICATIONS_DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS,url_encode(hostgroup_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Notifications For All Services In This Hostgroup'></td><td CLASS='command'><a href='%s?cmd_typ=%d&hostgroup=%s'>Enable notifications for all services in this hostgroup</a></td></tr>\n",url_images_path,NOTIFICATION_ICON,COMMAND_CGI,CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS,url_encode(hostgroup_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Notifications For All Services In This Hostgroup'></td><td CLASS='command'><a href='%s?cmd_typ=%d&hostgroup=%s'>Disable notifications for all services in this hostgroup</a></td></tr>\n",url_images_path,NOTIFICATIONS_DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS,url_encode(hostgroup_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Enable Checks Of All Services In This Hostgroup'></td><td CLASS='command'><a href='%s?cmd_typ=%d&hostgroup=%s'>Enable checks of all services in this hostgroup</a></td></tr>\n",url_images_path,ENABLED_ICON,COMMAND_CGI,CMD_ENABLE_HOSTGROUP_SVC_CHECKS,url_encode(hostgroup_name));

		printf("<tr CLASS='command'><td><img src='%s%s' border=0 ALT='Disable Checks Of All Services In This Hostgroup'></td><td CLASS='command'><a href='%s?cmd_typ=%d&hostgroup=%s'>Disable checks of all services in this hostgroup</a></td></tr>\n",url_images_path,DISABLED_ICON,COMMAND_CGI,CMD_DISABLE_HOSTGROUP_SVC_CHECKS,url_encode(hostgroup_name));

		printf("</table>\n");

		printf("</TD></TR>\n");
		printf("</TABLE>\n");
	        }
	else{
		printf("<DIV CLASS='infoMessage'>It appears as though Nagios is not running, so commands are temporarily unavailable...<br>\n");
		printf("Click <a href='%s?type=%d'>here</a> to view Nagios process information</DIV>\n",EXTINFO_CGI,DISPLAY_PROCESS_INFO);
	        }

	printf("</TD></TR>\n");
	printf("<TR>\n");

	/* left bottom panel */
	printf("<TD ALIGN=CENTER VALIGN=TOP CLASS='stateInfoPanel'>\n");

	printf("</TD></TR>\n");
	printf("</TABLE>\n");
	printf("</DIV>\n");


	printf("</div>\n");

	printf("</TD>\n");



	return;
	}



/* shows all service and host comments */
void show_all_comments(void){
	int total_comments=0;
	char *bg_class="";
	int odd=0;
	char date_time[MAX_DATETIME_LENGTH];
	comment *temp_comment;
	host *temp_host;
	service *temp_service;


	/* read in all comments */
	read_comment_data(get_cgi_config_location());

	printf("<P>\n");
	printf("<DIV CLASS='commentNav'>[&nbsp;<A HREF='#HOSTCOMMENTS' CLASS='commentNav'>Host Comments</A>&nbsp;|&nbsp;<A HREF='#SERVICECOMMENTS' CLASS='commentNav'>Service Comments</A>&nbsp;]</DIV>\n");
	printf("</P>\n");

	printf("<A NAME=HOSTCOMMENTS></A>\n");
	printf("<DIV CLASS='commentTitle'>Host Comments</DIV>\n");

	printf("<div CLASS='comment'><img src='%s%s' border=0>&nbsp;",url_images_path,COMMENT_ICON);
	printf("<a href='%s?cmd_typ=%d'>",COMMAND_CGI,CMD_ADD_HOST_COMMENT);
	printf("Add a new host comment</a></div>\n");

	printf("<P>\n");
	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CLASS='comment'>\n");
	printf("<TR CLASS='comment'><TH CLASS='comment'>Host Name</TH><TH CLASS='comment'>Entry Time</TH><TH CLASS='comment'>Author</TH><TH CLASS='comment'>Comment</TH><TH CLASS='comment'>Comment ID</TH><TH CLASS='comment'>Persistent</TH><TH CLASS='comment'>Actions</TH></TR>\n");

	/* display all the host comments */
	for(temp_comment=comment_list,total_comments=0;temp_comment!=NULL;temp_comment=temp_comment->next){

		if(temp_comment->comment_type!=HOST_COMMENT)
			continue;

		temp_host=find_host(temp_comment->host_name);

		/* make sure the user has rights to view host information */
		if(is_authorized_for_host(temp_host,&current_authdata)==FALSE)
			continue;

		total_comments++;

		if(odd){
			odd=0;
			bg_class="commentOdd";
		        }
		else{
			odd=1;
			bg_class="commentEven";
		        }

		get_time_string(&temp_comment->entry_time,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<tr CLASS='%s'>",bg_class);
		printf("<td CLASS='%s'><A HREF='%s?type=%d&host=%s'>%s</A></td>",bg_class,EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_comment->host_name),temp_comment->host_name);
		printf("<td CLASS='%s'>%s</td><td CLASS='%s'>%s</td><td CLASS='%s'>%s</td><td CLASS='%s'>%d</td><td CLASS='%s'>%s</td>",bg_class,date_time,bg_class,temp_comment->author,bg_class,temp_comment->comment_data,bg_class,temp_comment->comment_id,bg_class,(temp_comment->persistent)?"Yes":"No");
		printf("<td><a href='%s?cmd_typ=%d&com_id=%d'><img src='%s%s' border=0 ALT='Delete This Comment'></td>",COMMAND_CGI,CMD_DEL_HOST_COMMENT,temp_comment->comment_id,url_images_path,DELETE_ICON);
		printf("</tr>\n");
	        }

	if(total_comments==0)
		printf("<TR CLASS='commentOdd'><TD CLASS='commentOdd' COLSPAN=7>There are no host comments</TD></TR>");

	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	printf("<P><BR></P>\n");


	printf("<A NAME=SERVICECOMMENTS></A>\n");
	printf("<DIV CLASS='commentTitle'>Service Comments</DIV>\n");

	printf("<div CLASS='comment'><img src='%s%s' border=0>&nbsp;",url_images_path,COMMENT_ICON);
	printf("<a href='%s?cmd_typ=%d'>",COMMAND_CGI,CMD_ADD_SVC_COMMENT);
	printf("Add a new service comment</a></div>\n");

	printf("<P>\n");
	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CLASS='comment'>\n");
	printf("<TR CLASS='comment'><TH CLASS='comment'>Host Name</TH><TH CLASS='comment'>Service</TH><TH CLASS='comment'>Entry Time</TH><TH CLASS='comment'>Author</TH><TH CLASS='comment'>Comment</TH><TH CLASS='comment'>Comment ID</TH><TH CLASS='comment'>Persistent</TH><TH CLASS='comment'>Actions</TH></TR>\n");

	/* display all the service comments */
	for(temp_comment=comment_list,total_comments=0;temp_comment!=NULL;temp_comment=temp_comment->next){

		if(temp_comment->comment_type!=SERVICE_COMMENT)
			continue;

		temp_service=find_service(temp_comment->host_name,temp_comment->service_description);

		/* make sure the user has rights to view service information */
		if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
			continue;

		total_comments++;

		if(odd){
			odd=0;
			bg_class="commentOdd";
		        }
		else{
			odd=1;
			bg_class="commentEven";
		        }

		get_time_string(&temp_comment->entry_time,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<tr CLASS='%s'>",bg_class);
		printf("<td CLASS='%s'><A HREF='%s?type=%d&host=%s'>%s</A></td>",bg_class,EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_comment->host_name),temp_comment->host_name);
		printf("<td CLASS='%s'><A HREF='%s?type=%d&host=%s",bg_class,EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_comment->host_name));
		printf("&service=%s'>%s</A></td>",url_encode(temp_comment->service_description),temp_comment->service_description);
		printf("<td CLASS='%s'>%s</td><td CLASS='%s'>%s</td><td CLASS='%s'>%s</td><td CLASS='%s'>%d</td><td CLASS='%s'>%s</td>",bg_class,date_time,bg_class,temp_comment->author,bg_class,temp_comment->comment_data,bg_class,temp_comment->comment_id,bg_class,(temp_comment->persistent)?"Yes":"No");
		printf("<td><a href='%s?cmd_typ=%d&com_id=%d'><img src='%s%s' border=0 ALT='Delete This Comment'></td>",COMMAND_CGI,CMD_DEL_SVC_COMMENT,temp_comment->comment_id,url_images_path,DELETE_ICON);
		printf("</tr>\n");
	        }

	if(total_comments==0)
		printf("<TR CLASS='commentOdd'><TD CLASS='commentOdd' COLSPAN=8>There are no service comments</TD></TR>");

	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	return;
        }



void show_performance_data(void){
	service *temp_service=NULL;
	servicestatus *temp_servicestatus=NULL;
	int total_active_checks=0;
	int total_passive_checks=0;
	int min_execution_time=0;
	int max_execution_time=0;
	unsigned long total_execution_time=0L;
	int have_min_execution_time=FALSE;
	int have_max_execution_time=FALSE;
	int min_latency=0;
	int max_latency=0;
	unsigned long total_latency=0L;
	int have_min_latency=FALSE;
	int have_max_latency=FALSE;
	double min_percent_change_a=0.0;
	double max_percent_change_a=0.0;
	double total_percent_change_a=0.0;
	int have_min_percent_change_a=FALSE;
	int have_max_percent_change_a=FALSE;
	double min_percent_change_b=0.0;
	double max_percent_change_b=0.0;
	double total_percent_change_b=0.0;
	int have_min_percent_change_b=FALSE;
	int have_max_percent_change_b=FALSE;
	int active_checks_1min=0;
	int active_checks_5min=0;
	int active_checks_15min=0;
	int active_checks_1hour=0;
	int active_checks_start=0;
	int active_checks_ever=0;
	int passive_checks_1min=0;
	int passive_checks_5min=0;
	int passive_checks_15min=0;
	int passive_checks_1hour=0;
	int passive_checks_start=0;
	int passive_checks_ever=0;
	time_t current_time;


	time(&current_time);

	/* check all services */
	for(temp_servicestatus=servicestatus_list;temp_servicestatus!=NULL;temp_servicestatus=temp_servicestatus->next){

		/* find the service */
		temp_service=find_service(temp_servicestatus->host_name,temp_servicestatus->description);
		
		/* make sure the user has rights to view service information */
		if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
			continue;

		/* is this an active or passive check? */
		if(temp_servicestatus->check_type==SERVICE_CHECK_ACTIVE){

			total_active_checks++;

			total_execution_time+=temp_servicestatus->execution_time;
			if(have_min_execution_time==FALSE || temp_servicestatus->execution_time<min_execution_time){
				have_min_execution_time=TRUE;
				min_execution_time=temp_servicestatus->execution_time;
			        }
			if(have_max_execution_time==FALSE || temp_servicestatus->execution_time>max_execution_time){
				have_max_execution_time=TRUE;
				max_execution_time=temp_servicestatus->execution_time;
			        }

			total_percent_change_a+=temp_servicestatus->percent_state_change;
			if(have_min_percent_change_a==FALSE || temp_servicestatus->percent_state_change<min_percent_change_a){
				have_min_percent_change_a=TRUE;
				min_percent_change_a=temp_servicestatus->percent_state_change;
			        }
			if(have_max_percent_change_a==FALSE || temp_servicestatus->percent_state_change>max_percent_change_a){
				have_max_percent_change_a=TRUE;
				max_percent_change_a=temp_servicestatus->percent_state_change;
			        }

			total_latency+=temp_servicestatus->latency;
			if(have_min_latency==FALSE || temp_servicestatus->latency<min_latency){
				have_min_latency=TRUE;
				min_latency=temp_servicestatus->latency;
			        }
			if(have_max_latency==FALSE || temp_servicestatus->latency>max_latency){
				have_max_latency=TRUE;
				max_latency=temp_servicestatus->latency;
			        }

			if(temp_servicestatus->last_check>=(current_time-60))
				active_checks_1min++;
			if(temp_servicestatus->last_check>=(current_time-300))
				active_checks_5min++;
			if(temp_servicestatus->last_check>=(current_time-900))
				active_checks_15min++;
			if(temp_servicestatus->last_check>=(current_time-3600))
				active_checks_1hour++;
			if(temp_servicestatus->last_check>=program_start)
				active_checks_start++;
			if(temp_servicestatus->last_check!=(time_t)0)
				active_checks_ever++;
		        }

		else{
			total_passive_checks++;

			total_percent_change_b+=temp_servicestatus->percent_state_change;
			if(have_min_percent_change_b==FALSE || temp_servicestatus->percent_state_change<min_percent_change_b){
				have_min_percent_change_b=TRUE;
				min_percent_change_b=temp_servicestatus->percent_state_change;
			        }
			if(have_max_percent_change_b==FALSE || temp_servicestatus->percent_state_change>max_percent_change_b){
				have_max_percent_change_b=TRUE;
				max_percent_change_b=temp_servicestatus->percent_state_change;
			        }

			if(temp_servicestatus->last_check>=(current_time-60))
				passive_checks_1min++;
			if(temp_servicestatus->last_check>=(current_time-300))
				passive_checks_5min++;
			if(temp_servicestatus->last_check>=(current_time-900))
				passive_checks_15min++;
			if(temp_servicestatus->last_check>=(current_time-3600))
				passive_checks_1hour++;
			if(temp_servicestatus->last_check>=program_start)
				passive_checks_start++;
			if(temp_servicestatus->last_check!=(time_t)0)
				passive_checks_ever++;
		        }
	        }

	printf("<div align=center>\n");


	printf("<DIV CLASS='dataTitle'>Program-Wide Performance Information</DIV>\n");

	printf("<table border=0 cellpadding=10>\n");

	/***** ACTIVE CHECKS *****/

	printf("<tr>\n");
	printf("<td valign=center><div class='perfTypeTitle'>Active Checks:</div></td>\n");
	printf("<td valign=top>\n");

	/* fake this so we don't divide by zero for just showing the table */
	if(total_active_checks==0)
		total_active_checks=1;

	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0>\n");
	printf("<TR><TD class='stateInfoTable1'>\n");
	printf("<TABLE BORDER=0>\n");

	printf("<tr class='data'><th class='data'>Time Frame</th><th class='data'>Checks Completed</th></tr>\n");
	printf("<tr><td class='dataVar'>&lt;= 1 minute:</td><td class='dataVal'>%d (%.1f%%)</td></tr>",active_checks_1min,(double)(((double)active_checks_1min*100.0)/(double)total_active_checks));
	printf("<tr><td class='dataVar'>&lt;= 5 minutes:</td><td class='dataVal'>%d (%.1f%%)</td></tr>",active_checks_5min,(double)(((double)active_checks_5min*100.0)/(double)total_active_checks));
	printf("<tr><td class='dataVar'>&lt;= 15 minutes:</td><td class='dataVal'>%d (%.1f%%)</td></tr>",active_checks_15min,(double)(((double)active_checks_15min*100.0)/(double)total_active_checks));
	printf("<tr><td class='dataVar'>&lt;= 1 hour:</td><td class='dataVal'>%d (%.1f%%)</td></tr>",active_checks_1hour,(double)(((double)active_checks_1hour*100.0)/(double)total_active_checks));
	printf("<tr><td class='dataVar'>Since program start:&nbsp;&nbsp;</td><td class='dataVal'>%d (%.1f%%)</td>",active_checks_start,(double)(((double)active_checks_start*100.0)/(double)total_active_checks));

	printf("</TABLE>\n");
	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	printf("</td><td valign=top>\n");

	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0>\n");
	printf("<TR><TD class='stateInfoTable2'>\n");
	printf("<TABLE BORDER=0>\n");

	printf("<tr class='data'><th class='data'>Metric</th><th class='data'>Min.</th><th class='data'>Max.</th><th class='data'>Average</th></tr>\n");
	printf("<tr><td class='dataVar'>Check Execution Time:&nbsp;&nbsp;</td><td class='dataVal'>%s%d sec</td><td class='dataVal'>%s%d sec</td><td class='dataVal'>%.3f sec</td></tr>\n",(min_execution_time==0)?"&lt; ":"",(min_execution_time==0)?1:min_execution_time,(max_execution_time==0)?"&lt; ":"",(max_execution_time==0)?1:max_execution_time,(double)((double)total_execution_time/(double)total_active_checks));
	printf("<tr><td class='dataVar'>Check Latency:</td><td class='dataVal'>%s%d sec</td><td class='dataVal'>%s%d sec</td><td class='dataVal'>%.3f sec</td></tr>\n",(min_latency==0)?"&lt; ":"",(min_latency==0)?1:min_latency,(max_latency==0)?"&lt; ":"",(max_latency==0)?1:max_latency,(double)((double)total_latency/(double)total_active_checks));
	printf("<tr><td class='dataVar'>Percent State Change:</td><td class='dataVal'>%.2f%%</td><td class='dataVal'>%.2f%%</td><td class='dataVal'>%.2f%%</td></tr>\n",min_percent_change_a,max_percent_change_a,(double)((double)total_percent_change_a/(double)total_active_checks));

	printf("</TABLE>\n");
	printf("</TD></TR>\n");
	printf("</TABLE>\n");


	printf("</td>\n");
	printf("</tr>\n");


	/***** PASSIVE CHECKS *****/

	printf("<tr>\n");
	printf("<td valign=center><div class='perfTypeTitle'>Passive Checks:</div></td>\n");
	printf("<td valign=top>\n");
	

	/* fake this so we don't divide by zero for just showing the table */
	if(total_passive_checks==0)
		total_passive_checks=1;

	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0>\n");
	printf("<TR><TD class='stateInfoTable1'>\n");
	printf("<TABLE BORDER=0>\n");

	printf("<tr class='data'><th class='data'>Time Frame</th><th class='data'>Checks Completed</th></tr>\n");
	printf("<tr><td class='dataVar'>&lt;= 1 minute:</td><td class='dataVal'>%d (%.1f%%)</td></tr>",passive_checks_1min,(double)(((double)passive_checks_1min*100.0)/(double)total_passive_checks));
	printf("<tr><td class='dataVar'>&lt;= 5 minutes:</td><td class='dataVal'>%d (%.1f%%)</td></tr>",passive_checks_5min,(double)(((double)passive_checks_5min*100.0)/(double)total_passive_checks));
	printf("<tr><td class='dataVar'>&lt;= 15 minutes:</td><td class='dataVal'>%d (%.1f%%)</td></tr>",passive_checks_15min,(double)(((double)passive_checks_15min*100.0)/(double)total_passive_checks));
	printf("<tr><td class='dataVar'>&lt;= 1 hour:</td><td class='dataVal'>%d (%.1f%%)</td></tr>",passive_checks_1hour,(double)(((double)passive_checks_1hour*100.0)/(double)total_passive_checks));
	printf("<tr><td class='dataVar'>Since program start:&nbsp;&nbsp;</td><td class='dataVal'>%d (%.1f%%)</td></tr>",passive_checks_start,(double)(((double)passive_checks_start*100.0)/(double)total_passive_checks));

	printf("</TABLE>\n");
	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	printf("</td><td valign=top>\n");

	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0>\n");
	printf("<TR><TD class='stateInfoTable2'>\n");
	printf("<TABLE BORDER=0>\n");

	printf("<tr class='data'><th class='data'>Metric</th><th class='data'>Min.</th><th class='data'>Max.</th><th class='data'>Average</th></tr>\n");
	printf("<tr><td class='dataVar'>Percent State Change:&nbsp;&nbsp;</td><td class='dataVal'>%.2f%%</td><td class='dataVal'>%.2f%%</td><td class='dataVal'>%.2f%%</td></tr>\n",min_percent_change_b,max_percent_change_b,(double)((double)total_percent_change_b/(double)total_passive_checks));

	printf("</TABLE>\n");
	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");


	printf("</div>\n");

	return;
        }



void display_comments(int type){
	host *temp_host=NULL;
	service *temp_service=NULL;
	int total_comments=0;
	int display_comment=FALSE;
	char *bg_class="";
	int odd=1;
	char date_time[MAX_DATETIME_LENGTH];
	comment *temp_comment;


	/* find the host or service */
	if(type==HOST_COMMENT){
		temp_host=find_host(host_name);
		if(temp_host==NULL)
			return;
	        }
	else{
		temp_service=find_service(host_name,service_desc);
		if(temp_service==NULL)
			return;
	        }


	printf("<A NAME=COMMENTS></A>\n");
	printf("<DIV CLASS='commentTitle'>%s Comments</DIV>\n",(type==HOST_COMMENT)?"Host":"Service");
	printf("<TABLE BORDER=0>\n");

	printf("<tr><td valign=center><img src='%s%s' border=0 align=center></td><td CLASS='comment'>",url_images_path,COMMENT_ICON);
	if(type==HOST_COMMENT)
		printf("<a href='%s?cmd_typ=%d&host=%s' CLASS='comment'>",COMMAND_CGI,CMD_ADD_HOST_COMMENT,url_encode(host_name));
	else{
		printf("<a href='%s?cmd_typ=%d&host=%s&",COMMAND_CGI,CMD_ADD_SVC_COMMENT,url_encode(host_name));
		printf("service=%s' CLASS='comment'>",url_encode(service_desc));
	        }
	printf("Add a new comment</a></td></tr>\n");

	printf("<tr><td valign=center><img src='%s%s' border=0 align=center></td><td CLASS='comment'>",url_images_path,DELETE_ICON);
	if(type==HOST_COMMENT)
		printf("<a href='%s?cmd_typ=%d&host=%s' CLASS='comment'>",COMMAND_CGI,CMD_DEL_ALL_HOST_COMMENTS,url_encode(host_name));
	else{
		printf("<a href='%s?cmd_typ=%d&host=%s&",COMMAND_CGI,CMD_DEL_ALL_SVC_COMMENTS,url_encode(host_name));
		printf("service=%s' CLASS='comment'>",url_encode(service_desc));
	        }
	printf("Delete all comments</a></td></tr>\n");

	printf("</TABLE>\n");
	printf("</DIV>\n");
	printf("</P>\n");


	printf("<P>\n");
	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CLASS='comment'>\n");
	printf("<TR CLASS='comment'><TH CLASS='comment'>Entry Time</TH><TH CLASS='comment'>Author</TH><TH CLASS='comment'>Comment</TH><TH CLASS='comment'>Comment ID</TH><TH CLASS='comment'>Persistent</TH><TH CLASS='comment'>Actions</TH></TR>\n");

	/* read in all comments */
	read_comment_data(get_cgi_config_location());

	/* check all the comments to see if they apply to this host or service */
	for(temp_comment=comment_list;temp_comment!=NULL;temp_comment=temp_comment->next){

		display_comment=FALSE;

		if(type==HOST_COMMENT && temp_comment->comment_type==HOST_COMMENT && !strcmp(temp_comment->host_name,host_name))
			display_comment=TRUE;

		else if(type==SERVICE_COMMENT && temp_comment->comment_type==SERVICE_COMMENT && !strcmp(temp_comment->host_name,host_name) && !strcmp(temp_comment->service_description,service_desc))
			display_comment=TRUE;

		if(display_comment==TRUE){

			if(odd){
				odd=0;
				bg_class="commentOdd";
			        }
			else{
				odd=1;
				bg_class="commentEven";
			        }

			get_time_string(&temp_comment->entry_time,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
			printf("<tr CLASS='%s'>",bg_class);
			printf("<td CLASS='%s'>%s</td><td CLASS='%s'>%s</td><td CLASS='%s'>%s</td><td CLASS='%s'>%d</td><td CLASS='%s'>%s</td>",bg_class,date_time,bg_class,temp_comment->author,bg_class,temp_comment->comment_data,bg_class,temp_comment->comment_id,bg_class,(temp_comment->persistent)?"Yes":"No");
			printf("<td><a href='%s?cmd_typ=%d&com_id=%d'><img src='%s%s' border=0 ALT='Delete This Comment'></td>",COMMAND_CGI,(type==HOST_COMMENT)?CMD_DEL_HOST_COMMENT:CMD_DEL_SVC_COMMENT,temp_comment->comment_id,url_images_path,DELETE_ICON);
			printf("</tr>\n");
			}
	        }

	/* see if this host or service has any comments associated with it */
	if(type==HOST_COMMENT)
		total_comments=number_of_host_comments(temp_host->name);
	else
		total_comments=number_of_service_comments(temp_service->host_name,temp_service->description);
	if(total_comments==0)
		printf("<TR CLASS='commentOdd'><TD CLASS='commentOdd' COLSPAN='%d'>This %s has no comments associated with it</TD></TR>",(type==HOST_COMMENT)?6:7,(type==HOST_COMMENT)?"host":"service");

	printf("</TABLE>\n");

	return;
        }




/* shows all service and host scheduled downtime */
void show_all_downtime(void){
	int total_downtime=0;
	char *bg_class="";
	int odd=0;
	char date_time[MAX_DATETIME_LENGTH];
	scheduled_downtime *temp_downtime;
	host *temp_host;
	service *temp_service;
	int days;
	int hours;
	int minutes;
	int seconds;


	/* read in all downtime */
	read_downtime_data(get_cgi_config_location());

	printf("<P>\n");
	printf("<DIV CLASS='downtimeNav'>[&nbsp;<A HREF='#HOSTDOWNTIME' CLASS='downtimeNav'>Host Downtime</A>&nbsp;|&nbsp;<A HREF='#SERVICEDOWNTIME' CLASS='downtimeNav'>Service Downtime</A>&nbsp;]</DIV>\n");
	printf("</P>\n");

	printf("<A NAME=HOSTDOWNTIME></A>\n");
	printf("<DIV CLASS='downtimeTitle'>Scheduled Host Downtime</DIV>\n");

	printf("<div CLASS='comment'><img src='%s%s' border=0>&nbsp;",url_images_path,DOWNTIME_ICON);
	printf("<a href='%s?cmd_typ=%d'>",COMMAND_CGI,CMD_SCHEDULE_HOST_DOWNTIME);
	printf("Schedule host downtime</a></div>\n");

	printf("<P>\n");
	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CLASS='downtime'>\n");
	printf("<TR CLASS='downtime'><TH CLASS='downtime'>Host Name</TH><TH CLASS='downtime'>Entry Time</TH><TH CLASS='downtime'>Author</TH><TH CLASS='downtime'>Comment</TH><TH CLASS='downtime'>Start Time</TH><TH CLASS='downtime'>End Time</TH><TH CLASS='downtime'>Fixed?</TH><TH CLASS='downtime'>Duration</TH><TH CLASS='downtime'>Actions</TH></TR>\n");

	/* display all the host downtime */
	for(temp_downtime=scheduled_downtime_list,total_downtime=0;temp_downtime!=NULL;temp_downtime=temp_downtime->next){

		if(temp_downtime->type!=HOST_DOWNTIME)
			continue;

		temp_host=find_host(temp_downtime->host_name);

		/* make sure the user has rights to view host information */
		if(is_authorized_for_host(temp_host,&current_authdata)==FALSE)
			continue;

		total_downtime++;

		if(odd){
			odd=0;
			bg_class="downtimeOdd";
		        }
		else{
			odd=1;
			bg_class="downtimeEven";
		        }

		printf("<tr CLASS='%s'>",bg_class);
		printf("<td CLASS='%s'><A HREF='%s?type=%d&host=%s'>%s</A></td>",bg_class,EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_downtime->host_name),temp_downtime->host_name);
		get_time_string(&temp_downtime->entry_time,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<td CLASS='%s'>%s</td>",bg_class,date_time);
		printf("<td CLASS='%s'>%s</td>",bg_class,(temp_downtime->author==NULL)?"N/A":temp_downtime->author);
		printf("<td CLASS='%s'>%s</td>",bg_class,(temp_downtime->comment==NULL)?"N/A":temp_downtime->comment);
		get_time_string(&temp_downtime->start_time,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<td CLASS='%s'>%s</td>",bg_class,date_time);
		get_time_string(&temp_downtime->end_time,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<td CLASS='%s'>%s</td>",bg_class,date_time);
		printf("<td CLASS='%s'>%s</td>",bg_class,(temp_downtime->fixed==TRUE)?"Yes":"No");
		get_time_breakdown(temp_downtime->duration,&days,&hours,&minutes,&seconds);
		printf("<td CLASS='%s'>%dd %dh %dm %ds</td>",bg_class,days,hours,minutes,seconds);
		printf("<td><a href='%s?cmd_typ=%d&down_id=%d'><img src='%s%s' border=0 ALT='Delete/Cancel This Scheduled Downtime Entry'></td>",COMMAND_CGI,CMD_DEL_HOST_DOWNTIME,temp_downtime->downtime_id,url_images_path,DELETE_ICON);
		printf("</tr>\n");
	        }

	if(total_downtime==0)
		printf("<TR CLASS='downtimeOdd'><TD CLASS='downtimeOdd' COLSPAN=9>There are no hosts with scheduled downtime</TD></TR>");

	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	printf("<P><BR></P>\n");


	printf("<A NAME=SERVICEDOWNTIME></A>\n");
	printf("<DIV CLASS='downtimeTitle'>Scheduled Service Downtime</DIV>\n");

	printf("<div CLASS='comment'><img src='%s%s' border=0>&nbsp;",url_images_path,DOWNTIME_ICON);
	printf("<a href='%s?cmd_typ=%d'>",COMMAND_CGI,CMD_SCHEDULE_SVC_DOWNTIME);
	printf("Schedule service downtime</a></div>\n");

	printf("<P>\n");
	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CLASS='downtime'>\n");
	printf("<TR CLASS='downtime'><TH CLASS='downtime'>Host Name</TH><TH CLASS='downtime'>Service</TH><TH CLASS='downtime'>Entry Time</TH><TH CLASS='downtime'>Author</TH><TH CLASS='downtime'>Comment</TH><TH CLASS='downtime'>Start Time</TH><TH CLASS='downtime'>End Time</TH><TH CLASS='downtime'>Fixed?</TH><TH CLASS='downtime'>Duration</TH><TH CLASS='downtime'>Actions</TH></TR>\n");

	/* display all the service downtime */
	for(temp_downtime=scheduled_downtime_list,total_downtime=0;temp_downtime!=NULL;temp_downtime=temp_downtime->next){

		if(temp_downtime->type!=SERVICE_DOWNTIME)
			continue;

		temp_service=find_service(temp_downtime->host_name,temp_downtime->service_description);

		/* make sure the user has rights to view service information */
		if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
			continue;

		total_downtime++;

		if(odd){
			odd=0;
			bg_class="downtimeOdd";
		        }
		else{
			odd=1;
			bg_class="downtimeEven";
		        }

		printf("<tr CLASS='%s'>",bg_class);
		printf("<td CLASS='%s'><A HREF='%s?type=%d&host=%s'>%s</A></td>",bg_class,EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_downtime->host_name),temp_downtime->host_name);
		printf("<td CLASS='%s'><A HREF='%s?type=%d&host=%s",bg_class,EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_downtime->host_name));
		printf("&service=%s'>%s</A></td>",url_encode(temp_downtime->service_description),temp_downtime->service_description);
		get_time_string(&temp_downtime->entry_time,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<td CLASS='%s'>%s</td>",bg_class,date_time);
		printf("<td CLASS='%s'>%s</td>",bg_class,(temp_downtime->author==NULL)?"N/A":temp_downtime->author);
		printf("<td CLASS='%s'>%s</td>",bg_class,(temp_downtime->comment==NULL)?"N/A":temp_downtime->comment);
		get_time_string(&temp_downtime->start_time,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<td CLASS='%s'>%s</td>",bg_class,date_time);
		get_time_string(&temp_downtime->end_time,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<td CLASS='%s'>%s</td>",bg_class,date_time);
		printf("<td CLASS='%s'>%s</td>",bg_class,(temp_downtime->fixed==TRUE)?"Yes":"No");
		get_time_breakdown(temp_downtime->duration,&days,&hours,&minutes,&seconds);
		printf("<td CLASS='%s'>%dd %dh %dm %ds</td>",bg_class,days,hours,minutes,seconds);
		printf("<td><a href='%s?cmd_typ=%d&down_id=%d'><img src='%s%s' border=0 ALT='Delete/Cancel This Scheduled Downtime Entry'></td>",COMMAND_CGI,CMD_DEL_SVC_DOWNTIME,temp_downtime->downtime_id,url_images_path,DELETE_ICON);
		printf("</tr>\n");
	        }

	if(total_downtime==0)
		printf("<TR CLASS='downtimeOdd'><TD CLASS='downtimeOdd' COLSPAN=10>There are no services with scheduled downtime</TD></TR>");

	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	return;
        }



/* shows check scheduling queue */
void show_scheduling_queue(void){
	servicesort *temp_servicesort;
	servicestatus *temp_svcstatus;
	char date_time[MAX_DATETIME_LENGTH];
	char temp_url[MAX_INPUT_BUFFER];
	int odd=0;
	char *bgclass="";


	/* make sure the user has rights to view system information */
	if(is_authorized_for_system_information(&current_authdata)==FALSE){

		printf("<P><DIV CLASS='errorMessage'>It appears as though you do not have permission to view process information...</DIV></P>\n");
		printf("<P><DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
		printf("and check the authorization options in your CGI configuration file.</DIV></P>\n");

		return;
	        }

	/* sort services */
	sort_services(sort_type,sort_option);

	printf("<DIV ALIGN=CENTER CLASS='statusSort'>Entries sorted by <b>");
	if(sort_option==SORT_HOSTNAME)
		printf("host name");
	else if(sort_option==SORT_SERVICENAME)
		printf("service name");
	else if(sort_option==SORT_SERVICESTATUS)
		printf("service status");
	else if(sort_option==SORT_LASTCHECKTIME)
		printf("last check time");
	else if(sort_option==SORT_NEXTCHECKTIME)
		printf("next check time");
	printf("</b> (%s)\n",(sort_type==SORT_ASCENDING)?"ascending":"descending");
	printf("</DIV>\n");

	printf("<P>\n");
	printf("<DIV ALIGN=CENTER>\n");
	printf("<TABLE BORDER=0 CLASS='queue'>\n");
	printf("<TR CLASS='queue'>");

	snprintf(temp_url,sizeof(temp_url)-1,"%s?type=%d",EXTINFO_CGI,DISPLAY_SCHEDULING_QUEUE);
	temp_url[sizeof(temp_url)-1]='\x0';

	printf("<TH CLASS='queue'>Host&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by host name (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by host name (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_HOSTNAME,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_HOSTNAME,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='queue'>Service&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by service name (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by service name (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_SERVICENAME,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_SERVICENAME,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='queue'>Last Check&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by last check time (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by last check time (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_LASTCHECKTIME,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_LASTCHECKTIME,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='queue'>Next Check&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by next check time (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by next check time (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_NEXTCHECKTIME,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_NEXTCHECKTIME,url_images_path,DOWN_ARROW_ICON);


	printf("<TH CLASS='queue'>Active Checks</TH><TH CLASS='queue'>Actions</TH></TR>\n");


	/* display all services */
	for(temp_servicesort=servicesort_list;temp_servicesort!=NULL;temp_servicesort=temp_servicesort->next){
		
		/* get the service status */
		temp_svcstatus=temp_servicesort->svcstatus;

		if(odd){
			odd=0;
			bgclass="Even";
		        }
		else{
			odd=1;
			bgclass="Odd";
		        }

		printf("<TR CLASS='queue%s'>",bgclass);

		printf("<TD CLASS='queue%s'><A HREF='%s?type=%d&host=%s'>%s</A></TD>",bgclass,EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_svcstatus->host_name),temp_svcstatus->host_name);

		printf("<TD CLASS='queue%s'><A HREF='%s?type=%d&host=%s",bgclass,EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_svcstatus->host_name));
		printf("&service=%s'>%s</A></TD>",url_encode(temp_svcstatus->description),temp_svcstatus->description);

		get_time_string(&temp_svcstatus->last_check,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TD CLASS='queue%s'>%s</TD>",bgclass,(temp_svcstatus->last_check==(time_t)0)?"N/A":date_time);

		get_time_string(&temp_svcstatus->next_check,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
		printf("<TD CLASS='queue%s'>%s</TD>",bgclass,(temp_svcstatus->next_check==(time_t)0)?"N/A":date_time);

		printf("<TD CLASS='queue%s'>%s</TD>",(temp_svcstatus->checks_enabled==TRUE)?"ENABLED":"DISABLED",(temp_svcstatus->checks_enabled==TRUE)?"ENABLED":"DISABLED");

		printf("<TD CLASS='queue%s'>",bgclass);
		/*
		printf("<TABLE BORDER=0 CELLSPACING=0 CLASS='commands'>\n");
		printf("<TR CLASS='queue%s'>\n",bgclass);
		*/
		if(temp_svcstatus->checks_enabled==TRUE){
			printf("<a href='%s?cmd_typ=%d&host=%s",COMMAND_CGI,CMD_DISABLE_SVC_CHECK,url_encode(temp_svcstatus->host_name));
			printf("&service=%s'><img src='%s%s' border=0 ALT='Disable Active Checks Of This Service'></a>\n",url_encode(temp_svcstatus->description),url_images_path,DISABLED_ICON);
		        }
		else{
			printf("<a href='%s?cmd_typ=%d&host=%s",COMMAND_CGI,CMD_ENABLE_SVC_CHECK,url_encode(temp_svcstatus->host_name));
			printf("&service=%s'><img src='%s%s' border=0 ALT='Enable Active Checks Of This Service'></a>\n",url_encode(temp_svcstatus->description),url_images_path,ENABLED_ICON);
		        }
		printf("<a href='%s?cmd_typ=%d&host=%s",COMMAND_CGI,CMD_DELAY_SVC_CHECK,url_encode(temp_svcstatus->host_name));
		printf("&service=%s'><img src='%s%s' border=0 ALT='Re-schedule This Service Check'></a>\n",url_encode(temp_svcstatus->description),url_images_path,DELAY_ICON);
		/*
		printf("</TR>\n");
		printf("</TABLE>\n");
		*/
		printf("</TD>\n");

		printf("</TR>\n");

	        }

	printf("</TABLE>\n");
	printf("</DIV>\n");
	printf("</P>\n");


	/* free memory allocated to sorted service list */
	free_servicesort_list();

	return;
        }



/* sorts the service list */
int sort_services(int s_type, int s_option){
	servicesort *new_servicesort;
	servicesort *last_servicesort;
	servicesort *temp_servicesort;
	servicestatus *temp_svcstatus;

	if(s_type==SORT_NONE)
		return ERROR;

	if(servicestatus_list==NULL)
		return ERROR;

	/* sort all services status entries */
	for(temp_svcstatus=servicestatus_list;temp_svcstatus!=NULL;temp_svcstatus=temp_svcstatus->next){

		/* allocate memory for a new sort structure */
		new_servicesort=(servicesort *)malloc(sizeof(servicesort));
		if(new_servicesort==NULL)
			return ERROR;

		new_servicesort->svcstatus=temp_svcstatus;

		last_servicesort=servicesort_list;
		for(temp_servicesort=servicesort_list;temp_servicesort!=NULL;temp_servicesort=temp_servicesort->next){

			if(compare_servicesort_entries(s_type,s_option,new_servicesort,temp_servicesort)==TRUE){
				new_servicesort->next=temp_servicesort;
				if(temp_servicesort==servicesort_list)
					servicesort_list=new_servicesort;
				else
					last_servicesort->next=new_servicesort;
				break;
		                }
			else
				last_servicesort=temp_servicesort;
	                }

		if(servicesort_list==NULL){
			new_servicesort->next=NULL;
			servicesort_list=new_servicesort;
	                }
		else if(temp_servicesort==NULL){
			new_servicesort->next=NULL;
			last_servicesort->next=new_servicesort;
	                }
	        }

	return OK;
        }


int compare_servicesort_entries(int s_type, int s_option, servicesort *new_servicesort, servicesort *temp_servicesort){
	servicestatus *new_svcstatus;
	servicestatus *temp_svcstatus;

	new_svcstatus=new_servicesort->svcstatus;
	temp_svcstatus=temp_servicesort->svcstatus;

	if(s_type==SORT_ASCENDING){

		if(s_option==SORT_LASTCHECKTIME){
			if(new_svcstatus->last_check < temp_svcstatus->last_check)
				return TRUE;
			else
				return FALSE;
		        }
		if(s_option==SORT_NEXTCHECKTIME){
			if(new_svcstatus->next_check < temp_svcstatus->next_check)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_CURRENTATTEMPT){
			if(new_svcstatus->current_attempt < temp_svcstatus->current_attempt)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_SERVICESTATUS){
			if(new_svcstatus->status <= temp_svcstatus->status)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_HOSTNAME){
			if(strcasecmp(new_svcstatus->host_name,temp_svcstatus->host_name)<0)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_SERVICENAME){
			if(strcasecmp(new_svcstatus->description,temp_svcstatus->description)<0)
				return TRUE;
			else
				return FALSE;
		        }
	        }
	else{
		if(s_option==SORT_LASTCHECKTIME){
			if(new_svcstatus->last_check > temp_svcstatus->last_check)
				return TRUE;
			else
				return FALSE;
		        }
		if(s_option==SORT_NEXTCHECKTIME){
			if(new_svcstatus->next_check > temp_svcstatus->next_check)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_CURRENTATTEMPT){
			if(new_svcstatus->current_attempt > temp_svcstatus->current_attempt)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_SERVICESTATUS){
			if(new_svcstatus->status > temp_svcstatus->status)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_HOSTNAME){
			if(strcasecmp(new_svcstatus->host_name,temp_svcstatus->host_name)>0)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_SERVICENAME){
			if(strcasecmp(new_svcstatus->description,temp_svcstatus->description)>0)
				return TRUE;
			else
				return FALSE;
		        }
	        }

	return TRUE;
        }



/* free all memory allocated to the servicesort structures */
void free_servicesort_list(void){
	servicesort *this_servicesort;
	servicesort *next_servicesort;

	/* free memory for the servicesort list */
	for(this_servicesort=servicesort_list;this_servicesort!=NULL;this_servicesort=next_servicesort){
		next_servicesort=this_servicesort->next;
		free(this_servicesort);
	        }

	return;
        }

