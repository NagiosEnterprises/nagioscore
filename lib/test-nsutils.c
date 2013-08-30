#define _GNU_SOURCE
#include <stdio.h>
#include "nsutils.c"
#include "t-utils.h"
#include <sys/time.h>
#include <time.h>

int main(int argc, char **argv)
{
	struct timeval start, stop;
	float f_delta;
	int msec_delta;
	char *s1;
	const char *s2;

	t_set_colors(0);
	t_verbose = 1;
	t_start("tv_delta tests");

	stop.tv_sec = start.tv_sec = time(NULL);
	stop.tv_usec = 2500;
	start.tv_usec = 0;
	msec_delta = tv_delta_msec(&start, &stop);
	t_ok(msec_delta == 2, "tv_delta_msec()");
	f_delta = tv_delta_f(&start, &stop) * 1000;
	t_ok((double)f_delta == (double)2.5, "tv_delta_f() * 1000 is %.2f and should be 2.5", f_delta);
	gettimeofday(&start, NULL);
	memcpy(&stop, &start, sizeof(start));
	stop.tv_sec += 100;

	asprintf(&s1, "arg varg foo %d", 12);
	s2 = mkstr("arg varg foo %d", 12);
	ok_str(s1, s2, "mkstr() must build proper strings");
	if (real_online_cpus() > 0) {
		t_pass("%d online cpus detected", real_online_cpus());
	} else {
		t_fail("Can't determine the number of online cpus");
	}

	return t_end();
}
