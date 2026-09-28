/*
 * Unit tests for mcpconfig.c: parsing, surgical edits and diffs of Nagios
 * object configuration files. Uses the t-utils helpers from lib/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/t-utils.h"
#include "../include/mcpconfig.h"

static const char *sample =
	"###############################################################################\n"
	"# LOCALHOST.CFG - hand written, keep these comments\n"
	"###############################################################################\n"
	"\n"
	"define host{\n"
	"        use                     linux-server            ; template\n"
	"        host_name               web-01\n"
	"        alias                   Web Server 01\n"
	"        address                 10.0.0.11\n"
	"        }\n"
	"\n"
	"# the database\n"
	"define host {\n"
	"    host_name   db-01\n"
	"    address     10.0.0.21\n"
	"    notes       uses \\; semicolons\n"
	"    check_command check-host-alive!a \\\n"
	"                  continued\n"
	"}\n"
	"\n"
	"define service{\n"
	"        use                     generic-service\n"
	"        host_name               web-01\n"
	"        service_description     HTTP\n"
	"        check_command           check_http\n"
	"        }\n"
	"\n"
	"define host{\n"
	"        name                    linux-server\n"
	"        register                0\n"
	"        }\n";

static cfg_file *load(const char *text) {
	return cfg_file_new("objects/test.cfg", text);
	}

/* finds the block and returns its index, -1 if absent (blocks re-parsed) */
static int find(cfg_file *f, cfg_block **b, int *n, const char *type, const char *name,
                const char *host, const char *tmpl) {
	const char *err;
	int line, i;

	cfg_blocks_free(*b, *n);
	*b = NULL;
	*n = 0;
	if(cfg_parse(f, b, n, &err, &line) != 0)
		return -2;
	for(i = 0; i < *n; i++)
		if(cfg_block_matches(&(*b)[i], type, name, host, tmpl))
			return i;
	return -1;
	}

static void test_parse(void) {
	cfg_file *f = load(sample);
	cfg_block *b = NULL;
	const char *err = NULL;
	int n = 0, line = 0;
	char *text;

	t_start("parsing");
	ok_int(f->nlines, 31, "line count");
	ok_int(cfg_parse(f, &b, &n, &err, &line), 0, "sample parses");
	ok_int(n, 4, "four definitions");
	ok_str(b[0].type, "host", "define host{ without space");
	ok_int(b[0].start, 4, "block start line");
	ok_int(b[0].end, 9, "indented closing brace");
	ok_str(cfg_block_get(&b[0], "use"), "linux-server", "inline comment is stripped");
	ok_str(cfg_block_get(&b[1], "notes"), "uses ; semicolons", "escaped semicolon is unescaped");
	ok_str(cfg_block_get(&b[1], "check_command"), "check-host-alive!a continued", "continued line is joined, its indent dropped");
	ok_int(b[1].attrs[3].last_line, 17, "continued attribute spans two lines");
	test(cfg_block_matches(&b[2], "service", "HTTP", "web-01", NULL), "service matches by host and description");
	test(!cfg_block_matches(&b[2], "service", "HTTP", "db-01", NULL), "service on another host does not match");
	test(cfg_block_matches(&b[3], "host", NULL, NULL, "linux-server"), "template matches by name");
	test(!cfg_block_matches(&b[3], "host", "linux-server", NULL, NULL), "template is not a host named like it");

	text = cfg_file_text(f);
	ok_str(text, sample, "unchanged file round-trips byte for byte");
	free(text);
	cfg_blocks_free(b, n);
	cfg_file_free(f);

	f = load("define host {\n host_name x\n");
	test(cfg_parse(f, &b, &n, &err, &line) != 0 && line == 1, "unclosed definition is reported with its line");
	cfg_file_free(f);
	f = load("define host {\n host_name x\ndefine service {\n}\n");
	test(cfg_parse(f, &b, &n, &err, &line) != 0 && line == 3, "nested define is reported");
	cfg_file_free(f);

	f = load("define command {\n command_name c\n command_line /bin/echo a\\\\\n}\n");
	ok_int(cfg_parse(f, &b, &n, &err, &line), 0, "double backslash at the end parses");
	ok_str(cfg_block_get(&b[0], "command_line"), "/bin/echo a\\", "double backslash is a literal backslash, not a continuation");
	cfg_blocks_free(b, n);
	cfg_file_free(f);

	f = load("define host {\r\n  host_name x\r\n}\r\n");
	test(f->crlf, "CRLF is detected");
	cfg_parse(f, &b, &n, &err, &line);
	ok_str(cfg_block_get(&b[0], "host_name"), "x", "CRLF values have no \\r");
	cfg_blocks_free(b, n);
	text = cfg_file_text(f);
	ok_str(text, "define host {\r\n  host_name x\r\n}\r\n", "CRLF round-trips");
	free(text);
	cfg_file_free(f);
	t_end();
	}

static void test_edit(void) {
	cfg_file *f = load(sample);
	cfg_block *b = NULL;
	int n = 0, i;
	char *text;

	t_start("surgical edits");

	i = find(f, &b, &n, "host", "web-01", NULL, NULL);
	cfg_set_attr(f, &b[i], "address", "10.0.0.99");
	ok_str(f->lines[8], "        address                 10.0.0.99", "existing value replaced, alignment kept");

	i = find(f, &b, &n, "host", "web-01", NULL, NULL);
	cfg_set_attr(f, &b[i], "notes", "rack 4; row B");
	ok_str(f->lines[9], "        notes                   rack 4\\; row B", "new attribute aligned, semicolon escaped");
	ok_str(f->lines[10], "        }", "closing brace stays");

	i = find(f, &b, &n, "host", "web-01", NULL, NULL);
	ok_str(cfg_block_get(&b[i], "notes"), "rack 4; row B", "escaped value parses back");

	i = find(f, &b, &n, "host", "db-01", NULL, NULL);
	cfg_set_attr(f, &b[i], "check_command", "check-host-alive");
	i = find(f, &b, &n, "host", "db-01", NULL, NULL);
	ok_str(cfg_block_get(&b[i], "check_command"), "check-host-alive", "continued attribute replaced");
	ok_int(b[i].end - b[i].start, 5, "its continuation line is gone");

	cfg_unset_attr(f, &b[i], "notes");
	i = find(f, &b, &n, "host", "db-01", NULL, NULL);
	test(cfg_block_get(&b[i], "notes") == NULL, "attribute removed");

	i = find(f, &b, &n, "service", "HTTP", "web-01", NULL);
	cfg_delete_block(f, &b[i]);
	test(find(f, &b, &n, "service", "HTTP", "web-01", NULL) == -1, "block deleted");
	ok_int(n, 3, "other blocks remain");

	text = cfg_file_text(f);
	test(strstr(text, "# LOCALHOST.CFG - hand written, keep these comments") != NULL, "comments survive edits");
	test(strstr(text, "# the database") != NULL, "comments between blocks survive");
	test(strstr(text, "\n\n\n") == NULL, "no double blank lines after delete");
	free(text);

	{
		const char *names[] = { "host_name", "use", "address" };
		const char *values[] = { "web-03", "linux-server", "10.0.0.13" };
		cfg_file *nf = load("");
		cfg_append_block(nf, "host", names, values, 3);
		text = cfg_file_text(nf);
		ok_str(text, "define host {\n    host_name           web-03\n    use                 linux-server\n    address             10.0.0.13\n}\n",
		       "new block in an empty file");
		free(text);
		cfg_append_block(nf, "host", names, values, 1);
		ok_str(nf->lines[5], "", "blank line between appended blocks");
		cfg_file_free(nf);
	}

	cfg_blocks_free(b, n);
	cfg_file_free(f);
	t_end();
	}

static void test_validation(void) {
	t_start("names and values");
	ok_int(cfg_valid_attr_name("host_name"), 1, "plain attribute");
	ok_int(cfg_valid_attr_name("_SNMP_COMMUNITY"), 1, "custom variable");
	ok_int(cfg_valid_attr_name("host name"), 0, "space");
	ok_int(cfg_valid_attr_name("}"), 0, "brace");
	ok_int(cfg_valid_attr_name("define"), 0, "define keyword");
	ok_int(cfg_valid_attr_name(""), 0, "empty");
	ok_int(cfg_valid_value("check_http!-S -p 443"), 1, "normal value");
	ok_int(cfg_valid_value("a;b"), 1, "semicolon is allowed (escaped on write)");
	ok_int(cfg_valid_value("a\nb"), 0, "newline would start a new directive");
	ok_int(cfg_valid_value("abc\\"), 0, "trailing backslash would continue the line");
	ok_int(cfg_valid_value("  }"), 0, "lone brace would close the definition");
	ok_int(cfg_valid_value(""), 0, "empty value");
	ok_str(cfg_key_attr("service"), "service_description", "service key");
	test(cfg_key_attr("hostdependency") == NULL, "dependencies have no key");
	test(!cfg_type_editable("hostescalation"), "escalations are not editable");
	t_end();
	}

static void ok_diff(const char *a, const char *b, const char *expect, const char *name) {
	mcp_buf out;
	mcp_buf_init(&out);
	mcp_buf_add(&out, "");
	cfg_unified_diff(&out, "a", "b", a, b, 1);
	ok_str(out.s, expect, name);
	mcp_buf_free(&out);
	}

static void test_diff(void) {
	t_start("unified diff");
	ok_diff("x\ny\n", "x\ny\n", "", "no changes, no output");
	ok_diff("a\nb\nc\n", "a\nB\nc\n", "--- a\n+++ b\n@@ -1,3 +1,3 @@\n a\n-b\n+B\n c\n", "one changed line");
	ok_diff("", "new\n", "--- a\n+++ b\n@@ -0,0 +1,1 @@\n+new\n", "new file");
	ok_diff("old\n", "", "--- a\n+++ b\n@@ -1,1 +0,0 @@\n-old\n", "deleted content");
	ok_diff("1\n2\n3\n4\n5\n6\n7\n8\n9\n", "1\nX\n3\n4\n5\n6\n7\nY\n9\n",
	        "--- a\n+++ b\n@@ -1,3 +1,3 @@\n 1\n-2\n+X\n 3\n@@ -7,3 +7,3 @@\n 7\n-8\n+Y\n 9\n", "distant changes give two hunks");
	ok_diff("1\n2\n3\n4\n", "1\nX\n3\nY\n", "--- a\n+++ b\n@@ -1,4 +1,4 @@\n 1\n-2\n+X\n 3\n-4\n+Y\n", "close changes share a hunk");
	ok_diff("a\nb\n", "a\nx\nb\n", "--- a\n+++ b\n@@ -1,2 +1,3 @@\n a\n+x\n b\n", "insertion");
	t_end();
	}

int main(int argc, char **argv) {
	t_set_colors(0);
	t_verbose = argc > 1 && !strcmp(argv[1], "-v");
	test_parse();
	test_edit();
	test_validation();
	test_diff();
	return t_end();
	}
