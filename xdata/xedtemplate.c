/*****************************************************************************
 *
 * XEDTEMPLATE.C - Template-based extended information data input routines
 *
 * Copyright (c) 2001-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   02-20-2002
 *
 * Description:
 *
 *    1) Read
 *    2) Resolve
 *    3) Replicate
 *    4) Register
 *    5) Cleanup
 *
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
#include "../cgi/edata.h"


/**** DATA INPUT-SPECIFIC HEADER FILES ****/

#include "xedtemplate.h"



xedtemplate_hostextinfo *xedtemplate_hostextinfo_list=NULL;
xedtemplate_serviceextinfo *xedtemplate_serviceextinfo_list=NULL;

int xedtemplate_current_object_type=XEDTEMPLATE_NONE;

extern host *host_list;






/******************************************************************/
/************* TOP-LEVEL CONFIG DATA INPUT FUNCTION ***************/
/******************************************************************/

/* process all config files - CGIs pass in name of CGI config file */
int xedtemplate_read_extended_object_config_data(char *cgi_config_file, int options){
	char config_file[MAX_FILENAME_LENGTH];
	char input[MAX_XEDTEMPLATE_INPUT_BUFFER];
	char *temp_ptr;
	FILE *fpm;
	int result=OK;

#ifdef DEBUG0
	printf("xedtemplate_read_extended_object_config_data() start\n");
#endif

	/* open the CGI config file for reading (we need to find all the config files to read) */
	fpm=fopen(cgi_config_file,"r");
	if(fpm==NULL)
		return ERROR;

	/* initialize object definition lists to NULL - CGIs core dump w/o this (why?) */
	xedtemplate_hostextinfo_list=NULL;
	xedtemplate_serviceextinfo_list=NULL;

	/* read in all lines from the main config file */
	for(fgets(input,sizeof(input)-1,fpm);!feof(fpm);fgets(input,sizeof(input)-1,fpm)){

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]==';' || input[0]=='\x0' || input[0]=='\n' || input[0]=='\r')
			continue;

		/* strip input */
		xedtemplate_strip(input);

		temp_ptr=strtok(input,"=");
		if(temp_ptr==NULL)
			continue;

		/* skip lines that don't specify the config file location */
		if(strcmp(temp_ptr,"xedtemplate_config_file"))
			continue;

		/* get the config file name */
		temp_ptr=strtok(NULL,"\n");
		if(temp_ptr==NULL)
			continue;

		strncpy(config_file,temp_ptr,sizeof(config_file)-1);
		config_file[sizeof(config_file)-1]='\x0';

		/* process the config file... */
		result=xedtemplate_process_config_file(config_file,options);

		/* if there was an error processing the config file, break out of loop */
		if(result==ERROR)
			break;
	        }

	fclose(fpm);

	/* resolve objects definitions */
	result=xedtemplate_resolve_objects();

	/* duplicate object definitions */
	result=xedtemplate_duplicate_objects();

	/* register objects */
	result=xedtemplate_register_objects();

	/* cleanup */
	xedtemplate_free_memory();

#ifdef DEBUG0
	printf("xedtemplate_read_extended_object_config_data() end\n");
#endif

	return result;
	}




/* process data in a specific config file */
int xedtemplate_process_config_file(char *filename, int options){
	FILE *fp;
	char input[MAX_XEDTEMPLATE_INPUT_BUFFER];
	int in_definition=FALSE;
	char *temp_ptr;
	int current_line=0;
	int result=OK;

#ifdef DEBUG0
	printf("xedtemplate_process_config_file() start\n");
#endif

	/* open the config file for reading */
	fp=fopen(filename,"r");
	if(fp==NULL)
		return ERROR;

	/* read in all lines from the config file */
	for(fgets(input,sizeof(input)-1,fp);!feof(fp);fgets(input,sizeof(input)-1,fp)){

		current_line++;

		/* skip empty lines */
		if(input[0]=='#' || input[0]==';' || input[0]=='\r' || input[0]=='\n')
			continue;

		/* grab data before comment or newline */
		temp_ptr=strtok(input,";\r\n");
		if(temp_ptr==NULL)
			continue;
		else{
			strncpy(input,temp_ptr,sizeof(input)-1);
			input[sizeof(input)-1]='\x0';
		        }

		/* strip input */
		xedtemplate_strip(input);

		/* skip blank lines */
		if(input[0]=='\x0')
			continue;

		/* this is the start of an object definition */
		else if(strstr(input,"define ")==input){

			/* get the type of object we're defining... */
			temp_ptr=strtok(input," \t{");
			temp_ptr=strtok(NULL," \t{");

			/* make sure an object type is specified... */
			if(temp_ptr==NULL)
				continue;

			strncpy(input,temp_ptr,sizeof(input)-1);
			input[sizeof(input)-1]='\x0';

			/* check validity of object type */
			if(strcmp(input,"hostextinfo") && strcmp(input,"serviceextinfo"))
				continue;

			/* we're already in an object definition... */
			if(in_definition==TRUE)
				continue;

			/* start a new definition */
			xedtemplate_begin_object_definition(input);

			in_definition=TRUE;
		        }

		/* this is the close of an object definition */
		else if(!strcmp(input,"}") && in_definition==TRUE){

			in_definition=FALSE;

			/* close out current definition */
			xedtemplate_end_object_definition();
		        }

		/* this is a directive inside an object definition */
		else if(in_definition==TRUE){
			
			/* add directive to object definition */
			xedtemplate_add_object_property(input);
		        }
	        }

	/* close file */
	fclose(fp);

	/* whoops - EOF while we were in the middle of an object definition... */
	if(in_definition==TRUE && result==OK){
		result=ERROR;
	        }

#ifdef DEBUG0
	printf("xedtemplate_process_config_file() end\n");
#endif

	return result;
        }



/* strip newline, carriage return, and tab characters from beginning and end of a string */
void xedtemplate_strip(char *buffer){
	int x;
	char ch;
	int a,b;

	if(buffer==NULL)
		return;

	/* strip end of string */
	x=(int)strlen(buffer);
	for(;x>=1;x--){
		if(buffer[x-1]==' ' || buffer[x-1]=='\n' || buffer[x-1]=='\r' || buffer[x-1]=='\t' || buffer[x-1]==13)
			buffer[x-1]='\x0';
		else
			break;
	        }

	/* reverse string */
	x=(int)strlen(buffer);
	if(x==0)
		return;
	for(a=0,b=x-1;a<b;a++,b--){
		ch=buffer[a];
		buffer[a]=buffer[b];
		buffer[b]=ch;
	        }

	/* strip beginning of string (now end of string) */
	for(;x>=1;x--){
		if(buffer[x-1]==' ' || buffer[x-1]=='\n' || buffer[x-1]=='\r' || buffer[x-1]=='\t' || buffer[x-1]==13)
			buffer[x-1]='\x0';
		else
			break;
	        }

	/* reverse string again */
	x=(int)strlen(buffer);
	if(x==0)
		return;
	for(a=0,b=x-1;a<b;a++,b--){
		ch=buffer[a];
		buffer[a]=buffer[b];
		buffer[b]=ch;
	        }

	return;
	}





/******************************************************************/
/***************** OBJECT DEFINITION FUNCTIONS ********************/
/******************************************************************/

/* starts a new object definition */
int xedtemplate_begin_object_definition(char *input){
	int result=OK;
	xedtemplate_hostextinfo *new_hostextinfo;
	xedtemplate_serviceextinfo *new_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_begin_object_definition() start\n");
#endif

	if(!strcmp(input,"hostextinfo"))
		xedtemplate_current_object_type=XEDTEMPLATE_HOSTEXTINFO;
	else if(!strcmp(input,"serviceextinfo"))
		xedtemplate_current_object_type=XEDTEMPLATE_SERVICEEXTINFO;
	else
		return ERROR;


	/* add a new (blank) object */
	switch(xedtemplate_current_object_type){

	case XEDTEMPLATE_HOSTEXTINFO:

		/* allocate memory */
		new_hostextinfo=(xedtemplate_hostextinfo *)malloc(sizeof(xedtemplate_hostextinfo));
		if(new_hostextinfo==NULL)
			return ERROR;

		new_hostextinfo->template=NULL;
		new_hostextinfo->name=NULL;
		new_hostextinfo->host_name=NULL;
		new_hostextinfo->hostgroup=NULL;
		new_hostextinfo->notes_url=NULL;
		new_hostextinfo->icon_image=NULL;
		new_hostextinfo->icon_image_alt=NULL;
		new_hostextinfo->vrml_image=NULL;
		new_hostextinfo->gd2_image=NULL;
		new_hostextinfo->x_2d=-1;
		new_hostextinfo->y_2d=-1;
		new_hostextinfo->x_3d=0.0;
		new_hostextinfo->y_3d=0.0;
		new_hostextinfo->z_3d=0.0;
		new_hostextinfo->have_2d_coords=FALSE;
		new_hostextinfo->have_3d_coords=FALSE;

		new_hostextinfo->has_been_resolved=FALSE;
		new_hostextinfo->register_object=TRUE;

		/* add new timeperiod to head of list in memory */
		new_hostextinfo->next=xedtemplate_hostextinfo_list;
		xedtemplate_hostextinfo_list=new_hostextinfo;

		break;

	case XEDTEMPLATE_SERVICEEXTINFO:

		/* allocate memory */
		new_serviceextinfo=(xedtemplate_serviceextinfo *)malloc(sizeof(xedtemplate_serviceextinfo));
		if(new_serviceextinfo==NULL)
			return ERROR;

		new_serviceextinfo->template=NULL;
		new_serviceextinfo->name=NULL;
		new_serviceextinfo->host_name=NULL;
		new_serviceextinfo->hostgroup=NULL;
		new_serviceextinfo->service_description=NULL;
		new_serviceextinfo->notes_url=NULL;
		new_serviceextinfo->icon_image=NULL;
		new_serviceextinfo->icon_image_alt=NULL;

		new_serviceextinfo->has_been_resolved=FALSE;
		new_serviceextinfo->register_object=TRUE;

		/* add new timeperiod to head of list in memory */
		new_serviceextinfo->next=xedtemplate_serviceextinfo_list;
		xedtemplate_serviceextinfo_list=new_serviceextinfo;

		break;

	default:
		break;
	        }

#ifdef DEBUG0
	printf("xedtemplate_begin_object_definition() end\n");
#endif

	return result;
        }



/* adds a property to an object definition */
int xedtemplate_add_object_property(char *input){
	int result=OK;
	char variable[MAX_XEDTEMPLATE_INPUT_BUFFER];
	char value[MAX_XEDTEMPLATE_INPUT_BUFFER];
	char *temp_ptr;
	xedtemplate_hostextinfo *temp_hostextinfo;
	xedtemplate_serviceextinfo *temp_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_add_object_property() start\n");
#endif

	/* get variable name */
	temp_ptr=strtok(input," \t");
	strncpy(variable,temp_ptr,sizeof(variable)-1);
	variable[sizeof(variable)-1]='\x0';
	xedtemplate_strip(variable);

	/* get variable value */
	temp_ptr=strtok(NULL,"\n");
	if(temp_ptr==NULL)
		return ERROR;
	strncpy(value,temp_ptr,sizeof(value)-1);
	value[sizeof(value)-1]='\x0';
	xedtemplate_strip(value);

	switch(xedtemplate_current_object_type){

	case XEDTEMPLATE_HOSTEXTINFO:
		
		temp_hostextinfo=xedtemplate_hostextinfo_list;

		if(!strcmp(variable,"use")){
			temp_hostextinfo->template=(char *)malloc(strlen(value)+1);
			if(temp_hostextinfo->template==NULL)
				return ERROR;
			strcpy(temp_hostextinfo->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_hostextinfo->name=(char *)malloc(strlen(value)+1);
			if(temp_hostextinfo->name==NULL)
				return ERROR;
			strcpy(temp_hostextinfo->name,value);
		        }
		else if(!strcmp(variable,"host_name")){
			temp_hostextinfo->host_name=(char *)malloc(strlen(value)+1);
			if(temp_hostextinfo->host_name==NULL)
				return ERROR;
			strcpy(temp_hostextinfo->host_name,value);
		        }
		else if(!strcmp(variable,"hostgroup")){
			temp_hostextinfo->hostgroup=(char *)malloc(strlen(value)+1);
			if(temp_hostextinfo->hostgroup==NULL)
				return ERROR;
			strcpy(temp_hostextinfo->hostgroup,value);
		        }
		else if(!strcmp(variable,"notes_url")){
			temp_hostextinfo->notes_url=(char *)malloc(strlen(value)+1);
			if(temp_hostextinfo->notes_url==NULL)
				return ERROR;
			strcpy(temp_hostextinfo->notes_url,value);
		        }
		else if(!strcmp(variable,"icon_image")){
			temp_hostextinfo->icon_image=(char *)malloc(strlen(value)+1);
			if(temp_hostextinfo->icon_image==NULL)
				return ERROR;
			strcpy(temp_hostextinfo->icon_image,value);
		        }
		else if(!strcmp(variable,"icon_image_alt")){
			temp_hostextinfo->icon_image_alt=(char *)malloc(strlen(value)+1);
			if(temp_hostextinfo->icon_image_alt==NULL)
				return ERROR;
			strcpy(temp_hostextinfo->icon_image_alt,value);
		        }
		else if(!strcmp(variable,"vrml_image")){
			temp_hostextinfo->vrml_image=(char *)malloc(strlen(value)+1);
			if(temp_hostextinfo->vrml_image==NULL)
				return ERROR;
			strcpy(temp_hostextinfo->vrml_image,value);
		        }
		else if(!strcmp(variable,"gd2_image")|| !strcmp(variable,"statusmap_image")){
			temp_hostextinfo->gd2_image=(char *)malloc(strlen(value)+1);
			if(temp_hostextinfo->gd2_image==NULL)
				return ERROR;
			strcpy(temp_hostextinfo->gd2_image,value);
		        }
		else if(!strcmp(variable,"2d_coords")){
			temp_ptr=strtok(value,", ");
			if(temp_ptr==NULL)
				return ERROR;
			temp_hostextinfo->x_2d=atoi(temp_ptr);
			temp_ptr=strtok(NULL,", ");
			if(temp_ptr==NULL)
				return ERROR;
			temp_hostextinfo->y_2d=atoi(temp_ptr);
			temp_hostextinfo->have_2d_coords=TRUE;
		        }
		else if(!strcmp(variable,"3d_coords")){
			temp_ptr=strtok(value,", ");
			if(temp_ptr==NULL)
				return ERROR;
			temp_hostextinfo->x_3d=strtod(temp_ptr,NULL);
			temp_ptr=strtok(NULL,", ");
			if(temp_ptr==NULL)
				return ERROR;
			temp_hostextinfo->y_3d=strtod(temp_ptr,NULL);
			temp_ptr=strtok(NULL,", ");
			if(temp_ptr==NULL)
				return ERROR;
			temp_hostextinfo->z_3d=strtod(temp_ptr,NULL);
			temp_hostextinfo->have_3d_coords=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_hostextinfo->register_object=(atoi(value)>0)?TRUE:FALSE;

		break;
	
	case XEDTEMPLATE_SERVICEEXTINFO:
		
		temp_serviceextinfo=xedtemplate_serviceextinfo_list;

		if(!strcmp(variable,"use")){
			temp_serviceextinfo->template=(char *)malloc(strlen(value)+1);
			if(temp_serviceextinfo->template==NULL)
				return ERROR;
			strcpy(temp_serviceextinfo->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_serviceextinfo->name=(char *)malloc(strlen(value)+1);
			if(temp_serviceextinfo->name==NULL)
				return ERROR;
			strcpy(temp_serviceextinfo->name,value);
		        }
		else if(!strcmp(variable,"host_name")){
			temp_serviceextinfo->host_name=(char *)malloc(strlen(value)+1);
			if(temp_serviceextinfo->host_name==NULL)
				return ERROR;
			strcpy(temp_serviceextinfo->host_name,value);
		        }
		else if(!strcmp(variable,"hostgroup")){
			temp_serviceextinfo->hostgroup=(char *)malloc(strlen(value)+1);
			if(temp_serviceextinfo->hostgroup==NULL)
				return ERROR;
			strcpy(temp_serviceextinfo->hostgroup,value);
		        }
		else if(!strcmp(variable,"service_description")){
			temp_serviceextinfo->service_description=(char *)malloc(strlen(value)+1);
			if(temp_serviceextinfo->service_description==NULL)
				return ERROR;
			strcpy(temp_serviceextinfo->service_description,value);
		        }
		else if(!strcmp(variable,"notes_url")){
			temp_serviceextinfo->notes_url=(char *)malloc(strlen(value)+1);
			if(temp_serviceextinfo->notes_url==NULL)
				return ERROR;
			strcpy(temp_serviceextinfo->notes_url,value);
		        }
		else if(!strcmp(variable,"icon_image")){
			temp_serviceextinfo->icon_image=(char *)malloc(strlen(value)+1);
			if(temp_serviceextinfo->icon_image==NULL)
				return ERROR;
			strcpy(temp_serviceextinfo->icon_image,value);
		        }
		else if(!strcmp(variable,"icon_image_alt")){
			temp_serviceextinfo->icon_image_alt=(char *)malloc(strlen(value)+1);
			if(temp_serviceextinfo->icon_image_alt==NULL)
				return ERROR;
			strcpy(temp_serviceextinfo->icon_image_alt,value);
		        }
		else if(!strcmp(variable,"register"))
			temp_serviceextinfo->register_object=(atoi(value)>0)?TRUE:FALSE;

		break;

	default:
		break;
	        }

#ifdef DEBUG0
	printf("xedtemplate_add_object_property() end\n");
#endif

	return result;
        }



/* completes an object definition */
int xedtemplate_end_object_definition(void){

#ifdef DEBUG0
	printf("xedtemplate_end_object_definition() start\n");
#endif

	xedtemplate_current_object_type=XEDTEMPLATE_NONE;

#ifdef DEBUG0
	printf("xedtemplate_end_object_definition() end\n");
#endif

	return OK;
        }





/******************************************************************/
/***************** OBJECT DUPLICATION FUNCTIONS *******************/
/******************************************************************/


/* duplicates object definitions */
int xedtemplate_duplicate_objects(void){
	xedtemplate_hostextinfo *temp_hostextinfo;
	xedtemplate_serviceextinfo *temp_serviceextinfo;
	char *host_names;
	char *hostgroup_names;
	char *temp_ptr;
	int first_item;
	int result=OK;
	host *temp_host;
	hostgroup *temp_hostgroup;

#ifdef DEBUG0
	printf("xedtemplate_duplicate_objects() start\n");
#endif


	/* duplicate hostextinfo definitions that contain multiple host names */
	for(temp_hostextinfo=xedtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){

		if(temp_hostextinfo->host_name==NULL)
			continue;

		if(!strstr(temp_hostextinfo->host_name,","))
			continue;

		/* allocate memory for host name list */
		host_names=(char *)malloc(strlen(temp_hostextinfo->host_name)+1);
		if(host_names==NULL)
			continue;

		strcpy(host_names,temp_hostextinfo->host_name);

		/* duplicate service entries */
		first_item=TRUE;
		for(temp_ptr=strtok(host_names,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){

			/* existing definition gets first host name (memory has already been allocated) */
			if(first_item==TRUE){
				strcpy(temp_hostextinfo->host_name,temp_ptr);
				first_item=FALSE;
				continue;
			        }

			/* duplicate hostextinfo definition */
			result=xedtemplate_duplicate_hostextinfo(temp_hostextinfo,temp_ptr);

			/* exit on error */
			if(result==ERROR){
				free(host_names);
				return ERROR;
			        }
		        }

		/* free memory we used for host name list */
		free(host_names);
	        }


	/* duplicate hostextinfo definitions that contain one or more hostgroups - we break some "rules" here, but what the hell... */
	for(temp_hostextinfo=xedtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){

		if(temp_hostextinfo->hostgroup==NULL)
			continue;

		/* allocate memory for host name list */
		hostgroup_names=(char *)malloc(strlen(temp_hostextinfo->hostgroup)+1);
		if(hostgroup_names==NULL)
			continue;

		strcpy(hostgroup_names,temp_hostextinfo->hostgroup);

		/* duplicate host entries */
		first_item=TRUE;
		for(temp_ptr=strtok(hostgroup_names,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){

			/* find the hostgroup */
			temp_hostgroup=find_hostgroup(temp_ptr,NULL);
			if(temp_hostgroup==NULL)
				continue;

			/* find all hosts in the hostgroup */
			for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

				/* is this host in the specified hostgroup? */
				if(is_host_member_of_hostgroup(temp_hostgroup,temp_host)==FALSE)
					continue;

				/* if this is the first duplication, see if we can use the existing entry */
				if(first_item==TRUE){

					if(temp_hostextinfo->host_name==NULL){
						temp_hostextinfo->host_name=(char *)malloc(strlen(temp_host->name)+1);
						if(temp_hostextinfo->host_name==NULL){
							free(hostgroup_names);
							return ERROR;
						        }
						strcpy(temp_hostextinfo->host_name,temp_host->name);
					        }
					else{
						result=xedtemplate_duplicate_hostextinfo(temp_hostextinfo,temp_host->name);
						if(result==ERROR){
							free(hostgroup_names);
							return ERROR;
						        }
					        }

					first_item=FALSE;
					continue;
				        }

				/* duplicate hostextinfo definition */
				result=xedtemplate_duplicate_hostextinfo(temp_hostextinfo,temp_host->name);

				/* exit on error */
				if(result==ERROR){
					free(hostgroup_names);
					return ERROR;
			                }
			        }
		        }

		/* free memory we used for hostgroup name list */
		free(hostgroup_names);
	        }


	/* duplicate serviceextinfo definitions that contain multiple host names */
	for(temp_serviceextinfo=xedtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){

		if(temp_serviceextinfo->host_name==NULL)
			continue;

		if(!strstr(temp_serviceextinfo->host_name,","))
			continue;

		/* allocate memory for host name list */
		host_names=(char *)malloc(strlen(temp_serviceextinfo->host_name)+1);
		if(host_names==NULL)
			continue;

		strcpy(host_names,temp_serviceextinfo->host_name);

		/* duplicate service entries */
		first_item=TRUE;
		for(temp_ptr=strtok(host_names,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){

			/* existing definition gets first host name (memory has already been allocated) */
			if(first_item==TRUE){
				strcpy(temp_serviceextinfo->host_name,temp_ptr);
				first_item=FALSE;
				continue;
			        }

			/* duplicate serviceextinfo definition */
			result=xedtemplate_duplicate_serviceextinfo(temp_serviceextinfo,temp_ptr);

			/* exit on error */
			if(result==ERROR){
				free(host_names);
				return ERROR;
			        }
		        }

		/* free memory we used for host name list */
		free(host_names);
	        }


	/* duplicate serviceextinfo definitions that contain one or more hostgroups */
	for(temp_serviceextinfo=xedtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){

		if(temp_serviceextinfo->hostgroup==NULL)
			continue;

		/* allocate memory for host name list */
		hostgroup_names=(char *)malloc(strlen(temp_serviceextinfo->hostgroup)+1);
		if(hostgroup_names==NULL)
			continue;

		strcpy(hostgroup_names,temp_serviceextinfo->hostgroup);

		/* duplicate service entries */
		first_item=TRUE;
		for(temp_ptr=strtok(hostgroup_names,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){

			/* find the hostgroup */
			temp_hostgroup=find_hostgroup(temp_ptr,NULL);
			if(temp_hostgroup==NULL)
				continue;

			/* find all hosts in the hostgroup */
			for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

				/* is this host in the specified hostgroup? */
				if(is_host_member_of_hostgroup(temp_hostgroup,temp_host)==FALSE)
					continue;

				/* if this is the first duplication, see if we can use the existing entry */
				if(first_item==TRUE){

					if(temp_serviceextinfo->host_name==NULL){
						temp_serviceextinfo->host_name=(char *)malloc(strlen(temp_host->name)+1);
						if(temp_serviceextinfo->host_name==NULL){
							free(hostgroup_names);
							return ERROR;
						        }
						strcpy(temp_serviceextinfo->host_name,temp_host->name);
					        }
					else{
						result=xedtemplate_duplicate_serviceextinfo(temp_serviceextinfo,temp_host->name);
						if(result==ERROR){
							free(hostgroup_names);
							return ERROR;
						        }
					        }

					first_item=FALSE;
					continue;
				        }

				/* duplicate serviceextinfo definition */
				result=xedtemplate_duplicate_serviceextinfo(temp_serviceextinfo,temp_host->name);

				/* exit on error */
				if(result==ERROR){
					free(hostgroup_names);
					return ERROR;
			                }
			        }
		        }

		/* free memory we used for hostgroup name list */
		free(hostgroup_names);
	        }


#ifdef DEBUG0
	printf("xedtemplate_duplicate_objects() end\n");
#endif

	return OK;
        }



/* duplicates a hostextinfo object definition */
int xedtemplate_duplicate_hostextinfo(xedtemplate_hostextinfo *this_hostextinfo, char *host_name){
	xedtemplate_hostextinfo *new_hostextinfo;

#ifdef DEBUG0
	printf("xedtemplate_duplicate_hostextinfo() start\n");
#endif

	new_hostextinfo=(xedtemplate_hostextinfo *)malloc(sizeof(xedtemplate_hostextinfo));
	if(new_hostextinfo==NULL)
		return ERROR;

	new_hostextinfo->template=NULL;
	new_hostextinfo->name=NULL;
	new_hostextinfo->host_name=NULL;
	new_hostextinfo->hostgroup=NULL;
	new_hostextinfo->notes_url=NULL;
	new_hostextinfo->icon_image=NULL;
	new_hostextinfo->icon_image_alt=NULL;
	new_hostextinfo->vrml_image=NULL;
	new_hostextinfo->gd2_image=NULL;

	/* duplicate strings (host_name member is passed in) */
	if(host_name!=NULL){
		new_hostextinfo->host_name=(char *)malloc(strlen(host_name)+1);
		if(new_hostextinfo->host_name!=NULL)
			strcpy(new_hostextinfo->host_name,host_name);
	        }
	if(this_hostextinfo->template!=NULL){
		new_hostextinfo->template=(char *)malloc(strlen(this_hostextinfo->template)+1);
		if(new_hostextinfo->template!=NULL)
			strcpy(new_hostextinfo->template,this_hostextinfo->template);
	        }
	if(this_hostextinfo->name!=NULL){
		new_hostextinfo->name=(char *)malloc(strlen(this_hostextinfo->name)+1);
		if(new_hostextinfo->name!=NULL)
			strcpy(new_hostextinfo->name,this_hostextinfo->name);
	        }
	if(this_hostextinfo->hostgroup!=NULL){
		new_hostextinfo->hostgroup=(char *)malloc(strlen(this_hostextinfo->hostgroup)+1);
		if(new_hostextinfo->hostgroup!=NULL)
			strcpy(new_hostextinfo->hostgroup,this_hostextinfo->hostgroup);
	        }
	if(this_hostextinfo->notes_url!=NULL){
		new_hostextinfo->notes_url=(char *)malloc(strlen(this_hostextinfo->notes_url)+1);
		if(new_hostextinfo->notes_url!=NULL)
			strcpy(new_hostextinfo->notes_url,this_hostextinfo->notes_url);
	        }
	if(this_hostextinfo->icon_image!=NULL){
		new_hostextinfo->icon_image=(char *)malloc(strlen(this_hostextinfo->icon_image)+1);
		if(new_hostextinfo->icon_image!=NULL)
			strcpy(new_hostextinfo->icon_image,this_hostextinfo->icon_image);
	        }
	if(this_hostextinfo->icon_image_alt!=NULL){
		new_hostextinfo->icon_image_alt=(char *)malloc(strlen(this_hostextinfo->icon_image_alt)+1);
		if(new_hostextinfo->icon_image_alt!=NULL)
			strcpy(new_hostextinfo->icon_image_alt,this_hostextinfo->icon_image_alt);
	        }
	if(this_hostextinfo->vrml_image!=NULL){
		new_hostextinfo->vrml_image=(char *)malloc(strlen(this_hostextinfo->vrml_image)+1);
		if(new_hostextinfo->vrml_image!=NULL)
			strcpy(new_hostextinfo->vrml_image,this_hostextinfo->vrml_image);
	        }
	if(this_hostextinfo->gd2_image!=NULL){
		new_hostextinfo->gd2_image=(char *)malloc(strlen(this_hostextinfo->gd2_image)+1);
		if(new_hostextinfo->gd2_image!=NULL)
			strcpy(new_hostextinfo->gd2_image,this_hostextinfo->gd2_image);
	        }

	/* duplicate non-string members */
	new_hostextinfo->x_2d=this_hostextinfo->x_2d;
	new_hostextinfo->y_2d=this_hostextinfo->y_2d;
	new_hostextinfo->have_2d_coords=this_hostextinfo->have_2d_coords;
	new_hostextinfo->x_3d=this_hostextinfo->x_3d;
	new_hostextinfo->y_3d=this_hostextinfo->y_3d;
	new_hostextinfo->z_3d=this_hostextinfo->z_3d;
	new_hostextinfo->have_3d_coords=this_hostextinfo->have_3d_coords;

	new_hostextinfo->has_been_resolved=this_hostextinfo->has_been_resolved;
	new_hostextinfo->register_object=this_hostextinfo->register_object;

	/* add new object to head of list */
	new_hostextinfo->next=xedtemplate_hostextinfo_list;
	xedtemplate_hostextinfo_list=new_hostextinfo;

#ifdef DEBUG0
	printf("xedtemplate_duplicate_hostextinfo() end\n");
#endif

	return;
        }



/* duplicates a serviceextinfo object definition */
int xedtemplate_duplicate_serviceextinfo(xedtemplate_serviceextinfo *this_serviceextinfo, char *host_name){
	xedtemplate_serviceextinfo *new_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_duplicate_serviceextinfo() start\n");
#endif

	new_serviceextinfo=(xedtemplate_serviceextinfo *)malloc(sizeof(xedtemplate_serviceextinfo));
	if(new_serviceextinfo==NULL)
		return ERROR;

	new_serviceextinfo->template=NULL;
	new_serviceextinfo->name=NULL;
	new_serviceextinfo->host_name=NULL;
	new_serviceextinfo->hostgroup=NULL;
	new_serviceextinfo->notes_url=NULL;
	new_serviceextinfo->icon_image=NULL;
	new_serviceextinfo->icon_image_alt=NULL;

	new_serviceextinfo->has_been_resolved=this_serviceextinfo->has_been_resolved;
	new_serviceextinfo->register_object=this_serviceextinfo->register_object;

	/* duplicate strings (host_name member is passed in) */
	if(host_name!=NULL){
		new_serviceextinfo->host_name=(char *)malloc(strlen(host_name)+1);
		if(new_serviceextinfo->host_name!=NULL)
			strcpy(new_serviceextinfo->host_name,host_name);
	        }
	if(this_serviceextinfo->template!=NULL){
		new_serviceextinfo->template=(char *)malloc(strlen(this_serviceextinfo->template)+1);
		if(new_serviceextinfo->template!=NULL)
			strcpy(new_serviceextinfo->template,this_serviceextinfo->template);
	        }
	if(this_serviceextinfo->name!=NULL){
		new_serviceextinfo->name=(char *)malloc(strlen(this_serviceextinfo->name)+1);
		if(new_serviceextinfo->name!=NULL)
			strcpy(new_serviceextinfo->name,this_serviceextinfo->name);
	        }
	if(this_serviceextinfo->service_description!=NULL){
		new_serviceextinfo->service_description=(char *)malloc(strlen(this_serviceextinfo->service_description)+1);
		if(new_serviceextinfo->service_description!=NULL)
			strcpy(new_serviceextinfo->service_description,this_serviceextinfo->service_description);
	        }
	if(this_serviceextinfo->hostgroup!=NULL){
		new_serviceextinfo->hostgroup=(char *)malloc(strlen(this_serviceextinfo->hostgroup)+1);
		if(new_serviceextinfo->hostgroup!=NULL)
			strcpy(new_serviceextinfo->hostgroup,this_serviceextinfo->hostgroup);
	        }
	if(this_serviceextinfo->notes_url!=NULL){
		new_serviceextinfo->notes_url=(char *)malloc(strlen(this_serviceextinfo->notes_url)+1);
		if(new_serviceextinfo->notes_url!=NULL)
			strcpy(new_serviceextinfo->notes_url,this_serviceextinfo->notes_url);
	        }
	if(this_serviceextinfo->icon_image!=NULL){
		new_serviceextinfo->icon_image=(char *)malloc(strlen(this_serviceextinfo->icon_image)+1);
		if(new_serviceextinfo->icon_image!=NULL)
			strcpy(new_serviceextinfo->icon_image,this_serviceextinfo->icon_image);
	        }
	if(this_serviceextinfo->icon_image_alt!=NULL){
		new_serviceextinfo->icon_image_alt=(char *)malloc(strlen(this_serviceextinfo->icon_image_alt)+1);
		if(new_serviceextinfo->icon_image_alt!=NULL)
			strcpy(new_serviceextinfo->icon_image_alt,this_serviceextinfo->icon_image_alt);
	        }

	/* add new object to head of list */
	new_serviceextinfo->next=xedtemplate_serviceextinfo_list;
	xedtemplate_serviceextinfo_list=new_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_duplicate_serviceextinfo() end\n");
#endif

	return;
        }



/******************************************************************/
/***************** OBJECT RESOLUTION FUNCTIONS ********************/
/******************************************************************/

/* resolves object definitions */
int xedtemplate_resolve_objects(void){
	xedtemplate_hostextinfo *temp_hostextinfo;
	xedtemplate_serviceextinfo *temp_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_resolve_objects() start\n");
#endif

	/* resolve all hostextinfo objects */
	for(temp_hostextinfo=xedtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){
		if(xedtemplate_resolve_hostextinfo(temp_hostextinfo)==ERROR)
			return ERROR;
	        }

	/* resolve all serviceextinfo objects */
	for(temp_serviceextinfo=xedtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){
		if(xedtemplate_resolve_serviceextinfo(temp_serviceextinfo)==ERROR)
			return ERROR;
	        }

#ifdef DEBUG0
	printf("xedtemplate_resolve_objects() end\n");
#endif

	return OK;
        }



/* resolves a hostextinfo object */
int xedtemplate_resolve_hostextinfo(xedtemplate_hostextinfo *this_hostextinfo){
	xedtemplate_hostextinfo *template_hostextinfo;

#ifdef DEBUG0
	printf("xedtemplate_resolve_hostextinfo() start\n");
#endif

	/* return if this object has already been resolved */
	if(this_hostextinfo->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_hostextinfo->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_hostextinfo->template==NULL)
		return OK;

	template_hostextinfo=xedtemplate_find_hostextinfo(this_hostextinfo->template);
	if(template_hostextinfo==NULL)
		return ERROR;

	/* resolve the template hostextinfo... */
	xedtemplate_resolve_hostextinfo(template_hostextinfo);

	/* apply missing properties from template hostextinfo... */
	if(this_hostextinfo->name==NULL && template_hostextinfo->name!=NULL){
		this_hostextinfo->name=(char *)malloc(strlen(template_hostextinfo->name)+1);
		if(this_hostextinfo->name!=NULL)
			strcpy(this_hostextinfo->name,template_hostextinfo->name);
	        }
	if(this_hostextinfo->host_name==NULL && template_hostextinfo->host_name!=NULL){
		this_hostextinfo->host_name=(char *)malloc(strlen(template_hostextinfo->host_name)+1);
		if(this_hostextinfo->host_name!=NULL)
			strcpy(this_hostextinfo->host_name,template_hostextinfo->host_name);
	        }
	if(this_hostextinfo->notes_url==NULL && template_hostextinfo->notes_url!=NULL){
		this_hostextinfo->notes_url=(char *)malloc(strlen(template_hostextinfo->notes_url)+1);
		if(this_hostextinfo->notes_url!=NULL)
			strcpy(this_hostextinfo->notes_url,template_hostextinfo->notes_url);
	        }
	if(this_hostextinfo->icon_image==NULL && template_hostextinfo->icon_image!=NULL){
		this_hostextinfo->icon_image=(char *)malloc(strlen(template_hostextinfo->icon_image)+1);
		if(this_hostextinfo->icon_image!=NULL)
			strcpy(this_hostextinfo->icon_image,template_hostextinfo->icon_image);
	        }
	if(this_hostextinfo->icon_image_alt==NULL && template_hostextinfo->icon_image_alt!=NULL){
		this_hostextinfo->icon_image_alt=(char *)malloc(strlen(template_hostextinfo->icon_image_alt)+1);
		if(this_hostextinfo->icon_image_alt!=NULL)
			strcpy(this_hostextinfo->icon_image_alt,template_hostextinfo->icon_image_alt);
	        }
	if(this_hostextinfo->vrml_image==NULL && template_hostextinfo->vrml_image!=NULL){
		this_hostextinfo->vrml_image=(char *)malloc(strlen(template_hostextinfo->vrml_image)+1);
		if(this_hostextinfo->vrml_image!=NULL)
			strcpy(this_hostextinfo->vrml_image,template_hostextinfo->vrml_image);
	        }
	if(this_hostextinfo->gd2_image==NULL && template_hostextinfo->gd2_image!=NULL){
		this_hostextinfo->gd2_image=(char *)malloc(strlen(template_hostextinfo->gd2_image)+1);
		if(this_hostextinfo->gd2_image!=NULL)
			strcpy(this_hostextinfo->gd2_image,template_hostextinfo->gd2_image);
	        }
	if(this_hostextinfo->have_2d_coords==FALSE && template_hostextinfo->have_2d_coords==TRUE){
		this_hostextinfo->x_2d=template_hostextinfo->x_2d;
		this_hostextinfo->y_2d=template_hostextinfo->y_2d;
		this_hostextinfo->have_2d_coords=TRUE;
	        }
	if(this_hostextinfo->have_3d_coords==FALSE && template_hostextinfo->have_3d_coords==TRUE){
		this_hostextinfo->x_3d=template_hostextinfo->x_3d;
		this_hostextinfo->y_3d=template_hostextinfo->y_3d;
		this_hostextinfo->z_3d=template_hostextinfo->z_3d;
		this_hostextinfo->have_3d_coords=TRUE;
	        }

#ifdef DEBUG0
	printf("xedtemplate_resolve_hostextinfo() end\n");
#endif

	return OK;
        }



/* resolves a serviceextinfo object */
int xedtemplate_resolve_serviceextinfo(xedtemplate_serviceextinfo *this_serviceextinfo){
	xedtemplate_serviceextinfo *template_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_resolve_serviceextinfo() start\n");
#endif

	/* return if this object has already been resolved */
	if(this_serviceextinfo->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_serviceextinfo->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_serviceextinfo->template==NULL)
		return OK;

	template_serviceextinfo=xedtemplate_find_serviceextinfo(this_serviceextinfo->template);
	if(template_serviceextinfo==NULL)
		return ERROR;

	/* resolve the template serviceextinfo... */
	xedtemplate_resolve_serviceextinfo(template_serviceextinfo);

	/* apply missing properties from template serviceextinfo... */
	if(this_serviceextinfo->name==NULL && template_serviceextinfo->name!=NULL){
		this_serviceextinfo->name=(char *)malloc(strlen(template_serviceextinfo->name)+1);
		if(this_serviceextinfo->name!=NULL)
			strcpy(this_serviceextinfo->name,template_serviceextinfo->name);
	        }
	if(this_serviceextinfo->host_name==NULL && template_serviceextinfo->host_name!=NULL){
		this_serviceextinfo->host_name=(char *)malloc(strlen(template_serviceextinfo->host_name)+1);
		if(this_serviceextinfo->host_name!=NULL)
			strcpy(this_serviceextinfo->host_name,template_serviceextinfo->host_name);
	        }
	if(this_serviceextinfo->service_description==NULL && template_serviceextinfo->service_description!=NULL){
		this_serviceextinfo->service_description=(char *)malloc(strlen(template_serviceextinfo->service_description)+1);
		if(this_serviceextinfo->service_description!=NULL)
			strcpy(this_serviceextinfo->service_description,template_serviceextinfo->service_description);
	        }
	if(this_serviceextinfo->notes_url==NULL && template_serviceextinfo->notes_url!=NULL){
		this_serviceextinfo->notes_url=(char *)malloc(strlen(template_serviceextinfo->notes_url)+1);
		if(this_serviceextinfo->notes_url!=NULL)
			strcpy(this_serviceextinfo->notes_url,template_serviceextinfo->notes_url);
	        }
	if(this_serviceextinfo->icon_image==NULL && template_serviceextinfo->icon_image!=NULL){
		this_serviceextinfo->icon_image=(char *)malloc(strlen(template_serviceextinfo->icon_image)+1);
		if(this_serviceextinfo->icon_image!=NULL)
			strcpy(this_serviceextinfo->icon_image,template_serviceextinfo->icon_image);
	        }
	if(this_serviceextinfo->icon_image_alt==NULL && template_serviceextinfo->icon_image_alt!=NULL){
		this_serviceextinfo->icon_image_alt=(char *)malloc(strlen(template_serviceextinfo->icon_image_alt)+1);
		if(this_serviceextinfo->icon_image_alt!=NULL)
			strcpy(this_serviceextinfo->icon_image_alt,template_serviceextinfo->icon_image_alt);
	        }

#ifdef DEBUG0
	printf("xedtemplate_resolve_serviceextinfo() end\n");
#endif

	return OK;
        }




/******************************************************************/
/******************* OBJECT SEARCH FUNCTIONS **********************/
/******************************************************************/

/* finds a specific hostextinfo object */
xedtemplate_hostextinfo *xedtemplate_find_hostextinfo(char *name){
	xedtemplate_hostextinfo *temp_hostextinfo;

	if(name==NULL)
		return NULL;

	for(temp_hostextinfo=xedtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){
		if(temp_hostextinfo->name==NULL)
			continue;
		if(!strcmp(temp_hostextinfo->name,name))
			break;
	        }

	return temp_hostextinfo;
        }


/* finds a specific serviceextinfo object */
xedtemplate_serviceextinfo *xedtemplate_find_serviceextinfo(char *name){
	xedtemplate_serviceextinfo *temp_serviceextinfo;

	if(name==NULL)
		return NULL;

	for(temp_serviceextinfo=xedtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){
		if(temp_serviceextinfo->name==NULL)
			continue;
		if(!strcmp(temp_serviceextinfo->name,name))
			break;
	        }

	return temp_serviceextinfo;
        }




/******************************************************************/
/**************** OBJECT REGISTRATION FUNCTIONS *******************/
/******************************************************************/

/* registers object definitions */
int xedtemplate_register_objects(void){
	xedtemplate_hostextinfo *temp_hostextinfo;
	xedtemplate_serviceextinfo *temp_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_register_objects() start\n");
#endif

	/* register hostextinfo definitions */
	for(temp_hostextinfo=xedtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){
		if(xedtemplate_register_hostextinfo(temp_hostextinfo)==ERROR)
			return ERROR;
	        }

	/* register serviceextinfo definitions */
	for(temp_serviceextinfo=xedtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){
		if(xedtemplate_register_serviceextinfo(temp_serviceextinfo)==ERROR)
			return ERROR;
	        }

#ifdef DEBUG0
	printf("xedtemplate_register_objects() end\n");
#endif

	return OK;
        }



/* registers a hostextinfo definition */
int xedtemplate_register_hostextinfo(xedtemplate_hostextinfo *this_hostextinfo){

#ifdef DEBUG0
	printf("xedtemplate_register_hostextinfo() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_hostextinfo->register_object==FALSE)
		return OK;

	/* register the extended host object */
	add_extended_host_info(this_hostextinfo->host_name,this_hostextinfo->notes_url,this_hostextinfo->icon_image,this_hostextinfo->vrml_image,this_hostextinfo->gd2_image,this_hostextinfo->icon_image_alt,this_hostextinfo->x_2d,this_hostextinfo->y_2d,this_hostextinfo->x_3d,this_hostextinfo->y_3d,this_hostextinfo->z_3d,this_hostextinfo->have_2d_coords,this_hostextinfo->have_3d_coords);

#ifdef DEBUG0
	printf("xedtemplate_register_hostextinfo() end\n");
#endif

	return OK;
        }



/* registers a serviceextinfo definition */
int xedtemplate_register_serviceextinfo(xedtemplate_serviceextinfo *this_serviceextinfo){

#ifdef DEBUG0
	printf("xedtemplate_register_serviceextinfo() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_serviceextinfo->register_object==FALSE)
		return OK;

	/* register the extended service object */
	add_extended_service_info(this_serviceextinfo->host_name,this_serviceextinfo->service_description,this_serviceextinfo->notes_url,this_serviceextinfo->icon_image,this_serviceextinfo->icon_image_alt);

#ifdef DEBUG0
	printf("xedtemplate_register_serviceextinfo() end\n");
#endif

	return OK;
        }






/******************************************************************/
/********************** CLEANUP FUNCTIONS *************************/
/******************************************************************/

/* frees memory */
int xedtemplate_free_memory(void){
	xedtemplate_hostextinfo *this_hostextinfo;
	xedtemplate_hostextinfo *next_hostextinfo;
	xedtemplate_serviceextinfo *this_serviceextinfo;
	xedtemplate_serviceextinfo *next_serviceextinfo;

#ifdef DEBUG0
	printf("xedtemplate_free_memory() start\n");
#endif

	/* free memory allocated to hostextinfo list */
	for(this_hostextinfo=xedtemplate_hostextinfo_list;this_hostextinfo!=NULL;this_hostextinfo=next_hostextinfo){
		next_hostextinfo=this_hostextinfo->next;
		free(this_hostextinfo->template);
		free(this_hostextinfo->name);
		free(this_hostextinfo->host_name);
		free(this_hostextinfo->hostgroup);
		free(this_hostextinfo->notes_url);
		free(this_hostextinfo->icon_image);
		free(this_hostextinfo->icon_image_alt);
		free(this_hostextinfo->vrml_image);
		free(this_hostextinfo->gd2_image);
		free(this_hostextinfo);
	        }

	/* free memory allocated to serviceextinfo list */
	for(this_serviceextinfo=xedtemplate_serviceextinfo_list;this_serviceextinfo!=NULL;this_serviceextinfo=next_serviceextinfo){
		next_serviceextinfo=this_serviceextinfo->next;
		free(this_serviceextinfo->template);
		free(this_serviceextinfo->name);
		free(this_serviceextinfo->host_name);
		free(this_serviceextinfo->hostgroup);
		free(this_serviceextinfo->service_description);
		free(this_serviceextinfo->notes_url);
		free(this_serviceextinfo->icon_image);
		free(this_serviceextinfo->icon_image_alt);
		free(this_serviceextinfo);
	        }

#ifdef DEBUG0
	printf("xedtemplate_free_memory() end\n");
#endif

	return OK;
        }
