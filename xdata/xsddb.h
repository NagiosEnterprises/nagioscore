/*****************************************************************************
 *
 * XSDDB.H - Header file for database status data routines
 *
 * Copyright (c) 1999-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   02-20-2003
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

#define XSDDB_PROGRAMSTATUS_TABLE	"nagios_programstatus"
#define XSDDB_HOSTSTATUS_TABLE	        "nagios_hoststatus"
#define XSDDB_SERVICESTATUS_TABLE	"nagios_servicestatus"

#define XSDDB_BUFFER_LENGTH		1024
#define XSDDB_SQL_LENGTH		2048			/* buffer length for SQL queries */


#ifdef NSCORE
int xsddb_initialize_status_data(char *);
int xsddb_cleanup_status_data(char *,int);

int xsddb_save_status_data(void);
int xsddb_save_program_status(void);
int xsddb_save_host_status(void);
int xsddb_save_service_status(void);

int xsddb_check_connection(void);
int xsddb_reconnect(void);
int xsddb_begin_transaction(void);
int xsddb_commit_transaction(void);
int xsddb_rollback_transaction(void);
int xsddb_optimize_tables(void);
int xsddb_escape_string(char *,char *);
#endif

#ifdef NSCGI
int xsddb_read_status_data(char *,int);
int xsddb_add_program_status(void);
int xsddb_add_host_status(void);
int xsddb_add_service_status(void);
#endif

int xsddb_grab_config_info(char *);
void xsddb_grab_config_directives(char *);
int xsddb_initialize(void);
int xsddb_connect(void);
int xsddb_disconnect(void);
int xsddb_query(char *);
int xsddb_free_query_memory(void);

