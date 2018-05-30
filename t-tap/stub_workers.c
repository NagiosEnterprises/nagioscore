int wproc_run_check(check_result *cr, char *cmd, nagios_macros *mac) 
{ return OK; }

int wproc_can_spawn(struct load_control *lc) 
{ return 1; }

void wproc_reap(int jobs, int msecs) 
{ }

int get_desired_workers(int desired_workers) 
{ return 4; }
