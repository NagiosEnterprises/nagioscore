/*****************************************************************************
 *
 * AUTH.C - Authorization utilities for Nagios CGIs
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-05-2002
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

#include "../common/common.h"
#include "../common/config.h"
#include "../common/locations.h"
#include "../common/objects.h"

#include "cgiutils.h"
#include "auth.h"

extern char            main_config_file[MAX_FILENAME_LENGTH];

extern hostgroup       *hostgroup_list;

extern int             use_authentication;

extern int             hosts_have_been_read;
extern int             hostgroups_have_been_read;
extern int             contactgroups_have_been_read;
extern int             contacts_have_been_read;
extern int             services_have_been_read;
extern int             serviceescalations_have_been_read;
extern int             hostescalations_have_been_read;



/* get current authentication information */
int get_authentication_information(authdata *authinfo){
	FILE *fp;
	char input_buffer[MAX_INPUT_BUFFER];
	char *temp_ptr;
	int needed_options;

	if(authinfo==NULL)
		return ERROR;

	/* make sure we have read in all the configuration information we need for the authentication routines... */
	needed_options=0;
	if(hosts_have_been_read==FALSE)
		needed_options|=READ_HOSTS;
	if(hostgroups_have_been_read==FALSE)
		needed_options|=READ_HOSTGROUPS;
	if(contactgroups_have_been_read==FALSE)
		needed_options|=READ_CONTACTGROUPS;
	if(contacts_have_been_read==FALSE)
		needed_options|=READ_CONTACTS;
	if(services_have_been_read==FALSE)
		needed_options|=READ_SERVICES;
	if(serviceescalations_have_been_read==FALSE)
		needed_options|=READ_SERVICEESCALATIONS;
	if(hostescalations_have_been_read==FALSE)
		needed_options|=READ_HOSTESCALATIONS;
	if(needed_options>0)
		read_all_object_configuration_data(main_config_file,needed_options);

	/* initial values... */
	authinfo->authorized_for_all_hosts=FALSE;
	authinfo->authorized_for_all_host_commands=FALSE;
	authinfo->authorized_for_all_services=FALSE;
	authinfo->authorized_for_all_service_commands=FALSE;
	authinfo->authorized_for_system_information=FALSE;
	authinfo->authorized_for_system_commands=FALSE;
	authinfo->authorized_for_configuration_information=FALSE;

	/* grab username from the environment... */
	temp_ptr=getenv("REMOTE_USER");
	if(temp_ptr==NULL){
		authinfo->username="";
		authinfo->authenticated=FALSE;
	        }
	else{
		authinfo->username=(char *)malloc(strlen(temp_ptr)+1);
		if(authinfo->username==NULL)
			authinfo->username="";
		else
			strcpy(authinfo->username,temp_ptr);
		if(!strcmp(authinfo->username,""))
			authinfo->authenticated=FALSE;
		else
			authinfo->authenticated=TRUE;
	        }

	/* read in authorization override vars from config file... */
	fp=fopen(get_cgi_config_location(),"r");
	if(fp!=NULL){

		while(read_line(input_buffer,MAX_INPUT_BUFFER,fp)){

			if(feof(fp))
				break;

			strip(input_buffer);

			/* we don't have a username yet, so fake the authentication if we find a default username defined */
			if(!strcmp(authinfo->username,"") && strstr(input_buffer,"default_user_name=")==input_buffer){
				temp_ptr=strtok(input_buffer,"=");
				temp_ptr=strtok(NULL,",");
				authinfo->username=(char *)malloc(strlen(temp_ptr)+1);
				if(authinfo->username==NULL)
					authinfo->username="";
				else
					strcpy(authinfo->username,temp_ptr);
				if(!strcmp(authinfo->username,""))
					authinfo->authenticated=FALSE;
				else
					authinfo->authenticated=TRUE;
			        }

			else if(strstr(input_buffer,"authorized_for_all_hosts=")==input_buffer){
				temp_ptr=strtok(input_buffer,"=");
				while((temp_ptr=strtok(NULL,","))){
					if(!strcmp(temp_ptr,authinfo->username) || !strcmp(temp_ptr,"*"))
						authinfo->authorized_for_all_hosts=TRUE;
				        }
			        }
			else if(strstr(input_buffer,"authorized_for_all_services=")==input_buffer){
				temp_ptr=strtok(input_buffer,"=");
				while((temp_ptr=strtok(NULL,","))){
					if(!strcmp(temp_ptr,authinfo->username) || !strcmp(temp_ptr,"*"))
						authinfo->authorized_for_all_services=TRUE;
				        }
			        }
			else if(strstr(input_buffer,"authorized_for_system_information=")==input_buffer){
				temp_ptr=strtok(input_buffer,"=");
				while((temp_ptr=strtok(NULL,","))){
					if(!strcmp(temp_ptr,authinfo->username) || !strcmp(temp_ptr,"*"))
						authinfo->authorized_for_system_information=TRUE;
				        }
			        }
			else if(strstr(input_buffer,"authorized_for_configuration_information=")==input_buffer){
				temp_ptr=strtok(input_buffer,"=");
				while((temp_ptr=strtok(NULL,","))){
					if(!strcmp(temp_ptr,authinfo->username) || !strcmp(temp_ptr,"*"))
						authinfo->authorized_for_configuration_information=TRUE;
				        }
			        }
			else if(strstr(input_buffer,"authorized_for_all_host_commands=")==input_buffer){
				temp_ptr=strtok(input_buffer,"=");
				while((temp_ptr=strtok(NULL,","))){
					if(!strcmp(temp_ptr,authinfo->username) || !strcmp(temp_ptr,"*"))
						authinfo->authorized_for_all_host_commands=TRUE;
				        }
			        }
			else if(strstr(input_buffer,"authorized_for_all_service_commands=")==input_buffer){
				temp_ptr=strtok(input_buffer,"=");
				while((temp_ptr=strtok(NULL,","))){
					if(!strcmp(temp_ptr,authinfo->username) || !strcmp(temp_ptr,"*"))
						authinfo->authorized_for_all_service_commands=TRUE;
				        }
			        }
			else if(strstr(input_buffer,"authorized_for_system_commands=")==input_buffer){
				temp_ptr=strtok(input_buffer,"=");
				while((temp_ptr=strtok(NULL,","))){
					if(!strcmp(temp_ptr,authinfo->username) || !strcmp(temp_ptr,"*"))
						authinfo->authorized_for_system_commands=TRUE;
				        }
			        }
		        }

		fclose(fp);
	        }

	if(authinfo->authenticated==TRUE)
		return OK;
	else
		return ERROR;
        }



/* check if user is authorized to view information about a particular host */
int is_authorized_for_host(host *hst, authdata *authinfo){
	contact *temp_contact;

	if(hst==NULL)
		return FALSE;

	/* if we're not using authentication, fake it */
	if(use_authentication==FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated==FALSE)
		return FALSE;

	/* if this user is authorized for all hosts, they are for this one... */
	if(is_authorized_for_all_hosts(authinfo)==TRUE)
		return TRUE;

	/* find the contact */
	temp_contact=find_contact(authinfo->username,NULL);

	/* see if this user is a contact for the host */
	if(is_contact_for_host(hst,temp_contact)==TRUE)
		return TRUE;

	/* see if this user is an escalated contact for the host */
	if(is_escalated_contact_for_host(hst,temp_contact)==TRUE)
		return TRUE;

	return FALSE;
        }


/* check if user is authorized to view information about AT LEAST ONE host in a particular hostgroup */
int is_authorized_for_hostgroup(hostgroup *hg, authdata *authinfo){
	hostgroupmember *temp_hostgroupmember;
	host *temp_host;

	if(hg==NULL)
		return FALSE;

	/* see if user is authorized for any host in the hostgroup */
	for(temp_hostgroupmember=hg->members;temp_hostgroupmember!=NULL;temp_hostgroupmember=temp_hostgroupmember->next){
		temp_host=find_host(temp_hostgroupmember->host_name);
		if(is_authorized_for_host(temp_host,authinfo)==TRUE)
			return TRUE;
	        }

	return FALSE;
        }


/* check if user is authorized to view information about a particular service */
int is_authorized_for_service(service *svc, authdata *authinfo){
	host *temp_host;
	contact *temp_contact;

	if(svc==NULL)
		return FALSE;

	/* if we're not using authentication, fake it */
	if(use_authentication==FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated==FALSE)
		return FALSE;

	/* if this user is authorized for all services, they are for this one... */
	if(is_authorized_for_all_services(authinfo)==TRUE)
		return TRUE;

	/* find the host */
	temp_host=find_host(svc->host_name);
	if(temp_host==NULL)
		return FALSE;

	/* if this user is authorized for this host, they are for all services on it as well... */
	if(is_authorized_for_host(temp_host,authinfo)==TRUE)
		return TRUE;

	/* find the contact */
	temp_contact=find_contact(authinfo->username,NULL);

	/* see if this user is a contact for the service */
	if(is_contact_for_service(svc,temp_contact)==TRUE)
		return TRUE;

	/* see if this user is an escalated contact for the service */
	if(is_escalated_contact_for_service(svc,temp_contact)==TRUE)
		return TRUE;

	return FALSE;
        }


/* check if current user is authorized to view information on all hosts */
int is_authorized_for_all_hosts(authdata *authinfo){

	/* if we're not using authentication, fake it */
	if(use_authentication==FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated==FALSE)
		return FALSE;

	return authinfo->authorized_for_all_hosts;
        }


/* check if current user is authorized to view information on all service */
int is_authorized_for_all_services(authdata *authinfo){

	/* if we're not using authentication, fake it */
	if(use_authentication==FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated==FALSE)
		return FALSE;

	return authinfo->authorized_for_all_services;
        }


/* check if current user is authorized to view system information */
int is_authorized_for_system_information(authdata *authinfo){

	/* if we're not using authentication, fake it */
	if(use_authentication==FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated==FALSE)
		return FALSE;

	return authinfo->authorized_for_system_information;
        }


/* check if current user is authorized to view configuration information */
int is_authorized_for_configuration_information(authdata *authinfo){

	/* if we're not using authentication, fake it */
	if(use_authentication==FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated==FALSE)
		return FALSE;

	return authinfo->authorized_for_configuration_information;
        }


/* check if current user is authorized to issue system commands */
int is_authorized_for_system_commands(authdata *authinfo){

	/* if we're not using authentication, fake it */
	if(use_authentication==FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated==FALSE)
		return FALSE;

	return authinfo->authorized_for_system_commands;
        }


/* check is the current user is authorized to issue commands relating to a particular service */
int is_authorized_for_service_commands(service *svc, authdata *authinfo){
	host *temp_host;
	contact *temp_contact;

	if(svc==NULL)
		return FALSE;

	/* if we're not using authentication, fake it */
	if(use_authentication==FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated==FALSE)
		return FALSE;

	/* the user is authorized if they have rights to the service */
	if(is_authorized_for_service(svc,authinfo)==TRUE){

		/* find the host */
		temp_host=find_host(svc->host_name);
		if(temp_host==NULL)
			return FALSE;

		/* find the contact */
		temp_contact=find_contact(authinfo->username,NULL);

		/* see if this user is a contact for the host */
		if(is_contact_for_host(temp_host,temp_contact)==TRUE)
			return TRUE;

		/* see if this user is an escalated contact for the host */
		if(is_escalated_contact_for_host(temp_host,temp_contact)==TRUE)
			return TRUE;

		/* this user is a contact for the service, so they have permission... */
		if(is_contact_for_service(svc,temp_contact)==TRUE)
			return TRUE;

		/* this user is an escalated contact for the service, so they have permission... */
		if(is_escalated_contact_for_service(svc,temp_contact)==TRUE)
			return TRUE;

		/* this user is not a contact for the host, so they must have been given explicit permissions to all service commands */
		if(authinfo->authorized_for_all_service_commands==TRUE)
			return TRUE;
	        }

	return FALSE;
        }


/* check is the current user is authorized to issue commands relating to a particular host */
int is_authorized_for_host_commands(host *hst, authdata *authinfo){
	contact *temp_contact;

	if(hst==NULL)
		return FALSE;

	/* if we're not using authentication, fake it */
	if(use_authentication==FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated==FALSE)
		return FALSE;

	/* the user is authorized if they have rights to the host */
	if(is_authorized_for_host(hst,authinfo)==TRUE){

		/* find the contact */
		temp_contact=find_contact(authinfo->username,NULL);

		/* this user is a contact for the host, so they have permission... */
		if(is_contact_for_host(hst,temp_contact)==TRUE)
			return TRUE;

		/* this user is an escalated contact for the host, so they have permission... */
		if(is_escalated_contact_for_host(hst,temp_contact)==TRUE)
			return TRUE;

		/* this user is not a contact for the host, so they must have been given explicit permissions to all host commands */
		if(authinfo->authorized_for_all_host_commands==TRUE)
			return TRUE;
	        }

	return FALSE;
        }


