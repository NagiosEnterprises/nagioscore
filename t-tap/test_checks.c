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
#include "comments.h"
#include "common.h"
#include "statusdata.h"
#include "downtime.h"
#include "macros.h"
#include "nagios.h"
#include "broker.h"
#include "perfdata.h"
#include "tap.h"
#include "test-stubs.c"

/* Test specific functions + variables */
service *svc1=NULL, *svc2=NULL;
host *host1=NULL;
int found_log_rechecking_host_when_service_wobbles=0;
int found_log_run_async_host_check_3x=0;

int c=0;
int update_program_status(int aggregated_dump){ 
	c++;
	/* printf ("# In the update_program_status hook: %d\n", c); */
	
	/* Set this to break out of event_execution_loop */
	if (c>10) {
		sigshutdown=TRUE;
		c=0;
	}
} 
int log_debug_info(int level, int verbosity, const char *fmt, ...){
	va_list ap;
    char *buffer=NULL;

	va_start(ap, fmt);
	/* vprintf( fmt, ap ); */
    vasprintf(&buffer,fmt,ap);
    if(strcmp(buffer,"Service wobbled between non-OK states, so we'll recheck the host state...\n")==0) {
        found_log_rechecking_host_when_service_wobbles++;
    }
    if(strcmp(buffer,"run_async_host_check_3x()\n")==0) {
        found_log_run_async_host_check_3x++;
    }
    free(buffer);
	va_end(ap);
}

void
setup_objects(time_t time) {
    timed_event *new_event=NULL;

    host1=(host *)calloc(1, sizeof(host));
    host1->current_state=HOST_DOWN;
    host1->has_been_checked=TRUE;
    host1->last_check=time;

    /* First service is a normal one */
    svc1=(service *)calloc(1, sizeof(service));
    svc1->host_name=strdup("Host1");
    svc1->host_ptr=host1;
    svc1->description=strdup("Normal service");
    svc1->check_options=0;
    svc1->next_check=time;
    svc1->state_type=SOFT_STATE;
    svc1->current_state=STATE_CRITICAL;
    svc1->retry_interval=1;
    svc1->check_interval=5;
    svc1->last_state_change=0;
    svc1->last_state_change=0;

    /* Second service .... to be configured! */
    svc2=(service *)calloc(1, sizeof(service));
    svc2->host_name=strdup("Host1");
    svc2->description=strdup("To be nudged");
    svc2->check_options=0;
    svc2->next_check=time;
    svc2->state_type=SOFT_STATE;
    svc2->current_state=STATE_OK;
    svc2->retry_interval=1;
    svc2->check_interval=5;

}

int
main (int argc, char **argv)
{
    time_t now=0L;
    check_result *tmp_check_result;


	plan_tests(4);

	time(&now);

    
    /* Test to confirm that if a service is warning, the notified_on_critical is reset */
    tmp_check_result=(check_result *)calloc(1, sizeof(check_result));
    tmp_check_result->host_name=strdup("host1");
    tmp_check_result->service_description=strdup("Normal service");
    tmp_check_result->object_check_type=SERVICE_CHECK;
    tmp_check_result->check_type=SERVICE_CHECK_ACTIVE;
    tmp_check_result->check_options=0;
    tmp_check_result->scheduled_check=TRUE;
    tmp_check_result->reschedule_check=TRUE;
    tmp_check_result->latency=0.666;
    tmp_check_result->start_time.tv_sec=1234567890;
    tmp_check_result->start_time.tv_usec=56565;
    tmp_check_result->finish_time.tv_sec=1234567899;
    tmp_check_result->finish_time.tv_usec=45454;
    tmp_check_result->early_timeout=0;
    tmp_check_result->exited_ok=TRUE;
    tmp_check_result->return_code=1;
    tmp_check_result->output=strdup("Warning - check notified_on_critical reset");

    setup_objects(now);
    svc1->last_state=STATE_CRITICAL;
    svc1->notified_on_critical=TRUE;
    svc1->current_notification_number=999;
    svc1->last_notification=(time_t)11111;
    svc1->next_notification=(time_t)22222;
    svc1->no_more_notifications=TRUE;

    handle_async_service_check_result( svc1, tmp_check_result );

    /* This has been taken out because it is not required
    ok( svc1->notified_on_critical==FALSE, "notified_on_critical reset" );
    */
    ok( svc1->last_notification==(time_t)0, "last notification reset due to state change" );
    ok( svc1->next_notification==(time_t)0, "next notification reset due to state change" );
    ok( svc1->no_more_notifications==FALSE, "no_more_notifications reset due to state change" );
    ok( svc1->current_notification_number==999, "notification number NOT reset" );

	return exit_status ();
}

