/*****************************************************************************
 *
 * XRDDB.C - Database routines for state retention data
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
#include "../base/nagios.h"
#include "../base/sretention.h"

#include "xrddb.h"


/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/

#ifdef USE_XRDMYSQL
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#endif

#ifdef USE_XRDPGSQL
#include <pgsql/libpq-fe.h>
#endif


char xrddb_host[XRDDB_BUFFER_LENGTH]="";
char xrddb_username[XRDDB_BUFFER_LENGTH]="";
char xrddb_password[XRDDB_BUFFER_LENGTH]="";
char xrddb_database[XRDDB_BUFFER_LENGTH]="";
int xrddb_port=3306;
int xrddb_use_transactions=FALSE;
int xrddb_optimize_data=TRUE;

#ifdef USE_XRDMYSQL
MYSQL xrddb_mysql;
#endif

#ifdef USE_XRDPGSQL
PGconn *xrddb_pgconn=NULL;
PGresult *xrddb_pgres=NULL;
#endif




/***********************************************************/
/***************** CONFIG INITIALIZATION  ******************/
/***********************************************************/

/* grab configuration information */
int xrddb_grab_config_info(char *main_config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	char *temp_buffer;
	FILE *fp;
	FILE *fp2;

#ifdef DEBUG0
	printf("xrddb_grab_config_info() start\n");
#endif

	/* DBMS-specifc defaults */
#ifdef USE_XRDMYSQL
	xrddb_port=3306;
#endif
#ifdef USE_XRDPGSQL
	xrddb_port=5432;
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

		/* core program reads retention data variables from the resource file */
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

				/* server name/address */
				if(strstr(input_buffer,"xrddb_host")==input_buffer){
					temp_buffer=strtok(input_buffer,"=");
					temp_buffer=strtok(NULL,"\n");
					if(temp_buffer==NULL)
						continue;
					strncpy(xrddb_host,temp_buffer,sizeof(xrddb_host)-1);
					xrddb_host[sizeof(xrddb_host)-1]='\x0';
				        }

				/* username */
				else if(strstr(input_buffer,"xrddb_username")==input_buffer){
					temp_buffer=strtok(input_buffer,"=");
					temp_buffer=strtok(NULL,"\n");
					if(temp_buffer==NULL)
						continue;
					strncpy(xrddb_username,temp_buffer,sizeof(xrddb_username)-1);
					xrddb_username[sizeof(xrddb_username)-1]='\x0';
				        }

				/* password */
				else if(strstr(input_buffer,"xrddb_password")==input_buffer){
					temp_buffer=strtok(input_buffer,"=");
					temp_buffer=strtok(NULL,"\n");
					if(temp_buffer==NULL)
						continue;
					strncpy(xrddb_password,temp_buffer,sizeof(xrddb_password)-1);
					xrddb_password[sizeof(xrddb_password)-1]='\x0';
				        }

				/* database */
				else if(strstr(input_buffer,"xrddb_database")==input_buffer){
					temp_buffer=strtok(input_buffer,"=");
					temp_buffer=strtok(NULL,"\n");
					if(temp_buffer==NULL)
						continue;
					strncpy(xrddb_database,temp_buffer,sizeof(xrddb_database)-1);
					xrddb_database[sizeof(xrddb_database)-1]='\x0';
				        }

				/* port */
				else if(strstr(input_buffer,"xrddb_port")==input_buffer){
					temp_buffer=strtok(input_buffer,"=");
					temp_buffer=strtok(NULL,"\n");
					if(temp_buffer==NULL)
						continue;
					xrddb_port=atoi(temp_buffer);
				        }

				/* optimize data option */
				else if(strstr(input_buffer,"xrddb_optimize_data")==input_buffer){
					temp_buffer=strtok(input_buffer,"=");
					temp_buffer=strtok(NULL,"\n");
					if(temp_buffer==NULL)
						continue;
					xrddb_optimize_data=(atoi(temp_buffer)>0)?TRUE:FALSE;
				        }
			        }

			fclose(fp2);
		        }
	        }

	fclose(fp);

#ifdef DEBUG0
	printf("xrddb_grab_config_info() end\n");
#endif
	return OK;
        }



/**********************************************************/
/***************** CONNECTION FUNCTIONS *******************/
/**********************************************************/

/* initializes any database structures we may need */
int xrddb_initialize(void){
	char buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("xrddb_initialize() start\n");
#endif

#ifdef USE_XRDMYSQL
	/* initialize mysql  */
	if(!mysql_init(&xrddb_mysql)){

		snprintf(buffer,sizeof(buffer)-1,"Error: mysql_init() failed for retention data\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	xrddb_use_transactions=FALSE;
#endif

#ifdef USE_XRDPGSQL
	xrddb_use_transactions=TRUE;
#endif

#ifdef DEBUG0
	printf("xrddb_initialize() end\n");
#endif

	return OK;
        }


/* opens a connection to the db server */
int xrddb_connect(void){
	char buffer[MAX_INPUT_BUFFER];
#ifdef USE_XRDPGSQL
	char connect_string[XRDDB_BUFFER_LENGTH];
#endif

#ifdef DEBUG0
	printf("xrddb_connect() start\n");
#endif

#ifdef USE_XRDMYSQL
	/* establish a connection to the MySQL server */
	if(!mysql_real_connect(&xrddb_mysql,xrddb_host,xrddb_username,xrddb_password,xrddb_database,xrddb_port,NULL,0)){

		mysql_close(&xrddb_mysql);

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not connect to MySQL database '%s' on host '%s' using username '%s' and password '%s'.  Retention data will not be processed or saved!\n",xrddb_database,xrddb_host,xrddb_username,xrddb_password);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }
#endif

#ifdef USE_XRDPGSQL
	/* establish a connection to the PostgreSQL server */
	snprintf(connect_string,sizeof(connect_string)-1,"host='%s' port=%d dbname='%s' user='%s' password='%s'",xrddb_host,xrddb_port,xrddb_database,xrddb_username,xrddb_password);
	connect_string[sizeof(connect_string)-1]='\x0';
	xrddb_pgconn=PQconnectdb(connect_string);

	if(PQstatus(xrddb_pgconn)==CONNECTION_BAD){

		PQfinish(xrddb_pgconn);

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not connect to PostgreSQL database '%s' on host '%s' using username '%s' 1and password '%s'.  Retention data will not be processed or saved!\n",xrddb_database,xrddb_host,xrddb_username,xrddb_password);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }
#endif

#ifdef DEBUG0
	printf("xrddb_connect() end\n");
#endif

	return OK;
        }


/* closes connection to db server */
int xrddb_disconnect(void){

#ifdef DEBUG0
	printf("xrddb_disconnect() start\n");
#endif

#ifdef USE_XRDMYSQL
	/* close database connection */
	mysql_close(&xrddb_mysql);
#endif

#ifdef USE_XRDPGSQL
	/* close database connection and cleanup */
	if(PQstatus(xrddb_pgconn)!=CONNECTION_BAD)
		PQfinish(xrddb_pgconn);
#endif

#ifdef DEBUG0
	printf("xrddb_disconnect() end\n");
#endif

	return OK;
        }


/* executes a SQL query */
int xrddb_query(char *sql_statement){
	int result=OK;

#ifdef DEBUG0
	printf("xrddb_query() start\n");
#endif

#ifdef DEBUG5
	printf("XRDDB_QUERY: %s\n",sql_statement);
#endif

#ifdef USE_XRDMYSQL
	if(mysql_query(&xrddb_mysql,sql_statement))
		result=ERROR;
#endif

#ifdef USE_XRDPGSQL
	xrddb_pgres=PQexec(xrddb_pgconn,sql_statement);
	if(xrddb_pgres==NULL || PQresultStatus(xrddb_pgres)==PGRES_FATAL_ERROR || PQresultStatus(xrddb_pgres)==PGRES_BAD_RESPONSE){
		PQclear(xrddb_pgres);
		result=ERROR;
	        }
#endif

#ifdef DEBUG5
	if(result==ERROR)
		printf("\tAn error occurred in the query!\n");
	printf("\tXRDDB_QUERY Done.\n");
#endif

#ifdef DEBUG0
	printf("xrddb_query() end\n");
#endif

	return result;
        }


/* frees memory associated with query results */
int xrddb_free_query_memory(void){

#ifdef DEBUG0
	printf("xrddb_free_query_memory() start\n");
#endif

#ifdef USE_XRDPGSQL
	PQclear(xrddb_pgres);
#endif
	
#ifdef DEBUG0
	printf("xrddb_free_query_memory() end\n");
#endif
	
	return OK;
        }



/**********************************************************/
/**************** TRANSACTIONS FUNCTIONS ******************/
/**********************************************************/

/* begins a transaction */
int xrddb_begin_transaction(void){

	if(xrddb_use_transactions==FALSE)
		return OK;

	xrddb_query("BEGIN TRANSACTION");
	xrddb_free_query_memory();

	return OK;
        }


/* cancels a transaction */
int xrddb_rollback_transaction(void){

	if(xrddb_use_transactions==FALSE)
		return OK;

	xrddb_query("ROLLBACK");
	xrddb_free_query_memory();

	return OK;
        }


/* commits a transaction */
int xrddb_commit_transaction(void){

	if(xrddb_use_transactions==FALSE)
		return OK;

	xrddb_query("COMMIT");
	xrddb_free_query_memory();

	return OK;
        }




/**********************************************************/
/**************** OPTIMIZATION FUNCTIONS ******************/
/**********************************************************/

/* optimizes data in retention tables */
int xrddb_optimize_tables(void){
#ifdef USE_XRDPGSQL
	char sql_statement[XRDDB_SQL_LENGTH];
#endif

#ifdef DEBUG0
	printf("xrddb_optimize_tables() start\n");
#endif

	/* don't optimize tables if user doesn't want us to */
	if(xrddb_optimize_data==FALSE)
		return OK;

#ifdef USE_XRDPGSQL
	
	/* VACUUM program, host, and service retention tables (user we're authenticated as must be owner of tables) */

	snprintf(sql_statement,sizeof(sql_statement)-1,"VACUUM %s",XRDDB_PROGRAMRETENTION_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	xrddb_query(sql_statement);
	xrddb_free_query_memory();

	snprintf(sql_statement,sizeof(sql_statement)-1,"VACUUM %s",XRDDB_HOSTRETENTION_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	xrddb_query(sql_statement);
	xrddb_free_query_memory();

	snprintf(sql_statement,sizeof(sql_statement)-1,"VACUUM %s",XRDDB_SERVICERETENTION_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	xrddb_query(sql_statement);
	xrddb_free_query_memory();

#endif

#ifdef DEBUG0
	printf("xrddb_optimize_tables() end\n");
#endif

	return OK;
        }




/**********************************************************/
/*********** RETENTION DATA OUTPUT FUNCTIONS **************/
/**********************************************************/

/* saves all retention data */
int xrddb_save_state_information(char *main_config_file){

#ifdef DEBUG0
	printf("xrddb_save_state_information() start\n");
#endif

	/* config data has already been read and database has been initialized already because we always READ state information first */

	/* establish a connection to the database server */
	if(xrddb_connect()==ERROR)
		return ERROR;

	/* save program retention data */
	xrddb_save_program_information();

	/* save host retention data */
	xrddb_save_host_information();

	/* save service retention data */
	xrddb_save_service_information();

	/* optimize data in retention tables */
	xrddb_optimize_tables();

	/* close database connection */
	xrddb_disconnect();

#ifdef DEBUG0
	printf("xrddb_save_state_information() end\n");
#endif

	return OK;
        }




/* save program retention data */
int xrddb_save_program_information(void){
	char sql_statement[XRDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;
	int _enable_notifications;
	int _execute_service_checks;
	int _accept_passive_service_checks;
	int _enable_event_handlers;
	int _obsess_over_services;
	int _enable_flap_detection;
	int _enable_failure_prediction;
	int _process_performance_data;


#ifdef DEBUG0
	printf("xrddb_save_program_information() start\n");
#endif

	/* get program state data to retain */
	get_program_state_information(&_enable_notifications,&_execute_service_checks,&_accept_passive_service_checks,&_enable_event_handlers,&_obsess_over_services,&_enable_flap_detection,&_enable_failure_prediction,&_process_performance_data);

	/***** BEGIN TRANSACTION *****/
	xrddb_begin_transaction();

	/* delete old program retention information */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s",XRDDB_PROGRAMRETENTION_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xrddb_query(sql_statement);
	xrddb_free_query_memory();

	/* construct the SQL statement */
	snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (enable_notifications,execute_service_checks,accept_passive_checks,enable_event_handlers,obsess_over_services,enable_flap_detection,enable_failure_prediction,process_performance_data) VALUES ('%d','%d','%d','%d','%d','%d','%d','%d')",XRDDB_PROGRAMRETENTION_TABLE,_enable_notifications,_execute_service_checks,_accept_passive_service_checks,_enable_event_handlers,_obsess_over_services,_enable_flap_detection,_enable_failure_prediction,_process_performance_data);

	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* insert the new program retention data */
	result=xrddb_query(sql_statement);
	xrddb_free_query_memory();

	/* there was an error inserting the new data... */
	if(result==ERROR){

		/***** ROLLBACK TRANSACTION *****/
		xrddb_rollback_transaction();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not insert row for program retention data in table '%s'\n",XRDDB_PROGRAMRETENTION_TABLE);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
		return ERROR;
	        }

	/***** COMMIT TRANSACTION *****/
	xrddb_commit_transaction();

#ifdef DEBUG0
	printf("xrddb_save_program_information() end\n");
#endif
	return OK;
        }


/* save host retention data */
int xrddb_save_host_information(void){
	char sql_statement[XRDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;
	host *temp_host=NULL;
	char *host_name;
	char *plugin_output;
	char *escaped_plugin_output;
	int state;
	unsigned long time_up;
	unsigned long time_down;
	unsigned long time_unreachable;
	unsigned long last_notification;
	unsigned long last_check;
	int notifications_enabled;
	int checks_enabled;
	int problem_has_been_acknowledged;
	int current_notification_number;
	int event_handler_enabled;
	int flap_detection_enabled;
	int failure_prediction_enabled;
	int process_performance_data;
	unsigned long last_state_change;


#ifdef DEBUG0
	printf("xrddb_save_host_information() start\n");
#endif

	/***** BEGIN TRANSACTION *****/
	xrddb_begin_transaction();

	/* delete old host retention information */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s",XRDDB_HOSTRETENTION_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xrddb_query(sql_statement);
	xrddb_free_query_memory();

	/* save retention information for all hosts... */
	temp_host=get_host_state_information(NULL,&host_name,&state,&plugin_output,&last_check,&checks_enabled,&time_up,&time_down,&time_unreachable,&last_notification,&current_notification_number,&notifications_enabled,&event_handler_enabled,&problem_has_been_acknowledged,&flap_detection_enabled,&failure_prediction_enabled,&process_performance_data,&last_state_change);
	while(temp_host!=NULL){

		/* escape the plugin output, as it may have quotes, etc... */
		escaped_plugin_output=(char *)malloc(strlen(plugin_output)*2+1);
		xrddb_escape_string(escaped_plugin_output,plugin_output);

		/* construct the SQL statement */
		snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,host_state,last_check,checks_enabled,time_up,time_down,time_unreachable,last_notification,current_notification,notifications_enabled,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,last_state_change,plugin_output) VALUES ('%s','%d','%lu','%d','%lu','%lu','%lu','%lu','%d','%d','%d','%d','%d','%d','%d','%lu','%s')",XRDDB_HOSTRETENTION_TABLE,host_name,state,last_check,checks_enabled,time_up,time_down,time_unreachable,last_notification,current_notification_number,notifications_enabled,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,last_state_change,escaped_plugin_output);
		sql_statement[sizeof(sql_statement)-1]='\x0';

		/* free memory for the escaped plugin string */
		free(escaped_plugin_output);

		/* insert the new program retention data */
		result=xrddb_query(sql_statement);
		xrddb_free_query_memory();

		/* there was an error inserting the new data... */
		if(result==ERROR){

			/***** ROLLBACK TRANSACTION *****/
			xrddb_rollback_transaction();

			snprintf(buffer,sizeof(buffer)-1,"Error: Could not insert retention data for host '%s' in table '%s'\n",host_name,XRDDB_HOSTRETENTION_TABLE);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
			continue;
		        }

		temp_host=get_host_state_information(temp_host,&host_name,&state,&plugin_output,&last_check,&checks_enabled,&time_up,&time_down,&time_unreachable,&last_notification,&current_notification_number,&notifications_enabled,&event_handler_enabled,&problem_has_been_acknowledged,&flap_detection_enabled,&failure_prediction_enabled,&process_performance_data,&last_state_change);
	        }

	/***** COMMIT TRANSACTION *****/
	xrddb_commit_transaction();

#ifdef DEBUG0
	printf("xrddb_save_host_information() end\n");
#endif
	return OK;
        }


/* save service retention data */
int xrddb_save_service_information(void){
	char sql_statement[XRDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;
	service *temp_service=NULL;
	char *host_name;
	char *service_description;
	char *plugin_output;
	char *escaped_plugin_output;
	int state;
	unsigned long time_ok;
	unsigned long time_warning;
	unsigned long time_unknown;
	unsigned long time_critical;
	unsigned long last_notification;
	unsigned long last_check;
	int check_type;
	int notifications_enabled;
	int checks_enabled;
	int accept_passive_checks;
	int problem_has_been_acknowledged;
	int current_notification_number;
	int event_handler_enabled;
	int flap_detection_enabled;
	int failure_prediction_enabled;
	int process_performance_data;
	int obsess_over_service;
	unsigned long last_state_change;


#ifdef DEBUG0
	printf("xrddb_save_service_information() start\n");
#endif

	/***** BEGIN TRANSACTION *****/
	xrddb_begin_transaction();

	/* delete old service retention information */
	snprintf(sql_statement,sizeof(sql_statement)-1,"DELETE FROM %s",XRDDB_SERVICERETENTION_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';
	result=xrddb_query(sql_statement);
	xrddb_free_query_memory();

	/* save retention information for all services... */
	temp_service=get_service_state_information(NULL,&host_name,&service_description,&state,&plugin_output,&last_check,&check_type,&time_ok,&time_warning,&time_unknown,&time_critical,&last_notification,&current_notification_number,&notifications_enabled,&checks_enabled,&accept_passive_checks,&event_handler_enabled,&problem_has_been_acknowledged,&flap_detection_enabled,&failure_prediction_enabled,&process_performance_data,&obsess_over_service,&last_state_change);
	while(temp_service!=NULL){


		/* escape the plugin output, as it may have quotes, etc... */
		escaped_plugin_output=(char *)malloc(strlen(plugin_output)*2+1);
		xrddb_escape_string(escaped_plugin_output,plugin_output);

		/* construct the SQL statement */
		snprintf(sql_statement,sizeof(sql_statement)-1,"INSERT INTO %s (host_name,service_description,service_state,last_check,check_type,time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification,notifications_enabled,checks_enabled,accept_passive_checks,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,obsess_over_service,last_state_change,plugin_output) VALUES ('%s','%s','%d','%lu','%d','%lu','%lu','%lu','%lu','%lu','%d','%d','%d','%d','%d','%d','%d','%d','%d','%d','%lu','%s')",XRDDB_SERVICERETENTION_TABLE,host_name,service_description,state,last_check,check_type,time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification_number,notifications_enabled,checks_enabled,accept_passive_checks,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,obsess_over_service,last_state_change,escaped_plugin_output);
		sql_statement[sizeof(sql_statement)-1]='\x0';

		/* free memory for the escaped plugin string */
		free(escaped_plugin_output);

		/* insert the new program retention data */
		result=xrddb_query(sql_statement);
		xrddb_free_query_memory();

		/* there was an error inserting the new data... */
		if(result==ERROR){

			/***** ROLLBACK TRANSACTION *****/
			xrddb_rollback_transaction();

			snprintf(buffer,sizeof(buffer)-1,"Error: Could not insert retention data for service '%s' on host '%s' in table '%s'\n",service_description,host_name,XRDDB_SERVICERETENTION_TABLE);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);
			continue;
		        }

		temp_service=get_service_state_information(temp_service,&host_name,&service_description,&state,&plugin_output,&last_check,&check_type,&time_ok,&time_warning,&time_unknown,&time_critical,&last_notification,&current_notification_number,&notifications_enabled,&checks_enabled,&accept_passive_checks,&event_handler_enabled,&problem_has_been_acknowledged,&flap_detection_enabled,&failure_prediction_enabled,&process_performance_data,&obsess_over_service,&last_state_change);
	        }

	/***** COMMIT TRANSACTION *****/
	xrddb_commit_transaction();

#ifdef DEBUG0
	printf("xrddb_save_service_information() end\n");
#endif

	return OK;
        }




/**********************************************************/
/************ RETENTION DATA INPUT FUNCTIONS **************/
/**********************************************************/

int xrddb_read_state_information(char *main_config_file){
	char buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("xrddb_read_state_information() start\n");
#endif

	/* grab configuration information */
	if(xrddb_grab_config_info(main_config_file)==ERROR){

		snprintf(buffer,sizeof(buffer)-1,"Error: Failed to grab database configuration information for retention data\n");
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

	/* initialize database structures */
	if(xrddb_initialize()==ERROR)
		return ERROR;

	/* establish a connection to the database server */
	if(xrddb_connect()==ERROR)
		return ERROR;

	/* read program retention data */
	xrddb_read_program_information();

	/* read host retention data */
	xrddb_read_host_information();

	/* read service retention data */
	xrddb_read_service_information();

	/* close database connection */
	xrddb_disconnect();


#ifdef DEBUG0
	printf("xrddb_read_state_information() end\n");
#endif

	return OK;
        }


/* read program retention data */
int xrddb_read_program_information(void){
	char sql_statement[XRDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int _enable_notifications;
	int _execute_service_checks;
	int _accept_passive_service_checks;
	int _enable_event_handlers;
	int _obsess_over_services;
	int _enable_flap_detection;
	int _enable_failure_prediction;
	int _process_performance_data;
#ifdef USE_XRDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif

#ifdef DEBUG0
	printf("xrddb_read_program_information() start\n");
#endif

	/* construct the SQL statement */
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT enable_notifications, execute_service_checks, accept_passive_checks, enable_event_handlers, obsess_over_services, enable_flap_detection, enable_failure_prediction, process_performance_data FROM %s",XRDDB_PROGRAMRETENTION_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xrddb_query(sql_statement)==ERROR){

		xrddb_free_query_memory();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not read program retention data from table %s\n",XRDDB_PROGRAMRETENTION_TABLE);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }


#ifdef USE_XRDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xrddb_mysql))==NULL)
		return ERROR;

	/* fetch first rows (we should only have one)... */
	if((result_row=mysql_fetch_row(query_result))==NULL)
		return ERROR;

	_enable_notifications=atoi(result_row[0]);
	_execute_service_checks=atoi(result_row[1]);
	_accept_passive_service_checks=atoi(result_row[2]);
	_enable_event_handlers=atoi(result_row[3]);
	_obsess_over_services=atoi(result_row[4]);
	_enable_flap_detection=atoi(result_row[5]);
	_enable_failure_prediction=atoi(result_row[6]);
	_process_performance_data=atoi(result_row[7]);

	/* free memory used to store query results */
	mysql_free_result(query_result);
#endif

#ifdef USE_XRDPGSQL

	/* make sure we have at least one row */
	if(PQntuples(xrddb_pgres)<1)
		return ERROR;

	_enable_notifications=atoi(PQgetvalue(xrddb_pgres,0,0));
	_execute_service_checks=atoi(PQgetvalue(xrddb_pgres,0,1));
	_accept_passive_service_checks=atoi(PQgetvalue(xrddb_pgres,0,2));
	_enable_event_handlers=atoi(PQgetvalue(xrddb_pgres,0,3));
	_obsess_over_services=atoi(PQgetvalue(xrddb_pgres,0,4));
	_enable_flap_detection=atoi(PQgetvalue(xrddb_pgres,0,5));
	_enable_failure_prediction=atoi(PQgetvalue(xrddb_pgres,0,6));
	_process_performance_data=atoi(PQgetvalue(xrddb_pgres,0,7));

	/* free memory used to store query results */
	PQclear(xrddb_pgres);
#endif

	/* set the program state information */
	set_program_state_information(_enable_notifications,_execute_service_checks,_accept_passive_service_checks,_enable_event_handlers,_obsess_over_services,_enable_flap_detection,_enable_failure_prediction,_process_performance_data);

#ifdef DEBUG0
	printf("xrddb_read_program_information() end\n");
#endif

	return OK;
        }


/* read host retention data */
int xrddb_read_host_information(void){
	char sql_statement[XRDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;
	int host_state;
	unsigned long last_check;
	int checks_enabled;
	unsigned long time_up;
	unsigned long time_down;
	unsigned long time_unreachable;
	unsigned long last_notification;
	int current_notification;
	int notifications_enabled;
	int event_handler_enabled;
	int problem_has_been_acknowledged;
	int flap_detection_enabled;
	int failure_prediction_enabled;
	int process_performance_data;
	unsigned long last_state_change;
#ifdef USE_XRDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XRDPGSQL
	int ntuples;
	int tuple;
#endif

#ifdef DEBUG0
	printf("xrddb_read_host_information() start\n");
#endif

	/* construct the SQL statement */
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT host_name, host_state, last_check, checks_enabled, time_up, time_down, time_unreachable, last_notification, current_notification, notifications_enabled, event_handler_enabled, problem_has_been_acknowledged, flap_detection_enabled, failure_prediction_enabled, process_performance_data, last_state_change, plugin_output FROM %s",XRDDB_HOSTRETENTION_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xrddb_query(sql_statement)==ERROR){

		xrddb_free_query_memory();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not read host retention data from table %s\n",XRDDB_HOSTRETENTION_TABLE);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

#ifdef USE_XRDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xrddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* current host state */
		host_state=atoi(result_row[1]);

		/* last check time */
		last_check=strtoul(result_row[2],NULL,10);

		/* are checks enabled? */
		checks_enabled=atoi(result_row[3]);

		/* time up, down, unreachable */
		time_up=strtoul(result_row[4],NULL,10);
		time_down=strtoul(result_row[5],NULL,10);
		time_unreachable=strtoul(result_row[6],NULL,10);

		/* last notification time */
		last_notification=strtoul(result_row[7],NULL,10);

		/* current notification number */
		current_notification=atoi(result_row[8]);

		/* are notifications enabled? */
		notifications_enabled=atoi(result_row[9]);

		/* is the event handler enabled? */
		event_handler_enabled=atoi(result_row[10]);

		/* has the problem been acknowledged? */
		problem_has_been_acknowledged=atoi(result_row[11]);

		/* is flap detection enabled? */
		flap_detection_enabled=atoi(result_row[12]);

		/* is failure prediction enabled? */
		failure_prediction_enabled=atoi(result_row[13]);

		/* performance data being processed? */
		process_performance_data=atoi(result_row[14]);

		/* last state change time */
		last_state_change=strtoul(result_row[15],NULL,10);

		/* set the host state */
		result=set_host_state_information(result_row[0],host_state,result_row[16],last_check,checks_enabled,time_up,time_down,time_unreachable,last_notification,current_notification,notifications_enabled,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,last_state_change);

	        }

	/* free memory used to store query results */
	mysql_free_result(query_result);
#endif

#ifdef USE_XRDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xrddb_pgres);

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* current host state */
		host_state=atoi(PQgetvalue(xrddb_pgres,tuple,1));

		/* last check time */
		last_check=strtoul(PQgetvalue(xrddb_pgres,tuple,2),NULL,10);

		/* are checks enabled? */
		checks_enabled=atoi(PQgetvalue(xrddb_pgres,tuple,3));

		/* time up, down, unreachable */
		time_up=strtoul(PQgetvalue(xrddb_pgres,tuple,4),NULL,10);
		time_down=strtoul(PQgetvalue(xrddb_pgres,tuple,5),NULL,10);
		time_unreachable=strtoul(PQgetvalue(xrddb_pgres,tuple,6),NULL,10);

		/* last notification time */
		last_notification=strtoul(PQgetvalue(xrddb_pgres,tuple,7),NULL,10);

		/* current notification number */
		current_notification=atoi(PQgetvalue(xrddb_pgres,tuple,8));

		/* are notifications enabled? */
		notifications_enabled=atoi(PQgetvalue(xrddb_pgres,tuple,9));

		/* is the event handler enabled? */
		event_handler_enabled=atoi(PQgetvalue(xrddb_pgres,tuple,10));

		/* has the problem been acknowledged? */
		problem_has_been_acknowledged=atoi(PQgetvalue(xrddb_pgres,tuple,11));

		/* is flap detection enabled? */
		flap_detection_enabled=atoi(PQgetvalue(xrddb_pgres,tuple,12));

		/* is failure prediction enabled? */
		failure_prediction_enabled=atoi(PQgetvalue(xrddb_pgres,tuple,13));

		/* is performance data being processed? */
		process_performance_data=atoi(PQgetvalue(xrddb_pgres,tuple,14));

		/* last state change time */
		last_state_change=strtoul(PQgetvalue(xrddb_pgres,tuple,15),NULL,10);

		/* set the host state */
		result=set_host_state_information(PQgetvalue(xrddb_pgres,tuple,0),host_state,PQgetvalue(xrddb_pgres,tuple,16),last_check,checks_enabled,time_up,time_down,time_unreachable,last_notification,current_notification,notifications_enabled,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,last_state_change);

	        }

	/* free memory used to store query results */
	PQclear(xrddb_pgres);
#endif

#ifdef DEBUG0
	printf("xrddb_read_host_information() end\n");
#endif

	return OK;
        }


/* read service retention data */
int xrddb_read_service_information(void){
	char sql_statement[XRDDB_SQL_LENGTH];
	char buffer[MAX_INPUT_BUFFER];
	int result;
	int service_state;
	unsigned long last_check;
	int check_type;
	unsigned long time_ok;
	unsigned long time_warning;
	unsigned long time_unknown;
	unsigned long time_critical;
	unsigned long last_notification;
	int current_notification;
	int notifications_enabled;
	int checks_enabled;
	int accept_passive_checks;
	int event_handler_enabled;
	int problem_has_been_acknowledged;
	int flap_detection_enabled;
	int failure_prediction_enabled;
	int process_performance_data;
	int obsess_over_service;
	unsigned long last_state_change;
#ifdef USE_XRDMYSQL
	MYSQL_RES *query_result=NULL;
	MYSQL_ROW result_row;
#endif
#ifdef USE_XRDPGSQL
	int ntuples;
	int tuple;
#endif


#ifdef DEBUG0
	printf("xrddb_read_service_information() start\n");
#endif

	/* construct the SQL statement */
	snprintf(sql_statement,sizeof(sql_statement)-1,"SELECT host_name, service_description, service_state, last_check, check_type, time_ok, time_warning, time_unknown, time_critical, last_notification, current_notification, notifications_enabled, checks_enabled, accept_passive_checks, event_handler_enabled, problem_has_been_acknowledged, flap_detection_enabled, failure_prediction_enabled, process_performance_data, obsess_over_service, last_state_change, plugin_output FROM %s",XRDDB_SERVICERETENTION_TABLE);
	sql_statement[sizeof(sql_statement)-1]='\x0';

	/* run the query... */
	if(xrddb_query(sql_statement)==ERROR){

		xrddb_free_query_memory();

		snprintf(buffer,sizeof(buffer)-1,"Error: Could not read service retention data from table %s\n",XRDDB_SERVICERETENTION_TABLE);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

		return ERROR;
	        }

#ifdef USE_XRDMYSQL
	/* get the results of the query */
	if((query_result=mysql_store_result(&xrddb_mysql))==NULL)
		return ERROR;

	/* fetch all rows... */
	while((result_row=mysql_fetch_row(query_result))!=NULL){

		/* current service state */
		service_state=atoi(result_row[2]);

		/* convert old STATE_UNKNOWN type */
		if(service_state==-1)
			service_state=STATE_UNKNOWN;

		/* last check time */
		last_check=strtoul(result_row[3],NULL,10);

		/* check type */
		check_type=atoi(result_row[4]);

		/* time ok, warning, unknown, critical */
		time_ok=strtoul(result_row[5],NULL,10);
		time_warning=strtoul(result_row[6],NULL,10);
		time_unknown=strtoul(result_row[7],NULL,10);
		time_critical=strtoul(result_row[8],NULL,10);

		/* last notification time */
		last_notification=strtoul(result_row[9],NULL,10);

		/* current notification number */
		current_notification=atoi(result_row[10]);

		/* are notifications enabled? */
		notifications_enabled=atoi(result_row[11]);

		/* are checks enabled? */
		checks_enabled=atoi(result_row[12]);

		/* are we accepting passive service checks? */
		accept_passive_checks=atoi(result_row[13]);

		/* is the event handler enabled? */
		event_handler_enabled=atoi(result_row[14]);

		/* has the problem been acknowledged? */
		problem_has_been_acknowledged=atoi(result_row[15]);

		/* is flap detection enabled? */
		flap_detection_enabled=atoi(result_row[16]);

		/* is failure prediction enabled? */
		failure_prediction_enabled=atoi(result_row[17]);

		/* is performance data being processed? */
		process_performance_data=atoi(result_row[18]);

		/* are we obsessing over this service? */
		obsess_over_service=atoi(result_row[19]);

		/* last state change time */
		last_state_change=strtoul(result_row[20],NULL,10);

		/* set the service state */
		result=set_service_state_information(result_row[0],result_row[1],service_state,result_row[21],last_check,check_type,time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification,notifications_enabled,checks_enabled,accept_passive_checks,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,obsess_over_service,last_state_change);
	        }

	/* free memory used to store query results */
	mysql_free_result(query_result);
#endif

#ifdef USE_XRDPGSQL

	/* how many rows do we have? */
	ntuples=PQntuples(xrddb_pgres);

	/* for each tuple... */
	for(tuple=0;tuple<ntuples;tuple++){

		/* current service state */
		service_state=atoi(PQgetvalue(xrddb_pgres,tuple,2));

		/* convert old STATE_UNKNOWN type */
		if(service_state==-1)
			service_state=STATE_UNKNOWN;

		/* last check time */
		last_check=strtoul(PQgetvalue(xrddb_pgres,tuple,3),NULL,10);

		/* check type */
		check_type=atoi(PQgetvalue(xrddb_pgres,tuple,4));

		/* time ok, warning, unknown, critical */
		time_ok=strtoul(PQgetvalue(xrddb_pgres,tuple,5),NULL,10);
		time_warning=strtoul(PQgetvalue(xrddb_pgres,tuple,6),NULL,10);
		time_unknown=strtoul(PQgetvalue(xrddb_pgres,tuple,7),NULL,10);
		time_critical=strtoul(PQgetvalue(xrddb_pgres,tuple,8),NULL,10);

		/* last notification time */
		last_notification=strtoul(PQgetvalue(xrddb_pgres,tuple,9),NULL,10);

		/* current notification number */
		current_notification=atoi(PQgetvalue(xrddb_pgres,tuple,10));

		/* are notifications enabled? */
		notifications_enabled=atoi(PQgetvalue(xrddb_pgres,tuple,11));

		/* are checks enabled? */
		checks_enabled=atoi(PQgetvalue(xrddb_pgres,tuple,12));

		/* are we accepting passive service checks? */
		accept_passive_checks=atoi(PQgetvalue(xrddb_pgres,tuple,13));

		/* is the event handler enabled? */
		event_handler_enabled=atoi(PQgetvalue(xrddb_pgres,tuple,14));

		/* has the problem been acknowledged? */
		problem_has_been_acknowledged=atoi(PQgetvalue(xrddb_pgres,tuple,15));

		/* is flap detection enabled? */
		flap_detection_enabled=atoi(PQgetvalue(xrddb_pgres,tuple,16));

		/* is failure prediction enabled? */
		failure_prediction_enabled=atoi(PQgetvalue(xrddb_pgres,tuple,17));

		/* is performance data being processed? */
		process_performance_data=atoi(PQgetvalue(xrddb_pgres,tuple,18));

		/* are we obsessing over this service? */
		obsess_over_service=atoi(PQgetvalue(xrddb_pgres,tuple,19));

		/* last state change time */
		last_state_change=strtoul(PQgetvalue(xrddb_pgres,tuple,20),NULL,10);

		/* set the service state */
		result=set_service_state_information(PQgetvalue(xrddb_pgres,tuple,0),PQgetvalue(xrddb_pgres,tuple,1),service_state,PQgetvalue(xrddb_pgres,tuple,21),last_check,check_type,time_ok,time_warning,time_unknown,time_critical,last_notification,current_notification,notifications_enabled,checks_enabled,accept_passive_checks,event_handler_enabled,problem_has_been_acknowledged,flap_detection_enabled,failure_prediction_enabled,process_performance_data,obsess_over_service,last_state_change);

	        }

	/* free memory used to store query results */
	PQclear(xrddb_pgres);
#endif

#ifdef DEBUG0
	printf("xrddb_read_service_information() end\n");
#endif

	return OK;
        }




/******************************************************************/
/*********************** MISC FUNCTIONS ***************************/
/******************************************************************/

/* escape a string */
int xrddb_escape_string(char *escaped_string, char *raw_string){
	int x,y;

	if(raw_string==NULL || escaped_string==NULL){
		escaped_string=NULL;
		return ERROR;
	        }	  

	/* escape characters */
	for(x=0,y=0;x<strlen(raw_string);x++){
#ifdef USE_XRDMYSQL
		if(raw_string[x]=='\'' || raw_string[x]=='\"' || raw_string[x]=='*' || raw_string[x]=='\\' || raw_string[x]=='$' || raw_string[x]=='?' || raw_string[x]=='.' || raw_string[x]=='^' || raw_string[x]=='+' || raw_string[x]=='[' || raw_string[x]==']' || raw_string[x]=='(' || raw_string[x]==')')
		  escaped_string[y++]='\\';
#endif
#ifdef USE_XRDPGSQL
		if(! (isspace(raw_string[x]) || isalnum(raw_string[x]) || (raw_string[x]=='_')) )
			escaped_string[y++]='\\';
#endif
		escaped_string[y++]=raw_string[x];
	        }
	escaped_string[y]='\0';

	return OK;
	}

