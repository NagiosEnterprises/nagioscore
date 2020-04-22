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

#include <unistd.h>

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

#define NO_SERVICE      0,0,NULL

extern int date_format;
int test_check_debugging = FALSE;

service         * svc1          = NULL;
host            * hst1          = NULL;
host            * parent1       = NULL;
host            * parent2       = NULL;
check_result    * chk_result    = NULL;

int hst1_notifications  = 0;
int svc1_notifications  = 0;
int hst1_event_handlers = 0;
int svc1_event_handlers = 0;
int hst1_logs           = 0;
int svc1_logs           = 0;
int c                   = 0;

int get_host_check_return_code(host *hst, check_result *cr);
int get_service_check_return_code(service *svc, check_result *cr);

void free_host(host ** hst)
{
    if ((*hst) != NULL) {

        my_free((*hst)->name);
        my_free((*hst)->address);
        my_free((*hst)->plugin_output);
        my_free((*hst)->long_plugin_output);
        my_free((*hst)->perf_data);
        my_free((*hst)->check_command);

        if ((*hst)->parent_hosts != NULL) {
            my_free((*hst)->parent_hosts->next);
            my_free((*hst)->parent_hosts);
        }

        my_free((*hst));
    }    
}

void free_parent1()
{
    free_host(&parent1);
}

void free_parent2()
{
    free_host(&parent2);
}

void free_hst1()
{
    free_host(&hst1);
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
    free_parent1();
    free_parent2();
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
    hst1->last_hard_state            = STATE_UP;
    hst1->notifications_enabled      = TRUE;
    hst1->event_handler_enabled      = TRUE;
    hst1->accept_passive_checks      = TRUE;
    hst1->initial_state              = STATE_UP;
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

void create_check_result_file(int number, char * host_name, char * service_description, char * output)
{
    FILE * cr;
    char * filename = NULL;
    char * ok_filename = NULL;

    asprintf(&filename, "var/reaper/some_files/c%06d", number);
    asprintf(&ok_filename, "%s.ok", filename);

    cr = fopen(filename, "w");
    if (cr == NULL) {
        printf("Unable to open %s for writing.\n", filename);
        exit(1);
    }

    if (host_name != NULL) {
        fprintf(cr, "host_name=%s\n", host_name);
    }

    if (service_description != NULL) {
        fprintf(cr, "service_description=%s\n", service_description);
    }

    if (output != NULL) {
        fprintf(cr, "output=%s\n", output);
    }

    fclose(cr);

    cr = fopen(ok_filename, "w");
    if (cr == NULL) {
        printf("Unable to open %s for writing.\n", ok_filename);
        exit(1);
    }

    fclose(cr);

    my_free(filename);
    my_free(ok_filename);
}

void setup_parent(host ** parent, const char * host_name)
{
    (*parent) = (host *) calloc(1, sizeof(host));

    (*parent)->name             = strdup(host_name);
    (*parent)->address          = strdup(HOST_ADDRESS);
    (*parent)->check_command    = strdup(HOST_COMMAND);
    (*parent)->retry_interval   = 1;
    (*parent)->check_interval   = 5;
    (*parent)->current_attempt  = 1;
    (*parent)->max_attempts     = 4;
    (*parent)->check_options    = CHECK_OPTION_NONE;
    (*parent)->has_been_checked = TRUE;
    (*parent)->current_state    = STATE_DOWN;

}

void setup_parents()
{
    hostsmember * tmp_hsts = NULL;
    tmp_hsts = (hostsmember *) calloc(1, sizeof(hostsmember));

    setup_parent(&parent1, "Parent 1");
    setup_parent(&parent2, "Parent 2");

    tmp_hsts->host_ptr = parent1;
    tmp_hsts->next = (hostsmember *) calloc(1, sizeof(hostsmember));
    tmp_hsts->next->host_ptr = parent2;

    hst1->parent_hosts = tmp_hsts;
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

void run_service_tests(int check_type)
{
    int tmp = 0;

    /******************************************
    SERVICE CHECK TESTING
    ******************************************/
    /*
        Host down/hard, Service crit/soft -> warn
    */
    create_objects(STATE_DOWN, HARD_STATE, "host down", STATE_CRITICAL, SOFT_STATE, "service critical");
    create_check_result(check_type, STATE_WARNING, "service warning");

    svc1->last_notification             = (time_t) 11111L;
    svc1->next_notification             = (time_t) 22222L;
    svc1->no_more_notifications         = TRUE;

    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) ORIG_START_TIME, 
        "Expected last_hard_state_change time %lu, got %lu", (time_t) ORIG_START_TIME, svc1->last_hard_state_change);
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
    test_svc_handler_notification_logging(1, EVENT_HANDLED | LOGGED);



    /*
        Host down/hard, Service ok/hard -> crit
    */
    create_objects(STATE_DOWN, HARD_STATE, "host down", STATE_OK, HARD_STATE, "service ok");
    create_check_result(check_type, STATE_CRITICAL, "service critical");

    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) ORIG_START_TIME, 
        "Expected last_hard_state_change time %lu, got %lu", (time_t) ORIG_START_TIME, svc1->last_hard_state_change);
    ok(svc1->last_state_change == svc1->last_hard_state_change, 
        "Got same last_state_change");
    ok(svc1->last_hard_state == STATE_CRITICAL, 
        "Got last hard state as critical");
    ok(svc1->state_type == HARD_STATE,
        "Got hard state since host is down");
    ok(svc1->host_problem_at_last_check == TRUE, 
        "Got host_problem_at_last_check set to TRUE due to host failure");
    ok(svc1->current_attempt == 1, 
        "Expecting current attempt %d, got %d", 1, svc1->current_attempt);
    test_svc_handler_notification_logging(2, EVENT_HANDLED | LOGGED);



    /*
        Host up/hard, Service ok/soft -> warn
    */
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, SOFT_STATE, "service ok");
    create_check_result(check_type, STATE_WARNING, "service warning");

    handle_svc1();

    ok(svc1->state_type == SOFT_STATE,
        "Got soft state since non-ok state");
    ok(svc1->current_attempt == 1, 
        "Expecting current attempt %d, got %d", 1, svc1->current_attempt);
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
        "Expecting current attempt %d, got %d", 2, svc1->current_attempt);
    ok(svc1->last_notification == (time_t) 11111L,
        "Last notification not reset");
    ok(svc1->next_notification == (time_t) 22222L,
        "Next notification not reset");
    test_svc_handler_notification_logging(4, EVENT_HANDLED | LOGGED);



    /* 3rd warning */
    create_check_result(check_type, STATE_WARNING, "service warning");
    handle_svc1();

    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");
    ok(svc1->current_attempt == 3, 
        "Expecting current attempt %d, got %d", 3, svc1->current_attempt);
    test_svc_handler_notification_logging(5, EVENT_HANDLED | LOGGED);



    /* 4th warning (HARD change) */
    create_check_result(check_type, STATE_WARNING, "service warning");
    svc1->last_notification = (time_t) 11111L;
    svc1->next_notification = (time_t) 22222L;
    svc1->no_more_notifications = TRUE;
    chk_result->start_time.tv_sec = ORIG_START_TIME + 40L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 40L;
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 40L), 
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 40L), svc1->last_hard_state_change);
    ok(svc1->last_state_change == (time_t) (ORIG_START_TIME + 40L), 
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
        "Expecting current attempt %d, got %d", 4, svc1->current_attempt);
    test_svc_handler_notification_logging(6, EVENT_HANDLED | NOTIFIED | LOGGED);



    /* 5th warning */
    create_check_result(check_type, STATE_WARNING, "service warning");
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 40L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 40L), svc1->last_hard_state_change);
    ok(svc1->state_type == HARD_STATE,
        "Should still be a hard state");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");
    ok(svc1->current_attempt == 4,
        "Expecting current attempt %d, got %d", 4, svc1->current_attempt);
    test_svc_handler_notification_logging(7, NOTIFIED);



    /* 6th non-ok (critical) */
    create_check_result(check_type, STATE_CRITICAL, "service critical");
    svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;
    chk_result->start_time.tv_sec = ORIG_START_TIME + 60L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 60L;
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 60L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 60L), svc1->last_hard_state_change);
    ok(svc1->state_type == HARD_STATE,
        "Should still be a hard state");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "Ack reset");
    ok(svc1->current_attempt == 4,
        "Expecting current attempt %d, got %d", 4, svc1->current_attempt);
    test_svc_handler_notification_logging(8, EVENT_HANDLED | NOTIFIED | LOGGED);



    /* 7th, critical */
    create_check_result(check_type, STATE_CRITICAL, "service critical");
    svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 60L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 60L), svc1->last_hard_state_change);
    ok(svc1->state_type == HARD_STATE,
        "Should still be a hard state");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");
    ok(svc1->current_attempt == 4,
        "Expecting current attempt %d, got %d", 4, svc1->current_attempt);
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
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 80L;    
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 80L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 80L), svc1->last_hard_state_change);
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
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 90L;
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 80L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 80L), svc1->last_hard_state_change);
    ok(svc1->state_type == HARD_STATE,
        "Should still be a hard state");
    ok(svc1->current_state == STATE_OK,
        "Service is ok");
    test_svc_handler_notification_logging(13, EXPECT_NOTHING );



    /* 3rd ok, new output (stalking still enabled for ok) */
    create_check_result(check_type, STATE_OK, "service ok NEW");
    chk_result->start_time.tv_sec = ORIG_START_TIME + 90L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 90L;
    handle_svc1();

    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 80L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 80L), svc1->last_hard_state_change);
    ok(svc1->state_type == HARD_STATE,
        "Should still be a hard state");
    ok(svc1->current_state == STATE_OK,
        "Service is ok");
    test_svc_handler_notification_logging(14, LOGGED );



    /*
        Host up/hard, Service warn/soft -> critical
    */
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_WARNING, SOFT_STATE, "service warning");
    create_check_result(check_type, STATE_CRITICAL, "service critical");
    svc1->last_state_change = ORIG_START_TIME + 20L;
    svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;
    chk_result->start_time.tv_sec = ORIG_START_TIME + 40L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 40L;
    tmp = svc1->current_event_id;
    handle_svc1();

    ok(svc1->current_event_id > tmp,
        "Event id incremented appropriately current: %d, old: %d", svc1->current_event_id, tmp);
    ok(svc1->current_state == STATE_CRITICAL,
        "Service is critical");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "Ack reset");
    ok(svc1->last_state_change == (time_t) ORIG_START_TIME + 40L,
        "Expected last_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 40L), svc1->last_state_change);
    ok(svc1->state_type == SOFT_STATE,
        "Service is still soft state");
    ok(svc1->current_attempt == 2,
        "Expecting current attempt %d, got %d", 2, svc1->current_attempt);
    test_svc_handler_notification_logging(15, EVENT_HANDLED | LOGGED );



    /* 3rd non-ok, critical. changing max_attempts to 3 because i'm impatient */
    create_check_result(check_type, STATE_CRITICAL, "service critical");
    svc1->max_attempts = 3;
    chk_result->start_time.tv_sec = ORIG_START_TIME + 60L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 60L;
    handle_svc1();

    ok(svc1->current_state == STATE_CRITICAL,
        "Service is critical");
    ok(svc1->last_state_change == (time_t) (ORIG_START_TIME + 60L),
        "Expected last_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 60L), svc1->last_state_change);
    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 60L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 60L), svc1->last_hard_state_change);
    ok(svc1->last_hard_state == STATE_CRITICAL,
        "Service last hard state was critical");
    ok(svc1->last_state == STATE_CRITICAL,
        "Service last state was critical");
    ok(svc1->state_type == HARD_STATE,
        "Service is hard state");
    ok(svc1->current_attempt == 3,
        "Expecting current attempt %d, got %d", 3, svc1->current_attempt);
    test_svc_handler_notification_logging(16, EVENT_HANDLED | NOTIFIED | LOGGED );



    /* 4th crit */
    create_check_result(check_type, STATE_CRITICAL, "service critical");
    handle_svc1();

    test_svc_handler_notification_logging(17, NOTIFIED );



    /* 5th crit */
    create_check_result(check_type, STATE_CRITICAL, "service critical");
    handle_svc1();

    test_svc_handler_notification_logging(18, NOTIFIED );



    /* turn on volatility, 6th crit */
    create_check_result(check_type, STATE_CRITICAL, "service critical");
    svc1->is_volatile = TRUE;
    handle_svc1();

    test_svc_handler_notification_logging(19, EVENT_HANDLED | NOTIFIED | LOGGED );



    /* one more before we go to a hard recovery */
    create_check_result(check_type, STATE_CRITICAL, "service critical");
    tmp = svc1->current_problem_id;
    handle_svc1();

    ok(svc1->current_problem_id == tmp,
        "Problem ID not reset current: %d, old: %d", svc1->current_problem_id, tmp);
    test_svc_handler_notification_logging(20, EVENT_HANDLED | NOTIFIED | LOGGED );



    /* hard recovery */
    create_check_result(check_type, STATE_OK, "service ok");
    chk_result->start_time.tv_sec = ORIG_START_TIME + 120L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 120L;
    tmp = svc1->current_event_id;
    svc1->last_notification = (time_t) ORIG_START_TIME + 90L;
    svc1->no_more_notifications = TRUE;
    svc1->acknowledgement_type = ACKNOWLEDGEMENT_STICKY;
    handle_svc1();

    ok(svc1->current_problem_id == 0,
        "Reset problem id %d", svc1->current_problem_id);
    ok(svc1->current_event_id > tmp,
        "Got new event id new: %d, old: %d", svc1->current_event_id, tmp);
    ok(svc1->no_more_notifications == FALSE,
        "Reset no more notifications");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE,
        "Reset ack");
    ok(svc1->last_notification == (time_t) 0L,
        "Reset last notification");
    ok(svc1->current_state == STATE_OK,
        "Service is ok");
    ok(svc1->last_state_change == (time_t) ORIG_START_TIME + 120L,
        "Expected last_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 120L), svc1->last_state_change);
    ok(svc1->last_hard_state_change == (time_t) (ORIG_START_TIME + 120L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 120L), svc1->last_hard_state_change);
    ok(svc1->last_hard_state == STATE_OK,
        "Service last hard state was ok");
    ok(svc1->last_state == STATE_CRITICAL,
        "Service last state was critical");
    ok(svc1->state_type == HARD_STATE,
        "Service is hard state");
    ok(svc1->current_attempt == 1,
        "Expecting current attempt %d, got %d",1, svc1->current_attempt);
    test_svc_handler_notification_logging(21, EVENT_HANDLED | NOTIFIED | LOGGED );

    free_all();
}


void run_active_host_tests()
{
    int tmp = 0;
    int check_type = CHECK_TYPE_ACTIVE;

    /******************************************
     ACTIVE HOST CHECK TESTING
    ******************************************/
    /*
        Test up -> up to make sure no weird log/notify/event
    */
    create_objects(STATE_UP, HARD_STATE, "host up", NO_SERVICE);
    create_check_result(check_type, STATE_OK, "host up");

    handle_hst1();

    test_hst_handler_notification_logging(22, EXPECT_NOTHING );



    /* turn stalking on, don't change output */
    create_check_result(check_type, STATE_OK, "host up");
    hst1->stalking_options = OPT_UP;

    handle_hst1();

    test_hst_handler_notification_logging(23, EXPECT_NOTHING );



    /* stalking still enabled, change output */
    create_check_result(check_type, STATE_OK, "host UP");

    handle_hst1();

    test_hst_handler_notification_logging(24, LOGGED );



    /*
        Test that warning state checks keep the host up unless aggressive checking
    */
    create_objects(STATE_UP, HARD_STATE, "host up", NO_SERVICE);
    create_check_result(check_type, STATE_WARNING, "host warning");
    use_aggressive_host_checking = FALSE;
    hst1->max_attempts = 3;

    handle_hst1();

    ok(hst1->current_state == STATE_UP,
        "Warning state is UP with no use_aggressive_host_checking");
    ok(hst1->state_type == HARD_STATE,
        "Still hard up");
    test_hst_handler_notification_logging(25, EXPECT_NOTHING );



    /* now with aggressive enabled */
    create_check_result(check_type, STATE_WARNING, "host warning");
    use_aggressive_host_checking = TRUE;
    tmp = hst1->current_event_id;

    handle_hst1();

    use_aggressive_host_checking = FALSE;
    ok(hst1->current_state == STATE_DOWN,
        "Warning state is DOWN with aggressive host checking");
    ok(hst1->current_attempt == 1,
        "Expecting current attempt %d, got %d", 1, hst1->current_attempt);
    ok(hst1->current_event_id > tmp,
        "Got new event id new: %d, old: %d", hst1->current_event_id, tmp);
    test_hst_handler_notification_logging(26, EVENT_HANDLED | LOGGED );



    /* host down */
    create_check_result(check_type, STATE_CRITICAL, "host down");

    handle_hst1();

    ok(hst1->current_state == STATE_DOWN,
        "Host state is DOWN");
    ok(hst1->state_type == SOFT_STATE,
        "Host state type is soft");
    ok(hst1->current_attempt == 2,
        "Expecting current attempt %d, got %d", 2, hst1->current_attempt);
    test_hst_handler_notification_logging(27, EVENT_HANDLED );



    /* host down (should be hard) */
    create_check_result(check_type, STATE_CRITICAL, "host down");
    chk_result->start_time.tv_sec = ORIG_START_TIME + 40L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 40L;

    handle_hst1();

    ok(hst1->current_state == STATE_DOWN,
        "Host state is DOWN");
    ok(hst1->state_type == HARD_STATE,
        "Host state type is hard");
    ok(hst1->current_attempt == 3,
        "Expecting current attempt %d, got %d", 3, hst1->current_attempt);
    ok(hst1->last_hard_state_change == (time_t) (ORIG_START_TIME + 40L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 40L), hst1->last_hard_state_change);
    test_hst_handler_notification_logging(28, EVENT_HANDLED | NOTIFIED | LOGGED );



    /* host down (should STILL be hard) */
    create_check_result(check_type, STATE_CRITICAL, "host down");
    hst1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;
    chk_result->start_time.tv_sec = ORIG_START_TIME + 60L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 60L;

    handle_hst1();

    ok(hst1->current_state == STATE_DOWN,
        "Host state is DOWN");
    ok(hst1->state_type == HARD_STATE,
        "Host state type is hard");
    ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");
    ok(hst1->current_attempt == 3,
        "Expecting current attempt %d, got %d", 3, hst1->current_attempt);
    ok(hst1->last_hard_state_change == (time_t) (ORIG_START_TIME + 40L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 40L), hst1->last_hard_state_change);
    test_hst_handler_notification_logging(29, NOTIFIED );



    /* lets test unreachability */
    setup_parents();
    create_check_result(check_type, STATE_CRITICAL, "host down");
    chk_result->start_time.tv_sec = ORIG_START_TIME + 80L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 80L;

    handle_hst1();

    ok(hst1->current_state == STATE_UNREACHABLE,
        "Host state is UNREACHABLE");
    ok(hst1->state_type == HARD_STATE,
        "Host state type is hard");
    ok(hst1->current_attempt == 3,
        "Expecting current attempt %d, got %d", 3, hst1->current_attempt);
    ok(hst1->last_hard_state_change == (time_t) (ORIG_START_TIME + 80L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 80L), hst1->last_hard_state_change);
    test_hst_handler_notification_logging(30, EVENT_HANDLED | NOTIFIED | LOGGED );



    /* hard recovery */
    create_check_result(check_type, STATE_OK, "host up");
    tmp = hst1->current_event_id;
    chk_result->start_time.tv_sec = ORIG_START_TIME + 90L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 90L;
    hst1->last_notification = (time_t) ORIG_START_TIME + 80L;
    hst1->no_more_notifications = TRUE;
    hst1->acknowledgement_type = ACKNOWLEDGEMENT_STICKY;

    handle_hst1();

    ok(hst1->current_problem_id == 0,
        "Reset problem id %d", hst1->current_problem_id);
    ok(hst1->current_event_id > tmp,
        "Got new event id new: %d, old: %d", hst1->current_event_id, tmp);
    ok(hst1->no_more_notifications == FALSE,
        "Reset no more notifications");
    ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NONE,
        "Reset ack");
    ok(hst1->last_notification == (time_t) 0L,
        "Reset last notification");
    ok(hst1->current_state == STATE_UP,
        "Host is up");
    ok(hst1->last_state_change == (time_t) ORIG_START_TIME + 90L,
        "Expected last_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 90L), hst1->last_state_change);
    ok(hst1->last_hard_state_change == (time_t) (ORIG_START_TIME + 90L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 90L), hst1->last_hard_state_change);
    ok(hst1->last_hard_state == STATE_UP,
        "Host last hard state was ok");
    ok(hst1->last_state == STATE_CRITICAL,
        "Host last state was down (critical)");
    ok(hst1->state_type == HARD_STATE,
        "Host is hard state");
    ok(hst1->current_attempt == 1,
        "Expecting current attempt %d, got %d",1, hst1->current_attempt);
    test_hst_handler_notification_logging(31, EVENT_HANDLED | NOTIFIED | LOGGED );


    free_all();
}

void run_passive_host_tests()
{
    int tmp1 = 0;
    int tmp2 = 0;
    int check_type = CHECK_TYPE_PASSIVE;

    /******************************************
     PASSIVE HOST CHECK TESTING
    ******************************************/
    /*
        Host up -> up
    */
    create_objects(STATE_UP, HARD_STATE, "host up", NO_SERVICE);
    create_check_result(check_type, STATE_UP, "host up");
    hst1->max_attempts = 3;

    /* this makes passive checks act just like regular checks */
    passive_host_checks_are_soft = TRUE;

    handle_hst1();
    
    ok(hst1->current_state == STATE_UP,
        "Host is up");
    ok(hst1->state_type == HARD_STATE,
        "Host is hard state");
    ok(hst1->current_attempt == 1,
        "Expecting current attempt %d, got %d", 1, hst1->current_attempt);
    test_hst_handler_notification_logging(32, EXPECT_NOTHING );



    /* send in a weird return code */
    create_check_result(check_type, 42, "host is right about everything");
    chk_result->start_time.tv_sec = (time_t) ORIG_START_TIME + 10L;
    chk_result->finish_time.tv_sec = (time_t) ORIG_FINISH_TIME + 10L;
    hst1->max_attempts = 3;
    
    handle_hst1();

    ok(hst1->current_state == 42,
        "Host is weird (42) (%d)", hst1->current_state);
    ok(hst1->state_type == SOFT_STATE,
        "Host is soft state");
    ok(hst1->current_attempt == 1,
        "Expecting current attempt %d, got %d", 1, hst1->current_attempt);
    ok(hst1->last_state_change == (time_t) (ORIG_START_TIME + 10L),
        "Expected last_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 10L), hst1->last_state_change);
    test_hst_handler_notification_logging(33, EVENT_HANDLED | LOGGED );
    


    /* send in weird return code again */
    create_check_result(check_type, 42, "host is right about everything");
    chk_result->start_time.tv_sec = (time_t) ORIG_START_TIME + 20L;
    chk_result->finish_time.tv_sec = (time_t) ORIG_FINISH_TIME + 20L;

    handle_hst1();

    ok(hst1->current_state == 42,
        "Host is weird (42) (%d)", hst1->current_state);
    ok(hst1->state_type == SOFT_STATE,
        "Host is soft state");
    ok(hst1->current_attempt == 2,
        "Expecting current attempt %d, got %d", 2, hst1->current_attempt);
    test_hst_handler_notification_logging(34, EVENT_HANDLED );
    


    /* send in weird return code again */
    create_check_result(check_type, 42, "host is right about everything");
    chk_result->start_time.tv_sec = (time_t) ORIG_START_TIME + 30L;
    chk_result->finish_time.tv_sec = (time_t) ORIG_FINISH_TIME + 30L;

    handle_hst1();

    ok(hst1->current_state == 42,
        "Host is weird (42) (%d)", hst1->current_state);
    ok(hst1->state_type == HARD_STATE,
        "Host is hard state");
    ok(hst1->current_attempt == 3,
        "Expecting current attempt %d, got %d", 3, hst1->current_attempt);
    ok(hst1->last_state_change == (time_t) ORIG_START_TIME + 10L,
        "Expected last_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 10L), hst1->last_state_change);
    ok(hst1->last_hard_state_change == (time_t) (ORIG_START_TIME + 30L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 30L), hst1->last_hard_state_change);
    ok(hst1->last_hard_state == 42,
        "Host last hard state was 42");
    ok(hst1->last_state == 42,
        "Host last state was 42");
    test_hst_handler_notification_logging(36, EVENT_HANDLED | NOTIFIED | LOGGED );



    /* hard recovery */
    create_check_result(check_type, STATE_OK, "host up");
    tmp1 = hst1->current_event_id;
    chk_result->start_time.tv_sec = ORIG_START_TIME + 40L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 40L;
    hst1->last_notification = (time_t) ORIG_START_TIME + 20L;
    hst1->no_more_notifications = TRUE;
    hst1->acknowledgement_type = ACKNOWLEDGEMENT_STICKY;

    handle_hst1();

    ok(hst1->current_problem_id == 0,
        "Reset problem id %d", hst1->current_problem_id);
    ok(hst1->current_event_id > tmp1,
        "Got new event id new: %d, old: %d", hst1->current_event_id, tmp1);
    ok(hst1->no_more_notifications == FALSE,
        "Reset no more notifications");
    ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NONE,
        "Reset ack");
    ok(hst1->last_notification == (time_t) 0L,
        "Reset last notification");
    ok(hst1->current_state == STATE_UP,
        "Host is up");
    ok(hst1->last_state_change == (time_t) ORIG_START_TIME + 40L,
        "Expected last_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 40L), hst1->last_state_change);
    ok(hst1->last_hard_state_change == (time_t) (ORIG_START_TIME + 40L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 40L), hst1->last_hard_state_change);
    ok(hst1->last_hard_state == STATE_UP,
        "Host last hard state was up");
    ok(hst1->last_state == 42,
        "Host last state was 42");
    ok(hst1->state_type == HARD_STATE,
        "Host is hard state");
    ok(hst1->current_attempt == 1,
        "Expecting current attempt %d, got %d",1, hst1->current_attempt);
    test_hst_handler_notification_logging(37, EVENT_HANDLED | NOTIFIED | LOGGED );



    /* back to the default */
    passive_host_checks_are_soft = FALSE;



    /* down (not critical like before, since now no translation happens) */
    create_check_result(check_type, STATE_DOWN, "host down");
    chk_result->start_time.tv_sec = ORIG_START_TIME + 50L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 50L;
    tmp1 = hst1->current_event_id;
    
    handle_hst1();

    ok(hst1->current_problem_id > 0,
        "Non-zero problem id %d", hst1->current_problem_id);
    ok(hst1->current_event_id > tmp1,
        "Got new event id new: %d, old: %d", hst1->current_event_id, tmp1);
    ok(hst1->current_state == STATE_DOWN,
        "Host is down");
    ok(hst1->last_hard_state == STATE_DOWN,
        "Host last hard state was down");
    ok(hst1->last_state_change == (time_t) ORIG_START_TIME + 50L,
        "Expected last_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 50L), hst1->last_state_change);
    ok(hst1->last_hard_state_change == (time_t) (ORIG_START_TIME + 50L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 50L), hst1->last_hard_state_change);
    ok(hst1->current_attempt == 1,
        "Expecting current attempt %d, got %d",1, hst1->current_attempt);
    test_hst_handler_notification_logging(38, EVENT_HANDLED | NOTIFIED | LOGGED );



    /* down again */
    create_check_result(check_type, STATE_DOWN, "host down");
    hst1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;
    hst1->no_more_notifications = TRUE;
    hst1->last_notification = (time_t) 11111L;
    chk_result->start_time.tv_sec = ORIG_START_TIME + 60L;
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME + 60L;
    tmp1 = hst1->current_event_id;
    tmp2 = hst1->current_problem_id;
    
    handle_hst1();

    ok(hst1->current_problem_id == tmp2,
        "Got same problem id new: %d, old: %d", hst1->current_problem_id, tmp2);
    ok(hst1->current_event_id == tmp1,
        "Got same event id new: %d, old: %d", hst1->current_event_id, tmp1);
    ok(hst1->current_state == STATE_DOWN,
        "Host is down");
    ok(hst1->last_hard_state == STATE_DOWN,
        "Host last hard state was down");
    ok(hst1->no_more_notifications == TRUE,
        "No more notifications reatained");
    ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL,
        "Ack same");
    ok(hst1->last_notification != (time_t)0L,
        "Last notification not reset");
    ok(hst1->last_state_change == (time_t) ORIG_START_TIME + 50L,
        "Expected last_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 50L), hst1->last_state_change);
    ok(hst1->last_hard_state_change == (time_t) (ORIG_START_TIME + 50L),
        "Expected last_hard_state_change time %lu, got %lu", (time_t) (ORIG_START_TIME + 50L), hst1->last_hard_state_change);
    ok(hst1->current_attempt == 1,
        "Expecting current attempt %d, got %d",1, hst1->current_attempt);
    test_hst_handler_notification_logging(39, NOTIFIED );



    /* soft recovery */
    create_check_result(check_type, STATE_UP, "host up");
    passive_host_checks_are_soft = TRUE;
    hst1->checks_enabled = TRUE;
    hst1->current_attempt = 1;
    hst1->current_state = STATE_DOWN;
    hst1->state_type = SOFT_STATE;

    /* after this guy, we get a reschedule event (schedule_host_check())
       which creates a new event. we need to free that memory when done */
    handle_hst1();

    test_hst_handler_notification_logging(40, EVENT_HANDLED | LOGGED );

    my_free(hst1->next_check_event);

    free_all();
}

void run_misc_host_check_tests()
{
    time_t now = 0L;
    int result = 0;
    int tmp1 = 0;
    int check_type = CHECK_TYPE_ACTIVE;

    time(&now);

    ok(handle_async_host_check_result(NULL, NULL) == ERROR,
        "handle check result is ERROR when objects are null");
    ok(run_async_host_check(NULL, 0, 0, 0, 0, &tmp1, &now) == ERROR,
        "run host check is ERROR when object is null");

    ok(get_host_check_return_code(NULL, NULL) == STATE_UNREACHABLE,
        "host check return code is STATE_UNREACHABLE when objects are null");
    ok(get_service_check_return_code(NULL, NULL) == STATE_UNKNOWN,
        "service check return code is STATE_UNKNOWN when objects are null");


    /*
        most of the rest of these checks
        just test for a bunch of specific scenarios
        this is the beauty of coverage reporting!
    */
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, HARD_STATE, "service up");

    create_check_result(check_type, STATE_DOWN, "host down");
    chk_result->early_timeout = TRUE;
    chk_result->latency             = 42;
    chk_result->start_time.tv_sec          = now;
    chk_result->finish_time.tv_sec         = now + 42L;

    handle_hst1();

    ok(strstr(hst1->plugin_output, "after 42.00 seconds") != NULL,
        "found appropriate early timeout message, [%s]", hst1->plugin_output);

    create_check_result(check_type, STATE_DOWN, "host down");
    chk_result->exited_ok = FALSE;

    handle_hst1();

    ok(strstr(hst1->plugin_output, "did not exit properly") != NULL,
        "found appropriate exited ok message, [%s]", hst1->plugin_output);

    create_check_result(check_type, 126, "host down");

    handle_hst1();

    ok(strstr(hst1->plugin_output, "126 is out of bounds") != NULL,
        "found appropriate non executable message, [%s]", hst1->plugin_output);

    create_check_result(check_type, 127, "host down");

    handle_hst1();

    ok(strstr(hst1->plugin_output, "127 is out of bounds") != NULL,
        "found appropriate non existent message, [%s]", hst1->plugin_output);

    create_check_result(check_type, -1, "host down");
    
    handle_hst1();

    ok(strstr(hst1->plugin_output, "code of -1") != NULL,
        "found appropriate lower bound message, [%s]", hst1->plugin_output);
    ok(hst1->current_state == STATE_DOWN,
        "-1 is STATE_DOWN, %d", result);

    create_check_result(check_type, 4, "host down");
    
    handle_hst1();

    ok(strstr(hst1->plugin_output, "code of 4") != NULL,
        "found appropriate lower bound message, [%s]", hst1->plugin_output);
    ok(hst1->current_state == STATE_DOWN,
        "4 is STATE_DOWN, %d", result);

    create_check_result(check_type, STATE_DOWN, "host down");
    my_free(hst1->check_command);
    hst1->check_command = NULL;

    handle_hst1();

    ok(hst1->current_state == STATE_UP,
        "null check command is always alright");

    /* we set this to true simply for the coverage report */
    log_passive_checks = TRUE;

    /* we tested both objects null, but now let's test with one or the other */
    result = handle_async_host_check_result(hst1, NULL);
    ok(result == ERROR,
        "handle_async_host_check_result is ERROR when hst1 is valid, but check result is null");

    accept_passive_host_checks = FALSE;
    create_check_result(CHECK_TYPE_PASSIVE, HOST_UP, "host up");
    result = handle_async_host_check_result(hst1, chk_result);
    ok(result == ERROR,
        "when accept_passive_host_checks is false, can't handle passive check result");

    accept_passive_host_checks = TRUE;
    create_check_result(CHECK_TYPE_PASSIVE, HOST_UP, "host up");
    hst1->accept_passive_checks = FALSE;
    result = handle_async_host_check_result(hst1, chk_result);
    ok(result == ERROR,
        "when hst->accept_passive_checks is false, can't handle passive check result");

    create_check_result(check_type, HOST_UP, "host up");
    hst1->is_being_freshened = TRUE;
    chk_result->check_options |= CHECK_OPTION_FRESHNESS_CHECK;

    handle_hst1();

    ok(hst1->is_being_freshened == FALSE,
        "freshening flag was reset");

    hst1->accept_passive_checks = TRUE;
    hst1->check_command = strdup("SOME COMMAND");
    create_check_result(check_type, HOST_UP, "host up");
    my_free(chk_result->output);
    my_free(hst1->plugin_output);

    handle_hst1();

    ok(strstr(hst1->plugin_output, "No output returned") != NULL,
        "If there was no plugin output, tell us [%s]", hst1->plugin_output);

    /* check if the execution times are weird */
    create_check_result(check_type, HOST_UP, "host up");
    chk_result->finish_time.tv_sec = ORIG_FINISH_TIME - 100L;

    handle_hst1();

    ok(hst1->execution_time == 0.0,
        "execution time gets fixed when finish time is before start time");

    create_check_result(check_type, HOST_UP, "host up");

    free_all();
}

/* todo: run_parent_host_check_tests() */

void run_misc_service_check_tests()
{
    time_t now = 0L;
    int result = 0;
    int tmp1 = 0;
    int check_type = CHECK_TYPE_ACTIVE;

    time(&now);

    ok(handle_async_service_check_result(NULL, NULL) == ERROR,
        "handle check result is ERROR when objects are null");
    ok(run_async_service_check(NULL, 0, 0, 0, 0, &tmp1, &now) == ERROR,
        "run service check is ERROR when object is null");
}

void run_reaper_tests()
{
    int result;
    /* test null dir */
    my_free(check_result_path);
    check_result_path = NULL;
    ok(process_check_result_queue(check_result_path) == ERROR,
        "null check result path is an error");

    /* bad dir */
    check_result_path = strdup("/i/hope/you/dont/have/this/directory/on/your/testing/system/");
    ok(process_check_result_queue(check_result_path) == ERROR,
        "cant open check result path is an error");
    my_free(check_result_path);

    /* Allow the check reaper to take awhile */
    max_check_reaper_time = 10;

    /* existing dir, with nothing in it */
    check_result_path = nspath_absolute("./../t-tap/var/reaper/no_files", NULL);
    result = process_check_result_queue(check_result_path);
    ok(result == 0,
        "%d files processed, expected 0 files", result);
    my_free(check_result_path);

    /* existing dir, with 2 check files in it */
    create_check_result_file(1, "hst1", "svc1", "output");
    create_check_result_file(2, "hst1", NULL, "output");
    check_result_path = nspath_absolute("./../t-tap/var/reaper/some_files", NULL);
    result = process_check_result_queue(check_result_path);
    /* This test is disabled until we have time to figure out debugging on Travis VMs. */
    /* ok(result == 2,
        "%d files processed, expected 2 files", result); */
    my_free(check_result_path);
    test_check_debugging=FALSE;

    /* do sig_{shutdown,restart} work as intended */
    sigshutdown = TRUE;
    check_result_path = nspath_absolute("./../t-tap/var/reaper/some_files", NULL);
    result = process_check_result_queue(check_result_path);
    ok(result == 0,
        "%d files processed, expected 0 files", result);
    sigshutdown = FALSE;
    sigrestart = TRUE;
    result = process_check_result_queue(check_result_path);
    ok(result == 0,
        "%d files processed, expected 0 files", result);

    /* force too long of a check */
    max_check_reaper_time = -5;
    sigrestart = FALSE;
    result = process_check_result_queue(check_result_path);
    ok(result == 0,
        "cant process if taking too long");
    my_free(check_result_path);

    /* we already tested all of this, but just to make coverage happy */
    reap_check_results();

}

int main(int argc, char **argv)
{
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("\n\n********** cwd: %s\n\n", cwd);
    } else {
        printf("\n\n********** cwd error!\n\n");
    }
    
    time_t now = 0L;

    execute_host_checks             = TRUE;
    execute_service_checks          = TRUE;
    accept_passive_host_checks      = TRUE;
    accept_passive_service_checks   = TRUE;

    /* Increment this when the check_reaper test is fixed */
    plan_tests(452);

    time(&now);

    run_service_tests(CHECK_TYPE_ACTIVE);
    run_service_tests(CHECK_TYPE_PASSIVE);

    /* the way host checks are handled is
       very different based on active/passive */
    run_active_host_tests();

    passive_host_checks_are_soft = TRUE;
    run_passive_host_tests();

    run_misc_host_check_tests(now);
    run_reaper_tests();

    return exit_status();
}
