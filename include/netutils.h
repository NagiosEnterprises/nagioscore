#ifndef NAGIOS_NETUGILS_H_INCLUDED
#define NAGIOS_NETUGILS_H_INCLUDED
#include "common.h"

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

#ifdef __cplusplus
/** C++ compatibility macro that avoids confusing indentation programs */
//extern "C" {
#endif

#ifdef HAVE_SSL
int my_ssl_connect(const char *host_name, int port, int *sd, SSL **ssl, SSL_CTX **ctx, int timeout);
int my_ssl_sendall(int sd, SSL *ssl, const char *buf, int *len, int timeout);
int my_ssl_recvall(int s, SSL *ssl, char *buf, int *len, int timeout);
#endif

int my_tcp_connect(const char *host_name, int port, int *sd, int timeout);
int my_sendall(int s, const char *buf, int *len, int timeout);
int my_recvall(int s, char *buf, int *len, int timeout);
#ifdef __cplusplus
//}
#endif

#endif

