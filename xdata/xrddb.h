/*****************************************************************************
 *
 * XRDDB.H - Header file for database state retention routines
 *
 * Copyright (c) 2001 Ethan Galstad (nagios@nagios.org)
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

#define XRDDB_PROGRAMRETENTION_TABLE	"programretention"
#define XRDDB_HOSTRETENTION_TABLE 	"hostretention"
#define XRDDB_SERVICERETENTION_TABLE	"serviceretention"

#define XRDDB_BUFFER_LENGTH		1024
#define XRDDB_SQL_LENGTH		2048			/* buffer length for SQL queries */


int xrddb_save_state_information(char *);        /* saves all host and service state information */
int xrddb_read_state_information(char *);        /* reads in initial host and service state information */

int xrddb_save_program_information(void);
int xrddb_save_host_information(void);
int xrddb_save_service_information(void);

int xrddb_read_program_information(void);
int xrddb_read_host_information(void);
int xrddb_read_service_information(void);

int xrddb_initialize(void);
int xrddb_grab_config_info(char *);
int xrddb_connect(void);
int xrddb_disconnect(void);
int xrddb_query(char *);
int xrddb_free_query_memory(void);
int xrddb_escape_string(char *,char *);

int xrddb_begin_transaction(void);
int xrddb_rollback_transaction(void);
int xrddb_commit_transaction(void);

int xrddb_optimize_tables(void);



