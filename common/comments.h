/*****************************************************************************
 *
 * COMMENTS.H - Header file for comment functions
 *
 * Copyright (c) 1999-2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   05-05-2001
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

#ifdef NSCORE
int initialize_comment_data(char *);                                /* initializes comment data */
int cleanup_comment_data(char *);                                   /* cleans up comment data */
int save_comment(int,char *,char *,time_t,char *,char *,int,int *);       /* saves a host or service comment */
int save_host_comment(char *,time_t,char *,char *,int,int *);             /* saves a host comment */
int save_service_comment(char *,char *,time_t,char *,char *,int,int *);   /* saves a service comment */
int delete_comment(int,int);                                        /* deletes a host or service comment */
int delete_host_comment(int);                                       /* deletes a host comment */
int delete_service_comment(int);                                    /* deletes a service comment */
int delete_all_comments(int,char *,char *);                         /* deletes all comments for a particular host or service */
int delete_all_host_comments(char *);                               /* deletes all comments for a specific host */
int delete_all_service_comments(char *,char *);                     /* deletes all comments for a specific service */

#endif


#ifdef NSCGI

/* COMMENT structure */
typedef struct comment_struct{
	int 	comment_type;
	int 	comment_id;
	int     persistent;
	time_t 	entry_time;
	char 	*host_name;
	char 	*service_description;
	char 	*author;
	char 	*comment_data;
	struct 	comment_struct *next;
        }comment;

int read_comment_data(char *);                                        /* reads all host and service comments */
int add_comment(int,char *,char *,time_t,char *,char *,int,int);      /* adds a comment (host or service) */
int add_host_comment(char *,time_t,char *,char *,int,int);            /* adds a host comment */
int add_service_comment(char *,char *,time_t,char *,char *,int,int);  /* adds a service comment */
void free_comment_data(void);                                         /* frees memory allocated to the comment list */
int number_of_host_comments(char *);			              /* returns the number of comments associated with a particular host */
int number_of_service_comments(char *, char *);		              /* returns the number of comments associated with a particular service */
comment *find_comment(int,int);                                       /* finds a specific comment */
comment *find_service_comment(int);                                   /* finds a specific service comment */
comment *find_host_comment(int);                                      /* finds a specific host comment */

#endif

#endif







