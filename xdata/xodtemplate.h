/*****************************************************************************
 *
 * XODTEMPLATE.H - Template-based object configuration data header file
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
#define XODTEMPLATE_SERVICEESCALATION     9
#define XODTEMPLATE_HOSTESCALATION        10
#define XODTEMPLATE_HOSTDEPENDENCY        11
#define XODTEMPLATE_HOSTEXTINFO           12
#define XODTEMPLATE_SERVICEEXTINFO        13
#define XODTEMPLATE_SERVICEGROUP          14



/***************** SKIP LISTS ****************/

#define HOSTEXTINFO_SKIPLIST                 (NUM_OBJECT_SKIPLISTS)
#define SERVICEEXTINFO_SKIPLIST              (NUM_OBJECT_SKIPLISTS + 1)
#define NUM_XOBJECT_SKIPLISTS                (NUM_OBJECT_SKIPLISTS + 2)


/********** STRUCTURE DEFINITIONS **********/

/* CUSTOMVARIABLESMEMBER structure */
typedef struct xodtemplate_customvariablesmember_struct {
	char    *variable_name;
	char    *variable_value;
	struct xodtemplate_customvariablesmember_struct *next;
	} xodtemplate_customvariablesmember;


/* DATERANGE structure */
typedef struct xodtemplate_daterange_struct {
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
	} xodtemplate_daterange;


/* TIMEPERIOD TEMPLATE STRUCTURE */
typedef struct xodtemplate_timeperiod_struct {
	char       *template;
	char       *name;
	int        _config_file;
	int        _start_line;

	char       *timeperiod_name;
	char       *alias;
	char       *timeranges[7];
	xodtemplate_daterange *exceptions[DATERANGE_TYPES];
	char       *exclusions;

	unsigned has_been_resolved : 1;
	unsigned register_object : 1;
	struct xodtemplate_timeperiod_struct *next;
	} xodtemplate_timeperiod;


/* COMMAND TEMPLATE STRUCTURE */
typedef struct xodtemplate_command_struct {
    char       *template;
    char       *name;
    int        _config_file;
    int        _start_line;

    char       *command_name;
    char       *command_line;

    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_command_struct *next;
} xodtemplate_command;


/* CONTACT TEMPLATE STRUCTURE */
typedef struct xodtemplate_contact_struct {
	unsigned int id;
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
    int       host_notification_options;
    char      *service_notification_period;
    char      *service_notification_commands;
	int       service_notification_options;
    int       host_notifications_enabled;
    int       service_notifications_enabled;
    int       can_submit_commands;
    int       retain_status_information;
    int       retain_nonstatus_information;
    unsigned int minimum_value;
    xodtemplate_customvariablesmember *custom_variables;

    char have_contact_groups;
    char have_email;
    char have_pager;
    char have_address[MAX_XODTEMPLATE_CONTACT_ADDRESSES];
    char have_host_notification_period;
    char have_host_notification_commands;
    char have_service_notification_period;
    char have_service_notification_commands;

    char have_host_notification_options;
    char have_service_notification_options;
    char have_host_notifications_enabled;
    char have_service_notifications_enabled;
    char have_can_submit_commands;
    char have_retain_status_information;
    char have_retain_nonstatus_information;
    unsigned have_minimum_value : 1;

    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_contact_struct *next;
} xodtemplate_contact;


/* CONTACTGROUP TEMPLATE STRUCTURE */
typedef struct xodtemplate_contactgroup_struct {
    char      *template;
    char      *name;
    int        _config_file;
    int        _start_line;

    char      *contactgroup_name;
    char      *alias;
    char      *members;
    char      *contactgroup_members;
	objectlist *member_list;
	objectlist *group_list;
	bitmap *member_map;
	bitmap *reject_map;
	int loop_status;

    char have_members;
    char have_contactgroup_members;

    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_contactgroup_struct *next;
} xodtemplate_contactgroup;


/* HOST TEMPLATE STRUCTURE */
typedef struct xodtemplate_host_struct {
    unsigned int id;
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
    unsigned int hourly_value;
    int       initial_state;
    double    check_interval;
    double    retry_interval;
    int       max_check_attempts;
    int       active_checks_enabled;
    int       passive_checks_enabled;
    int       obsess;
    char      *event_handler;
    int       event_handler_enabled;
    int       check_freshness;
    int       freshness_threshold;
    float     low_flap_threshold;
    float     high_flap_threshold;
    int       flap_detection_enabled;
    int       flap_detection_options;
    char      *contact_groups;
    char      *contacts;
    int       notification_options;
    int       notifications_enabled;
    char      *notification_period;
    double    notification_interval;
    double    first_notification_delay;
    int       stalking_options;
    int       process_perf_data;
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

    /* these can't be bitfields */
    char have_host_groups;
    char have_contact_groups;
    char have_contacts;
    char have_parents;

    unsigned have_display_name : 1;
    unsigned have_check_command : 1;
    unsigned have_check_period : 1;
    unsigned have_event_handler : 1;
    unsigned have_notification_period : 1;
    unsigned have_notes : 1;
    unsigned have_notes_url : 1;
    unsigned have_action_url : 1;
    unsigned have_icon_image : 1;
    unsigned have_icon_image_alt : 1;
    unsigned have_vrml_image : 1;
    unsigned have_statusmap_image : 1;

    unsigned have_initial_state : 1;
    unsigned have_check_interval : 1;
    unsigned have_retry_interval : 1;
    unsigned have_max_check_attempts : 1;
    unsigned have_active_checks_enabled : 1;
    unsigned have_passive_checks_enabled : 1;
    unsigned have_obsess : 1;
    unsigned have_event_handler_enabled : 1;
    unsigned have_check_freshness : 1;
    unsigned have_freshness_threshold : 1;
    unsigned have_low_flap_threshold : 1;
    unsigned have_high_flap_threshold : 1;
    unsigned have_flap_detection_enabled : 1;
    unsigned have_flap_detection_options : 1;
    unsigned have_notification_options : 1;
    unsigned have_notifications_enabled : 1;
    unsigned have_notification_interval : 1;
    unsigned have_first_notification_delay : 1;
    unsigned have_stalking_options : 1;
    unsigned have_process_perf_data : 1;
    unsigned have_2d_coords : 1;
    unsigned have_3d_coords : 1;
    unsigned have_retain_status_information : 1;
    unsigned have_retain_nonstatus_information : 1;
    unsigned have_hourly_value : 1;

    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_host_struct *next;
} xodtemplate_host;


/* HOSTGROUP TEMPLATE STRUCTURE */
typedef struct xodtemplate_hostgroup_struct {
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
	objectlist *member_list;
	objectlist *group_list;
	bitmap *member_map;
	bitmap *reject_map;
	int loop_status;

    char have_members;
    char have_hostgroup_members;
    char have_notes;
    char have_notes_url;
    char have_action_url;

    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_hostgroup_struct *next;
} xodtemplate_hostgroup;


/* SERVICE TEMPLATE STRUCTURE */
typedef struct xodtemplate_service_struct {
	unsigned int id;
    char       *template;
    char       *name;
    int        _config_file;
    int        _start_line;

    char       *host_name;
    char       *service_description;
    char       *display_name;
	char       *parents;
    char       *hostgroup_name;
    char       *service_groups;
    char       *check_command;
    int        initial_state;
    int        max_check_attempts;
    double     check_interval;
    double     retry_interval;
    char       *check_period;
    unsigned int hourly_value;
    int        active_checks_enabled;
    int        passive_checks_enabled;
    int        parallelize_check;
    int        is_volatile;
    int        obsess;
    char       *event_handler;
    int        event_handler_enabled;
    int        check_freshness;
    int        freshness_threshold;
    double     low_flap_threshold;
    double     high_flap_threshold;
    int        flap_detection_enabled;
    int        flap_detection_options;
    int        notification_options;
    int        notifications_enabled;
    char       *notification_period;
    double     notification_interval;
    double     first_notification_delay;
    char       *contact_groups;
    char       *contacts;
    int        stalking_options;
    int        process_perf_data;
    char       *notes;
    char       *notes_url;
    char       *action_url;
    char       *icon_image;
    char       *icon_image_alt;
    int        retain_status_information;
    int        retain_nonstatus_information;
    xodtemplate_customvariablesmember *custom_variables;

    /* these can't be bitfields */
    char have_parents;
    char have_contact_groups;
    char have_contacts;
    char have_host_name;
    char have_hostgroup_name;
    char have_service_groups;

    unsigned have_service_description : 1;
    unsigned have_display_name : 1;
    unsigned have_check_command : 1;
    unsigned have_important_check_command : 1;
    unsigned have_check_period : 1;
    unsigned have_event_handler : 1;
    unsigned have_notification_period : 1;
    unsigned have_notes : 1;
    unsigned have_notes_url : 1;
    unsigned have_action_url : 1;
    unsigned have_icon_image : 1;
    unsigned have_icon_image_alt : 1;

    unsigned have_initial_state : 1;
    unsigned have_max_check_attempts : 1;
    unsigned have_check_interval : 1;
    unsigned have_retry_interval : 1;
    unsigned have_active_checks_enabled : 1;
    unsigned have_passive_checks_enabled : 1;
    unsigned have_parallelize_check : 1;
    unsigned have_is_volatile : 1;
    unsigned have_obsess : 1;
    unsigned have_event_handler_enabled : 1;
    unsigned have_check_freshness : 1;
    unsigned have_freshness_threshold : 1;
    unsigned have_low_flap_threshold : 1;
    unsigned have_high_flap_threshold : 1;
    unsigned have_flap_detection_enabled : 1;
    unsigned have_flap_detection_options : 1;
    unsigned have_notification_options : 1;
    unsigned have_notifications_enabled : 1;
    unsigned have_notification_dependencies : 1;
    unsigned have_notification_interval : 1;
    unsigned have_first_notification_delay : 1;
    unsigned have_stalking_options : 1;
    unsigned have_process_perf_data : 1;
    unsigned have_retain_status_information : 1;
    unsigned have_retain_nonstatus_information : 1;
    unsigned have_hourly_value : 1;
    unsigned is_from_hostgroup : 1;

    unsigned is_copy : 1;
    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_service_struct *next;
} xodtemplate_service;


/* SERVICEGROUP TEMPLATE STRUCTURE */
typedef struct xodtemplate_servicegroup_struct {
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
	objectlist *member_list;
	objectlist *group_list;
	bitmap *member_map;
	bitmap *reject_map;
	int loop_status;

    char have_members;
    char have_servicegroup_members;
    char have_notes;
    char have_notes_url;
    char have_action_url;

    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_servicegroup_struct *next;
} xodtemplate_servicegroup;


/* SERVICEDEPENDENCY TEMPLATE STRUCTURE */
typedef struct xodtemplate_servicedependency_struct {
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
    int        notification_failure_options;
    int        execution_failure_options;

    char have_host_name;
    char have_service_description;
    char have_dependent_host_name;
    char have_dependent_service_description;
    char have_servicegroup_name;
    char have_hostgroup_name;
    char have_dependent_servicegroup_name;
    char have_dependent_hostgroup_name;
    char have_dependency_period;

    char have_inherits_parent;
    char have_notification_failure_options;
    char have_execution_failure_options;

    unsigned is_copy : 1;
    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_servicedependency_struct *next;
} xodtemplate_servicedependency;


/* SERVICEESCALATION TEMPLATE STRUCTURE */
typedef struct xodtemplate_serviceescalation_struct {
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
    int       escalation_options;
    char      *contact_groups;
    char      *contacts;

    char have_host_name;
    char have_service_description;
    char have_servicegroup_name;
    char have_hostgroup_name;
    char have_escalation_period;
    char have_contact_groups;
    char have_contacts;

    char have_first_notification;
    char have_last_notification;
    char have_notification_interval;
    char have_escalation_options;

    unsigned is_copy : 1;
    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_serviceescalation_struct *next;
} xodtemplate_serviceescalation;


/* HOSTDEPENDENCY TEMPLATE STRUCTURE */
typedef struct xodtemplate_hostdependency_struct {
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
    int       notification_failure_options;
    int       execution_failure_options;

    char have_host_name;
    char have_dependent_host_name;
    char have_hostgroup_name;
    char have_dependent_hostgroup_name;
    char have_dependency_period;

    char have_inherits_parent;
    char have_notification_failure_options;
    char have_execution_failure_options;

    unsigned is_copy : 1;
    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_hostdependency_struct *next;
} xodtemplate_hostdependency;


/* HOSTESCALATION TEMPLATE STRUCTURE */
typedef struct xodtemplate_hostescalation_struct {
	unsigned int id;
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
    int       escalation_options;
    char      *contact_groups;
    char      *contacts;

    char have_host_name;
    char have_hostgroup_name;
    char have_escalation_period;
    char have_contact_groups;
    char have_contacts;

    char have_first_notification;
    char have_last_notification;
    char have_notification_interval;
    char have_escalation_options;

    unsigned is_copy : 1;
    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_hostescalation_struct *next;
} xodtemplate_hostescalation;


/* HOSTEXTINFO TEMPLATE STRUCTURE */
typedef struct xodtemplate_hostextinfo_struct {
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

    char have_host_name;
    char have_hostgroup_name;
    char have_notes;
    char have_notes_url;
    char have_action_url;
    char have_icon_image;
    char have_icon_image_alt;
    char have_vrml_image;
    char have_statusmap_image;

    char have_2d_coords;
    char have_3d_coords;

    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_hostextinfo_struct *next;
} xodtemplate_hostextinfo;


/* SERVICEEXTINFO TEMPLATE STRUCTURE */
typedef struct xodtemplate_serviceextinfo_struct {
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

    char have_host_name;
    char have_hostgroup_name;
    char have_service_description;
    char have_notes;
    char have_notes_url;
    char have_action_url;
    char have_icon_image;
    char have_icon_image_alt;

    unsigned has_been_resolved : 1;
    unsigned register_object : 1;
    struct xodtemplate_serviceextinfo_struct *next;
} xodtemplate_serviceextinfo;


/* CONTACT LIST STRUCTURE */
typedef struct xodtemplate_contactlist_struct {
    char      *contact_name;
    struct xodtemplate_contactlist_struct *next;
} xodtemplate_contactlist;


/* HOST LIST STRUCTURE */
typedef struct xodtemplate_hostlist_struct {
    char      *host_name;
    struct xodtemplate_hostlist_struct *next;
} xodtemplate_hostlist;


/* SERVICE LIST STRUCTURE */
typedef struct xodtemplate_servicelist_struct {
    char      *host_name;
    char      *service_description;
    struct xodtemplate_servicelist_struct *next;
} xodtemplate_servicelist;


/* MEMBER LIST STRUCTURE */
typedef struct xodtemplate_memberlist_struct {
    char      *name1;
    char      *name2;
    struct xodtemplate_memberlist_struct *next;
} xodtemplate_memberlist;


/***** CHAINED HASH DATA STRUCTURES ******/

typedef struct xodtemplate_service_cursor_struct {
    int xodtemplate_service_iterator;
    xodtemplate_service *current_xodtemplate_service;
} xodtemplate_service_cursor;



/********* FUNCTION DEFINITIONS **********/

int xodtemplate_read_config_data(const char *, int);    /* top-level routine processes all config files */
int xodtemplate_process_config_file(char *, int);           /* process data in a specific config file */
int xodtemplate_process_config_dir(char *, int);            /* process all files in a specific config directory */

int xodtemplate_expand_services(objectlist **, bitmap *, char *, char *, int, int);
int xodtemplate_expand_contactgroups(objectlist **, bitmap *, char *, int, int);
int xodtemplate_expand_contacts(objectlist **, bitmap *, char *, int, int);

objectlist *xodtemplate_expand_hostgroups_and_hosts(char *, char *, int, int);
int xodtemplate_expand_hostgroups(objectlist **, bitmap *, char *, int, int);
int xodtemplate_expand_hosts(objectlist **list, bitmap *reject_map, char *, int, int);

int xodtemplate_expand_servicegroups(objectlist **, bitmap *, char *, int, int);

char *xodtemplate_process_contactgroup_names(char *, int, int);
int xodtemplate_get_contactgroup_names(xodtemplate_memberlist **, xodtemplate_memberlist **, char *, int, int);

char *xodtemplate_process_hostgroup_names(char *, int, int);
int xodtemplate_get_hostgroup_names(xodtemplate_memberlist **, xodtemplate_memberlist **, char *, int, int);

char *xodtemplate_process_servicegroup_names(char *, int, int);
int xodtemplate_get_servicegroup_names(xodtemplate_memberlist **, xodtemplate_memberlist **, char *, int, int);

int xodtemplate_add_member_to_memberlist(xodtemplate_memberlist **, char *, char *);
int xodtemplate_free_memberlist(xodtemplate_memberlist **);
void xodtemplate_remove_memberlist_item(xodtemplate_memberlist *, xodtemplate_memberlist **);


int xodtemplate_begin_object_definition(char *, int, int, int);
int xodtemplate_add_object_property(char *, int);
int xodtemplate_end_object_definition(int);

int xodtemplate_parse_timeperiod_directive(xodtemplate_timeperiod *, char *, char *);
xodtemplate_daterange *xodtemplate_add_exception_to_timeperiod(xodtemplate_timeperiod *, int, int, int, int, int, int, int, int, int, int, int, int, char *);
int xodtemplate_get_month_from_string(char *, int *);
int xodtemplate_get_weekday_from_string(char *, int *);

xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_host(xodtemplate_host *, char *, char *);
xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_service(xodtemplate_service *, char *, char *);
xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_contact(xodtemplate_contact *, char *, char *);
xodtemplate_customvariablesmember *xodtemplate_add_custom_variable_to_object(xodtemplate_customvariablesmember **, char *, char *);


int xodtemplate_register_objects(void);
int xodtemplate_free_memory(void);

#ifdef NSCORE
int xodtemplate_duplicate_objects(void);
int xodtemplate_duplicate_services(void);

int xodtemplate_inherit_object_properties(void);

int xodtemplate_resolve_objects(void);

int xodtemplate_cache_objects(char *);

int xodtemplate_duplicate_service(xodtemplate_service *, char *, int);
int xodtemplate_duplicate_hostescalation(xodtemplate_hostescalation *, char *);
int xodtemplate_duplicate_serviceescalation(xodtemplate_serviceescalation *, char *, char *);
int xodtemplate_duplicate_hostdependency(xodtemplate_hostdependency *, char *, char *);
int xodtemplate_duplicate_servicedependency(xodtemplate_servicedependency *, char *, char *, char *, char *);
int xodtemplate_duplicate_hostextinfo(xodtemplate_hostextinfo *, char *);
int xodtemplate_duplicate_serviceextinfo(xodtemplate_serviceextinfo *, char *);
#endif

int xodtemplate_recombobulate_contactgroups(void);
int xodtemplate_recombobulate_hostgroups(void);
int xodtemplate_recombobulate_servicegroups(void);

#ifdef NSCORE
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

int xodtemplate_merge_extinfo_ojects(void);
int xodtemplate_merge_host_extinfo_object(xodtemplate_host *, xodtemplate_hostextinfo *);
int xodtemplate_merge_service_extinfo_object(xodtemplate_service *, xodtemplate_serviceextinfo *);
#endif

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
xodtemplate_service *xodtemplate_find_real_service(char *, char *);
xodtemplate_hostdependency *xodtemplate_find_hostdependency(char *);
xodtemplate_hostescalation *xodtemplate_find_hostescalation(char *);
xodtemplate_hostextinfo *xodtemplate_find_hostextinfo(char *);
xodtemplate_serviceextinfo *xodtemplate_find_serviceextinfo(char *);

int xodtemplate_get_inherited_string(char *, char **, char *, char **);
int xodtemplate_clean_additive_string(char **);
int xodtemplate_clean_additive_strings(void);

int xodtemplate_register_timeperiod(xodtemplate_timeperiod *);
int xodtemplate_get_time_ranges(char *, unsigned long *, unsigned long *);
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
