/*****************************************************************************
 *
 * UTILS.C - Miscellaneous utility functions for Nagios
 *
 * Copyright (c) 1999-2009 Ethan Galstad (egalstad@nagios.org)
 * Last Modified: 06-16-2009
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
#include "../include/statusdata.h"
#include "../include/comments.h"
#include "../include/macros.h"
#include "../include/nagios.h"
#include "../include/netutils.h"
#include "../include/broker.h"
#include "../include/nebmods.h"
#include "../include/nebmodules.h"


#ifdef EMBEDDEDPERL
#include "../include/epn_nagios.h"
static PerlInterpreter *my_perl = NULL;
int use_embedded_perl = TRUE;
#endif

extern char	*config_file;
extern char	*log_file;
extern char     *command_file;
extern char     *temp_file;
extern char     *temp_path;
extern char     *check_result_path;
extern char     *check_result_path;
extern char     *lock_file;
extern char	*log_archive_path;
extern char     *auth_file;
extern char	*p1_file;

extern char     *nagios_user;
extern char     *nagios_group;

extern char     *macro_x_names[MACRO_X_COUNT];
extern char     *macro_user[MAX_USER_MACROS];
extern customvariablesmember *macro_custom_host_vars;
extern customvariablesmember *macro_custom_service_vars;
extern customvariablesmember *macro_custom_contact_vars;

extern host         *macro_host_ptr;
extern hostgroup    *macro_hostgroup_ptr;
extern service      *macro_service_ptr;
extern servicegroup *macro_servicegroup_ptr;
extern contact      *macro_contact_ptr;
extern contactgroup *macro_contactgroup_ptr;

extern char     *global_host_event_handler;
extern char     *global_service_event_handler;
extern command  *global_host_event_handler_ptr;
extern command  *global_service_event_handler_ptr;

extern char     *ocsp_command;
extern char     *ochp_command;
extern command  *ocsp_command_ptr;
extern command  *ochp_command_ptr;

extern char     *illegal_object_chars;
extern char     *illegal_output_chars;

extern int      use_regexp_matches;
extern int      use_true_regexp_matching;

extern int      sigshutdown;
extern int      sigrestart;
extern char     *sigs[35];
extern int      caught_signal;
extern int      sig_id;

extern int      daemon_mode;
extern int      daemon_dumps_core;

extern int      nagios_pid;

extern int	use_syslog;
extern int      log_notifications;
extern int      log_service_retries;
extern int      log_host_retries;
extern int      log_event_handlers;
extern int      log_external_commands;
extern int      log_passive_checks;

extern unsigned long      logging_options;
extern unsigned long      syslog_options;

extern int      service_check_timeout;
extern int      service_check_timeout_state;
extern int      host_check_timeout;
extern int      event_handler_timeout;
extern int      notification_timeout;
extern int      ocsp_timeout;
extern int      ochp_timeout;

extern int      log_initial_states;

extern double   sleep_time;
extern int      interval_length;
extern int      service_inter_check_delay_method;
extern int      host_inter_check_delay_method;
extern int      service_interleave_factor_method;
extern int      max_host_check_spread;
extern int      max_service_check_spread;

extern int      command_check_interval;
extern int      check_reaper_interval;
extern int      max_check_reaper_time;
extern int      service_freshness_check_interval;
extern int      host_freshness_check_interval;
extern int      auto_rescheduling_interval;
extern int      auto_rescheduling_window;

extern int      check_external_commands;
extern int      check_orphaned_services;
extern int      check_orphaned_hosts;
extern int      check_service_freshness;
extern int      check_host_freshness;
extern int      auto_reschedule_checks;

extern int      additional_freshness_latency;

extern int      check_for_updates;
extern int      bare_update_check;
extern time_t   last_update_check;
extern unsigned long update_uid;
extern char     *last_program_version;
extern int      update_available;
extern char     *last_program_version;
extern char     *new_program_version;

extern int      use_aggressive_host_checking;
extern unsigned long cached_host_check_horizon;
extern unsigned long cached_service_check_horizon;
extern int      enable_predictive_host_dependency_checks;
extern int      enable_predictive_service_dependency_checks;

extern int      soft_state_dependencies;

extern int      retain_state_information;
extern int      retention_update_interval;
extern int      use_retained_program_state;
extern int      use_retained_scheduling_info;
extern int      retention_scheduling_horizon;
extern unsigned long modified_host_process_attributes;
extern unsigned long modified_service_process_attributes;
extern unsigned long retained_host_attribute_mask;
extern unsigned long retained_service_attribute_mask;
extern unsigned long retained_contact_host_attribute_mask;
extern unsigned long retained_contact_service_attribute_mask;
extern unsigned long retained_process_host_attribute_mask;
extern unsigned long retained_process_service_attribute_mask;

extern unsigned long next_comment_id;
extern unsigned long next_downtime_id;
extern unsigned long next_event_id;
extern unsigned long next_notification_id;

extern int      log_rotation_method;

extern time_t   program_start;

extern time_t   last_command_check;
extern time_t	last_command_status_update;
extern time_t   last_log_rotation;

extern int      verify_config;
extern int      test_scheduling;

extern check_result check_result_info;

extern int      max_parallel_service_checks;
extern int      currently_running_service_checks;

extern int      enable_notifications;
extern int      execute_service_checks;
extern int      accept_passive_service_checks;
extern int      execute_host_checks;
extern int      accept_passive_host_checks;
extern int      enable_event_handlers;
extern int      obsess_over_services;
extern int      obsess_over_hosts;
extern int      enable_failure_prediction;
extern int      process_performance_data;

extern int      translate_passive_host_checks;
extern int      passive_host_checks_are_soft;

extern int      aggregate_status_updates;
extern int      status_update_interval;

extern int      time_change_threshold;

extern unsigned long event_broker_options;

extern int      process_performance_data;

extern int      enable_flap_detection;

extern double   low_service_flap_threshold;
extern double   high_service_flap_threshold;
extern double   low_host_flap_threshold;
extern double   high_host_flap_threshold;

extern int      use_large_installation_tweaks;
extern int      enable_environment_macros;
extern int      free_child_process_memory;
extern int      child_processes_fork_twice;

extern int      enable_embedded_perl;
extern int      use_embedded_perl_implicitly;

extern int      date_format;

extern contact		*contact_list;
extern contactgroup	*contactgroup_list;
extern host             *host_list;
extern hostgroup	*hostgroup_list;
extern service          *service_list;
extern servicegroup     *servicegroup_list;
extern timed_event      *event_list_high;
extern timed_event      *event_list_low;
extern notification     *notification_list;
extern command          *command_list;
extern timeperiod       *timeperiod_list;

extern int      command_file_fd;
extern FILE     *command_file_fp;
extern int      command_file_created;

#ifdef HAVE_TZNAME
#ifdef CYGWIN
extern char     *_tzname[2] __declspec(dllimport);
#else
extern char     *tzname[2];
#endif
#endif

extern check_result    *check_result_list;
extern unsigned long   max_check_result_file_age;

extern dbuf            check_result_dbuf;

extern pthread_t       worker_threads[TOTAL_WORKER_THREADS];
extern circular_buffer external_command_buffer;
extern circular_buffer check_result_buffer;
extern circular_buffer event_broker_buffer;
extern int             external_command_buffer_slots;

extern check_stats     check_statistics[MAX_CHECK_STATS_TYPES];

extern char            *debug_file;
extern int             debug_level;
extern int             debug_verbosity;
extern unsigned long   max_debug_file_size;

/* from GNU defines errno as a macro, since it's a per-thread variable */
#ifndef errno
extern int errno;
#endif



/******************************************************************/
/******************** SYSTEM COMMAND FUNCTIONS ********************/
/******************************************************************/


/* executes a system command - used for notifications, event handlers, etc. */
int my_system_r(nagios_macros *mac, char *cmd, int timeout, int *early_timeout, double *exectime, char **output, int max_output_length) {
	pid_t pid = 0;
	int status = 0;
	int result = 0;
	char buffer[MAX_INPUT_BUFFER] = "";
	char *temp_buffer = NULL;
	int fd[2];
	FILE *fp = NULL;
	int bytes_read = 0;
	struct timeval start_time, end_time;
	dbuf output_dbuf;
	int dbuf_chunk = 1024;
	int flags;
#ifdef EMBEDDEDPERL
	char fname[512] = "";
	char *args[5] = {"", DO_CLEAN, "", "", NULL };
	SV *plugin_hndlr_cr = NULL;
	char *perl_output = NULL;
	int count;
	int use_epn = FALSE;
#ifdef aTHX
	dTHX;
#endif
	dSP;
#endif


	log_debug_info(DEBUGL_FUNCTIONS, 0, "my_system_r()\n");

	/* initialize return variables */
	if(output != NULL)
		*output = NULL;
	*early_timeout = FALSE;
	*exectime = 0.0;

	/* if no command was passed, return with no error */
	if(cmd == NULL)
		return STATE_OK;

	log_debug_info(DEBUGL_COMMANDS, 1, "Running command '%s'...\n", cmd);

#ifdef EMBEDDEDPERL

	/* get"filename" component of command */
	strncpy(fname, cmd, strcspn(cmd, " "));
	fname[strcspn(cmd, " ")] = '\x0';

	/* should we use the embedded Perl interpreter to run this script? */
	use_epn = file_uses_embedded_perl(fname);

	/* if yes, do some initialization */
	if(use_epn == TRUE) {

		args[0] = fname;
		args[2] = "";

		if(strchr(cmd, ' ') == NULL)
			args[3] = "";
		else
			args[3] = cmd + strlen(fname) + 1;

		/* call our perl interpreter to compile and optionally cache the compiled script. */

		ENTER;
		SAVETMPS;
		PUSHMARK(SP);

		XPUSHs(sv_2mortal(newSVpv(args[0], 0)));
		XPUSHs(sv_2mortal(newSVpv(args[1], 0)));
		XPUSHs(sv_2mortal(newSVpv(args[2], 0)));
		XPUSHs(sv_2mortal(newSVpv(args[3], 0)));

		PUTBACK;

		call_pv("Embed::Persistent::eval_file", G_EVAL);

		SPAGAIN;

		if(SvTRUE(ERRSV)) {
			/*
			 * XXXX need pipe open to send the compilation failure message back to Nagios ?
			 */
			(void) POPs ;

			asprintf(&temp_buffer, "%s", SvPVX(ERRSV));

			log_debug_info(DEBUGL_COMMANDS, 0, "Embedded perl failed to compile %s, compile error %s\n", fname, temp_buffer);

			logit(NSLOG_RUNTIME_WARNING, TRUE, "%s\n", temp_buffer);

			my_free(temp_buffer);

			return STATE_UNKNOWN;
			}
		else {
			plugin_hndlr_cr = newSVsv(POPs);

			log_debug_info(DEBUGL_COMMANDS, 0, "Embedded perl successfully compiled %s and returned plugin handler (Perl subroutine code ref)\n", fname);

			PUTBACK ;
			FREETMPS ;
			LEAVE ;
			}
		}
#endif

	/* create a pipe */
	pipe(fd);

	/* make the pipe non-blocking */
	fcntl(fd[0], F_SETFL, O_NONBLOCK);
	fcntl(fd[1], F_SETFL, O_NONBLOCK);

	/* get the command start time */
	gettimeofday(&start_time, NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec = 0L;
	end_time.tv_usec = 0L;
	broker_system_command(NEBTYPE_SYSTEM_COMMAND_START, NEBFLAG_NONE, NEBATTR_NONE, start_time, end_time, *exectime, timeout, *early_timeout, result, cmd, NULL, NULL);
#endif

	/* fork */
	pid = fork();

	/* return an error if we couldn't fork */
	if(pid == -1) {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: fork() in my_system_r() failed for command \"%s\"\n", cmd);

		/* close both ends of the pipe */
		close(fd[0]);
		close(fd[1]);

		return STATE_UNKNOWN;
		}

	/* execute the command in the child process */
	if(pid == 0) {

		/* become process group leader */
		setpgid(0, 0);

		/* set environment variables */
		set_all_macro_environment_vars_r(mac, TRUE);

		/* ADDED 11/12/07 EG */
		/* close external command file and shut down worker thread */
		close_command_file();

		/* reset signal handling */
		reset_sighandler();

		/* close pipe for reading */
		close(fd[0]);

		/* prevent fd from being inherited by child processed */
		flags = fcntl(fd[1], F_GETFD, 0);
		flags |= FD_CLOEXEC;
		fcntl(fd[1], F_SETFD, flags);

		/* trap commands that timeout */
		signal(SIGALRM, my_system_sighandler);
		alarm(timeout);


		/******** BEGIN EMBEDDED PERL CODE EXECUTION ********/

#ifdef EMBEDDEDPERL
		if(use_epn == TRUE) {

			/* execute our previously compiled script - by call_pv("Embed::Persistent::eval_file",..) */
			ENTER;
			SAVETMPS;
			PUSHMARK(SP);

			XPUSHs(sv_2mortal(newSVpv(args[0], 0)));
			XPUSHs(sv_2mortal(newSVpv(args[1], 0)));
			XPUSHs(plugin_hndlr_cr);
			XPUSHs(sv_2mortal(newSVpv(args[3], 0)));

			PUTBACK;

			count = call_pv("Embed::Persistent::run_package", G_ARRAY);
			/* count is a debug hook. It should always be two (2), because the persistence framework tries to return two (2) args */

			SPAGAIN;

			perl_output = POPpx ;
			strip(perl_output);
			strncpy(buffer, (perl_output == NULL) ? "" : perl_output, sizeof(buffer));
			buffer[sizeof(buffer) - 1] = '\x0';
			status = POPi ;

			PUTBACK;
			FREETMPS;
			LEAVE;

			log_debug_info(DEBUGL_COMMANDS, 0, "Embedded perl ran command %s with output %d, %s\n", fname, status, buffer);

			/* write the output back to the parent process */
			write(fd[1], buffer, strlen(buffer) + 1);

			/* close pipe for writing */
			close(fd[1]);

			/* reset the alarm */
			alarm(0);

			_exit(status);
			}
#endif
		/******** END EMBEDDED PERL CODE EXECUTION ********/


		/* run the command */
		fp = (FILE *)popen(cmd, "r");

		/* report an error if we couldn't run the command */
		if(fp == NULL) {

			strncpy(buffer, "(Error: Could not execute command)\n", sizeof(buffer) - 1);
			buffer[sizeof(buffer) - 1] = '\x0';

			/* write the error back to the parent process */
			write(fd[1], buffer, strlen(buffer) + 1);

			result = STATE_CRITICAL;
			}
		else {

			/* write all the lines of output back to the parent process */
			while(fgets(buffer, sizeof(buffer) - 1, fp))
				write(fd[1], buffer, strlen(buffer));

			/* close the command and get termination status */
			status = pclose(fp);

			/* report an error if we couldn't close the command */
			if(status == -1)
				result = STATE_CRITICAL;
			else {
				if(WEXITSTATUS(status) == 0 && WIFSIGNALED(status))
					result = 128 + WTERMSIG(status);
				result = WEXITSTATUS(status);
				}
			}

		/* close pipe for writing */
		close(fd[1]);

		/* reset the alarm */
		alarm(0);

		/* clear environment variables */
		set_all_macro_environment_vars_r(mac, FALSE);

#ifndef DONT_USE_MEMORY_PERFORMANCE_TWEAKS
		/* free allocated memory */
		/* this needs to be done last, so we don't free memory for variables before they're used above */
		if(free_child_process_memory == TRUE)
			free_memory(mac);
#endif

		_exit(result);
		}

	/* parent waits for child to finish executing command */
	else {

		/* close pipe for writing */
		close(fd[1]);

		/* wait for child to exit */
		waitpid(pid, &status, 0);

		/* get the end time for running the command */
		gettimeofday(&end_time, NULL);

		/* return execution time in milliseconds */
		*exectime = (double)((double)(end_time.tv_sec - start_time.tv_sec) + (double)((end_time.tv_usec - start_time.tv_usec) / 1000) / 1000.0);
		if(*exectime < 0.0)
			*exectime = 0.0;

		/* get the exit code returned from the program */
		result = WEXITSTATUS(status);

		/* check for possibly missing scripts/binaries/etc */
		if(result == 126 || result == 127) {
			temp_buffer = "\163\157\151\147\141\156\040\145\144\151\163\156\151";
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Attempting to execute the command \"%s\" resulted in a return code of %d.  Make sure the script or binary you are trying to execute actually exists...\n", cmd, result);
			}

		/* check bounds on the return value */
		if(result < -1 || result > 3)
			result = STATE_UNKNOWN;

		/* initialize dynamic buffer */
		dbuf_init(&output_dbuf, dbuf_chunk);

		/* Opsera patch to check timeout before attempting to read output via pipe. Originally by Sven Nierlein */
		/* if there was a critical return code AND the command time exceeded the timeout thresholds, assume a timeout */
		if(result == STATE_CRITICAL && (end_time.tv_sec - start_time.tv_sec) >= timeout) {

			/* set the early timeout flag */
			*early_timeout = TRUE;

			/* try to kill the command that timed out by sending termination signal to child process group */
			kill((pid_t)(-pid), SIGTERM);
			sleep(1);
			kill((pid_t)(-pid), SIGKILL);
			}

		/* read output if timeout has not occurred */
		else {

			/* initialize output */
			strcpy(buffer, "");

			/* try and read the results from the command output (retry if we encountered a signal) */
			do {
				bytes_read = read(fd[0], buffer, sizeof(buffer) - 1);

				/* append data we just read to dynamic buffer */
				if(bytes_read > 0) {
					buffer[bytes_read] = '\x0';
					dbuf_strcat(&output_dbuf, buffer);
					}

				/* handle errors */
				if(bytes_read == -1) {
					/* we encountered a recoverable error, so try again */
					if(errno == EINTR)
						continue;
					/* patch by Henning Brauer to prevent CPU hogging */
					else if(errno == EAGAIN) {
						struct pollfd pfd;

						pfd.fd = fd[0];
						pfd.events = POLLIN;
						poll(&pfd, 1, -1);
						continue;
						}
					else
						break;
					}

				/* we're done */
				if(bytes_read == 0)
					break;

				}
			while(1);

			/* cap output length - this isn't necessary, but it keeps runaway plugin output from causing problems */
			if(max_output_length > 0  && output_dbuf.used_size > max_output_length)
				output_dbuf.buf[max_output_length] = '\x0';

			if(output != NULL && output_dbuf.buf)
				*output = (char *)strdup(output_dbuf.buf);

			}

		log_debug_info(DEBUGL_COMMANDS, 1, "Execution time=%.3f sec, early timeout=%d, result=%d, output=%s\n", *exectime, *early_timeout, result, (output_dbuf.buf == NULL) ? "(null)" : output_dbuf.buf);

#ifdef USE_EVENT_BROKER
		/* send data to event broker */
		broker_system_command(NEBTYPE_SYSTEM_COMMAND_END, NEBFLAG_NONE, NEBATTR_NONE, start_time, end_time, *exectime, timeout, *early_timeout, result, cmd, (output_dbuf.buf == NULL) ? NULL : output_dbuf.buf, NULL);
#endif

		/* free memory */
		dbuf_free(&output_dbuf);

		/* close the pipe for reading */
		close(fd[0]);
		}

	return result;
	}

/*
 * For API compatibility, we must include a my_system() whose
 * signature doesn't include the nagios_macros variable.
 * NDOUtils uses this. Possibly other modules as well.
 */
int my_system(char *cmd, int timeout, int *early_timeout, double *exectime, char **output, int max_output_length) {
	return my_system_r(get_global_macros(), cmd, timeout, early_timeout, exectime, output, max_output_length);
	}


/* given a "raw" command, return the "expanded" or "whole" command line */
int get_raw_command_line_r(nagios_macros *mac, command *cmd_ptr, char *cmd, char **full_command, int macro_options) {
	char temp_arg[MAX_COMMAND_BUFFER] = "";
	char *arg_buffer = NULL;
	register int x = 0;
	register int y = 0;
	register int arg_index = 0;
	register int escaped = FALSE;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "get_raw_command_line_r()\n");

	/* clear the argv macros */
	clear_argv_macros_r(mac);

	/* make sure we've got all the requirements */
	if(cmd_ptr == NULL || full_command == NULL)
		return ERROR;

	log_debug_info(DEBUGL_COMMANDS | DEBUGL_CHECKS | DEBUGL_MACROS, 2, "Raw Command Input: %s\n", cmd_ptr->command_line);

	/* get the full command line */
	*full_command = (char *)strdup((cmd_ptr->command_line == NULL) ? "" : cmd_ptr->command_line);

	/* XXX: Crazy indent */
	/* get the command arguments */
	if(cmd != NULL) {

		/* skip the command name (we're about to get the arguments)... */
		for(arg_index = 0;; arg_index++) {
			if(cmd[arg_index] == '!' || cmd[arg_index] == '\x0')
				break;
			}

		/* get each command argument */
		for(x = 0; x < MAX_COMMAND_ARGUMENTS; x++) {

			/* we reached the end of the arguments... */
			if(cmd[arg_index] == '\x0')
				break;

			/* get the next argument */
			/* can't use strtok(), as that's used in process_macros... */
			for(arg_index++, y = 0; y < sizeof(temp_arg) - 1; arg_index++) {

				/* backslashes escape */
				if(cmd[arg_index] == '\\' && escaped == FALSE) {
					escaped = TRUE;
					continue;
					}

				/* end of argument */
				if((cmd[arg_index] == '!' && escaped == FALSE) || cmd[arg_index] == '\x0')
					break;

				/* normal of escaped char */
				temp_arg[y] = cmd[arg_index];
				y++;

				/* clear escaped flag */
				escaped = FALSE;
				}
			temp_arg[y] = '\x0';

			/* ADDED 01/29/04 EG */
			/* process any macros we find in the argument */
			process_macros_r(mac, temp_arg, &arg_buffer, macro_options);

			mac->argv[x] = arg_buffer;
			}
		}

	log_debug_info(DEBUGL_COMMANDS | DEBUGL_CHECKS | DEBUGL_MACROS, 2, "Expanded Command Output: %s\n", *full_command);

	return OK;
	}

/*
 * This function modifies the global macro struct and is thus not
 * threadsafe
 */
int get_raw_command_line(command *cmd_ptr, char *cmd, char **full_command, int macro_options) {
	nagios_macros *mac;

	mac = get_global_macros();
	return get_raw_command_line_r(mac, cmd_ptr, cmd, full_command, macro_options);
	}



/******************************************************************/
/******************** ENVIRONMENT FUNCTIONS ***********************/
/******************************************************************/

/* sets or unsets an environment variable */
int set_environment_var(char *name, char *value, int set) {
#ifndef HAVE_SETENV
	char *env_string = NULL;
#endif

	/* we won't mess with null variable names */
	if(name == NULL)
		return ERROR;

	/* set the environment variable */
	if(set == TRUE) {

#ifdef HAVE_SETENV
		setenv(name, (value == NULL) ? "" : value, 1);
#else
		/* needed for Solaris and systems that don't have setenv() */
		/* this will leak memory, but in a "controlled" way, since lost memory should be freed when the child process exits */
		asprintf(&env_string, "%s=%s", name, (value == NULL) ? "" : value);
		if(env_string)
			putenv(env_string);
#endif
		}
	/* clear the variable */
	else {
#ifdef HAVE_UNSETENV
		unsetenv(name);
#endif
		}

	return OK;
	}




/******************************************************************/
/************************* TIME FUNCTIONS *************************/
/******************************************************************/


/* Checks if the given time is in daylight time saving period */
int is_dst_time(time_t *time) {
	struct tm *bt = localtime(time);
	return bt->tm_isdst;
	}

/* Returns the shift in seconds if the given times are across the daylight time saving period change */
int get_dst_shift(time_t *start, time_t *end) {
	int shift = 0, dst_end, dst_start;
	dst_start = is_dst_time(start);
	dst_end = is_dst_time(end);
	if(dst_start < dst_end) {
		shift = 3600;
		}
	else if(dst_start > dst_end) {
		shift = -3600;
		}
	return shift;
	}

/*#define TEST_TIMEPERIODS_A 1*/

/* see if the specified time falls into a valid time range in the given time period */
int check_time_against_period(time_t test_time, timeperiod *tperiod) {
	timeperiodexclusion *temp_timeperiodexclusion = NULL;
	timeperiodexclusion *first_timeperiodexclusion = NULL;
	daterange *temp_daterange = NULL;
	timerange *temp_timerange = NULL;
	time_t midnight = (time_t)0L;
	time_t start_time = (time_t)0L;
	time_t end_time = (time_t)0L;
	int found_match = FALSE;
	struct tm *t, tm_s;
	int daterange_type = 0;
	unsigned long days = 0L;
	time_t day_range_start = (time_t)0L;
	time_t day_range_end = (time_t)0L;
	int test_time_year = 0;
	int test_time_mon = 0;
	int test_time_mday = 0;
	int test_time_wday = 0;
	int year = 0;
	int shift;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "check_time_against_period()\n");

	/* if no period was specified, assume the time is good */
	if(tperiod == NULL)
		return OK;

	/* test exclusions first - if exclusions match current time, bail out with an error */
	/* clear exclusions list before recursing (and restore afterwards) to prevent endless loops... */
	first_timeperiodexclusion = tperiod->exclusions;
	tperiod->exclusions = NULL;
	for(temp_timeperiodexclusion = first_timeperiodexclusion; temp_timeperiodexclusion != NULL; temp_timeperiodexclusion = temp_timeperiodexclusion->next) {
		if(check_time_against_period(test_time, temp_timeperiodexclusion->timeperiod_ptr) == OK) {
			tperiod->exclusions = first_timeperiodexclusion;
			return ERROR;
			}
		}
	tperiod->exclusions = first_timeperiodexclusion;

	/* save values for later */
	t = localtime_r((time_t *)&test_time, &tm_s);
	test_time_year = t->tm_year;
	test_time_mon = t->tm_mon;
	test_time_mday = t->tm_mday;
	test_time_wday = t->tm_wday;

	/* calculate the start of the day (midnight, 00:00 hours) when the specified test time occurs */
	t->tm_sec = 0;
	t->tm_min = 0;
	t->tm_hour = 0;
	midnight = mktime(t);

	/**** check exceptions first ****/
	for(daterange_type = 0; daterange_type < DATERANGE_TYPES; daterange_type++) {

		for(temp_daterange = tperiod->exceptions[daterange_type]; temp_daterange != NULL; temp_daterange = temp_daterange->next) {

#ifdef TEST_TIMEPERIODS_A
			printf("TYPE: %d\n", daterange_type);
			printf("TEST:     %lu = %s", (unsigned long)test_time, ctime(&test_time));
			printf("MIDNIGHT: %lu = %s", (unsigned long)midnight, ctime(&midnight));
#endif

			/* get the start time */
			switch(daterange_type) {
				case DATERANGE_CALENDAR_DATE:
					t->tm_sec = 0;
					t->tm_min = 0;
					t->tm_hour = 0;
					t->tm_wday = 0;
					t->tm_mday = temp_daterange->smday;
					t->tm_mon = temp_daterange->smon;
					t->tm_year = (temp_daterange->syear - 1900);
					t->tm_isdst = -1;
					start_time = mktime(t);
					break;
				case DATERANGE_MONTH_DATE:
					start_time = calculate_time_from_day_of_month(test_time_year, temp_daterange->smon, temp_daterange->smday);
					break;
				case DATERANGE_MONTH_DAY:
					start_time = calculate_time_from_day_of_month(test_time_year, test_time_mon, temp_daterange->smday);
					break;
				case DATERANGE_MONTH_WEEK_DAY:
					start_time = calculate_time_from_weekday_of_month(test_time_year, temp_daterange->smon, temp_daterange->swday, temp_daterange->swday_offset);
					break;
				case DATERANGE_WEEK_DAY:
					start_time = calculate_time_from_weekday_of_month(test_time_year, test_time_mon, temp_daterange->swday, temp_daterange->swday_offset);
					break;
				default:
					continue;
					break;
				}

			/* get the end time */
			switch(daterange_type) {
				case DATERANGE_CALENDAR_DATE:
					t->tm_sec = 0;
					t->tm_min = 0;
					t->tm_hour = 0;
					t->tm_wday = 0;
					t->tm_mday = temp_daterange->emday;
					t->tm_mon = temp_daterange->emon;
					t->tm_year = (temp_daterange->eyear - 1900);
					t->tm_isdst = -1;
					end_time = mktime(t);
					break;
				case DATERANGE_MONTH_DATE:
					year = test_time_year;
					end_time = calculate_time_from_day_of_month(year, temp_daterange->emon, temp_daterange->emday);
					/* advance a year if necessary: august 2 - february 5 */
					if(end_time < start_time) {
						year++;
						end_time = calculate_time_from_day_of_month(year, temp_daterange->emon, temp_daterange->emday);
						}
					break;
				case DATERANGE_MONTH_DAY:
					end_time = calculate_time_from_day_of_month(test_time_year, test_time_mon, temp_daterange->emday);
					break;
				case DATERANGE_MONTH_WEEK_DAY:
					year = test_time_year;
					end_time = calculate_time_from_weekday_of_month(year, temp_daterange->emon, temp_daterange->ewday, temp_daterange->ewday_offset);
					/* advance a year if necessary: thursday 2 august - monday 3 february */
					if(end_time < start_time) {
						year++;
						end_time = calculate_time_from_weekday_of_month(year, temp_daterange->emon, temp_daterange->ewday, temp_daterange->ewday_offset);
						}
					break;
				case DATERANGE_WEEK_DAY:
					end_time = calculate_time_from_weekday_of_month(test_time_year, test_time_mon, temp_daterange->ewday, temp_daterange->ewday_offset);
					break;
				default:
					continue;
					break;
				}

#ifdef TEST_TIMEPERIODS_A
			printf("START:    %lu = %s", (unsigned long)start_time, ctime(&start_time));
			printf("END:      %lu = %s", (unsigned long)end_time, ctime(&end_time));
#endif

			/* start date was bad, so skip this date range */
			if((unsigned long)start_time == 0L)
				continue;

			/* end date was bad - see if we can handle the error */
			if((unsigned long)end_time == 0L) {
				switch(daterange_type) {
					case DATERANGE_CALENDAR_DATE:
						continue;
						break;
					case DATERANGE_MONTH_DATE:
						/* end date can't be helped, so skip it */
						if(temp_daterange->emday < 0)
							continue;

						/* else end date slipped past end of month, so use last day of month as end date */
						/* use same year calculated above */
						end_time = calculate_time_from_day_of_month(year, temp_daterange->emon, -1);
						break;
					case DATERANGE_MONTH_DAY:
						/* end date can't be helped, so skip it */
						if(temp_daterange->emday < 0)
							continue;

						/* else end date slipped past end of month, so use last day of month as end date */
						end_time = calculate_time_from_day_of_month(test_time_year, test_time_mon, -1);
						break;
					case DATERANGE_MONTH_WEEK_DAY:
						/* end date can't be helped, so skip it */
						if(temp_daterange->ewday_offset < 0)
							continue;

						/* else end date slipped past end of month, so use last day of month as end date */
						/* use same year calculated above */
						end_time = calculate_time_from_day_of_month(year, test_time_mon, -1);
						break;
					case DATERANGE_WEEK_DAY:
						/* end date can't be helped, so skip it */
						if(temp_daterange->ewday_offset < 0)
							continue;

						/* else end date slipped past end of month, so use last day of month as end date */
						end_time = calculate_time_from_day_of_month(test_time_year, test_time_mon, -1);
						break;
					default:
						continue;
						break;
					}
				}

			/* calculate skip date start (and end) */
			if(temp_daterange->skip_interval > 1) {

				/* skip start date must be before test time */
				if(start_time > test_time)
					continue;

				/* check if interval is across dlst change and gets the compensation */
				shift = get_dst_shift(&start_time, &midnight);

				/* how many days have passed between skip start date and test time? */
				days = (shift + (unsigned long)midnight - (unsigned long)start_time) / (3600 * 24);

				/* if test date doesn't fall on a skip interval day, bail out early */
				if((days % temp_daterange->skip_interval) != 0)
					continue;

				/* use midnight of test date as start time */
				else
					start_time = midnight;

				/* if skipping range has no end, use test date as end */
				if((daterange_type == DATERANGE_CALENDAR_DATE) && (is_daterange_single_day(temp_daterange) == TRUE))
					end_time = midnight;
				}

#ifdef TEST_TIMEPERIODS_A
			printf("NEW START:    %lu = %s", (unsigned long)start_time, ctime(&start_time));
			printf("NEW END:      %lu = %s", (unsigned long)end_time, ctime(&end_time));
			printf("%d DAYS PASSED\n", days);
			printf("DLST SHIFT:   %d", shift);
#endif

			/* time falls into the range of days */
			if(midnight >= start_time && midnight <= end_time)
				found_match = TRUE;

			/* found a day match, so see if time ranges are good */
			if(found_match == TRUE) {

				for(temp_timerange = temp_daterange->times; temp_timerange != NULL; temp_timerange = temp_timerange->next) {

					/* ranges with start/end of zero mean exlude this day */
					if(temp_timerange->range_start == 0 && temp_timerange->range_end == 0) {
#ifdef TEST_TIMEPERIODS_A
						printf("0 MINUTE RANGE EXCLUSION\n");
#endif
						continue;
						}

					day_range_start = (time_t)(midnight + temp_timerange->range_start);
					day_range_end = (time_t)(midnight + temp_timerange->range_end);

#ifdef TEST_TIMEPERIODS_A
					printf("  RANGE START: %lu (%lu) = %s", temp_timerange->range_start, (unsigned long)day_range_start, ctime(&day_range_start));
					printf("  RANGE END:   %lu (%lu) = %s", temp_timerange->range_end, (unsigned long)day_range_end, ctime(&day_range_end));
#endif

					/* if the user-specified time falls in this range, return with a positive result */
					if(test_time >= day_range_start && test_time <= day_range_end)
						return OK;
					}

				/* no match, so bail with error */
				return ERROR;
				}
			}
		}


	/**** check normal, weekly rotating schedule last ****/

	/* check weekday time ranges */
	for(temp_timerange = tperiod->days[test_time_wday]; temp_timerange != NULL; temp_timerange = temp_timerange->next) {

		day_range_start = (time_t)(midnight + temp_timerange->range_start);
		day_range_end = (time_t)(midnight + temp_timerange->range_end);

		/* if the user-specified time falls in this range, return with a positive result */
		if(test_time >= day_range_start && test_time <= day_range_end)
			return OK;
		}

	return ERROR;
	}



/*#define TEST_TIMEPERIODS_B 1*/

/* Separate this out from public get_next_valid_time for testing, so we can mock current_time */
void _get_next_valid_time(time_t pref_time, time_t current_time, time_t *valid_time, timeperiod *tperiod) {
	time_t preferred_time = (time_t)0L;
	timerange *temp_timerange;
	daterange *temp_daterange;
	time_t midnight = (time_t)0L;
	struct tm *t, tm_s;
	time_t day_start = (time_t)0L;
	time_t day_range_start = (time_t)0L;
	time_t day_range_end = (time_t)0L;
	time_t start_time = (time_t)0L;
	time_t end_time = (time_t)0L;
	int have_earliest_time = FALSE;
	time_t earliest_time = (time_t)0L;
	time_t earliest_day = (time_t)0L;
	time_t potential_time = (time_t)0L;
	int weekday = 0;
	int has_looped = FALSE;
	int days_into_the_future = 0;
	int daterange_type = 0;
	unsigned long days = 0L;
	unsigned long advance_interval = 0L;
	int year = 0; /* new */
	int month = 0; /* new */

	int pref_time_year = 0;
	int pref_time_mon = 0;
	int pref_time_mday = 0;
	int pref_time_wday = 0;
	int current_time_year = 0;
	int current_time_mon = 0;
	int current_time_mday = 0;
	int current_time_wday = 0;
	int shift;

	/* preferred time must be now or in the future */
	preferred_time = (pref_time < current_time) ? current_time : pref_time;

	/* if no timeperiod, go with preferred time */
	if(tperiod == NULL) {
		*valid_time = preferred_time;
		return;
		}

	/* if the preferred time is valid in timeperiod, go with it */
	/* this is necessary because the code below won't catch exceptions where preferred day is last (or only) date in timeperiod (date range) and last valid time has already passed */
	/* performing this check and bailing out early allows us to skip having to check the next instance of a date range exception or weekday to determine the next valid time */
	if(check_time_against_period(preferred_time, tperiod) == OK) {
#ifdef TEST_TIMEPERIODS_B
		printf("PREF TIME IS VALID\n");
#endif
		*valid_time = preferred_time;
		return;
		}

	/* calculate the start of the day (midnight, 00:00 hours) of preferred time */
	t = localtime_r(&preferred_time, &tm_s);
	t->tm_sec = 0;
	t->tm_min = 0;
	t->tm_hour = 0;
	midnight = mktime(t);

	/* save pref time values for later */
	pref_time_year = t->tm_year;
	pref_time_mon = t->tm_mon;
	pref_time_mday = t->tm_mday;
	pref_time_wday = t->tm_wday;

	/* save current time values for later */
	t = localtime_r(&current_time, &tm_s);
	current_time_year = t->tm_year;
	current_time_mon = t->tm_mon;
	current_time_mday = t->tm_mday;
	current_time_wday = t->tm_wday;

#ifdef TEST_TIMEPERIODS_B
	printf("PREF TIME:    %lu = %s", (unsigned long)preferred_time, ctime(&preferred_time));
	printf("CURRENT TIME: %lu = %s", (unsigned long)current_time, ctime(&current_time));
	printf("PREF YEAR:    %d, MON: %d, MDAY: %d, WDAY: %d\n", pref_time_year, pref_time_mon, pref_time_mday, pref_time_wday);
	printf("CURRENT YEAR: %d, MON: %d, MDAY: %d, WDAY: %d\n", current_time_year, current_time_mon, current_time_mday, current_time_wday);
#endif

	/**** check exceptions (in this timeperiod definition) first ****/
	for(daterange_type = 0; daterange_type < DATERANGE_TYPES; daterange_type++) {

#ifdef TEST_TIMEPERIODS_B
		printf("TYPE: %d\n", daterange_type);
#endif

		for(temp_daterange = tperiod->exceptions[daterange_type]; temp_daterange != NULL; temp_daterange = temp_daterange->next) {

			/* get the start time */
			switch(daterange_type) {
				case DATERANGE_CALENDAR_DATE: /* 2009-08-11 */
					t->tm_sec = 0;
					t->tm_min = 0;
					t->tm_hour = 0;
					t->tm_mday = temp_daterange->smday;
					t->tm_mon = temp_daterange->smon;
					t->tm_year = (temp_daterange->syear - 1900);
					t->tm_isdst = -1;
					start_time = mktime(t);
					break;
				case DATERANGE_MONTH_DATE:  /* january 1 */
					/* what year should we use? */
					year = (pref_time_year < current_time_year) ? current_time_year : pref_time_year;
					/* advance an additional year if we already passed the end month date */
					if((temp_daterange->emon < current_time_mon) || ((temp_daterange->emon == current_time_mon) && temp_daterange->emday < current_time_mday))
						year++;
					start_time = calculate_time_from_day_of_month(year, temp_daterange->smon, temp_daterange->smday);
					break;
				case DATERANGE_MONTH_DAY:  /* day 3 */
					/* what year should we use? */
					year = (pref_time_year < current_time_year) ? current_time_year : pref_time_year;
					/* use current month */
					month = current_time_mon;
					/* advance an additional month (and possibly the year) if we already passed the end day of month */
					if(temp_daterange->emday < current_time_mday) {
						/*if(month==1){*/
						if(month == 11) {
							month = 0;
							year++;
							}
						else
							month++;
						}
					start_time = calculate_time_from_day_of_month(year, month, temp_daterange->smday);
					break;
				case DATERANGE_MONTH_WEEK_DAY: /* thursday 2 april */
					/* what year should we use? */
					year = (pref_time_year < current_time_year) ? current_time_year : pref_time_year;
					/* calculate time of specified weekday of specific month */
					start_time = calculate_time_from_weekday_of_month(year, temp_daterange->smon, temp_daterange->swday, temp_daterange->swday_offset);
					/* advance to next year if we've passed this month weekday already this year */
					if(start_time < preferred_time) {
						year++;
						start_time = calculate_time_from_weekday_of_month(year, temp_daterange->smon, temp_daterange->swday, temp_daterange->swday_offset);
						}
					break;
				case DATERANGE_WEEK_DAY: /* wednesday 1 */
					/* what year should we use? */
					year = (pref_time_year < current_time_year) ? current_time_year : pref_time_year;
					/* calculate time of specified weekday of month */
					start_time = calculate_time_from_weekday_of_month(year, pref_time_mon, temp_daterange->swday, temp_daterange->swday_offset);
					/* advance to next month (or year) if we've passed this weekday of this month already */
					if(start_time < preferred_time) {
						month = pref_time_mon;
						if(month == 11) {
							month = 0;
							year++;
							}
						else
							month++;
						start_time = calculate_time_from_weekday_of_month(year, month, temp_daterange->swday, temp_daterange->swday_offset);
						}
					break;
				default:
					continue;
					break;
				}

#ifdef TEST_TIMEPERIODS_B
			printf("START TIME: %lu = %s", start_time, ctime(&start_time));
#endif

			/* get the end time */
			switch(daterange_type) {
				case DATERANGE_CALENDAR_DATE:
					t->tm_sec = 0;
					t->tm_min = 0;
					t->tm_hour = 0;
					t->tm_mday = temp_daterange->emday;
					t->tm_mon = temp_daterange->emon;
					t->tm_year = (temp_daterange->eyear - 1900);
					t->tm_isdst = -1;
					end_time = mktime(t);
					break;
				case DATERANGE_MONTH_DATE:
					/* use same year as was calculated for start time above */
					end_time = calculate_time_from_day_of_month(year, temp_daterange->emon, temp_daterange->emday);
					/* advance a year if necessary: august 5 - feburary 2 */
					if(end_time < start_time) {
						year++;
						end_time = calculate_time_from_day_of_month(year, temp_daterange->emon, temp_daterange->emday);
						}
					break;
				case DATERANGE_MONTH_DAY:
					/* use same year and month as was calculated for start time above */
					end_time = calculate_time_from_day_of_month(year, month, temp_daterange->emday);
					break;
				case DATERANGE_MONTH_WEEK_DAY:
					/* use same year as was calculated for start time above */
					end_time = calculate_time_from_weekday_of_month(year, temp_daterange->emon, temp_daterange->ewday, temp_daterange->ewday_offset);
					/* advance a year if necessary: thursday 2 august - monday 3 february */
					if(end_time < start_time) {
						year++;
						end_time = calculate_time_from_weekday_of_month(year, temp_daterange->emon, temp_daterange->ewday, temp_daterange->ewday_offset);
						}
					break;
				case DATERANGE_WEEK_DAY:
					/* use same year and month as was calculated for start time above */
					end_time = calculate_time_from_weekday_of_month(year, month, temp_daterange->ewday, temp_daterange->ewday_offset);
					break;
				default:
					continue;
					break;
				}

#ifdef TEST_TIMEPERIODS_B
			printf("STARTTIME: %lu = %s", (unsigned long)start_time, ctime(&start_time));
			printf("ENDTIME1: %lu = %s", (unsigned long)end_time, ctime(&end_time));
#endif

			/* start date was bad, so skip this date range */
			if((unsigned long)start_time == 0L)
				continue;

			/* end date was bad - see if we can handle the error */
			if((unsigned long)end_time == 0L) {
				switch(daterange_type) {
					case DATERANGE_CALENDAR_DATE:
						continue;
						break;
					case DATERANGE_MONTH_DATE:
						/* end date can't be helped, so skip it */
						if(temp_daterange->emday < 0)
							continue;

						/* else end date slipped past end of month, so use last day of month as end date */
						end_time = calculate_time_from_day_of_month(year, temp_daterange->emon, -1);
						break;
					case DATERANGE_MONTH_DAY:
						/* end date can't be helped, so skip it */
						if(temp_daterange->emday < 0)
							continue;

						/* else end date slipped past end of month, so use last day of month as end date */
						end_time = calculate_time_from_day_of_month(year, month, -1);
						break;
					case DATERANGE_MONTH_WEEK_DAY:
						/* end date can't be helped, so skip it */
						if(temp_daterange->ewday_offset < 0)
							continue;

						/* else end date slipped past end of month, so use last day of month as end date */
						end_time = calculate_time_from_day_of_month(year, pref_time_mon, -1);
						break;
					case DATERANGE_WEEK_DAY:
						/* end date can't be helped, so skip it */
						if(temp_daterange->ewday_offset < 0)
							continue;

						/* else end date slipped past end of month, so use last day of month as end date */
						end_time = calculate_time_from_day_of_month(year, month, -1);
						break;
					default:
						continue;
						break;
					}
				}

#ifdef TEST_TIMEPERIODS_B
			printf("ENDTIME2: %lu = %s", (unsigned long)end_time, ctime(&end_time));
#endif

			/* if skipping days... */
			if(temp_daterange->skip_interval > 1) {

				/* advance to the next possible skip date */
				if(start_time < preferred_time) {
					/* check if interval is across dlst change and gets the compensation */
					shift = get_dst_shift(&start_time, &midnight);

					/* how many days have passed between skip start date and preferred time? */
					days = (shift + (unsigned long)midnight - (unsigned long)start_time) / (3600 * 24);

#ifdef TEST_TIMEPERIODS_B
					printf("MIDNIGHT: %lu = %s", midnight, ctime(&midnight));
					printf("%lu SECONDS PASSED\n", (midnight - (unsigned long)start_time));
					printf("%d DAYS PASSED\n", days);
					printf("REMAINDER: %d\n", (days % temp_daterange->skip_interval));
					printf("SKIP INTERVAL: %d\n", temp_daterange->skip_interval);
					printf("DLST SHIFT: %d", shift);
#endif

					/* advance start date to next skip day */
					if((days % temp_daterange->skip_interval) == 0)
						start_time += (days * 3600 * 24);
					else
						start_time += ((days - (days % temp_daterange->skip_interval) + temp_daterange->skip_interval) * 3600 * 24);
					}

				/* if skipping has no end, use start date as end */
				if((daterange_type == DATERANGE_CALENDAR_DATE) && is_daterange_single_day(temp_daterange) == TRUE)
					end_time = start_time;
				}

#ifdef TEST_TIMEPERIODS_B
			printf("\nSTART:     %lu = %s", (unsigned long)start_time, ctime(&start_time));
			printf("END:       %lu = %s", (unsigned long)end_time, ctime(&end_time));
			printf("PREFERRED: %lu = %s", (unsigned long)preferred_time, ctime(&preferred_time));
			printf("CURRENT:   %lu = %s", (unsigned long)current_time, ctime(&current_time));
#endif

			/* skip this date range its out of bounds with what we want */
			if(preferred_time > end_time)
				continue;

			/* how many days at a time should we advance? */
			if(temp_daterange->skip_interval > 1)
				advance_interval = temp_daterange->skip_interval;
			else
				advance_interval = 1;

			/* advance through the date range */
			for(day_start = start_time; day_start <= end_time; day_start += (advance_interval * 3600 * 24)) {

				/* we already found a time from a higher-precendence date range exception */
				if(day_start >= earliest_day && have_earliest_time == TRUE)
					continue;

				for(temp_timerange = temp_daterange->times; temp_timerange != NULL; temp_timerange = temp_timerange->next) {

					/* ranges with start/end of zero mean exlude this day */
					if(temp_timerange->range_start == 0 && temp_timerange->range_end == 0)
						continue;

					day_range_start = (time_t)(day_start + temp_timerange->range_start);
					day_range_end = (time_t)(day_start + temp_timerange->range_end);

#ifdef TEST_TIMEPERIODS_B
					printf("  RANGE START: %lu (%lu) = %s", temp_timerange->range_start, (unsigned long)day_range_start, ctime(&day_range_start));
					printf("  RANGE END:   %lu (%lu) = %s", temp_timerange->range_end, (unsigned long)day_range_end, ctime(&day_range_end));
#endif

					/* range is out of bounds */
					if(day_range_end < preferred_time)
						continue;

					/* preferred time occurs before range start, so use range start time as earliest potential time */
					if(day_range_start >= preferred_time)
						potential_time = day_range_start;
					/* preferred time occurs between range start/end, so use preferred time as earliest potential time */
					else if(day_range_end >= preferred_time)
						potential_time = preferred_time;

					/* is this the earliest time found thus far? */
					if(have_earliest_time == FALSE || potential_time < earliest_time) {
						have_earliest_time = TRUE;
						earliest_time = potential_time;
						earliest_day = day_start;
#ifdef TEST_TIMEPERIODS_B
						printf("    EARLIEST TIME: %lu = %s", (unsigned long)earliest_time, ctime(&earliest_time));
#endif
						}
					}
				}
			}

		}


	/**** find next available time from normal, weekly rotating schedule (in this timeperiod definition) ****/

	/* check a one week rotation of time */
	has_looped = FALSE;
	for(weekday = pref_time_wday, days_into_the_future = 0;; weekday++, days_into_the_future++) {

		/* break out of the loop if we have checked an entire week already */
		if(has_looped == TRUE && weekday >= pref_time_wday)
			break;

		if(weekday >= 7) {
			weekday -= 7;
			has_looped = TRUE;
			}

		/* calculate start of this future weekday */
		day_start = (time_t)(midnight + (days_into_the_future * 3600 * 24));

		/* we already found a time from a higher-precendence date range exception */
		if(day_start == earliest_day)
			continue;

		/* check all time ranges for this day of the week */
		for(temp_timerange = tperiod->days[weekday]; temp_timerange != NULL; temp_timerange = temp_timerange->next) {

			/* calculate the time for the start of this time range */
			day_range_start = (time_t)(day_start + temp_timerange->range_start);

			if((have_earliest_time == FALSE || day_range_start < earliest_time) && day_range_start >= preferred_time) {
				have_earliest_time = TRUE;
				earliest_time = day_range_start;
				earliest_day = day_start;
				}
			}
		}


	/* if we couldn't find a time period there must be none defined */
	if(have_earliest_time == FALSE || earliest_time == (time_t)0)
		*valid_time = (time_t)preferred_time;

	/* else use the calculated time */
	else
		*valid_time = earliest_time;

	return;
	}


/* given a preferred time, get the next valid time within a time period */
void get_next_valid_time(time_t pref_time, time_t *valid_time, timeperiod *tperiod) {
	time_t current_time = (time_t)0L;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "get_next_valid_time()\n");

	/* get time right now, preferred time must be now or in the future */
	time(&current_time);

	_get_next_valid_time(pref_time, current_time, valid_time, tperiod);
	}


/* tests if a date range covers just a single day */
int is_daterange_single_day(daterange *dr) {

	if(dr == NULL)
		return FALSE;

	if(dr->syear != dr->eyear)
		return FALSE;
	if(dr->smon != dr->emon)
		return FALSE;
	if(dr->smday != dr->emday)
		return FALSE;
	if(dr->swday != dr->ewday)
		return FALSE;
	if(dr->swday_offset != dr->ewday_offset)
		return FALSE;

	return TRUE;
	}



/* returns a time (midnight) of particular (3rd, last) day in a given month */
time_t calculate_time_from_day_of_month(int year, int month, int monthday) {
	time_t midnight;
	int day = 0;
	struct tm t;

#ifdef TEST_TIMEPERIODS
	printf("YEAR: %d, MON: %d, MDAY: %d\n", year, month, monthday);
#endif

	/* positive day (3rd day) */
	if(monthday > 0) {

		t.tm_sec = 0;
		t.tm_min = 0;
		t.tm_hour = 0;
		t.tm_year = year;
		t.tm_mon = month;
		t.tm_mday = monthday;
		t.tm_isdst = -1;

		midnight = mktime(&t);

#ifdef TEST_TIMEPERIODS
		printf("MIDNIGHT CALC: %s", ctime(&midnight));
#endif

		/* if we rolled over to the next month, time is invalid */
		/* assume the user's intention is to keep it in the current month */
		if(t.tm_mon != month)
			midnight = (time_t)0L;
		}

	/* negative offset (last day, 3rd to last day) */
	else {
		/* find last day in the month */
		day = 32;
		do {
			/* back up a day */
			day--;

			/* make the new time */
			t.tm_mon = month;
			t.tm_year = year;
			t.tm_mday = day;
			t.tm_isdst = -1;
			midnight = mktime(&t);

			}
		while(t.tm_mon != month);

		/* now that we know the last day, back up more */
		/* make the new time */
		t.tm_mon = month;
		t.tm_year = year;
		/* -1 means last day of month, so add one to to make this correct - Mike Bird */
		t.tm_mday += (monthday < -30) ? -30 : monthday + 1;
		t.tm_isdst = -1;
		midnight = mktime(&t);

		/* if we rolled over to the previous month, time is invalid */
		/* assume the user's intention is to keep it in the current month */
		if(t.tm_mon != month)
			midnight = (time_t)0L;
		}

	return midnight;
	}



/* returns a time (midnight) of particular (3rd, last) weekday in a given month */
time_t calculate_time_from_weekday_of_month(int year, int month, int weekday, int weekday_offset) {
	time_t midnight;
	int days = 0;
	int weeks = 0;
	struct tm t;

	t.tm_sec = 0;
	t.tm_min = 0;
	t.tm_hour = 0;
	t.tm_year = year;
	t.tm_mon = month;
	t.tm_mday = 1;
	t.tm_isdst = -1;

	midnight = mktime(&t);

	/* how many days must we advance to reach the first instance of the weekday this month? */
	days = weekday - (t.tm_wday);
	if(days < 0)
		days += 7;

	/* positive offset (3rd thursday) */
	if(weekday_offset > 0) {

		/* how many weeks must we advance (no more than 5 possible) */
		weeks = (weekday_offset > 5) ? 5 : weekday_offset;
		days += ((weeks - 1) * 7);

		/* make the new time */
		t.tm_mon = month;
		t.tm_year = year;
		t.tm_mday = days + 1;
		t.tm_isdst = -1;
		midnight = mktime(&t);

		/* if we rolled over to the next month, time is invalid */
		/* assume the user's intention is to keep it in the current month */
		if(t.tm_mon != month)
			midnight = (time_t)0L;
		}

	/* negative offset (last thursday, 3rd to last tuesday) */
	else {
		/* find last instance of weekday in the month */
		days += (5 * 7);
		do {
			/* back up a week */
			days -= 7;

			/* make the new time */
			t.tm_mon = month;
			t.tm_year = year;
			t.tm_mday = days + 1;
			t.tm_isdst = -1;
			midnight = mktime(&t);

			}
		while(t.tm_mon != month);

		/* now that we know the last instance of the weekday, back up more */
		weeks = (weekday_offset < -5) ? -5 : weekday_offset;
		days = ((weeks + 1) * 7);

		/* make the new time */
		t.tm_mon = month;
		t.tm_year = year;
		t.tm_mday += days;
		t.tm_isdst = -1;
		midnight = mktime(&t);

		/* if we rolled over to the previous month, time is invalid */
		/* assume the user's intention is to keep it in the current month */
		if(t.tm_mon != month)
			midnight = (time_t)0L;
		}

	return midnight;
	}


/* get the next time to schedule a log rotation */
time_t get_next_log_rotation_time(void) {
	time_t current_time;
	struct tm *t, tm_s;
	int is_dst_now = FALSE;
	time_t run_time;

	time(&current_time);
	t = localtime_r(&current_time, &tm_s);
	t->tm_min = 0;
	t->tm_sec = 0;
	is_dst_now = (t->tm_isdst > 0) ? TRUE : FALSE;

	switch(log_rotation_method) {
		case LOG_ROTATION_HOURLY:
			t->tm_hour++;
			run_time = mktime(t);
			break;
		case LOG_ROTATION_DAILY:
			t->tm_mday++;
			t->tm_hour = 0;
			run_time = mktime(t);
			break;
		case LOG_ROTATION_WEEKLY:
			t->tm_mday += (7 - t->tm_wday);
			t->tm_hour = 0;
			run_time = mktime(t);
			break;
		case LOG_ROTATION_MONTHLY:
		default:
			t->tm_mon++;
			t->tm_mday = 1;
			t->tm_hour = 0;
			run_time = mktime(t);
			break;
		}

	if(is_dst_now == TRUE && t->tm_isdst == 0)
		run_time += 3600;
	else if(is_dst_now == FALSE && t->tm_isdst > 0)
		run_time -= 3600;

	return run_time;
	}



/******************************************************************/
/******************** SIGNAL HANDLER FUNCTIONS ********************/
/******************************************************************/


/* trap signals so we can exit gracefully */
void setup_sighandler(void) {

	/* reset the shutdown flag */
	sigshutdown = FALSE;

	/* remove buffering from stderr, stdin, and stdout */
	setbuf(stdin, (char *)NULL);
	setbuf(stdout, (char *)NULL);
	setbuf(stderr, (char *)NULL);

	/* initialize signal handling */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGQUIT, sighandler);
	signal(SIGTERM, sighandler);
	signal(SIGHUP, sighandler);
	if(daemon_dumps_core == FALSE && daemon_mode == TRUE)
		signal(SIGSEGV, sighandler);

	return;
	}


/* reset signal handling... */
void reset_sighandler(void) {

	/* set signal handling to default actions */
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGSEGV, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

	return;
	}


/* handle signals */
void sighandler(int sig) {
	int x = 0;

	/* if shutdown is already true, we're in a signal trap loop! */
	/* changed 09/07/06 to only exit on segfaults */
	if(sigshutdown == TRUE && sig == SIGSEGV)
		exit(ERROR);

	caught_signal = TRUE;

	if(sig < 0)
		sig = -sig;

	for(x = 0; sigs[x] != (char *)NULL; x++);
	sig %= x;

	sig_id = sig;

	/* log errors about segfaults now, as we might not get a chance to later */
	/* all other signals are logged at a later point in main() to prevent problems with NPTL */
	if(sig == SIGSEGV)
		logit(NSLOG_PROCESS_INFO, TRUE, "Caught SIG%s, shutting down...\n", sigs[sig]);

	/* we received a SIGHUP, so restart... */
	if(sig == SIGHUP)
		sigrestart = TRUE;

	/* else begin shutting down... */
	else if(sig < 16)
		sigshutdown = TRUE;

	return;
	}


/* handle timeouts when executing service checks */
/* 07/16/08 EG also called when parent process gets a TERM signal */
void service_check_sighandler(int sig) {
	struct timeval end_time;

	/* get the current time */
	gettimeofday(&end_time, NULL);

	check_result_info.return_code = service_check_timeout_state;
	check_result_info.finish_time = end_time;
	check_result_info.early_timeout = TRUE;

	/* write check result to file */
	if(check_result_info.output_file_fp) {

		fprintf(check_result_info.output_file_fp, "finish_time=%lu.%lu\n", check_result_info.finish_time.tv_sec, check_result_info.finish_time.tv_usec);
		fprintf(check_result_info.output_file_fp, "early_timeout=%d\n", check_result_info.early_timeout);
		fprintf(check_result_info.output_file_fp, "exited_ok=%d\n", check_result_info.exited_ok);
		fprintf(check_result_info.output_file_fp, "return_code=%d\n", check_result_info.return_code);
		fprintf(check_result_info.output_file_fp, "output=%s\n", "(Service Check Timed Out)");

		/* close the temp file */
		fclose(check_result_info.output_file_fp);

		/* move check result to queue directory */
		move_check_result_to_queue(check_result_info.output_file);
		}

	/* free check result memory */
	free_check_result(&check_result_info);

	/* try to kill the command that timed out by sending termination signal to our process group */
	/* we also kill ourselves while doing this... */
	kill((pid_t)0, SIGKILL);

	/* force the child process (service check) to exit... */
	_exit(STATE_CRITICAL);
	}


/* handle timeouts when executing host checks */
/* 07/16/08 EG also called when parent process gets a TERM signal */
void host_check_sighandler(int sig) {
	struct timeval end_time;

	/* get the current time */
	gettimeofday(&end_time, NULL);

	check_result_info.return_code = STATE_CRITICAL;
	check_result_info.finish_time = end_time;
	check_result_info.early_timeout = TRUE;

	/* write check result to file */
	if(check_result_info.output_file_fp) {

		fprintf(check_result_info.output_file_fp, "finish_time=%lu.%lu\n", check_result_info.finish_time.tv_sec, check_result_info.finish_time.tv_usec);
		fprintf(check_result_info.output_file_fp, "early_timeout=%d\n", check_result_info.early_timeout);
		fprintf(check_result_info.output_file_fp, "exited_ok=%d\n", check_result_info.exited_ok);
		fprintf(check_result_info.output_file_fp, "return_code=%d\n", check_result_info.return_code);
		fprintf(check_result_info.output_file_fp, "output=%s\n", "(Host Check Timed Out)");

		/* close the temp file */
		fclose(check_result_info.output_file_fp);

		/* move check result to queue directory */
		move_check_result_to_queue(check_result_info.output_file);
		}

	/* free check result memory */
	free_check_result(&check_result_info);

	/* try to kill the command that timed out by sending termination signal to our process group */
	/* we also kill ourselves while doing this... */
	kill((pid_t)0, SIGKILL);

	/* force the child process (service check) to exit... */
	_exit(STATE_CRITICAL);
	}


/* handle timeouts when executing commands via my_system_r() */
void my_system_sighandler(int sig) {

	/* force the child process to exit... */
	_exit(STATE_CRITICAL);
	}




/******************************************************************/
/************************ DAEMON FUNCTIONS ************************/
/******************************************************************/

int daemon_init(void) {
	pid_t pid = -1;
	int pidno = 0;
	int lockfile = 0;
	int val = 0;
	char buf[256];
	struct flock lock;
	char *homedir = NULL;

#ifdef RLIMIT_CORE
	struct rlimit limit;
#endif

	/* change working directory. scuttle home if we're dumping core */
	homedir = getenv("HOME");
	if(daemon_dumps_core == TRUE && homedir != NULL)
		chdir(homedir);
	else
		chdir("/");

	umask(S_IWGRP | S_IWOTH);

	lockfile = open(lock_file, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);

	if(lockfile < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Failed to obtain lock on file %s: %s\n", lock_file, strerror(errno));
		logit(NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR, TRUE, "Bailing out due to errors encountered while attempting to daemonize... (PID=%d)", (int)getpid());

		cleanup();
		exit(ERROR);
		}

	/* see if we can read the contents of the lockfile */
	if((val = read(lockfile, buf, (size_t)10)) < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Lockfile exists but cannot be read");
		cleanup();
		exit(ERROR);
		}

	/* we read something - check the PID */
	if(val > 0) {
		if((val = sscanf(buf, "%d", &pidno)) < 1) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Lockfile '%s' does not contain a valid PID (%s)", lock_file, buf);
			cleanup();
			exit(ERROR);
			}
		}

	/* check for SIGHUP */
	if(val == 1 && (pid = (pid_t)pidno) == getpid()) {
		close(lockfile);
		return OK;
		}

	/* exit on errors... */
	if((pid = fork()) < 0)
		return(ERROR);

	/* parent process goes away.. */
	else if((int)pid != 0)
		exit(OK);

	/* child continues... */

	/* child becomes session leader... */
	setsid();

	/* place a file lock on the lock file */
	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;
	if(fcntl(lockfile, F_SETLK, &lock) < 0) {
		if(errno == EACCES || errno == EAGAIN) {
			fcntl(lockfile, F_GETLK, &lock);
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Lockfile '%s' looks like its already held by another instance of Nagios (PID %d).  Bailing out...", lock_file, (int)lock.l_pid);
			}
		else
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Cannot lock lockfile '%s': %s. Bailing out...", lock_file, strerror(errno));

		cleanup();
		exit(ERROR);
		}

	/* prevent daemon from dumping a core file... */
#ifdef RLIMIT_CORE
	if(daemon_dumps_core == FALSE) {
		getrlimit(RLIMIT_CORE, &limit);
		limit.rlim_cur = 0;
		setrlimit(RLIMIT_CORE, &limit);
		}
#endif

	/* write PID to lockfile... */
	lseek(lockfile, 0, SEEK_SET);
	ftruncate(lockfile, 0);
	sprintf(buf, "%d\n", (int)getpid());
	write(lockfile, buf, strlen(buf));

	/* make sure lock file stays open while program is executing... */
	val = fcntl(lockfile, F_GETFD, 0);
	val |= FD_CLOEXEC;
	fcntl(lockfile, F_SETFD, val);

	/* close existing stdin, stdout, stderr */
	close(0);
	close(1);
	close(2);

	/* THIS HAS TO BE DONE TO AVOID PROBLEMS WITH STDERR BEING REDIRECTED TO SERVICE MESSAGE PIPE! */
	/* re-open stdin, stdout, stderr with known values */
	open("/dev/null", O_RDONLY);
	open("/dev/null", O_WRONLY);
	open("/dev/null", O_WRONLY);

#ifdef USE_EVENT_BROKER
	/* send program data to broker */
	broker_program_state(NEBTYPE_PROCESS_DAEMONIZE, NEBFLAG_NONE, NEBATTR_NONE, NULL);
#endif

	return OK;
	}



/******************************************************************/
/*********************** SECURITY FUNCTIONS ***********************/
/******************************************************************/

/* drops privileges */
int drop_privileges(char *user, char *group) {
	uid_t uid = -1;
	gid_t gid = -1;
	struct group *grp = NULL;
	struct passwd *pw = NULL;
	int result = OK;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "drop_privileges() start\n");
	log_debug_info(DEBUGL_PROCESS, 0, "Original UID/GID: %d/%d\n", (int)getuid(), (int)getgid());

	/* only drop privileges if we're running as root, so we don't interfere with being debugged while running as some random user */
	if(getuid() != 0)
		return OK;

	/* set effective group ID */
	if(group != NULL) {

		/* see if this is a group name */
		if(strspn(group, "0123456789") < strlen(group)) {
			grp = (struct group *)getgrnam(group);
			if(grp != NULL)
				gid = (gid_t)(grp->gr_gid);
			else
				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Could not get group entry for '%s'", group);
			}

		/* else we were passed the GID */
		else
			gid = (gid_t)atoi(group);

		/* set effective group ID if other than current EGID */
		if(gid != getegid()) {

			if(setgid(gid) == -1) {
				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Could not set effective GID=%d", (int)gid);
				result = ERROR;
				}
			}
		}


	/* set effective user ID */
	if(user != NULL) {

		/* see if this is a user name */
		if(strspn(user, "0123456789") < strlen(user)) {
			pw = (struct passwd *)getpwnam(user);
			if(pw != NULL)
				uid = (uid_t)(pw->pw_uid);
			else
				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Could not get passwd entry for '%s'", user);
			}

		/* else we were passed the UID */
		else
			uid = (uid_t)atoi(user);

#ifdef HAVE_INITGROUPS

		if(uid != geteuid()) {

			/* initialize supplementary groups */
			if(initgroups(user, gid) == -1) {
				if(errno == EPERM)
					logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Unable to change supplementary groups using initgroups() -- I hope you know what you're doing");
				else {
					logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Possibly root user failed dropping privileges with initgroups()");
					return ERROR;
					}
				}
			}
#endif
		/* Change the ownership on the debug log file */
		chown_debug_log(uid, gid);

		if(setuid(uid) == -1) {
			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Could not set effective UID=%d", (int)uid);
			result = ERROR;
			}
		}

	log_debug_info(DEBUGL_PROCESS, 0, "New UID/GID: %d/%d\n", (int)getuid(), (int)getgid());

	return result;
	}




/******************************************************************/
/************************* IPC FUNCTIONS **************************/
/******************************************************************/

/* move check result to queue directory */
int move_check_result_to_queue(char *checkresult_file) {
	char *output_file = NULL;
	char *temp_buffer = NULL;
	int output_file_fd = -1;
	mode_t new_umask = 077;
	mode_t old_umask;
	int result = 0;

	/* save the file creation mask */
	old_umask = umask(new_umask);

	/* create a safe temp file */
	asprintf(&output_file, "%s/cXXXXXX", check_result_path);
	output_file_fd = mkstemp(output_file);

	/* file created okay */
	if(output_file_fd >= 0) {

		log_debug_info(DEBUGL_CHECKS, 2, "Moving temp check result file '%s' to queue file '%s'...\n", checkresult_file, output_file);

#ifdef __CYGWIN__
		/* Cygwin cannot rename open files - gives Permission Denied */
		/* close the file */
		close(output_file_fd);
#endif

		/* move the original file */
		result = my_rename(checkresult_file, output_file);

#ifndef __CYGWIN__
		/* close the file */
		close(output_file_fd);
#endif

		/* create an ok-to-go indicator file */
		asprintf(&temp_buffer, "%s.ok", output_file);
		if((output_file_fd = open(temp_buffer, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) >= 0)
			close(output_file_fd);
		my_free(temp_buffer);

		/* delete the original file if it couldn't be moved */
		if(result != 0)
			unlink(checkresult_file);
		}
	else
		result = -1;

	/* reset the file creation mask */
	umask(old_umask);

	/* log a warning on errors */
	if(result != 0)
		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Unable to move file '%s' to check results queue.\n", checkresult_file);

	/* free memory */
	my_free(output_file);

	return OK;
	}



/* processes files in the check result queue directory */
int process_check_result_queue(char *dirname) {
	char file[MAX_FILENAME_LENGTH];
	DIR *dirp = NULL;
	struct dirent *dirfile = NULL;
	register int x = 0;
	struct stat stat_buf;
	struct stat ok_stat_buf;
	char *temp_buffer = NULL;
	int result = OK;

	/* make sure we have what we need */
	if(dirname == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: No check result queue directory specified.\n");
		return ERROR;
		}

	/* open the directory for reading */
	if((dirp = opendir(dirname)) == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not open check result queue directory '%s' for reading.\n", dirname);
		return ERROR;
		}

	log_debug_info(DEBUGL_CHECKS, 1, "Starting to read check result queue '%s'...\n", dirname);

	/* process all files in the directory... */
	while((dirfile = readdir(dirp)) != NULL) {

		/* create /path/to/file */
		snprintf(file, sizeof(file), "%s/%s", dirname, dirfile->d_name);
		file[sizeof(file) - 1] = '\x0';

		/* process this if it's a check result file... */
		x = strlen(dirfile->d_name);
		if(x == 7 && dirfile->d_name[0] == 'c') {

			if(stat(file, &stat_buf) == -1) {
				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Could not stat() check result file '%s'.\n", file);
				continue;
				}

			switch(stat_buf.st_mode & S_IFMT) {

				case S_IFREG:
					/* don't process symlinked files */
					if(!S_ISREG(stat_buf.st_mode))
						continue;
					break;

				default:
					/* everything else we ignore */
					continue;
					break;
				}

			/* at this point we have a regular file... */

			/* can we find the associated ok-to-go file ? */
			asprintf(&temp_buffer, "%s.ok", file);
			result = stat(temp_buffer, &ok_stat_buf);
			my_free(temp_buffer);
			if(result == -1)
				continue;

			/* process the file */
			result = process_check_result_file(file, &check_result_list,
			                                   TRUE, TRUE);

			/* break out if we encountered an error */
			if(result == ERROR)
				break;
			}
		}

	closedir(dirp);

	return result;

	}




/* Find checks that are currently executing. This function is intended to
   be used on a Nagios restart to prevent currently executing checks from
   being rescheduled. */
int find_executing_checks(char *dirname) {
	char file[MAX_FILENAME_LENGTH];
	DIR *dirp = NULL;
	struct dirent *dirfile = NULL;
	int x = 0;
	struct stat stat_buf;
	int result = OK;
	check_result *crl = NULL;
	check_result *cr = NULL;
	service *svc = NULL;
	host *host = NULL;
	time_t current_time;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "find_executing_checks() start\n");

	/* make sure we have what we need */
	if(dirname == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: No check directory specified.\n");
		return ERROR;
		}

	/* open the directory for reading */
	if((dirp = opendir(dirname)) == NULL) {
		logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not open check directory '%s' for reading.\n", dirname);
		return ERROR;
		}

	log_debug_info(DEBUGL_CHECKS, 1, "Starting to read check directory '%s'...\n", dirname);

	/* process all files in the directory... */
	while((dirfile = readdir(dirp)) != NULL) {

		/* create /path/to/file */
		snprintf(file, sizeof(file), "%s/%s", dirname, dirfile->d_name);
		file[sizeof(file) - 1] = '\x0';

		/* process this if it's a check result file... */
		x = strlen(dirfile->d_name);
		if(x == 11 && !strncmp(dirfile->d_name, "check", 5)) {

			if(stat(file, &stat_buf) == -1) {
				logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Could not stat() check status '%s'.\n", file);
				continue;
				}

			switch(stat_buf.st_mode & S_IFMT) {

				case S_IFREG:
					/* don't process symlinked files */
					if(!S_ISREG(stat_buf.st_mode))
						continue;
					break;

				default:
					/* everything else we ignore */
					continue;
					break;
				}

			/* at this point we have a regular file... */
			log_debug_info(DEBUGL_CHECKS, 2,
			               "Looking for still-executing checks in %s.\n", file);

			/* process the file */
			result = process_check_result_file(file, &crl, FALSE, FALSE);

			/* break out if we encountered an error */
			if(result == ERROR)
				break;

			time(&current_time);

			/* examine the check results */
			while((cr = read_check_result(&crl)) != NULL) {
				if(HOST_CHECK == cr->object_check_type) {
					log_debug_info(DEBUGL_CHECKS, 2,
					               "Determining whether check for host '%s' is still executing.\n",
					               cr->host_name);
					if((host = find_host(cr->host_name)) == NULL) {
						logit(NSLOG_RUNTIME_WARNING, TRUE,
						      "Warning: Check status contained host '%s', "
						      "but the host could not be found! Ignoring "
						      "check.\n", cr->host_name);
						}
					else if(current_time - cr->start_time.tv_sec <
					        host_check_timeout) {
						log_debug_info(DEBUGL_CHECKS, 1,
						               "Check for host %s is still executing.\n",
						               cr->host_name);
						host->is_executing = TRUE;
						}
					}
				else if(SERVICE_CHECK == cr->object_check_type) {
					log_debug_info(DEBUGL_CHECKS, 2,
					               "Determining whether check for service '%s' on host '%s' is still executing.\n",
					               cr->host_name, cr->service_description);
					if((svc = find_service(cr->host_name,
					                       cr->service_description)) == NULL) {
						logit(NSLOG_RUNTIME_WARNING, TRUE,
						      "Warning: Check status contained service '%s' "
						      "on host '%s', but the service could not be "
						      "found! Ignoring check.\n",
						      cr->service_description, cr->host_name);
						}
					else if(current_time - cr->start_time.tv_sec <
					        service_check_timeout) {
						log_debug_info(DEBUGL_CHECKS, 1,
						               "Check for service %s:%s is still executing.\n",
						               cr->host_name, cr->service_description);
						svc->is_executing = TRUE;
						}
					}
				free_check_result_list(&crl);
				}
			}
		}

	closedir(dirp);

	return result;

	}




/* reads check result(s) from a file */
int process_check_result_file(char *fname, check_result **listp, int delete_file, int need_output) {
	mmapfile *thefile = NULL;
	char *input = NULL;
	char *var = NULL;
	char *val = NULL;
	char *v1 = NULL, *v2 = NULL;
	time_t current_time;
	check_result *new_cr = NULL;

	if(fname == NULL)
		return ERROR;

	time(&current_time);

	log_debug_info(DEBUGL_CHECKS, 1, "Processing check result file: '%s'\n", fname);

	/* open the file for reading */
	if((thefile = mmap_fopen(fname)) == NULL) {

		/* try removing the file - zero length files can't be mmap()'ed, so it might exist */
		unlink(fname);

		return ERROR;
		}

	/* read in all lines from the file */
	while(1) {

		/* free memory */
		my_free(input);

		/* read the next line */
		if((input = mmap_fgets_multiline(thefile)) == NULL)
			break;

		/* skip comments */
		if(input[0] == '#')
			continue;

		/* empty line indicates end of record */
		else if(input[0] == '\n') {

			/* we have something... */
			if(new_cr) {

				/* do we have the minimum amount of data? */
				if(new_cr->host_name != NULL &&
				        (!need_output || new_cr->output != NULL)) {

					/* add check result to list in memory */
					add_check_result_to_list(listp, new_cr);

					/* reset pointer */
					new_cr = NULL;
					}

				/* discard partial input */
				else {
					free_check_result(new_cr);
					init_check_result(new_cr);
					new_cr->output_file = (char *)strdup(fname);
					}
				}
			}

		if((var = my_strtok(input, "=")) == NULL)
			continue;
		if((val = my_strtok(NULL, "\n")) == NULL)
			continue;

		/* found the file time */
		if(!strcmp(var, "file_time")) {

			/* file is too old - ignore check results it contains and delete it */
			/* this will only work as intended if file_time comes before check results */
			if(max_check_result_file_age > 0 && (current_time - (strtoul(val, NULL, 0)) > max_check_result_file_age)) {
				delete_file = TRUE;
				break;
				}
			}

		/* else we have check result data */
		else {

			/* allocate new check result if necessary */
			if(new_cr == NULL) {

				if((new_cr = (check_result *)malloc(sizeof(check_result))) == NULL)
					continue;

				/* init values */
				init_check_result(new_cr);
				new_cr->output_file = (char *)strdup(fname);
				}

			if(!strcmp(var, "host_name"))
				new_cr->host_name = (char *)strdup(val);
			else if(!strcmp(var, "service_description")) {
				new_cr->service_description = (char *)strdup(val);
				new_cr->object_check_type = SERVICE_CHECK;
				}
			else if(!strcmp(var, "check_type"))
				new_cr->check_type = atoi(val);
			else if(!strcmp(var, "check_options"))
				new_cr->check_options = atoi(val);
			else if(!strcmp(var, "scheduled_check"))
				new_cr->scheduled_check = atoi(val);
			else if(!strcmp(var, "reschedule_check"))
				new_cr->reschedule_check = atoi(val);
			else if(!strcmp(var, "latency"))
				new_cr->latency = strtod(val, NULL);
			else if(!strcmp(var, "start_time")) {
				if((v1 = strtok(val, ".")) == NULL)
					continue;
				if((v2 = strtok(NULL, "\n")) == NULL)
					continue;
				new_cr->start_time.tv_sec = strtoul(v1, NULL, 0);
				new_cr->start_time.tv_usec = strtoul(v2, NULL, 0);
				}
			else if(!strcmp(var, "finish_time")) {
				if((v1 = strtok(val, ".")) == NULL)
					continue;
				if((v2 = strtok(NULL, "\n")) == NULL)
					continue;
				new_cr->finish_time.tv_sec = strtoul(v1, NULL, 0);
				new_cr->finish_time.tv_usec = strtoul(v2, NULL, 0);
				}
			else if(!strcmp(var, "early_timeout"))
				new_cr->early_timeout = atoi(val);
			else if(!strcmp(var, "exited_ok"))
				new_cr->exited_ok = atoi(val);
			else if(!strcmp(var, "return_code"))
				new_cr->return_code = atoi(val);
			else if(!strcmp(var, "output"))
				new_cr->output = (char *)strdup(val);
			}
		}

	/* we have something */
	if(new_cr) {

		/* do we have the minimum amount of data? */
		if(new_cr->host_name != NULL &&
		        (!need_output || new_cr->output != NULL)) {

			/* add check result to list in memory */
			add_check_result_to_list(listp, new_cr);

			/* reset pointer */
			new_cr = NULL;
			}

		/* discard partial input */
		/* free memory for current check result record */
		else {
			free_check_result(new_cr);
			my_free(new_cr);
			}
		}

	/* free memory and close file */
	my_free(input);
	mmap_fclose(thefile);

	/* delete the file (as well its ok-to-go file) if they exist */
	if(TRUE == delete_file)
		delete_check_result_file(fname);

	return OK;
	}




/* deletes as check result file, as well as its ok-to-go file */
int delete_check_result_file(char *fname) {
	char *temp_buffer = NULL;

	/* delete the result file */
	unlink(fname);

	/* delete the ok-to-go file */
	asprintf(&temp_buffer, "%s.ok", fname);
	unlink(temp_buffer);
	my_free(temp_buffer);

	return OK;
	}




/* reads the first host/service check result from the list in memory */
check_result *read_check_result(check_result **listp) {
	check_result *first_cr = NULL;

	if(*listp == NULL)
		return NULL;

	first_cr = *listp;
	*listp = (*listp)->next;

	return first_cr;
	}



/* initializes a host/service check result */
int init_check_result(check_result *info) {

	if(info == NULL)
		return ERROR;

	/* reset vars */
	info->object_check_type = HOST_CHECK;
	info->host_name = NULL;
	info->service_description = NULL;
	info->check_type = HOST_CHECK_ACTIVE;
	info->check_options = CHECK_OPTION_NONE;
	info->scheduled_check = FALSE;
	info->reschedule_check = FALSE;
	info->output_file_fp = NULL;
	info->output_file_fd = -1;
	info->latency = 0.0;
	info->start_time.tv_sec = 0;
	info->start_time.tv_usec = 0;
	info->finish_time.tv_sec = 0;
	info->finish_time.tv_usec = 0;
	info->early_timeout = FALSE;
	info->exited_ok = TRUE;
	info->return_code = 0;
	info->output = NULL;
	info->next = NULL;

	return OK;
	}




/* adds a new host/service check result to the list in memory */
int add_check_result_to_list(check_result **listp, check_result *new_cr) {
	check_result *temp_cr = NULL;
	check_result *last_cr = NULL;

	if(new_cr == NULL)
		return ERROR;

	/* add to list, sorted by finish time (asc) */

	/* find insertion point */
	last_cr = *listp;
	for(temp_cr = *listp; temp_cr != NULL; temp_cr = temp_cr->next) {
		if(temp_cr->finish_time.tv_sec >= new_cr->finish_time.tv_sec) {
			if(temp_cr->finish_time.tv_sec > new_cr->finish_time.tv_sec)
				break;
			else if(temp_cr->finish_time.tv_usec > new_cr->finish_time.tv_usec)
				break;
			}
		last_cr = temp_cr;
		}

	/* item goes at head of list */
	if(*listp == NULL || temp_cr == *listp) {
		new_cr->next = *listp;
		*listp = new_cr;
		}

	/* item goes in middle or at end of list */
	else {
		new_cr->next = temp_cr;
		last_cr->next = new_cr;
		}

	return OK;
	}




/* frees all memory associated with the check result list */
int free_check_result_list(check_result **listp) {
	check_result *this_cr = NULL;
	check_result *next_cr = NULL;

	for(this_cr = *listp; this_cr != NULL; this_cr = next_cr) {
		next_cr = this_cr->next;
		free_check_result(this_cr);
		my_free(this_cr);
		}

	*listp = NULL;

	return OK;
	}




/* frees memory associated with a host/service check result */
int free_check_result(check_result *info) {

	if(info == NULL)
		return OK;

	my_free(info->host_name);
	my_free(info->service_description);
	my_free(info->output_file);
	my_free(info->output);

	return OK;
	}


/* creates external command file as a named pipe (FIFO) and opens it for reading (non-blocked mode) */
int open_command_file(void) {
	struct stat st;
	int result = 0;

	/* if we're not checking external commands, don't do anything */
	if(check_external_commands == FALSE)
		return OK;

	/* the command file was already created */
	if(command_file_created == TRUE)
		return OK;

	/* reset umask (group needs write permissions) */
	umask(S_IWOTH);

	/* use existing FIFO if possible */
	if(!(stat(command_file, &st) != -1 && (st.st_mode & S_IFIFO))) {

		/* create the external command file as a named pipe (FIFO) */
		if((result = mkfifo(command_file, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) != 0) {

			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Could not create external command file '%s' as named pipe: (%d) -> %s.  If this file already exists and you are sure that another copy of Nagios is not running, you should delete this file.\n", command_file, errno, strerror(errno));
			return ERROR;
			}
		}

	/* open the command file for reading (non-blocked) - O_TRUNC flag cannot be used due to errors on some systems */
	/* NOTE: file must be opened read-write for poll() to work */
	if((command_file_fd = open(command_file, O_RDWR | O_NONBLOCK)) < 0) {

		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Could not open external command file for reading via open(): (%d) -> %s\n", errno, strerror(errno));

		return ERROR;
		}

	/* re-open the FIFO for use with fgets() */
	if((command_file_fp = (FILE *)fdopen(command_file_fd, "r")) == NULL) {

		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Could not open external command file for reading via fdopen(): (%d) -> %s\n", errno, strerror(errno));

		return ERROR;
		}

	/* initialize worker thread */
	if(init_command_file_worker_thread() == ERROR) {

		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Could not initialize command file worker thread.\n");

		/* close the command file */
		fclose(command_file_fp);

		/* delete the named pipe */
		unlink(command_file);

		return ERROR;
		}

	/* set a flag to remember we already created the file */
	command_file_created = TRUE;

	return OK;
	}


/* closes the external command file FIFO and deletes it */
int close_command_file(void) {

	/* if we're not checking external commands, don't do anything */
	if(check_external_commands == FALSE)
		return OK;

	/* the command file wasn't created or was already cleaned up */
	if(command_file_created == FALSE)
		return OK;

	/* reset our flag */
	command_file_created = FALSE;

	/* close the command file */
	fclose(command_file_fp);

	return OK;
	}




/******************************************************************/
/************************ STRING FUNCTIONS ************************/
/******************************************************************/

/* gets the next string from a buffer in memory - strings are terminated by newlines, which are removed */
char *get_next_string_from_buf(char *buf, int *start_index, int bufsize) {
	char *sptr = NULL;
	char *nl = "\n";
	int x;

	if(buf == NULL || start_index == NULL)
		return NULL;
	if(bufsize < 0)
		return NULL;
	if(*start_index >= (bufsize - 1))
		return NULL;

	sptr = buf + *start_index;

	/* end of buffer */
	if(sptr[0] == '\x0')
		return NULL;

	x = strcspn(sptr, nl);
	sptr[x] = '\x0';

	*start_index += x + 1;

	return sptr;
	}



/* determines whether or not an object name (host, service, etc) contains illegal characters */
int contains_illegal_object_chars(char *name) {
	register int x = 0;
	register int y = 0;
	register int ch = 0;

	if(name == NULL)
		return FALSE;

	x = (int)strlen(name) - 1;

	for(; x >= 0; x--) {

		ch = (int)name[x];

		/* illegal user-specified characters */
		if(illegal_object_chars != NULL)
			for(y = 0; illegal_object_chars[y]; y++)
				if(name[x] == illegal_object_chars[y])
					return TRUE;
		}

	return FALSE;
	}


/* escapes newlines in a string */
char *escape_newlines(char *rawbuf) {
	char *newbuf = NULL;
	register int x, y;

	if(rawbuf == NULL)
		return NULL;

	/* allocate enough memory to escape all chars if necessary */
	if((newbuf = malloc((strlen(rawbuf) * 2) + 1)) == NULL)
		return NULL;

	for(x = 0, y = 0; rawbuf[x] != (char)'\x0'; x++) {

		/* escape backslashes */
		if(rawbuf[x] == '\\') {
			newbuf[y++] = '\\';
			newbuf[y++] = '\\';
			}

		/* escape newlines */
		else if(rawbuf[x] == '\n') {
			newbuf[y++] = '\\';
			newbuf[y++] = 'n';
			}

		else
			newbuf[y++] = rawbuf[x];
		}
	newbuf[y] = '\x0';

	return newbuf;
	}


/* compares strings */
int compare_strings(char *val1a, char *val2a) {

	/* use the compare_hashdata() function */
	return compare_hashdata(val1a, NULL, val2a, NULL);
	}


/******************************************************************/
/************************* FILE FUNCTIONS *************************/
/******************************************************************/

/* renames a file - works across filesystems (Mike Wiacek) */
int my_rename(char *source, char *dest) {
	int rename_result = 0;


	/* make sure we have something */
	if(source == NULL || dest == NULL)
		return -1;

	/* first see if we can rename file with standard function */
	rename_result = rename(source, dest);

	/* handle any errors... */
	if(rename_result == -1) {

		/* an error occurred because the source and dest files are on different filesystems */
		if(errno == EXDEV) {

			/* try copying the file */
			if(my_fcopy(source, dest) == ERROR) {
				logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Unable to rename file '%s' to '%s': %s\n", source, dest, strerror(errno));
				return -1;
				}

			/* delete the original file */
			unlink(source);

			/* reset result since we successfully copied file */
			rename_result = 0;
			}

		/* some other error occurred */
		else {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Unable to rename file '%s' to '%s': %s\n", source, dest, strerror(errno));
			return rename_result;
			}
		}

	return rename_result;
	}

/*
 * copy a file from the path at source to the already opened
 * destination file dest.
 * This is handy when creating tempfiles with mkstemp()
 */
int my_fdcopy(char *source, char *dest, int dest_fd) {
	int source_fd, rd_result = 0, wr_result = 0;
	unsigned long tot_written = 0, tot_read = 0, buf_size = 0;
	struct stat st;
	char *buf;

	/* open source file for reading */
	if((source_fd = open(source, O_RDONLY, 0644)) < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Unable to open file '%s' for reading: %s\n", source, strerror(errno));
		return ERROR;
		}

	/*
	 * find out how large the source-file is so we can be sure
	 * we've written all of it
	 */
	if(fstat(source_fd, &st) < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Unable to stat source file '%s' for my_fcopy(): %s\n", source, strerror(errno));
		close(source_fd);
		return ERROR;
		}

	/*
	 * If the file is huge, read it and write it in chunks.
	 * This value (128K) is the result of "pick-one-at-random"
	 * with some minimal testing and may not be optimal for all
	 * hardware setups, but it should work ok for most. It's
	 * faster than 1K buffers and 1M buffers, so change at your
	 * own peril. Note that it's useful to make it fit in the L2
	 * cache, so larger isn't necessarily better.
	 */
	buf_size = st.st_size > 128 << 10 ? 128 << 10 : st.st_size;
	buf = malloc(buf_size);
	if(!buf) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Unable to malloc(%lu) bytes: %s\n", buf_size, strerror(errno));
		close(source_fd);
		return ERROR;
		}
	/* most of the times, this loop will be gone through once */
	while(tot_written < st.st_size) {
		int loop_wr = 0;

		rd_result = read(source_fd, buf, buf_size);
		if(rd_result < 0) {
			if(errno == EAGAIN || errno == EINTR)
				continue;
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: my_fcopy() failed to read from '%s': %s\n", source, strerror(errno));
			break;
			}
		tot_read += rd_result;

		while(loop_wr < rd_result) {
			wr_result = write(dest_fd, buf + loop_wr, rd_result - loop_wr);

			if(wr_result < 0) {
				if(errno == EAGAIN || errno == EINTR)
					continue;
				logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: my_fcopy() failed to write to '%s': %s\n", dest, strerror(errno));
				break;
				}
			loop_wr += wr_result;
			}
		if(wr_result < 0)
			break;
		tot_written += loop_wr;
		}

	/*
	 * clean up irregardless of how things went. dest_fd comes from
	 * our caller, so we mustn't close it.
	 */
	close(source_fd);
	free(buf);

	if(rd_result < 0 || wr_result < 0) {
		/* don't leave half-written files around */
		unlink(dest);
		return ERROR;
		}

	return OK;
	}

/* copies a file */
int my_fcopy(char *source, char *dest) {
	int dest_fd, result;

	/* make sure we have something */
	if(source == NULL || dest == NULL)
		return ERROR;

	/* unlink destination file first (not doing so can cause problems on network file systems like CIFS) */
	unlink(dest);

	/* open destination file for writing */
	if((dest_fd = open(dest, O_WRONLY | O_TRUNC | O_CREAT | O_APPEND, 0644)) < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Unable to open file '%s' for writing: %s\n", dest, strerror(errno));
		return ERROR;
		}

	result = my_fdcopy(source, dest, dest_fd);
	close(dest_fd);
	return result;
	}

/******************************************************************/
/******************** DYNAMIC BUFFER FUNCTIONS ********************/
/******************************************************************/

/* initializes a dynamic buffer */
int dbuf_init(dbuf *db, int chunk_size) {

	if(db == NULL)
		return ERROR;

	db->buf = NULL;
	db->used_size = 0L;
	db->allocated_size = 0L;
	db->chunk_size = chunk_size;

	return OK;
	}


/* frees a dynamic buffer */
int dbuf_free(dbuf *db) {

	if(db == NULL)
		return ERROR;

	if(db->buf != NULL)
		my_free(db->buf);
	db->buf = NULL;
	db->used_size = 0L;
	db->allocated_size = 0L;

	return OK;
	}


/* dynamically expands a string */
int dbuf_strcat(dbuf *db, char *buf) {
	char *newbuf = NULL;
	unsigned long buflen = 0L;
	unsigned long new_size = 0L;
	unsigned long memory_needed = 0L;

	if(db == NULL || buf == NULL)
		return ERROR;

	/* how much memory should we allocate (if any)? */
	buflen = strlen(buf);
	new_size = db->used_size + buflen + 1;

	/* we need more memory */
	if(db->allocated_size < new_size) {

		memory_needed = ((ceil(new_size / db->chunk_size) + 1) * db->chunk_size);

		/* allocate memory to store old and new string */
		if((newbuf = (char *)realloc((void *)db->buf, (size_t)memory_needed)) == NULL)
			return ERROR;

		/* update buffer pointer */
		db->buf = newbuf;

		/* update allocated size */
		db->allocated_size = memory_needed;

		/* terminate buffer */
		db->buf[db->used_size] = '\x0';
		}

	/* append the new string */
	strcat(db->buf, buf);

	/* update size allocated */
	db->used_size += buflen;

	return OK;
	}



/******************************************************************/
/******************** EMBEDDED PERL FUNCTIONS *********************/
/******************************************************************/

/* initializes embedded perl interpreter */
int init_embedded_perl(char **env) {
#ifdef EMBEDDEDPERL
	char **embedding;
	int exitstatus = 0;
	int argc = 2;
	struct stat stat_buf;

	/* make sure the P1 file exists... */
	if(p1_file == NULL || stat(p1_file, &stat_buf) != 0) {

		use_embedded_perl = FALSE;

		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: p1.pl file required for embedded Perl interpreter is missing!\n");
		}

	else {

		embedding = malloc(2 * sizeof(char *));
		if(embedding == NULL)
			return ERROR;
		*embedding = strdup("");
		*(embedding + 1) = strdup(p1_file);

		use_embedded_perl = TRUE;

		PERL_SYS_INIT3(&argc, &embedding, &env);

		if((my_perl = perl_alloc()) == NULL) {
			use_embedded_perl = FALSE;
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Could not allocate memory for embedded Perl interpreter!\n");
			}
		}

	/* a fatal error occurred... */
	if(use_embedded_perl == FALSE) {

		logit(NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR, TRUE, "Bailing out due to errors encountered while initializing the embedded Perl interpreter. (PID=%d)\n", (int)getpid());

		cleanup();
		exit(ERROR);
		}

	perl_construct(my_perl);
	exitstatus = perl_parse(my_perl, xs_init, 2, (char **)embedding, env);
	if(!exitstatus)
		exitstatus = perl_run(my_perl);

#endif
	return OK;
	}


/* closes embedded perl interpreter */
int deinit_embedded_perl(void) {
#ifdef EMBEDDEDPERL

	PL_perl_destruct_level = 0;
	perl_destruct(my_perl);
	perl_free(my_perl);
	PERL_SYS_TERM();

#endif
	return OK;
	}


/* checks to see if we should run a script using the embedded Perl interpreter */
int file_uses_embedded_perl(char *fname) {
#ifndef EMBEDDEDPERL
	return FALSE;
#else
	int line, use_epn = FALSE;
	FILE *fp = NULL;
	char buf[256] = "";

	if(enable_embedded_perl != TRUE)
		return FALSE;

	/* open the file, check if its a Perl script and see if we can use epn  */
	fp = fopen(fname, "r");
	if(fp == NULL)
		return FALSE;

	/* grab the first line - we should see Perl. go home if not */
	if(fgets(buf, 80, fp) == NULL || strstr(buf, "/bin/perl") == NULL) {
		fclose(fp);
		return FALSE;
		}

	/* epn directives must be found in first ten lines of plugin */
	for(line = 1; line < 10; line++) {
		if(fgets(buf, sizeof(buf) - 1, fp) == NULL)
			break;

		buf[sizeof(buf) - 1] = '\0';

		/* skip lines not containing nagios 'epn' directives */
		if(strstr(buf, "# nagios:")) {
			char *p;
			p = strstr(buf + 8, "epn");
			if(!p)
				continue;

			/*
			 * we found it, so close the file and return
			 * whatever it shows. '+epn' means yes. everything
			 * else means no
			 */
			fclose(fp);
			return *(p - 1) == '+' ? TRUE : FALSE;
			}
		}

	fclose(fp);

	return use_embedded_perl_implicitly;
#endif
	}





/******************************************************************/
/************************ THREAD FUNCTIONS ************************/
/******************************************************************/

/* initializes command file worker thread */
int init_command_file_worker_thread(void) {
	int result = 0;
	sigset_t newmask;

	/* initialize circular buffer */
	external_command_buffer.head = 0;
	external_command_buffer.tail = 0;
	external_command_buffer.items = 0;
	external_command_buffer.high = 0;
	external_command_buffer.overflow = 0L;
	external_command_buffer.buffer = (void **)malloc(external_command_buffer_slots * sizeof(char **));
	if(external_command_buffer.buffer == NULL)
		return ERROR;

	/* initialize mutex (only on cold startup) */
	if(sigrestart == FALSE)
		pthread_mutex_init(&external_command_buffer.buffer_lock, NULL);

	/* new thread should block all signals */
	sigfillset(&newmask);
	pthread_sigmask(SIG_BLOCK, &newmask, NULL);

	/* create worker thread */
	result = pthread_create(&worker_threads[COMMAND_WORKER_THREAD], NULL, command_file_worker_thread, NULL);

	/* main thread should unblock all signals */
	pthread_sigmask(SIG_UNBLOCK, &newmask, NULL);

	if(result)
		return ERROR;

	return OK;
	}


/* shutdown command file worker thread */
int shutdown_command_file_worker_thread(void) {
	int result = 0;

	/*
	 * calling pthread_cancel(0) will cause segfaults with some
	 * thread libraries. It's possible that will happen if the
	 * user has a number of config files larger than the max
	 * open file descriptor limit (ulimit -n) and some retarded
	 * eventbroker module leaks filedescriptors, since we'll then
	 * enter the cleanup() routine from main() before we've
	 * spawned any threads.
	 */
	if(worker_threads[COMMAND_WORKER_THREAD]) {
		/* tell the worker thread to exit */
		result = pthread_cancel(worker_threads[COMMAND_WORKER_THREAD]);

		/* wait for the worker thread to exit */
		if(result == 0) {
			result = pthread_join(worker_threads[COMMAND_WORKER_THREAD], NULL);
			}

		/* we're being called from a fork()'ed child process - can't cancel thread, so just cleanup memory */
		else {
			cleanup_command_file_worker_thread(NULL);
			}
		}

	return OK;
	}


/* clean up resources used by command file worker thread */
void cleanup_command_file_worker_thread(void *arg) {
	register int x = 0;

	/* release memory allocated to circular buffer */
	for(x = external_command_buffer.tail; x != external_command_buffer.head; x = (x + 1) % external_command_buffer_slots) {
		my_free(((char **)external_command_buffer.buffer)[x]);
		}
	my_free(external_command_buffer.buffer);

	return;
	}



/* worker thread - artificially increases buffer of named pipe */
void * command_file_worker_thread(void *arg) {
	char input_buffer[MAX_EXTERNAL_COMMAND_LENGTH];
	struct pollfd pfd;
	int pollval;
	struct timeval tv;
	int buffer_items = 0;
	int result = 0;

	/* specify cleanup routine */
	pthread_cleanup_push(cleanup_command_file_worker_thread, NULL);

	/* set cancellation info */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	while(1) {

		/* should we shutdown? */
		pthread_testcancel();

		/* wait for data to arrive */
		/* select seems to not work, so we have to use poll instead */
		/* 10-15-08 EG check into implementing William's patch @ http://blog.netways.de/2008/08/15/nagios-unter-mac-os-x-installieren/ */
		/* 10-15-08 EG poll() seems broken on OSX - see Jonathan's patch a few lines down */
		pfd.fd = command_file_fd;
		pfd.events = POLLIN;
		pollval = poll(&pfd, 1, 500);

		/* loop if no data */
		if(pollval == 0)
			continue;

		/* check for errors */
		if(pollval == -1) {

			switch(errno) {
				case EBADF:
					write_to_log("command_file_worker_thread(): poll(): EBADF", logging_options, NULL);
					break;
				case ENOMEM:
					write_to_log("command_file_worker_thread(): poll(): ENOMEM", logging_options, NULL);
					break;
				case EFAULT:
					write_to_log("command_file_worker_thread(): poll(): EFAULT", logging_options, NULL);
					break;
				case EINTR:
					/* this can happen when running under a debugger like gdb */
					/*
					write_to_log("command_file_worker_thread(): poll(): EINTR (impossible)",logging_options,NULL);
					*/
					break;
				default:
					write_to_log("command_file_worker_thread(): poll(): Unknown errno value.", logging_options, NULL);
					break;
				}

			continue;
			}

		/* should we shutdown? */
		pthread_testcancel();

		/* get number of items in the buffer */
		pthread_mutex_lock(&external_command_buffer.buffer_lock);
		buffer_items = external_command_buffer.items;
		pthread_mutex_unlock(&external_command_buffer.buffer_lock);

#ifdef DEBUG_CFWT
		printf("(CFWT) BUFFER ITEMS: %d/%d\n", buffer_items, external_command_buffer_slots);
#endif

		/* 10-15-08 Fix for OS X by Jonathan Saggau - see http://www.jonathansaggau.com/blog/2008/09/using_shark_and_custom_dtrace.html */
		/* Not sure if this would have negative effects on other OSes... */
		if(buffer_items == 0) {
			/* pause a bit so OS X doesn't go nuts with CPU overload */
			tv.tv_sec = 0;
			tv.tv_usec = 500;
			select(0, NULL, NULL, NULL, &tv);
			}

		/* process all commands in the file (named pipe) if there's some space in the buffer */
		if(buffer_items < external_command_buffer_slots) {

			/* clear EOF condition from prior run (FreeBSD fix) */
			/* FIXME: use_poll_on_cmd_pipe: Still needed? */
			clearerr(command_file_fp);

			/* read and process the next command in the file */
			while(fgets(input_buffer, (int)(sizeof(input_buffer) - 1), command_file_fp) != NULL) {

#ifdef DEBUG_CFWT
				printf("(CFWT) READ: %s", input_buffer);
#endif

				/* submit the external command for processing (retry if buffer is full) */
				while((result = submit_external_command(input_buffer, &buffer_items)) == ERROR && buffer_items == external_command_buffer_slots) {

					/* wait a bit */
					tv.tv_sec = 0;
					tv.tv_usec = 250000;
					select(0, NULL, NULL, NULL, &tv);

					/* should we shutdown? */
					pthread_testcancel();
					}

#ifdef DEBUG_CFWT
				printf("(CFWT) RES: %d, BUFFER_ITEMS: %d/%d\n", result, buffer_items, external_comand_buffer_slots);
#endif

				/* bail if the circular buffer is full */
				if(buffer_items == external_command_buffer_slots)
					break;

				/* should we shutdown? */
				pthread_testcancel();
				}
			}
		}

	/* removes cleanup handler - this should never be reached */
	pthread_cleanup_pop(0);

	return NULL;
	}



/* submits an external command for processing */
int submit_external_command(char *cmd, int *buffer_items) {
	int result = OK;

	if(cmd == NULL || external_command_buffer.buffer == NULL) {
		if(buffer_items != NULL)
			*buffer_items = -1;
		return ERROR;
		}

	/* obtain a lock for writing to the buffer */
	pthread_mutex_lock(&external_command_buffer.buffer_lock);

	if(external_command_buffer.items < external_command_buffer_slots) {

		/* save the line in the buffer */
		((char **)external_command_buffer.buffer)[external_command_buffer.head] = (char *)strdup(cmd);

		/* increment the head counter and items */
		external_command_buffer.head = (external_command_buffer.head + 1) % external_command_buffer_slots;
		external_command_buffer.items++;
		if(external_command_buffer.items > external_command_buffer.high)
			external_command_buffer.high = external_command_buffer.items;
		}

	/* buffer was full */
	else
		result = ERROR;

	/* return number of items now in buffer */
	if(buffer_items != NULL)
		*buffer_items = external_command_buffer.items;

	/* release lock on buffer */
	pthread_mutex_unlock(&external_command_buffer.buffer_lock);

	return result;
	}



/* submits a raw external command (without timestamp) for processing */
int submit_raw_external_command(char *cmd, time_t *ts, int *buffer_items) {
	char *newcmd = NULL;
	int result = OK;
	time_t timestamp;

	if(cmd == NULL)
		return ERROR;

	/* get the time */
	if(ts != NULL)
		timestamp = *ts;
	else
		time(&timestamp);

	/* create the command string */
	asprintf(&newcmd, "[%lu] %s", (unsigned long)timestamp, cmd);

	/* submit the command */
	result = submit_external_command(newcmd, buffer_items);

	/* free allocated memory */
	my_free(newcmd);

	return result;
	}



/******************************************************************/
/********************** CHECK STATS FUNCTIONS *********************/
/******************************************************************/

/* initialize check statistics data structures */
int init_check_stats(void) {
	int x = 0;
	int y = 0;

	for(x = 0; x < MAX_CHECK_STATS_TYPES; x++) {
		check_statistics[x].current_bucket = 0;
		for(y = 0; y < CHECK_STATS_BUCKETS; y++)
			check_statistics[x].bucket[y] = 0;
		check_statistics[x].overflow_bucket = 0;
		for(y = 0; y < 3; y++)
			check_statistics[x].minute_stats[y] = 0;
		check_statistics[x].last_update = (time_t)0L;
		}

	return OK;
	}


/* records stats for a given type of check */
int update_check_stats(int check_type, time_t check_time) {
	time_t current_time;
	unsigned long minutes = 0L;
	int new_current_bucket = 0;
	int this_bucket = 0;
	int x = 0;

	if(check_type < 0 || check_type >= MAX_CHECK_STATS_TYPES)
		return ERROR;

	time(&current_time);

	if((unsigned long)check_time == 0L) {
#ifdef DEBUG_CHECK_STATS
		printf("TYPE[%d] CHECK TIME==0!\n", check_type);
#endif
		check_time = current_time;
		}

	/* do some sanity checks on the age of the stats data before we start... */
	/* get the new current bucket number */
	minutes = ((unsigned long)check_time - (unsigned long)program_start) / 60;
	new_current_bucket = minutes % CHECK_STATS_BUCKETS;

	/* its been more than 15 minutes since stats were updated, so clear the stats */
	if((((unsigned long)current_time - (unsigned long)check_statistics[check_type].last_update) / 60) > CHECK_STATS_BUCKETS) {
		for(x = 0; x < CHECK_STATS_BUCKETS; x++)
			check_statistics[check_type].bucket[x] = 0;
		check_statistics[check_type].overflow_bucket = 0;
#ifdef DEBUG_CHECK_STATS
		printf("CLEARING ALL: TYPE[%d], CURRENT=%lu, LASTUPDATE=%lu\n", check_type, (unsigned long)current_time, (unsigned long)check_statistics[check_type].last_update);
#endif
		}

	/* different current bucket number than last time */
	else if(new_current_bucket != check_statistics[check_type].current_bucket) {

		/* clear stats in buckets between last current bucket and new current bucket - stats haven't been updated in a while */
		for(x = check_statistics[check_type].current_bucket; x < (CHECK_STATS_BUCKETS * 2); x++) {

			this_bucket = (x + CHECK_STATS_BUCKETS + 1) % CHECK_STATS_BUCKETS;

			if(this_bucket == new_current_bucket)
				break;

#ifdef DEBUG_CHECK_STATS
			printf("CLEARING BUCKET %d, (NEW=%d, OLD=%d)\n", this_bucket, new_current_bucket, check_statistics[check_type].current_bucket);
#endif

			/* clear old bucket value */
			check_statistics[check_type].bucket[this_bucket] = 0;
			}

		/* update the current bucket number, push old value to overflow bucket */
		check_statistics[check_type].overflow_bucket = check_statistics[check_type].bucket[new_current_bucket];
		check_statistics[check_type].current_bucket = new_current_bucket;
		check_statistics[check_type].bucket[new_current_bucket] = 0;
		}
#ifdef DEBUG_CHECK_STATS
	else
		printf("NO CLEARING NEEDED\n");
#endif


	/* increment the value of the current bucket */
	check_statistics[check_type].bucket[new_current_bucket]++;

#ifdef DEBUG_CHECK_STATS
	printf("TYPE[%d].BUCKET[%d]=%d\n", check_type, new_current_bucket, check_statistics[check_type].bucket[new_current_bucket]);
	printf("   ");
	for(x = 0; x < CHECK_STATS_BUCKETS; x++)
		printf("[%d] ", check_statistics[check_type].bucket[x]);
	printf(" (%d)\n", check_statistics[check_type].overflow_bucket);
#endif

	/* record last update time */
	check_statistics[check_type].last_update = current_time;

	return OK;
	}


/* generate 1/5/15 minute stats for a given type of check */
int generate_check_stats(void) {
	time_t current_time;
	int x = 0;
	int new_current_bucket = 0;
	int this_bucket = 0;
	int last_bucket = 0;
	int this_bucket_value = 0;
	int last_bucket_value = 0;
	int bucket_value = 0;
	int seconds = 0;
	int minutes = 0;
	int check_type = 0;
	float this_bucket_weight = 0.0;
	float last_bucket_weight = 0.0;
	int left_value = 0;
	int right_value = 0;


	time(&current_time);

	/* do some sanity checks on the age of the stats data before we start... */
	/* get the new current bucket number */
	minutes = ((unsigned long)current_time - (unsigned long)program_start) / 60;
	new_current_bucket = minutes % CHECK_STATS_BUCKETS;
	for(check_type = 0; check_type < MAX_CHECK_STATS_TYPES; check_type++) {

		/* its been more than 15 minutes since stats were updated, so clear the stats */
		if((((unsigned long)current_time - (unsigned long)check_statistics[check_type].last_update) / 60) > CHECK_STATS_BUCKETS) {
			for(x = 0; x < CHECK_STATS_BUCKETS; x++)
				check_statistics[check_type].bucket[x] = 0;
			check_statistics[check_type].overflow_bucket = 0;
#ifdef DEBUG_CHECK_STATS
			printf("GEN CLEARING ALL: TYPE[%d], CURRENT=%lu, LASTUPDATE=%lu\n", check_type, (unsigned long)current_time, (unsigned long)check_statistics[check_type].last_update);
#endif
			}

		/* different current bucket number than last time */
		else if(new_current_bucket != check_statistics[check_type].current_bucket) {

			/* clear stats in buckets between last current bucket and new current bucket - stats haven't been updated in a while */
			for(x = check_statistics[check_type].current_bucket; x < (CHECK_STATS_BUCKETS * 2); x++) {

				this_bucket = (x + CHECK_STATS_BUCKETS + 1) % CHECK_STATS_BUCKETS;

				if(this_bucket == new_current_bucket)
					break;

#ifdef DEBUG_CHECK_STATS
				printf("GEN CLEARING BUCKET %d, (NEW=%d, OLD=%d), CURRENT=%lu, LASTUPDATE=%lu\n", this_bucket, new_current_bucket, check_statistics[check_type].current_bucket, (unsigned long)current_time, (unsigned long)check_statistics[check_type].last_update);
#endif

				/* clear old bucket value */
				check_statistics[check_type].bucket[this_bucket] = 0;
				}

			/* update the current bucket number, push old value to overflow bucket */
			check_statistics[check_type].overflow_bucket = check_statistics[check_type].bucket[new_current_bucket];
			check_statistics[check_type].current_bucket = new_current_bucket;
			check_statistics[check_type].bucket[new_current_bucket] = 0;
			}
#ifdef DEBUG_CHECK_STATS
		else
			printf("GEN NO CLEARING NEEDED: TYPE[%d], CURRENT=%lu, LASTUPDATE=%lu\n", check_type, (unsigned long)current_time, (unsigned long)check_statistics[check_type].last_update);
#endif

		/* update last check time */
		check_statistics[check_type].last_update = current_time;
		}

	/* determine weights to use for this/last buckets */
	seconds = ((unsigned long)current_time - (unsigned long)program_start) % 60;
	this_bucket_weight = (seconds / 60.0);
	last_bucket_weight = ((60 - seconds) / 60.0);

	/* update statistics for all check types */
	for(check_type = 0; check_type < MAX_CHECK_STATS_TYPES; check_type++) {

		/* clear the old statistics */
		for(x = 0; x < 3; x++)
			check_statistics[check_type].minute_stats[x] = 0;

		/* loop through each bucket */
		for(x = 0; x < CHECK_STATS_BUCKETS; x++) {

			/* which buckets should we use for this/last bucket? */
			this_bucket = (check_statistics[check_type].current_bucket + CHECK_STATS_BUCKETS - x) % CHECK_STATS_BUCKETS;
			last_bucket = (this_bucket + CHECK_STATS_BUCKETS - 1) % CHECK_STATS_BUCKETS;

			/* raw/unweighted value for this bucket */
			this_bucket_value = check_statistics[check_type].bucket[this_bucket];

			/* raw/unweighted value for last bucket - use overflow bucket if last bucket is current bucket */
			if(last_bucket == check_statistics[check_type].current_bucket)
				last_bucket_value = check_statistics[check_type].overflow_bucket;
			else
				last_bucket_value = check_statistics[check_type].bucket[last_bucket];

			/* determine value by weighting this/last buckets... */
			/* if this is the current bucket, use its full value + weighted % of last bucket */
			if(x == 0) {
				right_value = this_bucket_value;
				left_value = (int)floor(last_bucket_value * last_bucket_weight);
				bucket_value = (int)(this_bucket_value + floor(last_bucket_value * last_bucket_weight));
				}
			/* otherwise use weighted % of this and last bucket */
			else {
				right_value = (int)ceil(this_bucket_value * this_bucket_weight);
				left_value = (int)floor(last_bucket_value * last_bucket_weight);
				bucket_value = (int)(ceil(this_bucket_value * this_bucket_weight) + floor(last_bucket_value * last_bucket_weight));
				}

			/* 1 minute stats */
			if(x == 0)
				check_statistics[check_type].minute_stats[0] = bucket_value;

			/* 5 minute stats */
			if(x < 5)
				check_statistics[check_type].minute_stats[1] += bucket_value;

			/* 15 minute stats */
			if(x < 15)
				check_statistics[check_type].minute_stats[2] += bucket_value;

#ifdef DEBUG_CHECK_STATS2
			printf("X=%d, THIS[%d]=%d, LAST[%d]=%d, 1/5/15=%d,%d,%d  L=%d R=%d\n", x, this_bucket, this_bucket_value, last_bucket, last_bucket_value, check_statistics[check_type].minute_stats[0], check_statistics[check_type].minute_stats[1], check_statistics[check_type].minute_stats[2], left_value, right_value);
#endif
			/* record last update time */
			check_statistics[check_type].last_update = current_time;
			}

#ifdef DEBUG_CHECK_STATS
		printf("TYPE[%d]   1/5/15 = %d, %d, %d (seconds=%d, this_weight=%f, last_weight=%f)\n", check_type, check_statistics[check_type].minute_stats[0], check_statistics[check_type].minute_stats[1], check_statistics[check_type].minute_stats[2], seconds, this_bucket_weight, last_bucket_weight);
#endif
		}

	return OK;
	}




/******************************************************************/
/************************ UPDATE FUNCTIONS ************************/
/******************************************************************/

/* check for new releases of Nagios */
int check_for_nagios_updates(int force, int reschedule) {
	time_t current_time;
	int result = OK;
	int api_result = OK;
	int do_check = TRUE;
	time_t next_check = 0L;
	unsigned int rand_seed = 0;
	int randnum = 0;

	time(&current_time);

	/*
	printf("NOW: %s",ctime(&current_time));
	printf("LAST CHECK: %s",ctime(&last_update_check));
	*/

	/* seed the random generator */
	rand_seed = (unsigned int)(current_time + nagios_pid);
	srand(rand_seed);

	/* generate a (probably) unique ID for this nagios install */
	/* the server api currently sees thousands of nagios installs behind single ip addresses, so this help determine if there are really thousands of servers out there, or if some nagios installs are misbehaving */
	if(update_uid == 0L)
		update_uid = current_time;

	/* update chekcs are disabled */
	if(check_for_updates == FALSE)
		do_check = FALSE;
	/* we checked for updates recently, so don't do it again */
	if((current_time - last_update_check) < MINIMUM_UPDATE_CHECK_INTERVAL)
		do_check = FALSE;
	/* the check is being forced */
	if(force == TRUE)
		do_check = TRUE;

	/* do a check */
	if(do_check == TRUE) {

		/*printf("RUNNING QUERY...\n");*/

		/* query api */
		api_result = query_update_api();
		}

	/* should we reschedule the update check? */
	if(reschedule == TRUE) {

		/*printf("RESCHEDULING...\n");*/

		randnum = rand();
		/*
		printf("RAND: %d\n",randnum);
		printf("RANDMAX: %d\n",RAND_MAX);
		printf("UCIW: %d\n",UPDATE_CHECK_INTERVAL_WOBBLE);
		printf("MULT: %f\n",(float)randnum/RAND_MAX);
		*/



		/* we didn't do an update, so calculate next possible update time */
		if(do_check == FALSE) {
			next_check = last_update_check + BASE_UPDATE_CHECK_INTERVAL;
			next_check = next_check + (unsigned long)(((float)randnum / RAND_MAX) * UPDATE_CHECK_INTERVAL_WOBBLE);
			}

		/* we tried to check for an update */
		else {

			/* api query was okay */
			if(api_result == OK) {
				next_check = current_time + BASE_UPDATE_CHECK_INTERVAL;
				next_check += (unsigned long)(((float)randnum / RAND_MAX) * UPDATE_CHECK_INTERVAL_WOBBLE);
				}

			/* query resulted in an error - retry at a shorter interval */
			else {
				next_check = current_time + BASE_UPDATE_CHECK_RETRY_INTERVAL;
				next_check += (unsigned long)(((float)randnum / RAND_MAX) * UPDATE_CHECK_RETRY_INTERVAL_WOBBLE);
				}
			}

		/* make sure next check isn't in the past - if it is, schedule a check in 1 minute */
		if(next_check < current_time)
			next_check = current_time + 60;

		/*printf("NEXT CHECK: %s",ctime(&next_check));*/

		/* schedule the next update event */
		schedule_new_event(EVENT_CHECK_PROGRAM_UPDATE, TRUE, next_check, FALSE, BASE_UPDATE_CHECK_INTERVAL, NULL, TRUE, NULL, NULL, 0);
		}

	return result;
	}



/* checks for updates at api.nagios.org */
int query_update_api(void) {
	char *api_server = "api.nagios.org";
	char *api_path = "/versioncheck/";
	char *api_query = NULL;
	char *api_query_opts = NULL;
	char *buf = NULL;
	char recv_buf[1024];
	int report_install = FALSE;
	int result = OK;
	char *ptr = NULL;
	int current_line = 0;
	int buf_index = 0;
	int in_header = TRUE;
	char *var = NULL;
	char *val = NULL;
	int sd = 0;
	int send_len = 0;
	int recv_len = 0;
	int update_check_succeeded = FALSE;

	/* report a new install, upgrade, or rollback */
	/* Nagios monitors the world and we monitor Nagios taking over the world. :-) */
	if(last_update_check == (time_t)0L)
		report_install = TRUE;
	if(last_program_version == NULL || strcmp(PROGRAM_VERSION, last_program_version))
		report_install = TRUE;
	if(report_install == TRUE) {
		asprintf(&api_query_opts, "&firstcheck=1");
		if(last_program_version != NULL) {
			char *qopts2 = NULL;
			asprintf(&qopts2, "%s&last_version=%s", api_query_opts, last_program_version);
			my_free(api_query_opts);
			api_query_opts = qopts2;
			}
		}

	/* generate the query */
	asprintf(&api_query, "v=1&product=nagios&tinycheck=1&stableonly=1&uid=%lu", update_uid);
	if(bare_update_check == FALSE) {
		char *api_query2 = NULL;
		asprintf(&api_query2, "%s&version=%s%s", api_query, PROGRAM_VERSION, (api_query_opts == NULL) ? "" : api_query_opts);
		my_free(api_query);
		api_query = api_query2;
		}

	/* generate the HTTP request */
	asprintf(&buf,
	         "POST %s HTTP/1.0\r\nUser-Agent: Nagios/%s\r\n"
	         "Connection: close\r\nHost: %s\r\n"
	         "Content-Type: application/x-www-form-urlencoded\r\n"
	         "Content-Length: %zd\r\n\r\n%s",
	         api_path, PROGRAM_VERSION, api_server,
	         strlen(api_query), api_query);

	/*
	printf("SENDING...\n");
	printf("==========\n");
	printf("%s",buf);
	printf("\n");
	*/


	result = my_tcp_connect(api_server, 80, &sd, 2);
	/*printf("CONN RESULT: %d, SD: %d\n",result,sd);*/
	if(sd > 0) {

		/* send request */
		send_len = strlen(buf);
		result = my_sendall(sd, buf, &send_len, 2);
		/*printf("SEND RESULT: %d, SENT: %d\n",result,send_len);*/

		/* get response */
		recv_len = sizeof(recv_buf);
		result = my_recvall(sd, recv_buf, &recv_len, 2);
		recv_buf[sizeof(recv_buf) - 1] = '\x0';
		/*printf("RECV RESULT: %d, RECEIVED: %d\n",result,recv_len);*/

		/*
		printf("\n");
		printf("RECEIVED...\n");
		printf("===========\n");
		printf("%s",recv_buf);
		printf("\n");
		*/

		/* close connection */
		close(sd);

		/* parse the result */
		in_header = TRUE;
		while((ptr = get_next_string_from_buf(recv_buf, &buf_index, sizeof(recv_buf)))) {

			strip(ptr);
			current_line++;

			if(!strcmp(ptr, "")) {
				in_header = FALSE;
				continue;
				}
			if(in_header == TRUE)
				continue;

			var = strtok(ptr, "=");
			val = strtok(NULL, "\n");
			/*printf("VAR: %s, VAL: %s\n",var,val);*/

			if(!strcmp(var, "UPDATE_AVAILABLE")) {
				update_available = atoi(val);
				/* we were successful */
				update_check_succeeded = TRUE;
				}
			else if(!strcmp(var, "UPDATE_VERSION")) {
				if(new_program_version)
					my_free(new_program_version);
				new_program_version = strdup(val);
				}
			else if(!strcmp(var, "UPDATE_RELEASEDATE")) {
				}
			}
		}

	/* cleanup */
	my_free(buf);
	my_free(api_query);
	my_free(api_query_opts);

	/* we were successful! */
	if(update_check_succeeded == TRUE) {

		time(&last_update_check);
		if(last_program_version)
			free(last_program_version);
		last_program_version = (char *)strdup(PROGRAM_VERSION);
		}

	return OK;
	}




/******************************************************************/
/************************* MISC FUNCTIONS *************************/
/******************************************************************/

/* returns Nagios version */
char *get_program_version(void) {

	return (char *)PROGRAM_VERSION;
	}


/* returns Nagios modification date */
char *get_program_modification_date(void) {

	return (char *)PROGRAM_MODIFICATION_DATE;
	}



/******************************************************************/
/*********************** CLEANUP FUNCTIONS ************************/
/******************************************************************/

/* do some cleanup before we exit */
void cleanup(void) {

#ifdef USE_EVENT_BROKER
	/* unload modules */
	if(test_scheduling == FALSE && verify_config == FALSE) {
		neb_free_callback_list();
		neb_unload_all_modules(NEBMODULE_FORCE_UNLOAD, (sigshutdown == TRUE) ? NEBMODULE_NEB_SHUTDOWN : NEBMODULE_NEB_RESTART);
		neb_free_module_list();
		neb_deinit_modules();
		}
#endif

	/* free all allocated memory - including macros */
	free_memory(get_global_macros());

	return;
	}


/* free the memory allocated to the linked lists */
void free_memory(nagios_macros *mac) {
	timed_event *this_event = NULL;
	timed_event *next_event = NULL;

	/* free all allocated memory for the object definitions */
	free_object_data();

	/* free memory allocated to comments */
	free_comment_data();

	/* free check result list */
	free_check_result_list(&check_result_list);

	/* free memory for the high priority event list */
	this_event = event_list_high;
	while(this_event != NULL) {
		if(this_event->event_type == EVENT_SCHEDULED_DOWNTIME)
			my_free(this_event->event_data);
		next_event = this_event->next;
		my_free(this_event);
		this_event = next_event;
		}

	/* reset the event pointer */
	event_list_high = NULL;

	/* free memory for the low priority event list */
	this_event = event_list_low;
	while(this_event != NULL) {
		if(this_event->event_type == EVENT_SCHEDULED_DOWNTIME)
			my_free(this_event->event_data);
		next_event = this_event->next;
		my_free(this_event);
		this_event = next_event;
		}

	/* reset the event pointer */
	event_list_low = NULL;

	/* free memory for global event handlers */
	my_free(global_host_event_handler);
	my_free(global_service_event_handler);

	/* free any notification list that may have been overlooked */
	free_notification_list();

	/* free obsessive compulsive commands */
	my_free(ocsp_command);
	my_free(ochp_command);

	/*
	 * free memory associated with macros.
	 * It's ok to only free the volatile ones, as the non-volatile
	 * are always free()'d before assignment if they're set.
	 * Doing a full free of them here means we'll wipe the constant
	 * macros when we get a reload or restart request through the
	 * command pipe, or when we receive a SIGHUP.
	 */
	clear_volatile_macros_r(mac);

	free_macrox_names();

	/* free illegal char strings */
	my_free(illegal_object_chars);
	my_free(illegal_output_chars);

	/* free nagios user and group */
	my_free(nagios_user);
	my_free(nagios_group);

	/* free version strings */
	my_free(last_program_version);
	my_free(new_program_version);

	/* free file/path variables */
	my_free(log_file);
	my_free(debug_file);
	my_free(temp_file);
	my_free(temp_path);
	my_free(check_result_path);
	my_free(command_file);
	my_free(lock_file);
	my_free(auth_file);
	my_free(p1_file);
	my_free(log_archive_path);

	return;
	}


/* free a notification list that was created */
void free_notification_list(void) {
	notification *temp_notification = NULL;
	notification *next_notification = NULL;

	temp_notification = notification_list;
	while(temp_notification != NULL) {
		next_notification = temp_notification->next;
		my_free(temp_notification);
		temp_notification = next_notification;
		}

	/* reset notification list pointer */
	notification_list = NULL;

	return;
	}


/* reset all system-wide variables, so when we've receive a SIGHUP we can restart cleanly */
int reset_variables(void) {

	log_file = (char *)strdup(DEFAULT_LOG_FILE);
	temp_file = (char *)strdup(DEFAULT_TEMP_FILE);
	temp_path = (char *)strdup(DEFAULT_TEMP_PATH);
	check_result_path = (char *)strdup(DEFAULT_CHECK_RESULT_PATH);
	command_file = (char *)strdup(DEFAULT_COMMAND_FILE);
	lock_file = (char *)strdup(DEFAULT_LOCK_FILE);
	auth_file = (char *)strdup(DEFAULT_AUTH_FILE);
	p1_file = (char *)strdup(DEFAULT_P1_FILE);
	log_archive_path = (char *)strdup(DEFAULT_LOG_ARCHIVE_PATH);
	debug_file = (char *)strdup(DEFAULT_DEBUG_FILE);

	nagios_user = (char *)strdup(DEFAULT_NAGIOS_USER);
	nagios_group = (char *)strdup(DEFAULT_NAGIOS_GROUP);

	use_regexp_matches = FALSE;
	use_true_regexp_matching = FALSE;

	use_syslog = DEFAULT_USE_SYSLOG;
	log_service_retries = DEFAULT_LOG_SERVICE_RETRIES;
	log_host_retries = DEFAULT_LOG_HOST_RETRIES;
	log_initial_states = DEFAULT_LOG_INITIAL_STATES;

	log_notifications = DEFAULT_NOTIFICATION_LOGGING;
	log_event_handlers = DEFAULT_LOG_EVENT_HANDLERS;
	log_external_commands = DEFAULT_LOG_EXTERNAL_COMMANDS;
	log_passive_checks = DEFAULT_LOG_PASSIVE_CHECKS;

	logging_options = NSLOG_RUNTIME_ERROR | NSLOG_RUNTIME_WARNING | NSLOG_VERIFICATION_ERROR | NSLOG_VERIFICATION_WARNING | NSLOG_CONFIG_ERROR | NSLOG_CONFIG_WARNING | NSLOG_PROCESS_INFO | NSLOG_HOST_NOTIFICATION | NSLOG_SERVICE_NOTIFICATION | NSLOG_EVENT_HANDLER | NSLOG_EXTERNAL_COMMAND | NSLOG_PASSIVE_CHECK | NSLOG_HOST_UP | NSLOG_HOST_DOWN | NSLOG_HOST_UNREACHABLE | NSLOG_SERVICE_OK | NSLOG_SERVICE_WARNING | NSLOG_SERVICE_UNKNOWN | NSLOG_SERVICE_CRITICAL | NSLOG_INFO_MESSAGE;

	syslog_options = NSLOG_RUNTIME_ERROR | NSLOG_RUNTIME_WARNING | NSLOG_VERIFICATION_ERROR | NSLOG_VERIFICATION_WARNING | NSLOG_CONFIG_ERROR | NSLOG_CONFIG_WARNING | NSLOG_PROCESS_INFO | NSLOG_HOST_NOTIFICATION | NSLOG_SERVICE_NOTIFICATION | NSLOG_EVENT_HANDLER | NSLOG_EXTERNAL_COMMAND | NSLOG_PASSIVE_CHECK | NSLOG_HOST_UP | NSLOG_HOST_DOWN | NSLOG_HOST_UNREACHABLE | NSLOG_SERVICE_OK | NSLOG_SERVICE_WARNING | NSLOG_SERVICE_UNKNOWN | NSLOG_SERVICE_CRITICAL | NSLOG_INFO_MESSAGE;

	service_check_timeout = DEFAULT_SERVICE_CHECK_TIMEOUT;
	host_check_timeout = DEFAULT_HOST_CHECK_TIMEOUT;
	event_handler_timeout = DEFAULT_EVENT_HANDLER_TIMEOUT;
	notification_timeout = DEFAULT_NOTIFICATION_TIMEOUT;
	ocsp_timeout = DEFAULT_OCSP_TIMEOUT;
	ochp_timeout = DEFAULT_OCHP_TIMEOUT;

	sleep_time = DEFAULT_SLEEP_TIME;
	interval_length = DEFAULT_INTERVAL_LENGTH;
	service_inter_check_delay_method = ICD_SMART;
	host_inter_check_delay_method = ICD_SMART;
	service_interleave_factor_method = ILF_SMART;
	max_service_check_spread = DEFAULT_SERVICE_CHECK_SPREAD;
	max_host_check_spread = DEFAULT_HOST_CHECK_SPREAD;

	use_aggressive_host_checking = DEFAULT_AGGRESSIVE_HOST_CHECKING;
	cached_host_check_horizon = DEFAULT_CACHED_HOST_CHECK_HORIZON;
	cached_service_check_horizon = DEFAULT_CACHED_SERVICE_CHECK_HORIZON;
	enable_predictive_host_dependency_checks = DEFAULT_ENABLE_PREDICTIVE_HOST_DEPENDENCY_CHECKS;
	enable_predictive_service_dependency_checks = DEFAULT_ENABLE_PREDICTIVE_SERVICE_DEPENDENCY_CHECKS;

	soft_state_dependencies = FALSE;

	retain_state_information = FALSE;
	retention_update_interval = DEFAULT_RETENTION_UPDATE_INTERVAL;
	use_retained_program_state = TRUE;
	use_retained_scheduling_info = FALSE;
	retention_scheduling_horizon = DEFAULT_RETENTION_SCHEDULING_HORIZON;
	modified_host_process_attributes = MODATTR_NONE;
	modified_service_process_attributes = MODATTR_NONE;
	retained_host_attribute_mask = 0L;
	retained_service_attribute_mask = 0L;
	retained_process_host_attribute_mask = 0L;
	retained_process_service_attribute_mask = 0L;
	retained_contact_host_attribute_mask = 0L;
	retained_contact_service_attribute_mask = 0L;

	command_check_interval = DEFAULT_COMMAND_CHECK_INTERVAL;
	check_reaper_interval = DEFAULT_CHECK_REAPER_INTERVAL;
	max_check_reaper_time = DEFAULT_MAX_REAPER_TIME;
	max_check_result_file_age = DEFAULT_MAX_CHECK_RESULT_AGE;
	service_freshness_check_interval = DEFAULT_FRESHNESS_CHECK_INTERVAL;
	host_freshness_check_interval = DEFAULT_FRESHNESS_CHECK_INTERVAL;
	auto_rescheduling_interval = DEFAULT_AUTO_RESCHEDULING_INTERVAL;
	auto_rescheduling_window = DEFAULT_AUTO_RESCHEDULING_WINDOW;

	check_external_commands = DEFAULT_CHECK_EXTERNAL_COMMANDS;
	check_orphaned_services = DEFAULT_CHECK_ORPHANED_SERVICES;
	check_orphaned_hosts = DEFAULT_CHECK_ORPHANED_HOSTS;
	check_service_freshness = DEFAULT_CHECK_SERVICE_FRESHNESS;
	check_host_freshness = DEFAULT_CHECK_HOST_FRESHNESS;
	auto_reschedule_checks = DEFAULT_AUTO_RESCHEDULE_CHECKS;

	log_rotation_method = LOG_ROTATION_NONE;

	last_command_check = 0L;
	last_command_status_update = 0L;
	last_log_rotation = 0L;

	max_parallel_service_checks = DEFAULT_MAX_PARALLEL_SERVICE_CHECKS;
	currently_running_service_checks = 0;

	enable_notifications = TRUE;
	execute_service_checks = TRUE;
	accept_passive_service_checks = TRUE;
	execute_host_checks = TRUE;
	accept_passive_service_checks = TRUE;
	enable_event_handlers = TRUE;
	obsess_over_services = FALSE;
	obsess_over_hosts = FALSE;
	enable_failure_prediction = TRUE;

	next_comment_id = 0L; /* comment and downtime id get initialized to nonzero elsewhere */
	next_downtime_id = 0L;
	next_event_id = 1;
	next_notification_id = 1;

	aggregate_status_updates = TRUE;
	status_update_interval = DEFAULT_STATUS_UPDATE_INTERVAL;

	event_broker_options = BROKER_NOTHING;

	time_change_threshold = DEFAULT_TIME_CHANGE_THRESHOLD;

	enable_flap_detection = DEFAULT_ENABLE_FLAP_DETECTION;
	low_service_flap_threshold = DEFAULT_LOW_SERVICE_FLAP_THRESHOLD;
	high_service_flap_threshold = DEFAULT_HIGH_SERVICE_FLAP_THRESHOLD;
	low_host_flap_threshold = DEFAULT_LOW_HOST_FLAP_THRESHOLD;
	high_host_flap_threshold = DEFAULT_HIGH_HOST_FLAP_THRESHOLD;

	process_performance_data = DEFAULT_PROCESS_PERFORMANCE_DATA;

	translate_passive_host_checks = DEFAULT_TRANSLATE_PASSIVE_HOST_CHECKS;
	passive_host_checks_are_soft = DEFAULT_PASSIVE_HOST_CHECKS_SOFT;

	use_large_installation_tweaks = DEFAULT_USE_LARGE_INSTALLATION_TWEAKS;
	enable_environment_macros = FALSE;
	free_child_process_memory = FALSE;
	child_processes_fork_twice = FALSE;

	additional_freshness_latency = DEFAULT_ADDITIONAL_FRESHNESS_LATENCY;

	enable_embedded_perl = DEFAULT_ENABLE_EMBEDDED_PERL;
	use_embedded_perl_implicitly = DEFAULT_USE_EMBEDDED_PERL_IMPLICITLY;

	external_command_buffer_slots = DEFAULT_EXTERNAL_COMMAND_BUFFER_SLOTS;

	debug_level = DEFAULT_DEBUG_LEVEL;
	debug_verbosity = DEFAULT_DEBUG_VERBOSITY;
	max_debug_file_size = DEFAULT_MAX_DEBUG_FILE_SIZE;

	date_format = DATE_FORMAT_US;

	/* initialize macros */
	init_macros();

	global_host_event_handler = NULL;
	global_service_event_handler = NULL;
	global_host_event_handler_ptr = NULL;
	global_service_event_handler_ptr = NULL;

	ocsp_command = NULL;
	ochp_command = NULL;
	ocsp_command_ptr = NULL;
	ochp_command_ptr = NULL;

	/* reset umask */
	umask(S_IWGRP | S_IWOTH);

	return OK;
	}

