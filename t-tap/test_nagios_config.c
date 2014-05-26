/*****************************************************************************
 *
 * test_nagios_config.c - Test configuration loading
 *
 * Program: Nagios Core Testing
 * License: GPL
 *
 * First Written:   10-08-2009, based on nagios.c
 *
 * Description:
 *
 * Tests Nagios configuration loading
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

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/comments.h"
#include "../include/downtime.h"
#include "../include/statusdata.h"
#include "../include/macros.h"
#include "../include/nagios.h"
#include "../include/sretention.h"
#include "../include/perfdata.h"
#include "../include/broker.h"
#include "../include/nebmods.h"
#include "../include/nebmodules.h"
#include "tap.h"
#include "stub_events.c"
#include "stub_logging.c"
#include "stub_commands.c"
#include "stub_checks.c"
#include "stub_nebmods.c"
#include "stub_netutils.c"
#include "stub_broker.c"
#include "stub_statusdata.c"
#include "stub_flapping.c"
#include "stub_notifications.c"


int main(int argc, char **argv) {
	int result;
	int error = FALSE;
	char *buffer = NULL;
	int display_license = FALSE;
	int display_help = FALSE;
	int c = 0;
	struct tm *tm;
	time_t now;
	char datestring[256];
	host *temp_host = NULL;
	hostgroup *temp_hostgroup = NULL;
	hostsmember *temp_member = NULL;

	plan_tests(14);

	/* reset program variables */
	reset_variables();

	printf("Reading configuration data...\n");

	config_file = strdup("smallconfig/nagios.cfg");
	/* read in the configuration files (main config file, resource and object config files) */
	result = read_main_config_file(config_file);
	ok(result == OK, "Read main configuration file okay - if fails, use nagios -v to check");

	result = read_all_object_data(config_file);
	ok(result == OK, "Read all object config files");

	result = pre_flight_check();
	ok(result == OK, "Preflight check okay");

	initialize_downtime_data();

	for(temp_hostgroup = hostgroup_list; temp_hostgroup != NULL; temp_hostgroup = temp_hostgroup->next) {
		c++;
		//printf("Hostgroup=%s\n", temp_hostgroup->group_name);
		}
	ok(c == 2, "Found all hostgroups");

	temp_hostgroup = find_hostgroup("hostgroup1");
	for(temp_member = temp_hostgroup->members; temp_member != NULL; temp_member = temp_member->next) {
		//printf("host pointer=%d\n", temp_member->host_ptr);
		}

	temp_hostgroup = find_hostgroup("hostgroup2");
	for(temp_member = temp_hostgroup->members; temp_member != NULL; temp_member = temp_member->next) {
		//printf("host pointer=%d\n", temp_member->host_ptr);
		}

	temp_host = find_host("host1");
	ok(temp_host->current_state == 0, "State is assumed OK on initial load");

//	xrddefault_retention_file = strdup("smallconfig/retention.dat");
	ok(xrddefault_read_state_information() == OK, "Reading retention data");

	ok(temp_host->current_state == 1, "State changed due to retention file settings");

	ok(find_host_comment(418) != NULL, "Found host comment id 418");
	ok(find_service_comment(419) != NULL, "Found service comment id 419");
	ok(find_service_comment(420) == NULL, "Did not find service comment id 420 as not persistent");
	ok(find_host_comment(1234567888) == NULL, "No such host comment");

	ok(find_host_downtime(1102) != NULL, "Found host downtime id 1102");
	ok(find_service_downtime(1110) != NULL, "Found service downtime 1110");
	ok(find_host_downtime(1234567888) == NULL, "No such host downtime");

	cleanup();

	my_free(config_file);

	return exit_status();
	}


