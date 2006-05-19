/*****************************************************************************
 *
 * FLAPPING.C - State flap detection and handling routines for Nagios
 *
 * Copyright (c) 2001-2005 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   05-18-2005
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
#include "../include/comments.h"
#include "../include/statusdata.h"
#include "../include/nagios.h"
#include "../include/broker.h"

extern int      interval_length;

extern int      enable_flap_detection;

extern double   low_service_flap_threshold;
extern double   high_service_flap_threshold;
extern double   low_host_flap_threshold;
extern double   high_host_flap_threshold;


/******************************************************************/
/******************** FLAP DETECTION FUNCTIONS ********************/
/******************************************************************/


/* detects service flapping */
void check_for_service_flapping(service *svc, int update_history){
	int is_flapping=FALSE;
	int x,y;
	int last_state_history_value=STATE_OK;
	double curved_changes=0.0;
	double curved_percent_change=0.0;
	double low_threshold=0.0;
	double high_threshold=0.0;
	double low_curve_value=0.75;
	double high_curve_value=1.25;

#ifdef DEBUG0
	printf("check_for_service_flapping() start\n");
#endif

	/* if this is a soft service state and not a soft recovery, don't record this in the history */
	/* only hard states and soft recoveries get recorded for flap detection */
	if(svc->state_type==SOFT_STATE && svc->current_state!=STATE_OK)
		return;

	/* what threshold values should we use (global or service-specific)? */
	low_threshold=(svc->low_flap_threshold<=0.0)?low_service_flap_threshold:svc->low_flap_threshold;
	high_threshold=(svc->high_flap_threshold<=0.0)?high_service_flap_threshold:svc->high_flap_threshold;

	if(update_history==TRUE){

		/* record the current state in the state history */
		svc->state_history[svc->state_history_index]=svc->current_state;

		/* increment state history index to next available slot */
		svc->state_history_index++;
		if(svc->state_history_index>=MAX_STATE_HISTORY_ENTRIES)
			svc->state_history_index=0;
	        }

	/* calculate overall and curved percent state changes */
	for(x=0,y=svc->state_history_index;x<MAX_STATE_HISTORY_ENTRIES;x++){

		if(x==0){
			last_state_history_value=svc->state_history[y];
			y++;
			if(y>=MAX_STATE_HISTORY_ENTRIES)
				y=0;
			continue;
		        }

		if(last_state_history_value!=svc->state_history[y])
			curved_changes+=(((double)(x-1)*(high_curve_value-low_curve_value))/((double)(MAX_STATE_HISTORY_ENTRIES-2)))+low_curve_value;

		last_state_history_value=svc->state_history[y];

		y++;
		if(y>=MAX_STATE_HISTORY_ENTRIES)
			y=0;
	        }

	/* calculate overall percent change in state */
	curved_percent_change=(double)(((double)curved_changes*100.0)/(double)(MAX_STATE_HISTORY_ENTRIES-1));

	svc->percent_state_change=curved_percent_change;


	/* are we flapping, undecided, or what?... */

	/* we're undecided, so don't change the current flap state */
	if(curved_percent_change>low_threshold && curved_percent_change<high_threshold)
		return;

	/* we're below the lower bound, so we're not flapping */
	else if(curved_percent_change<=low_threshold)
		is_flapping=FALSE;
       
	/* else we're above the upper bound, so we are flapping */
	else if(curved_percent_change>=high_threshold)
		is_flapping=TRUE;

	/* so what should we do (if anything)? */

	/* don't do anything if we don't have flap detection enabled on a program-wide basis */
	if(enable_flap_detection==FALSE)
		return;

	/* don't do anything if we don't have flap detection enabled for this service */
	if(svc->flap_detection_enabled==FALSE)
		return;

	/* did the service just start flapping? */
	if(is_flapping==TRUE && svc->is_flapping==FALSE)
		set_service_flap(svc,curved_percent_change,high_threshold,low_threshold);

	/* did the service just stop flapping? */
	else if(is_flapping==FALSE && svc->is_flapping==TRUE)
		clear_service_flap(svc,curved_percent_change,high_threshold,low_threshold);

#ifdef DEBUG0
	printf("check_for_service_flapping() end\n");
#endif

	return;
        }


/* detects host flapping */
void check_for_host_flapping(host *hst, int update_history){
	int is_flapping=FALSE;
	int x;
	int last_state_history_value=HOST_UP;
	unsigned long wait_threshold;
	double curved_changes=0.0;
	double curved_percent_change=0.0;
	time_t current_time;
	double low_threshold=0.0;
	double high_threshold=0.0;
	double low_curve_value=0.75;
	double high_curve_value=1.25;

#ifdef DEBUG0
	printf("check_for_host_flapping() start\n");
#endif

	time(&current_time);

	/* period to wait for updating archived state info if we have no state change */
	if(hst->total_services==0)
		wait_threshold=hst->notification_interval*interval_length;
	
	else
		wait_threshold=(hst->total_service_check_interval*interval_length)/hst->total_services;

	/* if we haven't waited long enough since last record, only update if we've had a state change */
	if((current_time-hst->last_state_history_update)<wait_threshold){

		/* get the last recorded state */
		last_state_history_value=hst->state_history[(hst->state_history_index==0)?MAX_STATE_HISTORY_ENTRIES-1:hst->state_history_index-1];

		/* if we haven't had a state change since our last recorded state, bail out */
		if(last_state_history_value==hst->current_state)
			return;
	        }

	/* what thresholds should we use (global or host-specific)? */
	low_threshold=(hst->low_flap_threshold<=0.0)?low_host_flap_threshold:hst->low_flap_threshold;
	high_threshold=(hst->high_flap_threshold<=0.0)?high_host_flap_threshold:hst->high_flap_threshold;

	if(update_history==TRUE){

		/* update the last record time */
		hst->last_state_history_update=current_time;

		/* record the current state in the state history */
		hst->state_history[hst->state_history_index]=hst->current_state;

		/* increment state history index to next available slot */
		hst->state_history_index++;
		if(hst->state_history_index>=MAX_STATE_HISTORY_ENTRIES)
			hst->state_history_index=0;
	        }

	/* calculate overall changes in state */
	for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++){

		if(x==0){
			last_state_history_value=hst->state_history[x];
			continue;
		        }

		if(last_state_history_value!=hst->state_history[x])
			curved_changes+=(((double)(x-1)*(high_curve_value-low_curve_value))/((double)(MAX_STATE_HISTORY_ENTRIES-2)))+low_curve_value;

		last_state_history_value=hst->state_history[x];
	        }

	/* calculate overall percent change in state */
	curved_percent_change=(double)(((double)curved_changes*100.0)/(double)(MAX_STATE_HISTORY_ENTRIES-1));

	hst->percent_state_change=curved_percent_change;


	/* are we flapping, undecided, or what?... */

	/* we're undecided, so don't change the current flap state */
	if(curved_percent_change>low_threshold && curved_percent_change<high_threshold)
		return;

	/* we're below the lower bound, so we're not flapping */
	else if(curved_percent_change<=low_threshold)
		is_flapping=FALSE;
       
	/* else we're above the upper bound, so we are flapping */
	else if(curved_percent_change>=high_threshold)
		is_flapping=TRUE;

	/* so what should we do (if anything)? */

	/* don't do anything if we don't have flap detection enabled on a program-wide basis */
	if(enable_flap_detection==FALSE)
		return;

	/* don't do anything if we don't have flap detection enabled for this host */
	if(hst->flap_detection_enabled==FALSE)
		return;

	/* did the host just start flapping? */
	if(is_flapping==TRUE && hst->is_flapping==FALSE)
		set_host_flap(hst,curved_percent_change,high_threshold,low_threshold);

	/* did the host just stop flapping? */
	else if(is_flapping==FALSE && hst->is_flapping==TRUE)
		clear_host_flap(hst,curved_percent_change,high_threshold,low_threshold);

#ifdef DEBUG0
	printf("check_for_host_flapping() end\n");
#endif

	return;
        }


/******************************************************************/
/********************* FLAP HANDLING FUNCTIONS ********************/
/******************************************************************/


/* handles a service that is flapping */
void set_service_flap(service *svc, double percent_change, double high_threshold, double low_threshold){
	char buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("set_service_flap() start\n");
#endif

	/* log a notice - this one is parsed by the history CGI */
	snprintf(buffer,sizeof(buffer)-1,"SERVICE FLAPPING ALERT: %s;%s;STARTED; Service appears to have started flapping (%2.1f%% change >= %2.1f%% threshold)\n",svc->host_name,svc->description,percent_change,high_threshold);
	buffer[sizeof(buffer)-1]='\x0';
	write_to_all_logs(buffer,NSLOG_RUNTIME_WARNING);

	/* add a non-persistent comment to the service */
	snprintf(buffer,sizeof(buffer)-1,"Notifications for this service are being suppressed because it was detected as having been flapping between different states (%2.1f%% change >= %2.1f%% threshold).  When the service state stabilizes and the flapping stops, notifications will be re-enabled.",percent_change,high_threshold);
	buffer[sizeof(buffer)-1]='\x0';
	add_new_service_comment(FLAPPING_COMMENT,svc->host_name,svc->description,time(NULL),"(Nagios Process)",buffer,0,COMMENTSOURCE_INTERNAL,FALSE,(time_t)0,&(svc->flapping_comment_id));

	/* set the flapping indicator */
	svc->is_flapping=TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_flapping_data(NEBTYPE_FLAPPING_START,NEBFLAG_NONE,NEBATTR_NONE,SERVICE_FLAPPING,svc,percent_change,high_threshold,low_threshold,NULL);
#endif

	/* see if we should check to send a recovery notification out when flapping stops */
	if(svc->current_state!=STATE_OK && svc->current_notification_number>0)
		svc->check_flapping_recovery_notification=TRUE;
	else
		svc->check_flapping_recovery_notification=FALSE;

	/* send a notification */
	service_notification(svc,NOTIFICATION_FLAPPINGSTART,NULL,NULL);

#ifdef DEBUG0
	printf("set_service_flap() end\n");
#endif

	return;
        }


/* handles a service that has stopped flapping */
void clear_service_flap(service *svc, double percent_change, double high_threshold, double low_threshold){
	char buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("clear_service_flap() start\n");
#endif

	/* log a notice - this one is parsed by the history CGI */
	snprintf(buffer,sizeof(buffer)-1,"SERVICE FLAPPING ALERT: %s;%s;STOPPED; Service appears to have stopped flapping (%2.1f%% change < %2.1f%% threshold)\n",svc->host_name,svc->description,percent_change,low_threshold);
	buffer[sizeof(buffer)-1]='\x0';
	write_to_all_logs(buffer,NSLOG_INFO_MESSAGE);

	/* delete the comment we added earlier */
	if(svc->flapping_comment_id!=0)
		delete_service_comment(svc->flapping_comment_id);
	svc->flapping_comment_id=0;

	/* clear the flapping indicator */
	svc->is_flapping=FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_flapping_data(NEBTYPE_FLAPPING_STOP,NEBFLAG_NONE,NEBATTR_FLAPPING_STOP_NORMAL,SERVICE_FLAPPING,svc,percent_change,high_threshold,low_threshold,NULL);
#endif

	/* send a notification */
	service_notification(svc,NOTIFICATION_FLAPPINGSTOP,NULL,NULL);

	/* should we send a recovery notification? */
	if(svc->check_flapping_recovery_notification==TRUE && svc->current_state==STATE_OK)
		service_notification(svc,NOTIFICATION_NORMAL,NULL,NULL);

	/* clear the recovery notification flag */
	svc->check_flapping_recovery_notification=FALSE;

#ifdef DEBUG0
	printf("clear_service_flap() end\n");
#endif

	return;
        }


/* handles a host that is flapping */
void set_host_flap(host *hst, double percent_change, double high_threshold, double low_threshold){
	char buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("set_host_flap() start\n");
#endif

	/* log a notice - this one is parsed by the history CGI */
	snprintf(buffer,sizeof(buffer)-1,"HOST FLAPPING ALERT: %s;STARTED; Host appears to have started flapping (%2.1f%% change > %2.1f%% threshold)\n",hst->name,percent_change,high_threshold);
	buffer[sizeof(buffer)-1]='\x0';
	write_to_all_logs(buffer,NSLOG_RUNTIME_WARNING);

	/* add a non-persistent comment to the host */
	snprintf(buffer,sizeof(buffer)-1,"Notifications for this host are being suppressed because it was detected as having been flapping between different states (%2.1f%% change > %2.1f%% threshold).  When the host state stabilizes and the flapping stops, notifications will be re-enabled.",percent_change,high_threshold);
	buffer[sizeof(buffer)-1]='\x0';
	add_new_host_comment(FLAPPING_COMMENT,hst->name,time(NULL),"(Nagios Process)",buffer,0,COMMENTSOURCE_INTERNAL,FALSE,(time_t)0,&(hst->flapping_comment_id));

	/* set the flapping indicator */
	hst->is_flapping=TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_flapping_data(NEBTYPE_FLAPPING_START,NEBFLAG_NONE,NEBATTR_NONE,HOST_FLAPPING,hst,percent_change,high_threshold,low_threshold,NULL);
#endif

	/* see if we should check to send a recovery notification out when flapping stops */
	if(hst->current_state!=HOST_UP && hst->current_notification_number>0)
		hst->check_flapping_recovery_notification=TRUE;
	else
		hst->check_flapping_recovery_notification=FALSE;

	/* send a notification */
	host_notification(hst,NOTIFICATION_FLAPPINGSTART,NULL,NULL);

#ifdef DEBUG0
	printf("set_host_flap() end\n");
#endif

	return;
        }


/* handles a host that has stopped flapping */
void clear_host_flap(host *hst, double percent_change, double high_threshold, double low_threshold){
	char buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("clear_host_flap() start\n");
#endif

	/* log a notice - this one is parsed by the history CGI */
	snprintf(buffer,sizeof(buffer)-1,"HOST FLAPPING ALERT: %s;STOPPED; Host appears to have stopped flapping (%2.1f%% change < %2.1f%% threshold)\n",hst->name,percent_change,low_threshold);
	buffer[sizeof(buffer)-1]='\x0';
	write_to_all_logs(buffer,NSLOG_INFO_MESSAGE);

	/* delete the comment we added earlier */
	if(hst->flapping_comment_id!=0)
		delete_host_comment(hst->flapping_comment_id);
	hst->flapping_comment_id=0;

	/* clear the flapping indicator */
	hst->is_flapping=FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_flapping_data(NEBTYPE_FLAPPING_STOP,NEBFLAG_NONE,NEBATTR_FLAPPING_STOP_NORMAL,HOST_FLAPPING,hst,percent_change,high_threshold,low_threshold,NULL);
#endif

	/* send a notification */
	host_notification(hst,NOTIFICATION_FLAPPINGSTOP,NULL,NULL);

	/* should we send a recovery notification? */
	if(hst->check_flapping_recovery_notification==TRUE && hst->current_state==HOST_UP)
		host_notification(hst,NOTIFICATION_NORMAL,NULL,NULL);

	/* clear the recovery notification flag */
	hst->check_flapping_recovery_notification=FALSE;

#ifdef DEBUG0
	printf("clear_host_flap() end\n");
#endif

	return;
        }



/******************************************************************/
/***************** FLAP DETECTION STATUS FUNCTIONS ****************/
/******************************************************************/

/* enables flap detection on a program wide basis */
void enable_flap_detection_routines(void){

#ifdef DEBUG0
	printf("enable_flap_detection() start\n");
#endif

	/* set flap detection flag */
	enable_flap_detection=TRUE;

	/* update program status */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("enable_flap_detection() end\n");
#endif

	return;
        }



/* disables flap detection on a program wide basis */
void disable_flap_detection_routines(void){

#ifdef DEBUG0
	printf("disable_flap_detection() start\n");
#endif

	/* set flap detection flag */
	enable_flap_detection=FALSE;

	/* update program status */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("disable_flap_detection() end\n");
#endif

	return;
        }



/* enables flap detection for a specific host */
void enable_host_flap_detection(host *hst){
	int x;

#ifdef DEBUG0
	printf("enable_host_flap_detection() start\n");
#endif

	/* nothing to do... */
	if(hst->flap_detection_enabled==TRUE)
		return;

	/* reset the archived state history */
	for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
		hst->state_history[x]=hst->current_state;

	/* reset percent state change indicator */
	hst->percent_state_change=0.0;

	/* set the flap detection enabled flag */
	hst->flap_detection_enabled=TRUE;

	/* update host status */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("enable_host_flap_detection() end\n");
#endif

	return;
        }



/* disables flap detection for a specific host */
void disable_host_flap_detection(host *hst){
	char buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("disable_host_flap_detection() start\n");
#endif

	/* nothing to do... */
	if(hst->flap_detection_enabled==FALSE)
		return;

	/* set the flap detection enabled flag */
	hst->flap_detection_enabled=FALSE;

	/* if the host was flapping, remove the flapping indicator */
	if(hst->is_flapping==TRUE){

		hst->is_flapping=FALSE;

		/* delete the original comment we added earlier */
		if(hst->flapping_comment_id!=0)
			delete_host_comment(hst->flapping_comment_id);
		hst->flapping_comment_id=0;

		/* log a notice - this one is parsed by the history CGI */
		snprintf(buffer,sizeof(buffer)-1,"HOST FLAPPING ALERT: %s;DISABLED; Flap detection has been disabled\n",hst->name);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_all_logs(buffer,NSLOG_INFO_MESSAGE);

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_flapping_data(NEBTYPE_FLAPPING_STOP,NEBFLAG_NONE,NEBATTR_FLAPPING_STOP_DISABLED,HOST_FLAPPING,hst,hst->percent_state_change,0.0,0.0,NULL);
#endif
	        }

	/* reset the percent change indicator */
	hst->percent_state_change=0.0;

	/* update host status */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("disable_host_flap_detection() end\n");
#endif

	return;
        }


/* enables flap detection for a specific service */
void enable_service_flap_detection(service *svc){
	int x;

#ifdef DEBUG0
	printf("enable_service_flap_detection() start\n");
#endif

	/* nothing to do... */
	if(svc->flap_detection_enabled==TRUE)
		return;

	/* reset the archived state history */
	for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
		svc->state_history[x]=svc->current_state;

	/* reset percent state change indicator */
	svc->percent_state_change=0.0;

	/* set the flap detection enabled flag */
	svc->flap_detection_enabled=TRUE;

	/* update service status */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("enable_service_flap_detection() end\n");
#endif

	return;
        }



/* disables flap detection for a specific service */
void disable_service_flap_detection(service *svc){
	char buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("disable_service_flap_detection() start\n");
#endif

	/* nothing to do... */
	if(svc->flap_detection_enabled==FALSE)
		return;

	/* set the flap detection enabled flag */
	svc->flap_detection_enabled=FALSE;

	/* if the service was flapping, remove the flapping indicator */
	if(svc->is_flapping==TRUE){

		svc->is_flapping=FALSE;

		/* delete the original comment we added earlier */
		if(svc->flapping_comment_id!=0)
			delete_service_comment(svc->flapping_comment_id);
		svc->flapping_comment_id=0;

		/* log a notice - this one is parsed by the history CGI */
		snprintf(buffer,sizeof(buffer)-1,"SERVICE FLAPPING ALERT: %s;%s;DISABLED; Flap detection has been disabled\n",svc->host_name,svc->description);
		buffer[sizeof(buffer)-1]='\x0';
		write_to_all_logs(buffer,NSLOG_INFO_MESSAGE);

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_flapping_data(NEBTYPE_FLAPPING_STOP,NEBFLAG_NONE,NEBATTR_FLAPPING_STOP_DISABLED,SERVICE_FLAPPING,svc,svc->percent_state_change,0.0,0.0,NULL);
#endif
	        }

	/* reset the percent change indicator */
	svc->percent_state_change=0.0;

	/* update service status */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("disable_service_flap_detection() end\n");
#endif

	return;
        }






