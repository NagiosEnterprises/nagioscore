/* Stub file for routines from macros.c */
char *macro_user[MAX_USER_MACROS];

int init_macros(void) { return OK; }
int grab_host_macros_r(nagios_macros *mac, host *hst) { return OK; }
int grab_service_macros_r(nagios_macros *mac, service *svc) { return OK; }
int process_macros_r(nagios_macros *mac, char *input_buffer, char **output_buffer, int options) { return OK; }
int clear_volatile_macros_r(nagios_macros *mac) { return OK; }
int clear_host_macros_r(nagios_macros *mac) { return OK; }
int free_macrox_names(void) { return OK; }
nagios_macros *get_global_macros(void) { return NULL; }
int clear_argv_macros_r(nagios_macros *mac) { return OK; }
int set_all_macro_environment_vars_r(nagios_macros *mac, int set) { return OK; }
