/*****************************************************************************
 *
 * DOWNTIME.H - Header file for scheduled downtime functions
 *
 * Copyright (c) 2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   10-16-2001
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


#ifndef _DOWNTIME_H
#define _DOWNTIME_H

#include "config.h"
#include "common.h"
#include "objects.h"


/* SCHEDULED_DOWNTIME_ENTRY structure */
typedef struct scheduled_downtime_struct{
	int type;
	char *host_name;
	char *service_description;
	time_t entry_time;
	time_t start_time;
	time_t end_time;
	int fixed;
	unsigned long duration;
	int downtime_id;
	char *author;
	char *comment;
#ifdef NSCORE
	int comment_id;
	int is_in_effect;
	int start_flex_downtime;
	int incremented_pending_downtime;
#endif
	struct scheduled_downtime_struct *next;
	}scheduled_downtime;



#ifdef NSCORE
int initialize_downtime_data(char *);                                /* initializes scheduled downtime data */
int cleanup_downtime_data(char *);                                   /* cleans up scheduled downtime data */

int add_new_downtime(int,char *,char *,time_t,char *,char *,time_t,time_t,int,unsigned long,int *);
int add_new_host_downtime(char *,time_t,char *,char *,time_t,time_t,int,unsigned long,int *);
int add_new_service_downtime(char *,char *,time_t,char *,char *,time_t,time_t,int,unsigned long,int *);

int delete_host_downtime(int);
int delete_service_downtime(int);
int delete_downtime(int,int);

int schedule_downtime(int,char *,char *,time_t,char *,char *,time_t,time_t,int,unsigned long);
int unschedule_downtime(int,int);

int register_downtime(int,int);
int handle_scheduled_downtime(scheduled_downtime *);

int check_pending_flex_host_downtime(host *,int);
int check_pending_flex_service_downtime(service *);

int check_for_expired_downtime(void);
#endif


#ifdef NSCGI
int read_downtime_data(char *);
#endif


int add_host_downtime(char *,time_t,char *,char *,time_t,time_t,int,unsigned long,int);
int add_service_downtime(char *,char *,time_t,char *,char *,time_t,time_t,int,unsigned long,int);
int add_downtime(int,char *,char *,time_t,char *,char *,time_t,time_t,int,unsigned long,int);

scheduled_downtime *find_host_downtime(int);
scheduled_downtime *find_service_downtime(int);

void free_downtime_data(void);                                       /* frees memory allocated to scheduled downtime list */


#endif







