/*****************************************************************************
 *
 * NOTIFICATIONS.C - Service and host notification functions for Nagios
 *
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

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"
#include "../include/macros.h"
#include "../include/nagios.h"
#include "../include/broker.h"
#include "../include/neberrors.h"
#include "../include/workers.h"
#include "../include/downtime.h"

/*** silly helpers ****/
static contact *find_contact_by_name_or_alias(const char *name)
{
	contact *c = NULL;

	if (!name || !(c = find_contact(name)))
		return c;
	for (c = contact_list; c; c = c->next)
		if (!strcmp(c->alias, name))
			break;

	return c;
}

const char *notification_reason_name(unsigned int reason_type)
{
	static const char *names[] = {
		"NORMAL", "ACKNOWLEDGEMENT",
		"FLAPPINGSTART", "FLAPPINGSTOP", "FLAPPINGDISABLED",
		"DOWNTIMESTART", "DOWNTIMEEND", "DOWNTIMECANCELLED",
		"CUSTOM"
	};

	if (reason_type < sizeof(names))
		return names[reason_type];

	return "(unknown)";
}


/******************************************************************/
/***************** SERVICE NOTIFICATION FUNCTIONS *****************/
/******************************************************************/


/* notify contacts about a service problem or recovery */
int service_notification(service *svc, int type, char *not_author, char *not_data, int options) {
	host *temp_host = NULL;
	notification *temp_notification = NULL;
	contact *temp_contact = NULL;
	time_t current_time;
	struct timeval start_time;
	struct timeval end_time;
	int escalated = FALSE;
	int result = OK;
	int contacts_notified = 0;
	int increment_notification_number = FALSE;
	nagios_macros mac;
	int neb_result;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "service_notification()\n");

	/* get the current time */
	time(&current_time);
	gettimeofday(&start_time, NULL);

	log_debug_info(DEBUGL_NOTIFICATIONS, 0, "** Service Notification Attempt ** Host: '%s', Service: '%s', Type: %s, Options: %d, Current State: %d, Last Notification: %s", svc->host_name, svc->description, notification_reason_name(type), options, svc->current_state, ctime(&svc->last_notification));

	/* if we couldn't find the host, return an error */
	if((temp_host = svc->host_ptr) == NULL) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 0, "Couldn't find the host associated with this service, so we won't send a notification!\n");
		return ERROR;
		}

	/* check the viability of sending out a service notification */
	if(check_service_notification_viability(svc, type, options) == ERROR) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 0, "Notification viability test failed.  No notification will be sent out.\n");
		/* Set next_notification time if we're in a downtime and
			notification_interval = zero, otherwise if the service
			doesn't recover after downtime ends, it will never send
			the notification */
			if (svc->notification_interval == 0.0) {
				if (temp_host->scheduled_downtime_depth > 0 || svc->scheduled_downtime_depth > 0)
					svc->next_notification = current_time;
				}
		return OK;
		}

	log_debug_info(DEBUGL_NOTIFICATIONS, 0, "Notification viability test passed.\n");

	/* should the notification number be increased? */
	if(type == NOTIFICATION_NORMAL || (options & NOTIFICATION_OPTION_INCREMENT)) {
		svc->current_notification_number++;
		increment_notification_number = TRUE;
		}

	log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Current notification number: %d (%s)\n", svc->current_notification_number, (increment_notification_number == TRUE) ? "incremented" : "unchanged");

	/* save and increase the current notification id */
	svc->current_notification_id = next_notification_id;
	next_notification_id++;

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Creating list of contacts to be notified.\n");

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec = 0L;
	end_time.tv_usec = 0L;
	neb_result = broker_notification_data(NEBTYPE_NOTIFICATION_START, NEBFLAG_NONE, NEBATTR_NONE, SERVICE_NOTIFICATION, type, start_time, end_time, (void *)svc, not_author, not_data, escalated, 0, NULL);
	if(neb_result == NEBERROR_CALLBACKCANCEL || neb_result == NEBERROR_CALLBACKOVERRIDE) {
		log_debug_info(DEBUGL_CHECKS, 0, "Service notification to %s;%s (id=%u) was blocked by a module\n",
		               svc->host_name, svc->description, svc->id);
		free_notification_list();
		return neb_result == NEBERROR_CALLBACKOVERRIDE ? OK : ERROR;
		}
#endif

	/* allocate memory for macros */
	memset(&mac, 0, sizeof(mac));

	/* create the contact notification list for this service */

	/*
	 * check viability before adding a contact to the notification
	 * list and build up the $NOTIFICATIONRECIPIENTS$ macro while
	 * we're at it.
	 * This prevents us from running through all the steps again in
	 * notify_contact_of_host|service.
	 * Furthermore the $NOTIFICATIONRECIPIENTS$ macro will contain
	 * only actual recipients (as the name implies), and not all
	 * contacts assigned to that host|service.
	 *
	 * note: checks against timeperiod will happen now(),
	 * and not when the notification is actually being sent.
	 */
	create_notification_list_from_service(&mac, svc, options, &escalated, type);

	/* we have contacts to notify... */
	if(notification_list != NULL) {

		/* grab the macro variables */
		grab_host_macros_r(&mac, temp_host);
		grab_service_macros_r(&mac, svc);

		/* if this notification has an author, attempt to lookup the associated contact */
		if(not_author != NULL) {
			temp_contact = find_contact_by_name_or_alias(not_author);
			}

		/* get author and comment macros */
		if(not_author)
			mac.x[MACRO_NOTIFICATIONAUTHOR] = strdup(not_author);
		if(temp_contact != NULL) {
			mac.x[MACRO_NOTIFICATIONAUTHORNAME] = strdup(temp_contact->name);
			mac.x[MACRO_NOTIFICATIONAUTHORALIAS] = strdup(temp_contact->alias);
			}
		if(not_data)
			mac.x[MACRO_NOTIFICATIONCOMMENT] = strdup(not_data);

		/* NOTE: these macros are deprecated and will likely disappear in Nagios 4.x */
		/* if this is an acknowledgement, get author and comment macros */
		if(type == NOTIFICATION_ACKNOWLEDGEMENT) {
			if(not_author)
				mac.x[MACRO_SERVICEACKAUTHOR] = strdup(not_author);

			if(not_data)
				mac.x[MACRO_SERVICEACKCOMMENT] = strdup(not_data);

			if(temp_contact != NULL) {
				mac.x[MACRO_SERVICEACKAUTHORNAME] = strdup(temp_contact->name);
				mac.x[MACRO_SERVICEACKAUTHORALIAS] = strdup(temp_contact->alias);
				}
			}

		/* set the notification type macro */
		if(type == NOTIFICATION_ACKNOWLEDGEMENT)
			mac.x[MACRO_NOTIFICATIONTYPE] = "ACKNOWLEDGEMENT";
		else if(type == NOTIFICATION_FLAPPINGSTART)
			mac.x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGSTART";
		else if(type == NOTIFICATION_FLAPPINGSTOP)
			mac.x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGSTOP";
		else if(type == NOTIFICATION_FLAPPINGDISABLED)
			mac.x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGDISABLED";
		else if(type == NOTIFICATION_DOWNTIMESTART)
			mac.x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMESTART";
		else if(type == NOTIFICATION_DOWNTIMEEND)
			mac.x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMEEND";
		else if(type == NOTIFICATION_DOWNTIMECANCELLED)
			mac.x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMECANCELLED";
		else if(type == NOTIFICATION_CUSTOM)
			mac.x[MACRO_NOTIFICATIONTYPE] = "CUSTOM";
		else if(svc->current_state == STATE_OK)
			mac.x[MACRO_NOTIFICATIONTYPE] = "RECOVERY";
		else
			mac.x[MACRO_NOTIFICATIONTYPE] = "PROBLEM";

		mac.x[MACRO_NOTIFICATIONTYPE] = strdup(mac.x[MACRO_NOTIFICATIONTYPE]);

		/* set the notification number macro */
		asprintf(&mac.x[MACRO_SERVICENOTIFICATIONNUMBER], "%d", svc->current_notification_number);

		/* the $NOTIFICATIONNUMBER$ macro is maintained for backward compatability */
		mac.x[MACRO_NOTIFICATIONNUMBER] = strdup(mac.x[MACRO_SERVICENOTIFICATIONNUMBER]);

		/* set the notification id macro */
		asprintf(&mac.x[MACRO_SERVICENOTIFICATIONID], "%lu", svc->current_notification_id);

		/* notify each contact (duplicates have been removed) */
		for(temp_notification = notification_list; temp_notification != NULL; temp_notification = temp_notification->next) {

			/* grab the macro variables for this contact */
			grab_contact_macros_r(&mac, temp_notification->contact);

			/* notify this contact */
			result = notify_contact_of_service(&mac, temp_notification->contact, svc, type, not_author, not_data, options, escalated);

			/* keep track of how many contacts were notified */
			if(result == OK)
				contacts_notified++;
			}

		/* free memory allocated to the notification list */
		free_notification_list();

		/* clear out all macros we created */
		my_free(mac.x[MACRO_NOTIFICATIONNUMBER]);
		my_free(mac.x[MACRO_SERVICENOTIFICATIONNUMBER]);
		my_free(mac.x[MACRO_SERVICENOTIFICATIONID]);
		my_free(mac.x[MACRO_NOTIFICATIONCOMMENT]);
		my_free(mac.x[MACRO_NOTIFICATIONTYPE]);
		my_free(mac.x[MACRO_NOTIFICATIONAUTHOR]);
		my_free(mac.x[MACRO_NOTIFICATIONAUTHORNAME]);
		my_free(mac.x[MACRO_NOTIFICATIONAUTHORALIAS]);
		my_free(mac.x[MACRO_SERVICEACKAUTHORNAME]);
		my_free(mac.x[MACRO_SERVICEACKAUTHORALIAS]);
		my_free(mac.x[MACRO_SERVICEACKAUTHOR]);
		my_free(mac.x[MACRO_SERVICEACKCOMMENT]);

		/* this gets set in add_notification() */
		my_free(mac.x[MACRO_NOTIFICATIONRECIPIENTS]);

		/*
		 * Clear all macros, or they will linger in memory
		 * now that we're done with the notifications.
		 */
		clear_summary_macros_r(&mac);
		clear_contact_macros_r(&mac);
		clear_contactgroup_macros_r(&mac);
		clear_argv_macros_r(&mac);
		clear_host_macros_r(&mac);
		clear_hostgroup_macros_r(&mac);
		clear_service_macros_r(&mac);
		clear_servicegroup_macros_r(&mac);
		clear_datetime_macros_r(&mac);

		if(type == NOTIFICATION_NORMAL) {

			/* adjust last/next notification time and notification flags if we notified someone */
			if(contacts_notified > 0) {

				/* calculate the next acceptable re-notification time */
				svc->next_notification = get_next_service_notification_time(svc, current_time);

				log_debug_info(DEBUGL_NOTIFICATIONS, 0, "%d contacts were notified.  Next possible notification time: %s", contacts_notified, ctime(&svc->next_notification));

				/* update the last notification time for this service (this is needed for rescheduling later notifications) */
				svc->last_notification = current_time;

				/* update notifications flags */
				add_notified_on(svc, svc->current_state);
				}

			/* we didn't end up notifying anyone */
			else if(increment_notification_number == TRUE) {

				/* adjust current notification number */
				svc->current_notification_number--;

				log_debug_info(DEBUGL_NOTIFICATIONS, 0, "No contacts were notified.  Next possible notification time: %s", ctime(&svc->next_notification));
				}

			}

		log_debug_info(DEBUGL_NOTIFICATIONS, 0, "%d contacts were notified.\n", contacts_notified);
		}

	/* there were no contacts, so no notification really occurred... */
	else {

		/* readjust current notification number, since one didn't go out */
		if(increment_notification_number == TRUE)
			svc->current_notification_number--;

		log_debug_info(DEBUGL_NOTIFICATIONS, 0, "No contacts were found for notification purposes.  No notification was sent out.\n");
		}

	/* this gets set in create_notification_list_from_service() */
	my_free(mac.x[MACRO_NOTIFICATIONISESCALATED]);

	/* get the time we finished */
	gettimeofday(&end_time, NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_notification_data(NEBTYPE_NOTIFICATION_END, NEBFLAG_NONE, NEBATTR_NONE, SERVICE_NOTIFICATION, type, start_time, end_time, (void *)svc, not_author, not_data, escalated, contacts_notified, NULL);
#endif

	/* update the status log with the service information */
	update_service_status(svc, FALSE);

	return OK;
	}



/* checks the viability of sending out a service alert (top level filters) */
int check_service_notification_viability(service *svc, int type, int options) {
	host *temp_host;
	timeperiod *temp_period;
	time_t current_time;
	time_t timeperiod_start;
	time_t first_problem_time;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_service_notification_viability()\n");

	/* forced notifications bust through everything */
	if(options & NOTIFICATION_OPTION_FORCED) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This is a forced service notification, so we'll send it out.\n");
		return OK;
		}

	/* get current time */
	time(&current_time);

	/* are notifications enabled? */
	if(enable_notifications == FALSE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Notifications are disabled, so service notifications will not be sent out.\n");
		return ERROR;
		}

	/* find the host this service is associated with */
	if((temp_host = (host *)svc->host_ptr) == NULL)
		return ERROR;

	/* if we couldn't find the host, return an error */
	if(temp_host == NULL) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Couldn't find the host associated with this service, so we won't send a notification.\n");
		return ERROR;
		}

	/* if all parents are bad (usually just one), we shouldn't notify */
	if(svc->parents) {
		int bad_parents = 0, total_parents = 0;
		servicesmember *sm;
		for(sm = svc->parents; sm; sm = sm->next) {
			/* @todo: tweak this so it handles hard states and whatnot */
			if(sm->service_ptr->current_state == STATE_OK)
				bad_parents += !!sm->service_ptr->current_state;
			total_parents++;
			}
		if(bad_parents == total_parents) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This service has no good parents, so notification will be blocked.\n");
			return ERROR;
			}
		}

	/* if the service has no notification period, inherit one from the host */
	temp_period = svc->notification_period_ptr;
	if(temp_period == NULL) {
		temp_period = svc->host_ptr->notification_period_ptr;
		}

	/* see if the service can have notifications sent out at this time */
	if(check_time_against_period(current_time, temp_period) == ERROR) {

		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This service shouldn't have notifications sent out at this time.\n");

		/* calculate the next acceptable notification time, once the next valid time range arrives... */
		if(type == NOTIFICATION_NORMAL) {

			get_next_valid_time(current_time, &timeperiod_start, svc->notification_period_ptr);

			/* looks like there are no valid notification times defined, so schedule the next one far into the future (one year)... */
			if(timeperiod_start == (time_t)0)
				svc->next_notification = (time_t)(current_time + (60 * 60 * 24 * 365));

			/* else use the next valid notification time */
			else
				svc->next_notification = timeperiod_start;

			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Next possible notification time: %s\n", ctime(&svc->next_notification));
			}

		return ERROR;
		}

	/* are notifications temporarily disabled for this service? */
	if(svc->notifications_enabled == FALSE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Notifications are temporarily disabled for this service, so we won't send one out.\n");
		return ERROR;
		}


	/*********************************************/
	/*** SPECIAL CASE FOR CUSTOM NOTIFICATIONS ***/
	/*********************************************/

	/* custom notifications are good to go at this point... */
	if(type == NOTIFICATION_CUSTOM) {
		if(svc->scheduled_downtime_depth > 0 || temp_host->scheduled_downtime_depth > 0) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't send custom notification during scheduled downtime.\n");
			return ERROR;
			}
		return OK;
		}



	/*****************************************/
	/*** SPECIAL CASE FOR ACKNOWLEDGEMENTS ***/
	/*****************************************/

	/* acknowledgements only have to pass three general filters, although they have another test of their own... */
	if(type == NOTIFICATION_ACKNOWLEDGEMENT) {

		/* don't send an acknowledgement if there isn't a problem... */
		if(svc->current_state == STATE_OK) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "The service is currently OK, so we won't send an acknowledgement.\n");
			return ERROR;
			}

		/* acknowledgement viability test passed, so the notification can be sent out */
		return OK;
		}


	/****************************************/
	/*** SPECIAL CASE FOR FLAPPING ALERTS ***/
	/****************************************/

	/* flapping notifications only have to pass three general filters */
	if(type == NOTIFICATION_FLAPPINGSTART || type == NOTIFICATION_FLAPPINGSTOP || type == NOTIFICATION_FLAPPINGDISABLED) {

		/* don't send a notification if we're not supposed to... */
		if(flag_isset(svc->notification_options, OPT_FLAPPING) == FALSE) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about FLAPPING events for this service.\n");
			return ERROR;
			}

		/* don't send notifications during scheduled downtime */
		if(svc->scheduled_downtime_depth > 0 || temp_host->scheduled_downtime_depth > 0) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about FLAPPING events during scheduled downtime.\n");
			return ERROR;
			}

		/* flapping viability test passed, so the notification can be sent out */
		return OK;
		}


	/****************************************/
	/*** SPECIAL CASE FOR DOWNTIME ALERTS ***/
	/****************************************/

	/* downtime notifications only have to pass three general filters */
	if(type == NOTIFICATION_DOWNTIMESTART || type == NOTIFICATION_DOWNTIMEEND || type == NOTIFICATION_DOWNTIMECANCELLED) {

		/* don't send a notification if we're not supposed to... */
		if(flag_isset(svc->notification_options, OPT_DOWNTIME) == FALSE) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about DOWNTIME events for this service.\n");
			return ERROR;
			}

		/* don't send notifications during scheduled downtime (for service only, not host) */
		if(svc->scheduled_downtime_depth > 0) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about DOWNTIME events during scheduled downtime.\n");
			return ERROR;
			}

		/* downtime viability test passed, so the notification can be sent out */
		return OK;
		}


	/****************************************/
	/*** NORMAL NOTIFICATIONS ***************/
	/****************************************/

	/* is this a hard problem/recovery? */
	if(svc->state_type == SOFT_STATE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This service is in a soft state, so we won't send a notification out.\n");
		return ERROR;
		}

	/* has this problem already been acknowledged? */
	if(svc->problem_has_been_acknowledged == TRUE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This service problem has already been acknowledged, so we won't send a notification out.\n");
		return ERROR;
		}

	/* check service notification dependencies */
	if(check_service_dependencies(svc, NOTIFICATION_DEPENDENCY) == DEPENDENCIES_FAILED) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Service notification dependencies for this service have failed, so we won't sent a notification out.\n");
		return ERROR;
		}

	/* check host notification dependencies */
	if(check_host_dependencies(temp_host, NOTIFICATION_DEPENDENCY) == DEPENDENCIES_FAILED) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Host notification dependencies for this service have failed, so we won't sent a notification out.\n");
		return ERROR;
		}

	/* see if we should notify about problems with this service */
	if(should_notify(svc) == FALSE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about %s states for this service.\n", service_state_name(svc->current_state));
		return ERROR;
		}
	if(svc->current_state == STATE_OK && svc->notified_on == 0) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about this recovery.\n");
		return ERROR;
		}


	/* see if enough time has elapsed for first notification (Mathias Sundman) */
	/* 10/02/07 don't place restrictions on recoveries or non-normal notifications, must use last time ok (or program start) in calculation */
	/* it is reasonable to assume that if the service was never up, the program start time should be used in this calculation */
	if(type == NOTIFICATION_NORMAL && svc->current_notification_number == 0 && svc->current_state != STATE_OK) {

		first_problem_time = svc->last_time_ok > 0 ? svc->last_time_ok : program_start;

		if(current_time < first_problem_time + (time_t)(svc->first_notification_delay * interval_length)) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Not enough time has elapsed since the service changed to a non-OK state, so we should not notify about this problem yet\n");
			return ERROR;
			}
		}

	/* if this service is currently flapping, don't send the notification */
	if(svc->is_flapping == TRUE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This service is currently flapping, so we won't send notifications.\n");
		return ERROR;
		}

	/***** RECOVERY NOTIFICATIONS ARE GOOD TO GO AT THIS POINT *****/
	if(svc->current_state == STATE_OK)
		return OK;

	/* don't notify contacts about this service problem again if the notification interval is set to 0 */
	if(svc->no_more_notifications == TRUE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't re-notify contacts about this service problem.\n");
		return ERROR;
		}

	/* if the host is down or unreachable, don't notify contacts about service failures */
	if(temp_host->current_state != STATE_UP && temp_host->state_type == HARD_STATE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "The host is either down or unreachable, so we won't notify contacts about this service.\n");
		return ERROR;
		}

	/* don't notify if we haven't waited long enough since the last time (and the service is not marked as being volatile) */
	if((current_time < svc->next_notification) && svc->is_volatile == FALSE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We haven't waited long enough to re-notify contacts about this service.\n");
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Next valid notification time: %s", ctime(&svc->next_notification));
		return ERROR;
		}

	/* if this service is currently in a scheduled downtime period, don't send the notification */
	if(svc->scheduled_downtime_depth > 0) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This service is currently in a scheduled downtime, so we won't send notifications.\n");
		return ERROR;
		}

	/* if this host is currently in a scheduled downtime period, don't send the notification */
	if(temp_host->scheduled_downtime_depth > 0) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "The host this service is associated with is currently in a scheduled downtime, so we won't send notifications.\n");
		return ERROR;
		}

	/* if this service is currently in a flex downtime period, don't send the notification */
	if(svc->pending_flex_downtime > 0 && is_service_in_pending_flex_downtime(svc) == TRUE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This service is starting a flex downtime, so we won't send notifications.\n");
		return ERROR;
		}

	/* if this host is currently in a flex downtime period, don't send the notification */
	if(temp_host->pending_flex_downtime > 0 && is_host_in_pending_flex_downtime(temp_host) == TRUE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "The host this service is associated with is starting a flex downtime, so we won't send notifications.\n");
		return ERROR;
		}

	return OK;
	}



/* check viability of sending out a service notification to a specific contact (contact-specific filters) */
int check_contact_service_notification_viability(contact *cntct, service *svc, int type, int options) {

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_contact_service_notification_viability()\n");

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "** Checking service notification viability for contact '%s'...\n", cntct->name);

	/* forced notifications bust through everything */
	if(options & NOTIFICATION_OPTION_FORCED) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This is a forced service notification, so we'll send it out to this contact.\n");
		return OK;
		}

	/* is this service not important enough? */
	if(cntct->minimum_value > svc->hourly_value) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Contact's minimum_importance is higher than service's importance. Notification will be blocked\n");
		return ERROR;
		}

	/* are notifications enabled? */
	if(cntct->service_notifications_enabled == FALSE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Service notifications are disabled for this contact.\n");
		return ERROR;
		}

	/* see if the contact can be notified at this time */
	if(check_time_against_period(time(NULL), cntct->service_notification_period_ptr) == ERROR) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "This contact shouldn't be notified at this time.\n");
		return ERROR;
		}

	/*********************************************/
	/*** SPECIAL CASE FOR CUSTOM NOTIFICATIONS ***/
	/*********************************************/

	/* custom notifications are good to go at this point... */
	if(type == NOTIFICATION_CUSTOM)
		return OK;


	/****************************************/
	/*** SPECIAL CASE FOR FLAPPING ALERTS ***/
	/****************************************/

	if(type == NOTIFICATION_FLAPPINGSTART || type == NOTIFICATION_FLAPPINGSTOP || type == NOTIFICATION_FLAPPINGDISABLED) {

		if((cntct->service_notification_options & OPT_FLAPPING) == FALSE) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "We shouldn't notify this contact about FLAPPING service events.\n");
			return ERROR;
			}

		return OK;
		}

	/****************************************/
	/*** SPECIAL CASE FOR DOWNTIME ALERTS ***/
	/****************************************/

	if(type == NOTIFICATION_DOWNTIMESTART || type == NOTIFICATION_DOWNTIMEEND || type == NOTIFICATION_DOWNTIMECANCELLED) {

		if((cntct->service_notification_options & OPT_DOWNTIME) == FALSE) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "We shouldn't notify this contact about DOWNTIME service events.\n");
			return ERROR;
			}

		return OK;
		}

	/*************************************/
	/*** ACKS AND NORMAL NOTIFICATIONS ***/
	/*************************************/

	/* see if we should notify about problems with this service */
	if(!(cntct->service_notification_options & (1 << svc->current_state))) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "We shouldn't notify this contact about %s service states.\n", service_state_name(svc->current_state));
		return ERROR;
		}

	if(svc->current_state == STATE_OK) {

		if((cntct->service_notification_options & OPT_RECOVERY) == FALSE) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "We shouldn't notify this contact about RECOVERY service states.\n");
			return ERROR;
			}

		if(!(svc->notified_on & cntct->service_notification_options)) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "We shouldn't notify about this recovery.\n");
			return ERROR;
			}
		}

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "** Service notification viability for contact '%s' PASSED.\n", cntct->name);

	return OK;
	}



/* notify a specific contact about a service problem or recovery */
int notify_contact_of_service(nagios_macros *mac, contact *cntct, service *svc, int type, char *not_author, char *not_data, int options, int escalated) {
	commandsmember *temp_commandsmember = NULL;
	char *command_name = NULL;
	char *command_name_ptr = NULL;
	char *raw_command = NULL;
	char *processed_command = NULL;
	char *temp_buffer = NULL;
	char *processed_buffer = NULL;
	struct timeval start_time, end_time;
	struct timeval method_start_time, method_end_time;
	int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;
	int neb_result;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "notify_contact_of_service()\n");

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "** Notifying contact '%s'\n", cntct->name);

	/* get start time */
	gettimeofday(&start_time, NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec = 0L;
	end_time.tv_usec = 0L;
	neb_result = broker_contact_notification_data(NEBTYPE_CONTACTNOTIFICATION_START, NEBFLAG_NONE, NEBATTR_NONE, SERVICE_NOTIFICATION, type, start_time, end_time, (void *)svc, cntct, not_author, not_data, escalated, NULL);
	if(NEBERROR_CALLBACKCANCEL == neb_result)
		return ERROR;
	else if(NEBERROR_CALLBACKOVERRIDE == neb_result)
		return OK;
#endif

	/* process all the notification commands this user has */
	for(temp_commandsmember = cntct->service_notification_commands; temp_commandsmember != NULL; temp_commandsmember = temp_commandsmember->next) {

		/* get start time */
		gettimeofday(&method_start_time, NULL);

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		method_end_time.tv_sec = 0L;
		method_end_time.tv_usec = 0L;
		neb_result = broker_contact_notification_method_data(NEBTYPE_CONTACTNOTIFICATIONMETHOD_START, NEBFLAG_NONE, NEBATTR_NONE, SERVICE_NOTIFICATION, type, method_start_time, method_end_time, (void *)svc, cntct, temp_commandsmember->command, not_author, not_data, escalated, NULL);
		if(NEBERROR_CALLBACKCANCEL == neb_result)
			break ;
		else if(NEBERROR_CALLBACKOVERRIDE == neb_result)
			continue ;
#endif

		/* get the raw command line */
		get_raw_command_line_r(mac, temp_commandsmember->command_ptr, temp_commandsmember->command, &raw_command, macro_options);
		if(raw_command == NULL)
			continue;

		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Raw notification command: %s\n", raw_command);

		/* process any macros contained in the argument */
		process_macros_r(mac, raw_command, &processed_command, macro_options);
		my_free(raw_command);
		if(processed_command == NULL)
			continue;

		/* get the command name */
		command_name = (char *)strdup(temp_commandsmember->command);
		command_name_ptr = strtok(command_name, "!");

		/* run the notification command... */

		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Processed notification command: %s\n", processed_command);

		/* log the notification to program log file */
		if(log_notifications == TRUE) {
			switch(type) {
				case NOTIFICATION_CUSTOM:
					asprintf(&temp_buffer, "SERVICE NOTIFICATION: %s;%s;%s;CUSTOM ($SERVICESTATE$);%s;$SERVICEOUTPUT$;$NOTIFICATIONAUTHOR$;$NOTIFICATIONCOMMENT$\n", cntct->name, svc->host_name, svc->description, command_name_ptr);
					break;
				case NOTIFICATION_ACKNOWLEDGEMENT:
					asprintf(&temp_buffer, "SERVICE NOTIFICATION: %s;%s;%s;ACKNOWLEDGEMENT ($SERVICESTATE$);%s;$SERVICEOUTPUT$;$NOTIFICATIONAUTHOR$;$NOTIFICATIONCOMMENT$\n", cntct->name, svc->host_name, svc->description, command_name_ptr);
					break;
				case NOTIFICATION_FLAPPINGSTART:
					asprintf(&temp_buffer, "SERVICE NOTIFICATION: %s;%s;%s;FLAPPINGSTART ($SERVICESTATE$);%s;$SERVICEOUTPUT$\n", cntct->name, svc->host_name, svc->description, command_name_ptr);
					break;
				case NOTIFICATION_FLAPPINGSTOP:
					asprintf(&temp_buffer, "SERVICE NOTIFICATION: %s;%s;%s;FLAPPINGSTOP ($SERVICESTATE$);%s;$SERVICEOUTPUT$\n", cntct->name, svc->host_name, svc->description, command_name_ptr);
					break;
				case NOTIFICATION_FLAPPINGDISABLED:
					asprintf(&temp_buffer, "SERVICE NOTIFICATION: %s;%s;%s;FLAPPINGDISABLED ($SERVICESTATE$);%s;$SERVICEOUTPUT$\n", cntct->name, svc->host_name, svc->description, command_name_ptr);
					break;
				case NOTIFICATION_DOWNTIMESTART:
					asprintf(&temp_buffer, "SERVICE NOTIFICATION: %s;%s;%s;DOWNTIMESTART ($SERVICESTATE$);%s;$SERVICEOUTPUT$\n", cntct->name, svc->host_name, svc->description, command_name_ptr);
					break;
				case NOTIFICATION_DOWNTIMEEND:
					asprintf(&temp_buffer, "SERVICE NOTIFICATION: %s;%s;%s;DOWNTIMEEND ($SERVICESTATE$);%s;$SERVICEOUTPUT$\n", cntct->name, svc->host_name, svc->description, command_name_ptr);
					break;
				case NOTIFICATION_DOWNTIMECANCELLED:
					asprintf(&temp_buffer, "SERVICE NOTIFICATION: %s;%s;%s;DOWNTIMECANCELLED ($SERVICESTATE$);%s;$SERVICEOUTPUT$\n", cntct->name, svc->host_name, svc->description, command_name_ptr);
					break;
				default:
					asprintf(&temp_buffer, "SERVICE NOTIFICATION: %s;%s;%s;$SERVICESTATE$;%s;$SERVICEOUTPUT$\n", cntct->name, svc->host_name, svc->description, command_name_ptr);
					break;
				}

			process_macros_r(mac, temp_buffer, &processed_buffer, 0);
			write_to_all_logs(processed_buffer, NSLOG_SERVICE_NOTIFICATION);

			my_free(temp_buffer);
			my_free(processed_buffer);
			}

		/* run the notification command */
		wproc_notify(cntct->name, svc->host_name, svc->description, processed_command, mac);

		/* free memory */
		my_free(command_name);
		my_free(processed_command);

		/* get end time */
		gettimeofday(&method_end_time, NULL);

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_contact_notification_method_data(NEBTYPE_CONTACTNOTIFICATIONMETHOD_END, NEBFLAG_NONE, NEBATTR_NONE, SERVICE_NOTIFICATION, type, method_start_time, method_end_time, (void *)svc, cntct, temp_commandsmember->command, not_author, not_data, escalated, NULL);
#endif
		}

	/* get end time */
	gettimeofday(&end_time, NULL);

	/* update the contact's last service notification time */
	cntct->last_service_notification = start_time.tv_sec;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_contact_notification_data(NEBTYPE_CONTACTNOTIFICATION_END, NEBFLAG_NONE, NEBATTR_NONE, SERVICE_NOTIFICATION, type, start_time, end_time, (void *)svc, cntct, not_author, not_data, escalated, NULL);
#endif

	return OK;
	}


/* checks to see if a service escalation entry is a match for the current service notification */
int is_valid_escalation_for_service_notification(service *svc, serviceescalation *se, int options) {
	int notification_number = 0;
	time_t current_time = 0L;
	service *temp_service = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "is_valid_escalation_for_service_notification()\n");

	/* get the current time */
	time(&current_time);

	/* if this is a recovery, really we check for who got notified about a previous problem */
	if(svc->current_state == STATE_OK)
		notification_number = svc->current_notification_number - 1;
	else
		notification_number = svc->current_notification_number;

	/* this entry if it is not for this service */
	temp_service = se->service_ptr;
	if(temp_service == NULL || temp_service != svc)
		return FALSE;

	/*** EXCEPTION ***/
	/* broadcast options go to everyone, so this escalation is valid */
	if(options & NOTIFICATION_OPTION_BROADCAST)
		return TRUE;

	/* skip this escalation if it happens later */
	if(se->first_notification > notification_number)
		return FALSE;

	/* skip this escalation if it has already passed */
	if(se->last_notification != 0 && se->last_notification < notification_number)
		return FALSE;

	/* skip this escalation if the state options don't match */
	if(flag_isset(se->escalation_options, 1 << svc->current_state) == FALSE)
		return FALSE;

	/* skip this escalation if it has a timeperiod and the current time isn't valid */
	if(se->escalation_period != NULL && check_time_against_period(current_time, se->escalation_period_ptr) == ERROR)
		return FALSE;

	return TRUE;
	}


/* checks to see whether a service notification should be escalation */
int should_service_notification_be_escalated(service *svc) {
	objectlist *list;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "should_service_notification_be_escalated()\n");

	/* search the service escalation list */
	for (list = svc->escalation_list; list; list = list->next) {
		serviceescalation *temp_se = (serviceescalation *)list->object_ptr;

		/* we found a matching entry, so escalate this notification! */
		if(is_valid_escalation_for_service_notification(svc, temp_se, NOTIFICATION_OPTION_NONE) == TRUE) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Service notification WILL be escalated.\n");
			return TRUE;
			}
		}

	log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Service notification will NOT be escalated.\n");

	return FALSE;
	}


/* given a service, create a list of contacts to be notified, removing duplicates, checking contact notification viability */
int create_notification_list_from_service(nagios_macros *mac, service *svc, int options, int *escalated, int type) {
	serviceescalation *temp_se = NULL;
	contactsmember *temp_contactsmember = NULL;
	contact *temp_contact = NULL;
	contactgroupsmember *temp_contactgroupsmember = NULL;
	contactgroup *temp_contactgroup = NULL;
	int escalate_notification = FALSE;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "create_notification_list_from_service()\n");

	/* see if this notification should be escalated */
	escalate_notification = should_service_notification_be_escalated(svc);

	/* set the escalation flag */
	*escalated = escalate_notification;

	/* make sure there aren't any leftover contacts */
	free_notification_list();

	/* set the escalation macro */
	mac->x[MACRO_NOTIFICATIONISESCALATED] = strdup(escalate_notification ? "1" : "0");

	if(options & NOTIFICATION_OPTION_BROADCAST)
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This notification will be BROADCAST to all (escalated and normal) contacts...\n");

	/* use escalated contacts for this notification */
	if(escalate_notification == TRUE || (options & NOTIFICATION_OPTION_BROADCAST)) {
		objectlist *list;

		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Adding contacts from service escalation(s) to notification list.\n");

		/* search all the escalation entries for valid matches */
		for(list = svc->escalation_list; list; list = list->next) {
			temp_se = (serviceescalation *)list->object_ptr;

			/* skip this entry if it isn't appropriate */
			if(is_valid_escalation_for_service_notification(svc, temp_se, options) == FALSE)
				continue;

			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Adding individual contacts from service escalation(s) to notification list.\n");

			/* add all individual contacts for this escalation entry */
			for(temp_contactsmember = temp_se->contacts; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
				if((temp_contact = temp_contactsmember->contact_ptr) == NULL)
					continue;
				/* check now if the contact can be notified */
				if (check_contact_service_notification_viability(temp_contact, svc, type, options) == OK)
					add_notification(mac, temp_contact);
				else
					log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Not adding contact '%s'\n",temp_contact->name);
				}

			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Adding members of contact groups from service escalation(s) to notification list.\n");

			/* add all contacts that belong to contactgroups for this escalation */
			for(temp_contactgroupsmember = temp_se->contact_groups; temp_contactgroupsmember != NULL; temp_contactgroupsmember = temp_contactgroupsmember->next) {
				log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Adding members of contact group '%s' for service escalation to notification list.\n", temp_contactgroupsmember->group_name);
				if((temp_contactgroup = temp_contactgroupsmember->group_ptr) == NULL)
					continue;
				for(temp_contactsmember = temp_contactgroup->members; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
					if((temp_contact = temp_contactsmember->contact_ptr) == NULL)
						continue;
					/* check now if the contact can be notified */
					if (check_contact_service_notification_viability(temp_contact, svc, type, options) == OK)
						add_notification(mac, temp_contact);
					else
						log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Not adding contact '%s'\n",temp_contact->name);
					}
				}
			}
		}

	/* else use normal, non-escalated contacts */
	if(escalate_notification == FALSE || (options & NOTIFICATION_OPTION_BROADCAST)) {

		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Adding normal contacts for service to notification list.\n");

		/* add all individual contacts for this service */
		for(temp_contactsmember = svc->contacts; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
			if((temp_contact = temp_contactsmember->contact_ptr) == NULL)
				continue;
			/* check now if the contact can be notified */
                        if (check_contact_service_notification_viability(temp_contact, svc, type, options) == OK)
                                add_notification(mac, temp_contact);
                        else
                                log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Not adding contact '%s'\n",temp_contact->name);
			}

		/* add all contacts that belong to contactgroups for this service */
		for(temp_contactgroupsmember = svc->contact_groups; temp_contactgroupsmember != NULL; temp_contactgroupsmember = temp_contactgroupsmember->next) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Adding members of contact group '%s' for service to notification list.\n", temp_contactgroupsmember->group_name);
			if((temp_contactgroup = temp_contactgroupsmember->group_ptr) == NULL)
				continue;
			for(temp_contactsmember = temp_contactgroup->members; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
				if((temp_contact = temp_contactsmember->contact_ptr) == NULL)
					continue;
	                        /* check now if the contact can be notified */
	                        if (check_contact_service_notification_viability(temp_contact, svc, type, options) == OK)
                                	add_notification(mac, temp_contact);
	                        else
	                                log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Not adding contact '%s'\n",temp_contact->name);
				}
			}
		}

	return OK;
	}




/******************************************************************/
/******************* HOST NOTIFICATION FUNCTIONS ******************/
/******************************************************************/



/* notify all contacts for a host that the entire host is down or up */
int host_notification(host *hst, int type, char *not_author, char *not_data, int options) {
	notification *temp_notification = NULL;
	contact *temp_contact = NULL;
	time_t current_time;
	struct timeval start_time;
	struct timeval end_time;
	int escalated = FALSE;
	int result = OK;
	int contacts_notified = 0;
	int increment_notification_number = FALSE;
	nagios_macros mac;
	int neb_result;

	/* get the current time */
	time(&current_time);
	gettimeofday(&start_time, NULL);

	log_debug_info(DEBUGL_NOTIFICATIONS, 0, "** Host Notification Attempt ** Host: '%s', Type: %s, Options: %d, Current State: %d, Last Notification: %s", hst->name, notification_reason_name(type), options, hst->current_state, ctime(&hst->last_notification));


	/* check viability of sending out a host notification */
	if(check_host_notification_viability(hst, type, options) == ERROR) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 0, "Notification viability test failed.  No notification will be sent out.\n");
		return OK;
		}

	log_debug_info(DEBUGL_NOTIFICATIONS, 0, "Notification viability test passed.\n");

	/* should the notification number be increased? */
	if(type == NOTIFICATION_NORMAL || (options & NOTIFICATION_OPTION_INCREMENT)) {
		hst->current_notification_number++;
		increment_notification_number = TRUE;
		}

	log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Current notification number: %d (%s)\n", hst->current_notification_number, (increment_notification_number == TRUE) ? "incremented" : "unchanged");

	/* save and increase the current notification id */
	hst->current_notification_id = next_notification_id;
	next_notification_id++;

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Creating list of contacts to be notified.\n");

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec = 0L;
	end_time.tv_usec = 0L;
	neb_result = broker_notification_data(NEBTYPE_NOTIFICATION_START, NEBFLAG_NONE, NEBATTR_NONE, HOST_NOTIFICATION, type, start_time, end_time, (void *)hst, not_author, not_data, escalated, 0, NULL);
	if(neb_result == NEBERROR_CALLBACKCANCEL || neb_result == NEBERROR_CALLBACKOVERRIDE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 0, "Host notification to %s (id=%u) was blocked by a module.\n", hst->name, hst->id);
		free_notification_list();
		return neb_result == NEBERROR_CALLBACKOVERRIDE ? OK : ERROR;
		}
#endif

	/* reset memory for local macro data */
	memset(&mac, 0, sizeof(mac));

	/*
	 * check viability before adding a contact to the notification
	 * list and build up the $NOTIFICATIONRECIPIENTS$ macro while
	 * we're at it.
	 * This prevents us from running through all the steps again in
	 * notify_contact_of_host|service.
	 * Furthermore the $NOTIFICATIONRECIPIENTS$ macro will contain
	 * only actual recipients (as the name implies), and not all
	 * contacts assigned to that host|service.
	 *
	 * note: checks against timeperiod will happen now(),
	 * and not when the notification is actually being sent.
	 */
	create_notification_list_from_host(&mac, hst, options, &escalated, type);

	/* there are contacts to be notified... */
	if(notification_list != NULL) {

		/* grab the macro variables */
		grab_host_macros_r(&mac, hst);

		/* if this notification has an author, attempt to lookup the associated contact */
		if(not_author != NULL) {
			temp_contact = find_contact_by_name_or_alias(not_author);
			}

		/* get author and comment macros */
		if(not_author)
			mac.x[MACRO_NOTIFICATIONAUTHOR] = strdup(not_author);
		if(temp_contact != NULL) {
			mac.x[MACRO_NOTIFICATIONAUTHORNAME] = strdup(temp_contact->name);
			mac.x[MACRO_NOTIFICATIONAUTHORALIAS] = strdup(temp_contact->alias);
			}
		if(not_data)
			mac.x[MACRO_NOTIFICATIONCOMMENT] = strdup(not_data);

		/* NOTE: these macros are deprecated and will likely disappear in Nagios 4.x */
		/* if this is an acknowledgement, get author and comment macros */
		if(type == NOTIFICATION_ACKNOWLEDGEMENT) {

			if(not_author)
				mac.x[MACRO_HOSTACKAUTHOR] = strdup(not_author);

			if(not_data)
				mac.x[MACRO_HOSTACKCOMMENT] = strdup(not_data);

			if(temp_contact != NULL) {
				mac.x[MACRO_HOSTACKAUTHORNAME] = strdup(temp_contact->name);
				mac.x[MACRO_HOSTACKAUTHORALIAS] = strdup(temp_contact->alias);
				}
			}

		/* set the notification type macro */
		if(type == NOTIFICATION_ACKNOWLEDGEMENT)
			mac.x[MACRO_NOTIFICATIONTYPE] = "ACKNOWLEDGEMENT";
		else if(type == NOTIFICATION_FLAPPINGSTART)
			mac.x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGSTART";
		else if(type == NOTIFICATION_FLAPPINGSTOP)
			mac.x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGSTOP";
		else if(type == NOTIFICATION_FLAPPINGDISABLED)
			mac.x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGDISABLED";
		else if(type == NOTIFICATION_DOWNTIMESTART)
			mac.x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMESTART";
		else if(type == NOTIFICATION_DOWNTIMEEND)
			mac.x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMEEND";
		else if(type == NOTIFICATION_DOWNTIMECANCELLED)
			mac.x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMECANCELLED";
		else if(type == NOTIFICATION_CUSTOM)
			mac.x[MACRO_NOTIFICATIONTYPE] = "CUSTOM";
		else if(hst->current_state == HOST_UP)
			mac.x[MACRO_NOTIFICATIONTYPE] = "RECOVERY";
		else
			mac.x[MACRO_NOTIFICATIONTYPE] = "PROBLEM";

		mac.x[MACRO_NOTIFICATIONTYPE] = strdup(mac.x[MACRO_NOTIFICATIONTYPE]);

		/* set the notification number macro */
		asprintf(&mac.x[MACRO_HOSTNOTIFICATIONNUMBER], "%d", hst->current_notification_number);

		/* the $NOTIFICATIONNUMBER$ macro is maintained for backward compatability */
		mac.x[MACRO_NOTIFICATIONNUMBER] = strdup(mac.x[MACRO_HOSTNOTIFICATIONNUMBER]);

		/* set the notification id macro */
		asprintf(&mac.x[MACRO_HOSTNOTIFICATIONID], "%lu", hst->current_notification_id);

		/* notify each contact (duplicates have been removed) */
		for(temp_notification = notification_list; temp_notification != NULL; temp_notification = temp_notification->next) {

			/* grab the macro variables for this contact */
			grab_contact_macros_r(&mac, temp_notification->contact);

			/* clear summary macros (they are customized for each contact) */
			clear_summary_macros_r(&mac);

			/* notify this contact */
			result = notify_contact_of_host(&mac, temp_notification->contact, hst, type, not_author, not_data, options, escalated);

			/* keep track of how many contacts were notified */
			if(result == OK)
				contacts_notified++;
			}

		/* free memory allocated to the notification list */
		free_notification_list();

		/* clear out all macros we created */
		my_free(mac.x[MACRO_HOSTNOTIFICATIONID]);
		my_free(mac.x[MACRO_NOTIFICATIONNUMBER]);
		my_free(mac.x[MACRO_NOTIFICATIONCOMMENT]);
		my_free(mac.x[MACRO_HOSTNOTIFICATIONNUMBER]);
		my_free(mac.x[MACRO_NOTIFICATIONTYPE]);
		my_free(mac.x[MACRO_NOTIFICATIONAUTHOR]);
		my_free(mac.x[MACRO_NOTIFICATIONAUTHORNAME]);
		my_free(mac.x[MACRO_NOTIFICATIONAUTHORALIAS]);
		my_free(mac.x[MACRO_HOSTACKAUTHORNAME]);
		my_free(mac.x[MACRO_HOSTACKAUTHORALIAS]);
		my_free(mac.x[MACRO_HOSTACKAUTHOR]);
		my_free(mac.x[MACRO_HOSTACKCOMMENT]);
		/* this gets set in add_notification() */
		my_free(mac.x[MACRO_NOTIFICATIONRECIPIENTS]);

		/*
		 * Clear all macros, or they will linger in memory
		 * now that we're done with the notifications.
		 */
		clear_summary_macros_r(&mac);
		clear_contact_macros_r(&mac);
		clear_contactgroup_macros_r(&mac);
		clear_argv_macros_r(&mac);
		clear_host_macros_r(&mac);
		clear_hostgroup_macros_r(&mac);
		clear_datetime_macros_r(&mac);

		if(type == NOTIFICATION_NORMAL) {

			/* adjust last/next notification time and notification flags if we notified someone */
			if(contacts_notified > 0) {

				/* calculate the next acceptable re-notification time */
				hst->next_notification = get_next_host_notification_time(hst, current_time);

				/* update the last notification time for this host (this is needed for scheduling the next problem notification) */
				hst->last_notification = current_time;

				/* update notifications flags */
				add_notified_on(hst, hst->current_state);

				log_debug_info(DEBUGL_NOTIFICATIONS, 0, "%d contacts were notified.  Next possible notification time: %s", contacts_notified, ctime(&hst->next_notification));
				}

			/* we didn't end up notifying anyone */
			else if(increment_notification_number == TRUE) {

				/* adjust current notification number */
				hst->current_notification_number--;

				log_debug_info(DEBUGL_NOTIFICATIONS, 0, "No contacts were notified.  Next possible notification time: %s", ctime(&hst->next_notification));
				}
			}

		log_debug_info(DEBUGL_NOTIFICATIONS, 0, "%d contacts were notified.\n", contacts_notified);
		}

	/* there were no contacts, so no notification really occurred... */
	else {

		/* adjust notification number, since no notification actually went out */
		if(increment_notification_number == TRUE)
			hst->current_notification_number--;

		log_debug_info(DEBUGL_NOTIFICATIONS, 0, "No contacts were found for notification purposes.  No notification was sent out.\n");
		}

	/* this gets set in create_notification_list_from_host() */
	my_free(mac.x[MACRO_NOTIFICATIONISESCALATED]);

	/* get the time we finished */
	gettimeofday(&end_time, NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_notification_data(NEBTYPE_NOTIFICATION_END, NEBFLAG_NONE, NEBATTR_NONE, HOST_NOTIFICATION, type, start_time, end_time, (void *)hst, not_author, not_data, escalated, contacts_notified, NULL);
#endif

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	return OK;
	}



/* checks viability of sending a host notification */
int check_host_notification_viability(host *hst, int type, int options) {
	time_t current_time;
	time_t timeperiod_start;
	time_t first_problem_time;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_host_notification_viability()\n");

	/* forced notifications bust through everything */
	if(options & NOTIFICATION_OPTION_FORCED) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This is a forced host notification, so we'll send it out.\n");
		return OK;
		}

	/* get current time */
	time(&current_time);

	/* are notifications enabled? */
	if(enable_notifications == FALSE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Notifications are disabled, so host notifications will not be sent out.\n");
		return ERROR;
		}

	/* see if the host can have notifications sent out at this time */
	if(check_time_against_period(current_time, hst->notification_period_ptr) == ERROR) {

		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This host shouldn't have notifications sent out at this time.\n");

		/* if this is a normal notification, calculate the next acceptable notification time, once the next valid time range arrives... */
		if(type == NOTIFICATION_NORMAL) {

			get_next_valid_time(current_time, &timeperiod_start, hst->notification_period_ptr);

			/* it looks like there is no notification time defined, so schedule next one far into the future (one year)... */
			if(timeperiod_start == (time_t)0)
				hst->next_notification = (time_t)(current_time + (60 * 60 * 24 * 365));

			/* else use the next valid notification time */
			else
				hst->next_notification = timeperiod_start;

			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Next possible notification time: %s\n", ctime(&hst->next_notification));
			}

		return ERROR;
		}

	/* are notifications temporarily disabled for this host? */
	if(hst->notifications_enabled == FALSE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Notifications are temporarily disabled for this host, so we won't send one out.\n");
		return ERROR;
		}


	/*********************************************/
	/*** SPECIAL CASE FOR CUSTOM NOTIFICATIONS ***/
	/*********************************************/

	/* custom notifications are good to go at this point... */
	if(type == NOTIFICATION_CUSTOM) {
		if(hst->scheduled_downtime_depth > 0) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't send custom notification during scheduled downtime.\n");
			return ERROR;
			}
		return OK;
		}



	/*****************************************/
	/*** SPECIAL CASE FOR ACKNOWLEDGEMENTS ***/
	/*****************************************/

	/* acknowledgements only have to pass three general filters, although they have another test of their own... */
	if(type == NOTIFICATION_ACKNOWLEDGEMENT) {

		/* don't send an acknowledgement if there isn't a problem... */
		if(hst->current_state == HOST_UP) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "The host is currently UP, so we won't send an acknowledgement.\n");
			return ERROR;
			}

		/* acknowledgement viability test passed, so the notification can be sent out */
		return OK;
		}


	/*****************************************/
	/*** SPECIAL CASE FOR FLAPPING ALERTS ***/
	/*****************************************/

	/* flapping notifications only have to pass three general filters */
	if(type == NOTIFICATION_FLAPPINGSTART || type == NOTIFICATION_FLAPPINGSTOP || type == NOTIFICATION_FLAPPINGDISABLED) {

		/* don't send a notification if we're not supposed to... */
		if(!(hst->notification_options & OPT_FLAPPING)) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about FLAPPING events for this host.\n");
			return ERROR;
			}

		/* don't send notifications during scheduled downtime */
		if(hst->scheduled_downtime_depth > 0) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about FLAPPING events during scheduled downtime.\n");
			return ERROR;
			}

		/* flapping viability test passed, so the notification can be sent out */
		return OK;
		}


	/*****************************************/
	/*** SPECIAL CASE FOR DOWNTIME ALERTS ***/
	/*****************************************/

	/* flapping notifications only have to pass three general filters */
	if(type == NOTIFICATION_DOWNTIMESTART || type == NOTIFICATION_DOWNTIMEEND || type == NOTIFICATION_DOWNTIMECANCELLED) {

		/* don't send a notification if we're not supposed to... */
		if((hst->notification_options & OPT_DOWNTIME) == FALSE) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about DOWNTIME events for this host.\n");
			return ERROR;
			}

		/* don't send notifications during scheduled downtime */
		if(hst->scheduled_downtime_depth > 0) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about DOWNTIME events during scheduled downtime!\n");
			return ERROR;
			}

		/* downtime viability test passed, so the notification can be sent out */
		return OK;
		}


	/****************************************/
	/*** NORMAL NOTIFICATIONS ***************/
	/****************************************/

	/* is this a hard problem/recovery? */
	if(hst->state_type == SOFT_STATE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This host is in a soft state, so we won't send a notification out.\n");
		return ERROR;
		}

	/* has this problem already been acknowledged? */
	if(hst->problem_has_been_acknowledged == TRUE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This host problem has already been acknowledged, so we won't send a notification out!\n");
		return ERROR;
		}

	/* check notification dependencies */
	if(check_host_dependencies(hst, NOTIFICATION_DEPENDENCY) == DEPENDENCIES_FAILED) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Notification dependencies for this host have failed, so we won't sent a notification out!\n");
		return ERROR;
		}

	/* see if we should notify about problems with this host */
	if((hst->notification_options & (1 << hst->current_state)) == FALSE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about %s status for this host.\n", host_state_name(hst->current_state));
		return ERROR;
		}
	if(hst->current_state == HOST_UP) {

		if((hst->notification_options & OPT_RECOVERY) == FALSE) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about RECOVERY states for this host.\n");
			return ERROR;
			}
		if(!hst->notified_on) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't notify about this recovery.\n");
			return ERROR;
			}

		}

	/* see if enough time has elapsed for first notification (Mathias Sundman) */
	/* 10/02/07 don't place restrictions on recoveries or non-normal notifications, must use last time up (or program start) in calculation */
	/* it is reasonable to assume that if the host was never up, the program start time should be used in this calculation */
	if(type == NOTIFICATION_NORMAL && hst->current_notification_number == 0 && hst->current_state != HOST_UP) {

		first_problem_time = hst->last_time_up > 0 ? hst->last_time_up : program_start;

		if(current_time < first_problem_time + (time_t)(hst->first_notification_delay * interval_length)) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Not enough time has elapsed since the host changed to a non-UP state (or since program start), so we shouldn't notify about this problem yet.\n");
			return ERROR;
			}
		}

	/* if this host is currently flapping, don't send the notification */
	if(hst->is_flapping == TRUE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This host is currently flapping, so we won't send notifications.\n");
		return ERROR;
		}

	/***** RECOVERY NOTIFICATIONS ARE GOOD TO GO AT THIS POINT *****/
	if(hst->current_state == HOST_UP)
		return OK;

	/* if this host is currently in a scheduled downtime period, don't send the notification */
	if(hst->scheduled_downtime_depth > 0) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This host is currently in a scheduled downtime, so we won't send notifications.\n");
		return ERROR;
		}

	/* check if we shouldn't renotify contacts about the host problem */
	if(hst->no_more_notifications == TRUE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "We shouldn't re-notify contacts about this host problem.\n");
		return ERROR;
		}

	/* check if its time to re-notify the contacts about the host... */
	if(current_time < hst->next_notification) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Its not yet time to re-notify the contacts about this host problem...\n");
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Next acceptable notification time: %s", ctime(&hst->next_notification));
		return ERROR;
		}

	return OK;
	}



/* checks the viability of notifying a specific contact about a host */
int check_contact_host_notification_viability(contact *cntct, host *hst, int type, int options) {

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_contact_host_notification_viability()\n");

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "** Checking host notification viability for contact '%s'...\n", cntct->name);

	/* forced notifications bust through everything */
	if(options & NOTIFICATION_OPTION_FORCED) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "This is a forced host notification, so we'll send it out for this contact.\n");
		return OK;
		}

	/* are notifications enabled? */
	if(cntct->host_notifications_enabled == FALSE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Host notifications are disabled for this contact.\n");
		return ERROR;
		}

	/* is this host important enough? */
	if(cntct->minimum_value > hst->hourly_value + host_services_value(hst)) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Contact's minimum_importance is greater than the importance of the host and all its services. Notification will be blocked\n");
		return ERROR;
		}

	/* see if the contact can be notified at this time */
	if(check_time_against_period(time(NULL), cntct->host_notification_period_ptr) == ERROR) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "This contact shouldn't be notified at this time.\n");
		return ERROR;
		}


	/*********************************************/
	/*** SPECIAL CASE FOR CUSTOM NOTIFICATIONS ***/
	/*********************************************/

	/* custom notifications are good to go at this point... */
	if(type == NOTIFICATION_CUSTOM)
		return OK;


	/****************************************/
	/*** SPECIAL CASE FOR FLAPPING ALERTS ***/
	/****************************************/

	if(type == NOTIFICATION_FLAPPINGSTART || type == NOTIFICATION_FLAPPINGSTOP || type == NOTIFICATION_FLAPPINGDISABLED) {

		if((cntct->host_notification_options & OPT_FLAPPING) == FALSE) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "We shouldn't notify this contact about FLAPPING host events.\n");
			return ERROR;
			}

		return OK;
		}


	/****************************************/
	/*** SPECIAL CASE FOR DOWNTIME ALERTS ***/
	/****************************************/

	if(type == NOTIFICATION_DOWNTIMESTART || type == NOTIFICATION_DOWNTIMEEND || type == NOTIFICATION_DOWNTIMECANCELLED) {

		if(flag_isset(cntct->host_notification_options, OPT_DOWNTIME) == FALSE) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "We shouldn't notify this contact about DOWNTIME host events.\n");
			return ERROR;
			}

		return OK;
		}


	/*************************************/
	/*** ACKS AND NORMAL NOTIFICATIONS ***/
	/*************************************/

	/* see if we should notify about problems with this host */
	if(flag_isset(cntct->host_notification_options, 1 << hst->current_state) == FALSE) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "We shouldn't notify this contact about %s states.\n", host_state_name(hst->current_state));
		return ERROR;
		}

	if(hst->current_state == HOST_UP && hst->notified_on == 0) {
		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "We shouldn't notify about this recovery.\n");
		return ERROR;
		}

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "** Host notification viability for contact '%s' PASSED.\n", cntct->name);

	return OK;
	}



/* notify a specific contact that an entire host is down or up */
int notify_contact_of_host(nagios_macros *mac, contact *cntct, host *hst, int type, char *not_author, char *not_data, int options, int escalated) {
	commandsmember *temp_commandsmember = NULL;
	char *command_name = NULL;
	char *command_name_ptr = NULL;
	char *temp_buffer = NULL;
	char *processed_buffer = NULL;
	char *raw_command = NULL;
	char *processed_command = NULL;
	struct timeval start_time;
	struct timeval end_time;
	struct timeval method_start_time;
	struct timeval method_end_time;
	int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;
	int neb_result;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "notify_contact_of_host()\n");

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "** Notifying contact '%s'\n", cntct->name);

	/* get start time */
	gettimeofday(&start_time, NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec = 0L;
	end_time.tv_usec = 0L;
	neb_result = broker_contact_notification_data(NEBTYPE_CONTACTNOTIFICATION_START, NEBFLAG_NONE, NEBATTR_NONE, HOST_NOTIFICATION, type, start_time, end_time, (void *)hst, cntct, not_author, not_data, escalated, NULL);
	if(NEBERROR_CALLBACKCANCEL == neb_result)
		return ERROR;
	else if(NEBERROR_CALLBACKOVERRIDE == neb_result)
		return OK;
#endif

	/* process all the notification commands this user has */
	for(temp_commandsmember = cntct->host_notification_commands; temp_commandsmember != NULL; temp_commandsmember = temp_commandsmember->next) {

		/* get start time */
		gettimeofday(&method_start_time, NULL);

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		method_end_time.tv_sec = 0L;
		method_end_time.tv_usec = 0L;
		neb_result = broker_contact_notification_method_data(NEBTYPE_CONTACTNOTIFICATIONMETHOD_START, NEBFLAG_NONE, NEBATTR_NONE, HOST_NOTIFICATION, type, method_start_time, method_end_time, (void *)hst, cntct, temp_commandsmember->command, not_author, not_data, escalated, NULL);
		if(NEBERROR_CALLBACKCANCEL == neb_result)
			break ;
		else if(NEBERROR_CALLBACKOVERRIDE == neb_result)
			continue ;
#endif

		/* get the raw command line */
		get_raw_command_line_r(mac, temp_commandsmember->command_ptr, temp_commandsmember->command, &raw_command, macro_options);
		if(raw_command == NULL)
			continue;

		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Raw notification command: %s\n", raw_command);

		/* process any macros contained in the argument */
		process_macros_r(mac, raw_command, &processed_command, macro_options);
		my_free(raw_command);
		if(processed_command == NULL)
			continue;

		/* get the command name */
		command_name = (char *)strdup(temp_commandsmember->command);
		command_name_ptr = strtok(command_name, "!");

		/* run the notification command... */

		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Processed notification command: %s\n", processed_command);

		/* log the notification to program log file */
		if(log_notifications == TRUE) {
			switch(type) {
				case NOTIFICATION_CUSTOM:
					asprintf(&temp_buffer, "HOST NOTIFICATION: %s;%s;CUSTOM ($HOSTSTATE$);%s;$HOSTOUTPUT$;$NOTIFICATIONAUTHOR$;$NOTIFICATIONCOMMENT$\n", cntct->name, hst->name, command_name_ptr);
					break;
				case NOTIFICATION_ACKNOWLEDGEMENT:
					asprintf(&temp_buffer, "HOST NOTIFICATION: %s;%s;ACKNOWLEDGEMENT ($HOSTSTATE$);%s;$HOSTOUTPUT$;$NOTIFICATIONAUTHOR$;$NOTIFICATIONCOMMENT$\n", cntct->name, hst->name, command_name_ptr);
					break;
				case NOTIFICATION_FLAPPINGSTART:
					asprintf(&temp_buffer, "HOST NOTIFICATION: %s;%s;FLAPPINGSTART ($HOSTSTATE$);%s;$HOSTOUTPUT$\n", cntct->name, hst->name, command_name_ptr);
					break;
				case NOTIFICATION_FLAPPINGSTOP:
					asprintf(&temp_buffer, "HOST NOTIFICATION: %s;%s;FLAPPINGSTOP ($HOSTSTATE$);%s;$HOSTOUTPUT$\n", cntct->name, hst->name, command_name_ptr);
					break;
				case NOTIFICATION_FLAPPINGDISABLED:
					asprintf(&temp_buffer, "HOST NOTIFICATION: %s;%s;FLAPPINGDISABLED ($HOSTSTATE$);%s;$HOSTOUTPUT$\n", cntct->name, hst->name, command_name_ptr);
					break;
				case NOTIFICATION_DOWNTIMESTART:
					asprintf(&temp_buffer, "HOST NOTIFICATION: %s;%s;DOWNTIMESTART ($HOSTSTATE$);%s;$HOSTOUTPUT$\n", cntct->name, hst->name, command_name_ptr);
					break;
				case NOTIFICATION_DOWNTIMEEND:
					asprintf(&temp_buffer, "HOST NOTIFICATION: %s;%s;DOWNTIMEEND ($HOSTSTATE$);%s;$HOSTOUTPUT$\n", cntct->name, hst->name, command_name_ptr);
					break;
				case NOTIFICATION_DOWNTIMECANCELLED:
					asprintf(&temp_buffer, "HOST NOTIFICATION: %s;%s;DOWNTIMECANCELLED ($HOSTSTATE$);%s;$HOSTOUTPUT$\n", cntct->name, hst->name, command_name_ptr);
					break;
				default:
					asprintf(&temp_buffer, "HOST NOTIFICATION: %s;%s;$HOSTSTATE$;%s;$HOSTOUTPUT$\n", cntct->name, hst->name, command_name_ptr);
					break;
				}

			process_macros_r(mac, temp_buffer, &processed_buffer, 0);
			write_to_all_logs(processed_buffer, NSLOG_HOST_NOTIFICATION);

			my_free(temp_buffer);
			my_free(processed_buffer);
			}

		/* run the notification command */
		wproc_notify(cntct->name, hst->name, NULL, processed_command, mac);

		/* @todo Handle nebmod stuff when getting results from workers */

		/* free memory */
		my_free(command_name);
		my_free(processed_command);

		/* get end time */
		gettimeofday(&method_end_time, NULL);

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_contact_notification_method_data(NEBTYPE_CONTACTNOTIFICATIONMETHOD_END, NEBFLAG_NONE, NEBATTR_NONE, HOST_NOTIFICATION, type, method_start_time, method_end_time, (void *)hst, cntct, temp_commandsmember->command, not_author, not_data, escalated, NULL);
#endif
		}

	/* get end time */
	gettimeofday(&end_time, NULL);

	/* update the contact's last host notification time */
	cntct->last_host_notification = start_time.tv_sec;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_contact_notification_data(NEBTYPE_CONTACTNOTIFICATION_END, NEBFLAG_NONE, NEBATTR_NONE, HOST_NOTIFICATION, type, start_time, end_time, (void *)hst, cntct, not_author, not_data, escalated, NULL);
#endif

	return OK;
	}


/* checks to see if a host escalation entry is a match for the current host notification */
int is_valid_escalation_for_host_notification(host *hst, hostescalation *he, int options) {
	int notification_number = 0;
	time_t current_time = 0L;
	host *temp_host = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "is_valid_escalation_for_host_notification()\n");

	/* get the current time */
	time(&current_time);

	/* if this is a recovery, really we check for who got notified about a previous problem */
	if(hst->current_state == HOST_UP)
		notification_number = hst->current_notification_number - 1;
	else
		notification_number = hst->current_notification_number;

	/* find the host this escalation entry is associated with */
	temp_host = he->host_ptr;
	if(temp_host == NULL || temp_host != hst)
		return FALSE;

	/*** EXCEPTION ***/
	/* broadcast options go to everyone, so this escalation is valid */
	if(options & NOTIFICATION_OPTION_BROADCAST)
		return TRUE;

	/* skip this escalation if it happens later */
	if(he->first_notification > notification_number)
		return FALSE;

	/* skip this escalation if it has already passed */
	if(he->last_notification != 0 && he->last_notification < notification_number)
		return FALSE;

	/* skip this escalation if the state options don't match */
	if(flag_isset(he->escalation_options, 1 << hst->current_state) == FALSE)
		return FALSE;

	/* skip this escalation if it has a timeperiod and the current time isn't valid */
	if(he->escalation_period != NULL && check_time_against_period(current_time, he->escalation_period_ptr) == ERROR)
		return FALSE;

	return TRUE;
	}



/* checks to see whether a host notification should be escalation */
int should_host_notification_be_escalated(host *hst) {
	objectlist *list;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "should_host_notification_be_escalated()\n");

	if(hst == NULL)
		return FALSE;

	/* search the host escalation list */
	for (list = hst->escalation_list; list; list = list->next) {
		hostescalation *temp_he = (hostescalation *)list->object_ptr;
		/* we found a matching entry, so escalate this notification! */
		if(is_valid_escalation_for_host_notification(hst, temp_he, NOTIFICATION_OPTION_NONE) == TRUE)
			return TRUE;
		}

	log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Host notification will NOT be escalated.\n");

	return FALSE;
	}


/* given a host, create a list of contacts to be notified, removing duplicates, checking contact notification viability */
int create_notification_list_from_host(nagios_macros *mac, host *hst, int options, int *escalated, int type) {
	hostescalation *temp_he = NULL;
	contactsmember *temp_contactsmember = NULL;
	contact *temp_contact = NULL;
	contactgroupsmember *temp_contactgroupsmember = NULL;
	contactgroup *temp_contactgroup = NULL;
	int escalate_notification = FALSE;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "create_notification_list_from_host()\n");

	/* see if this notification should be escalated */
	escalate_notification = should_host_notification_be_escalated(hst);

	/* set the escalation flag */
	*escalated = escalate_notification;

	/* make sure there aren't any leftover contacts */
	free_notification_list();

	/* set the escalation macro */
	mac->x[MACRO_NOTIFICATIONISESCALATED] = strdup(escalate_notification ? "1" : "0");

	if(options & NOTIFICATION_OPTION_BROADCAST)
		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "This notification will be BROADCAST to all (escalated and normal) contacts...\n");

	/* use escalated contacts for this notification */
	if(escalate_notification == TRUE || (options & NOTIFICATION_OPTION_BROADCAST)) {
		objectlist *list;

		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Adding contacts from host escalation(s) to notification list.\n");

		/* check all the host escalation entries */
		for(list = hst->escalation_list; list; list = list->next) {
			temp_he = (hostescalation *)list->object_ptr;

			/* see if this escalation if valid for this notification */
			if(is_valid_escalation_for_host_notification(hst, temp_he, options) == FALSE)
				continue;

			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Adding individual contacts from host escalation(s) to notification list.\n");

			/* add all individual contacts for this escalation */
			for(temp_contactsmember = temp_he->contacts; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
				if((temp_contact = temp_contactsmember->contact_ptr) == NULL)
					continue;
				/* check now if the contact can be notified */
				if (check_contact_host_notification_viability(temp_contact, hst, type, options) == OK)
					add_notification(mac, temp_contact);
				else
					log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Not adding contact '%s'\n", temp_contact->name);
				}

			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Adding members of contact groups from host escalation(s) to notification list.\n");

			/* add all contacts that belong to contactgroups for this escalation */
			for(temp_contactgroupsmember = temp_he->contact_groups; temp_contactgroupsmember != NULL; temp_contactgroupsmember = temp_contactgroupsmember->next) {
				log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Adding members of contact group '%s' for host escalation to notification list.\n", temp_contactgroupsmember->group_name);
				if((temp_contactgroup = temp_contactgroupsmember->group_ptr) == NULL)
					continue;
				for(temp_contactsmember = temp_contactgroup->members; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
					if((temp_contact = temp_contactsmember->contact_ptr) == NULL)
						continue;
	                                /* check now if the contact can be notified */
	                                if (check_contact_host_notification_viability(temp_contact, hst, type, options) == OK)
	                                        add_notification(mac, temp_contact);
	                                else
	                                        log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Not adding contact '%s'\n", temp_contact->name);
					}
				}
			}
		}

	/* use normal, non-escalated contacts for this notification */
	if(escalate_notification == FALSE  || (options & NOTIFICATION_OPTION_BROADCAST)) {

		log_debug_info(DEBUGL_NOTIFICATIONS, 1, "Adding normal contacts for host to notification list.\n");

		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Adding individual contacts for host to notification list.\n");

		/* add all individual contacts for this host */
		for(temp_contactsmember = hst->contacts; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
			if((temp_contact = temp_contactsmember->contact_ptr) == NULL)
				continue;
                        /* check now if the contact can be notified */
                        if (check_contact_host_notification_viability(temp_contact, hst, type, options) == OK)
                                add_notification(mac, temp_contact);
                        else
                                log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Not adding contact '%s'\n", temp_contact->name);
			}

		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Adding members of contact groups for host to notification list.\n");

		/* add all contacts that belong to contactgroups for this host */
		for(temp_contactgroupsmember = hst->contact_groups; temp_contactgroupsmember != NULL; temp_contactgroupsmember = temp_contactgroupsmember->next) {
			log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Adding members of contact group '%s' for host to notification list.\n", temp_contactgroupsmember->group_name);

			if((temp_contactgroup = temp_contactgroupsmember->group_ptr) == NULL)
				continue;
			for(temp_contactsmember = temp_contactgroup->members; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
				if((temp_contact = temp_contactsmember->contact_ptr) == NULL)
					continue;
	                        /* check now if the contact can be notified */
	                        if (check_contact_host_notification_viability(temp_contact, hst, type, options) == OK)
	                                add_notification(mac, temp_contact);
	                        else
	                                log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Not adding contact '%s'\n", temp_contact->name);
				}
			}
		}

	return OK;
	}




/******************************************************************/
/***************** NOTIFICATION TIMING FUNCTIONS ******************/
/******************************************************************/


/* calculates next acceptable re-notification time for a service */
time_t get_next_service_notification_time(service *svc, time_t offset) {
	time_t next_notification = 0L;
	double interval_to_use = 0.0;
	objectlist *list;
	int have_escalated_interval = FALSE;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "get_next_service_notification_time()\n");

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Calculating next valid notification time...\n");

	/* default notification interval */
	interval_to_use = svc->notification_interval;

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Default interval: %f\n", interval_to_use);

	/* search all the escalation entries for valid matches for this service (at its current notification number) */
	for(list = svc->escalation_list; list; list = list->next) {
		serviceescalation *temp_se = (serviceescalation *)list->object_ptr;

		/* interval < 0 means to use non-escalated interval */
		if(temp_se->notification_interval < 0.0)
			continue;

		/* skip this entry if it isn't appropriate */
		if(is_valid_escalation_for_service_notification(svc, temp_se, NOTIFICATION_OPTION_NONE) == FALSE)
			continue;

		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Found a valid escalation w/ interval of %f\n", temp_se->notification_interval);

		/* if we haven't used a notification interval from an escalation yet, use this one */
		if(have_escalated_interval == FALSE) {
			have_escalated_interval = TRUE;
			interval_to_use = temp_se->notification_interval;
			}

		/* else use the shortest of all valid escalation intervals */
		else if(temp_se->notification_interval < interval_to_use)
			interval_to_use = temp_se->notification_interval;

		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "New interval: %f\n", interval_to_use);
		}

	/* if notification interval is 0, we shouldn't send any more problem notifications (unless service is volatile) */
	if(interval_to_use == 0.0 && svc->is_volatile == FALSE)
		svc->no_more_notifications = TRUE;
	else
		svc->no_more_notifications = FALSE;

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Interval used for calculating next valid notification time: %f\n", interval_to_use);

	/* calculate next notification time */
	next_notification = offset + (interval_to_use * interval_length);

	return next_notification;
	}



/* calculates next acceptable re-notification time for a host */
time_t get_next_host_notification_time(host *hst, time_t offset) {
	time_t next_notification = 0L;
	double interval_to_use = 0.0;
	objectlist *list;
	int have_escalated_interval = FALSE;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "get_next_host_notification_time()\n");

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Calculating next valid notification time...\n");

	/* default notification interval */
	interval_to_use = hst->notification_interval;

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Default interval: %f\n", interval_to_use);

	/* check all the host escalation entries for valid matches for this host (at its current notification number) */
	for(list = hst->escalation_list; list; list = list->next) {
		hostescalation *temp_he = (hostescalation *)list->object_ptr;

		/* interval < 0 means to use non-escalated interval */
		if(temp_he->notification_interval < 0.0)
			continue;

		/* skip this entry if it isn't appropriate */
		if(is_valid_escalation_for_host_notification(hst, temp_he, NOTIFICATION_OPTION_NONE) == FALSE)
			continue;

		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Found a valid escalation w/ interval of %f\n", temp_he->notification_interval);

		/* if we haven't used a notification interval from an escalation yet, use this one */
		if(have_escalated_interval == FALSE) {
			have_escalated_interval = TRUE;
			interval_to_use = temp_he->notification_interval;
			}

		/* else use the shortest of all valid escalation intervals  */
		else if(temp_he->notification_interval < interval_to_use)
			interval_to_use = temp_he->notification_interval;

		log_debug_info(DEBUGL_NOTIFICATIONS, 2, "New interval: %f\n", interval_to_use);
		}

	/* if interval is 0, no more notifications should be sent */
	if(interval_to_use == 0.0)
		hst->no_more_notifications = TRUE;
	else
		hst->no_more_notifications = FALSE;

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Interval used for calculating next valid notification time: %f\n", interval_to_use);

	/* calculate next notification time */
	next_notification = offset + (interval_to_use * interval_length);

	return next_notification;
	}



/******************************************************************/
/***************** NOTIFICATION OBJECT FUNCTIONS ******************/
/******************************************************************/


/* given a contact name, find the notification entry for them for the list in memory */
notification * find_notification(contact *cntct) {
	notification *temp_notification = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "find_notification() start\n");

	if(cntct == NULL)
		return NULL;

	for(temp_notification = notification_list; temp_notification != NULL; temp_notification = temp_notification->next) {
		if(temp_notification->contact == cntct)
			return temp_notification;
		}

	/* we couldn't find the contact in the notification list */
	return NULL;
	}



/* add a new notification to the list in memory */
int add_notification(nagios_macros *mac, contact *cntct) {
	notification *new_notification = NULL;
	notification *temp_notification = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "add_notification() start\n");

	if(cntct == NULL)
		return ERROR;

	log_debug_info(DEBUGL_NOTIFICATIONS, 2, "Adding contact '%s' to notification list.\n", cntct->name);

	/* don't add anything if this contact is already on the notification list */
	if((temp_notification = find_notification(cntct)) != NULL)
		return OK;

	/* allocate memory for a new contact in the notification list */
	if((new_notification = malloc(sizeof(notification))) == NULL)
		return ERROR;

	/* fill in the contact info */
	new_notification->contact = cntct;

	/* add new notification to head of list */
	new_notification->next = notification_list;
	notification_list = new_notification;

	/* add contact to notification recipients macro */
	if(mac->x[MACRO_NOTIFICATIONRECIPIENTS] == NULL)
		mac->x[MACRO_NOTIFICATIONRECIPIENTS] = (char *)strdup(cntct->name);
	else {
		if((mac->x[MACRO_NOTIFICATIONRECIPIENTS] = (char *)realloc(mac->x[MACRO_NOTIFICATIONRECIPIENTS], strlen(mac->x[MACRO_NOTIFICATIONRECIPIENTS]) + strlen(cntct->name) + 2))) {
			strcat(mac->x[MACRO_NOTIFICATIONRECIPIENTS], ",");
			strcat(mac->x[MACRO_NOTIFICATIONRECIPIENTS], cntct->name);
			}
		}

	return OK;
	}
