/* Stub for common/statusdata.c */
int update_service_status(service *svc, int aggregated_dump) {}
int update_host_status(host *hst, int aggregated_dump) { return OK; }
#if !(defined(TEST_CHECKS_C) || defined(TEST_EVENTS_C))
int update_program_status(int aggregated_dump) {}
#endif
int update_contact_status(contact *cntct, int aggregated_dump) {}
int update_all_status_data(void) { return OK; }
