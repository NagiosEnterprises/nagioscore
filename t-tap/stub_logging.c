/* Stub for base/logging.c */
int write_to_all_logs(char *buffer, unsigned long data_type) {}
int log_host_event(host *hst) { return OK; }
int log_service_event(service *svc) { return OK; }
void logit(int data_type, int display, const char *fmt, ...) {}
int fix_log_file_owner(uid_t uid, gid_t gid) { return 0; }
int close_log_file(void) { return 0; }
int log_debug_info(int level, int verbosity, const char *fmt, ...) { return OK; }
