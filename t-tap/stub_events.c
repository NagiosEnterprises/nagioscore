/* Stub for base/events.c */
int schedule_new_event(int event_type, int high_priority, time_t run_time, int recurring, unsigned long event_interval, void *timing_func, int compensate_for_time_change, void *event_data, void *event_args, int event_options) {}
void remove_event(timed_event *event, timed_event **event_list, timed_event **event_list_tail) {}
