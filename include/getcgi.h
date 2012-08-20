/******************************************************
 *
 * GETCGI.H -  Nagios CGI Input Routine Include File
 *
 *
 *****************************************************/

#include "lib/lnag-utils.h"
NAGIOS_BEGIN_DECL

char **getcgivars(void);
void free_cgivars(char **);
void unescape_cgi_input(char *);
void sanitize_cgi_input(char **);
unsigned char hex_to_char(char *);

NAGIOS_END_DECL
