/*****************************************************************************
 *
 * STATUSDATA.C - External status data for Nagios CGIs
 *
 * Copyright (c) 2000-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   02-15-2003
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
 *
 *****************************************************************************/

/*********** COMMON HEADER FILES ***********/

#include "config.h"
#include "common.h"
#include "objects.h"
#include "statusdata.h"

#ifdef NSCORE
#include "../base/nagios.h"
#endif
#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif

/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/

#ifdef USE_XSDDEFAULT
#include "../xdata/xsddefault.h"		/* default routines */
#endif
#ifdef USE_XSDDB
#include "../xdata/xsddb.h"                     /* database routines */
#endif



#ifdef NSCGI
hoststatus      *hoststatus_list=NULL;
servicestatus   *servicestatus_list=NULL;

#endif



#ifdef NSCORE

/******************************************************************/
/****************** TOP-LEVEL OUTPUT FUNCTIONS ********************/
/******************************************************************/

/* initializes status data at program start */
int initialize_status_data(char *config_file){
	int result=OK;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_initialize_status_data(config_file);
#endif
#ifdef USE_XSDDB
	result=xsddb_initialize_status_data(config_file);
#endif

	return result;
        }


/* update all status data (aggregated dump) */
int update_all_status_data(void){
	int result;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_save_status_data();
#endif
#ifdef USE_XSDDB
	result=xsddb_sate_status_data();
#endif

	if(result!=OK)
		return ERROR;

	return OK;
        }


/* cleans up status data before program termination */
int cleanup_status_data(char *config_file,int delete_status_data){
	int result=OK;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_cleanup_status_data(config_file,delete_status_data);
#endif
#ifdef USE_XSDDB
	result=xsddb_cleanup_status_data(config_file,delete_status_data);
#endif

	return result;
        }



/* updates program status info */
int update_program_status(int aggregated_dump){

	/* currently a noop */

	return OK;
        }



/* updates host status info */
int update_host_status(host *hst,int aggregated_dump){

	/* currently a noop */

	return OK;
        }



/* updates service status info */
int update_service_status(service *svc,int aggregated_dump){

	/* currently a noop */

	return OK;
        }

#endif





#ifdef NSCGI

/******************************************************************/
/******************* TOP-LEVEL INPUT FUNCTIONS ********************/
/******************************************************************/


/* reads in all status data */
int read_status_data(char *config_file,int options){
	int result=OK;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XSDDEFAULT
	result=xsddefault_read_status_data(config_file,options);
#endif
#ifdef USE_XSDDB
	result=xsddb_read_status_data(config_file,options);
#endif

	return result;
        }



/******************************************************************/
/********************** ADDITION FUNCTIONS ************************/
/******************************************************************/


/* adds a host status entry to the list in memory */
int add_host_status(hoststatus *new_hoststatus){
	hoststatus *last_hoststatus=NULL;
	hoststatus *temp_hoststatus=NULL;

	/* make sure we have what we need */
	if(new_hoststatus==NULL)
		return ERROR;
	if(new_hoststatus->host_name==NULL)
		return ERROR;

	/* massage host status a bit */
	if(new_hoststatus!=NULL){
		switch(new_hoststatus->status){
		case 0:
			new_hoststatus->status=HOST_UP;
			break;
		case 1:
			new_hoststatus->status=HOST_DOWN;
			break;
		case 2:
			new_hoststatus->status=HOST_UNREACHABLE;
			break;
		default:
			new_hoststatus->status=HOST_UP;
			break;
		        }
		if(new_hoststatus->last_check==0L){
			new_hoststatus->status=HOST_PENDING;
			free(new_hoststatus->plugin_output);
			new_hoststatus->plugin_output=strdup("Host has not been checked yet");
		        }
	        }

	/* add new host status to list, sorted by host name */
	last_hoststatus=hoststatus_list;
	for(temp_hoststatus=hoststatus_list;temp_hoststatus!=NULL;temp_hoststatus=temp_hoststatus->next){
		if(strcmp(new_hoststatus->host_name,temp_hoststatus->host_name)<0){
			new_hoststatus->next=temp_hoststatus;
			if(temp_hoststatus==hoststatus_list)
				hoststatus_list=new_hoststatus;
			else
				last_hoststatus->next=new_hoststatus;
			break;
		        }
		else
			last_hoststatus=temp_hoststatus;
	        }
	if(hoststatus_list==NULL){
		new_hoststatus->next=NULL;
		hoststatus_list=new_hoststatus;
	        }
	else if(temp_hoststatus==NULL){
		new_hoststatus->next=NULL;
		last_hoststatus->next=new_hoststatus;
	        }


	return OK;
        }


/* adds a service status entry to the list in memory */
int add_service_status(servicestatus *new_svcstatus){
	char temp_buffer[MAX_INPUT_BUFFER];
	char date_string[MAX_DATETIME_LENGTH];
	servicestatus *last_svcstatus=NULL;
	servicestatus *temp_svcstatus=NULL;

	/* make sure we have what we need */
	if(new_svcstatus==NULL)
		return ERROR;
	if(new_svcstatus->host_name==NULL || new_svcstatus->description==NULL)
		return ERROR;


	/* massage service status a bit */
	if(new_svcstatus!=NULL){
		switch(new_svcstatus->status){
		case 0:
			new_svcstatus->status=SERVICE_OK;
			break;
		case 1:
			new_svcstatus->status=SERVICE_WARNING;
			break;
		case 2:
			new_svcstatus->status=SERVICE_CRITICAL;
			break;
		case 3:
			new_svcstatus->status=SERVICE_UNKNOWN;
			break;
		default:
			new_svcstatus->status=SERVICE_OK;
			break;
		        }
		if(new_svcstatus->last_check==0L){
			new_svcstatus->status=SERVICE_PENDING;
			free(new_svcstatus->plugin_output);
			if(new_svcstatus->should_be_scheduled==TRUE){
				get_time_string(&new_svcstatus->next_check,date_string,sizeof(date_string),LONG_DATE_TIME);
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Service check scheduled for %s",date_string);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				new_svcstatus->plugin_output=strdup(temp_buffer);
			        }
			else
				new_svcstatus->plugin_output=strdup("Service is not scheduled to be checked...");
		        }
	        }

	/* add new service status to list, sorted by host name then description */
	last_svcstatus=servicestatus_list;
	for(temp_svcstatus=servicestatus_list;temp_svcstatus!=NULL;temp_svcstatus=temp_svcstatus->next){

		if(strcmp(new_svcstatus->host_name,temp_svcstatus->host_name)<0){
			new_svcstatus->next=temp_svcstatus;
			if(temp_svcstatus==servicestatus_list)
				servicestatus_list=new_svcstatus;
			else
				last_svcstatus->next=new_svcstatus;
			break;
		        }

		else if(strcmp(new_svcstatus->host_name,temp_svcstatus->host_name)==0 && strcmp(new_svcstatus->description,temp_svcstatus->description)<0){
			new_svcstatus->next=temp_svcstatus;
			if(temp_svcstatus==servicestatus_list)
				servicestatus_list=new_svcstatus;
			else
				last_svcstatus->next=new_svcstatus;
			break;
		        }

		else
			last_svcstatus=temp_svcstatus;
	        }
	if(servicestatus_list==NULL){
		new_svcstatus->next=NULL;
		servicestatus_list=new_svcstatus;
	        }
	else if(temp_svcstatus==NULL){
		new_svcstatus->next=NULL;
		last_svcstatus->next=new_svcstatus;
	        }


	return OK;
        }





/******************************************************************/
/*********************** CLEANUP FUNCTIONS ************************/
/******************************************************************/


/* free all memory for status data */
void free_status_data(void){
	hoststatus *this_hoststatus;
	hoststatus *next_hoststatus;
	servicestatus *this_svcstatus;
	servicestatus *next_svcstatus;

	/* free memory for the host status list */
	for(this_hoststatus=hoststatus_list;this_hoststatus!=NULL;this_hoststatus=next_hoststatus){
		next_hoststatus=this_hoststatus->next;
		free(this_hoststatus->host_name);
		free(this_hoststatus->plugin_output);
		free(this_hoststatus->perf_data);
		free(this_hoststatus);
	        }

	/* free memory for the service status list */
	for(this_svcstatus=servicestatus_list;this_svcstatus!=NULL;this_svcstatus=next_svcstatus){
		next_svcstatus=this_svcstatus->next;
		free(this_svcstatus->host_name);
		free(this_svcstatus->description);
		free(this_svcstatus->plugin_output);
		free(this_svcstatus->perf_data);
		free(this_svcstatus);
	        }

	/* reset list pointers */
	hoststatus_list=NULL;
	servicestatus_list=NULL;

	return;
        }




/******************************************************************/
/************************ SEARCH FUNCTIONS ************************/
/******************************************************************/


/* find a host status entry */
hoststatus *find_hoststatus(char *host_name){
	hoststatus *temp_hoststatus;

	for(temp_hoststatus=hoststatus_list;temp_hoststatus!=NULL;temp_hoststatus=temp_hoststatus->next){
		if(!strcmp(temp_hoststatus->host_name,host_name))
			return temp_hoststatus;
	        }

	return NULL;
        }


/* find a service status entry */
servicestatus *find_servicestatus(char *host_name,char *svc_desc){
	servicestatus *temp_servicestatus;

	for(temp_servicestatus=servicestatus_list;temp_servicestatus!=NULL;temp_servicestatus=temp_servicestatus->next){
		if(!strcmp(temp_servicestatus->host_name,host_name) && !strcmp(temp_servicestatus->description,svc_desc))
			return temp_servicestatus;
	        }

	return NULL;
        }




/******************************************************************/
/*********************** UTILITY FUNCTIONS ************************/
/******************************************************************/


/* gets the total number of services of a certain state for a specific host */
int get_servicestatus_count(char *host_name, int type){
	servicestatus *temp_status;
	int count=0;

	if(host_name==NULL)
		return 0;

	for(temp_status=servicestatus_list;temp_status!=NULL;temp_status=temp_status->next){
		if(temp_status->status & type){
			if(!strcmp(host_name,temp_status->host_name))
				count++;
		        }
	        }

	return count;
        }



#endif

