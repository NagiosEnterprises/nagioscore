/******************************************************
 *
 * GETCGI.H -  Nagios CGI Input Routine Include File
 *
 * Last Modified: 11-17-2001
 *
 *****************************************************/

char **getcgivars(void);
void unescape_cgi_input(char *);
void sanitize_cgi_input(char **);
unsigned char hex_to_char(char *);
