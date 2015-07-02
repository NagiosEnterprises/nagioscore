/*****************************************************************************
 *
 * COMMANDS.C - External command functions for Nagios
 *
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
#include "../include/comments.h"
#include "../include/downtime.h"
#include "../include/statusdata.h"
#include "../include/perfdata.h"
#include "../include/sretention.h"
#include "../include/broker.h"
#include "../include/nagios.h"
#include "../include/workers.h"


extern int sigrestart;

static int command_file_fd;
static FILE *command_file_fp;
static int command_file_created = FALSE;

/* The command file worker process */
static struct {
	/* these must come first for check source detection */
	const char *type;
	const char *source_name;
	int pid;
	int sd;
	iocache *ioc;
} command_worker = { "command file", "command file worker", 0, 0, NULL };


/******************************************************************/
/************* EXTERNAL COMMAND WORKER CONTROLLERS ****************/
/******************************************************************/


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


/* shutdown command file worker thread */
int shutdown_command_file_worker(void) {
	if (!command_worker.pid)
		return 0;

	iocache_destroy(command_worker.ioc);
	command_worker.ioc = NULL;
	iobroker_close(nagios_iobs, command_worker.sd);
	command_worker.sd = -1;
	kill(command_worker.pid, SIGKILL);
	command_worker.pid = 0;
	return 0;
	}


static int command_input_handler(int sd, int events, void *discard) {
	int ret, cmd_ret;
	char *buf;
	unsigned long size;

	ret = iocache_read(command_worker.ioc, sd);
	log_debug_info(DEBUGL_COMMANDS, 2, "Read %d bytes from command worker\n", ret);
	if (ret == 0) {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "Command file worker seems to have died. Respawning\n");
		shutdown_command_file_worker();
		launch_command_file_worker();
		return 0;
		}
	while ((buf = iocache_use_delim(command_worker.ioc, "\n", 1, &size))) {
		if (buf[0] == '[') {
			/* raw external command */
			buf[size] = 0;
			log_debug_info(DEBUGL_COMMANDS, 1, "Read raw external command '%s'\n", buf);
			}

		/* Drop commands while restarting because we may not be ready to 
			handle them */
		if(TRUE == sigrestart) continue;

		if ((cmd_ret = process_external_command1(buf)) != CMD_ERROR_OK) {
			logit(NSLOG_EXTERNAL_COMMAND | NSLOG_RUNTIME_WARNING, TRUE, "External command error: %s\n", cmd_error_strerror(cmd_ret));
			}

		}
	return 0;
	}


/* main controller of command file helper process */
static int command_file_worker(int sd) {
	iocache *ioc;

	if (open_command_file() == ERROR)
		return (EXIT_FAILURE);

	ioc = iocache_create(65536);
	if (!ioc)
		exit(EXIT_FAILURE);

	while(1) {
		struct pollfd pfd;
		int pollval, ret;
		char *buf;
		unsigned long size;

		/* if our master has gone away, we need to die */
		if (kill(nagios_pid, 0) < 0 && errno == ESRCH) {
			return EXIT_SUCCESS;
			}

		errno = 0;
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
			/* @todo printf("Failed to poll() command file pipe: %m\n"); */
			if (errno == EINTR)
				continue;
			return EXIT_FAILURE;
			}

		errno = 0;
		ret = iocache_read(ioc, command_file_fd);
		if (ret < 1) {
			if (errno == EINTR)
				continue;
			return EXIT_FAILURE;
		}

		size = iocache_available(ioc);
		buf = iocache_use_size(ioc, size);
		ret = write(sd, buf, size);
		/*
		 * @todo Add libio to get io_write_all(), which handles
		 * EINTR and EAGAIN properly instead of just exiting.
		 */
		if (ret < 0 && errno != EINTR)
			return EXIT_FAILURE;
		} /* while(1) */
	}


int launch_command_file_worker(void) {
	int ret, sv[2];
	char *str;

	/*
	 * if we're restarting, we may well already have a command
	 * file worker process attached. Keep it if that's so.
	 */
	if (command_worker.pid && kill(command_worker.pid, 0) == 0 &&
		iobroker_is_registered(nagios_iobs, command_worker.sd))
	{
		return 0;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Failed to create socketpair for command file worker: %m\n");
		return ERROR;
	}

	command_worker.pid = fork();
	if (command_worker.pid < 0) {
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Failed to fork() command file worker: %m\n");
		goto err_close;
	}

	if (command_worker.pid) {
		command_worker.ioc = iocache_create(512 * 1024);
		if (!command_worker.ioc) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Failed to create I/O cache for command file worker: %m\n");
			goto err_close;
		}

		command_worker.sd = sv[0];
		ret = iobroker_register(nagios_iobs, command_worker.sd, NULL, command_input_handler);
		if (ret < 0) {
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Failed to register command file worker socket %d with io broker %p: %s; errno=%d: %s\n",
				  command_worker.sd, nagios_iobs, iobroker_strerror(ret), errno, strerror(errno));
			iocache_destroy(command_worker.ioc);
			goto err_close;
		}
		logit(NSLOG_INFO_MESSAGE, TRUE, "Successfully launched command file worker with pid %d\n",
			  command_worker.pid);
		return OK;
	}

	/* child goes here */
	close(sv[0]);

	/* make our own process-group so we can be traced into and stuff */
	setpgid(0, 0);

	/* we must preserve command_file before nuking memory */
	(void)chdir("/tmp");
	(void)chdir("nagios-cfw");
	str = strdup(command_file);
	free_memory(get_global_macros());
	command_file = str;
	exit(command_file_worker(sv[1]));

	/* error conditions for parent */
err_close:
	close(sv[0]);
	close(sv[1]);
	command_worker.pid = 0;
	command_worker.sd = -1;
	return ERROR;
	}

/******************************************************************/
/****************** EXTERNAL COMMAND PROCESSING *******************/
/******************************************************************/

/*** stupid helpers ****/
static host *find_host_by_name_or_address(const char *name)
{
	host *h;

	if ((h = find_host(name)) || !name)
		return h;

	for (h = host_list; h; h = h->next)
		if (!strcmp(h->address, name))
			return h;

	return NULL;
}

/* processes all external commands in a (regular) file */
int process_external_commands_from_file(char *fname, int delete_file) {
	mmapfile *thefile = NULL;
	char *input = NULL;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "process_external_commands_from_file()\n");

	if(fname == NULL)
		return ERROR;

	log_debug_info(DEBUGL_EXTERNALCOMMANDS, 1, "Processing commands from file '%s'.  File will %s deleted after processing.\n", fname, (delete_file == TRUE) ? "be" : "NOT be");

	/* open the config file for reading */
	if((thefile = mmap_fopen(fname)) == NULL) {
		logit(NSLOG_INFO_MESSAGE, FALSE, "Error: Cannot open file '%s' to process external commands!", fname);
		return ERROR;
		}

	/* process all commands in the file */
	while(1) {

		/* free memory */
		my_free(input);

		/* read the next line */
		if((input = mmap_fgets(thefile)) == NULL)
			break;

		/* process the command */
		process_external_command1(input);
		}

	/* close the file */
	mmap_fclose(thefile);

	/* delete the file */
	if(delete_file == TRUE)
		unlink(fname);

	return OK;
	}



/* top-level external command processor */
int process_external_command1(char *cmd) {
	char *temp_buffer = NULL;
	char *command_id = NULL;
	char *args = NULL;
	time_t entry_time = 0L;
	int command_type = CMD_NONE;
	char *temp_ptr = NULL;
	int external_command_ret = OK;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "process_external_command1()\n");

	if(cmd == NULL)
		return CMD_ERROR_MALFORMED_COMMAND;

	/* strip the command of newlines and carriage returns */
	strip(cmd);

	log_debug_info(DEBUGL_EXTERNALCOMMANDS, 2, "Raw command entry: %s\n", cmd);

	/* get the command entry time */
	if((temp_ptr = my_strtok(cmd, "[")) == NULL)
		return CMD_ERROR_MALFORMED_COMMAND;
	if((temp_ptr = my_strtok(NULL, "]")) == NULL)
		return CMD_ERROR_MALFORMED_COMMAND;
	entry_time = (time_t)strtoul(temp_ptr, NULL, 10);

	/* get the command identifier */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return CMD_ERROR_MALFORMED_COMMAND;
	if((command_id = (char *)strdup(temp_ptr + 1)) == NULL)
		return CMD_ERROR_INTERNAL_ERROR;

	/* get the command arguments */
	if((temp_ptr = my_strtok(NULL, "\n")) == NULL)
		args = (char *)strdup("");
	else
		args = (char *)strdup(temp_ptr);
	if(args == NULL) {
		my_free(command_id);
		return CMD_ERROR_INTERNAL_ERROR;
		}

	/* decide what type of command this is... */

	/**************************/
	/**** PROCESS COMMANDS ****/
	/**************************/

	if(!strcmp(command_id, "ENTER_STANDBY_MODE") || !strcmp(command_id, "DISABLE_NOTIFICATIONS"))
		command_type = CMD_DISABLE_NOTIFICATIONS;
	else if(!strcmp(command_id, "ENTER_ACTIVE_MODE") || !strcmp(command_id, "ENABLE_NOTIFICATIONS"))
		command_type = CMD_ENABLE_NOTIFICATIONS;

	else if(!strcmp(command_id, "SHUTDOWN_PROGRAM") || !strcmp(command_id, "SHUTDOWN_PROCESS"))
		command_type = CMD_SHUTDOWN_PROCESS;
	else if(!strcmp(command_id, "RESTART_PROGRAM") || !strcmp(command_id, "RESTART_PROCESS"))
		command_type = CMD_RESTART_PROCESS;

	else if(!strcmp(command_id, "SAVE_STATE_INFORMATION"))
		command_type = CMD_SAVE_STATE_INFORMATION;
	else if(!strcmp(command_id, "READ_STATE_INFORMATION"))
		command_type = CMD_READ_STATE_INFORMATION;

	else if(!strcmp(command_id, "ENABLE_EVENT_HANDLERS"))
		command_type = CMD_ENABLE_EVENT_HANDLERS;
	else if(!strcmp(command_id, "DISABLE_EVENT_HANDLERS"))
		command_type = CMD_DISABLE_EVENT_HANDLERS;

	else if(!strcmp(command_id, "ENABLE_PERFORMANCE_DATA"))
		command_type = CMD_ENABLE_PERFORMANCE_DATA;
	else if(!strcmp(command_id, "DISABLE_PERFORMANCE_DATA"))
		command_type = CMD_DISABLE_PERFORMANCE_DATA;

	else if(!strcmp(command_id, "START_EXECUTING_HOST_CHECKS"))
		command_type = CMD_START_EXECUTING_HOST_CHECKS;
	else if(!strcmp(command_id, "STOP_EXECUTING_HOST_CHECKS"))
		command_type = CMD_STOP_EXECUTING_HOST_CHECKS;

	else if(!strcmp(command_id, "START_EXECUTING_SVC_CHECKS"))
		command_type = CMD_START_EXECUTING_SVC_CHECKS;
	else if(!strcmp(command_id, "STOP_EXECUTING_SVC_CHECKS"))
		command_type = CMD_STOP_EXECUTING_SVC_CHECKS;

	else if(!strcmp(command_id, "START_ACCEPTING_PASSIVE_HOST_CHECKS"))
		command_type = CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS;
	else if(!strcmp(command_id, "STOP_ACCEPTING_PASSIVE_HOST_CHECKS"))
		command_type = CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS;

	else if(!strcmp(command_id, "START_ACCEPTING_PASSIVE_SVC_CHECKS"))
		command_type = CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS;
	else if(!strcmp(command_id, "STOP_ACCEPTING_PASSIVE_SVC_CHECKS"))
		command_type = CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS;

	else if(!strcmp(command_id, "START_OBSESSING_OVER_HOST_CHECKS"))
		command_type = CMD_START_OBSESSING_OVER_HOST_CHECKS;
	else if(!strcmp(command_id, "STOP_OBSESSING_OVER_HOST_CHECKS"))
		command_type = CMD_STOP_OBSESSING_OVER_HOST_CHECKS;

	else if(!strcmp(command_id, "START_OBSESSING_OVER_SVC_CHECKS"))
		command_type = CMD_START_OBSESSING_OVER_SVC_CHECKS;
	else if(!strcmp(command_id, "STOP_OBSESSING_OVER_SVC_CHECKS"))
		command_type = CMD_STOP_OBSESSING_OVER_SVC_CHECKS;

	else if(!strcmp(command_id, "ENABLE_FLAP_DETECTION"))
		command_type = CMD_ENABLE_FLAP_DETECTION;
	else if(!strcmp(command_id, "DISABLE_FLAP_DETECTION"))
		command_type = CMD_DISABLE_FLAP_DETECTION;

	else if(!strcmp(command_id, "CHANGE_GLOBAL_HOST_EVENT_HANDLER"))
		command_type = CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER;
	else if(!strcmp(command_id, "CHANGE_GLOBAL_SVC_EVENT_HANDLER"))
		command_type = CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER;

	else if(!strcmp(command_id, "ENABLE_SERVICE_FRESHNESS_CHECKS"))
		command_type = CMD_ENABLE_SERVICE_FRESHNESS_CHECKS;
	else if(!strcmp(command_id, "DISABLE_SERVICE_FRESHNESS_CHECKS"))
		command_type = CMD_DISABLE_SERVICE_FRESHNESS_CHECKS;

	else if(!strcmp(command_id, "ENABLE_HOST_FRESHNESS_CHECKS"))
		command_type = CMD_ENABLE_HOST_FRESHNESS_CHECKS;
	else if(!strcmp(command_id, "DISABLE_HOST_FRESHNESS_CHECKS"))
		command_type = CMD_DISABLE_HOST_FRESHNESS_CHECKS;


	/*******************************/
	/**** HOST-RELATED COMMANDS ****/
	/*******************************/

	else if(!strcmp(command_id, "ADD_HOST_COMMENT"))
		command_type = CMD_ADD_HOST_COMMENT;
	else if(!strcmp(command_id, "DEL_HOST_COMMENT"))
		command_type = CMD_DEL_HOST_COMMENT;
	else if(!strcmp(command_id, "DEL_ALL_HOST_COMMENTS"))
		command_type = CMD_DEL_ALL_HOST_COMMENTS;

	else if(!strcmp(command_id, "DELAY_HOST_NOTIFICATION"))
		command_type = CMD_DELAY_HOST_NOTIFICATION;

	else if(!strcmp(command_id, "ENABLE_HOST_NOTIFICATIONS"))
		command_type = CMD_ENABLE_HOST_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_HOST_NOTIFICATIONS"))
		command_type = CMD_DISABLE_HOST_NOTIFICATIONS;

	else if(!strcmp(command_id, "ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST"))
		command_type = CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST;
	else if(!strcmp(command_id, "DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST"))
		command_type = CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST;

	else if(!strcmp(command_id, "ENABLE_HOST_AND_CHILD_NOTIFICATIONS"))
		command_type = CMD_ENABLE_HOST_AND_CHILD_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_HOST_AND_CHILD_NOTIFICATIONS"))
		command_type = CMD_DISABLE_HOST_AND_CHILD_NOTIFICATIONS;

	else if(!strcmp(command_id, "ENABLE_HOST_SVC_NOTIFICATIONS"))
		command_type = CMD_ENABLE_HOST_SVC_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_HOST_SVC_NOTIFICATIONS"))
		command_type = CMD_DISABLE_HOST_SVC_NOTIFICATIONS;

	else if(!strcmp(command_id, "ENABLE_HOST_SVC_CHECKS"))
		command_type = CMD_ENABLE_HOST_SVC_CHECKS;
	else if(!strcmp(command_id, "DISABLE_HOST_SVC_CHECKS"))
		command_type = CMD_DISABLE_HOST_SVC_CHECKS;

	else if(!strcmp(command_id, "ENABLE_PASSIVE_HOST_CHECKS"))
		command_type = CMD_ENABLE_PASSIVE_HOST_CHECKS;
	else if(!strcmp(command_id, "DISABLE_PASSIVE_HOST_CHECKS"))
		command_type = CMD_DISABLE_PASSIVE_HOST_CHECKS;

	else if(!strcmp(command_id, "SCHEDULE_HOST_SVC_CHECKS"))
		command_type = CMD_SCHEDULE_HOST_SVC_CHECKS;
	else if(!strcmp(command_id, "SCHEDULE_FORCED_HOST_SVC_CHECKS"))
		command_type = CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS;

	else if(!strcmp(command_id, "ACKNOWLEDGE_HOST_PROBLEM"))
		command_type = CMD_ACKNOWLEDGE_HOST_PROBLEM;
	else if(!strcmp(command_id, "REMOVE_HOST_ACKNOWLEDGEMENT"))
		command_type = CMD_REMOVE_HOST_ACKNOWLEDGEMENT;

	else if(!strcmp(command_id, "ENABLE_HOST_EVENT_HANDLER"))
		command_type = CMD_ENABLE_HOST_EVENT_HANDLER;
	else if(!strcmp(command_id, "DISABLE_HOST_EVENT_HANDLER"))
		command_type = CMD_DISABLE_HOST_EVENT_HANDLER;

	else if(!strcmp(command_id, "ENABLE_HOST_CHECK"))
		command_type = CMD_ENABLE_HOST_CHECK;
	else if(!strcmp(command_id, "DISABLE_HOST_CHECK"))
		command_type = CMD_DISABLE_HOST_CHECK;

	else if(!strcmp(command_id, "SCHEDULE_HOST_CHECK"))
		command_type = CMD_SCHEDULE_HOST_CHECK;
	else if(!strcmp(command_id, "SCHEDULE_FORCED_HOST_CHECK"))
		command_type = CMD_SCHEDULE_FORCED_HOST_CHECK;

	else if(!strcmp(command_id, "SCHEDULE_HOST_DOWNTIME"))
		command_type = CMD_SCHEDULE_HOST_DOWNTIME;
	else if(!strcmp(command_id, "SCHEDULE_HOST_SVC_DOWNTIME"))
		command_type = CMD_SCHEDULE_HOST_SVC_DOWNTIME;
	else if(!strcmp(command_id, "DEL_HOST_DOWNTIME"))
		command_type = CMD_DEL_HOST_DOWNTIME;
	else if(!strcmp(command_id, "DEL_DOWNTIME_BY_HOST_NAME"))
		command_type = CMD_DEL_DOWNTIME_BY_HOST_NAME;
	else if(!strcmp(command_id, "DEL_DOWNTIME_BY_HOSTGROUP_NAME"))
		command_type = CMD_DEL_DOWNTIME_BY_HOSTGROUP_NAME;
	else if(!strcmp(command_id, "DEL_DOWNTIME_BY_START_TIME_COMMENT"))
		command_type = CMD_DEL_DOWNTIME_BY_START_TIME_COMMENT;

	else if(!strcmp(command_id, "ENABLE_HOST_FLAP_DETECTION"))
		command_type = CMD_ENABLE_HOST_FLAP_DETECTION;
	else if(!strcmp(command_id, "DISABLE_HOST_FLAP_DETECTION"))
		command_type = CMD_DISABLE_HOST_FLAP_DETECTION;

	else if(!strcmp(command_id, "START_OBSESSING_OVER_HOST"))
		command_type = CMD_START_OBSESSING_OVER_HOST;
	else if(!strcmp(command_id, "STOP_OBSESSING_OVER_HOST"))
		command_type = CMD_STOP_OBSESSING_OVER_HOST;

	else if(!strcmp(command_id, "CHANGE_HOST_EVENT_HANDLER"))
		command_type = CMD_CHANGE_HOST_EVENT_HANDLER;
	else if(!strcmp(command_id, "CHANGE_HOST_CHECK_COMMAND"))
		command_type = CMD_CHANGE_HOST_CHECK_COMMAND;

	else if(!strcmp(command_id, "CHANGE_NORMAL_HOST_CHECK_INTERVAL"))
		command_type = CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL;
	else if(!strcmp(command_id, "CHANGE_RETRY_HOST_CHECK_INTERVAL"))
		command_type = CMD_CHANGE_RETRY_HOST_CHECK_INTERVAL;

	else if(!strcmp(command_id, "CHANGE_MAX_HOST_CHECK_ATTEMPTS"))
		command_type = CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS;

	else if(!strcmp(command_id, "SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME"))
		command_type = CMD_SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME;

	else if(!strcmp(command_id, "SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME"))
		command_type = CMD_SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME;

	else if(!strcmp(command_id, "SET_HOST_NOTIFICATION_NUMBER"))
		command_type = CMD_SET_HOST_NOTIFICATION_NUMBER;

	else if(!strcmp(command_id, "CHANGE_HOST_CHECK_TIMEPERIOD"))
		command_type = CMD_CHANGE_HOST_CHECK_TIMEPERIOD;

	else if(!strcmp(command_id, "CHANGE_CUSTOM_HOST_VAR"))
		command_type = CMD_CHANGE_CUSTOM_HOST_VAR;

	else if(!strcmp(command_id, "SEND_CUSTOM_HOST_NOTIFICATION"))
		command_type = CMD_SEND_CUSTOM_HOST_NOTIFICATION;

	else if(!strcmp(command_id, "CHANGE_HOST_NOTIFICATION_TIMEPERIOD"))
		command_type = CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD;

	else if(!strcmp(command_id, "CHANGE_HOST_MODATTR"))
		command_type = CMD_CHANGE_HOST_MODATTR;


	/************************************/
	/**** HOSTGROUP-RELATED COMMANDS ****/
	/************************************/

	else if(!strcmp(command_id, "ENABLE_HOSTGROUP_HOST_NOTIFICATIONS"))
		command_type = CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_HOSTGROUP_HOST_NOTIFICATIONS"))
		command_type = CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS;

	else if(!strcmp(command_id, "ENABLE_HOSTGROUP_SVC_NOTIFICATIONS"))
		command_type = CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_HOSTGROUP_SVC_NOTIFICATIONS"))
		command_type = CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS;

	else if(!strcmp(command_id, "ENABLE_HOSTGROUP_HOST_CHECKS"))
		command_type = CMD_ENABLE_HOSTGROUP_HOST_CHECKS;
	else if(!strcmp(command_id, "DISABLE_HOSTGROUP_HOST_CHECKS"))
		command_type = CMD_DISABLE_HOSTGROUP_HOST_CHECKS;

	else if(!strcmp(command_id, "ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS"))
		command_type = CMD_ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS;
	else if(!strcmp(command_id, "DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS"))
		command_type = CMD_DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS;

	else if(!strcmp(command_id, "ENABLE_HOSTGROUP_SVC_CHECKS"))
		command_type = CMD_ENABLE_HOSTGROUP_SVC_CHECKS;
	else if(!strcmp(command_id, "DISABLE_HOSTGROUP_SVC_CHECKS"))
		command_type = CMD_DISABLE_HOSTGROUP_SVC_CHECKS;

	else if(!strcmp(command_id, "ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS"))
		command_type = CMD_ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS;
	else if(!strcmp(command_id, "DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS"))
		command_type = CMD_DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS;

	else if(!strcmp(command_id, "SCHEDULE_HOSTGROUP_HOST_DOWNTIME"))
		command_type = CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME;
	else if(!strcmp(command_id, "SCHEDULE_HOSTGROUP_SVC_DOWNTIME"))
		command_type = CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME;


	/**********************************/
	/**** SERVICE-RELATED COMMANDS ****/
	/**********************************/

	else if(!strcmp(command_id, "ADD_SVC_COMMENT"))
		command_type = CMD_ADD_SVC_COMMENT;
	else if(!strcmp(command_id, "DEL_SVC_COMMENT"))
		command_type = CMD_DEL_SVC_COMMENT;
	else if(!strcmp(command_id, "DEL_ALL_SVC_COMMENTS"))
		command_type = CMD_DEL_ALL_SVC_COMMENTS;

	else if(!strcmp(command_id, "SCHEDULE_SVC_CHECK"))
		command_type = CMD_SCHEDULE_SVC_CHECK;
	else if(!strcmp(command_id, "SCHEDULE_FORCED_SVC_CHECK"))
		command_type = CMD_SCHEDULE_FORCED_SVC_CHECK;

	else if(!strcmp(command_id, "ENABLE_SVC_CHECK"))
		command_type = CMD_ENABLE_SVC_CHECK;
	else if(!strcmp(command_id, "DISABLE_SVC_CHECK"))
		command_type = CMD_DISABLE_SVC_CHECK;

	else if(!strcmp(command_id, "ENABLE_PASSIVE_SVC_CHECKS"))
		command_type = CMD_ENABLE_PASSIVE_SVC_CHECKS;
	else if(!strcmp(command_id, "DISABLE_PASSIVE_SVC_CHECKS"))
		command_type = CMD_DISABLE_PASSIVE_SVC_CHECKS;

	else if(!strcmp(command_id, "DELAY_SVC_NOTIFICATION"))
		command_type = CMD_DELAY_SVC_NOTIFICATION;
	else if(!strcmp(command_id, "ENABLE_SVC_NOTIFICATIONS"))
		command_type = CMD_ENABLE_SVC_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_SVC_NOTIFICATIONS"))
		command_type = CMD_DISABLE_SVC_NOTIFICATIONS;

	else if(!strcmp(command_id, "PROCESS_SERVICE_CHECK_RESULT"))
		command_type = CMD_PROCESS_SERVICE_CHECK_RESULT;
	else if(!strcmp(command_id, "PROCESS_HOST_CHECK_RESULT"))
		command_type = CMD_PROCESS_HOST_CHECK_RESULT;

	else if(!strcmp(command_id, "ENABLE_SVC_EVENT_HANDLER"))
		command_type = CMD_ENABLE_SVC_EVENT_HANDLER;
	else if(!strcmp(command_id, "DISABLE_SVC_EVENT_HANDLER"))
		command_type = CMD_DISABLE_SVC_EVENT_HANDLER;

	else if(!strcmp(command_id, "ENABLE_SVC_FLAP_DETECTION"))
		command_type = CMD_ENABLE_SVC_FLAP_DETECTION;
	else if(!strcmp(command_id, "DISABLE_SVC_FLAP_DETECTION"))
		command_type = CMD_DISABLE_SVC_FLAP_DETECTION;

	else if(!strcmp(command_id, "SCHEDULE_SVC_DOWNTIME"))
		command_type = CMD_SCHEDULE_SVC_DOWNTIME;
	else if(!strcmp(command_id, "DEL_SVC_DOWNTIME"))
		command_type = CMD_DEL_SVC_DOWNTIME;

	else if(!strcmp(command_id, "ACKNOWLEDGE_SVC_PROBLEM"))
		command_type = CMD_ACKNOWLEDGE_SVC_PROBLEM;
	else if(!strcmp(command_id, "REMOVE_SVC_ACKNOWLEDGEMENT"))
		command_type = CMD_REMOVE_SVC_ACKNOWLEDGEMENT;

	else if(!strcmp(command_id, "START_OBSESSING_OVER_SVC"))
		command_type = CMD_START_OBSESSING_OVER_SVC;
	else if(!strcmp(command_id, "STOP_OBSESSING_OVER_SVC"))
		command_type = CMD_STOP_OBSESSING_OVER_SVC;

	else if(!strcmp(command_id, "CHANGE_SVC_EVENT_HANDLER"))
		command_type = CMD_CHANGE_SVC_EVENT_HANDLER;
	else if(!strcmp(command_id, "CHANGE_SVC_CHECK_COMMAND"))
		command_type = CMD_CHANGE_SVC_CHECK_COMMAND;

	else if(!strcmp(command_id, "CHANGE_NORMAL_SVC_CHECK_INTERVAL"))
		command_type = CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL;
	else if(!strcmp(command_id, "CHANGE_RETRY_SVC_CHECK_INTERVAL"))
		command_type = CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL;

	else if(!strcmp(command_id, "CHANGE_MAX_SVC_CHECK_ATTEMPTS"))
		command_type = CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS;

	else if(!strcmp(command_id, "SET_SVC_NOTIFICATION_NUMBER"))
		command_type = CMD_SET_SVC_NOTIFICATION_NUMBER;

	else if(!strcmp(command_id, "CHANGE_SVC_CHECK_TIMEPERIOD"))
		command_type = CMD_CHANGE_SVC_CHECK_TIMEPERIOD;

	else if(!strcmp(command_id, "CHANGE_CUSTOM_SVC_VAR"))
		command_type = CMD_CHANGE_CUSTOM_SVC_VAR;

	else if(!strcmp(command_id, "CHANGE_CUSTOM_CONTACT_VAR"))
		command_type = CMD_CHANGE_CUSTOM_CONTACT_VAR;

	else if(!strcmp(command_id, "SEND_CUSTOM_SVC_NOTIFICATION"))
		command_type = CMD_SEND_CUSTOM_SVC_NOTIFICATION;

	else if(!strcmp(command_id, "CHANGE_SVC_NOTIFICATION_TIMEPERIOD"))
		command_type = CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD;

	else if(!strcmp(command_id, "CHANGE_SVC_MODATTR"))
		command_type = CMD_CHANGE_SVC_MODATTR;


	/***************************************/
	/**** SERVICEGROUP-RELATED COMMANDS ****/
	/***************************************/

	else if(!strcmp(command_id, "ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS"))
		command_type = CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS"))
		command_type = CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS;

	else if(!strcmp(command_id, "ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS"))
		command_type = CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS"))
		command_type = CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS;

	else if(!strcmp(command_id, "ENABLE_SERVICEGROUP_HOST_CHECKS"))
		command_type = CMD_ENABLE_SERVICEGROUP_HOST_CHECKS;
	else if(!strcmp(command_id, "DISABLE_SERVICEGROUP_HOST_CHECKS"))
		command_type = CMD_DISABLE_SERVICEGROUP_HOST_CHECKS;

	else if(!strcmp(command_id, "ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS"))
		command_type = CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS;
	else if(!strcmp(command_id, "DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS"))
		command_type = CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS;

	else if(!strcmp(command_id, "ENABLE_SERVICEGROUP_SVC_CHECKS"))
		command_type = CMD_ENABLE_SERVICEGROUP_SVC_CHECKS;
	else if(!strcmp(command_id, "DISABLE_SERVICEGROUP_SVC_CHECKS"))
		command_type = CMD_DISABLE_SERVICEGROUP_SVC_CHECKS;

	else if(!strcmp(command_id, "ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS"))
		command_type = CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS;
	else if(!strcmp(command_id, "DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS"))
		command_type = CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS;

	else if(!strcmp(command_id, "SCHEDULE_SERVICEGROUP_HOST_DOWNTIME"))
		command_type = CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME;
	else if(!strcmp(command_id, "SCHEDULE_SERVICEGROUP_SVC_DOWNTIME"))
		command_type = CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME;


	/**********************************/
	/**** CONTACT-RELATED COMMANDS ****/
	/**********************************/

	else if(!strcmp(command_id, "ENABLE_CONTACT_HOST_NOTIFICATIONS"))
		command_type = CMD_ENABLE_CONTACT_HOST_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_CONTACT_HOST_NOTIFICATIONS"))
		command_type = CMD_DISABLE_CONTACT_HOST_NOTIFICATIONS;

	else if(!strcmp(command_id, "ENABLE_CONTACT_SVC_NOTIFICATIONS"))
		command_type = CMD_ENABLE_CONTACT_SVC_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_CONTACT_SVC_NOTIFICATIONS"))
		command_type = CMD_DISABLE_CONTACT_SVC_NOTIFICATIONS;

	else if(!strcmp(command_id, "CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD"))
		command_type = CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD;

	else if(!strcmp(command_id, "CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD"))
		command_type = CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD;

	else if(!strcmp(command_id, "CHANGE_CONTACT_MODATTR"))
		command_type = CMD_CHANGE_CONTACT_MODATTR;
	else if(!strcmp(command_id, "CHANGE_CONTACT_MODHATTR"))
		command_type = CMD_CHANGE_CONTACT_MODHATTR;
	else if(!strcmp(command_id, "CHANGE_CONTACT_MODSATTR"))
		command_type = CMD_CHANGE_CONTACT_MODSATTR;

	/***************************************/
	/**** CONTACTGROUP-RELATED COMMANDS ****/
	/***************************************/

	else if(!strcmp(command_id, "ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS"))
		command_type = CMD_ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS"))
		command_type = CMD_DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS;

	else if(!strcmp(command_id, "ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS"))
		command_type = CMD_ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS;
	else if(!strcmp(command_id, "DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS"))
		command_type = CMD_DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS;


	/**************************/
	/****** MISC COMMANDS *****/
	/**************************/

	else if(!strcmp(command_id, "PROCESS_FILE"))
		command_type = CMD_PROCESS_FILE;



	/****************************/
	/****** CUSTOM COMMANDS *****/
	/****************************/

	else if(command_id[0] == '_')
		command_type = CMD_CUSTOM_COMMAND;



	/**** UNKNOWN COMMAND ****/
	else {
		/* log the bad external command */
		logit(NSLOG_EXTERNAL_COMMAND | NSLOG_RUNTIME_WARNING, TRUE, "Warning: Unrecognized external command -> %s;%s\n", command_id, args);

		/* free memory */
		my_free(command_id);
		my_free(args);

		return CMD_ERROR_UNKNOWN_COMMAND;
		}

	/* update statistics for external commands */
	update_check_stats(EXTERNAL_COMMAND_STATS, time(NULL));

	/* log the external command */
	asprintf(&temp_buffer, "EXTERNAL COMMAND: %s;%s\n", command_id, args);
	if(command_type == CMD_PROCESS_SERVICE_CHECK_RESULT || command_type == CMD_PROCESS_HOST_CHECK_RESULT) {
		/* passive checks are logged in checks.c as well, as some my bypass external commands by getting dropped in checkresults dir */
		if(log_passive_checks == TRUE)
			write_to_all_logs(temp_buffer, NSLOG_PASSIVE_CHECK);
		}
	else {
		if(log_external_commands == TRUE)
			write_to_all_logs(temp_buffer, NSLOG_EXTERNAL_COMMAND);
		}
	my_free(temp_buffer);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_external_command(NEBTYPE_EXTERNALCOMMAND_START, NEBFLAG_NONE, NEBATTR_NONE, command_type, entry_time, command_id, args, NULL);
#endif

	/* process the command */
	external_command_ret = (process_external_command2(command_type, entry_time, args) == OK) ? CMD_ERROR_OK : CMD_ERROR_FAILURE;
	if (external_command_ret != CMD_ERROR_OK) {
			logit(NSLOG_EXTERNAL_COMMAND | NSLOG_RUNTIME_WARNING, TRUE, "Error: External command failed -> %s;%s\n", command_id, args);
	}


#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_external_command(NEBTYPE_EXTERNALCOMMAND_END, NEBFLAG_NONE, NEBATTR_NONE, command_type, entry_time, command_id, args, NULL);
#endif

	/* free memory */
	my_free(command_id);
	my_free(args);

	return external_command_ret;
	}

const char *cmd_error_strerror(int code) {
	switch(code) {
		case CMD_ERROR_OK:
			return "No error";
		case CMD_ERROR_FAILURE:
			return "Command failed";
		case CMD_ERROR_INTERNAL_ERROR:
			return "Internal error";
		case CMD_ERROR_UNKNOWN_COMMAND:
			return "Unknown or unsupported command";
		case CMD_ERROR_MALFORMED_COMMAND:
			return "Malformed command";
		}
	return "Unknown error";
	}

/* top-level processor for a single external command */
int process_external_command2(int cmd, time_t entry_time, char *args) {

	int ret = OK;
	log_debug_info(DEBUGL_FUNCTIONS, 0, "process_external_command2()\n");

	log_debug_info(DEBUGL_EXTERNALCOMMANDS, 1, "External Command Type: %d\n", cmd);
	log_debug_info(DEBUGL_EXTERNALCOMMANDS, 1, "Command Entry Time: %lu\n", (unsigned long)entry_time);
	log_debug_info(DEBUGL_EXTERNALCOMMANDS, 1, "Command Arguments: %s\n", (args == NULL) ? "" : args);

	/* how shall we execute the command? */
	switch(cmd) {

			/***************************/
			/***** SYSTEM COMMANDS *****/
			/***************************/

		case CMD_SHUTDOWN_PROCESS:
		case CMD_RESTART_PROCESS:
			ret = cmd_signal_process(cmd, args);
			break;

		case CMD_SAVE_STATE_INFORMATION:
			ret = save_state_information(FALSE);
			break;

		case CMD_READ_STATE_INFORMATION:
			ret = read_initial_state_information();
			break;

		case CMD_ENABLE_NOTIFICATIONS:
			enable_all_notifications();
			break;

		case CMD_DISABLE_NOTIFICATIONS:
			disable_all_notifications();
			break;

		case CMD_START_EXECUTING_SVC_CHECKS:
			start_executing_service_checks();
			break;

		case CMD_STOP_EXECUTING_SVC_CHECKS:
			stop_executing_service_checks();
			break;

		case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
			start_accepting_passive_service_checks();
			break;

		case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
			stop_accepting_passive_service_checks();
			break;

		case CMD_START_OBSESSING_OVER_SVC_CHECKS:
			start_obsessing_over_service_checks();
			break;

		case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
			stop_obsessing_over_service_checks();
			break;

		case CMD_START_EXECUTING_HOST_CHECKS:
			start_executing_host_checks();
			break;

		case CMD_STOP_EXECUTING_HOST_CHECKS:
			stop_executing_host_checks();
			break;

		case CMD_START_ACCEPTING_PASSIVE_HOST_CHECKS:
			start_accepting_passive_host_checks();
			break;

		case CMD_STOP_ACCEPTING_PASSIVE_HOST_CHECKS:
			stop_accepting_passive_host_checks();
			break;

		case CMD_START_OBSESSING_OVER_HOST_CHECKS:
			start_obsessing_over_host_checks();
			break;

		case CMD_STOP_OBSESSING_OVER_HOST_CHECKS:
			stop_obsessing_over_host_checks();
			break;

		case CMD_ENABLE_EVENT_HANDLERS:
			start_using_event_handlers();
			break;

		case CMD_DISABLE_EVENT_HANDLERS:
			stop_using_event_handlers();
			break;

		case CMD_ENABLE_FLAP_DETECTION:
			enable_flap_detection_routines();
			break;

		case CMD_DISABLE_FLAP_DETECTION:
			disable_flap_detection_routines();
			break;

		case CMD_ENABLE_SERVICE_FRESHNESS_CHECKS:
			enable_service_freshness_checks();
			break;

		case CMD_DISABLE_SERVICE_FRESHNESS_CHECKS:
			disable_service_freshness_checks();
			break;

		case CMD_ENABLE_HOST_FRESHNESS_CHECKS:
			enable_host_freshness_checks();
			break;

		case CMD_DISABLE_HOST_FRESHNESS_CHECKS:
			disable_host_freshness_checks();
			break;

		case CMD_ENABLE_PERFORMANCE_DATA:
			enable_performance_data();
			break;

		case CMD_DISABLE_PERFORMANCE_DATA:
			disable_performance_data();
			break;


			/***************************/
			/*****  HOST COMMANDS  *****/
			/***************************/

		case CMD_ENABLE_HOST_CHECK:
		case CMD_DISABLE_HOST_CHECK:
		case CMD_ENABLE_PASSIVE_HOST_CHECKS:
		case CMD_DISABLE_PASSIVE_HOST_CHECKS:
		case CMD_ENABLE_HOST_SVC_CHECKS:
		case CMD_DISABLE_HOST_SVC_CHECKS:
		case CMD_ENABLE_HOST_NOTIFICATIONS:
		case CMD_DISABLE_HOST_NOTIFICATIONS:
		case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		case CMD_ENABLE_HOST_AND_CHILD_NOTIFICATIONS:
		case CMD_DISABLE_HOST_AND_CHILD_NOTIFICATIONS:
		case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
		case CMD_ENABLE_HOST_FLAP_DETECTION:
		case CMD_DISABLE_HOST_FLAP_DETECTION:
		case CMD_ENABLE_HOST_EVENT_HANDLER:
		case CMD_DISABLE_HOST_EVENT_HANDLER:
		case CMD_START_OBSESSING_OVER_HOST:
		case CMD_STOP_OBSESSING_OVER_HOST:
		case CMD_SET_HOST_NOTIFICATION_NUMBER:
		case CMD_SEND_CUSTOM_HOST_NOTIFICATION:
			ret = process_host_command(cmd, entry_time, args);
			break;


			/*****************************/
			/***** HOSTGROUP COMMANDS ****/
			/*****************************/

		case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		case CMD_ENABLE_HOSTGROUP_HOST_CHECKS:
		case CMD_DISABLE_HOSTGROUP_HOST_CHECKS:
		case CMD_ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
		case CMD_DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
		case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
		case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
		case CMD_ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
		case CMD_DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
			ret = process_hostgroup_command(cmd, entry_time, args);
			break;


			/***************************/
			/***** SERVICE COMMANDS ****/
			/***************************/

		case CMD_ENABLE_SVC_CHECK:
		case CMD_DISABLE_SVC_CHECK:
		case CMD_ENABLE_PASSIVE_SVC_CHECKS:
		case CMD_DISABLE_PASSIVE_SVC_CHECKS:
		case CMD_ENABLE_SVC_NOTIFICATIONS:
		case CMD_DISABLE_SVC_NOTIFICATIONS:
		case CMD_ENABLE_SVC_FLAP_DETECTION:
		case CMD_DISABLE_SVC_FLAP_DETECTION:
		case CMD_ENABLE_SVC_EVENT_HANDLER:
		case CMD_DISABLE_SVC_EVENT_HANDLER:
		case CMD_START_OBSESSING_OVER_SVC:
		case CMD_STOP_OBSESSING_OVER_SVC:
		case CMD_SET_SVC_NOTIFICATION_NUMBER:
		case CMD_SEND_CUSTOM_SVC_NOTIFICATION:
			ret = process_service_command(cmd, entry_time, args);
			break;


			/********************************/
			/***** SERVICEGROUP COMMANDS ****/
			/********************************/

		case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
		case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
		case CMD_ENABLE_SERVICEGROUP_HOST_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_HOST_CHECKS:
		case CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
		case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
		case CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
			ret = process_servicegroup_command(cmd, entry_time, args);
			break;


			/**********************************/
			/**** CONTACT-RELATED COMMANDS ****/
			/**********************************/

		case CMD_ENABLE_CONTACT_HOST_NOTIFICATIONS:
		case CMD_DISABLE_CONTACT_HOST_NOTIFICATIONS:
		case CMD_ENABLE_CONTACT_SVC_NOTIFICATIONS:
		case CMD_DISABLE_CONTACT_SVC_NOTIFICATIONS:
			ret = process_contact_command(cmd, entry_time, args);
			break;


			/***************************************/
			/**** CONTACTGROUP-RELATED COMMANDS ****/
			/***************************************/

		case CMD_ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
		case CMD_ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS:
			ret = process_contactgroup_command(cmd, entry_time, args);
			break;


			/***************************/
			/**** UNSORTED COMMANDS ****/
			/***************************/


		case CMD_ADD_HOST_COMMENT:
		case CMD_ADD_SVC_COMMENT:
			ret= cmd_add_comment(cmd, entry_time, args);
			break;

		case CMD_DEL_HOST_COMMENT:
		case CMD_DEL_SVC_COMMENT:
			ret = cmd_delete_comment(cmd, args);
			break;

		case CMD_DELAY_HOST_NOTIFICATION:
		case CMD_DELAY_SVC_NOTIFICATION:
			ret = cmd_delay_notification(cmd, args);
			break;

		case CMD_SCHEDULE_SVC_CHECK:
		case CMD_SCHEDULE_FORCED_SVC_CHECK:
			ret =cmd_schedule_check(cmd, args);
			break;

		case CMD_SCHEDULE_HOST_SVC_CHECKS:
		case CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS:
			ret = cmd_schedule_check(cmd, args);
			break;

		case CMD_DEL_ALL_HOST_COMMENTS:
		case CMD_DEL_ALL_SVC_COMMENTS:
			ret = cmd_delete_all_comments(cmd, args);
			break;

		case CMD_PROCESS_SERVICE_CHECK_RESULT:
			ret = cmd_process_service_check_result(cmd, entry_time, args);
			break;

		case CMD_PROCESS_HOST_CHECK_RESULT:
			ret = cmd_process_host_check_result(cmd, entry_time, args);
			break;

		case CMD_ACKNOWLEDGE_HOST_PROBLEM:
		case CMD_ACKNOWLEDGE_SVC_PROBLEM:
			ret = cmd_acknowledge_problem(cmd, args);
			break;

		case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
		case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
			ret = cmd_remove_acknowledgement(cmd, args);
			break;

		case CMD_SCHEDULE_HOST_DOWNTIME:
		case CMD_SCHEDULE_SVC_DOWNTIME:
		case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
		case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
		case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
		case CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME:
		case CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME:
		case CMD_SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME:
		case CMD_SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME:
			ret = cmd_schedule_downtime(cmd, entry_time, args);
			break;

		case CMD_DEL_HOST_DOWNTIME:
		case CMD_DEL_SVC_DOWNTIME:
			ret = cmd_delete_downtime(cmd, args);
			break;

		case CMD_DEL_DOWNTIME_BY_HOST_NAME:
			ret = cmd_delete_downtime_by_host_name(cmd, args);
			break;

		case CMD_DEL_DOWNTIME_BY_HOSTGROUP_NAME:
			ret = cmd_delete_downtime_by_hostgroup_name(cmd, args);
			break;

		case CMD_DEL_DOWNTIME_BY_START_TIME_COMMENT:
			ret = cmd_delete_downtime_by_start_time_comment(cmd, args);
			break;

		case CMD_SCHEDULE_HOST_CHECK:
		case CMD_SCHEDULE_FORCED_HOST_CHECK:
			ret = cmd_schedule_check(cmd, args);
			break;

		case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
		case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:
		case CMD_CHANGE_HOST_EVENT_HANDLER:
		case CMD_CHANGE_SVC_EVENT_HANDLER:
		case CMD_CHANGE_HOST_CHECK_COMMAND:
		case CMD_CHANGE_SVC_CHECK_COMMAND:
		case CMD_CHANGE_HOST_CHECK_TIMEPERIOD:
		case CMD_CHANGE_SVC_CHECK_TIMEPERIOD:
		case CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD:
		case CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD:
		case CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD:
		case CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD:
			ret = cmd_change_object_char_var(cmd, args);
			break;

		case CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL:
		case CMD_CHANGE_RETRY_HOST_CHECK_INTERVAL:
		case CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL:
		case CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL:
		case CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS:
		case CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS:
		case CMD_CHANGE_HOST_MODATTR:
		case CMD_CHANGE_SVC_MODATTR:
		case CMD_CHANGE_CONTACT_MODATTR:
		case CMD_CHANGE_CONTACT_MODHATTR:
		case CMD_CHANGE_CONTACT_MODSATTR:
			ret = cmd_change_object_int_var(cmd, args);
			break;

		case CMD_CHANGE_CUSTOM_HOST_VAR:
		case CMD_CHANGE_CUSTOM_SVC_VAR:
		case CMD_CHANGE_CUSTOM_CONTACT_VAR:
			ret = cmd_change_object_custom_var(cmd, args);
			break;


			/***********************/
			/**** MISC COMMANDS ****/
			/***********************/


		case CMD_PROCESS_FILE:
			ret = cmd_process_external_commands_from_file(cmd, args);
			break;


			/*************************/
			/**** CUSTOM COMMANDS ****/
			/*************************/


		case CMD_CUSTOM_COMMAND:
			/* custom commands aren't handled internally by Nagios, but may be by NEB modules */
			break;

		default:
			return CMD_ERROR_UNKNOWN_COMMAND;
			break;
		}

	return ret;
	}


/* processes an external host command */
int process_host_command(int cmd, time_t entry_time, char *args) {
	char *host_name = NULL;
	host *temp_host = NULL;
	service *temp_service = NULL;
	servicesmember *temp_servicesmember = NULL;
	char *str = NULL;
	char *buf[2] = {NULL, NULL};
	int intval = 0;

	printf("ARGS: %s\n", args);

	/* get the host name */
	if((host_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* find the host */
	if((temp_host = find_host(host_name)) == NULL)
		return ERROR;

	switch(cmd) {

		case CMD_ENABLE_HOST_NOTIFICATIONS:
			enable_host_notifications(temp_host);
			break;

		case CMD_DISABLE_HOST_NOTIFICATIONS:
			disable_host_notifications(temp_host);
			break;

		case CMD_ENABLE_HOST_AND_CHILD_NOTIFICATIONS:
			enable_and_propagate_notifications(temp_host, 0, TRUE, TRUE, FALSE);
			break;

		case CMD_DISABLE_HOST_AND_CHILD_NOTIFICATIONS:
			disable_and_propagate_notifications(temp_host, 0, TRUE, TRUE, FALSE);
			break;

		case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
			enable_and_propagate_notifications(temp_host, 0, FALSE, TRUE, TRUE);
			break;

		case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
			disable_and_propagate_notifications(temp_host, 0, FALSE, TRUE, TRUE);
			break;

		case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
			for(temp_servicesmember = temp_host->services; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
				if((temp_service = temp_servicesmember->service_ptr) == NULL)
					continue;
				if(cmd == CMD_ENABLE_HOST_SVC_NOTIFICATIONS)
					enable_service_notifications(temp_service);
				else
					disable_service_notifications(temp_service);
				}
			break;

		case CMD_ENABLE_HOST_SVC_CHECKS:
		case CMD_DISABLE_HOST_SVC_CHECKS:
			for(temp_servicesmember = temp_host->services; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
				if((temp_service = temp_servicesmember->service_ptr) == NULL)
					continue;
				if(cmd == CMD_ENABLE_HOST_SVC_CHECKS)
					enable_service_checks(temp_service);
				else
					disable_service_checks(temp_service);
				}
			break;

		case CMD_ENABLE_HOST_CHECK:
			enable_host_checks(temp_host);
			break;

		case CMD_DISABLE_HOST_CHECK:
			disable_host_checks(temp_host);
			break;

		case CMD_ENABLE_HOST_EVENT_HANDLER:
			enable_host_event_handler(temp_host);
			break;

		case CMD_DISABLE_HOST_EVENT_HANDLER:
			disable_host_event_handler(temp_host);
			break;

		case CMD_ENABLE_HOST_FLAP_DETECTION:
			enable_host_flap_detection(temp_host);
			break;

		case CMD_DISABLE_HOST_FLAP_DETECTION:
			disable_host_flap_detection(temp_host);
			break;

		case CMD_ENABLE_PASSIVE_HOST_CHECKS:
			enable_passive_host_checks(temp_host);
			break;

		case CMD_DISABLE_PASSIVE_HOST_CHECKS:
			disable_passive_host_checks(temp_host);
			break;

		case CMD_START_OBSESSING_OVER_HOST:
			start_obsessing_over_host(temp_host);
			break;

		case CMD_STOP_OBSESSING_OVER_HOST:
			stop_obsessing_over_host(temp_host);
			break;

		case CMD_SET_HOST_NOTIFICATION_NUMBER:
			if((str = my_strtok(NULL, ";"))) {
				intval = atoi(str);
				set_host_notification_number(temp_host, intval);
				}
			break;

		case CMD_SEND_CUSTOM_HOST_NOTIFICATION:
			if((str = my_strtok(NULL, ";")))
				intval = atoi(str);
			str = my_strtok(NULL, ";");
			if(str)
				buf[0] = strdup(str);
			str = my_strtok(NULL, ";");
			if(str)
				buf[1] = strdup(str);
			if(buf[0] && buf[1])
				host_notification(temp_host, NOTIFICATION_CUSTOM, buf[0], buf[1], intval);
			break;

		default:
			break;
		}

	return OK;
	}


/* processes an external hostgroup command */
int process_hostgroup_command(int cmd, time_t entry_time, char *args) {
	char *hostgroup_name = NULL;
	hostgroup *temp_hostgroup = NULL;
	hostsmember *temp_member = NULL;
	host *temp_host = NULL;
	service *temp_service = NULL;
	servicesmember *temp_servicesmember = NULL;

	/* get the hostgroup name */
	if((hostgroup_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* find the hostgroup */
	if((temp_hostgroup = find_hostgroup(hostgroup_name)) == NULL)
		return ERROR;

	/* loop through all hosts in the hostgroup */
	for(temp_member = temp_hostgroup->members; temp_member != NULL; temp_member = temp_member->next) {

		if((temp_host = (host *)temp_member->host_ptr) == NULL)
			continue;

		switch(cmd) {

			case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
				enable_host_notifications(temp_host);
				break;

			case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
				disable_host_notifications(temp_host);
				break;

			case CMD_ENABLE_HOSTGROUP_HOST_CHECKS:
				enable_host_checks(temp_host);
				break;

			case CMD_DISABLE_HOSTGROUP_HOST_CHECKS:
				disable_host_checks(temp_host);
				break;

			case CMD_ENABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
				enable_passive_host_checks(temp_host);
				break;

			case CMD_DISABLE_HOSTGROUP_PASSIVE_HOST_CHECKS:
				disable_passive_host_checks(temp_host);
				break;

			default:

				/* loop through all services on the host */
				for(temp_servicesmember = temp_host->services; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
					if((temp_service = temp_servicesmember->service_ptr) == NULL)
						continue;

					switch(cmd) {

						case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
							enable_service_notifications(temp_service);
							break;

						case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
							disable_service_notifications(temp_service);
							break;

						case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
							enable_service_checks(temp_service);
							break;

						case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
							disable_service_checks(temp_service);
							break;

						case CMD_ENABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
							enable_passive_service_checks(temp_service);
							break;

						case CMD_DISABLE_HOSTGROUP_PASSIVE_SVC_CHECKS:
							disable_passive_service_checks(temp_service);
							break;

						default:
							break;
						}
					}

				break;
			}

		}

	return OK;
	}



/* processes an external service command */
int process_service_command(int cmd, time_t entry_time, char *args) {
	char *host_name = NULL;
	char *svc_description = NULL;
	service *temp_service = NULL;
	char *str = NULL;
	char *buf[2] = {NULL, NULL};
	int intval = 0;

	/* get the host name */
	if((host_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* get the service description */
	if((svc_description = my_strtok(NULL, ";")) == NULL)
		return ERROR;

	/* find the service */
	if((temp_service = find_service(host_name, svc_description)) == NULL)
		return ERROR;

	switch(cmd) {

		case CMD_ENABLE_SVC_NOTIFICATIONS:
			enable_service_notifications(temp_service);
			break;

		case CMD_DISABLE_SVC_NOTIFICATIONS:
			disable_service_notifications(temp_service);
			break;

		case CMD_ENABLE_SVC_CHECK:
			enable_service_checks(temp_service);
			break;

		case CMD_DISABLE_SVC_CHECK:
			disable_service_checks(temp_service);
			break;

		case CMD_ENABLE_SVC_EVENT_HANDLER:
			enable_service_event_handler(temp_service);
			break;

		case CMD_DISABLE_SVC_EVENT_HANDLER:
			disable_service_event_handler(temp_service);
			break;

		case CMD_ENABLE_SVC_FLAP_DETECTION:
			enable_service_flap_detection(temp_service);
			break;

		case CMD_DISABLE_SVC_FLAP_DETECTION:
			disable_service_flap_detection(temp_service);
			break;

		case CMD_ENABLE_PASSIVE_SVC_CHECKS:
			enable_passive_service_checks(temp_service);
			break;

		case CMD_DISABLE_PASSIVE_SVC_CHECKS:
			disable_passive_service_checks(temp_service);
			break;

		case CMD_START_OBSESSING_OVER_SVC:
			start_obsessing_over_service(temp_service);
			break;

		case CMD_STOP_OBSESSING_OVER_SVC:
			stop_obsessing_over_service(temp_service);
			break;

		case CMD_SET_SVC_NOTIFICATION_NUMBER:
			if((str = my_strtok(NULL, ";"))) {
				intval = atoi(str);
				set_service_notification_number(temp_service, intval);
				}
			break;

		case CMD_SEND_CUSTOM_SVC_NOTIFICATION:
			if((str = my_strtok(NULL, ";")))
				intval = atoi(str);
			str = my_strtok(NULL, ";");
			if(str)
				buf[0] = strdup(str);
			str = my_strtok(NULL, ";");
			if(str)
				buf[1] = strdup(str);
			if(buf[0] && buf[1])
				service_notification(temp_service, NOTIFICATION_CUSTOM, buf[0], buf[1], intval);
			break;

		default:
			break;
		}

	return OK;
	}


/* processes an external servicegroup command */
int process_servicegroup_command(int cmd, time_t entry_time, char *args) {
	char *servicegroup_name = NULL;
	servicegroup *temp_servicegroup = NULL;
	servicesmember *temp_member = NULL;
	host *temp_host = NULL;
	host *last_host = NULL;
	service *temp_service = NULL;

	/* get the servicegroup name */
	if((servicegroup_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* find the servicegroup */
	if((temp_servicegroup = find_servicegroup(servicegroup_name)) == NULL)
		return ERROR;

	switch(cmd) {

		case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
		case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
		case CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:

			/* loop through all servicegroup members */
			for(temp_member = temp_servicegroup->members; temp_member != NULL; temp_member = temp_member->next) {

				temp_service = find_service(temp_member->host_name, temp_member->service_description);
				if(temp_service == NULL)
					continue;

				switch(cmd) {

					case CMD_ENABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
						enable_service_notifications(temp_service);
						break;

					case CMD_DISABLE_SERVICEGROUP_SVC_NOTIFICATIONS:
						disable_service_notifications(temp_service);
						break;

					case CMD_ENABLE_SERVICEGROUP_SVC_CHECKS:
						enable_service_checks(temp_service);
						break;

					case CMD_DISABLE_SERVICEGROUP_SVC_CHECKS:
						disable_service_checks(temp_service);
						break;

					case CMD_ENABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
						enable_passive_service_checks(temp_service);
						break;

					case CMD_DISABLE_SERVICEGROUP_PASSIVE_SVC_CHECKS:
						disable_passive_service_checks(temp_service);
						break;

					default:
						break;
					}
				}

			break;

		case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
		case CMD_ENABLE_SERVICEGROUP_HOST_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_HOST_CHECKS:
		case CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
		case CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:

			/* loop through all hosts that have services belonging to the servicegroup */
			last_host = NULL;
			for(temp_member = temp_servicegroup->members; temp_member != NULL; temp_member = temp_member->next) {

				if((temp_host = find_host(temp_member->host_name)) == NULL)
					continue;

				if(temp_host == last_host)
					continue;

				switch(cmd) {

					case CMD_ENABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
						enable_host_notifications(temp_host);
						break;

					case CMD_DISABLE_SERVICEGROUP_HOST_NOTIFICATIONS:
						disable_host_notifications(temp_host);
						break;

					case CMD_ENABLE_SERVICEGROUP_HOST_CHECKS:
						enable_host_checks(temp_host);
						break;

					case CMD_DISABLE_SERVICEGROUP_HOST_CHECKS:
						disable_host_checks(temp_host);
						break;

					case CMD_ENABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
						enable_passive_host_checks(temp_host);
						break;

					case CMD_DISABLE_SERVICEGROUP_PASSIVE_HOST_CHECKS:
						disable_passive_host_checks(temp_host);
						break;

					default:
						break;
					}

				last_host = temp_host;
				}

			break;

		default:
			break;
		}

	return OK;
	}



/* processes an external contact command */
int process_contact_command(int cmd, time_t entry_time, char *args) {
	char *contact_name = NULL;
	contact *temp_contact = NULL;

	/* get the contact name */
	if((contact_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* find the contact */
	if((temp_contact = find_contact(contact_name)) == NULL)
		return ERROR;

	switch(cmd) {

		case CMD_ENABLE_CONTACT_HOST_NOTIFICATIONS:
			enable_contact_host_notifications(temp_contact);
			break;

		case CMD_DISABLE_CONTACT_HOST_NOTIFICATIONS:
			disable_contact_host_notifications(temp_contact);
			break;

		case CMD_ENABLE_CONTACT_SVC_NOTIFICATIONS:
			enable_contact_service_notifications(temp_contact);
			break;

		case CMD_DISABLE_CONTACT_SVC_NOTIFICATIONS:
			disable_contact_service_notifications(temp_contact);
			break;

		default:
			break;
		}

	return OK;
	}


/* processes an external contactgroup command */
int process_contactgroup_command(int cmd, time_t entry_time, char *args) {
	char *contactgroup_name = NULL;
	contactgroup *temp_contactgroup = NULL;
	contactsmember *temp_member = NULL;
	contact *temp_contact = NULL;

	/* get the contactgroup name */
	if((contactgroup_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* find the contactgroup */
	if((temp_contactgroup = find_contactgroup(contactgroup_name)) == NULL)
		return ERROR;

	switch(cmd) {

		case CMD_ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
		case CMD_ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS:

			/* loop through all contactgroup members */
			for(temp_member = temp_contactgroup->members; temp_member != NULL; temp_member = temp_member->next) {

				if((temp_contact = temp_member->contact_ptr) == NULL)
					continue;

				switch(cmd) {

					case CMD_ENABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
						enable_contact_host_notifications(temp_contact);
						break;

					case CMD_DISABLE_CONTACTGROUP_HOST_NOTIFICATIONS:
						disable_contact_host_notifications(temp_contact);
						break;

					case CMD_ENABLE_CONTACTGROUP_SVC_NOTIFICATIONS:
						enable_contact_service_notifications(temp_contact);
						break;

					case CMD_DISABLE_CONTACTGROUP_SVC_NOTIFICATIONS:
						disable_contact_service_notifications(temp_contact);
						break;

					default:
						break;
					}
				}

			break;

		default:
			break;
		}

	return OK;
	}



/******************************************************************/
/*************** EXTERNAL COMMAND IMPLEMENTATIONS  ****************/
/******************************************************************/

/* adds a host or service comment to the status log */
int cmd_add_comment(int cmd, time_t entry_time, char *args) {
	char *temp_ptr = NULL;
	host *temp_host = NULL;
	service *temp_service = NULL;
	char *host_name = NULL;
	char *svc_description = NULL;
	char *user = NULL;
	char *comment_data = NULL;
	int persistent = 0;
	int result = 0;

	/* get the host name */
	if((host_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* if we're adding a service comment...  */
	if(cmd == CMD_ADD_SVC_COMMENT) {

		/* get the service description */
		if((svc_description = my_strtok(NULL, ";")) == NULL)
			return ERROR;

		/* verify that the service is valid */
		if((temp_service = find_service(host_name, svc_description)) == NULL)
			return ERROR;
		}

	/* else verify that the host is valid */
	if((temp_host = find_host(host_name)) == NULL)
		return ERROR;

	/* get the persistent flag */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return ERROR;
	persistent = atoi(temp_ptr);
	if(persistent > 1)
		persistent = 1;
	else if(persistent < 0)
		persistent = 0;

	/* get the name of the user who entered the comment */
	if((user = my_strtok(NULL, ";")) == NULL)
		return ERROR;

	/* get the comment */
	if((comment_data = my_strtok(NULL, "\n")) == NULL)
		return ERROR;

	/* add the comment */
	result = add_new_comment((cmd == CMD_ADD_HOST_COMMENT) ? HOST_COMMENT : SERVICE_COMMENT, USER_COMMENT, host_name, svc_description, entry_time, user, comment_data, persistent, COMMENTSOURCE_EXTERNAL, FALSE, (time_t)0, NULL);

	if(result < 0)
		return ERROR;

	return OK;
	}



/* removes a host or service comment from the status log */
int cmd_delete_comment(int cmd, char *args) {
	unsigned long comment_id = 0L;

	/* get the comment id we should delete */
	if((comment_id = strtoul(args, NULL, 10)) == 0)
		return ERROR;

	/* delete the specified comment */
	if(cmd == CMD_DEL_HOST_COMMENT)
		delete_host_comment(comment_id);
	else
		delete_service_comment(comment_id);

	return OK;
	}



/* removes all comments associated with a host or service from the status log */
int cmd_delete_all_comments(int cmd, char *args) {
	service *temp_service = NULL;
	host *temp_host = NULL;
	char *host_name = NULL;
	char *svc_description = NULL;

	/* get the host name */
	if((host_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* if we're deleting service comments...  */
	if(cmd == CMD_DEL_ALL_SVC_COMMENTS) {

		/* get the service description */
		if((svc_description = my_strtok(NULL, ";")) == NULL)
			return ERROR;

		/* verify that the service is valid */
		if((temp_service = find_service(host_name, svc_description)) == NULL)
			return ERROR;
		}

	/* else verify that the host is valid */
	if((temp_host = find_host(host_name)) == NULL)
		return ERROR;

	/* delete comments */
	delete_all_comments((cmd == CMD_DEL_ALL_HOST_COMMENTS) ? HOST_COMMENT : SERVICE_COMMENT, host_name, svc_description);

	return OK;
	}



/* delays a host or service notification for given number of minutes */
int cmd_delay_notification(int cmd, char *args) {
	char *temp_ptr = NULL;
	host *temp_host = NULL;
	service *temp_service = NULL;
	char *host_name = NULL;
	char *svc_description = NULL;
	time_t delay_time = 0L;

	/* get the host name */
	if((host_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* if this is a service notification delay...  */
	if(cmd == CMD_DELAY_SVC_NOTIFICATION) {

		/* get the service description */
		if((svc_description = my_strtok(NULL, ";")) == NULL)
			return ERROR;

		/* verify that the service is valid */
		if((temp_service = find_service(host_name, svc_description)) == NULL)
			return ERROR;
		}

	/* else verify that the host is valid */
	else {

		if((temp_host = find_host(host_name)) == NULL)
			return ERROR;
		}

	/* get the time that we should delay until... */
	if((temp_ptr = my_strtok(NULL, "\n")) == NULL)
		return ERROR;
	delay_time = strtoul(temp_ptr, NULL, 10);

	/* delay the next notification... */
	if(cmd == CMD_DELAY_SVC_NOTIFICATION)
		temp_service->next_notification = delay_time;
	else
		temp_host->next_notification = delay_time;

	return OK;
	}



/* schedules a host check at a particular time */
int cmd_schedule_check(int cmd, char *args) {
	char *temp_ptr = NULL;
	host *temp_host = NULL;
	service *temp_service = NULL;
	servicesmember *temp_servicesmember = NULL;
	char *host_name = NULL;
	char *svc_description = NULL;
	time_t delay_time = 0L;

	/* get the host name */
	if((host_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	if(cmd == CMD_SCHEDULE_HOST_CHECK || cmd == CMD_SCHEDULE_FORCED_HOST_CHECK || cmd == CMD_SCHEDULE_HOST_SVC_CHECKS || cmd == CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS) {

		/* verify that the host is valid */
		if((temp_host = find_host(host_name)) == NULL)
			return ERROR;
		}

	else {

		/* get the service description */
		if((svc_description = my_strtok(NULL, ";")) == NULL)
			return ERROR;

		/* verify that the service is valid */
		if((temp_service = find_service(host_name, svc_description)) == NULL)
			return ERROR;
		}

	/* get the next check time */
	if((temp_ptr = my_strtok(NULL, "\n")) == NULL)
		return ERROR;
	delay_time = strtoul(temp_ptr, NULL, 10);

	/* schedule the host check */
	if(cmd == CMD_SCHEDULE_HOST_CHECK || cmd == CMD_SCHEDULE_FORCED_HOST_CHECK)
		schedule_host_check(temp_host, delay_time, (cmd == CMD_SCHEDULE_FORCED_HOST_CHECK) ? CHECK_OPTION_FORCE_EXECUTION : CHECK_OPTION_NONE);

	/* schedule service checks */
	else if(cmd == CMD_SCHEDULE_HOST_SVC_CHECKS || cmd == CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS) {
		for(temp_servicesmember = temp_host->services; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
			if((temp_service = temp_servicesmember->service_ptr) == NULL)
				continue;
			schedule_service_check(temp_service, delay_time, (cmd == CMD_SCHEDULE_FORCED_HOST_SVC_CHECKS) ? CHECK_OPTION_FORCE_EXECUTION : CHECK_OPTION_NONE);
			}
		}
	else
		schedule_service_check(temp_service, delay_time, (cmd == CMD_SCHEDULE_FORCED_SVC_CHECK) ? CHECK_OPTION_FORCE_EXECUTION : CHECK_OPTION_NONE);

	return OK;
	}



/* schedules all service checks on a host for a particular time */
int cmd_schedule_host_service_checks(int cmd, char *args, int force) {
	char *temp_ptr = NULL;
	service *temp_service = NULL;
	servicesmember *temp_servicesmember = NULL;
	host *temp_host = NULL;
	char *host_name = NULL;
	time_t delay_time = 0L;

	/* get the host name */
	if((host_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* verify that the host is valid */
	if((temp_host = find_host(host_name)) == NULL)
		return ERROR;

	/* get the next check time */
	if((temp_ptr = my_strtok(NULL, "\n")) == NULL)
		return ERROR;
	delay_time = strtoul(temp_ptr, NULL, 10);

	/* reschedule all services on the specified host */
	for(temp_servicesmember = temp_host->services; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
		if((temp_service = temp_servicesmember->service_ptr) == NULL)
			continue;
		schedule_service_check(temp_service, delay_time, (force == TRUE) ? CHECK_OPTION_FORCE_EXECUTION : CHECK_OPTION_NONE);
		}

	return OK;
	}




/* schedules a program shutdown or restart */
int cmd_signal_process(int cmd, char *args) {
	time_t scheduled_time = 0L;
	char *temp_ptr = NULL;

	/* get the time to schedule the event */
	if((temp_ptr = my_strtok(args, "\n")) == NULL)
		scheduled_time = 0L;
	else
		scheduled_time = strtoul(temp_ptr, NULL, 10);

	/* add a scheduled program shutdown or restart to the event list */
	if (!schedule_new_event((cmd == CMD_SHUTDOWN_PROCESS) ? EVENT_PROGRAM_SHUTDOWN : EVENT_PROGRAM_RESTART, TRUE, scheduled_time, FALSE, 0, NULL, FALSE, NULL, NULL, 0))
		return ERROR;

	return OK;
	}



/* processes results of an external service check */
int cmd_process_service_check_result(int cmd, time_t check_time, char *args) {
	char *temp_ptr = NULL;
	char *host_name = NULL;
	char *svc_description = NULL;
	int return_code = 0;
	char *output = NULL;
	int result = 0;

	/* get the host name */
	if((temp_ptr = my_strtok(args, ";")) == NULL)
		return ERROR;
	host_name = (char *)strdup(temp_ptr);

	/* get the service description */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL) {
		my_free(host_name);
		return ERROR;
		}
	svc_description = (char *)strdup(temp_ptr);

	/* get the service check return code */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL) {
		my_free(host_name);
		my_free(svc_description);
		return ERROR;
		}
	return_code = atoi(temp_ptr);

	/* get the plugin output (may be empty) */
	temp_ptr = my_strtok(NULL, "\n");
	/* Interpolate backslash and newline escape sequences to the literal
	 * characters they represent. This converts to the format we use internally
	 * so we don't have to worry about different representations later. */
	output = (temp_ptr) ? unescape_check_result_output(temp_ptr) : strdup("");

	/* submit the passive check result */
	result = process_passive_service_check(check_time, host_name, svc_description, return_code, output);

	/* free memory */
	my_free(host_name);
	my_free(svc_description);
	my_free(output);

	return result;
	}



/* submits a passive service check result for later processing */
int process_passive_service_check(time_t check_time, char *host_name, char *svc_description, int return_code, char *output) {
	check_result cr;
	host *temp_host = NULL;
	service *temp_service = NULL;
	struct timeval tv;

	/* skip this service check result if we aren't accepting passive service checks */
	if(accept_passive_service_checks == FALSE)
		return ERROR;

	/* make sure we have all required data */
	if(host_name == NULL || svc_description == NULL || output == NULL)
		return ERROR;

	temp_host = find_host_by_name_or_address(host_name);

	/* we couldn't find the host */
	if(temp_host == NULL) {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning:  Passive check result was received for service '%s' on host '%s', but the host could not be found!\n", svc_description, host_name);
		return ERROR;
		}

	/* make sure the service exists */
	if((temp_service = find_service(temp_host->name, svc_description)) == NULL) {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning:  Passive check result was received for service '%s' on host '%s', but the service could not be found!\n", svc_description, host_name);
		return ERROR;
		}

	/* skip this is we aren't accepting passive checks for this service */
	if(temp_service->accept_passive_checks == FALSE)
		return ERROR;

	memset(&cr, 0, sizeof(cr));
	cr.exited_ok = 1;
	cr.check_type = CHECK_TYPE_PASSIVE;
	cr.host_name = temp_host->name;
	cr.service_description = temp_service->description;
	cr.output = output;
	cr.start_time.tv_sec = cr.finish_time.tv_sec = check_time;
	cr.source = command_worker.source_name;

	/* save the return code and make sure it's sane */
	cr.return_code = return_code;
	if (cr.return_code < 0 || cr.return_code > 3)
		cr.return_code = STATE_UNKNOWN;

	/* calculate latency */
	gettimeofday(&tv, NULL);
	cr.latency = (double)((double)(tv.tv_sec - check_time) + (double)(tv.tv_usec / 1000.0) / 1000.0);
	if(cr.latency < 0.0)
		cr.latency = 0.0;

	return handle_async_service_check_result(temp_service, &cr);
	}



/* process passive host check result */
int cmd_process_host_check_result(int cmd, time_t check_time, char *args) {
	char *temp_ptr = NULL;
	char *host_name = NULL;
	int return_code = 0;
	char *output = NULL;
	int result = 0;

	/* get the host name */
	if((temp_ptr = my_strtok(args, ";")) == NULL)
		return ERROR;
	host_name = (char *)strdup(temp_ptr);

	/* get the host check return code */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL) {
		my_free(host_name);
		return ERROR;
		}
	return_code = atoi(temp_ptr);

	/* get the plugin output (may be empty) */
	temp_ptr = my_strtok(NULL, "\n");
	/* Interpolate backslash and newline escape sequences to the literal
	 * characters they represent. This converts to the format we use internally
	 * so we don't have to worry about different representations later. */
	output = (temp_ptr) ? unescape_check_result_output(temp_ptr) : strdup("");

	/* submit the check result */
	result = process_passive_host_check(check_time, host_name, return_code, output);

	/* free memory */
	my_free(host_name);
	my_free(output);

	return result;
	}


/* process passive host check result */
int process_passive_host_check(time_t check_time, char *host_name, int return_code, char *output) {
	check_result cr;
	host *temp_host = NULL;
	struct timeval tv;

	/* skip this host check result if we aren't accepting passive host checks */
	if(accept_passive_service_checks == FALSE)
		return ERROR;

	/* make sure we have all required data */
	if(host_name == NULL || output == NULL)
		return ERROR;

	/* make sure we have a reasonable return code */
	if(return_code < 0 || return_code > 2)
		return ERROR;

	/* find the host by its name or address */
	temp_host = find_host_by_name_or_address(host_name);

	/* we couldn't find the host */
	if(temp_host == NULL) {
		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning:  Passive check result was received for host '%s', but the host could not be found!\n", host_name);
		return ERROR;
		}

	/* skip this is we aren't accepting passive checks for this host */
	if(temp_host->accept_passive_checks == FALSE)
		return ERROR;

	memset(&cr, 0, sizeof(cr));
	cr.exited_ok = 1;
	cr.check_type = CHECK_TYPE_PASSIVE;
	cr.host_name = temp_host->name;
	cr.output = output;
	cr.start_time.tv_sec = cr.finish_time.tv_sec = check_time;
	cr.source = command_worker.source_name;
	cr.return_code = return_code;

	/* calculate latency */
	gettimeofday(&tv, NULL);
	cr.latency = (double)((double)(tv.tv_sec - check_time) + (double)(tv.tv_usec / 1000.0) / 1000.0);
	if(cr.latency < 0.0)
		cr.latency = 0.0;

	handle_async_host_check_result(temp_host, &cr);

	return OK;
	}



/* acknowledges a host or service problem */
int cmd_acknowledge_problem(int cmd, char *args) {
	service *temp_service = NULL;
	host *temp_host = NULL;
	char *host_name = NULL;
	char *svc_description = NULL;
	char *ack_author = NULL;
	char *ack_data = NULL;
	char *temp_ptr = NULL;
	int type = ACKNOWLEDGEMENT_NORMAL;
	int notify = TRUE;
	int persistent = TRUE;

	/* get the host name */
	if((host_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* verify that the host is valid */
	if((temp_host = find_host(host_name)) == NULL)
		return ERROR;

	/* this is a service acknowledgement */
	if(cmd == CMD_ACKNOWLEDGE_SVC_PROBLEM) {

		/* get the service name */
		if((svc_description = my_strtok(NULL, ";")) == NULL)
			return ERROR;

		/* verify that the service is valid */
		if((temp_service = find_service(temp_host->name, svc_description)) == NULL)
			return ERROR;
		}

	/* get the type */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return ERROR;
	type = atoi(temp_ptr);

	/* get the notification option */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return ERROR;
	notify = (atoi(temp_ptr) > 0) ? TRUE : FALSE;

	/* get the persistent option */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return ERROR;
	persistent = (atoi(temp_ptr) > 0) ? TRUE : FALSE;

	/* get the acknowledgement author */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return ERROR;
	ack_author = (char *)strdup(temp_ptr);

	/* get the acknowledgement data */
	if((temp_ptr = my_strtok(NULL, "\n")) == NULL) {
		my_free(ack_author);
		return ERROR;
		}
	ack_data = (char *)strdup(temp_ptr);

	/* acknowledge the host problem */
	if(cmd == CMD_ACKNOWLEDGE_HOST_PROBLEM)
		acknowledge_host_problem(temp_host, ack_author, ack_data, type, notify, persistent);

	/* acknowledge the service problem */
	else
		acknowledge_service_problem(temp_service, ack_author, ack_data, type, notify, persistent);

	/* free memory */
	my_free(ack_author);
	my_free(ack_data);

	return OK;
	}



/* removes a host or service acknowledgement */
int cmd_remove_acknowledgement(int cmd, char *args) {
	service *temp_service = NULL;
	host *temp_host = NULL;
	char *host_name = NULL;
	char *svc_description = NULL;

	/* get the host name */
	if((host_name = my_strtok(args, ";")) == NULL)
		return ERROR;

	/* verify that the host is valid */
	if((temp_host = find_host(host_name)) == NULL)
		return ERROR;

	/* we are removing a service acknowledgement */
	if(cmd == CMD_REMOVE_SVC_ACKNOWLEDGEMENT) {

		/* get the service name */
		if((svc_description = my_strtok(NULL, ";")) == NULL)
			return ERROR;

		/* verify that the service is valid */
		if((temp_service = find_service(temp_host->name, svc_description)) == NULL)
			return ERROR;
		}

	/* acknowledge the host problem */
	if(cmd == CMD_REMOVE_HOST_ACKNOWLEDGEMENT)
		remove_host_acknowledgement(temp_host);

	/* acknowledge the service problem */
	else
		remove_service_acknowledgement(temp_service);

	return OK;
	}



/* schedules downtime for a specific host or service */
int cmd_schedule_downtime(int cmd, time_t entry_time, char *args) {
	servicesmember *temp_servicesmember = NULL;
	service *temp_service = NULL;
	host *temp_host = NULL;
	host *last_host = NULL;
	hostgroup *temp_hostgroup = NULL;
	hostsmember *temp_hgmember = NULL;
	servicegroup *temp_servicegroup = NULL;
	servicesmember *temp_sgmember = NULL;
	char *host_name = NULL;
	char *hostgroup_name = NULL;
	char *servicegroup_name = NULL;
	char *svc_description = NULL;
	char *temp_ptr = NULL;
	time_t start_time = 0L;
	time_t end_time = 0L;
	int fixed = 0;
	unsigned long triggered_by = 0L;
	unsigned long duration = 0L;
	char *author = NULL;
	char *comment_data = NULL;
	unsigned long downtime_id = 0L;

	if(cmd == CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME || cmd == CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME) {

		/* get the hostgroup name */
		if((hostgroup_name = my_strtok(args, ";")) == NULL)
			return ERROR;

		/* verify that the hostgroup is valid */
		if((temp_hostgroup = find_hostgroup(hostgroup_name)) == NULL)
			return ERROR;
		}

	else if(cmd == CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME || cmd == CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME) {

		/* get the servicegroup name */
		if((servicegroup_name = my_strtok(args, ";")) == NULL)
			return ERROR;

		/* verify that the servicegroup is valid */
		if((temp_servicegroup = find_servicegroup(servicegroup_name)) == NULL)
			return ERROR;
		}

	else {

		/* get the host name */
		if((host_name = my_strtok(args, ";")) == NULL)
			return ERROR;

		/* verify that the host is valid */
		if((temp_host = find_host(host_name)) == NULL)
			return ERROR;

		/* this is a service downtime */
		if(cmd == CMD_SCHEDULE_SVC_DOWNTIME) {

			/* get the service name */
			if((svc_description = my_strtok(NULL, ";")) == NULL)
				return ERROR;

			/* verify that the service is valid */
			if((temp_service = find_service(temp_host->name, svc_description)) == NULL)
				return ERROR;
			}
		}

	/* get the start time */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return ERROR;
	start_time = (time_t)strtoul(temp_ptr, NULL, 10);

	/* get the end time */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return ERROR;
	end_time = (time_t)strtoul(temp_ptr, NULL, 10);

	/* get the fixed flag */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return ERROR;
	fixed = atoi(temp_ptr);

	/* get the trigger id */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return ERROR;
	triggered_by = strtoul(temp_ptr, NULL, 10);

	/* get the duration */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return ERROR;
	duration = strtoul(temp_ptr, NULL, 10);

	/* get the author */
	if((author = my_strtok(NULL, ";")) == NULL)
		return ERROR;

	/* get the comment */
	if((comment_data = my_strtok(NULL, ";")) == NULL)
		return ERROR;

	/* check if flexible downtime demanded and duration set
	   to non-zero.
	   according to the documentation, a flexible downtime is
	   started between start and end time and will last for
	   "duration" seconds. strtoul converts a NULL value to 0
	   so if set to 0, bail out as a duration>0 is needed. 	   */

	if(fixed == 0 && duration == 0)
		return ERROR;


	/* duration should be auto-calculated, not user-specified */
	if(fixed > 0)
		duration = (unsigned long)(end_time - start_time);

	/* schedule downtime */
	switch(cmd) {

		case CMD_SCHEDULE_HOST_DOWNTIME:
			schedule_downtime(HOST_DOWNTIME, host_name, NULL, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, &downtime_id);
			break;

		case CMD_SCHEDULE_SVC_DOWNTIME:
			schedule_downtime(SERVICE_DOWNTIME, host_name, svc_description, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, &downtime_id);
			break;

		case CMD_SCHEDULE_HOST_SVC_DOWNTIME:
			for(temp_servicesmember = temp_host->services; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
				if((temp_service = temp_servicesmember->service_ptr) == NULL)
					continue;
				schedule_downtime(SERVICE_DOWNTIME, host_name, temp_service->description, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, &downtime_id);
				}
			break;

		case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
			for(temp_hgmember = temp_hostgroup->members; temp_hgmember != NULL; temp_hgmember = temp_hgmember->next)
				schedule_downtime(HOST_DOWNTIME, temp_hgmember->host_name, NULL, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, &downtime_id);
			break;

		case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
			for(temp_hgmember = temp_hostgroup->members; temp_hgmember != NULL; temp_hgmember = temp_hgmember->next) {
				if((temp_host = temp_hgmember->host_ptr) == NULL)
					continue;
				for(temp_servicesmember = temp_host->services; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
					if((temp_service = temp_servicesmember->service_ptr) == NULL)
						continue;
					schedule_downtime(SERVICE_DOWNTIME, temp_service->host_name, temp_service->description, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, &downtime_id);
					}
				}
			break;

		case CMD_SCHEDULE_SERVICEGROUP_HOST_DOWNTIME:
			last_host = NULL;
			for(temp_sgmember = temp_servicegroup->members; temp_sgmember != NULL; temp_sgmember = temp_sgmember->next) {
				temp_host = find_host(temp_sgmember->host_name);
				if(temp_host == NULL)
					continue;
				if(last_host == temp_host)
					continue;
				schedule_downtime(HOST_DOWNTIME, temp_sgmember->host_name, NULL, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, &downtime_id);
				last_host = temp_host;
				}
			break;

		case CMD_SCHEDULE_SERVICEGROUP_SVC_DOWNTIME:
			for(temp_sgmember = temp_servicegroup->members; temp_sgmember != NULL; temp_sgmember = temp_sgmember->next)
				schedule_downtime(SERVICE_DOWNTIME, temp_sgmember->host_name, temp_sgmember->service_description, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, &downtime_id);
			break;

		case CMD_SCHEDULE_AND_PROPAGATE_HOST_DOWNTIME:

			/* schedule downtime for "parent" host */
			schedule_downtime(HOST_DOWNTIME, host_name, NULL, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, &downtime_id);

			/* schedule (non-triggered) downtime for all child hosts */
			schedule_and_propagate_downtime(temp_host, entry_time, author, comment_data, start_time, end_time, fixed, 0, duration);
			break;

		case CMD_SCHEDULE_AND_PROPAGATE_TRIGGERED_HOST_DOWNTIME:

			/* schedule downtime for "parent" host */
			schedule_downtime(HOST_DOWNTIME, host_name, NULL, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, &downtime_id);

			/* schedule triggered downtime for all child hosts */
			schedule_and_propagate_downtime(temp_host, entry_time, author, comment_data, start_time, end_time, fixed, downtime_id, duration);
			break;

		default:
			break;
		}

	return OK;
	}



/* deletes scheduled host or service downtime */
int cmd_delete_downtime(int cmd, char *args) {
	unsigned long downtime_id = 0L;
	char *temp_ptr = NULL;

	/* get the id of the downtime to delete */
	if((temp_ptr = my_strtok(args, "\n")) == NULL)
		return ERROR;
	downtime_id = strtoul(temp_ptr, NULL, 10);

	if(cmd == CMD_DEL_HOST_DOWNTIME)
		unschedule_downtime(HOST_DOWNTIME, downtime_id);
	else
		unschedule_downtime(SERVICE_DOWNTIME, downtime_id);

	return OK;
	}


/* Deletes scheduled host and service downtime based on hostname and optionally other filter arguments */
int cmd_delete_downtime_by_host_name(int cmd, char *args) {
	char *temp_ptr = NULL;
	char *end_ptr = NULL;
	char *hostname = NULL;
	char *service_description = NULL;
	char *downtime_comment = NULL;
	time_t downtime_start_time = 0L;
	int deleted = 0;

	/* get the host name of the downtime to delete */
	temp_ptr = my_strtok(args, ";");
	if(temp_ptr == NULL)
		return ERROR;
	hostname = temp_ptr;

	/* get the optional service name */
	temp_ptr = my_strtok(NULL, ";");
	if(temp_ptr != NULL) {
		if(*temp_ptr != '\0')
			service_description = temp_ptr;

		/* get the optional start time */
		temp_ptr = my_strtok(NULL, ";");
		if(temp_ptr != NULL) {
			downtime_start_time = strtoul(temp_ptr, &end_ptr, 10);

			/* get the optional comment */
			temp_ptr = my_strtok(NULL, ";");
			if(temp_ptr != NULL) {
				if(*temp_ptr != '\0')
					downtime_comment = temp_ptr;

				}
			}
		}

	deleted = delete_downtime_by_hostname_service_description_start_time_comment(hostname, service_description, downtime_start_time, downtime_comment);

	if(deleted == 0)
		return ERROR;

	return OK;
	}

int cmd_delete_downtime_by_hostgroup_name(int cmd, char *args) {
	char *temp_ptr = NULL;
	char *end_ptr = NULL;
	host *temp_host = NULL;
	hostgroup *temp_hostgroup = NULL;
	hostsmember *temp_member = NULL;
	char *service_description = NULL;
	char *downtime_comment = NULL;
	char *host_name = NULL;
	time_t downtime_start_time = 0L;
	int deleted = 0;

	/* get the host group name of the downtime to delete */
	temp_ptr = my_strtok(args, ";");
	if(temp_ptr == NULL)
		return ERROR;

	temp_hostgroup = find_hostgroup(temp_ptr);
	if(temp_hostgroup == NULL)
		return ERROR;

	/* get the optional host name */
	temp_ptr = my_strtok(NULL, ";");
	if(temp_ptr != NULL) {
		if(*temp_ptr != '\0')
			host_name = temp_ptr;

		/* get the optional service name */
		temp_ptr = my_strtok(NULL, ";");
		if(temp_ptr != NULL) {
			if(*temp_ptr != '\0')
				service_description = temp_ptr;

			/* get the optional start time */
			temp_ptr = my_strtok(NULL, ";");
			if(temp_ptr != NULL) {
				downtime_start_time = strtoul(temp_ptr, &end_ptr, 10);

				/* get the optional comment */
				temp_ptr = my_strtok(NULL, ";");
				if(temp_ptr != NULL) {
					if(*temp_ptr != '\0')
						downtime_comment = temp_ptr;

					}
				}
			}

		/* get the optional service name */
		temp_ptr = my_strtok(NULL, ";");
		if(temp_ptr != NULL) {
			if(*temp_ptr != '\0')
				service_description = temp_ptr;

			/* get the optional start time */
			temp_ptr = my_strtok(NULL, ";");
			if(temp_ptr != NULL) {
				downtime_start_time = strtoul(temp_ptr, &end_ptr, 10);

				/* get the optional comment */
				temp_ptr = my_strtok(NULL, ";");
				if(temp_ptr != NULL) {
					if(*temp_ptr != '\0')
						downtime_comment = temp_ptr;
					}
				}
			}
		}

	for(temp_member = temp_hostgroup->members; temp_member != NULL; temp_member = temp_member->next) {
		if((temp_host = (host *)temp_member->host_ptr) == NULL)
			continue;
		if(host_name != NULL && strcmp(temp_host->name, host_name) != 0)
			continue;
		deleted = +delete_downtime_by_hostname_service_description_start_time_comment(temp_host->name, service_description, downtime_start_time, downtime_comment);
		}

	if(deleted == 0)
		return ERROR;

	return OK;
	}

int cmd_delete_downtime_by_start_time_comment(int cmd, char *args) {
	time_t downtime_start_time = 0L;
	char *downtime_comment = NULL;
	char *temp_ptr = NULL;
	char *end_ptr = NULL;
	int deleted = 0;

	/* Get start time if set */
	temp_ptr = my_strtok(args, ";");
	if(temp_ptr != NULL) {
		/* This will be set to 0 if no start_time is entered or data is bad */
		downtime_start_time = strtoul(temp_ptr, &end_ptr, 10);
		}

	/* Get comment - not sure if this should be also tokenised by ; */
	temp_ptr = my_strtok(NULL, "\n");
	if(temp_ptr != NULL && *temp_ptr != '\0') {
		downtime_comment = temp_ptr;
		}

	/* No args should give an error */
	if(downtime_start_time == 0 && downtime_comment == NULL)
		return ERROR;

	deleted = delete_downtime_by_hostname_service_description_start_time_comment(NULL, NULL, downtime_start_time, downtime_comment);

	if(deleted == 0)
		return ERROR;

	return OK;
	}


/* changes a host or service (integer) variable */
int cmd_change_object_int_var(int cmd, char *args) {
	service *temp_service = NULL;
	host *temp_host = NULL;
	contact *temp_contact = NULL;
	char *host_name = NULL;
	char *svc_description = NULL;
	char *contact_name = NULL;
	char *temp_ptr = NULL;
	int intval = 0;
	double dval = 0.0;
	double old_dval = 0.0;
	time_t preferred_time = 0L;
	time_t next_valid_time = 0L;
	unsigned long attr = MODATTR_NONE;
	unsigned long hattr = MODATTR_NONE;
	unsigned long sattr = MODATTR_NONE;

	switch(cmd) {

		case CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL:
		case CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL:
		case CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS:
		case CMD_CHANGE_SVC_MODATTR:

			/* get the host name */
			if((host_name = my_strtok(args, ";")) == NULL)
				return ERROR;

			/* get the service name */
			if((svc_description = my_strtok(NULL, ";")) == NULL)
				return ERROR;

			/* verify that the service is valid */
			if((temp_service = find_service(host_name, svc_description)) == NULL)
				return ERROR;

			break;

		case CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL:
		case CMD_CHANGE_RETRY_HOST_CHECK_INTERVAL:
		case CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS:
		case CMD_CHANGE_HOST_MODATTR:

			/* get the host name */
			if((host_name = my_strtok(args, ";")) == NULL)
				return ERROR;

			/* verify that the host is valid */
			if((temp_host = find_host(host_name)) == NULL)
				return ERROR;
			break;

		case CMD_CHANGE_CONTACT_MODATTR:
		case CMD_CHANGE_CONTACT_MODHATTR:
		case CMD_CHANGE_CONTACT_MODSATTR:

			/* get the contact name */
			if((contact_name = my_strtok(args, ";")) == NULL)
				return ERROR;

			/* verify that the contact is valid */
			if((temp_contact = find_contact(contact_name)) == NULL)
				return ERROR;
			break;

		default:
			/* unknown command */
			return ERROR;
			break;
		}

	/* get the value */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL)
		return ERROR;
	intval = (int)strtol(temp_ptr, NULL, 0);
	if(intval < 0 || (intval == 0 && errno == EINVAL))
		return ERROR;
	dval = (int)strtod(temp_ptr, NULL);

	switch(cmd) {

		case CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL:

			/* save the old check interval */
			old_dval = temp_host->check_interval;

			/* modify the check interval */
			temp_host->check_interval = dval;
			attr = MODATTR_NORMAL_CHECK_INTERVAL;

			/* schedule a host check if previous interval was 0 (checks were not regularly scheduled) */
			if(old_dval == 0 && temp_host->checks_enabled == TRUE) {

				/* set the host check flag */
				temp_host->should_be_scheduled = TRUE;

				/* schedule a check for right now (or as soon as possible) */
				time(&preferred_time);
				if(check_time_against_period(preferred_time, temp_host->check_period_ptr) == ERROR) {
					get_next_valid_time(preferred_time, &next_valid_time, temp_host->check_period_ptr);
					temp_host->next_check = next_valid_time;
					}
				else
					temp_host->next_check = preferred_time;

				/* schedule a check if we should */
				if(temp_host->should_be_scheduled == TRUE)
					schedule_host_check(temp_host, temp_host->next_check, CHECK_OPTION_NONE);
				}

			break;

		case CMD_CHANGE_RETRY_HOST_CHECK_INTERVAL:

			temp_host->retry_interval = dval;
			attr = MODATTR_RETRY_CHECK_INTERVAL;

			break;

		case CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS:

			temp_host->max_attempts = intval;
			attr = MODATTR_MAX_CHECK_ATTEMPTS;

			/* adjust current attempt number if in a hard state */
			if(temp_host->state_type == HARD_STATE && temp_host->current_state != HOST_UP && temp_host->current_attempt > 1)
				temp_host->current_attempt = temp_host->max_attempts;

			break;

		case CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL:

			/* save the old check interval */
			old_dval = temp_service->check_interval;

			/* modify the check interval */
			temp_service->check_interval = dval;
			attr = MODATTR_NORMAL_CHECK_INTERVAL;

			/* schedule a service check if previous interval was 0 (checks were not regularly scheduled) */
			if(old_dval == 0 && temp_service->checks_enabled == TRUE && temp_service->check_interval != 0) {

				/* set the service check flag */
				temp_service->should_be_scheduled = TRUE;

				/* schedule a check for right now (or as soon as possible) */
				time(&preferred_time);
				if(check_time_against_period(preferred_time, temp_service->check_period_ptr) == ERROR) {
					get_next_valid_time(preferred_time, &next_valid_time, temp_service->check_period_ptr);
					temp_service->next_check = next_valid_time;
					}
				else
					temp_service->next_check = preferred_time;

				/* schedule a check if we should */
				if(temp_service->should_be_scheduled == TRUE)
					schedule_service_check(temp_service, temp_service->next_check, CHECK_OPTION_NONE);
				}

			break;

		case CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL:

			temp_service->retry_interval = dval;
			attr = MODATTR_RETRY_CHECK_INTERVAL;

			break;

		case CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS:

			temp_service->max_attempts = intval;
			attr = MODATTR_MAX_CHECK_ATTEMPTS;

			/* adjust current attempt number if in a hard state */
			if(temp_service->state_type == HARD_STATE && temp_service->current_state != STATE_OK && temp_service->current_attempt > 1)
				temp_service->current_attempt = temp_service->max_attempts;

			break;

		case CMD_CHANGE_HOST_MODATTR:
		case CMD_CHANGE_SVC_MODATTR:
		case CMD_CHANGE_CONTACT_MODATTR:

			attr = intval;
			break;

		case CMD_CHANGE_CONTACT_MODHATTR:

			hattr = intval;
			break;

		case CMD_CHANGE_CONTACT_MODSATTR:

			sattr = intval;
			break;


		default:
			break;
		}


	/* send data to event broker and update status file */
	switch(cmd) {

		case CMD_CHANGE_RETRY_SVC_CHECK_INTERVAL:
		case CMD_CHANGE_NORMAL_SVC_CHECK_INTERVAL:
		case CMD_CHANGE_MAX_SVC_CHECK_ATTEMPTS:
		case CMD_CHANGE_SVC_MODATTR:

			/* set the modified service attribute */
			if(cmd == CMD_CHANGE_SVC_MODATTR)
				temp_service->modified_attributes = attr;
			else
				temp_service->modified_attributes |= attr;

#ifdef USE_EVENT_BROKER
			/* send data to event broker */
			broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, temp_service, cmd, attr, temp_service->modified_attributes, NULL);
#endif

			/* update the status log with the service info */
			update_service_status(temp_service, FALSE);

			break;

		case CMD_CHANGE_NORMAL_HOST_CHECK_INTERVAL:
		case CMD_CHANGE_RETRY_HOST_CHECK_INTERVAL:
		case CMD_CHANGE_MAX_HOST_CHECK_ATTEMPTS:
		case CMD_CHANGE_HOST_MODATTR:

			/* set the modified host attribute */
			if(cmd == CMD_CHANGE_HOST_MODATTR)
				temp_host->modified_attributes = attr;
			else
				temp_host->modified_attributes |= attr;

#ifdef USE_EVENT_BROKER
			/* send data to event broker */
			broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, temp_host, cmd, attr, temp_host->modified_attributes, NULL);
#endif

			/* update the status log with the host info */
			update_host_status(temp_host, FALSE);
			break;

		case CMD_CHANGE_CONTACT_MODATTR:
		case CMD_CHANGE_CONTACT_MODHATTR:
		case CMD_CHANGE_CONTACT_MODSATTR:

			/* set the modified attribute */
			switch(cmd) {
				case CMD_CHANGE_CONTACT_MODATTR:
					temp_contact->modified_attributes = attr;
					break;
				case CMD_CHANGE_CONTACT_MODHATTR:
					temp_contact->modified_host_attributes = hattr;
					break;
				case CMD_CHANGE_CONTACT_MODSATTR:
					temp_contact->modified_service_attributes = sattr;
					break;
				default:
					break;
				}

#ifdef USE_EVENT_BROKER
			/* send data to event broker */
			broker_adaptive_contact_data(NEBTYPE_ADAPTIVECONTACT_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, temp_contact, cmd, attr, temp_contact->modified_attributes, hattr, temp_contact->modified_host_attributes, sattr, temp_contact->modified_service_attributes, NULL);
#endif

			/* update the status log with the contact info */
			update_contact_status(temp_contact, FALSE);
			break;

		default:
			break;
		}

	return OK;
	}



/* changes a host or service (char) variable */
int cmd_change_object_char_var(int cmd, char *args) {
	service *temp_service = NULL;
	host *temp_host = NULL;
	contact *temp_contact = NULL;
	timeperiod *temp_timeperiod = NULL;
	command *temp_command = NULL;
	char *host_name = NULL;
	char *svc_description = NULL;
	char *contact_name = NULL;
	char *charval = NULL;
	char *temp_ptr = NULL;
	char *temp_ptr2 = NULL;
	unsigned long attr = MODATTR_NONE;
	unsigned long hattr = MODATTR_NONE;
	unsigned long sattr = MODATTR_NONE;


	/* SECURITY PATCH - disable these for the time being */
	switch(cmd) {
		case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
		case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:
		case CMD_CHANGE_HOST_EVENT_HANDLER:
		case CMD_CHANGE_SVC_EVENT_HANDLER:
		case CMD_CHANGE_HOST_CHECK_COMMAND:
		case CMD_CHANGE_SVC_CHECK_COMMAND:
			return ERROR;
		}


	/* get the command arguments */
	switch(cmd) {

		case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
		case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:

			if((charval = my_strtok(args, "\n")) == NULL)
				return ERROR;

			break;

		case CMD_CHANGE_HOST_EVENT_HANDLER:
		case CMD_CHANGE_HOST_CHECK_COMMAND:
		case CMD_CHANGE_HOST_CHECK_TIMEPERIOD:
		case CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD:

			/* get the host name */
			if((host_name = my_strtok(args, ";")) == NULL)
				return ERROR;

			/* verify that the host is valid */
			if((temp_host = find_host(host_name)) == NULL)
				return ERROR;

			if((charval = my_strtok(NULL, "\n")) == NULL)
				return ERROR;

			break;

		case CMD_CHANGE_SVC_EVENT_HANDLER:
		case CMD_CHANGE_SVC_CHECK_COMMAND:
		case CMD_CHANGE_SVC_CHECK_TIMEPERIOD:
		case CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD:

			/* get the host name */
			if((host_name = my_strtok(args, ";")) == NULL)
				return ERROR;

			/* get the service name */
			if((svc_description = my_strtok(NULL, ";")) == NULL)
				return ERROR;

			/* verify that the service is valid */
			if((temp_service = find_service(host_name, svc_description)) == NULL)
				return ERROR;

			if((charval = my_strtok(NULL, "\n")) == NULL)
				return ERROR;

			break;


		case CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD:
		case CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD:

			/* get the contact name */
			if((contact_name = my_strtok(args, ";")) == NULL)
				return ERROR;

			/* verify that the contact is valid */
			if((temp_contact = find_contact(contact_name)) == NULL)
				return ERROR;

			if((charval = my_strtok(NULL, "\n")) == NULL)
				return ERROR;

			break;

		default:
			/* invalid command */
			return ERROR;
			break;

		}

	if((temp_ptr = (char *)strdup(charval)) == NULL)
		return ERROR;


	/* do some validation */
	switch(cmd) {

		case CMD_CHANGE_HOST_CHECK_TIMEPERIOD:
		case CMD_CHANGE_SVC_CHECK_TIMEPERIOD:
		case CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD:
		case CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD:
		case CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD:
		case CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD:

			/* make sure the timeperiod is valid */
			if((temp_timeperiod = find_timeperiod(temp_ptr)) == NULL) {
				my_free(temp_ptr);
				return ERROR;
				}

			break;

		case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:
		case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:
		case CMD_CHANGE_HOST_EVENT_HANDLER:
		case CMD_CHANGE_SVC_EVENT_HANDLER:
		case CMD_CHANGE_HOST_CHECK_COMMAND:
		case CMD_CHANGE_SVC_CHECK_COMMAND:

			/* make sure the command exists */
			temp_ptr2 = my_strtok(temp_ptr, "!");
			if((temp_command = find_command(temp_ptr2)) == NULL) {
				my_free(temp_ptr);
				return ERROR;
				}

			my_free(temp_ptr);
			if((temp_ptr = (char *)strdup(charval)) == NULL)
				return ERROR;

			break;

		default:
			break;
		}


	/* update the variable */
	switch(cmd) {

		case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:

			my_free(global_host_event_handler);
			global_host_event_handler = temp_ptr;
			global_host_event_handler_ptr = temp_command;
			attr = MODATTR_EVENT_HANDLER_COMMAND;
			break;

		case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:

			my_free(global_service_event_handler);
			global_service_event_handler = temp_ptr;
			global_service_event_handler_ptr = temp_command;
			attr = MODATTR_EVENT_HANDLER_COMMAND;
			break;

		case CMD_CHANGE_HOST_EVENT_HANDLER:

			my_free(temp_host->event_handler);
			temp_host->event_handler = temp_ptr;
			temp_host->event_handler_ptr = temp_command;
			attr = MODATTR_EVENT_HANDLER_COMMAND;
			break;

		case CMD_CHANGE_HOST_CHECK_COMMAND:

			my_free(temp_host->check_command);
			temp_host->check_command = temp_ptr;
			temp_host->check_command_ptr = temp_command;
			attr = MODATTR_CHECK_COMMAND;
			break;

		case CMD_CHANGE_HOST_CHECK_TIMEPERIOD:

			my_free(temp_host->check_period);
			temp_host->check_period = temp_ptr;
			temp_host->check_period_ptr = temp_timeperiod;
			attr = MODATTR_CHECK_TIMEPERIOD;
			break;

		case CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD:

			my_free(temp_host->notification_period);
			temp_host->notification_period = temp_ptr;
			temp_host->notification_period_ptr = temp_timeperiod;
			attr = MODATTR_NOTIFICATION_TIMEPERIOD;
			break;

		case CMD_CHANGE_SVC_EVENT_HANDLER:

			my_free(temp_service->event_handler);
			temp_service->event_handler = temp_ptr;
			temp_service->event_handler_ptr = temp_command;
			attr = MODATTR_EVENT_HANDLER_COMMAND;
			break;

		case CMD_CHANGE_SVC_CHECK_COMMAND:

			my_free(temp_service->check_command);
			temp_service->check_command = temp_ptr;
			temp_service->check_command_ptr = temp_command;
			attr = MODATTR_CHECK_COMMAND;
			break;

		case CMD_CHANGE_SVC_CHECK_TIMEPERIOD:

			my_free(temp_service->check_period);
			temp_service->check_period = temp_ptr;
			temp_service->check_period_ptr = temp_timeperiod;
			attr = MODATTR_CHECK_TIMEPERIOD;
			break;

		case CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD:

			my_free(temp_service->notification_period);
			temp_service->notification_period = temp_ptr;
			temp_service->notification_period_ptr = temp_timeperiod;
			attr = MODATTR_NOTIFICATION_TIMEPERIOD;
			break;

		case CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD:

			my_free(temp_contact->host_notification_period);
			temp_contact->host_notification_period = temp_ptr;
			temp_contact->host_notification_period_ptr = temp_timeperiod;
			hattr = MODATTR_NOTIFICATION_TIMEPERIOD;
			break;

		case CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD:

			my_free(temp_contact->service_notification_period);
			temp_contact->service_notification_period = temp_ptr;
			temp_contact->service_notification_period_ptr = temp_timeperiod;
			sattr = MODATTR_NOTIFICATION_TIMEPERIOD;
			break;

		default:
			break;
		}


	/* send data to event broker and update status file */
	switch(cmd) {

		case CMD_CHANGE_GLOBAL_HOST_EVENT_HANDLER:

			/* set the modified host attribute */
			modified_host_process_attributes |= attr;

#ifdef USE_EVENT_BROKER
			/* send data to event broker */
			broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, cmd, attr, modified_host_process_attributes, MODATTR_NONE, modified_service_process_attributes, NULL);
#endif

			/* update program status */
			update_program_status(FALSE);

			break;

		case CMD_CHANGE_GLOBAL_SVC_EVENT_HANDLER:

			/* set the modified service attribute */
			modified_service_process_attributes |= attr;

#ifdef USE_EVENT_BROKER
			/* send data to event broker */
			broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, cmd, MODATTR_NONE, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

			/* update program status */
			update_program_status(FALSE);

			break;

		case CMD_CHANGE_SVC_EVENT_HANDLER:
		case CMD_CHANGE_SVC_CHECK_COMMAND:
		case CMD_CHANGE_SVC_CHECK_TIMEPERIOD:
		case CMD_CHANGE_SVC_NOTIFICATION_TIMEPERIOD:

			/* set the modified service attribute */
			temp_service->modified_attributes |= attr;

#ifdef USE_EVENT_BROKER
			/* send data to event broker */
			broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, temp_service, cmd, attr, temp_service->modified_attributes, NULL);
#endif

			/* update the status log with the service info */
			update_service_status(temp_service, FALSE);

			break;

		case CMD_CHANGE_HOST_EVENT_HANDLER:
		case CMD_CHANGE_HOST_CHECK_COMMAND:
		case CMD_CHANGE_HOST_CHECK_TIMEPERIOD:
		case CMD_CHANGE_HOST_NOTIFICATION_TIMEPERIOD:

			/* set the modified host attribute */
			temp_host->modified_attributes |= attr;

#ifdef USE_EVENT_BROKER
			/* send data to event broker */
			broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, temp_host, cmd, attr, temp_host->modified_attributes, NULL);
#endif

			/* update the status log with the host info */
			update_host_status(temp_host, FALSE);
			break;

		case CMD_CHANGE_CONTACT_HOST_NOTIFICATION_TIMEPERIOD:
		case CMD_CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD:

			/* set the modified attributes */
			temp_contact->modified_host_attributes |= hattr;
			temp_contact->modified_service_attributes |= sattr;

#ifdef USE_EVENT_BROKER
			/* send data to event broker */
			broker_adaptive_contact_data(NEBTYPE_ADAPTIVECONTACT_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, temp_contact, cmd, attr, temp_contact->modified_attributes, hattr, temp_contact->modified_host_attributes, sattr, temp_contact->modified_service_attributes, NULL);
#endif

			/* update the status log with the contact info */
			update_contact_status(temp_contact, FALSE);
			break;

		default:
			break;
		}

	return OK;
	}



/* changes a custom host or service variable */
int cmd_change_object_custom_var(int cmd, char *args) {
	host *temp_host = NULL;
	service *temp_service = NULL;
	contact *temp_contact = NULL;
	customvariablesmember *temp_customvariablesmember = NULL;
	char *temp_ptr = NULL;
	char *name1 = NULL;
	char *name2 = NULL;
	char *varname = NULL;
	char *varvalue = NULL;
	register int x = 0;

	/* get the host or contact name */
	if((temp_ptr = my_strtok(args, ";")) == NULL)
		return ERROR;
	if((name1 = (char *)strdup(temp_ptr)) == NULL)
		return ERROR;

	/* get the service description if necessary */
	if(cmd == CMD_CHANGE_CUSTOM_SVC_VAR) {
		if((temp_ptr = my_strtok(NULL, ";")) == NULL) {
			my_free(name1);
			return ERROR;
			}
		if((name2 = (char *)strdup(temp_ptr)) == NULL) {
			my_free(name1);
			return ERROR;
			}
		}

	/* get the custom variable name */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL) {
		my_free(name1);
		my_free(name2);
		return ERROR;
		}
	if((varname = (char *)strdup(temp_ptr)) == NULL) {
		my_free(name1);
		my_free(name2);
		return ERROR;
		}

	/* get the custom variable value */
	if((temp_ptr = my_strtok(NULL, ";")) == NULL) {
		my_free(name1);
		my_free(name2);
		my_free(varname);
		return ERROR;
		}
	if((varvalue = (char *)strdup(temp_ptr)) == NULL) {
		my_free(name1);
		my_free(name2);
		my_free(varname);
		return ERROR;
		}

	/* find the object */
	switch(cmd) {
		case CMD_CHANGE_CUSTOM_HOST_VAR:
			if((temp_host = find_host(name1)) == NULL)
				return ERROR;
			temp_customvariablesmember = temp_host->custom_variables;
			break;
		case CMD_CHANGE_CUSTOM_SVC_VAR:
			if((temp_service = find_service(name1, name2)) == NULL)
				return ERROR;
			temp_customvariablesmember = temp_service->custom_variables;
			break;
		case CMD_CHANGE_CUSTOM_CONTACT_VAR:
			if((temp_contact = find_contact(name1)) == NULL)
				return ERROR;
			temp_customvariablesmember = temp_contact->custom_variables;
			break;
		default:
			break;
		}

	/* capitalize the custom variable name */
	for(x = 0; varname[x] != '\x0'; x++)
		varname[x] = toupper(varname[x]);

	/* find the proper variable */
	for(; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {

		/* we found the variable, so update the value */
		if(!strcmp(varname, temp_customvariablesmember->variable_name)) {

			/* update the value */
			if(temp_customvariablesmember->variable_value)
				my_free(temp_customvariablesmember->variable_value);
			temp_customvariablesmember->variable_value = (char *)strdup(varvalue);

			/* mark the variable value as having been changed */
			temp_customvariablesmember->has_been_modified = TRUE;

			break;
			}
		}

	/* free memory */
	my_free(name1);
	my_free(name2);
	my_free(varname);
	my_free(varvalue);

	/* set the modified attributes and update the status of the object */
	switch(cmd) {
		case CMD_CHANGE_CUSTOM_HOST_VAR:
			temp_host->modified_attributes |= MODATTR_CUSTOM_VARIABLE;
			update_host_status(temp_host, FALSE);
			break;
		case CMD_CHANGE_CUSTOM_SVC_VAR:
			temp_service->modified_attributes |= MODATTR_CUSTOM_VARIABLE;
			update_service_status(temp_service, FALSE);
			break;
		case CMD_CHANGE_CUSTOM_CONTACT_VAR:
			temp_contact->modified_attributes |= MODATTR_CUSTOM_VARIABLE;
			update_contact_status(temp_contact, FALSE);
			break;
		default:
			break;
		}

	return OK;
	}


/* processes an external host command */
int cmd_process_external_commands_from_file(int cmd, char *args) {
	char *fname = NULL;
	char *temp_ptr = NULL;
	int delete_file = FALSE;

	/* get the file name */
	if((temp_ptr = my_strtok(args, ";")) == NULL)
		return ERROR;
	if((fname = (char *)strdup(temp_ptr)) == NULL)
		return ERROR;

	/* find the deletion option */
	if((temp_ptr = my_strtok(NULL, "\n")) == NULL) {
		my_free(fname);
		return ERROR;
		}
	if(atoi(temp_ptr) == 0)
		delete_file = FALSE;
	else
		delete_file = TRUE;

	/* process the file */
	process_external_commands_from_file(fname, delete_file);

	/* free memory */
	my_free(fname);

	return OK;
	}


/******************************************************************/
/*************** INTERNAL COMMAND IMPLEMENTATIONS  ****************/
/******************************************************************/

/* temporarily disables a service check */
void disable_service_checks(service *svc) {
	unsigned long attr = MODATTR_ACTIVE_CHECKS_ENABLED;

	/* checks are already disabled */
	if(svc->checks_enabled == FALSE)
		return;

	/* set the attribute modified flag */
	svc->modified_attributes |= attr;

	/* disable the service check... */
	svc->checks_enabled = FALSE;
	svc->should_be_scheduled = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, svc, CMD_NONE, attr, svc->modified_attributes, NULL);
#endif

	/* update the status log to reflect the new service state */
	update_service_status(svc, FALSE);

	return;
	}



/* enables a service check */
void enable_service_checks(service *svc) {
	time_t preferred_time = 0L;
	time_t next_valid_time = 0L;
	unsigned long attr = MODATTR_ACTIVE_CHECKS_ENABLED;

	/* checks are already enabled */
	if(svc->checks_enabled == TRUE)
		return;

	/* set the attribute modified flag */
	svc->modified_attributes |= attr;

	/* enable the service check... */
	svc->checks_enabled = TRUE;
	svc->should_be_scheduled = TRUE;

	/* services with no check intervals don't get checked */
	if(svc->check_interval == 0)
		svc->should_be_scheduled = FALSE;

	/* schedule a check for right now (or as soon as possible) */
	time(&preferred_time);
	if(check_time_against_period(preferred_time, svc->check_period_ptr) == ERROR) {
		get_next_valid_time(preferred_time, &next_valid_time, svc->check_period_ptr);
		svc->next_check = next_valid_time;
		}
	else
		svc->next_check = preferred_time;

	/* schedule a check if we should */
	if(svc->should_be_scheduled == TRUE)
		schedule_service_check(svc, svc->next_check, CHECK_OPTION_NONE);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, svc, CMD_NONE, attr, svc->modified_attributes, NULL);
#endif

	/* update the status log to reflect the new service state */
	update_service_status(svc, FALSE);

	return;
	}



/* enable notifications on a program-wide basis */
void enable_all_notifications(void) {
	unsigned long attr = MODATTR_NOTIFICATIONS_ENABLED;

	/* bail out if we're already set... */
	if(enable_notifications == TRUE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;
	modified_service_process_attributes |= attr;

	/* update notification status */
	enable_notifications = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log */
	update_program_status(FALSE);

	return;
	}


/* disable notifications on a program-wide basis */
void disable_all_notifications(void) {
	unsigned long attr = MODATTR_NOTIFICATIONS_ENABLED;

	/* bail out if we're already set... */
	if(enable_notifications == FALSE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;
	modified_service_process_attributes |= attr;

	/* update notification status */
	enable_notifications = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log */
	update_program_status(FALSE);

	return;
	}


/* enables notifications for a service */
void enable_service_notifications(service *svc) {
	unsigned long attr = MODATTR_NOTIFICATIONS_ENABLED;

	/* no change */
	if(svc->notifications_enabled == TRUE)
		return;

	/* set the attribute modified flag */
	svc->modified_attributes |= attr;

	/* enable the service notifications... */
	svc->notifications_enabled = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, svc, CMD_NONE, attr, svc->modified_attributes, NULL);
#endif

	/* update the status log to reflect the new service state */
	update_service_status(svc, FALSE);

	return;
	}


/* disables notifications for a service */
void disable_service_notifications(service *svc) {
	unsigned long attr = MODATTR_NOTIFICATIONS_ENABLED;

	/* no change */
	if(svc->notifications_enabled == FALSE)
		return;

	/* set the attribute modified flag */
	svc->modified_attributes |= attr;

	/* disable the service notifications... */
	svc->notifications_enabled = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, svc, CMD_NONE, attr, svc->modified_attributes, NULL);
#endif

	/* update the status log to reflect the new service state */
	update_service_status(svc, FALSE);

	return;
	}


/* enables notifications for a host */
void enable_host_notifications(host *hst) {
	unsigned long attr = MODATTR_NOTIFICATIONS_ENABLED;

	/* no change */
	if(hst->notifications_enabled == TRUE)
		return;

	/* set the attribute modified flag */
	hst->modified_attributes |= attr;

	/* enable the host notifications... */
	hst->notifications_enabled = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, hst, CMD_NONE, attr, hst->modified_attributes, NULL);
#endif

	/* update the status log to reflect the new host state */
	update_host_status(hst, FALSE);

	return;
	}


/* disables notifications for a host */
void disable_host_notifications(host *hst) {
	unsigned long attr = MODATTR_NOTIFICATIONS_ENABLED;

	/* no change */
	if(hst->notifications_enabled == FALSE)
		return;

	/* set the attribute modified flag */
	hst->modified_attributes |= attr;

	/* disable the host notifications... */
	hst->notifications_enabled = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, hst, CMD_NONE, attr, hst->modified_attributes, NULL);
#endif

	/* update the status log to reflect the new host state */
	update_host_status(hst, FALSE);

	return;
	}


/* enables notifications for all hosts and services "beyond" a given host */
void enable_and_propagate_notifications(host *hst, int level, int affect_top_host, int affect_hosts, int affect_services) {
	host *child_host = NULL;
	service *temp_service = NULL;
	servicesmember *temp_servicesmember = NULL;
	hostsmember *temp_hostsmember = NULL;

	/* enable notification for top level host */
	if(affect_top_host == TRUE && level == 0)
		enable_host_notifications(hst);

	/* check all child hosts... */
	for(temp_hostsmember = hst->child_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {

		if((child_host = temp_hostsmember->host_ptr) == NULL)
			continue;

		/* recurse... */
		enable_and_propagate_notifications(child_host, level + 1, affect_top_host, affect_hosts, affect_services);

		/* enable notifications for this host */
		if(affect_hosts == TRUE)
			enable_host_notifications(child_host);

		/* enable notifications for all services on this host... */
		if(affect_services == TRUE) {
			for(temp_servicesmember = child_host->services; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
				if((temp_service = temp_servicesmember->service_ptr) == NULL)
					continue;
				enable_service_notifications(temp_service);
				}
			}
		}

	return;
	}


/* disables notifications for all hosts and services "beyond" a given host */
void disable_and_propagate_notifications(host *hst, int level, int affect_top_host, int affect_hosts, int affect_services) {
	host *child_host = NULL;
	service *temp_service = NULL;
	servicesmember *temp_servicesmember = NULL;
	hostsmember *temp_hostsmember = NULL;

	if(hst == NULL)
		return;

	/* disable notifications for top host */
	if(affect_top_host == TRUE && level == 0)
		disable_host_notifications(hst);

	/* check all child hosts... */
	for(temp_hostsmember = hst->child_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {

		if((child_host = temp_hostsmember->host_ptr) == NULL)
			continue;

		/* recurse... */
		disable_and_propagate_notifications(child_host, level + 1, affect_top_host, affect_hosts, affect_services);

		/* disable notifications for this host */
		if(affect_hosts == TRUE)
			disable_host_notifications(child_host);

		/* disable notifications for all services on this host... */
		if(affect_services == TRUE) {
			for(temp_servicesmember = child_host->services; temp_servicesmember != NULL; temp_servicesmember = temp_servicesmember->next) {
				if((temp_service = temp_servicesmember->service_ptr) == NULL)
					continue;
				disable_service_notifications(temp_service);
				}
			}
		}

	return;
	}



/* enables host notifications for a contact */
void enable_contact_host_notifications(contact *cntct) {
	unsigned long attr = MODATTR_NOTIFICATIONS_ENABLED;

	/* no change */
	if(cntct->host_notifications_enabled == TRUE)
		return;

	/* set the attribute modified flag */
	cntct->modified_host_attributes |= attr;

	/* enable the host notifications... */
	cntct->host_notifications_enabled = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_contact_data(NEBTYPE_ADAPTIVECONTACT_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, cntct, CMD_NONE, MODATTR_NONE, cntct->modified_attributes, attr, cntct->modified_host_attributes, MODATTR_NONE, cntct->modified_service_attributes, NULL);
#endif

	/* update the status log to reflect the new contact state */
	update_contact_status(cntct, FALSE);

	return;
	}



/* disables host notifications for a contact */
void disable_contact_host_notifications(contact *cntct) {
	unsigned long attr = MODATTR_NOTIFICATIONS_ENABLED;

	/* no change */
	if(cntct->host_notifications_enabled == FALSE)
		return;

	/* set the attribute modified flag */
	cntct->modified_host_attributes |= attr;

	/* enable the host notifications... */
	cntct->host_notifications_enabled = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_contact_data(NEBTYPE_ADAPTIVECONTACT_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, cntct, CMD_NONE, MODATTR_NONE, cntct->modified_attributes, attr, cntct->modified_host_attributes, MODATTR_NONE, cntct->modified_service_attributes, NULL);
#endif

	/* update the status log to reflect the new contact state */
	update_contact_status(cntct, FALSE);

	return;
	}



/* enables service notifications for a contact */
void enable_contact_service_notifications(contact *cntct) {
	unsigned long attr = MODATTR_NOTIFICATIONS_ENABLED;

	/* no change */
	if(cntct->service_notifications_enabled == TRUE)
		return;

	/* set the attribute modified flag */
	cntct->modified_service_attributes |= attr;

	/* enable the host notifications... */
	cntct->service_notifications_enabled = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_contact_data(NEBTYPE_ADAPTIVECONTACT_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, cntct, CMD_NONE, MODATTR_NONE, cntct->modified_attributes, MODATTR_NONE, cntct->modified_host_attributes, attr, cntct->modified_service_attributes, NULL);
#endif

	/* update the status log to reflect the new contact state */
	update_contact_status(cntct, FALSE);

	return;
	}



/* disables service notifications for a contact */
void disable_contact_service_notifications(contact *cntct) {
	unsigned long attr = MODATTR_NOTIFICATIONS_ENABLED;

	/* no change */
	if(cntct->service_notifications_enabled == FALSE)
		return;

	/* set the attribute modified flag */
	cntct->modified_service_attributes |= attr;

	/* enable the host notifications... */
	cntct->service_notifications_enabled = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_contact_data(NEBTYPE_ADAPTIVECONTACT_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, cntct, CMD_NONE, MODATTR_NONE, cntct->modified_attributes, MODATTR_NONE, cntct->modified_host_attributes, attr, cntct->modified_service_attributes, NULL);
#endif

	/* update the status log to reflect the new contact state */
	update_contact_status(cntct, FALSE);

	return;
	}



/* schedules downtime for all hosts "beyond" a given host */
void schedule_and_propagate_downtime(host *temp_host, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration) {
	host *child_host = NULL;
	hostsmember *temp_hostsmember = NULL;

	/* check all child hosts... */
	for(temp_hostsmember = temp_host->child_hosts; temp_hostsmember != NULL; temp_hostsmember = temp_hostsmember->next) {

		if((child_host = temp_hostsmember->host_ptr) == NULL)
			continue;

		/* recurse... */
		schedule_and_propagate_downtime(child_host, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration);

		/* schedule downtime for this host */
		schedule_downtime(HOST_DOWNTIME, child_host->name, NULL, entry_time, author, comment_data, start_time, end_time, fixed, triggered_by, duration, NULL);
		}

	return;
	}


/* acknowledges a host problem */
void acknowledge_host_problem(host *hst, char *ack_author, char *ack_data, int type, int notify, int persistent) {
	time_t current_time = 0L;

	/* cannot acknowledge a non-existent problem */
	if(hst->current_state == HOST_UP)
		return;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_acknowledgement_data(NEBTYPE_ACKNOWLEDGEMENT_ADD, NEBFLAG_NONE, NEBATTR_NONE, HOST_ACKNOWLEDGEMENT, (void *)hst, ack_author, ack_data, type, notify, persistent, NULL);
#endif

	/* send out an acknowledgement notification */
	if(notify == TRUE)
		host_notification(hst, NOTIFICATION_ACKNOWLEDGEMENT, ack_author, ack_data, NOTIFICATION_OPTION_NONE);

	/* set the acknowledgement flag */
	hst->problem_has_been_acknowledged = TRUE;

	/* set the acknowledgement type */
	hst->acknowledgement_type = (type == ACKNOWLEDGEMENT_STICKY) ? ACKNOWLEDGEMENT_STICKY : ACKNOWLEDGEMENT_NORMAL;

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	/* add a comment for the acknowledgement */
	time(&current_time);
	add_new_host_comment(ACKNOWLEDGEMENT_COMMENT, hst->name, current_time, ack_author, ack_data, persistent, COMMENTSOURCE_INTERNAL, FALSE, (time_t)0, NULL);

	return;
	}


/* acknowledges a service problem */
void acknowledge_service_problem(service *svc, char *ack_author, char *ack_data, int type, int notify, int persistent) {
	time_t current_time = 0L;

	/* cannot acknowledge a non-existent problem */
	if(svc->current_state == STATE_OK)
		return;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_acknowledgement_data(NEBTYPE_ACKNOWLEDGEMENT_ADD, NEBFLAG_NONE, NEBATTR_NONE, SERVICE_ACKNOWLEDGEMENT, (void *)svc, ack_author, ack_data, type, notify, persistent, NULL);
#endif

	/* send out an acknowledgement notification */
	if(notify == TRUE)
		service_notification(svc, NOTIFICATION_ACKNOWLEDGEMENT, ack_author, ack_data, NOTIFICATION_OPTION_NONE);

	/* set the acknowledgement flag */
	svc->problem_has_been_acknowledged = TRUE;

	/* set the acknowledgement type */
	svc->acknowledgement_type = (type == ACKNOWLEDGEMENT_STICKY) ? ACKNOWLEDGEMENT_STICKY : ACKNOWLEDGEMENT_NORMAL;

	/* update the status log with the service info */
	update_service_status(svc, FALSE);

	/* add a comment for the acknowledgement */
	time(&current_time);
	add_new_service_comment(ACKNOWLEDGEMENT_COMMENT, svc->host_name, svc->description, current_time, ack_author, ack_data, persistent, COMMENTSOURCE_INTERNAL, FALSE, (time_t)0, NULL);

	return;
	}


/* removes a host acknowledgement */
void remove_host_acknowledgement(host *hst) {

	/* set the acknowledgement flag */
	hst->problem_has_been_acknowledged = FALSE;

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	/* remove any non-persistant comments associated with the ack */
	delete_host_acknowledgement_comments(hst);

	return;
	}


/* removes a service acknowledgement */
void remove_service_acknowledgement(service *svc) {

	/* set the acknowledgement flag */
	svc->problem_has_been_acknowledged = FALSE;

	/* update the status log with the service info */
	update_service_status(svc, FALSE);

	/* remove any non-persistant comments associated with the ack */
	delete_service_acknowledgement_comments(svc);

	return;
	}


/* starts executing service checks */
void start_executing_service_checks(void) {
	unsigned long attr = MODATTR_ACTIVE_CHECKS_ENABLED;

	/* bail out if we're already executing services */
	if(execute_service_checks == TRUE)
		return;

	/* set the attribute modified flag */
	modified_service_process_attributes |= attr;

	/* set the service check execution flag */
	execute_service_checks = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, MODATTR_NONE, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}




/* stops executing service checks */
void stop_executing_service_checks(void) {
	unsigned long attr = MODATTR_ACTIVE_CHECKS_ENABLED;

	/* bail out if we're already not executing services */
	if(execute_service_checks == FALSE)
		return;

	/* set the attribute modified flag */
	modified_service_process_attributes |= attr;

	/* set the service check execution flag */
	execute_service_checks = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, MODATTR_NONE, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}



/* starts accepting passive service checks */
void start_accepting_passive_service_checks(void) {
	unsigned long attr = MODATTR_PASSIVE_CHECKS_ENABLED;

	/* bail out if we're already accepting passive services */
	if(accept_passive_service_checks == TRUE)
		return;

	/* set the attribute modified flag */
	modified_service_process_attributes |= attr;

	/* set the service check flag */
	accept_passive_service_checks = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, MODATTR_NONE, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}



/* stops accepting passive service checks */
void stop_accepting_passive_service_checks(void) {
	unsigned long attr = MODATTR_PASSIVE_CHECKS_ENABLED;

	/* bail out if we're already not accepting passive services */
	if(accept_passive_service_checks == FALSE)
		return;

	/* set the attribute modified flag */
	modified_service_process_attributes |= attr;

	/* set the service check flag */
	accept_passive_service_checks = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, MODATTR_NONE, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}



/* enables passive service checks for a particular service */
void enable_passive_service_checks(service *svc) {
	unsigned long attr = MODATTR_PASSIVE_CHECKS_ENABLED;

	/* no change */
	if(svc->accept_passive_checks == TRUE)
		return;

	/* set the attribute modified flag */
	svc->modified_attributes |= attr;

	/* set the passive check flag */
	svc->accept_passive_checks = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, svc, CMD_NONE, attr, svc->modified_attributes, NULL);
#endif

	/* update the status log with the service info */
	update_service_status(svc, FALSE);

	return;
	}



/* disables passive service checks for a particular service */
void disable_passive_service_checks(service *svc) {
	unsigned long attr = MODATTR_PASSIVE_CHECKS_ENABLED;

	/* no change */
	if(svc->accept_passive_checks == FALSE)
		return;

	/* set the attribute modified flag */
	svc->modified_attributes |= attr;

	/* set the passive check flag */
	svc->accept_passive_checks = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, svc, CMD_NONE, attr, svc->modified_attributes, NULL);
#endif

	/* update the status log with the service info */
	update_service_status(svc, FALSE);

	return;
	}



/* starts executing host checks */
void start_executing_host_checks(void) {
	unsigned long attr = MODATTR_ACTIVE_CHECKS_ENABLED;

	/* bail out if we're already executing hosts */
	if(execute_host_checks == TRUE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;

	/* set the host check execution flag */
	execute_host_checks = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, MODATTR_NONE, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}




/* stops executing host checks */
void stop_executing_host_checks(void) {
	unsigned long attr = MODATTR_ACTIVE_CHECKS_ENABLED;

	/* bail out if we're already not executing hosts */
	if(execute_host_checks == FALSE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;

	/* set the host check execution flag */
	execute_host_checks = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, MODATTR_NONE, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}



/* starts accepting passive host checks */
void start_accepting_passive_host_checks(void) {
	unsigned long attr = MODATTR_PASSIVE_CHECKS_ENABLED;

	/* bail out if we're already accepting passive hosts */
	if(accept_passive_host_checks == TRUE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;

	/* set the host check flag */
	accept_passive_host_checks = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, MODATTR_NONE, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}



/* stops accepting passive host checks */
void stop_accepting_passive_host_checks(void) {
	unsigned long attr = MODATTR_PASSIVE_CHECKS_ENABLED;

	/* bail out if we're already not accepting passive hosts */
	if(accept_passive_host_checks == FALSE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;

	/* set the host check flag */
	accept_passive_host_checks = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, MODATTR_NONE, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}



/* enables passive host checks for a particular host */
void enable_passive_host_checks(host *hst) {
	unsigned long attr = MODATTR_PASSIVE_CHECKS_ENABLED;

	/* no change */
	if(hst->accept_passive_checks == TRUE)
		return;

	/* set the attribute modified flag */
	hst->modified_attributes |= attr;

	/* set the passive check flag */
	hst->accept_passive_checks = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, hst, CMD_NONE, attr, hst->modified_attributes, NULL);
#endif

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	return;
	}



/* disables passive host checks for a particular host */
void disable_passive_host_checks(host *hst) {
	unsigned long attr = MODATTR_PASSIVE_CHECKS_ENABLED;

	/* no change */
	if(hst->accept_passive_checks == FALSE)
		return;

	/* set the attribute modified flag */
	hst->modified_attributes |= attr;

	/* set the passive check flag */
	hst->accept_passive_checks = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, hst, CMD_NONE, attr, hst->modified_attributes, NULL);
#endif

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	return;
	}


/* enables event handlers on a program-wide basis */
void start_using_event_handlers(void) {
	unsigned long attr = MODATTR_EVENT_HANDLER_ENABLED;

	/* no change */
	if(enable_event_handlers == TRUE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;
	modified_service_process_attributes |= attr;

	/* set the event handler flag */
	enable_event_handlers = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}


/* disables event handlers on a program-wide basis */
void stop_using_event_handlers(void) {
	unsigned long attr = MODATTR_EVENT_HANDLER_ENABLED;

	/* no change */
	if(enable_event_handlers == FALSE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;
	modified_service_process_attributes |= attr;

	/* set the event handler flag */
	enable_event_handlers = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}


/* enables the event handler for a particular service */
void enable_service_event_handler(service *svc) {
	unsigned long attr = MODATTR_EVENT_HANDLER_ENABLED;

	/* no change */
	if(svc->event_handler_enabled == TRUE)
		return;

	/* set the attribute modified flag */
	svc->modified_attributes |= attr;

	/* set the event handler flag */
	svc->event_handler_enabled = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, svc, CMD_NONE, attr, svc->modified_attributes, NULL);
#endif

	/* update the status log with the service info */
	update_service_status(svc, FALSE);

	return;
	}



/* disables the event handler for a particular service */
void disable_service_event_handler(service *svc) {
	unsigned long attr = MODATTR_EVENT_HANDLER_ENABLED;

	/* no change */
	if(svc->event_handler_enabled == FALSE)
		return;

	/* set the attribute modified flag */
	svc->modified_attributes |= attr;

	/* set the event handler flag */
	svc->event_handler_enabled = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, svc, CMD_NONE, attr, svc->modified_attributes, NULL);
#endif

	/* update the status log with the service info */
	update_service_status(svc, FALSE);

	return;
	}


/* enables the event handler for a particular host */
void enable_host_event_handler(host *hst) {
	unsigned long attr = MODATTR_EVENT_HANDLER_ENABLED;

	/* no change */
	if(hst->event_handler_enabled == TRUE)
		return;

	/* set the attribute modified flag */
	hst->modified_attributes |= attr;

	/* set the event handler flag */
	hst->event_handler_enabled = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, hst, CMD_NONE, attr, hst->modified_attributes, NULL);
#endif

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	return;
	}


/* disables the event handler for a particular host */
void disable_host_event_handler(host *hst) {
	unsigned long attr = MODATTR_EVENT_HANDLER_ENABLED;

	/* no change */
	if(hst->event_handler_enabled == FALSE)
		return;

	/* set the attribute modified flag */
	hst->modified_attributes |= attr;

	/* set the event handler flag */
	hst->event_handler_enabled = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, hst, CMD_NONE, attr, hst->modified_attributes, NULL);
#endif

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	return;
	}


/* disables checks of a particular host */
void disable_host_checks(host *hst) {
	unsigned long attr = MODATTR_ACTIVE_CHECKS_ENABLED;

	/* checks are already disabled */
	if(hst->checks_enabled == FALSE)
		return;

	/* set the attribute modified flag */
	hst->modified_attributes |= attr;

	/* set the host check flag */
	hst->checks_enabled = FALSE;
	hst->should_be_scheduled = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, hst, CMD_NONE, attr, hst->modified_attributes, NULL);
#endif

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	return;
	}


/* enables checks of a particular host */
void enable_host_checks(host *hst) {
	time_t preferred_time = 0L;
	time_t next_valid_time = 0L;
	unsigned long attr = MODATTR_ACTIVE_CHECKS_ENABLED;

	/* checks are already enabled */
	if(hst->checks_enabled == TRUE)
		return;

	/* set the attribute modified flag */
	hst->modified_attributes |= attr;

	/* set the host check flag */
	hst->checks_enabled = TRUE;
	hst->should_be_scheduled = TRUE;

	/* hosts with no check intervals don't get checked */
	if(hst->check_interval == 0)
		hst->should_be_scheduled = FALSE;

	/* schedule a check for right now (or as soon as possible) */
	time(&preferred_time);
	if(check_time_against_period(preferred_time, hst->check_period_ptr) == ERROR) {
		get_next_valid_time(preferred_time, &next_valid_time, hst->check_period_ptr);
		hst->next_check = next_valid_time;
		}
	else
		hst->next_check = preferred_time;

	/* schedule a check if we should */
	if(hst->should_be_scheduled == TRUE)
		schedule_host_check(hst, hst->next_check, CHECK_OPTION_NONE);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, hst, CMD_NONE, attr, hst->modified_attributes, NULL);
#endif

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	return;
	}



/* start obsessing over service check results */
void start_obsessing_over_service_checks(void) {
	unsigned long attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* no change */
	if(obsess_over_services == TRUE)
		return;

	/* set the attribute modified flag */
	modified_service_process_attributes |= attr;

	/* set the service obsession flag */
	obsess_over_services = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, MODATTR_NONE, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}



/* stop obsessing over service check results */
void stop_obsessing_over_service_checks(void) {
	unsigned long attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* no change */
	if(obsess_over_services == FALSE)
		return;

	/* set the attribute modified flag */
	modified_service_process_attributes |= attr;

	/* set the service obsession flag */
	obsess_over_services = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, MODATTR_NONE, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}



/* start obsessing over host check results */
void start_obsessing_over_host_checks(void) {
	unsigned long attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* no change */
	if(obsess_over_hosts == TRUE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;

	/* set the host obsession flag */
	obsess_over_hosts = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, MODATTR_NONE, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}



/* stop obsessing over host check results */
void stop_obsessing_over_host_checks(void) {
	unsigned long attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* no change */
	if(obsess_over_hosts == FALSE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;

	/* set the host obsession flag */
	obsess_over_hosts = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, MODATTR_NONE, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}



/* enables service freshness checking */
void enable_service_freshness_checks(void) {
	unsigned long attr = MODATTR_FRESHNESS_CHECKS_ENABLED;

	/* no change */
	if(check_service_freshness == TRUE)
		return;

	/* set the attribute modified flag */
	modified_service_process_attributes |= attr;

	/* set the freshness check flag */
	check_service_freshness = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, MODATTR_NONE, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}


/* disables service freshness checking */
void disable_service_freshness_checks(void) {
	unsigned long attr = MODATTR_FRESHNESS_CHECKS_ENABLED;

	/* no change */
	if(check_service_freshness == FALSE)
		return;

	/* set the attribute modified flag */
	modified_service_process_attributes |= attr;

	/* set the freshness check flag */
	check_service_freshness = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, MODATTR_NONE, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}


/* enables host freshness checking */
void enable_host_freshness_checks(void) {
	unsigned long attr = MODATTR_FRESHNESS_CHECKS_ENABLED;

	/* no change */
	if(check_host_freshness == TRUE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;

	/* set the freshness check flag */
	check_host_freshness = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, MODATTR_NONE, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}


/* disables host freshness checking */
void disable_host_freshness_checks(void) {
	unsigned long attr = MODATTR_FRESHNESS_CHECKS_ENABLED;

	/* no change */
	if(check_host_freshness == FALSE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;

	/* set the freshness check flag */
	check_host_freshness = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, MODATTR_NONE, modified_service_process_attributes, NULL);
#endif

	/* update the status log with the program info */
	update_program_status(FALSE);

	return;
	}


/* enable performance data on a program-wide basis */
void enable_performance_data(void) {
	unsigned long attr = MODATTR_PERFORMANCE_DATA_ENABLED;

	/* bail out if we're already set... */
	if(process_performance_data == TRUE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;
	modified_service_process_attributes |= attr;

	process_performance_data = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log */
	update_program_status(FALSE);

	return;
	}


/* disable performance data on a program-wide basis */
void disable_performance_data(void) {
	unsigned long attr = MODATTR_PERFORMANCE_DATA_ENABLED;

#	/* bail out if we're already set... */
	if(process_performance_data == FALSE)
		return;

	/* set the attribute modified flag */
	modified_host_process_attributes |= attr;
	modified_service_process_attributes |= attr;

	process_performance_data = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_program_data(NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE, attr, modified_host_process_attributes, attr, modified_service_process_attributes, NULL);
#endif

	/* update the status log */
	update_program_status(FALSE);

	return;
	}


/* start obsessing over a particular service */
void start_obsessing_over_service(service *svc) {
	unsigned long attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* no change */
	if(svc->obsess == TRUE)
		return;

	/* set the attribute modified flag */
	svc->modified_attributes |= attr;

	/* set the obsess over service flag */
	svc->obsess = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, svc, CMD_NONE, attr, svc->modified_attributes, NULL);
#endif

	/* update the status log with the service info */
	update_service_status(svc, FALSE);

	return;
	}


/* stop obsessing over a particular service */
void stop_obsessing_over_service(service *svc) {
	unsigned long attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* no change */
	if(svc->obsess == FALSE)
		return;

	/* set the attribute modified flag */
	svc->modified_attributes |= attr;

	/* set the obsess over service flag */
	svc->obsess = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, svc, CMD_NONE, attr, svc->modified_attributes, NULL);
#endif

	/* update the status log with the service info */
	update_service_status(svc, FALSE);

	return;
	}


/* start obsessing over a particular host */
void start_obsessing_over_host(host *hst) {
	unsigned long attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* no change */
	if(hst->obsess == TRUE)
		return;

	/* set the attribute modified flag */
	hst->modified_attributes |= attr;

	/* set the obsess flag */
	hst->obsess = TRUE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, hst, CMD_NONE, attr, hst->modified_attributes, NULL);
#endif

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	return;
	}


/* stop obsessing over a particular host */
void stop_obsessing_over_host(host *hst) {
	unsigned long attr = MODATTR_OBSESSIVE_HANDLER_ENABLED;

	/* no change */
	if(hst->obsess == FALSE)
		return;

	/* set the attribute modified flag */
	hst->modified_attributes |= attr;

	/* set the obsess over host flag */
	hst->obsess = FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, hst, CMD_NONE, attr, hst->modified_attributes, NULL);
#endif

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	return;
	}


/* sets the current notification number for a specific host */
void set_host_notification_number(host *hst, int num) {

	/* set the notification number */
	hst->current_notification_number = num;

	/* update the status log with the host info */
	update_host_status(hst, FALSE);

	return;
	}


/* sets the current notification number for a specific service */
void set_service_notification_number(service *svc, int num) {

	/* set the notification number */
	svc->current_notification_number = num;

	/* update the status log with the service info */
	update_service_status(svc, FALSE);

	return;
	}
