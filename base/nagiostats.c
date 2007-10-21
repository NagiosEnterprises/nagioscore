/*****************************************************************************
 *
 * NAGIOSTATS.C - Displays Nagios Statistics
 *
 * Program: Nagiostats
 * Version: 2.10
 * License: GPL
 * Copyright (c) 2003-2007 Ethan Galstad (nagios@nagios.org)
 *
 * Last Modified:   10-21-2007
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
#include "../include/nagios.h"
#include "../include/locations.h"

#define STATUS_NO_DATA             0
#define STATUS_INFO_DATA           1
#define STATUS_PROGRAM_DATA        2
#define STATUS_HOST_DATA           3
#define STATUS_SERVICE_DATA        4


char main_config_file[MAX_INPUT_BUFFER]=DEFAULT_CONFIG_FILE;
char status_file[MAX_INPUT_BUFFER]=DEFAULT_STATUS_FILE;
char *mrtg_variables=NULL;

int mrtg_mode=FALSE;

time_t status_creation_date=0L;
char *status_version=NULL;
time_t program_start=0L;
int status_service_entries=0;
int status_host_entries=0;
unsigned long nagios_pid=0L;

double min_service_state_change=0.0;
int have_min_service_state_change=FALSE;
double max_service_state_change=0.0;
int have_max_service_state_change=FALSE;
double average_service_state_change=0.0;
double min_active_service_state_change=0.0;
int have_min_active_service_state_change=FALSE;
double max_active_service_state_change=0.0;
int have_max_active_service_state_change=FALSE;
double average_active_service_state_change=0.0;
double min_active_service_latency=0.0;
int have_min_active_service_latency=FALSE;
double max_active_service_latency=0.0;
int have_max_active_service_latency=FALSE;
double average_active_service_latency=0.0;
double min_active_service_execution_time=0.0;
int have_min_active_service_execution_time=FALSE;
double max_active_service_execution_time=0.0;
int have_max_active_service_execution_time=FALSE;
double average_active_service_execution_time=0.0;
double min_passive_service_state_change=0.0;
int have_min_passive_service_state_change=FALSE;
double max_passive_service_state_change=0.0;
int have_max_passive_service_state_change=FALSE;
double average_passive_service_state_change=0.0;

int have_min_host_state_change=FALSE;
double min_host_state_change=0.0;
int have_max_host_state_change=FALSE;
double max_host_state_change=0.0;
double average_host_state_change=0.0;
int have_min_active_host_state_change=FALSE;
double min_active_host_state_change=0.0;
int have_max_active_host_state_change=FALSE;
double max_active_host_state_change=0.0;
double average_active_host_state_change=0.0;
int have_min_active_host_latency=FALSE;
double min_active_host_latency=0.0;
int have_max_active_host_latency=FALSE;
double max_active_host_latency=0.0;
double average_active_host_latency=0.0;
int have_min_active_host_execution_time=FALSE;
double min_active_host_execution_time=0.0;
int have_max_active_host_execution_time=FALSE;
double max_active_host_execution_time=0.0;
double average_active_host_execution_time=0.0;
double min_passive_host_state_change=0.0;
int have_min_passive_host_state_change=FALSE;
double max_passive_host_state_change=0.0;
int have_max_passive_host_state_change=FALSE;
double average_passive_host_state_change=0.0;

int passive_service_checks=0;
int active_service_checks=0;
int services_ok=0;
int services_warning=0;
int services_unknown=0;
int services_critical=0;
int services_flapping=0;
int services_in_downtime=0;
int services_checked=0;
int services_scheduled=0;
int passive_host_checks=0;
int active_host_checks=0;
int hosts_up=0;
int hosts_down=0;
int hosts_unreachable=0;
int hosts_flapping=0;
int hosts_in_downtime=0;
int hosts_checked=0;
int hosts_scheduled=0;

int passive_services_checked_last_1min=0;
int passive_services_checked_last_5min=0;
int passive_services_checked_last_15min=0;
int passive_services_checked_last_1hour=0;
int active_services_checked_last_1min=0;
int active_services_checked_last_5min=0;
int active_services_checked_last_15min=0;
int active_services_checked_last_1hour=0;
int passive_hosts_checked_last_1min=0;
int passive_hosts_checked_last_5min=0;
int passive_hosts_checked_last_15min=0;
int passive_hosts_checked_last_1hour=0;
int active_hosts_checked_last_1min=0;
int active_hosts_checked_last_5min=0;
int active_hosts_checked_last_15min=0;
int active_hosts_checked_last_1hour=0;

int total_external_command_buffer_slots=0;
int used_external_command_buffer_slots=0;
int high_external_command_buffer_slots=0;
int total_check_result_buffer_slots=0;
int used_check_result_buffer_slots=0;
int high_check_result_buffer_slots=0;



int display_mrtg_values(void);
int display_stats(void);
int read_config_file(void);
int read_status_file(void);
void strip(char *);
void get_time_breakdown(unsigned long,int *,int *,int *,int *);


int main(int argc, char **argv){
	int result;
	int error=FALSE;
	int display_license=FALSE;
	int display_help=FALSE;
	int c;

#ifdef HAVE_GETOPT_H
	int option_index=0;
	static struct option long_options[]=
	{
		{"help",no_argument,0,'h'},
		{"version",no_argument,0,'V'},
		{"license",no_argument,0,'L'},
		{"config",required_argument,0,'c'},
		{"mrtg",no_argument,0,'m'},
		{"data",required_argument,0,'d'},
		{0,0,0,0}
	};
#endif

	/* get all command line arguments */
	while(1){

#ifdef HAVE_GETOPT_H
		c=getopt_long(argc,argv,"+hVLc:md:",long_options,&option_index);
#else
		c=getopt(argc,argv,"+hVLc:md:");
#endif

		if(c==-1 || c==EOF)
			break;

		switch(c){
			
		case '?':
		case 'h':
			display_help=TRUE;
			break;
		case 'V':
			display_license=TRUE;
			break;
		case 'L':
			display_license=TRUE;
			break;
		case 'c':
			strncpy(main_config_file,optarg,sizeof(main_config_file));
			main_config_file[sizeof(main_config_file)-1]='\x0';
			break;
		case 'm':
			mrtg_mode=TRUE;
			break;
		case 'd':
			mrtg_variables=strdup(optarg);
			break;

		default:
			break;
		        }

	        }

	if(mrtg_mode==FALSE){
		printf("\nNagios Stats %s\n",PROGRAM_VERSION);
		printf("Copyright (c) 2003-2007 Ethan Galstad (www.nagios.org)\n");
		printf("Last Modified: %s\n",PROGRAM_MODIFICATION_DATE);
		printf("License: GPL\n\n");
	        }

	/* just display the license */
	if(display_license==TRUE){

		printf("This program is free software; you can redistribute it and/or modify\n");
		printf("it under the terms of the GNU General Public License version 2 as\n");
		printf("published by the Free Software Foundation.\n\n");
		printf("This program is distributed in the hope that it will be useful,\n");
		printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
		printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
		printf("GNU General Public License for more details.\n\n");
		printf("You should have received a copy of the GNU General Public License\n");
		printf("along with this program; if not, write to the Free Software\n");
		printf("Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n\n");

		exit(OK);
	        }

	/* if there are no command line options (or if we encountered an error), print usage */
	if(error==TRUE || display_help==TRUE){

		printf("Usage: %s [options]\n",argv[0]);
		printf("\n");
		printf("Startup:\n");
		printf(" -V, --version      display program version information and exit.\n");
		printf(" -L, --license      display license information and exit.\n");
		printf(" -h, --help         display usage information and exit.\n");
		printf("\n");
		printf("Input file:\n");
		printf(" -c, --config=FILE  specifies location of main Nagios config file.\n");
		printf("\n");
		printf("Output:\n");
		printf(" -m, --mrtg         display output in MRTG compatible format.\n");
		printf(" -d, --data=VARS    comma-seperated list of variables to output in MRTG\n");
		printf("                    (or compatible) format.  See possible values below.\n");
		printf("                    Percentages are rounded, times are in milliseconds.\n");
		printf("\n");
		printf("MRTG DATA VARIABLES (-d option):\n");
		printf(" PROGRUNTIME        string with time Nagios process has been running.\n");
		printf(" PROGRUNTIMETT      time Nagios process has been running (time_t format).\n");
		printf(" STATUSFILEAGE      string with age of status data file.\n");
		printf(" STATUSFILEAGETT    string with age of status data file (time_t format).\n");
		printf(" NAGIOSVERSION      string with Nagios version.\n");
		printf(" NAGIOSPID          pid number of Nagios deamon.\n");
		printf(" NAGIOSVERPID       string with Nagios version and PID.\n");
		printf(" TOTCMDBUF          total number of external command buffer slots available.\n");
		printf(" USEDCMDBUF         number of external command buffer slots currently in use.\n");
		printf(" HIGHCMDBUF         highest number of external command buffer slots ever in use.\n");
		printf(" TOTCHKBUF          total number of check result buffer slots available.\n");
		printf(" USEDCHKBUF         number of check result buffer slots currently in use.\n");
		printf(" HIGHCHKBUF         highest number of check result buffer slots ever in use.\n");
		printf(" NUMSERVICES        total number of services.\n");
		printf(" NUMHOSTS           total number of hosts.\n");
		printf(" NUMSVCOK           number of services OK.\n");
		printf(" NUMSVCWARN         number of services WARNING.\n");
		printf(" NUMSVCUNKN         number of services UNKNOWN.\n");
		printf(" NUMSVCCRIT         number of services CRITICAL.\n");
		printf(" NUMSVCPROB         number of service problems (WARNING, UNKNOWN or CRITIAL).\n");
		printf(" NUMSVCCHECKED      number of services that have been checked since start.\n");
		printf(" NUMSVCSCHEDULED    number of services that are currently scheduled to be checked.\n");
		printf(" NUMSVCFLAPPING     number of services that are currently flapping.\n");
		printf(" NUMSVCDOWNTIME     number of services that are currently in downtime.\n");
		printf(" NUMHSTUP           number of hosts UP.\n");
		printf(" NUMHSTDOWN         number of hosts DOWN.\n");
		printf(" NUMHSTUNR          number of hosts UNREACHABLE.\n");
		printf(" NUMHSTPROB         number of host problems (DOWN or UNREACHABLE).\n");
		printf(" NUMHSTCHECKED      number of hosts that have been checked since start.\n");
		printf(" NUMHSTSCHEDULED    number of hosts that are currently scheduled to be checked.\n");
		printf(" NUMHSTFLAPPING     number of hosts that are currently flapping.\n");
		printf(" NUMHSTDOWNTIME     number of hosts that are currently in downtime.\n");
		printf(" xxxACTSVCLAT       MIN/MAX/AVG active service check latency (ms).\n");
		printf(" xxxACTSVCEXT       MIN/MAX/AVG active service check execution time (ms).\n");
		printf(" xxxACTSVCPSC       MIN/MAX/AVG active service check %% state change.\n");
		printf(" xxxPSVSVCPSC       MIN/MAX/AVG passive service check %% state change.\n");
		printf(" xxxSVCPSC          MIN/MAX/AVG service check %% state change.\n");
		printf(" xxxACTHSTLAT       MIN/MAX/AVG active host check latency (ms).\n");
		printf(" xxxACTHSTEXT       MIN/MAX/AVG active host check execution time (ms).\n");
		printf(" xxxACTHSTPSC       MIN/MAX/AVG active host check %% state change.\n");
		printf(" xxxPSVHSTPSC       MIN/MAX/AVG passive host check %% state change.\n");
		printf(" xxxHSTPSC          MIN/MAX/AVG host check %% state change.\n");
		printf(" NUMACTHSTCHKxM     number of active host checks in last 1/5/15/60 minutes.\n");
		printf(" NUMPSVHSTCHKxM     number of passive host checks in last 1/5/15/60 minutes.\n");
		printf(" NUMACTSVCCHKxM     number of active service checks in last 1/5/15/60 minutes.\n");
		printf(" NUMPSVSVCCHKxM     number of passive service checks in last 1/5/15/60 minutes.\n");

		printf("\n");
		printf(" Note: Replace x's in MRTG variable names with 'MIN', 'MAX', 'AVG', or the\n");
		printf("       the appropriate number (i.e. '1', '5', '15', or '60').\n");
		printf("\n");

		exit(ERROR);
		}

	/* read main config file */
	result=read_config_file();
	if(result==ERROR && mrtg_mode==FALSE){
		printf("Error processing config file '%s'\n",main_config_file);
		return ERROR;
	        }

	/* read the status file */
	result=read_status_file();
	if(result==ERROR && mrtg_mode==FALSE){
		printf("Error reading status file '%s'\n",status_file);
		return ERROR;
	        }

	/* display stats */
	if(mrtg_mode==FALSE)
		display_stats();
	else
		display_mrtg_values();

	return OK;
        }



int display_mrtg_values(void){
	char *temp_ptr;
	time_t current_time;
	unsigned long time_difference;
	int days;
	int hours;
	int minutes;
	int seconds;

	time(&current_time);

	/* process all variables */
	for(temp_ptr=strtok(mrtg_variables,",");temp_ptr!=NULL;temp_ptr=strtok(NULL,",")){

		if(!strcmp(temp_ptr,"PROGRUNTIME")){
			time_difference=(current_time-program_start);
			get_time_breakdown(time_difference,&days,&hours,&minutes,&seconds);
			printf("%dd %dh %dm %ds\n",days,hours,minutes,seconds);
			}
		else if(!strcmp(temp_ptr,"PROGRUNTIMETT")){
			time_difference=(current_time-program_start);
			printf("%lu\n",time_difference);
			}
		else if(!strcmp(temp_ptr,"STATUSFILEAGE")){
			time_difference=(current_time-status_creation_date);
			get_time_breakdown(time_difference,&days,&hours,&minutes,&seconds);
			printf("%dd %dh %dm %ds\n",days,hours,minutes,seconds);
			}
		else if(!strcmp(temp_ptr,"STATUSFILEAGETT")){
			time_difference=(current_time-status_creation_date);
			printf("%lu\n",time_difference);
			}
		else if(!strcmp(temp_ptr,"NAGIOSVERSION"))
			printf("%s\n",status_version);
		else if(!strcmp(temp_ptr,"NAGIOSPID"))
			printf("%lu\n",nagios_pid);
		else if(!strcmp(temp_ptr,"NAGIOSVERPID"))
			printf("Nagios %s (pid=%lu)\n",status_version,nagios_pid);


		else if(!strcmp(temp_ptr,"TOTCMDBUF"))
			printf("%d\n",total_external_command_buffer_slots);
		else if(!strcmp(temp_ptr,"USEDCMDBUF"))
			printf("%d\n",used_external_command_buffer_slots);
		else if(!strcmp(temp_ptr,"HIGHCMDBUF"))
			printf("%d\n",high_external_command_buffer_slots);
		else if(!strcmp(temp_ptr,"TOTCHKBUF"))
			printf("%d\n",total_check_result_buffer_slots);
		else if(!strcmp(temp_ptr,"USEDCHKBUF"))
			printf("%d\n",used_check_result_buffer_slots);
		else if(!strcmp(temp_ptr,"HIGHCHKBUF"))
			printf("%d\n",high_check_result_buffer_slots);

		else if(!strcmp(temp_ptr,"NUMSERVICES"))
			printf("%d\n",status_service_entries);
		else if(!strcmp(temp_ptr,"NUMHOSTS"))
			printf("%d\n",status_host_entries);

		/* active service check latency */
		else if(!strcmp(temp_ptr,"MINACTSVCLAT"))
			printf("%d\n",(int)(min_active_service_latency*1000));
		else if(!strcmp(temp_ptr,"MAXACTSVCLAT"))
			printf("%d\n",(int)(max_active_service_latency*1000));
		else if(!strcmp(temp_ptr,"AVGACTSVCLAT"))
			printf("%d\n",(int)(average_active_service_latency*1000));

		/* active service check execution time */
		else if(!strcmp(temp_ptr,"MINACTSVCEXT"))
			printf("%d\n",(int)(min_active_service_execution_time*1000));
		else if(!strcmp(temp_ptr,"MAXACTSVCEXT"))
			printf("%d\n",(int)(max_active_service_execution_time*1000));
		else if(!strcmp(temp_ptr,"AVGACTSVCEXT"))
			printf("%d\n",(int)(average_active_service_execution_time*1000));

		/* active service check percent state change */
		else if(!strcmp(temp_ptr,"MINACTSVCPSC"))
			printf("%d\n",(int)min_active_service_state_change);
		else if(!strcmp(temp_ptr,"MAXACTSVCPSC"))
			printf("%d\n",(int)max_active_service_state_change);
		else if(!strcmp(temp_ptr,"AVGACTSVCPSC"))
			printf("%d\n",(int)average_active_service_state_change);

		/* passive service check percent state change */
		else if(!strcmp(temp_ptr,"MINPSVSVCPSC"))
			printf("%d\n",(int)min_passive_service_state_change);
		else if(!strcmp(temp_ptr,"MAXPSVSVCPSC"))
			printf("%d\n",(int)max_passive_service_state_change);
		else if(!strcmp(temp_ptr,"AVGPSVSVCPSC"))
			printf("%d\n",(int)average_passive_service_state_change);

		/* service check percent state change */
		else if(!strcmp(temp_ptr,"MINSVCPSC"))
			printf("%d\n",(int)min_service_state_change);
		else if(!strcmp(temp_ptr,"MAXSVCPSC"))
			printf("%d\n",(int)max_service_state_change);
		else if(!strcmp(temp_ptr,"AVGSVCPSC"))
			printf("%d\n",(int)average_service_state_change);

		/* active host check latency */
		else if(!strcmp(temp_ptr,"MINACTHSTLAT"))
			printf("%d\n",(int)(min_active_host_latency*1000));
		else if(!strcmp(temp_ptr,"MAXACTHSTLAT"))
			printf("%d\n",(int)(max_active_host_latency*1000));
		else if(!strcmp(temp_ptr,"AVGACTHSTLAT"))
			printf("%d\n",(int)(average_active_host_latency*1000));

		/* active host check execution time */
		else if(!strcmp(temp_ptr,"MINACTHSTEXT"))
			printf("%d\n",(int)(min_active_host_execution_time*1000));
		else if(!strcmp(temp_ptr,"MAXACTHSTEXT"))
			printf("%d\n",(int)(max_active_host_execution_time*1000));
		else if(!strcmp(temp_ptr,"AVGACTHSTEXT"))
			printf("%d\n",(int)(average_active_host_execution_time*1000));

		/* active host check percent state change */
		else if(!strcmp(temp_ptr,"MINACTHSTPSC"))
			printf("%d\n",(int)min_active_host_state_change);
		else if(!strcmp(temp_ptr,"MAXACTHSTPSC"))
			printf("%d\n",(int)max_active_host_state_change);
		else if(!strcmp(temp_ptr,"AVGACTHSTPSC"))
			printf("%d\n",(int)average_active_host_state_change);

		/* passive host check percent state change */
		else if(!strcmp(temp_ptr,"MINPSVHSTPSC"))
			printf("%d\n",(int)min_passive_host_state_change);
		else if(!strcmp(temp_ptr,"MAXPSVHSTPSC"))
			printf("%d\n",(int)max_passive_host_state_change);
		else if(!strcmp(temp_ptr,"AVGPSVHSTPSC"))
			printf("%d\n",(int)average_passive_host_state_change);

		/* host check percent state change */
		else if(!strcmp(temp_ptr,"MINHSTPSC"))
			printf("%d\n",(int)min_host_state_change);
		else if(!strcmp(temp_ptr,"MAXHSTPSC"))
			printf("%d\n",(int)max_host_state_change);
		else if(!strcmp(temp_ptr,"AVGHSTPSC"))
			printf("%d\n",(int)average_host_state_change);

		/* active host checks over time */
		else if(!strcmp(temp_ptr,"NUMACTHSTCHK1M"))
			printf("%d\n",active_hosts_checked_last_1min);
		else if(!strcmp(temp_ptr,"NUMACTHSTCHK5M"))
			printf("%d\n",active_hosts_checked_last_5min);
		else if(!strcmp(temp_ptr,"NUMACTHSTCHK15M"))
			printf("%d\n",active_hosts_checked_last_15min);
		else if(!strcmp(temp_ptr,"NUMACTHSTCHK60M"))
			printf("%d\n",active_hosts_checked_last_1hour);

		/* passive host checks over time */
		else if(!strcmp(temp_ptr,"NUMPSVHSTCHK1M"))
			printf("%d\n",passive_hosts_checked_last_1min);
		else if(!strcmp(temp_ptr,"NUMPSVHSTCHK5M"))
			printf("%d\n",passive_hosts_checked_last_5min);
		else if(!strcmp(temp_ptr,"NUMPSVHSTCHK15M"))
			printf("%d\n",passive_hosts_checked_last_15min);
		else if(!strcmp(temp_ptr,"NUMPSVHSTCHK60M"))
			printf("%d\n",passive_hosts_checked_last_1hour);

		/* active service checks over time */
		else if(!strcmp(temp_ptr,"NUMACTSVCCHK1M"))
			printf("%d\n",active_services_checked_last_1min);
		else if(!strcmp(temp_ptr,"NUMACTSVCCHK5M"))
			printf("%d\n",active_services_checked_last_5min);
		else if(!strcmp(temp_ptr,"NUMACTSVCCHK15M"))
			printf("%d\n",active_services_checked_last_15min);
		else if(!strcmp(temp_ptr,"NUMACTSVCCHK60M"))
			printf("%d\n",active_services_checked_last_1hour);

		/* passive service checks over time */
		else if(!strcmp(temp_ptr,"NUMPSVSVCCHK1M"))
			printf("%d\n",passive_services_checked_last_1min);
		else if(!strcmp(temp_ptr,"NUMPSVSVCCHK5M"))
			printf("%d\n",passive_services_checked_last_5min);
		else if(!strcmp(temp_ptr,"NUMPSVSVCCHK15M"))
			printf("%d\n",passive_services_checked_last_15min);
		else if(!strcmp(temp_ptr,"NUMPSVSVCCHK60M"))
			printf("%d\n",passive_services_checked_last_1hour);

		/* service states */
		else if(!strcmp(temp_ptr,"NUMSVCOK"))
			printf("%d\n",services_ok);
		else if(!strcmp(temp_ptr,"NUMSVCWARN"))
			printf("%d\n",services_warning);
		else if(!strcmp(temp_ptr,"NUMSVCUNKN"))
			printf("%d\n",services_unknown);
		else if(!strcmp(temp_ptr,"NUMSVCCRIT"))
			printf("%d\n",services_critical);
		else if(!strcmp(temp_ptr,"NUMSVCPROB"))
			printf("%d\n",services_warning+services_unknown+services_critical);

		/* misc service info */
		else if(!strcmp(temp_ptr,"NUMSVCCHECKED"))
			printf("%d\n",services_checked);
		else if(!strcmp(temp_ptr,"NUMSVCSCHEDULED"))
			printf("%d\n",services_scheduled);
		else if(!strcmp(temp_ptr,"NUMSVCFLAPPING"))
			printf("%d\n",services_flapping);
		else if(!strcmp(temp_ptr,"NUMSVCDOWNTIME"))
			printf("%d\n",services_in_downtime);

		/* host states */
		else if(!strcmp(temp_ptr,"NUMHSTUP"))
			printf("%d\n",hosts_up);
		else if(!strcmp(temp_ptr,"NUMHSTDOWN"))
			printf("%d\n",hosts_down);
		else if(!strcmp(temp_ptr,"NUMHSTUNR"))
			printf("%d\n",hosts_unreachable);
		else if(!strcmp(temp_ptr,"NUMHSTPROB"))
			printf("%d\n",hosts_down+hosts_unreachable);

		/* misc host info */
		else if(!strcmp(temp_ptr,"NUMHSTCHECKED"))
			printf("%d\n",hosts_checked);
		else if(!strcmp(temp_ptr,"NUMHSTSCHEDULED"))
			printf("%d\n",hosts_scheduled);
		else if(!strcmp(temp_ptr,"NUMHSTFLAPPING"))
			printf("%d\n",hosts_flapping);
		else if(!strcmp(temp_ptr,"NUMHSTDOWNTIME"))
			printf("%d\n",hosts_in_downtime);

		else
			printf("%s\n",temp_ptr);
	        }

	return OK;
        }


int display_stats(void){
	time_t current_time;
	unsigned long time_difference;
	int days;
	int hours;
	int minutes;
	int seconds;

	time(&current_time);

	printf("CURRENT STATUS DATA\n");
	printf("----------------------------------------------------\n");
	printf("Status File:                          %s\n",status_file);
	time_difference=(current_time-status_creation_date);
	get_time_breakdown(time_difference,&days,&hours,&minutes,&seconds);
	printf("Status File Age:                      %dd %dh %dm %ds\n",days,hours,minutes,seconds);
	printf("Status File Version:                  %s\n",status_version);
	printf("\n");
	time_difference=(current_time-program_start);
	get_time_breakdown(time_difference,&days,&hours,&minutes,&seconds);
	printf("Program Running Time:                 %dd %dh %dm %ds\n",days,hours,minutes,seconds);
	printf("Nagios PID:                           %lu\n",nagios_pid);
	printf("Used/High/Total Command Buffers:      %d / %d / %d\n",used_external_command_buffer_slots,high_external_command_buffer_slots,total_external_command_buffer_slots);
	printf("Used/High/Total Check Result Buffers: %d / %d / %d\n",used_check_result_buffer_slots,high_check_result_buffer_slots,total_check_result_buffer_slots);
	printf("\n");
	printf("Total Services:                       %d\n",status_service_entries);
	printf("Services Checked:                     %d\n",services_checked);
	printf("Services Scheduled:                   %d\n",services_scheduled);
	printf("Active Service Checks:                %d\n",active_service_checks);
	printf("Passive Service Checks:               %d\n",passive_service_checks);
	printf("Total Service State Change:           %.3f / %.3f / %.3f %%\n",min_service_state_change,max_service_state_change,average_service_state_change);
	printf("Active Service Latency:               %.3f / %.3f / %.3f sec\n",min_active_service_latency,max_active_service_latency,average_active_service_latency);
	printf("Active Service Execution Time:        %.3f / %.3f / %.3f sec\n",min_active_service_execution_time,max_active_service_execution_time,average_active_service_execution_time);
	printf("Active Service State Change:          %.3f / %.3f / %.3f %%\n",min_active_service_state_change,max_active_service_state_change,average_active_service_state_change);
	printf("Active Services Last 1/5/15/60 min:   %d / %d / %d / %d\n",active_services_checked_last_1min,active_services_checked_last_5min,active_services_checked_last_15min,active_services_checked_last_1hour);
	printf("Passive Service State Change:         %.3f / %.3f / %.3f %%\n",min_passive_service_state_change,max_passive_service_state_change,average_passive_service_state_change);
	printf("Passive Services Last 1/5/15/60 min:  %d / %d / %d / %d\n",passive_services_checked_last_1min,passive_services_checked_last_5min,passive_services_checked_last_15min,passive_services_checked_last_1hour);
	printf("Services Ok/Warn/Unk/Crit:            %d / %d / %d / %d\n",services_ok,services_warning,services_unknown,services_critical);
	printf("Services Flapping:                    %d\n",services_flapping);
	printf("Services In Downtime:                 %d\n",services_in_downtime);
	printf("\n");
	printf("Total Hosts:                          %d\n",status_host_entries);
	printf("Hosts Checked:                        %d\n",hosts_checked);
	printf("Hosts Scheduled:                      %d\n",hosts_scheduled);
	printf("Active Host Checks:                   %d\n",active_host_checks);
	printf("Passive Host Checks:                  %d\n",passive_host_checks);
	printf("Total Host State Change:              %.3f / %.3f / %.3f %%\n",min_host_state_change,max_host_state_change,average_host_state_change);
	printf("Active Host Latency:                  %.3f / %.3f / %.3f sec\n",min_active_host_latency,max_active_host_latency,average_active_host_latency);
	printf("Active Host Execution Time:           %.3f / %.3f / %.3f sec\n",min_active_host_execution_time,max_active_host_execution_time,average_active_host_execution_time);
	printf("Active Host State Change:             %.3f / %.3f / %.3f %%\n",min_active_host_state_change,max_active_host_state_change,average_active_host_state_change);
	printf("Active Hosts Last 1/5/15/60 min:      %d / %d / %d / %d\n",active_hosts_checked_last_1min,active_hosts_checked_last_5min,active_hosts_checked_last_15min,active_hosts_checked_last_1hour);
	printf("Passive Host State Change:            %.3f / %.3f / %.3f %%\n",min_passive_host_state_change,max_passive_host_state_change,average_passive_host_state_change);
	printf("Passive Hosts Last 1/5/15/60 min:     %d / %d / %d / %d\n",passive_hosts_checked_last_1min,passive_hosts_checked_last_5min,passive_hosts_checked_last_15min,passive_hosts_checked_last_1hour);
	printf("Hosts Up/Down/Unreach:                %d / %d / %d\n",hosts_up,hosts_down,hosts_unreachable);
	printf("Hosts Flapping:                       %d\n",hosts_flapping);
	printf("Hosts In Downtime:                    %d\n",hosts_in_downtime);
	printf("\n");
	printf("\n");

	/*
	printf("CURRENT COMMENT DATA\n");
	printf("----------------------------------------------------\n");
	printf("\n");
	printf("\n");

	printf("CURRENT DOWNTIME DATA\n");
	printf("----------------------------------------------------\n");
	printf("\n");
	*/

	return OK;
        }


int read_config_file(void){
	char temp_buffer[MAX_INPUT_BUFFER];
	FILE *fp;
	char *var;
	char *val;


	fp=fopen(main_config_file,"r");
	if(fp==NULL)
		return ERROR;

	/* read all lines in the status file */
	while(fgets(temp_buffer,sizeof(temp_buffer)-1,fp)){

		strip(temp_buffer);

		/* skip blank lines and comments */
		if(temp_buffer[0]=='#' || temp_buffer[0]=='\x0')
			continue;

		var=strtok(temp_buffer,"=");
		val=strtok(NULL,"\n");
		if(val==NULL)
			continue;

		if(!strcmp(var,"status_file") || !strcmp(var,"status_log") || !strcmp(var,"xsddefault_status_log")){
			strncpy(status_file,val,sizeof(status_file));
			status_file[sizeof(status_file)-1]='\x0';
		        }
			
	        }

	fclose(fp);

	return OK;
        }


int read_status_file(void){
	char temp_buffer[MAX_INPUT_BUFFER];
	FILE *fp;
	int data_type=STATUS_NO_DATA;
	char *var;
	char *val;
	time_t current_time;
	unsigned long time_difference;

	double execution_time=0.0;
	double latency=0.0;
	int check_type=SERVICE_CHECK_ACTIVE;
	int current_state=STATE_OK;
	double state_change=0.0;
	int is_flapping=FALSE;
	int downtime_depth=0;
	time_t last_check=0L;
	int should_be_scheduled=TRUE;
	int has_been_checked=TRUE;


	time(&current_time);

	fp=fopen(status_file,"r");
	if(fp==NULL)
		return ERROR;

	/* read all lines in the status file */
	while(fgets(temp_buffer,sizeof(temp_buffer)-1,fp)){

		strip(temp_buffer);

		/* skip blank lines and comments */
		if(temp_buffer[0]=='#' || temp_buffer[0]=='\x0')
			continue;

		else if(!strcmp(temp_buffer,"info {"))
			data_type=STATUS_INFO_DATA;
		else if(!strcmp(temp_buffer,"program {"))
			data_type=STATUS_PROGRAM_DATA;
		else if(!strcmp(temp_buffer,"host {")){
			data_type=STATUS_HOST_DATA;
			status_host_entries++;
		        }
		else if(!strcmp(temp_buffer,"service {")){
			data_type=STATUS_SERVICE_DATA;
			status_service_entries++;
		        }

		else if(!strcmp(temp_buffer,"}")){

			switch(data_type){

			case STATUS_INFO_DATA:
				break;

			case STATUS_PROGRAM_DATA:
				break;

			case STATUS_HOST_DATA:
				average_host_state_change=(((average_host_state_change*((double)status_host_entries-1.0))+state_change)/(double)status_host_entries);
				if(have_min_host_state_change==FALSE || min_host_state_change>state_change){
					have_min_host_state_change=TRUE;
					min_host_state_change=state_change;
				        }
				if(have_max_host_state_change==FALSE || max_host_state_change<state_change){
					have_max_host_state_change=TRUE;
					max_host_state_change=state_change;
				        }
				if(check_type==HOST_CHECK_ACTIVE){
					active_host_checks++;
					average_active_host_latency=(((average_active_host_latency*((double)active_host_checks-1.0))+latency)/(double)active_host_checks);
					if(have_min_active_host_latency==FALSE || min_active_host_latency>latency){
						have_min_active_host_latency=TRUE;
						min_active_host_latency=latency;
					        }
					if(have_max_active_host_latency==FALSE || max_active_host_latency<latency){
						have_max_active_host_latency=TRUE;
						max_active_host_latency=latency;
					        }
					average_active_host_execution_time=(((average_active_host_execution_time*((double)active_host_checks-1.0))+execution_time)/(double)active_host_checks);
					if(have_min_active_host_execution_time==FALSE || min_active_host_execution_time>execution_time){
						have_min_active_host_execution_time=TRUE;
						min_active_host_execution_time=execution_time;
					        }
					if(have_max_active_host_execution_time==FALSE || max_active_host_execution_time<execution_time){
						have_max_active_host_execution_time=TRUE;
						max_active_host_execution_time=execution_time;
					        }
					average_active_host_state_change=(((average_active_host_state_change*((double)active_host_checks-1.0))+state_change)/(double)active_host_checks);
					if(have_min_active_host_state_change==FALSE || min_active_host_state_change>state_change){
						have_min_active_host_state_change=TRUE;
						min_active_host_state_change=state_change;
					        }
					if(have_max_active_host_state_change==FALSE || max_active_host_state_change<state_change){
						have_max_active_host_state_change=TRUE;
						max_active_host_state_change=state_change;
					        }
					time_difference=current_time-last_check;
					if(time_difference<=3600)
						active_hosts_checked_last_1hour++;
					if(time_difference<=900)
						active_hosts_checked_last_15min++;
					if(time_difference<=300)
						active_hosts_checked_last_5min++;
					if(time_difference<=60)
						active_hosts_checked_last_1min++;
				        }
				else{
					passive_host_checks++;
					average_passive_host_state_change=(((average_passive_host_state_change*((double)passive_host_checks-1.0))+state_change)/(double)passive_host_checks);
					if(have_min_passive_host_state_change==FALSE || min_passive_host_state_change>state_change){
						have_min_passive_host_state_change=TRUE;
						min_passive_host_state_change=state_change;
					        }
					if(have_max_passive_host_state_change==FALSE || max_passive_host_state_change<state_change){
						have_max_passive_host_state_change=TRUE;
						max_passive_host_state_change=state_change;
					        }
					time_difference=current_time-last_check;
					if(time_difference<=3600)
						passive_hosts_checked_last_1hour++;
					if(time_difference<=900)
						passive_hosts_checked_last_15min++;
					if(time_difference<=300)
						passive_hosts_checked_last_5min++;
					if(time_difference<=60)
						passive_hosts_checked_last_1min++;
				        }
				switch(current_state){
				case HOST_UP:
					hosts_up++;
					break;
				case HOST_DOWN:
					hosts_down++;
					break;
				case HOST_UNREACHABLE:
					hosts_unreachable++;
					break;
				default:
					break;
				        }
				if(is_flapping==TRUE)
					hosts_flapping++;
				if(downtime_depth>0)
					hosts_in_downtime++;
				if(has_been_checked==TRUE)
					hosts_checked++;
				if(should_be_scheduled==TRUE)
					hosts_scheduled++;
				break;

			case STATUS_SERVICE_DATA:
				average_service_state_change=(((average_service_state_change*((double)status_service_entries-1.0))+state_change)/(double)status_service_entries);
				if(have_min_service_state_change==FALSE || min_service_state_change>state_change){
					have_min_service_state_change=TRUE;
					min_service_state_change=state_change;
				        }
				if(have_max_service_state_change==FALSE || max_service_state_change<state_change){
					have_max_service_state_change=TRUE;
					max_service_state_change=state_change;
				        }
				if(check_type==SERVICE_CHECK_ACTIVE){
					active_service_checks++;
					average_active_service_latency=(((average_active_service_latency*((double)active_service_checks-1.0))+latency)/(double)active_service_checks);
					if(have_min_active_service_latency==FALSE || min_active_service_latency>latency){
						have_min_active_service_latency=TRUE;
						min_active_service_latency=latency;
					        }
					if(have_max_active_service_latency==FALSE || max_active_service_latency<latency){
						have_max_active_service_latency=TRUE;
						max_active_service_latency=latency;
					        }
					average_active_service_execution_time=(((average_active_service_execution_time*((double)active_service_checks-1.0))+execution_time)/(double)active_service_checks);
					if(have_min_active_service_execution_time==FALSE || min_active_service_execution_time>execution_time){
						have_min_active_service_execution_time=TRUE;
						min_active_service_execution_time=execution_time;
					        }
					if(have_max_active_service_execution_time==FALSE || max_active_service_execution_time<execution_time){
						have_max_active_service_execution_time=TRUE;
						max_active_service_execution_time=execution_time;
					        }
					average_active_service_state_change=(((average_active_service_state_change*((double)active_service_checks-1.0))+state_change)/(double)active_service_checks);
					if(have_min_active_service_state_change==FALSE || min_active_service_state_change>state_change){
						have_min_active_service_state_change=TRUE;
						min_active_service_state_change=state_change;
					        }
					if(have_max_active_service_state_change==FALSE || max_active_service_state_change<state_change){
						have_max_active_service_state_change=TRUE;
						max_active_service_state_change=state_change;
					        }
					time_difference=current_time-last_check;
					if(time_difference<=3600)
						active_services_checked_last_1hour++;
					if(time_difference<=900)
						active_services_checked_last_15min++;
					if(time_difference<=300)
						active_services_checked_last_5min++;
					if(time_difference<=60)
						active_services_checked_last_1min++;
				        }
				else{
					passive_service_checks++;
					average_passive_service_state_change=(((average_passive_service_state_change*((double)passive_service_checks-1.0))+state_change)/(double)passive_service_checks);
					if(have_min_passive_service_state_change==FALSE || min_passive_service_state_change>state_change){
						have_min_passive_service_state_change=TRUE;
						min_passive_service_state_change=state_change;
					        }
					if(have_max_passive_service_state_change==FALSE || max_passive_service_state_change<state_change){
						have_max_passive_service_state_change=TRUE;
						max_passive_service_state_change=state_change;
					        }
					time_difference=current_time-last_check;
					if(time_difference<=3600)
						passive_services_checked_last_1hour++;
					if(time_difference<=900)
						passive_services_checked_last_15min++;
					if(time_difference<=300)
						passive_services_checked_last_5min++;
					if(time_difference<=60)
						passive_services_checked_last_1min++;
				        }
				switch(current_state){
				case STATE_OK:
					services_ok++;
					break;
				case STATE_WARNING:
					services_warning++;
					break;
				case STATE_UNKNOWN:
					services_unknown++;
					break;
				case STATE_CRITICAL:
					services_critical++;
					break;
				default:
					break;
				        }
				if(is_flapping==TRUE)
					services_flapping++;
				if(downtime_depth>0)
					services_in_downtime++;
				if(has_been_checked==TRUE)
					services_checked++;
				if(should_be_scheduled==TRUE)
					services_scheduled++;
				break;

			default:
				break;
			        }

			data_type=STATUS_NO_DATA;

			execution_time=0.0;
			latency=0.0;
			check_type=0;
			current_state=0;
			state_change=0.0;
			is_flapping=FALSE;
			downtime_depth=0;
			last_check=(time_t)0;
			has_been_checked=FALSE;
			should_be_scheduled=FALSE;
		        }

		else if(data_type!=STATUS_NO_DATA){

			var=strtok(temp_buffer,"=");
			val=strtok(NULL,"\n");
			if(val==NULL)
				continue;

			switch(data_type){

			case STATUS_INFO_DATA:
				if(!strcmp(var,"created"))
					status_creation_date=strtoul(val,NULL,10);
				else if(!strcmp(var,"version"))
					status_version=strdup(val);
				break;

			case STATUS_PROGRAM_DATA:
				if(!strcmp(var,"program_start"))
					program_start=strtoul(val,NULL,10);
				else if(!strcmp(var,"total_external_command_buffer_slots"))
					total_external_command_buffer_slots=atoi(val);
				else if(!strcmp(var,"used_external_command_buffer_slots"))
					used_external_command_buffer_slots=atoi(val);
				else if(!strcmp(var,"high_external_command_buffer_slots"))
					high_external_command_buffer_slots=atoi(val);
				else if(!strcmp(var,"total_check_result_buffer_slots"))
					total_check_result_buffer_slots=atoi(val);
				else if(!strcmp(var,"used_check_result_buffer_slots"))
					used_check_result_buffer_slots=atoi(val);
				else if(!strcmp(var,"high_check_result_buffer_slots"))
					high_check_result_buffer_slots=atoi(val);
				else if(!strcmp(var,"nagios_pid"))
					nagios_pid=strtoul(val,NULL,10);
				break;

			case STATUS_HOST_DATA:
				if(!strcmp(var,"check_execution_time"))
					execution_time=strtod(val,NULL);
				else if(!strcmp(var,"check_latency"))
					latency=strtod(val,NULL);
				else if(!strcmp(var,"percent_state_change"))
					state_change=strtod(val,NULL);
				else if(!strcmp(var,"check_type"))
					check_type=atoi(val);
				else if(!strcmp(var,"current_state"))
					current_state=atoi(val);
				else if(!strcmp(var,"is_flapping"))
					is_flapping=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"scheduled_downtime_depth"))
					downtime_depth=atoi(val);
				else if(!strcmp(var,"last_check"))
					last_check=strtoul(val,NULL,10);
				else if(!strcmp(var,"has_been_checked"))
					has_been_checked=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"should_be_scheduled"))
					should_be_scheduled=(atoi(val)>0)?TRUE:FALSE;
				break;

			case STATUS_SERVICE_DATA:
				if(!strcmp(var,"check_execution_time"))
					execution_time=strtod(val,NULL);
				else if(!strcmp(var,"check_latency"))
					latency=strtod(val,NULL);
				else if(!strcmp(var,"percent_state_change"))
					state_change=strtod(val,NULL);
				else if(!strcmp(var,"check_type"))
					check_type=atoi(val);
				else if(!strcmp(var,"current_state"))
					current_state=atoi(val);
				else if(!strcmp(var,"is_flapping"))
					is_flapping=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"scheduled_downtime_depth"))
					downtime_depth=atoi(val);
				else if(!strcmp(var,"last_check"))
					last_check=strtoul(val,NULL,10);
				else if(!strcmp(var,"has_been_checked"))
					has_been_checked=(atoi(val)>0)?TRUE:FALSE;
				else if(!strcmp(var,"should_be_scheduled"))
					should_be_scheduled=(atoi(val)>0)?TRUE:FALSE;
				break;

			default:
				break;
			        }

		        }
	        }

	fclose(fp);

	return OK;
        }


/* strip newline, carriage return, and tab characters from beginning and end of a string */
void strip(char *buffer){
	register int x;
	register int y;
	register int z;

	if(buffer==NULL || buffer[0]=='\x0')
		return;

	/* strip end of string */
	y=(int)strlen(buffer);
	for(x=y-1;x>=0;x--){
		if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
			buffer[x]='\x0';
		else
			break;
	        }

	/* strip beginning of string (by shifting) */
	y=(int)strlen(buffer);
	for(x=0;x<y;x++){
		if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
			continue;
		else
			break;
	        }
	if(x>0){
		for(z=x;z<y;z++)
			buffer[z-x]=buffer[z];
		buffer[y-x]='\x0';
	        }

	return;
	}



/* get days, hours, minutes, and seconds from a raw time_t format or total seconds */
void get_time_breakdown(unsigned long raw_time,int *days,int *hours,int *minutes,int *seconds){
	unsigned long temp_time;
	int temp_days;
	int temp_hours;
	int temp_minutes;
	int temp_seconds;

	temp_time=raw_time;

	temp_days=temp_time/86400;
	temp_time-=(temp_days * 86400);
	temp_hours=temp_time/3600;
	temp_time-=(temp_hours * 3600);
	temp_minutes=temp_time/60;
	temp_time-=(temp_minutes * 60);
	temp_seconds=(int)temp_time;

	*days=temp_days;
	*hours=temp_hours;
	*minutes=temp_minutes;
	*seconds=temp_seconds;

	return;
	}

