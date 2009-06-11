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
#include "config.h"
#include "common.h"
#include "downtime.h"
#include "comments.h"
#include "statusdata.h"
#include "nagios.h"
#include "broker.h"
#include "sretention.h"
#include "tap.h"

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


extern timed_event *event_list_low;
extern timed_event *event_list_low_tail;
extern timed_event *event_list_high;
extern timed_event *event_list_high_tail;

host     *host_list;
service  *service_list;

int check_for_expired_comment(unsigned long temp_long) {}
void broker_timed_event(int int1, int int2, int int3, timed_event *timed_event1, struct timeval *timeval1) {}
int perform_scheduled_host_check(host *temp_host,int int1,double double1) {}
int check_for_expired_downtime(void) {}
int reap_check_results(void) {}
void check_host_result_freshness() {}
int check_for_nagios_updates(int int1, int int2) {}
time_t get_next_service_notification_time(service *temp_service, time_t time_t1) {}
int save_state_information(int int1) {}
void get_time_breakdown(unsigned long long1, int *int1, int *int2, int *int3, int *int4) {}
int check_for_external_commands(void) {}
void check_for_orphaned_hosts() {}
void check_service_result_freshness() {}
int check_time_against_period(time_t time_t1, timeperiod *timeperiod) {}
time_t get_next_log_rotation_time(void) {}
void check_for_orphaned_services() {}
int run_scheduled_service_check(service *service1, int int1, double double1) {
	currently_running_service_checks++;
	/* printf("Currently running service checks: %d\n", currently_running_service_checks); */
}
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
	va_start(ap, fmt);
	/* vprintf( fmt, ap ); */
	va_end(ap);
}
int update_host_status(host *hst,int aggregated_dump){}

/* Test variables */
service *svc1=NULL, *svc2=NULL;

void
setup_events(time_t time) {
	timed_event *new_event=NULL;

	/* First service is a normal one */
	svc1=(service *)malloc(sizeof(service));
	svc1->host_name=strdup("Host1");
	svc1->description=strdup("Normal service");
	svc1->check_options=0;
	svc1->next_check=time;
	svc1->state_type=SOFT_STATE;
	svc1->current_state=STATE_OK;
	svc1->retry_interval=1;
	svc1->check_interval=5;

	new_event=(timed_event *)malloc(sizeof(timed_event));
	new_event->event_type=EVENT_SERVICE_CHECK;
	new_event->event_data=(void *)svc1;
	new_event->event_args=(void *)NULL;
	new_event->event_options=0;
	new_event->run_time=0L;				/* Straight away */
	new_event->recurring=FALSE;
	new_event->event_interval=0L;
	new_event->timing_func=NULL;
	new_event->compensate_for_time_change=TRUE;
	reschedule_event(new_event,&event_list_low,&event_list_low_tail);
	
	/* Second service is one that will get nudged forward */
	svc2=(service *)malloc(sizeof(service));
	svc2->host_name=strdup("Host1");
	svc2->description=strdup("To be nudged");
	svc2->check_options=0;
	svc2->next_check=time;
	svc2->state_type=SOFT_STATE;
	svc2->current_state=STATE_OK;
	svc2->retry_interval=1;
	svc2->check_interval=5;

	new_event=(timed_event *)malloc(sizeof(timed_event));
	new_event->event_type=EVENT_SERVICE_CHECK;
	new_event->event_data=(void *)svc2;
	new_event->event_args=(void *)NULL;
	new_event->event_options=0;
	new_event->run_time=0L;				/* Straight away */
	new_event->recurring=FALSE;
	new_event->event_interval=0L;
	new_event->timing_func=NULL;
	new_event->compensate_for_time_change=TRUE;
	reschedule_event(new_event,&event_list_low,&event_list_low_tail);
}

int
main (int argc, char **argv)
{
	time_t now=0L;

	plan_tests(6);

	time(&now);

	currently_running_service_checks=0;
	max_parallel_service_checks=1;
	setup_events(now);
	event_execution_loop();

	ok(svc1->next_check == now, "svc1 has not had its next check time changed" );
	printf("# Nudge amount: %d\n", svc2->next_check - now );
	ok(svc2->next_check > now+5 && svc2->next_check < now+5+10, "svc2 has been nudged" );


	sigshutdown=FALSE;
	currently_running_service_checks=0;
	max_parallel_service_checks=2;
	setup_events(now);
	event_execution_loop();
	ok(svc1->next_check == now, "svc1 has not had its next check time changed" );
	printf("# Nudge amount: %d\n", svc2->next_check - now );
	ok(svc2->next_check == now, "svc2 also has not changed, because can execute" );


	/* This test should have both services moved forward due to not executing any service checks */
	/* Will make svc2 move forward by a retry_interval amount */
	execute_service_checks=0;
	sigshutdown=FALSE;
	currently_running_service_checks=0;
	max_parallel_service_checks=1;
	setup_events(now);
	svc2->current_state=STATE_CRITICAL;
	event_execution_loop();
	ok(svc1->next_check == now+300, "svc1 rescheduled ahead - normal interval" );
	ok(svc2->next_check == now+60,  "svc2 rescheduled ahead - retry interval" );


	return exit_status ();
}

