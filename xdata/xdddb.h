/*****************************************************************************
 *
 * XDDDB.H - Include file for database routines for downtime data
 *
 * Copyright (c) 2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   07-09-2001
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

#define XDDDB_HOSTDOWNTIME_TABLE	"hostdowntime"
#define XDDDB_SERVICEDOWNTIME_TABLE	"servicedowntime"

#define XDDDB_BUFFER_LENGTH		1024
#define XDDDB_SQL_LENGTH		2048			/* buffer length for SQL queries */



#ifdef NSCORE
int xdddb_initialize_downtime_data(char *);
int xdddb_validate_downtime_data(void);
int xdddb_cleanup_downtime_data(char *);

int xdddb_validate_host_downtime(void);
int xdddb_validate_service_downtime(void);
int xdddb_optimize_tables(void);

int xdddb_save_host_downtime(char *,time_t,char *,char *,time_t,time_t,int,unsigned long,int *);
int xdddb_save_service_downtime(char *,char *,time_t,char *,char *,time_t,time_t,int,unsigned long,int *);

int xdddb_delete_host_downtime(int,int);
int xdddb_delete_service_downtime(int,int);

int xcddb_escape_string(char *,char *);
#endif



int xdddb_read_downtime_data(char *);

int xdddb_read_host_downtime(void);
int xdddb_read_service_downtime(void);


int xdddb_grab_config_info(char *);
void xdddb_grab_config_directives(char *);

int xdddb_initialize(void);
int xdddb_connect(void);
int xdddb_disconnect(void);
int xdddb_query(char *);
int xdddb_free_query_memory(void);
