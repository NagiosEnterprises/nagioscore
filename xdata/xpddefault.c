/*****************************************************************************
 *
 * XPDDEFAULT.C - Default performance data routines
 *
 * Copyright (c) 2000-2008 Ethan Galstad (egalstad@nagios.org)
 * Last Modified: 11-02-2008
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/macros.h"
#include "../include/nagios.h"


/**** DATA INPUT-SPECIFIC HEADER FILES ****/

#include "xpddefault.h"


int     xpddefault_perfdata_timeout;

char    *xpddefault_host_perfdata_command=NULL;
char    *xpddefault_service_perfdata_command=NULL;
command *xpddefault_host_perfdata_command_ptr=NULL;
command *xpddefault_service_perfdata_command_ptr=NULL;

char    *xpddefault_host_perfdata_file_template=NULL;
char    *xpddefault_service_perfdata_file_template=NULL;

char    *xpddefault_host_perfdata_file=NULL;
char    *xpddefault_service_perfdata_file=NULL;

int     xpddefault_host_perfdata_file_append=TRUE;
int     xpddefault_service_perfdata_file_append=TRUE;
int     xpddefault_host_perfdata_file_pipe=FALSE;
int     xpddefault_service_perfdata_file_pipe=FALSE;

unsigned long xpddefault_host_perfdata_file_processing_interval=0L;
unsigned long xpddefault_service_perfdata_file_processing_interval=0L;

char    *xpddefault_host_perfdata_file_processing_command=NULL;
char    *xpddefault_service_perfdata_file_processing_command=NULL;
command *xpddefault_host_perfdata_file_processing_command_ptr=NULL;
command *xpddefault_service_perfdata_file_processing_command_ptr=NULL;

FILE    *xpddefault_host_perfdata_fp=NULL;
FILE    *xpddefault_service_perfdata_fp=NULL;
int     xpddefault_host_perfdata_fd=-1;
int     xpddefault_service_perfdata_fd=-1;

extern char *macro_x[MACRO_X_COUNT];




/******************************************************************/
/***************** COMMON CONFIG INITIALIZATION  ******************/
/******************************************************************/

/* grabs configuration information from main config file */
int xpddefault_grab_config_info(char *config_file){
	char *input=NULL;
	mmapfile *thefile=NULL;

	/* open the config file for reading */
	if((thefile=mmap_fopen(config_file))==NULL){
		logit(NSLOG_CONFIG_ERROR,TRUE,"Error: Could not open main config file '%s' for reading performance variables!\n",config_file);
		return ERROR;
	        }

	/* read in all lines from the config file */
	while(1){

		/* free memory */
		my_free(input);

		/* read the next line */
		if((input=mmap_fgets_multiline(thefile))==NULL)
			break;

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]=='\x0')
			continue;

		strip(input);

		xpddefault_grab_config_directives(input);
	        }

	/* free memory and close the file */
	my_free(input);
	mmap_fclose(thefile);

	return OK;
        }


/* processes a single directive */
int xpddefault_grab_config_directives(char *input){
	char *temp_ptr=NULL;
	char *varname=NULL;
	char *varvalue=NULL;

	/* get the variable name */
	if((temp_ptr=my_strtok(input,"="))==NULL)
		return ERROR;
	if((varname=(char *)strdup(temp_ptr))==NULL)
		return ERROR;

	/* get the variable value */
	if((temp_ptr=my_strtok(NULL,"\n"))==NULL){
		my_free(varname);
		return ERROR;
	        }
	if((varvalue=(char *)strdup(temp_ptr))==NULL){
		my_free(varname);
		return ERROR;
	        }

	if(!strcmp(varname,"perfdata_timeout")){
		strip(varvalue);
		xpddefault_perfdata_timeout=atoi(varvalue);
	        }

	else if(!strcmp(varname,"host_perfdata_command"))
		xpddefault_host_perfdata_command=(char *)strdup(varvalue);

	else if(!strcmp(varname,"service_perfdata_command"))
		xpddefault_service_perfdata_command=(char *)strdup(varvalue);

	else if(!strcmp(varname,"host_perfdata_file_template"))
		xpddefault_host_perfdata_file_template=(char *)strdup(varvalue);

	else if(!strcmp(varname,"service_perfdata_file_template"))
		xpddefault_service_perfdata_file_template=(char *)strdup(varvalue);

	else if(!strcmp(varname,"host_perfdata_file"))
		xpddefault_host_perfdata_file=(char *)strdup(varvalue);

	else if(!strcmp(varname,"service_perfdata_file"))
		xpddefault_service_perfdata_file=(char *)strdup(varvalue);

	else if(!strcmp(varname,"host_perfdata_file_mode")){
		if(strstr(varvalue,"p")!=NULL)
			xpddefault_host_perfdata_file_pipe=TRUE;
		else if(strstr(varvalue,"w")!=NULL)
			xpddefault_host_perfdata_file_append=FALSE;
		else
			xpddefault_host_perfdata_file_append=TRUE;
	        }

	else if(!strcmp(varname,"service_perfdata_file_mode")){
		if(strstr(varvalue,"p")!=NULL)
			xpddefault_service_perfdata_file_pipe=TRUE;
		else if(strstr(varvalue,"w")!=NULL)
			xpddefault_service_perfdata_file_append=FALSE;
		else
			xpddefault_service_perfdata_file_append=TRUE;
	        }

	else if(!strcmp(varname,"host_perfdata_file_processing_interval"))
		xpddefault_host_perfdata_file_processing_interval=strtoul(varvalue,NULL,0);

	else if(!strcmp(varname,"service_perfdata_file_processing_interval"))
		xpddefault_service_perfdata_file_processing_interval=strtoul(varvalue,NULL,0);

	else if(!strcmp(varname,"host_perfdata_file_processing_command"))
		xpddefault_host_perfdata_file_processing_command=(char *)strdup(varvalue);

	else if(!strcmp(varname,"service_perfdata_file_processing_command"))
		xpddefault_service_perfdata_file_processing_command=(char *)strdup(varvalue);

	/* free memory */
	my_free(varname);
	my_free(varvalue);

	return OK;
        }



/******************************************************************/
/************** INITIALIZATION & CLEANUP FUNCTIONS ****************/
/******************************************************************/

/* initializes performance data */
int xpddefault_initialize_performance_data(char *config_file){
	char *buffer=NULL;
	char *temp_buffer=NULL;
	char *temp_command_name=NULL;
	command *temp_command=NULL;
	time_t current_time;

	time(&current_time);

	/* reset vars */
	xpddefault_host_perfdata_command_ptr=NULL;
	xpddefault_service_perfdata_command_ptr=NULL;
	xpddefault_host_perfdata_file_processing_command_ptr=NULL;
	xpddefault_service_perfdata_file_processing_command_ptr=NULL;

	/* grab config info from main config file */
	xpddefault_grab_config_info(config_file);

	/* make sure we have some templates defined */
	if(xpddefault_host_perfdata_file_template==NULL)
		xpddefault_host_perfdata_file_template=(char *)strdup(DEFAULT_HOST_PERFDATA_FILE_TEMPLATE);
	if(xpddefault_service_perfdata_file_template==NULL)
		xpddefault_service_perfdata_file_template=(char *)strdup(DEFAULT_SERVICE_PERFDATA_FILE_TEMPLATE);

	/* process special chars in templates */
	xpddefault_preprocess_file_templates(xpddefault_host_perfdata_file_template);
	xpddefault_preprocess_file_templates(xpddefault_service_perfdata_file_template);

	/* open the performance data files */
	xpddefault_open_host_perfdata_file();
	xpddefault_open_service_perfdata_file();

	/* verify that performance data commands are valid */
	if(xpddefault_host_perfdata_command!=NULL){

		temp_buffer=(char *)strdup(xpddefault_host_perfdata_command);

		/* get the command name, leave any arguments behind */
		temp_command_name=my_strtok(temp_buffer,"!");

		if((temp_command=find_command(temp_command_name))==NULL){

			logit(NSLOG_RUNTIME_WARNING,TRUE,"Warning: Host performance command '%s' was not found - host performance data will not be processed!\n",temp_command_name);

			my_free(xpddefault_host_perfdata_command);
		        }

		my_free(temp_buffer);

		/* save the command pointer for later */
		xpddefault_host_perfdata_command_ptr=temp_command;
	        }
	if(xpddefault_service_perfdata_command!=NULL){

		temp_buffer=(char *)strdup(xpddefault_service_perfdata_command);

		/* get the command name, leave any arguments behind */
		temp_command_name=my_strtok(temp_buffer,"!");

		if((temp_command=find_command(temp_command_name))==NULL){

			logit(NSLOG_RUNTIME_WARNING,TRUE,"Warning: Service performance command '%s' was not found - service performance data will not be processed!\n",temp_command_name);

			my_free(xpddefault_service_perfdata_command);
		        }

		/* free memory */
		my_free(temp_buffer);

		/* save the command pointer for later */
		xpddefault_service_perfdata_command_ptr=temp_command;
	        }
	if(xpddefault_host_perfdata_file_processing_command!=NULL){

		temp_buffer=(char *)strdup(xpddefault_host_perfdata_file_processing_command);

		/* get the command name, leave any arguments behind */
		temp_command_name=my_strtok(temp_buffer,"!");

		if((temp_command=find_command(temp_command_name))==NULL){

			logit(NSLOG_RUNTIME_WARNING,TRUE,"Warning: Host performance file processing command '%s' was not found - host performance data file will not be processed!\n",temp_command_name);

			my_free(xpddefault_host_perfdata_file_processing_command);
		        }

		/* free memory */
		my_free(temp_buffer);

		/* save the command pointer for later */
		xpddefault_host_perfdata_file_processing_command_ptr=temp_command;
	        }
	if(xpddefault_service_perfdata_file_processing_command!=NULL){

		temp_buffer=(char *)strdup(xpddefault_service_perfdata_file_processing_command);

		/* get the command name, leave any arguments behind */
		temp_command_name=my_strtok(temp_buffer,"!");

		if((temp_command=find_command(temp_command_name))==NULL){

			logit(NSLOG_RUNTIME_WARNING,TRUE,"Warning: Service performance file processing command '%s' was not found - service performance data file will not be processed!\n",temp_command_name);

			my_free(xpddefault_service_perfdata_file_processing_command);
		        }

		/* save the command pointer for later */
		xpddefault_service_perfdata_file_processing_command_ptr=temp_command;
	        }

	/* periodically process the host perfdata file */
	if(xpddefault_host_perfdata_file_processing_interval>0 && xpddefault_host_perfdata_file_processing_command!=NULL)
		schedule_new_event(EVENT_USER_FUNCTION,TRUE,current_time+xpddefault_host_perfdata_file_processing_interval,TRUE,xpddefault_host_perfdata_file_processing_interval,NULL,TRUE,(void *)xpddefault_process_host_perfdata_file,NULL,0);

	/* periodically process the service perfdata file */
	if(xpddefault_service_perfdata_file_processing_interval>0 && xpddefault_service_perfdata_file_processing_command!=NULL)
		schedule_new_event(EVENT_USER_FUNCTION,TRUE,current_time+xpddefault_service_perfdata_file_processing_interval,TRUE,xpddefault_service_perfdata_file_processing_interval,NULL,TRUE,(void *)xpddefault_process_service_perfdata_file,NULL,0);

	/* save the host perf data file macro */
	my_free(macro_x[MACRO_HOSTPERFDATAFILE]);
	if(xpddefault_host_perfdata_file!=NULL){
		if((macro_x[MACRO_HOSTPERFDATAFILE]=(char *)strdup(xpddefault_host_perfdata_file)))
			strip(macro_x[MACRO_HOSTPERFDATAFILE]);
	        }

	/* save the service perf data file macro */
	my_free(macro_x[MACRO_SERVICEPERFDATAFILE]);
	if(xpddefault_service_perfdata_file!=NULL){
		if((macro_x[MACRO_SERVICEPERFDATAFILE]=(char *)strdup(xpddefault_service_perfdata_file)))
			strip(macro_x[MACRO_SERVICEPERFDATAFILE]);
	        }

	/* free memory */
	my_free(temp_buffer);
	my_free(buffer);

	return OK;
        }



/* cleans up performance data */
int xpddefault_cleanup_performance_data(char *config_file){

	/* free memory */
	my_free(xpddefault_host_perfdata_command);
	my_free(xpddefault_service_perfdata_command);
	my_free(xpddefault_host_perfdata_file_template);
	my_free(xpddefault_service_perfdata_file_template);
	my_free(xpddefault_host_perfdata_file);
	my_free(xpddefault_service_perfdata_file);
	my_free(xpddefault_host_perfdata_file_processing_command);
	my_free(xpddefault_service_perfdata_file_processing_command);

	/* close the files */
	xpddefault_close_host_perfdata_file();
	xpddefault_close_service_perfdata_file();

	return OK;
        }



/******************************************************************/
/****************** PERFORMANCE DATA FUNCTIONS ********************/
/******************************************************************/


/* updates service performance data */
int xpddefault_update_service_performance_data(service *svc){

	/* run the performance data command */
	xpddefault_run_service_performance_data_command(svc);

	/* update the performance data file */
	xpddefault_update_service_performance_data_file(svc);

	return OK;
        }


/* updates host performance data */
int xpddefault_update_host_performance_data(host *hst){

	/* run the performance data command */
	xpddefault_run_host_performance_data_command(hst);

	/* update the performance data file */
	xpddefault_update_host_performance_data_file(hst);

	return OK;
        }




/******************************************************************/
/************** PERFORMANCE DATA COMMAND FUNCTIONS ****************/
/******************************************************************/


/* runs the service performance data command */
int xpddefault_run_service_performance_data_command(service *svc){
	char *raw_command_line=NULL;
	char *processed_command_line=NULL;
	host *temp_host;
	int early_timeout=FALSE;
	double exectime;
	int result=OK;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;

	log_debug_info(DEBUGL_FUNCTIONS,0,"run_service_performance_data_command()\n");

	if(svc==NULL)
		return ERROR;

	/* we don't have a command */
	if(xpddefault_service_perfdata_command==NULL)
		return OK;

	/* find the associated host */
	temp_host=find_host(svc->host_name);

	/* update service macros */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);

	/* get the raw command line */
	get_raw_command_line(xpddefault_service_perfdata_command_ptr,xpddefault_service_perfdata_command,&raw_command_line,macro_options);
	if(raw_command_line==NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA,2,"Raw service performance data command line: %s\n",raw_command_line);

	/* process any macros in the raw command line */
	process_macros(raw_command_line,&processed_command_line,macro_options);
	if(processed_command_line==NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA,2,"Processed service performance data command line: %s\n",processed_command_line);

	/* run the command */
	my_system(processed_command_line,xpddefault_perfdata_timeout,&early_timeout,&exectime,NULL,0);

	/* check to see if the command timed out */
	if(early_timeout==TRUE)
		logit(NSLOG_RUNTIME_WARNING,TRUE,"Warning: Service performance data command '%s' for service '%s' on host '%s' timed out after %d seconds\n",processed_command_line,svc->description,svc->host_name,xpddefault_perfdata_timeout);

	/* free memory */
	my_free(raw_command_line);
	my_free(processed_command_line);

	return result;
        }


/* runs the host performance data command */
int xpddefault_run_host_performance_data_command(host *hst){
	char *raw_command_line=NULL;
	char *processed_command_line=NULL;
	int early_timeout=FALSE;
	double exectime;
	int result=OK;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;

	log_debug_info(DEBUGL_FUNCTIONS,0,"run_host_performance_data_command()\n");

	if(hst==NULL)
		return ERROR;

	/* we don't have a command */
	if(xpddefault_host_perfdata_command==NULL)
		return OK;

	/* update host macros */
	clear_volatile_macros();
	grab_host_macros(hst);

	/* get the raw command line */
	get_raw_command_line(xpddefault_host_perfdata_command_ptr,xpddefault_host_perfdata_command,&raw_command_line,macro_options);
	if(raw_command_line==NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA,2,"Raw host performance data command line: %s\n",raw_command_line);

	/* process any macros in the raw command line */
	process_macros(raw_command_line,&processed_command_line,macro_options);

	log_debug_info(DEBUGL_PERFDATA,2,"Processed host performance data command line: %s\n",processed_command_line);

	/* run the command */
	my_system(processed_command_line,xpddefault_perfdata_timeout,&early_timeout,&exectime,NULL,0);
	if(processed_command_line==NULL)
		return ERROR;

	/* check to see if the command timed out */
	if(early_timeout==TRUE)
		logit(NSLOG_RUNTIME_WARNING,TRUE,"Warning: Host performance data command '%s' for host '%s' timed out after %d seconds\n",processed_command_line,hst->name,xpddefault_perfdata_timeout);

	/* free memory */
	my_free(raw_command_line);
	my_free(processed_command_line);

	return result;
        }



/******************************************************************/
/**************** FILE PERFORMANCE DATA FUNCTIONS *****************/
/******************************************************************/

/* open the host performance data file for writing */
int xpddefault_open_host_perfdata_file(void){

	if(xpddefault_host_perfdata_file!=NULL){

		if(xpddefault_host_perfdata_file_pipe==TRUE) {
			/* must open read-write to avoid failure if the other end isn't ready yet */
			xpddefault_host_perfdata_fd=open(xpddefault_host_perfdata_file,O_NONBLOCK | O_RDWR);
			xpddefault_host_perfdata_fp=fdopen(xpddefault_host_perfdata_fd,"w");
			}
		else
			xpddefault_host_perfdata_fp=fopen(xpddefault_host_perfdata_file,(xpddefault_host_perfdata_file_append==TRUE)?"a":"w");

		if(xpddefault_host_perfdata_fp==NULL){

			logit(NSLOG_RUNTIME_WARNING,TRUE,"Warning: File '%s' could not be opened - host performance data will not be written to file!\n",xpddefault_host_perfdata_file);

			return ERROR;
		        }
	        }

	return OK;
        }


/* open the service performance data file for writing */
int xpddefault_open_service_perfdata_file(void){

	if(xpddefault_service_perfdata_file!=NULL){
		if(xpddefault_service_perfdata_file_pipe==TRUE) {
			/* must open read-write to avoid failure if the other end isn't ready yet */
			xpddefault_service_perfdata_fd=open(xpddefault_service_perfdata_file,O_NONBLOCK | O_RDWR);
			xpddefault_service_perfdata_fp=fdopen(xpddefault_service_perfdata_fd,"w");
			}
		else
			xpddefault_service_perfdata_fp=fopen(xpddefault_service_perfdata_file,(xpddefault_service_perfdata_file_append==TRUE)?"a":"w");

		if(xpddefault_service_perfdata_fp==NULL){

			logit(NSLOG_RUNTIME_WARNING,TRUE,"Warning: File '%s' could not be opened - service performance data will not be written to file!\n",xpddefault_service_perfdata_file);

			return ERROR;
		        }
	        }

	return OK;
        }


/* close the host performance data file */
int xpddefault_close_host_perfdata_file(void){

	if(xpddefault_host_perfdata_fp!=NULL)
		fclose(xpddefault_host_perfdata_fp);
	if(xpddefault_host_perfdata_fd>0){
		close(xpddefault_host_perfdata_fd);
		xpddefault_host_perfdata_fd=-1;
		}

	return OK;
        }


/* close the service performance data file */
int xpddefault_close_service_perfdata_file(void){

	if(xpddefault_service_perfdata_fp!=NULL)
		fclose(xpddefault_service_perfdata_fp);
	if(xpddefault_service_perfdata_fd>0){
		close(xpddefault_service_perfdata_fd);
		xpddefault_service_perfdata_fd=-1;
		}

	return OK;
        }


/* processes delimiter characters in templates */
int xpddefault_preprocess_file_templates(char *template){
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
	my_free(tempbuf);

	return OK;
        }


/* updates service performance data file */
int xpddefault_update_service_performance_data_file(service *svc){
	char *raw_output=NULL;
	char *processed_output=NULL;
	host *temp_host=NULL;
	int result=OK;

	log_debug_info(DEBUGL_FUNCTIONS,0,"update_service_performance_data_file()\n");

	if(svc==NULL)
		return ERROR;

	/* we don't have a file to write to*/
	if(xpddefault_service_perfdata_fp==NULL || xpddefault_service_perfdata_file_template==NULL)
		return OK;

	/* find the associated host */
	temp_host=find_host(svc->host_name);

	/* update service macros */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);

	/* get the raw line to write */
	raw_output=(char *)strdup(xpddefault_service_perfdata_file_template);

	log_debug_info(DEBUGL_PERFDATA,2,"Raw service performance data file output: %s\n",raw_output);

	/* process any macros in the raw output line */
	process_macros(raw_output,&processed_output,0);
	if(processed_output==NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA,2,"Processed service performance data file output: %s\n",processed_output);

	/* write the processed output line containing performance data to the service perfdata file */
	fputs(processed_output,xpddefault_service_perfdata_fp);
	fputs("\n",xpddefault_service_perfdata_fp);
	fflush(xpddefault_service_perfdata_fp);

	/* free memory */
	my_free(raw_output);
	my_free(processed_output);

	return result;
        }


/* updates host performance data file */
int xpddefault_update_host_performance_data_file(host *hst){
	char *raw_output=NULL;
	char *processed_output=NULL;
	int result=OK;

	log_debug_info(DEBUGL_FUNCTIONS,0,"update_host_performance_data_file()\n");

	if(hst==NULL)
		return ERROR;

	/* we don't have a host perfdata file */
	if(xpddefault_host_perfdata_fp==NULL || xpddefault_host_perfdata_file_template==NULL)
		return OK;

	/* update host macros */
	clear_volatile_macros();
	grab_host_macros(hst);

	/* get the raw output */
	raw_output=(char *)strdup(xpddefault_host_perfdata_file_template);

	log_debug_info(DEBUGL_PERFDATA,2,"Raw host performance file output: %s\n",raw_output);

	/* process any macros in the raw output */
	process_macros(raw_output,&processed_output,0);
	if(processed_output==NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA,2,"Processed host performance data file output: %s\n",processed_output);

	/* write the processed output line containing performance data to the host perfdata file */
	fputs(processed_output,xpddefault_host_perfdata_fp);
	fputs("\n",xpddefault_host_perfdata_fp);
	fflush(xpddefault_host_perfdata_fp);

	/* free memory */
	my_free(raw_output);
	my_free(processed_output);

	return result;
        }


/* periodically process the host perf data file */
int xpddefault_process_host_perfdata_file(void){
	char *raw_command_line=NULL;
	char *processed_command_line=NULL;
	int early_timeout=FALSE;
	double exectime=0.0;
	int result=OK;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;


	log_debug_info(DEBUGL_FUNCTIONS,0,"process_host_perfdata_file()\n");

	/* we don't have a command */
	if(xpddefault_host_perfdata_file_processing_command==NULL)
		return OK;

	/* close the performance data file */
	xpddefault_close_host_perfdata_file();

	/* update macros */
	clear_volatile_macros();

	/* get the raw command line */
	get_raw_command_line(xpddefault_host_perfdata_file_processing_command_ptr,xpddefault_host_perfdata_file_processing_command,&raw_command_line,macro_options);
	if(raw_command_line==NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA,2,"Raw host performance data file processing command line: %s\n",raw_command_line);

	/* process any macros in the raw command line */
	process_macros(raw_command_line,&processed_command_line,macro_options);
	if(processed_command_line==NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA,2,"Processed host performance data file processing command line: %s\n",processed_command_line);

	/* run the command */
	my_system(processed_command_line,xpddefault_perfdata_timeout,&early_timeout,&exectime,NULL,0);

	/* check to see if the command timed out */
	if(early_timeout==TRUE)
		logit(NSLOG_RUNTIME_WARNING,TRUE,"Warning: Host performance data file processing command '%s' timed out after %d seconds\n",processed_command_line,xpddefault_perfdata_timeout);

	/* re-open the performance data file */
	xpddefault_open_host_perfdata_file();

	/* free memory */
	my_free(raw_command_line);
	my_free(processed_command_line);

	return result;
        }


/* periodically process the service perf data file */
int xpddefault_process_service_perfdata_file(void){
	char *raw_command_line=NULL;
	char *processed_command_line=NULL;
	int early_timeout=FALSE;
	double exectime=0.0;
	int result=OK;
	int macro_options=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;


	log_debug_info(DEBUGL_FUNCTIONS,0,"process_service_perfdata_file()\n");

	/* we don't have a command */
	if(xpddefault_service_perfdata_file_processing_command==NULL)
		return OK;

	/* close the performance data file */
	xpddefault_close_service_perfdata_file();

	/* update macros */
	clear_volatile_macros();

	/* get the raw command line */
	get_raw_command_line(xpddefault_service_perfdata_file_processing_command_ptr,xpddefault_service_perfdata_file_processing_command,&raw_command_line,macro_options);
	if(raw_command_line==NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA,2,"Raw service performance data file processing command line: %s\n",raw_command_line);

	/* process any macros in the raw command line */
	process_macros(raw_command_line,&processed_command_line,macro_options);
	if(processed_command_line==NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA,2,"Processed service performance data file processing command line: %s\n",processed_command_line);

	/* run the command */
	my_system(processed_command_line,xpddefault_perfdata_timeout,&early_timeout,&exectime,NULL,0);

	/* check to see if the command timed out */
	if(early_timeout==TRUE)
		logit(NSLOG_RUNTIME_WARNING,TRUE,"Warning: Service performance data file processing command '%s' timed out after %d seconds\n",processed_command_line,xpddefault_perfdata_timeout);

	/* re-open the performance data file */
	xpddefault_open_service_perfdata_file();

	/* free memory */
	my_free(raw_command_line);
	my_free(processed_command_line);

	return result;
        }

