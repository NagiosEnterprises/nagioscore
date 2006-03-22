/*****************************************************************************
 *
 * CHECKS.C - Service and host check functions for Nagios
 *
 * Copyright (c) 1999-2006 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   03-11-2006
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
#include "../include/statusdata.h"
#include "../include/downtime.h"
#include "../include/nagios.h"
#include "../include/broker.h"
#include "../include/perfdata.h"

/*#define DEBUG_CHECKS*/
/*#define DEBUG_HOST_CHECKS 1*/


#ifdef EMBEDDEDPERL
#include "../include/epn_nagios.h"
#endif

extern char     *temp_file;
extern char     *temp_path;

extern int      interval_length;

extern int      command_check_interval;

extern int      ipc_pipe[2];

extern int      log_initial_states;

extern int      service_check_timeout;
extern int      host_check_timeout;

extern int      check_reaper_interval;
extern int      max_check_reaper_time;

extern int      use_aggressive_host_checking;
extern int      use_old_host_check_logic;
extern unsigned long cached_host_check_horizon;
extern unsigned long cached_service_check_horizon;
extern int      enable_predictive_host_dependency_checks;
extern int      enable_predictive_service_dependency_checks;

extern int      soft_state_dependencies;

extern int      currently_running_service_checks;
extern int      currently_running_host_checks;

extern int      accept_passive_service_checks;
extern int      execute_service_checks;
extern int      accept_passive_host_checks;
extern int      execute_host_checks;
extern int      obsess_over_services;
extern int      obsess_over_hosts;

extern int      check_service_freshness;
extern int      check_host_freshness;

extern time_t   program_start;

extern timed_event       *event_list_low;

extern host              *host_list;
extern service           *service_list;
extern servicedependency *servicedependency_list;
extern hostdependency    *hostdependency_list;

extern unsigned long   next_event_id;

extern check_result    check_result_info;

extern pthread_t       worker_threads[TOTAL_WORKER_THREADS];

#ifdef EMBEDDEDPERL
extern int      use_embedded_perl;
#endif





/******************************************************************/
/********************** CHECK REAPER FUNCTIONS ********************/
/******************************************************************/

/* reaps host and service check results */
int reap_check_results(void){
	check_result queued_check_result;
	service *temp_service=NULL;
	host *temp_host=NULL;
	char *temp_buffer=NULL;
	time_t current_time=0L;
	time_t reaper_start_time=0L;

#ifdef DEBUG0
        printf("reap_check_results() start\n");
#endif

#ifdef DEBUG3
	printf("Starting to reap check results...\n");
#endif

	/* get the start time */
	time(&reaper_start_time);

	/* read all check results that have come in... */
	while(read_check_result(&queued_check_result)>0){

		/* service check */
		if(queued_check_result.object_check_type==SERVICE_CHECK){

			/* make sure the service exists */
			if((temp_service=find_service(queued_check_result.host_name,queued_check_result.service_description))==NULL){

				asprintf(&temp_buffer,"Warning: Check result queue contained results for service '%s' on host '%s', but the service could not be found!  Perhaps you forgot to define the service in your config files?\n",queued_check_result.service_description,queued_check_result.host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
				my_free((void **)&temp_buffer);

				free_check_result(&queued_check_result);
				continue;
		                }

			/* process the check result */
			handle_async_service_check_result(temp_service,&queued_check_result);
		        }

		/* host check */
		else{
			if((temp_host=find_host(queued_check_result.host_name))==NULL){

				/* make sure the host exists */
				asprintf(&temp_buffer,"Warning: Check result queue contained results for host '%s', but the host could not be found!  Perhaps you forgot to define the host in your config files?\n",queued_check_result.host_name);
				write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
				my_free((void **)&temp_buffer);

				free_check_result(&queued_check_result);
				continue;
		                }

			/* process the check result */
			handle_async_host_check_result_3x(temp_host,&queued_check_result);
		        }

		/* free allocated memory */
		free_check_result(&queued_check_result);

		/* break out if we've been here too long (max_check_reaper_time seconds) */
		time(&current_time);
		if((int)(current_time-reaper_start_time)>max_check_reaper_time)
			break;
	        }

#ifdef DEBUG3
	printf("Finished reaping check results.\n");
#endif

#ifdef DEBUG0
	printf("reap_check_results() end\n");
#endif

	return OK;
        }




/******************************************************************/
/****************** SERVICE MONITORING FUNCTIONS ******************/
/******************************************************************/

/* executes a scheduled service check */
int run_scheduled_service_check(service *svc, int check_options, double latency){
	int result=OK;
	time_t preferred_time=0L;
	time_t next_valid_time=0L;
	int time_is_valid=TRUE;

	/* attempt to run the check */
	result=run_async_service_check(svc,check_options,latency,TRUE,&time_is_valid,&preferred_time);

	/* an error occurred, so reschedule the check */
	if(result==ERROR){

		/* only attempt to (re)schedule checks that should get checked... */
		if(svc->should_be_scheduled==TRUE){

			/* make sure we rescheduled the next service check at a valid time */
			get_next_valid_time(preferred_time,&next_valid_time,svc->check_period);

			/* the service could not be rescheduled properly - set the next check time for next year, but don't actually reschedule it */
			if(time_is_valid==FALSE && next_valid_time==preferred_time){

				svc->next_check=(time_t)(next_valid_time+(60*60*24*365));
				svc->should_be_scheduled=FALSE;
#ifdef DEBUG1
				printf("Warning: Could not find any valid times to reschedule a check of service '%s' on host '%s'!\n",svc->description,svc->host_name);
#endif
		                }

			/* this service could be rescheduled... */
			else{
				svc->next_check=next_valid_time;
				svc->should_be_scheduled=TRUE;
			        }
		        }

		/* update the status log */
		update_service_status(svc,FALSE);

		/* reschedule the next service check - unless we couldn't find a valid next check time */
		if(svc->should_be_scheduled==TRUE)
			schedule_service_check(svc,svc->next_check,FALSE);

		return ERROR;
	        }

	return OK;
        }


/* forks a child process to run a service check, but does not wait for the service check result */
int run_async_service_check(service *svc, int check_options, double latency, int reschedule_check, int *time_is_valid, time_t *preferred_time){
	char raw_command[MAX_COMMAND_BUFFER]="";
	char processed_command[MAX_COMMAND_BUFFER]="";
	char output_buffer[MAX_INPUT_BUFFER]="";
	char *temp_buffer=NULL;
	int check_service=TRUE;
	struct timeval start_time,end_time;
	pid_t pid=0;
	int fork_error=FALSE;
	int wait_result=0;
	host *temp_host=NULL;
	FILE *fp=NULL;
	int pclose_result=0;
	mode_t new_umask=077;
	mode_t old_umask;
	char *output_file=NULL;
	double old_latency=0.0;
#ifdef EMBEDDEDPERL
	char fname[512];
	char *args[5] = {"",DO_CLEAN, "", "", NULL };
	char *perl_plugin_output ;
	int isperl;
	SV *plugin_hndlr_cr;
	STRLEN n_a ;
#ifdef aTHX
	dTHX;
#endif
	dSP;
#endif

	/* make sure we have something */
	if(svc==NULL)
		return ERROR;

	/* is the service check viable at this time? */
	if(check_service_check_viability(svc,check_options,time_is_valid,preferred_time)==ERROR)
		return ERROR;

	/* find the host associated with this service */
	if((temp_host=find_host(svc->host_name))==NULL)
		return ERROR;

	/******** GOOD TO GO FOR A REAL SERVICE CHECK AT THIS POINT ********/

#ifdef DEBUG3
	printf("\tChecking service '%s' on host '%s'...\n",svc->description,svc->host_name);
#endif

	/* increment number of service checks that are currently running... */
	currently_running_service_checks++;

	/* set the execution flag */
	svc->is_executing=TRUE;

	/* clear the force execution flag */
	if((check_options & CHECK_OPTION_FORCE_EXECUTION) && (svc->check_options & CHECK_OPTION_FORCE_EXECUTION))
		svc->check_options-=CHECK_OPTION_FORCE_EXECUTION;

	/* update latency for macros, event broker, save old value for later */
	old_latency=svc->latency;
	svc->latency=latency;

	/* grab the host and service macro variables */
	clear_volatile_macros();
	grab_host_macros(temp_host);
	grab_service_macros(svc);
	grab_summary_macros(NULL);

	/* get the raw command line */
	get_raw_command_line(svc->service_check_command,raw_command,sizeof(raw_command),0);
	strip(raw_command);

	/* process any macros contained in the argument */
	process_macros(raw_command,processed_command,sizeof(processed_command),0);
	strip(processed_command);

	/* get the command start time */
	gettimeofday(&start_time,NULL);

	/* open a temp file for storing check output */
	old_umask=umask(new_umask);
	asprintf(&output_file,"%s/nagiosXXXXXX",temp_path);
	check_result_info.output_file_fd=mkstemp(output_file);
#ifdef DEBUG_CHECK_IPC
	printf("OUTPUT FILE: %s\n",output_file);
#endif
	if(check_result_info.output_file_fd>0)
		check_result_info.output_file_fp=fdopen(check_result_info.output_file_fd,"w");
	else{
		check_result_info.output_file_fp=NULL;
		check_result_info.output_file_fd=-1;
	        }
	umask(old_umask);
#ifdef DEBUG_CHECK_IPC
	printf("OUTPUT FILE FD: %d\n",check_result_info.output_file_fd);
#endif

	/* save check info */
	check_result_info.object_check_type=SERVICE_CHECK;
	check_result_info.host_name=(char *)strdup(svc->host_name);
	check_result_info.service_description=(char *)strdup(svc->description);
	check_result_info.check_type=SERVICE_CHECK_ACTIVE;
	check_result_info.output_file=(check_result_info.output_file_fd<0 || output_file==NULL)?NULL:strdup(output_file);
	check_result_info.start_time=start_time;
	check_result_info.finish_time=start_time;
	check_result_info.early_timeout=FALSE;
	check_result_info.exited_ok=TRUE;
	check_result_info.return_code=STATE_OK;

	/* free memory */
	my_free((void **)&output_file);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_service_check(NEBTYPE_SERVICECHECK_INITIATE,NEBFLAG_NONE,NEBATTR_NONE,svc,SERVICE_CHECK_ACTIVE,start_time,end_time,svc->service_check_command,svc->latency,0.0,0,FALSE,0,processed_command,NULL);
#endif

	/* reset latency (permanent value will be set later */
	svc->latency=old_latency;

#ifdef EMBEDDEDPERL
	strncpy(fname,processed_command,strcspn(processed_command," "));
	fname[strcspn(processed_command," ")] = '\x0';

	/* have "filename" component of command. Check for PERL */
	fp=fopen(fname, "r");
	if(fp==NULL)
		strcpy(raw_command,"");
	else{
		fgets(raw_command,80,fp);
		fclose(fp);
	        }

	isperl=FALSE;

	if(strstr(raw_command,"/bin/perl")!=NULL){

		isperl = TRUE;
		args[0] = fname;
		args[2] = "";

		if(strchr(processed_command,' ')==NULL)
			args[3]="";
		else
			args[3]=processed_command+strlen(fname)+1;

		ENTER; 
		SAVETMPS;
		PUSHMARK(SP);
		XPUSHs(sv_2mortal(newSVpv(args[0],0)));
		XPUSHs(sv_2mortal(newSVpv(args[1],0)));
		XPUSHs(sv_2mortal(newSVpv(args[2],0)));
		XPUSHs(sv_2mortal(newSVpv(args[3],0)));
		PUTBACK;

		/* call our perl interpreter to compile and optionally cache the command */

		call_pv("Embed::Persistent::eval_file", G_SCALAR | G_EVAL);

		SPAGAIN ;

		if ( SvTRUE(ERRSV) ) {

			/*
			 * if SvTRUE(ERRSV)
			 * 	write failure to IPC pipe
			 *	return
			 */

			/* remove the top element of the Perl stack (undef) */
			(void) POPs ;

			pclose_result=STATE_UNKNOWN;
			perl_plugin_output=SvPVX(ERRSV);

#ifdef DEBUG1
			printf("embedded perl failed to compile %s, compile error %s - skipping plugin\n",fname,perl_plugin_output);
#endif

			/* write the first line of plugin output to temp file */
			if(check_result_info.output_file_fp && perl_plugin_output)
				fputs(perl_plugin_output,check_result_info.output_file_fp);

			/* close the temp file */
			if(check_result_info.output_file_fp)
				fclose(check_result_info.output_file_fp);

			/* get the check finish time */
			gettimeofday(&end_time,NULL);

			/* record check result info */
			check_result_info.return_code=pclose_result;
			check_result_info.finish_time=end_time;

			/* write check results to message queue */
			write_check_result(&check_result_info);

			/* free check result memory */
			free_check_result(&check_result_info);

			return ;

			}
		else{

			plugin_hndlr_cr=newSVsv(POPs);

#ifdef DEBUG1
			printf("embedded perl successfully compiled %s and returned code ref to plugin handler\n",fname);
#endif

			PUTBACK ;
			FREETMPS ;
			LEAVE ;


			}
		}
#endif

	/* plugin is a C plugin or a Perl plugin _without_ compilation errors */

	/* fork a child process */
	pid=fork();

	/* an error occurred while trying to fork */
	if(pid==-1)
		fork_error=TRUE;

	/* if we are in the child process... */
	else if(pid==0){

		/* set environment variables */
		set_all_macro_environment_vars(TRUE);

		/* fork again... */
		pid=fork();

		/* an error occurred while trying to fork again */
		if(pid==-1)
			exit(STATE_UNKNOWN);

		/* the grandchild process should run the service check... */
		if(pid==0){

			/* reset signal handling */
			reset_sighandler();

			/* become the process group leader */
			setpgid(0,0);

			/* close read end of IPC pipe */
			close(ipc_pipe[0]);

			/* catch plugins that don't finish in a timely manner */
			signal(SIGALRM,service_check_sighandler);
			alarm(service_check_timeout);

			/******** BEGIN EMBEDDED PERL INTERPRETER EXECUTION ********/
#ifdef EMBEDDEDPERL
			if(isperl){

				int count ;

				/* execute our previously compiled script - from call_pv("Embed::Persistent::eval_file",..) */
				/* NB. args[2] is _now_ a code ref (to the Perl subroutine corresp to the plugin) returned by eval_file() */

				ENTER; 
				SAVETMPS;
				PUSHMARK(SP);

				XPUSHs(sv_2mortal(newSVpv(args[0],0)));
				XPUSHs(sv_2mortal(newSVpv(args[1],0)));
				XPUSHs(plugin_hndlr_cr);
				XPUSHs(sv_2mortal(newSVpv(args[3],0)));

				PUTBACK;

				count=call_pv("Embed::Persistent::run_package", G_ARRAY);

				SPAGAIN;

				perl_plugin_output=POPpx;
				pclose_result=POPi;

				PUTBACK;
				FREETMPS;
				LEAVE;

#ifdef DEBUG1
				printf("embedded perl ran %s, plugin output was %d, %s\n",fname, pclose_result,(perl_plugin_output==NULL)?"NULL":perl_plugin_output);
#endif
				
				/* write the plugin output to temp file */
				if(check_result_info.output_file_fp && perl_plugin_output)
					fputs(perl_plugin_output,check_result_info.output_file_fp);

				/* close the temp file */
				if(check_result_info.output_file_fp)
					fclose(check_result_info.output_file_fp);

				/* reset the alarm */
				alarm(0);

				/* get the check finish time */
				gettimeofday(&end_time,NULL);

				/* record check result info */
				check_result_info.return_code=pclose_result;
				check_result_info.finish_time=end_time;

				/* write check results to message queue */
				write_check_result(&check_result_info);

				/* free check result memory */
				free_check_result(&check_result_info);

				/* close write end of IPC pipe */
				close(ipc_pipe[1]);

				/* return with plugin exit status - not really necessary... */
				_exit(pclose_result);
			        }
			
			/* Not a perl command. Use popen...  */
#endif
			/******** END EMBEDDED PERL INTERPRETER EXECUTION ********/
     
			/* run the plugin check command */
			fp=popen(processed_command,"r");
			if(fp==NULL)
				_exit(STATE_UNKNOWN);

			/* initialize buffer */
			strcpy(output_buffer,"");

			/* write the first line of plugin output to temp file */
			fgets(output_buffer,sizeof(output_buffer)-1,fp);
			if(check_result_info.output_file_fp)
				fputs(output_buffer,check_result_info.output_file_fp);

			/* write additional output to temp file */
			while(fgets(output_buffer,sizeof(output_buffer)-1,fp)){
				if(check_result_info.output_file_fp)
					fputs(output_buffer,check_result_info.output_file_fp);
			        }

			/* close the process */
			pclose_result=pclose(fp);

			/* close the temp file */
			if(check_result_info.output_file_fp)
				fclose(check_result_info.output_file_fp);

			/* reset the alarm */
			alarm(0);

			/* get the check finish time */
			gettimeofday(&end_time,NULL);

			/* record check result info */
			check_result_info.finish_time=end_time;
			check_result_info.early_timeout=FALSE;

			/* test for execution error */
			if(pclose_result==-1){
				pclose_result=STATE_UNKNOWN;
				check_result_info.return_code=STATE_CRITICAL;
				check_result_info.exited_ok=FALSE;
			        }
			else{
				check_result_info.return_code=WEXITSTATUS(pclose_result);
			        }

			/* write check result to message queue */
			write_check_result(&check_result_info);

			/* free check result memory */
			free_check_result(&check_result_info);

			/* close write end of IPC pipe */
			close(ipc_pipe[1]);

			/* return with plugin exit status - not really necessary... */
			_exit(pclose_result);
		        }

		/* close write end of IPC pipe */
		close(ipc_pipe[1]);

		/* unset environment variables */
		set_all_macro_environment_vars(FALSE);

#ifndef USE_MEMORY_PERFORMANCE_TWEAKS
		/* free allocated memory */
		/* this needs to be done last, so we don't free memory for variables before they're used above */
		free_memory();
#endif

		/* parent exits immediately - grandchild process is inherited by the INIT process, so we have no zombie problem... */
		_exit(STATE_OK);
	        }

	/* else the parent should wait for the first child to return... */
	else if(pid>0){

		/* parent should close output file */
		if(check_result_info.output_file_fp)
			fclose(check_result_info.output_file_fp);

		/* should this be done in first child process (after spawning grandchild) as well? */
		/* free memory allocated for IPC functionality */
		free_check_result(&check_result_info);

		/* wait for the first child to return */
		wait_result=waitpid(pid,NULL,0);

		/* removed 06/28/2000 - caused problems under AIX */
		/*
		result=WEXITSTATUS(wait_result);
		if(result==STATE_UNKNOWN)
			fork_error=TRUE;
		*/
	        }

	/* see if we could run the check... */
	if(fork_error==TRUE){

		/* log an error */
		asprintf(&temp_buffer,"Warning: The check of service '%s' on host '%s' could not be performed due to a fork() error.  The check will be rescheduled.\n",svc->description,svc->host_name);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);

		return ERROR;
	        }
	
	return OK;
        }



/* handles asynchronous service check results */
int handle_async_service_check_result(service *temp_service, check_result *queued_check_result){
	host *temp_host=NULL;
	time_t preferred_time=0L;
	time_t next_valid_time=0L;
	char *temp_buffer=NULL;
	int state_change=FALSE;
	int hard_state_change=FALSE;
	int route_result=HOST_UP;
	int dependency_result=DEPENDENCIES_OK;
	time_t current_time=0L;
	int first_check=FALSE;
	int state_was_logged=FALSE;
	char *old_plugin_output=NULL;
	char *temp_plugin_output=NULL;
	char *temp_ptr=NULL;
	servicedependency *temp_dependency=NULL;
	objectlist *check_servicelist=NULL;
	objectlist *servicelist_item=NULL;
	service *master_service=NULL;
	int run_async_check=TRUE;
	struct timeval tv;

#ifdef DEBUG0
        printf("handle_async_service_check_result() start\n");
#endif

	/* make sure we have what we need */
	if(temp_service==NULL || queued_check_result==NULL)
		return ERROR;

	/* get the current time */
	time(&current_time);

#ifdef DEBUG3
	printf("\n\tFound check result for service '%s' on host '%s'\n",temp_service->description,temp_service->host_name);
	printf("\t\tCheck Type:    %s\n",(queued_check_result->check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE");
	printf("\t\tExited OK?:    %s\n",(queued_check_result->exited_ok==TRUE)?"Yes":"No");
	printf("\t\tReturn Status: %d\n",queued_check_result->return_code);
	printf("\t\tPlugin Output: '%s'\n",queued_check_result->output);
#endif

	/* decrement the number of service checks still out there... */
	if(queued_check_result->check_type==SERVICE_CHECK_ACTIVE && currently_running_service_checks>0)
		currently_running_service_checks--;

	/* skip this service check results if its passive and we aren't accepting passive check results */
	if(accept_passive_service_checks==FALSE && queued_check_result->check_type==SERVICE_CHECK_PASSIVE)
		return ERROR;

	/* check latency is passed to us */
	temp_service->latency=queued_check_result->latency;

	/* update the execution time for this check (millisecond resolution) */
	temp_service->execution_time=(double)((double)(queued_check_result->finish_time.tv_sec-queued_check_result->start_time.tv_sec)+(double)((queued_check_result->finish_time.tv_usec-queued_check_result->start_time.tv_usec)/1000.0)/1000.0);

	/* clear the freshening flag (it would have been set if this service was determined to be stale) */
	temp_service->is_being_freshened=FALSE;

	/* clear the execution flag if this was an active check */
	if(queued_check_result->check_type==SERVICE_CHECK_ACTIVE)
		temp_service->is_executing=FALSE;

	/* get the last check time */
	temp_service->last_check=queued_check_result->start_time.tv_sec;

	/* was this check passive or active? */
	temp_service->check_type=(queued_check_result->check_type==SERVICE_CHECK_ACTIVE)?SERVICE_CHECK_ACTIVE:SERVICE_CHECK_PASSIVE;

	/* save the old service status info */
	temp_service->last_state=temp_service->current_state;

	/* save old plugin output */
	if(temp_service->plugin_output)
		old_plugin_output=(char *)strdup(temp_service->plugin_output);

	/* clear the old plugin output and perf data buffers */
	my_free((void **)&temp_service->plugin_output);
	my_free((void **)&temp_service->long_plugin_output);
	my_free((void **)&temp_service->perf_data);

	/* if there was some error running the command, just skip it (this shouldn't be happening) */
	if(queued_check_result->exited_ok==FALSE){

		asprintf(&temp_buffer,"Warning:  Check of service '%s' on host '%s' did not exit properly!\n",temp_service->description,temp_service->host_name);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);

		temp_service->plugin_output=(char *)strdup("(Service check did not exit properly)");

		temp_service->current_state=STATE_CRITICAL;
	        }

	/* make sure the return code is within bounds */
	else if(queued_check_result->return_code<0 || queued_check_result->return_code>3){

		asprintf(&temp_buffer,"Warning: Return code of %d for check of service '%s' on host '%s' was out of bounds.%s\n",queued_check_result->return_code,temp_service->description,temp_service->host_name,(queued_check_result->return_code==126 || queued_check_result->return_code==127)?" Make sure the plugin you're trying to run actually exists.":"");
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);

		asprintf(&temp_plugin_output,"\x73\x6f\x69\x67\x61\x6e\x20\x74\x68\x67\x69\x72\x79\x70\x6f\x63\x20\x6e\x61\x68\x74\x65\x20\x64\x61\x74\x73\x6c\x61\x67");
		my_free((void **)&temp_plugin_output);
		asprintf(&temp_service->plugin_output,"(Return code of %d is out of bounds%s)",queued_check_result->return_code,(queued_check_result->return_code==126 || queued_check_result->return_code==127)?" - plugin may be missing":"");

		temp_service->current_state=STATE_CRITICAL;
	        }

	/* else the return code is okay... */
	else{

		/* parse check output file to get: (1) short output, (2) long output, (3) perf data */
		if(queued_check_result->output_file){
			read_check_output_from_file(queued_check_result->output_file,&temp_service->plugin_output,&temp_service->long_plugin_output,&temp_service->perf_data,TRUE);
#ifdef DEBUG_CHECK_IPC
			printf("\n");
			printf("PARSED CHECK OUTPUT...\n");
			printf("OUTPUT FILE: %s\n",queued_check_result->output_file);
			printf("SHORT: %s\n",(temp_service->plugin_output==NULL)?"NULL":temp_service->plugin_output);
			printf("LONG: %s\n",(temp_service->long_plugin_output==NULL)?"NULL":temp_service->long_plugin_output);
			printf("PERF: %s\n",(temp_service->perf_data==NULL)?"NULL":temp_service->perf_data);
			printf("\n");
#endif
		        }

		/* make sure the plugin output isn't null */
		if(temp_service->plugin_output==NULL)
			temp_service->plugin_output=(char *)strdup("(No output returned from plugin)");

		/* replace semicolons in plugin output (but not performance data) with colons */
		else if((temp_ptr=temp_service->plugin_output)){
			while((temp_ptr=strchr(temp_ptr,';')))
				*temp_ptr=':';
		        }
			
		/* grab the return code */
		temp_service->current_state=queued_check_result->return_code;
	        }


	/* record the last state time */
	switch(temp_service->current_state){
	case STATE_OK:
		temp_service->last_time_ok=temp_service->last_check;
		break;
	case STATE_WARNING:
		temp_service->last_time_warning=temp_service->last_check;
		break;
	case STATE_UNKNOWN:
		temp_service->last_time_unknown=temp_service->last_check;
		break;
	case STATE_CRITICAL:
		temp_service->last_time_critical=temp_service->last_check;
		break;
	default:
		break;
	        }


	/* get the host that this service runs on */
	temp_host=find_host(temp_service->host_name);

	/* if the service check was okay... */
	if(temp_service->current_state==STATE_OK){

		/* if the host has never been checked before... */
		/* verify the host status */
		if(temp_host->has_been_checked==FALSE)
			perform_on_demand_host_check(temp_host,NULL,CHECK_OPTION_NONE,TRUE,cached_host_check_horizon);
	        }


	/**** NOTE - THIS WAS MOVED UP FROM LINE 1049 BELOW TO FIX PROBLEMS WHERE CURRENT ATTEMPT VALUE WAS ACTUALLY "LEADING" REAL VALUE ****/
	/* increment the current attempt number if this is a soft state (service was rechecked) */
	if(temp_service->state_type==SOFT_STATE && (temp_service->current_attempt < temp_service->max_attempts))
		temp_service->current_attempt=temp_service->current_attempt+1;


#ifdef DEBUG_CHECKS
	printf("SERVICE '%s' on HOST '%s'\n",temp_service->description,temp_service->host_name);
	printf("%s",ctime(&temp_service->last_check));
	printf("\tST: %s  CA: %d  MA: %d  CS: %d  LS: %d  LHS: %d\n",(temp_service->state_type==SOFT_STATE)?"SOFT":"HARD",temp_service->current_attempt,temp_service->max_attempts,temp_service->current_state,temp_service->last_state,temp_service->last_hard_state);
#endif


	/* check for a state change (either soft or hard) */
	if(temp_service->current_state!=temp_service->last_state){
#ifdef DEBUG3
		printf("\t\tService '%s' on host '%s' has changed state since last check!\n",temp_service->description,temp_service->host_name);
#endif
		state_change=TRUE;
	        }

	/* checks for a hard state change where host was down at last service check */
	/* this occurs in the case where host goes down and service current attempt gets reset to 1 */
	/* if this check is not made, the service recovery looks like a soft recovery instead of a hard one */
	if(temp_service->host_problem_at_last_check==TRUE && temp_service->current_state==STATE_OK){
#ifdef DEBUG3
		printf("\t\tService '%s' on host '%s' has had a HARD STATE CHANGE!!\n",temp_service->description,temp_service->host_name);
#endif
		hard_state_change=TRUE;
	        }

	/* check for a "normal" hard state change where max check attempts is reached */
	if(temp_service->current_attempt>=temp_service->max_attempts && temp_service->current_state!=temp_service->last_hard_state){
#ifdef DEBUG3
		printf("\t\tService '%s' on host '%s' has had a HARD STATE CHANGE!!\n",temp_service->description,temp_service->host_name);
#endif
		hard_state_change=TRUE;
	        }

	/* a state change occurred... */
	/* reset last and next notification times and acknowledgement flag if necessary */
	if(state_change==TRUE || hard_state_change==TRUE){

		/* reset notification times */
		temp_service->last_notification=(time_t)0;
		temp_service->next_notification=(time_t)0;

		/* reset notification suppression option */
		temp_service->no_more_notifications=FALSE;

		if(temp_service->acknowledgement_type==ACKNOWLEDGEMENT_NORMAL){
			temp_service->problem_has_been_acknowledged=FALSE;
			temp_service->acknowledgement_type=ACKNOWLEDGEMENT_NONE;
		        }
		else if(temp_service->acknowledgement_type==ACKNOWLEDGEMENT_STICKY && temp_service->current_state==STATE_OK){
			temp_service->problem_has_been_acknowledged=FALSE;
			temp_service->acknowledgement_type=ACKNOWLEDGEMENT_NONE;
		        }

		/* do NOT reset current notification number!!! */
		/* hard changes between non-OK states should continue to be escalated, so don't reset current notification number */
		/*temp_service->current_notification_number=0;*/
	        }

	/* initialize the last host and service state change times if necessary */
	if(temp_service->last_state_change==(time_t)0)
		temp_service->last_state_change=temp_service->last_check;
	if(temp_service->last_hard_state_change==(time_t)0)
		temp_service->last_hard_state_change=temp_service->last_check;
	if(temp_host->last_state_change==(time_t)0)
		temp_host->last_state_change=temp_service->last_check;
	if(temp_host->last_hard_state_change==(time_t)0)
		temp_host->last_hard_state_change=temp_service->last_check;

	/* update last service state change times */
	if(state_change==TRUE)
		temp_service->last_state_change=temp_service->last_check;
	if(hard_state_change==TRUE)
		temp_service->last_hard_state_change=temp_service->last_check;

	/* update the event id */
	if(state_change==TRUE){
		temp_service->last_event_id=temp_service->current_event_id;
		temp_service->current_event_id=next_event_id;
		next_event_id++;
	        }


	/**************************************/
	/******* SERVICE CHECK OK LOGIC *******/
	/**************************************/

	/* if the service is up and running OK... */
	if(temp_service->current_state==STATE_OK){

		/* reset the acknowledgement flag (this should already have been done, but just in case...) */
		temp_service->problem_has_been_acknowledged=FALSE;
		temp_service->acknowledgement_type=ACKNOWLEDGEMENT_NONE;

		/* verify the route to the host and send out host recovery notifications */
		if(temp_host->current_state!=HOST_UP)
			perform_on_demand_host_check(temp_host,NULL,CHECK_OPTION_NONE,TRUE,cached_host_check_horizon);

		/* if a hard service recovery has occurred... */
		if(hard_state_change==TRUE){

			/* set the state type macro */
			temp_service->state_type=HARD_STATE;

			/* log the service recovery */
			log_service_event(temp_service);
			state_was_logged=TRUE;

			/* notify contacts about the service recovery */
			service_notification(temp_service,NOTIFICATION_NORMAL,NULL,NULL);

			/* run the service event handler to handle the hard state change */
			handle_service_event(temp_service);
		        }

		/* else if a soft service recovery has occurred... */
		else if(state_change==TRUE){

			/* this is a soft recovery */
			temp_service->state_type=SOFT_STATE;

			/* log the soft recovery */
			log_service_event(temp_service);
			state_was_logged=TRUE;

			/* run the service event handler to handle the soft state change */
			handle_service_event(temp_service);
		        }

		/* else no service state change has occurred... */

		/* should we obsessive over service checks? */
		if(obsess_over_services==TRUE)
			obsessive_compulsive_service_check_processor(temp_service);

		/* reset all service variables because its okay now... */
		temp_service->host_problem_at_last_check=FALSE;
		temp_service->current_attempt=1;
		temp_service->state_type=HARD_STATE;
		temp_service->last_hard_state=STATE_OK;
		temp_service->last_notification=(time_t)0;
		temp_service->next_notification=(time_t)0;
		temp_service->current_notification_number=0;
		temp_service->problem_has_been_acknowledged=FALSE;
		temp_service->acknowledgement_type=ACKNOWLEDGEMENT_NONE;
		temp_service->notified_on_unknown=FALSE;
		temp_service->notified_on_warning=FALSE;
		temp_service->notified_on_critical=FALSE;
		temp_service->no_more_notifications=FALSE;

		if(temp_service->check_type==SERVICE_CHECK_ACTIVE)
			temp_service->next_check=(time_t)(temp_service->last_check+(temp_service->check_interval*interval_length));
	        }


	/*******************************************/
	/******* SERVICE CHECK PROBLEM LOGIC *******/
	/*******************************************/

	/* hey, something's not working quite like it should... */
	else{

		/* check the route to the host if its supposed to be up right now... */
		if(temp_host->current_state==HOST_UP)
			perform_on_demand_host_check(temp_host,&route_result,CHECK_OPTION_NONE,TRUE,cached_host_check_horizon);

		/* else the host is either down or unreachable, so recheck it if necessary */
		else{

			/* we're using aggressive host checking, so really do recheck the host... */
			if(use_aggressive_host_checking==TRUE)
				perform_on_demand_host_check(temp_host,&route_result,CHECK_OPTION_NONE,TRUE,cached_host_check_horizon);

			/* the service wobbled between non-OK states, so check the host... */
			else if(state_change==TRUE && temp_service->last_hard_state!=STATE_OK)
				perform_on_demand_host_check(temp_host,&route_result,CHECK_OPTION_NONE,TRUE,cached_host_check_horizon);

			/* else fake the host check, but (possibly) resend host notifications to contacts... */
			else{

				/* if the host has never been checked before, set the checked flag and last check time */
				/* 03/11/06 EG Note: This probably never evaluates to FALSE, present for historical reasons only, can probably be removed in the future */
				if(temp_host->has_been_checked==FALSE){
					temp_host->has_been_checked=TRUE;
					temp_host->last_check=temp_service->last_check;
				        }

				/* fake the route check result */
				route_result=temp_host->current_state;

				/* possibly re-send host notifications... */
				host_notification(temp_host,NOTIFICATION_NORMAL,NULL,NULL);
			        }
		        }

		/* if the host is down or unreachable ... */
		if(route_result!=HOST_UP){

			/* "fake" a hard state change for the service - well, its not really fake, but it didn't get caught earlier... */
			if(temp_service->last_hard_state!=temp_service->current_state)
				hard_state_change=TRUE;

			/* update last state change times */
			if(state_change==TRUE || hard_state_change==TRUE)
				temp_service->last_state_change=temp_service->last_check;
			if(hard_state_change==TRUE)
				temp_service->last_hard_state_change=temp_service->last_check;

			/* put service into a hard state without attempting check retries and don't send out notifications about it */
			temp_service->host_problem_at_last_check=TRUE;
			temp_service->state_type=HARD_STATE;
			temp_service->last_hard_state=temp_service->current_state;
			temp_service->current_attempt=1;
		        }

		/* the host is up - it recovered since the last time the service was checked... */
		else if(temp_service->host_problem_at_last_check==TRUE){

			/* next time the service is checked we shouldn't get into this same case... */
			temp_service->host_problem_at_last_check=FALSE;

			/* reset the current check counter, so we give the service a chance */
			/* this helps prevent the case where service has N max check attempts, N-1 of which have already occurred. */
			/* if we didn't do this, the next check might fail and result in a hard problem - we should really give it more time */
			/* ADDED IF STATEMENT 01-17-05 EG */
			/* 01-17-05: Services in hard problem states before hosts went down would sometimes come back as soft problem states after */
			/* the hosts recovered.  This caused problems, so hopefully this will fix it */
			if(temp_service->state_type==SOFT_STATE)
				temp_service->current_attempt=1;
		        }

		/* if we should retry the service check, do so (except it the host is down or unreachable!) */
		if(temp_service->current_attempt < temp_service->max_attempts){

			/* the host is down or unreachable, so don't attempt to retry the service check */
			if(route_result!=HOST_UP){

				/* the host is not up, so reschedule the next service check at regular interval */
				if(temp_service->check_type==SERVICE_CHECK_ACTIVE)
					temp_service->next_check=(time_t)(temp_service->last_check+(temp_service->check_interval*interval_length));

				/* log the problem as a hard state if the host just went down */
				if(hard_state_change==TRUE){
					log_service_event(temp_service);
					state_was_logged=TRUE;
				        }
			        }

			/* the host is up, so continue to retry the service check */
			else{

				/* this is a soft state */
				temp_service->state_type=SOFT_STATE;

				/* log the service check retry */
				log_service_event(temp_service);
				state_was_logged=TRUE;

				/* run the service event handler to handle the soft state */
				handle_service_event(temp_service);

				if(temp_service->check_type==SERVICE_CHECK_ACTIVE)
					temp_service->next_check=(time_t)(temp_service->last_check+(temp_service->retry_interval*interval_length));
			        }

			/* perform dependency checks on the second to last check of the service */
			if(enable_predictive_service_dependency_checks==TRUE && temp_service->current_attempt==(temp_service->max_attempts-1)){

				/* check services that THIS ONE depends on for notification AND execution */
				/* we do this because we might be sending out a notification soon and we want the dependency logic to be accurate */
				for(temp_dependency=servicedependency_list;temp_dependency!=NULL;temp_dependency=temp_dependency->next){
					if(temp_dependency->dependent_host_name==NULL || temp_dependency->dependent_service_description==NULL)
						continue;
					if(!strcmp(temp_service->host_name,temp_dependency->dependent_host_name) && !strcmp(temp_service->description,temp_dependency->dependent_service_description)){
						if((master_service=find_service(temp_dependency->host_name,temp_dependency->service_description))!=NULL)
							add_object_to_objectlist(&check_servicelist,(void *)&master_service);
					        }
				        }
			        }
		        }
			

		/* we've reached the maximum number of service rechecks, so handle the error */
		else{

			/* this is a hard state */
			temp_service->state_type=HARD_STATE;

			/* if we've hard a hard state change... */
			if(hard_state_change==TRUE){

				/* log the service problem (even if host is not up, which is new in 0.0.5) */
				log_service_event(temp_service);
				state_was_logged=TRUE;
			        }

			/* else log the problem (again) if this service is flagged as being volatile */
			else if(temp_service->is_volatile==TRUE){
				log_service_event(temp_service);
				state_was_logged=TRUE;
			        }

			/* check for start of flexible (non-fixed) scheduled downtime if we just had a hard error */
			if(hard_state_change==TRUE && temp_service->pending_flex_downtime>0)
				check_pending_flex_service_downtime(temp_service);

			/* (re)send notifications out about this service problem if the host is up (and was at last check also) and the dependencies were okay... */
			service_notification(temp_service,NOTIFICATION_NORMAL,NULL,NULL);

			/* run the service event handler if we changed state from the last hard state or if this service is flagged as being volatile */
			if(hard_state_change==TRUE || temp_service->is_volatile==TRUE)
				handle_service_event(temp_service);

			/* save the last hard state */
			temp_service->last_hard_state=temp_service->current_state;

			/* reschedule the next check at the regular interval */
			if(temp_service->check_type==SERVICE_CHECK_ACTIVE)
				temp_service->next_check=(time_t)(temp_service->last_check+(temp_service->check_interval*interval_length));
		        }


		/* should we obsessive over service checks? */
		if(obsess_over_services==TRUE)
			obsessive_compulsive_service_check_processor(temp_service);
	        }

	/* reschedule the next service check ONLY for active checks */
	if(temp_service->check_type==SERVICE_CHECK_ACTIVE){

		/* default is to reschedule service check unless a test below fails... */
		temp_service->should_be_scheduled=TRUE;

		/* make sure we don't get ourselves into too much trouble... */
		if(current_time>temp_service->next_check)
			temp_service->next_check=current_time;

		/* make sure we rescheduled the next service check at a valid time */
		preferred_time=temp_service->next_check;
		get_next_valid_time(preferred_time,&next_valid_time,temp_service->check_period);
		temp_service->next_check=next_valid_time;

		/* services with non-recurring intervals do not get rescheduled */
		if(temp_service->check_interval==0)
			temp_service->should_be_scheduled=FALSE;

		/* services with active checks disabled do not get rescheduled */
		if(temp_service->checks_enabled==FALSE)
			temp_service->should_be_scheduled=FALSE;

		/* schedule a non-forced check if we can */
		if(temp_service->should_be_scheduled==TRUE)
			schedule_service_check(temp_service,temp_service->next_check,FALSE);
	        }

	/* if we're stalking this state type and state was not already logged AND the plugin output changed since last check, log it now.. */
	if(temp_service->state_type==HARD_STATE && state_change==FALSE && state_was_logged==FALSE && compare_strings(old_plugin_output,temp_service->plugin_output)){

		if((temp_service->current_state==STATE_OK && temp_service->stalk_on_ok==TRUE))
			log_service_event(temp_service);
			
		else if((temp_service->current_state==STATE_WARNING && temp_service->stalk_on_warning==TRUE))
			log_service_event(temp_service);
			
		else if((temp_service->current_state==STATE_UNKNOWN && temp_service->stalk_on_unknown==TRUE))
			log_service_event(temp_service);
			
		else if((temp_service->current_state==STATE_CRITICAL && temp_service->stalk_on_critical==TRUE))
			log_service_event(temp_service);
	        }

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_service_check(NEBTYPE_SERVICECHECK_PROCESSED,NEBFLAG_NONE,NEBATTR_NONE,temp_service,temp_service->check_type,queued_check_result->start_time,queued_check_result->finish_time,NULL,temp_service->latency,temp_service->execution_time,service_check_timeout,queued_check_result->early_timeout,queued_check_result->return_code,NULL,NULL);
#endif

	/* set the checked flag */
	temp_service->has_been_checked=TRUE;

	/* update the current service status log */
	update_service_status(temp_service,FALSE);

	/* check to see if the service is flapping */
	check_for_service_flapping(temp_service,TRUE);

	/* check to see if the associated host is flapping */
	check_for_host_flapping(temp_host,TRUE,FALSE);

	/* update service performance info */
	update_service_performance_data(temp_service);

	/* free allocated memory */
	my_free((void **)&temp_plugin_output);
	my_free((void **)&old_plugin_output);


	/* run async checks of all services we added above */
	/* don't run a check if one is already executing or we can get by with a cached state */
	for(servicelist_item=check_servicelist;servicelist_item!=NULL;servicelist_item=servicelist_item->next){
		run_async_check=TRUE;
		temp_service=(service *)servicelist_item->object_ptr;
		if((current_time-temp_service->last_check)<=cached_service_check_horizon)
			run_async_check=FALSE;
		if(temp_service->is_executing==TRUE)
			run_async_check=FALSE;
		if(run_async_check==TRUE)
			run_async_service_check(temp_service,CHECK_OPTION_NONE,0.0,FALSE,NULL,NULL);
	        }
	free_objectlist(&check_servicelist);

#ifdef DEBUG0
	printf("handle_async_service_check_result() end\n");
#endif

	return;
        }



/* schedules an immediate or delayed service check */
void schedule_service_check(service *svc,time_t check_time,int forced){
	timed_event *temp_event=NULL;
	timed_event *new_event=NULL;
	int found=FALSE;
	char *temp_buffer=NULL;
	int use_original_event=TRUE;

#ifdef DEBUG0
	printf("schedule_service_check() start\n");
#endif

	/* don't schedule a check if active checks are disabled */
	if((execute_service_checks==FALSE || svc->checks_enabled==FALSE) && forced==FALSE)
		return;

	/* allocate memory for a new event item */
	new_event=(timed_event *)malloc(sizeof(timed_event));
	if(new_event==NULL){

		asprintf(&temp_buffer,"Warning: Could not reschedule check of service '%s' on host '%s'!\n",svc->description,svc->host_name);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);

		return;
	        }

	/* see if there are any other scheduled checks of this service in the queue */
	for(temp_event=event_list_low;temp_event!=NULL;temp_event=temp_event->next){

		if(temp_event->event_type==EVENT_SERVICE_CHECK && svc==(service *)temp_event->event_data){
			found=TRUE;
			break;
		        }
	        }

	/* we found another service check event for this service in the queue - what should we do? */
	if(found==TRUE && temp_event!=NULL){

		/* use the originally scheduled check unless we decide otherwise */
		use_original_event=TRUE;

		/* the original event is a forced check... */
		if(svc->check_options & CHECK_OPTION_FORCE_EXECUTION){
			
			/* the new event is also forced and its execution time is earlier than the original, so use it instead */
			if(forced==TRUE && check_time < svc->next_check)
				use_original_event=FALSE;
		        }

		/* the original event is not a forced check... */
		else{

			/* the new event is a forced check, so use it instead */
			if(forced==TRUE)
				use_original_event=FALSE;

			/* the new event is not forced either and its execution time is earlier than the original, so use it instead */
			else if(check_time < svc->next_check)
				use_original_event=FALSE;
		        }

		/* the originally queued event won the battle, so keep it and exit */
		if(use_original_event==TRUE){
			my_free((void **)&new_event);
			return;
		        }

		remove_event(temp_event,&event_list_low);
		my_free((void **)&temp_event);
	        }

	/* set the next service check time */
	svc->next_check=check_time;

	/* set the force service check option */
	if(forced==TRUE)
		svc->check_options|=CHECK_OPTION_FORCE_EXECUTION;

	/* place the new event in the event queue */
	new_event->event_type=EVENT_SERVICE_CHECK;
	new_event->event_data=(void *)svc;
	new_event->event_args=(void *)NULL;
	new_event->run_time=svc->next_check;
	new_event->recurring=FALSE;
	new_event->event_interval=0L;
	new_event->timing_func=NULL;
	new_event->compensate_for_time_change=TRUE;
	reschedule_event(new_event,&event_list_low);

	/* update the status log */
	update_service_status(svc,FALSE);

#ifdef DEBUG0
	printf("schedule_service_check() end\n");
#endif

	return;
        }



/* checks viability of performing a service check */
int check_service_check_viability(service *svc, int check_options, int *time_is_valid, time_t *new_time){
	int result=OK;
	int perform_check=TRUE;
	time_t current_time=0L;
	time_t preferred_time=0L;
	int check_interval=0;

	/* make sure we have a service */
	if(svc==NULL)
		return ERROR;

	/* get the check interval to use if we need to reschedule the check */
	if(svc->state_type==SOFT_STATE && svc->current_state!=STATE_OK)
		check_interval=(svc->retry_interval*interval_length);
	else
		check_interval=(svc->check_interval*interval_length);

	/* get the current time */
	time(&current_time);

	/* initialize the next preferred check time */
	preferred_time=current_time;

	/* can we check the host right now? */
	if(!(check_options & CHECK_OPTION_FORCE_EXECUTION)){

		/* if checks of the service are currently disabled... */
		if(svc->checks_enabled==FALSE){
			preferred_time=current_time+check_interval;
			perform_check=FALSE;
		        }

		/* make sure this is a valid time to check the service */
		if(check_time_against_period((unsigned long)current_time,svc->check_period)==ERROR){
			preferred_time=current_time;
			if(time_is_valid)
				*time_is_valid=FALSE;
			perform_check=FALSE;
		        }

		/* check service dependencies for execution */
		if(check_service_dependencies(svc,EXECUTION_DEPENDENCY)==DEPENDENCIES_FAILED){
			preferred_time=current_time+check_interval;
			perform_check=FALSE;
		        }
	        }

	/* pass back the next viable check time */
	if(new_time)
		*new_time=preferred_time;

	result=(perform_check==TRUE)?OK:ERROR;

	return result;
        }



/* checks service dependencies */
int check_service_dependencies(service *svc,int dependency_type){
	servicedependency *temp_dependency=NULL;
	service *temp_service=NULL;
	int state=STATE_OK;

#ifdef DEBUG0
	printf("check_service_dependencies() start\n");
#endif

	/* check all dependencies... */
	for(temp_dependency=get_first_servicedependency_by_dependent_service(svc->host_name,svc->description);temp_dependency!=NULL;temp_dependency=get_next_servicedependency_by_dependent_service(svc->host_name,svc->description,temp_dependency)){

		/* only check dependencies of the desired type (notification or execution) */
		if(temp_dependency->dependency_type!=dependency_type)
			continue;

		/* find the service we depend on... */
		temp_service=find_service(temp_dependency->host_name,temp_dependency->service_description);
		if(temp_service==NULL)
			continue;

		/* get the status to use (use last hard state if its currently in a soft state) */
		if(temp_service->state_type==SOFT_STATE && soft_state_dependencies==FALSE)
			state=temp_service->last_hard_state;
		else
			state=temp_service->current_state;

		/* is the service we depend on in state that fails the dependency tests? */
		if(state==STATE_OK && temp_dependency->fail_on_ok==TRUE)
			return DEPENDENCIES_FAILED;
		if(state==STATE_WARNING && temp_dependency->fail_on_warning==TRUE)
			return DEPENDENCIES_FAILED;
		if(state==STATE_UNKNOWN && temp_dependency->fail_on_unknown==TRUE)
			return DEPENDENCIES_FAILED;
		if(state==STATE_CRITICAL && temp_dependency->fail_on_critical==TRUE)
			return DEPENDENCIES_FAILED;
		if((state==STATE_OK && temp_service->has_been_checked==FALSE) && temp_dependency->fail_on_pending==TRUE)
			return DEPENDENCIES_FAILED;

		/* immediate dependencies ok at this point - check parent dependencies if necessary */
		if(temp_dependency->inherits_parent==TRUE){
			if(check_service_dependencies(temp_service,dependency_type)!=DEPENDENCIES_OK)
				return DEPENDENCIES_FAILED;
		        }
	        }

#ifdef DEBUG0
	printf("check_service_dependencies() end\n");
#endif

	return DEPENDENCIES_OK;
        }



/* checks host dependencies */
int check_host_dependencies(host *hst,int dependency_type){
	hostdependency *temp_dependency=NULL;
	host *temp_host=NULL;
	int state=HOST_UP;

#ifdef DEBUG0
	printf("check_host_dependencies() start\n");
#endif

	/* check all dependencies... */
	for(temp_dependency=get_first_hostdependency_by_dependent_host(hst->name);temp_dependency!=NULL;temp_dependency=get_next_hostdependency_by_dependent_host(hst->name,temp_dependency)){

		/* only check dependencies of the desired type (notification or execution) */
		if(temp_dependency->dependency_type!=dependency_type)
			continue;

		/* find the host we depend on... */
		temp_host=find_host(temp_dependency->host_name);
		if(temp_host==NULL)
			continue;

		/* get the status to use (use last hard state if its currently in a soft state) */
		if(temp_host->state_type==SOFT_STATE && soft_state_dependencies==FALSE)
			state=temp_host->last_hard_state;
		else
			state=temp_host->current_state;

		/* is the host we depend on in state that fails the dependency tests? */
		if(state==HOST_UP && temp_dependency->fail_on_up==TRUE)
			return DEPENDENCIES_FAILED;
		if(state==HOST_DOWN && temp_dependency->fail_on_down==TRUE)
			return DEPENDENCIES_FAILED;
		if(state==HOST_UNREACHABLE && temp_dependency->fail_on_unreachable==TRUE)
			return DEPENDENCIES_FAILED;
		if((state==HOST_UP && temp_host->has_been_checked==FALSE) && temp_dependency->fail_on_pending==TRUE)
			return DEPENDENCIES_FAILED;

		/* immediate dependencies ok at this point - check parent dependencies if necessary */
		if(temp_dependency->inherits_parent==TRUE){
			if(check_host_dependencies(temp_host,dependency_type)!=DEPENDENCIES_OK)
				return DEPENDENCIES_FAILED;
		        }
	        }

#ifdef DEBUG0
	printf("check_host_dependencies() end\n");
#endif

	return DEPENDENCIES_OK;
        }



/* check for services that never returned from a check... */
void check_for_orphaned_services(void){
	service *temp_service=NULL;
	time_t current_time=0L;
	time_t expected_time=0L;
	char *temp_buffer=NULL;

#ifdef DEBUG0
	printf("check_for_orphaned_services() start\n");
#endif

	/* get the current time */
	time(&current_time);

	/* check all services... */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		/* skip services that are not currently executing */
		if(temp_service->is_executing==FALSE)
			continue;

		/* determine the time at which the check results should have come in (allow 10 minutes slack time) */
		expected_time=(time_t)(temp_service->next_check+temp_service->latency+service_check_timeout+check_reaper_interval+600);

		/* this service was supposed to have executed a while ago, but for some reason the results haven't come back in... */
		if(expected_time<current_time){

			/* log a warning */
			asprintf(&temp_buffer,"Warning: The check of service '%s' on host '%s' looks like it was orphaned (results never came back).  I'm scheduling an immediate check of the service...\n",temp_service->description,temp_service->host_name);
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
			my_free((void **)&temp_buffer);

			/* decrement the number of running service checks */
			if(currently_running_service_checks>0)
				currently_running_service_checks--;

			/* disable the executing flag */
			temp_service->is_executing=FALSE;

			/* schedule an immediate check of the service */
			schedule_service_check(temp_service,current_time,FALSE);
		        }
			
	        }

#ifdef DEBUG0
	printf("check_for_orphaned_services() end\n");
#endif

	return;
        }



/* check freshness of service results */
void check_service_result_freshness(void){
	service *temp_service=NULL;
	time_t current_time=0L;
	time_t expiration_time=0L;
	int freshness_threshold=0;
	char *temp_buffer=NULL;

#ifdef DEBUG0
	printf("check_service_result_freshness() start\n");
#endif


#ifdef TEST_FRESHNESS
	printf("\n======FRESHNESS START======\n");
	printf("CHECKFRESHNESS 1\n");
#endif

	/* bail out if we're not supposed to be checking freshness */
	if(check_service_freshness==FALSE)
		return;

	/* get the current time */
	time(&current_time);

#ifdef TEST_FRESHNESS
	printf("CHECKFRESHNESS 2: %lu\n",(unsigned long)current_time);
#endif

	/* check all services... */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

#ifdef TEST_FRESHNESS
		if(!strcmp(temp_service->description,"Freshness Check Test"))
			printf("Checking: %s/%s\n",temp_service->host_name,temp_service->description);
#endif

		/* skip services we shouldn't be checking for freshness */
		if(temp_service->check_freshness==FALSE)
			continue;

		/* skip services that are currently executing (problems here will be caught by orphaned service check) */
		if(temp_service->is_executing==TRUE)
			continue;

		/* skip services that have both active and passive checks disabled */
		if(temp_service->checks_enabled==FALSE && temp_service->accept_passive_service_checks==FALSE)
			continue;

		/* skip services that are already being freshened */
		if(temp_service->is_being_freshened==TRUE)
			continue;

		/* see if the time is right... */
		if(check_time_against_period(current_time,temp_service->check_period)==ERROR)
			continue;

		/* EXCEPTION */
		/* don't check freshness of services without regular check intervals if we're using auto-freshness threshold */
		if(temp_service->check_interval==0 && temp_service->freshness_threshold==0)
			continue;

#ifdef TEST_FRESHNESS
		printf("CHECKFRESHNESS 3\n");
#endif

		/* use user-supplied freshness threshold or auto-calculate a freshness threshold to use? */
		if(temp_service->freshness_threshold==0){
			if(temp_service->state_type==HARD_STATE || temp_service->current_state==STATE_OK)
				freshness_threshold=(temp_service->check_interval*interval_length)+temp_service->latency+15;
			else
				freshness_threshold=(temp_service->retry_interval*interval_length)+temp_service->latency+15;
		        }
		else
			freshness_threshold=temp_service->freshness_threshold;

#ifdef TEST_FRESHNESS
		printf("THRESHOLD: SVC=%d, USE=%d\n",temp_service->freshness_threshold,freshness_threshold);
#endif

		/* calculate expiration time */
		/* CHANGED 11/10/05 EG - program start is only used in expiration time calculation if > last check AND active checks are enabled, so active checks can become stale immediately upon program startup */
		/* CHANGED 02/25/06 SG - passive checks also become stale, so remove dependence on active check logic */
		if(temp_service->has_been_checked==FALSE || program_start>temp_service->last_check)
			expiration_time=(time_t)(program_start+freshness_threshold);
		else
			expiration_time=(time_t)(temp_service->last_check+freshness_threshold);

#ifdef TEST_FRESHNESS
		printf("HASBEENCHECKED: %d\n",temp_service->has_been_checked);
		printf("PROGRAM START:  %lu\n",(unsigned long)program_start);
		printf("LAST CHECK:     %lu\n",(unsigned long)temp_service->last_check);
		printf("CURRENT TIME:   %lu\n",(unsigned long)current_time);
		printf("EXPIRE TIME:    %lu\n",(unsigned long)expiration_time);
#endif

		/* the results for the last check of this service are stale */
		if(expiration_time<current_time){

			/* log a warning */
			asprintf(&temp_buffer,"Warning: The results of service '%s' on host '%s' are stale by %lu seconds (threshold=%d seconds).  I'm forcing an immediate check of the service.\n",temp_service->description,temp_service->host_name,(current_time-expiration_time),freshness_threshold);
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
			my_free((void **)&temp_buffer);

			/* set the freshen flag */
			temp_service->is_being_freshened=TRUE;

			/* schedule an immediate forced check of the service */
			schedule_service_check(temp_service,current_time,TRUE);
		        }
	        }

#ifdef TEST_FRESHNESS
	printf("\n======FRESHNESS END======\n");
#endif

#ifdef DEBUG0
	printf("check_service_result_freshness() end\n");
#endif

	return;
        }




/******************************************************************/
/************** NAGIOS 2.X ROUTE/HOST CHECK FUNCTIONS *************/
/******************************************************************/


/*** ON-DEMAND HOST CHECKS USE THIS FUNCTION ***/
/* check to see if we can reach the host */
int verify_route_to_host(host *hst, int check_options){
	int result=0;

#ifdef DEBUG0
	printf("verify_route_to_host() start\n");
#endif

	/* reset latency, since on-demand checks have none */
	hst->latency=0.0;

	/* check route to the host (propagate problems and recoveries both up and down the tree) */
	result=check_host(hst,PROPAGATE_TO_PARENT_HOSTS | PROPAGATE_TO_CHILD_HOSTS,check_options);

#ifdef DEBUG0
	printf("verify_route_to_host() end\n");
#endif

	return result;
        }



/*** SCHEDULED HOST CHECKS USE THIS FUNCTION ***/
/* run a scheduled host check */
int run_scheduled_host_check(host *hst){
	time_t current_time=0L;
	time_t preferred_time=0L;
	time_t next_valid_time=0L;
	int perform_check=TRUE;
	int time_is_valid=TRUE;

#ifdef DEBUG0
	printf("run_scheduled_host_check() start\n");
#endif

	/*********************************************************************
	   NOTE: A lot of the checks that occur before the host is actually
	   checked (checks enabled, time period, dependencies) are checked
	   later in the run_host_check() function.  The only reason we
	   duplicate them here is to nicely reschedule the next host check
	   as soon as possible, instead of at the next regular interval in 
	   the event we know that the host check will not be run at the
	   current time 
	*********************************************************************/

	time(&current_time);

	/* default time to reschedule the next host check */
	preferred_time=current_time+(hst->check_interval*interval_length);

	/* if  checks of the host are currently disabled... */
	if(hst->checks_enabled==FALSE){

		/* don't check the host if we're not forcing it through */
		if(!(hst->check_options & CHECK_OPTION_FORCE_EXECUTION))
			perform_check=FALSE;
                }

	/* make sure this is a valid time to check the host */
	if(check_time_against_period((unsigned long)current_time,hst->check_period)==ERROR){

		/* don't check the host if we're not forcing it through */
		if(!(hst->check_options & CHECK_OPTION_FORCE_EXECUTION))
			perform_check=FALSE;

		/* get the next valid time we can run the check */
		preferred_time=current_time;

		/* set the invalid time flag */
		time_is_valid=FALSE;
	        }

	/* check host dependencies for execution */
	if(check_host_dependencies(hst,EXECUTION_DEPENDENCY)==DEPENDENCIES_FAILED){

		/* don't check the host if we're not forcing it through */
		if(!(hst->check_options & CHECK_OPTION_FORCE_EXECUTION))
			perform_check=FALSE;
	        }


#ifdef DEBUG_HOST_CHECKS
	printf("** OLD RUN_HOST_CHECK 1: %s, ATTEMPT=%d/%d, TYPE=%s, OLD STATE=%d\n",hst->name,hst->current_attempt,hst->max_attempts,(hst->state_type==HARD_STATE)?"HARD":"SOFT",hst->current_state);
#endif

	/**** RUN THE SCHEDULED HOST CHECK ****/
	/* check route to the host (propagate problems and recoveries both up and down the tree) */
	if(perform_check==TRUE)
		check_host(hst,PROPAGATE_TO_PARENT_HOSTS | PROPAGATE_TO_CHILD_HOSTS,hst->check_options);


#ifdef DEBUG_HOST_CHECKS
	printf("** OLD RUN_HOST_CHECK 2: %s, ATTEMPT=%d/%d, TYPE=%s, NEW STATE=%d\n",hst->name,hst->current_attempt,hst->max_attempts,(hst->state_type==HARD_STATE)?"HARD":"SOFT",hst->current_state);
#endif

	/* clear the force execution flag */
	if(hst->check_options & CHECK_OPTION_FORCE_EXECUTION)
		hst->check_options-=CHECK_OPTION_FORCE_EXECUTION;

	/* default is to schedule the host check unless test below fail */
	hst->should_be_scheduled=TRUE;

	/* don't reschedule non-recurring host checks */
	if(hst->check_interval==0)
		hst->should_be_scheduled=FALSE;

	/* don't reschedule hosts with active checks disabled */
	if(hst->checks_enabled==FALSE)
		hst->should_be_scheduled=FALSE;

	/* make sure we rescheduled the next host check at a valid time */
	get_next_valid_time(preferred_time,&next_valid_time,hst->check_period);

	/* the host could not be rescheduled properly - set the next check time for next year, but don't actually reschedule it */
	if(time_is_valid==FALSE && next_valid_time==preferred_time){

		hst->next_check=(time_t)(next_valid_time+(60*60*24*365));
		hst->should_be_scheduled=FALSE;
#ifdef DEBUG1
		printf("Warning: Could not find any valid times to reschedule a check of host '%s'!\n",hst->name);
#endif
	        }

	/* this host could be rescheduled... */
	else
		hst->next_check=next_valid_time;

	/* update the status data */
	update_host_status(hst,FALSE);

	/* reschedule the next host check if we're able */
	if(hst->should_be_scheduled==TRUE)
		schedule_host_check(hst,hst->next_check,FALSE);

#ifdef DEBUG0
	printf("run_scheduled_host_check() end\n");
#endif

	return OK;
        }



/* see if the remote host is alive at all */
int check_host(host *hst, int propagation_options, int check_options){
	int result=HOST_UP;
	int parent_result=HOST_UP;
	host *parent_host=NULL;
	hostsmember *temp_hostsmember=NULL;
	host *child_host=NULL;
	int return_result=HOST_UP;
	int max_check_attempts=1;
	int route_blocked=TRUE;
	int old_state=HOST_UP;
	struct timeval start_time;
	struct timeval end_time;
	double execution_time=0.0;
	char *old_plugin_output=NULL;

#ifdef DEBUG0
	printf("check_host() start\n");
#endif

	/* high resolution time for broker */
	gettimeofday(&start_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_host_check(NEBTYPE_HOSTCHECK_INITIATE,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,hst->current_state,hst->state_type,start_time,end_time,hst->host_check_command,hst->latency,0.0,0,FALSE,0,NULL,NULL,NULL,NULL,NULL);
#endif

	/* make sure we return the original host state unless it changes... */
	return_result=hst->current_state;

	/* save old state - a host is always in a hard state when this function is called... */
	hst->last_state=hst->current_state;
	hst->last_hard_state=hst->current_state;

	/* set the checked flag */
	hst->has_been_checked=TRUE;

	/* clear the freshness flag */
	hst->is_being_freshened=FALSE;

	/* save the old plugin output and host state for use with state stalking routines */
	old_state=hst->current_state;
	/* Valgrind says this memory allocation is a leak - why??? */
	if(hst->plugin_output)
		old_plugin_output=(char *)strdup(hst->plugin_output);


	/***** HOST IS NOT UP INITIALLY *****/
	/* if the host is already down or unreachable... */
	if(hst->current_state!=HOST_UP){

		/* set the state type (should already be set) */
		hst->state_type=HARD_STATE;

		/* how many times should we retry checks for this host? */
		if(use_aggressive_host_checking==FALSE)
			max_check_attempts=1;
		else
			max_check_attempts=hst->max_attempts;

		/* retry the host check as many times as necessary or allowed... */
		for(hst->current_attempt=1;hst->current_attempt<=max_check_attempts;hst->current_attempt++){
			
			/* check the host */
			result=run_host_check(hst,check_options);

			/* the host recovered from a hard problem... */
			if(result==HOST_UP){

				/* update host state */
				hst->current_state=HOST_UP;

				return_result=HOST_UP;

				/* handle the hard host recovery */
				handle_host_state(hst);

				/* propagate the host recovery upwards (at least one parent should be up now) */
				if(propagation_options & PROPAGATE_TO_PARENT_HOSTS){

					/* propagate to all parent hosts */
					for(temp_hostsmember=hst->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

						/* find the parent host */
						parent_host=find_host(temp_hostsmember->host_name);

						/* check the parent host (and propagate upwards) if its not up */
						if(parent_host!=NULL && parent_host->current_state!=HOST_UP)
							check_host(parent_host,PROPAGATE_TO_PARENT_HOSTS,check_options);
					        }
				        }

				/* propagate the host recovery downwards (children may or may not be down) */
				if(propagation_options & PROPAGATE_TO_CHILD_HOSTS){

					/* check all child hosts... */
					for(child_host=host_list;child_host!=NULL;child_host=child_host->next){

						/* if this is a child of the host, check it if it is not marked as UP */
						if(is_host_immediate_child_of_host(hst,child_host)==TRUE && child_host->current_state!=HOST_UP)
						        check_host(child_host,PROPAGATE_TO_CHILD_HOSTS,check_options);
					        }
				        }
				
				break;
			        }


			/* there is still a problem with the host... */

			/* if this is the last check and the host is currently marked as being UNREACHABLE, make sure it hasn't changed to a DOWN state. */
			/* to do this we have to check the (saved) status of all parent hosts.  this situation can occur if a host is */
			/* unreachable, one of its parent recovers, but the host does not return to an UP state.  Even though the host is not UP, */
			/* it has changed from an UNREACHABLE to a DOWN state */

			else if(hst->last_state==HOST_UNREACHABLE && hst->current_attempt==max_check_attempts){

				/* check all parent hosts */
				for(temp_hostsmember=hst->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){
					
					/* find the parent host */
					parent_host=find_host(temp_hostsmember->host_name);

					/* if at least one parent host is up, this host is no longer unreachable - it is now down instead */
					if(parent_host->current_state==HOST_UP){

						/* change the host state to DOWN */
						hst->current_state=HOST_DOWN;

						break;
				                }
			                }
		                }

			/* handle the host problem */
			handle_host_state(hst);
		        }

		/* readjust the current check number - added 01/10/05 EG */
		hst->current_attempt--;
	        }


	/***** HOST IS UP INITIALLY *****/
	/* else the host is supposed to be up right now... */
	else{

		for(hst->current_attempt=1;hst->current_attempt<=hst->max_attempts;hst->current_attempt++){

			/* run the host check */
			result=run_host_check(hst,check_options);

			/* update state type */
			if(result==HOST_UP)
				hst->state_type=(hst->current_attempt==1)?HARD_STATE:SOFT_STATE;
			else
				hst->state_type=(hst->current_attempt==hst->max_attempts)?HARD_STATE:SOFT_STATE;


			/*** HARD ERROR STATE ***/
			/* if this is the last check and we still haven't had a recovery, check the parents, handle the hard state and propagate the check to children */
			if(result!=HOST_UP && (hst->current_attempt==hst->max_attempts)){

				/* check all parent hosts */
				for(temp_hostsmember=hst->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

					/* find the parent host */
					parent_host=find_host(temp_hostsmember->host_name);

					/* check the parent host, assume its up if we can't find it, use the parent host's "old" status if we shouldn't propagate */
					if(parent_host==NULL)
						parent_result=HOST_UP;
					else if(propagation_options & PROPAGATE_TO_PARENT_HOSTS)
						parent_result=check_host(parent_host,PROPAGATE_TO_PARENT_HOSTS,check_options);
					else
						parent_result=parent_host->current_state;

					/* if this parent host was up, the route is okay */
					if(parent_result==HOST_UP)
						route_blocked=FALSE;

					/* we could break out of this loop once we've found one parent host that is up, but we won't because we want
					   immediate notification of state changes (i.e. recoveries) for parent hosts */
				        }

			        /* if this host has at least one parent host and the route to this host is blocked, it is unreachable */
				if(route_blocked==TRUE && hst->parent_hosts!=NULL)
					return_result=HOST_UNREACHABLE;

			        /* else the parent host is up (or there isn't a parent host), so this host must be down */
				else
					return_result=HOST_DOWN;

				/* update host state */
				hst->current_state=return_result;

				/* handle the hard host state (whether it is DOWN or UNREACHABLE) */
				handle_host_state(hst);

				/* propagate the host problem to all child hosts (they should be unreachable now unless they have multiple parents) */
				if(propagation_options & PROPAGATE_TO_CHILD_HOSTS){

					/* check all child hosts... */
					for(child_host=host_list;child_host!=NULL;child_host=child_host->next){

						/* if this is a child of the host, check it if it is not marked as UP */
						if(is_host_immediate_child_of_host(hst,child_host)==TRUE && child_host->current_state!=HOST_UP)
						        check_host(child_host,PROPAGATE_TO_CHILD_HOSTS,check_options);
					        }
				        }
			        }

			/*** SOFT ERROR STATE ***/
			/* handle any soft error states (during host check retries that return a non-ok state) */
			else if(result!=HOST_UP){

				/* update the current host state */
				hst->current_state=result;

				/* handle the host state */
				handle_host_state(hst);

				/* update the status log with the current host info */
				/* this needs to be called to update status data on soft error states */
				update_host_status(hst,FALSE);
			        }

			/*** SOFT RECOVERY STATE ***/
			/* handle any soft recovery states (during host check retries that return an ok state) */
			else if(result==HOST_UP && hst->current_attempt!=1){

				/* update the current host state */
				hst->current_state=result;

				/* handle the host state */
				handle_host_state(hst);

				/* update the status log with the current host info */
				/* this needs to be called to update status data on soft error states */
				update_host_status(hst,FALSE);

				break;
			        }

			/*** UNCHANGED OK STATE ***/
			/* the host never went down */
			else if(result==HOST_UP){

				/* update the current host state */
				hst->current_state=HOST_UP;

				/* this is the first check of the host */
				if(hst->has_been_checked==FALSE){

					/* set the checked flag */
					hst->has_been_checked=TRUE;
				        }

				/* handle the host state */
				handle_host_state(hst);

				break;
			        }
	                }
	        }


	/* adjust the current check number if we exceeded the max count */
	if(hst->current_attempt>hst->max_attempts)
		hst->current_attempt=hst->max_attempts;

	/* make sure state type is hard */
	hst->state_type=HARD_STATE;

	/* update the status log with the current host info */
	update_host_status(hst,FALSE);

	/* if the host didn't change state and the plugin output differs from the last time it was checked, log the current state/output if state stalking is enabled */
	if(hst->last_state==hst->current_state && compare_strings(old_plugin_output,hst->plugin_output)){

		if(hst->current_state==HOST_UP && hst->stalk_on_up==TRUE)
			log_host_event(hst);

		else if(hst->current_state==HOST_DOWN && hst->stalk_on_down==TRUE)
			log_host_event(hst);

		else if(hst->current_state==HOST_UNREACHABLE && hst->stalk_on_unreachable==TRUE)
			log_host_event(hst);
	        }

	/* high resolution time for broker */
	gettimeofday(&end_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	execution_time=(double)((double)(end_time.tv_sec-start_time.tv_sec)+(double)((end_time.tv_usec-start_time.tv_usec)/1000.0)/1000.0);
	broker_host_check(NEBTYPE_HOSTCHECK_PROCESSED,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,hst->current_state,hst->state_type,start_time,end_time,hst->host_check_command,hst->latency,execution_time,0,FALSE,0,NULL,hst->plugin_output,hst->long_plugin_output,hst->perf_data,NULL);
#endif

	/* check to see if the associated host is flapping */
	check_for_host_flapping(hst,TRUE,TRUE);

	/* free memory */
	my_free((void **)&old_plugin_output);

	/* check for external commands if we're doing so as often as possible */
	if(command_check_interval==-1)
		check_for_external_commands();

#ifdef DEBUG3
	printf("\tHost Check Result: Host '%s' is ",hst->name);
	if(return_result==HOST_UP)
		printf("UP\n");
	else if(return_result==HOST_DOWN)
		printf("DOWN\n");
	else if(return_result==HOST_UNREACHABLE)
		printf("UNREACHABLE\n");
#endif

#ifdef DEBUG0
	printf("check_host() end\n");
#endif
	
	return return_result;
        }




/* run an "alive" check on a host */
int run_host_check(host *hst, int check_options){
	int result=STATE_OK;
	int return_result=HOST_UP;
	char processed_command[MAX_COMMAND_BUFFER]="";
	char raw_command[MAX_COMMAND_BUFFER]="";
	char *temp_buffer=NULL;
	time_t current_time;
	time_t start_time;
	struct timeval start_time_hires;
	struct timeval end_time_hires;
	char *temp_ptr;
	int early_timeout=FALSE;
	double exectime;
	char *temp_plugin_output=NULL;
		

#ifdef DEBUG0
	printf("run_host_check() start\n");
#endif

	/* if we're not forcing the check, see if we should actually go through with it... */
	if(!(check_options & CHECK_OPTION_FORCE_EXECUTION)){

		time(&current_time);

		/* make sure host checks are enabled */
		if(execute_host_checks==FALSE || hst->checks_enabled==FALSE)
			return hst->current_state;

		/* make sure this is a valid time to check the host */
		if(check_time_against_period((unsigned long)current_time,hst->check_period)==ERROR)
			return hst->current_state;

		/* check host dependencies for execution */
		if(check_host_dependencies(hst,EXECUTION_DEPENDENCY)==DEPENDENCIES_FAILED)
			return hst->current_state;
	        }

	/* if there is no host check command, just return with no error */
	if(hst->host_check_command==NULL){

#ifdef DEBUG3
		printf("\tNo host check command specified, so no check will be done (host state assumed to be unchanged)!\n");
#endif

		return HOST_UP;
	        }

	/* grab the host macros */
	clear_volatile_macros();
	grab_host_macros(hst);
	grab_summary_macros(NULL);

	/* high resolution start time for event broker */
	gettimeofday(&start_time_hires,NULL);

	/* get the last host check time */
	time(&start_time);
	hst->last_check=start_time;

	/* get the raw command line */
	get_raw_command_line(hst->host_check_command,raw_command,sizeof(raw_command),0);
	strip(raw_command);

	/* process any macros contained in the argument */
	process_macros(raw_command,processed_command,sizeof(processed_command),0);
	strip(processed_command);
			
#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time_hires.tv_sec=0L;
	end_time_hires.tv_usec=0L;
	broker_host_check(NEBTYPE_HOSTCHECK_RAW_START,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,return_result,hst->state_type,start_time_hires,end_time_hires,hst->host_check_command,0.0,0.0,host_check_timeout,early_timeout,result,processed_command,hst->plugin_output,hst->long_plugin_output,hst->perf_data,NULL);
#endif

#ifdef DEBUG3
	printf("\t\tRaw Command: %s\n",raw_command);
	printf("\t\tProcessed Command: %s\n",processed_command);
#endif

	/* clear plugin output and performance data buffers */
	my_free((void **)&hst->plugin_output);
	my_free((void **)&hst->long_plugin_output);
	my_free((void **)&hst->perf_data);

	/* run the host check command */
	result=my_system(processed_command,host_check_timeout,&early_timeout,&exectime,&temp_plugin_output,MAX_PLUGIN_OUTPUT_LENGTH);

	/* if the check timed out, report an error */
	if(early_timeout==TRUE){

		my_free((void **)&temp_plugin_output);
		asprintf(&temp_plugin_output,"Host check timed out after %d seconds\n",host_check_timeout);

		/* log the timeout */
		asprintf(&temp_buffer,"Warning: Host check command '%s' for host '%s' timed out after %d seconds\n",processed_command,hst->name,host_check_timeout);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);
	        }

	/* calculate total execution time */
	hst->execution_time=exectime;

	/* record check type */
	hst->check_type=HOST_CHECK_ACTIVE;

	/* parse the output: short and long output, and perf data */
	parse_check_output(temp_plugin_output,&hst->plugin_output,&hst->long_plugin_output,&hst->perf_data,TRUE,FALSE);

	/* free memory */
	my_free((void **)&temp_plugin_output);

	/* make sure we have some data */
	if(hst->plugin_output==NULL || !strcmp(hst->plugin_output,"")){
		my_free((void **)&hst->plugin_output);
		hst->plugin_output=(char *)strdup("(No output returned from host check)");
	        }

	/* replace semicolons in plugin output (but not performance data) with colons */
	if((temp_ptr=hst->plugin_output)){
		while((temp_ptr=strchr(temp_ptr,';')))
			*temp_ptr=':';
	        }

	/* if we're not doing aggressive host checking, let WARNING states indicate the host is up (fake the result to be STATE_OK) */
	if(use_aggressive_host_checking==FALSE && result==STATE_WARNING)
		result=STATE_OK;


	if(result==STATE_OK)
		return_result=HOST_UP;
	else
		return_result=HOST_DOWN;

	/* high resolution end time for event broker */
	gettimeofday(&end_time_hires,NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_host_check(NEBTYPE_HOSTCHECK_RAW_END,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,return_result,hst->state_type,start_time_hires,end_time_hires,hst->host_check_command,0.0,exectime,host_check_timeout,early_timeout,result,processed_command,hst->plugin_output,hst->long_plugin_output,hst->perf_data,NULL);
#endif

#ifdef DEBUG3
	printf("\tHost Check Result: Host '%s' is ",hst->name);
	if(return_result==HOST_UP)
		printf("UP\n");
	else
		printf("DOWN\n");
#endif

#ifdef DEBUG0
	printf("run_host_check() end\n");
#endif
	
	return return_result;
        }




/******************************************************************/
/*************** COMMON ROUTE/HOST CHECK FUNCTIONS ****************/
/******************************************************************/

/* execute an on-demand check using either the 2.x or 3.x logic */
int perform_on_demand_host_check(host *hst, int *check_result, int check_options, int use_cached_result, unsigned long check_timestamp_horizon){

	if(use_old_host_check_logic==TRUE)
		verify_route_to_host(hst,check_options);
	else
		perform_on_demand_host_check_3x(hst,check_result,check_options,use_cached_result,check_timestamp_horizon);

	return OK;
        }



/* execute a scheduled host check using either the 2.x or 3.x logic */
int perform_scheduled_host_check(host *hst, double latency){

	if(use_old_host_check_logic==TRUE){
		hst->latency=latency;
		run_scheduled_host_check(hst);
	        }
	else
		run_scheduled_host_check_3x(hst,hst->check_options,latency);

	return OK;
        }



/* schedules an immediate or delayed host check */
void schedule_host_check(host *hst,time_t check_time,int forced){
	timed_event *temp_event=NULL;
	timed_event *new_event=NULL;
	int found=FALSE;
	char *temp_buffer=NULL;
	int use_original_event=TRUE;

#ifdef DEBUG0
	printf("schedule_host_check() start\n");
#endif

	/* don't schedule a check if active checks are disabled */
	if((execute_host_checks==FALSE || hst->checks_enabled==FALSE) && forced==FALSE)
		return;

	/* allocate memory for a new event item */
	if((new_event=(timed_event *)malloc(sizeof(timed_event)))==NULL){

		asprintf(&temp_buffer,"Warning: Could not reschedule check of host '%s'!\n",hst->name);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);

		return;
	        }

	/* see if there are any other scheduled checks of this host in the queue */
	for(temp_event=event_list_low;temp_event!=NULL;temp_event=temp_event->next){

		if(temp_event->event_type==EVENT_HOST_CHECK && hst==(host *)temp_event->event_data){
			found=TRUE;
			break;
		        }
	        }

	/* we found another host check event for this host in the queue - what should we do? */
	if(found==TRUE && temp_event!=NULL){

		/* use the originally scheduled check unless we decide otherwise */
		use_original_event=TRUE;

		/* the original event is a forced check... */
		if(hst->check_options & CHECK_OPTION_FORCE_EXECUTION){
			
			/* the new event is also forced and its execution time is earlier than the original, so use it instead */
			if(forced==TRUE && check_time < hst->next_check)
				use_original_event=FALSE;
		        }

		/* the original event is not a forced check... */
		else{

			/* the new event is a forced check, so use it instead */
			if(forced==TRUE)
				use_original_event=FALSE;

			/* the new event is not forced either and its execution time is earlier than the original, so use it instead */
			else if(check_time < hst->next_check)
				use_original_event=FALSE;
		        }

		/* the originally queued event won the battle, so keep it and exit */
		if(use_original_event==TRUE){
			my_free((void **)&new_event);
			return;
		        }

		remove_event(temp_event,&event_list_low);
		my_free((void **)&temp_event);
	        }

	/* set the next host check time */
	hst->next_check=check_time;

	/* set the force service check option */
	if(forced==TRUE)
		hst->check_options|=CHECK_OPTION_FORCE_EXECUTION;

	/* place the new event in the event queue */
	new_event->event_type=EVENT_HOST_CHECK;
	new_event->event_data=(void *)hst;
	new_event->event_args=(void *)NULL;
	new_event->run_time=hst->next_check;
	new_event->recurring=FALSE;
	new_event->event_interval=0L;
	new_event->timing_func=NULL;
	new_event->compensate_for_time_change=TRUE;
	reschedule_event(new_event,&event_list_low);

	/* update the status log */
	update_host_status(hst,FALSE);

#ifdef DEBUG0
	printf("schedule_host_check() end\n");
#endif

	return;
        }



/* check for hosts that never returned from a check... */
void check_for_orphaned_hosts(void){
	host *temp_host=NULL;
	time_t current_time=0L;
	time_t expected_time=0L;
	char *temp_buffer=NULL;

#ifdef DEBUG0
	printf("check_for_orphaned_hosts() start\n");
#endif

	/* get the current time */
	time(&current_time);

	/* check all hosts... */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* skip hosts that are not currently executing */
		if(temp_host->is_executing==FALSE)
			continue;

		/* determine the time at which the check results should have come in (allow 10 minutes slack time) */
		expected_time=(time_t)(temp_host->next_check+temp_host->latency+host_check_timeout+check_reaper_interval+600);

		/* this host was supposed to have executed a while ago, but for some reason the results haven't come back in... */
		if(expected_time<current_time){

			/* log a warning */
			asprintf(&temp_buffer,"Warning: The check of host '%s' looks like it was orphaned (results never came back).  I'm scheduling an immediate check of the host...\n",temp_host->name);
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
			my_free((void **)&temp_buffer);

			/* decrement the number of running host checks */
			if(currently_running_host_checks>0)
				currently_running_host_checks--;

			/* disable the executing flag */
			temp_host->is_executing=FALSE;

			/* schedule an immediate check of the host */
			schedule_host_check(temp_host,current_time,FALSE);
		        }
			
	        }

#ifdef DEBUG0
	printf("check_for_orphaned_hosts() end\n");
#endif

	return;
        }



/* check freshness of host results */
void check_host_result_freshness(void){
	host *temp_host=NULL;
	time_t current_time=0L;
	time_t expiration_time=0L;
	int freshness_threshold=0;
	char *temp_buffer=NULL;

#ifdef DEBUG0
	printf("check_host_result_freshness() start\n");
#endif

	/* bail out if we're not supposed to be checking freshness */
	if(check_host_freshness==FALSE)
		return;

	/* get the current time */
	time(&current_time);

	/* check all hosts... */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* skip hosts we shouldn't be checking for freshness */
		if(temp_host->check_freshness==FALSE)
			continue;

		/* skip hosts that have both active and passive checks disabled */
		if(temp_host->checks_enabled==FALSE && temp_host->accept_passive_host_checks==FALSE)
			continue;

		/* skip hosts that are currently executing (problems here will be caught by orphaned host check) */
		if(temp_host->is_executing==TRUE)
			continue;

		/* skip hosts that are already being freshened */
		if(temp_host->is_being_freshened==TRUE)
			continue;

		/* see if the time is right... */
		if(check_time_against_period(current_time,temp_host->check_period)==ERROR)
			continue;

		/* use user-supplied freshness threshold or auto-calculate a freshness threshold to use? */
		if(temp_host->freshness_threshold==0)
			freshness_threshold=(temp_host->check_interval*interval_length)+temp_host->latency+15;
		else
			freshness_threshold=temp_host->freshness_threshold;

		/* calculate expiration time */
		/* CHANGED 11/10/05 EG - program start is only used in expiration time calculation if > last check AND active checks are enabled, so active checks can become stale immediately upon program startup */
		if(temp_host->has_been_checked==FALSE || (temp_host->checks_enabled==TRUE && (program_start>temp_host->last_check)))
			expiration_time=(time_t)(program_start+freshness_threshold);
		else
			expiration_time=(time_t)(temp_host->last_check+freshness_threshold);

		/* the results for the last check of this host are stale */
		if(expiration_time<current_time){

			/* log a warning */
			asprintf(&temp_buffer,"Warning: The results of host '%s' are stale by %lu seconds (threshold=%d seconds).  I'm forcing an immediate check of the host.\n",temp_host->name,(current_time-expiration_time),freshness_threshold);
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
			my_free((void **)&temp_buffer);

			/* set the freshen flag */
			temp_host->is_being_freshened=TRUE;

			/* schedule an immediate forced check of the host */
			schedule_host_check(temp_host,current_time,TRUE);
		        }
	        }

#ifdef DEBUG0
	printf("check_host_result_freshness() end\n");
#endif

	return;
        }



/******************************************************************/
/************* NAGIOS 3.X ROUTE/HOST CHECK FUNCTIONS **************/
/******************************************************************/


/*** ON-DEMAND HOST CHECKS USE THIS FUNCTION ***/
/* check to see if we can reach the host */
int perform_on_demand_host_check_3x(host *hst, int *check_result, int check_options, int use_cached_result, unsigned long check_timestamp_horizon){
	int result=OK;

#ifdef DEBUG0
	printf("perform_on_demand_host_check() start\n");
#endif

	/* make sure we have a host */
	if(hst==NULL)
		return ERROR;

#ifdef DEBUG_HOST_CHECKS
	printf("\nON-DEMAND CHECK FOR: %s\n",hst->name);
#endif

	/* check the status of the host */
	result=run_sync_host_check_3x(hst,check_result,check_options,use_cached_result,check_timestamp_horizon);

#ifdef DEBUG0
	printf("perform_on_demand_host_check() end\n");
#endif

	return result;
        }



/* perform a synchronous check of a host */
/* on-demand host checks will use this... */
int run_sync_host_check_3x(host *hst, int *check_result, int check_options, int use_cached_result, unsigned long check_timestamp_horizon){
	int result=OK;
	time_t current_time=0L;
	int host_result=HOST_UP;
	char *old_plugin_output=NULL;
	struct timeval start_time;
	struct timeval end_time;

#ifdef DEBUG0
	printf("run_sync_host_check() start\n");
#endif

	/* make sure we have a host */
	if(hst==NULL)
		return ERROR;

#ifdef DEBUG_HOST_CHECKS
	printf("RUN_SYNC_HOST_CHECK 1: %s\n",hst->name);
#endif

	/* is the host check viable at this time? */
	/* if not, return current state and bail out */
	if(check_host_check_viability_3x(hst,check_options,NULL,NULL)==ERROR){
		if(check_result)
			*check_result=hst->current_state;
#ifdef DEBUG_HOST_CHECKS
		printf("! SYNC CHECK NOT VIABLE: %s\n",hst->name);
#endif
		return OK;
	        }

	/* get the current time */
	time(&current_time);

	/* high resolution start time for event broker */
	gettimeofday(&start_time,NULL);

	/* can we use the last cached host state? */
	if(use_cached_result==TRUE && !(check_options & CHECK_OPTION_FORCE_EXECUTION)){

		/* we can used the cached result, so return it and get out of here... */
		if(hst->has_been_checked==TRUE && ((current_time-hst->last_check) <= check_timestamp_horizon)){
			if(check_result)
				*check_result=hst->current_state;
#ifdef DEBUG_HOST_CHECKS
			printf("* USED CACHED STATE: %s\n",hst->name);
#endif
			return OK;
	                }
	        }

#ifdef DEBUG_HOST_CHECKS
	printf("RUN_SYNC_HOST_CHECK START: %s, OLD STATE=%d\n",hst->name,hst->current_state);
#endif

	/******** GOOD TO GO FOR A REAL HOST CHECK AT THIS POINT ********/

	/* reset host check latency, since on-demand checks have none */
	hst->latency=0.0;

	/* adjust host check attempt */
	adjust_host_check_attempt_3x(hst);

	/* save old host state */
	hst->last_state=hst->current_state;
	if(hst->state_type==HARD_STATE)
		hst->last_hard_state=hst->current_state;

	/* save old plugin output for state stalking */
	if(hst->plugin_output)
		old_plugin_output=(char *)strdup(hst->plugin_output);

	/* set the checked flag */
	hst->has_been_checked=TRUE;

	/* clear the freshness flag */
	hst->is_being_freshened=FALSE;

	/* clear the force execution flag */
	if((check_options & CHECK_OPTION_FORCE_EXECUTION) && (hst->check_options & CHECK_OPTION_FORCE_EXECUTION))
		hst->check_options-=CHECK_OPTION_FORCE_EXECUTION;

	/* set the check type */
	hst->check_type=HOST_CHECK_ACTIVE;


	/*********** EXECUTE THE CHECK AND PROCESS THE RESULTS **********/

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_host_check(NEBTYPE_HOSTCHECK_INITIATE,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,hst->current_state,hst->state_type,start_time,end_time,hst->host_check_command,hst->latency,0.0,host_check_timeout,FALSE,0,NULL,NULL,NULL,NULL,NULL);
#endif

	/* execute the host check */
	host_result=execute_sync_host_check_3x(hst);

	/* process the host check result */
	process_host_check_result_3x(hst,host_result,old_plugin_output,check_options,use_cached_result,check_timestamp_horizon);

	/* free memory */
	my_free((void **)&old_plugin_output);

#ifdef DEBUG_HOST_CHECKS
	printf("RUN_SYNC_HOST_CHECK DONE: %s, NEW STATE=%d\n\n",hst->name,hst->current_state);
#endif

	/* high resolution end time for event broker */
	gettimeofday(&end_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_host_check(NEBTYPE_HOSTCHECK_PROCESSED,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,hst->current_state,hst->state_type,start_time,end_time,hst->host_check_command,hst->latency,hst->execution_time,host_check_timeout,FALSE,hst->current_state,NULL,hst->plugin_output,hst->long_plugin_output,hst->perf_data,NULL);
#endif

#ifdef DEBUG0
	printf("run_sync_host_check() end\n");
#endif

	return result;
        }



/* run an "alive" check on a host */
/* on-demand host checks will use this... */
int execute_sync_host_check_3x(host *hst){
	int result=STATE_OK;
	int return_result=HOST_UP;
	char processed_command[MAX_COMMAND_BUFFER]="";
	char raw_command[MAX_COMMAND_BUFFER]="";
	char *temp_buffer=NULL;
	time_t current_time;
	time_t start_time;
	struct timeval start_time_hires;
	struct timeval end_time_hires;
	char *temp_ptr;
	int early_timeout=FALSE;
	double exectime;
	char *temp_plugin_output=NULL;
		

#ifdef DEBUG0
	printf("execute_sync_host_check() start\n");
#endif

#ifdef DEBUG_HOST_CHECKS
	printf("EXECUTE_SYNC_HOST_CHECK: %s\n",hst->name);
#endif

	/* grab the host macros */
	clear_volatile_macros();
	grab_host_macros(hst);
	grab_summary_macros(NULL);

	/* high resolution start time for event broker */
	gettimeofday(&start_time_hires,NULL);

	/* get the last host check time */
	time(&start_time);
	hst->last_check=start_time;

	/* get the raw command line */
	get_raw_command_line(hst->host_check_command,raw_command,sizeof(raw_command),0);
	strip(raw_command);

	/* process any macros contained in the argument */
	process_macros(raw_command,processed_command,sizeof(processed_command),0);
	strip(processed_command);
			
#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time_hires.tv_sec=0L;
	end_time_hires.tv_usec=0L;
	broker_host_check(NEBTYPE_HOSTCHECK_RAW_START,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,return_result,hst->state_type,start_time_hires,end_time_hires,hst->host_check_command,0.0,0.0,host_check_timeout,early_timeout,result,processed_command,hst->plugin_output,hst->long_plugin_output,hst->perf_data,NULL);
#endif

#ifdef DEBUG3
	printf("\t\tRaw Command: %s\n",raw_command);
	printf("\t\tProcessed Command: %s\n",processed_command);
#endif

	/* clear plugin output and performance data buffers */
	my_free((void **)&hst->plugin_output);
	my_free((void **)&hst->long_plugin_output);
	my_free((void **)&hst->perf_data);

	/* run the host check command */
	result=my_system(processed_command,host_check_timeout,&early_timeout,&exectime,&temp_plugin_output,MAX_PLUGIN_OUTPUT_LENGTH);

	/* if the check timed out, report an error */
	if(early_timeout==TRUE){

		my_free((void **)&temp_plugin_output);
		asprintf(&temp_plugin_output,"Host check timed out after %d seconds\n",host_check_timeout);

		/* log the timeout */
		asprintf(&temp_buffer,"Warning: Host check command '%s' for host '%s' timed out after %d seconds\n",processed_command,hst->name,host_check_timeout);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);
	        }

	/* calculate total execution time */
	hst->execution_time=exectime;

	/* record check type */
	hst->check_type=HOST_CHECK_ACTIVE;

	/* parse the output: short and long output, and perf data */
	parse_check_output(temp_plugin_output,&hst->plugin_output,&hst->long_plugin_output,&hst->perf_data,TRUE,FALSE);

	/* free memory */
	my_free((void **)&temp_plugin_output);

	/* a NULL host check command means we should assume the host is UP */
	if(hst->host_check_command==NULL){
		my_free((void **)&hst->plugin_output);
		hst->plugin_output=(char *)strdup("(Host assumed to be UP)");
		result=STATE_OK;
	        }

	/* make sure we have some data */
	if(hst->plugin_output==NULL || !strcmp(hst->plugin_output,"")){
		my_free((void **)&hst->plugin_output);
		hst->plugin_output=(char *)strdup("(No output returned from host check)");
	        }

	/* replace semicolons in plugin output (but not performance data) with colons */
	if((temp_ptr=hst->plugin_output)){
		while((temp_ptr=strchr(temp_ptr,';')))
			*temp_ptr=':';
	        }

	/* if we're not doing aggressive host checking, let WARNING states indicate the host is up (fake the result to be STATE_OK) */
	if(use_aggressive_host_checking==FALSE && result==STATE_WARNING)
		result=STATE_OK;


	if(result==STATE_OK)
		return_result=HOST_UP;
	else
		return_result=HOST_DOWN;

	/* high resolution end time for event broker */
	gettimeofday(&end_time_hires,NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_host_check(NEBTYPE_HOSTCHECK_RAW_END,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,return_result,hst->state_type,start_time_hires,end_time_hires,hst->host_check_command,0.0,exectime,host_check_timeout,early_timeout,result,processed_command,hst->plugin_output,hst->long_plugin_output,hst->perf_data,NULL);
#endif

#ifdef DEBUG3
	printf("\tHost Check Result: Host '%s' is ",hst->name);
	if(return_result==HOST_UP)
		printf("UP\n");
	else
		printf("DOWN\n");
#endif

#ifdef DEBUG0
	printf("execute_sync_host_check() end\n");
#endif
	
	return return_result;
        }



/* run a scheduled host check asynchronously */
int run_scheduled_host_check_3x(host *hst, int check_options, double latency){
	int result=OK;
	time_t preferred_time=0L;
	time_t next_valid_time=0L;
	int time_is_valid=TRUE;

#ifdef DEBUG_HOST_CHECKS
	printf("RUN_SCHEDULED_HOST_CHECK: %s\n",hst->name);
#endif

	/* attempt to run the check */
	result=run_async_host_check_3x(hst,check_options,latency,TRUE,&time_is_valid,&preferred_time);

	/* an error occurred, so reschedule the check */
	if(result==ERROR){

		/* only attempt to (re)schedule checks that should get checked... */
		if(hst->should_be_scheduled==TRUE){

			/* make sure we rescheduled the next host check at a valid time */
			get_next_valid_time(preferred_time,&next_valid_time,hst->check_period);

			/* the host could not be rescheduled properly - set the next check time for next year, but don't actually reschedule it */
			if(time_is_valid==FALSE && next_valid_time==preferred_time){

				hst->next_check=(time_t)(next_valid_time+(60*60*24*365));
				hst->should_be_scheduled=FALSE;
#ifdef DEBUG1
				printf("Warning: Could not find any valid times to reschedule a check of host '%s'!\n",hst->name);
#endif
		                }

			/* this service could be rescheduled... */
			else{
				hst->next_check=next_valid_time;
				hst->should_be_scheduled=TRUE;
			        }
		        }

		/* update the status log */
		update_host_status(hst,FALSE);

		/* reschedule the next host check - unless we couldn't find a valid next check time */
		if(hst->should_be_scheduled==TRUE)
			schedule_host_check(hst,hst->next_check,FALSE);

		return ERROR;
	        }

	return OK;
        }



/* perform an asynchronous check of a host */
/* scheduled host checks will use this, as will some checks that result from on-demand checks... */
int run_async_host_check_3x(host *hst, int check_options, double latency, int reschedule_check, int *time_is_valid, time_t *preferred_time){
	char raw_command[MAX_COMMAND_BUFFER]="";
	char processed_command[MAX_COMMAND_BUFFER]="";
	char output_buffer[MAX_INPUT_BUFFER]="";
	char *temp_buffer=NULL;
	struct timeval start_time,end_time;
	pid_t pid=0;
	int fork_error=FALSE;
	int wait_result=0;
	FILE *fp=NULL;
	int pclose_result=0;
	mode_t new_umask=077;
	mode_t old_umask;
	char *output_file=NULL;
	double old_latency=0.0;
	int result=OK;

	/* make sure we have a host */
	if(hst==NULL)
		return ERROR;

#ifdef DEBUG_HOST_CHECKS
	printf("RUN_ASYNC_HOST_CHECK 1: %s\n",hst->name);
#endif

	/* is the host check viable at this time? */
	if(check_host_check_viability_3x(hst,check_options,time_is_valid,preferred_time)==ERROR)
		return ERROR;

	/******** GOOD TO GO FOR A REAL HOST CHECK AT THIS POINT ********/

#ifdef DEBUG3
	printf("\tChecking host '%s'...\n",hst->name);
#endif

#ifdef DEBUG_HOST_CHECKS
	printf("RUN_ASYNC_HOST_CHECK 2: %s\n",hst->name);
#endif

	/* clear the force execution flag */
	if((check_options & CHECK_OPTION_FORCE_EXECUTION) && (hst->check_options & CHECK_OPTION_FORCE_EXECUTION))
		hst->check_options-=CHECK_OPTION_FORCE_EXECUTION;

	/* adjust host check attempt */
	adjust_host_check_attempt_3x(hst);

	/* increment number of host checks that are currently running... */
	currently_running_host_checks++;

	/* set the execution flag */
	hst->is_executing=TRUE;

	/* clear the force execution flag */
	if((check_options & CHECK_OPTION_FORCE_EXECUTION) && (hst->check_options & CHECK_OPTION_FORCE_EXECUTION))
		hst->check_options-=CHECK_OPTION_FORCE_EXECUTION;

	/* set latency (temporarily) for macros and event broker */
	old_latency=hst->latency;
	hst->latency=latency;

	/* grab the host macro variables */
	clear_volatile_macros();
	grab_host_macros(hst);
	grab_summary_macros(NULL);

	/* get the raw command line */
	get_raw_command_line(hst->host_check_command,raw_command,sizeof(raw_command),0);
	strip(raw_command);

	/* process any macros contained in the argument */
	process_macros(raw_command,processed_command,sizeof(processed_command),0);
	strip(processed_command);

	/* get the command start time */
	gettimeofday(&start_time,NULL);

	/* open a temp file for storing check output */
	old_umask=umask(new_umask);
	asprintf(&output_file,"%s/nagiosXXXXXX",temp_path);
	check_result_info.output_file_fd=mkstemp(output_file);
#ifdef DEBUG_CHECK_IPC
	printf("OUTPUT FILE: %s\n",output_file);
#endif
	if(check_result_info.output_file_fd>0)
		check_result_info.output_file_fp=fdopen(check_result_info.output_file_fd,"w");
	else{
		check_result_info.output_file_fp=NULL;
		check_result_info.output_file_fd=-1;
	        }
	umask(old_umask);
#ifdef DEBUG_CHECK_IPC
	printf("OUTPUT FILE FD: %d\n",check_result_info.output_file_fd);
#endif

	/* save check info */
	check_result_info.object_check_type=HOST_CHECK;
	check_result_info.host_name=(char *)strdup(hst->name);
	check_result_info.service_description=NULL;
	check_result_info.check_type=HOST_CHECK_ACTIVE;
	check_result_info.output_file=(check_result_info.output_file_fd<0 || output_file==NULL)?NULL:strdup(output_file);
	check_result_info.latency=latency;
	check_result_info.start_time=start_time;
	check_result_info.finish_time=start_time;
	check_result_info.early_timeout=FALSE;
	check_result_info.exited_ok=TRUE;
	check_result_info.return_code=STATE_OK;

	/* free memory */
	my_free((void **)&output_file);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_host_check(NEBTYPE_HOSTCHECK_INITIATE,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,hst->current_state,hst->state_type,start_time,end_time,hst->host_check_command,hst->latency,0.0,host_check_timeout,FALSE,0,processed_command,NULL,NULL,NULL,NULL);
#endif

	/* reset latency (permanent value for this check will get set later) */
	hst->latency=old_latency;

	/* fork a child process */
	pid=fork();

	/* an error occurred while trying to fork */
	if(pid==-1)
		fork_error=TRUE;

	/* if we are in the child process... */
	else if(pid==0){

		/* set environment variables */
		set_all_macro_environment_vars(TRUE);

		/* fork again... */
		pid=fork();

		/* an error occurred while trying to fork again */
		if(pid==-1)
			exit(STATE_UNKNOWN);

		/* the grandchild process should run the service check... */
		if(pid==0){

			/* reset signal handling */
			reset_sighandler();

			/* become the process group leader */
			setpgid(0,0);

			/* close read end of IPC pipe */
			close(ipc_pipe[0]);

			/* catch plugins that don't finish in a timely manner */
			signal(SIGALRM,host_check_sighandler);
			alarm(host_check_timeout);
     
			/* run the plugin check command */
			fp=popen(processed_command,"r");
			if(fp==NULL)
				_exit(STATE_UNKNOWN);

			/* initialize buffer */
			strcpy(output_buffer,"");

			/* write the first line of plugin output to temp file */
			fgets(output_buffer,sizeof(output_buffer)-1,fp);
			if(check_result_info.output_file_fp)
				fputs(output_buffer,check_result_info.output_file_fp);

			/* write additional output to temp file */
			while(fgets(output_buffer,sizeof(output_buffer)-1,fp)){
				if(check_result_info.output_file_fp)
					fputs(output_buffer,check_result_info.output_file_fp);
			        }

			/* close the process */
			pclose_result=pclose(fp);

			/* close the temp file */
			if(check_result_info.output_file_fp)
				fclose(check_result_info.output_file_fp);

			/* reset the alarm */
			alarm(0);

			/* get the check finish time */
			gettimeofday(&end_time,NULL);

			/* record check result info */
			check_result_info.finish_time=end_time;
			check_result_info.early_timeout=FALSE;

			/* test for execution error */
			if(pclose_result==-1){
				pclose_result=STATE_UNKNOWN;
				check_result_info.return_code=STATE_CRITICAL;
				check_result_info.exited_ok=FALSE;
			        }
			else{
				check_result_info.return_code=WEXITSTATUS(pclose_result);
			        }

			/* write check result to message queue */
			write_check_result(&check_result_info);

			/* free check result memory */
			free_check_result(&check_result_info);

			/* close write end of IPC pipe */
			close(ipc_pipe[1]);

			/* return with plugin exit status - not really necessary... */
			_exit(pclose_result);
		        }

		/* close write end of IPC pipe */
		close(ipc_pipe[1]);

		/* unset environment variables */
		set_all_macro_environment_vars(FALSE);

#ifndef USE_MEMORY_PERFORMANCE_TWEAKS
		/* free allocated memory */
		/* this needs to be done last, so we don't free memory for variables before they're used above */
		free_memory();
#endif

		/* parent exits immediately - grandchild process is inherited by the INIT process, so we have no zombie problem... */
		_exit(STATE_OK);
	        }

	/* else the parent should wait for the first child to return... */
	else if(pid>0){

		/* parent should close output file */
		if(check_result_info.output_file_fp)
			fclose(check_result_info.output_file_fp);

		/* should this be done in first child process (after spawning grandchild) as well? */
		/* free memory allocated for IPC functionality */
		free_check_result(&check_result_info);

		/* wait for the first child to return */
		wait_result=waitpid(pid,NULL,0);
	        }

	/* see if we could run the check... */
	if(fork_error==TRUE){

		/* log an error */
		asprintf(&temp_buffer,"Warning: The check of host '%s' could not be performed due to a fork() error.\n",hst->name);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);

		return ERROR;
	        }
	
	return OK;
        }



/* process results of an asynchronous host check */
int handle_async_host_check_result_3x(host *temp_host, check_result *queued_check_result){
	struct timeval tv;
	int result=STATE_OK;
	char *old_plugin_output=NULL;
	char *temp_buffer=NULL;
	char *temp_ptr=NULL;
	struct timeval start_time_hires;
	struct timeval end_time_hires;

	/* make sure we have what we need */
	if(temp_host==NULL || queued_check_result==NULL)
		return ERROR;
	
#ifdef DEBUG_HOST_CHECKS
	printf("\nHANDLE_ASYNC_HOST_CHECK START: %s\n",temp_host->name);
#endif

#ifdef DEBUG_HOST_CHECKS
	printf("\tFound check result for host '%s'\n",temp_host->name);
	printf("\t\tCheck Type:    %s\n",(queued_check_result->check_type==HOST_CHECK_ACTIVE)?"ACTIVE":"PASSIVE");
	printf("\t\tExited OK?:    %s\n",(queued_check_result->exited_ok==TRUE)?"Yes":"No");
	printf("\t\tExec Time:     %.3f\n",temp_host->execution_time);
	printf("\t\tLatency:       %.3f\n",temp_host->latency);
	printf("\t\tReturn Status: %d\n",queued_check_result->return_code);
	printf("\t\tOutput File:   '%s'\n",queued_check_result->output_file);
#endif

	/* decrement the number of host checks still out there... */
	if(queued_check_result->check_type==HOST_CHECK_ACTIVE && currently_running_host_checks>0)
		currently_running_host_checks--;

	/* skip this host check results if its passive and we aren't accepting passive check results */
	if(accept_passive_host_checks==FALSE && queued_check_result->check_type==HOST_CHECK_PASSIVE)
		return ERROR;

	/* was this check passive or active? */
	temp_host->check_type=(queued_check_result->check_type==HOST_CHECK_ACTIVE)?HOST_CHECK_ACTIVE:HOST_CHECK_PASSIVE;

	/* check latency is passed to us for both active and passive checks */
	temp_host->latency=queued_check_result->latency;

	/* update the execution time for this check (millisecond resolution) */
	temp_host->execution_time=(double)((double)(queued_check_result->finish_time.tv_sec-queued_check_result->start_time.tv_sec)+(double)((queued_check_result->finish_time.tv_usec-queued_check_result->start_time.tv_usec)/1000.0)/1000.0);

	/* set the checked flag */
	temp_host->has_been_checked=TRUE;

	/* clear the freshening flag (it would have been set if this host was determined to be stale) */
	temp_host->is_being_freshened=FALSE;

	/* clear the execution flag if this was an active check */
	if(queued_check_result->check_type==HOST_CHECK_ACTIVE)
		temp_host->is_executing=FALSE;

	/* get the last check time */
	temp_host->last_check=queued_check_result->start_time.tv_sec;

	/* was this check passive or active? */
	temp_host->check_type=(queued_check_result->check_type==HOST_CHECK_ACTIVE)?HOST_CHECK_ACTIVE:HOST_CHECK_PASSIVE;

	/* save the old host state */
	temp_host->last_state=temp_host->current_state;
	if(temp_host->state_type==HARD_STATE)
		temp_host->last_hard_state=temp_host->current_state;

	/* save old plugin output */
	if(temp_host->plugin_output)
		old_plugin_output=(char *)strdup(temp_host->plugin_output);

	/* clear the old plugin output and perf data buffers */
	my_free((void **)&temp_host->plugin_output);
	my_free((void **)&temp_host->long_plugin_output);
	my_free((void **)&temp_host->perf_data);

	/* get the unprocessed return code */
	result=queued_check_result->return_code;

	/* if there was some error running the command, just skip it (this shouldn't be happening) */
	if(queued_check_result->exited_ok==FALSE){

		asprintf(&temp_buffer,"Warning:  Check of host '%s' did not exit properly!\n",temp_host->name);
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);

		temp_host->plugin_output=(char *)strdup("(Host check did not exit properly)");

		result=STATE_CRITICAL;
	        }

	/* make sure the return code is within bounds */
	else if(queued_check_result->return_code<0 || queued_check_result->return_code>3){

		asprintf(&temp_buffer,"Warning: Return code of %d for check of host '%s' was out of bounds.%s\n",queued_check_result->return_code,temp_host->name,(queued_check_result->return_code==126 || queued_check_result->return_code==127)?" Make sure the plugin you're trying to run actually exists.":"");
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
		my_free((void **)&temp_buffer);

		asprintf(&temp_host->plugin_output,"(Return code of %d is out of bounds%s)",queued_check_result->return_code,(queued_check_result->return_code==126 || queued_check_result->return_code==127)?" - plugin may be missing":"");

		result=STATE_CRITICAL;
	        }

	/* else the return code is okay... */
	else{

		/* parse check output file to get: (1) short output, (2) long output, (3) perf data */
		if(queued_check_result->output_file){
			read_check_output_from_file(queued_check_result->output_file,&temp_host->plugin_output,&temp_host->long_plugin_output,&temp_host->perf_data,TRUE);
#ifdef DEBUG_CHECK_IPC
			printf("\n");
			printf("PARSED CHECK OUTPUT...\n");
			printf("OUTPUT FILE: %s\n",queued_check_result->output_file);
			printf("SHORT: %s\n",(temp_host->plugin_output==NULL)?"NULL":temp_host->plugin_output);
			printf("LONG: %s\n",(temp_host->long_plugin_output==NULL)?"NULL":temp_host->long_plugin_output);
			printf("PERF: %s\n",(temp_host->perf_data==NULL)?"NULL":temp_host->perf_data);
			printf("\n");
#endif
		        }
	        }

	/* a NULL host check command means we should assume the host is UP */
	if(temp_host->host_check_command==NULL){
		my_free((void **)&temp_host->plugin_output);
		temp_host->plugin_output=(char *)strdup("(Host assumed to be UP)");
		result=STATE_OK;
	        }

	/* make sure we have some data */
	if(temp_host->plugin_output==NULL || !strcmp(temp_host->plugin_output,"")){
		my_free((void **)&temp_host->plugin_output);
		temp_host->plugin_output=(char *)strdup("(No output returned from host check)");
	        }

	/* replace semicolons in plugin output (but not performance data) with colons */
	if((temp_ptr=temp_host->plugin_output)){
		while((temp_ptr=strchr(temp_ptr,';')))
			*temp_ptr=':';
	        }

	/* if we're not doing aggressive host checking, let WARNING states indicate the host is up (fake the result to be STATE_OK) */
	if(use_aggressive_host_checking==FALSE && result==STATE_WARNING)
		result=STATE_OK;

	if(result==STATE_OK)
		result=HOST_UP;
	else
		result=HOST_DOWN;


	/******************* PROCESS THE CHECK RESULTS ******************/

	/* process the host check result */
	process_host_check_result_3x(temp_host,result,old_plugin_output,CHECK_OPTION_NONE,TRUE,cached_host_check_horizon);

	/* free memory */
	my_free((void **)&old_plugin_output);

#ifdef DEBUG_HOST_CHECKS
	printf("HANDLE_ASYNC_HOST_CHECK DONE: %s, NEW STATE=%d\n\n",temp_host->name,temp_host->current_state);
#endif

	/* high resolution start time for event broker */
	start_time_hires=queued_check_result->start_time;

	/* high resolution end time for event broker */
	gettimeofday(&end_time_hires,NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_host_check(NEBTYPE_HOSTCHECK_PROCESSED,NEBFLAG_NONE,NEBATTR_NONE,temp_host,HOST_CHECK_ACTIVE,temp_host->current_state,temp_host->state_type,start_time_hires,end_time_hires,temp_host->host_check_command,temp_host->latency,temp_host->execution_time,host_check_timeout,queued_check_result->early_timeout,queued_check_result->return_code,NULL,temp_host->plugin_output,temp_host->long_plugin_output,temp_host->perf_data,NULL);
#endif

	return OK;
        }



/* processes the result of a synchronous or asynchronous host check */
int process_host_check_result_3x(host *hst, int new_state, char *old_plugin_output, int check_options, int use_cached_result, unsigned long check_timestamp_horizon){
	hostsmember *temp_hostsmember=NULL;
	host *child_host=NULL;
	host *parent_host=NULL;
	host *dependent_host=NULL;
	host *master_host=NULL;
	host *temp_host=NULL;
	hostdependency *temp_dependency=NULL;
	objectlist *check_hostlist=NULL;
	objectlist *hostlist_item=NULL;
	int parent_state=HOST_UP;
	time_t current_time=0L;
	time_t next_check=0L;
	time_t preferred_time=0L;
	time_t next_valid_time=0L;
	int run_async_check=TRUE;


#ifdef DEBUG_HOST_CHECKS
	printf("PROCESS_HOST_CHECK_RESULT START: %s, ATTEMPT=%d/%d, TYPE=%s, OLD STATE=%d, NEW STATE=%d\n",hst->name,hst->current_attempt,hst->max_attempts,(hst->state_type==HARD_STATE)?"HARD":"SOFT",hst->current_state,new_state);
#endif

	/* get the current time */
	time(&current_time);

	/******* HOST IS DOWN/UNREACHABLE INITIALLY *******/
	if(hst->current_state!=HOST_UP){

		/***** HOST IS NOW UP *****/
		/* the host just recovered! */
		if(new_state==HOST_UP){

			/* set the current state */
			hst->current_state=HOST_UP;

			/* set the state type */
			/* set state type to HARD for passive checks and active checks that were previously in a HARD STATE*/
			if(hst->check_type==HOST_CHECK_PASSIVE || hst->state_type==HARD_STATE)
				hst->state_type=HARD_STATE;
			else
				hst->state_type=SOFT_STATE;

			/* reschedule the next check of the host at the normal interval */
			next_check=(unsigned long)(current_time+(hst->check_interval*interval_length));

			/* propagate checks to immediate parents if they are not already UP */
			/* we do this because a parent host (or grandparent) may have recovered somewhere and we should catch the recovery as soon as possible */
			for(temp_hostsmember=hst->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){
				if((parent_host=find_host(temp_hostsmember->host_name))==NULL)
					continue;
				if(parent_host->current_state!=HOST_UP)
					add_object_to_objectlist(&check_hostlist,(void *)parent_host);
			        }

			/* propagate checks to immediate children if they are not already UP */
			/* we do this because children may currently be UNREACHABLE, but may (as a result of this recovery) switch to UP or DOWN states */
			for(child_host=host_list;child_host!=NULL;child_host=child_host->next){
				if(is_host_immediate_child_of_host(hst,child_host)==TRUE && child_host->current_state!=HOST_UP)
					add_object_to_objectlist(&check_hostlist,(void *)child_host);
			        }
		        }

		/***** HOST IS STILL DOWN/UNREACHABLE *****/
		/* we're still in a problem state... */
		else{

			/* active checks are the norm... */
			if(hst->check_type==HOST_CHECK_ACTIVE){

				/* set the state type */
				/* we've maxed out on the retries */
				if(hst->current_attempt==hst->max_attempts)
					hst->state_type=HARD_STATE;
				/* the host was in a hard problem state before, so it still is now */
				else if(hst->current_attempt==1)
					hst->state_type=HARD_STATE;
				/* the host is in a soft state and the check will be retried */
				else
					hst->state_type=SOFT_STATE;

				/* schedule a re-check of the host at the retry interval because we can't determine its final state yet... */
				if(hst->state_type==SOFT_STATE)
					next_check=(unsigned long)(current_time+(hst->retry_interval*interval_length));

				/* host has maxed out on retries (or was previously in a hard problem state), so reschedule the next check at the normal interval */
				else
					next_check=(unsigned long)(current_time+(hst->check_interval*interval_length));

				/* make a determination of the host's state */
				hst->current_state=determine_host_reachability(hst);
			        }

			/* passive checks have some exceptions... */
			else{

				/* set the state type */
				hst->state_type=HARD_STATE;

				/* reset the current attempt */
				hst->current_attempt=1;

				/* schedule another check at the normal interval */
				next_check=(unsigned long)(current_time+(hst->check_interval*interval_length));
			        }

		        }

	        }

	/******* HOST IS UP INITIALLY *******/
	else{

		/***** HOST IS STILL UP *****/
		/* either the host never went down since last check */
		if(new_state==HOST_UP){
			
			/* set the current state */
			hst->current_state=HOST_UP;

			/* set the state type */
			hst->state_type=HARD_STATE;

			/* reschedule the next check at the normal interval */
			next_check=(unsigned long)(current_time+(hst->check_interval*interval_length));
		        }

		/***** HOST IS NOW DOWN/UNREACHABLE *****/
		else{

			/***** SPECIAL CASE FOR HOSTS WITH MAX_ATTEMPTS==1 *****/
			if(hst->max_attempts==1){

				/* set the state type */
				hst->state_type=HARD_STATE;

				/* host has maxed out on retries, so reschedule the next check at the normal interval */
				next_check=(unsigned long)(current_time+(hst->check_interval*interval_length));

				/* we need to run SYNCHRONOUS checks of all parent hosts to accurately determine the state of this host */
				/* this is extremely inefficient (reminiscent of Nagios 2.x logic), but there's no other way around it */
				/* check all parent hosts to see if we're DOWN or UNREACHABLE */
				/* only do this for ACTIVE checks, as PASSIVE checks contain a pre-determined state */
				if(hst->check_type==HOST_CHECK_ACTIVE){

					for(temp_hostsmember=hst->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

						if((parent_host=find_host(temp_hostsmember->host_name))==NULL)
							continue;

						/* run an immediate check of the parent host */
						run_sync_host_check_3x(parent_host,&parent_state,check_options,use_cached_result,check_timestamp_horizon);

						/* bail out as soon as we find one parent host that is UP */
						if(parent_state==HOST_UP){

							/* set the current state */
							hst->current_state=HOST_DOWN;
							break;
					                }
					        }
					if(temp_hostsmember==NULL){
						/* host has no parents, so its up */
						if(hst->parent_hosts==NULL)
							hst->current_state=HOST_DOWN;
						else
							/* no parents were up, so this host is UNREACHABLE */
							hst->current_state=HOST_UNREACHABLE;
				                }
				        }
				
				/* propagate checks to immediate children if they are not UNREACHABLE */
				/* we do this because we may now be blocking the route to child hosts */
				for(child_host=host_list;child_host!=NULL;child_host=child_host->next){
					if(is_host_immediate_child_of_host(hst,child_host)==TRUE && child_host->current_state!=HOST_UNREACHABLE)
						add_object_to_objectlist(&check_hostlist,(void *)child_host);
				        }
			        }

			/***** RETRY THE CHECK *****/
			else{

				/* active checks are the normal case... */
				if(hst->check_type==HOST_CHECK_ACTIVE){

					/* set the state type */
					hst->state_type=SOFT_STATE;

					/* schedule a re-check of the host at the retry interval because we can't determine its final state yet... */
					next_check=(unsigned long)(current_time+(hst->retry_interval*interval_length));

					/* make a preliminary determination of the host's state */
					hst->current_state=determine_host_reachability(hst);
				        }

				/* passive checks have a few differences... */
				else{

					/* set the state type */
					hst->state_type=HARD_STATE;

					/* reset the current attempt */
					hst->current_attempt=1;

					/* schedule a re-check of the host at the normal interval */
					next_check=(unsigned long)(current_time+(hst->check_interval*interval_length));
				        }

				/* propagate checks to immediate parents if they are UP */
				/* we do this because a parent host (or grandparent) may have gone down and blocked our route */
				/* checking the parents ASAP will allow us to better determine the final state (DOWN/UNREACHABLE) of this host later */
				for(temp_hostsmember=hst->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){
					if((parent_host=find_host(temp_hostsmember->host_name))==NULL)
						continue;
					if(parent_host->current_state==HOST_UP)
						add_object_to_objectlist(&check_hostlist,(void *)parent_host);
			                }

				/* propagate checks to immediate children if they are not UNREACHABLE */
				/* we do this because we may now be blocking the route to child hosts */
				for(child_host=host_list;child_host!=NULL;child_host=child_host->next){
					if(is_host_immediate_child_of_host(hst,child_host)==TRUE && child_host->current_state!=HOST_UNREACHABLE)
						add_object_to_objectlist(&check_hostlist,(void *)child_host);
				        }

				/* check dependencies on second to last host check */
				if(enable_predictive_host_dependency_checks==TRUE && hst->current_attempt==(hst->max_attempts-1)){

					/* propagate checks to hosts that THIS ONE depends on for notifications AND execution */
					/* we do to help ensure that the dependency checks are accurate before it comes time to notify */
					for(temp_dependency=hostdependency_list;temp_dependency!=NULL;temp_dependency=temp_dependency->next){
						if(temp_dependency->dependent_host_name==NULL)
							continue;
						if(!strcmp(hst->name,temp_dependency->dependent_host_name)){
							if((master_host=find_host(temp_dependency->host_name))!=NULL)
								add_object_to_objectlist(&check_hostlist,(void *)master_host);
				                        }
					        }
		                        }
			        }
		        }
	        }

#ifdef DEBUG_HOST_CHECKS
	printf("PROCESS_HOST_CHECK_RESULT 2: %s, ATTEMPT=%d/%d, TYPE=%s, FINAL STATE=%d\n",hst->name,hst->current_attempt,hst->max_attempts,(hst->state_type==HARD_STATE)?"HARD":"SOFT",hst->current_state);
#endif
	/* handle the host state */
	handle_host_state(hst);

#ifdef DEBUG_HOST_CHECKS
	printf("PROCESS_HOST_CHECK_RESULT DONE: %s, ATTEMPT=%d/%d, TYPE=%s, FINAL STATE=%d\n",hst->name,hst->current_attempt,hst->max_attempts,(hst->state_type==HARD_STATE)?"HARD":"SOFT",hst->current_state);
#endif

	/******************** POST-PROCESSING STUFF *********************/

	/* if the plugin output differs from previous check and no state change, log the current state/output if state stalking is enabled */
	if(hst->last_state==hst->current_state && compare_strings(old_plugin_output,hst->plugin_output)){

		if(hst->current_state==HOST_UP && hst->stalk_on_up==TRUE)
			log_host_event(hst);

		else if(hst->current_state==HOST_DOWN && hst->stalk_on_down==TRUE)
			log_host_event(hst);

		else if(hst->current_state==HOST_UNREACHABLE && hst->stalk_on_unreachable==TRUE)
			log_host_event(hst);
	        }

	/* check to see if the associated host is flapping */
	check_for_host_flapping(hst,TRUE,TRUE);

	/* reschedule the next check of the host ONLY for active checks */
	if(hst->check_type==HOST_CHECK_ACTIVE){

		/* default is to reschedule host check unless a test below fails... */
		hst->should_be_scheduled=TRUE;

		/* get the new current time */
		time(&current_time);

		/* make sure we don't get ourselves into too much trouble... */
		if(current_time>next_check)
			hst->next_check=current_time;
		else
			hst->next_check=next_check;

		/* make sure we rescheduled the next service check at a valid time */
		preferred_time=hst->next_check;
		get_next_valid_time(preferred_time,&next_valid_time,hst->check_period);
		hst->next_check=next_valid_time;

		/* hosts with non-recurring intervals do not get rescheduled if we're in a HARD or UP state */
		if(hst->check_interval==0 && (hst->state_type==HARD_STATE || hst->current_state==HOST_UP))
			hst->should_be_scheduled=FALSE;

		/* host with active checks disabled do not get rescheduled */
		if(hst->checks_enabled==FALSE)
			hst->should_be_scheduled=FALSE;

		/* schedule a non-forced check if we can */
		if(hst->should_be_scheduled==TRUE){
#ifdef DEBUG_HOST_CHECKS
			printf("SCHEDULED HOST: %s, CURRENT=%lu, NEXT=%lu\n",hst->name,current_time,hst->next_check);
#endif
			schedule_host_check(hst,hst->next_check,FALSE);
		        }

		/* update host status */
		update_host_status(hst,FALSE);
	        }

	/* run async checks of all hosts we added above */
	/* don't run a check if one is already executing or we can get by with a cached state */
	for(hostlist_item=check_hostlist;hostlist_item!=NULL;hostlist_item=hostlist_item->next){
		run_async_check=TRUE;
		temp_host=(host *)hostlist_item->object_ptr;
		if(use_cached_result==TRUE && ((current_time-temp_host->last_check)<=check_timestamp_horizon))
			run_async_check=FALSE;
		if(temp_host->is_executing==TRUE)
			run_async_check=FALSE;
		if(run_async_check==TRUE)
			run_async_host_check_3x(temp_host,CHECK_OPTION_NONE,0.0,FALSE,NULL,NULL);
	        }
	free_objectlist(&check_hostlist);

	return OK;
        }



/* checks viability of performing a host check */
int check_host_check_viability_3x(host *hst, int check_options, int *time_is_valid, time_t *new_time){
	int result=OK;
	int perform_check=TRUE;
	time_t current_time=0L;
	time_t preferred_time=0L;
	int check_interval=0;

	/* make sure we have a host */
	if(hst==NULL)
		return ERROR;

	/* get the check interval to use if we need to reschedule the check */
	if(hst->state_type==SOFT_STATE && hst->current_state!=HOST_UP)
		check_interval=(hst->retry_interval*interval_length);
	else
		check_interval=(hst->check_interval*interval_length);

	/* get the current time */
	time(&current_time);

	/* initialize the next preferred check time */
	preferred_time=current_time;

	/* can we check the host right now? */
	if(!(check_options & CHECK_OPTION_FORCE_EXECUTION)){

		/* if checks of the host are currently disabled... */
		if(hst->checks_enabled==FALSE){
			preferred_time=current_time+check_interval;
			perform_check=FALSE;
		        }

		/* make sure this is a valid time to check the host */
		if(check_time_against_period((unsigned long)current_time,hst->check_period)==ERROR){
			preferred_time=current_time;
			if(time_is_valid)
				*time_is_valid=FALSE;
			perform_check=FALSE;
		        }

		/* check host dependencies for execution */
		if(check_host_dependencies(hst,EXECUTION_DEPENDENCY)==DEPENDENCIES_FAILED){
			preferred_time=current_time+check_interval;
			perform_check=FALSE;
		        }
	        }

	/* pass back the next viable check time */
	if(new_time)
		*new_time=preferred_time;

	result=(perform_check==TRUE)?OK:ERROR;

	return result;
        }



/* adjusts current host check attempt before a new check is performed */
int adjust_host_check_attempt_3x(host *hst){

	/* if host is in a hard state, reset current attempt number */
	if(hst->state_type==HARD_STATE)
		hst->current_attempt=1;

	/* if host is in a soft UP state, reset current attempt number */
	else if(hst->state_type==SOFT_STATE && hst->current_state==HOST_UP)
		hst->current_attempt=1;

	/* increment current attempt number */
	else if(hst->current_attempt < hst->max_attempts)
		hst->current_attempt++;

	return OK;
        }



/* determination of the host's state based on route availability*/
/* used only to determine difference between DOWN and UNREACHABLE states */
int determine_host_reachability(host *hst){
	int state=HOST_DOWN;
	host *parent_host=NULL;
	hostsmember *temp_hostsmember=NULL;

	/* host has no parents, so it is DOWN */
	if(hst->parent_hosts==NULL)
		state=HOST_DOWN;

	/* check all parent hosts to see if we're DOWN or UNREACHABLE */
	else{

		for(temp_hostsmember=hst->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

			if((parent_host=find_host(temp_hostsmember->host_name))==NULL)
				continue;

			/* bail out as soon as we find one parent host that is UP */
			if(parent_host->current_state==HOST_UP){
				/* set the current state */
				state=HOST_DOWN;
				break;
		                }
	                }
		/* no parents were up, so this host is UNREACHABLE */
		if(temp_hostsmember==NULL)
			state=HOST_UNREACHABLE;
	        }

	return state;
        }
