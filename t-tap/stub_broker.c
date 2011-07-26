/* Stub file for routines from broker.c */
void broker_downtime_data(int type, int flags, int attr, int downtime_type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id, struct timeval *timestamp) {}
void broker_adaptive_host_data(int type, int flags, int attr, host *hst, int command_type, unsigned long modattr, unsigned long modattrs, struct timeval *timestamp) {}
void broker_adaptive_program_data(int type, int flags, int attr, int command_type, unsigned long modhattr, unsigned long modhattrs, unsigned long modsattr, unsigned long modsattrs, struct timeval *timestamp) {}
void broker_adaptive_service_data(int type, int flags, int attr, service *svc, int command_type, unsigned long modattr, unsigned long modattrs, struct timeval *timestamp) {}
void broker_adaptive_contact_data(int type, int flags, int attr, contact *cntct, int command_type, unsigned long modattr, unsigned long modattrs, unsigned long modhattr, unsigned long modhattrs, unsigned long modsattr, unsigned long modsattrs, struct timeval *timestamp) {}
void broker_external_command(int type, int flags, int attr, int command_type, time_t entry_time, char *command_string, char *command_args, struct timeval *timestamp) {}
void broker_acknowledgement_data(int type, int flags, int attr, int acknowledgement_type, void *data, char *ack_author, char *ack_data, int subtype, int notify_contacts, int persistent_comment, struct timeval *timestamp) {}
