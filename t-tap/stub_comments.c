int delete_service_comment(unsigned long comment_id) 
{ return OK; }

int delete_host_comment(unsigned long comment_id) 
{ return OK; }

int add_new_comment(int type, int entry_type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, int expires, time_t expire_time, unsigned long *comment_id)
{ return OK; }

int delete_service_acknowledgement_comments(service *svc) 
{ return OK; }

int delete_host_acknowledgement_comments(host *hst) 
{ return OK; }

int add_new_service_comment(int entry_type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, int expires, time_t expire_time, unsigned long *comment_id) 
{ return OK; }

int add_new_host_comment(int entry_type, char *host_name, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, int expires, time_t expire_time, unsigned long *comment_id) 
{ return OK; }

int delete_all_comments(int type, char *host_name, char *svc_description) 
{ return OK; }

void free_comment_data(void) 
{ }

int check_for_expired_comment(unsigned long comment_id) 
{ return OK; }
