int my_tcp_connect(const char *host_name, int port, int *sd, int timeout) 
{ return OK; }

int my_sendall(int s, const char *buf, int *len, int timeout) 
{ return OK; }

int my_recvall(int s, char *buf, int *len, int timeout) 
{ return OK; }

#ifdef HAVE_SSL
#include <openssl/ssl.h>

int my_ssl_connect(const char *host_name, int port, int *sd, SSL **ssl, SSL_CTX **ctx, int timeout)
{ return OK; }

int my_ssl_sendall(int sd, SSL *ssl, const char *buf, int *len, int timeout)
{ return OK; }

int my_ssl_recvall(int s, SSL *ssl, char *buf, int *len, int timeout)
{ return OK; }
#endif
