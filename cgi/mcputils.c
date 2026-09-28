/*****************************************************************************
 *
 * MCPUTILS.C - Helpers for the MCP server CGI (mcp.cgi)
 *
 * License: GPL v2
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

#include "../include/mcputils.h"


/* ---------------------------------------------------------------- buffer */

void mcp_buf_init(mcp_buf *b) {
	b->s = NULL;
	b->len = 0;
	b->cap = 0;
	}

void mcp_buf_free(mcp_buf *b) {
	free(b->s);
	mcp_buf_init(b);
	}

static void mcp_buf_reserve(mcp_buf *b, size_t extra) {
	size_t need = b->len + extra + 1;
	if(need <= b->cap)
		return;
	size_t cap = b->cap ? b->cap : 256;
	while(cap < need)
		cap *= 2;
	char *s = realloc(b->s, cap);
	if(s == NULL) {
		fprintf(stderr, "mcp: out of memory\n");
		exit(1);
		}
	b->s = s;
	b->cap = cap;
	}

void mcp_buf_addn(mcp_buf *b, const char *s, size_t n) {
	mcp_buf_reserve(b, n);
	memcpy(b->s + b->len, s, n);
	b->len += n;
	b->s[b->len] = '\0';
	}

void mcp_buf_add(mcp_buf *b, const char *s) {
	mcp_buf_addn(b, s, strlen(s));
	}

void mcp_buf_addf(mcp_buf *b, const char *fmt, ...) {
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if(n < 0)
		return;
	mcp_buf_reserve(b, (size_t)n);
	va_start(ap, fmt);
	vsnprintf(b->s + b->len, (size_t)n + 1, fmt, ap);
	va_end(ap);
	b->len += (size_t)n;
	}

void mcp_buf_add_jstr(mcp_buf *b, const char *s) {
	const unsigned char *p;

	mcp_buf_add(b, "\"");
	for(p = (const unsigned char *)(s ? s : ""); *p; p++) {
		switch(*p) {
			case '"': mcp_buf_add(b, "\\\""); break;
			case '\\': mcp_buf_add(b, "\\\\"); break;
			case '\n': mcp_buf_add(b, "\\n"); break;
			case '\r': mcp_buf_add(b, "\\r"); break;
			case '\t': mcp_buf_add(b, "\\t"); break;
			case '\b': mcp_buf_add(b, "\\b"); break;
			case '\f': mcp_buf_add(b, "\\f"); break;
			default:
				if(*p < 0x20)
					mcp_buf_addf(b, "\\u%04x", *p);
				else
					mcp_buf_addn(b, (const char *)p, 1);
			}
		}
	mcp_buf_add(b, "\"");
	}

void mcp_buf_add_urlenc(mcp_buf *b, const char *s) {
	const unsigned char *p;

	for(p = (const unsigned char *)s; *p; p++) {
		if((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')
		        || *p == '-' || *p == '_' || *p == '.' || *p == '~')
			mcp_buf_addn(b, (const char *)p, 1);
		else
			mcp_buf_addf(b, "%%%02X", *p);
		}
	}


/* ---------------------------------------------------------------- JSON values */

mj *mj_new(mj_type type) {
	mj *v = calloc(1, sizeof(mj));
	if(v == NULL) {
		fprintf(stderr, "mcp: out of memory\n");
		exit(1);
		}
	v->type = type;
	return v;
	}

mj *mj_new_str(const char *s) {
	mj *v = mj_new(MJ_STRING);
	v->string = strdup(s ? s : "");
	return v;
	}

mj *mj_new_num(double n) {
	mj *v = mj_new(MJ_NUMBER);
	v->number = n;
	return v;
	}

mj *mj_new_bool(int b) {
	mj *v = mj_new(MJ_BOOL);
	v->boolean = b ? 1 : 0;
	return v;
	}

mj *mj_add(mj *parent, const char *key, mj *child) {
	if(key) {
		free(child->key);
		child->key = strdup(key);
		}
	child->next = NULL;
	if(parent->last)
		parent->last->next = child;
	else
		parent->child = child;
	parent->last = child;
	return child;
	}

mj *mj_clone(const mj *v) {
	mj *c, *e;

	if(v == NULL)
		return mj_new(MJ_NULL);
	c = mj_new(v->type);
	c->boolean = v->boolean;
	c->number = v->number;
	if(v->string)
		c->string = strdup(v->string);
	for(e = v->child; e; e = e->next)
		mj_add(c, e->key, mj_clone(e));
	return c;
	}

void mj_free(mj *v) {
	while(v) {
		mj *next = v->next;
		mj_free(v->child);
		free(v->key);
		free(v->string);
		free(v);
		v = next;
		}
	}

mj *mj_get(const mj *obj, const char *key) {
	mj *e;

	if(obj == NULL || obj->type != MJ_OBJECT)
		return NULL;
	for(e = obj->child; e; e = e->next)
		if(e->key && !strcmp(e->key, key))
			return e;
	return NULL;
	}

const char *mj_get_str(const mj *obj, const char *key) {
	mj *v = mj_get(obj, key);
	return (v && v->type == MJ_STRING) ? v->string : NULL;
	}

int mj_get_num(const mj *obj, const char *key, double *out) {
	mj *v = mj_get(obj, key);
	char *end;

	if(v == NULL)
		return 0;
	if(v->type == MJ_NUMBER) {
		*out = v->number;
		return 1;
		}
	if(v->type == MJ_STRING && *v->string) {
		double d = strtod(v->string, &end);
		if(*end == '\0') {
			*out = d;
			return 1;
			}
		}
	return 0;
	}

int mj_get_bool(const mj *obj, const char *key, int dflt) {
	mj *v = mj_get(obj, key);
	if(v && v->type == MJ_BOOL)
		return v->boolean;
	if(v && v->type == MJ_STRING) {
		if(!strcmp(v->string, "true"))
			return 1;
		if(!strcmp(v->string, "false"))
			return 0;
		}
	return dflt;
	}

size_t mj_count(const mj *v) {
	size_t n = 0;
	mj *e;
	if(v == NULL)
		return 0;
	for(e = v->child; e; e = e->next)
		n++;
	return n;
	}


/* ---------------------------------------------------------------- parser */

typedef struct {
	const char *p;
	const char *end;
	const char *err;
	int depth;
} mj_parser;

#define MJ_MAX_DEPTH 64

static void skip_ws(mj_parser *ps) {
	while(ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r'))
		ps->p++;
	}

static int hexval(char c) {
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
	}

static int read_hex4(mj_parser *ps, unsigned *out) {
	unsigned v = 0;
	int i;
	if(ps->end - ps->p < 4)
		return 0;
	for(i = 0; i < 4; i++) {
		int h = hexval(ps->p[i]);
		if(h < 0)
			return 0;
		v = (v << 4) | (unsigned)h;
		}
	ps->p += 4;
	*out = v;
	return 1;
	}

static void add_utf8(mcp_buf *b, unsigned cp) {
	char u[4];
	if(cp < 0x80) {
		u[0] = (char)cp;
		mcp_buf_addn(b, u, 1);
		}
	else if(cp < 0x800) {
		u[0] = (char)(0xC0 | (cp >> 6));
		u[1] = (char)(0x80 | (cp & 0x3F));
		mcp_buf_addn(b, u, 2);
		}
	else if(cp < 0x10000) {
		u[0] = (char)(0xE0 | (cp >> 12));
		u[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
		u[2] = (char)(0x80 | (cp & 0x3F));
		mcp_buf_addn(b, u, 3);
		}
	else {
		u[0] = (char)(0xF0 | (cp >> 18));
		u[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
		u[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
		u[3] = (char)(0x80 | (cp & 0x3F));
		mcp_buf_addn(b, u, 4);
		}
	}

static char *parse_string(mj_parser *ps) {
	mcp_buf b;

	mcp_buf_init(&b);
	mcp_buf_add(&b, "");
	ps->p++; /* opening quote */
	while(ps->p < ps->end) {
		unsigned char c = (unsigned char)*ps->p++;
		if(c == '"')
			return b.s;
		if(c < 0x20) {
			ps->err = "control character in string";
			break;
			}
		if(c != '\\') {
			mcp_buf_addn(&b, (const char *)&c, 1);
			continue;
			}
		if(ps->p >= ps->end)
			break;
		c = (unsigned char)*ps->p++;
		switch(c) {
			case '"': mcp_buf_add(&b, "\""); break;
			case '\\': mcp_buf_add(&b, "\\"); break;
			case '/': mcp_buf_add(&b, "/"); break;
			case 'b': mcp_buf_add(&b, "\b"); break;
			case 'f': mcp_buf_add(&b, "\f"); break;
			case 'n': mcp_buf_add(&b, "\n"); break;
			case 'r': mcp_buf_add(&b, "\r"); break;
			case 't': mcp_buf_add(&b, "\t"); break;
			case 'u': {
				unsigned cp, lo;
				if(!read_hex4(ps, &cp)) {
					ps->err = "invalid \\u escape";
					goto fail;
					}
				if(cp >= 0xD800 && cp <= 0xDBFF) {
					if(ps->end - ps->p >= 6 && ps->p[0] == '\\' && ps->p[1] == 'u') {
						ps->p += 2;
						if(!read_hex4(ps, &lo) || lo < 0xDC00 || lo > 0xDFFF) {
							ps->err = "invalid surrogate pair";
							goto fail;
							}
						cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
						}
					else {
						ps->err = "invalid surrogate pair";
						goto fail;
						}
					}
				/* NUL would truncate C strings */
				if(cp == 0) {
					ps->err = "NUL character in string";
					goto fail;
					}
				add_utf8(&b, cp);
				break;
				}
			default:
				ps->err = "invalid escape in string";
				goto fail;
			}
		}
	if(!ps->err)
		ps->err = "unterminated string";
fail:
	mcp_buf_free(&b);
	return NULL;
	}

static mj *parse_value(mj_parser *ps);

static mj *parse_container(mj_parser *ps, int is_object) {
	mj *v = mj_new(is_object ? MJ_OBJECT : MJ_ARRAY);
	char close = is_object ? '}' : ']';

	if(++ps->depth > MJ_MAX_DEPTH) {
		ps->err = "nesting too deep";
		mj_free(v);
		return NULL;
		}
	ps->p++;
	skip_ws(ps);
	if(ps->p < ps->end && *ps->p == close) {
		ps->p++;
		ps->depth--;
		return v;
		}
	while(ps->p < ps->end) {
		char *key = NULL;
		mj *child;

		skip_ws(ps);
		if(is_object) {
			if(ps->p >= ps->end || *ps->p != '"') {
				ps->err = "expected member name";
				break;
				}
			if((key = parse_string(ps)) == NULL)
				break;
			skip_ws(ps);
			if(ps->p >= ps->end || *ps->p != ':') {
				ps->err = "expected ':'";
				free(key);
				break;
				}
			ps->p++;
			}
		child = parse_value(ps);
		if(child == NULL) {
			free(key);
			break;
			}
		mj_add(v, key, child);
		free(key);
		skip_ws(ps);
		if(ps->p < ps->end && *ps->p == ',') {
			ps->p++;
			continue;
			}
		if(ps->p < ps->end && *ps->p == close) {
			ps->p++;
			ps->depth--;
			return v;
			}
		ps->err = is_object ? "expected ',' or '}'" : "expected ',' or ']'";
		break;
		}
	if(!ps->err)
		ps->err = "unexpected end of input";
	mj_free(v);
	return NULL;
	}

static int match_word(mj_parser *ps, const char *w) {
	size_t n = strlen(w);
	if((size_t)(ps->end - ps->p) >= n && !strncmp(ps->p, w, n)) {
		ps->p += n;
		return 1;
		}
	return 0;
	}

static mj *parse_value(mj_parser *ps) {
	mj *v;

	skip_ws(ps);
	if(ps->p >= ps->end) {
		ps->err = "unexpected end of input";
		return NULL;
		}
	switch(*ps->p) {
		case '{':
			return parse_container(ps, 1);
		case '[':
			return parse_container(ps, 0);
		case '"': {
			char *s = parse_string(ps);
			if(s == NULL)
				return NULL;
			v = mj_new(MJ_STRING);
			v->string = s;
			return v;
			}
		case 't':
			if(match_word(ps, "true"))
				return mj_new_bool(1);
			break;
		case 'f':
			if(match_word(ps, "false"))
				return mj_new_bool(0);
			break;
		case 'n':
			if(match_word(ps, "null"))
				return mj_new(MJ_NULL);
			break;
		default: {
			const char *start = ps->p;
			char *end;
			char tmp[64];
			size_t n;

			while(ps->p < ps->end && strchr("+-0123456789.eE", *ps->p))
				ps->p++;
			n = (size_t)(ps->p - start);
			if(n == 0 || n >= sizeof(tmp))
				break;
			memcpy(tmp, start, n);
			tmp[n] = '\0';
			v = mj_new(MJ_NUMBER);
			v->number = strtod(tmp, &end);
			if(*end != '\0' || !isfinite(v->number)) {
				mj_free(v);
				break;
				}
			v->string = strdup(tmp);
			return v;
			}
		}
	ps->err = "invalid value";
	return NULL;
	}

mj *mj_parse(const char *text, size_t len, const char **err) {
	mj_parser ps;
	mj *v;

	ps.p = text;
	ps.end = text + len;
	ps.err = NULL;
	ps.depth = 0;
	v = parse_value(&ps);
	if(v) {
		skip_ws(&ps);
		if(ps.p != ps.end) {
			ps.err = "trailing characters after JSON value";
			mj_free(v);
			v = NULL;
			}
		}
	if(err)
		*err = ps.err;
	return v;
	}


/* ---------------------------------------------------------------- serializer */

void mj_write(mcp_buf *b, const mj *v) {
	mj *e;

	if(v == NULL) {
		mcp_buf_add(b, "null");
		return;
		}
	switch(v->type) {
		case MJ_NULL:
			mcp_buf_add(b, "null");
			break;
		case MJ_BOOL:
			mcp_buf_add(b, v->boolean ? "true" : "false");
			break;
		case MJ_NUMBER:
			if(v->string)
				mcp_buf_add(b, v->string);
			else if(v->number == floor(v->number) && fabs(v->number) < 9007199254740992.0)
				mcp_buf_addf(b, "%.0f", v->number);
			else
				mcp_buf_addf(b, "%.17g", v->number);
			break;
		case MJ_STRING:
			mcp_buf_add_jstr(b, v->string);
			break;
		case MJ_ARRAY:
		case MJ_OBJECT:
			mcp_buf_add(b, v->type == MJ_ARRAY ? "[" : "{");
			for(e = v->child; e; e = e->next) {
				if(e != v->child)
					mcp_buf_add(b, ",");
				if(v->type == MJ_OBJECT) {
					mcp_buf_add_jstr(b, e->key);
					mcp_buf_add(b, ":");
					}
				mj_write(b, e);
				}
			mcp_buf_add(b, v->type == MJ_ARRAY ? "]" : "}");
			break;
		}
	}


/* ---------------------------------------------------------------- SHA-256 */

static const uint32_t sha256_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(uint32_t h[8], const unsigned char *blk) {
	uint32_t w[64], a, b, c, d, e, f, g, hh, t1, t2;
	int i;

	for(i = 0; i < 16; i++)
		w[i] = ((uint32_t)blk[i * 4] << 24) | ((uint32_t)blk[i * 4 + 1] << 16)
		       | ((uint32_t)blk[i * 4 + 2] << 8) | (uint32_t)blk[i * 4 + 3];
	for(i = 16; i < 64; i++) {
		uint32_t s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
		uint32_t s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
		}
	a = h[0]; b = h[1]; c = h[2]; d = h[3];
	e = h[4]; f = h[5]; g = h[6]; hh = h[7];
	for(i = 0; i < 64; i++) {
		t1 = hh + (ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25)) + ((e & f) ^ (~e & g)) + sha256_k[i] + w[i];
		t2 = (ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		hh = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
		}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d;
	h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
	}

void mcp_sha256_hex(const unsigned char *data, size_t len, char out[65]) {
	uint32_t h[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};
	unsigned char blk[64];
	uint64_t bits = (uint64_t)len * 8;
	size_t i, rem;

	for(i = 0; i + 64 <= len; i += 64)
		sha256_block(h, data + i);
	rem = len - i;
	memset(blk, 0, sizeof(blk));
	memcpy(blk, data + i, rem);
	blk[rem] = 0x80;
	if(rem >= 56) {
		sha256_block(h, blk);
		memset(blk, 0, sizeof(blk));
		}
	for(i = 0; i < 8; i++)
		blk[63 - i] = (unsigned char)(bits >> (i * 8));
	sha256_block(h, blk);
	for(i = 0; i < 8; i++)
		snprintf(out + i * 8, 9, "%08x", h[i]);
	}


/* ---------------------------------------------------------------- tokens */

int mcp_parse_scopes(const char *s) {
	int scopes = 0;
	char word[16];
	size_t n;

	if(s == NULL)
		return -1;
	while(*s) {
		while(*s == ',' || *s == ' ')
			s++;
		if(!*s)
			break;
		n = strcspn(s, ", ");
		if(n >= sizeof(word))
			return -1;
		memcpy(word, s, n);
		word[n] = '\0';
		s += n;
		if(!strcmp(word, "read"))
			scopes |= MCP_SCOPE_READ;
		else if(!strcmp(word, "write"))
			scopes |= MCP_SCOPE_READ | MCP_SCOPE_WRITE;
		else if(!strcmp(word, "admin"))
			scopes |= MCP_SCOPE_READ | MCP_SCOPE_WRITE | MCP_SCOPE_ADMIN;
		else if(!strcmp(word, "config"))
			scopes |= MCP_SCOPE_READ | MCP_SCOPE_CONFIG;
		else
			return -1;
		}
	return scopes ? scopes : -1;
	}

void mcp_format_scopes(int scopes, char *out, size_t n) {
	snprintf(out, n, "%s%s%s%s",
	         (scopes & MCP_SCOPE_READ) ? "read" : "",
	         (scopes & MCP_SCOPE_WRITE) ? ",write" : "",
	         (scopes & MCP_SCOPE_ADMIN) ? ",admin" : "",
	         (scopes & MCP_SCOPE_CONFIG) ? ",config" : "");
	}

int mcp_valid_user(const char *u) {
	size_t n = 0;

	if(u == NULL || !*u)
		return 0;
	for(; *u; u++, n++) {
		if(n >= 64)
			return 0;
		if(!((*u >= 'a' && *u <= 'z') || (*u >= 'A' && *u <= 'Z') || (*u >= '0' && *u <= '9')
		        || *u == '.' || *u == '_' || *u == '@' || *u == '-'))
			return 0;
		}
	return 1;
	}

int mcp_valid_label(const char *l) {
	size_t n = 0;

	if(l == NULL || !*l)
		return 0;
	for(; *l; l++, n++)
		if(n >= 128 || (unsigned char)*l < 0x20 || *l == 0x7f)
			return 0;
	return 1;
	}

int mcp_token_parse_line(const char *line, mcp_token *t) {
	char scopes[64];
	long long created, expires;
	int consumed = 0;
	const char *p = line;
	size_t n;

	while(*p == ' ' || *p == '\t')
		p++;
	if(*p == '\0' || *p == '\n' || *p == '#')
		return 0;

	memset(t, 0, sizeof(*t));
	if(sscanf(p, "%64s %64s %63s %lld %lld %n", t->hash, t->user, scopes, &created, &expires, &consumed) < 5
	        || consumed == 0)
		return -1;
	if(strlen(t->hash) != 64 || strspn(t->hash, "0123456789abcdef") != 64)
		return -1;
	if(!mcp_valid_user(t->user) || (t->scopes = mcp_parse_scopes(scopes)) < 0)
		return -1;
	t->created = (time_t)created;
	t->expires = (time_t)expires;

	/* the label is the rest of the line */
	p += consumed;
	n = strcspn(p, "\r\n");
	if(n >= sizeof(t->label))
		n = sizeof(t->label) - 1;
	memcpy(t->label, p, n);
	t->label[n] = '\0';
	return 1;
	}

void mcp_token_format_line(const mcp_token *t, mcp_buf *b) {
	char scopes[32];

	mcp_format_scopes(t->scopes, scopes, sizeof(scopes));
	mcp_buf_addf(b, "%s %s %s %lld %lld %s\n", t->hash, t->user, scopes,
	             (long long)t->created, (long long)t->expires, t->label);
	}

int mcp_token_expired(const mcp_token *t, time_t now) {
	return t->expires != 0 && now >= t->expires;
	}


/* ---------------------------------------------------------------- arguments */

/* days since 1970-01-01 for a proleptic Gregorian date (no timegm needed) */
static long long days_from_civil(int y, int m, int d) {
	long long era, yoe, doy, doe;

	y -= m <= 2;
	era = (y >= 0 ? y : y - 399) / 400;
	yoe = y - era * 400;
	doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
	}

int mcp_parse_time(const char *s, time_t now, time_t *out) {
	char *end;
	int Y, M, D, h, mi, sec = 0, n = 0;

	if(s == NULL || !*s)
		return 0;
	if(!strcmp(s, "now")) {
		*out = now;
		return 1;
		}

	/* relative: +2h, -30m, +1d, +45s */
	if(*s == '+' || *s == '-') {
		long long v = strtoll(s + 1, &end, 10);
		long long mult;
		if(end == s + 1 || end[0] == '\0' || end[1] != '\0' || v < 0)
			return 0;
		switch(*end) {
			case 's': mult = 1; break;
			case 'm': mult = 60; break;
			case 'h': mult = 3600; break;
			case 'd': mult = 86400; break;
			case 'w': mult = 604800; break;
			default: return 0;
			}
		*out = now + (time_t)((*s == '-' ? -1 : 1) * v * mult);
		return 1;
		}

	/* unix seconds */
	if(strspn(s, "0123456789") == strlen(s)) {
		long long v = strtoll(s, &end, 10);
		*out = (time_t)v;
		return 1;
		}

	/* ISO 8601 */
	if(sscanf(s, "%4d-%2d-%2dT%2d:%2d%n", &Y, &M, &D, &h, &mi, &n) != 5
	        && sscanf(s, "%4d-%2d-%2d %2d:%2d%n", &Y, &M, &D, &h, &mi, &n) != 5)
		return 0;
	s += n;
	if(*s == ':') {
		int k = 0;
		if(sscanf(s, ":%2d%n", &sec, &k) != 1)
			return 0;
		s += k;
		}
	if(*s == '.') {
		s++;
		while(*s >= '0' && *s <= '9')
			s++;
		}
	if(M < 1 || M > 12 || D < 1 || D > 31 || h > 23 || mi > 59 || sec > 60 || h < 0 || mi < 0 || sec < 0)
		return 0;

	long long t = days_from_civil(Y, M, D) * 86400 + h * 3600 + mi * 60 + sec;

	if(*s == '\0' || !strcmp(s, "Z") || !strcmp(s, "z")) {
		*out = (time_t)t;
		return 1;
		}
	if(*s == '+' || *s == '-') {
		int oh, om = 0, sign = (*s == '-') ? -1 : 1;
		if(sscanf(s + 1, "%2d:%2d", &oh, &om) != 2 && sscanf(s + 1, "%2d%2d", &oh, &om) < 1)
			return 0;
		if(oh > 23 || om > 59)
			return 0;
		*out = (time_t)(t - sign * (oh * 3600 + om * 60));
		return 1;
		}
	return 0;
	}

int mcp_safe_field(const char *s) {
	return s != NULL && *s != '\0' && strpbrk(s, ";\r\n") == NULL;
	}

char *mcp_clean_text(const char *s) {
	char *c = strdup(s ? s : ""), *p;

	if(c == NULL)
		return NULL;
	for(p = c; *p; p++) {
		if(*p == ';')
			*p = ',';
		else if(*p == '\r' || *p == '\n' || *p == '\t')
			*p = ' ';
		}
	return c;
	}
