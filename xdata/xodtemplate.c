 /*****************************************************************************
 *
 * XODTEMPLATE.C - Template-based object configuration data input routines
 *
 * Copyright (c) 2001-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 02-14-2003
 *
 * Description:
 *
 * Routines for parsing and resolving template-based object definitions.
 * Basic steps involved in this are as follows:
 *
 *    1) Read
 *    2) Resolve
 *    3) Recombobulate
 *    4) Replicate
 *    5) Register
 *    6) Release
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
#include "../common/locations.h"

/**** CORE OR CGI SPECIFIC HEADER FILES ****/

#ifdef NSCORE
#include "../base/nagios.h"
#endif

#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif


/**** DATA INPUT-SPECIFIC HEADER FILES ****/

#include "xodtemplate.h"



xodtemplate_timeperiod *xodtemplate_timeperiod_list=NULL;
xodtemplate_command *xodtemplate_command_list=NULL;
xodtemplate_contactgroup *xodtemplate_contactgroup_list=NULL;
xodtemplate_hostgroup *xodtemplate_hostgroup_list=NULL;
xodtemplate_servicedependency *xodtemplate_servicedependency_list=NULL;
xodtemplate_serviceescalation *xodtemplate_serviceescalation_list=NULL;
xodtemplate_contact *xodtemplate_contact_list=NULL;
xodtemplate_host *xodtemplate_host_list=NULL;
xodtemplate_service **xodtemplate_service_list=NULL;
xodtemplate_hostdependency *xodtemplate_hostdependency_list=NULL;
xodtemplate_hostescalation *xodtemplate_hostescalation_list=NULL;
xodtemplate_hostextinfo *xodtemplate_hostextinfo_list=NULL;
xodtemplate_serviceextinfo *xodtemplate_serviceextinfo_list=NULL;

void *xodtemplate_current_object=NULL;
int xodtemplate_current_object_type=XODTEMPLATE_NONE;

int xodtemplate_current_config_file=0;
char **xodtemplate_config_files;

char xodtemplate_cache_file[MAX_FILENAME_LENGTH];

#ifdef NSCORE
FILE *xodtemplate_cache_fp=NULL;
#endif




/******************************************************************/
/************* TOP-LEVEL CONFIG DATA INPUT FUNCTION ***************/
/******************************************************************/

/* process all config files - both core and CGIs pass in name of main config file */
int xodtemplate_read_config_data(char *main_config_file,int options,int cache){
	char config_file[MAX_FILENAME_LENGTH];
	char input[MAX_XODTEMPLATE_INPUT_BUFFER];
	char *temp_ptr;
	FILE *fp;
	int result=OK;

#ifdef DEBUG0
	printf("xodtemplate_read_config_data() start\n");
#endif

	/* get variables from main config file */
	xodtemplate_grab_config_info(main_config_file);
	
	/* open the main config file for reading (we need to find all the config files to read) */
	fp=fopen(main_config_file,"r");
	if(fp==NULL){
#ifdef DEBUG1
		printf("Error: Cannot open main configuration file '%s' for reading!\n",main_config_file);
#endif
		return ERROR;
	        }

	/* reinitialize variables */
	xodtemplate_timeperiod_list=NULL;
	xodtemplate_command_list=NULL;
	xodtemplate_contactgroup_list=NULL;
	xodtemplate_hostgroup_list=NULL;
	xodtemplate_servicedependency_list=NULL;
	xodtemplate_serviceescalation_list=NULL;
	xodtemplate_contact_list=NULL;
	xodtemplate_host_list=NULL;
	xodtemplate_service_list=NULL;
	xodtemplate_hostdependency_list=NULL;
	xodtemplate_hostescalation_list=NULL;
	xodtemplate_hostextinfo_list=NULL;
	xodtemplate_serviceextinfo_list=NULL;

	xodtemplate_current_object=NULL;
	xodtemplate_current_object_type=XODTEMPLATE_NONE;

	/* allocate memory for 256 config files (increased dynamically) */
	xodtemplate_current_config_file=0;
	xodtemplate_config_files=(char **)malloc(256*sizeof(char **));
	if(fp==NULL){
#ifdef DEBUG1
		printf("Error: Cannot allocate memory for config file names!\n");
#endif
		return ERROR;
	        }

#ifdef NSCORE

	/**** NOTE TO SELF: THIS IS A HORRIBLE HACKISH WAY TO CACHE THE DATA - FIX IT IN 3.0 WHEN WEB INTERFACE IS REWRITTEN! ****/

	/* open the cache file for writing */
	if(cache==TRUE)
		xodtemplate_cache_fp=fopen(xodtemplate_cache_file,"w");
	if(xodtemplate_cache_fp!=NULL)
		fputs("###########################\n# NAGIOS OBJECT CACHE FILE\n# DO NOT MODIFY THIS FILE!!\n###########################\n\n",xodtemplate_cache_fp);

	/* daemon reads all config files/dirs specified in the main config file */
	/* read in all lines from the main config file */
	for(fgets(input,sizeof(input)-1,fp);!feof(fp);fgets(input,sizeof(input)-1,fp)){

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]==';' || input[0]=='\x0' || input[0]=='\n' || input[0]=='\r')
			continue;

		/* strip input */
		strip(input);

		temp_ptr=strtok(input,"=");
		if(temp_ptr==NULL)
			continue;

		/* process a single config file */
		if(strstr(temp_ptr,"xodtemplate_config_file")==temp_ptr || strstr(temp_ptr,"cfg_file")==temp_ptr){

			/* get the config file name */
			temp_ptr=strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;

			strncpy(config_file,temp_ptr,sizeof(config_file)-1);
			config_file[sizeof(config_file)-1]='\x0';

			/* process the config file... */
			result=xodtemplate_process_config_file(config_file,options);

			/* if there was an error processing the config file, break out of loop */
			if(result==ERROR)
				break;
		        }

		/* process all files in a config directory */
		else if(strstr(temp_ptr,"xodtemplate_config_dir")==temp_ptr || strstr(temp_ptr,"cfg_dir")==temp_ptr){

			/* get the config directory name */
			temp_ptr=strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;

			strncpy(config_file,temp_ptr,sizeof(config_file)-1);
			config_file[sizeof(config_file)-1]='\x0';

			/* strip trailing / if necessary */
			if(config_file[strlen(config_file)-1]=='/')
				config_file[strlen(config_file)-1]='\x0';

			/* process the config directory... */
			result=xodtemplate_process_config_dir(config_file,options);

			/* if there was an error processing the config file, break out of loop */
			if(result==ERROR)
				break;
		        }
	        }

	fclose(fp);

	/* close the cache file */
	if(xodtemplate_cache_fp!=NULL)
		fclose(xodtemplate_cache_fp);
#endif

#ifdef NSCGI
	/* CGIs process only one file - the cached objects file */
	result=xodtemplate_process_config_file(xodtemplate_cache_file,options);
#endif

	/* resolve objects definitions */
	if(result==OK)
		result=xodtemplate_resolve_objects();

	/* recombobulate object definitions */
	if(result==OK)
		result=xodtemplate_recombobulate_objects();

	/* duplicate object definitions */
	if(result==OK)
		result=xodtemplate_duplicate_objects();

	/* register objects */
	if(result==OK)
		result=xodtemplate_register_objects();

	/* cleanup */
	xodtemplate_free_memory();

#ifdef DEBUG0
	printf("xodtemplate_read_config_data() end\n");
#endif

	return result;
	}



/* grab config variable from main config file */
int xodtemplate_grab_config_info(char *main_config_file){
	char input[MAX_XODTEMPLATE_INPUT_BUFFER];
	char *temp_ptr;
	FILE *fp;
	
#ifdef DEBUGO
	printf("xodtemplate_grab_config_info() start\n");
#endif

	/* default location of cached object file */
	snprintf(xodtemplate_cache_file,sizeof(xodtemplate_cache_file)-1,"%s",DEFAULT_OBJECT_CACHE_FILE);
	xodtemplate_cache_file[sizeof(xodtemplate_cache_file)-1]='\x0';

	/* open the main config file for reading */
	fp=fopen(main_config_file,"r");
	if(fp==NULL)
		return ERROR;

	/* read in all lines from the main config file */
	for(fgets(input,sizeof(input)-1,fp);!feof(fp);fgets(input,sizeof(input)-1,fp)){

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]==';' || input[0]=='\x0' || input[0]=='\n' || input[0]=='\r')
			continue;

		/* strip input */
		strip(input);

		temp_ptr=strtok(input,"=");
		if(temp_ptr==NULL)
			continue;

		/* cached object file definition (overrides default location) */
		if(strstr(temp_ptr,"object_cache_file")==temp_ptr){

			/* get the config file name */
			temp_ptr=strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;

			strncpy(xodtemplate_cache_file,temp_ptr,sizeof(xodtemplate_cache_file)-1);
			xodtemplate_cache_file[sizeof(xodtemplate_cache_file)-1]='\x0';
		        }
	        }

	fclose(fp);


#ifdef DEBUGO
	printf("xodtemplate_grab_config_info() end\n");
#endif
	return OK;
        }



/* process all files in a specific config directory */
int xodtemplate_process_config_dir(char *dirname, int options){
	char config_file[MAX_FILENAME_LENGTH];
	DIR *dirp;
	struct dirent *dirfile;
	int result=OK;
	int x;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_process_config_dir() start\n");
#endif

	/* open the directory for reading */
	dirp=opendir(dirname);
        if(dirp==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not open config directory '%s' for reading.\n",dirname);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		result=ERROR;
	        }

	/* process all files in the directory... */
	while((dirfile=readdir(dirp))!=NULL){

		/* process this if it's a config file... */
		x=strlen(dirfile->d_name);
		if(x>4 && !strcmp(dirfile->d_name+(x-4),".cfg")){

#ifdef _DIRENT_HAVE_D_TYPE
			/* only process normal files */
			if(dirfile->d_type!=DT_REG)
				continue;
#endif

			/* create the full path to the config file */
			snprintf(config_file,sizeof(config_file)-1,"%s/%s",dirname,dirfile->d_name);
			config_file[sizeof(config_file)-1]='\x0';

			/* process the config file */
			result=xodtemplate_process_config_file(config_file,options);

			/* break out if we encountered an error */
			if(result==ERROR)
				break;
		        }

#ifdef _DIRENT_HAVE_D_TYPE
		/* recurse into subdirectories... */
		if(dirfile->d_type==DT_DIR){

			/* ignore current, parent and hidden directory entries */
			if(dirfile->d_name[0]=='.')
				continue;

			/* create the full path to the config directory */
			snprintf(config_file,sizeof(config_file)-1,"%s/%s",dirname,dirfile->d_name);
			config_file[sizeof(config_file)-1]='\x0';

			/* process the config directory */
			result=xodtemplate_process_config_dir(config_file,options);

			/* break out if we encountered an error */
			if(result==ERROR)
				break;
		        }
#endif
		}

	closedir(dirp);

#ifdef DEBUG0
	printf("xodtemplate_process_config_dir() end\n");
#endif

	return result;
        }


/* process data in a specific config file */
int xodtemplate_process_config_file(char *filename, int options){
	FILE *fp;
	char input[MAX_XODTEMPLATE_INPUT_BUFFER];
	register in_definition=FALSE;
	register current_line=0;
	int result=OK;
	register int x;
	register int y;
#ifdef NSCORE
	char temp_buffer[MAX_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_process_config_file() start\n");
#endif


	/* save config file name */
	xodtemplate_config_files[xodtemplate_current_config_file++]=strdup(filename);

	/* reallocate memory for config files */
	if(!(xodtemplate_current_config_file%256)){
		xodtemplate_config_files=(char **)realloc(xodtemplate_config_files,(xodtemplate_current_config_file+256)*sizeof(char **));
		if(xodtemplate_config_files==NULL){
#ifdef DEBUG1
			printf("Error: Cannot re-allocate memory for config file names!\n");
#endif
			return ERROR;
		        }
	        }

	/* open the config file for reading */
	fp=fopen(filename,"r");
	if(fp==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Cannot open config file '%s' for reading!\n",filename);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* read in all lines from the config file */
	while(fgets(input,sizeof(input)-1,fp)){

		current_line++;

		/* skip empty lines */
		if(input[0]=='#' || input[0]==';' || input[0]=='\r' || input[0]=='\n')
			continue;

		/* grab data before comment delimiter - faster than a strtok() and strncpy()... */
		for(x=0;input[x]!='\x0';x++)
			if(input[x]==';')
				break;
		input[x]='\x0';

		/* strip input */
		strip(input);

		/* skip blank lines */
		if(input[0]=='\x0')
			continue;

#ifdef NSCORE
		/* write this line to the cache file */
		if(xodtemplate_cache_fp!=NULL)
			fprintf(xodtemplate_cache_fp,"%s\n",input);
#endif

		/* this is the start of an object definition */
		if(strstr(input,"define")==input){

			/* get the type of object we're defining... */
			for(x=6;input[x]!='\x0';x++)
				if(input[x]!=' ' && input[x]!='\t')
					break;
			for(y=0;input[x]!='\x0';x++){
				if(input[x]==' ' || input[x]=='\t' ||  input[x]=='{')
					break;
				else
					input[y++]=input[x];
			        }
			input[y]='\x0';

			/* make sure an object type is specified... */
			if(input[0]=='\x0'){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: No object type specified in file '%s' on line %d.\n",filename,current_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				result=ERROR;
				break;
			        }

			/* check validity of object type */
			if(strcmp(input,"timeperiod") && strcmp(input,"command") && strcmp(input,"contact") && strcmp(input,"contactgroup") && strcmp(input,"host") && strcmp(input,"hostgroup") && strcmp(input,"service") && strcmp(input,"servicedependency") && strcmp(input,"serviceescalation") && strcmp(input,"hostgroupescalation") && strcmp(input,"hostdependency") && strcmp(input,"hostescalation") && strcmp(input,"hostextinfo") && strcmp(input,"serviceextinfo")){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid object definition type '%s' in file '%s' on line %d.\n",input,filename,current_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				result=ERROR;
				break;
			        }

			/* we're already in an object definition... */
			if(in_definition==TRUE){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Unexpected start of object definition in file '%s' on line %d.  Make sure you close preceding objects before starting a new one.\n",filename,current_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif				
				result=ERROR;
				break;
			        }

			/* start a new definition */
			if(xodtemplate_begin_object_definition(input,options,xodtemplate_current_config_file,current_line)==ERROR){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add object definition in file '%s' on line %d.\n",filename,current_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				result=ERROR;
				break;
			        }

			in_definition=TRUE;
		        }

		/* this is the close of an object definition */
		else if(!strcmp(input,"}") && in_definition==TRUE){

			in_definition=FALSE;

			/* close out current definition */
			if(xodtemplate_end_object_definition(options)==ERROR){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not complete object definition in file '%s' on line %d.\n",filename,current_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				result=ERROR;
				break;
			        }
		        }

		/* this is a directive inside an object definition */
		else if(in_definition==TRUE){
			
			/* add directive to object definition */
			if(xodtemplate_add_object_property(input,options)==ERROR){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add object property in file '%s' on line %d.\n",filename,current_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				result=ERROR;
				break;
			        }
		        }

		/* unexpected token or statement */
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Unexpected token or statement in file '%s' on line %d.\n",filename,current_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			result=ERROR;
			break;
		        }
	        }

	/* close file */
	fclose(fp);

	/* whoops - EOF while we were in the middle of an object definition... */
	if(in_definition==TRUE && result==OK){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Unexpected EOF in file '%s' on line %d - check for a missing closing bracket.\n",filename,current_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		result=ERROR;
	        }

#ifdef DEBUG0
	printf("xodtemplate_process_config_file() end\n");
#endif

	return result;
        }





/******************************************************************/
/***************** OBJECT DEFINITION FUNCTIONS ********************/
/******************************************************************/

/* starts a new object definition */
int xodtemplate_begin_object_definition(char *input, int options, int config_file, int start_line){
	int result=OK;
	xodtemplate_timeperiod *new_timeperiod;
	xodtemplate_command *new_command;
	xodtemplate_contactgroup *new_contactgroup;
	xodtemplate_hostgroup *new_hostgroup;
	xodtemplate_servicedependency *new_servicedependency;
	xodtemplate_serviceescalation *new_serviceescalation;
	xodtemplate_contact *new_contact;
	xodtemplate_host *new_host;
	xodtemplate_service *new_service;
	xodtemplate_hostdependency *new_hostdependency;
	xodtemplate_hostescalation *new_hostescalation;
	xodtemplate_hostextinfo *new_hostextinfo;
	xodtemplate_serviceextinfo *new_serviceextinfo;
	int x;

#ifdef DEBUG0
	printf("xodtemplate_begin_object_definition() start\n");
#endif

	if(!strcmp(input,"service"))
		xodtemplate_current_object_type=XODTEMPLATE_SERVICE;
	else if(!strcmp(input,"host"))
		xodtemplate_current_object_type=XODTEMPLATE_HOST;
	else if(!strcmp(input,"command"))
		xodtemplate_current_object_type=XODTEMPLATE_COMMAND;
	else if(!strcmp(input,"contact"))
		xodtemplate_current_object_type=XODTEMPLATE_CONTACT;
	else if(!strcmp(input,"contactgroup"))
		xodtemplate_current_object_type=XODTEMPLATE_CONTACTGROUP;
	else if(!strcmp(input,"hostgroup"))
		xodtemplate_current_object_type=XODTEMPLATE_HOSTGROUP;
	else if(!strcmp(input,"timeperiod"))
		xodtemplate_current_object_type=XODTEMPLATE_TIMEPERIOD;
	else if(!strcmp(input,"servicedependency"))
		xodtemplate_current_object_type=XODTEMPLATE_SERVICEDEPENDENCY;
	else if(!strcmp(input,"serviceescalation"))
		xodtemplate_current_object_type=XODTEMPLATE_SERVICEESCALATION;
	else if(!strcmp(input,"hostdependency"))
		xodtemplate_current_object_type=XODTEMPLATE_HOSTDEPENDENCY;
	else if(!strcmp(input,"hostescalation"))
		xodtemplate_current_object_type=XODTEMPLATE_HOSTESCALATION;
	else if(!strcmp(input,"hostextinfo"))
		xodtemplate_current_object_type=XODTEMPLATE_HOSTEXTINFO;
	else if(!strcmp(input,"serviceextinfo"))
		xodtemplate_current_object_type=XODTEMPLATE_SERVICEEXTINFO;
	else
		return ERROR;


	/* check to see if we should process this type of object */
	switch(xodtemplate_current_object_type){
	case XODTEMPLATE_TIMEPERIOD:
		if(!(options & READ_TIMEPERIODS))
			return OK;
		break;
	case XODTEMPLATE_COMMAND:
		if(!(options & READ_COMMANDS))
			return OK;
		break;
	case XODTEMPLATE_CONTACT:
		if(!(options & READ_CONTACTS))
			return OK;
		break;
	case XODTEMPLATE_CONTACTGROUP:
		if(!(options & READ_CONTACTGROUPS))
			return OK;
		break;
	case XODTEMPLATE_HOST:
		if(!(options & READ_HOSTS))
			return OK;
		break;
	case XODTEMPLATE_HOSTGROUP:
		if(!(options & READ_HOSTGROUPS))
			return OK;
		break;
	case XODTEMPLATE_SERVICE:
		if(!(options & READ_SERVICES))
			return OK;
		break;
	case XODTEMPLATE_SERVICEDEPENDENCY:
		if(!(options & READ_SERVICEDEPENDENCIES))
			return OK;
		break;
	case XODTEMPLATE_SERVICEESCALATION:
		if(!(options & READ_SERVICEESCALATIONS))
			return OK;
		break;
	case XODTEMPLATE_HOSTDEPENDENCY:
		if(!(options & READ_HOSTDEPENDENCIES))
			return OK;
		break;
	case XODTEMPLATE_HOSTESCALATION:
		if(!(options & READ_HOSTESCALATIONS))
			return OK;
		break;
	case XODTEMPLATE_HOSTEXTINFO:
		if(!(options & READ_HOSTEXTINFO))
			return OK;
		break;
	case XODTEMPLATE_SERVICEEXTINFO:
		if(!(options & READ_SERVICEEXTINFO))
			return OK;
		break;
	default:
		return ERROR;
		break;
	        }



	/* add a new (blank) object */
	switch(xodtemplate_current_object_type){

	case XODTEMPLATE_TIMEPERIOD:

		/* allocate memory */
		new_timeperiod=(xodtemplate_timeperiod *)malloc(sizeof(xodtemplate_timeperiod));
		if(new_timeperiod==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for timeperiod definition\n");
#endif
			return ERROR;
		        }

		new_timeperiod->template=NULL;
		new_timeperiod->name=NULL;
		new_timeperiod->timeperiod_name=NULL;
		new_timeperiod->alias=NULL;
		for(x=0;x<7;x++)
			new_timeperiod->timeranges[x]=NULL;
		new_timeperiod->has_been_resolved=FALSE;
		new_timeperiod->register_object=TRUE;
		new_timeperiod->_config_file=config_file;
		new_timeperiod->_start_line=start_line;

		/* add new timeperiod to head of list in memory */
		new_timeperiod->next=xodtemplate_timeperiod_list;
		xodtemplate_timeperiod_list=new_timeperiod;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_timeperiod_list;

		break;

	case XODTEMPLATE_COMMAND:

		/* allocate memory */
		new_command=(xodtemplate_command *)malloc(sizeof(xodtemplate_command));
		if(new_command==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for command definition\n");
#endif
			return ERROR;
		        }
		
		new_command->template=NULL;
		new_command->name=NULL;
		new_command->command_name=NULL;
		new_command->command_line=NULL;
		new_command->has_been_resolved=FALSE;
		new_command->register_object=TRUE;
		new_command->_config_file=config_file;
		new_command->_start_line=start_line;

		/* add new command to head of list in memory */
		new_command->next=xodtemplate_command_list;
		xodtemplate_command_list=new_command;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_command_list;

		break;

	case XODTEMPLATE_CONTACTGROUP:

		/* allocate memory */
		new_contactgroup=(xodtemplate_contactgroup *)malloc(sizeof(xodtemplate_contactgroup));
		if(new_contactgroup==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for contactgroup definition\n");
#endif
			return ERROR;
		        }
		
		new_contactgroup->template=NULL;
		new_contactgroup->name=NULL;
		new_contactgroup->contactgroup_name=NULL;
		new_contactgroup->alias=NULL;
		new_contactgroup->members=NULL;
		new_contactgroup->has_been_resolved=FALSE;
		new_contactgroup->register_object=TRUE;
		new_contactgroup->_config_file=config_file;
		new_contactgroup->_start_line=start_line;

		/* add new contactgroup to head of list in memory */
		new_contactgroup->next=xodtemplate_contactgroup_list;
		xodtemplate_contactgroup_list=new_contactgroup;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_contactgroup_list;
		break;


	case XODTEMPLATE_HOSTGROUP:

		/* allocate memory */
		new_hostgroup=(xodtemplate_hostgroup *)malloc(sizeof(xodtemplate_hostgroup));
		if(new_hostgroup==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for hostgroup definition\n");
#endif
			return ERROR;
		        }
		
		new_hostgroup->template=NULL;
		new_hostgroup->name=NULL;
		new_hostgroup->hostgroup_name=NULL;
		new_hostgroup->alias=NULL;
		new_hostgroup->members=NULL;
		new_hostgroup->has_been_resolved=FALSE;
		new_hostgroup->register_object=TRUE;
		new_hostgroup->_config_file=config_file;
		new_hostgroup->_start_line=start_line;

		/* add new hostgroup to head of list in memory */
		new_hostgroup->next=xodtemplate_hostgroup_list;
		xodtemplate_hostgroup_list=new_hostgroup;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_hostgroup_list;
		break;

	case XODTEMPLATE_SERVICEDEPENDENCY:

		/* allocate memory */
		new_servicedependency=(xodtemplate_servicedependency *)malloc(sizeof(xodtemplate_servicedependency));
		if(new_servicedependency==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for servicedependency definition\n");
#endif
			return ERROR;
		        }
		
		new_servicedependency->template=NULL;
		new_servicedependency->name=NULL;
		new_servicedependency->hostgroup_name=NULL;
		new_servicedependency->host_name=NULL;
		new_servicedependency->service_description=NULL;
		new_servicedependency->dependent_hostgroup_name=NULL;
		new_servicedependency->dependent_host_name=NULL;
		new_servicedependency->dependent_service_description=NULL;
		new_servicedependency->fail_execute_on_ok=FALSE;
		new_servicedependency->fail_execute_on_unknown=FALSE;
		new_servicedependency->fail_execute_on_warning=FALSE;
		new_servicedependency->fail_execute_on_critical=FALSE;
		new_servicedependency->fail_notify_on_ok=FALSE;
		new_servicedependency->fail_notify_on_unknown=FALSE;
		new_servicedependency->fail_notify_on_warning=FALSE;
		new_servicedependency->fail_notify_on_critical=FALSE;
		new_servicedependency->have_execution_dependency_options=FALSE;
		new_servicedependency->have_notification_dependency_options=FALSE;
		new_servicedependency->has_been_resolved=FALSE;
		new_servicedependency->register_object=TRUE;
		new_servicedependency->_config_file=config_file;
		new_servicedependency->_start_line=start_line;

		/* add new servicedependency to head of list in memory */
		new_servicedependency->next=xodtemplate_servicedependency_list;
		xodtemplate_servicedependency_list=new_servicedependency;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_servicedependency_list;
		break;

	case XODTEMPLATE_SERVICEESCALATION:

		/* allocate memory */
		new_serviceescalation=(xodtemplate_serviceescalation *)malloc(sizeof(xodtemplate_serviceescalation));
		if(new_serviceescalation==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for serviceescalation definition\n");
#endif
			return ERROR;
		        }
		
		new_serviceescalation->template=NULL;
		new_serviceescalation->name=NULL;
		new_serviceescalation->hostgroup_name=NULL;
		new_serviceescalation->host_name=NULL;
		new_serviceescalation->service_description=NULL;
		new_serviceescalation->contact_groups=NULL;
		new_serviceescalation->first_notification=-2;
		new_serviceescalation->last_notification=-2;
		new_serviceescalation->notification_interval=-2;
		new_serviceescalation->have_first_notification=FALSE;
		new_serviceescalation->have_last_notification=FALSE;
		new_serviceescalation->have_notification_interval=FALSE;
		new_serviceescalation->has_been_resolved=FALSE;
		new_serviceescalation->register_object=TRUE;
		new_serviceescalation->_config_file=config_file;
		new_serviceescalation->_start_line=start_line;

		/* add new serviceescalation to head of list in memory */
		new_serviceescalation->next=xodtemplate_serviceescalation_list;
		xodtemplate_serviceescalation_list=new_serviceescalation;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_serviceescalation_list;
		break;

	case XODTEMPLATE_CONTACT:

		/* allocate memory */
		new_contact=(xodtemplate_contact *)malloc(sizeof(xodtemplate_contact));
		if(new_contact==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for contact definition\n");
#endif
			return ERROR;
		        }
		
		new_contact->template=NULL;
		new_contact->name=NULL;
		new_contact->contact_name=NULL;
		new_contact->alias=NULL;
		new_contact->email=NULL;
		new_contact->pager=NULL;
		new_contact->host_notification_period=NULL;
		new_contact->host_notification_commands=NULL;
		new_contact->service_notification_period=NULL;
		new_contact->service_notification_commands=NULL;
		new_contact->notify_on_host_down=FALSE;
		new_contact->notify_on_host_unreachable=FALSE;
		new_contact->notify_on_host_recovery=FALSE;
		new_contact->notify_on_service_unknown=FALSE;
		new_contact->notify_on_service_warning=FALSE;
		new_contact->notify_on_service_critical=FALSE;
		new_contact->notify_on_service_recovery=FALSE;
		new_contact->have_host_notification_options=FALSE;
		new_contact->have_service_notification_options=FALSE;
		new_contact->has_been_resolved=FALSE;
		new_contact->register_object=TRUE;
		new_contact->_config_file=config_file;
		new_contact->_start_line=start_line;

		/* add new contact to head of list in memory */
		new_contact->next=xodtemplate_contact_list;
		xodtemplate_contact_list=new_contact;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_contact_list;
		break;

	case XODTEMPLATE_HOST:

		/* allocate memory */
		new_host=(xodtemplate_host *)malloc(sizeof(xodtemplate_host));
		if(new_host==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for host definition\n");
#endif
			return ERROR;
		        }
		
		new_host->template=NULL;
		new_host->name=NULL;
		new_host->host_name=NULL;
		new_host->alias=NULL;
		new_host->address=NULL;
		new_host->parents=NULL;
		new_host->hostgroups=NULL;
		new_host->check_command=NULL;
		new_host->event_handler=NULL;
		new_host->contact_groups=NULL;
		new_host->notification_period=NULL;

		new_host->active_checks_enabled=TRUE;
		new_host->have_active_checks_enabled=FALSE;
		new_host->passive_checks_enabled=TRUE;
		new_host->have_passive_checks_enabled=FALSE;
		new_host->obsess_over_host=TRUE;
		new_host->have_obsess_over_host=FALSE;
		new_host->max_check_attempts=-2;
		new_host->have_max_check_attempts=FALSE;
		new_host->event_handler_enabled=TRUE;
		new_host->have_event_handler_enabled=FALSE;
		new_host->flap_detection_enabled=TRUE;
		new_host->have_flap_detection_enabled=FALSE;
		new_host->low_flap_threshold=0.0;
		new_host->have_low_flap_threshold=FALSE;
		new_host->high_flap_threshold=0.0;
		new_host->have_high_flap_threshold=FALSE;
		new_host->notify_on_down=FALSE;
		new_host->notify_on_unreachable=FALSE;
		new_host->notify_on_recovery=FALSE;
		new_host->have_notification_options=FALSE;
		new_host->notifications_enabled=TRUE;
		new_host->have_notifications_enabled=FALSE;
		new_host->notification_interval=-2;
		new_host->have_notification_interval=FALSE;
		new_host->stalk_on_up=FALSE;
		new_host->stalk_on_down=FALSE;
		new_host->stalk_on_unreachable=FALSE;
		new_host->have_stalking_options=FALSE;
		new_host->process_perf_data=TRUE;
		new_host->have_process_perf_data=FALSE;
		new_host->failure_prediction_enabled=TRUE;
		new_host->have_failure_prediction_enabled=FALSE;
		new_host->failure_prediction_options=NULL;
		new_host->retain_status_information=TRUE;
		new_host->have_retain_status_information=FALSE;
		new_host->retain_nonstatus_information=TRUE;
		new_host->have_retain_nonstatus_information=FALSE;
		new_host->has_been_resolved=FALSE;
		new_host->register_object=TRUE;
		new_host->_config_file=config_file;
		new_host->_start_line=start_line;

		/* add new host to head of list in memory */
		new_host->next=xodtemplate_host_list;
		xodtemplate_host_list=new_host;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_host_list;
		break;

	case XODTEMPLATE_SERVICE:

		/* allocate memory */
		new_service=(xodtemplate_service *)malloc(sizeof(xodtemplate_service));
		if(new_service==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for service definition\n");
#endif
			return ERROR;
		        }
		
		new_service->template=NULL;
		new_service->name=NULL;
		new_service->hostgroup_name=NULL;
		new_service->host_name=NULL;
		new_service->service_description=NULL;
		new_service->check_command=NULL;
		new_service->check_period=NULL;
		new_service->event_handler=NULL;
		new_service->notification_period=NULL;
		new_service->contact_groups=NULL;

		new_service->max_check_attempts=-2;
		new_service->have_max_check_attempts=FALSE;
		new_service->normal_check_interval=-2;
		new_service->have_normal_check_interval=FALSE;
		new_service->retry_check_interval=-2;
		new_service->have_retry_check_interval=FALSE;
		new_service->active_checks_enabled=TRUE;
		new_service->have_active_checks_enabled=FALSE;
		new_service->passive_checks_enabled=TRUE;
		new_service->have_passive_checks_enabled=FALSE;
		new_service->parallelize_check=TRUE;
		new_service->have_parallelize_check=FALSE;
		new_service->is_volatile=FALSE;
		new_service->have_is_volatile=FALSE;
		new_service->obsess_over_service=TRUE;
		new_service->have_obsess_over_service=FALSE;
		new_service->event_handler_enabled=TRUE;
		new_service->have_event_handler_enabled=FALSE;
		new_service->check_freshness=FALSE;
		new_service->have_check_freshness=FALSE;
		new_service->freshness_threshold=0;
		new_service->have_freshness_threshold=FALSE;
		new_service->flap_detection_enabled=TRUE;
		new_service->have_flap_detection_enabled=FALSE;
		new_service->low_flap_threshold=0.0;
		new_service->have_low_flap_threshold=FALSE;
		new_service->high_flap_threshold=0.0;
		new_service->have_high_flap_threshold=FALSE;
		new_service->notify_on_unknown=FALSE;
		new_service->notify_on_warning=FALSE;
		new_service->notify_on_critical=FALSE;
		new_service->notify_on_recovery=FALSE;
		new_service->have_notification_options=FALSE;
		new_service->notifications_enabled=TRUE;
		new_service->have_notifications_enabled=FALSE;
		new_service->notification_interval=-2;
		new_service->have_notification_interval=FALSE;
		new_service->stalk_on_ok=FALSE;
		new_service->stalk_on_unknown=FALSE;
		new_service->stalk_on_warning=FALSE;
		new_service->stalk_on_critical=FALSE;
		new_service->have_stalking_options=FALSE;
		new_service->process_perf_data=TRUE;
		new_service->have_process_perf_data=FALSE;
		new_service->failure_prediction_enabled=TRUE;
		new_service->have_failure_prediction_enabled=FALSE;
		new_service->failure_prediction_options=NULL;
		new_service->retain_status_information=TRUE;
		new_service->have_retain_status_information=FALSE;
		new_service->retain_nonstatus_information=TRUE;
		new_service->have_retain_nonstatus_information=FALSE;
		new_service->has_been_resolved=FALSE;
		new_service->register_object=TRUE;
		new_service->_config_file=config_file;
		new_service->_start_line=start_line;

		/* add new service to list */
		xodtemplate_add_service_allocated(new_service);

		/* update current object pointer */
		xodtemplate_current_object=new_service;
		break;

	case XODTEMPLATE_HOSTDEPENDENCY:

		/* allocate memory */
		new_hostdependency=(xodtemplate_hostdependency *)malloc(sizeof(xodtemplate_hostdependency));
		if(new_hostdependency==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for hostdependency definition\n");
#endif
			return ERROR;
		        }
		
		new_hostdependency->template=NULL;
		new_hostdependency->name=NULL;
		new_hostdependency->hostgroup_name=NULL;
		new_hostdependency->dependent_hostgroup_name=NULL;
		new_hostdependency->host_name=NULL;
		new_hostdependency->dependent_host_name=NULL;
		new_hostdependency->fail_notify_on_up=FALSE;
		new_hostdependency->fail_notify_on_down=FALSE;
		new_hostdependency->fail_notify_on_unreachable=FALSE;
		new_hostdependency->have_notification_dependency_options=FALSE;
		new_hostdependency->has_been_resolved=FALSE;
		new_hostdependency->register_object=TRUE;
		new_hostdependency->_config_file=config_file;
		new_hostdependency->_start_line=start_line;

		/* add new hostdependency to head of list in memory */
		new_hostdependency->next=xodtemplate_hostdependency_list;
		xodtemplate_hostdependency_list=new_hostdependency;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_hostdependency_list;
		break;

	case XODTEMPLATE_HOSTESCALATION:

		/* allocate memory */
		new_hostescalation=(xodtemplate_hostescalation *)malloc(sizeof(xodtemplate_hostescalation));
		if(new_hostescalation==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for hostescalation definition\n");
#endif
			return ERROR;
		        }
		
		new_hostescalation->template=NULL;
		new_hostescalation->name=NULL;
		new_hostescalation->hostgroup_name=NULL;
		new_hostescalation->host_name=NULL;
		new_hostescalation->contact_groups=NULL;
		new_hostescalation->first_notification=-2;
		new_hostescalation->last_notification=-2;
		new_hostescalation->notification_interval=-2;
		new_hostescalation->have_first_notification=FALSE;
		new_hostescalation->have_last_notification=FALSE;
		new_hostescalation->have_notification_interval=FALSE;
		new_hostescalation->has_been_resolved=FALSE;
		new_hostescalation->register_object=TRUE;
		new_hostescalation->_config_file=config_file;
		new_hostescalation->_start_line=start_line;

		/* add new hostescalation to head of list in memory */
		new_hostescalation->next=xodtemplate_hostescalation_list;
		xodtemplate_hostescalation_list=new_hostescalation;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_hostescalation_list;
		break;

	case XODTEMPLATE_HOSTEXTINFO:

		/* allocate memory */
		new_hostextinfo=(xodtemplate_hostextinfo *)malloc(sizeof(xodtemplate_hostextinfo));
		if(new_hostextinfo==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for extended host info\n");
#endif
			return ERROR;
		        }

		new_hostextinfo->template=NULL;
		new_hostextinfo->name=NULL;
		new_hostextinfo->host_name=NULL;
		new_hostextinfo->hostgroup_name=NULL;
		new_hostextinfo->notes_url=NULL;
		new_hostextinfo->icon_image=NULL;
		new_hostextinfo->icon_image_alt=NULL;
		new_hostextinfo->vrml_image=NULL;
		new_hostextinfo->statusmap_image=NULL;
		new_hostextinfo->x_2d=-1;
		new_hostextinfo->y_2d=-1;
		new_hostextinfo->x_3d=0.0;
		new_hostextinfo->y_3d=0.0;
		new_hostextinfo->z_3d=0.0;
		new_hostextinfo->have_2d_coords=FALSE;
		new_hostextinfo->have_3d_coords=FALSE;
		new_hostextinfo->has_been_resolved=FALSE;
		new_hostextinfo->register_object=TRUE;
		new_hostextinfo->_config_file=config_file;
		new_hostextinfo->_start_line=start_line;

		/* add new timeperiod to head of list in memory */
		new_hostextinfo->next=xodtemplate_hostextinfo_list;
		xodtemplate_hostextinfo_list=new_hostextinfo;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_hostextinfo_list;
		break;

	case XODTEMPLATE_SERVICEEXTINFO:

		/* allocate memory */
		new_serviceextinfo=(xodtemplate_serviceextinfo *)malloc(sizeof(xodtemplate_serviceextinfo));
		if(new_serviceextinfo==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for extended service info\n");
#endif
			return ERROR;
		        }

		new_serviceextinfo->template=NULL;
		new_serviceextinfo->name=NULL;
		new_serviceextinfo->host_name=NULL;
		new_serviceextinfo->hostgroup_name=NULL;
		new_serviceextinfo->service_description=NULL;
		new_serviceextinfo->notes_url=NULL;
		new_serviceextinfo->icon_image=NULL;
		new_serviceextinfo->icon_image_alt=NULL;
		new_serviceextinfo->has_been_resolved=FALSE;
		new_serviceextinfo->register_object=TRUE;
		new_serviceextinfo->_config_file=config_file;
		new_serviceextinfo->_start_line=start_line;

		/* add new timeperiod to head of list in memory */
		new_serviceextinfo->next=xodtemplate_serviceextinfo_list;
		xodtemplate_serviceextinfo_list=new_serviceextinfo;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_serviceextinfo_list;
		break;

	default:
		return ERROR;
		break;
	        }

#ifdef DEBUG0
	printf("xodtemplate_begin_object_definition() end\n");
#endif

	return result;
        }



/* adds a property to an object definition */
int xodtemplate_add_object_property(char *input, int options){
	int result=OK;
	char variable[MAX_XODTEMPLATE_INPUT_BUFFER];
	char value[MAX_XODTEMPLATE_INPUT_BUFFER];
	char *temp_ptr;
	xodtemplate_timeperiod *temp_timeperiod;
	xodtemplate_command *temp_command;
	xodtemplate_contactgroup *temp_contactgroup;
	xodtemplate_hostgroup *temp_hostgroup;
	xodtemplate_servicedependency *temp_servicedependency;
	xodtemplate_serviceescalation *temp_serviceescalation;
	xodtemplate_contact *temp_contact;
	xodtemplate_host *temp_host;
	xodtemplate_service *temp_service;
	xodtemplate_hostdependency *temp_hostdependency;
	xodtemplate_hostescalation *temp_hostescalation;
	xodtemplate_hostextinfo *temp_hostextinfo;
	xodtemplate_serviceextinfo *temp_serviceextinfo;
	register int x;
	register int y;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_add_object_property() start\n");
#endif


	/* check to see if we should process this type of object */
	switch(xodtemplate_current_object_type){
	case XODTEMPLATE_TIMEPERIOD:
		if(!(options & READ_TIMEPERIODS))
			return OK;
		break;
	case XODTEMPLATE_COMMAND:
		if(!(options & READ_COMMANDS))
			return OK;
		break;
	case XODTEMPLATE_CONTACT:
		if(!(options & READ_CONTACTS))
			return OK;
		break;
	case XODTEMPLATE_CONTACTGROUP:
		if(!(options & READ_CONTACTGROUPS))
			return OK;
		break;
	case XODTEMPLATE_HOST:
		if(!(options & READ_HOSTS))
			return OK;
		break;
	case XODTEMPLATE_HOSTGROUP:
		if(!(options & READ_HOSTGROUPS))
			return OK;
		break;
	case XODTEMPLATE_SERVICE:
		if(!(options & READ_SERVICES))
			return OK;
		break;
	case XODTEMPLATE_SERVICEDEPENDENCY:
		if(!(options & READ_SERVICEDEPENDENCIES))
			return OK;
		break;
	case XODTEMPLATE_SERVICEESCALATION:
		if(!(options & READ_SERVICEESCALATIONS))
			return OK;
		break;
	case XODTEMPLATE_HOSTDEPENDENCY:
		if(!(options & READ_HOSTDEPENDENCIES))
			return OK;
		break;
	case XODTEMPLATE_HOSTESCALATION:
		if(!(options & READ_HOSTESCALATIONS))
			return OK;
		break;
	case XODTEMPLATE_HOSTEXTINFO:
		if(!(options & READ_HOSTEXTINFO))
			return OK;
		break;
	case XODTEMPLATE_SERVICEEXTINFO:
		if(!(options & READ_SERVICEEXTINFO))
			return OK;
		break;
	default:
		return ERROR;
		break;
	        }



	/* truncate if necessary */
	if(strlen(input)>MAX_XODTEMPLATE_INPUT_BUFFER)
		input[MAX_XODTEMPLATE_INPUT_BUFFER-1]='\x0';

	/* get variable name */
	for(x=0,y=0;input[x]!='\x0';x++){
		if(input[x]==' ' || input[x]=='\t')
			break;
		else
			variable[y++]=input[x];
	        }
	variable[y]='\x0';
			
	/* get variable value */
	if(x>=strlen(input)){
#ifdef DEBUG1
		printf("Error: NULL variable value in object definition.\n");
#endif
		return ERROR;
	        }
	for(y=0;input[x]!='\x0';x++)
		value[y++]=input[x];
	value[y]='\x0';

	/*
	printf("RAW VARIABLE: '%s'\n",variable);
	printf("RAW VALUE: '%s'\n",value);
	*/

#ifdef RUN_SLOW_AS_HELL
	strip(variable);
#endif
	strip(value);

	/*
	printf("STRIPPED VARIABLE: '%s'\n",variable);
	printf("STRIPPED VALUE: '%s'\n\n",value);
	*/


	switch(xodtemplate_current_object_type){

	case XODTEMPLATE_TIMEPERIOD:
		
		temp_timeperiod=(xodtemplate_timeperiod *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_timeperiod->template=strdup(value);
			if(temp_timeperiod->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for timeperiod template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_timeperiod->name=strdup(value);
			if(temp_timeperiod->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for timeperiod name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"timeperiod_name")){
			temp_timeperiod->timeperiod_name=strdup(value);
			if(temp_timeperiod->timeperiod_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for timeperiod name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"alias")){
			temp_timeperiod->alias=strdup(value);
			if(temp_timeperiod->alias==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for timeperiod alias.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"monday") || !strcmp(variable,"tuesday") || !strcmp(variable,"wednesday") || !strcmp(variable,"thursday") || !strcmp(variable,"friday") || !strcmp(variable,"saturday") || !strcmp(variable,"sunday")){
			if(!strcmp(variable,"monday"))
				x=1;
			else if(!strcmp(variable,"tuesday"))
				x=2;
			else if(!strcmp(variable,"wednesday"))
				x=3;
			else if(!strcmp(variable,"thursday"))
				x=4;
			else if(!strcmp(variable,"friday"))
				x=5;
			else if(!strcmp(variable,"saturday"))
				x=6;
			else
				x=0;
			temp_timeperiod->timeranges[x]=strdup(value);
			if(temp_timeperiod->timeranges[x]==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for timeperiod timerange.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"register"))
			temp_timeperiod->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid timeperiod object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }
		break;



	case XODTEMPLATE_COMMAND:

		temp_command=(xodtemplate_command *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_command->template=strdup(value);
			if(temp_command->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for command template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_command->name=strdup(value);
			if(temp_command->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for command name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"command_name")){
			temp_command->command_name=strdup(value);
			if(temp_command->command_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for command name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"command_line")){
			temp_command->command_line=strdup(value);
			if(temp_command->command_line==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for command line.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"register"))
			temp_command->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid command object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;

	case XODTEMPLATE_CONTACTGROUP:

		temp_contactgroup=(xodtemplate_contactgroup *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_contactgroup->template=strdup(value);
			if(temp_contactgroup->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contactgroup template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_contactgroup->name=strdup(value);
			if(temp_contactgroup->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contactgroup name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"contactgroup_name")){
			temp_contactgroup->contactgroup_name=strdup(value);
			if(temp_contactgroup->contactgroup_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contactgroup name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"alias")){
			temp_contactgroup->alias=strdup(value);
			if(temp_contactgroup->alias==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contactgroup alias.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"members")){
			temp_contactgroup->members=strdup(value);
			if(temp_contactgroup->members==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contactgroup members.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"register"))
			temp_contactgroup->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid contactgroup object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;

	case XODTEMPLATE_HOSTGROUP:

		temp_hostgroup=(xodtemplate_hostgroup *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_hostgroup->template=strdup(value);
			if(temp_hostgroup->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostgroup template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_hostgroup->name=strdup(value);
			if(temp_hostgroup->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostgroup name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"hostgroup_name")){
			temp_hostgroup->hostgroup_name=strdup(value);
			if(temp_hostgroup->hostgroup_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostgroup name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"alias")){
			temp_hostgroup->alias=strdup(value);
			if(temp_hostgroup->alias==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostgroup alias.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"members")){
			if(temp_hostgroup->members==NULL)
				temp_hostgroup->members=strdup(value);
			else{
				temp_hostgroup->members=(char *)realloc(temp_hostgroup->members,strlen(temp_hostgroup->members)+strlen(value)+2);
				if(temp_hostgroup->members!=NULL){
					strcat(temp_hostgroup->members,",");
					strcat(temp_hostgroup->members,value);
				        }
			        }
			if(temp_hostgroup->members==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostgroup members.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"register"))
			temp_hostgroup->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid hostgroup object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;

	
	case XODTEMPLATE_SERVICEDEPENDENCY:

		temp_servicedependency=(xodtemplate_servicedependency *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_servicedependency->template=strdup(value);
			if(temp_servicedependency->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for servicedependency template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_servicedependency->name=strdup(value);
			if(temp_servicedependency->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for servicedependency name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroups") || !strcmp(variable,"hostgroup_name")){
			temp_servicedependency->hostgroup_name=strdup(value);
			if(temp_servicedependency->hostgroup_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for servicedependency hostgroup.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host") || !strcmp(variable,"host_name") || !strcmp(variable,"master_host") || !strcmp(variable,"master_host_name")){
			temp_servicedependency->host_name=strdup(value);
			if(temp_servicedependency->host_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for servicedependency host.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"description") || !strcmp(variable,"service_description") || !strcmp(variable,"master_description") || !strcmp(variable,"master_service_description")){
			temp_servicedependency->service_description=strdup(value);
			if(temp_servicedependency->service_description==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for servicedependency service_description.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"dependent_hostgroup") || !strcmp(variable,"dependent_hostgroups") || !strcmp(variable,"dependent_hostgroup_name")){
			temp_servicedependency->dependent_hostgroup_name=strdup(value);
			if(temp_servicedependency->dependent_hostgroup_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for servicedependency dependent hostgroup.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"dependent_host") || !strcmp(variable,"dependent_host_name")){
			temp_servicedependency->dependent_host_name=strdup(value);
			if(temp_servicedependency->dependent_host_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for servicedependency dependent host.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"dependent_description") || !strcmp(variable,"dependent_service_description")){
			temp_servicedependency->dependent_service_description=strdup(value);
			if(temp_servicedependency->dependent_service_description==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for servicedependency dependent service_description.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"execution_failure_options") || !strcmp(variable,"execution_failure_criteria")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"o") || !strcmp(temp_ptr,"ok"))
					temp_servicedependency->fail_execute_on_ok=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unknown"))
					temp_servicedependency->fail_execute_on_unknown=TRUE;
				else if(!strcmp(temp_ptr,"w") || !strcmp(temp_ptr,"warning"))
					temp_servicedependency->fail_execute_on_warning=TRUE;
				else if(!strcmp(temp_ptr,"c") || !strcmp(temp_ptr,"critical"))
					temp_servicedependency->fail_execute_on_critical=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_servicedependency->fail_execute_on_ok=FALSE;
					temp_servicedependency->fail_execute_on_unknown=FALSE;
					temp_servicedependency->fail_execute_on_warning=FALSE;
					temp_servicedependency->fail_execute_on_critical=FALSE;
				        }
				else{
#ifdef NSCORE
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid execution dependency option '%s' in servicedependency definition.\n",temp_ptr);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
					return ERROR;
				        }
			        }
			temp_servicedependency->have_execution_dependency_options=TRUE;
		        }
		else if(!strcmp(variable,"notification_failure_options") || !strcmp(variable,"notification_failure_criteria")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"o") || !strcmp(temp_ptr,"ok"))
					temp_servicedependency->fail_notify_on_ok=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unknown"))
					temp_servicedependency->fail_notify_on_unknown=TRUE;
				else if(!strcmp(temp_ptr,"w") || !strcmp(temp_ptr,"warning"))
					temp_servicedependency->fail_notify_on_warning=TRUE;
				else if(!strcmp(temp_ptr,"c") || !strcmp(temp_ptr,"critical"))
					temp_servicedependency->fail_notify_on_critical=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_servicedependency->fail_notify_on_ok=FALSE;
					temp_servicedependency->fail_notify_on_unknown=FALSE;
					temp_servicedependency->fail_notify_on_warning=FALSE;
					temp_servicedependency->fail_notify_on_critical=FALSE;
				        }
				else{
#ifdef NSCORE
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notification dependency option '%s' in servicedependency definition.\n",temp_ptr);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
					return ERROR;
				        }
			        }
			temp_servicedependency->have_notification_dependency_options=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_servicedependency->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid servicedependency object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;

	
	case XODTEMPLATE_SERVICEESCALATION:

		temp_serviceescalation=(xodtemplate_serviceescalation *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_serviceescalation->template=strdup(value);
			if(temp_serviceescalation->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for serviceescalation template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_serviceescalation->name=strdup(value);
			if(temp_serviceescalation->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for serviceescalation name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroups") || !strcmp(variable,"hostgroup_name")){
			temp_serviceescalation->hostgroup_name=strdup(value);
			if(temp_serviceescalation->hostgroup_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for serviceescalation hostgroup.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host") || !strcmp(variable,"host_name")){
			temp_serviceescalation->host_name=strdup(value);
			if(temp_serviceescalation->host_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for serviceescalation host.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"description") || !strcmp(variable,"service_description")){
			temp_serviceescalation->service_description=strdup(value);
			if(temp_serviceescalation->service_description==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for serviceescalation service_description.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"contact_groups")){
			temp_serviceescalation->contact_groups=strdup(value);
			if(temp_serviceescalation->contact_groups==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for serviceescalation contact_groups.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"first_notification")){
			temp_serviceescalation->first_notification=atoi(value);
			temp_serviceescalation->have_first_notification=TRUE;
		        }
		else if(!strcmp(variable,"last_notification")){
			temp_serviceescalation->last_notification=atoi(value);
			temp_serviceescalation->have_last_notification=TRUE;
		        }
		else if(!strcmp(variable,"notification_interval")){
			temp_serviceescalation->notification_interval=atoi(value);
			temp_serviceescalation->have_notification_interval=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_serviceescalation->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid serviceescalation object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;
	

	case XODTEMPLATE_CONTACT:

		temp_contact=(xodtemplate_contact *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_contact->template=strdup(value);
			if(temp_contact->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_contact->name=strdup(value);
			if(temp_contact->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"contact_name")){
			temp_contact->contact_name=strdup(value);
			if(temp_contact->contact_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"alias")){
			temp_contact->alias=strdup(value);
			if(temp_contact->alias==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact alias.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"email")){
			temp_contact->email=strdup(value);
			if(temp_contact->email==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact email.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"pager")){
			temp_contact->pager=strdup(value);
			if(temp_contact->pager==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact pager.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host_notification_period")){
			temp_contact->host_notification_period=strdup(value);
			if(temp_contact->host_notification_period==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact host_notification_period.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host_notification_commands")){
			temp_contact->host_notification_commands=strdup(value);
			if(temp_contact->host_notification_commands==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact host_notification_commands.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"service_notification_period")){
			temp_contact->service_notification_period=strdup(value);
			if(temp_contact->service_notification_period==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact service_notification_period.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"service_notification_commands")){
			temp_contact->service_notification_commands=strdup(value);
			if(temp_contact->service_notification_commands==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact service_notification_commands.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host_notification_period")){
			temp_contact->host_notification_period=strdup(value);
			if(temp_contact->host_notification_period==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact host_notification_period.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"service_notification_period")){
			temp_contact->service_notification_period=strdup(value);
			if(temp_contact->service_notification_period==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for contact service_notification_period.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host_notification_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"d") || !strcmp(temp_ptr,"down"))
					temp_contact->notify_on_host_down=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unreachable"))
					temp_contact->notify_on_host_unreachable=TRUE;
				else if(!strcmp(temp_ptr,"r") || !strcmp(temp_ptr,"recovery"))
					temp_contact->notify_on_host_recovery=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_contact->notify_on_host_down=FALSE;
					temp_contact->notify_on_host_unreachable=FALSE;
					temp_contact->notify_on_host_recovery=FALSE;
				        }
				else{
#ifdef NSCORE
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid host notification option '%s' in contact definition.\n",temp_ptr);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
					return ERROR;
				        }
			        }
			temp_contact->have_host_notification_options=TRUE;
		        }
		else if(!strcmp(variable,"service_notification_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unknown"))
					temp_contact->notify_on_service_unknown=TRUE;
				else if(!strcmp(temp_ptr,"w") || !strcmp(temp_ptr,"warning"))
					temp_contact->notify_on_service_warning=TRUE;
				else if(!strcmp(temp_ptr,"c") || !strcmp(temp_ptr,"critical"))
					temp_contact->notify_on_service_critical=TRUE;
				else if(!strcmp(temp_ptr,"r") || !strcmp(temp_ptr,"recovery"))
					temp_contact->notify_on_service_recovery=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_contact->notify_on_service_unknown=FALSE;
					temp_contact->notify_on_service_warning=FALSE;
					temp_contact->notify_on_service_critical=FALSE;
					temp_contact->notify_on_service_recovery=FALSE;
				        }
				else{
#ifdef NSCORE
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid service notification option '%s' in contact definition.\n",temp_ptr);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
					return ERROR;
				        }
			        }
			temp_contact->have_service_notification_options=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_contact->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid contact object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;


	case XODTEMPLATE_HOST:

		temp_host=(xodtemplate_host *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_host->template=strdup(value);
			if(temp_host->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_host->name=strdup(value);
			if(temp_host->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host_name")){
			temp_host->host_name=strdup(value);
			if(temp_host->host_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"alias")){
			temp_host->alias=strdup(value);
			if(temp_host->alias==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host alias.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"address")){
			temp_host->address=strdup(value);
			if(temp_host->address==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host address.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"parents")){
			temp_host->parents=strdup(value);
			if(temp_host->parents==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host parents.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"hostgroups")){
			temp_host->hostgroups=strdup(value);
			if(temp_host->hostgroups==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host hostgroups.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"contact_groups")){
			temp_host->contact_groups=strdup(value);
			if(temp_host->contact_groups==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host contact_groups.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"notification_period")){
			temp_host->notification_period=strdup(value);
			if(temp_host->notification_period==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host notification_period.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"check_command")){
			temp_host->check_command=strdup(value);
			if(temp_host->check_command==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host check_command.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"event_handler")){
			temp_host->event_handler=strdup(value);
			if(temp_host->event_handler==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host event_handler.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"failure_prediction_options")){
			temp_host->failure_prediction_options=strdup(value);
			if(temp_host->failure_prediction_options==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for host failure_prediction_options.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"max_check_attempts")){
			temp_host->max_check_attempts=atoi(value);
			temp_host->have_max_check_attempts=TRUE;
		        }
		else if(!strcmp(variable,"checks_enabled") || !strcmp(variable,"active_checks_enabled")){
			temp_host->active_checks_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_active_checks_enabled=TRUE;
		        }
		else if(!strcmp(variable,"passive_checks_enabled")){
			temp_host->passive_checks_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_passive_checks_enabled=TRUE;
		        }
		else if(!strcmp(variable,"event_handler_enabled")){
			temp_host->event_handler_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_event_handler_enabled=TRUE;
		        }
		else if(!strcmp(variable,"low_flap_threshold")){
			temp_host->low_flap_threshold=strtod(value,NULL);
			temp_host->have_low_flap_threshold=TRUE;
		        }
		else if(!strcmp(variable,"high_flap_threshold")){
			temp_host->high_flap_threshold=strtod(value,NULL);
			temp_host->have_high_flap_threshold=TRUE;
		        }
		else if(!strcmp(variable,"flap_detection_enabled")){
			temp_host->flap_detection_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_flap_detection_enabled=TRUE;
		        }
		else if(!strcmp(variable,"notification_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"d") || !strcmp(temp_ptr,"down"))
					temp_host->notify_on_down=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unreachable"))
					temp_host->notify_on_unreachable=TRUE;
				else if(!strcmp(temp_ptr,"r") || !strcmp(temp_ptr,"recovery"))
					temp_host->notify_on_recovery=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_host->notify_on_down=FALSE;
					temp_host->notify_on_unreachable=FALSE;
					temp_host->notify_on_recovery=FALSE;
				        }
				else{
#ifdef NSCORE
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notification option '%s' in host definition.\n",temp_ptr);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
					return ERROR;
				        }
			        }
			temp_host->have_notification_options=TRUE;
		        }
		else if(!strcmp(variable,"notifications_enabled")){
			temp_host->notifications_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_notifications_enabled=TRUE;
		        }
		else if(!strcmp(variable,"notification_interval")){
			temp_host->notification_interval=atoi(value);
			temp_host->have_notification_interval=TRUE;
		        }
		else if(!strcmp(variable,"stalking_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"o") || !strcmp(temp_ptr,"up"))
					temp_host->stalk_on_up=TRUE;
				else if(!strcmp(temp_ptr,"d") || !strcmp(temp_ptr,"down"))
					temp_host->stalk_on_down=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unreachable"))
					temp_host->stalk_on_unreachable=TRUE;
				else{
#ifdef NSCORE
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid stalking option '%s' in host definition.\n",temp_ptr);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
					return ERROR;
				        }
			        }
			temp_host->have_stalking_options=TRUE;
		        }
		else if(!strcmp(variable,"process_perf_data")){
			temp_host->process_perf_data=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_process_perf_data=TRUE;
		        }
		else if(!strcmp(variable,"failure_prediction_enabled")){
			temp_host->failure_prediction_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_failure_prediction_enabled=TRUE;
		        }
		else if(!strcmp(variable,"retain_status_information")){
			temp_host->retain_status_information=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_retain_status_information=TRUE;
		        }
		else if(!strcmp(variable,"retain_nonstatus_information")){
			temp_host->retain_nonstatus_information=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_retain_nonstatus_information=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_host->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid host object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;

	case XODTEMPLATE_SERVICE:

		temp_service=(xodtemplate_service *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_service->template=strdup(value);
			if(temp_service->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for service template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			if(!xodtemplate_rename_service(temp_service,value)){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for service name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroups") || !strcmp(variable,"hostgroup_name")){
			temp_service->hostgroup_name=strdup(value);
			if(temp_service->hostgroup_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for service hostgroup.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host") || !strcmp(variable,"hosts") || !strcmp(variable,"host_name")){
			temp_service->host_name=strdup(value);
			if(temp_service->host_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for service host.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"service_description") || !strcmp(variable,"description")){
			temp_service->service_description=strdup(value);
			if(temp_service->service_description==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for service service_description.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"check_command")){
			temp_service->check_command=strdup(value);
			if(temp_service->check_command==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for service check_command.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"check_period")){
			temp_service->check_period=strdup(value);
			if(temp_service->check_period==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for service check_period.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"event_handler")){
			temp_service->event_handler=strdup(value);
			if(temp_service->event_handler==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for service event_handler.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"notification_period")){
			temp_service->notification_period=strdup(value);
			if(temp_service->notification_period==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for service notification_period.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"contact_groups")){
			temp_service->contact_groups=strdup(value);
			if(temp_service->contact_groups==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for service contact_groups.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"failure_prediction_options")){
			temp_service->failure_prediction_options=strdup(value);
			if(temp_service->failure_prediction_options==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for service failure_prediction_options.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"max_check_attempts")){
			temp_service->max_check_attempts=atoi(value);
			temp_service->have_max_check_attempts=TRUE;
		        }
		else if(!strcmp(variable,"normal_check_interval")){
			temp_service->normal_check_interval=atoi(value);
			temp_service->have_normal_check_interval=TRUE;
		        }
		else if(!strcmp(variable,"retry_check_interval")){
			temp_service->retry_check_interval=atoi(value);
			temp_service->have_retry_check_interval=TRUE;
		        }
		else if(!strcmp(variable,"active_checks_enabled")){
			temp_service->active_checks_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_active_checks_enabled=TRUE;
		        }
		else if(!strcmp(variable,"passive_checks_enabled")){
			temp_service->passive_checks_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_passive_checks_enabled=TRUE;
		        }
		else if(!strcmp(variable,"parallelize_check")){
			temp_service->parallelize_check=atoi(value);
			temp_service->have_parallelize_check=TRUE;
		        }
		else if(!strcmp(variable,"is_volatile")){
			temp_service->is_volatile=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_is_volatile=TRUE;
		        }
		else if(!strcmp(variable,"obsess_over_service")){
			temp_service->obsess_over_service=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_obsess_over_service=TRUE;
		        }
		else if(!strcmp(variable,"event_handler_enabled")){
			temp_service->event_handler_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_event_handler_enabled=TRUE;
		        }
		else if(!strcmp(variable,"check_freshness")){
			temp_service->check_freshness=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_check_freshness=TRUE;
		        }
		else if(!strcmp(variable,"freshness_threshold")){
			temp_service->freshness_threshold=atoi(value);
			temp_service->have_freshness_threshold=TRUE;
		        }
		else if(!strcmp(variable,"low_flap_threshold")){
			temp_service->low_flap_threshold=strtod(value,NULL);
			temp_service->have_low_flap_threshold=TRUE;
		        }
		else if(!strcmp(variable,"high_flap_threshold")){
			temp_service->high_flap_threshold=strtod(value,NULL);
			temp_service->have_high_flap_threshold=TRUE;
		        }
		else if(!strcmp(variable,"flap_detection_enabled")){
			temp_service->flap_detection_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_flap_detection_enabled=TRUE;
		        }
		else if(!strcmp(variable,"notification_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unknown"))
					temp_service->notify_on_unknown=TRUE;
				else if(!strcmp(temp_ptr,"w") || !strcmp(temp_ptr,"warning"))
					temp_service->notify_on_warning=TRUE;
				else if(!strcmp(temp_ptr,"c") || !strcmp(temp_ptr,"critical"))
					temp_service->notify_on_critical=TRUE;
				else if(!strcmp(temp_ptr,"r") || !strcmp(temp_ptr,"recovery"))
					temp_service->notify_on_recovery=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_service->notify_on_unknown=FALSE;
					temp_service->notify_on_warning=FALSE;
					temp_service->notify_on_critical=FALSE;
					temp_service->notify_on_recovery=FALSE;
				        }
				else{
#ifdef NSCORE
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notification option '%s' in service definition.\n",temp_ptr);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
					return ERROR;
				        }
			        }
			temp_service->have_notification_options=TRUE;
		        }
		else if(!strcmp(variable,"notifications_enabled")){
			temp_service->notifications_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_notifications_enabled=TRUE;
		        }
		else if(!strcmp(variable,"notification_interval")){
			temp_service->notification_interval=atoi(value);
			temp_service->have_notification_interval=TRUE;
		        }
		else if(!strcmp(variable,"stalking_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"o") || !strcmp(temp_ptr,"ok"))
					temp_service->stalk_on_ok=TRUE;
				else if(!strcmp(temp_ptr,"w") || !strcmp(temp_ptr,"warning"))
					temp_service->stalk_on_warning=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unknown"))
					temp_service->stalk_on_unknown=TRUE;
				else if(!strcmp(temp_ptr,"c") || !strcmp(temp_ptr,"critical"))
					temp_service->stalk_on_critical=TRUE;
				else{
#ifdef NSCORE
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid stalking option '%s' in service definition.\n",temp_ptr);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
					return ERROR;
				        }
			        }
			temp_service->have_stalking_options=TRUE;
		        }
		else if(!strcmp(variable,"process_perf_data")){
			temp_service->process_perf_data=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_process_perf_data=TRUE;
		        }
		else if(!strcmp(variable,"failure_prediction_enabled")){
			temp_service->failure_prediction_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_failure_prediction_enabled=TRUE;
		        }
		else if(!strcmp(variable,"retain_status_information")){
			temp_service->retain_status_information=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_retain_status_information=TRUE;
		        }
		else if(!strcmp(variable,"retain_nonstatus_information")){
			temp_service->retain_nonstatus_information=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_retain_nonstatus_information=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_service->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid service object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;

	case XODTEMPLATE_HOSTDEPENDENCY:

		temp_hostdependency=(xodtemplate_hostdependency *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_hostdependency->template=strdup(value);
			if(temp_hostdependency->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostdependency template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_hostdependency->name=strdup(value);
			if(temp_hostdependency->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostdependency name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroups") || !strcmp(variable,"hostgroup_name")){
			temp_hostdependency->hostgroup_name=strdup(value);
			if(temp_hostdependency->hostgroup_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostdependency hostgroup.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host") || !strcmp(variable,"host_name") || !strcmp(variable,"master_host") || !strcmp(variable,"master_host_name")){
			temp_hostdependency->host_name=strdup(value);
			if(temp_hostdependency->host_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostdependency host.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"dependent_hostgroup") || !strcmp(variable,"dependent_hostgroups") || !strcmp(variable,"dependent_hostgroup_name")){
			temp_hostdependency->dependent_hostgroup_name=strdup(value);
			if(temp_hostdependency->dependent_hostgroup_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostdependency dependent hostgroup.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"dependent_host") || !strcmp(variable,"dependent_host_name")){
			temp_hostdependency->dependent_host_name=strdup(value);
			if(temp_hostdependency->dependent_host_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostdependency dependent host.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"notification_failure_options") || !strcmp(variable,"notification_failure_criteria")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"o") || !strcmp(temp_ptr,"up"))
					temp_hostdependency->fail_notify_on_up=TRUE;
				else if(!strcmp(temp_ptr,"d") || !strcmp(temp_ptr,"down"))
					temp_hostdependency->fail_notify_on_down=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unreachable"))
					temp_hostdependency->fail_notify_on_unreachable=TRUE;
				else{
#ifdef NSCORE
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid notification dependency option '%s' in hostdependency definition.\n",temp_ptr);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
					return ERROR;
				        }
			        }
			temp_hostdependency->have_notification_dependency_options=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_hostdependency->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid hostdependency object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;

	
	case XODTEMPLATE_HOSTESCALATION:

		temp_hostescalation=(xodtemplate_hostescalation *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_hostescalation->template=strdup(value);
			if(temp_hostescalation->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostescalation template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_hostescalation->name=strdup(value);
			if(temp_hostescalation->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostescalation name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroups") || !strcmp(variable,"hostgroup_name")){
			temp_hostescalation->hostgroup_name=strdup(value);
			if(temp_hostescalation->hostgroup_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostescalation hostgroup.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host") || !strcmp(variable,"host_name")){
			temp_hostescalation->host_name=strdup(value);
			if(temp_hostescalation->host_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostescalation host.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"contact_groups")){
			temp_hostescalation->contact_groups=strdup(value);
			if(temp_hostescalation->contact_groups==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for hostescalation contact_groups.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"first_notification")){
			temp_hostescalation->first_notification=atoi(value);
			temp_hostescalation->have_first_notification=TRUE;
		        }
		else if(!strcmp(variable,"last_notification")){
			temp_hostescalation->last_notification=atoi(value);
			temp_hostescalation->have_last_notification=TRUE;
		        }
		else if(!strcmp(variable,"notification_interval")){
			temp_hostescalation->notification_interval=atoi(value);
			temp_hostescalation->have_notification_interval=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_hostescalation->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid hostescalation object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;
	
	case XODTEMPLATE_HOSTEXTINFO:
		
		temp_hostextinfo=xodtemplate_hostextinfo_list;

		if(!strcmp(variable,"use")){
			temp_hostextinfo->template=strdup(value);
			if(temp_hostextinfo->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended host info template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_hostextinfo->name=strdup(value);
			if(temp_hostextinfo->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended host info name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host_name")){
			temp_hostextinfo->host_name=(char *)malloc(strlen(value)+1);
			if(temp_hostextinfo->host_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended host info host_name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostextinfo->host_name,value);
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroup_name")){
			temp_hostextinfo->hostgroup_name=strdup(value);
			if(temp_hostextinfo->hostgroup_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended host info hostgroup_name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"notes_url")){
			temp_hostextinfo->notes_url=strdup(value);
			if(temp_hostextinfo->notes_url==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended host info notes_url.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"icon_image")){
			temp_hostextinfo->icon_image=strdup(value);
			if(temp_hostextinfo->icon_image==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended host info icon_image.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"icon_image_alt")){
			temp_hostextinfo->icon_image_alt=strdup(value);
			if(temp_hostextinfo->icon_image_alt==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended host info icon_image_alt.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"vrml_image")){
			temp_hostextinfo->vrml_image=strdup(value);
			if(temp_hostextinfo->vrml_image==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended host info vrml_image.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"gd2_image")|| !strcmp(variable,"statusmap_image")){
			temp_hostextinfo->statusmap_image=strdup(value);
			if(temp_hostextinfo->statusmap_image==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended host info statusmap_image.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"2d_coords")){
			temp_ptr=strtok(value,", ");
			if(temp_ptr==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid 2d_coords value in extended host info definition.\n",temp_ptr);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
			temp_hostextinfo->x_2d=atoi(temp_ptr);
			temp_ptr=strtok(NULL,", ");
			if(temp_ptr==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid 2d_coords value in extended host info definition.\n",temp_ptr);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
			temp_hostextinfo->y_2d=atoi(temp_ptr);
			temp_hostextinfo->have_2d_coords=TRUE;
		        }
		else if(!strcmp(variable,"3d_coords")){
			temp_ptr=strtok(value,", ");
			if(temp_ptr==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid 3d_coords value in extended host info definition.\n",temp_ptr);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
			temp_hostextinfo->x_3d=strtod(temp_ptr,NULL);
			temp_ptr=strtok(NULL,", ");
			if(temp_ptr==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid 3d_coords value in extended host info definition.\n",temp_ptr);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
			temp_hostextinfo->y_3d=strtod(temp_ptr,NULL);
			temp_ptr=strtok(NULL,", ");
			if(temp_ptr==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid 3d_coords value in extended host info definition.\n",temp_ptr);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
			temp_hostextinfo->z_3d=strtod(temp_ptr,NULL);
			temp_hostextinfo->have_3d_coords=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_hostextinfo->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid hostextinfo object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;
	
	case XODTEMPLATE_SERVICEEXTINFO:
		
		temp_serviceextinfo=xodtemplate_serviceextinfo_list;

		if(!strcmp(variable,"use")){
			temp_serviceextinfo->template=strdup(value);
			if(temp_serviceextinfo->template==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended service info template.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"name")){
			temp_serviceextinfo->name=strdup(value);
			if(temp_serviceextinfo->name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended service info name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"host_name")){
			temp_serviceextinfo->host_name=strdup(value);
			if(temp_serviceextinfo->host_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended service info host_name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroup_name")){
			temp_serviceextinfo->hostgroup_name=strdup(value);
			if(temp_serviceextinfo->hostgroup_name==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended service info hostgroup_name.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"service_description")){
			temp_serviceextinfo->service_description=strdup(value);
			if(temp_serviceextinfo->service_description==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended service info service_description.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"notes_url")){
			temp_serviceextinfo->notes_url=strdup(value);
			if(temp_serviceextinfo->notes_url==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended service info notes_url.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"icon_image")){
			temp_serviceextinfo->icon_image=strdup(value);
			if(temp_serviceextinfo->icon_image==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended service info icon_image.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"icon_image_alt")){
			temp_serviceextinfo->icon_image_alt=strdup(value);
			if(temp_serviceextinfo->icon_image_alt==NULL){
#ifdef DEBUG1
				printf("Error: Could not allocate memory for extended service info icon_image_alt.\n");
#endif
				return ERROR;
			        }
		        }
		else if(!strcmp(variable,"register"))
			temp_serviceextinfo->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Invalid serviceextinfo object directive '%s'.\n",variable);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		break;

	default:
		return ERROR;
		break;
	        }

#ifdef DEBUG0
	printf("xodtemplate_add_object_property() end\n");
#endif

	return result;
        }



/* completes an object definition */
int xodtemplate_end_object_definition(int options){
	int result=OK;

#ifdef DEBUG0
	printf("xodtemplate_end_object_definition() start\n");
#endif

	xodtemplate_current_object=NULL;
	xodtemplate_current_object_type=XODTEMPLATE_NONE;

#ifdef DEBUG0
	printf("xodtemplate_end_object_definition() end\n");
#endif

	return result;
        }





/******************************************************************/
/***************** OBJECT DUPLICATION FUNCTIONS *******************/
/******************************************************************/


/* duplicates object definitions */
int xodtemplate_duplicate_objects(void){
	int result=OK;
	xodtemplate_service *temp_service;
	xodtemplate_hostescalation *temp_hostescalation;
	xodtemplate_serviceescalation *temp_serviceescalation;
	xodtemplate_hostdependency *temp_hostdependency;
	xodtemplate_servicedependency *temp_servicedependency;
	xodtemplate_hostlist *temp_hostlist;
	xodtemplate_hostlist *this_hostlist;
	xodtemplate_servicelist *temp_servicelist;
	xodtemplate_servicelist *this_servicelist;
	xodtemplate_hostlist *master_hostlist;
	xodtemplate_hostlist *dependent_hostlist;
	xodtemplate_hostextinfo *temp_hostextinfo;
	xodtemplate_serviceextinfo *temp_serviceextinfo;
	char *host_name;
	int first_item;
	void *xod_svc_cursor;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_duplicate_objects() start\n");
#endif


	/*********************************************************/
	/* ALL OBJECTS HAVE ALREADY BEEN RESOLVED AT THIS POINT! */
	/*********************************************************/


	/****** DUPLICATE SERVICE DEFINITIONS WITH ONE OR MORE HOSTGROUP AND/OR HOST NAMES ******/
	xod_svc_cursor=get_xodtemplate_service_cursor();
	while(temp_service=get_next_xodtemplate_service(xod_svc_cursor)){

		/* skip service definitions without enough data */
		if(temp_service->hostgroup_name==NULL && temp_service->host_name==NULL)
			continue;

		/* get list of hosts */
		temp_hostlist=xodtemplate_expand_hostgroups_and_hosts(temp_service->hostgroup_name,temp_service->host_name);
		if(temp_hostlist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand hostgroups and/or hosts specified in service (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_service->_config_file),temp_service->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* add a copy of the service for every host in the hostgroup/host name list */
		first_item=TRUE;
		for(this_hostlist=temp_hostlist;this_hostlist!=NULL;this_hostlist=this_hostlist->next){

			/* if this is the first duplication, use the existing entry */
			if(first_item==TRUE){

				free(temp_service->host_name);
				temp_service->host_name=strdup(this_hostlist->host_name);
				if(temp_service->host_name==NULL){
					xodtemplate_free_hostlist(temp_hostlist);
					return ERROR;
				        }

				first_item=FALSE;
				continue;
			        }

			/* duplicate service definition */
			result=xodtemplate_duplicate_service(temp_service,this_hostlist->host_name);

			/* exit on error */
			if(result==ERROR){
				free(host_name);
				return ERROR;
		                }
		        }

		/* free memory we used for host list */
		xodtemplate_free_hostlist(temp_hostlist);
	        }
	free_xodtemplate_service_cursor(xod_svc_cursor);


	/****** DUPLICATE HOST ESCALATION DEFINITIONS WITH ONE OR MORE HOSTGROUP AND/OR HOST NAMES ******/
	for(temp_hostescalation=xodtemplate_hostescalation_list;temp_hostescalation!=NULL;temp_hostescalation=temp_hostescalation->next){

		/* skip host escalation definitions without enough data */
		if(temp_hostescalation->hostgroup_name==NULL && temp_hostescalation->host_name==NULL)
			continue;

		/* get list of hosts */
		temp_hostlist=xodtemplate_expand_hostgroups_and_hosts(temp_hostescalation->hostgroup_name,temp_hostescalation->host_name);
		if(temp_hostlist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand hostgroups and/or hosts specified in host escalation (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_hostescalation->_config_file),temp_hostescalation->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* add a copy of the hostescalation for every host in the hostgroup/host name list */
		first_item=TRUE;
		for(this_hostlist=temp_hostlist;this_hostlist!=NULL;this_hostlist=this_hostlist->next){

			/* if this is the first duplication, use the existing entry */
			if(first_item==TRUE){

				free(temp_hostescalation->host_name);
				temp_hostescalation->host_name=strdup(this_hostlist->host_name);
				if(temp_hostescalation->host_name==NULL){
					xodtemplate_free_hostlist(temp_hostlist);
					return ERROR;
				        }

				first_item=FALSE;
				continue;
			        }

			/* duplicate hostescalation definition */
			result=xodtemplate_duplicate_hostescalation(temp_hostescalation,this_hostlist->host_name);

			/* exit on error */
			if(result==ERROR){
				free(host_name);
				return ERROR;
		                }
		        }

		/* free memory we used for host list */
		xodtemplate_free_hostlist(temp_hostlist);
	        }

	
	/****** DUPLICATE SERVICE ESCALATION DEFINITIONS WITH ONE OR MORE HOSTGROUP AND/OR HOST NAMES ******/
	for(temp_serviceescalation=xodtemplate_serviceescalation_list;temp_serviceescalation!=NULL;temp_serviceescalation=temp_serviceescalation->next){

		/* skip service escalation definitions without enough data */
		if(temp_serviceescalation->hostgroup_name==NULL && temp_serviceescalation->host_name==NULL)
			continue;

		/* get list of hosts */
		temp_hostlist=xodtemplate_expand_hostgroups_and_hosts(temp_serviceescalation->hostgroup_name,temp_serviceescalation->host_name);
		if(temp_hostlist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand hostgroups and/or hosts specified in service escalation (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_serviceescalation->_config_file),temp_serviceescalation->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* duplicate service escalation entries */
		first_item=TRUE;
		for(this_hostlist=temp_hostlist;this_hostlist!=NULL;this_hostlist=this_hostlist->next){

			/* if this is the first duplication,use the existing entry */
			if(first_item==TRUE){

				free(temp_serviceescalation->host_name);
				temp_serviceescalation->host_name=strdup(this_hostlist->host_name);
				if(temp_serviceescalation->host_name==NULL){
					xodtemplate_free_hostlist(temp_hostlist);
					return ERROR;
				        }

				first_item=FALSE;
				continue;
			        }

			/* duplicate service escalation definition */
			result=xodtemplate_duplicate_serviceescalation(temp_serviceescalation,this_hostlist->host_name,NULL);

			/* exit on error */
			if(result==ERROR){
				xodtemplate_free_hostlist(temp_hostlist);
				return ERROR;
		                }
		        }

		/* free memory we used for host list */
		xodtemplate_free_hostlist(temp_hostlist);
	        }


	/****** DUPLICATE SERVICE ESCALATION DEFINITIONS WITH MULTIPLE DESCRIPTIONS ******/
	/* THIS MUST BE DONE AFTER DUPLICATING FOR MULTIPLE HOST NAMES (SEE ABOVE) */
	for(temp_serviceescalation=xodtemplate_serviceescalation_list;temp_serviceescalation!=NULL;temp_serviceescalation=temp_serviceescalation->next){

		/* skip serviceescalations without enough data */
		if(temp_serviceescalation->service_description==NULL || temp_serviceescalation->host_name==NULL)
			continue;

		/* get list of services */
		temp_servicelist=xodtemplate_expand_services(temp_serviceescalation->host_name,temp_serviceescalation->service_description);
		if(temp_servicelist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand services specified in service escalation (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_serviceescalation->_config_file),temp_serviceescalation->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* duplicate service escalation entries */
		first_item=TRUE;
		for(this_servicelist=temp_servicelist;this_servicelist!=NULL;this_servicelist=this_servicelist->next){

			/* if this is the first duplication, use the existing entry */
			if(first_item==TRUE){

				free(temp_serviceescalation->service_description);
				temp_serviceescalation->service_description=strdup(this_servicelist->service_description);
				if(temp_serviceescalation->service_description==NULL){
					xodtemplate_free_servicelist(temp_servicelist);
					return ERROR;
				        }

				first_item=FALSE;
				continue;
			        }

			/* duplicate service escalation definition */
			result=xodtemplate_duplicate_serviceescalation(temp_serviceescalation,temp_serviceescalation->host_name,this_servicelist->service_description);

			/* exit on error */
			if(result==ERROR){
				xodtemplate_free_servicelist(temp_servicelist);
				return ERROR;
		                }
		        }

		/* free memory we used for service list */
		xodtemplate_free_servicelist(temp_servicelist);
	        }


	/****** DUPLICATE HOST DEPENDENCY DEFINITIONS WITH MULTIPLE HOSTGROUP AND/OR HOST NAMES (MASTER AND DEPENDENT) ******/
	for(temp_hostdependency=xodtemplate_hostdependency_list;temp_hostdependency!=NULL;temp_hostdependency=temp_hostdependency->next){
		
		/* skip host dependencies without enough data */
		if(temp_hostdependency->hostgroup_name==NULL && temp_hostdependency->dependent_hostgroup_name==NULL && temp_hostdependency->host_name==NULL && temp_hostdependency->dependent_host_name==NULL)
			continue;

		/* get list of master host names */
		master_hostlist=xodtemplate_expand_hostgroups_and_hosts(temp_hostdependency->hostgroup_name,temp_hostdependency->host_name);
		if(master_hostlist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand master hostgroups and/or hosts specified in host dependency (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_hostdependency->_config_file),temp_hostdependency->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* get list of dependent host names */
		dependent_hostlist=xodtemplate_expand_hostgroups_and_hosts(temp_hostdependency->dependent_hostgroup_name,temp_hostdependency->dependent_host_name);
		if(dependent_hostlist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand dependent hostgroups and/or hosts specified in host dependency (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_hostdependency->_config_file),temp_hostdependency->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* duplicate the dependency definitions */
		first_item=TRUE;
		for(temp_hostlist=master_hostlist;temp_hostlist!=NULL;temp_hostlist=temp_hostlist->next){

			for(this_hostlist=dependent_hostlist;this_hostlist!=NULL;this_hostlist=this_hostlist->next){

				/* existing definition gets first names */
				if(first_item==TRUE){
					free(temp_hostdependency->host_name);
					free(temp_hostdependency->dependent_host_name);
					temp_hostdependency->host_name=strdup(temp_hostlist->host_name);
					temp_hostdependency->dependent_host_name=strdup(this_hostlist->host_name);
					first_item=FALSE;
					continue;
				        }
				else
					result=xodtemplate_duplicate_hostdependency(temp_hostdependency,temp_hostlist->host_name,this_hostlist->host_name);

				/* exit on error */
				if(result==ERROR)
					return ERROR;
			        }
		        }

		/* free memory used to store host lists */
		xodtemplate_free_hostlist(master_hostlist);
		xodtemplate_free_hostlist(dependent_hostlist);
	        }


	/****** DUPLICATE SERVICE DEPENDENCY DEFINITIONS WITH MULTIPLE HOSTGROUP AND/OR HOST NAMES (MASTER AND DEPENDENT) ******/
	for(temp_servicedependency=xodtemplate_servicedependency_list;temp_servicedependency!=NULL;temp_servicedependency=temp_servicedependency->next){

		/* skip service dependencies without enough data */
		if(temp_servicedependency->hostgroup_name==NULL && temp_servicedependency->dependent_hostgroup_name==NULL && temp_servicedependency->host_name==NULL && temp_servicedependency->dependent_host_name==NULL)
			continue;

		/* get list of master host names */
		master_hostlist=xodtemplate_expand_hostgroups_and_hosts(temp_servicedependency->hostgroup_name,temp_servicedependency->host_name);
		if(master_hostlist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand master hostgroups and/or hosts specified in service dependency (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_servicedependency->_config_file),temp_servicedependency->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* get list of dependent host names */
		dependent_hostlist=xodtemplate_expand_hostgroups_and_hosts(temp_servicedependency->dependent_hostgroup_name,temp_servicedependency->dependent_host_name);
		if(dependent_hostlist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand dependent hostgroups and/or hosts specified in service dependency (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_servicedependency->_config_file),temp_servicedependency->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* duplicate the dependency definitions */
		first_item=TRUE;
		for(temp_hostlist=master_hostlist;temp_hostlist!=NULL;temp_hostlist=temp_hostlist->next){

			for(this_hostlist=dependent_hostlist;this_hostlist!=NULL;this_hostlist=this_hostlist->next){

				/* existing definition gets first names */
				if(first_item==TRUE){
					free(temp_servicedependency->host_name);
					free(temp_servicedependency->dependent_host_name);
					temp_servicedependency->host_name=strdup(temp_hostlist->host_name);
					temp_servicedependency->dependent_host_name=strdup(this_hostlist->host_name);
					first_item=FALSE;
					continue;
				        }
				else
					result=xodtemplate_duplicate_servicedependency(temp_servicedependency,temp_hostlist->host_name,temp_servicedependency->service_description,this_hostlist->host_name,temp_servicedependency->dependent_service_description);

				/* exit on error */
				if(result==ERROR)
					return ERROR;
			        }
		        }

		/* free memory used to store host lists */
		xodtemplate_free_hostlist(master_hostlist);
		xodtemplate_free_hostlist(dependent_hostlist);
	        }


	/****** DUPLICATE SERVICE DEPENDENCY DEFINITIONS WITH MULTIPLE MASTER DESCRIPTIONS ******/
	/* THIS MUST BE DONE AFTER DUPLICATING FOR MULTIPLE HOST NAMES (SEE ABOVE) */
	for(temp_servicedependency=xodtemplate_servicedependency_list;temp_servicedependency!=NULL;temp_servicedependency=temp_servicedependency->next){

		/* skip servicedependencies without enough data */
		if(temp_servicedependency->service_description==NULL || temp_servicedependency->host_name==NULL)
			continue;

		/* get list of services */
		temp_servicelist=xodtemplate_expand_services(temp_servicedependency->host_name,temp_servicedependency->service_description);
		if(temp_servicelist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand services specified in service dependency (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_servicedependency->_config_file),temp_servicedependency->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* duplicate service escalation entries */
		first_item=TRUE;
		for(this_servicelist=temp_servicelist;this_servicelist!=NULL;this_servicelist=this_servicelist->next){

			/* if this is the first duplication, use the existing entry */
			if(first_item==TRUE){

				free(temp_servicedependency->service_description);
				temp_servicedependency->service_description=strdup(this_servicelist->service_description);
				if(temp_servicedependency->service_description==NULL){
					xodtemplate_free_servicelist(temp_servicelist);
					return ERROR;
				        }

				first_item=FALSE;
				continue;
			        }

			/* duplicate service escalation definition */
			result=xodtemplate_duplicate_servicedependency(temp_servicedependency,temp_servicedependency->host_name,this_servicelist->service_description,temp_servicedependency->dependent_host_name,temp_servicedependency->dependent_service_description);

			/* exit on error */
			if(result==ERROR){
				xodtemplate_free_servicelist(temp_servicelist);
				return ERROR;
		                }
		        }

		/* free memory we used for service list */
		xodtemplate_free_servicelist(temp_servicelist);
	        }


	/****** DUPLICATE SERVICE DEPENDENCY DEFINITIONS WITH MULTIPLE DEPENDENCY DESCRIPTIONS ******/
	/* THIS MUST BE DONE AFTER DUPLICATING FOR MULTIPLE HOST NAMES (SEE ABOVE) */
	for(temp_servicedependency=xodtemplate_servicedependency_list;temp_servicedependency!=NULL;temp_servicedependency=temp_servicedependency->next){

		/* skip servicedependencies without enough data */
		if(temp_servicedependency->dependent_service_description==NULL || temp_servicedependency->dependent_host_name==NULL)
			continue;

		/* get list of services */
		temp_servicelist=xodtemplate_expand_services(temp_servicedependency->dependent_host_name,temp_servicedependency->dependent_service_description);
		if(temp_servicelist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand services specified in service dependency (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_servicedependency->_config_file),temp_servicedependency->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* duplicate service escalation entries */
		first_item=TRUE;
		for(this_servicelist=temp_servicelist;this_servicelist!=NULL;this_servicelist=this_servicelist->next){

			/* if this is the first duplication, use the existing entry */
			if(first_item==TRUE){

				free(temp_servicedependency->dependent_service_description);
				temp_servicedependency->dependent_service_description=strdup(this_servicelist->service_description);
				if(temp_servicedependency->dependent_service_description==NULL){
					xodtemplate_free_servicelist(temp_servicelist);
					return ERROR;
				        }

				first_item=FALSE;
				continue;
			        }

			/* duplicate service escalation definition */
			result=xodtemplate_duplicate_servicedependency(temp_servicedependency,temp_servicedependency->host_name,temp_servicedependency->service_description,temp_servicedependency->dependent_host_name,this_servicelist->service_description);

			/* exit on error */
			if(result==ERROR){
				xodtemplate_free_servicelist(temp_servicelist);
				return ERROR;
		                }
		        }

		/* free memory we used for service list */
		xodtemplate_free_servicelist(temp_servicelist);
	        }


	/****** DUPLICATE HOSTEXTINFO DEFINITIONS WITH ONE OR MORE HOSTGROUP AND/OR HOST NAMES ******/
	for(temp_hostextinfo=xodtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){

		/* skip definitions without enough data */
		if(temp_hostextinfo->hostgroup_name==NULL && temp_hostextinfo->host_name==NULL)
			continue;

		/* get list of hosts */
		temp_hostlist=xodtemplate_expand_hostgroups_and_hosts(temp_hostextinfo->hostgroup_name,temp_hostextinfo->host_name);
		if(temp_hostlist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand hostgroups and/or hosts specified in extended host info (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_service->_config_file),temp_service->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* add a copy of the definition for every host in the hostgroup/host name list */
		first_item=TRUE;
		for(this_hostlist=temp_hostlist;this_hostlist!=NULL;this_hostlist=this_hostlist->next){

			/* if this is the first duplication, use the existing entry */
			if(first_item==TRUE){

				free(temp_hostextinfo->host_name);
				temp_hostextinfo->host_name=strdup(this_hostlist->host_name);
				if(temp_hostextinfo->host_name==NULL){
					xodtemplate_free_hostlist(temp_hostlist);
					return ERROR;
				        }
				first_item=FALSE;
				continue;
			        }

			/* duplicate hostextinfo definition */
			result=xodtemplate_duplicate_hostextinfo(temp_hostextinfo,this_hostlist->host_name);

			/* exit on error */
			if(result==ERROR){
				xodtemplate_free_hostlist(temp_hostlist);
				return ERROR;
			        }
		        }

		/* free memory we used for host list */
		xodtemplate_free_hostlist(temp_hostlist);
	        }


	/****** DUPLICATE SERVICEEXTINFO DEFINITIONS WITH ONE OR MORE HOSTGROUP AND/OR HOST NAMES ******/
	for(temp_serviceextinfo=xodtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){

		/* skip definitions without enough data */
		if(temp_serviceextinfo->hostgroup_name==NULL && temp_serviceextinfo->host_name==NULL)
			continue;

		/* get list of hosts */
		temp_hostlist=xodtemplate_expand_hostgroups_and_hosts(temp_serviceextinfo->hostgroup_name,temp_serviceextinfo->host_name);
		if(temp_hostlist==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand hostgroups and/or hosts specified in extended service info (config file '%s', line %d)\n",xodtemplate_config_file_name(temp_service->_config_file),temp_service->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }

		/* add a copy of the definition for every host in the hostgroup/host name list */
		first_item=TRUE;
		for(this_hostlist=temp_hostlist;this_hostlist!=NULL;this_hostlist=this_hostlist->next){

			/* existing definition gets first host name */
			if(first_item==TRUE){
				free(temp_serviceextinfo->host_name);
				temp_serviceextinfo->host_name=strdup(this_hostlist->host_name);
				if(temp_serviceextinfo->host_name==NULL){
					xodtemplate_free_hostlist(temp_hostlist);
					return ERROR;
				        }
				first_item=FALSE;
				continue;
			        }

			/* duplicate serviceextinfo definition */
			result=xodtemplate_duplicate_serviceextinfo(temp_serviceextinfo,this_hostlist->host_name);

			/* exit on error */
			if(result==ERROR){
				xodtemplate_free_hostlist(temp_hostlist);
				return ERROR;
			        }
		        }

		/* free memory we used for host list */
		xodtemplate_free_hostlist(temp_hostlist);
	        }

#ifdef DEBUG0
	printf("xodtemplate_duplicate_objects() end\n");
#endif

	return OK;
        }



/* duplicates a service definition (with a new host name) */
int xodtemplate_duplicate_service(xodtemplate_service *temp_service, char *host_name){
	xodtemplate_service *new_service;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_service() start\n");
#endif

	/* allocate memory for a new service definition */
	new_service=(xodtemplate_service *)malloc(sizeof(xodtemplate_service));
	if(new_service==NULL){
#ifdef DEBUG1
		printf("Error: Could not allocate memory for duplicate definition of service.\n");
#endif
		return ERROR;
	        }

	/* defaults */
	new_service->template=NULL;
	new_service->name=NULL;
	new_service->hostgroup_name=NULL;
	new_service->host_name=NULL;
	new_service->service_description=NULL;
	new_service->check_command=NULL;
	new_service->check_period=NULL;
	new_service->event_handler=NULL;
	new_service->notification_period=NULL;
	new_service->contact_groups=NULL;
	new_service->failure_prediction_options=NULL;

	/* make sure hostgroup member in new service definition is NULL */
	new_service->hostgroup_name=NULL;

	new_service->max_check_attempts=temp_service->max_check_attempts;
	new_service->have_max_check_attempts=temp_service->have_max_check_attempts;
	new_service->normal_check_interval=temp_service->normal_check_interval;
	new_service->have_normal_check_interval=temp_service->have_normal_check_interval;
	new_service->retry_check_interval=temp_service->retry_check_interval;
	new_service->have_retry_check_interval=temp_service->have_retry_check_interval;
	new_service->active_checks_enabled=temp_service->active_checks_enabled;
	new_service->have_active_checks_enabled=temp_service->have_active_checks_enabled;
	new_service->passive_checks_enabled=temp_service->passive_checks_enabled;
	new_service->have_passive_checks_enabled=temp_service->have_passive_checks_enabled;
	new_service->parallelize_check=temp_service->parallelize_check;
	new_service->have_parallelize_check=temp_service->have_parallelize_check;
	new_service->is_volatile=temp_service->is_volatile;
	new_service->have_is_volatile=temp_service->have_is_volatile;
	new_service->obsess_over_service=temp_service->obsess_over_service;
	new_service->have_obsess_over_service=temp_service->have_obsess_over_service;
	new_service->event_handler_enabled=temp_service->event_handler_enabled;
	new_service->have_event_handler_enabled=temp_service->have_event_handler_enabled;
	new_service->check_freshness=temp_service->check_freshness;
	new_service->have_check_freshness=temp_service->have_check_freshness;
	new_service->freshness_threshold=temp_service->freshness_threshold;
	new_service->have_freshness_threshold=temp_service->have_freshness_threshold;
	new_service->flap_detection_enabled=temp_service->flap_detection_enabled;
	new_service->have_flap_detection_enabled=temp_service->have_flap_detection_enabled;
	new_service->low_flap_threshold=temp_service->low_flap_threshold;
	new_service->have_low_flap_threshold=temp_service->have_low_flap_threshold;
	new_service->high_flap_threshold=temp_service->high_flap_threshold;
	new_service->have_high_flap_threshold=temp_service->have_high_flap_threshold;
	new_service->notify_on_unknown=temp_service->notify_on_unknown;
	new_service->notify_on_warning=temp_service->notify_on_warning;
	new_service->notify_on_critical=temp_service->notify_on_critical;
	new_service->notify_on_recovery=temp_service->notify_on_recovery;
	new_service->have_notification_options=temp_service->have_notification_options;
	new_service->notifications_enabled=temp_service->notifications_enabled;
	new_service->have_notifications_enabled=temp_service->have_notifications_enabled;
	new_service->notification_interval=temp_service->notification_interval;
	new_service->have_notification_interval=temp_service->have_notification_interval;
	new_service->stalk_on_ok=temp_service->stalk_on_ok;
	new_service->stalk_on_unknown=temp_service->stalk_on_unknown;
	new_service->stalk_on_warning=temp_service->stalk_on_warning;
	new_service->stalk_on_critical=temp_service->stalk_on_critical;
	new_service->have_stalking_options=temp_service->have_stalking_options;
	new_service->process_perf_data=temp_service->process_perf_data;
	new_service->have_process_perf_data=temp_service->have_process_perf_data;
	new_service->failure_prediction_enabled=temp_service->failure_prediction_enabled;
	new_service->have_failure_prediction_enabled=temp_service->have_failure_prediction_enabled;
	new_service->retain_status_information=temp_service->retain_status_information;
	new_service->have_retain_status_information=temp_service->have_retain_status_information;
	new_service->retain_nonstatus_information=temp_service->retain_nonstatus_information;
	new_service->have_retain_nonstatus_information=temp_service->have_retain_nonstatus_information;

	new_service->has_been_resolved=temp_service->has_been_resolved;
	new_service->register_object=temp_service->register_object;
	new_service->_config_file=temp_service->_config_file;
	new_service->_start_line=temp_service->_start_line;

	/* allocate memory for and copy string members of service definition (host name provided, DO NOT duplicate hostgroup member!)*/
	if(temp_service->host_name!=NULL){
		new_service->host_name=strdup(host_name);
		if(new_service->host_name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service.\n");
#endif
			free(new_service);
			return ERROR;
		        }
	        } 
	if(temp_service->template!=NULL){
		new_service->template=strdup(temp_service->template);
		if(new_service->template==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service.\n");
#endif
			free(new_service->host_name);
			free(new_service);
			return ERROR;
		        }
	        } 
	if(temp_service->name!=NULL){
		new_service->name=strdup(temp_service->name);
		if(new_service->name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service.\n");
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service);
			return ERROR;
		        }
	        } 
	if(temp_service->service_description!=NULL){
		new_service->service_description=strdup(temp_service->service_description);
		if(new_service->service_description==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service.\n");
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service);
			return ERROR;
		        }
	        } 
	if(temp_service->check_command!=NULL){
		new_service->check_command=strdup(temp_service->check_command);
		if(new_service->check_command==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service.\n");
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service);
			return ERROR;
		        }
	        } 
	if(temp_service->check_period!=NULL){
		new_service->check_period=strdup(temp_service->check_period);
		if(new_service->check_period==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service.\n");
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service->check_command);
			free(new_service);
			return ERROR;
		        }
	        } 
	if(temp_service->event_handler!=NULL){
		new_service->event_handler=strdup(temp_service->event_handler);
		if(new_service->event_handler==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service.\n");
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service->check_command);
			free(new_service->check_period);
			free(new_service);
			return ERROR;
		        }
	        } 
	if(temp_service->notification_period!=NULL){
		new_service->notification_period=strdup(temp_service->notification_period);
		if(new_service->notification_period==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service.\n");
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service->check_command);
			free(new_service->check_period);
			free(new_service->event_handler);
			free(new_service);
			return ERROR;
		        }
	        } 
	if(temp_service->contact_groups!=NULL){
		new_service->contact_groups=strdup(temp_service->contact_groups);
		if(new_service->contact_groups==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service.\n");
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service->check_command);
			free(new_service->check_period);
			free(new_service->event_handler);
			free(new_service->notification_period);
			free(new_service);
			return ERROR;
		        }
	        } 
	if(temp_service->failure_prediction_options!=NULL){
		new_service->failure_prediction_options=strdup(temp_service->failure_prediction_options);
		if(new_service->failure_prediction_options==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service.\n");
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service->check_command);
			free(new_service->check_period);
			free(new_service->event_handler);
			free(new_service->notification_period);
			free(new_service->contact_groups);
			free(new_service);
			return ERROR;
		        }
	        } 

	/* add new service to service list */
	xodtemplate_add_service_allocated(new_service);

#ifdef DEBUG0
	printf("xodtemplate_duplicate_service() end\n");
#endif

	return OK;
        }




/* duplicates a host escalation definition (with a new host name) */
int xodtemplate_duplicate_hostescalation(xodtemplate_hostescalation *temp_hostescalation, char *host_name){
	xodtemplate_hostescalation *new_hostescalation;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_hostescalation() start\n");
#endif

	/* allocate memory for a new host escalation definition */
	new_hostescalation=(xodtemplate_hostescalation *)malloc(sizeof(xodtemplate_hostescalation));
	if(new_hostescalation==NULL){
#ifdef DEBUG1
		printf("Error: Could not allocate memory for duplicate definition of host escalation.\n");
#endif
		return ERROR;
	        }

	/* defaults */
	new_hostescalation->template=NULL;
	new_hostescalation->name=NULL;
	new_hostescalation->hostgroup_name=NULL;
	new_hostescalation->host_name=NULL;
	new_hostescalation->contact_groups=NULL;

	new_hostescalation->has_been_resolved=temp_hostescalation->has_been_resolved;
	new_hostescalation->register_object=temp_hostescalation->register_object;
	new_hostescalation->_config_file=temp_hostescalation->_config_file;
	new_hostescalation->_start_line=temp_hostescalation->_start_line;

	new_hostescalation->first_notification=temp_hostescalation->first_notification;
	new_hostescalation->last_notification=temp_hostescalation->last_notification;
	new_hostescalation->have_first_notification=temp_hostescalation->have_first_notification;
	new_hostescalation->have_last_notification=temp_hostescalation->have_last_notification;
	new_hostescalation->notification_interval=temp_hostescalation->notification_interval;
	new_hostescalation->have_notification_interval=temp_hostescalation->have_notification_interval;
	

	/* allocate memory for and copy string members of hostescalation definition */
	if(temp_hostescalation->host_name!=NULL){
		new_hostescalation->host_name=strdup(host_name);
		if(new_hostescalation->host_name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of host escalation.\n");
#endif
			free(new_hostescalation);
			return ERROR;
		        }
	        } 
	if(temp_hostescalation->template!=NULL){
		new_hostescalation->template=strdup(temp_hostescalation->template);
		if(new_hostescalation->template==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of host escalation.\n");
#endif
			free(new_hostescalation->host_name);
			free(new_hostescalation);
			return ERROR;
		        }
	        } 
	if(temp_hostescalation->name!=NULL){
		new_hostescalation->name=strdup(temp_hostescalation->name);
		if(new_hostescalation->name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of host escalation.\n");
#endif
			free(new_hostescalation->host_name);
			free(new_hostescalation->template);
			free(new_hostescalation);
			return ERROR;
		        }
	        } 
	if(temp_hostescalation->contact_groups!=NULL){
		new_hostescalation->contact_groups=strdup(temp_hostescalation->contact_groups);
		if(new_hostescalation->contact_groups==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of host escalation.\n");
#endif
			free(new_hostescalation->host_name);
			free(new_hostescalation->template);
			free(new_hostescalation->name);
			free(new_hostescalation);
			return ERROR;
		        }
	        } 

	/* add new hostescalation to head of list in memory */
	new_hostescalation->next=xodtemplate_hostescalation_list;
	xodtemplate_hostescalation_list=new_hostescalation;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_hostescalation() end\n");
#endif

	return OK;
        }



/* duplicates a service escalation definition (with a new host name and/or service description) */
int xodtemplate_duplicate_serviceescalation(xodtemplate_serviceescalation *temp_serviceescalation, char *host_name, char *svc_description){
	xodtemplate_serviceescalation *new_serviceescalation;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_serviceescalation() start\n");
#endif

	/* allocate memory for a new service escalation definition */
	new_serviceescalation=(xodtemplate_serviceescalation *)malloc(sizeof(xodtemplate_serviceescalation));
	if(new_serviceescalation==NULL){
#ifdef DEBUG1
		printf("Error: Could not allocate memory for duplicate definition of service escalation.\n");
#endif
		return ERROR;
	        }

	/* defaults */
	new_serviceescalation->template=NULL;
	new_serviceescalation->name=NULL;
	new_serviceescalation->hostgroup_name=NULL;
	new_serviceescalation->host_name=NULL;
	new_serviceescalation->service_description=NULL;
	new_serviceescalation->contact_groups=NULL;

	new_serviceescalation->has_been_resolved=temp_serviceescalation->has_been_resolved;
	new_serviceescalation->register_object=temp_serviceescalation->register_object;
	new_serviceescalation->_config_file=temp_serviceescalation->_config_file;
	new_serviceescalation->_start_line=temp_serviceescalation->_start_line;

	new_serviceescalation->first_notification=temp_serviceescalation->first_notification;
	new_serviceescalation->last_notification=temp_serviceescalation->last_notification;
	new_serviceescalation->have_first_notification=temp_serviceescalation->have_first_notification;
	new_serviceescalation->have_last_notification=temp_serviceescalation->have_last_notification;
	new_serviceescalation->notification_interval=temp_serviceescalation->notification_interval;
	new_serviceescalation->have_notification_interval=temp_serviceescalation->have_notification_interval;
	

	/* allocate memory for and copy string members of serviceescalation definition */
	if(host_name!=NULL){
		new_serviceescalation->host_name=strdup(host_name);
		if(new_serviceescalation->host_name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service escalation.\n");
#endif
			free(new_serviceescalation);
			return ERROR;
		        }
	        } 
	else if(temp_serviceescalation->host_name!=NULL){
		new_serviceescalation->host_name=strdup(temp_serviceescalation->host_name);
		if(new_serviceescalation->host_name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service escalation.\n");
#endif
			free(new_serviceescalation);
			return ERROR;
		        }
	        } 
	if(temp_serviceescalation->template!=NULL){
		new_serviceescalation->template=strdup(temp_serviceescalation->template);
		if(new_serviceescalation->template==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service escalation.\n");
#endif
			free(new_serviceescalation->host_name);
			free(new_serviceescalation);
			return ERROR;
		        }
	        } 
	if(temp_serviceescalation->name!=NULL){
		new_serviceescalation->name=strdup(temp_serviceescalation->name);
		if(new_serviceescalation->name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service escalation.\n");
#endif
			free(new_serviceescalation->host_name);
			free(new_serviceescalation->template);
			free(new_serviceescalation);
			return ERROR;
		        }
	        } 
	if(svc_description!=NULL){
		new_serviceescalation->service_description=strdup(svc_description);
		if(new_serviceescalation->service_description==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service escalation.\n");
#endif
			free(new_serviceescalation->host_name);
			free(new_serviceescalation->template);
			free(new_serviceescalation->name);
			free(new_serviceescalation);
			return ERROR;
		        }
	        } 
	else if(temp_serviceescalation->service_description!=NULL){
		new_serviceescalation->service_description=strdup(temp_serviceescalation->service_description);
		if(new_serviceescalation->service_description==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service escalation.\n");
#endif
			free(new_serviceescalation->host_name);
			free(new_serviceescalation->template);
			free(new_serviceescalation->name);
			free(new_serviceescalation);
			return ERROR;
		        }
	        } 
	if(temp_serviceescalation->contact_groups!=NULL){
		new_serviceescalation->contact_groups=strdup(temp_serviceescalation->contact_groups);
		if(new_serviceescalation->contact_groups==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service escalation.\n");
#endif
			free(new_serviceescalation->service_description);
			free(new_serviceescalation->host_name);
			free(new_serviceescalation->template);
			free(new_serviceescalation->name);
			free(new_serviceescalation);
			return ERROR;
		        }
	        } 

	/* add new serviceescalation to head of list in memory */
	new_serviceescalation->next=xodtemplate_serviceescalation_list;
	xodtemplate_serviceescalation_list=new_serviceescalation;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_serviceescalation() end\n");
#endif

	return OK;
        }



/* duplicates a host dependency definition (with master and dependent host names) */
int xodtemplate_duplicate_hostdependency(xodtemplate_hostdependency *temp_hostdependency, char *master_host_name, char *dependent_host_name){
	xodtemplate_hostdependency *new_hostdependency;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_hostdependency() start\n");
#endif

	/* allocate memory for a new host dependency definition */
	new_hostdependency=(xodtemplate_hostdependency *)malloc(sizeof(xodtemplate_hostdependency));
	if(new_hostdependency==NULL){
#ifdef DEBUG1
		printf("Error: Could not allocate memory for duplicate definition of host dependency.\n");
#endif
		return ERROR;
	        }

	/* defaults */
	new_hostdependency->template=NULL;
	new_hostdependency->name=NULL;
	new_hostdependency->hostgroup_name=NULL;
	new_hostdependency->dependent_hostgroup_name=NULL;
	new_hostdependency->host_name=NULL;
	new_hostdependency->dependent_host_name=NULL;

	new_hostdependency->has_been_resolved=temp_hostdependency->has_been_resolved;
	new_hostdependency->register_object=temp_hostdependency->register_object;
	new_hostdependency->_config_file=temp_hostdependency->_config_file;
	new_hostdependency->_start_line=temp_hostdependency->_start_line;

	new_hostdependency->fail_notify_on_up=temp_hostdependency->fail_notify_on_up;
	new_hostdependency->fail_notify_on_down=temp_hostdependency->fail_notify_on_down;
	new_hostdependency->fail_notify_on_unreachable=temp_hostdependency->fail_notify_on_unreachable;
	new_hostdependency->have_notification_dependency_options=temp_hostdependency->have_notification_dependency_options;
	

	/* allocate memory for and copy string members of hostdependency definition */
	if(temp_hostdependency->host_name!=NULL){
		new_hostdependency->host_name=strdup(master_host_name);
		if(new_hostdependency->host_name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of host dependency.\n");
#endif
			free(new_hostdependency);
			return ERROR;
		        }
	        } 
       	if(temp_hostdependency->dependent_host_name!=NULL){
		new_hostdependency->dependent_host_name=strdup(dependent_host_name);
		if(new_hostdependency->dependent_host_name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of host dependency.\n");
#endif
			free(new_hostdependency);
			return ERROR;
		        }
	        } 
	if(temp_hostdependency->template!=NULL){
		new_hostdependency->template=strdup(temp_hostdependency->template);
		if(new_hostdependency->template==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of host dependency.\n");
#endif
			free(new_hostdependency->host_name);
			free(new_hostdependency);
			return ERROR;
		        }
	        } 
	if(temp_hostdependency->name!=NULL){
		new_hostdependency->name=strdup(temp_hostdependency->name);
		if(new_hostdependency->name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of host dependency.\n");
#endif
			free(new_hostdependency->host_name);
			free(new_hostdependency->template);
			free(new_hostdependency);
			return ERROR;
		        }
	        } 

	/* add new hostdependency to head of list in memory */
	new_hostdependency->next=xodtemplate_hostdependency_list;
	xodtemplate_hostdependency_list=new_hostdependency;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_hostdependency() end\n");
#endif

	return OK;
        }



/* duplicates a service dependency definition */
int xodtemplate_duplicate_servicedependency(xodtemplate_servicedependency *temp_servicedependency, char *master_host_name, char *master_service_description, char *dependent_host_name, char *dependent_service_description){
	xodtemplate_servicedependency *new_servicedependency;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_servicedependency() start\n");
#endif

	/* allocate memory for a new service dependency definition */
	new_servicedependency=(xodtemplate_servicedependency *)malloc(sizeof(xodtemplate_servicedependency));
	if(new_servicedependency==NULL){
#ifdef DEBUG1
		printf("Error: Could not allocate memory for duplicate definition of service dependency.\n");
#endif
		return ERROR;
	        }

	/* defaults */
	new_servicedependency->template=NULL;
	new_servicedependency->name=NULL;
	new_servicedependency->hostgroup_name=NULL;
	new_servicedependency->dependent_hostgroup_name=NULL;
	new_servicedependency->host_name=NULL;
	new_servicedependency->dependent_host_name=NULL;

	new_servicedependency->has_been_resolved=temp_servicedependency->has_been_resolved;
	new_servicedependency->register_object=temp_servicedependency->register_object;
	new_servicedependency->_config_file=temp_servicedependency->_config_file;
	new_servicedependency->_start_line=temp_servicedependency->_start_line;

	new_servicedependency->fail_notify_on_ok=temp_servicedependency->fail_notify_on_ok;
	new_servicedependency->fail_notify_on_unknown=temp_servicedependency->fail_notify_on_unknown;
	new_servicedependency->fail_notify_on_warning=temp_servicedependency->fail_notify_on_warning;
	new_servicedependency->fail_notify_on_critical=temp_servicedependency->fail_notify_on_critical;
	new_servicedependency->have_notification_dependency_options=temp_servicedependency->have_notification_dependency_options;
	new_servicedependency->fail_execute_on_ok=temp_servicedependency->fail_execute_on_ok;
	new_servicedependency->fail_execute_on_unknown=temp_servicedependency->fail_execute_on_unknown;
	new_servicedependency->fail_execute_on_warning=temp_servicedependency->fail_execute_on_warning;
	new_servicedependency->fail_execute_on_critical=temp_servicedependency->fail_execute_on_critical;
	new_servicedependency->have_execution_dependency_options=temp_servicedependency->have_execution_dependency_options;
	

	/* allocate memory for and copy string members of hostdependency definition */
	if(temp_servicedependency->host_name!=NULL){
		new_servicedependency->host_name=strdup(master_host_name);
		if(new_servicedependency->host_name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service dependency.\n");
#endif
			free(new_servicedependency);
			return ERROR;
		        }
	        } 
	if(temp_servicedependency->service_description!=NULL){
		new_servicedependency->service_description=strdup(master_service_description);
		if(new_servicedependency->service_description==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service dependency.\n");
#endif
			free(new_servicedependency);
			return ERROR;
		        }
	        } 
       	if(temp_servicedependency->dependent_host_name!=NULL){
		new_servicedependency->dependent_host_name=strdup(dependent_host_name);
		if(new_servicedependency->dependent_host_name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service dependency.\n");
#endif
			free(new_servicedependency);
			return ERROR;
		        }
	        } 
	if(temp_servicedependency->dependent_service_description!=NULL){
		new_servicedependency->dependent_service_description=strdup(dependent_service_description);
		if(new_servicedependency->dependent_service_description==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service dependency.\n");
#endif
			free(new_servicedependency);
			return ERROR;
		        }
	        } 
	if(temp_servicedependency->template!=NULL){
		new_servicedependency->template=strdup(temp_servicedependency->template);
		if(new_servicedependency->template==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service dependency.\n");
#endif
			free(new_servicedependency->host_name);
			free(new_servicedependency);
			return ERROR;
		        }
	        } 
	if(temp_servicedependency->name!=NULL){
		new_servicedependency->name=strdup(temp_servicedependency->name);
		if(new_servicedependency->name==NULL){
#ifdef DEBUG1
			printf("Error: Could not allocate memory for duplicate definition of service dependency.\n");
#endif
			free(new_servicedependency->host_name);
			free(new_servicedependency->template);
			free(new_servicedependency);
			return ERROR;
		        }
	        } 

	/* add new servicedependency to head of list in memory */
	new_servicedependency->next=xodtemplate_servicedependency_list;
	xodtemplate_servicedependency_list=new_servicedependency;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_servicedependency() end\n");
#endif

	return OK;
        }



/* duplicates a hostextinfo object definition */
int xodtemplate_duplicate_hostextinfo(xodtemplate_hostextinfo *this_hostextinfo, char *host_name){
	xodtemplate_hostextinfo *new_hostextinfo;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_hostextinfo() start\n");
#endif

	new_hostextinfo=(xodtemplate_hostextinfo *)malloc(sizeof(xodtemplate_hostextinfo));
	if(new_hostextinfo==NULL){
#ifdef DEBUG1
		printf("Error: Could not allocate memory for duplicate definition of extended host info.\n");
#endif
		return ERROR;
	        }

	new_hostextinfo->template=NULL;
	new_hostextinfo->name=NULL;
	new_hostextinfo->host_name=NULL;
	new_hostextinfo->hostgroup_name=NULL;
	new_hostextinfo->notes_url=NULL;
	new_hostextinfo->icon_image=NULL;
	new_hostextinfo->icon_image_alt=NULL;
	new_hostextinfo->vrml_image=NULL;
	new_hostextinfo->statusmap_image=NULL;

	/* duplicate strings (host_name member is passed in) */
	if(host_name!=NULL)
		new_hostextinfo->host_name=strdup(host_name);
	if(this_hostextinfo->template!=NULL)
		new_hostextinfo->template=strdup(this_hostextinfo->template);
	if(this_hostextinfo->name!=NULL)
		new_hostextinfo->name=strdup(this_hostextinfo->name);
	if(this_hostextinfo->notes_url!=NULL)
		new_hostextinfo->notes_url=strdup(this_hostextinfo->notes_url);
	if(this_hostextinfo->icon_image!=NULL)
		new_hostextinfo->icon_image=strdup(this_hostextinfo->icon_image);
	if(this_hostextinfo->icon_image_alt!=NULL)
		new_hostextinfo->icon_image_alt=strdup(this_hostextinfo->icon_image_alt);
	if(this_hostextinfo->vrml_image!=NULL)
		new_hostextinfo->vrml_image=strdup(this_hostextinfo->vrml_image);
	if(this_hostextinfo->statusmap_image!=NULL)
		new_hostextinfo->statusmap_image=strdup(this_hostextinfo->statusmap_image);

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
	new_hostextinfo->next=xodtemplate_hostextinfo_list;
	xodtemplate_hostextinfo_list=new_hostextinfo;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_hostextinfo() end\n");
#endif

	return;
        }



/* duplicates a serviceextinfo object definition */
int xodtemplate_duplicate_serviceextinfo(xodtemplate_serviceextinfo *this_serviceextinfo, char *host_name){
	xodtemplate_serviceextinfo *new_serviceextinfo;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_serviceextinfo() start\n");
#endif

	new_serviceextinfo=(xodtemplate_serviceextinfo *)malloc(sizeof(xodtemplate_serviceextinfo));
	if(new_serviceextinfo==NULL){
#ifdef DEBUG1
		printf("Error: Could not allocate memory for duplicate definition of extended service info.\n");
#endif
		return ERROR;
	        }

	new_serviceextinfo->template=NULL;
	new_serviceextinfo->name=NULL;
	new_serviceextinfo->host_name=NULL;
	new_serviceextinfo->hostgroup_name=NULL;
	new_serviceextinfo->notes_url=NULL;
	new_serviceextinfo->icon_image=NULL;
	new_serviceextinfo->icon_image_alt=NULL;

	new_serviceextinfo->has_been_resolved=this_serviceextinfo->has_been_resolved;
	new_serviceextinfo->register_object=this_serviceextinfo->register_object;

	/* duplicate strings (host_name member is passed in) */
	if(host_name!=NULL)
		new_serviceextinfo->host_name=strdup(host_name);
	if(this_serviceextinfo->template!=NULL)
		new_serviceextinfo->template=strdup(this_serviceextinfo->template);
	if(this_serviceextinfo->name!=NULL)
		new_serviceextinfo->name=strdup(this_serviceextinfo->name);
	if(this_serviceextinfo->service_description!=NULL)
		new_serviceextinfo->service_description=strdup(this_serviceextinfo->service_description);
	if(this_serviceextinfo->notes_url!=NULL)
		new_serviceextinfo->notes_url=strdup(this_serviceextinfo->notes_url);
	if(this_serviceextinfo->icon_image!=NULL)
		new_serviceextinfo->icon_image=strdup(this_serviceextinfo->icon_image);
	if(this_serviceextinfo->icon_image_alt!=NULL)
		new_serviceextinfo->icon_image_alt=strdup(this_serviceextinfo->icon_image_alt);

	/* add new object to head of list */
	new_serviceextinfo->next=xodtemplate_serviceextinfo_list;
	xodtemplate_serviceextinfo_list=new_serviceextinfo;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_serviceextinfo() end\n");
#endif

	return;
        }




/******************************************************************/
/***************** OBJECT RESOLUTION FUNCTIONS ********************/
/******************************************************************/

/* resolves object definitions */
int xodtemplate_resolve_objects(void){
	xodtemplate_timeperiod *temp_timeperiod;
	xodtemplate_command *temp_command;
	xodtemplate_contactgroup *temp_contactgroup;
	xodtemplate_hostgroup *temp_hostgroup;
	xodtemplate_servicedependency *temp_servicedependency;
	xodtemplate_serviceescalation *temp_serviceescalation;
	xodtemplate_contact *temp_contact;
	xodtemplate_host *temp_host;
	xodtemplate_service *temp_service;
	xodtemplate_hostdependency *temp_hostdependency;
	xodtemplate_hostescalation *temp_hostescalation;
	xodtemplate_hostextinfo *temp_hostextinfo;
	xodtemplate_serviceextinfo *temp_serviceextinfo;
	void *xod_svc_cursor;

#ifdef DEBUG0
	printf("xodtemplate_resolve_objects() start\n");
#endif

	/* resolve all timeperiod objects */
	for(temp_timeperiod=xodtemplate_timeperiod_list;temp_timeperiod!=NULL;temp_timeperiod=temp_timeperiod->next){
		if(xodtemplate_resolve_timeperiod(temp_timeperiod)==ERROR)
			return ERROR;
	        }

	/* resolve all command objects */
	for(temp_command=xodtemplate_command_list;temp_command!=NULL;temp_command=temp_command->next){
		if(xodtemplate_resolve_command(temp_command)==ERROR)
			return ERROR;
	        }

	/* resolve all contactgroup objects */
	for(temp_contactgroup=xodtemplate_contactgroup_list;temp_contactgroup!=NULL;temp_contactgroup=temp_contactgroup->next){
		if(xodtemplate_resolve_contactgroup(temp_contactgroup)==ERROR)
			return ERROR;
	        }

	/* resolve all hostgroup objects */
	for(temp_hostgroup=xodtemplate_hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
		if(xodtemplate_resolve_hostgroup(temp_hostgroup)==ERROR)
			return ERROR;
	        }

	/* resolve all servicedependency objects */
	for(temp_servicedependency=xodtemplate_servicedependency_list;temp_servicedependency!=NULL;temp_servicedependency=temp_servicedependency->next){
		if(xodtemplate_resolve_servicedependency(temp_servicedependency)==ERROR)
			return ERROR;
	        }

	/* resolve all serviceescalation objects */
	for(temp_serviceescalation=xodtemplate_serviceescalation_list;temp_serviceescalation!=NULL;temp_serviceescalation=temp_serviceescalation->next){
		if(xodtemplate_resolve_serviceescalation(temp_serviceescalation)==ERROR)
			return ERROR;
	        }

	/* resolve all contact objects */
	for(temp_contact=xodtemplate_contact_list;temp_contact!=NULL;temp_contact=temp_contact->next){
		if(xodtemplate_resolve_contact(temp_contact)==ERROR)
			return ERROR;
	        }

	/* resolve all host objects */
	for(temp_host=xodtemplate_host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(xodtemplate_resolve_host(temp_host)==ERROR)
			return ERROR;
	        }

	/* resolve all service objects */
	xod_svc_cursor=get_xodtemplate_service_cursor();
	while(temp_service=get_next_xodtemplate_service(xod_svc_cursor)){
		if(xodtemplate_resolve_service(temp_service)==ERROR)
			return ERROR;
	        }
	free_xodtemplate_service_cursor(xod_svc_cursor);

	/* resolve all hostdependency objects */
	for(temp_hostdependency=xodtemplate_hostdependency_list;temp_hostdependency!=NULL;temp_hostdependency=temp_hostdependency->next){
		if(xodtemplate_resolve_hostdependency(temp_hostdependency)==ERROR)
			return ERROR;
	        }

	/* resolve all hostescalation objects */
	for(temp_hostescalation=xodtemplate_hostescalation_list;temp_hostescalation!=NULL;temp_hostescalation=temp_hostescalation->next){
		if(xodtemplate_resolve_hostescalation(temp_hostescalation)==ERROR)
			return ERROR;
	        }

	/* resolve all hostextinfo objects */
	for(temp_hostextinfo=xodtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){
		if(xodtemplate_resolve_hostextinfo(temp_hostextinfo)==ERROR)
			return ERROR;
	        }

	/* resolve all serviceextinfo objects */
	for(temp_serviceextinfo=xodtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){
		if(xodtemplate_resolve_serviceextinfo(temp_serviceextinfo)==ERROR)
			return ERROR;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_objects() end\n");
#endif

	return OK;
        }



/* resolves a timperiod object */
int xodtemplate_resolve_timeperiod(xodtemplate_timeperiod *this_timeperiod){
	xodtemplate_timeperiod *template_timeperiod;
	int x;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_timperiod() start\n");
#endif

	/* return if this timperiod has already been resolved */
	if(this_timeperiod->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_timeperiod->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_timeperiod->template==NULL)
		return OK;

	template_timeperiod=xodtemplate_find_timeperiod(this_timeperiod->template);
	if(template_timeperiod==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in timeperiod definition could not be not found (config file '%s', line %d)\n",this_timeperiod->template,xodtemplate_config_file_name(this_timeperiod->_config_file),this_timeperiod->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template timeperiod... */
	xodtemplate_resolve_timeperiod(template_timeperiod);

	/* apply missing properties from template timeperiod... */
	if(this_timeperiod->timeperiod_name==NULL && template_timeperiod->timeperiod_name!=NULL)
		this_timeperiod->timeperiod_name=strdup(template_timeperiod->timeperiod_name);
	if(this_timeperiod->alias==NULL && template_timeperiod->alias!=NULL)
		this_timeperiod->alias=strdup(template_timeperiod->alias);
	for(x=0;x<7;x++){
		if(this_timeperiod->timeranges[x]==NULL && template_timeperiod->timeranges[x]!=NULL){
			this_timeperiod->timeranges[x]=strdup(template_timeperiod->timeranges[x]);
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_timeperiod() end\n");
#endif

	return OK;
        }




/* resolves a command object */
int xodtemplate_resolve_command(xodtemplate_command *this_command){
	xodtemplate_command *template_command;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_command() start\n");
#endif

	/* return if this command has already been resolved */
	if(this_command->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_command->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_command->template==NULL)
		return OK;

	template_command=xodtemplate_find_command(this_command->template);
	if(template_command==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in command definition could not be not found (config file '%s', line %d)\n",this_command->template,xodtemplate_config_file_name(this_command->_config_file),this_command->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template command... */
	xodtemplate_resolve_command(template_command);

	/* apply missing properties from template command... */
	if(this_command->command_name==NULL && template_command->command_name!=NULL)
		this_command->command_name=strdup(template_command->command_name);
	if(this_command->command_line==NULL && template_command->command_line!=NULL)
		this_command->command_line=strdup(template_command->command_line);

#ifdef DEBUG0
	printf("xodtemplate_resolve_command() end\n");
#endif

	return OK;
        }




/* resolves a contactgroup object */
int xodtemplate_resolve_contactgroup(xodtemplate_contactgroup *this_contactgroup){
	xodtemplate_contactgroup *template_contactgroup;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_contactgroup() start\n");
#endif

	/* return if this contactgroup has already been resolved */
	if(this_contactgroup->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_contactgroup->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_contactgroup->template==NULL)
		return OK;

	template_contactgroup=xodtemplate_find_contactgroup(this_contactgroup->template);
	if(template_contactgroup==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in contactgroup definition could not be not found (config file '%s', line %d)\n",this_contactgroup->template,xodtemplate_config_file_name(this_contactgroup->_config_file),this_contactgroup->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template contactgroup... */
	xodtemplate_resolve_contactgroup(template_contactgroup);

	/* apply missing properties from template contactgroup... */
	if(this_contactgroup->contactgroup_name==NULL && template_contactgroup->contactgroup_name!=NULL)
		this_contactgroup->contactgroup_name=strdup(template_contactgroup->contactgroup_name);
	if(this_contactgroup->alias==NULL && template_contactgroup->alias!=NULL)
		this_contactgroup->alias=strdup(template_contactgroup->alias);
	if(this_contactgroup->members==NULL && template_contactgroup->members!=NULL)
		this_contactgroup->members=strdup(template_contactgroup->members);

#ifdef DEBUG0
	printf("xodtemplate_resolve_contactgroup() end\n");
#endif

	return OK;
        }




/* resolves a hostgroup object */
int xodtemplate_resolve_hostgroup(xodtemplate_hostgroup *this_hostgroup){
	xodtemplate_hostgroup *template_hostgroup;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostgroup() start\n");
#endif

	/* return if this hostgroup has already been resolved */
	if(this_hostgroup->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_hostgroup->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_hostgroup->template==NULL)
		return OK;

	template_hostgroup=xodtemplate_find_hostgroup(this_hostgroup->template);
	if(template_hostgroup==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in hostgroup definition could not be not found (config file '%s', line %d)\n",this_hostgroup->template,xodtemplate_config_file_name(this_hostgroup->_config_file),this_hostgroup->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template hostgroup... */
	xodtemplate_resolve_hostgroup(template_hostgroup);

	/* apply missing properties from template hostgroup... */
	if(this_hostgroup->hostgroup_name==NULL && template_hostgroup->hostgroup_name!=NULL)
		this_hostgroup->hostgroup_name=strdup(template_hostgroup->hostgroup_name);
	if(this_hostgroup->alias==NULL && template_hostgroup->alias!=NULL)
		this_hostgroup->alias=strdup(template_hostgroup->alias);
	if(this_hostgroup->members==NULL && template_hostgroup->members!=NULL)
		this_hostgroup->members=strdup(template_hostgroup->members);

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostgroup() end\n");
#endif

	return OK;
        }


/* resolves a servicedependency object */
int xodtemplate_resolve_servicedependency(xodtemplate_servicedependency *this_servicedependency){
	xodtemplate_servicedependency *template_servicedependency;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_servicedependency() start\n");
#endif

	/* return if this servicedependency has already been resolved */
	if(this_servicedependency->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_servicedependency->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_servicedependency->template==NULL)
		return OK;

	template_servicedependency=xodtemplate_find_servicedependency(this_servicedependency->template);
	if(template_servicedependency==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in service dependency definition could not be not found (config file '%s', line %d)\n",this_servicedependency->template,xodtemplate_config_file_name(this_servicedependency->_config_file),this_servicedependency->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template servicedependency... */
	xodtemplate_resolve_servicedependency(template_servicedependency);

	/* apply missing properties from template servicedependency... */
	if(this_servicedependency->hostgroup_name==NULL && template_servicedependency->hostgroup_name!=NULL)
		this_servicedependency->hostgroup_name=strdup(template_servicedependency->hostgroup_name);
	if(this_servicedependency->host_name==NULL && template_servicedependency->host_name!=NULL)
		this_servicedependency->host_name=strdup(template_servicedependency->host_name);
	if(this_servicedependency->service_description==NULL && template_servicedependency->service_description!=NULL)
		this_servicedependency->service_description=strdup(template_servicedependency->service_description);
	if(this_servicedependency->dependent_hostgroup_name==NULL && template_servicedependency->dependent_hostgroup_name!=NULL)
		this_servicedependency->dependent_hostgroup_name=strdup(template_servicedependency->dependent_hostgroup_name);
	if(this_servicedependency->dependent_host_name==NULL && template_servicedependency->dependent_host_name!=NULL)
		this_servicedependency->dependent_host_name=strdup(template_servicedependency->dependent_host_name);
	if(this_servicedependency->dependent_service_description==NULL && template_servicedependency->dependent_service_description!=NULL)
		this_servicedependency->dependent_service_description=strdup(template_servicedependency->dependent_service_description);
	if(this_servicedependency->have_execution_dependency_options==FALSE && template_servicedependency->have_execution_dependency_options==TRUE){
		this_servicedependency->fail_execute_on_ok=template_servicedependency->fail_execute_on_ok;
		this_servicedependency->fail_execute_on_unknown=template_servicedependency->fail_execute_on_unknown;
		this_servicedependency->fail_execute_on_warning=template_servicedependency->fail_execute_on_warning;
		this_servicedependency->fail_execute_on_critical=template_servicedependency->fail_execute_on_critical;
		this_servicedependency->have_execution_dependency_options=TRUE;
	        }
	if(this_servicedependency->have_notification_dependency_options==FALSE && template_servicedependency->have_notification_dependency_options==TRUE){
		this_servicedependency->fail_notify_on_ok=template_servicedependency->fail_notify_on_ok;
		this_servicedependency->fail_notify_on_unknown=template_servicedependency->fail_notify_on_unknown;
		this_servicedependency->fail_notify_on_warning=template_servicedependency->fail_notify_on_warning;
		this_servicedependency->fail_notify_on_critical=template_servicedependency->fail_notify_on_critical;
		this_servicedependency->have_notification_dependency_options=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_servicedependency() end\n");
#endif

	return OK;
        }


/* resolves a serviceescalation object */
int xodtemplate_resolve_serviceescalation(xodtemplate_serviceescalation *this_serviceescalation){
	xodtemplate_serviceescalation *template_serviceescalation;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_serviceescalation() start\n");
#endif

	/* return if this serviceescalation has already been resolved */
	if(this_serviceescalation->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_serviceescalation->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_serviceescalation->template==NULL)
		return OK;

	template_serviceescalation=xodtemplate_find_serviceescalation(this_serviceescalation->template);
	if(template_serviceescalation==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in service escalation definition could not be not found (config file '%s', line %d)\n",this_serviceescalation->template,xodtemplate_config_file_name(this_serviceescalation->_config_file),this_serviceescalation->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template serviceescalation... */
	xodtemplate_resolve_serviceescalation(template_serviceescalation);

	/* apply missing properties from template serviceescalation... */
	if(this_serviceescalation->hostgroup_name==NULL && template_serviceescalation->hostgroup_name!=NULL)
		this_serviceescalation->hostgroup_name=strdup(template_serviceescalation->hostgroup_name);
	if(this_serviceescalation->host_name==NULL && template_serviceescalation->host_name!=NULL)
		this_serviceescalation->host_name=strdup(template_serviceescalation->host_name);
	if(this_serviceescalation->service_description==NULL && template_serviceescalation->service_description!=NULL)
		this_serviceescalation->service_description=strdup(template_serviceescalation->service_description);
	if(this_serviceescalation->contact_groups==NULL && template_serviceescalation->contact_groups!=NULL)
		this_serviceescalation->contact_groups=strdup(template_serviceescalation->contact_groups);
	if(this_serviceescalation->have_first_notification==FALSE && template_serviceescalation->have_first_notification==TRUE){
		this_serviceescalation->first_notification=template_serviceescalation->first_notification;
		this_serviceescalation->have_first_notification=TRUE;
	        }
	if(this_serviceescalation->have_last_notification==FALSE && template_serviceescalation->have_last_notification==TRUE){
		this_serviceescalation->last_notification=template_serviceescalation->last_notification;
		this_serviceescalation->have_last_notification=TRUE;
	        }
	if(this_serviceescalation->have_notification_interval==FALSE && template_serviceescalation->have_notification_interval==TRUE){
		this_serviceescalation->notification_interval=template_serviceescalation->notification_interval;
		this_serviceescalation->have_notification_interval=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_servicedeescalation() end\n");
#endif

	return OK;
        }



/* resolves a contact object */
int xodtemplate_resolve_contact(xodtemplate_contact *this_contact){
	xodtemplate_contact *template_contact;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_contact() start\n");
#endif

	/* return if this contact has already been resolved */
	if(this_contact->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_contact->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_contact->template==NULL)
		return OK;

	template_contact=xodtemplate_find_contact(this_contact->template);
	if(template_contact==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in contact definition could not be not found (config file '%s', line %d)\n",this_contact->template,xodtemplate_config_file_name(this_contact->_config_file),this_contact->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template contact... */
	xodtemplate_resolve_contact(template_contact);

	/* apply missing properties from template contact... */
	if(this_contact->contact_name==NULL && template_contact->contact_name!=NULL)
		this_contact->contact_name=strdup(template_contact->contact_name);
	if(this_contact->alias==NULL && template_contact->alias!=NULL)
		this_contact->alias=strdup(template_contact->alias);
	if(this_contact->email==NULL && template_contact->email!=NULL)
		this_contact->email=strdup(template_contact->email);
	if(this_contact->pager==NULL && template_contact->pager!=NULL)
		this_contact->pager=strdup(template_contact->pager);
	if(this_contact->host_notification_period==NULL && template_contact->host_notification_period!=NULL)
		this_contact->host_notification_period=strdup(template_contact->host_notification_period);
	if(this_contact->service_notification_period==NULL && template_contact->service_notification_period!=NULL)
		this_contact->service_notification_period=strdup(template_contact->service_notification_period);
	if(this_contact->host_notification_commands==NULL && template_contact->host_notification_commands!=NULL)
		this_contact->host_notification_commands=strdup(template_contact->host_notification_commands);
	if(this_contact->service_notification_commands==NULL && template_contact->service_notification_commands!=NULL)
		this_contact->service_notification_commands=strdup(template_contact->service_notification_commands);
	if(this_contact->have_host_notification_options==FALSE && template_contact->have_host_notification_options==TRUE){
		this_contact->notify_on_host_down=template_contact->notify_on_host_down;
		this_contact->notify_on_host_unreachable=template_contact->notify_on_host_unreachable;
		this_contact->notify_on_host_recovery=template_contact->notify_on_host_recovery;
		this_contact->have_host_notification_options=TRUE;
	        }
	if(this_contact->have_service_notification_options==FALSE && template_contact->have_service_notification_options==TRUE){
		this_contact->notify_on_service_unknown=template_contact->notify_on_service_unknown;
		this_contact->notify_on_service_warning=template_contact->notify_on_service_warning;
		this_contact->notify_on_service_critical=template_contact->notify_on_service_critical;
		this_contact->notify_on_service_recovery=template_contact->notify_on_service_recovery;
		this_contact->have_service_notification_options=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_contact() end\n");
#endif

	return OK;
        }



/* resolves a host object */
int xodtemplate_resolve_host(xodtemplate_host *this_host){
	xodtemplate_host *template_host;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_host() start\n");
#endif

	/* return if this host has already been resolved */
	if(this_host->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_host->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_host->template==NULL)
		return OK;

	template_host=xodtemplate_find_host(this_host->template);
	if(template_host==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in host definition could not be not found (config file '%s', line %d)\n",this_host->template,xodtemplate_config_file_name(this_host->_config_file),this_host->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template host... */
	xodtemplate_resolve_host(template_host);

	/* apply missing properties from template host... */
	if(this_host->host_name==NULL && template_host->host_name!=NULL)
		this_host->host_name=strdup(template_host->host_name);
	if(this_host->alias==NULL && template_host->alias!=NULL)
		this_host->alias=strdup(template_host->alias);
	if(this_host->address==NULL && template_host->address!=NULL)
		this_host->address=strdup(template_host->address);
	if(this_host->parents==NULL && template_host->parents!=NULL)
		this_host->parents=strdup(template_host->parents);
	if(this_host->hostgroups==NULL && template_host->hostgroups!=NULL)
		this_host->hostgroups=strdup(template_host->hostgroups);
	if(this_host->check_command==NULL && template_host->check_command!=NULL)
		this_host->check_command=strdup(template_host->check_command);
	if(this_host->event_handler==NULL && template_host->event_handler!=NULL)
		this_host->event_handler=strdup(template_host->event_handler);
	if(this_host->contact_groups==NULL && template_host->contact_groups!=NULL)
		this_host->contact_groups=strdup(template_host->contact_groups);
	if(this_host->notification_period==NULL && template_host->notification_period!=NULL)
		this_host->notification_period=strdup(template_host->notification_period);
	if(this_host->failure_prediction_options==NULL && template_host->failure_prediction_options!=NULL)
		this_host->failure_prediction_options=strdup(template_host->failure_prediction_options);
	if(this_host->have_max_check_attempts==FALSE && template_host->have_max_check_attempts==TRUE){
		this_host->max_check_attempts=template_host->max_check_attempts;
		this_host->have_max_check_attempts=TRUE;
	        }
	if(this_host->have_active_checks_enabled==FALSE && template_host->have_active_checks_enabled==TRUE){
		this_host->active_checks_enabled=template_host->active_checks_enabled;
		this_host->have_active_checks_enabled=TRUE;
	        }
	if(this_host->have_passive_checks_enabled==FALSE && template_host->have_passive_checks_enabled==TRUE){
		this_host->passive_checks_enabled=template_host->passive_checks_enabled;
		this_host->have_passive_checks_enabled=TRUE;
	        }
	if(this_host->have_obsess_over_host==FALSE && template_host->have_obsess_over_host==TRUE){
		this_host->obsess_over_host=template_host->obsess_over_host;
		this_host->have_obsess_over_host=TRUE;
	        }
	if(this_host->have_event_handler_enabled==FALSE && template_host->have_event_handler_enabled==TRUE){
		this_host->event_handler_enabled=template_host->event_handler_enabled;
		this_host->have_event_handler_enabled=TRUE;
	        }
	if(this_host->have_low_flap_threshold==FALSE && template_host->have_low_flap_threshold==TRUE){
		this_host->low_flap_threshold=template_host->low_flap_threshold;
		this_host->have_low_flap_threshold=TRUE;
	        }
	if(this_host->have_high_flap_threshold==FALSE && template_host->have_high_flap_threshold==TRUE){
		this_host->high_flap_threshold=template_host->high_flap_threshold;
		this_host->have_high_flap_threshold=TRUE;
	        }
	if(this_host->have_flap_detection_enabled==FALSE && template_host->have_flap_detection_enabled==TRUE){
		this_host->flap_detection_enabled=template_host->flap_detection_enabled;
		this_host->have_flap_detection_enabled=TRUE;
	        }
	if(this_host->have_notification_options==FALSE && template_host->have_notification_options==TRUE){
		this_host->notify_on_down=template_host->notify_on_down;
		this_host->notify_on_unreachable=template_host->notify_on_unreachable;
		this_host->notify_on_recovery=template_host->notify_on_recovery;
		this_host->have_notification_options=TRUE;
	        }
	if(this_host->have_notifications_enabled==FALSE && template_host->have_notifications_enabled==TRUE){
		this_host->notifications_enabled=template_host->notifications_enabled;
		this_host->have_notifications_enabled=TRUE;
	        }
	if(this_host->have_notification_interval==FALSE && template_host->have_notification_interval==TRUE){
		this_host->notification_interval=template_host->notification_interval;
		this_host->have_notification_interval=TRUE;
	        }
	if(this_host->have_stalking_options==FALSE && template_host->have_stalking_options==TRUE){
		this_host->stalk_on_up=template_host->stalk_on_up;
		this_host->stalk_on_down=template_host->stalk_on_down;
		this_host->stalk_on_unreachable=template_host->stalk_on_unreachable;
		this_host->have_stalking_options=TRUE;
	        }
	if(this_host->have_process_perf_data==FALSE && template_host->have_process_perf_data==TRUE){
		this_host->process_perf_data=template_host->process_perf_data;
		this_host->have_process_perf_data=TRUE;
	        }
	if(this_host->have_failure_prediction_enabled==FALSE && template_host->have_failure_prediction_enabled==TRUE){
		this_host->failure_prediction_enabled=template_host->failure_prediction_enabled;
		this_host->have_failure_prediction_enabled=TRUE;
	        }
	if(this_host->have_retain_status_information==FALSE && template_host->have_retain_status_information==TRUE){
		this_host->retain_status_information=template_host->retain_status_information;
		this_host->have_retain_status_information=TRUE;
	        }
	if(this_host->have_retain_nonstatus_information==FALSE && template_host->have_retain_nonstatus_information==TRUE){
		this_host->retain_nonstatus_information=template_host->retain_nonstatus_information;
		this_host->have_retain_nonstatus_information=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_host() end\n");
#endif

	return OK;
        }



/* resolves a service object */
int xodtemplate_resolve_service(xodtemplate_service *this_service){
	xodtemplate_service *template_service;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_service() start\n");
#endif

	/* return if this service has already been resolved */
	if(this_service->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_service->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_service->template==NULL)
		return OK;

	template_service=xodtemplate_find_service(this_service->template);
	if(template_service==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in service definition could not be not found (config file '%s', line %d)\n",this_service->template,xodtemplate_config_file_name(this_service->_config_file),this_service->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template service... */
	xodtemplate_resolve_service(template_service);

	/* apply missing properties from template service... */
	if(this_service->hostgroup_name==NULL && template_service->hostgroup_name!=NULL)
		this_service->hostgroup_name=strdup(template_service->hostgroup_name);
	if(this_service->host_name==NULL && template_service->host_name!=NULL)
		this_service->host_name=strdup(template_service->host_name);
	if(this_service->service_description==NULL && template_service->service_description!=NULL)
		this_service->service_description=strdup(template_service->service_description);
	if(this_service->check_command==NULL && template_service->check_command!=NULL)
		this_service->check_command=strdup(template_service->check_command);
	if(this_service->check_period==NULL && template_service->check_period!=NULL)
		this_service->check_period=strdup(template_service->check_period);
	if(this_service->event_handler==NULL && template_service->event_handler!=NULL)
		this_service->event_handler=strdup(template_service->event_handler);
	if(this_service->notification_period==NULL && template_service->notification_period!=NULL)
		this_service->notification_period=strdup(template_service->notification_period);
	if(this_service->contact_groups==NULL && template_service->contact_groups!=NULL)
		this_service->contact_groups=strdup(template_service->contact_groups);
	if(this_service->failure_prediction_options==NULL && template_service->failure_prediction_options!=NULL)
		this_service->failure_prediction_options=strdup(template_service->failure_prediction_options);
	if(this_service->have_max_check_attempts==FALSE && template_service->have_max_check_attempts==TRUE){
		this_service->max_check_attempts=template_service->max_check_attempts;
		this_service->have_max_check_attempts=TRUE;
	        }
	if(this_service->have_normal_check_interval==FALSE && template_service->have_normal_check_interval==TRUE){
		this_service->normal_check_interval=template_service->normal_check_interval;
		this_service->have_normal_check_interval=TRUE;
	        }
	if(this_service->have_retry_check_interval==FALSE && template_service->have_retry_check_interval==TRUE){
		this_service->retry_check_interval=template_service->retry_check_interval;
		this_service->have_retry_check_interval=TRUE;
	        }
	if(this_service->have_active_checks_enabled==FALSE && template_service->have_active_checks_enabled==TRUE){
		this_service->active_checks_enabled=template_service->active_checks_enabled;
		this_service->have_active_checks_enabled=TRUE;
	        }
	if(this_service->have_passive_checks_enabled==FALSE && template_service->have_passive_checks_enabled==TRUE){
		this_service->passive_checks_enabled=template_service->passive_checks_enabled;
		this_service->have_passive_checks_enabled=TRUE;
	        }
	if(this_service->have_parallelize_check==FALSE && template_service->have_parallelize_check==TRUE){
		this_service->parallelize_check=template_service->parallelize_check;
		this_service->have_parallelize_check=TRUE;
	        }
	if(this_service->have_is_volatile==FALSE && template_service->have_is_volatile==TRUE){
		this_service->is_volatile=template_service->is_volatile;
		this_service->have_is_volatile=TRUE;
	        }
	if(this_service->have_obsess_over_service==FALSE && template_service->have_obsess_over_service==TRUE){
		this_service->obsess_over_service=template_service->obsess_over_service;
		this_service->have_obsess_over_service=TRUE;
	        }
	if(this_service->have_event_handler_enabled==FALSE && template_service->have_event_handler_enabled==TRUE){
		this_service->event_handler_enabled=template_service->event_handler_enabled;
		this_service->have_event_handler_enabled=TRUE;
	        }
	if(this_service->have_check_freshness==FALSE && template_service->have_check_freshness==TRUE){
		this_service->check_freshness=template_service->check_freshness;
		this_service->have_check_freshness=TRUE;
	        }
	if(this_service->have_freshness_threshold==FALSE && template_service->have_freshness_threshold==TRUE){
		this_service->freshness_threshold=template_service->freshness_threshold;
		this_service->have_freshness_threshold=TRUE;
	        }
	if(this_service->have_low_flap_threshold==FALSE && template_service->have_low_flap_threshold==TRUE){
		this_service->low_flap_threshold=template_service->low_flap_threshold;
		this_service->have_low_flap_threshold=TRUE;
	        }
	if(this_service->have_high_flap_threshold==FALSE && template_service->have_high_flap_threshold==TRUE){
		this_service->high_flap_threshold=template_service->high_flap_threshold;
		this_service->have_high_flap_threshold=TRUE;
	        }
	if(this_service->have_flap_detection_enabled==FALSE && template_service->have_flap_detection_enabled==TRUE){
		this_service->flap_detection_enabled=template_service->flap_detection_enabled;
		this_service->have_flap_detection_enabled=TRUE;
	        }
	if(this_service->have_notification_options==FALSE && template_service->have_notification_options==TRUE){
		this_service->notify_on_unknown=template_service->notify_on_unknown;
		this_service->notify_on_warning=template_service->notify_on_warning;
		this_service->notify_on_critical=template_service->notify_on_critical;
		this_service->notify_on_recovery=template_service->notify_on_recovery;
		this_service->have_notification_options=TRUE;
	        }
	if(this_service->have_notifications_enabled==FALSE && template_service->have_notifications_enabled==TRUE){
		this_service->notifications_enabled=template_service->notifications_enabled;
		this_service->have_notifications_enabled=TRUE;
	        }
	if(this_service->have_notification_interval==FALSE && template_service->have_notification_interval==TRUE){
		this_service->notification_interval=template_service->notification_interval;
		this_service->have_notification_interval=TRUE;
	        }
	if(this_service->have_stalking_options==FALSE && template_service->have_stalking_options==TRUE){
		this_service->stalk_on_ok=template_service->stalk_on_ok;
		this_service->stalk_on_unknown=template_service->stalk_on_unknown;
		this_service->stalk_on_warning=template_service->stalk_on_warning;
		this_service->stalk_on_critical=template_service->stalk_on_critical;
		this_service->have_stalking_options=TRUE;
	        }
	if(this_service->have_process_perf_data==FALSE && template_service->have_process_perf_data==TRUE){
		this_service->process_perf_data=template_service->process_perf_data;
		this_service->have_process_perf_data=TRUE;
	        }
	if(this_service->have_failure_prediction_enabled==FALSE && template_service->have_failure_prediction_enabled==TRUE){
		this_service->failure_prediction_enabled=template_service->failure_prediction_enabled;
		this_service->have_failure_prediction_enabled=TRUE;
	        }
	if(this_service->have_retain_status_information==FALSE && template_service->have_retain_status_information==TRUE){
		this_service->retain_status_information=template_service->retain_status_information;
		this_service->have_retain_status_information=TRUE;
	        }
	if(this_service->have_retain_nonstatus_information==FALSE && template_service->have_retain_nonstatus_information==TRUE){
		this_service->retain_nonstatus_information=template_service->retain_nonstatus_information;
		this_service->have_retain_nonstatus_information=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_service() end\n");
#endif

	return OK;
        }


/* resolves a hostdependency object */
int xodtemplate_resolve_hostdependency(xodtemplate_hostdependency *this_hostdependency){
	xodtemplate_hostdependency *template_hostdependency;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostdependency() start\n");
#endif

	/* return if this hostdependency has already been resolved */
	if(this_hostdependency->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_hostdependency->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_hostdependency->template==NULL)
		return OK;

	template_hostdependency=xodtemplate_find_hostdependency(this_hostdependency->template);
	if(template_hostdependency==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in host dependency definition could not be not found (config file '%s', line %d)\n",this_hostdependency->template,xodtemplate_config_file_name(this_hostdependency->_config_file),this_hostdependency->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template hostdependency... */
	xodtemplate_resolve_hostdependency(template_hostdependency);

	/* apply missing properties from template hostdependency... */
	if(this_hostdependency->hostgroup_name==NULL && template_hostdependency->hostgroup_name!=NULL)
		this_hostdependency->hostgroup_name=strdup(template_hostdependency->hostgroup_name);
	if(this_hostdependency->dependent_hostgroup_name==NULL && template_hostdependency->dependent_hostgroup_name!=NULL)
		this_hostdependency->dependent_hostgroup_name=strdup(template_hostdependency->dependent_hostgroup_name);
	if(this_hostdependency->host_name==NULL && template_hostdependency->host_name!=NULL)
		this_hostdependency->host_name=strdup(template_hostdependency->host_name);
	if(this_hostdependency->dependent_host_name==NULL && template_hostdependency->dependent_host_name!=NULL)
		this_hostdependency->dependent_host_name=strdup(template_hostdependency->dependent_host_name);
	if(this_hostdependency->have_notification_dependency_options==FALSE && template_hostdependency->have_notification_dependency_options==TRUE){
		this_hostdependency->fail_notify_on_up=template_hostdependency->fail_notify_on_up;
		this_hostdependency->fail_notify_on_down=template_hostdependency->fail_notify_on_down;
		this_hostdependency->fail_notify_on_unreachable=template_hostdependency->fail_notify_on_unreachable;
		this_hostdependency->have_notification_dependency_options=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostdependency() end\n");
#endif

	return OK;
        }


/* resolves a hostescalation object */
int xodtemplate_resolve_hostescalation(xodtemplate_hostescalation *this_hostescalation){
	xodtemplate_hostescalation *template_hostescalation;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostescalation() start\n");
#endif

	/* return if this hostescalation has already been resolved */
	if(this_hostescalation->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_hostescalation->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_hostescalation->template==NULL)
		return OK;

	template_hostescalation=xodtemplate_find_hostescalation(this_hostescalation->template);
	if(template_hostescalation==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in host escalation definition could not be not found (config file '%s', line %d)\n",this_hostescalation->template,xodtemplate_config_file_name(this_hostescalation->_config_file),this_hostescalation->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template hostescalation... */
	xodtemplate_resolve_hostescalation(template_hostescalation);

	/* apply missing properties from template hostescalation... */
	if(this_hostescalation->host_name==NULL && template_hostescalation->host_name!=NULL)
		this_hostescalation->host_name=strdup(template_hostescalation->host_name);
	if(this_hostescalation->contact_groups==NULL && template_hostescalation->contact_groups!=NULL)
		this_hostescalation->contact_groups=strdup(template_hostescalation->contact_groups);
	if(this_hostescalation->have_first_notification==FALSE && template_hostescalation->have_first_notification==TRUE){
		this_hostescalation->first_notification=template_hostescalation->first_notification;
		this_hostescalation->have_first_notification=TRUE;
	        }
	if(this_hostescalation->have_last_notification==FALSE && template_hostescalation->have_last_notification==TRUE){
		this_hostescalation->last_notification=template_hostescalation->last_notification;
		this_hostescalation->have_last_notification=TRUE;
	        }
	if(this_hostescalation->have_notification_interval==FALSE && template_hostescalation->have_notification_interval==TRUE){
		this_hostescalation->notification_interval=template_hostescalation->notification_interval;
		this_hostescalation->have_notification_interval=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostdependency() end\n");
#endif

	return OK;
        }



/* resolves a hostextinfo object */
int xodtemplate_resolve_hostextinfo(xodtemplate_hostextinfo *this_hostextinfo){
	xodtemplate_hostextinfo *template_hostextinfo;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostextinfo() start\n");
#endif

	/* return if this object has already been resolved */
	if(this_hostextinfo->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_hostextinfo->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_hostextinfo->template==NULL)
		return OK;

	template_hostextinfo=xodtemplate_find_hostextinfo(this_hostextinfo->template);
	if(template_hostextinfo==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in extended host info definition could not be not found (config file '%s', line %d)\n",this_hostextinfo->template,xodtemplate_config_file_name(this_hostextinfo->_config_file),this_hostextinfo->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template hostextinfo... */
	xodtemplate_resolve_hostextinfo(template_hostextinfo);

	/* apply missing properties from template hostextinfo... */
	if(this_hostextinfo->name==NULL && template_hostextinfo->name!=NULL)
		this_hostextinfo->name=strdup(template_hostextinfo->name);
	if(this_hostextinfo->host_name==NULL && template_hostextinfo->host_name!=NULL)
		this_hostextinfo->host_name=strdup(template_hostextinfo->host_name);
	if(this_hostextinfo->hostgroup_name==NULL && template_hostextinfo->hostgroup_name!=NULL)
		this_hostextinfo->hostgroup_name=strdup(template_hostextinfo->hostgroup_name);
	if(this_hostextinfo->notes_url==NULL && template_hostextinfo->notes_url!=NULL)
		this_hostextinfo->notes_url=strdup(template_hostextinfo->notes_url);
	if(this_hostextinfo->icon_image==NULL && template_hostextinfo->icon_image!=NULL)
		this_hostextinfo->icon_image=strdup(template_hostextinfo->icon_image);
	if(this_hostextinfo->icon_image_alt==NULL && template_hostextinfo->icon_image_alt!=NULL)
		this_hostextinfo->icon_image_alt=strdup(template_hostextinfo->icon_image_alt);
	if(this_hostextinfo->vrml_image==NULL && template_hostextinfo->vrml_image!=NULL)
		this_hostextinfo->vrml_image=strdup(template_hostextinfo->vrml_image);
	if(this_hostextinfo->statusmap_image==NULL && template_hostextinfo->statusmap_image!=NULL)
		this_hostextinfo->statusmap_image=strdup(template_hostextinfo->statusmap_image);
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
	printf("xodtemplate_resolve_hostextinfo() end\n");
#endif

	return OK;
        }



/* resolves a serviceextinfo object */
int xodtemplate_resolve_serviceextinfo(xodtemplate_serviceextinfo *this_serviceextinfo){
	xodtemplate_serviceextinfo *template_serviceextinfo;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_resolve_serviceextinfo() start\n");
#endif

	/* return if this object has already been resolved */
	if(this_serviceextinfo->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_serviceextinfo->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_serviceextinfo->template==NULL)
		return OK;

	template_serviceextinfo=xodtemplate_find_serviceextinfo(this_serviceextinfo->template);
	if(template_serviceextinfo==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Template '%s' specified in extended service info definition could not be not found (config file '%s', line %d)\n",this_serviceextinfo->template,xodtemplate_config_file_name(this_serviceextinfo->_config_file),this_serviceextinfo->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* resolve the template serviceextinfo... */
	xodtemplate_resolve_serviceextinfo(template_serviceextinfo);

	/* apply missing properties from template serviceextinfo... */
	if(this_serviceextinfo->name==NULL && template_serviceextinfo->name!=NULL)
		this_serviceextinfo->name=strdup(template_serviceextinfo->name);
	if(this_serviceextinfo->host_name==NULL && template_serviceextinfo->host_name!=NULL)
		this_serviceextinfo->host_name=strdup(template_serviceextinfo->host_name);
	if(this_serviceextinfo->hostgroup_name==NULL && template_serviceextinfo->hostgroup_name!=NULL)
		this_serviceextinfo->hostgroup_name=strdup(template_serviceextinfo->hostgroup_name);
	if(this_serviceextinfo->service_description==NULL && template_serviceextinfo->service_description!=NULL)
		this_serviceextinfo->service_description=strdup(template_serviceextinfo->service_description);
	if(this_serviceextinfo->notes_url==NULL && template_serviceextinfo->notes_url!=NULL)
		this_serviceextinfo->notes_url=strdup(template_serviceextinfo->notes_url);
	if(this_serviceextinfo->icon_image==NULL && template_serviceextinfo->icon_image!=NULL)
		this_serviceextinfo->icon_image=strdup(template_serviceextinfo->icon_image);
	if(this_serviceextinfo->icon_image_alt==NULL && template_serviceextinfo->icon_image_alt!=NULL)
		this_serviceextinfo->icon_image_alt=strdup(template_serviceextinfo->icon_image_alt);

#ifdef DEBUG0
	printf("xodtemplate_resolve_serviceextinfo() end\n");
#endif

	return OK;
        }





/******************************************************************/
/*************** OBJECT RECOMBOBULATION FUNCTIONS *****************/
/******************************************************************/


/* recombobulates object definitions */
int xodtemplate_recombobulate_objects(void){
	int result=OK;
	xodtemplate_host *temp_host;
	xodtemplate_hostgroup *temp_hostgroup;
	char *hostgroup_names;
	char *temp_ptr;
	char *new_members;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_recombobulate_objects() start\n");
#endif

	/* process all hosts that have hostgroup directives */
	for(temp_host=xodtemplate_host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* skip hosts without hostgroup directives or host names */
		if(temp_host->hostgroups==NULL || temp_host->host_name==NULL)
			continue;

		/* process the list of hostgroups */
		hostgroup_names=strdup(temp_host->hostgroups);
		if(hostgroup_names==NULL)
			continue;
		for(temp_ptr=strtok(hostgroup_names,",");temp_ptr;temp_ptr=strtok(NULL,",")){

			/* strip trailing spaces */
			strip(temp_ptr);
			
			/* find the hostgroup */
			temp_hostgroup=xodtemplate_find_real_hostgroup(temp_ptr);
			if(temp_hostgroup==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not find hostgroup '%s' specified in host '%s' definition (config file '%s', line %d)\n",temp_ptr,temp_host->host_name,xodtemplate_config_file_name(temp_host->_config_file),temp_host->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				free(hostgroup_names);
				return ERROR;
			        }

			/* add this list to the hostgroup members directive */
			if(temp_hostgroup->members==NULL)
				temp_hostgroup->members=strdup(temp_ptr);
			else{
				new_members=(char *)realloc(temp_hostgroup->members,strlen(temp_hostgroup->members)+strlen(temp_host->host_name)+2);
				if(new_members!=NULL){
					temp_hostgroup->members=new_members;
					strcat(temp_hostgroup->members,",");
					strcat(temp_hostgroup->members,temp_host->host_name);
				        }
			        }
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_recombobulate_objects() end\n");
#endif

	return OK;
        }



/******************************************************************/
/******************* OBJECT SEARCH FUNCTIONS **********************/
/******************************************************************/

/* finds a specific timeperiod object */
xodtemplate_timeperiod *xodtemplate_find_timeperiod(char *name){
	xodtemplate_timeperiod *temp_timeperiod;

	if(name==NULL)
		return NULL;

	for(temp_timeperiod=xodtemplate_timeperiod_list;temp_timeperiod!=NULL;temp_timeperiod=temp_timeperiod->next){
		if(temp_timeperiod->name==NULL)
			continue;
		if(!strcmp(temp_timeperiod->name,name))
			break;
	        }

	return temp_timeperiod;
        }


/* finds a specific command object */
xodtemplate_command *xodtemplate_find_command(char *name){
	xodtemplate_command *temp_command;

	if(name==NULL)
		return NULL;

	for(temp_command=xodtemplate_command_list;temp_command!=NULL;temp_command=temp_command->next){
		if(temp_command->name==NULL)
			continue;
		if(!strcmp(temp_command->name,name))
			break;
	        }

	return temp_command;
        }


/* finds a specific contactgroup object */
xodtemplate_contactgroup *xodtemplate_find_contactgroup(char *name){
	xodtemplate_contactgroup *temp_contactgroup;

	if(name==NULL)
		return NULL;

	for(temp_contactgroup=xodtemplate_contactgroup_list;temp_contactgroup!=NULL;temp_contactgroup=temp_contactgroup->next){
		if(temp_contactgroup->name==NULL)
			continue;
		if(!strcmp(temp_contactgroup->name,name))
			break;
	        }

	return temp_contactgroup;
        }


/* finds a specific hostgroup object */
xodtemplate_hostgroup *xodtemplate_find_hostgroup(char *name){
	xodtemplate_hostgroup *temp_hostgroup;

	if(name==NULL)
		return NULL;

	for(temp_hostgroup=xodtemplate_hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
		if(temp_hostgroup->name==NULL)
			continue;
		if(!strcmp(temp_hostgroup->name,name))
			break;
	        }

	return temp_hostgroup;
        }


/* finds a specific hostgroup object by its REAL name, not its TEMPLATE name */
xodtemplate_hostgroup *xodtemplate_find_real_hostgroup(char *name){
	xodtemplate_hostgroup *temp_hostgroup;

	if(name==NULL)
		return NULL;

	for(temp_hostgroup=xodtemplate_hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
		if(temp_hostgroup->hostgroup_name==NULL)
			continue;
		if(!strcmp(temp_hostgroup->hostgroup_name,name))
			break;
	        }

	return temp_hostgroup;
        }


/* finds a specific servicedependency object */
xodtemplate_servicedependency *xodtemplate_find_servicedependency(char *name){
	xodtemplate_servicedependency *temp_servicedependency;

	if(name==NULL)
		return NULL;

	for(temp_servicedependency=xodtemplate_servicedependency_list;temp_servicedependency!=NULL;temp_servicedependency=temp_servicedependency->next){
		if(temp_servicedependency->name==NULL)
			continue;
		if(!strcmp(temp_servicedependency->name,name))
			break;
	        }

	return temp_servicedependency;
        }


/* finds a specific serviceescalation object */
xodtemplate_serviceescalation *xodtemplate_find_serviceescalation(char *name){
	xodtemplate_serviceescalation *temp_serviceescalation;

	if(name==NULL)
		return NULL;

	for(temp_serviceescalation=xodtemplate_serviceescalation_list;temp_serviceescalation!=NULL;temp_serviceescalation=temp_serviceescalation->next){
		if(temp_serviceescalation->name==NULL)
			continue;
		if(!strcmp(temp_serviceescalation->name,name))
			break;
	        }

	return temp_serviceescalation;
        }


/* finds a specific contact object */
xodtemplate_contact *xodtemplate_find_contact(char *name){
	xodtemplate_contact *temp_contact;

	if(name==NULL)
		return NULL;

	for(temp_contact=xodtemplate_contact_list;temp_contact!=NULL;temp_contact=temp_contact->next){
		if(temp_contact->name==NULL)
			continue;
		if(!strcmp(temp_contact->name,name))
			break;
	        }

	return temp_contact;
        }


/* finds a specific host object */
xodtemplate_host *xodtemplate_find_host(char *name){
	xodtemplate_host *temp_host;

	if(name==NULL)
		return NULL;

	for(temp_host=xodtemplate_host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(temp_host->name==NULL)
			continue;
		if(!strcmp(temp_host->name,name))
			break;
	        }

	return temp_host;
        }


/* finds a specific host object by its REAL name, not its TEMPLATE name */
xodtemplate_host *xodtemplate_find_real_host(char *name){
	xodtemplate_host *temp_host;

	if(name==NULL)
		return NULL;

	for(temp_host=xodtemplate_host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(temp_host->host_name==NULL)
			continue;
		if(!strcmp(temp_host->host_name,name))
			break;
	        }

	return temp_host;
        }


/* finds a specific service object */
xodtemplate_service *xodtemplate_find_service(char *name){
	xodtemplate_service *iptr;

	if(name==NULL || xodtemplate_service_list==NULL)
		return NULL;

	for(iptr=xodtemplate_service_list[hashfunc1(name,SERVICES_HASHSLOTS)];iptr && xodtemplate_service_comes_after(iptr, name);iptr=iptr->next);

	if(iptr && (compare_xodtemplate_service(iptr,name)==0))
		return iptr;

	return NULL;
        }


/* finds a specific service object by its REAL name, not its TEMPLATE name */
xodtemplate_service *xodtemplate_find_real_service(char *host, char *description){
	xodtemplate_service *temp_service;
	void *xod_svc_cursor;

	if(host==NULL || description==NULL || xodtemplate_service_list==NULL)
		return NULL;

	temp_service=NULL;
	xod_svc_cursor=get_xodtemplate_service_cursor();
	while(temp_service=get_next_xodtemplate_service(xod_svc_cursor)){
		if(temp_service->host_name==NULL || temp_service->service_description==NULL)
			continue;
		if(!strcmp(host,temp_service->host_name) && !strcmp(description,temp_service->service_description))
			break;
	        }
	free_xodtemplate_service_cursor(xod_svc_cursor);

	return temp_service;
        }


xodtemplate_service *get_next_xodtemplate_service(void *v_cursor){
	xodtemplate_service_cursor *cursor=v_cursor;

	if(!cursor)
		return NULL;

	cursor->current_xodtemplate_service=get_next_N((void **)xodtemplate_service_list,SERVICES_HASHSLOTS,&(cursor->xodtemplate_service_iterator),cursor->current_xodtemplate_service,(cursor->current_xodtemplate_service?cursor->current_xodtemplate_service->next:NULL));

	return cursor->current_xodtemplate_service;
        }


/* finds a specific hostdependency object */
xodtemplate_hostdependency *xodtemplate_find_hostdependency(char *name){
	xodtemplate_hostdependency *temp_hostdependency;

	if(name==NULL)
		return NULL;

	for(temp_hostdependency=xodtemplate_hostdependency_list;temp_hostdependency!=NULL;temp_hostdependency=temp_hostdependency->next){
		if(temp_hostdependency->name==NULL)
			continue;
		if(!strcmp(temp_hostdependency->name,name))
			break;
	        }

	return temp_hostdependency;
        }


/* finds a specific hostescalation object */
xodtemplate_hostescalation *xodtemplate_find_hostescalation(char *name){
	xodtemplate_hostescalation *temp_hostescalation;

	if(name==NULL)
		return NULL;

	for(temp_hostescalation=xodtemplate_hostescalation_list;temp_hostescalation!=NULL;temp_hostescalation=temp_hostescalation->next){
		if(temp_hostescalation->name==NULL)
			continue;
		if(!strcmp(temp_hostescalation->name,name))
			break;
	        }

	return temp_hostescalation;
        }


/* finds a specific hostextinfo object */
xodtemplate_hostextinfo *xodtemplate_find_hostextinfo(char *name){
	xodtemplate_hostextinfo *temp_hostextinfo;

	if(name==NULL)
		return NULL;

	for(temp_hostextinfo=xodtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){
		if(temp_hostextinfo->name==NULL)
			continue;
		if(!strcmp(temp_hostextinfo->name,name))
			break;
	        }

	return temp_hostextinfo;
        }


/* finds a specific serviceextinfo object */
xodtemplate_serviceextinfo *xodtemplate_find_serviceextinfo(char *name){
	xodtemplate_serviceextinfo *temp_serviceextinfo;

	if(name==NULL)
		return NULL;

	for(temp_serviceextinfo=xodtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){
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
int xodtemplate_register_objects(void){
	int result=OK;
	xodtemplate_timeperiod *temp_timeperiod;
	xodtemplate_command *temp_command;
	xodtemplate_contactgroup *temp_contactgroup;
	xodtemplate_hostgroup *temp_hostgroup;
	xodtemplate_contact *temp_contact;
	xodtemplate_host *temp_host;
	xodtemplate_service *temp_service;
	xodtemplate_servicedependency *temp_servicedependency;
	xodtemplate_serviceescalation *temp_serviceescalation;
	xodtemplate_hostdependency *temp_hostdependency;
	xodtemplate_hostescalation *temp_hostescalation;
	xodtemplate_hostextinfo *temp_hostextinfo;
	xodtemplate_serviceextinfo *temp_serviceextinfo;
	void *xod_svc_cursor;

#ifdef DEBUG0
	printf("xodtemplate_register_objects() start\n");
#endif

	/* register timeperiods */
	for(temp_timeperiod=xodtemplate_timeperiod_list;temp_timeperiod!=NULL;temp_timeperiod=temp_timeperiod->next){
		if((result=xodtemplate_register_timeperiod(temp_timeperiod))==ERROR)
			return ERROR;
	        }

	/* register commands */
	for(temp_command=xodtemplate_command_list;temp_command!=NULL;temp_command=temp_command->next){
		if((result=xodtemplate_register_command(temp_command))==ERROR)
			return ERROR;
	        }

	/* register contactgroups */
	for(temp_contactgroup=xodtemplate_contactgroup_list;temp_contactgroup!=NULL;temp_contactgroup=temp_contactgroup->next){
		if((result=xodtemplate_register_contactgroup(temp_contactgroup))==ERROR)
			return ERROR;
	        }

	/* register hostgroups */
	for(temp_hostgroup=xodtemplate_hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
		if((result=xodtemplate_register_hostgroup(temp_hostgroup))==ERROR)
			return ERROR;
	        }

	/* register contacts */
	for(temp_contact=xodtemplate_contact_list;temp_contact!=NULL;temp_contact=temp_contact->next){
		if((result=xodtemplate_register_contact(temp_contact))==ERROR)
			return ERROR;
	        }

	/* register hosts */
	for(temp_host=xodtemplate_host_list;temp_host!=NULL;temp_host=temp_host->next){
		if((result=xodtemplate_register_host(temp_host))==ERROR)
			return ERROR;
	        }

	/* register services */
	xod_svc_cursor = get_xodtemplate_service_cursor();
	while(temp_service=get_next_xodtemplate_service(xod_svc_cursor)) {
		if((result=xodtemplate_register_service(temp_service))==ERROR)
			return ERROR;
	        }
	free_xodtemplate_service_cursor(xod_svc_cursor);

	/* register service dependencies */
	for(temp_servicedependency=xodtemplate_servicedependency_list;temp_servicedependency!=NULL;temp_servicedependency=temp_servicedependency->next){
		if((result=xodtemplate_register_servicedependency(temp_servicedependency))==ERROR)
			return ERROR;
	        }

	/* register service escalations */
	for(temp_serviceescalation=xodtemplate_serviceescalation_list;temp_serviceescalation!=NULL;temp_serviceescalation=temp_serviceescalation->next){
		if((result=xodtemplate_register_serviceescalation(temp_serviceescalation))==ERROR)
			return ERROR;
	        }

	/* register host dependencies */
	for(temp_hostdependency=xodtemplate_hostdependency_list;temp_hostdependency!=NULL;temp_hostdependency=temp_hostdependency->next){
		if((result=xodtemplate_register_hostdependency(temp_hostdependency))==ERROR)
			return ERROR;
	        }

	/* register host escalations */
	for(temp_hostescalation=xodtemplate_hostescalation_list;temp_hostescalation!=NULL;temp_hostescalation=temp_hostescalation->next){
		if((result=xodtemplate_register_hostescalation(temp_hostescalation))==ERROR)
			return ERROR;
	        }

	/* register host extended info */
	for(temp_hostextinfo=xodtemplate_hostextinfo_list;temp_hostextinfo!=NULL;temp_hostextinfo=temp_hostextinfo->next){
		if((result=xodtemplate_register_hostextinfo(temp_hostextinfo))==ERROR)
			return ERROR;
	        }

	/* register service extended info */
	for(temp_serviceextinfo=xodtemplate_serviceextinfo_list;temp_serviceextinfo!=NULL;temp_serviceextinfo=temp_serviceextinfo->next){
		if((result=xodtemplate_register_serviceextinfo(temp_serviceextinfo))==ERROR)
			return ERROR;
	        }


#ifdef DEBUG0
	printf("xodtemplate_register_objects() end\n");
#endif

	return OK;
        }



/* registers a timeperiod definition */
int xodtemplate_register_timeperiod(xodtemplate_timeperiod *this_timeperiod){
	timeperiod *new_timeperiod;
	timerange *new_timerange;
	int day;
	char *day_range_ptr;
	char *day_range_start_buffer;
	char *range_ptr;
	char *range_buffer;
	char *time_ptr;
	char *time_buffer;
	int hours;
	int minutes;
	unsigned long range_start_time;
	unsigned long range_end_time;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif
	

#ifdef DEBUG0
	printf("xodtemplate_register_timeperiod() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_timeperiod->register_object==FALSE)
		return OK;

	/* add the timeperiod */
	new_timeperiod=add_timeperiod(this_timeperiod->timeperiod_name,this_timeperiod->alias);

	/* return with an error if we couldn't add the timeperiod */
	if(new_timeperiod==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register timeperiod (config file '%s', line %d)\n",xodtemplate_config_file_name(this_timeperiod->_config_file),this_timeperiod->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* add all necessary timeranges to timeperiod */
	for(day=0;day<7;day++){

		if(this_timeperiod->timeranges[day]==NULL)
			continue;

		day_range_ptr=this_timeperiod->timeranges[day];
		for(day_range_start_buffer=my_strsep(&day_range_ptr,", ");day_range_start_buffer!=NULL;day_range_start_buffer=my_strsep(&day_range_ptr,", ")){

			range_ptr=day_range_start_buffer;
			range_buffer=my_strsep(&range_ptr,"-");
			if(range_buffer==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add timerange for day %d to timeperiod (No start time delimiter) (config file '%s', line %d)\n",day,xodtemplate_config_file_name(this_timeperiod->_config_file),this_timeperiod->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }

			time_ptr=range_buffer;
			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add timerange for day %d to timeperiod (No start time hours) (config file '%s', line %d)\n",day,xodtemplate_config_file_name(this_timeperiod->_config_file),this_timeperiod->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
			hours=atoi(time_buffer);
			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add timerange for day %d to timeperiod (No start time minutes) (config file '%s', line %d)\n",day,xodtemplate_config_file_name(this_timeperiod->_config_file),this_timeperiod->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
			minutes=atoi(time_buffer);

			/* calculate the range start time in seconds */
			range_start_time=(unsigned long)((minutes*60)+(hours*60*60));

			range_buffer=my_strsep(&range_ptr,"-");
			if(range_buffer==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add timerange for day %d to timeperiod (No end time delimiter) (config file '%s', line %d)\n",day,xodtemplate_config_file_name(this_timeperiod->_config_file),this_timeperiod->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }

			time_ptr=range_buffer;
			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add timerange for day %d to timeperiod (No end time hours) (config file '%s', line %d)\n",day,xodtemplate_config_file_name(this_timeperiod->_config_file),this_timeperiod->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
			hours=atoi(time_buffer);

			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add timerange for day %d to timeperiod (No end time minutes) (config file '%s', line %d)\n",day,xodtemplate_config_file_name(this_timeperiod->_config_file),this_timeperiod->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
			minutes=atoi(time_buffer);

			/* calculate the range end time in seconds */
			range_end_time=(unsigned long)((minutes*60)+(hours*3600));

			/* add the new time range to the time period */
			new_timerange=add_timerange_to_timeperiod(new_timeperiod,day,range_start_time,range_end_time);
			if(new_timerange==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add timerange for day %d to timeperiod (config file '%s', line %d)\n",day,xodtemplate_config_file_name(this_timeperiod->_config_file),this_timeperiod->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
		        }
		
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_timeperiod() end\n");
#endif

	return OK;
        }



/* registers a command definition */
int xodtemplate_register_command(xodtemplate_command *this_command){
	command *new_command;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_command() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_command->register_object==FALSE)
		return OK;

	/* add the command */
	new_command=add_command(this_command->command_name,this_command->command_line);

	/* return with an error if we couldn't add the command */
	if(new_command==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register command (config file '%s', line %d)\n",xodtemplate_config_file_name(this_command->_config_file),this_command->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_command() end\n");
#endif

	return OK;
        }



/* registers a contactgroup definition */
int xodtemplate_register_contactgroup(xodtemplate_contactgroup *this_contactgroup){
	contactgroup *new_contactgroup;
	contactgroupmember *new_contactgroupmember;
	char *contact_name;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_contactgroup() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_contactgroup->register_object==FALSE)
		return OK;

	/* add the contact group */
	new_contactgroup=add_contactgroup(this_contactgroup->contactgroup_name,this_contactgroup->alias);

	/* return with an error if we couldn't add the contactgroup */
	if(new_contactgroup==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register contactgroup (config file '%s', line %d)\n",xodtemplate_config_file_name(this_contactgroup->_config_file),this_contactgroup->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* add all members to the contact group */
	if(this_contactgroup->members==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Contactgroup has no members (config file '%s', line %d)\n",xodtemplate_config_file_name(this_contactgroup->_config_file),this_contactgroup->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	for(contact_name=strtok(this_contactgroup->members,",");contact_name!=NULL;contact_name=strtok(NULL,",")){
		strip(contact_name);
		new_contactgroupmember=add_contact_to_contactgroup(new_contactgroup,contact_name);
		if(new_contactgroupmember==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contact '%s' to contactgroup (config file '%s', line %d)\n",contact_name,xodtemplate_config_file_name(this_contactgroup->_config_file),this_contactgroup->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_contactgroup() end\n");
#endif

	return OK;
        }



/* registers a hostgroup definition */
int xodtemplate_register_hostgroup(xodtemplate_hostgroup *this_hostgroup){
	hostgroup *new_hostgroup;
	hostgroupmember *new_hostgroupmember;
	xodtemplate_hostlist *this_hostlist;
	xodtemplate_hostlist *temp_hostlist;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_hostgroup() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_hostgroup->register_object==FALSE)
		return OK;

	/* add the  host group */
	new_hostgroup=add_hostgroup(this_hostgroup->hostgroup_name,this_hostgroup->alias);

	/* return with an error if we couldn't add the hostgroup */
	if(new_hostgroup==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register hostgroup (config file '%s', line %d)\n",xodtemplate_config_file_name(this_hostgroup->_config_file),this_hostgroup->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* get list of hosts in the hostgroup */
	temp_hostlist=xodtemplate_expand_hostgroups_and_hosts(NULL,this_hostgroup->members);

	/* add all members to the host group */
	if(temp_hostlist==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not expand member hosts specified in hostgroup (config file '%s', line %d)\n",xodtemplate_config_file_name(this_hostgroup->_config_file),this_hostgroup->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	for(this_hostlist=temp_hostlist;this_hostlist;this_hostlist=this_hostlist->next){
		new_hostgroupmember=add_host_to_hostgroup(new_hostgroup,this_hostlist->host_name);
		if(new_hostgroupmember==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add host '%s' to hostgroup (config file '%s', line %d)\n",this_hostlist->host_name,xodtemplate_config_file_name(this_hostgroup->_config_file),this_hostgroup->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }
	        }
	xodtemplate_free_hostlist(temp_hostlist);
	

#ifdef DEBUG0
	printf("xodtemplate_register_hostgroup() end\n");
#endif

	return OK;
        }



/* registers a servicedependency definition */
int xodtemplate_register_servicedependency(xodtemplate_servicedependency *this_servicedependency){
	servicedependency *new_servicedependency;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_servicedependency() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_servicedependency->register_object==FALSE)
		return OK;

	/* add the servicedependency */
	if(this_servicedependency->have_execution_dependency_options==TRUE){

		new_servicedependency=add_service_dependency(this_servicedependency->dependent_host_name,this_servicedependency->dependent_service_description,this_servicedependency->host_name,this_servicedependency->service_description,EXECUTION_DEPENDENCY,this_servicedependency->fail_execute_on_ok,this_servicedependency->fail_execute_on_warning,this_servicedependency->fail_execute_on_unknown,this_servicedependency->fail_execute_on_critical);

		/* return with an error if we couldn't add the servicedependency */
		if(new_servicedependency==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register service execution dependency (config file '%s', line %d)\n",xodtemplate_config_file_name(this_servicedependency->_config_file),this_servicedependency->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }
	        }
	if(this_servicedependency->have_notification_dependency_options==TRUE){

		new_servicedependency=add_service_dependency(this_servicedependency->dependent_host_name,this_servicedependency->dependent_service_description,this_servicedependency->host_name,this_servicedependency->service_description,NOTIFICATION_DEPENDENCY,this_servicedependency->fail_notify_on_ok,this_servicedependency->fail_notify_on_warning,this_servicedependency->fail_notify_on_unknown,this_servicedependency->fail_notify_on_critical);

		/* return with an error if we couldn't add the servicedependency */
		if(new_servicedependency==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register service notification dependency (config file '%s', line %d)\n",xodtemplate_config_file_name(this_servicedependency->_config_file),this_servicedependency->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_servicedependency() end\n");
#endif

	return OK;
        }



/* registers a serviceescalation definition */
int xodtemplate_register_serviceescalation(xodtemplate_serviceescalation *this_serviceescalation){
	serviceescalation *new_serviceescalation;
	contactgroupsmember *new_contactgroupsmember;
	char *contact_group;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_serviceescalation() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_serviceescalation->register_object==FALSE)
		return OK;

	/* add the serviceescalation */
	new_serviceescalation=add_serviceescalation(this_serviceescalation->host_name,this_serviceescalation->service_description,this_serviceescalation->first_notification,this_serviceescalation->last_notification,this_serviceescalation->notification_interval);

	/* return with an error if we couldn't add the serviceescalation */
	if(new_serviceescalation==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register service escalation (config file '%s', line %d)\n",xodtemplate_config_file_name(this_serviceescalation->_config_file),this_serviceescalation->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* add the contact groups */
	if(this_serviceescalation->contact_groups==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Service escalation has no contact groups (config file '%s', line %d)\n",xodtemplate_config_file_name(this_serviceescalation->_config_file),this_serviceescalation->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	for(contact_group=strtok(this_serviceescalation->contact_groups,", ");contact_group!=NULL;contact_group=strtok(NULL,", ")){
		new_contactgroupsmember=add_contactgroup_to_serviceescalation(new_serviceescalation,contact_group);
		if(new_contactgroupsmember==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contactgroup '%s' to service escalation (config file '%s', line %d)\n",contact_group,xodtemplate_config_file_name(this_serviceescalation->_config_file),this_serviceescalation->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_serviceescalation() end\n");
#endif

	return OK;
        }



/* registers a contact definition */
int xodtemplate_register_contact(xodtemplate_contact *this_contact){
	contact *new_contact;
	char *command_name;
	commandsmember *new_commandsmember;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_contact() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_contact->register_object==FALSE)
		return OK;

	/* add the contact */
	new_contact=add_contact(this_contact->contact_name,this_contact->alias,this_contact->email,this_contact->pager,this_contact->service_notification_period,this_contact->host_notification_period,this_contact->notify_on_service_recovery,this_contact->notify_on_service_critical,this_contact->notify_on_service_warning,this_contact->notify_on_service_unknown,this_contact->notify_on_host_recovery,this_contact->notify_on_host_down,this_contact->notify_on_host_unreachable);

	/* return with an error if we couldn't add the contact */
	if(new_contact==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register contact (config file '%s', line %d)\n",xodtemplate_config_file_name(this_contact->_config_file),this_contact->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* add all the host notification commands */
	if(this_contact->host_notification_commands!=NULL){

		for(command_name=strtok(this_contact->host_notification_commands,", ");command_name!=NULL;command_name=strtok(NULL,", ")){
			new_commandsmember=add_host_notification_command_to_contact(new_contact,command_name);
			if(new_commandsmember==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add host notification command '%s' to contact (config file '%s', line %d)\n",command_name,xodtemplate_config_file_name(this_contact->_config_file),this_contact->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
		        }
	        }

	/* add all the service notification commands */
	if(this_contact->service_notification_commands!=NULL){

		for(command_name=strtok(this_contact->service_notification_commands,", ");command_name!=NULL;command_name=strtok(NULL,", ")){
			new_commandsmember=add_service_notification_command_to_contact(new_contact,command_name);
			if(new_commandsmember==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add service notification command '%s' to contact (config file '%s', line %d)\n",command_name,xodtemplate_config_file_name(this_contact->_config_file),this_contact->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_contact() end\n");
#endif

	return OK;
        }



/* registers a host definition */
int xodtemplate_register_host(xodtemplate_host *this_host){
	host *new_host;
	char *parent_host;
	hostsmember *new_hostsmember;
	contactgroupsmember *new_contactgroupsmember;
	char *contact_group;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_host() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_host->register_object==FALSE)
		return OK;

	/* add the host definition */
	new_host=add_host(this_host->host_name,this_host->alias,(this_host->address==NULL)?this_host->host_name:this_host->address,this_host->max_check_attempts,this_host->notify_on_recovery,this_host->notify_on_down,this_host->notify_on_unreachable,this_host->notification_interval,this_host->notification_period,this_host->notifications_enabled,this_host->check_command,this_host->active_checks_enabled,this_host->passive_checks_enabled,this_host->event_handler,this_host->event_handler_enabled,this_host->flap_detection_enabled,this_host->low_flap_threshold,this_host->high_flap_threshold,this_host->stalk_on_up,this_host->stalk_on_down,this_host->stalk_on_unreachable,this_host->process_perf_data,this_host->failure_prediction_enabled,this_host->failure_prediction_options,this_host->retain_status_information,this_host->retain_nonstatus_information,this_host->obsess_over_host);

	/* return with an error if we couldn't add the host */
	if(new_host==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register host (config file '%s', line %d)\n",xodtemplate_config_file_name(this_host->_config_file),this_host->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* add the parent hosts */
	if(this_host->parents!=NULL){

		for(parent_host=strtok(this_host->parents,",");parent_host!=NULL;parent_host=strtok(NULL,",")){
			strip(parent_host);
			new_hostsmember=add_parent_host_to_host(new_host,parent_host);
			if(new_hostsmember==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add parent host '%s' to host (config file '%s', line %d)\n",parent_host,xodtemplate_config_file_name(this_host->_config_file),this_host->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
		        }
	        }

	/* add all contact groups to the host group */
	if(this_host->contact_groups!=NULL){

		for(contact_group=strtok(this_host->contact_groups,",");contact_group!=NULL;contact_group=strtok(NULL,",")){

			strip(contact_group);
			new_contactgroupsmember=add_contactgroup_to_host(new_host,contact_group);
			if(new_contactgroupsmember==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contactgroup '%s' to host (config file '%s', line %d)\n",contact_group,xodtemplate_config_file_name(this_host->_config_file),this_host->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
			        }
	                }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_host() end\n");
#endif

	return OK;
        }



/* registers a service definition */
int xodtemplate_register_service(xodtemplate_service *this_service){
	service *new_service;
	contactgroupsmember *new_contactgroupsmember;
	char *contactgroup_name;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_service() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_service->register_object==FALSE)
		return OK;

	/* add the service */
	new_service=add_service(this_service->host_name,this_service->service_description,this_service->check_period,this_service->max_check_attempts,this_service->parallelize_check,this_service->passive_checks_enabled,this_service->normal_check_interval,this_service->retry_check_interval,this_service->notification_interval,this_service->notification_period,this_service->notify_on_recovery,this_service->notify_on_unknown,this_service->notify_on_warning,this_service->notify_on_critical,this_service->notifications_enabled,this_service->is_volatile,this_service->event_handler,this_service->event_handler_enabled,this_service->check_command,this_service->active_checks_enabled,this_service->flap_detection_enabled,this_service->low_flap_threshold,this_service->high_flap_threshold,this_service->stalk_on_ok,this_service->stalk_on_warning,this_service->stalk_on_unknown,this_service->stalk_on_critical,this_service->process_perf_data,this_service->failure_prediction_enabled,this_service->failure_prediction_options,this_service->check_freshness,this_service->freshness_threshold,this_service->retain_status_information,this_service->retain_nonstatus_information,this_service->obsess_over_service);

	/* return with an error if we couldn't add the service */
	if(new_service==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register service (config file '%s', line %d)\n",xodtemplate_config_file_name(this_service->_config_file),this_service->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* add all the contact groups to the service */
	if(this_service->contact_groups!=NULL){

		for(contactgroup_name=strtok(this_service->contact_groups,",");contactgroup_name!=NULL;contactgroup_name=strtok(NULL,",")){

			/* add this contactgroup to the service definition */
			strip(contactgroup_name);
			new_contactgroupsmember=add_contactgroup_to_service(new_service,contactgroup_name);

			/* stop adding contact groups if we ran into an error */
			if(new_contactgroupsmember==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contact group '%s' to service (config file '%s', line %d)\n",contactgroup_name,xodtemplate_config_file_name(this_service->_config_file),this_service->_start_line);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return ERROR;
		                }
		        }
	        }


#ifdef DEBUG0
	printf("xodtemplate_register_service() end\n");
#endif

	return OK;
        }



/* registers a hostdependency definition */
int xodtemplate_register_hostdependency(xodtemplate_hostdependency *this_hostdependency){
	hostdependency *new_hostdependency;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_hostdependency() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_hostdependency->register_object==FALSE)
		return OK;

	/* add the hostdependency */
	if(this_hostdependency->have_notification_dependency_options==TRUE){

		new_hostdependency=add_host_dependency(this_hostdependency->dependent_host_name,this_hostdependency->host_name,NOTIFICATION_DEPENDENCY,this_hostdependency->fail_notify_on_up,this_hostdependency->fail_notify_on_down,this_hostdependency->fail_notify_on_unreachable);

		/* return with an error if we couldn't add the hostdependency */
		if(new_hostdependency==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register host notification dependency (config file '%s', line %d)\n",xodtemplate_config_file_name(this_hostdependency->_config_file),this_hostdependency->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
                        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_hostdependency() end\n");
#endif

	return OK;
        }



/* registers a hostescalation definition */
int xodtemplate_register_hostescalation(xodtemplate_hostescalation *this_hostescalation){
	hostescalation *new_hostescalation;
	contactgroupsmember *new_contactgroupsmember;
	char *contact_group;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_hostescalation() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_hostescalation->register_object==FALSE)
		return OK;

	/* add the hostescalation */
	new_hostescalation=add_hostescalation(this_hostescalation->host_name,this_hostescalation->first_notification,this_hostescalation->last_notification,this_hostescalation->notification_interval);

	/* return with an error if we couldn't add the hostescalation */
	if(new_hostescalation==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register host escalation (config file '%s', line %d)\n",xodtemplate_config_file_name(this_hostescalation->_config_file),this_hostescalation->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

	/* add the contact groups */
	if(this_hostescalation->contact_groups==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Host escalation has no contact groups (config file '%s', line %d)\n",xodtemplate_config_file_name(this_hostescalation->_config_file),this_hostescalation->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }
	for(contact_group=strtok(this_hostescalation->contact_groups,", ");contact_group!=NULL;contact_group=strtok(NULL,", ")){
		new_contactgroupsmember=add_contactgroup_to_hostescalation(new_hostescalation,contact_group);
		if(new_contactgroupsmember==NULL){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not add contactgroup '%s' to host escalation (config file '%s', line %d)\n",contact_group,xodtemplate_config_file_name(this_hostescalation->_config_file),this_hostescalation->_start_line);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			return ERROR;
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_hostescalation() end\n");
#endif

	return OK;
        }



/* registers a hostextinfo definition */
int xodtemplate_register_hostextinfo(xodtemplate_hostextinfo *this_hostextinfo){
	hostextinfo *new_hostextinfo;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_hostextinfo() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_hostextinfo->register_object==FALSE)
		return OK;

	/* register the extended host object */
	new_hostextinfo=add_hostextinfo(this_hostextinfo->host_name,this_hostextinfo->notes_url,this_hostextinfo->icon_image,this_hostextinfo->vrml_image,this_hostextinfo->statusmap_image,this_hostextinfo->icon_image_alt,this_hostextinfo->x_2d,this_hostextinfo->y_2d,this_hostextinfo->x_3d,this_hostextinfo->y_3d,this_hostextinfo->z_3d,this_hostextinfo->have_2d_coords,this_hostextinfo->have_3d_coords);

	/* return with an error if we couldn't add the definition */
	if(new_hostextinfo==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register host extended information (config file '%s', line %d)\n",xodtemplate_config_file_name(this_hostextinfo->_config_file),this_hostextinfo->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_hostextinfo() end\n");
#endif

	return OK;
        }



/* registers a serviceextinfo definition */
int xodtemplate_register_serviceextinfo(xodtemplate_serviceextinfo *this_serviceextinfo){
	serviceextinfo *new_serviceextinfo;
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_register_serviceextinfo() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_serviceextinfo->register_object==FALSE)
		return OK;

	/* register the extended service object */
	new_serviceextinfo=add_serviceextinfo(this_serviceextinfo->host_name,this_serviceextinfo->service_description,this_serviceextinfo->notes_url,this_serviceextinfo->icon_image,this_serviceextinfo->icon_image_alt);

	/* return with an error if we couldn't add the definition */
	if(new_serviceextinfo==NULL){
#ifdef NSCORE
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not register service extended information (config file '%s', line %d)\n",xodtemplate_config_file_name(this_serviceextinfo->_config_file),this_serviceextinfo->_start_line);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
		return ERROR;
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_serviceextinfo() end\n");
#endif

	return OK;
        }




/******************************************************************/
/********************** CLEANUP FUNCTIONS *************************/
/******************************************************************/

/* frees memory */
int xodtemplate_free_memory(void){
	xodtemplate_timeperiod *this_timeperiod;
	xodtemplate_timeperiod *next_timeperiod;
	xodtemplate_command *this_command;
	xodtemplate_command *next_command;
	xodtemplate_contactgroup *this_contactgroup;
	xodtemplate_contactgroup *next_contactgroup;
	xodtemplate_hostgroup *this_hostgroup;
	xodtemplate_hostgroup *next_hostgroup;
	xodtemplate_servicedependency *this_servicedependency;
	xodtemplate_servicedependency *next_servicedependency;
	xodtemplate_serviceescalation *this_serviceescalation;
	xodtemplate_serviceescalation *next_serviceescalation;
	xodtemplate_contact *this_contact;
	xodtemplate_contact *next_contact;
	xodtemplate_host *this_host;
	xodtemplate_host *next_host;
	xodtemplate_service *this_service;
	xodtemplate_service *next_service;
	xodtemplate_hostdependency *this_hostdependency;
	xodtemplate_hostdependency *next_hostdependency;
	xodtemplate_hostescalation *this_hostescalation;
	xodtemplate_hostescalation *next_hostescalation;
	xodtemplate_hostextinfo *this_hostextinfo;
	xodtemplate_hostextinfo *next_hostextinfo;
	xodtemplate_serviceextinfo *this_serviceextinfo;
	xodtemplate_serviceextinfo *next_serviceextinfo;
	int x;

#ifdef DEBUG0
	printf("xodtemplate_free_memory() start\n");
#endif

	/* free memory allocated to timeperiod list */
	for(this_timeperiod=xodtemplate_timeperiod_list;this_timeperiod!=NULL;this_timeperiod=next_timeperiod){
		next_timeperiod=this_timeperiod->next;
		free(this_timeperiod->template);
		free(this_timeperiod->name);
		free(this_timeperiod->timeperiod_name);
		free(this_timeperiod->alias);
		for(x=0;x<7;x++)
			free(this_timeperiod->timeranges[x]);
		free(this_timeperiod);
	        }
	xodtemplate_timeperiod_list=NULL;

	/* free memory allocated to command list */
	for(this_command=xodtemplate_command_list;this_command!=NULL;this_command=next_command){
		next_command=this_command->next;
		free(this_command->template);
		free(this_command->name);
		free(this_command->command_name);
		free(this_command->command_line);
		free(this_command);
	        }
	xodtemplate_command_list=NULL;

	/* free memory allocated to contactgroup list */
	for(this_contactgroup=xodtemplate_contactgroup_list;this_contactgroup!=NULL;this_contactgroup=next_contactgroup){
		next_contactgroup=this_contactgroup->next;
		free(this_contactgroup->template);
		free(this_contactgroup->name);
		free(this_contactgroup->contactgroup_name);
		free(this_contactgroup->alias);
		free(this_contactgroup->members);
		free(this_contactgroup);
	        }
	xodtemplate_contactgroup_list=NULL;

	/* free memory allocated to hostgroup list */
	for(this_hostgroup=xodtemplate_hostgroup_list;this_hostgroup!=NULL;this_hostgroup=next_hostgroup){
		next_hostgroup=this_hostgroup->next;
		free(this_hostgroup->template);
		free(this_hostgroup->name);
		free(this_hostgroup->hostgroup_name);
		free(this_hostgroup->alias);
		free(this_hostgroup->members);
		free(this_hostgroup);
	        }
	xodtemplate_hostgroup_list=NULL;

	/* free memory allocated to servicedependency list */
	for(this_servicedependency=xodtemplate_servicedependency_list;this_servicedependency!=NULL;this_servicedependency=next_servicedependency){
		next_servicedependency=this_servicedependency->next;
		free(this_servicedependency->template);
		free(this_servicedependency->name);
		free(this_servicedependency->hostgroup_name);
		free(this_servicedependency->host_name);
		free(this_servicedependency->service_description);
		free(this_servicedependency->dependent_hostgroup_name);
		free(this_servicedependency->dependent_host_name);
		free(this_servicedependency->dependent_service_description);
		free(this_servicedependency);
	        }
	xodtemplate_servicedependency_list=NULL;

	/* free memory allocated to serviceescalation list */
	for(this_serviceescalation=xodtemplate_serviceescalation_list;this_serviceescalation!=NULL;this_serviceescalation=next_serviceescalation){
		next_serviceescalation=this_serviceescalation->next;
		free(this_serviceescalation->template);
		free(this_serviceescalation->name);
		free(this_serviceescalation->hostgroup_name);
		free(this_serviceescalation->host_name);
		free(this_serviceescalation->service_description);
		free(this_serviceescalation->contact_groups);
		free(this_serviceescalation);
	        }
	xodtemplate_serviceescalation_list=NULL;

	/* free memory allocated to contact list */
	for(this_contact=xodtemplate_contact_list;this_contact!=NULL;this_contact=next_contact){
		next_contact=this_contact->next;
		free(this_contact->template);
		free(this_contact->name);
		free(this_contact->contact_name);
		free(this_contact->alias);
		free(this_contact->email);
		free(this_contact->pager);
		free(this_contact->service_notification_period);
		free(this_contact->service_notification_commands);
		free(this_contact->host_notification_period);
		free(this_contact->host_notification_commands);
		free(this_contact);
	        }
	xodtemplate_contact_list=NULL;

	/* free memory allocated to host list */
	for(this_host=xodtemplate_host_list;this_host!=NULL;this_host=next_host){
		next_host=this_host->next;
		free(this_host->template);
		free(this_host->name);
		free(this_host->host_name);
		free(this_host->alias);
		free(this_host->address);
		free(this_host->parents);
		free(this_host->hostgroups);
		free(this_host->check_command);
		free(this_host->event_handler);
		free(this_host->contact_groups);
		free(this_host->notification_period);
		free(this_host->failure_prediction_options);
		free(this_host);
	        }
	xodtemplate_host_list=NULL;

	/* free memory allocated to service list (chained hash) */
	if(xodtemplate_service_list){

		for(x=0;x<SERVICES_HASHSLOTS;x++){
			for(this_service=xodtemplate_service_list[x];this_service!=NULL;this_service=next_service){

				next_service=this_service->next;
				free(this_service->template);
				free(this_service->name);
				free(this_service->hostgroup_name);
				free(this_service->host_name);
				free(this_service->service_description);
				free(this_service->check_command);
				free(this_service->check_period);
				free(this_service->event_handler);
				free(this_service->notification_period);
				free(this_service->contact_groups);
				free(this_service->failure_prediction_options);
				free(this_service);
			        }
		        }
		free(xodtemplate_service_list);
	        }
	xodtemplate_service_list=NULL;

	/* free memory allocated to hostdependency list */
	for(this_hostdependency=xodtemplate_hostdependency_list;this_hostdependency!=NULL;this_hostdependency=next_hostdependency){
		next_hostdependency=this_hostdependency->next;
		free(this_hostdependency->template);
		free(this_hostdependency->name);
		free(this_hostdependency->hostgroup_name);
		free(this_hostdependency->dependent_hostgroup_name);
		free(this_hostdependency->host_name);
		free(this_hostdependency->dependent_host_name);
		free(this_hostdependency);
	        }
	xodtemplate_hostdependency_list=NULL;

	/* free memory allocated to hostescalation list */
	for(this_hostescalation=xodtemplate_hostescalation_list;this_hostescalation!=NULL;this_hostescalation=next_hostescalation){
		next_hostescalation=this_hostescalation->next;
		free(this_hostescalation->template);
		free(this_hostescalation->name);
		free(this_hostescalation->hostgroup_name);
		free(this_hostescalation->host_name);
		free(this_hostescalation->contact_groups);
		free(this_hostescalation);
	        }
	xodtemplate_hostescalation_list=NULL;

	/* free memory allocated to hostextinfo list */
	for(this_hostextinfo=xodtemplate_hostextinfo_list;this_hostextinfo!=NULL;this_hostextinfo=next_hostextinfo){
		next_hostextinfo=this_hostextinfo->next;
		free(this_hostextinfo->template);
		free(this_hostextinfo->name);
		free(this_hostextinfo->host_name);
		free(this_hostextinfo->hostgroup_name);
		free(this_hostextinfo->notes_url);
		free(this_hostextinfo->icon_image);
		free(this_hostextinfo->icon_image_alt);
		free(this_hostextinfo->vrml_image);
		free(this_hostextinfo->statusmap_image);
		free(this_hostextinfo);
	        }
	xodtemplate_hostextinfo_list=NULL;

	/* free memory allocated to serviceextinfo list */
	for(this_serviceextinfo=xodtemplate_serviceextinfo_list;this_serviceextinfo!=NULL;this_serviceextinfo=next_serviceextinfo){
		next_serviceextinfo=this_serviceextinfo->next;
		free(this_serviceextinfo->template);
		free(this_serviceextinfo->name);
		free(this_serviceextinfo->host_name);
		free(this_serviceextinfo->hostgroup_name);
		free(this_serviceextinfo->service_description);
		free(this_serviceextinfo->notes_url);
		free(this_serviceextinfo->icon_image);
		free(this_serviceextinfo->icon_image_alt);
		free(this_serviceextinfo);
	        }
	xodtemplate_serviceextinfo_list=NULL;

	/* free memory for the config file names */
	for(x=0;x<xodtemplate_current_config_file;x++)
		free(xodtemplate_config_files[x]);
	free(xodtemplate_config_files);
	xodtemplate_current_config_file=0;


#ifdef DEBUG0
	printf("xodtemplate_free_memory() end\n");
#endif

	return OK;
        }


/* frees memory allocated to a temporary host list */
int xodtemplate_free_hostlist(xodtemplate_hostlist *temp_list){
	xodtemplate_hostlist *this_hostlist;
	xodtemplate_hostlist *next_hostlist;

#ifdef DEBUG0
	printf("xodtemplate_free_hostlist() start\n");
#endif

	/* free memory allocated to host name list */
	for(this_hostlist=temp_list;this_hostlist!=NULL;this_hostlist=next_hostlist){
		next_hostlist=this_hostlist->next;
		free(this_hostlist->host_name);
		free(this_hostlist);
	        }

	temp_list=NULL;

#ifdef DEBUG0
	printf("xodtemplate_free_hostlist() end\n");
#endif

	return OK;
        }


/* frees memory allocated to a temporary service list */
int xodtemplate_free_servicelist(xodtemplate_servicelist *temp_list){
	xodtemplate_servicelist *this_servicelist;
	xodtemplate_servicelist *next_servicelist;

#ifdef DEBUG0
	printf("xodtemplate_free_servicelist() start\n");
#endif

	/* free memory allocated to service name list */
	for(this_servicelist=temp_list;this_servicelist!=NULL;this_servicelist=next_servicelist){
		next_servicelist=this_servicelist->next;
		free(this_servicelist->service_description);
		free(this_servicelist);
	        }

	temp_list=NULL;

#ifdef DEBUG0
	printf("xodtemplate_free_servicelist() end\n");
#endif

	return OK;
        }



/******************************************************************/
/********************** UTILITY FUNCTIONS *************************/
/******************************************************************/

/* expands a comma-delimited list of hostgroups and/or hosts to member host names */
xodtemplate_hostlist *xodtemplate_expand_hostgroups_and_hosts(char *hostgroups,char *hosts){
	xodtemplate_hostlist *temp_list;
	xodtemplate_hostlist *new_list;
	xodtemplate_hostgroup *temp_hostgroup;
	xodtemplate_host *temp_host;
	char *hostgroup_names;
	char *host_names;
	char *host_name;
	char *temp_ptr;
	char *host_name_ptr;
#ifdef USE_REGEXP_MATCHING
	regex_t preg;
	int found_match;
#endif
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_expand_hostgroups() start\n");
#endif

	temp_list=NULL;

	/* process list of hostgroups... */
	if(hostgroups!=NULL){

		/* allocate memory for hostgroup name list */
		hostgroup_names=strdup(hostgroups);
		if(hostgroup_names==NULL)
			return temp_list;

		for(temp_ptr=strtok(hostgroup_names,",");temp_ptr;temp_ptr=strtok(NULL,",")){

			/* strip trailing spaces */
			strip(temp_ptr);

#ifdef USE_REGEXP_MATCHING

			/* compile regular expression */
			if(regcomp(&preg,temp_ptr,0)){
				free(hostgroup_names);
				return NULL;
		                }
			
			/* test match against all hostgroup names */
			for(temp_hostgroup=xodtemplate_hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){

				if(temp_hostgroup->name==NULL)
					continue;

				/* break out if this hostgroup matched the expression */
				if(!regexec(&preg,temp_hostgroup->name,0,NULL,0))
					break;
			        }

			/* free memory allocated to compiled regexp */
			regfree(&preg);
	
#else

			/* find the hostgroup */
			temp_hostgroup=xodtemplate_find_real_hostgroup(temp_ptr);

#endif

			if(temp_hostgroup==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not find any hostgroup matching '%s'\n",temp_ptr);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				free(hostgroup_names);
				return NULL;
		                }

			/* save a copy of the hosts */
			host_names=strdup(temp_hostgroup->members);
			if(host_names==NULL){
				free(hostgroup_names);
				return temp_list;
	                        }

			/* process all hosts that belong to the hostgroup */
			host_name_ptr=host_names;
			for(host_name=my_strsep(&host_name_ptr,",");host_name!=NULL;host_name=my_strsep(&host_name_ptr,",")){

				/* strip trailing spaces from host name */
				strip(host_name);

				/* skip this host if its already in the list */
				for(new_list=temp_list;new_list;new_list=new_list->next)
					if(!strcmp(host_name,new_list->host_name))
						break;
				if(new_list!=NULL)
					continue;

				/* allocate memory for a new list item */
				new_list=(xodtemplate_hostlist *)malloc(sizeof(xodtemplate_hostlist));
				if(new_list==NULL){
					free(host_names);
					free(hostgroup_names);
					return temp_list;
			                }

				/* save the host name */
				new_list->host_name=strdup(host_name);
				if(new_list->host_name==NULL){
					free(host_names);
					free(hostgroup_names);
					return temp_list;
			                }

				/* add new item to head of list */
				new_list->next=temp_list;
				temp_list=new_list;
		                }

			free(host_names);
	                }

		free(hostgroup_names);
	        }

	/* process list of hosts... */
	if(hosts!=NULL){

		/* allocate memory for host name list */
		host_names=strdup(hosts);
		if(host_names==NULL)
			return temp_list;

#ifdef USE_REGEXP_MATCHING

		for(temp_ptr=strtok(host_names,",");temp_ptr;temp_ptr=strtok(NULL,",")){

			/* strip trailing spaces */
			strip(temp_ptr);

			/* compile regular expression */
			if(regcomp(&preg,temp_ptr,0)){
				free(host_names);
				return NULL;
		                }
			
			/* test match against all host names */
			found_match=FALSE;
			for(temp_host=xodtemplate_host_list;temp_host!=NULL;temp_host=temp_host->next){

				if(temp_host->host_name==NULL)
					continue;

				/* skip this host if it doesn't match the expression */
				if(regexec(&preg,temp_host->host_name,0,NULL,0))
					continue;
				else
					found_match=TRUE;

				/* skip this host if its already in the list */
				for(new_list=temp_list;new_list;new_list=new_list->next)
					if(!strcmp(temp_host->host_name,new_list->host_name))
						break;
				if(new_list!=NULL)
					continue;

				/* allocate memory for a new list item */
				new_list=(xodtemplate_hostlist *)malloc(sizeof(xodtemplate_hostlist));
				if(new_list==NULL){
					regfree(&preg);
					free(host_names);
					return temp_list;
		                        }

				/* save the host name */
				new_list->host_name=strdup(temp_host->host_name);
				if(new_list->host_name==NULL){
					regfree(&preg);
					free(host_names);
					return temp_list;
			                }
				
				/* add new item to head of list */
				new_list->next=temp_list;
				temp_list=new_list;
	                        }

			/* free memory allocated to compiled regexp */
			regfree(&preg);

			/* we didn't find a match */
			if(found_match==FALSE){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not find a host matching '%s'\n",temp_ptr);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				return NULL;
			        }
		        }

#else

		/* return list of all hosts */
		if(!strcmp(host_names,"*")){

			for(temp_host=xodtemplate_host_list;temp_host!=NULL;temp_host=temp_host->next){

				if(temp_host->host_name==NULL)
					continue;

				/* allocate memory for a new list item */
				new_list=(xodtemplate_hostlist *)malloc(sizeof(xodtemplate_hostlist));
				if(new_list==NULL){
					free(host_names);
					return temp_list;
			                }

				/* save the host name */
				new_list->host_name=strdup(temp_host->host_name);
				if(new_list->host_name==NULL){
					free(host_names);
					return temp_list;
	                                }

				/* add new item to head of list */
				new_list->next=temp_list;
				temp_list=new_list;
		                }
	                }

		/* else lookup individual hosts */
		else{

			for(temp_ptr=strtok(host_names,",");temp_ptr;temp_ptr=strtok(NULL,",")){
			
				/* strip trailing spaces */
				strip(temp_ptr);
			
				/* find the host */
				temp_host=xodtemplate_find_real_host(temp_ptr);
				if(temp_host==NULL){
#ifdef NSCORE
					snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not find host '%s'\n",temp_ptr);
					temp_buffer[sizeof(temp_buffer)-1]='\x0';
					write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
					free(host_names);
					return NULL;
		                        }

				/* skip this host if its already in the list */
				for(new_list=temp_list;new_list;new_list=new_list->next)
					if(!strcmp(new_list->host_name,temp_ptr))
						break;
				if(new_list)
					continue;

				/* allocate memory for a new list item */
				new_list=(xodtemplate_hostlist *)malloc(sizeof(xodtemplate_hostlist));
				if(new_list==NULL){
					free(host_names);
					return temp_list;
			                }

				/* save the host name */
				new_list->host_name=strdup(temp_host->host_name);
				if(new_list->host_name==NULL){
					free(host_names);
					return temp_list;
	                                }

				/* add new item to head of list */
				new_list->next=temp_list;
				temp_list=new_list;
                                }
	                }
#endif

		free(host_names);
	        }

#ifdef DEBUG0
	printf("xodtemplate_expand_hostgroups_and_hosts() end\n");
#endif

	return temp_list;
        }


/* expands a comma-delimited list of service descriptions */
xodtemplate_servicelist *xodtemplate_expand_services(char *host,char *services){
	xodtemplate_servicelist *temp_list;
	xodtemplate_servicelist *new_list;
	xodtemplate_service *temp_service;
	char *service_names;
	char *service_name;
	char *temp_ptr;
	void *xod_svc_cursor;
#ifdef USE_REGEXP_MATCHING
	regex_t preg;
	int found_match;
#endif
#ifdef NSCORE
	char temp_buffer[MAX_XODTEMPLATE_INPUT_BUFFER];
#endif

#ifdef DEBUG0
	printf("xodtemplate_expand_services() start\n");
#endif

	if(host==NULL || services==NULL)
		return NULL;

	temp_list=NULL;

	/* allocate memory for service name list */
	service_names=strdup(services);
	if(service_names==NULL)
		return NULL;

#ifdef USE_REGEXP_MATCHING

	for(temp_ptr=strtok(service_names,",");temp_ptr;temp_ptr=strtok(NULL,",")){

		/* strip trailing spaces */
		strip(temp_ptr);

		/* compile regular expression */
		if(regcomp(&preg,temp_ptr,0)){
			free(service_names);
			return NULL;
	                }

		/* test match against all services on this host */
		found_match=FALSE;
		xod_svc_cursor=get_xodtemplate_service_cursor();
		while(temp_service=get_next_xodtemplate_service(xod_svc_cursor)){

			if(temp_service->host_name==NULL || temp_service->service_description==NULL)
				continue;

			if(strcmp(temp_service->host_name,host))
				continue;

			/* skip this service if it doesn't match the expression */
			if(regexec(&preg,temp_service->service_description,0,NULL,0))
				continue;
			else
				found_match=TRUE;

			/* skip this service if its already in the list */
			for(new_list=temp_list;new_list;new_list=new_list->next)
				if(!strcmp(new_list->service_description,temp_ptr))
					break;
			if(new_list)
				continue;

			/* allocate memory for a new list item */
			new_list=(xodtemplate_servicelist *)malloc(sizeof(xodtemplate_servicelist));
			if(new_list==NULL){
				free(service_names);
				return temp_list;
			        }

			/* save the service description */
			new_list->service_description=strdup(temp_service->service_description);
			if(new_list->service_description==NULL){
				free(service_names);
				return temp_list;
                                }

			/* add new item to head of list */
			new_list->next=temp_list;
			temp_list=new_list;
		        }

		/* we didn't find a match */
		if(found_match==FALSE){
#ifdef NSCORE
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not find a service matching '%s'\n",temp_ptr);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
			regfree(&preg);
			free(service_names);
			return NULL;
		        }

		/* free memory allocated to compiled regexp */
		regfree(&preg);
	        }

#else

	/* return list of all services on the host */
	if(!strcmp(service_names,"*")){

		xod_svc_cursor=get_xodtemplate_service_cursor();
		while(temp_service=get_next_xodtemplate_service(xod_svc_cursor)){

			if(temp_service->host_name==NULL || temp_service->service_description==NULL)
				continue;

			if(strcmp(temp_service->host_name,host))
				continue;

			/* allocate memory for a new list item */
			new_list=(xodtemplate_servicelist *)malloc(sizeof(xodtemplate_servicelist));
			if(new_list==NULL){
				free(service_names);
				return temp_list;
		                }

			/* save the service name */
			new_list->service_description=strdup(temp_service->service_description);
			if(new_list->service_description==NULL){
				free(service_names);
				return temp_list;
                                }

			/* add new item to head of list */
			new_list->next=temp_list;
			temp_list=new_list;
	                }

		free_xodtemplate_service_cursor(xod_svc_cursor);
                }

	/* else lookup individual services */
	else{

		for(temp_ptr=strtok(service_names,",");temp_ptr;temp_ptr=strtok(NULL,",")){

			/* strip trailing spaces */
			strip(temp_ptr);
			
			/* find the service */
			temp_service=xodtemplate_find_real_service(host,temp_ptr);
			if(temp_service==NULL){
#ifdef NSCORE
				snprintf(temp_buffer,sizeof(temp_buffer)-1,"Error: Could not find service '%s' on host '%s'\n",temp_ptr,host);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_CONFIG_ERROR,TRUE);
#endif
				free(service_names);
				return NULL;
			        }

			/* skip this service if its already in the list */
			for(new_list=temp_list;new_list;new_list=new_list->next)
				if(!strcmp(new_list->service_description,temp_ptr))
					break;
			if(new_list)
				continue;

			/* allocate memory for a new list item */
			new_list=(xodtemplate_servicelist *)malloc(sizeof(xodtemplate_servicelist));
			if(new_list==NULL){
				free(service_names);
				return temp_list;
			        }

			/* save the service description */
			new_list->service_description=strdup(temp_service->service_description);
			if(new_list->service_description==NULL){
				free(service_names);
				return temp_list;
                                }

			/* add new item to head of list */
			new_list->next=temp_list;
			temp_list=new_list;
		        }

	        }
#endif

	free(service_names);

#ifdef DEBUG0
	printf("xodtemplate_expand_services() end\n");
#endif

	return temp_list;
        }



/* returns the name of a numbered config file */
char *xodtemplate_config_file_name(int config_file){

	if(config_file<=xodtemplate_current_config_file)
		return xodtemplate_config_files[config_file-1];

	return "?";
        }



/******************************************************************/
/************************ HASH FUNCTIONS **************************/
/******************************************************************/

int compare_xodtemplate_service(xodtemplate_service *xod_service,const char *service_name){

	if(xod_service->name && service_name)
		return strcmp(xod_service->name,service_name);
	else if(!(xod_service->name) && !service_name)
		return 0;
	else if(!(xod_service->name))
		return 1;

	return -1;
        }


int xodtemplate_service_comes_after(xodtemplate_service *xod_service, const char *service_name){

	return compare_xodtemplate_service(xod_service,service_name) < 0;
        }


int xodtemplate_add_service_allocated(xodtemplate_service *new_service){
	xodtemplate_service *tempsvc, *lastpointer;
	int hashslot=hashfunc1(new_service->name,SERVICES_HASHSLOTS);

	if(xodtemplate_service_list==NULL){
		int i;

		xodtemplate_service_list=(xodtemplate_service **)malloc(sizeof(xodtemplate_service *)*SERVICES_HASHSLOTS);
		if(xodtemplate_service_list==NULL)
			return 0;

		for(i=0;i<SERVICES_HASHSLOTS;i++)
			xodtemplate_service_list[i]=NULL;
	        }

	if(!new_service)
		return 0;

	lastpointer=NULL;
	for(tempsvc=xodtemplate_service_list[hashslot];tempsvc && xodtemplate_service_comes_after(tempsvc,new_service->name);tempsvc=tempsvc->next)
		lastpointer=tempsvc;

	if(lastpointer)
		lastpointer->next=new_service;
	else
		xodtemplate_service_list[hashslot]=new_service;
	new_service->next=tempsvc;

	return 1;
        }


int xodtemplate_remove_pointer(xodtemplate_service *to_remove){
	int hashslot=hashfunc1(to_remove->name,SERVICES_HASHSLOTS);
	xodtemplate_service *tempsvc, *lastpointer;

	if(xodtemplate_service_list==NULL)
		return 0;

	if(!to_remove)
		return 0;

	lastpointer=NULL;
	for(tempsvc=xodtemplate_service_list[hashslot];tempsvc && (tempsvc!=to_remove);tempsvc=tempsvc->next)
		lastpointer=tempsvc;

	if(tempsvc!=to_remove)
		return 0;

	if(lastpointer)
		lastpointer->next=to_remove->next;
	else
		xodtemplate_service_list[hashslot]=to_remove->next;

	return 1;
        }


int xodtemplate_rename_service(xodtemplate_service *new_service,const char *newname){

	/* remove the pointer from its old position */
	if(!xodtemplate_remove_pointer(new_service))
		return 0;

	if(new_service->name)
		free(new_service->name);

	new_service->name=strdup(newname);
	if(new_service->name==NULL)
		return 0;

	/* re-add the pointer */
	return xodtemplate_add_service_allocated(new_service);
        }


void *get_xodtemplate_service_cursor(void){
	xodtemplate_service_cursor *retval;

	if(!(retval=malloc(sizeof(xodtemplate_service_cursor))))
		return NULL;

	retval->xodtemplate_service_iterator=0;
	retval->current_xodtemplate_service=NULL;

	return retval;
        }


void free_xodtemplate_service_cursor(void *cursor){

	if(cursor)
		free(cursor);
        }

