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
#include "downtime.h"
#include "stub_broker.c"
#include "stub_comments.c"
#include "stub_statusdata.c"
#include "stub_notifications.c"
#include "stub_events.c"
#include "stub_logging.c"
#include "stub_nebmods.c"
#include "stub_netutils.c"
#include "stub_commands.c"
#include "stub_checks.c"
#include "tap.h"

timed_event *event_list_high = NULL;
timed_event *event_list_high_tail = NULL;

extern scheduled_downtime *scheduled_downtime_list;

int
main(int argc, char **argv) {
	time_t now = 0L;
	time_t temp_start_time = 1234567890L;
	time_t temp_end_time = 2134567890L;
	unsigned long downtime_id = 0L;
	scheduled_downtime *temp_downtime;
	int i = 0;
	char *main_config_file = "../t/etc/nagios-test-downtime.cfg";

	/* Initialize configuration variables */
	init_main_cfg_vars(1);
	init_shared_cfg_vars(1);

	/* Read the configuration */
	read_main_config_file(main_config_file);
	read_object_config_data(main_config_file, READ_ALL_OBJECT_DATA);
	pre_flight_check();
	initialize_downtime_data();

	plan_tests(38);

	time(&now);

	next_downtime_id = 1L;

	schedule_downtime(HOST_DOWNTIME, "host1", NULL, temp_start_time, "user", "test comment", temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 1L, "Got host1 downtime: %lu", downtime_id);
	schedule_downtime(HOST_DOWNTIME, "host2", NULL, temp_start_time, "user", "test comment", temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 2L, "Got host2 downtime: %lu", downtime_id);
	schedule_downtime(HOST_DOWNTIME, "host3", NULL, temp_start_time, "user", "diff comment", temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 3L, "Got host3 downtime: %lu", downtime_id);
	schedule_downtime(HOST_DOWNTIME, "host4", NULL, temp_start_time, "user", "test comment", temp_start_time + 1, temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 4L, "Got host4 downtime: %lu", downtime_id);

	schedule_downtime(SERVICE_DOWNTIME, "host1", "svc", temp_start_time, "user", "svc comment",  temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 5L, "Got host1::svc downtime: %lu", downtime_id);
	schedule_downtime(SERVICE_DOWNTIME, "host2", "svc", temp_start_time, "user", "diff comment", temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 6L, "Got host2::svc downtime: %lu", downtime_id);
	schedule_downtime(SERVICE_DOWNTIME, "host3", "svc", temp_start_time, "user", "svc comment",  temp_start_time + 1, temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 7L, "Got host3::svc downtime: %lu", downtime_id);
	schedule_downtime(SERVICE_DOWNTIME, "host4", "svc", temp_start_time, "user", "uniq comment", temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 8L, "Got host4::svc downtime: %lu", downtime_id);

	for(temp_downtime = scheduled_downtime_list, i = 0; temp_downtime != NULL; temp_downtime = temp_downtime->next, i++) {}
	ok(i == 8, "Got 8 downtimes: %d", i);

	i = delete_downtime_by_hostname_service_description_start_time_comment(NULL, NULL, 0, NULL);
	ok(i == 0, "No deletions") || diag("%d", i);

	i = delete_downtime_by_hostname_service_description_start_time_comment(NULL, NULL, 0, NULL);
	ok(i == 0, "No deletions");

	i = delete_downtime_by_hostname_service_description_start_time_comment(NULL, NULL, temp_start_time, "test comment");
	ok(i == 2, "Deleted 2 downtimes");

	for(temp_downtime = scheduled_downtime_list, i = 0; temp_downtime != NULL; temp_downtime = temp_downtime->next, i++) {}
	ok(i == 6, "Got 6 downtimes left: %d", i);

	i = delete_downtime_by_hostname_service_description_start_time_comment(NULL, NULL, temp_start_time + 200, "test comment");
	ok(i == 0, "Nothing matched, so 0 downtimes deleted");

	i = delete_downtime_by_hostname_service_description_start_time_comment(NULL, NULL, temp_start_time + 1, NULL);
	ok(i == 2, "Deleted 2 by start_time only: %d", i);

	i = delete_downtime_by_hostname_service_description_start_time_comment(NULL, NULL, 0, "uniq comment");
	ok(i == 1, "Deleted 1 by unique comment: %d", i);

	for(temp_downtime = scheduled_downtime_list, i = 0; temp_downtime != NULL; temp_downtime = temp_downtime->next, i++) {
		printf("# downtime id: %d\n", temp_downtime->downtime_id);
		}
	ok(i == 3, "Got 3 downtimes left: %d", i);

	unschedule_downtime(HOST_DOWNTIME, 3);
	unschedule_downtime(SERVICE_DOWNTIME, 5);
	unschedule_downtime(SERVICE_DOWNTIME, 6);

	for(temp_downtime = scheduled_downtime_list, i = 0; temp_downtime != NULL; temp_downtime = temp_downtime->next, i++) {}
	ok(i == 0, "No downtimes left");


	/* Set all downtimes up again */
	schedule_downtime(HOST_DOWNTIME, "host1", NULL, temp_start_time, "user", "test comment", temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 9L, "Got host1 downtime: %lu", downtime_id);
	schedule_downtime(HOST_DOWNTIME, "host2", NULL, temp_start_time, "user", "test comment", temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 10L, "Got host2 downtime: %lu", downtime_id);
	schedule_downtime(HOST_DOWNTIME, "host3", NULL, temp_start_time, "user", "diff comment", temp_start_time + 1,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 11L, "Got host3 downtime: %lu", downtime_id);
	schedule_downtime(HOST_DOWNTIME, "host4", NULL, temp_start_time, "user", "test comment", temp_start_time + 1, temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 12L, "Got host4 downtime: %lu", downtime_id);

	schedule_downtime(SERVICE_DOWNTIME, "host1", "svc", temp_start_time, "user", "svc comment",  temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 13L, "Got host1::svc downtime: %lu", downtime_id);
	schedule_downtime(SERVICE_DOWNTIME, "host2", "svc", temp_start_time, "user", "diff comment", temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 14L, "Got host2::svc downtime: %lu", downtime_id);
	schedule_downtime(SERVICE_DOWNTIME, "host3", "svc", temp_start_time, "user", "svc comment",  temp_start_time, temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 15L, "Got host3::svc downtime: %lu", downtime_id);
	schedule_downtime(SERVICE_DOWNTIME, "host4", "svc", temp_start_time, "user", "uniq comment", temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 16L, "Got host4::svc downtime: %lu", downtime_id);

	schedule_downtime(SERVICE_DOWNTIME, "host1", "svc2", temp_start_time, "user", "svc2 comment",  temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 17L, "Got host1::svc2 downtime: %lu", downtime_id);
	schedule_downtime(SERVICE_DOWNTIME, "host2", "svc2", temp_start_time, "user", "test comment", temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 18L, "Got host2::svc2 downtime: %lu", downtime_id);
	schedule_downtime(SERVICE_DOWNTIME, "host3", "svc2", temp_start_time, "user", "svc2 comment",  temp_start_time + 1, temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 19L, "Got host3::svc2 downtime: %lu", downtime_id);
	schedule_downtime(SERVICE_DOWNTIME, "host4", "svc2", temp_start_time, "user", "test comment", temp_start_time,  temp_end_time, 1, 0, 0, &downtime_id);
	ok(downtime_id == 20L, "Got host4::svc2 downtime: %lu", downtime_id);

	i = delete_downtime_by_hostname_service_description_start_time_comment("host2", NULL, 0, "test comment");
	ok(i == 2, "Deleted 2");

	i = delete_downtime_by_hostname_service_description_start_time_comment("host1", "svc", 0, NULL);
	ok(i == 1, "Deleted 1") || diag("Actually deleted: %d", i);

	i = delete_downtime_by_hostname_service_description_start_time_comment("host3", NULL, temp_start_time + 1, NULL);
	ok(i == 2, "Deleted 2");

	i = delete_downtime_by_hostname_service_description_start_time_comment(NULL, "svc2", 0, NULL);
	ok(i == 2, "Deleted 2") || diag("Actually deleted: %d", i);

	i = delete_downtime_by_hostname_service_description_start_time_comment("host4", NULL, 0, "test comment");
	ok(i == 1, "Deleted 1") || diag("Actually deleted: %d", i);

	i = delete_downtime_by_hostname_service_description_start_time_comment("host4", NULL, 0, "svc comment");
	ok(i == 0, "Deleted 0") || diag("Actually deleted: %d", i);

	for(temp_downtime = scheduled_downtime_list, i = 0; temp_downtime != NULL; temp_downtime = temp_downtime->next, i++) {
		printf("# downtime id: %d\n", temp_downtime->downtime_id);
		}
	ok(i == 4, "Got 4 downtimes left: %d", i);

	unschedule_downtime(HOST_DOWNTIME, 9);
	unschedule_downtime(SERVICE_DOWNTIME, 14);
	unschedule_downtime(SERVICE_DOWNTIME, 15);
	unschedule_downtime(SERVICE_DOWNTIME, 16);

	for(temp_downtime = scheduled_downtime_list, i = 0; temp_downtime != NULL; temp_downtime = temp_downtime->next, i++) {}
	ok(i == 0, "No downtimes left") || diag("Left: %d", i);



	return exit_status();
	}

