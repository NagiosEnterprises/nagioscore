/*****************************************************************************
 *
 * XODDEFAULT.C - Default object configuration data input routines
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   01-19-2002
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

#include "../common/config.h"
#include "../common/common.h"
#include "../common/objects.h"

/**** CORE OR CGI SPECIFIC HEADER FILES ****/

#ifdef NSCORE
#include "../base/nagios.h"
#endif

#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif


/**** DATA INPUT-SPECIFIC HEADER FILES ****/

#include "xoddefault.h"


#define MAX_HOSTS_INPUT_BUFFER 8196	/* needed for very long definitions (i.e. hostgroups) */



/******************************************************************/
/*********** DEFAULT HOST CONFIG DATA INPUT FUNCTION **************/
/******************************************************************/

/* process all host config files (default method) */
int xoddefault_read_config_data(char *main_config_file,int options){
	char config_file[MAX_FILENAME_LENGTH];
	char input[MAX_HOSTS_INPUT_BUFFER];
	char *temp_ptr;
	FILE *fp;
	int result=OK;

#ifdef DEBUG0
	printf("xoddefault_read_config_data() start\n");
#endif

	/* open the main config file for reading (we need to find all the host config files to read) */
	fp=fopen(main_config_file,"r");
	if(fp==NULL){
#ifdef DEBUG1
		printf("Error: Cannot open main configuration file '%s' for reading!\n",main_config_file);
#endif
		return ERROR;
	        }

	/* read in all lines from the main config file */
	for(fgets(input,sizeof(input)-1,fp);!feof(fp);fgets(input,sizeof(input)-1,fp)){

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]=='\x0' || input[0]=='\n' || input[0]=='\r')
			continue;

		strip(input);

		temp_ptr=my_strtok(input,"=");
		if(temp_ptr==NULL)
			continue;

		/* process a single config file */
		if(strstr(temp_ptr,"xoddefault_config_file")==temp_ptr || strstr(temp_ptr,"cfg_file")==temp_ptr){

			/* get the config file name */
			temp_ptr=my_strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;

			strncpy(config_file,temp_ptr,sizeof(config_file)-1);
			config_file[sizeof(config_file)-1]='\x0';

			/* process the config file... */
			result=xoddefault_process_config_file(config_file,options);

			/* if there was an error processing the config file, break out of loop */
			if(result==ERROR)
				break;
		        }

		/* process all files in a config directory */
		else if(strstr(temp_ptr,"xoddefault_config_dir")==temp_ptr || strstr(temp_ptr,"cfg_dir")==temp_ptr){

			/* get the config directory name */
			temp_ptr=my_strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;

			strncpy(config_file,temp_ptr,sizeof(config_file)-1);
			config_file[sizeof(config_file)-1]='\x0';

			/* strip trailing / if necessary */
			if(config_file[strlen(config_file)-1]=='/')
				config_file[strlen(config_file)-1]='\x0';

			/* process the config directory... */
			result=xoddefault_process_config_dir(config_file,options);

			/* if there was an error processing the config file, break out of loop */
			if(result==ERROR)
				break;
		        }
		}

	fclose(fp);

#ifdef DEBUG0
	printf("xoddefault_read_config_data() end\n");
#endif

	return result;
	}





/* parse the raw input line and get the variable and value */
int xoddefault_parse_input(char *input, char *variable, char *value, int maxvarlength, int maxvallength){
	char *temp_ptr;

#ifdef DEBUG0
	printf("xoddefault_parse_input() start\n");
#endif

	/* get the variable */
	temp_ptr=my_strtok(input,"=");

	/* if it doesn't exist, return an error */
	if(temp_ptr==NULL){

#ifdef DEBUG1
		printf("\tVariable is NULL\n");
#endif

		return ERROR;
		}

	/* else the variable is okay */
	strncpy(variable,temp_ptr,maxvarlength);
	variable[maxvarlength-1]='\x0';

	/* get the value */
	temp_ptr=my_strtok(NULL,"\n");

	/* if the value doesn't exist, return an error */
	if(temp_ptr==NULL){

#ifdef DEBUG1
		printf("\tValue is NULL\n");
#endif

		return ERROR;
		}

	/* else the value is good */
	strncpy(value,temp_ptr,maxvallength);
	value[maxvallength-1]='\x0';

	strip(variable);
	strip(value);

#ifdef DEBUG0
	printf("xoddefault_parse_input() end\n");
#endif

	return OK;
	}




/* parse the variable name */
int xoddefault_parse_variable(char *varstring, char *vartype, char *varname, int maxtypelength, int maxnamelength){
	char *temp;

#ifdef DEBUG0
	printf("xoddefault_parse_variable() start\n");
#endif

	/* get the variable type */
	temp=my_strtok(varstring,"[");

	/* return error if its empty */
	if(temp==NULL){

#ifdef DEBUG2
		printf("\tVariable type is NULL\n");
#endif

		return ERROR;
		}

	/* return error if var type is too long */
	if(strlen(temp) > (maxtypelength-1)){

#ifdef DEBUG2
		printf("\tVariable type '%s' is too long\n",temp);
#endif

		return ERROR;
                }

	/* else its good */
	strncpy(vartype,temp,maxtypelength);
	vartype[maxtypelength-1]='\x0';

	/* now get the name of the variable */
	temp=my_strtok(NULL,"]");

	/* return error if its empty */
	if(temp==NULL){

#ifdef DEBUG2
		printf("\tVariable name is NULL\n");
#endif

		return ERROR;
		}

	/* return error if var name is too long */
	if(strlen(temp) > (maxnamelength-1)){

#ifdef DEBUG2
		printf("\tVariable name '%s' is too long\n",temp);
#endif

		return ERROR;
                }

	/* else its good */
	strncpy(varname,temp,maxnamelength);
	varname[maxnamelength-1]='\x0';

	strip(vartype);
	strip(varname);

#ifdef DEBUG0
	printf("xoddefault_parse_variable() end\n");
#endif

	return OK;
	}



/* process all files in a specific config directory */
int xoddefault_process_config_dir(char *dirname, int options){
	char config_file[MAX_FILENAME_LENGTH];
	DIR *dirp;
	struct dirent *dirfile;
	int result=OK;
	int x;

#ifdef DEBUG0
	printf("xoddefault_process_config_dir() start\n");
#endif

	/* open the directory for reading */
	dirp=opendir(dirname);
        if(dirp==NULL){
#ifdef NSCORE
		printf("Error: Could not open config directory '%s' for reading.\n",dirname);
#endif
		result=ERROR;
	        }

	/* process all files in the directory... */
	while((dirfile=readdir(dirp))!=NULL){

		/* test if this is a config file, otherwise skip it... */
		x=strlen(dirfile->d_name);
		if(x<=4)
			continue;
		if(strcmp(dirfile->d_name+(x-4),".cfg"))
			continue;

		/* create the full path to the config file */
		snprintf(config_file,sizeof(config_file)-1,"%s/%s",dirname,dirfile->d_name);
		config_file[sizeof(config_file)-1]='\x0';

		/* process the config file */
		result=xoddefault_process_config_file(config_file,options);

		/* break out if we encountered an error */
		if(result==ERROR)
			break;
		}

	closedir(dirp);

#ifdef DEBUG0
	printf("xoddefault_process_config_dir() end\n");
#endif

	return result;
        }



/* process data in a specific config file */
int xoddefault_process_config_file(char *filename, int options){
	char variable[MAX_INPUT_BUFFER];
	char value[MAX_HOSTS_INPUT_BUFFER];
	char vartype[MAX_XODDEFAULT_VAR_TYPE];
	char varname[MAX_XODDEFAULT_VAR_NAME];
	char input[MAX_HOSTS_INPUT_BUFFER];
	FILE *fp;
	int current_line=0;
	int result=OK;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xoddefault_process_config_file() start\n");
#endif

	/* open the host config file for reading */
	fp=fopen(filename,"r");
	if(fp==NULL){
#ifdef NSCORE
		printf("Error: Cannot open host configuration file '%s' for reading!\n",filename);
#endif
		return ERROR;
	        }
		
	/* read all lines from the config file */
	for(current_line=1,fgets(input,MAX_HOSTS_INPUT_BUFFER-1,fp);!feof(fp);current_line++,fgets(input,MAX_HOSTS_INPUT_BUFFER-1,fp)){

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]=='\x0' || input[0]=='\n' || input[0]=='\r')
			continue;

		strip(input);

		result=xoddefault_parse_input(input,variable,value,sizeof(variable),sizeof(value));
		if(result!=OK){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not parse input)\n",filename,current_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			result=ERROR;
			break;
		        }

		result=xoddefault_parse_variable(variable,vartype,varname,sizeof(vartype),sizeof(varname));
		if(result!=OK){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not parse variable)\n",filename,current_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			result=ERROR;
			break;
		        }

#ifdef DEBUG1
		printf("\tVar Type: %s\n",vartype);
		printf("\tVar Name: %s\n",varname);
#endif

		/* decide how to process the variable ...*/

		/* this is a TIMEPERIOD entry */
		if(!strcmp(vartype,"timeperiod")){
			if(options & READ_TIMEPERIODS){
				result=xoddefault_add_timeperiod(varname,value,current_line);
#ifdef NSCORE
				if(result==ERROR){
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not add timeperiod definition)\n",filename,current_line);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
				        }
#endif
			        }
		        }

		/* this is a HOST entry */
		else if(!strcmp(vartype,"host")){
			if(options & READ_HOSTS){
				result=xoddefault_add_host(varname,value,current_line);
#ifdef NSCORE
				if(result==ERROR){
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not add host definition)\n",filename,current_line);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
				        }
#endif
			        }
		        }

		/* this is a HOSTGROUP entry */
		else if(!strcmp(vartype,"hostgroup")){
			if(options & READ_HOSTGROUPS){
				result=xoddefault_add_hostgroup(varname,value,current_line);
#ifdef NSCORE
				if(result==ERROR){
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not add hostgroup definition)\n",filename,current_line);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
				        }
#endif
			        }
	                }

		/* this is a CONTACT entry */
		else if(!strcmp(vartype,"contact")){
			if(options & READ_CONTACTS){
				result=xoddefault_add_contact(varname,value,current_line);
#ifdef NSCORE
				if(result==ERROR){
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not add contact definition)\n",filename,current_line);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
				        }
#endif
			        }
		        }

		/* this is a CONTACTGROUP entry */
		else if(!strcmp(vartype,"contactgroup")){
			if(options & READ_CONTACTGROUPS){
				result=xoddefault_add_contactgroup(varname,value,current_line);
#ifdef NSCORE
				if(result==ERROR){
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not add contactgroup definition)\n",filename,current_line);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
				        }
#endif
			        }
		        }

		/* this is a SERVICE entry */
		else if(!strcmp(vartype,"service")){
			if(options & READ_SERVICES){
				result=xoddefault_add_service(varname,value,current_line);
#ifdef NSCORE
				if(result==ERROR){
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not add service definition)\n",filename,current_line);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
				        }
#endif
			        }
		        }

		/* this is a COMMAND entry */
		else if(!strcmp(vartype,"command")){
			if(options & READ_COMMANDS){
				result=xoddefault_add_command(varname,value,current_line);
#ifdef NSCORE
				if(result==ERROR){
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not add command definition)\n",filename,current_line);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
				        }
#endif
			        }
		        }

		/* this is a SERVICE ESCALATION entry */
		else if(!strcmp(vartype,"serviceescalation")){
			if(options & READ_SERVICEESCALATIONS){
				result=xoddefault_add_serviceescalation(varname,value,current_line);
#ifdef NSCORE
				if(result==ERROR){
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not add service escalation definition)\n",filename,current_line);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
				        }
#endif
			        }
		        }

		/* this is a HOSTGROUP ESCALATION entry */
		else if(!strcmp(vartype,"hostgroupescalation")){
			if(options & READ_HOSTGROUPESCALATIONS){
				result=xoddefault_add_hostgroupescalation(varname,value,current_line);
#ifdef NSCORE
				if(result==ERROR){
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not add hostgroup escalation definition)\n",filename,current_line);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
				        }
#endif
			        }
		        }

		/* this is a SERVICE DEPENDENCY entry */
		else if(!strcmp(vartype,"servicedependency")){
			if(options & READ_SERVICEDEPENDENCIES){
				result=xoddefault_add_servicedependency(varname,value,current_line);
#ifdef NSCORE
				if(result==ERROR){
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (Could not add service dependency definition)\n",filename,current_line);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
				        }
#endif
			        }
		        }

		/* else we don't know what the hell it is */
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error in object configuration file '%s' - Line %d (UNKNOWN VARIABLE)\n",filename,current_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			result=ERROR;
		        }

		if(result==ERROR)
			break;
	        }

#ifdef DEBUG0
	printf("xoddefault_process_config_file() end\n");
#endif

	return OK;
        }



/******************************************************************/
/********** DEFAULT HOST CONFIG DATA ADDITION FUNCTIONS ***********/
/******************************************************************/


/* add a hostgroup definition */
int xoddefault_add_hostgroup(char *varname, char *value, int line){
	hostgroup *new_hostgroup;
	hostgroupmember *new_hostgroupmember;
	contactgroupsmember *new_contactgroupsmember;
	char *alias;
	char *hosts;
	char *temp_hosts;
	char *host_name;
	char *groups;
	char *temp_groups;
	char *group;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xoddefault_add_hostgroup() start\n");
#endif

	/* get the group alias */
	alias=my_strtok(value,";");
	if(alias==NULL || !strcmp(alias,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Hostgroup '%s' alias is NULL - Line %d\n",varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR; 
	        }

	/* get the contact groups */
	groups=my_strtok(NULL,";");
	if(groups==NULL || !strcmp(groups,""))
		temp_groups=NULL;
	else{
		temp_groups=(char *)malloc(strlen(groups)+1);
		if(temp_groups==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate temporary memory for hostgroup '%s' contact groups - Line %d\n",varname,line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
	                }
		strcpy(temp_groups,groups);
	        }

	/* get all the hosts */
	hosts=my_strtok(NULL,"\n");
	if(hosts==NULL || !strcmp(hosts,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Hostgroup '%s' member list is NULL - Line %d\n",varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	temp_hosts=(char *)malloc(strlen(hosts)+1);
	if(temp_hosts==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate temporary memory for hostgroup '%s' host members - Line %d\n",varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(temp_groups);
		return ERROR;
	        } 
	strcpy(temp_hosts,hosts);

	
	/**** ADD THE HOST GROUP ****/

	/* add the  host group */
	new_hostgroup=add_hostgroup(varname,alias);
	if(new_hostgroup==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add hostgroup '%s' - Line %d\n",varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(temp_hosts);
		free(temp_groups);
		return ERROR;
	        }

	/* add all members to the host group */
	for(host_name=my_strtok(temp_hosts,",");host_name!=NULL;host_name=my_strtok(NULL,",")){
		new_hostgroupmember=add_host_to_hostgroup(new_hostgroup,host_name);
		if(new_hostgroupmember==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add host '%s' to hostgroup '%s' - Line %d\n",host_name,new_hostgroup->group_name,line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(temp_hosts);
			free(temp_groups);
			return ERROR;
		        }
	        }

	/* add all contact groups to the host group */
	if(temp_groups!=NULL){

		for(group=my_strtok(temp_groups,",");group!=NULL;group=my_strtok(NULL,",")){
			new_contactgroupsmember=add_contactgroup_to_hostgroup(new_hostgroup,group);
			if(new_contactgroupsmember==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contactgroup '%s' to hostgroup '%s' - Line %d\n",group,new_hostgroup->group_name,line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				free(temp_hosts);
				free(temp_groups);
				return ERROR;
		                }
		        }
	        }

	free(temp_hosts);
	free(temp_groups);

#ifdef DEBUG0
	printf("xoddefault_add_hostgroup() end\n");
#endif
	return OK;
        }



/* add a contactgroup definition */
int xoddefault_add_contactgroup(char *varname, char *value, int line){
	contactgroup *new_contactgroup;
	contactgroupmember *new_contactgroupmember;
	char *alias;
	char *contacts;
	char *temp_contacts;
	char *contact_name;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xoddefault_add_contactgroup() start\n");
#endif

	/* get the group alias */
	alias=my_strtok(value,";");
	if(alias==NULL || !strcmp(alias,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contactgroup '%s' alias is NULL - Line %d\n",varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* get all the contacts */
	contacts=my_strtok(NULL,"\n");
	if(contacts==NULL || !strcmp(contacts,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contactgroup '%s' member list is NULL - Line %d\n",varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	temp_contacts=(char *)malloc(strlen(contacts)+1);
	if(temp_contacts==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate temporary memory for contactgroup '%s' members - Line %d\n",varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strcpy(temp_contacts,contacts);


	
	/**** ADD THE CONTACT GROUP ****/

	/* add the contact group */
	new_contactgroup=add_contactgroup(varname,alias);
	if(new_contactgroup==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contactgroup '%s' - Line %d\n",varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(temp_contacts);
		return ERROR;
	        }

	/* add all members to the contact group */
	for(contact_name=my_strtok(temp_contacts,",");contact_name!=NULL;contact_name=my_strtok(NULL,",")){
		new_contactgroupmember=add_contact_to_contactgroup(new_contactgroup,contact_name);
		if(new_contactgroupmember==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contact '%s' to contactgroup '%s' - Line %d\n",contact_name,varname,line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(temp_contacts);
			return ERROR;
		        }
	        }

	free(temp_contacts);


#ifdef DEBUG0
	printf("xoddefault_add_contactgroup() end\n");
#endif
	return OK;
        }



/* add a service definition */
int xoddefault_add_service(char *varname, char *value, int line){
	service *new_service;
	contactgroupsmember *new_contactgroupsmember;
	char *temp_ptr;
	char *check_command;
	char *event_handler;
	char *description;
	char *notification_period;
	char *check_period;
	char *contacts;
	int max_attempts;
	int check_interval;
	int retry_interval;
	int notify_recovery;
	int notify_critical;
	int notify_warning;
	int notify_unknown;
	int parallelize;
	int is_volatile;
	int notification_interval;
	int accept_passive_checks;
	int event_handler_enabled;
	int checks_enabled;
	int notifications_enabled;
	int flap_detection_enabled;
	double low_flap_threshold;
	double high_flap_threshold;
	char *contactgroups;
	char *contactgroup_name;
	int stalk_ok;
	int stalk_warning;
	int stalk_unknown;
	int stalk_critical;
	int process_perfdata;
	int check_freshness;
	int freshness_threshold;
	int retain_status_information;
	int retain_nonstatus_information;
	int obsess_over_service;
	int failure_prediction_enabled;
	char *failure_prediction_options;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xoddefault_add_service() start\n");
#endif

	/* get the service description */
	description=my_strtok(value,";");
	if(description==NULL || !strcmp(description,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service description is NULL - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* get the volatile option */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Volatile option for service '%s' on host '%s' is NULL - Line %d\n",description,varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	is_volatile=atoi(temp_ptr);
	if(is_volatile<0 || is_volatile>1){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid volatile option for service '%s' on host '%s' - Line %d\n",description,varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* get the check timeperiod name */
	check_period=my_strtok(NULL,";");

	/* get the max number of tries to check service status */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Max check attempts for service '%s' on host '%s' is NULL - Line %d\n",description,varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	max_attempts=atoi(temp_ptr);

	/* get the check interval */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Check interval for service '%s' on host '%s' is NULL - Line %d\n",description,varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	check_interval=atoi(temp_ptr);

	/* get the retry interval */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Retry interval for service '%s' on host '%s' is NULL - Line %d\n",description,varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	retry_interval=atoi(temp_ptr);

	/* get the contact groups to be notified */
	contacts=my_strtok(NULL,";");
	if(contacts==NULL || !strcmp(contacts,""))
		contactgroups=NULL;
	else{
		contactgroups=(char *)malloc(strlen(contacts)+1);
		if(contactgroups==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for contact group on service '%s' on host '%s' is NULL - Line %d\n",description,varname,line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
	                }
		strcpy(contactgroups,contacts);
	        }

	/* get the notifcation interval */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notification interval for service '%s' on host '%s' is NULL - Line %d\n",description,varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	notification_interval=atoi(temp_ptr);

	/* get the notification timeperiod name */
	notification_period=my_strtok(NULL,";");

	/* get the notifcation on recovery option */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on recovery option for service '%s' on host '%s' is NULL - Line %d\n",description,varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(contactgroups);
		return ERROR;
	        }
	strip(temp_ptr);
	notify_recovery=atoi(temp_ptr);

	/* get the notifcation on critical (down) option */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on critical option for service '%s' on host '%s' is NULL - Line %d\n",description,varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(contactgroups);
		return ERROR;
	        }
	strip(temp_ptr);
	notify_critical=atoi(temp_ptr);

	/* get the notifcation on warning option */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on warning option for service '%s' on host '%s' is NULL - Line %d\n",description,varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(contactgroups);
		return ERROR;
	        }
	strip(temp_ptr);
	notify_warning=atoi(temp_ptr);

	/* get the command name to run when the service state changes */
	event_handler=my_strtok(NULL,";");

	/* get the command name to run when checking this service */
	check_command=my_strtok(NULL,";");


	/* use the same value for unknown notifications as for warning notifications */
	notify_unknown=notify_warning;

	/* service check is parallelized by default */
	parallelize=1;

	/* checks are enabled by default */
	checks_enabled=1;

	/* passive service checks are accepted by default */
	accept_passive_checks=1;

	/* event handler is enabled by default */
	event_handler_enabled=1;

	/* notifications are enabled by default */
	notifications_enabled=1;

	/* flap detection is enabled by default */
	flap_detection_enabled=1;

	/* flap threshold default to program-wide values */
	low_flap_threshold=0.0;
	high_flap_threshold=0.0;

	/* state stalking not available in default config file - default is to disable it */
	stalk_ok=0;
	stalk_warning=0;
	stalk_unknown=0;
	stalk_critical=0;

	/* default is to process performance data */
	process_perfdata=1;

	/* default it to enable failure prediction (no options) */
	failure_prediction_enabled=1;
	failure_prediction_options=NULL;

	/* default is to not check freshness */
	check_freshness=0;
	freshness_threshold=0;

	/* default is to retain state information (all types) */
	retain_status_information=1;
	retain_nonstatus_information=1;

	/* default is to obsess over service */
	obsess_over_service=1;


	/**** ADD THE NEW SERVICE ****/

	/* add the service */
	new_service=add_service(varname,description,check_period,max_attempts,parallelize,accept_passive_checks,check_interval,retry_interval,notification_interval,notification_period,notify_recovery,notify_unknown,notify_warning,notify_critical,notifications_enabled,is_volatile,event_handler,event_handler_enabled,check_command,checks_enabled,flap_detection_enabled,low_flap_threshold,high_flap_threshold,stalk_ok,stalk_warning,stalk_unknown,stalk_critical,process_perfdata,failure_prediction_enabled,failure_prediction_options,check_freshness,freshness_threshold,retain_status_information,retain_nonstatus_information,obsess_over_service);

	/* return with an error if we couldn't add the service */
	if(new_service==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add service '%s' on host '%s' - Line %d\n",description,varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(contactgroups);
		return ERROR;
	        }

	/* add all the contact groups to the service */
	if(contactgroups!=NULL){

		for(contactgroup_name=my_strtok(contactgroups,",");contactgroup_name!=NULL;contactgroup_name=my_strtok(NULL,",")){

			/* add this contactgroup to the service definition */
			new_contactgroupsmember=add_contactgroup_to_service(new_service,contactgroup_name);

			/* stop adding contact groups if we ran into an error */
			if(new_contactgroupsmember==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contact group '%s' to service '%s' on host '%s' is NULL - Line %d\n",contactgroup_name,description,varname,line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				free(contactgroups);
				return ERROR;
		                }
		        }
	        }

	free(contactgroups);

	return OK;
        }




/* add a command definition */
int xoddefault_add_command(char *varname, char *value, int line){
	command *new_command;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xoddefault_add_command() start\n");
#endif

	/* add the command */
	new_command=add_command(varname,value);
	if(new_command==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add command '%s' - Line %d\n",varname,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

#ifdef DEBUG0
	printf("xoddefault_add_command() end\n");
#endif

	return OK;
        }



/* add a timeperiod definition */
int xoddefault_add_timeperiod(char *name,char *value, int line){
	timeperiod *new_timeperiod;
	timerange *new_timerange;
	char *day_range_start_buffer;
	char *range_buffer;
	char *day_range_ptr;
	char *range_ptr;
	char *time_ptr;
	char *temp_buffer;
	char *time_buffer;
	char *period_buffer;
	char *alias;
	int hours;
	int minutes;
	unsigned long range_start_time;
	unsigned long range_end_time;
	int day;

#ifdef DEBUG0
	printf("xoddefault_add_timeperiod() start\n");
#endif

	/* get the alias */
	alias=my_strtok(value,";");
	if(alias==NULL)
		return ERROR;

	/* get the time period day ranges */
	temp_buffer=my_strtok(NULL,"\n");
	if(temp_buffer==NULL)
		return ERROR;

	strip(name);
	strip(alias);


	/**** ADD THE TIMEPERIOD ****/

	period_buffer=(char *)malloc(strlen(temp_buffer)+1);
	if(period_buffer==NULL)
		return ERROR;

	strcpy(period_buffer,temp_buffer);
	strip(period_buffer);

	/* add the timeperiod object */
	new_timeperiod=add_timeperiod(name,alias);
	if(new_timeperiod==NULL){
		free(period_buffer);
		return ERROR;
	        }

	/* parse time ranges for each day in the time period */
	for(day=0,temp_buffer=my_strtok(period_buffer,";");temp_buffer!=NULL && day<7;temp_buffer=my_strtok(NULL,";"),day++){

#ifdef DEBUG1
		printf("\nDay #%d = '%s'\n",day,temp_buffer);
#endif

		/* no time range found for today, range already initialized to NULL, so continue */
		if(!strcmp(temp_buffer,""))
			continue;

		day_range_ptr=temp_buffer;
		for(day_range_start_buffer=my_strsep(&day_range_ptr,",");day_range_start_buffer!=NULL;day_range_start_buffer=my_strsep(&day_range_ptr,",")){

			range_ptr=day_range_start_buffer;
			range_buffer=my_strsep(&range_ptr,"-");
			if(range_buffer==NULL){
				free(period_buffer);
				return ERROR;
			        }

			time_ptr=range_buffer;
			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
				free(period_buffer);
				return ERROR;
			        }
			hours=atoi(time_buffer);
			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
				free(period_buffer);
				return ERROR;
			        }
			minutes=atoi(time_buffer);

			/* calculate the range start time in seconds */
			range_start_time=(unsigned long)((minutes*60)+(hours*60*60));
			
			range_buffer=my_strsep(&range_ptr,"-");
			if(range_buffer==NULL){
				free(period_buffer);
				return ERROR;
			        }

			time_ptr=range_buffer;
			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
				free(period_buffer);
				return ERROR;
			        }
			hours=atoi(time_buffer);

			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
				free(period_buffer);
				return ERROR;
			        }
			minutes=atoi(time_buffer);

			/* calculate the range end time in seconds */
			range_end_time=(unsigned long)((minutes*60)+(hours*60*60));


			/* add the new time range to the time period */
			new_timerange=add_timerange_to_timeperiod(new_timeperiod,day,range_start_time,range_end_time);
			if(new_timerange==NULL){
				free(period_buffer);
				return ERROR;
			        }
		        }
	        }

#ifdef DEBUG0
	printf("xoddefault_add_timeperiod() end\n");
#endif

	return OK;
        }




/* add a new host definition */
int xoddefault_add_host(char *name,char *value, int line){
	host *new_host;
	hostsmember *new_hostsmember;
	char *temp_ptr;
	char *alias;
        char *address;
	char *check_command;
	char *event_handler;
	char *parent;
	char *parents;
	char *parent_hosts;
	char *notification_period;
	int max_attempts;
	int notify_up;
	int notify_down;
	int notify_unreachable;
	int notification_interval;
	int event_handler_enabled;
	int checks_enabled;
	int notifications_enabled;
	int flap_detection_enabled;
	double low_flap_threshold=0.0;
	double high_flap_threshold=0.0;
	int stalk_up;
	int stalk_down;
	int stalk_unreachable;
	int process_perfdata;
	int retain_status_information;
	int retain_nonstatus_information;
	int failure_prediction_enabled;
	char *failure_prediction_options;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xoddefault_add_host() start\n");
#endif

	/* grab the host alias name */
	alias=my_strtok(value,";");

	/* if no alias, use the host name */
	if(alias==NULL)
		alias=name;

	/* grab the host address */
	address=my_strtok(NULL,";");
	if(address==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Address for host '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* get the parent hosts (if any) - no parent host means this host is on the local net */
	parents=my_strtok(NULL,";");

	/* grab the host check command - no command means we won't check the host */
	check_command=my_strtok(NULL,";");
	
	/* get the max number of check attempts */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Max. check attempts for host '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	max_attempts=atoi(temp_ptr);
	if(max_attempts<=0){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid max. check attempts value for host '%s' - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* get the notification interval */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notification interval for host '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	notification_interval=atoi(temp_ptr);
	if(notification_interval<0){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notification interval for host '%s' - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* get the notification timeperiod name */
	notification_period=my_strtok(NULL,";");

	/* get the notifcation on recovery option */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on recovery option for host '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	notify_up=atoi(temp_ptr);
	if(notify_up<0){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify on recovery option for host '%s' - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	if(notify_up>1)
		notify_up=1;

	/* get the notifcation on down option */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on down option for host '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	notify_down=atoi(temp_ptr);
	if(notify_down<0){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify on down option for host '%s' - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	if(notify_down>1)
		notify_down=1;

	/* get the notifcation on unreachable option */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on unreachable option for host '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	notify_unreachable=atoi(temp_ptr);
	if(notify_unreachable<0){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notify on unreachable option for host '%s' - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	if(notify_unreachable>1)
		notify_unreachable=1;

	/* get the command name to run when the host state changes - no command means we won't run one */
	event_handler=my_strtok(NULL,";");

	strip(name);
	strip(alias);
	strip(address);
	strip(check_command);
	strip(event_handler);
	strip(notification_period);

	/* event handler is enabled by default */
	event_handler_enabled=1;

	/* checks of the host are enabled by default */
	checks_enabled=1;

	/* host notifications are enabled by default */
	notifications_enabled=1;

	/* flap detection is enabled by default */
	flap_detection_enabled=1;

	/* flap thresholds default to program-wide values */
	low_flap_threshold=0.0;
	high_flap_threshold=0.0;

	/* state stalking option not supported in default config file - default to disabled */
	stalk_up=0;
	stalk_down=0;
	stalk_unreachable=0;

	/* default is to process performance data */
	process_perfdata=1;

	/* failure prediction is enabled by default (no options) */
	failure_prediction_enabled=1;
	failure_prediction_options=NULL;

	/* default is to retain state information (all types) */
	retain_status_information=1;
	retain_nonstatus_information=1;


	/***** ADD THE HOST ****/

	strip(parents);
	parent_hosts=(char *)malloc(strlen(parents)+1);
	if(parent_hosts==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for parent hosts for for host '%s' - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strcpy(parent_hosts,parents);

	/* add the host definition */
	new_host=add_host(name,alias,address,max_attempts,notify_up,notify_down,notify_unreachable,notification_interval,notification_period,notifications_enabled,check_command,checks_enabled,event_handler,event_handler_enabled,flap_detection_enabled,low_flap_threshold,high_flap_threshold,stalk_up,stalk_down,stalk_unreachable,process_perfdata,failure_prediction_enabled,failure_prediction_options,retain_status_information,retain_nonstatus_information);
	if(new_host==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add host '%s' - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(parent_hosts);
		return ERROR;
	        }

	/* add the parent hosts */
	for(parent=my_strtok(parent_hosts,",");parent!=NULL;parent=my_strtok(NULL,",")){
		new_hostsmember=add_parent_host_to_host(new_host,parent);
		if(new_hostsmember==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add parent host '%s' to host '%s' - Line %d\n",parent,name,line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(parent_hosts);
			return ERROR;
		        }
	        }

	free(parent_hosts);


#ifdef DEBUG0
	printf("xoddefault_add_host() end\n");
#endif

	return OK;
	}




/* add a new contact to the list in memory */
int xoddefault_add_contact(char *name,char *value, int line){
	contact *new_contact;
	commandsmember *new_commandsmember;
	char *service_commands;
	char *host_commands;
	char *command_name;
	char *svc_commands_buffer;
	char *host_commands_buffer;
	char *email;
	char *pager;
	char *alias;
	char *svc_notification_period;
	char *host_notification_period;
	int notify_service_recovery;
	int notify_service_critical;
	int notify_service_warning;
	int notify_service_unknown;
	int notify_host_recovery;
	int notify_host_down;
	int notify_host_unreachable;
	char *temp_ptr;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xoddefault_add_contact() start\n");
#endif

	/* get the alias */
	alias=my_strtok(value,";");

	/* if no alias, use name */
	if(alias==NULL)
		alias=name;

	/* get the service notification timeperiod name */
	svc_notification_period=my_strtok(NULL,";");

	/* get the host notification timeperiod name */
	host_notification_period=my_strtok(NULL,";");

	/* get the notifcation on recovery option for services */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on service recovery option for contact '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	notify_service_recovery=atoi(temp_ptr);
	if(notify_service_recovery>1)
		notify_service_recovery=1;

	/* get the notifcation on critical option for services */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on service critical option for contact '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	notify_service_critical=atoi(temp_ptr);
	if(notify_service_critical>1)
		notify_service_critical=1;

	/* get the notifcation on warning option for services */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on service warning option for contact '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	notify_service_warning=atoi(temp_ptr);
	if(notify_service_warning>1)
		notify_service_warning=1;

	/* get the notifcation on recovery option for hosts */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on host recovery option for contact '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	notify_host_recovery=atoi(temp_ptr);
	if(notify_host_recovery>1)
		notify_host_recovery=1;

	/* get the notifcation on down option for hosts */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on host down option for contact '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	notify_host_down=atoi(temp_ptr);
	if(notify_host_down>1)
		notify_host_down=1;

	/* get the notifcation on unreachable option for hosts */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL || !strcmp(temp_ptr,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notify on host unreachable option for contact '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strip(temp_ptr);
	notify_host_unreachable=atoi(temp_ptr);
	if(notify_host_unreachable>1)
		notify_host_unreachable=1;


	/* get the service notification commands */
	service_commands=my_strtok(NULL,";");
	if(service_commands==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service notification commands for contact '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	svc_commands_buffer=(char *)malloc(strlen(service_commands)+1);
	if(svc_commands_buffer==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service notification commands for contact '%s' - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strcpy(svc_commands_buffer,service_commands);

	/* get the host notification commands */
	host_commands=my_strtok(NULL,";");
	if(host_commands==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host notification commands for contact '%s' is NULL - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	host_commands_buffer=(char *)malloc(strlen(host_commands)+1);
	if(host_commands_buffer==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host notification commands for contact '%s' - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(svc_commands_buffer);
		return ERROR;
	        }
	strcpy(host_commands_buffer,host_commands);

	/* grab the email address */
	email=my_strtok(NULL,";");

	/* grab the pager number */
	pager=my_strtok(NULL,";");


	notify_service_unknown=notify_service_warning;


	/***** ADD THE CONTACT ******/

	new_contact=add_contact(name,alias,email,pager,svc_notification_period,host_notification_period,notify_service_recovery,notify_service_critical,notify_service_warning,notify_service_unknown,notify_host_recovery,notify_host_down,notify_host_unreachable);
	if(new_contact==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contact '%s' - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		free(svc_commands_buffer);
		free(host_commands_buffer);
		return ERROR;
	        }

	/* add all the host notification commands */
	for(command_name=my_strtok(host_commands_buffer,",");command_name!=NULL;command_name=my_strtok(NULL,",")){
		new_commandsmember=add_host_notification_command_to_contact(new_contact,command_name);
		if(new_commandsmember==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add host notification command '%s' to contact '%s' - Line %d\n",command_name,name,line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(svc_commands_buffer);
			free(host_commands_buffer);
			return ERROR;
		        }
	        }

	/* add all the service notification commands */
	for(command_name=my_strtok(svc_commands_buffer,",");command_name!=NULL;command_name=my_strtok(NULL,",")){
		new_commandsmember=add_service_notification_command_to_contact(new_contact,command_name);
		if(new_commandsmember==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add service notification command '%s' to contact '%s' - Line %d\n",command_name,name,line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(svc_commands_buffer);
			free(host_commands_buffer);
			return ERROR;
		        }
	        }

	free(svc_commands_buffer);
	free(host_commands_buffer);

	
#ifdef DEBUG0
	printf("xoddefault_add_contact() end\n");
#endif

	return OK;
	}



/* add a new service escalation to the list in memory */
int xoddefault_add_serviceescalation(char *name,char *value, int line){
	serviceescalation *new_serviceescalation;
	contactgroupsmember *new_contactgroupsmember;
	char *host_name;
	char *svc_description;
	char *temp_ptr;
	int first_notification;
	int last_notification;
	int notification_interval=0;
	char *temp_groups;
	char *group;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xoddefault_add_serviceescalation() start\n");
#endif

	/* get the host name */
        temp_ptr=my_strtok(name,";");

	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service escalation host name is NULL - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	host_name=(char *)malloc(strlen(temp_ptr)+1);
	if(host_name==NULL)
		return ERROR;
	strcpy(host_name,temp_ptr);

	/* get the service description */
	temp_ptr=my_strtok(NULL,"\n");

	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service escalation service description is NULL - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	svc_description=(char *)malloc(strlen(temp_ptr)+1);
	if(svc_description==NULL)
		return ERROR;
	strcpy(svc_description,temp_ptr);

	/* get the first notification */
	temp_ptr=my_strtok(value,"-");
	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service escalation for service '%s' on host '%s' has NULL first notification number - Line %d\n",svc_description,host_name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	first_notification=atoi(temp_ptr);

	/* get the last notification */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service escalation for service '%s' on host '%s' has NULL last notification number - Line %d\n",svc_description,host_name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	last_notification=atoi(temp_ptr);

	/* check notification number sanity... */
	if(first_notification<0 || last_notification<0 || (last_notification!=0 && first_notification>last_notification)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notification range (%d - %d) in escalation for service '%s' on host '%s' is invalid - Line %d\n",first_notification,last_notification,svc_description,host_name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* get the list of contact groups */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notification escalation for service '%s' on host '%s' has NULL contact groups - Line %d\n",svc_description,host_name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* allocate temporary memory for the contact groups */
	temp_groups=(char *)malloc(strlen(temp_ptr)+1);
	if(temp_groups==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate temporary memory for contact groups in escalation for service '%s' on host '%s' - Line %d\n",svc_description,host_name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	strcpy(temp_groups,temp_ptr);

	/* get the notification interval */
	temp_ptr=my_strtok(NULL,"\n");
	if(temp_ptr==NULL)
		notification_interval=0;
	else
		notification_interval=atoi(temp_ptr);


	/***** ADD THE SERVICE ESCALATION ******/

	new_serviceescalation=add_serviceescalation(host_name,svc_description,first_notification,last_notification,notification_interval);
	if(new_serviceescalation==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add service escalation for service '%s' on host '%s'  - Line %d\n",svc_description,host_name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* add the contact groups */
	for(group=my_strtok(temp_groups,",");group!=NULL;group=my_strtok(NULL,",")){
		new_contactgroupsmember=add_contactgroup_to_serviceescalation(new_serviceescalation,group);
		if(new_contactgroupsmember==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contactgroup '%s' to escalation for service '%s' on host '%s' - Line %d\n",group,svc_description,host_name,line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(temp_groups);
			return ERROR;
		        }
	        }


#ifdef DEBUG0
	printf("xoddefault_add_serviceescalation() end\n");
#endif

	return OK;
        }




/* add a new hostgroup escalation to the list in memory */
int xoddefault_add_hostgroupescalation(char *name,char *value, int line){
	hostgroupescalation *new_hostgroupescalation;
	contactgroupsmember *new_contactgroupsmember;
	char *temp_ptr;
	int first_notification;
	int last_notification;
	int notification_interval=0;
	char *temp_groups;
	char *group;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xoddefault_add_serviceescalation() start\n");
#endif

	if(name==NULL || !strcmp(name,"")){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Hostgroup escalation group name is NULL - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* get the first notification */
	temp_ptr=my_strtok(value,"-");
	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Hostgroup escalation for hostgroup '%s' has NULL first notification number - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	first_notification=atoi(temp_ptr);

	/* get the last notification */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Hostgroup escalation for hostgroup '%s' has NULL last notification number - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	last_notification=atoi(temp_ptr);

	/* check notification number sanity... */
	if(first_notification<0 || last_notification<0 || (last_notification!=0 && first_notification>last_notification)){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notification range (%d - %d) in escalation for hostgroup '%s' is invalid - Line %d\n",first_notification,last_notification,name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* get the list of contact groups */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Notification escalation for hostgroup '%s' has NULL contact groups - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* allocate temporary memory for the contact groups */
	temp_groups=(char *)malloc(strlen(temp_ptr)+1);
	if(temp_groups==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate temporary memory for contact groups in escalation for hostgroup '%s' - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	strcpy(temp_groups,temp_ptr);

	/* get the notification interval */
	temp_ptr=my_strtok(NULL,"\n");
	if(temp_ptr==NULL)
		notification_interval=0;
	else
		notification_interval=atoi(temp_ptr);

	/***** ADD THE HOSTGROUP ESCALATION ******/

	new_hostgroupescalation=add_hostgroupescalation(name,first_notification,last_notification,notification_interval);
	if(new_hostgroupescalation==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add hostgroup escalation for hostgroup '%s'  - Line %d\n",name,line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* add the contact groups */
	for(group=my_strtok(temp_groups,",");group!=NULL;group=my_strtok(NULL,",")){
		new_contactgroupsmember=add_contactgroup_to_hostgroupescalation(new_hostgroupescalation,group);
		if(new_contactgroupsmember==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contactgroup '%s' to escalation for hostgroup '%s' - Line %d\n",group,name,line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(temp_groups);
			return ERROR;
		        }
	        }



#ifdef DEBUG0
	printf("xoddefault_add_hostgroupescalation() end\n");
#endif

	return OK;
        }




/* add a new service dependency definition */
int xoddefault_add_servicedependency(char *name,char *value, int line){
	servicedependency *new_servicedependency;
	char *temp_ptr;
	char *dependent_host_name;
	char *dependent_service_description;
	char *host_name;
	char *service_description;
	int fail_on_ok=0;
	int fail_on_warning=0;
	int fail_on_unknown=0;
	int fail_on_critical=0;
	int x;
	
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xoddefault_add_host() start\n");
#endif

	/* grab the dependent host name */
	temp_ptr=my_strtok(name,";");
	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host name for dependent service is NULL - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	dependent_host_name=(char *)malloc(strlen(temp_ptr)+1);
	if(dependent_host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for dependent host name - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strcpy(dependent_host_name,temp_ptr);

	/* grab the dependent service description */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Description for dependent service is NULL - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	dependent_service_description=(char *)malloc(strlen(temp_ptr)+1);
	if(dependent_service_description==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for dependent service description - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strcpy(dependent_service_description,temp_ptr);

	/* grab the host name */
	temp_ptr=my_strtok(value,";");
	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host name is NULL - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	host_name=(char *)malloc(strlen(temp_ptr)+1);
	if(host_name==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for host name - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strcpy(host_name,temp_ptr);

	/* grab the service description */
	temp_ptr=my_strtok(NULL,";");
	if(temp_ptr==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Description for service is NULL - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	service_description=(char *)malloc(strlen(temp_ptr)+1);
	if(service_description==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not allocate memory for service description - Line %d\n",line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	strcpy(service_description,temp_ptr);


	strip(dependent_host_name);
	strip(dependent_service_description);
	strip(host_name);
	strip(service_description);


	/* reset the failure options */
	fail_on_ok=FALSE;
	fail_on_warning=FALSE;
	fail_on_unknown=FALSE;
	fail_on_critical=FALSE;

	/* get the execution failure options */
	temp_ptr=my_strtok(NULL,";");

	/* there were execution failure options specified... */
	if(temp_ptr!=NULL && strcmp(temp_ptr,"")){

		/* get all the execution failure options */
		for(x=0;temp_ptr[x]!='\x0';x++){
			if(temp_ptr[x]=='o')
				fail_on_ok=TRUE;
			else if(temp_ptr[x]=='w')
				fail_on_warning=TRUE;
			else if(temp_ptr[x]=='u')
				fail_on_unknown=TRUE;
			else if(temp_ptr[x]=='c')
				fail_on_critical=TRUE;
			else{
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: '%c' is not a valid service execution dependency option for service '%s' on host '%s'  - Line %d\n",temp_ptr[x],dependent_service_description,dependent_host_name,line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				free(dependent_host_name);
				free(dependent_service_description);
				return ERROR;
			        }
		        }

		/***** ADD THE SERVICE EXECUTION DEPENDENCY ****/

		new_servicedependency=add_service_dependency(dependent_host_name,dependent_service_description,host_name,service_description,EXECUTION_DEPENDENCY,fail_on_ok,fail_on_warning,fail_on_unknown,fail_on_critical);
		if(new_servicedependency==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add service execution dependency for service '%s' on host '%s'  - Line %d\n",dependent_service_description,dependent_host_name,line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(dependent_host_name);
			free(dependent_service_description);
			return ERROR;
	                }

	        }


	/* reset the failure options */
	fail_on_ok=FALSE;
	fail_on_warning=FALSE;
	fail_on_unknown=FALSE;
	fail_on_critical=FALSE;

	/* get the notification failure options */
	temp_ptr=my_strtok(NULL,"\n");

	/* there were notification failure options specified... */
	if(temp_ptr!=NULL && strcmp(temp_ptr,"")){

		/* get all the notifcation failure options */
		for(x=0;temp_ptr[x]!='\x0';x++){
			if(temp_ptr[x]=='o')
				fail_on_ok=TRUE;
			else if(temp_ptr[x]=='w')
				fail_on_warning=TRUE;
			else if(temp_ptr[x]=='u')
				fail_on_unknown=TRUE;
			else if(temp_ptr[x]=='c')
				fail_on_critical=TRUE;
			else{
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: '%c' is not a valid service notification dependency option for service '%s' on host '%s'  - Line %d\n",temp_ptr[x],dependent_service_description,dependent_host_name,line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				free(dependent_host_name);
				free(dependent_service_description);
				return ERROR;
			        }
		        }

		/***** ADD THE SERVICE NOTIFICATION DEPENDENCY ****/

		new_servicedependency=add_service_dependency(dependent_host_name,dependent_service_description,host_name,service_description,NOTIFICATION_DEPENDENCY,fail_on_ok,fail_on_warning,fail_on_unknown,fail_on_critical);
		if(new_servicedependency==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add service notification dependency for service '%s' on host '%s'  - Line %d\n",dependent_service_description,dependent_host_name,line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			free(dependent_host_name);
			free(dependent_service_description);
			return ERROR;
	                }

	        }


	free(dependent_host_name);
	free(dependent_service_description);

#ifdef DEBUG0
	printf("xoddefault_add_servicedependency() end\n");
#endif

	return OK;
	}


