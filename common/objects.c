/*****************************************************************************
 *
 * OBJECTS.C - Object addition and search functions for Nagios
 *
 * Copyright (c) 1999-2006 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 02-24-2006
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
serviceextinfo  *serviceextinfo_list=NULL,*serviceextinfo_list_tail=NULL;

host		**host_hashlist=NULL;
service		**service_hashlist=NULL;
command         **command_hashlist=NULL;
timeperiod      **timeperiod_hashlist=NULL;
contact         **contact_hashlist=NULL;
contactgroup    **contactgroup_hashlist=NULL;
hostgroup       **hostgroup_hashlist=NULL;
servicegroup    **servicegroup_hashlist=NULL;
hostextinfo     **hostextinfo_hashlist=NULL;
serviceextinfo  **serviceextinfo_hashlist=NULL;
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
int read_object_config_data(char *main_config_file,int options,int cache){
	int result=OK;

#ifdef DEBUG0
	printf("read_object_config_data() start\n");
#endif

	/********* IMPLEMENTATION-SPECIFIC INPUT FUNCTION ********/
#ifdef USE_XODTEMPLATE
	/* read in data from all text host config files (template-based) */
	result=xodtemplate_read_config_data(main_config_file,options,cache);
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
	host *temp_host, *lastpointer;
	int hashslot;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
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

	hashslot=hashfunc1(new_host->name,HOST_HASHSLOTS);
	lastpointer=NULL;
	for(temp_host=host_hashlist[hashslot];temp_host && compare_hashdata1(temp_host->name,new_host->name)<0;temp_host=temp_host->nexthash)
		lastpointer=temp_host;

	if(!temp_host || (compare_hashdata1(temp_host->name,new_host->name)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_host;
		else
			host_hashlist[hashslot]=new_host;
		new_host->nexthash=temp_host;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add duplicate host '%s'.\n",new_host->name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
	return 0;
        }


int add_service_to_hashlist(service *new_service){
	service *temp_service, *lastpointer;
	int hashslot;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
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

	hashslot=hashfunc2(new_service->host_name,new_service->description,SERVICE_HASHSLOTS);
	lastpointer=NULL;
	for(temp_service=service_hashlist[hashslot];temp_service && compare_hashdata2(temp_service->host_name,temp_service->description,new_service->host_name,new_service->description)<0;temp_service=temp_service->nexthash)
		lastpointer=temp_service;

	if(!temp_service || (compare_hashdata2(temp_service->host_name,temp_service->description,new_service->host_name,new_service->description)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_service;
		else
			service_hashlist[hashslot]=new_service;
		new_service->nexthash=temp_service;


		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add duplicate service '%s' on host '%s'.\n",new_service->description,new_service->host_name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
	return 0;
        }


int add_command_to_hashlist(command *new_command){
	command *temp_command, *lastpointer;
	int hashslot;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
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

	hashslot=hashfunc1(new_command->name,COMMAND_HASHSLOTS);
	lastpointer=NULL;
	for(temp_command=command_hashlist[hashslot];temp_command && compare_hashdata1(temp_command->name,new_command->name)<0;temp_command=temp_command->nexthash)
		lastpointer=temp_command;

	if(!temp_command || (compare_hashdata1(temp_command->name,new_command->name)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_command;
		else
			command_hashlist[hashslot]=new_command;
		new_command->nexthash=temp_command;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add duplicate command '%s'.\n",new_command->name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
	return 0;
        }


int add_timeperiod_to_hashlist(timeperiod *new_timeperiod){
	timeperiod *temp_timeperiod, *lastpointer;
	int hashslot;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
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

	hashslot=hashfunc1(new_timeperiod->name,TIMEPERIOD_HASHSLOTS);
	lastpointer=NULL;
	for(temp_timeperiod=timeperiod_hashlist[hashslot];temp_timeperiod && compare_hashdata1(temp_timeperiod->name,new_timeperiod->name)<0;temp_timeperiod=temp_timeperiod->nexthash)
		lastpointer=temp_timeperiod;

	if(!temp_timeperiod || (compare_hashdata1(temp_timeperiod->name,new_timeperiod->name)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_timeperiod;
		else
			timeperiod_hashlist[hashslot]=new_timeperiod;
		new_timeperiod->nexthash=temp_timeperiod;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add duplicate timeperiod '%s'.\n",new_timeperiod->name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
	return 0;
        }


int add_contact_to_hashlist(contact *new_contact){
	contact *temp_contact, *lastpointer;
	int hashslot;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
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

	hashslot=hashfunc1(new_contact->name,CONTACT_HASHSLOTS);
	lastpointer=NULL;
	for(temp_contact=contact_hashlist[hashslot];temp_contact && compare_hashdata1(temp_contact->name,new_contact->name)<0;temp_contact=temp_contact->nexthash)
		lastpointer=temp_contact;

	if(!temp_contact || (compare_hashdata1(temp_contact->name,new_contact->name)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_contact;
		else
			contact_hashlist[hashslot]=new_contact;
		new_contact->nexthash=temp_contact;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add duplicate contact '%s'.\n",new_contact->name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
	return 0;
        }


int add_contactgroup_to_hashlist(contactgroup *new_contactgroup){
	contactgroup *temp_contactgroup, *lastpointer;
	int hashslot;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
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

	hashslot=hashfunc1(new_contactgroup->group_name,CONTACTGROUP_HASHSLOTS);
	lastpointer=NULL;
	for(temp_contactgroup=contactgroup_hashlist[hashslot];temp_contactgroup && compare_hashdata1(temp_contactgroup->group_name,new_contactgroup->group_name)<0;temp_contactgroup=temp_contactgroup->nexthash)
		lastpointer=temp_contactgroup;

	if(!temp_contactgroup || (compare_hashdata1(temp_contactgroup->group_name,new_contactgroup->group_name)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_contactgroup;
		else
			contactgroup_hashlist[hashslot]=new_contactgroup;
		new_contactgroup->nexthash=temp_contactgroup;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add duplicate contactgroup '%s'.\n",new_contactgroup->group_name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
	return 0;
        }


int add_hostgroup_to_hashlist(hostgroup *new_hostgroup){
	hostgroup *temp_hostgroup, *lastpointer;
	int hashslot;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
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

	hashslot=hashfunc1(new_hostgroup->group_name,HOSTGROUP_HASHSLOTS);
	lastpointer=NULL;
	for(temp_hostgroup=hostgroup_hashlist[hashslot];temp_hostgroup && compare_hashdata1(temp_hostgroup->group_name,new_hostgroup->group_name)<0;temp_hostgroup=temp_hostgroup->nexthash)
		lastpointer=temp_hostgroup;

	if(!temp_hostgroup || (compare_hashdata1(temp_hostgroup->group_name,new_hostgroup->group_name)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_hostgroup;
		else
			hostgroup_hashlist[hashslot]=new_hostgroup;
		new_hostgroup->nexthash=temp_hostgroup;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add duplicate hostgroup '%s'.\n",new_hostgroup->group_name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
	return 0;
        }


int add_servicegroup_to_hashlist(servicegroup *new_servicegroup){
	servicegroup *temp_servicegroup, *lastpointer;
	int hashslot;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
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

	hashslot=hashfunc1(new_servicegroup->group_name,SERVICEGROUP_HASHSLOTS);
	lastpointer=NULL;
	for(temp_servicegroup=servicegroup_hashlist[hashslot];temp_servicegroup && compare_hashdata1(temp_servicegroup->group_name,new_servicegroup->group_name)<0;temp_servicegroup=temp_servicegroup->nexthash)
		lastpointer=temp_servicegroup;

	if(!temp_servicegroup || (compare_hashdata1(temp_servicegroup->group_name,new_servicegroup->group_name)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_servicegroup;
		else
			servicegroup_hashlist[hashslot]=new_servicegroup;
		new_servicegroup->nexthash=temp_servicegroup;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add duplicate servicegroup '%s'.\n",new_servicegroup->group_name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
	return 0;
        }


/* adds hostextinfo to hash list in memory */
int add_hostextinfo_to_hashlist(hostextinfo *new_hostextinfo){
	hostextinfo *temp_hostextinfo, *lastpointer;
	int hashslot;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
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

	hashslot=hashfunc1(new_hostextinfo->host_name,HOSTEXTINFO_HASHSLOTS);
	lastpointer=NULL;
	for(temp_hostextinfo=hostextinfo_hashlist[hashslot];temp_hostextinfo && compare_hashdata1(temp_hostextinfo->host_name,new_hostextinfo->host_name)<0;temp_hostextinfo=temp_hostextinfo->nexthash)
		lastpointer=temp_hostextinfo;

	if(!temp_hostextinfo || (compare_hashdata1(temp_hostextinfo->host_name,new_hostextinfo->host_name)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_hostextinfo;
		else
			hostextinfo_hashlist[hashslot]=new_hostextinfo;
		new_hostextinfo->nexthash=temp_hostextinfo;

		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add duplicate hostextinfo entry for host '%s'.\n",new_hostextinfo->host_name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
	return 0;
        }


int add_serviceextinfo_to_hashlist(serviceextinfo *new_serviceextinfo){
	serviceextinfo *temp_serviceextinfo, *lastpointer;
	int hashslot;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

	/* initialize hash list */
	if(serviceextinfo_hashlist==NULL){
		int i;

		serviceextinfo_hashlist=(serviceextinfo **)malloc(sizeof(serviceextinfo *)*SERVICEEXTINFO_HASHSLOTS);
		if(serviceextinfo_hashlist==NULL)
			return 0;
		
		for(i=0;i< SERVICEEXTINFO_HASHSLOTS;i++)
			serviceextinfo_hashlist[i]=NULL;
	        }

	if(!new_serviceextinfo)
		return 0;

	hashslot=hashfunc2(new_serviceextinfo->host_name,new_serviceextinfo->description,SERVICEEXTINFO_HASHSLOTS);
	lastpointer=NULL;
	for(temp_serviceextinfo=serviceextinfo_hashlist[hashslot];temp_serviceextinfo && compare_hashdata2(temp_serviceextinfo->host_name,temp_serviceextinfo->description,new_serviceextinfo->host_name,new_serviceextinfo->description)<0;temp_serviceextinfo=temp_serviceextinfo->nexthash)
		lastpointer=temp_serviceextinfo;

	if(!temp_serviceextinfo || (compare_hashdata2(temp_serviceextinfo->host_name,temp_serviceextinfo->description,new_serviceextinfo->host_name,new_serviceextinfo->description)!=0)){
		if(lastpointer)
			lastpointer->nexthash=new_serviceextinfo;
		else
			serviceextinfo_hashlist[hashslot]=new_serviceextinfo;
		new_serviceextinfo->nexthash=temp_serviceextinfo;


		return 1;
	        }

	/* else already exists */
#ifdef NSCORE
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add duplicate serviceextinfo entry for service '%s' on host '%s'.\n",new_serviceextinfo->description,new_serviceextinfo->host_name);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
	return 0;
        }


/* adds hostdependency to hash list in memory */
int add_hostdependency_to_hashlist(hostdependency *new_hostdependency){
	hostdependency *temp_hostdependency, *lastpointer;
	int hashslot;

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

	hashslot=hashfunc1(new_hostdependency->dependent_host_name,HOSTDEPENDENCY_HASHSLOTS);
	lastpointer=NULL;
	for(temp_hostdependency=hostdependency_hashlist[hashslot];temp_hostdependency && compare_hashdata1(temp_hostdependency->dependent_host_name,new_hostdependency->dependent_host_name)<0;temp_hostdependency=temp_hostdependency->nexthash)
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
	servicedependency *temp_servicedependency, *lastpointer;
	int hashslot;

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

	hashslot=hashfunc2(new_servicedependency->dependent_host_name,new_servicedependency->dependent_service_description,SERVICEDEPENDENCY_HASHSLOTS);
	lastpointer=NULL;
	for(temp_servicedependency=servicedependency_hashlist[hashslot];temp_servicedependency && compare_hashdata2(temp_servicedependency->dependent_host_name,temp_servicedependency->dependent_service_description,new_servicedependency->dependent_host_name,new_servicedependency->dependent_service_description)<0;temp_servicedependency=temp_servicedependency->nexthash)
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
	hostescalation *temp_hostescalation, *lastpointer;
	int hashslot;

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

	hashslot=hashfunc1(new_hostescalation->host_name,HOSTESCALATION_HASHSLOTS);
	lastpointer=NULL;
	for(temp_hostescalation=hostescalation_hashlist[hashslot];temp_hostescalation && compare_hashdata1(temp_hostescalation->host_name,new_hostescalation->host_name)<0;temp_hostescalation=temp_hostescalation->nexthash)
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
	serviceescalation *temp_serviceescalation, *lastpointer;
	int hashslot;

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

	hashslot=hashfunc2(new_serviceescalation->host_name,new_serviceescalation->description,SERVICEESCALATION_HASHSLOTS);
	lastpointer=NULL;
	for(temp_serviceescalation=serviceescalation_hashlist[hashslot];temp_serviceescalation && compare_hashdata2(temp_serviceescalation->host_name,temp_serviceescalation->description,new_serviceescalation->host_name,new_serviceescalation->description)<0;temp_serviceescalation=temp_serviceescalation->nexthash)
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
	timeperiod *new_timeperiod;
	timeperiod *temp_timeperiod;
	int day=0;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_timeperiod() start\n");
#endif

	/* make sure we have the data we need */
	if(name==NULL || alias==NULL)
		return NULL;

	strip(name);
	strip(alias);

	if(!strcmp(name,"") || !strcmp(alias,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Name or alias for timeperiod is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* make sure there isn't a timeperiod by this name added already */
	temp_timeperiod=find_timeperiod(name);
	if(temp_timeperiod!=NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Timeperiod '%s' has already been defined\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for the new timeperiod */
	new_timeperiod=malloc(sizeof(timeperiod));
	if(new_timeperiod==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for timeperiod '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_timeperiod->name=(char *)malloc(strlen(name)+1);
	if(new_timeperiod->name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for timeperiod '%s' name\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_timeperiod);
		return NULL;
	        }
	new_timeperiod->alias=(char *)malloc(strlen(alias)+1);
	if(new_timeperiod->alias==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for timeperiod '%s' alias\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_timeperiod->name);
		free(new_timeperiod);
		return NULL;
	        }

	strcpy(new_timeperiod->name,name);
	strcpy(new_timeperiod->alias,alias);

	/* initialize the time ranges for each day in the time period to NULL */
	for(day=0;day<7;day++)
		new_timeperiod->days[day]=NULL;

	new_timeperiod->next=NULL;
	new_timeperiod->nexthash=NULL;

	/* add new timeperiod to timeperiod chained hash list */
	if(!add_timeperiod_to_hashlist(new_timeperiod)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for timeperiod list to add timeperiod '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_timeperiod->alias);
		free(new_timeperiod->name);
		free(new_timeperiod);
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
	timerange *new_timerange;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_timerange_to_timeperiod() start\n");
#endif

	/* make sure we have the data we need */
	if(period==NULL)
		return NULL;

	if(day<0 || day>6){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Day %d is not valid for timeperiod '%s'\n",day,period->name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(start_time<0 || start_time>86400){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Start time %lu on day %d is not valid for timeperiod '%s'\n",start_time,day,period->name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(end_time<0 || end_time>86400){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: End time %lu on day %d is not value for timeperiod '%s'\n",end_time,day,period->name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for the new time range */
	new_timerange=malloc(sizeof(timerange));
	if(new_timerange==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for day %d timerange in timeperiod '%s'\n",day,period->name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

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
host *add_host(char *name, char *alias, char *address, char *check_period, int check_interval, int max_attempts, int notify_up, int notify_down, int notify_unreachable, int notify_flapping, int notification_interval, char *notification_period, int notifications_enabled, char *check_command, int checks_enabled, int accept_passive_checks, char *event_handler, int event_handler_enabled, int flap_detection_enabled, double low_flap_threshold, double high_flap_threshold, int stalk_up, int stalk_down, int stalk_unreachable, int process_perfdata, int failure_prediction_enabled, char *failure_prediction_options, int check_freshness, int freshness_threshold, int retain_status_information, int retain_nonstatus_information, int obsess_over_host){
	host *temp_host;
	host *new_host;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
	int x;
#endif

#ifdef DEBUG0
	printf("add_host() start\n");
#endif

	/* make sure we have the data we need */
	if(name==NULL || alias==NULL || address==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host name, alias, or address is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(name);
	strip(alias);
	strip(address);
	strip(check_command);
	strip(event_handler);
	strip(notification_period);

	if(!strcmp(name,"") || !strcmp(alias,"") || !strcmp(address,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host name, alias, or address is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* make sure there isn't a host by this name already added */
	temp_host=find_host(name);
	if(temp_host!=NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host '%s' has already been defined\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* make sure the name isn't too long */
	if(strlen(name)>MAX_HOSTNAME_LENGTH-1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host name '%s' exceeds maximum length of %d characters\n",name,MAX_HOSTNAME_LENGTH-1);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(max_attempts<=0){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid max_check_attempts value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(check_interval<0){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid check_interval value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(notification_interval<0){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notification_interval value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(notify_up<0 || notify_up>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_up value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_down<0 || notify_down>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_down value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_unreachable<0 || notify_unreachable>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_unreachable value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_flapping<0 || notify_flapping>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_flappingvalue for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(checks_enabled<0 || checks_enabled>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid checks_enabled value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(accept_passive_checks<0 || accept_passive_checks>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid accept_passive_checks value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notifications_enabled<0 || notifications_enabled>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notifications_enabled value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(event_handler_enabled<0 || event_handler_enabled>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid event_handler_enabled value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(flap_detection_enabled<0 || flap_detection_enabled>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid flap_detection_enabled value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(stalk_up<0 || stalk_up>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid stalk_up value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(stalk_down<0 || stalk_down>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid stalk_warning value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(stalk_unreachable<0 || stalk_unreachable>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid stalk_unknown value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(process_perfdata<0 || process_perfdata>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid process_perfdata value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(failure_prediction_enabled<0 || failure_prediction_enabled>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid failure_prediction_enabled value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(check_freshness<0 || check_freshness>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid check_freshness value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(freshness_threshold<0){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid freshness_threshold value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(obsess_over_host<0 || obsess_over_host>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid obsess_over_host value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(retain_status_information<0 || retain_status_information>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid retain_status_information value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(retain_nonstatus_information<0 || retain_nonstatus_information>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid retain_nonstatus_information value for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }


	/* allocate memory for a new host */
	new_host=(host *)malloc(sizeof(host));
	if(new_host==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_host->name=strdup(name);
	if(new_host->name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' name\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_host);
		return NULL;
	        }
	new_host->alias=strdup(alias);
	if(new_host->alias==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' alias\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_host->name);
		free(new_host);
		return NULL;
	        }
	new_host->address=strdup(address);
	if(new_host->address==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' address\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_host->alias);
		free(new_host->name);
		free(new_host);
		return NULL;
	        }
	if(check_period!=NULL && strcmp(check_period,"")){
		new_host->check_period=strdup(check_period);
		if(new_host->check_period==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' check period\n",name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(new_host->address);
			free(new_host->alias);
			free(new_host->name);
			free(new_host);
			return NULL;
		        }
	        }
	else
		new_host->check_period=NULL;
	if(notification_period!=NULL && strcmp(notification_period,"")){
		new_host->notification_period=strdup(notification_period);
		if(new_host->notification_period==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' notification period\n",name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(new_host->check_period);
			free(new_host->address);
			free(new_host->alias);
			free(new_host->name);
			free(new_host);
			return NULL;
		        }
	        }
	else
		new_host->notification_period=NULL;
	if(check_command!=NULL && strcmp(check_command,"")){
		new_host->host_check_command=strdup(check_command);
		if(new_host->host_check_command==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' check command\n",name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			if(new_host->notification_period!=NULL)
				free(new_host->notification_period);
			free(new_host->check_period);
			free(new_host->address);
			free(new_host->alias);
			free(new_host->name);
			free(new_host);
			return NULL;
		        }
	        }
	else
		new_host->host_check_command=NULL;
	if(event_handler!=NULL && strcmp(event_handler,"")){
		new_host->event_handler=strdup(event_handler);
		if(new_host->event_handler==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' event handler\n",name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			if(new_host->host_check_command!=NULL)
				free(new_host->host_check_command);
			if(new_host->notification_period!=NULL)
				free(new_host->notification_period);
			free(new_host->check_period);
			free(new_host->address);
			free(new_host->alias);
			free(new_host->name);
			free(new_host);
			return NULL;
	                }
	        }
	else
		new_host->event_handler=NULL;
	if(failure_prediction_options!=NULL && strcmp(failure_prediction_options,"")){
		new_host->failure_prediction_options=strdup(failure_prediction_options);
		if(new_host->failure_prediction_options==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' failure prediction options\n",name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			if(new_host->event_handler!=NULL)
				free(new_host->event_handler);
			if(new_host->host_check_command!=NULL)
				free(new_host->host_check_command);
			if(new_host->notification_period!=NULL)
				free(new_host->notification_period);
			free(new_host->check_period);
			free(new_host->address);
			free(new_host->alias);
			free(new_host->name);
			free(new_host);
			return NULL;
	                }
	        }
	else
		new_host->failure_prediction_options=NULL;

	
	new_host->parent_hosts=NULL;
	new_host->max_attempts=max_attempts;
	new_host->contact_groups=NULL;
	new_host->check_interval=check_interval;
	new_host->notification_interval=notification_interval;
	new_host->notify_on_recovery=(notify_up>0)?TRUE:FALSE;
	new_host->notify_on_down=(notify_down>0)?TRUE:FALSE;
	new_host->notify_on_unreachable=(notify_unreachable>0)?TRUE:FALSE;
	new_host->notify_on_flapping=(notify_flapping>0)?TRUE:FALSE;
	new_host->flap_detection_enabled=(flap_detection_enabled>0)?TRUE:FALSE;
	new_host->low_flap_threshold=low_flap_threshold;
	new_host->high_flap_threshold=high_flap_threshold;
	new_host->stalk_on_up=(stalk_up>0)?TRUE:FALSE;
	new_host->stalk_on_down=(stalk_down>0)?TRUE:FALSE;
	new_host->stalk_on_unreachable=(stalk_unreachable>0)?TRUE:FALSE;
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

	/* allocate new plugin output buffer */
	new_host->plugin_output=(char *)malloc(MAX_PLUGINOUTPUT_LENGTH);
	if(new_host->plugin_output==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' plugin output buffer\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);

		if(new_host->failure_prediction_options!=NULL)
			free(new_host->failure_prediction_options);
		if(new_host->event_handler!=NULL)
			free(new_host->event_handler);
		if(new_host->host_check_command!=NULL)
			free(new_host->host_check_command);
		if(new_host->notification_period!=NULL)
			free(new_host->notification_period);
		free(new_host->address);
		free(new_host->alias);
		free(new_host->name);
		free(new_host);
		return NULL;
	        }
	strcpy(new_host->plugin_output,"");

	/* allocate new performance data buffer */
	new_host->perf_data=(char *)malloc(MAX_PLUGINOUTPUT_LENGTH);
	if(new_host->perf_data==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' performance data buffer\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);

		if(new_host->failure_prediction_options!=NULL)
			free(new_host->failure_prediction_options);
		if(new_host->event_handler!=NULL)
			free(new_host->event_handler);
		if(new_host->host_check_command!=NULL)
			free(new_host->host_check_command);
		if(new_host->notification_period!=NULL)
			free(new_host->notification_period);
		free(new_host->address);
		free(new_host->alias);
		free(new_host->name);
		free(new_host->plugin_output);
		free(new_host);
		return NULL;
	        }
	strcpy(new_host->perf_data,"");

#endif

	new_host->next=NULL;
	new_host->nexthash=NULL;

	/* add new host to host chained hash list */
	if(!add_host_to_hashlist(new_host)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host list to add host '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
		
		free(new_host->plugin_output);
		if(new_host->perf_data)
			free(new_host->perf_data);
#endif

		if(new_host->failure_prediction_options!=NULL)
			free(new_host->failure_prediction_options);
		if(new_host->event_handler!=NULL)
			free(new_host->event_handler);
		if(new_host->host_check_command!=NULL)
			free(new_host->host_check_command);
		if(new_host->notification_period!=NULL)
			free(new_host->notification_period);
		free(new_host->address);
		free(new_host->alias);
		free(new_host->name);
		free(new_host);
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
	hostsmember *new_hostsmember;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_parent_host_to_host() start\n");
#endif

	/* make sure we have the data we need */
	if(hst==NULL || host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host is NULL or parent host name is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(host_name);

	if(!strcmp(host_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host '%s' parent host name is NULL\n",hst->name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* a host cannot be a parent/child of itself */
	if(!strcmp(host_name,hst->name)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host '%s' cannot be a child/parent of itself\n",hst->name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory */
	new_hostsmember=(hostsmember *)malloc(sizeof(hostsmember));
	if(new_hostsmember==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' parent host (%s)\n",hst->name,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	new_hostsmember->host_name=strdup(host_name);
	if(new_hostsmember->host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' parent host (%s) name\n",hst->name,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_hostsmember);
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
	contactgroupsmember *new_contactgroupsmember;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_contactgroup_to_host() start\n");
#endif

	/* make sure we have the data we need */
	if(hst==NULL || group_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host or contactgroup member is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(group_name);

	if(!strcmp(group_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host '%s' contactgroup member is NULL\n",hst->name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for a new member */
	new_contactgroupsmember=malloc(sizeof(contactgroupsmember));
	if(new_contactgroupsmember==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' contactgroup member '%s'\n",hst->name,group_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_contactgroupsmember->group_name=strdup(group_name);
	if(new_contactgroupsmember->group_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' contactgroup member '%s' name\n",hst->name,group_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_contactgroupsmember);
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


/* add a new host group to the list in memory */
hostgroup *add_hostgroup(char *name,char *alias){
	hostgroup *temp_hostgroup;
	hostgroup *new_hostgroup;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_hostgroup() start\n");
#endif

	/* make sure we have the data we need */
	if(name==NULL || alias==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Hostgroup name and/or alias is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(name);
	strip(alias);

	if(!strcmp(name,"") || !strcmp(alias,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Hostgroup name and/or alias is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* make sure a hostgroup by this name hasn't been added already */
	temp_hostgroup=find_hostgroup(name);
	if(temp_hostgroup!=NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Hostgroup '%s' has already been defined\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory */
	new_hostgroup=(hostgroup *)malloc(sizeof(hostgroup));
	if(new_hostgroup==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for hostgroup '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_hostgroup->group_name=strdup(name);
	if(new_hostgroup->group_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for hostgroup '%s' name\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_hostgroup);
		return NULL;
	        }
	new_hostgroup->alias=strdup(alias);
	if(new_hostgroup->alias==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for hostgroup '%s' alias\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_hostgroup->group_name);
		free(new_hostgroup);
		return NULL;
	        }

	new_hostgroup->members=NULL;

	new_hostgroup->next=NULL;
	new_hostgroup->nexthash=NULL;

	/* add new hostgroup to hostgroup chained hash list */
	if(!add_hostgroup_to_hashlist(new_hostgroup)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for hostgroup list to add hostgroup '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif

		free(new_hostgroup->group_name);
		free(new_hostgroup->alias);
		free(new_hostgroup);
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
	hostgroupmember *new_member;
	hostgroupmember *last_member;
	hostgroupmember *temp_member;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_host_to_hostgroup() start\n");
#endif

	/* make sure we have the data we need */
	if(temp_hostgroup==NULL || host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Hostgroup or group member is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(host_name);

	if(!strcmp(host_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Hostgroup member is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for a new member */
	new_member=malloc(sizeof(hostgroupmember));
	if(new_member==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for hostgroup '%s' member '%s'\n",temp_hostgroup->group_name,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_member->host_name=strdup(host_name);
	if(new_member->host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for hostgroup '%s' member '%s' name\n",temp_hostgroup->group_name,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_member);
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
	servicegroup *temp_servicegroup;
	servicegroup *new_servicegroup;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_servicegroup() start\n");
#endif

	/* make sure we have the data we need */
	if(name==NULL || alias==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Servicegroup name and/or alias is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(name);
	strip(alias);

	if(!strcmp(name,"") || !strcmp(alias,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Servicegroup name and/or alias is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* make sure a servicegroup by this name hasn't been added already */
	temp_servicegroup=find_servicegroup(name);
	if(temp_servicegroup!=NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Servicegroup '%s' has already been defined\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory */
	new_servicegroup=(servicegroup *)malloc(sizeof(servicegroup));
	if(new_servicegroup==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for servicegroup '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_servicegroup->group_name=strdup(name);
	if(new_servicegroup->group_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for servicegroup '%s' name\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_servicegroup);
		return NULL;
	        }
	new_servicegroup->alias=strdup(alias);
	if(new_servicegroup->alias==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for servicegroup '%s' alias\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_servicegroup->group_name);
		free(new_servicegroup);
		return NULL;
	        }

	new_servicegroup->members=NULL;

	new_servicegroup->next=NULL;
	new_servicegroup->nexthash=NULL;

	/* add new servicegroup to servicegroup chained hash list */
	if(!add_servicegroup_to_hashlist(new_servicegroup)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for servicegroup list to add servicegroup '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif

		free(new_servicegroup->group_name);
		free(new_servicegroup->alias);
		free(new_servicegroup);
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
	servicegroupmember *new_member;
	servicegroupmember *last_member;
	servicegroupmember *temp_member;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_service_to_servicegroup() start\n");
#endif

	/* make sure we have the data we need */
	if(temp_servicegroup==NULL || host_name==NULL || svc_description==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Servicegroup or group member is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(host_name);
	strip(svc_description);

	if(!strcmp(host_name,"") || !strcmp(svc_description,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Servicegroup member is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for a new member */
	new_member=malloc(sizeof(servicegroupmember));
	if(new_member==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for servicegroup '%s' member (service '%s' on host '%s')\n",temp_servicegroup->group_name,host_name,svc_description);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_member->host_name=strdup(host_name);
	if(new_member->host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for servicegroup '%s' member (service '%s' on host '%s') name\n",temp_servicegroup->group_name,host_name,svc_description);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_member);
		return NULL;
	        }
	new_member->service_description=strdup(svc_description);
	if(new_member->service_description==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for servicegroup '%s' member (service '%s' on host '%s') name\n",temp_servicegroup->group_name,host_name,svc_description);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_member->host_name);
		free(new_member);
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
contact *add_contact(char *name,char *alias, char *email, char *pager, char **addresses, char *svc_notification_period, char *host_notification_period,int notify_service_ok,int notify_service_critical,int notify_service_warning, int notify_service_unknown, int notify_service_flapping, int notify_host_up, int notify_host_down, int notify_host_unreachable, int notify_host_flapping){
	contact *temp_contact;
	contact *new_contact;
	int x;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_contact() start\n");
#endif

	/* make sure we have the data we need */
	if(name==NULL || alias==NULL || (email==NULL && pager==NULL)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contact name, alias, or email address and pager number are NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(name);
	strip(alias);
	strip(email);
	strip(pager);
	strip(svc_notification_period);
	strip(host_notification_period);

	if(!strcmp(name,"") || !strcmp(alias,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contact name or alias is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if((email==NULL || !strcmp(email,"")) && (pager==NULL || !strcmp(pager,""))){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contact email address and pager number are both NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* make sure there isn't a contact by this name already added */
	temp_contact=find_contact(name);
	if(temp_contact!=NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contact '%s' has already been defined\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(notify_service_ok<0 || notify_service_ok>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_service_ok value for contact '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_service_critical<0 || notify_service_critical>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_service_critical value for contact '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_service_warning<0 || notify_service_warning>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_service_warning value for contact '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_service_unknown<0 || notify_service_unknown>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_service_unknown value for contact '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_service_flapping<0 || notify_service_flapping>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_service_flapping value for contact '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(notify_host_up<0 || notify_host_up>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_host_up value for contact '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_host_down<0 || notify_host_down>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_host_down value for contact '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_host_unreachable<0 || notify_host_unreachable>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_host_unreachable value for contact '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_host_flapping<0 || notify_host_flapping>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_host_flapping value for contact '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for a new contact */
	new_contact=(contact *)malloc(sizeof(contact));
	if(new_contact==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_contact->name=strdup(name);
	if(new_contact->name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s' name\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_contact);
		return NULL;
	        }
	new_contact->alias=strdup(alias);
	if(new_contact->alias==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s' alias\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_contact->name);
		free(new_contact);
		return NULL;
	        }
	if(email!=NULL && strcmp(email,"")){
		new_contact->email=strdup(email);
		if(new_contact->email==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s' email address\n",name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_contact->name);
			free(new_contact->alias);
			free(new_contact);
			return NULL;
		        }
	        }
	else
		new_contact->email=NULL;
	if(pager!=NULL && strcmp(pager,"")){
		new_contact->pager=strdup(pager);
		if(new_contact->pager==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s' pager number\n",name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(new_contact->name);
			free(new_contact->alias);
			if(new_contact->email!=NULL)
				free(new_contact->email);
			free(new_contact);
			return NULL;
		        }
	        }
	else
		new_contact->pager=NULL;
	if(svc_notification_period!=NULL && strcmp(svc_notification_period,"")){
		new_contact->service_notification_period=strdup(svc_notification_period);
		if(new_contact->service_notification_period==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s' service notification period\n",name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(new_contact->name);
			free(new_contact->alias);
			if(new_contact->email!=NULL)
				free(new_contact->email);
			if(new_contact->pager!=NULL)
				free(new_contact->pager);
			free(new_contact);
			return NULL;
		        }
	        }
	else 
		new_contact->service_notification_period=NULL;
	if(host_notification_period!=NULL && strcmp(host_notification_period,"")){
		new_contact->host_notification_period=strdup(host_notification_period);
		if(new_contact->host_notification_period==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s' host notification period\n",name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(new_contact->name);
			free(new_contact->alias);
			if(new_contact->email!=NULL)
				free(new_contact->email);
			if(new_contact->pager!=NULL)
				free(new_contact->pager);
			if(new_contact->service_notification_period!=NULL)
				free(new_contact->service_notification_period);
			free(new_contact);
			return NULL;
		        }
	        }
	else
		new_contact->host_notification_period=NULL;


	for(x=0;x<MAX_CONTACT_ADDRESSES;x++){
		new_contact->address[x]=NULL;
		if(addresses[x]!=NULL){
			strip(addresses[x]);
			new_contact->address[x]=strdup(addresses[x]);
			if(new_contact->address[x]==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s' address #%d\n",name,x);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				free(new_contact->name);
				free(new_contact->alias);
				free(new_contact->email);
				free(new_contact->pager);
				free(new_contact->service_notification_period);
				free(new_contact->host_notification_period);
				free(new_contact);
				return NULL;
		                }
		        }
	        }

	new_contact->host_notification_commands=NULL;
	new_contact->service_notification_commands=NULL;

	new_contact->notify_on_service_recovery=(notify_service_ok>0)?TRUE:FALSE;
	new_contact->notify_on_service_critical=(notify_service_critical>0)?TRUE:FALSE;
	new_contact->notify_on_service_warning=(notify_service_warning>0)?TRUE:FALSE;
	new_contact->notify_on_service_unknown=(notify_service_unknown>0)?TRUE:FALSE;
	new_contact->notify_on_service_flapping=(notify_service_flapping>0)?TRUE:FALSE;
	new_contact->notify_on_host_recovery=(notify_host_up>0)?TRUE:FALSE;
	new_contact->notify_on_host_down=(notify_host_down>0)?TRUE:FALSE;
	new_contact->notify_on_host_unreachable=(notify_host_unreachable>0)?TRUE:FALSE;
	new_contact->notify_on_host_flapping=(notify_host_flapping>0)?TRUE:FALSE;

	new_contact->next=NULL;
	new_contact->nexthash=NULL;

	/* add new contact to contact chained hash list */
	if(!add_contact_to_hashlist(new_contact)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact list to add contact '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
			free(new_contact->address[x]);
		free(new_contact->name);
		free(new_contact->alias);
		free(new_contact->email);
		free(new_contact->pager);
		free(new_contact->service_notification_period);
		free(new_contact->host_notification_period);
		free(new_contact);

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
	commandsmember *new_commandsmember;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_host_notification_command_to_contact() start\n");
#endif

	/* make sure we have the data we need */
	if(cntct==NULL || command_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contact or host notification command is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(command_name);

	if(!strcmp(command_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contact '%s' host notification command is NULL\n",cntct->name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory */
	new_commandsmember=malloc(sizeof(commandsmember));
	if(new_commandsmember==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s' host notification command '%s'\n",cntct->name,command_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	new_commandsmember->command=strdup(command_name);
	if(new_commandsmember->command==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s' host notification command '%s' name\n",cntct->name,command_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_commandsmember);
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
	commandsmember *new_commandsmember;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_service_notification_command_to_contact() start\n");
#endif

	/* make sure we have the data we need */
	if(cntct==NULL || command_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contact or service notification command is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(command_name);

	if(!strcmp(command_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contact '%s' service notification command is NULL\n",cntct->name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory */
	new_commandsmember=malloc(sizeof(commandsmember));
	if(new_commandsmember==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s' service notification command '%s'\n",cntct->name,command_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	new_commandsmember->command=strdup(command_name);
	if(new_commandsmember->command==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact '%s' service notification command '%s' name\n",cntct->name,command_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_commandsmember);
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



/* add a new contact group to the list in memory */
contactgroup *add_contactgroup(char *name,char *alias){
	contactgroup *temp_contactgroup;
	contactgroup *new_contactgroup;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_contactgroup() start\n");
#endif

	/* make sure we have the data we need */
	if(name==NULL || alias==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contactgroup name or alias is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(name);
	strip(alias);

	if(!strcmp(name,"") || !strcmp(alias,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contactgroup name or alias is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* make sure there isn't a contactgroup by this name added already */
	temp_contactgroup=find_contactgroup(name);
	if(temp_contactgroup!=NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contactgroup '%s' has already been defined\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for a new contactgroup entry */
	new_contactgroup=malloc(sizeof(contactgroup));
	if(new_contactgroup==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contactgroup '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_contactgroup->group_name=strdup(name);
	if(new_contactgroup->group_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contactgroup '%s' name\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_contactgroup);
		return NULL;
	        }
	new_contactgroup->alias=strdup(alias);
	if(new_contactgroup->alias==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contactgroup '%s' alias\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_contactgroup->group_name);
		free(new_contactgroup);
		return NULL;
	        }

	new_contactgroup->members=NULL;

	new_contactgroup->next=NULL;
	new_contactgroup->nexthash=NULL;

	/* add new contactgroup to contactgroup chained hash list */
	if(!add_contactgroup_to_hashlist(new_contactgroup)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contactgroup list to add contactgroup '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_contactgroup->alias);
		free(new_contactgroup->group_name);
		free(new_contactgroup);

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
	contactgroupmember *new_contactgroupmember;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_contact_to_contactgroup() start\n");
#endif

	/* make sure we have the data we need */
	if(grp==NULL || contact_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contactgroup or contact name is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(contact_name);

	if(!strcmp(contact_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contactgroup '%s' contact name is NULL\n",grp->group_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for a new member */
	new_contactgroupmember=malloc(sizeof(contactgroupmember));
	if(new_contactgroupmember==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contactgroup '%s' contact '%s'\n",grp->group_name,contact_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_contactgroupmember->contact_name=strdup(contact_name);
	if(new_contactgroupmember->contact_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contactgroup '%s' contact '%s' name\n",grp->group_name,contact_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_contactgroupmember);
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
service *add_service(char *host_name, char *description, char *check_period, int max_attempts, int parallelize, int accept_passive_checks, int check_interval, int retry_interval, int notification_interval, char *notification_period, int notify_recovery, int notify_unknown, int notify_warning, int notify_critical, int notify_flapping, int notifications_enabled, int is_volatile, char *event_handler, int event_handler_enabled, char *check_command, int checks_enabled, int flap_detection_enabled, double low_flap_threshold, double high_flap_threshold, int stalk_ok, int stalk_warning, int stalk_unknown, int stalk_critical, int process_perfdata, int failure_prediction_enabled, char *failure_prediction_options, int check_freshness, int freshness_threshold, int retain_status_information, int retain_nonstatus_information, int obsess_over_service){
	service *temp_service;
	service *new_service;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
	int x;
#endif

#ifdef DEBUG0
	printf("add_service() start\n");
#endif

	/* make sure we have everything we need */
	if(host_name==NULL || description==NULL || check_command==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service description, host name, or check command is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(host_name);
	strip(description);
	strip(check_command);
	strip(event_handler);
	strip(notification_period);
	strip(check_period);

	if(!strcmp(host_name,"") || !strcmp(description,"") || !strcmp(check_command,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service description, host name, or check command is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* make sure the host name isn't too long */
	if(strlen(host_name)>MAX_HOSTNAME_LENGTH-1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host name '%s' for service '%s' exceeds maximum length of %d characters\n",host_name,description,MAX_HOSTNAME_LENGTH-1);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* make sure there isn't a service by this name added already */
	temp_service=find_service(host_name,description);
	if(temp_service!=NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service '%s' on host '%s' has already been defined\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* make sure the service description isn't too long */
	if(strlen(description)>MAX_SERVICEDESC_LENGTH-1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Name of service '%s' on host '%s' exceeds maximum length of %d characters\n",description,host_name,MAX_SERVICEDESC_LENGTH-1);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(parallelize<0 || parallelize>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid parallelize value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(accept_passive_checks<0 || accept_passive_checks>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid accept_passive_checks value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(event_handler_enabled<0 || event_handler_enabled>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid event_handler_enabled value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(checks_enabled<0 || checks_enabled>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid checks_enabled value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(notifications_enabled<0 || notifications_enabled>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notifications_enabled value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(max_attempts<=0 || check_interval<0 || retry_interval<=0 || notification_interval<0){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid max_attempts, check_interval, retry_interval, or notification_interval value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_recovery<0 || notify_recovery>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_recovery value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_critical<0 || notify_critical>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_critical value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_flapping<0 || notify_flapping>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_flapping value '%d' for service '%s' on host '%s'\n",notify_flapping,description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(notify_recovery<0 || notify_recovery>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify_recovery value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(is_volatile<0 || is_volatile>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid is_volatile value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(flap_detection_enabled<0 || flap_detection_enabled>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid flap_detection_enabled value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(stalk_ok<0 || stalk_ok>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid stalk_ok value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(stalk_warning<0 || stalk_warning>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid stalk_warning value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(stalk_unknown<0 || stalk_unknown>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid stalk_unknown value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(stalk_critical<0 || stalk_critical>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid stalk_critical value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(process_perfdata<0 || process_perfdata>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid process_perfdata value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(failure_prediction_enabled<0 || failure_prediction_enabled>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid failure_prediction_enabled value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(check_freshness<0 || check_freshness>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid check_freshness value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(freshness_threshold<0){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid freshness_threshold value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(retain_status_information<0 || retain_status_information>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid retain_status_information value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(retain_nonstatus_information<0 || retain_nonstatus_information>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid retain_nonstatus_information value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	if(obsess_over_service<0 || obsess_over_service>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid obsess_over_service value for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }


	/* allocate memory */
	new_service=(service *)malloc(sizeof(service));
	if(new_service==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_service->host_name=strdup(host_name);
	if(new_service->host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' name\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_service);
		return NULL;
	        }
	new_service->description=strdup(description);
	if(new_service->description==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' description\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_service->host_name);
		free(new_service);
		return NULL;
	        }
	new_service->service_check_command=strdup(check_command);
	if(new_service->service_check_command==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' check command\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_service->description);
		free(new_service->host_name);
		free(new_service);
		return NULL;
	        }
	if(event_handler!=NULL && strcmp(event_handler,"")){
		new_service->event_handler=strdup(event_handler);
		if(new_service->event_handler==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' event handler\n",description,host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(new_service->service_check_command);
			free(new_service->description);
			free(new_service->host_name);
			free(new_service);
			return NULL;
		        }
	        }
	else
		new_service->event_handler=NULL;
	if(notification_period!=NULL && strcmp(notification_period,"")){
		new_service->notification_period=strdup(notification_period);
		if(new_service->notification_period==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' notification period\n",description,host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			if(new_service->event_handler!=NULL)
				free(new_service->event_handler);
			free(new_service->service_check_command);
			free(new_service->description);
			free(new_service->host_name);
			free(new_service);
			return NULL;
		        }
	        }
	else
		new_service->notification_period=NULL;
	if(check_period!=NULL && strcmp(check_period,"")){
		new_service->check_period=strdup(check_period);
		if(new_service->check_period==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' check period\n",description,host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			if(new_service->notification_period!=NULL)
				free(new_service->notification_period);
			if(new_service->event_handler!=NULL)
				free(new_service->event_handler);
			free(new_service->service_check_command);
			free(new_service->description);
			free(new_service->host_name);
			free(new_service);
			return NULL;
		        }
	        }
	else
		new_service->check_period=NULL;
	if(failure_prediction_options!=NULL && strcmp(failure_prediction_options,"")){
		new_service->failure_prediction_options=strdup(failure_prediction_options);
		if(new_service->failure_prediction_options==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' failure prediction options\n",description,host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			if(new_service->check_period!=NULL)
				free(new_service->check_period);
			if(new_service->notification_period!=NULL)
				free(new_service->notification_period);
			if(new_service->event_handler!=NULL)
				free(new_service->event_handler);
			free(new_service->service_check_command);
			free(new_service->description);
			free(new_service->host_name);
			free(new_service);
			return NULL;
		        }
	        }
	else
		new_service->failure_prediction_options=NULL;


	new_service->contact_groups=NULL;
	new_service->check_interval=check_interval;
	new_service->retry_interval=retry_interval;
	new_service->max_attempts=max_attempts;
	new_service->parallelize=(parallelize>0)?TRUE:FALSE;
	new_service->notification_interval=notification_interval;
	new_service->notify_on_unknown=(notify_unknown>0)?TRUE:FALSE;
	new_service->notify_on_warning=(notify_warning>0)?TRUE:FALSE;
	new_service->notify_on_critical=(notify_critical>0)?TRUE:FALSE;
	new_service->notify_on_recovery=(notify_recovery>0)?TRUE:FALSE;
	new_service->notify_on_flapping=(notify_flapping>0)?TRUE:FALSE;
	new_service->is_volatile=(is_volatile>0)?TRUE:FALSE;
	new_service->flap_detection_enabled=(flap_detection_enabled>0)?TRUE:FALSE;
	new_service->low_flap_threshold=low_flap_threshold;
	new_service->high_flap_threshold=high_flap_threshold;
	new_service->stalk_on_ok=(stalk_ok>0)?TRUE:FALSE;
	new_service->stalk_on_warning=(stalk_warning>0)?TRUE:FALSE;
	new_service->stalk_on_unknown=(stalk_unknown>0)?TRUE:FALSE;
	new_service->stalk_on_critical=(stalk_critical>0)?TRUE:FALSE;
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
	new_service->last_state=STATE_OK;
	new_service->last_hard_state=STATE_OK;
	/* initial state type changed from SOFT_STATE on 6/17/03 - shouldn't this have been HARD_STATE all along? */
	new_service->state_type=HARD_STATE;
	new_service->host_problem_at_last_check=FALSE;
#ifdef REMOVED_041403
	new_service->no_recovery_notification=FALSE;
#endif
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

	/* allocate new plugin output buffer */
	new_service->plugin_output=(char *)malloc(MAX_PLUGINOUTPUT_LENGTH);
	if(new_service->plugin_output==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' plugin output buffer\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);

		if(new_service->failure_prediction_options!=NULL)
			free(new_service->failure_prediction_options);
		if(new_service->notification_period!=NULL)
			free(new_service->notification_period);
		if(new_service->event_handler!=NULL)
			free(new_service->event_handler);
		free(new_service->service_check_command);
		free(new_service->description);
		free(new_service->host_name);
		free(new_service);
		return NULL;
	        }
	strcpy(new_service->plugin_output,"(Service assumed to be ok)");

	/* allocate new performance data buffer */
	new_service->perf_data=(char *)malloc(MAX_PLUGINOUTPUT_LENGTH);
	if(new_service->perf_data==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' performance data buffer\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);

		if(new_service->failure_prediction_options!=NULL)
			free(new_service->failure_prediction_options);
		if(new_service->notification_period!=NULL)
			free(new_service->notification_period);
		if(new_service->event_handler!=NULL)
			free(new_service->event_handler);
		free(new_service->service_check_command);
		free(new_service->description);
		free(new_service->host_name);
		free(new_service->plugin_output);
		free(new_service);
		return NULL;
	        }
	strcpy(new_service->perf_data,"");

#endif

	new_service->next=NULL;
	new_service->nexthash=NULL;

	/* add new service to service chained hash list */
	if(!add_service_to_hashlist(new_service)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service list to add new service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);

		if(new_service->perf_data!=NULL)
			free(new_service->perf_data);
		if(new_service->plugin_output)
			free(new_service->plugin_output);
#endif
		if(new_service->failure_prediction_options!=NULL)
			free(new_service->failure_prediction_options);
		if(new_service->notification_period!=NULL)
			free(new_service->notification_period);
		if(new_service->event_handler!=NULL)
			free(new_service->event_handler);
		if(new_service->service_check_command)
			free(new_service->service_check_command);
		if(new_service->description)
			free(new_service->description);
		if(new_service->host_name)
			free(new_service->host_name);
		free(new_service);
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
	contactgroupsmember *new_contactgroupsmember;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_contactgroup_to_service() start\n");
#endif

	/* bail out if we weren't given the data we need */
	if(svc==NULL || group_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service or contactgroup name is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(group_name);

	if(!strcmp(group_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contactgroup name is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for the contactgroups member */
	new_contactgroupsmember=malloc(sizeof(contactgroupsmember));
	if(new_contactgroupsmember==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact group '%s' for service '%s' on host '%s'\n",group_name,svc->description,svc->host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_contactgroupsmember->group_name=strdup(group_name);
	if(new_contactgroupsmember->group_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact group '%s' name for service '%s' on host '%s'\n",group_name,svc->description,svc->host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_contactgroupsmember);
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




/* add a new command to the list in memory */
command *add_command(char *name,char *value){
	command *new_command;
	command *temp_command;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_command() start\n");
#endif

	/* make sure we have the data we need */
	if(name==NULL || value==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Command name of command line is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(name);
	strip(value);

	if(!strcmp(name,"") || !strcmp(value,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Command name of command line is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* make sure there isn't a command by this name added already */
	temp_command=find_command(name);
	if(temp_command!=NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Command '%s' has already been defined\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for the new command */
	new_command=(command *)malloc(sizeof(command));
	if(new_command==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for command '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_command->name=strdup(name);
	if(new_command->name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for command '%s' name\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_command);
		return NULL;
	        }
	new_command->command_line=strdup(value);
	if(new_command->command_line==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for command '%s' alias\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_command->name);
		free(new_command);
		return NULL;
	        }

	new_command->next=NULL;
	new_command->nexthash=NULL;

	/* add new command to command chained hash list */
	if(!add_command_to_hashlist(new_command)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for command list to add command '%s'\n",name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_command->name);
		free(new_command);
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
	serviceescalation *new_serviceescalation;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_serviceescalation() start\n");
#endif

	/* make sure we have the data we need */
	if(host_name==NULL || description==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service escalation host name or description is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(host_name);
	strip(description);

	if(!strcmp(host_name,"") || !strcmp(description,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service escalation host name or description is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* check options */
	if(escalate_on_warning<0 || escalate_on_warning>1 || escalate_on_unknown<0 || escalate_on_unknown>1 || escalate_on_critical<0 || escalate_on_critical>1 || escalate_on_recovery<0 || escalate_on_recovery>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid escalation options in service '%s' on host '%s' escalation\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for a new service escalation entry */
	new_serviceescalation=malloc(sizeof(serviceescalation));
	if(new_serviceescalation==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' escalation\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_serviceescalation->host_name=strdup(host_name);
	if(new_serviceescalation->host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' escalation host name\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_serviceescalation);
		return NULL;
	        }
	new_serviceescalation->description=strdup(description);
	if(new_serviceescalation->description==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' escalation description\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_serviceescalation->host_name);
		free(new_serviceescalation);
		return NULL;
	        }
	if(escalation_period==NULL)
		new_serviceescalation->escalation_period=NULL;
	else{
		new_serviceescalation->escalation_period=strdup(escalation_period);
		if(new_serviceescalation->escalation_period==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' escalation period\n",description,host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(new_serviceescalation->host_name);
			free(new_serviceescalation->description);
			free(new_serviceescalation);
			return NULL;
		        }
	        }


	new_serviceescalation->first_notification=first_notification;
	new_serviceescalation->last_notification=last_notification;
	new_serviceescalation->notification_interval=(notification_interval<=0)?0:notification_interval;
	new_serviceescalation->escalate_on_recovery=(escalate_on_recovery>0)?TRUE:FALSE;
	new_serviceescalation->escalate_on_warning=(escalate_on_warning>0)?TRUE:FALSE;
	new_serviceescalation->escalate_on_unknown=(escalate_on_unknown>0)?TRUE:FALSE;
	new_serviceescalation->escalate_on_critical=(escalate_on_critical>0)?TRUE:FALSE;
	new_serviceescalation->contact_groups=NULL;

	new_serviceescalation->next=NULL;
	new_serviceescalation->nexthash=NULL;

	/* add new serviceescalation to serviceescalation chained hash list */
	if(!add_serviceescalation_to_hashlist(new_serviceescalation)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for serviceescalation list to add escalation for service '%s' on host '%s'\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_serviceescalation->host_name);
		free(new_serviceescalation->description);
		free(new_serviceescalation->escalation_period);
		free(new_serviceescalation);
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
	contactgroupsmember *new_contactgroupsmember;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_contactgroup_to_serviceescalation() start\n");
#endif

	/* bail out if we weren't given the data we need */
	if(se==NULL || group_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service escalation or contactgroup name is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(group_name);

	if(!strcmp(group_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contactgroup name is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for the contactgroups member */
	new_contactgroupsmember=(contactgroupsmember *)malloc(sizeof(contactgroupsmember));
	if(new_contactgroupsmember==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact group '%s' for service '%s' on host '%s' escalation\n",group_name,se->description,se->host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_contactgroupsmember->group_name=strdup(group_name);
	if(new_contactgroupsmember->group_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact group '%s' name for service '%s' on host '%s' escalation\n",group_name,se->description,se->host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_contactgroupsmember);
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
	servicedependency *new_servicedependency;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_service_dependency() start\n");
#endif

	/* make sure we have what we need */
	if(dependent_host_name==NULL || dependent_service_description==NULL || host_name==NULL || service_description==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: NULL service description/host name in service dependency definition\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(dependent_host_name);
	strip(dependent_service_description);
	strip(host_name);
	strip(service_description);

	if(!strcmp(dependent_host_name,"") || !strcmp(dependent_service_description,"") || !strcmp(host_name,"") || !strcmp(service_description,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: NULL service description/host name in service dependency definition\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(fail_on_ok<0 || fail_on_ok>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid fail_on_ok value for service '%s' on host '%s' dependency definition\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(fail_on_warning<0 || fail_on_warning>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid fail_on_warning value for service '%s' on host '%s' dependency definition\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(fail_on_unknown<0 || fail_on_unknown>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid fail_on_unknown value for service '%s' on host '%s' dependency definition\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(fail_on_critical<0 || fail_on_critical>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid fail_on_critical value for service '%s' on host '%s' dependency definition\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(fail_on_pending<0 || fail_on_pending>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid fail_on_pending value for service '%s' on host '%s' dependency definition\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(inherits_parent<0 || inherits_parent>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid inherits_parent value for service '%s' on host '%s' dependency definition\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
                }

	/* allocate memory for a new service dependency entry */
	new_servicedependency=(servicedependency *)malloc(sizeof(servicedependency));
	if(new_servicedependency==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' dependency\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_servicedependency->dependent_host_name=strdup(dependent_host_name);
	if(new_servicedependency->dependent_host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' dependency\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_servicedependency);
		return NULL;
	        }
	new_servicedependency->dependent_service_description=strdup(dependent_service_description);
	if(new_servicedependency->dependent_service_description==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' dependency\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_servicedependency);
		free(new_servicedependency->dependent_host_name);
		return NULL;
	        }
	new_servicedependency->host_name=strdup(host_name);
	if(new_servicedependency->host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' dependency\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_servicedependency);
		free(new_servicedependency->dependent_host_name);
		free(new_servicedependency->dependent_service_description);
		return NULL;
	        }
	new_servicedependency->service_description=strdup(service_description);
	if(new_servicedependency->service_description==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' dependency\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_servicedependency);
		free(new_servicedependency->dependent_host_name);
		free(new_servicedependency->dependent_service_description);
		free(new_servicedependency->host_name);
		return NULL;
	        }

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

	new_servicedependency->next=NULL;
	new_servicedependency->nexthash=NULL;

	/* add new servicedependency to servicedependency chained hash list */
	if(!add_servicedependency_to_hashlist(new_servicedependency)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for servicedependency list to add dependency for service '%s' on host '%s'\n",dependent_service_description,dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_servicedependency->host_name);
		free(new_servicedependency->service_description);
		free(new_servicedependency->dependent_host_name);
		free(new_servicedependency->dependent_service_description);
		free(new_servicedependency);
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
	hostdependency *new_hostdependency;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_host_dependency() start\n");
#endif

	/* make sure we have what we need */
	if(dependent_host_name==NULL || host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: NULL host name in host dependency definition\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(dependent_host_name);
	strip(host_name);

	if(!strcmp(dependent_host_name,"") || !strcmp(host_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: NULL host name in host dependency definition\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(fail_on_up<0 || fail_on_up>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid fail_on_up value for host '%s' dependency definition\n",dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(fail_on_down<0 || fail_on_down>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid fail_on_down value for host '%s' dependency definition\n",dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(fail_on_unreachable<0 || fail_on_unreachable>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid fail_on_unreachable value for host '%s' dependency definition\n",dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(fail_on_pending<0 || fail_on_pending>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid fail_on_pending value for host '%s' dependency definition\n",dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for a new host dependency entry */
	new_hostdependency=(hostdependency *)malloc(sizeof(hostdependency));
	if(new_hostdependency==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' dependency\n",dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_hostdependency->dependent_host_name=strdup(dependent_host_name);
	if(new_hostdependency->dependent_host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' dependency\n",dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_hostdependency);
		return NULL;
	        }
	new_hostdependency->host_name=strdup(host_name);
	if(new_hostdependency->host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' dependency\n",dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_hostdependency);
		free(new_hostdependency->dependent_host_name);
		return NULL;
	        }

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

	new_hostdependency->next=NULL;
	new_hostdependency->nexthash=NULL;

	/* add new hostdependency to hostdependency chained hash list */
	if(!add_hostdependency_to_hashlist(new_hostdependency)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for hostdependency list to add dependency for host '%s'\n",dependent_host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_hostdependency->host_name);
		free(new_hostdependency->dependent_host_name);
		free(new_hostdependency);
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
	hostescalation *new_hostescalation;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_hostescalation() start\n");
#endif

	/* make sure we have the data we need */
	if(host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host escalation host name is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(host_name);

	if(!strcmp(host_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host escalation host name is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* check options */
	if(escalate_on_down<0 || escalate_on_down>1 || escalate_on_unreachable<0 || escalate_on_unreachable>1 || escalate_on_recovery<0 || escalate_on_recovery>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid escalation options in host '%s' escalation\n",host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for a new host escalation entry */
	new_hostescalation=malloc(sizeof(hostescalation));
	if(new_hostescalation==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' escalation\n",host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_hostescalation->host_name=strdup(host_name);
	if(new_hostescalation->host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' escalation host name\n",host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_hostescalation);
		return NULL;
	        }
	if(escalation_period==NULL)
		new_hostescalation->escalation_period=NULL;
	else{
		new_hostescalation->escalation_period=strdup(escalation_period);
		if(new_hostescalation->escalation_period==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' escalation period\n",host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(new_hostescalation->host_name);
			free(new_hostescalation);
			return NULL;
		        }
	        }

	new_hostescalation->first_notification=first_notification;
	new_hostescalation->last_notification=last_notification;
	new_hostescalation->notification_interval=(notification_interval<=0)?0:notification_interval;
	new_hostescalation->escalate_on_recovery=(escalate_on_recovery>0)?TRUE:FALSE;
	new_hostescalation->escalate_on_down=(escalate_on_down>0)?TRUE:FALSE;
	new_hostescalation->escalate_on_unreachable=(escalate_on_unreachable>0)?TRUE:FALSE;
	new_hostescalation->contact_groups=NULL;

	new_hostescalation->next=NULL;
	new_hostescalation->nexthash=NULL;

	/* add new hostescalation to hostescalation chained hash list */
	if(!add_hostescalation_to_hashlist(new_hostescalation)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for hostescalation list to add escalation for host '%s'\n",host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_hostescalation->host_name);
		free(new_hostescalation->escalation_period);
		free(new_hostescalation);
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
	contactgroupsmember *new_contactgroupsmember;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_contactgroup_to_hostescalation() start\n");
#endif

	/* bail out if we weren't given the data we need */
	if(he==NULL || group_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host escalation or contactgroup name is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	strip(group_name);

	if(!strcmp(group_name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contactgroup name is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for the contactgroups member */
	new_contactgroupsmember=(contactgroupsmember *)malloc(sizeof(contactgroupsmember));
	if(new_contactgroupsmember==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact group '%s' for host '%s' escalation\n",group_name,he->host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
	new_contactgroupsmember->group_name=strdup(group_name);
	if(new_contactgroupsmember->group_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact group '%s' name for host '%s' escalation\n",group_name,he->host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_contactgroupsmember);
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
				
	new_hostextinfo->host_name=strdup(host_name);
	if(new_hostextinfo->host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host '%s' extended info.\n",host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_hostextinfo);
		return NULL;
	        }

	if(notes==NULL || !strcmp(notes,""))
		new_hostextinfo->notes=NULL;
	else{
		new_hostextinfo->notes=strdup(notes);
		if(new_hostextinfo->notes==NULL){
			free(new_hostextinfo->host_name);
			free(new_hostextinfo);
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
		new_hostextinfo->notes_url=strdup(notes_url);
		if(new_hostextinfo->notes_url==NULL){
			free(new_hostextinfo->notes);
			free(new_hostextinfo->host_name);
			free(new_hostextinfo);
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
		new_hostextinfo->action_url=strdup(action_url);
		if(new_hostextinfo->action_url==NULL){
			free(new_hostextinfo->notes_url);
			free(new_hostextinfo->notes);
			free(new_hostextinfo->host_name);
			free(new_hostextinfo);
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
		new_hostextinfo->icon_image=strdup(icon_image);
		if(new_hostextinfo->icon_image==NULL){
			free(new_hostextinfo->action_url);
			free(new_hostextinfo->notes_url);
			free(new_hostextinfo->notes);
			free(new_hostextinfo->host_name);
			free(new_hostextinfo);
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
		new_hostextinfo->vrml_image=strdup(vrml_image);
		if(new_hostextinfo->vrml_image==NULL){
			free(new_hostextinfo->icon_image);
			free(new_hostextinfo->action_url);
			free(new_hostextinfo->notes_url);
			free(new_hostextinfo->notes);
			free(new_hostextinfo->host_name);
			free(new_hostextinfo);
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
		new_hostextinfo->statusmap_image=strdup(statusmap_image);
		if(new_hostextinfo->statusmap_image==NULL){
			free(new_hostextinfo->vrml_image);
			free(new_hostextinfo->icon_image);
			free(new_hostextinfo->action_url);
			free(new_hostextinfo->notes_url);
			free(new_hostextinfo->notes);
			free(new_hostextinfo->host_name);
			free(new_hostextinfo);
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
		new_hostextinfo->icon_image_alt=strdup(icon_image_alt);
		if(new_hostextinfo->icon_image_alt==NULL){
			free(new_hostextinfo->statusmap_image);
			free(new_hostextinfo->vrml_image);
			free(new_hostextinfo->icon_image);
			free(new_hostextinfo->action_url);
			free(new_hostextinfo->notes_url);
			free(new_hostextinfo->notes);
			free(new_hostextinfo->host_name);
			free(new_hostextinfo);
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
		free(new_hostextinfo->statusmap_image);
		free(new_hostextinfo->vrml_image);
		free(new_hostextinfo->icon_image);
		free(new_hostextinfo->icon_image_alt);
		free(new_hostextinfo->action_url);
		free(new_hostextinfo->notes_url);
		free(new_hostextinfo->notes);
		free(new_hostextinfo->host_name);
		free(new_hostextinfo);
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
	


/* adds an extended service info structure to the list in memory */
serviceextinfo * add_serviceextinfo(char *host_name, char *description, char *notes, char *notes_url, char *action_url, char *icon_image, char *icon_image_alt){
	serviceextinfo *new_serviceextinfo;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("add_serviceextinfo() start\n");
#endif

	/* make sure we have what we need */
	if((host_name==NULL || !strcmp(host_name,"")) || (description==NULL || !strcmp(description,""))){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host name or service description is NULL\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	/* allocate memory for a new data structure */
	new_serviceextinfo=(serviceextinfo *)malloc(sizeof(serviceextinfo));
	if(new_serviceextinfo==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' extended info.\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
				
	new_serviceextinfo->host_name=strdup(host_name);
	if(new_serviceextinfo->host_name==NULL){
		free(new_serviceextinfo);
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' extended info.\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }
				
	new_serviceextinfo->description=strdup(description);
	if(new_serviceextinfo->description==NULL){
		free(new_serviceextinfo->host_name);
		free(new_serviceextinfo);
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' extended info.\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return NULL;
	        }

	if(notes==NULL || !strcmp(notes,""))
		new_serviceextinfo->notes=NULL;
	else{
		new_serviceextinfo->notes=strdup(notes);
		if(new_serviceextinfo->notes==NULL){
			free(new_serviceextinfo->description);
			free(new_serviceextinfo->host_name);
			free(new_serviceextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' extended info.\n",description,host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
		        }
	        }

	if(notes_url==NULL || !strcmp(notes_url,""))
		new_serviceextinfo->notes_url=NULL;
	else{
		new_serviceextinfo->notes_url=strdup(notes_url);
		if(new_serviceextinfo->notes_url==NULL){
			free(new_serviceextinfo->notes);
			free(new_serviceextinfo->description);
			free(new_serviceextinfo->host_name);
			free(new_serviceextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' extended info.\n",description,host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
		        }
	        }

	if(action_url==NULL || !strcmp(action_url,""))
		new_serviceextinfo->action_url=NULL;
	else{
		new_serviceextinfo->action_url=strdup(action_url);
		if(new_serviceextinfo->action_url==NULL){
			free(new_serviceextinfo->notes_url);
			free(new_serviceextinfo->notes);
			free(new_serviceextinfo->description);
			free(new_serviceextinfo->host_name);
			free(new_serviceextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' extended info.\n",description,host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
		        }
	        }

	if(icon_image==NULL || !strcmp(icon_image,""))
		new_serviceextinfo->icon_image=NULL;
	else{
		new_serviceextinfo->icon_image=strdup(icon_image);
		if(new_serviceextinfo->icon_image==NULL){
			free(new_serviceextinfo->notes);
			free(new_serviceextinfo->notes_url);
			free(new_serviceextinfo->action_url);
			free(new_serviceextinfo->description);
			free(new_serviceextinfo->host_name);
			free(new_serviceextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' extended info.\n",description,host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
	                }
	        }

	if(icon_image_alt==NULL || !strcmp(icon_image_alt,""))
		new_serviceextinfo->icon_image_alt=NULL;
	else{
		new_serviceextinfo->icon_image_alt=strdup(icon_image_alt);
		if(new_serviceextinfo->icon_image_alt==NULL){
			free(new_serviceextinfo->icon_image);
			free(new_serviceextinfo->notes);
			free(new_serviceextinfo->notes_url);
			free(new_serviceextinfo->action_url);
			free(new_serviceextinfo->description);
			free(new_serviceextinfo->host_name);
			free(new_serviceextinfo);
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service '%s' on host '%s' extended info.\n",description,host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return NULL;
		        }
	        }

	new_serviceextinfo->next=NULL;
	new_serviceextinfo->nexthash=NULL;

	/* add new serviceextinfo to serviceextinfo chained hash list */
	if(!add_serviceextinfo_to_hashlist(new_serviceextinfo)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for serviceextinfo list to add extended info for service '%s' on host '%s'.\n",description,host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(new_serviceextinfo->icon_image);
		free(new_serviceextinfo->icon_image_alt);
		free(new_serviceextinfo->notes_url);
		free(new_serviceextinfo->notes);
		free(new_serviceextinfo->action_url);
		free(new_serviceextinfo->description);
		free(new_serviceextinfo->host_name);
		free(new_serviceextinfo);
		return NULL;
	        }

#ifdef NSCORE
	/* serviceextinfo entries are sorted alphabetically for daemon, so add new items to tail of list */
	if(serviceextinfo_list==NULL){
		serviceextinfo_list=new_serviceextinfo;
		serviceextinfo_list_tail=serviceextinfo_list;
		}
	else{
		serviceextinfo_list_tail->next=new_serviceextinfo;
		serviceextinfo_list_tail=new_serviceextinfo;
		}
#else
	/* serviceextinfo entries are sorted in reverse for CGIs, so add new items to head of list */
	new_serviceextinfo->next=serviceextinfo_list;
	serviceextinfo_list=new_serviceextinfo;
#endif		

#ifdef DEBUG0
	printf("add_serviceextinfo() end\n");
#endif

	return new_serviceextinfo;
        }
	



/******************************************************************/
/******************** OBJECT SEARCH FUNCTIONS *********************/
/******************************************************************/

/* given a timeperiod name and a starting point, find a timeperiod from the list in memory */
timeperiod * find_timeperiod(char *name){
	timeperiod *temp_timeperiod;

#ifdef DEBUG0
	printf("find_timeperiod() start\n");
#endif

	if(name==NULL || timeperiod_hashlist==NULL)
		return NULL;

	for(temp_timeperiod=timeperiod_hashlist[hashfunc1(name,TIMEPERIOD_HASHSLOTS)];temp_timeperiod && compare_hashdata1(temp_timeperiod->name,name)<0;temp_timeperiod=temp_timeperiod->nexthash);

	if(temp_timeperiod && (compare_hashdata1(temp_timeperiod->name,name)==0))
		return temp_timeperiod;

#ifdef DEBUG0
	printf("find_timeperiod() end\n");
#endif

	/* we couldn't find a matching timeperiod */
	return NULL;
	}


/* given a host name, find it in the list in memory */
host * find_host(char *name){
	host *temp_host;

#ifdef DEBUG0
	printf("find_host() start\n");
#endif

	if(name==NULL || host_hashlist==NULL)
		return NULL;

	for(temp_host=host_hashlist[hashfunc1(name,HOST_HASHSLOTS)];temp_host && compare_hashdata1(temp_host->name,name)<0;temp_host=temp_host->nexthash);

	if(temp_host && (compare_hashdata1(temp_host->name,name)==0))
		return temp_host;

#ifdef DEBUG0
	printf("find_host() end\n");
#endif

	/* Couldn't find matching host */
	return NULL;
        }


/* find a hostgroup from the list in memory */
hostgroup * find_hostgroup(char *name){
	hostgroup *temp_hostgroup;

#ifdef DEBUG0
	printf("find_hostgroup() start\n");
#endif

	if(name==NULL || hostgroup_hashlist==NULL)
		return NULL;

	for(temp_hostgroup=hostgroup_hashlist[hashfunc1(name,HOSTGROUP_HASHSLOTS)];temp_hostgroup && compare_hashdata1(temp_hostgroup->group_name,name)<0;temp_hostgroup=temp_hostgroup->nexthash);

	if(temp_hostgroup && (compare_hashdata1(temp_hostgroup->group_name,name)==0))
		return temp_hostgroup;

#ifdef DEBUG0
	printf("find_hostgroup() end\n");
#endif

	/* we couldn't find a matching hostgroup */
	return NULL;
	}


#ifdef REMOVED_061803
/* find a member of a host group */
hostgroupmember * find_hostgroupmember(char *name,hostgroup *grp){
	hostgroupmember *temp_member;

#ifdef DEBUG0
	printf("find_hostgroupmember() start\n");
#endif

	if(name==NULL || grp==NULL)
		return NULL;

	temp_member=grp->members;
	while(temp_member!=NULL){

		/* we found a match */
		if(!strcmp(temp_member->host_name,name))
			return temp_member;

		temp_member=temp_member->next;
	        }
	

#ifdef DEBUG0
	printf("find_hostgroupmember() end\n");
#endif

	/* we couldn't find a matching member */
	return NULL;
        }
#endif


/* find a servicegroup from the list in memory */
servicegroup * find_servicegroup(char *name){
	servicegroup *temp_servicegroup;

#ifdef DEBUG0
	printf("find_servicegroup() start\n");
#endif

	if(name==NULL || servicegroup_hashlist==NULL)
		return NULL;

	for(temp_servicegroup=servicegroup_hashlist[hashfunc1(name,SERVICEGROUP_HASHSLOTS)];temp_servicegroup && compare_hashdata1(temp_servicegroup->group_name,name)<0;temp_servicegroup=temp_servicegroup->nexthash);

	if(temp_servicegroup && (compare_hashdata1(temp_servicegroup->group_name,name)==0))
		return temp_servicegroup;


#ifdef DEBUG0
	printf("find_servicegroup() end\n");
#endif

	/* we couldn't find a matching servicegroup */
	return NULL;
	}

#ifdef REMOVED_0618003
/* find a member of a service group */
servicegroupmember * find_servicegroupmember(char *host_name,char *svc_description,servicegroup *grp){
	servicegroupmember *temp_member;

#ifdef DEBUG0
	printf("find_servicegroupmember() start\n");
#endif

	if(host_name==NULL || svc_description==NULL || grp==NULL)
		return NULL;

	temp_member=grp->members;
	while(temp_member!=NULL){

		/* we found a match */
		if(!strcmp(temp_member->host_name,host_name) && !strcmp(temp_member->service_description,svc_description))
			return temp_member;

		temp_member=temp_member->next;
	        }
	

#ifdef DEBUG0
	printf("find_servicegroupmember() end\n");
#endif

	/* we couldn't find a matching member */
	return NULL;
        }
#endif


/* find a contact from the list in memory */
contact * find_contact(char *name){
	contact *temp_contact;

#ifdef DEBUG0
	printf("find_contact() start\n");
#endif

	if(name==NULL || contact_hashlist==NULL)
		return NULL;

	for(temp_contact=contact_hashlist[hashfunc1(name,CONTACT_HASHSLOTS)];temp_contact && compare_hashdata1(temp_contact->name,name)<0;temp_contact=temp_contact->nexthash);

	if(temp_contact && (compare_hashdata1(temp_contact->name,name)==0))
		return temp_contact;

#ifdef DEBUG0
	printf("find_contact() end\n");
#endif

	/* we couldn't find a matching contact */
	return NULL;
	}


/* find a contact group from the list in memory */
contactgroup * find_contactgroup(char *name){
	contactgroup *temp_contactgroup;

#ifdef DEBUG0
	printf("find_contactgroup() start\n");
#endif

	if(name==NULL || contactgroup_hashlist==NULL)
		return NULL;

	for(temp_contactgroup=contactgroup_hashlist[hashfunc1(name,CONTACTGROUP_HASHSLOTS)];temp_contactgroup && compare_hashdata1(temp_contactgroup->group_name,name)<0;temp_contactgroup=temp_contactgroup->nexthash);

	if(temp_contactgroup && (compare_hashdata1(temp_contactgroup->group_name,name)==0))
		return temp_contactgroup;

#ifdef DEBUG0
	printf("find_contactgroup() end\n");
#endif

	/* we couldn't find a matching contactgroup */
	return NULL;
	}


/* find the corresponding member of a contact group */
contactgroupmember * find_contactgroupmember(char *name,contactgroup *grp){
	contactgroupmember *temp_member;

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
	command *temp_command;

#ifdef DEBUG0
	printf("find_command() start\n");
#endif

	if(name==NULL || command_hashlist==NULL)
		return NULL;

	for(temp_command=command_hashlist[hashfunc1(name,COMMAND_HASHSLOTS)];temp_command && compare_hashdata1(temp_command->name,name)<0;temp_command=temp_command->nexthash);

	if(temp_command && (compare_hashdata1(temp_command->name,name)==0))
		return temp_command;

#ifdef DEBUG0
	printf("find_command() end\n");
#endif

	/* we couldn't find a matching command */
	return NULL;
        }


/* given a host/service name, find the service in the list in memory */
service * find_service(char *host_name,char *svc_desc){
	service *temp_service;

#ifdef DEBUG0
	printf("find_service() start\n");
#endif

	if(host_name==NULL || svc_desc==NULL || service_hashlist==NULL)
		return NULL;

	for(temp_service=service_hashlist[hashfunc2(host_name,svc_desc,SERVICE_HASHSLOTS)];temp_service && compare_hashdata2(temp_service->host_name,temp_service->description,host_name,svc_desc)<0;temp_service=temp_service->nexthash);

	if(temp_service && (compare_hashdata2(temp_service->host_name,temp_service->description,host_name,svc_desc)==0))
		return temp_service;

#ifdef DEBUG0
	printf("find_service() end\n");
#endif

	/* we couldn't find a matching service */
	return NULL;
        }



/* find the extended information for a given host */
hostextinfo * find_hostextinfo(char *host_name){
	hostextinfo *temp_hostextinfo;

#ifdef DEBUG0
	printf("find_hostextinfo() start\n");
#endif

	if(host_name==NULL || hostextinfo_hashlist==NULL)
		return NULL;

	for(temp_hostextinfo=hostextinfo_hashlist[hashfunc1(host_name,HOSTEXTINFO_HASHSLOTS)];temp_hostextinfo && compare_hashdata1(temp_hostextinfo->host_name,host_name)<0;temp_hostextinfo=temp_hostextinfo->nexthash);

	if(temp_hostextinfo && (compare_hashdata1(temp_hostextinfo->host_name,host_name)==0))
		return temp_hostextinfo;

#ifdef DEBUG0
	printf("find_hostextinfo() end\n");
#endif

	/* we couldn't find a matching extended host info object */
	return NULL;
        }


/* find the extended information for a given service */
serviceextinfo * find_serviceextinfo(char *host_name, char *description){
	serviceextinfo *temp_serviceextinfo;

#ifdef DEBUG0
	printf("find_serviceextinfo() start\n");
#endif

	if(host_name==NULL || description==NULL || serviceextinfo_hashlist==NULL)
		return NULL;

	for(temp_serviceextinfo=serviceextinfo_hashlist[hashfunc2(host_name,description,SERVICEEXTINFO_HASHSLOTS)];temp_serviceextinfo && compare_hashdata2(temp_serviceextinfo->host_name,temp_serviceextinfo->description,host_name,description)<0;temp_serviceextinfo=temp_serviceextinfo->nexthash);

	if(temp_serviceextinfo && (compare_hashdata2(temp_serviceextinfo->host_name,temp_serviceextinfo->description,host_name,description)==0))
		return temp_serviceextinfo;

#ifdef DEBUG0
	printf("find_serviceextinfo() end\n");
#endif

	/* we couldn't find a matching extended service info object */
	return NULL;
        }




/******************************************************************/
/******************* OBJECT TRAVERSAL FUNCTIONS *******************/
/******************************************************************/

hostescalation *get_first_hostescalation_by_host(char *host_name){

	return get_next_hostescalation_by_host(host_name,NULL);
        }


hostescalation *get_next_hostescalation_by_host(char *host_name, hostescalation *start){
	hostescalation *temp_hostescalation;

	if(host_name==NULL || hostescalation_hashlist==NULL)
		return NULL;

	if(start==NULL)
		temp_hostescalation=hostescalation_hashlist[hashfunc1(host_name,HOSTESCALATION_HASHSLOTS)];
	else
		temp_hostescalation=start->nexthash;

	for(;temp_hostescalation && compare_hashdata1(temp_hostescalation->host_name,host_name)<0;temp_hostescalation=temp_hostescalation->nexthash);

	if(temp_hostescalation && compare_hashdata1(temp_hostescalation->host_name,host_name)==0)
		return temp_hostescalation;

	return NULL;
        }


serviceescalation *get_first_serviceescalation_by_service(char *host_name, char *svc_description){

	return get_next_serviceescalation_by_service(host_name,svc_description,NULL);
        }


serviceescalation *get_next_serviceescalation_by_service(char *host_name, char *svc_description, serviceescalation *start){
	serviceescalation *temp_serviceescalation;

	if(host_name==NULL || svc_description==NULL || serviceescalation_hashlist==NULL)
		return NULL;

	if(start==NULL)
		temp_serviceescalation=serviceescalation_hashlist[hashfunc2(host_name,svc_description,SERVICEESCALATION_HASHSLOTS)];
	else
		temp_serviceescalation=start->nexthash;

	for(;temp_serviceescalation && compare_hashdata2(temp_serviceescalation->host_name,temp_serviceescalation->description,host_name,svc_description)<0;temp_serviceescalation=temp_serviceescalation->nexthash);

	if(temp_serviceescalation && compare_hashdata2(temp_serviceescalation->host_name,temp_serviceescalation->description,host_name,svc_description)==0)
		return temp_serviceescalation;

	return NULL;
        }


hostdependency *get_first_hostdependency_by_dependent_host(char *host_name){

	return get_next_hostdependency_by_dependent_host(host_name,NULL);
        }


hostdependency *get_next_hostdependency_by_dependent_host(char *host_name, hostdependency *start){
	hostdependency *temp_hostdependency;

	if(host_name==NULL || hostdependency_hashlist==NULL)
		return NULL;

	if(start==NULL)
		temp_hostdependency=hostdependency_hashlist[hashfunc1(host_name,HOSTDEPENDENCY_HASHSLOTS)];
	else
		temp_hostdependency=start->nexthash;

	for(;temp_hostdependency && compare_hashdata1(temp_hostdependency->dependent_host_name,host_name)<0;temp_hostdependency=temp_hostdependency->nexthash);

	if(temp_hostdependency && compare_hashdata1(temp_hostdependency->dependent_host_name,host_name)==0)
		return temp_hostdependency;

	return NULL;
        }


servicedependency *get_first_servicedependency_by_dependent_service(char *host_name, char *svc_description){

	return get_next_servicedependency_by_dependent_service(host_name,svc_description,NULL);
        }


servicedependency *get_next_servicedependency_by_dependent_service(char *host_name, char *svc_description, servicedependency *start){
	servicedependency *temp_servicedependency;

	if(host_name==NULL || svc_description==NULL || servicedependency_hashlist==NULL)
		return NULL;

	if(start==NULL)
		temp_servicedependency=servicedependency_hashlist[hashfunc2(host_name,svc_description,SERVICEDEPENDENCY_HASHSLOTS)];
	else
		temp_servicedependency=start->nexthash;

	for(;temp_servicedependency && compare_hashdata2(temp_servicedependency->dependent_host_name,temp_servicedependency->dependent_service_description,host_name,svc_description)<0;temp_servicedependency=temp_servicedependency->nexthash);

	if(temp_servicedependency && compare_hashdata2(temp_servicedependency->dependent_host_name,temp_servicedependency->dependent_service_description,host_name,svc_description)==0)
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
	hostsmember *temp_hostsmember;

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
	host *temp_host;

	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(is_host_immediate_child_of_host(hst,temp_host)==TRUE)
			children++;
		}

	return children;
	}


/* returns a count of the total children for a given host */
int number_of_total_child_hosts(host *hst){
	int children=0;
	host *temp_host;

	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(is_host_immediate_child_of_host(hst,temp_host)==TRUE)
			children+=number_of_total_child_hosts(temp_host)+1;
		}

	return children;
	}


/* get the number of immediate parent hosts for a given host */
int number_of_immediate_parent_hosts(host *hst){
	int parents=0;
	host *temp_host;

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
	host *temp_host;

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
	hostgroupmember *temp_hostgroupmember;

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
	servicegroupmember *temp_servicegroupmember;

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
	servicegroupmember *temp_servicegroupmember;

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
	contactgroupmember *temp_contactgroupmember;

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
	hostgroupmember *temp_hostgroupmember;
	host *temp_host;

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
	servicegroupmember *temp_servicegroupmember;
	service *temp_service;

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
	contactgroupsmember *temp_contactgroupsmember;
	contactgroup *temp_contactgroup;
	
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
	contactgroupsmember *temp_contactgroupsmember;
	contactgroup *temp_contactgroup;
	hostescalation *temp_hostescalation;


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
	contactgroupsmember *temp_contactgroupsmember;
	contactgroup *temp_contactgroup;

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
	serviceescalation *temp_serviceescalation;
	contactgroupsmember *temp_contactgroupsmember;
	contactgroup *temp_contactgroup;

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
	host *temp_host;

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
	servicedependency *temp_sd;

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
	hostdependency *temp_hd;

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
	int day;
	int i;

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
				free(this_timerange);
			        }
		        }

		next_timeperiod=this_timeperiod->next;
		free(this_timeperiod->name);
		free(this_timeperiod->alias);
		free(this_timeperiod);
		this_timeperiod=next_timeperiod;
		}

	/* free hashlist and reset pointers */
	free(timeperiod_hashlist);
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
			free(this_hostsmember->host_name);
			free(this_hostsmember);
			this_hostsmember=next_hostsmember;
			}

		/* free memory for contact groups */
		this_contactgroupsmember=this_host->contact_groups;
		while(this_contactgroupsmember!=NULL){
			next_contactgroupsmember=this_contactgroupsmember->next;
			free(this_contactgroupsmember->group_name);
			free(this_contactgroupsmember);
			this_contactgroupsmember=next_contactgroupsmember;
			}

		free(this_host->name);
		free(this_host->alias);
		free(this_host->address);

#ifdef NSCORE
		free(this_host->plugin_output);
		free(this_host->perf_data);
#endif
		free(this_host->check_period);
		free(this_host->host_check_command);
		free(this_host->event_handler);
		free(this_host->failure_prediction_options);
		free(this_host->notification_period);
		free(this_host);
		this_host=next_host;
	        }

	/* free hashlist and reset pointers */
	free(host_hashlist);
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
			free(this_hostgroupmember->host_name);
			free(this_hostgroupmember);
			this_hostgroupmember=next_hostgroupmember;
		        }

		next_hostgroup=this_hostgroup->next;
		free(this_hostgroup->group_name);
		free(this_hostgroup->alias);
		free(this_hostgroup);
		this_hostgroup=next_hostgroup;
		}

	/* free hashlist and reset pointers */
	free(hostgroup_hashlist);
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
			free(this_servicegroupmember->host_name);
			free(this_servicegroupmember->service_description);
			free(this_servicegroupmember);
			this_servicegroupmember=next_servicegroupmember;
		        }

		next_servicegroup=this_servicegroup->next;
		free(this_servicegroup->group_name);
		free(this_servicegroup->alias);
		free(this_servicegroup);
		this_servicegroup=next_servicegroup;
		}

	/* free hashlist and reset pointers */
	free(servicegroup_hashlist);
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
				free(this_commandsmember->command);
			free(this_commandsmember);
			this_commandsmember=next_commandsmember;
		        }

		/* free memory for the service notification commands */
		this_commandsmember=this_contact->service_notification_commands;
		while(this_commandsmember!=NULL){
			next_commandsmember=this_commandsmember->next;
			if(this_commandsmember->command!=NULL)
				free(this_commandsmember->command);
			free(this_commandsmember);
			this_commandsmember=next_commandsmember;
		        }

		next_contact=this_contact->next;
		free(this_contact->name);
		free(this_contact->alias);
		free(this_contact->email);
		free(this_contact->pager);
		for(i=0;i<MAX_CONTACT_ADDRESSES;i++)
			free(this_contact->address[i]);
		free(this_contact->host_notification_period);
		free(this_contact->service_notification_period);
		free(this_contact);
		this_contact=next_contact;
		}

	/* free hashlist and reset pointers */
	free(contact_hashlist);
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
			free(this_contactgroupmember->contact_name);
			free(this_contactgroupmember);
			this_contactgroupmember=next_contactgroupmember;
		        }

		next_contactgroup=this_contactgroup->next;
		free(this_contactgroup->group_name);
		free(this_contactgroup->alias);
		free(this_contactgroup);
		this_contactgroup=next_contactgroup;
		}

	/* free hashlist and reset pointers */
	free(contactgroup_hashlist);
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
			free(this_contactgroupsmember->group_name);
			free(this_contactgroupsmember);
			this_contactgroupsmember=next_contactgroupsmember;
	                }

		free(this_service->host_name);
		free(this_service->description);
		free(this_service->service_check_command);
#ifdef NSCORE
		free(this_service->plugin_output);
		free(this_service->perf_data);
#endif
		free(this_service->notification_period);
		free(this_service->check_period);
		free(this_service->event_handler);
		free(this_service->failure_prediction_options);
		free(this_service);
		this_service=next_service;
	        }

	/* free hashlist and reset pointers */
	free(service_hashlist);
	service_hashlist=NULL;
	service_list=NULL;

#ifdef DEBUG1
	printf("\tservice_list freed\n");
#endif

	/* free memory for the command list */
	this_command=command_list;
	while(this_command!=NULL){
		next_command=this_command->next;
		free(this_command->name);
		free(this_command->command_line);
		free(this_command);
		this_command=next_command;
	        }

	/* free hashlist and reset pointers */
	free(command_hashlist);
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
			free(this_contactgroupsmember->group_name);
			free(this_contactgroupsmember);
			this_contactgroupsmember=next_contactgroupsmember;
		        }

		next_serviceescalation=this_serviceescalation->next;
		free(this_serviceescalation->host_name);
		free(this_serviceescalation->description);
		free(this_serviceescalation->escalation_period);
		free(this_serviceescalation);
		this_serviceescalation=next_serviceescalation;
	        }

	/* free hashlist and reset pointers */
	free(serviceescalation_hashlist);
	serviceescalation_hashlist=NULL;
	serviceescalation_list=NULL;

#ifdef DEBUG1
	printf("\tserviceescalation_list freed\n");
#endif

	/* free memory for the service dependency list */
	this_servicedependency=servicedependency_list;
	while(this_servicedependency!=NULL){
		next_servicedependency=this_servicedependency->next;
		free(this_servicedependency->dependent_host_name);
		free(this_servicedependency->dependent_service_description);
		free(this_servicedependency->host_name);
		free(this_servicedependency->service_description);
		free(this_servicedependency);
		this_servicedependency=next_servicedependency;
	        }

	/* free hashlist and reset pointers */
	free(servicedependency_hashlist);
	servicedependency_hashlist=NULL;
	servicedependency_list=NULL;

#ifdef DEBUG1
	printf("\tservicedependency_list freed\n");
#endif

	/* free memory for the host dependency list */
	this_hostdependency=hostdependency_list;
	while(this_hostdependency!=NULL){
		next_hostdependency=this_hostdependency->next;
		free(this_hostdependency->dependent_host_name);
		free(this_hostdependency->host_name);
		free(this_hostdependency);
		this_hostdependency=next_hostdependency;
	        }

	/* free hashlist and reset pointers */
	free(hostdependency_hashlist);
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
			free(this_contactgroupsmember->group_name);
			free(this_contactgroupsmember);
			this_contactgroupsmember=next_contactgroupsmember;
		        }

		next_hostescalation=this_hostescalation->next;
		free(this_hostescalation->host_name);
		free(this_hostescalation->escalation_period);
		free(this_hostescalation);
		this_hostescalation=next_hostescalation;
	        }

	/* free hashlist and reset pointers */
	free(hostescalation_hashlist);
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
	serviceextinfo *this_serviceextinfo=NULL;
	serviceextinfo *next_serviceextinfo=NULL;

#ifdef DEBUG0
	printf("free_extended_data() start\n");
#endif

	/* free memory for the extended host info list */
	for(this_hostextinfo=hostextinfo_list;this_hostextinfo!=NULL;this_hostextinfo=next_hostextinfo){
		next_hostextinfo=this_hostextinfo->next;
		free(this_hostextinfo->host_name);
		free(this_hostextinfo->notes);
		free(this_hostextinfo->notes_url);
		free(this_hostextinfo->action_url);
		free(this_hostextinfo->icon_image);
		free(this_hostextinfo->vrml_image);
		free(this_hostextinfo->statusmap_image);
		free(this_hostextinfo->icon_image_alt);
		free(this_hostextinfo);
	        }

#ifdef DEBUG1
	printf("\thostextinfo_list freed\n");
#endif

	/* free hashlist and reset pointers */
	free(hostextinfo_hashlist);
	hostextinfo_hashlist=NULL;
	hostextinfo_list=NULL;

	/* free memory for the extended service info list */
	for(this_serviceextinfo=serviceextinfo_list;this_serviceextinfo!=NULL;this_serviceextinfo=next_serviceextinfo){
		next_serviceextinfo=this_serviceextinfo->next;
		free(this_serviceextinfo->host_name);
		free(this_serviceextinfo->description);
		free(this_serviceextinfo->notes);
		free(this_serviceextinfo->notes_url);
		free(this_serviceextinfo->action_url);
		free(this_serviceextinfo->icon_image);
		free(this_serviceextinfo->icon_image_alt);
		free(this_serviceextinfo);
	        }

#ifdef DEBUG1
	printf("\tserviceextinfo_list freed\n");
#endif

	/* free hashlist and reset pointers */
	free(serviceextinfo_hashlist);
	serviceextinfo_hashlist=NULL;
	serviceextinfo_list=NULL;


#ifdef DEBUG0
	printf("free_extended_data() end\n");
#endif

	return OK;
        }

