/*****************************************************************************
 *
 * XDDDB.C - Database routines for downtime data
 *
 * Copyright (c) 2001-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   05-15-2002
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
#include "../common/downtime.h"

#ifdef NSCORE
#include "../common/objects.h"
#include "../base/nagios.h"
#endif

#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/

#include "xdddb.h"

#ifdef USE_XDDMYSQL
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#endif

#ifdef USE_XDDPGSQL
#include <pgsql/libpq-fe.h>
#endif


char xdddb_host[XDDDB_BUFFER_LENGTH]="";
char xdddb_username[XDDDB_BUFFER_LENGTH]="";
char xdddb_password[XDDDB_BUFFER_LENGTH]="";
char xdddb_database[XDDDB_BUFFER_LENGTH]="";
int xdddb_port=3306;
int xdddb_optimize_data=TRUE;

#ifdef USE_XDDMYSQL
MYSQL xdddb_mysql;
#endif

#ifdef USE_XDDPGSQL
PGconn *xdddb_pgconn=NULL;
PGresult *xdddb_pgres=NULL;
#endif




/******************************************************************/
/***************** COMMON CONFIG INITIALIZATION  ******************/
/******************************************************************/

/* grab configuration information from appropriate config file(s) */
int xdddb_grab_config_info(char *config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	FILE *fp;
#ifdef NSCORE
	FILE *fp2;
	char *temp_buffer;
#endif

	/* DBMS-specifc defaults */
#ifdef USE_XDDMYSQL
	xdddb_port=3306;
#endif
#ifdef USE_XDDPGSQL
	xdddb_port=5432;
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
		/* core programs reads comment vars from resource config file */
		if(strstr(input_buffer,"resource_file")==input_buffer){

			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			
			fp2=fopen(temp_buffer,"r");
			if(fp2==NULL)
				continue;

			/* read in all lines from the resource config file */
			for(fgets(input_buffer,sizeof(input_buffer)-1,fp2);!feof(fp2);fgets(input_buffer,sizeof(input_buffer)-1,fp2)){

				/* skip blank lines and comments */
				if(input_buffer[0]=='#' || input_buffer[0]=='\x0' || input_buffer[0]=='\n' || input_buffer[0]=='\r')
					continue;

				strip(input_buffer);

				xdddb_grab_config_directives(input_buffer);
			        }

			fclose(fp2);
		        }
#endif

#ifdef NSCGI
		/* CGIs read variables directly from the CGI config file */
		xdddb_grab_config_directives(input_buffer);
#endif
	        }

	fclose(fp);

	return OK;
        }


void xdddb_grab_config_directives(char *input_buffer){
	char *temp_buffer;

	/* server name/address */
	if(strstr(input_buffer,"xdddb_host")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xdddb_host,temp_buffer,sizeof(xdddb_host)-1);
		xdddb_host[sizeof(xdddb_host)-1]='\x0';
	        }

	/* username */
	else if(strstr(input_buffer,"xdddb_username")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xdddb_username,temp_buffer,sizeof(xdddb_username)-1);
		xdddb_username[sizeof(xdddb_username)-1]='\x0';
	        }

	/* password */
	else if(strstr(input_buffer,"xdddb_password")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xdddb_password,temp_buffer,sizeof(xdddb_password)-1);
		xdddb_password[sizeof(xdddb_password)-1]='\x0';
	        }

	/* database */
	else if(strstr(input_buffer,"xdddb_database")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xdddb_database,temp_buffer,sizeof(xdddb_database)-1);
		xdddb_database[sizeof(xdddb_database)-1]='\x0';
	        }

	/* port */
	else if(strstr(input_buffer,"xdddb_port")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		xdddb_port=atoi(temp_buffer);
	        }

	/* optimize data option */
	else if(strstr(input_buffer,"xdddb_optimize_data")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		xdddb_optimize_data=(atoi(temp_buffer)>0)?TRUE:FALSE;
	        }

	return;	
        }




/******************************************************************/
/******************** CONNECTION FUNCTIONS ************************/
/******************************************************************/


/* initialize database structures */
int xdddb_initialize(void){
#ifdef NSCORE
	char buffer[MAX_INPUT_BUFFER];
#endif

#ifdef USE_XDDMYSQL
	if(!mysql_init(&xdddb_mysql)){
#ifdef NSCORE
		snprintf(buffer,sizeof(buffer)-1,"Error: mysql_init() failed for downtime data\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
#endif
		return ERROR;
	        }
#endif

	return OK;
        }


/* try and connect to the database server */
int xdddb_connect(void){
#ifdef USE_XDDPGSQL
	char connect_string[XDDDB_BUFFER_LENGTH];
#endif
#ifdef NSCORE
	char buffer[MAX_INPUT_BUFFER];
#endif

#ifdef USE_XDDMYSQL
	/* establish a connection to the MySQL server */
	if(!mysql_real_connect(&xdddb_mysql,xdddb_host,xdddb_username,xdddb_password,xdddb_database,xdddb_port,NULL,0)){

		mysql_close(&xdddb_mysql);

#ifdef NSCORE
		snprintf(buffer,sizeof(buffer)-1,"Error: Could not connect to MySQL database '%s' on host '%s' using username '%s' and password '%s' for downtime data!\n",xdddb_database,xdddb_host,xdddb_username,xdddb_password);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
#endif

		return ERROR;
	        }
#endif


#ifdef USE_XDDPGSQL
	/* establish a connection to the PostgreSQL server */
	snprintf(connect_string,sizeof(connect_string)-1,"host='%s' port=%d dbname='%s' user='%s' password='%s'",xdddb_host,xdddb_port,xdddb_database,xdddb_username,xdddb_password);
	connect_string[sizeof(connect_string)-1]='\x0';
	xdddb_pgconn=PQconnectdb(connect_string);

	if(PQstatus(xdddb_pgconn)==CONNECTION_BAD){

		PQfinish(xdddb_pgconn);

#ifdef NSCORE
		snprintf(buffer,sizeof(buffer)-1,"Error: Could not connect to PostgreSQL database '%s' on host '%s' using username '%s' and password '%s' for downtime data!\n",xdddb_database,xdddb_host,xdddb_username,xdddb_password);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
#endif

		return ERROR;
	        }
#endif

	return OK;
        }


/* disconnect from database */
int xdddb_disconnect(void){

#ifdef USE_XDDMYSQL
	/* close the connection to the database server */		
	mysql_close(&xdddb_mysql);
#endif

#ifdef USE_XDDPGSQL
	/* close database connection and cleanup */
	if(PQstatus(xdddb_pgconn)!=CONNECTION_BAD)
		PQfinish(xdddb_pgconn);
#endif

	return OK;
        }


/* execute a SQL query */
int xdddb_query(char *sql_statement){
	int result=OK;

#ifdef DEBUG0
	printf("xdddb_query() start\n");
#endif

#ifdef DEBUG5
	printf("XDDDB_QUERY: %s\n",sql_statement);
#endif

#ifdef USE_XDDMYSQL
	if(mysql_query(&xdddb_mysql,sql_statement))
		result=ERROR;
#endif

#ifdef USE_XDDPGSQL
	xdddb_pgres=PQexec(xdddb_pgconn,sql_statement);
	if(xdddb_pgres==NULL || PQresultStatus(xdddb_pgres)==PGRES_FATAL_ERROR || PQresultStatus(xdddb_pgres)==PGRES_BAD_RESPONSE){
		PQclear(xdddb_pgres);
		result=ERROR;
	        }
#endif

#ifdef DEBUG5
	if(result==ERROR)
		printf("\tAn error occurred in the query!\n");
	printf("\tXDDDB_QUERY Done.\n");
#endif

#ifdef DEBUG0
	printf("xdddb_query() end\n");
#endif

	return result;
        }


/* frees memory associated with query results */
int xdddb_free_query_memory(void){

#ifdef DEBUG0
	printf("xdddb_free_query_memory() start\n");
#endif

#ifdef USE_XDDPGSQL
	PQclear(xdddb_pgres);
#endif
	
#ifdef DEBUG0
	printf("xdddb_free_query_memory() end\n");
#endif
	
	return OK;
        }



#ifdef NSCORE

/******************************************************************/
/************ DOWNTIME INITIALIZATION/CLEANUP FUNCTIONS ************/
/******************************************************************/


/* initialize downtime data */
int xdddb_initialize_downtime_data(char *main_config_file){
	char buffer[MAX_INPUT_BUFFER];

	/* grab configuration information */
	if(xdddb_grab_config_info(main_config_file)==ERROR){

		snprintf(buffer,sizeof(buffer)-1,"Error: Failed to grab database configuration information for downtime data\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* initialize database structures */
	xdddb_initialize();

	/* connect to the db */
	if(xdddb_connect()==ERROR)
		return ERROR;

	/* clean up the old downtime data */
	xdddb_validate_downtime_data();

	/* optimize downtime tables */
	xdddb_optimize_tables();

	/* read downtime data into memory */
	xdddb_read_downtime_data(main_config_file);

	/* disconnect from db */
	xdddb_disconnect();

	return OK;
        }



/* removes invalid and old downtime entries */
int xdddb_validate_downtime_data(void){

	/* delete downtime entries for hosts that don't exist anymore... */
	xdddb_validate_host_downtime();

	/* delete downtime entries for services that don't exist anymore... */
	xdddb_validate_service_downtime();

	return OK;
        }



/* deletes downtime entries for hosts that do not exist */
int xdddb_validate_host_downtime(void){
	char sql_statement[XDDDB_SQL_LENGTH];
	host *temp_host=NULL;
	time_t end_time;
#ifdef USE_XDDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XDDPGSQL
	int ntuples;
	int tuple;
	int *deletes=NULL;
	int current_delete=0;
	int total_deletes=0;
#endif

#ifdef DEBUG0
	printf("xdddb_validate_host_downtime() start\n");
#endif

	/* construct the SQL statement */
#ifdef USE_XDDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT hostdowntime_id, host_name, UNIX_TIMESTAMP(end_time) AS end_time FROM %s",XDDDB_HOSTDOWNTIME_TABLE);
#endif
#ifdef USE_XDDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT hostdowntime_id, host_name, date_part('epoch',end_time) FROM %s",XDDDB_HOSTDOWNTIME_TABLE);
#endif
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xdddb_query(sql_statement)==ERROR){
		xdddb_free_query_memory();
		return ERROR;
	        }


#ifdef USE_XDDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xdddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* find the host that this entry is associated with */
		temp_host=find_host(result_row[1],NULL);

		/* if we couldn't find the host, delete this downtime entry */
		if(temp_host==NULL){
			xdddb_delete_host_downtime(atoi(result_row[0]),TRUE);
			continue;
		        }

		/* get the end time */
		end_time=(time_t)strtoul(result_row[2],NULL,10);

		/* delete the downtime entry if its already expired */
		if(end_time<time(NULL))
			xdddb_delete_host_downtime(atoi(result_row[0]),TRUE);
                }
#endif

#ifdef USE_XDDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xdddb_pgres);

	/* allocate memory for storing IDs we're going to delete */
	deletes=(int *)malloc((ntuples+1)*sizeof(int));
	if(deletes==NULL){
		PQclear(xdddb_pgres);
		return ERROR;
	        }

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* find the host that this entry is associated with */
		temp_host=find_host(PQgetvalue(xdddb_pgres,tuple,1),NULL);

		/* if we couldn't find the host, mark this entry for deletion */
		if(temp_host==NULL){
			deletes[total_deletes++]=atoi(PQgetvalue(xdddb_pgres,tuple,0));
			continue;
		        }
	
		/* get the end time */
		end_time=(time_t)strtoul(PQgetvalue(xdddb_pgres,tuple,2),NULL,10);

		/* mark this entry for deletion if its already expired */
		if(end_time<time(NULL))
			deletes[total_deletes++]=atoi(PQgetvalue(xdddb_pgres,tuple,0));
	        }

	/* free memory used to store query results */
	PQclear(xdddb_pgres);

	/* delete all records we should */
	for(current_delete=0;current_delete<total_deletes;current_delete++)
		xdddb_delete_host_downtime(deletes[current_delete],TRUE);
	
	/* free memory */
	free(deletes);
#endif

#ifdef DEBUG0
	printf("xdddb_validate_host_downtime() end\n");
#endif

	return OK;
        }


/* deletes downtime entries for services that do not exist */
int xdddb_validate_service_downtime(void){
	char sql_statement[XDDDB_SQL_LENGTH];
	service *temp_service=NULL;
	time_t end_time;
#ifdef USE_XDDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XDDPGSQL
	int ntuples;
	int tuple;
	int *deletes=NULL;
	int current_delete=0;
	int total_deletes=0;
#endif


#ifdef DEBUG0
	printf("xdddb_validate_service_downtime() start\n");
#endif

	/* construct the SQL statement */
#ifdef USE_XDDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT servicedowntime_id, host_name, service_description, UNIX_TIMESTAMP(end_time) AS end_time FROM %s",XDDDB_SERVICEDOWNTIME_TABLE);
#endif
#ifdef USE_XDDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT servicedowntime_id, host_name, service_description, date_part('epoch',end_time) FROM %s",XDDDB_SERVICEDOWNTIME_TABLE);
#endif
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xdddb_query(sql_statement)==ERROR){
		xdddb_free_query_memory();
		return ERROR;
	        }

#ifdef USE_XDDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xdddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* find the service that this entry is associated with */
		temp_service=find_service(result_row[1],result_row[2],NULL);

		/* if we couldn't find the service, delete this entry */
		if(temp_service==NULL){
			xdddb_delete_service_downtime(atoi(result_row[0]),TRUE);
			continue;
		        }

		/* get the end time */
		end_time=(time_t)strtoul(result_row[3],NULL,10);

		/* delete the downtime entry if its already expired */
		if(end_time<time(NULL))
			xdddb_delete_service_downtime(atoi(result_row[0]),TRUE);
                }
#endif

#ifdef USE_XDDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xdddb_pgres);

	/* allocate memory for storing IDs we're going to delete */
	deletes=(int *)malloc((ntuples+1)*sizeof(int));
	if(deletes==NULL){
		PQclear(xdddb_pgres);
		return ERROR;
	        }

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* find the service that this entry is associated with */
		temp_service=find_service(PQgetvalue(xdddb_pgres,tuple,1),PQgetvalue(xdddb_pgres,tuple,2),NULL);

		/* if we couldn't find the service, delete this entry */
		if(temp_service==NULL){
			deletes[total_deletes++]=atoi(PQgetvalue(xdddb_pgres,tuple,0));
			continue;
		        }

		/* get the end time */
		end_time=(time_t)strtoul(PQgetvalue(xdddb_pgres,tuple,3),NULL,10);

		/* delete the downtime entry if its already expired */
		if(end_time<time(NULL))
			deletes[total_deletes++]=atoi(PQgetvalue(xdddb_pgres,tuple,0));
	        }

	/* free memory used to store query results */
	PQclear(xdddb_pgres);

	/* delete all records we should */
	for(current_delete=0;current_delete<total_deletes;current_delete++)
		xdddb_delete_service_downtime(deletes[current_delete],TRUE);
	
	/* free memory */
	free(deletes);
#endif

#ifdef DEBUG0
	printf("xdddb_validate_service_downtime() start\n");
#endif

	return OK;
        }




/* deinit downtime data */
int xdddb_cleanup_downtime_data(char *main_config_file){

	/* we don't have to do anything... */

	return OK;
        }




/**********************************************************/
/**************** OPTIMIZATION FUNCTIONS ******************/
/**********************************************************/

/* optimizes data in downtime tables */
int xdddb_optimize_tables(void){
#ifdef USE_XDDPGSQL
	char sql_statement[XDDDB_SQL_LENGTH];
#endif

#ifdef DEBUG0
	printf("xdddb_optimize_tables() start\n");
#endif

	/* don't optimize tables if user doesn't want us to */
	if(xdddb_optimize_data==FALSE)
		return OK;

#ifdef USE_XDDPGSQL
	
	/* VACUUM host and service downtime tables (user we're authenticated as must be owner of tables) */

	snprintf(sql_statement,sizeof(sql_statement)-1,"VACUUM %s",XDDDB_HOSTDOWNTIME_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	xdddb_query(sql_statement);
	xdddb_free_query_memory();

	snprintf(sql_statement,sizeof(sql_statement)-1,"VACUUM %s",XDDDB_SERVICEDOWNTIME_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	xdddb_query(sql_statement);
	xdddb_free_query_memory();

#endif

#ifdef DEBUG0
	printf("xdddb_optimize_tables() end\n");
#endif

	return OK;
        }




/******************************************************************/
/***************** DEFAULT DATA OUTPUT FUNCTIONS ******************/
/******************************************************************/


/* saves a new host downtime */
int xdddb_save_host_downtime(char *host_name, time_t entry_time, char *author_name, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long duration, int *downtime_id){
	char sql_statement[XDDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;
	char *escaped_host_name;
	char *escaped_author_name;
	char *escaped_comment_data;

#ifdef DEBUG0
	printf("xdddb_save_host_downtime() start\n");
#endif

	/* connect to the database */
	if(xdddb_connect()==ERROR)
		return ERROR;

	/* escape the strings, as they may have quotes, etc... */
	escaped_host_name=(char *)malloc(strlen(host_name)*2+1);
	xdddb_escape_string(escaped_host_name,host_name);
	escaped_author_name=(char *)malloc(strlen(author_name)*2+1);
	xdddb_escape_string(escaped_author_name,author_name);
	escaped_comment_data=(char *)malloc(strlen(comment_data)*2+1);
	xdddb_escape_string(escaped_comment_data,comment_data);

	/* construct the SQL statement */
#ifdef USE_XDDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,entry_time,author_name,comment_data,start_time,end_time,fixed,duration) VALUES ('%s',FROM_UNIXTIME(%lu),'%s','%s',FROM_UNIXTIME(%lu),FROM_UNIXTIME(%lu),'%d','%lu')",XDDDB_HOSTDOWNTIME_TABLE,escaped_host_name,entry_time,escaped_author_name,escaped_comment_data,start_time,end_time,fixed,duration);
#endif
#ifdef USE_XDDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,entry_time,author_name,comment_data,start_time,end_time,fixed,duration) VALUES ('%s',%lu::int4::abstime::timestamp,'%s','%s',%lu::int4::abstime::timestamp,%lu::int4::abstime::timestamp,'%d','%lu')",XDDDB_HOSTDOWNTIME_TABLE,escaped_host_name,entry_time,escaped_author_name,escaped_comment_data,start_time,end_time,fixed,duration);
#endif
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* free memory for the escaped strings */
	free(escaped_host_name);
	free(escaped_author_name);
	free(escaped_comment_data);

	/* add the host downtime */
	result=xdddb_query(sql_statement);

	/* there was an error adding the downtime... */
	if(result==ERROR){

		xdddb_free_query_memory();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not insert record for host downtime in table '%s' of database '%s'\n",XDDDB_HOSTDOWNTIME_TABLE,xdddb_database);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		/* return an invalid downtime number */
		if(downtime_id!=NULL)
			*downtime_id=-1;

		/* disconnect from the database */
		xdddb_disconnect();

		return ERROR;
	        }

	/* pass back the id of the downtime entry we just added */
	if(downtime_id!=NULL){
#ifdef USE_XDDMYSQL
		*downtime_id=(int)mysql_insert_id(&xdddb_mysql);
#endif
#ifdef USE_XDDPGSQL
		snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT hostdowntime_id FROM %s WHERE oid='%s'",XDDDB_HOSTDOWNTIME_TABLE,PQoidStatus(xdddb_pgres));
		sql_statement[sizeof(sql_statement)-1]='\x0';
		xdddb_free_query_memory();
		result=xdddb_query(sql_statement);
		if(result==ERROR)
			*downtime_id=-1;
		else{
			if(PQntuples(xdddb_pgres)<1)
				*downtime_id=-1;
			else
				*downtime_id=atoi(PQgetvalue(xdddb_pgres,0,0));
		        }
#endif
	        }

	xdddb_free_query_memory();

	/* disconnect from the database */
	xdddb_disconnect();

#ifdef DEBUG0
	printf("xdddb_save_host_downtime() end\n");
#endif

	return OK;
        }


/* saves a new service downtime entry */
int xdddb_save_service_downtime(char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long duration, int *downtime_id){
	char sql_statement[XDDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;
	char *escaped_host_name;
	char *escaped_svc_description;
	char *escaped_author_name;
	char *escaped_comment_data;

#ifdef DEBUG0
	printf("xdddb_save_service_downtime() start\n");
#endif

	/* connect to the database */
	if(xdddb_connect()==ERROR)
		return ERROR;

	/* escape the strings, as they may have quotes, etc... */
	escaped_host_name=(char *)malloc(strlen(host_name)*2+1);
	xdddb_escape_string(escaped_host_name,host_name);
	escaped_svc_description=(char *)malloc(strlen(svc_description)*2+1);
	xdddb_escape_string(escaped_svc_description,svc_description);
	escaped_author_name=(char *)malloc(strlen(author_name)*2+1);
	xdddb_escape_string(escaped_author_name,author_name);
	escaped_comment_data=(char *)malloc(strlen(comment_data)*2+1);
	xdddb_escape_string(escaped_comment_data,comment_data);

	/* construct the SQL statement */
#ifdef USE_XDDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,service_description,entry_time,author_name,comment_data,start_time,end_time,fixed,duration) VALUES ('%s','%s',FROM_UNIXTIME(%lu),'%s','%s',FROM_UNIXTIME(%lu),FROM_UNIXTIME(%lu),'%d','%lu')",XDDDB_SERVICEDOWNTIME_TABLE,escaped_host_name,escaped_svc_description,entry_time,escaped_author_name,escaped_comment_data,start_time,end_time,fixed,duration);
#endif
#ifdef USE_XDDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,service_description,entry_time,author_name,comment_data,start_time,end_time,fixed,duration) VALUES ('%s','%s',%lu::int4::abstime::timestamp,'%s','%s',%lu::int4::abstime::timestamp,%lu::int4::abstime::timestamp,'%d','%lu')",XDDDB_SERVICEDOWNTIME_TABLE,escaped_host_name,escaped_svc_description,entry_time,escaped_author_name,escaped_comment_data,start_time,end_time,fixed,duration);
#endif
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* free memory for the escaped strings */
	free(escaped_host_name);
	free(escaped_svc_description);
	free(escaped_author_name);
	free(escaped_comment_data);

	/* add the service downtime */
	result=xdddb_query(sql_statement);

	/* there was an error adding the downtime... */
	if(result==ERROR){

		xdddb_free_query_memory();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not insert record for service downtime in table '%s' of database '%s'\n",XDDDB_SERVICEDOWNTIME_TABLE,xdddb_database);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		/* return an invalid downtime number */
		if(downtime_id!=NULL)
			*downtime_id=-1;

		/* disconnect from the database */
		xdddb_disconnect();

		return ERROR;
	        }

	/* pass back the id of the downtime entry we just added */
	if(downtime_id!=NULL){
#ifdef USE_XDDMYSQL
		*downtime_id=(int)mysql_insert_id(&xdddb_mysql);
#endif
#ifdef USE_XDDPGSQL
		snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT servicedowntime_id FROM %s WHERE oid='%s'",XDDDB_SERVICEDOWNTIME_TABLE,PQoidStatus(xdddb_pgres));
		sql_statement[sizeof(sql_statement)-1]='\x0';
		xdddb_free_query_memory();
		result=xdddb_query(sql_statement);
		if(result==ERROR)
			*downtime_id=-1;
		else{
			if(PQntuples(xdddb_pgres)<1)
				*downtime_id=-1;
			else
				*downtime_id=atoi(PQgetvalue(xdddb_pgres,0,0));
		        }
#endif
	        }

	xdddb_free_query_memory();

	/* disconnect from the database */
	xdddb_disconnect();

#ifdef DEBUG0
	printf("xdddb_save_service_downtime() end\n");
#endif

	return OK;
        }



/******************************************************************/
/**************** DOWNTIME DELETION FUNCTIONS *********************/
/******************************************************************/


/* deletes a host downtime entry */
int xdddb_delete_host_downtime(int downtime_id, int bulk_operation){
	char sql_statement[XDDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;

#ifdef DEBUG0
	printf("xdddb_delete_host_downtime() start\n");
#endif

	/* connect to the database is necessary */
	if(bulk_operation==FALSE){
		if(xdddb_connect()==ERROR)
			return ERROR;
	        }

	/* delete a specific service downtime entry */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s WHERE hostdowntime_id='%d'",XDDDB_HOSTDOWNTIME_TABLE,downtime_id);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xdddb_query(sql_statement);

#ifdef USE_XDDPGSQL
	/* free memory used to store query results */
	PQclear(xdddb_pgres);
#endif

	/* there was an error */
	if(result==ERROR){

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not delete host downtime entry #%d from table '%s'\n",downtime_id,XDDDB_HOSTDOWNTIME_TABLE);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* disconnect from the database is necessary */
	if(bulk_operation==FALSE)
		xdddb_disconnect();

#ifdef DEBUG0
	printf("xdddb_delete_host_downtime() end\n");
#endif

	return OK;
        }


/* deletes a service downtime entry */
int xdddb_delete_service_downtime(int downtime_id, int bulk_operation){
	char sql_statement[XDDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;

#ifdef DEBUG0
	printf("xdddb_delete_service_downtime() start\n");
#endif

	/* connect to the database is necessary */
	if(bulk_operation==FALSE){
		if(xdddb_connect()==ERROR)
			return ERROR;
	        }

	/* delete a specific service downtime entry */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s WHERE servicedowntime_id='%d'",XDDDB_SERVICEDOWNTIME_TABLE,downtime_id);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xdddb_query(sql_statement);

#ifdef USE_XDDPGSQL
	/* free memory used to store query results */
	PQclear(xdddb_pgres);
#endif

	/* there was an error deleting the downtime entry... */
	if(result==ERROR){

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not delete service downtime entry #%d from table '%s'\n",downtime_id,XDDDB_SERVICEDOWNTIME_TABLE);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* disconnect from the database is necessary */
	if(bulk_operation==FALSE)
		xdddb_disconnect();

#ifdef DEBUG0
	printf("xdddb_delete_service_downtime() end\n");
#endif

	return OK;
        }


#endif






/******************************************************************/
/****************** DOWNTIME INPUT FUNCTIONS **********************/
/******************************************************************/


/* read the downtime data */
int xdddb_read_downtime_data(char *config_file){

#ifdef DEBUG0
	printf("xdddb_read_downtime_data() start\n");
#endif

#ifdef NSCGI
	/* grab configuration information */
	if(xdddb_grab_config_info(config_file)==ERROR)
		return ERROR;

	/* initialize database structures */
	if(xdddb_initialize()==ERROR)
		return ERROR;

	/* establish a connection to the database server */
	if(xdddb_connect()==ERROR)
		return ERROR;
#endif

	/* read in host donwitme... */
	xdddb_read_host_downtime();

	/* read in service downtime... */
	xdddb_read_service_downtime();

#ifdef NSCGI
	/* close database connection */
	xdddb_disconnect();
#endif

#ifdef DEBUG0
	printf("xdddb_read_downtime_data() end\n");
#endif

	return OK;
        }


/* reads all host downtime */
int xdddb_read_host_downtime(void){
	char sql_statement[XDDDB_SQL_LENGTH];
	int x;
	time_t entry_time;
	time_t start_time;
	time_t end_time;
	int fixed;
	unsigned long duration;
	int hostdowntime_id;
#ifdef USE_XDDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XDDPGSQL
	int ntuples;
	int tuple;
#endif

#ifdef DEBUG0
	printf("xdddb_read_host_downtime() start\n");
#endif

	/* construct the SQL statement */
#ifdef USE_XDDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT hostdowntime_id, host_name, UNIX_TIMESTAMP(entry_time) AS entry_time, UNIX_TIMESTAMP(start_time) AS start_time, UNIX_TIMESTAMP(end_time) AS end_time, fixed, duration, author_name, comment_data FROM %s",XDDDB_HOSTDOWNTIME_TABLE);
#endif
#ifdef USE_XDDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT hostdowntime_id, host_name, date_part('epoch',entry_time), date_part('epoch',start_time), date_part('epoch',end_time), fixed, duration, author_name, comment_data FROM %s",XDDDB_HOSTDOWNTIME_TABLE);
#endif
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xdddb_query(sql_statement)==ERROR)
		return ERROR;


#ifdef USE_XDDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xdddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* we have nine rows - make sure they're not NULL */
		for(x=0;x<9;x++){
			if(result_row[x]==NULL){
				mysql_free_result(query_result);
				return ERROR;
			        }
	                }

		/* get the downtime id */
		hostdowntime_id=atoi(result_row[0]);

		/* entry time */
		entry_time=(time_t)strtoul(result_row[2],NULL,10);

		/* start time */
		start_time=(time_t)strtoul(result_row[3],NULL,10);

		/* end time */
		end_time=(time_t)strtoul(result_row[4],NULL,10);

		/* fixed flag */
		fixed=atoi(result_row[5]);

		/* duration */
		duration=strtoul(result_row[6],NULL,10);

		/* add the host downtime */
		add_host_downtime(result_row[1],entry_time,result_row[7],result_row[8],start_time,end_time,fixed,duration,hostdowntime_id);

#ifdef NSCORE
		/* must register the downtime with Nagios so it can schedule it, add comments, etc. */
		register_downtime(HOST_DOWNTIME,hostdowntime_id);
#endif
	        }

	/* free memory used to store query results */
	mysql_free_result(query_result);
#endif

#ifdef USE_XDDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xdddb_pgres);

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* downtime id */
		hostdowntime_id=atoi(PQgetvalue(xdddb_pgres,tuple,0));
		
		/* entry time */
		entry_time=(time_t)strtoul(PQgetvalue(xdddb_pgres,tuple,2),NULL,10);
		
		/* start time */
		start_time=(time_t)strtoul(PQgetvalue(xdddb_pgres,tuple,3),NULL,10);
		
		/* end time */
		end_time=(time_t)strtoul(PQgetvalue(xdddb_pgres,tuple,4),NULL,10);
		
		/* fixed flag */
		fixed=atoi(PQgetvalue(xdddb_pgres,tuple,5));

		/* duration */
		duration=strtoul(PQgetvalue(xdddb_pgres,tuple,6),NULL,10);
		
		/* add the host downtime */
		add_host_downtime(PQgetvalue(xdddb_pgres,tuple,1),entry_time,PQgetvalue(xdddb_pgres,tuple,7),PQgetvalue(xdddb_pgres,tuple,8),start_time,end_time,fixed,duration,hostdowntime_id);

#ifdef NSCORE
		/* must register the downtime with Nagios so it can schedule it, add comments, etc. */
		register_downtime(HOST_DOWNTIME,hostdowntime_id);
#endif
	        }

	/* free memory used to store query results */
	PQclear(xdddb_pgres);
#endif

#ifdef DEBUG0
	printf("xdddb_read_host_downtime() end\n");
#endif

	return OK;
        }


/* reads all service downtime */
int xdddb_read_service_downtime(void){
	char sql_statement[XDDDB_SQL_LENGTH];
	int x;
	time_t entry_time;
	time_t start_time;
	time_t end_time;
	int fixed;
	unsigned long duration;
	int servicedowntime_id;
#ifdef USE_XDDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XDDPGSQL
	int ntuples;
	int tuple;
#endif

#ifdef DEBUG0
	printf("xdddb_read_service_downtime() start\n");
#endif

	/* construct the SQL statement */
#ifdef USE_XDDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT servicedowntime_id, host_name, service_description, UNIX_TIMESTAMP(entry_time) AS entry_time, UNIX_TIMESTAMP(start_time) AS start_time, UNIX_TIMESTAMP(end_time) AS end_time, fixed, duration, author_name, comment_data FROM %s",XDDDB_SERVICEDOWNTIME_TABLE);
#endif
#ifdef USE_XDDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT servicedowntime_id, host_name, service_description, date_part('epoch',entry_time), date_part('epoch',start_time), date_part('epoch',end_time), fixed, duration, author_name, comment_data FROM %s",XDDDB_SERVICEDOWNTIME_TABLE);
#endif
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xdddb_query(sql_statement)==ERROR)
		return ERROR;


#ifdef USE_XDDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xdddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* we have ten rows - make sure they're not NULL */
		for(x=0;x<10;x++){
			if(result_row[x]==NULL){
				mysql_free_result(query_result);
				return ERROR;
			        }
	                }

		/* get the downtime id */
		servicedowntime_id=atoi(result_row[0]);

		/* entry time */
		entry_time=(time_t)strtoul(result_row[3],NULL,10);

		/* start time */
		start_time=(time_t)strtoul(result_row[4],NULL,10);

		/* end time */
		end_time=(time_t)strtoul(result_row[5],NULL,10);

		/* fixed flag */
		fixed=atoi(result_row[6]);

		/* duration */
		duration=(time_t)strtoul(result_row[7],NULL,10);

		/* add the service downtime */
		add_service_downtime(result_row[1],result_row[2],entry_time,result_row[8],result_row[9],start_time,end_time,fixed,duration,servicedowntime_id);

#ifdef NSCORE
		/* must register the downtime with Nagios so it can schedule it, add comments, etc. */
		register_downtime(SERVICE_DOWNTIME,servicedowntime_id);
#endif
	        }

	/* free memory used to store query results */
	mysql_free_result(query_result);
#endif

#ifdef USE_XDDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xdddb_pgres);

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* downtime id */
		servicedowntime_id=atoi(PQgetvalue(xdddb_pgres,tuple,0));
		
		/* entry time */
		entry_time=(time_t)strtoul(PQgetvalue(xdddb_pgres,tuple,3),NULL,10);
		
		/* start time */
	        start_time=(time_t)strtoul(PQgetvalue(xdddb_pgres,tuple,4),NULL,10);
		
		/* end time */
		end_time=(time_t)strtoul(PQgetvalue(xdddb_pgres,tuple,5),NULL,10);
		
		/* fixed flag */
		fixed=atoi(PQgetvalue(xdddb_pgres,tuple,6));

		/* duration */
		duration=strtoul(PQgetvalue(xdddb_pgres,tuple,7),NULL,10);
		
		/* add the service downtime */
		add_service_downtime(PQgetvalue(xdddb_pgres,tuple,1),PQgetvalue(xdddb_pgres,tuple,2),entry_time,PQgetvalue(xdddb_pgres,tuple,8),PQgetvalue(xdddb_pgres,tuple,9),start_time,end_time,fixed,duration,servicedowntime_id);

#ifdef NSCORE
		/* must register the downtime with Nagios so it can schedule it, add comments, etc. */
		register_downtime(SERVICE_DOWNTIME,servicedowntime_id);
#endif
	        }

	/* free memory used to store query results */
	PQclear(xdddb_pgres);
#endif

#ifdef DEBUG0
	printf("xdddb_read_service_downtime() end\n");
#endif

	return OK;
        }




/******************************************************************/
/*********************** MISC FUNCTIONS ***************************/
/******************************************************************/

#ifdef NSCORE

/* escape a string */
int xdddb_escape_string(char *escaped_string, char *raw_string){
	int x,y;

	if(raw_string==NULL || escaped_string==NULL){
		escaped_string=NULL;
		return ERROR;
	        }	  

	/* escape characters */
	for(x=0,y=0;x<strlen(raw_string);x++){
#ifdef USE_XDDMYSQL
		if(raw_string[x]=='\'' || raw_string[x]=='\"' || raw_string[x]=='*' || raw_string[x]=='\\' || raw_string[x]=='$' || raw_string[x]=='?' || raw_string[x]=='.' || raw_string[x]=='^' || raw_string[x]=='+' || raw_string[x]=='[' || raw_string[x]==']' || raw_string[x]=='(' || raw_string[x]==')')
		  escaped_string[y++]='\\';
#endif
#ifdef USE_XDDPGSQL
		if(! (isspace(raw_string[x]) || isalnum(raw_string[x]) || (raw_string[x]=='_')) )
			escaped_string[y++]='\\';
#endif
		escaped_string[y++]=raw_string[x];
	        }
	escaped_string[y]='\0';

	return OK;
	}

#endif

