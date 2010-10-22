/*****************************************************************************
* 
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
* 
* 
*****************************************************************************/

#define NSCORE 1
#include "../base/checks.c"
#include "tap.h"

/* Test specific functions + variables */
service *svc1=NULL, *svc2=NULL;
host *host1=NULL;
int found_log_rechecking_host_when_service_wobbles=0;
int found_log_run_async_host_check_3x=0;

/* Loads of variables + stubbed functions */
char *config_file="etc/nagios.cfg";
int      test_scheduling;

time_t   program_start;
time_t   event_start;
time_t   last_command_check;

int      sigshutdown=FALSE;
int      sigrestart=FALSE;

double   sleep_time;
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

int      aggregate_status_updates;
int      status_update_interval;

int      log_rotation_method;

int      service_check_timeout;

int      execute_service_checks=1;
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
void get_time_breakdown(unsigned long long1, int *int1, int *int2, int *int3, int *int4) {}
int check_for_external_commands(void) {}
int check_time_against_period(time_t time_t1, timeperiod *timeperiod) {
}
time_t get_next_log_rotation_time(void) {}
int handle_scheduled_downtime_by_id(unsigned long long1) {}
int rotate_log_file(time_t time_t1) {}
time_t get_next_host_notification_time(host *temp_host,time_t time_t1) {}
void get_next_valid_time(time_t time_t1, time_t *time_t2,timeperiod *temp_timeperiod) {}
void logit(int int1,int int2,const char *fmt, ...) {}


int c=0;
int update_program_status(int aggregated_dump){ 
	c++;
	/* printf ("# In the update_program_status hook: %d\n", c); */
	
	/* Set this to break out of event_execution_loop */
	if (c>10) {
		sigshutdown=TRUE;
		c=0;
	}
} 
int update_service_status(service *svc,int aggregated_dump){}
int update_all_status_data(void){}
int log_debug_info(int level, int verbosity, const char *fmt, ...){
	va_list ap;
    char *buffer=NULL;

	va_start(ap, fmt);
	/* vprintf( fmt, ap ); */
    vasprintf(&buffer,fmt,ap);
    if(strcmp(buffer,"Service wobbled between non-OK states, so we'll recheck the host state...\n")==0) {
        found_log_rechecking_host_when_service_wobbles++;
    }
    if(strcmp(buffer,"run_async_host_check_3x()\n")==0) {
        found_log_run_async_host_check_3x++;
    }
    free(buffer);
	va_end(ap);
}
int update_host_status(host *hst,int aggregated_dump){}

char    *check_result_path=NULL;
int process_check_result_queue(char *dirname){}
service * find_service(char *host_name,char *svc_desc){}
int delete_check_result_file(char *fname){}
int free_check_result(check_result *info){}
host * find_host(char *name){}
int             max_check_reaper_time=DEFAULT_MAX_REAPER_TIME;
check_result *read_check_result(void){}
int broker_service_check(int type, int flags, int attr, service *svc, int check_type, struct timeval start_time, struct timeval end_time, char *cmd, double latency, double exectime, int timeout, int early_timeout, int retcode, char *cmdline, struct timeval *timestamp){}
int grab_service_macros(service *svc){}
int get_raw_command_line(command *cmd_ptr, char *cmd, char **full_command, int macro_options){}
check_result    check_result_info;
int clear_volatile_macros(void){}
int grab_host_macros(host *hst){}
int process_macros(char *input_buffer, char **output_buffer, int options){}
char *temp_path;
int dbuf_init(dbuf *db, int chunk_size){}
int update_check_stats(int check_type, time_t check_time){}
int set_all_macro_environment_vars(int set){}
int close_command_file(void){}
void reset_sighandler(void){}
void service_check_sighandler(int sig){}
unsigned long   max_debug_file_size=DEFAULT_MAX_DEBUG_FILE_SIZE;
int             host_check_timeout=DEFAULT_HOST_CHECK_TIMEOUT;
int broker_host_check(int type, int flags, int attr, host *hst, int check_type, int state, int state_type, struct timeval start_time, struct timeval end_time, char *cmd, double latency, double exectime, int timeout, int early_timeout, int retcode, char *cmdline, char *output, char *long_output, char *perfdata, struct timeval *timestamp){}
char *escape_newlines(char *rawbuf){}
int dbuf_strcat(dbuf *db, char *buf){}
int dbuf_free(dbuf *db){}
unsigned long   next_event_id=0L;
unsigned long   next_problem_id=0L;
int move_check_result_to_queue(char *checkresult_file){}
int             free_child_process_memory=-1;
void free_memory(void){}
int accept_passive_service_checks=TRUE;
int parse_check_output(char *buf, char **short_output, char **long_output, char **perf_data, int escape_newlines_please, int newlines_are_escaped){}
int log_passive_checks=TRUE;
int use_aggressive_host_checking=FALSE;
int handle_service_event(service *svc){}
int delete_service_acknowledgement_comments(service *svc){}
unsigned long cached_host_check_horizon=DEFAULT_CACHED_HOST_CHECK_HORIZON;
int log_service_event(service *svc){}
void check_for_service_flapping(service *svc, int update, int allow_flapstart_notification){}
void check_for_host_flapping(host *hst, int update, int actual_check, int allow_flapstart_notification){}
int service_notification(service *svc, int type, char *not_author, char *not_data, int options){}
int obsess_over_services=FALSE;
int obsessive_compulsive_service_check_processor(service *svc){}
int host_notification(host *hst, int type, char *not_author, char *not_data, int options){}
int             enable_predictive_service_dependency_checks=DEFAULT_ENABLE_PREDICTIVE_SERVICE_DEPENDENCY_CHECKS;
servicedependency *get_first_servicedependency_by_dependent_service(char *host_name, char *svc_description, void **ptr){}
servicedependency *get_next_servicedependency_by_dependent_service(char *host_name, char *svc_description, void **ptr){}
int add_object_to_objectlist(objectlist **list, void *object_ptr){}
int check_pending_flex_service_downtime(service *svc){}
int compare_strings(char *val1a, char *val2a){}
int update_service_performance_data(service *svc){}
int free_objectlist(objectlist **temp_list){}
unsigned long   cached_service_check_horizon=DEFAULT_CACHED_SERVICE_CHECK_HORIZON;
timed_event *event_list_low=NULL;
timed_event *event_list_low_tail=NULL;
void remove_event(timed_event *event, timed_event **event_list, timed_event **event_list_tail){}
void reschedule_event(timed_event *event, timed_event **event_list, timed_event **event_list_tail){}
int process_passive_service_check(time_t check_time, char *host_name, char *svc_description, int return_code, char *output){}
void process_passive_checks(void){}
int             soft_state_dependencies=FALSE;
int             additional_freshness_latency=DEFAULT_ADDITIONAL_FRESHNESS_LATENCY;
hostdependency *get_first_hostdependency_by_dependent_host(char *host_name, void **ptr){ 
    /* Return NULL so check_host_dependencies returns back */
    return NULL;
}
hostdependency *get_next_hostdependency_by_dependent_host(char *host_name, void **ptr){}
int             currently_running_host_checks=0;
int my_system(char *cmd,int timeout,int *early_timeout,double *exectime,char **output,int max_output_length){}
void host_check_sighandler(int sig){}
int             accept_passive_host_checks=TRUE;
int             passive_host_checks_are_soft=DEFAULT_PASSIVE_HOST_CHECKS_SOFT;
int             translate_passive_host_checks=DEFAULT_TRANSLATE_PASSIVE_HOST_CHECKS;
int             enable_predictive_host_dependency_checks=DEFAULT_ENABLE_PREDICTIVE_HOST_DEPENDENCY_CHECKS;
int log_host_event(host *hst){}
int handle_host_state(host *hst){}


void
setup_objects(time_t time) {
    timed_event *new_event=NULL;

    host1=(host *)malloc(sizeof(host));
    host1->current_state=HOST_DOWN;
    host1->has_been_checked=TRUE;
    host1->last_check=time;

    /* First service is a normal one */
    svc1=(service *)malloc(sizeof(service));
    svc1->host_name=strdup("Host1");
    svc1->host_ptr=host1;
    svc1->description=strdup("Normal service");
    svc1->check_options=0;
    svc1->next_check=time;
    svc1->state_type=SOFT_STATE;
    svc1->current_state=STATE_CRITICAL;
    svc1->retry_interval=1;
    svc1->check_interval=5;
    svc1->last_state_change=0;
    svc1->last_state_change=0;

    /* Second service .... to be configured! */
    svc2=(service *)malloc(sizeof(service));
    svc2->host_name=strdup("Host1");
    svc2->description=strdup("To be nudged");
    svc2->check_options=0;
    svc2->next_check=time;
    svc2->state_type=SOFT_STATE;
    svc2->current_state=STATE_OK;
    svc2->retry_interval=1;
    svc2->check_interval=5;

}

int
main (int argc, char **argv)
{
    time_t now=0L;
    check_result *tmp_check_result;


	plan_tests(5);

	time(&now);

    
    /* Test to confirm that if a service is warning, the notified_on_critical is reset */
    tmp_check_result=(check_result *)malloc(sizeof(check_result));
    tmp_check_result->host_name=strdup("host1");
    tmp_check_result->service_description=strdup("Normal service");
    tmp_check_result->object_check_type=SERVICE_CHECK;
    tmp_check_result->check_type=SERVICE_CHECK_ACTIVE;
    tmp_check_result->check_options=0;
    tmp_check_result->scheduled_check=TRUE;
    tmp_check_result->reschedule_check=TRUE;
    tmp_check_result->latency=0.666;
    tmp_check_result->start_time.tv_sec=1234567890;
    tmp_check_result->start_time.tv_usec=56565;
    tmp_check_result->finish_time.tv_sec=1234567899;
    tmp_check_result->finish_time.tv_usec=45454;
    tmp_check_result->early_timeout=0;
    tmp_check_result->exited_ok=TRUE;
    tmp_check_result->return_code=1;
    tmp_check_result->output=strdup("Warning - check notified_on_critical reset");

    setup_objects(now);
    svc1->last_state=STATE_CRITICAL;
    svc1->notified_on_critical=TRUE;
    svc1->current_notification_number=999;
    svc1->last_notification=(time_t)11111;
    svc1->next_notification=(time_t)22222;
    svc1->no_more_notifications=TRUE;

    handle_async_service_check_result( svc1, tmp_check_result );

    ok( svc1->notified_on_critical==FALSE, "notified_on_critical reset" );
    ok( svc1->last_notification==(time_t)0, "last notification reset due to state change" );
    ok( svc1->next_notification==(time_t)0, "next notification reset due to state change" );
    ok( svc1->no_more_notifications==FALSE, "no_more_notifications reset due to state change" );
    ok( svc1->current_notification_number==999, "notification number NOT reset" );

	return exit_status ();
}

