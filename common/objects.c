/*****************************************************************************
 *
 * OBJECTS.C - Object addition and search functions for Nagios
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
#include "../xdata/xodtemplate.h"

#ifdef NSCGI
#include "../include/cgiutils.h"
#else
#include "../include/nagios.h"
#endif



/*
 * These get created in xdata/xodtemplate.c:xodtemplate_register_objects()
 * Escalations are attached to the objects they belong to.
 * Dependencies are attached to the dependent end of the object chain.
 */
dkhash_table *object_hash_tables[NUM_OBJECT_SKIPLISTS];

command *command_list = NULL;
timeperiod *timeperiod_list = NULL;
host *host_list = NULL;
service *service_list = NULL;
contact *contact_list = NULL;
hostgroup *hostgroup_list = NULL;
servicegroup *servicegroup_list = NULL;
contactgroup *contactgroup_list = NULL;
hostescalation *hostescalation_list = NULL;
serviceescalation *serviceescalation_list = NULL;
command **command_ary = NULL;
timeperiod **timeperiod_ary = NULL;
host **host_ary = NULL;
service **service_ary = NULL;
contact **contact_ary = NULL;
hostgroup **hostgroup_ary = NULL;
servicegroup **servicegroup_ary = NULL;
contactgroup **contactgroup_ary = NULL;
hostescalation **hostescalation_ary = NULL;
serviceescalation **serviceescalation_ary = NULL;
hostdependency **hostdependency_ary = NULL;
servicedependency **servicedependency_ary = NULL;

#ifndef NSCGI
int __nagios_object_structure_version = CURRENT_OBJECT_STRUCTURE_VERSION;

struct flag_map {
	int opt;
	int ch;
	const char *name;
};

static const struct flag_map service_flag_map[] = {
	{ OPT_WARNING, 'w', "warning" },
	{ OPT_UNKNOWN, 'u', "unknown" },
	{ OPT_CRITICAL, 'c', "critical" },
	{ OPT_FLAPPING, 'f', "flapping" },
	{ OPT_DOWNTIME, 's', "downtime" },
	{ OPT_OK, 'o', "ok" },
	{ OPT_RECOVERY, 'r', "recovery" },
	{ OPT_PENDING, 'p', "pending" },
	{ 0, 0, NULL },
};

static const struct flag_map host_flag_map[] = {
	{ OPT_DOWN, 'd', "down" },
	{ OPT_UNREACHABLE, 'u', "unreachable" },
	{ OPT_FLAPPING, 'f', "flapping" },
	{ OPT_RECOVERY, 'r', "recovery" },
	{ OPT_DOWNTIME, 's', "downtime" },
	{ OPT_PENDING, 'p', "pending" },
	{ 0, 0, NULL },
};

static const char *opts2str(int opts, const struct flag_map *map, char ok_char)
{
	int i, pos = 0;
	static char buf[16];

	if(!opts)
		return "n";

	if(opts == OPT_ALL)
		return "a";

	if(flag_isset(opts, OPT_OK)) {
		flag_unset(opts, OPT_OK);
		buf[pos++] = ok_char;
		buf[pos++] = opts ? ',' : 0;
		}

	for (i = 0; map[i].name; i++) {
		if(flag_isset(opts, map[i].opt)) {
			buf[pos++] = map[i].ch;
			flag_unset(opts, map[i].opt);
			if(!opts)
				break;
			buf[pos++] = ',';
		}
	}
	buf[pos++] = 0;
	return buf;
}
#endif

unsigned int host_services_value(host *h) {
	servicesmember *sm;
	unsigned int ret = 0;
	for(sm = h->services; sm; sm = sm->next) {
		ret += sm->service_ptr->hourly_value;
		}
	return ret;
	}


#ifndef NSCGI
/* Host/Service dependencies are not visible in Nagios CGIs, so we exclude them */
static int cmp_sdep(const void *a_, const void *b_) {
	const servicedependency *a = *(servicedependency **)a_;
	const servicedependency *b = *(servicedependency **)b_;
	int ret;
	ret = a->master_service_ptr->id - b->master_service_ptr->id;
	return ret ? ret : (int)(a->dependent_service_ptr->id - b->dependent_service_ptr->id);
	}

static int cmp_hdep(const void *a_, const void *b_) {
	const hostdependency *a = *(const hostdependency **)a_;
	const hostdependency *b = *(const hostdependency **)b_;
	int ret;
	ret = a->master_host_ptr->id - b->master_host_ptr->id;
	return ret ? ret : (int)(a->dependent_host_ptr->id - b->dependent_host_ptr->id);
	}

static int cmp_serviceesc(const void *a_, const void *b_) {
	const serviceescalation *a = *(const serviceescalation **)a_;
	const serviceescalation *b = *(const serviceescalation **)b_;
	return a->service_ptr->id - b->service_ptr->id;
	}

static int cmp_hostesc(const void *a_, const void *b_) {
	const hostescalation *a = *(const hostescalation **)a_;
	const hostescalation *b = *(const hostescalation **)b_;
	return a->host_ptr->id - b->host_ptr->id;
	}
#endif

static void post_process_object_config(void) {
	objectlist *list;
	unsigned int i, slot;

	if(hostdependency_ary)
		free(hostdependency_ary);
	if(servicedependency_ary)
		free(servicedependency_ary);

	hostdependency_ary = calloc(num_objects.hostdependencies, sizeof(void *));
	servicedependency_ary = calloc(num_objects.servicedependencies, sizeof(void *));

	slot = 0;
	for(i = 0; slot < num_objects.servicedependencies && i < num_objects.services; i++) {
		service *s = service_ary[i];
		for(list = s->notify_deps; list; list = list->next)
			servicedependency_ary[slot++] = (servicedependency *)list->object_ptr;
		for(list = s->exec_deps; list; list = list->next)
			servicedependency_ary[slot++] = (servicedependency *)list->object_ptr;
	}
	timing_point("Done post-processing servicedependencies\n");

	slot = 0;
	for(i = 0; slot < num_objects.hostdependencies && i < num_objects.hosts; i++) {
		host *h = host_ary[i];
		for(list = h->notify_deps; list; list = list->next)
			hostdependency_ary[slot++] = (hostdependency *)list->object_ptr;
		for(list = h->exec_deps; list; list = list->next)
			hostdependency_ary[slot++] = (hostdependency *)list->object_ptr;
	}
	timing_point("Done post-processing host dependencies\n");

#ifndef NSCGI
	/* cgi's always get their objects in sorted order */
	if(servicedependency_ary)
		qsort(servicedependency_ary, num_objects.servicedependencies, sizeof(servicedependency *), cmp_sdep);
	if(hostdependency_ary)
		qsort(hostdependency_ary, num_objects.hostdependencies, sizeof(hostdependency *), cmp_hdep);
	if(hostescalation_ary)
		qsort(hostescalation_ary, num_objects.hostescalations, sizeof(hostescalation *), cmp_hostesc);
	if(serviceescalation_ary)
		qsort(serviceescalation_ary, num_objects.serviceescalations, sizeof(serviceescalation *), cmp_serviceesc);
	timing_point("Done post-sorting slave objects\n");
#endif

	timeperiod_list = timeperiod_ary ? *timeperiod_ary : NULL;
	command_list = command_ary ? *command_ary : NULL;
	hostgroup_list = hostgroup_ary ? *hostgroup_ary : NULL;
	contactgroup_list = contactgroup_ary ? *contactgroup_ary : NULL;
	servicegroup_list = servicegroup_ary ? *servicegroup_ary : NULL;
	contact_list = contact_ary ? *contact_ary : NULL;
	host_list = host_ary ? *host_ary : NULL;
	service_list = service_ary ? *service_ary : NULL;
	hostescalation_list = hostescalation_ary ? *hostescalation_ary : NULL;
	serviceescalation_list = serviceescalation_ary ? *serviceescalation_ary : NULL;
}

/* simple state-name helpers, nifty to have all over the place */
const char *service_state_name(int state)
{
	switch (state) {
	case STATE_OK: return "OK";
	case STATE_WARNING: return "WARNING";
	case STATE_CRITICAL: return "CRITICAL";
	}

	return "UNKNOWN";
}

const char *host_state_name(int state)
{
	switch (state) {
	case HOST_UP: return "UP";
	case HOST_DOWN: return "DOWN";
	case HOST_UNREACHABLE: return "UNREACHABLE";
	}

	return "(unknown)";
}

const char *state_type_name(int state_type)
{
	return state_type == HARD_STATE ? "HARD" : "SOFT";
}

const char *check_type_name(int check_type)
{
	return check_type == CHECK_TYPE_PASSIVE ? "PASSIVE" : "ACTIVE";
}

/******************************************************************/
/******* TOP-LEVEL HOST CONFIGURATION DATA INPUT FUNCTION *********/
/******************************************************************/


/* read all host configuration data from external source */
int read_object_config_data(const char *main_config_file, int options) {
	int result = OK;

	/* reset object counts */
	memset(&num_objects, 0, sizeof(num_objects));

	/* read in data from all text host config files (template-based) */
	result = xodtemplate_read_config_data(main_config_file, options);
	if(result != OK)
		return ERROR;

	/* handle any remaining config mangling */
	post_process_object_config();
	timing_point("Done post-processing configuration\n");

	return result;
	}



/******************************************************************/
/******************** SKIPLIST FUNCTIONS **************************/
/******************************************************************/


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


int get_host_count(void) {
	return num_objects.hosts;
	}


int get_service_count(void) {
	return num_objects.services;
	}




/******************************************************************/
/**************** OBJECT ADDITION FUNCTIONS ***********************/
/******************************************************************/

static int create_object_table(const char *name, unsigned int elems, unsigned int size, void **ptr)
{
	void *ret;
	if (!elems) {
		*ptr = NULL;
		return OK;
		}
	ret = calloc(elems, size);
	if (!ret) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to allocate %s table with %u elements\n", name, elems);
		return ERROR;
		}
	*ptr = ret;
	return OK;
}

#define mktable(name, id) \
	create_object_table(#name, ocount[id], sizeof(name *), (void **)&name##_ary)

/* ocount is an array with NUM_OBJECT_TYPES members */
int create_object_tables(unsigned int *ocount)
{
	int i;

	for (i = 0; i < NUM_HASHED_OBJECT_TYPES; i++) {
		const unsigned int hash_size = ocount[i];
		if (!hash_size)
			continue;
		object_hash_tables[i] = dkhash_create(hash_size);
		if (!object_hash_tables[i]) {
			logit(NSLOG_CONFIG_ERROR, TRUE, "Failed to create hash table with %u entries\n", hash_size);
		}
	}

	/*
	 * errors here will always lead to an early exit, so there's no need
	 * to free() successful allocs when later ones fail
	 */
	if (mktable(timeperiod, TIMEPERIOD_SKIPLIST) != OK)
		return ERROR;
	if (mktable(command, COMMAND_SKIPLIST) != OK)
		return ERROR;
	if (mktable(host, HOST_SKIPLIST) != OK)
		return ERROR;
	if (mktable(service, SERVICE_SKIPLIST) != OK)
		return ERROR;
	if (mktable(contact, CONTACT_SKIPLIST) != OK)
		return ERROR;
	if (mktable(hostgroup, HOSTGROUP_SKIPLIST) != OK)
		return ERROR;
	if (mktable(servicegroup, SERVICEGROUP_SKIPLIST) != OK)
		return ERROR;
	if (mktable(contactgroup, CONTACTGROUP_SKIPLIST) != OK)
		return ERROR;
	if (mktable(hostescalation, HOSTESCALATION_SKIPLIST) != OK)
		return ERROR;
	if (mktable(hostdependency, HOSTDEPENDENCY_SKIPLIST) != OK)
		return ERROR;
	if (mktable(serviceescalation, SERVICEESCALATION_SKIPLIST) != OK)
		return ERROR;
	if (mktable(servicedependency, SERVICEDEPENDENCY_SKIPLIST) != OK)
		return ERROR;

	return OK;
}


/* add a new timeperiod to the list in memory */
timeperiod *add_timeperiod(char *name, char *alias) {
	timeperiod *new_timeperiod = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if((name == NULL || !strcmp(name, "")) || (alias == NULL || !strcmp(alias, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Name or alias for timeperiod is NULL\n");
		return NULL;
		}

	new_timeperiod = calloc(1, sizeof(*new_timeperiod));
	if(!new_timeperiod)
		return NULL;

	/* copy string vars */
	new_timeperiod->name = name;
	new_timeperiod->alias = alias ? alias : name;

	/* add new timeperiod to hash table */
	if(result == OK) {
		result = dkhash_insert(object_hash_tables[TIMEPERIOD_SKIPLIST], new_timeperiod->name, NULL, new_timeperiod);
		switch(result) {
			case DKHASH_EDUPE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Timeperiod '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case DKHASH_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add timeperiod '%s' to hash table\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		free(new_timeperiod);
		return NULL;
		}

	new_timeperiod->id = num_objects.timeperiods++;
	if(new_timeperiod->id)
		timeperiod_ary[new_timeperiod->id - 1]->next = new_timeperiod;
	timeperiod_ary[new_timeperiod->id] = new_timeperiod;
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
	timerange *prev = NULL, *tr, *new_timerange = NULL;

	/* make sure we have the data we need */
	if(period == NULL)
		return NULL;

	if(day < 0 || day > 6) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Day %d is not valid for timeperiod '%s'\n", day, period->name);
		return NULL;
		}
	if(start_time > 86400) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Start time %lu on day %d is not valid for timeperiod '%s'\n", start_time, day, period->name);
		return NULL;
		}
	if(end_time > 86400) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: End time %lu on day %d is not value for timeperiod '%s'\n", end_time, day, period->name);
		return NULL;
		}

	/* allocate memory for the new time range */
	if((new_timerange = malloc(sizeof(timerange))) == NULL)
		return NULL;

	new_timerange->range_start = start_time;
	new_timerange->range_end = end_time;

	/* insertion-sort the new time range into the list for this day */
	if(!period->days[day] || period->days[day]->range_start > new_timerange->range_start) {
		new_timerange->next = period->days[day];
		period->days[day] = new_timerange;
		return new_timerange;
		}

	for(tr = period->days[day]; tr; tr = tr->next) {
		if(new_timerange->range_start < tr->range_start) {
			new_timerange->next = tr;
			prev->next = new_timerange;
			break;
			}
		if(!tr->next) {
			tr->next = new_timerange;
			new_timerange->next = NULL;
			break;
			}
		prev = tr;
		}

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

	if(start_time > 86400) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Start time %lu is not valid for timeperiod\n", start_time);
		return NULL;
		}
	if(end_time > 86400) {
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
host *add_host(char *name, char *display_name, char *alias, char *address, char *check_period, int initial_state, double check_interval, double retry_interval, int max_attempts, int notification_options, double notification_interval, double first_notification_delay, char *notification_period, int notifications_enabled, char *check_command, int checks_enabled, int accept_passive_checks, char *event_handler, int event_handler_enabled, int flap_detection_enabled, double low_flap_threshold, double high_flap_threshold, int flap_detection_options, int stalking_options, int process_perfdata, int check_freshness, int freshness_threshold, char *notes, char *notes_url, char *action_url, char *icon_image, char *icon_image_alt, char *vrml_image, char *statusmap_image, int x_2d, int y_2d, int have_2d_coords, double x_3d, double y_3d, double z_3d, int have_3d_coords, int should_be_drawn, int retain_status_information, int retain_nonstatus_information, int obsess, unsigned int hourly_value) {
	host *new_host = NULL;
	timeperiod *check_tp = NULL, *notify_tp = NULL;
	int result = OK;

	/* make sure we have the data we need */
	if(name == NULL || !strcmp(name, "")) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Host name is NULL\n");
		return NULL;
		}

	if(check_period && !(check_tp = find_timeperiod(check_period))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to locate check_period '%s' for host '%s'!\n",
			  check_period, name);
		return NULL;
	}
	if(notification_period && !(notify_tp = find_timeperiod(notification_period))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to locate notification_period '%s' for host '%s'!\n",
			  notification_period, name);
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

	new_host = calloc(1, sizeof(*new_host));

	/* assign string vars */
	new_host->name = name;
	new_host->display_name = display_name ? display_name : new_host->name;
	new_host->alias = alias ? alias : new_host->name;
	new_host->address = address ? address : new_host->name;
	new_host->check_period = check_tp ? (char *)strdup(check_tp->name) : NULL;
	new_host->notification_period = notify_tp ? (char *)strdup(notify_tp->name) : NULL;
	new_host->notification_period_ptr = notify_tp;
	new_host->check_period_ptr = check_tp;
	new_host->check_command = check_command;
	new_host->event_handler = event_handler;
	new_host->notes = notes;
	new_host->notes_url = notes_url;
	new_host->action_url = action_url;
	new_host->icon_image = icon_image;
	new_host->icon_image_alt = icon_image_alt;
	new_host->vrml_image = vrml_image;
	new_host->statusmap_image = statusmap_image;

	/* duplicate non-string vars */
	new_host->hourly_value = hourly_value;
	new_host->max_attempts = max_attempts;
	new_host->check_interval = check_interval;
	new_host->retry_interval = retry_interval;
	new_host->notification_interval = notification_interval;
	new_host->first_notification_delay = first_notification_delay;
	new_host->notification_options = notification_options;
	new_host->flap_detection_enabled = (flap_detection_enabled > 0) ? TRUE : FALSE;
	new_host->low_flap_threshold = low_flap_threshold;
	new_host->high_flap_threshold = high_flap_threshold;
	new_host->flap_detection_options = flap_detection_options;
	new_host->stalking_options = stalking_options;
	new_host->process_performance_data = (process_perfdata > 0) ? TRUE : FALSE;
	new_host->check_freshness = (check_freshness > 0) ? TRUE : FALSE;
	new_host->freshness_threshold = freshness_threshold;
	new_host->checks_enabled = (checks_enabled > 0) ? TRUE : FALSE;
	new_host->accept_passive_checks = (accept_passive_checks > 0) ? TRUE : FALSE;
	new_host->event_handler_enabled = (event_handler_enabled > 0) ? TRUE : FALSE;
	new_host->x_2d = x_2d;
	new_host->y_2d = y_2d;
	new_host->have_2d_coords = (have_2d_coords > 0) ? TRUE : FALSE;
	new_host->x_3d = x_3d;
	new_host->y_3d = y_3d;
	new_host->z_3d = z_3d;
	new_host->have_3d_coords = (have_3d_coords > 0) ? TRUE : FALSE;
	new_host->should_be_drawn = (should_be_drawn > 0) ? TRUE : FALSE;
	new_host->obsess = (obsess > 0) ? TRUE : FALSE;
	new_host->retain_status_information = (retain_status_information > 0) ? TRUE : FALSE;
	new_host->retain_nonstatus_information = (retain_nonstatus_information > 0) ? TRUE : FALSE;
#ifdef NSCORE
	new_host->current_state = initial_state;
	new_host->last_state = initial_state;
	new_host->last_hard_state = initial_state;
	new_host->check_type = CHECK_TYPE_ACTIVE;
	new_host->should_be_scheduled = TRUE;
	new_host->current_attempt = (initial_state == HOST_UP) ? 1 : max_attempts;
	new_host->state_type = HARD_STATE;
	new_host->acknowledgement_type = ACKNOWLEDGEMENT_NONE;
	new_host->notifications_enabled = (notifications_enabled > 0) ? TRUE : FALSE;
	new_host->check_options = CHECK_OPTION_NONE;
#endif

	/* add new host to hash table */
	if(result == OK) {
		result = dkhash_insert(object_hash_tables[HOST_SKIPLIST], new_host->name, NULL, new_host);
		switch(result) {
			case DKHASH_EDUPE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Host '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case DKHASH_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add host '%s' to hash table\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_host);
		return NULL;
		}

	new_host->id = num_objects.hosts++;
	host_ary[new_host->id] = new_host;
	if(new_host->id)
		host_ary[new_host->id - 1]->next = new_host;
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

servicesmember *add_parent_service_to_service(service *svc, char *host_name, char *description) {
	servicesmember *sm;

	if(!svc || !host_name || !description || !*host_name || !*description)
		return NULL;

	if((sm = calloc(1, sizeof(*sm))) == NULL)
		return NULL;

	if (strcmp(svc->host_name, host_name) == 0 && strcmp(svc->description, description) == 0) {
		logit(NSLOG_CONFIG_ERROR, TRUE,
				"Error: Host '%s' Service '%s' cannot be a child/parent of itself\n",
				svc->host_name, svc->description);
		return NULL;
	}

	if ((sm->host_name = strdup(host_name)) == NULL || (sm->service_description = strdup(description)) == NULL) {
		/* there was an error copying (description is NULL now) */
		my_free(sm->host_name);
		free(sm);
		return NULL;
		}

	sm->next = svc->parents;
	svc->parents = sm;
	return sm;
	}

hostsmember *add_child_link_to_host(host *hst, host *child_ptr) {
	hostsmember *new_hostsmember = NULL;

	/* make sure we have the data we need */
	if(hst == NULL || child_ptr == NULL)
		return NULL;

	/* allocate memory */
	if((new_hostsmember = (hostsmember *)malloc(sizeof(hostsmember))) == NULL)
		return NULL;

	/* assign values */
	new_hostsmember->host_name = child_ptr->name;
	new_hostsmember->host_ptr = child_ptr;

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

	/* assign values */
	new_servicesmember->host_name = service_ptr->host_name;
	new_servicesmember->service_description = service_ptr->description;
	new_servicesmember->service_ptr = service_ptr;
#ifndef NSCGI
	hst->total_services++;
#endif

	/* add the child entry to the host definition */
	new_servicesmember->next = hst->services;
	hst->services = new_servicesmember;

	return new_servicesmember;
	}



servicesmember *add_child_link_to_service(service *svc, service *child_ptr) {
	servicesmember *new_servicesmember = NULL;

	/* make sure we have the data we need */
	if(svc == NULL || child_ptr == NULL)
		return NULL;

	/* allocate memory */
	if((new_servicesmember = (servicesmember *)malloc(sizeof(servicesmember))) 
			== NULL) return NULL;

	/* assign values */
	new_servicesmember->host_name = child_ptr->host_name;
	new_servicesmember->service_description = child_ptr->description;
	new_servicesmember->service_ptr = child_ptr;

	/* add the child entry to the host definition */
	new_servicesmember->next = svc->children;
	svc->children = new_servicesmember;

	return new_servicesmember;
	}



static contactgroupsmember *add_contactgroup_to_object(contactgroupsmember **cg_list, const char *group_name) {
	contactgroupsmember *cgm;
	contactgroup *cg;

	if(!group_name || !*group_name) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact name is NULL\n");
		return NULL;
		}
	if(!(cg = find_contactgroup(group_name))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contactgroup '%s' is not defined anywhere\n", group_name);
		return NULL;
		}
	if(!(cgm = malloc(sizeof(*cgm)))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not allocate memory for contactgroup\n");
		return NULL;
		}
	cgm->group_name = cg->group_name;
	cgm->group_ptr = cg;
	cgm->next = *cg_list;
	*cg_list = cgm;

	return cgm;
	}


/* add a new contactgroup to a host */
contactgroupsmember *add_contactgroup_to_host(host *hst, char *group_name) {
	return add_contactgroup_to_object(&hst->contact_groups, group_name);
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

	new_hostgroup = calloc(1, sizeof(*new_hostgroup));

	/* assign vars */
	new_hostgroup->group_name = name;
	new_hostgroup->alias = alias ? alias : name;
	new_hostgroup->notes = notes;
	new_hostgroup->notes_url = notes_url;
	new_hostgroup->action_url = action_url;

	/* add new host group to hash table */
	if(result == OK) {
		result = dkhash_insert(object_hash_tables[HOSTGROUP_SKIPLIST], new_hostgroup->group_name, NULL, new_hostgroup);
		switch(result) {
			case DKHASH_EDUPE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Hostgroup '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case DKHASH_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add hostgroup '%s' to hash table\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		free(new_hostgroup);
		return NULL;
		}

	new_hostgroup->id = num_objects.hostgroups++;
	hostgroup_ary[new_hostgroup->id] = new_hostgroup;
	if(new_hostgroup->id)
		hostgroup_ary[new_hostgroup->id - 1]->next = new_hostgroup;
	return new_hostgroup;
	}


/* add a new host to a host group */
hostsmember *add_host_to_hostgroup(hostgroup *temp_hostgroup, char *host_name) {
	hostsmember *new_member = NULL;
	hostsmember *last_member = NULL;
	hostsmember *temp_member = NULL;
	struct host *h;

	/* make sure we have the data we need */
	if(temp_hostgroup == NULL || (host_name == NULL || !strcmp(host_name, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Hostgroup or group member is NULL\n");
		return NULL;
		}
	if (!(h = find_host(host_name))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to locate host '%s' for hostgroup '%s'\n", host_name, temp_hostgroup->group_name);
		return NULL;
	}

	/* allocate memory for a new member */
	if((new_member = calloc(1, sizeof(hostsmember))) == NULL)
		return NULL;

	/* assign vars */
	new_member->host_name = h->name;
	new_member->host_ptr = h;

	/* add (unsorted) link from the host to its group */
	prepend_object_to_objectlist(&h->hostgroups_ptr, (void *)temp_hostgroup);

	/* add the new member to the member list, sorted by host name */
#ifndef NSCGI
	if(use_large_installation_tweaks == TRUE) {
		new_member->next = temp_hostgroup->members;
		temp_hostgroup->members = new_member;
		return new_member;
		}
#endif
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

	new_servicegroup = calloc(1, sizeof(*new_servicegroup));

	/* duplicate vars */
	new_servicegroup->group_name = name;
	new_servicegroup->alias = alias ? alias : name;
	new_servicegroup->notes = notes;
	new_servicegroup->notes_url = notes_url;
	new_servicegroup->action_url = action_url;

	/* add new service group to hash table */
	if(result == OK) {
		result = dkhash_insert(object_hash_tables[SERVICEGROUP_SKIPLIST], new_servicegroup->group_name, NULL, new_servicegroup);
		switch(result) {
			case DKHASH_EDUPE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Servicegroup '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case DKHASH_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add servicegroup '%s' to hash table\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_servicegroup);
		return NULL;
		}

	new_servicegroup->id = num_objects.servicegroups++;
	servicegroup_ary[new_servicegroup->id] = new_servicegroup;
	if(new_servicegroup->id)
		servicegroup_ary[new_servicegroup->id - 1]->next = new_servicegroup;
	return new_servicegroup;
	}


/* add a new service to a service group */
servicesmember *add_service_to_servicegroup(servicegroup *temp_servicegroup, char *host_name, char *svc_description) {
	servicesmember *new_member = NULL;
	servicesmember *last_member = NULL;
	servicesmember *temp_member = NULL;
	struct service *svc;

	/* make sure we have the data we need */
	if(temp_servicegroup == NULL || (host_name == NULL || !strcmp(host_name, "")) || (svc_description == NULL || !strcmp(svc_description, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Servicegroup or group member is NULL\n");
		return NULL;
		}
	if (!(svc = find_service(host_name, svc_description))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to locate service '%s' on host '%s' for servicegroup '%s'\n", svc_description, host_name, temp_servicegroup->group_name);
		return NULL;
		}

	/* allocate memory for a new member */
	if((new_member = calloc(1, sizeof(servicesmember))) == NULL)
		return NULL;

	/* assign vars */
	new_member->host_name = svc->host_name;
	new_member->service_description = svc->description;
	new_member->service_ptr = svc;

	/* add (unsorted) link from the service to its groups */
	prepend_object_to_objectlist(&svc->servicegroups_ptr, temp_servicegroup);

	/*
	 * add new member to member list, sorted by host name then
	 * service description, unless we're a large installation, in
	 * which case insertion-sorting will take far too long
	 */
#ifndef NSCGI
	if(use_large_installation_tweaks == TRUE) {
		new_member->next = temp_servicegroup->members;
		temp_servicegroup->members = new_member;
		return new_member;
		}
#endif
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
contact *add_contact(char *name, char *alias, char *email, char *pager, char **addresses, char *svc_notification_period, char *host_notification_period, int service_notification_options, int host_notification_options, int host_notifications_enabled, int service_notifications_enabled, int can_submit_commands, int retain_status_information, int retain_nonstatus_information, unsigned int minimum_value) {
	contact *new_contact = NULL;
	timeperiod *htp = NULL, *stp = NULL;
	int x = 0;
	int result = OK;

	/* make sure we have the data we need */
	if(name == NULL || !*name) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact name is NULL\n");
		return NULL;
		}
	if(svc_notification_period && !(stp = find_timeperiod(svc_notification_period))) {
		logit(NSLOG_VERIFICATION_ERROR, TRUE, "Error: Service notification period '%s' specified for contact '%s' is not defined anywhere!\n",
			  svc_notification_period, name);
		return NULL;
		}
	if(host_notification_period && !(htp = find_timeperiod(host_notification_period))) {
		logit(NSLOG_VERIFICATION_ERROR, TRUE, "Error: Host notification period '%s' specified for contact '%s' is not defined anywhere!\n",
			  host_notification_period, name);
		return NULL;
		}


	new_contact = calloc(1, sizeof(*new_contact));
	if(!new_contact)
		return NULL;

	new_contact->host_notification_period = htp ? (char *)strdup(htp->name) : NULL;
	new_contact->service_notification_period = stp ? (char *)strdup(stp->name) : NULL;
	new_contact->host_notification_period_ptr = htp;
	new_contact->service_notification_period_ptr = stp;
	new_contact->name = name;
	new_contact->alias = alias ? alias : name;
	new_contact->email = email;
	new_contact->pager = pager;
	if(addresses) {
		for(x = 0; x < MAX_CONTACT_ADDRESSES; x++)
			new_contact->address[x] = addresses[x];
		}

	new_contact->minimum_value = minimum_value;
	new_contact->service_notification_options = service_notification_options;
	new_contact->host_notification_options = host_notification_options;
	new_contact->host_notifications_enabled = (host_notifications_enabled > 0) ? TRUE : FALSE;
	new_contact->service_notifications_enabled = (service_notifications_enabled > 0) ? TRUE : FALSE;
	new_contact->can_submit_commands = (can_submit_commands > 0) ? TRUE : FALSE;
	new_contact->retain_status_information = (retain_status_information > 0) ? TRUE : FALSE;
	new_contact->retain_nonstatus_information = (retain_nonstatus_information > 0) ? TRUE : FALSE;

	/* add new contact to hash table */
	if(result == OK) {
		result = dkhash_insert(object_hash_tables[CONTACT_SKIPLIST], new_contact->name, NULL, new_contact);
		switch(result) {
			case DKHASH_EDUPE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case DKHASH_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contact '%s' to hash table\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		free(new_contact);
		return NULL;
		}

	new_contact->id = num_objects.contacts++;
	contact_ary[new_contact->id] = new_contact;
	if(new_contact->id)
		contact_ary[new_contact->id - 1]->next = new_contact;
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

	new_contactgroup = calloc(1, sizeof(*new_contactgroup));
	if(!new_contactgroup)
		return NULL;

	/* assign vars */
	new_contactgroup->group_name = name;
	new_contactgroup->alias = alias ? alias : name;

	/* add new contact group to hash table */
	if(result == OK) {
		result = dkhash_insert(object_hash_tables[CONTACTGROUP_SKIPLIST], new_contactgroup->group_name, NULL, new_contactgroup);
		switch(result) {
			case DKHASH_EDUPE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contactgroup '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case DKHASH_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add contactgroup '%s' to hash table\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		free(new_contactgroup);
		return NULL;
		}

	new_contactgroup->id = num_objects.contactgroups++;
	contactgroup_ary[new_contactgroup->id] = new_contactgroup;
	if(new_contactgroup->id)
		contactgroup_ary[new_contactgroup->id - 1]->next = new_contactgroup;
	return new_contactgroup;
	}



/* add a new member to a contact group */
contactsmember *add_contact_to_contactgroup(contactgroup *grp, char *contact_name) {
	contactsmember *new_contactsmember = NULL;
	struct contact *c;

	/* make sure we have the data we need */
	if(grp == NULL || (contact_name == NULL || !strcmp(contact_name, ""))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contactgroup or contact name is NULL\n");
		return NULL;
		}

	if (!(c = find_contact(contact_name))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to locate contact '%s' for contactgroup '%s'\n", contact_name, grp->group_name);
		return NULL;
		}

	/* allocate memory for a new member */
	if((new_contactsmember = calloc(1, sizeof(contactsmember))) == NULL)
		return NULL;

	/* assign vars */
	new_contactsmember->contact_name = c->name;
	new_contactsmember->contact_ptr = c;

	/* add the new member to the head of the member list */
	new_contactsmember->next = grp->members;
	grp->members = new_contactsmember;

	prepend_object_to_objectlist(&c->contactgroups_ptr, (void *)grp);

	return new_contactsmember;
	}



/* add a new service to the list in memory */
service *add_service(char *host_name, char *description, char *display_name, char *check_period, int initial_state, int max_attempts, int parallelize, int accept_passive_checks, double check_interval, double retry_interval, double notification_interval, double first_notification_delay, char *notification_period, int notification_options, int notifications_enabled, int is_volatile, char *event_handler, int event_handler_enabled, char *check_command, int checks_enabled, int flap_detection_enabled, double low_flap_threshold, double high_flap_threshold, int flap_detection_options, int stalking_options, int process_perfdata, int check_freshness, int freshness_threshold, char *notes, char *notes_url, char *action_url, char *icon_image, char *icon_image_alt, int retain_status_information, int retain_nonstatus_information, int obsess, unsigned int hourly_value) {
	host *h;
	timeperiod *cp = NULL, *np = NULL;
	service *new_service = NULL;
	int result = OK;

	/* make sure we have everything we need */
	if(host_name == NULL || description == NULL || !*description || check_command == NULL || !*check_command) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Service description, host name, or check command is NULL\n");
		return NULL;
		}
	if (!(h = find_host(host_name))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Unable to locate host '%s' for service '%s'\n",
			  host_name, description);
		return NULL;
		}
	if(notification_period && !(np = find_timeperiod(notification_period))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: notification_period '%s' for service '%s' on host '%s' could not be found!\n",
			  notification_period, description, host_name);
		return NULL;
		}
	if(check_period && !(cp = find_timeperiod(check_period))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: check_period '%s' for service '%s' on host '%s' not found!\n",
			  check_period, description, host_name);
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
	new_service = calloc(1, sizeof(*new_service));
	if(!new_service)
		return NULL;

	/* duplicate vars, but assign what we can */
	new_service->notification_period_ptr = np;
	new_service->check_period_ptr = cp;
	new_service->host_ptr = h;
	new_service->check_period = cp ? (char *)strdup(cp->name) : NULL;
	new_service->notification_period = np ? (char *)strdup(np->name) : NULL;
	new_service->host_name = h->name;
	if((new_service->description = (char *)strdup(description)) == NULL)
		result = ERROR;
	if(display_name) {
		if((new_service->display_name = (char *)strdup(display_name)) == NULL)
			result = ERROR;
		}
	else {
		new_service->display_name = new_service->description;
		}
	if((new_service->check_command = (char *)strdup(check_command)) == NULL)
		result = ERROR;
	if(event_handler) {
		if((new_service->event_handler = (char *)strdup(event_handler)) == NULL)
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

	new_service->hourly_value = hourly_value;
	new_service->check_interval = check_interval;
	new_service->retry_interval = retry_interval;
	new_service->max_attempts = max_attempts;
	new_service->parallelize = (parallelize > 0) ? TRUE : FALSE;
	new_service->notification_interval = notification_interval;
	new_service->first_notification_delay = first_notification_delay;
	new_service->notification_options = notification_options;
	new_service->is_volatile = (is_volatile > 0) ? TRUE : FALSE;
	new_service->flap_detection_enabled = (flap_detection_enabled > 0) ? TRUE : FALSE;
	new_service->low_flap_threshold = low_flap_threshold;
	new_service->high_flap_threshold = high_flap_threshold;
	new_service->flap_detection_options = flap_detection_options;
	new_service->stalking_options = stalking_options;
	new_service->process_performance_data = (process_perfdata > 0) ? TRUE : FALSE;
	new_service->check_freshness = (check_freshness > 0) ? TRUE : FALSE;
	new_service->freshness_threshold = freshness_threshold;
	new_service->accept_passive_checks = (accept_passive_checks > 0) ? TRUE : FALSE;
	new_service->event_handler_enabled = (event_handler_enabled > 0) ? TRUE : FALSE;
	new_service->checks_enabled = (checks_enabled > 0) ? TRUE : FALSE;
	new_service->retain_status_information = (retain_status_information > 0) ? TRUE : FALSE;
	new_service->retain_nonstatus_information = (retain_nonstatus_information > 0) ? TRUE : FALSE;
	new_service->notifications_enabled = (notifications_enabled > 0) ? TRUE : FALSE;
	new_service->obsess = (obsess > 0) ? TRUE : FALSE;
#ifdef NSCORE
	new_service->acknowledgement_type = ACKNOWLEDGEMENT_NONE;
	new_service->check_type = CHECK_TYPE_ACTIVE;
	new_service->current_attempt = (initial_state == STATE_OK) ? 1 : max_attempts;
	new_service->current_state = initial_state;
	new_service->last_state = initial_state;
	new_service->last_hard_state = initial_state;
	new_service->state_type = HARD_STATE;
	new_service->should_be_scheduled = TRUE;
	new_service->check_options = CHECK_OPTION_NONE;
#endif

	/* add new service to hash table */
	if(result == OK) {
		result = dkhash_insert(object_hash_tables[SERVICE_SKIPLIST], new_service->host_name, new_service->description, new_service);
		switch(result) {
			case DKHASH_EDUPE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Service '%s' on host '%s' has already been defined\n", description, host_name);
				result = ERROR;
				break;
			case DKHASH_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add service '%s' on host '%s' to hash table\n", description, host_name);
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
		my_free(new_service->event_handler);
		my_free(new_service->check_command);
		my_free(new_service->description);
		if(display_name)
			my_free(new_service->display_name);
		return NULL;
		}

	add_service_link_to_host(h, new_service);

	new_service->id = num_objects.services++;
	service_ary[new_service->id] = new_service;
	if(new_service->id)
		service_ary[new_service->id - 1]->next = new_service;
	return new_service;
	}



/* adds a contact group to a service */
contactgroupsmember *add_contactgroup_to_service(service *svc, char *group_name) {
	return add_contactgroup_to_object(&svc->contact_groups, group_name);
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
	new_command = calloc(1, sizeof(*new_command));
	if(!new_command)
		return NULL;

	/* assign vars */
	new_command->name = name;
	new_command->command_line = value;

	/* add new command to hash table */
	if(result == OK) {
		result = dkhash_insert(object_hash_tables[COMMAND_SKIPLIST], new_command->name, NULL, new_command);
		switch(result) {
			case DKHASH_EDUPE:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Command '%s' has already been defined\n", name);
				result = ERROR;
				break;
			case DKHASH_OK:
				result = OK;
				break;
			default:
				logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add command '%s' to hash table\n", name);
				result = ERROR;
				break;
			}
		}

	/* handle errors */
	if(result == ERROR) {
		my_free(new_command);
		return NULL;
		}

	new_command->id = num_objects.commands++;
	command_ary[new_command->id] = new_command;
	if(new_command->id)
		command_ary[new_command->id - 1]->next = new_command;
	return new_command;
	}



/* add a new service escalation to the list in memory */
serviceescalation *add_serviceescalation(char *host_name, char *description, int first_notification, int last_notification, double notification_interval, char *escalation_period, int escalation_options) {
	serviceescalation *new_serviceescalation = NULL;
	service *svc;
	timeperiod *tp = NULL;

	/* make sure we have the data we need */
	if(host_name == NULL || !*host_name || description == NULL || !*description) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Service escalation host name or description is NULL\n");
		return NULL;
		}
	if(!(svc = find_service(host_name, description))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Service '%s' on host '%s' has an escalation but is not defined anywhere!\n",
			  host_name, description);
		return NULL;
		}
	if (escalation_period && !(tp = find_timeperiod(escalation_period))) {
		logit(NSLOG_VERIFICATION_ERROR, TRUE, "Error: Escalation period '%s' specified in service escalation for service '%s' on host '%s' is not defined anywhere!\n",
			  escalation_period, description, host_name);
		return NULL ;
		}

	new_serviceescalation = calloc(1, sizeof(*new_serviceescalation));
	if(!new_serviceescalation)
		return NULL;

	if(add_object_to_objectlist(&svc->escalation_list, new_serviceescalation) != OK) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Could not add escalation to service '%s' on host '%s'\n",
			  svc->host_name, svc->description);
		return NULL;
		}

	/* assign vars. object names are immutable, so no need to copy */
	new_serviceescalation->host_name = svc->host_name;
	new_serviceescalation->description = svc->description;
	new_serviceescalation->service_ptr = svc;
	new_serviceescalation->escalation_period_ptr = tp;
	if(tp)
		new_serviceescalation->escalation_period = (char *)strdup(tp->name);

	new_serviceescalation->first_notification = first_notification;
	new_serviceescalation->last_notification = last_notification;
	new_serviceescalation->notification_interval = (notification_interval <= 0) ? 0 : notification_interval;
	new_serviceescalation->escalation_options = escalation_options;

	new_serviceescalation->id = num_objects.serviceescalations++;
	serviceescalation_ary[new_serviceescalation->id] = new_serviceescalation;
	return new_serviceescalation;
	}



/* adds a contact group to a service escalation */
contactgroupsmember *add_contactgroup_to_serviceescalation(serviceescalation *se, char *group_name) {
	return add_contactgroup_to_object(&se->contact_groups, group_name);
	}



/* adds a contact to a service escalation */
contactsmember *add_contact_to_serviceescalation(serviceescalation *se, char *contact_name) {

	return add_contact_to_object(&se->contacts, contact_name);
	}



/* adds a service dependency definition */
servicedependency *add_service_dependency(char *dependent_host_name, char *dependent_service_description, char *host_name, char *service_description, int dependency_type, int inherits_parent, int failure_options, char *dependency_period) {
	servicedependency *new_servicedependency = NULL;
	service *parent, *child;
	timeperiod *tp = NULL;
	int result;

	/* make sure we have what we need */
	parent = find_service(host_name, service_description);
	if(!parent) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Master service '%s' on host '%s' is not defined anywhere!\n",
			  service_description, host_name);
		return NULL;
		}
	child = find_service(dependent_host_name, dependent_service_description);
	if(!child) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Dependent service '%s' on host '%s' is not defined anywhere!\n",
			 dependent_service_description, dependent_host_name);
		return NULL;
		}
	if (dependency_period && !(tp = find_timeperiod(dependency_period))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Failed to locate timeperiod '%s' for dependency from service '%s' on host '%s' to service '%s' on host '%s'\n",
			  dependency_period, dependent_service_description, dependent_host_name, service_description, host_name);
		return NULL;
		}

	/* allocate memory for a new service dependency entry */
	if((new_servicedependency = calloc(1, sizeof(*new_servicedependency))) == NULL)
		return NULL;

	new_servicedependency->dependent_service_ptr = child;
	new_servicedependency->master_service_ptr = parent;
	new_servicedependency->dependency_period_ptr = tp;

	/* assign vars. object names are immutable, so no need to copy */
	new_servicedependency->dependent_host_name = child->host_name;
	new_servicedependency->dependent_service_description = child->description;
	new_servicedependency->host_name = parent->host_name;
	new_servicedependency->service_description = parent->description;
	if (tp)
		new_servicedependency->dependency_period = (char *)strdup(tp->name);

	new_servicedependency->dependency_type = (dependency_type == EXECUTION_DEPENDENCY) ? EXECUTION_DEPENDENCY : NOTIFICATION_DEPENDENCY;
	new_servicedependency->inherits_parent = (inherits_parent > 0) ? TRUE : FALSE;
	new_servicedependency->failure_options = failure_options;

	/*
	 * add new service dependency to its respective services.
	 * Ordering doesn't matter here as we'll have to check them
	 * all anyway. We avoid adding dupes though, since we can
	 * apparently get zillion's and zillion's of them.
	 */
	if(dependency_type == NOTIFICATION_DEPENDENCY)
		result = prepend_unique_object_to_objectlist(&child->notify_deps, new_servicedependency, sizeof(*new_servicedependency));
	else
		result = prepend_unique_object_to_objectlist(&child->exec_deps, new_servicedependency, sizeof(*new_servicedependency));

	if(result != OK) {
		free(new_servicedependency);
		/* hack to avoid caller bombing out */
		return result == OBJECTLIST_DUPE ? (void *)1 : NULL;
		}

	num_objects.servicedependencies++;
	return new_servicedependency;
	}


/* adds a host dependency definition */
hostdependency *add_host_dependency(char *dependent_host_name, char *host_name, int dependency_type, int inherits_parent, int failure_options, char *dependency_period) {
	hostdependency *new_hostdependency = NULL;
	host *parent, *child;
	timeperiod *tp = NULL;
	int result;

	/* make sure we have what we need */
	parent = find_host(host_name);
	if (!parent) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Master host '%s' in hostdependency from '%s' to '%s' is not defined anywhere!\n",
			  host_name, dependent_host_name, host_name);
		return NULL;
		}
	child = find_host(dependent_host_name);
	if (!child) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Dependent host '%s' in hostdependency from '%s' to '%s' is not defined anywhere!\n",
			  dependent_host_name, dependent_host_name, host_name);
		return NULL;
		}
	if (dependency_period && !(tp = find_timeperiod(dependency_period))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Unable to locate dependency_period '%s' for %s->%s host dependency\n",
			  dependency_period, parent->name, child->name);
		return NULL ;
	}

	if((new_hostdependency = calloc(1, sizeof(*new_hostdependency))) == NULL)
		return NULL;

	new_hostdependency->dependent_host_ptr = child;
	new_hostdependency->master_host_ptr = parent;
	new_hostdependency->dependency_period_ptr = tp;

	/* assign vars. Objects are immutable, so no need to copy */
	new_hostdependency->dependent_host_name = child->name;
	new_hostdependency->host_name = parent->name;
	if(tp)
		new_hostdependency->dependency_period = (char *)strdup(tp->name);

	new_hostdependency->dependency_type = (dependency_type == EXECUTION_DEPENDENCY) ? EXECUTION_DEPENDENCY : NOTIFICATION_DEPENDENCY;
	new_hostdependency->inherits_parent = (inherits_parent > 0) ? TRUE : FALSE;
	new_hostdependency->failure_options = failure_options;

	if(dependency_type == NOTIFICATION_DEPENDENCY)
		result = prepend_unique_object_to_objectlist(&child->notify_deps, new_hostdependency, sizeof(*new_hostdependency));
	else
		result = prepend_unique_object_to_objectlist(&child->exec_deps, new_hostdependency, sizeof(*new_hostdependency));

	if(result != OK) {
		free(new_hostdependency);
		/* hack to avoid caller bombing out */
		return result == OBJECTLIST_DUPE ? (void *)1 : NULL;
		}

	num_objects.hostdependencies++;
	return new_hostdependency;
	}



/* add a new host escalation to the list in memory */
hostescalation *add_hostescalation(char *host_name, int first_notification, int last_notification, double notification_interval, char *escalation_period, int escalation_options) {
	hostescalation *new_hostescalation = NULL;
	host *h;
	timeperiod *tp = NULL;

	/* make sure we have the data we need */
	if(host_name == NULL || !*host_name) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Host escalation host name is NULL\n");
		return NULL;
		}
	if (!(h = find_host(host_name))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Host '%s' has an escalation, but is not defined anywhere!\n", host_name);
		return NULL;
		}
	if (escalation_period && !(tp = find_timeperiod(escalation_period))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Unable to locate timeperiod '%s' for hostescalation '%s'\n",
			  escalation_period, host_name);
		return NULL;
		}

	new_hostescalation = calloc(1, sizeof(*new_hostescalation));

	/* add the escalation to its host */
	if (add_object_to_objectlist(&h->escalation_list, new_hostescalation) != OK) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not add hostescalation to host '%s'\n", host_name);
		free(new_hostescalation);
		return NULL;
		}

	/* assign vars. Object names are immutable, so no need to copy */
	new_hostescalation->host_name = h->name;
	new_hostescalation->host_ptr = h;
	new_hostescalation->escalation_period = tp ? (char *)strdup(tp->name) : NULL;
	new_hostescalation->escalation_period_ptr = tp;
	new_hostescalation->first_notification = first_notification;
	new_hostescalation->last_notification = last_notification;
	new_hostescalation->notification_interval = (notification_interval <= 0) ? 0 : notification_interval;
	new_hostescalation->escalation_options = escalation_options;

	new_hostescalation->id = num_objects.hostescalations++;
	hostescalation_ary[new_hostescalation->id] = new_hostescalation;
	return new_hostescalation;
	}



/* adds a contact group to a host escalation */
contactgroupsmember *add_contactgroup_to_hostescalation(hostescalation *he, char *group_name) {
	return add_contactgroup_to_object(&he->contact_groups, group_name);
	}



/* adds a contact to a host escalation */
contactsmember *add_contact_to_hostescalation(hostescalation *he, char *contact_name) {

	return add_contact_to_object(&he->contacts, contact_name);
	}



/* adds a contact to an object */
contactsmember *add_contact_to_object(contactsmember **object_ptr, char *contactname) {
	contactsmember *new_contactsmember = NULL;
	contact *c;

	/* make sure we have the data we need */
	if(object_ptr == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact object is NULL\n");
		return NULL;
		}

	if(contactname == NULL || !*contactname) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact name is NULL\n");
		return NULL;
		}
	if(!(c = find_contact(contactname))) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Contact '%s' is not defined anywhere!\n", contactname);
		return NULL;
		}

	/* allocate memory for a new member */
	if((new_contactsmember = malloc(sizeof(contactsmember))) == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not allocate memory for contact\n");
		return NULL;
		}
	new_contactsmember->contact_name = c->name;

	/* set initial values */
	new_contactsmember->contact_ptr = c;

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

timeperiod *find_timeperiod(const char *name) {
	return dkhash_get(object_hash_tables[TIMEPERIOD_SKIPLIST], name, NULL);
	}

host *find_host(const char *name) {
	return dkhash_get(object_hash_tables[HOST_SKIPLIST], name, NULL);
	}

hostgroup *find_hostgroup(const char *name) {
	return dkhash_get(object_hash_tables[HOSTGROUP_SKIPLIST], name, NULL);
	}

servicegroup *find_servicegroup(const char *name) {
	return dkhash_get(object_hash_tables[SERVICEGROUP_SKIPLIST], name, NULL);
	}

contact *find_contact(const char *name) {
	return dkhash_get(object_hash_tables[CONTACT_SKIPLIST], name, NULL);
	}

contactgroup *find_contactgroup(const char *name) {
	return dkhash_get(object_hash_tables[CONTACTGROUP_SKIPLIST], name, NULL);
	}

command *find_command(const char *name) {
	return dkhash_get(object_hash_tables[COMMAND_SKIPLIST], name, NULL);
	}

service *find_service(const char *host_name, const char *svc_desc) {
	return dkhash_get(object_hash_tables[SERVICE_SKIPLIST], host_name, svc_desc);
	}



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


/* useful when we don't care if the object is unique or not */
int prepend_object_to_objectlist(objectlist **list, void *object_ptr)
{
	objectlist *item;
	if(list == NULL || object_ptr == NULL)
		return ERROR;
	if((item = malloc(sizeof(*item))) == NULL)
		return ERROR;
	item->next = *list;
	item->object_ptr = object_ptr;
	*list = item;
	return OK;
	}

/* useful for adding dependencies to master objects */
int prepend_unique_object_to_objectlist(objectlist **list, void *object_ptr, size_t size) {
	objectlist *l;
	if(list == NULL || object_ptr == NULL)
		return ERROR;
	for(l = *list; l; l = l->next) {
		if(!memcmp(l->object_ptr, object_ptr, size))
			return OBJECTLIST_DUPE;
		}
	return prepend_object_to_objectlist(list, object_ptr);
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
			if(temp_hostsmember->host_ptr == parent_host)
				return TRUE;
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

#ifdef NSCGI
int hostsmember_elements(hostsmember *list)
{
	int elems = 0;
	for (; list; list = list->next)
		elems++;
	return elems;
}

/* returns a count of the immediate children for a given host */
/* NOTE: This function is only used by the CGIS */
int number_of_immediate_child_hosts(host *hst) {
	int children = 0;
	host *temp_host = NULL;

	if(hst == NULL) {
		for(temp_host = host_list; temp_host != NULL;
				temp_host = temp_host->next) {
			if(is_host_immediate_child_of_host(hst, temp_host) == TRUE)
				children++;
			}
		return children;
		}
	else {
		return hostsmember_elements(hst->child_hosts);
		}
	}


/* get the number of immediate parent hosts for a given host */
/* NOTE: This function is only used by the CGIS */
int number_of_immediate_parent_hosts(host *hst) {
	int parents = 0;
	host *temp_host = NULL;

	if(hst == NULL) {
		for(temp_host = host_list; temp_host != NULL;
				temp_host = temp_host->next) {
			if(is_host_immediate_parent_of_host(hst, temp_host) == TRUE) {
				parents++;
				}
			}
		return parents;
		}
	else {
		return hostsmember_elements(hst->parent_hosts);
		}
	}
#endif

/*  tests whether a host is a member of a particular hostgroup */
/* NOTE: This function is only used by the CGIS */
int is_host_member_of_hostgroup(hostgroup *group, host *hst) {
	hostsmember *temp_hostsmember = NULL;

	if(group == NULL || hst == NULL)
		return FALSE;

	for(temp_hostsmember = group->members; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {
		if(temp_hostsmember->host_ptr == hst)
			return TRUE;
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
		if(temp_servicesmember->service_ptr != NULL && temp_servicesmember->service_ptr->host_ptr == hst)
			return TRUE;
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
		if(temp_servicesmember->service_ptr == svc)
			return TRUE;
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

	if(!group || !cntct)
		return FALSE;

	/* search all contacts in this contact group */
	for(member = group->members; member; member = member->next) {
		if (member->contact_ptr == cntct)
			return TRUE;
		}

	return FALSE;
	}

/*  tests whether a contact is a contact for a particular host */
int is_contact_for_host(host *hst, contact *cntct) {
	contactsmember *temp_contactsmember = NULL;
	contactgroupsmember *temp_contactgroupsmember = NULL;
	contactgroup *temp_contactgroup = NULL;

	if(hst == NULL || cntct == NULL) {
		return FALSE;
		}

	/* search all individual contacts of this host */
	for(temp_contactsmember = hst->contacts; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
		if (temp_contactsmember->contact_ptr == cntct)
			return TRUE;
		}

	/* search all contactgroups of this host */
	for(temp_contactgroupsmember = hst->contact_groups; temp_contactgroupsmember != NULL; temp_contactgroupsmember = temp_contactgroupsmember->next) {
		temp_contactgroup = temp_contactgroupsmember->group_ptr;
		if(is_contact_member_of_contactgroup(temp_contactgroup, cntct))
			return TRUE;
		}

	return FALSE;
	}



/*  tests whether a contactgroup is a contactgroup for a particular host */
int is_contactgroup_for_host(host *hst, contactgroup *group) {
	contactgroupsmember *temp_contactgroupsmember = NULL;

	if(hst == NULL || group == NULL) {
		return FALSE;
		}

	/* search all contactgroups of this host */
	for(temp_contactgroupsmember = hst->contact_groups;
			temp_contactgroupsmember != NULL;
			temp_contactgroupsmember = temp_contactgroupsmember->next) {
		if(temp_contactgroupsmember->group_ptr == group) {
			return TRUE;
			}
		}

	return FALSE;
	}



/* tests whether a contact is an escalated contact for a particular host */
int is_escalated_contact_for_host(host *hst, contact *cntct) {
	hostescalation *temp_hostescalation = NULL;
	objectlist *list;

	/* search all host escalations */
	for(list = hst->escalation_list; list; list = list->next) {
		temp_hostescalation = (hostescalation *)list->object_ptr;

		if(is_contact_for_host_escalation(temp_hostescalation, cntct) == TRUE) {
			return TRUE;
			}
		}

	return FALSE;
	}



/* tests whether a contact is an contact for a particular host escalation */
int is_contact_for_host_escalation(hostescalation *escalation, contact *cntct) {
	contactsmember *temp_contactsmember = NULL;
	contactgroupsmember *temp_contactgroupsmember = NULL;
	contactgroup *temp_contactgroup = NULL;

	if(escalation == NULL || cntct == NULL) {
		return FALSE;
		}

	/* search all contacts of this host escalation */
	for(temp_contactsmember = escalation->contacts;
			temp_contactsmember != NULL;
			temp_contactsmember = temp_contactsmember->next) {
		if(temp_contactsmember->contact_ptr == cntct)
			return TRUE;
		}

	/* search all contactgroups of this host escalation */
	for(temp_contactgroupsmember = escalation->contact_groups;
			temp_contactgroupsmember != NULL;
			temp_contactgroupsmember = temp_contactgroupsmember->next) {
		temp_contactgroup = temp_contactgroupsmember->group_ptr;
		if(is_contact_member_of_contactgroup(temp_contactgroup, cntct))
			return TRUE;
		}

	return FALSE;
	}



/*  tests whether a contactgroup is a contactgroup for a particular
	host escalation */
int is_contactgroup_for_host_escalation(hostescalation *escalation,
		contactgroup *group) {
	contactgroupsmember *temp_contactgroupsmember = NULL;

	if(escalation == NULL || group == NULL) {
		return FALSE;
		}

	/* search all contactgroups of this host escalation */
	for(temp_contactgroupsmember = escalation->contact_groups;
			temp_contactgroupsmember != NULL;
			temp_contactgroupsmember = temp_contactgroupsmember->next) {
		if(temp_contactgroupsmember->group_ptr == group) {
			return TRUE;
			}
		}

	return FALSE;
	}



/*  tests whether a contact is a contact for a particular service */
int is_contact_for_service(service *svc, contact *cntct) {
	contactsmember *temp_contactsmember = NULL;
	contactgroupsmember *temp_contactgroupsmember = NULL;
	contactgroup *temp_contactgroup = NULL;

	if(svc == NULL || cntct == NULL)
		return FALSE;

	/* search all individual contacts of this service */
	for(temp_contactsmember = svc->contacts; temp_contactsmember != NULL; temp_contactsmember = temp_contactsmember->next) {
		if(temp_contactsmember->contact_ptr == cntct)
			return TRUE;
		}

	/* search all contactgroups of this service */
	for(temp_contactgroupsmember = svc->contact_groups; temp_contactgroupsmember != NULL; temp_contactgroupsmember = temp_contactgroupsmember->next) {
		temp_contactgroup = temp_contactgroupsmember->group_ptr;
		if(is_contact_member_of_contactgroup(temp_contactgroup, cntct))
			return TRUE;

		}

	return FALSE;
	}



/*  tests whether a contactgroup is a contactgroup for a particular service */
int is_contactgroup_for_service(service *svc, contactgroup *group) {
	contactgroupsmember *temp_contactgroupsmember = NULL;

	if(svc == NULL || group == NULL)
		return FALSE;

	/* search all contactgroups of this service */
	for(temp_contactgroupsmember = svc->contact_groups;
			temp_contactgroupsmember != NULL;
			temp_contactgroupsmember = temp_contactgroupsmember->next) {
		if(temp_contactgroupsmember->group_ptr == group) {
			return TRUE;
			}
		}

	return FALSE;
	}



/* tests whether a contact is an escalated contact for a particular service */
int is_escalated_contact_for_service(service *svc, contact *cntct) {
	serviceescalation *temp_serviceescalation = NULL;
	objectlist *list;

	/* search all the service escalations */
	for(list = svc->escalation_list; list; list = list->next) {
		temp_serviceescalation = (serviceescalation *)list->object_ptr;

		if(is_contact_for_service_escalation(temp_serviceescalation,
				cntct) == TRUE) {
			return TRUE;
			}
		}

	return FALSE;
	}



/* tests whether a contact is an contact for a particular service escalation */
int is_contact_for_service_escalation(serviceescalation *escalation,
		contact *cntct) {
	contactsmember *temp_contactsmember = NULL;
	contactgroupsmember *temp_contactgroupsmember = NULL;
	contactgroup *temp_contactgroup = NULL;

	/* search all contacts of this service escalation */
	for(temp_contactsmember = escalation->contacts;
			temp_contactsmember != NULL;
			temp_contactsmember = temp_contactsmember->next) {
		if(temp_contactsmember->contact_ptr == cntct)
			return TRUE;
		}

	/* search all contactgroups of this service escalation */
	for(temp_contactgroupsmember =escalation->contact_groups;
			temp_contactgroupsmember != NULL;
			temp_contactgroupsmember = temp_contactgroupsmember->next) {
		temp_contactgroup = temp_contactgroupsmember->group_ptr;
		if(is_contact_member_of_contactgroup(temp_contactgroup, cntct))
			return TRUE;
		}

	return FALSE;
	}



/*  tests whether a contactgroup is a contactgroup for a particular
	service escalation */
int is_contactgroup_for_service_escalation(serviceescalation *escalation,
		contactgroup *group) {
	contactgroupsmember *temp_contactgroupsmember = NULL;

	if(escalation == NULL || group == NULL) {
		return FALSE;
		}

	/* search all contactgroups of this service escalation */
	for(temp_contactgroupsmember = escalation->contact_groups;
			temp_contactgroupsmember != NULL;
			temp_contactgroupsmember = temp_contactgroupsmember->next) {
		if(temp_contactgroupsmember->group_ptr == group) {
			return TRUE;
			}
		}

	return FALSE;
	}



/******************************************************************/
/******************* OBJECT DELETION FUNCTIONS ********************/
/******************************************************************/


/* free all allocated memory for objects */
int free_object_data(void) {
	daterange *this_daterange = NULL;
	daterange *next_daterange = NULL;
	timerange *this_timerange = NULL;
	timerange *next_timerange = NULL;
	timeperiodexclusion *this_timeperiodexclusion = NULL;
	timeperiodexclusion *next_timeperiodexclusion = NULL;
	hostsmember *this_hostsmember = NULL;
	hostsmember *next_hostsmember = NULL;
	servicesmember *this_servicesmember = NULL;
	servicesmember *next_servicesmember = NULL;
	contactsmember *this_contactsmember = NULL;
	contactsmember *next_contactsmember = NULL;
	contactgroupsmember *this_contactgroupsmember = NULL;
	contactgroupsmember *next_contactgroupsmember = NULL;
	customvariablesmember *this_customvariablesmember = NULL;
	customvariablesmember *next_customvariablesmember = NULL;
	commandsmember *this_commandsmember = NULL;
	commandsmember *next_commandsmember = NULL;
	unsigned int i = 0, x = 0;


	/*
	 * kill off hash tables so lingering modules don't look stuff up
	 * while we're busy removing it.
	 */
	for (i = 0; i < ARRAY_SIZE(object_hash_tables); i++) {
		dkhash_table *t = object_hash_tables[i];
		object_hash_tables[i] = NULL;
		dkhash_destroy(t);
	}

	/**** free memory for the timeperiod list ****/
	for (i = 0; i < num_objects.timeperiods; i++) {
		timeperiod *this_timeperiod = timeperiod_ary[i];

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

		if (this_timeperiod->alias != this_timeperiod->name)
			my_free(this_timeperiod->alias);
		my_free(this_timeperiod->name);
		my_free(this_timeperiod);
		}

	/* reset pointers */
	my_free(timeperiod_ary);


	/**** free memory for the host list ****/
	for (i = 0; i < num_objects.hosts; i++) {
		host *this_host = host_ary[i];

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
			my_free(this_hostsmember);
			this_hostsmember = next_hostsmember;
			}

		/* free memory for service links */
		this_servicesmember = this_host->services;
		while(this_servicesmember != NULL) {
			next_servicesmember = this_servicesmember->next;
			my_free(this_servicesmember);
			this_servicesmember = next_servicesmember;
			}

		/* free memory for contact groups */
		this_contactgroupsmember = this_host->contact_groups;
		while(this_contactgroupsmember != NULL) {
			next_contactgroupsmember = this_contactgroupsmember->next;
			my_free(this_contactgroupsmember);
			this_contactgroupsmember = next_contactgroupsmember;
			}

		/* free memory for contacts */
		this_contactsmember = this_host->contacts;
		while(this_contactsmember != NULL) {
			next_contactsmember = this_contactsmember->next;
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

		if(this_host->display_name != this_host->name)
			my_free(this_host->display_name);
		if(this_host->alias != this_host->name)
			my_free(this_host->alias);
		if(this_host->address != this_host->name)
			my_free(this_host->address);
		my_free(this_host->name);
#ifdef NSCORE
		my_free(this_host->plugin_output);
		my_free(this_host->long_plugin_output);
		my_free(this_host->perf_data);
#endif
		free_objectlist(&this_host->hostgroups_ptr);
		free_objectlist(&this_host->notify_deps);
		free_objectlist(&this_host->exec_deps);
		free_objectlist(&this_host->escalation_list);
		my_free(this_host->check_command);
		my_free(this_host->event_handler);
		my_free(this_host->notes);
		my_free(this_host->notes_url);
		my_free(this_host->action_url);
		my_free(this_host->icon_image);
		my_free(this_host->icon_image_alt);
		my_free(this_host->vrml_image);
		my_free(this_host->statusmap_image);
		my_free(this_host);
		}

	/* reset pointers */
	my_free(host_ary);


	/**** free memory for the host group list ****/
	for (i = 0; i < num_objects.hostgroups; i++) {
		hostgroup *this_hostgroup = hostgroup_ary[i];

		/* free memory for the group members */
		this_hostsmember = this_hostgroup->members;
		while(this_hostsmember != NULL) {
			next_hostsmember = this_hostsmember->next;
			my_free(this_hostsmember);
			this_hostsmember = next_hostsmember;
			}

		if (this_hostgroup->alias != this_hostgroup->group_name)
			my_free(this_hostgroup->alias);
		my_free(this_hostgroup->group_name);
		my_free(this_hostgroup->notes);
		my_free(this_hostgroup->notes_url);
		my_free(this_hostgroup->action_url);
		my_free(this_hostgroup);
		}

	/* reset pointers */
	my_free(hostgroup_ary);

	/**** free memory for the service group list ****/
	for (i = 0; i < num_objects.servicegroups; i++) {
		servicegroup *this_servicegroup = servicegroup_ary[i];

		/* free memory for the group members */
		this_servicesmember = this_servicegroup->members;
		while(this_servicesmember != NULL) {
			next_servicesmember = this_servicesmember->next;
			my_free(this_servicesmember);
			this_servicesmember = next_servicesmember;
			}

		if (this_servicegroup->alias != this_servicegroup->group_name)
			my_free(this_servicegroup->alias);
		my_free(this_servicegroup->group_name);
		my_free(this_servicegroup->notes);
		my_free(this_servicegroup->notes_url);
		my_free(this_servicegroup->action_url);
		my_free(this_servicegroup);
		}

	/* reset pointers */
	my_free(servicegroup_ary);

	/**** free memory for the contact list ****/
	for (i = 0; i < num_objects.contacts; i++) {
		int j;
		contact *this_contact = contact_ary[i];

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

		if (this_contact->alias != this_contact->name)
			my_free(this_contact->alias);
		my_free(this_contact->name);
		my_free(this_contact->email);
		my_free(this_contact->pager);
		for(j = 0; j < MAX_CONTACT_ADDRESSES; j++)
			my_free(this_contact->address[j]);

		free_objectlist(&this_contact->contactgroups_ptr);
		my_free(this_contact);
		}

	/* reset pointers */
	my_free(contact_ary);


	/**** free memory for the contact group list ****/
	for (i = 0; i < num_objects.contactgroups; i++) {
		contactgroup *this_contactgroup = contactgroup_ary[i];

		/* free memory for the group members */
		this_contactsmember = this_contactgroup->members;
		while(this_contactsmember != NULL) {
			next_contactsmember = this_contactsmember->next;
			my_free(this_contactsmember);
			this_contactsmember = next_contactsmember;
			}

		if (this_contactgroup->alias != this_contactgroup->group_name)
			my_free(this_contactgroup->alias);
		my_free(this_contactgroup->group_name);
		my_free(this_contactgroup);
		}

	/* reset pointers */
	my_free(contactgroup_ary);


	/**** free memory for the service list ****/
	for (i = 0; i < num_objects.services; i++) {
		service *this_service = service_ary[i];

		/* free memory for contact groups */
		this_contactgroupsmember = this_service->contact_groups;
		while(this_contactgroupsmember != NULL) {
			next_contactgroupsmember = this_contactgroupsmember->next;
			my_free(this_contactgroupsmember);
			this_contactgroupsmember = next_contactgroupsmember;
			}

		/* free memory for contacts */
		this_contactsmember = this_service->contacts;
		while(this_contactsmember != NULL) {
			next_contactsmember = this_contactsmember->next;
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

		if(this_service->display_name != this_service->description)
			my_free(this_service->display_name);
		my_free(this_service->description);
		my_free(this_service->check_command);
#ifdef NSCORE
		my_free(this_service->plugin_output);
		my_free(this_service->long_plugin_output);
		my_free(this_service->perf_data);

		my_free(this_service->event_handler_args);
		my_free(this_service->check_command_args);
#endif

		free_objectlist(&this_service->servicegroups_ptr);
		free_objectlist(&this_service->notify_deps);
		free_objectlist(&this_service->exec_deps);
		free_objectlist(&this_service->escalation_list);
		my_free(this_service->event_handler);
		my_free(this_service->notes);
		my_free(this_service->notes_url);
		my_free(this_service->action_url);
		my_free(this_service->icon_image);
		my_free(this_service->icon_image_alt);
		my_free(this_service);
		}

	/* reset pointers */
	my_free(service_ary);


	/**** free command memory ****/
	for (i = 0; i < num_objects.commands; i++) {
		command *this_command = command_ary[i];
		my_free(this_command->name);
		my_free(this_command->command_line);
		my_free(this_command);
		}

	/* reset pointers */
	my_free(command_ary);


	/**** free service escalation memory ****/
	for (i = 0; i < num_objects.serviceescalations; i++) {
		serviceescalation *this_serviceescalation = serviceescalation_ary[i];

		/* free memory for the contact group members */
		this_contactgroupsmember = this_serviceescalation->contact_groups;
		while(this_contactgroupsmember != NULL) {
			next_contactgroupsmember = this_contactgroupsmember->next;
			my_free(this_contactgroupsmember);
			this_contactgroupsmember = next_contactgroupsmember;
			}

		/* free memory for contacts */
		this_contactsmember = this_serviceescalation->contacts;
		while(this_contactsmember != NULL) {
			next_contactsmember = this_contactsmember->next;
			my_free(this_contactsmember);
			this_contactsmember = next_contactsmember;
			}
		my_free(this_serviceescalation);
		}

	/* reset pointers */
	my_free(serviceescalation_ary);


	/**** free service dependency memory ****/
	if (servicedependency_ary) {
		for(i = 0; i < num_objects.servicedependencies; i++)
			my_free(servicedependency_ary[i]);
		my_free(servicedependency_ary);
		}


	/**** free host dependency memory ****/
	if (hostdependency_ary) {
		for(i = 0; i < num_objects.hostdependencies; i++)
			my_free(hostdependency_ary[i]);
		my_free(hostdependency_ary);
		}


	/**** free host escalation memory ****/
	for (i = 0; i < num_objects.hostescalations; i++) {
		hostescalation *this_hostescalation = hostescalation_ary[i];

		/* free memory for the contact group members */
		this_contactgroupsmember = this_hostescalation->contact_groups;
		while(this_contactgroupsmember != NULL) {
			next_contactgroupsmember = this_contactgroupsmember->next;
			my_free(this_contactgroupsmember);
			this_contactgroupsmember = next_contactgroupsmember;
			}

		/* free memory for contacts */
		this_contactsmember = this_hostescalation->contacts;
		while(this_contactsmember != NULL) {
			next_contactsmember = this_contactsmember->next;
			my_free(this_contactsmember);
			this_contactsmember = next_contactsmember;
			}
		my_free(this_hostescalation);
		}

	/* reset pointers */
	my_free(hostescalation_ary);

	/* we no longer have any objects */
	memset(&num_objects, 0, sizeof(num_objects));

	return OK;
	}



/******************************************************************/
/*********************** CACHE FUNCTIONS **************************/
/******************************************************************/

#ifndef NSCGI
static const char *timerange2str(const timerange *tr)
{
	static char str[12];
	int sh, sm, eh, em;

	if(!tr)
		return "";
	sh = tr->range_start / 3600;
	sm = (tr->range_start / 60) % 60;
	eh = tr->range_end / 3600;
	em = (tr->range_end / 60) % 60;
	sprintf(str, "%02d:%02d-%02d:%02d", sh, sm, eh, em);
	return str;
}

void fcache_contactlist(FILE *fp, const char *prefix, contactsmember *list)
{
	if(list) {
		contactsmember *l;
		fprintf(fp, "%s", prefix);
		for(l = list; l; l = l->next)
			fprintf(fp, "%s%c", l->contact_name, l->next ? ',' : '\n');
	}
}

void fcache_contactgrouplist(FILE *fp, const char *prefix, contactgroupsmember *list)
{
	if(list) {
		contactgroupsmember *l;
		fprintf(fp, "%s", prefix);
		for(l = list; l; l = l->next)
			fprintf(fp, "%s%c", l->group_name, l->next ? ',' : '\n');
	}
}

void fcache_hostlist(FILE *fp, const char *prefix, hostsmember *list)
{
	if(list) {
		hostsmember *l;
		fprintf(fp, "%s", prefix);
		for(l = list; l; l = l->next)
			fprintf(fp, "%s%c", l->host_name, l->next ? ',' : '\n');
	}
}

void fcache_customvars(FILE *fp, customvariablesmember *cvlist)
{
	if(cvlist) {
		customvariablesmember *l;
		for(l = cvlist; l; l = l->next)
			fprintf(fp, "\t_%s\t%s\n", l->variable_name, (l->variable_value == NULL) ? XODTEMPLATE_NULL : l->variable_value);
	}
}

void fcache_timeperiod(FILE *fp, timeperiod *temp_timeperiod)
{
	const char *days[7] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
	const char *months[12] = {"january", "february", "march", "april", "may", "june", "july", "august", "september", "october", "november", "december"};
	daterange *temp_daterange;
	timerange *tr;
	register int x;

	fprintf(fp, "define timeperiod {\n");
	fprintf(fp, "\ttimeperiod_name\t%s\n", temp_timeperiod->name);
	if(temp_timeperiod->alias)
		fprintf(fp, "\talias\t%s\n", temp_timeperiod->alias);

	if(temp_timeperiod->exclusions) {
		timeperiodexclusion *exclude;
		fprintf(fp, "\texclude\t");
		for(exclude = temp_timeperiod->exclusions; exclude; exclude = exclude->next) {
			fprintf(fp, "%s%c", exclude->timeperiod_name, exclude->next ? ',' : '\n');
		}
	}

	for(x = 0; x < DATERANGE_TYPES; x++) {
		for(temp_daterange = temp_timeperiod->exceptions[x]; temp_daterange != NULL; temp_daterange = temp_daterange->next) {

			/* skip null entries */
			if(temp_daterange->times == NULL)
				continue;

			switch(temp_daterange->type) {
				case DATERANGE_CALENDAR_DATE:
				fprintf(fp, "\t%d-%02d-%02d", temp_daterange->syear, temp_daterange->smon + 1, temp_daterange->smday);
				if((temp_daterange->smday != temp_daterange->emday) || (temp_daterange->smon != temp_daterange->emon) || (temp_daterange->syear != temp_daterange->eyear))
					fprintf(fp, " - %d-%02d-%02d", temp_daterange->eyear, temp_daterange->emon + 1, temp_daterange->emday);
				if(temp_daterange->skip_interval > 1)
					fprintf(fp, " / %d", temp_daterange->skip_interval);
				break;
			case DATERANGE_MONTH_DATE:
				fprintf(fp, "\t%s %d", months[temp_daterange->smon], temp_daterange->smday);
				if((temp_daterange->smon != temp_daterange->emon) || (temp_daterange->smday != temp_daterange->emday)) {
					fprintf(fp, " - %s %d", months[temp_daterange->emon], temp_daterange->emday);
					if(temp_daterange->skip_interval > 1)
						fprintf(fp, " / %d", temp_daterange->skip_interval);
				}
				break;
			case DATERANGE_MONTH_DAY:
				fprintf(fp, "\tday %d", temp_daterange->smday);
				if(temp_daterange->smday != temp_daterange->emday) {
					fprintf(fp, " - %d", temp_daterange->emday);
					if(temp_daterange->skip_interval > 1)
						fprintf(fp, " / %d", temp_daterange->skip_interval);
				}
				break;
			case DATERANGE_MONTH_WEEK_DAY:
				fprintf(fp, "\t%s %d %s", days[temp_daterange->swday], temp_daterange->swday_offset, months[temp_daterange->smon]);
				if((temp_daterange->smon != temp_daterange->emon) || (temp_daterange->swday != temp_daterange->ewday) || (temp_daterange->swday_offset != temp_daterange->ewday_offset)) {
					fprintf(fp, " - %s %d %s", days[temp_daterange->ewday], temp_daterange->ewday_offset, months[temp_daterange->emon]);
					if(temp_daterange->skip_interval > 1)
						fprintf(fp, " / %d", temp_daterange->skip_interval);
				}
				break;
			case DATERANGE_WEEK_DAY:
				fprintf(fp, "\t%s %d", days[temp_daterange->swday], temp_daterange->swday_offset);
				if((temp_daterange->swday != temp_daterange->ewday) || (temp_daterange->swday_offset != temp_daterange->ewday_offset)) {
					fprintf(fp, " - %s %d", days[temp_daterange->ewday], temp_daterange->ewday_offset);
					if(temp_daterange->skip_interval > 1)
						fprintf(fp, " / %d", temp_daterange->skip_interval);
				}
				break;
			default:
				break;
			}

			fputc('\t', fp);
			for(tr = temp_daterange->times; tr; tr = tr->next) {
				fprintf(fp, "%s%c", timerange2str(tr), tr->next ? ',' : '\n');
			}
		}
	}
	for(x = 0; x < 7; x++) {
		/* skip null entries */
		if(temp_timeperiod->days[x] == NULL)
			continue;

		fprintf(fp, "\t%s\t", days[x]);
		for(tr = temp_timeperiod->days[x]; tr; tr = tr->next) {
			fprintf(fp, "%s%c", timerange2str(tr), tr->next ? ',' : '\n');
		}
	}
	fprintf(fp, "\t}\n\n");
}

void fcache_command(FILE *fp, command *temp_command)
{
	fprintf(fp, "define command {\n\tcommand_name\t%s\n\tcommand_line\t%s\n\t}\n\n",
		   temp_command->name, temp_command->command_line);
}

void fcache_contactgroup(FILE *fp, contactgroup *temp_contactgroup)
{
	fprintf(fp, "define contactgroup {\n");
	fprintf(fp, "\tcontactgroup_name\t%s\n", temp_contactgroup->group_name);
	if(temp_contactgroup->alias)
		fprintf(fp, "\talias\t%s\n", temp_contactgroup->alias);
	fcache_contactlist(fp, "\tmembers\t", temp_contactgroup->members);
	fprintf(fp, "\t}\n\n");
}

void fcache_hostgroup(FILE *fp, hostgroup *temp_hostgroup)
{
	fprintf(fp, "define hostgroup {\n");
	fprintf(fp, "\thostgroup_name\t%s\n", temp_hostgroup->group_name);
	if(temp_hostgroup->alias)
		fprintf(fp, "\talias\t%s\n", temp_hostgroup->alias);
	if(temp_hostgroup->members) {
		hostsmember *list;
		fprintf(fp, "\tmembers\t");
		for(list = temp_hostgroup->members; list; list = list->next)
			fprintf(fp, "%s%c", list->host_name, list->next ? ',' : '\n');
	}
	if(temp_hostgroup->notes)
		fprintf(fp, "\tnotes\t%s\n", temp_hostgroup->notes);
	if(temp_hostgroup->notes_url)
		fprintf(fp, "\tnotes_url\t%s\n", temp_hostgroup->notes_url);
	if(temp_hostgroup->action_url)
		fprintf(fp, "\taction_url\t%s\n", temp_hostgroup->action_url);
	fprintf(fp, "\t}\n\n");
}

void fcache_servicegroup(FILE *fp, servicegroup *temp_servicegroup)
{
	fprintf(fp, "define servicegroup {\n");
	fprintf(fp, "\tservicegroup_name\t%s\n", temp_servicegroup->group_name);
	if(temp_servicegroup->alias)
		fprintf(fp, "\talias\t%s\n", temp_servicegroup->alias);
	if(temp_servicegroup->members) {
		servicesmember *list;
		fprintf(fp, "\tmembers\t");
		for(list = temp_servicegroup->members; list; list = list->next) {
			service *s = list->service_ptr;
			fprintf(fp, "%s,%s%c", s->host_name, s->description, list->next ? ',' : '\n');
		}
	}
	if(temp_servicegroup->notes)
		fprintf(fp, "\tnotes\t%s\n", temp_servicegroup->notes);
	if(temp_servicegroup->notes_url)
		fprintf(fp, "\tnotes_url\t%s\n", temp_servicegroup->notes_url);
	if(temp_servicegroup->action_url)
		fprintf(fp, "\taction_url\t%s\n", temp_servicegroup->action_url);
	fprintf(fp, "\t}\n\n");
}

void fcache_contact(FILE *fp, contact *temp_contact)
{
	commandsmember *list;
	int x;

	fprintf(fp, "define contact {\n");
	fprintf(fp, "\tcontact_name\t%s\n", temp_contact->name);
	if(temp_contact->alias)
		fprintf(fp, "\talias\t%s\n", temp_contact->alias);
	if(temp_contact->service_notification_period)
		fprintf(fp, "\tservice_notification_period\t%s\n", temp_contact->service_notification_period);
	if(temp_contact->host_notification_period)
		fprintf(fp, "\thost_notification_period\t%s\n", temp_contact->host_notification_period);
	fprintf(fp, "\tservice_notification_options\t%s\n", opts2str(temp_contact->service_notification_options, service_flag_map, 'r'));
	fprintf(fp, "\thost_notification_options\t%s\n", opts2str(temp_contact->host_notification_options, host_flag_map, 'r'));
	if(temp_contact->service_notification_commands) {
		fprintf(fp, "\tservice_notification_commands\t");
		for(list = temp_contact->service_notification_commands; list; list = list->next) {
			fprintf(fp, "%s%c", list->command, list->next ? ',' : '\n');
		}
	}
	if(temp_contact->host_notification_commands) {
		fprintf(fp, "\thost_notification_commands\t");
		for(list = temp_contact->host_notification_commands; list; list = list->next) {
			fprintf(fp, "%s%c", list->command, list->next ? ',' : '\n');
		}
	}
	if(temp_contact->email)
		fprintf(fp, "\temail\t%s\n", temp_contact->email);
	if(temp_contact->pager)
		fprintf(fp, "\tpager\t%s\n", temp_contact->pager);
	for(x = 0; x < MAX_XODTEMPLATE_CONTACT_ADDRESSES; x++) {
		if(temp_contact->address[x])
			fprintf(fp, "\taddress%d\t%s\n", x + 1, temp_contact->address[x]);
	}
	fprintf(fp, "\tminimum_importance\t%u\n", temp_contact->minimum_value);
	fprintf(fp, "\thost_notifications_enabled\t%d\n", temp_contact->host_notifications_enabled);
	fprintf(fp, "\tservice_notifications_enabled\t%d\n", temp_contact->service_notifications_enabled);
	fprintf(fp, "\tcan_submit_commands\t%d\n", temp_contact->can_submit_commands);
	fprintf(fp, "\tretain_status_information\t%d\n", temp_contact->retain_status_information);
	fprintf(fp, "\tretain_nonstatus_information\t%d\n", temp_contact->retain_nonstatus_information);

	/* custom variables */
	fcache_customvars(fp, temp_contact->custom_variables);
	fprintf(fp, "\t}\n\n");
}

void fcache_host(FILE *fp, host *temp_host)
{
	fprintf(fp, "define host {\n");
	fprintf(fp, "\thost_name\t%s\n", temp_host->name);
	if(temp_host->display_name != temp_host->name)
		fprintf(fp, "\tdisplay_name\t%s\n", temp_host->display_name);
	if(temp_host->alias)
		fprintf(fp, "\talias\t%s\n", temp_host->alias);
	if(temp_host->address)
		fprintf(fp, "\taddress\t%s\n", temp_host->address);
	fcache_hostlist(fp, "\tparents\t", temp_host->parent_hosts);
	if(temp_host->check_period)
		fprintf(fp, "\tcheck_period\t%s\n", temp_host->check_period);
	if(temp_host->check_command)
		fprintf(fp, "\tcheck_command\t%s\n", temp_host->check_command);
	if(temp_host->event_handler)
		fprintf(fp, "\tevent_handler\t%s\n", temp_host->event_handler);
	fcache_contactlist(fp, "\tcontacts\t", temp_host->contacts);
	fcache_contactgrouplist(fp, "\tcontact_groups\t", temp_host->contact_groups);
	if(temp_host->notification_period)
		fprintf(fp, "\tnotification_period\t%s\n", temp_host->notification_period);
	fprintf(fp, "\tinitial_state\t");
	if(temp_host->initial_state == HOST_DOWN)
		fprintf(fp, "d\n");
	else if(temp_host->initial_state == HOST_UNREACHABLE)
		fprintf(fp, "u\n");
	else
		fprintf(fp, "o\n");
	fprintf(fp, "\timportance\t%u\n", temp_host->hourly_value);
	fprintf(fp, "\tcheck_interval\t%f\n", temp_host->check_interval);
	fprintf(fp, "\tretry_interval\t%f\n", temp_host->retry_interval);
	fprintf(fp, "\tmax_check_attempts\t%d\n", temp_host->max_attempts);
	fprintf(fp, "\tactive_checks_enabled\t%d\n", temp_host->checks_enabled);
	fprintf(fp, "\tpassive_checks_enabled\t%d\n", temp_host->accept_passive_checks);
	fprintf(fp, "\tobsess\t%d\n", temp_host->obsess);
	fprintf(fp, "\tevent_handler_enabled\t%d\n", temp_host->event_handler_enabled);
	fprintf(fp, "\tlow_flap_threshold\t%f\n", temp_host->low_flap_threshold);
	fprintf(fp, "\thigh_flap_threshold\t%f\n", temp_host->high_flap_threshold);
	fprintf(fp, "\tflap_detection_enabled\t%d\n", temp_host->flap_detection_enabled);
	fprintf(fp, "\tflap_detection_options\t%s\n", opts2str(temp_host->flap_detection_options, host_flag_map, 'u'));
	fprintf(fp, "\tfreshness_threshold\t%d\n", temp_host->freshness_threshold);
	fprintf(fp, "\tcheck_freshness\t%d\n", temp_host->check_freshness);
	fprintf(fp, "\tnotification_options\t%s\n", opts2str(temp_host->notification_options, host_flag_map, 'r'));
	fprintf(fp, "\tnotifications_enabled\t%d\n", temp_host->notifications_enabled);
	fprintf(fp, "\tnotification_interval\t%f\n", temp_host->notification_interval);
	fprintf(fp, "\tfirst_notification_delay\t%f\n", temp_host->first_notification_delay);
	fprintf(fp, "\tstalking_options\t%s\n", opts2str(temp_host->stalking_options, host_flag_map, 'u'));
	fprintf(fp, "\tprocess_perf_data\t%d\n", temp_host->process_performance_data);
	if(temp_host->icon_image)
		fprintf(fp, "\ticon_image\t%s\n", temp_host->icon_image);
	if(temp_host->icon_image_alt)
		fprintf(fp, "\ticon_image_alt\t%s\n", temp_host->icon_image_alt);
	if(temp_host->vrml_image)
		fprintf(fp, "\tvrml_image\t%s\n", temp_host->vrml_image);
	if(temp_host->statusmap_image)
		fprintf(fp, "\tstatusmap_image\t%s\n", temp_host->statusmap_image);
	if(temp_host->have_2d_coords == TRUE)
		fprintf(fp, "\t2d_coords\t%d,%d\n", temp_host->x_2d, temp_host->y_2d);
	if(temp_host->have_3d_coords == TRUE)
		fprintf(fp, "\t3d_coords\t%f,%f,%f\n", temp_host->x_3d, temp_host->y_3d, temp_host->z_3d);
	if(temp_host->notes)
		fprintf(fp, "\tnotes\t%s\n", temp_host->notes);
	if(temp_host->notes_url)
		fprintf(fp, "\tnotes_url\t%s\n", temp_host->notes_url);
	if(temp_host->action_url)
		fprintf(fp, "\taction_url\t%s\n", temp_host->action_url);
	fprintf(fp, "\tretain_status_information\t%d\n", temp_host->retain_status_information);
	fprintf(fp, "\tretain_nonstatus_information\t%d\n", temp_host->retain_nonstatus_information);

	/* custom variables */
	fcache_customvars(fp, temp_host->custom_variables);
	fprintf(fp, "\t}\n\n");
}

void fcache_service(FILE *fp, service *temp_service)
{
	fprintf(fp, "define service {\n");
	fprintf(fp, "\thost_name\t%s\n", temp_service->host_name);
	fprintf(fp, "\tservice_description\t%s\n", temp_service->description);
	if(temp_service->display_name != temp_service->description)
		fprintf(fp, "\tdisplay_name\t%s\n", temp_service->display_name);
	if(temp_service->parents) {
		fprintf(fp, "\tparents\t");
		/* same-host, single-parent? */
		if(!temp_service->parents->next && temp_service->parents->service_ptr->host_ptr == temp_service->host_ptr)
			fprintf(fp, "%s\n", temp_service->parents->service_ptr->description);
		else {
			servicesmember *sm;
			for(sm = temp_service->parents; sm; sm = sm->next) {
				fprintf(fp, "%s,%s%c", sm->host_name, sm->service_description, sm->next ? ',' : '\n');
			}
		}
	}
	if(temp_service->check_period)
		fprintf(fp, "\tcheck_period\t%s\n", temp_service->check_period);
	if(temp_service->check_command)
		fprintf(fp, "\tcheck_command\t%s\n", temp_service->check_command);
	if(temp_service->event_handler)
		fprintf(fp, "\tevent_handler\t%s\n", temp_service->event_handler);
	fcache_contactlist(fp, "\tcontacts\t", temp_service->contacts);
	fcache_contactgrouplist(fp, "\tcontact_groups\t", temp_service->contact_groups);
	if(temp_service->notification_period)
		fprintf(fp, "\tnotification_period\t%s\n", temp_service->notification_period);
	fprintf(fp, "\tinitial_state\t");
	if(temp_service->initial_state == STATE_WARNING)
		fprintf(fp, "w\n");
	else if(temp_service->initial_state == STATE_UNKNOWN)
		fprintf(fp, "u\n");
	else if(temp_service->initial_state == STATE_CRITICAL)
		fprintf(fp, "c\n");
	else
		fprintf(fp, "o\n");
	fprintf(fp, "\timportance\t%u\n", temp_service->hourly_value);
	fprintf(fp, "\tcheck_interval\t%f\n", temp_service->check_interval);
	fprintf(fp, "\tretry_interval\t%f\n", temp_service->retry_interval);
	fprintf(fp, "\tmax_check_attempts\t%d\n", temp_service->max_attempts);
	fprintf(fp, "\tis_volatile\t%d\n", temp_service->is_volatile);
	fprintf(fp, "\tparallelize_check\t%d\n", temp_service->parallelize);
	fprintf(fp, "\tactive_checks_enabled\t%d\n", temp_service->checks_enabled);
	fprintf(fp, "\tpassive_checks_enabled\t%d\n", temp_service->accept_passive_checks);
	fprintf(fp, "\tobsess\t%d\n", temp_service->obsess);
	fprintf(fp, "\tevent_handler_enabled\t%d\n", temp_service->event_handler_enabled);
	fprintf(fp, "\tlow_flap_threshold\t%f\n", temp_service->low_flap_threshold);
	fprintf(fp, "\thigh_flap_threshold\t%f\n", temp_service->high_flap_threshold);
	fprintf(fp, "\tflap_detection_enabled\t%d\n", temp_service->flap_detection_enabled);
	fprintf(fp, "\tflap_detection_options\t%s\n", opts2str(temp_service->flap_detection_options, service_flag_map, 'o'));
	fprintf(fp, "\tfreshness_threshold\t%d\n", temp_service->freshness_threshold);
	fprintf(fp, "\tcheck_freshness\t%d\n", temp_service->check_freshness);
	fprintf(fp, "\tnotification_options\t%s\n", opts2str(temp_service->notification_options, service_flag_map, 'r'));
	fprintf(fp, "\tnotifications_enabled\t%d\n", temp_service->notifications_enabled);
	fprintf(fp, "\tnotification_interval\t%f\n", temp_service->notification_interval);
	fprintf(fp, "\tfirst_notification_delay\t%f\n", temp_service->first_notification_delay);
	fprintf(fp, "\tstalking_options\t%s\n", opts2str(temp_service->stalking_options, service_flag_map, 'o'));
	fprintf(fp, "\tprocess_perf_data\t%d\n", temp_service->process_performance_data);
	if(temp_service->icon_image)
		fprintf(fp, "\ticon_image\t%s\n", temp_service->icon_image);
	if(temp_service->icon_image_alt)
		fprintf(fp, "\ticon_image_alt\t%s\n", temp_service->icon_image_alt);
	if(temp_service->notes)
		fprintf(fp, "\tnotes\t%s\n", temp_service->notes);
	if(temp_service->notes_url)
		fprintf(fp, "\tnotes_url\t%s\n", temp_service->notes_url);
	if(temp_service->action_url)
		fprintf(fp, "\taction_url\t%s\n", temp_service->action_url);
	fprintf(fp, "\tretain_status_information\t%d\n", temp_service->retain_status_information);
	fprintf(fp, "\tretain_nonstatus_information\t%d\n", temp_service->retain_nonstatus_information);

	/* custom variables */
	fcache_customvars(fp, temp_service->custom_variables);
	fprintf(fp, "\t}\n\n");
}

void fcache_servicedependency(FILE *fp, servicedependency *temp_servicedependency)
{
	fprintf(fp, "define servicedependency {\n");
	fprintf(fp, "\thost_name\t%s\n", temp_servicedependency->host_name);
	fprintf(fp, "\tservice_description\t%s\n", temp_servicedependency->service_description);
	fprintf(fp, "\tdependent_host_name\t%s\n", temp_servicedependency->dependent_host_name);
	fprintf(fp, "\tdependent_service_description\t%s\n", temp_servicedependency->dependent_service_description);
	if(temp_servicedependency->dependency_period)
		fprintf(fp, "\tdependency_period\t%s\n", temp_servicedependency->dependency_period);
	fprintf(fp, "\tinherits_parent\t%d\n", temp_servicedependency->inherits_parent);
	fprintf(fp, "\t%s_failure_options\t%s\n",
	        temp_servicedependency->dependency_type == NOTIFICATION_DEPENDENCY ? "notification" : "execution",
	        opts2str(temp_servicedependency->failure_options, service_flag_map, 'o'));
	fprintf(fp, "\t}\n\n");
}

void fcache_serviceescalation(FILE *fp, serviceescalation *temp_serviceescalation)
{
	fprintf(fp, "define serviceescalation {\n");
	fprintf(fp, "\thost_name\t%s\n", temp_serviceescalation->host_name);
	fprintf(fp, "\tservice_description\t%s\n", temp_serviceescalation->description);
	fprintf(fp, "\tfirst_notification\t%d\n", temp_serviceescalation->first_notification);
	fprintf(fp, "\tlast_notification\t%d\n", temp_serviceescalation->last_notification);
	fprintf(fp, "\tnotification_interval\t%f\n", temp_serviceescalation->notification_interval);
	if(temp_serviceescalation->escalation_period)
		fprintf(fp, "\tescalation_period\t%s\n", temp_serviceescalation->escalation_period);
	fprintf(fp, "\tescalation_options\t%s\n", opts2str(temp_serviceescalation->escalation_options, service_flag_map, 'r'));

	if(temp_serviceescalation->contacts) {
		contactsmember *cl;
		fprintf(fp, "\tcontacts\t");
		for(cl = temp_serviceescalation->contacts; cl; cl = cl->next)
			fprintf(fp, "%s%c", cl->contact_ptr->name, cl->next ? ',' : '\n');
	}
	if(temp_serviceescalation->contact_groups) {
		contactgroupsmember *cgl;
		fprintf(fp, "\tcontact_groups\t");
		for (cgl = temp_serviceescalation->contact_groups; cgl; cgl = cgl->next)
			fprintf(fp, "%s%c", cgl->group_name, cgl->next ? ',' : '\n');
	}
	fprintf(fp, "\t}\n\n");
}

void fcache_hostdependency(FILE *fp, hostdependency *temp_hostdependency)
{
	fprintf(fp, "define hostdependency {\n");
	fprintf(fp, "\thost_name\t%s\n", temp_hostdependency->host_name);
	fprintf(fp, "\tdependent_host_name\t%s\n", temp_hostdependency->dependent_host_name);
	if(temp_hostdependency->dependency_period)
		fprintf(fp, "\tdependency_period\t%s\n", temp_hostdependency->dependency_period);
	fprintf(fp, "\tinherits_parent\t%d\n", temp_hostdependency->inherits_parent);
	fprintf(fp, "\t%s_failure_options\t%s\n",
			temp_hostdependency->dependency_type == NOTIFICATION_DEPENDENCY ? "notification" : "execution",
			opts2str(temp_hostdependency->failure_options, host_flag_map, 'o'));
	fprintf(fp, "\t}\n\n");
}

void fcache_hostescalation(FILE *fp, hostescalation *temp_hostescalation)
{
	fprintf(fp, "define hostescalation {\n");
	fprintf(fp, "\thost_name\t%s\n", temp_hostescalation->host_name);
	fprintf(fp, "\tfirst_notification\t%d\n", temp_hostescalation->first_notification);
	fprintf(fp, "\tlast_notification\t%d\n", temp_hostescalation->last_notification);
	fprintf(fp, "\tnotification_interval\t%f\n", temp_hostescalation->notification_interval);
	if(temp_hostescalation->escalation_period)
		fprintf(fp, "\tescalation_period\t%s\n", temp_hostescalation->escalation_period);
	fprintf(fp, "\tescalation_options\t%s\n", opts2str(temp_hostescalation->escalation_options, host_flag_map, 'r'));

	fcache_contactlist(fp, "\tcontacts\t", temp_hostescalation->contacts);
	fcache_contactgrouplist(fp, "\tcontact_groups\t", temp_hostescalation->contact_groups);
	fprintf(fp, "\t}\n\n");
}

/* writes cached object definitions for use by web interface */
int fcache_objects(char *cache_file) {
	FILE *fp = NULL;
	time_t current_time = 0L;
	unsigned int i;

	/* some people won't want to cache their objects */
	if(!cache_file || !strcmp(cache_file, "/dev/null"))
		return OK;

	time(&current_time);

	/* open the cache file for writing */
	fp = fopen(cache_file, "w");
	if(fp == NULL) {
		logit(NSLOG_CONFIG_WARNING, TRUE, "Warning: Could not open object cache file '%s' for writing!\n", cache_file);
		return ERROR;
		}

	/* write header to cache file */
	fprintf(fp, "########################################\n");
	fprintf(fp, "#       NAGIOS OBJECT CACHE FILE\n");
	fprintf(fp, "#\n");
	fprintf(fp, "# THIS FILE IS AUTOMATICALLY GENERATED\n");
	fprintf(fp, "# BY NAGIOS.  DO NOT MODIFY THIS FILE!\n");
	fprintf(fp, "#\n");
	fprintf(fp, "# Created: %s", ctime(&current_time));
	fprintf(fp, "########################################\n\n");


	/* cache timeperiods */
	for(i = 0; i < num_objects.timeperiods; i++)
		fcache_timeperiod(fp, timeperiod_ary[i]);

	/* cache commands */
	for(i = 0; i < num_objects.commands; i++)
		fcache_command(fp, command_ary[i]);

	/* cache contactgroups */
	for(i = 0; i < num_objects.contactgroups; i++)
		fcache_contactgroup(fp, contactgroup_ary[i]);

	/* cache hostgroups */
	for(i = 0; i < num_objects.hostgroups; i++)
		fcache_hostgroup(fp, hostgroup_ary[i]);

	/* cache servicegroups */
	for(i = 0; i < num_objects.servicegroups; i++)
		fcache_servicegroup(fp, servicegroup_ary[i]);

	/* cache contacts */
	for(i = 0; i < num_objects.contacts; i++)
		fcache_contact(fp, contact_ary[i]);

	/* cache hosts */
	for(i = 0; i < num_objects.hosts; i++)
		fcache_host(fp, host_ary[i]);

	/* cache services */
	for(i = 0; i < num_objects.services; i++)
		fcache_service(fp, service_ary[i]);

	/* cache service dependencies */
	for(i = 0; i < num_objects.servicedependencies; i++)
		fcache_servicedependency(fp, servicedependency_ary[i]);

	/* cache service escalations */
	for(i = 0; i < num_objects.serviceescalations; i++)
		fcache_serviceescalation(fp, serviceescalation_ary[i]);

	/* cache host dependencies */
	for(i = 0; i < num_objects.hostdependencies; i++)
		fcache_hostdependency(fp, hostdependency_ary[i]);

	/* cache host escalations */
	for(i = 0; i < num_objects.hostescalations; i++)
		fcache_hostescalation(fp, hostescalation_ary[i]);

	fclose(fp);

	return OK;
	}
#endif
