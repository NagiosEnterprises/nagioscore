/*****************************************************************************
 *
 * NOTIFICATIONS.C - Service and host notification functions for Nagios
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-10-2002
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

#include "../common/config.h"
#include "../common/common.h"
#include "../common/objects.h"
#include "../common/statusdata.h"
#include "nagios.h"

extern notification    *notification_list;
extern contact         *contact_list;
extern serviceescalation *serviceescalation_list;
extern hostescalation  *hostescalation_list;

extern int             interval_length;
extern int             log_notifications;

extern int             enable_notifications;

extern int             notification_timeout;

extern char            *macro_x[MACRO_X_COUNT];

extern char            *generic_summary;



/******************************************************************/
/***************** SERVICE NOTIFICATION FUNCTIONS *****************/
/******************************************************************/


/* notify contacts about a service problem or recovery */
int service_notification(service *svc, char *ack_data){
	host *temp_host;
	notification *temp_notification;
	time_t current_time;

#ifdef DEBUG0
	printf("service_notification() start\n");
#endif

	/* get the current time */
	time(&current_time);

#ifdef DEBUG4
	printf("\nSERVICE NOTIFICATION ATTEMPT: Service '%s' on host '%s'\n",svc->description,svc->host_name);
	printf("\tCurrent time: %s",ctime(&current_time));
#endif

	/* find the host this service is associated with */
	temp_host=find_host(svc->host_name);

	/* if we couldn't find the host, return an error */
	if(temp_host==NULL){
#ifdef DEBUG4
		printf("\tCouldn't find the host associated with this service, so we won't send a notification!\n");
#endif
		return ERROR;
	        }

	/* check the viability of sending out a service notification */
	if(check_service_notification_viability(svc,ack_data)==ERROR){
#ifdef DEBUG4
		printf("\tSending out a notification for this service is not viable at this time.\n");
#endif
		return OK;
	        }

	/* if this is just a normal notification and NOT a problem acknowledgement... */
	if(ack_data==NULL){

		/* increase the current notification number */
		svc->current_notification_number++;

#ifdef DEBUG4
		printf("\tCurrent notification number: %d\n",svc->current_notification_number);
#endif

		/* calculate the next acceptable re-notification time */
		svc->next_notification=get_next_service_notification_time(svc,current_time);

#ifdef DEBUG4
		printf("\tCurrent Time: %s",ctime(&current_time));
		printf("\tNext acceptable notification time: %s",ctime(&svc->next_notification));
#endif
	        }

	/* grab the macro variables */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);

	/* if this is an aknowledgement, modify the $OUTPUT$ macro to have it contain the comment that was entered by the user */
	if(ack_data!=NULL){
		if(macro_x[MACRO_OUTPUT]!=NULL)
			free(macro_x[MACRO_OUTPUT]);
		macro_x[MACRO_OUTPUT]=strdup(ack_data);
	        }

	/* set the notification type macro */
	if(macro_x[MACRO_NOTIFICATIONTYPE]!=NULL)
		free(macro_x[MACRO_NOTIFICATIONTYPE]);
	macro_x[MACRO_NOTIFICATIONTYPE]=(char *)malloc(MAX_NOTIFICATIONTYPE_LENGTH);
	if(macro_x[MACRO_NOTIFICATIONTYPE]!=NULL){
		if(ack_data!=NULL)
			strcpy(macro_x[MACRO_NOTIFICATIONTYPE],"ACKNOWLEDGEMENT");
		else if(svc->current_state==STATE_OK)
			strcpy(macro_x[MACRO_NOTIFICATIONTYPE],"RECOVERY");
		else
			strcpy(macro_x[MACRO_NOTIFICATIONTYPE],"PROBLEM");
	        }

	/* set the notification number macro */
	if(macro_x[MACRO_NOTIFICATIONNUMBER]!=NULL)
		free(macro_x[MACRO_NOTIFICATIONNUMBER]);
	macro_x[MACRO_NOTIFICATIONNUMBER]=(char *)malloc(MAX_NOTIFICATIONNUMBER_LENGTH);
	if(macro_x[MACRO_NOTIFICATIONNUMBER]!=NULL){
		snprintf(macro_x[MACRO_NOTIFICATIONNUMBER],MAX_NOTIFICATIONNUMBER_LENGTH-1,"%d",svc->current_notification_number);
		macro_x[MACRO_NOTIFICATIONNUMBER][MAX_NOTIFICATIONNUMBER_LENGTH-1]='\x0';
	        }

	/* create the contact notification list for this service */
	create_notification_list_from_service(svc);

	/* notify each contact (duplicates have been removed) */
	for(temp_notification=notification_list;temp_notification!=NULL;temp_notification=temp_notification->next){

		/* grab the macro variables for this contact */
		grab_contact_macros(temp_notification->contact);

		/* notify this contact */
		notify_contact_of_service(temp_notification->contact,svc,ack_data);
	        }

	/* there were no contacts, so no notification really occurred... */
	if(notification_list==NULL){
		if(ack_data==NULL)
			svc->current_notification_number--;

#ifdef DEBUG4
		printf("\tTHERE WERE NO CONTACTS TO BE NOTIFIED!\n");
#endif
	        }

	/* else there were contacts who got notified... */
	else{

		/* update the last notification time for this service if this was NOT an acknowledgement (this is needed for rescheduling later notifications) */
		if(ack_data==NULL)
			svc->last_notification=current_time;

#ifdef DEBUG4
		printf("\tAPPROPRIATE CONTACTS HAVE BEEN NOTIFIED\n");
#endif
	        }

	/* free memory allocated to the notification list */
	free_notification_list();

	/* update the status log with the service information */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("service_notification() end\n");
#endif

	return OK;
	}



/* checks the viability of sending out a service alert (top level filters) */
int check_service_notification_viability(service *svc, char *ack_data){
	host *temp_host;
	time_t current_time;
	time_t timeperiod_start;
	
#ifdef DEBUG0
	printf("check_service_notification_viability() start\n");
#endif

	/* get current time */
	time(&current_time);

	/* are notifications enabled? */
	if(enable_notifications==FALSE){
#ifdef DEBUG4
		printf("\tNotifications are disabled, so service notifications (problems and acknowledments) will not be sent out!\n");
#endif
		return ERROR;
	        }

	/* see if the service can have notifications sent out at this time */
	if(check_time_against_period(current_time,svc->notification_period)==ERROR){
#ifdef DEBUG4
		printf("\tThis service shouldn't have notifications sent out at this time!\n");
#endif

		/* calculate the next acceptable notification time, once the next valid time range arrives... */
		if(ack_data==NULL){

			get_next_valid_time(current_time,&timeperiod_start,svc->notification_period);

			/* looks like there are no valid notification times defined, so schedule the next one far into the future (one year)... */
			if(timeperiod_start==(time_t)0)
				svc->next_notification=(time_t)(current_time+(60*60*24*365));

			/* else use the next valid notification time */
			else
				svc->next_notification=timeperiod_start;
		        }

		return ERROR;
	        }

	/* are notifications temporarily disabled for this service? */
	if(svc->notifications_enabled==FALSE){
#ifdef DEBUG4
		printf("\tNotifications are temporarily disabled for this service, so we won't send one out!\n");
#endif
		return ERROR;
	        }


	/****************************************/
	/*** SPECIAL CASE FOR ACKNOWLEGEMENTS ***/
	/****************************************/

	/* acknowledgements only have to pass three general filters, although they have another test of their own... */
	if(ack_data!=NULL){

		/* don't send an acknowledgement if there isn't a problem... */
		if(svc->current_state==STATE_OK){
#ifdef DEBUG4
			printf("\tThe service is currently OK, so we won't send an acknowledgement!\n");
#endif
			return ERROR;
	                }

		/* acknowledgement viability test passed, so the notification can be sent out */
		return OK;
	        }


	/****************************************/
	/*** NORMAL NOTIFICATIONS ***************/
	/****************************************/

	/* has this problem already been acknowledged? */
	if(svc->problem_has_been_acknowledged==TRUE){
#ifdef DEBUG4
		printf("\tThis service problem has already been acknowledged, so we won't send a notification out!\n");
#endif
		return ERROR;
	        }

	/* do not send a recovery notification if we shouldn't (this flag was set by the check logic) */
	if(svc->current_state==STATE_OK && svc->no_recovery_notification==TRUE){
#ifdef DEBUG4
		printf("\tRecovery notifications should not be sent out for this service!\n");
#endif
		return ERROR;
	        }

	/* don't send notifications if the host was down or unreachable at last service check */
	if(svc->host_problem_at_last_check==TRUE){
#ifdef DEBUG4
		printf("\tThe host associated with this service had problems at last check, so notifications won't be sent out!\n");
#endif
		return ERROR;
	        }

	/* don't send notifications if the there were failed notification dependencies at the last check */
	if(svc->dependency_failure_at_last_check==TRUE){
#ifdef DEBUG4
		printf("\tNotification dependencies failed for this service, so notifications won't be sent out!\n");
#endif
		return ERROR;
	        }

	/* find the host this service is associated with */
	temp_host=find_host(svc->host_name);

	/* if we couldn't find the host, return an error */
	if(temp_host==NULL){
#ifdef DEBUG4
		printf("\tCouldn't find the host associated with this service, so we won't send a notification!\n");
#endif
		return ERROR;
	        }

	/* if the host is down or unreachable, don't notify contacts about service failures */
	if(temp_host->status!=HOST_UP){
#ifdef DEBUG4
		printf("\tThe host is either down or unreachable, so we won't notify contacts about this service!\n");
#endif
		return ERROR;
	        }
	
	/* see if we should notify about problems with this service */
	if(svc->current_state==STATE_UNKNOWN && svc->notify_on_unknown==FALSE){
#ifdef DEBUG4
		printf("\tWe shouldn't notify about UNKNOWN states for this service!\n");
#endif
		return ERROR;
	        }
	if(svc->current_state==STATE_WARNING && svc->notify_on_warning==FALSE){
#ifdef DEBUG4
		printf("\tWe shouldn't notify about WARNING states for this service!\n");
#endif
		return ERROR;
	        }
	if(svc->current_state==STATE_CRITICAL && svc->notify_on_critical==FALSE){
#ifdef DEBUG4
		printf("\tWe shouldn't notify about CRITICAL states for this service!\n");
#endif
		return ERROR;
	        }
	if(svc->current_state==STATE_OK){
		if(svc->notify_on_recovery==FALSE){
#ifdef DEBUG4
			printf("\tWe shouldn't notify about RECOVERY states for this service!\n");
#endif
			return ERROR;
		        }
		if(!((svc->has_been_unknown==TRUE && svc->notify_on_unknown==TRUE) || (svc->has_been_warning==TRUE && svc->notify_on_warning==TRUE) || (svc->has_been_critical==TRUE && svc->notify_on_critical==TRUE))){
#ifdef DEBUG4
			printf("\tWe shouldn't notify about this recovery\n");
#endif
			return ERROR;
		        }
	        }


	/* dont notify contacts about this service problem again if the notification interval is set to 0 */
	if(svc->current_state!=STATE_OK && svc->no_more_notifications==TRUE){
#ifdef DEBUG4
		printf("\tWe shouldn't re-notify contacts about this service problem!\n");
#endif
		return ERROR;
	        }

	/* don't notify if we haven't waited long enough since the last time (and the service is not marked as being volatile) */
	if((current_time < svc->next_notification) && svc->is_volatile==FALSE){
#ifdef DEBUG4
		printf("\tWe haven't waited long enough to re-notify contacts about this service!\n");
		printf("\tNext valid notification time: %s",ctime(&svc->next_notification));
#endif
		return ERROR;
	        }

	/* if this service is currently in a scheduled downtime period, don't send the notification */
	if(svc->scheduled_downtime_depth>0){
#ifdef DEBUG4
		printf("\tThis service is currently in a scheduled downtime, so we won't send notifications!\n");
#endif
		return ERROR;
	        }

	/* if this host is currently in a scheduled downtime period, don't send the notification */
	if(temp_host->scheduled_downtime_depth>0){
#ifdef DEBUG4
		printf("\tThe host this service is associated with is currently in a scheduled downtime, so we won't send notifications!\n");
#endif
		return ERROR;
	        }

	/* if this service is currently flapping, don't send the notification */
	if(svc->is_flapping==TRUE){
#ifdef DEBUG4
		printf("\tThis service is currently flapping, so we won't send notifications!\n");
#endif
		return ERROR;
	        }

#ifdef DEBUG0
	printf("check_service_notification_viability() end\n");
#endif

	return OK;
        }



/* check viability of sending out a service notification to a specific contact (contact-specific filters) */
int check_contact_service_notification_viability(contact *cntct,service *svc){

#ifdef DEBUG0
	printf("check_contact_service_notification_viability() start\n");
#endif

	/* see if we should notify about problems with this service */
	if(svc->current_state==STATE_UNKNOWN && cntct->notify_on_service_unknown==FALSE){
#ifdef DEBUG4
		printf("\tWe shouldn't notify this contact about UNKNOWN service states!\n");
#endif
		return ERROR;
	        }

	if(svc->current_state==STATE_WARNING && cntct->notify_on_service_warning==FALSE){
#ifdef DEBUG4
		printf("\tWe shouldn't notify this contact about WARNING service states!\n");
#endif
		return ERROR;
	        }

	if(svc->current_state==STATE_CRITICAL && cntct->notify_on_service_critical==FALSE){
#ifdef DEBUG4
		printf("\tWe shouldn't notify this contact about CRITICAL service states!\n");
#endif
		return ERROR;
	        }

	if(svc->current_state==STATE_OK){

		if(cntct->notify_on_service_recovery==FALSE){
#ifdef DEBUG4
			printf("\tWe shouldn't notify this contact about RECOVERY service states!\n");
#endif
			return ERROR;
		        }

		if(!((svc->has_been_unknown==TRUE && cntct->notify_on_service_unknown==TRUE) || (svc->has_been_warning==TRUE && cntct->notify_on_service_warning==TRUE) || (svc->has_been_critical==TRUE && cntct->notify_on_service_critical==TRUE))){
#ifdef DEBUG4
			printf("\tWe shouldn't notify about this recovery\n");
#endif
			return ERROR;
		        }

	        }

#ifdef DEBUG0
	printf("check_contact_service_notification_viability() end\n");
#endif

	return OK;
        }


/* notify a specific contact about a service problem or recovery */
int notify_contact_of_service(contact *cntct,service *svc,char *ack_data){
	commandsmember *temp_commandsmember;
	char temp_command_line[MAX_INPUT_BUFFER];
	char raw_command_line[MAX_INPUT_BUFFER];
	char command_line[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	int early_timeout=FALSE;

#ifdef DEBUG0
	printf("notify_contact_of_service() start\n");
#endif
#ifdef DEBUG4
	printf("\tNotify user %s\n",cntct->name);
#endif

	/* see if the contact can be notified at this time */
	if(check_time_against_period(time(NULL),cntct->service_notification_period)==ERROR){
#ifdef DEBUG4
		printf("\tThis contact shouldn't be notified at this time!\n");
#endif
		return OK;
	        }

	/* check viability of notifying this user */
	/* acknowledgements are no longer excluded from this test - added 8/19/02 Tom Bertelson */
	if(check_contact_service_notification_viability(cntct,svc)==ERROR)
		return OK;

	/* process all the notification commands this user has */
	for(temp_commandsmember=cntct->service_notification_commands;temp_commandsmember!=NULL;temp_commandsmember=temp_commandsmember->next){

		/* get the raw command line */
		strncpy(temp_command_line,temp_commandsmember->command,sizeof(temp_command_line)-1);
		temp_command_line[sizeof(temp_command_line)-1]='\x0';
		get_raw_command_line(temp_command_line,&raw_command_line[0],(int)sizeof(raw_command_line));

		/* run the notification command */
		if(strcmp(raw_command_line,"")){

#ifdef DEBUG4
			printf("\tRaw Command:       %s\n",raw_command_line);
#endif

			/* replace macros in the command line */
			process_macros(raw_command_line,&command_line[0],(int)sizeof(command_line),STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS);

#ifdef DEBUG4
			printf("\tProcessed Command: %s\n",command_line);
#endif

			/* log the notification to program log file */
			if(log_notifications==TRUE){
				if(ack_data==NULL)
					snprintf(temp_buffer,sizeof(temp_buffer),"SERVICE NOTIFICATION: %s;%s;%s;%s;%s;%s\n",cntct->name,svc->host_name,svc->description,macro_x[MACRO_SERVICESTATE],temp_command_line,macro_x[MACRO_OUTPUT]);
				else
					snprintf(temp_buffer,sizeof(temp_buffer),"SERVICE NOTIFICATION: %s;%s;%s;ACKNOWLEDGEMENT (%s);%s;%s\n",cntct->name,svc->host_name,svc->description,macro_x[MACRO_SERVICESTATE],temp_command_line,macro_x[MACRO_OUTPUT]);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_SERVICE_NOTIFICATION,FALSE);
			        }

			/* run the command */
			my_system(command_line,notification_timeout,&early_timeout,NULL,0);

			/* check to see if the notification command timed out */
			if(early_timeout==TRUE){
				snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Contact '%s' service notification command '%s' timed out after %d seconds\n",cntct->name,temp_commandsmember->command,DEFAULT_NOTIFICATION_TIMEOUT);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_SERVICE_NOTIFICATION | NSLOG_RUNTIME_WARNING,TRUE);
			        }
		        }
	        }

#ifdef DEBUG0
	printf("notify_contact_of_service() end\n");
#endif

	return OK;
        }


/* checks to see if a service escalation entry is a match for the current service notification */
int is_valid_escalation_for_service_notification(service *svc,serviceescalation *se){
	int notification_number;

#ifdef DEBUG0
	printf("is_valid_escalation_for_service_notification() start\n");
#endif

	/* if this is a recovery, really we check for who got notified about a previous problem */
	if(svc->current_state==STATE_OK)
		notification_number=svc->current_notification_number-1;
	else
		notification_number=svc->current_notification_number;

	/* this entry if it is not for this service */
	if(strcmp(svc->host_name,se->host_name) || strcmp(svc->description,se->description))
		return FALSE;

	/* skip this escalation if it happens later */
	if(se->first_notification > notification_number)
		return FALSE;

	/* skip this escalation if it has already passed */
	if(se->last_notification!=0 && se->last_notification < notification_number)
		return FALSE;

#ifdef DEBUG0
	printf("is_valid_escalation_for_service_notification() end\n");
#endif

	return TRUE;
        }


/* checks to see whether a service notification should be escalation */
int should_service_notification_be_escalated(service *svc){
	serviceescalation *temp_se;

#ifdef DEBUG0
	printf("should_service_notification_be_escalated() start\n");
#endif

	/* search the service escalation list */
	for(temp_se=serviceescalation_list;temp_se!=NULL;temp_se=temp_se->next){

		/* we found a matching entry, so escalate this notification! */
		if(is_valid_escalation_for_service_notification(svc,temp_se)==TRUE){
#ifdef DEBUG4
			printf("\tService notification WILL BE escalated\n");
#endif
			return TRUE;
		        }
	        }

#ifdef DEBUG4
	printf("\tService notification will NOT be escalated\n");
#endif

#ifdef DEBUG0
	printf("should_service_notification_be_escalated() end\n");
#endif

	return FALSE;
        }


/* given a service, create a list of contacts to be notified, removing duplicates */
int create_notification_list_from_service(service *svc){
	serviceescalation *temp_se;
	contactgroupsmember *temp_group;
	contactgroup *temp_contactgroup;
	contact *temp_contact;

#ifdef DEBUG0
	printf("create_notification_list_from_service() end\n");
#endif

	/* should this notification be escalated? */
	if(should_service_notification_be_escalated(svc)==TRUE){

		/* search all the escalation entries for valid matches */
		for(temp_se=serviceescalation_list;temp_se!=NULL;temp_se=temp_se->next){

			/* skip this entry if it isn't appropriate */
			if(is_valid_escalation_for_service_notification(svc,temp_se)==FALSE)
				continue;

			/* find each contact group in this escalation entry */
			for(temp_group=temp_se->contact_groups;temp_group!=NULL;temp_group=temp_group->next){

				temp_contactgroup=find_contactgroup(temp_group->group_name,NULL);
				if(temp_contactgroup==NULL)
					continue;

				/* check all contacts */
				for(temp_contact=contact_list;temp_contact!=NULL;temp_contact=temp_contact->next){
					
					if(is_contact_member_of_contactgroup(temp_contactgroup,temp_contact)==TRUE)
						add_notification(temp_contact);
				        }
			        }
		        }
	        }

	/* no escalation is necessary - just continue normally... */
	else{

		/* find all contacts for this service */
		for(temp_contact=contact_list;temp_contact!=NULL;temp_contact=temp_contact->next){
		
			if(is_contact_for_service(svc,temp_contact)==TRUE)
				add_notification(temp_contact);
	                }
	        }

#ifdef DEBUG0
	printf("create_notification_list_from_service() end\n");
#endif

	return OK;
        }




/******************************************************************/
/******************* HOST NOTIFICATION FUNCTIONS ******************/
/******************************************************************/



/* notify all contacts for a host that the entire host is down or up */
int host_notification(host *hst,int state, char *ack_data){
	notification *temp_notification;
	time_t current_time;


#ifdef DEBUG0
	printf("host_notification() start\n");
#endif

	/* get the current time */
	time(&current_time);

#ifdef DEBUG4
	printf("\nHOST NOTIFICATION ATTEMPT: Host '%s'\n",hst->name);
	printf("\tCurrent time: %s",ctime(&current_time));
#endif

	/* check viability of sending out a host notification */
	if(check_host_notification_viability(hst,state,ack_data)==ERROR){
#ifdef DEBUG4
		printf("\tSending out a notification for this host is not viable at this time.\n");
#endif
		return OK;
	        }

	/* if this is just a normal notification and NOT an acknowledgement... */
	if(ack_data==NULL){

		/* increment the current notification number */
		hst->current_notification_number++;

#ifdef DEBUG4
		printf("\tCurrent notification number: %d\n",hst->current_notification_number);
#endif

		/* calculate the next acceptable re-notification time */
		hst->next_host_notification=get_next_host_notification_time(hst,state,current_time);

#ifdef DEBUG4
		printf("\tCurrent Time: %s",ctime(&current_time));
		printf("\tNext acceptable notification time: %s",ctime(&hst->next_host_notification));
#endif
	        }

	/* grab the macro variables */
	clear_volatile_macros();
	grab_host_macros(hst);

	/* make sure the host state macro is correct */
	if(macro_x[MACRO_HOSTSTATE]!=NULL)
		free(macro_x[MACRO_HOSTSTATE]);
	macro_x[MACRO_HOSTSTATE]=(char *)malloc(MAX_STATE_LENGTH);
	if(macro_x[MACRO_HOSTSTATE]!=NULL){
		if(state==HOST_DOWN)
			strcpy(macro_x[MACRO_HOSTSTATE],"DOWN");
		else if(state==HOST_UNREACHABLE)
			strcpy(macro_x[MACRO_HOSTSTATE],"UNREACHABLE");
		else
			strcpy(macro_x[MACRO_HOSTSTATE],"UP");
	        }

	/* if this is an aknowledgement, modify the $OUTPUT$ macro to contain the comment that the user entered */
	if(ack_data!=NULL){
		if(macro_x[MACRO_OUTPUT]!=NULL)
			free(macro_x[MACRO_OUTPUT]);
		macro_x[MACRO_OUTPUT]=strdup(ack_data);
	        }

	/* set the notification type macro */
	if(macro_x[MACRO_NOTIFICATIONTYPE]!=NULL)
		free(macro_x[MACRO_NOTIFICATIONTYPE]);
	macro_x[MACRO_NOTIFICATIONTYPE]=(char *)malloc(MAX_NOTIFICATIONTYPE_LENGTH);
	if(macro_x[MACRO_NOTIFICATIONTYPE]!=NULL){
		if(ack_data!=NULL)
			strcpy(macro_x[MACRO_NOTIFICATIONTYPE],"ACKNOWLEDGEMENT");
		else if(state==HOST_UP)
			strcpy(macro_x[MACRO_NOTIFICATIONTYPE],"RECOVERY");
		else
			strcpy(macro_x[MACRO_NOTIFICATIONTYPE],"PROBLEM");
	        }

	/* set the notification number macro */
	if(macro_x[MACRO_NOTIFICATIONNUMBER]!=NULL)
		free(macro_x[MACRO_NOTIFICATIONNUMBER]);
	macro_x[MACRO_NOTIFICATIONNUMBER]=(char *)malloc(MAX_NOTIFICATIONNUMBER_LENGTH);
	if(macro_x[MACRO_NOTIFICATIONNUMBER]!=NULL){
		snprintf(macro_x[MACRO_NOTIFICATIONNUMBER],MAX_NOTIFICATIONNUMBER_LENGTH-1,"%d",hst->current_notification_number);
		macro_x[MACRO_NOTIFICATIONNUMBER][MAX_NOTIFICATIONNUMBER_LENGTH-1]='\x0';
	        }

	/* create the contact notification list for this host */
	create_notification_list_from_host(hst,state);

	/* notify each contact (duplicates have been removed) */
	for(temp_notification=notification_list;temp_notification!=NULL;temp_notification=temp_notification->next){

		/* grab the macro variables for this contact */
		grab_contact_macros(temp_notification->contact);

		/* notify this contact */
		notify_contact_of_host(temp_notification->contact,hst,state,ack_data);
	        }

	/* there were no contacts, so no notification really occurred... */
	if(notification_list==NULL){
		if(ack_data==NULL)
			hst->current_notification_number--;

#ifdef DEBUG4
		printf("\tTHERE WERE NO CONTACTS TO BE NOTIFIED!\n");
#endif
	        }

	/* else there were contacts who got notified... */
	else{

		/* update the last notification time for this host if this was NOT an acknowledgement (this is needed for scheduling the next problem notification) */
		if(ack_data==NULL)
			hst->last_host_notification=current_time;

#ifdef DEBUG4
		printf("\tAPPROPRIATE CONTACTS HAVE BEEN NOTIFIED\n");
#endif
	        }

	/* free memory allocated to the notification list */
	free_notification_list();

	/* update the status log with the host info */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("host_notification() end\n");
#endif

	return OK;
        }



/* checks viability of sending a host notification */
int check_host_notification_viability(host *hst,int state,char *ack_data){
	int state_change=FALSE;
	time_t current_time;
	time_t timeperiod_start;

#ifdef DEBUG0
	printf("check_host_notification_viability() start\n");
#endif

	/* get current time */
	time(&current_time);

	/* are notifications enabled? */
	if(enable_notifications==FALSE){
#ifdef DEBUG4
		printf("\tNotifications are disabled, so host notifications (problems or acknowledgements) will not be sent out!\n");
#endif
		return ERROR;
	        }

	/* see if the host can have notifications sent out at this time */
	if(check_time_against_period(current_time,hst->notification_period)==ERROR){
#ifdef DEBUG4
		printf("\tThis host shouldn't have notifications sent out at this time!\n");
#endif

		/* if this is NOT an acknowledgement, calculate the next acceptable notification time, once the next valid time range arrives... */
		if(ack_data==NULL){

			get_next_valid_time(current_time,&timeperiod_start,hst->notification_period);

			/* it looks like there is no notification time defined, so schedule next one far into the future (one year)... */
			if(timeperiod_start==(time_t)0)
				hst->next_host_notification=(time_t)(current_time+(60*60*24*365));

			/* else use the next valid notification time */
			else
				hst->next_host_notification=timeperiod_start;
		        }

		return ERROR;
	        }

	/* are notifications temporarily disabled for this host? */
	if(hst->notifications_enabled==FALSE){
#ifdef DEBUG4
		printf("\tNotifications are temporarily disabled for this host, so we won't send one out!\n");
#endif
		return ERROR;
	        }


	/****************************************/
	/*** SPECIAL CASE FOR ACKNOWLEGEMENTS ***/
	/****************************************/

	/* acknowledgements only have to pass three general filters, although they have another test of their own... */
	if(ack_data!=NULL){

		/* don't send an acknowledgement if there isn't a problem... */
		if(state==HOST_UP){
#ifdef DEBUG4
			printf("\tThe host is currently UP, so we won't send an acknowledgement!\n");
#endif
			return ERROR;
	                }

		/* acknowledgement viability test passed, so the notification can be sent out */
		return OK;
	        }


	/****************************************/
	/*** NORMAL NOTIFICATIONS ***************/
	/****************************************/

	/* has this problem already been acknowledged? */
	if(hst->problem_has_been_acknowledged==TRUE){
#ifdef DEBUG4
		printf("\tThis host problem has already been acknowledged, so we won't send a notification out!\n");
#endif
		return ERROR;
	        }

	/* check notification dependencies */
	if(check_host_dependencies(hst,NOTIFICATION_DEPENDENCY)==DEPENDENCIES_FAILED){
#ifdef DEBUG4
		printf("\tNotification dependencies for this host have failed, so we won't sent a notification out!\n");
#endif
		return ERROR;
	        }

	/* has the host state changed? */
	if(hst->status!=state)
		state_change=TRUE;
	else
		state_change=FALSE;

	/* see if we should notify about problems with this host */
	if((state==HOST_UNREACHABLE) && hst->notify_on_unreachable==FALSE){
#ifdef DEBUG4
		printf("\tWe shouldn't notify about UNREACHABLE status for this host!\n");
#endif
		return ERROR;
	        }
	if(state==HOST_DOWN && hst->notify_on_down==FALSE){
#ifdef DEBUG4
		printf("\tWe shouldn't notify about DOWN states for this host!\n");
#endif
		return ERROR;
	        }
	if(state==HOST_UP){

		if(hst->notify_on_recovery==FALSE){
#ifdef DEBUG4
			printf("\tWe shouldn't notify about RECOVERY states for this host!\n");
#endif
			return ERROR;
		       }
		if(!((hst->has_been_down==TRUE && hst->notify_on_down==TRUE) || (hst->has_been_unreachable==TRUE && hst->notify_on_unreachable==TRUE))){
#ifdef DEBUG4
			printf("\tWe shouldn't notify about this recovery\n");
#endif
			return ERROR;
		        }

	        }

	/* if this host is currently in a scheduled downtime period, don't send the notification */
	if(hst->scheduled_downtime_depth>0){
#ifdef DEBUG4
		printf("\tThis host is currently in a scheduled downtime, so we won't send notifications!\n");
#endif
		return ERROR;
	        }

	/* if this host is currently flapping, don't send the notification */
	if(hst->is_flapping==TRUE){
#ifdef DEBUG4
		printf("\tThis host is currently flapping, so we won't send notifications!\n");
#endif
		return ERROR;
	        }

	/* if we haven't notified the contacts about the host state change, do so */
	if(state_change==TRUE){
#ifdef DEBUG4
		printf("\tHOST STATE CHANGE!\n");
#endif
	        }

	/* check if we shouldn't renotify contacts about the host problem */
	else if(state!=HOST_UP && hst->no_more_notifications==TRUE){

#ifdef DEBUG4
		printf("\tWe shouldn't re-notify contacts about this host problem!!\n");
#endif
		return ERROR;
	        }

	/* else if its time to re-notify the contacts about the host... */
	else if(state!=HOST_UP && current_time >= hst->next_host_notification){

#ifdef DEBUG4
		printf("\tTime to re-notify the contacts about the host problem!\n");
#endif
                }

	/* else we shouldn't do the notification, so exit */
	else{

#ifdef DEBUG4
		printf("\tIts not yet time to re-notify the contacts about this host problem...\n");
		printf("\tNext acceptable notification time: %s",ctime(&hst->next_host_notification));
#endif

		return ERROR;
	        }


#ifdef DEBUG0
	printf("check_host_notification_viability() end\n");
#endif

	return OK;
        }



/* checks the viability of notifying a specific contact about a host */
int check_contact_host_notification_viability(contact *cntct, host *hst, int state){

#ifdef DEBUG0
	printf("check_contact_host_notification_viability() start\n");
#endif

	/* see if we should notify about problems with this host */
	if((state==HOST_DOWN) && cntct->notify_on_host_down==FALSE){
#ifdef DEBUG4
		printf("\tWe shouldn't notify this contact about DOWN states!\n");
#endif
		return ERROR;
	        }

	if(state==HOST_UNREACHABLE && cntct->notify_on_host_unreachable==FALSE){
#ifdef DEBUG4
		printf("\tWe shouldn't notify this contact about UNREACHABLE states!\n");
#endif
		return ERROR;
	        }

	if(state==HOST_UP){

		if(cntct->notify_on_host_recovery==FALSE){
#ifdef DEBUG4
			printf("\tWe shouldn't notify this contact about RECOVERY states!\n");
#endif
			return ERROR;
		        }

		if(!((hst->has_been_down==TRUE && cntct->notify_on_host_down==TRUE) || (hst->has_been_unreachable==TRUE && cntct->notify_on_host_unreachable==TRUE))){
#ifdef DEBUG4
			printf("\tWe shouldn't notify about this recovery\n");
#endif
			return ERROR;
		        }

	        }

#ifdef DEBUG0
	printf("check_contact_host_notification_viability() end\n");
#endif

	return OK;
        }



/* notify a specific contact that an entire host is down or up */
int notify_contact_of_host(contact *cntct,host *hst,int state, char *ack_data){
	commandsmember *temp_commandsmember;
	char temp_command_line[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	char raw_command_line[MAX_INPUT_BUFFER];
	char command_line[MAX_INPUT_BUFFER];
	int early_timeout;


#ifdef DEBUG0
	printf("notify_contact_of_host() start\n");
#endif
#ifdef DEBUG4
	printf("\tNotify user %s\n",cntct->name);
#endif

	/* see if the contact can be notified at this time */
	if(check_time_against_period(time(NULL),cntct->host_notification_period)==ERROR){
#ifdef DEBUG4
		printf("\tThis contact shouldn't be notified at this time!\n");
#endif
		return OK;
	        }

	/* check viability of notifying this user about the host */
	/* acknowledgements are no longer excluded from this test - added 8/19/02 Tom Bertelson */
	if(check_contact_host_notification_viability(cntct,hst,state)==ERROR)
		return OK;

	/* process all the notification commands this user has */
	for(temp_commandsmember=cntct->host_notification_commands;temp_commandsmember!=NULL;temp_commandsmember=temp_commandsmember->next){

		/* get the raw command line */
		strncpy(temp_command_line,temp_commandsmember->command,sizeof(temp_command_line)-1);
		temp_command_line[sizeof(temp_command_line)-1]='\x0';
		get_raw_command_line(temp_command_line,&raw_command_line[0],(int)sizeof(raw_command_line));

		/* run the notification command */
		if(strcmp(raw_command_line,"")){


#ifdef DEBUG4
			printf("\tRaw Command:       %s\n",raw_command_line);
#endif

			/* replace macros in the command line */
			process_macros(raw_command_line,&command_line[0],(int)sizeof(command_line),STRIP_ILLEGAL_MACRO_CHARS|ESCAPE_MACRO_CHARS);

#ifdef DEBUG4
			printf("\tProcessed Command: %s\n",command_line);
#endif

			/* log the notification to program log file */
			if(log_notifications==TRUE){
				if(ack_data==NULL)
					snprintf(temp_buffer,sizeof(temp_buffer),"HOST NOTIFICATION: %s;%s;%s;%s;%s\n",cntct->name,hst->name,macro_x[MACRO_HOSTSTATE],temp_command_line,macro_x[MACRO_OUTPUT]);
				else
					snprintf(temp_buffer,sizeof(temp_buffer),"HOST NOTIFICATION: %s;%s;ACKNOWLEDGEMENT (%s);%s;%s\n",cntct->name,hst->name,macro_x[MACRO_HOSTSTATE],temp_command_line,macro_x[MACRO_OUTPUT]);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_HOST_NOTIFICATION,FALSE);
			        }

			/* run the command */
			my_system(command_line,notification_timeout,&early_timeout,NULL,0);

			/* check to see if the notification timed out */
			if(early_timeout==TRUE){
				snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Contact '%s' host notification command '%s' timed out after %d seconds\n",cntct->name,temp_commandsmember->command,DEFAULT_NOTIFICATION_TIMEOUT);
				temp_buffer[sizeof(temp_buffer)-1]='\x0';
				write_to_logs_and_console(temp_buffer,NSLOG_HOST_NOTIFICATION | NSLOG_RUNTIME_WARNING,TRUE);
			        }
		        }
	        }


#ifdef DEBUG0
	printf("notify_contact_of_host() end\n");
#endif

	return OK;
        }


/* checks to see if a host escalation entry is a match for the current host notification */
int is_valid_host_escalation_for_host_notification(host *hst,int state,hostescalation *he){
	host *temp_host;
	int notification_number;

#ifdef DEBUG0
	printf("is_valid_host_escalation_for_host_notification() start\n");
#endif

	/* if this is a recovery, really we check for who got notified about a previous problem */
	if(state==HOST_UP)
		notification_number=hst->current_notification_number-1;
	else
		notification_number=hst->current_notification_number;

	/* find the host this escalation entry is associated with */
	temp_host=find_host(he->host_name);
	if(temp_host==NULL || temp_host!=hst)
		return FALSE;

	/* skip this escalation if it happens later */
	if(he->first_notification > notification_number)
		return FALSE;

	/* skip this escalation if it has already passed */
	if(he->last_notification!=0 && he->last_notification < notification_number)
		return FALSE;

#ifdef DEBUG0
	printf("is_valid_host_escalation_for_host_notification() end\n");
#endif

	return TRUE;
        }



/* checks to see whether a host notification should be escalation */
int should_host_notification_be_escalated(host *hst,int state){
	hostescalation *temp_he;

#ifdef DEBUG0
	printf("should_host_notification_be_escalated() start\n");
#endif

	/* search the host escalation list */
	for(temp_he=hostescalation_list;temp_he!=NULL;temp_he=temp_he->next){

		/* we found a matching entry, so escalate this notification! */
		if(is_valid_host_escalation_for_host_notification(hst,state,temp_he)==TRUE)
			return TRUE;
	        }

#ifdef DEBUG0
	printf("should_host_notification_be_escalated() end\n");
#endif

	return FALSE;
        }


/* given a host, create a list of contacts to be notified, removing duplicates */
int create_notification_list_from_host(host *hst,int state){
	hostescalation *temp_he;
	contactgroupsmember *temp_group;
	contactgroup *temp_contactgroup;
	contact *temp_contact;

#ifdef DEBUG0
	printf("create_notification_list_from_host() start\n");
#endif

	/* see if this notification should be escalated */
	if(should_host_notification_be_escalated(hst,state)==TRUE){

		/* check all the host escalation entries */
		for(temp_he=hostescalation_list;temp_he!=NULL;temp_he=temp_he->next){

			/* see if this escalation if valid for this notification */
			if(is_valid_host_escalation_for_host_notification(hst,state,temp_he)==FALSE)
				continue;

			/* find each contact group in this escalation entry */
			for(temp_group=temp_he->contact_groups;temp_group!=NULL;temp_group=temp_group->next){

				temp_contactgroup=find_contactgroup(temp_group->group_name,NULL);
				if(temp_contactgroup==NULL)
					continue;

				/* check all contacts */
				for(temp_contact=contact_list;temp_contact!=NULL;temp_contact=temp_contact->next){
					
					if(is_contact_member_of_contactgroup(temp_contactgroup,temp_contact)==TRUE)
						add_notification(temp_contact);
				        }
			        }
		        }
	        }

	/* else we shouldn't escalate the notification, so continue as normal... */
	else{

		/* get all contacts for this host */
		for(temp_contact=contact_list;temp_contact!=NULL;temp_contact=temp_contact->next){

			if(is_contact_for_host(hst,temp_contact)==TRUE)
				add_notification(temp_contact);
		        }
	        }

#ifdef DEBUG0
	printf("create_notification_list_from_host() end\n");
#endif

	return OK;
        }




/******************************************************************/
/***************** NOTIFICATION TIMING FUNCTIONS ******************/
/******************************************************************/


/* calculates next acceptable re-notification time for a service */
time_t get_next_service_notification_time(service *svc, time_t offset){
	time_t next_notification;
	int interval_to_use;
	serviceescalation *temp_se;
	int have_escalated_interval=FALSE;

#ifdef DEBUG0
	printf("get_next_service_notification_time() start\n");
#endif

#ifdef DEBUG4
	printf("\tCalculating next valid notification time...\n");
#endif

	/* default notification interval */
	interval_to_use=svc->notification_interval;

#ifdef DEBUG4
	printf("\t\tDefault interval: %d\n",interval_to_use);
#endif

	/* search all the escalation entries for valid matches for this service (at its current notification number) */
	for(temp_se=serviceescalation_list;temp_se!=NULL;temp_se=temp_se->next){

		/* interval < 0 means to use non-escalated interval */
		if(temp_se->notification_interval<0)
			continue;

		/* skip this entry if it isn't appropriate */
		if(is_valid_escalation_for_service_notification(svc,temp_se)==FALSE)
			continue;

#ifdef DEBUG4
		printf("\t\tFound a valid escalation w/ interval of %d\n",temp_se->notification_interval);
#endif

		/* if we haven't used a notification interval from an escalation yet, use this one */
		if(have_escalated_interval==FALSE){
			have_escalated_interval=TRUE;
			interval_to_use=temp_se->notification_interval;
		        }

		/* else use the shortest of all valid escalation intervals */
		else if(temp_se->notification_interval<interval_to_use)
			interval_to_use=svc->notification_interval;
#ifdef DEBUG4
		printf("\t\tNew interval: %d\n",interval_to_use);
#endif

	        }

	/* if notification interval is 0, we shouldn't send any more problem notifications */
	if(interval_to_use==0)
		svc->no_more_notifications=TRUE;

#ifdef DEBUG4
	printf("\tInterval used for calculating next valid notification time: %d\n",interval_to_use);
#endif

	/* calculate next notification time */
	next_notification=offset+(interval_to_use*interval_length);

#ifdef DEBUG0
	printf("get_next_service_notification_time() end\n");
#endif

	return next_notification;
        }



/* calculates next acceptable re-notification time for a host */
time_t get_next_host_notification_time(host *hst, int state, time_t offset){
	time_t next_notification;
	int interval_to_use;
	hostescalation *temp_he;
	int have_escalated_interval=FALSE;

#ifdef DEBUG0
	printf("get_next_host_notification_time() start\n");
#endif

	/* default notification interval */
	interval_to_use=hst->notification_interval;

	/* check all the host escalation entries for valid matches for this host (at its current notification number) */
	for(temp_he=hostescalation_list;temp_he!=NULL;temp_he=temp_he->next){

		/* interval < 0 means to use non-escalated interval */
		if(temp_he->notification_interval<0)
			continue;

		/* skip this entry if it itsn't appropriate */
		if(is_valid_host_escalation_for_host_notification(hst,state,temp_he)==FALSE)
			continue;

		/* if we haven't used a notification interval from an escalation yet, use this one */
		if(have_escalated_interval==FALSE){
			have_escalated_interval=TRUE;
			interval_to_use=temp_he->notification_interval;
		        }

		/* else use the shortest of all valid escalation intervals  */
		else if(temp_he->notification_interval<interval_to_use)
			interval_to_use=temp_he->notification_interval;
	        }

	/* if interval is 0, no more notifications should be sent */
	if(interval_to_use==0)
		hst->no_more_notifications=TRUE;
	else
		hst->no_more_notifications=FALSE;

	/* calculate next notification time */
	next_notification=offset+(interval_to_use*interval_length);

#ifdef DEBUG0
	printf("get_next_host_notification_time() end\n");
#endif

	return next_notification;
        }



/******************************************************************/
/***************** NOTIFICATION OBJECT FUNCTIONS ******************/
/******************************************************************/


/* given a contact name, find the notification entry for them for the list in memory */
notification * find_notification(char *contact_name){
	notification *temp_notification;

#ifdef DEBUG0
	printf("find_notification() start\n");
#endif

	if(contact_name==NULL)
		return NULL;
	
	temp_notification=notification_list;
	while(temp_notification!=NULL){
		if(!strcmp(contact_name,temp_notification->contact->name))
		  return temp_notification;
		temp_notification=temp_notification->next;
	        }

#ifdef DEBUG0
	printf("find_notification() end\n");
#endif

	/* we couldn't find the contact in the notification list */
	return NULL;
        }
	


/* add a new notification to the list in memory */
int add_notification(contact *cntct){
	notification *new_notification;
	notification *temp_notification;

#ifdef DEBUG0
	printf("add_notification() start\n");
#endif
#ifdef DEBUG1
	printf("\tAdd contact '%s'\n",cntct->name);
#endif

	/* don't add anything if this contact is already on the notification list */
	temp_notification=find_notification(cntct->name);
	if(temp_notification!=NULL)
		return OK;

	/* allocate memory for a new contact in the notification list */
	new_notification=malloc(sizeof(notification));
	if(new_notification==NULL)
		return ERROR;

	/* fill in the contact info */
	new_notification->contact=cntct;

	/* add new notification to head of list */
	new_notification->next=notification_list;
	notification_list=new_notification;

#ifdef DEBUG0
	printf("add_notification() end\n");
#endif

	return OK;
        }

