/* Stub for base/logging.c */
int write_to_all_logs(char *buffer, unsigned long data_type) {}
int log_host_event(host *hst) { return OK; }
int log_service_event(service *svc) { return OK; }
void logit(int data_type, int display, const char *fmt, ...) {}
int fix_log_file_owner(uid_t uid, gid_t gid) { return 0; }
int close_log_file(void) { return 0; }
#ifndef TEST_CHECKS_C
int log_debug_info(int level, int verbosity, const char *fmt, ...) {
	va_list ap;
	char *buffer = NULL;
	struct timeval now;
	struct tm tmnow;

	if(!(debug_level == DEBUGL_ALL || (level & debug_level)))
		return OK;

	if(verbosity > debug_verbosity)
		return OK;

	gettimeofday(&now, NULL);
	localtime_r(&(now.tv_sec), &tmnow);
	va_start(ap, fmt);
	vasprintf(&buffer, fmt, ap);
	printf("[%04d-%02d-%02d %02d:%02d:%02d.%06d] %s", tmnow.tm_year+1900,
			tmnow.tm_mon+1, tmnow.tm_mday, tmnow.tm_hour, tmnow.tm_min,
			tmnow.tm_sec, now.tv_usec, buffer);
	free(buffer);
	va_end(ap);
	}
#endif
int rotate_log_file(time_t rotation_time) { return OK; }
