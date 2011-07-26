/*****************************************************************************
*
* test_strtoul.c - Test strtoul function for downtime
*
* Program: Nagios Core Testing
* License: GPL
* Copyright (c) 2009 Nagios Core Development Team and Community Contributors
* Copyright (c) 1999-2009 Ethan Galstad
*
* First Written:   12-Mov-2010
* Last Modified:   12-Nov-2010
*
* Description:
*
* Tests system strtoul - to ensure that it works as expected as some systems
* may differ in usage
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

#include <stdlib.h>
#include <string.h>
#include "tap.h"

char *svr_hostname = "hostname";
char *svr_fqdn = "hostname.domain.name";
char *svr_ip = "192.168.1.1";
char *svr_downtime_id = "1234";
char *end_ptr = NULL;
unsigned long downtime_id = 0L;

int main(int argc, char **argv) {

	plan_tests(8);

	downtime_id = strtoul(svr_hostname, &end_ptr, 10);
	ok(downtime_id == 0, "hostname downtime_id is 0");
	ok(strlen(end_ptr) == 8, "hostname end_ptr is 8 chars");

	downtime_id = strtoul(svr_fqdn, &end_ptr, 10);
	ok(downtime_id == 0, "fqdn downtime_id is 0");
	ok(strlen(end_ptr) == 20, "fqdn end_ptr is 20 chars");

	downtime_id = strtoul(svr_ip, &end_ptr, 10);
	ok(downtime_id == 192, "ip downtime_id is 192");
	ok(strlen(end_ptr) == 8, "ip end_ptr is 8 chars");

	downtime_id = strtoul(svr_downtime_id, &end_ptr, 10);
	ok(downtime_id == 1234, "svr_downtime_id downtime_id is 1234");
	ok(strlen(end_ptr) == 0, "svr_downtime_id end_ptr is 0 chars");

	return exit_status();
	}
