#define _GNU_SOURCE
#include "runcmd.c"
#include "t-utils.h"
#include <stdio.h>

#define BUF_SIZE 1024

struct cases {
	char *input;
	char *output;
};

struct cases cases[] = {
	{"test0\\", "test0"},
	{"te\\st1", "test1"},
	{"te\\\\st2", "te\\st2"},
	{"te\\\\\\st3", "te\\st3"},
	{"te\\\\\\\\st4", "te\\\\st4"},

	{"\"te\\st5\"", "te\\st5"},
	{"\"te\\\\st6\"", "te\\st6"},
	{"\"te\\\\\\st7\"", "te\\\\st7"},
	{"\"te\\\\\\\\st8\"", "te\\\\st8"},

	{"'te\\st9'", "te\\st9"},
	{"'te\\\\st10'", "te\\\\st10"},
	{"'te\\\\\\st11'", "te\\\\\\st11"},
	{"'te\\\\\\\\st12'", "te\\\\\\\\st12"},

	{"\\'te\\\\st13", "'te\\st13"},
	{"'test14\"'", "test14\""},
	{"\"\\\\test\"", "\\test"},
	{NULL},
};

struct {
	int ret;
	char *cmd;
} anomaly[] = {
	{ RUNCMD_HAS_REDIR, "cat lala | grep foo" },
	{ 0, "cat lala \\| grep foo" },
	{ RUNCMD_HAS_JOBCONTROL, "foo && bar" },
	{ 0, "foo \\&\\& bar" },
	{ RUNCMD_HAS_JOBCONTROL, "foo & bar" },
	{ 0, "foo \\& bar" },
	{ RUNCMD_HAS_JOBCONTROL, "lala foo ; bar" },
	{ 0, "lala 'foo; bar'" },
	{ RUNCMD_HAS_SUBCOMMAND, "foo \"`extcmd1`\"" },
	{ 0, "foo \"\\`extcmd1\\`\"" },
	{ RUNCMD_HAS_SUBCOMMAND, "foo `extcmd2`" },
	{ 0, "foo \\`extcmd2\\`" },
	{ RUNCMD_HAS_SUBCOMMAND, "foo \"$(extcmd3)\"" },
	{ 0, "foo \\$\\(extcmd3\\)" },
	{ RUNCMD_HAS_SUBCOMMAND | RUNCMD_HAS_PAREN, "foo $(extcmd4" },
	{ 0, "foo \\$\\(extcmd4\\)" },
	{ RUNCMD_HAS_UBDQ, "foo\" bar" },
	{ 0, "foo\\\" bar" },
	{ RUNCMD_HAS_UBSQ, "foo' bar" },
	{ 0, "foo\\' bar" },
	{ RUNCMD_HAS_WILDCARD, "ls -l /tmp/*" },
	{ 0, "ls -l /tmp/\\*" },
	{ RUNCMD_HAS_WILDCARD, "ls -l /dev/tty?" },
	{ 0, "ls -l /dev/tty\\?" },
	{ RUNCMD_HAS_SHVAR, "echo $foo" },
	{ 0, "echo \\$foo" },
	{ RUNCMD_HAS_PAREN, "\\$(hoopla booyaka" },
	{ 0, "\\$\\(hoopla booyaka" },
	{ 0, NULL},
};

struct {
	int ret;
	char *cmd;
	int argc_exp;
	char *argv_exp[10];
} parse_case[] = {
	{ 0, "foo bar nisse", 3, { "foo", "bar", "nisse", NULL }},
	{ 0, "foo\\ bar nisse", 2, { "foo bar", "nisse", NULL }},
	{ 0, "\"\\\\foo\"", 1, { "\\foo", NULL }},
	{ 0, "\"\\1bs in dq\"", 1, { "\\1bs in dq", NULL }},
	{ 0, "\"\\\\2bs in dq\"", 1, { "\\2bs in dq", NULL }},
	{ 0, "\"\\\\\\3bs in dq\"", 1, { "\\\\3bs in dq", NULL }},
	{ 0, "\"\\\\\\\\4bs in dq\"", 1, { "\\\\4bs in dq", NULL }},
	{ 0, "\\ \t \\\t  \\ ", 3, { " ", "\t", " ", NULL }},
	{ 0, "\\$foo walla wonga", 3, { "$foo", "walla", "wonga", NULL }},
	{ 0, "\"\\$bar is\" very wide open", 4, { "$bar is", "very", "wide", "open", NULL }},
	{ 0, NULL, 0, { NULL, NULL, NULL }},
};

int main(int argc, char **argv)
{
	int ret, r2;

	runcmd_init();
	t_set_colors(0);
	t_start("exec output comparison");
	{
		int i;
		char *out = calloc(1, BUF_SIZE);
		for (i = 0; cases[i].input != NULL; i++) {
			memset(out, 0, BUF_SIZE);
			int pfd[2] = {-1, -1}, pfderr[2] = {-1, -1};
			int fd;
			char *cmd;
			asprintf(&cmd, "/bin/echo -n %s", cases[i].input);
			fd = runcmd_open(cmd, pfd, pfderr, NULL);
			read(pfd[0], out, BUF_SIZE);
			ok_str(cases[i].output, out, "Echoing a command should give expected output");
			close(pfd[0]);
			close(pfderr[0]);
			close(fd);
		}
	}
	ret = t_end();
	t_reset();
	t_start("anomaly detection");
	{
		int i;
		for (i = 0; anomaly[i].cmd; i++) {
			int out_argc;
			char *out_argv[256];
			int result = runcmd_cmd2strv(anomaly[i].cmd, &out_argc, out_argv);
			ok_int(result, anomaly[i].ret, anomaly[i].cmd);
		}
	}
	r2 = t_end();
	ret = r2 ? r2 : ret;
	t_reset();
	t_start("argument splitting");
	{
		int i;
		for (i = 0; parse_case[i].cmd; i++) {
			int x, out_argc;
			char *out_argv[256];
			int result = runcmd_cmd2strv(parse_case[i].cmd, &out_argc, out_argv);
			out_argv[out_argc] = NULL;
			ok_int(result, 0, parse_case[i].cmd);
			ok_int(out_argc, parse_case[i].argc_exp, parse_case[i].cmd);
			for (x = 0; x < parse_case[x].argc_exp && out_argv[x]; x++) {
				ok_str(parse_case[i].argv_exp[x], out_argv[x], "argv comparison test");
			}
		}
	}

	r2 = t_end();
	return r2 ? r2 : ret;
}
