/*****************************************************************************
 *
 * NAGIOS.C - Core Program Code For Nagios
 *
 * Program: Nagios
 * Version: 1.0b1
 * License: GPL
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 *
 * First Written:   01-28-1999 (start of development)
 * Last Modified:   04-20-2002
 *
 * Description:
 *
 * Nagios is a network monitoring tool that will check hosts and services
 * that you specify.  It has the ability to notify contacts via email, pager,
 * or other user-defined methods when a service or host goes down and
 * recovers.  Service and host monitoring is done through the use of external
 * plugins which can be developed independently of Nagios.
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "../common/config.h"
#include "../common/common.h"
#include "../common/objects.h"
#include "../common/comments.h"
#include "../common/downtime.h"
#include "../common/statusdata.h"
#include "nagios.h"
#include "sretention.h"
#include "perfdata.h"


#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

/******** BEGIN EMBEDDED PERL INTERPRETER DECLARATIONS ********/

#ifdef EMBEDDEDPERL 
#include <EXTERN.h>
#include <perl.h>
#include <fcntl.h>

/* include PERL xs_init code for module and C library support */

#if defined(__cplusplus) && !defined(PERL_OBJECT)
#define is_cplusplus
#endif

#ifdef is_cplusplus
extern "C" {
#endif

#ifdef PERL_OBJECT
#define NO_XSLOCKS
#include <XSUB.h>
#include "win32iop.h"
#include <perlhost.h>
#endif
#ifdef is_cplusplus
}
#  ifndef EXTERN_C
#    define EXTERN_C extern "C"
#  endif
#else
#  ifndef EXTERN_C
#    define EXTERN_C extern
#  endif
#endif
 
EXTERN_C void xs_init _((void));

EXTERN_C void boot_DynaLoader _((CV* cv));

EXTERN_C void
xs_init(void)
{
	char *file = __FILE__;
#ifdef THREADEDPERL
	dTHX;
#endif
	dXSUB_SYS;

	/* DynaLoader is a special case */
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

 static PerlInterpreter *perl = NULL;

#endif

/******** END EMBEDDED PERL INTERPRETER DECLARATIONS ********/


char		config_file[MAX_FILENAME_LENGTH]="";
char		log_file[MAX_FILENAME_LENGTH]="";
char            command_file[MAX_FILENAME_LENGTH]="";
char            temp_file[MAX_FILENAME_LENGTH]="";
char            comment_file[MAX_FILENAME_LENGTH]="";
char            lock_file[MAX_FILENAME_LENGTH]="";
char            log_archive_path[MAX_FILENAME_LENGTH]="";
char            auth_file[MAX_FILENAME_LENGTH]="";  /**** EMBEDDED PERL INTERPRETER AUTH FILE ****/

char            *nagios_user=NULL;
char            *nagios_group=NULL;

char            *macro_contact_name=NULL;
char            *macro_contact_alias=NULL;
char	        *macro_host_name=NULL;
char	        *macro_host_alias=NULL;
char	        *macro_host_address=NULL;
char	        *macro_service_description=NULL;
char	        *macro_service_state=NULL;
char	        *macro_date_time[7]={NULL,NULL,NULL,NULL,NULL,NULL,NULL};
char	        *macro_output=NULL;
char	        *macro_perfdata=NULL;
char            *macro_contact_email=NULL;
char            *macro_contact_pager=NULL;
char            *macro_admin_email=NULL;
char            *macro_admin_pager=NULL;
char            *macro_host_state=NULL;
char            *macro_state_type=NULL;
char            *macro_current_service_attempt=NULL;
char            *macro_current_host_attempt=NULL;
char            *macro_notification_type=NULL;
char            *macro_notification_number=NULL;
char            *macro_execution_time=NULL;
char            *macro_latency=NULL;
char            *macro_argv[MAX_COMMAND_ARGUMENTS];
char            *macro_user[MAX_USER_MACROS];

char            *global_host_event_handler=NULL;
char            *global_service_event_handler=NULL;

char            *ocsp_command=NULL;

int		use_syslog=DEFAULT_USE_SYSLOG;
int             log_notifications=DEFAULT_NOTIFICATION_LOGGING;
int             log_service_retries=DEFAULT_LOG_SERVICE_RETRIES;
int             log_host_retries=DEFAULT_LOG_HOST_RETRIES;
int             log_event_handlers=DEFAULT_LOG_EVENT_HANDLERS;
int             log_initial_states=DEFAULT_LOG_INITIAL_STATES;
int             log_external_commands=DEFAULT_LOG_EXTERNAL_COMMANDS;
int             log_passive_checks=DEFAULT_LOG_PASSIVE_CHECKS;

unsigned long   logging_options=0;
unsigned long   syslog_options=0;

int             service_check_timeout=DEFAULT_SERVICE_CHECK_TIMEOUT;
int             host_check_timeout=DEFAULT_HOST_CHECK_TIMEOUT;
int             event_handler_timeout=DEFAULT_EVENT_HANDLER_TIMEOUT;
int             notification_timeout=DEFAULT_NOTIFICATION_TIMEOUT;
int             ocsp_timeout=DEFAULT_OCSP_TIMEOUT;

int             sleep_time=DEFAULT_SLEEP_TIME;
int             interval_length=DEFAULT_INTERVAL_LENGTH;
int             inter_check_delay_method=ICD_SMART;
int             interleave_factor_method=ILF_SMART;

int             command_check_interval=DEFAULT_COMMAND_CHECK_INTERVAL;
int             service_check_reaper_interval=DEFAULT_SERVICE_REAPER_INTERVAL;
int             freshness_check_interval=DEFAULT_FRESHNESS_CHECK_INTERVAL;

int             non_parallelized_check_running=FALSE;

int             check_external_commands=DEFAULT_CHECK_EXTERNAL_COMMANDS;
int             check_orphaned_services=DEFAULT_CHECK_ORPHANED_SERVICES;
int             check_service_freshness=DEFAULT_CHECK_SERVICE_FRESHNESS;

time_t          last_command_check=0L;
time_t          last_log_rotation=0L;

int             use_aggressive_host_checking=DEFAULT_AGGRESSIVE_HOST_CHECKING;

int             soft_state_dependencies=FALSE;

int             retain_state_information=FALSE;
int             retention_update_interval=DEFAULT_RETENTION_UPDATE_INTERVAL;
int             use_retained_program_state=TRUE;

int             log_rotation_method=LOG_ROTATION_NONE;

int             sigshutdown=FALSE;
int             sigrestart=FALSE;

int             restarting=FALSE;

int             verify_config=FALSE;
int             test_scheduling=FALSE;

int             daemon_mode=FALSE;

int             ipc_pipe[2];

int             max_parallel_service_checks=DEFAULT_MAX_PARALLEL_SERVICE_CHECKS;
int             currently_running_service_checks=0;

time_t          program_start=0L;
int             nagios_pid=0;
int             enable_notifications=TRUE;
int             execute_service_checks=TRUE;
int             accept_passive_service_checks=TRUE;
int             enable_event_handlers=TRUE;
int             obsess_over_services=FALSE;
int             enable_failure_prediction=TRUE;

int             aggregate_status_updates=TRUE;
int             status_update_interval=DEFAULT_STATUS_UPDATE_INTERVAL;

int             time_change_threshold=DEFAULT_TIME_CHANGE_THRESHOLD;

int             process_performance_data=DEFAULT_PROCESS_PERFORMANCE_DATA;

int             enable_flap_detection=DEFAULT_ENABLE_FLAP_DETECTION;

double          low_service_flap_threshold=DEFAULT_LOW_SERVICE_FLAP_THRESHOLD;
double          high_service_flap_threshold=DEFAULT_HIGH_SERVICE_FLAP_THRESHOLD;
double          low_host_flap_threshold=DEFAULT_LOW_HOST_FLAP_THRESHOLD;
double          high_host_flap_threshold=DEFAULT_HIGH_HOST_FLAP_THRESHOLD;

int             date_format=DATE_FORMAT_US;

int             command_file_fd;
FILE            *command_file_fp;
int             command_file_created=FALSE;


sched_info scheduling_info;

extern contact	       *contact_list;
extern contactgroup    *contactgroup_list;
extern host	       *host_list;
extern hostgroup       *hostgroup_list;
extern service         *service_list;
extern timed_event     *event_list_high;
extern timed_event     *event_list_low;
extern command         *command_list;
extern timeperiod      *timeperiod_list;
extern serviceescalation *serviceescalation_list;
extern hostgroupescalation *hostgroupescalation_list;

timed_event *event_list_low=NULL;
timed_event *event_list_high=NULL;

notification    *notification_list;

service_message svc_msg;



/* Following main() declaration required by older versions of Perl ut 5.00503 */
#ifdef EMBEDDEDPERL
int main(int argc, char **argv, char **env){
#else
int main(int argc, char **argv){
#endif
	int result;
	int error=FALSE;
	char buffer[MAX_INPUT_BUFFER];
	int display_license=FALSE;
	int display_help=FALSE;
	int create_status_data=FALSE;
	int display_modules=FALSE;
	int c;

#ifdef EMBEDDEDPERL
	char *embedding[] = { "", P1LOC };
	int exitstatus = 0;
#endif

#ifdef HAVE_GETOPT_H
	int option_index=0;
	static struct option long_options[]=
	{
		{"help",no_argument,0,'h'},
		{"version",no_argument,0,'V'},
		{"modules",no_argument,0,'m'},
		{"license",no_argument,0,'V'},
		{"verify",no_argument,0,'v'},
		{"daemon",no_argument,0,'d'},
		{0,0,0,0}
	};
#endif

	/* make sure we have the correct number of command line arguments */
	if(argc<2)
		error=TRUE;


	/* get all command line arguments */
	while(1){

#ifdef HAVE_GETOPT_H
		c=getopt_long(argc,argv,"+hVvdscm",long_options,&option_index);
#else
		c=getopt(argc,argv,"+hVvdscm");
#endif

		if(c==-1 || c==EOF)
			break;

		switch(c){
			
		case '?': /* usage */
		case 'h':
			display_help=TRUE;
			break;

		case 'm':
			display_modules=TRUE;
			break;

		case 'V': /* version */
			display_license=TRUE;
			break;

		case 'v': /* verify */
			verify_config=TRUE;
			break;

		case 's': /* scheduling check */
			test_scheduling=TRUE;
			break;

		case 'c': /* create status data */
			create_status_data=TRUE;
			break;

		case 'd': /* daemon mode */
			daemon_mode=TRUE;
			break;

		default:
			break;
		        }

	        }

	if(daemon_mode==FALSE){
		printf("\nNagios %s\n",PROGRAM_VERSION);
		printf("Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)\n");
		printf("Last Modified: %s\n",PROGRAM_MODIFICATION_DATE);
		printf("License: GPL\n\n");
	        }

	/* just display what modules are compiled in */
	if(display_modules==TRUE){
		display_xdata_modules();
		exit(OK);
	        }

	/* just display the license */
	if(display_license==TRUE){

		printf("This program is free software; you can redistribute it and/or modify\n");
		printf("it under the terms of the GNU General Public License as published by\n");
		printf("the Free Software Foundation; either version 2 of the License, or\n");
		printf("(at your option) any later version.\n\n");
		printf("This program is distributed in the hope that it will be useful,\n");
		printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
		printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
		printf("GNU General Public License for more details.\n\n");
		printf("You should have received a copy of the GNU General Public License\n");
		printf("along with this program; if not, write to the Free Software\n");
		printf("Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n\n");

		exit(OK);
	        }

	/* make sure we got the main config file on the command line... */
	if(optind>=argc)
		error=TRUE;

	/* if there are no command line options (or if we encountered an error), print usage */
	if(error==TRUE || display_help==TRUE){

		printf("Usage: %s [option] <main_config_file>\n",argv[0]);
		printf("\n");
		printf("Options:\n");
		printf("\n");
		printf("  -v   Reads all data in the configuration files and performs a basic\n");
		printf("       verification/sanity check.  Always make sure you verify your\n");
		printf("       config data before (re)starting Nagios.\n");
		printf("\n");
		printf("  -s   Shows projected/recommended service check scheduling information.\n");
		printf("\n");
		printf("  -c   When used in conjunction with the -s option, this option will cause\n");
		printf("       the status data to be updated with scheduling information.  It is\n");
		printf("       not recommended that you use this option while Nagios is running, as\n");
		printf("       the information in the status log will be temporarily overwritten.\n");
		printf("       Provided as an aid to help you understand initial check scheduling.\n");
		printf("\n");
		printf("  -d   Starts Nagios in daemon mode (instead of as a foreground process).\n");
		printf("       This is the recommended way of starting Nagios for normal operation.\n");
		printf("\n");
		printf("Visit the Nagios website at http://www.nagios.org for information on known\n");
		printf("bugs, new releases, online documentation, and information on subscribing to\n");
		printf("the mailing lists.\n");
		printf("\n");

		exit(ERROR);
		}


	/* config file is last argument specified */
	strncpy(config_file,argv[optind],sizeof(config_file));
	config_file[sizeof(config_file)-1]='\x0';

	/* make sure the config file uses an absolute path */
	if(config_file[0]!='/'){

		/* save the name of the config file */
		strncpy(buffer,config_file,sizeof(buffer));
		buffer[sizeof(buffer)-1]='\x0';

		/* get absolute path of current working directory */
		strcpy(config_file,"");
		getcwd(config_file,sizeof(config_file));

		/* append a forward slash */
		strncat(config_file,"/",sizeof(config_file)-2);
		config_file[sizeof(config_file)-1]='\x0';

		/* append the config file to the path */
		strncat(config_file,buffer,sizeof(config_file)-strlen(config_file)-1);
		config_file[sizeof(config_file)-1]='\x0';
	        }


	/* we're just verifying the configuration... */
	if(verify_config==TRUE){

		/* reset program variables */
		reset_variables();

		printf("Reading configuration data...\n\n");

		/* read in the configuration files (main config file and all host config files) */
		result=read_all_config_data(config_file);

		/* there was a problem reading the config files */
		if(result!=OK){

			/* if the config filename looks fishy, warn the user */
			if(!strstr(config_file,"nagios.cfg")){
				printf("\n***> The name of the main configuration file looks suspicious...\n");
				printf("\n");
				printf("     Make sure you are specifying the name of the MAIN configuration file on\n");
				printf("     the command line and not the name of the host configuration file.  The\n");
				printf("     main configuration file is typically '/usr/local/nagios/etc/nagios.cfg'\n");
	                        }

			printf("\n***> One or more problems was encountered while processing the config files...\n");
			printf("\n");
			printf("     Check your configuration file(s) to ensure that they contain valid\n");
			printf("     directives and data defintions.  If you are upgrading from a previous\n");
			printf("     version of Nagios, you should be aware that some variables/definitions\n");
			printf("     may have been removed or modified in this version.  Make sure to read\n");
			printf("     the HTML documentation on the main and host config files, as well as the\n");
			printf("     'Whats New' section to find out what has changed.\n\n");
		        }

		/* the config files were okay, so run the pre-flight check */
		else{

			printf("Running pre-flight check on configuration data...\n\n");

			/* run the pre-flight check to make sure things look okay... */
			result=pre_flight_check();

			if(result==OK)
				printf("\nThings look okay - No serious problems were detected during the pre-flight check\n");
			else{
				printf("\n***> One or more problems was encountered while running the pre-flight check...\n");
				printf("\n");
				printf("     Check your configuration file(s) to ensure that they contain valid\n");
				printf("     directives and data defintions.  If you are upgrading from a previous\n");
				printf("     version of Nagios, you should be aware that some variables/definitions\n");
				printf("     may have been removed or modified in this version.  Make sure to read\n");
				printf("     the HTML documentation on the main and host config files, as well as the\n");
				printf("     'Whats New' section to find out what has changed.\n\n");
			        }
		        }

		/* clean up after ourselves */
		cleanup();

		/* exit */
		exit(result);
	        }


	/* we're just testing scheduling... */
	else if(test_scheduling==TRUE){

		/* reset program variables */
		reset_variables();

		/* get program (re)start time */
		program_start=time(NULL);

		/* get PID */
		nagios_pid=(int)getpid();

		/* read in the configuration files (main config file and all host config files) */
		if(read_all_config_data(config_file)!=OK){

			printf("***> One or more problems was encountered while reading configuration data...\n");

			cleanup();
			exit(ERROR);
	                }

		/* run the pre-flight check to make sure everything looks okay*/
		if(pre_flight_check()!=OK){

			printf("***> One or more problems was encountered while running the pre-flight check...\n");

			cleanup();
			exit(ERROR);
		        }

	        /* calculate the inter-check delay to use when initially scheduling services */
		calculate_inter_check_delay();

		/* calculate interleave factor to use for spreading out service checks */
		calculate_interleave_factor();

	        /* initialize the event timing loop */
		init_timing_loop();

		/* display scheduling information */
		display_scheduling_info();

	        /* create status data (leave it around for later inspection) */
		if(create_status_data==TRUE){

			printf("Updating status log with initial check scheduling information...\n");

			printf("\tInitializing status data...\n");
			result=initialize_status_data(config_file);
			if(result==ERROR)
				printf("\t\tWarning: An error occurred while attempting to initialize the status data!\n");

			printf("\tUpdating all status data...\n");
			result=update_all_status_data();
			if(result==ERROR)
				printf("\t\tWarning: An error occurred while attempting to update the status data!\n");

			printf("\tCleaning up status data...\n");
			result=cleanup_status_data(config_file,FALSE);
			if(result==ERROR)
				printf("\t\tWarning: An error occurred while attempting to clean up the status data!\n");

			printf("Status log has been created successfully.\n\n");
		        }
	        }


	/* else start to monitor things... */
	else{

		/* keep monitoring things until we get a shutdown command */
		do{

			/* reset program variables */
			reset_variables();

			/* get program (re)start time */
			program_start=time(NULL);

			/* get PID */
			nagios_pid=(int)getpid();

			/* read in the configuration files (main config file and all host config files) */
			result=read_all_config_data(config_file);

			/* this must be logged after we read config data, as user may have changed location of main log file */
			snprintf(buffer,sizeof(buffer),"Nagios %s starting... (PID=%d)\n",PROGRAM_VERSION,(int)getpid());
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO,TRUE);

			/* there was a problem reading the config files */
			if(result!=OK){

				snprintf(buffer,sizeof(buffer),"Bailing out due to one or more errors encountered in the configuration files.  Run Nagios from the command line with the -v option to verify your config before restarting. (PID=%d)\n",(int)getpid());
				buffer[sizeof(buffer)-1]='\x0';
				write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR | NSLOG_CONFIG_ERROR,TRUE);

				cleanup();
				exit(ERROR);
		                }

			/* initialize embedded Perl interpreter unless we're restarting - in this case its already been initialized */
#ifdef EMBEDDEDPERL
			if(sigrestart==FALSE){

				if((perl=perl_alloc())==NULL){

					snprintf(buffer,sizeof(buffer),"Error: Could not allocate memory for embedded Perl interpreter!\n");
					buffer[sizeof(buffer)-1]='\x0';
					write_to_logs_and_console(buffer,NSLOG_RUNTIME_ERROR,TRUE);

					snprintf(buffer,sizeof(buffer),"Bailing out due to errors encountered while initializing embedded Perl interpreter... (PID=%d)\n",(int)getpid());
					buffer[sizeof(buffer)-1]='\x0';
					write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR,TRUE);
					
					exit(1);
		                }
				perl_construct(perl);
				exitstatus=perl_parse(perl,xs_init,2,embedding,NULL);
				if(!exitstatus)
					exitstatus=perl_run(perl);
			        }
#endif

			/* run the pre-flight check to make sure everything looks okay*/
			result=pre_flight_check();

			/* there was a problem running the pre-flight check */
			if(result!=OK){

				snprintf(buffer,sizeof(buffer),"Bailing out due to errors encountered while running the pre-flight check.  Run Nagios from the command line with the -v option to verify your config before restarting. (PID=%d)\n",(int)getpid());
				buffer[sizeof(buffer)-1]='\x0';
				write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR | NSLOG_VERIFICATION_ERROR ,TRUE);

				cleanup();
				exit(ERROR);
			        }

		        /* handle signals (interrupts) */
			setup_sighandler();

			/* enter daemon mode (unless we're restarting...) */
			if(daemon_mode==TRUE && sigrestart==FALSE){
#ifdef DEBUG2
				printf("$0: Cannot enter daemon mode with DEBUG option(s) enabled.  We'll run as a foreground process instead...\n");
				daemon_mode=FALSE;
#else
				daemon_init();

				snprintf(buffer,sizeof(buffer),"Finished daemonizing... (New PID=%d)\n",(int)getpid());
				buffer[sizeof(buffer)-1]='\x0';
				write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO,FALSE);

				/* get new PID */
				nagios_pid=(int)getpid();
#endif
			        }

			/* drop privileges */
			drop_privileges(nagios_user,nagios_group);

			/* open the command file (named pipe) for reading */
			result=open_command_file();
			
			/* there was a problem opening the command file */
			if(result!=OK){

				snprintf(buffer,sizeof(buffer),"Bailing out due to errors encountered while trying to open the external command file for reading... (PID=%d)\n",(int)getpid());
				buffer[sizeof(buffer)-1]='\x0';
				write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR ,TRUE);

				cleanup();
				exit(ERROR);
		                }

		        /* calculate the inter-check delay to use when initially scheduling services */
			calculate_inter_check_delay();

			/* calculate interleave factor to use for spreading out service checks */
			calculate_interleave_factor();

		        /* initialize the event timing loop */
			init_timing_loop();

		        /* initialize status data */
			initialize_status_data(config_file);

			/* read initial service and host state information  */
			read_initial_state_information(config_file);

			/* update all status data (with retained information) */
			update_all_status_data();

			/* initialize comment data */
			initialize_comment_data(config_file);
			
			/* initialize scheduled downtime data */
			initialize_downtime_data(config_file);
			
			/* initialize performance data */
			initialize_performance_data(config_file);

			/* create pipe used for IPC */
			if(pipe(ipc_pipe)){

				snprintf(buffer,sizeof(buffer),"Error: Could not initialize IPC pipe...\n");
				buffer[sizeof(buffer)-1]='\x0';
				write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO | NSLOG_RUNTIME_ERROR,TRUE);

				cleanup();
				break;
			        }

			/* read end of the pipe should be non-blocking */
			fcntl(ipc_pipe[0],F_SETFL,O_NONBLOCK);

			/* reset the restart flag */
			sigrestart=FALSE;

		        /***** start monitoring all services *****/
			/* (doesn't return until a restart or shutdown signal is encountered) */
			event_execution_loop();

			/* save service and host state information */
			save_state_information(config_file,FALSE);

			/* clean up the status data */
			cleanup_status_data(config_file,TRUE);

			/* clean up the comment data */
			cleanup_comment_data(config_file);

			/* clean up the scheduled downtime data */
			cleanup_downtime_data(config_file);

			/* clean up performance data */
			cleanup_performance_data(config_file);

			/* close the original pipe used for IPC (we'll create a new one if restarting) */
			close(ipc_pipe[0]);
			close(ipc_pipe[1]);

			/* close and delete the external command file FIFO unless we're restarting */
			if(sigrestart==FALSE)
				close_command_file();

			/* clean up after ourselves */
			cleanup();

	                }while(sigrestart==TRUE && sigshutdown==FALSE);



		/* we've shutdown... */

		/* make sure lock file has been removed - it may not have been if we received a shutdown command */
		if(daemon_mode==TRUE)
			unlink(lock_file);

		/* cleanup embedded perl */
#ifdef EMBEDDEDPERL
		PL_perl_destruct_level=0;
		perl_destruct(perl);
		perl_free(perl);
#endif

		/* log a shutdown message */
		snprintf(buffer,sizeof(buffer),"Successfully shutdown... (PID=%d)\n",(int)getpid());
		buffer[sizeof(buffer)-1]='\x0';
		write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO,TRUE);
 	        }

	return OK;
	}




/******************************************************************/
/************ EVENT SCHEDULING/HANDLING FUNCTIONS *****************/
/******************************************************************/


/* calculate the inter check delay to use when initially scheduling service checks */
void calculate_inter_check_delay(void){
	service *temp_service;

#ifdef DEBUG0
	printf("calculate_inter_check_delay() start\n");
#endif

	/* how should we determine the inter-check delay to use? */
	switch(inter_check_delay_method){

	case ICD_NONE:

		/* don't spread checks out - useful for testing parallelization code */
		scheduling_info.inter_check_delay=0.0;
		break;

	case ICD_DUMB:

		/* be dumb and just schedule checks 1 second apart */
		scheduling_info.inter_check_delay=1.0;
		break;
		
	case ICD_USER:

		/* the user specified a delay, so don't try to calculate one */
		break;

	case ICD_SMART:
	default:

		/* be smart and calculate the best delay to use to minimize CPU load... */

		scheduling_info.inter_check_delay=0.0;
		scheduling_info.check_interval_total=0L;

		for(temp_service=service_list,scheduling_info.total_services=0;temp_service!=NULL;temp_service=temp_service->next,scheduling_info.total_services++)
			scheduling_info.check_interval_total+=temp_service->check_interval;

		if(scheduling_info.total_services==0 || scheduling_info.check_interval_total==0)
			return;

		/* adjust the check interval total to correspond to the interval length */
		scheduling_info.check_interval_total=(scheduling_info.check_interval_total*interval_length);

		/* calculate the average check interval for services */
		scheduling_info.average_check_interval=(double)((double)scheduling_info.check_interval_total/(double)scheduling_info.total_services);

		/* calculate the average inter check delay (in seconds) needed to evenly space the service checks out */
		scheduling_info.average_inter_check_delay=(double)(scheduling_info.average_check_interval/(double)scheduling_info.total_services);

		/* set the global inter check delay value */
		scheduling_info.inter_check_delay=scheduling_info.average_inter_check_delay;

#ifdef DEBUG1
		printf("\tTotal service checks:    %d\n",scheduling_info.total_services);
		printf("\tCheck interval total:    %lu\n",scheduling_info.check_interval_total);
		printf("\tAverage check interval:  %0.2f sec\n",scheduling_info.average_check_interval);
		printf("\tInter-check delay:       %0.2f sec\n",scheduling_info.inter_check_delay);
#endif
	        }

#ifdef DEBUG0
	printf("calculate_inter_check_delay() end\n");
#endif

	return;
        }


/* calculate the interleave factor for spreading out service checks */
void calculate_interleave_factor(void){
	host *temp_host=NULL;
	service *temp_service=NULL;

#ifdef DEBUG0
	printf("calculate_interleave_factor() start\n");
#endif

	/* how should we determine the interleave factor? */
	switch(interleave_factor_method){

	case ILF_USER:

		/* the user supplied a value, so don't do any calculation */
		break;


	case ILF_SMART:
	default:


		/* count the number of service we have */
		for(scheduling_info.total_services=0,temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next,scheduling_info.total_services++);

		/* count the number of hosts we have */
		for(scheduling_info.total_hosts=0,temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next,scheduling_info.total_hosts++);

		/* protect against a divide by zero problem - shouldn't happen, but just in case... */
		if(scheduling_info.total_hosts==0)
			scheduling_info.total_hosts=1;

		scheduling_info.average_services_per_host=(double)((double)scheduling_info.total_services/(double)scheduling_info.total_hosts);
		scheduling_info.interleave_factor=(int)(ceil(scheduling_info.average_services_per_host));

		/* temporary hack for Alpha systems, not sure why calculation results in a 0... */
		if(scheduling_info.interleave_factor==0)
			scheduling_info.interleave_factor=(int)(scheduling_info.total_services/scheduling_info.total_hosts);

#ifdef DEBUG1
		printf("\tTotal service checks:    %d\n",scheduling_info.total_services);
		printf("\tTotal hosts:             %d\n",scheduling_info.total_hosts);
		printf("\tInterleave factor:       %d\n",scheduling_info.interleave_factor);
#endif
	        }

#ifdef DEBUG0
	printf("calculate_interleave_factor() end\n");
#endif

	return;
        }


/* initialize the event timing loop before we start monitoring */
void init_timing_loop(void){
	service *temp_service;
	time_t current_time;
	timed_event *new_event;
	time_t run_time;
	int total_interleave_blocks=0;
	int current_interleave_block=1;
	int interleave_block_index=0;
	int mult_factor;
	int is_valid_time;
	time_t next_valid_time;

#ifdef DEBUG0
	printf("init_timing_loop() start\n");
#endif

	/* get the time right now */
	time(&current_time);

	/* calculate number of interleave blocks */
	if(scheduling_info.interleave_factor==0)
		total_interleave_blocks=scheduling_info.total_services;
	else
		total_interleave_blocks=(int)ceil((double)scheduling_info.total_services/(double)scheduling_info.interleave_factor);

	scheduling_info.first_service_check=(time_t)0L;
	scheduling_info.last_service_check=(time_t)0L;

#ifdef DEBUG1
	printf("Total services: %d\n",scheduling_info.total_services);
	printf("Interleave factor: %d\n",scheduling_info.interleave_factor);
	printf("Total interleave blocks: %d\n",total_interleave_blocks);
	printf("Inter-check delay: %2.1f\n",scheduling_info.inter_check_delay);
#endif

	/* add all service checks as separate events (with interleaving) */
	for(current_interleave_block=1,temp_service=service_list;temp_service!=NULL;current_interleave_block++){

#ifdef DEBUG1
		printf("\tCurrent Interleave Block: %d\n",current_interleave_block);
#endif

		for(interleave_block_index=0;interleave_block_index<scheduling_info.interleave_factor;interleave_block_index++,temp_service=temp_service->next){

			if(temp_service==NULL)
				break;

			mult_factor=current_interleave_block+(interleave_block_index*total_interleave_blocks);

#ifdef DEBUG1
			printf("\t\tInterleave Block Index: %d\n",interleave_block_index);
			printf("\t\tMult factor: %d\n",mult_factor);
#endif

			/* set the next check time for the service */
			temp_service->next_check=(time_t)(current_time+(mult_factor*scheduling_info.inter_check_delay));

			/* make sure the service can actually be scheduled */
			is_valid_time=check_time_against_period(temp_service->next_check,temp_service->check_period);
			if(is_valid_time==ERROR){

				/* the initial time didn't work out, so try and find a next valid time for running the check */
				get_next_valid_time(temp_service->next_check,&next_valid_time,temp_service->check_period);

				/* whoa!  we couldn't find a next valid time, so don't schedule the check at all */
				if(temp_service->next_check==next_valid_time)
					temp_service->should_be_scheduled=FALSE;
			        }

#ifdef DEBUG1
			printf("\t\tService '%s' on host '%s'\n",temp_service->description,temp_service->host_name);
			if(temp_service->should_be_scheduled==TRUE)
				printf("\t\tNext Check: %lu --> %s",(unsigned long)temp_service->next_check,ctime(&temp_service->next_check));
			else
				printf("\t\tService check should *not* be scheduled!\n");
#endif

			if(scheduling_info.first_service_check==(time_t)0 || (temp_service->next_check<scheduling_info.first_service_check))
				scheduling_info.first_service_check=temp_service->next_check;
			if(temp_service->next_check > scheduling_info.last_service_check)
				scheduling_info.last_service_check=temp_service->next_check;

			/* create a new service check event if we should schedule this check */
			if(temp_service->should_be_scheduled==TRUE){

				new_event=malloc(sizeof(timed_event));
				if(new_event!=NULL){
					new_event->event_type=EVENT_SERVICE_CHECK;
					new_event->event_data=(void *)temp_service;
					new_event->run_time=temp_service->next_check;

				        /* service checks really are recurring, but are rescheduled manually - not automatically */
					new_event->recurring=FALSE;

					schedule_event(new_event,&event_list_low);
				        }
		                }
		        }

	        }

	/* add a service check reaper event */
	new_event=malloc(sizeof(timed_event));
	if(new_event!=NULL){
		new_event->event_type=EVENT_SERVICE_REAPER;
		new_event->event_data=(void *)NULL;
		new_event->run_time=current_time;
		new_event->recurring=TRUE;
		schedule_event(new_event,&event_list_high);
	        }

	/* add an orphaned service check event */
	if(check_orphaned_services==TRUE){
		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_ORPHAN_CHECK;
			new_event->event_data=(void *)NULL;
			new_event->run_time=current_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
	                }
	        }

	/* add a service result "freshness" check event */
	if(check_service_freshness==TRUE){
		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_FRESHNESS_CHECK;
			new_event->event_data=(void *)NULL;
			new_event->run_time=current_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
	                }
	        }

	/* add a status save event */
	if(aggregate_status_updates==TRUE){
		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_STATUS_SAVE;
			new_event->event_data=(void *)NULL;
			new_event->run_time=current_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
	                }
	        }

	/* add an external command check event if needed */
	if(check_external_commands==TRUE){
		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_COMMAND_CHECK;
			new_event->event_data=(void *)NULL;
			new_event->run_time=current_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
		        }
	        }

	/* add a log rotation event if necessary */
	if(log_rotation_method!=LOG_ROTATION_NONE){

		/* get the next time to run the log rotation */
		get_next_log_rotation_time(&run_time);

		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_LOG_ROTATION;
			new_event->event_data=(void *)NULL;
			new_event->run_time=run_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
		        }
	        }

	/* add a retention data save event if needed */
	if(retain_state_information==TRUE && retention_update_interval>0){
		new_event=malloc(sizeof(timed_event));
		if(new_event!=NULL){
			new_event->event_type=EVENT_RETENTION_SAVE;
			new_event->event_data=(void *)NULL;
			new_event->run_time=current_time;
			new_event->recurring=TRUE;
			schedule_event(new_event,&event_list_high);
		        }
	        }

#ifdef DEBUG0
	printf("init_timing_loop() end\n");
#endif

	return;
        }


/* displays service check scheduling information */
void display_scheduling_info(void){
	float minimum_concurrent_checks;

	printf("\n\tSERVICE SCHEDULING INFORMATION\n");
	printf("\t-------------------------------\n");
	printf("\tTotal services:             %d\n",scheduling_info.total_services);
	printf("\tTotal hosts:                %d\n",scheduling_info.total_hosts);
	printf("\n");

	printf("\tCheck reaper interval:      %d sec\n",service_check_reaper_interval);
	printf("\n");

	printf("\tInter-check delay method:   ");
	if(inter_check_delay_method==ICD_NONE)
		printf("NONE\n");
	else if(inter_check_delay_method==ICD_DUMB)
		printf("DUMB\n");
	else if(inter_check_delay_method==ICD_SMART){
		printf("SMART\n");
		printf("\tAverage check interval:     %2.3f sec\n",scheduling_info.average_check_interval);
	        }
	else
		printf("USER-SUPPLIED VALUE\n");
	printf("\tInter-check delay:          %2.3f sec\n",scheduling_info.inter_check_delay);
	printf("\n");

	printf("\tInterleave factor method:   %s\n",(interleave_factor_method==ILF_USER)?"USER-SUPPLIED VALUE":"SMART");
	if(interleave_factor_method==ILF_SMART)
		printf("\tAverage services per host:  %2.3f\n",scheduling_info.average_services_per_host);
	printf("\tService interleave factor:  %d\n",scheduling_info.interleave_factor);
	printf("\n");

	printf("\tInitial service check scheduling info:\n");
	printf("\t--------------------------------------\n");
	printf("\tFirst scheduled check:      %lu -> %s",(unsigned long)scheduling_info.first_service_check,ctime(&scheduling_info.first_service_check));
	printf("\tLast scheduled check:       %lu -> %s",(unsigned long)scheduling_info.last_service_check,ctime(&scheduling_info.last_service_check));
	printf("\n");
	printf("\tRough guidelines for max_concurrent_checks value:\n");
	printf("\t-------------------------------------------------\n");

	minimum_concurrent_checks=ceil((float)service_check_reaper_interval/scheduling_info.inter_check_delay);

	printf("\tAbsolute minimum value:     %d\n",(int)minimum_concurrent_checks);
	printf("\tRecommend value:            %d\n",(int)(minimum_concurrent_checks*3.0));
	printf("\n");
	printf("\tNotes:\n");
	printf("\tThe recommendations for the max_concurrent_checks value\n");
	printf("\tassume that the average execution time for service\n");
	printf("\tchecks is less than the service check reaper interval.\n");
	printf("\tThe minimum value also reflects best case scenarios\n");
	printf("\twhere there are no problems on your network.  You will\n");
	printf("\thave to tweak this value as necessary after testing.\n");
	printf("\tHigh latency values for checks are often indicative of\n");
	printf("\tthe max_concurrent_checks value being set too low and/or\n");
	printf("\tthe service_reaper_frequency being set too high.\n");
	printf("\tIt is important to note that the values displayed above\n");
	printf("\tdo not reflect current performance information for any\n");
	printf("\tNagios process that may currently be running.  They are\n");
	printf("\tprovided solely to project expected and recommended\n");
	printf("\tvalues based on the current data in the config files.\n");
	printf("\n");

	return;
        }


/* displays which external data modules are compiled in */
void display_xdata_modules(void){

	printf("External Data I/O\n");
	printf("-----------------\n");

	printf("Object Data:      ");
#ifdef USE_XODDEFAULT
	printf("DEFAULT");
#endif
#ifdef USE_XODTEMPLATE
	printf("TEMPLATE");
#endif
#ifdef USE_XODDB
	printf("DATABASE (");
#ifdef USE_XODMYSQL
	printf("MySQL");
#endif
#ifdef USE_XODPGSQL
	printf("PostgreSQL");
#endif
	printf(")");
#endif
	printf("\n");

	printf("Status Data:      ");
#ifdef USE_XSDDEFAULT
	printf("DEFAULT");
#endif
#ifdef USE_XSDDB
	printf("DATABASE (");
#ifdef USE_XSDMYSQL
	printf("MySQL");
#endif
#ifdef USE_XSDPGSQL
	printf("PostgreSQL");
#endif
	printf(")");
#endif
	printf("\n");

	printf("Retention Data:   ");
#ifdef USE_XRDDEFAULT
	printf("DEFAULT");
#endif
#ifdef USE_XRDDB
	printf("DATABASE (");
#ifdef USE_XRDMYSQL
	printf("MySQL");
#endif
#ifdef USE_XRDPGSQL
	printf("PostgreSQL");
#endif
	printf(")");
#endif
	printf("\n");

	printf("Comment Data:     ");
#ifdef USE_XCDDEFAULT
	printf("DEFAULT");
#endif
#ifdef USE_XCDDB
	printf("DATABASE (");
#ifdef USE_XCDMYSQL
	printf("MySQL");
#endif
#ifdef USE_XCDPGSQL
	printf("PostgreSQL");
#endif
	printf(")");
#endif
	printf("\n");
	
	printf("Downtime Data:    ");
#ifdef USE_XDDDEFAULT
	printf("DEFAULT");
#endif
#ifdef USE_XDDDB
	printf("DATABASE (");
#ifdef USE_XDDMYSQL
	printf("MySQL");
#endif
#ifdef USE_XDDPGSQL
	printf("PostgreSQL");
#endif
	printf(")");
#endif
	printf("\n");

	printf("Performance Data: ");
#ifdef USE_XPDDEFAULT
	printf("DEFAULT");
#endif
#ifdef USE_XPDFILE
	printf("FILE");
#endif
	printf("\n\n");

	printf("Options\n");
	printf("-------\n");

#ifdef EMBEDDEDPERL
	printf("* Embedded Perl compiler (");
	if(!strcmp(DO_CLEAN,"1"))
		printf("No caching");
	else
		printf("With caching");
	printf(")\n");
#endif

	printf("\n\n");

	return;
        }


/* schedule an event in order of execution time */
void schedule_event(timed_event *event,timed_event **event_list){
	timed_event *temp_event;
	timed_event *first_event;
	service *temp_service;
	time_t run_time;

#ifdef DEBUG0
	printf("schedule_event() start\n");
#endif

	/* if this event is a service check... */
	if(event->event_type==EVENT_SERVICE_CHECK){
		temp_service=(service *)event->event_data;
		event->run_time=temp_service->next_check;
	        }

	/* if this is an external command check event ... */
	else if(event->event_type==EVENT_COMMAND_CHECK){

		/* we're supposed to check as often as possible, so we schedule regular checks at 60 second intervals */
		if(command_check_interval==-1)
			event->run_time=event->run_time+60;

		/* else schedule external command checks at user-specified intervals */
		else
			event->run_time=event->run_time+(command_check_interval*interval_length);
	        }

	/* if this is a log rotation event... */
	else if(event->event_type==EVENT_LOG_ROTATION){
		get_next_log_rotation_time(&run_time);
		event->run_time=run_time;
	        }

	/* if this is a service check reaper event... */
	else if(event->event_type==EVENT_SERVICE_REAPER)
		event->run_time=event->run_time+service_check_reaper_interval;

	/* if this is a orphaned service check event... */
	else if(event->event_type==EVENT_ORPHAN_CHECK)
		event->run_time=event->run_time+(service_check_timeout*2);

	/* if this is a service result freshness check */
	else if(event->event_type==EVENT_FRESHNESS_CHECK)
		event->run_time=event->run_time+freshness_check_interval;

	/* if this is a state retention save event... */
	else if(event->event_type==EVENT_RETENTION_SAVE)
		event->run_time=event->run_time+(retention_update_interval*60);

	/* if this is a status save event... */
	else if(event->event_type==EVENT_STATUS_SAVE)
		event->run_time=event->run_time+(status_update_interval);

	/* if this is a scheduled program shutdown or restart, we already know the time... */

	event->next=NULL;

	first_event=*event_list;

	/* add the event to the head of the list if there are no other events */
	if(*event_list==NULL)
		*event_list=event;

        /* add event to head of the list if it should be executed first */
	else if(event->run_time < first_event->run_time){
	        event->next=*event_list;
		*event_list=event;
	        }

	/* else place the event according to next execution time */
	else{

		temp_event=*event_list;
		while(temp_event!=NULL){

			if(temp_event->next==NULL){
				temp_event->next=event;
				break;
			        }
			
			if(event->run_time < temp_event->next->run_time){
				event->next=temp_event->next;
				temp_event->next=event;
				break;
			        }

			temp_event=temp_event->next;
		        }
	        }

#ifdef DEBUG0
	printf("schedule_event() end\n");
#endif

	return;
        }



/* remove an event from the queue */
void remove_event(timed_event *event,timed_event **event_list){
	timed_event *temp_event;

#ifdef DEBUG0
	printf("remove_event() start\n");
#endif

	if(*event_list==NULL)
		return;

	if(*event_list==event)
		*event_list=event->next;

	else{

		for(temp_event=*event_list;temp_event!=NULL;temp_event=temp_event->next){
			if(temp_event->next==event){
				temp_event->next=temp_event->next->next;
				event->next=NULL;
				break;
			        }
		        }
	        }

#ifdef DEBUG0
	printf("remove_event() end\n");
#endif

	return;
        }



/* this is the main event handler loop */
int event_execution_loop(void){
	timed_event *temp_event;
	time_t last_time;
	time_t current_time;
	int run_event=TRUE;
	service *temp_service;

#ifdef DEBUG0
	printf("event_execution_loop() start\n");
#endif

	time(&last_time);

	while(1){

		/* see if we should exit or restart (a signal was encountered) */
		if(sigshutdown==TRUE || sigrestart==TRUE)
			break;

		/* if we don't have any events to handle, exit */
		if(event_list_high==NULL && event_list_low==NULL){
#ifdef DEBUG1
			printf("There aren't any events that need to be handled!\n");
#endif
			break;
	                }

		/* get the current time */
		time(&current_time);

		/* hey, wait a second...  we traveled back in time! */
		if(current_time<last_time)
			compensate_for_system_time_change((unsigned long)last_time,(unsigned long)current_time);

		/* else if the time advanced over the specified threshold, try and compensate... */
		else if((current_time-last_time)>=time_change_threshold)
			compensate_for_system_time_change((unsigned long)last_time,(unsigned long)current_time);

		/* keep track of the last time */
		last_time=current_time;

#ifdef DEBUG3
		printf("\n");
		printf("*** Event Check Loop ***\n");
		printf("\tCurrent time: %s",ctime(&current_time));
		if(event_list_high!=NULL)
			printf("\tNext High Priority Event Time: %s",ctime(&event_list_high->run_time));
		else
			printf("\tNo high priority events are scheduled...\n");
		if(event_list_low!=NULL)
			printf("\tNext Low Priority Event Time:  %s",ctime(&event_list_low->run_time));
		else
			printf("\tNo low priority events are scheduled...\n");
		printf("Current/Max Outstanding Checks: %d/%d\n",currently_running_service_checks,max_parallel_service_checks);
#endif

		/* handle high priority events */
		if(event_list_high!=NULL && (current_time>=event_list_high->run_time)){

			/* remove the first event from the timing loop */
			temp_event=event_list_high;
			event_list_high=event_list_high->next;

			/* handle the event */
			handle_timed_event(temp_event);

			/* reschedule the event if necessary */
			if(temp_event->recurring==TRUE)
				schedule_event(temp_event,&event_list_high);

			/* else free memory associated with the event */
			else
				free(temp_event);
		        }

		/* handle low priority events */
		else if(event_list_low!=NULL && (current_time>=event_list_low->run_time)){

			/* default action is to execute the event */
			run_event=TRUE;

			/* run a few checks before executing a service check... */
			if(event_list_low->event_type==EVENT_SERVICE_CHECK){

				temp_service=(service *)event_list_low->event_data;

				/* update service check latency */
				temp_service->latency=(unsigned long)(current_time-event_list_low->run_time);

				/* don't run a service check if we're not supposed to right now */
				if(execute_service_checks==FALSE && !(temp_service->check_options & CHECK_OPTION_FORCE_EXECUTION)){
#ifdef DEBUG3
					printf("\tWe're not executing service checks right now...\n");
#endif

					/* remove the service check from the event queue and reschedule it for a later time */
					temp_event=event_list_low;
					event_list_low=event_list_low->next;
					if(temp_service->state_type==SOFT_STATE && temp_service->current_state!=STATE_OK)
						temp_service->next_check=(time_t)(temp_service->next_check+(temp_service->retry_interval*interval_length));
					else
						temp_service->next_check=(time_t)(temp_service->next_check+(temp_service->check_interval*interval_length));
					schedule_event(temp_event,&event_list_low);
					update_service_status(temp_service,FALSE);

					run_event=FALSE;
				        }

				/* dont' run a service check if we're already maxed out on the number of parallel service checks...  */
				if(max_parallel_service_checks!=0 && (currently_running_service_checks >= max_parallel_service_checks)){
#ifdef DEBUG3
					printf("\tMax concurrent service checks (%d) has been reached.  Delaying further checks until previous checks are complete...\n",max_parallel_service_checks);
#endif
					run_event=FALSE;
				        }

				/* don't run a service check that can't be parallized if there are other checks out there... */
				if(temp_service->parallelize==FALSE && currently_running_service_checks>0){
#ifdef DEBUG3
					printf("\tA non-parallelizable check is queued for execution, but there are still other checks executing.  We'll wait...\n");
#endif
					run_event=FALSE;
				        }

				/* a service check that shouldn't be parallized with other checks is currently running, so don't execute another check */
				if(non_parallelized_check_running==TRUE){
#ifdef DEBUG3
					printf("\tA non-parallelizable check is currently running, so we have to wait before executing other checks...\n");
#endif
					run_event=FALSE;
				        }
			        }

			/* run the event except if it is a service check and we already are maxed out on number or parallel service checks... */
			if(run_event==TRUE){

				/* remove the first event from the timing loop */
				temp_event=event_list_low;
				event_list_low=event_list_low->next;

				/* handle the event */
				handle_timed_event(temp_event);

				/* reschedule the event if necessary */
				if(temp_event->recurring==TRUE)
					schedule_event(temp_event,&event_list_low);

				/* else free memory associated with the event */
				else
					free(temp_event);
			        }

			/* wait a second so we don't hog the CPU... */
			else
				sleep((unsigned int)sleep_time);
		        }

		/* we don't have anything to do at this moment in time */
		else{

			/* check for external commands if we're supposed to check as often as possible */
			if(command_check_interval==-1)
				check_for_external_commands();

			/* wait a second so we don't hog the CPU... */
			sleep((unsigned int)sleep_time);
		        }
	        }

#ifdef DEBUG0
	printf("event_execution_loop() end\n");
#endif

	return OK;
	}



/* handles a timed event */
int handle_timed_event(timed_event *event){
	service *temp_service;
	char temp_buffer[MAX_INPUT_BUFFER];

#ifdef DEBUG0
	printf("handle_timed_event() start\n");
#endif

#ifdef DEBUG3
	printf("*** Event Details ***\n");
	printf("\tEvent type: %d ",event->event_type);

	if(event->event_type==EVENT_SERVICE_CHECK){
		printf("(service check)\n");
		temp_service=(service *)event->event_data;
		printf("\t\tService Description: %s\n",temp_service->description);
		printf("\t\tAssociated Host:     %s\n",temp_service->host_name);
	        }

	else if(event->event_type==EVENT_COMMAND_CHECK)
		printf("(external command check)\n");

	else if(event->event_type==EVENT_LOG_ROTATION)
		printf("(log file rotation)\n");

	else if(event->event_type==EVENT_PROGRAM_SHUTDOWN)
		printf("(program shutdown)\n");

	else if(event->event_type==EVENT_PROGRAM_RESTART)
		printf("(program restart)\n");

	else if(event->event_type==EVENT_ENABLE_NOTIFICATIONS)
		printf("(enable notifications)\n");

	else if(event->event_type==EVENT_DISABLE_NOTIFICATIONS)
		printf("(disable_notifications)\n");

	else if(event->event_type==EVENT_SERVICE_REAPER)
		printf("(service check reaper)\n");

	else if(event->event_type==EVENT_ORPHAN_CHECK)
		printf("(orphaned service check)\n");

	else if(event->event_type==EVENT_RETENTION_SAVE)
		printf("(retention save)\n");

	else if(event->event_type==EVENT_STATUS_SAVE)
		printf("(status save)\n");

	else if(event->event_type==EVENT_SCHEDULED_DOWNTIME)
		printf("(scheduled downtime)\n");

	else if(event->event_type==EVENT_FRESHNESS_CHECK)
		printf("(service result freshness check)\n");

	else if(event->event_type==EVENT_EXPIRE_DOWNTIME)
		printf("(expire downtime)\n");

	printf("\tEvent time: %s",ctime(&event->run_time));
#endif
		
	/* how should we handle the event? */
	switch(event->event_type){

	case EVENT_SERVICE_CHECK:

		/* run  a service check */
		temp_service=(service *)event->event_data;
		run_service_check(temp_service);

		break;

	case EVENT_COMMAND_CHECK:

		/* check for external commands */
		check_for_external_commands();
		break;

	case EVENT_LOG_ROTATION:

		/* rotate the log file */
		rotate_log_file(event->run_time);
		break;

	case EVENT_PROGRAM_SHUTDOWN:
		
		/* set the shutdown flag */
		sigshutdown=TRUE;

		/* log the shutdown */
		snprintf(temp_buffer,sizeof(temp_buffer),"PROGRAM_SHUTDOWN event encountered, shutting down...\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO,TRUE);
		break;

	case EVENT_PROGRAM_RESTART:

		/* set the restart flag */
		sigrestart=TRUE;

		/* log the restart */
		snprintf(temp_buffer,sizeof(temp_buffer),"PROGRAM_RESTART event encountered, restarting...\n");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_PROCESS_INFO,TRUE);
		break;

	case EVENT_ENABLE_NOTIFICATIONS:

		/* enable notifications */
		enable_all_notifications();
		break;

	case EVENT_DISABLE_NOTIFICATIONS:

		/* disable notifications */
		disable_all_notifications();
		break;

	case EVENT_SERVICE_REAPER:

		/* reap service check results */
		reap_service_checks();
		break;

	case EVENT_ORPHAN_CHECK:

		/* check for orphaned services */
		check_for_orphaned_services();
		break;

	case EVENT_RETENTION_SAVE:

		/* save state retention data */
		save_state_information(config_file,TRUE);
		break;

	case EVENT_STATUS_SAVE:

		/* save all status data (program, host, and service) */
		update_all_status_data();
		break;

	case EVENT_SCHEDULED_DOWNTIME:

		/* process scheduled downtime info */
		handle_scheduled_downtime((scheduled_downtime *)event->event_data);
		break;

	case EVENT_FRESHNESS_CHECK:
		
		/* check service result freshness */
		check_service_result_freshness();
		break;

	case EVENT_EXPIRE_DOWNTIME:

		/* check for expired scheduled downtime entries */
		check_for_expired_downtime();

	default:

		break;
	        }

#ifdef DEBUG0
	printf("handle_timed_event() end\n");
#endif

	return OK;
        }



/* attempts to compensate for a change in the system time */
void compensate_for_system_time_change(unsigned long last_time,unsigned long current_time){
	char buffer[MAX_INPUT_BUFFER];
	unsigned long time_difference;
	timed_event *temp_event;
	service *temp_service;
	host *temp_host;

#ifdef DEBUG0
	printf("compensate_for_system_time_change() start\n");
#endif

	/* we moved back in time... */
	if(last_time>current_time)
		time_difference=last_time-current_time;

	/* we moved into the future... */
	else
		time_difference=current_time-last_time;

	/* log the time change */
	snprintf(buffer,sizeof(buffer),"Warning: A system time change of %lu seconds (%s in time) has been detected.  Compensating...\n",time_difference,(last_time>current_time)?"backwards":"forwards");
	buffer[sizeof(buffer)-1]='\x0';
	write_to_logs_and_console(buffer,NSLOG_PROCESS_INFO | NSLOG_RUNTIME_WARNING,TRUE);

	/* adjust the next run time for all high priority timed events */
	for(temp_event=event_list_high;temp_event!=NULL;temp_event=temp_event->next){

		/* we moved back in time... */
		if(last_time>current_time){

			/* we can't precede the UNIX epoch */
			if(time_difference>(unsigned long)temp_event->run_time)
				temp_event->run_time=(time_t)0;
			else
				temp_event->run_time=(time_t)(temp_event->run_time-(time_t)time_difference);
		        }

		/* we moved into the future... */
		else
			temp_event->run_time=(time_t)(temp_event->run_time+(time_t)time_difference);
	        }

	/* adjust the next run time for all low priority timed events */
	for(temp_event=event_list_low;temp_event!=NULL;temp_event=temp_event->next){

		/* we moved back in time... */
		if(last_time>current_time){

			/* we can't precede the UNIX epoch */
			if(time_difference>(unsigned long)temp_event->run_time)
				temp_event->run_time=(time_t)0;
			else
				temp_event->run_time=(time_t)(temp_event->run_time-(time_t)time_difference);
		        }

		/* we moved into the future... */
		else
			temp_event->run_time=(time_t)(temp_event->run_time+(time_t)time_difference);
	        }

	/* adjust the last notification time for all services */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		if(temp_service->last_notification==(time_t)0)
			continue;

		/* we moved back in time... */
		if(last_time>current_time){

			/* we can't precede the UNIX epoch */
			if(time_difference>(unsigned long)temp_service->last_notification)
				temp_service->last_notification=(time_t)0;
			else
				temp_service->last_notification=(time_t)(temp_service->last_notification-(time_t)time_difference);
		        }

		/* we moved into the future... */
		else
			temp_service->last_notification=(time_t)(temp_service->last_notification+(time_t)time_difference);

		/* update the status data */
		update_service_status(temp_service,FALSE);
	        }


	/* adjust the next check time for all services */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		/* we moved back in time... */
		if(last_time>current_time){

			/* we can't precede the UNIX epoch */
			if(time_difference>(unsigned long)temp_service->next_check)
				temp_service->next_check=(time_t)0;
			else
				temp_service->next_check=(time_t)(temp_service->next_check-(time_t)time_difference);
		        }

		/* we moved into the future... */
		else
			temp_service->next_check=(time_t)(temp_service->next_check+(time_t)time_difference);

		/* update the status data */
		update_service_status(temp_service,FALSE);
	        }

	/* adjust the last notification time for all hosts */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		if(temp_host->last_host_notification==(time_t)0)
			continue;

		/* we moved back in time... */
		if(last_time>current_time){

			/* we can't precede the UNIX epoch */
			if(time_difference>(unsigned long)temp_host->last_host_notification)
				temp_host->last_host_notification=(time_t)0;
			else
				temp_host->last_host_notification=(time_t)(temp_host->last_host_notification-(time_t)time_difference);
		        }

		/* we moved into the future... */
		else
			temp_host->last_host_notification=(time_t)(temp_host->last_host_notification+(time_t)time_difference);

		/* update the status data */
		update_host_status(temp_host,FALSE);
	        }


	/* adjust program start time (necessary for state stats calculations) */
	/* we moved back in time... */
	if(last_time>current_time){

		/* we can't precede the UNIX epoch */
		if(time_difference>(unsigned long)program_start)
			program_start=(time_t)0;
		else
			program_start=(time_t)(program_start-(time_t)time_difference);
	        }

	/* we moved into the future... */
	else
		program_start=(time_t)(program_start+(time_t)time_difference);

	/* update status data */
	update_program_status(FALSE);

#ifdef DEBUG0
	printf("compensate_for_system_time_change() end\n");
#endif

	return;
        }


