/*****************************************************************************
 *
 * XPDFILE.C - File-based performance data routines
 *
 * Copyright (c) 2000-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   02-16-2003
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

#include "xpdfile.h"



char    *xpdfile_host_perfdata_template=NULL;
char    *xpdfile_service_perfdata_template=NULL;

char    *xpdfile_host_perfdata_file=NULL;
char    *xpdfile_service_perfdata_file=NULL;

FILE    *xpdfile_host_perfdata_fp=NULL;
FILE    *xpdfile_service_perfdata_fp=NULL;


extern char            *macro_host_state;
extern char	       *macro_service_state;
extern char            *macro_state_type;


/******************************************************************/
/************** INITIALIZATION & CLEANUP FUNCTIONS ****************/
/******************************************************************/

/* initializes performance data */
int xpdfile_initialize_performance_data(char *config_file){
	char buffer[MAX_INPUT_BUFFER];
	command *temp_command;

	/* default values */
	xpdfile_host_perfdata_template=NULL;
	xpdfile_service_perfdata_template=NULL;
	xpdfile_host_perfdata_file=NULL;
	xpdfile_service_perfdata_file=NULL;
	xpdfile_host_perfdata_fp=NULL;
	xpdfile_service_perfdata_fp=NULL;

	/* grab config info from main config file */
	xpdfile_grab_config_info(config_file);

	/* process special chars in templates */
	xpdfile_preprocess_delimiters(xpdfile_host_perfdata_template);
	xpdfile_preprocess_delimiters(xpdfile_service_perfdata_template);

	/* open the host performance data file for writing */
	if(xpdfile_host_perfdata_file!=NULL){

		xpdfile_host_perfdata_fp=fopen(xpdfile_host_perfdata_file,"a");

		if(xpdfile_host_perfdata_fp==NULL){
			snprintf(buffer,sizeof(buffer),"Warning: File '%s' could not be opened for writing - host performance data will not be processed!\n",xpdfile_host_perfdata_file);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);
		        }
	        }

	/* open the service performance data file for writing */
	if(xpdfile_service_perfdata_file!=NULL){

		xpdfile_service_perfdata_fp=fopen(xpdfile_service_perfdata_file,"a");

		if(xpdfile_service_perfdata_fp==NULL){
			snprintf(buffer,sizeof(buffer),"Warning: File '%s' could not be opened for writing - service performance data will not be processed!\n",xpdfile_service_perfdata_file);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);
		        }
	        }

	return OK;
        }


/* grabs configuration information from main config file */
int xpdfile_grab_config_info(char *config_file){
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

		if(!strcmp(variable,"xpdfile_host_perfdata_template")){
			xpdfile_host_perfdata_template=(char *)malloc(strlen(value)+1);
			if(xpdfile_host_perfdata_template==NULL){
				error=TRUE;
				break;
			        }

			strcpy(xpdfile_host_perfdata_template,value);
			strip(xpdfile_host_perfdata_template);
		        }
		else if(!strcmp(variable,"xpdfile_service_perfdata_template")){
			xpdfile_service_perfdata_template=(char *)malloc(strlen(value)+1);
			if(xpdfile_service_perfdata_template==NULL){
				error=TRUE;
				break;
			        }

			strcpy(xpdfile_service_perfdata_template,value);
			strip(xpdfile_service_perfdata_template);
		        }
		if(!strcmp(variable,"xpdfile_host_perfdata_file")){
			xpdfile_host_perfdata_file=(char *)malloc(strlen(value)+1);
			if(xpdfile_host_perfdata_file==NULL){
				error=TRUE;
				break;
			        }

			strcpy(xpdfile_host_perfdata_file,value);
			strip(xpdfile_host_perfdata_file);
		        }
		else if(!strcmp(variable,"xpdfile_service_perfdata_file")){
			xpdfile_service_perfdata_file=(char *)malloc(strlen(value)+1);
			if(xpdfile_service_perfdata_file==NULL){
				error=TRUE;
				break;
			        }

			strcpy(xpdfile_service_perfdata_file,value);
			strip(xpdfile_service_perfdata_file);
		        }
	        }

	fclose(fp);

	return OK;
        }



/* cleans up performance data */
int xpdfile_cleanup_performance_data(char *config_file){

	/* free memory */
	free(xpdfile_host_perfdata_template);
	free(xpdfile_service_perfdata_template);
	free(xpdfile_host_perfdata_file);
	free(xpdfile_service_perfdata_file);

	/* close the files */
	if(xpdfile_service_perfdata_fp!=NULL)
		fclose(xpdfile_service_perfdata_fp);
	if(xpdfile_host_perfdata_fp!=NULL)
		fclose(xpdfile_host_perfdata_fp);

	return OK;
        }


/* processes delimiter characters in template */
int xpdfile_preprocess_delimiters(char *template){
	char *tempbuf;
	int x=0;
	int y=0;

	if(template==NULL)
		return OK;

	/* allocate temporary buffer */
	tempbuf=(char *)malloc(strlen(template)+1);
	if(tempbuf==NULL)
		return ERROR;
	strcpy(tempbuf,"");

	for(x=0,y=0;x<strlen(template);x++,y++){
		if(template[x]=='\\'){
			if(template[x+1]=='t'){
				tempbuf[y]='\t';
				x++;
			        }
			else if(template[x+1]=='r'){
				tempbuf[y]='\r';
				x++;
			        }
			else if(template[x+1]=='n'){
				tempbuf[y]='\n';
				x++;
			        }
			else
				tempbuf[y]=template[x];
		        }
		else
			tempbuf[y]=template[x];
	        }
	tempbuf[y]='\x0';

	strcpy(template,tempbuf);
	free(tempbuf);

	return OK;
        }




/******************************************************************/
/****************** PERFORMANCE DATA FUNCTIONS ********************/
/******************************************************************/


/* updates service performance data */
int xpdfile_update_service_performance_data(service *svc){
	char raw_output[MAX_INPUT_BUFFER];
	char processed_output[MAX_INPUT_BUFFER];
	host *temp_host;
	int result=OK;

#ifdef DEBUG0
	printf("xpdfile_update_service_performance_data() start\n");
#endif

	/* we don't have a file to write to*/
	if(xpdfile_service_perfdata_fp==NULL)
		return OK;

	/* find the associated host */
	temp_host=find_host(svc->host_name);

	/* update service macros */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);

	/* get the raw line to write */
	strncpy(raw_output,xpdfile_service_perfdata_template,sizeof(raw_output));
	raw_output[sizeof(raw_output)-1]='\x0';

#ifdef DEBUG3
	printf("\tRaw service performance data output: %s\n",raw_output);
#endif

	/* process any macros in the raw output line */
	process_macros(raw_output,processed_output,(int)sizeof(processed_output),0);

#ifdef DEBUG3
	printf("\tProcessed service performance data output: %s\n",processed_output);
#endif

	/* write the processed output line containing performance data to the service perfdata file */
	fputs(processed_output,xpdfile_service_perfdata_fp);
	fputs("\n",xpdfile_service_perfdata_fp);
	fflush(xpdfile_service_perfdata_fp);

#ifdef DEBUG0
	printf("xpdfile_update_service_performance_data() end\n");
#endif

	return result;
        }


/* updates host performance data */
int xpdfile_update_host_performance_data(host *hst, int state){
	char raw_output[MAX_INPUT_BUFFER];
	char processed_output[MAX_INPUT_BUFFER];
	int result=OK;

#ifdef DEBUG0
	printf("xpdfile_update_host_performance_data() start\n");
#endif

	/* we don't have a host perfdata file */
	if(xpdfile_host_perfdata_fp==NULL)
		return OK;

	/* update host macros */
	clear_volatile_macros();
	grab_host_macros(hst);

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

	/* get the raw output */
	strncpy(raw_output,xpdfile_host_perfdata_template,sizeof(raw_output));
	raw_output[sizeof(raw_output)-1]='\x0';

#ifdef DEBUG3
	printf("\tRaw host performance output: %s\n",raw_output);
#endif

	/* process any macros in the raw output */
	process_macros(raw_output,processed_output,(int)sizeof(processed_output),0);

#ifdef DEBUG3
	printf("\tProcessed host performance data output: %s\n",processed_output);
#endif

	/* write the processed output line containing performance data to the host perfdata file */
	fputs(processed_output,xpdfile_host_perfdata_fp);
	fputs("\n",xpdfile_host_perfdata_fp);
	fflush(xpdfile_host_perfdata_fp);

#ifdef DEBUG0
	printf("xpdfile_update_host_performance_data() end\n");
#endif

	return result;
        }

