/*****************************************************************************
 *
 * STATUSDATA.C - External status data for Nagios CGIs
 *
 * Copyright (c) 2000-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   06-15-2003
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


#ifdef NSCORE
extern int      aggregate_status_updates;
#endif

#ifdef NSCGI
hoststatus      *hoststatus_list=NULL;
servicestatus   *servicestatus_list=NULL;

hoststatus      **hoststatus_hashlist=NULL;
servicestatus   **servicestatus_hashlist=NULL;
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
	result=xsddb_save_status_data();
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

	/* currently a noop if aggregated updates is TRUE */

	/* update all status data if we're not aggregating updates*/
	if(aggregate_status_updates==FALSE)
		update_all_status_data();

	return OK;
        }



/* updates host status info */
int update_host_status(host *hst,int aggregated_dump){

	/* currently a noop if aggregated updates is TRUE */

	/* update all status data if we're not aggregating updates*/
	if(aggregate_status_updates==FALSE)
		update_all_status_data();

	return OK;
        }



/* updates service status info */
int update_service_status(service *svc,int aggregated_dump){

	/* currently a noop if aggregated updates is TRUE */

	/* update all status data if we're not aggregating updates*/
	if(aggregate_status_updates==FALSE)
		update_all_status_data();

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
/****************** CHAINED HASH FUNCTIONS ************************/
/******************************************************************/

/* adds hoststatus to hash list in memory */
int add_hoststatus_to_hashlist(hoststatus *new_hoststatus){
	hoststatus *temp_hoststatus, *lastpointer;
	int hashslot;

	/* initialize hash list */
	if(hoststatus_hashlist==NULL){
		int i;

		hoststatus_hashlist=(hoststatus **)malloc(sizeof(hoststatus *)*HOSTSTATUS_HASHSLOTS);
		if(hoststatus_hashlist==NULL)
			return 0;
		
		for(i=0;i<HOSTSTATUS_HASHSLOTS;i++)
			hoststatus_hashlist[i]=NULL;
	        }

	if(!new_hoststatus)
		return 0;

	hashslot=hashfunc1(new_hoststatus->host_name,HOSTSTATUS_HASHSLOTS);
	lastpointer=NULL;
	for(temp_hoststatus=hoststatus_hashlist[hashslot];temp_hoststatus && compare_hashdata1(temp_hoststatus->host_name,new_hoststatus->host_name)<0;temp_hoststatus=temp_hoststatus->nexthash)
		lastpointer=temp_hoststatus;

	if(!temp_hoststatus || (compare_hashdata1(temp_hoststatus->host_name,new_hoststatus->host_name)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_hoststatus;
		else
			hoststatus_hashlist[hashslot]=new_hoststatus;
		new_hoststatus->nexthash=temp_hoststatus;

		return 1;
	        }

	/* else already exists */
	return 0;
        }


int add_servicestatus_to_hashlist(servicestatus *new_servicestatus){
	servicestatus *temp_servicestatus, *lastpointer;
	int hashslot;

	/* initialize hash list */
	if(servicestatus_hashlist==NULL){
		int i;

		servicestatus_hashlist=(servicestatus **)malloc(sizeof(servicestatus *)*SERVICESTATUS_HASHSLOTS);
		if(servicestatus_hashlist==NULL)
			return 0;
		
		for(i=0;i< SERVICESTATUS_HASHSLOTS;i++)
			servicestatus_hashlist[i]=NULL;
	        }

	if(!new_servicestatus)
		return 0;

	hashslot=hashfunc2(new_servicestatus->host_name,new_servicestatus->description,SERVICESTATUS_HASHSLOTS);
	lastpointer=NULL;
	for(temp_servicestatus=servicestatus_hashlist[hashslot];temp_servicestatus && compare_hashdata2(temp_servicestatus->host_name,temp_servicestatus->description,new_servicestatus->host_name,new_servicestatus->description)<0;temp_servicestatus=temp_servicestatus->nexthash)
		lastpointer=temp_servicestatus;

	if(!temp_servicestatus || (compare_hashdata2(temp_servicestatus->host_name,temp_servicestatus->description,new_servicestatus->host_name,new_servicestatus->description)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_servicestatus;
		else
			servicestatus_hashlist[hashslot]=new_servicestatus;
		new_servicestatus->nexthash=temp_servicestatus;


		return 1;
	        }

	/* else already exists */
	return 0;
        }



/******************************************************************/
/********************** ADDITION FUNCTIONS ************************/
/******************************************************************/


/* adds a host status entry to the list in memory */
int add_host_status(hoststatus *new_hoststatus){
	char temp_buffer[MAX_INPUT_BUFFER];
	char date_string[MAX_DATETIME_LENGTH];
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
			if(new_hoststatus->should_be_scheduled==TRUE){
				get_time_string(&new_hoststatus->next_check,date_string,sizeof(date_string),LONG_DATE_TIME);
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Host check scheduled for %s",date_string);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				new_hoststatus->plugin_output=strdup(temp_buffer);
			        }
			else
				new_hoststatus->plugin_output=strdup("Host has not been checked yet");
		        }
	        }

	new_hoststatus->next=NULL;
	new_hoststatus->nexthash=NULL;

	/* add new hoststatus to hoststatus chained hash list */
	if(!add_hoststatus_to_hashlist(new_hoststatus))
		return ERROR;

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

	new_svcstatus->next=NULL;
	new_svcstatus->nexthash=NULL;

	/* add new servicestatus to servicestatus chained hash list */
	if(!add_servicestatus_to_hashlist(new_svcstatus))
		return ERROR;

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

	if(host_name==NULL || hoststatus_hashlist==NULL)
		return NULL;

	for(temp_hoststatus=hoststatus_hashlist[hashfunc1(host_name,HOSTSTATUS_HASHSLOTS)];temp_hoststatus && compare_hashdata1(temp_hoststatus->host_name,host_name)<0;temp_hoststatus=temp_hoststatus->nexthash);

	if(temp_hoststatus && (compare_hashdata1(temp_hoststatus->host_name,host_name)==0))
		return temp_hoststatus;

	return NULL;
        }


/* find a service status entry */
servicestatus *find_servicestatus(char *host_name,char *svc_desc){
	servicestatus *temp_servicestatus;

	if(host_name==NULL || svc_desc==NULL || servicestatus_hashlist==NULL)
		return NULL;

	for(temp_servicestatus=servicestatus_hashlist[hashfunc2(host_name,svc_desc,SERVICESTATUS_HASHSLOTS)];temp_servicestatus && compare_hashdata2(temp_servicestatus->host_name,temp_servicestatus->description,host_name,svc_desc)<0;temp_servicestatus=temp_servicestatus->nexthash);

	if(temp_servicestatus && (compare_hashdata2(temp_servicestatus->host_name,temp_servicestatus->description,host_name,svc_desc)==0))
		return temp_servicestatus;

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

