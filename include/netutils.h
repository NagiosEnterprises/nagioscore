#ifndef NAGIOS_NETUGILS_H_INCLUDED
#define NAGIOS_NETUGILS_H_INCLUDED
#include "common.h"
NAGIOS_BEGIN_DECL
int my_tcp_connect(const char *host_name, int port, int *sd, int timeout);
int my_sendall(int s, const char *buf, int *len, int timeout);
int my_recvall(int s, char *buf, int *len, int timeout);
NAGIOS_END_DECL
#endif

