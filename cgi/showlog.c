/***********************************************************************
 *
 * SHOWLOG.C - Nagios Log File CGI
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 04-20-2002
 *
 * This CGI program will display the contents of the Nagios
 * log file.
 *
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
 ***********************************************************************/

#include "../common/config.h"
#include "../common/locations.h"
#include "../common/common.h"
#include "../common/objects.h"

#include "getcgi.h"
#include "cgiutils.h"
#include "auth.h"
#include "lifo.h"

extern char   main_config_file[MAX_FILENAME_LENGTH];
extern char   url_images_path[MAX_FILENAME_LENGTH];
extern char   url_stylesheets_path[MAX_FILENAME_LENGTH];

extern int    log_rotation_method;

void document_header(int);
void document_footer(void);
int process_cgivars(void);

authdata current_authdata;

int display_log(void);

char log_file_to_use[MAX_FILENAME_LENGTH]="";
int log_archive=0;

int use_lifo=TRUE;

int embedded=FALSE;
int display_header=TRUE;
int display_frills=TRUE;
int display_timebreaks=TRUE;


int main(void){
	int result=OK;
	char temp_buffer[MAX_INPUT_BUFFER];


	/* get the CGI variables passed in the URL */
	process_cgivars();

	/* reset internal variables */
	reset_cgi_vars();

	/* read the CGI configuration file */
	result=read_cgi_config_file(DEFAULT_CGI_CONFIG_FILE);
	if(result==ERROR){
		document_header(FALSE);
		cgi_config_file_error(DEFAULT_CGI_CONFIG_FILE);
		document_footer();
		return ERROR;
	        }

	/* read the main configuration file */
	result=read_main_config_file(main_config_file);
	if(result==ERROR){
		document_header(FALSE);
		main_config_file_error(main_config_file);
		document_footer();
		return ERROR;
	        }

	/* read all object configuration data */
	result=read_all_object_configuration_data(main_config_file,READ_ALL_OBJECT_DATA);
	if(result==ERROR){
		document_header(FALSE);
		object_data_error();
		document_footer();
		return ERROR;
                }


	document_header(TRUE);

	/* get authentication information */
	get_authentication_information(&current_authdata);

	/* determine what log file we should be using */
	get_log_archive_to_use(log_archive,log_file_to_use,(int)sizeof(log_file_to_use));

	if(display_header==TRUE){

		/* begin top table */
		printf("<table border=0 width=100%% cellpadding=0 cellspacing=0>\n");
		printf("<tr>\n");

		/* left column of top table - info box */
		printf("<td align=left valign=top width=33%%>\n");
		display_info_table((log_rotation_method==LOG_ROTATION_NONE || log_archive==0)?"Current Event Log":"Archived Event Log",FALSE,&current_authdata);
		printf("</td>\n");

		/* middle column of top table - log file navigation options */
		printf("<td align=center valign=top width=33%%>\n");
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"%s?%s",SHOWLOG_CGI,(use_lifo==FALSE)?"oldestfirst&":"");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		display_nav_table(temp_buffer,log_archive);
		printf("</td>\n");

		/* right hand column of top row */
		printf("<td align=right valign=top width=33%%>\n");

		printf("<table border=0 cellspacing=0 cellpadding=0 CLASS='optBox'>\n");
		printf("<form method='GET' action='%s'>\n",SHOWLOG_CGI);
		printf("<input type='hidden' name='archive' value='%d'>\n",log_archive);
		printf("<tr>");
		printf("<td align=left valign=bottom CLASS='optBoxItem'><input type='checkbox' name='oldestfirst' %s> Older Entries First:</td>",(use_lifo==FALSE)?"checked":"");
		printf("</tr>\n");
		printf("<tr>");
		printf("<td align=left valign=bottom CLASS='optBoxItem'><input type='submit' value='Update'></td>\n");
		printf("</tr>\n");

#ifdef CONTEXT_HELP
		printf("<tr>\n");
		printf("<td align=right>\n");
		display_context_help(CONTEXTHELP_LOG);
		printf("</td>\n");
		printf("</tr>\n");
#endif

		printf("</form>\n");
		printf("</table>\n");

		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");
		printf("</p>\n");

	        }


	/* display the contents of the log file */
	display_log();

	document_footer();

	/* free allocated memory */
	free_memory();
	
	return OK;
        }




void document_header(int use_stylesheet){
	char date_time[MAX_DATETIME_LENGTH];
	time_t current_time;
	time_t expire_time;

	printf("Cache-Control: no-store\n");
	printf("Pragma: no-cache\n");

	time(&current_time);
	get_time_string(&current_time,date_time,(int)sizeof(date_time),HTTP_DATE_TIME);
	printf("Last-Modified: %s\n",date_time);

	expire_time=(time_t)0L;
	get_time_string(&expire_time,date_time,(int)sizeof(date_time),HTTP_DATE_TIME);
	printf("Expires: %s\n",date_time);

	printf("Content-type: text/html\n\n");

	if(embedded==TRUE)
		return;

	printf("<HTML>\n");
	printf("<HEAD>\n");
	printf("<TITLE>\n");
	printf("Nagios Log File\n");
	printf("</TITLE>\n");

	if(use_stylesheet==TRUE)
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n",url_stylesheets_path,SHOWLOG_CSS);

	printf("</HEAD>\n");
	printf("<BODY CLASS='showlog'>\n");

	/* include user SSI header */
	include_ssi_files(SHOWLOG_CGI,SSI_HEADER);

	return;
        }


void document_footer(void){

	if(embedded==TRUE)
		return;

	/* include user SSI footer */
	include_ssi_files(SHOWLOG_CGI,SSI_FOOTER);

	printf("</BODY>\n");
	printf("</HTML>\n");

	return;
        }


int process_cgivars(void){
	char **variables;
	int error=FALSE;
	int x;

	variables=getcgivars();

	for(x=0;variables[x]!=NULL;x++){

		/* do some basic length checking on the variable identifier to prevent buffer overflows */
		if(strlen(variables[x])>=MAX_INPUT_BUFFER-1){
			continue;
		        }

		/* we found the archive argument */
		else if(!strcmp(variables[x],"archive")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			log_archive=atoi(variables[x]);
			if(log_archive<0)
				log_archive=0;
		        }

		/* we found the order argument */
		else if(!strcmp(variables[x],"oldestfirst")){
			use_lifo=FALSE;
		        }

		/* we found the embed option */
		else if(!strcmp(variables[x],"embedded"))
			embedded=TRUE;

		/* we found the noheader option */
		else if(!strcmp(variables[x],"noheader"))
			display_header=FALSE;

		/* we found the nofrills option */
		else if (!strcmp(variables[x],"nofrills"))
			display_frills=FALSE;

		/* we found the notimebreaks option */
		else if(!strcmp(variables[x],"notimebreaks"))
			display_timebreaks=FALSE;

		/* we recieved an invalid argument */
		else
			error=TRUE;
	
	        }

	return error;
        }



/* display the contents of the log file */
int display_log(void){
	char input_buffer[MAX_INPUT_BUFFER];
	char image[MAX_INPUT_BUFFER];
	char image_alt[MAX_INPUT_BUFFER];
	time_t t;
	char *temp_buffer;
	char date_time[MAX_DATETIME_LENGTH];
	int error;
	FILE *fp=NULL;
	char last_message_date[MAX_INPUT_BUFFER]="";
	char current_message_date[MAX_INPUT_BUFFER]="";
	struct tm *time_ptr;


	/* check to see if the user is authorized to view the log file */
	if(is_authorized_for_system_information(&current_authdata)==FALSE){
		printf("<HR>\n");
		printf("<DIV CLASS='errorMessage'>It appears as though you do not have permission to view the log file...</DIV><br><br>\n");
		printf("<DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>and check the authorization options in your CGI configuration file.</DIV>\n");
		printf("<HR>\n");
		return ERROR;
	        }

	error=FALSE;

	if(use_lifo==TRUE){
		error=read_file_into_lifo(log_file_to_use);
		if(error!=LIFO_OK){
			if(error==LIFO_ERROR_MEMORY){
				printf("<P><DIV CLASS='warningMessage'>Not enough memory to reverse log file - displaying log in natural order...</DIV></P>");
				error=FALSE;
			        }
			else
				error=TRUE;
			use_lifo=FALSE;
		        }
		else
			error=FALSE;
	        }

	if(use_lifo==FALSE){
  
		fp=fopen(log_file_to_use,"r");
		if(fp==NULL){
			printf("<HR>\n");
			printf("<P><DIV CLASS='errorMessage'>Error: Could not open log file '%s' for reading!</DIV></P>",log_file_to_use);
			printf("<HR>\n");
			error=TRUE;
	                }
	        }

	if(error==FALSE){

		printf("<P><DIV CLASS='logEntries'>\n");
		
		while(1){

			if(use_lifo==TRUE){
				if(pop_lifo(input_buffer,(int)sizeof(input_buffer))!=LIFO_OK)
					break;
			        }
			else{
				fgets(input_buffer,(int)(sizeof(input_buffer)-1),fp);
				if(feof(fp))
					break;
			        }

			if(strstr(input_buffer," starting...")){
				strcpy(image,START_ICON);
				strcpy(image_alt,START_ICON_ALT);
			        }
			else if(strstr(input_buffer," shutting down...")){
				strcpy(image,STOP_ICON);
				strcpy(image_alt,STOP_ICON_ALT);
			        }
			else if(strstr(input_buffer,"Bailing out")){
				strcpy(image,STOP_ICON);
				strcpy(image_alt,STOP_ICON_ALT);
			        }
			else if(strstr(input_buffer," restarting...")){
				strcpy(image,RESTART_ICON);
				strcpy(image_alt,RESTART_ICON_ALT);
			        }
			else if(strstr(input_buffer,"HOST ALERT:") && strstr(input_buffer,";DOWN;")){
				strcpy(image,HOST_DOWN_ICON);
				strcpy(image_alt,HOST_DOWN_ICON_ALT);
			        }
			else if(strstr(input_buffer,"HOST ALERT:") && strstr(input_buffer,";UNREACHABLE;")){
				strcpy(image,HOST_UNREACHABLE_ICON);
				strcpy(image_alt,HOST_UNREACHABLE_ICON_ALT);
			        }
			else if(strstr(input_buffer,"HOST ALERT:") && (strstr(input_buffer,";RECOVERY;") || strstr(input_buffer,";UP;"))){
				strcpy(image,HOST_UP_ICON);
				strcpy(image_alt,HOST_UP_ICON_ALT);
			        }
			else if(strstr(input_buffer,"HOST NOTIFICATION:")){
				strcpy(image,HOST_NOTIFICATION_ICON);
				strcpy(image_alt,HOST_NOTIFICATION_ICON_ALT);
			        }
			else if(strstr(input_buffer,"SERVICE ALERT:") && strstr(input_buffer,";CRITICAL;")){
				strcpy(image,CRITICAL_ICON);
				strcpy(image_alt,CRITICAL_ICON_ALT);
			        }
			else if(strstr(input_buffer,"SERVICE ALERT:") && strstr(input_buffer,";WARNING;")){
				strcpy(image,WARNING_ICON);
				strcpy(image_alt,WARNING_ICON_ALT);
			        }
			else if(strstr(input_buffer,"SERVICE ALERT:") && strstr(input_buffer,";UNKNOWN;")){
				strcpy(image,UNKNOWN_ICON);
				strcpy(image_alt,UNKNOWN_ICON_ALT);
			        }
			else if(strstr(input_buffer,"SERVICE ALERT:") && (strstr(input_buffer,";RECOVERY;") || strstr(input_buffer,";OK;"))){
				strcpy(image,OK_ICON);
				strcpy(image_alt,OK_ICON_ALT);
			        }
			else if(strstr(input_buffer,"SERVICE NOTIFICATION:")){
				strcpy(image,NOTIFICATION_ICON);
				strcpy(image_alt,NOTIFICATION_ICON_ALT);
			        }
			else if(strstr(input_buffer,"SERVICE EVENT HANDLER:")){
				strcpy(image,SERVICE_EVENT_ICON);
				strcpy(image_alt,SERVICE_EVENT_ICON_ALT);
			        }
			else if(strstr(input_buffer,"HOST EVENT HANDLER:")){
				strcpy(image,HOST_EVENT_ICON);
				strcpy(image_alt,HOST_EVENT_ICON_ALT);
			        }
			else if(strstr(input_buffer,"EXTERNAL COMMAND:")){
				strcpy(image,EXTERNAL_COMMAND_ICON);
				strcpy(image_alt,EXTERNAL_COMMAND_ICON_ALT);
			        }
			else if(strstr(input_buffer,"LOG ROTATION:")){
				strcpy(image,LOG_ROTATION_ICON);
				strcpy(image_alt,LOG_ROTATION_ICON_ALT);
			        }
			else if(strstr(input_buffer,"active mode...")){
				strcpy(image,ACTIVE_ICON);
				strcpy(image_alt,ACTIVE_ICON_ALT);
			        }
			else if(strstr(input_buffer,"standby mode...")){
				strcpy(image,STANDBY_ICON);
				strcpy(image_alt,STANDBY_ICON_ALT);
			        }
			else if(strstr(input_buffer,"SERVICE FLAPPING ALERT:") && strstr(input_buffer,";STARTED;")){
				strcpy(image,FLAPPING_ICON);
				strcpy(image_alt,"Service started flapping");
			        }
			else if(strstr(input_buffer,"SERVICE FLAPPING ALERT:") && strstr(input_buffer,";STOPPED;")){
				strcpy(image,FLAPPING_ICON);
				strcpy(image_alt,"Service stopped flapping");
			        }
			else if(strstr(input_buffer,"SERVICE FLAPPING ALERT:") && strstr(input_buffer,";DISABLED;")){
				strcpy(image,FLAPPING_ICON);
				strcpy(image_alt,"Service flap detection disabled");
			        }
			else if(strstr(input_buffer,"HOST FLAPPING ALERT:") && strstr(input_buffer,";STARTED;")){
				strcpy(image,FLAPPING_ICON);
				strcpy(image_alt,"Host started flapping");
			        }
			else if(strstr(input_buffer,"HOST FLAPPING ALERT:") && strstr(input_buffer,";STOPPED;")){
				strcpy(image,FLAPPING_ICON);
				strcpy(image_alt,"Host stopped flapping");
			        }
			else if(strstr(input_buffer,"HOST FLAPPING ALERT:") && strstr(input_buffer,";DISABLED;")){
				strcpy(image,FLAPPING_ICON);
				strcpy(image_alt,"Host flap detection disabled");
			        }
			else if(strstr(input_buffer,"SERVICE DOWNTIME ALERT:") && strstr(input_buffer,";STARTED;")){
				strcpy(image,SCHEDULED_DOWNTIME_ICON);
				strcpy(image_alt,"Service entered a period of scheduled downtime");
			        }
			else if(strstr(input_buffer,"SERVICE DOWNTIME ALERT:") && strstr(input_buffer,";STOPPED;")){
				strcpy(image,SCHEDULED_DOWNTIME_ICON);
				strcpy(image_alt,"Service exited a period of scheduled downtime");
			        }
			else if(strstr(input_buffer,"SERVICE DOWNTIME ALERT:") && strstr(input_buffer,";CANCELLED;")){
				strcpy(image,SCHEDULED_DOWNTIME_ICON);
				strcpy(image_alt,"Service scheduled downtime has been cancelled");
			        }
			else if(strstr(input_buffer,"HOST DOWNTIME ALERT:") && strstr(input_buffer,";STARTED;")){
				strcpy(image,SCHEDULED_DOWNTIME_ICON);
				strcpy(image_alt,"Host entered a period of scheduled downtime");
			        }
			else if(strstr(input_buffer,"HOST DOWNTIME ALERT:") && strstr(input_buffer,";STOPPED;")){
				strcpy(image,SCHEDULED_DOWNTIME_ICON);
				strcpy(image_alt,"Host exited a period of scheduled downtime");
			        }
			else if(strstr(input_buffer,"HOST DOWNTIME ALERT:") && strstr(input_buffer,";CANCELLED;")){
				strcpy(image,SCHEDULED_DOWNTIME_ICON);
				strcpy(image_alt,"Host scheduled downtime has been cancelled");
			        }
			else{
				strcpy(image,INFO_ICON);
				strcpy(image_alt,INFO_ICON_ALT);
			        }

			temp_buffer=strtok(input_buffer,"]");
			t=(temp_buffer==NULL)?0L:strtoul(temp_buffer+1,NULL,10);

			time_ptr=localtime(&t);
			strftime(current_message_date,sizeof(current_message_date),"%B %d, %Y %H:00\n",time_ptr);
			current_message_date[sizeof(current_message_date)-1]='\x0';

			if(strcmp(last_message_date,current_message_date)!=0 && display_timebreaks==TRUE){
				printf("<BR CLEAR='all'>\n");
				printf("<DIV CLASS='dateTimeBreak'>\n");
				printf("<table border=0 width=95%%><tr>");
				printf("<td width=40%%><hr width=100%%></td>");
				printf("<td align=center CLASS='dateTimeBreak'>%s</td>",current_message_date);
				printf("<td width=40%%><hr width=100%%></td>");
				printf("</tr></table>\n");
				printf("</DIV>\n");
				printf("<BR CLEAR='all'><DIV CLASS='logEntries'>\n");
				strncpy(last_message_date,current_message_date,sizeof(last_message_date));
				last_message_date[sizeof(last_message_date)-1]='\x0';
				}

			get_time_string(&t,date_time,(int)sizeof(date_time),SHORT_DATE_TIME);
			strip(date_time);

			temp_buffer=strtok(NULL,"\n");
			
			if(display_frills==TRUE)
				printf("<img align='left' src='%s%s' alt='%s'>",url_images_path,image,image_alt);
			printf("[%s] %s<br clear='all'>\n",date_time,(temp_buffer==NULL)?"":temp_buffer);
		        }

		printf("</DIV></P>\n");
		printf("<HR>\n");

		if(use_lifo==FALSE)
			fclose(fp);
	        }

	if(use_lifo==TRUE)
		free_lifo_memory();

	return OK;
        }















































