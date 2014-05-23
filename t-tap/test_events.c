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
#include "stub_macros.c"
#include "stub_nebmods.c"
#include "stub_netutils.c"
#include "stub_commands.c"
#include "stub_xodtemplate.c"
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

void
setup_events(time_t time) {
	timed_event *new_event = NULL;

	/* First service is a normal one */
	svc1 = (service *)malloc(sizeof(service));
	svc1->host_name = strdup("Host1");
	svc1->description = strdup("Normal service");
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
	new_event->run_time = 0L;				/* Straight away */
	new_event->recurring = FALSE;
	new_event->event_interval = 0L;
	new_event->timing_func = NULL;
	new_event->compensate_for_time_change = TRUE;
	reschedule_event(nagios_squeue, new_event);

	/* Second service is one that will get nudged forward */
	svc2 = (service *)malloc(sizeof(service));
	svc2->host_name = strdup("Host1");
	svc2->description = strdup("To be nudged");
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
	new_event->run_time = 0L;				/* Straight away */
	new_event->recurring = FALSE;
	new_event->event_interval = 0L;
	new_event->timing_func = NULL;
	new_event->compensate_for_time_change = TRUE;
	reschedule_event(nagios_squeue, new_event);
	}

void
setup_events_with_host(time_t time) {
	timed_event *new_event = NULL;

	/* First service is a normal one */
	if(svc3 == NULL)
		svc3 = (service *)malloc(sizeof(service));
	svc3->host_name = strdup("Host0");
	svc3->description = strdup("Normal service");
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
	new_event->run_time = 0L;				/* Straight away */
	new_event->recurring = FALSE;
	new_event->event_interval = 0L;
	new_event->timing_func = NULL;
	new_event->compensate_for_time_change = TRUE;
	reschedule_event(nagios_squeue, new_event);

	if(host1 == NULL)
		host1 = (host *)malloc(sizeof(host));
	host1->name = strdup("Host1");
	host1->address = strdup("127.0.0.1");
	host1->retry_interval = 1;
	host1->check_interval = 5;
	host1->check_options = 0;
	host1->next_check = time;
	new_event->recurring = TRUE;
	host1->state_type = SOFT_STATE;
	host1->current_state = STATE_OK;

	new_event = (timed_event *)malloc(sizeof(timed_event));
	new_event->event_type = EVENT_HOST_CHECK;
	new_event->event_data = (void *)host1;
	new_event->event_args = (void *)NULL;
	new_event->event_options = 0;
	new_event->run_time = 0L;				/* Straight away */
	new_event->recurring = TRUE;
	new_event->event_interval = 0L;
	new_event->timing_func = NULL;
	new_event->compensate_for_time_change = TRUE;
	reschedule_event(nagios_squeue, new_event);
	}

int
main(int argc, char **argv) {
	int result;
	time_t now = 0L;

	plan_tests(11);

	time(&now);
	config_file = "etc/nagios.cfg";
	interval_length = 60;
	execute_service_checks = 1;

	nagios_squeue = squeue_create(4096);
	ok(nagios_squeue != NULL, "Created nagios squeue");

	currently_running_service_checks = 0;
	max_parallel_service_checks = 1;
	setup_events(now);
	event_execution_loop();

	ok(svc1->next_check == now, "svc1 has not had its next check time changed");
	printf("# Nudge amount: %d\n", svc2->next_check - now);
	ok(svc2->next_check > now + 5 && svc2->next_check < now + 5 + 10, "svc2 has been nudged");


	sigshutdown = FALSE;
	currently_running_service_checks = 0;
	max_parallel_service_checks = 2;
	setup_events(now);
	event_execution_loop();
	ok(svc1->next_check == now, "svc1 has not had its next check time changed");
	printf("# Nudge amount: %d\n", svc2->next_check - now);
	ok(svc2->next_check == now, "svc2 also has not changed, because can execute");


	/* This test should have both services moved forward due to not executing any service checks */
	/* Will make svc2 move forward by a retry_interval amount */
	execute_service_checks = 0;
	sigshutdown = FALSE;
	currently_running_service_checks = 0;
	max_parallel_service_checks = 2;
	setup_events(now);
	svc2->current_state = STATE_CRITICAL;
	event_execution_loop();
	ok(svc1->next_check == now + 300, "svc1 rescheduled ahead - normal interval");
	ok(svc2->next_check == now + 60,  "svc2 rescheduled ahead - retry interval");


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
	setup_events_with_host(now);
	event_execution_loop();

	ok(host1->last_check - now <= 2,  "host1 was checked (within 2 seconds tolerance)") || diag("last_check:%lu now:%lu", host1->last_check, now);
	ok(svc3->last_check == 0, "svc3 was skipped");
	ok(host1->next_check == now,  "host1 rescheduled ahead - normal interval");
	ok(svc3->next_check == now + 300, "svc3 rescheduled ahead - normal interval");

	return exit_status();
	}

