
#if defined(HAVE_SSL) && HAVE_SSL
int my_ssl_connect(const char *host_name, int port, int *sd, SSL **ssl, SSL_CTX **ctx, int timeout) { return 0; }
int my_ssl_sendall(int sd, SSL *ssl, const char *buf, int *len, int timeout) { return 0; }
int my_ssl_recvall(int s, SSL *ssl, char *buf, int *len, int timeout) { return 0; }
#endif

void get_next_valid_time(time_t pref_time, time_t *valid_time, timeperiod *tperiod) 
{ }

int update_check_stats(int check_type, time_t check_time) 
{ return OK; }

int check_time_against_period(time_t test_time, timeperiod *tperiod) 
{ return OK; }

void free_memory(nagios_macros *mac) 
{ }

char * unescape_check_result_output(const char *rawbuf)
{ return strdup(rawbuf); }
