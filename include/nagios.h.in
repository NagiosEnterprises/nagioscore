/************************************************************************
 *
 * Nagios Main Header File
 * Written By: Ethan Galstad (nagios@nagios.org)
 * Last Modified: 01-08-2007
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
 ************************************************************************/

#ifndef _NAGIOS_H
#define _NAGIOS_H

#include "config.h"
#include "common.h"
#include "locations.h"
#include "objects.h"

#ifdef __cplusplus
extern "C" { 
#endif

#define MAX_COMMAND_ARGUMENTS			32	/* maximum number of $ARGx$ macros */
#define MAX_USER_MACROS				256	/* maximum number of $USERx$ macros */

#define MAX_STATE_LENGTH			32	/* length definitions used in macros */
#define MAX_STATETYPE_LENGTH			24
#define MAX_CHECKTYPE_LENGTH         		8
#define MAX_NOTIFICATIONTYPE_LENGTH		32
#define MAX_NOTIFICATIONNUMBER_LENGTH		8
#define MAX_ATTEMPT_LENGTH			8
#define MAX_TOTALS_LENGTH			8
#define MAX_EXECUTIONTIME_LENGTH		10
#define MAX_LATENCY_LENGTH			10
#define MAX_DURATION_LENGTH			17
#define MAX_DOWNTIME_LENGTH			3
#define MAX_STATEID_LENGTH			2
#define MAX_PERCENTCHANGE_LENGTH           	8

#define MACRO_ENV_VAR_PREFIX			"NAGIOS_"

#define MACRO_X_COUNT				99	/* size of macro_x[] array */

#define MACRO_HOSTNAME				0
#define MACRO_HOSTALIAS				1
#define MACRO_HOSTADDRESS			2
#define MACRO_SERVICEDESC			3
#define MACRO_SERVICESTATE			4
#define MACRO_SERVICESTATEID                    5
#define MACRO_SERVICEATTEMPT			6
#define MACRO_LONGDATETIME			7
#define MACRO_SHORTDATETIME			8
#define MACRO_DATE				9
#define MACRO_TIME				10
#define MACRO_TIMET				11
#define MACRO_LASTHOSTCHECK			12
#define MACRO_LASTSERVICECHECK			13
#define MACRO_LASTHOSTSTATECHANGE		14
#define MACRO_LASTSERVICESTATECHANGE		15
#define MACRO_HOSTOUTPUT			16
#define MACRO_SERVICEOUTPUT			17
#define MACRO_HOSTPERFDATA			18
#define MACRO_SERVICEPERFDATA			19
#define MACRO_CONTACTNAME			20
#define MACRO_CONTACTALIAS			21
#define MACRO_CONTACTEMAIL			22
#define MACRO_CONTACTPAGER			23
#define MACRO_ADMINEMAIL			24
#define MACRO_ADMINPAGER			25
#define MACRO_HOSTSTATE				26
#define MACRO_HOSTSTATEID                       27
#define MACRO_HOSTATTEMPT			28
#define MACRO_NOTIFICATIONTYPE			29
#define MACRO_NOTIFICATIONNUMBER		30
#define MACRO_HOSTEXECUTIONTIME			31
#define MACRO_SERVICEEXECUTIONTIME		32
#define MACRO_HOSTLATENCY                       33
#define MACRO_SERVICELATENCY			34
#define MACRO_HOSTDURATION			35
#define MACRO_SERVICEDURATION			36
#define MACRO_HOSTDURATIONSEC			37
#define MACRO_SERVICEDURATIONSEC		38
#define MACRO_HOSTDOWNTIME			39
#define MACRO_SERVICEDOWNTIME			40
#define MACRO_HOSTSTATETYPE			41
#define MACRO_SERVICESTATETYPE			42
#define MACRO_HOSTPERCENTCHANGE			43
#define MACRO_SERVICEPERCENTCHANGE		44
#define MACRO_HOSTGROUPNAME			45
#define MACRO_HOSTGROUPALIAS			46
#define MACRO_SERVICEGROUPNAME			47
#define MACRO_SERVICEGROUPALIAS			48
#define MACRO_HOSTACKAUTHOR                     49
#define MACRO_HOSTACKCOMMENT                    50
#define MACRO_SERVICEACKAUTHOR                  51
#define MACRO_SERVICEACKCOMMENT                 52
#define MACRO_LASTSERVICEOK                     53
#define MACRO_LASTSERVICEWARNING                54
#define MACRO_LASTSERVICEUNKNOWN                55
#define MACRO_LASTSERVICECRITICAL               56
#define MACRO_LASTHOSTUP                        57
#define MACRO_LASTHOSTDOWN                      58
#define MACRO_LASTHOSTUNREACHABLE               59
#define MACRO_SERVICECHECKCOMMAND		60
#define MACRO_HOSTCHECKCOMMAND			61
#define MACRO_MAINCONFIGFILE			62
#define MACRO_STATUSDATAFILE			63
#define MACRO_COMMENTDATAFILE			64
#define MACRO_DOWNTIMEDATAFILE			65
#define MACRO_RETENTIONDATAFILE			66
#define MACRO_OBJECTCACHEFILE			67
#define MACRO_TEMPFILE				68
#define MACRO_LOGFILE				69
#define MACRO_RESOURCEFILE			70
#define MACRO_COMMANDFILE			71
#define MACRO_HOSTPERFDATAFILE			72
#define MACRO_SERVICEPERFDATAFILE		73
#define MACRO_HOSTACTIONURL			74
#define MACRO_HOSTNOTESURL			75
#define MACRO_HOSTNOTES				76
#define MACRO_SERVICEACTIONURL			77
#define MACRO_SERVICENOTESURL			78
#define MACRO_SERVICENOTES			79
#define MACRO_TOTALHOSTSUP			80
#define MACRO_TOTALHOSTSDOWN			81
#define MACRO_TOTALHOSTSUNREACHABLE		82
#define MACRO_TOTALHOSTSDOWNUNHANDLED		83
#define MACRO_TOTALHOSTSUNREACHABLEUNHANDLED	84
#define MACRO_TOTALHOSTPROBLEMS			85
#define MACRO_TOTALHOSTPROBLEMSUNHANDLED	86
#define MACRO_TOTALSERVICESOK			87
#define MACRO_TOTALSERVICESWARNING		88
#define MACRO_TOTALSERVICESCRITICAL		89
#define MACRO_TOTALSERVICESUNKNOWN		90
#define MACRO_TOTALSERVICESWARNINGUNHANDLED	91
#define MACRO_TOTALSERVICESCRITICALUNHANDLED	92
#define MACRO_TOTALSERVICESUNKNOWNUNHANDLED	93
#define MACRO_TOTALSERVICEPROBLEMS		94
#define MACRO_TOTALSERVICEPROBLEMSUNHANDLED	95
#define MACRO_PROCESSSTARTTIME			96
#define MACRO_HOSTCHECKTYPE			97
#define MACRO_SERVICECHECKTYPE			98



#define DEFAULT_LOG_LEVEL			1	/* log all events to main log file */
#define DEFAULT_USE_SYSLOG			1	/* log events to syslog? 1=yes, 0=no */
#define DEFAULT_SYSLOG_LEVEL			2	/* log only severe events to syslog */

#define DEFAULT_NOTIFICATION_LOGGING		1	/* log notification events? 1=yes, 0=no */

#define DEFAULT_INTER_CHECK_DELAY		5.0	/* seconds between initial service check scheduling */
#define DEFAULT_INTERLEAVE_FACTOR      		1       /* default interleave to use when scheduling checks */
#define DEFAULT_SLEEP_TIME      		0.5    	/* seconds between event run checks */
#define DEFAULT_INTERVAL_LENGTH 		60     	/* seconds per interval unit for check scheduling */
#define DEFAULT_RETRY_INTERVAL  		30	/* services are retried in 30 seconds if they're not OK */
#define DEFAULT_COMMAND_CHECK_INTERVAL		-1	/* interval to check for external commands (default = as often as possible) */
#define DEFAULT_SERVICE_REAPER_INTERVAL		10	/* interval in seconds to reap service check results */
#define DEFAULT_MAX_REAPER_TIME                 30      /* maximum number of seconds to spend reaping service checks before we break out for a while */
#define DEFAULT_MAX_PARALLEL_SERVICE_CHECKS 	0	/* maximum number of service checks we can have running at any given time (0=unlimited) */
#define DEFAULT_RETENTION_UPDATE_INTERVAL	60	/* minutes between auto-save of retention data */
#define DEFAULT_RETENTION_SCHEDULING_HORIZON    900     /* max seconds between program restarts that we will preserve scheduling information */
#define DEFAULT_STATUS_UPDATE_INTERVAL		60	/* seconds between aggregated status data updates */
#define DEFAULT_FRESHNESS_CHECK_INTERVAL        60      /* seconds between service result freshness checks */
#define DEFAULT_AUTO_RESCHEDULING_INTERVAL      30      /* seconds between host and service check rescheduling events */
#define DEFAULT_AUTO_RESCHEDULING_WINDOW        180     /* window of time (in seconds) for which we should reschedule host and service checks */

#define DEFAULT_NOTIFICATION_TIMEOUT		30	/* max time in seconds to wait for notification commands to complete */
#define DEFAULT_EVENT_HANDLER_TIMEOUT		30	/* max time in seconds to wait for event handler commands to complete */
#define DEFAULT_HOST_CHECK_TIMEOUT		30	/* max time in seconds to wait for host check commands to complete */
#define DEFAULT_SERVICE_CHECK_TIMEOUT		60	/* max time in seconds to wait for service check commands to complete */
#define DEFAULT_OCSP_TIMEOUT			15	/* max time in seconds to wait for obsessive compulsive processing commands to complete */
#define DEFAULT_OCHP_TIMEOUT			15	/* max time in seconds to wait for obsessive compulsive processing commands to complete */
#define DEFAULT_PERFDATA_TIMEOUT                5       /* max time in seconds to wait for performance data commands to complete */
#define DEFAULT_TIME_CHANGE_THRESHOLD		900	/* compensate for time changes of more than 15 minutes */

#define DEFAULT_LOG_HOST_RETRIES		0	/* don't log host retries */
#define DEFAULT_LOG_SERVICE_RETRIES		0	/* don't log service retries */
#define DEFAULT_LOG_EVENT_HANDLERS		1	/* log event handlers */
#define DEFAULT_LOG_INITIAL_STATES		0	/* don't log initial service and host states */
#define DEFAULT_LOG_EXTERNAL_COMMANDS		1	/* log external commands */
#define DEFAULT_LOG_PASSIVE_CHECKS		1	/* log passive service checks */

#define DEFAULT_AGGRESSIVE_HOST_CHECKING	0	/* don't use "aggressive" host checking */
#define DEFAULT_CHECK_EXTERNAL_COMMANDS		0 	/* don't check for external commands */
#define DEFAULT_CHECK_ORPHANED_SERVICES		1	/* don't check for orphaned services */
#define DEFAULT_ENABLE_FLAP_DETECTION           0       /* don't enable flap detection */
#define DEFAULT_PROCESS_PERFORMANCE_DATA        0       /* don't process performance data */
#define DEFAULT_CHECK_SERVICE_FRESHNESS         1       /* check service result freshness */
#define DEFAULT_CHECK_HOST_FRESHNESS            0       /* don't check host result freshness */
#define DEFAULT_AUTO_RESCHEDULE_CHECKS          0       /* don't auto-reschedule host and service checks */

#define DEFAULT_LOW_SERVICE_FLAP_THRESHOLD	20.0	/* low threshold for detection of service flapping */
#define DEFAULT_HIGH_SERVICE_FLAP_THRESHOLD	30.0	/* high threshold for detection of service flapping */
#define DEFAULT_LOW_HOST_FLAP_THRESHOLD		20.0	/* low threshold for detection of host flapping */
#define DEFAULT_HIGH_HOST_FLAP_THRESHOLD	30.0	/* high threshold for detection of host flapping */

#define DEFAULT_HOST_CHECK_SPREAD		30	/* max minutes to schedule all initial host checks */
#define DEFAULT_SERVICE_CHECK_SPREAD		30	/* max minutes to schedule all initial service checks */



/******************* LOGGING TYPES ********************/

#define NSLOG_RUNTIME_ERROR		1
#define NSLOG_RUNTIME_WARNING		2

#define NSLOG_VERIFICATION_ERROR	4
#define NSLOG_VERIFICATION_WARNING	8

#define NSLOG_CONFIG_ERROR		16
#define NSLOG_CONFIG_WARNING		32

#define NSLOG_PROCESS_INFO		64
#define NSLOG_EVENT_HANDLER		128
/*#define NSLOG_NOTIFICATION		256*/	/* NOT USED ANYMORE - CAN BE REUSED */
#define NSLOG_EXTERNAL_COMMAND		512

#define NSLOG_HOST_UP      		1024
#define NSLOG_HOST_DOWN			2048
#define NSLOG_HOST_UNREACHABLE		4096

#define NSLOG_SERVICE_OK		8192
#define NSLOG_SERVICE_UNKNOWN		16384
#define NSLOG_SERVICE_WARNING		32768
#define NSLOG_SERVICE_CRITICAL		65536

#define NSLOG_PASSIVE_CHECK		131072

#define NSLOG_INFO_MESSAGE		262144

#define NSLOG_HOST_NOTIFICATION		524288
#define NSLOG_SERVICE_NOTIFICATION	1048576


/******************** HOST STATUS *********************/

#define HOST_UP				0
#define HOST_DOWN			1
#define HOST_UNREACHABLE		2	


/******************* STATE LOGGING TYPES **************/

#define INITIAL_STATES                  1
#define CURRENT_STATES                  2


/************ SERVICE DEPENDENCY VALUES ***************/

#define DEPENDENCIES_OK			0
#define DEPENDENCIES_FAILED		1


/*********** ROUTE CHECK PROPAGATION TYPES ************/

#define PROPAGATE_TO_PARENT_HOSTS	1
#define PROPAGATE_TO_CHILD_HOSTS	2


/****************** SERVICE STATES ********************/

#define STATE_OK			0
#define STATE_WARNING			1
#define STATE_CRITICAL			2
#define STATE_UNKNOWN			3       /* changed from -1 on 02/24/2001 */


/****************** FLAPPING TYPES ********************/

#define HOST_FLAPPING                   0
#define SERVICE_FLAPPING                1


/**************** NOTIFICATION TYPES ******************/

#define HOST_NOTIFICATION               0
#define SERVICE_NOTIFICATION            1


/************* NOTIFICATION REASON TYPES ***************/

#define NOTIFICATION_NORMAL             0
#define NOTIFICATION_ACKNOWLEDGEMENT    1
#define NOTIFICATION_FLAPPINGSTART      2
#define NOTIFICATION_FLAPPINGSTOP       3


/**************** EVENT HANDLER TYPES *****************/

#define HOST_EVENTHANDLER               0
#define SERVICE_EVENTHANDLER            1
#define GLOBAL_HOST_EVENTHANDLER        2
#define GLOBAL_SERVICE_EVENTHANDLER     3


/***************** STATE CHANGE TYPES *****************/

#define HOST_STATECHANGE                0
#define SERVICE_STATECHANGE             1


/******************* EVENT TYPES **********************/

#define EVENT_SERVICE_CHECK		0	/* active service check */
#define EVENT_COMMAND_CHECK		1	/* external command check */
#define EVENT_LOG_ROTATION		2	/* log file rotation */
#define EVENT_PROGRAM_SHUTDOWN		3	/* program shutdown */
#define EVENT_PROGRAM_RESTART		4	/* program restart */
#define EVENT_SERVICE_REAPER		5	/* reaps results from service checks */
#define EVENT_ORPHAN_CHECK		6	/* checks for orphaned service checks */
#define EVENT_RETENTION_SAVE		7	/* save (dump) retention data */
#define EVENT_STATUS_SAVE		8	/* save (dump) status data */
#define EVENT_SCHEDULED_DOWNTIME	9	/* scheduled host or service downtime */
#define EVENT_SFRESHNESS_CHECK          10      /* checks service result "freshness" */
#define EVENT_EXPIRE_DOWNTIME		11      /* checks for (and removes) expired scheduled downtime */
#define EVENT_HOST_CHECK                12      /* active host check */
#define EVENT_HFRESHNESS_CHECK          13      /* checks host result "freshness" */
#define EVENT_RESCHEDULE_CHECKS		14      /* adjust scheduling of host and service checks */
#define EVENT_EXPIRE_COMMENT            15      /* removes expired comments */
#define EVENT_SLEEP                     98      /* asynchronous sleep event that occurs when event queues are empty */
#define EVENT_USER_FUNCTION             99      /* USER-defined function (modules) */


/******* INTER-CHECK DELAY CALCULATION TYPES **********/

#define ICD_NONE			0	/* no inter-check delay */
#define ICD_DUMB			1	/* dumb delay of 1 second */
#define ICD_SMART			2	/* smart delay */
#define ICD_USER			3       /* user-specified delay */


/******* INTERLEAVE FACTOR CALCULATION TYPES **********/

#define ILF_USER			0	/* user-specified interleave factor */
#define ILF_SMART			1	/* smart interleave */


/************** SERVICE CHECK OPTIONS *****************/

#define CHECK_OPTION_NONE		0	/* no check options */
#define CHECK_OPTION_FORCE_EXECUTION	1	/* force execution of a service check (ignores disabled services, invalid timeperiods) */


/************ SCHEDULED DOWNTIME TYPES ****************/

#define ACTIVE_DOWNTIME                 0       /* active downtime - currently in effect */
#define PENDING_DOWNTIME                1       /* pending downtime - scheduled for the future */


/************* MACRO CLEANING OPTIONS *****************/

#define STRIP_ILLEGAL_MACRO_CHARS       1
#define ESCAPE_MACRO_CHARS              2
#define URL_ENCODE_MACRO_CHARS		4



/****************** DATA STRUCTURES *******************/

/* TIMED_EVENT structure */
typedef struct timed_event_struct{
	int event_type;
	time_t run_time;
	int recurring;
	unsigned long event_interval;
	int compensate_for_time_change;
	void *timing_func;
	void *event_data;
	void *event_args;
        struct timed_event_struct *next;
        }timed_event;


/* NOTIFY_LIST structure */
typedef struct notify_list_struct{
	contact *contact;
	struct notify_list_struct *next;
        }notification;


/* SERVICE_MESSAGE structure */
typedef struct service_message_struct{
	char host_name[MAX_HOSTNAME_LENGTH];		/* host name */
	char description[MAX_SERVICEDESC_LENGTH];	/* service description */
	int return_code;				/* plugin return code */
	int exited_ok;					/* did the plugin check return okay? */
	int check_type;					/* was this an active or passive service check? */
	int parallelized;                               /* was this check run in parallel? */
	struct timeval start_time;			/* time the service check was initiated */
	struct timeval finish_time;			/* time the service check was completed */
	int early_timeout;                              /* did the service check timeout? */
	char output[MAX_PLUGINOUTPUT_LENGTH];		/* plugin output */
	}service_message;


/* SCHED_INFO structure */
typedef struct sched_info_struct{
	int total_services;
	int total_scheduled_services;
	int total_hosts;
	int total_scheduled_hosts;
	double average_services_per_host;
	double average_scheduled_services_per_host;
	unsigned long service_check_interval_total;
	unsigned long host_check_interval_total;
	double average_service_check_interval;
	double average_host_check_interval;
	double average_service_inter_check_delay;
	double average_host_inter_check_delay;
	double service_inter_check_delay;
	double host_inter_check_delay;
	int service_interleave_factor;
	int max_service_check_spread;
	int max_host_check_spread;
	time_t first_service_check;
	time_t last_service_check;
	time_t first_host_check;
	time_t last_host_check;
        }sched_info;


/* PASSIVE_CHECK_RESULT structure */
typedef struct passive_check_result_struct{
	char *host_name;
	char *svc_description;
	int return_code;
	char *output;
	time_t check_time;
	struct passive_check_result_struct *next;
	}passive_check_result;


/* CIRCULAR_BUFFER structure - used by worker threads */
typedef struct circular_buffer_struct{
	void            **buffer;
	int             tail;
	int             head;
	int             items;
	int		high;		/* highest number of items that has ever been stored in buffer */
	unsigned long   overflow;
	pthread_mutex_t buffer_lock;
        }circular_buffer;


/* MMAPFILE structure - used for reading files via mmap() */
typedef struct mmapfile_struct{
	char *path;
	int mode;
	int fd;
	unsigned long file_size;
	unsigned long current_position;
	unsigned long current_line;
	void *mmap_buf;
        }mmapfile;



/* slots in circular buffers */
#define DEFAULT_EXTERNAL_COMMAND_BUFFER_SLOTS   4096
#define DEFAULT_CHECK_RESULT_BUFFER_SLOTS	4096

/* worker threads */
#define TOTAL_WORKER_THREADS              2

#define COMMAND_WORKER_THREAD		  0
#define SERVICE_WORKER_THREAD  		  1


/******************** FUNCTIONS **********************/

/**** Configuration Functions ****/
int read_main_config_file(char *);                     	/* reads the main config file (nagios.cfg) */
int read_resource_file(char *);				/* processes macros in resource file */
int read_all_object_data(char *);			/* reads all object config data */


/**** Setup Functions ****/
int pre_flight_check(void);                          	/* try and verify the configuration data */
void init_timing_loop(void);                         	/* setup the initial scheduling queue */
void setup_sighandler(void);                         	/* trap signals */
void reset_sighandler(void);                         	/* reset signals to default action */
int daemon_init(void);				     	/* switches to daemon mode */
int drop_privileges(char *,char *);			/* drops privileges before startup */
void display_scheduling_info(void);			/* displays service check scheduling information */


/**** IPC Functions ****/
int read_svc_message(service_message *);		/* reads a service check message from the message pipe */
int write_svc_message(service_message *);		/* writes a service check message to the message pipe */
int open_command_file(void);				/* creates the external command file as a named pipe (FIFO) and opens it for reading */
int close_command_file(void);				/* closes and deletes the external command file (FIFO) */


/**** Monitoring/Event Handler Functions ****/
int schedule_new_event(int,int,time_t,int,unsigned long,void *,int,void *,void *); /* schedules a new timed event */
void reschedule_event(timed_event *,timed_event **);   	/* reschedules an event */
int deschedule_event(int,int,void *,void *);            /* removes an event from the schedule */
void add_event(timed_event *,timed_event **);     	/* adds an event to the execution queue */
void remove_event(timed_event *,timed_event **);     	/* remove an event from the execution queue */
int event_execution_loop(void);                      	/* main monitoring/event handler loop */
int handle_timed_event(timed_event *);		     	/* top level handler for timed events */
void run_service_check(service *);			/* parallelized service check routine */
void reap_service_checks(void);				/* handles results from service checks */
int check_service_dependencies(service *,int);          /* checks service dependencies */
int check_host_dependencies(host *,int);                /* checks host dependencies */
void check_for_orphaned_services(void);			/* checks for orphaned services */
void check_service_result_freshness(void);              /* checks the "freshness" of service check results */
void check_host_result_freshness(void);                 /* checks the "freshness" of host check results */
void adjust_check_scheduling(void);		        /* auto-adjusts scheduling of host and service checks */
int my_system(char *,int,int *,double *,char *,int);            	/* executes a command via popen(), but also protects against timeouts */
void compensate_for_system_time_change(unsigned long,unsigned long);	/* attempts to compensate for a change in the system time */
void adjust_timestamp_for_time_change(time_t,time_t,unsigned long,time_t *); /* adjusts a timestamp variable for a system time change */
void resort_event_list(timed_event **);                 /* resorts event list by event run time for system time changes */


/**** Flap Detection Functions ****/
void check_for_service_flapping(service *,int);		/* determines whether or not a service is "flapping" between states */
void check_for_host_flapping(host *,int);			/* determines whether or not a host is "flapping" between states */
void set_service_flap(service *,double,double,double);		/* handles a service that is flapping */
void clear_service_flap(service *,double,double,double);	/* handles a service that has stopped flapping */
void set_host_flap(host *,double,double,double);		/* handles a host that is flapping */
void clear_host_flap(host *,double,double,double);		/* handles a host that has stopped flapping */
void enable_flap_detection_routines(void);		/* enables flap detection on a program-wide basis */
void disable_flap_detection_routines(void);		/* disables flap detection on a program-wide basis */
void enable_host_flap_detection(host *);		/* enables flap detection for a particular host */
void disable_host_flap_detection(host *);		/* disables flap detection for a particular host */
void enable_service_flap_detection(service *);		/* enables flap detection for a particular service */
void disable_service_flap_detection(service *);		/* disables flap detection for a particular service */


/**** Route/Host Check Functions ****/
int verify_route_to_host(host *,int);                   /* on-demand check of whether a host is up, down, or unreachable */
int run_scheduled_host_check(host *);			/* runs a scheduled host check */
int check_host(host *,int,int);                         /* checks if a host is up or down */
int run_host_check(host *,int);                 	/* runs a host check */
int handle_host_state(host *);               		/* top level host state handler */


/**** Event Handler Functions ****/
int obsessive_compulsive_service_check_processor(service *);	/* distributed monitoring craziness... */
int obsessive_compulsive_host_check_processor(host *);		/* distributed monitoring craziness... */
int handle_service_event(service *);				/* top level service event logic */
int run_service_event_handler(service *);			/* runs the event handler for a specific service */
int run_global_service_event_handler(service *);		/* runs the global service event handler */
int handle_host_event(host *);					/* top level host event logic */
int run_host_event_handler(host *);				/* runs the event handler for a specific host */
int run_global_host_event_handler(host *);			/* runs the global host event handler */


/**** Notification Functions ****/
int check_service_notification_viability(service *,int);		/* checks viability of notifying all contacts about a service */
int is_valid_escalation_for_service_notification(service *,serviceescalation *);	/* checks if an escalation entry is valid for a particular service notification */
int should_service_notification_be_escalated(service *);		/* checks if a service notification should be escalated */
int service_notification(service *,int,char *,char *);                        	/* notify all contacts about a service (problem or recovery) */
int check_contact_service_notification_viability(contact *,service *,int);	/* checks viability of notifying a contact about a service */ 
int notify_contact_of_service(contact *,service *,int,char *,char *,int);  		/* notify a single contact about a service */
int check_host_notification_viability(host *,int);			/* checks viability of notifying all contacts about a host */
int is_valid_host_escalation_for_host_notification(host *,hostescalation *);	/* checks if an escalation entry is valid for a particular host notification */
int should_host_notification_be_escalated(host *);			/* checks if a host notification should be escalated */
int host_notification(host *,int,char *,char *);                           	/* notify all contacts about a host (problem or recovery) */
int check_contact_host_notification_viability(contact *,host *,int);	/* checks viability of notifying a contact about a host */ 
int notify_contact_of_host(contact *,host *,int,char *,char *,int);            	/* notify a single contact about a host */
int create_notification_list_from_host(host *,int *);         	/* given a host, create list of contacts to be notified (remove duplicates) */
int create_notification_list_from_service(service *,int *);    	/* given a service, create list of contacts to be notified (remove duplicates) */
int add_notification(contact *);				/* adds a notification instance */
notification * find_notification(char *);			/* finds a notification object */
time_t get_next_host_notification_time(host *,time_t);		/* calculates nex acceptable re-notification time for a host */
time_t get_next_service_notification_time(service *,time_t);	/* calculates nex acceptable re-notification time for a service */


/**** Logging Functions ****/
int write_to_logs_and_console(char *,unsigned long,int); /* writes a string to screen and logs */
int write_to_console(char *);                           /* writes a string to screen */
int write_to_all_logs(char *,unsigned long);            /* writes a string to main log file and syslog facility */
int write_to_all_logs_with_timestamp(char *,unsigned long,time_t *);   /* writes a string to main log file and syslog facility */
int write_to_log(char *,unsigned long,time_t *);       	/* write a string to the main log file */
int write_to_syslog(char *,unsigned long);             	/* write a string to the syslog facility */
int log_service_event(service *);			/* logs a service event */
int log_host_event(host *);				/* logs a host event */
int log_host_states(int,time_t *);	                /* logs initial/current host states */
int log_service_states(int,time_t *);                   /* logs initial/current service states */
int rotate_log_file(time_t);			     	/* rotates the main log file */
int write_log_file_info(time_t *); 			/* records log file/version info */


/**** Cleanup Functions ****/
void cleanup(void);                                  	/* cleanup after ourselves (before quitting or restarting) */
void free_memory(void);                              	/* free memory allocated to all linked lists in memory */
int reset_variables(void);                           	/* reset all global variables */
void free_notification_list(void);		     	/* frees all memory allocated to the notification list */


/**** Hash Functions ****/
int hashfunc1(const char *name1, int hashslots);
int hashfunc2(const char *name1, const char *name2, int hashslots);
int compare_hashdata1(const char *,const char *);
int compare_hashdata2(const char *,const char *,const char *,const char *);


/**** Miscellaneous Functions ****/
void sighandler(int);                                	/* handles signals */
void service_check_sighandler(int);                     /* handles timeouts when executing service checks */
void my_system_sighandler(int);				/* handles timeouts when executing commands via my_system() */
void file_lock_sighandler(int);				/* handles timeouts while waiting for file locks */
void strip(char *);                                  	/* strips whitespace from string */  
char *my_strtok(char *,char *);                      	/* my replacement for strtok() function (doesn't skip consecutive tokens) */
char *my_strsep(char **,const char *);		     	/* Solaris doesn't have strsep(), so I took this from the glibc source code */
char *get_url_encoded_string(char *);			/* URL encode a string */
int contains_illegal_object_chars(char *);		/* tests whether or not an object name (host, service, etc.) contains illegal characters */
int my_rename(char *,char *);                           /* renames a file - works across filesystems */
void get_raw_command_line(char *,char *,int,int);      	/* given a raw command line, determine the actual command to run */
int check_time_against_period(time_t,char *);		/* check to see if a specific time is covered by a time period */
void get_next_valid_time(time_t, time_t *,char *);	/* get the next valid time in a time period */
void get_datetime_string(time_t *,char *,int,int);	/* get a date/time string for use in output */
time_t get_next_log_rotation_time(void);	     	/* determine the next time to schedule a log rotation */
int init_embedded_perl(char **);			/* initialized embedded perl interpreter */
int deinit_embedded_perl(void);				/* cleans up embedded perl */


/**** Macro Functions ****/
int process_macros(char *,char *,int,int);             	/* replace macros with their actual values */
char *clean_macro_chars(char *,int);                    /* cleans macros characters before insertion into output string */
int grab_service_macros(service *);                  	/* updates the service macro data */
int grab_host_macros(host *);                        	/* updates the host macro data */
int grab_contact_macros(contact *);                  	/* updates the contact macro data */
int grab_datetime_macros(void);				/* updates date/time macros */
int grab_summary_macros(contact *);			/* updates summary macros */
int grab_on_demand_macro(char *);                       /* fetches an on-demand macro */
int grab_on_demand_host_macro(host *,char *);		/* fetches an on-demand host macro */
int grab_on_demand_service_macro(service *,char *);     /* fetches an on-demand service macro */
int clear_argv_macros(void);                            /* clear all argv macros used in commands */
int clear_volatile_macros(void);                     	/* clear all "volatile" macros that change between service/host checks */
int clear_nonvolatile_macros(void);                  	/* clear all "nonvolatile" macros which remain relatively constant */


/**** External Command Functions ****/
void check_for_external_commands(void);			/* checks for any external commands */
void process_external_command(int,time_t,char *);	/* process an external command */
int process_host_command(int,time_t,char *);            /* process an external host command */
int process_hostgroup_command(int,time_t,char *);       /* process an external hostgroup command */
int process_service_command(int,time_t,char *);         /* process an external service command */
int process_servicegroup_command(int,time_t,char *);    /* process an external servicegroup command */


/**** External Command Implementations ****/
int cmd_add_comment(int,time_t,char *);			/* add a service or host comment */
int cmd_delete_comment(int,char *);			/* delete a service or host comment */
int cmd_delete_all_comments(int,char *);		/* delete all comments associated with a host or service */
int cmd_delay_notification(int,char *);			/* delay a service or host notification */
int cmd_schedule_service_check(int,char *,int);		/* schedule an immediate or delayed service check */
int cmd_schedule_check(int,char *);			/* schedule an immediate or delayed host check */
int cmd_schedule_host_service_checks(int,char *,int);	/* schedule an immediate or delayed checks of all services on a host */
int cmd_signal_process(int,char *);				/* schedules a program shutdown or restart */
int cmd_process_service_check_result(int,time_t,char *);	/* processes a passive service check */
int cmd_process_host_check_result(int,time_t,char *);		/* processes a passive host check */
int cmd_acknowledge_problem(int,char *);			/* acknowledges a host or service problem */
int cmd_remove_acknowledgement(int,char *);			/* removes a host or service acknowledgement */
int cmd_schedule_downtime(int,time_t,char *);                   /* schedules host or service downtime */
int cmd_delete_downtime(int,char *);				/* cancels active/pending host or service scheduled downtime */
int cmd_change_command(int,char *);				/* changes host/svc command */
int cmd_change_check_interval(int,char *);			/* changes host/svc check interval */
int cmd_change_max_attempts(int,char *);			/* changes host/svc max attempts */

int process_passive_service_check(time_t,char *,char *,int,char *);
int process_passive_host_check(time_t,char *,int,char *);


/**** Internal Command Implementations ****/
void disable_service_checks(service *);			/* disables a service check */
void enable_service_checks(service *);			/* enables a service check */
void schedule_service_check(service *,time_t,int);	/* schedules an immediate or delayed service check */
void schedule_host_check(host *,time_t,int);		/* schedules an immediate or delayed host check */
void enable_all_notifications(void);                    /* enables notifications on a program-wide basis */
void disable_all_notifications(void);                   /* disables notifications on a program-wide basis */
void enable_service_notifications(service *);		/* enables service notifications */
void disable_service_notifications(service *);		/* disables service notifications */
void enable_host_notifications(host *);			/* enables host notifications */
void disable_host_notifications(host *);		/* disables host notifications */
void enable_and_propagate_notifications(host *,int,int,int,int);	/* enables notifications for all hosts and services beyond a given host */
void disable_and_propagate_notifications(host *,int,int,int,int);	/* disables notifications for all hosts and services beyond a given host */
void schedule_and_propagate_downtime(host *,time_t,char *,char *,time_t,time_t,int,unsigned long,unsigned long); /* schedules downtime for all hosts beyond a given host */
void acknowledge_host_problem(host *,char *,char *,int,int,int);	/* acknowledges a host problem */
void acknowledge_service_problem(service *,char *,char *,int,int,int);	/* acknowledges a service problem */
void remove_host_acknowledgement(host *);		/* removes a host acknowledgement */
void remove_service_acknowledgement(service *);		/* removes a service acknowledgement */
void start_executing_service_checks(void);		/* starts executing service checks */
void stop_executing_service_checks(void);		/* stops executing service checks */
void start_accepting_passive_service_checks(void);	/* starts accepting passive service check results */
void stop_accepting_passive_service_checks(void);	/* stops accepting passive service check results */
void enable_passive_service_checks(service *);	        /* enables passive service checks for a particular service */
void disable_passive_service_checks(service *);         /* disables passive service checks for a particular service */
void start_using_event_handlers(void);			/* enables event handlers on a program-wide basis */
void stop_using_event_handlers(void);			/* disables event handlers on a program-wide basis */
void enable_service_event_handler(service *);		/* enables the event handler for a particular service */
void disable_service_event_handler(service *);		/* disables the event handler for a particular service */
void enable_host_event_handler(host *);			/* enables the event handler for a particular host */
void disable_host_event_handler(host *);		/* disables the event handler for a particular host */
void enable_host_checks(host *);			/* enables checks of a particular host */
void disable_host_checks(host *);			/* disables checks of a particular host */
void start_obsessing_over_service_checks(void);		/* start obsessing about service check results */
void stop_obsessing_over_service_checks(void);		/* stop obsessing about service check results */
void start_obsessing_over_host_checks(void);		/* start obsessing about host check results */
void stop_obsessing_over_host_checks(void);		/* stop obsessing about host check results */
void enable_service_freshness_checks(void);		/* enable service freshness checks */
void disable_service_freshness_checks(void);		/* disable service freshness checks */
void enable_host_freshness_checks(void);		/* enable host freshness checks */
void disable_host_freshness_checks(void);		/* disable host freshness checks */
void process_passive_service_checks(void);              /* processes passive service check results */
void enable_all_failure_prediction(void);               /* enables failure prediction on a program-wide basis */
void disable_all_failure_prediction(void);              /* disables failure prediction on a program-wide basis */
void enable_performance_data(void);                     /* enables processing of performance data on a program-wide basis */
void disable_performance_data(void);                    /* disables processing of performance data on a program-wide basis */
void start_executing_host_checks(void);			/* starts executing host checks */
void stop_executing_host_checks(void);			/* stops executing host checks */
void start_accepting_passive_host_checks(void);		/* starts accepting passive host check results */
void stop_accepting_passive_host_checks(void);		/* stops accepting passive host check results */
void enable_passive_host_checks(host *);	        /* enables passive host checks for a particular host */
void disable_passive_host_checks(host *);         	/* disables passive host checks for a particular host */
void start_obsessing_over_service(service *);		/* start obsessing about specific service check results */
void stop_obsessing_over_service(service *);		/* stop obsessing about specific service check results */
void start_obsessing_over_host(host *);			/* start obsessing about specific host check results */
void stop_obsessing_over_host(host *);			/* stop obsessing about specific host check results */
void set_host_notification_number(host *,int);		/* sets current notification number for a specific host */
void set_service_notification_number(service *,int);	/* sets current notification number for a specific service */

int init_service_result_worker_thread(void);
int shutdown_service_result_worker_thread(void);
void * service_result_worker_thread(void *);
void cleanup_service_result_worker_thread(void *);

int init_command_file_worker_thread(void);
int shutdown_command_file_worker_thread(void);
void * command_file_worker_thread(void *);
void cleanup_command_file_worker_thread(void *);

int submit_external_command(char *,int *);
int submit_raw_external_command(char *,time_t *,int *);

char *get_program_version(void);
char *get_program_modification_date(void);

mmapfile *mmap_fopen(char *);				/* open a file read-only via mmap() */
int mmap_fclose(mmapfile *);
char *mmap_fgets(mmapfile *);
char *mmap_fgets_multiline(mmapfile *);


int init_macrox_names(void);
int add_macrox_name(int,char *);
int free_macrox_names(void);
int set_all_macro_environment_vars(int);
int set_macrox_environment_vars(int);
int set_argv_macro_environment_vars(int);
int set_macro_environment_var(char *,char *,int);

#ifdef __cplusplus
}
#endif
#endif

