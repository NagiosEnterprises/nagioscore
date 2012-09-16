#ifndef NAGIOS_TEST_STUBS__
#define NAGIOS_TEST_STUBS__
#include "macros.h"

/* Loads of variables + stubbed functions */
char *config_file = "etc/nagios.cfg";
int      test_scheduling;

time_t   program_start;
time_t   event_start;

int      sigshutdown = FALSE;
int      sigrestart = FALSE;

int      interval_length = 60;
int      service_inter_check_delay_method;
int      host_inter_check_delay_method;
int      service_interleave_factor_method;
int      max_host_check_spread;
int      max_service_check_spread;

int      command_check_interval;
int      check_reaper_interval;
int      service_freshness_check_interval;
int      host_freshness_check_interval;
int      auto_rescheduling_interval;
int      host_freshness_check_interval;
int      auto_rescheduling_interval;
int      auto_rescheduling_window;

int      check_external_commands;
int      check_orphaned_services;
int      check_orphaned_hosts;
int      check_service_freshness;
int      check_host_freshness;
int      auto_reschedule_checks;

int      retain_state_information;
int      retention_update_interval;

int      max_parallel_service_checks;
int      currently_running_service_checks;

int      status_update_interval;

int      log_rotation_method;

int      service_check_timeout;

int      execute_service_checks = 1;
int      execute_host_checks;

int      child_processes_fork_twice;

int      time_change_threshold;


host     *host_list;
service  *service_list;

int check_for_expired_comment(unsigned long temp_long) {}
void broker_timed_event(int int1, int int2, int int3, timed_event *timed_event1, struct timeval *timeval1) {}
int check_for_expired_downtime(void) {}
int check_for_nagios_updates(int int1, int int2) {}
time_t get_next_service_notification_time(service *temp_service, time_t time_t1) {}
int save_state_information(int int1) {}
int check_for_external_commands(void) {}
int check_time_against_period(time_t time_t1, timeperiod *timeperiod) {}
time_t get_next_log_rotation_time(void) {}
int handle_scheduled_downtime_by_id(unsigned long long1) {}
#ifndef TEST_LOGGING
int log_host_event(host *hst) {}
int log_service_event_flag = 0;
int log_service_event(service *svc) {
	log_service_event_flag++;
	}
int rotate_log_file(time_t time_t1) {}
void logit(int int1, int int2, const char *fmt, ...) {}
#endif
time_t get_next_host_notification_time(host *temp_host, time_t time_t1) {}
void get_next_valid_time(time_t time_t1, time_t *time_t2, timeperiod *temp_timeperiod) {}


char *log_file = "var/nagios.log";
char *temp_file = "";
char *log_archive_path = "var";
host *host_list = NULL;
service *service_list = NULL;
int use_syslog = 0;
int log_service_retries;
int log_initial_states;
unsigned long logging_options = NSLOG_PROCESS_INFO;
unsigned long syslog_options;
int verify_config;
int test_scheduling;
time_t last_log_rotation;
int log_rotation_method;
int daemon_mode = TRUE;
char *debug_file = "";
int debug_level;
int debug_verbosity;
unsigned long max_debug_file_size;

int grab_host_macros_r(nagios_macros *mac, host *hst) {}

int grab_service_macros_r(nagios_macros *mac, service *svc) {}

void broker_log_data(int a, int b, int c, char *d, unsigned long e, time_t f, struct timeval *g) {}

int clear_volatile_macros_r(nagios_macros *mac) {}
int clear_service_macros_r(nagios_macros *mac) {}
int clear_host_macros_r(nagios_macros *mac) {}

int process_macros(char *a, char **b, int c) {}
int process_macros_r(nagios_macros *mac, char *a, char **b, int c) {}

int update_host_status(host *hst, int aggregated_dump) {}
int update_service_status(service *svc, int aggregated_dump) {}
int update_all_status_data(void) {}
char    *check_result_path = NULL;
int process_check_result_queue(char *dirname) {}
service * find_service(char *host_name, char *svc_desc) {}
int delete_check_result_file(char *fname) {}
int free_check_result(check_result *info) {}
host * find_host(char *name) {}
int             max_check_reaper_time = DEFAULT_MAX_REAPER_TIME;
check_result *read_check_result(void) {}
int broker_service_check(int type, int flags, int attr, service *svc, int check_type, struct timeval start_time, struct timeval end_time, char *cmd, double latency, double exectime, int timeout, int early_timeout, int retcode, char *cmdline, struct timeval *timestamp) {}
int get_raw_command_line(command *a, char *b, char **c, int d) {}
int get_raw_command_line_r(nagios_macros *mac, command *a, char *b, char **c, int d) {}
char *temp_path;
int dbuf_init(dbuf *db, int chunk_size) {}
int update_check_stats(int check_type, time_t check_time) {}
int set_all_macro_environment_vars_r(nagios_macros *mac, int set) {}
int close_command_file(void) {}
void reset_sighandler(void) {}
unsigned long   max_debug_file_size = DEFAULT_MAX_DEBUG_FILE_SIZE;
int             host_check_timeout = DEFAULT_HOST_CHECK_TIMEOUT;
int broker_host_check(int type, int flags, int attr, host *hst, int check_type, int state, int state_type, struct timeval start_time, struct timeval end_time, char *cmd, double latency, double exectime, int timeout, int early_timeout, int retcode, char *cmdline, char *output, char *long_output, char *perfdata, struct timeval *timestamp) {}
char *escape_newlines(char *rawbuf) {}
int dbuf_strcat(dbuf *db, char *buf) {}
int dbuf_free(dbuf *db) {}
unsigned long   next_event_id = 0L;
unsigned long   next_problem_id = 0L;
int             free_child_process_memory = -1;
void free_memory(nagios_macros *mac) {}
int accept_passive_service_checks = TRUE;
int log_passive_checks = TRUE;
int use_aggressive_host_checking = FALSE;
int handle_service_event(service *svc) {}
unsigned long cached_host_check_horizon = DEFAULT_CACHED_HOST_CHECK_HORIZON;
void check_for_service_flapping(service *svc, int update, int allow_flapstart_notification) {}
void check_for_host_flapping(host *hst, int update, int actual_check, int allow_flapstart_notification) {}
int service_notification(service *svc, int type, char *not_author, char *not_data, int options) {}
int obsess_over_services = FALSE;
int obsessive_compulsive_service_check_processor(service *svc) {}
int host_notification(host *hst, int type, char *not_author, char *not_data, int options) {}
int             enable_predictive_service_dependency_checks = DEFAULT_ENABLE_PREDICTIVE_SERVICE_DEPENDENCY_CHECKS;
servicedependency *get_first_servicedependency_by_dependent_service(char *host_name, char *svc_description, void **ptr) {}
servicedependency *get_next_servicedependency_by_dependent_service(char *host_name, char *svc_description, void **ptr) {}
int add_object_to_objectlist(objectlist **list, void *object_ptr) {}
int check_pending_flex_service_downtime(service *svc) {}
int compare_strings(char *val1a, char *val2a) {}
int update_service_performance_data(service *svc) {}
int free_objectlist(objectlist **temp_list) {}
unsigned long   cached_service_check_horizon = DEFAULT_CACHED_SERVICE_CHECK_HORIZON;
timed_event *event_list_low = NULL;
timed_event *event_list_low_tail = NULL;
void remove_event(timed_event *event, timed_event **event_list, timed_event **event_list_tail) {}
void reschedule_event(timed_event *event, timed_event **event_list, timed_event **event_list_tail) {}
int process_passive_service_check(time_t check_time, char *host_name, char *svc_description, int return_code, char *output) {}
int             soft_state_dependencies = FALSE;
int             additional_freshness_latency = DEFAULT_ADDITIONAL_FRESHNESS_LATENCY;
hostdependency *get_first_hostdependency_by_dependent_host(char *host_name, void **ptr) {
	/* Return NULL so check_host_dependencies returns back */
	return NULL;
	}
hostdependency *get_next_hostdependency_by_dependent_host(char *host_name, void **ptr) {}
int             currently_running_host_checks = 0;
int my_system_r(nagios_macros *mac, char *cmd, int timeout, int *early_timeout, double *exectime, char **output, int max_output_length) {}
int             accept_passive_host_checks = TRUE;
int             passive_host_checks_are_soft = DEFAULT_PASSIVE_HOST_CHECKS_SOFT;
int             translate_passive_host_checks = DEFAULT_TRANSLATE_PASSIVE_HOST_CHECKS;
int             enable_predictive_host_dependency_checks = DEFAULT_ENABLE_PREDICTIVE_HOST_DEPENDENCY_CHECKS;


#endif
