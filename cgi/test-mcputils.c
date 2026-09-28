/*
 * Unit tests for mcputils.c (JSON, SHA-256, tokens, argument helpers).
 * Uses the small t-utils test helpers from lib/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../lib/t-utils.h"
#include "../include/mcputils.h"


/* parses text and serializes it again; returns a malloc'd string or NULL */
static char *roundtrip(const char *text) {
	const char *err = NULL;
	mj *v = mj_parse(text, strlen(text), &err);
	mcp_buf b;

	if(v == NULL)
		return NULL;
	mcp_buf_init(&b);
	mj_write(&b, v);
	mj_free(v);
	return b.s;
	}

static int parse_fails(const char *text) {
	const char *err = NULL;
	mj *v = mj_parse(text, strlen(text), &err);
	if(v) {
		mj_free(v);
		return 0;
		}
	return err != NULL;
	}

static void ok_roundtrip(const char *in, const char *expect) {
	char *out = roundtrip(in);
	ok_str(out ? out : "(parse error)", expect, in);
	free(out);
	}

static void test_json_parse(void) {
	t_start("JSON parser and serializer");

	ok_roundtrip("{}", "{}");
	ok_roundtrip("[]", "[]");
	ok_roundtrip("  { \"a\" : 1 , \"b\" : [ true, false, null ] }  ", "{\"a\":1,\"b\":[true,false,null]}");
	ok_roundtrip("{\"n\":-12.5e3}", "{\"n\":-12.5e3}");
	ok_roundtrip("\"tab\\tquote\\\"slash\\/\"", "\"tab\\tquote\\\"slash/\"");
	/* \u escapes become UTF-8, surrogate pairs included */
	ok_roundtrip("\"\\u00e4\\u20ac\\ud83d\\ude00\"", "\"\xc3\xa4\xe2\x82\xac\xf0\x9f\x98\x80\"");
	/* control characters are escaped on output */
	ok_roundtrip("\"a\\u0001b\"", "\"a\\u0001b\"");
	ok_roundtrip("{\"nested\":{\"deeper\":[1,[2,[3]]]}}", "{\"nested\":{\"deeper\":[1,[2,[3]]]}}");

	test(parse_fails(""), "empty input is rejected");
	test(parse_fails("{"), "unterminated object is rejected");
	test(parse_fails("{\"a\" 1}"), "missing colon is rejected");
	test(parse_fails("[1,]"), "trailing comma is rejected");
	test(parse_fails("{\"a\":1} x"), "trailing garbage is rejected");
	test(parse_fails("\"abc"), "unterminated string is rejected");
	test(parse_fails("\"a\nb\""), "raw newline in string is rejected");
	test(parse_fails("\"\\x\""), "unknown escape is rejected");
	test(parse_fails("\"\\ud83d\""), "lone surrogate is rejected");
	test(parse_fails("\"\\u0000\""), "NUL character is rejected");
	test(parse_fails("tru"), "truncated literal is rejected");
	test(parse_fails("1e999"), "infinite number is rejected");

	/* nesting limit protects the stack */
	{
		char deep[200];
		memset(deep, '[', 100);
		memset(deep + 100, ']', 100);
		char *copy = strndup(deep, 200);
		const char *err = NULL;
		mj *v = mj_parse(copy, 200, &err);
		test(v == NULL && err != NULL, "nesting deeper than the limit is rejected");
		mj_free(v);
		free(copy);
	}

	t_end();
	}

static void test_json_access(void) {
	const char *text = "{\"s\":\"x\",\"n\":42,\"ns\":\"7\",\"b\":true,\"bs\":\"false\",\"arr\":[1,2,3]}";
	const char *err = NULL;
	mj *v = mj_parse(text, strlen(text), &err);
	double d = 0;

	t_start("JSON accessors and constructors");
	t_req(v != NULL);

	ok_str(mj_get_str(v, "s"), "x", "string member");
	test(mj_get_str(v, "n") == NULL, "number is not returned as string");
	test(mj_get_num(v, "n", &d) && d == 42, "number member");
	test(mj_get_num(v, "ns", &d) && d == 7, "numeric string is accepted as number");
	test(!mj_get_num(v, "s", &d), "non-numeric string is not a number");
	ok_int(mj_get_bool(v, "b", 0), 1, "boolean member");
	ok_int(mj_get_bool(v, "bs", 1), 0, "\"false\" string is accepted as boolean");
	ok_int(mj_get_bool(v, "missing", 1), 1, "missing boolean returns default");
	ok_uint((unsigned)mj_count(mj_get(v, "arr")), 3, "array length");
	test(mj_get(v, "missing") == NULL, "missing member is NULL");

	{
		mj *o = mj_new(MJ_OBJECT), *c;
		mcp_buf b;

		mj_add(o, "name", mj_new_str("web-01"));
		mj_add(o, "count", mj_new_num(3));
		mj_add(o, "big", mj_new_num(1790541656000.0));
		mj_add(o, "ratio", mj_new_num(0.5));
		mj_add(o, "ok", mj_new_bool(1));
		c = mj_clone(o);
		mcp_buf_init(&b);
		mj_write(&b, c);
		ok_str(b.s, "{\"name\":\"web-01\",\"count\":3,\"big\":1790541656000,\"ratio\":0.5,\"ok\":true}",
		       "constructed object serializes, clone is deep");
		mcp_buf_free(&b);
		mj_free(o);
		mj_free(c);
	}

	mj_free(v);
	t_end();
	}

static void test_buffer(void) {
	mcp_buf b;
	int i;

	t_start("string buffer");
	mcp_buf_init(&b);
	for(i = 0; i < 1000; i++)
		mcp_buf_add(&b, "abcdefghij");
	ok_uint((unsigned)b.len, 10000, "buffer grows across many appends");
	mcp_buf_free(&b);

	mcp_buf_init(&b);
	mcp_buf_addf(&b, "%s=%d", "x", 5);
	ok_str(b.s, "x=5", "formatted append");
	mcp_buf_free(&b);

	mcp_buf_init(&b);
	mcp_buf_add_urlenc(&b, "Disk /var & more;ä");
	ok_str(b.s, "Disk%20%2Fvar%20%26%20more%3B%C3%A4", "URL encoding");
	mcp_buf_free(&b);

	mcp_buf_init(&b);
	mcp_buf_add_jstr(&b, NULL);
	ok_str(b.s, "\"\"", "NULL string serializes as empty string");
	mcp_buf_free(&b);
	t_end();
	}

static void test_sha256(void) {
	char hex[65];
	char *million;

	t_start("SHA-256");
	mcp_sha256_hex((const unsigned char *)"", 0, hex);
	ok_str(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "empty string");
	mcp_sha256_hex((const unsigned char *)"abc", 3, hex);
	ok_str(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "\"abc\"");
	mcp_sha256_hex((const unsigned char *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, hex);
	ok_str(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "56 bytes (padding needs an extra block)");
	million = malloc(1000000);
	t_req(million != NULL);
	memset(million, 'a', 1000000);
	mcp_sha256_hex((const unsigned char *)million, 1000000, hex);
	ok_str(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "one million 'a'");
	free(million);
	t_end();
	}

static void test_scopes(void) {
	char out[32];

	t_start("token scopes");
	ok_int(mcp_parse_scopes("read"), MCP_SCOPE_READ, "read");
	ok_int(mcp_parse_scopes("write"), MCP_SCOPE_READ | MCP_SCOPE_WRITE, "write implies read");
	ok_int(mcp_parse_scopes("admin"), MCP_SCOPE_READ | MCP_SCOPE_WRITE | MCP_SCOPE_ADMIN, "admin implies write and read");
	ok_int(mcp_parse_scopes("read, write"), MCP_SCOPE_READ | MCP_SCOPE_WRITE, "list with comma and space");
	ok_int(mcp_parse_scopes("config"), MCP_SCOPE_READ | MCP_SCOPE_CONFIG, "config implies read only");
	ok_int(mcp_parse_scopes("admin") & MCP_SCOPE_CONFIG, 0, "admin does not imply config");
	ok_int(mcp_parse_scopes("admin,config"), MCP_SCOPE_READ | MCP_SCOPE_WRITE | MCP_SCOPE_ADMIN | MCP_SCOPE_CONFIG, "admin and config together");
	ok_int(mcp_parse_scopes("root"), -1, "unknown scope is rejected");
	ok_int(mcp_parse_scopes(""), -1, "empty scope list is rejected");
	ok_int(mcp_parse_scopes(NULL), -1, "NULL is rejected");
	mcp_format_scopes(MCP_SCOPE_READ | MCP_SCOPE_WRITE, out, sizeof(out));
	ok_str(out, "read,write", "formatting");
	mcp_format_scopes(mcp_parse_scopes("config"), out, sizeof(out));
	ok_str(out, "read,config", "config formats and parses back");
	ok_int(mcp_parse_scopes(out), MCP_SCOPE_READ | MCP_SCOPE_CONFIG, "round trip");
	t_end();
	}

static void test_token_lines(void) {
	mcp_token t;
	mcp_buf b;
	const char *hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
	char line[512];

	t_start("token file lines");
	ok_int(mcp_token_parse_line("", &t), 0, "blank line is skipped");
	ok_int(mcp_token_parse_line("   # comment", &t), 0, "comment is skipped");

	snprintf(line, sizeof(line), "%s nagiosadmin read,write 1790000000 0 Claude on my laptop\n", hash);
	ok_int(mcp_token_parse_line(line, &t), 1, "valid line parses");
	ok_str(t.hash, hash, "hash");
	ok_str(t.user, "nagiosadmin", "user");
	ok_int(t.scopes, MCP_SCOPE_READ | MCP_SCOPE_WRITE, "scopes");
	ok_str(t.label, "Claude on my laptop", "label keeps spaces, drops newline");
	test(t.created == 1790000000 && t.expires == 0, "timestamps");
	test(!mcp_token_expired(&t, 2000000000), "expires=0 never expires");

	mcp_buf_init(&b);
	mcp_token_format_line(&t, &b);
	ok_str(b.s, line, "format is the inverse of parse");
	mcp_buf_free(&b);

	snprintf(line, sizeof(line), "%s ops read 1 100 x", hash);
	ok_int(mcp_token_parse_line(line, &t), 1, "short label");
	test(mcp_token_expired(&t, 100) && !mcp_token_expired(&t, 99), "expiry boundary");

	ok_int(mcp_token_parse_line("abc nagiosadmin read 1 0 x", &t), -1, "short hash is rejected");
	snprintf(line, sizeof(line), "%s bad;user read 1 0 x", hash);
	ok_int(mcp_token_parse_line(line, &t), -1, "invalid user is rejected");
	snprintf(line, sizeof(line), "%s ops superuser 1 0 x", hash);
	ok_int(mcp_token_parse_line(line, &t), -1, "invalid scope is rejected");
	snprintf(line, sizeof(line), "%s ops read", hash);
	ok_int(mcp_token_parse_line(line, &t), -1, "missing fields are rejected");
	t_end();
	}

static void test_validation(void) {
	t_start("user, label and field validation");
	ok_int(mcp_valid_user("nagiosadmin"), 1, "plain user");
	ok_int(mcp_valid_user("jane.doe@example.com"), 1, "email style user");
	ok_int(mcp_valid_user(""), 0, "empty user");
	ok_int(mcp_valid_user("a b"), 0, "space in user");
	ok_int(mcp_valid_user("a;b"), 0, "semicolon in user");
	ok_int(mcp_valid_user("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), 0, "65 characters");
	ok_int(mcp_valid_label("Claude Desktop (Stephan)"), 1, "label with spaces");
	ok_int(mcp_valid_label("a\nb"), 0, "newline in label");
	ok_int(mcp_valid_label(""), 0, "empty label");

	ok_int(mcp_safe_field("web-01"), 1, "plain field");
	ok_int(mcp_safe_field("Disk /var"), 1, "field with space and slash");
	ok_int(mcp_safe_field("a;b"), 0, "semicolon would inject a field");
	ok_int(mcp_safe_field("a\nDISABLE_NOTIFICATIONS"), 0, "newline would inject a command");
	ok_int(mcp_safe_field(""), 0, "empty field");
	ok_int(mcp_safe_field(NULL), 0, "NULL field");
	{
		char *c = mcp_clean_text("on it; see ticket\n#42");
		ok_str(c, "on it, see ticket #42", "comment text is cleaned");
		free(c);
	}
	t_end();
	}

static void test_time(void) {
	time_t now = 1790000000, t = 0;

	t_start("time parsing");
	test(mcp_parse_time("now", now, &t) && t == now, "now");
	test(mcp_parse_time("+2h", now, &t) && t == now + 7200, "+2h");
	test(mcp_parse_time("-30m", now, &t) && t == now - 1800, "-30m");
	test(mcp_parse_time("+1d", now, &t) && t == now + 86400, "+1d");
	test(mcp_parse_time("+1w", now, &t) && t == now + 604800, "+1w");
	test(mcp_parse_time("1790000123", now, &t) && t == 1790000123, "unix seconds");
	test(mcp_parse_time("2026-09-27T20:00:00Z", now, &t) && t == 1790539200, "ISO with Z");
	test(mcp_parse_time("2026-09-27T22:00:00+02:00", now, &t) && t == 1790539200, "ISO with +02:00");
	test(mcp_parse_time("2026-09-27T15:00-0500", now, &t) && t == 1790539200, "ISO with -0500, no seconds");
	test(mcp_parse_time("2026-09-27 20:00", now, &t) && t == 1790539200, "space separator, no zone = UTC");
	test(mcp_parse_time("2026-09-27T20:00:00.123Z", now, &t) && t == 1790539200, "fractional seconds");
	test(mcp_parse_time("2000-02-29T00:00:00Z", now, &t) && t == 951782400, "leap day");
	test(mcp_parse_time("1969-12-31T23:59:59Z", now, &t) && t == -1, "before the epoch");
	test(!mcp_parse_time("", now, &t), "empty");
	test(!mcp_parse_time("tomorrow", now, &t), "words");
	test(!mcp_parse_time("+2x", now, &t), "unknown unit");
	test(!mcp_parse_time("+h", now, &t), "missing amount");
	test(!mcp_parse_time("2026-13-01T00:00Z", now, &t), "month 13");
	test(!mcp_parse_time("2026-09-27T20:00+25:00", now, &t), "bad offset");
	test(!mcp_parse_time("2026-09-27T20:00junk", now, &t), "trailing junk");
	t_end();
	}

int main(int argc, char **argv) {
	t_set_colors(0);
	t_verbose = argc > 1 && !strcmp(argv[1], "-v");

	test_json_parse();
	test_json_access();
	test_buffer();
	test_sha256();
	test_scopes();
	test_token_lines();
	test_validation();
	test_time();

	return t_end();
	}
