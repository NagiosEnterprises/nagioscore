/*****************************************************************************
 *
 * COMMENTS.H - Header file for comment functions
 *
 * Copyright (c) 1999-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   03-21-2003
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


#ifndef _COMMENTS_H
#define _COMMENTS_H

#include "config.h"
#include "common.h"
#include "objects.h"


#define COMMENTSOURCE_INTERNAL  0
#define COMMENTSOURCE_EXTERNAL  1


/* COMMENT structure */
typedef struct comment_struct{
	int 	comment_type;
	unsigned long comment_id;
	int     source;
	int     persistent;
	time_t 	entry_time;
	char 	*host_name;
	char 	*service_description;
	char 	*author;
	char 	*comment_data;
	struct 	comment_struct *next;
        }comment;


#ifdef NSCORE
int initialize_comment_data(char *);                                /* initializes comment data */
int cleanup_comment_data(char *);                                   /* cleans up comment data */
int add_new_comment(int,char *,char *,time_t,char *,char *,int,int,unsigned long *);       /* adds a new host or service comment */
int add_new_host_comment(char *,time_t,char *,char *,int,int,unsigned long *);             /* adds a new host comment */
int add_new_service_comment(char *,char *,time_t,char *,char *,int,int,unsigned long *);   /* adds a new ervice comment */
int delete_comment(int,unsigned long);                              /* deletes a host or service comment */
int delete_host_comment(unsigned long);                             /* deletes a host comment */
int delete_service_comment(unsigned long);                          /* deletes a service comment */
int delete_all_comments(int,char *,char *);                         /* deletes all comments for a particular host or service */
int delete_all_host_comments(char *);                               /* deletes all comments for a specific host */
int delete_all_service_comments(char *,char *);                     /* deletes all comments for a specific service */
#endif

comment *find_comment(unsigned long,int);                             /* finds a specific comment */
comment *find_service_comment(unsigned long);                         /* finds a specific service comment */
comment *find_host_comment(unsigned long);                            /* finds a specific host comment */
int number_of_host_comments(char *);			              /* returns the number of comments associated with a particular host */
int number_of_service_comments(char *, char *);		              /* returns the number of comments associated with a particular service */

int read_comment_data(char *);                                            /* reads all host and service comments */
int add_comment(int,char *,char *,time_t,char *,char *,unsigned long,int,int);      /* adds a comment (host or service) */
int add_host_comment(char *,time_t,char *,char *,unsigned long,int,int);            /* adds a host comment */
int add_service_comment(char *,char *,time_t,char *,char *,unsigned long,int,int);  /* adds a service comment */
void free_comment_data(void);                                             /* frees memory allocated to the comment list */

#endif







