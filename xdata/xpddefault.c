/*****************************************************************************
 *
 * XPDDEFAULT.C - Default performance data routines
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


/*********** COMMON HEADER FILES ***********/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/macros.h"
#include "../include/nagios.h"
#include "../include/workers.h"
#include "xpddefault.h"


static command *host_perfdata_command_ptr = NULL;
static command *service_perfdata_command_ptr = NULL;
static command *host_perfdata_file_processing_command_ptr = NULL;
static command *service_perfdata_file_processing_command_ptr = NULL;
static FILE    *host_perfdata_fp = NULL;
static FILE    *service_perfdata_fp = NULL;
static int     host_perfdata_fd = -1;
static int     service_perfdata_fd = -1;


/******************************************************************/
/************** INITIALIZATION & CLEANUP FUNCTIONS ****************/
/******************************************************************/

/* initializes performance data */
int xpddefault_initialize_performance_data(const char *cfgfile) {
	char *buffer = NULL;
	char *temp_buffer = NULL;
	char *temp_command_name = NULL;
	command *temp_command = NULL;
	time_t current_time;
	nagios_macros *mac;

	mac = get_global_macros();
	time(&current_time);

	/* reset vars */
	host_perfdata_command_ptr = NULL;
	service_perfdata_command_ptr = NULL;
	host_perfdata_file_processing_command_ptr = NULL;
	service_perfdata_file_processing_command_ptr = NULL;

	/* make sure we have some templates defined */
	if(host_perfdata_file_template == NULL)
		host_perfdata_file_template = (char *)strdup(DEFAULT_HOST_PERFDATA_FILE_TEMPLATE);
	if(service_perfdata_file_template == NULL)
		service_perfdata_file_template = (char *)strdup(DEFAULT_SERVICE_PERFDATA_FILE_TEMPLATE);

	/* process special chars in templates */
	xpddefault_preprocess_file_templates(host_perfdata_file_template);
	xpddefault_preprocess_file_templates(service_perfdata_file_template);

	/* open the performance data files */
	xpddefault_open_host_perfdata_file();
	xpddefault_open_service_perfdata_file();

	/* verify that performance data commands are valid */
	if(host_perfdata_command != NULL) {

		temp_buffer = (char *)strdup(host_perfdata_command);

		/* get the command name, leave any arguments behind */
		temp_command_name = my_strtok(temp_buffer, "!");

		if((temp_command = find_command(temp_command_name)) == NULL) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Host performance command '%s' was not found - host performance data will not be processed!\n", temp_command_name);

			my_free(host_perfdata_command);
			}

		my_free(temp_buffer);

		/* save the command pointer for later */
		host_perfdata_command_ptr = temp_command;
		}
	if(service_perfdata_command != NULL) {

		temp_buffer = (char *)strdup(service_perfdata_command);

		/* get the command name, leave any arguments behind */
		temp_command_name = my_strtok(temp_buffer, "!");

		if((temp_command = find_command(temp_command_name)) == NULL) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Service performance command '%s' was not found - service performance data will not be processed!\n", temp_command_name);

			my_free(service_perfdata_command);
			}

		/* free memory */
		my_free(temp_buffer);

		/* save the command pointer for later */
		service_perfdata_command_ptr = temp_command;
		}
	if(host_perfdata_file_processing_command != NULL) {

		temp_buffer = (char *)strdup(host_perfdata_file_processing_command);

		/* get the command name, leave any arguments behind */
		temp_command_name = my_strtok(temp_buffer, "!");

		if((temp_command = find_command(temp_command_name)) == NULL) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Host performance file processing command '%s' was not found - host performance data file will not be processed!\n", temp_command_name);

			my_free(host_perfdata_file_processing_command);
			}

		/* free memory */
		my_free(temp_buffer);

		/* save the command pointer for later */
		host_perfdata_file_processing_command_ptr = temp_command;
		}
	if(service_perfdata_file_processing_command != NULL) {

		temp_buffer = (char *)strdup(service_perfdata_file_processing_command);

		/* get the command name, leave any arguments behind */
		temp_command_name = my_strtok(temp_buffer, "!");

		if((temp_command = find_command(temp_command_name)) == NULL) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Service performance file processing command '%s' was not found - service performance data file will not be processed!\n", temp_command_name);

			my_free(service_perfdata_file_processing_command);
			}

		/* save the command pointer for later */
		service_perfdata_file_processing_command_ptr = temp_command;
		}

	/* periodically process the host perfdata file */
	if(host_perfdata_file_processing_interval > 0 && host_perfdata_file_processing_command != NULL)
		schedule_new_event(EVENT_USER_FUNCTION, TRUE, current_time + host_perfdata_file_processing_interval, TRUE, host_perfdata_file_processing_interval, NULL, TRUE, (void *)xpddefault_process_host_perfdata_file, NULL, 0);

	/* periodically process the service perfdata file */
	if(service_perfdata_file_processing_interval > 0 && service_perfdata_file_processing_command != NULL)
		schedule_new_event(EVENT_USER_FUNCTION, TRUE, current_time + service_perfdata_file_processing_interval, TRUE, service_perfdata_file_processing_interval, NULL, TRUE, (void *)xpddefault_process_service_perfdata_file, NULL, 0);

	/* save the host perf data file macro */
	my_free(mac->x[MACRO_HOSTPERFDATAFILE]);
	if(host_perfdata_file != NULL) {
		if((mac->x[MACRO_HOSTPERFDATAFILE] = (char *)strdup(host_perfdata_file)))
			strip(mac->x[MACRO_HOSTPERFDATAFILE]);
		}

	/* save the service perf data file macro */
	my_free(mac->x[MACRO_SERVICEPERFDATAFILE]);
	if(service_perfdata_file != NULL) {
		if((mac->x[MACRO_SERVICEPERFDATAFILE] = (char *)strdup(service_perfdata_file)))
			strip(mac->x[MACRO_SERVICEPERFDATAFILE]);
		}

	/* free memory */
	my_free(temp_buffer);
	my_free(buffer);

	return OK;
	}



/* cleans up performance data */
int xpddefault_cleanup_performance_data(void) {

	/* free memory */
	my_free(host_perfdata_command);
	my_free(service_perfdata_command);
	my_free(host_perfdata_file_template);
	my_free(service_perfdata_file_template);
	my_free(host_perfdata_file);
	my_free(service_perfdata_file);
	my_free(host_perfdata_file_processing_command);
	my_free(service_perfdata_file_processing_command);

	/* close the files */
	xpddefault_close_host_perfdata_file();
	xpddefault_close_service_perfdata_file();

	return OK;
	}



/******************************************************************/
/****************** PERFORMANCE DATA FUNCTIONS ********************/
/******************************************************************/


/* updates service performance data */
int xpddefault_update_service_performance_data(service *svc) {
	nagios_macros mac;
	host *hst;

	/*
	 * bail early if we've got nothing to do so we don't spend a lot
	 * of time calculating macros that never get used
	 * on distributed setups, empty perfdata results are required, so
	 * only drop out if demanded via configs.
	*/
	if(service_perfdata_process_empty_results==FALSE){
	        if(!svc || !svc->perf_data || !*svc->perf_data) {
		       return OK;
		}
	        if((!service_perfdata_fp || !service_perfdata_file_template) && !service_perfdata_command) {
		       return OK;
		}

	}
	/*
	 * we know we've got some work to do, so grab the necessary
	 * macros and get busy
	 */
	memset(&mac, 0, sizeof(mac));
	hst = find_host(svc->host_name);
	grab_host_macros_r(&mac, hst);
	grab_service_macros_r(&mac, svc);

	/* run the performance data command */
	xpddefault_run_service_performance_data_command(&mac, svc);

	/* get rid of used memory we won't need anymore */
	clear_argv_macros_r(&mac);

	/* update the performance data file */
	xpddefault_update_service_performance_data_file(&mac, svc);

	/* now free() it all */
	clear_volatile_macros_r(&mac);

	return OK;
	}


/* updates host performance data */
int xpddefault_update_host_performance_data(host *hst) {
	nagios_macros mac;


	/*
	 * bail early if we've got nothing to do so we don't spend a lot
	 * of time calculating macros that never get used
	 * on distributed setups, empty perfdata results are required, so
	 * only drop out if demanded via configs.
	 */
	if(host_perfdata_process_empty_results==FALSE){
		if(!hst || !hst->perf_data || !*hst->perf_data) {
			return OK;
			}
		if((!host_perfdata_fp || !host_perfdata_file_template) && !host_perfdata_command) {
			return OK;
			}
		}

	/* set up macros and get to work */
	memset(&mac, 0, sizeof(mac));
	grab_host_macros_r(&mac, hst);

	/* run the performance data command */
	xpddefault_run_host_performance_data_command(&mac, hst);

	/* no more commands to run, so we won't need this any more */
	clear_argv_macros_r(&mac);

	/* update the performance data file */
	xpddefault_update_host_performance_data_file(&mac, hst);

	/* free() all */
	clear_volatile_macros_r(&mac);

	return OK;
	}




/******************************************************************/
/************** PERFORMANCE DATA COMMAND FUNCTIONS ****************/
/******************************************************************/


/* runs the service performance data command */
int xpddefault_run_service_performance_data_command(nagios_macros *mac, service *svc) {
	char *raw_command_line = NULL;
	char *processed_command_line = NULL;
	int result = OK;
	int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "run_service_performance_data_command()\n");

	if(svc == NULL)
		return ERROR;

	/* we don't have a command */
	if(service_perfdata_command == NULL)
		return OK;

	/* get the raw command line */
	get_raw_command_line_r(mac, service_perfdata_command_ptr, service_perfdata_command, &raw_command_line, macro_options);
	if(raw_command_line == NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA, 2, "Raw service performance data command line: %s\n", raw_command_line);

	/* process any macros in the raw command line */
	process_macros_r(mac, raw_command_line, &processed_command_line, macro_options);
	my_free(raw_command_line);
	if(processed_command_line == NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA, 2, "Processed service performance data command line: %s\n", processed_command_line);

	/* run the command */
	wproc_run(WPJOB_SVC_PERFDATA, processed_command_line, perfdata_timeout, NULL);

	/* free memory */
	my_free(processed_command_line);

	return result;
	}


/* runs the host performance data command */
int xpddefault_run_host_performance_data_command(nagios_macros *mac, host *hst) {
	char *raw_command_line = NULL;
	char *processed_command_line = NULL;
	int result = OK;
	int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "run_host_performance_data_command()\n");

	if(hst == NULL)
		return ERROR;

	/* we don't have a command */
	if(host_perfdata_command == NULL)
		return OK;

	/* get the raw command line */
	get_raw_command_line_r(mac, host_perfdata_command_ptr, host_perfdata_command, &raw_command_line, macro_options);
	if(raw_command_line == NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA, 2, "Raw host performance data command line: %s\n", raw_command_line);

	/* process any macros in the raw command line */
	process_macros_r(mac, raw_command_line, &processed_command_line, macro_options);
	my_free(raw_command_line);
	if (!processed_command_line)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA, 2, "Processed host performance data command line: %s\n", processed_command_line);

	/* run the command */
	wproc_run(WPJOB_HOST_PERFDATA, processed_command_line, perfdata_timeout, NULL);

	/* free memory */
	my_free(processed_command_line);

	return result;
	}



/******************************************************************/
/**************** FILE PERFORMANCE DATA FUNCTIONS *****************/
/******************************************************************/

/* open the host performance data file for writing */
int xpddefault_open_host_perfdata_file(void) {

	if(host_perfdata_file != NULL) {

		if(host_perfdata_file_pipe == TRUE) {
			/* must open read-write to avoid failure if the other end isn't ready yet */
			host_perfdata_fd = open(host_perfdata_file, O_NONBLOCK | O_RDWR | O_CREAT, 0644);
			host_perfdata_fp = fdopen(host_perfdata_fd, "w");
			}
		else
			host_perfdata_fp = fopen(host_perfdata_file, (host_perfdata_file_append == TRUE) ? "a" : "w");

		if(host_perfdata_fp == NULL) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: File '%s' could not be opened - host performance data will not be written to file!\n", host_perfdata_file);

			return ERROR;
			}
		}

	return OK;
	}


/* open the service performance data file for writing */
int xpddefault_open_service_perfdata_file(void) {

	if(service_perfdata_file != NULL) {
		if(service_perfdata_file_pipe == TRUE) {
			/* must open read-write to avoid failure if the other end isn't ready yet */
			service_perfdata_fd = open(service_perfdata_file, O_NONBLOCK | O_RDWR);
			service_perfdata_fp = fdopen(service_perfdata_fd, "w");
			}
		else
			service_perfdata_fp = fopen(service_perfdata_file, (service_perfdata_file_append == TRUE) ? "a" : "w");

		if(service_perfdata_fp == NULL) {

			logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: File '%s' could not be opened - service performance data will not be written to file!\n", service_perfdata_file);

			return ERROR;
			}
		}

	return OK;
	}


/* close the host performance data file */
int xpddefault_close_host_perfdata_file(void) {

	if(host_perfdata_fp != NULL)
		fclose(host_perfdata_fp);
	if(host_perfdata_fd >= 0) {
		close(host_perfdata_fd);
		host_perfdata_fd = -1;
		}

	return OK;
	}


/* close the service performance data file */
int xpddefault_close_service_perfdata_file(void) {

	if(service_perfdata_fp != NULL)
		fclose(service_perfdata_fp);
	if(service_perfdata_fd >= 0) {
		close(service_perfdata_fd);
		service_perfdata_fd = -1;
		}

	return OK;
	}


/* processes delimiter characters in templates */
int xpddefault_preprocess_file_templates(char *template) {
	char *tempbuf;
	unsigned int x, y;

	if(template == NULL)
		return OK;

	/* allocate temporary buffer */
	tempbuf = (char *)malloc(strlen(template) + 1);
	if(tempbuf == NULL)
		return ERROR;
	strcpy(tempbuf, "");

	for(x = 0, y = 0; x < strlen(template); x++, y++) {
		if(template[x] == '\\') {
			if(template[x + 1] == 't') {
				tempbuf[y] = '\t';
				x++;
				}
			else if(template[x + 1] == 'r') {
				tempbuf[y] = '\r';
				x++;
				}
			else if(template[x + 1] == 'n') {
				tempbuf[y] = '\n';
				x++;
				}
			else
				tempbuf[y] = template[x];
			}
		else
			tempbuf[y] = template[x];
		}
	tempbuf[y] = '\x0';

	strcpy(template, tempbuf);
	my_free(tempbuf);

	return OK;
	}


/* updates service performance data file */
int xpddefault_update_service_performance_data_file(nagios_macros *mac, service *svc) {
	char *raw_output = NULL;
	char *processed_output = NULL;
	int result = OK;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "update_service_performance_data_file()\n");

	if(svc == NULL)
		return ERROR;

	/* we don't have a file to write to*/
	if(service_perfdata_fp == NULL || service_perfdata_file_template == NULL)
		return OK;

	/* get the raw line to write */
	raw_output = (char *)strdup(service_perfdata_file_template);

	log_debug_info(DEBUGL_PERFDATA, 2, "Raw service performance data file output: %s\n", raw_output);

	/* process any macros in the raw output line */
	process_macros_r(mac, raw_output, &processed_output, 0);
	if(processed_output == NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA, 2, "Processed service performance data file output: %s\n", processed_output);

	/* write to host performance data file */
	fputs(processed_output, service_perfdata_fp);
	fputc('\n', service_perfdata_fp);
	fflush(service_perfdata_fp);

	/* free memory */
	my_free(raw_output);
	my_free(processed_output);

	return result;
	}


/* updates host performance data file */
int xpddefault_update_host_performance_data_file(nagios_macros *mac, host *hst) {
	char *raw_output = NULL;
	char *processed_output = NULL;
	int result = OK;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "update_host_performance_data_file()\n");

	if(hst == NULL)
		return ERROR;

	/* we don't have a host perfdata file */
	if(host_perfdata_fp == NULL || host_perfdata_file_template == NULL)
		return OK;

	/* get the raw output */
	raw_output = (char *)strdup(host_perfdata_file_template);

	log_debug_info(DEBUGL_PERFDATA, 2, "Raw host performance file output: %s\n", raw_output);

	/* process any macros in the raw output */
	process_macros_r(mac, raw_output, &processed_output, 0);
	if(processed_output == NULL)
		return ERROR;

	log_debug_info(DEBUGL_PERFDATA, 2, "Processed host performance data file output: %s\n", processed_output);

	/* write to host performance data file */
	fputs(processed_output, host_perfdata_fp);
	fputc('\n', host_perfdata_fp);
	fflush(host_perfdata_fp);

	/* free memory */
	my_free(raw_output);
	my_free(processed_output);

	return result;
	}


/* periodically process the host perf data file */
int xpddefault_process_host_perfdata_file(void) {
	char *raw_command_line = NULL;
	char *processed_command_line = NULL;
	int early_timeout = FALSE;
	double exectime = 0.0;
	int result = OK;
	int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;
	nagios_macros mac;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "process_host_perfdata_file()\n");

	/* we don't have a command */
	if(host_perfdata_file_processing_command == NULL)
		return OK;

	/* init macros */
	memset(&mac, 0, sizeof(mac));

	/* get the raw command line */
	get_raw_command_line_r(&mac, host_perfdata_file_processing_command_ptr, host_perfdata_file_processing_command, &raw_command_line, macro_options);
	if(raw_command_line == NULL) {
		clear_volatile_macros_r(&mac);
		return ERROR;
		}

	log_debug_info(DEBUGL_PERFDATA, 2, "Raw host performance data file processing command line: %s\n", raw_command_line);

	/* process any macros in the raw command line */
	process_macros_r(&mac, raw_command_line, &processed_command_line, macro_options);
	my_free(raw_command_line);
	if(processed_command_line == NULL) {
		clear_volatile_macros_r(&mac);
		return ERROR;
		}

	log_debug_info(DEBUGL_PERFDATA, 2, "Processed host performance data file processing command line: %s\n", processed_command_line);

	/* close the performance data file */
	xpddefault_close_host_perfdata_file();

	/* run the command */
	my_system_r(&mac, processed_command_line, perfdata_timeout, &early_timeout, &exectime, NULL, 0);
	clear_volatile_macros_r(&mac);

	/* re-open the performance data file */
	xpddefault_open_host_perfdata_file();

	/* check to see if the command timed out */
	if(early_timeout == TRUE)
		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Host performance data file processing command '%s' timed out after %d seconds\n", processed_command_line, perfdata_timeout);


	/* free memory */
	my_free(processed_command_line);

	return result;
	}


/* periodically process the service perf data file */
int xpddefault_process_service_perfdata_file(void) {
	char *raw_command_line = NULL;
	char *processed_command_line = NULL;
	int early_timeout = FALSE;
	double exectime = 0.0;
	int result = OK;
	int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;
	nagios_macros mac;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "process_service_perfdata_file()\n");

	/* we don't have a command */
	if(service_perfdata_file_processing_command == NULL)
		return OK;

	/* init macros */
	memset(&mac, 0, sizeof(mac));

	/* get the raw command line */
	get_raw_command_line_r(&mac, service_perfdata_file_processing_command_ptr, service_perfdata_file_processing_command, &raw_command_line, macro_options);
	if(raw_command_line == NULL) {
		clear_volatile_macros_r(&mac);
		return ERROR;
		}

	log_debug_info(DEBUGL_PERFDATA, 2, "Raw service performance data file processing command line: %s\n", raw_command_line);

	/* process any macros in the raw command line */
	process_macros_r(&mac, raw_command_line, &processed_command_line, macro_options);
	my_free(raw_command_line);
	if(processed_command_line == NULL) {
		clear_volatile_macros_r(&mac);
		return ERROR;
		}

	log_debug_info(DEBUGL_PERFDATA, 2, "Processed service performance data file processing command line: %s\n", processed_command_line);

	/* close the performance data file */
	xpddefault_close_service_perfdata_file();

	/* run the command */
	my_system_r(&mac, processed_command_line, perfdata_timeout, &early_timeout, &exectime, NULL, 0);

	/* re-open the performance data file */
	xpddefault_open_service_perfdata_file();

	clear_volatile_macros_r(&mac);

	/* check to see if the command timed out */
	if(early_timeout == TRUE)
		logit(NSLOG_RUNTIME_WARNING, TRUE, "Warning: Service performance data file processing command '%s' timed out after %d seconds\n", processed_command_line, perfdata_timeout);

	/* free memory */
	my_free(processed_command_line);

	return result;
	}
