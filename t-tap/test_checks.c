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

#define HOST_NAME       "hst1"
#define HOST_ADDRESS    "127.0.0.1"
#define HOST_COMMAND    "hst1_command"
#define SERVICE_NAME    "svc1"

#define ORIG_START_TIME  1234567890L
#define ORIG_FINISH_TIME 1234567891L

#define EXPECT_NOTHING  0
#define EVENT_HANDLED   1
#define NOTIFIED        2
#define LOGGED          4

int date_format;
int test_check_debugging = FALSE;

service         * svc1          = NULL;
host            * hst1          = NULL;
check_result    * chk_result    = NULL;

int hst1_notifications  = 0;
int svc1_notifications  = 0;
int hst1_event_handlers = 0;
int svc1_event_handlers = 0;
int hst1_logs           = 0;
int svc1_logs           = 0;
int c                   = 0;

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

    start_time.tv_sec = ORIG_START_TIME;
    start_time.tv_usec = 0L;

    finish_time.tv_sec = ORIG_FINISH_TIME;
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

    if (test_check_debugging == TRUE) {

        va_start(ap, fmt);
        /* vprintf( fmt, ap ); */
        vasprintf(&buffer, fmt, ap);

        printf("DEBUG: %s", buffer);

        free(buffer);
        va_end(ap);
    }

    return OK;
}

void test_hst_handler_notification_logging(int id, int bitmask)
{
    if (bitmask & EVENT_HANDLED) {
        ok(hst1_event_handlers == 1,
            "[%d] Host event WAS handled", id);
    } else {
        ok(hst1_event_handlers == 0,
            "[%d] Host event was NOT handled", id);
    }

    if (bitmask & NOTIFIED) {

        ok(hst1_notifications == 1,
            "[%d] Host contacts WERE notified", id);
    } else {
        ok(hst1_notifications == 0,
            "[%d] Host contacts were NOT notified", id);

    }
    if (bitmask & LOGGED) {
        ok(hst1_logs == 1,
            "[%d] Host event WAS logged", id);
    } else {
        ok(hst1_logs == 0,
            "[%d] Host event NOT logged", id);
    }
}

void test_svc_handler_notification_logging(int id, int bitmask)
{
    if (bitmask & EVENT_HANDLED) {
        ok(svc1_event_handlers == 1,
            "[%d] Service event WAS handled", id);
    } else {
        ok(svc1_event_handlers == 0,
            "[%d] Service event was NOT handled", id);
    }

    if (bitmask & NOTIFIED) {
        ok(svc1_notifications == 1,
            "[%d] Service contacts WERE notified", id);
    } else {
        ok(svc1_notifications == 0,
            "[%d] Service contacts were NOT notified", id);

    }
    if (bitmask & LOGGED) {
        ok(svc1_logs == 1,
            "[%d] Service event WAS logged", id);
    } else {
        ok(svc1_logs == 0,
            "[%d] Service event NOT logged", id);
    }
}

void run_check_tests(int check_type, time_t when)
{
    /*
        Host down/hard, Service crit/soft -> warn
    */
    create_objects(HOST_DOWN, HARD_STATE, "host down", STATE_CRITICAL, SOFT_STATE, "service critical");
    create_check_result(check_type, STATE_WARNING, "service warning");

    svc1->last_notification             = (time_t) 11111L;
    svc1->next_notification             = (time_t) 22222L;
    svc1->no_more_notifications         = TRUE;

    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) ORIG_START_TIME, 
        "Got last_hard_state_change time=%lu", svc1->last_hard_state_change);
    ok(svc1->last_state_change == svc1->last_hard_state_change, 
        "Got same last_state_change");
    ok(svc1->last_hard_state == STATE_WARNING, 
        "Got last hard state as warning");
    ok(svc1->state_type == HARD_STATE,
        "Got hard state since host is down");
    ok(svc1->host_problem_at_last_check == TRUE, 
        "Got host_problem_at_last_check set to TRUE due to host failure");
    ok(svc1->last_notification == (time_t) 0L, 
        "last notification reset due to state change");
    ok(svc1->next_notification == (time_t) 0L, 
        "next notification reset due to state change");
    ok(svc1->no_more_notifications == FALSE, 
        "no_more_notifications reset due to state change");
    test_svc_handler_notification_logging(1, EVENT_HANDLED | NOTIFIED | LOGGED);



    /*
        Host down/hard, Service ok/hard -> crit
    */
    create_objects(HOST_DOWN, HARD_STATE, "host down", STATE_OK, HARD_STATE, "service ok");
    create_check_result(check_type, STATE_CRITICAL, "service critical");

    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) ORIG_START_TIME, 
        "Got last_hard_state_change time=%lu", svc1->last_hard_state_change);
    ok(svc1->last_state_change == svc1->last_hard_state_change, 
        "Got same last_state_change");
    ok(svc1->last_hard_state == STATE_CRITICAL, 
        "Got last hard state as critical");
    ok(svc1->state_type == HARD_STATE,
        "Got hard state since host is down");
    ok(svc1->host_problem_at_last_check == TRUE, 
        "Got host_problem_at_last_check set to TRUE due to host failure");
    ok(svc1->current_attempt == 1, 
        "Previous status was OK, so this failure should show current_attempt=1") 
        || diag("Current attempt=%d", svc1->current_attempt);
    test_svc_handler_notification_logging(2, EVENT_HANDLED | LOGGED);



    /*
        Host up/hard, Service ok/soft -> warn
    */
    create_objects(HOST_UP, HARD_STATE, "host up", STATE_OK, SOFT_STATE, "service ok");
    create_check_result(check_type, STATE_WARNING, "service_warning");

    handle_svc1();

    ok(svc1->state_type == SOFT_STATE,
        "Got soft state since non-ok state");
    ok(svc1->current_attempt == 1, 
        "Previous status was OK, so this failure should show current_attempt=1") 
        || diag("Current attempt=%d", svc1->current_attempt);
    test_svc_handler_notification_logging(3, EVENT_HANDLED | LOGGED);

    /* 2nd warning */
    svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;
    svc1->last_notification = (time_t) 11111L;
    svc1->next_notification = (time_t) 22222L;
    create_check_result(check_type, STATE_WARNING, "service warning");
    handle_svc1();

    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");
    ok(svc1->current_attempt == 2, 
        "Previous status was warning, so this failure should show current_attempt=2") 
        || diag("Current attempt=%d", svc1->current_attempt);
    ok(svc1->last_notification == (time_t) 11111L,
        "Last notification not reset");
    ok(svc1->next_notification == (time_t) 22222L,
        "Next notification not reset");
    test_svc_handler_notification_logging(4, EVENT_HANDLED);

    /* 3rd warning */
    create_check_result(check_type, STATE_WARNING, "service warning");
    handle_svc1();

    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");
    ok(svc1->current_attempt == 3, 
        "Previous status was warning, so this failure should show current_attempt=3") 
        || diag("Current attempt=%d", svc1->current_attempt);
    test_svc_handler_notification_logging(5, EVENT_HANDLED);

    /* 4th warning (HARD change) */
    create_check_result(check_type, STATE_WARNING, "service warning");
    svc1->last_notification = (time_t) 11111L;
    svc1->next_notification = (time_t) 22222L;
    svc1->no_more_notifications = TRUE;
    chk_result->start_time.tv_sec = ORIG_START_TIME + 40L;
    chk_result->finish_time.tv_usec = ORIG_FINISH_TIME + 40L;
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 40L), 
        "Got last_hard_state_change time=%lu", svc1->last_hard_state_change);
    ok(svc1->last_state_change == (time_t) ORIG_START_TIME, 
        "Got appropriate last_state_change");
    ok(svc1->last_notification == (time_t) 0L,
        "Last notification was reset");
    ok(svc1->next_notification == (time_t) 0L,
        "Next notification was reset");
    ok(svc1->no_more_notifications == FALSE,
        "No more notifications was reset");
    ok(svc1->last_hard_state == STATE_WARNING, 
        "Got last hard state as warning");
    ok(svc1->state_type == HARD_STATE,
        "Got hard state since max_attempts was reached");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");
    ok(svc1->current_attempt == 4, 
        "Previous status was warning, so this failure should show current_attempt=4") 
        || diag("Current attempt=%d", svc1->current_attempt);
    test_svc_handler_notification_logging(6, EVENT_HANDLED | NOTIFIED | LOGGED);

    /* 5th warning */
    create_check_result(check_type, STATE_WARNING, "service warning");
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 40L),
        "Got proper last hard state change (the last one) =%lu", svc1->last_hard_state_change);
    ok(svc1->state_type == HARD_STATE,
        "Should still be a hard state");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");
    ok(svc1->current_attempt == 4,
        "Current attempt shouldn't change as this is still a hard state");
    test_svc_handler_notification_logging(7, NOTIFIED);

    /* 6th non-ok (critical) */
    create_check_result(check_type, STATE_CRITICAL, "service critical");
    svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;
    chk_result->start_time.tv_sec = ORIG_START_TIME + 60L;
    chk_result->finish_time.tv_usec = ORIG_FINISH_TIME + 60L;
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 60L),
        "Got proper last hard state change (this one) =%lu", svc1->last_hard_state_change);
    ok(svc1->state_type == HARD_STATE,
        "Should still be a hard state");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "Ack reset");
    ok(svc1->current_attempt == 4,
        "Current attempt shouldn't change as this is still a hard state");
    test_svc_handler_notification_logging(8, EVENT_HANDLED | NOTIFIED | LOGGED);

    /* 7th, critical */
    create_check_result(check_type, STATE_CRITICAL, "service critical");
    svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 60L),
        "Got proper last hard state change (the last one) =%lu", svc1->last_hard_state_change);
    ok(svc1->state_type == HARD_STATE,
        "Should still be a hard state");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");
    ok(svc1->current_attempt == 4,
        "Current attempt shouldn't change as this is still a hard state");
    test_svc_handler_notification_logging(9, NOTIFIED);

    /* 8th, critical, new output, and enabling stalking for critical */
    create_check_result(check_type, STATE_CRITICAL, "service critical NEW");
    svc1->stalking_options = OPT_CRITICAL;
    handle_svc1();

    test_svc_handler_notification_logging(10, NOTIFIED | LOGGED);

    /* 9th, critical, new output, and enabling stalking only for OK */
    create_check_result(check_type, STATE_CRITICAL, "service critical NEWER");
    svc1->stalking_options = OPT_OK;
    handle_svc1();

    test_svc_handler_notification_logging(11, NOTIFIED);

    /* change state back to ok */
    create_check_result(check_type, STATE_OK, "service ok");
    svc1->last_notification = (time_t) 11111L;
    svc1->next_notification = (time_t) 22222L;
    svc1->no_more_notifications = TRUE;
    chk_result->start_time.tv_sec = ORIG_START_TIME + 80L;
    chk_result->finish_time.tv_usec = ORIG_FINISH_TIME + 80L;    
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 80L),
        "Got proper last hard state change (this one) =%lu", svc1->last_hard_state_change);
    ok(svc1->state_type == HARD_STATE,
        "Should still be a hard state because hard recovery");
    ok(svc1->current_state == STATE_OK,
        "Service is ok");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "Ack reset");
    ok(svc1->last_notification == (time_t) 0L,
        "Last notification was reset");
    ok(svc1->next_notification == (time_t) 0L,
        "Next notification was reset");
    ok(svc1->no_more_notifications == FALSE,
        "No more notifications was reset");
    ok(svc1->last_problem_id != 0,
        "Last problem ID set properly");
    ok(svc1->current_problem_id == 0,
        "Current problem ID reset");
    test_svc_handler_notification_logging(12, EVENT_HANDLED | NOTIFIED | LOGGED);

    /* 2nd ok */
    create_check_result(check_type, STATE_OK, "service ok");
    chk_result->start_time.tv_sec = ORIG_START_TIME + 90L;
    chk_result->finish_time.tv_usec = ORIG_FINISH_TIME + 90L;
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 80L),
        "Got proper last hard state change (this last one) =%lu", svc1->last_hard_state_change);
    ok(svc1->state_type == HARD_STATE,
        "Should still be a hard state");
    ok(svc1->current_state == STATE_OK,
        "Service is ok");
    test_svc_handler_notification_logging(13, EXPECT_NOTHING );

    /* 3rd ok, new output (stalking still enabled for ok) */
    create_check_result(check_type, STATE_OK, "service ok NEW");
    chk_result->start_time.tv_sec = ORIG_START_TIME + 90L;
    chk_result->finish_time.tv_usec = ORIG_FINISH_TIME + 90L;
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 80L),
        "Got proper last hard state change =%lu", svc1->last_hard_state_change);
    ok(svc1->state_type == HARD_STATE,
        "Should still be a hard state");
    ok(svc1->current_state == STATE_OK,
        "Service is ok");
    test_svc_handler_notification_logging(14, LOGGED );

/*
        svc->last_event_id = svc->current_event_id;
        svc->current_event_id = next_event_id;
        next_event_id++;
        svc->last_notification = (time_t)0;
        svc->next_notification = (time_t)0;
        svc->no_more_notifications = FALSE;


    // ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
    //     "Ack left");

    // adjust_check_result(check_type, STATE_OK, "Back to OK");
    // handle_async_service_check_result(svc1, chk_result);

    // ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
    //     "Ack reset to none");



    /* Test:
        OK -> WARNING 1/4 -> ack -> WARNING 2/4 -> WARNING 3/4 -> WARNING 4/4 -> WARNING 4/4 -> OK transition
        Tests that the ack is not removed on hard state change
    */
    // setup_objects(when);
    // adjust_check_result(check_type, STATE_OK, "Reset to OK");

    // hst1->current_state    = HOST_UP;

    // svc1->last_state        = STATE_OK;
    // svc1->last_hard_state   = STATE_OK;
    // svc1->current_state     = STATE_OK;
    // svc1->state_type        = SOFT_STATE;
    // svc1->current_attempt   = 1;

    // handle_async_service_check_result(svc1, chk_result);

    // ok(svc1->current_attempt == 1, 
    //     "Current attempt is 1")
    //     || diag("Current attempt now: %d", svc1->current_attempt);

    // adjust_check_result(check_type, STATE_WARNING, "WARNING failure");
    // handle_async_service_check_result(svc1, chk_result);

    // ok(svc1->state_type == SOFT_STATE, 
    //     "Soft state");
    // ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
    //     "No acks - testing transition to hard warning state");

    // svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;

    // ok(svc1->current_attempt == 1, 
    //     "Current attempt is 1") 
    //     || diag("Current attempt now: %d", svc1->current_attempt);

    // adjust_check_result(check_type, STATE_WARNING, "WARNING failure 2");
    // handle_async_service_check_result(svc1, chk_result);

    // ok(svc1->state_type == SOFT_STATE, 
    //     "Soft state");
    // ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
    //     "Ack left");
    // ok(svc1->current_attempt == 2, 
    //     "Current attempt is 2") 
    //     || diag("Current attempt now: %d", svc1->current_attempt);

    // adjust_check_result(check_type, STATE_WARNING, "WARNING failure 3");
    // handle_async_service_check_result(svc1, chk_result);

    // ok(svc1->state_type == SOFT_STATE, 
    //     "Soft state");
    // ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
    //     "Ack left");
    // ok(svc1->current_attempt == 3, 
    //     "Current attempt is 3") 
    //     || diag("Current attempt now: %d", svc1->current_attempt);

    // adjust_check_result(check_type, STATE_WARNING, "WARNING failure 4");
    // handle_async_service_check_result(svc1, chk_result);

    // ok(svc1->state_type == HARD_STATE, 
    //     "Hard state");
    // ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
    //     "Ack left on hard failure");
    // ok(svc1->current_attempt == 4, 
    //     "Current attempt is 4") 
    //     || diag("Current attempt now: %d", svc1->current_attempt);

    // adjust_check_result(check_type, STATE_OK, "Back to OK");
    // handle_async_service_check_result(svc1, chk_result);

    // ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
    //     "Ack removed");



    /* Test:
        OK -> WARNING 1/1 -> ack -> WARNING -> OK transition
        Tests that the ack is not removed on 2nd warning, but is on OK
    */
    // setup_objects(when);
    // adjust_check_result(check_type, STATE_WARNING, "WARNING failure 1");

    // hst1->current_state     = HOST_UP;

    // svc1->last_state        = STATE_OK;
    // svc1->last_hard_state   = STATE_OK;
    // svc1->current_state     = STATE_OK;
    // svc1->state_type        = SOFT_STATE;
    // svc1->max_attempts      = 2;

    // handle_async_service_check_result(svc1, chk_result);

    // ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
    //     "No acks - testing transition to immediate hard then OK");

    // svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;

    // adjust_check_result(check_type, STATE_WARNING, "WARNING failure 2");
    // handle_async_service_check_result(svc1, chk_result);

    // ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
    //     "Ack left");

    // adjust_check_result(check_type, STATE_OK, "Back to OK");
    // handle_async_service_check_result(svc1, chk_result);

    // ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
    //     "Ack removed");



    /* Test:
        UP -> DOWN 1/4 -> ack -> DOWN 2/4 -> DOWN 3/4 -> DOWN 4/4 -> UP transition
        Tests that the ack is not removed on 2nd DOWN, but is on UP
    */
    // setup_objects(when);
    // adjust_check_result(HOST_CHECK_PASSIVE, HOST_DOWN, "DOWN failure 2");

    // hst1->current_state             = HOST_UP;
    // hst1->last_state                = HOST_UP;
    // hst1->last_hard_state           = HOST_UP;
    // hst1->state_type                = SOFT_STATE;
    // hst1->acknowledgement_type      = ACKNOWLEDGEMENT_NONE;
    // hst1->plugin_output             = strdup("");
    // hst1->long_plugin_output        = strdup("");
    // hst1->perf_data                 = strdup("");
    // hst1->check_command             = strdup("Dummy command required");
    // hst1->accept_passive_checks     = TRUE;

    // passive_host_checks_are_soft    = TRUE;

    // handle_async_host_check_result(hst1, chk_result);

    // ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
    //     "No ack set");
    // ok(hst1->current_attempt == 2, 
    //     "Attempts right (not sure why this goes into 2 and not 1)") 
    //     || diag("current_attempt=%d", hst1->current_attempt);
    // ok(strcmp(hst1->plugin_output, "DOWN failure 2") == 0, 
    //     "output set") 
    //     || diag("plugin_output=%s", hst1->plugin_output);

    // hst1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;

    // adjust_check_result(HOST_CHECK_PASSIVE, HOST_DOWN, "DOWN failure 3");
    // handle_async_host_check_result(hst1, chk_result);

    // ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
    //     "Ack should be retained as in soft state");
    // ok(hst1->current_attempt == 3, 
    //     "Attempts incremented") 
    //     || diag("current_attempt=%d", hst1->current_attempt);
    // ok(strcmp(hst1->plugin_output, "DOWN failure 3") == 0, 
    //     "output set") 
    //     || diag("plugin_output=%s", hst1->plugin_output);

    // adjust_check_result(HOST_CHECK_PASSIVE, HOST_DOWN, "DOWN failure 4");
    // handle_async_host_check_result(hst1, chk_result);

    // ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
    //     "Ack should be retained as in soft state");
    // ok(hst1->current_attempt == 4, 
    //     "Attempts incremented") 
    //     || diag("current_attempt=%d", hst1->current_attempt);
    // ok(strcmp(hst1->plugin_output, "DOWN failure 4") == 0, 
    //     "output set") 
    //     || diag("plugin_output=%s", hst1->plugin_output);

    // adjust_check_result(HOST_CHECK_PASSIVE, HOST_UP, "UP again");
    // handle_async_host_check_result(hst1, chk_result);

    // ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
    //     "Ack reset due to state change");
    // ok(hst1->current_attempt == 1, 
    //     "Attempts reset") 
    //     || diag("current_attempt=%d", hst1->current_attempt);
    // ok(strcmp(hst1->plugin_output, "UP again") == 0, 
    //     "output set") 
    //     || diag("plugin_output=%s", hst1->plugin_output);

    free_all();
}

int main(int argc, char **argv)
{
    time_t now = 0L;

    execute_host_checks             = TRUE;
    execute_service_checks          = TRUE;
    accept_passive_host_checks      = TRUE;
    accept_passive_service_checks   = TRUE;

    //plan_tests(96);

    time(&now);

    run_check_tests(CHECK_TYPE_ACTIVE, now);
    run_check_tests(CHECK_TYPE_PASSIVE, now);


    return exit_status();
}
