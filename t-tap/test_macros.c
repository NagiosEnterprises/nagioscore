/*****************************************************************************
 *
 * test_macros.c - Test macro expansion and escaping
 *
 * Program: Nagios Core Testing
 * License: GPL
 *
 * First Written:   2013-05-21
 *
 * Description:
 *
 * Tests expansion of macros and escaping.
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/

#define TEST_MACROS_C
#include <string.h>
#include <time.h>

#include "config.h"
#include "common.h"
#include "nagios.h"
#include "downtime.h"
#include "perfdata.h"
#include "comments.h"
#include "statusdata.h"
#include "macros.h"
#include "broker.h"
#include "../lib/lnag-utils.h"
#include "tap.h"

#include "stub_sehandlers.c"
#include "stub_comments.c"
#include "stub_perfdata.c"
#include "stub_downtime.c"
#include "stub_notifications.c"
#include "stub_logging.c"
#include "stub_broker.c"
#include "stub_workers.c"
#include "stub_events.c"
#include "stub_statusdata.c"
#include "stub_flapping.c"
#include "stub_nebmods.c"
#include "stub_netutils.c"
#include "stub_commands.c"

host * hst1         = NULL;
service * svc1      = NULL;
nagios_macros * mac = NULL;
hostgroup * hstgrp1 = NULL;
servicegroup * svcgrp1 = NULL;
objectlist * hostgroups_ptr = NULL;
objectlist * servicegroups_ptr = NULL;

#define NO_OPTIONS 0

void free_svc1()
{
    if (svc1 != NULL) {
        my_free(svc1->host_name);
        my_free(svc1->description);
        my_free(svc1->check_command);
        my_free(svc1->notes_url);
        my_free(svc1->notes);
        my_free(svc1->action_url);
        my_free(svc1->plugin_output);
        my_free(svc1);
    }
}

void free_hst1()
{
    if (hst1 != NULL) {
        my_free(hst1->name);
        my_free(hst1->address);
        my_free(hst1->check_command);
        my_free(hst1->notes_url);
        my_free(hst1->notes);
        my_free(hst1->action_url);
        my_free(hst1->plugin_output);
        my_free(hst1);
    }
}

void free_hstgrp1()
{
    if (hostgroups_ptr != NULL) {
        my_free(hostgroups_ptr);
    }
    if (hstgrp1 != NULL) {
        if (hstgrp1->members != NULL) {
            my_free(hstgrp1->members->host_name);
        }
        my_free(hstgrp1->members);
        my_free(hstgrp1->group_name);
        my_free(hstgrp1->notes);
        my_free(hstgrp1);
    }
}

void free_svcgrp1()
{
    if (servicegroups_ptr != NULL) {
        my_free(servicegroups_ptr);
    }
    if (svcgrp1 != NULL) {
        if (svcgrp1->members != NULL) {
            my_free(svcgrp1->members->host_name);
            my_free(svcgrp1->members->service_description);
        }
        my_free(svcgrp1->members);
        my_free(svcgrp1->group_name);
        my_free(svcgrp1->notes);
        my_free(svcgrp1);
    }
}

void setup_objects()
{
    free_hst1();
    free_svc1();
    free_hstgrp1();
    free_svcgrp1();

    hst1              = (host *) calloc(1, sizeof(host));
    svc1              = (service *) calloc(1, sizeof(service));
    hstgrp1           = (hostgroup *) calloc(1, sizeof(hostgroup));
    svcgrp1           = (servicegroup *) calloc(1, sizeof(servicegroup));
    hostgroups_ptr    = (objectlist *) calloc(1, sizeof(objectlist));
    servicegroups_ptr = (objectlist *) calloc(1, sizeof(objectlist));
    mac               = (nagios_macros *) calloc(1, sizeof(nagios_macros));

    hst1->name                            = strdup("name'&%");
    hst1->address                         = strdup("address'&%");
    hst1->check_command                   = strdup("$USER1$");
    hst1->notes_url                       = strdup("notes_url'&%($HOSTNOTES$)");
    hst1->notes                           = strdup("notes'&%($HOSTACTIONURL$)");
    hst1->action_url                      = strdup("action_url'&%");
    hst1->plugin_output                   = strdup("name'&%");

    svc1->host_name                       = strdup("name'&%");
    svc1->description                     = strdup("service'&&%");
    svc1->host_ptr                        = hst1;
    svc1->check_command                   = strdup("$USER1$");
    svc1->notes_url                       = strdup("notes_url'&%($SERVICENOTES$)");
    svc1->notes                           = strdup("notes'&%($SERVICEACTIONURL$)");
    svc1->action_url                      = strdup("action_url'&%");
    svc1->plugin_output                   = strdup("name'&%");

    hstgrp1->group_name                   = strdup("hstgrp1");
    hstgrp1->notes                        = strdup("&&&notes!");
    hstgrp1->next                         = NULL;
    hstgrp1->members                      = (hostsmember *) calloc(1, sizeof(hostsmember));
    hstgrp1->members->host_name           = strdup(hst1->name);
    hstgrp1->members->host_ptr            = hst1;
    hstgrp1->members->next                = NULL;

    svcgrp1->group_name                   = strdup("svcgrp1");
    svcgrp1->notes                        = strdup("$SERVICEGROUPNAME$&notes?!");
    svcgrp1->next                         = NULL;
    svcgrp1->members                      = (servicesmember *) calloc(1, sizeof(servicesmember));
    svcgrp1->members->host_name           = strdup(svc1->host_name);
    svcgrp1->members->service_description = strdup(svc1->description);
    svcgrp1->members->service_ptr         = svc1;
    svcgrp1->members->next                = NULL;

    hostgroups_ptr->object_ptr            = hstgrp1;
    hst1->hostgroups_ptr                  = hostgroups_ptr;

    servicegroups_ptr->object_ptr         = svcgrp1;
    svc1->servicegroups_ptr               = servicegroups_ptr;

    grab_host_macros_r(mac, hst1);
    grab_service_macros_r(mac, svc1);
    grab_hostgroup_macros_r(mac, hstgrp1);
    grab_servicegroup_macros_r(mac, svcgrp1);
}

void setup_environment()
{
    char *p;

    my_free(illegal_output_chars);
    illegal_output_chars = strdup("'&"); /* For this tests, remove ' and & */

    /* This is a part of preflight check, which we can't run */
    for (p = illegal_output_chars; *p; p++) {
        illegal_output_char_map[(int) *p] = 1;
    }

    my_free(website_url);
    website_url = strdup("https://nagios.com");
}

#define RUN_MACRO_TEST(_STR, _EXPECT, _OPTS)                                                        \
    do {                                                                                            \
        if (process_macros_r(mac, (_STR), &output, _OPTS) == OK) {                                  \
            ok(strcmp(output, _EXPECT) == 0, "'%s': '%s' == '%s'", (_STR), output, (_EXPECT));      \
        } else {                                                                                    \
            fail("process_macros_r returns ERROR for " _STR);                                       \
        }                                                                                           \
        my_free(output);                                                                            \
    } while(0)

#define ALLOC_MACROS(_STR)                                                                          \
    do {                                                                                            \
        if (process_macros_r(mac, (_STR), &output, NO_OPTIONS) == OK) {                             \
            ok(0 == 0, "Called macros (%s) that require allocation (%s)", (_STR), output);          \
        } else {                                                                                    \
            fail("process_macros_r returns ERROR for " _STR);                                       \
        }                                                                                           \
        my_free(output);                                                                            \
    } while (0)

void test_escaping(nagios_macros *mac)
{
    char * output = NULL;

    /*
        Nothing should be changed
    */
    RUN_MACRO_TEST(
        "$HOSTNAME$ '&%", 
        "name'&% '&%", 
        NO_OPTIONS);

    /*
        Nothing should be changed... HOSTNAME doesn't accept STRIP_ILLEGAL_MACRO_CHARS
    */
    RUN_MACRO_TEST(
        "$HOSTNAME$ '&%", 
        "name'&% '&%", 
        STRIP_ILLEGAL_MACRO_CHARS);

    /*
        ' and & should be stripped from the macro, according to
        init_environment(), but not from the initial string
    */
    RUN_MACRO_TEST(
        "$HOSTOUTPUT$ '&%", 
        "name% '&%", 
        STRIP_ILLEGAL_MACRO_CHARS);

    /*
        ESCAPE_MACRO_CHARS doesn't seem to do anything... exist always in pair
        with STRIP_ILLEGAL_MACRO_CHARS
    */
    RUN_MACRO_TEST(
        "$HOSTOUTPUT$ '&%", 
        "name'&% '&%", 
        ESCAPE_MACRO_CHARS);
    RUN_MACRO_TEST(
        "$HOSTOUTPUT$ '&%", 
        "name% '&%",
        STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);

    /*
        $HOSTNAME$ should be url-encoded, but not the tailing chars
    */
    RUN_MACRO_TEST(
        "$HOSTNAME$ '&%", 
        "name%27%26%25 '&%",
        URL_ENCODE_MACRO_CHARS);

    /*
        The notes in the notesurl should be url-encoded, no more encoding should
        exist
    */
    RUN_MACRO_TEST(
        "$HOSTNOTESURL$ '&%",
        "notes_url'&%(notes%27%26%25%28action_url%27%26%25%29) '&%",
        NO_OPTIONS);

    /*
        '& in the source string shouldn't be removed, because HOSTNOTESURL
        doesn't accept STRIP_ILLEGAL_MACRO_CHARS, as in the url. the macros
        included in the string should be url-encoded, and therefore not contain &
        and '
    */
    RUN_MACRO_TEST(
        "$HOSTNOTESURL$ '&%",
        "notes_url'&%(notes%27%26%25%28action_url%27%26%25%29) '&%",
        STRIP_ILLEGAL_MACRO_CHARS);

    /*
        This should double-encode some chars ($HOSTNOTESURL$ should contain
        url-encoded chars, and should itself be url-encoded
    */
    RUN_MACRO_TEST(
        "$HOSTNOTESURL$ '&%",
        "notes_url%27%26%25%28notes%2527%2526%2525%2528action_url%2527%2526%2525%2529%29 '&%",
        URL_ENCODE_MACRO_CHARS);

    RUN_MACRO_TEST(
        "$HOSTINFOURL$ && $SERVICEINFOURL$",
        "https://nagios.com/cgi-bin/extinfo.cgi?type=1&host=name%27%26%25 && https://nagios.com/cgi-bin/extinfo.cgi?type=2&host=name%27%26%25&service=service%27%26%26%25",
        NO_OPTIONS);

    /*
        These are a few macros that require an alloc() of some kind
        So I just throw them in to do memory leak testing and make sure it's
        handled properly
    */
    RUN_MACRO_TEST(
        "$HOSTGROUPNAME$ $HOSTGROUPNAMES$ $SERVICEGROUPNAME$ $SERVICEGROUPNAMES$",
        "hstgrp1 hstgrp1 svcgrp1 svcgrp1",
        NO_OPTIONS);

    RUN_MACRO_TEST(
        "$HOSTGROUPNOTES$ $SERVICEGROUPNOTES$",
        "&&&notes! svcgrp1&notes?!",
        NO_OPTIONS);

    RUN_MACRO_TEST(
        "$HOSTACTIONURL$ $SERVICEACTIONURL$",
        "action_url'&% action_url'&%",
        NO_OPTIONS);

    RUN_MACRO_TEST(
        "$HOSTGROUPMEMBERS$ $HOSTGROUPMEMBERADDRESSES$",
        "name'&% address'&%",
        NO_OPTIONS);

    RUN_MACRO_TEST(
        "$SERVICEGROUPMEMBERS$",
        "name'&%,service'&&%",
        NO_OPTIONS);

    ALLOC_MACROS("$LONGDATETIME$ - $SHORTDATETIME$");
    ALLOC_MACROS("$DATE$ - $TIME$ - $TIMET$");
    ALLOC_MACROS("$TOTALHOSTSUP$ - $TOTALHOSTSDOWN$ - $TOTALHOSTSUNREACHABLE$");
    ALLOC_MACROS("$TOTALHOSTSDOWNUNHANDLED$ - $TOTALHOSTSUNREACHABLEUNHANDLED$ - $TOTALHOSTPROBLEMS$");
    ALLOC_MACROS("$TOTALHOSTPROBLEMSUNHANDLED$ - $TOTALSERVICESOK$ - $TOTALSERVICESWARNING$");
    ALLOC_MACROS("$TOTALSERVICESCRITICAL$ - $TOTALSERVICESUNKNOWN$ - $TOTALSERVICESWARNINGUNHANDLED$");
    ALLOC_MACROS("$TOTALHOSTSDOWNUNHANDLED$ - $TOTALSERVICESUNKNOWNUNHANDLED$ - $TOTALSERVICEPROBLEMS$");
    ALLOC_MACROS("$TOTALSERVICESCRITICALUNHANDLED$");
}

/*****************************************************************************/
/*                             Main function                                 */
/*****************************************************************************/

int main(void) {

    plan_tests(23);

    reset_variables();
    setup_environment();
    setup_objects();

    test_escaping(mac);

    free_memory(mac);
    free(mac);
    free_hstgrp1();
    free_svcgrp1();
    free_svc1();
    free_hst1();

    return exit_status();
}
