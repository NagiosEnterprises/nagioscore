/*****************************************************************************
 *
 * CHECKS.C - Service and host check functions for Nagios
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   11-14-2002
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
#include "../common/statusdata.h"
#include "../common/downtime.h"
#include "nagios.h"
#include "perfdata.h"

#ifdef EMBEDDEDPERL
#include <EXTERN.h>
#include <perl.h>
#include <fcntl.h>
#endif

extern char     temp_file[MAX_FILENAME_LENGTH];

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
extern int      obsess_over_services;

extern int      check_service_freshness;

extern time_t   program_start;

extern timed_event       *event_list_low;

extern service           *service_list;
extern host              *host_list;
extern servicedependency *servicedependency_list;
extern hostdependency    *hostdependency_list;

extern service_message svc_msg;



/******************************************************************/
/****************** SERVICE MONITORING FUNCTIONS ******************/
/******************************************************************/

/* forks a child process to run a service check, but does not wait for the service check result */
void run_service_check(service *svc){
	char raw_command[MAX_INPUT_BUFFER];
	char processed_command[MAX_INPUT_BUFFER];
	char plugin_output[MAX_PLUGINOUTPUT_LENGTH];
	char temp_buffer[MAX_INPUT_BUFFER];
	int check_service=TRUE;
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
	char tmpfname[32];
	char *args[5] = {"",DO_CLEAN, "", "", NULL };
	int isperl;
	int tmpfd;
#ifdef THREADEDPERL
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
	temp_host=find_host(svc->host_name,NULL);

	/* don't check the service if we couldn't find the associated host */
	if(temp_host==NULL)
		check_service=FALSE;

	/* if we shouldn't check the service, just reschedule it and leave... */
	if(check_service==FALSE){

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

	/* get the raw command line */
	get_raw_command_line(svc->service_check_command,raw_command,sizeof(raw_command));
	strip(raw_command);

	/* process any macros contained in the argument */
	process_macros(raw_command,processed_command,sizeof(processed_command),0);
	strip(processed_command);

	/* save service info */
	strncpy(svc_msg.host_name,svc->host_name,sizeof(svc_msg.host_name)-1);
	svc_msg.host_name[sizeof(svc_msg.host_name)-1]='\x0';
	strncpy(svc_msg.description,svc->description,sizeof(svc_msg.description)-1);
	svc_msg.description[sizeof(svc_msg.description)-1]='\x0';
	svc_msg.parallelized=svc->parallelize;
	svc_msg.check_time=current_time;
	svc_msg.finish_time=current_time;

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

	isperl=0;

	if(strstr(raw_command,"/bin/perl")!=NULL){

		isperl = 1;
		args[0] = fname;
		args[2] = tmpfname;

		if(strchr(processed_command,' ')==NULL)
			args[3]="";
		else
			args[3]=processed_command+strlen(fname)+1;

		/* call our perl interpreter to compile and optionally cache the command */
		perl_call_argv("Embed::Persistent::eval_file", G_DISCARD | G_EVAL, args);
	        }
#endif

	/* fork a child process */
	pid=fork();

	/* an error occurred while trying to fork */
	if(pid==-1)
		fork_error=TRUE;

	/* if we are in the child process... */
	else if(pid==0){

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

				/* generate a temporary filename to which stdout can be redirected. */
				snprintf(tmpfname,sizeof(tmpfname)-1,"/tmp/embeddedXXXXXX");
				if((tmpfd=mkstemp(tmpfname))==-1)
					_exit(STATE_UNKNOWN);

				/* execute our previously compiled script  */
				ENTER; 
				SAVETMPS;
				PUSHMARK(SP);
				XPUSHs(sv_2mortal(newSVpv(args[0],0)));
				XPUSHs(sv_2mortal(newSVpv(args[1],0)));
				XPUSHs(sv_2mortal(newSVpv(args[2],0)));
				XPUSHs(sv_2mortal(newSVpv(args[3],0)));
				PUTBACK;
				perl_call_pv("Embed::Persistent::run_package", G_EVAL);
				SPAGAIN;
				pclose_result=POPi;
				PUTBACK;
				FREETMPS;
				LEAVE;

				/* check return status  */
				if(SvTRUE(ERRSV)){
					pclose_result=-2;
#ifdef DEBUG1
					printf("embedded perl ran %s with error %s\n",fname,SvPV(ERRSV,PL_na));
#endif
			                }

				/* read back stdout from script */
				fp=fopen(tmpfname, "r");

				/* default return string in case nothing was returned */
				strcpy(plugin_output,"(No output!)");

				/* read output from plugin (which was redirected to temp file) */
				fgets(plugin_output,sizeof(plugin_output)-1,fp);
				plugin_output[sizeof(plugin_output)-1]='\x0';
				strip(plugin_output);

				/* close and delete temp file */
				fclose(fp);
				close(tmpfd);
				unlink(tmpfname);    
#ifdef DEBUG1
				printf("embedded perl plugin output was %d,%s\n",pclose_result, plugin_output);
#endif

				/* reset the alarm */
				alarm(0);

				/* test for execution error */
				if(pclose_result==-2){
					strncpy(svc_msg.output,"(Error returned by call to perl script)",sizeof(svc_msg.output)-1);
					svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
					svc_msg.return_code=STATE_CRITICAL;
					svc_msg.exited_ok=FALSE;
					svc_msg.finish_time=time(NULL);
					write_svc_message(&svc_msg);

					/* close write end of IPC pipe */
					close(ipc_pipe[1]);

					_exit(STATE_UNKNOWN);
			 	        }

				/* else write plugin check results to message queue */
				else{
					strncpy(svc_msg.output,plugin_output,sizeof(svc_msg.output)-1);
					svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
					svc_msg.return_code=pclose_result;
					svc_msg.exited_ok=TRUE;
					svc_msg.check_type=SERVICE_CHECK_ACTIVE;
					svc_msg.finish_time=time(NULL);
					write_svc_message(&svc_msg);
				        }

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

			/* close the process */
			pclose_result=pclose(fp);

			/* reset the alarm */
			alarm(0);

			/* test for execution error */
			if(pclose_result==-1){
				strncpy(svc_msg.output,"(Error returned by call to pclose() function)",sizeof(svc_msg.output)-1);
				svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
				svc_msg.return_code=STATE_CRITICAL;
				svc_msg.exited_ok=FALSE;
				svc_msg.finish_time=time(NULL);
				write_svc_message(&svc_msg);

				/* close write end of IPC pipe */
				close(ipc_pipe[1]);

				_exit(STATE_UNKNOWN);
			        }

			/* else write plugin check results to message queue */
			else{
				strncpy(svc_msg.output,plugin_output,sizeof(svc_msg.output)-1);
				svc_msg.output[sizeof(svc_msg.output)-1]='\x0';
				svc_msg.return_code=(int)WEXITSTATUS(pclose_result);
				svc_msg.exited_ok=TRUE;
				svc_msg.check_type=SERVICE_CHECK_ACTIVE;
				svc_msg.finish_time=time(NULL);
				write_svc_message(&svc_msg);
			        }

			/* close write end of IPC pipe */
			close(ipc_pipe[1]);

			/* return with plugin exit status - not really necessary... */
			_exit(pclose_result);
		        }

		/* close write end of IPC pipe */
		close(ipc_pipe[1]);

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
		temp_service=find_service(queued_svc_msg.host_name,queued_svc_msg.description,NULL);
		if(temp_service==NULL){

			snprintf(temp_buffer,sizeof(temp_buffer),"Warning:  Message queue contained results for service '%s' on host '%s'.  The service could not be found!\n",queued_svc_msg.description,queued_svc_msg.host_name);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
			write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);

			continue;
		        }

		/* update the execution time for this check */
		if(queued_svc_msg.check_time>current_time || queued_svc_msg.finish_time>current_time || (queued_svc_msg.finish_time<queued_svc_msg.check_time))
			temp_service->execution_time=0;
		else
			temp_service->execution_time=(int)(queued_svc_msg.finish_time-queued_svc_msg.check_time);

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
		temp_service->last_check=queued_svc_msg.check_time;

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

		/* get the host that this service runs on */
		temp_host=find_host(temp_service->host_name,NULL);

		/* if the service check was okay... */
		if(temp_service->current_state==STATE_OK){

			/* if the host has never been checked before... */
			if(temp_host->has_been_checked==FALSE){

				/* set the checked flag */
				temp_host->has_been_checked=TRUE;

				/* update the last check time */
				temp_host->last_check=temp_service->last_check;

				/* assume the host is up */
				temp_host->status=HOST_UP;

				/* plugin output should reflect our guess at the current host state */
				strncpy(temp_host->plugin_output,"(Host assumed to be up)",MAX_PLUGINOUTPUT_LENGTH);
				temp_host->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';

				/* update the status log with the host status */
				update_host_status(temp_host,FALSE);

				/* log the initial state if the user wants */
				if(log_initial_states==TRUE)
					log_host_event(temp_host,HOST_UP,HARD_STATE);
		                }

			/* log the initial state if the user wants */
			if(temp_service->has_been_checked==FALSE && log_initial_states==TRUE){
				log_service_event(temp_service,HARD_STATE);
				state_was_logged=TRUE;
			        }
		        }

		/* check for a state change (either soft or hard) */
		if(temp_service->current_state!=temp_service->last_state){
		
#ifdef DEBUG3
			printf("\t\tService '%s' on host '%s' has changed state since last check!\n",temp_service->description,temp_service->host_name);
#endif

			/* set the state change flag */
			state_change=TRUE;
		        }


		/* checks for a hard state change where host was down or dependency failed at last service check */
		if((temp_service->host_problem_at_last_check==TRUE || temp_service->dependency_failure_at_last_check==TRUE) && temp_service->current_state==STATE_OK){

#ifdef DEBUG3
			printf("\t\tService '%s' on host '%s' has had a HARD STATE CHANGE!!\n",temp_service->description,temp_service->host_name);
#endif

			hard_state_change=TRUE;
	                }


		/* check for a "normal" hard state change where max check attempts is reached */
		else if(temp_service->current_attempt>=temp_service->max_attempts){

			if(temp_service->current_state!=temp_service->last_hard_state){

#ifdef DEBUG3
				printf("\t\tService '%s' on host '%s' has had a HARD STATE CHANGE!!\n",temp_service->description,temp_service->host_name);
#endif

				hard_state_change=TRUE;
			        }
		        }

		/* reset last and next notification times and acknowledgement flag if necessary */
		if(state_change==TRUE || hard_state_change==TRUE){

			/* reset notification times */
			temp_service->last_notification=(time_t)0;
			temp_service->next_notification=(time_t)0;

			/* reset notification supression option */
			temp_service->no_more_notifications=FALSE;

			/* reset acknowledgement flag */
			temp_service->problem_has_been_acknowledged=FALSE;

			/* do NOT reset current notification number!!! */
			/*temp_service->current_notification_number=0;*/
		        }

		/* initialize the last state change times if necessary */
		if(temp_service->last_state_change==(time_t)0)
			temp_service->last_state_change=temp_service->last_check;
		if(temp_host->last_state_change==(time_t)0)
			temp_host->last_state_change=temp_service->last_check;



		/**************************************/
		/******* SERVICE CHECK OK LOGIC *******/
		/**************************************/

		/* if the service is up and running OK... */
		if(temp_service->current_state==STATE_OK){

			/* clear the next notification time (this is not used, since we are now in an OK state) */
			temp_service->next_notification=(time_t)0;

			/* reset the acknowledgement flag (this should already have been done, but just in case...) */
			temp_service->problem_has_been_acknowledged=FALSE;

			/* the service check was okay, so the associated host must be up... */
			if(temp_host->status!=HOST_UP){

				/* verify the route to the host and send out host recovery notifications */
				verify_route_to_host(temp_host);

				/* set the host problem flag (i.e. don't notify about recoveries for this service) */
				temp_service->host_problem_at_last_check=TRUE;
			        }

			/* if a hard service recovery has occurred... */
			if(hard_state_change==TRUE){

				/* set the state type macro */
				temp_service->state_type=HARD_STATE;

				/* update service state times and last state change time */
				update_service_state_times(temp_service);

				/* log the service recovery */
				log_service_event(temp_service,HARD_STATE);
				state_was_logged=TRUE;

				/* notify contacts about the service recovery */
				service_notification(temp_service,NULL);

				/* run the service event handler to handle the hard state change */
				handle_service_event(temp_service,HARD_STATE);
			        }

			/* else if a soft service recovery has occurred... */
			else if(state_change==TRUE){

				/* this is a soft recovery */
				temp_service->state_type=SOFT_STATE;

				/* update service state times and last state change time */
				update_service_state_times(temp_service);

				/* log the soft recovery */
				log_service_event(temp_service,SOFT_STATE);
				state_was_logged=TRUE;

				/* run the service event handler to handle the soft state change */
				handle_service_event(temp_service,SOFT_STATE);
		                }

			/* else no service state change has occured... */

			/* should we obsessive over service checks? */
			if(obsess_over_services==TRUE)
				obsessive_compulsive_service_check_processor(temp_service,temp_service->state_type);

			/* reset all service variables because its okay now... */
			temp_service->host_problem_at_last_check=FALSE;
			temp_service->dependency_failure_at_last_check=FALSE;
			temp_service->no_recovery_notification=FALSE;
			temp_service->current_attempt=1;
			temp_service->state_type=HARD_STATE;
			temp_service->last_hard_state=STATE_OK;
			temp_service->last_notification=(time_t)0;
			temp_service->next_notification=(time_t)0;
			temp_service->current_notification_number=0;
			temp_service->problem_has_been_acknowledged=FALSE;
			temp_service->has_been_unknown=FALSE;
			temp_service->has_been_warning=FALSE;
			temp_service->has_been_critical=FALSE;
			temp_service->no_more_notifications=FALSE;

			if(temp_service->check_type==SERVICE_CHECK_ACTIVE)
				temp_service->next_check=(time_t)(temp_service->last_check+(temp_service->check_interval*interval_length));
		        }


		/*******************************************/
		/******* SERVICE CHECK PROBLEM LOGIC *******/
		/*******************************************/

		/* hey, something's not working quite like it should... */
		else{

			/* reset the recovery notification flag (it may get set again though) */
			temp_service->no_recovery_notification=FALSE;
			
			/* check the route to the host if its supposed to be up right now... */
			if(temp_host->status==HOST_UP)
				route_result=verify_route_to_host(temp_host);

			/* else the host is either down or unreachable, so recheck it if necessary */
			else{

				/* we're using agressive host checking, so really do recheck the host... */
				if(use_aggressive_host_checking==TRUE)
					route_result=verify_route_to_host(temp_host);

				/* the service wobbled between non-OK states, so check the host... */
				else if(state_change==TRUE && temp_service->last_hard_state!=STATE_OK)
					route_result=verify_route_to_host(temp_host);

				/* else fake the host check, but (possibly) resend host notifications to contacts... */
				else{

					/* if the host has never been checked beforem, set the checked flag */
					if(temp_host->has_been_checked==FALSE)
						temp_host->has_been_checked=TRUE;

					/* update the last host check time */
					temp_host->last_check=temp_service->last_check;

					/* fake the route check result */
					route_result=temp_host->status;

					/* log the initial state if the user wants to and this host hasn't been checked yet */
					if(log_initial_states==TRUE && temp_service->has_been_checked==FALSE)
						log_host_event(temp_host,temp_host->status,HARD_STATE);

				        /* possibly re-send host notifications... */
					host_notification(temp_host,temp_host->status,NULL);
				        }
			        }

			/* if the host is down or unreachable ... */
			if(route_result!=HOST_UP){

				/* "fake" a hard state change for the service - well, its not really fake, but it didn't get caught earler... */
				if(temp_service->last_hard_state!=temp_service->current_state)
					hard_state_change=TRUE;

				/* update service state times if necessary and last state change time */
				if(hard_state_change==TRUE)
					update_service_state_times(temp_service);

				/* put service into a hard state without attempting check retries and don't send out notifications about it */
				temp_service->host_problem_at_last_check=TRUE;
				temp_service->state_type=HARD_STATE;
				temp_service->last_hard_state=temp_service->current_state;
				temp_service->current_attempt=1;
			        }

			/* else the host is up.. */
			else{

				/* check service notification dependencies */
				dependency_result=check_service_dependencies(temp_service,NOTIFICATION_DEPENDENCY);

				/* the service notification dependencies failed... */
				if(dependency_result==DEPENDENCIES_FAILED){

					/* set the dependency problem */
					temp_service->dependency_failure_at_last_check=TRUE;

					/* don't send a recovery notification if the service recovers at the next check */
					temp_service->no_recovery_notification=TRUE;
				        }

				/* the service notification dependencies are okay */
				else
					temp_service->dependency_failure_at_last_check=FALSE;


				/*  the host recovered since the last time the service was checked... */
				if(temp_service->host_problem_at_last_check==TRUE){

					/* next time the service is checked we shouldn't get into this same case... */
					temp_service->host_problem_at_last_check=FALSE;

					/* reset the current check counter, so we can mabye avoid a false recovery alarm - added 07/28/99 */
					temp_service->current_attempt=1;

					/* don't send a recovery notification if the service recovers at the next check */
					temp_service->no_recovery_notification=TRUE;
				        }
			        }

			 /* if we should retry the service check, do so (except it the host is down or unreachable!) */
			if(temp_service->current_attempt < temp_service->max_attempts){

				/* the host is down or unreachable, so don't attempt to retry the service check */
				if(route_result!=HOST_UP){

					/* the host is not up, so reschedule the next service check at regular interval */
					if(temp_service->check_type==SERVICE_CHECK_ACTIVE)
						temp_service->next_check=(time_t)(temp_service->last_check+(temp_service->check_interval*interval_length));

					/* log the problem as a hard state if the host just went down (new in 0.0.5) */
					if(hard_state_change==TRUE){
						log_service_event(temp_service,HARD_STATE);
						state_was_logged=TRUE;
					        }
				        }

				/* the host is up, so continue to retry the service check */
				else{

					/* this is a soft state */
					temp_service->state_type=SOFT_STATE;

				        /* update service state times if necessary and last state change time */
					if(state_change==TRUE)
						update_service_state_times(temp_service);

					/* log the service check retry */
					log_service_event(temp_service,SOFT_STATE);
					state_was_logged=TRUE;

					/* run the service event handler to handle the soft state */
					handle_service_event(temp_service,SOFT_STATE);

					/* increment the current attempt number */
					temp_service->current_attempt=temp_service->current_attempt+1;

					if(temp_service->check_type==SERVICE_CHECK_ACTIVE)
						temp_service->next_check=(time_t)(temp_service->last_check+(temp_service->retry_interval*interval_length));
				        }
			        }
			

			/* we've reached the maximum number of service rechecks, so handle the error */
			else{

				/* this is a hard state */
				temp_service->state_type=HARD_STATE;

				/* keep track of this state, in case we "float" amongst other non-OK states before recovering */
				if(temp_service->current_state==STATE_UNKNOWN)
					temp_service->has_been_unknown=TRUE;
				else if(temp_service->current_state==STATE_WARNING)
					temp_service->has_been_warning=TRUE;
				else if(temp_service->current_state==STATE_CRITICAL)
					temp_service->has_been_critical=TRUE;

				/* if we've hard a hard state change... */
				if(hard_state_change==TRUE){

					/* update service state times and last state change time */
					update_service_state_times(temp_service);

					/* log the service problem (even if host is not up, which is new in 0.0.5) */
					log_service_event(temp_service,HARD_STATE);
					state_was_logged=TRUE;
				        }

				/* else log the problem (again) if this service is flagged as being volatile */
				else if(temp_service->is_volatile==TRUE){
					log_service_event(temp_service,HARD_STATE);
					state_was_logged=TRUE;
				        }

				/* check for start of flexible (non-fixed) scheduled downtime if we just had a hard error */
				if(hard_state_change==TRUE && temp_service->pending_flex_downtime>0)
					check_pending_flex_service_downtime(temp_service);

				/* (re)send notifications out about this service problem if the host is up (and was at last check also) and the dependencies were okay... */
				service_notification(temp_service,NULL);

				/* run the service event handler if we changed state from the last hard state or if this service is flagged as being volatile */
				if(hard_state_change==TRUE || temp_service->is_volatile==TRUE)
					handle_service_event(temp_service,HARD_STATE);

				/* save the last hard state */
				temp_service->last_hard_state=temp_service->current_state;

				/* reschedule the next check at the regular interval */
				if(temp_service->check_type==SERVICE_CHECK_ACTIVE)
					temp_service->next_check=(time_t)(temp_service->last_check+(temp_service->check_interval*interval_length));
			        }



			/* should we obsessive over service checks? */
			if(obsess_over_services==TRUE)
				obsessive_compulsive_service_check_processor(temp_service,temp_service->state_type);
		        }

		/* reschedule the next service check ONLY for active checks */
		if(temp_service->check_type==SERVICE_CHECK_ACTIVE){

			/* make sure we don't get ourselves into too much trouble... */
			if(current_time>temp_service->next_check)
				temp_service->next_check=current_time;

			/* make sure we rescheduled the next service check at a valid time */
			preferred_time=temp_service->next_check;
			get_next_valid_time(preferred_time,&next_valid_time,temp_service->check_period);
			temp_service->next_check=next_valid_time;

			/* schedule a non-forced check */
			schedule_service_check(temp_service,temp_service->next_check,FALSE);
		        }

		/* if we're stalking this state type and state was not already logged AND the plugin output changed since last check, log it now.. */
		if(temp_service->state_type==HARD_STATE && state_change==FALSE && state_was_logged==FALSE && strcmp(old_plugin_output,temp_service->plugin_output)){

			if((temp_service->current_state==STATE_OK && temp_service->stalk_on_ok==TRUE))
				log_service_event(temp_service,HARD_STATE);
			
			else if((temp_service->current_state==STATE_WARNING && temp_service->stalk_on_warning==TRUE))
				log_service_event(temp_service,HARD_STATE);
			
			else if((temp_service->current_state==STATE_UNKNOWN && temp_service->stalk_on_unknown==TRUE))
				log_service_event(temp_service,HARD_STATE);
			
			else if((temp_service->current_state==STATE_CRITICAL && temp_service->stalk_on_critical==TRUE))
				log_service_event(temp_service,HARD_STATE);
		        }

		/* set the checked flag */
		temp_service->has_been_checked=TRUE;

		/* update the current service status log */
		update_service_status(temp_service,FALSE);

		/* check to see if the service is flapping */
		check_for_service_flapping(temp_service);

		/* check to see if the associated host is flapping */
		check_for_host_flapping(temp_host);

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



/* checks service dependencies */
int check_service_dependencies(service *svc,int dependency_type){
	servicedependency *temp_dependency;
	service *temp_service;
	int state;

#ifdef DEBUG0
	printf("check_service_dependencies() start\n");
#endif

	/* check all dependencies... */
	for(temp_dependency=servicedependency_list;temp_dependency!=NULL;temp_dependency=temp_dependency->next){

		/* only check dependencies of the desired type (notification or execution) */
		if(temp_dependency->dependency_type!=dependency_type)
			continue;

		/* if we have the right service... */
		if(!strcmp(svc->host_name,temp_dependency->dependent_host_name) && !strcmp(svc->description,temp_dependency->dependent_service_description)){

			/* find the service we depend on... */
			temp_service=find_service(temp_dependency->host_name,temp_dependency->service_description,NULL);
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
	for(temp_dependency=hostdependency_list;temp_dependency!=NULL;temp_dependency=temp_dependency->next){

		/* only check dependencies of the desired type (notification or execution) */
		if(temp_dependency->dependency_type!=dependency_type)
			continue;

		/* if we have the right host... */
		if(!strcmp(hst->name,temp_dependency->dependent_host_name)){

			/* find the host we depend on... */
			temp_host=find_host(temp_dependency->host_name,NULL);
			if(temp_host==NULL)
				continue;

			/* is the host we depend on in state that fails the dependency tests? */
			if(temp_host->status==HOST_UP && temp_dependency->fail_on_up==TRUE)
				return DEPENDENCIES_FAILED;
			if(temp_host->status==HOST_DOWN && temp_dependency->fail_on_down==TRUE)
				return DEPENDENCIES_FAILED;
			if(temp_host->status==HOST_UNREACHABLE && temp_dependency->fail_on_unreachable==TRUE)
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

			/* decremement the number of running service checks */
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

	/* bail out if we're not supposed to be checking freshness */
	if(check_service_freshness==FALSE)
		return;

	/* get the current time */
	time(&current_time);

	/* check all services... */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

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

		/* use user-supplied freshness threshold or auto-calculate a freshness threshold to use? */
		if(temp_service->freshness_threshold==0){
			if(temp_service->state_type==HARD_STATE || temp_service->current_state==STATE_OK)
				freshness_threshold=(temp_service->check_interval*interval_length)+temp_service->latency+15;
			else
				freshness_threshold=(temp_service->retry_interval*interval_length)+temp_service->latency+15;
		        }
		else
			freshness_threshold=temp_service->freshness_threshold;

		/* calculate expiration time */
		if(temp_service->has_been_checked==FALSE)
			expiration_time=(time_t)(program_start+freshness_threshold);
		else
			expiration_time=(time_t)(temp_service->last_check+freshness_threshold);

		/* the results for the last check of this service are stale */
		if(expiration_time<current_time){

			/* log a warning */
			snprintf(buffer,sizeof(buffer)-1,"Warning: The results of service '%s' on host '%s' are stale by %lu seconds (threshold=%lu seconds).  I'm forcing an immediate check of the service.\n",temp_service->description,temp_service->host_name,(current_time-expiration_time),freshness_threshold);
			buffer[sizeof(buffer)-1]='\x0';
			write_to_logs_and_console(buffer,NSLOG_RUNTIME_WARNING,TRUE);

			/* set the freshen flag */
			temp_service->is_being_freshened=TRUE;

			/* schedule an immediate forced check of the service */
			schedule_service_check(temp_service,current_time,TRUE);
		        }
	        }

#ifdef DEBUG0
	printf("check_service_result_freshness() end\n");
#endif

	return;
        }




/******************************************************************/
/******************* ROUTE/HOST CHECK FUNCTIONS *******************/
/******************************************************************/


/* check to see if we can reach the host */
int verify_route_to_host(host *hst){
	int result;

#ifdef DEBUG0
	printf("verify_route_to_host() start\n");
#endif

	/* check route to the host (propagate problems and recoveries both up and down the tree) */
	result=check_host(hst,PROPAGATE_TO_PARENT_HOSTS | PROPAGATE_TO_CHILD_HOSTS);

#ifdef DEBUG0
	printf("verify_route_to_host() end\n");
#endif

	return result;
        }



/* see if the remote host is alive at all */
int check_host(host *hst,int propagation_options){
	int result=HOST_UP;
	int parent_result=HOST_UP;
	host *parent_host=NULL;
	hostsmember *temp_hostsmember=NULL;
	host *child_host=NULL;
	int return_result=HOST_UP;
	int max_check_attempts=1;
	int route_blocked=TRUE;
	int old_state=HOST_UP;
	char old_plugin_output[MAX_PLUGINOUTPUT_LENGTH]="";

#ifdef DEBUG0
	printf("check_host() start\n");
#endif

	/* make sure we return the original host state unless it changes... */
	return_result=hst->status;

	/* set the checked flag */
	hst->has_been_checked=TRUE;

	/* save the old plugin output and host state for use with state stalking routines */
	old_state=hst->status;
	strncpy(old_plugin_output,(hst->plugin_output==NULL)?"":hst->plugin_output,sizeof(old_plugin_output)-1);
	old_plugin_output[sizeof(old_plugin_output)-1]='\x0';

	/* if the host is already down or unreachable... */
	if(hst->status!=HOST_UP){

		/* how many times should we retry checks for this host? */
		if(use_aggressive_host_checking==FALSE)
			max_check_attempts=1;
		else
			max_check_attempts=hst->max_attempts;

		/* retry the host check as many times as necessary or allowed... */
		for(hst->current_attempt=1;hst->current_attempt<=max_check_attempts;hst->current_attempt++){
			
			/* check the host */
			result=run_host_check(hst);

			/* update performance data */
			update_host_performance_data(hst,result,HARD_STATE);

			/* the host recovered from a hard problem... */
			if(result==HOST_UP){

				return_result=HOST_UP;

				/* handle the hard host recovery */
				handle_host_state(hst,HOST_UP,HARD_STATE);

				/* propagate the host recovery upwards (at least one parent should be up now) */
				if(propagation_options & PROPAGATE_TO_PARENT_HOSTS){

					/* propagate to all parent hosts */
					for(temp_hostsmember=hst->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

						/* find the parent host */
						parent_host=find_host(temp_hostsmember->host_name,NULL);

						/* check the parent host (and propagate upwards) if its not up */
						if(parent_host!=NULL && parent_host->status!=HOST_UP)
							check_host(parent_host,PROPAGATE_TO_PARENT_HOSTS);
					        }
				        }

				/* propagate the host recovery downwards (children may or may not be down) */
				if(propagation_options & PROPAGATE_TO_CHILD_HOSTS){

					/* check all child hosts... */
					for(child_host=host_list;child_host!=NULL;child_host=child_host->next){

						/* if this is a child of the host, check it if it is not marked as UP */
						if(is_host_immediate_child_of_host(hst,child_host)==TRUE && child_host->status!=HOST_UP)
						        check_host(child_host,PROPAGATE_TO_CHILD_HOSTS);
					        }
				        }

				break;
			        }

		        }

		/* if the host isn't up and its currently marked as being unreachable, make absolutely sure it isn't down now. */
		/* to do this we have to check the (saved) status of all parent hosts.  this situation can occur if a host is */
		/* unreachable, its parent recovers, but it does not return to an UP state.  Even though it is not up, the host */
		/* has changed from an unreachable to a down state */

		if(hst->status==HOST_UNREACHABLE && result!=HOST_UP){

			/* check all parent hosts */
			for(temp_hostsmember=hst->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

				/* find the parent host */
				parent_host=find_host(temp_hostsmember->host_name,NULL);

				/* if at least one parent host is up, this host is no longer unreachable - it is now down instead */
				if(parent_host->status==HOST_UP){
					
					/* handle the hard host state change */
					handle_host_state(hst,HOST_DOWN,HARD_STATE);

					break;
				        }
			        }
		        }
	        }


	/* else the host is supposed to be up right now... */
	else{

		for(hst->current_attempt=1;hst->current_attempt<=hst->max_attempts;hst->current_attempt++){

			/* run the host check */
			result=run_host_check(hst);

			/* update performance data */
			if((result==HOST_UP && hst->current_attempt==1) || (result!=HOST_UP && hst->current_attempt>=hst->max_attempts))
				update_host_performance_data(hst,result,HARD_STATE);
			else
				update_host_performance_data(hst,result,SOFT_STATE);

			/* if this is the last check and we still haven't had a recovery, check the parents and children and then handle the hard host state */
			if(result!=HOST_UP && (hst->current_attempt==hst->max_attempts)){

				/* check all parent hosts */
				for(temp_hostsmember=hst->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

					/* find the parent host */
					parent_host=find_host(temp_hostsmember->host_name,NULL);

					/* check the parent host, assume its up if we can't find it, use the parent host's "old" status if we shouldn't propagate */
					if(parent_host==NULL)
						parent_result=HOST_UP;
					else if(propagation_options & PROPAGATE_TO_PARENT_HOSTS)
						parent_result=check_host(parent_host,PROPAGATE_TO_PARENT_HOSTS);
					else
						parent_result=parent_host->status;

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

				/* handle the hard host state (whether it is DOWN or UNREACHABLE) */
				handle_host_state(hst,return_result,HARD_STATE);

				/* propagate the host problem to all child hosts (they should be unreachable now unless they have multiple parents) */
				if(propagation_options & PROPAGATE_TO_CHILD_HOSTS){

					/* check all child hosts... */
					for(child_host=host_list;child_host!=NULL;child_host=child_host->next){

						/* if this is a child of the host, check it if it is not marked as UP */
						if(is_host_immediate_child_of_host(hst,child_host)==TRUE && child_host->status!=HOST_UP)
						        check_host(child_host,PROPAGATE_TO_CHILD_HOSTS);
					        }
				        }
			        }

			/* handle any soft states (during host check retries that return a non-ok state) */
			else if(result!=HOST_UP || (result==HOST_UP && hst->current_attempt!=1))
				handle_host_state(hst,result,SOFT_STATE);

			/* the host recovered (or it was never down), so break out of the check loop */
			if(result==HOST_UP){

				/* this is the first check of the host */
				if(hst->has_been_checked==FALSE){

					/* set the checked flag */
					hst->has_been_checked=TRUE;

					/* update the status log with the current host info */
					update_host_status(hst,FALSE);
				        }
				break;
			        }
	                }
	        }


	/* adjust the current check number if we exceeded the max count */
	if(hst->current_attempt>hst->max_attempts)
		hst->current_attempt=hst->max_attempts;

	/* if the host didn't change state and the plugin output differs from the last time it was checked, log the current state/output if state stalking is enabled */
	if(old_state==return_result && strcmp(old_plugin_output,hst->plugin_output)){

		if(return_result==HOST_UP && hst->stalk_on_up==TRUE)
			log_host_event(hst,HOST_UP,HARD_STATE);

		if(return_result==HOST_DOWN && hst->stalk_on_down==TRUE)
			log_host_event(hst,HOST_DOWN,HARD_STATE);

		if(return_result==HOST_UNREACHABLE && hst->stalk_on_unreachable==TRUE)
			log_host_event(hst,HOST_UNREACHABLE,HARD_STATE);
	        }


	/* check to see if the associated host is flapping */
	check_for_host_flapping(hst);

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
int run_host_check(host *hst){
	int result=STATE_OK;
	int return_result=HOST_UP;
	char processed_check_command[MAX_INPUT_BUFFER];
	char raw_check_command[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	command *temp_command;
	time_t start_time;
	time_t finish_time;
	char *temp_ptr;
	int early_timeout=FALSE;
	char temp_plugin_output[MAX_PLUGINOUTPUT_LENGTH];
		

#ifdef DEBUG0
	printf("run_host_check() start\n");
#endif

	/* if checks are disabled, just return the last host state */
	if(hst->checks_enabled==FALSE)
		return hst->status;

	/* if there is no host check command, just return with no error */
	if(hst->host_check_command==NULL){

#ifdef DEBUG3
		printf("\tNo host check command specified, so no check will be done (host assumed to be up)!\n");
#endif

		return HOST_UP;
	        }

	/* find the command we use to check the host */
	temp_command=find_command(hst->host_check_command,NULL);

	/* if we couldn't find the command, return with an error */
	if(temp_command==NULL){

#ifdef DEBUG3
		printf("\tCouldn't find the host check command!\n");
#endif

		return HOST_UP;
	        }

	/* grab the host macros */
	clear_volatile_macros();
	grab_host_macros(hst);

	/* get the last host check time */
	time(&start_time);
	hst->last_check=start_time;

	/* get the raw command line */
	strncpy(raw_check_command,temp_command->command_line,sizeof(raw_check_command));
	raw_check_command[sizeof(raw_check_command)-1]='\x0';

	/* process any macros in the check command */
	process_macros(raw_check_command,&processed_check_command[0],(int)sizeof(processed_check_command),0);

			
#ifdef DEBUG3
	printf("\t\tRaw Command: %s\n",hst->host_check_command);
	printf("\t\tProcessed Command: %s\n",processed_check_command);
#endif

	/* clear plugin output and performance data buffers */
	strcpy(hst->plugin_output,"");
	strcpy(hst->perf_data,"");

	/* run the host check command */
	result=my_system(processed_check_command,host_check_timeout,&early_timeout,temp_plugin_output,MAX_PLUGINOUTPUT_LENGTH-1);

	/* if the check timed out, report an error */
	if(early_timeout==TRUE){

		snprintf(hst->plugin_output,MAX_PLUGINOUTPUT_LENGTH-1,"Host check timed out after %d seconds\n",host_check_timeout);
		hst->plugin_output[MAX_PLUGINOUTPUT_LENGTH-1]='\x0';

		/* log the timeout */
		snprintf(temp_buffer,sizeof(temp_buffer)-1,"Warning: Host check command '%s' for host '%s' timed out after %d seconds\n",hst->host_check_command,hst->name,host_check_timeout);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_logs_and_console(temp_buffer,NSLOG_RUNTIME_WARNING,TRUE);
	        }

	/* calculate total execution time */
	time(&finish_time);
	hst->execution_time=(int)(finish_time-start_time);

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


	/* if we're not doing agressive host checking, let WARNING states indicate the host is up (fake the result to be STATE_OK) */
	if(use_aggressive_host_checking==FALSE && result==STATE_WARNING)
		result=STATE_OK;


	if(result==STATE_OK)
		return_result=HOST_UP;
	else
		return_result=HOST_DOWN;

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

