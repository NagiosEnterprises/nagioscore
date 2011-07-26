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
#include "nagios.h"
#include "objects.h"
#include "tap.h"
#include "../common/shared.c"

#define TEST_LOGGING 1
#include "test-stubs.c"

int date_format;

char *saved_source;
char *saved_dest;
int my_rename(char *source, char *dest) {
	char *temp = "renamefailure";
	saved_source = strdup(source);
	saved_dest = strdup(dest);
	if(strcmp(source, "renamefailure") == 0) {
		return ERROR;
		}
	return rename(source, dest);
	}

void update_program_status() {}

int
main(int argc, char **argv) {
	time_t rotation_time;
	struct stat stat_info, stat_new;
	char *log_filename_localtime = NULL;
	char *temp_command = NULL;
	struct tm *t;

	plan_tests(14);

	rotation_time = (time_t)1242949698;
	t = localtime(&rotation_time);

	asprintf(&log_filename_localtime, "var/nagios-%02d-%02d-%d-%02d.log", t->tm_mon + 1, t->tm_mday, t->tm_year + 1900, t->tm_hour);

	log_rotation_method = 5;
	ok(rotate_log_file(rotation_time) == ERROR, "Got error for a bad log_rotation_method");

	log_file = "renamefailure";
	log_rotation_method = LOG_ROTATION_HOURLY;
	ok(rotate_log_file(rotation_time) == ERROR, "Got an error with rename");
	ok(strcmp(saved_dest, log_filename_localtime) == 0, "Got an hourly rotation");

	log_file = "var/nagios.log";
	log_rotation_method = LOG_ROTATION_HOURLY;
	ok(system("cp var/nagios.log.dummy var/nagios.log") == 0, "Copied in dummy nagios.log for archiving");
	ok(rotate_log_file(rotation_time) == OK, "Log rotation should work happily");

	ok(system("diff var/nagios.log var/nagios.log.expected > /dev/null") == 0, "Got correct contents of nagios.log");

	asprintf(&temp_command, "diff var/nagios.log.dummy %s", log_filename_localtime);
	ok(system(temp_command) == 0, "nagios log archived correctly");

	unlink(log_filename_localtime);
	ok(system("chmod 777 var/nagios.log") == 0, "Changed mode of nagios.log");
	ok(stat("var/nagios.log", &stat_info) == 0, "Got stat info for log file");
	ok(rotate_log_file(rotation_time) == OK, "Log rotate to check if mode is retained");
	ok(stat(log_filename_localtime, &stat_new) == 0, "Got new stat info for archived log file");
	ok(stat_info.st_mode == stat_new.st_mode, "Mode for archived file same as original log file");

	ok(stat("var/nagios.log", &stat_new) == 0, "Got new stat info for new log file");
	ok(stat_info.st_mode == stat_new.st_mode, "Mode for new log file kept same as original log file");

	return exit_status();
	}
