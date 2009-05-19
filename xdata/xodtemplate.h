/*****************************************************************************
 *
 * XODTEMPLATE.H - Template-based object configuration data header file
 *
 * Copyright (c) 2001-2008 Ethan Galstad (egalstad@nagios.org)
 * Last Modified:   02-17-2008
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


#ifndef _XODTEMPLATE_H
#define _XODTEMPLATE_H



/*********** GENERAL DEFINITIONS ************/

#define XODTEMPLATE_NULL                  "null"

#define MAX_XODTEMPLATE_INPUT_BUFFER      1024

#define MAX_XODTEMPLATE_CONTACT_ADDRESSES 6

#define XODTEMPLATE_NONE                  0
#define XODTEMPLATE_TIMEPERIOD            1
#define XODTEMPLATE_COMMAND               2
#define XODTEMPLATE_CONTACT               3
#define XODTEMPLATE_CONTACTGROUP          4
#define XODTEMPLATE_HOST                  5
#define XODTEMPLATE_HOSTGROUP             6
#define XODTEMPLATE_SERVICE               7
#define XODTEMPLATE_SERVICEDEPENDENCY     8
#define XODTEMPLATE_HOSTGROUPESCALATION   9      /* no longer implemented */
#define XODTEMPLATE_SERVICEESCALATION     10
#define XODTEMPLATE_HOSTESCALATION        11
#define XODTEMPLATE_HOSTDEPENDENCY        12
#define XODTEMPLATE_HOSTEXTINFO           13
#define XODTEMPLATE_SERVICEEXTINFO        14
#define XODTEMPLATE_SERVICEGROUP          15



/***************** SKIP LISTS ****************/

#define NUM_XOBJECT_SKIPLISTS                  15

#define X_HOST_SKIPLIST                        1
#define X_SERVICE_SKIPLIST                     2
#define X_COMMAND_SKIPLIST                     3
#define X_TIMEPERIOD_SKIPLIST                  4
#define X_CONTACT_SKIPLIST                     5
#define X_CONTACTGROUP_SKIPLIST                6
#define X_HOSTGROUP_SKIPLIST                   7
#define X_SERVICEGROUP_SKIPLIST                8
#define X_HOSTDEPENDENCY_SKIPLIST              9
#define X_SERVICEDEPENDENCY_SKIPLIST           10
#define X_HOSTESCALATION_SKIPLIST              11
#define X_SERVICEESCALATION_SKIPLIST           12
#define X_HOSTEXTINFO_SKIPLIST                 13
#define X_SERVICEEXTINFO_SKIPLIST              14


/********** STRUCTURE DEFINITIONS **********/

/* CUSTOMVARIABLESMEMBER structure */
typedef struct xodtemplate_customvariablesmember_struct{
	char    *variable_name;
	char    *variable_value;
	struct xodtemplate_customvariablesmember_struct *next;
        }xodtemplate_customvariablesmember;


/* DATERANGE structure */
typedef struct xodtemplate_daterange_struct{
	int type;
	int syear;          /* start year */
	int smon;           /* start month */
	int smday;          /* start day of month (may 3rd, last day in feb) */
	int swday;          /* start day of week (thursday) */
	int swday_offset;   /* start weekday offset (3rd thursday, last monday in jan) */
	int eyear;
	int emon;
	int emday;
	int ewday;
	int ewday_offset;
	int skip_interval;
	char *timeranges;
	struct xodtemplate_daterange_struct *next;
	}xodtemplate_daterange;


/* TIMEPERIOD TEMPLATE STRUCTURE */
typedef struct xodtemplate_timeperiod_struct{
	char       *template;
	char       *name;
	int        _config_file;
	int        _start_line;

	char       *timeperiod_name;
	char       *alias;
	char       *timeranges[7];
	xodtemplate_daterange *exceptions[DATERANGE_TYPES];
	char       *exclusions;

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
	char      *contact_groups;
	char      *email;
	char      *pager;
	char      *address[MAX_XODTEMPLATE_CONTACT_ADDRESSES];
	char      *host_notification_period;
	char      *host_notification_commands;
	int       notify_on_host_down;
	int       notify_on_host_unreachable;
	int       notify_on_host_recovery;
	int       notify_on_host_flapping;
	int       notify_on_host_downtime;
	char      *service_notification_period;
	char      *service_notification_commands;
	int       notify_on_service_unknown;
	int       notify_on_service_warning;
	int       notify_on_service_critical;
	int       notify_on_service_recovery;
	int       notify_on_service_flapping;
	int       notify_on_service_downtime;
	int       host_notifications_enabled;
	int       service_notifications_enabled;
	int       can_submit_commands;
	int       retain_status_information;
	int       retain_nonstatus_information;
	xodtemplate_customvariablesmember *custom_variables;

	int       have_contact_groups;
	int       have_email;
	int       have_pager;
	int       have_address[MAX_XODTEMPLATE_CONTACT_ADDRESSES];
	int       have_host_notification_period;
	int       have_host_notification_commands;
	int       have_service_notification_period;
	int       have_service_notification_commands;

	int       have_host_notification_options;
	int       have_service_notification_options;
	int       have_host_notifications_enabled;
	int       have_service_notifications_enabled;
	int       have_can_submit_commands;
	int       have_retain_status_information;
	int       have_retain_nonstatus_information;

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
	char      *contactgroup_members;

	int       have_members;
	int       have_contactgroup_members;

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
	char      *display_name;
	char      *alias;
	char      *address;
	char      *parents;
	char      *host_groups;
	char      *check_command;
	char      *check_period;
	int       initial_state;
	double    check_interval;
	double    retry_interval;
	int       max_check_attempts;
	int       active_checks_enabled;
	int       passive_checks_enabled;
	int       obsess_over_host;
	char      *event_handler;
	int       event_handler_enabled;
	int       check_freshness;
	int       freshness_threshold;
	float     low_flap_threshold;
	float     high_flap_threshold;
	int       flap_detection_enabled;
	int       flap_detection_on_up;
	int       flap_detection_on_down;
	int       flap_detection_on_unreachable;
	char      *contact_groups;
	char      *contacts;
	int       notify_on_down;
	int       notify_on_unreachable;
	int       notify_on_recovery;
	int       notify_on_flapping;
	int       notify_on_downtime;
	int       notifications_enabled;
	char      *notification_period;
	double    notification_interval;
	double    first_notification_delay;
	int       stalk_on_up;
	int       stalk_on_down;
	int       stalk_on_unreachable;
	int       process_perf_data;
	int       failure_prediction_enabled;
	char      *failure_prediction_options;
	char      *notes;
	char      *notes_url;
	char      *action_url;
	char      *icon_image;
	char      *icon_image_alt;
	char      *vrml_image;
	char      *statusmap_image;
	int       x_2d;
	int       y_2d;
	double    x_3d;
	double    y_3d;
	double    z_3d;
	int       retain_status_information;
	int       retain_nonstatus_information;
	xodtemplate_customvariablesmember *custom_variables;

	int       have_display_name;
	int       have_parents;
	int       have_host_groups;
	int       have_check_command;
	int       have_check_period;
	int       have_event_handler;
	int       have_contact_groups;
	int       have_contacts;
	int       have_notification_period;
	int       have_failure_prediction_options;
	int       have_notes;
	int       have_notes_url;
	int       have_action_url;
	int       have_icon_image;
	int       have_icon_image_alt;
	int       have_vrml_image;
	int       have_statusmap_image;

	int       have_initial_state;
	int       have_check_interval;
	int       have_retry_interval;
	int       have_max_check_attempts;
	int       have_active_checks_enabled;
	int       have_passive_checks_enabled;
	int       have_obsess_over_host;
	int       have_event_handler_enabled;
	int       have_check_freshness;
	int       have_freshness_threshold;
	int       have_low_flap_threshold;
	int       have_high_flap_threshold;
	int       have_flap_detection_enabled;
	int       have_flap_detection_options;
	int       have_notification_options;
	int       have_notifications_enabled;
	int       have_notification_interval;
	int       have_first_notification_delay;
	int       have_stalking_options;
	int       have_process_perf_data;
	int       have_failure_prediction_enabled;
	int       have_2d_coords;
	int       have_3d_coords;
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
	char      *hostgroup_members;
	char      *notes;
	char      *notes_url;
	char      *action_url;

	int       have_members;
	int       have_hostgroup_members;
	int       have_notes;
	int       have_notes_url;
	int       have_action_url;

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

	char       *host_name;
	char       *service_description;
	char       *display_name;
	char       *hostgroup_name;
	char       *service_groups;
	char       *check_command;
	int        initial_state;
	int        max_check_attempts;
        double     check_interval;
        double     retry_interval;
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
	int        flap_detection_on_ok;
	int        flap_detection_on_warning;
	int        flap_detection_on_unknown;
	int        flap_detection_on_critical;
	int        notify_on_unknown;
	int        notify_on_warning;
	int        notify_on_critical;
	int        notify_on_recovery;
	int        notify_on_flapping;
	int        notify_on_downtime;
	int        notifications_enabled;
	char       *notification_period;
	double     notification_interval;
	double     first_notification_delay;
	char       *contact_groups;
	char       *contacts;
	int        stalk_on_ok;
	int        stalk_on_unknown;
	int        stalk_on_warning;
	int        stalk_on_critical;
	int        process_perf_data;
	int        failure_prediction_enabled;
	char       *failure_prediction_options;
	char       *notes;
	char       *notes_url;
	char       *action_url;
	char       *icon_image;
	char       *icon_image_alt;
	int        retain_status_information;
	int        retain_nonstatus_information;
	xodtemplate_customvariablesmember *custom_variables;

	int        have_host_name;
	int        have_service_description;
	int        have_display_name;
	int        have_hostgroup_name;
	int        have_service_groups;
	int        have_check_command;
	int        have_important_check_command;
	int        have_check_period;
	int        have_event_handler;
	int        have_notification_period;
	int        have_contact_groups;
	int        have_contacts;
	int        have_failure_prediction_options;
	int        have_notes;
	int        have_notes_url;
	int        have_action_url;
	int        have_icon_image;
	int        have_icon_image_alt;

	int        have_initial_state;
	int        have_max_check_attempts;
	int        have_check_interval;
	int        have_retry_interval;
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
	int        have_flap_detection_options;
	int        have_notification_options;
	int        have_notifications_enabled;
	int        have_notification_dependencies;
	int        have_notification_interval;
	int        have_first_notification_delay;
	int        have_stalking_options;
	int        have_process_perf_data;
	int        have_failure_prediction_enabled;
	int        have_retain_status_information;
	int        have_retain_nonstatus_information;
	
	int        has_been_resolved;
	int        register_object;
	struct xodtemplate_service_struct *next;
        }xodtemplate_service;


/* SERVICEGROUP TEMPLATE STRUCTURE */
typedef struct xodtemplate_servicegroup_struct{
	char      *template;
	char      *name;
	int        _config_file;
	int        _start_line;

	char      *servicegroup_name;
	char      *alias;
	char      *members;
	char      *servicegroup_members;
	char      *notes;
	char      *notes_url;
	char      *action_url;

	int       have_members;
	int       have_servicegroup_members;
	int       have_notes;
	int       have_notes_url;
	int       have_action_url;

	int       has_been_resolved;
	int       register_object;
	struct xodtemplate_servicegroup_struct *next;
        }xodtemplate_servicegroup;


/* SERVICEDEPENDENCY TEMPLATE STRUCTURE */
typedef struct xodtemplate_servicedependency_struct{
	char       *template;
        char       *name;
	int        _config_file;
	int        _start_line;

	char       *host_name;
	char       *service_description;
	char       *dependent_host_name;
	char       *dependent_service_description;
	char       *servicegroup_name;
	char       *hostgroup_name;
	char       *dependent_servicegroup_name;
	char       *dependent_hostgroup_name;
	char       *dependency_period;
	int        inherits_parent;
	int        fail_notify_on_ok;
	int        fail_notify_on_unknown;
	int        fail_notify_on_warning;
	int        fail_notify_on_critical;
	int        fail_notify_on_pending;
	int        fail_execute_on_ok;
	int        fail_execute_on_unknown;
	int        fail_execute_on_warning;
	int        fail_execute_on_critical;
	int        fail_execute_on_pending;

	int        have_host_name;
	int        have_service_description;
	int        have_dependent_host_name;
	int        have_dependent_service_description;
	int        have_servicegroup_name;
	int        have_hostgroup_name;
	int        have_dependent_servicegroup_name;
	int        have_dependent_hostgroup_name;
	int        have_dependency_period;

	int        have_inherits_parent;
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

	char      *host_name;
	char      *service_description;
	char      *servicegroup_name;
	char      *hostgroup_name;
	int       first_notification;
	int       last_notification;
	double    notification_interval;
	char      *escalation_period;
	int       escalate_on_warning;
	int       escalate_on_unknown;
	int       escalate_on_critical;
	int       escalate_on_recovery;
	char      *contact_groups;
	char      *contacts;

	int       have_host_name;
	int       have_service_description;
	int       have_servicegroup_name;
	int       have_hostgroup_name;
	int       have_escalation_period;
	int       have_contact_groups;
	int       have_contacts;

	int       have_first_notification;
	int       have_last_notification;
	int       have_notification_interval;
	int       have_escalation_options;

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

	char      *host_name;
	char      *dependent_host_name;
	char      *hostgroup_name;
	char      *dependent_hostgroup_name;
	char      *dependency_period;
	int       inherits_parent;
	int       fail_notify_on_up;
	int       fail_notify_on_down;
	int       fail_notify_on_unreachable;
	int       fail_notify_on_pending;
	int       fail_execute_on_up;
	int       fail_execute_on_down;
	int       fail_execute_on_unreachable;
	int       fail_execute_on_pending;

	int       have_host_name;
	int       have_dependent_host_name;
	int       have_hostgroup_name;
	int       have_dependent_hostgroup_name;
	int       have_dependency_period;

	int       have_inherits_parent;
	int       have_notification_dependency_options;
	int       have_execution_dependency_options;

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

	char      *host_name;
	char      *hostgroup_name;
	int       first_notification;
	int       last_notification;
	double    notification_interval;
	char      *escalation_period;
	int       escalate_on_down;
	int       escalate_on_unreachable;
	int       escalate_on_recovery;
	char      *contact_groups;
	char      *contacts;

	int       have_host_name;
	int       have_hostgroup_name;
	int       have_escalation_period;
	int       have_contact_groups;
	int       have_contacts;

	int       have_first_notification;
	int       have_last_notification;
	int       have_notification_interval;
	int       have_escalation_options;

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
	char       *notes;
	char       *notes_url;
	char       *action_url;
	char       *icon_image;
	char       *icon_image_alt;
	char       *vrml_image;
	char       *statusmap_image;
	int        x_2d;
	int        y_2d;
	double     x_3d;
	double     y_3d;
	double     z_3d;

	int        have_host_name;
	int        have_hostgroup_name;
	int        have_notes;
	int        have_notes_url;
	int        have_action_url;
	int        have_icon_image;
	int        have_icon_image_alt;
	int        have_vrml_image;
	int        have_statusmap_image;
	
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
	char       *notes;
	char       *notes_url;
	char       *action_url;
	char       *icon_image;
	char       *icon_image_alt;

	int        have_host_name;
	int        have_hostgroup_name;
	int        have_service_description;
	int        have_notes;
	int        have_notes_url;
	int        have_action_url;
	int        have_icon_image;
	int        have_icon_image_alt;

	int        has_been_resolved;
	int        register_object;
	struct xodtemplate_serviceextinfo_struct *next;
        }xodtemplate_serviceextinfo;


/* CONTACT LIST STRUCTURE */
typedef struct xodtemplate_contactlist_struct{
	char      *contact_name;
	struct xodtemplate_contactlist_struct *next;
        }xodtemplate_contactlist;


/* HOST LIST STRUCTURE */
typedef struct xodtemplate_hostlist_struct{
	char      *host_name;
	struct xodtemplate_hostlist_struct *next;
        }xodtemplate_hostlist;


/* SERVICE LIST STRUCTURE */
typedef struct xodtemplate_servicelist_struct{
	char      *host_name;
	char      *service_description;
	struct xodtemplate_servicelist_struct *next;
        }xodtemplate_servicelist;


/* MEMBER LIST STRUCTURE */
typedef struct xodtemplate_memberlist_struct{
	char      *name1;
	char      *name2;
	struct xodtemplate_memberlist_struct *next;
        }xodtemplate_memberlist;


/***** CHAINED HASH DATA STRUCTURES ******/

typedef struct xodtemplate_service_cursor_struct{
	int xodtemplate_service_iterator;
	xodtemplate_service *current_xodtemplate_service;
        }xodtemplate_service_cursor;



/********* FUNCTION DEFINITIONS **********/

int xodtemplate_read_config_data(char *,int,int,int);       /* top-level routine processes all config files */
int xodtemplate_grab_config_info(char *);                   /* grabs variables from main config file */
int xodtemplate_process_config_file(char *,int);            /* process data in a specific config file */
int xodtemplate_process_config_dir(char *,int);             /* process all files in a specific config directory */

#ifdef NSCORE
char *xodtemplate_config_file_name(int);                    /* returns the name of a numbered config file */

xodtemplate_memberlist *xodtemplate_expand_contactgroups_and_contacts(char *,char *,int,int);
int xodtemplate_expand_contactgroups(xodtemplate_memberlist **,xodtemplate_memberlist **,char *,int,int);
int xodtemplate_expand_contacts(xodtemplate_memberlist **,xodtemplate_memberlist **,char *,int,int);
int xodtemplate_add_contactgroup_members_to_memberlist(xodtemplate_memberlist **,xodtemplate_contactgroup *,int,int);

xodtemplate_memberlist *xodtemplate_expand_hostgroups_and_hosts(char *,char *,int,int);
int xodtemplate_expand_hostgroups(xodtemplate_memberlist **,xodtemplate_memberlist **,char *,int,int);
int xodtemplate_expand_hosts(xodtemplate_memberlist **,xodtemplate_memberlist **,char *,int,int);
int xodtemplate_add_hostgroup_members_to_memberlist(xodtemplate_memberlist **,xodtemplate_hostgroup *,int,int);

xodtemplate_memberlist *xodtemplate_expand_servicegroups_and_services(char *,char *,char *,int,int);
int xodtemplate_expand_servicegroups(xodtemplate_memberlist **,xodtemplate_memberlist **,char *,int,int);
int xodtemplate_expand_services(xodtemplate_memberlist **,xodtemplate_memberlist **,char *,char *,int,int);
int xodtemplate_add_servicegroup_members_to_memberlist(xodtemplate_memberlist **,xodtemplate_servicegroup *,int,int);

char *xodtemplate_process_contactgroup_names(char *,int,int);
int xodtemplate_get_contactgroup_names(xodtemplate_memberlist **,xodtemplate_memberlist **,char *,int,int);

char *xodtemplate_process_hostgroup_names(char *,int,int);
int xodtemplate_get_hostgroup_names(xodtemplate_memberlist **,xodtemplate_memberlist **,char *,int,int);

char *xodtemplate_process_servicegroup_names(char *,int,int);
int xodtemplate_get_servicegroup_names(xodtemplate_memberlist **,xodtemplate_memberlist **,char *,int,int);

int xodtemplate_add_member_to_memberlist(xodtemplate_memberlist **,char *,char *);
int xodtemplate_free_memberlist(xodtemplate_memberlist **);
void xodtemplate_remove_memberlist_item(xodtemplate_memberlist *,xodtemplate_memberlist **);
#endif


int xodtemplate_begin_object_definition(char *,int,int,int);
int xodtemplate_add_object_property(char *,int);
int xodtemplate_end_object_definition(int);

int xodtemplate_parse_timeperiod_directive(xodtemplate_timeperiod *,char *,char *);
xodtemplate_daterange *xodtemplate_add_exception_to_timeperiod(xodtemplate_timeperiod *,int,int,int,int,int,int,int,int,int,int,int,int,char *);
int xodtemplate_get_month_from_string(char *,int *);
int xodtemplate_get_weekday_from_string(char *,int *);

xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_host(xodtemplate_host *,char *,char *);
xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_service(xodtemplate_service *,char *,char *);
xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_contact(xodtemplate_contact *,char *,char *);
xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_object(xodtemplate_customvariablesmember **,char *,char *);


int xodtemplate_register_objects(void);
int xodtemplate_free_memory(void);

#ifdef NSCORE
int xodtemplate_duplicate_objects(void);
int xodtemplate_duplicate_services(void);

int xodtemplate_inherit_object_properties(void);

int xodtemplate_resolve_objects(void);

int xodtemplate_sort_objects(void);
int xodtemplate_compare_strings1(char *,char *);
int xodtemplate_compare_strings2(char *,char *,char *,char *);

int xodtemplate_cache_objects(char *);

int xodtemplate_duplicate_service(xodtemplate_service *,char *);
int xodtemplate_duplicate_hostescalation(xodtemplate_hostescalation *,char *);
int xodtemplate_duplicate_serviceescalation(xodtemplate_serviceescalation *,char *,char *);
int xodtemplate_duplicate_hostdependency(xodtemplate_hostdependency *,char *,char *);
int xodtemplate_duplicate_servicedependency(xodtemplate_servicedependency *,char *,char *,char *, char *,char *,char *,char *,char *);
int xodtemplate_duplicate_hostextinfo(xodtemplate_hostextinfo *,char *);
int xodtemplate_duplicate_serviceextinfo(xodtemplate_serviceextinfo *,char *);

int xodtemplate_recombobulate_contactgroups(void);
int xodtemplate_recombobulate_contactgroup_subgroups(xodtemplate_contactgroup *,char **);
int xodtemplate_recombobulate_object_contacts(void);
int xodtemplate_recombobulate_hostgroups(void);
int xodtemplate_recombobulate_hostgroup_subgroups(xodtemplate_hostgroup *, char **);
int xodtemplate_recombobulate_servicegroups(void);
int xodtemplate_recombobulate_servicegroup_subgroups(xodtemplate_servicegroup *,char **);

int xodtemplate_resolve_timeperiod(xodtemplate_timeperiod *);
int xodtemplate_resolve_command(xodtemplate_command *);
int xodtemplate_resolve_contactgroup(xodtemplate_contactgroup *);
int xodtemplate_resolve_hostgroup(xodtemplate_hostgroup *);
int xodtemplate_resolve_servicegroup(xodtemplate_servicegroup *);
int xodtemplate_resolve_servicedependency(xodtemplate_servicedependency *);
int xodtemplate_resolve_serviceescalation(xodtemplate_serviceescalation *);
int xodtemplate_resolve_contact(xodtemplate_contact *);
int xodtemplate_resolve_host(xodtemplate_host *);
int xodtemplate_resolve_service(xodtemplate_service *);
int xodtemplate_resolve_hostdependency(xodtemplate_hostdependency *);
int xodtemplate_resolve_hostescalation(xodtemplate_hostescalation *);
int xodtemplate_resolve_hostextinfo(xodtemplate_hostextinfo *);
int xodtemplate_resolve_serviceextinfo(xodtemplate_serviceextinfo *);

int xodtemplate_sort_timeperiods(void);
int xodtemplate_sort_commands(void);
int xodtemplate_sort_contactgroups(void);
int xodtemplate_sort_hostgroups(void);
int xodtemplate_sort_servicegroups(void);
int xodtemplate_sort_contacts(void);
int xodtemplate_sort_hosts(void);
int xodtemplate_sort_services(void);
int xodtemplate_sort_servicedependencies(void);
int xodtemplate_sort_serviceescalations(void);
int xodtemplate_sort_hostdependencies(void);
int xodtemplate_sort_hostescalations(void);

int xodtemplate_merge_extinfo_ojects(void);
int xodtemplate_merge_host_extinfo_object(xodtemplate_host *,xodtemplate_hostextinfo *);
int xodtemplate_merge_service_extinfo_object(xodtemplate_service *,xodtemplate_serviceextinfo *);

xodtemplate_timeperiod *xodtemplate_find_timeperiod(char *);
xodtemplate_command *xodtemplate_find_command(char *);
xodtemplate_contactgroup *xodtemplate_find_contactgroup(char *);
xodtemplate_contactgroup *xodtemplate_find_real_contactgroup(char *);
xodtemplate_hostgroup *xodtemplate_find_hostgroup(char *);
xodtemplate_hostgroup *xodtemplate_find_real_hostgroup(char *);
xodtemplate_servicegroup *xodtemplate_find_servicegroup(char *);
xodtemplate_servicegroup *xodtemplate_find_real_servicegroup(char *);
xodtemplate_servicedependency *xodtemplate_find_servicedependency(char *);
xodtemplate_serviceescalation *xodtemplate_find_serviceescalation(char *);
xodtemplate_contact *xodtemplate_find_contact(char *);
xodtemplate_contact *xodtemplate_find_real_contact(char *);
xodtemplate_host *xodtemplate_find_host(char *);
xodtemplate_host *xodtemplate_find_real_host(char *);
xodtemplate_service *xodtemplate_find_service(char *);
xodtemplate_service *xodtemplate_find_real_service(char *,char *);
xodtemplate_hostdependency *xodtemplate_find_hostdependency(char *);
xodtemplate_hostescalation *xodtemplate_find_hostescalation(char *);
xodtemplate_hostextinfo *xodtemplate_find_hostextinfo(char *);
xodtemplate_serviceextinfo *xodtemplate_find_serviceextinfo(char *);

int xodtemplate_get_inherited_string(int *,char **,int *,char **);
int xodtemplate_clean_additive_string(char **);
int xodtemplate_clean_additive_strings(void);
#endif

int xodtemplate_register_timeperiod(xodtemplate_timeperiod *);
int xodtemplate_get_time_ranges(char *,unsigned long *,unsigned long *);
int xodtemplate_register_command(xodtemplate_command *);
int xodtemplate_register_contactgroup(xodtemplate_contactgroup *);
int xodtemplate_register_hostgroup(xodtemplate_hostgroup *);
int xodtemplate_register_servicegroup(xodtemplate_servicegroup *);
int xodtemplate_register_servicedependency(xodtemplate_servicedependency *);
int xodtemplate_register_serviceescalation(xodtemplate_serviceescalation *);
int xodtemplate_register_contact(xodtemplate_contact *);
int xodtemplate_register_host(xodtemplate_host *);
int xodtemplate_register_service(xodtemplate_service *);
int xodtemplate_register_hostdependency(xodtemplate_hostdependency *);
int xodtemplate_register_hostescalation(xodtemplate_hostescalation *);


int xodtemplate_init_xobject_skiplists(void);
int xodtemplate_free_xobject_skiplists(void);

int xodtemplate_skiplist_compare_text(const char *val1a, const char *val1b, const char *val2a, const char *val2b);
int xodtemplate_skiplist_compare_host_template(void *a, void *b);
int xodtemplate_skiplist_compare_service_template(void *a, void *b);
int xodtemplate_skiplist_compare_command_template(void *a, void *b);
int xodtemplate_skiplist_compare_timeperiod_template(void *a, void *b);
int xodtemplate_skiplist_compare_contact_template(void *a, void *b);
int xodtemplate_skiplist_compare_contactgroup_template(void *a, void *b);
int xodtemplate_skiplist_compare_hostgroup_template(void *a, void *b);
int xodtemplate_skiplist_compare_servicegroup_template(void *a, void *b);
int xodtemplate_skiplist_compare_hostdependency_template(void *a, void *b);
int xodtemplate_skiplist_compare_servicedependency_template(void *a, void *b);
int xodtemplate_skiplist_compare_hostescalation_template(void *a, void *b);
int xodtemplate_skiplist_compare_serviceescalation_template(void *a, void *b);
int xodtemplate_skiplist_compare_hostextinfo_template(void *a, void *b);
int xodtemplate_skiplist_compare_serviceextinfo_template(void *a, void *b);

int xodtemplate_skiplist_compare_host(void *a, void *b);
int xodtemplate_skiplist_compare_service(void *a, void *b);
int xodtemplate_skiplist_compare_contact(void *a, void *b);
int xodtemplate_skiplist_compare_contactgroup(void *a, void *b);
int xodtemplate_skiplist_compare_hostgroup(void *a, void *b);
int xodtemplate_skiplist_compare_servicegroup(void *a, void *b);
int xodtemplate_skiplist_compare_command(void *a, void *b);
int xodtemplate_skiplist_compare_timeperiod(void *a, void *b);
int xodtemplate_skiplist_compare_hostdependency(void *a, void *b);
int xodtemplate_skiplist_compare_servicedependency(void *a, void *b);
int xodtemplate_skiplist_compare_hostescalation(void *a, void *b);
int xodtemplate_skiplist_compare_serviceescalation(void *a, void *b);


#endif


