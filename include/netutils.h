#ifndef _NETUTILS_H
#define _NETUTILS_H
#include "compat.h"
NAGIOS_BEGIN_DECL
int my_tcp_connect(char *host_name, int port, int *sd, int timeout);
int my_sendall(int s, char *buf, int *len, int timeout);
int my_recvall(int s, char *buf, int *len, int timeout);
NAGIOS_END_DECL
#endif

