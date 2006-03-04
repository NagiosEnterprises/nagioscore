/*****************************************************************************
 *
 * OBJECTS.C - Object addition and search functions for Nagios
 *
 * Copyright (c) 1999-2006 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 03-03-2006
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
 *
 *****************************************************************************/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"

#ifdef NSCORE
#include "../include/nagios.h"
#endif

#ifdef NSCGI
#include "../include/cgiutils.h"
#endif

/**** IMPLEMENTATION-SPECIFIC HEADER FILES ****/

#ifdef USE_XODTEMPLATE                          /* template-based routines */
#include "../xdata/xodtemplate.h"
#endif
  

host            *host_list=NULL,*host_list_tail=NULL;
service         *service_list=NULL,*service_list_tail=NULL;
contact		*contact_list=NULL,*contact_list_tail=NULL;
contactgroup	*contactgroup_list=NULL,*contactgroup_list_tail=NULL;
hostgroup	*hostgroup_list=NULL,*hostgroup_list_tail=NULL;
servicegroup    *servicegroup_list=NULL,*servicegroup_list_tail=NULL;
command         *command_list=NULL,*command_list_tail=NULL;
timeperiod      *timeperiod_list=NULL,*timeperiod_list_tail=NULL;
serviceescalation *serviceescalation_list=NULL,*serviceescalation_list_tail=NULL;
servicedependency *servicedependency_list=NULL,*servicedependency_list_tail=NULL;
hostdependency  *hostdependency_list=NULL,*hostdependency_list_tail=NULL;
hostescalation  *hostescalation_list=NULL,*hostescalation_list_tail=NULL;
hostextinfo     *hostextinfo_list=NULL,*hostextinfo_list_tail=NULL;

host		**host_hashlist=NULL;
service		**service_hashlist=NULL;
command         **command_hashlist=NULL;
timeperiod      **timeperiod_hashlist=NULL;
contact         **contact_hashlist=NULL;
contactgroup    **contactgroup_hashlist=NULL;
hostgroup       **hostgroup_hashlist=NULL;
servicegroup    **servicegroup_hashlist=NULL;
hostextinfo     **hostextinfo_hashlist=NULL;
hostdependency  **hostdependency_hashlist=NULL;
servicedependency **servicedependency_hashlist=NULL;
hostescalation  **hostescalation_hashlist=NULL;
serviceescalation **serviceescalation_hashlist=NULL;


#ifdef NSCORE
int __nagios_object_structure_version=CURRENT_OBJECT_STRUCTURE_VERSION;
#endif



/******************************************************************/
/******* TOP-LEVEL HOST CONFIGURATION DATA INPUT FUNCTION *********/
/******************************************************************/


/* read all host configuration data from external source */
int read_object_config_data(char *main_config_file, int options, int cache, int precache){
	int result=OK;

#ifdef DEBUG0
	printf("read_object_config_data() start\n");
#endif

	/********* IMPLEMENTATION-SPECIFIC INPUT FUNCTION ********/
#ifdef USE_XODTEMPLATE
	/* read in data from all text host config files (template-based) */
	result=xodtemplate_read_config_data(main_config_file,options,cache,precache);
	if(result!=OK)
		return ERROR;
#endif

#ifdef DEBUG0
	printf("read_object_config_data() end\n");
#endif

	return result;
        }



/******************************************************************/
/****************** CHAINED HASH FUNCTIONS ************************/
/******************************************************************/

/* adds host to hash list in memory */
int add_host_to_hashlist(host *new_host){
	host *temp_host=NULL;
	host *lastpointer=NULL;
	int hashslot=0;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

	/* initialize hash list */
	if(host_hashlist==NULL){
		int i;

		host_hashlist=(host **)malloc(sizeof(host *)*HOST_HASHSLOTS);
		if(host_hashlist==NULL)
			return 0;
		
		for(i=0;i<HOST_HASHSLOTS;i++)
			host_hashlist[i]=NULL;
	        }

	if(!new_host)
		return 0;

	hashslot=hashfunc(new_host->name,NULL,HOST_HASHSLOTS);
	lastpointer=NULL;
	for(temp_host=host_hashlist[hashslot];temp_host && compare_hashdata(temp_host->name,NULL,new_host->name,NULL)<0;temp_host=temp_host->nexthash)
		lastpointer=temp_host;

	if(!temp_host || (compare_hashdata(temp_host->name,NULL,new_host->name,NULL)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_host;
		else
			host_hashlist[hashslot]=new_host;
		new_host->nexthash=temp_host;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	asprintf(&temp_buffer,"Error: Could not add duplicate host '%s'.\n",new_host->name);
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
	my_free((void **)&temp_buffer);
#endif
	return 0;
        }


int add_service_to_hashlist(service *new_service){
	service *temp_service=NULL;
	service *lastpointer=NULL;
	int hashslot=0;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

	/* initialize hash list */
	if(service_hashlist==NULL){
		int i;

		service_hashlist=(service **)malloc(sizeof(service *)*SERVICE_HASHSLOTS);
		if(service_hashlist==NULL)
			return 0;
		
		for(i=0;i< SERVICE_HASHSLOTS;i++)
			service_hashlist[i]=NULL;
	        }

	if(!new_service)
		return 0;

	hashslot=hashfunc(new_service->host_name,new_service->description,SERVICE_HASHSLOTS);
	lastpointer=NULL;
	for(temp_service=service_hashlist[hashslot];temp_service && compare_hashdata(temp_service->host_name,temp_service->description,new_service->host_name,new_service->description)<0;temp_service=temp_service->nexthash)
		lastpointer=temp_service;

	if(!temp_service || (compare_hashdata(temp_service->host_name,temp_service->description,new_service->host_name,new_service->description)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_service;
		else
			service_hashlist[hashslot]=new_service;
		new_service->nexthash=temp_service;


		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	asprintf(&temp_buffer,"Error: Could not add duplicate service '%s' on host '%s'.\n",new_service->description,new_service->host_name);
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
	my_free((void **)&temp_buffer);
#endif
	return 0;
        }


int add_command_to_hashlist(command *new_command){
	command *temp_command=NULL;
	command *lastpointer=NULL;
	int hashslot=0;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

	/* initialize hash list */
	if(command_hashlist==NULL){
		int i;

		command_hashlist=(command **)malloc(sizeof(command *)*COMMAND_HASHSLOTS);
		if(command_hashlist==NULL)
			return 0;
		
		for(i=0;i<COMMAND_HASHSLOTS;i++)
			command_hashlist[i]=NULL;
	        }

	if(!new_command)
		return 0;

	hashslot=hashfunc(new_command->name,NULL,COMMAND_HASHSLOTS);
	lastpointer=NULL;
	for(temp_command=command_hashlist[hashslot];temp_command && compare_hashdata(temp_command->name,NULL,new_command->name,NULL)<0;temp_command=temp_command->nexthash)
		lastpointer=temp_command;

	if(!temp_command || (compare_hashdata(temp_command->name,NULL,new_command->name,NULL)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_command;
		else
			command_hashlist[hashslot]=new_command;
		new_command->nexthash=temp_command;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	asprintf(&temp_buffer,"Error: Could not add duplicate command '%s'.\n",new_command->name);
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
	my_free((void **)&temp_buffer);
#endif
	return 0;
        }


int add_timeperiod_to_hashlist(timeperiod *new_timeperiod){
	timeperiod *temp_timeperiod=NULL;
	timeperiod *lastpointer=NULL;
	int hashslot=0;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

	/* initialize hash list */
	if(timeperiod_hashlist==NULL){
		int i;

		timeperiod_hashlist=(timeperiod **)malloc(sizeof(timeperiod *)*TIMEPERIOD_HASHSLOTS);
		if(timeperiod_hashlist==NULL)
			return 0;
		
		for(i=0;i<TIMEPERIOD_HASHSLOTS;i++)
			timeperiod_hashlist[i]=NULL;
	        }

	if(!new_timeperiod)
		return 0;

	hashslot=hashfunc(new_timeperiod->name,NULL,TIMEPERIOD_HASHSLOTS);
	lastpointer=NULL;
	for(temp_timeperiod=timeperiod_hashlist[hashslot];temp_timeperiod && compare_hashdata(temp_timeperiod->name,NULL,new_timeperiod->name,NULL)<0;temp_timeperiod=temp_timeperiod->nexthash)
		lastpointer=temp_timeperiod;

	if(!temp_timeperiod || (compare_hashdata(temp_timeperiod->name,NULL,new_timeperiod->name,NULL)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_timeperiod;
		else
			timeperiod_hashlist[hashslot]=new_timeperiod;
		new_timeperiod->nexthash=temp_timeperiod;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	asprintf(&temp_buffer,"Error: Could not add duplicate timeperiod '%s'.\n",new_timeperiod->name);
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
	my_free((void **)&temp_buffer);
#endif
	return 0;
        }


int add_contact_to_hashlist(contact *new_contact){
	contact *temp_contact=NULL;
	contact *lastpointer=NULL;
	int hashslot=0;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

	/* initialize hash list */
	if(contact_hashlist==NULL){
		int i;

		contact_hashlist=(contact **)malloc(sizeof(contact *)*CONTACT_HASHSLOTS);
		if(contact_hashlist==NULL)
			return 0;
		
		for(i=0;i<CONTACT_HASHSLOTS;i++)
			contact_hashlist[i]=NULL;
	        }

	if(!new_contact)
		return 0;

	hashslot=hashfunc(new_contact->name,NULL,CONTACT_HASHSLOTS);
	lastpointer=NULL;
	for(temp_contact=contact_hashlist[hashslot];temp_contact && compare_hashdata(temp_contact->name,NULL,new_contact->name,NULL)<0;temp_contact=temp_contact->nexthash)
		lastpointer=temp_contact;

	if(!temp_contact || (compare_hashdata(temp_contact->name,NULL,new_contact->name,NULL)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_contact;
		else
			contact_hashlist[hashslot]=new_contact;
		new_contact->nexthash=temp_contact;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	asprintf(&temp_buffer,"Error: Could not add duplicate contact '%s'.\n",new_contact->name);
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
	my_free((void **)&temp_buffer);
#endif
	return 0;
        }


int add_contactgroup_to_hashlist(contactgroup *new_contactgroup){
	contactgroup *temp_contactgroup=NULL;
	contactgroup *lastpointer=NULL;
	int hashslot=0;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

	/* initialize hash list */
	if(contactgroup_hashlist==NULL){
		int i;

		contactgroup_hashlist=(contactgroup **)malloc(sizeof(contactgroup *)*CONTACTGROUP_HASHSLOTS);
		if(contactgroup_hashlist==NULL)
			return 0;
		
		for(i=0;i<CONTACTGROUP_HASHSLOTS;i++)
			contactgroup_hashlist[i]=NULL;
	        }

	if(!new_contactgroup)
		return 0;

	hashslot=hashfunc(new_contactgroup->group_name,NULL,CONTACTGROUP_HASHSLOTS);
	lastpointer=NULL;
	for(temp_contactgroup=contactgroup_hashlist[hashslot];temp_contactgroup && compare_hashdata(temp_contactgroup->group_name,NULL,new_contactgroup->group_name,NULL)<0;temp_contactgroup=temp_contactgroup->nexthash)
		lastpointer=temp_contactgroup;

	if(!temp_contactgroup || (compare_hashdata(temp_contactgroup->group_name,NULL,new_contactgroup->group_name,NULL)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_contactgroup;
		else
			contactgroup_hashlist[hashslot]=new_contactgroup;
		new_contactgroup->nexthash=temp_contactgroup;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	asprintf(&temp_buffer,"Error: Could not add duplicate contactgroup '%s'.\n",new_contactgroup->group_name);
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
	my_free((void **)&temp_buffer);
#endif
	return 0;
        }


int add_hostgroup_to_hashlist(hostgroup *new_hostgroup){
	hostgroup *temp_hostgroup=NULL;
	hostgroup *lastpointer=NULL;
	int hashslot=0;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

	/* initialize hash list */
	if(hostgroup_hashlist==NULL){
		int i;

		hostgroup_hashlist=(hostgroup **)malloc(sizeof(hostgroup *)*HOSTGROUP_HASHSLOTS);
		if(hostgroup_hashlist==NULL)
			return 0;
		
		for(i=0;i<HOSTGROUP_HASHSLOTS;i++)
			hostgroup_hashlist[i]=NULL;
	        }

	if(!new_hostgroup)
		return 0;

	hashslot=hashfunc(new_hostgroup->group_name,NULL,HOSTGROUP_HASHSLOTS);
	lastpointer=NULL;
	for(temp_hostgroup=hostgroup_hashlist[hashslot];temp_hostgroup && compare_hashdata(temp_hostgroup->group_name,NULL,new_hostgroup->group_name,NULL)<0;temp_hostgroup=temp_hostgroup->nexthash)
		lastpointer=temp_hostgroup;

	if(!temp_hostgroup || (compare_hashdata(temp_hostgroup->group_name,NULL,new_hostgroup->group_name,NULL)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_hostgroup;
		else
			hostgroup_hashlist[hashslot]=new_hostgroup;
		new_hostgroup->nexthash=temp_hostgroup;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	asprintf(&temp_buffer,"Error: Could not add duplicate hostgroup '%s'.\n",new_hostgroup->group_name);
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
	my_free((void **)&temp_buffer);
#endif
	return 0;
        }


int add_servicegroup_to_hashlist(servicegroup *new_servicegroup){
	servicegroup *temp_servicegroup=NULL;
	servicegroup *lastpointer=NULL;
	int hashslot=0;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

	/* initialize hash list */
	if(servicegroup_hashlist==NULL){
		int i;

		servicegroup_hashlist=(servicegroup **)malloc(sizeof(servicegroup *)*SERVICEGROUP_HASHSLOTS);
		if(servicegroup_hashlist==NULL)
			return 0;
		
		for(i=0;i<SERVICEGROUP_HASHSLOTS;i++)
			servicegroup_hashlist[i]=NULL;
	        }

	if(!new_servicegroup)
		return 0;

	hashslot=hashfunc(new_servicegroup->group_name,NULL,SERVICEGROUP_HASHSLOTS);
	lastpointer=NULL;
	for(temp_servicegroup=servicegroup_hashlist[hashslot];temp_servicegroup && compare_hashdata(temp_servicegroup->group_name,NULL,new_servicegroup->group_name,NULL)<0;temp_servicegroup=temp_servicegroup->nexthash)
		lastpointer=temp_servicegroup;

	if(!temp_servicegroup || (compare_hashdata(temp_servicegroup->group_name,NULL,new_servicegroup->group_name,NULL)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_servicegroup;
		else
			servicegroup_hashlist[hashslot]=new_servicegroup;
		new_servicegroup->nexthash=temp_servicegroup;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	asprintf(&temp_buffer,"Error: Could not add duplicate servicegroup '%s'.\n",new_servicegroup->group_name);
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
	my_free((void **)&temp_buffer);
#endif
	return 0;
        }


/* adds hostextinfo to hash list in memory */
int add_hostextinfo_to_hashlist(hostextinfo *new_hostextinfo){
	hostextinfo *temp_hostextinfo=NULL;
	hostextinfo *lastpointer=NULL;
	int hashslot=0;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

	/* initialize hash list */
	if(hostextinfo_hashlist==NULL){
		int i;

		hostextinfo_hashlist=(hostextinfo **)malloc(sizeof(hostextinfo *)*HOSTEXTINFO_HASHSLOTS);
		if(hostextinfo_hashlist==NULL)
			return 0;
		
		for(i=0;i<HOSTEXTINFO_HASHSLOTS;i++)
			hostextinfo_hashlist[i]=NULL;
	        }

	if(!new_hostextinfo)
		return 0;

	hashslot=hashfunc(new_hostextinfo->host_name,NULL,HOSTEXTINFO_HASHSLOTS);
	lastpointer=NULL;
	for(temp_hostextinfo=hostextinfo_hashlist[hashslot];temp_hostextinfo && compare_hashdata(temp_hostextinfo->host_name,NULL,new_hostextinfo->host_name,NULL)<0;temp_hostextinfo=temp_hostextinfo->nexthash)
		lastpointer=temp_hostextinfo;

	if(!temp_hostextinfo || (compare_hashdata(temp_hostextinfo->host_name,NULL,new_hostextinfo->host_name,NULL)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_hostextinfo;
		else
			hostextinfo_hashlist[hashslot]=new_hostextinfo;
		new_hostextinfo->nexthash=temp_hostextinfo;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	asprintf(temp_buffer,"Error: Could not add duplicate hostextinfo entry for host '%s'.\n",new_hostextinfo->host_name);
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
	my_free((void **)&temp_buffer);
#endif
	return 0;
        }


/* adds hostdependency to hash list in memory */
int add_hostdependency_to_hashlist(hostdependency *new_hostdependency){
	hostdependency *temp_hostdependency=NULL;
	hostdependency *lastpointer=NULL;
	int hashslot=0;

	/* initialize hash list */
	if(hostdependency_hashlist==NULL){
		int i;

		hostdependency_hashlist=(hostdependency **)malloc(sizeof(hostdependency *)*HOSTDEPENDENCY_HASHSLOTS);
		if(hostdependency_hashlist==NULL)
			return 0;
		
		for(i=0;i<HOSTDEPENDENCY_HASHSLOTS;i++)
			hostdependency_hashlist[i]=NULL;
	        }

	if(!new_hostdependency)
		return 0;

	hashslot=hashfunc(new_hostdependency->dependent_host_name,NULL,HOSTDEPENDENCY_HASHSLOTS);
	lastpointer=NULL;
	for(temp_hostdependency=hostdependency_hashlist[hashslot];temp_hostdependency && compare_hashdata(temp_hostdependency->dependent_host_name,NULL,new_hostdependency->dependent_host_name,NULL)<0;temp_hostdependency=temp_hostdependency->nexthash)
		lastpointer=temp_hostdependency;

	/* duplicates are allowed */
	if(lastpointer)
		lastpointer->nexthash=new_hostdependency;
	else
		hostdependency_hashlist[hashslot]=new_hostdependency;
	new_hostdependency->nexthash=temp_hostdependency;

	return 1;
        }


int add_servicedependency_to_hashlist(servicedependency *new_servicedependency){
	servicedependency *temp_servicedependency=NULL;
	servicedependency *lastpointer=NULL;
	int hashslot=0;

	/* initialize hash list */
	if(servicedependency_hashlist==NULL){
		int i;

		servicedependency_hashlist=(servicedependency **)malloc(sizeof(servicedependency *)*SERVICEDEPENDENCY_HASHSLOTS);
		if(servicedependency_hashlist==NULL)
			return 0;
		
		for(i=0;i< SERVICEDEPENDENCY_HASHSLOTS;i++)
			servicedependency_hashlist[i]=NULL;
	        }

	if(!new_servicedependency)
		return 0;

	hashslot=hashfunc(new_servicedependency->dependent_host_name,new_servicedependency->dependent_service_description,SERVICEDEPENDENCY_HASHSLOTS);
	lastpointer=NULL;
	for(temp_servicedependency=servicedependency_hashlist[hashslot];temp_servicedependency && compare_hashdata(temp_servicedependency->dependent_host_name,temp_servicedependency->dependent_service_description,new_servicedependency->dependent_host_name,new_servicedependency->dependent_service_description)<0;temp_servicedependency=temp_servicedependency->nexthash)
		lastpointer=temp_servicedependency;

	/* duplicates are allowed */
	if(lastpointer)
		lastpointer->nexthash=new_servicedependency;
	else
		servicedependency_hashlist[hashslot]=new_servicedependency;
	new_servicedependency->nexthash=temp_servicedependency;

	return 1;
        }


/* adds hostescalation to hash list in memory */
int add_hostescalation_to_hashlist(hostescalation *new_hostescalation){
	hostescalation *temp_hostescalation=NULL;
	hostescalation *lastpointer=NULL;
	int hashslot=0;

	/* initialize hash list */
	if(hostescalation_hashlist==NULL){
		int i;

		hostescalation_hashlist=(hostescalation **)malloc(sizeof(hostescalation *)*HOSTESCALATION_HASHSLOTS);
		if(hostescalation_hashlist==NULL)
			return 0;
		
		for(i=0;i<HOSTESCALATION_HASHSLOTS;i++)
			hostescalation_hashlist[i]=NULL;
	        }

	if(!new_hostescalation)
		return 0;

	hashslot=hashfunc(new_hostescalation->host_name,NULL,HOSTESCALATION_HASHSLOTS);
	lastpointer=NULL;
	for(temp_hostescalation=hostescalation_hashlist[hashslot];temp_hostescalation && compare_hashdata(temp_hostescalation->host_name,NULL,new_hostescalation->host_name,NULL)<0;temp_hostescalation=temp_hostescalation->nexthash)
		lastpointer=temp_hostescalation;

	/* duplicates are allowed */
	if(lastpointer)
		lastpointer->nexthash=new_hostescalation;
	else
		hostescalation_hashlist[hashslot]=new_hostescalation;
	new_hostescalation->nexthash=temp_hostescalation;

	return 1;
        }


int add_serviceescalation_to_hashlist(serviceescalation *new_serviceescalation){
	serviceescalation *temp_serviceescalation=NULL;
	serviceescalation *lastpointer=NULL;
	int hashslot=0;

	/* initialize hash list */
	if(serviceescalation_hashlist==NULL){
		int i;

		serviceescalation_hashlist=(serviceescalation **)malloc(sizeof(serviceescalation *)*SERVICEESCALATION_HASHSLOTS);
		if(serviceescalation_hashlist==NULL)
			return 0;
		
		for(i=0;i< SERVICEESCALATION_HASHSLOTS;i++)
			serviceescalation_hashlist[i]=NULL;
	        }

	if(!new_serviceescalation)
		return 0;

	hashslot=hashfunc(new_serviceescalation->host_name,new_serviceescalation->description,SERVICEESCALATION_HASHSLOTS);
	lastpointer=NULL;
	for(temp_serviceescalation=serviceescalation_hashlist[hashslot];temp_serviceescalation && compare_hashdata(temp_serviceescalation->host_name,temp_serviceescalation->description,new_serviceescalation->host_name,new_serviceescalation->description)<0;temp_serviceescalation=temp_serviceescalation->nexthash)
		lastpointer=temp_serviceescalation;

	/* duplicates are allowed */
	if(lastpointer)
		lastpointer->nexthash=new_serviceescalation;
	else
		serviceescalation_hashlist[hashslot]=new_serviceescalation;
	new_serviceescalation->nexthash=temp_serviceescalation;

	return 1;
        }



/******************************************************************/
/**************** OBJECT ADDITION FUNCTIONS ***********************/
/******************************************************************/



/* add a new timeperiod to the list in memory */
timeperiod *add_timeperiod(char *name,char *alias){
	timeperiod *new_timeperiod=NULL;
	int day=0;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_timeperiod() start\n");
#endif

	/* make sure we have the data we need */
	if((name==NULL || !strcmp(name,"")) || (alias==NULL || !strcmp(alias,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Name or alias for timeperiod is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* make sure there isn't a timeperiod by this name added already */
	if(find_timeperiod(name)!=NULL){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Timeperiod '%s' has already been defined\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for the new timeperiod */
	if((new_timeperiod=malloc(sizeof(timeperiod)))==NULL)
		return NULL;

	/* initialize values */
	new_timeperiod->name=NULL;
	new_timeperiod->alias=NULL;
	for(day=0;day<7;day++)
		new_timeperiod->days[day]=NULL;
	new_timeperiod->next=NULL;
	new_timeperiod->nexthash=NULL;

	/* copy string vars */
	if((new_timeperiod->name=(char *)strdup(name))==NULL)
		result=ERROR;
	if((new_timeperiod->alias=(char *)strdup(alias))==NULL)
		result=ERROR;

	/* add new timeperiod to timeperiod chained hash list */
	if(result==OK){
		if(!add_timeperiod_to_hashlist(new_timeperiod))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_timeperiod->alias);
		my_free((void **)&new_timeperiod->name);
		my_free((void **)&new_timeperiod);
		return NULL;
	        }

#ifdef NSCORE
	/* timeperiods are sorted alphabetically for daemon, so add new items to tail of list */
	if(timeperiod_list==NULL){
		timeperiod_list=new_timeperiod;
		timeperiod_list_tail=timeperiod_list;
		}
	else{
		timeperiod_list_tail->next=new_timeperiod;
		timeperiod_list_tail=new_timeperiod;
		}
#else
	/* timeperiods are sorted in reverse for CGIs, so add new items to head of list */
	new_timeperiod->next=timeperiod_list;
	timeperiod_list=new_timeperiod;
#endif

#ifdef DEBUG1
	printf("\tName:         %s\n",new_timeperiod->name);
	printf("\tAlias:        %s\n",new_timeperiod->alias);
#endif
#ifdef DEBUG0
	printf("add_timeperiod() end\n");
#endif

	return new_timeperiod;
        }



/* add a new timerange to a timeperiod */
timerange *add_timerange_to_timeperiod(timeperiod *period, int day, unsigned long start_time, unsigned long end_time){
	timerange *new_timerange=NULL;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_timerange_to_timeperiod() start\n");
#endif

	/* make sure we have the data we need */
	if(period==NULL)
		return NULL;

	if(day<0 || day>6){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Day %d is not valid for timeperiod '%s'\n",day,period->name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }
	if(start_time<0 || start_time>86400){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Start time %lu on day %d is not valid for timeperiod '%s'\n",start_time,day,period->name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }
	if(end_time<0 || end_time>86400){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: End time %lu on day %d is not value for timeperiod '%s'\n",end_time,day,period->name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for the new time range */
	if((new_timerange=malloc(sizeof(timerange)))==NULL)
		return NULL;

	new_timerange->range_start=start_time;
	new_timerange->range_end=end_time;

	/* add the new time range to the head of the range list for this day */
	new_timerange->next=period->days[day];
	period->days[day]=new_timerange;

#ifdef DEBUG0
	printf("add_timerange_to_timeperiod() end\n");
#endif

	return new_timerange;
        }


/* add a new host definition */
host *add_host(char *name, char *display_name, char *alias, char *address, char *check_period, int check_interval, int max_attempts, int notify_up, int notify_down, int notify_unreachable, int notify_flapping, int notification_interval, int first_notification_delay, char *notification_period, int notifications_enabled, char *check_command, int checks_enabled, int accept_passive_checks, char *event_handler, int event_handler_enabled, int flap_detection_enabled, double low_flap_threshold, double high_flap_threshold, int flap_detection_on_up, int flap_detection_on_down, int flap_detection_on_unreachable, int stalk_on_up, int stalk_on_down, int stalk_on_unreachable, int process_perfdata, int failure_prediction_enabled, char *failure_prediction_options, int check_freshness, int freshness_threshold, int retain_status_information, int retain_nonstatus_information, int obsess_over_host){
	host *temp_host=NULL;
	host *new_host=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
	int x=0;
#endif

#ifdef DEBUG0
	printf("add_host() start\n");
#endif

	/* make sure we have the data we need */
	if((name==NULL || !strcmp(name,"")) || (alias==NULL || !strcmp(alias,"")) || (address==NULL || !strcmp(address,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Host name, alias, or address is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* make sure there isn't a host by this name already added */
	if(find_host(name)!=NULL){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Host '%s' has already been defined\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* check values */
	if(max_attempts<=0){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Invalid max_check_attempts value for host '%s'\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }
	if(check_interval<0){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Invalid check_interval value for host '%s'\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }
	if(notification_interval<0){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Invalid notification_interval value for host '%s'\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }
	if(first_notification_delay<0){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Invalid first_notification_delay value for host '%s'\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }
	if(freshness_threshold<0){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Invalid freshness_threshold value for host '%s'\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }


	/* allocate memory for a new host */
	if((new_host=(host *)malloc(sizeof(host)))==NULL)
		return NULL;

	/* initialize values */
	new_host->name=NULL;
	new_host->alias=NULL;
	new_host->address=NULL;
	new_host->check_period=NULL;
	new_host->notification_period=NULL;
	new_host->host_check_command=NULL;
	new_host->event_handler=NULL;
	new_host->failure_prediction_options=NULL;
	new_host->display_name=NULL;
	new_host->parent_hosts=NULL;
	new_host->contact_groups=NULL;
	new_host->custom_variables=NULL;
#ifdef NSCORE
	new_host->plugin_output=NULL;
	new_host->long_plugin_output=NULL;
	new_host->perf_data=NULL;
#endif
	new_host->next=NULL;
	new_host->nexthash=NULL;


	/* duplicate string vars */
	if((new_host->name=(char *)strdup(name))==NULL)
		result=ERROR;
	if((new_host->display_name=(char *)strdup((display_name==NULL)?name:display_name))==NULL)
		result=ERROR;
	if((new_host->alias=(char *)strdup(alias))==NULL)
		result=ERROR;
	if((new_host->address=(char *)strdup(address))==NULL)
		result=ERROR;
	if(check_period){
		if((new_host->check_period=(char *)strdup(check_period))==NULL)
			result=ERROR;
	        }
	if(notification_period){
		if((new_host->notification_period=(char *)strdup(notification_period))==NULL)
			result=ERROR;
	        }
	if(check_command){
		if((new_host->host_check_command=(char *)strdup(check_command))==NULL)
			result=ERROR;
	        }
	if(event_handler){
		if((new_host->event_handler=(char *)strdup(event_handler))==NULL)
			result=ERROR;
	        }
	if(failure_prediction_options){
		if((new_host->failure_prediction_options=(char *)strdup(failure_prediction_options))==NULL)
			result=ERROR;
	        }

	/* duplicate non-string vars */
	new_host->max_attempts=max_attempts;
	new_host->check_interval=check_interval;
	new_host->notification_interval=notification_interval;
	new_host->first_notification_delay=first_notification_delay;
	new_host->notify_on_recovery=(notify_up>0)?TRUE:FALSE;
	new_host->notify_on_down=(notify_down>0)?TRUE:FALSE;
	new_host->notify_on_unreachable=(notify_unreachable>0)?TRUE:FALSE;
	new_host->notify_on_flapping=(notify_flapping>0)?TRUE:FALSE;
	new_host->flap_detection_enabled=(flap_detection_enabled>0)?TRUE:FALSE;
	new_host->low_flap_threshold=low_flap_threshold;
	new_host->high_flap_threshold=high_flap_threshold;
	new_host->flap_detection_on_up=(flap_detection_on_up>0)?TRUE:FALSE;
	new_host->flap_detection_on_down=(flap_detection_on_down>0)?TRUE:FALSE;
	new_host->flap_detection_on_unreachable=(flap_detection_on_unreachable>0)?TRUE:FALSE;
	new_host->stalk_on_up=(stalk_on_up>0)?TRUE:FALSE;
	new_host->stalk_on_down=(stalk_on_down>0)?TRUE:FALSE;
	new_host->stalk_on_unreachable=(stalk_on_unreachable>0)?TRUE:FALSE;
	new_host->process_performance_data=(process_perfdata>0)?TRUE:FALSE;
	new_host->check_freshness=(check_freshness>0)?TRUE:FALSE;
	new_host->freshness_threshold=freshness_threshold;
	new_host->accept_passive_host_checks=(accept_passive_checks>0)?TRUE:FALSE;
	new_host->event_handler_enabled=(event_handler_enabled>0)?TRUE:FALSE;
	new_host->failure_prediction_enabled=(failure_prediction_enabled>0)?TRUE:FALSE;
	new_host->obsess_over_host=(obsess_over_host>0)?TRUE:FALSE;
	new_host->retain_status_information=(retain_status_information>0)?TRUE:FALSE;
	new_host->retain_nonstatus_information=(retain_nonstatus_information>0)?TRUE:FALSE;
#ifdef NSCORE
	new_host->current_state=HOST_UP;
	new_host->current_event_id=0L;
	new_host->last_event_id=0L;
	new_host->last_state=HOST_UP;
	new_host->last_hard_state=HOST_UP;
	new_host->check_type=HOST_CHECK_ACTIVE;
	new_host->last_host_notification=(time_t)0;
	new_host->next_host_notification=(time_t)0;
	new_host->next_check=(time_t)0;
	new_host->should_be_scheduled=TRUE;
	new_host->last_check=(time_t)0;
	new_host->current_attempt=1;
	new_host->state_type=HARD_STATE;
	new_host->execution_time=0.0;
	new_host->latency=0.0;
	new_host->last_state_change=(time_t)0;
	new_host->last_hard_state_change=(time_t)0;
	new_host->last_time_up=(time_t)0;
	new_host->last_time_down=(time_t)0;
	new_host->last_time_unreachable=(time_t)0;
	new_host->has_been_checked=FALSE;
	new_host->is_being_freshened=FALSE;
	new_host->problem_has_been_acknowledged=FALSE;
	new_host->acknowledgement_type=ACKNOWLEDGEMENT_NONE;
	new_host->notified_on_down=FALSE;
	new_host->notified_on_unreachable=FALSE;
	new_host->current_notification_number=0;
	new_host->current_notification_id=0L;
	new_host->no_more_notifications=FALSE;
	new_host->check_flapping_recovery_notification=FALSE;
	new_host->checks_enabled=(checks_enabled>0)?TRUE:FALSE;
	new_host->notifications_enabled=(notifications_enabled>0)?TRUE:FALSE;
	new_host->scheduled_downtime_depth=0;
	new_host->check_options=CHECK_OPTION_NONE;
	new_host->pending_flex_downtime=0;
	for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
		new_host->state_history[x]=STATE_OK;
	new_host->state_history_index=0;
	new_host->last_state_history_update=(time_t)0;
	new_host->is_flapping=FALSE;
	new_host->flapping_comment_id=0;
	new_host->percent_state_change=0.0;
	new_host->total_services=0;
	new_host->total_service_check_interval=0L;
	new_host->modified_attributes=MODATTR_NONE;
	new_host->circular_path_checked=FALSE;
	new_host->contains_circular_path=FALSE;
#endif

	/* add new host to host chained hash list */
	if(result==OK){
		if(!add_host_to_hashlist(new_host))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
#ifdef NSCORE
		my_free((void **)&new_host->plugin_output);
		my_free((void **)&new_host->long_plugin_output);
		my_free((void **)&new_host->perf_data);
#endif
		my_free((void **)&new_host->display_name);
		my_free((void **)&new_host->failure_prediction_options);
		my_free((void **)&new_host->event_handler);
		my_free((void **)&new_host->host_check_command);
		my_free((void **)&new_host->notification_period);
		my_free((void **)&new_host->check_period);
		my_free((void **)&new_host->address);
		my_free((void **)&new_host->alias);
		my_free((void **)&new_host->name);
		my_free((void **)&new_host);
		return NULL;
	        }

#ifdef NSCORE
	/* hosts are sorted alphabetically for daemon, so add new items to tail of list */
	if(host_list==NULL){
		host_list=new_host;
		host_list_tail=host_list;
		}
	else{
		host_list_tail->next=new_host;
		host_list_tail=new_host;
		}
#else
	/* hosts are sorted in reverse for CGIs, so add new items to head of list */
	new_host->next=host_list;
	host_list=new_host;
#endif

#ifdef DEBUG1
	printf("\tHost Name:                %s\n",new_host->name);
	printf("\tHost Alias:               %s\n",new_host->alias);
        printf("\tHost Address:             %s\n",new_host->address);
	printf("\tHost Check Command:       %s\n",new_host->host_check_command);
	printf("\tMax. Check Attempts:      %d\n",new_host->max_attempts);
	printf("\tHost Event Handler:       %s\n",(new_host->event_handler==NULL)?"N/A":new_host->event_handler);
	printf("\tNotify On Down:           %s\n",(new_host->notify_on_down==1)?"yes":"no");
	printf("\tNotify On Unreachable:    %s\n",(new_host->notify_on_unreachable==1)?"yes":"no");
	printf("\tNotify On Recovery:       %s\n",(new_host->notify_on_recovery==1)?"yes":"no");
	printf("\tNotification Interval:    %d\n",new_host->notification_interval);
	printf("\tNotification Time Period: %s\n",(new_host->notification_period==NULL)?"N/A":new_host->notification_period);
#endif
#ifdef DEBUG0
	printf("add_host() end\n");
#endif

	return new_host;
	}



hostsmember *add_parent_host_to_host(host *hst,char *host_name){
	hostsmember *new_hostsmember=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_parent_host_to_host() start\n");
#endif

	/* make sure we have the data we need */
	if(hst==NULL || host_name==NULL || !strcmp(host_name,"")){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Host is NULL or parent host name is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* a host cannot be a parent/child of itself */
	if(!strcmp(host_name,hst->name)){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Host '%s' cannot be a child/parent of itself\n",hst->name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory */
	if((new_hostsmember=(hostsmember *)malloc(sizeof(hostsmember)))==NULL)
		return NULL;

	/* initialize values */
	new_hostsmember->host_name=NULL;

	/* duplicate string vars */
	if((new_hostsmember->host_name=(char *)strdup(host_name))==NULL)
		result=ERROR;

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_hostsmember->host_name);
		my_free((void **)&new_hostsmember);
		return NULL;
	        }

	/* add the parent host entry to the host definition */
	new_hostsmember->next=hst->parent_hosts;
	hst->parent_hosts=new_hostsmember;

#ifdef DEBUG0
	printf("add_parent_host_to_host() end\n");
#endif

	return new_hostsmember;
        }



/* add a new contactgroup to a host */
contactgroupsmember *add_contactgroup_to_host(host *hst, char *group_name){
	contactgroupsmember *new_contactgroupsmember=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_contactgroup_to_host() start\n");
#endif

	/* make sure we have the data we need */
	if(hst==NULL || (group_name==NULL || !strcmp(group_name,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Host or contactgroup member is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for a new member */
	if((new_contactgroupsmember=malloc(sizeof(contactgroupsmember)))==NULL)
		return NULL;
	
	/* initialize vars */
	new_contactgroupsmember->group_name=NULL;

	/* duplicate string vars */
	if((new_contactgroupsmember->group_name=(char *)strdup(group_name))==NULL)
		result=ERROR;

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_contactgroupsmember->group_name);
		my_free((void **)&new_contactgroupsmember);
		return NULL;
	        }
	
	/* add the new member to the head of the member list */
	new_contactgroupsmember->next=hst->contact_groups;
	hst->contact_groups=new_contactgroupsmember;;

#ifdef DEBUG0
	printf("add_host_to_host() end\n");
#endif

	return new_contactgroupsmember;
        }



/* adds a custom variable to a host */
customvariablesmember *add_custom_variable_to_host(host *hst, char *varname, char *varvalue){

	return add_custom_variable_to_object(&hst->custom_variables,varname,varvalue);
        }



/* add a new host group to the list in memory */
hostgroup *add_hostgroup(char *name,char *alias){
	hostgroup *new_hostgroup=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_hostgroup() start\n");
#endif

	/* make sure we have the data we need */
	if((name==NULL || !strcmp(name,"")) || (alias==NULL || !strcmp(alias,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Hostgroup name and/or alias is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* make sure a hostgroup by this name hasn't been added already */
	if(find_hostgroup(name)!=NULL){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Hostgroup '%s' has already been defined\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory */
	if((new_hostgroup=(hostgroup *)malloc(sizeof(hostgroup)))==NULL)
		return NULL;

	/* initialize vars */
	new_hostgroup->group_name=NULL;
	new_hostgroup->alias=NULL;
	new_hostgroup->members=NULL;
	new_hostgroup->next=NULL;
	new_hostgroup->nexthash=NULL;

	/* duplicate vars */
	if((new_hostgroup->group_name=(char *)strdup(name))==NULL)
		result=ERROR;
	if((new_hostgroup->alias=(char *)strdup(alias))==NULL)
		result=ERROR;

	/* add new hostgroup to hostgroup chained hash list */
	if(result==OK){
		if(!add_hostgroup_to_hashlist(new_hostgroup))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_hostgroup->alias);
		my_free((void **)&new_hostgroup->group_name);
		my_free((void **)&new_hostgroup);
		return NULL;
	        }

#ifdef NSCORE
	/* hostgroups are sorted alphabetically for daemon, so add new items to tail of list */
	if(hostgroup_list==NULL){
		hostgroup_list=new_hostgroup;
		hostgroup_list_tail=hostgroup_list;
		}
	else{
		hostgroup_list_tail->next=new_hostgroup;
		hostgroup_list_tail=new_hostgroup;
		}
#else
	/* hostgroups are sorted in reverse for CGIs, so add new items to head of list */
	new_hostgroup->next=hostgroup_list;
	hostgroup_list=new_hostgroup;
#endif
		
#ifdef DEBUG1
	printf("\tGroup name:     %s\n",new_hostgroup->group_name);
	printf("\tAlias:          %s\n",new_hostgroup->alias);
#endif

#ifdef DEBUG0
	printf("add_hostgroup() end\n");
#endif

	return new_hostgroup;
	}


/* add a new host to a host group */
hostgroupmember *add_host_to_hostgroup(hostgroup *temp_hostgroup, char *host_name){
	hostgroupmember *new_member=NULL;
	hostgroupmember *last_member=NULL;
	hostgroupmember *temp_member=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_host_to_hostgroup() start\n");
#endif

	/* make sure we have the data we need */
	if(temp_hostgroup==NULL || (host_name==NULL || !strcmp(host_name,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Hostgroup or group member is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for a new member */
	if((new_member=malloc(sizeof(hostgroupmember)))==NULL)
		return NULL;

	/* initialize vars */
	new_member->host_name=NULL;

	/* duplicate vars */
	if((new_member->host_name=(char *)strdup(host_name))==NULL)
		result=ERROR;

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_member->host_name);
		my_free((void **)&new_member);
		return NULL;
	        }
	
	/* add the new member to the member list, sorted by host name */
	last_member=temp_hostgroup->members;
	for(temp_member=temp_hostgroup->members;temp_member!=NULL;temp_member=temp_member->next){
		if(strcmp(new_member->host_name,temp_member->host_name)<0){
			new_member->next=temp_member;
			if(temp_member==temp_hostgroup->members)
				temp_hostgroup->members=new_member;
			else
				last_member->next=new_member;
			break;
		        }
		else
			last_member=temp_member;
	        }
	if(temp_hostgroup->members==NULL){
		new_member->next=NULL;
		temp_hostgroup->members=new_member;
	        }
	else if(temp_member==NULL){
		new_member->next=NULL;
		last_member->next=new_member;
	        }

#ifdef DEBUG0
	printf("add_host_to_hostgroup() end\n");
#endif

	return new_member;
        }


/* add a new service group to the list in memory */
servicegroup *add_servicegroup(char *name,char *alias){
	servicegroup *new_servicegroup=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_servicegroup() start\n");
#endif

	/* make sure we have the data we need */
	if((name==NULL || !strcmp(name,"")) || (alias==NULL || !strcmp(alias,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Servicegroup name and/or alias is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* make sure a servicegroup by this name hasn't been added already */
	if(find_servicegroup(name)!=NULL){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Servicegroup '%s' has already been defined\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory */
	if((new_servicegroup=(servicegroup *)malloc(sizeof(servicegroup)))==NULL)
		return NULL;

	/* initialize vars */
	new_servicegroup->group_name=NULL;
	new_servicegroup->alias=NULL;
	new_servicegroup->members=NULL;
	new_servicegroup->next=NULL;
	new_servicegroup->nexthash=NULL;

	/* duplicate vars */
	if((new_servicegroup->group_name=(char *)strdup(name))==NULL)
		result=ERROR;
	if((new_servicegroup->alias=(char *)strdup(alias))==NULL)
		result=ERROR;

	/* add new servicegroup to servicegroup chained hash list */
	if(result==OK){
		if(!add_servicegroup_to_hashlist(new_servicegroup))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_servicegroup->alias);
		my_free((void **)&new_servicegroup->group_name);
		my_free((void **)&new_servicegroup);
		return NULL;
	        }

#ifdef NSCORE
	/* servicegroups are sorted alphabetically for daemon, so add new items to tail of list */
	if(servicegroup_list==NULL){
		servicegroup_list=new_servicegroup;
		servicegroup_list_tail=servicegroup_list;
		}
	else{
		servicegroup_list_tail->next=new_servicegroup;
		servicegroup_list_tail=new_servicegroup;
		}
#else
	/* servicegroups are sorted in reverse for CGIs, so add new items to head of list */
	new_servicegroup->next=servicegroup_list;
	servicegroup_list=new_servicegroup;
#endif
		
#ifdef DEBUG1
	printf("\tGroup name:     %s\n",new_servicegroup->group_name);
	printf("\tAlias:          %s\n",new_servicegroup->alias);
#endif

#ifdef DEBUG0
	printf("add_servicegroup() end\n");
#endif

	return new_servicegroup;
	}


/* add a new service to a service group */
servicegroupmember *add_service_to_servicegroup(servicegroup *temp_servicegroup, char *host_name, char *svc_description){
	servicegroupmember *new_member=NULL;
	servicegroupmember *last_member=NULL;
	servicegroupmember *temp_member=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_service_to_servicegroup() start\n");
#endif

	/* make sure we have the data we need */
	if(temp_servicegroup==NULL || (host_name==NULL || !strcmp(host_name,"")) || (svc_description==NULL || !strcmp(svc_description,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Servicegroup or group member is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for a new member */
	if((new_member=malloc(sizeof(servicegroupmember)))==NULL)
		return NULL;

	/* initialize vars */
	new_member->host_name=NULL;
	new_member->service_description=NULL;

	/* duplicate vars */
	if((new_member->host_name=(char *)strdup(host_name))==NULL)
		result=ERROR;
	if((new_member->service_description=(char *)strdup(svc_description))==NULL)
		result=ERROR;

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_member->service_description);
		my_free((void **)&new_member->host_name);
		my_free((void **)&new_member);
		return NULL;
	        }
	
	/* add new member to member list, sorted by host name then service description */
	last_member=temp_servicegroup->members;
	for(temp_member=temp_servicegroup->members;temp_member!=NULL;temp_member=temp_member->next){

		if(strcmp(new_member->host_name,temp_member->host_name)<0){
			new_member->next=temp_member;
			if(temp_member==temp_servicegroup->members)
				temp_servicegroup->members=new_member;
			else
				last_member->next=new_member;
			break;
		        }

		else if(strcmp(new_member->host_name,temp_member->host_name)==0 && strcmp(new_member->service_description,temp_member->service_description)<0){
			new_member->next=temp_member;
			if(temp_member==temp_servicegroup->members)
				temp_servicegroup->members=new_member;
			else
				last_member->next=new_member;
			break;
		        }

		else
			last_member=temp_member;
	        }
	if(temp_servicegroup->members==NULL){
		new_member->next=NULL;
		temp_servicegroup->members=new_member;
	        }
	else if(temp_member==NULL){
		new_member->next=NULL;
		last_member->next=new_member;
	        }

#ifdef DEBUG0
	printf("add_service_to_servicegroup() end\n");
#endif

	return new_member;
        }


/* add a new contact to the list in memory */
contact *add_contact(char *name,char *alias, char *email, char *pager, char **addresses, char *svc_notification_period, char *host_notification_period,int notify_service_ok,int notify_service_critical,int notify_service_warning, int notify_service_unknown, int notify_service_flapping, int notify_host_up, int notify_host_down, int notify_host_unreachable, int notify_host_flapping, int host_notifications_enabled, int service_notifications_enabled, int can_submit_commands, int retain_status_information, int retain_nonstatus_information){
	contact *new_contact=NULL;
	int x=0;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_contact() start\n");
#endif

	/* make sure we have the data we need */
	if((name==NULL || !strcmp(name,"")) || (alias==NULL || !strcmp(alias,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Contact name or alias are NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* make sure there isn't a contact by this name already added */
	if(find_contact(name)!=NULL){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Contact '%s' has already been defined\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for a new contact */
	if((new_contact=(contact *)malloc(sizeof(contact)))==NULL)
		return NULL;

	/* initialize vars */
	new_contact->name=NULL;
	new_contact->alias=NULL;
	new_contact->email=NULL;
	new_contact->pager=NULL;
	for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
		new_contact->address[x]=NULL;
	new_contact->host_notification_commands=NULL;
	new_contact->service_notification_commands=NULL;
	new_contact->custom_variables=NULL;
	new_contact->next=NULL;
	new_contact->nexthash=NULL;

	/* duplicate vars */
	if((new_contact->name=(char *)strdup(name))==NULL)
		result=ERROR;
	if((new_contact->alias=(char *)strdup(alias))==NULL)
		result=ERROR;
	if(email){
		if((new_contact->email=(char *)strdup(email))==NULL)
			result=ERROR;
	        }
	if(pager){
		if((new_contact->pager=(char *)strdup(pager))==NULL)
			result=ERROR;
	        }
	if(svc_notification_period){
		if((new_contact->service_notification_period=(char *)strdup(svc_notification_period))==NULL)
			result=ERROR;
	        }
	if(host_notification_period){
		if((new_contact->host_notification_period=(char *)strdup(host_notification_period))==NULL)
			result=ERROR;
	        }
	if(addresses){
		for(x=0;x<MAX_CONTACT_ADDRESSES;x++){
			if(addresses[x]){
				if((new_contact->address[x]=(char *)strdup(addresses[x]))==NULL)
					result=ERROR;
			        }
		        }
	        }

	new_contact->notify_on_service_recovery=(notify_service_ok>0)?TRUE:FALSE;
	new_contact->notify_on_service_critical=(notify_service_critical>0)?TRUE:FALSE;
	new_contact->notify_on_service_warning=(notify_service_warning>0)?TRUE:FALSE;
	new_contact->notify_on_service_unknown=(notify_service_unknown>0)?TRUE:FALSE;
	new_contact->notify_on_service_flapping=(notify_service_flapping>0)?TRUE:FALSE;
	new_contact->notify_on_host_recovery=(notify_host_up>0)?TRUE:FALSE;
	new_contact->notify_on_host_down=(notify_host_down>0)?TRUE:FALSE;
	new_contact->notify_on_host_unreachable=(notify_host_unreachable>0)?TRUE:FALSE;
	new_contact->notify_on_host_flapping=(notify_host_flapping>0)?TRUE:FALSE;
	new_contact->host_notifications_enabled=(host_notifications_enabled>0)?TRUE:FALSE;
	new_contact->service_notifications_enabled=(service_notifications_enabled>0)?TRUE:FALSE;
	new_contact->can_submit_commands=(can_submit_commands>0)?TRUE:FALSE;
	new_contact->retain_status_information=(retain_status_information>0)?TRUE:FALSE;
	new_contact->retain_nonstatus_information=(retain_nonstatus_information>0)?TRUE:FALSE;
#ifdef NSCORE
	new_contact->last_host_notification=(time_t)0L;
	new_contact->last_service_notification=(time_t)0L;
	new_contact->modified_attributes=MODATTR_NONE;
	new_contact->modified_host_attributes=MODATTR_NONE;
	new_contact->modified_service_attributes=MODATTR_NONE;
#endif

	/* add new contact to contact chained hash list */
	if(result==OK){
		if(!add_contact_to_hashlist(new_contact))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
		for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
			my_free((void **)&new_contact->address[x]);
		my_free((void **)&new_contact->name);
		my_free((void **)&new_contact->alias);
		my_free((void **)&new_contact->email);
		my_free((void **)&new_contact->pager);
		my_free((void **)&new_contact->service_notification_period);
		my_free((void **)&new_contact->host_notification_period);
		my_free((void **)&new_contact);
		return NULL;
	        }

#ifdef NSCORE
	/* contacts are sorted alphabetically for daemon, so add new items to tail of list */
	if(contact_list==NULL){
		contact_list=new_contact;
		contact_list_tail=contact_list;
		}
	else{
		contact_list_tail->next=new_contact;
		contact_list_tail=new_contact;
		}
#else
	/* contacts are sorted in reverse for CGIs, so add new items to head of list */
	new_contact->next=contact_list;
	contact_list=new_contact;
#endif
		
#ifdef DEBUG1
	printf("\tContact Name:                  %s\n",new_contact->name);
	printf("\tContact Alias:                 %s\n",new_contact->alias);
	printf("\tContact Email Address:         %s\n",(new_contact->email==NULL)?"":new_contact->email);
	printf("\tContact Pager Address/Number:  %s\n",(new_contact->pager==NULL)?"":new_contact->pager);
	printf("\tSvc Notification Time Period:  %s\n",new_contact->service_notification_period);
	printf("\tHost Notification Time Period: %s\n",new_contact->host_notification_period);
#endif
#ifdef DEBUG0
	printf("add_contact() end\n");
#endif

	return new_contact;
	}



/* adds a host notification command to a contact definition */
commandsmember *add_host_notification_command_to_contact(contact *cntct,char *command_name){
	commandsmember *new_commandsmember=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_host_notification_command_to_contact() start\n");
#endif

	/* make sure we have the data we need */
	if(cntct==NULL || (command_name==NULL || !strcmp(command_name,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Contact or host notification command is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory */
	if((new_commandsmember=malloc(sizeof(commandsmember)))==NULL)
		return NULL;

	/* initialize vars */
	new_commandsmember->command=NULL;

	/* duplicate vars */
	if((new_commandsmember->command=(char *)strdup(command_name))==NULL)
		result=ERROR;

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_commandsmember->command);
		my_free((void **)&new_commandsmember);
		return NULL;
	        }

	/* add the notification command */
	new_commandsmember->next=cntct->host_notification_commands;
	cntct->host_notification_commands=new_commandsmember;

#ifdef DEBUG0
	printf("add_host_notification_command_to_contact() end\n");
#endif

	return new_commandsmember;
        }



/* adds a service notification command to a contact definition */
commandsmember *add_service_notification_command_to_contact(contact *cntct,char *command_name){
	commandsmember *new_commandsmember=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_service_notification_command_to_contact() start\n");
#endif

	/* make sure we have the data we need */
	if(cntct==NULL || (command_name==NULL || !strcmp(command_name,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Contact or service notification command is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory */
	if((new_commandsmember=malloc(sizeof(commandsmember)))==NULL)
		return NULL;

	/* initialize vars */
	new_commandsmember->command=NULL;

	/* duplicate vars */
	if((new_commandsmember->command=(char *)strdup(command_name))==NULL)
		result=ERROR;

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_commandsmember->command);
		my_free((void **)&new_commandsmember);
		return NULL;
	        }

	/* add the notification command */
	new_commandsmember->next=cntct->service_notification_commands;
	cntct->service_notification_commands=new_commandsmember;

#ifdef DEBUG0
	printf("add_service_notification_command_to_contact() end\n");
#endif

	return new_commandsmember;
        }



/* adds a custom variable to a contact */
customvariablesmember *add_custom_variable_to_contact(contact *cntct, char *varname, char *varvalue){

	return add_custom_variable_to_object(&cntct->custom_variables,varname,varvalue);
        }



/* add a new contact group to the list in memory */
contactgroup *add_contactgroup(char *name,char *alias){
	contactgroup *new_contactgroup=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_contactgroup() start\n");
#endif

	/* make sure we have the data we need */
	if((name==NULL || !strcmp(name,"")) || (alias==NULL || !strcmp(alias,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Contactgroup name or alias is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* make sure there isn't a contactgroup by this name added already */
	if(find_contactgroup(name)!=NULL){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Contactgroup '%s' has already been defined\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for a new contactgroup entry */
	if((new_contactgroup=malloc(sizeof(contactgroup)))==NULL)
		return NULL;

	/* initialize vars */
	new_contactgroup->group_name=NULL;
	new_contactgroup->alias=NULL;
	new_contactgroup->members=NULL;
	new_contactgroup->next=NULL;
	new_contactgroup->nexthash=NULL;

	/* duplicate vars */
	if((new_contactgroup->group_name=(char *)strdup(name))==NULL)
		result=ERROR;
	if((new_contactgroup->alias=(char *)strdup(alias))==NULL)
		result=ERROR;

	/* add new contactgroup to contactgroup chained hash list */
	if(result==OK){
		if(!add_contactgroup_to_hashlist(new_contactgroup))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_contactgroup->alias);
		my_free((void **)&new_contactgroup->group_name);
		my_free((void **)&new_contactgroup);
		return NULL;
	        }

#ifdef NSCORE
	/* contactgroups are sorted alphabetically for daemon, so add new items to tail of list */
	if(contactgroup_list==NULL){
		contactgroup_list=new_contactgroup;
		contactgroup_list_tail=contactgroup_list;
		}
	else{
		contactgroup_list_tail->next=new_contactgroup;
		contactgroup_list_tail=new_contactgroup;
		}
#else
	/* contactgroups are sorted in reverse for CGIs, so add new items to head of list */
	new_contactgroup->next=contactgroup_list;
	contactgroup_list=new_contactgroup;
#endif		

#ifdef DEBUG1
	printf("\tGroup name:   %s\n",new_contactgroup->group_name);
	printf("\tAlias:        %s\n",new_contactgroup->alias);
#endif

#ifdef DEBUG0
	printf("add_contactgroup() end\n");
#endif

	return new_contactgroup;
	}



/* add a new member to a contact group */
contactgroupmember *add_contact_to_contactgroup(contactgroup *grp,char *contact_name){
	contactgroupmember *new_contactgroupmember=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_contact_to_contactgroup() start\n");
#endif

	/* make sure we have the data we need */
	if(grp==NULL || (contact_name==NULL || !strcmp(contact_name,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Contactgroup or contact name is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for a new member */
	if((new_contactgroupmember=malloc(sizeof(contactgroupmember)))==NULL)
		return NULL;

	/* initialize vars */
	new_contactgroupmember->contact_name=NULL;

	/* duplicate vars */
	if((new_contactgroupmember->contact_name=(char *)strdup(contact_name))==NULL)
		result=ERROR;

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_contactgroupmember->contact_name);
		my_free((void **)&new_contactgroupmember);
		return NULL;
	        }
	
	/* add the new member to the head of the member list */
	new_contactgroupmember->next=grp->members;
	grp->members=new_contactgroupmember;

#ifdef DEBUG0
	printf("add_contact_to_contactgroup() end\n");
#endif

	return new_contactgroupmember;
        }



/* add a new service to the list in memory */
service *add_service(char *host_name, char *description, char *display_name, char *check_period, int max_attempts, int parallelize, int accept_passive_checks, int check_interval, int retry_interval, int notification_interval, int first_notification_delay, char *notification_period, int notify_recovery, int notify_unknown, int notify_warning, int notify_critical, int notify_flapping, int notifications_enabled, int is_volatile, char *event_handler, int event_handler_enabled, char *check_command, int checks_enabled, int flap_detection_enabled, double low_flap_threshold, double high_flap_threshold, int flap_detection_on_ok, int flap_detection_on_warning, int flap_detection_on_unknown, int flap_detection_on_critical, int stalk_on_ok, int stalk_on_warning, int stalk_on_unknown, int stalk_on_critical, int process_perfdata, int failure_prediction_enabled, char *failure_prediction_options, int check_freshness, int freshness_threshold, char *notes, char *notes_url, char *action_url, char *icon_image, char *icon_image_alt, int retain_status_information, int retain_nonstatus_information, int obsess_over_service){
	service *new_service=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
	int x=0;
#endif

#ifdef DEBUG0
	printf("add_service() start\n");
#endif

	/* make sure we have everything we need */
	if((host_name==NULL || !strcmp(host_name,"")) || (description==NULL || !strcmp(description,"")) || (check_command==NULL || !strcmp(check_command,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Service description, host name, or check command is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* make sure there isn't a service by this name added already */
	if(find_service(host_name,description)!=NULL){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Service '%s' on host '%s' has already been defined\n",description,host_name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* check values */
	if(max_attempts<=0 || check_interval<0 || retry_interval<=0 || notification_interval<0){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Invalid max_attempts, check_interval, retry_interval, or notification_interval value for service '%s' on host '%s'\n",description,host_name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	if(first_notification_delay<0){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Invalid first_notification_delay value for service '%s' on host '%s'\n",description,host_name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory */
	if((new_service=(service *)malloc(sizeof(service)))==NULL)
		return NULL;

	/* initialize vars */
	new_service->host_name=NULL;
	new_service->description=NULL;
	new_service->display_name=NULL;
	new_service->event_handler=NULL;
	new_service->notification_period=NULL;
	new_service->check_period=NULL;
	new_service->failure_prediction_options=NULL;
	new_service->notes=NULL;
	new_service->notes_url=NULL;
	new_service->action_url=NULL;
	new_service->icon_image=NULL;
	new_service->icon_image_alt=NULL;
	new_service->contact_groups=NULL;
	new_service->custom_variables=NULL;
#ifdef NSCORE
	new_service->plugin_output=NULL;
	new_service->long_plugin_output=NULL;
	new_service->perf_data=NULL;
#endif
	new_service->next=NULL;
	new_service->nexthash=NULL;

	/* duplicate vars */
	if((new_service->host_name=(char *)strdup(host_name))==NULL)
		result=ERROR;
	if((new_service->description=(char *)strdup(description))==NULL)
		result=ERROR;
	if((new_service->display_name=(char *)strdup((display_name==NULL)?description:display_name))==NULL)
		result=ERROR;
	if((new_service->service_check_command=(char *)strdup(check_command))==NULL)
		result=ERROR;
	if(event_handler){
		if((new_service->event_handler=(char *)strdup(event_handler))==NULL)
			result=ERROR;
	        }
	if(notification_period){
		if((new_service->notification_period=(char *)strdup(notification_period))==NULL)
			result=ERROR;
	        }
	if(check_period){
		if((new_service->check_period=(char *)strdup(check_period))==NULL)
			result=ERROR;
	        }
	if(failure_prediction_options){
		if((new_service->failure_prediction_options=(char *)strdup(failure_prediction_options))==NULL)
			result=ERROR;
	        }
	if(notes){
		if((new_service->notes=(char *)strdup(notes))==NULL)
			result=ERROR;
	        }
	if(notes_url){
		if((new_service->notes_url=(char *)strdup(notes_url))==NULL)
			result=ERROR;
	        }
	if(action_url){
		if((new_service->action_url=(char *)strdup(action_url))==NULL)
			result=ERROR;
	        }
	if(icon_image){
		if((new_service->icon_image=(char *)strdup(icon_image))==NULL)
			result=ERROR;
	        }
	if(icon_image_alt){
		if((new_service->icon_image_alt=(char *)strdup(icon_image_alt))==NULL)
			result=ERROR;
	        }

	new_service->check_interval=check_interval;
	new_service->retry_interval=retry_interval;
	new_service->max_attempts=max_attempts;
	new_service->parallelize=(parallelize>0)?TRUE:FALSE;
	new_service->notification_interval=notification_interval;
	new_service->first_notification_delay=first_notification_delay;
	new_service->notify_on_unknown=(notify_unknown>0)?TRUE:FALSE;
	new_service->notify_on_warning=(notify_warning>0)?TRUE:FALSE;
	new_service->notify_on_critical=(notify_critical>0)?TRUE:FALSE;
	new_service->notify_on_recovery=(notify_recovery>0)?TRUE:FALSE;
	new_service->notify_on_flapping=(notify_flapping>0)?TRUE:FALSE;
	new_service->is_volatile=(is_volatile>0)?TRUE:FALSE;
	new_service->flap_detection_enabled=(flap_detection_enabled>0)?TRUE:FALSE;
	new_service->low_flap_threshold=low_flap_threshold;
	new_service->high_flap_threshold=high_flap_threshold;
	new_service->flap_detection_on_ok=(flap_detection_on_ok>0)?TRUE:FALSE;
	new_service->flap_detection_on_warning=(flap_detection_on_warning>0)?TRUE:FALSE;
	new_service->flap_detection_on_unknown=(flap_detection_on_unknown>0)?TRUE:FALSE;
	new_service->flap_detection_on_critical=(flap_detection_on_critical>0)?TRUE:FALSE;
	new_service->stalk_on_ok=(stalk_on_ok>0)?TRUE:FALSE;
	new_service->stalk_on_warning=(stalk_on_warning>0)?TRUE:FALSE;
	new_service->stalk_on_unknown=(stalk_on_unknown>0)?TRUE:FALSE;
	new_service->stalk_on_critical=(stalk_on_critical>0)?TRUE:FALSE;
	new_service->process_performance_data=(process_perfdata>0)?TRUE:FALSE;
	new_service->check_freshness=(check_freshness>0)?TRUE:FALSE;
	new_service->freshness_threshold=freshness_threshold;
	new_service->accept_passive_service_checks=(accept_passive_checks>0)?TRUE:FALSE;
	new_service->event_handler_enabled=(event_handler_enabled>0)?TRUE:FALSE;
	new_service->checks_enabled=(checks_enabled>0)?TRUE:FALSE;
	new_service->retain_status_information=(retain_status_information>0)?TRUE:FALSE;
	new_service->retain_nonstatus_information=(retain_nonstatus_information>0)?TRUE:FALSE;
	new_service->notifications_enabled=(notifications_enabled>0)?TRUE:FALSE;
	new_service->obsess_over_service=(obsess_over_service>0)?TRUE:FALSE;
	new_service->failure_prediction_enabled=(failure_prediction_enabled>0)?TRUE:FALSE;
#ifdef NSCORE
	new_service->problem_has_been_acknowledged=FALSE;
	new_service->acknowledgement_type=ACKNOWLEDGEMENT_NONE;
	new_service->check_type=SERVICE_CHECK_ACTIVE;
	new_service->current_attempt=1;
	new_service->current_state=STATE_OK;
	new_service->current_event_id=0L;
	new_service->last_event_id=0L;
	new_service->last_state=STATE_OK;
	new_service->last_hard_state=STATE_OK;
	new_service->state_type=HARD_STATE;
	new_service->host_problem_at_last_check=FALSE;
	new_service->check_flapping_recovery_notification=FALSE;
	new_service->next_check=(time_t)0;
	new_service->should_be_scheduled=TRUE;
	new_service->last_check=(time_t)0;
	new_service->last_notification=(time_t)0;
	new_service->next_notification=(time_t)0;
	new_service->no_more_notifications=FALSE;
	new_service->last_state_change=(time_t)0;
	new_service->last_hard_state_change=(time_t)0;
	new_service->last_time_ok=(time_t)0;
	new_service->last_time_warning=(time_t)0;
	new_service->last_time_unknown=(time_t)0;
	new_service->last_time_critical=(time_t)0;
	new_service->has_been_checked=FALSE;
	new_service->is_being_freshened=FALSE;
	new_service->notified_on_unknown=FALSE;
	new_service->notified_on_warning=FALSE;
	new_service->notified_on_critical=FALSE;
	new_service->current_notification_number=0;
	new_service->current_notification_id=0L;
	new_service->latency=0.0;
	new_service->execution_time=0.0;
	new_service->is_executing=FALSE;
	new_service->check_options=CHECK_OPTION_NONE;
	new_service->scheduled_downtime_depth=0;
	new_service->pending_flex_downtime=0;
	for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
		new_service->state_history[x]=STATE_OK;
	new_service->state_history_index=0;
	new_service->is_flapping=FALSE;
	new_service->flapping_comment_id=0;
	new_service->percent_state_change=0.0;
	new_service->modified_attributes=MODATTR_NONE;
#endif

	/* add new service to service chained hash list */
	if(result==OK){
		if(!add_service_to_hashlist(new_service))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
#ifdef NSCORE
		my_free((void **)&new_service->perf_data);
		my_free((void **)&new_service->plugin_output);
		my_free((void **)&new_service->long_plugin_output);
#endif
		my_free((void **)&new_service->failure_prediction_options);
		my_free((void **)&new_service->notification_period);
		my_free((void **)&new_service->event_handler);
		my_free((void **)&new_service->service_check_command);
		my_free((void **)&new_service->description);
		my_free((void **)&new_service->host_name);
		my_free((void **)&new_service);
		return NULL;
	        }

#ifdef NSCORE
	/* services are sorted alphabetically for daemon, so add new items to tail of list */
	if(service_list==NULL){
		service_list=new_service;
		service_list_tail=service_list;
		}
	else{
		service_list_tail->next=new_service;
		service_list_tail=new_service;
		}
#else
	/* services are sorted in reverse for CGIs, so add new items to head of list */
	new_service->next=service_list;
	service_list=new_service;
#endif

#ifdef DEBUG1
	printf("\tHost:                     %s\n",new_service->host_name);
	printf("\tDescription:              %s\n",new_service->description);
	printf("\tCommand:                  %s\n",new_service->service_check_command);
	printf("\tCheck Interval:           %d\n",new_service->check_interval);
	printf("\tRetry Interval:           %d\n",new_service->retry_interval);
	printf("\tMax attempts:             %d\n",new_service->max_attempts);
	printf("\tNotification Interval:    %d\n",new_service->notification_interval);
	printf("\tNotification Time Period: %s\n",new_service->notification_period);
	printf("\tNotify On Warning:        %s\n",(new_service->notify_on_warning==1)?"yes":"no");
	printf("\tNotify On Critical:       %s\n",(new_service->notify_on_critical==1)?"yes":"no");
	printf("\tNotify On Recovery:       %s\n",(new_service->notify_on_recovery==1)?"yes":"no");
	printf("\tEvent Handler:            %s\n",(new_service->event_handler==NULL)?"N/A":new_service->event_handler);
#endif

#ifdef DEBUG0
	printf("add_service() end\n");
#endif

	return new_service;
	}



/* adds a contact group to a service */
contactgroupsmember *add_contactgroup_to_service(service *svc,char *group_name){
	contactgroupsmember *new_contactgroupsmember=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_contactgroup_to_service() start\n");
#endif

	/* bail out if we weren't given the data we need */
	if(svc==NULL || (group_name==NULL || !strcmp(group_name,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Service or contactgroup name is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for the contactgroups member */
	if((new_contactgroupsmember=malloc(sizeof(contactgroupsmember)))==NULL)
		return NULL;

	/* initialize vars */
	new_contactgroupsmember->group_name=NULL;
	
	/* duplicate vars */
	if((new_contactgroupsmember->group_name=(char *)strdup(group_name))==NULL)
		result=ERROR;

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_contactgroupsmember);
		return NULL;
	        }

	/* add this contactgroup to the service */
	new_contactgroupsmember->next=svc->contact_groups;
	svc->contact_groups=new_contactgroupsmember;

#ifdef DEBUG0
	printf("add_contactgroup_to_service() end\n");
#endif

	return new_contactgroupsmember;
	}



/* adds a custom variable to a service */
customvariablesmember *add_custom_variable_to_service(service *svc, char *varname, char *varvalue){

	return add_custom_variable_to_object(&svc->custom_variables,varname,varvalue);
        }



/* add a new command to the list in memory */
command *add_command(char *name,char *value){
	command *new_command=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_command() start\n");
#endif

	/* make sure we have the data we need */
	if((name==NULL || !strcmp(name,"")) || (value==NULL || !strcmp(value,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Command name of command line is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* make sure there isn't a command by this name added already */
	if(find_command(name)!=NULL){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Command '%s' has already been defined\n",name);
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for the new command */
	if((new_command=(command *)malloc(sizeof(command)))==NULL)
		return NULL;

	/* initialize vars */
	new_command->name=NULL;
	new_command->command_line=NULL;
	new_command->next=NULL;
	new_command->nexthash=NULL;

	/* duplicate vars */
	if((new_command->name=(char *)strdup(name))==NULL)
		result=ERROR;
	if((new_command->command_line=(char *)strdup(value))==NULL)
		result=ERROR;

	/* add new command to command chained hash list */
	if(result==OK){
		if(!add_command_to_hashlist(new_command))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_command->command_line);
		my_free((void **)&new_command->name);
		my_free((void **)&new_command);
		return NULL;
	        }

#ifdef NSCORE
	/* commands are sorted alphabetically for daemon, so add new items to tail of list */
	if(command_list==NULL){
		command_list=new_command;
		command_list_tail=command_list;
		}
	else {
		command_list_tail->next=new_command;
		command_list_tail=new_command;
		}
#else
	/* commands are sorted in reverse for CGIs, so add new items to head of list */
	new_command->next=command_list;
	command_list=new_command;
#endif		

#ifdef DEBUG1
	printf("\tName:         %s\n",new_command->name);
	printf("\tCommand Line: %s\n",new_command->command_line);
#endif
#ifdef DEBUG0
	printf("add_command() end\n");
#endif

	return new_command;
        }



/* add a new service escalation to the list in memory */
serviceescalation *add_serviceescalation(char *host_name,char *description,int first_notification,int last_notification, int notification_interval, char *escalation_period, int escalate_on_warning, int escalate_on_unknown, int escalate_on_critical, int escalate_on_recovery){
	serviceescalation *new_serviceescalation=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_serviceescalation() start\n");
#endif

	/* make sure we have the data we need */
	if((host_name==NULL || !strcmp(host_name,"")) || (description==NULL || !strcmp(description,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Service escalation host name or description is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for a new service escalation entry */
	if((new_serviceescalation=malloc(sizeof(serviceescalation)))==NULL)
		return NULL;

	/* initialize vars */
	new_serviceescalation->host_name=NULL;
	new_serviceescalation->description=NULL;
	new_serviceescalation->escalation_period=NULL;
	new_serviceescalation->contact_groups=NULL;
	new_serviceescalation->next=NULL;
	new_serviceescalation->nexthash=NULL;

	/* duplicate vars */
	if((new_serviceescalation->host_name=(char *)strdup(host_name))==NULL)
		result=ERROR;
	if((new_serviceescalation->description=(char *)strdup(description))==NULL)
		result=ERROR;
	if(escalation_period){
		if((new_serviceescalation->escalation_period=(char *)strdup(escalation_period))==NULL)
			result=ERROR;
	        }

	new_serviceescalation->first_notification=first_notification;
	new_serviceescalation->last_notification=last_notification;
	new_serviceescalation->notification_interval=(notification_interval<=0)?0:notification_interval;
	new_serviceescalation->escalate_on_recovery=(escalate_on_recovery>0)?TRUE:FALSE;
	new_serviceescalation->escalate_on_warning=(escalate_on_warning>0)?TRUE:FALSE;
	new_serviceescalation->escalate_on_unknown=(escalate_on_unknown>0)?TRUE:FALSE;
	new_serviceescalation->escalate_on_critical=(escalate_on_critical>0)?TRUE:FALSE;

	/* add new serviceescalation to serviceescalation chained hash list */
	if(result==OK){
		if(!add_serviceescalation_to_hashlist(new_serviceescalation))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_serviceescalation->host_name);
		my_free((void **)&new_serviceescalation->description);
		my_free((void **)&new_serviceescalation->escalation_period);
		my_free((void **)&new_serviceescalation);
		return NULL;
	        }

#ifdef NSCORE
	/* service escalations are sorted alphabetically for daemon, so add new items to tail of list */
	if(serviceescalation_list==NULL){
		serviceescalation_list=new_serviceescalation;
		serviceescalation_list_tail=serviceescalation_list;
		}
	else{
		serviceescalation_list_tail->next=new_serviceescalation;
		serviceescalation_list_tail=new_serviceescalation;
		}
#else
	/* service escalations are sorted in reverse for CGIs, so add new items to head of list */
	new_serviceescalation->next=serviceescalation_list;
	serviceescalation_list=new_serviceescalation;
#endif		

#ifdef DEBUG1
	printf("\tHost name:             %s\n",new_serviceescalation->host_name);
	printf("\tSvc description:       %s\n",new_serviceescalation->description);
	printf("\tFirst notification:    %d\n",new_serviceescalation->first_notification);
	printf("\tLast notification:     %d\n",new_serviceescalation->last_notification);
	printf("\tNotification Interval: %d\n",new_serviceescalation->notification_interval);
#endif

#ifdef DEBUG0
	printf("add_serviceescalation() end\n");
#endif

	return new_serviceescalation;
	}



/* adds a contact group to a service escalation */
contactgroupsmember *add_contactgroup_to_serviceescalation(serviceescalation *se,char *group_name){
	contactgroupsmember *new_contactgroupsmember=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_contactgroup_to_serviceescalation() start\n");
#endif

	/* bail out if we weren't given the data we need */
	if(se==NULL || (group_name==NULL || !strcmp(group_name,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Service escalation or contactgroup name is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for the contactgroups member */
	if((new_contactgroupsmember=(contactgroupsmember *)malloc(sizeof(contactgroupsmember)))==NULL)
		return NULL;

	/* initialize vars */
	new_contactgroupsmember->group_name=NULL;

	/* duplicate vars */
	if((new_contactgroupsmember->group_name=(char *)strdup(group_name))==NULL)
		result=ERROR;

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_contactgroupsmember->group_name);
		my_free((void **)&new_contactgroupsmember);
		return NULL;
	        }

	/* add this contactgroup to the service escalation */
	new_contactgroupsmember->next=se->contact_groups;
	se->contact_groups=new_contactgroupsmember;

#ifdef DEBUG0
	printf("add_contactgroup_to_serviceescalation() end\n");
#endif

	return new_contactgroupsmember;
	}



/* adds a service dependency definition */
servicedependency *add_service_dependency(char *dependent_host_name, char *dependent_service_description, char *host_name, char *service_description, int dependency_type, int inherits_parent, int fail_on_ok, int fail_on_warning, int fail_on_unknown, int fail_on_critical, int fail_on_pending){
	servicedependency *new_servicedependency=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_service_dependency() start\n");
#endif

	/* make sure we have what we need */
	if((dependent_host_name==NULL || !strcmp(dependent_host_name,"")) || (dependent_service_description==NULL || !strcmp(dependent_service_description,"")) || (host_name==NULL || !strcmp(host_name,"")) || (service_description==NULL || !strcmp(service_description,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: NULL service description/host name in service dependency definition\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for a new service dependency entry */
	if((new_servicedependency=(servicedependency *)malloc(sizeof(servicedependency)))==NULL)
		return NULL;

	/* initialize vars */
	new_servicedependency->dependent_host_name=NULL;
	new_servicedependency->dependent_service_description=NULL;
	new_servicedependency->host_name=NULL;
	new_servicedependency->service_description=NULL;
	new_servicedependency->next=NULL;
	new_servicedependency->nexthash=NULL;

	/* duplicate vars */
	if((new_servicedependency->dependent_host_name=(char *)strdup(dependent_host_name))==NULL)
		result=ERROR;
	if((new_servicedependency->dependent_service_description=(char *)strdup(dependent_service_description))==NULL)
		result=ERROR;
	if((new_servicedependency->host_name=(char *)strdup(host_name))==NULL)
		result=ERROR;
	if((new_servicedependency->service_description=(char *)strdup(service_description))==NULL)
		result=ERROR;

	new_servicedependency->dependency_type=(dependency_type==EXECUTION_DEPENDENCY)?EXECUTION_DEPENDENCY:NOTIFICATION_DEPENDENCY;
	new_servicedependency->inherits_parent=(inherits_parent>0)?TRUE:FALSE;
	new_servicedependency->fail_on_ok=(fail_on_ok==1)?TRUE:FALSE;
	new_servicedependency->fail_on_warning=(fail_on_warning==1)?TRUE:FALSE;
	new_servicedependency->fail_on_unknown=(fail_on_unknown==1)?TRUE:FALSE;
	new_servicedependency->fail_on_critical=(fail_on_critical==1)?TRUE:FALSE;
	new_servicedependency->fail_on_pending=(fail_on_pending==1)?TRUE:FALSE;
#ifdef NSCORE
	new_servicedependency->circular_path_checked=FALSE;
	new_servicedependency->contains_circular_path=FALSE;
#endif

	/* add new servicedependency to servicedependency chained hash list */
	if(result==OK){
		if(!add_servicedependency_to_hashlist(new_servicedependency))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_servicedependency->host_name);
		my_free((void **)&new_servicedependency->service_description);
		my_free((void **)&new_servicedependency->dependent_host_name);
		my_free((void **)&new_servicedependency->dependent_service_description);
		my_free((void **)&new_servicedependency);
		return NULL;
	        }

#ifdef NSCORE
	/* service dependencies are sorted alphabetically for daemon, so add new items to tail of list */
	if(servicedependency_list==NULL){
		servicedependency_list=new_servicedependency;
		servicedependency_list_tail=servicedependency_list;
		}
	else{
		servicedependency_list_tail->next=new_servicedependency;
		servicedependency_list_tail=new_servicedependency;
		}
#else
	/* service dependencies are sorted in reverse for CGIs, so add new items to head of list */
	new_servicedependency->next=servicedependency_list;
	servicedependency_list=new_servicedependency;
#endif		

#ifdef DEBUG0
	printf("add_service_dependency() end\n");
#endif

	return new_servicedependency;
        }


/* adds a host dependency definition */
hostdependency *add_host_dependency(char *dependent_host_name, char *host_name, int dependency_type, int inherits_parent, int fail_on_up, int fail_on_down, int fail_on_unreachable, int fail_on_pending){
	hostdependency *new_hostdependency=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_host_dependency() start\n");
#endif

	/* make sure we have what we need */
	if((dependent_host_name==NULL || !strcmp(dependent_host_name,"")) || (host_name==NULL || !strcmp(host_name,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: NULL host name in host dependency definition\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for a new host dependency entry */
	if((new_hostdependency=(hostdependency *)malloc(sizeof(hostdependency)))==NULL)
		return NULL;

	/* initialize vars */
	new_hostdependency->dependent_host_name=NULL;
	new_hostdependency->host_name=NULL;
	new_hostdependency->next=NULL;
	new_hostdependency->nexthash=NULL;

	/* duplicate vars */
	if((new_hostdependency->dependent_host_name=(char *)strdup(dependent_host_name))==NULL)
		result=ERROR;
	if((new_hostdependency->host_name=(char *)strdup(host_name))==NULL)
		result=ERROR;

	new_hostdependency->dependency_type=(dependency_type==EXECUTION_DEPENDENCY)?EXECUTION_DEPENDENCY:NOTIFICATION_DEPENDENCY;
	new_hostdependency->inherits_parent=(inherits_parent>0)?TRUE:FALSE;
	new_hostdependency->fail_on_up=(fail_on_up==1)?TRUE:FALSE;
	new_hostdependency->fail_on_down=(fail_on_down==1)?TRUE:FALSE;
	new_hostdependency->fail_on_unreachable=(fail_on_unreachable==1)?TRUE:FALSE;
	new_hostdependency->fail_on_pending=(fail_on_pending==1)?TRUE:FALSE;
#ifdef NSCORE
	new_hostdependency->circular_path_checked=FALSE;
	new_hostdependency->contains_circular_path=FALSE;
#endif

	/* add new hostdependency to hostdependency chained hash list */
	if(result==OK){
		if(!add_hostdependency_to_hashlist(new_hostdependency))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_hostdependency->host_name);
		my_free((void **)&new_hostdependency->dependent_host_name);
		my_free((void **)&new_hostdependency);
		return NULL;
	        }

#ifdef NSCORE
	/* host dependencies are sorted alphabetically for daemon, so add new items to tail of list */
	if(hostdependency_list==NULL){
		hostdependency_list=new_hostdependency;
		hostdependency_list_tail=hostdependency_list;
		}
	else {
		hostdependency_list_tail->next=new_hostdependency;
		hostdependency_list_tail=new_hostdependency;
		}
#else
	/* host dependencies are sorted in reverse for CGIs, so add new items to head of list */
	new_hostdependency->next=hostdependency_list;
	hostdependency_list=new_hostdependency;
#endif		

#ifdef DEBUG0
	printf("add_host_dependency() end\n");
#endif

	return new_hostdependency;
        }



/* add a new host escalation to the list in memory */
hostescalation *add_hostescalation(char *host_name,int first_notification,int last_notification, int notification_interval, char *escalation_period, int escalate_on_down, int escalate_on_unreachable, int escalate_on_recovery){
	hostescalation *new_hostescalation=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_hostescalation() start\n");
#endif

	/* make sure we have the data we need */
	if(host_name==NULL || !strcmp(host_name,"")){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Host escalation host name is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for a new host escalation entry */
	if((new_hostescalation=malloc(sizeof(hostescalation)))==NULL)
		return NULL;

	/* initialize vars */
	new_hostescalation->host_name=NULL;
	new_hostescalation->escalation_period=NULL;
	new_hostescalation->contact_groups=NULL;
	new_hostescalation->next=NULL;
	new_hostescalation->nexthash=NULL;

	/* duplicate vars */
	if((new_hostescalation->host_name=(char *)strdup(host_name))==NULL)
		result=ERROR;
	if(escalation_period){
		if((new_hostescalation->escalation_period=(char *)strdup(escalation_period))==NULL)
			result=ERROR;
	        }

	new_hostescalation->first_notification=first_notification;
	new_hostescalation->last_notification=last_notification;
	new_hostescalation->notification_interval=(notification_interval<=0)?0:notification_interval;
	new_hostescalation->escalate_on_recovery=(escalate_on_recovery>0)?TRUE:FALSE;
	new_hostescalation->escalate_on_down=(escalate_on_down>0)?TRUE:FALSE;
	new_hostescalation->escalate_on_unreachable=(escalate_on_unreachable>0)?TRUE:FALSE;

	/* add new hostescalation to hostescalation chained hash list */
	if(result==OK){
		if(!add_hostescalation_to_hashlist(new_hostescalation))
			result=ERROR;
	        }

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_hostescalation->host_name);
		my_free((void **)&new_hostescalation->escalation_period);
		my_free((void **)&new_hostescalation);
		return NULL;
	        }

#ifdef NSCORE
	/* host escalations are sorted alphabetically for daemon, so add new items to tail of list */
	if(hostescalation_list==NULL){
		hostescalation_list=new_hostescalation;
		hostescalation_list_tail=hostescalation_list;
		}
	else{
		hostescalation_list_tail->next=new_hostescalation;
		hostescalation_list_tail=new_hostescalation;
		}
#else
	/* host escalations are sorted in reverse for CGIs, so add new items to head of list */
	new_hostescalation->next=hostescalation_list;
	hostescalation_list=new_hostescalation;
#endif		

#ifdef DEBUG1
	printf("\tHost name:             %s\n",new_hostescalation->host_name);
	printf("\tFirst notification:    %d\n",new_hostescalation->first_notification);
	printf("\tLast notification:     %d\n",new_hostescalation->last_notification);
	printf("\tNotification Interval: %d\n",new_hostescalation->notification_interval);
#endif

#ifdef DEBUG0
	printf("add_hostescalation() end\n");
#endif

	return new_hostescalation;
	}



/* adds a contact group to a host escalation */
contactgroupsmember *add_contactgroup_to_hostescalation(hostescalation *he,char *group_name){
	contactgroupsmember *new_contactgroupsmember=NULL;
	int result=OK;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUG0
	printf("add_contactgroup_to_hostescalation() start\n");
#endif

	/* bail out if we weren't given the data we need */
	if(he==NULL || (group_name==NULL || !strcmp(group_name,""))){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Host escalation or contactgroup name is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for the contactgroups member */
	if((new_contactgroupsmember=(contactgroupsmember *)malloc(sizeof(contactgroupsmember)))==NULL)
		return NULL;

	/* initialize vars */
	new_contactgroupsmember->group_name=NULL;

	/* duplicate vars */
	if((new_contactgroupsmember->group_name=(char *)strdup(group_name))==NULL)
		result=ERROR;

	/* handle errors */
	if(result==ERROR){
		my_free((void **)&new_contactgroupsmember->group_name);
		my_free((void **)&new_contactgroupsmember);
		return NULL;
	        }

	/* add this contactgroup to the host escalation */
	new_contactgroupsmember->next=he->contact_groups;
	he->contact_groups=new_contactgroupsmember;

#ifdef DEBUG0
	printf("add_contactgroup_to_hostescalation() end\n");
#endif

	return new_contactgroupsmember;
	}




/* adds an extended host info structure to the list in memory */
hostextinfo * add_hostextinfo(char *host_name, char *notes, char *notes_url, char *action_url, char *icon_image, char *vrml_image, char *statusmap_image, char *icon_image_alt, int x_2d, int y_2d, double x_3d, double y_3d, double z_3d, int have_2d_coords, int have_3d_coords){
	hostextinfo *new_hostextinfo;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_hostextinfo() start\n");
#endif

	/* make sure we have what we need */
	if(host_name==NULL || !strcmp(host_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host name is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for a new data structure */
	new_hostextinfo=(hostextinfo *)malloc(sizeof(hostextinfo));
	if(new_hostextinfo==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' extended info.\n",host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
				
	new_hostextinfo->host_name=(char *)strdup(host_name);
	if(new_hostextinfo->host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' extended info.\n",host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		my_free((void **)&new_hostextinfo);
		return NULL;
	        }

	if(notes==NULL || !strcmp(notes,""))
		new_hostextinfo->notes=NULL;
	else{
		new_hostextinfo->notes=(char *)strdup(notes);
		if(new_hostextinfo->notes==NULL){
			my_free((void **)&new_hostextinfo->host_name);
			my_free((void **)&new_hostextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' extended info.\n",host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
		        }
	        }

	if(notes_url==NULL || !strcmp(notes_url,""))
		new_hostextinfo->notes_url=NULL;
	else{
		new_hostextinfo->notes_url=(char *)strdup(notes_url);
		if(new_hostextinfo->notes_url==NULL){
			my_free((void **)&new_hostextinfo->notes);
			my_free((void **)&new_hostextinfo->host_name);
			my_free((void **)&new_hostextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' extended info.\n",host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
		        }
	        }

	if(action_url==NULL || !strcmp(action_url,""))
		new_hostextinfo->action_url=NULL;
	else{
		new_hostextinfo->action_url=(char *)strdup(action_url);
		if(new_hostextinfo->action_url==NULL){
			my_free((void **)&new_hostextinfo->notes_url);
			my_free((void **)&new_hostextinfo->notes);
			my_free((void **)&new_hostextinfo->host_name);
			my_free((void **)&new_hostextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' extended info.\n",host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
		        }
	        }

	if(icon_image==NULL || !strcmp(icon_image,""))
		new_hostextinfo->icon_image=NULL;
	else{
		new_hostextinfo->icon_image=(char *)strdup(icon_image);
		if(new_hostextinfo->icon_image==NULL){
			my_free((void **)&new_hostextinfo->action_url);
			my_free((void **)&new_hostextinfo->notes_url);
			my_free((void **)&new_hostextinfo->notes);
			my_free((void **)&new_hostextinfo->host_name);
			my_free((void **)&new_hostextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' extended info.\n",host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
	                }
	        }

	if(vrml_image==NULL || !strcmp(vrml_image,""))
		new_hostextinfo->vrml_image=NULL;
	else{
		new_hostextinfo->vrml_image=(char *)strdup(vrml_image);
		if(new_hostextinfo->vrml_image==NULL){
			my_free((void **)&new_hostextinfo->icon_image);
			my_free((void **)&new_hostextinfo->action_url);
			my_free((void **)&new_hostextinfo->notes_url);
			my_free((void **)&new_hostextinfo->notes);
			my_free((void **)&new_hostextinfo->host_name);
			my_free((void **)&new_hostextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' extended info.\n",host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
	                }
	        }


	if(statusmap_image==NULL || !strcmp(statusmap_image,""))
		new_hostextinfo->statusmap_image=NULL;
	else{
		new_hostextinfo->statusmap_image=(char *)strdup(statusmap_image);
		if(new_hostextinfo->statusmap_image==NULL){
			my_free((void **)&new_hostextinfo->vrml_image);
			my_free((void **)&new_hostextinfo->icon_image);
			my_free((void **)&new_hostextinfo->action_url);
			my_free((void **)&new_hostextinfo->notes_url);
			my_free((void **)&new_hostextinfo->notes);
			my_free((void **)&new_hostextinfo->host_name);
			my_free((void **)&new_hostextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' extended info.\n",host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
		        }
	        }


	if(icon_image_alt==NULL || !strcmp(icon_image_alt,""))
		new_hostextinfo->icon_image_alt=NULL;
	else{
		new_hostextinfo->icon_image_alt=(char *)strdup(icon_image_alt);
		if(new_hostextinfo->icon_image_alt==NULL){
			my_free((void **)&new_hostextinfo->statusmap_image);
			my_free((void **)&new_hostextinfo->vrml_image);
			my_free((void **)&new_hostextinfo->icon_image);
			my_free((void **)&new_hostextinfo->action_url);
			my_free((void **)&new_hostextinfo->notes_url);
			my_free((void **)&new_hostextinfo->notes);
			my_free((void **)&new_hostextinfo->host_name);
			my_free((void **)&new_hostextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' extended info.\n",host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
		        }
	        }

	/* 2-D coordinates */
	new_hostextinfo->x_2d=x_2d;
	new_hostextinfo->y_2d=y_2d;
	new_hostextinfo->have_2d_coords=have_2d_coords;

	/* 3-D coordinates */
	new_hostextinfo->x_3d=x_3d;
	new_hostextinfo->y_3d=y_3d;
	new_hostextinfo->z_3d=z_3d;
	new_hostextinfo->have_3d_coords=have_3d_coords;

	/* default is to not draw this item */
	new_hostextinfo->should_be_drawn=FALSE;

	new_hostextinfo->next=NULL;
	new_hostextinfo->nexthash=NULL;

	/* add new hostextinfo to hostextinfo chained hash list */
	if(!add_hostextinfo_to_hashlist(new_hostextinfo)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for hostextinfo list to add extended info for host '%s'.\n",host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		my_free((void **)&new_hostextinfo->statusmap_image);
		my_free((void **)&new_hostextinfo->vrml_image);
		my_free((void **)&new_hostextinfo->icon_image);
		my_free((void **)&new_hostextinfo->icon_image_alt);
		my_free((void **)&new_hostextinfo->action_url);
		my_free((void **)&new_hostextinfo->notes_url);
		my_free((void **)&new_hostextinfo->notes);
		my_free((void **)&new_hostextinfo->host_name);
		my_free((void **)&new_hostextinfo);
		return NULL;
	        }

#ifdef NSCORE
	/* hostextinfo entries are sorted alphabetically for daemon, so add new items to tail of list */
	if(hostextinfo_list==NULL){
		hostextinfo_list=new_hostextinfo;
		hostextinfo_list_tail=hostextinfo_list;
		}
	else{
		hostextinfo_list_tail->next=new_hostextinfo;
		hostextinfo_list_tail=new_hostextinfo;
		}
#else
	/* hostextinfo entries are sorted in reverse for CGIs, so add new items to head of list */
	new_hostextinfo->next=hostextinfo_list;
	hostextinfo_list=new_hostextinfo;
#endif		

#ifdef DEBUG0
	printf("add_hostextinfo() end\n");
#endif
	return new_hostextinfo;
        }
	


/* adds a custom variable to an object */
customvariablesmember *add_custom_variable_to_object(customvariablesmember **object_ptr, char *varname, char *varvalue){
	customvariablesmember *new_customvariablesmember=NULL;
#ifdef NSCORE
	char *temp_buffer=NULL;
#endif

#ifdef DEBUD0
	printf("add_custom_variable_to_object() start\n");
#endif

	/* make sure we have the data we need */
	if(object_ptr==NULL){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Custom variable object is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	if(varname==NULL || !strcmp(varname,"")){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Custom variable name is NULL\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }

	/* allocate memory for a new member */
	if((new_customvariablesmember=malloc(sizeof(customvariablesmember)))==NULL){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Could not allocate memory for custom variable\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		return NULL;
	        }
	if((new_customvariablesmember->variable_name=(char *)strdup(varname))==NULL){
#ifdef NSCORE
		asprintf(&temp_buffer,"Error: Could not allocate memory for custom variable name\n");
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		my_free((void **)&temp_buffer);
#endif
		my_free((void **)&new_customvariablesmember);
		return NULL;
	        }
	if(varvalue){
		if((new_customvariablesmember->variable_value=(char *)strdup(varvalue))==NULL){
#ifdef NSCORE
			asprintf(&temp_buffer,"Error: Could not allocate memory for custom variable value\n");
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
			my_free((void **)&temp_buffer);
#endif
			my_free((void **)&new_customvariablesmember->variable_name);
			my_free((void **)&new_customvariablesmember);
			return NULL;
	                }
	        }
	else
		new_customvariablesmember->variable_value=NULL;

	/* set initial values */
	new_customvariablesmember->has_been_modified=FALSE;

	/* add the new member to the head of the member list */
	new_customvariablesmember->next=*object_ptr;
	*object_ptr=new_customvariablesmember;

#ifdef DEBUD0
	printf("add_custom_variable_to_object() end\n");
#endif

	return new_customvariablesmember;
        }




/******************************************************************/
/******************** OBJECT SEARCH FUNCTIONS *********************/
/******************************************************************/

/* given a timeperiod name and a starting point, find a timeperiod from the list in memory */
timeperiod * find_timeperiod(char *name){
	timeperiod *temp_timeperiod=NULL;

#ifdef DEBUG0
	printf("find_timeperiod() start\n");
#endif

	if(name==NULL || timeperiod_hashlist==NULL)
		return NULL;

	for(temp_timeperiod=timeperiod_hashlist[hashfunc(name,NULL,TIMEPERIOD_HASHSLOTS)];temp_timeperiod && compare_hashdata(temp_timeperiod->name,NULL,name,NULL)<0;temp_timeperiod=temp_timeperiod->nexthash);

	if(temp_timeperiod && (compare_hashdata(temp_timeperiod->name,NULL,name,NULL)==0))
		return temp_timeperiod;

#ifdef DEBUG0
	printf("find_timeperiod() end\n");
#endif

	/* we couldn't find a matching timeperiod */
	return NULL;
	}


/* given a host name, find it in the list in memory */
host * find_host(char *name){
	host *temp_host=NULL;

#ifdef DEBUG0
	printf("find_host() start\n");
#endif

	if(name==NULL || host_hashlist==NULL)
		return NULL;

	for(temp_host=host_hashlist[hashfunc(name,NULL,HOST_HASHSLOTS)];temp_host && compare_hashdata(temp_host->name,NULL,name,NULL)<0;temp_host=temp_host->nexthash);

	if(temp_host && (compare_hashdata(temp_host->name,NULL,name,NULL)==0))
		return temp_host;

#ifdef DEBUG0
	printf("find_host() end\n");
#endif

	/* Couldn't find matching host */
	return NULL;
        }


/* find a hostgroup from the list in memory */
hostgroup * find_hostgroup(char *name){
	hostgroup *temp_hostgroup=NULL;

#ifdef DEBUG0
	printf("find_hostgroup() start\n");
#endif

	if(name==NULL || hostgroup_hashlist==NULL)
		return NULL;

	for(temp_hostgroup=hostgroup_hashlist[hashfunc(name,NULL,HOSTGROUP_HASHSLOTS)];temp_hostgroup && compare_hashdata(temp_hostgroup->group_name,NULL,name,NULL)<0;temp_hostgroup=temp_hostgroup->nexthash);

	if(temp_hostgroup && (compare_hashdata(temp_hostgroup->group_name,NULL,name,NULL)==0))
		return temp_hostgroup;

#ifdef DEBUG0
	printf("find_hostgroup() end\n");
#endif

	/* we couldn't find a matching hostgroup */
	return NULL;
	}


/* find a servicegroup from the list in memory */
servicegroup * find_servicegroup(char *name){
	servicegroup *temp_servicegroup=NULL;

#ifdef DEBUG0
	printf("find_servicegroup() start\n");
#endif

	if(name==NULL || servicegroup_hashlist==NULL)
		return NULL;

	for(temp_servicegroup=servicegroup_hashlist[hashfunc(name,NULL,SERVICEGROUP_HASHSLOTS)];temp_servicegroup && compare_hashdata(temp_servicegroup->group_name,NULL,name,NULL)<0;temp_servicegroup=temp_servicegroup->nexthash);

	if(temp_servicegroup && (compare_hashdata(temp_servicegroup->group_name,NULL,name,NULL)==0))
		return temp_servicegroup;


#ifdef DEBUG0
	printf("find_servicegroup() end\n");
#endif

	/* we couldn't find a matching servicegroup */
	return NULL;
	}


/* find a contact from the list in memory */
contact * find_contact(char *name){
	contact *temp_contact=NULL;

#ifdef DEBUG0
	printf("find_contact() start\n");
#endif

	if(name==NULL || contact_hashlist==NULL)
		return NULL;

	for(temp_contact=contact_hashlist[hashfunc(name,NULL,CONTACT_HASHSLOTS)];temp_contact && compare_hashdata(temp_contact->name,NULL,name,NULL)<0;temp_contact=temp_contact->nexthash);

	if(temp_contact && (compare_hashdata(temp_contact->name,NULL,name,NULL)==0))
		return temp_contact;

#ifdef DEBUG0
	printf("find_contact() end\n");
#endif

	/* we couldn't find a matching contact */
	return NULL;
	}


/* find a contact group from the list in memory */
contactgroup * find_contactgroup(char *name){
	contactgroup *temp_contactgroup=NULL;

#ifdef DEBUG0
	printf("find_contactgroup() start\n");
#endif

	if(name==NULL || contactgroup_hashlist==NULL)
		return NULL;

	for(temp_contactgroup=contactgroup_hashlist[hashfunc(name,NULL,CONTACTGROUP_HASHSLOTS)];temp_contactgroup && compare_hashdata(temp_contactgroup->group_name,NULL,name,NULL)<0;temp_contactgroup=temp_contactgroup->nexthash);

	if(temp_contactgroup && (compare_hashdata(temp_contactgroup->group_name,NULL,name,NULL)==0))
		return temp_contactgroup;

#ifdef DEBUG0
	printf("find_contactgroup() end\n");
#endif

	/* we couldn't find a matching contactgroup */
	return NULL;
	}


/* find the corresponding member of a contact group */
contactgroupmember * find_contactgroupmember(char *name,contactgroup *grp){
	contactgroupmember *temp_member=NULL;

#ifdef DEBUG0
	printf("find_contactgroupmember() start\n");
#endif

	if(name==NULL || grp==NULL)
		return NULL;

	temp_member=grp->members;
	while(temp_member!=NULL){

		/* we found a match */
		if(!strcmp(temp_member->contact_name,name))
			return temp_member;

		temp_member=temp_member->next;
	        }

#ifdef DEBUG0
	printf("find_contactgroupmember() end\n");
#endif

	/* we couldn't find a matching member */
	return NULL;
        }


/* given a command name, find a command from the list in memory */
command * find_command(char *name){
	command *temp_command=NULL;

#ifdef DEBUG0
	printf("find_command() start\n");
#endif

	if(name==NULL || command_hashlist==NULL)
		return NULL;

	for(temp_command=command_hashlist[hashfunc(name,NULL,COMMAND_HASHSLOTS)];temp_command && compare_hashdata(temp_command->name,NULL,name,NULL)<0;temp_command=temp_command->nexthash);

	if(temp_command && (compare_hashdata(temp_command->name,NULL,name,NULL)==0))
		return temp_command;

#ifdef DEBUG0
	printf("find_command() end\n");
#endif

	/* we couldn't find a matching command */
	return NULL;
        }


/* given a host/service name, find the service in the list in memory */
service * find_service(char *host_name,char *svc_desc){
	service *temp_service=NULL;

#ifdef DEBUG0
	printf("find_service() start\n");
#endif

	if(host_name==NULL || svc_desc==NULL || service_hashlist==NULL)
		return NULL;

	for(temp_service=service_hashlist[hashfunc(host_name,svc_desc,SERVICE_HASHSLOTS)];temp_service && compare_hashdata(temp_service->host_name,temp_service->description,host_name,svc_desc)<0;temp_service=temp_service->nexthash);

	if(temp_service && (compare_hashdata(temp_service->host_name,temp_service->description,host_name,svc_desc)==0))
		return temp_service;

#ifdef DEBUG0
	printf("find_service() end\n");
#endif

	/* we couldn't find a matching service */
	return NULL;
        }



/* find the extended information for a given host */
hostextinfo * find_hostextinfo(char *host_name){
	hostextinfo *temp_hostextinfo=NULL;

#ifdef DEBUG0
	printf("find_hostextinfo() start\n");
#endif

	if(host_name==NULL || hostextinfo_hashlist==NULL)
		return NULL;

	for(temp_hostextinfo=hostextinfo_hashlist[hashfunc(host_name,NULL,HOSTEXTINFO_HASHSLOTS)];temp_hostextinfo && compare_hashdata(temp_hostextinfo->host_name,NULL,host_name,NULL)<0;temp_hostextinfo=temp_hostextinfo->nexthash);

	if(temp_hostextinfo && (compare_hashdata(temp_hostextinfo->host_name,NULL,host_name,NULL)==0))
		return temp_hostextinfo;

#ifdef DEBUG0
	printf("find_hostextinfo() end\n");
#endif

	/* we couldn't find a matching extended host info object */
	return NULL;
        }




/******************************************************************/
/******************* OBJECT TRAVERSAL FUNCTIONS *******************/
/******************************************************************/

hostescalation *get_first_hostescalation_by_host(char *host_name){

	return get_next_hostescalation_by_host(host_name,NULL);
        }


hostescalation *get_next_hostescalation_by_host(char *host_name, hostescalation *start){
	hostescalation *temp_hostescalation=NULL;

	if(host_name==NULL || hostescalation_hashlist==NULL)
		return NULL;

	if(start==NULL)
		temp_hostescalation=hostescalation_hashlist[hashfunc(host_name,NULL,HOSTESCALATION_HASHSLOTS)];
	else
		temp_hostescalation=start->nexthash;

	for(;temp_hostescalation && compare_hashdata(temp_hostescalation->host_name,NULL,host_name,NULL)<0;temp_hostescalation=temp_hostescalation->nexthash);

	if(temp_hostescalation && compare_hashdata(temp_hostescalation->host_name,NULL,host_name,NULL)==0)
		return temp_hostescalation;

	return NULL;
        }


serviceescalation *get_first_serviceescalation_by_service(char *host_name, char *svc_description){

	return get_next_serviceescalation_by_service(host_name,svc_description,NULL);
        }


serviceescalation *get_next_serviceescalation_by_service(char *host_name, char *svc_description, serviceescalation *start){
	serviceescalation *temp_serviceescalation=NULL;

	if(host_name==NULL || svc_description==NULL || serviceescalation_hashlist==NULL)
		return NULL;

	if(start==NULL)
		temp_serviceescalation=serviceescalation_hashlist[hashfunc(host_name,svc_description,SERVICEESCALATION_HASHSLOTS)];
	else
		temp_serviceescalation=start->nexthash;

	for(;temp_serviceescalation && compare_hashdata(temp_serviceescalation->host_name,temp_serviceescalation->description,host_name,svc_description)<0;temp_serviceescalation=temp_serviceescalation->nexthash);

	if(temp_serviceescalation && compare_hashdata(temp_serviceescalation->host_name,temp_serviceescalation->description,host_name,svc_description)==0)
		return temp_serviceescalation;

	return NULL;
        }


hostdependency *get_first_hostdependency_by_dependent_host(char *host_name){

	return get_next_hostdependency_by_dependent_host(host_name,NULL);
        }


hostdependency *get_next_hostdependency_by_dependent_host(char *host_name, hostdependency *start){
	hostdependency *temp_hostdependency=NULL;

	if(host_name==NULL || hostdependency_hashlist==NULL)
		return NULL;

	if(start==NULL)
		temp_hostdependency=hostdependency_hashlist[hashfunc(host_name,NULL,HOSTDEPENDENCY_HASHSLOTS)];
	else
		temp_hostdependency=start->nexthash;

	for(;temp_hostdependency && compare_hashdata(temp_hostdependency->dependent_host_name,NULL,host_name,NULL)<0;temp_hostdependency=temp_hostdependency->nexthash);

	if(temp_hostdependency && compare_hashdata(temp_hostdependency->dependent_host_name,NULL,host_name,NULL)==0)
		return temp_hostdependency;

	return NULL;
        }


servicedependency *get_first_servicedependency_by_dependent_service(char *host_name, char *svc_description){

	return get_next_servicedependency_by_dependent_service(host_name,svc_description,NULL);
        }


servicedependency *get_next_servicedependency_by_dependent_service(char *host_name, char *svc_description, servicedependency *start){
	servicedependency *temp_servicedependency=NULL;

	if(host_name==NULL || svc_description==NULL || servicedependency_hashlist==NULL)
		return NULL;

	if(start==NULL)
		temp_servicedependency=servicedependency_hashlist[hashfunc(host_name,svc_description,SERVICEDEPENDENCY_HASHSLOTS)];
	else
		temp_servicedependency=start->nexthash;

	for(;temp_servicedependency && compare_hashdata(temp_servicedependency->dependent_host_name,temp_servicedependency->dependent_service_description,host_name,svc_description)<0;temp_servicedependency=temp_servicedependency->nexthash);

	if(temp_servicedependency && compare_hashdata(temp_servicedependency->dependent_host_name,temp_servicedependency->dependent_service_description,host_name,svc_description)==0)
		return temp_servicedependency;

	return NULL;
        }



/******************************************************************/
/********************* OBJECT QUERY FUNCTIONS *********************/
/******************************************************************/

/* determines whether or not a specific host is an immediate child of another host */
int is_host_immediate_child_of_host(host *parent_host,host *child_host){
	hostsmember *temp_hostsmember=NULL;

	if(child_host==NULL)
		return FALSE;

	if(parent_host==NULL){
		if(child_host->parent_hosts==NULL)
			return TRUE;
	        }

	else{

		for(temp_hostsmember=child_host->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){
			if(!strcmp(temp_hostsmember->host_name,parent_host->name))
				return TRUE;
		        }
	        }

	return FALSE;
	}


/* determines whether or not a specific host is an immediate child (and the primary child) of another host */
int is_host_primary_immediate_child_of_host(host *parent_host, host *child_host){
	hostsmember *temp_hostsmember=NULL;

	if(is_host_immediate_child_of_host(parent_host,child_host)==FALSE)
		return FALSE;

	if(parent_host==NULL)
		return TRUE;

	if(child_host==NULL)
		return FALSE;

	if(child_host->parent_hosts==NULL)
		return TRUE;

	for(temp_hostsmember=child_host->parent_hosts;temp_hostsmember->next!=NULL;temp_hostsmember=temp_hostsmember->next);

	if(!strcmp(temp_hostsmember->host_name,parent_host->name))
		return TRUE;

	return FALSE;
        }



/* determines whether or not a specific host is an immediate parent of another host */
int is_host_immediate_parent_of_host(host *child_host,host *parent_host){

	if(is_host_immediate_child_of_host(parent_host,child_host)==TRUE)
		return TRUE;

	return FALSE;
	}


/* returns a count of the immediate children for a given host */
int number_of_immediate_child_hosts(host *hst){
	int children=0;
	host *temp_host=NULL;

	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(is_host_immediate_child_of_host(hst,temp_host)==TRUE)
			children++;
		}

	return children;
	}


/* returns a count of the total children for a given host */
int number_of_total_child_hosts(host *hst){
	int children=0;
	host *temp_host=NULL;

	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(is_host_immediate_child_of_host(hst,temp_host)==TRUE)
			children+=number_of_total_child_hosts(temp_host)+1;
		}

	return children;
	}


/* get the number of immediate parent hosts for a given host */
int number_of_immediate_parent_hosts(host *hst){
	int parents=0;
	host *temp_host=NULL;

	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(is_host_immediate_parent_of_host(hst,temp_host)==TRUE){
			parents++;
			break;
		        }
	        }

	return parents;
        }


/* get the total number of parent hosts for a given host */
int number_of_total_parent_hosts(host *hst){
	int parents=0;
	host *temp_host=NULL;

	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(is_host_immediate_parent_of_host(hst,temp_host)==TRUE){
			parents+=number_of_total_parent_hosts(temp_host)+1;
			break;
		        }
	        }

	return parents;
        }


/*  tests whether a host is a member of a particular hostgroup */
int is_host_member_of_hostgroup(hostgroup *group, host *hst){
	hostgroupmember *temp_hostgroupmember=NULL;

	if(group==NULL || hst==NULL)
		return FALSE;

	for(temp_hostgroupmember=group->members;temp_hostgroupmember!=NULL;temp_hostgroupmember=temp_hostgroupmember->next){
		if(!strcmp(temp_hostgroupmember->host_name,hst->name))
			return TRUE;
	        }

	return FALSE;
        }


/*  tests whether a host is a member of a particular servicegroup */
int is_host_member_of_servicegroup(servicegroup *group, host *hst){
	servicegroupmember *temp_servicegroupmember=NULL;

	if(group==NULL || hst==NULL)
		return FALSE;

	for(temp_servicegroupmember=group->members;temp_servicegroupmember!=NULL;temp_servicegroupmember=temp_servicegroupmember->next){
		if(!strcmp(temp_servicegroupmember->host_name,hst->name))
			return TRUE;
	        }

	return FALSE;
        }


/*  tests whether a service is a member of a particular servicegroup */
int is_service_member_of_servicegroup(servicegroup *group, service *svc){
	servicegroupmember *temp_servicegroupmember=NULL;

	if(group==NULL || svc==NULL)
		return FALSE;

	for(temp_servicegroupmember=group->members;temp_servicegroupmember!=NULL;temp_servicegroupmember=temp_servicegroupmember->next){
		if(!strcmp(temp_servicegroupmember->host_name,svc->host_name) && !strcmp(temp_servicegroupmember->service_description,svc->description))
			return TRUE;
	        }

	return FALSE;
        }


/*  tests whether a contact is a member of a particular contactgroup */
int is_contact_member_of_contactgroup(contactgroup *group, contact *cntct){
	contactgroupmember *temp_contactgroupmember=NULL;

	if(group==NULL || cntct==NULL)
		return FALSE;

	/* search all contacts in this contact group */
	for(temp_contactgroupmember=group->members;temp_contactgroupmember!=NULL;temp_contactgroupmember=temp_contactgroupmember->next){

		/* we found the contact! */
		if(!strcmp(temp_contactgroupmember->contact_name,cntct->name))
			return TRUE;
                }

	return FALSE;
        }


/*  tests whether a contact is a member of a particular hostgroup - used only by the CGIs */
int is_contact_for_hostgroup(hostgroup *group, contact *cntct){
	hostgroupmember *temp_hostgroupmember=NULL;
	host *temp_host=NULL;

	if(group==NULL || cntct==NULL)
		return FALSE;

	for(temp_hostgroupmember=group->members;temp_hostgroupmember!=NULL;temp_hostgroupmember=temp_hostgroupmember->next){
		temp_host=find_host(temp_hostgroupmember->host_name);
		if(temp_host==NULL)
			continue;
		if(is_contact_for_host(temp_host,cntct)==TRUE)
			return TRUE;
	        }

	return FALSE;
        }



/*  tests whether a contact is a member of a particular servicegroup - used only by the CGIs */
int is_contact_for_servicegroup(servicegroup *group, contact *cntct){
	servicegroupmember *temp_servicegroupmember=NULL;
	service *temp_service=NULL;

	if(group==NULL || cntct==NULL)
		return FALSE;

	for(temp_servicegroupmember=group->members;temp_servicegroupmember!=NULL;temp_servicegroupmember=temp_servicegroupmember->next){
		temp_service=find_service(temp_servicegroupmember->host_name,temp_servicegroupmember->service_description);
		if(temp_service==NULL)
			continue;
		if(is_contact_for_service(temp_service,cntct)==TRUE)
			return TRUE;
	        }

	return FALSE;
        }



/*  tests whether a contact is a contact for a particular host */
int is_contact_for_host(host *hst, contact *cntct){
	contactgroupsmember *temp_contactgroupsmember=NULL;
	contactgroup *temp_contactgroup=NULL;
	
	if(hst==NULL || cntct==NULL){
		return FALSE;
	        }

	/* search all contact groups of this host */
	for(temp_contactgroupsmember=hst->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){

		/* find the contact group */
		temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name);
		if(temp_contactgroup==NULL)
			continue;

		if(is_contact_member_of_contactgroup(temp_contactgroup,cntct)==TRUE)
			return TRUE;
	        }

	return FALSE;
        }



/* tests whether or not a contact is an escalated contact for a particular host */
int is_escalated_contact_for_host(host *hst, contact *cntct){
	contactgroupsmember *temp_contactgroupsmember=NULL;
	contactgroup *temp_contactgroup=NULL;
	hostescalation *temp_hostescalation=NULL;


	/* search all host escalations */
	for(temp_hostescalation=get_first_hostescalation_by_host(hst->name);temp_hostescalation!=NULL;temp_hostescalation=get_next_hostescalation_by_host(hst->name,temp_hostescalation)){

		/* search all the contact groups in this escalation... */
		for(temp_contactgroupsmember=temp_hostescalation->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){

			/* find the contact group */
			temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name);
			if(temp_contactgroup==NULL)
				continue;

			/* see if the contact is a member of this contact group */
			if(is_contact_member_of_contactgroup(temp_contactgroup,cntct)==TRUE)
				return TRUE;
		        }
	         }

	return FALSE;
        }


/*  tests whether a contact is a contact for a particular service */
int is_contact_for_service(service *svc, contact *cntct){
	contactgroupsmember *temp_contactgroupsmember=NULL;
	contactgroup *temp_contactgroup=NULL;

	if(svc==NULL || cntct==NULL)
		return FALSE;

	/* search all contact groups of this service */
	for(temp_contactgroupsmember=svc->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){

		/* find the contact group */
		temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name);
		if(temp_contactgroup==NULL)
			continue;

		if(is_contact_member_of_contactgroup(temp_contactgroup,cntct)==TRUE)
			return TRUE;
	        }

	return FALSE;
        }



/* tests whether or not a contact is an escalated contact for a particular service */
int is_escalated_contact_for_service(service *svc, contact *cntct){
	serviceescalation *temp_serviceescalation=NULL;
	contactgroupsmember *temp_contactgroupsmember=NULL;
	contactgroup *temp_contactgroup=NULL;

	/* search all the service escalations */
	for(temp_serviceescalation=get_first_serviceescalation_by_service(svc->host_name,svc->description);temp_serviceescalation!=NULL;temp_serviceescalation=get_next_serviceescalation_by_service(svc->host_name,svc->description,temp_serviceescalation)){

		/* search all the contact groups in this escalation... */
		for(temp_contactgroupsmember=temp_serviceescalation->contact_groups;temp_contactgroupsmember!=NULL;temp_contactgroupsmember=temp_contactgroupsmember->next){

			/* find the contact group */
			temp_contactgroup=find_contactgroup(temp_contactgroupsmember->group_name);
			if(temp_contactgroup==NULL)
				continue;

			/* see if the contact is a member of this contact group */
			if(is_contact_member_of_contactgroup(temp_contactgroup,cntct)==TRUE)
				return TRUE;
		        }
	        }

	return FALSE;
        }


#ifdef NSCORE

/* checks to see if there exists a circular parent/child path for a host */
int check_for_circular_path(host *root_hst, host *hst){
	host *temp_host=NULL;

	/* don't go into a loop, don't bother checking anymore if we know this host already has a loop */
	if(root_hst->contains_circular_path==TRUE)
		return TRUE;

	/* host has already been checked - there is a path somewhere, but it may not be for this particular host... */
	/* this should speed up detection for some loops */
	if(hst->circular_path_checked==TRUE)
		return FALSE;

	/* set the check flag so we don't get into an infinite loop */
	hst->circular_path_checked=TRUE;

	/* check this hosts' parents to see if a circular path exists */
	if(is_host_immediate_parent_of_host(root_hst,hst)==TRUE){
		root_hst->contains_circular_path=TRUE;
		hst->contains_circular_path=TRUE;
		return TRUE;
	        }

	/* check all immediate children for a circular path */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(is_host_immediate_child_of_host(hst,temp_host)==TRUE)
			if(check_for_circular_path(root_hst,temp_host)==TRUE)
				return TRUE;
	        }

	return FALSE;
        }


/* checks to see if there exists a circular dependency for a service */
int check_for_circular_servicedependency(servicedependency *root_dep, servicedependency *dep, int dependency_type){
	servicedependency *temp_sd=NULL;

	if(root_dep==NULL || dep==NULL)
		return FALSE;

	/* this is not the proper dependency type */
	if(root_dep->dependency_type!=dependency_type || dep->dependency_type!=dependency_type)
		return FALSE;

	/* don't go into a loop, don't bother checking anymore if we know this dependency already has a loop */
	if(root_dep->contains_circular_path==TRUE)
		return TRUE;

	/* dependency has already been checked - there is a path somewhere, but it may not be for this particular dep... */
	/* this should speed up detection for some loops */
	if(dep->circular_path_checked==TRUE)
		return FALSE;

	/* set the check flag so we don't get into an infinite loop */
	dep->circular_path_checked=TRUE;

	/* is this service dependent on the root service? */
	if(dep!=root_dep){
		if(!strcmp(root_dep->dependent_host_name,dep->host_name) && !strcmp(root_dep->dependent_service_description,dep->service_description)){
			root_dep->contains_circular_path=TRUE;
			dep->contains_circular_path=TRUE;
			return TRUE;
		        }
	        }

	/* notification dependencies are ok at this point as long as they don't inherit */
	if(dependency_type==NOTIFICATION_DEPENDENCY && dep->inherits_parent==FALSE)
		return FALSE;

	/* check all parent dependencies */
	for(temp_sd=servicedependency_list;temp_sd!=NULL;temp_sd=temp_sd->next){

		/* only check parent dependencies */
		if(strcmp(dep->host_name,temp_sd->dependent_host_name) || strcmp(dep->service_description,temp_sd->dependent_service_description))
			continue;

		if(check_for_circular_servicedependency(root_dep,temp_sd,dependency_type)==TRUE)
			return TRUE;
	        }

	return FALSE;
        }


/* checks to see if there exists a circular dependency for a host */
int check_for_circular_hostdependency(hostdependency *root_dep, hostdependency *dep, int dependency_type){
	hostdependency *temp_hd=NULL;

	if(root_dep==NULL || dep==NULL)
		return FALSE;

	/* this is not the proper dependency type */
	if(root_dep->dependency_type!=dependency_type || dep->dependency_type!=dependency_type)
		return FALSE;

	/* don't go into a loop, don't bother checking anymore if we know this dependency already has a loop */
	if(root_dep->contains_circular_path==TRUE)
		return TRUE;

	/* dependency has already been checked - there is a path somewhere, but it may not be for this particular dep... */
	/* this should speed up detection for some loops */
	if(dep->circular_path_checked==TRUE)
		return FALSE;

	/* set the check flag so we don't get into an infinite loop */
	dep->circular_path_checked=TRUE;

	/* is this host dependent on the root host? */
	if(dep!=root_dep){
		if(!strcmp(root_dep->dependent_host_name,dep->host_name)){
			root_dep->contains_circular_path=TRUE;
			dep->contains_circular_path=TRUE;
			return TRUE;
		        }
	        }

	/* notification dependencies are ok at this point as long as they don't inherit */
	if(dependency_type==NOTIFICATION_DEPENDENCY && dep->inherits_parent==FALSE)
		return FALSE;

	/* check all parent dependencies */
	for(temp_hd=hostdependency_list;temp_hd!=NULL;temp_hd=temp_hd->next){

		/* only check parent dependencies */
		if(strcmp(dep->host_name,temp_hd->dependent_host_name))
			continue;

		if(check_for_circular_hostdependency(root_dep,temp_hd,dependency_type)==TRUE)
			return TRUE;
	        }

	return FALSE;
        }

#endif




/******************************************************************/
/******************* OBJECT DELETION FUNCTIONS ********************/
/******************************************************************/


/* free all allocated memory for objects */
int free_object_data(void){
	timeperiod *this_timeperiod=NULL;
	timeperiod *next_timeperiod=NULL;
	timerange *this_timerange=NULL;
	timerange *next_timerange=NULL;
	host *this_host=NULL;
	host *next_host=NULL;
	hostsmember *this_hostsmember=NULL;
	hostsmember *next_hostsmember=NULL;
	hostgroup *this_hostgroup=NULL;
	hostgroup *next_hostgroup=NULL;
	hostgroupmember *this_hostgroupmember=NULL;
	hostgroupmember *next_hostgroupmember=NULL;
	servicegroup *this_servicegroup=NULL;
	servicegroup *next_servicegroup=NULL;
	servicegroupmember *this_servicegroupmember=NULL;
	servicegroupmember *next_servicegroupmember=NULL;
	contact	*this_contact=NULL;
	contact *next_contact=NULL;
	contactgroup *this_contactgroup=NULL;
	contactgroup *next_contactgroup=NULL;
	contactgroupmember *this_contactgroupmember=NULL;
	contactgroupmember *next_contactgroupmember=NULL;
	contactgroupsmember *this_contactgroupsmember=NULL;
	contactgroupsmember *next_contactgroupsmember=NULL;
	customvariablesmember *this_customvariablesmember=NULL;
	customvariablesmember *next_customvariablesmember=NULL;
	service *this_service=NULL;
	service *next_service=NULL;
	command *this_command=NULL;
	command *next_command=NULL;
	commandsmember *this_commandsmember=NULL;
	commandsmember *next_commandsmember=NULL;
	serviceescalation *this_serviceescalation=NULL;
	serviceescalation *next_serviceescalation=NULL;
	servicedependency *this_servicedependency=NULL;
	servicedependency *next_servicedependency=NULL;
	hostdependency *this_hostdependency=NULL;
	hostdependency *next_hostdependency=NULL;
	hostescalation *this_hostescalation=NULL;
	hostescalation *next_hostescalation=NULL;
	register int day=0;
	register int i=0;

#ifdef DEBUG0
	printf("free_object_data() start\n");
#endif

	/* free memory for the timeperiod list */
	this_timeperiod=timeperiod_list;
	while(this_timeperiod!=NULL){

		/* free the time ranges contained in this timeperiod */
		for(day=0;day<7;day++){

			for(this_timerange=this_timeperiod->days[day];this_timerange!=NULL;this_timerange=next_timerange){
				next_timerange=this_timerange->next;
				my_free((void **)&this_timerange);
			        }
		        }

		next_timeperiod=this_timeperiod->next;
		my_free((void **)&this_timeperiod->name);
		my_free((void **)&this_timeperiod->alias);
		my_free((void **)&this_timeperiod);
		this_timeperiod=next_timeperiod;
		}

	/* free hashlist and reset pointers */
	my_free((void **)&timeperiod_hashlist);
	timeperiod_hashlist=NULL;
	timeperiod_list=NULL;

#ifdef DEBUG1
	printf("\ttimeperiod_list freed\n");
#endif

	/* free memory for the host list */
	this_host=host_list;
	while(this_host!=NULL){

		next_host=this_host->next;

		/* free memory for parent hosts */
		this_hostsmember=this_host->parent_hosts;
		while(this_hostsmember!=NULL){
			next_hostsmember=this_hostsmember->next;
			my_free((void **)&this_hostsmember->host_name);
			my_free((void **)&this_hostsmember);
			this_hostsmember=next_hostsmember;
			}

		/* free memory for contact groups */
		this_contactgroupsmember=this_host->contact_groups;
		while(this_contactgroupsmember!=NULL){
			next_contactgroupsmember=this_contactgroupsmember->next;
			my_free((void **)&this_contactgroupsmember->group_name);
			my_free((void **)&this_contactgroupsmember);
			this_contactgroupsmember=next_contactgroupsmember;
			}

		/* free memory for custom variables */
		this_customvariablesmember=this_host->custom_variables;
		while(this_customvariablesmember!=NULL){
			next_customvariablesmember=this_customvariablesmember->next;
			my_free((void **)&this_customvariablesmember->variable_name);
			my_free((void **)&this_customvariablesmember->variable_value);
			my_free((void **)&this_customvariablesmember);
			this_customvariablesmember=next_customvariablesmember;
		        }

		my_free((void **)&this_host->name);
		my_free((void **)&this_host->display_name);
		my_free((void **)&this_host->alias);
		my_free((void **)&this_host->address);

#ifdef NSCORE
		my_free((void **)&this_host->plugin_output);
		my_free((void **)&this_host->long_plugin_output);
		my_free((void **)&this_host->perf_data);
#endif
		my_free((void **)&this_host->check_period);
		my_free((void **)&this_host->host_check_command);
		my_free((void **)&this_host->event_handler);
		my_free((void **)&this_host->failure_prediction_options);
		my_free((void **)&this_host->notification_period);
		my_free((void **)&this_host);
		this_host=next_host;
	        }

	/* free hashlist and reset pointers */
	my_free((void **)&host_hashlist);
	host_hashlist=NULL;
	host_list=NULL;

#ifdef DEBUG1
	printf("\thost_list freed\n");
#endif

	/* free memory for the host group list */
	this_hostgroup=hostgroup_list;
	while(this_hostgroup!=NULL){

		/* free memory for the group members */
		this_hostgroupmember=this_hostgroup->members;
		while(this_hostgroupmember!=NULL){
			next_hostgroupmember=this_hostgroupmember->next;
			my_free((void **)&this_hostgroupmember->host_name);
			my_free((void **)&this_hostgroupmember);
			this_hostgroupmember=next_hostgroupmember;
		        }

		next_hostgroup=this_hostgroup->next;
		my_free((void **)&this_hostgroup->group_name);
		my_free((void **)&this_hostgroup->alias);
		my_free((void **)&this_hostgroup);
		this_hostgroup=next_hostgroup;
		}

	/* free hashlist and reset pointers */
	my_free((void **)&hostgroup_hashlist);
	hostgroup_hashlist=NULL;
	hostgroup_list=NULL;

#ifdef DEBUG1
	printf("\thostgroup_list freed\n");
#endif

	/* free memory for the service group list */
	this_servicegroup=servicegroup_list;
	while(this_servicegroup!=NULL){

		/* free memory for the group members */
		this_servicegroupmember=this_servicegroup->members;
		while(this_servicegroupmember!=NULL){
			next_servicegroupmember=this_servicegroupmember->next;
			my_free((void **)&this_servicegroupmember->host_name);
			my_free((void **)&this_servicegroupmember->service_description);
			my_free((void **)&this_servicegroupmember);
			this_servicegroupmember=next_servicegroupmember;
		        }

		next_servicegroup=this_servicegroup->next;
		my_free((void **)&this_servicegroup->group_name);
		my_free((void **)&this_servicegroup->alias);
		my_free((void **)&this_servicegroup);
		this_servicegroup=next_servicegroup;
		}

	/* free hashlist and reset pointers */
	my_free((void **)&servicegroup_hashlist);
	servicegroup_hashlist=NULL;
	servicegroup_list=NULL;

#ifdef DEBUG1
	printf("\tservicegroup_list freed\n");
#endif

	/* free memory for the contact list */
	this_contact=contact_list;
	while(this_contact!=NULL){
	  
		/* free memory for the host notification commands */
		this_commandsmember=this_contact->host_notification_commands;
		while(this_commandsmember!=NULL){
			next_commandsmember=this_commandsmember->next;
			if(this_commandsmember->command!=NULL)
				my_free((void **)&this_commandsmember->command);
			my_free((void **)&this_commandsmember);
			this_commandsmember=next_commandsmember;
		        }

		/* free memory for the service notification commands */
		this_commandsmember=this_contact->service_notification_commands;
		while(this_commandsmember!=NULL){
			next_commandsmember=this_commandsmember->next;
			if(this_commandsmember->command!=NULL)
				my_free((void **)&this_commandsmember->command);
			my_free((void **)&this_commandsmember);
			this_commandsmember=next_commandsmember;
		        }

		/* free memory for custom variables */
		this_customvariablesmember=this_contact->custom_variables;
		while(this_customvariablesmember!=NULL){
			next_customvariablesmember=this_customvariablesmember->next;
			my_free((void **)&this_customvariablesmember->variable_name);
			my_free((void **)&this_customvariablesmember->variable_value);
			my_free((void **)&this_customvariablesmember);
			this_customvariablesmember=next_customvariablesmember;
		        }

		next_contact=this_contact->next;
		my_free((void **)&this_contact->name);
		my_free((void **)&this_contact->alias);
		my_free((void **)&this_contact->email);
		my_free((void **)&this_contact->pager);
		for(i=0;i<MAX_CONTACT_ADDRESSES;i++)
			my_free((void **)&this_contact->address[i]);
		my_free((void **)&this_contact->host_notification_period);
		my_free((void **)&this_contact->service_notification_period);
		my_free((void **)&this_contact);
		this_contact=next_contact;
		}

	/* free hashlist and reset pointers */
	my_free((void **)&contact_hashlist);
	contact_hashlist=NULL;
	contact_list=NULL;

#ifdef DEBUG1
	printf("\tcontact_list freed\n");
#endif

	/* free memory for the contact group list */
	this_contactgroup=contactgroup_list;
	while(this_contactgroup!=NULL){

		/* free memory for the group members */
		this_contactgroupmember=this_contactgroup->members;
		while(this_contactgroupmember!=NULL){
			next_contactgroupmember=this_contactgroupmember->next;
			my_free((void **)&this_contactgroupmember->contact_name);
			my_free((void **)&this_contactgroupmember);
			this_contactgroupmember=next_contactgroupmember;
		        }

		next_contactgroup=this_contactgroup->next;
		my_free((void **)&this_contactgroup->group_name);
		my_free((void **)&this_contactgroup->alias);
		my_free((void **)&this_contactgroup);
		this_contactgroup=next_contactgroup;
		}

	/* free hashlist and reset pointers */
	my_free((void **)&contactgroup_hashlist);
	contactgroup_hashlist=NULL;
	contactgroup_list=NULL;

#ifdef DEBUG1
	printf("\tcontactgroup_list freed\n");
#endif

	/* free memory for the service list */
	this_service=service_list;
	while(this_service!=NULL){

		next_service=this_service->next;

		/* free memory for contact groups */
		this_contactgroupsmember=this_service->contact_groups;
		while(this_contactgroupsmember!=NULL){
			next_contactgroupsmember=this_contactgroupsmember->next;
			my_free((void **)&this_contactgroupsmember->group_name);
			my_free((void **)&this_contactgroupsmember);
			this_contactgroupsmember=next_contactgroupsmember;
	                }

		/* free memory for custom variables */
		this_customvariablesmember=this_service->custom_variables;
		while(this_customvariablesmember!=NULL){
			next_customvariablesmember=this_customvariablesmember->next;
			my_free((void **)&this_customvariablesmember->variable_name);
			my_free((void **)&this_customvariablesmember->variable_value);
			my_free((void **)&this_customvariablesmember);
			this_customvariablesmember=next_customvariablesmember;
		        }

		my_free((void **)&this_service->host_name);
		my_free((void **)&this_service->description);
		my_free((void **)&this_service->display_name);
		my_free((void **)&this_service->service_check_command);
#ifdef NSCORE
		my_free((void **)&this_service->plugin_output);
		my_free((void **)&this_service->long_plugin_output);
		my_free((void **)&this_service->perf_data);
#endif
		my_free((void **)&this_service->notification_period);
		my_free((void **)&this_service->check_period);
		my_free((void **)&this_service->event_handler);
		my_free((void **)&this_service->failure_prediction_options);
		my_free((void **)&this_service->notes);
		my_free((void **)&this_service->notes_url);
		my_free((void **)&this_service->action_url);
		my_free((void **)&this_service->icon_image);
		my_free((void **)&this_service->icon_image_alt);
		my_free((void **)&this_service);
		this_service=next_service;
	        }

	/* free hashlist and reset pointers */
	my_free((void **)&service_hashlist);
	service_hashlist=NULL;
	service_list=NULL;

#ifdef DEBUG1
	printf("\tservice_list freed\n");
#endif

	/* free memory for the command list */
	this_command=command_list;
	while(this_command!=NULL){
		next_command=this_command->next;
		my_free((void **)&this_command->name);
		my_free((void **)&this_command->command_line);
		my_free((void **)&this_command);
		this_command=next_command;
	        }

	/* free hashlist and reset pointers */
	my_free((void **)&command_hashlist);
	command_hashlist=NULL;
	command_list=NULL;

#ifdef DEBUG1
	printf("\tcommand_list freed\n");
#endif

	/* free memory for the service escalation list */
	this_serviceescalation=serviceescalation_list;
	while(this_serviceescalation!=NULL){

		/* free memory for the contact group members */
		this_contactgroupsmember=this_serviceescalation->contact_groups;
		while(this_contactgroupsmember!=NULL){
			next_contactgroupsmember=this_contactgroupsmember->next;
			my_free((void **)&this_contactgroupsmember->group_name);
			my_free((void **)&this_contactgroupsmember);
			this_contactgroupsmember=next_contactgroupsmember;
		        }

		next_serviceescalation=this_serviceescalation->next;
		my_free((void **)&this_serviceescalation->host_name);
		my_free((void **)&this_serviceescalation->description);
		my_free((void **)&this_serviceescalation->escalation_period);
		my_free((void **)&this_serviceescalation);
		this_serviceescalation=next_serviceescalation;
	        }

	/* free hashlist and reset pointers */
	my_free((void **)&serviceescalation_hashlist);
	serviceescalation_hashlist=NULL;
	serviceescalation_list=NULL;

#ifdef DEBUG1
	printf("\tserviceescalation_list freed\n");
#endif

	/* free memory for the service dependency list */
	this_servicedependency=servicedependency_list;
	while(this_servicedependency!=NULL){
		next_servicedependency=this_servicedependency->next;
		my_free((void **)&this_servicedependency->dependent_host_name);
		my_free((void **)&this_servicedependency->dependent_service_description);
		my_free((void **)&this_servicedependency->host_name);
		my_free((void **)&this_servicedependency->service_description);
		my_free((void **)&this_servicedependency);
		this_servicedependency=next_servicedependency;
	        }

	/* free hashlist and reset pointers */
	my_free((void **)&servicedependency_hashlist);
	servicedependency_hashlist=NULL;
	servicedependency_list=NULL;

#ifdef DEBUG1
	printf("\tservicedependency_list freed\n");
#endif

	/* free memory for the host dependency list */
	this_hostdependency=hostdependency_list;
	while(this_hostdependency!=NULL){
		next_hostdependency=this_hostdependency->next;
		my_free((void **)&this_hostdependency->dependent_host_name);
		my_free((void **)&this_hostdependency->host_name);
		my_free((void **)&this_hostdependency);
		this_hostdependency=next_hostdependency;
	        }

	/* free hashlist and reset pointers */
	my_free((void **)&hostdependency_hashlist);
	hostdependency_hashlist=NULL;
	hostdependency_list=NULL;

#ifdef DEBUG1
	printf("\thostdependency_list freed\n");
#endif

	/* free memory for the host escalation list */
	this_hostescalation=hostescalation_list;
	while(this_hostescalation!=NULL){

		/* free memory for the contact group members */
		this_contactgroupsmember=this_hostescalation->contact_groups;
		while(this_contactgroupsmember!=NULL){
			next_contactgroupsmember=this_contactgroupsmember->next;
			my_free((void **)&this_contactgroupsmember->group_name);
			my_free((void **)&this_contactgroupsmember);
			this_contactgroupsmember=next_contactgroupsmember;
		        }

		next_hostescalation=this_hostescalation->next;
		my_free((void **)&this_hostescalation->host_name);
		my_free((void **)&this_hostescalation->escalation_period);
		my_free((void **)&this_hostescalation);
		this_hostescalation=next_hostescalation;
	        }

	/* free hashlist and reset pointers */
	my_free((void **)&hostescalation_hashlist);
	hostescalation_hashlist=NULL;
	hostescalation_list=NULL;

	/* free extended info data */
	free_extended_data();

#ifdef DEBUG1
	printf("\thostescalation_list freed\n");
#endif

#ifdef DEBUG0
	printf("free_object_data() end\n");
#endif

	return OK;
        }


/* free all allocated memory for extended info objects */
int free_extended_data(void){
	hostextinfo *this_hostextinfo=NULL;
	hostextinfo *next_hostextinfo=NULL;

#ifdef DEBUG0
	printf("free_extended_data() start\n");
#endif

	/* free memory for the extended host info list */
	for(this_hostextinfo=hostextinfo_list;this_hostextinfo!=NULL;this_hostextinfo=next_hostextinfo){
		next_hostextinfo=this_hostextinfo->next;
		my_free((void **)&this_hostextinfo->host_name);
		my_free((void **)&this_hostextinfo->notes);
		my_free((void **)&this_hostextinfo->notes_url);
		my_free((void **)&this_hostextinfo->action_url);
		my_free((void **)&this_hostextinfo->icon_image);
		my_free((void **)&this_hostextinfo->vrml_image);
		my_free((void **)&this_hostextinfo->statusmap_image);
		my_free((void **)&this_hostextinfo->icon_image_alt);
		my_free((void **)&this_hostextinfo);
	        }

#ifdef DEBUG1
	printf("\thostextinfo_list freed\n");
#endif

	/* free hashlist and reset pointers */
	my_free((void **)&hostextinfo_hashlist);
	hostextinfo_hashlist=NULL;
	hostextinfo_list=NULL;

#ifdef DEBUG0
	printf("free_extended_data() end\n");
#endif

	return OK;
        }

