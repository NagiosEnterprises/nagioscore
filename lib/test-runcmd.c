#include "runcmd.c"
#include "t-utils.h"
#include <stdio.h>

#define BUF_SIZE 1024

struct cases {
	char *input;
	char *output;
};

struct cases cases[] = {
	{"te\\st1", "test1"},
	{"te\\\\st2", "te\\st2"},
	{"te\\\\\\st3", "te\\st3"},
	{"te\\\\\\\\st4", "te\\\\st4"},

	{"\"te\\st5\"", "test5"},
	{"\"te\\\\st6\"", "te\\st6"},
	{"\"te\\\\\\st7\"", "te\\st7"},
	{"\"te\\\\\\\\st8\"", "te\\\\st8"},

	{"'te\\st9'", "te\\st9"},
	{"'te\\\\st10'", "te\\\\st10"},
	{"'te\\\\\\st11'", "te\\\\\\st11"},
	{"'te\\\\\\\\st12'", "te\\\\\\\\st12"},

	{"\\'te\\\\st13", "'te\\st13"},
	{"'test14\"'", "test14\""},
	{NULL},
};

struct {
	int ret;
	char *cmd;
} anomaly[] = {
	{ CMD_HAS_REDIR, "cat lala | grep foo" },
	{ 0, "cat lala \\| grep foo" },
	{ CMD_HAS_JOBCONTROL, "foo && bar" },
	{ 0, "foo \\&\\& bar" },
	{ CMD_HAS_JOBCONTROL, "foo & bar" },
	{ 0, "foo \\& bar" },
	{ CMD_HAS_SUBCOMMAND, "foo \"`extcmd`\"" },
	{ 0, "foo \"\\`extcmd\\`\"" },
	{ CMD_HAS_SUBCOMMAND, "foo `extcmd`" },
	{ 0, "foo \\`extcmd\\`" },
	{ CMD_HAS_SUBCOMMAND, "foo \"$(extcmd)\"" },
	{ 0, "foo \\$\\(extcmd\\)" },
	{ CMD_HAS_SUBCOMMAND | CMD_HAS_PAREN, "foo $(extcmd" },
	{ 0, "foo \\$\\(extcmd\\)" },
	{ CMD_HAS_UBDQ, "foo\" bar" },
	{ 0, "foo\\\" bar" },
	{ CMD_HAS_UBSQ, "foo' bar" },
	{ 0, "foo\\' bar" },
	{ CMD_HAS_WILDCARD, "ls -l /tmp/*" },
	{ 0, "ls -l /tmp/\\*" },
	{ CMD_HAS_WILDCARD, "ls -l /dev/tty?" },
	{ 0, "ls -l /dev/tty\\?" },
	{ CMD_HAS_SHVAR, "echo $foo" },
	{ 0, "echo \\$foo" },
	{ 0, NULL},
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
			int ret = runcmd_cmd2strv(anomaly[i].cmd, &out_argc, out_argv);
			ok_int(ret, anomaly[i].ret, anomaly[i].cmd);
		}
	}
	r2 = t_end();
	return r2 ? r2 : ret;
}
