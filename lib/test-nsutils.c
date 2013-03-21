#include "nsutils.c"
#include "t-utils.h"

int main(int argc, char **argv)
{
	struct timeval start, stop;
	float f_delta;
	int msec_delta;

	t_start("tv_delta tests");
	t_verbose = 1;
	t_set_colors(0);

	stop.tv_sec = start.tv_sec = time(NULL);
	stop.tv_usec = 2500;
	start.tv_usec = 0;
	msec_delta = nsu_tv_delta_msec(&start, &stop);
	t_ok(msec_delta == 2, "nsu_tv_delta_msec()");
	f_delta = nsu_tv_delta_f(&start, &stop) * 1000;
	t_ok((double)f_delta == (double)2.5, "nsu_tv_delta_f() * 1000 is %.2f and should be 2.5", f_delta);
	gettimeofday(&start, NULL);
	memcpy(&stop, &start, sizeof(start));
	stop.tv_sec += 100;
	return t_end();
}
