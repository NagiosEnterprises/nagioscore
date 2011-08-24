/*****************************************************************************
 *
 * OBJECTS.C - Object addition and search functions for Nagios
 *
 * Copyright (c) 1999-2008 Ethan Galstad (egalstad@nagios.org)
 * Last Modified: 11-30-2008
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
#include "../include/skiplist.h"

#ifdef NSCORE
#include "../include/nagios.h"
#endif

#ifdef NSCGI
#include "../include/cgiutils.h"
#endif

/**** IMPLEMENTATION-SPECIFIC HEADER FILES ****/

#ifdef USE_XODTEMPLATE                          /* template-based routines */
#include "../xdata/xodtemplate.h"
#endif


host            *host_list = NULL, *host_list_tail = NULL;
service         *service_list = NULL, *service_list_tail = NULL;
contact		*contact_list = NULL, *contact_list_tail = NULL;
contactgroup	*contactgroup_list = NULL, *contactgroup_list_tail = NULL;
hostgroup	*hostgroup_list = NULL, *hostgroup_list_tail = NULL;
servicegroup    *servicegroup_list = NULL, *servicegroup_list_tail = NULL;
command         *command_list = NULL, *command_list_tail = NULL;
timeperiod      *timeperiod_list = NULL, *timeperiod_list_tail = NULL;
serviceescalation *serviceescalation_list = NULL, *serviceescalation_list_tail = NULL;
servicedependency *servicedependency_list = NULL, *servicedependency_list_tail = NULL;
hostdependency  *hostdependency_list = NULL, *hostdependency_list_tail = NULL;
hostescalation  *hostescalation_list = NULL, *hostescalation_list_tail = NULL;

skiplist *object_skiplists[NUM_OBJECT_SKIPLISTS];


#ifdef NSCORE
int __nagios_object_structure_version = CURRENT_OBJECT_STRUCTURE_VERSION;
extern int use_precached_objects;
#endif



/******************************************************************/
/******* TOP-LEVEL HOST CONFIGURATION DATA INPUT FUNCTION *********/
/******************************************************************/


/* read all host configuration data from external source */
int read_object_config_data(char *main_config_file, int options, int cache, int precache) {
	int result = OK;

	/* initialize object skiplists */
	init_object_skiplists();

	/********* IMPLEMENTATION-SPECIFIC INPUT FUNCTION ********/
#ifdef USE_XODTEMPLATE
	/* read in data from all text host config files (template-based) */
	result = xodtemplate_read_config_data(main_config_file, options, cache, precache);
	if(result != OK)
		return ERROR;
#endif

	return result;
	}



/******************************************************************/
/******************** SKIPLIST FUNCTIONS **************************/
/******************************************************************/

int init_object_skiplists(void) {
	int x = 0;

	for(x = 0; x < NUM_OBJECT_SKIPLISTS; x++)
		object_skiplists[x] = NULL;

	object_skiplists[HOST_SKIPLIST] = skiplist_new(15, 0.5, FALSE, FALSE, skiplist_compare_host);
	object_skiplists[SERVICE_SKIPLIST] = skiplist_new(15, 0.5, FALSE, FALSE, skiplist_compare_service);

	object_skiplists[COMMAND_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, skiplist_compare_command);
	object_skiplists[TIMEPERIOD_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, skiplist_compare_timeperiod);
	object_skiplists[CONTACT_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, skiplist_compare_contact);
	object_skiplists[CONTACTGROUP_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, skiplist_compare_contactgroup);
	object_skiplists[HOSTGROUP_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, skiplist_compare_hostgroup);
	object_skiplists[SERVICEGROUP_SKIPLIST] = skiplist_new(10, 0.5, FALSE, FALSE, skiplist_compare_servicegroup);

	object_skiplists[HOSTESCALATION_SKIPLIST] = skiplist_new(15, 0.5, TRUE, FALSE, skiplist_compare_hostescalation);
	object_skiplists[SERVICEESCALATION_SKIPLIST] = skiplist_new(15, 0.5, TRUE, FALSE, skiplist_compare_serviceescalation);
	object_skiplists[HOSTDEPENDENCY_SKIPLIST] = skiplist_new(15, 0.5, TRUE, FALSE, skiplist_compare_hostdependency);
	object_skiplists[SERVICEDEPENDENCY_SKIPLIST] = skiplist_new(15, 0.5, TRUE, FALSE, skiplist_compare_servicedependency);

	return OK;
	}



int free_object_skiplists(void) {
	int x = 0;

	for(x = 0; x < NUM_OBJECT_SKIPLISTS; x++)
		skiplist_free(&object_skiplists[x]);

	return OK;
	}


int skiplist_compare_text(const char *val1a, const char *val1b, const char *val2a, const char *val2b) {
	int result = 0;

	/* check first name */
	if(val1a == NULL && val2a == NULL)
		result = 0;
	else if(val1a == NULL)
		result = 1;
	else if(val2a == NULL)
		result = -1;
	else
		result = strcmp(val1a, val2a);

	/* check second name if necessary */
	if(result == 0) {
		if(val1b == NULL && val2b == NULL)
			result = 0;
		else if(val1b == NULL)
			result = 1;
		else if(val2b == NULL)
			result = -1;
		else
			result = strcmp(val1b, val2b);
		}

	return result;
	}


int skiplist_compare_host(void *a, void *b) {
	host *oa = NULL;
	host *ob = NULL;

	oa = (host *)a;
	ob = (host *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}


int skiplist_compare_service(void *a, void *b) {
	service *oa = NULL;
	service *ob = NULL;

	oa = (service *)a;
	ob = (service *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->host_name, oa->description, ob->host_name, ob->description);
	}


int skiplist_compare_command(void *a, void *b) {
	command *oa = NULL;
	command *ob = NULL;

	oa = (command *)a;
	ob = (command *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}


int skiplist_compare_timeperiod(void *a, void *b) {
	timeperiod *oa = NULL;
	timeperiod *ob = NULL;

	oa = (timeperiod *)a;
	ob = (timeperiod *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}


int skiplist_compare_contact(void *a, void *b) {
	contact *oa = NULL;
	contact *ob = NULL;

	oa = (contact *)a;
	ob = (contact *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->name, NULL, ob->name, NULL);
	}


int skiplist_compare_contactgroup(void *a, void *b) {
	contactgroup *oa = NULL;
	contactgroup *ob = NULL;

	oa = (contactgroup *)a;
	ob = (contactgroup *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->group_name, NULL, ob->group_name, NULL);
	}


int skiplist_compare_hostgroup(void *a, void *b) {
	hostgroup *oa = NULL;
	hostgroup *ob = NULL;

	oa = (hostgroup *)a;
	ob = (hostgroup *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->group_name, NULL, ob->group_name, NULL);
	}


int skiplist_compare_servicegroup(void *a, void *b) {
	servicegroup *oa = NULL;
	servicegroup *ob = NULL;

	oa = (servicegroup *)a;
	ob = (servicegroup *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->group_name, NULL, ob->group_name, NULL);
	}


int skiplist_compare_hostescalation(void *a, void *b) {
	hostescalation *oa = NULL;
	hostescalation *ob = NULL;

	oa = (hostescalation *)a;
	ob = (hostescalation *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->host_name, NULL, ob->host_name, NULL);
	}


int skiplist_compare_serviceescalation(void *a, void *b) {
	serviceescalation *oa = NULL;
	serviceescalation *ob = NULL;

	oa = (serviceescalation *)a;
	ob = (serviceescalation *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->host_name, oa->description, ob->host_name, ob->description);
	}


int skiplist_compare_hostdependency(void *a, void *b) {
	hostdependency *oa = NULL;
	hostdependency *ob = NULL;

	oa = (hostdependency *)a;
	ob = (hostdependency *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->dependent_host_name, NULL, ob->dependent_host_name, NULL);
	}


int skiplist_compare_servicedependency(void *a, void *b) {
	servicedependency *oa = NULL;
	servicedependency *ob = NULL;

	oa = (servicedependency *)a;
	ob = (servicedependency *)b;

	if(oa == NULL && ob == NULL)
		return 0;
	if(oa == NULL)
		return 1;
	if(ob == NULL)
		return -1;

	return skiplist_compare_text(oa->dependent_host_name, oa->dependent_service_description, ob->dependent_host_name, ob->dependent_service_description);
	}


int get_host_count(void) {

	if(object_skiplists[HOST_SKIPLIST])
		return object_skiplists[HOST_SKIPLIST]->items;

	return 0;
	}


int get_service_count(void) {

	if(object_skiplists[SERVICE_SKIPLIST])
		return object_skiplists[SERVICE_SKIPLIST]->items;

	return 0;
	}




/******************************************************************/
/**************** OBJECT ADDITION FUNCTIONS ***********************/
/******************************************************************/



/* add a new timeperiod to the list in memory */
timeperiod *add_timeperiod(char *name, char *alias) {
	timeperiod *new_timeperiod = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if((name == NULL || !strcmp(name, "")) || (alias == NULL || !strcmp(alias, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Name or alias for timeperiod is NULL\n");
		return NULL;
		}

	/* allocate memory for the new timeperiod */
	if((new_timeperiod = calloc(1, sizeof(timeperiod))) == NULL)
		return NULL;

	/* copy string vars */
	if((new_timeperiod->name = (char *)strdup(name)) == NULL)
		result = ERROR;
	if((new_timeperiod->alias = (char *)strdup(alias)) == NULL)
		result = ERROR;

	/* add new timeperiod to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[TIMEPERIOD_SKIPLIST], (void *)new_timeperiod);
		switch(result) {
			case SKIPLIST_ERROR_DUPLICATE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Timeperiod '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add timeperiod '%s' to skiplist\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_timeperiod->alias);
		my_free(new_timeperiod->name);
		my_free(new_timeperiod);
		return NULL;
		}

	/* timeperiods are registered alphabetically, so add new items to tail of list */
	if(timeperiod_list == NULL) {
		timeperiod_list = new_timeperiod;
		timeperiod_list_tail = timeperiod_list;
		}
	else {
		timeperiod_list_tail->next = new_timeperiod;
		timeperiod_list_tail = new_timeperiod;
		}

	return new_timeperiod;
	}




/* adds a new exclusion to a timeperiod */
timeperiodexclusion *add_exclusion_to_timeperiod(timeperiod *period, char *name) {
	timeperiodexclusion *new_timeperiodexclusion = NULL;

	/* make sure we have enough data */
	if(period == NULL || name == NULL)
		return NULL;

	if((new_timeperiodexclusion = (timeperiodexclusion *)malloc(sizeof(timeperiodexclusion))) == NULL)
		return NULL;

	new_timeperiodexclusion->timeperiod_name = (char *)strdup(name);

	new_timeperiodexclusion->next = period->exclusions;
	period->exclusions = new_timeperiodexclusion;

	return new_timeperiodexclusion;
	}




/* add a new timerange to a timeperiod */
timerange *add_timerange_to_timeperiod(timeperiod *period, int day, unsigned long start_time, unsigned long end_time) {
	timerange *new_timerange = NULL;

	/* make sure we have the data we need */
	if(period == NULL)
		return NULL;

	if(day < 0 || day > 6) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Day %d is not valid for timeperiod '%s'\n", day, period->name);
		return NULL;
		}
	if(start_time < 0 || start_time > 86400) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Start time %lu on day %d is not valid for timeperiod '%s'\n", start_time, day, period->name);
		return NULL;
		}
	if(end_time < 0 || end_time > 86400) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: End time %lu on day %d is not value for timeperiod '%s'\n", end_time, day, period->name);
		return NULL;
		}

	/* allocate memory for the new time range */
	if((new_timerange = malloc(sizeof(timerange))) == NULL)
		return NULL;

	new_timerange->range_start = start_time;
	new_timerange->range_end = end_time;

	/* add the new time range to the head of the range list for this day */
	new_timerange->next = period->days[day];
	period->days[day] = new_timerange;

	return new_timerange;
	}


/* add a new exception to a timeperiod */
daterange *add_exception_to_timeperiod(timeperiod *period, int type, int syear, int smon, int smday, int swday, int swday_offset, int eyear, int emon, int emday, int ewday, int ewday_offset, int skip_interval) {
	daterange *new_daterange = NULL;

	/* make sure we have the data we need */
	if(period == NULL)
		return NULL;

	/* allocate memory for the date range range */
	if((new_daterange = malloc(sizeof(daterange))) == NULL)
		return NULL;

	new_daterange->times = NULL;
	new_daterange->next = NULL;

	new_daterange->type = type;
	new_daterange->syear = syear;
	new_daterange->smon = smon;
	new_daterange->smday = smday;
	new_daterange->swday = swday;
	new_daterange->swday_offset = swday_offset;
	new_daterange->eyear = eyear;
	new_daterange->emon = emon;
	new_daterange->emday = emday;
	new_daterange->ewday = ewday;
	new_daterange->ewday_offset = ewday_offset;
	new_daterange->skip_interval = skip_interval;

	/* add the new date range to the head of the range list for this exception type */
	new_daterange->next = period->exceptions[type];
	period->exceptions[type] = new_daterange;

	return new_daterange;
	}



/* add a new timerange to a daterange */
timerange *add_timerange_to_daterange(daterange *drange, unsigned long start_time, unsigned long end_time) {
	timerange *new_timerange = NULL;

	/* make sure we have the data we need */
	if(drange == NULL)
		return NULL;

	if(start_time < 0 || start_time > 86400) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Start time %lu is not valid for timeperiod\n", start_time);
		return NULL;
		}
	if(end_time < 0 || end_time > 86400) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: End time %lu is not value for timeperiod\n", end_time);
		return NULL;
		}

	/* allocate memory for the new time range */
	if((new_timerange = malloc(sizeof(timerange))) == NULL)
		return NULL;

	new_timerange->range_start = start_time;
	new_timerange->range_end = end_time;

	/* add the new time range to the head of the range list for this date range */
	new_timerange->next = drange->times;
	drange->times = new_timerange;

	return new_timerange;
	}



/* add a new host definition */
host *add_host(char *name, char *display_name, char *alias, char *address, char *check_period, int initial_state, double check_interval, double retry_interval, int max_attempts, int notify_up, int notify_down, int notify_unreachable, int notify_flapping, int notify_downtime, double notification_interval, double first_notification_delay, char *notification_period, int notifications_enabled, char *check_command, int checks_enabled, int accept_passive_checks, char *event_handler, int event_handler_enabled, int flap_detection_enabled, double low_flap_threshold, double high_flap_threshold, int flap_detection_on_up, int flap_detection_on_down, int flap_detection_on_unreachable, int stalk_on_up, int stalk_on_down, int stalk_on_unreachable, int process_perfdata, int failure_prediction_enabled, char *failure_prediction_options, int check_freshness, int freshness_threshold, char *notes, char *notes_url, char *action_url, char *icon_image, char *icon_image_alt, char *vrml_image, char *statusmap_image, int x_2d, int y_2d, int have_2d_coords, double x_3d, double y_3d, double z_3d, int have_3d_coords, int should_be_drawn, int retain_status_information, int retain_nonstatus_information, int obsess_over_host) {
	host *new_host = NULL;
	int result = OK;
#ifdef NSCORE
	int x = 0;
#endif

	/* make sure we have the data we need */
	if((name == NULL || !strcmp(name, "")) || (address == NULL || !strcmp(address, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Host name or address is NULL\n");
		return NULL;
		}

	/* check values */
	if(max_attempts <= 0) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid max_check_attempts value for host '%s'\n", name);
		return NULL;
		}
	if(check_interval < 0) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid check_interval value for host '%s'\n", name);
		return NULL;
		}
	if(notification_interval < 0) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid notification_interval value for host '%s'\n", name);
		return NULL;
		}
	if(first_notification_delay < 0) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid first_notification_delay value for host '%s'\n", name);
		return NULL;
		}
	if(freshness_threshold < 0) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid freshness_threshold value for host '%s'\n", name);
		return NULL;
		}

	/* allocate memory for a new host */
	if((new_host = (host *)calloc(1, sizeof(host))) == NULL)
		return NULL;

	/* duplicate string vars */
	if((new_host->name = (char *)strdup(name)) == NULL)
		result = ERROR;
	if((new_host->display_name = (char *)strdup((display_name == NULL) ? name : display_name)) == NULL)
		result = ERROR;
	if((new_host->alias = (char *)strdup((alias == NULL) ? name : alias)) == NULL)
		result = ERROR;
	if((new_host->address = (char *)strdup(address)) == NULL)
		result = ERROR;
	if(check_period) {
		if((new_host->check_period = (char *)strdup(check_period)) == NULL)
			result = ERROR;
		}
	if(notification_period) {
		if((new_host->notification_period = (char *)strdup(notification_period)) == NULL)
			result = ERROR;
		}
	if(check_command) {
		if((new_host->host_check_command = (char *)strdup(check_command)) == NULL)
			result = ERROR;
		}
	if(event_handler) {
		if((new_host->event_handler = (char *)strdup(event_handler)) == NULL)
			result = ERROR;
		}
	if(failure_prediction_options) {
		if((new_host->failure_prediction_options = (char *)strdup(failure_prediction_options)) == NULL)
			result = ERROR;
		}
	if(notes) {
		if((new_host->notes = (char *)strdup(notes)) == NULL)
			result = ERROR;
		}
	if(notes_url) {
		if((new_host->notes_url = (char *)strdup(notes_url)) == NULL)
			result = ERROR;
		}
	if(action_url) {
		if((new_host->action_url = (char *)strdup(action_url)) == NULL)
			result = ERROR;
		}
	if(icon_image) {
		if((new_host->icon_image = (char *)strdup(icon_image)) == NULL)
			result = ERROR;
		}
	if(icon_image_alt) {
		if((new_host->icon_image_alt = (char *)strdup(icon_image_alt)) == NULL)
			result = ERROR;
		}
	if(vrml_image) {
		if((new_host->vrml_image = (char *)strdup(vrml_image)) == NULL)
			result = ERROR;
		}
	if(statusmap_image) {
		if((new_host->statusmap_image = (char *)strdup(statusmap_image)) == NULL)
			result = ERROR;
		}


	/* duplicate non-string vars */
	new_host->max_attempts = max_attempts;
	new_host->check_interval = check_interval;
	new_host->retry_interval = retry_interval;
	new_host->notification_interval = notification_interval;
	new_host->first_notification_delay = first_notification_delay;
	new_host->notify_on_recovery = (notify_up > 0) ? TRUE : FALSE;
	new_host->notify_on_down = (notify_down > 0) ? TRUE : FALSE;
	new_host->notify_on_unreachable = (notify_unreachable > 0) ? TRUE : FALSE;
	new_host->notify_on_flapping = (notify_flapping > 0) ? TRUE : FALSE;
	new_host->notify_on_downtime = (notify_downtime > 0) ? TRUE : FALSE;
	new_host->flap_detection_enabled = (flap_detection_enabled > 0) ? TRUE : FALSE;
	new_host->low_flap_threshold = low_flap_threshold;
	new_host->high_flap_threshold = high_flap_threshold;
	new_host->flap_detection_on_up = (flap_detection_on_up > 0) ? TRUE : FALSE;
	new_host->flap_detection_on_down = (flap_detection_on_down > 0) ? TRUE : FALSE;
	new_host->flap_detection_on_unreachable = (flap_detection_on_unreachable > 0) ? TRUE : FALSE;
	new_host->stalk_on_up = (stalk_on_up > 0) ? TRUE : FALSE;
	new_host->stalk_on_down = (stalk_on_down > 0) ? TRUE : FALSE;
	new_host->stalk_on_unreachable = (stalk_on_unreachable > 0) ? TRUE : FALSE;
	new_host->process_performance_data = (process_perfdata > 0) ? TRUE : FALSE;
	new_host->check_freshness = (check_freshness > 0) ? TRUE : FALSE;
	new_host->freshness_threshold = freshness_threshold;
	new_host->checks_enabled = (checks_enabled > 0) ? TRUE : FALSE;
	new_host->accept_passive_host_checks = (accept_passive_checks > 0) ? TRUE : FALSE;
	new_host->event_handler_enabled = (event_handler_enabled > 0) ? TRUE : FALSE;
	new_host->failure_prediction_enabled = (failure_prediction_enabled > 0) ? TRUE : FALSE;
	new_host->x_2d = x_2d;
	new_host->y_2d = y_2d;
	new_host->have_2d_coords = (have_2d_coords > 0) ? TRUE : FALSE;
	new_host->x_3d = x_3d;
	new_host->y_3d = y_3d;
	new_host->z_3d = z_3d;
	new_host->have_3d_coords = (have_3d_coords > 0) ? TRUE : FALSE;
	new_host->should_be_drawn = (should_be_drawn > 0) ? TRUE : FALSE;
	new_host->obsess_over_host = (obsess_over_host > 0) ? TRUE : FALSE;
	new_host->retain_status_information = (retain_status_information > 0) ? TRUE : FALSE;
	new_host->retain_nonstatus_information = (retain_nonstatus_information > 0) ? TRUE : FALSE;
#ifdef NSCORE
	new_host->current_state = initial_state;
	new_host->current_event_id = 0L;
	new_host->last_event_id = 0L;
	new_host->current_problem_id = 0L;
	new_host->last_problem_id = 0L;
	new_host->last_state = initial_state;
	new_host->last_hard_state = initial_state;
	new_host->check_type = HOST_CHECK_ACTIVE;
	new_host->last_host_notification = (time_t)0;
	new_host->next_host_notification = (time_t)0;
	new_host->next_check = (time_t)0;
	new_host->should_be_scheduled = TRUE;
	new_host->last_check = (time_t)0;
	new_host->current_attempt = (initial_state == HOST_UP) ? 1 : max_attempts;
	new_host->state_type = HARD_STATE;
	new_host->execution_time = 0.0;
	new_host->is_executing = FALSE;
	new_host->latency = 0.0;
	new_host->last_state_change = (time_t)0;
	new_host->last_hard_state_change = (time_t)0;
	new_host->last_time_up = (time_t)0;
	new_host->last_time_down = (time_t)0;
	new_host->last_time_unreachable = (time_t)0;
	new_host->has_been_checked = FALSE;
	new_host->is_being_freshened = FALSE;
	new_host->problem_has_been_acknowledged = FALSE;
	new_host->acknowledgement_type = ACKNOWLEDGEMENT_NONE;
	new_host->notifications_enabled = (notifications_enabled > 0) ? TRUE : FALSE;
	new_host->notified_on_down = FALSE;
	new_host->notified_on_unreachable = FALSE;
	new_host->current_notification_number = 0;
	new_host->current_notification_id = 0L;
	new_host->no_more_notifications = FALSE;
	new_host->check_flapping_recovery_notification = FALSE;
	new_host->scheduled_downtime_depth = 0;
	new_host->check_options = CHECK_OPTION_NONE;
	new_host->pending_flex_downtime = 0;
	for(x = 0; x < MAX_STATE_HISTORY_ENTRIES; x++)
		new_host->state_history[x] = STATE_OK;
	new_host->state_history_index = 0;
	new_host->last_state_history_update = (time_t)0;
	new_host->is_flapping = FALSE;
	new_host->flapping_comment_id = 0;
	new_host->percent_state_change = 0.0;
	new_host->total_services = 0;
	new_host->total_service_check_interval = 0L;
	new_host->modified_attributes = MODATTR_NONE;
	new_host->circular_path_checked = FALSE;
	new_host->contains_circular_path = FALSE;
#endif

	/* add new host to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[HOST_SKIPLIST], (void *)new_host);
		switch(result) {
			case SKIPLIST_ERROR_DUPLICATE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Host '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add host '%s' to skiplist\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
#ifdef NSCORE
		my_free(new_host->plugin_output);
		my_free(new_host->long_plugin_output);
		my_free(new_host->perf_data);
#endif
		my_free(new_host->statusmap_image);
		my_free(new_host->vrml_image);
		my_free(new_host->icon_image_alt);
		my_free(new_host->icon_image);
		my_free(new_host->action_url);
		my_free(new_host->notes_url);
		my_free(new_host->notes);
		my_free(new_host->failure_prediction_options);
		my_free(new_host->event_handler);
		my_free(new_host->host_check_command);
		my_free(new_host->notification_period);
		my_free(new_host->check_period);
		my_free(new_host->address);
		my_free(new_host->alias);
		my_free(new_host->display_name);
		my_free(new_host->name);
		my_free(new_host);
		return NULL;
		}

	/* hosts are sorted alphabetically, so add new items to tail of list */
	if(host_list == NULL) {
		host_list = new_host;
		host_list_tail = host_list;
		}
	else {
		host_list_tail->next = new_host;
		host_list_tail = new_host;
		}

	return new_host;
	}



hostsmember *add_parent_host_to_host(host *hst, char *host_name) {
	hostsmember *new_hostsmember = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(hst == NULL || host_name == NULL || !strcmp(host_name, "")) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Host is NULL or parent host name is NULL\n");
		return NULL;
		}

	/* a host cannot be a parent/child of itself */
	if(!strcmp(host_name, hst->name)) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Host '%s' cannot be a child/parent of itself\n", hst->name);
		return NULL;
		}

	/* allocate memory */
	if((new_hostsmember = (hostsmember *)calloc(1, sizeof(hostsmember))) == NULL)
		return NULL;

	/* duplicate string vars */
	if((new_hostsmember->host_name = (char *)strdup(host_name)) == NULL)
		result = ERROR;

	/* handle errors */
	if(result == ERROR) {
		my_free(new_hostsmember->host_name);
		my_free(new_hostsmember);
		return NULL;
		}

	/* add the parent host entry to the host definition */
	new_hostsmember->next = hst->parent_hosts;
	hst->parent_hosts = new_hostsmember;

	return new_hostsmember;
	}



hostsmember *add_child_link_to_host(host *hst, host *child_ptr) {
	hostsmember *new_hostsmember = NULL;

	/* make sure we have the data we need */
	if(hst == NULL || child_ptr == NULL)
		return NULL;

	/* allocate memory */
	if((new_hostsmember = (hostsmember *)malloc(sizeof(hostsmember))) == NULL)
		return NULL;

	/* initialize values */
	new_hostsmember->host_name = NULL;
#ifdef NSCORE
	new_hostsmember->host_ptr = child_ptr;
#endif

	/* add the child entry to the host definition */
	new_hostsmember->next = hst->child_hosts;
	hst->child_hosts = new_hostsmember;

	return new_hostsmember;
	}



servicesmember *add_service_link_to_host(host *hst, service *service_ptr) {
	servicesmember *new_servicesmember = NULL;

	/* make sure we have the data we need */
	if(hst == NULL || service_ptr == NULL)
		return NULL;

	/* allocate memory */
	if((new_servicesmember = (servicesmember *)calloc(1, sizeof(servicesmember))) == NULL)
		return NULL;

	/* initialize values */
#ifdef NSCORE
	new_servicesmember->service_ptr = service_ptr;
#endif

	/* add the child entry to the host definition */
	new_servicesmember->next = hst->services;
	hst->services = new_servicesmember;

	return new_servicesmember;
	}



/* add a new contactgroup to a host */
contactgroupsmember *add_contactgroup_to_host(host *hst, char *group_name) {
	contactgroupsmember *new_contactgroupsmember = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(hst == NULL || (group_name == NULL || !strcmp(group_name, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Host or contactgroup member is NULL\n");
		return NULL;
		}

	/* allocate memory for a new member */
	if((new_contactgroupsmember = calloc(1, sizeof(contactgroupsmember))) == NULL)
		return NULL;

	/* duplicate string vars */
	if((new_contactgroupsmember->group_name = (char *)strdup(group_name)) == NULL)
		result = ERROR;

	/* handle errors */
	if(result == ERROR) {
		my_free(new_contactgroupsmember->group_name);
		my_free(new_contactgroupsmember);
		return NULL;
		}

	/* add the new member to the head of the member list */
	new_contactgroupsmember->next = hst->contact_groups;
	hst->contact_groups = new_contactgroupsmember;;

	return new_contactgroupsmember;
	}



/* adds a contact to a host */
contactsmember *add_contact_to_host(host *hst, char *contact_name) {

	return add_contact_to_object(&hst->contacts, contact_name);
	}



/* adds a custom variable to a host */
customvariablesmember *add_custom_variable_to_host(host *hst, char *varname, char *varvalue) {

	return add_custom_variable_to_object(&hst->custom_variables, varname, varvalue);
	}



/* add a new host group to the list in memory */
hostgroup *add_hostgroup(char *name, char *alias, char *notes, char *notes_url, char *action_url) {
	hostgroup *new_hostgroup = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(name == NULL || !strcmp(name, "")) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Hostgroup name is NULL\n");
		return NULL;
		}

	/* allocate memory */
	if((new_hostgroup = (hostgroup *)calloc(1, sizeof(hostgroup))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_hostgroup->group_name = (char *)strdup(name)) == NULL)
		result = ERROR;
	if((new_hostgroup->alias = (char *)strdup((alias == NULL) ? name : alias)) == NULL)
		result = ERROR;
	if(notes) {
		if((new_hostgroup->notes = (char *)strdup(notes)) == NULL)
			result = ERROR;
		}
	if(notes_url) {
		if((new_hostgroup->notes_url = (char *)strdup(notes_url)) == NULL)
			result = ERROR;
		}
	if(action_url) {
		if((new_hostgroup->action_url = (char *)strdup(action_url)) == NULL)
			result = ERROR;
		}

	/* add new host group to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[HOSTGROUP_SKIPLIST], (void *)new_hostgroup);
		switch(result) {
			case SKIPLIST_ERROR_DUPLICATE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Hostgroup '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add hostgroup '%s' to skiplist\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_hostgroup->alias);
		my_free(new_hostgroup->group_name);
		my_free(new_hostgroup);
		return NULL;
		}

	/* hostgroups are sorted alphabetically, so add new items to tail of list */
	if(hostgroup_list == NULL) {
		hostgroup_list = new_hostgroup;
		hostgroup_list_tail = hostgroup_list;
		}
	else {
		hostgroup_list_tail->next = new_hostgroup;
		hostgroup_list_tail = new_hostgroup;
		}

	return new_hostgroup;
	}


/* add a new host to a host group */
hostsmember *add_host_to_hostgroup(hostgroup *temp_hostgroup, char *host_name) {
	hostsmember *new_member = NULL;
	hostsmember *last_member = NULL;
	hostsmember *temp_member = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(temp_hostgroup == NULL || (host_name == NULL || !strcmp(host_name, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Hostgroup or group member is NULL\n");
		return NULL;
		}

	/* allocate memory for a new member */
	if((new_member = calloc(1, sizeof(hostsmember))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_member->host_name = (char *)strdup(host_name)) == NULL)
		result = ERROR;

	/* handle errors */
	if(result == ERROR) {
		my_free(new_member->host_name);
		my_free(new_member);
		return NULL;
		}

	/* add the new member to the member list, sorted by host name */
	last_member = temp_hostgroup->members;
	for(temp_member = temp_hostgroup->members; temp_member != NULL; temp_member = temp_member->next) {
		if(strcmp(new_member->host_name, temp_member->host_name) < 0) {
			new_member->next = temp_member;
			if(temp_member == temp_hostgroup->members)
				temp_hostgroup->members = new_member;
			else
				last_member->next = new_member;
			break;
			}
		else
			last_member = temp_member;
		}
	if(temp_hostgroup->members == NULL) {
		new_member->next = NULL;
		temp_hostgroup->members = new_member;
		}
	else if(temp_member == NULL) {
		new_member->next = NULL;
		last_member->next = new_member;
		}

	return new_member;
	}


/* add a new service group to the list in memory */
servicegroup *add_servicegroup(char *name, char *alias, char *notes, char *notes_url, char *action_url) {
	servicegroup *new_servicegroup = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(name == NULL || !strcmp(name, "")) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Servicegroup name is NULL\n");
		return NULL;
		}

	/* allocate memory */
	if((new_servicegroup = (servicegroup *)calloc(1, sizeof(servicegroup))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_servicegroup->group_name = (char *)strdup(name)) == NULL)
		result = ERROR;
	if((new_servicegroup->alias = (char *)strdup((alias == NULL) ? name : alias)) == NULL)
		result = ERROR;
	if(notes) {
		if((new_servicegroup->notes = (char *)strdup(notes)) == NULL)
			result = ERROR;
		}
	if(notes_url) {
		if((new_servicegroup->notes_url = (char *)strdup(notes_url)) == NULL)
			result = ERROR;
		}
	if(action_url) {
		if((new_servicegroup->action_url = (char *)strdup(action_url)) == NULL)
			result = ERROR;
		}

	/* add new service group to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[SERVICEGROUP_SKIPLIST], (void *)new_servicegroup);
		switch(result) {
			case SKIPLIST_ERROR_DUPLICATE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Servicegroup '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add servicegroup '%s' to skiplist\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_servicegroup->alias);
		my_free(new_servicegroup->group_name);
		my_free(new_servicegroup);
		return NULL;
		}

	/* servicegroups are sorted alphabetically, so add new items to tail of list */
	if(servicegroup_list == NULL) {
		servicegroup_list = new_servicegroup;
		servicegroup_list_tail = servicegroup_list;
		}
	else {
		servicegroup_list_tail->next = new_servicegroup;
		servicegroup_list_tail = new_servicegroup;
		}

	return new_servicegroup;
	}


/* add a new service to a service group */
servicesmember *add_service_to_servicegroup(servicegroup *temp_servicegroup, char *host_name, char *svc_description) {
	servicesmember *new_member = NULL;
	servicesmember *last_member = NULL;
	servicesmember *temp_member = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(temp_servicegroup == NULL || (host_name == NULL || !strcmp(host_name, "")) || (svc_description == NULL || !strcmp(svc_description, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Servicegroup or group member is NULL\n");
		return NULL;
		}

	/* allocate memory for a new member */
	if((new_member = calloc(1, sizeof(servicesmember))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_member->host_name = (char *)strdup(host_name)) == NULL)
		result = ERROR;
	if((new_member->service_description = (char *)strdup(svc_description)) == NULL)
		result = ERROR;

	/* handle errors */
	if(result == ERROR) {
		my_free(new_member->service_description);
		my_free(new_member->host_name);
		my_free(new_member);
		return NULL;
		}

	/* add new member to member list, sorted by host name then service description */
	last_member = temp_servicegroup->members;
	for(temp_member = temp_servicegroup->members; temp_member != NULL; temp_member = temp_member->next) {

		if(strcmp(new_member->host_name, temp_member->host_name) < 0) {
			new_member->next = temp_member;
			if(temp_member == temp_servicegroup->members)
				temp_servicegroup->members = new_member;
			else
				last_member->next = new_member;
			break;
			}

		else if(strcmp(new_member->host_name, temp_member->host_name) == 0 && strcmp(new_member->service_description, temp_member->service_description) < 0) {
			new_member->next = temp_member;
			if(temp_member == temp_servicegroup->members)
				temp_servicegroup->members = new_member;
			else
				last_member->next = new_member;
			break;
			}

		else
			last_member = temp_member;
		}
	if(temp_servicegroup->members == NULL) {
		new_member->next = NULL;
		temp_servicegroup->members = new_member;
		}
	else if(temp_member == NULL) {
		new_member->next = NULL;
		last_member->next = new_member;
		}

	return new_member;
	}


/* add a new contact to the list in memory */
contact *add_contact(char *name, char *alias, char *email, char *pager, char **addresses, char *svc_notification_period, char *host_notification_period, int notify_service_ok, int notify_service_critical, int notify_service_warning, int notify_service_unknown, int notify_service_flapping, int notify_service_downtime, int notify_host_up, int notify_host_down, int notify_host_unreachable, int notify_host_flapping, int notify_host_downtime, int host_notifications_enabled, int service_notifications_enabled, int can_submit_commands, int retain_status_information, int retain_nonstatus_information) {
	contact *new_contact = NULL;
	int x = 0;
	int result = OK;

	/* make sure we have the data we need */
	if(name == NULL || !strcmp(name, "")) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact name is NULL\n");
		return NULL;
		}

	/* allocate memory for a new contact */
	if((new_contact = (contact *)calloc(1, sizeof(contact))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_contact->name = (char *)strdup(name)) == NULL)
		result = ERROR;
	if((new_contact->alias = (char *)strdup((alias == NULL) ? name : alias)) == NULL)
		result = ERROR;
	if(email) {
		if((new_contact->email = (char *)strdup(email)) == NULL)
			result = ERROR;
		}
	if(pager) {
		if((new_contact->pager = (char *)strdup(pager)) == NULL)
			result = ERROR;
		}
	if(svc_notification_period) {
		if((new_contact->service_notification_period = (char *)strdup(svc_notification_period)) == NULL)
			result = ERROR;
		}
	if(host_notification_period) {
		if((new_contact->host_notification_period = (char *)strdup(host_notification_period)) == NULL)
			result = ERROR;
		}
	if(addresses) {
		for(x = 0; x < MAX_CONTACT_ADDRESSES; x++) {
			if(addresses[x]) {
				if((new_contact->address[x] = (char *)strdup(addresses[x])) == NULL)
					result = ERROR;
				}
			}
		}

	new_contact->notify_on_service_recovery = (notify_service_ok > 0) ? TRUE : FALSE;
	new_contact->notify_on_service_critical = (notify_service_critical > 0) ? TRUE : FALSE;
	new_contact->notify_on_service_warning = (notify_service_warning > 0) ? TRUE : FALSE;
	new_contact->notify_on_service_unknown = (notify_service_unknown > 0) ? TRUE : FALSE;
	new_contact->notify_on_service_flapping = (notify_service_flapping > 0) ? TRUE : FALSE;
	new_contact->notify_on_service_downtime = (notify_service_downtime > 0) ? TRUE : FALSE;
	new_contact->notify_on_host_recovery = (notify_host_up > 0) ? TRUE : FALSE;
	new_contact->notify_on_host_down = (notify_host_down > 0) ? TRUE : FALSE;
	new_contact->notify_on_host_unreachable = (notify_host_unreachable > 0) ? TRUE : FALSE;
	new_contact->notify_on_host_flapping = (notify_host_flapping > 0) ? TRUE : FALSE;
	new_contact->notify_on_host_downtime = (notify_host_downtime > 0) ? TRUE : FALSE;
	new_contact->host_notifications_enabled = (host_notifications_enabled > 0) ? TRUE : FALSE;
	new_contact->service_notifications_enabled = (service_notifications_enabled > 0) ? TRUE : FALSE;
	new_contact->can_submit_commands = (can_submit_commands > 0) ? TRUE : FALSE;
	new_contact->retain_status_information = (retain_status_information > 0) ? TRUE : FALSE;
	new_contact->retain_nonstatus_information = (retain_nonstatus_information > 0) ? TRUE : FALSE;
#ifdef NSCORE
	new_contact->last_host_notification = (time_t)0L;
	new_contact->last_service_notification = (time_t)0L;
	new_contact->modified_attributes = MODATTR_NONE;
	new_contact->modified_host_attributes = MODATTR_NONE;
	new_contact->modified_service_attributes = MODATTR_NONE;

	new_contact->host_notification_period_ptr = NULL;
	new_contact->service_notification_period_ptr = NULL;
	new_contact->contactgroups_ptr = NULL;
#endif

	/* add new contact to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[CONTACT_SKIPLIST], (void *)new_contact);
		switch(result) {
			case SKIPLIST_ERROR_DUPLICATE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contact '%s' to skiplist\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		for(x = 0; x < MAX_CONTACT_ADDRESSES; x++)
			my_free(new_contact->address[x]);
		my_free(new_contact->name);
		my_free(new_contact->alias);
		my_free(new_contact->email);
		my_free(new_contact->pager);
		my_free(new_contact->service_notification_period);
		my_free(new_contact->host_notification_period);
		my_free(new_contact);
		return NULL;
		}

	/* contacts are sorted alphabetically, so add new items to tail of list */
	if(contact_list == NULL) {
		contact_list = new_contact;
		contact_list_tail = contact_list;
		}
	else {
		contact_list_tail->next = new_contact;
		contact_list_tail = new_contact;
		}

	return new_contact;
	}



/* adds a host notification command to a contact definition */
commandsmember *add_host_notification_command_to_contact(contact *cntct, char *command_name) {
	commandsmember *new_commandsmember = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(cntct == NULL || (command_name == NULL || !strcmp(command_name, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact or host notification command is NULL\n");
		return NULL;
		}

	/* allocate memory */
	if((new_commandsmember = calloc(1, sizeof(commandsmember))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_commandsmember->command = (char *)strdup(command_name)) == NULL)
		result = ERROR;

	/* handle errors */
	if(result == ERROR) {
		my_free(new_commandsmember->command);
		my_free(new_commandsmember);
		return NULL;
		}

	/* add the notification command */
	new_commandsmember->next = cntct->host_notification_commands;
	cntct->host_notification_commands = new_commandsmember;

	return new_commandsmember;
	}



/* adds a service notification command to a contact definition */
commandsmember *add_service_notification_command_to_contact(contact *cntct, char *command_name) {
	commandsmember *new_commandsmember = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(cntct == NULL || (command_name == NULL || !strcmp(command_name, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact or service notification command is NULL\n");
		return NULL;
		}

	/* allocate memory */
	if((new_commandsmember = calloc(1, sizeof(commandsmember))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_commandsmember->command = (char *)strdup(command_name)) == NULL)
		result = ERROR;

	/* handle errors */
	if(result == ERROR) {
		my_free(new_commandsmember->command);
		my_free(new_commandsmember);
		return NULL;
		}

	/* add the notification command */
	new_commandsmember->next = cntct->service_notification_commands;
	cntct->service_notification_commands = new_commandsmember;

	return new_commandsmember;
	}



/* adds a custom variable to a contact */
customvariablesmember *add_custom_variable_to_contact(contact *cntct, char *varname, char *varvalue) {

	return add_custom_variable_to_object(&cntct->custom_variables, varname, varvalue);
	}



/* add a new contact group to the list in memory */
contactgroup *add_contactgroup(char *name, char *alias) {
	contactgroup *new_contactgroup = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(name == NULL || !strcmp(name, "")) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contactgroup name is NULL\n");
		return NULL;
		}

	/* allocate memory for a new contactgroup entry */
	if((new_contactgroup = calloc(1, sizeof(contactgroup))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_contactgroup->group_name = (char *)strdup(name)) == NULL)
		result = ERROR;
	if((new_contactgroup->alias = (char *)strdup((alias == NULL) ? name : alias)) == NULL)
		result = ERROR;

	/* add new contact group to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[CONTACTGROUP_SKIPLIST], (void *)new_contactgroup);
		switch(result) {
			case SKIPLIST_ERROR_DUPLICATE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contactgroup '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contactgroup '%s' to skiplist\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_contactgroup->alias);
		my_free(new_contactgroup->group_name);
		my_free(new_contactgroup);
		return NULL;
		}

	/* contactgroups are sorted alphabetically, so add new items to tail of list */
	if(contactgroup_list == NULL) {
		contactgroup_list = new_contactgroup;
		contactgroup_list_tail = contactgroup_list;
		}
	else {
		contactgroup_list_tail->next = new_contactgroup;
		contactgroup_list_tail = new_contactgroup;
		}

	return new_contactgroup;
	}



/* add a new member to a contact group */
contactsmember *add_contact_to_contactgroup(contactgroup *grp, char *contact_name) {
	contactsmember *new_contactsmember = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(grp == NULL || (contact_name == NULL || !strcmp(contact_name, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contactgroup or contact name is NULL\n");
		return NULL;
		}

	/* allocate memory for a new member */
	if((new_contactsmember = calloc(1, sizeof(contactsmember))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_contactsmember->contact_name = (char *)strdup(contact_name)) == NULL)
		result = ERROR;

	/* handle errors */
	if(result == ERROR) {
		my_free(new_contactsmember->contact_name);
		my_free(new_contactsmember);
		return NULL;
		}

	/* add the new member to the head of the member list */
	new_contactsmember->next = grp->members;
	grp->members = new_contactsmember;

	return new_contactsmember;
	}



/* add a new service to the list in memory */
service *add_service(char *host_name, char *description, char *display_name, char *check_period, int initial_state, int max_attempts, int parallelize, int accept_passive_checks, double check_interval, double retry_interval, double notification_interval, double first_notification_delay, char *notification_period, int notify_recovery, int notify_unknown, int notify_warning, int notify_critical, int notify_flapping, int notify_downtime, int notifications_enabled, int is_volatile, char *event_handler, int event_handler_enabled, char *check_command, int checks_enabled, int flap_detection_enabled, double low_flap_threshold, double high_flap_threshold, int flap_detection_on_ok, int flap_detection_on_warning, int flap_detection_on_unknown, int flap_detection_on_critical, int stalk_on_ok, int stalk_on_warning, int stalk_on_unknown, int stalk_on_critical, int process_perfdata, int failure_prediction_enabled, char *failure_prediction_options, int check_freshness, int freshness_threshold, char *notes, char *notes_url, char *action_url, char *icon_image, char *icon_image_alt, int retain_status_information, int retain_nonstatus_information, int obsess_over_service) {
	service *new_service = NULL;
	int result = OK;
#ifdef NSCORE
	int x = 0;
#endif

	/* make sure we have everything we need */
	if((host_name == NULL || !strcmp(host_name, "")) || (description == NULL || !strcmp(description, "")) || (check_command == NULL || !strcmp(check_command, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Service description, host name, or check command is NULL\n");
		return NULL;
		}

	/* check values */
	if(max_attempts <= 0 || check_interval < 0 || retry_interval <= 0 || notification_interval < 0) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid max_attempts, check_interval, retry_interval, or notification_interval value for service '%s' on host '%s'\n", description, host_name);
		return NULL;
		}

	if(first_notification_delay < 0) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Invalid first_notification_delay value for service '%s' on host '%s'\n", description, host_name);
		return NULL;
		}

	/* allocate memory */
	if((new_service = (service *)calloc(1, sizeof(service))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_service->host_name = (char *)strdup(host_name)) == NULL)
		result = ERROR;
	if((new_service->description = (char *)strdup(description)) == NULL)
		result = ERROR;
	if((new_service->display_name = (char *)strdup((display_name == NULL) ? description : display_name)) == NULL)
		result = ERROR;
	if((new_service->service_check_command = (char *)strdup(check_command)) == NULL)
		result = ERROR;
	if(event_handler) {
		if((new_service->event_handler = (char *)strdup(event_handler)) == NULL)
			result = ERROR;
		}
	if(notification_period) {
		if((new_service->notification_period = (char *)strdup(notification_period)) == NULL)
			result = ERROR;
		}
	if(check_period) {
		if((new_service->check_period = (char *)strdup(check_period)) == NULL)
			result = ERROR;
		}
	if(failure_prediction_options) {
		if((new_service->failure_prediction_options = (char *)strdup(failure_prediction_options)) == NULL)
			result = ERROR;
		}
	if(notes) {
		if((new_service->notes = (char *)strdup(notes)) == NULL)
			result = ERROR;
		}
	if(notes_url) {
		if((new_service->notes_url = (char *)strdup(notes_url)) == NULL)
			result = ERROR;
		}
	if(action_url) {
		if((new_service->action_url = (char *)strdup(action_url)) == NULL)
			result = ERROR;
		}
	if(icon_image) {
		if((new_service->icon_image = (char *)strdup(icon_image)) == NULL)
			result = ERROR;
		}
	if(icon_image_alt) {
		if((new_service->icon_image_alt = (char *)strdup(icon_image_alt)) == NULL)
			result = ERROR;
		}

	new_service->check_interval = check_interval;
	new_service->retry_interval = retry_interval;
	new_service->max_attempts = max_attempts;
	new_service->parallelize = (parallelize > 0) ? TRUE : FALSE;
	new_service->notification_interval = notification_interval;
	new_service->first_notification_delay = first_notification_delay;
	new_service->notify_on_unknown = (notify_unknown > 0) ? TRUE : FALSE;
	new_service->notify_on_warning = (notify_warning > 0) ? TRUE : FALSE;
	new_service->notify_on_critical = (notify_critical > 0) ? TRUE : FALSE;
	new_service->notify_on_recovery = (notify_recovery > 0) ? TRUE : FALSE;
	new_service->notify_on_flapping = (notify_flapping > 0) ? TRUE : FALSE;
	new_service->notify_on_downtime = (notify_downtime > 0) ? TRUE : FALSE;
	new_service->is_volatile = (is_volatile > 0) ? TRUE : FALSE;
	new_service->flap_detection_enabled = (flap_detection_enabled > 0) ? TRUE : FALSE;
	new_service->low_flap_threshold = low_flap_threshold;
	new_service->high_flap_threshold = high_flap_threshold;
	new_service->flap_detection_on_ok = (flap_detection_on_ok > 0) ? TRUE : FALSE;
	new_service->flap_detection_on_warning = (flap_detection_on_warning > 0) ? TRUE : FALSE;
	new_service->flap_detection_on_unknown = (flap_detection_on_unknown > 0) ? TRUE : FALSE;
	new_service->flap_detection_on_critical = (flap_detection_on_critical > 0) ? TRUE : FALSE;
	new_service->stalk_on_ok = (stalk_on_ok > 0) ? TRUE : FALSE;
	new_service->stalk_on_warning = (stalk_on_warning > 0) ? TRUE : FALSE;
	new_service->stalk_on_unknown = (stalk_on_unknown > 0) ? TRUE : FALSE;
	new_service->stalk_on_critical = (stalk_on_critical > 0) ? TRUE : FALSE;
	new_service->process_performance_data = (process_perfdata > 0) ? TRUE : FALSE;
	new_service->check_freshness = (check_freshness > 0) ? TRUE : FALSE;
	new_service->freshness_threshold = freshness_threshold;
	new_service->accept_passive_service_checks = (accept_passive_checks > 0) ? TRUE : FALSE;
	new_service->event_handler_enabled = (event_handler_enabled > 0) ? TRUE : FALSE;
	new_service->checks_enabled = (checks_enabled > 0) ? TRUE : FALSE;
	new_service->retain_status_information = (retain_status_information > 0) ? TRUE : FALSE;
	new_service->retain_nonstatus_information = (retain_nonstatus_information > 0) ? TRUE : FALSE;
	new_service->notifications_enabled = (notifications_enabled > 0) ? TRUE : FALSE;
	new_service->obsess_over_service = (obsess_over_service > 0) ? TRUE : FALSE;
	new_service->failure_prediction_enabled = (failure_prediction_enabled > 0) ? TRUE : FALSE;
#ifdef NSCORE
	new_service->problem_has_been_acknowledged = FALSE;
	new_service->acknowledgement_type = ACKNOWLEDGEMENT_NONE;
	new_service->check_type = SERVICE_CHECK_ACTIVE;
	new_service->current_attempt = (initial_state == STATE_OK) ? 1 : max_attempts;
	new_service->current_state = initial_state;
	new_service->current_event_id = 0L;
	new_service->last_event_id = 0L;
	new_service->current_problem_id = 0L;
	new_service->last_problem_id = 0L;
	new_service->last_state = initial_state;
	new_service->last_hard_state = initial_state;
	new_service->state_type = HARD_STATE;
	new_service->host_problem_at_last_check = FALSE;
	new_service->check_flapping_recovery_notification = FALSE;
	new_service->next_check = (time_t)0;
	new_service->should_be_scheduled = TRUE;
	new_service->last_check = (time_t)0;
	new_service->last_notification = (time_t)0;
	new_service->next_notification = (time_t)0;
	new_service->no_more_notifications = FALSE;
	new_service->last_state_change = (time_t)0;
	new_service->last_hard_state_change = (time_t)0;
	new_service->last_time_ok = (time_t)0;
	new_service->last_time_warning = (time_t)0;
	new_service->last_time_unknown = (time_t)0;
	new_service->last_time_critical = (time_t)0;
	new_service->has_been_checked = FALSE;
	new_service->is_being_freshened = FALSE;
	new_service->notified_on_unknown = FALSE;
	new_service->notified_on_warning = FALSE;
	new_service->notified_on_critical = FALSE;
	new_service->current_notification_number = 0;
	new_service->current_notification_id = 0L;
	new_service->latency = 0.0;
	new_service->execution_time = 0.0;
	new_service->is_executing = FALSE;
	new_service->check_options = CHECK_OPTION_NONE;
	new_service->scheduled_downtime_depth = 0;
	new_service->pending_flex_downtime = 0;
	for(x = 0; x < MAX_STATE_HISTORY_ENTRIES; x++)
		new_service->state_history[x] = STATE_OK;
	new_service->state_history_index = 0;
	new_service->is_flapping = FALSE;
	new_service->flapping_comment_id = 0;
	new_service->percent_state_change = 0.0;
	new_service->modified_attributes = MODATTR_NONE;
#endif

	/* add new service to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[SERVICE_SKIPLIST], (void *)new_service);
		switch(result) {
			case SKIPLIST_ERROR_DUPLICATE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Service '%s' on host '%s' has already been defined\n", description, host_name);
				result = ERROR;
				break;
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add service '%s' on host '%s' to skiplist\n", description, host_name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
#ifdef NSCORE
		my_free(new_service->perf_data);
		my_free(new_service->plugin_output);
		my_free(new_service->long_plugin_output);
#endif
		my_free(new_service->failure_prediction_options);
		my_free(new_service->notification_period);
		my_free(new_service->event_handler);
		my_free(new_service->service_check_command);
		my_free(new_service->description);
		my_free(new_service->host_name);
		my_free(new_service->display_name);
		my_free(new_service);
		return NULL;
		}

	/* services are sorted alphabetically, so add new items to tail of list */
	if(service_list == NULL) {
		service_list = new_service;
		service_list_tail = service_list;
		}
	else {
		service_list_tail->next = new_service;
		service_list_tail = new_service;
		}

	return new_service;
	}



/* adds a contact group to a service */
contactgroupsmember *add_contactgroup_to_service(service *svc, char *group_name) {
	contactgroupsmember *new_contactgroupsmember = NULL;
	int result = OK;

	/* bail out if we weren't given the data we need */
	if(svc == NULL || (group_name == NULL || !strcmp(group_name, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Service or contactgroup name is NULL\n");
		return NULL;
		}

	/* allocate memory for the contactgroups member */
	if((new_contactgroupsmember = calloc(1, sizeof(contactgroupsmember))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_contactgroupsmember->group_name = (char *)strdup(group_name)) == NULL)
		result = ERROR;

	/* handle errors */
	if(result == ERROR) {
		my_free(new_contactgroupsmember);
		return NULL;
		}

	/* add this contactgroup to the service */
	new_contactgroupsmember->next = svc->contact_groups;
	svc->contact_groups = new_contactgroupsmember;

	return new_contactgroupsmember;
	}



/* adds a contact to a service */
contactsmember *add_contact_to_service(service *svc, char *contact_name) {

	return add_contact_to_object(&svc->contacts, contact_name);
	}



/* adds a custom variable to a service */
customvariablesmember *add_custom_variable_to_service(service *svc, char *varname, char *varvalue) {

	return add_custom_variable_to_object(&svc->custom_variables, varname, varvalue);
	}



/* add a new command to the list in memory */
command *add_command(char *name, char *value) {
	command *new_command = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if((name == NULL || !strcmp(name, "")) || (value == NULL || !strcmp(value, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Command name of command line is NULL\n");
		return NULL;
		}

	/* allocate memory for the new command */
	if((new_command = (command *)calloc(1, sizeof(command))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_command->name = (char *)strdup(name)) == NULL)
		result = ERROR;
	if((new_command->command_line = (char *)strdup(value)) == NULL)
		result = ERROR;

	/* add new command to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[COMMAND_SKIPLIST], (void *)new_command);
		switch(result) {
			case SKIPLIST_ERROR_DUPLICATE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Command '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add command '%s' to skiplist\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_command->command_line);
		my_free(new_command->name);
		my_free(new_command);
		return NULL;
		}

	/* commands are sorted alphabetically, so add new items to tail of list */
	if(command_list == NULL) {
		command_list = new_command;
		command_list_tail = command_list;
		}
	else {
		command_list_tail->next = new_command;
		command_list_tail = new_command;
		}

	return new_command;
	}



/* add a new service escalation to the list in memory */
serviceescalation *add_serviceescalation(char *host_name, char *description, int first_notification, int last_notification, double notification_interval, char *escalation_period, int escalate_on_warning, int escalate_on_unknown, int escalate_on_critical, int escalate_on_recovery) {
	serviceescalation *new_serviceescalation = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if((host_name == NULL || !strcmp(host_name, "")) || (description == NULL || !strcmp(description, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Service escalation host name or description is NULL\n");
		return NULL;
		}

#ifdef TEST
	printf("NEW SVC ESCALATION: %s/%s = %d/%d/%.3f\n", host_name, description, first_notification, last_notification, notification_interval);
#endif

	/* allocate memory for a new service escalation entry */
	if((new_serviceescalation = calloc(1, sizeof(serviceescalation))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_serviceescalation->host_name = (char *)strdup(host_name)) == NULL)
		result = ERROR;
	if((new_serviceescalation->description = (char *)strdup(description)) == NULL)
		result = ERROR;
	if(escalation_period) {
		if((new_serviceescalation->escalation_period = (char *)strdup(escalation_period)) == NULL)
			result = ERROR;
		}

	new_serviceescalation->first_notification = first_notification;
	new_serviceescalation->last_notification = last_notification;
	new_serviceescalation->notification_interval = (notification_interval <= 0) ? 0 : notification_interval;
	new_serviceescalation->escalate_on_recovery = (escalate_on_recovery > 0) ? TRUE : FALSE;
	new_serviceescalation->escalate_on_warning = (escalate_on_warning > 0) ? TRUE : FALSE;
	new_serviceescalation->escalate_on_unknown = (escalate_on_unknown > 0) ? TRUE : FALSE;
	new_serviceescalation->escalate_on_critical = (escalate_on_critical > 0) ? TRUE : FALSE;

	/* add new serviceescalation to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[SERVICEESCALATION_SKIPLIST], (void *)new_serviceescalation);
		switch(result) {
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add escalation for service '%s' on host '%s' to skiplist\n", description, host_name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_serviceescalation->host_name);
		my_free(new_serviceescalation->description);
		my_free(new_serviceescalation->escalation_period);
		my_free(new_serviceescalation);
		return NULL;
		}

	/* service escalations are sorted alphabetically, so add new items to tail of list */
	if(serviceescalation_list == NULL) {
		serviceescalation_list = new_serviceescalation;
		serviceescalation_list_tail = serviceescalation_list;
		}
	else {
		serviceescalation_list_tail->next = new_serviceescalation;
		serviceescalation_list_tail = new_serviceescalation;
		}

	return new_serviceescalation;
	}



/* adds a contact group to a service escalation */
contactgroupsmember *add_contactgroup_to_serviceescalation(serviceescalation *se, char *group_name) {
	contactgroupsmember *new_contactgroupsmember = NULL;
	int result = OK;

	/* bail out if we weren't given the data we need */
	if(se == NULL || (group_name == NULL || !strcmp(group_name, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Service escalation or contactgroup name is NULL\n");
		return NULL;
		}

	/* allocate memory for the contactgroups member */
	if((new_contactgroupsmember = (contactgroupsmember *)calloc(1, sizeof(contactgroupsmember))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_contactgroupsmember->group_name = (char *)strdup(group_name)) == NULL)
		result = ERROR;

	/* handle errors */
	if(result == ERROR) {
		my_free(new_contactgroupsmember->group_name);
		my_free(new_contactgroupsmember);
		return NULL;
		}

	/* add this contactgroup to the service escalation */
	new_contactgroupsmember->next = se->contact_groups;
	se->contact_groups = new_contactgroupsmember;

	return new_contactgroupsmember;
	}



/* adds a contact to a service escalation */
contactsmember *add_contact_to_serviceescalation(serviceescalation *se, char *contact_name) {

	return add_contact_to_object(&se->contacts, contact_name);
	}



/* adds a service dependency definition */
servicedependency *add_service_dependency(char *dependent_host_name, char *dependent_service_description, char *host_name, char *service_description, int dependency_type, int inherits_parent, int fail_on_ok, int fail_on_warning, int fail_on_unknown, int fail_on_critical, int fail_on_pending, char *dependency_period) {
	servicedependency *new_servicedependency = NULL;
	int result = OK;

	/* make sure we have what we need */
	if((host_name == NULL || !strcmp(host_name, "")) || (service_description == NULL || !strcmp(service_description, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: NULL master service description/host name in service dependency definition\n");
		return NULL;
		}
	if((dependent_host_name == NULL || !strcmp(dependent_host_name, "")) || (dependent_service_description == NULL || !strcmp(dependent_service_description, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: NULL dependent service description/host name in service dependency definition\n");
		return NULL;
		}

	/* allocate memory for a new service dependency entry */
	if((new_servicedependency = (servicedependency *)calloc(1, sizeof(servicedependency))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_servicedependency->dependent_host_name = (char *)strdup(dependent_host_name)) == NULL)
		result = ERROR;
	if((new_servicedependency->dependent_service_description = (char *)strdup(dependent_service_description)) == NULL)
		result = ERROR;
	if((new_servicedependency->host_name = (char *)strdup(host_name)) == NULL)
		result = ERROR;
	if((new_servicedependency->service_description = (char *)strdup(service_description)) == NULL)
		result = ERROR;
	if(dependency_period) {
		if((new_servicedependency->dependency_period = (char *)strdup(dependency_period)) == NULL)
			result = ERROR;
		}

	new_servicedependency->dependency_type = (dependency_type == EXECUTION_DEPENDENCY) ? EXECUTION_DEPENDENCY : NOTIFICATION_DEPENDENCY;
	new_servicedependency->inherits_parent = (inherits_parent > 0) ? TRUE : FALSE;
	new_servicedependency->fail_on_ok = (fail_on_ok == 1) ? TRUE : FALSE;
	new_servicedependency->fail_on_warning = (fail_on_warning == 1) ? TRUE : FALSE;
	new_servicedependency->fail_on_unknown = (fail_on_unknown == 1) ? TRUE : FALSE;
	new_servicedependency->fail_on_critical = (fail_on_critical == 1) ? TRUE : FALSE;
	new_servicedependency->fail_on_pending = (fail_on_pending == 1) ? TRUE : FALSE;
#ifdef NSCORE
	new_servicedependency->circular_path_checked = FALSE;
	new_servicedependency->contains_circular_path = FALSE;
#endif

	/* add new service dependency to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[SERVICEDEPENDENCY_SKIPLIST], (void *)new_servicedependency);
		switch(result) {
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add service dependency to skiplist\n");
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_servicedependency->host_name);
		my_free(new_servicedependency->service_description);
		my_free(new_servicedependency->dependent_host_name);
		my_free(new_servicedependency->dependent_service_description);
		my_free(new_servicedependency);
		return NULL;
		}

	/* service dependencies are sorted alphabetically, so add new items to tail of list */
	if(servicedependency_list == NULL) {
		servicedependency_list = new_servicedependency;
		servicedependency_list_tail = servicedependency_list;
		}
	else {
		servicedependency_list_tail->next = new_servicedependency;
		servicedependency_list_tail = new_servicedependency;
		}

	return new_servicedependency;
	}


/* adds a host dependency definition */
hostdependency *add_host_dependency(char *dependent_host_name, char *host_name, int dependency_type, int inherits_parent, int fail_on_up, int fail_on_down, int fail_on_unreachable, int fail_on_pending, char *dependency_period) {
	hostdependency *new_hostdependency = NULL;
	int result = OK;

	/* make sure we have what we need */
	if((dependent_host_name == NULL || !strcmp(dependent_host_name, "")) || (host_name == NULL || !strcmp(host_name, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: NULL host name in host dependency definition\n");
		return NULL;
		}

	/* allocate memory for a new host dependency entry */
	if((new_hostdependency = (hostdependency *)calloc(1, sizeof(hostdependency))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_hostdependency->dependent_host_name = (char *)strdup(dependent_host_name)) == NULL)
		result = ERROR;
	if((new_hostdependency->host_name = (char *)strdup(host_name)) == NULL)
		result = ERROR;
	if(dependency_period) {
		if((new_hostdependency->dependency_period = (char *)strdup(dependency_period)) == NULL)
			result = ERROR;
		}

	new_hostdependency->dependency_type = (dependency_type == EXECUTION_DEPENDENCY) ? EXECUTION_DEPENDENCY : NOTIFICATION_DEPENDENCY;
	new_hostdependency->inherits_parent = (inherits_parent > 0) ? TRUE : FALSE;
	new_hostdependency->fail_on_up = (fail_on_up == 1) ? TRUE : FALSE;
	new_hostdependency->fail_on_down = (fail_on_down == 1) ? TRUE : FALSE;
	new_hostdependency->fail_on_unreachable = (fail_on_unreachable == 1) ? TRUE : FALSE;
	new_hostdependency->fail_on_pending = (fail_on_pending == 1) ? TRUE : FALSE;
#ifdef NSCORE
	new_hostdependency->circular_path_checked = FALSE;
	new_hostdependency->contains_circular_path = FALSE;
#endif

	/* add new host dependency to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[HOSTDEPENDENCY_SKIPLIST], (void *)new_hostdependency);
		switch(result) {
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add host dependency to skiplist\n");
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_hostdependency->host_name);
		my_free(new_hostdependency->dependent_host_name);
		my_free(new_hostdependency);
		return NULL;
		}

	/* host dependencies are sorted alphabetically, so add new items to tail of list */
	if(hostdependency_list == NULL) {
		hostdependency_list = new_hostdependency;
		hostdependency_list_tail = hostdependency_list;
		}
	else {
		hostdependency_list_tail->next = new_hostdependency;
		hostdependency_list_tail = new_hostdependency;
		}

	return new_hostdependency;
	}



/* add a new host escalation to the list in memory */
hostescalation *add_hostescalation(char *host_name, int first_notification, int last_notification, double notification_interval, char *escalation_period, int escalate_on_down, int escalate_on_unreachable, int escalate_on_recovery) {
	hostescalation *new_hostescalation = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(host_name == NULL || !strcmp(host_name, "")) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Host escalation host name is NULL\n");
		return NULL;
		}

#ifdef TEST
	printf("NEW HST ESCALATION: %s = %d/%d/%.3f\n", host_name, first_notification, last_notification, notification_interval);
#endif

	/* allocate memory for a new host escalation entry */
	if((new_hostescalation = calloc(1, sizeof(hostescalation))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_hostescalation->host_name = (char *)strdup(host_name)) == NULL)
		result = ERROR;
	if(escalation_period) {
		if((new_hostescalation->escalation_period = (char *)strdup(escalation_period)) == NULL)
			result = ERROR;
		}

	new_hostescalation->first_notification = first_notification;
	new_hostescalation->last_notification = last_notification;
	new_hostescalation->notification_interval = (notification_interval <= 0) ? 0 : notification_interval;
	new_hostescalation->escalate_on_recovery = (escalate_on_recovery > 0) ? TRUE : FALSE;
	new_hostescalation->escalate_on_down = (escalate_on_down > 0) ? TRUE : FALSE;
	new_hostescalation->escalate_on_unreachable = (escalate_on_unreachable > 0) ? TRUE : FALSE;

	/* add new hostescalation to skiplist */
	if(result == OK) {
		result = skiplist_insert(object_skiplists[HOSTESCALATION_SKIPLIST], (void *)new_hostescalation);
		switch(result) {
			case SKIPLIST_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add hostescalation '%s' to skiplist\n", host_name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_hostescalation->host_name);
		my_free(new_hostescalation->escalation_period);
		my_free(new_hostescalation);
		return NULL;
		}

	/* host escalations are sorted alphabetically, so add new items to tail of list */
	if(hostescalation_list == NULL) {
		hostescalation_list = new_hostescalation;
		hostescalation_list_tail = hostescalation_list;
		}
	else {
		hostescalation_list_tail->next = new_hostescalation;
		hostescalation_list_tail = new_hostescalation;
		}

	return new_hostescalation;
	}



/* adds a contact group to a host escalation */
contactgroupsmember *add_contactgroup_to_hostescalation(hostescalation *he, char *group_name) {
	contactgroupsmember *new_contactgroupsmember = NULL;
	int result = OK;

	/* bail out if we weren't given the data we need */
	if(he == NULL || (group_name == NULL || !strcmp(group_name, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Host escalation or contactgroup name is NULL\n");
		return NULL;
		}

	/* allocate memory for the contactgroups member */
	if((new_contactgroupsmember = (contactgroupsmember *)calloc(1, sizeof(contactgroupsmember))) == NULL)
		return NULL;

	/* duplicate vars */
	if((new_contactgroupsmember->group_name = (char *)strdup(group_name)) == NULL)
		result = ERROR;

	/* handle errors */
	if(result == ERROR) {
		my_free(new_contactgroupsmember->group_name);
		my_free(new_contactgroupsmember);
		return NULL;
		}

	/* add this contactgroup to the host escalation */
	new_contactgroupsmember->next = he->contact_groups;
	he->contact_groups = new_contactgroupsmember;

	return new_contactgroupsmember;
	}



/* adds a contact to a host escalation */
contactsmember *add_contact_to_hostescalation(hostescalation *he, char *contact_name) {

	return add_contact_to_object(&he->contacts, contact_name);
	}



/* adds a contact to an object */
contactsmember *add_contact_to_object(contactsmember **object_ptr, char *contactname) {
	contactsmember *new_contactsmember = NULL;

	/* make sure we have the data we need */
	if(object_ptr == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact object is NULL\n");
		return NULL;
		}

	if(contactname == NULL || !strcmp(contactname, "")) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact name is NULL\n");
		return NULL;
		}

	/* allocate memory for a new member */
	if((new_contactsmember = malloc(sizeof(contactsmember))) == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not allocate memory for contact\n");
		return NULL;
		}
	if((new_contactsmember->contact_name = (char *)strdup(contactname)) == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not allocate memory for contact name\n");
		my_free(new_contactsmember);
		return NULL;
		}

	/* set initial values */
#ifdef NSCORE
	new_contactsmember->contact_ptr = NULL;
#endif

	/* add the new contact to the head of the contact list */
	new_contactsmember->next = *object_ptr;
	*object_ptr = new_contactsmember;

	return new_contactsmember;
	}



/* adds a custom variable to an object */
customvariablesmember *add_custom_variable_to_object(customvariablesmember **object_ptr, char *varname, char *varvalue) {
	customvariablesmember *new_customvariablesmember = NULL;

	/* make sure we have the data we need */
	if(object_ptr == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Custom variable object is NULL\n");
		return NULL;
		}

	if(varname == NULL || !strcmp(varname, "")) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Custom variable name is NULL\n");
		return NULL;
		}

	/* allocate memory for a new member */
	if((new_customvariablesmember = malloc(sizeof(customvariablesmember))) == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not allocate memory for custom variable\n");
		return NULL;
		}
	if((new_customvariablesmember->variable_name = (char *)strdup(varname)) == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not allocate memory for custom variable name\n");
		my_free(new_customvariablesmember);
		return NULL;
		}
	if(varvalue) {
		if((new_customvariablesmember->variable_value = (char *)strdup(varvalue)) == NULL) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not allocate memory for custom variable value\n");
			my_free(new_customvariablesmember->variable_name);
			my_free(new_customvariablesmember);
			return NULL;
			}
		}
	else
		new_customvariablesmember->variable_value = NULL;

	/* set initial values */
	new_customvariablesmember->has_been_modified = FALSE;

	/* add the new member to the head of the member list */
	new_customvariablesmember->next = *object_ptr;
	*object_ptr = new_customvariablesmember;

	return new_customvariablesmember;
	}




/******************************************************************/
/******************** OBJECT SEARCH FUNCTIONS *********************/
/******************************************************************/

/* given a timeperiod name and a starting point, find a timeperiod from the list in memory */
timeperiod * find_timeperiod(char *name) {
	timeperiod temp_timeperiod;

	if(name == NULL)
		return NULL;

	temp_timeperiod.name = name;

	return skiplist_find_first(object_skiplists[TIMEPERIOD_SKIPLIST], &temp_timeperiod, NULL);
	}


/* given a host name, find it in the list in memory */
host * find_host(char *name) {
	host temp_host;

	if(name == NULL)
		return NULL;

	temp_host.name = name;

	return skiplist_find_first(object_skiplists[HOST_SKIPLIST], &temp_host, NULL);
	}


/* find a hostgroup from the list in memory */
hostgroup * find_hostgroup(char *name) {
	hostgroup temp_hostgroup;

	if(name == NULL)
		return NULL;

	temp_hostgroup.group_name = name;

	return skiplist_find_first(object_skiplists[HOSTGROUP_SKIPLIST], &temp_hostgroup, NULL);
	}


/* find a servicegroup from the list in memory */
servicegroup * find_servicegroup(char *name) {
	servicegroup temp_servicegroup;

	if(name == NULL)
		return NULL;

	temp_servicegroup.group_name = name;

	return skiplist_find_first(object_skiplists[SERVICEGROUP_SKIPLIST], &temp_servicegroup, NULL);
	}


/* find a contact from the list in memory */
contact * find_contact(char *name) {
	contact temp_contact;

	if(name == NULL)
		return NULL;

	temp_contact.name = name;

	return skiplist_find_first(object_skiplists[CONTACT_SKIPLIST], &temp_contact, NULL);
	}


/* find a contact group from the list in memory */
contactgroup * find_contactgroup(char *name) {
	contactgroup temp_contactgroup;

	if(name == NULL)
		return NULL;

	temp_contactgroup.group_name = name;

	return skiplist_find_first(object_skiplists[CONTACTGROUP_SKIPLIST], &temp_contactgroup, NULL);
	}


/* given a command name, find a command from the list in memory */
command * find_command(char *name) {
	command temp_command;

	if(name == NULL)
		return NULL;

	temp_command.name = name;

	return skiplist_find_first(object_skiplists[COMMAND_SKIPLIST], &temp_command, NULL);
	}


/* given a host/service name, find the service in the list in memory */
service * find_service(char *host_name, char *svc_desc) {
	service temp_service;

	if(host_name == NULL || svc_desc == NULL)
		return NULL;

	temp_service.host_name = host_name;
	temp_service.description = svc_desc;

	return skiplist_find_first(object_skiplists[SERVICE_SKIPLIST], &temp_service, NULL);
	}




/******************************************************************/
/******************* OBJECT TRAVERSAL FUNCTIONS *******************/
/******************************************************************/

hostescalation *get_first_hostescalation_by_host(char *host_name, void **ptr) {
	hostescalation temp_hostescalation;

	if(host_name == NULL)
		return NULL;

	temp_hostescalation.host_name = host_name;

	return skiplist_find_first(object_skiplists[HOSTESCALATION_SKIPLIST], &temp_hostescalation, ptr);
	}


hostescalation *get_next_hostescalation_by_host(char *host_name, void **ptr) {
	hostescalation temp_hostescalation;

	if(host_name == NULL)
		return NULL;

	temp_hostescalation.host_name = host_name;

	return skiplist_find_next(object_skiplists[HOSTESCALATION_SKIPLIST], &temp_hostescalation, ptr);
	}


serviceescalation *get_first_serviceescalation_by_service(char *host_name, char *svc_description, void **ptr) {
	serviceescalation temp_serviceescalation;

	if(host_name == NULL || svc_description == NULL)
		return NULL;

	temp_serviceescalation.host_name = host_name;
	temp_serviceescalation.description = svc_description;

	return skiplist_find_first(object_skiplists[SERVICEESCALATION_SKIPLIST], &temp_serviceescalation, ptr);
	}


serviceescalation *get_next_serviceescalation_by_service(char *host_name, char *svc_description, void **ptr) {
	serviceescalation temp_serviceescalation;

	if(host_name == NULL || svc_description == NULL)
		return NULL;

	temp_serviceescalation.host_name = host_name;
	temp_serviceescalation.description = svc_description;

	return skiplist_find_next(object_skiplists[SERVICEESCALATION_SKIPLIST], &temp_serviceescalation, ptr);
	}


hostdependency *get_first_hostdependency_by_dependent_host(char *host_name, void **ptr) {
	hostdependency temp_hostdependency;

	if(host_name == NULL)
		return NULL;

	temp_hostdependency.dependent_host_name = host_name;

	return skiplist_find_first(object_skiplists[HOSTDEPENDENCY_SKIPLIST], &temp_hostdependency, ptr);
	}


hostdependency *get_next_hostdependency_by_dependent_host(char *host_name, void **ptr) {
	hostdependency temp_hostdependency;

	if(host_name == NULL || ptr == NULL)
		return NULL;

	temp_hostdependency.dependent_host_name = host_name;

	return skiplist_find_next(object_skiplists[HOSTDEPENDENCY_SKIPLIST], &temp_hostdependency, ptr);
	}


servicedependency *get_first_servicedependency_by_dependent_service(char *host_name, char *svc_description, void **ptr) {
	servicedependency temp_servicedependency;

	if(host_name == NULL || svc_description == NULL)
		return NULL;

	temp_servicedependency.dependent_host_name = host_name;
	temp_servicedependency.dependent_service_description = svc_description;

	return skiplist_find_first(object_skiplists[SERVICEDEPENDENCY_SKIPLIST], &temp_servicedependency, ptr);
	}


servicedependency *get_next_servicedependency_by_dependent_service(char *host_name, char *svc_description, void **ptr) {
	servicedependency temp_servicedependency;

	if(host_name == NULL || svc_description == NULL || ptr == NULL)
		return NULL;

	temp_servicedependency.dependent_host_name = host_name;
	temp_servicedependency.dependent_service_description = svc_description;

	return skiplist_find_next(object_skiplists[SERVICEDEPENDENCY_SKIPLIST], &temp_servicedependency, ptr);

	return NULL;
	}


#ifdef NSCORE
/* adds a object to a list of objects */
int add_object_to_objectlist(objectlist **list, void *object_ptr) {
	objectlist *temp_item = NULL;
	objectlist *new_item = NULL;

	if(list == NULL || object_ptr == NULL)
		return ERROR;

	/* skip this object if its already in the list */
	for(temp_item = *list; temp_item; temp_item = temp_item->next) {
		if(temp_item->object_ptr == object_ptr)
			break;
		}
	if(temp_item)
		return OK;

	/* allocate memory for a new list item */
	if((new_item = (objectlist *)malloc(sizeof(objectlist))) == NULL)
		return ERROR;

	/* initialize vars */
	new_item->object_ptr = object_ptr;

	/* add new item to head of list */
	new_item->next = *list;
	*list = new_item;

	return OK;
	}



/* frees memory allocated to a temporary object list */
int free_objectlist(objectlist **temp_list) {
	objectlist *this_objectlist = NULL;
	objectlist *next_objectlist = NULL;

	if(temp_list == NULL)
		return ERROR;

	/* free memory allocated to object list */
	for(this_objectlist = *temp_list; this_objectlist != NULL; this_objectlist = next_objectlist) {
		next_objectlist = this_objectlist->next;
		my_free(this_objectlist);
		}

	*temp_list = NULL;

	return OK;
	}
#endif



/******************************************************************/
/********************* OBJECT QUERY FUNCTIONS *********************/
/******************************************************************/

/* determines whether or not a specific host is an immediate child of another host */
int is_host_immediate_child_of_host(host *parent_host, host *child_host) {
	hostsmember *temp_hostsmember = NULL;

	/* not enough data */
	if(child_host == NULL)
		return FALSE;

	/* root/top-level hosts */
	if(parent_host == NULL) {
		if(child_host->parent_hosts == NULL)
			return TRUE;
		}

	/* mid-level/bottom hosts */
	else {

		for(temp_hostsmember = child_host->parent_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
#ifdef NSCORE
			if(temp_hostsmember->host_ptr == parent_host)
				return TRUE;
#else
			if(!strcmp(temp_hostsmember->host_name, parent_host->name))
				return TRUE;
#endif
			}
		}

	return FALSE;
	}


/* determines whether or not a specific host is an immediate parent of another host */
int is_host_immediate_parent_of_host(host *child_host, host *parent_host) {

	if(is_host_immediate_child_of_host(parent_host, child_host) == TRUE)
		return TRUE;

	return FALSE;
	}


/* returns a count of the immediate children for a given host */
/* NOTE: This function is only used by the CGIS */
int number_of_immediate_child_hosts(host *hst) {
	int children = 0;
	host *temp_host = NULL;

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
		if(is_host_immediate_child_of_host(hst, temp_host) == TRUE)
			children++;
		}

	return children;
	}


/* returns a count of the total children for a given host */
/* NOTE: This function is only used by the CGIS */
int number_of_total_child_hosts(host *hst) {
	int children = 0;
	host *temp_host = NULL;

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
		if(is_host_immediate_child_of_host(hst, temp_host) == TRUE)
			children += number_of_total_child_hosts(temp_host) + 1;
		}

	return children;
	}


/* get the number of immediate parent hosts for a given host */
/* NOTE: This function is only used by the CGIS */
int number_of_immediate_parent_hosts(host *hst) {
	int parents = 0;
	host *temp_host = NULL;

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
		if(is_host_immediate_parent_of_host(hst, temp_host) == TRUE) {
			parents++;
			}
		}

	return parents;
	}


/* get the total number of parent hosts for a given host */
/* NOTE: This function is only used by the CGIS */
int number_of_total_parent_hosts(host *hst) {
	int parents = 0;
	host *temp_host = NULL;

	for(temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
		if(is_host_immediate_parent_of_host(hst, temp_host) == TRUE) {
			parents += number_of_total_parent_hosts(temp_host) + 1;
			}
		}

	return parents;
	}


/*  tests whether a host is a member of a particular hostgroup */
/* NOTE: This function is only used by the CGIS */
int is_host_member_of_hostgroup(hostgroup *group, host *hst) {
	hostsmember *temp_hostsmember = NULL;

	if(group == NULL || hst == NULL)
		return FALSE;

	for(temp_hostsmember = group->members; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
#ifdef NSCORE
		if(temp_hostsmember->host_ptr == hst)
			return TRUE;
#else
		if(!strcmp(temp_hostsmember->host_name, hst->name))
			return TRUE;
#endif
		}

	return FALSE;
	}


/*  tests whether a host is a member of a particular servicegroup */
/* NOTE: This function is only used by the CGIS */
int is_host_member_of_servicegroup(servicegroup *group, host *hst) {
	servicesmember *temp_servicesmember = NULL;

	if(group == NULL || hst == NULL)
		return FALSE;

	for(temp_servicesmember = group->members; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
#ifdef NSCORE
		if(temp_servicesmember->service_ptr != NULL && temp_servicesmember->service_ptr->host_ptr == hst)
			return TRUE;
#else
		if(!strcmp(temp_servicesmember->host_name, hst->name))
			return TRUE;
#endif
		}

	return FALSE;
	}


/*  tests whether a service is a member of a particular servicegroup */
/* NOTE: This function is only used by the CGIS */
int is_service_member_of_servicegroup(servicegroup *group, service *svc) {
	servicesmember *temp_servicesmember = NULL;

	if(group == NULL || svc == NULL)
		return FALSE;

	for(temp_servicesmember = group->members; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
#ifdef NSCORE
		if(temp_servicesmember->service_ptr == svc)
			return TRUE;
#else
		if(!strcmp(temp_servicesmember->host_name, svc->host_name) && !strcmp(temp_servicesmember->service_description, svc->description))
			return TRUE;
#endif
		}

	return FALSE;
	}


/*
 * tests whether a contact is a member of a particular contactgroup.
 * The mk-livestatus eventbroker module uses this, so it must hang
 * around until 4.0 to prevent api breakage.
 * The cgi's stopped using it quite long ago though, so we need only
 * compile it if we're building the core
 */
int is_contact_member_of_contactgroup(contactgroup *group, contact *cntct) {
	contactsmember *member;
	contact *temp_contact = NULL;

	if(!group || !cntct)
		return FALSE;

	/* search all contacts in this contact group */
	for(member = group->members; member; member = member->next) {
#ifdef NSCORE
		temp_contact = member->contact_ptr;
#else
		temp_contact = find_contact(member->contact_name);
#endif
		if(temp_contact == NULL)
			continue;
		if(temp_contact == cntct)
			return TRUE;
		}

	return FALSE;
	}

/*  tests whether a contact is a contact for a particular host */
int is_contact_for_host(host *hst, contact *cntct) {
	contactsmember *temp_contactsmember = NULL;
	contact *temp_contact = NULL;
	contactgroupsmember *temp_contactgroupsmember = NULL;
	contactgroup *temp_contactgroup = NULL;

	if(hst == NULL || cntct == NULL) {
		return FALSE;
		}

	/* search all individual contacts of this host */
	for(temp_contactsmember = hst->contacts; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
#ifdef NSCORE
		temp_contact = temp_contactsmember->contact_ptr;
#else
		temp_contact = find_contact(temp_contactsmember->contact_name);
#endif
		if(temp_contact == NULL)
			continue;
		if(temp_contact == cntct)
			return TRUE;
		}

	/* search all contactgroups of this host */
	for(temp_contactgroupsmember = hst->contact_groups; temp_contactgroupsmember != NULL; temp_contactgroupsmember = temp_contactgroupsmember->next) {
#ifdef NSCORE
		temp_contactgroup = temp_contactgroupsmember->group_ptr;
#else
		temp_contactgroup = find_contactgroup(temp_contactgroupsmember->group_name);
#endif
		if(temp_contactgroup == NULL)
			continue;
		if(is_contact_member_of_contactgroup(temp_contactgroup, cntct))
			return TRUE;
		}

	return FALSE;
	}



/* tests whether or not a contact is an escalated contact for a particular host */
int is_escalated_contact_for_host(host *hst, contact *cntct) {
	contactsmember *temp_contactsmember = NULL;
	contact *temp_contact = NULL;
	hostescalation *temp_hostescalation = NULL;
	contactgroupsmember *temp_contactgroupsmember = NULL;
	contactgroup *temp_contactgroup = NULL;
	void *ptr = NULL;


	/* search all host escalations */
	for(temp_hostescalation = get_first_hostescalation_by_host(hst->name, &ptr); temp_hostescalation != NULL; temp_hostescalation = get_next_hostescalation_by_host(hst->name, &ptr)) {

		/* search all contacts of this host escalation */
		for(temp_contactsmember = temp_hostescalation->contacts; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
#ifdef NSCORE
			temp_contact = temp_contactsmember->contact_ptr;
#else
			temp_contact = find_contact(temp_contactsmember->contact_name);
#endif
			if(temp_contact == NULL)
				continue;
			if(temp_contact == cntct)
				return TRUE;
			}

		/* search all contactgroups of this host escalation */
		for(temp_contactgroupsmember = temp_hostescalation->contact_groups; temp_contactgroupsmember != NULL; temp_contactgroupsmember = temp_contactgroupsmember->next) {
#ifdef NSCORE
			temp_contactgroup = temp_contactgroupsmember->group_ptr;
#else
			temp_contactgroup = find_contactgroup(temp_contactgroupsmember->group_name);
#endif
			if(temp_contactgroup == NULL)
				continue;
			if(is_contact_member_of_contactgroup(temp_contactgroup, cntct))
				return TRUE;

			}
		}

	return FALSE;
	}


/*  tests whether a contact is a contact for a particular service */
int is_contact_for_service(service *svc, contact *cntct) {
	contactsmember *temp_contactsmember = NULL;
	contact *temp_contact = NULL;
	contactgroupsmember *temp_contactgroupsmember = NULL;
	contactgroup *temp_contactgroup = NULL;

	if(svc == NULL || cntct == NULL)
		return FALSE;

	/* search all individual contacts of this service */
	for(temp_contactsmember = svc->contacts; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
#ifdef NSCORE
		temp_contact = temp_contactsmember->contact_ptr;
#else
		temp_contact = find_contact(temp_contactsmember->contact_name);
#endif

		if(temp_contact == cntct)
			return TRUE;
		}

	/* search all contactgroups of this service */
	for(temp_contactgroupsmember = svc->contact_groups; temp_contactgroupsmember != NULL; temp_contactgroupsmember = temp_contactgroupsmember->next) {
#ifdef NSCORE
		temp_contactgroup = temp_contactgroupsmember->group_ptr;
#else
		temp_contactgroup = find_contactgroup(temp_contactgroupsmember->group_name);
#endif
		if(temp_contactgroup == NULL)
			continue;
		if(is_contact_member_of_contactgroup(temp_contactgroup, cntct))
			return TRUE;

		}

	return FALSE;
	}



/* tests whether or not a contact is an escalated contact for a particular service */
int is_escalated_contact_for_service(service *svc, contact *cntct) {
	serviceescalation *temp_serviceescalation = NULL;
	contactsmember *temp_contactsmember = NULL;
	contact *temp_contact = NULL;
	contactgroupsmember *temp_contactgroupsmember = NULL;
	contactgroup *temp_contactgroup = NULL;
	void *ptr = NULL;

	/* search all the service escalations */
	for(temp_serviceescalation = get_first_serviceescalation_by_service(svc->host_name, svc->description, &ptr); temp_serviceescalation != NULL; temp_serviceescalation = get_next_serviceescalation_by_service(svc->host_name, svc->description, &ptr)) {

		/* search all contacts of this service escalation */
		for(temp_contactsmember = temp_serviceescalation->contacts; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
#ifdef NSCORE
			temp_contact = temp_contactsmember->contact_ptr;
#else
			temp_contact = find_contact(temp_contactsmember->contact_name);
#endif
			if(temp_contact == NULL)
				continue;
			if(temp_contact == cntct)
				return TRUE;
			}

		/* search all contactgroups of this service escalation */
		for(temp_contactgroupsmember = temp_serviceescalation->contact_groups; temp_contactgroupsmember != NULL; temp_contactgroupsmember = temp_contactgroupsmember->next) {
#ifdef NSCORE
			temp_contactgroup = temp_contactgroupsmember->group_ptr;
#else
			temp_contactgroup = find_contactgroup(temp_contactgroupsmember->group_name);
#endif
			if(temp_contactgroup == NULL)
				continue;
			if(is_contact_member_of_contactgroup(temp_contactgroup, cntct))
				return TRUE;
			}
		}

	return FALSE;
	}


#ifdef NSCORE

/* checks to see if there exists a circular dependency for a service */
int check_for_circular_servicedependency_path(servicedependency *root_dep, servicedependency *dep, int dependency_type) {
	servicedependency *temp_sd = NULL;

	if(root_dep == NULL || dep == NULL)
		return FALSE;

	/* this is not the proper dependency type */
	if(root_dep->dependency_type != dependency_type || dep->dependency_type != dependency_type)
		return FALSE;

	/* don't go into a loop, don't bother checking anymore if we know this dependency already has a loop */
	if(root_dep->contains_circular_path == TRUE)
		return TRUE;

	/* dependency has already been checked - there is a path somewhere, but it may not be for this particular dep... */
	/* this should speed up detection for some loops */
	if(dep->circular_path_checked == TRUE)
		return FALSE;

	/* set the check flag so we don't get into an infinite loop */
	dep->circular_path_checked = TRUE;

	/* is this service dependent on the root service? */
	if(dep != root_dep) {
		if(root_dep->dependent_service_ptr == dep->master_service_ptr) {
			root_dep->contains_circular_path = TRUE;
			dep->contains_circular_path = TRUE;
			return TRUE;
			}
		}

	/* notification dependencies are ok at this point as long as they don't inherit */
	if(dependency_type == NOTIFICATION_DEPENDENCY && dep->inherits_parent == FALSE)
		return FALSE;

	/* check all parent dependencies */
	for(temp_sd = servicedependency_list; temp_sd != NULL; temp_sd = temp_sd->next) {

		/* only check parent dependencies */
		if(dep->master_service_ptr != temp_sd->dependent_service_ptr)
			continue;

		if(check_for_circular_servicedependency_path(root_dep, temp_sd, dependency_type) == TRUE)
			return TRUE;
		}

	return FALSE;
	}


/* checks to see if there exists a circular dependency for a host */
int check_for_circular_hostdependency_path(hostdependency *root_dep, hostdependency *dep, int dependency_type) {
	hostdependency *temp_hd = NULL;

	if(root_dep == NULL || dep == NULL)
		return FALSE;

	/* this is not the proper dependency type */
	if(root_dep->dependency_type != dependency_type || dep->dependency_type != dependency_type)
		return FALSE;

	/* don't go into a loop, don't bother checking anymore if we know this dependency already has a loop */
	if(root_dep->contains_circular_path == TRUE)
		return TRUE;

	/* dependency has already been checked - there is a path somewhere, but it may not be for this particular dep... */
	/* this should speed up detection for some loops */
	if(dep->circular_path_checked == TRUE)
		return FALSE;

	/* set the check flag so we don't get into an infinite loop */
	dep->circular_path_checked = TRUE;

	/* is this host dependent on the root host? */
	if(dep != root_dep) {
		if(root_dep->dependent_host_ptr == dep->master_host_ptr) {
			root_dep->contains_circular_path = TRUE;
			dep->contains_circular_path = TRUE;
			return TRUE;
			}
		}

	/* notification dependencies are ok at this point as long as they don't inherit */
	if(dependency_type == NOTIFICATION_DEPENDENCY && dep->inherits_parent == FALSE)
		return FALSE;

	/* check all parent dependencies */
	for(temp_hd = hostdependency_list; temp_hd != NULL; temp_hd = temp_hd->next) {

		/* only check parent dependencies */
		if(dep->master_host_ptr != temp_hd->dependent_host_ptr)
			continue;

		if(check_for_circular_hostdependency_path(root_dep, temp_hd, dependency_type) == TRUE)
			return TRUE;
		}

	return FALSE;
	}

#endif




/******************************************************************/
/******************* OBJECT DELETION FUNCTIONS ********************/
/******************************************************************/


/* free all allocated memory for objects */
int free_object_data(void) {
	timeperiod *this_timeperiod = NULL;
	timeperiod *next_timeperiod = NULL;
	daterange *this_daterange = NULL;
	daterange *next_daterange = NULL;
	timerange *this_timerange = NULL;
	timerange *next_timerange = NULL;
	timeperiodexclusion *this_timeperiodexclusion = NULL;
	timeperiodexclusion *next_timeperiodexclusion = NULL;
	host *this_host = NULL;
	host *next_host = NULL;
	hostsmember *this_hostsmember = NULL;
	hostsmember *next_hostsmember = NULL;
	hostgroup *this_hostgroup = NULL;
	hostgroup *next_hostgroup = NULL;
	servicegroup *this_servicegroup = NULL;
	servicegroup *next_servicegroup = NULL;
	servicesmember *this_servicesmember = NULL;
	servicesmember *next_servicesmember = NULL;
	contact	*this_contact = NULL;
	contact *next_contact = NULL;
	contactgroup *this_contactgroup = NULL;
	contactgroup *next_contactgroup = NULL;
	contactsmember *this_contactsmember = NULL;
	contactsmember *next_contactsmember = NULL;
	contactgroupsmember *this_contactgroupsmember = NULL;
	contactgroupsmember *next_contactgroupsmember = NULL;
	customvariablesmember *this_customvariablesmember = NULL;
	customvariablesmember *next_customvariablesmember = NULL;
	service *this_service = NULL;
	service *next_service = NULL;
	command *this_command = NULL;
	command *next_command = NULL;
	commandsmember *this_commandsmember = NULL;
	commandsmember *next_commandsmember = NULL;
	serviceescalation *this_serviceescalation = NULL;
	serviceescalation *next_serviceescalation = NULL;
	servicedependency *this_servicedependency = NULL;
	servicedependency *next_servicedependency = NULL;
	hostdependency *this_hostdependency = NULL;
	hostdependency *next_hostdependency = NULL;
	hostescalation *this_hostescalation = NULL;
	hostescalation *next_hostescalation = NULL;
	register int x = 0;
	register int i = 0;


	/**** free memory for the timeperiod list ****/
	this_timeperiod = timeperiod_list;
	while(this_timeperiod != NULL) {

		/* free the exception time ranges contained in this timeperiod */
		for(x = 0; x < DATERANGE_TYPES; x++) {

			for(this_daterange = this_timeperiod->exceptions[x]; this_daterange != NULL; this_daterange = next_daterange) {
				next_daterange = this_daterange->next;
				for(this_timerange = this_daterange->times; this_timerange != NULL; this_timerange = next_timerange) {
					next_timerange = this_timerange->next;
					my_free(this_timerange);
					}
				my_free(this_daterange);
				}
			}

		/* free the day time ranges contained in this timeperiod */
		for(x = 0; x < 7; x++) {

			for(this_timerange = this_timeperiod->days[x]; this_timerange != NULL; this_timerange = next_timerange) {
				next_timerange = this_timerange->next;
				my_free(this_timerange);
				}
			}

		/* free exclusions */
		for(this_timeperiodexclusion = this_timeperiod->exclusions; this_timeperiodexclusion != NULL; this_timeperiodexclusion = next_timeperiodexclusion) {
			next_timeperiodexclusion = this_timeperiodexclusion->next;
			my_free(this_timeperiodexclusion->timeperiod_name);
			my_free(this_timeperiodexclusion);
			}

		next_timeperiod = this_timeperiod->next;
		my_free(this_timeperiod->name);
		my_free(this_timeperiod->alias);
		my_free(this_timeperiod);
		this_timeperiod = next_timeperiod;
		}

	/* reset pointers */
	timeperiod_list = NULL;


	/**** free memory for the host list ****/
	this_host = host_list;
	while(this_host != NULL) {

		next_host = this_host->next;

		/* free memory for parent hosts */
		this_hostsmember = this_host->parent_hosts;
		while(this_hostsmember != NULL) {
			next_hostsmember = this_hostsmember->next;
			my_free(this_hostsmember->host_name);
			my_free(this_hostsmember);
			this_hostsmember = next_hostsmember;
			}

		/* free memory for child host links */
		this_hostsmember = this_host->child_hosts;
		while(this_hostsmember != NULL) {
			next_hostsmember = this_hostsmember->next;
			my_free(this_hostsmember->host_name);
			my_free(this_hostsmember);
			this_hostsmember = next_hostsmember;
			}

		/* free memory for service links */
		this_servicesmember = this_host->services;
		while(this_servicesmember != NULL) {
			next_servicesmember = this_servicesmember->next;
			my_free(this_servicesmember->host_name);
			my_free(this_servicesmember->service_description);
			my_free(this_servicesmember);
			this_servicesmember = next_servicesmember;
			}

		/* free memory for contact groups */
		this_contactgroupsmember = this_host->contact_groups;
		while(this_contactgroupsmember != NULL) {
			next_contactgroupsmember = this_contactgroupsmember->next;
			my_free(this_contactgroupsmember->group_name);
			my_free(this_contactgroupsmember);
			this_contactgroupsmember = next_contactgroupsmember;
			}

		/* free memory for contacts */
		this_contactsmember = this_host->contacts;
		while(this_contactsmember != NULL) {
			next_contactsmember = this_contactsmember->next;
			my_free(this_contactsmember->contact_name);
			my_free(this_contactsmember);
			this_contactsmember = next_contactsmember;
			}

		/* free memory for custom variables */
		this_customvariablesmember = this_host->custom_variables;
		while(this_customvariablesmember != NULL) {
			next_customvariablesmember = this_customvariablesmember->next;
			my_free(this_customvariablesmember->variable_name);
			my_free(this_customvariablesmember->variable_value);
			my_free(this_customvariablesmember);
			this_customvariablesmember = next_customvariablesmember;
			}

		my_free(this_host->name);
		my_free(this_host->display_name);
		my_free(this_host->alias);
		my_free(this_host->address);
#ifdef NSCORE
		my_free(this_host->plugin_output);
		my_free(this_host->long_plugin_output);
		my_free(this_host->perf_data);

		free_objectlist(&this_host->hostgroups_ptr);
#endif
		my_free(this_host->check_period);
		my_free(this_host->host_check_command);
		my_free(this_host->event_handler);
		my_free(this_host->failure_prediction_options);
		my_free(this_host->notification_period);
		my_free(this_host->notes);
		my_free(this_host->notes_url);
		my_free(this_host->action_url);
		my_free(this_host->icon_image);
		my_free(this_host->icon_image_alt);
		my_free(this_host->vrml_image);
		my_free(this_host->statusmap_image);
		my_free(this_host);
		this_host = next_host;
		}

	/* reset pointers */
	host_list = NULL;


	/**** free memory for the host group list ****/
	this_hostgroup = hostgroup_list;
	while(this_hostgroup != NULL) {

		/* free memory for the group members */
		this_hostsmember = this_hostgroup->members;
		while(this_hostsmember != NULL) {
			next_hostsmember = this_hostsmember->next;
			my_free(this_hostsmember->host_name);
			my_free(this_hostsmember);
			this_hostsmember = next_hostsmember;
			}

		next_hostgroup = this_hostgroup->next;
		my_free(this_hostgroup->group_name);
		my_free(this_hostgroup->alias);
		my_free(this_hostgroup->notes);
		my_free(this_hostgroup->notes_url);
		my_free(this_hostgroup->action_url);
		my_free(this_hostgroup);
		this_hostgroup = next_hostgroup;
		}

	/* reset pointers */
	hostgroup_list = NULL;


	/**** free memory for the service group list ****/
	this_servicegroup = servicegroup_list;
	while(this_servicegroup != NULL) {

		/* free memory for the group members */
		this_servicesmember = this_servicegroup->members;
		while(this_servicesmember != NULL) {
			next_servicesmember = this_servicesmember->next;
			my_free(this_servicesmember->host_name);
			my_free(this_servicesmember->service_description);
			my_free(this_servicesmember);
			this_servicesmember = next_servicesmember;
			}

		next_servicegroup = this_servicegroup->next;
		my_free(this_servicegroup->group_name);
		my_free(this_servicegroup->alias);
		my_free(this_servicegroup->notes);
		my_free(this_servicegroup->notes_url);
		my_free(this_servicegroup->action_url);
		my_free(this_servicegroup);
		this_servicegroup = next_servicegroup;
		}

	/* reset pointers */
	servicegroup_list = NULL;


	/**** free memory for the contact list ****/
	this_contact = contact_list;
	while(this_contact != NULL) {

		/* free memory for the host notification commands */
		this_commandsmember = this_contact->host_notification_commands;
		while(this_commandsmember != NULL) {
			next_commandsmember = this_commandsmember->next;
			if(this_commandsmember->command != NULL)
				my_free(this_commandsmember->command);
			my_free(this_commandsmember);
			this_commandsmember = next_commandsmember;
			}

		/* free memory for the service notification commands */
		this_commandsmember = this_contact->service_notification_commands;
		while(this_commandsmember != NULL) {
			next_commandsmember = this_commandsmember->next;
			if(this_commandsmember->command != NULL)
				my_free(this_commandsmember->command);
			my_free(this_commandsmember);
			this_commandsmember = next_commandsmember;
			}

		/* free memory for custom variables */
		this_customvariablesmember = this_contact->custom_variables;
		while(this_customvariablesmember != NULL) {
			next_customvariablesmember = this_customvariablesmember->next;
			my_free(this_customvariablesmember->variable_name);
			my_free(this_customvariablesmember->variable_value);
			my_free(this_customvariablesmember);
			this_customvariablesmember = next_customvariablesmember;
			}

		next_contact = this_contact->next;
		my_free(this_contact->name);
		my_free(this_contact->alias);
		my_free(this_contact->email);
		my_free(this_contact->pager);
		for(i = 0; i < MAX_CONTACT_ADDRESSES; i++)
			my_free(this_contact->address[i]);
		my_free(this_contact->host_notification_period);
		my_free(this_contact->service_notification_period);

#ifdef NSCORE
		free_objectlist(&this_contact->contactgroups_ptr);
#endif

		my_free(this_contact);
		this_contact = next_contact;
		}

	/* reset pointers */
	contact_list = NULL;


	/**** free memory for the contact group list ****/
	this_contactgroup = contactgroup_list;
	while(this_contactgroup != NULL) {

		/* free memory for the group members */
		this_contactsmember = this_contactgroup->members;
		while(this_contactsmember != NULL) {
			next_contactsmember = this_contactsmember->next;
			my_free(this_contactsmember->contact_name);
			my_free(this_contactsmember);
			this_contactsmember = next_contactsmember;
			}

		next_contactgroup = this_contactgroup->next;
		my_free(this_contactgroup->group_name);
		my_free(this_contactgroup->alias);
		my_free(this_contactgroup);
		this_contactgroup = next_contactgroup;
		}

	/* reset pointers */
	contactgroup_list = NULL;


	/**** free memory for the service list ****/
	this_service = service_list;
	while(this_service != NULL) {

		next_service = this_service->next;

		/* free memory for contact groups */
		this_contactgroupsmember = this_service->contact_groups;
		while(this_contactgroupsmember != NULL) {
			next_contactgroupsmember = this_contactgroupsmember->next;
			my_free(this_contactgroupsmember->group_name);
			my_free(this_contactgroupsmember);
			this_contactgroupsmember = next_contactgroupsmember;
			}

		/* free memory for contacts */
		this_contactsmember = this_service->contacts;
		while(this_contactsmember != NULL) {
			next_contactsmember = this_contactsmember->next;
			my_free(this_contactsmember->contact_name);
			my_free(this_contactsmember);
			this_contactsmember = next_contactsmember;
			}

		/* free memory for custom variables */
		this_customvariablesmember = this_service->custom_variables;
		while(this_customvariablesmember != NULL) {
			next_customvariablesmember = this_customvariablesmember->next;
			my_free(this_customvariablesmember->variable_name);
			my_free(this_customvariablesmember->variable_value);
			my_free(this_customvariablesmember);
			this_customvariablesmember = next_customvariablesmember;
			}

		my_free(this_service->host_name);
		my_free(this_service->description);
		my_free(this_service->display_name);
		my_free(this_service->service_check_command);
#ifdef NSCORE
		my_free(this_service->plugin_output);
		my_free(this_service->long_plugin_output);
		my_free(this_service->perf_data);

		my_free(this_service->event_handler_args);
		my_free(this_service->check_command_args);

		free_objectlist(&this_service->servicegroups_ptr);
#endif
		my_free(this_service->notification_period);
		my_free(this_service->check_period);
		my_free(this_service->event_handler);
		my_free(this_service->failure_prediction_options);
		my_free(this_service->notes);
		my_free(this_service->notes_url);
		my_free(this_service->action_url);
		my_free(this_service->icon_image);
		my_free(this_service->icon_image_alt);
		my_free(this_service);
		this_service = next_service;
		}

	/* reset pointers */
	service_list = NULL;


	/**** free memory for the command list ****/
	this_command = command_list;
	while(this_command != NULL) {
		next_command = this_command->next;
		my_free(this_command->name);
		my_free(this_command->command_line);
		my_free(this_command);
		this_command = next_command;
		}

	/* reset pointers */
	command_list = NULL;


	/**** free memory for the service escalation list ****/
	this_serviceescalation = serviceescalation_list;
	while(this_serviceescalation != NULL) {

		/* free memory for the contact group members */
		this_contactgroupsmember = this_serviceescalation->contact_groups;
		while(this_contactgroupsmember != NULL) {
			next_contactgroupsmember = this_contactgroupsmember->next;
			my_free(this_contactgroupsmember->group_name);
			my_free(this_contactgroupsmember);
			this_contactgroupsmember = next_contactgroupsmember;
			}

		/* free memory for contacts */
		this_contactsmember = this_serviceescalation->contacts;
		while(this_contactsmember != NULL) {
			next_contactsmember = this_contactsmember->next;
			my_free(this_contactsmember->contact_name);
			my_free(this_contactsmember);
			this_contactsmember = next_contactsmember;
			}

		next_serviceescalation = this_serviceescalation->next;
		my_free(this_serviceescalation->host_name);
		my_free(this_serviceescalation->description);
		my_free(this_serviceescalation->escalation_period);
		my_free(this_serviceescalation);
		this_serviceescalation = next_serviceescalation;
		}

	/* reset pointers */
	serviceescalation_list = NULL;


	/**** free memory for the service dependency list ****/
	this_servicedependency = servicedependency_list;
	while(this_servicedependency != NULL) {
		next_servicedependency = this_servicedependency->next;
		my_free(this_servicedependency->dependency_period);
		my_free(this_servicedependency->dependent_host_name);
		my_free(this_servicedependency->dependent_service_description);
		my_free(this_servicedependency->host_name);
		my_free(this_servicedependency->service_description);
		my_free(this_servicedependency);
		this_servicedependency = next_servicedependency;
		}

	/* reset pointers */
	servicedependency_list = NULL;


	/**** free memory for the host dependency list ****/
	this_hostdependency = hostdependency_list;
	while(this_hostdependency != NULL) {
		next_hostdependency = this_hostdependency->next;
		my_free(this_hostdependency->dependency_period);
		my_free(this_hostdependency->dependent_host_name);
		my_free(this_hostdependency->host_name);
		my_free(this_hostdependency);
		this_hostdependency = next_hostdependency;
		}

	/* reset pointers */
	hostdependency_list = NULL;


	/**** free memory for the host escalation list ****/
	this_hostescalation = hostescalation_list;
	while(this_hostescalation != NULL) {

		/* free memory for the contact group members */
		this_contactgroupsmember = this_hostescalation->contact_groups;
		while(this_contactgroupsmember != NULL) {
			next_contactgroupsmember = this_contactgroupsmember->next;
			my_free(this_contactgroupsmember->group_name);
			my_free(this_contactgroupsmember);
			this_contactgroupsmember = next_contactgroupsmember;
			}

		/* free memory for contacts */
		this_contactsmember = this_hostescalation->contacts;
		while(this_contactsmember != NULL) {
			next_contactsmember = this_contactsmember->next;
			my_free(this_contactsmember->contact_name);
			my_free(this_contactsmember);
			this_contactsmember = next_contactsmember;
			}

		next_hostescalation = this_hostescalation->next;
		my_free(this_hostescalation->host_name);
		my_free(this_hostescalation->escalation_period);
		my_free(this_hostescalation);
		this_hostescalation = next_hostescalation;
		}

	/* reset pointers */
	hostescalation_list = NULL;

	/* free object skiplists */
	free_object_skiplists();

	return OK;
	}

