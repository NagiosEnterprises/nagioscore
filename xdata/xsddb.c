/*****************************************************************************
 *
 * XSDDB.C - Database routines for status data
 *
 * Copyright (c) 2000-2001 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   08-04-2001
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
#include "../common/statusdata.h"

#ifdef NSCORE
#include "../base/nagios.h"
#endif

#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif

#include "xsddb.h"

/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/

#ifdef USE_XSDMYSQL
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#endif

#ifdef USE_XSDPGSQL
#include <pgsql/libpq-fe.h>
#endif


char xsddb_host[XSDDB_BUFFER_LENGTH]="";
char xsddb_username[XSDDB_BUFFER_LENGTH]="";
char xsddb_password[XSDDB_BUFFER_LENGTH]="";
char xsddb_database[XSDDB_BUFFER_LENGTH]="";
int xsddb_port=3306;

#ifdef USE_XSDMYSQL
MYSQL xsddb_mysql;
#endif

#ifdef USE_XSDPGSQL
PGconn *xsddb_pgconn=NULL;
PGresult *xsddb_pgres=NULL;
#endif


int xsddb_init=OK;
int xsddb_connection_error=FALSE;
int xsddb_reconnect_time=60;
unsigned long xsddb_last_reconnect_attempt=0L;
int xsddb_complain_time=600;
unsigned long xsddb_last_complain_time=0L;
int xsddb_use_transactions=FALSE;
int xsddb_optimize_data=TRUE;
int xsddb_optimize_interval=3600;             /* optimize status tables every hour */
unsigned long xsddb_last_optimize_time=0L;



/******************************************************************/
/***************** COMMON CONFIG INITIALIZATION  ******************/
/******************************************************************/

/* grab configuration information */
int xsddb_grab_config_info(char *config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	FILE *fp;
#ifdef NSCORE
	FILE *fp2;
	char *temp_buffer;
#endif

	/* DBMS-specifc defaults */
#ifdef USE_XSDMYSQL
	xsddb_port=3306;
#endif
#ifdef USE_XSDPGSQL
	xsddb_port=5432;
#endif

	/*** CORE PASSES IN MAIN CONFIG FILE, CGIS PASS IN CGI CONFIG FILE! ***/

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

#ifdef NSCORE
		/* core program reads variables from the resource file */
		if(strstr(input_buffer,"resource_file")==input_buffer){

			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;

			/* open the resource file for reading */
			fp2=fopen(temp_buffer,"r");
			if(fp2==NULL)
				continue;

			/* read in all lines from the resource config file */
			for(fgets(input_buffer,sizeof(input_buffer)-1,fp2);!feof(fp2);fgets(input_buffer,sizeof(input_buffer)-1,fp2)){

				/* skip blank lines and comments */
				if(input_buffer[0]=='#' || input_buffer[0]=='\x0' || input_buffer[0]=='\n' || input_buffer[0]=='\r')
					continue;

				strip(input_buffer);

				xsddb_grab_config_directives(input_buffer);
			        }

			fclose(fp2);
		        }
#endif

#ifdef NSCGI
		/* CGIs read directives directly from the CGI config file */
		xsddb_grab_config_directives(input_buffer);
#endif
	        }

	fclose(fp);

	return OK;
        }


void xsddb_grab_config_directives(char *input_buffer){
	char *temp_buffer;

	/* server name/address */
	if(strstr(input_buffer,"xsddb_host")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xsddb_host,temp_buffer,sizeof(xsddb_host)-1);
		xsddb_host[sizeof(xsddb_host)-1]='\x0';
	        }

	/* username */
	else if(strstr(input_buffer,"xsddb_username")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xsddb_username,temp_buffer,sizeof(xsddb_username)-1);
		xsddb_username[sizeof(xsddb_username)-1]='\x0';
	        }

	/* password */
	else if(strstr(input_buffer,"xsddb_password")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xsddb_password,temp_buffer,sizeof(xsddb_password)-1);
		xsddb_password[sizeof(xsddb_password)-1]='\x0';
	        }

	/* database */
	else if(strstr(input_buffer,"xsddb_database")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xsddb_database,temp_buffer,sizeof(xsddb_database)-1);
		xsddb_database[sizeof(xsddb_database)-1]='\x0';
	        }

	/* port */
	else if(strstr(input_buffer,"xsddb_port")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		xsddb_port=atoi(temp_buffer);
	        }

	/* optimize data option */
	else if(strstr(input_buffer,"xsddb_optimize_data")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		xsddb_optimize_data=(atoi(temp_buffer)>0)?TRUE:FALSE;
	        }

	/* optimization interval */
	else if(strstr(input_buffer,"xsddb_optimize_interval")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		xsddb_optimize_interval=atoi(temp_buffer);
		if(xsddb_optimize_interval<=0)
			xsddb_optimize_interval=3600;
	        }


	return;
        }



/**********************************************************/
/***************** CONNECTION FUNCTIONS *******************/
/**********************************************************/

/* initializes any database structures we may need */
int xsddb_initialize(void){
#ifdef NSCORE
	char buffer[MAX_INPUT_BUFFER];
#endif

#ifdef USE_XSDMYSQL
	/* initialize mysql  */
	if(!mysql_init(&xsddb_mysql)){

#ifdef NSCORE
		snprintf(buffer,sizeof(buffer)-1,"Error: mysql_init() failed for status data\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
#endif

		return ERROR;
	        }

	xsddb_use_transactions=FALSE;
#endif

#ifdef USE_XSDPGSQL
	xsddb_use_transactions=TRUE;
#endif

	return OK;
        }


/* opens a connection to the db server */
int xsddb_connect(void){
#ifdef NSCORE
	char buffer[MAX_INPUT_BUFFER];
#endif
#ifdef USE_XSDPGSQL
	char connect_string[XSDDB_BUFFER_LENGTH];
#endif

#ifdef USE_XSDMYSQL
	/* establish a connection to the MySQL server */
	if(!mysql_real_connect(&xsddb_mysql,xsddb_host,xsddb_username,xsddb_password,xsddb_database,xsddb_port,NULL,0)){

		mysql_close(&xsddb_mysql);

#ifdef NSCORE
		/* complain if we don't already know theres a problem */
		if(xsddb_connection_error==FALSE){
			snprintf(buffer,sizeof(buffer)-1,"Error: Could not connect to MySQL database '%s' on host '%s' using username '%s' and password '%s'.  Status data will not be saved!\n",xsddb_database,xsddb_host,xsddb_username,xsddb_password);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
		        }
#endif

		return ERROR;
	        }
#endif

#ifdef USE_XSDPGSQL
	/* establish a connection to the PostgreSQL server */
	snprintf(connect_string,sizeof(connect_string)-1,"host='%s' port=%d dbname='%s' user='%s' password='%s'",xsddb_host,xsddb_port,xsddb_database,xsddb_username,xsddb_password);
	connect_string[sizeof(connect_string)-1]='\x0';
	xsddb_pgconn=PQconnectdb(connect_string);

	if(PQstatus(xsddb_pgconn)==CONNECTION_BAD){
		
		/* free memory */
		PQfinish(xsddb_pgconn);

#ifdef NSCORE
		/* complain if we don't already know theres a problem */
		if(xsddb_connection_error==FALSE){
			snprintf(buffer,sizeof(buffer)-1,"Error: Could not connect to PostgreSQL database '%s' on host '%s' using username '%s' and password '%s'.  Status data will not be saved!\n",xsddb_database,xsddb_host,xsddb_username,xsddb_password);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
		        }
#endif
		return ERROR;
	        }
#endif

	return OK;
        }


/* closes connection to db server */
int xsddb_disconnect(void){

#ifdef USE_XSDMYSQL
	/* close database connection */
	mysql_close(&xsddb_mysql);
#endif

#ifdef USE_XSDPGSQL
	/* close database connection and cleanup */
	if(PQstatus(xsddb_pgconn)!=CONNECTION_BAD)
		PQfinish(xsddb_pgconn);
#endif

	return OK;
        }



#ifdef NSCORE

/* check the connection to the MySQL server */
int xsddb_check_connection(void){
	char buffer[MAX_INPUT_BUFFER];
	int connection_lost=FALSE;
#ifdef USE_XSDMYSQL
	unsigned int mysql_error;
#endif


#ifdef USE_XSDMYSQL

	/* get the last MySQL error */
	mysql_error=mysql_errno(&xsddb_mysql);

	/* see if we lost the connection to the MySQL server */
	if(mysql_error==CR_SERVER_LOST || mysql_error==CR_SERVER_GONE_ERROR){

		/* close the connection to the database server */		
		mysql_close(&xsddb_mysql);

		connection_lost=TRUE;
	        }
#endif

#ifdef USE_XSDPGSQL

	/* check the status of the connection to the PostgreSQL server */
	if(PQstatus(xsddb_pgconn)==CONNECTION_BAD){

		/* close the connection to the db server and cleanup */
		PQfinish(xsddb_pgconn);

		connection_lost=TRUE;
	        }
#endif

	/* handle problems with the server or connection */
	if(connection_lost==TRUE){

		snprintf(buffer,sizeof(buffer)-1,"Warning: The connection to database server used to store status data has been been lost.  I'll try and re-connect soon...\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);

		/* set the connection error flag */
		xsddb_connection_error=TRUE;

		/* don't try and reconnect right away... */
		xsddb_last_reconnect_attempt=(unsigned long)time(NULL);

		return ERROR;
	        }

	return OK;
        }


/* try and reconnect to the MySQL server */
int xsddb_reconnect(void){
	char buffer[MAX_INPUT_BUFFER];
	time_t current_time;

	time(&current_time);

	/* we haven't waited long enough to try and reconnect... */
	if(((unsigned long)current_time-xsddb_last_reconnect_attempt)<xsddb_reconnect_time)
		return ERROR;

	/* try and reconnect... */
	if(xsddb_connect()==ERROR){

		/* reset the last re-connect attempt time */
		xsddb_last_reconnect_attempt=(unsigned long)current_time;


		/* complain */
		if(((unsigned long)current_time-xsddb_last_complain_time)>=xsddb_complain_time){

			xsddb_last_complain_time=(unsigned long)current_time;

			snprintf(buffer,sizeof(buffer)-1,"Error: Could not re-connect to database server on host '%s' for status data.  I'll keep trying every %d seconds...",xsddb_host,xsddb_reconnect_time);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
		        }

		return ERROR;
	        }

	/* we were able to reconnect successfully! */
	xsddb_connection_error=FALSE;

	snprintf(buffer,sizeof(buffer)-1,"Good news!  We were able to re-connect to database server on host '%s' for status data.",xsddb_host);
	buffer[sizeof(buffer)-1]='\x0';
	write_to_logs_and_console(buffer,NSLOG_INFO_MESSAGE,TRUE);

	return OK;
        }
#endif


/* executes a SQL query */
int xsddb_query(char *sql_statement){
	int result=OK;

#ifdef DEBUG0
	printf("xsddb_query() start\n");
#endif

#ifdef DEBUG5
	printf("XSDDB_QUERY: %s\n",sql_statement);
#endif

#ifdef USE_XSDMYSQL
	if(mysql_query(&xsddb_mysql,sql_statement))
		result=ERROR;
#endif

#ifdef USE_XSDPGSQL
	xsddb_pgres=PQexec(xsddb_pgconn,sql_statement);
	if(xsddb_pgres==NULL || PQresultStatus(xsddb_pgres)==PGRES_FATAL_ERROR || PQresultStatus(xsddb_pgres)==PGRES_BAD_RESPONSE){
		PQclear(xsddb_pgres);
		result=ERROR;
	        }
#endif

#ifdef DEBUG5
	if(result==ERROR)
		printf("\tAn error occurred in the query!\n");
	printf("\tXSDDB_QUERY Done.\n");
#endif

#ifdef DEBUG0
	printf("xsddb_query() end\n");
#endif

	return result;
        }


/* frees memory associated with query results */
int xsddb_free_query_memory(void){

#ifdef DEBUG0
	printf("xsddb_free_query_memory() start\n");
#endif

#ifdef USE_XSDPGSQL
	PQclear(xsddb_pgres);
#endif
	
#ifdef DEBUG0
	printf("xsddb_free_query_memory() end\n");
#endif
	
	return OK;
        }



#ifdef NSCORE

/**********************************************************/
/**************** TRANSACTIONS FUNCTIONS ******************/
/**********************************************************/

/* begins a transaction */
int xsddb_begin_transaction(void){

	if(xsddb_use_transactions==FALSE)
		return OK;

	xsddb_query("BEGIN TRANSACTION");
	xsddb_free_query_memory();

	return OK;
        }


/* cancels a transaction */
int xsddb_rollback_transaction(void){

	if(xsddb_use_transactions==FALSE)
		return OK;

	xsddb_query("ROLLBACK");
	xsddb_free_query_memory();

	return OK;
        }


/* commits a transaction */
int xsddb_commit_transaction(void){

	if(xsddb_use_transactions==FALSE)
		return OK;

	xsddb_query("COMMIT");
	xsddb_free_query_memory();

	return OK;
        }




/******************************************************************/
/******************** OPTIMIZATION FUNCTIONS **********************/
/******************************************************************/

/* optimizes data in status tables */
int xsddb_optimize_tables(void){
#ifdef USE_XSDPGSQL
	char sql_statement[XSDDB_SQL_LENGTH];
#endif
	time_t current_time;

#ifdef DEBUG0
	printf("xsddb_optimize_tables() start\n");
#endif

	/* don't optimize tables if user doesn't want us to */
	if(xsddb_optimize_data==FALSE)
		return OK;

	time(&current_time);
	if(((unsigned long)current_time-xsddb_last_optimize_time)>xsddb_optimize_interval){

		xsddb_last_optimize_time=(unsigned long)current_time;

#ifdef USE_XSDPGSQL

		/* VACUUM program, host, and service status tables (user we're authenticated as must be owner of tables) */

		snprintf(sql_statement,sizeof(sql_statement)-1,"VACUUM %s",XSDDB_PROGRAMSTATUS_TABLE);
		sql_statement[sizeof(sql_statement)-1]='\x0';
		xsddb_query(sql_statement);
		xsddb_free_query_memory();

		snprintf(sql_statement,sizeof(sql_statement)-1,"VACUUM %s",XSDDB_HOSTSTATUS_TABLE);
		sql_statement[sizeof(sql_statement)-1]='\x0';
		xsddb_query(sql_statement);
		xsddb_free_query_memory();

		snprintf(sql_statement,sizeof(sql_statement)-1,"VACUUM %s",XSDDB_SERVICESTATUS_TABLE);
		sql_statement[sizeof(sql_statement)-1]='\x0';
		xsddb_query(sql_statement);
		xsddb_free_query_memory();
#endif
	        }

#ifdef DEBUG0
	printf("xsddb_optimize_tables() end\n");
#endif

	return OK;
        }




/******************************************************************/
/********************* DATA OUTPUT FUNCTIONS **********************/
/******************************************************************/

/* initialize status data */
int xsddb_initialize_status_data(char *main_config_file){
	char sql_statement[XSDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result=OK;

#ifdef DEBUG0
	printf("xsddb_initialize_status_data() start\n");
#endif

	xsddb_init=FALSE;
	xsddb_last_reconnect_attempt=0L;

	/* grab configuration information */
	if(xsddb_grab_config_info(main_config_file)==ERROR){

		snprintf(buffer,sizeof(buffer)-1,"Error: Failed to grab database configuration information for status data\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* initialize database structures */
	if(xsddb_initialize()==ERROR)
		return ERROR;

	/* things look okay so far... */
	xsddb_init=OK;

	/* establish a connection to the database (this connection stays open while Nagios is running) */
	if(xsddb_connect()==ERROR){
		xsddb_connection_error=TRUE;
		return ERROR;
	        }


	/***** BEGIN TRANSACTION *****/
	xsddb_begin_transaction();

	/* delete all entries from the program status table */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s;",XSDDB_PROGRAMSTATUS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xsddb_query(sql_statement);
	xsddb_free_query_memory();
	if(result==ERROR){
		xsddb_rollback_transaction();
		return ERROR;
	        }

	/* delete all entries from the host status table */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s;",XSDDB_HOSTSTATUS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xsddb_query(sql_statement);
	xsddb_free_query_memory();
	if(result==ERROR){
		xsddb_rollback_transaction();
		return ERROR;
	        }

	/* delete all entries from the service status table */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s;",XSDDB_SERVICESTATUS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xsddb_query(sql_statement);
	xsddb_free_query_memory();
	if(result==ERROR){
		xsddb_rollback_transaction();
		return ERROR;
	        }

	/***** COMMIT TRANSACTION *****/
	xsddb_commit_transaction();

	/* optimize status data tables */
	xsddb_optimize_tables();

#ifdef DEBUG0
	printf("xsddb_initialize_status_data() end\n");
#endif

	return OK;
        }



/* cleanup status data before terminating */
int xsddb_cleanup_status_data(char *main_config_file, int delete_status_data){
	char sql_statement[XSDDB_SQL_LENGTH];

#ifdef DEBUG0
	printf("xsddb_cleanup_status_data() start\n");
#endif

	if(xsddb_init==ERROR)
		return ERROR;

	/* if we should delete the status data... */
	if(delete_status_data==TRUE){

#ifdef DEBUG1
		printf("Deleting old status data...\n");
#endif

		/* delete all entries from the program status table */
		snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s",XSDDB_PROGRAMSTATUS_TABLE);
		sql_statement[sizeof(sql_statement)-1]='\x0';
		xsddb_query(sql_statement);
		xsddb_free_query_memory();

		/* delete all entries from the host status table */
		snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s",XSDDB_HOSTSTATUS_TABLE);
		sql_statement[sizeof(sql_statement)-1]='\x0';
		xsddb_query(sql_statement);
		xsddb_free_query_memory();

		/* delete all entries from the service status table */
		snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s",XSDDB_SERVICESTATUS_TABLE);
		sql_statement[sizeof(sql_statement)-1]='\x0';
		xsddb_query(sql_statement);
		xsddb_free_query_memory();

#ifdef DEBUG1
		printf("Done deleting old status data.\n");
#endif
	        }

	/* close the connection to the database server */		
	xsddb_disconnect();

#ifdef DEBUG0
	printf("xsddb_cleanup_status_data() end\n");
#endif

	return OK;
        }



/* starts an aggregated dump of the status data */
int xsddb_begin_aggregated_dump(void){
	char sql_statement[XSDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result=OK;

#ifdef DEBUG0
	printf("xsddb_begin_aggregated_dump() start\n");
#endif

	/* try and reconnect to the MySQL server if necessary... */
	if(xsddb_connection_error==TRUE){
		if(xsddb_reconnect()==ERROR)
			return ERROR;
	        }

	/***** BEGIN TRANSACTION *****/
	xsddb_begin_transaction();

#ifdef USE_XSDMYSQL
	/* lock tables */
	snprintf(sql_statement,sizeof(sql_statement)-1,"LOCK TABLES %s WRITE, %s WRITE, %s WRITE",XSDDB_PROGRAMSTATUS_TABLE,XSDDB_HOSTSTATUS_TABLE,XSDDB_SERVICESTATUS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	if(xsddb_query(sql_statement)==ERROR){

		xsddb_rollback_transaction();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not lock status data tables in database '%s'\n",xsddb_database);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }
#endif

	/* delete all entries from the program status table */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s;",XSDDB_PROGRAMSTATUS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xsddb_query(sql_statement);
	xsddb_free_query_memory();
	if(result==ERROR){

		xsddb_rollback_transaction();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not delete old program status data in table '%s' of database '%s'\n",XSDDB_PROGRAMSTATUS_TABLE,xsddb_database);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* delete all entries from the host status table */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s;",XSDDB_HOSTSTATUS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xsddb_query(sql_statement);
	xsddb_free_query_memory();
	if(result==ERROR){

		xsddb_rollback_transaction();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not delete old host status data in table '%s' of database '%s'\n",XSDDB_HOSTSTATUS_TABLE,xsddb_database);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* delete all entries from the service status table */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s;",XSDDB_SERVICESTATUS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xsddb_query(sql_statement);
	xsddb_free_query_memory();
	if(result==ERROR){

		xsddb_rollback_transaction();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not delete old service status data in table '%s' of database '%s'\n",XSDDB_SERVICESTATUS_TABLE,xsddb_database);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

#ifdef DEBUG0
	printf("xsddb_begin_aggregated_dump() end\n");
#endif

	return OK;
        }



/* finishes an aggregated dump of the status data */
int xsddb_end_aggregated_dump(void){
	char sql_statement[XSDDB_SQL_LENGTH];

#ifdef DEBUG0
	printf("xsddb_end_aggregated_dump() start\n");
#endif

	/* try and reconnect to the database server if necessary... */
	if(xsddb_connection_error==TRUE){
		if(xsddb_reconnect()==ERROR)
			return ERROR;
	        }

#ifdef USE_XSDMYSQL
	/* unlock tables */
	snprintf(sql_statement,sizeof(sql_statement)-1,"UNLOCK TABLES");
	sql_statement[sizeof(sql_statement)-1]='\x0';
	xsddb_query(sql_statement);
#endif

	/***** COMMIT TRANSACTION *****/
	xsddb_commit_transaction();

	/* optimize status data tables */
	xsddb_optimize_tables();

#ifdef DEBUG0
	printf("xsddb_end_aggregated_dump() end\n");
#endif

	return OK;
        }



/* updates program status data */
int xsddb_update_program_status(time_t _program_start, int _nagios_pid, int _daemon_mode, time_t _last_command_check, time_t _last_log_rotation, int _enable_notifications, int _execute_service_checks, int _accept_passive_service_checks, int _enable_event_handlers, int _obsess_over_services, int _enable_flap_detection, int _enable_failure_prediction, int _process_performance_data, int aggregated_dump){
	char sql_statement[XSDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	time_t current_time;
	int result;

#ifdef DEBUG0
	printf("xsddb_update_program_status() start\n");
#endif

	if(xsddb_init==ERROR)
		return ERROR;

	/* try and reconnect to the database server if necessary... */
	if(xsddb_connection_error==TRUE){
		if(xsddb_reconnect()==ERROR)
			return ERROR;
	        }

	/* get current time */
	time(&current_time);

	/* construct the SQL statement */
	
	/* we're doing an aggregated dump, so we use INSERTs... */
	if(aggregated_dump==TRUE){
#ifdef USE_XSDMYSQL
		snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (last_update,program_start,nagios_pid,daemon_mode,last_command_check,last_log_rotation,enable_notifications,execute_service_checks,accept_passive_service_checks,enable_event_handlers,obsess_over_services,enable_flap_detection,enable_failure_prediction,process_performance_data) VALUES (FROM_UNIXTIME(%lu),FROM_UNIXTIME(%lu),'%d','%d',FROM_UNIXTIME(%lu),FROM_UNIXTIME(%lu),'%d','%d','%d','%d','%d','%d','%d','%d')",XSDDB_PROGRAMSTATUS_TABLE,(unsigned long)current_time,(unsigned long)_program_start,_nagios_pid,_daemon_mode,(unsigned long)_last_command_check,(unsigned long)_last_log_rotation,_enable_notifications,_execute_service_checks,_accept_passive_service_checks,_enable_event_handlers,_obsess_over_services,_enable_flap_detection,_enable_failure_prediction,_process_performance_data);
#endif
#ifdef USE_XSDPGSQL
		snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (last_update,program_start,nagios_pid,daemon_mode,last_command_check,last_log_rotation,enable_notifications,execute_service_checks,accept_passive_service_checks,enable_event_handlers,obsess_over_services,enable_flap_detection,enable_failure_prediction,process_performance_data) VALUES (datetime(abstime(%lu)),datetime(abstime(%lu)),'%d','%d',datetime(abstime(%lu)),datetime(abstime(%lu)),'%d','%d','%d','%d','%d','%d','%d','%d')",XSDDB_PROGRAMSTATUS_TABLE,(unsigned long)current_time,(unsigned long)_program_start,_nagios_pid,_daemon_mode,(unsigned long)_last_command_check,(unsigned long)_last_log_rotation,_enable_notifications,_execute_service_checks,_accept_passive_service_checks,_enable_event_handlers,_obsess_over_services,_enable_flap_detection,_enable_failure_prediction,_process_performance_data);
#endif
	        }

	/* else normal operation so UPDATE the already existing record... */
	else{
#ifdef USE_XSDMYSQL
		snprintf(sql_statement,sizeof(sql_statement)-1,"UPDATE %s SET last_update=FROM_UNIXTIME(%lu),program_start=FROM_UNIXTIME(%lu),nagios_pid='%d',daemon_mode='%d',last_command_check=FROM_UNIXTIME(%lu),last_log_rotation=FROM_UNIXTIME(%lu),enable_notifications='%d',execute_service_checks='%d',accept_passive_service_checks='%d',enable_event_handlers='%d',obsess_over_services='%d',enable_flap_detection='%d',enable_failure_prediction='%d',process_performance_data='%d'",XSDDB_PROGRAMSTATUS_TABLE,(unsigned long)current_time,(unsigned long)_program_start,_nagios_pid,_daemon_mode,(unsigned long)_last_command_check,(unsigned long)_last_log_rotation,_enable_notifications,_execute_service_checks,_accept_passive_service_checks,_enable_event_handlers,_obsess_over_services,_enable_flap_detection,_enable_failure_prediction,_process_performance_data);
#endif
#ifdef USE_XSDPGSQL
		snprintf(sql_statement,sizeof(sql_statement)-1,"UPDATE %s SET last_update=datetime(abstime(%lu)),program_start=datetime(abstime(%lu)),nagios_pid='%d',daemon_mode='%d',last_command_check=datetime(abstime(%lu)),last_log_rotation=datetime(abstime(%lu)),enable_notifications='%d',execute_service_checks='%d',accept_passive_service_checks='%d',enable_event_handlers='%d',obsess_over_services='%d',enable_flap_detection='%d',enable_failure_prediction='%d',process_performance_data='%d'",XSDDB_PROGRAMSTATUS_TABLE,(unsigned long)current_time,(unsigned long)_program_start,_nagios_pid,_daemon_mode,(unsigned long)_last_command_check,(unsigned long)_last_log_rotation,_enable_notifications,_execute_service_checks,_accept_passive_service_checks,_enable_event_handlers,_obsess_over_services,_enable_flap_detection,_enable_failure_prediction,_process_performance_data);
#endif
	        }

	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* insert the new host status record or update the existing one... */
	result=xsddb_query(sql_statement);
	xsddb_free_query_memory();

	/* there was an error... */
	if(result==ERROR){

		/***** ROLLBACK TRANSACTION *****/
		if(aggregated_dump==TRUE)
			xsddb_rollback_transaction();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not insert/update status record for program in table '%s' of database '%s'\n",XSDDB_PROGRAMSTATUS_TABLE,xsddb_database);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

#ifdef DEBUG1
		snprintf(buffer,sizeof(buffer)-1,"The offending SQL statement follows: %s\n",sql_statement);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
#endif

		/* check the database connection */
		xsddb_check_connection();

		return ERROR;
	        }

#ifdef DEBUG0
	printf("xsddb_update_program_status() start\n");
#endif

	return OK;
        }



/* updates host status data */
int xsddb_update_host_status(char *host_name, char *status, time_t last_update, time_t last_check, time_t last_state_change, int problem_has_been_acknowledged, unsigned long time_up, unsigned long time_down, unsigned long time_unreachable, time_t last_notification, int current_notification_number, int notifications_enabled, int event_handler_enabled, int checks_enabled, int flap_detection_enabled, int is_flapping, double percent_state_change, int scheduled_downtime_depth, int failure_prediction_enabled, int process_performance_data, char *plugin_output, int aggregated_dump){
	char sql_statement[XSDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	char *escaped_plugin_output;
	int result;

#ifdef DEBUG0
	printf("xsddb_update_host_status() start\n");
#endif

	if(xsddb_init==ERROR)
		return ERROR;

	/* try and reconnect to the database server if necessary... */
	if(xsddb_connection_error==TRUE){
		if(xsddb_reconnect()==ERROR)
			return ERROR;
	        }

	/* construct the SQL statement */
	
	/* we're doing an aggregated dump, so we use INSERTs... */
	if(aggregated_dump==TRUE){

		/* the host has not been checked yet... */
		if(last_check==(time_t)0){
#ifdef USE_XSDMYSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,host_status,last_update,last_check,last_state_change,problem_acknowledged,time_up,time_down,time_unreachable,last_notification,current_notification,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,plugin_output) VALUES ('%s','PENDING',FROM_UNIXTIME(%lu),FROM_UNIXTIME(0),FROM_UNIXTIME(0),'0','0','0','0',FROM_UNIXTIME(0),'0','%d','%d','%d','%d','0','0.0','%d','%d','%d','(Not enough data to determine host status yet)')",XSDDB_HOSTSTATUS_TABLE,host_name,(unsigned long)last_update,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data);
#endif
#ifdef USE_XSDPGSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,host_status,last_update,last_check,last_state_change,problem_acknowledged,time_up,time_down,time_unreachable,last_notification,current_notification,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,plugin_output) VALUES ('%s','PENDING',datetime(abstime(%lu)),datetime(abstime(0)),datetime(abstime(0)),'0','0','0','0',datetime(abstime(0)),'0','%d','%d','%d','%d','0','0.0','%d','%d','%d','(Not enough data to determine host status yet)')",XSDDB_HOSTSTATUS_TABLE,host_name,(unsigned long)last_update,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data);
#endif
		        }

		/* the host has been checked... */
		else{

			/* escape the plugin output, as it may have quotes, etc... */
			escaped_plugin_output=(char *)malloc(strlen(plugin_output)*2+1);
			xsddb_escape_string(escaped_plugin_output,plugin_output);

#ifdef USE_XSDMYSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,host_status,last_update,last_check,last_state_change,problem_acknowledged,time_up,time_down,time_unreachable,last_notification,current_notification,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,plugin_output) VALUES ('%s','%s',FROM_UNIXTIME(%lu),FROM_UNIXTIME(%lu),FROM_UNIXTIME(%lu),'%d','%lu','%lu','%lu',FROM_UNIXTIME(%lu),'%d','%d','%d','%d','%d','%d','%f','%d','%d','%d','%s')",XSDDB_HOSTSTATUS_TABLE,host_name,status,(unsigned long)last_update,(unsigned long)last_check,(unsigned long)last_state_change,problem_has_been_acknowledged,time_up,time_down,time_unreachable,(unsigned long)last_notification,current_notification_number,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,escaped_plugin_output);
#endif
#ifdef USE_XSDPGSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,host_status,last_update,last_check,last_state_change,problem_acknowledged,time_up,time_down,time_unreachable,last_notification,current_notification,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,plugin_output) VALUES ('%s','%s',datetime(abstime(%lu)),datetime(abstime(%lu)),datetime(abstime(%lu)),'%d','%lu','%lu','%lu',datetime(abstime(%lu)),'%d','%d','%d','%d','%d','%d','%f','%d','%d','%d','%s')",XSDDB_HOSTSTATUS_TABLE,host_name,status,(unsigned long)last_update,(unsigned long)last_check,(unsigned long)last_state_change,problem_has_been_acknowledged,time_up,time_down,time_unreachable,(unsigned long)last_notification,current_notification_number,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,escaped_plugin_output);
#endif

			/* free memory for the escaped string */
			free(escaped_plugin_output);
		        }
	        }

	/* else normal operation so UPDATE the already existing record... */
	else{

		/* the host has not been checked yet... */
		if(last_check==(time_t)0){
#ifdef USE_XSDMYSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"UPDATE %s SET host_status='PENDING',last_update=FROM_UNIXTIME(%lu),notifications_enabled='%d',event_handler_enabled='%d',checks_enabled='%d',flap_detection_enabled='%d',is_flapping='%d',percent_state_change='%f',scheduled_downtime_depth='%d',failure_prediction_enabled='%d',process_performance_data='%d',plugin_output='(Not enough data to determine host status yet)' WHERE host_name='%s'",XSDDB_HOSTSTATUS_TABLE,(unsigned long)last_update,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,host_name);
#endif
#ifdef USE_XSDPGSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"UPDATE %s SET host_status='PENDING',last_update=datetime(abstime(%lu)),notifications_enabled='%d',event_handler_enabled='%d',checks_enabled='%d',flap_detection_enabled='%d',is_flapping='%d',percent_state_change='%f',scheduled_downtime_depth='%d',failure_prediction_enabled='%d',process_performance_data='%d',plugin_output='(Not enough data to determine host status yet)' WHERE host_name='%s'",XSDDB_HOSTSTATUS_TABLE,(unsigned long)last_update,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,host_name);
#endif
		        }

		/* the host has been checked... */
		else{

			/* escape the plugin output, as it may have quotes, etc... */
			escaped_plugin_output=(char *)malloc(strlen(plugin_output)*2+1);
			xsddb_escape_string(escaped_plugin_output,plugin_output);

#ifdef USE_XSDMYSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"UPDATE %s SET host_status='%s',last_update=FROM_UNIXTIME(%lu),last_check=FROM_UNIXTIME(%lu),last_state_change=FROM_UNIXTIME(%lu),problem_acknowledged='%d',time_up='%lu',time_down='%lu',time_unreachable='%lu',last_notification=FROM_UNIXTIME(%lu),current_notification='%d',notifications_enabled='%d',event_handler_enabled='%d',checks_enabled='%d',flap_detection_enabled='%d',is_flapping='%d',percent_state_change='%f',scheduled_downtime_depth='%d',failure_prediction_enabled='%d',process_performance_data='%d',plugin_output='%s' WHERE host_name='%s'",XSDDB_HOSTSTATUS_TABLE,status,(unsigned long)last_update,(unsigned long)last_check,(unsigned long)last_state_change,problem_has_been_acknowledged,time_up,time_down,time_unreachable,(unsigned long)last_notification,current_notification_number,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,(last_check==(time_t)0)?"(Not enough data to determine host status yet)":escaped_plugin_output,host_name);
#endif
#ifdef USE_XSDPGSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"UPDATE %s SET host_status='%s',last_update=datetime(abstime(%lu)),last_check=datetime(abstime(%lu)),last_state_change=datetime(abstime(%lu)),problem_acknowledged='%d',time_up='%lu',time_down='%lu',time_unreachable='%lu',last_notification=datetime(abstime(%lu)),current_notification='%d',notifications_enabled='%d',event_handler_enabled='%d',checks_enabled='%d',flap_detection_enabled='%d',is_flapping='%d',percent_state_change='%f',scheduled_downtime_depth='%d',failure_prediction_enabled='%d',process_performance_data='%d',plugin_output='%s' WHERE host_name='%s'",XSDDB_HOSTSTATUS_TABLE,status,(unsigned long)last_update,(unsigned long)last_check,(unsigned long)last_state_change,problem_has_been_acknowledged,time_up,time_down,time_unreachable,(unsigned long)last_notification,current_notification_number,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,(last_check==(time_t)0)?"(Not enough data to determine host status yet)":escaped_plugin_output,host_name);
#endif

			/* free memory for the escaped string */
			free(escaped_plugin_output);
		        }
	        }

	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* insert the new host status record or update the existing one... */
	result=xsddb_query(sql_statement);
	xsddb_free_query_memory();

	/* there was an error... */
	if(result==ERROR){

		/***** ROLLBACK TRANSACTION *****/
		if(aggregated_dump==TRUE)
			xsddb_rollback_transaction();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not insert/update status record for host '%s' in table '%s' of database '%s'\n",host_name,XSDDB_HOSTSTATUS_TABLE,xsddb_database);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

#ifdef DEBUG1
		snprintf(buffer,sizeof(buffer)-1,"The offending SQL statement follows: %s\n",sql_statement);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
#endif

		/* check the database connection */
		xsddb_check_connection();

		return ERROR;
	        }

#ifdef DEBUG0
	printf("xsddb_update_host_status() end\n");
#endif

	return OK;
        }



/* updates service status data */
int xsddb_update_service_status(char *host_name, char *description, char *status, time_t last_update, int current_attempt, int max_attempts, int state_type, time_t last_check, time_t next_check, int should_be_scheduled, int check_type, int checks_enabled, int accept_passive_service_checks, int event_handler_enabled, time_t last_state_change, int problem_has_been_acknowledged, char *last_hard_state, unsigned long time_ok, unsigned long time_warning, unsigned long time_unknown, unsigned long time_critical, time_t last_notification, int current_notification_number, int notifications_enabled, int latency, int execution_time, int flap_detection_enabled, int is_flapping, double percent_state_change, int scheduled_downtime_depth, int failure_prediction_enabled, int process_performance_data, int obsess_over_service, char *plugin_output, int aggregated_dump){
	char sql_statement[XSDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	char *escaped_plugin_output;
	time_t current_time;
	int result;

#ifdef DEBUG0
	printf("xsddb_update_service_status() start\n");
#endif

	if(xsddb_init==ERROR)
		return ERROR;

	/* try and reconnect to the database server if necessary... */
	if(xsddb_connection_error==TRUE){
		if(xsddb_reconnect()==ERROR)
			return ERROR;
	        }

	/* get current time */
	time(&current_time);

	if(should_be_scheduled==FALSE)
		next_check=(time_t)0L;

	/* construct the SQL statement */
	
	/* we're doing an aggregated dump, so we use INSERTs... */
	if(aggregated_dump==TRUE){

		if(should_be_scheduled==TRUE){
			snprintf(buffer,sizeof(buffer)-1,"Service check scheduled for %s",ctime(&next_check));
			buffer[sizeof(buffer)-1]='\x0';
			buffer[strlen(buffer)-1]='\x0';
			}
		else{
			snprintf(buffer,sizeof(buffer)-1,"Service check is not scheduled for execution...");
			buffer[sizeof(buffer)-1]='\x0';
		        }

		/* the service has not been checked yet... */
		if(last_check==(time_t)0){
#ifdef USE_XSDMYSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,service_description,service_status,last_update,current_attempt,max_attempts,state_type,last_check,next_check,should_be_scheduled,check_type,checks_enabled,accept_passive_checks,event_handler_enabled,last_state_change,problem_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,plugin_output) VALUES ('%s','%s','PENDING',FROM_UNIXTIME(%lu),'0','%d','%s',FROM_UNIXTIME(0),FROM_UNIXTIME(%lu),'%d','%s','%d','%d','%d',FROM_UNIXTIME(%lu),'%d','%s','%lu','%lu','%lu','%lu',FROM_UNIXTIME(%lu),'%d','%d','%d','%d','%d','0','0.0','%d','%d','%d','%d','%s')",XSDDB_SERVICESTATUS_TABLE,host_name,description,(unsigned long)last_update,max_attempts,(state_type==HARD_STATE)?"HARD":"SOFT",(unsigned long)next_check,should_be_scheduled,(check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE",checks_enabled,accept_passive_service_checks,event_handler_enabled,(unsigned long)last_state_change,problem_has_been_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,(unsigned long)last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,buffer); 
#endif
#ifdef USE_XSDPGSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,service_description,service_status,last_update,current_attempt,max_attempts,state_type,last_check,next_check,should_be_scheduled,check_type,checks_enabled,accept_passive_checks,event_handler_enabled,last_state_change,problem_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,plugin_output) VALUES ('%s','%s','PENDING',datetime(abstime(%lu)),'0','%d','%s',datetime(abstime(0)),datetime(abstime(%lu)),'%d','%s','%d','%d','%d',datetime(abstime(%lu)),'%d','%s','%lu','%lu','%lu','%lu',datetime(abstime(%lu)),'%d','%d','%d','%d','%d','0','0.0','%d','%d','%d','%d','%s')",XSDDB_SERVICESTATUS_TABLE,host_name,description,(unsigned long)last_update,max_attempts,(state_type==HARD_STATE)?"HARD":"SOFT",(unsigned long)next_check,should_be_scheduled,(check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE",checks_enabled,accept_passive_service_checks,event_handler_enabled,(unsigned long)last_state_change,problem_has_been_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,(unsigned long)last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,buffer); 
#endif
		        }


		/* the service has been checked... */
		else{

			/* escape the plugin output, as it may have quotes, etc... */
			escaped_plugin_output=(char *)malloc(strlen(plugin_output)*2+1);
			xsddb_escape_string(escaped_plugin_output,plugin_output);

#ifdef USE_XSDMYSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,service_description,service_status,last_update,current_attempt,max_attempts,state_type,last_check,next_check,should_be_scheduled,check_type,checks_enabled,accept_passive_checks,event_handler_enabled,last_state_change,problem_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,plugin_output) VALUES ('%s','%s','%s',FROM_UNIXTIME(%lu),'%d','%d','%s',FROM_UNIXTIME(%lu),FROM_UNIXTIME(%lu),'%d','%s','%d','%d','%d',FROM_UNIXTIME(%lu),'%d','%s','%lu','%lu','%lu','%lu',FROM_UNIXTIME(%lu),'%d','%d','%d','%d','%d','%d','%f','%d','%d','%d','%d','%s')",XSDDB_SERVICESTATUS_TABLE,host_name,description,status,(unsigned long)last_update,current_attempt,max_attempts,(state_type==HARD_STATE)?"HARD":"SOFT",(unsigned long)last_check,(unsigned long)next_check,should_be_scheduled,(check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE",checks_enabled,accept_passive_service_checks,event_handler_enabled,(unsigned long)last_state_change,problem_has_been_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,(unsigned long)last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,escaped_plugin_output);
#endif
#ifdef USE_XSDPGSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,service_description,service_status,last_update,current_attempt,max_attempts,state_type,last_check,next_check,should_be_scheduled,check_type,checks_enabled,accept_passive_checks,event_handler_enabled,last_state_change,problem_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,plugin_output) VALUES ('%s','%s','%s',datetime(abstime(%lu)),'%d','%d','%s',datetime(abstime(%lu)),datetime(abstime(%lu)),'%d','%s','%d','%d','%d',datetime(abstime(%lu)),'%d','%s','%lu','%lu','%lu','%lu',datetime(abstime(%lu)),'%d','%d','%d','%d','%d','%d','%f','%d','%d','%d','%d','%s')",XSDDB_SERVICESTATUS_TABLE,host_name,description,status,(unsigned long)last_update,current_attempt,max_attempts,(state_type==HARD_STATE)?"HARD":"SOFT",(unsigned long)last_check,(unsigned long)next_check,should_be_scheduled,(check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE",checks_enabled,accept_passive_service_checks,event_handler_enabled,(unsigned long)last_state_change,problem_has_been_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,(unsigned long)last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,escaped_plugin_output);
#endif

			/* free memory for the escaped string */
			free(escaped_plugin_output);
		        }

		}


	/* else normal operation, so UPDATE the already existing record... */
	else{

		/* the service has not been checked yet... */
		if(last_check==(time_t)0){

			if(should_be_scheduled==TRUE){
				snprintf(buffer,sizeof(buffer)-1,"Service check scheduled for %s",ctime(&next_check));
				buffer[strlen(buffer)-1]='\x0';
			        }
			else{
				snprintf(buffer,sizeof(buffer)-1,"Service check is not scheduled for execution...");
				buffer[sizeof(buffer)-1]='\x0';
		                }

#ifdef USE_XSDMYSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"UPDATE %s SET service_status='PENDING',last_update=FROM_UNIXTIME(%lu),current_attempt='%d',max_attempts='%d',state_type='%s',last_check=FROM_UNIXTIME(%lu),next_check=FROM_UNIXTIME(%lu),should_be_scheduled='%d',check_type='%s',checks_enabled='%d',accept_passive_checks='%d',event_handler_enabled='%d',last_state_change=FROM_UNIXTIME(%lu),problem_acknowledged='%d',last_hard_state='%s',time_ok='%lu',time_warning='%lu',time_unknown='%lu',time_critical='%lu',last_notification=FROM_UNIXTIME(%lu),current_notification='%d',notifications_enabled='%d',latency='%d',execution_time='%d',flap_detection_enabled='%d',is_flapping='%d',percent_state_change='%f',scheduled_downtime_depth='%d',failure_prediction_enabled='%d',process_performance_data='%d',obsess_over_service='%d',plugin_output='%s' WHERE host_name='%s' AND service_description='%s'",XSDDB_SERVICESTATUS_TABLE,(unsigned long)last_update,current_attempt,max_attempts,(state_type==HARD_STATE)?"HARD":"SOFT",(unsigned long)last_check,(unsigned long)next_check,should_be_scheduled,(check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE",checks_enabled,accept_passive_service_checks,event_handler_enabled,(unsigned long)last_state_change,problem_has_been_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,(unsigned long)last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,buffer,host_name,description);
#endif
#ifdef USE_XSDPGSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"UPDATE %s SET service_status='PENDING',last_update=datetime(abstime(%lu)),current_attempt='%d',max_attempts='%d',state_type='%s',last_check=datetime(abstime(%lu)),next_check=datetime(abstime(%lu)),should_be_scheduled='%d',check_type='%s',checks_enabled='%d',accept_passive_checks='%d',event_handler_enabled='%d',last_state_change=datetime(abstime(%lu)),problem_acknowledged='%d',last_hard_state='%s',time_ok='%lu',time_warning='%lu',time_unknown='%lu',time_critical='%lu',last_notification=datetime(abstime(%lu)),current_notification='%d',notifications_enabled='%d',latency='%d',execution_time='%d',flap_detection_enabled='%d',is_flapping='%d',percent_state_change='%f',scheduled_downtime_depth='%d',failure_prediction_enabled='%d',process_performance_data='%d',obsess_over_service='%d',plugin_output='%s' WHERE host_name='%s' AND service_description='%s'",XSDDB_SERVICESTATUS_TABLE,(unsigned long)last_update,current_attempt,max_attempts,(state_type==HARD_STATE)?"HARD":"SOFT",(unsigned long)last_check,(unsigned long)next_check,should_be_scheduled,(check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE",checks_enabled,accept_passive_service_checks,event_handler_enabled,(unsigned long)last_state_change,problem_has_been_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,(unsigned long)last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,buffer,host_name,description);
#endif
		        }

		/* the service has been checked... */
		else{

			/* escape the plugin output, as it may have quotes, etc... */
			escaped_plugin_output=(char *)malloc(strlen(plugin_output)*2+1);
			xsddb_escape_string(escaped_plugin_output,plugin_output);

#ifdef USE_XSDMYSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"UPDATE %s SET service_status='%s',last_update=FROM_UNIXTIME(%lu),current_attempt='%d',max_attempts='%d',state_type='%s',last_check=FROM_UNIXTIME(%lu),next_check=FROM_UNIXTIME(%lu),should_be_scheduled='%d',check_type='%s',checks_enabled='%d',accept_passive_checks='%d',event_handler_enabled='%d',last_state_change=FROM_UNIXTIME(%lu),problem_acknowledged='%d',last_hard_state='%s',time_ok='%lu',time_warning='%lu',time_unknown='%lu',time_critical='%lu',last_notification=FROM_UNIXTIME(%lu),current_notification='%d',notifications_enabled='%d',latency='%d',execution_time='%d',flap_detection_enabled='%d',is_flapping='%d',percent_state_change='%f',scheduled_downtime_depth='%d',failure_prediction_enabled='%d',process_performance_data='%d',obsess_over_service='%d',plugin_output='%s' WHERE host_name='%s' AND service_description='%s'",XSDDB_SERVICESTATUS_TABLE,status,(unsigned long)last_update,current_attempt,max_attempts,(state_type==HARD_STATE)?"HARD":"SOFT",(unsigned long)last_check,(unsigned long)next_check,should_be_scheduled,(check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE",checks_enabled,accept_passive_service_checks,event_handler_enabled,(unsigned long)last_state_change,problem_has_been_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,(unsigned long)last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,escaped_plugin_output,host_name,description);
#endif
#ifdef USE_XSDPGSQL
			snprintf(sql_statement,sizeof(sql_statement)-1,"UPDATE %s SET service_status='%s',last_update=datetime(abstime(%lu)),current_attempt='%d',max_attempts='%d',state_type='%s',last_check=datetime(abstime(%lu)),next_check=datetime(abstime(%lu)),should_be_scheduled='%d',check_type='%s',checks_enabled='%d',accept_passive_checks='%d',event_handler_enabled='%d',last_state_change=datetime(abstime(%lu)),problem_acknowledged='%d',last_hard_state='%s',time_ok='%lu',time_warning='%lu',time_unknown='%lu',time_critical='%lu',last_notification=datetime(abstime(%lu)),current_notification='%d',notifications_enabled='%d',latency='%d',execution_time='%d',flap_detection_enabled='%d',is_flapping='%d',percent_state_change='%f',scheduled_downtime_depth='%d',failure_prediction_enabled='%d',process_performance_data='%d',obsess_over_service='%d',plugin_output='%s' WHERE host_name='%s' AND service_description='%s'",XSDDB_SERVICESTATUS_TABLE,status,(unsigned long)last_update,current_attempt,max_attempts,(state_type==HARD_STATE)?"HARD":"SOFT",(unsigned long)last_check,(unsigned long)next_check,should_be_scheduled,(check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE",checks_enabled,accept_passive_service_checks,event_handler_enabled,(unsigned long)last_state_change,problem_has_been_acknowledged,last_hard_state,time_ok,time_warning,time_unknown,time_critical,(unsigned long)last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,escaped_plugin_output,host_name,description);
#endif

			/* free memory for the escaped plugin string */
			free(escaped_plugin_output);
		        }
	        }

	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* insert the new host status record or update the existing one... */
	result=xsddb_query(sql_statement);
	xsddb_free_query_memory();

	/* there was an error... */
	if(result==ERROR){

		/***** ROLLBACK TRANSACTION *****/
		if(aggregated_dump==TRUE)
			xsddb_rollback_transaction();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not insert/update status record for service '%s' on host '%s' in table '%s' of database '%s'\n",description,host_name,XSDDB_SERVICESTATUS_TABLE,xsddb_database);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

#ifdef DEBUG1
		snprintf(buffer,sizeof(buffer)-1,"The offending SQL statement follows: %s\n",sql_statement);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
#endif

		/* check the database connection */
		xsddb_check_connection();

		return ERROR;
	        }

#ifdef DEBUG0
	printf("xsddb_update_service_status() end\n");
#endif

	return OK;
        }


#endif



#ifdef NSCGI

/******************************************************************/
/****************** DEFAULT DATA INPUT FUNCTIONS ******************/
/******************************************************************/

/* read all program, host, and service status information */
int xsddb_read_status_data(char *main_config_file,int options){
	int result;

	/* grab configuration information */
	if(xsddb_grab_config_info(main_config_file)==ERROR)
		return ERROR;

	/* initialize database structures */
	if(xsddb_initialize()==ERROR)
		return ERROR;

	/* establish a connection to the database server */
	if(xsddb_connect()==ERROR)
		return ERROR;

	/* read in program status information */
	if(options & READ_PROGRAM_STATUS){
		result=xsddb_add_program_status();
		if(result==ERROR)
			return ERROR;
	        }

	/* read in all host status information */
	if(options & READ_HOST_STATUS){
		result=xsddb_add_host_status();
		if(result==ERROR)
			return ERROR;
	        }

	/* read in all service status information */
	if(options & READ_SERVICE_STATUS){
		result=xsddb_add_service_status();
		if(result==ERROR)
			return ERROR;
	        }

	/* close the connection to the database server */		
	xsddb_disconnect();

	return OK;
        }



/* adds program status information */
int xsddb_add_program_status(void){
	char sql_statement[XSDDB_SQL_LENGTH];
	int result;
	int x;
	time_t _program_start;
	int _nagios_pid;
	int _daemon_mode;
	time_t _last_command_check;
	time_t _last_log_rotation;
	int _enable_notifications;
	int _execute_service_checks;
	int _accept_passive_service_checks;
	int _enable_event_handlers;
	int _obsess_over_services;
	int _enable_flap_detection;
	int _enable_failure_prediction;
	int _process_performance_data;
#ifdef USE_XSDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XSDPGSQL
	int ntuples;
	int tuple;
#endif


	/* construct the SQL statement */
#ifdef USE_XSDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT UNIX_TIMESTAMP(last_update) AS last_update, UNIX_TIMESTAMP(program_start) AS program_start, nagios_pid, daemon_mode, UNIX_TIMESTAMP(last_command_check) AS last_command_check, UNIX_TIMESTAMP(last_log_rotation) AS last_log_rotation, enable_notifications, execute_service_checks, accept_passive_service_checks, enable_event_handlers, obsess_over_services, enable_flap_detection, enable_failure_prediction, process_performance_data FROM %s",XSDDB_PROGRAMSTATUS_TABLE);
#endif
#ifdef USE_XSDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT date_part('epoch',last_update) AS last_update, date_part('epoch',program_start) AS program_start, nagios_pid, daemon_mode, date_part('epoch',last_command_check) AS last_command_check, date_part('epoch',last_log_rotation) AS last_log_rotation, enable_notifications, execute_service_checks, accept_passive_service_checks, enable_event_handlers, obsess_over_services, enable_flap_detection, enable_failure_prediction, process_performance_data FROM %s",XSDDB_PROGRAMSTATUS_TABLE);
#endif

	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xsddb_query(sql_statement)==ERROR){
		xsddb_free_query_memory();
		return ERROR;
	        }


#ifdef USE_XSDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xsddb_mysql))==NULL)
		return ERROR;

	/* get the first row returned (there should be only one anyways...) */
	if((result_row=mysql_fetch_row(query_result))==NULL)
		return ERROR;

	/* make sure the row we have aren't NULL */
	for(x=0;x<14;x++){
		if(result_row[x]==NULL)
			return ERROR;
	        }

	/* last update time */

	/* program start time */
	_program_start=(time_t)strtoul(result_row[1],NULL,10);

	/* PID */
	_nagios_pid=atoi(result_row[2]);

	/* daemon mode flag */
	_daemon_mode=atoi(result_row[3]);

	/* last command check */
	_last_command_check=(time_t)strtoul(result_row[4],NULL,10);

	/* last log rotation */
	_last_log_rotation=(time_t)strtoul(result_row[5],NULL,10);

	/* enable notifications option */
	_enable_notifications=atoi(result_row[6]);

	/* execute service checks option */
	_execute_service_checks=atoi(result_row[7]);

	/* accept passive service checks option */
	_accept_passive_service_checks=atoi(result_row[8]);

	/* event handlers enabled option */
	_enable_event_handlers=atoi(result_row[9]);

	/* obsess over services option */
	_obsess_over_services=atoi(result_row[10]);

	/* enable flap detection option */
	_enable_flap_detection=atoi(result_row[11]);

	/* enable failure prediction option */
	_enable_failure_prediction=atoi(result_row[12]);

	/* process performance data option */
	_process_performance_data=atoi(result_row[13]);
#endif

#ifdef USE_XSDPGSQL

	/* make sure we have at least one row */
	if(PQntuples(xsddb_pgres)<1)
		return ERROR;

	/* program start time */
	_program_start=(time_t)strtoul(PQgetvalue(xsddb_pgres,0,1),NULL,10);

	/* PID */
	_nagios_pid=atoi(PQgetvalue(xsddb_pgres,0,2));

	/* daemon mode flag */
	_daemon_mode=atoi(PQgetvalue(xsddb_pgres,0,3));

	/* last command check */
	_last_command_check=(time_t)strtoul(PQgetvalue(xsddb_pgres,0,4),NULL,10);

	/* last log rotation */
	_last_log_rotation=(time_t)strtoul(PQgetvalue(xsddb_pgres,0,5),NULL,10);

	/* are notifications enabled? */
	_enable_notifications=atoi(PQgetvalue(xsddb_pgres,0,6));

	/* are service checks being executed? */
	_execute_service_checks=atoi(PQgetvalue(xsddb_pgres,0,7));

	/* are passive check results being accepted? */
	_accept_passive_service_checks=atoi(PQgetvalue(xsddb_pgres,0,8));

	/* are event handlers enabled? */
	_enable_event_handlers=atoi(PQgetvalue(xsddb_pgres,0,9));

	/* are we obsessing over services? */
	_obsess_over_services=atoi(PQgetvalue(xsddb_pgres,0,10));

	/* is flap detection enabled? */
	_enable_flap_detection=atoi(PQgetvalue(xsddb_pgres,0,11));

	/* is failure prediction enabled? */
	_enable_failure_prediction=atoi(PQgetvalue(xsddb_pgres,0,12));

	/* are we processing performance data? */
	_process_performance_data=atoi(PQgetvalue(xsddb_pgres,0,13));

	/* free memory used to store query results */
	PQclear(xsddb_pgres);
#endif


	/* add program status information */
	result=add_program_status(_program_start,_nagios_pid,_daemon_mode,_last_command_check,_last_log_rotation,_enable_notifications,_execute_service_checks,_accept_passive_service_checks,_enable_event_handlers,_obsess_over_services,_enable_flap_detection,_enable_failure_prediction,_process_performance_data);
	if(result!=OK)
		return ERROR;

	return OK;
        }



/* adds host status information */
int xsddb_add_host_status(void){
	char sql_statement[XSDDB_SQL_LENGTH];
	int result;
	int x;
	time_t last_update;
	time_t last_check;
	time_t last_state_change;
	int problem_has_been_acknowledged;
	unsigned long time_up;
	unsigned long time_down;
	unsigned long time_unreachable;
	time_t last_notification;
	int current_notification_number;
	int notifications_enabled;
	int event_handler_enabled;
	int checks_enabled;
	int flap_detection_enabled;
	int is_flapping;
	double percent_state_change;
	int scheduled_downtime_depth;
	int failure_prediction_enabled;
	int process_performance_data;
#ifdef USE_XSDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XSDPGSQL
	int ntuples;
	int tuple;
#endif


	/* construct the SQL statement */
#ifdef USE_XSDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT host_name, host_status, UNIX_TIMESTAMP(last_update) AS last_update, UNIX_TIMESTAMP(last_check) AS last_check, UNIX_TIMESTAMP(last_state_change) AS last_state_change, problem_acknowledged, time_up, time_down, time_unreachable, UNIX_TIMESTAMP(last_notification) AS last_notification, current_notification, notifications_enabled, event_handler_enabled, checks_enabled, flap_detection_enabled, is_flapping, percent_state_change, scheduled_downtime_depth, failure_prediction_enabled, process_performance_data, plugin_output FROM %s",XSDDB_HOSTSTATUS_TABLE);
#endif
#ifdef USE_XSDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT host_name, host_status, date_part('epoch',last_update) AS last_update, date_part('epoch',last_check) AS last_check, date_part('epoch',last_state_change) AS last_state_change, problem_acknowledged, time_up, time_down, time_unreachable, date_part('epoch',last_notification) AS last_notification, current_notification, notifications_enabled, event_handler_enabled, checks_enabled, flap_detection_enabled, is_flapping, percent_state_change, scheduled_downtime_depth, failure_prediction_enabled, process_performance_data, plugin_output FROM %s",XSDDB_HOSTSTATUS_TABLE);
#endif

	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xsddb_query(sql_statement)==ERROR){
		xsddb_free_query_memory();
		return ERROR;
	        }


#ifdef USE_XSDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xsddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* we have 19 rows - make sure they're not NULL */
		for(x=0;x<19;x++){
			if(result_row[x]==NULL){
				mysql_free_result(query_result);
				return ERROR;
			        }
	                }

		/* last update */
		last_update=(time_t)strtoul(result_row[2],NULL,10);

		/* last check */
		last_check=(time_t)strtoul(result_row[3],NULL,10);

		/* last state change */
		last_state_change=(time_t)strtoul(result_row[4],NULL,10);

		/* problem acknowledgement */
		problem_has_been_acknowledged=atoi(result_row[5]);

		/* time up, down, unreachable */
		time_up=strtoul(result_row[6],NULL,10);
		time_down=strtoul(result_row[7],NULL,10);
		time_unreachable=strtoul(result_row[8],NULL,10);

		/* last notification */
		last_notification=(time_t)strtoul(result_row[9],NULL,10);

		/* current notification number */
		current_notification_number=atoi(result_row[10]);

		/* notifications enabled */
		notifications_enabled=atoi(result_row[11]);

		/* event handler enabled */
		event_handler_enabled=atoi(result_row[12]);

		/* checks enabled */
		checks_enabled=atoi(result_row[13]);

		/* flap detection enabled */
		flap_detection_enabled=atoi(result_row[14]);

		/* flapping indicator */
		is_flapping=atoi(result_row[15]);

		/* percent state change */
		percent_state_change=strtod(result_row[16],NULL);

		/* scheduled downtime depth */
		scheduled_downtime_depth=atoi(result_row[17]);

		/* failure prediction enabled */
		failure_prediction_enabled=atoi(result_row[18]);

		/* process performance data */
		process_performance_data=atoi(result_row[19]);

		/* add this host status */
		result=add_host_status(result_row[0],result_row[1],last_update,last_check,last_state_change,problem_has_been_acknowledged,time_up,time_down,time_unreachable,last_notification,current_notification_number,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,result_row[20]);
		if(result!=OK){
			mysql_free_result(query_result);
			return ERROR;
		        }
	        }

	/* free buffer used to store query result */
	mysql_free_result(query_result);
#endif


#ifdef USE_XSDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xsddb_pgres);

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* last update */
		last_update=(time_t)strtoul(PQgetvalue(xsddb_pgres,tuple,2),NULL,10);

		/* last check */
		last_check=(time_t)strtoul(PQgetvalue(xsddb_pgres,tuple,3),NULL,10);

		/* last state change */
		last_state_change=(time_t)strtoul(PQgetvalue(xsddb_pgres,tuple,4),NULL,10);

		/* problem acknowledgement */
		problem_has_been_acknowledged=atoi(PQgetvalue(xsddb_pgres,tuple,5));

		/* time up, down, unreachable */
		time_up=strtoul(PQgetvalue(xsddb_pgres,tuple,6),NULL,10);
		time_down=strtoul(PQgetvalue(xsddb_pgres,tuple,7),NULL,10);
		time_unreachable=strtoul(PQgetvalue(xsddb_pgres,tuple,8),NULL,10);

		/* last notification */
		last_notification=(time_t)strtoul(PQgetvalue(xsddb_pgres,tuple,9),NULL,10);

		/* current notification number */
		current_notification_number=atoi(PQgetvalue(xsddb_pgres,tuple,10));

		/* notifications enabled */
		notifications_enabled=atoi(PQgetvalue(xsddb_pgres,tuple,11));

		/* event handler enabled */
		event_handler_enabled=atoi(PQgetvalue(xsddb_pgres,tuple,12));

		/* checks enabled */
		checks_enabled=atoi(PQgetvalue(xsddb_pgres,tuple,13));

		/* flap detection enabled */
		flap_detection_enabled=atoi(PQgetvalue(xsddb_pgres,tuple,14));

		/* flapping indicator */
		is_flapping=atoi(PQgetvalue(xsddb_pgres,tuple,15));

		/* percent state change */
		percent_state_change=strtod(PQgetvalue(xsddb_pgres,tuple,16),NULL);

		/* scheduled downtime depth */
		scheduled_downtime_depth=atoi(PQgetvalue(xsddb_pgres,tuple,17));

		/* failure prediction enabled */
		failure_prediction_enabled=atoi(PQgetvalue(xsddb_pgres,tuple,18));

		/* process performance data */
		process_performance_data=atoi(PQgetvalue(xsddb_pgres,tuple,19));

		/* add the host status */
		result=add_host_status(PQgetvalue(xsddb_pgres,tuple,0),PQgetvalue(xsddb_pgres,tuple,1),last_update,last_check,last_state_change,problem_has_been_acknowledged,time_up,time_down,time_unreachable,last_notification,current_notification_number,notifications_enabled,event_handler_enabled,checks_enabled,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,PQgetvalue(xsddb_pgres,tuple,20));
		if(result!=OK){
			PQclear(xsddb_pgres);
			return ERROR;
		        }
	        }

	/* free memory used to store query results */
	PQclear(xsddb_pgres);
#endif

	return OK;
        }



/* adds service status information */
int xsddb_add_service_status(void){
	char sql_statement[XSDDB_SQL_LENGTH];
	int result;
	int x;
	time_t last_update;
	int current_attempt;
	int max_attempts;
	int state_type;
	time_t last_check;
	time_t next_check;
	int check_type;
	int checks_enabled;
	int accept_passive_service_checks;
	int event_handler_enabled;
	time_t last_state_change;
	int problem_has_been_acknowledged;
	unsigned long time_ok;
	unsigned long time_warning;
	unsigned long time_unknown;
	unsigned long time_critical;
	time_t last_notification;
	int current_notification_number;
	int notifications_enabled;
	int latency;
	int execution_time;
	int flap_detection_enabled;
	int is_flapping;
	double percent_state_change;
	int scheduled_downtime_depth;
	int failure_prediction_enabled;
	int process_performance_data;
	int obsess_over_service;
#ifdef USE_XSDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XSDPGSQL
	int ntuples;
	int tuple;
#endif


	/* construct the SQL statement */
#ifdef USE_XSDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT host_name, service_description, service_status, UNIX_TIMESTAMP(last_update) AS last_update, current_attempt, max_attempts, state_type, UNIX_TIMESTAMP(last_check) AS last_check, UNIX_TIMESTAMP(next_check) AS next_check, should_be_scheduled, check_type, checks_enabled, accept_passive_checks, event_handler_enabled, UNIX_TIMESTAMP(last_state_change) AS last_state_change, problem_acknowledged, last_hard_state, time_ok, time_warning, time_unknown, time_critical, UNIX_TIMESTAMP(last_notification) AS last_notification, current_notification, notifications_enabled, latency, execution_time, flap_detection_enabled, is_flapping, percent_state_change, scheduled_downtime_depth, failure_prediction_enabled, process_performance_data, obsess_over_service, plugin_output FROM %s",XSDDB_SERVICESTATUS_TABLE);
#endif
#ifdef USE_XSDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT host_name, service_description, service_status, date_part('epoch',last_update) AS last_update, current_attempt, max_attempts, state_type, date_part('epoch',last_check) AS last_check, date_part('epoch',next_check) AS next_check, should_be_scheduled, check_type, checks_enabled, accept_passive_checks, event_handler_enabled, date_part('epoch',last_state_change) AS last_state_change, problem_acknowledged, last_hard_state, time_ok, time_warning, time_unknown, time_critical, date_part('epoch',last_notification) AS last_notification, current_notification, notifications_enabled, latency, execution_time, flap_detection_enabled, is_flapping, percent_state_change, scheduled_downtime_depth, failure_prediction_enabled, process_performance_data, obsess_over_service, plugin_output FROM %s",XSDDB_SERVICESTATUS_TABLE);
#endif

	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xsddb_query(sql_statement)==ERROR){
		xsddb_free_query_memory();
		return ERROR;
	        }


#ifdef USE_XSDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xsddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* we have 32 rows - make sure they're not NULL */
		for(x=0;x<32;x++){
			if(result_row[x]==NULL){
				mysql_free_result(query_result);
				return ERROR;
			        }
	                }

		/* last update */
		last_update=(time_t)strtoul(result_row[3],NULL,10);

		/* current and max attempts */
		current_attempt=atoi(result_row[4]);
		max_attempts=atoi(result_row[5]);

		/* state type */
		if(!strcmp(result_row[6],"HARD"))
			state_type=HARD_STATE;
		else
			state_type=SOFT_STATE;

		/* last and next check times */
		last_check=(time_t)strtoul(result_row[7],NULL,10);
		next_check=(time_t)strtoul(result_row[8],NULL,10);

		/* should be scheduled */

		/* check type */
		if(!strcmp(result_row[10],"ACTIVE"))
			check_type=SERVICE_CHECK_ACTIVE;
		else
			check_type=SERVICE_CHECK_PASSIVE;

		/* checks enabled */
		checks_enabled=atoi(result_row[11]);

		/* accept passive service checks */
		accept_passive_service_checks=atoi(result_row[12]);

		/* event handler_enabled */
		event_handler_enabled=atoi(result_row[13]);

		/* last state change */
		last_state_change=(time_t)strtoul(result_row[14],NULL,10);

		/* problem has been acknowledged */
		problem_has_been_acknowledged=atoi(result_row[15]);

		/* times ok, warning, unknown, and critical */
		time_ok=strtoul(result_row[17],NULL,10);
		time_warning=strtoul(result_row[18],NULL,10);
		time_unknown=strtoul(result_row[19],NULL,10);
		time_critical=strtoul(result_row[20],NULL,10);

		/* last notification */
		last_notification=(time_t)strtoul(result_row[21],NULL,10);

		/* current notification number */
		current_notification_number=atoi(result_row[22]);

		/* notifications enabled */
		notifications_enabled=atoi(result_row[23]);

		/* latency */
		latency=atoi(result_row[24]);

		/* execution time */
		execution_time=atoi(result_row[25]);

		/* flap detection enabled */
		flap_detection_enabled=atoi(result_row[26]);

		/* flapping indicator */
		is_flapping=atoi(result_row[27]);

		/* percent state change */
		percent_state_change=strtod(result_row[28],NULL);

		/* scheduled downtime depth */
		scheduled_downtime_depth=atoi(result_row[29]);

		/* failure prediction enabled */
		failure_prediction_enabled=atoi(result_row[30]);

		/* process performance data */
		process_performance_data=atoi(result_row[31]);

		/* obsess over service */
		obsess_over_service=atoi(result_row[32]);

		/* add this service status */
		result=add_service_status(result_row[0],result_row[1],result_row[2],last_update,current_attempt,max_attempts,state_type,last_check,next_check,check_type,checks_enabled,accept_passive_service_checks,event_handler_enabled,last_state_change,problem_has_been_acknowledged,result_row[16],time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,result_row[33]);
		if(result!=OK){
			mysql_free_result(query_result);
			return ERROR;
		        }
	        }

	/* free memory used to store query results */
	mysql_free_result(query_result);
#endif


#ifdef USE_XSDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xsddb_pgres);

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* last update */
		last_update=(time_t)strtoul(PQgetvalue(xsddb_pgres,tuple,3),NULL,10);

		/* current and max attempts */
		current_attempt=atoi(PQgetvalue(xsddb_pgres,tuple,4));
		max_attempts=atoi(PQgetvalue(xsddb_pgres,tuple,5));

		/* state type */
		if(!strcmp(PQgetvalue(xsddb_pgres,tuple,6),"HARD"))
			state_type=HARD_STATE;
		else
			state_type=SOFT_STATE;

		/* last and next check times */
		last_check=(time_t)strtoul(PQgetvalue(xsddb_pgres,tuple,7),NULL,10);
		next_check=(time_t)strtoul(PQgetvalue(xsddb_pgres,tuple,8),NULL,10);

		/* should be scheduled */

		/* check type */
		if(!strcmp(PQgetvalue(xsddb_pgres,tuple,10),"ACTIVE"))
			check_type=SERVICE_CHECK_ACTIVE;
		else
			check_type=SERVICE_CHECK_PASSIVE;

		/* checks enabled */
		checks_enabled=atoi(PQgetvalue(xsddb_pgres,tuple,11));

		/* accept passive service checks */
		accept_passive_service_checks=atoi(PQgetvalue(xsddb_pgres,tuple,12));

		/* event handler_enabled */
		event_handler_enabled=atoi(PQgetvalue(xsddb_pgres,tuple,13));

		/* last state change */
		last_state_change=(time_t)strtoul(PQgetvalue(xsddb_pgres,tuple,14),NULL,10);

		/* problem has been acknowledged */
		problem_has_been_acknowledged=atoi(PQgetvalue(xsddb_pgres,tuple,15));

		/* times ok, warning, unknown, and critical */
		time_ok=strtoul(PQgetvalue(xsddb_pgres,tuple,17),NULL,10);
		time_warning=strtoul(PQgetvalue(xsddb_pgres,tuple,18),NULL,10);
		time_unknown=strtoul(PQgetvalue(xsddb_pgres,tuple,19),NULL,10);
		time_critical=strtoul(PQgetvalue(xsddb_pgres,tuple,20),NULL,10);

		/* last notification */
		last_notification=(time_t)strtoul(PQgetvalue(xsddb_pgres,tuple,21),NULL,10);

		/* current notification number */
		current_notification_number=atoi(PQgetvalue(xsddb_pgres,tuple,22));

		/* notifications enabled */
		notifications_enabled=atoi(PQgetvalue(xsddb_pgres,tuple,23));

		/* latency */
		latency=atoi(PQgetvalue(xsddb_pgres,tuple,24));

		/* execution time */
		execution_time=atoi(PQgetvalue(xsddb_pgres,tuple,25));

		/* flap detection enabled */
		flap_detection_enabled=atoi(PQgetvalue(xsddb_pgres,tuple,26));

		/* flapping indicator */
		is_flapping=atoi(PQgetvalue(xsddb_pgres,tuple,27));

		/* percent state change */
		percent_state_change=strtod(PQgetvalue(xsddb_pgres,tuple,28),NULL);

		/* scheduled downtime depth */
		scheduled_downtime_depth=atoi(PQgetvalue(xsddb_pgres,tuple,29));

		/* failure prediction enabled */
		failure_prediction_enabled=atoi(PQgetvalue(xsddb_pgres,tuple,30));

		/* process performance data */
		process_performance_data=atoi(PQgetvalue(xsddb_pgres,tuple,31));

		/* obsess over service */
		obsess_over_service=atoi(PQgetvalue(xsddb_pgres,tuple,32));

		/* add the service status */
		result=add_service_status(PQgetvalue(xsddb_pgres,tuple,0),PQgetvalue(xsddb_pgres,tuple,1),PQgetvalue(xsddb_pgres,tuple,2),last_update,current_attempt,max_attempts,state_type,last_check,next_check,check_type,checks_enabled,accept_passive_service_checks,event_handler_enabled,last_state_change,problem_has_been_acknowledged,PQgetvalue(xsddb_pgres,tuple,16),time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification_number,notifications_enabled,latency,execution_time,flap_detection_enabled,is_flapping,percent_state_change,scheduled_downtime_depth,failure_prediction_enabled,process_performance_data,obsess_over_service,PQgetvalue(xsddb_pgres,tuple,33));
		if(result!=OK){
			PQclear(xsddb_pgres);
			return ERROR;
		        }
	        }

	/* free memory used to store query results */
	PQclear(xsddb_pgres);
#endif

	return OK;
        }


#endif




#ifdef NSCORE

/******************************************************************/
/*********************** MISC FUNCTIONS ***************************/
/******************************************************************/

/* escape a string */
int xsddb_escape_string(char *escaped_string, char *raw_string){
	int x,y;

	if(raw_string==NULL || escaped_string==NULL){
		escaped_string=NULL;
		return ERROR;
	        }	  

	/* escape characters */
	for(x=0,y=0;x<strlen(raw_string);x++){
#ifdef USE_XSDMYSQL
		if(raw_string[x]=='\'' || raw_string[x]=='\"' || raw_string[x]=='*' || raw_string[x]=='\\' || raw_string[x]=='$' || raw_string[x]=='?' || raw_string[x]=='.' || raw_string[x]=='^' || raw_string[x]=='+' || raw_string[x]=='[' || raw_string[x]==']' || raw_string[x]=='(' || raw_string[x]==')')
		  escaped_string[y++]='\\';
#endif
#ifdef USE_XSDPGSQL
		if(! (isspace(raw_string[x]) || isalnum(raw_string[x]) || (raw_string[x]=='_')) )
			escaped_string[y++]='\\';
#endif
		escaped_string[y++]=raw_string[x];
	        }
	escaped_string[y]='\0';

	return OK;
	}

#endif

