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

#define TEST_EVENTS_C

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
#include "stub_logging.c"
#include "stub_broker.c"
#include "stub_sretention.c"
#include "stub_statusdata.c"
#include "stub_downtime.c"
#include "stub_comments.c"
#include "stub_notifications.c"
#include "stub_workers.c"
#include "stub_nebmods.c"
#include "stub_netutils.c"
#include "stub_commands.c"
#include "stub_flapping.c"
#include "stub_sehandlers.c"
#include "stub_perfdata.c"
#include "stub_nsock.c"
#include "stub_iobroker.c"

int perform_scheduled_host_check(host *temp_host, int int1, double double1) {
	time_t now = 0L;
	time(&now);
	temp_host->last_check = now;
	}

#if 0
int run_scheduled_service_check(service *service1, int int1, double double1) {
	currently_running_service_checks++;
	time_t now = 0L;
	time(&now);
	service1->last_check = now;
	/* printf("Currently running service checks: %d\n", currently_running_service_checks); */
	}
#endif

int c = 0;
int update_program_status(int aggregated_dump) {
	c++;
	/* printf ("# In the update_program_status hook: %d\n", c); */

	/* Set this to break out of event_execution_loop */
	if(c > 10) {
		sigshutdown = TRUE;
		c = 0;
		}
	}

/* Test variables */
service *svc1 = NULL, *svc2 = NULL, *svc3 = NULL;
host *host1 = NULL;

void test_event_process_svc1_results(void *args) {
	check_result *tmp_check_result = (check_result *)args;
	service *svc = find_service(tmp_check_result->host_name,
			tmp_check_result->service_description);
	handle_async_service_check_result(svc, tmp_check_result);
}

void
setup_events(time_t time) {
	timed_event *new_event = NULL;

	/* First service is a normal one */
	svc1 = find_service("host1", "Normal service");
	svc1->check_options = 0;
	svc1->next_check = time;
	svc1->state_type = SOFT_STATE;
	svc1->current_state = STATE_OK;
	svc1->retry_interval = 1;
	svc1->check_interval = 5;

	new_event = (timed_event *)malloc(sizeof(timed_event));
	new_event->event_type = EVENT_SERVICE_CHECK;
	new_event->event_data = (void *)svc1;
	new_event->event_args = (void *)NULL;
	new_event->event_options = 0;
	new_event->priority = 0;
	new_event->run_time = svc1->next_check;
	new_event->recurring = FALSE;
	new_event->event_interval = 0L;
	new_event->timing_func = NULL;
	new_event->compensate_for_time_change = TRUE;
	new_event->sq_event = NULL;
	reschedule_event(nagios_squeue, new_event);

	/* Second service is one that will get nudged forward */
	svc2 = find_service("host1", "To be nudged");
	svc2->check_options = 0;
	svc2->next_check = time;
	svc2->state_type = SOFT_STATE;
	svc2->current_state = STATE_OK;
	svc2->retry_interval = 1;
	svc2->check_interval = 5;

	new_event = (timed_event *)malloc(sizeof(timed_event));
	new_event->event_type = EVENT_SERVICE_CHECK;
	new_event->event_data = (void *)svc2;
	new_event->event_args = (void *)NULL;
	new_event->event_options = 0;
	new_event->priority = 0;
	new_event->run_time = svc2->next_check;
	new_event->recurring = FALSE;
	new_event->event_interval = 0L;
	new_event->timing_func = NULL;
	new_event->compensate_for_time_change = TRUE;
	new_event->sq_event = NULL;
	reschedule_event(nagios_squeue, new_event);

}

setup_svc1_result_events(time_t time) {
	timed_event *new_event = NULL;
	check_result *tmp_check_result = NULL;

	/* Results for first check */
	tmp_check_result = (check_result *)calloc(1, sizeof(check_result));
	tmp_check_result->host_name = strdup("host1");
	tmp_check_result->service_description = strdup("Normal service");
	tmp_check_result->object_check_type = SERVICE_CHECK;
	tmp_check_result->check_type = SERVICE_CHECK_ACTIVE;
	tmp_check_result->check_options = 0;
	tmp_check_result->scheduled_check = TRUE;
	tmp_check_result->reschedule_check = FALSE;
	tmp_check_result->latency = 0.666;
	tmp_check_result->start_time.tv_sec = time;
	tmp_check_result->start_time.tv_usec = 500000;
	tmp_check_result->finish_time.tv_sec = time + 4;
	tmp_check_result->finish_time.tv_usec = 0;
	tmp_check_result->early_timeout = 0;
	tmp_check_result->exited_ok = TRUE;
	tmp_check_result->return_code = 0;
	tmp_check_result->output = strdup("OK - Everything Hunky Dorey");

	/* Event to process those results */
	new_event = (timed_event *)malloc(sizeof(timed_event));
	new_event->event_type = EVENT_USER_FUNCTION;
	new_event->event_data = (void *)test_event_process_svc1_results;
	new_event->event_args = (void *)tmp_check_result;
	new_event->event_options = 0;
	new_event->priority = 0;
	new_event->run_time = tmp_check_result->finish_time.tv_sec;
	new_event->recurring = FALSE;
	new_event->event_interval = 0L;
	new_event->timing_func = NULL;
	new_event->compensate_for_time_change = TRUE;
	new_event->sq_event = NULL;
	reschedule_event(nagios_squeue, new_event);

	}

void
setup_events_with_host(time_t time) {
	timed_event *new_event = NULL;

	/* First service is a normal one */
	svc3 = find_service("host1", "Normal service 2");
	svc3->check_options = 0;
	svc3->next_check = time;
	svc3->state_type = SOFT_STATE;
	svc3->current_state = STATE_OK;
	svc3->retry_interval = 1;
	svc3->check_interval = 5;

	new_event = (timed_event *)malloc(sizeof(timed_event));
	new_event->event_type = EVENT_SERVICE_CHECK;
	new_event->event_data = (void *)svc3;
	new_event->event_args = (void *)NULL;
	new_event->event_options = 0;
	new_event->priority = 0;
	new_event->run_time = svc3->next_check;
	new_event->recurring = FALSE;
	new_event->event_interval = 0L;
	new_event->timing_func = NULL;
	new_event->compensate_for_time_change = TRUE;
	new_event->sq_event = NULL;
	reschedule_event(nagios_squeue, new_event);

	host1 = find_host("host1");
	host1->retry_interval = 1;
	host1->check_interval = 5;
	host1->check_options = 0;
	host1->next_check = time;
	host1->state_type = SOFT_STATE;
	host1->current_state = STATE_OK;
	host1->is_executing = 0;

	new_event = (timed_event *)malloc(sizeof(timed_event));
	new_event->event_type = EVENT_HOST_CHECK;
	new_event->event_data = (void *)host1;
	new_event->event_args = (void *)NULL;
	new_event->event_options = 0;
	new_event->priority = 0;
	new_event->run_time = host1->next_check;
	new_event->recurring = FALSE;
	new_event->event_interval = 0L;
	new_event->timing_func = NULL;
	new_event->compensate_for_time_change = TRUE;
	new_event->sq_event = NULL;
	reschedule_event(nagios_squeue, new_event);
	}

int
main(int argc, char **argv) {
	int result;
	time_t now = 0L;
	char *main_config_file = "../t/etc/nagios-test-events.cfg";

	/* Initialize configuration variables */
	init_main_cfg_vars(1);
	init_shared_cfg_vars(1);

	/* Read the configuration */
	read_main_config_file(main_config_file);
	read_object_config_data(main_config_file, READ_ALL_OBJECT_DATA);
	pre_flight_check();

	plan_tests(11);

	interval_length = 60;

	nagios_squeue = squeue_create(4096);
	ok(nagios_squeue != NULL, "Created nagios squeue");

	execute_service_checks = 1;
	currently_running_service_checks = 0;
	max_parallel_service_checks = 1;
	time(&now);
	setup_events(now);
	setup_svc1_result_events(now);
	printf("# Running execution loop - may take some time...\n");
	event_execution_loop();

	ok(svc1->last_check == now, "svc1 has not had its next check time changed")
			|| diag("last_check = now: %ld", svc1->last_check - now);
	ok(svc2->next_check > now + NUDGE_MIN && svc2->next_check < now + NUDGE_MAX,
			"svc2 has been nudged") ||
			diag("Nudge amount: %ld\n", svc2->next_check - now);


	sigshutdown = FALSE;
	currently_running_service_checks = 0;
	max_parallel_service_checks = 2;
	time(&now);
	setup_events(now);
	printf("# Running execution loop - may take some time...\n");
	event_execution_loop();
	ok(svc1->next_check == now, "svc1 has not had its next check time changed")
			|| diag("next_check - now: %ld", svc1->next_check - now);
	ok(svc2->next_check == now, "svc2 also has not changed, because can execute")
			|| diag("Nudge amount: %ld\n", svc2->next_check - now);


	/* This test should have both services moved forward due to not executing any service checks */
	/* Will make svc2 move forward by a retry_interval amount */
	execute_service_checks = 0;
	sigshutdown = FALSE;
	currently_running_service_checks = 0;
	max_parallel_service_checks = 2;
	time(&now);
	setup_events(now);
	svc2->current_state = STATE_CRITICAL;
	printf("# Running execution loop - may take some time...\n");
	event_execution_loop();
	ok(svc1->next_check == now + 300, "svc1 rescheduled ahead - normal interval")
			|| diag("next_check - now: %ld", svc1->next_check - now);
	ok(svc2->next_check == now + 60, "svc2 rescheduled ahead - retry interval")
			|| diag("next_check - now: %ld", svc2->next_check - now);

	/* Checking that a host check immediately following a service check
	 * correctly checks the host
	*/
	squeue_destroy(nagios_squeue, 0);
	nagios_squeue = squeue_create(4096);

	sigshutdown = FALSE;
	currently_running_service_checks = 0;
	max_parallel_service_checks = 2;
	execute_service_checks = 0;
	execute_host_checks = 1;
	time(&now);
	setup_events_with_host(now);
	printf("# Running execution loop - may take some time...\n");
	event_execution_loop();

	ok(host1->last_check - now <= 2,
			"host1 was checked (within 2 seconds tolerance)") ||
			diag("last_check - now: %ld", host1->last_check - now);
	ok(svc3->last_check == 0, "svc3 was skipped");
	ok(host1->next_check == now, "host1 rescheduled ahead - normal interval")
			|| diag("next_check - now: %ld", host1->next_check - now);
	ok(svc3->next_check == now + 300, "svc3 rescheduled ahead - normal interval");

	return exit_status();
	}

