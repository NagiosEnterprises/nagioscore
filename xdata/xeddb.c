/*****************************************************************************
 *
 * XEDDB.C - Database routines for extended data
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
#include "../common/objects.h"

#include "../cgi/cgiutils.h"
#include "../cgi/edata.h"

#include "xeddb.h"


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/

#ifdef USE_XEDMYSQL
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#endif

#ifdef USE_XEDPGSQL
#include <pgsql/libpq-fe.h>
#endif


char xeddb_host[XEDDB_BUFFER_LENGTH]="";
char xeddb_username[XEDDB_BUFFER_LENGTH]="";
char xeddb_password[XEDDB_BUFFER_LENGTH]="";
char xeddb_database[XEDDB_BUFFER_LENGTH]="";
int xeddb_port=3306;

#ifdef USE_XEDMYSQL
MYSQL xeddb_mysql;
#endif

#ifdef USE_XEDPGSQL
PGconn *xeddb_pgconn=NULL;
PGresult *xeddb_pgres=NULL;
#endif




/***********************************************************/
/***************** CONFIG INITIALIZATION  ******************/
/***********************************************************/

/* grab configuration information */
int xeddb_grab_config_info(char *main_config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	char *temp_buffer;
	FILE *fp;

	/* DBMS-specifc defaults */
#ifdef USE_XEDMYSQL
	xeddb_port=3306;
#endif
#ifdef USE_XEDPGSQL
	xeddb_port=5432;
#endif

	/* open the main config file for reading */
	fp=fopen(main_config_file,"r");
	if(fp==NULL)
		return ERROR;

	/* read in all lines from the main config file */
	for(fgets(input_buffer,sizeof(input_buffer)-1,fp);!feof(fp);fgets(input_buffer,sizeof(input_buffer)-1,fp)){

		/* skip blank lines and comments */
		if(input_buffer[0]=='#' || input_buffer[0]=='\x0' || input_buffer[0]=='\n' || input_buffer[0]=='\r')
			continue;

		strip(input_buffer);

		/* server name/address */
		if(strstr(input_buffer,"xeddb_host")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			strncpy(xeddb_host,temp_buffer,sizeof(xeddb_host)-1);
			xeddb_host[sizeof(xeddb_host)-1]='\x0';
		        }

		/* username */
		else if(strstr(input_buffer,"xeddb_username")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			strncpy(xeddb_username,temp_buffer,sizeof(xeddb_username)-1);
			xeddb_username[sizeof(xeddb_username)-1]='\x0';
		        }

		/* password */
		else if(strstr(input_buffer,"xeddb_password")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			strncpy(xeddb_password,temp_buffer,sizeof(xeddb_password)-1);
			xeddb_password[sizeof(xeddb_password)-1]='\x0';
		        }

		/* database */
		else if(strstr(input_buffer,"xeddb_database")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			strncpy(xeddb_database,temp_buffer,sizeof(xeddb_database)-1);
			xeddb_database[sizeof(xeddb_database)-1]='\x0';
		        }

		/* port */
		else if(strstr(input_buffer,"xeddb_port")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			xeddb_port=atoi(temp_buffer);
		        }

	        }

	fclose(fp);

	return OK;
        }



/**********************************************************/
/***************** CONNECTION FUNCTIONS *******************/
/**********************************************************/

/* initializes any database structures we may need */
int xeddb_initialize(void){

#ifdef USE_XEDMYSQL
	/* initialize mysql  */
	if(!mysql_init(&xeddb_mysql))
		return ERROR;
#endif

	return OK;
        }


/* opens a connection to the db server */
int xeddb_connect(void){
#ifdef USE_XEDPGSQL
	char connect_string[XEDDB_BUFFER_LENGTH];
#endif

#ifdef USE_XEDMYSQL
	/* establish a connection to the MySQL server */
	if(!mysql_real_connect(&xeddb_mysql,xeddb_host,xeddb_username,xeddb_password,xeddb_database,xeddb_port,NULL,0)){
		mysql_close(&xeddb_mysql);
		return ERROR;
	        }
#endif

#ifdef USE_XEDPGSQL
	/* establish a connection to the PostgreSQL server */
	snprintf(connect_string,sizeof(connect_string)-1,"host='%s' port=%d dbname='%s' user='%s' password='%s'",xeddb_host,xeddb_port,xeddb_database,xeddb_username,xeddb_password);
	connect_string[sizeof(connect_string)-1]='\x0';
	xeddb_pgconn=PQconnectdb(connect_string);

	if(PQstatus(xeddb_pgconn)==CONNECTION_BAD){
		PQfinish(xeddb_pgconn);
		return ERROR;
	        }
#endif

	return OK;
        }


/* closes connection to db server */
int xeddb_disconnect(void){

#ifdef USE_XEDMYSQL
	/* close database connection */
	mysql_close(&xeddb_mysql);
#endif

#ifdef USE_XEDPGSQL
	/* close database connection and cleanup */
	if(PQstatus(xeddb_pgconn)!=CONNECTION_BAD)
		PQfinish(xeddb_pgconn);
#endif

	return OK;
        }


/* executes a SQL query */
int xeddb_query(char *sql){
	int result=OK;

#ifdef USE_XEDMYSQL
	if(mysql_query(&xeddb_mysql,sql))
		result=ERROR;
#endif

#ifdef USE_XEDPGSQL
	xeddb_pgres=PQexec(xeddb_pgconn,sql);
	if(xeddb_pgres==NULL || PQresultStatus(xeddb_pgres)==PGRES_FATAL_ERROR || PQresultStatus(xeddb_pgres)==PGRES_BAD_RESPONSE){
		PQclear(xeddb_pgres);
		result=ERROR;
	        }
#endif

	return result;
        }




/**********************************************************/
/***************** DATA INPUT FUNCTIONS *******************/
/**********************************************************/


/* read extended host and service information */
int xeddb_read_extended_object_config_data(char *config_file, int options){

	/* grab configuration information from the CGI config file (nscgi.cfg) */
	if(xeddb_grab_config_info(config_file)==ERROR)
		return ERROR;

	/* initialize db structures */
	if(xeddb_initialize()==ERROR)
		return ERROR;

	/* connect to the database */
	if(xeddb_connect()==ERROR){
		xeddb_disconnect();
		return ERROR;
	        }

	/* read in extended host info */
	if(options & READ_EXTENDED_HOST_INFO)
		xeddb_read_extended_host_config_data();

	/* read in extended service info */
	if(options & READ_EXTENDED_SERVICE_INFO)
		xeddb_read_extended_service_config_data();

	/* disconnect from db server */
	xeddb_disconnect();

	return OK;
        }


/* read extended host information */
int xeddb_read_extended_host_config_data(void){
	char sql_statement[XEDDB_SQL_LENGTH];
	int x_2d,y_2d;
	double x_3d,y_3d,z_3d;
	int have_2d_coords, have_3d_coords;
#ifdef USE_XEDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XEDPGSQL
	int ntuples;
	int tuple;
#endif

	/* construct the SQL statement */
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT host_name, notes_url, icon_image, vrml_image, gd2_icon_image, icon_image_alt, x_2d, y_2d, x_3d, y_3d, z_3d, have_2d_coords, have_3d_coords FROM %s",XEDDB_HOSTEXTINFO_TABLE);

	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xeddb_query(sql_statement)==ERROR){
#ifdef USE_XEDPGSQL
		PQclear(xeddb_pgres);
#endif
		return ERROR;
	        }

#ifdef USE_XEDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xeddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* 2-D drawing coordinates */
		x_2d=(result_row[6]==NULL)?-1:atoi(result_row[6]);
		y_2d=(result_row[7]==NULL)?-1:atoi(result_row[7]);

		/* 3-D drawing coordinates */
		x_3d=(result_row[8]==NULL)?-1.0:strtod(result_row[8],NULL);
		y_3d=(result_row[9]==NULL)?-1.0:strtod(result_row[9],NULL);
		z_3d=(result_row[10]==NULL)?-1.0:strtod(result_row[10],NULL);

		if(result_row[11]==NULL)
			have_2d_coords=FALSE;
		else
			have_2d_coords=(atoi(result_row[11])>0)?TRUE:FALSE;
		if(result_row[12]==NULL)
			have_3d_coords=FALSE;
		else
			have_3d_coords=(atoi(result_row[12])>0)?TRUE:FALSE;

		/* add the extended host info */
		add_extended_host_info(result_row[0],result_row[1],result_row[2],result_row[3],result_row[4],result_row[5],x_2d,y_2d,x_3d,y_3d,z_3d,have_2d_coords,have_3d_coords);
	        }

	/* free memory used to store query results */
	mysql_free_result(query_result);
#endif

#ifdef USE_XEDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xeddb_pgres);

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){
		
		/* 2-D drawing coordinates */
		x_2d=atoi(PQgetvalue(xeddb_pgres,tuple,6));
		if(x_2d<=0)
			x_2d=-1;
		y_2d=atoi(PQgetvalue(xeddb_pgres,tuple,7));
		if(y_2d<=0)
			y_2d=-1;

		/* 3-D drawing coordinates */
		x_3d=atoi(PQgetvalue(xeddb_pgres,tuple,8));
		y_3d=atoi(PQgetvalue(xeddb_pgres,tuple,9));
		z_3d=atoi(PQgetvalue(xeddb_pgres,tuple,10));

		have_2d_coords=(atoi(PQgetvalue(xeddb_pgres,tuple,11))>0)?TRUE:FALSE;

		have_3d_coords=(atoi(PQgetvalue(xeddb_pgres,tuple,12))>0)?TRUE:FALSE;

		/* add the extended service info */
		add_extended_host_info(PQgetvalue(xeddb_pgres,tuple,0),PQgetvalue(xeddb_pgres,tuple,1),PQgetvalue(xeddb_pgres,tuple,2),PQgetvalue(xeddb_pgres,tuple,3),PQgetvalue(xeddb_pgres,tuple,4),PQgetvalue(xeddb_pgres,tuple,5),x_2d,y_2d,x_3d,y_3d,z_3d,have_2d_coords,have_3d_coords);
	        }

	/* free memory used to store query results */
	PQclear(xeddb_pgres);
#endif

	return OK;
        }


/* read extended service information */
int xeddb_read_extended_service_config_data(void){
	char sql_statement[XEDDB_SQL_LENGTH];
#ifdef USE_XEDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XEDPGSQL
	int ntuples;
	int tuple;
#endif

	/* construct the SQL statement */
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT host_name, service_description, notes_url, icon_image, icon_image_alt FROM %s",XEDDB_SERVICEEXTINFO_TABLE);

	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xeddb_query(sql_statement)==ERROR){
#ifdef USE_XEDPGSQL
		PQclear(xeddb_pgres);
#endif
		return ERROR;
	        }

#ifdef USE_XEDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xeddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* add the extended service info */
		add_extended_service_info(result_row[0],result_row[1],result_row[2],result_row[3],result_row[4]);
	        }

	/* free memory used to store query results */
	mysql_free_result(query_result);
#endif

#ifdef USE_XEDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xeddb_pgres);

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){
		
		/* add the extended service info */
		add_extended_service_info(PQgetvalue(xeddb_pgres,tuple,0),PQgetvalue(xeddb_pgres,tuple,1),PQgetvalue(xeddb_pgres,tuple,2),PQgetvalue(xeddb_pgres,tuple,3),PQgetvalue(xeddb_pgres,tuple,4));
	        }

	/* free memory used to store query results */
	PQclear(xeddb_pgres);
#endif

	return OK;
        }



