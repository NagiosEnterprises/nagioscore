/*****************************************************************************
 *
 * XCDDEFAULT.C - Default external comment data routines for Nagios
 *
 * Copyright (c) 2000-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   08-14-2003
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

#include "../include/config.h"
#include "../include/common.h"
#include "../include/locations.h"
#include "../include/comments.h"

#ifdef NSCORE
#include "../include/objects.h"
#include "../include/nagios.h"
#endif

#ifdef NSCGI
#include "../include/cgiutils.h"
#endif


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/
#include "xcddefault.h"

char xcddefault_comment_file[MAX_FILENAME_LENGTH]="";
char xcddefault_temp_file[MAX_FILENAME_LENGTH]="";

#ifdef NSCORE
unsigned long current_comment_id=0;
extern comment *comment_list;
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

	/* read comment data */
	xcddefault_read_comment_data(main_config_file);

	/* clean up the old comment data */
	xcddefault_validate_comment_data();

	return OK;
        }



/* creates an empty comment data file if one doesn't already exist */
int xcddefault_create_comment_file(void){
	struct stat statbuf;

	/* bail out if file already exists */
	if(!stat(xcddefault_comment_file,&statbuf))
		return OK;

	/* create an empty file */
	xcddefault_save_comment_data();

	return OK;
        }



/* removes invalid and old comments from the comment file */
int xcddefault_validate_comment_data(void){
	comment *temp_comment;
	comment *next_comment;
	int update_file=FALSE;
	int save=TRUE;

	/* remove stale comments */
	for(temp_comment=comment_list;temp_comment!=NULL;temp_comment=next_comment){

		next_comment=temp_comment->next;
		save=TRUE;

		/* delete comments with invalid host names */
		if(find_host(temp_comment->host_name)==NULL)
			save=FALSE;

		/* delete comments with invalid service descriptions */
		if(temp_comment->comment_type==SERVICE_COMMENT && find_service(temp_comment->host_name,temp_comment->service_description)==NULL)
			save=FALSE;

		/* delete non-persistent comments */
		if(temp_comment->persistent==FALSE)
			save=FALSE;

		/* delete the comment */
		if(save==FALSE){
			update_file=TRUE;
			delete_comment(temp_comment->comment_type,temp_comment->comment_id);
		        }
	        }	

	/* update comment file */
	if(update_file==TRUE)
		xcddefault_save_comment_data();

	/* reset the current comment counter */
	current_comment_id=0;

	/* find the new starting index for comment id */
	for(temp_comment=comment_list;temp_comment!=NULL;temp_comment=temp_comment->next){
		if(temp_comment->comment_id>current_comment_id)
			current_comment_id=temp_comment->comment_id;
	        }

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


/* adds a new host comment */
int xcddefault_add_new_host_comment(char *host_name, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, unsigned long *comment_id){

	/* find the next valid comment id */
	do{
		current_comment_id++;
		if(current_comment_id==0)
			current_comment_id++;
  	        }while(find_host_comment(current_comment_id)!=NULL);

	/* add comment to list in memory */
	add_host_comment(host_name,entry_time,author_name,comment_data,current_comment_id,persistent,source);

	/* update comment file */
	xcddefault_save_comment_data();

	/* return the id for the comment we are about to add (this happens in the main code) */
	if(comment_id!=NULL)
		*comment_id=current_comment_id;

	return OK;
        }


/* adds a new service comment */
int xcddefault_add_new_service_comment(char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, unsigned long *comment_id){

	/* find the next valid comment id */
	do{
		current_comment_id++;
		if(current_comment_id==0)
			current_comment_id++;
  	        }while(find_service_comment(current_comment_id)!=NULL);

	/* add comment to list in memory */
	add_service_comment(host_name,svc_description,entry_time,author_name,comment_data,current_comment_id,persistent,source);

	/* update comment file */
	xcddefault_save_comment_data();

	/* return the id for the comment we are about to add (this happens in the main code) */
	if(comment_id!=NULL)
		*comment_id=current_comment_id;

	return OK;
        }



/******************************************************************/
/**************** COMMENT DELETION FUNCTIONS **********************/
/******************************************************************/


/* deletes a host comment */
int xcddefault_delete_host_comment(unsigned long comment_id){

	/* update comment file */
	xcddefault_save_comment_data();

	return OK;
        }


/* deletes a service comment */
int xcddefault_delete_service_comment(unsigned long comment_id){

	/* update comment file */
	xcddefault_save_comment_data();

	return OK;
        }


/* deletes all comments for a particular host */
int xcddefault_delete_all_host_comments(char *host_name){

	/* update comment file */
	xcddefault_save_comment_data();

	return OK;
        }


/* deletes all comments for a particular service */
int xcddefault_delete_all_service_comments(char *host_name, char *svc_description){

	/* update comment file */
	xcddefault_save_comment_data();

	return OK;
        }


/******************************************************************/
/****************** COMMENT OUTPUT FUNCTIONS **********************/
/******************************************************************/

/* writes comment data to file */
int xcddefault_save_comment_data(void){
	char temp_file[MAX_FILENAME_LENGTH];
	time_t current_time;
	comment *temp_comment;
	int fd=0;
	FILE *fp=NULL;

	/* open a safe temp file for output */
	snprintf(temp_file,sizeof(temp_file)-1,"%sXXXXXX",xcddefault_temp_file);
	temp_file[sizeof(temp_file)-1]='\x0';
	if((fd=mkstemp(temp_file))==-1)
		return ERROR;
	fp=fdopen(fd,"w");
	if(fp==NULL){
		close(fd);
		unlink(temp_file);
		return ERROR;
	        }

	/* write header */
	fprintf(fp,"########################################\n");
	fprintf(fp,"#          NAGIOS COMMENT FILE\n");
	fprintf(fp,"#\n");
	fprintf(fp,"# THIS FILE IS AUTOMATICALLY GENERATED\n");
	fprintf(fp,"# BY NAGIOS.  DO NOT MODIFY THIS FILE!\n");
	fprintf(fp,"########################################\n\n");

	time(&current_time);

	/* write file info */
	fprintf(fp,"info {\n");
	fprintf(fp,"\tcreated=%lu\n",current_time);
	fprintf(fp,"\tversion=%s\n",PROGRAM_VERSION);
	fprintf(fp,"\t}\n\n");

	/* save all comments */
	for(temp_comment=comment_list;temp_comment!=NULL;temp_comment=temp_comment->next){

		if(temp_comment->comment_type==HOST_COMMENT)
			fprintf(fp,"hostcomment {\n");
		else
			fprintf(fp,"servicecomment {\n");
		fprintf(fp,"\thost_name=%s\n",temp_comment->host_name);
		if(temp_comment->comment_type==SERVICE_COMMENT)
			fprintf(fp,"\tservice_description=%s\n",temp_comment->service_description);
		fprintf(fp,"\tcomment_id=%lu\n",temp_comment->comment_id);
		fprintf(fp,"\tsource=%d\n",temp_comment->source);
		fprintf(fp,"\tpersistent=%d\n",temp_comment->persistent);
		fprintf(fp,"\tentry_time=%lu\n",temp_comment->entry_time);
		fprintf(fp,"\tauthor=%s\n",temp_comment->author);
		fprintf(fp,"\tcomment_data=%s\n",temp_comment->comment_data);
		fprintf(fp,"\t}\n\n");
	        }

	/* reset file permissions */
	fchmod(fd,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* close the temp file */
	fclose(fp);

	/* move the temp file to the comment file (overwrite the old comment file) */
	if(my_rename(temp_file,xcddefault_comment_file))
		return ERROR;

	return OK;
        }

#endif




/******************************************************************/
/****************** COMMENT INPUT FUNCTIONS ***********************/
/******************************************************************/


/* read the comment file */
int xcddefault_read_comment_data(char *main_config_file){
	char temp_buffer[MAX_INPUT_BUFFER];
	FILE *fp;
	int data_type=XCDDEFAULT_NO_DATA;
	char *var;
	char *val;
	int result;
	unsigned long comment_id=0;
	int persistent=FALSE;
	int source=COMMENTSOURCE_INTERNAL;
	time_t entry_time=0L;
	char *host_name=NULL;
	char *service_description=NULL;
	char *author=NULL;
	char *comment_data=NULL;

	/* grab configuration data */
	result=xcddefault_grab_config_info(main_config_file);
	if(result==ERROR)
		return ERROR;

	/* open the comment file for reading */
	fp=fopen(xcddefault_comment_file,"r");
	if(fp==NULL)
		return ERROR;

	/* read all lines in the comment file */
	while(fgets(temp_buffer,sizeof(temp_buffer)-1,fp)){

		strip(temp_buffer);

		/* skip blank lines and comments */
		if(temp_buffer[0]=='#' || temp_buffer[0]=='\x0')
			continue;

		else if(!strcmp(temp_buffer,"info {"))
			data_type=XCDDEFAULT_INFO_DATA;
		else if(!strcmp(temp_buffer,"hostcomment {"))
			data_type=XCDDEFAULT_HOST_DATA;
		else if(!strcmp(temp_buffer,"servicecomment {"))
			data_type=XCDDEFAULT_SERVICE_DATA;

		else if(!strcmp(temp_buffer,"}")){

			switch(data_type){

			case XCDDEFAULT_INFO_DATA:
				break;

			case XCDDEFAULT_HOST_DATA:
			case XCDDEFAULT_SERVICE_DATA:
				add_comment((data_type==XCDDEFAULT_HOST_DATA)?HOST_COMMENT:SERVICE_COMMENT,host_name,service_description,entry_time,author,comment_data,comment_id,persistent,source);
				break;

			default:
				break;
			        }

			data_type=XCDDEFAULT_NO_DATA;

			/* free temp memory */
			free(host_name);
			free(service_description);
			free(author);
			free(comment_data);

			/* reset defaults */
			host_name=NULL;
			service_description=NULL;
			author=NULL;
			comment_data=NULL;
			comment_id=0;
			source=COMMENTSOURCE_INTERNAL;
			persistent=FALSE;
			entry_time=0L;
		        }

		else if(data_type!=XCDDEFAULT_NO_DATA){

			var=strtok(temp_buffer,"=");
			val=strtok(NULL,"\n");
			if(val==NULL)
				continue;

			switch(data_type){

			case XCDDEFAULT_INFO_DATA:
				break;

			case XCDDEFAULT_HOST_DATA:
			case XCDDEFAULT_SERVICE_DATA:
				if(!strcmp(var,"host_name"))
					host_name=strdup(val);
				else if(!strcmp(var,"service_description"))
					service_description=strdup(val);
				else if(!strcmp(var,"comment_id"))
					comment_id=strtoul(val,NULL,10);
				else if(!strcmp(var,"source"))
					source=atoi(val);
				else if(!strcmp(var,"persistent"))
					persistent=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"entry_time"))
					entry_time=strtoul(val,NULL,10);
				else if(!strcmp(var,"author"))
					author=strdup(val);
				else if(!strcmp(var,"comment_data"))
					comment_data=strdup(val);
				break;

			default:
				break;
			        }

		        }
	        }

	fclose(fp);

	return OK;
        }


