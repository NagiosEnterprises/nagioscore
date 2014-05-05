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

#include <string.h>
#include <time.h>
#include "../include/objects.h"
#include "../include/nagios.h"
#include "tap.h"
#include "stub_downtime.c"
#include "stub_comments.c"

/*****************************************************************************/
/*                             Dummy functions                               */
/*****************************************************************************/
void logit(int data_type, int display, const char *fmt, ...) {
}
int my_sendall(int s, char *buf, int *len, int timeout) {
	return 0;
}

int log_debug_info(int level, int verbosity, const char *fmt, ...) {
	return 0;
}

int neb_free_callback_list(void) {
	return 0;
}

int neb_deinit_modules(void) {
	return 0;
}
void broker_program_state(int type, int flags, int attr,
		struct timeval *timestamp) {
}
int neb_unload_all_modules(int flags, int reason) {
	return 0;
}
int neb_add_module(char *filename, char *args, int should_be_loaded) {
	return 0;
}
void broker_system_command(int type, int flags, int attr,
		struct timeval start_time, struct timeval end_time, double exectime,
		int timeout, int early_timeout, int retcode, char *cmd, char *output,
		struct timeval *timestamp) {
}

timed_event *schedule_new_event(int event_type, int high_priority,
		time_t run_time, int recurring, unsigned long event_interval,
		void *timing_func, int compensate_for_time_change, void *event_data,
		void *event_args, int event_options) {
	return NULL ;
}
int my_tcp_connect(char *host_name, int port, int *sd, int timeout) {
	return 0;
}
int my_recvall(int s, char *buf, int *len, int timeout) {
	return 0;
}
int neb_free_module_list(void) {
	return 0;
}
int close_command_file(void) {
	return 0;
}
int close_log_file(void) {
	return 0;
}
int fix_log_file_owner(uid_t uid, gid_t gid) {
	return 0;
}
int handle_async_service_check_result(service *temp_service,
		check_result *queued_check_result) {
	return 0;
}
int handle_async_host_check_result(host *temp_host,
		check_result *queued_check_result) {
	return 0;
}

/*****************************************************************************/
/*                             Local test environment                        */
/*****************************************************************************/

host test_host = { .name = "name'&%", .address = "address'&%", .notes_url =
		"notes_url'&%($HOSTNOTES$)", .notes = "notes'&%($HOSTACTIONURL$)",
		.action_url = "action_url'&%", .plugin_output = "name'&%" };

/*****************************************************************************/
/*                             Helper functions                              */
/*****************************************************************************/

void init_environment() {
	char *p;

	my_free(illegal_output_chars);
	illegal_output_chars = strdup("'&"); /* For this tests, remove ' and & */

	/* This is a part of preflight check, which we can't run */
	for (p = illegal_output_chars; *p; p++) {
		illegal_output_char_map[(int) *p] = 1;
	}
}

nagios_macros *setup_macro_object(void) {
	nagios_macros *mac = (nagios_macros *) calloc(1, sizeof(nagios_macros));
	grab_host_macros_r(mac, &test_host);
	return mac;
}

#define RUN_MACRO_TEST(_STR, _EXPECT, _OPTS) \
	do { \
		if( OK == process_macros_r(mac, (_STR), &output, _OPTS ) ) {\
			ok( 0 == strcmp( output, _EXPECT ), "'%s': '%s' == '%s'", (_STR), output, (_EXPECT) ); \
		} else { \
			fail( "process_macros_r returns ERROR for " _STR ); \
		} \
	} while(0)

/*****************************************************************************/
/*                             Tests                                         */
/*****************************************************************************/

void test_escaping(nagios_macros *mac) {
	char *output;

	/* Nothing should be changed... options == 0 */
	RUN_MACRO_TEST( "$HOSTNAME$ '&%", "name'&% '&%", 0);

	/* Nothing should be changed... HOSTNAME doesn't accept STRIP_ILLEGAL_MACRO_CHARS */
	RUN_MACRO_TEST( "$HOSTNAME$ '&%", "name'&% '&%", STRIP_ILLEGAL_MACRO_CHARS);

	/* ' and & should be stripped from the macro, according to
	 * init_environment(), but not from the initial string
	 */
	RUN_MACRO_TEST( "$HOSTOUTPUT$ '&%", "name% '&%", STRIP_ILLEGAL_MACRO_CHARS);

	/* ESCAPE_MACRO_CHARS doesn't seem to do anything... exist always in pair
	 * with STRIP_ILLEGAL_MACRO_CHARS
	 */
	RUN_MACRO_TEST( "$HOSTOUTPUT$ '&%", "name'&% '&%", ESCAPE_MACRO_CHARS);
	RUN_MACRO_TEST( "$HOSTOUTPUT$ '&%", "name% '&%",
			STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);

	/* $HOSTNAME$ should be url-encoded, but not the tailing chars */
	RUN_MACRO_TEST( "$HOSTNAME$ '&%", "name%27%26%25 '&%",
			URL_ENCODE_MACRO_CHARS);

	/* The notes in the notesurl should be url-encoded, no more encoding should
	 * exist
	 */
	RUN_MACRO_TEST( "$HOSTNOTESURL$ '&%",
			"notes_url'&%(notes%27%26%25%28action_url%27%26%25%29) '&%", 0);

	/* '& in the source string shouldn't be removed, because HOSTNOTESURL
	 * doesn't accept STRIP_ILLEGAL_MACRO_CHARS, as in the url. the macros
	 * included in the string should be url-encoded, and therefore not contain &
	 * and '
	 */
	RUN_MACRO_TEST( "$HOSTNOTESURL$ '&%",
			"notes_url'&%(notes%27%26%25%28action_url%27%26%25%29) '&%",
			STRIP_ILLEGAL_MACRO_CHARS);

	/* This should double-encode some chars ($HOSTNOTESURL$ should contain
	 * url-encoded chars, and should itself be url-encoded
	 */
	RUN_MACRO_TEST( "$HOSTNOTESURL$ '&%",
			"notes_url%27%26%25%28notes%2527%2526%2525%2528action_url%2527%2526%2525%2529%29 '&%",
			URL_ENCODE_MACRO_CHARS);
}

/*****************************************************************************/
/*                             Main function                                 */
/*****************************************************************************/

int main(void) {
	nagios_macros *mac;

	plan_tests(9);

	reset_variables();
	init_environment();
	init_macros();

	mac = setup_macro_object();

	test_escaping(mac);

	cleanup();
	free(mac);
	return exit_status();
}
