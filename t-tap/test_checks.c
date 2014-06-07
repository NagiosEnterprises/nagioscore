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

#define TEST_CHECKS_C
#define NSCORE 1
#include "config.h"
#include "comments.h"
#include "common.h"
#include "statusdata.h"
#include "downtime.h"
#include "macros.h"
#include "nagios.h"
#include "broker.h"
#include "perfdata.h"
#include "../lib/lnag-utils.h"
#include "tap.h"
#include "stub_sehandlers.c"
#include "stub_comments.c"
#include "stub_perfdata.c"
#include "stub_downtime.c"
#include "stub_notifications.c"
#include "stub_logging.c"
#include "stub_broker.c"
#include "stub_macros.c"
#include "stub_workers.c"
#include "stub_events.c"
#include "stub_statusdata.c"
#include "stub_flapping.c"
#include "stub_nebmods.c"
#include "stub_netutils.c"
#include "stub_commands.c"
#include "stub_xodtemplate.c"

int date_format;

/* Test specific functions + variables */
service *svc1 = NULL, *svc2 = NULL;
host *host1 = NULL;
int found_log_rechecking_host_when_service_wobbles = 0;
int found_log_run_async_host_check = 0;
check_result *tmp_check_result;

void setup_check_result(int check_type) {
	struct timeval start_time, finish_time;
	start_time.tv_sec = 1234567890L;
	start_time.tv_usec = 0L;
	finish_time.tv_sec = 1234567891L;
	finish_time.tv_usec = 0L;

	tmp_check_result = (check_result *)malloc(sizeof(check_result));
	tmp_check_result->check_type = check_type;
	tmp_check_result->check_options = 0;
	tmp_check_result->scheduled_check = TRUE;
	tmp_check_result->reschedule_check = TRUE;
	tmp_check_result->exited_ok = TRUE;
	tmp_check_result->return_code = 0;
	tmp_check_result->output = strdup("Fake result");
	tmp_check_result->latency = 0.6969;
	tmp_check_result->start_time = start_time;
	tmp_check_result->finish_time = finish_time;
	}

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
int log_debug_info(int level, int verbosity, const char *fmt, ...) {
	va_list ap;
	char *buffer = NULL;

	va_start(ap, fmt);
	/* vprintf( fmt, ap ); */
	vasprintf(&buffer, fmt, ap);
	if(strcmp(buffer, "Service wobbled between non-OK states, so we'll recheck the host state...\n") == 0) {
		found_log_rechecking_host_when_service_wobbles++;
		}
	if(strcmp(buffer, "run_async_host_check()\n") == 0) {
		found_log_run_async_host_check++;
		}
	free(buffer);
	va_end(ap);
	}


void
setup_objects(time_t time) {
	timed_event *new_event = NULL;

	enable_predictive_service_dependency_checks = FALSE;

	host1 = (host *)calloc(1, sizeof(host));
	host1->name = strdup("Host1");
	host1->address = strdup("127.0.0.1");
	host1->retry_interval = 1;
	host1->check_interval = 5;
	host1->check_options = 0;
	host1->state_type = SOFT_STATE;
	host1->current_state = HOST_DOWN;
	host1->has_been_checked = TRUE;
	host1->last_check = time;
	host1->next_check = time;

	/* First service is a normal one */
	svc1 = (service *)calloc(1, sizeof(service));
	svc1->host_name = strdup("Host1");
	svc1->host_ptr = host1;
	svc1->description = strdup("Normal service");
	svc1->check_options = 0;
	svc1->next_check = time;
	svc1->state_type = SOFT_STATE;
	svc1->current_state = STATE_CRITICAL;
	svc1->retry_interval = 1;
	svc1->check_interval = 5;
	svc1->current_attempt = 1;
	svc1->max_attempts = 4;
	svc1->last_state_change = 0;
	svc1->last_state_change = 0;
	svc1->last_check = (time_t)1234560000;
	svc1->host_problem_at_last_check = FALSE;
	svc1->plugin_output = strdup("Initial state");
	svc1->last_hard_state_change = (time_t)1111111111;
	svc1->accept_passive_checks = 1;

	/* Second service .... to be configured! */
	svc2 = (service *)calloc(1, sizeof(service));
	svc2->host_name = strdup("Host1");
	svc2->description = strdup("To be nudged");
	svc2->check_options = 0;
	svc2->next_check = time;
	svc2->state_type = SOFT_STATE;
	svc2->current_state = STATE_OK;
	svc2->retry_interval = 1;
	svc2->check_interval = 5;

	}

void run_service_check_tests(int check_type, time_t when) {

	/* Test to confirm that if a service is warning, the notified_on_critical is reset */
	tmp_check_result = (check_result *)calloc(1, sizeof(check_result));
	tmp_check_result->host_name = strdup("host1");
	tmp_check_result->service_description = strdup("Normal service");
	tmp_check_result->object_check_type = SERVICE_CHECK;
	tmp_check_result->check_type = check_type;
	tmp_check_result->check_options = 0;
	tmp_check_result->scheduled_check = TRUE;
	tmp_check_result->reschedule_check = TRUE;
	tmp_check_result->latency = 0.666;
	tmp_check_result->start_time.tv_sec = 1234567890;
	tmp_check_result->start_time.tv_usec = 56565;
	tmp_check_result->finish_time.tv_sec = 1234567899;
	tmp_check_result->finish_time.tv_usec = 45454;
	tmp_check_result->early_timeout = 0;
	tmp_check_result->exited_ok = TRUE;
	tmp_check_result->return_code = 1;
	tmp_check_result->output = strdup("Warning - check notified_on_critical reset");

	setup_objects(when);
	svc1->last_state = STATE_CRITICAL;
	svc1->notification_options = OPT_CRITICAL;
	svc1->current_notification_number = 999;
	svc1->last_notification = (time_t)11111;
	svc1->next_notification = (time_t)22222;
	svc1->no_more_notifications = TRUE;

	handle_async_service_check_result(svc1, tmp_check_result);

	/* This has been taken out because it is not required
	ok( svc1->notified_on_critical==FALSE, "notified_on_critical reset" );
	*/
	ok(svc1->last_notification == (time_t)0, "last notification reset due to state change");
	ok(svc1->next_notification == (time_t)0, "next notification reset due to state change");
	ok(svc1->no_more_notifications == FALSE, "no_more_notifications reset due to state change");
	ok(svc1->current_notification_number == 999, "notification number NOT reset");

	/* Test case:
		service that transitions from OK to CRITICAL (where its host is set to DOWN) will get set to a hard state
		even though check attempts = 1 of 4
	*/
	setup_objects((time_t) 1234567800L);
	host1->current_state = HOST_DOWN;
	svc1->current_state = STATE_OK;
	svc1->state_type = HARD_STATE;
	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_CRITICAL;
	tmp_check_result->output = strdup("CRITICAL failure");

	handle_async_service_check_result(svc1, tmp_check_result);

	ok(svc1->last_hard_state_change == (time_t)1234567890, "Got last_hard_state_change time=%lu", svc1->last_hard_state_change);
	ok(svc1->last_state_change == svc1->last_hard_state_change, "Got same last_state_change");
	ok(svc1->last_hard_state == 2, "Should save the last hard state as critical for next time");
	ok(svc1->host_problem_at_last_check == TRUE, "Got host_problem_at_last_check set to TRUE due to host failure - this needs to be saved otherwise extra alerts raised in subsequent runs");
	ok(svc1->state_type == HARD_STATE, "This should be a HARD state since the host is in a failure state");
	ok(svc1->current_attempt == 1, "Previous status was OK, so this failure should show current_attempt=1") || diag("Current attempt=%d", svc1->current_attempt);





	/* Test case:
		OK -> WARNING 1/4 -> ack -> WARNING 2/4 -> OK transition
		Tests that the ack is left for 2/4
	*/
	setup_objects(when);
	host1->current_state = HOST_UP;
	host1->max_attempts = 4;
	svc1->last_state = STATE_OK;
	svc1->last_hard_state = STATE_OK;
	svc1->current_state = STATE_OK;
	svc1->state_type = SOFT_STATE;

	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_WARNING;
	tmp_check_result->output = strdup("WARNING failure");
	handle_async_service_check_result(svc1, tmp_check_result);

	ok(svc1->last_notification == (time_t)0, "last notification reset due to state change");
	ok(svc1->next_notification == (time_t)0, "next notification reset due to state change");
	ok(svc1->no_more_notifications == FALSE, "no_more_notifications reset due to state change");
	ok(svc1->current_notification_number == 0, "notification number reset");
	ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, "No acks");

	svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;

	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_WARNING;
	tmp_check_result->output = strdup("WARNING failure");
	handle_async_service_check_result(svc1, tmp_check_result);

	ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, "Ack left");

	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_OK;
	tmp_check_result->output = strdup("Back to OK");
	handle_async_service_check_result(svc1, tmp_check_result);

	ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, "Ack reset to none");




	/* Test case:
	   OK -> WARNING 1/4 -> ack -> WARNING 2/4 -> WARNING 3/4 -> WARNING 4/4 -> WARNING 4/4 -> OK transition
	   Tests that the ack is not removed on hard state change
	*/
	setup_objects(when);
	host1->current_state = HOST_UP;
	host1->max_attempts = 4;
	svc1->last_state = STATE_OK;
	svc1->last_hard_state = STATE_OK;
	svc1->current_state = STATE_OK;
	svc1->state_type = SOFT_STATE;
	svc1->current_attempt = 1;

	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_OK;
	tmp_check_result->output = strdup("Reset to OK");
	handle_async_service_check_result(svc1, tmp_check_result);
	ok(svc1->current_attempt == 1, "Current attempt is 1") ||
			diag("Current attempt now: %d", svc1->current_attempt);

	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_WARNING;
	tmp_check_result->output = strdup("WARNING failure 1");
	handle_async_service_check_result(svc1, tmp_check_result);

	ok(svc1->state_type == SOFT_STATE, "Soft state");
	ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, "No acks - testing transition to hard warning state");

	svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;
	ok(svc1->current_attempt == 1, "Current attempt is 1") ||
			diag("Current attempt now: %d", svc1->current_attempt);

	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_WARNING;
	tmp_check_result->output = strdup("WARNING failure 2");
	handle_async_service_check_result(svc1, tmp_check_result);
	ok(svc1->state_type == SOFT_STATE, "Soft state");
	ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, "Ack left");
	ok(svc1->current_attempt == 2, "Current attempt is 2") ||
			diag("Current attempt now: %d", svc1->current_attempt);

	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_WARNING;
	tmp_check_result->output = strdup("WARNING failure 3");
	handle_async_service_check_result(svc1, tmp_check_result);
	ok(svc1->state_type == SOFT_STATE, "Soft state");
	ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, "Ack left");
	ok(svc1->current_attempt == 3, "Current attempt is 3") ||
			diag("Current attempt now: %d", svc1->current_attempt);

	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_WARNING;
	tmp_check_result->output = strdup("WARNING failure 4");
	handle_async_service_check_result(svc1, tmp_check_result);
	ok(svc1->state_type == HARD_STATE, "Hard state");
	ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, "Ack left on hard failure");
	ok(svc1->current_attempt == 4, "Current attempt is 4") ||
			diag("Current attempt now: %d", svc1->current_attempt);

	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_OK;
	tmp_check_result->output = strdup("Back to OK");
	handle_async_service_check_result(svc1, tmp_check_result);
	ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, "Ack removed");




	/* Test case:
	   OK -> WARNING 1/1 -> ack -> WARNING -> OK transition
	   Tests that the ack is not removed on 2nd warning, but is on OK
	*/
	setup_objects(when);
	host1->current_state = HOST_UP;
	host1->max_attempts = 4;
	svc1->last_state = STATE_OK;
	svc1->last_hard_state = STATE_OK;
	svc1->current_state = STATE_OK;
	svc1->state_type = SOFT_STATE;
	svc1->current_attempt = 1;
	svc1->max_attempts = 2;
	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_WARNING;
	tmp_check_result->output = strdup("WARNING failure 1");

	handle_async_service_check_result(svc1, tmp_check_result);

	ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, "No acks - testing transition to immediate hard then OK");

	svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;

	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_WARNING;
	tmp_check_result->output = strdup("WARNING failure 2");
	handle_async_service_check_result(svc1, tmp_check_result);
	ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, "Ack left");

	setup_check_result(check_type);
	tmp_check_result->return_code = STATE_OK;
	tmp_check_result->output = strdup("Back to OK");
	handle_async_service_check_result(svc1, tmp_check_result);
	ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, "Ack removed");


	/* Test case:
	   UP -> DOWN 1/4 -> ack -> DOWN 2/4 -> DOWN 3/4 -> DOWN 4/4 -> UP transition
	   Tests that the ack is not removed on 2nd DOWN, but is on UP
	*/
	setup_objects(when);
	host1->current_state = HOST_UP;
	host1->last_state = HOST_UP;
	host1->last_hard_state = HOST_UP;
	host1->state_type = SOFT_STATE;
	host1->current_attempt = 1;
	host1->max_attempts = 4;
	host1->acknowledgement_type = ACKNOWLEDGEMENT_NONE;
	host1->plugin_output = strdup("");
	host1->long_plugin_output = strdup("");
	host1->perf_data = strdup("");
	host1->check_command = strdup("Dummy command required");
	host1->accept_passive_checks = TRUE;
	passive_host_checks_are_soft = TRUE;
	setup_check_result(check_type);

	tmp_check_result->return_code = STATE_CRITICAL;
	tmp_check_result->output = strdup("DOWN failure 2");
	tmp_check_result->check_type = HOST_CHECK_PASSIVE;
	handle_async_host_check_result(host1, tmp_check_result);
	ok(host1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, "No ack set");
	ok(host1->current_attempt == 2, "Attempts right (not sure why this goes into 2 and not 1)") || diag("current_attempt=%d", host1->current_attempt);
	ok(strcmp(host1->plugin_output, "DOWN failure 2") == 0, "output set") || diag("plugin_output=%s", host1->plugin_output);

	host1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;

	tmp_check_result->output = strdup("DOWN failure 3");
	handle_async_host_check_result(host1, tmp_check_result);
	ok(host1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, "Ack should be retained as in soft state");
	ok(host1->current_attempt == 3, "Attempts incremented") || diag("current_attempt=%d", host1->current_attempt);
	ok(strcmp(host1->plugin_output, "DOWN failure 3") == 0, "output set") || diag("plugin_output=%s", host1->plugin_output);


	tmp_check_result->output = strdup("DOWN failure 4");
	handle_async_host_check_result(host1, tmp_check_result);
	ok(host1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, "Ack should be retained as in soft state");
	ok(host1->current_attempt == 4, "Attempts incremented") || diag("current_attempt=%d", host1->current_attempt);
	ok(strcmp(host1->plugin_output, "DOWN failure 4") == 0, "output set") || diag("plugin_output=%s", host1->plugin_output);


	tmp_check_result->return_code = STATE_OK;
	tmp_check_result->output = strdup("UP again");
	handle_async_host_check_result(host1, tmp_check_result);
	ok(host1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, "Ack reset due to state change");
	ok(host1->current_attempt == 1, "Attempts reset") || diag("current_attempt=%d", host1->current_attempt);
	ok(strcmp(host1->plugin_output, "UP again") == 0, "output set") || diag("plugin_output=%s", host1->plugin_output);


	}

int
main(int argc, char **argv) {
	time_t now = 0L;

	accept_passive_host_checks = TRUE;
	accept_passive_service_checks = TRUE;

	plan_tests(92);

	time(&now);

	run_service_check_tests(SERVICE_CHECK_ACTIVE, now);
	run_service_check_tests(SERVICE_CHECK_PASSIVE, now);

	return exit_status();
	}

