/******************************************************
 *
 * GETCGI.H -  Nagios CGI Input Routine Include File
 *
 * Last Modified: 11-25-2005
 *
 *****************************************************/

#include "compat.h"
NAGIOS_BEGIN_DECL

char **getcgivars(void);
void free_cgivars(char **);
void unescape_cgi_input(char *);
void sanitize_cgi_input(char **);
unsigned char hex_to_char(char *);

NAGIOS_END_DECL
