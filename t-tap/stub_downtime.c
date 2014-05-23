/* Stub for common/downtime.c */
unsigned long next_downtime_id = 0;
int schedule_downtime(int type, char *host_name, char *service_description, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *new_downtime_id) {}
int unschedule_downtime(int type, unsigned long downtime_id) {}
int check_pending_flex_host_downtime(host *hst) {}
int check_pending_flex_service_downtime(service *svc) { return OK; }
int handle_scheduled_downtime_by_id(unsigned long downtime_id) { return OK; }
int check_for_expired_downtime(void) { return OK; }
