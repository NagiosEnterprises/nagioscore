/*****************************************************************************
 *
 * MACROS.C - Common macro functions for Nagios
 *
 * Copyright (c) 1999-2007 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   10-21-2007
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

#include "../include/macros.h"
#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"
#include "../include/comments.h"
#ifdef NSCORE
#include "../include/nagios.h"
#else
#include "../include/cgiutils.h"
#endif

#ifdef NSCORE
extern int      use_large_installation_tweaks;
#endif

extern char     *illegal_output_chars;

extern contact		*contact_list;
extern contactgroup	*contactgroup_list;
extern host             *host_list;
extern hostgroup	*hostgroup_list;
extern service          *service_list;
extern servicegroup     *servicegroup_list;
extern command          *command_list;
extern timeperiod       *timeperiod_list;


char            *macro_x[MACRO_X_COUNT];
char            *macro_x_names[MACRO_X_COUNT];
char            *macro_argv[MAX_COMMAND_ARGUMENTS];
char            *macro_user[MAX_USER_MACROS];
char            *macro_contactaddress[MAX_CONTACT_ADDRESSES];
char            *macro_ondemand=NULL;
customvariablesmember *macro_custom_host_vars=NULL;
customvariablesmember *macro_custom_service_vars=NULL;
customvariablesmember *macro_custom_contact_vars=NULL;



/******************************************************************/
/************************ MACRO FUNCTIONS *************************/
/******************************************************************/

/* replace macros in notification commands with their values */
int process_macros(char *input_buffer, char **output_buffer, int options){
	customvariablesmember *temp_customvariablesmember=NULL;
	char *temp_buffer=NULL;
	int in_macro=FALSE;
	int x=0;
	int arg_index=0;
	int user_index=0;
	int address_index=0;
	char *selected_macro=NULL;
	char *cleaned_macro=NULL;
	int clean_macro=FALSE;
	int found_macro_x=FALSE;


#ifdef NSCORE
	log_debug_info(DEBUGL_FUNCTIONS,0,"process_macros()\n");
#endif

	if(output_buffer==NULL)
		return ERROR;

	*output_buffer=(char *)strdup("");

	if(input_buffer==NULL)
		return ERROR;

	in_macro=FALSE;

#ifdef NSCORE
	log_debug_info(DEBUGL_MACROS,1,"**** BEGIN MACRO PROCESSING ***********\n");
	log_debug_info(DEBUGL_MACROS,1,"Processing: '%s'\n",input_buffer);
#endif

	for(temp_buffer=my_strtok(input_buffer,"$");temp_buffer!=NULL;temp_buffer=my_strtok(NULL,"$")){

#ifdef NSCORE
		log_debug_info(DEBUGL_MACROS,2,"  Processing part: '%s'\n",temp_buffer);
#endif

		selected_macro=NULL;
		found_macro_x=FALSE;
		clean_macro=FALSE;

		/* we're in plain text... */
		if(in_macro==FALSE){

			/* add the plain text to the end of the already processed buffer */
			*output_buffer=(char *)realloc(*output_buffer,strlen(*output_buffer)+strlen(temp_buffer)+1);
			strcat(*output_buffer,temp_buffer);

#ifdef NSCORE
			log_debug_info(DEBUGL_MACROS,2,"  Not currently in macro.  Running output (%d): '%s'\n",strlen(*output_buffer),*output_buffer);
#endif

			in_macro=TRUE;
			}

		/* looks like we're in a macro, so process it... */
		else{

			/* general macros */
			for(x=0;x<MACRO_X_COUNT;x++){
				if(macro_x_names[x]==NULL)
					continue;
				if(!strcmp(temp_buffer,macro_x_names[x])){

					selected_macro=macro_x[x];
					found_macro_x=TRUE;
						
					/* host/service output/perfdata and author/comment macros get cleaned */
					if((x>=16 && x<=19) ||(x>=49 && x<=52) || (x>=99 && x<=100) || (x>=124 && x<=127)){
						clean_macro=TRUE;
						options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
						}

					break;
					}
				}

			/* we already have a macro... */
			if(found_macro_x==TRUE)
				x=0;

			/* argv macros */
			else if(strstr(temp_buffer,"ARG")==temp_buffer){
				arg_index=atoi(temp_buffer+3);
				if(arg_index>=1 && arg_index<=MAX_COMMAND_ARGUMENTS)
					selected_macro=macro_argv[arg_index-1];
				else
					selected_macro=NULL;
				}

			/* user macros */
			else if(strstr(temp_buffer,"USER")==temp_buffer){
				user_index=atoi(temp_buffer+4);
				if(user_index>=1 && user_index<=MAX_USER_MACROS)
					selected_macro=macro_user[user_index-1];
				else
					selected_macro=NULL;
				}

			/* custom host variable macros */
			else if(strstr(temp_buffer,"_HOST")==temp_buffer){
				selected_macro=NULL;
				for(temp_customvariablesmember=macro_custom_host_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
					if(!strcmp(temp_buffer,temp_customvariablesmember->variable_name)){
						selected_macro=temp_customvariablesmember->variable_value;
						clean_macro=TRUE;
						options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
						break;
						}
					}
				}

			/* custom service variable macros */
			else if(strstr(temp_buffer,"_SERVICE")==temp_buffer){
				selected_macro=NULL;
				for(temp_customvariablesmember=macro_custom_service_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
					if(!strcmp(temp_buffer,temp_customvariablesmember->variable_name)){
						selected_macro=temp_customvariablesmember->variable_value;
						clean_macro=TRUE;
						options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
						break;
						}
					}
				}

			/* custom contact variable macros */
			else if(strstr(temp_buffer,"_CONTACT")==temp_buffer){
				selected_macro=NULL;
				for(temp_customvariablesmember=macro_custom_contact_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
					if(!strcmp(temp_buffer,temp_customvariablesmember->variable_name)){
						selected_macro=temp_customvariablesmember->variable_value;
						clean_macro=TRUE;
						options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
						break;
						}
					}
				}

			/* contact address macros */
			else if(strstr(temp_buffer,"CONTACTADDRESS")==temp_buffer){
				address_index=atoi(temp_buffer+14);
				if(address_index>=1 && address_index<=MAX_CONTACT_ADDRESSES)
					selected_macro=macro_contactaddress[address_index-1];
				else
					selected_macro=NULL;
				}

			/* on-demand hostgroup macros */
			else if(strstr(temp_buffer,"HOSTGROUP") && strstr(temp_buffer,":")){
				
				grab_on_demand_macro(temp_buffer);
				selected_macro=macro_ondemand;
				}

			/* on-demand host macros */
			else if(strstr(temp_buffer,"HOST") && strstr(temp_buffer,":")){
				
				grab_on_demand_macro(temp_buffer);
				selected_macro=macro_ondemand;
				
				/* output/perfdata macros get cleaned */
				if(strstr(temp_buffer,"HOSTOUTPUT:")==temp_buffer || strstr(temp_buffer,"LONGHOSTOUTPUT:")==temp_buffer  || strstr(temp_buffer,"HOSTPERFDATA:")){
					clean_macro=TRUE;
					options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
					}
				}

			/* on-demand servicegroup macros */
			else if(strstr(temp_buffer,"SERVICEGROUP") && strstr(temp_buffer,":")){
				
				grab_on_demand_macro(temp_buffer);
				selected_macro=macro_ondemand;
				}

			/* on-demand service macros */
			else if(strstr(temp_buffer,"SERVICE") && strstr(temp_buffer,":")){

				grab_on_demand_macro(temp_buffer);
				selected_macro=macro_ondemand;

				/* output/perfdata macros get cleaned */
				if(strstr(temp_buffer,"SERVICEOUTPUT:")==temp_buffer || strstr(temp_buffer,"LONGSERVICEOUTPUT:")==temp_buffer || strstr(temp_buffer,"SERVICEPERFDATA:")){
					clean_macro=TRUE;
					options&=STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS;
					}
				}
				
			/* on-demand contactgroup macros */
			else if(strstr(temp_buffer,"CONTACTGROUP") && strstr(temp_buffer,":")){
				
				grab_on_demand_macro(temp_buffer);
				selected_macro=macro_ondemand;
				}

			/* on-demand time macros */
			else if(strstr(temp_buffer,"ISVALIDTIME:") || strstr(temp_buffer,"NEXTVALIDTIME:")){

				grab_on_demand_macro(temp_buffer);
				selected_macro=macro_ondemand;
				}

			/* an escaped $ is done by specifying two $$ next to each other */
			else if(!strcmp(temp_buffer,"")){

#ifdef NSCORE
				log_debug_info(DEBUGL_MACROS,2,"  Escaped $.  Running output (%d): '%s'\n",strlen(*output_buffer),*output_buffer);
#endif

				*output_buffer=(char *)realloc(*output_buffer,strlen(*output_buffer)+2);
				strcat(*output_buffer,"$");
				}

			/* a non-macro, just some user-defined string between two $s */
			else{

#ifdef NSCORE
				log_debug_info(DEBUGL_MACROS,2,"  Non-macro.  Running output (%d): '%s'\n",strlen(*output_buffer),*output_buffer);
#endif

				/* add the plain text to the end of the already processed buffer */
				*output_buffer=(char *)realloc(*output_buffer,strlen(*output_buffer)+strlen(temp_buffer)+3);
				strcat(*output_buffer,"$");
				strcat(*output_buffer,temp_buffer);
				strcat(*output_buffer,"$");
				}

			/* insert macro */
			if(selected_macro!=NULL){

				/* URL encode the macro if requested - this allocates new memory */
				if(options & URL_ENCODE_MACRO_CHARS)
					selected_macro=get_url_encoded_string(selected_macro);
				
				/* some macros are cleaned... */
				if(clean_macro==TRUE || ((options & STRIP_ILLEGAL_MACRO_CHARS) || (options & ESCAPE_MACRO_CHARS))){

					/* add the (cleaned) processed macro to the end of the already processed buffer */
					if(selected_macro!=NULL && (cleaned_macro=clean_macro_chars(selected_macro,options))!=NULL){
						*output_buffer=(char *)realloc(*output_buffer,strlen(*output_buffer)+strlen(cleaned_macro)+1);
						strcat(*output_buffer,cleaned_macro);

#ifdef NSCORE
						log_debug_info(DEBUGL_MACROS,2,"  Cleaned macro.  Running output (%d): '%s'\n",strlen(*output_buffer),*output_buffer);
#endif
						}
					}

				/* others are not cleaned */
				else{
					/* add the processed macro to the end of the already processed buffer */
					if(selected_macro!=NULL){
						*output_buffer=(char *)realloc(*output_buffer,strlen(*output_buffer)+strlen(selected_macro)+1);
						strcat(*output_buffer,selected_macro);

#ifdef NSCORE
						log_debug_info(DEBUGL_MACROS,2,"  Uncleaned macro.  Running output (%d): '%s'\n",strlen(*output_buffer),*output_buffer);
#endif
						}
					}

				/* free memory if necessary (it was only allocated if we URL encoded the macro) */
				if(options & URL_ENCODE_MACRO_CHARS)
					my_free(&selected_macro);

#ifdef NSCORE
				log_debug_info(DEBUGL_MACROS,2,"  Just finished macro.  Running output (%d): '%s'\n",strlen(*output_buffer),*output_buffer);
#endif
				}

			in_macro=FALSE;
			}
		}

#ifdef NSCORE
	log_debug_info(DEBUGL_MACROS,1,"  Done.  Final output: '%s'\n",*output_buffer);
	log_debug_info(DEBUGL_MACROS,1,"**** END MACRO PROCESSING *************\n");
#endif

	return OK;
	}


/* grab macros that are specific to a particular service */
int grab_service_macros(service *svc){
	servicegroup *temp_servicegroup=NULL;
	customvariablesmember *temp_customvariablesmember=NULL;
	objectlist *temp_objectlist=NULL;
	char *customvarname=NULL;
	time_t current_time=0L;
	unsigned long duration=0L;
	int days=0;
	int hours=0;
	int minutes=0;
	int seconds=0;
	char *temp_buffer=NULL;
	char *buf1=NULL;
	char *buf2=NULL;

	/* clear service-related macros */
	clear_service_macros();
	clear_servicegroup_macros();
	
	if(svc==NULL)
		return ERROR;

	/* get the service description */
	macro_x[MACRO_SERVICEDESC]=(char *)strdup(svc->description);

	/* get the service display name */
	if(svc->display_name)
		macro_x[MACRO_SERVICEDISPLAYNAME]=(char *)strdup(svc->display_name);

#ifdef NSCORE
	/* get the plugin output */
	if(svc->plugin_output)
		macro_x[MACRO_SERVICEOUTPUT]=(char *)strdup(svc->plugin_output);

	/* get the long plugin output */
	if(svc->long_plugin_output)
		macro_x[MACRO_LONGSERVICEOUTPUT]=(char *)strdup(svc->long_plugin_output);

	/* get the performance data */
	if(svc->perf_data)
		macro_x[MACRO_SERVICEPERFDATA]=(char *)strdup(svc->perf_data);
#endif

	/* get the service check command */
	if(svc->service_check_command)
		macro_x[MACRO_SERVICECHECKCOMMAND]=(char *)strdup(svc->service_check_command);

#ifdef NSCORE
	/* grab the service check type */
	macro_x[MACRO_SERVICECHECKTYPE]=(char *)strdup((svc->check_type==SERVICE_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");

	/* grab the service state type macro (this is usually overridden later on) */
	macro_x[MACRO_SERVICESTATETYPE]=(char *)strdup((svc->state_type==HARD_STATE)?"HARD":"SOFT");

	/* get the service state */
	if(svc->current_state==STATE_OK)
		macro_x[MACRO_SERVICESTATE]=(char *)strdup("OK");
	else if(svc->current_state==STATE_WARNING)
		macro_x[MACRO_SERVICESTATE]=(char *)strdup("WARNING");
	else if(svc->current_state==STATE_CRITICAL)
		macro_x[MACRO_SERVICESTATE]=(char *)strdup("CRITICAL");
	else
		macro_x[MACRO_SERVICESTATE]=(char *)strdup("UNKNOWN");
#endif

	/* get the service volatility */
	asprintf(&macro_x[MACRO_SERVICEISVOLATILE],"%d",svc->is_volatile);

#ifdef NSCORE
	/* get the service state id */
	asprintf(&macro_x[MACRO_SERVICESTATEID],"%d",svc->current_state);

	/* get the current service check attempt macro */
	asprintf(&macro_x[MACRO_SERVICEATTEMPT],"%d",svc->current_attempt);

	/* get the max service check attempts macro */
	asprintf(&macro_x[MACRO_MAXSERVICEATTEMPTS],"%d",svc->max_attempts);

	/* get the execution time macro */
	asprintf(&macro_x[MACRO_SERVICEEXECUTIONTIME],"%.3f",svc->execution_time);

	/* get the latency macro */
	asprintf(&macro_x[MACRO_SERVICELATENCY],"%.3f",svc->latency);

	/* get the last check time macro */
	asprintf(&macro_x[MACRO_LASTSERVICECHECK],"%lu",(unsigned long)svc->last_check);

	/* get the last state change time macro */
	asprintf(&macro_x[MACRO_LASTSERVICESTATECHANGE],"%lu",(unsigned long)svc->last_state_change);

	/* get the last time ok macro */
	asprintf(&macro_x[MACRO_LASTSERVICEOK],"%lu",(unsigned long)svc->last_time_ok);

	/* get the last time warning macro */
	asprintf(&macro_x[MACRO_LASTSERVICEWARNING],"%lu",(unsigned long)svc->last_time_warning);

	/* get the last time unknown macro */
	asprintf(&macro_x[MACRO_LASTSERVICEUNKNOWN],"%lu",(unsigned long)svc->last_time_unknown);

	/* get the last time critical macro */
	asprintf(&macro_x[MACRO_LASTSERVICECRITICAL],"%lu",(unsigned long)svc->last_time_critical);

	/* get the service downtime depth */
	asprintf(&macro_x[MACRO_SERVICEDOWNTIME],"%d",svc->scheduled_downtime_depth);

	/* get the percent state change */
	asprintf(&macro_x[MACRO_SERVICEPERCENTCHANGE],"%.2f",svc->percent_state_change);

	time(&current_time);
	duration=(unsigned long)(current_time-svc->last_state_change);

	/* get the state duration in seconds */
	asprintf(&macro_x[MACRO_SERVICEDURATIONSEC],"%lu",duration);

	/* get the state duration */
	days=duration/86400;
	duration-=(days*86400);
	hours=duration/3600;
	duration-=(hours*3600);
	minutes=duration/60;
	duration-=(minutes*60);
	seconds=duration;
	asprintf(&macro_x[MACRO_SERVICEDURATION],"%dd %dh %dm %ds",days,hours,minutes,seconds);

	/* get the notification number macro */
	asprintf(&macro_x[MACRO_SERVICENOTIFICATIONNUMBER],"%d",svc->current_notification_number);

	/* get the notification id macro */
	asprintf(&macro_x[MACRO_SERVICENOTIFICATIONID],"%lu",svc->current_notification_id);

	/* get the event id macro */
	asprintf(&macro_x[MACRO_SERVICEEVENTID],"%lu",svc->current_event_id);

	/* get the last event id macro */
	asprintf(&macro_x[MACRO_LASTSERVICEEVENTID],"%lu",svc->last_event_id);
#endif

	/* get the action url */
	if(svc->action_url)
		macro_x[MACRO_SERVICEACTIONURL]=(char *)strdup(svc->action_url);

	/* get the notes url */
	if(svc->notes_url)
		macro_x[MACRO_SERVICENOTESURL]=(char *)strdup(svc->notes_url);

	/* get the notes */
	if(svc->notes)
		macro_x[MACRO_SERVICENOTES]=(char *)strdup(svc->notes);


#ifdef NSCORE
	/* find all servicegroups this service is associated with */
	for(temp_objectlist=svc->servicegroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

		if((temp_servicegroup=(servicegroup *)temp_objectlist->object_ptr)==NULL)
			continue;

		asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_servicegroup->group_name);
		my_free(&buf2);
		buf2=buf1;
		}
	if(buf2){
		macro_x[MACRO_SERVICEGROUPNAMES]=(char *)strdup(buf2);
		my_free(&buf2);
		}

	/* get first/primary servicegroup macros */
	if(svc->servicegroups_ptr){
		if((temp_servicegroup=(servicegroup *)svc->servicegroups_ptr->object_ptr))
			grab_servicegroup_macros(temp_servicegroup);
		}
#endif


	/* get custom variables */
	for(temp_customvariablesmember=svc->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
		asprintf(&customvarname,"_SERVICE%s",temp_customvariablesmember->variable_name);
		add_custom_variable_to_object(&macro_custom_service_vars,customvarname,temp_customvariablesmember->variable_value);
		my_free(&customvarname);
	        }

	/*
	strip(macro_x[MACRO_SERVICEOUTPUT]);
	strip(macro_x[MACRO_SERVICEPERFDATA]);
	strip(macro_x[MACRO_SERVICECHECKCOMMAND]);
	strip(macro_x[MACRO_SERVICENOTES]);
	*/

	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	if(macro_x[MACRO_SERVICEACTIONURL]!=NULL){
		process_macros(macro_x[MACRO_SERVICEACTIONURL],&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(&macro_x[MACRO_SERVICEACTIONURL]);
		macro_x[MACRO_SERVICEACTIONURL]=temp_buffer;
	        }
	if(macro_x[MACRO_SERVICENOTESURL]!=NULL){
		process_macros(macro_x[MACRO_SERVICENOTESURL],&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(&macro_x[MACRO_SERVICENOTESURL]);
		macro_x[MACRO_SERVICENOTESURL]=temp_buffer;
	        }
	if(macro_x[MACRO_SERVICENOTES]!=NULL){
		process_macros(macro_x[MACRO_SERVICENOTES],&temp_buffer,0);
		my_free(&macro_x[MACRO_SERVICENOTES]);
		macro_x[MACRO_SERVICENOTES]=temp_buffer;
	        }

	return OK;
        }



/* grab macros that are specific to a particular servicegroup */
int grab_servicegroup_macros(servicegroup *sg){
	servicesmember *temp_servicesmember=NULL;
	char *temp_buffer=NULL;

	/* clear servicegroup macros */
	clear_servicegroup_macros();

	if(sg==NULL)
		return ERROR;

	/* get the servicegroup name */
	macro_x[MACRO_SERVICEGROUPNAME]=(char *)strdup(sg->group_name);
	
	/* get the servicegroup alias */
	if(sg->alias)
		macro_x[MACRO_SERVICEGROUPALIAS]=(char *)strdup(sg->alias);

	/* get the group members */
#ifdef THIS_CAUSES_A_SIGABRT
	for(temp_servicesmember=sg->members;temp_servicesmember!=NULL;temp_servicesmember=temp_servicesmember->next){
		if(macro_x[MACRO_SERVICEGROUPMEMBERS]==NULL){
			if((macro_x[MACRO_SERVICEGROUPMEMBERS]=(char *)malloc(strlen(temp_servicesmember->host_name)+strlen(temp_servicesmember->service_description)+1))){
				sprintf(macro_x[MACRO_SERVICEGROUPMEMBERS],"%s,%s",temp_servicesmember->host_name,temp_servicesmember->service_description);
				}
			}
		else if((macro_x[MACRO_SERVICEGROUPMEMBERS]=(char *)realloc(macro_x[MACRO_SERVICEGROUPMEMBERS],strlen(macro_x[MACRO_SERVICEGROUPMEMBERS])+strlen(temp_servicesmember->host_name)+strlen(temp_servicesmember->service_description)+3))){
			strcat(macro_x[MACRO_SERVICEGROUPMEMBERS],",");
			strcat(macro_x[MACRO_SERVICEGROUPMEMBERS],temp_servicesmember->host_name);
			strcat(macro_x[MACRO_SERVICEGROUPMEMBERS],",");
			strcat(macro_x[MACRO_SERVICEGROUPMEMBERS],temp_servicesmember->service_description);
			}
		}
#endif

	/* get the servicegroup action url */
	if(sg->action_url)
		macro_x[MACRO_SERVICEGROUPACTIONURL]=(char *)strdup(sg->action_url);

	/* get the servicegroup notes url */
	if(sg->notes_url)
		macro_x[MACRO_SERVICEGROUPNOTESURL]=(char *)strdup(sg->notes_url);

	/* get the servicegroup notes */
	if(sg->notes)
		macro_x[MACRO_SERVICEGROUPNOTES]=(char *)strdup(sg->notes);

	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	if(macro_x[MACRO_SERVICEGROUPACTIONURL]!=NULL){
		process_macros(macro_x[MACRO_SERVICEGROUPACTIONURL],&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(&macro_x[MACRO_SERVICEGROUPACTIONURL]);
		macro_x[MACRO_SERVICEGROUPACTIONURL]=temp_buffer;
	        }
	if(macro_x[MACRO_SERVICEGROUPNOTESURL]!=NULL){
		process_macros(macro_x[MACRO_SERVICEGROUPNOTESURL],&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(&macro_x[MACRO_SERVICEGROUPNOTESURL]);
		macro_x[MACRO_SERVICEGROUPNOTESURL]=temp_buffer;
	        }
	if(macro_x[MACRO_SERVICEGROUPNOTES]!=NULL){
		process_macros(macro_x[MACRO_SERVICEGROUPNOTES],&temp_buffer,0);
		my_free(&macro_x[MACRO_SERVICEGROUPNOTES]);
		macro_x[MACRO_SERVICEGROUPNOTES]=temp_buffer;
	        }

	return OK;
	}



/* grab macros that are specific to a particular host */
int grab_host_macros(host *hst){
	hostgroup *temp_hostgroup=NULL;
	customvariablesmember *temp_customvariablesmember=NULL;
	servicesmember *temp_servicesmember=NULL;
	service *temp_service=NULL;
	objectlist *temp_objectlist=NULL;
	char *customvarname=NULL;
	time_t current_time=0L;
	unsigned long duration=0L;
	int days=0;
	int hours=0;
	int minutes=0;
	int seconds=0;
	int total_host_services=0;
	int total_host_services_ok=0;
	int total_host_services_warning=0;
	int total_host_services_unknown=0;
	int total_host_services_critical=0;
	char *temp_buffer=NULL;
	char *buf1=NULL;
	char *buf2=NULL;

	/* clear host-related macros */
	clear_host_macros();
	clear_hostgroup_macros();

	if(hst==NULL)
		return ERROR;

	/* get the host name */
	macro_x[MACRO_HOSTNAME]=(char *)strdup(hst->name);
	
	/* get the host display name */
	if(hst->display_name)
		macro_x[MACRO_HOSTDISPLAYNAME]=(char *)strdup(hst->display_name);

	/* get the host alias */
	macro_x[MACRO_HOSTALIAS]=(char *)strdup(hst->alias);

	/* get the host address */
	macro_x[MACRO_HOSTADDRESS]=(char *)strdup(hst->address);

#ifdef NSCORE
	/* get the host state */
	if(hst->current_state==HOST_DOWN)
		macro_x[MACRO_HOSTSTATE]=(char *)strdup("DOWN");
	else if(hst->current_state==HOST_UNREACHABLE)
		macro_x[MACRO_HOSTSTATE]=(char *)strdup("UNREACHABLE");
	else
		macro_x[MACRO_HOSTSTATE]=(char *)strdup("UP");

	/* get the host state id */
	asprintf(&macro_x[MACRO_HOSTSTATEID],"%d",hst->current_state);

	/* grab the host check type */
	asprintf(&macro_x[MACRO_HOSTCHECKTYPE],"%s",(hst->check_type==HOST_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");

	/* get the host state type macro */
	asprintf(&macro_x[MACRO_HOSTSTATETYPE],"%s",(hst->state_type==HARD_STATE)?"HARD":"SOFT");

	/* get the plugin output */
	if(hst->plugin_output)
		macro_x[MACRO_HOSTOUTPUT]=(char *)strdup(hst->plugin_output);

	/* get the long plugin output */
	if(hst->long_plugin_output)
		macro_x[MACRO_LONGHOSTOUTPUT]=(char *)strdup(hst->long_plugin_output);

	/* get the performance data */
	if(hst->perf_data)
		macro_x[MACRO_HOSTPERFDATA]=(char *)strdup(hst->perf_data);
#endif

	/* get the host check command */
	if(hst->host_check_command)
		macro_x[MACRO_HOSTCHECKCOMMAND]=(char *)strdup(hst->host_check_command);

#ifdef NSCORE
	/* get the host current attempt */
	asprintf(&macro_x[MACRO_HOSTATTEMPT],"%d",hst->current_attempt);

	/* get the max host attempts */
	asprintf(&macro_x[MACRO_MAXHOSTATTEMPTS],"%d",hst->max_attempts);

	/* get the host downtime depth */
	asprintf(&macro_x[MACRO_HOSTDOWNTIME],"%d",hst->scheduled_downtime_depth);

	/* get the percent state change */
	asprintf(&macro_x[MACRO_HOSTPERCENTCHANGE],"%.2f",hst->percent_state_change);

	time(&current_time);
	duration=(unsigned long)(current_time-hst->last_state_change);

	/* get the state duration in seconds */
	asprintf(&macro_x[MACRO_HOSTDURATIONSEC],"%lu",duration);

	/* get the state duration */
	days=duration/86400;
	duration-=(days*86400);
	hours=duration/3600;
	duration-=(hours*3600);
	minutes=duration/60;
	duration-=(minutes*60);
	seconds=duration;
	asprintf(&macro_x[MACRO_HOSTDURATION],"%dd %dh %dm %ds",days,hours,minutes,seconds);

	/* get the execution time macro */
	asprintf(&macro_x[MACRO_HOSTEXECUTIONTIME],"%.3f",hst->execution_time);

	/* get the latency macro */
	asprintf(&macro_x[MACRO_HOSTLATENCY],"%.3f",hst->latency);

	/* get the last check time macro */
	asprintf(&macro_x[MACRO_LASTHOSTCHECK],"%lu",(unsigned long)hst->last_check);

	/* get the last state change time macro */
	asprintf(&macro_x[MACRO_LASTHOSTSTATECHANGE],"%lu",(unsigned long)hst->last_state_change);

	/* get the last time up macro */
	asprintf(&macro_x[MACRO_LASTHOSTUP],"%lu",(unsigned long)hst->last_time_up);

	/* get the last time down macro */
	asprintf(&macro_x[MACRO_LASTHOSTDOWN],"%lu",(unsigned long)hst->last_time_down);

	/* get the last time unreachable macro */
	asprintf(&macro_x[MACRO_LASTHOSTUNREACHABLE],"%lu",(unsigned long)hst->last_time_unreachable);

	/* get the notification number macro */
	asprintf(&macro_x[MACRO_HOSTNOTIFICATIONNUMBER],"%d",hst->current_notification_number);

	/* get the notification id macro */
	asprintf(&macro_x[MACRO_HOSTNOTIFICATIONID],"%lu",hst->current_notification_id);

	/* get the event id macro */
	asprintf(&macro_x[MACRO_HOSTEVENTID],"%lu",hst->current_event_id);

	/* get the last event id macro */
	asprintf(&macro_x[MACRO_LASTHOSTEVENTID],"%lu",hst->last_event_id);
#endif

	/* get the action url */
	if(hst->action_url)
		macro_x[MACRO_HOSTACTIONURL]=(char *)strdup(hst->action_url);

	/* get the notes url */
	if(hst->notes_url)
		macro_x[MACRO_HOSTNOTESURL]=(char *)strdup(hst->notes_url);

	/* get the notes */
	if(hst->notes)
		macro_x[MACRO_HOSTNOTES]=(char *)strdup(hst->notes);


#ifdef NSCORE
	/* find all hostgroups this host is associated with */
	for(temp_objectlist=hst->hostgroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

		if((temp_hostgroup=(hostgroup *)temp_objectlist->object_ptr)==NULL)
			continue;

		asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_hostgroup->group_name);
		my_free(&buf2);
		buf2=buf1;
		}
	if(buf2){
		macro_x[MACRO_HOSTGROUPNAMES]=(char *)strdup(buf2);
		my_free(&buf2);
		}

	/* get first/primary hostgroup macros */
	if(hst->hostgroups_ptr){
		if((temp_hostgroup=(hostgroup *)hst->hostgroups_ptr->object_ptr))
			grab_hostgroup_macros(temp_hostgroup);
		}


	/* service summary macros */
	for(temp_servicesmember=hst->services;temp_servicesmember!=NULL;temp_servicesmember=temp_servicesmember->next){
		if((temp_service=temp_servicesmember->service_ptr)==NULL)
			continue;

		total_host_services++;

		switch(temp_service->current_state){
		case STATE_OK:
			total_host_services_ok++;
			break;
		case STATE_WARNING:
			total_host_services_warning++;
			break;
		case STATE_UNKNOWN:
			total_host_services_unknown++;
			break;
		case STATE_CRITICAL:
			total_host_services_critical++;
			break;
		default:
			break;
			}
		}
	asprintf(&macro_x[MACRO_TOTALHOSTSERVICES],"%lu",total_host_services);
	asprintf(&macro_x[MACRO_TOTALHOSTSERVICESOK],"%lu",total_host_services_ok);
	asprintf(&macro_x[MACRO_TOTALHOSTSERVICESWARNING],"%lu",total_host_services_warning);
	asprintf(&macro_x[MACRO_TOTALHOSTSERVICESUNKNOWN],"%lu",total_host_services_unknown);
	asprintf(&macro_x[MACRO_TOTALHOSTSERVICESCRITICAL],"%lu",total_host_services_critical);
#endif

	/* get custom variables */
	for(temp_customvariablesmember=hst->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
		asprintf(&customvarname,"_HOST%s",temp_customvariablesmember->variable_name);
		add_custom_variable_to_object(&macro_custom_host_vars,customvarname,temp_customvariablesmember->variable_value);
		my_free(&customvarname);
	        }

	/*
	strip(macro_x[MACRO_HOSTOUTPUT]);
	strip(macro_x[MACRO_HOSTPERFDATA]);
	strip(macro_x[MACRO_HOSTCHECKCOMMAND]);
	strip(macro_x[MACRO_HOSTNOTES]);
	*/

	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	if(macro_x[MACRO_HOSTACTIONURL]!=NULL){
		process_macros(macro_x[MACRO_HOSTACTIONURL],&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(&macro_x[MACRO_HOSTACTIONURL]);
		macro_x[MACRO_HOSTACTIONURL]=temp_buffer;
	        }
	if(macro_x[MACRO_HOSTNOTESURL]!=NULL){
		process_macros(macro_x[MACRO_HOSTNOTESURL],&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(&macro_x[MACRO_HOSTNOTESURL]);
		macro_x[MACRO_HOSTNOTESURL]=temp_buffer;
	        }
	if(macro_x[MACRO_HOSTNOTES]!=NULL){
		process_macros(macro_x[MACRO_HOSTNOTES],&temp_buffer,0);
		my_free(&macro_x[MACRO_HOSTNOTES]);
		macro_x[MACRO_HOSTNOTES]=temp_buffer;
	        }

	return OK;
        }


/* grab hostgroup macros */
int grab_hostgroup_macros(hostgroup *hg){
	hostsmember *temp_hostsmember=NULL;
	char *temp_buffer=NULL;

	/* clear hostgroup macros */
	clear_hostgroup_macros();

	if(hg==NULL)
		return ERROR;

	/* get the hostgroup name */
	macro_x[MACRO_HOSTGROUPNAME]=(char *)strdup(hg->group_name);
	
	/* get the hostgroup alias */
	if(hg->alias)
		macro_x[MACRO_HOSTGROUPALIAS]=(char *)strdup(hg->alias);

	/* get the group members */
	for(temp_hostsmember=hg->members;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){
		if(macro_x[MACRO_HOSTGROUPMEMBERS]==NULL)
			macro_x[MACRO_HOSTGROUPMEMBERS]=(char *)strdup(temp_hostsmember->host_name);
		else if((macro_x[MACRO_HOSTGROUPMEMBERS]=(char *)realloc(macro_x[MACRO_HOSTGROUPMEMBERS],strlen(macro_x[MACRO_HOSTGROUPMEMBERS])+strlen(temp_hostsmember->host_name)+2))){
			strcat(macro_x[MACRO_HOSTGROUPMEMBERS],",");
			strcat(macro_x[MACRO_HOSTGROUPMEMBERS],temp_hostsmember->host_name);
			}
		}

	/* get the hostgroup action url */
	if(hg->action_url)
		macro_x[MACRO_HOSTGROUPACTIONURL]=(char *)strdup(hg->action_url);

	/* get the hostgroup notes url */
	if(hg->notes_url)
		macro_x[MACRO_HOSTGROUPNOTESURL]=(char *)strdup(hg->notes_url);

	/* get the hostgroup notes */
	if(hg->notes)
		macro_x[MACRO_HOSTGROUPNOTES]=(char *)strdup(hg->notes);

	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	if(macro_x[MACRO_HOSTGROUPACTIONURL]!=NULL){
		process_macros(macro_x[MACRO_HOSTGROUPACTIONURL],&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(&macro_x[MACRO_HOSTGROUPACTIONURL]);
		macro_x[MACRO_HOSTGROUPACTIONURL]=temp_buffer;
	        }
	if(macro_x[MACRO_HOSTGROUPNOTESURL]!=NULL){
		process_macros(macro_x[MACRO_HOSTGROUPNOTESURL],&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(&macro_x[MACRO_HOSTGROUPNOTESURL]);
		macro_x[MACRO_HOSTGROUPNOTESURL]=temp_buffer;
	        }
	if(macro_x[MACRO_HOSTGROUPNOTES]!=NULL){
		process_macros(macro_x[MACRO_HOSTGROUPNOTES],&temp_buffer,0);
		my_free(&macro_x[MACRO_HOSTGROUPNOTES]);
		macro_x[MACRO_HOSTGROUPNOTES]=temp_buffer;
	        }

	return OK;
	}


/* grab macros that are specific to a particular contact */
int grab_contact_macros(contact *cntct){
	customvariablesmember *temp_customvariablesmember=NULL;
	char *customvarname=NULL;
	contactgroup *temp_contactgroup=NULL;
	objectlist *temp_objectlist=NULL;
	char *buf1=NULL;
	char *buf2=NULL;
	int x=0;

	/* clear contact-related macros */
	clear_contact_macros();
	clear_contactgroup_macros();

	if(cntct==NULL)
		return ERROR;

	/* get the name */
	my_free(&macro_x[MACRO_CONTACTNAME]);
	macro_x[MACRO_CONTACTNAME]=(char *)strdup(cntct->name);

	/* get the alias */
	my_free(&macro_x[MACRO_CONTACTALIAS]);
	macro_x[MACRO_CONTACTALIAS]=(char *)strdup(cntct->alias);

	/* get the email address */
	my_free(&macro_x[MACRO_CONTACTEMAIL]);
	if(cntct->email)
		macro_x[MACRO_CONTACTEMAIL]=(char *)strdup(cntct->email);

	/* get the pager number */
	my_free(&macro_x[MACRO_CONTACTPAGER]);
	if(cntct->pager)
		macro_x[MACRO_CONTACTPAGER]=(char *)strdup(cntct->pager);

	/* get misc contact addresses */
	for(x=0;x<MAX_CONTACT_ADDRESSES;x++){
		my_free(&macro_contactaddress[x]);
		if(cntct->address[x]){
			macro_contactaddress[x]=(char *)strdup(cntct->address[x]);
			/*strip(macro_contactaddress[x]);*/
		        }
	        }

#ifdef NSCORE
	/* get the contactgroup names */
	/* find all contactgroups this contact is a member of */
	for(temp_objectlist=cntct->contactgroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

		if((temp_contactgroup=(contactgroup *)temp_objectlist->object_ptr)==NULL)
			continue;

		asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_contactgroup->group_name);
		my_free(&buf2);
		buf2=buf1;
		}
	if(buf2){
		macro_x[MACRO_CONTACTGROUPNAMES]=(char *)strdup(buf2);
		my_free(&buf2);
		}

	/* get first/primary contactgroup macros */
	if(cntct->contactgroups_ptr){
		if((temp_contactgroup=(contactgroup *)cntct->contactgroups_ptr->object_ptr))
			grab_contactgroup_macros(temp_contactgroup);
		}
#endif

	/* get custom variables */
	for(temp_customvariablesmember=cntct->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
		asprintf(&customvarname,"_CONTACT%s",temp_customvariablesmember->variable_name);
		add_custom_variable_to_object(&macro_custom_contact_vars,customvarname,temp_customvariablesmember->variable_value);
		my_free(&customvarname);
	        }

	/*
	strip(macro_x[MACRO_CONTACTNAME]);
	strip(macro_x[MACRO_CONTACTALIAS]);
	strip(macro_x[MACRO_CONTACTEMAIL]);
	strip(macro_x[MACRO_CONTACTPAGER]);
	*/

	return OK;
        }



/* grab contactgroup macros */
int grab_contactgroup_macros(contactgroup *cg){
	contactgroup *temp_contactgroup=NULL;
	contactsmember *temp_contactsmember=NULL;

	/* clear contactgroup macros */
	clear_contactgroup_macros();

	if(cg==NULL)
		return ERROR;

	/* get the group alias */
	my_free(&macro_x[MACRO_CONTACTGROUPALIAS]);
	if(cg->alias)
		macro_x[MACRO_CONTACTGROUPALIAS]=(char *)strdup(cg->alias);

	/* get the member list */
	my_free(&macro_x[MACRO_CONTACTGROUPMEMBERS]);
	for(temp_contactsmember=cg->members;temp_contactsmember!=NULL;temp_contactsmember=temp_contactsmember->next){
		if(macro_x[MACRO_CONTACTGROUPMEMBERS]==NULL)
			macro_x[MACRO_CONTACTGROUPMEMBERS]=(char *)strdup(temp_contactsmember->contact_name);
		else if((macro_x[MACRO_CONTACTGROUPMEMBERS]=(char *)realloc(macro_x[MACRO_CONTACTGROUPMEMBERS],strlen(macro_x[MACRO_CONTACTGROUPMEMBERS])+strlen(temp_contactsmember->contact_name)+2))){
			strcat(macro_x[MACRO_CONTACTGROUPMEMBERS],",");
			strcat(macro_x[MACRO_CONTACTGROUPMEMBERS],temp_contactsmember->contact_name);
			}
	        }

	return OK;
	}


/* grab an on-demand macro */
int grab_on_demand_macro(char *str){
	char *macro=NULL;
	char *first_arg=NULL;
	char *second_arg=NULL;
	char result_buffer[MAX_INPUT_BUFFER]="";
	int result_buffer_len=0;
	int delimiter_len=0;
	host *temp_host=NULL;
	hostgroup *temp_hostgroup=NULL;
	hostsmember *temp_hostsmember=NULL;
	service *temp_service=NULL;
	servicegroup *temp_servicegroup=NULL;
	servicesmember *temp_servicesmember=NULL;
	contact *temp_contact=NULL;
	contactgroup *temp_contactgroup=NULL;
	contactsmember *temp_contactsmember=NULL;
	char *ptr=NULL;
	char *host_name=NULL;
	int return_val=ERROR;

	/* clear the on-demand macro */
	my_free(&macro_ondemand);

	/* save a copy of the macro */
	if((macro=(char *)strdup(str))==NULL)
		return ERROR;

	/* get the first argument (e.g. host name) */
	if((ptr=strchr(macro,':'))==NULL){
		my_free(&macro);
		return ERROR;
	        }

	/* terminate the macro name at the first arg's delimiter */
	ptr[0]='\x0';
	first_arg=ptr+1;

	/* get the second argument (if present) */
	ptr=strchr(first_arg,':');
	if(ptr!=NULL){
		/* terminate the first arg at the second arg's delimiter */
		ptr[0]='\x0';
		second_arg=ptr+1;
	        }

	/* process the macro... */

	/* on-demand hostgroup macros */
	if(strstr(macro,"HOSTGROUP")){

		temp_hostgroup=find_hostgroup(first_arg);
		return_val=grab_on_demand_hostgroup_macro(temp_hostgroup,macro);
		}

	/* on-demand host macros */
	else if(strstr(macro,"HOST")){

		/* process a host macro */
		if(second_arg==NULL){
			temp_host=find_host(first_arg);
			return_val=grab_on_demand_host_macro(temp_host,macro);
	                }

		/* process a host macro containing a hostgroup */
		else{
			if((temp_hostgroup=find_hostgroup(first_arg))==NULL){
				my_free(&macro);
				return ERROR;
				}

			return_val=OK;  /* start off assuming there's no error */
			result_buffer[0]='\0';
			result_buffer[sizeof(result_buffer)-1]='\0';
			result_buffer_len=0;
			delimiter_len=strlen(second_arg);

			/* process each host in the hostgroup */
			if((temp_hostsmember=temp_hostgroup->members)==NULL){
				macro_ondemand=(char *)strdup("");
				my_free(&macro);
				return OK;
				}
			while(1){
#ifdef NSCORE
				if((temp_host=temp_hostsmember->host_ptr)==NULL)
					continue;
#else
				if((temp_host=find_host(temp_hostsmember->host_name))==NULL)
					continue;
#endif
				if(grab_on_demand_host_macro(temp_host,macro)==OK){
					strncat(result_buffer,macro_ondemand,sizeof(result_buffer)-result_buffer_len-1);
					result_buffer_len+=strlen(macro_ondemand);
					if(result_buffer_len>sizeof(result_buffer)-1){
						return_val=ERROR;
						break;
						}
					if((temp_hostsmember=temp_hostsmember->next)==NULL)
						break;
					strncat(result_buffer,second_arg,sizeof(result_buffer)-result_buffer_len-1);
					result_buffer_len+=delimiter_len;
					if(result_buffer_len>sizeof(result_buffer)-1){
						return_val=ERROR;
						break;
						}
					}
				else{
					return_val=ERROR;
					if((temp_hostsmember=temp_hostsmember->next)==NULL)
						break;
					}

				my_free(&macro_ondemand);
				}

			my_free(&macro_ondemand);
			macro_ondemand=(char *)strdup(result_buffer);
			}
	        }

	/* on-demand servicegroup macros */
	else if(strstr(macro,"SERVICEGROUP")){

		temp_servicegroup=find_servicegroup(first_arg);
		return_val=grab_on_demand_servicegroup_macro(temp_servicegroup,macro);
		}

	/* on-demand service macros */
	else if(strstr(macro,"SERVICE")){

		/* second args will either be service description or delimiter */
		if(second_arg==NULL){
			my_free(&macro);
			return ERROR;
	                }

		/* if first arg (host name) is blank, it means refer to the "current" host, so look to other macros for help... */
		if(!strcmp(first_arg,""))
			host_name=macro_x[MACRO_HOSTNAME];
		else
			host_name=first_arg;

		/* process a service macro */
		temp_service=find_service(host_name,second_arg);
		if(temp_service!=NULL)
			return_val=grab_on_demand_service_macro(temp_service,macro);

		/* process a service macro containing a servicegroup */
		else{
			if((temp_servicegroup=find_servicegroup(first_arg))==NULL){
				my_free(&macro);
				return ERROR;
				}

			return_val=OK;  /* start off assuming there's no error */
			result_buffer[0]='\0';
			result_buffer[sizeof(result_buffer)-1]='\0';
			result_buffer_len=0;
			delimiter_len=strlen(second_arg);

			/* process each service in the servicegroup */
			if((temp_servicesmember=temp_servicegroup->members)==NULL){
				macro_ondemand=(char *)strdup("");
				my_free(&macro);
				return OK;
				}
			while(1){
#ifdef NSCORE
				if((temp_service=temp_servicesmember->service_ptr)==NULL)
					continue;
#else
				if((temp_service=find_service(temp_servicesmember->host_name,temp_servicesmember->service_description))==NULL)
					continue;
#endif
				if(grab_on_demand_service_macro(temp_service,macro)==OK){
					strncat(result_buffer,macro_ondemand,sizeof(result_buffer)-result_buffer_len-1);
					result_buffer_len+=strlen(macro_ondemand);
					if(result_buffer_len>sizeof(result_buffer)-1){
						return_val=ERROR;
						break;
						}
					if((temp_servicesmember=temp_servicesmember->next)==NULL)
						break;
					strncat(result_buffer,second_arg,sizeof(result_buffer)-result_buffer_len-1);
					result_buffer_len+=delimiter_len;
					if(result_buffer_len>sizeof(result_buffer)-1){
						return_val=ERROR;
						break;
						}
					}
				else{
					return_val=ERROR;
					if((temp_servicesmember=temp_servicesmember->next)==NULL)
						break;
					}

				my_free(&macro_ondemand);
				}

			my_free(&macro_ondemand);
			macro_ondemand=(char *)strdup(result_buffer);
			}
	        }

	/* on-demand contactgroup macros */
	else if(strstr(macro,"CONTACTGROUP")){

		temp_contactgroup=find_contactgroup(first_arg);
		return_val=grab_on_demand_contactgroup_macro(temp_contactgroup,macro);
		}

	/* on-demand contact macros */
	else if(strstr(macro,"CONTACT")){

		/* process a contact macro */
		if(second_arg==NULL){
			temp_contact=find_contact(first_arg);
			return_val=grab_on_demand_contact_macro(temp_contact,macro);
	                }

		/* process a contact macro containing a contactgroup name */
		else{
			if((temp_contactgroup=find_contactgroup(first_arg))==NULL){
				my_free(&macro);
				return ERROR;
				}

			return_val=OK;  /* start off assuming there's no error */
			result_buffer[0]='\0';
			result_buffer[sizeof(result_buffer)-1]='\0';
			result_buffer_len=0;
			delimiter_len=strlen(second_arg);

			/* process each contact in the contactgroup */
			if((temp_contactsmember=temp_contactgroup->members)==NULL){
				macro_ondemand=(char *)strdup("");
				my_free(&macro);
				return OK;
				}
			while(1){
#ifdef NSCORE
				if((temp_contact=temp_contactsmember->contact_ptr)==NULL)
					continue;
#else
				if((temp_contact=find_contact(temp_contactsmember->contact_name))==NULL)
					continue;
#endif
				if(grab_on_demand_contact_macro(temp_contact,macro)==OK){
					strncat(result_buffer,macro_ondemand,sizeof(result_buffer)-result_buffer_len-1);
					result_buffer_len+=strlen(macro_ondemand);
					if(result_buffer_len>sizeof(result_buffer)-1){
						return_val=ERROR;
						break;
						}
					if((temp_hostsmember=temp_hostsmember->next)==NULL)
						break;
					strncat(result_buffer,second_arg,sizeof(result_buffer)-result_buffer_len-1);
					result_buffer_len+=delimiter_len;
					if(result_buffer_len>sizeof(result_buffer)-1){
						return_val=ERROR;
						break;
						}
					}
				else{
					return_val=ERROR;
					if((temp_contactsmember=temp_contactsmember->next)==NULL)
						break;
					}

				my_free(&macro_ondemand);
				}

			my_free(&macro_ondemand);
			macro_ondemand=(char *)strdup(result_buffer);
			}
	        }

	/* on-demand time macros */
	else if(!strcmp(macro,"ISVALIDTIME") || !strcmp(macro,"NEXTVALIDTIME"))
		return_val=grab_on_demand_time_macro(macro,first_arg,second_arg);

	else
		return_val=ERROR;

	my_free(&macro);

	return return_val;
        }


/* grab an on-demand host macro */
int grab_on_demand_host_macro(host *hst, char *macro){
	hostgroup *temp_hostgroup=NULL;
	customvariablesmember *temp_customvariablesmember=NULL;
	objectlist *temp_objectlist=NULL;
	char *customvarname=NULL;
	char *temp_buffer=NULL;
	char *buf1=NULL;
	char *buf2=NULL;
	time_t current_time=0L;
	unsigned long duration=0L;
	int days=0;
	int hours=0;
	int minutes=0;
	int seconds=0;
	int total_host_services=0;
	int total_host_services_ok=0;
	int total_host_services_warning=0;
	int total_host_services_unknown=0;
	int total_host_services_critical=0;
	servicesmember *temp_servicesmember=NULL;
	service *temp_service=NULL;


	if(hst==NULL || macro==NULL)
		return ERROR;

	/* initialize the macro */
	macro_ondemand=NULL;

#ifdef NSCORE
	time(&current_time);
	duration=(unsigned long)(current_time-hst->last_state_change);
#endif

#ifdef NSCORE
	log_debug_info(DEBUGL_MACROS,2,"  Getting on-demand macro for host '%s': %s\n",hst->name,macro);
#endif

	/* get the host display name */
	if(!strcmp(macro,"HOSTDISPLAYNAME")){
		if(hst->display_name)
			macro_ondemand=(char *)strdup(hst->display_name);
	        }

	/* get the host alias */
	else if(!strcmp(macro,"HOSTALIAS"))
		macro_ondemand=(char *)strdup(hst->alias);

	/* get the host address */
	else if(!strcmp(macro,"HOSTADDRESS"))
		macro_ondemand=(char *)strdup(hst->address);

	/* get the host check command */
	else if(!strcmp(macro,"HOSTCHECKCOMMAND"))
		macro_ondemand=(char *)strdup(hst->host_check_command);

#ifdef NSCORE
	/* get the host state */
	else if(!strcmp(macro,"HOSTSTATE")){
		if(hst->current_state==HOST_DOWN)
			macro_ondemand=(char *)strdup("DOWN");
		else if(hst->current_state==HOST_UNREACHABLE)
			macro_ondemand=(char *)strdup("UNREACHABLE");
		else
			macro_ondemand=(char *)strdup("UP");
	        }

	/* get the host state id */
	else if(!strcmp(macro,"HOSTSTATEID"))
		asprintf(&macro_ondemand,"%d",hst->current_state);

	/* grab the host check type */
	else if(!strcmp(macro,"HOSTCHECKTYPE"))
		asprintf(&macro_ondemand,"%s",(hst->check_type==HOST_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");

	/* get the host state type macro */
	else if(!strcmp(macro,"HOSTSTATETYPE"))
		asprintf(&macro_ondemand,"%s",(hst->state_type==HARD_STATE)?"HARD":"SOFT");

	/* get the plugin output */
	else if(!strcmp(macro,"HOSTOUTPUT")){
		if(hst->plugin_output){
			macro_ondemand=(char *)strdup(hst->plugin_output);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* get the long plugin output */
	else if(!strcmp(macro,"LONGHOSTOUTPUT")){
		if(hst->long_plugin_output==NULL)
			macro_ondemand=NULL;
		else{
			macro_ondemand=(char *)strdup(hst->long_plugin_output);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* get the performance data */
	else if(!strcmp(macro,"HOSTPERFDATA")){
		if(hst->perf_data){
			macro_ondemand=(char *)strdup(hst->perf_data);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* get the host current attempt */
	else if(!strcmp(macro,"HOSTATTEMPT"))
		asprintf(&macro_ondemand,"%d",hst->current_attempt);

	/* get the max host attempts */
	else if(!strcmp(macro,"MAXHOSTATTEMPTS"))
		asprintf(&macro_ondemand,"%d",hst->max_attempts);

	/* get the host downtime depth */
	else if(!strcmp(macro,"HOSTDOWNTIME"))
		asprintf(&macro_ondemand,"%d",hst->scheduled_downtime_depth);

	/* get the percent state change */
	else if(!strcmp(macro,"HOSTPERCENTCHANGE"))
		asprintf(&macro_ondemand,"%.2f",hst->percent_state_change);

	/* get the state duration in seconds */
	else if(!strcmp(macro,"HOSTDURATIONSEC"))
		asprintf(&macro_ondemand,"%lu",duration);

	/* get the state duration */
	else if(!strcmp(macro,"HOSTDURATION")){
		days=duration/86400;
		duration-=(days*86400);
		hours=duration/3600;
		duration-=(hours*3600);
		minutes=duration/60;
		duration-=(minutes*60);
		seconds=duration;
		asprintf(&macro_ondemand,"%dd %dh %dm %ds",days,hours,minutes,seconds);
	        }

	/* get the execution time macro */
	else if(!strcmp(macro,"HOSTEXECUTIONTIME"))
		asprintf(&macro_ondemand,"%.3f",hst->execution_time);

	/* get the latency macro */
	else if(!strcmp(macro,"HOSTLATENCY"))
		asprintf(&macro_ondemand,"%.3f",hst->latency);

	/* get the last check time macro */
	else if(!strcmp(macro,"LASTHOSTCHECK"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)hst->last_check);

	/* get the last state change time macro */
	else if(!strcmp(macro,"LASTHOSTSTATECHANGE"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)hst->last_state_change);

	/* get the last time up macro */
	else if(!strcmp(macro,"LASTHOSTUP"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)hst->last_time_up);

	/* get the last time down macro */
	else if(!strcmp(macro,"LASTHOSTDOWN"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)hst->last_time_down);

	/* get the last time unreachable macro */
	else if(!strcmp(macro,"LASTHOSTUNREACHABLE"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)hst->last_time_unreachable);

	/* get the notification number macro */
	else if(!strcmp(macro,"HOSTNOTIFICATIONNUMBER"))
		asprintf(&macro_ondemand,"%d",hst->current_notification_number);

	/* get the notification id macro */
	else if(!strcmp(macro,"HOSTNOTIFICATIONID"))
		asprintf(&macro_ondemand,"%lu",hst->current_notification_id);

	/* get the event id macro */
	else if(!strcmp(macro,"HOSTEVENTID"))
		asprintf(&macro_ondemand,"%lu",hst->current_event_id);

	/* get the last event id macro */
	else if(!strcmp(macro,"LASTHOSTEVENTID"))
		asprintf(&macro_ondemand,"%lu",hst->last_event_id);

	/* get the hostgroup names */
	else if(!strcmp(macro,"HOSTGROUPNAMES")){

		/* find all hostgroups this host is associated with */
		for(temp_objectlist=hst->hostgroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

			if((temp_hostgroup=(hostgroup *)temp_objectlist->object_ptr)==NULL)
				continue;

			asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_hostgroup->group_name);
			my_free(&buf2);
			buf2=buf1;
			}
		if(buf2){
			macro_ondemand=(char *)strdup(buf2);
			my_free(&buf2);
			}
		}

	/** NOTE: 09/06/07 other hostgroup macros are handled by grab_on_demand_hostgroup_macro(), and thus are not available in on-demand host macros **/
	/* get the hostgroup name */
	else if(!strcmp(macro,"HOSTGROUPNAME")){
		if(hst->hostgroups_ptr){
			temp_hostgroup=(hostgroup *)hst->hostgroups_ptr->object_ptr;
			macro_ondemand=(char *)strdup(temp_hostgroup->group_name);
			}
	        }
#endif

	/* action url */
	else if(!strcmp(macro,"HOSTACTIONURL")){

		if(hst->action_url)
			macro_ondemand=(char *)strdup(hst->action_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,URL_ENCODE_MACRO_CHARS);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

	/* notes url */
	else if(!strcmp(macro,"HOSTNOTESURL")){

		if(hst->notes_url)
			macro_ondemand=(char *)strdup(hst->notes_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,URL_ENCODE_MACRO_CHARS);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

	/* notes */
	else if(!strcmp(macro,"HOSTNOTES")){
		if(hst->notes)
			macro_ondemand=(char *)strdup(hst->notes);

		/* notes macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,0);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

#ifdef NSCORE
	/* service summary macros */
	/* NOTE: 08/14/07 These are not supported as "normal" host macros (only on-demand macros) at the moment */
	else if(strstr(macro,"TOTALHOSTSERVICES")==macro){

		for(temp_servicesmember=hst->services;temp_servicesmember!=NULL;temp_servicesmember=temp_servicesmember->next){
			if((temp_service=temp_servicesmember->service_ptr)==NULL)
				continue;

			total_host_services++;

			switch(temp_service->current_state){
			case STATE_OK:
				total_host_services_ok++;
				break;
			case STATE_WARNING:
				total_host_services_warning++;
				break;
			case STATE_UNKNOWN:
				total_host_services_unknown++;
				break;
			case STATE_CRITICAL:
				total_host_services_critical++;
				break;
			default:
				break;
				}
			}
		
		if(!strcmp(macro,"TOTALHOSTSERVICES"))
			asprintf(&macro_ondemand,"%lu",total_host_services);
		else if(!strcmp(macro,"TOTALHOSTSERVICESOK"))
			asprintf(&macro_ondemand,"%lu",total_host_services_ok);
		else if(!strcmp(macro,"TOTALHOSTSERVICESWARNING"))
			asprintf(&macro_ondemand,"%lu",total_host_services_warning);
		else if(!strcmp(macro,"TOTALHOSTSERVICESUNKNOWN"))
			asprintf(&macro_ondemand,"%lu",total_host_services_unknown);
		else if(!strcmp(macro,"TOTALHOSTSERVICESCRITICAL"))
			asprintf(&macro_ondemand,"%lu",total_host_services_critical);
		else
			return ERROR;
		}
#endif

	/* custom variables */
	else if(strstr(macro,"_HOST")==macro){
		
		/* get the variable name */
		if((customvarname=(char *)strdup(macro+5))){

			for(temp_customvariablesmember=hst->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){

				if(!strcmp(customvarname,temp_customvariablesmember->variable_name)){
					macro_ondemand=(char *)strdup(temp_customvariablesmember->variable_value);
					break;
				        }
			        }

			/* free memory */
			my_free(&customvarname);
		        }
	        }

	else
		return ERROR;

	return OK;
        }


/* grab an on-demand service macro */
int grab_on_demand_service_macro(service *svc, char *macro){
	servicegroup *temp_servicegroup=NULL;
	customvariablesmember *temp_customvariablesmember=NULL;
	objectlist *temp_objectlist=NULL;
	char *customvarname=NULL;
	char *temp_buffer=NULL;
	char *buf1=NULL;
	char *buf2=NULL;
	time_t current_time=0L;
	unsigned long duration=0L;
	int days=0;
	int hours=0;
	int minutes=0;
	int seconds=0;

	if(svc==NULL || macro==NULL)
		return ERROR;

	/* initialize the macro */
	macro_ondemand=NULL;

#ifdef NSCORE
	time(&current_time);
	duration=(unsigned long)(current_time-svc->last_state_change);
#endif

#ifdef NSCORE
	log_debug_info(DEBUGL_MACROS,2,"  Getting on-demand macro for service '%s' on host '%s': %s\n",svc->description,svc->host_name,macro);
#endif

	/* get the service display name */
	if(!strcmp(macro,"SERVICEDISPLAYNAME")){
		if(svc->display_name)
			macro_ondemand=(char *)strdup(svc->display_name);
	        }

	/* get the service check command */
	else if(!strcmp(macro,"SERVICECHECKCOMMAND"))
		macro_ondemand=(char *)strdup(svc->service_check_command);

#ifdef NSCORE
	/* get the plugin output */
	else if(!strcmp(macro,"SERVICEOUTPUT")){
		if(svc->plugin_output){
			macro_ondemand=(char *)strdup(svc->plugin_output);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* get the long plugin output */
	else if(!strcmp(macro,"LONGSERVICEOUTPUT")){
		if(svc->long_plugin_output){
			macro_ondemand=(char *)strdup(svc->long_plugin_output);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* get the performance data */
	else if(!strcmp(macro,"SERVICEPERFDATA")){
		if(svc->perf_data==NULL){
			macro_ondemand=(char *)strdup(svc->perf_data);
			/*strip(macro_ondemand);*/
		        }
	        }

	/* grab the servuce check type */
	else if(!strcmp(macro,"SERVICECHECKTYPE"))
		macro_ondemand=(char *)strdup((svc->check_type==SERVICE_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");

	/* grab the service state type macro (this is usually overridden later on) */
	else if(!strcmp(macro,"SERVICESTATETYPE"))
		macro_ondemand=(char *)strdup((svc->state_type==HARD_STATE)?"HARD":"SOFT");

	/* get the service state */
	else if(!strcmp(macro,"SERVICESTATE")){
		if(svc->current_state==STATE_OK)
			macro_ondemand=(char *)strdup("OK");
		else if(svc->current_state==STATE_WARNING)
			macro_ondemand=(char *)strdup("WARNING");
		else if(svc->current_state==STATE_CRITICAL)
			macro_ondemand=(char *)strdup("CRITICAL");
		else
			macro_ondemand=(char *)strdup("UNKNOWN");
	        }
#endif

	/* get the service volatility */
	else if(!strcmp(macro,"SERVICEISVOLATILE"))
		asprintf(&macro_ondemand,"%d",svc->is_volatile);

#ifdef NSCORE
	/* get the service state id */
	else if(!strcmp(macro,"SERVICESTATEID"))
		asprintf(&macro_ondemand,"%d",svc->current_state);

	/* get the current service check attempt macro */
	else if(!strcmp(macro,"SERVICEATTEMPT"))
		asprintf(&macro_ondemand,"%d",svc->current_attempt);

	/* get the max service check attempts macro */
	else if(!strcmp(macro,"MAXSERVICEATTEMPTS"))
		asprintf(&macro_ondemand,"%d",svc->max_attempts);

	/* get the execution time macro */
	else if(!strcmp(macro,"SERVICEEXECUTIONTIME"))
		asprintf(&macro_ondemand,"%.3f",svc->execution_time);

	/* get the latency macro */
	else if(!strcmp(macro,"SERVICELATENCY"))
		asprintf(&macro_ondemand,"%.3f",svc->latency);

	/* get the last check time macro */
	else if(!strcmp(macro,"LASTSERVICECHECK"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_check);

	/* get the last state change time macro */
	else if(!strcmp(macro,"LASTSERVICESTATECHANGE"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_state_change);

	/* get the last time ok macro */
	else if(!strcmp(macro,"LASTSERVICEOK"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_time_ok);

	/* get the last time warning macro */
	else if(!strcmp(macro,"LASTSERVICEWARNING"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_time_warning);

	/* get the last time unknown macro */
	else if(!strcmp(macro,"LASTSERVICEUNKNOWN"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_time_unknown);

	/* get the last time critical macro */
	else if(!strcmp(macro,"LASTSERVICECRITICAL"))
		asprintf(&macro_ondemand,"%lu",(unsigned long)svc->last_time_critical);

	/* get the service downtime depth */
	else if(!strcmp(macro,"SERVICEDOWNTIME"))
		asprintf(&macro_ondemand,"%d",svc->scheduled_downtime_depth);

	/* get the percent state change */
	else if(!strcmp(macro,"SERVICEPERCENTCHANGE"))
		asprintf(&macro_ondemand,"%.2f",svc->percent_state_change);

	/* get the state duration in seconds */
	else if(!strcmp(macro,"SERVICEDURATIONSEC"))
		asprintf(&macro_ondemand,"%lu",duration);

	/* get the state duration */
	else if(!strcmp(macro,"SERVICEDURATION")){
		days=duration/86400;
		duration-=(days*86400);
		hours=duration/3600;
		duration-=(hours*3600);
		minutes=duration/60;
		duration-=(minutes*60);
		seconds=duration;
		asprintf(&macro_ondemand,"%dd %dh %dm %ds",days,hours,minutes,seconds);
	        }

	/* get the notification number macro */
	else if(!strcmp(macro,"SERVICENOTIFICATIONNUMBER"))
		asprintf(&macro_ondemand,"%d",svc->current_notification_number);

	/* get the notification id macro */
	else if(!strcmp(macro,"SERVICENOTIFICATIONID"))
		asprintf(&macro_ondemand,"%lu",svc->current_notification_id);

	/* get the event id macro */
	else if(!strcmp(macro,"SERVICEEVENTID"))
		asprintf(&macro_ondemand,"%lu",svc->current_event_id);

	/* get the event id macro */
	else if(!strcmp(macro,"LASTSERVICEEVENTID"))
		asprintf(&macro_ondemand,"%lu",svc->last_event_id);

	/* get the servicegroup names */
	else if(!strcmp(macro,"SERVICEGROUPNAMES")){

		/* find all servicegroups this service is associated with */
		for(temp_objectlist=svc->servicegroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

			if((temp_servicegroup=(servicegroup *)temp_objectlist->object_ptr)==NULL)
				continue;

			asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_servicegroup->group_name);
			my_free(&buf2);
			buf2=buf1;
			}
		if(buf2){
			macro_ondemand=(char *)strdup(buf2);
			my_free(&buf2);
			}
		}

	/** NOTE: 09/06/07 other servicegroup macros are handled by grab_on_demand_servicegroup_macro(), and thus are not available in on-demand service macros **/
	/* get the servicegroup name */
	else if(!strcmp(macro,"SERVICEGROUPNAME")){
		if(svc->servicegroups_ptr){
			temp_servicegroup=(servicegroup *)svc->servicegroups_ptr->object_ptr;
			macro_ondemand=(char *)strdup(temp_servicegroup->group_name);
			}
	        }
#endif

	/* action url */
	else if(!strcmp(macro,"SERVICEACTIONURL")){
		if(svc->action_url)
			macro_ondemand=(char *)strdup(svc->action_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,URL_ENCODE_MACRO_CHARS);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

	/* notes url */
	else if(!strcmp(macro,"SERVICENOTESURL")){

		if(svc->notes_url)
			macro_ondemand=(char *)strdup(svc->notes_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,URL_ENCODE_MACRO_CHARS);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

	/* notes */
	else if(!strcmp(macro,"SERVICENOTES")){
		if(svc->notes)
			macro_ondemand=(char *)strdup(svc->notes);

		/* notes macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,0);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

	/* custom variables */
	else if(strstr(macro,"_SERVICE")==macro){
		
		/* get the variable name */
		if((customvarname=(char *)strdup(macro+8))){

			for(temp_customvariablesmember=svc->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){

				if(!strcmp(customvarname,temp_customvariablesmember->variable_name)){
					macro_ondemand=(char *)strdup(temp_customvariablesmember->variable_value);
					break;
				        }
			        }

			/* free memory */
			my_free(&customvarname);
		        }
	        }

	else
		return ERROR;

	return OK;
        }



/* grab an on-demand contact macro */
int grab_on_demand_contact_macro(contact *cntct, char *macro){
	contactgroup *temp_contactgroup=NULL;
	customvariablesmember *temp_customvariablesmember=NULL;
	objectlist *temp_objectlist=NULL;
	char *customvarname=NULL;
	char *buf1=NULL;
	char *buf2=NULL;
	register int x=0;
	register int address=0;

	if(cntct==NULL || macro==NULL)
		return ERROR;

	/* initialize the macro */
	macro_ondemand=NULL;

#ifdef NSCORE
	log_debug_info(DEBUGL_MACROS,2,"  Getting on-demand macro for contact '%s'\n",cntct->name,macro);
#endif

	/* get the name */
	my_free(&macro_x[MACRO_CONTACTNAME]);
	macro_x[MACRO_CONTACTNAME]=(char *)strdup(cntct->name);

	/* get the alias */
	if(!strcmp(macro,"CONTACTALIAS")){
		if(cntct->alias)
			macro_ondemand=(char *)strdup(cntct->alias);
	        }

	/* get the email address */
	else if(!strcmp(macro,"CONTACTALIAS")){
		if(cntct->email)
			macro_ondemand=(char *)strdup(cntct->email);
		}

	/* get the pager number */
	else if(!strcmp(macro,"CONTACTPAGER")){
		if(cntct->pager)
			macro_ondemand=(char *)strdup(cntct->pager);
		}

	/* get misc contact addresses */
	else if(strstr(macro,"CONTACTADDRESS")==macro){
		address=atoi(macro+14);
		for(x=0;x<MAX_CONTACT_ADDRESSES;x++){
			if(x==address && cntct->address[x]){
				macro_ondemand=(char *)strdup(cntct->address[x]);
				}
			}
		}

#ifdef NSCORE
	/** NOTE: 09/06/07 other contactgroup macros are handled by grab_on_demand_contactgroup_macro(), and thus are not available in on-demand contact macros **/
	/* get the contactgroup names */
	/* find all contactgroups this contact is a member of */
	else if(!strcmp(macro,"CONTACTGROUPNAMES")){

		for(temp_objectlist=cntct->contactgroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

			if((temp_contactgroup=(contactgroup *)temp_objectlist->object_ptr)==NULL)
				continue;

			asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_contactgroup->group_name);
			my_free(&buf2);
			buf2=buf1;
			}
		if(buf2){
			macro_ondemand=(char *)strdup(buf2);
			my_free(&buf2);
			}
		}

	/* get first/primary contactgroup macros */
	else if(!strcmp(macro,"CONTACTGROUPNAME")){
		if(cntct->contactgroups_ptr){
			temp_contactgroup=(contactgroup *)cntct->contactgroups_ptr->object_ptr;
			macro_ondemand=(char *)strdup(temp_contactgroup->group_name);
			}
		}
#endif

	/* custom variables */
	else if(strstr(macro,"_CONTACT")==macro){
		
		/* get the variable name */
		if((customvarname=(char *)strdup(macro+8))){

			for(temp_customvariablesmember=cntct->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){

				if(!strcmp(customvarname,temp_customvariablesmember->variable_name)){
					macro_ondemand=(char *)strdup(temp_customvariablesmember->variable_value);
					break;
				        }
			        }

			/* free memory */
			my_free(&customvarname);
		        }
	        }

	return OK;
	}



/* grab an on-demand hostgroup macro */
int grab_on_demand_hostgroup_macro(hostgroup *hg, char *macro){
	hostgroup *temp_hostgroup=NULL;
	hostsmember *temp_hostsmember=NULL;
	char *temp_buffer=NULL;

	if(hg==NULL || macro==NULL)
		return ERROR;

	/* initialize the macro */
	macro_ondemand=NULL;

#ifdef NSCORE
	log_debug_info(DEBUGL_MACROS,2,"  Getting on-demand macro for hostgroup '%s': %s\n",hg->group_name,macro);
#endif

	/* get the group alias */
	if(!strcmp(macro,"HOSTGROUPALIAS")){
		if(hg->alias)
			macro_ondemand=(char *)strdup(hg->alias);
		}

	/* get the member list */
	else if(!strcmp(macro,"HOSTGROUPMEMBERS")){
#ifdef NSCORE
		for(temp_hostsmember=hg->members;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){
			if(macro_ondemand==NULL)
				macro_ondemand=(char *)strdup(temp_hostsmember->host_name);
			else if((macro_ondemand=(char *)realloc(macro_ondemand,strlen(macro_ondemand)+strlen(temp_hostsmember->host_name)+2))){
				strcat(macro_ondemand,",");
				strcat(macro_ondemand,temp_hostsmember->host_name);
				}
			}
#endif
	        }

	/* action url */
	else if(!strcmp(macro,"HOSTGROUPACTIONURL")){

		if(hg->action_url)
			macro_ondemand=(char *)strdup(hg->action_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,URL_ENCODE_MACRO_CHARS);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

	/* notes url */
	else if(!strcmp(macro,"HOSTGROUPNOTESURL")){

		if(hg->notes_url)
			macro_ondemand=(char *)strdup(hg->notes_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,URL_ENCODE_MACRO_CHARS);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

	/* notes */
	else if(!strcmp(macro,"HOSTGROUPNOTES")){
		if(hg->notes)
			macro_ondemand=(char *)strdup(hg->notes);

		/* notes macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,0);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

	else
		return ERROR;

	return OK;
	}



/* grab an on-demand servicegroup macro */
int grab_on_demand_servicegroup_macro(servicegroup *sg, char *macro){
	servicegroup *temp_servicegroup=NULL;
	servicesmember *temp_servicesmember=NULL;
	char *temp_buffer=NULL;

	if(sg==NULL || macro==NULL)
		return ERROR;

	/* initialize the macro */
	macro_ondemand=NULL;

#ifdef NSCORE
	log_debug_info(DEBUGL_MACROS,2,"  Getting on-demand macro for servicegroup '%s': %s\n",sg->group_name,macro);
#endif

	/* get the group alias */
	if(!strcmp(macro,"SERVICEGROUPALIAS")){
		if(sg->alias)
			macro_ondemand=(char *)strdup(sg->alias);
		}

	/* get the member list */
	else if(!strcmp(macro,"SERVICEGROUPMEMBERS")){
#ifdef NSCORE
		for(temp_servicesmember=sg->members;temp_servicesmember!=NULL;temp_servicesmember=temp_servicesmember->next){
			if(macro_ondemand==NULL){
				if((macro_ondemand=(char *)malloc(strlen(temp_servicesmember->host_name)+strlen(temp_servicesmember->service_description)+1))){
					sprintf(macro_ondemand,"%s,%s",temp_servicesmember->host_name,temp_servicesmember->service_description);
					}
				}
			else if((macro_ondemand=(char *)realloc(macro_ondemand,strlen(macro_ondemand)+strlen(temp_servicesmember->host_name)+strlen(temp_servicesmember->service_description)+3))){
				strcat(macro_ondemand,",");
				strcat(macro_ondemand,temp_servicesmember->host_name);
				strcat(macro_ondemand,",");
				strcat(macro_ondemand,temp_servicesmember->service_description);
				}
			}
#endif
	        }

	/* action url */
	else if(!strcmp(macro,"SERVICEGROUPACTIONURL")){

		if(sg->action_url)
			macro_ondemand=(char *)strdup(sg->action_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,URL_ENCODE_MACRO_CHARS);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

	/* notes url */
	else if(!strcmp(macro,"SERVICEGROUPNOTESURL")){

		if(sg->notes_url)
			macro_ondemand=(char *)strdup(sg->notes_url);

		/* action URL macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,URL_ENCODE_MACRO_CHARS);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

	/* notes */
	else if(!strcmp(macro,"SERVICEGROUPNOTES")){
		if(sg->notes)
			macro_ondemand=(char *)strdup(sg->notes);

		/* notes macros may themselves contain macros, so process them... */
		if(macro_ondemand!=NULL){
			process_macros(macro_ondemand,&temp_buffer,0);
			my_free(&macro_ondemand);
			macro_ondemand=temp_buffer;
		        }
	        }

	else
		return ERROR;

	return OK;
	}



/* grab an on-demand contactgroup macro */
int grab_on_demand_contactgroup_macro(contactgroup *cg, char *macro){
	contactgroup *temp_contactgroup=NULL;
	contactsmember *temp_contactsmember=NULL;

	if(cg==NULL || macro==NULL)
		return ERROR;

	/* initialize the macro */
	macro_ondemand=NULL;

#ifdef NSCORE
	log_debug_info(DEBUGL_MACROS,2,"  Getting on-demand macro for contactgroup '%s': %s\n",cg->group_name,macro);
#endif

	/* get the group alias */
	if(!strcmp(macro,"CONTACTGROUPALIAS")){
		if(cg->alias)
			macro_ondemand=(char *)strdup(cg->alias);
		}

	/* get the member list */
	else if(!strcmp(macro,"CONTACTGROUPMEMBERS")){
#ifdef NSCORE
		for(temp_contactsmember=cg->members;temp_contactsmember!=NULL;temp_contactsmember=temp_contactsmember->next){
			if(macro_ondemand==NULL)
				macro_ondemand=(char *)strdup(temp_contactsmember->contact_name);
			else if((macro_ondemand=(char *)realloc(macro_ondemand,strlen(macro_ondemand)+strlen(temp_contactsmember->contact_name)+2))){
				strcat(macro_ondemand,",");
				strcat(macro_ondemand,temp_contactsmember->contact_name);
				}
			}
#endif
	        }

	else
		return ERROR;

	return OK;
	}



/* grab an on-demand time macro */
int grab_on_demand_time_macro(char *macro, char *tp, char *ts){
	timeperiod *temp_timeperiod=NULL;
	time_t test_time=0L;
	time_t next_valid_time=0L;
	char *temp_buffer=NULL;

	if(macro==NULL || tp==NULL)
		return ERROR;

	/* initialize the macro */
	macro_ondemand=NULL;

#ifdef NSCORE
	log_debug_info(DEBUGL_MACROS,2,"  Getting on-demand time macro '%s'\n",macro);
#endif

#ifdef NSCORE
	/* find the timeperiod */
	if((temp_timeperiod=find_timeperiod(tp))==NULL)
		return ERROR;

	/* what timestamp should we use? */
	if(ts)
		test_time=(time_t)strtoul(ts,NULL,0);
	else
		time(&test_time);

	/* is valid time? */
	if(!strcmp(macro,"ISVALIDTIME"))
		asprintf(&macro_ondemand,"%d",(check_time_against_period(test_time,temp_timeperiod)==OK)?1:0);
	
	/* get next valid time */
	else if(!strcmp(macro,"NEXTVALIDTIME")){
		get_next_valid_time(test_time,&next_valid_time,temp_timeperiod);
		if(next_valid_time==test_time && check_time_against_period(test_time,temp_timeperiod)==ERROR)
			next_valid_time=(time_t)0L;
		asprintf(&macro_ondemand,"%lu",(unsigned long)next_valid_time);
		}
#endif

	return OK;
	}



/* grab summary macros (filtered for a specific contact) */
int grab_summary_macros(contact *temp_contact){
	host *temp_host=NULL;
	service  *temp_service=NULL;
	int authorized=TRUE;
	int problem=TRUE;
	int hosts_up=0;
	int hosts_down=0;
	int hosts_unreachable=0;
	int hosts_down_unhandled=0;
	int hosts_unreachable_unhandled=0;
	int host_problems=0;
	int host_problems_unhandled=0;
	int services_ok=0;
	int services_warning=0;
	int services_unknown=0;
	int services_critical=0;
	int services_warning_unhandled=0;
	int services_unknown_unhandled=0;
	int services_critical_unhandled=0;
	int service_problems=0;
	int service_problems_unhandled=0;

#ifdef NSCORE
	/* this func seems to take up quite a bit of CPU, so skip it if we have a large install... */
	if(use_large_installation_tweaks==TRUE)
		return OK;

	/* get host totals */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* filter totals based on contact if necessary */
		if(temp_contact!=NULL)
			authorized=is_contact_for_host(temp_host,temp_contact);

		if(authorized==TRUE){
			problem=TRUE;
			
			if(temp_host->current_state==HOST_UP && temp_host->has_been_checked==TRUE)
				hosts_up++;
			else if(temp_host->current_state==HOST_DOWN){
				if(temp_host->scheduled_downtime_depth>0)
					problem=FALSE;
				if(temp_host->problem_has_been_acknowledged==TRUE)
					problem=FALSE;
				if(temp_host->checks_enabled==FALSE)
					problem=FALSE;
				if(problem==TRUE)
					hosts_down_unhandled++;
				hosts_down++;
				}
			else if(temp_host->current_state==HOST_UNREACHABLE){
				if(temp_host->scheduled_downtime_depth>0)
					problem=FALSE;
				if(temp_host->problem_has_been_acknowledged==TRUE)
					problem=FALSE;
				if(temp_host->checks_enabled==FALSE)
					problem=FALSE;
				if(problem==TRUE)
					hosts_down_unhandled++;
				hosts_unreachable++;
				}
			}
		}

	host_problems=hosts_down+hosts_unreachable;
	host_problems_unhandled=hosts_down_unhandled+hosts_unreachable_unhandled;

	/* get service totals */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		/* filter totals based on contact if necessary */
		if(temp_contact!=NULL)
			authorized=is_contact_for_service(temp_service,temp_contact);

		if(authorized==TRUE){
			problem=TRUE;
			
			if(temp_service->current_state==STATE_OK && temp_service->has_been_checked==TRUE)
				services_ok++;
			else if(temp_service->current_state==STATE_WARNING){
				temp_host=find_host(temp_service->host_name);
				if(temp_host!=NULL && (temp_host->current_state==HOST_DOWN || temp_host->current_state==HOST_UNREACHABLE))
					problem=FALSE;
				if(temp_service->scheduled_downtime_depth>0)
					problem=FALSE;
				if(temp_service->problem_has_been_acknowledged==TRUE)
					problem=FALSE;
				if(temp_service->checks_enabled==FALSE)
					problem=FALSE;
				if(problem==TRUE)
					services_warning_unhandled++;
				services_warning++;
		        	}
			else if(temp_service->current_state==STATE_UNKNOWN){
				temp_host=find_host(temp_service->host_name);
				if(temp_host!=NULL && (temp_host->current_state==HOST_DOWN || temp_host->current_state==HOST_UNREACHABLE))
					problem=FALSE;
				if(temp_service->scheduled_downtime_depth>0)
					problem=FALSE;
				if(temp_service->problem_has_been_acknowledged==TRUE)
					problem=FALSE;
				if(temp_service->checks_enabled==FALSE)
					problem=FALSE;
				if(problem==TRUE)
					services_unknown_unhandled++;
				services_unknown++;
				}
			else if(temp_service->current_state==STATE_CRITICAL){
				temp_host=find_host(temp_service->host_name);
				if(temp_host!=NULL && (temp_host->current_state==HOST_DOWN || temp_host->current_state==HOST_UNREACHABLE))
					problem=FALSE;
				if(temp_service->scheduled_downtime_depth>0)
					problem=FALSE;
				if(temp_service->problem_has_been_acknowledged==TRUE)
					problem=FALSE;
				if(temp_service->checks_enabled==FALSE)
					problem=FALSE;
				if(problem==TRUE)
					services_critical_unhandled++;
				services_critical++;
				}
			}
		}

	service_problems=services_warning+services_critical+services_unknown;
	service_problems_unhandled=services_warning_unhandled+services_critical_unhandled+services_unknown_unhandled;


	/* get total hosts up */
	my_free(&macro_x[MACRO_TOTALHOSTSUP]);
	asprintf(&macro_x[MACRO_TOTALHOSTSUP],"%d",hosts_up);

	/* get total hosts down */
	my_free(&macro_x[MACRO_TOTALHOSTSDOWN]);
	asprintf(&macro_x[MACRO_TOTALHOSTSDOWN],"%d",hosts_down);

	/* get total hosts unreachable */
	my_free(&macro_x[MACRO_TOTALHOSTSUNREACHABLE]);
	asprintf(&macro_x[MACRO_TOTALHOSTSUNREACHABLE],"%d",hosts_unreachable);

	/* get total unhandled hosts down */
	my_free(&macro_x[MACRO_TOTALHOSTSDOWNUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALHOSTSDOWNUNHANDLED],"%d",hosts_down_unhandled);

	/* get total unhandled hosts unreachable */
	my_free(&macro_x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED],"%d",hosts_unreachable_unhandled);

	/* get total host problems */
	my_free(&macro_x[MACRO_TOTALHOSTPROBLEMS]);
	asprintf(&macro_x[MACRO_TOTALHOSTPROBLEMS],"%d",host_problems);

	/* get total unhandled host problems */
	my_free(&macro_x[MACRO_TOTALHOSTPROBLEMSUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALHOSTPROBLEMSUNHANDLED],"%d",host_problems_unhandled);

	/* get total services ok */
	my_free(&macro_x[MACRO_TOTALSERVICESOK]);
	asprintf(&macro_x[MACRO_TOTALSERVICESOK],"%d",services_ok);

	/* get total services warning */
	my_free(&macro_x[MACRO_TOTALSERVICESWARNING]);
	asprintf(&macro_x[MACRO_TOTALSERVICESWARNING],"%d",services_warning);

	/* get total services critical */
	my_free(&macro_x[MACRO_TOTALSERVICESCRITICAL]);
	asprintf(&macro_x[MACRO_TOTALSERVICESCRITICAL],"%d",services_critical);

	/* get total services unknown */
	my_free(&macro_x[MACRO_TOTALSERVICESUNKNOWN]);
	asprintf(&macro_x[MACRO_TOTALSERVICESUNKNOWN],"%d",services_unknown);

	/* get total unhandled services warning */
	my_free(&macro_x[MACRO_TOTALSERVICESWARNINGUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALSERVICESWARNINGUNHANDLED],"%d",services_warning_unhandled);

	/* get total unhandled services critical */
	my_free(&macro_x[MACRO_TOTALSERVICESCRITICALUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALSERVICESCRITICALUNHANDLED],"%d",services_critical_unhandled);

	/* get total unhandled services unknown */
	my_free(&macro_x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED],"%d",services_unknown_unhandled);

	/* get total service problems */
	my_free(&macro_x[MACRO_TOTALSERVICEPROBLEMS]);
	asprintf(&macro_x[MACRO_TOTALSERVICEPROBLEMS],"%d",service_problems);

	/* get total unhandled service problems */
	my_free(&macro_x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED]);
	asprintf(&macro_x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED],"%d",service_problems_unhandled);
#endif

	return OK;
        }



/* updates date/time macros */
int grab_datetime_macros(void){
	time_t t=0L;

	/* get the current time */
	time(&t);

	/* get the current date/time (long format macro) */
	if(macro_x[MACRO_LONGDATETIME]==NULL)
		macro_x[MACRO_LONGDATETIME]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_LONGDATETIME]!=NULL)
		get_datetime_string(&t,macro_x[MACRO_LONGDATETIME],MAX_DATETIME_LENGTH,LONG_DATE_TIME);

	/* get the current date/time (short format macro) */
	if(macro_x[MACRO_SHORTDATETIME]==NULL)
		macro_x[MACRO_SHORTDATETIME]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_SHORTDATETIME]!=NULL)
		get_datetime_string(&t,macro_x[MACRO_SHORTDATETIME],MAX_DATETIME_LENGTH,SHORT_DATE_TIME);

	/* get the short format date macro */
	if(macro_x[MACRO_DATE]==NULL)
		macro_x[MACRO_DATE]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_DATE]!=NULL)
		get_datetime_string(&t,macro_x[MACRO_DATE],MAX_DATETIME_LENGTH,SHORT_DATE);

	/* get the short format time macro */
	if(macro_x[MACRO_TIME]==NULL)
		macro_x[MACRO_TIME]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_TIME]!=NULL)
		get_datetime_string(&t,macro_x[MACRO_TIME],MAX_DATETIME_LENGTH,SHORT_TIME);

	/* get the time_t format time macro */
	if(macro_x[MACRO_TIMET]==NULL)
		macro_x[MACRO_TIMET]=(char *)malloc(MAX_DATETIME_LENGTH);
	if(macro_x[MACRO_TIMET]!=NULL){
		snprintf(macro_x[MACRO_TIMET],MAX_DATETIME_LENGTH,"%lu",(unsigned long)t);
		macro_x[MACRO_TIMET][MAX_DATETIME_LENGTH-1]='\x0';
	        }

	/*
	strip(macro_x[MACRO_LONGDATETIME]);
	strip(macro_x[MACRO_SHORTDATETIME]);
	strip(macro_x[MACRO_DATE]);
	strip(macro_x[MACRO_TIME]);
	strip(macro_x[MACRO_TIMET]);
	*/

	return OK;
        }



/* cleans illegal characters in macros before output */
char *clean_macro_chars(char *macro,int options){
	register int x=0;
	register int y=0;
	register int z=0;
	register int ch=0;
	register int len=0;
	register int illegal_char=0;

	if(macro==NULL)
		return "";

	len=(int)strlen(macro);

	/* strip illegal characters out of macro */
	if(options & STRIP_ILLEGAL_MACRO_CHARS){

		for(y=0,x=0;x<len;x++){
			
			ch=(int)macro[x];

			/* illegal ASCII characters */
			if(ch<32 || ch==127)
				continue;

			/* REMOVED 3/11/05 to allow for non-english spellings, etc. */
			/* illegal extended ASCII characters */
			/*
			if(ch>=166)
				continue;
			*/

			/* illegal user-specified characters */
			illegal_char=FALSE;
			if(illegal_output_chars!=NULL){
				for(z=0;illegal_output_chars[z]!='\x0';z++){
					if(ch==(int)illegal_output_chars[z]){
						illegal_char=TRUE;
						break;
					        }
				        }
			        }
				
		        if(illegal_char==FALSE)
				macro[y++]=macro[x];
		        }

		macro[y++]='\x0';
	        }

#ifdef ON_HOLD_FOR_NOW
	/* escape nasty character in macro */
	if(options & ESCAPE_MACRO_CHARS){
	        }
#endif

	return macro;
        }



/* encodes a string in proper URL format */
char *get_url_encoded_string(char *input){
	register int x=0;
	register int y=0;
	char *encoded_url_string=NULL;
	char temp_expansion[4]="";


	/* bail if no input */
	if(input==NULL)
		return NULL;

	/* allocate enough memory to escape all characters if necessary */
	if((encoded_url_string=(char *)malloc((strlen(input)*3)+1))==NULL)
		return NULL;

	/* check/encode all characters */
	for(x=0,y=0;input[x]!=(char)'\x0';x++){

		/* alpha-numeric characters and a few other characters don't get encoded */
		if(((char)input[x]>='0' && (char)input[x]<='9') || ((char)input[x]>='A' && (char)input[x]<='Z') || ((char)input[x]>=(char)'a' && (char)input[x]<=(char)'z') || (char)input[x]==(char)'.' || (char)input[x]==(char)'-' || (char)input[x]==(char)'_'){
			encoded_url_string[y]=input[x];
			y++;
		        }

		/* spaces are pluses */
		else if((char)input[x]<=(char)' '){
			encoded_url_string[y]='+';
			y++;
		        }

		/* anything else gets represented by its hex value */
		else{
			encoded_url_string[y]='\x0';
			sprintf(temp_expansion,"%%%02X",(unsigned int)input[x]);
			strcat(encoded_url_string,temp_expansion);
			y+=3;
		        }
	        }

	/* terminate encoded string */
	encoded_url_string[y]='\x0';

	return encoded_url_string;
        }



/******************************************************************/
/********************* DAEMON MACRO FUNCTIONS *********************/
/******************************************************************/

/* initializes macros */
int init_macros(void){
	register int x=0;

	/* macro names */
	init_macrox_names();

	/* contact address macros */
	for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
		macro_contactaddress[x]=NULL;

	/* on-demand macro */
	macro_ondemand=NULL;

	/* ARGx macros */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++)
		macro_argv[x]=NULL;

	/* custom object variable macros */
	macro_custom_host_vars=NULL;
	macro_custom_service_vars=NULL;
	macro_custom_contact_vars=NULL;
	}



/* initializes the names of macros */
int init_macrox_names(void){
	register int x=0;

	/* initialize macro names */
	for(x=0;x<MACRO_X_COUNT;x++)
		macro_x_names[x]=NULL;

	/* initialize each macro name */
	add_macrox_name(MACRO_HOSTNAME,"HOSTNAME");
	add_macrox_name(MACRO_HOSTALIAS,"HOSTALIAS");
	add_macrox_name(MACRO_HOSTADDRESS,"HOSTADDRESS");
	add_macrox_name(MACRO_SERVICEDESC,"SERVICEDESC");
	add_macrox_name(MACRO_SERVICESTATE,"SERVICESTATE");
	add_macrox_name(MACRO_SERVICESTATEID,"SERVICESTATEID");
	add_macrox_name(MACRO_SERVICEATTEMPT,"SERVICEATTEMPT");
	add_macrox_name(MACRO_SERVICEISVOLATILE,"SERVICEISVOLATILE");
	add_macrox_name(MACRO_LONGDATETIME,"LONGDATETIME");
	add_macrox_name(MACRO_SHORTDATETIME,"SHORTDATETIME");
	add_macrox_name(MACRO_DATE,"DATE");
	add_macrox_name(MACRO_TIME,"TIME");
	add_macrox_name(MACRO_TIMET,"TIMET");
	add_macrox_name(MACRO_LASTHOSTCHECK,"LASTHOSTCHECK");
	add_macrox_name(MACRO_LASTSERVICECHECK,"LASTSERVICECHECK");
	add_macrox_name(MACRO_LASTHOSTSTATECHANGE,"LASTHOSTSTATECHANGE");
	add_macrox_name(MACRO_LASTSERVICESTATECHANGE,"LASTSERVICESTATECHANGE");
	add_macrox_name(MACRO_HOSTOUTPUT,"HOSTOUTPUT");
	add_macrox_name(MACRO_SERVICEOUTPUT,"SERVICEOUTPUT");
	add_macrox_name(MACRO_HOSTPERFDATA,"HOSTPERFDATA");
	add_macrox_name(MACRO_SERVICEPERFDATA,"SERVICEPERFDATA");
	add_macrox_name(MACRO_CONTACTNAME,"CONTACTNAME");
	add_macrox_name(MACRO_CONTACTALIAS,"CONTACTALIAS");
	add_macrox_name(MACRO_CONTACTEMAIL,"CONTACTEMAIL");
	add_macrox_name(MACRO_CONTACTPAGER,"CONTACTPAGER");
	add_macrox_name(MACRO_ADMINEMAIL,"ADMINEMAIL");
	add_macrox_name(MACRO_ADMINPAGER,"ADMINPAGER");
	add_macrox_name(MACRO_HOSTSTATE,"HOSTSTATE");
	add_macrox_name(MACRO_HOSTSTATEID,"HOSTSTATEID");
	add_macrox_name(MACRO_HOSTATTEMPT,"HOSTATTEMPT");
	add_macrox_name(MACRO_NOTIFICATIONTYPE,"NOTIFICATIONTYPE");
	add_macrox_name(MACRO_NOTIFICATIONNUMBER,"NOTIFICATIONNUMBER");
	add_macrox_name(MACRO_HOSTEXECUTIONTIME,"HOSTEXECUTIONTIME");
	add_macrox_name(MACRO_SERVICEEXECUTIONTIME,"SERVICEEXECUTIONTIME");
	add_macrox_name(MACRO_HOSTLATENCY,"HOSTLATENCY");
	add_macrox_name(MACRO_SERVICELATENCY,"SERVICELATENCY");
	add_macrox_name(MACRO_HOSTDURATION,"HOSTDURATION");
	add_macrox_name(MACRO_SERVICEDURATION,"SERVICEDURATION");
	add_macrox_name(MACRO_HOSTDURATIONSEC,"HOSTDURATIONSEC");
	add_macrox_name(MACRO_SERVICEDURATIONSEC,"SERVICEDURATIONSEC");
	add_macrox_name(MACRO_HOSTDOWNTIME,"HOSTDOWNTIME");
	add_macrox_name(MACRO_SERVICEDOWNTIME,"SERVICEDOWNTIME");
	add_macrox_name(MACRO_HOSTSTATETYPE,"HOSTSTATETYPE");
	add_macrox_name(MACRO_SERVICESTATETYPE,"SERVICESTATETYPE");
	add_macrox_name(MACRO_HOSTPERCENTCHANGE,"HOSTPERCENTCHANGE");
	add_macrox_name(MACRO_SERVICEPERCENTCHANGE,"SERVICEPERCENTCHANGE");
	add_macrox_name(MACRO_HOSTGROUPNAME,"HOSTGROUPNAME");
	add_macrox_name(MACRO_HOSTGROUPALIAS,"HOSTGROUPALIAS");
	add_macrox_name(MACRO_SERVICEGROUPNAME,"SERVICEGROUPNAME");
	add_macrox_name(MACRO_SERVICEGROUPALIAS,"SERVICEGROUPALIAS");
	add_macrox_name(MACRO_HOSTACKAUTHOR,"HOSTACKAUTHOR");
	add_macrox_name(MACRO_HOSTACKCOMMENT,"HOSTACKCOMMENT");
	add_macrox_name(MACRO_SERVICEACKAUTHOR,"SERVICEACKAUTHOR");
	add_macrox_name(MACRO_SERVICEACKCOMMENT,"SERVICEACKCOMMENT");
	add_macrox_name(MACRO_LASTSERVICEOK,"LASTSERVICEOK");
	add_macrox_name(MACRO_LASTSERVICEWARNING,"LASTSERVICEWARNING");
	add_macrox_name(MACRO_LASTSERVICEUNKNOWN,"LASTSERVICEUNKNOWN");
	add_macrox_name(MACRO_LASTSERVICECRITICAL,"LASTSERVICECRITICAL");
	add_macrox_name(MACRO_LASTHOSTUP,"LASTHOSTUP");
	add_macrox_name(MACRO_LASTHOSTDOWN,"LASTHOSTDOWN");
	add_macrox_name(MACRO_LASTHOSTUNREACHABLE,"LASTHOSTUNREACHABLE");
	add_macrox_name(MACRO_SERVICECHECKCOMMAND,"SERVICECHECKCOMMAND");
	add_macrox_name(MACRO_HOSTCHECKCOMMAND,"HOSTCHECKCOMMAND");
	add_macrox_name(MACRO_MAINCONFIGFILE,"MAINCONFIGFILE");
	add_macrox_name(MACRO_STATUSDATAFILE,"STATUSDATAFILE");
	add_macrox_name(MACRO_HOSTDISPLAYNAME,"HOSTDISPLAYNAME");
	add_macrox_name(MACRO_SERVICEDISPLAYNAME,"SERVICEDISPLAYNAME");
	add_macrox_name(MACRO_RETENTIONDATAFILE,"RETENTIONDATAFILE");
	add_macrox_name(MACRO_OBJECTCACHEFILE,"OBJECTCACHEFILE");
	add_macrox_name(MACRO_TEMPFILE,"TEMPFILE");
	add_macrox_name(MACRO_LOGFILE,"LOGFILE");
	add_macrox_name(MACRO_RESOURCEFILE,"RESOURCEFILE");
	add_macrox_name(MACRO_COMMANDFILE,"COMMANDFILE");
	add_macrox_name(MACRO_HOSTPERFDATAFILE,"HOSTPERFDATAFILE");
	add_macrox_name(MACRO_SERVICEPERFDATAFILE,"SERVICEPERFDATAFILE");
	add_macrox_name(MACRO_HOSTACTIONURL,"HOSTACTIONURL");
	add_macrox_name(MACRO_HOSTNOTESURL,"HOSTNOTESURL");
	add_macrox_name(MACRO_HOSTNOTES,"HOSTNOTES");
	add_macrox_name(MACRO_SERVICEACTIONURL,"SERVICEACTIONURL");
	add_macrox_name(MACRO_SERVICENOTESURL,"SERVICENOTESURL");
	add_macrox_name(MACRO_SERVICENOTES,"SERVICENOTES");
	add_macrox_name(MACRO_TOTALHOSTSUP,"TOTALHOSTSUP");
	add_macrox_name(MACRO_TOTALHOSTSDOWN,"TOTALHOSTSDOWN");
	add_macrox_name(MACRO_TOTALHOSTSUNREACHABLE,"TOTALHOSTSUNREACHABLE");
	add_macrox_name(MACRO_TOTALHOSTSDOWNUNHANDLED,"TOTALHOSTSDOWNUNHANDLED");
	add_macrox_name(MACRO_TOTALHOSTSUNREACHABLEUNHANDLED,"TOTALHOSTSUNREACHABLEUNHANDLED");
	add_macrox_name(MACRO_TOTALHOSTPROBLEMS,"TOTALHOSTPROBLEMS");
	add_macrox_name(MACRO_TOTALHOSTPROBLEMSUNHANDLED,"TOTALHOSTPROBLEMSUNHANDLED");
	add_macrox_name(MACRO_TOTALSERVICESOK,"TOTALSERVICESOK");
	add_macrox_name(MACRO_TOTALSERVICESWARNING,"TOTALSERVICESWARNING");
	add_macrox_name(MACRO_TOTALSERVICESCRITICAL,"TOTALSERVICESCRITICAL");
	add_macrox_name(MACRO_TOTALSERVICESUNKNOWN,"TOTALSERVICESUNKNOWN");
	add_macrox_name(MACRO_TOTALSERVICESWARNINGUNHANDLED,"TOTALSERVICESWARNINGUNHANDLED");
	add_macrox_name(MACRO_TOTALSERVICESCRITICALUNHANDLED,"TOTALSERVICESCRITICALUNHANDLED");
	add_macrox_name(MACRO_TOTALSERVICESUNKNOWNUNHANDLED,"TOTALSERVICESUNKNOWNUNHANDLED");
	add_macrox_name(MACRO_TOTALSERVICEPROBLEMS,"TOTALSERVICEPROBLEMS");
	add_macrox_name(MACRO_TOTALSERVICEPROBLEMSUNHANDLED,"TOTALSERVICEPROBLEMSUNHANDLED");
	add_macrox_name(MACRO_PROCESSSTARTTIME,"PROCESSSTARTTIME");
	add_macrox_name(MACRO_HOSTCHECKTYPE,"HOSTCHECKTYPE");
	add_macrox_name(MACRO_SERVICECHECKTYPE,"SERVICECHECKTYPE");
	add_macrox_name(MACRO_LONGHOSTOUTPUT,"LONGHOSTOUTPUT");
	add_macrox_name(MACRO_LONGSERVICEOUTPUT,"LONGSERVICEOUTPUT");
	add_macrox_name(MACRO_TEMPPATH,"TEMPPATH");
	add_macrox_name(MACRO_HOSTNOTIFICATIONNUMBER,"HOSTNOTIFICATIONNUMBER");
	add_macrox_name(MACRO_SERVICENOTIFICATIONNUMBER,"SERVICENOTIFICATIONNUMBER");
	add_macrox_name(MACRO_HOSTNOTIFICATIONID,"HOSTNOTIFICATIONID");
	add_macrox_name(MACRO_SERVICENOTIFICATIONID,"SERVICENOTIFICATIONID");
	add_macrox_name(MACRO_HOSTEVENTID,"HOSTEVENTID");
	add_macrox_name(MACRO_LASTHOSTEVENTID,"LASTHOSTEVENTID");
	add_macrox_name(MACRO_SERVICEEVENTID,"SERVICEEVENTID");
	add_macrox_name(MACRO_LASTSERVICEEVENTID,"LASTSERVICEEVENTID");
	add_macrox_name(MACRO_HOSTGROUPNAMES,"HOSTGROUPNAMES");
	add_macrox_name(MACRO_SERVICEGROUPNAMES,"SERVICEGROUPNAMES");
	add_macrox_name(MACRO_HOSTACKAUTHORNAME,"HOSTACKAUTHORNAME");
	add_macrox_name(MACRO_HOSTACKAUTHORALIAS,"HOSTACKAUTHORALIAS");
	add_macrox_name(MACRO_SERVICEACKAUTHORNAME,"SERVICEACKAUTHORNAME");
	add_macrox_name(MACRO_SERVICEACKAUTHORALIAS,"SERVICEACKAUTHORALIAS");
	add_macrox_name(MACRO_MAXHOSTATTEMPTS,"MAXHOSTATTEMPTS");
	add_macrox_name(MACRO_MAXSERVICEATTEMPTS,"MAXSERVICEATTEMPTS");
	add_macrox_name(MACRO_SERVICEISVOLATILE,"SERVICEISVOLATILE");
	add_macrox_name(MACRO_TOTALHOSTSERVICES,"TOTALHOSTSERVICES");
	add_macrox_name(MACRO_TOTALHOSTSERVICESOK,"TOTALHOSTSERVICESOK");
	add_macrox_name(MACRO_TOTALHOSTSERVICESWARNING,"TOTALHOSTSERVICESWARNING");
	add_macrox_name(MACRO_TOTALHOSTSERVICESUNKNOWN,"TOTALHOSTSERVICESUNKNOWN");
	add_macrox_name(MACRO_TOTALHOSTSERVICESCRITICAL,"TOTALHOSTSERVICESCRITICAL");
	add_macrox_name(MACRO_HOSTGROUPNOTES,"HOSTGROUPNOTES");
	add_macrox_name(MACRO_HOSTGROUPNOTESURL,"HOSTGROUPNOTESURL");
	add_macrox_name(MACRO_HOSTGROUPACTIONURL,"HOSTGROUPACTIONURL");
	add_macrox_name(MACRO_SERVICEGROUPNOTES,"SERVICEGROUPNOTES");
	add_macrox_name(MACRO_SERVICEGROUPNOTESURL,"SERVICEGROUPNOTESURL");
	add_macrox_name(MACRO_SERVICEGROUPACTIONURL,"SERVICEGROUPACTIONURL");
	add_macrox_name(MACRO_HOSTGROUPMEMBERS,"HOSTGROUPMEMBERS");
	add_macrox_name(MACRO_SERVICEGROUPMEMBERS,"SERVICEGROUPMEMBERS");
	add_macrox_name(MACRO_CONTACTGROUPNAME,"CONTACTGROUPNAME");
	add_macrox_name(MACRO_CONTACTGROUPALIAS,"CONTACTGROUPALIAS");
	add_macrox_name(MACRO_CONTACTGROUPMEMBERS,"CONTACTGROUPMEMBERS");
	add_macrox_name(MACRO_CONTACTGROUPNAMES,"CONTACTGROUPNAMES");
	add_macrox_name(MACRO_NOTIFICATIONRECIPIENTS,"NOTIFICATIONRECIPIENTS");
	add_macrox_name(MACRO_NOTIFICATIONAUTHOR,"NOTIFICATIONAUTHOR");
	add_macrox_name(MACRO_NOTIFICATIONAUTHORNAME,"NOTIFICATIONAUTHORNAME");
	add_macrox_name(MACRO_NOTIFICATIONAUTHORALIAS,"NOTIFICATIONAUTHORALIAS");
	add_macrox_name(MACRO_NOTIFICATIONCOMMENT,"NOTIFICATIONCOMMENT");
	add_macrox_name(MACRO_EVENTSTARTTIME,"EVENTSTARTTIME");

	return OK;
        }


/* saves the name of a macro */
int add_macrox_name(int i, char *name){

	/* dup the macro name */
	macro_x_names[i]=(char *)strdup(name);

	return OK;
        }


/* free memory associated with the macrox names */
int free_macrox_names(void){
	register int x=0;

	/* free each macro name */
	for(x=0;x<MACRO_X_COUNT;x++)
		my_free(&macro_x_names[x]);

	return OK;
        }



/* clear argv macros - used in commands */
int clear_argv_macros(void){
	register int x=0;

	/* command argument macros */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++)
		my_free(&macro_argv[x]);

	return OK;
        }



/* clear all macros that are not "constant" (i.e. they change throughout the course of monitoring) */
int clear_volatile_macros(void){
	customvariablesmember *this_customvariablesmember=NULL;
	customvariablesmember *next_customvariablesmember=NULL;
	register int x=0;

	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_ADMINEMAIL:
		case MACRO_ADMINPAGER:
		case MACRO_MAINCONFIGFILE:
		case MACRO_STATUSDATAFILE:
		case MACRO_RETENTIONDATAFILE:
		case MACRO_OBJECTCACHEFILE:
		case MACRO_TEMPFILE:
		case MACRO_LOGFILE:
		case MACRO_RESOURCEFILE:
		case MACRO_COMMANDFILE:
		case MACRO_HOSTPERFDATAFILE:
		case MACRO_SERVICEPERFDATAFILE:
		case MACRO_PROCESSSTARTTIME:
		case MACRO_TEMPPATH:
			break;
		default:
			my_free(&macro_x[x]);
			break;
		        }
	        }

	/* contact address macros */
	for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
		my_free(&macro_contactaddress[x]);

	/* clear on-demand macro */
	my_free(&macro_ondemand);

	/* clear ARGx macros */
	clear_argv_macros();

	/* clear custom host variables */
	for(this_customvariablesmember=macro_custom_host_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(&this_customvariablesmember->variable_name);
		my_free(&this_customvariablesmember->variable_value);
		my_free(&this_customvariablesmember);
	        }
	macro_custom_host_vars=NULL;

	/* clear custom service variables */
	for(this_customvariablesmember=macro_custom_service_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(&this_customvariablesmember->variable_name);
		my_free(&this_customvariablesmember->variable_value);
		my_free(&this_customvariablesmember);
	        }
	macro_custom_service_vars=NULL;

	/* clear custom contact variables */
	for(this_customvariablesmember=macro_custom_contact_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(&this_customvariablesmember->variable_name);
		my_free(&this_customvariablesmember->variable_value);
		my_free(&this_customvariablesmember);
	        }
	macro_custom_contact_vars=NULL;

	return OK;
        }


/* clear macros that are constant (i.e. they do NOT change during monitoring) */
int clear_nonvolatile_macros(void){
	register int x=0;

	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_ADMINEMAIL:
		case MACRO_ADMINPAGER:
		case MACRO_MAINCONFIGFILE:
		case MACRO_STATUSDATAFILE:
		case MACRO_RETENTIONDATAFILE:
		case MACRO_OBJECTCACHEFILE:
		case MACRO_TEMPFILE:
		case MACRO_LOGFILE:
		case MACRO_RESOURCEFILE:
		case MACRO_COMMANDFILE:
		case MACRO_HOSTPERFDATAFILE:
		case MACRO_SERVICEPERFDATAFILE:
		case MACRO_PROCESSSTARTTIME:
		case MACRO_TEMPPATH:
		case MACRO_EVENTSTARTTIME:
			my_free(&macro_x[x]);
			break;
		default:
			break;
		        }
	        }

	return OK;
        }


/* clear service macros */
int clear_service_macros(void){
	register int x;
	customvariablesmember *this_customvariablesmember=NULL;
	customvariablesmember *next_customvariablesmember=NULL;
	
	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_SERVICEDESC:
		case MACRO_SERVICEDISPLAYNAME:
		case MACRO_SERVICEOUTPUT:
		case MACRO_LONGSERVICEOUTPUT:
		case MACRO_SERVICEPERFDATA:
		case MACRO_SERVICECHECKCOMMAND:
		case MACRO_SERVICECHECKTYPE:
		case MACRO_SERVICESTATETYPE:
		case MACRO_SERVICESTATE:
		case MACRO_SERVICEISVOLATILE:
		case MACRO_SERVICESTATEID:
		case MACRO_SERVICEATTEMPT:
		case MACRO_MAXSERVICEATTEMPTS:
		case MACRO_SERVICEEXECUTIONTIME:
		case MACRO_SERVICELATENCY:
		case MACRO_LASTSERVICECHECK:
		case MACRO_LASTSERVICESTATECHANGE:
		case MACRO_LASTSERVICEOK:
		case MACRO_LASTSERVICEWARNING:
		case MACRO_LASTSERVICEUNKNOWN:
		case MACRO_LASTSERVICECRITICAL:
		case MACRO_SERVICEDOWNTIME:
		case MACRO_SERVICEPERCENTCHANGE:
		case MACRO_SERVICEDURATIONSEC:
		case MACRO_SERVICEDURATION:
		case MACRO_SERVICENOTIFICATIONNUMBER:
		case MACRO_SERVICENOTIFICATIONID:
		case MACRO_SERVICEEVENTID:
		case MACRO_LASTSERVICEEVENTID:
		case MACRO_SERVICEACTIONURL:
		case MACRO_SERVICENOTESURL:
		case MACRO_SERVICENOTES:

		case MACRO_SERVICEGROUPNAMES:

			my_free(&macro_x[x]);
			break;
		default:
			break;
			}
		}

	/* clear custom service variables */
	for(this_customvariablesmember=macro_custom_service_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(&this_customvariablesmember->variable_name);
		my_free(&this_customvariablesmember->variable_value);
		my_free(&this_customvariablesmember);
	        }
	macro_custom_service_vars=NULL;

	return OK;
	}


/* clear host macros */
int clear_host_macros(void){
	register int x;
	customvariablesmember *this_customvariablesmember=NULL;
	customvariablesmember *next_customvariablesmember=NULL;
	
	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_HOSTNAME:
		case MACRO_HOSTDISPLAYNAME:
		case MACRO_HOSTALIAS:
		case MACRO_HOSTADDRESS:
		case MACRO_HOSTSTATE:
		case MACRO_HOSTSTATEID:
		case MACRO_HOSTCHECKTYPE:
		case MACRO_HOSTSTATETYPE:
		case MACRO_HOSTOUTPUT:
		case MACRO_LONGHOSTOUTPUT:
		case MACRO_HOSTPERFDATA:
		case MACRO_HOSTCHECKCOMMAND:
		case MACRO_HOSTATTEMPT:
		case MACRO_MAXHOSTATTEMPTS:
		case MACRO_HOSTDOWNTIME:
		case MACRO_HOSTPERCENTCHANGE:
		case MACRO_HOSTDURATIONSEC:
		case MACRO_HOSTDURATION:
		case MACRO_HOSTEXECUTIONTIME:
		case MACRO_HOSTLATENCY:
		case MACRO_LASTHOSTCHECK:
		case MACRO_LASTHOSTSTATECHANGE:
		case MACRO_LASTHOSTUP:
		case MACRO_LASTHOSTDOWN:
		case MACRO_LASTHOSTUNREACHABLE:
		case MACRO_HOSTNOTIFICATIONNUMBER:
		case MACRO_HOSTNOTIFICATIONID:
		case MACRO_HOSTEVENTID:
		case MACRO_LASTHOSTEVENTID:
		case MACRO_HOSTACTIONURL:
		case MACRO_HOSTNOTESURL:
		case MACRO_HOSTNOTES:

		case MACRO_HOSTGROUPNAMES:

		case MACRO_TOTALHOSTSERVICES:
		case MACRO_TOTALHOSTSERVICESOK:
		case MACRO_TOTALHOSTSERVICESWARNING:
		case MACRO_TOTALHOSTSERVICESUNKNOWN:
		case MACRO_TOTALHOSTSERVICESCRITICAL:

			my_free(&macro_x[x]);
			break;
		default:
			break;
			}
		}

	/* clear custom host variables */
	for(this_customvariablesmember=macro_custom_host_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(&this_customvariablesmember->variable_name);
		my_free(&this_customvariablesmember->variable_value);
		my_free(&this_customvariablesmember);
	        }
	macro_custom_host_vars=NULL;

	return OK;
	}


/* clear hostgroup macros */
int clear_hostgroup_macros(void){
	register int x;
	
	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_HOSTGROUPNAME:
		case MACRO_HOSTGROUPALIAS:
		case MACRO_HOSTGROUPMEMBERS:
		case MACRO_HOSTGROUPACTIONURL:
		case MACRO_HOSTGROUPNOTESURL:
		case MACRO_HOSTGROUPNOTES:
			my_free(&macro_x[x]);
			break;
		default:
			break;
			}
		}

	return OK;
	}


/* clear servicegroup macros */
int clear_servicegroup_macros(void){
	register int x;
	
	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_SERVICEGROUPNAME:
		case MACRO_SERVICEGROUPALIAS:
		case MACRO_SERVICEGROUPMEMBERS:
		case MACRO_SERVICEGROUPACTIONURL:
		case MACRO_SERVICEGROUPNOTESURL:
		case MACRO_SERVICEGROUPNOTES:
			my_free(&macro_x[x]);
			break;
		default:
			break;
			}
		}

	return OK;
	}


/* clear contact macros */
int clear_contact_macros(void){
	register int x;
	customvariablesmember *this_customvariablesmember=NULL;
	customvariablesmember *next_customvariablesmember=NULL;
	
	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_CONTACTNAME:
		case MACRO_CONTACTALIAS:
		case MACRO_CONTACTEMAIL:
		case MACRO_CONTACTPAGER:
		case MACRO_CONTACTGROUPNAMES:
			my_free(&macro_x[x]);
			break;
		default:
			break;
			}
		}

	/* clear contact addresses */
	for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
		my_free(&macro_contactaddress[x]);

	/* clear custom contact variables */
	for(this_customvariablesmember=macro_custom_contact_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(&this_customvariablesmember->variable_name);
		my_free(&this_customvariablesmember->variable_value);
		my_free(&this_customvariablesmember);
	        }
	macro_custom_contact_vars=NULL;

	return OK;
	}



/* clear contactgroup macros */
int clear_contactgroup_macros(void){
	register int x;
	
	for(x=0;x<MACRO_X_COUNT;x++){
		switch(x){
		case MACRO_CONTACTGROUPNAME:
		case MACRO_CONTACTGROUPALIAS:
		case MACRO_CONTACTGROUPMEMBERS:
			my_free(&macro_x[x]);
			break;
		default:
			break;
			}
		}

	return OK;
	}


#ifdef NSCORE

/* sets or unsets all macro environment variables */
int set_all_macro_environment_vars(int set){

	set_macrox_environment_vars(set);
	set_argv_macro_environment_vars(set);
	set_custom_macro_environment_vars(set);

	return OK;
        }


/* sets or unsets macrox environment variables */
int set_macrox_environment_vars(int set){
	register int x=0;

	/* set each of the macrox environment variables */
	for(x=0;x<MACRO_X_COUNT;x++){

		/* host/service output/perfdata macros get cleaned */
		if(x>=16 && x<=19)
			set_macro_environment_var(macro_x_names[x],clean_macro_chars(macro_x[x],STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS),set);

		/* others don't */
		else
			set_macro_environment_var(macro_x_names[x],macro_x[x],set);
		}

	return OK;
        }


/* sets or unsets argv macro environment variables */
int set_argv_macro_environment_vars(int set){
	char *macro_name=NULL;
	register int x=0;

	/* set each of the argv macro environment variables */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++){
		asprintf(&macro_name,"ARG%d",x+1);
		set_macro_environment_var(macro_name,macro_argv[x],set);
		my_free(&macro_name);
	        }

	return OK;
        }


/* sets or unsets custom host/service/contact macro environment variables */
int set_custom_macro_environment_vars(int set){
	customvariablesmember *temp_customvariablesmember=NULL;

	/* set each of the custom host environment variables */
	for(temp_customvariablesmember=macro_custom_host_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
		set_macro_environment_var(temp_customvariablesmember->variable_name,clean_macro_chars(temp_customvariablesmember->variable_value,STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS),set);
		}

	/* set each of the custom service environment variables */
	for(temp_customvariablesmember=macro_custom_service_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next)
		set_macro_environment_var(temp_customvariablesmember->variable_name,clean_macro_chars(temp_customvariablesmember->variable_value,STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS),set);

	/* set each of the custom contact environment variables */
	for(temp_customvariablesmember=macro_custom_contact_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next)
		set_macro_environment_var(temp_customvariablesmember->variable_name,clean_macro_chars(temp_customvariablesmember->variable_value,STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS),set);

	return OK;
        }


/* sets or unsets a macro environment variable */
int set_macro_environment_var(char *name, char *value, int set){
	char *env_macro_name=NULL;
#ifndef HAVE_SETENV
	char *env_macro_string=NULL;
#endif

	/* we won't mess with null variable names */
	if(name==NULL)
		return ERROR;

	/* allocate memory */
	if((env_macro_name=(char *)malloc(strlen(MACRO_ENV_VAR_PREFIX)+strlen(name)+1))==NULL)
		return ERROR;

	/* create the name */
	strcpy(env_macro_name,"");
	strcpy(env_macro_name,MACRO_ENV_VAR_PREFIX);
	strcat(env_macro_name,name);

	/* set or unset the environment variable */
	if(set==TRUE){

#ifdef HAVE_SETENV
		setenv(env_macro_name,(value==NULL)?"":value,1);
#else
		/* needed for Solaris and systems that don't have setenv() */
		/* this will leak memory, but in a "controlled" way, since lost memory should be freed when the child process exits */
		env_macro_string=(char *)malloc(strlen(env_macro_name)+strlen((value==NULL)?"":value)+2);
		if(env_macro_string!=NULL){
			sprintf(env_macro_string,"%s=%s",env_macro_name,(value==NULL)?"":value);
			putenv(env_macro_string);
		        }
#endif
	        }
	else{
#ifdef HAVE_UNSETENV
		unsetenv(env_macro_name);
#endif
	        }

	/* free allocated memory */
	my_free(&env_macro_name);

	return OK;
        }

#endif


