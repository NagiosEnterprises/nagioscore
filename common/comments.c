/*****************************************************************************
 *
 * COMMENTS.C - Comment functions for Nagios
 *
 * Copyright (c) 1999-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   06-09-2003
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

#include "config.h"
#include "common.h"
#include "comments.h"
#include "objects.h"

/***** IMPLEMENTATION-SPECIFIC INCLUDES *****/

#ifdef USE_XCDDEFAULT
#include "../xdata/xcddefault.h"
#endif
#ifdef USE_XCDDB
#include "../xdata/xcddb.h"
#endif

#ifdef NSCORE
#include "../base/nagios.h"
#include "../base/broker.h"
#endif

#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif


comment     *comment_list=NULL;




#ifdef NSCORE

/******************************************************************/
/**************** INITIALIZATION/CLEANUP FUNCTIONS ****************/
/******************************************************************/


/* initalizes comment data */
int initialize_comment_data(char *config_file){
	int result;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XCDDEFAULT
	result=xcddefault_initialize_comment_data(config_file);
#endif
#ifdef USE_XCDDB
	result=xcddb_initialize_comment_data(config_file);
#endif

	return result;
        }


/* removes old/invalid comments */
int cleanup_comment_data(char *config_file){
	int result;
	
	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XCDDEFAULT
	result=xcddefault_cleanup_comment_data(config_file);
#endif
#ifdef USE_XCDDB
	result=xcddb_cleanup_comment_data(config_file);
#endif

	return result;
        }



/******************************************************************/
/****************** COMMENT OUTPUT FUNCTIONS **********************/
/******************************************************************/


/* adds a new host or service comment */
int add_new_comment(int type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, unsigned long *comment_id){
	int result;

	if(type==HOST_COMMENT)
		result=add_new_host_comment(host_name,entry_time,author_name,comment_data,persistent,source,comment_id);
	else
		result=add_new_service_comment(host_name,svc_description,entry_time,author_name,comment_data,persistent,source,comment_id);

	return result;
        }


/* adds a new host comment */
int add_new_host_comment(char *host_name, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, unsigned long *comment_id){
	int result;
	unsigned long new_comment_id;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XCDDEFAULT
	result=xcddefault_add_new_host_comment(host_name,entry_time,author_name,comment_data,persistent,source,&new_comment_id);
#endif
#ifdef USE_XCDDB
	result=xcddb_add_new_host_comment(host_name,entry_time,author_name,comment_data,persistent,source,&new_comment_id);
#endif

	/* save comment id */
	if(comment_id!=NULL)
		*comment_id=new_comment_id;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_comment_data(NEBTYPE_COMMENT_ADD,NEBFLAG_NONE,NEBATTR_HOST_COMMENT,host_name,NULL,entry_time,author_name,comment_data,persistent,source,new_comment_id,NULL);
#endif

	return result;
        }


/* adds a new service comment */
int add_new_service_comment(char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, unsigned long *comment_id){
	int result;
	unsigned long new_comment_id;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XCDDEFAULT
	result=xcddefault_add_new_service_comment(host_name,svc_description,entry_time,author_name,comment_data,persistent,source,&new_comment_id);
#endif
#ifdef USE_XCDDB
	result=xcddb_add_new_service_comment(host_name,svc_description,entry_time,author_name,comment_data,persistent,source,&new_comment_id);
#endif

	/* save comment id */
	if(comment_id!=NULL)
		*comment_id=new_comment_id;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_comment_data(NEBTYPE_COMMENT_ADD,NEBFLAG_NONE,NEBATTR_SERVICE_COMMENT,host_name,svc_description,entry_time,author_name,comment_data,persistent,source,new_comment_id,NULL);
#endif

	return result;
        }



/******************************************************************/
/***************** COMMENT DELETION FUNCTIONS *********************/
/******************************************************************/


/* deletes a host or service comment */
int delete_comment(int type, unsigned long comment_id){
	int result;
	comment *this_comment=NULL;
	comment *last_comment=NULL;
	comment *next_comment=NULL;

	/* find the comment we should remove */
	for(this_comment=comment_list,last_comment=comment_list;this_comment!=NULL;this_comment=next_comment){
		next_comment=this_comment->next;

		/* we found the comment we should delete */
		if(this_comment->comment_id==comment_id && this_comment->comment_type==type)
			break;

		last_comment=this_comment;
	        }

	/* remove the comment from the list in memory */
	if(this_comment!=NULL){

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_comment_data(NEBTYPE_COMMENT_DELETE,NEBFLAG_NONE,(type==HOST_COMMENT)?NEBATTR_HOST_COMMENT:NEBATTR_SERVICE_COMMENT,this_comment->host_name,this_comment->service_description,this_comment->entry_time,this_comment->author,this_comment->comment_data,this_comment->persistent,this_comment->source,comment_id,NULL);
#endif

		if(comment_list==this_comment)
			comment_list=this_comment->next;
		else
			last_comment->next=next_comment;
		
		/* free memory */
		free(this_comment->host_name);
		free(this_comment->service_description);
		free(this_comment->author);
		free(this_comment->comment_data);
		free(this_comment);

		result=OK;
	        }
	else
		result=ERROR;
	
	return result;
        }


/* deletes a host comment */
int delete_host_comment(unsigned long comment_id){
	int result;

	/* delete the comment from memory */
	delete_comment(HOST_COMMENT,comment_id);
	
	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XCDDEFAULT
	result=xcddefault_delete_host_comment(comment_id);
#endif
#ifdef USE_XCDDB
	result=xcddb_delete_host_comment(comment_id,FALSE);
#endif

	return result;
        }



/* deletes a service comment */
int delete_service_comment(unsigned long comment_id){
	int result;
	
	/* delete the comment from memory */
	delete_comment(SERVICE_COMMENT,comment_id);
	
	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XCDDEFAULT
	result=xcddefault_delete_service_comment(comment_id);
#endif
#ifdef USE_XCDDB
	result=xcddb_delete_service_comment(comment_id,FALSE);
#endif

	return result;
        }


/* deletes all comments for a particular host or service */
int delete_all_comments(int type, char *host_name, char *svc_description){
	int result;

	if(type==HOST_COMMENT)
		result=delete_all_host_comments(host_name);
	else
		result=delete_all_service_comments(host_name,svc_description);

	return result;
        }


/* deletes all comments for a particular host */
int delete_all_host_comments(char *host_name){
	int result;
	comment *temp_comment;
	comment *next_comment;

	if(host_name==NULL)
		return ERROR;
	
	for(temp_comment=comment_list;temp_comment!=NULL;temp_comment=next_comment){
		next_comment=temp_comment;
		if(temp_comment->comment_type!=HOST_COMMENT)
			continue;
		if(temp_comment->host_name==NULL)
			continue;
		if(!strcmp(temp_comment->host_name,host_name))
			delete_comment(HOST_COMMENT,temp_comment->comment_id);
	        }

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XCDDEFAULT
	result=xcddefault_delete_all_host_comments(host_name);
#endif
#ifdef USE_XCDDB
	result=xcddb_delete_all_host_comments(host_name);
#endif

	return result;
        }


/* deletes all comments for a particular service */
int delete_all_service_comments(char *host_name, char *svc_description){
	int result;
	comment *temp_comment;
	comment *next_comment;

	if(host_name==NULL || svc_description==NULL)
		return ERROR;
	
	for(temp_comment=comment_list;temp_comment!=NULL;temp_comment=next_comment){
		next_comment=temp_comment;
		if(temp_comment->comment_type!=SERVICE_COMMENT)
			continue;
		if(temp_comment->host_name==NULL || temp_comment->service_description==NULL)
			continue;
		if(!strcmp(temp_comment->host_name,host_name) && !strcmp(temp_comment->service_description,svc_description))
			delete_comment(SERVICE_COMMENT,temp_comment->comment_id);
	        }

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XCDDEFAULT
	result=xcddefault_delete_all_service_comments(host_name,svc_description);
#endif
#ifdef USE_XCDDB
	result=xcddb_delete_all_service_comments(host_name,svc_description);
#endif

	return result;
        }




#endif




/******************************************************************/
/********************** INPUT FUNCTIONS ***************************/
/******************************************************************/


int read_comment_data(char *main_config_file){
	int result;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XCDDEFAULT
	result=xcddefault_read_comment_data(main_config_file);
#endif
#ifdef USE_XCDDB
	result=xcddb_read_comment_data(main_config_file);
#endif

	return result;
        }



/******************************************************************/
/******************** ADDITION FUNCTIONS **************************/
/******************************************************************/


/* adds a host comment to the list in memory */
int add_host_comment(char *host_name, time_t entry_time, char *author, char *comment_data, unsigned long comment_id, int persistent, int source){
	int result;

	result=add_comment(HOST_COMMENT,host_name,NULL,entry_time,author,comment_data,comment_id,persistent,source);

	return result;
        }



/* adds a service comment to the list in memory */
int add_service_comment(char *host_name, char *svc_description, time_t entry_time, char *author, char *comment_data, unsigned long comment_id, int persistent, int source){
	int result;

	result=add_comment(SERVICE_COMMENT,host_name,svc_description,entry_time,author,comment_data,comment_id,persistent,source);

	return result;
        }



/* adds a comment to the list in memory */
int add_comment(int comment_type, char *host_name, char *svc_description, time_t entry_time, char *author, char *comment_data, unsigned long comment_id, int persistent, int source){
	comment *new_comment=NULL;
	comment *last_comment=NULL;
	comment *temp_comment=NULL;

	/* make sure we have the data we need */
	if(host_name==NULL || author==NULL || comment_data==NULL || (comment_type==SERVICE_COMMENT && svc_description==NULL))
		return ERROR;

	/* allocate memory for the comment */
	new_comment=(comment *)malloc(sizeof(comment));
	if(new_comment==NULL)
		return ERROR;

	new_comment->host_name=strdup(host_name);
	if(new_comment->host_name==NULL){
		free(new_comment);
		return ERROR;
	        }

	if(comment_type==SERVICE_COMMENT){
		new_comment->service_description=strdup(svc_description);
		if(new_comment->service_description==NULL){
			free(new_comment->host_name);
			free(new_comment);
			return ERROR;
		        }
	        }
	else
		new_comment->service_description=NULL;

	new_comment->author=strdup(author);
	if(new_comment->author==NULL){
		if(new_comment->service_description!=NULL)
			free(new_comment->service_description);
		free(new_comment->host_name);
		free(new_comment);
		return ERROR;
	        }

	new_comment->comment_data=strdup(comment_data);
	if(new_comment->comment_data==NULL){
		free(new_comment->author);
		if(new_comment->service_description!=NULL)
			free(new_comment->service_description);
		free(new_comment->host_name);
		free(new_comment);
		return ERROR;
	        }

	new_comment->comment_type=comment_type;
	new_comment->source=source;
	new_comment->entry_time=entry_time;
	new_comment->comment_id=comment_id;
	new_comment->persistent=(persistent>0)?TRUE:FALSE;


	/* add new comment to comment list, sorted by comment id */
	last_comment=comment_list;
	for(temp_comment=comment_list;temp_comment!=NULL;temp_comment=temp_comment->next){
		if(new_comment->comment_id<temp_comment->comment_id){
			new_comment->next=temp_comment;
			if(temp_comment==comment_list)
				comment_list=new_comment;
			else
				last_comment->next=new_comment;
			break;
		        }
		else
			last_comment=temp_comment;
	        }
	if(comment_list==NULL){
		new_comment->next=NULL;
		comment_list=new_comment;
	        }
	else if(temp_comment==NULL){
		new_comment->next=NULL;
		last_comment->next=new_comment;
	        }

#ifdef NSCORE
#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_comment_data(NEBTYPE_COMMENT_LOAD,NEBFLAG_NONE,(comment_type==HOST_COMMENT)?NEBATTR_HOST_COMMENT:NEBATTR_SERVICE_COMMENT,host_name,svc_description,entry_time,author,comment_data,persistent,source,comment_id,NULL);
#endif
#endif

	return OK;
        }




/******************************************************************/
/********************* CLEANUP FUNCTIONS **************************/
/******************************************************************/

/* frees memory allocated for the comment data */
void free_comment_data(void){
	comment *this_comment;
	comment *next_comment;

	/* free memory for the comment list */
	for(this_comment=comment_list;this_comment!=NULL;this_comment=next_comment){
		next_comment=this_comment->next;
		free(this_comment->host_name);
		free(this_comment->service_description);
		free(this_comment->author);
		free(this_comment->comment_data);
		free(this_comment);
	        }

	/* reset list pointer */
	comment_list=NULL;

	return;
        }




/******************************************************************/
/********************* UTILITY FUNCTIONS **************************/
/******************************************************************/

/* get the number of comments associated wth a particular host */
int number_of_host_comments(char *host_name){
	comment *temp_comment;
	int total_comments=0;

	if(host_name==NULL)
		return 0;

	for(temp_comment=comment_list;temp_comment!=NULL;temp_comment=temp_comment->next){
		if(temp_comment->comment_type==HOST_COMMENT && !strcmp(temp_comment->host_name,host_name))
			total_comments++;
	        }

	return total_comments;
        }


/* get the number of comments associated wth a particular service */
int number_of_service_comments(char *host_name, char *svc_description){
	comment *temp_comment;
	int total_comments=0;

	if(host_name==NULL || svc_description==NULL)
		return 0;

	for(temp_comment=comment_list;temp_comment!=NULL;temp_comment=temp_comment->next){
		if(temp_comment->comment_type==SERVICE_COMMENT && !strcmp(temp_comment->host_name,host_name) && !strcmp(temp_comment->service_description,svc_description))
			total_comments++;
	        }

	return total_comments;
        }


/* find a service comment by id */
comment *find_service_comment(unsigned long comment_id){
	
	return find_comment(comment_id,SERVICE_COMMENT);
        }


/* find a host comment by id */
comment *find_host_comment(unsigned long comment_id){
	
	return find_comment(comment_id,HOST_COMMENT);
        }


/* find a comment by id */
comment *find_comment(unsigned long comment_id, int comment_type){
	comment *temp_comment;

	for(temp_comment=comment_list;temp_comment!=NULL;temp_comment=temp_comment->next){
		if(temp_comment->comment_id==comment_id && temp_comment->comment_type==comment_type)
			return temp_comment;
	        }

	return NULL;
        }





