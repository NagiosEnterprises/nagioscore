/*****************************************************************************
 *
 * XCDDEFAULT.C - Default external comment data routines for Nagios
 *
 * Copyright (c) 2000-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   07-03-2002
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
#include "../common/comments.h"

#ifdef NSCORE
#include "../common/objects.h"
#include "../base/nagios.h"
#endif

#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/
#include "xcddefault.h"

char xcddefault_comment_file[MAX_FILENAME_LENGTH]="";
char xcddefault_temp_file[MAX_FILENAME_LENGTH]="";

#ifdef NSCORE
int current_comment_id=0;
#endif



/******************************************************************/
/***************** COMMON CONFIG INITIALIZATION  ******************/
/******************************************************************/

/* grab configuration information from appropriate config file(s) */
int xcddefault_grab_config_info(char *config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	FILE *fp;
#ifdef NSCGI
	FILE *fp2;
	char *temp_buffer;
#endif

	/*** CORE PASSES IN MAIN CONFIG FILE, CGIS PASS IN CGI CONFIG FILE! ***/

	/* initialize the location of the comment and temp */
	strncpy(xcddefault_comment_file,DEFAULT_COMMENT_FILE,sizeof(xcddefault_comment_file)-1);
	strncpy(xcddefault_temp_file,DEFAULT_TEMP_FILE,sizeof(xcddefault_temp_file)-1);
	xcddefault_comment_file[sizeof(xcddefault_comment_file)-1]='\x0';
	xcddefault_temp_file[sizeof(xcddefault_temp_file)-1]='\x0';

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

				xcddefault_grab_config_directives(input_buffer);
			        }

			fclose(fp2);
		        }
#endif

#ifdef NSCORE
		/* core read variables directly from the main config file */
		xcddefault_grab_config_directives(input_buffer);
#endif
	        }

	fclose(fp);

	/* we didn't find the comment file */
	if(!strcmp(xcddefault_comment_file,""))
		return ERROR;

	/* we didn't find the temp file */
	if(!strcmp(xcddefault_temp_file,""))
		return ERROR;

	return OK;
        }


void xcddefault_grab_config_directives(char *input_buffer){
	char *temp_buffer;

	/* comment file definition */
	if((strstr(input_buffer,"comment_file")==input_buffer) || (strstr(input_buffer,"xcddefault_comment_file")==input_buffer)){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xcddefault_comment_file,temp_buffer,sizeof(xcddefault_comment_file)-1);
		xcddefault_comment_file[sizeof(xcddefault_comment_file)-1]='\x0';
	        }


	/* status log definition */
	if((strstr(input_buffer,"temp_file")==input_buffer) || (strstr(input_buffer,"xcddefault_temp_file")==input_buffer)){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xcddefault_temp_file,temp_buffer,sizeof(xcddefault_temp_file)-1);
		xcddefault_temp_file[sizeof(xcddefault_temp_file)-1]='\x0';
	        }

	return;	
        }



#ifdef NSCORE


/******************************************************************/
/************ COMMENT INITIALIZATION/CLEANUP FUNCTIONS ************/
/******************************************************************/


/* initialize comment data */
int xcddefault_initialize_comment_data(char *main_config_file){

	/* grab configuration information */
	if(xcddefault_grab_config_info(main_config_file)==ERROR)
		return ERROR;

	/* create comment file if necessary */
	xcddefault_create_comment_file();

	/* clean up the old comment data */
	xcddefault_validate_comment_data();

	return OK;
        }



/* creates an empty comment data file if one doesn't already exist */
int xcddefault_create_comment_file(void){
	FILE *fp;
	struct stat statbuf;

	/* bail out if file already exists */
	if(!stat(xcddefault_comment_file,&statbuf))
		return OK;

	/* create an empty file */
	fp=fopen(xcddefault_comment_file,"w");
	if(fp==NULL)
		return ERROR;

	fclose(fp);

	return OK;
        }



/* removes invalid and old comments from the comment file */
int xcddefault_validate_comment_data(void){
	char input_buffer[MAX_INPUT_BUFFER];
	char output_buffer[MAX_INPUT_BUFFER];
	char *entry_time;
	char *comment_author;
	char *comment_data;
	char *temp_buffer;
	FILE *fpin;
	FILE *fpout;
	int save=TRUE;
	host *temp_host=NULL;
	service *temp_service=NULL;
	int comment_type=HOST_COMMENT;
	int comment_id;
	char temp_file[MAX_INPUT_BUFFER];
	int tempfd;


	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xcddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((tempfd=mkstemp(temp_file))==-1)
		return ERROR;
	fpout=fdopen(tempfd,"w");
	if(fpout==NULL){
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* open current comment file for reading */
	fpin=fopen(xcddefault_comment_file,"r");
	if(fpin==NULL){
		fclose(fpout);
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* reset the current comment counter */
	current_comment_id=0;

	/* process each line in the old comment file */
	while(fgets(input_buffer,(int)(sizeof(input_buffer)-1),fpin)){

		if(feof(fpin))
			break;

		save=TRUE;

		/* this is a host comment */
		if(strstr(input_buffer,"] HOST_COMMENT;"))
			comment_type=HOST_COMMENT;

		/* this is a service comment */
		else if(strstr(input_buffer,"] SERVICE_COMMENT;"))
			comment_type=SERVICE_COMMENT;
		
		/* else this is not a comment, so skip it */
		else
			continue;

		/* get the comment entry time */
		temp_buffer=my_strtok(input_buffer,"[");
		entry_time=my_strtok(NULL,"]");

		/* skip the comment type identifier */
		temp_buffer=my_strtok(NULL,";");

		/* grab the comment ID number - make sure global comment counter is as big as the max value found */
		temp_buffer=my_strtok(NULL,";");
		comment_id=atoi(temp_buffer);
		if(comment_id>=current_comment_id)
			current_comment_id=comment_id;

		/* see if the host still exists */
		temp_buffer=my_strtok(NULL,";");
		temp_host=find_host((temp_buffer==NULL)?"":temp_buffer,NULL);
		if(temp_host==NULL)
			save=FALSE;

		/* if this is a service comment, make sure the service still exists */
		if(comment_type==SERVICE_COMMENT){
			temp_buffer=my_strtok(NULL,";");
			if(temp_host!=NULL)
				temp_service=find_service(temp_host->name,(temp_buffer==NULL)?"":temp_buffer,NULL);
			else
				temp_service=NULL;
			if(temp_service==NULL)
				save=FALSE;

		        }

		/* see if the comment is persistent */
		temp_buffer=my_strtok(NULL,";");
		if(temp_buffer==NULL)
			save=FALSE;
		else if(atoi(temp_buffer)==0)
			save=FALSE;

		/* get the comment author */
		comment_author=my_strtok(NULL,";");

		/* get the comment data */
		comment_data=my_strtok(NULL,"\n");

		/* write the old comment to the new file - COMMENTS ARE NO LONGER RE-NUMBERED */
		if(save==TRUE){

			snprintf(output_buffer,sizeof(output_buffer)-1,"[%s] %s_COMMENT;%d;%s;",entry_time,(comment_type==HOST_COMMENT)?"HOST":"SERVICE",comment_id,temp_host->name);
			output_buffer[sizeof(output_buffer)-1]='\x0';
			fputs(output_buffer,fpout);

			if(comment_type==SERVICE_COMMENT){
				snprintf(output_buffer,sizeof(output_buffer)-1,"%s;",temp_service->description);
				output_buffer[sizeof(output_buffer)-1]='\x0';
				fputs(output_buffer,fpout);
			        }

			snprintf(output_buffer,sizeof(output_buffer)-1,"1;%s;%s\n",comment_author,comment_data);
			output_buffer[sizeof(output_buffer)-1]='\x0';
			fputs(output_buffer,fpout);
		        }
	        }

	/* reset file permissions */
	fchmod(tempfd,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* close files */
	fclose(fpout);
	close(tempfd);
	fclose(fpin);

	/* replace old comment file */
	if(my_rename(temp_file,xcddefault_comment_file))
		return ERROR;

	return OK;
        }


/* removes invalid and old comments from the comment file */
int xcddefault_cleanup_comment_data(char *main_config_file){

	/* we don't need to do any cleanup... */
	return OK;
        }





/******************************************************************/
/***************** DEFAULT DATA OUTPUT FUNCTIONS ******************/
/******************************************************************/


/* saves a new host comment */
int xcddefault_save_host_comment(char *host_name, time_t entry_time, char *author_name, char *comment_data, int persistent, int *comment_id){
	char temp_buffer[MAX_INPUT_BUFFER];
	FILE *fp;

	/* increment the current comment id */
	current_comment_id++;

	/* create the comment string */
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu] HOST_COMMENT;%d;%s;%d;%s;%s\n",(unsigned long)entry_time,current_comment_id,host_name,persistent,author_name,comment_data);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	
	/* open the comment file */
	fp=fopen(xcddefault_comment_file,"a");
	if(fp==NULL)
		return ERROR;
	fputs(temp_buffer,fp);
	fclose(fp);

	/* return the id for the comment we just added */
	if(comment_id!=NULL)
		*comment_id=current_comment_id;

	return OK;
        }


/* saves a new service comment */
int xcddefault_save_service_comment(char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int *comment_id){
	char temp_buffer[MAX_INPUT_BUFFER];
	FILE *fp;

	/* increment the current comment id */
	current_comment_id++;

	/* create the comment string */
	snprintf(temp_buffer,sizeof(temp_buffer)-1,"[%lu] SERVICE_COMMENT;%d;%s;%s;%d;%s;%s\n",(unsigned long)entry_time,current_comment_id,host_name,svc_description,persistent,author_name,comment_data);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	
	/* open the comment file */
	fp=fopen(xcddefault_comment_file,"a");
	if(fp==NULL)
		return ERROR;
	fputs(temp_buffer,fp);
	fclose(fp);

	/* return the id for the comment we just added */
	if(comment_id!=NULL)
		*comment_id=current_comment_id;

	return OK;
        }



/******************************************************************/
/**************** COMMENT DELETION FUNCTIONS **********************/
/******************************************************************/


/* deletes a host comment */
int xcddefault_delete_host_comment(int comment_id){
	FILE *fpin;
	FILE *fpout;
	char match[MAX_INPUT_BUFFER];
	char buffer[MAX_INPUT_BUFFER];
	char temp_file[MAX_INPUT_BUFFER];
	int tempfd;

	/* create the matching string we're looking for in the log file */
	snprintf(match,sizeof(match),"] HOST_COMMENT;%d;",comment_id);
	match[sizeof(match)-1]='\x0';

	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xcddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((tempfd=mkstemp(temp_file))==-1)
		return ERROR;
	fpout=fdopen(tempfd,"w");
	if(fpout==NULL){
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* open current comment file for reading */
	fpin=fopen(xcddefault_comment_file,"r");
	if(fpin==NULL){
		fclose(fpout);
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* write every line in the comment file, except the matching comment */
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
	close(tempfd);
	fclose(fpin);

	/* replace old comment file */
	if(my_rename(temp_file,xcddefault_comment_file))
		return ERROR;

	return OK;
        }


/* deletes a service comment */
int xcddefault_delete_service_comment(int comment_id){
	FILE *fpin;
	FILE *fpout;
	char match[MAX_INPUT_BUFFER];
	char buffer[MAX_INPUT_BUFFER];
	char temp_file[MAX_INPUT_BUFFER];
	int tempfd;

	/* create the matching string we're looking for in the log file */
	snprintf(match,sizeof(match),"] SERVICE_COMMENT;%d;",comment_id);
	match[sizeof(match)-1]='\x0';

	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xcddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((tempfd=mkstemp(temp_file))==-1)
		return ERROR;
	fpout=fdopen(tempfd,"w");
	if(fpout==NULL){
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* open current comment file for reading */
	fpin=fopen(xcddefault_comment_file,"r");
	if(fpin==NULL){
		fclose(fpout);
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* write every line in the comment file, except the matching comment */
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
	close(tempfd);
	fclose(fpin);

	/* replace old comment file */
	if(my_rename(temp_file,xcddefault_comment_file))
		return ERROR;

	return OK;
        }


/* deletes all comments for a particular host */
int xcddefault_delete_all_host_comments(char *host_name){
	FILE *fpin;
	FILE *fpout;
	char buffer[MAX_INPUT_BUFFER];
	char match1[MAX_INPUT_BUFFER];
	char match2[MAX_INPUT_BUFFER];
	char temp_file[MAX_INPUT_BUFFER];
	int tempfd;

	/* create the matching string we're looking for in the log file */
	snprintf(match1,sizeof(match1),"] HOST_COMMENT;");
	snprintf(match2,sizeof(match2),";%s;",host_name);
	match1[sizeof(match1)-1]='\x0';
	match2[sizeof(match2)-1]='\x0';

	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xcddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((tempfd=mkstemp(temp_file))==-1)
		return ERROR;
	fpout=fdopen(tempfd,"w");
	if(fpout==NULL){
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* open current comment file for reading */
	fpin=fopen(xcddefault_comment_file,"r");
	if(fpin==NULL){
		fclose(fpout);
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* write every line in the comment file, except the matching comment(s) */
	while(fgets(buffer,(int)(sizeof(buffer)-1),fpin)){

		if(feof(fpin))
			break;

		if(!(strstr(buffer,match1) && strstr(buffer,match2)))
			fputs(buffer,fpout);
	        }

	/* reset file permissions */
	fchmod(tempfd,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* close files */
	fclose(fpout);
	close(tempfd);
	fclose(fpin);

	/* replace old comment file */
	if(my_rename(temp_file,xcddefault_comment_file))
		return ERROR;

	return OK;
        }


/* deletes all comments for a particular service */
int xcddefault_delete_all_service_comments(char *host_name, char *svc_description){
	FILE *fpin;
	FILE *fpout;
	char buffer[MAX_INPUT_BUFFER];
	char match1[MAX_INPUT_BUFFER];
	char match2[MAX_INPUT_BUFFER];
	char temp_file[MAX_INPUT_BUFFER];
	int tempfd;

	/* create the matching string we're looking for in the log file */
	snprintf(match1,sizeof(match1),"] SERVICE_COMMENT;");
	snprintf(match2,sizeof(match2),";%s;%s;",host_name,svc_description);
	match1[sizeof(match1)-1]='\x0';
	match2[sizeof(match2)-1]='\x0';

	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xcddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((tempfd=mkstemp(temp_file))==-1)
		return ERROR;
	fpout=fdopen(tempfd,"w");
	if(fpout==NULL){
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* open current comment file for reading */
	fpin=fopen(xcddefault_comment_file,"r");
	if(fpin==NULL){
		fclose(fpout);
		close(tempfd);
		unlink(temp_file);
		return ERROR;
	        }

	/* write every line in the comment file, except the matching comment(s) */
	while(fgets(buffer,(int)(sizeof(buffer)-1),fpin)){

		if(feof(fpin))
			break;

		if(!(strstr(buffer,match1) && strstr(buffer,match2)))
			fputs(buffer,fpout);
	        }

	/* reset file permissions */
	fchmod(tempfd,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* close files */
	fclose(fpout);
	close(tempfd);
	fclose(fpin);

	/* replace old comment file */
	if(my_rename(temp_file,xcddefault_comment_file))
		return ERROR;

	return OK;
        }

#endif



#ifdef NSCGI

/******************************************************************/
/****************** COMMENT INPUT FUNCTIONS ***********************/
/******************************************************************/


/* read the comment file */
int xcddefault_read_comment_data(char *main_config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	char *temp_buffer;
	FILE *fp;
	int comment_id;
	int comment_type;
	int persistent=0;
	time_t entry_time;
	char *host_name="";
	char *service_description="";
	char *author="";
	char *comment_data="";

	/* grab configuration information */
	if(xcddefault_grab_config_info(main_config_file)==ERROR)
		return ERROR;

	/* open the comment */
	fp=fopen(xcddefault_comment_file,"r");
	if(fp==NULL)
		return ERROR;

	while(read_line(input_buffer,MAX_INPUT_BUFFER-1,fp)){

	        if(feof(fp))
		        break;

		if(input_buffer==NULL)
		        continue;

		if(!strstr(input_buffer,"] HOST_COMMENT;") && !strstr(input_buffer,"] SERVICE_COMMENT;"))
			continue;

		/* host comment or service comment? */
		if(strstr(input_buffer,"] HOST_COMMENT;"))
			comment_type=HOST_COMMENT;
		else
			comment_type=SERVICE_COMMENT;


		/* get the entry time */
		temp_buffer=my_strtok(input_buffer,"[");
		temp_buffer=my_strtok(NULL,"]");
		if(temp_buffer==NULL)
			continue;
		entry_time=(time_t)strtoul(temp_buffer,NULL,10);

		/* get the comment id */
		temp_buffer=my_strtok(NULL,";");
		temp_buffer=my_strtok(NULL,";");
		if(temp_buffer==NULL)
			continue;
		comment_id=atoi(temp_buffer);

		/* get the host name */
		host_name=my_strtok(NULL,";");
		if(host_name==NULL)
			continue;

		/* get the service name */
		if(comment_type==SERVICE_COMMENT){
			service_description=my_strtok(NULL,";");
			if(service_description==NULL)
				continue;
		        }
		
		/* get the perisitent flag */
		temp_buffer=my_strtok(NULL,";");
		if(temp_buffer==NULL)
			continue;
		persistent=atoi(temp_buffer);

		/* get the author */
		author=my_strtok(NULL,";");
		if(author==NULL)
			continue;
		
		/* get the comment data */
		comment_data=my_strtok(NULL,"\n");
		if(comment_data==NULL)
			continue;

		/* add the comment */
		if(comment_type==HOST_COMMENT)
			add_host_comment(host_name,entry_time,author,comment_data,comment_id,persistent);
		else
			add_service_comment(host_name,service_description,entry_time,author,comment_data,comment_id,persistent);
	        }

	fclose(fp);

	return OK;
        }

#endif

