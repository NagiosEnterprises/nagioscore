/**************************************************************************
 *
 * MINISTATUS.C -  Nagios Mini Status CGI
 *
 * Submitted By: Paul Kent
 * Based On: STATUS.C, Copyright (c) 1999-2001 Ethan Galstad
 * Last Modified: 01-27-2002
 *
 * Comments:
 *
 * Very very hacked down status.cgi, to display overall information. 
 * Does _not_ use the authentication system.  Takes one CGI parameter
 * 'types' with values 1-15. Value turned into an integer and used as
 * a bitmask:
 *    8 = Host bottom line table
 *    4 = Hosts summary/totals table
 *    2 = Services bottom line
 *    1 = Service summary/totals
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
#include "../common/statusdata.h"

#include "cgiutils.h"
#include "getcgi.h"

extern char main_config_file[MAX_FILENAME_LENGTH];
extern hoststatus *hoststatus_list;
extern servicestatus *servicestatus_list;

#define DISPLAY_HOSTS			0
#define DISPLAY_HOSTGROUPS		1

#define STYLE_OVERVIEW			0
#define STYLE_DETAIL			1
#define STYLE_SUMMARY			2

void show_host_status_totals(void);
void show_service_status_totals(void);
int process_cgivars(void);

char *hostgroup_name="all";

int types = 15;

int host_status_types=HOST_PENDING|HOST_UP|HOST_DOWN|HOST_UNREACHABLE;

int problem_hosts_down=0;
int problem_hosts_unreachable=0;

int problem_services_critical=0;
int problem_services_warning=0;
int problem_services_unknown=0;


int main(void){
	int result=OK;
	
	/* get the arguments passed in the URL */
	process_cgivars();

	/* reset internal variables */
	reset_cgi_vars();

	/* header */
	printf("Content-type: text/html\n\n");
	printf("<!-- ministatus.cgi Current Service Status -->\n");
	
	/* read the CGI configuration file */
	result=read_cgi_config_file(DEFAULT_CGI_CONFIG_FILE);
	if(result==ERROR){
		cgi_config_file_error(DEFAULT_CGI_CONFIG_FILE);
		return ERROR;
	        }

	/* read the main configuration file */
	result=read_main_config_file(main_config_file);
	if(result==ERROR){
		main_config_file_error(main_config_file);
		return ERROR;
	        }

	/* read all object configuration data */
	result=read_all_object_configuration_data(main_config_file,READ_HOSTGROUPS|READ_CONTACTGROUPS|READ_HOSTS|READ_SERVICES);
	if(result==ERROR){
		object_data_error();
		return ERROR;
	        }

	/* read all status data */
	result=read_all_status_data(DEFAULT_CGI_CONFIG_FILE,READ_PROGRAM_STATUS|READ_HOST_STATUS|READ_SERVICE_STATUS);
	if(result==ERROR){
		status_data_error();
		free_memory();
		return ERROR;
	        }

	show_host_status_totals();
	show_service_status_totals();

	printf("<!-- /ministatus.cgi -->\n");

	/* free all allocated memory */
	free_memory();

	return OK;
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

		/* we found the type argument */
		if(!strcmp(variables[x],"types")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }
			types=atoi(variables[x]);
		        }
	        }
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

	/* check the status of all services... */
	for(temp_servicestatus=servicestatus_list; temp_servicestatus!=NULL; temp_servicestatus=temp_servicestatus->next){

		/* find the host and service... */
		temp_host=find_host(temp_servicestatus->host_name,NULL);
		temp_service=find_service(temp_servicestatus->host_name,temp_servicestatus->description,NULL);

		if(temp_servicestatus->status==SERVICE_CRITICAL || temp_servicestatus->status==SERVICE_HOST_DOWN || temp_servicestatus->status==SERVICE_UNREACHABLE){
			total_critical++;
			if(temp_servicestatus->problem_has_been_acknowledged==FALSE && temp_servicestatus->checks_enabled==TRUE && temp_servicestatus->notifications_enabled==TRUE)
				problem_services_critical++;
		        }
		else if(temp_servicestatus->status==SERVICE_WARNING){
			total_warning++;
			if(temp_servicestatus->problem_has_been_acknowledged==FALSE && temp_servicestatus->checks_enabled==TRUE && temp_servicestatus->notifications_enabled==TRUE)
				problem_services_warning++;
		        }
		else if(temp_servicestatus->status==SERVICE_UNKNOWN){
			total_unknown++;
			if(temp_servicestatus->problem_has_been_acknowledged==FALSE && temp_servicestatus->checks_enabled==TRUE && temp_servicestatus->notifications_enabled==TRUE)
				problem_services_unknown++;
		        }
		else if(temp_servicestatus->status==SERVICE_RECOVERY)
			total_ok++;
		else if(temp_servicestatus->status==SERVICE_OK)
			total_ok++;
		else if(temp_servicestatus->status==SERVICE_PENDING)
			total_pending++;
		else
			total_ok++;
	        }

	total_services=total_ok+total_unknown+total_warning+total_critical+total_pending;
	total_problems=total_unknown+total_warning+total_critical;

	/* service totals table */
	if (types & 1) {
		printf("<!-- ministatus.cgi - services --><div CLASS='serviceTotalsDiv'><strong>Services:</strong>\n");

		/* total services ok */
		printf("<A CLASS='serviceTotals' HREF='%s?hostgroup=%s&style=detail",STATUS_CGI,hostgroup_name);
		printf("&servicestatustypes=%d",SERVICE_OK|SERVICE_RECOVERY);
		printf("&hoststatustypes=%d'>Ok</A>: ",host_status_types);
		printf("<span CLASS='serviceTotals%s'> %d </span>\n",(total_ok>0)?"OK":"",total_ok);

		/* total services in warning state */
		printf("<A CLASS='serviceTotals' HREF='%s?hostgroup=%s&style=detail",STATUS_CGI,hostgroup_name);
		printf("&servicestatustypes=%d",SERVICE_WARNING);
		printf("&hoststatustypes=%d'>Warning</A>: ",host_status_types);
		printf("<span CLASS='serviceTotals%s'> %d </span>\n",(total_warning>0)?"WARNING":"",total_warning);

		/* total services in unknown state */
		printf("<A CLASS='serviceTotals' HREF='%s?hostgroup=%s&style=detail",STATUS_CGI,hostgroup_name);
		printf("&servicestatustypes=%d",SERVICE_UNKNOWN);
		printf("&hoststatustypes=%d'>Unknown</A>: ",host_status_types);
		printf("<span CLASS='serviceTotals%s'> %d </span>\n",(total_unknown>0)?"UNKNOWN":"",total_unknown);

		/* total services in critical state */
		printf("<A CLASS='serviceTotals' HREF='%s?hostgroup=%s&style=detail",STATUS_CGI,hostgroup_name);
		printf("&servicestatustypes=%d",SERVICE_CRITICAL|SERVICE_HOST_DOWN|SERVICE_UNREACHABLE);
		printf("&hoststatustypes=%d'>Critical</A>: ",host_status_types);
		printf("<span CLASS='serviceTotals%s'> %d </span>\n",(total_critical>0)?"CRITICAL":"",total_critical);

		/* total services in pending state */
		printf("<A CLASS='serviceTotals' HREF='%s?hostgroup=%s&style=detail",STATUS_CGI,hostgroup_name);
		printf("&servicestatustypes=%d",SERVICE_PENDING);
		printf("&hoststatustypes=%d'>Pending</A>: ",host_status_types);
		printf("<span CLASS='serviceTotals%s'> %d </span>\n",(total_pending>0)?"PENDING":"",total_pending);

		printf("</div><!-- /ministatus.cgi - services -->\n\n");
	        }
	/* /service totals table */

	/* service totals summary table */
	if (types & 2) {
		printf("<!-- ministatus.cgi - service summary --><div CLASS='serviceTotalsDiv'><strong>Services Summary:</strong>\n");
		
		/* total services */
		printf("<A CLASS='serviceTotals' HREF='%s?hostgroup=%s&style=detail",STATUS_CGI,hostgroup_name);
		printf("&hoststatustypes=%d'>Total Services</A>: ",host_status_types);
		printf("<span CLASS='serviceTotals'> %d </span>\n",total_services);

		/* total service problems */
		printf("<A CLASS='serviceTotals' HREF='%s?hostgroup=%s&style=detail",STATUS_CGI,hostgroup_name);
		printf("&servicestatustypes=%d",SERVICE_UNKNOWN|SERVICE_WARNING|SERVICE_CRITICAL|SERVICE_HOST_DOWN|SERVICE_UNREACHABLE);
		printf("&hoststatustypes=%d'>Total Problems</A>: ",host_status_types);
		printf("<span CLASS='serviceTotals%s'> %d </span>\n",(total_problems>0)?"PROBLEMS":"",total_problems);

		printf("</div><!-- /ministatus.cgi - service summary -->\n\n");
	        }
	/* /service totals summary table */

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

	/* check the status of all hosts... */
	for(temp_hoststatus=hoststatus_list;temp_hoststatus!=NULL;temp_hoststatus=temp_hoststatus->next){

		/* find the host... */
		temp_host=find_host(temp_hoststatus->host_name,NULL);

		if(temp_hoststatus->status==HOST_UP)
			total_up++;
		else if(temp_hoststatus->status==HOST_DOWN){
			total_down++;
			if(temp_hoststatus->problem_has_been_acknowledged==FALSE && temp_hoststatus->notifications_enabled==TRUE && temp_hoststatus->checks_enabled==TRUE)
				problem_hosts_down++;
		        }
		else if(temp_hoststatus->status==HOST_UNREACHABLE){
			total_unreachable++;
			if(temp_hoststatus->problem_has_been_acknowledged==FALSE && temp_hoststatus->notifications_enabled==TRUE && temp_hoststatus->checks_enabled==TRUE)
				problem_hosts_unreachable++;
		        }

		else if(temp_hoststatus->status==HOST_PENDING)
			total_pending++;
		else
			total_up++;
	        }

	total_hosts=total_up+total_down+total_unreachable+total_pending;
	total_problems=total_down+total_unreachable;

	/* host table */
	if (types & 4) {
		printf("<!-- ministatus.cgi - hosts --><div CLASS='hostTotalsDiv'><strong>Hosts:</strong>\n");

		/* total hosts up */
		printf("<A CLASS='hostTotals' HREF='%s?hostgroup=%s",STATUS_CGI,hostgroup_name);
		printf("&hoststatustypes=%d'>Up</A>: ",HOST_UP);
		printf("<span CLASS='hostTotals%s'> %d </span>\n",(total_up>0)?"UP":"",total_up);

		/* total hosts down */
		printf("<A CLASS='hostTotals' HREF='%s?hostgroup=%s",STATUS_CGI,hostgroup_name);
		printf("&hoststatustypes=%d'>Down</A>: ",HOST_DOWN);
		printf("<span CLASS='hostTotals%s'> %d </span>\n",(total_down>0)?"DOWN":"",total_down);

		/* total hosts unreachable */
		printf("<A CLASS='hostTotals' HREF='%s?hostgroup=%s",STATUS_CGI,hostgroup_name);
		printf("&hoststatustypes=%d'>Unreachable</A>: ",HOST_UNREACHABLE);
		printf("<span CLASS='hostTotals%s'> %d </span>\n",(total_unreachable>0)?"UNREACHABLE":"",total_unreachable);

		/* total hosts pending */
		printf("<A CLASS='hostTotals' HREF='%s?hostgroup=%s",STATUS_CGI,hostgroup_name);
		printf("&hoststatustypes=%d'>Pending</A>: ",HOST_PENDING);
		printf("<span CLASS='hostTotals%s'> %d </span>\n",(total_pending>0)?"PENDING":"",total_pending);

		printf("</div><!-- /ministatus.cgi - hosts -->\n\n");
	        }
	/* /host table */

	/* host summary table */
	if (types & 8) {
		printf("<!-- ministatus.cgi - hosts summary --><div CLASS='hostTotalsDiv'><strong>Hosts Summary:</strong>\n");
		
		/* total hosts */
		printf("<A CLASS='hostTotals' HREF='%s?hostgroup=%s'>Total Hosts</A>: ",STATUS_CGI,hostgroup_name);
		printf("<span CLASS='hostTotals'> %d </span>\n",total_hosts);

		/* total hosts with problems */
		printf("<A CLASS='hostTotals' HREF='%s?hostgroup=%s",STATUS_CGI,hostgroup_name);
		printf("&hoststatustypes=%d'>Total Problems</A>: ",HOST_DOWN|HOST_UNREACHABLE);
		printf("<span CLASS='hostTotals%s'> %d </span>\n",(total_problems>0)?"PROBLEMS":"",total_problems);

		printf("</div><!-- /ministatus.cgi - hosts summary -->\n\n");
	        }
	/* /host summary table */

	return;
        }
