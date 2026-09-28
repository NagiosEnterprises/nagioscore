/*****************************************************************************
 *
 * MCPCONFIG.H - Surgical editing of Nagios object configuration files
 *
 * Used by mcp.cgi to change object definitions in place: only the affected
 * "define" block is touched, comments and formatting elsewhere are kept.
 * Follows the object file syntax of xdata/xodtemplate.c: '#' and ';'
 * start comments ("\;" is a literal semicolon), a trailing backslash
 * continues a line, "}" closes a definition on a line of its own.
 *
 * License: GPL v2
 *
 *****************************************************************************/

#ifndef NAGIOS_MCPCONFIG_H_INCLUDED
#define NAGIOS_MCPCONFIG_H_INCLUDED

#include "mcputils.h"

/* a config file as an editable array of lines (without line endings) */
typedef struct cfg_file {
	char *path;
	char **lines;
	int nlines;
	int cap;
	int trailing_newline;
	int crlf;                /* keep Windows line endings */
} cfg_file;

typedef struct cfg_attr {
	char *name;
	char *value;             /* comment stripped, "\;" unescaped */
	int first_line;
	int last_line;           /* > first_line for continued lines */
} cfg_attr;

typedef struct cfg_block {
	char *type;
	int start;               /* line of "define type {" */
	int end;                 /* line of "}" */
	cfg_attr *attrs;
	int nattrs;
} cfg_block;

cfg_file *cfg_file_new(const char *path, const char *text);
char *cfg_file_text(const cfg_file *f);
cfg_file *cfg_file_clone(const cfg_file *f);
void cfg_file_free(cfg_file *f);

/* parses all define blocks; returns 0 or -1 with *err set (static text) */
int cfg_parse(const cfg_file *f, cfg_block **blocks, int *count, const char **err, int *err_line);
void cfg_blocks_free(cfg_block *blocks, int count);

const char *cfg_block_get(const cfg_block *b, const char *name);

/* the attribute naming an object of this type ("host_name" for hosts,
 * "service_description" for services, ...), NULL if the type has none */
const char *cfg_key_attr(const char *type);
int cfg_type_editable(const char *type);

/*
 * 1 if block b is the object: of type, named name (for services: the
 * service_description, with host = host_name), or when template is set,
 * the template with that "name".
 */
int cfg_block_matches(const cfg_block *b, const char *type, const char *name,
                      const char *host, const char *template_name);

/* validation of what may be written */
int cfg_valid_attr_name(const char *name);
int cfg_valid_value(const char *value);

/* edits; after an edit, blocks parsed earlier are stale */
void cfg_set_attr(cfg_file *f, const cfg_block *b, const char *name, const char *value);
void cfg_unset_attr(cfg_file *f, const cfg_block *b, const char *name);
void cfg_delete_block(cfg_file *f, const cfg_block *b);
/* appends "define type {" with the given attributes (names[i]/values[i]) */
void cfg_append_block(cfg_file *f, const char *type, const char *const *names,
                      const char *const *values, int n);
/* inserts a line at index (0..nlines) */
void cfg_insert_line(cfg_file *f, int index, const char *text);

/* unified diff of two texts, labels as in "--- a" / "+++ b" */
void cfg_unified_diff(mcp_buf *out, const char *label_a, const char *label_b,
                      const char *text_a, const char *text_b, int context);

#endif
