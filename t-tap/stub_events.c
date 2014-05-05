/* Stub for base/events.c */
timed_event *schedule_new_event(int event_type, int high_priority, time_t run_time, int recurring, unsigned long event_interval, void *timing_func, int compensate_for_time_change, void *event_data, void *event_args, int event_options) { return NULL; }
void add_event(squeue_t *sq, timed_event *event) {}
void remove_event(squeue_t *sq, timed_event *event) {}
