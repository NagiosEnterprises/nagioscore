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
#include "stub_macros.c"
#include "tap.h"

char *temp_path;
int date_format;
host *host_list;
service *service_list;
int accept_passive_service_checks;
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
int process_performance_data;
int execute_host_checks;
int execute_service_checks;
char *global_service_event_handler;
command *global_service_event_handler_ptr;
char *global_host_event_handler;
command *global_host_event_handler_ptr;
char *command_file = NULL;
iobroker_set *nagios_iobs = NULL;
int sigrestart = FALSE;
int debug_level = 0;
int debug_verbosity = 0;

/* Catch lower calls through these stubs */
time_t test_start_time = 0L;
char *test_hostname;
char *test_servicename;
char *test_comment = NULL;
int deleted = 1;
scheduled_downtime *find_host_downtime_by_name(char *hostname) {}
scheduled_downtime *find_host_service_downtime_by_name(char *hostname, char *service_description) {}
int delete_downtime_by_start_time_comment(time_t start_time, char *comment) {
	test_start_time = start_time;
	test_comment = comment;
	return deleted;
	}

int delete_downtime_by_hostname_service_description_start_time_comment(char *hostname, char *service_description, time_t start_time, char *comment) {
	test_hostname = hostname;
	test_servicename = service_description;
	test_start_time = start_time;
	test_comment = comment;
	return deleted;
	}

void reset_vars() {
	test_start_time = 0L;
	test_hostname = NULL;
	test_servicename = NULL;
	test_comment = NULL;
	}

hostgroup *temp_hostgroup = NULL;

int
main() {
	time_t now = 0L;

	plan_tests(62);

	ok(test_start_time == 0L, "Start time is empty");
	ok(test_comment == NULL, "And test_comment is blank");

	ok(cmd_delete_downtime_by_start_time_comment(1, "1234567890;This is a comment") == OK, "cmd_delete_downtime_by_start_time_comment");
	ok(test_start_time == 1234567890L, "Start time called correctly");
	ok(strcmp(test_comment, "This is a comment") == 0, "comment set correctly");

	ok(cmd_delete_downtime_by_start_time_comment(1, "") == ERROR, "cmd_delete_downtime_by_start_time_comment errors if no args sent");

	ok(cmd_delete_downtime_by_start_time_comment(1, "1234;Comment;") == OK, "cmd_delete_downtime_by_start_time_comment");
	ok(test_start_time == 1234, "Silly start time parsed but will get rejected lower down");
	ok(strcmp(test_comment, "Comment;") == 0, "Comment with ; will get dropped: '%s' <--- not sure if this should be true or not", test_comment);

	ok(cmd_delete_downtime_by_start_time_comment(1, "1234;Comment with \n in it;") == OK, "cmd_delete_downtime_by_start_time_comment");
	ok(strcmp(test_comment, "Comment with ") == 0, "Comment truncated at \\n") || diag("comment=%s", test_comment);

	ok(cmd_delete_downtime_by_start_time_comment(1, ";") == ERROR, "cmd_delete_downtime_by_start_time_comment error due to no args");

	test_start_time = 0L;
	test_comment = "somethingelse";
	ok(cmd_delete_downtime_by_start_time_comment(1, "badtime") == ERROR, "cmd_delete_downtime_by_start_time_comment error due to bad time value");

	ok(cmd_delete_downtime_by_start_time_comment(1, "123badtime;comment") == OK, "cmd_delete_downtime_by_start_time_comment");
	ok(test_start_time == 123L, "Partly bad start time is parsed, but that's an input problem");
	ok(strcmp(test_comment, "comment") == 0, "Comment");

	ok(cmd_delete_downtime_by_start_time_comment(1, ";commentonly") == OK, "cmd_delete_downtime_by_start_time_comment");
	ok(test_start_time == 0L, "No start time, but comment is set");
	ok(strcmp(test_comment, "commentonly") == 0, "Comment");

	deleted = 0;
	ok(cmd_delete_downtime_by_start_time_comment(1, ";commentonly") == ERROR, "Got an error here because no items deleted");


	/* Test deletion of downtimes by name */
	ok(cmd_delete_downtime_by_host_name(1, "") == ERROR, "cmd_delete_downtime_by_start_time_comment - errors if no args set");

	ok(cmd_delete_downtime_by_host_name(1, ";svc;1234567890;Some comment") == ERROR, "cmd_delete_downtime_by_start_time_comment - errors if no host name set");

	reset_vars();
	ok(cmd_delete_downtime_by_host_name(1, "host1") == ERROR, "cmd_delete_downtime_by_start_time_comment - errors if no host name set");
	ok(test_start_time == 0, "start time right");
	ok(strcmp(test_hostname, "host1") == 0, "hostname right");
	ok(test_servicename == NULL, "servicename right");
	ok(test_comment == NULL, "comment right");

	reset_vars();
	ok(cmd_delete_downtime_by_host_name(1, "host1;svc1;;") == ERROR, "cmd_delete_downtime_by_start_time_comment - errors if no host name set");
	ok(test_start_time == 0, "start time right");
	ok(strcmp(test_hostname, "host1") == 0, "hostname right");
	ok(strcmp(test_servicename, "svc1") == 0, "servicename right");
	ok(test_comment == NULL, "comment right") || diag("comment=%s", test_comment);

	reset_vars();
	ok(cmd_delete_downtime_by_host_name(1, "host1;svc1") == ERROR, "cmd_delete_downtime_by_start_time_comment - errors if no host name set");
	ok(test_start_time == 0, "start time right");
	ok(strcmp(test_hostname, "host1") == 0, "hostname right");
	ok(strcmp(test_servicename, "svc1") == 0, "servicename right");
	ok(test_comment == NULL, "comment right") || diag("comment=%s", test_comment);

	reset_vars();
	ok(cmd_delete_downtime_by_host_name(1, "host1;svc1;1234567;") == ERROR, "cmd_delete_downtime_by_start_time_comment - errors if no host name set");
	ok(test_start_time == 1234567L, "start time right");
	ok(strcmp(test_hostname, "host1") == 0, "hostname right");
	ok(strcmp(test_servicename, "svc1") == 0, "servicename right");
	ok(test_comment == NULL, "comment right") || diag("comment=%s", test_comment);

	reset_vars();
	ok(cmd_delete_downtime_by_host_name(1, "host1;svc1;12345678") == ERROR, "cmd_delete_downtime_by_start_time_comment - errors if no host name set");
	ok(test_start_time == 12345678L, "start time right");
	ok(strcmp(test_hostname, "host1") == 0, "hostname right");
	ok(strcmp(test_servicename, "svc1") == 0, "servicename right");
	ok(test_comment == NULL, "comment right") || diag("comment=%s", test_comment);

	reset_vars();
	ok(cmd_delete_downtime_by_host_name(1, "host1;;12345678;comment too") == ERROR, "cmd_delete_downtime_by_start_time_comment - errors if no host name set");
	ok(test_start_time == 12345678, "start time right");
	ok(strcmp(test_hostname, "host1") == 0, "hostname right");
	ok(test_servicename == NULL, "servicename right") || diag("servicename=%s", test_servicename);
	ok(strcmp(test_comment, "comment too") == 0, "comment right") || diag("comment=%s", test_comment);

	reset_vars();
	ok(cmd_delete_downtime_by_host_name(1, "host1;;12345678;") == ERROR, "cmd_delete_downtime_by_start_time_comment - errors if no host name set");
	ok(test_start_time == 12345678, "start time right");
	ok(strcmp(test_hostname, "host1") == 0, "hostname right");
	ok(test_servicename == NULL, "servicename right") || diag("servicename=%s", test_servicename);
	ok(test_comment == NULL, "comment right") || diag("comment=%s", test_comment);

	reset_vars();
	ok(cmd_delete_downtime_by_host_name(1, "host1;;;comment") == ERROR, "cmd_delete_downtime_by_start_time_comment - errors if no host name set");
	ok(test_start_time == 0L, "start time right");
	ok(strcmp(test_hostname, "host1") == 0, "hostname right");
	ok(test_servicename == NULL, "servicename right") || diag("servicename=%s", test_servicename);
	ok(strcmp(test_comment, "comment") == 0, "comment right") || diag("comment=%s", test_comment);



	return exit_status();
	}

