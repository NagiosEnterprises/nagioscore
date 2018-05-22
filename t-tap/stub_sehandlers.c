int obsessive_compulsive_host_check_processor(host *hst) 
{ return OK; }

int obsessive_compulsive_service_check_processor(service *svc) 
{ return OK; }


#ifndef TEST_CHECKS_C

int handle_host_event(host *hst) 
{ return OK; }

int handle_service_event(service *svc) 
{ return OK; }

#endif
