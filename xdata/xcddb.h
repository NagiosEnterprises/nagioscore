/*****************************************************************************
 *
 * XCDDB.H - Include file for database routines for comment data
 *
 * Copyright (c) 2000-2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   06-14-2001
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

#define XCDDB_HOSTCOMMENTS_TABLE	"hostcomments"
#define XCDDB_SERVICECOMMENTS_TABLE	"servicecomments"

#define XCDDB_BUFFER_LENGTH		1024
#define XCDDB_SQL_LENGTH		2048			/* buffer length for SQL queries */



#ifdef NSCORE
int xcddb_initialize_comment_data(char *);
int xcddb_validate_comment_data(void);
int xcddb_validate_host_comments(void);
int xcddb_validate_service_comments(void);
int xcddb_cleanup_comment_data(char *);
int xcddb_optimize_tables(void);
int xcddb_save_host_comment(char *,time_t,char *,char *,int,int *);
int xcddb_save_service_comment(char *,char *,time_t,char *,char *,int,int *);
int xcddb_delete_host_comment(int,int);
int xcddb_delete_service_comment(int,int);
int xcddb_delete_all_host_comments(char *);
int xcddb_delete_all_service_comments(char *,char *);

int xcddb_escape_string(char *,char *);
#endif

#ifdef NSCGI
int xcddb_read_comment_data(char *);
int xcddb_read_host_comments(void);
int xcddb_read_service_comments(void);
#endif


int xcddb_grab_config_info(char *);
void xcddb_grab_config_directives(char *);
int xcddb_initialize(void);
int xcddb_connect(void);
int xcddb_disconnect(void);
int xcddb_query(char *);
int xcddb_free_query_memory(void);
