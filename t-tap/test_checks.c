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

service         * svc1          = NULL;
host            * hst1          = NULL;
check_result    * chk_result    = NULL;

int found_log_rechecking_host_when_service_wobbles = 0;
int found_log_run_async_host_check                 = 0;
int hst1_notifications  = 0;
int svc1_notifications  = 0;
int hst1_event_handlers = 0;
int svc1_event_handlers = 0;
int hst1_logs           = 0;
int svc1_logs           = 0;
int c                   = 0;
unsigned int current_test        = 0;

#define HOST_NAME "hst1"
#define HOST_ADDRESS "127.0.0.1"
#define HOST_COMMAND "hst1_command"
#define SERVICE_NAME "svc1"

#define EVENT_HANDLED   1
#define NOTIFIED        2
#define LOGGED          4

void free_hst1()
{
    if (hst1 != NULL) {
        my_free(hst1->name);
        my_free(hst1->address);
        my_free(hst1->plugin_output);
        my_free(hst1->long_plugin_output);
        my_free(hst1->perf_data);
        my_free(hst1->check_command);
        my_free(hst1);
    }
}

void free_svc1()
{
    if (svc1 != NULL) {
        my_free(svc1->host_name);
        my_free(svc1->description);
        my_free(svc1->plugin_output);
        my_free(svc1->long_plugin_output);
        my_free(svc1->perf_data);
        my_free(svc1->check_command);
        my_free(svc1);
    }
}

void free_chk_result()
{
    if (chk_result != NULL) {
        my_free(chk_result->host_name);
        my_free(chk_result->service_description);
        my_free(chk_result->output);
        my_free(chk_result);
    }
}

void free_all()
{
    free_hst1();
    free_svc1();
    free_chk_result();
}

int service_notification(service *svc, int type, char *not_author, char *not_data, int options)
{
    svc1_notifications++;
    return OK;
}

int host_notification(host *hst, int type, char *not_author, char *not_data, int options)
{
    hst1_notifications++;
    return OK;
}

int handle_host_event(host *hst) 
{
    hst1_event_handlers++;
    return OK;
}

int handle_service_event(service *svc) 
{
    svc1_event_handlers++;
    return OK;
}

int log_host_event(host *hst)
{
    hst1_logs++;
    return OK;
}

int log_service_event(service *svc)
{
    svc1_logs++;
    return OK;
}

void reset_globals()
{
    hst1_notifications  = 0;
    svc1_notifications  = 0;
    hst1_event_handlers = 0;
    svc1_event_handlers = 0;
    hst1_logs           = 0;
    svc1_logs           = 0;
}

void handle_hst1()
{
    reset_globals();
    handle_async_host_check_result(hst1, chk_result);
}

void handle_svc1()
{
    reset_globals();
    handle_async_service_check_result(svc1, chk_result);
}

void adjust_check_result_output(char * output)
{
    my_free(chk_result->output);
    chk_result->output = strdup(output);
}

void adjust_host_output(char * output)
{
    my_free(hst1->plugin_output);
    hst1->plugin_output = strdup(output);
}

void adjust_service_output(char * output)
{
    my_free(svc1->plugin_output);
    svc1->plugin_output = strdup(output);
}

void create_check_result(int check_type, int return_code, char * output)
{
    struct timeval start_time, finish_time;

    start_time.tv_sec = 1234567890L;
    start_time.tv_usec = 0L;

    finish_time.tv_sec = 1234567891L;
    finish_time.tv_usec = 0L;

    free_chk_result();

    chk_result = (check_result *) calloc(1, sizeof(check_result));

    chk_result->host_name           = strdup(HOST_NAME);
    chk_result->service_description = strdup(SERVICE_NAME);
    chk_result->check_options       = CHECK_OPTION_NONE;
    chk_result->scheduled_check     = TRUE;
    chk_result->reschedule_check    = TRUE;
    chk_result->exited_ok           = TRUE;    
    chk_result->early_timeout       = FALSE;
    chk_result->latency             = 0.5;
    chk_result->start_time          = start_time;
    chk_result->finish_time         = finish_time;

    chk_result->return_code         = return_code;
    chk_result->check_type          = check_type;

    adjust_check_result_output(output);
}

void create_objects(int host_state, int host_state_type, char * host_output, int service_state, int service_state_type, char * service_output)
{
    free_hst1();
    free_svc1();

    hst1 = (host *) calloc(1, sizeof(host));

    if (service_output != NULL) {
        svc1 = (service *) calloc(1, sizeof(service));
    }

    hst1->name                       = strdup(HOST_NAME);
    hst1->address                    = strdup(HOST_ADDRESS);
    hst1->check_command              = strdup(HOST_COMMAND);
    hst1->retry_interval             = 1;
    hst1->check_interval             = 5;
    hst1->current_attempt            = 1;
    hst1->max_attempts               = 4;
    hst1->check_options              = CHECK_OPTION_NONE;
    hst1->has_been_checked           = TRUE;
    hst1->last_state_change          = (time_t) 0L;
    hst1->last_hard_state_change     = (time_t) 0L;
    hst1->last_time_up               = (time_t) 0L;
    hst1->last_time_down             = (time_t) 0L;
    hst1->last_time_unreachable      = (time_t) 0L;
    hst1->last_notification          = (time_t) 0L;
    hst1->next_notification          = (time_t) 0L;
    hst1->last_check                 = (time_t) 0L;
    hst1->next_check                 = (time_t) 0L;
    hst1->should_be_scheduled        = TRUE;
    hst1->last_hard_state            = HOST_UP;
    hst1->notifications_enabled      = TRUE;
    hst1->event_handler_enabled      = TRUE;
    hst1->accept_passive_checks      = TRUE;
    hst1->initial_state              = HOST_UP;
    hst1->accept_passive_checks      = TRUE;

    if (service_output != NULL) {
        svc1->host_name                  = strdup(HOST_NAME);
        svc1->description                = strdup(SERVICE_NAME);
        svc1->host_ptr                   = hst1;
        svc1->retry_interval             = 1;
        svc1->check_interval             = 5;
        svc1->current_attempt            = 1;
        svc1->max_attempts               = 4;
        svc1->check_options              = CHECK_OPTION_NONE;
        svc1->has_been_checked           = TRUE;
        svc1->last_state_change          = (time_t) 0L;
        svc1->last_hard_state_change     = (time_t) 0L;
        svc1->last_time_ok               = (time_t) 0L;
        svc1->last_time_warning          = (time_t) 0L;
        svc1->last_time_unknown          = (time_t) 0L;
        svc1->last_time_critical         = (time_t) 0L;
        svc1->last_notification          = (time_t) 0L;
        svc1->next_notification          = (time_t) 0L;
        svc1->last_check                 = (time_t) 0L;
        svc1->next_check                 = (time_t) 0L;
        svc1->host_problem_at_last_check = FALSE;
        svc1->should_be_scheduled        = TRUE;
        svc1->last_hard_state            = STATE_OK;
        svc1->notifications_enabled      = TRUE;
        svc1->event_handler_enabled      = TRUE;
        svc1->accept_passive_checks      = TRUE;
        svc1->initial_state              = STATE_OK;
        svc1->accept_passive_checks      = TRUE;
    }

    hst1->current_state = host_state;
    hst1->state_type    = host_state_type;
    adjust_host_output(host_output);

    if (service_output != NULL) {
        svc1->current_state = service_state;
        svc1->state_type    = service_state_type;
        adjust_service_output(service_output);
    }
}

int update_program_status(int aggregated_dump)
{
    c++;
    /* printf ("# In the update_program_status hook: %d\n", c); */

    /* Set this to break out of event_execution_loop */
    if (c > 10) {
        sigshutdown = TRUE;
        c = 0;
    }

    return OK;
}

int log_debug_info(int level, int verbosity, const char *fmt, ...)
{
    va_list ap;
    char *buffer = NULL;
    int r = 0;

    va_start(ap, fmt);
    /* vprintf( fmt, ap ); */
    r = vasprintf(&buffer, fmt, ap);

    if (strcmp(buffer, "Service wobbled between non-OK states, so we'll recheck the host state...\n") == 0) {
        found_log_rechecking_host_when_service_wobbles++;
    }
    else if (strcmp(buffer, "run_async_host_check()\n") == 0) {
        found_log_run_async_host_check++;
    }

    /*
    printf("DEBUG: %s", buffer);
    */

    free(buffer);
    va_end(ap);

    return OK;
}

void test_hst_handler_notification_logging(int bitmask)
{
    int expected_event_handler = 0;
    int expected_notification = 0;
    int expected_logging = 0;

    if (bitmask & EVENT_HANDLED) {
        expected_event_handler = 1;
    }
    if (bitmask & NOTIFIED) {
        expected_notification = 1;
    }
    if (bitmask & LOGGED) {
        expected_logging = 1;
    }

    ok(hst1_event_handlers == expected_event_handler,
        "[%d] Host event handler was executed", ++current_test);
    ok(hst1_notifications == expected_notification,
        "[%d] Host contacts were notified", ++current_test);
    ok(hst1_logs == expected_logging,
        "[%d] Host event logged", ++current_test);
}

void test_svc_handler_notification_logging(int bitmask)
{
    int expected_event_handler = 0;
    int expected_notification = 0;
    int expected_logging = 0;

    if (bitmask & EVENT_HANDLED) {
        expected_event_handler = 1;
    }
    if (bitmask & NOTIFIED) {
        expected_notification = 1;
    }
    if (bitmask & LOGGED) {
        expected_logging = 1;
    }

    ok(svc1_event_handlers == expected_event_handler,
        "[%d] Service event handler was executed", ++current_test);
    ok(svc1_notifications == expected_notification,
        "[%d] Service contacts were notified", ++current_test);
    ok(svc1_logs == expected_logging,
        "[%d] Service event logged", ++current_test);
}

void test_hst_current_attempt(int attempt)
{
    ok(hst1->current_attempt == attempt, 
        "[%d] Current attempt is %d", ++current_test, attempt) 
        || diag("Current attempt now: %d", hst1->current_attempt);
}

void test_svc_current_attempt(int attempt)
{
    ok(svc1->current_attempt == attempt, 
        "[%d] Current attempt is %d", ++current_test, attempt) 
        || diag("Current attempt now: %d", svc1->current_attempt);
}

void test_hst_state_type(int state_type)
{
    ok(hst1->state_type == state_type, 
        "[%d] Got state_type %s", ++current_test, state_type_name(state_type));    
}

void test_svc_state_type(int state_type)
{
    ok(svc1->state_type == state_type, 
        "[%d] Got state_type %s", ++current_test, state_type_name(state_type));    
}

void test_hst_hard_state_change(time_t last_hard_state_change, int last_hard_state, int state_type)
{
    ok(hst1->last_hard_state_change == last_hard_state_change, 
        "[%d] Got last_hard_state_change time=%lu", ++current_test, hst1->last_hard_state_change);
    ok(hst1->last_state_change == hst1->last_hard_state_change, 
        "[%d] Got same last_state_change", ++current_test);
    ok(hst1->last_hard_state == last_hard_state, 
        "[%d] Got last_hard_state %s", ++current_test, service_state_name(last_hard_state));

    test_hst_state_type(state_type);
}

void test_svc_hard_state_change(time_t last_hard_state_change, int last_hard_state, int state_type, int host_problem)
{
    ok(svc1->last_hard_state_change == last_hard_state_change, 
        "[%d] Got last_hard_state_change time=%lu", ++current_test, svc1->last_hard_state_change);
    ok(svc1->last_state_change == svc1->last_hard_state_change, 
        "[%d] Got same last_state_change", ++current_test);
    ok(svc1->last_hard_state == last_hard_state, 
        "[%d] Got last_hard_state %s", ++current_test, service_state_name(last_hard_state));
    ok(svc1->host_problem_at_last_check == host_problem, 
        "[%d] Got host_problem_at_last_check set to %s", ++current_test, (host_problem == TRUE ? "TRUE" : "FALSE"));

    test_svc_state_type(state_type);
}

void test_hst_notification_settings(time_t last_notification, time_t next_notification, int no_more_notifications, int notification_number)
{
    ok(hst1->last_notification == last_notification, 
        "[%d] last_notification time expected", ++current_test);
    ok(hst1->next_notification == next_notification, 
        "[%d] next_notification time expected", ++current_test);
    ok(hst1->no_more_notifications == no_more_notifications, 
        "[%d] no_more_notifications was %s", ++current_test, (no_more_notifications == TRUE ? "TRUE" : "FALSE"));
    ok(hst1->current_notification_number == notification_number, 
        "[%d] notification_number was expected", ++current_test);
}

void test_svc_notification_settings(time_t last_notification, time_t next_notification, int no_more_notifications, int notification_number)
{
    ok(svc1->last_notification == last_notification, 
        "[%d] last_notification time expected", ++current_test);
    ok(svc1->next_notification == next_notification, 
        "[%d] next_notification time expected", ++current_test);
    ok(svc1->no_more_notifications == no_more_notifications, 
        "[%d] no_more_notifications was %s", ++current_test, (no_more_notifications == TRUE ? "TRUE" : "FALSE"));
    ok(svc1->current_notification_number == notification_number, 
        "[%d] notification_number was expected", ++current_test);
}

void test_hst_acknowledgement(int acknowledgement_type)
{
    char * ack_msg;

    switch (acknowledgement_type) {
    case ACKNOWLEDGEMENT_NONE:
        ack_msg = strdup("No acknowledgement");
        break;
    case ACKNOWLEDGEMENT_NORMAL:
        ack_msg = strdup("Normal acknowledgement, not reset");
        break;
    default:
        ack_msg = strdup("Acknowledgement as expected");
        break;
    }

    ok(hst1->acknowledgement_type == acknowledgement_type, 
        "[%d] %s", ++current_test, ack_msg);

    free(ack_msg);
}

void test_svc_acknowledgement(int acknowledgement_type)
{
    char * ack_msg;

    switch (acknowledgement_type) {
    case ACKNOWLEDGEMENT_NONE:
        ack_msg = strdup("No acknowledgement");
        break;
    case ACKNOWLEDGEMENT_NORMAL:
        ack_msg = strdup("Normal acknowledgement, not reset");
        break;
    default:
        ack_msg = strdup("Acknowledgement as expected");
        break;
    }

    ok(svc1->acknowledgement_type == acknowledgement_type, 
        "[%d] %s", ++current_test, ack_msg);

    free(ack_msg);
}

void run_check_tests(int check_type, time_t when)
{
    time_t tmp_time = (time_t) 0L;
    int tmp_int     = 0;
    /**************************************************************
        hst is soft/down, svc is soft/crit
        submit warning check result
        - last and next notification reset due to state change
        - no more notifications reset
        - notification number not reset
        - state type turns hard since host is down
        - current attempt stays at 1 since host is down and service is hard
        - event handlers, logging, notifications execute
    */
    create_objects(HOST_DOWN, SOFT_STATE, "host output", STATE_CRITICAL, SOFT_STATE, "service output");
    create_check_result(check_type, STATE_WARNING, "Warning - check notified_on_critical reset");

    svc1->last_state                    = STATE_CRITICAL;
    svc1->notification_options          = OPT_CRITICAL;
    svc1->current_notification_number   = 999;
    svc1->last_notification             = (time_t) 11111L;
    svc1->next_notification             = (time_t) 22222L;
    svc1->no_more_notifications         = TRUE;

    handle_svc1();

    test_svc_notification_settings((time_t) 0L, (time_t) 0L, FALSE, 999);    
    test_svc_state_type(HARD_STATE);
    test_svc_current_attempt(1);
    test_svc_handler_notification_logging( EVENT_HANDLED | NOTIFIED | LOGGED );


    /**************************************************************
        hst down/soft, svc ok/hard
        submit critical check result
        - svc goes crit/hard because hst is down
        - host problem at last check
        - last hard state svc data updated appropriately
        - event handlers, logging, notifications execute
    */
    create_objects(HOST_DOWN, SOFT_STATE, "host output", STATE_OK, HARD_STATE, "service output");
    create_check_result(check_type, STATE_CRITICAL, "Critical output");

    handle_svc1();

    test_svc_hard_state_change((time_t) 1234567890L, STATE_CRITICAL, HARD_STATE, TRUE);
    test_svc_current_attempt(1);
    test_svc_handler_notification_logging( EVENT_HANDLED | NOTIFIED | LOGGED );



    /**************************************************************
        hst up/hard, svc ok/hard
        submit warning check result
        - notification information reset due to state change
        - acknowledgements reset due to state change
        - event handlers, logging executed, no notifications
        set ack to normal
        submit warning check result
        - current attempt = 2
        - acknowledgement not reset
        - event handlers executed, no notification or logging
        submit warning check result
        - current attempt = 3
        - ack not reset
        - event handlers executed, no notification or logging
        submit warning check result
        - event handlers, notification, logging executed
        - hard state change data set appropriately
        - ack reset
        - current attempt = 4
        set ack to normal
        submit warning check result
        - notifications executed, no event handlers or logging
        - current attempt = 4
        set stalking enabled for warning on svc
        submit warning check result with different output
        - current attempt = 4
        - notifications, logging (because stalking) executed, no event handlers
        submit warning check result with same as last output
        - current attempt = 4
        - notifications executed, no event handlers logging
        change stalking to ok on svc
        submit warning check result with different output
        - notifications executed, no event handlers or logging
        - current attempt = 4
        submit ok check result
        - state type is hard (recovery)
        - event handlers, notifications, logging executed
        - current attempt = 1
        - ack reset
        - all last hard state data is appropriate
        submit ok check result, same output
        - current attempt = 1
        - state type is still hard
        - no event handlers, notifications, logging executed
        submit ok check result, different output
        - current attempt = 1
        - logging executed, no event handlers or notifications
        submit warning check result
        - current attempt = 1
        - state type = soft
        - event handlers, logging executed, no notifications
        submit critical check result
        - current attempt = 2
        - state type = soft
        - event handlers, logging executed, no notifications
        submit ok check result
        - current attempt = 1
        - state type = soft
        - event handlers, logging executed, no notification
        submit ok check result
        - current attempt = 1
        - state type = hard
        - no event handlers, logging or notification
    */
    /*
        hst up/hard, svc ok/hard
        submit warning check result
        - notification information reset due to state change
        - acknowledgements reset due to state change
        - event handlers, logging executed, no notifications
    */
    create_objects(HOST_UP, HARD_STATE, "host output", STATE_OK, HARD_STATE, "service output");
    create_check_result(check_type, STATE_WARNING, "Warning output");

    handle_svc1();

    test_svc_notification_settings((time_t) 0L, (time_t) 0L, FALSE, 0);
    test_svc_acknowledgement(ACKNOWLEDGEMENT_NONE);
    test_svc_handler_notification_logging( EVENT_HANDLED | NOTIFIED );

    /*
        set ack to normal
        submit warning check result
        - current attempt = 2
        - acknowledgement not reset
        - event handlers executed, no notification or logging
    */
    svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;

    create_check_result(check_type, STATE_WARNING, "Warning output");

    handle_svc1();

    test_svc_acknowledgement(ACKNOWLEDGEMENT_NORMAL);
    test_svc_current_attempt(2);
    test_svc_handler_notification_logging( EVENT_HANDLED );


    /*
        submit warning check result
        - current attempt = 3
        - ack not reset
        - event handlers executed, no notification or logging
    */
    create_check_result(check_type, STATE_WARNING, "Warning output");

    handle_svc1();

    test_svc_acknowledgement(ACKNOWLEDGEMENT_NORMAL);
    test_svc_current_attempt(3);
    test_svc_handler_notification_logging( EVENT_HANDLED );

    /*
        submit warning check result
        - event handlers, notification, logging executed
        - hard state change data set appropriately
        - ack not reset
        - current attempt = 4
    */
    create_check_result(check_type, STATE_WARNING, "Warning output");

    handle_svc1();

    test_svc_hard_state_change((time_t) 1234567890L, STATE_WARNING, HARD_STATE, FALSE);
    test_svc_acknowledgement(ACKNOWLEDGEMENT_NORMAL);
    test_svc_current_attempt(4);
    test_svc_handler_notification_logging( EVENT_HANDLED | NOTIFIED | LOGGED );

    /*
        set ack to normal
        submit warning check result
        - notifications executed, no event handlers or logging
        - current attempt = 4
        - ack not reset
    */
    create_check_result(check_type, STATE_WARNING, "Warning output");
    
    handle_svc1();

    test_svc_acknowledgement(ACKNOWLEDGEMENT_NORMAL);
    test_svc_current_attempt(4);
    test_svc_state_type(HARD_STATE);
    test_svc_handler_notification_logging( LOGGED );

    /*
        set stalking enabled for warning on svc
        submit warning check result with different output
        - current attempt = 4
        - notifications, logging (because stalking) executed, no event handlers
    */
    svc1->stalking_options = OPT_WARNING;
    create_check_result(check_type, STATE_WARNING, "Warning output NEW");
    
    handle_svc1();

    test_svc_current_attempt(4);
    test_svc_state_type(HARD_STATE);
    test_svc_handler_notification_logging( NOTIFIED | LOGGED );

    /*
        submit warning check result with same as last output
        - current attempt = 4
        - notifications executed, no event handlers logging
    */
    create_check_result(check_type, STATE_WARNING, "Warning output NEW");
    
    handle_svc1();

    test_svc_current_attempt(4);
    test_svc_state_type(HARD_STATE);
    test_svc_handler_notification_logging( NOTIFIED );

    /*
        change stalking to ok on svc
        submit warning check result with different output
        - notifications executed, no event handlers or logging
        - current attempt = 4
    */
    svc1->stalking_options = OPT_OK;
    create_check_result(check_type, STATE_WARNING, "Warning output NEW");
    
    handle_svc1();

    test_svc_current_attempt(4);
    test_svc_handler_notification_logging( NOTIFIED );

    /*
        submit ok check result
        - state type is hard (recovery)
        - event handlers, notifications, logging executed
        - current attempt = 1
        - all last hard state data is appropriate
        - ack reset
    */
    create_check_result(check_type, STATE_OK, "Ok output");
    chk_result->start_time.tv_sec  = (time_t) 1678901231L;
    chk_result->finish_time.tv_sec = (time_t) 1678901233L;

    handle_svc1();

    test_svc_hard_state_change((time_t) 1678901231L, STATE_OK, HARD_STATE, FALSE);
    test_svc_current_attempt(1);
    test_svc_acknowledgement(ACKNOWLEDGEMENT_NONE);
    test_svc_handler_notification_logging( EVENT_HANDLED | NOTIFIED | LOGGED );

    /*
        submit ok check result, same output
        - current attempt = 1
        - state type is still hard
        - no event handlers, notifications, logging executed
    */
    create_check_result(check_type, STATE_OK, "Ok output");

    handle_svc1();

    test_svc_current_attempt(1);
    test_svc_state_type(HARD_STATE);
    test_svc_handler_notification_logging( 0 );

    /*
        submit ok check result, different output
        - current attempt = 1
        - logging executed, no event handlers or notifications
    */
    create_check_result(check_type, STATE_OK, "Ok output NEW");

    handle_svc1();
    test_svc_current_attempt(1);
    test_svc_state_type(HARD_STATE);
    test_svc_handler_notification_logging( LOGGED );

    /*
        submit warning check result
        - current attempt = 1
        - state type = soft
        - event handlers, logging executed, no notifications
    */
    create_check_result(check_type, STATE_WARNING, "Warning output");

    handle_svc1();

    test_svc_current_attempt(1);
    test_svc_state_type(SOFT_STATE);
    test_svc_handler_notification_logging( EVENT_HANDLED | LOGGED );

    /*
        submit critical check result
        - current attempt = 2
        - state type = soft
        - event handlers, logging executed, no notifications
    */
    create_check_result(check_type, STATE_CRITICAL, "Ok output");

    handle_svc1();

    test_svc_current_attempt(2);
    test_svc_state_type(SOFT_STATE);
    test_svc_handler_notification_logging( EVENT_HANDLED | LOGGED );

    /*
        submit ok check result
        - current attempt = 1
        - state type = soft
        - event handlers, logging executed, no notification
    */
    create_check_result(check_type, STATE_OK, "Ok output");

    handle_svc1();

    test_svc_current_attempt(1);
    test_svc_state_type(SOFT_STATE);
    test_svc_handler_notification_logging( EVENT_HANDLED | LOGGED );
    /*
        submit ok check result
        - current attempt = 1
        - state type = hard
        - no event handlers, logging or notification
    */
    create_check_result(check_type, STATE_OK, "Ok output");

    handle_svc1();

    test_svc_current_attempt(1);
    test_svc_state_type(HARD_STATE);
    test_svc_handler_notification_logging( 0 );



    /**************************************************************
        hst up/hard
        submit up check result
        - current attempt = 1
        - hard state
        - no event handlers, logging, or notifications
        submit down check result
        - current attempt = 1
        - soft state
        - event handlers, logging executed
        set ack normal
        submit down check result
        - current attempt = 2
        - soft state
        - ack not reset
        - event handlers executed
        submit down check result
        - current attempt = 3
        - soft state
        - ack not reset
        - event handlers executed
        submit down check result
        - current attempt = 4
        - hard state data correct
        - ack not reset
        - event handlers, logging, notifications executed
        submit down check result
        - current attempt = 4
        - hard state
        - ack not reset
        - notifications executed
        turn stalking on for down
        submit down check result with different output
        - notifications, logging executed
        submit up check result
        - current attempt = 1
        - hard state data correct
        - event handlers, logging, notifications executed
        submit up check result with different output
        - no event handlers or logging or notifications executed
        turn stalking on for up
        submit check result with different output
        - logging executed
    */
    /*
        hst up/hard
        submit up check result
        - current attempt = 1
        - hard state
        - no event handlers, logging, or notifications
    */
    create_objects(HOST_UP, HARD_STATE, "host output", 0, 0, NULL);
    create_check_result(check_type, HOST_UP, "Up output");

    handle_hst1();

    test_hst_current_attempt(1);
    test_hst_state_type(HARD_STATE);
    test_hst_handler_notification_logging( 0 );

    /*
        submit down check result
        - current attempt = 1
        - soft state
        - event handlers, logging executed
    */
    create_check_result(check_type, HOST_DOWN, "Down output");

    handle_hst1();

    test_hst_current_attempt(1);
    test_hst_state_type(SOFT_STATE);
    test_hst_handler_notification_logging( EVENT_HANDLED | LOGGED );

    /*
        set ack normal
        submit down check result
        - current attempt = 2
        - soft state
        - ack not reset
        - event handlers executed
    */
    hst1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;
    create_check_result(check_type, HOST_DOWN, "Down output");

    handle_hst1();

    test_hst_current_attempt(2);
    test_hst_state_type(SOFT_STATE);
    test_hst_acknowledgement(ACKNOWLEDGEMENT_NORMAL);
    test_hst_handler_notification_logging( EVENT_HANDLED );

    /*
        submit down check result
        - current attempt = 3
        - soft state
        - ack not reset
        - event handlers executed
    */
    create_check_result(check_type, HOST_DOWN, "Down output");

    handle_hst1();

    test_hst_current_attempt(3);
    test_hst_state_type(SOFT_STATE);
    test_hst_acknowledgement(ACKNOWLEDGEMENT_NORMAL);
    test_hst_handler_notification_logging( EVENT_HANDLED );

    /*
        submit down check result
        - current attempt = 4
        - hard state data correct
        - ack not reset
        - event handlers, logging, notifications executed
    */
    create_check_result(check_type, HOST_DOWN, "Down output");
    chk_result->start_time.tv_sec  = (time_t) 1234567770L;
    chk_result->finish_time.tv_sec = (time_t) 1234567771L;

    handle_hst1();

    test_hst_current_attempt(4);
    test_hst_hard_state_change((time_t) 1234567770L, HOST_DOWN, HARD_STATE);
    test_hst_acknowledgement(ACKNOWLEDGEMENT_NORMAL);
    test_hst_handler_notification_logging( EVENT_HANDLED | LOGGED | NOTIFIED );

    /*
        submit down check result
        - current attempt = 4
        - hard state
        - ack not reset
        - notifications executed
    */
    create_check_result(check_type, HOST_DOWN, "Down output");

    handle_hst1();

    test_hst_current_attempt(4);
    test_hst_state_type(HARD_STATE);
    test_hst_acknowledgement(ACKNOWLEDGEMENT_NORMAL);
    test_hst_handler_notification_logging( EVENT_HANDLED | LOGGED | NOTIFIED );

    /*
        turn stalking on for down
        submit down check result with different output
        - notifications, logging executed
    */
    /*
        submit up check result
        - current attempt = 1
        - hard state data correct
        - event handlers, logging, notifications executed
    */
    /*
        submit up check result with different output
        - no event handlers or logging or notifications executed
    */
    /*
        turn stalking on for up
        submit check result with different output
        - logging executed
    */


    free_all();
}

int main(int argc, char **argv)
{
    time_t now = 0L;

    enable_predictive_service_dependency_checks = FALSE;
    execute_host_checks                         = TRUE;
    execute_service_checks                      = TRUE;
    accept_passive_host_checks                  = TRUE;
    accept_passive_service_checks               = TRUE;

    //plan_tests(96);

    time(&now);

    run_check_tests(CHECK_TYPE_ACTIVE, now);
    run_check_tests(CHECK_TYPE_PASSIVE, now);

    return exit_status();
}
