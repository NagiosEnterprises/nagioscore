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

int main(int argc, char **argv)
{
	runcmd_init();
	t_set_colors(0);
	t_start("runcmd test");
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
	t_end();
	return 0;
}
