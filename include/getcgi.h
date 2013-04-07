/******************************************************
 *
 * GETCGI.H -  Nagios CGI Input Routine Include File
 *
 *
 *****************************************************/

#include "lib/lnag-utils.h"
NAGIOS_BEGIN_DECL

#define ACCEPT_LANGUAGE_Q_DELIMITER	";q="

/* information for a single language in the variable HTTP_ACCEPT_LANGUAGE
	sent by the browser */
typedef struct accept_language_struct {
	char *	language;
	char *	locality;
	double	q;
} accept_language;

/* information for all languages in the variable HTTP_ACCEPT_LANGUAGE
	sent by the browser */
typedef struct accept_languages_struct {
	int					count;
	accept_language **	languages;
} accept_languages;

char **getcgivars(void);
void free_cgivars(char **);
void unescape_cgi_input(char *);
void sanitize_cgi_input(char **);
unsigned char hex_to_char(char *);

void	process_language( char *);
accept_languages *	parse_accept_languages( char *);
int compare_accept_languages( const void *, const void *);
void	free_accept_languages( accept_languages *);

NAGIOS_END_DECL
