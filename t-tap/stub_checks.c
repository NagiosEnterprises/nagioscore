void schedule_host_check(host *hst, time_t check_time, int options) {}
void schedule_service_check(service *svc, time_t check_time, int options) {}
int handle_async_host_check_result(host *temp_host, check_result *queued_check_result) { return OK; }
