/*****************************************************************************
 *
 * XDDDEFAULT.C - Default scheduled downtime data routines for Nagios
 *
 * Copyright (c) 2001-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-07-2002
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
#include "../common/locations.h"
#include "../common/downtime.h"

#ifdef NSCORE
#include "../common/objects.h"
#include "../base/nagios.h"
#endif

#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/
#include "xdddefault.h"



char xdddefault_downtime_file[MAX_FILENAME_LENGTH]="";
char xdddefault_temp_file[MAX_FILENAME_LENGTH]="";

#ifdef NSCORE
int current_downtime_id=0;
#endif




/******************************************************************/
/***************** COMMON CONFIG INITIALIZATION  ******************/
/******************************************************************/

/* grab configuration information from appropriate config file(s) */
int xdddefault_grab_config_info(char *config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	FILE *fp;
#ifdef NSCGI
	FILE *fp2;
	char *temp_buffer;
#endif

	/*** CORE PASSES IN MAIN CONFIG FILE, CGIS PASS IN CGI CONFIG FILE! ***/

	/* initialize the location of the downtime and temp files */
	strncpy(xdddefault_downtime_file,DEFAULT_DOWNTIME_FILE,sizeof(xdddefault_downtime_file)-1);
	strncpy(xdddefault_temp_file,DEFAULT_TEMP_FILE,sizeof(xdddefault_temp_file)-1);
	xdddefault_downtime_file[sizeof(xdddefault_downtime_file)-1]='\x0';
	xdddefault_temp_file[sizeof(xdddefault_temp_file)-1]='\x0';

	/* open the config file for reading */
	fp=fopen(config_file,"r");
	if(fp==NULL)
		return ERROR;

	/* read in all lines from the config file */
	for(fgets(input_buffer,sizeof(input_buffer)-1,fp);!feof(fp);fgets(input_buffer,sizeof(input_buffer)-1,fp)){

		/* skip blank lines and comments */
		if(input_buffer[0]=='#' || input_buffer[0]=='\x0' || input_buffer[0]=='\n' || input_buffer[0]=='\r')
			continue;

		strip(input_buffer);

#ifdef NSCGI
		/* CGI needs to find and read contents of main config file, since it was passed the name of the CGI config file */
		if(strstr(input_buffer,"main_config_file")==input_buffer){

			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			
			fp2=fopen(temp_buffer,"r");
			if(fp2==NULL)
				continue;

			/* read in all lines from the main config file */
			for(fgets(input_buffer,sizeof(input_buffer)-1,fp2);!feof(fp2);fgets(input_buffer,sizeof(input_buffer)-1,fp2)){

				/* skip blank lines and comments */
				if(input_buffer[0]=='#' || input_buffer[0]=='\x0' || input_buffer[0]=='\n' || input_buffer[0]=='\r')
					continue;

				strip(input_buffer);

				xdddefault_grab_config_directives(input_buffer);
			        }

			fclose(fp2);
		        }
#endif

#ifdef NSCORE
		/* core read variables directly from the main config file */
		xdddefault_grab_config_directives(input_buffer);
#endif
	        }

	fclose(fp);

	/* we didn't find the downtime file */
	if(!strcmp(xdddefault_downtime_file,""))
		return ERROR;

	/* we didn't find the temp file */
	if(!strcmp(xdddefault_temp_file,""))
		return ERROR;

	return OK;
        }


void xdddefault_grab_config_directives(char *input_buffer){
	char *temp_buffer;

	/* downtime file definition */
	if((strstr(input_buffer,"downtime_file")==input_buffer) || (strstr(input_buffer,"xdddefault_downtime_file")==input_buffer)){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xdddefault_downtime_file,temp_buffer,sizeof(xdddefault_downtime_file)-1);
		xdddefault_downtime_file[sizeof(xdddefault_downtime_file)-1]='\x0';
	        }


	/* temp file definition */
	if((strstr(input_buffer,"temp_file")==input_buffer) || (strstr(input_buffer,"xdddefault_temp_file")==input_buffer)){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xdddefault_temp_file,temp_buffer,sizeof(xdddefault_temp_file)-1);
		xdddefault_temp_file[sizeof(xdddefault_temp_file)-1]='\x0';
	        }

	return;	
        }



#ifdef NSCORE


/******************************************************************/
/*********** DOWNTIME INITIALIZATION/CLEANUP FUNCTIONS ************/
/******************************************************************/


/* initialize downtime data */
int xdddefault_initialize_downtime_data(char *main_config_file){

	/* grab configuration information */
	if(xdddefault_grab_config_info(main_config_file)==ERROR)
		return ERROR;

	/* create downtime file if necessary */
	xdddefault_create_downtime_file();

	/* clean up the old downtime data */
	xdddefault_validate_downtime_data();

	/* read downtime data into memory */
	xdddefault_read_downtime_data(main_config_file);

	return OK;
        }



/* creates an empty downtime data file if one doesn't already exist */
int xdddefault_create_downtime_file(void){
	FILE *fp;
	struct stat statbuf;

	/* bail out if file already exists */
	if(!stat(xdddefault_downtime_file,&statbuf))
		return OK;

	/* create an empty file */
	fp=fopen(xdddefault_downtime_file,"w");
	if(fp==NULL)
		return ERROR;

	fclose(fp);

	return OK;
        }



/* removes invalid and old downtime entries from the downtime file */
int xdddefault_validate_downtime_data(void){
	char input_buffer[MAX_INPUT_BUFFER];
	char output_buffer[MAX_INPUT_BUFFER];
	char *entry_time;
	char *temp_buffer;
	FILE *fpin;
	FILE *fpout;
	int save=TRUE;
	host *temp_host=NULL;
	service *temp_service=NULL;
	int downtime_type=HOST_DOWNTIME;
	time_t start_time;
	time_t end_time;
	char temp_file[MAX_INPUT_BUFFER];
	int tempfd;


	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xdddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((tempfd=mkstemp(temp_file))==-1)
		return ERROR;
	fpout=fdopen(tempfd,"w");
	if(fpout==NULL){
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* open current downtime file for reading */
	fpin=fopen(xdddefault_downtime_file,"r");
	if(fpin==NULL){
		fclose(fpout);
		unlink(temp_file);
		return ERROR;
	        }

	/* reset the current downtime counter */
	current_downtime_id=1;

	/* process each line in the old downtime file */
	while(fgets(input_buffer,(int)(sizeof(input_buffer)-1),fpin)){

		if(feof(fpin))
			break;

		save=TRUE;

		/* this is a host entry */
		if(strstr(input_buffer,"] HOST_DOWNTIME;"))
			downtime_type=HOST_DOWNTIME;

		/* this is a service entry */
		else if(strstr(input_buffer,"] SERVICE_DOWNTIME;"))
			downtime_type=SERVICE_DOWNTIME;
		
		/* else this is not a downtime entry, so skip it */
		else
			continue;

		/* get the downtime entry time */
		temp_buffer=my_strtok(input_buffer,"[");
		entry_time=my_strtok(NULL,"]");

		/* skip the downtime type identifier */
		temp_buffer=my_strtok(NULL,";");

		/* skip the downtime ID number */
		temp_buffer=my_strtok(NULL,";");

		/* see if the host still exists */
		temp_buffer=my_strtok(NULL,";");
		temp_host=find_host((temp_buffer==NULL)?"":temp_buffer);
		if(temp_host==NULL)
			save=FALSE;

		/* if this is a service entry, make sure the service still exists */
		if(downtime_type==SERVICE_DOWNTIME){
			temp_buffer=my_strtok(NULL,";");
			if(temp_host!=NULL)
				temp_service=find_service(temp_host->name,(temp_buffer==NULL)?"":temp_buffer);
			else
				temp_service=NULL;
			if(temp_service==NULL)
				save=FALSE;

		        }

		/* get the start time */
		temp_buffer=my_strtok(NULL,";");
		if(temp_buffer==NULL)
			continue;
		start_time=(time_t)strtoul(temp_buffer,NULL,10);

		/* get the end time */
		temp_buffer=my_strtok(NULL,";");
		if(temp_buffer==NULL)
			continue;
		end_time=(time_t)strtoul(temp_buffer,NULL,10);

		/* see if this downtime has expired */
		if(end_time<=time(NULL))
			save=FALSE;

		/* get the rest of the downtime data */
		temp_buffer=my_strtok(NULL,"\n");

		/* write the old entry to the new file, re-numbering them as we go */
		if(save==TRUE){

			snprintf(output_buffer,sizeof(output_buffer)-1,"[%s] %s_DOWNTIME;%d;%s;",entry_time,(downtime_type==HOST_DOWNTIME)?"HOST":"SERVICE",current_downtime_id,temp_host->name);
			output_buffer[sizeof(output_buffer)-1]='\x0';
			fputs(output_buffer,fpout);

			if(downtime_type==SERVICE_DOWNTIME){
				snprintf(output_buffer,sizeof(output_buffer)-1,"%s;",temp_service->description);
				output_buffer[sizeof(output_buffer)-1]='\x0';
				fputs(output_buffer,fpout);
			        }

			snprintf(output_buffer,sizeof(output_buffer)-1,"%lu;%lu;%s\n",(unsigned long)start_time,(unsigned long)end_time,temp_buffer);
			output_buffer[sizeof(output_buffer)-1]='\x0';
			fputs(output_buffer,fpout);

			/* increment the current downtime counter */
			current_downtime_id++;
		        }
	        }

	/* reset file permissions */
	fchmod(tempfd,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* close files */
	fclose(fpout);
	fclose(fpin);

	/* replace old downtime file */
	if(my_rename(temp_file,xdddefault_downtime_file))
		return ERROR;

	return OK;
        }



/* removes invalid and old downtime entries from the downtime file */
int xdddefault_cleanup_downtime_data(char *main_config_file){

	/* we don't need to do any cleanup... */
	return OK;
        }



/******************************************************************/
/************************ SAVE FUNCTIONS **************************/
/******************************************************************/

/* saves a scheduled host downtime entry */
int xdddefault_save_host_downtime(char *host_name, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long duration, int *downtime_id){
	char temp_buffer[MAX_INPUT_BUFFER];
	FILE *fp;

	/* increment the current downtime id */
	current_downtime_id++;

	/* create the downtime string */
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu] HOST_DOWNTIME;%d;%s;%lu;%lu;%d;%lu;%s;%s\n",(unsigned long)entry_time,current_downtime_id,host_name,(unsigned long)start_time,(unsigned long)end_time,fixed,duration,author,comment);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	
	/* open the downtime file */
	fp=fopen(xdddefault_downtime_file,"a");
	if(fp==NULL)
		return ERROR;
	fputs(temp_buffer,fp);
	fclose(fp);

	/* return the id for the downtime we just added */
	if(downtime_id!=NULL)
		*downtime_id=current_downtime_id;

	return OK;
        }



/* saves a scheduled service downtime entry */
int xdddefault_save_service_downtime(char *host_name, char *service_description, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long duration, int *downtime_id){
	char temp_buffer[MAX_INPUT_BUFFER];
	FILE *fp;

	/* increment the current downtime id */
	current_downtime_id++;

	/* create the downtime string */
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu] SERVICE_DOWNTIME;%d;%s;%s;%lu;%lu;%d;%lu;%s;%s\n",(unsigned long)entry_time,current_downtime_id,host_name,service_description,(unsigned long)start_time,(unsigned long)end_time,fixed,duration,author,comment);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	
	/* open the downtime file */
	fp=fopen(xdddefault_downtime_file,"a");
	if(fp==NULL)
		return ERROR;
	fputs(temp_buffer,fp);
	fclose(fp);

	/* return the id for the downtime we just added */
	if(downtime_id!=NULL)
		*downtime_id=current_downtime_id;


	return OK;
        }


/******************************************************************/
/********************** DELETION FUNCTIONS ************************/
/******************************************************************/

/* deletes a scheduled host downtime entry */
int xdddefault_delete_host_downtime(int downtime_id){
	int result;

	result=xdddefault_delete_downtime(HOST_DOWNTIME,downtime_id);

	return result;
        }


/* deletes a scheduled service downtime entry */
int xdddefault_delete_service_downtime(int downtime_id){
	int result;

	result=xdddefault_delete_downtime(SERVICE_DOWNTIME,downtime_id);

	return result;
        }


/* deletes a scheduled host or service downtime entry */
int xdddefault_delete_downtime(int type, int downtime_id){
	FILE *fpin;
	FILE *fpout;
	char match[MAX_INPUT_BUFFER];
	char buffer[MAX_INPUT_BUFFER];
	char temp_file[MAX_INPUT_BUFFER];
	int tempfd;

	/* create the matching string we're looking for in the log file */
	snprintf(match,sizeof(match),"] %s_DOWNTIME;%d;",(type==HOST_DOWNTIME)?"HOST":"SERVICE",downtime_id);
	match[sizeof(match)-1]='\x0';

	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xdddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((tempfd=mkstemp(temp_file))==-1)
		return ERROR;
	fpout=fdopen(tempfd,"w");
	if(fpout==NULL){
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* open the current downtime file for reading */
	fpin=fopen(xdddefault_downtime_file,"r");
	if(fpin==NULL){
		fclose(fpout);
		unlink(temp_file);
		return ERROR;
	        }

	/* write every line in the downtime file, except the matching comment */
	while(fgets(buffer,(int)(sizeof(buffer)-1),fpin)){

		if(feof(fpin))
			break;

		if(!strstr(buffer,match))
			fputs(buffer,fpout);
	        }

	/* reset file permissions */
	fchmod(tempfd,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* close files */
	fclose(fpout);
	fclose(fpin);

	/* replace old downtime file */
	if(my_rename(temp_file,xdddefault_downtime_file))
		return ERROR;
 
	return OK;
        }


#endif




/******************************************************************/
/****************** DOWNTIME INPUT FUNCTIONS **********************/
/******************************************************************/


/* read the downtime file */
int xdddefault_read_downtime_data(char *main_config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	char *temp_buffer;
	FILE *fp;
	int downtime_id;
	int downtime_type;
	time_t entry_time;
	time_t start_time;
	time_t end_time;
	int fixed;
	unsigned long duration;
	char *host_name="";
	char *service_description="";
	char *comment="";
	char *author="";


#ifdef NSCGI
	/* grab configuration information */
	if(xdddefault_grab_config_info(main_config_file)==ERROR)
		return ERROR;
#endif

	/* open the downtime file */
	fp=fopen(xdddefault_downtime_file,"r");
	if(fp==NULL)
		return ERROR;

	while(fgets(input_buffer,(int)(sizeof(input_buffer)-1),fp)){

	        if(feof(fp))
		        break;

		if(input_buffer==NULL)
		        continue;

		/* host comment or service downtime? */
		if(strstr(input_buffer,"] HOST_DOWNTIME;"))
			downtime_type=HOST_DOWNTIME;
		else if(strstr(input_buffer,"] SERVICE_DOWNTIME;"))
			downtime_type=SERVICE_DOWNTIME;
		else
			continue;

		/* get the entry time */
		temp_buffer=my_strtok(input_buffer,"[");
		temp_buffer=my_strtok(NULL,"]");
		if(temp_buffer==NULL)
			continue;
		entry_time=(time_t)strtoul(temp_buffer,NULL,10);

		/* get the downtime id */
		temp_buffer=my_strtok(NULL,";");
		temp_buffer=my_strtok(NULL,";");
		if(temp_buffer==NULL)
			continue;
		downtime_id=atoi(temp_buffer);

		/* get the host name */
		host_name=my_strtok(NULL,";");
		if(host_name==NULL)
			continue;

		/* get the service name */
		if(downtime_type==SERVICE_DOWNTIME){
			service_description=my_strtok(NULL,";");
			if(service_description==NULL)
				continue;
		        }
		
		/* get the start time */
		temp_buffer=my_strtok(NULL,";");
		if(temp_buffer==NULL)
			continue;
		start_time=(time_t)strtoul(temp_buffer,NULL,10);

		/* get the end time */
		temp_buffer=my_strtok(NULL,";");
		if(temp_buffer==NULL)
			continue;
		end_time=(time_t)strtoul(temp_buffer,NULL,10);

		/* get the fixed flag */
		temp_buffer=my_strtok(NULL,";");
		if(temp_buffer==NULL)
			continue;
		fixed=atoi(temp_buffer);

		/* get the duration */
		temp_buffer=my_strtok(NULL,";");
		if(temp_buffer==NULL)
			continue;
		duration=strtoul(temp_buffer,NULL,10);

		/* get the author */
		author=my_strtok(NULL,";");
		if(author==NULL)
			continue;

		/* get the comment */
		comment=my_strtok(NULL,"\n");
		if(comment==NULL)
			continue;


		/* add the downtime */
		if(downtime_type==HOST_DOWNTIME)
			add_host_downtime(host_name,entry_time,author,comment,start_time,end_time,fixed,duration,downtime_id);
		else
			add_service_downtime(host_name,service_description,entry_time,author,comment,start_time,end_time,fixed,duration,downtime_id);

#ifdef NSCORE
		/* must register the downtime with Nagios so it can schedule it, add comments, etc. */
		register_downtime(downtime_type,downtime_id);
#endif
	        }

	fclose(fp);

	return OK;
        }

