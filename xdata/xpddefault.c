/*****************************************************************************
 *
 * XPDDEFAULT.C - Default performance data routines
 *
 * Copyright (c) 2000-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-18-2002
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


/**** DATA INPUT-SPECIFIC HEADER FILES ****/

#include "xpddefault.h"



extern char            *macro_host_state;
extern char	       *macro_service_state;
extern char            *macro_state_type;

int                    xpddefault_perfdata_timeout;

char                   *xpddefault_host_perfdata_command=NULL;
char                   *xpddefault_service_perfdata_command=NULL;




/******************************************************************/
/************** INITIALIZATION & CLEANUP FUNCTIONS ****************/
/******************************************************************/

/* initializes performance data */
int xpddefault_initialize_performance_data(char *config_file){
	char buffer[MAX_INPUT_BUFFER];

	/* default values */
	xpddefault_perfdata_timeout=DEFAULT_PERFDATA_TIMEOUT;
	xpddefault_host_perfdata_command=NULL;
	xpddefault_service_perfdata_command=NULL;

	/* grab config info from main config file */
	xpddefault_grab_config_info(config_file);

	/* verify that performance data commands are valid */
	if(xpddefault_host_perfdata_command!=NULL){
		if(find_command(xpddefault_host_perfdata_command,NULL)==NULL){
			snprintf(buffer,sizeof(buffer),"Warning: Host performance command '%s' was not found - host performance data will not be processed!\n",xpddefault_host_perfdata_command);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);
			free(xpddefault_host_perfdata_command);
			xpddefault_host_perfdata_command=NULL;
		        }
	        }
	if(xpddefault_service_perfdata_command!=NULL){
		if(find_command(xpddefault_service_perfdata_command,NULL)==NULL){
			snprintf(buffer,sizeof(buffer),"Warning: Service performance command '%s' was not found - service performance data will not be processed!\n",xpddefault_service_perfdata_command);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);
			free(xpddefault_service_perfdata_command);
			xpddefault_service_perfdata_command=NULL;
		        }
	        }

	return OK;
        }


/* grabs configuration information from main config file */
int xpddefault_grab_config_info(char *config_file){
	char input_buffer[MAX_INPUT_BUFFER];
	char variable[MAX_INPUT_BUFFER];
	char value[MAX_INPUT_BUFFER];
	char *temp_ptr;
	FILE *fp;
	int error=FALSE;

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

		/* get the variable name */
		temp_ptr=my_strtok(input_buffer,"=");

		/* if there is no variable name, return error */
		if(temp_ptr==NULL){
			error=TRUE;
			break;
			}

		/* else the variable is good */
		strncpy(variable,temp_ptr,sizeof(variable));
		variable[sizeof(variable)-1]='\x0';

		/* get the value */
		temp_ptr=my_strtok(NULL,"=");

		/* if no value exists, return error */
		if(temp_ptr==NULL){
			error=TRUE;
			break;
			}

		/* else the value is good */
		strncpy(value,temp_ptr,sizeof(value));
		value[sizeof(value)-1]='\x0';
		strip(value);

		if(!strcmp(variable,"perfdata_timeout") || !strcmp(variable,"xpddefault_perfdata_timeout")){
			strip(value);
			xpddefault_perfdata_timeout=atoi(value);

			if(xpddefault_perfdata_timeout<=0){
				error=TRUE;
				break;
			        }
		        }
		else if(!strcmp(variable,"host_perfdata_command") || !strcmp(variable,"xpddefault_host_perfdata_command")){
			xpddefault_host_perfdata_command=(char *)malloc(strlen(value)+1);
			if(xpddefault_host_perfdata_command==NULL){
				error=TRUE;
				break;
			        }

			strcpy(xpddefault_host_perfdata_command,value);
			strip(xpddefault_host_perfdata_command);
		        }
		else if(!strcmp(variable,"service_perfdata_command") || !strcmp(variable,"xpddefault_service_perfdata_command")){
			xpddefault_service_perfdata_command=(char *)malloc(strlen(value)+1);
			if(xpddefault_service_perfdata_command==NULL){
				error=TRUE;
				break;
			        }

			strcpy(xpddefault_service_perfdata_command,value);
			strip(xpddefault_service_perfdata_command);
		        }
	        }

	fclose(fp);

	return OK;
        }


/* cleans up performance data */
int xpddefault_cleanup_performance_data(char *config_file){

	/* free memory */
	free(xpddefault_host_perfdata_command);
	free(xpddefault_service_perfdata_command);

	return OK;
        }




/******************************************************************/
/****************** PERFORMANCE DATA FUNCTIONS ********************/
/******************************************************************/


/* updates service performance data */
int xpddefault_update_service_performance_data(service *svc){
	char raw_command_line[MAX_INPUT_BUFFER];
	char processed_command_line[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	command *temp_command;
	host *temp_host;
	int early_timeout=FALSE;
	double exectime;
	int result=OK;

#ifdef DEBUG0
	printf("xpddefault_update_service_performance_data() start\n");
#endif

	/* we don't have a command */
	if(xpddefault_service_perfdata_command==NULL)
		return OK;

	/* find the associated host */
	temp_host=find_host(svc->host_name);

	/* update service macros */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);

	/* find the service performance data command */
	temp_command=find_command(xpddefault_service_perfdata_command,NULL);

	/* if there is no valid command, exit */
	if(temp_command==NULL)
		return ERROR;

	/* get the raw command line to execute */
	strncpy(raw_command_line,temp_command->command_line,sizeof(raw_command_line));
	raw_command_line[sizeof(raw_command_line)-1]='\x0';

#ifdef DEBUG3
	printf("\tRaw service performance data command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS);

#ifdef DEBUG3
	printf("\tProcessed service performance data command line: %s\n",processed_command_line);
#endif

	/* run the command */
	my_system(processed_command_line,xpddefault_perfdata_timeout,&early_timeout,&exectime,NULL,0);

	/* check to see if the command timed out */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Service performance data command '%s' for service '%s' on host '%s' timed out after %d seconds\n",xpddefault_service_perfdata_command,svc->description,svc->host_name,xpddefault_perfdata_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }

#ifdef DEBUG0
	printf("xpddefault_update_service_performance_data() end\n");
#endif

	return result;
        }


/* updates host performance data */
int xpddefault_update_host_performance_data(host *hst, int state, int state_type){
	char raw_command_line[MAX_INPUT_BUFFER];
	char processed_command_line[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	command *temp_command;
	int early_timeout=FALSE;
	double exectime;
	int result=OK;

#ifdef DEBUG0
	printf("xpddefault_update_host_performance_data() start\n");
#endif

	/* we don't have a command */
	if(xpddefault_host_perfdata_command==NULL)
		return OK;

	/* update host macros */
	clear_volatile_macros();
	grab_host_macros(hst);

	/* grab the host state type macro */
	if(macro_state_type!=NULL)
		free(macro_state_type);
	macro_state_type=(char *)malloc(MAX_STATETYPE_LENGTH);
	if(macro_state_type!=NULL)
		strcpy(macro_state_type,(state_type==HARD_STATE)?"HARD":"SOFT");

	/* make sure the host state macro is correct */
	if(macro_host_state!=NULL)
		free(macro_host_state);
	macro_host_state=(char *)malloc(MAX_STATE_LENGTH);
	if(macro_host_state!=NULL){
		if(state==HOST_DOWN)
			strcpy(macro_host_state,"DOWN");
		else if(state==HOST_UNREACHABLE)
			strcpy(macro_host_state,"UNREACHABLE");
		else
			strcpy(macro_host_state,"UP");
	        }

	/* find the host performance data command */
	temp_command=find_command(xpddefault_host_perfdata_command,NULL);

	/* if there is no valid command, exit */
	if(temp_command==NULL)
		return ERROR;

	/* get the raw command line to execute */
	strncpy(raw_command_line,temp_command->command_line,sizeof(raw_command_line));
	raw_command_line[sizeof(raw_command_line)-1]='\x0';

#ifdef DEBUG3
	printf("\tRaw host performance data command line: %s\n",raw_command_line);
#endif

	/* process any macros in the raw command line */
	process_macros(raw_command_line,processed_command_line,(int)sizeof(processed_command_line),STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS);

#ifdef DEBUG3
	printf("\tProcessed host performance data command line: %s\n",processed_command_line);
#endif

	/* run the command */
	my_system(processed_command_line,xpddefault_perfdata_timeout,&early_timeout,&exectime,NULL,0);

	/* check to see if the command timed out */
	if(early_timeout==TRUE){
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Host performance data command '%s' for host '%s' timed out after %d seconds\n",xpddefault_host_perfdata_command,hst->name,xpddefault_perfdata_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }

#ifdef DEBUG0
	printf("xpddefault_update_host_performance_data() end\n");
#endif

	return result;
        }
