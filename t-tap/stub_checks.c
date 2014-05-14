void schedule_host_check(host *hst, time_t check_time, int options) {}
void schedule_service_check(service *svc, time_t check_time, int options) {}
int handle_async_host_check_result(host *temp_host, check_result *queued_check_result) { return OK; }
int handle_async_service_check_result(service *temp_service, check_result *queued_check_result) { return OK; }
#ifndef TEST_EVENTS_C
int run_scheduled_service_check(service *svc, int check_options, double latency) { return OK; }
#endif
int reap_check_results(void) { return OK; }
void check_for_orphaned_services(void) {}
void check_service_result_freshness(void) {}
int run_scheduled_host_check(host *hst, int check_options, double latency) { return OK; }
void check_host_result_freshness(void) {}
void check_for_orphaned_hosts(void) {}
