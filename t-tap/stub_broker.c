void broker_downtime_data(int type, int flags, int attr, int downtime_type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id, struct timeval *timestamp) 
{ }

void broker_adaptive_host_data(int type, int flags, int attr, host *hst, int command_type, unsigned long modattr, unsigned long modattrs, struct timeval *timestamp) 
{ }

void broker_adaptive_program_data(int type, int flags, int attr, int command_type, unsigned long modhattr, unsigned long modhattrs, unsigned long modsattr, unsigned long modsattrs, struct timeval *timestamp) 
{ }

void broker_adaptive_service_data(int type, int flags, int attr, service *svc, int command_type, unsigned long modattr, unsigned long modattrs, struct timeval *timestamp) 
{ }

void broker_adaptive_contact_data(int type, int flags, int attr, contact *cntct, int command_type, unsigned long modattr, unsigned long modattrs, unsigned long modhattr, unsigned long modhattrs, unsigned long modsattr, unsigned long modsattrs, struct timeval *timestamp) 
{ }

void broker_external_command(int type, int flags, int attr, int command_type, time_t entry_time, char *command_string, char *command_args, struct timeval *timestamp) 
{ }

void broker_acknowledgement_data(int type, int flags, int attr, int acknowledgement_type, void *data, char *ack_author, char *ack_data, int subtype, int notify_contacts, int persistent_comment, struct timeval *timestamp) 
{ }

int broker_host_check(int type, int flags, int attr, host *hst, int check_type, int state, int state_type, struct timeval start_time, struct timeval end_time, char *cmd, double latency, double exectime, int timeout, int early_timeout, int retcode, char *cmdline, char *output, char *long_output, char *perfdata, struct timeval *timestamp, check_result *cr) 
{ return OK; }

int broker_service_check(int type, int flags, int attr, service *svc, int check_type, struct timeval start_time, struct timeval end_time, char *cmd, double latency, double exectime, int timeout, int early_timeout, int retcode, char *cmdline, struct timeval *timestamp, check_result *cr) 
{ return OK; }

void broker_program_state(int type, int flags, int attr, struct timeval *timestamp) 
{ }

void broker_system_command(int type, int flags, int attr, struct timeval start_time, struct timeval end_time, double exectime, int timeout, int early_timeout, int retcode, char *cmd, char *output, struct timeval *timestamp) 
{ }

void broker_log_data(int type, int flags, int attr, char *data, unsigned long data_type, time_t entry_time, struct timeval *timestamp) 
{ }

void broker_comment_data(int type, int flags, int attr, int comment_type, int entry_type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, int expires, time_t expire_time, unsigned long comment_id, struct timeval *timestamp) 
{ }

void broker_timed_event(int type, int flags, int attr, timed_event *event, struct timeval *timestamp) 
{ }

void broker_flapping_data(int type, int flags, int attr, int flapping_type, void *data, double percent_change, double high_threshold, double low_threshold, struct timeval *timestamp)
{ }
