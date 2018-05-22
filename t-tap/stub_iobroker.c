int iobroker_get_num_fds(iobroker_set *iobs) 
{ return 1; }

int iobroker_poll(iobroker_set *iobs, int timeout) 
{ return 0; }

const char *iobroker_strerror(int error) 
{ return ""; }

void iobroker_destroy(iobroker_set *iobs, int flags) 
{ }
