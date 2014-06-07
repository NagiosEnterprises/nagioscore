/**************************************************************************
 *
 * ARCHIVEUTILS.C -  Utilities for Nagios CGIs that read archive logs
 *
 * Copyright (c) 2013 Nagios Enterprises, LLC
 * Last Modified: 06-30-2013
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
 *************************************************************************/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/statusdata.h"
#include "../include/comments.h"

#include "../include/cgiutils.h"
#include "../include/statusdata.h"
#include "../include/archiveutils.h"

#define AU_INITIAL_LIST_SIZE	16

const string_value_mapping svm_au_object_types[] = {
	{ "none", AU_OBJTYPE_NONE, "None" },
	{ "host", AU_OBJTYPE_HOST, "Host" },
	{ "service", AU_OBJTYPE_SERVICE, "Service" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_au_state_types[] = {
	{ "hard", AU_STATETYPE_HARD, "Hard" },
	{ "soft", AU_STATETYPE_SOFT, "Soft" },
	{ "nodata", AU_STATETYPE_NO_DATA, "No Data" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_au_states[] = {
	{ "nodata", AU_STATE_NO_DATA, "No Data" },
	{ "up", AU_STATE_HOST_UP, "Host Up" },
	{ "down", AU_STATE_HOST_DOWN, "How Down" },
	{ "unreachable", AU_STATE_HOST_UNREACHABLE, "Host Unreachable" },
	{ "ok", AU_STATE_SERVICE_OK, "Service OK" },
	{ "warning", AU_STATE_SERVICE_WARNING, "Service Warning" },
	{ "critical", AU_STATE_SERVICE_CRITICAL, "Service Critical" },
	{ "unknown", AU_STATE_SERVICE_UNKNOWN, "Service Unknown" },
	{ "programstart", AU_STATE_PROGRAM_START, "Program Start"},
	{ "programend", AU_STATE_PROGRAM_END, "Program End"},
	{ "downtimestart", AU_STATE_DOWNTIME_START, "Downtime Start"},
	{ "downtimeend", AU_STATE_DOWNTIME_END, "Downtime End"},
	{ "currentstate", AU_STATE_CURRENT_STATE, "Current State"},
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_au_log_types[] = {
	{ "alert", AU_LOGTYPE_ALERT, "Alert" },
	{ "initialstate", AU_LOGTYPE_STATE_INITIAL, "Initial State" },
	{ "currentstate", AU_LOGTYPE_STATE_CURRENT, "Current State" },
	{ "notification", AU_LOGTYPE_NOTIFICATION, "Notification" },
	{ "downtime", AU_LOGTYPE_DOWNTIME, "Downtime" },
	{ "nagios", AU_LOGTYPE_NAGIOS, "Nagios" },
	{ NULL, -1, NULL },
	};

const string_value_mapping svm_au_notification_types[] = {
	{ "nodata", AU_NOTIFICATION_NO_DATA, 
			"No Data" },
	{ "down", AU_NOTIFICATION_HOST_DOWN, 
			"Host Down" },
	{ "unreachable", AU_NOTIFICATION_HOST_UNREACHABLE, 
			"Host Unreachable" },
	{ "recovery", AU_NOTIFICATION_HOST_RECOVERY, 
			"Host Recovery" },
	{ "hostcustom", AU_NOTIFICATION_HOST_CUSTOM, 
			"Host Custom" },
	{ "hostack", AU_NOTIFICATION_HOST_ACK, 
			"Host Acknowlegement" },
	{ "hostflapstart", AU_NOTIFICATION_HOST_FLAPPING_START, 
			"Host Flapping Start" },
	{ "hostflapstop", AU_NOTIFICATION_HOST_FLAPPING_STOP, 
			"Host Flapping Stop" },
	{ "critical", AU_NOTIFICATION_SERVICE_CRITICAL, 
			"Service Critical" },
	{ "warning", AU_NOTIFICATION_SERVICE_WARNING, 
			"Service Warning" },
	{ "recovery", AU_NOTIFICATION_SERVICE_RECOVERY, 
			"Service Recovery" },
	{ "custom", AU_NOTIFICATION_SERVICE_CUSTOM, 
			"Service Custom" },
	{ "serviceack", AU_NOTIFICATION_SERVICE_ACK, 
			"Service Acknowlegement" },
	{ "serviceflapstart", AU_NOTIFICATION_SERVICE_FLAPPING_START, 
			"Service Flapping Start" },
	{ "serviceflapstop", AU_NOTIFICATION_SERVICE_FLAPPING_STOP, 
			"Service Flapping Stop" },
	{ "unknown", AU_NOTIFICATION_SERVICE_UNKNOWN, 
			"Service Unknown" },
	{ NULL, -1, NULL },
	};

/* Function prototypes */
int read_log_file(char *, unsigned, unsigned, unsigned, au_log *);
int au_add_nagios_log(au_log *, time_t, int, char *);
void au_free_nagios_log(au_log_nagios *);
int parse_states_and_alerts(char *, time_t, int, unsigned, unsigned, au_log *);
int parse_downtime_alerts(char *, time_t, unsigned, au_log *);
int au_add_downtime_log(au_log *, time_t, int, void *, int);
void au_free_downtime_log(au_log_downtime *);
int parse_notification_log(char *, time_t, int, au_log *);
int au_add_notification_log(au_log *, time_t, int, void *, au_contact *, int, 
		char *, char *);
void au_free_notification_log(au_log_notification *);
au_log_entry *au_add_log_entry(au_log *, time_t, int, void *);
void au_free_log_entry_void(void *);
void au_free_log_entry(au_log_entry *);
au_host *au_add_host_and_sort(au_array *, char *);
void au_free_host_void(void *);
void au_free_host(au_host *);
int	au_cmp_hosts(const void *, const void *);
au_service *au_add_service_and_sort(au_array *, char *, char *);
int	au_cmp_services(const void *, const void *);
void au_free_service_void(void *);
void au_free_service(au_service *);
au_contact *au_add_contact_and_sort(au_array *, char *);
au_contact *au_add_contact(au_array *, char *);
void au_free_contact_void(void *);
void au_free_contact(au_contact *);
int	au_cmp_contacts(const void *, const void *);
au_contact *au_find_contact(au_array *, char *);
void au_sort_array(au_array *, int(*cmp)(const void *, const void *));
void *au_find_in_array(au_array *, void *, 
		int(*cmp)(const void *, const void *));
au_linked_list *au_init_list(char *);
void au_empty_list(au_linked_list *, void(*)(void *));
void au_free_list(au_linked_list *, void(*)(void *));

/* External variables */
extern int log_rotation_method;

/* Initialize log structure */
au_log *au_init_log(void) {
	au_log *log;

	/* Initialize the log structure itself */
	if((log = calloc(1, sizeof( au_log))) == NULL) {
		return NULL;
		}

	/* Initialize the host subjects */
	if((log->host_subjects = au_init_array("host_subjects")) == NULL) {
		au_free_log(log);
		return NULL;
		}

	/* Initialize the service subjects */
	if((log->service_subjects = au_init_array("service_subjects")) == NULL) {
		au_free_log(log);
		return NULL;
		}

	/* Initialize the log entry list */
	if((log->entry_list = au_init_list("global log entries")) == NULL) {
		au_free_log(log);
		return NULL;
		}

	/* Initialize the host list */
	if((log->hosts = au_init_array("hosts")) == NULL) {
		au_free_log(log);
		return NULL;
		}

	/* Initialize the service list */
	if((log->services = au_init_array("services")) == NULL) {
		au_free_log(log);
		return NULL;
		}

	/* Initialize the contact list */
	if((log->contacts = au_init_array("contacts")) == NULL) {
		au_free_log(log);
		return NULL;
		}

	return log;
	}

void au_free_log(au_log *log) {
	if(NULL == log) return;
	if(NULL != log->host_subjects) {
		au_free_array(log->host_subjects, au_free_host_void);
		}
	if(NULL != log->service_subjects) {
		au_free_array(log->service_subjects, au_free_service_void);
		}
	if(NULL != log->hosts) {
		au_free_array(log->hosts, au_free_host_void);
		}
	if(NULL != log->services) {
		au_free_array(log->services, au_free_service_void);
		}
	if(NULL != log->contacts) {
		au_free_array(log->contacts, au_free_contact_void);
		}
	if(NULL != log->entry_list) {
		au_free_list(log->entry_list, au_free_log_entry_void);
		}
	free(log);
	}

/* reads log files for archived data */
int read_archived_data(time_t start_time, time_t end_time, 
		int backtrack_archives, unsigned obj_types, unsigned state_types, 
		unsigned log_types, au_log *log, time_t *last_archive_data_update) {

	char filename[MAX_FILENAME_LENGTH];
	int oldest_archive = 0;
	int newest_archive = 0;
	int current_archive = 0;
	int	x;
	int	retval = 1;
	au_host *global_host;
	au_host *temp_host;
	au_service *temp_service;
	au_node *temp_entry;
	struct stat adstat;

	/* Determine oldest archive to use when scanning for data 
		(include backtracked archives as well) */
	oldest_archive = determine_archive_to_use_from_time(start_time);
	if(log_rotation_method != LOG_ROTATION_NONE) {
		oldest_archive += backtrack_archives;
		}

	/* determine most recent archive to use when scanning for data */
	newest_archive = determine_archive_to_use_from_time(end_time);

	if(oldest_archive < newest_archive) {
		oldest_archive = newest_archive;
		}

	/* read in all the necessary archived logs (from most recent to earliest) */
	for(current_archive = newest_archive; current_archive <= oldest_archive; 
			current_archive++) {

#ifdef DEBUG
		printf("Reading archive #%d... ", current_archive);
#endif

		/* get the name of the log file that contains this archive */
		get_log_archive_to_use(current_archive, filename, sizeof(filename) - 1);

#ifdef DEBUG
		printf("Archive name: '%s'\n", filename);
#endif

		/* Record the last modification time of the the archive file */
		if(stat(filename, &adstat) < 0) {
			/* ENOENT is OK because Nagios may have been down when the 
				logs were being rotated */
			if(ENOENT != errno) return -1;
			}
		else {
			if(*last_archive_data_update < adstat.st_mtime) {
				*last_archive_data_update = adstat.st_mtime;
				}

			/* scan the log file for archived state data */
			if(read_log_file(filename, obj_types, state_types, log_types, 
					log) == 0) {
				return 0;	/* Memory allocation error */
				}
			}
		}

	/* Add Nagios log events to all hosts and services found */
	global_host = au_find_host(log->hosts, "*");
	if(NULL != global_host) {
		for(temp_entry = global_host->log_entries->head; NULL != temp_entry; 
				temp_entry = temp_entry->next) {
			for(x = 0; x < log->hosts->count; x++) {
				temp_host = log->hosts->members[x];
				if(temp_host == global_host) continue;
				if(au_list_add_node(temp_host->log_entries, 
						temp_entry->data, au_cmp_log_entries) == 0) {
					retval = 0;
					break;
					}
				}
			if(0 == retval) break;
			for(x = 0; x < log->services->count; x++) {
				temp_service = log->services->members[x];
				if(au_list_add_node(temp_service->log_entries, 
							temp_entry->data, au_cmp_log_entries) == 0) {
					retval = 0;
					break;
					}
				}
			if(0 == retval) break;
			}
		}

	return 1;
	}

/* grabs archives state data from a log file */
int read_log_file(char *filename, unsigned obj_types, unsigned state_types, 
		unsigned log_types, au_log *log) {
	char *input = NULL;
	char *input2 = NULL;
	char *temp_buffer = NULL;
	time_t time_stamp;
	mmapfile *thefile = NULL;
	int	retval = 1;

	if((thefile = mmap_fopen(filename)) == NULL) {
		return 1;
		}

	while(1) {

		/* free memory */
		free(input);
		free(input2);
		input = NULL;
		input2 = NULL;

		/* read the next line */
		if((input = mmap_fgets(thefile)) == NULL) break;

		strip(input);

		if((input2 = strdup(input)) == NULL) continue;

		temp_buffer = my_strtok(input2, "]");
		time_stamp = (temp_buffer == NULL) ? (time_t)0 : 
				(time_t)strtoul(temp_buffer + 1, NULL, 10);

		/* program starts/restarts */
		if(strstr(input, " starting...")) {
			if(au_add_nagios_log(log, time_stamp, AU_STATE_PROGRAM_START, 
					"Program start") == 0) {
				retval = 0;
				break;
				}
			}
		if(strstr(input, " restarting...")) {
			if(au_add_nagios_log(log, time_stamp, AU_STATE_PROGRAM_START, 
					"Program restart") == 0) {
				retval = 0;
				break;
				}
			}

		/* program stops */
		if(strstr(input, " shutting down...")) {
			if(au_add_nagios_log(log, time_stamp, AU_STATE_PROGRAM_END, 
					"Normal program termination") == 0) {
				retval = 0;
				break;
				}
			}
		if(strstr(input, "Bailing out")) {
			if(au_add_nagios_log(log, time_stamp, AU_STATE_PROGRAM_END, 
					"Abnormal program termination") == 0) {
				retval = 0;
				break;
				}
			}

		if(obj_types & AU_OBJTYPE_HOST) {

			/* normal host alerts */
			if((log_types & AU_LOGTYPE_ALERT) && strstr(input, "HOST ALERT:")) {
				if(parse_states_and_alerts(input, time_stamp, AU_LOGTYPE_ALERT, 
						AU_OBJTYPE_HOST, state_types, log) == 0) {
					retval = 0;
					break;
					}
				}

			/* host initial states */
			else if((log_types & AU_LOGTYPE_STATE) && 
					strstr(input, "INITIAL HOST STATE:")) {
				if(parse_states_and_alerts(input, time_stamp, 
						AU_LOGTYPE_STATE_INITIAL, AU_OBJTYPE_HOST, state_types, 
						log) == 0) {
					retval = 0;
					break;
					}
				}

			/* host current states */
			else if((log_types & AU_LOGTYPE_STATE) && 
					strstr(input, "CURRENT HOST STATE:")) {
				if(parse_states_and_alerts(input, time_stamp, 
						AU_LOGTYPE_STATE_CURRENT, AU_OBJTYPE_HOST, state_types, 
						log) == 0) {
					retval = 0;
					break;
					}
				}

			/* scheduled downtime notices */
			else if((log_types & AU_LOGTYPE_DOWNTIME) && 
					strstr(input, "HOST DOWNTIME ALERT:")) {
				if(parse_downtime_alerts(input, time_stamp, AU_OBJTYPE_HOST, 
						log) == 0) {
					retval = 0;
					break;
					}
				}

			/* host notifications */
			else if((log_types & AU_LOGTYPE_NOTIFICATION) && 
					strstr(input, "HOST NOTIFICATION:")) {
				if(parse_notification_log(input, time_stamp, AU_OBJTYPE_HOST, 
						log) ==0) {
					retval = 0;
					break;
					}
				}
	
			}

		if(obj_types & AU_OBJTYPE_SERVICE) {

			/* normal service alerts */
			if((log_types & AU_LOGTYPE_ALERT) && 
					strstr(input, "SERVICE ALERT:")) {
				if(parse_states_and_alerts(input, time_stamp, AU_LOGTYPE_ALERT, 
						AU_OBJTYPE_SERVICE, state_types, log) == 0) {
					retval = 0;
					break;
					}
				}

			/* service initial states */
			else if((log_types & AU_LOGTYPE_STATE) && 
					strstr(input, "INITIAL SERVICE STATE:")) {
				if(parse_states_and_alerts(input, time_stamp, 
						AU_LOGTYPE_STATE_INITIAL, AU_OBJTYPE_SERVICE, 
						state_types, log) == 0) {
					retval = 0;
					break;
					}
				}

			/* service current states */
			else if((log_types & AU_LOGTYPE_STATE) && 
					strstr(input, "CURRENT SERVICE STATE:")) {
				if(parse_states_and_alerts(input, time_stamp, 
						AU_LOGTYPE_STATE_CURRENT, AU_OBJTYPE_SERVICE, 
						state_types, log) == 0) {
					retval = 0;
					break;
					}
				}

			/* scheduled service downtime notices */
			else if((log_types & AU_LOGTYPE_DOWNTIME) && 
					strstr(input, "SERVICE DOWNTIME ALERT:")) {
				if(parse_downtime_alerts(input, time_stamp, AU_OBJTYPE_SERVICE, 
						log) == 0) {
					retval = 0;
					break;
					}
				}

			/* service notifications */
			else if((log_types & AU_LOGTYPE_NOTIFICATION) && 
					strstr(input, "SERVICE NOTIFICATION:")) {
				if(parse_notification_log(input, time_stamp, 
						AU_OBJTYPE_SERVICE, log) ==0) {
					retval = 0;
					break;
					}
				}
			}
		}

	/* free memory and close the file */
	free(input);
	free(input2);
	mmap_fclose(thefile);
	return retval;
	}

int	au_cmp_log_entries(const void *a, const void *b) {

	au_log_entry *lea = *(au_log_entry **)a;
	au_log_entry *leb = *(au_log_entry **)b;

	if(lea->timestamp == leb->timestamp) return 0;
	else if(lea->timestamp < leb->timestamp) return -1;
	else return 1;
	}

int au_add_nagios_log(au_log *log, time_t timestamp, int type, 
		char *description) {

	au_log_nagios *nagios_log;
	au_log_entry *new_log_entry;
	au_host *global_host;

	/* Create the au_log_nagios */
	if((nagios_log = calloc(1, sizeof(au_log_nagios))) == NULL) {
		return 0;
		}
	nagios_log->type = type;
	if((nagios_log->description = strdup(description)) == NULL) {
		au_free_nagios_log(nagios_log);
		return 0;
		}

	/* Create the log entry */
	if((new_log_entry = au_add_log_entry(log, timestamp, AU_LOGTYPE_NAGIOS, 
			(void *)nagios_log)) == NULL) {
		au_free_nagios_log(nagios_log);
		return 0;
		}

	/* Find the au_host object associated with the global host */
	global_host = au_find_host(log->hosts, "*");
	if(NULL == global_host) {
		global_host = au_add_host_and_sort(log->hosts, "*");
		if(NULL == global_host) { /* Could not allocate memory */
			return 0;
			}
		}

	/* Add the entry to the global host */
	if(au_list_add_node(global_host->log_entries, new_log_entry, 
			au_cmp_log_entries) == 0) {
		return 0;
		}

	return 1;
	}

void au_free_nagios_log(au_log_nagios *nagios_log) {
	if(NULL == nagios_log) return;
	if(NULL != nagios_log->description) free(nagios_log->description);
	free(nagios_log);
	}

/* parse state and alert log entries */
int parse_states_and_alerts(char *input, time_t timestamp, int log_type, 
		unsigned obj_type, unsigned state_types, au_log *log) {

	char *temp_buffer = NULL;
	char entry_host_name[MAX_INPUT_BUFFER];
	char entry_svc_description[MAX_INPUT_BUFFER];
	char *plugin_output;
	au_host *temp_host_subject;
	au_host *temp_host = NULL;
	au_service *temp_service_subject;
	au_service *temp_service = NULL;
	int	state_type;
	int	state;

	/* get host name */
	temp_buffer = my_strtok(NULL, ":");
	temp_buffer = my_strtok(NULL, ";");
	strncpy(entry_host_name, (temp_buffer == NULL) ? "" : 
			temp_buffer + 1, sizeof(entry_host_name));
	entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

	switch(obj_type) {
	case AU_OBJTYPE_HOST:
		if(log->host_subjects->count > 0) {
			/* see if there is a corresponding subject for this host */
			temp_host_subject = au_find_host(log->host_subjects, 
					entry_host_name);
			if(temp_host_subject == NULL) return 1;
			}

		/* Find the au_host object associated with the host name */
		temp_host = au_find_host(log->hosts, entry_host_name);
		if(NULL == temp_host) {
			temp_host = au_add_host_and_sort(log->hosts, entry_host_name);
			if(NULL == temp_host) { /* Could not allocate memory */
				return 0;
				}
			}

		break;
	case AU_OBJTYPE_SERVICE:
		/* get service description */
		temp_buffer = my_strtok(NULL, ";");
		strncpy(entry_svc_description, (temp_buffer == NULL) ? "" : 
				temp_buffer, sizeof(entry_svc_description));
		entry_svc_description[sizeof(entry_svc_description) - 1] = '\x0';

		if(log->service_subjects->count > 0) {
			/* see if there is a corresponding subject for this service */
			temp_service_subject = au_find_service(log->service_subjects, 
					entry_host_name, entry_svc_description);
			if(temp_service_subject == NULL) return 1;
			}

		/* Find the au_service object associated with the service */
		temp_service = au_find_service(log->services, entry_host_name, 
				entry_svc_description);
		if(NULL == temp_service) {
			temp_service = au_add_service_and_sort(log->services, 
					entry_host_name, entry_svc_description);
			if(NULL == temp_service) { /* Could not allocate memory */
				return 0;
				}
			}

		break;
		}

	/* state types */
	if(strstr(input, ";SOFT;")) {
		if(!(state_types & AU_STATETYPE_SOFT)) return 1;
		state_type = AU_STATETYPE_SOFT;
		}
	if(strstr(input, ";HARD;")) {
		if(!(state_types & AU_STATETYPE_HARD)) return 1;
		state_type = AU_STATETYPE_HARD;
		}

	/* get the plugin output */
	temp_buffer = my_strtok(NULL, ";");
	temp_buffer = my_strtok(NULL, ";");
	temp_buffer = my_strtok(NULL, ";");
	plugin_output = my_strtok(NULL, "\n");

	switch(obj_type) {
	case AU_OBJTYPE_HOST:
		if(strstr(input, ";DOWN;")) {
			state = AU_STATE_HOST_DOWN;
			}
		else if(strstr(input, ";UNREACHABLE;")) {
			state = AU_STATE_HOST_UNREACHABLE;
			}
		else if(strstr(input, ";RECOVERY") || strstr(input, ";UP;")) {
			state = AU_STATE_HOST_UP;
			}
		else {
			state = AU_STATE_NO_DATA;
			state_type = AU_STATETYPE_NO_DATA;
			}
		if(au_add_alert_or_state_log(log, timestamp, log_type, AU_OBJTYPE_HOST, 
				(void *)temp_host, state_type, state, plugin_output) == 0) {
			return 0;
			}
		break;
	case AU_OBJTYPE_SERVICE:
		if(strstr(input, ";CRITICAL;")) {
			state = AU_STATE_SERVICE_CRITICAL;
			}
		else if(strstr(input, ";WARNING;")) {
			state = AU_STATE_SERVICE_WARNING;
			}
		else if(strstr(input, ";UNKNOWN;")) {
			state = AU_STATE_SERVICE_UNKNOWN;
			}
		else if(strstr(input, ";RECOVERY;") || strstr(input, ";OK;")) {
			state = AU_STATE_SERVICE_OK;
			}
		else {
			state = AU_STATE_NO_DATA;
			state_type = AU_STATETYPE_NO_DATA;
			}
		if(au_add_alert_or_state_log(log, timestamp, log_type, 
				AU_OBJTYPE_SERVICE, (void *)temp_service, state_type, state, 
				plugin_output) == 0) {
			return 0;
			}
		break;
		}

	return 1;
	}

int au_add_alert_or_state_log(au_log *log, time_t timestamp, int log_type,
		int obj_type, void *object, int state_type, int state, 
		char *plugin_output) {

	au_log_alert *alert_log;
	au_log_entry *new_log_entry;

	/* Create the au_log_alert */
	if((alert_log = au_create_alert_or_state_log(obj_type, object, state_type, 
			state, plugin_output)) == NULL) {
		return 0;
		}

	/* Create the log entry */
	if((new_log_entry = au_add_log_entry(log, timestamp, log_type, 
			(void *)alert_log)) == NULL) {
		au_free_alert_log(alert_log);
		return 0;
		}

	/* Add the log entry to the logs for the object supplied */
	switch(obj_type) {
	case AU_OBJTYPE_HOST:
		if(au_list_add_node(((au_host *)object)->log_entries,
				new_log_entry, au_cmp_log_entries) == 0) {
			return 0;
			}
		break;
	case AU_OBJTYPE_SERVICE:
		if(au_list_add_node(((au_service *)object)->log_entries, 
				new_log_entry, au_cmp_log_entries) == 0) {
			return 0;
			}
		break;
		}

	return 1;
	}

au_log_alert *au_create_alert_or_state_log(int obj_type, void *object, 
		int state_type, int state, char *plugin_output) {

	au_log_alert *alert_log;

	/* Create the au_log_alert */
	if((alert_log = calloc(1, sizeof(au_log_alert))) == NULL) {
		return NULL;
		}
	alert_log->obj_type = obj_type;
	alert_log->object = object;
	alert_log->state_type = state_type;
	alert_log->state = state;
	alert_log->plugin_output = NULL;
	if(plugin_output != NULL) {
		if((alert_log->plugin_output = strdup(plugin_output)) == NULL) {
			au_free_alert_log(alert_log);
			return NULL;
			}
		}

	return alert_log;
	}

void au_free_alert_log(au_log_alert *alert_log) {
	if(NULL == alert_log) return;
	if(NULL != alert_log->plugin_output) free(alert_log->plugin_output);
	free(alert_log);
	}

/* parse archive log downtime notifications */
int parse_downtime_alerts(char *input, time_t timestamp, unsigned obj_type, 
		au_log *log) {

	char *temp_buffer = NULL;
	char entry_host_name[MAX_INPUT_BUFFER];
	char entry_svc_description[MAX_INPUT_BUFFER];
	au_host *temp_host_subject;
	au_host *temp_host = NULL;
	au_service *temp_service_subject;
	au_service *temp_service = NULL;

	/* get host name */
	temp_buffer = my_strtok(NULL, ":");
	temp_buffer = my_strtok(NULL, ";");
	strncpy(entry_host_name, (temp_buffer == NULL) ? "" : 
			temp_buffer + 1, sizeof(entry_host_name));
	entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

	switch(obj_type) {
	case AU_OBJTYPE_HOST:
		if(log->host_subjects->count > 0) {
			/* see if there is a corresponding subject for this host */
			temp_host_subject = au_find_host(log->host_subjects, 
					entry_host_name);
			if(temp_host_subject == NULL) return 1;
			}

		/* Find the au_host object associated with the host name */
		temp_host = au_find_host(log->hosts, entry_host_name);
		if(NULL == temp_host) {
			temp_host = au_add_host_and_sort(log->hosts, entry_host_name);
			if(NULL == temp_host) { /* Could not allocate memory */
				return 0;
				}
			}

		break;
	case AU_OBJTYPE_SERVICE:
		/* get service description */
		temp_buffer = my_strtok(NULL, ";");
		strncpy(entry_svc_description, (temp_buffer == NULL) ? "" : 
				temp_buffer, sizeof(entry_svc_description));
		entry_svc_description[sizeof(entry_svc_description) - 1] = '\x0';

		if(log->service_subjects->count > 0) {
			/* see if there is a corresponding subject for this service */
			temp_service_subject = au_find_service(log->service_subjects, 
					entry_host_name, entry_svc_description);
			if(temp_service_subject == NULL) return 1;
			}

		/* Find the au_service object associated with the service */
		temp_service = au_find_service(log->services, entry_host_name, 
				entry_svc_description);
		if(NULL == temp_service) {
			temp_service = au_add_service_and_sort(log->services, 
					entry_host_name, entry_svc_description);
			if(NULL == temp_service) { /* Could not allocate memory */
				return 0;
				}
			}

		break;
		}

	switch(obj_type) {
	case AU_OBJTYPE_HOST:
		if(strstr(input, ";STARTED;")) {
			if(au_add_downtime_log(log, timestamp, AU_OBJTYPE_HOST, 
					(void *)temp_host, AU_STATE_DOWNTIME_START) == 0) {
				return 0;
				}
			}
		else {
			if(au_add_downtime_log(log, timestamp, AU_OBJTYPE_HOST, 
					(void *)temp_host, AU_STATE_DOWNTIME_END) == 0) {
				return 0;
				}
			}
		break;
	case AU_OBJTYPE_SERVICE:
		if(strstr(input, ";STARTED;")) {
			if(au_add_downtime_log(log, timestamp, AU_OBJTYPE_SERVICE, 
					(void *)temp_service, AU_STATE_DOWNTIME_START) == 0) {
				return 0;
				}
			}
		else {
			if(au_add_downtime_log(log, timestamp, AU_OBJTYPE_SERVICE, 
					(void *)temp_service, AU_STATE_DOWNTIME_END) == 0) {
				return 0;
				}
			}
		break;
		}

	return 1;
	}

int au_add_downtime_log(au_log *log, time_t timestamp, int obj_type, 
		void *object, int downtime_type) {

	au_log_downtime *downtime_log;
	au_log_entry *new_log_entry;

	/* Create the au_log_downtime */
	if((downtime_log = calloc(1, sizeof(au_log_downtime))) == NULL) {
		return 0;
		}
	downtime_log->obj_type = obj_type;
	downtime_log->object = object;
	downtime_log->downtime_type = downtime_type;

	/* Create the log entry */
	if((new_log_entry = au_add_log_entry(log, timestamp, AU_LOGTYPE_DOWNTIME, 
			(void *)downtime_log)) == NULL) {
		au_free_downtime_log(downtime_log);
		return 0;
		}

	/* Add the log entry to the logs for the object supplied */
	switch(obj_type) {
	case AU_OBJTYPE_HOST:
		if(au_list_add_node(((au_host *)object)->log_entries,
				new_log_entry, au_cmp_log_entries) == 0) {
			return 0;
			}
		break;
	case AU_OBJTYPE_SERVICE:
		if(au_list_add_node(((au_service *)object)->log_entries, 
				new_log_entry, au_cmp_log_entries) == 0) {
			return 0;
			}
		break;
		}

	return 1;
	}

void au_free_downtime_log(au_log_downtime *downtime_log) {
	if(NULL == downtime_log) return;
	free(downtime_log);
	}

int parse_notification_log(char *input, time_t timestamp, int obj_type, 
		au_log *log) {

	char entry_contact_name[MAX_INPUT_BUFFER];
	char entry_host_name[MAX_INPUT_BUFFER];
	char entry_svc_description[MAX_INPUT_BUFFER];
	char alert_level[MAX_INPUT_BUFFER];
	char method_name[MAX_INPUT_BUFFER];
	int	notification_detail_type = AU_NOTIFICATION_NO_DATA;
	char *temp_buffer = NULL;
	au_host *temp_host_subject;
	au_service *temp_service_subject;
	au_contact *temp_contact;
	au_host *temp_host = NULL;
	au_service *temp_service = NULL;

	/* get the contact name */
	temp_buffer = my_strtok(NULL, ":");
	temp_buffer = my_strtok(NULL, ";");
	strncpy(entry_contact_name, (temp_buffer == NULL) ? "" : 
			temp_buffer + 1, sizeof(entry_contact_name));
	entry_contact_name[sizeof(entry_contact_name) - 1] = '\x0';

	/* Find the au_contact object associated with the contact name */
	temp_contact = au_find_contact(log->contacts, entry_contact_name);
	if(NULL == temp_contact) {
		temp_contact = au_add_contact_and_sort(log->contacts, 
				entry_contact_name);
		if(NULL == temp_contact) { /* Could not allocate memory */
			return 0;
			}
		}

	/* get the host name */
	temp_buffer = (char *)my_strtok(NULL, ";");
	snprintf(entry_host_name, sizeof(entry_host_name), "%s", 
			(temp_buffer == NULL) ? "" : temp_buffer);
	entry_host_name[sizeof(entry_host_name) - 1] = '\x0';

	switch(obj_type) {
	case AU_OBJTYPE_HOST:
		if(log->host_subjects->count > 0) {
			/* see if there is a corresponding subject for this host */
			temp_host_subject = au_find_host(log->host_subjects, 
					entry_host_name);
			if(temp_host_subject == NULL) return 1;
			}

		/* Find the au_host object associated with the host name */
		temp_host = au_find_host(log->hosts, entry_host_name);
		if(NULL == temp_host) {
			temp_host = au_add_host_and_sort(log->hosts, entry_host_name);
			if(NULL == temp_host) { /* Could not allocate memory */
				return 0;
				}
			}

		break;
	case AU_OBJTYPE_SERVICE:
		/* get service description */
		temp_buffer = my_strtok(NULL, ";");
		strncpy(entry_svc_description, (temp_buffer == NULL) ? "" : 
				temp_buffer, sizeof(entry_svc_description));
		entry_svc_description[sizeof(entry_svc_description) - 1] = '\x0';

		if(log->service_subjects->count > 0) {
			/* see if there is a corresponding subject for this service */
			temp_service_subject = au_find_service(log->service_subjects, 
					entry_host_name, entry_svc_description);
			if(temp_service_subject == NULL) return 1;
			}

		/* Find the au_service object associated with the service */
		temp_service = au_find_service(log->services, entry_host_name, 
				entry_svc_description);
		if(NULL == temp_service) {
			temp_service = au_add_service_and_sort(log->services, 
					entry_host_name, entry_svc_description);
			if(NULL == temp_service) { /* Could not allocate memory */
				return 0;
				}
			}

		break;
		}

	/* get the alert level */
	temp_buffer = (char *)my_strtok(NULL, ";");
	snprintf(alert_level, sizeof(alert_level), "%s", 
			(temp_buffer == NULL) ? "" : temp_buffer);
	alert_level[sizeof(alert_level) - 1] = '\x0';

	switch(obj_type) {
	case AU_OBJTYPE_HOST:
		if(!strcmp(alert_level, "DOWN")) {
			notification_detail_type = AU_NOTIFICATION_HOST_DOWN;
			}
		else if(!strcmp(alert_level, "UNREACHABLE")) {
			notification_detail_type = AU_NOTIFICATION_HOST_UNREACHABLE;
			}
		else if(!strcmp(alert_level, "RECOVERY") || 
				!strcmp(alert_level, "UP")) {
			notification_detail_type = AU_NOTIFICATION_HOST_RECOVERY;
			}
		else if(strstr(alert_level, "CUSTOM (")) {
			notification_detail_type = AU_NOTIFICATION_HOST_CUSTOM;
			}
		else if(strstr(alert_level, "ACKNOWLEDGEMENT (")) {
			notification_detail_type = AU_NOTIFICATION_HOST_ACK;
			}
		else if(strstr(alert_level, "FLAPPINGSTART (")) {
			notification_detail_type = AU_NOTIFICATION_HOST_FLAPPING_START;
			}
		else if(strstr(alert_level, "FLAPPINGSTOP (")) {
			notification_detail_type = AU_NOTIFICATION_HOST_FLAPPING_STOP;
			}
		break;
	case AU_OBJTYPE_SERVICE:
		if(!strcmp(alert_level, "CRITICAL")) {
			notification_detail_type = AU_NOTIFICATION_SERVICE_CRITICAL;
			}
		else if(!strcmp(alert_level, "WARNING")) {
			notification_detail_type = AU_NOTIFICATION_SERVICE_WARNING;
			}
		else if(!strcmp(alert_level, "RECOVERY") || 
				!strcmp(alert_level, "OK")) {
			notification_detail_type = AU_NOTIFICATION_SERVICE_RECOVERY;
			}
		else if(strstr(alert_level, "CUSTOM (")) {
			notification_detail_type = AU_NOTIFICATION_SERVICE_CUSTOM;
			}
		else if(strstr(alert_level, "ACKNOWLEDGEMENT (")) {
			notification_detail_type = AU_NOTIFICATION_SERVICE_ACK;
			}
		else if(strstr(alert_level, "FLAPPINGSTART (")) {
			notification_detail_type = AU_NOTIFICATION_SERVICE_FLAPPING_START;
			}
		else if(strstr(alert_level, "FLAPPINGSTOP (")) {
			notification_detail_type = AU_NOTIFICATION_SERVICE_FLAPPING_STOP;
			}
		else {
			notification_detail_type = AU_NOTIFICATION_SERVICE_UNKNOWN;
			}
		break;
		}

	/* get the method name */
	temp_buffer = (char *)my_strtok(NULL, ";");
	snprintf(method_name, sizeof(method_name), "%s", 
			(temp_buffer == NULL) ? "" : temp_buffer);
	method_name[sizeof(method_name) - 1] = '\x0';

	/* move to the informational message */
	temp_buffer = my_strtok(NULL, ";");

	/* Create the log entry */
	switch(obj_type) {
	case AU_OBJTYPE_HOST:
		if(au_add_notification_log(log, timestamp, AU_OBJTYPE_HOST, 
				(void *)temp_host, temp_contact, notification_detail_type, 
				method_name, temp_buffer) == 0) {
			return 0;
			}
		break;
	case AU_OBJTYPE_SERVICE:
		if(au_add_notification_log(log, timestamp, AU_OBJTYPE_SERVICE, 
				(void *)temp_service, temp_contact, notification_detail_type, 
				method_name, temp_buffer) == 0) {
			return 0;
			}
		break;
		}

	return 1;
	}

int au_add_notification_log(au_log *log, time_t timestamp, int obj_type, 
		void *object, au_contact *contact, int notification_type, 
		char *method, char *message) {

	au_log_notification *notification_log;
	au_log_entry *new_log_entry;

	/* Create the au_log_downtime */
	if((notification_log = calloc(1, sizeof(au_log_notification))) == NULL) {
		return 0;
		}
	notification_log->obj_type = obj_type;
	notification_log->object = object;
	notification_log->contact = contact;
	notification_log->notification_type = notification_type;
	if((notification_log->method = strdup(method)) 
			== NULL) {
		au_free_notification_log(notification_log);
		return 0;
		}
	if((notification_log->message = strdup(message)) == NULL) {
		au_free_notification_log(notification_log);
		return 0;
		}

	/* Create the log entry */
	if((new_log_entry = au_add_log_entry(log, timestamp, 
			AU_LOGTYPE_NOTIFICATION, (void *)notification_log)) == NULL) {
		au_free_notification_log(notification_log);
		return 0;
		}

	/* Add the log entry to the logs for the object supplied */
	switch(obj_type) {
	case AU_OBJTYPE_HOST:
		if(au_list_add_node((void *)((au_host *)object)->log_entries,
				new_log_entry, au_cmp_log_entries) == 0) {
			return 0;
			}
		break;
	case AU_OBJTYPE_SERVICE:
		if(au_list_add_node((void *)((au_service *)object)->log_entries, 
				new_log_entry, au_cmp_log_entries) == 0) {
			return 0;
			}
		break;
		}

	return 1;
	}

void au_free_notification_log(au_log_notification *notification_log) {
	if(NULL == notification_log) return;
	if(NULL != notification_log->method) free(notification_log->method);
	if(NULL != notification_log->message) free(notification_log->message);
	free(notification_log);
	}

au_log_entry *au_add_log_entry(au_log *log, time_t timestamp, int entry_type, 
		void *entry) {

	au_log_entry *new_log_entry;

	/* Create the au_log_entry */
	if((new_log_entry = calloc(1, sizeof(au_log_entry))) == NULL) {
		return NULL;
		}
	new_log_entry->timestamp = timestamp;
	new_log_entry->entry_type = entry_type;
	new_log_entry->entry = entry;

	if(au_list_add_node(log->entry_list, (void *)new_log_entry, 
			au_cmp_log_entries) == NULL) {
		au_free_log_entry(new_log_entry);
		return NULL;
		}

	return new_log_entry;
	}

void au_free_log_entry_void(void *log_entry) {
	au_free_log_entry((au_log_entry *)log_entry);
	}

void au_free_log_entry(au_log_entry *log_entry) {
	if(NULL == log_entry) return;
	switch(log_entry->entry_type) {
	case AU_LOGTYPE_ALERT:
	case AU_LOGTYPE_STATE:
		au_free_alert_log((au_log_alert *)log_entry->entry);
		break;
	case AU_LOGTYPE_NOTIFICATION:
		au_free_notification_log((au_log_notification *)log_entry->entry);
		break;
	case AU_LOGTYPE_DOWNTIME:
		au_free_downtime_log((au_log_downtime *)log_entry->entry);
		break;
	case AU_LOGTYPE_NAGIOS:
		au_free_nagios_log((au_log_nagios *)log_entry->entry);
		break;
		}
	free(log_entry);
	}

/* Add a host to a host list and sort the list */
au_host *au_add_host_and_sort(au_array *host_list, char *name) {

	au_host *temp_host;

	temp_host = au_add_host(host_list, name);
	if(NULL != temp_host) {
		au_sort_array(host_list, au_cmp_hosts);
		}

	return temp_host;
	}

/* Add a host to a host list */
au_host *au_add_host(au_array *host_list, char *name) {

	au_host	*new_host;
	char	buf[8192];

	/* Should have been allocated during au_log_init() */
	if(NULL == host_list) {
		return NULL;
		}

	/* Create the host */
	if((new_host = calloc(1, sizeof(au_host))) == NULL) {
		return NULL;
		}
	if((new_host->name = strdup(name)) == NULL) {
		au_free_host(new_host);
		return NULL;
		}
	new_host->hostp = find_host(name);
	new_host->availability = NULL;
	snprintf(buf, sizeof(buf) - 1, "Host %s log entries", name);
	if((new_host->log_entries = au_init_list(buf)) == NULL) {
		au_free_host(new_host);
		return NULL;
		}

	/* Add it to the list of hosts */
	if(0 == au_array_append_member(host_list, (void *)new_host)) {
		au_free_host(new_host);
		return NULL;
		}

	return new_host;
	}

void au_free_host_void(void *host) {
	au_free_host((au_host *)host);
	}

void au_free_host(au_host *host) {
	if(NULL == host) return;
	if(NULL != host->name) free(host->name);
	/* Do not free the log entry data here because they are freed when the
		master list's log entries are freed */
	if(NULL != host->log_entries) au_free_list(host->log_entries, NULL);
	if(NULL != host->availability) free(host->availability);
	free(host);
	return;
	}

int	au_cmp_hosts(const void *a, const void *b) {
	au_host *hosta = *(au_host **)a;
	au_host *hostb = *(au_host **)b;

	return strcmp(hosta->name, hostb->name);
	}

au_host *au_find_host(au_array *host_list, char *name) {

	au_host	key;
	void *found;

	if(NULL == host_list) return NULL;
	key.name = name;
	found = au_find_in_array(host_list, (void *)&key, au_cmp_hosts);
	if(NULL == found) return NULL;
	return *(au_host **)found;
	}

au_service *au_add_service_and_sort(au_array *service_list, char *host_name, 
		char *description) {

	au_service *temp_service;

	temp_service = au_add_service(service_list, host_name, description);
	if(NULL != temp_service) {
		au_sort_array(service_list, au_cmp_services);
		}

	return temp_service;
	}

au_service *au_add_service(au_array *service_list, char *host_name, 
		char *description) {

	au_service	*new_service;
	char	buf[8192];

	/* Should have been allocated during au_log_init() */
	if(NULL == service_list) {
		return NULL;
		}

	/* Create the service */
	if((new_service = calloc(1, sizeof(au_service))) == NULL) {
		return NULL;
		}
	if((new_service->host_name = strdup(host_name)) == NULL) {
		au_free_service(new_service);
		return NULL;
		}
	if((new_service->description = strdup(description)) == NULL) {
		au_free_service(new_service);
		return NULL;
		}
	new_service->servicep = find_service(host_name, description);
	new_service->availability = NULL;
	snprintf(buf, sizeof(buf) - 1, "Service %s:%s log entries", host_name, 
			description);
	if((new_service->log_entries = au_init_list(buf)) == NULL) {
		au_free_service(new_service);
		return NULL;
		}

	/* Add it to the list of services */
	if(0 == au_array_append_member(service_list, (void *)new_service)) {
		au_free_service(new_service);
		return NULL;
		}
	
	return new_service;
	}

void au_free_service_void(void *service) {
	au_free_service((au_service *)service);
	}

void au_free_service(au_service *service) {
	if(NULL == service) return;
	if(NULL != service->host_name) free(service->host_name);
	if(NULL != service->description) free(service->description);
	/* Do not free the log entry data here because they are freed when the
		master list's log entries are freed */
	if(NULL != service->log_entries) au_free_list(service->log_entries, NULL);
	if(NULL != service->availability) free(service->availability);
	free(service);
	return;
	}

au_service *au_find_service(au_array *service_list, char *host_name,
		char *description) {

	au_service	key;
	void *found;

	if(NULL == service_list) return NULL;
	key.host_name = host_name;
	key.description = description;
	found = (au_service *)au_find_in_array(service_list, (void *)&key,
			au_cmp_services);
	if(NULL == found) return NULL;
	return *(au_service **)found;
	}

int	au_cmp_services(const void *a, const void *b) {

	au_service *servicea = *(au_service **)a;
	au_service *serviceb = *(au_service **)b;
	int	host_result;

	host_result = strcmp(servicea->host_name, serviceb->host_name);
	if(0 == host_result) {
		return strcmp(servicea->description, serviceb->description);
		}
	else {
		return host_result;
		}
	}

/* Add a contact to a contact list and sort the list */
au_contact *au_add_contact_and_sort(au_array *contact_list, char *name) {

	au_contact *temp_contact;

	temp_contact = au_add_contact(contact_list, name);
	if(NULL != temp_contact) {
		au_sort_array(contact_list, au_cmp_contacts);
		}
	return temp_contact;
	}

/* Add a contact to a contact list */
au_contact *au_add_contact(au_array *contact_list, char *name) {

	au_contact	*new_contact;

	/* Should have been allocated during au_log_init() */
	if(NULL == contact_list) {
		return NULL;
		}

	/* Create the contact */
	if((new_contact = calloc(1, sizeof(au_contact))) == NULL) {
		return NULL;
		}
	if((new_contact->name = strdup(name)) == NULL) {
		au_free_contact(new_contact);
		return NULL;
		}

	/* Add it to the list of contacts */
	if(0 == au_array_append_member(contact_list, (void *)new_contact)) {
		au_free_contact(new_contact);
		return NULL;
		}

	return new_contact;
	}

void au_free_contact_void(void *contact) {
	au_free_contact((au_contact *)contact);
	}

void au_free_contact(au_contact *contact) {
	if(NULL == contact) return;
	if(NULL != contact->name) free(contact->name);
	free(contact);
	return;
	}

int	au_cmp_contacts(const void *a, const void *b) {

	au_contact *contacta = (au_contact *)a;
	au_contact *contactb = (au_contact *)b;

	return strcmp(contacta->name, contactb->name);
	}

au_contact *au_find_contact(au_array *contact_list, char *name) {

	au_contact	key;
	void *found;

	if(NULL == contact_list) return NULL;
	key.name = name;
	found = (au_contact *)au_find_in_array(contact_list, (void *)&key,
			au_cmp_contacts);
	if(NULL == found) return NULL;
	return *(au_contact **)found;
	}

au_array *au_init_array(char *label) {
	au_array *array;

	if((array = calloc(1, sizeof(au_array))) == NULL) {
		return NULL;
		}
	array->label = NULL;
	if(NULL != label) {
		if((array->label = strdup(label)) == NULL) {
			free(array);
			return NULL;
			}
		}
	array->size = 0;
	array->count = 0;
	array->members = (void **)NULL;
	array->new = 0;

	return array;
	}

void au_free_array(au_array *array, void(*datafree)(void *)) {

	int x;

	if(NULL == array) return;
	if(NULL != array->label) free(array->label);
	for(x = 0; x < array->count; x++) {
		if((NULL != datafree) && (NULL != array->members[x])) {
			datafree(array->members[x]);
			}
		}
	if(NULL != array->members) free(array->members);
	free(array);
	return;
	}

int au_array_append_member(au_array *array, void *member) {

	/* Check whether the list needs to be grown before adding the member */
	if(array->count == array->size) { 
		/* Need to grow the list */
		if(0 == array->count) {
			/* Never had any members */
			if((array->members = (void **)calloc(AU_INITIAL_LIST_SIZE, 
					sizeof(void *))) == NULL) {
				return 0;
				}
			array->size = AU_INITIAL_LIST_SIZE;
			}
		else {
			/* Double the size of the list */
			if((array->members = (void **)realloc(array->members, 
					sizeof(void *) * array->size * 2)) == NULL) {
				return 0;
				}
			array->size *= 2;
			}
		}

	array->members[array->count] = member;
	array->new++;		/* Number of appends since last sort */
	array->count++;

	return 1;
	}

void au_sort_array(au_array *array, int(*cmp)(const void *, const void *)) {

	/* TODO: Use array->new to determine whether to do a quick sort or a 
		bubble sort */
	qsort(array->members, array->count, sizeof(void *), cmp);
	array->new = 0;
	}

void *au_find_in_array(au_array *array, void *key, 
		int(*cmp)(const void *, const void *)) {

	return bsearch(&key, array->members, array->count, sizeof(void *), cmp);
	}

au_linked_list *au_init_list(char *label) {

	au_linked_list *list;

	if((list = calloc(1, sizeof(au_linked_list))) == NULL) {
		return NULL;
		}
	list->label = NULL;
	if(NULL != label) {
		if((list->label = strdup(label)) == NULL) {
			free(list);
			return NULL;
			}
		}
	list->head = (au_node *)0;
	list->last_new = (au_node *)0;

	return list;
	}

au_node *au_list_add_node(au_linked_list *list, void *data, 
		int(*cmp)(const void *, const void *)) {

	au_node *new_node;
	au_node *temp_node;

	/* Create the new node */
	if((new_node = calloc(1, sizeof(au_node))) == NULL) {
		return NULL;
		}
	new_node->data = data;
	new_node->next = NULL;

	/* Add it to the list */
	if(NULL == list->head) {
		/* If the list is empty, add this node as the only item in the list */
		list->head = new_node;
		}
	else if(cmp(&(list->last_new->data), &(new_node->data)) <= 0) {
		/* The new node goes somewhere on the list downstream of the most
			recently added node */
		if(NULL == list->last_new->next) {
			/* If the most recently added node is the last one on the list, 
				append the list with this node */
			list->last_new->next = new_node;
			}
		else if(cmp(&(list->last_new->next->data), &(new_node->data)) > 0) {
			/* If the next node is "greater than" the new node, the new 
				node goes here */
			new_node->next = list->last_new->next;
			list->last_new->next = new_node;
			}
		else {
			/* Visit each node downstream in the list until we reach the 
				end of the list or until we find a node whose next
				node is "greater than" the new node */
			temp_node = list->last_new;
			while((NULL != temp_node->next) && (cmp(&(temp_node->next->data),
					&(new_node->data)) <= 0)) {
				temp_node = temp_node->next;
				}
			if(NULL == temp_node->next) {
				/* If we reach the end of the list, the new node gets
					appended */
				temp_node->next = new_node;
				}
			else {
				/* Otherwise, the new node gets inserted here */
				new_node->next = temp_node->next;
				temp_node->next = new_node;
				}
			}
		}
	else {
		/* The new node is "less than" the last new node.  Visit each node 
			starting at the beginning of the list until we reach the end of 
			the list (which shouldn't happen because that case was covered 
			earlier) or we find a node whose next node is is "greater than"
			the new node. */
		temp_node = list->head;
		while((NULL != temp_node->next) && (cmp(&(temp_node->next->data), 
				&(new_node->data)) <= 0)) {
			temp_node = temp_node->next;
			}
		if(temp_node == list->head) {
			/* We insert at the beginning of the list */
			new_node->next = list->head;
			list->head = new_node;
			}
		else if(NULL == temp_node->next) {
			/* If we reach the end of the list, the new node gets appended */
			temp_node->next = new_node;
			}
		else {
			/* Otherwise, the new node gets inserted here */
			new_node->next = temp_node->next;
			temp_node->next = new_node;
			}
		}
	list->last_new = new_node;

	return new_node;
	}

void au_empty_list(au_linked_list *list, void(*datafree)(void *)) {

	au_node *temp_node1;
	au_node *temp_node2;

	temp_node1 = list->head;
	while(NULL != temp_node1) {
		temp_node2 = temp_node1->next;
		if((NULL != datafree) && (NULL != temp_node1->data)) {
			datafree(temp_node1->data);
			}
		free(temp_node1);
		temp_node1 = temp_node2;
		}
	list->head = NULL;
	list->last_new = NULL;
	}

void au_free_list(au_linked_list *list, void(*datafree)(void *)) {

	if(NULL == list) return;
	if(NULL != list->label) free(list->label);
	au_empty_list(list, datafree);
	free(list);
	}
