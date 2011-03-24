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
#include "nagios.h"
#include "../base/commands.c"
#include "stub_broker.c"
#include "stub_comments.c"
#include "stub_objects.c"
#include "stub_statusdata.c"
#include "stub_notifications.c"
#include "stub_events.c"
#include "stub_downtime.c"
#include "stub_flapping.c"
#include "stub_logging.c"
#include "stub_utils.c"
#include "stub_sretention.c"
#include "stub_checks.c"
#include "tap.h"

void logit(int data_type, int display, const char *fmt, ...) {}
int log_debug_info(int level,int verbosity,const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        /* vprintf( fmt, ap ); */
        va_end(ap);
}

circular_buffer external_command_buffer;
time_t last_command_check;
int external_command_buffer_slots;
char *temp_path;
int date_format;
host *host_list;
service *service_list;
int accept_passive_service_checks;
time_t last_command_status_update;
int check_host_freshness;
int check_service_freshness;
int check_external_commands;
unsigned long modified_host_process_attributes;
unsigned long modified_service_process_attributes;
int enable_notifications;
int obsess_over_hosts;
int obsess_over_services;
int execute_service_checks;
int enable_event_handlers;
int accept_passive_host_checks;
int accept_passive_service_checks;
int enable_failure_prediction;
int process_performance_data;
int execute_host_checks;
int execute_service_checks;
char *global_service_event_handler;
command *global_service_event_handler_ptr;
char *global_host_event_handler;
command *global_host_event_handler_ptr;

/* Catch lower calls through these stubs */
time_t test_start_time=0L;
char *test_hostname;
char *test_servicename;
char *test_comment=NULL;
int deleted=1;
scheduled_downtime *find_host_downtime_by_name(char *hostname){}
scheduled_downtime *find_host_service_downtime_by_name(char *hostname, char *service_description){}
int delete_downtime_by_start_time_comment(time_t start_time, char *comment){
    test_start_time=start_time;
    test_comment=comment;
    return deleted;
}

int
main (int argc, char **argv)
{
	time_t now=0L;

    plan_tests(26);

    ok( test_start_time==0L, "Start time is empty");
    ok( test_comment==NULL, "And test_comment is blank");

    ok( cmd_delete_downtime_by_start_time_comment(1,"1234567890;This is a comment") == OK, "cmd_delete_downtime_by_start_time_comment" );
    ok( test_start_time==1234567890L, "Start time called correctly");
    ok( strcmp(test_comment,"This is a comment")==0, "comment set correctly");

    ok( cmd_delete_downtime_by_start_time_comment(1,"") == OK, "cmd_delete_downtime_by_start_time_comment" );
    ok( test_start_time==0L, "start time not parsed, so set to 0");
    ok( test_comment==NULL, "And a null passed through for comment");

    ok( cmd_delete_downtime_by_start_time_comment(1,"1234;Comment;") == OK, "cmd_delete_downtime_by_start_time_comment" );
    ok( test_start_time==1234, "Silly start time parsed but will get rejected lower down");
    ok( strcmp(test_comment,"Comment;")==0, "Comment with ; will get dropped: '%s' <--- not sure if this should be true or not", test_comment);

    ok( cmd_delete_downtime_by_start_time_comment(1,"1234;Comment with \n in it;") == OK, "cmd_delete_downtime_by_start_time_comment" );
    ok( strcmp(test_comment,"Comment with ")==0, "Comment truncated at \\n") || diag("comment=%s",test_comment);

    ok( cmd_delete_downtime_by_start_time_comment(1,";") == OK, "cmd_delete_downtime_by_start_time_comment" );
    ok( test_start_time==0L, "start time not parsed, so set to 0");
    ok( test_comment==NULL, "And a null passed through for comment");

    test_start_time=0L;
    test_comment="somethingelse";
    ok( cmd_delete_downtime_by_start_time_comment(1,"badtime") == OK, "cmd_delete_downtime_by_start_time_comment" );
    ok( test_start_time==0L, "bad start time not parsed, so set to 0");
    ok( test_comment==NULL, "And a null passed through for comment");

    ok( cmd_delete_downtime_by_start_time_comment(1,"123badtime;comment") == OK, "cmd_delete_downtime_by_start_time_comment" );
    ok( test_start_time==123L, "Partly bad start time is parsed, but that's an input problem");
    ok( strcmp(test_comment,"comment")==0, "Comment");

    ok( cmd_delete_downtime_by_start_time_comment(1,";commentonly") == OK, "cmd_delete_downtime_by_start_time_comment" );
    ok( test_start_time==0L, "No start time, but comment is set");
    ok( strcmp(test_comment,"commentonly")==0, "Comment");

    deleted=0;
    ok( cmd_delete_downtime_by_start_time_comment(1,";commentonly") == ERROR, "Got an error here because no items deleted");

	return exit_status ();
}

