/*****************************************************************************
 *
 * XODTEMPLATE.H - Template-based object configuration data header file
 *
 * Copyright (c) 2001-2003 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   02-12-2003
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


#ifndef _XODTEMPLATE_H
#define _XODTEMPLATE_H



/*********** GENERAL DEFINITIONS ************/

#define MAX_XODTEMPLATE_INPUT_BUFFER    8196

#define XODTEMPLATE_NONE                0
#define XODTEMPLATE_TIMEPERIOD          1
#define XODTEMPLATE_COMMAND             2
#define XODTEMPLATE_CONTACT             3
#define XODTEMPLATE_CONTACTGROUP        4
#define XODTEMPLATE_HOST                5
#define XODTEMPLATE_HOSTGROUP           6
#define XODTEMPLATE_SERVICE             7
#define XODTEMPLATE_SERVICEDEPENDENCY   8
#define XODTEMPLATE_HOSTGROUPESCALATION 9      /* no longer implemented */
#define XODTEMPLATE_SERVICEESCALATION   10
#define XODTEMPLATE_HOSTESCALATION      11
#define XODTEMPLATE_HOSTDEPENDENCY      12
#define XODTEMPLATE_HOSTEXTINFO         13
#define XODTEMPLATE_SERVICEEXTINFO      14



/********** STRUCTURE DEFINITIONS **********/

/* TIMEPERIOD TEMPLATE STRUCTURE */
typedef struct xodtemplate_timeperiod_struct{
	char       *template;
	char       *name;
	int        _config_file;
	int        _start_line;

	char       *timeperiod_name;
	char       *alias;
	char       *timeranges[7];

	int        has_been_resolved;
	int        register_object;
	struct xodtemplate_timeperiod_struct *next;
        }xodtemplate_timeperiod;


/* COMMAND TEMPLATE STRUCTURE */
typedef struct xodtemplate_command_struct{
	char       *template;
	char       *name;
	int        _config_file;
	int        _start_line;

	char       *command_name;
	char       *command_line;

	int        has_been_resolved;
	int        register_object;
	struct xodtemplate_command_struct *next;
        }xodtemplate_command;


/* CONTACT TEMPLATE STRUCTURE */
typedef struct xodtemplate_contact_struct{
	char      *template;
	char      *name;
	int        _config_file;
	int        _start_line;

	char      *contact_name;
	char      *alias;
	char      *email;
	char      *pager;
	char      *host_notification_period;
	char      *host_notification_commands;
	int       notify_on_host_down;
	int       notify_on_host_unreachable;
	int       notify_on_host_recovery;
	char      *service_notification_period;
	char      *service_notification_commands;
	int       notify_on_service_unknown;
	int       notify_on_service_warning;
	int       notify_on_service_critical;
	int       notify_on_service_recovery;

	int       have_host_notification_options;
	int       have_service_notification_options;

	int       has_been_resolved;
	int       register_object;
	struct xodtemplate_contact_struct *next;
        }xodtemplate_contact;


/* CONTACTGROUP TEMPLATE STRUCTURE */
typedef struct xodtemplate_contactgroup_struct{
	char      *template;
	char      *name;
	int        _config_file;
	int        _start_line;

	char      *contactgroup_name;
	char      *alias;
        char      *members;

	int       has_been_resolved;
	int       register_object;
	struct xodtemplate_contactgroup_struct *next;
        }xodtemplate_contactgroup;


/* HOST TEMPLATE STRUCTURE */
typedef struct xodtemplate_host_struct{
	char      *template;
	char      *name;
	int        _config_file;
	int        _start_line;

	char      *host_name;
	char      *alias;
	char      *address;
	char      *parents;
	char      *hostgroups;
	char      *check_command;
	int       max_check_attempts;
	int       active_checks_enabled;
	int       passive_checks_enabled;
	int       obsess_over_host;
	char      *event_handler;
	int       event_handler_enabled;
	float     low_flap_threshold;
	float     high_flap_threshold;
	int       flap_detection_enabled;
	char      *contact_groups;
	int       notify_on_down;
	int       notify_on_unreachable;
	int       notify_on_recovery;
	int       notifications_enabled;
	char      *notification_period;
	int       notification_interval;
	int       stalk_on_up;
	int       stalk_on_down;
	int       stalk_on_unreachable;
	int       process_perf_data;
	int       failure_prediction_enabled;
	char      *failure_prediction_options;
	int       retain_status_information;
	int       retain_nonstatus_information;

	int       have_max_check_attempts;
	int       have_active_checks_enabled;
	int       have_passive_checks_enabled;
	int       have_obsess_over_host;
	int       have_event_handler_enabled;
	int       have_low_flap_threshold;
	int       have_high_flap_threshold;
	int       have_flap_detection_enabled;
	int       have_notification_options;
	int       have_notifications_enabled;
	int       have_notification_interval;
	int       have_stalking_options;
	int       have_process_perf_data;
	int       have_failure_prediction_enabled;
	int       have_retain_status_information;
	int       have_retain_nonstatus_information;

	int       has_been_resolved;
	int       register_object;
	struct xodtemplate_host_struct *next;
        }xodtemplate_host;


/* HOSTGROUP TEMPLATE STRUCTURE */
typedef struct xodtemplate_hostgroup_struct{
	char      *template;
	char      *name;
	int        _config_file;
	int        _start_line;

	char      *hostgroup_name;
	char      *alias;
	char      *members;

	int       has_been_resolved;
	int       register_object;
	struct xodtemplate_hostgroup_struct *next;
        }xodtemplate_hostgroup;


/* SERVICE TEMPLATE STRUCTURE */
typedef struct xodtemplate_service_struct{
        char       *template;
	char       *name;
	int        _config_file;
	int        _start_line;

	char       *hostgroup_name;
	char       *host_name;
	char       *service_description;
	char       *check_command;
	int        max_check_attempts;
        int        normal_check_interval;
        int        retry_check_interval;
        char       *check_period;
        int        active_checks_enabled;
        int        passive_checks_enabled;
        int        parallelize_check;
	int        is_volatile;
	int        obsess_over_service;
	char       *event_handler;
	int        event_handler_enabled;
	int        check_freshness;
	int        freshness_threshold;
	double     low_flap_threshold;
	double     high_flap_threshold;
	int        flap_detection_enabled;
	int        notify_on_unknown;
	int        notify_on_warning;
	int        notify_on_critical;
	int        notify_on_recovery;
	int        notifications_enabled;
	char       *notification_period;
	int        notification_interval;
	char       *contact_groups;
	int        stalk_on_ok;
	int        stalk_on_unknown;
	int        stalk_on_warning;
	int        stalk_on_critical;
	int        process_perf_data;
	int        failure_prediction_enabled;
	char       *failure_prediction_options;
	int        retain_status_information;
	int        retain_nonstatus_information;

	int        have_max_check_attempts;
	int        have_normal_check_interval;
	int        have_retry_check_interval;
        int        have_active_checks_enabled;
        int        have_passive_checks_enabled;
        int        have_parallelize_check;
	int        have_is_volatile;
	int        have_obsess_over_service;
	int        have_event_handler_enabled;
	int        have_check_freshness;
	int        have_freshness_threshold;
	int        have_low_flap_threshold;
	int        have_high_flap_threshold;
	int        have_flap_detection_enabled;
	int        have_notification_options;
	int        have_notifications_enabled;
	int        have_notification_dependencies;
	int        have_notification_interval;
	int        have_stalking_options;
	int        have_process_perf_data;
	int        have_failure_prediction_enabled;
	int        have_retain_status_information;
	int        have_retain_nonstatus_information;
	
	int        has_been_resolved;
	int        register_object;
	struct xodtemplate_service_struct *next;
        }xodtemplate_service;


/* SERVICEDEPENDENCY TEMPLATE STRUCTURE */
typedef struct xodtemplate_servicedependency_struct{
	char       *template;
        char       *name;
	int        _config_file;
	int        _start_line;

	char       *hostgroup_name;
	char       *host_name;
	char       *service_description;
	char       *dependent_hostgroup_name;
	char       *dependent_host_name;
	char       *dependent_service_description;
	int        fail_notify_on_ok;
	int        fail_notify_on_unknown;
	int        fail_notify_on_warning;
	int        fail_notify_on_critical;
	int        fail_execute_on_ok;
	int        fail_execute_on_unknown;
	int        fail_execute_on_warning;
	int        fail_execute_on_critical;

	int        have_notification_dependency_options;
	int        have_execution_dependency_options;

	int        has_been_resolved;
	int        register_object;
	struct xodtemplate_servicedependency_struct *next;
        }xodtemplate_servicedependency;


/* SERVICEESCALATION TEMPLATE STRUCTURE */
typedef struct xodtemplate_serviceescalation_struct{
	char      *template;
	char      *name;
	int        _config_file;
	int        _start_line;

	char      *hostgroup_name;
	char      *host_name;
	char      *service_description;
	int       first_notification;
	int       last_notification;
	int       notification_interval;
	char      *contact_groups;

	int       have_first_notification;
	int       have_last_notification;
	int       have_notification_interval;

	int       has_been_resolved;
	int       register_object;
	struct xodtemplate_serviceescalation_struct *next;
        }xodtemplate_serviceescalation;


/* HOSTDEPENDENCY TEMPLATE STRUCTURE */
typedef struct xodtemplate_hostdependency_struct{
	char      *template;
        char      *name;
	int        _config_file;
	int        _start_line;

	char      *hostgroup_name;
	char      *dependent_hostgroup_name;
	char      *host_name;
	char      *dependent_host_name;
	int       fail_notify_on_up;
	int       fail_notify_on_down;
	int       fail_notify_on_unreachable;

	int       have_notification_dependency_options;

	int       has_been_resolved;
	int       register_object;
	struct xodtemplate_hostdependency_struct *next;
        }xodtemplate_hostdependency;


/* HOSTESCALATION TEMPLATE STRUCTURE */
typedef struct xodtemplate_hostescalation_struct{
	char      *template;
	char      *name;
	int        _config_file;
	int        _start_line;

	char      *hostgroup_name;
	char      *host_name;
	int       first_notification;
	int       last_notification;
	int       notification_interval;
	char      *contact_groups;

	int       have_first_notification;
	int       have_last_notification;
	int       have_notification_interval;

	int       has_been_resolved;
	int       register_object;
	struct xodtemplate_hostescalation_struct *next;
        }xodtemplate_hostescalation;


/* HOSTEXTINFO TEMPLATE STRUCTURE */
typedef struct xodtemplate_hostextinfo_struct{
	char       *template;
	char       *name;
	int        _config_file;
	int        _start_line;

	char       *host_name;
	char       *hostgroup_name;
	char       *notes_url;
	char       *icon_image;
	char       *icon_image_alt;
	char       *vrml_image;
	char       *statusmap_image;
	int        x_2d;
	int        y_2d;
	double     x_3d;
	double     y_3d;
	double     z_3d;
	
	int        have_2d_coords;
	int        have_3d_coords;

	int        has_been_resolved;
	int        register_object;
	struct xodtemplate_hostextinfo_struct *next;
        }xodtemplate_hostextinfo;


/* SERVICEEXTINFO TEMPLATE STRUCTURE */
typedef struct xodtemplate_serviceextinfo_struct{
	char       *template;
	char       *name;
	int        _config_file;
	int        _start_line;

	char       *host_name;
	char       *hostgroup_name;
	char       *service_description;
	char       *notes_url;
	char       *icon_image;
	char       *icon_image_alt;

	int        has_been_resolved;
	int        register_object;
	struct xodtemplate_serviceextinfo_struct *next;
        }xodtemplate_serviceextinfo;


/* HOST LIST STRUCTURE */
typedef struct xodtemplate_hostlist_struct{
	char      *host_name;
	struct xodtemplate_hostlist_struct *next;
        }xodtemplate_hostlist;


/* SERVICE LIST STRUCTURE */
typedef struct xodtemplate_servicelist_struct{
	char      *service_description;
	struct xodtemplate_servicelist_struct *next;
        }xodtemplate_servicelist;



/***** CHAINED HASH DATA STRUCTURES ******/

typedef struct xodtemplate_service_cursor_struct{
	int xodtemplate_service_iterator;
	xodtemplate_service *current_xodtemplate_service;
        }xodtemplate_service_cursor;



/********* FUNCTION DEFINITIONS **********/

int xodtemplate_read_config_data(char *,int,int);           /* top-level routine processes all config files */
int xodtemplate_grab_config_info(char *);                   /* grabs variables from main config file */
int xodtemplate_process_config_file(char *,int);            /* process data in a specific config file */
int xodtemplate_process_config_dir(char *,int);             /* process all files in a specific config directory */
char *xodtemplate_config_file_name(int);                    /* returns the name of a numbered config file */

void xodtemplate_strip(char *);

xodtemplate_hostlist *xodtemplate_expand_hostgroups_and_hosts(char *,char *);
xodtemplate_servicelist *xodtemplate_expand_services(char *,char *);
int xodtemplate_free_hostlist(xodtemplate_hostlist *);
int xodtemplate_free_servicelist(xodtemplate_servicelist *);

int xodtemplate_begin_object_definition(char *,int,int,int);
int xodtemplate_add_object_property(char *,int);
int xodtemplate_end_object_definition(int);

int xodtemplate_duplicate_objects(void);
int xodtemplate_resolve_objects(void);
int xodtemplate_recombobulate_objects(void);
int xodtemplate_register_objects(void);
int xodtemplate_free_memory(void);

int xodtemplate_duplicate_service(xodtemplate_service *,char *);
int xodtemplate_duplicate_hostescalation(xodtemplate_hostescalation *,char *);
int xodtemplate_duplicate_serviceescalation(xodtemplate_serviceescalation *,char *,char *);
int xodtemplate_duplicate_hostdependency(xodtemplate_hostdependency *,char *,char *);
int xodtemplate_duplicate_servicedependency(xodtemplate_servicedependency *,char *,char *,char *,char *);
int xodtemplate_duplicate_hostextinfo(xodtemplate_hostextinfo *,char *);
int xodtemplate_duplicate_serviceextinfo(xodtemplate_serviceextinfo *,char *);

int xodtemplate_resolve_timeperiod(xodtemplate_timeperiod *);
int xodtemplate_resolve_command(xodtemplate_command *);
int xodtemplate_resolve_contactgroup(xodtemplate_contactgroup *);
int xodtemplate_resolve_hostgroup(xodtemplate_hostgroup *);
int xodtemplate_resolve_servicedependency(xodtemplate_servicedependency *);
int xodtemplate_resolve_serviceescalation(xodtemplate_serviceescalation *);
int xodtemplate_resolve_contact(xodtemplate_contact *);
int xodtemplate_resolve_host(xodtemplate_host *);
int xodtemplate_resolve_service(xodtemplate_service *);
int xodtemplate_resolve_hostdependency(xodtemplate_hostdependency *);
int xodtemplate_resolve_hostescalation(xodtemplate_hostescalation *);
int xodtemplate_resolve_hostextinfo(xodtemplate_hostextinfo *);
int xodtemplate_resolve_serviceextinfo(xodtemplate_serviceextinfo *);

xodtemplate_timeperiod *xodtemplate_find_timeperiod(char *);
xodtemplate_command *xodtemplate_find_command(char *);
xodtemplate_contactgroup *xodtemplate_find_contactgroup(char *);
xodtemplate_hostgroup *xodtemplate_find_hostgroup(char *);
xodtemplate_hostgroup *xodtemplate_find_real_hostgroup(char *);
xodtemplate_servicedependency *xodtemplate_find_servicedependency(char *);
xodtemplate_serviceescalation *xodtemplate_find_serviceescalation(char *);
xodtemplate_contact *xodtemplate_find_contact(char *);
xodtemplate_host *xodtemplate_find_host(char *);
xodtemplate_host *xodtemplate_find_real_host(char *);
xodtemplate_service *xodtemplate_find_service(char *);
xodtemplate_service *xodtemplate_find_real_service(char *,char *);
xodtemplate_hostdependency *xodtemplate_find_hostdependency(char *);
xodtemplate_hostescalation *xodtemplate_find_hostescalation(char *);
xodtemplate_hostextinfo *xodtemplate_find_hostextinfo(char *);
xodtemplate_serviceextinfo *xodtemplate_find_serviceextinfo(char *);

void *get_xodtemplate_service_cursor(void);
xodtemplate_service *get_next_xodtemplate_service(void *v_cursor);
void free_xodtemplate_service_cursor(void *cursor);
int xodtemplate_rename_service(xodtemplate_service *,const char *);
int compare_xodtemplate_service(xodtemplate_service *,const char *);
int xodtemplate_service_comes_after(xodtemplate_service *, const char *);
int xodtemplate_add_service_allocated(xodtemplate_service *);
int xodtemplate_remove_pointer(xodtemplate_service *);

int xodtemplate_register_timeperiod(xodtemplate_timeperiod *);
int xodtemplate_register_command(xodtemplate_command *);
int xodtemplate_register_contactgroup(xodtemplate_contactgroup *);
int xodtemplate_register_hostgroup(xodtemplate_hostgroup *);
int xodtemplate_register_servicedependency(xodtemplate_servicedependency *);
int xodtemplate_register_serviceescalation(xodtemplate_serviceescalation *);
int xodtemplate_register_contact(xodtemplate_contact *);
int xodtemplate_register_host(xodtemplate_host *);
int xodtemplate_register_service(xodtemplate_service *);
int xodtemplate_register_hostdependency(xodtemplate_hostdependency *);
int xodtemplate_register_hostescalation(xodtemplate_hostescalation *);
int xodtemplate_register_hostextinfo(xodtemplate_hostextinfo *);
int xodtemplate_register_serviceextinfo(xodtemplate_serviceextinfo *);


#endif


