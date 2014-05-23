/* Stub for base/notifications.c */
int service_notification(service *svc, int type, char *not_author, char *not_data, int options) {}
int host_notification(host *hst, int type, char *not_author, char *not_data, int options) {}
time_t get_next_host_notification_time(host *temp_host, time_t time_t1) { return (time_t)0; }
time_t get_next_service_notification_time(service *temp_service, time_t time_t1) { return (time_t)0; }
