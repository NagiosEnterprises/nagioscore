/*****************************************************************************
 *
 * OBJECTS.H - Header file for object addition/search functions
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   12-13-2002
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


#ifndef _OBJECTS_H
#define _OBJECTS_H

#include "../common/config.h"
#include "../common/common.h"



#define MAX_HOSTNAME_LENGTH            		64	/* max. host name length */
#define MAX_SERVICEDESC_LENGTH			64	/* max. service description length */
#define MAX_PLUGINOUTPUT_LENGTH			352	/* max. length of plugin output */

#define MAX_STATE_HISTORY_ENTRIES		21	/* max number of old states to keep track of for flap detection */

#define SERVICES_HASHSLOTS 1024
#define HOSTS_HASHSLOTS 1024



/****************** DATA STRUCTURES *******************/

/* TIMERANGE structure */
typedef struct timerange_struct{
	unsigned long range_start;
	unsigned long range_end;
	struct timerange_struct *next;
        }timerange;


/* TIMEPERIOD structure */
typedef struct timeperiod_struct{
	char    *name;
	char    *alias;
	timerange *days[7];
	struct 	timeperiod_struct *next;
	}timeperiod;


/* CONTACTGROUPMEMBER structure */
typedef struct contactgroupmember_struct{
	char    *contact_name;
	struct  contactgroupmember_struct *next;
        }contactgroupmember;


/* CONTACTGROUP structure */
typedef struct contactgroup_struct{
	char	*group_name;
	char    *alias;
	contactgroupmember *members;
	struct	contactgroup_struct *next;
	}contactgroup;


/* CONTACTGROUPSMEMBER structure */
typedef struct contactgroupsmember_struct{
	char *group_name;
	struct contactgroupsmember_struct *next;
        }contactgroupsmember;


/* HOSTSMEMBER structure */
typedef struct hostsmember_struct{
	char *host_name;
	struct hostsmember_struct *next;
        }hostsmember;


/* HOST structure */
typedef struct host_struct{
	char    *name;
	char	*alias;
	char    *address;
        hostsmember *parent_hosts;
	char    *host_check_command;
	int     max_attempts;
	char    *event_handler;
	contactgroupsmember *contact_groups;
	int     notification_interval;
	int	notify_on_down;
	int	notify_on_unreachable;
	int     notify_on_recovery;
	char	*notification_period;
	int     flap_detection_enabled;
	double  low_flap_threshold;
	double  high_flap_threshold;
	int     stalk_on_up;
	int     stalk_on_down;
	int     stalk_on_unreachable;
	int     process_performance_data;
	int     checks_enabled;
	int     accept_passive_host_checks;
	int     event_handler_enabled;
	int     retain_status_information;
	int     retain_nonstatus_information;
	int     failure_prediction_enabled;
	char    *failure_prediction_options;
#ifdef NSCORE
	int     problem_has_been_acknowledged;
	int     status;
	char	*plugin_output;
	char    *perf_data;
	int     current_attempt;
	double  execution_time;
	int     notifications_enabled;
	time_t  last_host_notification;
	time_t  next_host_notification;
	time_t  last_check;
	time_t	last_state_change;
	int     has_been_checked;
	int     has_been_down;
	int     has_been_unreachable;
	int     current_notification_number;
	int     no_more_notifications;
	int     check_flapping_recovery_notification;
	int     scheduled_downtime_depth;
	int     pending_flex_downtime;
	int     state_history[MAX_STATE_HISTORY_ENTRIES];    /* flap detection */
	int     state_history_index;
	time_t  last_state_history_update;
	int     is_flapping;
	int     flapping_comment_id;
	double  percent_state_change;
	int     total_services;
	unsigned long total_service_check_interval;
#endif
	struct  host_struct *next;
        }host;


/* HOSTGROUPMEMBER structure */
typedef struct hostgroupmember_struct{
	char    *host_name;
	struct  hostgroupmember_struct *next;
        }hostgroupmember;


/* HOSTGROUP structure */
typedef struct hostgroup_struct{
	char 	*group_name;
	char    *alias;
	hostgroupmember *members;
	struct	hostgroup_struct *next;
	}hostgroup;


/* COMMANDSMEMBER structure */
typedef struct commandsmember_struct{
	char	*command;
	struct	commandsmember_struct *next;
	}commandsmember;


/* CONTACT structure */
typedef struct contact_struct{
	char	*name;
	char	*alias;
	char	*email;
	char	*pager;
	commandsmember *host_notification_commands;
	commandsmember *service_notification_commands;	
	int     notify_on_service_unknown;
	int     notify_on_service_warning;
	int     notify_on_service_critical;
	int     notify_on_service_recovery;
	int 	notify_on_host_down;
	int	notify_on_host_unreachable;
	int	notify_on_host_recovery;
	char	*host_notification_period;
	char	*service_notification_period;
	struct	contact_struct *next;
	}contact;



/* SERVICE structure */
typedef struct service_struct{
	char	*host_name;
	char	*description;
        char    *service_check_command;
	char    *event_handler;
	int	check_interval;
	int     retry_interval;
	int	max_attempts;
	int     parallelize;
	contactgroupsmember *contact_groups;
	int	notification_interval;
	int     notify_on_unknown;
	int	notify_on_warning;
	int	notify_on_critical;
	int	notify_on_recovery;
	int     stalk_on_ok;
	int     stalk_on_warning;
	int     stalk_on_unknown;
	int     stalk_on_critical;
	int     is_volatile;
	char	*notification_period;
	char	*check_period;
	int     flap_detection_enabled;
	double  low_flap_threshold;
	double  high_flap_threshold;
	int     process_performance_data;
	int     check_freshness;
	int     freshness_threshold;
	int     accept_passive_service_checks;
	int     event_handler_enabled;
	int	checks_enabled;
	int     retain_status_information;
	int     retain_nonstatus_information;
	int     notifications_enabled;
	int     obsess_over_service;
	int     failure_prediction_enabled;
	char    *failure_prediction_options;
#ifdef NSCORE
	int     problem_has_been_acknowledged;
	int     host_problem_at_last_check;
	int     dependency_failure_at_last_check;
	int     no_recovery_notification;
	int     check_type;
	int	current_state;
	int	last_state;
	int	last_hard_state;
	char	*plugin_output;
	char    *perf_data;
        int     state_type;
	time_t	next_check;
	int     should_be_scheduled;
	time_t	last_check;
	int	current_attempt;
	time_t	last_notification;
	time_t  next_notification;
	int     no_more_notifications;
	int     check_flapping_recovery_notification;
	time_t	last_state_change;
	int     has_been_checked;
	int     is_being_freshened;
	int     has_been_unknown;
	int     has_been_warning;
	int     has_been_critical;
	int     current_notification_number;
	unsigned long latency;
	double  execution_time;
	int     is_executing;
	int     check_options;
	int     scheduled_downtime_depth;
	int     pending_flex_downtime;
	int     state_history[MAX_STATE_HISTORY_ENTRIES];    /* flap detection */
	int     state_history_index;
	int     is_flapping;
	int     flapping_comment_id;
	double  percent_state_change;
#endif
	struct service_struct *next;
	}service;


/* COMMAND structure */
typedef struct command_struct{
	char    *name;
	char    *command_line;
	struct command_struct *next;
        }command;


/* SERVICE ESCALATION structure */
typedef struct serviceescalation_struct{
	char    *host_name;
	char    *description;
	int     first_notification;
	int     last_notification;
	int     notification_interval;
	contactgroupsmember *contact_groups;
	struct  serviceescalation_struct *next;
        }serviceescalation;


/* SERVICE DEPENDENCY structure */
typedef struct servicedependency_struct{
	int     dependency_type;
	char    *dependent_host_name;
	char    *dependent_service_description;
	char    *host_name;
	char    *service_description;
	int     fail_on_ok;
	int     fail_on_warning;
	int     fail_on_unknown;
	int     fail_on_critical;
#ifdef NSCORE
	int     has_been_checked;
#endif
	struct servicedependency_struct *next;
        }servicedependency;


/* HOST ESCALATION structure */
typedef struct hostescalation_struct{
	char    *host_name;
	int     first_notification;
	int     last_notification;
	int     notification_interval;
	contactgroupsmember *contact_groups;
	struct  hostescalation_struct *next;
        }hostescalation;


/* HOST DEPENDENCY structure */
typedef struct hostdependency_struct{
	int     dependency_type;
	char    *dependent_host_name;
	char    *host_name;
	int     fail_on_up;
	int     fail_on_down;
	int     fail_on_unreachable;
	struct hostdependency_struct *next;
        }hostdependency;



/****************** HASH STRUCTURES ********************/

typedef struct host_cursor_struct{
	int     host_hashchain_iterator;
	host    *current_host_pointer;
        }host_cursor;





/********************* FUNCTIONS **********************/

/**** DEBUG functions ****/
/* RMO: 9/25/01
   Send debug output to stdout. Does nothing if 'level' is
   not enabled by a corresponding 'DEBUGn' define.
   Accepts format string (fmt) and variable-length arg list
   (as printf does). Prints to stdout and, if NSCGI environment,
   surrounded with HTML comment delimiters to be viewed through
   browser's 'view source' option.

   Use as: dbg_print((level,fmt,...)); [NOTE double parens]
 
   The macro def below causes dbg_print(()) calls to vaporize
   if none of the DEBUGn levels are defined.
*/
#if defined(DEBUG0) || defined(DEBUG1) || defined(DEBUG2) || defined(DEBUG3) || defined(DEBUG4) || defined(DEBUG5) || defined(DEBUG6) || defined(DEBUG7) || defined(DEBUG8) || defined(DEBUG9) || defined(DEBUG10) || defined(DEBUG11)
#define dbg_print(args) dbg_print_x args
#else
#define dbg_print(args)
#endif


/**** Top-level input functions ****/
int read_object_config_data(char *,int);        /* reads all external configuration data of specific types */

/**** Object Creation Functions ****/
contact *add_contact(char *,char *,char *,char *,char *,char *,int,int,int,int,int,int,int);		/* adds a contact definition */
commandsmember *add_service_notification_command_to_contact(contact *,char *);				/* adds a service notification command to a contact definition */
commandsmember *add_host_notification_command_to_contact(contact *,char *);				/* adds a host notification command to a contact definition */
host *add_host(char *,char *,char *,int,int,int,int,int,char *,int,char *,int,int,char *,int,int,double,double,int,int,int,int,int,char *,int,int);	/* adds a host definition */
hostsmember *add_parent_host_to_host(host *,char *);							/* adds a parent host to a host definition */
contactgroupsmember *add_contactgroup_to_host(host *,char *);					        /* adds a contactgroup to a host definition */
timeperiod *add_timeperiod(char *,char *);								/* adds a timeperiod definition */
timerange *add_timerange_to_timeperiod(timeperiod *,int,unsigned long,unsigned long);			/* adds a timerange to a timeperiod definition */
hostgroup *add_hostgroup(char *,char *);								/* adds a hostgroup definition */
hostgroupmember *add_host_to_hostgroup(hostgroup *, char *);						/* adds a host to a hostgroup definition */
contactgroup *add_contactgroup(char *,char *);								/* adds a contactgroup definition */
contactgroupmember *add_contact_to_contactgroup(contactgroup *,char *);					/* adds a contact to a contact group defintion */
command *add_command(char *,char *);									/* adds a command definition */
service *add_service(char *,char *,char *,int,int,int,int,int,int,char *,int,int,int,int,int,int,char *,int,char *,int,int,double,double,int,int,int,int,int,int,char *,int,int,int,int,int);	/* adds a service definition */
contactgroupsmember *add_contactgroup_to_service(service *,char *);					/* adds a contact group to a service definition */
serviceescalation *add_serviceescalation(char *,char *,int,int,int);                                    /* adds a service escalation definition */
contactgroupsmember *add_contactgroup_to_serviceescalation(serviceescalation *,char *);                 /* adds a contact group to a service escalation definition */
servicedependency *add_service_dependency(char *,char *,char *,char *,int,int,int,int,int);             /* adds a service dependency definition */
hostdependency *add_host_dependency(char *,char *,int,int,int,int);                                     /* adds a host dependency definition */
hostescalation *add_hostescalation(char *,int,int,int);                                                 /* adds a host escalation definition */
contactgroupsmember *add_contactgroup_to_hostescalation(hostescalation *,char *);                       /* adds a contact group to a host escalation definition */


/**** Object Search Functions ****/
timeperiod * find_timeperiod(char *,timeperiod *);						/* finds a timeperiod object */
host * find_host(char *);									/* finds a host object */
hostgroup * find_hostgroup(char *, hostgroup *);						/* finds a hostgroup object */
hostgroupmember *find_hostgroupmember(char *,hostgroup *,hostgroupmember *);			/* finds a hostgroup member object */
contact * find_contact(char *, contact *);							/* finds a contact object */
contactgroup * find_contactgroup(char *, contactgroup *);					/* finds a contactgroup object */
contactgroupmember *find_contactgroupmember(char *,contactgroup *,contactgroupmember *);	/* finds a contactgroup member object */
command * find_command(char *,command *);							/* finds a command object */
service * find_service(char *,char *);								/* finds a service object */
void move_first_service(void);									/* sets up the static memory area for get_next_service */
service *get_next_service(void);								/* returns the next service, NULL at the end of the list */
int find_all_services_by_host(char *host);							/* sets up the static memory area for get_next_service_by_host */
service *get_next_service_by_host(void);							/* returns the next service for the host, NULL at the end of the list */
void move_first_host(void);									/* sets up the static memory area for get_next_host */
host *get_next_host(void);									/* returns the next host, NULL at the end of the list */
void *get_host_cursor(void);					                                /* allocate memory for the host cursor */
host *get_next_host_cursor(void *v_cursor);							/* return the next host, NULL at the end of the list */
void free_host_cursor(void *cursor);								/* free allocated cursor memory */
int compare_host(host *,const char *);
int host_comes_after(host *,const char *);
int add_host_allocated(host *);
int compare_service(service *,const char *,const char *);
int service_comes_after(service *,const char *,const char *);
int add_service_allocated(service *);



/**** Object Query Functions ****/
int is_host_immediate_child_of_host(host *,host *);	                /* checks if a host is an immediate child of another host */	
int is_host_primary_immediate_child_of_host(host *,host *);             /* checsk if a host is an immediate child (and primary child) of another host */
int is_host_immediate_parent_of_host(host *,host *);	                /* checks if a host is an immediate child of another host */	
int is_host_member_of_hostgroup(hostgroup *,host *);		        /* tests whether or not a host is a member of a specific hostgroup */
int is_contact_member_of_contactgroup(contactgroup *, contact *);	/* tests whether or not a contact is a member of a specific contact group */
int is_contact_for_hostgroup(hostgroup *,contact *);	                /* tests whether or not a contact is a member of a specific hostgroup */
int is_contact_for_host(host *,contact *);			        /* tests whether or not a contact is a contact member for a specific host */
int is_escalated_contact_for_host(host *,contact *);                    /* checsk whether or not a contact is an escalated contact for a specific host */
int is_contact_for_service(service *,contact *);		        /* tests whether or not a contact is a contact member for a specific service */
int is_escalated_contact_for_service(service *,contact *);              /* checsk whether or not a contact is an escalated contact for a specific service */
int is_host_immediate_parent_of_host(host *,host *);		        /* tests whether or not a host is an immediate parent of another host */

int number_of_immediate_child_hosts(host *);		                /* counts the number of immediate child hosts for a particular host */
int number_of_total_child_hosts(host *);				/* counts the number of total child hosts for a particular host */
int number_of_immediate_parent_hosts(host *);				/* counts the number of immediate parents hosts for a particular host */
int number_of_total_parent_hosts(host *);				/* counts the number of total parents hosts for a particular host */

#ifdef NSCORE
int check_for_circular_path(host *,host *);                             /* checks if a circular path exists for a given host */
int check_for_circular_dependency(servicedependency *,servicedependency *);   /* checks if a circular execution dependency exists for a given service */
#endif


/**** Object Cleanup Functions ****/
int free_object_data(void);                             /* frees all allocated memory for the object definitions */



/**** Hash Functions ****/
void *get_next_N(void **hashchain, int hashslots, int *iterator, void *current, void *next);
int hashfunc1(const char *name1, int hashslots);
int hashfunc2(const char *name1, const char *name2, int hashslots);

#endif

