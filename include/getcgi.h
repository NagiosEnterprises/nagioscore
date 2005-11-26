/******************************************************
 *
 * GETCGI.H -  Nagios CGI Input Routine Include File
 *
 * Last Modified: 11-25-2005
 *
 *****************************************************/

#ifdef __cplusplus
  extern "C" {
#endif

char **getcgivars(void);
void free_cgivars(char **);
void unescape_cgi_input(char *);
void sanitize_cgi_input(char **);
unsigned char hex_to_char(char *);

#ifdef __cplusplus
  }
#endif
