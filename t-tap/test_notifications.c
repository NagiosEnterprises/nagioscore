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

#define TEST_NOTIFICATIONS_C
#define NSCORE 1

#include "config.h"

#include <unistd.h>
#include <stdio.h>

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
#include "stub_iobroker.c"

#define HOST_NAME       "hst1"
#define HOST_ADDRESS    "127.0.0.1"
#define HOST_COMMAND    "hst1_command"
#define SERVICE_NAME    "svc1"

service         * svc1          = NULL;
host            * hst1          = NULL;
host            * parent1       = NULL;
host            * parent2       = NULL;

int c                   = 0;


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

void free_all()
{
    free_parent1();
    free_parent2();
    free_hst1();
    free_svc1();
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
    hst1->notification_options       = OPT_ALL;
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
        svc1->notification_options       = OPT_ALL;
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

void run_service_tests() {

    int result = OK;

    // debug_level = DEBUGL_NOTIFICATIONS;
    // debug_verbosity = 2;

    enable_notifications = FALSE;

    // Forced notifications always pass
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, HARD_STATE, "service ok");
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_FORCED);
    ok(result == OK, "Notification Forced - Service should notify");

    // No notifications when notifications are disabled
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_CRITICAL, HARD_STATE, "service critical");
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Notifications disabled - Service should NOT notify");

    enable_notifications = TRUE;

    // All parents are bad
    create_objects(STATE_DOWN, HARD_STATE, "host down", STATE_CRITICAL, HARD_STATE, "service critical");
    setup_parents();
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "All parents are down - Service should NOT notify");

    // Not all parents are bad
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_CRITICAL, HARD_STATE, "service critical");
    setup_parents();
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == OK, "Not all parents are down - Service should notify");

    free_all();

    // Timeperiod checking

    // Notifications disabled for service
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_CRITICAL, HARD_STATE, "service critical");
    svc1->notifications_enabled = FALSE;
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Service notifications disabled - Service should NOT notify");

    // Custom notifications
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, HARD_STATE, "service ok");
    result = check_service_notification_viability(svc1, NOTIFICATION_CUSTOM, NOTIFICATION_OPTION_NONE);
    ok(result == OK, "Custom notification - Service should notify");

    // Custom Notification in downtime
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, HARD_STATE, "service ok");
    svc1->scheduled_downtime_depth = 1;
    result = check_service_notification_viability(svc1, NOTIFICATION_CUSTOM, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Custom notification in downtime - Service should NOT notify");

    // Acknowledgement
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_CRITICAL, HARD_STATE, "service critical");
    result = check_service_notification_viability(svc1, NOTIFICATION_ACKNOWLEDGEMENT, NOTIFICATION_OPTION_NONE);
    ok(result == OK, "Acknowledgement - Service should notify");

    // Acknowledgement while ok
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, HARD_STATE, "service ok");
    result = check_service_notification_viability(svc1, NOTIFICATION_ACKNOWLEDGEMENT, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Acknowledgement while OK - Service should NOT notify");

    // Service Flapping start
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, HARD_STATE, "service ok");
    result = check_service_notification_viability(svc1, NOTIFICATION_FLAPPINGSTART, NOTIFICATION_OPTION_NONE);
    ok(result == OK, "Flapping started - Service should notify");

    // Service Flapping start while in downtime
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, HARD_STATE, "service ok");
    svc1->scheduled_downtime_depth = 1;
    result = check_service_notification_viability(svc1, NOTIFICATION_FLAPPINGSTART, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Flapping started in downtime - Service should NOT notify");

    // Service downtime start
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, HARD_STATE, "service ok");
    result = check_service_notification_viability(svc1, NOTIFICATION_DOWNTIMESTART, NOTIFICATION_OPTION_NONE);
    ok(result == OK, "Downtime started - Service should notify");

    // Service downtime start while in downtime (Sounds wrong)
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, HARD_STATE, "service ok");
    svc1->scheduled_downtime_depth = 1;
    result = check_service_notification_viability(svc1, NOTIFICATION_DOWNTIMESTART, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Downtime started in downtime - Service should NOT notify");

    // Soft states don't get notifications
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_CRITICAL, SOFT_STATE, "service critical");
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Host: up,hard - Service: critical,soft - Service should NOT notify");

    // Problem has already been acknowledged
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_CRITICAL, HARD_STATE, "service critical");
    svc1->problem_has_been_acknowledged = TRUE;
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Service problem acknowledged - Service should NOT notify");

    // Host and Service Dependencies

    // Notification options for the service don't match
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_CRITICAL, HARD_STATE, "service critical");
    svc1->notification_options = OPT_NOTHING;
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Service options don't have critical - Service should NOT notify");

    // Recovery notification
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, HARD_STATE, "service ok");
    svc1->notified_on = 1;
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == OK, "Recovery notification - Service should notify");

    // Recovery notification without matching error notification
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_OK, HARD_STATE, "service ok");
    svc1->notified_on = 0;
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Recovery notification without matching error - Service should NOT notify");

    // Enough time between last notification

    // Service is flapping
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_CRITICAL, HARD_STATE, "service critical");
    svc1->is_flapping = TRUE;
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Service is flapping - Service should NOT notify");

    // Service in downtime
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_CRITICAL, HARD_STATE, "service critical");
    svc1->scheduled_downtime_depth = 1;
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Service is in downtime - Service should NOT notify");

    // Parent host in downtime
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_CRITICAL, HARD_STATE, "service critical");
    hst1->scheduled_downtime_depth = 1;
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Parent host is in downtime - Service should NOT notify");

    // Host is down, service shouldn't get notification
    create_objects(STATE_DOWN, HARD_STATE, "host down", STATE_CRITICAL, HARD_STATE, "service critical");
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == ERROR, "Host: down,hard - Service: critical,hard - Service should NOT notify");

    // When you should be typically notified
    create_objects(STATE_UP, HARD_STATE, "host up", STATE_CRITICAL, HARD_STATE, "service critical");
    result = check_service_notification_viability(svc1, NOTIFICATION_NORMAL, NOTIFICATION_OPTION_NONE);
    ok(result == OK, "Host: up,hard - Service: critical,hard - Service should notify");

    // Some other thing with not notifying again too soon

    free_all();
}

void run_host_tests() {

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

    plan_tests(23);

    time(&now);

    run_service_tests();
    run_host_tests();

    return exit_status();
}