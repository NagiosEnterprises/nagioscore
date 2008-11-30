/*****************************************************************************
 *
 * MACROS.C - Common macro functions for Nagios
 *
 * Copyright (c) 1999-2009 Ethan Galstad (egalstad@nagios.org)
 * Last Modified: 06-23-2008
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
extern int      enable_environment_macros;
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

host            *macro_host_ptr=NULL;
hostgroup       *macro_hostgroup_ptr=NULL;
service         *macro_service_ptr=NULL;
servicegroup    *macro_servicegroup_ptr=NULL;
contact         *macro_contact_ptr=NULL;
contactgroup    *macro_contactgroup_ptr=NULL;



/******************************************************************/
/************************ MACRO FUNCTIONS *************************/
/******************************************************************/

/* replace macros in notification commands with their values */
int process_macros(char *input_buffer, char **output_buffer, int options){
	char *temp_buffer=NULL;
	char *buf_ptr=NULL;
	char *delim_ptr=NULL;
	int in_macro=FALSE;
	int x=0;
	char *selected_macro=NULL;
	char *original_macro=NULL;
	char *cleaned_macro=NULL;
	int clean_macro=FALSE;
	int found_macro_x=FALSE;
	int result=OK;
	int clean_options=0;
	int free_macro=FALSE;
	int macro_options=0;


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

	buf_ptr=input_buffer;
	while(buf_ptr){

		/* save pointer to this working part of buffer */
		temp_buffer=buf_ptr;

		/* find the next delimiter - terminate preceding string and advance buffer pointer for next run */
		if((delim_ptr=strchr(buf_ptr,'$'))){
			   delim_ptr[0]='\x0';
			   buf_ptr=(char *)delim_ptr+1;
			   }
		/* no delimiter found - we already have the last of the buffer */
		else
			buf_ptr=NULL;

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

			/* reset clean options */
			clean_options=0;

			/* grab the macro value */
			result=grab_macro_value(temp_buffer,&selected_macro,&clean_options,&free_macro);
#ifdef NSCORE
			log_debug_info(DEBUGL_MACROS,2,"  Processed '%s', Clean Options: %d, Free: %d\n",temp_buffer,clean_options,free_macro);
#endif

			/* an error occurred - we couldn't parse the macro, so continue on */
			if(result==ERROR){
#ifdef NSCORE
				log_debug_info(DEBUGL_MACROS,0," WARNING: An error occurred processing macro '%s'!\n",temp_buffer);
#endif
				if(free_macro==TRUE)
					my_free(selected_macro);
				}

			/* we already have a macro... */
			if(result==OK)
				x=0;

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

#ifdef NSCORE
				log_debug_info(DEBUGL_MACROS,2,"  Processed '%s', Clean Options: %d, Free: %d\n",temp_buffer,clean_options,free_macro);
#endif

				/* include any cleaning options passed back to us */
				macro_options=(options | clean_options);

#ifdef NSCORE
				log_debug_info(DEBUGL_MACROS,2,"  Cleaning options: global=%d, local=%d, effective=%d\n",options,clean_options,macro_options);
#endif

				/* URL encode the macro if requested - this allocates new memory */
				if(macro_options & URL_ENCODE_MACRO_CHARS){
					original_macro=selected_macro;
					selected_macro=get_url_encoded_string(selected_macro);
					if(free_macro==TRUE){
						my_free(original_macro);
						}
					free_macro=TRUE;
					}
				
				/* some macros are cleaned... */
				if(clean_macro==TRUE || ((macro_options & STRIP_ILLEGAL_MACRO_CHARS) || (macro_options & ESCAPE_MACRO_CHARS))){

					/* add the (cleaned) processed macro to the end of the already processed buffer */
					if(selected_macro!=NULL && (cleaned_macro=clean_macro_chars(selected_macro,macro_options))!=NULL){
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

				/* free memory if necessary (if we URL encoded the macro or we were told to do so by grab_macro_value()) */
				if(free_macro==TRUE)
					my_free(selected_macro);

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



/******************************************************************/
/********************** MACRO GRAB FUNCTIONS **********************/
/******************************************************************/

/* grab macros that are specific to a particular host */
int grab_host_macros(host *hst){

	/* clear host-related macros */
	clear_host_macros();
	clear_hostgroup_macros();

	/* save pointer to host */
	macro_host_ptr=hst;
	macro_hostgroup_ptr=NULL;

	if(hst==NULL)
		return ERROR;

#ifdef NSCORE
	/* save pointer to host's first/primary hostgroup */
	if(hst->hostgroups_ptr)
		macro_hostgroup_ptr=(hostgroup *)hst->hostgroups_ptr->object_ptr;
#endif

	return OK;
        }


/* grab hostgroup macros */
int grab_hostgroup_macros(hostgroup *hg){

	/* clear hostgroup macros */
	clear_hostgroup_macros();

	/* save the hostgroup pointer for later */
	macro_hostgroup_ptr=hg;

	if(hg==NULL)
		return ERROR;

	return OK;
	}


/* grab macros that are specific to a particular service */
int grab_service_macros(service *svc){

	/* clear service-related macros */
	clear_service_macros();
	clear_servicegroup_macros();

	/* save pointer for later */
	macro_service_ptr=svc;
	macro_servicegroup_ptr=NULL;
	
	if(svc==NULL)
		return ERROR;

#ifdef NSCORE
	/* save first/primary servicegroup pointer for later */
	if(svc->servicegroups_ptr)
		macro_servicegroup_ptr=(servicegroup *)svc->servicegroups_ptr->object_ptr;
#endif

	return OK;
        }



/* grab macros that are specific to a particular servicegroup */
int grab_servicegroup_macros(servicegroup *sg){

	/* clear servicegroup macros */
	clear_servicegroup_macros();

	/* save the pointer for later */
	macro_servicegroup_ptr=sg;

	if(sg==NULL)
		return ERROR;

	return OK;
	}



/* grab macros that are specific to a particular contact */
int grab_contact_macros(contact *cntct){

	/* clear contact-related macros */
	clear_contact_macros();
	clear_contactgroup_macros();

	/* save pointer to contact for later */
	macro_contact_ptr=cntct;
	macro_contactgroup_ptr=NULL;

	if(cntct==NULL)
		return ERROR;

#ifdef NSCORE
	/* save pointer to first/primary contactgroup for later */
	if(cntct->contactgroups_ptr)
		macro_contactgroup_ptr=(contactgroup *)cntct->contactgroups_ptr->object_ptr;
#endif

	return OK;
	}



/* grab contactgroup macros */
int grab_contactgroup_macros(contactgroup *cg){

	/* clear contactgroup macros */
	clear_contactgroup_macros();

	/* save pointer to contactgroup for later */
	macro_contactgroup_ptr=cg;

	if(cg==NULL)
		return ERROR;

	return OK;
	}




/******************************************************************/
/******************* MACRO GENERATION FUNCTIONS *******************/
/******************************************************************/

/* this is the big one */
int grab_macro_value(char *macro_buffer, char **output, int *clean_options, int *free_macro){
	char *buf=NULL;
	char *ptr=NULL;
	char *macro_name=NULL;
	char *arg[2]={NULL,NULL};
	contact *temp_contact=NULL;
	contactgroup *temp_contactgroup=NULL;
	contactsmember *temp_contactsmember=NULL;
	char *temp_buffer=NULL;
	int delimiter_len=0;
	register int x;
	int result=OK;

	if(output==NULL)
		return ERROR;

	/* clear the old macro value */
	my_free(*output);

	if(macro_buffer==NULL || clean_options==NULL || free_macro==NULL)
		return ERROR;

	/* work with a copy of the original buffer */
	if((buf=(char *)strdup(macro_buffer))==NULL)
		return ERROR;

	/* BY DEFAULT, TELL CALLER TO FREE MACRO BUFFER WHEN DONE */
	*free_macro=TRUE;

	/* macro name is at start of buffer */
	macro_name=buf;

	/* see if there's an argument - if so, this is most likely an on-demand macro */
	if((ptr=strchr(buf,':'))){

		ptr[0]='\x0';
		ptr++;

		/* save the first argument - host name, hostgroup name, etc. */
		arg[0]=ptr;

		/* try and find a second argument */
		if((ptr=strchr(ptr,':'))){

			ptr[0]='\x0';
			ptr++;

			/* save second argument - service description or delimiter */
			arg[1]=ptr;
			}
		}

	/***** X MACROS *****/
	/* see if this is an x macro */
	for(x=0;x<MACRO_X_COUNT;x++){

		if(macro_x_names[x]==NULL)
			continue;

		if(!strcmp(macro_name,macro_x_names[x])){

#ifdef NSCORE
			log_debug_info(DEBUGL_MACROS,2,"  macro_x[%d] (%s) match.\n",x,macro_x_names[x]);
#endif

			/* get the macro value */
			result=grab_macrox_value(x,arg[0],arg[1],output,free_macro);

			/* post-processing */
			/* host/service output/perfdata and author/comment macros should get cleaned */
			if((x>=16 && x<=19) ||(x>=49 && x<=52) || (x>=99 && x<=100) || (x>=124 && x<=127)){
				*clean_options|=(STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS);
#ifdef NSCORE
				log_debug_info(DEBUGL_MACROS,2,"  New clean options: %d\n",*clean_options);
#endif
				}
			/* url macros should get cleaned */
			if((x>=125 && x<=126) ||(x>=128 && x<=129) || (x>=77 && x<=78) || (x>=74 && x<=75)){
				*clean_options|=URL_ENCODE_MACRO_CHARS;
#ifdef NSCORE
				log_debug_info(DEBUGL_MACROS,2,"  New clean options: %d\n",*clean_options);
#endif
				}




			break;
			}
		}

	/* we already found the macro... */
	if(x<MACRO_X_COUNT)
		x=x;

	/***** ARGV MACROS *****/
	else if(strstr(macro_name,"ARG")==macro_name){

		/* which arg do we want? */
		x=atoi(macro_name+3);

		if(x<=0 || x>MAX_COMMAND_ARGUMENTS){
			my_free(buf);
			return ERROR;
			}

		/* use a pre-computed macro value */
		*output=macro_argv[x-1];
		*free_macro=FALSE;
		}

	/***** USER MACROS *****/
	else if(strstr(macro_name,"USER")==macro_name){

		/* which macro do we want? */
		x=atoi(macro_name+4);

		if(x<=0 || x>MAX_USER_MACROS){
			my_free(buf);
			return ERROR;
			}

		/* use a pre-computed macro value */
		*output=macro_user[x-1];
		*free_macro=FALSE;
		}

	/***** CONTACT ADDRESS MACROS *****/
	/* NOTE: the code below should be broken out into a separate function */
	else if(strstr(macro_name,"CONTACTADDRESS")==macro_name){

		/* which address do we want? */
		x=atoi(macro_name+14)-1;

		/* regular macro */
		if(arg[0]==NULL){
			
			/* use the saved pointer */
			if((temp_contact=macro_contact_ptr)==NULL){
				my_free(buf);
				return ERROR;
				}

			/* get the macro value */
			result=grab_contact_address_macro(x,temp_contact,output);
			}

		/* on-demand macro */
		else{

			/* on-demand contact macro with a contactgroup and a delimiter */
			if(arg[1]!=NULL){

				if((temp_contactgroup=find_contactgroup(arg[0]))==NULL)
					return ERROR;

				delimiter_len=strlen(arg[1]);
				
				/* concatenate macro values for all contactgroup members */
				for(temp_contactsmember=temp_contactgroup->members;temp_contactsmember!=NULL;temp_contactsmember=temp_contactsmember->next){

#ifdef NSCORE
					if((temp_contact=temp_contactsmember->contact_ptr)==NULL)
						continue;
					if((temp_contact=find_contact(temp_contactsmember->contact_name))==NULL)
						continue;
#endif

					/* get the macro value for this contact */
					grab_contact_address_macro(x,temp_contact,&temp_buffer);

					if(temp_buffer==NULL)
						continue;
				
					/* add macro value to already running macro */
					if(*output==NULL)
						*output=(char *)strdup(temp_buffer);
					else{
						if((*output=(char *)realloc(*output,strlen(*output)+strlen(temp_buffer)+delimiter_len+1))==NULL)
							continue;
						strcat(*output,arg[1]);
						strcat(*output,temp_buffer);
						}
					my_free(temp_buffer);
					}
				}

			/* else on-demand contact macro */
			else{

				/* find the contact */
				if((temp_contact=find_contact(arg[0]))==NULL){
					my_free(buf);
					return ERROR;
					}

				/* get the macro value */
				result=grab_contact_address_macro(x,temp_contact,output);
				}
			}
		}

	/***** CUSTOM VARIABLE MACROS *****/
	else if(macro_name[0]=='_'){

		/* get the macro value */
		result=grab_custom_macro_value(macro_name,arg[0],arg[1],output);

		/* custom variable values get cleaned */
		if(result==OK)
			*clean_options|=(STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS);
		}

	/* no macro matched... */
	else{
#ifdef NSCORE
		log_debug_info(DEBUGL_MACROS,0," WARNING: Could not find a macro matching '%s'!\n",macro_name);
#endif
		result=ERROR;
		}
	
	/* free memory */
	my_free(buf);

	return result;
	}
	


int grab_macrox_value(int macro_type, char *arg1, char *arg2, char **output, int *free_macro){
	host *temp_host=NULL;
	hostgroup *temp_hostgroup=NULL;
	hostsmember *temp_hostsmember=NULL;
	service *temp_service=NULL;
	servicegroup *temp_servicegroup=NULL;
	servicesmember *temp_servicesmember=NULL;
	contact *temp_contact=NULL;
	contactgroup *temp_contactgroup=NULL;
	contactsmember *temp_contactsmember=NULL;
	char *temp_buffer=NULL;
	int result=OK;
	int delimiter_len=0;
	int free_sub_macro=FALSE;
#ifdef NSCORE
	register int x;
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
#endif


	if(output==NULL || free_macro==NULL)
		return ERROR;

	/* BY DEFAULT, TELL CALLER TO FREE MACRO BUFFER WHEN DONE */
	*free_macro=TRUE;

	/* handle the macro */
	switch(macro_type){

		/***************/
		/* HOST MACROS */
		/***************/
	case MACRO_HOSTNAME:
	case MACRO_HOSTALIAS:
	case MACRO_HOSTADDRESS:
	case MACRO_LASTHOSTCHECK:
	case MACRO_LASTHOSTSTATECHANGE:
	case MACRO_HOSTOUTPUT:
	case MACRO_HOSTPERFDATA:
	case MACRO_HOSTSTATE:
	case MACRO_HOSTSTATEID:
	case MACRO_HOSTATTEMPT:
	case MACRO_HOSTEXECUTIONTIME:
	case MACRO_HOSTLATENCY:
	case MACRO_HOSTDURATION:
	case MACRO_HOSTDURATIONSEC:
	case MACRO_HOSTDOWNTIME:
	case MACRO_HOSTSTATETYPE:
	case MACRO_HOSTPERCENTCHANGE:
	case MACRO_HOSTACKAUTHOR:
	case MACRO_HOSTACKCOMMENT:
	case MACRO_LASTHOSTUP:
	case MACRO_LASTHOSTDOWN:
	case MACRO_LASTHOSTUNREACHABLE:
	case MACRO_HOSTCHECKCOMMAND:
	case MACRO_HOSTDISPLAYNAME:
	case MACRO_HOSTACTIONURL:
	case MACRO_HOSTNOTESURL:
	case MACRO_HOSTNOTES:
	case MACRO_HOSTCHECKTYPE:
	case MACRO_LONGHOSTOUTPUT:
	case MACRO_HOSTNOTIFICATIONNUMBER:
	case MACRO_HOSTNOTIFICATIONID:
	case MACRO_HOSTEVENTID:
	case MACRO_LASTHOSTEVENTID:
	case MACRO_HOSTGROUPNAMES:
	case MACRO_HOSTACKAUTHORNAME:
	case MACRO_HOSTACKAUTHORALIAS:
	case MACRO_MAXHOSTATTEMPTS:
	case MACRO_TOTALHOSTSERVICES:
	case MACRO_TOTALHOSTSERVICESOK:
	case MACRO_TOTALHOSTSERVICESWARNING:
	case MACRO_TOTALHOSTSERVICESUNKNOWN:
	case MACRO_TOTALHOSTSERVICESCRITICAL:
	case MACRO_HOSTPROBLEMID:
	case MACRO_LASTHOSTPROBLEMID:
	case MACRO_LASTHOSTSTATE:
	case MACRO_LASTHOSTSTATEID:

		/* a standard host macro */
		if(arg2==NULL){

			/* find the host for on-demand macros */
			if(arg1){
				if((temp_host=find_host(arg1))==NULL)
					return ERROR;
				}

			/* else use saved host pointer */
			else if((temp_host=macro_host_ptr)==NULL)
				return ERROR;

			/* get the host macro value */
			result=grab_standard_host_macro(macro_type,temp_host,output,free_macro);
			}

		/* a host macro with a hostgroup name and delimiter */
		else{

			if((temp_hostgroup=find_hostgroup(arg1))==NULL)
				return ERROR;

			delimiter_len=strlen(arg2);

			/* concatenate macro values for all hostgroup members */
			for(temp_hostsmember=temp_hostgroup->members;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

#ifdef NSCORE
				if((temp_host=temp_hostsmember->host_ptr)==NULL)
					continue;
#else
				if((temp_host=find_host(temp_hostsmember->host_name))==NULL)
					continue;
#endif

				/* get the macro value for this host */
				grab_standard_host_macro(macro_type,temp_host,&temp_buffer,&free_sub_macro);

				if(temp_buffer==NULL)
					continue;
				
				/* add macro value to already running macro */
				if(*output==NULL)
					*output=(char *)strdup(temp_buffer);
				else{
					if((*output=(char *)realloc(*output,strlen(*output)+strlen(temp_buffer)+delimiter_len+1))==NULL)
						continue;
					strcat(*output,arg2);
					strcat(*output,temp_buffer);
					}
				if(free_sub_macro==TRUE)
					my_free(temp_buffer);
				}
			}
		break;

		/********************/
		/* HOSTGROUP MACROS */
		/********************/
	case MACRO_HOSTGROUPNAME:
	case MACRO_HOSTGROUPALIAS:
	case MACRO_HOSTGROUPNOTES:
	case MACRO_HOSTGROUPNOTESURL:
	case MACRO_HOSTGROUPACTIONURL:
	case MACRO_HOSTGROUPMEMBERS:

		/* a standard hostgroup macro */
		/* use the saved hostgroup pointer */
		if(arg1==NULL){
			if((temp_hostgroup=macro_hostgroup_ptr)==NULL)
				return ERROR;
			}

		/* else find the hostgroup for on-demand macros */
		else{
			if((temp_hostgroup=find_hostgroup(arg1))==NULL)
				return ERROR;
			}

		/* get the hostgroup macro value */
		result=grab_standard_hostgroup_macro(macro_type,temp_hostgroup,output);
		break;

		/******************/
		/* SERVICE MACROS */
		/******************/
	case MACRO_SERVICEDESC:
	case MACRO_SERVICESTATE:
	case MACRO_SERVICESTATEID:
	case MACRO_SERVICEATTEMPT:
	case MACRO_LASTSERVICECHECK:
	case MACRO_LASTSERVICESTATECHANGE:
	case MACRO_SERVICEOUTPUT:
	case MACRO_SERVICEPERFDATA:
	case MACRO_SERVICEEXECUTIONTIME:
	case MACRO_SERVICELATENCY:
	case MACRO_SERVICEDURATION:
	case MACRO_SERVICEDURATIONSEC:
	case MACRO_SERVICEDOWNTIME:
	case MACRO_SERVICESTATETYPE:
	case MACRO_SERVICEPERCENTCHANGE:
	case MACRO_SERVICEACKAUTHOR:
	case MACRO_SERVICEACKCOMMENT:
	case MACRO_LASTSERVICEOK:
	case MACRO_LASTSERVICEWARNING:
	case MACRO_LASTSERVICEUNKNOWN:
	case MACRO_LASTSERVICECRITICAL:
	case MACRO_SERVICECHECKCOMMAND:
	case MACRO_SERVICEDISPLAYNAME:
	case MACRO_SERVICEACTIONURL:
	case MACRO_SERVICENOTESURL:
	case MACRO_SERVICENOTES:
	case MACRO_SERVICECHECKTYPE:
	case MACRO_LONGSERVICEOUTPUT:
	case MACRO_SERVICENOTIFICATIONNUMBER:
	case MACRO_SERVICENOTIFICATIONID:
	case MACRO_SERVICEEVENTID:
	case MACRO_LASTSERVICEEVENTID:
	case MACRO_SERVICEGROUPNAMES:
	case MACRO_SERVICEACKAUTHORNAME:
	case MACRO_SERVICEACKAUTHORALIAS:
	case MACRO_MAXSERVICEATTEMPTS:
	case MACRO_SERVICEISVOLATILE:
	case MACRO_SERVICEPROBLEMID:
	case MACRO_LASTSERVICEPROBLEMID:
	case MACRO_LASTSERVICESTATE:
	case MACRO_LASTSERVICESTATEID:

		/* use saved service pointer */
		if(arg1==NULL && arg2==NULL){

			if((temp_service=macro_service_ptr)==NULL)
				return ERROR;

			result=grab_standard_service_macro(macro_type,temp_service,output,free_macro);
			}

		/* else and ondemand macro... */
		else{

			/* if first arg is blank, it means use the current host name */
			if(arg1==NULL || arg1[0]=='\x0'){

				if(macro_host_ptr==NULL)
					return ERROR;

				if((temp_service=find_service(macro_host_ptr->name,arg2))){

					/* get the service macro value */
					result=grab_standard_service_macro(macro_type,temp_service,output,free_macro);
					}
				}

			/* on-demand macro with both host and service name */
			else if((temp_service=find_service(arg1,arg2))){

				/* get the service macro value */
				result=grab_standard_service_macro(macro_type,temp_service,output,free_macro);
				}

			/* else we have a service macro with a servicegroup name and a delimiter... */
			else if(arg1 && arg2){

				if((temp_servicegroup=find_servicegroup(arg1))==NULL)
					return ERROR;

				delimiter_len=strlen(arg2);

				/* concatenate macro values for all servicegroup members */
				for(temp_servicesmember=temp_servicegroup->members;temp_servicesmember!=NULL;temp_servicesmember=temp_servicesmember->next){

#ifdef NSCORE
					if((temp_service=temp_servicesmember->service_ptr)==NULL)
						continue;
#else
					if((temp_service=find_service(temp_servicesmember->host_name,temp_servicesmember->service_description))==NULL)
						continue;
#endif

					/* get the macro value for this service */
					grab_standard_service_macro(macro_type,temp_service,&temp_buffer,&free_sub_macro);

					if(temp_buffer==NULL)
						continue;
				
					/* add macro value to already running macro */
					if(*output==NULL)
						*output=(char *)strdup(temp_buffer);
					else{
						if((*output=(char *)realloc(*output,strlen(*output)+strlen(temp_buffer)+delimiter_len+1))==NULL)
							continue;
						strcat(*output,arg2);
						strcat(*output,temp_buffer);
						}
					if(free_sub_macro==TRUE)
						my_free(temp_buffer);
					}
				}
			else
				return ERROR;
			}
		break;

		/***********************/
		/* SERVICEGROUP MACROS */
		/***********************/
	case MACRO_SERVICEGROUPNAME:
	case MACRO_SERVICEGROUPALIAS:
	case MACRO_SERVICEGROUPNOTES:
	case MACRO_SERVICEGROUPNOTESURL:
	case MACRO_SERVICEGROUPACTIONURL:
	case MACRO_SERVICEGROUPMEMBERS:
		/* a standard servicegroup macro */
		/* use the saved servicegroup pointer */
		if(arg1==NULL){
			if((temp_servicegroup=macro_servicegroup_ptr)==NULL)
				return ERROR;
			}

		/* else find the servicegroup for on-demand macros */
		else{
			if((temp_servicegroup=find_servicegroup(arg1))==NULL)
				return ERROR;
			}

		/* get the servicegroup macro value */
		result=grab_standard_servicegroup_macro(macro_type,temp_servicegroup,output);
		break;

		/******************/
		/* CONTACT MACROS */
		/******************/
	case MACRO_CONTACTNAME:
	case MACRO_CONTACTALIAS:
	case MACRO_CONTACTEMAIL:
	case MACRO_CONTACTPAGER:
	case MACRO_CONTACTGROUPNAMES:
		/* a standard contact macro */
		if(arg2==NULL){

			/* find the contact for on-demand macros */
			if(arg1){
				if((temp_contact=find_contact(arg1))==NULL)
					return ERROR;
				}

			/* else use saved contact pointer */
			else if((temp_contact=macro_contact_ptr)==NULL)
				return ERROR;

			/* get the contact macro value */
			result=grab_standard_contact_macro(macro_type,temp_contact,output);
			}

		/* a contact macro with a contactgroup name and delimiter */
		else{

			if((temp_contactgroup=find_contactgroup(arg1))==NULL)
				return ERROR;

			delimiter_len=strlen(arg2);

			/* concatenate macro values for all contactgroup members */
			for(temp_contactsmember=temp_contactgroup->members;temp_contactsmember!=NULL;temp_contactsmember=temp_contactsmember->next){

#ifdef NSCORE
				if((temp_contact=temp_contactsmember->contact_ptr)==NULL)
					continue;
#else
				if((temp_contact=find_contact(temp_contactsmember->contact_name))==NULL)
					continue;
#endif

				/* get the macro value for this contact */
				grab_standard_contact_macro(macro_type,temp_contact,&temp_buffer);

				if(temp_buffer==NULL)
					continue;
				
				/* add macro value to already running macro */
				if(*output==NULL)
					*output=(char *)strdup(temp_buffer);
				else{
					if((*output=(char *)realloc(*output,strlen(*output)+strlen(temp_buffer)+delimiter_len+1))==NULL)
						continue;
					strcat(*output,arg2);
					strcat(*output,temp_buffer);
					}
				my_free(temp_buffer);
				}
			}
		break;

		/***********************/
		/* CONTACTGROUP MACROS */
		/***********************/
	case MACRO_CONTACTGROUPNAME:
	case MACRO_CONTACTGROUPALIAS:
	case MACRO_CONTACTGROUPMEMBERS:
		/* a standard contactgroup macro */
		/* use the saved contactgroup pointer */
		if(arg1==NULL){
			if((temp_contactgroup=macro_contactgroup_ptr)==NULL)
				return ERROR;
			}

		/* else find the contactgroup for on-demand macros */
		else{
			if((temp_contactgroup=find_contactgroup(arg1))==NULL)
				return ERROR;
			}

		/* get the contactgroup macro value */
		result=grab_standard_contactgroup_macro(macro_type,temp_contactgroup,output);
		break;

		/***********************/
		/* NOTIFICATION MACROS */
		/***********************/
	case MACRO_NOTIFICATIONTYPE:
	case MACRO_NOTIFICATIONNUMBER:
	case MACRO_NOTIFICATIONRECIPIENTS:
	case MACRO_NOTIFICATIONISESCALATED:
	case MACRO_NOTIFICATIONAUTHOR:
	case MACRO_NOTIFICATIONAUTHORNAME:
	case MACRO_NOTIFICATIONAUTHORALIAS:
	case MACRO_NOTIFICATIONCOMMENT:

		/* notification macros have already been pre-computed */
		*output=macro_x[macro_type];
		*free_macro=FALSE;
		break;

		/********************/
		/* DATE/TIME MACROS */
		/********************/
	case MACRO_LONGDATETIME:
	case MACRO_SHORTDATETIME:
	case MACRO_DATE:
	case MACRO_TIME:
	case MACRO_TIMET:
	case MACRO_ISVALIDTIME:
	case MACRO_NEXTVALIDTIME:

		/* calculate macros */
		result=grab_datetime_macro(macro_type,arg1,arg2,output);
		break;

		/*****************/
		/* STATIC MACROS */
		/*****************/
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

		/* no need to do any more work - these are already precomputed for us */
		*output=macro_x[macro_type];
		*free_macro=FALSE;
		break;

		/******************/
		/* SUMMARY MACROS */
		/******************/
	case MACRO_TOTALHOSTSUP:
	case MACRO_TOTALHOSTSDOWN:
	case MACRO_TOTALHOSTSUNREACHABLE:
	case MACRO_TOTALHOSTSDOWNUNHANDLED:
	case MACRO_TOTALHOSTSUNREACHABLEUNHANDLED:
	case MACRO_TOTALHOSTPROBLEMS:
	case MACRO_TOTALHOSTPROBLEMSUNHANDLED:
	case MACRO_TOTALSERVICESOK:
	case MACRO_TOTALSERVICESWARNING:
	case MACRO_TOTALSERVICESCRITICAL:
	case MACRO_TOTALSERVICESUNKNOWN:
	case MACRO_TOTALSERVICESWARNINGUNHANDLED:
	case MACRO_TOTALSERVICESCRITICALUNHANDLED:
	case MACRO_TOTALSERVICESUNKNOWNUNHANDLED:
	case MACRO_TOTALSERVICEPROBLEMS:
	case MACRO_TOTALSERVICEPROBLEMSUNHANDLED:

#ifdef NSCORE
		/* generate summary macros if needed */
		if(macro_x[MACRO_TOTALHOSTSUP]==NULL){

			/* get host totals */
			for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

				/* filter totals based on contact if necessary */
				if(macro_contact_ptr!=NULL)
					authorized=is_contact_for_host(temp_host,macro_contact_ptr);

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
				if(macro_contact_ptr!=NULL)
					authorized=is_contact_for_service(temp_service,macro_contact_ptr);

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

			/* these macros are time-intensive to compute, and will likely be used together, so save them all for future use */
			for(x=MACRO_TOTALHOSTSUP;x<=MACRO_TOTALSERVICEPROBLEMSUNHANDLED;x++)
				my_free(macro_x[x]);
			asprintf(&macro_x[MACRO_TOTALHOSTSUP],"%d",hosts_up);
			asprintf(&macro_x[MACRO_TOTALHOSTSDOWN],"%d",hosts_down);
			asprintf(&macro_x[MACRO_TOTALHOSTSUNREACHABLE],"%d",hosts_unreachable);
			asprintf(&macro_x[MACRO_TOTALHOSTSDOWNUNHANDLED],"%d",hosts_down_unhandled);
			asprintf(&macro_x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED],"%d",hosts_unreachable_unhandled);
			asprintf(&macro_x[MACRO_TOTALHOSTPROBLEMS],"%d",host_problems);
			asprintf(&macro_x[MACRO_TOTALHOSTPROBLEMSUNHANDLED],"%d",host_problems_unhandled);
			asprintf(&macro_x[MACRO_TOTALSERVICESOK],"%d",services_ok);
			asprintf(&macro_x[MACRO_TOTALSERVICESWARNING],"%d",services_warning);
			asprintf(&macro_x[MACRO_TOTALSERVICESCRITICAL],"%d",services_critical);
			asprintf(&macro_x[MACRO_TOTALSERVICESUNKNOWN],"%d",services_unknown);
			asprintf(&macro_x[MACRO_TOTALSERVICESWARNINGUNHANDLED],"%d",services_warning_unhandled);
			asprintf(&macro_x[MACRO_TOTALSERVICESCRITICALUNHANDLED],"%d",services_critical_unhandled);
			asprintf(&macro_x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED],"%d",services_unknown_unhandled);
			asprintf(&macro_x[MACRO_TOTALSERVICEPROBLEMS],"%d",service_problems);
			asprintf(&macro_x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED],"%d",service_problems_unhandled);
			}

		/* return only the macro the user requested */
		*output=macro_x[macro_type];

		/* tell caller to NOT free memory when done */
		*free_macro=FALSE;
#endif
		break;

	default:
#ifdef NSCORE
		log_debug_info(DEBUGL_MACROS,0,"UNHANDLED MACRO #%d! THIS IS A BUG!\n",macro_type);
#endif
		return ERROR;
		break;
		}

	return result;
	}



/* calculates the value of a custom macro */
int grab_custom_macro_value(char *macro_name, char *arg1, char *arg2, char **output){
	host *temp_host=NULL;
	hostgroup *temp_hostgroup=NULL;
	hostsmember *temp_hostsmember=NULL;
	service *temp_service=NULL;
	servicegroup *temp_servicegroup=NULL;
	servicesmember *temp_servicesmember=NULL;
	contact *temp_contact=NULL;
	contactgroup *temp_contactgroup=NULL;
	contactsmember *temp_contactsmember=NULL;
	int delimiter_len=0;
	char *temp_buffer=NULL;
	int result=OK;

	if(macro_name==NULL || output==NULL)
		return ERROR;

	/***** CUSTOM HOST MACRO *****/
	if(strstr(macro_name,"_HOST")==macro_name){

		/* a standard host macro */
		if(arg2==NULL){

			/* find the host for on-demand macros */
			if(arg1){
				if((temp_host=find_host(arg1))==NULL)
					return ERROR;
				}

			/* else use saved host pointer */
			else if((temp_host=macro_host_ptr)==NULL)
				return ERROR;

			/* get the host macro value */
			result=grab_custom_object_macro(macro_name+5,temp_host->custom_variables,output);
			}

		/* a host macro with a hostgroup name and delimiter */
		else{
			if((temp_hostgroup=find_hostgroup(arg1))==NULL)
				return ERROR;

			delimiter_len=strlen(arg2);

			/* concatenate macro values for all hostgroup members */
			for(temp_hostsmember=temp_hostgroup->members;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

#ifdef NSCORE
				if((temp_host=temp_hostsmember->host_ptr)==NULL)
					continue;
#else
				if((temp_host=find_host(temp_hostsmember->host_name))==NULL)
					continue;
#endif

				/* get the macro value for this host */
				grab_custom_macro_value(macro_name,temp_host->name,NULL,&temp_buffer);

				if(temp_buffer==NULL)
					continue;
				
				/* add macro value to already running macro */
				if(*output==NULL)
					*output=(char *)strdup(temp_buffer);
				else{
					if((*output=(char *)realloc(*output,strlen(*output)+strlen(temp_buffer)+delimiter_len+1))==NULL)
						continue;
					strcat(*output,arg2);
					strcat(*output,temp_buffer);
					}
				my_free(temp_buffer);
				}
			}
		}

	/***** CUSTOM SERVICE MACRO *****/
	else if(strstr(macro_name,"_SERVICE")==macro_name){

		/* use saved service pointer */
		if(arg1==NULL && arg2==NULL){

			if((temp_service=macro_service_ptr)==NULL)
				return ERROR;

			/* get the service macro value */
			result=grab_custom_object_macro(macro_name+8,temp_service->custom_variables,output);
			}

		/* else and ondemand macro... */
		else{

			/* if first arg is blank, it means use the current host name */
			if(macro_host_ptr==NULL)
				return ERROR;
			if((temp_service=find_service((macro_host_ptr)?macro_host_ptr->name:NULL,arg2))){

				/* get the service macro value */
				result=grab_custom_object_macro(macro_name+8,temp_service->custom_variables,output);
				}

			/* else we have a service macro with a servicegroup name and a delimiter... */
			else{

				if((temp_servicegroup=find_servicegroup(arg1))==NULL)
					return ERROR;

				delimiter_len=strlen(arg2);

				/* concatenate macro values for all servicegroup members */
				for(temp_servicesmember=temp_servicegroup->members;temp_servicesmember!=NULL;temp_servicesmember=temp_servicesmember->next){

#ifdef NSCORE
					if((temp_service=temp_servicesmember->service_ptr)==NULL)
						continue;
#else
					if((temp_service=find_service(temp_servicesmember->host_name,temp_servicesmember->service_description))==NULL)
						continue;
#endif

					/* get the macro value for this service */
					grab_custom_macro_value(macro_name,temp_service->host_name,temp_service->description,&temp_buffer);

					if(temp_buffer==NULL)
						continue;
				
					/* add macro value to already running macro */
					if(*output==NULL)
						*output=(char *)strdup(temp_buffer);
					else{
						if((*output=(char *)realloc(*output,strlen(*output)+strlen(temp_buffer)+delimiter_len+1))==NULL)
							continue;
						strcat(*output,arg2);
						strcat(*output,temp_buffer);
						}
					my_free(temp_buffer);
					}
				}
			}
		}

	/***** CUSTOM CONTACT VARIABLE *****/
	else if(strstr(macro_name,"_CONTACT")==macro_name){

		/* a standard contact macro */
		if(arg2==NULL){

			/* find the contact for on-demand macros */
			if(arg1){
				if((temp_contact=find_contact(arg1))==NULL)
					return ERROR;
				}

			/* else use saved contact pointer */
			else if((temp_contact=macro_contact_ptr)==NULL)
				return ERROR;

			/* get the contact macro value */
			result=grab_custom_object_macro(macro_name+8,temp_contact->custom_variables,output);
			}

		/* a contact macro with a contactgroup name and delimiter */
		else{

			if((temp_contactgroup=find_contactgroup(arg1))==NULL)
				return ERROR;

			delimiter_len=strlen(arg2);

			/* concatenate macro values for all contactgroup members */
			for(temp_contactsmember=temp_contactgroup->members;temp_contactsmember!=NULL;temp_contactsmember=temp_contactsmember->next){

#ifdef NSCORE
				if((temp_contact=temp_contactsmember->contact_ptr)==NULL)
					continue;
#else
				if((temp_contact=find_contact(temp_contactsmember->contact_name))==NULL)
					continue;
#endif

				/* get the macro value for this contact */
				grab_custom_macro_value(macro_name,temp_contact->name,NULL,&temp_buffer);

				if(temp_buffer==NULL)
					continue;
				
				/* add macro value to already running macro */
				if(*output==NULL)
					*output=(char *)strdup(temp_buffer);
				else{
					if((*output=(char *)realloc(*output,strlen(*output)+strlen(temp_buffer)+delimiter_len+1))==NULL)
						continue;
					strcat(*output,arg2);
					strcat(*output,temp_buffer);
					}
				my_free(temp_buffer);
				}
			}
		}

	else
		return ERROR;

	return result;
	}



/* calculates a date/time macro */
int grab_datetime_macro(int macro_type, char *arg1, char *arg2, char **output){
	time_t current_time=0L;
	timeperiod *temp_timeperiod=NULL;
	time_t test_time=0L;
#ifdef NSCORE
	time_t next_valid_time=0L;
#endif

	if(output==NULL)
		return ERROR;

	/* get the current time */
	time(&current_time);

	/* parse args, do prep work */
	switch(macro_type){

	case MACRO_ISVALIDTIME:
	case MACRO_NEXTVALIDTIME:

		/* find the timeperiod */
		if((temp_timeperiod=find_timeperiod(arg1))==NULL)
			return ERROR;

		/* what timestamp should we use? */
		if(arg2)
			test_time=(time_t)strtoul(arg2,NULL,0);
		else
			test_time=current_time;
		break;

	default:
		break;
		}

	/* calculate the value */
	switch(macro_type){

	case MACRO_LONGDATETIME:
		if(*output==NULL)
			*output=(char *)malloc(MAX_DATETIME_LENGTH);
		if(*output)
			get_datetime_string(&current_time,*output,MAX_DATETIME_LENGTH,LONG_DATE_TIME);
		break;

	case MACRO_SHORTDATETIME:
		if(*output==NULL)
			*output=(char *)malloc(MAX_DATETIME_LENGTH);
		if(*output)
			get_datetime_string(&current_time,*output,MAX_DATETIME_LENGTH,SHORT_DATE_TIME);
		break;

	case MACRO_DATE:
		if(*output==NULL)
			*output=(char *)malloc(MAX_DATETIME_LENGTH);
		if(*output)
			get_datetime_string(&current_time,*output,MAX_DATETIME_LENGTH,SHORT_DATE);
		break;

	case MACRO_TIME:
		if(*output==NULL)
			*output=(char *)malloc(MAX_DATETIME_LENGTH);
		if(*output)
			get_datetime_string(&current_time,*output,MAX_DATETIME_LENGTH,SHORT_TIME);
		break;

	case MACRO_TIMET:
		asprintf(output,"%lu",(unsigned long)current_time);
		break;

#ifdef NSCORE
	case MACRO_ISVALIDTIME:
		asprintf(output,"%d",(check_time_against_period(test_time,temp_timeperiod)==OK)?1:0);
		break;

	case MACRO_NEXTVALIDTIME:
		get_next_valid_time(test_time,&next_valid_time,temp_timeperiod);
		if(next_valid_time==test_time && check_time_against_period(test_time,temp_timeperiod)==ERROR)
			next_valid_time=(time_t)0L;
		asprintf(output,"%lu",(unsigned long)next_valid_time);
		break;
#endif

	default:
		return ERROR;
		break;
		}

	return OK;
        }



/* calculates a host macro */
int grab_standard_host_macro(int macro_type, host *temp_host, char **output, int *free_macro){
	char *temp_buffer=NULL;
#ifdef NSCORE
	hostgroup *temp_hostgroup=NULL;
	servicesmember *temp_servicesmember=NULL;
	service *temp_service=NULL;
	objectlist *temp_objectlist=NULL;
	time_t current_time=0L;
	unsigned long duration=0L;
	int days=0;
	int hours=0;
	int minutes=0;
	int seconds=0;
	char *buf1=NULL;
	char *buf2=NULL;
	int total_host_services=0;
	int total_host_services_ok=0;
	int total_host_services_warning=0;
	int total_host_services_unknown=0;
	int total_host_services_critical=0;
#endif

	if(temp_host==NULL || output==NULL || free_macro==NULL)
		return ERROR;

	/* BY DEFAULT TELL CALLER TO FREE MACRO BUFFER WHEN DONE */
	*free_macro=TRUE;

	/* get the macro */
	switch(macro_type){

	case MACRO_HOSTNAME:
		*output=(char *)strdup(temp_host->name);
		break;
	case MACRO_HOSTDISPLAYNAME:
		if(temp_host->display_name)
			*output=(char *)strdup(temp_host->display_name);
		break;
	case MACRO_HOSTALIAS:
		*output=(char *)strdup(temp_host->alias);
		break;
	case MACRO_HOSTADDRESS:
		*output=(char *)strdup(temp_host->address);
		break;
#ifdef NSCORE
	case MACRO_HOSTSTATE:
		if(temp_host->current_state==HOST_DOWN)
			*output=(char *)strdup("DOWN");
		else if(temp_host->current_state==HOST_UNREACHABLE)
			*output=(char *)strdup("UNREACHABLE");
		else
			*output=(char *)strdup("UP");
		break;
	case MACRO_HOSTSTATEID:
		asprintf(output,"%d",temp_host->current_state);
		break;
	case MACRO_LASTHOSTSTATE:
		if(temp_host->last_state==HOST_DOWN)
			*output=(char *)strdup("DOWN");
		else if(temp_host->last_state==HOST_UNREACHABLE)
			*output=(char *)strdup("UNREACHABLE");
		else
			*output=(char *)strdup("UP");
		break;
	case MACRO_LASTHOSTSTATEID:
		asprintf(output,"%d",temp_host->last_state);
		break;
	case MACRO_HOSTCHECKTYPE:
		asprintf(output,"%s",(temp_host->check_type==HOST_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");
		break;
	case MACRO_HOSTSTATETYPE:
		asprintf(output,"%s",(temp_host->state_type==HARD_STATE)?"HARD":"SOFT");
		break;
	case MACRO_HOSTOUTPUT:
		if(temp_host->plugin_output)
			*output=(char *)strdup(temp_host->plugin_output);
		break;
	case MACRO_LONGHOSTOUTPUT:
		if(temp_host->long_plugin_output)
			*output=(char *)strdup(temp_host->long_plugin_output);
		break;
	case MACRO_HOSTPERFDATA:
		if(temp_host->perf_data)
			*output=(char *)strdup(temp_host->perf_data);
		break;
#endif
	case MACRO_HOSTCHECKCOMMAND:
		if(temp_host->host_check_command)
			*output=(char *)strdup(temp_host->host_check_command);
		break;
#ifdef NSCORE
	case MACRO_HOSTATTEMPT:
		asprintf(output,"%d",temp_host->current_attempt);
		break;
	case MACRO_MAXHOSTATTEMPTS:
		asprintf(output,"%d",temp_host->max_attempts);
		break;
	case MACRO_HOSTDOWNTIME:
		asprintf(output,"%d",temp_host->scheduled_downtime_depth);
		break;
	case MACRO_HOSTPERCENTCHANGE:
		asprintf(output,"%.2f",temp_host->percent_state_change);
		break;
	case MACRO_HOSTDURATIONSEC:
	case MACRO_HOSTDURATION:
		time(&current_time);
		duration=(unsigned long)(current_time-temp_host->last_state_change);

		if(macro_type==MACRO_HOSTDURATIONSEC)
			asprintf(output,"%lu",duration);
		else{

			days=duration/86400;
			duration-=(days*86400);
			hours=duration/3600;
			duration-=(hours*3600);
			minutes=duration/60;
			duration-=(minutes*60);
			seconds=duration;
			asprintf(output,"%dd %dh %dm %ds",days,hours,minutes,seconds);
			}
		break;
	case MACRO_HOSTEXECUTIONTIME:
		asprintf(output,"%.3f",temp_host->execution_time);
		break;
	case MACRO_HOSTLATENCY:
		asprintf(output,"%.3f",temp_host->latency);
		break;
	case MACRO_LASTHOSTCHECK:
		asprintf(output,"%lu",(unsigned long)temp_host->last_check);
		break;
	case MACRO_LASTHOSTSTATECHANGE:
		asprintf(output,"%lu",(unsigned long)temp_host->last_state_change);
		break;
	case MACRO_LASTHOSTUP:
		asprintf(output,"%lu",(unsigned long)temp_host->last_time_up);
		break;
	case MACRO_LASTHOSTDOWN:
		asprintf(output,"%lu",(unsigned long)temp_host->last_time_down);
		break;
	case MACRO_LASTHOSTUNREACHABLE:
		asprintf(output,"%lu",(unsigned long)temp_host->last_time_unreachable);
		break;
	case MACRO_HOSTNOTIFICATIONNUMBER:
		asprintf(output,"%d",temp_host->current_notification_number);
		break;
	case MACRO_HOSTNOTIFICATIONID:
		asprintf(output,"%lu",temp_host->current_notification_id);
		break;
	case MACRO_HOSTEVENTID:
		asprintf(output,"%lu",temp_host->current_event_id);
		break;
	case MACRO_LASTHOSTEVENTID:
		asprintf(output,"%lu",temp_host->last_event_id);
		break;
	case MACRO_HOSTPROBLEMID:
		asprintf(output,"%lu",temp_host->current_problem_id);
		break;
	case MACRO_LASTHOSTPROBLEMID:
		asprintf(output,"%lu",temp_host->last_problem_id);
		break;
#endif
	case MACRO_HOSTACTIONURL:
		if(temp_host->action_url)
			*output=(char *)strdup(temp_host->action_url);
		break;
	case MACRO_HOSTNOTESURL:
		if(temp_host->notes_url)
			*output=(char *)strdup(temp_host->notes_url);
		break;
	case MACRO_HOSTNOTES:
		if(temp_host->notes)
			*output=(char *)strdup(temp_host->notes);
		break;
#ifdef NSCORE
	case MACRO_HOSTGROUPNAMES:
		/* find all hostgroups this host is associated with */
		for(temp_objectlist=temp_host->hostgroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

			if((temp_hostgroup=(hostgroup *)temp_objectlist->object_ptr)==NULL)
				continue;

			asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_hostgroup->group_name);
			my_free(buf2);
			buf2=buf1;
			}
		if(buf2){
			*output=(char *)strdup(buf2);
			my_free(buf2);
			}
		break;
	case MACRO_TOTALHOSTSERVICES:
	case MACRO_TOTALHOSTSERVICESOK:
	case MACRO_TOTALHOSTSERVICESWARNING:
	case MACRO_TOTALHOSTSERVICESUNKNOWN:
	case MACRO_TOTALHOSTSERVICESCRITICAL:

		/* generate host service summary macros (if they haven't already been computed) */
		if(macro_x[MACRO_TOTALHOSTSERVICES]==NULL){

			for(temp_servicesmember=temp_host->services;temp_servicesmember!=NULL;temp_servicesmember=temp_servicesmember->next){
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

			/* these macros are time-intensive to compute, and will likely be used together, so save them all for future use */
			my_free(macro_x[MACRO_TOTALHOSTSERVICES]);
			asprintf(&macro_x[MACRO_TOTALHOSTSERVICES],"%d",total_host_services);
			my_free(macro_x[MACRO_TOTALHOSTSERVICESOK]);
			asprintf(&macro_x[MACRO_TOTALHOSTSERVICESOK],"%d",total_host_services_ok);
			my_free(macro_x[MACRO_TOTALHOSTSERVICESWARNING]);
			asprintf(&macro_x[MACRO_TOTALHOSTSERVICESWARNING],"%d",total_host_services_warning);
			my_free(macro_x[MACRO_TOTALHOSTSERVICESUNKNOWN]);
			asprintf(&macro_x[MACRO_TOTALHOSTSERVICESUNKNOWN],"%d",total_host_services_unknown);
			my_free(macro_x[MACRO_TOTALHOSTSERVICESCRITICAL]);
			asprintf(&macro_x[MACRO_TOTALHOSTSERVICESCRITICAL],"%d",total_host_services_critical);
			}

		/* return only the macro the user requested */
		*output=macro_x[macro_type];

		/* tell caller to NOT free memory when done */
		*free_macro=FALSE;
		break;
#endif
		/***************/
		/* MISC MACROS */
		/***************/
	case MACRO_HOSTACKAUTHOR:
	case MACRO_HOSTACKAUTHORNAME:
	case MACRO_HOSTACKAUTHORALIAS:
	case MACRO_HOSTACKCOMMENT:
		/* no need to do any more work - these are already precomputed elsewhere */
		/* NOTE: these macros won't work as on-demand macros */
		*output=macro_x[macro_type];
		*free_macro=FALSE;
		break;

	default:
#ifdef NSCORE
		log_debug_info(DEBUGL_MACROS,0,"UNHANDLED HOST MACRO #%d! THIS IS A BUG!\n",macro_type);
#endif
		return ERROR;
		break;
		}

	/* post-processing */
	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	switch(macro_type){
	case MACRO_HOSTACTIONURL:
	case MACRO_HOSTNOTESURL:
		process_macros(*output,&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(*output);
		*output=temp_buffer;
		break;
	case MACRO_HOSTNOTES:
		process_macros(*output,&temp_buffer,0);
		my_free(*output);
		*output=temp_buffer;
		break;
	default:
		break;
		}

	return OK;
	}



/* computes a hostgroup macro */
int grab_standard_hostgroup_macro(int macro_type, hostgroup *temp_hostgroup, char **output){
	hostsmember *temp_hostsmember=NULL;
	char *temp_buffer=NULL;

	if(temp_hostgroup==NULL || output==NULL)
		return ERROR;

	/* get the macro value */
	switch(macro_type){
	case MACRO_HOSTGROUPNAME:
		*output=(char *)strdup(temp_hostgroup->group_name);
		break;
	case MACRO_HOSTGROUPALIAS:
		if(temp_hostgroup->alias)
			*output=(char *)strdup(temp_hostgroup->alias);
		break;
	case MACRO_HOSTGROUPMEMBERS:
		/* get the group members */
		for(temp_hostsmember=temp_hostgroup->members;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){
			if(temp_hostsmember->host_name==NULL)
				continue;
			if(*output==NULL)
				*output=(char *)strdup(temp_hostsmember->host_name);
			else if((*output=(char *)realloc(*output,strlen(*output)+strlen(temp_hostsmember->host_name)+2))){
				strcat(*output,",");
				strcat(*output,temp_hostsmember->host_name);
				}
			}
		break;
	case MACRO_HOSTGROUPACTIONURL:
		if(temp_hostgroup->action_url)
			*output=(char *)strdup(temp_hostgroup->action_url);
		break;
	case MACRO_HOSTGROUPNOTESURL:
		if(temp_hostgroup->notes_url)
			*output=(char *)strdup(temp_hostgroup->notes_url);
		break;
	case MACRO_HOSTGROUPNOTES:
		if(temp_hostgroup->notes)
			*output=(char *)strdup(temp_hostgroup->notes);
		break;
	default:
#ifdef NSCORE
		log_debug_info(DEBUGL_MACROS,0,"UNHANDLED HOSTGROUP MACRO #%d! THIS IS A BUG!\n",macro_type);
#endif
		return ERROR;
		break;
		}

	/* post-processing */
	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	switch(macro_type){
	case MACRO_HOSTGROUPACTIONURL:
	case MACRO_HOSTGROUPNOTESURL:
		process_macros(*output,&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(*output);
		*output=temp_buffer;
		break;
	case MACRO_HOSTGROUPNOTES:
		process_macros(*output,&temp_buffer,0);
		my_free(*output);
		*output=temp_buffer;
		break;
	default:
		break;
		}

	return OK;
	}



/* computes a service macro */
int grab_standard_service_macro(int macro_type, service *temp_service, char **output, int *free_macro){
	char *temp_buffer=NULL;
#ifdef NSCORE
	servicegroup *temp_servicegroup=NULL;
	objectlist *temp_objectlist=NULL;
	time_t current_time=0L;
	unsigned long duration=0L;
	int days=0;
	int hours=0;
	int minutes=0;
	int seconds=0;
	char *buf1=NULL;
	char *buf2=NULL;
#endif

	if(temp_service==NULL || output==NULL)
		return ERROR;

	/* BY DEFAULT TELL CALLER TO FREE MACRO BUFFER WHEN DONE */
	*free_macro=TRUE;

	/* get the macro value */
	switch(macro_type){
	case MACRO_SERVICEDESC:
		*output=(char *)strdup(temp_service->description);
		break;
	case MACRO_SERVICEDISPLAYNAME:
		if(temp_service->display_name)
			*output=(char *)strdup(temp_service->display_name);
		break;
#ifdef NSCORE
	case MACRO_SERVICEOUTPUT:
		if(temp_service->plugin_output)
			*output=(char *)strdup(temp_service->plugin_output);
		break;
	case MACRO_LONGSERVICEOUTPUT:
		if(temp_service->long_plugin_output)
			*output=(char *)strdup(temp_service->long_plugin_output);
		break;
	case MACRO_SERVICEPERFDATA:
		if(temp_service->perf_data)
			*output=(char *)strdup(temp_service->perf_data);
		break;
#endif
	case MACRO_SERVICECHECKCOMMAND:
		if(temp_service->service_check_command)
			*output=(char *)strdup(temp_service->service_check_command);
		break;
#ifdef NSCORE
	case MACRO_SERVICECHECKTYPE:
		*output=(char *)strdup((temp_service->check_type==SERVICE_CHECK_PASSIVE)?"PASSIVE":"ACTIVE");
		break;
	case MACRO_SERVICESTATETYPE:
		*output=(char *)strdup((temp_service->state_type==HARD_STATE)?"HARD":"SOFT");
		break;
	case MACRO_SERVICESTATE:
		if(temp_service->current_state==STATE_OK)
			*output=(char *)strdup("OK");
		else if(temp_service->current_state==STATE_WARNING)
			*output=(char *)strdup("WARNING");
		else if(temp_service->current_state==STATE_CRITICAL)
			*output=(char *)strdup("CRITICAL");
		else
			*output=(char *)strdup("UNKNOWN");
		break;
	case MACRO_SERVICESTATEID:
		asprintf(output,"%d",temp_service->current_state);
		break;
	case MACRO_LASTSERVICESTATE:
		if(temp_service->last_state==STATE_OK)
			*output=(char *)strdup("OK");
		else if(temp_service->last_state==STATE_WARNING)
			*output=(char *)strdup("WARNING");
		else if(temp_service->last_state==STATE_CRITICAL)
			*output=(char *)strdup("CRITICAL");
		else
			*output=(char *)strdup("UNKNOWN");
		break;
	case MACRO_LASTSERVICESTATEID:
		asprintf(output,"%d",temp_service->last_state);
		break;
#endif
	case MACRO_SERVICEISVOLATILE:
		asprintf(output,"%d",temp_service->is_volatile);
		break;
#ifdef NSCORE
	case MACRO_SERVICEATTEMPT:
		asprintf(output,"%d",temp_service->current_attempt);
		break;
	case MACRO_MAXSERVICEATTEMPTS:
		asprintf(output,"%d",temp_service->max_attempts);
		break;
	case MACRO_SERVICEEXECUTIONTIME:
		asprintf(output,"%.3f",temp_service->execution_time);
		break;
	case MACRO_SERVICELATENCY:
		asprintf(output,"%.3f",temp_service->latency);
		break;
	case MACRO_LASTSERVICECHECK:
		asprintf(output,"%lu",(unsigned long)temp_service->last_check);
		break;
	case MACRO_LASTSERVICESTATECHANGE:
		asprintf(output,"%lu",(unsigned long)temp_service->last_state_change);
		break;
	case MACRO_LASTSERVICEOK:
		asprintf(output,"%lu",(unsigned long)temp_service->last_time_ok);
		break;
	case MACRO_LASTSERVICEWARNING:
		asprintf(output,"%lu",(unsigned long)temp_service->last_time_warning);
		break;
	case MACRO_LASTSERVICEUNKNOWN:
		asprintf(output,"%lu",(unsigned long)temp_service->last_time_unknown);
		break;
	case MACRO_LASTSERVICECRITICAL:
		asprintf(output,"%lu",(unsigned long)temp_service->last_time_critical);
		break;
	case MACRO_SERVICEDOWNTIME:
		asprintf(output,"%d",temp_service->scheduled_downtime_depth);
		break;
	case MACRO_SERVICEPERCENTCHANGE:
		asprintf(output,"%.2f",temp_service->percent_state_change);
		break;
	case MACRO_SERVICEDURATIONSEC:
	case MACRO_SERVICEDURATION:

		time(&current_time);
		duration=(unsigned long)(current_time-temp_service->last_state_change);

		/* get the state duration in seconds */
		if(macro_type==MACRO_SERVICEDURATIONSEC)
			asprintf(output,"%lu",duration);

		/* get the state duration */
		else{
			days=duration/86400;
			duration-=(days*86400);
			hours=duration/3600;
			duration-=(hours*3600);
			minutes=duration/60;
			duration-=(minutes*60);
			seconds=duration;
			asprintf(output,"%dd %dh %dm %ds",days,hours,minutes,seconds);
			}
		break;
	case MACRO_SERVICENOTIFICATIONNUMBER:
		asprintf(output,"%d",temp_service->current_notification_number);
		break;
	case MACRO_SERVICENOTIFICATIONID:
		asprintf(output,"%lu",temp_service->current_notification_id);
		break;
	case MACRO_SERVICEEVENTID:
		asprintf(output,"%lu",temp_service->current_event_id);
		break;
	case MACRO_LASTSERVICEEVENTID:
		asprintf(output,"%lu",temp_service->last_event_id);
		break;
	case MACRO_SERVICEPROBLEMID:
		asprintf(output,"%lu",temp_service->current_problem_id);
		break;
	case MACRO_LASTSERVICEPROBLEMID:
		asprintf(output,"%lu",temp_service->last_problem_id);
		break;
#endif
	case MACRO_SERVICEACTIONURL:
		if(temp_service->action_url)
			*output=(char *)strdup(temp_service->action_url);
		break;
	case MACRO_SERVICENOTESURL:
		if(temp_service->notes_url)
			*output=(char *)strdup(temp_service->notes_url);
		break;
	case MACRO_SERVICENOTES:
		if(temp_service->notes)
			*output=(char *)strdup(temp_service->notes);
		break;
#ifdef NSCORE
	case MACRO_SERVICEGROUPNAMES:
		/* find all servicegroups this service is associated with */
		for(temp_objectlist=temp_service->servicegroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

			if((temp_servicegroup=(servicegroup *)temp_objectlist->object_ptr)==NULL)
				continue;

			asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_servicegroup->group_name);
			my_free(buf2);
			buf2=buf1;
			}
		if(buf2){
			*output=(char *)strdup(buf2);
			my_free(buf2);
			}
		break;
#endif
		/***************/
		/* MISC MACROS */
		/***************/
	case MACRO_SERVICEACKAUTHOR:
	case MACRO_SERVICEACKAUTHORNAME:
	case MACRO_SERVICEACKAUTHORALIAS:
	case MACRO_SERVICEACKCOMMENT:
		/* no need to do any more work - these are already precomputed elsewhere */
		/* NOTE: these macros won't work as on-demand macros */
		*output=macro_x[macro_type];
		*free_macro=FALSE;
		break;

	default:
#ifdef NSCORE
		log_debug_info(DEBUGL_MACROS,0,"UNHANDLED SERVICE MACRO #%d! THIS IS A BUG!\n",macro_type);
#endif
		return ERROR;
		break;
		}

	/* post-processing */
	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	switch(macro_type){
	case MACRO_SERVICEACTIONURL:
	case MACRO_SERVICENOTESURL:
		process_macros(*output,&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(*output);
		*output=temp_buffer;
		break;
	case MACRO_SERVICENOTES:
		process_macros(*output,&temp_buffer,0);
		my_free(*output);
		*output=temp_buffer;
		break;
	default:
		break;
		}

	return OK;
	}



/* computes a servicegroup macro */
int grab_standard_servicegroup_macro(int macro_type, servicegroup *temp_servicegroup, char **output){
	servicesmember *temp_servicesmember=NULL;
	char *temp_buffer=NULL;

	if(temp_servicegroup==NULL || output==NULL)
		return ERROR;

	/* get the macro value */
	switch(macro_type){
	case MACRO_SERVICEGROUPNAME:
		*output=(char *)strdup(temp_servicegroup->group_name);
		break;
	case MACRO_SERVICEGROUPALIAS:
		if(temp_servicegroup->alias)
			*output=(char *)strdup(temp_servicegroup->alias);
		break;
	case MACRO_SERVICEGROUPMEMBERS:
		/* get the group members */
		for(temp_servicesmember=temp_servicegroup->members;temp_servicesmember!=NULL;temp_servicesmember=temp_servicesmember->next){
			if(temp_servicesmember->host_name==NULL || temp_servicesmember->service_description==NULL)
				continue;
			if(*output==NULL){
				if((*output=(char *)malloc(strlen(temp_servicesmember->host_name)+strlen(temp_servicesmember->service_description)+2))){
					sprintf(*output,"%s,%s",temp_servicesmember->host_name,temp_servicesmember->service_description);
					}
				}
			else if((*output=(char *)realloc(*output,strlen(*output)+strlen(temp_servicesmember->host_name)+strlen(temp_servicesmember->service_description)+3))){
				strcat(*output,",");
				strcat(*output,temp_servicesmember->host_name);
				strcat(*output,",");
				strcat(*output,temp_servicesmember->service_description);
				}
			}
		break;
	case MACRO_SERVICEGROUPACTIONURL:
		if(temp_servicegroup->action_url)
			*output=(char *)strdup(temp_servicegroup->action_url);
		break;
	case MACRO_SERVICEGROUPNOTESURL:
		if(temp_servicegroup->notes_url)
			*output=(char *)strdup(temp_servicegroup->notes_url);
		break;
	case MACRO_SERVICEGROUPNOTES:
		if(temp_servicegroup->notes)
			*output=(char *)strdup(temp_servicegroup->notes);
		break;
	default:
#ifdef NSCORE
		log_debug_info(DEBUGL_MACROS,0,"UNHANDLED SERVICEGROUP MACRO #%d! THIS IS A BUG!\n",macro_type);
#endif
		return ERROR;
		break;
		}

	/* post-processing */
	/* notes, notes URL and action URL macros may themselves contain macros, so process them... */
	switch(macro_type){
	case MACRO_SERVICEGROUPACTIONURL:
	case MACRO_SERVICEGROUPNOTESURL:
		process_macros(*output,&temp_buffer,URL_ENCODE_MACRO_CHARS);
		my_free(*output);
		*output=temp_buffer;
		break;
	case MACRO_SERVICEGROUPNOTES:
		process_macros(*output,&temp_buffer,0);
		my_free(*output);
		*output=temp_buffer;
		break;
	default:
		break;
		}

	return OK;
	}



/* computes a contact macro */
int grab_standard_contact_macro(int macro_type, contact *temp_contact, char **output){
#ifdef NSCORE
	contactgroup *temp_contactgroup=NULL;
	objectlist *temp_objectlist=NULL;
	char *buf1=NULL;
	char *buf2=NULL;
#endif

	if(temp_contact==NULL || output==NULL)
		return ERROR;

	/* get the macro value */
	switch(macro_type){
	case MACRO_CONTACTNAME:
		*output=(char *)strdup(temp_contact->name);
		break;
	case MACRO_CONTACTALIAS:
		*output=(char *)strdup(temp_contact->alias);
		break;
	case MACRO_CONTACTEMAIL:
		if(temp_contact->email)
			*output=(char *)strdup(temp_contact->email);
		break;
	case MACRO_CONTACTPAGER:
		if(temp_contact->pager)
			*output=(char *)strdup(temp_contact->pager);
		break;
#ifdef NSCORE
	case MACRO_CONTACTGROUPNAMES:
		/* get the contactgroup names */
		/* find all contactgroups this contact is a member of */
		for(temp_objectlist=temp_contact->contactgroups_ptr;temp_objectlist!=NULL;temp_objectlist=temp_objectlist->next){

			if((temp_contactgroup=(contactgroup *)temp_objectlist->object_ptr)==NULL)
				continue;

			asprintf(&buf1,"%s%s%s",(buf2)?buf2:"",(buf2)?",":"",temp_contactgroup->group_name);
			my_free(buf2);
			buf2=buf1;
			}
		if(buf2){
			*output=(char *)strdup(buf2);
			my_free(buf2);
			}
		break;
#endif
	default:
#ifdef NSCORE
		log_debug_info(DEBUGL_MACROS,0,"UNHANDLED CONTACT MACRO #%d! THIS IS A BUG!\n",macro_type);
#endif
		return ERROR;
		break;
		}

	return OK;
	}



/* computes a contact address macro */
int grab_contact_address_macro(int macro_num, contact *temp_contact, char **output){

	if(macro_num<0 || macro_num>=MAX_CONTACT_ADDRESSES)
		return ERROR;

	if(temp_contact==NULL || output==NULL)
		return ERROR;

	/* get the macro */
	if(temp_contact->address[macro_num])
		*output=(char *)strdup(temp_contact->address[macro_num]);

	return OK;
	}



/* computes a contactgroup macro */
int grab_standard_contactgroup_macro(int macro_type, contactgroup *temp_contactgroup, char **output){
	contactsmember *temp_contactsmember=NULL;

	if(temp_contactgroup==NULL || output==NULL)
		return ERROR;

	/* get the macro value */
	switch(macro_type){
	case MACRO_CONTACTGROUPNAME:
		*output=(char *)strdup(temp_contactgroup->group_name);
		break;
	case MACRO_CONTACTGROUPALIAS:
		if(temp_contactgroup->alias)
			*output=(char *)strdup(temp_contactgroup->alias);
		break;
	case MACRO_CONTACTGROUPMEMBERS:
		/* get the member list */
		for(temp_contactsmember=temp_contactgroup->members;temp_contactsmember!=NULL;temp_contactsmember=temp_contactsmember->next){
			if(temp_contactsmember->contact_name==NULL)
				continue;
			if(*output==NULL)
				*output=(char *)strdup(temp_contactsmember->contact_name);
			else if((*output=(char *)realloc(*output,strlen(*output)+strlen(temp_contactsmember->contact_name)+2))){
				strcat(*output,",");
				strcat(*output,temp_contactsmember->contact_name);
				}
			}
		break;
	default:
#ifdef NSCORE
		log_debug_info(DEBUGL_MACROS,0,"UNHANDLED CONTACTGROUP MACRO #%d! THIS IS A BUG!\n",macro_type);
#endif
		return ERROR;
		break;
		}

	return OK;
	}



/* computes a custom object macro */
int grab_custom_object_macro(char *macro_name, customvariablesmember *vars, char **output){
	customvariablesmember *temp_customvariablesmember=NULL;
	int result=ERROR;

	if(macro_name==NULL || vars==NULL || output==NULL)
		return ERROR;

	/* get the custom variable */
	for(temp_customvariablesmember=vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){

		if(temp_customvariablesmember->variable_name==NULL)
			continue;

		if(!strcmp(macro_name,temp_customvariablesmember->variable_name)){
			if(temp_customvariablesmember->variable_value)
				*output=(char *)strdup(temp_customvariablesmember->variable_value);
			result=OK;
			break;
			}
	        }

	return result;
	}



/******************************************************************/
/********************* MACRO STRING FUNCTIONS *********************/
/******************************************************************/

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
			
			/*ch=(int)macro[x];*/
			/* allow non-ASCII characters (Japanese, etc) */
			ch=macro[x] & 0xff;

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
	char temp_expansion[6]="";


	/* bail if no input */
	if(input==NULL)
		return NULL;

	/* allocate enough memory to escape all characters if necessary */
	if((encoded_url_string=(char *)malloc((strlen(input)*3)+1))==NULL)
		return NULL;

	/* check/encode all characters */
	for(x=0,y=0;input[x]!=(char)'\x0';x++){

		/* alpha-numeric characters and a few other characters don't get encoded */
		if(((char)input[x]>='0' && (char)input[x]<='9') || ((char)input[x]>='A' && (char)input[x]<='Z') || ((char)input[x]>=(char)'a' && (char)input[x]<=(char)'z') || (char)input[x]==(char)'.' || (char)input[x]==(char)'-' || (char)input[x]==(char)'_' || (char)input[x]==(char)':' || (char)input[x]==(char)'/' || (char)input[x]==(char)'?' || (char)input[x]==(char)'=' || (char)input[x]==(char)'&'){
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
			sprintf(temp_expansion,"%%%02X",(unsigned int)(input[x] & 0xFF));
			strcat(encoded_url_string,temp_expansion);
			y+=3;
		        }
	        }

	/* terminate encoded string */
	encoded_url_string[y]='\x0';

	return encoded_url_string;
        }



/******************************************************************/
/***************** MACRO INITIALIZATION FUNCTIONS *****************/
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

	/* macro object pointers */
	macro_host_ptr=NULL;
	macro_hostgroup_ptr=NULL;
	macro_service_ptr=NULL;
	macro_servicegroup_ptr=NULL;
	macro_contact_ptr=NULL;
	macro_contactgroup_ptr=NULL;

	return OK;
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
	add_macrox_name(MACRO_HOSTPROBLEMID,"HOSTPROBLEMID");
	add_macrox_name(MACRO_LASTHOSTPROBLEMID,"LASTHOSTPROBLEMID");
	add_macrox_name(MACRO_SERVICEPROBLEMID,"SERVICEPROBLEMID");
	add_macrox_name(MACRO_LASTSERVICEPROBLEMID,"LASTSERVICEPROBLEMID");
	add_macrox_name(MACRO_ISVALIDTIME,"ISVALIDTIME");
	add_macrox_name(MACRO_NEXTVALIDTIME,"NEXTVALIDTIME");
	add_macrox_name(MACRO_LASTHOSTSTATE,"LASTHOSTSTATE");
	add_macrox_name(MACRO_LASTHOSTSTATEID,"LASTHOSTSTATEID");
	add_macrox_name(MACRO_LASTSERVICESTATE,"LASTSERVICESTATE");
	add_macrox_name(MACRO_LASTSERVICESTATEID,"LASTSERVICESTATEID");

	return OK;
        }


/* saves the name of a macro */
int add_macrox_name(int i, char *name){

	/* dup the macro name */
	macro_x_names[i]=(char *)strdup(name);

	return OK;
        }



/******************************************************************/
/********************* MACRO CLEANUP FUNCTIONS ********************/
/******************************************************************/

/* free memory associated with the macrox names */
int free_macrox_names(void){
	register int x=0;

	/* free each macro name */
	for(x=0;x<MACRO_X_COUNT;x++)
		my_free(macro_x_names[x]);

	return OK;
        }



/* clear argv macros - used in commands */
int clear_argv_macros(void){
	register int x=0;

	/* command argument macros */
	for(x=0;x<MAX_COMMAND_ARGUMENTS;x++)
		my_free(macro_argv[x]);

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
		case MACRO_EVENTSTARTTIME:
			/* these don't change during the course of monitoring, so no need to free them */
			break;
		default:
			my_free(macro_x[x]);
			break;
		        }
	        }

	/* contact address macros */
	for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
		my_free(macro_contactaddress[x]);

	/* clear macro pointers */
	macro_host_ptr=NULL;
	macro_hostgroup_ptr=NULL;
	macro_service_ptr=NULL;
	macro_servicegroup_ptr=NULL;
	macro_contact_ptr=NULL;
	macro_contactgroup_ptr=NULL;

	/* clear on-demand macro */
	my_free(macro_ondemand);

	/* clear ARGx macros */
	clear_argv_macros();

	/* clear custom host variables */
	for(this_customvariablesmember=macro_custom_host_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(this_customvariablesmember->variable_name);
		my_free(this_customvariablesmember->variable_value);
		my_free(this_customvariablesmember);
	        }
	macro_custom_host_vars=NULL;

	/* clear custom service variables */
	for(this_customvariablesmember=macro_custom_service_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(this_customvariablesmember->variable_name);
		my_free(this_customvariablesmember->variable_value);
		my_free(this_customvariablesmember);
	        }
	macro_custom_service_vars=NULL;

	/* clear custom contact variables */
	for(this_customvariablesmember=macro_custom_contact_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(this_customvariablesmember->variable_name);
		my_free(this_customvariablesmember->variable_value);
		my_free(this_customvariablesmember);
	        }
	macro_custom_contact_vars=NULL;

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
		case MACRO_SERVICEPROBLEMID:
		case MACRO_LASTSERVICEPROBLEMID:

			my_free(macro_x[x]);
			break;
		default:
			break;
			}
		}

	/* clear custom service variables */
	for(this_customvariablesmember=macro_custom_service_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(this_customvariablesmember->variable_name);
		my_free(this_customvariablesmember->variable_value);
		my_free(this_customvariablesmember);
	        }
	macro_custom_service_vars=NULL;

	/* clear pointers */
	macro_service_ptr=NULL;

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
		case MACRO_HOSTPROBLEMID:
		case MACRO_LASTHOSTPROBLEMID:

			my_free(macro_x[x]);
			break;
		default:
			break;
			}
		}

	/* clear custom host variables */
	for(this_customvariablesmember=macro_custom_host_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(this_customvariablesmember->variable_name);
		my_free(this_customvariablesmember->variable_value);
		my_free(this_customvariablesmember);
	        }
	macro_custom_host_vars=NULL;

	/* clear pointers */
	macro_host_ptr=NULL;

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
			my_free(macro_x[x]);
			break;
		default:
			break;
			}
		}

	/* clear pointers */
	macro_hostgroup_ptr=NULL;

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
			my_free(macro_x[x]);
			break;
		default:
			break;
			}
		}

	/* clear pointers */
	macro_servicegroup_ptr=NULL;

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
			my_free(macro_x[x]);
			break;
		default:
			break;
			}
		}

	/* clear contact addresses */
	for(x=0;x<MAX_CONTACT_ADDRESSES;x++)
		my_free(macro_contactaddress[x]);

	/* clear custom contact variables */
	for(this_customvariablesmember=macro_custom_contact_vars;this_customvariablesmember!=NULL;this_customvariablesmember=next_customvariablesmember){
		next_customvariablesmember=this_customvariablesmember->next;
		my_free(this_customvariablesmember->variable_name);
		my_free(this_customvariablesmember->variable_value);
		my_free(this_customvariablesmember);
	        }
	macro_custom_contact_vars=NULL;

	/* clear pointers */
	macro_contact_ptr=NULL;

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
			my_free(macro_x[x]);
			break;
		default:
			break;
			}
		}

	/* clear pointers */
	macro_contactgroup_ptr=NULL;

	return OK;
	}



/* clear summary macros */
int clear_summary_macros(void){
	register int x;

	for(x=MACRO_TOTALHOSTSUP;x<=MACRO_TOTALSERVICEPROBLEMSUNHANDLED;x++)
		my_free(macro_x[x]);

	return OK;
	}



/******************************************************************/
/****************** ENVIRONMENT MACRO FUNCTIONS *******************/
/******************************************************************/

#ifdef NSCORE

/* sets or unsets all macro environment variables */
int set_all_macro_environment_vars(int set){


	if(enable_environment_macros==FALSE)
		return ERROR;

	set_macrox_environment_vars(set);
	set_argv_macro_environment_vars(set);
	set_custom_macro_environment_vars(set);
	set_contact_address_environment_vars(set);

	return OK;
        }



/* sets or unsets macrox environment variables */
int set_macrox_environment_vars(int set){
	register int x=0;
	int free_macro=FALSE;
	int generate_macro=TRUE;

	/* set each of the macrox environment variables */
	for(x=0;x<MACRO_X_COUNT;x++){

		free_macro=FALSE;

		/* generate the macro value if it hasn't already been done */
		/* THIS IS EXPENSIVE */
		if(set==TRUE){

			generate_macro=TRUE;

			/* skip summary macro generation if lage installation tweaks are enabled */
			if((x>=MACRO_TOTALHOSTSUP && x<=MACRO_TOTALSERVICEPROBLEMSUNHANDLED) && use_large_installation_tweaks==TRUE)
				generate_macro=FALSE;

			if(macro_x[x]==NULL && generate_macro==TRUE)
				grab_macrox_value(x,NULL,NULL,&macro_x[x],&free_macro);
			}

		/* set the value */
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
		my_free(macro_name);
	        }

	return OK;
        }



/* sets or unsets custom host/service/contact macro environment variables */
int set_custom_macro_environment_vars(int set){
	customvariablesmember *temp_customvariablesmember=NULL;
	host *temp_host=NULL;
	service *temp_service=NULL;
	contact *temp_contact=NULL;
	char *customvarname=NULL;

	/***** CUSTOM HOST VARIABLES *****/
	/* generate variables and save them for later */
	if((temp_host=macro_host_ptr) && set==TRUE){
		for(temp_customvariablesmember=temp_host->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
			asprintf(&customvarname,"_HOST%s",temp_customvariablesmember->variable_name);
			add_custom_variable_to_object(&macro_custom_host_vars,customvarname,temp_customvariablesmember->variable_value);
			my_free(customvarname);
			}
		}
	/* set variables */
	for(temp_customvariablesmember=macro_custom_host_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
		set_macro_environment_var(temp_customvariablesmember->variable_name,clean_macro_chars(temp_customvariablesmember->variable_value,STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS),set);
		}

	/***** CUSTOM SERVICE VARIABLES *****/
	/* generate variables and save them for later */
	if((temp_service=macro_service_ptr) && set==TRUE){
		for(temp_customvariablesmember=temp_service->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
			asprintf(&customvarname,"_SERVICE%s",temp_customvariablesmember->variable_name);
			add_custom_variable_to_object(&macro_custom_service_vars,customvarname,temp_customvariablesmember->variable_value);
			my_free(customvarname);
			}
		}
	/* set variables */
	for(temp_customvariablesmember=macro_custom_service_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next)
		set_macro_environment_var(temp_customvariablesmember->variable_name,clean_macro_chars(temp_customvariablesmember->variable_value,STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS),set);

	/***** CUSTOM CONTACT VARIABLES *****/
	/* generate variables and save them for later */
	if((temp_contact=macro_contact_ptr) && set==TRUE){
		for(temp_customvariablesmember=temp_contact->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
			asprintf(&customvarname,"_CONTACT%s",temp_customvariablesmember->variable_name);
			add_custom_variable_to_object(&macro_custom_contact_vars,customvarname,temp_customvariablesmember->variable_value);
			my_free(customvarname);
			}
		}
	/* set variables */
	for(temp_customvariablesmember=macro_custom_contact_vars;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next)
		set_macro_environment_var(temp_customvariablesmember->variable_name,clean_macro_chars(temp_customvariablesmember->variable_value,STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS),set);

	return OK;
        }



/* sets or unsets contact address environment variables */
int set_contact_address_environment_vars(int set){
	char *varname=NULL;
	register int x;

	/* these only get set during notifications */
	if(macro_contact_ptr==NULL)
		return OK;

	for(x=0;x<MAX_CONTACT_ADDRESSES;x++){
		asprintf(&varname,"CONTACTADDRESS%d",x);
		set_macro_environment_var(varname,macro_contact_ptr->address[x],set);
		my_free(varname);
		}

	return OK;
        }



/* sets or unsets a macro environment variable */
int set_macro_environment_var(char *name, char *value, int set){
	char *env_macro_name=NULL;

	/* we won't mess with null variable names */
	if(name==NULL)
		return ERROR;

	/* create environment var name */
	asprintf(&env_macro_name,"%s%s",MACRO_ENV_VAR_PREFIX,name);

	/* set or unset the environment variable */
	set_environment_var(env_macro_name,value,set);

	/* free allocated memory */
	my_free(env_macro_name);

	return OK;
        }

#endif


