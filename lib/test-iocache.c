#include <stdio.h>
#include <stdarg.h>
#include "iocache.c"
#include "t-utils.h"

struct strcode {
	char *str;
	unsigned int len;
};
#define ADDSTR(str) { str, sizeof(str) - 1 }
static int test_delimiter(const char *delim, unsigned int delim_len)
{
	struct strcode sc[] = {
		ADDSTR("Charlie Chaplin"),
		ADDSTR("Madonna Something something"),
		ADDSTR("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nulla turpis augue, laoreet eleifend ultricies et, tincidunt non felis. Suspendisse vitae accumsan dolor. Vivamus posuere venenatis dictum. Integer hendrerit est eget turpis scelerisque porttitor. Donec ullamcorper sodales purus, sed bibendum odio porttitor sit amet. Donec pretium sem ac sapien iaculis feugiat. Quisque commodo consequat quam, ac cursus est sodales euismod. Sed nec massa felis, sit amet varius dui. Morbi fermentum varius tellus, eget tempus felis imperdiet quis. Praesent congue auctor ligula, a tempor ipsum malesuada at. Proin pharetra tempor adipiscing. Aenean egestas tellus vitae arcu sagittis non ultrices turpis cursus."),
		ADDSTR("Emma Blomqvist"),
		ADDSTR("Random message"),
		ADDSTR("Random\0message\0with\0nuls\0embedded"),
		{ NULL, 0, },
	};
	int i;
	iocache *ioc;

	ioc = iocache_create(512 * 1024);
	if (!test(ioc != NULL, "iocache_create must work"))
		crash("can't test with no available memory");

	for (i = 0; sc[i].str; i++) {
		memcpy(&ioc->ioc_buf[ioc->ioc_buflen], sc[i].str, sc[i].len);
		ioc->ioc_buflen += sc[i].len;
		memcpy(ioc->ioc_buf + ioc->ioc_buflen, delim, delim_len);
		ioc->ioc_buflen += delim_len;
	}

	for (i = 0; sc[i].str; i++) {
		char *ptr;
		unsigned long len;
		int error = 0;
		ptr = iocache_use_delim(ioc, delim, delim_len, &len);
		t_req(ptr != NULL);
		if (!ptr) {
			printf("Null pointer. What weird shit is this??\n");
			exit(1);
		}
		test(len == sc[i].len, "len check, string %d, delim_len %d", i, delim_len);
		test(!memcmp(ptr, sc[i].str, len), "memcmp() check, string %d, delim_len %d", i, delim_len);
		if (error) {
			printf("delim_len: %d. i: %d; len: %lu; sc[i].len: %d\n",
				   delim_len, i, len, sc[i].len);
			printf("sc[i].str: %s\n", sc[i].str);
			printf("ptr      : %s\n", ptr);
			printf("strlen(sc[i].str): %lu\n", (unsigned long)strlen(sc[i].str));
			printf("strlen(ptr)      : %lu\n", (unsigned long)strlen(ptr));
			exit(1);
		}
	}
	iocache_destroy(ioc);
	return 0;
}

int main(int argc, char **argv)
{
	unsigned int i;
	struct strcode sc[] = {
		ADDSTR("\n"),
		ADDSTR("\0\0"),
		ADDSTR("XXXxXXX"),
		ADDSTR("LALALALALALALAKALASBALLE\n"),
	};

	t_set_colors(0);
	t_start("iocache_use_delim() test");
	for (i = 0; i < ARRAY_SIZE(sc); i++) {
		t_start("Testing delimiter of len %d", sc[i].len);
		test_delimiter(sc[i].str, sc[i].len);
		t_end();
	}

	return t_end();
}
