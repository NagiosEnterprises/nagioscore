/**************************************************************************
 *
 * STATUS.C -  Nagios Status CGI
 *
 * Copyright (c) 1999-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 06-19-2003
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
#include "../common/statusdata.h"

#include "cgiutils.h"
#include "getcgi.h"
#include "auth.h"

extern int             refresh_rate;
extern time_t          program_start;

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_html_path[MAX_FILENAME_LENGTH];
extern char url_docs_path[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char url_stylesheets_path[MAX_FILENAME_LENGTH];
extern char url_logo_images_path[MAX_FILENAME_LENGTH];
extern char url_media_path[MAX_FILENAME_LENGTH];
extern char log_file[MAX_FILENAME_LENGTH];

extern char *service_critical_sound;
extern char *service_warning_sound;
extern char *service_unknown_sound;
extern char *host_down_sound;
extern char *host_unreachable_sound;
extern char *normal_sound;

extern int suppress_alert_window;

extern host *host_list;
extern service *service_list;
extern hostgroup *hostgroup_list;
extern servicegroup *servicegroup_list;
extern hoststatus *hoststatus_list;
extern servicestatus *servicestatus_list;


#define MAX_MESSAGE_BUFFER		4096

#define DISPLAY_HOSTS			0
#define DISPLAY_HOSTGROUPS		1
#define DISPLAY_SERVICEGROUPS           2

#define STYLE_OVERVIEW			0
#define STYLE_DETAIL			1
#define STYLE_SUMMARY			2
#define STYLE_GRID                      3
#define STYLE_HOST_DETAIL               4

/* HOSTSORT structure */
typedef struct hostsort_struct{
	hoststatus *hststatus;
	struct hostsort_struct *next;
        }hostsort;

/* SERVICESORT structure */
typedef struct servicesort_struct{
	servicestatus *svcstatus;
	struct servicesort_struct *next;
        }servicesort;

hostsort *hostsort_list=NULL;
servicesort *servicesort_list=NULL;

int sort_services(int,int);						/* sorts services */
int sort_hosts(int,int);                                                /* sorts hosts */
int compare_servicesort_entries(int,int,servicesort *,servicesort *);	/* compares service sort entries */
int compare_hostsort_entries(int,int,hostsort *,hostsort *);            /* compares host sort entries */
void free_servicesort_list(void);
void free_hostsort_list(void);

void show_host_status_totals(void);
void show_service_status_totals(void);
void show_service_detail(void);
void show_host_detail(void);
void show_servicegroup_overviews(void);
void show_servicegroup_overview(servicegroup *);
void show_servicegroup_summaries(void);
void show_servicegroup_summary(servicegroup *,int);
void show_servicegroup_host_totals_summary(servicegroup *);
void show_servicegroup_service_totals_summary(servicegroup *);
void show_servicegroup_grids(void);
void show_servicegroup_grid(servicegroup *);
void show_hostgroup_overviews(void);
void show_hostgroup_overview(hostgroup *);
void show_servicegroup_hostgroup_member_overview(hoststatus *,int,void *);
void show_servicegroup_hostgroup_member_service_status_totals(char *,void *);
void show_hostgroup_summaries(void);
void show_hostgroup_summary(hostgroup *,int);
void show_hostgroup_host_totals_summary(hostgroup *);
void show_hostgroup_service_totals_summary(hostgroup *);
void show_hostgroup_grids(void);
void show_hostgroup_grid(hostgroup *);

void show_filters(void);

int passes_host_properties_filter(hoststatus *);
int passes_service_properties_filter(servicestatus *);

void document_header(int);
void document_footer(void);
int process_cgivars(void);


authdata current_authdata;
time_t current_time;

char alert_message[MAX_MESSAGE_BUFFER];
char *host_name=NULL;
char *hostgroup_name=NULL;
char *servicegroup_name=NULL;
int host_alert=FALSE;
int show_all_hosts=TRUE;
int show_all_hostgroups=TRUE;
int show_all_servicegroups=TRUE;
int display_type=DISPLAY_HOSTS;
int overview_columns=3;
int max_grid_width=8;
int group_style_type=STYLE_OVERVIEW;

int service_status_types=SERVICE_PENDING|SERVICE_OK|SERVICE_UNKNOWN|SERVICE_WARNING|SERVICE_CRITICAL;
int all_service_status_types=SERVICE_PENDING|SERVICE_OK|SERVICE_UNKNOWN|SERVICE_WARNING|SERVICE_CRITICAL;

int host_status_types=HOST_PENDING|HOST_UP|HOST_DOWN|HOST_UNREACHABLE;
int all_host_status_types=HOST_PENDING|HOST_UP|HOST_DOWN|HOST_UNREACHABLE;

int all_service_problems=SERVICE_UNKNOWN|SERVICE_WARNING|SERVICE_CRITICAL;
int all_host_problems=HOST_DOWN|HOST_UNREACHABLE;

unsigned long host_properties=0L;
unsigned long service_properties=0L;




int sort_type=SORT_NONE;
int sort_option=SORT_HOSTNAME;

int problem_hosts_down=0;
int problem_hosts_unreachable=0;
int problem_services_critical=0;
int problem_services_warning=0;
int problem_services_unknown=0;

int embedded=FALSE;
int display_header=TRUE;



int main(void){
	int result=OK;
	char *sound=NULL;
	
	time(&current_time);

	/* get the arguments passed in the URL */
	process_cgivars();

	/* reset internal variables */
	reset_cgi_vars();

	/* read the CGI configuration file */
	result=read_cgi_config_file(get_cgi_config_location());
	if(result==ERROR){
		document_header(FALSE);
		cgi_config_file_error(get_cgi_config_location());
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

	/* read in all host and service comments */
	read_comment_data(get_cgi_config_location());

	/* get authentication information */
	get_authentication_information(&current_authdata);

	if(display_header==TRUE){

		/* begin top table */
		printf("<table border=0 width=100%% cellspacing=0 cellpadding=0>\n");
		printf("<tr>\n");

		/* left column of the first row */
		printf("<td align=left valign=top width=33%%>\n");

		/* info table */
		display_info_table("Current Network Status",TRUE,&current_authdata);

		printf("<TABLE BORDER=1 CELLPADDING=0 CELLSPACING=0 CLASS='linkBox'>\n");
		printf("<TR><TD CLASS='linkBox'>\n");

		if(display_type==DISPLAY_HOSTS){
			printf("<a href='%s?host=%s'>View History For %s</a><br>\n",HISTORY_CGI,(show_all_hosts==TRUE)?"all":url_encode(host_name),(show_all_hosts==TRUE)?"all hosts":"This Host");
			printf("<a href='%s?host=%s'>View Notifications For %s</a>\n",NOTIFICATIONS_CGI,(show_all_hosts==TRUE)?"all":url_encode(host_name),(show_all_hosts==TRUE)?"All Hosts":"This Host");
			if(show_all_hosts==FALSE)
				printf("<br><a href='%s?host=all'>View Service Status Detail For All Hosts</a>\n",STATUS_CGI);
			else
				printf("<br><a href='%s?hostgroup=all&style=hostdetail'>View Host Status Detail For All Hosts</a>\n",STATUS_CGI);
	                }
		else if(display_type==DISPLAY_SERVICEGROUPS){
			if(show_all_servicegroups==FALSE){

				if(group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_GRID || group_style_type==STYLE_SUMMARY)
					printf("<a href='%s?servicegroup=%s&style=detail'>View Service Status Detail For This Service Group</a><br>\n",STATUS_CGI,url_encode(servicegroup_name));
				if(group_style_type==STYLE_DETAIL || group_style_type==STYLE_GRID || group_style_type==STYLE_SUMMARY)
					printf("<a href='%s?servicegroup=%s&style=overview'>View Status Overview For This Service Group</a><br>\n",STATUS_CGI,url_encode(servicegroup_name));
				if(group_style_type==STYLE_DETAIL || group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_GRID)
					printf("<a href='%s?servicegroup=%s&style=summary'>View Status Summary For This Service Group</a><br>\n",STATUS_CGI,url_encode(servicegroup_name));
				if(group_style_type==STYLE_DETAIL || group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_SUMMARY)
					printf("<a href='%s?servicegroup=%s&style=grid'>View Service Status Grid For This Service Group</a><br>\n",STATUS_CGI,url_encode(servicegroup_name));

				if(group_style_type==STYLE_DETAIL)
					printf("<a href='%s?servicegroup=all&style=detail'>View Service Status Detail For All Service Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_OVERVIEW)
					printf("<a href='%s?servicegroup&style=overview'>View Status Overview For All Service Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_SUMMARY)
					printf("<a href='%s?servicegroup&style=summary'>View Status Summary For All Service Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_GRID)
					printf("<a href='%s?servicegroup=all&style=grid'>View Service Status Grid For All Service Groups</a><br>\n",STATUS_CGI);

			        }
			else{
				if(group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_GRID || group_style_type==STYLE_SUMMARY)
					printf("<a href='%s?servicegroup=all&style=detail'>View Service Status Detail For All Service Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_DETAIL || group_style_type==STYLE_GRID || group_style_type==STYLE_SUMMARY)
					printf("<a href='%s?servicegroup=all&style=overview'>View Status Overview For All Service Groups</a><br>\n",STATUS_CGI,url_encode(servicegroup_name));
				if(group_style_type==STYLE_DETAIL || group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_GRID)
					printf("<a href='%s?servicegroup=all&style=summary'>View Status Summary For All Service Groups</a><br>\n",STATUS_CGI,url_encode(servicegroup_name));
				if(group_style_type==STYLE_DETAIL || group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_SUMMARY)
					printf("<a href='%s?servicegroup=all&style=grid'>View Service Status Grid For All Service Groups</a><br>\n",STATUS_CGI);
			        }
		
		        }
		else{
			if(show_all_hostgroups==FALSE){

				if(group_style_type==STYLE_DETAIL)
					printf("<a href='%s?hostgroup=all&style=detail'>View Service Status Detail For All Host Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_HOST_DETAIL)
					printf("<a href='%s?hostgroup=all&style=hostdetail'>View Host Status Detail For All Host Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_OVERVIEW)
					printf("<a href='%s?hostgroup=all&style=overview'>View Status Overview For All Host Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_SUMMARY)
					printf("<a href='%s?hostgroup=all&style=summary'>View Status Summary For All Host Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_GRID)
					printf("<a href='%s?hostgroup=all&style=grid'>View Status Grid For All Host Groups</a><br>\n",STATUS_CGI);

				if(group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_SUMMARY || group_style_type==STYLE_GRID || group_style_type==STYLE_HOST_DETAIL)
					printf("<a href='%s?hostgroup=%s&style=detail'>View Service Status Detail For This Host Group</a><br>\n",STATUS_CGI,url_encode(hostgroup_name));
				if(group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_DETAIL || group_style_type==STYLE_SUMMARY || group_style_type==STYLE_GRID)
					printf("<a href='%s?hostgroup=%s&style=hostdetail'>View Host Status Detail For This Host Group</a><br>\n",STATUS_CGI,url_encode(hostgroup_name));
				if(group_style_type==STYLE_DETAIL || group_style_type==STYLE_SUMMARY || group_style_type==STYLE_GRID || group_style_type==STYLE_HOST_DETAIL)
					printf("<a href='%s?hostgroup=%s&style=overview'>View Status Overview For This Host Group</a><br>\n",STATUS_CGI,url_encode(hostgroup_name));
				if(group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_DETAIL || group_style_type==STYLE_GRID || group_style_type==STYLE_HOST_DETAIL)
					printf("<a href='%s?hostgroup=%s&style=summary'>View Status Summary For This Host Group</a><br>\n",STATUS_CGI,url_encode(hostgroup_name));
				if(group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_DETAIL || group_style_type==STYLE_SUMMARY || group_style_type==STYLE_HOST_DETAIL)
					printf("<a href='%s?hostgroup=%s&style=grid'>View Status Grid For This Host Group</a><br>\n",STATUS_CGI,url_encode(hostgroup_name));
		                }
			else{
				if(group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_SUMMARY || group_style_type==STYLE_GRID || group_style_type==STYLE_HOST_DETAIL)
					printf("<a href='%s?hostgroup=all&style=detail'>View Service Status Detail For All Host Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_DETAIL || group_style_type==STYLE_SUMMARY || group_style_type==STYLE_GRID)
					printf("<a href='%s?hostgroup=all&style=hostdetail'>View Host Status Detail For All Host Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_DETAIL || group_style_type==STYLE_SUMMARY || group_style_type==STYLE_GRID || group_style_type==STYLE_HOST_DETAIL)
					printf("<a href='%s?hostgroup=all&style=overview'>View Status Overview For All Host Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_DETAIL || group_style_type==STYLE_GRID || group_style_type==STYLE_HOST_DETAIL)
					printf("<a href='%s?hostgroup=all&style=summary'>View Status Summary For All Host Groups</a><br>\n",STATUS_CGI);
				if(group_style_type==STYLE_OVERVIEW || group_style_type==STYLE_DETAIL || group_style_type==STYLE_SUMMARY || group_style_type==STYLE_HOST_DETAIL)
					printf("<a href='%s?hostgroup=all&style=grid'>View Status Grid For All Host Groups</a><br>\n",STATUS_CGI);
		                }
	                }

		printf("</TD></TR>\n");
		printf("</TABLE>\n");

		printf("</td>\n");

		/* middle column of top row */
		printf("<td align=center valign=top width=33%%>\n");
		show_host_status_totals();
		printf("</td>\n");

		/* right hand column of top row */
		printf("<td align=center valign=top width=33%%>\n");
		show_service_status_totals();
		printf("</td>\n");

		/* display context-sensitive help */
		printf("<td align=right valign=bottom>\n");
		if(display_type==DISPLAY_HOSTS)
			display_context_help(CONTEXTHELP_STATUS_DETAIL);
		else if(display_type==DISPLAY_SERVICEGROUPS){
			if(group_style_type==STYLE_HOST_DETAIL)
				display_context_help(CONTEXTHELP_STATUS_DETAIL);
			else if(group_style_type==STYLE_OVERVIEW)
				display_context_help(CONTEXTHELP_STATUS_SGOVERVIEW);
			else if(group_style_type==STYLE_SUMMARY)
				display_context_help(CONTEXTHELP_STATUS_SGSUMMARY);
			else if(group_style_type==STYLE_GRID)
				display_context_help(CONTEXTHELP_STATUS_SGGRID);
		        }
		else{
			if(group_style_type==STYLE_HOST_DETAIL)
				display_context_help(CONTEXTHELP_STATUS_HOST_DETAIL);
			else if(group_style_type==STYLE_OVERVIEW)
				display_context_help(CONTEXTHELP_STATUS_HGOVERVIEW);
			else if(group_style_type==STYLE_SUMMARY)
				display_context_help(CONTEXTHELP_STATUS_HGSUMMARY);
			else if(group_style_type==STYLE_GRID)
				display_context_help(CONTEXTHELP_STATUS_HGGRID);
		        }
		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");
	        }


	/* embed sound tag if necessary... */
	if(problem_hosts_unreachable>0 && host_unreachable_sound!=NULL)
		sound=host_unreachable_sound;
	else if(problem_hosts_down>0 && host_down_sound!=NULL)
		sound=host_down_sound;
	else if(problem_services_critical>0 && service_critical_sound!=NULL)
		sound=service_critical_sound;
	else if(problem_services_warning>0 && service_warning_sound!=NULL)
		sound=service_warning_sound;
	else if(problem_services_unknown>0 && service_unknown_sound!=NULL)
		sound=service_unknown_sound;
	else if(problem_services_unknown==0 && problem_services_warning==0 && problem_services_critical==0 && problem_hosts_down==0 && problem_hosts_unreachable==0 && normal_sound!=NULL)
		sound=normal_sound;
	if(sound!=NULL)
		printf("<EMBED SRC='%s%s' HIDDEN=TRUE AUTOSTART=TRUE>",url_media_path,sound);


	/* bottom portion of screen - service or hostgroup detail */
	if(display_type==DISPLAY_HOSTS)
		show_service_detail();
	else if(display_type==DISPLAY_SERVICEGROUPS){
		if(group_style_type==STYLE_OVERVIEW)
			show_servicegroup_overviews();
		else if(group_style_type==STYLE_SUMMARY)
			show_servicegroup_summaries();
		else if(group_style_type==STYLE_GRID)
			show_servicegroup_grids();
		else
			show_service_detail();
	        }
	else{
		if(group_style_type==STYLE_OVERVIEW)
			show_hostgroup_overviews();
		else if(group_style_type==STYLE_SUMMARY)
			show_hostgroup_summaries();
		else if(group_style_type==STYLE_GRID)
			show_hostgroup_grids();
		else if(group_style_type==STYLE_HOST_DETAIL)
			show_host_detail();
		else
			show_service_detail();
	        }

	document_footer();

	/* free all allocated memory */
	free_memory();
	free_extended_data();
	free_comment_data();

	/* free memory allocated to the sort lists */
	free_servicesort_list();
	free_hostsort_list();

	return OK;
        }


void document_header(int use_stylesheet){
	char date_time[MAX_DATETIME_LENGTH];
	time_t expire_time;

	printf("Cache-Control: no-store\n");
	printf("Pragma: no-cache\n");
	printf("Refresh: %d\n",refresh_rate);

	get_time_string(&current_time,date_time,(int)sizeof(date_time),HTTP_DATE_TIME);
	printf("Last-Modified: %s\n",date_time);

	expire_time=(time_t)0L;
	get_time_string(&expire_time,date_time,(int)sizeof(date_time),HTTP_DATE_TIME);
	printf("Expires: %s\n",date_time);

	printf("Content-type: text/html\n\n");

	if(embedded==TRUE)
		return;

	printf("<html>\n");
	printf("<head>\n");
	printf("<title>\n");
	printf("Current Network Status\n");
	printf("</title>\n");

	if(use_stylesheet==TRUE)
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>",url_stylesheets_path,STATUS_CSS);

	printf("</head>\n");

	printf("<body CLASS='status'>\n");

	/* include user SSI header */
	include_ssi_files(STATUS_CGI,SSI_HEADER);

	return;
        }


void document_footer(void){

	if(embedded==TRUE)
		return;

	/* include user SSI footer */
	include_ssi_files(STATUS_CGI,SSI_FOOTER);

	printf("</body>\n");
	printf("</html>\n");

	return;
        }


int process_cgivars(void){
	char **variables;
	int error=FALSE;
	int x;

	variables=getcgivars();

	for(x=0;variables[x]!=NULL;x++){

		/* do some basic length checking on the variable identifier to prevent buffer overflows */
		if(strlen(variables[x])>=MAX_INPUT_BUFFER-1){
			x++;
			continue;
		        }

		/* we found the hostgroup argument */
		else if(!strcmp(variables[x],"hostgroup")){
			display_type=DISPLAY_HOSTGROUPS;
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			hostgroup_name=strdup(variables[x]);

			if(hostgroup_name!=NULL && !strcmp(hostgroup_name,"all"))
				show_all_hostgroups=TRUE;
			else
				show_all_hostgroups=FALSE;
		        }

		/* we found the servicegroup argument */
		else if(!strcmp(variables[x],"servicegroup")){
			display_type=DISPLAY_SERVICEGROUPS;
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			servicegroup_name=strdup(variables[x]);

			if(servicegroup_name!=NULL && !strcmp(servicegroup_name,"all"))
				show_all_servicegroups=TRUE;
			else
				show_all_servicegroups=FALSE;
		        }

		/* we found the host argument */
		else if(!strcmp(variables[x],"host")){
			display_type=DISPLAY_HOSTS;
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			host_name=strdup(variables[x]);

			if(host_name!=NULL && !strcmp(host_name,"all"))
				show_all_hosts=TRUE;
			else
				show_all_hosts=FALSE;
		        }

		/* we found the columns argument */
		else if(!strcmp(variables[x],"columns")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			overview_columns=atoi(variables[x]);
			if(overview_columns<=0)
				overview_columns=1;
		        }

		/* we found the service status type argument */
		else if(!strcmp(variables[x],"servicestatustypes")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			service_status_types=atoi(variables[x]);
		        }

		/* we found the host status type argument */
		else if(!strcmp(variables[x],"hoststatustypes")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			host_status_types=atoi(variables[x]);
		        }

		/* we found the service properties argument */
		else if(!strcmp(variables[x],"serviceprops")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			service_properties=strtoul(variables[x],NULL,10);
		        }

		/* we found the host properties argument */
		else if(!strcmp(variables[x],"hostprops")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			host_properties=strtoul(variables[x],NULL,10);
		        }

		/* we found the host or service group style argument */
		else if(!strcmp(variables[x],"style")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			if(!strcmp(variables[x],"overview"))
				group_style_type=STYLE_OVERVIEW;
			else if(!strcmp(variables[x],"detail"))
				group_style_type=STYLE_DETAIL;
			else if(!strcmp(variables[x],"summary"))
				group_style_type=STYLE_SUMMARY;
			else if(!strcmp(variables[x],"grid"))
				group_style_type=STYLE_GRID;
			else if(!strcmp(variables[x],"hostdetail"))
				group_style_type=STYLE_HOST_DETAIL;
			else
				group_style_type=STYLE_DETAIL;
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



/* display table with service status totals... */
void show_service_status_totals(void){
	int total_ok=0;
	int total_warning=0;
	int total_unknown=0;
	int total_critical=0;
	int total_pending=0;
	int total_services=0;
	int total_problems=0;
	servicestatus *temp_servicestatus;
	service *temp_service;
	host *temp_host;
	int count_service;


	/* check the status of all services... */
	for(temp_servicestatus=servicestatus_list;temp_servicestatus!=NULL;temp_servicestatus=temp_servicestatus->next){

		/* find the host and service... */
		temp_host=find_host(temp_servicestatus->host_name);
		temp_service=find_service(temp_servicestatus->host_name,temp_servicestatus->description);

		/* make sure user has rights to see this service... */
		if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
			continue;

		count_service=0;

		if(display_type==DISPLAY_HOSTS && (show_all_hosts==TRUE || !strcmp(host_name,temp_servicestatus->host_name)))
			count_service=1;
		else if(display_type==DISPLAY_SERVICEGROUPS && (show_all_servicegroups==TRUE || (is_service_member_of_servicegroup(find_servicegroup(servicegroup_name),temp_service)==TRUE)))
			count_service=1;
		else if(display_type==DISPLAY_HOSTGROUPS && (show_all_hostgroups==TRUE || (is_host_member_of_hostgroup(find_hostgroup(hostgroup_name),temp_host)==TRUE)))
			count_service=1;

		if(count_service){

			if(temp_servicestatus->status==SERVICE_CRITICAL){
				total_critical++;
				if(temp_servicestatus->problem_has_been_acknowledged==FALSE && temp_servicestatus->checks_enabled==TRUE && temp_servicestatus->notifications_enabled==TRUE && temp_servicestatus->scheduled_downtime_depth==0)
					problem_services_critical++;
			        }
			else if(temp_servicestatus->status==SERVICE_WARNING){
				total_warning++;
				if(temp_servicestatus->problem_has_been_acknowledged==FALSE && temp_servicestatus->checks_enabled==TRUE && temp_servicestatus->notifications_enabled==TRUE && temp_servicestatus->scheduled_downtime_depth==0)
					problem_services_warning++;
			        }
			else if(temp_servicestatus->status==SERVICE_UNKNOWN){
				total_unknown++;
				if(temp_servicestatus->problem_has_been_acknowledged==FALSE && temp_servicestatus->checks_enabled==TRUE && temp_servicestatus->notifications_enabled==TRUE && temp_servicestatus->scheduled_downtime_depth==0)
					problem_services_unknown++;
			        }
			else if(temp_servicestatus->status==SERVICE_OK)
				total_ok++;
			else if(temp_servicestatus->status==SERVICE_PENDING)
				total_pending++;
			else
				total_ok++;
		        }
	        }

	total_services=total_ok+total_unknown+total_warning+total_critical+total_pending;
	total_problems=total_unknown+total_warning+total_critical;


	printf("<DIV CLASS='serviceTotals'>Service Status Totals</DIV>\n");

	printf("<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=0>\n");
	printf("<TR><TD>\n");

	printf("<TABLE BORDER=1 CLASS='serviceTotals'>\n");
	printf("<TR>\n");

	printf("<TH CLASS='serviceTotals'>");
	printf("<A CLASS='serviceTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s&style=detail",servicegroup_name);
	else
		printf("hostgroup=%s&style=detail",hostgroup_name);
	printf("&servicestatustypes=%d",SERVICE_OK);
	printf("&hoststatustypes=%d'>",host_status_types);
	printf("Ok</A></TH>\n");

	printf("<TH CLASS='serviceTotals'>");
	printf("<A CLASS='serviceTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s&style=detail",servicegroup_name);
	else
		printf("hostgroup=%s&style=detail",hostgroup_name);
	printf("&servicestatustypes=%d",SERVICE_WARNING);
	printf("&hoststatustypes=%d'>",host_status_types);
	printf("Warning</A></TH>\n");

	printf("<TH CLASS='serviceTotals'>");
	printf("<A CLASS='serviceTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s&style=detail",servicegroup_name);
	else
		printf("hostgroup=%s&style=detail",hostgroup_name);
	printf("&servicestatustypes=%d",SERVICE_UNKNOWN);
	printf("&hoststatustypes=%d'>",host_status_types);
	printf("Unknown</A></TH>\n");

	printf("<TH CLASS='serviceTotals'>");
	printf("<A CLASS='serviceTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s&style=detail",servicegroup_name);
	else
		printf("hostgroup=%s&style=detail",hostgroup_name);
	printf("&servicestatustypes=%d",SERVICE_CRITICAL);
	printf("&hoststatustypes=%d'>",host_status_types);
	printf("Critical</A></TH>\n");

	printf("<TH CLASS='serviceTotals'>");
	printf("<A CLASS='serviceTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s&style=detail",servicegroup_name);
	else
		printf("hostgroup=%s&style=detail",hostgroup_name);
	printf("&servicestatustypes=%d",SERVICE_PENDING);
	printf("&hoststatustypes=%d'>",host_status_types);
	printf("Pending</A></TH>\n");

	printf("</TR>\n");

	printf("<TR>\n");


	/* total services ok */
	printf("<TD CLASS='serviceTotals%s'>%d</TD>\n",(total_ok>0)?"OK":"",total_ok);

	/* total services in warning state */
	printf("<TD CLASS='serviceTotals%s'>%d</TD>\n",(total_warning>0)?"WARNING":"",total_warning);

	/* total services in unknown state */
	printf("<TD CLASS='serviceTotals%s'>%d</TD>\n",(total_unknown>0)?"UNKNOWN":"",total_unknown);

	/* total services in critical state */
	printf("<TD CLASS='serviceTotals%s'>%d</TD>\n",(total_critical>0)?"CRITICAL":"",total_critical);

	/* total services in pending state */
	printf("<TD CLASS='serviceTotals%s'>%d</TD>\n",(total_pending>0)?"PENDING":"",total_pending);


	printf("</TR>\n");
	printf("</TABLE>\n");

	printf("</TD></TR><TR><TD ALIGN=CENTER>\n");

	printf("<TABLE BORDER=1 CLASS='serviceTotals'>\n");
	printf("<TR>\n");

	printf("<TH CLASS='serviceTotals'>");
	printf("<A CLASS='serviceTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s&style=detail",servicegroup_name);
	else
		printf("hostgroup=%s&style=detail",hostgroup_name);
	printf("&servicestatustypes=%d",SERVICE_UNKNOWN|SERVICE_WARNING|SERVICE_CRITICAL);
	printf("&hoststatustypes=%d'>",host_status_types);
	printf("<I>All Problems</I></A></TH>\n");

	printf("<TH CLASS='serviceTotals'>");
	printf("<A CLASS='serviceTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s&style=detail",servicegroup_name);
	else
		printf("hostgroup=%s&style=detail",hostgroup_name);
	printf("&hoststatustypes=%d'>",host_status_types);
	printf("<I>All Types</I></A></TH>\n");


	printf("</TR><TR>\n");

	/* total service problems */
	printf("<TD CLASS='serviceTotals%s'>%d</TD>\n",(total_problems>0)?"PROBLEMS":"",total_problems);

	/* total services */
	printf("<TD CLASS='serviceTotals'>%d</TD>\n",total_services);

	printf("</TR>\n");
	printf("</TABLE>\n");

	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	printf("</DIV>\n");

	return;
        }


/* display a table with host status totals... */
void show_host_status_totals(void){
	int total_up=0;
	int total_down=0;
	int total_unreachable=0;
	int total_pending=0;
	int total_hosts=0;
	int total_problems=0;
	hoststatus *temp_hoststatus;
	host *temp_host;
	servicestatus *temp_servicestatus;
	service *temp_service;
	int count_host;


	/* check the status of all hosts... */
	for(temp_hoststatus=hoststatus_list;temp_hoststatus!=NULL;temp_hoststatus=temp_hoststatus->next){

		/* find the host... */
		temp_host=find_host(temp_hoststatus->host_name);

		/* make sure user has rights to view this host */
		if(is_authorized_for_host(temp_host,&current_authdata)==FALSE)
			continue;

		count_host=0;

		if(display_type==DISPLAY_HOSTS && (show_all_hosts==TRUE || !strcmp(host_name,temp_hoststatus->host_name)))
			count_host=1;
		else if(display_type==DISPLAY_SERVICEGROUPS){
			if(show_all_servicegroups==TRUE)
				count_host=1;
			else{
				for(temp_servicestatus=servicestatus_list;temp_servicestatus!=NULL;temp_servicestatus=temp_servicestatus->next){
					if(strcmp(temp_servicestatus->host_name,temp_hoststatus->host_name))
						continue;
					temp_service=find_service(temp_servicestatus->host_name,temp_servicestatus->description);
					if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
						continue;
					count_host=1;
					break;
				        }
			        }
		        }
		else if(display_type==DISPLAY_HOSTGROUPS && (show_all_hostgroups==TRUE || (is_host_member_of_hostgroup(find_hostgroup(hostgroup_name),temp_host)==TRUE)))
			count_host=1;

		if(count_host){

			if(temp_hoststatus->status==HOST_UP)
				total_up++;
			else if(temp_hoststatus->status==HOST_DOWN){
				total_down++;
				if(temp_hoststatus->problem_has_been_acknowledged==FALSE && temp_hoststatus->notifications_enabled==TRUE && temp_hoststatus->checks_enabled==TRUE && temp_hoststatus->scheduled_downtime_depth==0)
					problem_hosts_down++;
			        }
			else if(temp_hoststatus->status==HOST_UNREACHABLE){
				total_unreachable++;
				if(temp_hoststatus->problem_has_been_acknowledged==FALSE && temp_hoststatus->notifications_enabled==TRUE && temp_hoststatus->checks_enabled==TRUE && temp_hoststatus->scheduled_downtime_depth==0)
					problem_hosts_unreachable++;
			        }

			else if(temp_hoststatus->status==HOST_PENDING)
				total_pending++;
			else
				total_up++;
		        }
	        }

	total_hosts=total_up+total_down+total_unreachable+total_pending;
	total_problems=total_down+total_unreachable;

	printf("<DIV CLASS='hostTotals'>Host Status Totals</DIV>\n");

	printf("<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=0>\n");
	printf("<TR><TD>\n");


	printf("<TABLE BORDER=1 CLASS='hostTotals'>\n");
	printf("<TR>\n");

	printf("<TH CLASS='hostTotals'>");
	printf("<A CLASS='hostTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s",servicegroup_name);
	else{
		printf("hostgroup=%s",hostgroup_name);
		if((service_status_types!=all_service_status_types) || group_style_type==STYLE_DETAIL)
			printf("&style=detail");
		else if(group_style_type==STYLE_HOST_DETAIL)
			printf("&style=hostdetail");
	        }
	if(service_status_types!=all_service_status_types)
		printf("&servicestatustypes=%d",service_status_types);
	printf("&hoststatustypes=%d'>",HOST_UP);
	printf("Up</A></TH>\n");

	printf("<TH CLASS='hostTotals'>");
	printf("<A CLASS='hostTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s",servicegroup_name);
	else{
		printf("hostgroup=%s",hostgroup_name);
		if((service_status_types!=all_service_status_types) || group_style_type==STYLE_DETAIL)
			printf("&style=detail");
		else if(group_style_type==STYLE_HOST_DETAIL)
			printf("&style=hostdetail");
	        }
	if(service_status_types!=all_service_status_types)
		printf("&servicestatustypes=%d",service_status_types);
	printf("&hoststatustypes=%d'>",HOST_DOWN);
	printf("Down</A></TH>\n");

	printf("<TH CLASS='hostTotals'>");
	printf("<A CLASS='hostTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s",servicegroup_name);
	else{
		printf("hostgroup=%s",hostgroup_name);
		if((service_status_types!=all_service_status_types) || group_style_type==STYLE_DETAIL)
			printf("&style=detail");
		else if(group_style_type==STYLE_HOST_DETAIL)
			printf("&style=hostdetail");
	        }
	if(service_status_types!=all_service_status_types)
		printf("&servicestatustypes=%d",service_status_types);
	printf("&hoststatustypes=%d'>",HOST_UNREACHABLE);
	printf("Unreachable</A></TH>\n");

	printf("<TH CLASS='hostTotals'>");
	printf("<A CLASS='hostTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s",servicegroup_name);
	else{
		printf("hostgroup=%s",hostgroup_name);
		if((service_status_types!=all_service_status_types) || group_style_type==STYLE_DETAIL)
			printf("&style=detail");
		else if(group_style_type==STYLE_HOST_DETAIL)
			printf("&style=hostdetail");
	        }
	if(service_status_types!=all_service_status_types)
		printf("&servicestatustypes=%d",service_status_types);
	printf("&hoststatustypes=%d'>",HOST_PENDING);
	printf("Pending</A></TH>\n");

	printf("</TR>\n");


	printf("<TR>\n");

	/* total hosts up */
	printf("<TD CLASS='hostTotals%s'>%d</TD>\n",(total_up>0)?"UP":"",total_up);

	/* total hosts down */
	printf("<TD CLASS='hostTotals%s'>%d</TD>\n",(total_down>0)?"DOWN":"",total_down);

	/* total hosts unreachable */
	printf("<TD CLASS='hostTotals%s'>%d</TD>\n",(total_unreachable>0)?"UNREACHABLE":"",total_unreachable);

	/* total hosts pending */
	printf("<TD CLASS='hostTotals%s'>%d</TD>\n",(total_pending>0)?"PENDING":"",total_pending);

	printf("</TR>\n");
	printf("</TABLE>\n");

	printf("</TD></TR><TR><TD ALIGN=CENTER>\n");

	printf("<TABLE BORDER=1 CLASS='hostTotals'>\n");
	printf("<TR>\n");

	printf("<TH CLASS='hostTotals'>");
	printf("<A CLASS='hostTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s",servicegroup_name);
	else{
		printf("hostgroup=%s",hostgroup_name);
		if((service_status_types!=all_service_status_types) || group_style_type==STYLE_DETAIL)
			printf("&style=detail");
		else if(group_style_type==STYLE_HOST_DETAIL)
			printf("&style=hostdetail");
	        }
	if(service_status_types!=all_service_status_types)
		printf("&servicestatustypes=%d",service_status_types);
	printf("&hoststatustypes=%d'>",HOST_DOWN|HOST_UNREACHABLE);
	printf("<I>All Problems</I></A></TH>\n");

	printf("<TH CLASS='hostTotals'>");
	printf("<A CLASS='hostTotals' HREF='%s?",STATUS_CGI);
	if(display_type==DISPLAY_HOSTS)
		printf("host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		printf("servicegroup=%s",servicegroup_name);
	else{
		printf("hostgroup=%s",hostgroup_name);
		if((service_status_types!=all_service_status_types) || group_style_type==STYLE_DETAIL)
			printf("&style=detail");
		else if(group_style_type==STYLE_HOST_DETAIL)
			printf("&style=hostdetail");
	        }
	if(service_status_types!=all_service_status_types)
		printf("&servicestatustypes=%d",service_status_types);
	printf("'>");
	printf("<I>All Types</I></A></TH>\n");

	printf("</TR><TR>\n");

	/* total hosts with problems */
	printf("<TD CLASS='hostTotals%s'>%d</TD>\n",(total_problems>0)?"PROBLEMS":"",total_problems);

	/* total hosts */
	printf("<TD CLASS='hostTotals'>%d</TD>\n",total_hosts);

	printf("</TR>\n");
	printf("</TABLE>\n");

	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	printf("</DIV>\n");

	return;
        }



/* display a detailed listing of the status of all services... */
void show_service_detail(void){
	time_t t;
	char date_time[MAX_DATETIME_LENGTH];
	char state_duration[48];
	char status[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	char temp_url[MAX_INPUT_BUFFER];
	char *status_class="";
	char *status_bg_class="";
	char *host_status_bg_class="";
	char *last_host="";
	int new_host=FALSE;
	servicestatus *temp_status=NULL;
	hostgroup *temp_hostgroup=NULL;
	servicegroup *temp_servicegroup=NULL;
	hostextinfo *temp_hostextinfo=NULL;
	serviceextinfo *temp_serviceextinfo=NULL;
	hoststatus *temp_hoststatus=NULL;
	host *temp_host=NULL;
	service *temp_service=NULL;
	int odd=0;
	int total_comments=0;
	int user_has_seen_something=FALSE;
	servicesort *temp_servicesort=NULL;
	int use_sort=FALSE;
	int result=OK;
	int first_entry=TRUE;
	int days;
	int hours;
	int minutes;
	int seconds;
	int duration_error=FALSE;
	int total_entries=0;
	int show_service=FALSE;


	/* sort the service list if necessary */
	if(sort_type!=SORT_NONE){
		result=sort_services(sort_type,sort_option);
		if(result==ERROR)
			use_sort=FALSE;
		else
			use_sort=TRUE;
	        }
	else
		use_sort=FALSE;


	printf("<P>\n");

	printf("<table border=0 width=100%%>\n");
	printf("<tr>\n");

	printf("<td valign=top align=left width=33%%>\n");

	if(display_header==TRUE)
		show_filters();

	printf("</td>");

	printf("<td valign=top align=center width=33%%>\n");

	printf("<DIV ALIGN=CENTER CLASS='statusTitle'>Service Status Details For ");
	if(display_type==DISPLAY_HOSTS){
		if(show_all_hosts==TRUE)
			printf("All Hosts");
		else
			printf("Host '%s'",host_name);
	        }
	else if(display_type==DISPLAY_SERVICEGROUPS){
		if(show_all_servicegroups==TRUE)
			printf("All Service Groups");
		else
			printf("Service Group '%s'",servicegroup_name);
	        }
	else{
		if(show_all_hostgroups==TRUE)
			printf("All Host Groups");
		else
			printf("Host Group '%s'",hostgroup_name);
	        }
	printf("</DIV>\n");

	if(use_sort==TRUE){
		printf("<DIV ALIGN=CENTER CLASS='statusSort'>Entries sorted by <b>");
		if(sort_option==SORT_HOSTNAME)
			printf("host name");
		else if(sort_option==SORT_SERVICENAME)
			printf("service name");
		else if(sort_option==SORT_SERVICESTATUS)
			printf("service status");
		else if(sort_option==SORT_LASTCHECKTIME)
			printf("last check time");
		else if(sort_option==SORT_CURRENTATTEMPT)
			printf("attempt number");
		else if(sort_option==SORT_STATEDURATION)
			printf("state duration");
		printf("</b> (%s)\n",(sort_type==SORT_ASCENDING)?"ascending":"descending");
		printf("</DIV>\n");
	        }

	printf("<br>");

	printf("</td>\n");

	printf("<td valign=top align=right width=33%%></td>\n");
	
	printf("</tr>\n");
	printf("</table>\n");





	snprintf(temp_url,sizeof(temp_url)-1,"%s?",STATUS_CGI);
	temp_url[sizeof(temp_url)-1]='\x0';
	if(display_type==DISPLAY_HOSTS)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"host=%s",host_name);
	else if(display_type==DISPLAY_SERVICEGROUPS)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"servicegroup=%s&style=detail",servicegroup_name);
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"hostgroup=%s&style=detail",hostgroup_name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	strncat(temp_url,temp_buffer,sizeof(temp_url)-strlen(temp_url)-1);
	temp_url[sizeof(temp_url)-1]='\x0';
	if(service_status_types!=all_service_status_types){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"&servicestatustypes=%d",service_status_types);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		strncat(temp_url,temp_buffer,sizeof(temp_url)-strlen(temp_url)-1);
		temp_url[sizeof(temp_url)-1]='\x0';
	        }
	if(host_status_types!=all_host_status_types){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"&hoststatustypes=%d",host_status_types);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		strncat(temp_url,temp_buffer,sizeof(temp_url)-strlen(temp_url)-1);
		temp_url[sizeof(temp_url)-1]='\x0';
	        }
	if(service_properties!=0){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"&serviceprops=%lu",service_properties);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		strncat(temp_url,temp_buffer,sizeof(temp_url)-strlen(temp_url)-1);
		temp_url[sizeof(temp_url)-1]='\x0';
	        }
	if(host_properties!=0){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"&hostprops=%lu",host_properties);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		strncat(temp_url,temp_buffer,sizeof(temp_url)-strlen(temp_url)-1);
		temp_url[sizeof(temp_url)-1]='\x0';
	        }

	/* the main list of services */
	printf("<TABLE BORDER=0 width=100%% CLASS='status'>\n");
	printf("<TR>\n");

	printf("<TH CLASS='status'>Host&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by host name (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by host name (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_HOSTNAME,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_HOSTNAME,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='status'>Service&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by service name (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by service name (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_SERVICENAME,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_SERVICENAME,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='status'>Status&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by service status (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by service status (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_SERVICESTATUS,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_SERVICESTATUS,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='status'>Last Check&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by last check time (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by last check time (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_LASTCHECKTIME,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_LASTCHECKTIME,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='status'>Duration&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by state duration (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by state duration time (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_STATEDURATION,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_STATEDURATION,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='status'>Attempt&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by current attempt (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by current attempt (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_CURRENTATTEMPT,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_CURRENTATTEMPT,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='status'>Status Information</TH>\n");
	printf("</TR>\n");


	temp_hostgroup=find_hostgroup(hostgroup_name);
	temp_servicegroup=find_servicegroup(servicegroup_name);

	/* check all services... */
	while(1){

		/* get the next service to display */
		if(use_sort==TRUE){
			if(first_entry==TRUE)
				temp_servicesort=servicesort_list;
			else
				temp_servicesort=temp_servicesort->next;
			if(temp_servicesort==NULL)
				break;
			temp_status=temp_servicesort->svcstatus;
	                }
		else{
			if(first_entry==TRUE)
				temp_status=servicestatus_list;
			else
				temp_status=temp_status->next;
		        }

		if(temp_status==NULL)
			break;

		first_entry=FALSE;

		/* find the service  */
		temp_service=find_service(temp_status->host_name,temp_status->description);

		/* if we couldn't find the service, go to the next service */
		if(temp_service==NULL)
			continue;

		/* find the host */
		temp_host=find_host(temp_service->host_name);

		/* make sure user has rights to see this... */
		if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
			continue;

		user_has_seen_something=TRUE;

		/* get the host status information */
		temp_hoststatus=find_hoststatus(temp_service->host_name);

		/* see if we should display services for hosts with tis type of status */
		if(!(host_status_types & temp_hoststatus->status))
			continue;

		/* see if we should display this type of service status */
		if(!(service_status_types & temp_status->status))
			continue;	

		/* check host properties filter */
		if(passes_host_properties_filter(temp_hoststatus)==FALSE)
			continue;

		/* check service properties filter */
		if(passes_service_properties_filter(temp_status)==FALSE)
			continue;


		show_service=FALSE;

		if(display_type==DISPLAY_HOSTS){
			if(show_all_hosts==TRUE)
				show_service=TRUE;
			else if(!strcmp(host_name,temp_status->host_name))
				show_service=TRUE;
		        }

		else if(display_type==DISPLAY_HOSTGROUPS){
			if(show_all_hostgroups==TRUE)
				show_service=TRUE;
			else if(is_host_member_of_hostgroup(temp_hostgroup,temp_host)==TRUE)
				show_service=TRUE;
		        }

		else if(display_type==DISPLAY_SERVICEGROUPS){
			if(show_all_servicegroups==TRUE)
				show_service=TRUE;
			else if(is_service_member_of_servicegroup(temp_servicegroup,temp_service)==TRUE)
				show_service=TRUE;
		        }

		if(show_service==TRUE){

			if(strcmp(last_host,temp_status->host_name))
				new_host=TRUE;
			else
				new_host=FALSE;

			if(new_host==TRUE){
				if(strcmp(last_host,"")){
					printf("<TR><TD colspan=6></TD></TR>\n");
					printf("<TR><TD colspan=6></TD></TR>\n");
				        }
			        }

			if(odd)
				odd=0;
			else
				odd=1;

			/* keep track of total number of services we're displaying */
			total_entries++;

		        /* get the last service check time */
			t=temp_status->last_check;
			get_time_string(&t,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
			if((unsigned long)temp_status->last_check==0L)
				strcpy(date_time,"N/A");

			if(temp_status->status==SERVICE_PENDING){
				strncpy(status,"PENDING",sizeof(status));
				status_class="PENDING";
				status_bg_class=(odd)?"Even":"Odd";
		                }
			else if(temp_status->status==SERVICE_OK){
				strncpy(status,"OK",sizeof(status));
				status_class="OK";
				status_bg_class=(odd)?"Even":"Odd";
		                }
			else if(temp_status->status==SERVICE_WARNING){
				strncpy(status,"WARNING",sizeof(status));
				status_class="WARNING";
				if(temp_status->problem_has_been_acknowledged==TRUE)
					status_bg_class="BGWARNINGACK";
				else if(temp_status->scheduled_downtime_depth>0)
					status_bg_class="BGWARNINGSCHED";
				else
					status_bg_class="BGWARNING";
		                }
			else if(temp_status->status==SERVICE_UNKNOWN){
				strncpy(status,"UNKNOWN",sizeof(status));
				status_class="UNKNOWN";
				if(temp_status->problem_has_been_acknowledged==TRUE)
					status_bg_class="BGUNKNOWNACK";
				else if(temp_status->scheduled_downtime_depth>0)
					status_bg_class="BGUNKNOWNSCHED";
				else
					status_bg_class="BGUNKNOWN";
		                }
			else if(temp_status->status==SERVICE_CRITICAL){
				strncpy(status,"CRITICAL",sizeof(status));
				status_class="CRITICAL";
				if(temp_status->problem_has_been_acknowledged==TRUE)
					status_bg_class="BGCRITICALACK";
				else if(temp_status->scheduled_downtime_depth>0)
					status_bg_class="BGCRITICALSCHED";
				else
					status_bg_class="BGCRITICAL";
		                }
			status[sizeof(status)-1]='\x0';


			printf("<TR>\n");

			/* host name column */
			if(new_host==TRUE){

				/* find extended information for this host */
				temp_hostextinfo=find_hostextinfo(temp_status->host_name);

				if(temp_hoststatus->status==HOST_DOWN){
					if(temp_hoststatus->problem_has_been_acknowledged==TRUE)
						host_status_bg_class="HOSTDOWNACK";
					else if(temp_hoststatus->scheduled_downtime_depth>0)
						host_status_bg_class="HOSTDOWNSCHED";
					else
						host_status_bg_class="HOSTDOWN";
				        }
				else if(temp_hoststatus->status==HOST_UNREACHABLE){
					if(temp_hoststatus->problem_has_been_acknowledged==TRUE)
						host_status_bg_class="HOSTUNREACHABLEACK";
					else if(temp_hoststatus->scheduled_downtime_depth>0)
						host_status_bg_class="HOSTUNREACHABLESCHED";
					else
						host_status_bg_class="HOSTUNREACHABLE";
				        }
				else
					host_status_bg_class=(odd)?"Even":"Odd";

				printf("<TD CLASS='status%s'>",host_status_bg_class);

				printf("<TABLE BORDER=0 WIDTH='100%%' cellpadding=0 cellspacing=0>\n");
				printf("<TR>\n");
				printf("<TD ALIGN=LEFT>\n");
				printf("<TABLE BORDER=0 cellpadding=0 cellspacing=0>\n");
				printf("<TR>\n");
				printf("<TD align=left valign=center CLASS='status%s'><A HREF='%s?type=%d&host=%s'>%s</A></TD>\n",host_status_bg_class,EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),temp_status->host_name);
				printf("</TR>\n");
				printf("</TABLE>\n");
				printf("</TD>\n");
				printf("<TD align=right valign=center>\n");
				printf("<TABLE BORDER=0 cellpadding=0 cellspacing=0>\n");
				printf("<TR>\n");
				total_comments=number_of_host_comments(temp_host->name);
				if(temp_hoststatus->problem_has_been_acknowledged==TRUE){
					printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s#comments'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This host problem has been acknowledged'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,ACKNOWLEDGEMENT_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
			                }
				if(total_comments>0)
					printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s#comments'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This host has %d comment%s associated with it'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,COMMENT_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,total_comments,(total_comments==1)?"":"s");
				if(temp_hoststatus->notifications_enabled==FALSE){
					printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='Notifications for this host have been disabled'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,NOTIFICATIONS_DISABLED_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
			                }
				if(temp_hoststatus->checks_enabled==FALSE){
					printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='Checks of this host have been disabled'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,DISABLED_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
				        }
				if(temp_hoststatus->is_flapping==TRUE){
					printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This host is flapping between states'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,FLAPPING_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
				        }
				if(temp_hoststatus->scheduled_downtime_depth>0){
					printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This host is currently in a period of scheduled downtime'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,SCHEDULED_DOWNTIME_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
				        }
				if(temp_hostextinfo!=NULL){
					if(temp_hostextinfo->notes_url!=NULL){
						printf("<TD align=center valign=center>");
						printf("<A HREF='");
						print_extra_host_url(temp_hostextinfo->host_name,temp_hostextinfo->notes_url);
						printf("' TARGET='_blank'>");
						printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,NOTES_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"View Extra Host Notes");
						printf("</A>");
						printf("</TD>\n");
					        }
					if(temp_hostextinfo->action_url!=NULL){
						printf("<TD align=center valign=center>");
						printf("<A HREF='");
						print_extra_host_url(temp_hostextinfo->host_name,temp_hostextinfo->action_url);
						printf("' TARGET='_blank'>");
						printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,ACTION_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"Perform Extra Host Actions");
						printf("</A>");
						printf("</TD>\n");
					        }
					if(temp_hostextinfo->icon_image!=NULL){
						printf("<TD align=center valign=center>");
						printf("<A HREF='%s?type=%d&host=%s'>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name));
						printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_logo_images_path,temp_hostextinfo->icon_image,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,(temp_hostextinfo->icon_image_alt==NULL)?"":temp_hostextinfo->icon_image_alt);
						printf("</A>");
						printf("</TD>\n");
					        }
				        }
				printf("</TR>\n");
				printf("</TABLE>\n");
				printf("</TD>\n");
				printf("</TR>\n");
				printf("</TABLE>\n");
			        }
			else
				printf("<TD>");
			printf("</TD>\n");

			/* service name column */
			printf("<TD CLASS='status%s'>",status_bg_class);
			printf("<TABLE BORDER=0 WIDTH='100%%' CELLSPACING=0 CELLPADDING=0>");
			printf("<TR>");
			printf("<TD ALIGN=LEFT>");
			printf("<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=0>\n");
			printf("<TR>\n");
			printf("<TD ALIGN=LEFT valign=center CLASS='status%s'><A HREF='%s?type=%d&host=%s",status_bg_class,EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_status->host_name));
			printf("&service=%s'>%s</A></TD>",url_encode(temp_status->description),temp_status->description);
			printf("</TR>\n");
			printf("</TABLE>\n");
			printf("</TD>\n");
			printf("<TD ALIGN=RIGHT CLASS='status%s'>\n",status_bg_class);
			printf("<TABLE BORDER=0 cellspacing=0 cellpadding=0>\n");
			printf("<TR>\n");
			total_comments=number_of_service_comments(temp_service->host_name,temp_service->description);
			if(total_comments>0){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s",EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_status->host_name));
				printf("&service=%s#comments'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This service has %d comment%s associated with it'></A></TD>",url_encode(temp_status->description),url_images_path,COMMENT_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,total_comments,(total_comments==1)?"":"s");
			        }
			if(temp_status->problem_has_been_acknowledged==TRUE){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s",EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_status->host_name));
				printf("&service=%s#comments'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This service problem has been acknowledged'></A></TD>",url_encode(temp_status->description),url_images_path,ACKNOWLEDGEMENT_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
			        }
			if(temp_status->checks_enabled==FALSE && temp_status->accept_passive_service_checks==FALSE){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s",EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_status->host_name));
				printf("&service=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='Active and passive checks have been disabled for this service'></A></TD>",url_encode(temp_status->description),url_images_path,DISABLED_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
			        }
			else if(temp_status->checks_enabled==FALSE){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s",EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_status->host_name));
				printf("&service=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='Active checks of the service have been disabled - only passive checks are being accepted'></A></TD>",url_encode(temp_status->description),url_images_path,PASSIVE_ONLY_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
			        }
			if(temp_status->notifications_enabled==FALSE){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s",EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_status->host_name));
				printf("&service=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='Notifications for this service have been disabled'></A></TD>",url_encode(temp_status->description),url_images_path,NOTIFICATIONS_DISABLED_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
			        }
			if(temp_status->is_flapping==TRUE){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s",EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_status->host_name));
				printf("&service=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This service is flapping between states'></A></TD>",url_encode(temp_status->description),url_images_path,FLAPPING_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
			        }
			if(temp_status->scheduled_downtime_depth>0){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s",EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_status->host_name));
				printf("&service=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This service is currently in a period of scheduled downtime'></A></TD>",url_encode(temp_status->description),url_images_path,SCHEDULED_DOWNTIME_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
			        }
			temp_serviceextinfo=find_serviceextinfo(temp_service->host_name,temp_service->description);
			if(temp_serviceextinfo!=NULL){
				if(temp_serviceextinfo->icon_image!=NULL){
					printf("<TD ALIGN=center valign=center>");
					printf("<A HREF='%s?type=%d&host=%s",EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_service->host_name));
					printf("&service=%s'>",url_encode(temp_service->description));
					printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_logo_images_path,temp_serviceextinfo->icon_image,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,(temp_serviceextinfo->icon_image_alt==NULL)?"":temp_serviceextinfo->icon_image_alt);
					printf("</A>");
					printf("</TD>\n");
				        }
			        }
			printf("</TR>\n");
			printf("</TABLE>\n");
			printf("</TD>\n");
			printf("</TR>");
			printf("</TABLE>");
			printf("</TD>\n");

			/* state duration calculation... */
			t=0;
			duration_error=FALSE;
			if(temp_status->last_state_change==(time_t)0){
				if(program_start>current_time)
					duration_error=TRUE;
				else
					t=current_time-program_start;
			        }
			else{
				if(temp_status->last_state_change>current_time)
					duration_error=TRUE;
				else
					t=current_time-temp_status->last_state_change;
			        }
			get_time_breakdown((unsigned long)t,&days,&hours,&minutes,&seconds);
			if(duration_error==TRUE)
				snprintf(state_duration,sizeof(state_duration)-1,"???");
			else
				snprintf(state_duration,sizeof(state_duration)-1,"%2dd %2dh %2dm %2ds%s",days,hours,minutes,seconds,(temp_status->last_state_change==(time_t)0)?"+":"");
			state_duration[sizeof(state_duration)-1]='\x0';

                        /* the rest of the columns... */
			printf("<TD CLASS='status%s'>%s</TD>\n",status_class,status);
			printf("<TD CLASS='status%s' nowrap>%s</TD>\n",status_bg_class,date_time);
			printf("<TD CLASS='status%s' nowrap>%s</TD>\n",status_bg_class,state_duration);
			printf("<TD CLASS='status%s'>%d/%d</TD>\n",status_bg_class,temp_status->current_attempt,temp_status->max_attempts);
			printf("<TD CLASS='status%s'>%s&nbsp;</TD>\n",status_bg_class,temp_status->plugin_output);

			printf("</TR>\n");

			last_host=temp_status->host_name;
		        }

	        }

	printf("</TABLE>\n");

	/* if user couldn't see anything, print out some helpful info... */
	if(user_has_seen_something==FALSE){

		if(servicestatus_list!=NULL){
			printf("<P><DIV CLASS='errorMessage'>It appears as though you do not have permission to view information for any of the services you requested...</DIV></P>\n");
			printf("<P><DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
			printf("and check the authorization options in your CGI configuration file.</DIV></P>\n");
		        }
		else{
			printf("<P><DIV CLASS='infoMessage'>There doesn't appear to be any service status information in the status log...<br><br>\n");
			printf("Make sure that Nagios is running and that you have specified the location of you status log correctly in the configuration files.</DIV></P>\n");
		        }
	        }

	else
		printf("<BR><DIV CLASS='itemTotalsTitle'>%d Matching Service Entries Displayed</DIV>\n",total_entries);

	return;
        }




/* display a detailed listing of the status of all hosts... */
void show_host_detail(void){
	time_t t;
	char date_time[MAX_DATETIME_LENGTH];
	char state_duration[48];
	char status[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	char temp_url[MAX_INPUT_BUFFER];
	char *status_class="";
	char *status_bg_class="";
	hoststatus *temp_status=NULL;
	hostgroup *temp_hostgroup=NULL;
	hostextinfo *temp_hostextinfo=NULL;
	host *temp_host=NULL;
	hostsort *temp_hostsort=NULL;
	int odd=0;
	int total_comments=0;
	int user_has_seen_something=FALSE;
	int use_sort=FALSE;
	int result=OK;
	int first_entry=TRUE;
	int days;
	int hours;
	int minutes;
	int seconds;
	int duration_error=FALSE;
	int total_entries=0;


	/* sort the host list if necessary */
	if(sort_type!=SORT_NONE){
		result=sort_hosts(sort_type,sort_option);
		if(result==ERROR)
			use_sort=FALSE;
		else
			use_sort=TRUE;
	        }
	else
		use_sort=FALSE;


	printf("<P>\n");

	printf("<table border=0 width=100%%>\n");
	printf("<tr>\n");

	printf("<td valign=top align=left width=33%%>\n");

	show_filters();

	printf("</td>");

	printf("<td valign=top align=center width=33%%>\n");

	printf("<DIV ALIGN=CENTER CLASS='statusTitle'>Host Status Details For ");
	if(show_all_hostgroups==TRUE)
		printf("All Host Groups");
	else
		printf("Host Group '%s'",hostgroup_name);
	printf("</DIV>\n");

	if(use_sort==TRUE){
		printf("<DIV ALIGN=CENTER CLASS='statusSort'>Entries sorted by <b>");
		if(sort_option==SORT_HOSTNAME)
			printf("host name");
		else if(sort_option==SORT_HOSTSTATUS)
			printf("host status");
		else if(sort_option==SORT_LASTCHECKTIME)
			printf("last check time");
		else if(sort_option==SORT_CURRENTATTEMPT)
			printf("attempt number");
		else if(sort_option==SORT_STATEDURATION)
			printf("state duration");
		printf("</b> (%s)\n",(sort_type==SORT_ASCENDING)?"ascending":"descending");
		printf("</DIV>\n");
	        }

	printf("<br>");

	printf("</td>\n");

	printf("<td valign=top align=right width=33%%></td>\n");
	
	printf("</tr>\n");
	printf("</table>\n");





	snprintf(temp_url,sizeof(temp_url)-1,"%s?",STATUS_CGI);
	temp_url[sizeof(temp_url)-1]='\x0';
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"hostgroup=%s&style=hostdetail",hostgroup_name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	strncat(temp_url,temp_buffer,sizeof(temp_url)-strlen(temp_url)-1);
	temp_url[sizeof(temp_url)-1]='\x0';
	if(service_status_types!=all_service_status_types){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"&servicestatustypes=%d",service_status_types);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		strncat(temp_url,temp_buffer,sizeof(temp_url)-strlen(temp_url)-1);
		temp_url[sizeof(temp_url)-1]='\x0';
	        }
	if(host_status_types!=all_host_status_types){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"&hoststatustypes=%d",host_status_types);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		strncat(temp_url,temp_buffer,sizeof(temp_url)-strlen(temp_url)-1);
		temp_url[sizeof(temp_url)-1]='\x0';
	        }
	if(service_properties!=0){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"&serviceprops=%lu",service_properties);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		strncat(temp_url,temp_buffer,sizeof(temp_url)-strlen(temp_url)-1);
		temp_url[sizeof(temp_url)-1]='\x0';
	        }
	if(host_properties!=0){
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"&hostprops=%lu",host_properties);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		strncat(temp_url,temp_buffer,sizeof(temp_url)-strlen(temp_url)-1);
		temp_url[sizeof(temp_url)-1]='\x0';
	        }


	/* the main list of hosts */
	printf("<DIV ALIGN='center'>\n");
	printf("<TABLE BORDER=0 CLASS='status' WIDTH=100%>\n");
	printf("<TR>\n");

	printf("<TH CLASS='status'>Host&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by host name (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by host name (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_HOSTNAME,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_HOSTNAME,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='status'>Status&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by host status (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by host status (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_HOSTSTATUS,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_HOSTSTATUS,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='status'>Last Check&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by last check time (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by last check time (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_LASTCHECKTIME,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_LASTCHECKTIME,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='status'>Duration&nbsp;<A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by state duration (ascending)'></A><A HREF='%s&sorttype=%d&sortoption=%d'><IMG SRC='%s%s' BORDER=0 ALT='Sort by state duration time (descending)'></A></TH>",temp_url,SORT_ASCENDING,SORT_STATEDURATION,url_images_path,UP_ARROW_ICON,temp_url,SORT_DESCENDING,SORT_STATEDURATION,url_images_path,DOWN_ARROW_ICON);

	printf("<TH CLASS='status'>Status Information</TH>\n");
	printf("</TR>\n");


	/* check all hosts... */
	while(1){

		/* get the next service to display */
		if(use_sort==TRUE){
			if(first_entry==TRUE)
				temp_hostsort=hostsort_list;
			else
				temp_hostsort=temp_hostsort->next;
			if(temp_hostsort==NULL)
				break;
			temp_status=temp_hostsort->hststatus;
	                }
		else{
			if(first_entry==TRUE)
				temp_status=hoststatus_list;
			else
				temp_status=temp_status->next;
		        }

		if(temp_status==NULL)
			break;

		first_entry=FALSE;

		/* find the host  */
		temp_host=find_host(temp_status->host_name);

		/* if we couldn't find the host, go to the next status entry */
		if(temp_host==NULL)
			continue;

		/* make sure user has rights to see this... */
		if(is_authorized_for_host(temp_host,&current_authdata)==FALSE)
			continue;

		user_has_seen_something=TRUE;

		/* see if we should display services for hosts with tis type of status */
		if(!(host_status_types & temp_status->status))
			continue;

		/* check host properties filter */
		if(passes_host_properties_filter(temp_status)==FALSE)
			continue;


		/* see if this host is a member of the hostgroup */
		if(show_all_hostgroups==FALSE){
			temp_hostgroup=find_hostgroup(hostgroup_name);
			if(temp_hostgroup==NULL)
				continue;
			if(is_host_member_of_hostgroup(temp_hostgroup,temp_host)==FALSE)
				continue;
	                }
	
		total_entries++;


		if(display_type==DISPLAY_HOSTGROUPS){

			if(odd)
				odd=0;
			else
				odd=1;

			
		        /* get the last host check time */
			t=temp_status->last_check;
			get_time_string(&t,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
			if((unsigned long)temp_status->last_check==0L)
				strcpy(date_time,"N/A");

			if(temp_status->status==HOST_PENDING){
				strncpy(status,"PENDING",sizeof(status));
				status_class="PENDING";
				status_bg_class=(odd)?"Even":"Odd";
		                }
			else if(temp_status->status==HOST_UP){
				strncpy(status,"UP",sizeof(status));
				status_class="HOSTUP";
				status_bg_class=(odd)?"Even":"Odd";
		                }
			else if(temp_status->status==HOST_DOWN){
				strncpy(status,"DOWN",sizeof(status));
				status_class="HOSTDOWN";
				if(temp_status->problem_has_been_acknowledged==TRUE)
					status_bg_class="BGDOWNACK";
				else if(temp_status->scheduled_downtime_depth>0)
					status_bg_class="BGDOWNSCHED";
				else
					status_bg_class="BGDOWN";
		                }
			else if(temp_status->status==HOST_UNREACHABLE){
				strncpy(status,"UNREACHABLE",sizeof(status));
				status_class="HOSTUNREACHABLE";
				if(temp_status->problem_has_been_acknowledged==TRUE)
					status_bg_class="BGUNREACHABLEACK";
				else if(temp_status->scheduled_downtime_depth>0)
					status_bg_class="BGUNREACHABLESCHED";
				else
					status_bg_class="BGUNREACHABLE";
		                }
			status[sizeof(status)-1]='\x0';


			printf("<TR>\n");


			/**** host name column ****/

			/* find extended information for this host */
			temp_hostextinfo=find_hostextinfo(temp_status->host_name);

			printf("<TD CLASS='status%s'>",status_class);

			printf("<TABLE BORDER=0 WIDTH='100%%' cellpadding=0 cellspacing=0>\n");
			printf("<TR>\n");
			printf("<TD ALIGN=LEFT>\n");
			printf("<TABLE BORDER=0 cellpadding=0 cellspacing=0>\n");
			printf("<TR>\n");
			printf("<TD align=left valign=center CLASS='status%s'><A HREF='%s?type=%d&host=%s'>%s</A>&nbsp;</TD>\n",status_class,EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),temp_status->host_name);
			printf("</TR>\n");
			printf("</TABLE>\n");
			printf("</TD>\n");
			printf("<TD align=right valign=center>\n");
			printf("<TABLE BORDER=0 cellpadding=0 cellspacing=0>\n");
			printf("<TR>\n");
			total_comments=number_of_host_comments(temp_host->name);
			if(temp_status->problem_has_been_acknowledged==TRUE){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s#comments'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This host problem has been acknowledged'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,ACKNOWLEDGEMENT_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
		                }
			if(total_comments>0)
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s#comments'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This host has %d comment%s associated with it'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,COMMENT_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,total_comments,(total_comments==1)?"":"s");
			if(temp_status->notifications_enabled==FALSE){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='Notifications for this host have been disabled'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,NOTIFICATIONS_DISABLED_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
		                }
			if(temp_status->checks_enabled==FALSE){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='Checks of this host have been disabled'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,DISABLED_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
			        }
			if(temp_status->is_flapping==TRUE){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This host is flapping between states'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,FLAPPING_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
			        }
			if(temp_status->scheduled_downtime_depth>0){
				printf("<TD ALIGN=center valign=center><A HREF='%s?type=%d&host=%s'><IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='This host is currently in a period of scheduled downtime'></A></TD>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name),url_images_path,SCHEDULED_DOWNTIME_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT);
			        }
			if(temp_hostextinfo!=NULL){
				if(temp_hostextinfo->notes_url!=NULL){
					printf("<TD align=center valign=center>");
					printf("<A HREF='");
					print_extra_host_url(temp_hostextinfo->host_name,temp_hostextinfo->notes_url);
					printf("' TARGET='_blank'>");
					printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,NOTES_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"View Extra Host Notes");
					printf("</A>");
					printf("</TD>\n");
				        }
				if(temp_hostextinfo->action_url!=NULL){
					printf("<TD align=center valign=center>");
					printf("<A HREF='");
					print_extra_host_url(temp_hostextinfo->host_name,temp_hostextinfo->action_url);
					printf("' TARGET='_blank'>");
					printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,ACTION_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"Perform Extra Host Actions");
					printf("</A>");
					printf("</TD>\n");
				        }
				if(temp_hostextinfo->icon_image!=NULL){
					printf("<TD align=center valign=center>");
					printf("<A HREF='%s?type=%d&host=%s'>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_status->host_name));
					printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_logo_images_path,temp_hostextinfo->icon_image,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,(temp_hostextinfo->icon_image_alt==NULL)?"":temp_hostextinfo->icon_image_alt);
					printf("</A>");
					printf("</TD>\n");
				        }
			        }
			printf("<TD><a href='%s?host=%s'><img src='%s%s' border=0 alt='View Service Details For This Host'></a></TD>\n",STATUS_CGI,url_encode(temp_status->host_name),url_images_path,STATUS_DETAIL_ICON);
			printf("</TR>\n");
			printf("</TABLE>\n");
			printf("</TD>\n");
			printf("</TR>\n");
			printf("</TABLE>\n");

			printf("</TD>\n");


			/* state duration calculation... */
			t=0;
			duration_error=FALSE;
			if(temp_status->last_state_change==(time_t)0){
				if(program_start>current_time)
					duration_error=TRUE;
				else
					t=current_time-program_start;
			        }
			else{
				if(temp_status->last_state_change>current_time)
					duration_error=TRUE;
				else
					t=current_time-temp_status->last_state_change;
			        }
			get_time_breakdown((unsigned long)t,&days,&hours,&minutes,&seconds);
			if(duration_error==TRUE)
				snprintf(state_duration,sizeof(state_duration)-1,"???");
			else
				snprintf(state_duration,sizeof(state_duration)-1,"%2dd %2dh %2dm %2ds%s",days,hours,minutes,seconds,(temp_status->last_state_change==(time_t)0)?"+":"");
			state_duration[sizeof(state_duration)-1]='\x0';

                        /* the rest of the columns... */
			printf("<TD CLASS='status%s'>%s</TD>\n",status_class,status);
			printf("<TD CLASS='status%s' nowrap>%s</TD>\n",status_bg_class,date_time);
			printf("<TD CLASS='status%s' nowrap>%s</TD>\n",status_bg_class,state_duration);
			printf("<TD CLASS='status%s'>%s&nbsp;</TD>\n",status_bg_class,temp_status->plugin_output);

			printf("</TR>\n");
		        }

	        }

	printf("</TABLE>\n");
	printf("</DIV>\n");

	/* if user couldn't see anything, print out some helpful info... */
	if(user_has_seen_something==FALSE){

		if(hoststatus_list!=NULL){
			printf("<P><DIV CLASS='errorMessage'>It appears as though you do not have permission to view information for any of the hosts you requested...</DIV></P>\n");
			printf("<P><DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
			printf("and check the authorization options in your CGI configuration file.</DIV></P>\n");
		        }
		else{
			printf("<P><DIV CLASS='infoMessage'>There doesn't appear to be any host status information in the status log...<br><br>\n");
			printf("Make sure that Nagios is running and that you have specified the location of you status log correctly in the configuration files.</DIV></P>\n");
		        }
	        }

	else
		printf("<BR><DIV CLASS='itemTotalsTitle'>%d Matching Host Entries Displayed</DIV>\n",total_entries);

	return;
        }




/* show an overview of servicegroup(s)... */
void show_servicegroup_overviews(void){
	servicegroup *temp_servicegroup=NULL;
	int current_column;
	int user_has_seen_something=FALSE;
	int servicegroup_error=FALSE;


	printf("<P>\n");

	printf("<table border=0 width=100%%>\n");
	printf("<tr>\n");

	printf("<td valign=top align=left width=33%%>\n");

	show_filters();

	printf("</td>");

	printf("<td valign=top align=center width=33%%>\n");

	printf("<DIV ALIGN=CENTER CLASS='statusTitle'>Service Overview For ");
	if(show_all_servicegroups==TRUE)
		printf("All Service Groups");
	else
		printf("Service Group '%s'",servicegroup_name);
	printf("</DIV>\n");

	printf("<br>");

	printf("</td>\n");

	printf("<td valign=top align=right width=33%%></td>\n");
	
	printf("</tr>\n");
	printf("</table>\n");

	printf("</P>\n");


	/* display status overviews for all servicegroups */
	if(show_all_servicegroups==TRUE){


		printf("<DIV ALIGN=center>\n");
		printf("<TABLE BORDER=0 CELLPADDING=10>\n");

		current_column=1;

		/* loop through all servicegroups... */
		for(temp_servicegroup=servicegroup_list;temp_servicegroup!=NULL;temp_servicegroup=temp_servicegroup->next){

			/* make sure the user is authorized to view at least one host in this servicegroup */
			if(is_authorized_for_servicegroup(temp_servicegroup,&current_authdata)==FALSE)
				continue;

			if(current_column==1)
				printf("<TR>\n");
			printf("<TD VALIGN=top ALIGN=center>\n");
				
			show_servicegroup_overview(temp_servicegroup);

			user_has_seen_something=TRUE;

			printf("</TD>\n");
			if(current_column==overview_columns)
				printf("</TR>\n");

			if(current_column<overview_columns)
				current_column++;
			else
				current_column=1;
		        }

		if(current_column!=1){

			for(;current_column<=overview_columns;current_column++)
				printf("<TD></TD>\n");
			printf("</TR>\n");
		        }

		printf("</TABLE>\n");
		printf("</DIV>\n");
	        }

	/* else display overview for just a specific servicegroup */
	else{

		temp_servicegroup=find_servicegroup(servicegroup_name);
		if(temp_servicegroup!=NULL){

			printf("<P>\n");
			printf("<DIV ALIGN=CENTER>\n");
			printf("<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0><TR><TD ALIGN=CENTER>\n");
			
			if(is_authorized_for_servicegroup(temp_servicegroup,&current_authdata)==TRUE){

				show_servicegroup_overview(temp_servicegroup);
				
				user_has_seen_something=TRUE;
			        }

			printf("</TD></TR></TABLE>\n");
			printf("</DIV>\n");
			printf("</P>\n");
		        }
		else{
			printf("<DIV CLASS='errorMessage'>Sorry, but service group '%s' doesn't seem to exist...</DIV>",servicegroup_name);
			servicegroup_error=TRUE;
		        }
	        }

	/* if user couldn't see anything, print out some helpful info... */
	if(user_has_seen_something==FALSE && servicegroup_error==FALSE){

		printf("<p>\n");
		printf("<div align='center'>\n");

		if(hoststatus_list!=NULL){
			printf("<DIV CLASS='errorMessage'>It appears as though you do not have permission to view information for any of the hosts you requested...</DIV>\n");
			printf("<DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
			printf("and check the authorization options in your CGI configuration file.</DIV>\n");
		        }
		else{
			printf("<DIV CLASS='infoMessage'>There doesn't appear to be any host status information in the status log...<br><br>\n");
			printf("Make sure that Nagios is running and that you have specified the location of you status log correctly in the configuration files.</DIV>\n");
		        }

		printf("</div>\n");
		printf("</p>\n");
	        }

	return;
        }



/* shows an overview of a specific servicegroup... */
void show_servicegroup_overview(servicegroup *temp_servicegroup){
	host *temp_host;
	hoststatus *temp_hoststatus=NULL;
	service *temp_service;
	int odd=0;

	/* make sure the user is authorized to view at least one service in this servicegroup */
	if(is_authorized_for_servicegroup(temp_servicegroup,&current_authdata)==FALSE)
		return;

	printf("<DIV CLASS='status'>\n");
	printf("<A HREF='%s?servicegroup=%s&style=detail'>%s</A>",STATUS_CGI,url_encode(temp_servicegroup->group_name),temp_servicegroup->alias);
	printf(" (<A HREF='%s?type=%d&servicegroup=%s'>%s</A>)",EXTINFO_CGI,DISPLAY_SERVICEGROUP_INFO,url_encode(temp_servicegroup->group_name),temp_servicegroup->group_name);
	printf("</DIV>\n");

	printf("<DIV CLASS='status'>\n");
	printf("<table border=1 CLASS='status'>\n");

	printf("<TR>\n");
	printf("<TH CLASS='status'>Host</TH><TH CLASS='status'>Status</TH><TH CLASS='status'>Services</TH><TH CLASS='status'>Actions</TH>\n");
	printf("</TR>\n");

	/* find all the hosts associated with services that belong to the servicegroup */
	for(temp_hoststatus=hoststatus_list;temp_hoststatus!=NULL;temp_hoststatus=temp_hoststatus->next){

		/* find the host... */
		temp_host=find_host(temp_hoststatus->host_name);

		/* see if this host has a service that belongs to the servicegroup */
		for(temp_service=service_list;temp_service;temp_service=temp_service->next){

			/* skip this service if it's not associate with the host */
			if(strcmp(temp_service->host_name,temp_host->name))
				continue;

			/* is this service a member of the servicegroup? */
			if(is_service_member_of_servicegroup(temp_servicegroup,temp_service)==FALSE)
				continue;

			/* is user authorized for this service? */
			if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
				continue;

			break;
		        }
		if(temp_service==NULL)
			continue;

		/* make sure we only display hosts of the specified status levels */
		if(!(host_status_types & temp_hoststatus->status))
			continue;

		/* make sure we only display hosts that have the desired properties */
		if(passes_host_properties_filter(temp_hoststatus)==FALSE)
			continue;

		if(odd)
			odd=0;
		else
			odd=1;

		show_servicegroup_hostgroup_member_overview(temp_hoststatus,odd,temp_servicegroup);
	        }

	printf("</table>\n");
	printf("</DIV>\n");

	return;
        }



/* show a summary of servicegroup(s)... */
void show_servicegroup_summaries(void){
	servicegroup *temp_servicegroup=NULL;
	int user_has_seen_something=FALSE;
	int servicegroup_error=FALSE;
	int odd=0;


	printf("<P>\n");

	printf("<table border=0 width=100%%>\n");
	printf("<tr>\n");

	printf("<td valign=top align=left width=33%%>\n");

	show_filters();

	printf("</td>");

	printf("<td valign=top align=center width=33%%>\n");

	printf("<DIV ALIGN=CENTER CLASS='statusTitle'>Status Summary For ");
	if(show_all_servicegroups==TRUE)
		printf("All Service Groups");
	else
		printf("Service Group '%s'",servicegroup_name);
	printf("</DIV>\n");

	printf("<br>");

	printf("</td>\n");

	printf("<td valign=top align=right width=33%%></td>\n");
	
	printf("</tr>\n");
	printf("</table>\n");

	printf("</P>\n");


	printf("<DIV ALIGN=center>\n");
	printf("<table border=1 CLASS='status'>\n");

	printf("<TR>\n");
	printf("<TH CLASS='status'>Host Group</TH><TH CLASS='status'>Host Status Totals</TH><TH CLASS='status'>Service Status Totals</TH>\n");
	printf("</TR>\n");

	/* display status summary for all servicegroups */
	if(show_all_servicegroups==TRUE){

		/* loop through all servicegroups... */
		for(temp_servicegroup=servicegroup_list;temp_servicegroup!=NULL;temp_servicegroup=temp_servicegroup->next){

			/* make sure the user is authorized to view at least one host in this servicegroup */
			if(is_authorized_for_servicegroup(temp_servicegroup,&current_authdata)==FALSE)
				continue;

			if(odd==0)
				odd=1;
			else
				odd=0;

			/* show summary for this servicegroup */
			show_servicegroup_summary(temp_servicegroup,odd);

			user_has_seen_something=TRUE;
		        }

	        }

	/* else just show summary for a specific servicegroup */
	else{
		temp_servicegroup=find_servicegroup(servicegroup_name);
		if(temp_servicegroup==NULL)
			servicegroup_error=TRUE;
		else{
			show_servicegroup_summary(temp_servicegroup,1);
			user_has_seen_something=TRUE;
		        }
	        }

	printf("</TABLE>\n");
	printf("</DIV>\n");

	/* if user couldn't see anything, print out some helpful info... */
	if(user_has_seen_something==FALSE && servicegroup_error==FALSE){

		printf("<P><DIV ALIGN=CENTER>\n");

		if(hoststatus_list!=NULL){
			printf("<DIV CLASS='errorMessage'>It appears as though you do not have permission to view information for any of the hosts you requested...</DIV>\n");
			printf("<DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
			printf("and check the authorization options in your CGI configuration file.</DIV>\n");
		        }
		else{
			printf("<DIV CLASS='infoMessage'>There doesn't appear to be any host status information in the status log...<br><br>\n");
			printf("Make sure that Nagios is running and that you have specified the location of you status log correctly in the configuration files.</DIV>\n");
		        }

		printf("</DIV></P>\n");
	        }

	/* we dcouldn't find the servicegroup */
	else if(servicegroup_error==TRUE){
		printf("<P><DIV ALIGN=CENTER>\n");
		printf("<DIV CLASS='errorMessage'>Sorry, but servicegroup '%s' doesn't seem to exist...</DIV>\n",servicegroup_name);
		printf("</DIV></P>\n");
	        }

	return;
        }



/* displays status summary information for a specific servicegroup */
void show_servicegroup_summary(servicegroup *temp_servicegroup,int odd){
	char *status_bg_class="";

	if(odd==1)
		status_bg_class="Even";
	else
		status_bg_class="Odd";

	printf("<TR CLASS='status%s'><TD CLASS='status%s'>\n",status_bg_class,status_bg_class);
	printf("<A HREF='%s?servicegroup=%s&style=overview'>%s</A> ",STATUS_CGI,url_encode(temp_servicegroup->group_name),temp_servicegroup->alias);
	printf("(<A HREF='%s?type=%d&servicegroup=%s'>%s</a>)",EXTINFO_CGI,DISPLAY_SERVICEGROUP_INFO,url_encode(temp_servicegroup->group_name),temp_servicegroup->group_name);
	printf("</TD>");
				
	printf("<TD CLASS='status%s' ALIGN=CENTER VALIGN=CENTER>",status_bg_class);
	show_servicegroup_host_totals_summary(temp_servicegroup);
	printf("</TD>");

	printf("<TD CLASS='status%s' ALIGN=CENTER VALIGN=CENTER>",status_bg_class);
	show_servicegroup_service_totals_summary(temp_servicegroup);
	printf("</TD>");

	printf("</TR>\n");

	return;
        }



/* shows host total summary information for a specific servicegroup */
void show_servicegroup_host_totals_summary(servicegroup *temp_servicegroup){
	int total_up=0;
	int total_down=0;
	int total_unreachable=0;
	int total_pending=0;
	hoststatus *temp_hoststatus;
	host *temp_host;
	service *temp_service;

	/* check all hosts... */
	for(temp_hoststatus=hoststatus_list;temp_hoststatus!=NULL;temp_hoststatus=temp_hoststatus->next){

		/* make sure the user is authorized to see this host... */
		temp_host=find_host(temp_hoststatus->host_name);
		if(is_authorized_for_host(temp_host,&current_authdata)==FALSE)
			continue;

		/* see if this host has a service that belongs to the servicegroup */
		for(temp_service=service_list;temp_service;temp_service=temp_service->next){

			/* skip this service if it's not associate with the host */
			if(strcmp(temp_service->host_name,temp_host->name))
				continue;

			/* is this service a member of the servicegroup? */
			if(is_service_member_of_servicegroup(temp_servicegroup,temp_service)==FALSE)
				continue;

			/* is user authorized for this service? */
			if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
				continue;

			break;
		        }
		if(temp_service==NULL)
			continue;

		/* make sure we only display hosts of the specified status levels */
		if(!(host_status_types & temp_hoststatus->status))
			continue;

		/* make sure we only display hosts that have the desired properties */
		if(passes_host_properties_filter(temp_hoststatus)==FALSE)
			continue;

		if(temp_hoststatus->status==HOST_UP)
			total_up++;
		else if(temp_hoststatus->status==HOST_DOWN)
			total_down++;
		else if(temp_hoststatus->status==HOST_UNREACHABLE)
			total_unreachable++;
		else
			total_pending++;
	        }

	printf("<TABLE BORDER=0>\n");

	if(total_up>0)
		printf("<TR><TD CLASS='miniStatusUP'><A HREF='%s?servicegroup=%s&style=detail&&hoststatustypes=%d&hostprops=%lu'>%d UP</A></TD></TR>\n",STATUS_CGI,url_encode(temp_servicegroup->group_name),HOST_UP,host_properties,total_up);
	if(total_down>0)
		printf("<TR><TD CLASS='miniStatusDOWN'><A HREF='%s?servicegroup=%s&style=detail&hoststatustypes=%d&hostprops=%lu'>%d DOWN</A></TD></TR>\n",STATUS_CGI,url_encode(temp_servicegroup->group_name),HOST_DOWN,host_properties,total_down);
	if(total_unreachable>0)
		printf("<TR><TD CLASS='miniStatusUNREACHABLE'><A HREF='%s?servicegroup=%s&style=detail&hoststatustypes=%d&hostprops=%lu'>%d UNREACHABLE</A></TD></TR>\n",STATUS_CGI,url_encode(temp_servicegroup->group_name),HOST_UNREACHABLE,host_properties,total_unreachable);
	if(total_pending>0)
		printf("<TR><TD CLASS='miniStatusPENDING'><A HREF='%s?servicegroup=%s&style=detail&hoststatustypes=%d&hostprops=%lu'>%d PENDING</A></TD></TR>\n",STATUS_CGI,url_encode(temp_servicegroup->group_name),HOST_PENDING,host_properties,total_pending);

	printf("</TABLE>\n");

	if((total_up + total_down + total_unreachable + total_pending)==0)
		printf("No matching hosts");

	return;
        }



/* shows service total summary information for a specific servicegroup */
void show_servicegroup_service_totals_summary(servicegroup *temp_servicegroup){
	int total_ok=0;
	int total_warning=0;
	int total_unknown=0;
	int total_critical=0;
	int total_pending=0;
	servicestatus *temp_servicestatus;
	service *temp_service;
	hoststatus *temp_hoststatus;


	/* check all services... */
	for(temp_servicestatus=servicestatus_list;temp_servicestatus!=NULL;temp_servicestatus=temp_servicestatus->next){

		/* make sure the user is authorized to see this service... */
		temp_service=find_service(temp_servicestatus->host_name,temp_servicestatus->description);
		if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
			continue;

		/* see if this service is associated with the specified servicegroup */
		if(is_service_member_of_servicegroup(temp_servicegroup,temp_service)==FALSE)
			continue;

		/* find the status of the associated host */
		temp_hoststatus=find_hoststatus(temp_servicestatus->host_name);
		if(temp_hoststatus==NULL)
			continue;

		/* make sure we only display hosts of the specified status levels */
		if(!(host_status_types & temp_hoststatus->status))
			continue;

		/* make sure we only display hosts that have the desired properties */
		if(passes_host_properties_filter(temp_hoststatus)==FALSE)
			continue;

		/* make sure we only display services of the specified status levels */
		if(!(service_status_types & temp_servicestatus->status))
			continue;

		/* make sure we only display services that have the desired properties */
		if(passes_service_properties_filter(temp_servicestatus)==FALSE)
			continue;

		if(temp_servicestatus->status==SERVICE_CRITICAL)
			total_critical++;
		else if(temp_servicestatus->status==SERVICE_WARNING)
			total_warning++;
		else if(temp_servicestatus->status==SERVICE_UNKNOWN)
			total_unknown++;
		else if(temp_servicestatus->status==SERVICE_OK)
			total_ok++;
		else if(temp_servicestatus->status==SERVICE_PENDING)
			total_pending++;
		else
			total_ok++;
	        }


	printf("<TABLE BORDER=0>\n");

	if(total_ok>0)
		printf("<TR><TD CLASS='miniStatusOK'><A HREF='%s?servicegroup=%s&style=detail&&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d OK</A></TD></TR>\n",STATUS_CGI,url_encode(temp_servicegroup->group_name),SERVICE_OK,host_status_types,service_properties,host_properties,total_ok);
	if(total_warning>0)
		printf("<TR><TD CLASS='miniStatusWARNING'><A HREF='%s?servicegroup=%s&style=detail&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d WARNING</A></TD></TR>\n",STATUS_CGI,url_encode(temp_servicegroup->group_name),SERVICE_WARNING,host_status_types,service_properties,host_properties,total_warning);
	if(total_unknown>0)
		printf("<TR><TD CLASS='miniStatusUNKNOWN'><A HREF='%s?servicegroup=%s&style=detail&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d UNKNOWN</A></TD></TR>\n",STATUS_CGI,url_encode(temp_servicegroup->group_name),SERVICE_UNKNOWN,host_status_types,service_properties,host_properties,total_unknown);
	if(total_critical>0)
		printf("<TR><TD CLASS='miniStatusCRITICAL'><A HREF='%s?servicegroup=%s&style=detail&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d CRITICAL</A></TD></TR>\n",STATUS_CGI,url_encode(temp_servicegroup->group_name),SERVICE_CRITICAL,host_status_types,service_properties,host_properties,total_critical);
	if(total_pending>0)
		printf("<TR><TD CLASS='miniStatusPENDING'><A HREF='%s?servicegroup=%s&style=detail&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d PENDING</A></TD></TR>\n",STATUS_CGI,url_encode(temp_servicegroup->group_name),SERVICE_PENDING,host_status_types,service_properties,host_properties,total_pending);

	printf("</TABLE>\n");

	if((total_ok + total_warning + total_unknown + total_critical + total_pending)==0)
		printf("No matching services");

	return;
        }



/* show a grid layout of servicegroup(s)... */
void show_servicegroup_grids(void){
	servicegroup *temp_servicegroup=NULL;
	int user_has_seen_something=FALSE;
	int servicegroup_error=FALSE;
	int odd=0;


	printf("<P>\n");

	printf("<table border=0 width=100%%>\n");
	printf("<tr>\n");

	printf("<td valign=top align=left width=33%%>\n");

	show_filters();

	printf("</td>");

	printf("<td valign=top align=center width=33%%>\n");

	printf("<DIV ALIGN=CENTER CLASS='statusTitle'>Status Grid For ");
	if(show_all_servicegroups==TRUE)
		printf("All Service Groups");
	else
		printf("Service Group '%s'",servicegroup_name);
	printf("</DIV>\n");

	printf("<br>");

	printf("</td>\n");

	printf("<td valign=top align=right width=33%%></td>\n");
	
	printf("</tr>\n");
	printf("</table>\n");

	printf("</P>\n");


	/* display status grids for all servicegroups */
	if(show_all_servicegroups==TRUE){

		/* loop through all servicegroups... */
		for(temp_servicegroup=servicegroup_list;temp_servicegroup!=NULL;temp_servicegroup=temp_servicegroup->next){

			/* make sure the user is authorized to view at least one host in this servicegroup */
			if(is_authorized_for_servicegroup(temp_servicegroup,&current_authdata)==FALSE)
				continue;

			if(odd==0)
				odd=1;
			else
				odd=0;

			/* show grid for this servicegroup */
			show_servicegroup_grid(temp_servicegroup);

			user_has_seen_something=TRUE;
		        }

	        }

	/* else just show grid for a specific servicegroup */
	else{
		temp_servicegroup=find_servicegroup(servicegroup_name);
		if(temp_servicegroup==NULL)
			servicegroup_error=TRUE;
		else{
			show_servicegroup_grid(temp_servicegroup);
			user_has_seen_something=TRUE;
		        }
	        }

	/* if user couldn't see anything, print out some helpful info... */
	if(user_has_seen_something==FALSE && servicegroup_error==FALSE){

		printf("<P><DIV ALIGN=CENTER>\n");

		if(hoststatus_list!=NULL){
			printf("<DIV CLASS='errorMessage'>It appears as though you do not have permission to view information for any of the hosts you requested...</DIV>\n");
			printf("<DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
			printf("and check the authorization options in your CGI configuration file.</DIV>\n");
		        }
		else{
			printf("<DIV CLASS='infoMessage'>There doesn't appear to be any host status information in the status log...<br><br>\n");
			printf("Make sure that Nagios is running and that you have specified the location of you status log correctly in the configuration files.</DIV>\n");
		        }

		printf("</DIV></P>\n");
	        }

	/* we dcouldn't find the servicegroup */
	else if(servicegroup_error==TRUE){
		printf("<P><DIV ALIGN=CENTER>\n");
		printf("<DIV CLASS='errorMessage'>Sorry, but servicegroup '%s' doesn't seem to exist...</DIV>\n",servicegroup_name);
		printf("</DIV></P>\n");
	        }

	return;
        }


/* displays status grid for a specific servicegroup */
void show_servicegroup_grid(servicegroup *temp_servicegroup){
	char *status_bg_class="";
	char *host_status_class="";
	char *service_status_class="";
	host *temp_host;
	service *temp_service;
	hoststatus *temp_hoststatus;
	servicestatus *temp_servicestatus;
	hostextinfo *temp_hostextinfo;
	int odd;
	int current_item;


	printf("<P>\n");
	printf("<DIV ALIGN=CENTER>\n");

	printf("<DIV CLASS='status'><A HREF='%s?servicegroup=%s&style=detail'>%s</A>",STATUS_CGI,url_encode(temp_servicegroup->group_name),temp_servicegroup->alias);
	printf(" (<A HREF='%s?type=%d&servicegroup=%s'>%s</A>)</DIV>",EXTINFO_CGI,DISPLAY_SERVICEGROUP_INFO,url_encode(temp_servicegroup->group_name),temp_servicegroup->group_name);

	printf("<TABLE BORDER=1 CLASS='status' ALIGN=CENTER>\n");
	printf("<TR><TH CLASS='status'>Host</TH><TH CLASS='status'>Services</a></TH><TH CLASS='status'>Actions</TH></TR>\n");

	/* display info for all services in the servicegroup */
	for(temp_host=host_list;temp_host;temp_host=temp_host->next){

		/* see if this host has a service that belongs to the servicegroup */
		for(temp_service=service_list;temp_service;temp_service=temp_service->next){

			/* skip this service if it's not associate with the host */
			if(strcmp(temp_service->host_name,temp_host->name))
				continue;

			/* is this service a member of the servicegroup? */
			if(is_service_member_of_servicegroup(temp_servicegroup,temp_service)==FALSE)
				continue;

			/* is user authorized for this service? */
			if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
				continue;

			break;
		        }
		if(temp_service==NULL)
			continue;

		if(odd==1){
			status_bg_class="Even";
			odd=0;
		        }
		else{
			status_bg_class="Odd";
			odd=1;
		        }

		printf("<TR CLASS='status%s'>\n",status_bg_class);

		/* get the status of the host */
		temp_hoststatus=find_hoststatus(temp_host->name);
		if(temp_hoststatus==NULL)
			host_status_class="NULL";
		else if(temp_hoststatus->status==HOST_DOWN)
			host_status_class="HOSTDOWN";
		else if(temp_hoststatus->status==HOST_UNREACHABLE)
			host_status_class="HOSTUNREACHABLE";
		else
			host_status_class=status_bg_class;

		printf("<TD CLASS='status%s'>",host_status_class);

		printf("<TABLE BORDER=0 WIDTH='100%%' cellpadding=0 cellspacing=0>\n");
		printf("<TR>\n");
		printf("<TD ALIGN=LEFT>\n");
		printf("<TABLE BORDER=0 cellpadding=0 cellspacing=0>\n");
		printf("<TR>\n");
		printf("<TD align=left valign=center CLASS='status%s'>",host_status_class);
		printf("<A HREF='%s?type=%d&host=%s'>%s</A>\n",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_host->name),temp_host->name);
		printf("</TD>\n");
		printf("</TR>\n");
		printf("</TABLE>\n");
		printf("</TD>\n");
		printf("<TD align=right valign=center nowrap>\n");
		printf("<TABLE BORDER=0 cellpadding=0 cellspacing=0>\n");
		printf("<TR>\n");

		temp_hostextinfo=find_hostextinfo(temp_host->name);
		if(temp_hostextinfo!=NULL){
			if(temp_hostextinfo->icon_image!=NULL){
				printf("<TD align=center valign=center>");
				printf("<A HREF='%s?type=%d&host=%s'>\n",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_host->name));
				printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_logo_images_path,temp_hostextinfo->icon_image,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,(temp_hostextinfo->icon_image_alt==NULL)?"":temp_hostextinfo->icon_image_alt);
				printf("</A>");
				printf("<TD>\n");
			        }
		        }

		printf("</TR>\n");
		printf("</TABLE>\n");
		printf("</TD>\n");
		printf("</TR>\n");
		printf("</TABLE>\n");

		printf("</TD>\n");

		printf("<TD CLASS='status%s'>",host_status_class);

		/* display all services on the host */
		current_item=1;
		for(temp_service=service_list;temp_service;temp_service=temp_service->next){

			/* skip this service if it's not associate with the host */
			if(strcmp(temp_service->host_name,temp_host->name))
				continue;

			/* is this service a member of the servicegroup? */
			if(is_service_member_of_servicegroup(temp_servicegroup,temp_service)==FALSE)
				continue;

			/* is user authorized for this service? */
			if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
				continue;
				
			if(current_item>max_grid_width && max_grid_width>0){
				printf("<BR>\n");
				current_item=1;
		                }

			/* get the status of the service */
			temp_servicestatus=find_servicestatus(temp_service->host_name,temp_service->description);
			if(temp_servicestatus==NULL)
				service_status_class="NULL";
			else if(temp_servicestatus->status==SERVICE_OK)
				service_status_class="OK";
			else if(temp_servicestatus->status==SERVICE_WARNING)
				service_status_class="WARNING";
			else if(temp_servicestatus->status==SERVICE_UNKNOWN)
				service_status_class="UNKNOWN";
			else if(temp_servicestatus->status==SERVICE_CRITICAL)
				service_status_class="CRITICAL";
			else
				service_status_class="PENDING";

			printf("<A HREF='%s?type=%d&host=%s",EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_servicestatus->host_name));
			printf("&service=%s' CLASS='status%s'>%s</A>&nbsp;",url_encode(temp_servicestatus->description),service_status_class,temp_servicestatus->description);

			current_item++;
	                }

		/* actions */
		printf("<TD CLASS='status%s'>",host_status_class);

		printf("<A HREF='%s?type=%d&host=%s'>\n",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_host->name));
		printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,DETAIL_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"View Extended Information For This Host");
		printf("</A>");

		if(temp_hostextinfo!=NULL){
			if(temp_hostextinfo->notes_url!=NULL){
				printf("<A HREF='");
				print_extra_host_url(temp_hostextinfo->host_name,temp_hostextinfo->notes_url);
				printf("' TARGET='_blank'>");
				printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,NOTES_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"View Extra Host Notes");
				printf("</A>");
			        }
			if(temp_hostextinfo->action_url!=NULL){
				printf("<A HREF='");
				print_extra_host_url(temp_hostextinfo->host_name,temp_hostextinfo->action_url);
				printf("' TARGET='_blank'>");
				printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,ACTION_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"Perform Extra Host Actions");
				printf("</A>");
			        }
		        }

		printf("<a href='%s?host=%s'><img src='%s%s' border=0 alt='View Service Details For This Host'></a></TD>\n",STATUS_CGI,url_encode(temp_host->name),url_images_path,STATUS_DETAIL_ICON);

		printf("</TD>\n");
		printf("</TR>\n");
		}

	printf("</TABLE>\n");
	printf("</DIV>\n");
	printf("</P>\n");

	return;
        }



/* show an overview of hostgroup(s)... */
void show_hostgroup_overviews(void){
	hostgroup *temp_hostgroup=NULL;
	int current_column;
	int user_has_seen_something=FALSE;
	int hostgroup_error=FALSE;


	printf("<P>\n");

	printf("<table border=0 width=100%%>\n");
	printf("<tr>\n");

	printf("<td valign=top align=left width=33%%>\n");

	show_filters();

	printf("</td>");

	printf("<td valign=top align=center width=33%%>\n");

	printf("<DIV ALIGN=CENTER CLASS='statusTitle'>Service Overview For ");
	if(show_all_hostgroups==TRUE)
		printf("All Host Groups");
	else
		printf("Host Group '%s'",hostgroup_name);
	printf("</DIV>\n");

	printf("<br>");

	printf("</td>\n");

	printf("<td valign=top align=right width=33%%></td>\n");
	
	printf("</tr>\n");
	printf("</table>\n");

	printf("</P>\n");


	/* display status overviews for all hostgroups */
	if(show_all_hostgroups==TRUE){


		printf("<DIV ALIGN=center>\n");
		printf("<TABLE BORDER=0 CELLPADDING=10>\n");

		current_column=1;

		/* loop through all hostgroups... */
		for(temp_hostgroup=hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){

			/* make sure the user is authorized to view at least one host in this hostgroup */
			if(is_authorized_for_hostgroup(temp_hostgroup,&current_authdata)==FALSE)
				continue;

			if(current_column==1)
				printf("<TR>\n");
			printf("<TD VALIGN=top ALIGN=center>\n");
				
			show_hostgroup_overview(temp_hostgroup);

			user_has_seen_something=TRUE;

			printf("</TD>\n");
			if(current_column==overview_columns)
				printf("</TR>\n");

			if(current_column<overview_columns)
				current_column++;
			else
				current_column=1;
		        }

		if(current_column!=1){

			for(;current_column<=overview_columns;current_column++)
				printf("<TD></TD>\n");
			printf("</TR>\n");
		        }

		printf("</TABLE>\n");
		printf("</DIV>\n");
	        }

	/* else display overview for just a specific hostgroup */
	else{

		temp_hostgroup=find_hostgroup(hostgroup_name);
		if(temp_hostgroup!=NULL){

			printf("<P>\n");
			printf("<DIV ALIGN=CENTER>\n");
			printf("<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0><TR><TD ALIGN=CENTER>\n");
			
			if(is_authorized_for_hostgroup(temp_hostgroup,&current_authdata)==TRUE){

				show_hostgroup_overview(temp_hostgroup);
				
				user_has_seen_something=TRUE;
			        }

			printf("</TD></TR></TABLE>\n");
			printf("</DIV>\n");
			printf("</P>\n");
		        }
		else{
			printf("<DIV CLASS='errorMessage'>Sorry, but host group '%s' doesn't seem to exist...</DIV>",hostgroup_name);
			hostgroup_error=TRUE;
		        }
	        }

	/* if user couldn't see anything, print out some helpful info... */
	if(user_has_seen_something==FALSE && hostgroup_error==FALSE){

		printf("<p>\n");
		printf("<div align='center'>\n");

		if(hoststatus_list!=NULL){
			printf("<DIV CLASS='errorMessage'>It appears as though you do not have permission to view information for any of the hosts you requested...</DIV>\n");
			printf("<DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
			printf("and check the authorization options in your CGI configuration file.</DIV>\n");
		        }
		else{
			printf("<DIV CLASS='infoMessage'>There doesn't appear to be any host status information in the status log...<br><br>\n");
			printf("Make sure that Nagios is running and that you have specified the location of you status log correctly in the configuration files.</DIV>\n");
		        }

		printf("</div>\n");
		printf("</p>\n");
	        }

	return;
        }



/* shows an overview of a specific hostgroup... */
void show_hostgroup_overview(hostgroup *hstgrp){
	host *temp_host;
	hoststatus *temp_hoststatus=NULL;
	int odd=0;

	/* make sure the user is authorized to view at least one host in this hostgroup */
	if(is_authorized_for_hostgroup(hstgrp,&current_authdata)==FALSE)
		return;

	printf("<DIV CLASS='status'>\n");
	printf("<A HREF='%s?hostgroup=%s&style=detail'>%s</A>",STATUS_CGI,url_encode(hstgrp->group_name),hstgrp->alias);
	printf(" (<A HREF='%s?type=%d&hostgroup=%s'>%s</A>)",EXTINFO_CGI,DISPLAY_HOSTGROUP_INFO,url_encode(hstgrp->group_name),hstgrp->group_name);
	printf("</DIV>\n");

	printf("<DIV CLASS='status'>\n");
	printf("<table border=1 CLASS='status'>\n");

	printf("<TR>\n");
	printf("<TH CLASS='status'>Host</TH><TH CLASS='status'>Status</TH><TH CLASS='status'>Services</TH><TH CLASS='status'>Actions</TH>\n");
	printf("</TR>\n");

	/* find all the hosts that belong to the hostgroup */
	for(temp_hoststatus=hoststatus_list;temp_hoststatus!=NULL;temp_hoststatus=temp_hoststatus->next){

		/* find the host... */
		temp_host=find_host(temp_hoststatus->host_name);

		/* make sure this host a member of the hostgroup */
		if(!is_host_member_of_hostgroup(hstgrp,temp_host))
			continue;

		/* make sure the user is authorized to see this host */
		if(is_authorized_for_host(temp_host,&current_authdata)==FALSE)
			continue;

		/* make sure we only display hosts of the specified status levels */
		if(!(host_status_types & temp_hoststatus->status))
			continue;

		/* make sure we only display hosts that have the desired properties */
		if(passes_host_properties_filter(temp_hoststatus)==FALSE)
			continue;

		if(odd)
			odd=0;
		else
			odd=1;

		show_servicegroup_hostgroup_member_overview(temp_hoststatus,odd,NULL);
	        }

	printf("</table>\n");
	printf("</DIV>\n");

	return;
        }

 

/* shows a host status overview... */
void show_servicegroup_hostgroup_member_overview(hoststatus *hststatus,int odd,void *data){
	char status[MAX_INPUT_BUFFER];
	char *status_bg_class="";
	char *status_class="";
	hostextinfo *temp_hostextinfo;

	if(hststatus->status==HOST_PENDING){
		strncpy(status,"PENDING",sizeof(status));
		status_class="HOSTPENDING";
		status_bg_class=(odd)?"Even":"Odd";
	        }
	else if(hststatus->status==HOST_UP){
		strncpy(status,"UP",sizeof(status));
		status_class="HOSTUP";
		status_bg_class=(odd)?"Even":"Odd";
	        }
	else if(hststatus->status==HOST_DOWN){
		strncpy(status,"DOWN",sizeof(status));
		status_class="HOSTDOWN";
		status_bg_class="HOSTDOWN";
	        }
	else if(hststatus->status==HOST_UNREACHABLE){
		strncpy(status,"UNREACHABLE",sizeof(status));
		status_class="HOSTUNREACHABLE";
		status_bg_class="HOSTUNREACHABLE";
	        }

	status[sizeof(status)-1]='\x0';

	printf("<TR CLASS='status%s'>\n",status_bg_class);

	/* find extended information for this host */
	temp_hostextinfo=find_hostextinfo(hststatus->host_name);

	printf("<TD CLASS='status%s'>\n",status_bg_class);

	printf("<TABLE BORDER=0 WIDTH=100%% cellpadding=0 cellspacing=0>\n");
	printf("<TR CLASS='status%s'>\n",status_bg_class);
	printf("<TD CLASS='status%s'><A HREF='%s?host=%s&style=detail'>%s</A></TD>\n",status_bg_class,STATUS_CGI,url_encode(hststatus->host_name),hststatus->host_name);

	if(temp_hostextinfo!=NULL){
		if(temp_hostextinfo->icon_image!=NULL){
			printf("<TD CLASS='status%s' WIDTH=5></TD>\n",status_bg_class);
			printf("<TD CLASS='status%s' ALIGN=right>",status_bg_class);
			printf("<a href='%s?type=%d&host=%s'>",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(hststatus->host_name));
			printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_logo_images_path,temp_hostextinfo->icon_image,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,(temp_hostextinfo->icon_image_alt==NULL)?"":temp_hostextinfo->icon_image_alt);
			printf("</A>");
			printf("</TD>\n");
	                }
	        }
	printf("</TR>\n");
	printf("</TABLE>\n");
	printf("</TD>\n");

	printf("<td CLASS='status%s'>%s</td>\n",status_class,status);

	printf("<td CLASS='status%s'>\n",status_bg_class);
	show_servicegroup_hostgroup_member_service_status_totals(hststatus->host_name,data);
	printf("</td>\n");

	printf("<td valign=center CLASS='status%s'>",status_bg_class);
	printf("<a href='%s?type=%d&host=%s'><img src='%s%s' border=0 alt='View Extended Information For This Host'></a>\n",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(hststatus->host_name),url_images_path,DETAIL_ICON);
	if(temp_hostextinfo!=NULL){
		if(temp_hostextinfo->notes_url!=NULL){
			printf("<A HREF='");
			print_extra_host_url(temp_hostextinfo->host_name,temp_hostextinfo->notes_url);
			printf("' TARGET='_blank'>");
			printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,NOTES_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"View Extra Host Notes");
			printf("</A>");
		        }
		if(temp_hostextinfo->action_url!=NULL){
			printf("<A HREF='");
			print_extra_host_url(temp_hostextinfo->host_name,temp_hostextinfo->action_url);
			printf("' TARGET='_blank'>");
			printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,ACTION_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"Perform Extra Host Actions");
			printf("</A>");
		        }
	        }
	printf("<a href='%s?host=%s'><img src='%s%s' border=0 alt='View Service Details For This Host'></a>\n",STATUS_CGI,url_encode(hststatus->host_name),url_images_path,STATUS_DETAIL_ICON);
	printf("</td>");

	printf("</TR>\n");

	return;
        }



void show_servicegroup_hostgroup_member_service_status_totals(char *host_name,void *data){
	int total_ok=0;
	int total_warning=0;
	int total_unknown=0;
	int total_critical=0;
	int total_pending=0;
	servicestatus *temp_servicestatus;
	service *temp_service;
	servicegroup *temp_servicegroup;
	char temp_buffer[MAX_INPUT_BUFFER];


	if(display_type==DISPLAY_SERVICEGROUPS)
		temp_servicegroup=(servicegroup *)data;

	/* check all services... */
	for(temp_servicestatus=servicestatus_list;temp_servicestatus!=NULL;temp_servicestatus=temp_servicestatus->next){

		if(!strcmp(host_name,temp_servicestatus->host_name)){

			/* make sure the user is authorized to see this service... */
			temp_service=find_service(temp_servicestatus->host_name,temp_servicestatus->description);
			if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
				continue;

			if(display_type==DISPLAY_SERVICEGROUPS){

				/* is this service a member of the servicegroup? */
				if(is_service_member_of_servicegroup(temp_servicegroup,temp_service)==FALSE)
					continue;
			        }

			/* make sure we only display services of the specified status levels */
			if(!(service_status_types & temp_servicestatus->status))
				continue;

			/* make sure we only display services that have the desired properties */
			if(passes_service_properties_filter(temp_servicestatus)==FALSE)
				continue;

			if(temp_servicestatus->status==SERVICE_CRITICAL)
				total_critical++;
			else if(temp_servicestatus->status==SERVICE_WARNING)
				total_warning++;
			else if(temp_servicestatus->status==SERVICE_UNKNOWN)
				total_unknown++;
			else if(temp_servicestatus->status==SERVICE_OK)
				total_ok++;
			else if(temp_servicestatus->status==SERVICE_PENDING)
				total_pending++;
			else
				total_ok++;
		        }
	        }


	printf("<TABLE BORDER=0 WIDTH=100%%>\n");

	if(display_type==DISPLAY_SERVICEGROUPS)
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"servicegroup=%s&style=detail",url_encode(temp_servicegroup->group_name));
	else
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"host=%s",url_encode(host_name));
	temp_buffer[sizeof(temp_buffer)-1]='\x0';

	if(total_ok>0)
		printf("<TR><TD CLASS='miniStatusOK'><A HREF='%s?%s&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d OK</A></TD></TR>\n",STATUS_CGI,temp_buffer,SERVICE_OK,host_status_types,service_properties,host_properties,total_ok);
	if(total_warning>0)
		printf("<TR><TD CLASS='miniStatusWARNING'><A HREF='%s?%s&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d WARNING</A></TD></TR>\n",STATUS_CGI,temp_buffer,SERVICE_WARNING,host_status_types,service_properties,host_properties,total_warning);
	if(total_unknown>0)
		printf("<TR><TD CLASS='miniStatusUNKNOWN'><A HREF='%s?%s&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d UNKNOWN</A></TD></TR>\n",STATUS_CGI,temp_buffer,SERVICE_UNKNOWN,host_status_types,service_properties,host_properties,total_unknown);
	if(total_critical>0)
		printf("<TR><TD CLASS='miniStatusCRITICAL'><A HREF='%s?%s&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d CRITICAL</A></TD></TR>\n",STATUS_CGI,temp_buffer,SERVICE_CRITICAL,host_status_types,service_properties,host_properties,total_critical);
	if(total_pending>0)
		printf("<TR><TD CLASS='miniStatusPENDING'><A HREF='%s?%s&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d PENDING</A></TD></TR>\n",STATUS_CGI,temp_buffer,SERVICE_PENDING,host_status_types,service_properties,host_properties,total_pending);

	printf("</TABLE>\n");

	if((total_ok + total_warning + total_unknown + total_critical + total_pending)==0)
		printf("No matching services");

	return;
        }



/* show a summary of hostgroup(s)... */
void show_hostgroup_summaries(void){
	hostgroup *temp_hostgroup=NULL;
	int user_has_seen_something=FALSE;
	int hostgroup_error=FALSE;
	int odd=0;


	printf("<P>\n");

	printf("<table border=0 width=100%%>\n");
	printf("<tr>\n");

	printf("<td valign=top align=left width=33%%>\n");

	show_filters();

	printf("</td>");

	printf("<td valign=top align=center width=33%%>\n");

	printf("<DIV ALIGN=CENTER CLASS='statusTitle'>Status Summary For ");
	if(show_all_hostgroups==TRUE)
		printf("All Host Groups");
	else
		printf("Host Group '%s'",hostgroup_name);
	printf("</DIV>\n");

	printf("<br>");

	printf("</td>\n");

	printf("<td valign=top align=right width=33%%></td>\n");
	
	printf("</tr>\n");
	printf("</table>\n");

	printf("</P>\n");


	printf("<DIV ALIGN=center>\n");
	printf("<table border=1 CLASS='status'>\n");

	printf("<TR>\n");
	printf("<TH CLASS='status'>Host Group</TH><TH CLASS='status'>Host Status Totals</TH><TH CLASS='status'>Service Status Totals</TH>\n");
	printf("</TR>\n");

	/* display status summary for all hostgroups */
	if(show_all_hostgroups==TRUE){

		/* loop through all hostgroups... */
		for(temp_hostgroup=hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){

			/* make sure the user is authorized to view at least one host in this hostgroup */
			if(is_authorized_for_hostgroup(temp_hostgroup,&current_authdata)==FALSE)
				continue;

			if(odd==0)
				odd=1;
			else
				odd=0;

			/* show summary for this hostgroup */
			show_hostgroup_summary(temp_hostgroup,odd);

			user_has_seen_something=TRUE;
		        }

	        }

	/* else just show summary for a specific hostgroup */
	else{
		temp_hostgroup=find_hostgroup(hostgroup_name);
		if(temp_hostgroup==NULL)
			hostgroup_error=TRUE;
		else{
			show_hostgroup_summary(temp_hostgroup,1);
			user_has_seen_something=TRUE;
		        }
	        }

	printf("</TABLE>\n");
	printf("</DIV>\n");

	/* if user couldn't see anything, print out some helpful info... */
	if(user_has_seen_something==FALSE && hostgroup_error==FALSE){

		printf("<P><DIV ALIGN=CENTER>\n");

		if(hoststatus_list!=NULL){
			printf("<DIV CLASS='errorMessage'>It appears as though you do not have permission to view information for any of the hosts you requested...</DIV>\n");
			printf("<DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
			printf("and check the authorization options in your CGI configuration file.</DIV>\n");
		        }
		else{
			printf("<DIV CLASS='infoMessage'>There doesn't appear to be any host status information in the status log...<br><br>\n");
			printf("Make sure that Nagios is running and that you have specified the location of you status log correctly in the configuration files.</DIV>\n");
		        }

		printf("</DIV></P>\n");
	        }

	/* we dcouldn't find the hostgroup */
	else if(hostgroup_error==TRUE){
		printf("<P><DIV ALIGN=CENTER>\n");
		printf("<DIV CLASS='errorMessage'>Sorry, but hostgroup '%s' doesn't seem to exist...</DIV>\n",hostgroup_name);
		printf("</DIV></P>\n");
	        }

	return;
        }



/* displays status summary information for a specific hostgroup */
void show_hostgroup_summary(hostgroup *temp_hostgroup,int odd){
	char *status_bg_class="";

	if(odd==1)
		status_bg_class="Even";
	else
		status_bg_class="Odd";

	printf("<TR CLASS='status%s'><TD CLASS='status%s'>\n",status_bg_class,status_bg_class);
	printf("<A HREF='%s?hostgroup=%s&style=overview'>%s</A> ",STATUS_CGI,url_encode(temp_hostgroup->group_name),temp_hostgroup->alias);
	printf("(<A HREF='%s?type=%d&hostgroup=%s'>%s</a>)",EXTINFO_CGI,DISPLAY_HOSTGROUP_INFO,url_encode(temp_hostgroup->group_name),temp_hostgroup->group_name);
	printf("</TD>");
				
	printf("<TD CLASS='status%s' ALIGN=CENTER VALIGN=CENTER>",status_bg_class);
	show_hostgroup_host_totals_summary(temp_hostgroup);
	printf("</TD>");

	printf("<TD CLASS='status%s' ALIGN=CENTER VALIGN=CENTER>",status_bg_class);
	show_hostgroup_service_totals_summary(temp_hostgroup);
	printf("</TD>");

	printf("</TR>\n");

	return;
        }



/* shows host total summary information for a specific hostgroup */
void show_hostgroup_host_totals_summary(hostgroup *temp_hostgroup){
	int total_up=0;
	int total_down=0;
	int total_unreachable=0;
	int total_pending=0;
	hoststatus *temp_hoststatus;
	host *temp_host;

	/* check all hosts... */
	for(temp_hoststatus=hoststatus_list;temp_hoststatus!=NULL;temp_hoststatus=temp_hoststatus->next){

		/* make sure the user is authorized to see this host... */
		temp_host=find_host(temp_hoststatus->host_name);
		if(is_authorized_for_host(temp_host,&current_authdata)==FALSE)
			continue;

		/* make sure this host is a member of the specified hostgroup */
		if(is_host_member_of_hostgroup(temp_hostgroup,temp_host)==FALSE)
			continue;

		/* make sure we only display hosts of the specified status levels */
		if(!(host_status_types & temp_hoststatus->status))
			continue;

		/* make sure we only display hosts that have the desired properties */
		if(passes_host_properties_filter(temp_hoststatus)==FALSE)
			continue;

		if(temp_hoststatus->status==HOST_UP)
			total_up++;
		else if(temp_hoststatus->status==HOST_DOWN)
			total_down++;
		else if(temp_hoststatus->status==HOST_UNREACHABLE)
			total_unreachable++;
		else
			total_pending++;
	        }

	printf("<TABLE BORDER=0>\n");

	if(total_up>0)
		printf("<TR><TD CLASS='miniStatusUP'><A HREF='%s?hostgroup=%s&style=detail&&hoststatustypes=%d&hostprops=%lu'>%d UP</A></TD></TR>\n",STATUS_CGI,url_encode(temp_hostgroup->group_name),HOST_UP,host_properties,total_up);
	if(total_down>0)
		printf("<TR><TD CLASS='miniStatusDOWN'><A HREF='%s?hostgroup=%s&style=detail&hoststatustypes=%d&hostprops=%lu'>%d DOWN</A></TD></TR>\n",STATUS_CGI,url_encode(temp_hostgroup->group_name),HOST_DOWN,host_properties,total_down);
	if(total_unreachable>0)
		printf("<TR><TD CLASS='miniStatusUNREACHABLE'><A HREF='%s?hostgroup=%s&style=detail&hoststatustypes=%d&hostprops=%lu'>%d UNREACHABLE</A></TD></TR>\n",STATUS_CGI,url_encode(temp_hostgroup->group_name),HOST_UNREACHABLE,host_properties,total_unreachable);
	if(total_pending>0)
		printf("<TR><TD CLASS='miniStatusPENDING'><A HREF='%s?hostgroup=%s&style=detail&hoststatustypes=%d&hostprops=%lu'>%d PENDING</A></TD></TR>\n",STATUS_CGI,url_encode(temp_hostgroup->group_name),HOST_PENDING,host_properties,total_pending);

	printf("</TABLE>\n");

	if((total_up + total_down + total_unreachable + total_pending)==0)
		printf("No matching hosts");

	return;
        }



/* shows service total summary information for a specific hostgroup */
void show_hostgroup_service_totals_summary(hostgroup *temp_hostgroup){
	int total_ok=0;
	int total_warning=0;
	int total_unknown=0;
	int total_critical=0;
	int total_pending=0;
	servicestatus *temp_servicestatus;
	service *temp_service;
	hoststatus *temp_hoststatus;
	host *temp_host;


	/* check all services... */
	for(temp_servicestatus=servicestatus_list;temp_servicestatus!=NULL;temp_servicestatus=temp_servicestatus->next){

		/* make sure the user is authorized to see this service... */
		temp_service=find_service(temp_servicestatus->host_name,temp_servicestatus->description);
		if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
			continue;

		/* find the host this service is associated with */
		temp_host=find_host(temp_servicestatus->host_name);
		if(temp_host==NULL)
			continue;

		/* see if this service is associated with a host in the specified hostgroup */
		if(is_host_member_of_hostgroup(temp_hostgroup,temp_host)==FALSE)
			continue;

		/* find the status of the associated host */
		temp_hoststatus=find_hoststatus(temp_servicestatus->host_name);
		if(temp_hoststatus==NULL)
			continue;

		/* make sure we only display hosts of the specified status levels */
		if(!(host_status_types & temp_hoststatus->status))
			continue;

		/* make sure we only display hosts that have the desired properties */
		if(passes_host_properties_filter(temp_hoststatus)==FALSE)
			continue;

		/* make sure we only display services of the specified status levels */
		if(!(service_status_types & temp_servicestatus->status))
			continue;

		/* make sure we only display services that have the desired properties */
		if(passes_service_properties_filter(temp_servicestatus)==FALSE)
			continue;

		if(temp_servicestatus->status==SERVICE_CRITICAL)
			total_critical++;
		else if(temp_servicestatus->status==SERVICE_WARNING)
			total_warning++;
		else if(temp_servicestatus->status==SERVICE_UNKNOWN)
			total_unknown++;
		else if(temp_servicestatus->status==SERVICE_OK)
			total_ok++;
		else if(temp_servicestatus->status==SERVICE_PENDING)
			total_pending++;
		else
			total_ok++;
	        }


	printf("<TABLE BORDER=0>\n");

	if(total_ok>0)
		printf("<TR><TD CLASS='miniStatusOK'><A HREF='%s?hostgroup=%s&style=detail&&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d OK</A></TD></TR>\n",STATUS_CGI,url_encode(temp_hostgroup->group_name),SERVICE_OK,host_status_types,service_properties,host_properties,total_ok);
	if(total_warning>0)
		printf("<TR><TD CLASS='miniStatusWARNING'><A HREF='%s?hostgroup=%s&style=detail&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d WARNING</A></TD></TR>\n",STATUS_CGI,url_encode(temp_hostgroup->group_name),SERVICE_WARNING,host_status_types,service_properties,host_properties,total_warning);
	if(total_unknown>0)
		printf("<TR><TD CLASS='miniStatusUNKNOWN'><A HREF='%s?hostgroup=%s&style=detail&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d UNKNOWN</A></TD></TR>\n",STATUS_CGI,url_encode(temp_hostgroup->group_name),SERVICE_UNKNOWN,host_status_types,service_properties,host_properties,total_unknown);
	if(total_critical>0)
		printf("<TR><TD CLASS='miniStatusCRITICAL'><A HREF='%s?hostgroup=%s&style=detail&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d CRITICAL</A></TD></TR>\n",STATUS_CGI,url_encode(temp_hostgroup->group_name),SERVICE_CRITICAL,host_status_types,service_properties,host_properties,total_critical);
	if(total_pending>0)
		printf("<TR><TD CLASS='miniStatusPENDING'><A HREF='%s?hostgroup=%s&style=detail&servicestatustypes=%d&hoststatustypes=%d&serviceprops=%lu&hostprops=%lu'>%d PENDING</A></TD></TR>\n",STATUS_CGI,url_encode(temp_hostgroup->group_name),SERVICE_PENDING,host_status_types,service_properties,host_properties,total_pending);

	printf("</TABLE>\n");

	if((total_ok + total_warning + total_unknown + total_critical + total_pending)==0)
		printf("No matching services");

	return;
        }



/* show a grid layout of hostgroup(s)... */
void show_hostgroup_grids(void){
	hostgroup *temp_hostgroup=NULL;
	int user_has_seen_something=FALSE;
	int hostgroup_error=FALSE;
	int odd=0;


	printf("<P>\n");

	printf("<table border=0 width=100%%>\n");
	printf("<tr>\n");

	printf("<td valign=top align=left width=33%%>\n");

	show_filters();

	printf("</td>");

	printf("<td valign=top align=center width=33%%>\n");

	printf("<DIV ALIGN=CENTER CLASS='statusTitle'>Status Grid For ");
	if(show_all_hostgroups==TRUE)
		printf("All Host Groups");
	else
		printf("Host Group '%s'",hostgroup_name);
	printf("</DIV>\n");

	printf("<br>");

	printf("</td>\n");

	printf("<td valign=top align=right width=33%%></td>\n");
	
	printf("</tr>\n");
	printf("</table>\n");

	printf("</P>\n");


	/* display status grids for all hostgroups */
	if(show_all_hostgroups==TRUE){

		/* loop through all hostgroups... */
		for(temp_hostgroup=hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){

			/* make sure the user is authorized to view at least one host in this hostgroup */
			if(is_authorized_for_hostgroup(temp_hostgroup,&current_authdata)==FALSE)
				continue;

			if(odd==0)
				odd=1;
			else
				odd=0;

			/* show grid for this hostgroup */
			show_hostgroup_grid(temp_hostgroup);

			user_has_seen_something=TRUE;
		        }

	        }

	/* else just show grid for a specific hostgroup */
	else{
		temp_hostgroup=find_hostgroup(hostgroup_name);
		if(temp_hostgroup==NULL)
			hostgroup_error=TRUE;
		else{
			show_hostgroup_grid(temp_hostgroup);
			user_has_seen_something=TRUE;
		        }
	        }

	/* if user couldn't see anything, print out some helpful info... */
	if(user_has_seen_something==FALSE && hostgroup_error==FALSE){

		printf("<P><DIV ALIGN=CENTER>\n");

		if(hoststatus_list!=NULL){
			printf("<DIV CLASS='errorMessage'>It appears as though you do not have permission to view information for any of the hosts you requested...</DIV>\n");
			printf("<DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
			printf("and check the authorization options in your CGI configuration file.</DIV>\n");
		        }
		else{
			printf("<DIV CLASS='infoMessage'>There doesn't appear to be any host status information in the status log...<br><br>\n");
			printf("Make sure that Nagios is running and that you have specified the location of you status log correctly in the configuration files.</DIV>\n");
		        }

		printf("</DIV></P>\n");
	        }

	/* we dcouldn't find the hostgroup */
	else if(hostgroup_error==TRUE){
		printf("<P><DIV ALIGN=CENTER>\n");
		printf("<DIV CLASS='errorMessage'>Sorry, but hostgroup '%s' doesn't seem to exist...</DIV>\n",hostgroup_name);
		printf("</DIV></P>\n");
	        }

	return;
        }


/* displays status grid for a specific hostgroup */
void show_hostgroup_grid(hostgroup *temp_hostgroup){
	char *status_bg_class="";
	char *host_status_class="";
	char *service_status_class="";
	host *temp_host;
	service *temp_service;
	hoststatus *temp_hoststatus;
	servicestatus *temp_servicestatus;
	hostextinfo *temp_hostextinfo;
	int odd;
	int current_item;


	printf("<P>\n");
	printf("<DIV ALIGN=CENTER>\n");

	printf("<DIV CLASS='status'><A HREF='%s?hostgroup=%s&style=detail'>%s</A>",STATUS_CGI,url_encode(temp_hostgroup->group_name),temp_hostgroup->alias);
	printf(" (<A HREF='%s?type=%d&hostgroup=%s'>%s</A>)</DIV>",EXTINFO_CGI,DISPLAY_HOSTGROUP_INFO,url_encode(temp_hostgroup->group_name),temp_hostgroup->group_name);

	printf("<TABLE BORDER=1 CLASS='status' ALIGN=CENTER>\n");
	printf("<TR><TH CLASS='status'>Host</TH><TH CLASS='status'>Services</a></TH><TH CLASS='status'>Actions</TH></TR>\n");

	/* display info for all hosts in the hostgroup */
	for(temp_host=host_list;temp_host;temp_host=temp_host->next){

		/* is this host a member of the hostgroup? */
		if(is_host_member_of_hostgroup(temp_hostgroup,temp_host)==FALSE)
			continue;

		/* is the user authorized for this host? */
		if(is_authorized_for_host(temp_host,&current_authdata)==FALSE)
			continue;

		if(odd==1){
			status_bg_class="Even";
			odd=0;
		        }
		else{
			status_bg_class="Odd";
			odd=1;
		        }

		printf("<TR CLASS='status%s'>\n",status_bg_class);

		/* get the status of the host */
		temp_hoststatus=find_hoststatus(temp_host->name);
		if(temp_hoststatus==NULL)
			host_status_class="NULL";
		else if(temp_hoststatus->status==HOST_DOWN)
			host_status_class="HOSTDOWN";
		else if(temp_hoststatus->status==HOST_UNREACHABLE)
			host_status_class="HOSTUNREACHABLE";
		else
			host_status_class=status_bg_class;

		printf("<TD CLASS='status%s'>",host_status_class);

		printf("<TABLE BORDER=0 WIDTH='100%%' cellpadding=0 cellspacing=0>\n");
		printf("<TR>\n");
		printf("<TD ALIGN=LEFT>\n");
		printf("<TABLE BORDER=0 cellpadding=0 cellspacing=0>\n");
		printf("<TR>\n");
		printf("<TD align=left valign=center CLASS='status%s'>",host_status_class);
		printf("<A HREF='%s?type=%d&host=%s'>%s</A>\n",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_host->name),temp_host->name);
		printf("</TD>\n");
		printf("</TR>\n");
		printf("</TABLE>\n");
		printf("</TD>\n");
		printf("<TD align=right valign=center nowrap>\n");
		printf("<TABLE BORDER=0 cellpadding=0 cellspacing=0>\n");
		printf("<TR>\n");

		temp_hostextinfo=find_hostextinfo(temp_host->name);
		if(temp_hostextinfo!=NULL){
			if(temp_hostextinfo->icon_image!=NULL){
				printf("<TD align=center valign=center>");
				printf("<A HREF='%s?type=%d&host=%s'>\n",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_host->name));
				printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_logo_images_path,temp_hostextinfo->icon_image,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,(temp_hostextinfo->icon_image_alt==NULL)?"":temp_hostextinfo->icon_image_alt);
				printf("</A>");
				printf("<TD>\n");
			        }
		        }
		printf("<TD>\n");

		printf("</TR>\n");
		printf("</TABLE>\n");
		printf("</TD>\n");
		printf("</TR>\n");
		printf("</TABLE>\n");

		printf("</TD>\n");

		printf("<TD CLASS='status%s'>",host_status_class);

		/* display all services on the host */
		current_item=1;
		for(temp_service=service_list;temp_service;temp_service=temp_service->next){

			/* skip this service if it's not associate with the host */
			if(strcmp(temp_service->host_name,temp_host->name))
				continue;

			/* is user authorized for this service? */
			if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
				continue;

			if(current_item>max_grid_width && max_grid_width>0){
				printf("<BR>\n");
				current_item=1;
			        }

			/* get the status of the service */
			temp_servicestatus=find_servicestatus(temp_service->host_name,temp_service->description);
			if(temp_servicestatus==NULL)
				service_status_class="NULL";
			else if(temp_servicestatus->status==SERVICE_OK)
				service_status_class="OK";
			else if(temp_servicestatus->status==SERVICE_WARNING)
				service_status_class="WARNING";
			else if(temp_servicestatus->status==SERVICE_UNKNOWN)
				service_status_class="UNKNOWN";
			else if(temp_servicestatus->status==SERVICE_CRITICAL)
				service_status_class="CRITICAL";
			else
				service_status_class="PENDING";

			printf("<A HREF='%s?type=%d&host=%s",EXTINFO_CGI,DISPLAY_SERVICE_INFO,url_encode(temp_servicestatus->host_name));
			printf("&service=%s' CLASS='status%s'>%s</A>&nbsp;",url_encode(temp_servicestatus->description),service_status_class,temp_servicestatus->description);

			current_item++;
		        }

		printf("</TD>\n");

		/* actions */
		printf("<TD CLASS='status%s'>",host_status_class);

		printf("<A HREF='%s?type=%d&host=%s'>\n",EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_host->name));
		printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,DETAIL_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"View Extended Information For This Host");
		printf("</A>");

		if(temp_hostextinfo!=NULL){
			if(temp_hostextinfo->notes_url!=NULL){
				printf("<A HREF='");
				print_extra_host_url(temp_hostextinfo->host_name,temp_hostextinfo->notes_url);
				printf("' TARGET='_blank'>");
				printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,NOTES_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"View Extra Host Notes");
				printf("</A>");
			        }
			if(temp_hostextinfo->action_url!=NULL){
				printf("<A HREF='");
				print_extra_host_url(temp_hostextinfo->host_name,temp_hostextinfo->action_url);
				printf("' TARGET='_blank'>");
				printf("<IMG SRC='%s%s' BORDER=0 WIDTH=%d HEIGHT=%d ALT='%s'>",url_images_path,ACTION_ICON,STATUS_ICON_WIDTH,STATUS_ICON_HEIGHT,"Perform Extra Host Actions");
				printf("</A>");
			        }
		        }

		printf("<a href='%s?host=%s'><img src='%s%s' border=0 alt='View Service Details For This Host'></a></TD>\n",STATUS_CGI,url_encode(temp_host->name),url_images_path,STATUS_DETAIL_ICON);

		printf("</TD>\n");

		printf("</TR>\n");
		}

	printf("</TABLE>\n");
	printf("</DIV>\n");
	printf("</P>\n");

	return;
        }




/******************************************************************/
/**********  SERVICE SORTING & FILTERING FUNCTIONS  ***************/
/******************************************************************/


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
	time_t nt;
	time_t tt;

	new_svcstatus=new_servicesort->svcstatus;
	temp_svcstatus=temp_servicesort->svcstatus;

	if(s_type==SORT_ASCENDING){

		if(s_option==SORT_LASTCHECKTIME){
			if(new_svcstatus->last_check < temp_svcstatus->last_check)
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
		else if(s_option==SORT_STATEDURATION){
			if(new_svcstatus->last_state_change==(time_t)0)
				nt=(program_start>current_time)?0:(current_time-program_start);
			else
				nt=(new_svcstatus->last_state_change>current_time)?0:(current_time-new_svcstatus->last_state_change);
			if(temp_svcstatus->last_state_change==(time_t)0)
				tt=(program_start>current_time)?0:(current_time-program_start);
			else
				tt=(temp_svcstatus->last_state_change>current_time)?0:(current_time-temp_svcstatus->last_state_change);
			if(nt<tt)
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
		else if(s_option==SORT_STATEDURATION){
			if(new_svcstatus->last_state_change==(time_t)0)
				nt=(program_start>current_time)?0:(current_time-program_start);
			else
				nt=(new_svcstatus->last_state_change>current_time)?0:(current_time-new_svcstatus->last_state_change);
			if(temp_svcstatus->last_state_change==(time_t)0)
				tt=(program_start>current_time)?0:(current_time-program_start);
			else
				tt=(temp_svcstatus->last_state_change>current_time)?0:(current_time-temp_svcstatus->last_state_change);
			if(nt>tt)
				return TRUE;
			else
				return FALSE;
		        }
	        }

	return TRUE;
        }



/* sorts the host list */
int sort_hosts(int s_type, int s_option){
	hostsort *new_hostsort;
	hostsort *last_hostsort;
	hostsort *temp_hostsort;
	hoststatus *temp_hststatus;

	if(s_type==SORT_NONE)
		return ERROR;

	if(hoststatus_list==NULL)
		return ERROR;

	/* sort all hosts status entries */
	for(temp_hststatus=hoststatus_list;temp_hststatus!=NULL;temp_hststatus=temp_hststatus->next){

		/* allocate memory for a new sort structure */
		new_hostsort=(hostsort *)malloc(sizeof(hostsort));
		if(new_hostsort==NULL)
			return ERROR;

		new_hostsort->hststatus=temp_hststatus;

		last_hostsort=hostsort_list;
		for(temp_hostsort=hostsort_list;temp_hostsort!=NULL;temp_hostsort=temp_hostsort->next){

			if(compare_hostsort_entries(s_type,s_option,new_hostsort,temp_hostsort)==TRUE){
				new_hostsort->next=temp_hostsort;
				if(temp_hostsort==hostsort_list)
					hostsort_list=new_hostsort;
				else
					last_hostsort->next=new_hostsort;
				break;
		                }
			else
				last_hostsort=temp_hostsort;
	                }

		if(hostsort_list==NULL){
			new_hostsort->next=NULL;
			hostsort_list=new_hostsort;
	                }
		else if(temp_hostsort==NULL){
			new_hostsort->next=NULL;
			last_hostsort->next=new_hostsort;
	                }
	        }

	return OK;
        }


int compare_hostsort_entries(int s_type, int s_option, hostsort *new_hostsort, hostsort *temp_hostsort){
	hoststatus *new_hststatus;
	hoststatus *temp_hststatus;
	time_t nt;
	time_t tt;

	new_hststatus=new_hostsort->hststatus;
	temp_hststatus=temp_hostsort->hststatus;

	if(s_type==SORT_ASCENDING){

		if(s_option==SORT_LASTCHECKTIME){
			if(new_hststatus->last_check < temp_hststatus->last_check)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_HOSTSTATUS){
			if(new_hststatus->status <= temp_hststatus->status)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_HOSTNAME){
			if(strcasecmp(new_hststatus->host_name,temp_hststatus->host_name)<0)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_STATEDURATION){
			if(new_hststatus->last_state_change==(time_t)0)
				nt=(program_start>current_time)?0:(current_time-program_start);
			else
				nt=(new_hststatus->last_state_change>current_time)?0:(current_time-new_hststatus->last_state_change);
			if(temp_hststatus->last_state_change==(time_t)0)
				tt=(program_start>current_time)?0:(current_time-program_start);
			else
				tt=(temp_hststatus->last_state_change>current_time)?0:(current_time-temp_hststatus->last_state_change);
			if(nt<tt)
				return TRUE;
			else
				return FALSE;
		        }
	        }
	else{
		if(s_option==SORT_LASTCHECKTIME){
			if(new_hststatus->last_check > temp_hststatus->last_check)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_HOSTSTATUS){
			if(new_hststatus->status > temp_hststatus->status)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_HOSTNAME){
			if(strcasecmp(new_hststatus->host_name,temp_hststatus->host_name)>0)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_STATEDURATION){
			if(new_hststatus->last_state_change==(time_t)0)
				nt=(program_start>current_time)?0:(current_time-program_start);
			else
				nt=(new_hststatus->last_state_change>current_time)?0:(current_time-new_hststatus->last_state_change);
			if(temp_hststatus->last_state_change==(time_t)0)
				tt=(program_start>current_time)?0:(current_time-program_start);
			else
				tt=(temp_hststatus->last_state_change>current_time)?0:(current_time-temp_hststatus->last_state_change);
			if(nt>tt)
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


/* free all memory allocated to the hostsort structures */
void free_hostsort_list(void){
	hostsort *this_hostsort;
	hostsort *next_hostsort;

	/* free memory for the hostsort list */
	for(this_hostsort=hostsort_list;this_hostsort!=NULL;this_hostsort=next_hostsort){
		next_hostsort=this_hostsort->next;
		free(this_hostsort);
	        }

	return;
        }



/* check host properties filter */
int passes_host_properties_filter(hoststatus *temp_hoststatus){

	if((host_properties & HOST_SCHEDULED_DOWNTIME) && temp_hoststatus->scheduled_downtime_depth<=0)
		return FALSE;

	if((host_properties & HOST_NO_SCHEDULED_DOWNTIME) && temp_hoststatus->scheduled_downtime_depth>0)
		return FALSE;

	if((host_properties & HOST_STATE_ACKNOWLEDGED) && temp_hoststatus->problem_has_been_acknowledged==FALSE)
		return FALSE;

	if((host_properties & HOST_STATE_UNACKNOWLEDGED) && temp_hoststatus->problem_has_been_acknowledged==TRUE)
		return FALSE;

	if((host_properties & HOST_CHECKS_DISABLED) && temp_hoststatus->checks_enabled==TRUE)
		return FALSE;

	if((host_properties & HOST_CHECKS_ENABLED) && temp_hoststatus->checks_enabled==FALSE)
		return FALSE;

	if((host_properties & HOST_EVENT_HANDLER_DISABLED) && temp_hoststatus->event_handler_enabled==TRUE)
		return FALSE;

	if((host_properties & HOST_EVENT_HANDLER_ENABLED) && temp_hoststatus->event_handler_enabled==FALSE)
		return FALSE;

	if((host_properties & HOST_FLAP_DETECTION_DISABLED) && temp_hoststatus->flap_detection_enabled==TRUE)
		return FALSE;

	if((host_properties & HOST_FLAP_DETECTION_ENABLED) && temp_hoststatus->flap_detection_enabled==FALSE)
		return FALSE;

	if((host_properties & HOST_IS_FLAPPING) && temp_hoststatus->is_flapping==FALSE)
		return FALSE;

	if((host_properties & HOST_IS_NOT_FLAPPING) && temp_hoststatus->is_flapping==TRUE)
		return FALSE;

	if((host_properties & HOST_NOTIFICATIONS_DISABLED) && temp_hoststatus->notifications_enabled==TRUE)
		return FALSE;

	if((host_properties & HOST_NOTIFICATIONS_ENABLED) && temp_hoststatus->notifications_enabled==FALSE)
		return FALSE;

	if((host_properties & HOST_PASSIVE_CHECKS_DISABLED) && temp_hoststatus->accept_passive_host_checks==TRUE)
		return FALSE;

	if((host_properties & HOST_PASSIVE_CHECKS_ENABLED) && temp_hoststatus->accept_passive_host_checks==FALSE)
		return FALSE;

	if((host_properties & HOST_PASSIVE_CHECK) && temp_hoststatus->check_type==HOST_CHECK_ACTIVE)
		return FALSE;

	if((host_properties & HOST_ACTIVE_CHECK) && temp_hoststatus->check_type==HOST_CHECK_PASSIVE)
		return FALSE;

	return TRUE;
        }



/* check service properties filter */
int passes_service_properties_filter(servicestatus *temp_servicestatus){

	if((service_properties & SERVICE_SCHEDULED_DOWNTIME) && temp_servicestatus->scheduled_downtime_depth<=0)
		return FALSE;

	if((service_properties & SERVICE_NO_SCHEDULED_DOWNTIME) && temp_servicestatus->scheduled_downtime_depth>0)
		return FALSE;

	if((service_properties & SERVICE_STATE_ACKNOWLEDGED) && temp_servicestatus->problem_has_been_acknowledged==FALSE)
		return FALSE;

	if((service_properties & SERVICE_STATE_UNACKNOWLEDGED) && temp_servicestatus->problem_has_been_acknowledged==TRUE)
		return FALSE;

	if((service_properties & SERVICE_CHECKS_DISABLED) && temp_servicestatus->checks_enabled==TRUE)
		return FALSE;

	if((service_properties & SERVICE_CHECKS_ENABLED) && temp_servicestatus->checks_enabled==FALSE)
		return FALSE;

	if((service_properties & SERVICE_EVENT_HANDLER_DISABLED) && temp_servicestatus->event_handler_enabled==TRUE)
		return FALSE;

	if((service_properties & SERVICE_EVENT_HANDLER_ENABLED) && temp_servicestatus->event_handler_enabled==FALSE)
		return FALSE;

	if((service_properties & SERVICE_FLAP_DETECTION_DISABLED) && temp_servicestatus->flap_detection_enabled==TRUE)
		return FALSE;

	if((service_properties & SERVICE_FLAP_DETECTION_ENABLED) && temp_servicestatus->flap_detection_enabled==FALSE)
		return FALSE;

	if((service_properties & SERVICE_IS_FLAPPING) && temp_servicestatus->is_flapping==FALSE)
		return FALSE;

	if((service_properties & SERVICE_IS_NOT_FLAPPING) && temp_servicestatus->is_flapping==TRUE)
		return FALSE;

	if((service_properties & SERVICE_NOTIFICATIONS_DISABLED) && temp_servicestatus->notifications_enabled==TRUE)
		return FALSE;

	if((service_properties & SERVICE_NOTIFICATIONS_ENABLED) && temp_servicestatus->notifications_enabled==FALSE)
		return FALSE;

	if((service_properties & SERVICE_PASSIVE_CHECKS_DISABLED) && temp_servicestatus->accept_passive_service_checks==TRUE)
		return FALSE;

	if((service_properties & SERVICE_PASSIVE_CHECKS_ENABLED) && temp_servicestatus->accept_passive_service_checks==FALSE)
		return FALSE;

	if((service_properties & SERVICE_PASSIVE_CHECK) && temp_servicestatus->check_type==SERVICE_CHECK_ACTIVE)
		return FALSE;

	if((service_properties & SERVICE_ACTIVE_CHECK) && temp_servicestatus->check_type==SERVICE_CHECK_PASSIVE)
		return FALSE;

	return TRUE;
        }



/* shows service and host filters in use */
void show_filters(void){
	int found=0;

	/* show filters box if necessary */
	if(host_properties!=0L || service_properties!=0L || host_status_types!=all_host_status_types || service_status_types!=all_service_status_types){

		printf("<table border=1 class='filter' cellspacing=0 cellpadding=0>\n");
		printf("<tr><td class='filter'>\n");
		printf("<table border=0 cellspacing=2 cellpadding=0>\n");
		printf("<tr><td colspan=2 valign=top align=left CLASS='filterTitle'>Display Filters:</td></tr>");
		printf("<tr><td valign=top align=left CLASS='filterName'>Host Status Types:</td>");
		printf("<td valign=top align=left CLASS='filterValue'>");
		if(host_status_types==all_host_status_types)
			printf("All");
		else if(host_status_types==all_host_problems)
			printf("All problems");
		else{
			found=0;
			if(host_status_types & HOST_PENDING){
				printf(" Pending");
				found=1;
		                }
			if(host_status_types & HOST_UP){
				printf("%s Up",(found==1)?" |":"");
				found=1;
		                }
			if(host_status_types & HOST_DOWN){
				printf("%s Down",(found==1)?" |":"");
				found=1;
		                }
			if(host_status_types & HOST_UNREACHABLE)
				printf("%s Unreachable",(found==1)?" |":"");
	                }
		printf("</td></tr>");
		printf("<tr><td valign=top align=left CLASS='filterName'>Host Properties:</td>");
		printf("<td valign=top align=left CLASS='filterValue'>");
		if(host_properties==0)
			printf("Any");
		else{
			found=0;
			if(host_properties & HOST_SCHEDULED_DOWNTIME){
				printf(" In Scheduled Downtime");
				found=1;
		                }
			if(host_properties & HOST_NO_SCHEDULED_DOWNTIME){
				printf("%s Not In Scheduled Downtime",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_STATE_ACKNOWLEDGED){
				printf("%s Has Been Acknowledged",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_STATE_UNACKNOWLEDGED){
				printf("%s Has Not Been Acknowledged",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_CHECKS_DISABLED){
				printf("%s Checks Disabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_CHECKS_ENABLED){
				printf("%s Checks Enabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_EVENT_HANDLER_DISABLED){
				printf("%s Event Handler Disabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_EVENT_HANDLER_ENABLED){
				printf("%s Event Handler Enabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_FLAP_DETECTION_DISABLED){
				printf("%s Flap Detection Disabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_FLAP_DETECTION_ENABLED){
				printf("%s Flap Detection Enabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_IS_FLAPPING){
				printf("%s Is Flapping",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_IS_NOT_FLAPPING){
				printf("%s Is Not Flapping",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_NOTIFICATIONS_DISABLED){
				printf("%s Notifications Disabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_NOTIFICATIONS_ENABLED){
				printf("%s Notifications Enabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_PASSIVE_CHECKS_DISABLED){
				printf("%s Passive Checks Disabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_PASSIVE_CHECKS_ENABLED){
				printf("%s Passive Checks Enabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_PASSIVE_CHECK){
				printf("%s Passive Checks",(found==1)?" &amp;":"");
				found=1;
		                }
			if(host_properties & HOST_ACTIVE_CHECK){
				printf("%s Active Checks",(found==1)?" &amp;":"");
				found=1;
		                }
	                }
		printf("</td>");
		printf("</tr>\n");


		printf("<tr><td valign=top align=left CLASS='filterName'>Service Status Types:</td>");
		printf("<td valign=top align=left CLASS='filterValue'>");
		if(service_status_types==all_service_status_types)
			printf("All");
		else if(service_status_types==all_service_problems)
			printf("All Problems");
		else{
			found=0;
			if(service_status_types & SERVICE_PENDING){
				printf(" Pending");
				found=1;
		                }
			if(service_status_types & SERVICE_OK){
				printf("%s Ok",(found==1)?" |":"");
				found=1;
		                }
			if(service_status_types & SERVICE_UNKNOWN){
				printf("%s Unknown",(found==1)?" |":"");
				found=1;
		                }
			if(service_status_types & SERVICE_WARNING){
				printf("%s Warning",(found==1)?" |":"");
				found=1;
		                }
			if(service_status_types & SERVICE_CRITICAL){
				printf("%s Critical",(found==1)?" |":"");
				found=1;
		                }
	                }
		printf("</td></tr>");
		printf("<tr><td valign=top align=left CLASS='filterName'>Service Properties:</td>");
		printf("<td valign=top align=left CLASS='filterValue'>");
		if(service_properties==0)
			printf("Any");
		else{
			found=0;
			if(service_properties & SERVICE_SCHEDULED_DOWNTIME){
				printf(" In Scheduled Downtime");
				found=1;
		                }
			if(service_properties & SERVICE_NO_SCHEDULED_DOWNTIME){
				printf("%s Not In Scheduled Downtime",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_STATE_ACKNOWLEDGED){
				printf("%s Has Been Acknowledged",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_STATE_UNACKNOWLEDGED){
				printf("%s Has Not Been Acknowledged",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_CHECKS_DISABLED){
				printf("%s Active Checks Disabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_CHECKS_ENABLED){
				printf("%s Active Checks Enabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_EVENT_HANDLER_DISABLED){
				printf("%s Event Handler Disabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_EVENT_HANDLER_ENABLED){
				printf("%s Event Handler Enabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_FLAP_DETECTION_DISABLED){
				printf("%s Flap Detection Disabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_FLAP_DETECTION_ENABLED){
				printf("%s Flap Detection Enabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_IS_FLAPPING){
				printf("%s Is Flapping",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_IS_NOT_FLAPPING){
				printf("%s Is Not Flapping",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_NOTIFICATIONS_DISABLED){
				printf("%s Notifications Disabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_NOTIFICATIONS_ENABLED){
				printf("%s Notifications Enabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_PASSIVE_CHECKS_DISABLED){
				printf("%s Passive Checks Disabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_PASSIVE_CHECKS_ENABLED){
				printf("%s Passive Checks Enabled",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_PASSIVE_CHECK){
				printf("%s Passive Checks",(found==1)?" &amp;":"");
				found=1;
		                }
			if(service_properties & SERVICE_ACTIVE_CHECK){
				printf("%s Active Checks",(found==1)?" &amp;":"");
				found=1;
		                }
	                }
		printf("</td></tr>");
		printf("</table>\n");

		printf("</td></tr>");
		printf("</table>\n");
	        }

	return;
        }
