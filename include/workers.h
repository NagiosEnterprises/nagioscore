#ifndef INCLUDE_workers_h__
#define INCLUDE_workers_h__
#include "lib/libnagios.h"
#include "lib/worker.h"
#include "nagios.h" /* for check_result definition */

/* different jobtypes. We add more as needed */
#define WPJOB_CHECK   0
#define WPJOB_NOTIFY  1
#define WPJOB_OCSP    2
#define WPJOB_OCHP    3
#define WPJOB_GLOBAL_SVC_EVTHANDLER 4
#define WPJOB_SVC_EVTHANDLER  5
#define WPJOB_GLOBAL_HOST_EVTHANDLER 6
#define WPJOB_HOST_EVTHANDLER 7

#define WPROC_FORCE  (1 << 0)

extern unsigned int wproc_num_workers_spawned;
extern unsigned int wproc_num_workers_online;
extern unsigned int wproc_num_workers_desired;

extern void wproc_reap(int jobs, int msecs);
extern int wproc_can_spawn(struct load_control *lc);
extern void free_worker_memory(int flags);
extern int workers_alive(void);
extern int init_workers(int desired_workers);
extern int wproc_run_check(check_result *cr, char *cmd, nagios_macros *mac);
extern int wproc_notify(char *cname, char *hname, char *sdesc, char *cmd, nagios_macros *mac);
extern int wproc_run(int job_type, char *cmd, int timeout, nagios_macros *mac);
extern int wproc_run_service_job(int jtype, int timeout, service *svc, char *cmd, nagios_macros *mac);
extern int wproc_run_host_job(int jtype, int timeout, host *hst, char *cmd, nagios_macros *mac);
extern int wproc_destroy(worker_process *wp, int flags);
#endif
