/*****************************************************************************
 *
 * CGIAUTH.C - Authorization utilities for Nagios CGIs
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
 *
 *****************************************************************************/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"

#include "../include/cgiutils.h"
#include "../include/cgiauth.h"

extern char            main_config_file[MAX_FILENAME_LENGTH];

extern int             use_authentication;
extern int             use_ssl_authentication;



/* get current authentication information */
int get_authentication_information(authdata *authinfo) {
	mmapfile *thefile;
	char *input = NULL;
	char *temp_ptr = NULL;
	contact *temp_contact = NULL;
	contactgroup *temp_contactgroup = NULL;

	if(authinfo == NULL)
		return ERROR;

	/* initial values... */
	authinfo->authorized_for_all_hosts = FALSE;
	authinfo->authorized_for_all_host_commands = FALSE;
	authinfo->authorized_for_all_services = FALSE;
	authinfo->authorized_for_all_service_commands = FALSE;
	authinfo->authorized_for_system_information = FALSE;
	authinfo->authorized_for_system_commands = FALSE;
	authinfo->authorized_for_configuration_information = FALSE;
	authinfo->authorized_for_read_only = FALSE;

	/* grab username from the environment... */
	if(use_ssl_authentication) {
		/* patch by Pawl Zuzelski - 7/22/08 */
		temp_ptr = getenv("SSL_CLIENT_S_DN_CN");
		}
	else {
		temp_ptr = getenv("REMOTE_USER");
		}
	if(temp_ptr == NULL) {
		authinfo->username = "";
		authinfo->authenticated = FALSE;
		}
	else {
		authinfo->username = (char *)malloc(strlen(temp_ptr) + 1);
		if(authinfo->username == NULL)
			authinfo->username = "";
		else
			strcpy(authinfo->username, temp_ptr);
		if(!strcmp(authinfo->username, ""))
			authinfo->authenticated = FALSE;
		else
			authinfo->authenticated = TRUE;
		}

	/* read in authorization override vars from config file... */
	if((thefile = mmap_fopen(get_cgi_config_location())) != NULL) {

		while(1) {

			/* free memory */
			free(input);

			/* read the next line */
			if((input = mmap_fgets_multiline(thefile)) == NULL)
				break;

			strip(input);

			/* we don't have a username yet, so fake the authentication if we find a default username defined */
			if(!strcmp(authinfo->username, "") && strstr(input, "default_user_name=") == input) {
				temp_ptr = strtok(input, "=");
				temp_ptr = strtok(NULL, ",");
				authinfo->username = (char *)malloc(strlen(temp_ptr) + 1);
				if(authinfo->username == NULL)
					authinfo->username = "";
				else
					strcpy(authinfo->username, temp_ptr);
				if(!strcmp(authinfo->username, ""))
					authinfo->authenticated = FALSE;
				else
					authinfo->authenticated = TRUE;
				}

			else if(strstr(input, "authorized_for_all_hosts=") == input) {
				temp_ptr = strtok(input, "=");
				while((temp_ptr = strtok(NULL, ","))) {
					if(!strcmp(temp_ptr, authinfo->username) || !strcmp(temp_ptr, "*"))
						authinfo->authorized_for_all_hosts = TRUE;
					}
				}
			else if(strstr(input, "authorized_for_all_services=") == input) {
				temp_ptr = strtok(input, "=");
				while((temp_ptr = strtok(NULL, ","))) {
					if(!strcmp(temp_ptr, authinfo->username) || !strcmp(temp_ptr, "*"))
						authinfo->authorized_for_all_services = TRUE;
					}
				}
			else if(strstr(input, "authorized_for_system_information=") == input) {
				temp_ptr = strtok(input, "=");
				while((temp_ptr = strtok(NULL, ","))) {
					if(!strcmp(temp_ptr, authinfo->username) || !strcmp(temp_ptr, "*"))
						authinfo->authorized_for_system_information = TRUE;
					}
				}
			else if(strstr(input, "authorized_for_configuration_information=") == input) {
				temp_ptr = strtok(input, "=");
				while((temp_ptr = strtok(NULL, ","))) {
					if(!strcmp(temp_ptr, authinfo->username) || !strcmp(temp_ptr, "*"))
						authinfo->authorized_for_configuration_information = TRUE;
					}
				}
			else if(strstr(input, "authorized_for_all_host_commands=") == input) {
				temp_ptr = strtok(input, "=");
				while((temp_ptr = strtok(NULL, ","))) {
					if(!strcmp(temp_ptr, authinfo->username) || !strcmp(temp_ptr, "*"))
						authinfo->authorized_for_all_host_commands = TRUE;
					}
				}
			else if(strstr(input, "authorized_for_all_service_commands=") == input) {
				temp_ptr = strtok(input, "=");
				while((temp_ptr = strtok(NULL, ","))) {
					if(!strcmp(temp_ptr, authinfo->username) || !strcmp(temp_ptr, "*"))
						authinfo->authorized_for_all_service_commands = TRUE;
					}
				}
			else if(strstr(input, "authorized_for_system_commands=") == input) {
				temp_ptr = strtok(input, "=");
				while((temp_ptr = strtok(NULL, ","))) {
					if(!strcmp(temp_ptr, authinfo->username) || !strcmp(temp_ptr, "*"))
						authinfo->authorized_for_system_commands = TRUE;
					}
				}
			else if(strstr(input, "authorized_for_read_only=") == input) {
				temp_ptr = strtok(input, "=");
				while((temp_ptr = strtok(NULL, ","))) {
					if(!strcmp(temp_ptr, authinfo->username) || !strcmp(temp_ptr, "*"))
						authinfo->authorized_for_read_only = TRUE;
					}
				}
			else if((temp_contact = find_contact(authinfo->username)) != NULL) {
				if(strstr(input, "authorized_contactgroup_for_all_hosts=") == input) {
					temp_ptr = strtok(input, "=");
					while((temp_ptr = strtok(NULL, ","))) {
						temp_contactgroup = find_contactgroup(temp_ptr);
						if(is_contact_member_of_contactgroup(temp_contactgroup, temp_contact))
							authinfo->authorized_for_all_hosts = TRUE;
						}
					}
				else if(strstr(input, "authorized_contactgroup_for_all_services=") == input) {
					temp_ptr = strtok(input, "=");
					while((temp_ptr = strtok(NULL, ","))) {
						temp_contactgroup = find_contactgroup(temp_ptr);
						if(is_contact_member_of_contactgroup(temp_contactgroup, temp_contact))
							authinfo->authorized_for_all_services = TRUE;
						}
					}
				else if(strstr(input, "authorized_contactgroup_for_system_information=") == input) {
					temp_ptr = strtok(input, "=");
					while((temp_ptr = strtok(NULL, ","))) {
						temp_contactgroup = find_contactgroup(temp_ptr);
						if(is_contact_member_of_contactgroup(temp_contactgroup, temp_contact))
							authinfo->authorized_for_system_information = TRUE;
						}
					}
				else if(strstr(input, "authorized_contactgroup_for_configuration_information=") == input) {
					temp_ptr = strtok(input, "=");
					while((temp_ptr = strtok(NULL, ","))) {
						temp_contactgroup = find_contactgroup(temp_ptr);
						if(is_contact_member_of_contactgroup(temp_contactgroup, temp_contact))
							authinfo->authorized_for_configuration_information = TRUE;
						}
					}
				else if(strstr(input, "authorized_contactgroup_for_all_host_commands=") == input) {
					temp_ptr = strtok(input, "=");
					while((temp_ptr = strtok(NULL, ","))) {
						temp_contactgroup = find_contactgroup(temp_ptr);
						if(is_contact_member_of_contactgroup(temp_contactgroup, temp_contact))
							authinfo->authorized_for_all_host_commands = TRUE;
						}
					}
				else if(strstr(input, "authorized_contactgroup_for_all_service_commands=") == input) {
					temp_ptr = strtok(input, "=");
					while((temp_ptr = strtok(NULL, ","))) {
						temp_contactgroup = find_contactgroup(temp_ptr);
						if(is_contact_member_of_contactgroup(temp_contactgroup, temp_contact))
							authinfo->authorized_for_all_service_commands = TRUE;
						}
					}
				else if(strstr(input, "authorized_contactgroup_for_system_commands=") == input) {
					temp_ptr = strtok(input, "=");
					while((temp_ptr = strtok(NULL, ","))) {
						temp_contactgroup = find_contactgroup(temp_ptr);
						if(is_contact_member_of_contactgroup(temp_contactgroup, temp_contact))
							authinfo->authorized_for_system_commands = TRUE;
						}
					}
				else if(strstr(input, "authorized_contactgroup_for_read_only=") == input) {
					temp_ptr = strtok(input, "=");
					while((temp_ptr = strtok(NULL, ","))) {
						temp_contactgroup = find_contactgroup(temp_ptr);
						if(is_contact_member_of_contactgroup(temp_contactgroup, temp_contact))
							authinfo->authorized_for_read_only = TRUE;
						}
					}
				}
			}

		/* free memory and close the file */
		free(input);
		mmap_fclose(thefile);
		}

	if(authinfo->authenticated == TRUE)
		return OK;
	else
		return ERROR;
	}



/* check if user is authorized to view information about a particular host */
int is_authorized_for_host(host *hst, authdata *authinfo) {
	contact *temp_contact;

	if(hst == NULL)
		return FALSE;

	/* if we're not using authentication, fake it */
	if(use_authentication == FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated == FALSE)
		return FALSE;

	/* if this user is authorized for all hosts, they are for this one... */
	if(is_authorized_for_all_hosts(authinfo) == TRUE)
		return TRUE;

	/* find the contact */
	temp_contact = find_contact(authinfo->username);

	/* see if this user is a contact for the host */
	if(is_contact_for_host(hst, temp_contact) == TRUE)
		return TRUE;

	/* see if this user is an escalated contact for the host */
	if(is_escalated_contact_for_host(hst, temp_contact) == TRUE)
		return TRUE;

	return FALSE;
	}


/* check if user is authorized to view information about all hosts in a particular hostgroup */
int is_authorized_for_hostgroup(hostgroup *hg, authdata *authinfo) {
	hostsmember *temp_hostsmember;
	host *temp_host;

	if(hg == NULL)
		return FALSE;

	/* CHANGED in 2.0 - user must be authorized for ALL hosts in a hostgroup, not just one */
	/* see if user is authorized for all hosts in the hostgroup */
	/*
	for(temp_hostsmember = hg->members; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
		temp_host = find_host(temp_hostsmember->host_name);
		if(is_authorized_for_host(temp_host, authinfo) == FALSE)
			return FALSE;
		}
	*/
	/* Reverted for 3.3.2 - must only be a member of one hostgroup */
	for(temp_hostsmember = hg->members; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
		temp_host = find_host(temp_hostsmember->host_name);
		if(is_authorized_for_host(temp_host, authinfo) == TRUE)
			return TRUE;
		}

	/*return TRUE;*/
	return FALSE;
	}



/* check if user is authorized to view information about all services in a particular servicegroup */
int is_authorized_for_servicegroup(servicegroup *sg, authdata *authinfo) {
	servicesmember *temp_servicesmember;
	service *temp_service;

	if(sg == NULL)
		return FALSE;

	/* see if user is authorized for all services in the servicegroup */
	/*
	for(temp_servicesmember = sg->members; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
		temp_service = find_service(temp_servicesmember->host_name, temp_servicesmember->service_description);
		if(is_authorized_for_service(temp_service, authinfo) == FALSE)
			return FALSE;
		}
	*/
	/* Reverted for 3.3.2 - must only be a member of one hostgroup */
	for(temp_servicesmember = sg->members; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
		temp_service = find_service(temp_servicesmember->host_name, temp_servicesmember->service_description);
		if(is_authorized_for_service(temp_service, authinfo) == TRUE)
			return TRUE;
		}

	/*return TRUE*/;
	return FALSE;
	}

/* check if current user is restricted to read only */
int is_authorized_for_read_only(authdata *authinfo) {

	/* if we're not using authentication, fake it */
	if(use_authentication == FALSE)
		return FALSE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated == FALSE)
		return FALSE;

	return authinfo->authorized_for_read_only;
	}

/* check if user is authorized to view information about a particular service */
int is_authorized_for_service(service *svc, authdata *authinfo) {
	host *temp_host;
	contact *temp_contact;

	if(svc == NULL)
		return FALSE;

	/* if we're not using authentication, fake it */
	if(use_authentication == FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated == FALSE)
		return FALSE;

	/* if this user is authorized for all services, they are for this one... */
	if(is_authorized_for_all_services(authinfo) == TRUE)
		return TRUE;

	/* find the host */
	temp_host = find_host(svc->host_name);
	if(temp_host == NULL)
		return FALSE;

	/* if this user is authorized for this host, they are for all services on it as well... */
	if(is_authorized_for_host(temp_host, authinfo) == TRUE)
		return TRUE;

	/* find the contact */
	temp_contact = find_contact(authinfo->username);

	/* see if this user is a contact for the service */
	if(is_contact_for_service(svc, temp_contact) == TRUE)
		return TRUE;

	/* see if this user is an escalated contact for the service */
	if(is_escalated_contact_for_service(svc, temp_contact) == TRUE)
		return TRUE;

	return FALSE;
	}


/* check if current user is authorized to view information on all hosts */
int is_authorized_for_all_hosts(authdata *authinfo) {

	/* if we're not using authentication, fake it */
	if(use_authentication == FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated == FALSE)
		return FALSE;

	return authinfo->authorized_for_all_hosts;
	}


/* check if current user is authorized to view information on all service */
int is_authorized_for_all_services(authdata *authinfo) {

	/* if we're not using authentication, fake it */
	if(use_authentication == FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated == FALSE)
		return FALSE;

	return authinfo->authorized_for_all_services;
	}


/* check if current user is authorized to view system information */
int is_authorized_for_system_information(authdata *authinfo) {

	/* if we're not using authentication, fake it */
	if(use_authentication == FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated == FALSE)
		return FALSE;

	return authinfo->authorized_for_system_information;
	}


/* check if current user is authorized to view configuration information */
int is_authorized_for_configuration_information(authdata *authinfo) {

	/* if we're not using authentication, fake it */
	if(use_authentication == FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated == FALSE)
		return FALSE;

	return authinfo->authorized_for_configuration_information;
	}


/* check if current user is authorized to issue system commands */
int is_authorized_for_system_commands(authdata *authinfo) {

	/* if we're not using authentication, fake it */
	if(use_authentication == FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated == FALSE)
		return FALSE;

	return authinfo->authorized_for_system_commands;
	}


/* check is the current user is authorized to issue commands relating to a particular service */
int is_authorized_for_service_commands(service *svc, authdata *authinfo) {
	host *temp_host;
	contact *temp_contact;

	if(svc == NULL)
		return FALSE;

	/* if we're not using authentication, fake it */
	if(use_authentication == FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated == FALSE)
		return FALSE;

	/* the user is authorized if they have rights to the service */
	if(is_authorized_for_service(svc, authinfo) == TRUE) {

		/* find the host */
		temp_host = find_host(svc->host_name);
		if(temp_host == NULL)
			return FALSE;

		/* find the contact */
		temp_contact = find_contact(authinfo->username);

		/* reject if contact is not allowed to issue commands */
		if(temp_contact && temp_contact->can_submit_commands == FALSE)
			return FALSE;

		/* see if this user is a contact for the host */
		if(is_contact_for_host(temp_host, temp_contact) == TRUE)
			return TRUE;

		/* see if this user is an escalated contact for the host */
		if(is_escalated_contact_for_host(temp_host, temp_contact) == TRUE)
			return TRUE;

		/* this user is a contact for the service, so they have permission... */
		if(is_contact_for_service(svc, temp_contact) == TRUE)
			return TRUE;

		/* this user is an escalated contact for the service, so they have permission... */
		if(is_escalated_contact_for_service(svc, temp_contact) == TRUE)
			return TRUE;

		/* this user is not a contact for the host, so they must have been given explicit permissions to all service commands */
		if(authinfo->authorized_for_all_service_commands == TRUE)
			return TRUE;
		}

	return FALSE;
	}


/* check is the current user is authorized to issue commands relating to a particular host */
int is_authorized_for_host_commands(host *hst, authdata *authinfo) {
	contact *temp_contact;

	if(hst == NULL)
		return FALSE;

	/* if we're not using authentication, fake it */
	if(use_authentication == FALSE)
		return TRUE;

	/* if this user has not authenticated return error */
	if(authinfo->authenticated == FALSE)
		return FALSE;

	/* the user is authorized if they have rights to the host */
	if(is_authorized_for_host(hst, authinfo) == TRUE) {

		/* find the contact */
		temp_contact = find_contact(authinfo->username);

		/* reject if contact is not allowed to issue commands */
		if(temp_contact && temp_contact->can_submit_commands == FALSE)
			return FALSE;

		/* this user is a contact for the host, so they have permission... */
		if(is_contact_for_host(hst, temp_contact) == TRUE)
			return TRUE;

		/* this user is an escalated contact for the host, so they have permission... */
		if(is_escalated_contact_for_host(hst, temp_contact) == TRUE)
			return TRUE;

		/* this user is not a contact for the host, so they must have been given explicit permissions to all host commands */
		if(authinfo->authorized_for_all_host_commands == TRUE)
			return TRUE;
		}

	return FALSE;
	}


/* check is the current user is authorized to issue commands relating to a particular servicegroup */
int is_authorized_for_servicegroup_commands(servicegroup *sg, authdata *authinfo) {
	servicesmember *temp_servicesmember;
	service *temp_service;

	if(sg == NULL)
		return FALSE;

	/* see if user is authorized for all services commands in the servicegroup */
	for(temp_servicesmember = sg->members; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
		temp_service = find_service(temp_servicesmember->host_name, temp_servicesmember->service_description);
		if(is_authorized_for_service_commands(temp_service, authinfo) == FALSE)
			return FALSE;
		}

	return TRUE;
	}


/* check is the current user is authorized to issue commands relating to a particular hostgroup */
int is_authorized_for_hostgroup_commands(hostgroup *hg, authdata *authinfo) {
	hostsmember *temp_hostsmember;
	host *temp_host;

	if(hg == NULL)
		return FALSE;

	/* see if user is authorized for all hosts in the hostgroup */
	for(temp_hostsmember = hg->members; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
		temp_host = find_host(temp_hostsmember->host_name);
		if(is_authorized_for_host_commands(temp_host, authinfo) == FALSE)
			return FALSE;
		}

	return TRUE;
	}
