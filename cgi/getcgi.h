/******************************************************
 *
 * GETCGI.H -  Nagios CGI Input Routine Include File
 *
 * Last Modified: 07-28-2002
 *
 *****************************************************/

char **getcgivars(void);
void free_cgivars(char **);
void unescape_cgi_input(char *);
void sanitize_cgi_input(char **);
unsigned char hex_to_char(char *);
