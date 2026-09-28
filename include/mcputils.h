/*****************************************************************************
 *
 * MCPUTILS.H - Helpers for the MCP server CGI (mcp.cgi)
 *
 * A small JSON parser/serializer, a growable string buffer and SHA-256,
 * so mcp.cgi has no dependencies beyond the other CGIs.
 *
 * License: GPL v2
 *
 *****************************************************************************/

#ifndef NAGIOS_MCPUTILS_H_INCLUDED
#define NAGIOS_MCPUTILS_H_INCLUDED

#include <stddef.h>

/* ---- string buffer ---- */

typedef struct mcp_buf {
	char *s;
	size_t len;
	size_t cap;
} mcp_buf;

void mcp_buf_init(mcp_buf *b);
void mcp_buf_free(mcp_buf *b);
void mcp_buf_addn(mcp_buf *b, const char *s, size_t n);
void mcp_buf_add(mcp_buf *b, const char *s);
void mcp_buf_addf(mcp_buf *b, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
/* adds s as a quoted, escaped JSON string */
void mcp_buf_add_jstr(mcp_buf *b, const char *s);
/* adds s percent-encoded for use in a query string */
void mcp_buf_add_urlenc(mcp_buf *b, const char *s);

/* ---- JSON values ---- */

typedef enum {
	MJ_NULL,
	MJ_BOOL,
	MJ_NUMBER,
	MJ_STRING,
	MJ_ARRAY,
	MJ_OBJECT
} mj_type;

typedef struct mj {
	mj_type type;
	char *key;               /* member name when inside an object */
	int boolean;
	double number;
	char *string;            /* also holds the literal text of numbers */
	struct mj *child;        /* first element / member */
	struct mj *last;         /* last element / member, for appending */
	struct mj *next;
} mj;

/* parse text; returns NULL and sets *err on failure */
mj *mj_parse(const char *text, size_t len, const char **err);
void mj_free(mj *v);

mj *mj_get(const mj *obj, const char *key);
const char *mj_get_str(const mj *obj, const char *key);
/* returns 1 and sets *out if key holds a number (or numeric string) */
int mj_get_num(const mj *obj, const char *key, double *out);
/* returns dflt when key is missing or not a boolean */
int mj_get_bool(const mj *obj, const char *key, int dflt);
size_t mj_count(const mj *v);

/* constructors; mj_add() appends child to an array or object */
mj *mj_new(mj_type type);
mj *mj_new_str(const char *s);
mj *mj_new_num(double n);
mj *mj_new_bool(int b);
mj *mj_add(mj *parent, const char *key, mj *child);
/* moves child out of its parent list is not supported: use mj_clone */
mj *mj_clone(const mj *v);

/* serializes v compactly */
void mj_write(mcp_buf *b, const mj *v);

/* ---- SHA-256 ---- */

void mcp_sha256_hex(const unsigned char *data, size_t len, char out[65]);

/* ---- tokens ---- */

#include <time.h>

#define MCP_SCOPE_READ   1
#define MCP_SCOPE_WRITE  2
#define MCP_SCOPE_ADMIN  4
#define MCP_SCOPE_CONFIG 8      /* object configuration changes, not implied by admin */

#define MCP_TOKEN_PREFIX "nagmcp_"
#define MCP_TOKEN_ID_LEN 12      /* hash prefix used to refer to a token */

typedef struct mcp_token {
	char hash[65];           /* sha256 of the token, lowercase hex */
	char user[65];           /* Nagios user the token acts as */
	int scopes;              /* MCP_SCOPE_* bits */
	time_t created;
	time_t expires;          /* 0 = never */
	char label[129];
} mcp_token;

/* "read", "write", "admin", "config" separated by commas or spaces; write
 * implies read, admin implies write, config implies read (but not write or
 * admin). Returns the bits or -1 on an unknown scope. */
int mcp_parse_scopes(const char *s);
/* writes e.g. "read,write" */
void mcp_format_scopes(int scopes, char *out, size_t n);

/* token file line: <hash> <user> <scopes> <created> <expires> <label...>
 * returns 1 on success, 0 for blank/comment lines, -1 if malformed */
int mcp_token_parse_line(const char *line, mcp_token *t);
void mcp_token_format_line(const mcp_token *t, mcp_buf *b);
int mcp_token_expired(const mcp_token *t, time_t now);

/* user names: 1-64 of [A-Za-z0-9._@-] */
int mcp_valid_user(const char *u);
/* labels: 1-128 printable characters, no control characters */
int mcp_valid_label(const char *l);

/* ---- arguments ---- */

/* Accepts unix seconds, "now", relative "+2h"/"-30m"/"+1d"/"+45s",
 * or ISO 8601 "YYYY-MM-DDTHH:MM[:SS]" with "Z" or "+HH:MM"/"-HHMM"
 * (no zone = UTC). Returns 1 on success. */
int mcp_parse_time(const char *s, time_t now, time_t *out);

/* 1 if s may be used as a ';' separated external command field */
int mcp_safe_field(const char *s);
/* copies s into a new string with ';' replaced by ',' and line breaks
 * replaced by spaces, for free text such as comments */
char *mcp_clean_text(const char *s);

#endif
