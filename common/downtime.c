/*****************************************************************************
 *
 * DOWNTIME.C - Scheduled downtime functions for Nagios
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
#include "../include/comments.h"
#include "../include/downtime.h"
#include "../include/statusdata.h"

#ifdef NSCGI
#include "../include/cgiutils.h"
#else
#include "../include/nagios.h"
#include "../include/broker.h"
#endif


scheduled_downtime *scheduled_downtime_list = NULL;
int		   defer_downtime_sorting = 0;
static fanout_table *dt_fanout;
unsigned long next_downtime_id = 0;


#define DT_ENULL (-1)
#define DT_EHOST (-2)
#define DT_ESERVICE (-3)
#define DT_ETYPE (-4)
#define DT_ETRIGGER (-5)
#define DT_ETIME (-6)
static const char *dt_strerror(int err)
{
	if (err > 0)
		return strerror(err);

	switch(err) {
	 case DT_ENULL: return "NULL pointer";
	 case DT_EHOST: return "No hostname, or host not found";
	 case DT_ESERVICE: return "No service_description, or service not found";
	 case DT_ETYPE: return "Invalid downtime type, or type/data mismatch";
	 case DT_ETRIGGER: return "Triggering downtime not found";
	 case DT_ETIME: return "Bad time spec";
	}
	return "Unknown error";
}

static int downtime_compar(const void *p1, const void *p2) {
	scheduled_downtime *d1 = *(scheduled_downtime **)p1;
	scheduled_downtime *d2 = *(scheduled_downtime **)p2;

	/*
		If the start times of two downtimes are equal and one is triggered
		but the other is not, the triggered downtime should be later in the
		list than the untriggered one. This is so they are written to the
		retention.dat and status.dat in the correct order.

		Previously the triggered downtime always appeared before its
		triggering downtime in those files. When the downtimes were read
		from those files, either on a core restart or by the CGIs, the
		triggered downtime would be discarded because the triggering
		downtime did not yet exist.

		The most common case for this is when a downtime is created and
		the option is selected to create triggered downtimes on all child
		objects. This change in the sort order does NOT resolve the
		case where a manually created, triggered downtime is created with
		a start time earlier than the triggering downtime.

		This would need to be resolved by comparing the triggered_by value
		with the downtime ID regardless of the start time. However, this
		should be a relatively rare case and only caused by intentional
		scheduling by a human. This change was not implemented because it
		would cause the downtime list to be out of time order and the
		implications of this were not well understood.
	*/

	if(d1->start_time == d2->start_time) {
		if(( d1->triggered_by == 0 && d2->triggered_by != 0) ||
				( d1->triggered_by != 0 && d2->triggered_by == 0)) {
			return d1->triggered_by == 0 ? -1 : 1;
			}
		}
	return (d1->start_time < d2->start_time) ? -1 : (d1->start_time - d2->start_time);
	}

static int downtime_add(scheduled_downtime *dt)
{
	unsigned long prev_downtime_id;
	scheduled_downtime *trigger = NULL;
	struct host *h;
	struct service *s;

	if (!dt)
		return DT_ENULL;

	log_debug_info(DEBUGL_DOWNTIME, 0, "downtime_add(): id=%lu; type=%s; host=%s; service=%s\n",
				   dt->downtime_id,
				   dt->type == HOST_DOWNTIME ? "host" : "service",
				   dt->host_name, dt->service_description);
	/*
	 * check for errors.
	 * host_name must always be set
	 */
	if (!dt->host_name)
		return DT_EHOST;

	/* service_description should only be set for service downtime */
	if ((dt->type == HOST_DOWNTIME) != (!dt->service_description))
		return DT_ETYPE;
	/* type must be either SERVICE_DOWNTIME or HOST_DOWNTIME */
	if (dt->type != SERVICE_DOWNTIME && dt->type != HOST_DOWNTIME)
		return DT_ETYPE;
	/* triggered downtime must be triggered by an existing downtime */
	if (dt->triggered_by && !(trigger = find_downtime(ANY_DOWNTIME, dt->triggered_by)))
		return DT_ETRIGGER;
	/* non-triggered downtime must have start_time < end_time */
	if (!trigger && dt->start_time >= dt->end_time)
		return DT_ETIME;
	/* flexible downtime must have a duration */
	if (!dt->fixed && !dt->duration)
		return DT_ETIME;

	/* the object we're adding downtime for must exist */
	if (!dt->service_description) {
		if (!(h = find_host(dt->host_name)))
			return DT_EHOST;
	} else if (!(s = find_service(dt->host_name, dt->service_description))) {
		return DT_ESERVICE;
	}

	/* set downtime_id if not already set */
	prev_downtime_id = next_downtime_id;
	if (!dt->downtime_id) {
		dt->downtime_id = next_downtime_id++;
	} else if (dt->downtime_id > next_downtime_id) {
		next_downtime_id = dt->downtime_id + 1;
	}

	if (fanout_add(dt_fanout, dt->downtime_id, dt) < 0) {
		next_downtime_id = prev_downtime_id;
		return errno;
	}

	if(defer_downtime_sorting || !scheduled_downtime_list ||
	   downtime_compar(&dt, &scheduled_downtime_list) < 0)
	{
		if (scheduled_downtime_list) {
			scheduled_downtime_list->prev = dt;
		}
		dt->next = scheduled_downtime_list;
		scheduled_downtime_list = dt;
		}
	else {
		scheduled_downtime *cur;

		/* add new downtime to downtime list, sorted by start time */
		for(cur = scheduled_downtime_list; cur; cur = cur->next) {
			if(downtime_compar(&dt, &cur) < 0) {
				dt->prev = cur->prev;
				dt->next = cur;
				cur->prev->next = dt;
				cur->prev = dt;
				break;
				}
			if (!cur->next) {
				dt->next = NULL;
				cur->next = dt;
				dt->prev = cur;
				break;
			}
		}
	}
	return OK;
}

void downtime_remove(scheduled_downtime *dt)
{
	fanout_remove(dt_fanout, dt->downtime_id);
	if(scheduled_downtime_list == dt)
		scheduled_downtime_list = dt->next;
	else {
		dt->prev->next = dt->next;
		if (dt->next)
			dt->next->prev = dt->prev;
	}
}

/******************************************************************/
/**************** INITIALIZATION/CLEANUP FUNCTIONS ****************/
/******************************************************************/


/* initializes scheduled downtime data */
int initialize_downtime_data(void) {
	log_debug_info(DEBUGL_FUNCTIONS, 0, "initialize_downtime_data()\n");
	dt_fanout = fanout_create(16384);
	next_downtime_id = 1;
	return dt_fanout ? OK : ERROR;
	}


/* cleans up scheduled downtime data */
int cleanup_downtime_data(void) {
	/* free memory allocated to downtime data */
	free_downtime_data();
	return OK;
	}


#ifdef NSCORE

/******************************************************************/
/********************** SCHEDULING FUNCTIONS **********************/
/******************************************************************/

/* schedules a host or service downtime */
int schedule_downtime(int type, char *host_name, char *service_description, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *new_downtime_id) {
	unsigned long downtime_id = 0L;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "schedule_downtime()\n");

	/* don't add old or invalid downtimes */
	if(start_time >= end_time || end_time <= time(NULL)) {
		log_debug_info(DEBUGL_DOWNTIME, 1, "Invalid start (%lu) or end (%lu) times\n",
				start_time, end_time);
		return ERROR;
		}

	/* add a new downtime entry */
	add_new_downtime(type, host_name, service_description, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, &downtime_id, FALSE, FALSE);

	/* register the scheduled downtime */
	register_downtime(type, downtime_id);

	/* return downtime id */
	if(new_downtime_id != NULL)
		*new_downtime_id = downtime_id;

	return OK;
	}


/* unschedules a host or service downtime */
int unschedule_downtime(int type, unsigned long downtime_id) {
	scheduled_downtime *temp_downtime = NULL;
	scheduled_downtime *next_downtime = NULL;
	host *hst = NULL;
	service *svc = NULL;
#ifdef USE_EVENT_BROKER
	int attr = 0;
#endif

	log_debug_info(DEBUGL_FUNCTIONS, 0, "unschedule_downtime()\n");

	/* find the downtime entry in the list in memory */
	if((temp_downtime = find_downtime(type, downtime_id)) == NULL)
		return ERROR;

	/* find the host or service associated with this downtime */
	if(temp_downtime->type == HOST_DOWNTIME) {
		if((hst = find_host(temp_downtime->host_name)) == NULL)
			return ERROR;
		}
	else {
		if((svc = find_service(temp_downtime->host_name, temp_downtime->service_description)) == NULL)
			return ERROR;
		}

	/* decrement pending flex downtime if necessary ... */
	if(temp_downtime->fixed == FALSE && temp_downtime->incremented_pending_downtime == TRUE) {
		if(temp_downtime->type == HOST_DOWNTIME)
			hst->pending_flex_downtime--;
		else
			svc->pending_flex_downtime--;
		}

	log_debug_info(DEBUGL_DOWNTIME, 0, "Cancelling %s downtime (id=%lu)\n",
	               temp_downtime->type == HOST_DOWNTIME ? "host" : "service",
	               temp_downtime->downtime_id);

	/* decrement the downtime depth variable and update status data if necessary */
	if(temp_downtime->is_in_effect == TRUE) {

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		attr = NEBATTR_DOWNTIME_STOP_CANCELLED;
		broker_downtime_data(NEBTYPE_DOWNTIME_STOP, NEBFLAG_NONE, attr, temp_downtime->type, temp_downtime->host_name, temp_downtime->service_description, temp_downtime->entry_time, temp_downtime->author, temp_downtime->comment, temp_downtime->start_time, temp_downtime->end_time, temp_downtime->fixed, temp_downtime->triggered_by, temp_downtime->duration, temp_downtime->downtime_id, NULL);
#endif

		if(temp_downtime->type == HOST_DOWNTIME) {

			if (hst->scheduled_downtime_depth > 0)
				hst->scheduled_downtime_depth--;
			update_host_status(hst, FALSE);

			/* log a notice - this is parsed by the history CGI */
			if(hst->scheduled_downtime_depth == 0) {

				logit(NSLOG_INFO_MESSAGE, FALSE, "HOST DOWNTIME ALERT: %s;CANCELLED; Scheduled downtime for host has been cancelled.\n", hst->name);

				/* send a notification */
				host_notification(hst, NOTIFICATION_DOWNTIMECANCELLED, NULL, NULL, NOTIFICATION_OPTION_NONE);
				}
			}

		else {

			if (svc->scheduled_downtime_depth > 0)
				svc->scheduled_downtime_depth--;
			update_service_status(svc, FALSE);

			/* log a notice - this is parsed by the history CGI */
			if(svc->scheduled_downtime_depth == 0) {

				logit(NSLOG_INFO_MESSAGE, FALSE, "SERVICE DOWNTIME ALERT: %s;%s;CANCELLED; Scheduled downtime for service has been cancelled.\n", svc->host_name, svc->description);

				/* send a notification */
				service_notification(svc, NOTIFICATION_DOWNTIMECANCELLED, NULL, NULL, NOTIFICATION_OPTION_NONE);
				}
			}
		}

	/* remove scheduled entries from event queue */
	if (temp_downtime->start_event) {
		remove_event(nagios_squeue, temp_downtime->start_event);
		my_free(temp_downtime->start_event);
		}
	if (temp_downtime->stop_event) {
		remove_event(nagios_squeue, temp_downtime->stop_event);
		my_free(temp_downtime->stop_event);
		}

	/* delete downtime entry */
	if(temp_downtime->type == HOST_DOWNTIME)
		delete_host_downtime(downtime_id);
	else
		delete_service_downtime(downtime_id);

	/*
	 * unschedule all downtime entries that were triggered by this one
	 * @TODO: Fix this algorithm so it uses something sane instead
	 * of this horrible mess of recursive O(n * t), where t is
	 * "downtime triggered by this downtime"
	 */
	while(1) {

		for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; temp_downtime = next_downtime) {
			next_downtime = temp_downtime->next;
			if(temp_downtime->triggered_by == downtime_id) {
				unschedule_downtime(ANY_DOWNTIME, temp_downtime->downtime_id);
				break;
				}
			}

		if(temp_downtime == NULL)
			break;
		}

	return OK;
	}



/* registers scheduled downtime (schedules it, adds comments, etc.) */
int register_downtime(int type, unsigned long downtime_id) {
	char *temp_buffer = NULL;
	char start_time_string[MAX_DATETIME_LENGTH] = "";
	char flex_start_string[MAX_DATETIME_LENGTH] = "";
	char end_time_string[MAX_DATETIME_LENGTH] = "";
	scheduled_downtime *temp_downtime = NULL;
	host *hst = NULL;
	service *svc = NULL;
	const char *type_string = NULL;
	int hours = 0;
	int minutes = 0;
	int seconds = 0;
	unsigned long *new_downtime_id = NULL;
	int was_in_effect = FALSE;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "register_downtime( %d, %lu)\n", type,
			downtime_id);

	/* find the downtime entry in memory */
	temp_downtime = find_downtime(type, downtime_id);
	if(temp_downtime == NULL) {
		log_debug_info(DEBUGL_DOWNTIME, 0, "Cannot find downtime ID: %lu\n", downtime_id);
		return ERROR;
		}

	/* find the host or service associated with this downtime */
	if(temp_downtime->type == HOST_DOWNTIME) {
		if((hst = find_host(temp_downtime->host_name)) == NULL) {
			log_debug_info(DEBUGL_DOWNTIME, 1,
					"Cannot find host (%s) for downtime ID: %lu\n",
					temp_downtime->host_name, downtime_id);
			return ERROR;
			}
		}
	else {
		if((svc = find_service(temp_downtime->host_name, temp_downtime->service_description)) == NULL) {
			log_debug_info(DEBUGL_DOWNTIME, 1,
					"Cannot find service (%s) for host (%s) for downtime ID: %lu\n",
					temp_downtime->service_description, temp_downtime->host_name,
					downtime_id);
			return ERROR;
			}
		}

	/* create the comment */
	get_datetime_string(&(temp_downtime->start_time), start_time_string, MAX_DATETIME_LENGTH, SHORT_DATE_TIME);
	get_datetime_string(&(temp_downtime->flex_downtime_start), flex_start_string, MAX_DATETIME_LENGTH, SHORT_DATE_TIME);
	get_datetime_string(&(temp_downtime->end_time), end_time_string, MAX_DATETIME_LENGTH, SHORT_DATE_TIME);
	hours = temp_downtime->duration / 3600;
	minutes = ((temp_downtime->duration - (hours * 3600)) / 60);
	seconds = temp_downtime->duration - (hours * 3600) - (minutes * 60);
	if(temp_downtime->type == HOST_DOWNTIME)
		type_string = "host";
	else
		type_string = "service";
	if(temp_downtime->fixed == TRUE)
		asprintf(&temp_buffer, "This %s has been scheduled for fixed downtime from %s to %s.  Notifications for the %s will not be sent out during that time period.", type_string, start_time_string, end_time_string, type_string);
	else
		asprintf(&temp_buffer, "This %s has been scheduled for flexible downtime starting between %s and %s and lasting for a period of %d hours and %d minutes.  Notifications for the %s will not be sent out during that time period.", type_string, start_time_string, end_time_string, hours, minutes, type_string);


	log_debug_info(DEBUGL_DOWNTIME, 0, "Scheduled Downtime Details:\n");
	if(temp_downtime->type == HOST_DOWNTIME) {
		log_debug_info(DEBUGL_DOWNTIME, 0, " Type:        Host Downtime\n");
		log_debug_info(DEBUGL_DOWNTIME, 0, " Host:        %s\n", hst->name);
		}
	else {
		log_debug_info(DEBUGL_DOWNTIME, 0, " Type:        Service Downtime\n");
		log_debug_info(DEBUGL_DOWNTIME, 0, " Host:        %s\n", svc->host_name);
		log_debug_info(DEBUGL_DOWNTIME, 0, " Service:     %s\n", svc->description);
		}
	log_debug_info(DEBUGL_DOWNTIME, 0, " Fixed/Flex:  %s\n", (temp_downtime->fixed == TRUE) ? "Fixed" : "Flexible");
	log_debug_info(DEBUGL_DOWNTIME, 0, " Start:       %s\n", start_time_string);
	if( temp_downtime->flex_downtime_start) {
		log_debug_info(DEBUGL_DOWNTIME, 0, " Flex Start:  %s\n", flex_start_string);
		}
	log_debug_info(DEBUGL_DOWNTIME, 0, " End:         %s\n", end_time_string);
	log_debug_info(DEBUGL_DOWNTIME, 0, " Duration:    %dh %dm %ds\n", hours, minutes, seconds);
	log_debug_info(DEBUGL_DOWNTIME, 0, " Downtime ID: %lu\n", temp_downtime->downtime_id);
	log_debug_info(DEBUGL_DOWNTIME, 0, " Trigger ID:  %lu\n", temp_downtime->triggered_by);


	/* add a non-persistent comment to the host or service regarding the scheduled outage */
	if(temp_downtime->type == SERVICE_DOWNTIME)
		add_new_comment(SERVICE_COMMENT, DOWNTIME_COMMENT, svc->host_name, svc->description, time(NULL), ( NULL == temp_downtime->author ? "(Nagios Process)" : temp_downtime->author), temp_buffer, 0, COMMENTSOURCE_INTERNAL, FALSE, (time_t)0, &(temp_downtime->comment_id));
	else
		add_new_comment(HOST_COMMENT, DOWNTIME_COMMENT, hst->name, NULL, time(NULL), ( NULL == temp_downtime->author ? "(Nagios Process)" : temp_downtime->author), temp_buffer, 0, COMMENTSOURCE_INTERNAL, FALSE, (time_t)0, &(temp_downtime->comment_id));

	my_free(temp_buffer);

	/* only non-triggered downtime is scheduled... */
	if((temp_downtime->triggered_by == 0) && ((TRUE == temp_downtime->fixed) ||
			((FALSE == temp_downtime->fixed) &&
			(TRUE == temp_downtime->is_in_effect)))) {
		/* If this is a fixed downtime, schedule the event to start it. If this
			is a flexible downtime, normally we wait for one of the
			check_pending_flex_*_downtime() functions to start it, but if the
			downtime is already in effect, this means that we are restarting
			Nagios and the downtime was in effect when we last shutdown
			Nagios, so we should restart the flexible downtime now. This
			should work even if the downtime has ended because the
			handle_scheduled_dowtime() function will immediately schedule
			another downtime event which will end the downtime. */
		if((new_downtime_id = (unsigned long *)malloc(sizeof(unsigned long *)))) {
			*new_downtime_id = downtime_id;
			temp_downtime->start_event = schedule_new_event(EVENT_SCHEDULED_DOWNTIME, TRUE, temp_downtime->start_time, FALSE, 0, NULL, FALSE, (void *)new_downtime_id, NULL, 0);
			/* Turn off is_in_effect flag so handle_scheduled_downtime() will
				handle it correctly */
			was_in_effect = temp_downtime->is_in_effect;
			temp_downtime->is_in_effect = FALSE;
			}
		}

	/* If the downtime is triggered and was in effect, mark it as not in 
		effect so it gets scheduled correctly */
	if((temp_downtime->triggered_by != 0) && 
			(TRUE == temp_downtime->is_in_effect)) {
		was_in_effect = temp_downtime->is_in_effect;
		temp_downtime->is_in_effect = FALSE;
		}

	if((FALSE == temp_downtime->fixed) && (FALSE == was_in_effect)) {
		/* increment pending flex downtime counter */
		if(temp_downtime->type == HOST_DOWNTIME)
			hst->pending_flex_downtime++;
		else
			svc->pending_flex_downtime++;
		temp_downtime->incremented_pending_downtime = TRUE;

		/* Since a flex downtime may never start, schedule an expiring event in
			case the event is never triggered. The expire event will NOT cancel
			a downtime event that is in effect */
		log_debug_info(DEBUGL_DOWNTIME, 1, "Scheduling downtime expire event in case flexible downtime is never triggered\n");
		temp_downtime->stop_event = schedule_new_event(EVENT_EXPIRE_DOWNTIME, TRUE, (temp_downtime->end_time + 1), FALSE, 0, NULL, FALSE, NULL, NULL, 0);
		}

#ifdef PROBABLY_NOT_NEEDED
	/*** FLEXIBLE DOWNTIME SANITY CHECK - ADDED 02/17/2008 ****/

	/* if host/service is in a non-OK/UP state right now, see if we should start flexible time immediately */
	/* this is new logic added in 3.0rc3 */
	if(temp_downtime->fixed == FALSE) {
		if(temp_downtime->type == HOST_DOWNTIME)
			check_pending_flex_host_downtime(hst);
		else
			check_pending_flex_service_downtime(svc);
		}
#endif

	return OK;
	}



/* handles scheduled downtime (id passed from timed event queue) */
int handle_scheduled_downtime_by_id(unsigned long downtime_id) {
	scheduled_downtime *temp_downtime = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "handle_scheduled_downtime_by_id()\n");

	/* find the downtime entry */
	if((temp_downtime = find_downtime(ANY_DOWNTIME, downtime_id)) == NULL) {
		log_debug_info(DEBUGL_DOWNTIME, 1, "Unable to find downtime id: %lu\n",
				downtime_id);
		return ERROR;
		}

	/* NULL out this event's start time since the calling function,
		handle_timed_event(), will free the event, this will prevent
		unschedule_downtime from freeing something that has already been
		freed. The start event is not needed within
		handle_scheduled_downtime(). */
	temp_downtime->start_event = NULL;

	/* handle the downtime */
	return handle_scheduled_downtime(temp_downtime);
	}



/* handles scheduled host or service downtime */
int handle_scheduled_downtime(scheduled_downtime *temp_downtime) {
	scheduled_downtime *this_downtime = NULL;
	host *hst = NULL;
	service *svc = NULL;
	time_t event_time = 0L;
	unsigned long *new_downtime_id = NULL;
#ifdef USE_EVENT_BROKER
	int attr = 0;
#endif


	log_debug_info(DEBUGL_FUNCTIONS, 0, "handle_scheduled_downtime()\n");

	if(temp_downtime == NULL)
		return ERROR;

	/* find the host or service associated with this downtime */
	if(temp_downtime->type == HOST_DOWNTIME) {
		if((hst = find_host(temp_downtime->host_name)) == NULL) {
			log_debug_info(DEBUGL_DOWNTIME, 1, "Unable to find host (%s) for downtime\n", temp_downtime->host_name);
			return ERROR;
			}
		}
	else {
		if((svc = find_service(temp_downtime->host_name, temp_downtime->service_description)) == NULL) {
			log_debug_info(DEBUGL_DOWNTIME, 1, "Unable to find service (%s) host (%s) for downtime\n", temp_downtime->service_description, temp_downtime->host_name);
			return ERROR;
			}
		}

	/* have we come to the end of the scheduled downtime? */
	if(temp_downtime->is_in_effect == TRUE) {

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		attr = NEBATTR_DOWNTIME_STOP_NORMAL;
		broker_downtime_data(NEBTYPE_DOWNTIME_STOP, NEBFLAG_NONE, attr, temp_downtime->type, temp_downtime->host_name, temp_downtime->service_description, temp_downtime->entry_time, temp_downtime->author, temp_downtime->comment, temp_downtime->start_time, temp_downtime->end_time, temp_downtime->fixed, temp_downtime->triggered_by, temp_downtime->duration, temp_downtime->downtime_id, NULL);
#endif

		/* decrement the downtime depth variable */
		if(temp_downtime->type == HOST_DOWNTIME && hst->scheduled_downtime_depth > 0)
			hst->scheduled_downtime_depth--;
		else if (svc->scheduled_downtime_depth > 0)
			svc->scheduled_downtime_depth--;

		if(temp_downtime->type == HOST_DOWNTIME && hst->scheduled_downtime_depth == 0) {

			log_debug_info(DEBUGL_DOWNTIME, 0, "Host '%s' has exited from a period of scheduled downtime (id=%lu).\n", hst->name, temp_downtime->downtime_id);

			/* log a notice - this one is parsed by the history CGI */
			logit(NSLOG_INFO_MESSAGE, FALSE, "HOST DOWNTIME ALERT: %s;STOPPED; Host has exited from a period of scheduled downtime", hst->name);

			/* send a notification */
			host_notification(hst, NOTIFICATION_DOWNTIMEEND, temp_downtime->author, temp_downtime->comment, NOTIFICATION_OPTION_NONE);
			}

		else if(temp_downtime->type == SERVICE_DOWNTIME && svc->scheduled_downtime_depth == 0) {

			log_debug_info(DEBUGL_DOWNTIME, 0, "Service '%s' on host '%s' has exited from a period of scheduled downtime (id=%lu).\n", svc->description, svc->host_name, temp_downtime->downtime_id);

			/* log a notice - this one is parsed by the history CGI */
			logit(NSLOG_INFO_MESSAGE, FALSE, "SERVICE DOWNTIME ALERT: %s;%s;STOPPED; Service has exited from a period of scheduled downtime", svc->host_name, svc->description);

			/* send a notification */
			service_notification(svc, NOTIFICATION_DOWNTIMEEND, temp_downtime->author, temp_downtime->comment, NOTIFICATION_OPTION_NONE);
			}


		/* update the status data */
		if(temp_downtime->type == HOST_DOWNTIME)
			update_host_status(hst, FALSE);
		else
			update_service_status(svc, FALSE);

		/* decrement pending flex downtime if necessary */
		if(temp_downtime->fixed == FALSE && temp_downtime->incremented_pending_downtime == TRUE) {
			if(temp_downtime->type == HOST_DOWNTIME) {
				if(hst->pending_flex_downtime > 0)
					hst->pending_flex_downtime--;
				}
			else {
				if(svc->pending_flex_downtime > 0)
					svc->pending_flex_downtime--;
				}
			}

		/* handle (stop) downtime that is triggered by this one */
		while(1) {

			/* list contents might change by recursive calls, so we use this inefficient method to prevent segfaults */
			for(this_downtime = scheduled_downtime_list; this_downtime != NULL; this_downtime = this_downtime->next) {
				if(this_downtime->triggered_by == temp_downtime->downtime_id) {
					handle_scheduled_downtime(this_downtime);
					break;
					}
				}

			if(this_downtime == NULL)
				break;
			}

		/* delete downtime entry */
		if(temp_downtime->type == HOST_DOWNTIME)
			delete_host_downtime(temp_downtime->downtime_id);
		else
			delete_service_downtime(temp_downtime->downtime_id);
		}

	/* else we are just starting the scheduled downtime */
	else {

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_downtime_data(NEBTYPE_DOWNTIME_START, NEBFLAG_NONE, NEBATTR_NONE, temp_downtime->type, temp_downtime->host_name, temp_downtime->service_description, temp_downtime->entry_time, temp_downtime->author, temp_downtime->comment, temp_downtime->start_time, temp_downtime->end_time, temp_downtime->fixed, temp_downtime->triggered_by, temp_downtime->duration, temp_downtime->downtime_id, NULL);
#endif

		if(temp_downtime->type == HOST_DOWNTIME && hst->scheduled_downtime_depth == 0) {

			log_debug_info(DEBUGL_DOWNTIME, 0, "Host '%s' has entered a period of scheduled downtime (id=%lu).\n", hst->name, temp_downtime->downtime_id);

			/* log a notice - this one is parsed by the history CGI */
			logit(NSLOG_INFO_MESSAGE, FALSE, "HOST DOWNTIME ALERT: %s;STARTED; Host has entered a period of scheduled downtime", hst->name);

			/* send a notification */
			if( FALSE == temp_downtime->start_notification_sent) {
				host_notification(hst, NOTIFICATION_DOWNTIMESTART, temp_downtime->author, temp_downtime->comment, NOTIFICATION_OPTION_NONE);
				temp_downtime->start_notification_sent = TRUE;
				}
			}

		else if(temp_downtime->type == SERVICE_DOWNTIME && svc->scheduled_downtime_depth == 0) {

			log_debug_info(DEBUGL_DOWNTIME, 0, "Service '%s' on host '%s' has entered a period of scheduled downtime (id=%lu).\n", svc->description, svc->host_name, temp_downtime->downtime_id);

			/* log a notice - this one is parsed by the history CGI */
			logit(NSLOG_INFO_MESSAGE, FALSE, "SERVICE DOWNTIME ALERT: %s;%s;STARTED; Service has entered a period of scheduled downtime", svc->host_name, svc->description);

			/* send a notification */
			if( FALSE == temp_downtime->start_notification_sent) {
				service_notification(svc, NOTIFICATION_DOWNTIMESTART, temp_downtime->author, temp_downtime->comment, NOTIFICATION_OPTION_NONE);
				temp_downtime->start_notification_sent = TRUE;
				}
			}

		/* increment the downtime depth variable */
		if(temp_downtime->type == HOST_DOWNTIME)
			hst->scheduled_downtime_depth++;
		else
			svc->scheduled_downtime_depth++;

		/* set the in effect flag */
		temp_downtime->is_in_effect = TRUE;

		/* update the status data */
		if(temp_downtime->type == HOST_DOWNTIME)
			update_host_status(hst, FALSE);
		else
			update_service_status(svc, FALSE);

		/* schedule an event to end the downtime */
		if(temp_downtime->fixed == FALSE) {
			event_time = (time_t)((unsigned long)temp_downtime->flex_downtime_start
					+ temp_downtime->duration);
			}
		else {
			event_time = temp_downtime->end_time;
			}
		if((new_downtime_id = (unsigned long *)malloc(sizeof(unsigned long *)))) {
			*new_downtime_id = temp_downtime->downtime_id;
			schedule_new_event(EVENT_SCHEDULED_DOWNTIME, TRUE, event_time, FALSE, 0, NULL, FALSE, (void *)new_downtime_id, NULL, 0);
			}

		/* handle (start) downtime that is triggered by this one */
		for(this_downtime = scheduled_downtime_list; this_downtime != NULL; this_downtime = this_downtime->next) {
			if(this_downtime->triggered_by == temp_downtime->downtime_id)
				handle_scheduled_downtime(this_downtime);
			}
		}

	return OK;
	}



/* checks for flexible (non-fixed) host downtime that should start now */
int check_pending_flex_host_downtime(host *hst) {
	scheduled_downtime *temp_downtime = NULL;
	time_t current_time = 0L;
	unsigned long * new_downtime_id = NULL;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_pending_flex_host_downtime()\n");

	if(hst == NULL)
		return ERROR;

	time(&current_time);

	/* if host is currently up, nothing to do */
	if(hst->current_state == HOST_UP)
		return OK;

	/* check all downtime entries */
	for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; temp_downtime = temp_downtime->next) {

		if(temp_downtime->type != HOST_DOWNTIME)
			continue;

		if(temp_downtime->fixed == TRUE)
			continue;

		if(temp_downtime->is_in_effect == TRUE)
			continue;

		/* triggered downtime entries should be ignored here */
		if(temp_downtime->triggered_by != 0)
			continue;

		/* this entry matches our host! */
		if(find_host(temp_downtime->host_name) == hst) {

			/* if the time boundaries are okay, start this scheduled downtime */
			if(temp_downtime->start_time <= current_time && current_time <= temp_downtime->end_time) {

				log_debug_info(DEBUGL_DOWNTIME, 0, "Flexible downtime (id=%lu) for host '%s' starting now...\n", temp_downtime->downtime_id, hst->name);
				temp_downtime->flex_downtime_start = current_time;
				if((new_downtime_id = (unsigned long *)malloc(sizeof(unsigned long *)))) {
					*new_downtime_id = temp_downtime->downtime_id;
					temp_downtime->start_event = schedule_new_event(EVENT_SCHEDULED_DOWNTIME, TRUE, temp_downtime->flex_downtime_start, FALSE, 0, NULL, FALSE, (void *)new_downtime_id, NULL, 0);
					}
				}
			}
		}

	return OK;
	}


/* checks for flexible (non-fixed) service downtime that should start now */
int check_pending_flex_service_downtime(service *svc) {
	scheduled_downtime *temp_downtime = NULL;
	time_t current_time = 0L;
	unsigned long * new_downtime_id = NULL;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_pending_flex_service_downtime()\n");

	if(svc == NULL)
		return ERROR;

	time(&current_time);

	/* if service is currently ok, nothing to do */
	if(svc->current_state == STATE_OK)
		return OK;

	/* check all downtime entries */
	for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; temp_downtime = temp_downtime->next) {

		if(temp_downtime->type != SERVICE_DOWNTIME)
			continue;

		if(temp_downtime->fixed == TRUE)
			continue;

		if(temp_downtime->is_in_effect == TRUE)
			continue;

		/* triggered downtime entries should be ignored here */
		if(temp_downtime->triggered_by != 0)
			continue;

		/* this entry matches our service! */
		if(find_service(temp_downtime->host_name, temp_downtime->service_description) == svc) {

			/* if the time boundaries are okay, start this scheduled downtime */
			if(temp_downtime->start_time <= current_time && current_time <= temp_downtime->end_time) {

				log_debug_info(DEBUGL_DOWNTIME, 0, "Flexible downtime (id=%lu) for service '%s' on host '%s' starting now...\n", temp_downtime->downtime_id, svc->description, svc->host_name);

				temp_downtime->flex_downtime_start = current_time;
				if((new_downtime_id = (unsigned long *)malloc(sizeof(unsigned long *)))) {
					*new_downtime_id = temp_downtime->downtime_id;
					temp_downtime->start_event = schedule_new_event(EVENT_SCHEDULED_DOWNTIME, TRUE, temp_downtime->flex_downtime_start, FALSE, 0, NULL, FALSE, (void *)new_downtime_id, NULL, 0);
					}
				}
			}
		}

	return OK;
	}


/* checks for (and removes) expired downtime entries */
int check_for_expired_downtime(void) {
	scheduled_downtime *temp_downtime = NULL;
	scheduled_downtime *next_downtime = NULL;
	time_t current_time = 0L;
	service *svc = NULL;
	host *hst = NULL;


	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_for_expired_downtime()\n");

	time(&current_time);

	/* check all downtime entries... */
	for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; temp_downtime = next_downtime) {

		next_downtime = temp_downtime->next;

		/* this entry should be removed */
		if(temp_downtime->is_in_effect == FALSE && temp_downtime->end_time <= current_time) {

			log_debug_info(DEBUGL_DOWNTIME, 0, "Expiring %s downtime (id=%lu)...\n", (temp_downtime->type == HOST_DOWNTIME) ? "host" : "service", temp_downtime->downtime_id);

			/* find the host or service associated with this downtime */
			if(temp_downtime->type == HOST_DOWNTIME) {
				if((hst = find_host(temp_downtime->host_name)) == NULL) {
					log_debug_info(DEBUGL_DOWNTIME, 1, 
							"Unable to find host (%s) for downtime\n", 
							temp_downtime->host_name);
					return ERROR;
					}

					/* send a notification */
					host_notification(hst, NOTIFICATION_DOWNTIMEEND, 
							temp_downtime->author, temp_downtime->comment, 
							NOTIFICATION_OPTION_NONE);
				}
			else {
				if((svc = find_service(temp_downtime->host_name, 
							temp_downtime->service_description)) == NULL) {
					log_debug_info(DEBUGL_DOWNTIME, 1, 
							"Unable to find service (%s) host (%s) for downtime\n", 
							temp_downtime->service_description, 
							temp_downtime->host_name);
					return ERROR;
					}

				/* send a notification */
				service_notification(svc, NOTIFICATION_DOWNTIMEEND, 
							temp_downtime->author, temp_downtime->comment, 
							NOTIFICATION_OPTION_NONE);
			}

			/* delete the downtime entry */
			if(temp_downtime->type == HOST_DOWNTIME)
				delete_host_downtime(temp_downtime->downtime_id);
			else
				delete_service_downtime(temp_downtime->downtime_id);
			}
		}

	return OK;
	}



/******************************************************************/
/************************* SAVE FUNCTIONS *************************/
/******************************************************************/


/* save a host or service downtime */
int add_new_downtime(int type, char *host_name, char *service_description, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id, int is_in_effect, int start_notification_sent){
	int result = OK;

	if(type == HOST_DOWNTIME)
		return add_new_host_downtime(host_name, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, downtime_id, is_in_effect, start_notification_sent);
	else
		return add_new_service_downtime(host_name, service_description, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, downtime_id, is_in_effect, start_notification_sent);

	return result;
	}

static unsigned long get_next_downtime_id(void) {
	unsigned long new_dt_id;
	for (;;) {
		new_dt_id = next_downtime_id++;
		if (!find_downtime(ANY_DOWNTIME, next_downtime_id)) {
			return new_dt_id;
			}
		}
	return 0;
	}

/* saves a host downtime entry */
int add_new_host_downtime(char *host_name, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id, int is_in_effect, int start_notification_sent){
	int result = OK;
	unsigned long new_downtime_id = 0L;

	if(host_name == NULL)
		return ERROR;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "cleanup_downtime_data()\n");
	new_downtime_id = get_next_downtime_id();
	result = add_host_downtime(host_name, entry_time, author, comment_data, start_time, 0, end_time, fixed, triggered_by, duration, new_downtime_id, is_in_effect, start_notification_sent);

	/* save downtime id */
	if(downtime_id != NULL)
		*downtime_id = new_downtime_id;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_downtime_data(NEBTYPE_DOWNTIME_ADD, NEBFLAG_NONE, NEBATTR_NONE, HOST_DOWNTIME, host_name, NULL, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, new_downtime_id, NULL);
#endif

	return result;
	}


/* saves a service downtime entry */
int add_new_service_downtime(char *host_name, char *service_description, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id, int is_in_effect, int start_notification_sent){
	int result = OK;
	unsigned long new_downtime_id = 0L;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "add_new_service_downtime()\n");

	if(host_name == NULL || service_description == NULL) {
		log_debug_info(DEBUGL_DOWNTIME, 1,
				"Host name (%s) or service description (%s) is null\n",
				((NULL == host_name) ? "null" : host_name),
				((NULL == service_description) ? "null" : service_description));
		return ERROR;
		}

	new_downtime_id = get_next_downtime_id();
	result = add_service_downtime(host_name, service_description, entry_time, author, comment_data, start_time, 0, end_time, fixed, triggered_by, duration, new_downtime_id, is_in_effect, start_notification_sent);

	/* save downtime id */
	if(downtime_id != NULL)
		*downtime_id = new_downtime_id;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_downtime_data(NEBTYPE_DOWNTIME_ADD, NEBFLAG_NONE, NEBATTR_NONE, SERVICE_DOWNTIME, host_name, service_description, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, new_downtime_id, NULL);
#endif

	return result;
	}



/******************************************************************/
/*********************** DELETION FUNCTIONS ***********************/
/******************************************************************/


/* deletes a scheduled host or service downtime entry from the list in memory */
int delete_downtime(int type, unsigned long downtime_id) {
	scheduled_downtime *this_downtime = NULL;

	/* find the downtime we should remove */
	this_downtime = find_downtime(type, downtime_id);
	if(!this_downtime)
		return ERROR;

	downtime_remove(this_downtime);

	/* first remove the comment associated with this downtime */
	if(this_downtime->type == HOST_DOWNTIME)
		delete_host_comment(this_downtime->comment_id);
	else
		delete_service_comment(this_downtime->comment_id);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_downtime_data(NEBTYPE_DOWNTIME_DELETE, NEBFLAG_NONE, NEBATTR_NONE, type, this_downtime->host_name, this_downtime->service_description, this_downtime->entry_time, this_downtime->author, this_downtime->comment, this_downtime->start_time, this_downtime->end_time, this_downtime->fixed, this_downtime->triggered_by, this_downtime->duration, downtime_id, NULL);
#endif

	/* free memory */
	my_free(this_downtime->host_name);
	my_free(this_downtime->service_description);
	my_free(this_downtime->author);
	my_free(this_downtime->comment);
	my_free(this_downtime);
	return OK;
	}


int delete_host_downtime(unsigned long downtime_id) {
	return delete_downtime(HOST_DOWNTIME, downtime_id);
	}


int delete_service_downtime(unsigned long downtime_id) {
	return delete_downtime(SERVICE_DOWNTIME, downtime_id);
	}

/*
 * Deletes all host and service downtimes on a host by hostname,
 * optionally filtered by service description, start time and comment.
 * All char* must be set or NULL - "" will silently fail to match
 * Returns number deleted
 */
int delete_downtime_by_hostname_service_description_start_time_comment(char *hostname, char *service_description, time_t start_time, char *cmnt) {
	scheduled_downtime *temp_downtime;
	scheduled_downtime *next_downtime;
	void *downtime_cpy;
	int deleted = 0;
	objectlist *matches = NULL, *tmp_match = NULL;

	/* Do not allow deletion of everything - must have at least 1 filter on */
	if(hostname == NULL && service_description == NULL && start_time == 0 && cmnt == NULL)
		return deleted;

	for(temp_downtime = scheduled_downtime_list; temp_downtime != NULL; temp_downtime = next_downtime) {
		next_downtime = temp_downtime->next;
		if(start_time != 0 && temp_downtime->start_time != start_time) {
			continue;
			}
		if(cmnt != NULL && strcmp(temp_downtime->comment, cmnt) != 0)
			continue;
		if(temp_downtime->type == HOST_DOWNTIME) {
			/* If service is specified, then do not delete the host downtime */
			if(service_description != NULL)
				continue;
			if(hostname != NULL && strcmp(temp_downtime->host_name, hostname) != 0)
				continue;
			}
		else if(temp_downtime->type == SERVICE_DOWNTIME) {
			if(hostname != NULL && strcmp(temp_downtime->host_name, hostname) != 0)
				continue;
			if(service_description != NULL && strcmp(temp_downtime->service_description, service_description) != 0)
				continue;
			}

		downtime_cpy = malloc(sizeof(scheduled_downtime));
		memcpy(downtime_cpy, temp_downtime, sizeof(scheduled_downtime));
		prepend_object_to_objectlist(&matches, downtime_cpy);
		deleted++;
		}

	for(tmp_match = matches; tmp_match != NULL; tmp_match = tmp_match->next) {
		temp_downtime = (scheduled_downtime *)tmp_match->object_ptr;
		unschedule_downtime(temp_downtime->type, temp_downtime->downtime_id);
		my_free(temp_downtime);
		}

	free_objectlist(&matches);

	return deleted;
	}

#endif





/******************************************************************/
/******************** ADDITION FUNCTIONS **************************/
/******************************************************************/

/* adds a host downtime entry to the list in memory */
int add_host_downtime(char *host_name, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t flex_downtime_start, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id, int is_in_effect, int start_notification_sent){
	return add_downtime(HOST_DOWNTIME, host_name, NULL, entry_time, author, comment_data, start_time, flex_downtime_start, end_time, fixed, triggered_by, duration, downtime_id, is_in_effect, start_notification_sent);
	}


/* adds a service downtime entry to the list in memory */
int add_service_downtime(char *host_name, char *svc_description, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t flex_downtime_start, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id, int is_in_effect, int start_notification_sent){
	return add_downtime(SERVICE_DOWNTIME, host_name, svc_description, entry_time, author, comment_data, start_time, flex_downtime_start, end_time, fixed, triggered_by, duration, downtime_id, is_in_effect, start_notification_sent);
	}


/* adds a host or service downtime entry to the list in memory */
int add_downtime(int downtime_type, char *host_name, char *svc_description, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t flex_downtime_start, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id, int is_in_effect, int start_notification_sent){
	scheduled_downtime *new_downtime = NULL;
	int result = OK;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "add_downtime()\n");

	/* we don't have enough info */
	if(host_name == NULL || (downtime_type == SERVICE_DOWNTIME && svc_description == NULL)) {
		log_debug_info(DEBUGL_DOWNTIME, 1,
				"Host name (%s) or service description (%s) is null\n",
				((NULL == host_name) ? "null" : host_name),
				((NULL == svc_description) ? "null" : svc_description));
		return ERROR;
		}

	/* allocate memory for the downtime */
	if((new_downtime = (scheduled_downtime *)calloc(1, sizeof(scheduled_downtime))) == NULL) {
		log_debug_info(DEBUGL_DOWNTIME, 1,
				"Unable to allocate memory for new downtime\n");
		return ERROR;
		}

	/* duplicate vars */
	if((new_downtime->host_name = (char *)strdup(host_name)) == NULL) {
		log_debug_info(DEBUGL_DOWNTIME, 1,
				"Unable to allocate memory for new downtime's host name\n");
		result = ERROR;
		}
	if(downtime_type == SERVICE_DOWNTIME) {
		if((new_downtime->service_description = (char *)strdup(svc_description)) == NULL) {
			log_debug_info(DEBUGL_DOWNTIME, 1,
					"Unable to allocate memory for new downtime's service description\n");
			result = ERROR;
			}
		}
	if(author) {
		if((new_downtime->author = (char *)strdup(author)) == NULL) {
			log_debug_info(DEBUGL_DOWNTIME, 1,
					"Unable to allocate memory for new downtime's author\n");
			result = ERROR;
			}
		}
	if(comment_data) {
		if((new_downtime->comment = (char *)strdup(comment_data)) == NULL) {
			log_debug_info(DEBUGL_DOWNTIME, 1,
					"Unable to allocate memory for new downtime's comment\n");
			result = ERROR;
			}
		}

	new_downtime->type = downtime_type;
	new_downtime->entry_time = entry_time;
	new_downtime->start_time = start_time;
	new_downtime->flex_downtime_start = flex_downtime_start;
	new_downtime->end_time = end_time;
	new_downtime->fixed = (fixed > 0) ? TRUE : FALSE;
	new_downtime->triggered_by = triggered_by;
	new_downtime->duration = duration;
	new_downtime->downtime_id = downtime_id;
	new_downtime->is_in_effect=is_in_effect;
	new_downtime->start_notification_sent=start_notification_sent;
#ifdef NSCORE
	new_downtime->start_event = ( timed_event *)0;
	new_downtime->stop_event = ( timed_event *)0;
#endif
	result = downtime_add(new_downtime);
	if (result) {
		if (new_downtime->type == SERVICE_DOWNTIME) {
			log_debug_info(DEBUGL_DOWNTIME, 0, "Failed to add downtime for service '%s' on host '%s': %s\n",
			               new_downtime->service_description, new_downtime->host_name, dt_strerror(result));
		}
		else {
			log_debug_info(DEBUGL_DOWNTIME, 0, "Failed to add downtime for host '%s': %s\n", new_downtime->host_name, dt_strerror(result));
		}
		result = ERROR;
	}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_downtime->comment);
		my_free(new_downtime->author);
		my_free(new_downtime->service_description);
		my_free(new_downtime->host_name);
		my_free(new_downtime);
		return ERROR;
		}

#ifdef NSCORE
#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_downtime_data(NEBTYPE_DOWNTIME_LOAD, NEBFLAG_NONE, NEBATTR_NONE, downtime_type, host_name, svc_description, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, downtime_id, NULL);
#endif
#endif

	return OK;
	}

int sort_downtime(void) {
	scheduled_downtime **array, *temp_downtime;
	unsigned long i = 0, unsorted_downtimes = 0;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "sort_downtime()\n");

	if(!defer_downtime_sorting)
		return OK;
	defer_downtime_sorting = 0;

	temp_downtime = scheduled_downtime_list;
	while(temp_downtime != NULL) {
		temp_downtime = temp_downtime->next;
		unsorted_downtimes++;
		}

	if(!unsorted_downtimes)
		return OK;

	if(!(array = malloc(sizeof(*array) * unsorted_downtimes)))
		return ERROR;
	while(scheduled_downtime_list) {
		array[i++] = scheduled_downtime_list;
		scheduled_downtime_list = scheduled_downtime_list->next;
		}

	qsort((void *)array, i, sizeof(*array), downtime_compar);
	scheduled_downtime_list = temp_downtime = array[0];
	temp_downtime->prev = NULL;
	for(i = 1; i < unsorted_downtimes; i++) {
		temp_downtime->next = array[i];
		temp_downtime = temp_downtime->next;
		temp_downtime->prev = array[i-1];
		}
	temp_downtime->next = NULL;
	my_free(array);
	return OK;
	}



/******************************************************************/
/************************ SEARCH FUNCTIONS ************************/
/******************************************************************/

/* finds a specific downtime entry */
scheduled_downtime *find_downtime(int type, unsigned long downtime_id) {
	scheduled_downtime *dt = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "find_downtime()\n");

	dt = fanout_get(dt_fanout, downtime_id);
	if (dt && (type == ANY_DOWNTIME || type == dt->type))
		return dt;
	return NULL;
	}


/* finds a specific host downtime entry */
scheduled_downtime *find_host_downtime(unsigned long downtime_id) {

	return find_downtime(HOST_DOWNTIME, downtime_id);
	}


/* finds a specific service downtime entry */
scheduled_downtime *find_service_downtime(unsigned long downtime_id) {

	return find_downtime(SERVICE_DOWNTIME, downtime_id);
	}



/******************************************************************/
/********************* CLEANUP FUNCTIONS **************************/
/******************************************************************/

/* frees memory allocated for the scheduled downtime data */
void free_downtime_data(void) {
	scheduled_downtime *this_downtime = NULL;
	scheduled_downtime *next_downtime = NULL;

	fanout_destroy(dt_fanout, NULL);

	/* free memory for the scheduled_downtime list */
	for(this_downtime = scheduled_downtime_list; this_downtime != NULL; this_downtime = next_downtime) {
		next_downtime = this_downtime->next;
		my_free(this_downtime->host_name);
		my_free(this_downtime->service_description);
		my_free(this_downtime->author);
		my_free(this_downtime->comment);
		my_free(this_downtime);
		}

	/* reset list pointer */
	scheduled_downtime_list = NULL;

	return;
	}
