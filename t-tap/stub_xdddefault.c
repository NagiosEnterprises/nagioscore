/* Stub file for xdata/xddefault.c */
int xdddefault_add_new_host_downtime(char *host_name, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id) {}
int xdddefault_add_new_service_downtime(char *host_name, char *service_description, time_t entry_time, char *author, char *comment, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id) {}
int xdddefault_cleanup_downtime_data(char *main_config_file) {}
int xdddefault_delete_service_downtime(unsigned long downtime_id) {}
int xdddefault_delete_host_downtime(unsigned long downtime_id) {}
int xdddefault_initialize_downtime_data(char *main_config_file) {}
