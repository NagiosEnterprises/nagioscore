/* Stub file for common/objects.c */
service *find_service(const char *host_name, const char *svc_desc) { return NULL; }
host *find_host(const char *name) { return NULL; }
hostgroup *find_hostgroup(const char *name) { return NULL; }
contactgroup *find_contactgroup(const char *name) { return NULL; }
servicegroup *find_servicegroup(const char *name) { return NULL; }
contact *find_contact(const char *name) { return NULL; }
command *find_command(const char *name) { return NULL; }
timeperiod *find_timeperiod(const char *name) { return NULL; }
int prepend_object_to_objectlist(objectlist **list, void *object_ptr) { return OK; }
int free_objectlist(objectlist **temp_list) { return OK; }
int free_object_data(void) { return OK; }
