/*****************************************************************************
 *
 * XCDDB.C - Database routines for comment data
 *
 * Copyright (c) 2000-2002 Ethan Galstad (nagios@nagios.org)
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
#include "../common/comments.h"

#ifdef NSCORE
#include "../common/objects.h"
#include "../base/nagios.h"
#endif

#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/

#include "xcddb.h"

#ifdef USE_XCDMYSQL
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#endif

#ifdef USE_XCDPGSQL
#include <pgsql/libpq-fe.h>
#endif


char xcddb_host[XCDDB_BUFFER_LENGTH]="";
char xcddb_username[XCDDB_BUFFER_LENGTH]="";
char xcddb_password[XCDDB_BUFFER_LENGTH]="";
char xcddb_database[XCDDB_BUFFER_LENGTH]="";
int xcddb_port=3306;
int xcddb_optimize_data=TRUE;

#ifdef USE_XCDMYSQL
MYSQL xcddb_mysql;
#endif

#ifdef USE_XCDPGSQL
PGconn *xcddb_pgconn=NULL;
PGresult *xcddb_pgres=NULL;
#endif




/******************************************************************/
/***************** COMMON CONFIG INITIALIZATION  ******************/
/******************************************************************/

/* grab configuration information from appropriate config file(s) */
int xcddb_grab_config_info(char *config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	FILE *fp;
#ifdef NSCORE
	FILE *fp2;
	char *temp_buffer;
#endif

	/* DBMS-specifc defaults */
#ifdef USE_XCDMYSQL
	xcddb_port=3306;
#endif
#ifdef USE_XCDPGSQL
	xcddb_port=5432;
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

				xcddb_grab_config_directives(input_buffer);
			        }

			fclose(fp2);
		        }
#endif

#ifdef NSCGI
		/* CGIs read variables directly from the CGI config file */
		xcddb_grab_config_directives(input_buffer);
#endif
	        }

	fclose(fp);

	return OK;
        }


void xcddb_grab_config_directives(char *input_buffer){
	char *temp_buffer;

	/* server name/address */
	if(strstr(input_buffer,"xcddb_host")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xcddb_host,temp_buffer,sizeof(xcddb_host)-1);
		xcddb_host[sizeof(xcddb_host)-1]='\x0';
	        }

	/* username */
	else if(strstr(input_buffer,"xcddb_username")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xcddb_username,temp_buffer,sizeof(xcddb_username)-1);
		xcddb_username[sizeof(xcddb_username)-1]='\x0';
	        }

	/* password */
	else if(strstr(input_buffer,"xcddb_password")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xcddb_password,temp_buffer,sizeof(xcddb_password)-1);
		xcddb_password[sizeof(xcddb_password)-1]='\x0';
	        }

	/* database */
	else if(strstr(input_buffer,"xcddb_database")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		strncpy(xcddb_database,temp_buffer,sizeof(xcddb_database)-1);
		xcddb_database[sizeof(xcddb_database)-1]='\x0';
	        }

	/* port */
	else if(strstr(input_buffer,"xcddb_port")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		xcddb_port=atoi(temp_buffer);
	        }

	/* optimize data option */
	else if(strstr(input_buffer,"xcddb_optimize_data")==input_buffer){
		temp_buffer=strtok(input_buffer,"=");
		temp_buffer=strtok(NULL,"\n");
		if(temp_buffer==NULL)
			return;
		xcddb_optimize_data=(atoi(temp_buffer)>0)?TRUE:FALSE;
	        }

	return;	
        }




/******************************************************************/
/******************** CONNECTION FUNCTIONS ************************/
/******************************************************************/


/* initialize database structures */
int xcddb_initialize(void){
#ifdef NSCORE
	char buffer[MAX_INPUT_BUFFER];
#endif

#ifdef USE_XCDMYSQL
	if(!mysql_init(&xcddb_mysql)){
#ifdef NSCORE
		snprintf(buffer,sizeof(buffer)-1,"Error: mysql_init() failed for comment data\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
#endif
		return ERROR;
	        }
#endif

	return OK;
        }


/* try and connect to the database server */
int xcddb_connect(void){
#ifdef USE_XCDPGSQL
	char connect_string[XCDDB_BUFFER_LENGTH];
#endif
#ifdef NSCORE
	char buffer[MAX_INPUT_BUFFER];
#endif

#ifdef USE_XCDMYSQL
	/* establish a connection to the MySQL server */
	if(!mysql_real_connect(&xcddb_mysql,xcddb_host,xcddb_username,xcddb_password,xcddb_database,xcddb_port,NULL,0)){

		mysql_close(&xcddb_mysql);

#ifdef NSCORE
		snprintf(buffer,sizeof(buffer)-1,"Error: Could not connect to MySQL database '%s' on host '%s' using username '%s' and password '%s' for comment data!\n",xcddb_database,xcddb_host,xcddb_username,xcddb_password);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
#endif

		return ERROR;
	        }
#endif


#ifdef USE_XCDPGSQL
	/* establish a connection to the PostgreSQL server */
	snprintf(connect_string,sizeof(connect_string)-1,"host='%s' port=%d dbname='%s' user='%s' password='%s'",xcddb_host,xcddb_port,xcddb_database,xcddb_username,xcddb_password);
	connect_string[sizeof(connect_string)-1]='\x0';
	xcddb_pgconn=PQconnectdb(connect_string);

	if(PQstatus(xcddb_pgconn)==CONNECTION_BAD){

		PQfinish(xcddb_pgconn);

#ifdef NSCORE
		snprintf(buffer,sizeof(buffer)-1,"Error: Could not connect to PostgreSQL database '%s' on host '%s' using username '%s' and password '%s' for comment data!\n",xcddb_database,xcddb_host,xcddb_username,xcddb_password);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
#endif

		return ERROR;
	        }
#endif

	return OK;
        }


/* disconnect from database */
int xcddb_disconnect(void){

#ifdef USE_XCDMYSQL
	/* close the connection to the database server */		
	mysql_close(&xcddb_mysql);
#endif

#ifdef USE_XCDPGSQL
	/* close database connection and cleanup */
	if(PQstatus(xcddb_pgconn)!=CONNECTION_BAD)
		PQfinish(xcddb_pgconn);
#endif

	return OK;
        }


/* execute a SQL query */
int xcddb_query(char *sql_statement){
	int result=OK;

#ifdef DEBUG0
	printf("xcddb_query() start\n");
#endif

#ifdef DEBUG5
	printf("XCDDB_QUERY: %s\n",sql_statement);
#endif

#ifdef USE_XCDMYSQL
	if(mysql_query(&xcddb_mysql,sql_statement))
		result=ERROR;
#endif

#ifdef USE_XCDPGSQL
	xcddb_pgres=PQexec(xcddb_pgconn,sql_statement);
	if(xcddb_pgres==NULL || PQresultStatus(xcddb_pgres)==PGRES_FATAL_ERROR || PQresultStatus(xcddb_pgres)==PGRES_BAD_RESPONSE){
		PQclear(xcddb_pgres);
		result=ERROR;
	        }
#endif

#ifdef DEBUG5
	if(result==ERROR)
		printf("\tAn error occurred in the query!\n");
	printf("\tXCDDB_QUERY Done.\n");
#endif

#ifdef DEBUG0
	printf("xcddb_query() end\n");
#endif

	return result;
        }


/* frees memory associated with query results */
int xcddb_free_query_memory(void){

#ifdef DEBUG0
	printf("xcddb_free_query_memory() start\n");
#endif

#ifdef USE_XCDPGSQL
	PQclear(xcddb_pgres);
#endif
	
#ifdef DEBUG0
	printf("xcddb_free_query_memory() end\n");
#endif
	
	return OK;
        }



#ifdef NSCORE

/******************************************************************/
/************ COMMENT INITIALIZATION/CLEANUP FUNCTIONS ************/
/******************************************************************/


/* initialize comment data */
int xcddb_initialize_comment_data(char *main_config_file){
	char buffer[MAX_INPUT_BUFFER];

	/* grab configuration information */
	if(xcddb_grab_config_info(main_config_file)==ERROR){

		snprintf(buffer,sizeof(buffer)-1,"Error: Failed to grab database configuration information for comment data\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* initialize database structures */
	xcddb_initialize();

	/* connect to the db */
	if(xcddb_connect()==ERROR)
		return ERROR;

	/* clean up the old comment data */
	xcddb_validate_comment_data();

	/* optimize comment tables */
	xcddb_optimize_tables();

	/* disconnect from db */
	xcddb_disconnect();

	return OK;
        }



/* removes invalid and old comments */
int xcddb_validate_comment_data(void){
	char sql_statement[XCDDB_SQL_LENGTH];

	/* delete all non-persistent host comments... */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s WHERE persistent='0'",XCDDB_HOSTCOMMENTS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	xcddb_query(sql_statement);
	xcddb_free_query_memory();

	/* delete all non-persistent service comments... */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s WHERE persistent='0'",XCDDB_SERVICECOMMENTS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	xcddb_query(sql_statement);
	xcddb_free_query_memory();

	/* delete comments for hosts that don't exist anymore... */
	xcddb_validate_host_comments();

	/* delete comments for services that don't exist anymore... */
	xcddb_validate_service_comments();

	return OK;
        }



/* deletes comments for hosts that do not exist */
int xcddb_validate_host_comments(void){
	char sql_statement[XCDDB_SQL_LENGTH];
	host *temp_host=NULL;
#ifdef USE_XCDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XCDPGSQL
	int ntuples;
	int tuple;
	int *deletes=NULL;
	int total_deletes=0;
	int current_delete=0;
#endif

#ifdef DEBUG0
	printf("xcddb_validate_host_comments() start\n");
#endif

	/* construct the SQL statement */
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT hostcomment_id, host_name FROM %s",XCDDB_HOSTCOMMENTS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xcddb_query(sql_statement)==ERROR){
		xcddb_free_query_memory();
		return ERROR;
	        }


#ifdef USE_XCDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xcddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* find the host that this comment is associated with */
		temp_host=find_host(result_row[1],NULL);

		/* if we couldn't find the host, delete this comment */
		if(temp_host==NULL)
			xcddb_delete_host_comment(atoi(result_row[0]),TRUE);
                }
#endif

#ifdef USE_XCDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xcddb_pgres);

	/* allocate memory for storing IDs we're going to delete */
	deletes=(int *)malloc((ntuples+1)*sizeof(int));
	if(deletes==NULL){
		PQclear(xcddb_pgres);
		return ERROR;
	        }

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* find the host that this comment is associated with */
		temp_host=find_host(PQgetvalue(xcddb_pgres,tuple,1),NULL);

		/* if we couldn't find the host, mark this comment for deletion */
		if(temp_host==NULL)
			deletes[total_deletes++]=atoi(PQgetvalue(xcddb_pgres,tuple,0));
	        }

	/* free memory used to store query results */
	PQclear(xcddb_pgres);

	/* delete all records we should */
	for(current_delete=0;current_delete<total_deletes;current_delete++)
		xcddb_delete_host_comment(deletes[current_delete],TRUE);
	
	/* free memory */
	free(deletes);
#endif

#ifdef DEBUG0
	printf("xcddb_validate_host_comments() end\n");
#endif

	return OK;
        }


/* deletes comments for service that do not exist */
int xcddb_validate_service_comments(void){
	char sql_statement[XCDDB_SQL_LENGTH];
	service *temp_service=NULL;
#ifdef USE_XCDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XCDPGSQL
	int ntuples;
	int tuple;
	int *deletes=NULL;
	int current_delete=0;
	int total_deletes=0;
#endif

#ifdef DEBUG0
	printf("xcddb_validate_service_comments() start\n");
#endif

	/* construct the SQL statement */
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT servicecomment_id, host_name, service_description FROM %s",XCDDB_SERVICECOMMENTS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xcddb_query(sql_statement)==ERROR){
		xcddb_free_query_memory();
		return ERROR;
	        }

#ifdef USE_XCDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xcddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* find the service that this comment is associated with */
		temp_service=find_service(result_row[1],result_row[2],NULL);

		/* if we couldn't find the service, delete this comment */
		if(temp_service==NULL)
			xcddb_delete_service_comment(atoi(result_row[0]),TRUE);
                }
#endif

#ifdef USE_XCDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xcddb_pgres);

	/* allocate memory for storing IDs we're going to delete */
	deletes=(int *)malloc((ntuples+1)*sizeof(int));
	if(deletes==NULL){
		PQclear(xcddb_pgres);
		return ERROR;
	        }

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* find the service that this comment is associated with */
		temp_service=find_service(PQgetvalue(xcddb_pgres,tuple,1),PQgetvalue(xcddb_pgres,tuple,2),NULL);

		/* if we couldn't find the service, mark this comment for deletion */
		if(temp_service==NULL)
			deletes[total_deletes++]=atoi(PQgetvalue(xcddb_pgres,tuple,0));
	        }

	/* free memory used to store query results */
	PQclear(xcddb_pgres);

	/* delete all records we should */
	for(current_delete=0;current_delete<total_deletes;current_delete++)
		xcddb_delete_service_comment(deletes[current_delete],TRUE);
	
	/* free memory */
	free(deletes);
#endif

#ifdef DEBUG0
	printf("xcddb_validate_service_comments() end\n");
#endif

	return OK;
        }



/* deinit comment data */
int xcddb_cleanup_comment_data(char *main_config_file){

	/* we don't have to do anything... */

	return OK;
        }




/**********************************************************/
/**************** OPTIMIZATION FUNCTIONS ******************/
/**********************************************************/

/* optimizes data in comment tables */
int xcddb_optimize_tables(void){
#ifdef USE_XCDPGSQL
	char sql_statement[XCDDB_SQL_LENGTH];
#endif

#ifdef DEBUG0
	printf("xcddb_optimize_tables() start\n");
#endif

	/* don't optimize tables if user doesn't want us to */
	if(xcddb_optimize_data==FALSE)
		return OK;

#ifdef USE_XCDPGSQL
	
	/* VACUUM host and service comment tables (user we're authenticated as must be owner of tables) */

	snprintf(sql_statement,sizeof(sql_statement)-1,"VACUUM %s",XCDDB_HOSTCOMMENTS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	xcddb_query(sql_statement);
	xcddb_free_query_memory();

	snprintf(sql_statement,sizeof(sql_statement)-1,"VACUUM %s",XCDDB_SERVICECOMMENTS_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	xcddb_query(sql_statement);
	xcddb_free_query_memory();

#endif

#ifdef DEBUG0
	printf("xcddb_optimize_tables() end\n");
#endif

	return OK;
        }




/******************************************************************/
/***************** DEFAULT DATA OUTPUT FUNCTIONS ******************/
/******************************************************************/


/* saves a new host comment */
int xcddb_save_host_comment(char *host_name, time_t entry_time, char *author_name, char *comment_data, int persistent, int *comment_id){
	char sql_statement[XCDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;
	char *escaped_host_name;
	char *escaped_author_name;
	char *escaped_comment_data;

#ifdef DEBUG0
	printf("xcddb_save_host_comment() start\n");
#endif

	/* connect to the database */
	if(xcddb_connect()==ERROR)
		return ERROR;

	/* escape the strings, as they may have quotes, etc... */
	escaped_host_name=(char *)malloc(strlen(host_name)*2+1);
	xcddb_escape_string(escaped_host_name,host_name);
	escaped_author_name=(char *)malloc(strlen(author_name)*2+1);
	xcddb_escape_string(escaped_author_name,author_name);
	escaped_comment_data=(char *)malloc(strlen(comment_data)*2+1);
	xcddb_escape_string(escaped_comment_data,comment_data);

	/* construct the SQL statement */
#ifdef USE_XCDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,persistent,entry_time,author_name,comment_data) VALUES ('%s','%d',FROM_UNIXTIME(%lu),'%s','%s')",XCDDB_HOSTCOMMENTS_TABLE,escaped_host_name,persistent,entry_time,escaped_author_name,escaped_comment_data);
#endif
#ifdef USE_XCDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,persistent,entry_time,author_name,comment_data) VALUES ('%s','%d',%lu::abstime::timestamp,'%s','%s')",XCDDB_HOSTCOMMENTS_TABLE,escaped_host_name,persistent,entry_time,escaped_author_name,escaped_comment_data);
#endif
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* free memory for the escaped strings */
	free(escaped_host_name);
	free(escaped_author_name);
	free(escaped_comment_data);

	/* add the host comment */
	result=xcddb_query(sql_statement);

	/* there was an error adding the comment... */
	if(result==ERROR){

		xcddb_free_query_memory();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not insert record for host comment in table '%s' of database '%s'\n",XCDDB_HOSTCOMMENTS_TABLE,xcddb_database);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		/* return an invalid comment number */
		if(comment_id!=NULL)
			*comment_id=-1;

		/* disconnect from the database */
		xcddb_disconnect();

		return ERROR;
	        }

	/* pass back the id of the comment we just added */
	if(comment_id!=NULL){
#ifdef USE_XCDMYSQL
		*comment_id=(int)mysql_insert_id(&xcddb_mysql);
#endif
#ifdef USE_XCDPGSQL
		snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT hostcomment_id FROM %s WHERE oid='%s'",XCDDB_HOSTCOMMENTS_TABLE,PQoidStatus(xcddb_pgres));
		sql_statement[sizeof(sql_statement)-1]='\x0';
		xcddb_free_query_memory();
		result=xcddb_query(sql_statement);
		if(result==ERROR)
			*comment_id=-1;
		else{
			if(PQntuples(xcddb_pgres)<1)
				*comment_id=-1;
			else
				*comment_id=atoi(PQgetvalue(xcddb_pgres,0,0));
		        }
#endif
	        }

	xcddb_free_query_memory();

	/* disconnect from the database */
	xcddb_disconnect();

#ifdef DEBUG0
	printf("xcddb_save_host_comment() end\n");
#endif

	return OK;
        }


/* saves a new service comment */
int xcddb_save_service_comment(char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int *comment_id){
	char sql_statement[XCDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;
	char *escaped_host_name;
	char *escaped_svc_description;
	char *escaped_author_name;
	char *escaped_comment_data;

#ifdef DEBUG0
	printf("xcddb_save_service_comment() start\n");
#endif

	/* connect to the database */
	if(xcddb_connect()==ERROR)
		return ERROR;

	/* escape the strings, as they may have quotes, etc... */
	escaped_host_name=(char *)malloc(strlen(host_name)*2+1);
	xcddb_escape_string(escaped_host_name,host_name);
	escaped_svc_description=(char *)malloc(strlen(svc_description)*2+1);
	xcddb_escape_string(escaped_svc_description,svc_description);
	escaped_author_name=(char *)malloc(strlen(author_name)*2+1);
	xcddb_escape_string(escaped_author_name,author_name);
	escaped_comment_data=(char *)malloc(strlen(comment_data)*2+1);
	xcddb_escape_string(escaped_comment_data,comment_data);

	/* construct the SQL statement */
#ifdef USE_XCDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,service_description,persistent,entry_time,author_name,comment_data) VALUES ('%s','%s','%d',FROM_UNIXTIME(%lu),'%s','%s')",XCDDB_SERVICECOMMENTS_TABLE,escaped_host_name,escaped_svc_description,persistent,entry_time,escaped_author_name,escaped_comment_data);
#endif
#ifdef USE_XCDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,service_description,persistent,entry_time,author_name,comment_data) VALUES ('%s','%s','%d',%lu::abstime::timestamp,'%s','%s')",XCDDB_SERVICECOMMENTS_TABLE,escaped_host_name,escaped_svc_description,persistent,entry_time,escaped_author_name,escaped_comment_data);
#endif
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* free memory for the escaped strings */
	free(escaped_host_name);
	free(escaped_svc_description);
	free(escaped_author_name);
	free(escaped_comment_data);

	/* add the service comment */
	result=xcddb_query(sql_statement);

	/* there was an error adding the comment... */
	if(result==ERROR){

		xcddb_free_query_memory();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not insert record for service comment in table '%s' of database '%s'\n",XCDDB_SERVICECOMMENTS_TABLE,xcddb_database);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		/* return an invalid comment number */
		if(comment_id!=NULL)
			*comment_id=-1;

		/* disconnect from the database */
		xcddb_disconnect();

		return ERROR;
	        }

	/* pass back the id of the comment we just added */
	if(comment_id!=NULL){
#ifdef USE_XCDMYSQL
		*comment_id=(int)mysql_insert_id(&xcddb_mysql);
#endif
#ifdef USE_XCDPGSQL
		snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT servicecomment_id FROM %s WHERE oid='%s'",XCDDB_SERVICECOMMENTS_TABLE,PQoidStatus(xcddb_pgres));
		sql_statement[sizeof(sql_statement)-1]='\x0';
		xcddb_free_query_memory();
		result=xcddb_query(sql_statement);
		if(result==ERROR)
			*comment_id=-1;
		else{
			if(PQntuples(xcddb_pgres)<1)
				*comment_id=-1;
			else
				*comment_id=atoi(PQgetvalue(xcddb_pgres,0,0));
		        }
#endif
	        }

	xcddb_free_query_memory();

	/* disconnect from the database */
	xcddb_disconnect();

#ifdef DEBUG0
	printf("xcddb_save_service_comment() end\n");
#endif

	return OK;
        }



/******************************************************************/
/**************** COMMENT DELETION FUNCTIONS **********************/
/******************************************************************/


/* deletes a host comment */
int xcddb_delete_host_comment(int comment_id, int bulk_operation){
	char sql_statement[XCDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;

#ifdef DEBUG0
	printf("xcddb_delete_host_comment() start\n");
#endif

	/* connect to the database is necessary */
	if(bulk_operation==FALSE){
		if(xcddb_connect()==ERROR)
			return ERROR;
	        }

	/* delete a specific service comment */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s WHERE hostcomment_id='%d'",XCDDB_HOSTCOMMENTS_TABLE,comment_id);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xcddb_query(sql_statement);

#ifdef USE_XCDPGSQL
	/* free memory used to store query results */
	PQclear(xcddb_pgres);
#endif

	/* there was an error */
	if(result==ERROR){

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not delete host comment #%d from table '%s'\n",comment_id,XCDDB_HOSTCOMMENTS_TABLE);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* disconnect from the database is necessary */
	if(bulk_operation==FALSE)
		xcddb_disconnect();

#ifdef DEBUG0
	printf("xcddb_delete_host_comment() end\n");
#endif

	return OK;
        }


/* deletes a service comment */
int xcddb_delete_service_comment(int comment_id, int bulk_operation){
	char sql_statement[XCDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;

#ifdef DEBUG0
	printf("xcddb_delete_service_comment() start\n");
#endif

	/* connect to the database is necessary */
	if(bulk_operation==FALSE){
		if(xcddb_connect()==ERROR)
			return ERROR;
	        }

	/* delete a specific service comment */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s WHERE servicecomment_id='%d'",XCDDB_SERVICECOMMENTS_TABLE,comment_id);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xcddb_query(sql_statement);

#ifdef USE_XCDPGSQL
	/* free memory used to store query results */
	PQclear(xcddb_pgres);
#endif

	/* there was an error deleting the comment... */
	if(result==ERROR){

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not delete service comment #%d from table '%s'\n",comment_id,XCDDB_SERVICECOMMENTS_TABLE);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* disconnect from the database is necessary */
	if(bulk_operation==FALSE)
		xcddb_disconnect();

#ifdef DEBUG0
	printf("xcddb_delete_service_comment() end\n");
#endif

	return OK;
        }


/* deletes all comments for a particular host */
int xcddb_delete_all_host_comments(char *host_name){
	char sql_statement[XCDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;

	/* connect to the database */
	if(xcddb_connect()==ERROR)
		return ERROR;

	/* delete all comments for the specified host */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s WHERE host_name='%s'",XCDDB_HOSTCOMMENTS_TABLE,host_name);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xcddb_query(sql_statement);

#ifdef USE_XCDPGSQL
	/* free memory used to store query results */
	PQclear(xcddb_pgres);
#endif

	/* there was an error deleting the comments... */
	if(result==ERROR){

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not delete all comments for host '%s' from table '%s'\n",host_name,XCDDB_HOSTCOMMENTS_TABLE);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* disconnect from database */
	xcddb_disconnect();

	return OK;
        }


/* deletes all comments for a particular service */
int xcddb_delete_all_service_comments(char *host_name, char *svc_description){
	char sql_statement[XCDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;

	/* connect to the database */
	if(xcddb_connect()==ERROR)
		return ERROR;

	/* delete all comments for the specified service */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s WHERE host_name='%s' AND service_description='%s'",XCDDB_SERVICECOMMENTS_TABLE,host_name,svc_description);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xcddb_query(sql_statement);

#ifdef USE_XCDPGSQL
	/* free memory used to store query results */
	PQclear(xcddb_pgres);
#endif

	/* there was an error deleting the comments... */
	if(result==ERROR){

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not delete all comments for service '%s' on host '%s' from table '%s'\n",svc_description,host_name,XCDDB_HOSTCOMMENTS_TABLE);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* disconnect from database */
	xcddb_disconnect();

	return OK;
        }

#endif





#ifdef NSCGI

/******************************************************************/
/****************** COMMENT INPUT FUNCTIONS ***********************/
/******************************************************************/


/* read the comment file */
int xcddb_read_comment_data(char *config_file){

	/* grab configuration information */
	if(xcddb_grab_config_info(config_file)==ERROR)
		return ERROR;

	/* initialize database structures */
	if(xcddb_initialize()==ERROR)
		return ERROR;

	/* establish a connection to the database server */
	if(xcddb_connect()==ERROR)
		return ERROR;

	/* read in host comments... */
	xcddb_read_host_comments();

	/* read in service comments... */
	xcddb_read_service_comments();

	/* close database connection */
	xcddb_disconnect();

	return OK;
        }


/* reads all host comments */
int xcddb_read_host_comments(void){
	char sql_statement[XCDDB_SQL_LENGTH];
	int x;
	time_t entry_time;
	int persistent;
	int hostcomment_id;
#ifdef USE_XCDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XCDPGSQL
	int ntuples;
	int tuple;
#endif


	/* construct the SQL statement */
#ifdef USE_XCDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT hostcomment_id, host_name, UNIX_TIMESTAMP(entry_time) AS entry_time, persistent, author_name, comment_data FROM %s",XCDDB_HOSTCOMMENTS_TABLE);
#endif
#ifdef USE_XCDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT hostcomment_id, host_name, date_part('epoch',entry_time), persistent, author_name, comment_data FROM %s",XCDDB_HOSTCOMMENTS_TABLE);
#endif
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xcddb_query(sql_statement)==ERROR)
		return ERROR;


#ifdef USE_XCDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xcddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* we have six rows - make sure they're not NULL */
		for(x=0;x<6;x++){
			if(result_row[x]==NULL){
				mysql_free_result(query_result);
				return ERROR;
			        }
	                }

		/* get the comment id */
		hostcomment_id=atoi(result_row[0]);

		/* entry time */
		entry_time=(time_t)strtoul(result_row[2],NULL,10);

		/* persistent flag */
		persistent=atoi(result_row[3]);

		/* add the host comment */
		add_host_comment(result_row[1],entry_time,result_row[4],result_row[5],hostcomment_id,persistent);
	        }

	/* free memory used to store query results */
	mysql_free_result(query_result);
#endif

#ifdef USE_XCDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xcddb_pgres);

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* comment id */
		hostcomment_id=atoi(PQgetvalue(xcddb_pgres,tuple,0));
		
		/* entry time */
		entry_time=(time_t)strtoul(PQgetvalue(xcddb_pgres,tuple,2),NULL,10);
		
		/* persistent flag */
		persistent=atoi(PQgetvalue(xcddb_pgres,tuple,3));

		/* add the host comment */
		add_host_comment(PQgetvalue(xcddb_pgres,tuple,1),entry_time,PQgetvalue(xcddb_pgres,tuple,4),PQgetvalue(xcddb_pgres,tuple,5),hostcomment_id,persistent);
	        }

	/* free memory used to store query results */
	PQclear(xcddb_pgres);
#endif

	return OK;
        }


/* reads all service comments */
int xcddb_read_service_comments(void){
	char sql_statement[XCDDB_SQL_LENGTH];
	int x;
	time_t entry_time;
	int persistent;
	int servicecomment_id;
#ifdef USE_XCDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XCDPGSQL
	int ntuples;
	int tuple;
#endif


	/* construct the SQL statement */
#ifdef USE_XCDMYSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT servicecomment_id, host_name, service_description, UNIX_TIMESTAMP(entry_time) AS entry_time, persistent, author_name, comment_data FROM %s",XCDDB_SERVICECOMMENTS_TABLE);
#endif
#ifdef USE_XCDPGSQL
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT servicecomment_id, host_name, service_description, date_part('epoch',entry_time), persistent, author_name, comment_data FROM %s",XCDDB_SERVICECOMMENTS_TABLE);
#endif
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xcddb_query(sql_statement)==ERROR)
		return ERROR;


#ifdef USE_XCDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xcddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* we have six rows - make sure they're not NULL */
		for(x=0;x<7;x++){
			if(result_row[x]==NULL){
				mysql_free_result(query_result);
				return ERROR;
			        }
	                }

		/* get the comment id */
		servicecomment_id=atoi(result_row[0]);

		/* entry time */
		entry_time=(time_t)strtoul(result_row[3],NULL,10);

		/* persistent flag */
		persistent=atoi(result_row[4]);

		/* add the service comment */
		add_service_comment(result_row[1],result_row[2],entry_time,result_row[5],result_row[6],servicecomment_id,persistent);
	        }

	/* free memory used to store query results */
	mysql_free_result(query_result);
#endif

#ifdef USE_XCDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xcddb_pgres);

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* comment id */
		servicecomment_id=atoi(PQgetvalue(xcddb_pgres,tuple,0));
		
		/* entry time */
		entry_time=(time_t)strtoul(PQgetvalue(xcddb_pgres,tuple,3),NULL,10);
		
		/* persistent flag */
		persistent=atoi(PQgetvalue(xcddb_pgres,tuple,4));

		/* add the service comment */
		add_service_comment(PQgetvalue(xcddb_pgres,tuple,1),PQgetvalue(xcddb_pgres,tuple,2),entry_time,PQgetvalue(xcddb_pgres,tuple,5),PQgetvalue(xcddb_pgres,tuple,6),servicecomment_id,persistent);
	        }

	/* free memory used to store query results */
	PQclear(xcddb_pgres);
#endif

	return OK;
        }

#endif



/******************************************************************/
/*********************** MISC FUNCTIONS ***************************/
/******************************************************************/

#ifdef NSCORE

/* escape a string */
int xcddb_escape_string(char *escaped_string, char *raw_string){
	int x,y;

	if(raw_string==NULL || escaped_string==NULL){
		escaped_string=NULL;
		return ERROR;
	        }	  

	/* escape characters */
	for(x=0,y=0;x<strlen(raw_string);x++){
#ifdef USE_XCDMYSQL
		if(raw_string[x]=='\'' || raw_string[x]=='\"' || raw_string[x]=='*' || raw_string[x]=='\\' || raw_string[x]=='$' || raw_string[x]=='?' || raw_string[x]=='.' || raw_string[x]=='^' || raw_string[x]=='+' || raw_string[x]=='[' || raw_string[x]==']' || raw_string[x]=='(' || raw_string[x]==')')
		  escaped_string[y++]='\\';
#endif
#ifdef USE_XCDPGSQL
		if(! (isspace(raw_string[x]) || isalnum(raw_string[x]) || (raw_string[x]=='_')) )
			escaped_string[y++]='\\';
#endif
		escaped_string[y++]=raw_string[x];
	        }
	escaped_string[y]='\0';

	return OK;
	}

#endif

