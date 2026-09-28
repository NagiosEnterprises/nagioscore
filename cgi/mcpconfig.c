/*****************************************************************************
 *
 * MCPCONFIG.C - Surgical editing of Nagios object configuration files
 *
 * License: GPL v2
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/mcpconfig.h"


static void *xrealloc(void *p, size_t n) {
	void *r = realloc(p, n);
	if(r == NULL) {
		fprintf(stderr, "mcp: out of memory\n");
		exit(1);
		}
	return r;
	}

static char *xstrndup(const char *s, size_t n) {
	char *r = xrealloc(NULL, n + 1);
	memcpy(r, s, n);
	r[n] = '\0';
	return r;
	}

static char *xstrdup(const char *s) {
	return xstrndup(s, strlen(s));
	}


/* ---------------------------------------------------------------- lines */

static void push_line(cfg_file *f, char *line) {
	if(f->nlines == f->cap) {
		f->cap = f->cap ? f->cap * 2 : 64;
		f->lines = xrealloc(f->lines, (size_t)f->cap * sizeof(char *));
		}
	f->lines[f->nlines++] = line;
	}

cfg_file *cfg_file_new(const char *path, const char *text) {
	cfg_file *f = xrealloc(NULL, sizeof(cfg_file));
	const char *p = text ? text : "", *nl;

	memset(f, 0, sizeof(*f));
	f->path = xstrdup(path ? path : "");
	f->trailing_newline = 1;
	if(*p == '\0')
		return f;
	while(*p) {
		size_t n;
		nl = strchr(p, '\n');
		n = nl ? (size_t)(nl - p) : strlen(p);
		if(n > 0 && p[n - 1] == '\r') {
			f->crlf = 1;
			n--;
			}
		push_line(f, xstrndup(p, n));
		if(nl == NULL) {
			f->trailing_newline = 0;
			break;
			}
		p = nl + 1;
		}
	return f;
	}

char *cfg_file_text(const cfg_file *f) {
	mcp_buf b;
	int i;

	mcp_buf_init(&b);
	mcp_buf_add(&b, "");
	for(i = 0; i < f->nlines; i++) {
		mcp_buf_add(&b, f->lines[i]);
		if(i < f->nlines - 1 || f->trailing_newline)
			mcp_buf_add(&b, f->crlf ? "\r\n" : "\n");
		}
	return b.s;
	}

cfg_file *cfg_file_clone(const cfg_file *f) {
	char *text = cfg_file_text(f);
	cfg_file *c = cfg_file_new(f->path, text);
	c->trailing_newline = f->trailing_newline;
	c->crlf = f->crlf;
	free(text);
	return c;
	}

void cfg_file_free(cfg_file *f) {
	int i;
	if(f == NULL)
		return;
	for(i = 0; i < f->nlines; i++)
		free(f->lines[i]);
	free(f->lines);
	free(f->path);
	free(f);
	}

void cfg_insert_line(cfg_file *f, int index, const char *text) {
	if(index < 0)
		index = 0;
	if(index > f->nlines)
		index = f->nlines;
	push_line(f, NULL);
	memmove(&f->lines[index + 1], &f->lines[index], (size_t)(f->nlines - 1 - index) * sizeof(char *));
	f->lines[index] = xstrdup(text);
	}

static void delete_lines(cfg_file *f, int first, int last) {
	int i, n = last - first + 1;
	if(first < 0 || last >= f->nlines || n <= 0)
		return;
	for(i = first; i <= last; i++)
		free(f->lines[i]);
	memmove(&f->lines[first], &f->lines[last + 1], (size_t)(f->nlines - last - 1) * sizeof(char *));
	f->nlines -= n;
	}

static void replace_line(cfg_file *f, int index, const char *text) {
	free(f->lines[index]);
	f->lines[index] = xstrdup(text);
	}


/* ---------------------------------------------------------------- parsing */

static void block_free_contents(cfg_block *b);

/* the directive part of a logical line: comment removed, "\;" unescaped,
 * surrounding whitespace trimmed (like xodtemplate_handle_semicolons + strip) */
static char *directive(const char *line) {
	mcp_buf b;
	const char *p;
	char *s, *e;

	mcp_buf_init(&b);
	mcp_buf_add(&b, "");
	for(p = line; *p; p++) {
		if(p[0] == '\\' && p[1] == ';') {
			mcp_buf_add(&b, ";");
			p++;
			continue;
			}
		if(*p == ';')
			break;
		mcp_buf_addn(&b, p, 1);
		}
	s = b.s;
	while(*s == ' ' || *s == '\t')
		s++;
	e = s + strlen(s);
	while(e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
		e--;
	*e = '\0';
	if(*s == '#')
		*s = '\0';
	s = xstrdup(s);
	free(b.s);
	return s;
	}

/*
 * Joins continued lines starting at i and sets *last, like
 * mmap_fgets_multiline(): one trailing backslash continues the line (the
 * next line's leading whitespace is dropped), two trailing backslashes are
 * an escaped, literal backslash.
 */
static char *logical_line(const cfg_file *f, int i, int *last) {
	mcp_buf b;
	int j = i;

	mcp_buf_init(&b);
	mcp_buf_add(&b, "");
	for(;;) {
		const char *l = f->lines[j];
		size_t n;

		if(j > i)
			l += strspn(l, " \t");
		n = strlen(l);
		if(n >= 2 && l[n - 1] == '\\' && l[n - 2] == '\\') {
			mcp_buf_addn(&b, l, n - 1);
			break;
			}
		if(b.len + n >= 2 && n >= 1 && l[n - 1] == '\\' && j + 1 < f->nlines) {
			mcp_buf_addn(&b, l, n - 1);
			j++;
			continue;
			}
		mcp_buf_add(&b, l);
		break;
		}
	*last = j;
	return b.s;
	}

static int starts_define(const char *s) {
	return !strncmp(s, "define", 6) && (s[6] == ' ' || s[6] == '\t' || s[6] == '{');
	}

int cfg_parse(const cfg_file *f, cfg_block **blocks, int *count, const char **err, int *err_line) {
	cfg_block *list = NULL, cur;
	int n = 0, cap = 0, in_def = 0, i, last;

	memset(&cur, 0, sizeof(cur));
	*err = NULL;
	for(i = 0; i < f->nlines; i = last + 1) {
		char *raw = logical_line(f, i, &last);
		char *d = directive(raw);
		free(raw);

		if(*d == '\0') {
			free(d);
			continue;
			}
		if(!in_def) {
			if(starts_define(d)) {
				const char *t = d + 6;
				size_t tl;
				while(*t == ' ' || *t == '\t')
					t++;
				tl = strcspn(t, " \t{");
				if(tl == 0) {
					*err = "no object type after 'define'";
					*err_line = i + 1;
					free(d);
					goto fail;
					}
				memset(&cur, 0, sizeof(cur));
				cur.type = xstrndup(t, tl);
				cur.start = i;
				in_def = 1;
				}
			/* other top level directives (include_file, ...) are ignored */
			free(d);
			continue;
			}

		if(!strcmp(d, "}")) {
			cur.end = i;
			if(n == cap) {
				cap = cap ? cap * 2 : 32;
				list = xrealloc(list, (size_t)cap * sizeof(cfg_block));
				}
			list[n++] = cur;
			memset(&cur, 0, sizeof(cur));
			in_def = 0;
			free(d);
			continue;
			}
		if(starts_define(d)) {
			*err = "unexpected start of object definition (missing '}')";
			*err_line = i + 1;
			free(d);
			goto fail;
			}

		/* attribute: name, whitespace, value */
		{
			size_t nl = strcspn(d, " \t");
			const char *v = d + nl;
			while(*v == ' ' || *v == '\t')
				v++;
			cur.attrs = xrealloc(cur.attrs, (size_t)(cur.nattrs + 1) * sizeof(cfg_attr));
			cur.attrs[cur.nattrs].name = xstrndup(d, nl);
			cur.attrs[cur.nattrs].value = xstrdup(v);
			cur.attrs[cur.nattrs].first_line = i;
			cur.attrs[cur.nattrs].last_line = last;
			cur.nattrs++;
		}
		free(d);
		}
	if(in_def) {
		*err = "object definition is not closed with '}'";
		*err_line = cur.start + 1;
		goto fail;
		}
	*blocks = list;
	*count = n;
	return 0;

fail:
	if(in_def)
		block_free_contents(&cur);
	cfg_blocks_free(list, n);
	*blocks = NULL;
	*count = 0;
	return -1;
	}

static void block_free_contents(cfg_block *b) {
	int j;
	for(j = 0; j < b->nattrs; j++) {
		free(b->attrs[j].name);
		free(b->attrs[j].value);
		}
	free(b->attrs);
	free(b->type);
	}

void cfg_blocks_free(cfg_block *blocks, int count) {
	int i;
	if(blocks == NULL)
		return;
	for(i = 0; i < count; i++)
		block_free_contents(&blocks[i]);
	free(blocks);
	}

const char *cfg_block_get(const cfg_block *b, const char *name) {
	int i;
	/* the last definition of an attribute wins, as in Nagios */
	for(i = b->nattrs - 1; i >= 0; i--)
		if(!strcmp(b->attrs[i].name, name))
			return b->attrs[i].value;
	return NULL;
	}

static const struct {
	const char *type;
	const char *key;
} key_attrs[] = {
	{ "host", "host_name" },
	{ "hostgroup", "hostgroup_name" },
	{ "service", "service_description" },
	{ "servicegroup", "servicegroup_name" },
	{ "contact", "contact_name" },
	{ "contactgroup", "contactgroup_name" },
	{ "timeperiod", "timeperiod_name" },
	{ "command", "command_name" },
};

const char *cfg_key_attr(const char *type) {
	size_t i;
	for(i = 0; type && i < sizeof(key_attrs) / sizeof(key_attrs[0]); i++)
		if(!strcmp(type, key_attrs[i].type))
			return key_attrs[i].key;
	return NULL;
	}

int cfg_type_editable(const char *type) {
	return cfg_key_attr(type) != NULL;
	}

static int str_eq(const char *a, const char *b) {
	return a && b && !strcmp(a, b);
	}

int cfg_block_matches(const cfg_block *b, const char *type, const char *name,
                      const char *host, const char *template_name) {
	const char *key;

	if(!str_eq(b->type, type))
		return 0;
	if(template_name)
		return str_eq(cfg_block_get(b, "name"), template_name);
	if((key = cfg_key_attr(type)) == NULL || !str_eq(cfg_block_get(b, key), name))
		return 0;
	if(!strcmp(type, "service"))
		return str_eq(cfg_block_get(b, "host_name"), host);
	return 1;
	}


/* ---------------------------------------------------------------- validation */

int cfg_valid_attr_name(const char *name) {
	size_t n = 0;
	if(name == NULL || !*name || !strcmp(name, "define"))
		return 0;
	for(; *name; name++, n++) {
		if(n >= 64)
			return 0;
		if(!((*name >= 'a' && *name <= 'z') || (*name >= 'A' && *name <= 'Z')
		        || (*name >= '0' && *name <= '9') || *name == '_'))
			return 0;
		}
	return 1;
	}

int cfg_valid_value(const char *value) {
	size_t n;
	const char *p;

	if(value == NULL || (n = strlen(value)) == 0 || n > 4096)
		return 0;
	/* a trailing backslash would continue onto the next line */
	if(value[n - 1] == '\\' || strpbrk(value, "\r\n"))
		return 0;
	for(p = value; *p; p++)
		if((unsigned char)*p < 0x20 && *p != '\t')
			return 0;
	/* a value that is only "}" would close the definition */
	for(p = value; *p == ' ' || *p == '\t'; p++)
		;
	if(!strcmp(p, "}"))
		return 0;
	return 1;
	}

/* ';' starts a comment in object files; write it escaped */
static void add_escaped(mcp_buf *b, const char *value) {
	for(; *value; value++) {
		if(*value == ';')
			mcp_buf_add(b, "\\;");
		else
			mcp_buf_addn(b, value, 1);
		}
	}


/* ---------------------------------------------------------------- editing */

/* column where the value starts on an attribute line, and the indent */
static int value_column(const char *line, int *indent) {
	const char *p = line;
	while(*p == ' ' || *p == '\t')
		p++;
	if(indent)
		*indent = (int)(p - line);
	while(*p && *p != ' ' && *p != '\t')
		p++;
	while(*p == ' ' || *p == '\t')
		p++;
	return (int)(p - line);
	}

static int find_attr(const cfg_block *b, const char *name) {
	int i;
	for(i = b->nattrs - 1; i >= 0; i--)
		if(!strcmp(b->attrs[i].name, name))
			return i;
	return -1;
	}

/* builds "<indent><name><pad><value>" aligned like the block's first attribute */
static char *format_attr(const cfg_file *f, const cfg_block *b, const char *name, const char *value) {
	mcp_buf out;
	int indent = 4, col = 28, pad;
	char ind[64];

	if(b && b->nattrs > 0) {
		const char *first = f->lines[b->attrs[0].first_line];
		col = value_column(first, &indent);
		if(col <= indent)
			col = indent + 24;
		}
	if(indent >= (int)sizeof(ind))
		indent = (int)sizeof(ind) - 1;
	memcpy(ind, b && b->nattrs > 0 ? f->lines[b->attrs[0].first_line] : "                                                               ", (size_t)indent);
	ind[indent] = '\0';

	mcp_buf_init(&out);
	mcp_buf_add(&out, ind);
	mcp_buf_add(&out, name);
	pad = col - indent - (int)strlen(name);
	if(pad < 1)
		pad = 1;
	while(pad-- > 0)
		mcp_buf_add(&out, " ");
	add_escaped(&out, value);
	return out.s;
	}

void cfg_set_attr(cfg_file *f, const cfg_block *b, const char *name, const char *value) {
	int a = find_attr(b, name);
	char *line;

	if(a >= 0) {
		/* keep the original prefix (indent, name, spacing), replace the value */
		const cfg_attr *at = &b->attrs[a];
		const char *orig = f->lines[at->first_line];
		int col = value_column(orig, NULL);
		mcp_buf nb;

		mcp_buf_init(&nb);
		mcp_buf_addn(&nb, orig, (size_t)col);
		if(col > 0 && orig[col - 1] != ' ' && orig[col - 1] != '\t')
			mcp_buf_add(&nb, " ");
		add_escaped(&nb, value);
		replace_line(f, at->first_line, nb.s);
		mcp_buf_free(&nb);
		if(at->last_line > at->first_line)
			delete_lines(f, at->first_line + 1, at->last_line);
		return;
		}
	line = format_attr(f, b, name, value);
	cfg_insert_line(f, b->end, line);
	free(line);
	}

void cfg_unset_attr(cfg_file *f, const cfg_block *b, const char *name) {
	int i;
	/* remove every definition of the attribute, last first */
	for(i = b->nattrs - 1; i >= 0; i--)
		if(!strcmp(b->attrs[i].name, name))
			delete_lines(f, b->attrs[i].first_line, b->attrs[i].last_line);
	}

static int blank(const char *s) {
	return s[strspn(s, " \t")] == '\0';
	}

void cfg_delete_block(cfg_file *f, const cfg_block *b) {
	int start = b->start;
	delete_lines(f, start, b->end);
	/* avoid leaving two blank lines behind */
	if(start < f->nlines && blank(f->lines[start]) && (start == 0 || blank(f->lines[start - 1])))
		delete_lines(f, start, start);
	}

void cfg_append_block(cfg_file *f, const char *type, const char *const *names,
                      const char *const *values, int n) {
	mcp_buf line;
	int i, width = 20;

	for(i = 0; i < n; i++)
		if((int)strlen(names[i]) + 2 > width)
			width = (int)strlen(names[i]) + 2;

	if(f->nlines > 0 && !blank(f->lines[f->nlines - 1]))
		push_line(f, xstrdup(""));
	mcp_buf_init(&line);
	mcp_buf_addf(&line, "define %s {", type);
	push_line(f, line.s);
	for(i = 0; i < n; i++) {
		int pad = width - (int)strlen(names[i]);
		mcp_buf_init(&line);
		mcp_buf_addf(&line, "    %s", names[i]);
		while(pad-- > 0)
			mcp_buf_add(&line, " ");
		add_escaped(&line, values[i]);
		push_line(f, line.s);
		}
	push_line(f, xstrdup("}"));
	f->trailing_newline = 1;
	}


/* ---------------------------------------------------------------- diff */

typedef struct {
	char **v;
	int n;
} line_list;

static line_list split_lines(const char *text) {
	cfg_file *f = cfg_file_new("", text);
	line_list l;
	l.v = f->lines;
	l.n = f->nlines;
	f->lines = NULL;
	f->nlines = 0;
	cfg_file_free(f);
	return l;
	}

static void free_lines(line_list *l) {
	int i;
	for(i = 0; i < l->n; i++)
		free(l->v[i]);
	free(l->v);
	}

typedef struct {
	char op;                 /* ' ', '-', '+' */
	const char *text;
} edit;

#define DIFF_MAX_CELLS 20000000L

void cfg_unified_diff(mcp_buf *out, const char *label_a, const char *label_b,
                      const char *text_a, const char *text_b, int context) {
	line_list a = split_lines(text_a), b = split_lines(text_b);
	int pre = 0, suf = 0, ma, mb, i, j, ne = 0;
	edit *ed;

	while(pre < a.n && pre < b.n && !strcmp(a.v[pre], b.v[pre]))
		pre++;
	while(suf < a.n - pre && suf < b.n - pre && !strcmp(a.v[a.n - 1 - suf], b.v[b.n - 1 - suf]))
		suf++;
	ma = a.n - pre - suf;
	mb = b.n - pre - suf;
	if(ma == 0 && mb == 0) {
		free_lines(&a);
		free_lines(&b);
		return;
		}

	ed = xrealloc(NULL, (size_t)(a.n + b.n + 1) * sizeof(edit));
	for(i = 0; i < pre; i++) {
		ed[ne].op = ' ';
		ed[ne++].text = a.v[i];
		}

	if((long)(ma + 1) * (mb + 1) <= DIFF_MAX_CELLS) {
		/* LCS of the changed middle part */
		int *lcs = xrealloc(NULL, (size_t)(ma + 1) * (size_t)(mb + 1) * sizeof(int));
#define L(x, y) lcs[(size_t)(x) * (size_t)(mb + 1) + (size_t)(y)]
		for(i = ma; i >= 0; i--)
			for(j = mb; j >= 0; j--) {
				if(i == ma || j == mb)
					L(i, j) = 0;
				else if(!strcmp(a.v[pre + i], b.v[pre + j]))
					L(i, j) = L(i + 1, j + 1) + 1;
				else
					L(i, j) = L(i + 1, j) > L(i, j + 1) ? L(i + 1, j) : L(i, j + 1);
				}
		i = j = 0;
		while(i < ma || j < mb) {
			if(i < ma && j < mb && !strcmp(a.v[pre + i], b.v[pre + j])) {
				ed[ne].op = ' ';
				ed[ne++].text = a.v[pre + i];
				i++;
				j++;
				}
			/* deletions before insertions, as diff(1) does */
			else if(i < ma && (j == mb || L(i + 1, j) >= L(i, j + 1))) {
				ed[ne].op = '-';
				ed[ne++].text = a.v[pre + i];
				i++;
				}
			else {
				ed[ne].op = '+';
				ed[ne++].text = b.v[pre + j];
				j++;
				}
			}
#undef L
		free(lcs);
		}
	else {
		for(i = 0; i < ma; i++) {
			ed[ne].op = '-';
			ed[ne++].text = a.v[pre + i];
			}
		for(j = 0; j < mb; j++) {
			ed[ne].op = '+';
			ed[ne++].text = b.v[pre + j];
			}
		}
	for(i = a.n - suf; i < a.n; i++) {
		ed[ne].op = ' ';
		ed[ne++].text = a.v[i];
		}

	mcp_buf_addf(out, "--- %s\n+++ %s\n", label_a, label_b);

	/* group changes into hunks with context lines */
	i = 0;
	while(i < ne) {
		int start, end, k, la = 0, lb = 0, pos_a = 1, pos_b = 1;

		while(i < ne && ed[i].op == ' ')
			i++;
		if(i >= ne)
			break;
		start = i - context < 0 ? 0 : i - context;
		end = i;
		for(;;) {
			while(end < ne && ed[end].op != ' ')
				end++;
			/* next change within 2*context joins this hunk */
			k = end;
			while(k < ne && ed[k].op == ' ' && k - end < 2 * context)
				k++;
			if(k < ne && ed[k].op != ' ' && k - end <= 2 * context) {
				end = k;
				continue;
				}
			break;
			}
		end = end + context > ne ? ne : end + context;

		for(k = 0; k < start; k++) {
			if(ed[k].op != '+')
				pos_a++;
			if(ed[k].op != '-')
				pos_b++;
			}
		for(k = start; k < end; k++) {
			if(ed[k].op != '+')
				la++;
			if(ed[k].op != '-')
				lb++;
			}
		mcp_buf_addf(out, "@@ -%d,%d +%d,%d @@\n", la ? pos_a : pos_a - 1, la, lb ? pos_b : pos_b - 1, lb);
		for(k = start; k < end; k++)
			mcp_buf_addf(out, "%c%s\n", ed[k].op, ed[k].text);
		i = end;
		}

	free(ed);
	free_lines(&a);
	free_lines(&b);
	}
