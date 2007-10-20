/*****************************************************************************
 *
 * CHECKS.C - Service and host check functions for Nagios
 *
 * Copyright (c) 1999-2007 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   10-19-2007
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

#ifdef EMBEDDEDPERL
#include "../include/epn_nagios.h"
#endif

extern int      sigshutdown;
extern int      sigrestart;

extern char     *temp_file;

extern int      interval_length;

extern int      command_check_interval;

extern int      ipc_pipe[2];

extern int      log_initial_states;

extern int      service_check_timeout;
extern int      host_check_timeout;

extern int      service_check_reaper_interval;
extern int      max_check_reaper_time;

extern int      use_aggressive_host_checking;

extern int      soft_state_dependencies;

extern int      currently_running_service_checks;
extern int      non_parallelized_check_running;

extern int      accept_passive_service_checks;
extern int      execute_service_checks;
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

extern service_message svc_msg;

extern pthread_t       worker_threads[TOTAL_WORKER_THREADS];
extern circular_buffer service_result_buffer;

#ifdef EMBEDDEDPERL
extern int      use_embedded_perl;
#endif


/******************************************************************/
/****************** SERVICE MONITORING FUNCTIONS ******************/
/******************************************************************/

/* forks a child process to run a service check, but does not wait for the service check result */
void run_service_check(service *svc){
	char raw_command[MAX_COMMAND_BUFFER];
	char processed_command[MAX_COMMAND_BUFFER];
	char plugin_output[MAX_PLUGINOUTPUT_LENGTH];
	char temp_buffer[MAX_INPUT_BUFFER];
	int check_service=TRUE;
	struct timeval start_time,end_time;
	time_t current_time;
	time_t preferred_time=0L;
	time_t next_valid_time;
	pid_t pid;
	int fork_error=FALSE;
	int wait_result=0;
	host *temp_host=NULL;
	FILE *fp;
	int pclose_result=0;
	int time_is_valid=TRUE;
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


	/* get the current time */
	time(&current_time);

	/* if the service check is currently disabled... */
	if(svc->checks_enabled==FALSE){

		/* don't check the service if we're not forcing it through */
		if(!(svc->check_options & CHECK_OPTION_FORCE_EXECUTION))
			check_service=FALSE;

		/* reschedule the service check */
		preferred_time=current_time+(svc->check_interval*interval_length);
                }

	/* if the service check should not be checked on a regular interval... */
	if(svc->check_interval==0){

		/* don't check the service if we're not forcing it through */
		if(!(svc->check_options & CHECK_OPTION_FORCE_EXECUTION))
			check_service=FALSE;

		/* don't reschedule the service check */
		svc->should_be_scheduled=FALSE;
                }

	/* make sure this is a valid time to check the service */
	if(check_time_against_period((unsigned long)current_time,svc->check_period)==ERROR){

		/* don't check the service if we're not forcing it through */
		if(!(svc->check_options & CHECK_OPTION_FORCE_EXECUTION))
			check_service=FALSE;

		/* get the next valid time we can run the check */
		preferred_time=current_time;

		/* set the invalid time flag */
		time_is_valid=FALSE;
	        }

	/* check service dependencies for execution */
	if(check_service_dependencies(svc,EXECUTION_DEPENDENCY)==DEPENDENCIES_FAILED){

		/* don't check the service if we're not forcing it through */
		if(!(svc->check_options & CHECK_OPTION_FORCE_EXECUTION))
			check_service=FALSE;

		/* reschedule the service check */
		preferred_time=current_time+(svc->check_interval*interval_length);
	        }

	/* clear the force execution flag */
	if(svc->check_options & CHECK_OPTION_FORCE_EXECUTION)
		svc->check_options-=CHECK_OPTION_FORCE_EXECUTION;

	/* find the host associated with this service */
	temp_host=find_host(svc->host_name);

	/* don't check the service if we couldn't find the associated host */
	if(temp_host==NULL)
		check_service=FALSE;

	/* if we shouldn't check the service, just reschedule it and leave... */
	if(check_service==FALSE){

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

		/* update the status log with the current service */
		update_service_status(svc,FALSE);

		/* reschedule the next service check - unless we couldn't find a valid next check time */
		if(svc->should_be_scheduled==TRUE)
			schedule_service_check(svc,svc->next_check,FALSE);

		return;
	        }


	/**** ELSE RUN THE SERVICE CHECK ****/

#ifdef DEBUG3
	printf("\tChecking service '%s' on host '%s'...\n",svc->description,svc->host_name);
#endif

	/* increment number of parallel service checks currently out there... */
	currently_running_service_checks++;

	/* set a flag if this service check shouldn't be parallelized with others... */
	if(svc->parallelize==FALSE)
		non_parallelized_check_running=TRUE;

	/* set the execution flag */
	svc->is_executing=TRUE;

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

	/* save service info */
	strncpy(svc_msg.host_name,svc->host_name,sizeof(svc_msg.host_name)-1);
	svc_msg.host_name[sizeof(svc_msg.host_name)-1]='\x0';
	strncpy(svc_msg.description,svc->description,sizeof(svc_msg.description)-1);
	svc_msg.description[sizeof(svc_msg.description)-1]='\x0';
	svc_msg.parallelized=svc->parallelize;
	svc_msg.start_time=start_time;
	svc_msg.finish_time=start_time;
	svc_msg.early_timeout=FALSE;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_service_check(NEBTYPE_SERVICECHECK_INITIATE,NEBFLAG_NONE,NEBATTR_NONE,svc,SERVICE_CHECK_ACTIVE,start_time,end_time,svc->service_check_command,svc->latency,0.0,service_check_timeout,FALSE,0,processed_command,NULL);
#endif

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
			strip(perl_plugin_output);

#ifdef DEBUG1
			printf("embedded perl failed to compile %s, compile error %s - skipping plugin\n",fname,perl_plugin_output);
#endif

			/* get the check finish time */
			gettimeofday(&end_time,NULL);

			/* record check result info */
			strncpy(svc_msg.output,perl_plugin_output,sizeof(svc_msg.output)-1);
			svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
			svc_msg.return_code=pclose_result;
			svc_msg.exited_ok=TRUE;
			svc_msg.check_type=SERVICE_CHECK_ACTIVE;
			svc_msg.finish_time=end_time;
			svc_msg.early_timeout=FALSE;

			/* write check results to message queue */
			write_svc_message(&svc_msg);

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

#ifndef USE_MEMORY_PERFORMANCE_TWEAKS
		/* free allocated memory */
		free_memory();
#endif

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
				strip(perl_plugin_output);
				strncpy(plugin_output, perl_plugin_output, sizeof(plugin_output));
				perl_plugin_output[sizeof(perl_plugin_output)-1]='\x0';
				pclose_result=POPi;

				PUTBACK;
				FREETMPS;
				LEAVE;

#ifdef DEBUG1
				printf("embedded perl ran %s, plugin output was %d, %s\n",fname, pclose_result, plugin_output);
#endif

				/* reset the alarm */
				alarm(0);

				/* get the check finish time */
				gettimeofday(&end_time,NULL);

				/* record check result info */
				strncpy(svc_msg.output,plugin_output,sizeof(svc_msg.output)-1);
				svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
				svc_msg.return_code=pclose_result;
				svc_msg.exited_ok=TRUE;
				svc_msg.check_type=SERVICE_CHECK_ACTIVE;
				svc_msg.finish_time=end_time;
				svc_msg.early_timeout=FALSE;

				/* write check results to message queue */
				write_svc_message(&svc_msg);

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

			/* default return string in case nothing was returned */
			strcpy(plugin_output,"(No output!)");

			/* grab the plugin output and clean it */
			fgets(plugin_output,sizeof(plugin_output)-1,fp);
			strip(plugin_output);

			/* ADDED 01/04/2004 */
			/* ignore any additional lines of output */
			while(fgets(temp_buffer,sizeof(temp_buffer)-1,fp));

			/* close the process */
			pclose_result=pclose(fp);

			/* reset the alarm */
			alarm(0);

			/* get the check finish time */
			gettimeofday(&end_time,NULL);

			/* record check result info */
			strncpy(svc_msg.output,plugin_output,sizeof(svc_msg.output)-1);
			svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
			svc_msg.return_code=WEXITSTATUS(pclose_result);
			svc_msg.exited_ok=TRUE;
			svc_msg.check_type=SERVICE_CHECK_ACTIVE;
			svc_msg.finish_time=end_time;
			svc_msg.early_timeout=FALSE;

			/* test for execution error */
			if(pclose_result==-1){
				pclose_result=STATE_UNKNOWN;
				strncpy(svc_msg.output,"(Error returned by call to pclose() function)",sizeof(svc_msg.output)-1);
				svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
				svc_msg.return_code=STATE_CRITICAL;
				svc_msg.exited_ok=FALSE;
				svc_msg.early_timeout=FALSE;
			        }
			else if(WEXITSTATUS(pclose_result)==0 && WIFSIGNALED(pclose_result)){
				snprintf(svc_msg.output,sizeof(svc_msg.output)-1,"(Plugin received signal %d!)\n",WTERMSIG(pclose_result));
				svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
				svc_msg.exited_ok=TRUE;
				svc_msg.early_timeout=FALSE;
				/* like bash */
				pclose_result=128+WTERMSIG(pclose_result);
				svc_msg.return_code=pclose_result;
				}

			/* write check result to message queue */
			write_svc_message(&svc_msg);

			/* close write end of IPC pipe */
			close(ipc_pipe[1]);

			/* return with plugin exit status - not really necessary... */
			_exit(pclose_result);
		        }

		/* close write end of IPC pipe */
		close(ipc_pipe[1]);

		/* unset environment variables */
		set_all_macro_environment_vars(FALSE);

		/* parent exits immediately - grandchild process is inherited by the INIT process, so we have no zombie problem... */
		_exit(STATE_OK);
	        }

	/* else the parent should wait for the first child to return... */
	else if(pid>0){

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
		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: The check of service '%s' on host '%s' could not be performed due to a fork() error.  The check will be rescheduled.\n",svc_msg.description,svc_msg.host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);

		/* make sure we rescheduled the next service check at a valid time */
		preferred_time=current_time;
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

			/* only reschedule checks with recurring intervals */
			if(svc->check_interval==0)
				svc->should_be_scheduled=FALSE;
			else
				svc->should_be_scheduled=TRUE;
		        }

		/* update the status log with the current service */
		update_service_status(svc,FALSE);

		/* reschedule the next service check - unless we couldn't find a valid next check time */
		if(svc->should_be_scheduled==TRUE)
			schedule_service_check(svc,svc->next_check,FALSE);
	        }
	
	return;
        }



/* reaps service check results */
void reap_service_checks(void){
	service_message queued_svc_msg;
	service *temp_service=NULL;
	host *temp_host=NULL;
	time_t preferred_time;
	time_t next_valid_time;
	char temp_buffer[MAX_INPUT_BUFFER];
	int state_change=FALSE;
	int hard_state_change=FALSE;
	int route_result=HOST_UP;
	int dependency_result=DEPENDENCIES_OK;
	time_t current_time;
	int first_check=FALSE;
	int state_was_logged=FALSE;
	char old_plugin_output[MAX_PLUGINOUTPUT_LENGTH]="";
	char temp_plugin_output[MAX_PLUGINOUTPUT_LENGTH]="";
	char *temp_ptr;
	time_t reaper_start_time;
	struct timeval tv;

#ifdef DEBUG0
        printf("reap_service_checks() start\n");
#endif

#ifdef DEBUG3
	printf("Starting to reap service check results...\n");
#endif

	time(&reaper_start_time);

	/* read all service checks results that have come in... */
	while(read_svc_message(&queued_svc_msg)!=-1){

		/* make sure we really have something... */
		if(!strcmp(queued_svc_msg.description,"") && !strcmp(queued_svc_msg.host_name,"")){
#ifdef DEBUG1
			printf("Found an empty message in service result pipe!\n");
#endif
			continue;
		        }

		/* get the current time */
		time(&current_time);

		/* skip this service check results if its passive and we aren't accepting passive check results */
		if(accept_passive_service_checks==FALSE && queued_svc_msg.check_type==SERVICE_CHECK_PASSIVE)
			continue;

		/* because of my idiotic idea of having UNKNOWN states be equivalent to -1, I must hack things a bit... */
		if(queued_svc_msg.return_code==255 || queued_svc_msg.return_code==-1)
			queued_svc_msg.return_code=STATE_UNKNOWN;

		/* find the service */
		temp_service=find_service(queued_svc_msg.host_name,queued_svc_msg.description);
		if(temp_service==NULL){

			snprintf(temp_buffer,sizeof(temp_buffer),"Warning:  Message queue contained results for service '%s' on host '%s'.  The service could not be found!\n",queued_svc_msg.description,queued_svc_msg.host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);

			continue;
		        }

		/* calculate passive check latency */
		if(queued_svc_msg.check_type==SERVICE_CHECK_PASSIVE){
			gettimeofday(&tv,NULL);
			temp_service->latency=(double)((double)(tv.tv_sec-queued_svc_msg.finish_time.tv_sec)+(double)((tv.tv_usec-queued_svc_msg.finish_time.tv_usec)/1000.0/1000.0));
			if(temp_service->latency<0.0)
				temp_service->latency=0.0;
		        }

		/* update the execution time for this check (millisecond resolution) */
		temp_service->execution_time=(double)((double)(queued_svc_msg.finish_time.tv_sec-queued_svc_msg.start_time.tv_sec)+(double)((queued_svc_msg.finish_time.tv_usec-queued_svc_msg.start_time.tv_usec)/1000)/1000.0);
#ifdef REMOVED_050803
		if(queued_svc_msg.start_time.time>current_time || queued_svc_msg.finish_time.time>current_time || (queued_svc_msg.finish_time.time<queued_svc_msg.start_time.time))
			temp_service->execution_time=0.0;
		else
			temp_service->execution_time=(double)((double)(queued_svc_msg.finish_time.time-queued_svc_msg.start_time.time)+(double)((queued_svc_msg.finish_time.millitm-queued_svc_msg.start_time.millitm)/1000.0));
#endif

		/* clear the freshening flag (it would have been set if this service was determined to be stale) */
		temp_service->is_being_freshened=FALSE;

		/* ignore passive service check results if we're not accepting them for this service */
		if(temp_service->accept_passive_service_checks==FALSE && queued_svc_msg.check_type==SERVICE_CHECK_PASSIVE)
			continue;

#ifdef DEBUG3
		printf("\n\tFound check result for service '%s' on host '%s'\n",temp_service->description,temp_service->host_name);
		printf("\t\tCheck Type:    %s\n",(queued_svc_msg.check_type==SERVICE_CHECK_ACTIVE)?"ACTIVE":"PASSIVE");
		printf("\t\tParallelized?: %s\n",(queued_svc_msg.parallelized==TRUE)?"Yes":"No");
		printf("\t\tExited OK?:    %s\n",(queued_svc_msg.exited_ok==TRUE)?"Yes":"No");
		printf("\t\tReturn Status: %d\n",queued_svc_msg.return_code);
		printf("\t\tPlugin Output: '%s'\n",queued_svc_msg.output);
#endif

		/* decrement the number of service checks still out there... */
		if(queued_svc_msg.check_type==SERVICE_CHECK_ACTIVE && currently_running_service_checks>0)
			currently_running_service_checks--;

		/* if this check was not parallelized, clear the flag */
		if(queued_svc_msg.parallelized==FALSE && queued_svc_msg.check_type==SERVICE_CHECK_ACTIVE)
			non_parallelized_check_running=FALSE;

		/* clear the execution flag if this was an active check */
		if(queued_svc_msg.check_type==SERVICE_CHECK_ACTIVE)
			temp_service->is_executing=FALSE;

		/* get the last check time */
		temp_service->last_check=queued_svc_msg.start_time.tv_sec;
#ifdef REMOVED_050803
		temp_service->last_check=queued_svc_msg.start_time.time;
#endif

		/* was this check passive or active? */
		temp_service->check_type=(queued_svc_msg.check_type==SERVICE_CHECK_ACTIVE)?SERVICE_CHECK_ACTIVE:SERVICE_CHECK_PASSIVE;

		/* INITIALIZE VARIABLES FOR THIS SERVICE */
		state_change=FALSE;
		hard_state_change=FALSE;
		route_result=HOST_UP;
		dependency_result=DEPENDENCIES_OK;
		first_check=FALSE;
		state_was_logged=FALSE;
		strcpy(old_plugin_output,"");
		strcpy(temp_plugin_output,"");

		/* save the old service status info */
		temp_service->last_state=temp_service->current_state;

		/* save old plugin output */
		strncpy(old_plugin_output,(temp_service->plugin_output==NULL)?"":temp_service->plugin_output,sizeof(old_plugin_output)-1);
		old_plugin_output[sizeof(old_plugin_output)-1]='\x0';

		/* clear the old plugin output and perf data buffers */
		strcpy(temp_service->plugin_output,"");
		strcpy(temp_service->perf_data,"");

		/* check for empty plugin output */
		if(!strcmp(temp_plugin_output,""))
			strcpy(temp_plugin_output,"(No output returned from plugin)");

		/* get performance data (if it exists) */
		strncpy(temp_plugin_output,queued_svc_msg.output,sizeof(temp_plugin_output)-1);
		temp_plugin_output[sizeof(temp_plugin_output)-1]='\x0';
		temp_ptr=strtok(temp_plugin_output,"|\n");
		temp_ptr=strtok(NULL,"\n");
		if(temp_ptr!=NULL){
			strip(temp_ptr);
			strncpy(temp_service->perf_data,temp_ptr,MAX_PLUGINOUTPUT_LENGTH-1);
			temp_service->perf_data[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
		        }

		/* get status data - everything before pipe sign */
		strncpy(temp_plugin_output,queued_svc_msg.output,sizeof(temp_plugin_output)-1);
		temp_plugin_output[sizeof(temp_plugin_output)-1]='\x0';
		temp_ptr=strtok(temp_plugin_output,"|\n");

		/* if there was some error running the command, just skip it (this shouldn't be happening) */
		if(queued_svc_msg.exited_ok==FALSE){

			snprintf(temp_buffer,sizeof(temp_buffer),"Warning:  Check of service '%s' on host '%s' did not exit properly!\n",temp_service->description,temp_service->host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);

			snprintf(temp_service->plugin_output,MAX_PLUGINOUTPUT_LENGTH-1,"(Service check did not exit properly)");
			temp_service->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';

			temp_service->current_state=STATE_CRITICAL;
                        }

		/* make sure the return code is within bounds */
		else if(queued_svc_msg.return_code<0 || queued_svc_msg.return_code>3){

			snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Return code of %d for check of service '%s' on host '%s' was out of bounds.%s\n",queued_svc_msg.return_code,temp_service->description,temp_service->host_name,(queued_svc_msg.return_code==126 || queued_svc_msg.return_code==127)?" Make sure the plugin you're trying to run actually exists.":"");
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);

			snprintf(temp_service->plugin_output,MAX_PLUGINOUTPUT_LENGTH-1,"(Return code of %d is out of bounds%s)",queued_svc_msg.return_code,(queued_svc_msg.return_code==126 || queued_svc_msg.return_code==127)?" - plugin may be missing":"");
			temp_service->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';

			temp_service->current_state=STATE_CRITICAL;
                        }

		/* else the return code is okay... */
		else{

			/* make sure the plugin output isn't null */
			if(temp_ptr==NULL){
				strncpy(temp_service->plugin_output,"(No output returned from plugin)",MAX_PLUGINOUTPUT_LENGTH-1);
				temp_service->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
			        }

			/* grab the plugin output */
			else{

				strip(temp_ptr);
				if(!strcmp(temp_ptr,""))
					strncpy(temp_service->plugin_output,"(No output returned from plugin)",MAX_PLUGINOUTPUT_LENGTH-1);
				else
					strncpy(temp_service->plugin_output,temp_ptr,MAX_PLUGINOUTPUT_LENGTH-1);
				temp_service->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
			        }

			/* replace semicolons in plugin output (but not performance data) with colons */
			temp_ptr=temp_service->plugin_output;
			while((temp_ptr=strchr(temp_ptr,';')))
				*temp_ptr=':';
			
			/* grab the return code */
			temp_service->current_state=queued_svc_msg.return_code;
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
			if(temp_host->has_been_checked==FALSE){

				/* verify the host status */
				verify_route_to_host(temp_host,CHECK_OPTION_NONE);

#ifdef REMOVED_080303
				/* really check the host status if we're using aggressive host checking */
				if(use_aggressive_host_checking==TRUE)
					verify_route_to_host(temp_host,CHECK_OPTION_NONE);

				/* else let's just assume the host is up... */
				else{

					/* set the checked flag */
					temp_host->has_been_checked=TRUE;
				
					/* update the last check time */
					temp_host->last_check=temp_service->last_check;

					/* set the host state and check types */
					temp_host->current_state=HOST_UP;
					temp_host->current_attempt=1;
					temp_host->state_type=HARD_STATE;
					temp_host->check_type=HOST_CHECK_ACTIVE;
					
					/* plugin output should reflect our guess at the current host state */
					strncpy(temp_host->plugin_output,"(Host assumed to be up)",MAX_PLUGINOUTPUT_LENGTH);
					temp_host->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';

					/* should we be calling handle_host_state() here?  probably not, but i'm not sure at the present time - 02/18/03 */

					/* update the status log with the host status */
					update_host_status(temp_host,FALSE);

#ifdef REMOVED_042903
					/* log the initial state if the user wants */
					if(log_initial_states==TRUE)
						log_host_event(temp_host);
#endif
				        }
#endif
		                }

#ifdef REMOVED_042903
			/* log the initial state if the user wants */
			if(temp_service->has_been_checked==FALSE && log_initial_states==TRUE){
				log_service_event(temp_service);
				state_was_logged=TRUE;
			        }
#endif
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
#ifdef DEBUG_CHECKS
			printf("\tSTATE CHANGE\n");
#endif
		        }

		/* checks for a hard state change where host was down at last service check */
		/* this occurs in the case where host goes down and service current attempt gets reset to 1 */
		/* if this check is not made, the service recovery looks like a soft recovery instead of a hard one */
		if(temp_service->host_problem_at_last_check==TRUE && temp_service->current_state==STATE_OK){
#ifdef DEBUG3
			printf("\t\tService '%s' on host '%s' has had a HARD STATE CHANGE!!\n",temp_service->description,temp_service->host_name);
#endif
			hard_state_change=TRUE;
#ifdef DEBUG_CHECKS
			printf("\tHARD STATE CHANGE A\n");
#endif
	                }

		/* check for a "normal" hard state change where max check attempts is reached */
		if(temp_service->current_attempt>=temp_service->max_attempts && temp_service->current_state!=temp_service->last_hard_state){
#ifdef DEBUG3
			printf("\t\tService '%s' on host '%s' has had a HARD STATE CHANGE!!\n",temp_service->description,temp_service->host_name);
#endif
			hard_state_change=TRUE;
#ifdef DEBUG_CHECKS
			printf("\tHARD STATE CHANGE B\n");
#endif
		        }

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



		/**************************************/
		/******* SERVICE CHECK OK LOGIC *******/
		/**************************************/

		/* if the service is up and running OK... */
		if(temp_service->current_state==STATE_OK){

			/* reset the acknowledgement flag (this should already have been done, but just in case...) */
			temp_service->problem_has_been_acknowledged=FALSE;
			temp_service->acknowledgement_type=ACKNOWLEDGEMENT_NONE;

#ifdef DEBUG_CHECKS
			printf("\tOriginally OK\n");
#endif

			/* the service check was okay, so the associated host must be up... */
			if(temp_host->current_state!=HOST_UP){

#ifdef DEBUG_CHECKS
				printf("\tSECTION A1\n");
#endif

				/* verify the route to the host and send out host recovery notifications */
				verify_route_to_host(temp_host,CHECK_OPTION_NONE);

#ifdef REMOVED_041403
				/* set the host problem flag (i.e. don't notify about recoveries for this service) */
				temp_service->host_problem_at_last_check=TRUE;
#endif
			        }

			/* if a hard service recovery has occurred... */
			if(hard_state_change==TRUE){

#ifdef DEBUG_CHECKS
				printf("\tSECTION A2\n");
#endif

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

#ifdef DEBUG_CHECKS
				printf("\tSECTION A3\n");
#endif

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
#ifdef REMOVED_041403
			temp_service->no_recovery_notification=FALSE;
#endif
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

#ifdef DEBUG_CHECKS
			printf("\tOriginally PROBLEM\n");
#endif

#ifdef REMOVED_041403
			/* reset the recovery notification flag (it may get set again though) */
			temp_service->no_recovery_notification=FALSE;
#endif
			
			/* check the route to the host if its supposed to be up right now... */
			if(temp_host->current_state==HOST_UP){
				route_result=verify_route_to_host(temp_host,CHECK_OPTION_NONE);
#ifdef DEBUG_CHECKS
				printf("\tSECTION B1\n");
#endif
			        }

			/* else the host is either down or unreachable, so recheck it if necessary */
			else{

#ifdef DEBUG_CHECKS
				printf("\tSECTION B2a\n");
#endif

				/* we're using aggressive host checking, so really do recheck the host... */
				if(use_aggressive_host_checking==TRUE){
					route_result=verify_route_to_host(temp_host,CHECK_OPTION_NONE);
#ifdef DEBUG_CHECKS
					printf("\tSECTION B2b\n");
#endif
				        }

				/* the service wobbled between non-OK states, so check the host... */
				else if(state_change==TRUE && temp_service->last_hard_state!=STATE_OK){
					route_result=verify_route_to_host(temp_host,CHECK_OPTION_NONE);
#ifdef DEBUG_CHECKS
					printf("\tSECTION B2c\n");
#endif
				        }

				/* else fake the host check, but (possibly) resend host notifications to contacts... */
				else{

#ifdef DEBUG_CHECKS
					printf("\tSECTION B2d\n");
#endif

#ifdef REMOVED_042903
					/* log the initial state if the user wants to and this host hasn't been checked yet */
					if(log_initial_states==TRUE && temp_host->has_been_checked==FALSE)
						log_host_event(temp_host);
#endif

					/* if the host has never been checked before, set the checked flag */
					if(temp_host->has_been_checked==FALSE)
						temp_host->has_been_checked=TRUE;

					/* update the last host check time */
					temp_host->last_check=temp_service->last_check;

					/* fake the route check result */
					route_result=temp_host->current_state;

				        /* possibly re-send host notifications... */
					host_notification(temp_host,NOTIFICATION_NORMAL,NULL,NULL);
				        }
			        }

			/* if the host is down or unreachable ... */
			if(route_result!=HOST_UP){

#ifdef DEBUG_CHECKS
				printf("\tSECTION B3\n");
#endif

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

#ifdef DEBUG_CHECKS
				printf("\tSECTION B4\n");
#endif

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

#ifdef REMOVED_041403
				/* don't send a recovery notification if the service recovers at the next check */
				temp_service->no_recovery_notification=TRUE;
#endif
			        }

			/* if we should retry the service check, do so (except it the host is down or unreachable!) */
			if(temp_service->current_attempt < temp_service->max_attempts){

#ifdef DEBUG_CHECKS
				printf("\tSECTION B5a\n");
#endif

				/* the host is down or unreachable, so don't attempt to retry the service check */
				if(route_result!=HOST_UP){

#ifdef DEBUG_CHECKS
					printf("\tSECTION B5b\n");
#endif

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

#ifdef DEBUG_CHECKS
					printf("\tSECTION B5c\n");
#endif

					/* this is a soft state */
					temp_service->state_type=SOFT_STATE;

					/* log the service check retry */
					log_service_event(temp_service);
					state_was_logged=TRUE;

					/* run the service event handler to handle the soft state */
					handle_service_event(temp_service);

#ifdef REMOVED_021803
					/*** NOTE TO SELF - THIS SHOULD BE MOVED SOMEWHERE ELSE - 02/18/03 ***/
					/*** MOVED UP TO ~ LINE 780 ***/
					/* increment the current attempt number */
					temp_service->current_attempt=temp_service->current_attempt+1;
#endif

					if(temp_service->check_type==SERVICE_CHECK_ACTIVE)
						temp_service->next_check=(time_t)(temp_service->last_check+(temp_service->retry_interval*interval_length));
				        }
			        }
			

			/* we've reached the maximum number of service rechecks, so handle the error */
			else{

#ifdef DEBUG_CHECKS
				printf("\tSECTION B6a\n");
				printf("\tMAXED OUT! HSC: %d\n",hard_state_change);
#endif

				/* this is a hard state */
				temp_service->state_type=HARD_STATE;

				/* if we've hard a hard state change... */
				if(hard_state_change==TRUE){

#ifdef DEBUG_CHECKS
					printf("\tSECTION B6b\n");
#endif

					/* log the service problem (even if host is not up, which is new in 0.0.5) */
					log_service_event(temp_service);
					state_was_logged=TRUE;
				        }

				/* else log the problem (again) if this service is flagged as being volatile */
				else if(temp_service->is_volatile==TRUE){
#ifdef DEBUG_CHECKS
					printf("\tSECTION B6c\n");
#endif

					log_service_event(temp_service);
					state_was_logged=TRUE;
				        }

				/* check for start of flexible (non-fixed) scheduled downtime if we just had a hard error */
				if(hard_state_change==TRUE && temp_service->pending_flex_downtime>0)
					check_pending_flex_service_downtime(temp_service);

				/* (re)send notifications out about this service problem if the host is up (and was at last check also) and the dependencies were okay... */
				service_notification(temp_service,NOTIFICATION_NORMAL,NULL,NULL);

				/* run the service event handler if we changed state from the last hard state or if this service is flagged as being volatile */
				if(hard_state_change==TRUE || temp_service->is_volatile==TRUE){
#ifdef DEBUG_CHECKS
					printf("\tSECTION B6d\n");
#endif

					handle_service_event(temp_service);
				        }

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

#ifdef DEBUG_CHECKS
		printf("\tDONE\n");
#endif

		/* if we're stalking this state type and state was not already logged AND the plugin output changed since last check, log it now.. */
		if(temp_service->state_type==HARD_STATE && state_change==FALSE && state_was_logged==FALSE && strcmp(old_plugin_output,temp_service->plugin_output)){

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
		broker_service_check(NEBTYPE_SERVICECHECK_PROCESSED,NEBFLAG_NONE,NEBATTR_NONE,temp_service,temp_service->check_type,queued_svc_msg.start_time,queued_svc_msg.finish_time,NULL,temp_service->latency,temp_service->execution_time,service_check_timeout,queued_svc_msg.early_timeout,queued_svc_msg.return_code,NULL,NULL);
#endif

		/* set the checked flag */
		temp_service->has_been_checked=TRUE;

		/* update the current service status log */
		update_service_status(temp_service,FALSE);

		/* check to see if the service is flapping */
		check_for_service_flapping(temp_service,TRUE);

		/* check to see if the associated host is flapping */
		check_for_host_flapping(temp_host,TRUE);

		/* update service performance info */
		update_service_performance_data(temp_service);

		/* break out if we've been here too long (max_check_reaper_time seconds) */
		time(&current_time);
		if((int)(current_time-reaper_start_time)>max_check_reaper_time)
			break;

#if OLD_CRUD
		/* check for external commands if we're doing so as often as possible */
		if(command_check_interval==-1)
			check_for_external_commands();
#endif
	        }

#ifdef DEBUG3
	printf("Finished reaping service check results.\n");
#endif

#ifdef DEBUG0
	printf("reap_service_checks() end\n");
#endif

	return;
        }



/* schedules an immediate or delayed service check */
void schedule_service_check(service *svc,time_t check_time,int forced){
	timed_event *temp_event;
	timed_event *new_event;
	int found=FALSE;
	char temp_buffer[MAX_INPUT_BUFFER];
	int use_original_event=TRUE;

#ifdef DEBUG0
	printf("schedule_service_check() start\n");
#endif

	/* don't schedule a check if active checks are disabled */
	/* 10/19/07 EG - schedule service checks even if they're disabled on a program-wide basis */
	/*if((execute_service_checks==FALSE || svc->checks_enabled==FALSE) && forced==FALSE)*/
	if(svc->checks_enabled==FALSE && forced==FALSE)
		return;

	/* allocate memory for a new event item */
	new_event=(timed_event *)malloc(sizeof(timed_event));
	if(new_event==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Could not reschedule check of service '%s' on host '%s'!\n",svc->description,svc->host_name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);

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
			free(new_event);
			return;
		        }

		remove_event(temp_event,&event_list_low);
		free(temp_event);
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



/* checks service dependencies */
int check_service_dependencies(service *svc,int dependency_type){
	servicedependency *temp_dependency;
	service *temp_service;
	int state;

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
	hostdependency *temp_dependency;
	host *temp_host;

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

		/* is the host we depend on in state that fails the dependency tests? */
		if(temp_host->current_state==HOST_UP && temp_dependency->fail_on_up==TRUE)
			return DEPENDENCIES_FAILED;
		if(temp_host->current_state==HOST_DOWN && temp_dependency->fail_on_down==TRUE)
			return DEPENDENCIES_FAILED;
		if(temp_host->current_state==HOST_UNREACHABLE && temp_dependency->fail_on_unreachable==TRUE)
			return DEPENDENCIES_FAILED;
		if((temp_host->current_state==HOST_UP && temp_host->has_been_checked==FALSE) && temp_dependency->fail_on_pending==TRUE)
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
	service *temp_service;
	time_t current_time;
	time_t expected_time;
	char buffer[MAX_INPUT_BUFFER];

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
		expected_time=(time_t)(temp_service->next_check+temp_service->latency+service_check_timeout+service_check_reaper_interval+600);

		/* this service was supposed to have executed a while ago, but for some reason the results haven't come back in... */
		if(expected_time<current_time){

			/* log a warning */
			snprintf(buffer,sizeof(buffer)-1,"Warning: The check of service '%s' on host '%s' looks like it was orphaned (results never came back).  I'm scheduling an immediate check of the service...\n",temp_service->description,temp_service->host_name);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);

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
	service *temp_service;
	time_t current_time;
	time_t expiration_time;
	int freshness_threshold;
	char buffer[MAX_INPUT_BUFFER];

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
			snprintf(buffer,sizeof(buffer)-1,"Warning: The results of service '%s' on host '%s' are stale by %lu seconds (threshold=%d seconds).  I'm forcing an immediate check of the service.\n",temp_service->description,temp_service->host_name,(current_time-expiration_time),freshness_threshold);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);

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
/******************* ROUTE/HOST CHECK FUNCTIONS *******************/
/******************************************************************/


/*** ON-DEMAND HOST CHECKS USE THIS FUNCTION ***/
/* check to see if we can reach the host */
int verify_route_to_host(host *hst, int check_options){
	int result;

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
	time_t current_time;
	time_t preferred_time;
	time_t next_valid_time;
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


	/**** RUN THE SCHEDULED HOST CHECK ****/
	/* check route to the host (propagate problems and recoveries both up and down the tree) */
	if(perform_check==TRUE)
		check_host(hst,PROPAGATE_TO_PARENT_HOSTS | PROPAGATE_TO_CHILD_HOSTS,hst->check_options);


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



/* check freshness of host results */
void check_host_result_freshness(void){
	host *temp_host;
	time_t current_time;
	time_t expiration_time;
	int freshness_threshold;
	char buffer[MAX_INPUT_BUFFER];

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
			snprintf(buffer,sizeof(buffer)-1,"Warning: The results of host '%s' are stale by %lu seconds (threshold=%d seconds).  I'm forcing an immediate check of the host.\n",temp_host->name,(current_time-expiration_time),freshness_threshold);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);

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
	char old_plugin_output[MAX_PLUGINOUTPUT_LENGTH]="";

#ifdef DEBUG0
	printf("check_host() start\n");
#endif

	/* ADDED 06/20/2006 EG */
	/* bail out if signal encountered */
	if(sigrestart==TRUE || sigshutdown==TRUE)
		return hst->current_state;

	/* high resolution time for broker */
	gettimeofday(&start_time,NULL);

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	end_time.tv_sec=0L;
	end_time.tv_usec=0L;
	broker_host_check(NEBTYPE_HOSTCHECK_INITIATE,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,hst->current_state,hst->state_type,start_time,end_time,hst->host_check_command,hst->latency,0.0,host_check_timeout,FALSE,0,NULL,NULL,NULL,NULL);
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
	strncpy(old_plugin_output,(hst->plugin_output==NULL)?"":hst->plugin_output,sizeof(old_plugin_output)-1);
	old_plugin_output[sizeof(old_plugin_output)-1]='\x0';


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
			
			/* ADDED 06/20/2006 EG */
			/* bail out if signal encountered - use old state */
			if(sigrestart==TRUE || sigshutdown==TRUE){
				hst->current_attempt=1;
				hst->current_state=old_state;
				if(hst->plugin_output!=NULL)
					free(hst->plugin_output);
				hst->plugin_output=(char *)strdup(old_plugin_output);
				return hst->current_state;
				}

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

			/* ADDED 06/20/2006 EG */
			/* bail out if signal encountered - use old state */
			if(sigrestart==TRUE || sigshutdown==TRUE){
				hst->current_attempt=1;
				hst->current_state=old_state;
				if(hst->plugin_output!=NULL)
					free(hst->plugin_output);
				hst->plugin_output=(char *)strdup(old_plugin_output);
				return hst->current_state;
				}

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
	if(hst->last_state==hst->current_state && strcmp(old_plugin_output,hst->plugin_output)){

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
	execution_time=(double)((double)(end_time.tv_sec-start_time.tv_sec)+(double)((end_time.tv_usec-start_time.tv_usec)/1000)/1000.0);
	broker_host_check(NEBTYPE_HOSTCHECK_PROCESSED,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,hst->current_state,hst->state_type,start_time,end_time,hst->host_check_command,hst->latency,execution_time,0,FALSE,0,NULL,hst->plugin_output,hst->perf_data,NULL);
#endif

	/* check to see if the associated host is flapping */
	check_for_host_flapping(hst,TRUE);

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
	char processed_command[MAX_COMMAND_BUFFER];
	char raw_command[MAX_COMMAND_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	time_t current_time;
	time_t start_time;
	struct timeval start_time_hires;
	struct timeval end_time_hires;
	char *temp_ptr;
	int early_timeout=FALSE;
	double exectime;
	char temp_plugin_output[MAX_PLUGINOUTPUT_LENGTH];
		

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

		return hst->current_state;
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
	broker_host_check(NEBTYPE_HOSTCHECK_RAW_START,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,return_result,hst->state_type,start_time_hires,end_time_hires,hst->host_check_command,0.0,0.0,host_check_timeout,early_timeout,result,processed_command,hst->plugin_output,hst->perf_data,NULL);
#endif

#ifdef DEBUG3
	printf("\t\tRaw Command: %s\n",raw_command);
	printf("\t\tProcessed Command: %s\n",processed_command);
#endif

	/* clear plugin output and performance data buffers */
	strcpy(hst->plugin_output,"");
	strcpy(hst->perf_data,"");

	/* run the host check command */
	result=my_system(processed_command,host_check_timeout,&early_timeout,&exectime,temp_plugin_output,MAX_PLUGINOUTPUT_LENGTH-1);

	/* if the check timed out, report an error */
	if(early_timeout==TRUE){

		snprintf(hst->plugin_output,MAX_PLUGINOUTPUT_LENGTH-1,"Host check timed out after %d seconds\n",host_check_timeout);
		hst->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';

		/* log the timeout */
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Warning: Host check command '%s' for host '%s' timed out after %d seconds\n",processed_command,hst->name,host_check_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* calculate total execution time */
	hst->execution_time=exectime;

	/* record check type */
	hst->check_type=HOST_CHECK_ACTIVE;

	/* check for empty plugin output */
	if(!strcmp(temp_plugin_output,""))
		strcpy(temp_plugin_output,"(No Information Returned From Host Check)");

	/* first part of plugin output (up to pipe) is status info */
	temp_ptr=strtok(temp_plugin_output,"|\n");

	/* make sure the plugin output isn't NULL */
	if(temp_ptr==NULL){
		strncpy(hst->plugin_output,"(No output returned from host check)",MAX_PLUGINOUTPUT_LENGTH-1);
		hst->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
	        }

	else{

		strip(temp_ptr);
		if(!strcmp(temp_ptr,"")){
			strncpy(hst->plugin_output,"(No output returned from host check)",MAX_PLUGINOUTPUT_LENGTH-1);
			hst->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
                        }
		else{
			strncpy(hst->plugin_output,temp_ptr,MAX_PLUGINOUTPUT_LENGTH-1);
			hst->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
                        }
	        }

	/* second part of plugin output (after pipe) is performance data (which may or may not exist) */
	temp_ptr=strtok(NULL,"\n");

	/* grab performance data if we found it available */
	if(temp_ptr!=NULL){
		strip(temp_ptr);
		strncpy(hst->perf_data,temp_ptr,MAX_PLUGINOUTPUT_LENGTH-1);
		hst->perf_data[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';
	        }

	/* replace semicolons in plugin output (but not performance data) with colons */
	temp_ptr=hst->plugin_output;
	while((temp_ptr=strchr(temp_ptr,';')))
	      *temp_ptr=':';


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
	broker_host_check(NEBTYPE_HOSTCHECK_RAW_END,NEBFLAG_NONE,NEBATTR_NONE,hst,HOST_CHECK_ACTIVE,return_result,hst->state_type,start_time_hires,end_time_hires,hst->host_check_command,0.0,exectime,host_check_timeout,early_timeout,result,processed_command,hst->plugin_output,hst->perf_data,NULL);
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



/* schedules an immediate or delayed host check */
void schedule_host_check(host *hst,time_t check_time,int forced){
	timed_event *temp_event;
	timed_event *new_event;
	int found=FALSE;
	char temp_buffer[MAX_INPUT_BUFFER];
	int use_original_event=TRUE;

#ifdef DEBUG0
	printf("schedule_host_check() start\n");
#endif

	/* don't schedule a check if active checks are disabled */
	if((execute_host_checks==FALSE || hst->checks_enabled==FALSE) && forced==FALSE)
		return;

	/* allocate memory for a new event item */
	new_event=(timed_event *)malloc(sizeof(timed_event));
	if(new_event==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer),"Warning: Could not reschedule check of host '%s'!\n",hst->name);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);

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
			free(new_event);
			return;
		        }

		remove_event(temp_event,&event_list_low);
		free(temp_event);
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
