#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

#include "../lib/snprintf.h"
#include "../lib/nsock.h"
#include "../lib/worker.h"

void usage(char *progname)
{
	printf("Usage: %s [--name <worker_name>] <nagsock_path>\n", progname);
}

int main(int argc, char *argv[])
{
	int sd, i, ret;
	char *path = NULL, *name = NULL, *cmd = NULL, response[128];

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	for (i = 1; i < argc; i++) {
		char *opt, *arg = argv[i];
		if (*arg != '-') {
			if (!path) {
				path = arg;
				continue;
			}
			usage(argv[0]);
		}
		if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
			usage(argv[0]);
			return 0;
		}

		if ((opt = strchr(arg, '='))) {
			opt = '\0';
			opt++;
		}
		else if (i < argc - 1) {
			opt = argv[i + 1];
			i++;
		}
		else {
			usage(argv[0]);
			return 1;
		}

		if (!strcmp(arg, "--name")) {
			name = opt;
		}
	}

	if (!path)
		usage(argv[0]);

	if (!name)
		name = basename(argv[0]);

	sd = nsock_unix(path, NSOCK_TCP | NSOCK_CONNECT);
	if (sd < 0) {
		printf("Couldn't open socket: %s\n", nsock_strerror(sd));
		return 1;
	}

	nsock_printf_nul(sd, "@wproc register name=%s\n", name);

	/*
	 * we read only 3 bytes here, as we don't want to read any
	 * commands from Nagios before we've entered the worker loop
	 */
	ret = read(sd, response, 3);
	if (ret <= 0) {
		printf("Failed to read response: %s\n", strerror(errno));
		return 1;
	}
	if (ret == 3 && !memcmp(response, "OK", 3))
		printf("Connected. Wahoo\n");
	else {
		ret = read(sd, response + 3, sizeof(response) - 3);
		printf("Error: %s\n", response);
		return 1;
	}

	enter_worker(sd, start_cmd);
	return 0;
}
