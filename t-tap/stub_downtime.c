unsigned long next_downtime_id = 0;

int schedule_downtime(int type, char *host_name, char *service_description, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *new_downtime_id) 
{ return OK; }

int unschedule_downtime(int type, unsigned long downtime_id) 
{ return OK; }

int check_pending_flex_host_downtime(host *hst) 
{ return OK; }

int check_pending_flex_service_downtime(service *svc) 
{ return OK; }

int handle_scheduled_downtime_by_id(unsigned long downtime_id) 
{ return OK; }

int check_for_expired_downtime(void) 
{ return OK; }

int cleanup_downtime_data(void) 
{ return OK; }

int initialize_downtime_data(void) 
{ return OK; }

void free_downtime_data(void) 
{ }

int add_new_downtime(int type, char *host_name, char *service_description, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id, int is_in_effect, int start_notification_sent)
{ return OK; }

int add_new_host_downtime(char *host_name, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id, int is_in_effect, int start_notification_sent)
{ return OK; }

int add_new_service_downtime(char *host_name, char *service_description, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long *downtime_id, int is_in_effect, int start_notification_sent)
{ return OK; }

int delete_host_downtime(unsigned long downtime_id) 
{ return OK; }

int delete_service_downtime(unsigned long downtime_id) 
{ return OK; }

int delete_downtime(int id, unsigned long downtime_id) 
{ return OK; }

int register_downtime(int id, unsigned long downtime_id) 
{ return OK; }

int handle_scheduled_downtime(struct scheduled_downtime *sd) 
{ return OK; }

int is_host_in_pending_flex_downtime(struct host *hst) 
{ return OK; }

int is_service_in_pending_flex_downtime(struct service *svc) 
{ return OK; }

int add_host_downtime(char *host_name, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t flex_downtime_start, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id, int is_in_effect, int start_notification_sent)
{ return OK; }

int add_service_downtime(char *host_name, char *svc_description, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t flex_downtime_start, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id, int is_in_effect, int start_notification_sent)
{ return OK; }

int add_downtime(int downtime_type, char *host_name, char *svc_description, time_t entry_time, char *author, char *comment_data, time_t start_time, time_t flex_downtime_start, time_t end_time, int fixed, unsigned long triggered_by, unsigned long duration, unsigned long downtime_id, int is_in_effect, int start_notification_sent)
{ return OK; }

int sort_downtime(void) 
{ return OK; }

struct scheduled_downtime *find_downtime(int id, unsigned long downtime_id) 
{ return NULL; }

struct scheduled_downtime *find_host_downtime(unsigned long downtime_id) 
{ return NULL; }

struct scheduled_downtime *find_service_downtime(unsigned long downtime_id) 
{ return NULL; }
