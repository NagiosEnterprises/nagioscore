#ifndef INCLUDE_test_utils_h__
#define INCLUDE_test_utils_h__
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#define TEST_PASS 1
#define TEST_FAIL 0

#define CLR_RESET          "\033[m"
#define CLR_BOLD           "\033[1m"
#define CLR_RED            "\033[31m"
#define CLR_GREEN          "\033[32m"
#define CLR_BROWN          "\033[33m"
#define CLR_YELLOW         "\033[33m\033[1m"
#define CLR_BLUE           "\033[34m"
#define CLR_MAGENTA        "\033[35m"
#define CLR_CYAN           "\033[36m"
#define CLR_BG_RED         "\033[41m"
#define CLR_BRIGHT_RED     "\033[31m\033[1m"
#define CLR_BRIGHT_GREEN   "\033[32m\033[1m"
#define CLR_BRIGHT_BLUE    "\033[34m\033[1m"
#define CLR_BRIGHT_MAGENTA "\033[35m\033[1m"
#define CLR_BRIGHT_CYAN    "\033[36m\033[1m"

extern const char *red, *green, *yellow, *cyan, *reset;
extern unsigned int passed, failed, t_verbose;

#define CHECKPOINT() \
	do { \
		fprintf(stderr, "ALIVE @ %s:%s:%d\n", __FILE__, __func__, __LINE__); \
	} while(0)

#define t_assert(expr) \

extern void t_set_colors(int force);
extern void t_start(const char *fmt, ...)
	__attribute__((__format__(__printf__, 1, 2)));
extern void t_pass(const char *fmt, ...)
	__attribute__((__format__(__printf__, 1, 2)));
extern void t_fail(const char *fmt, ...)
	__attribute__((__format__(__printf__, 1, 2)));
extern void t_diag(const char *fmt, ...)
	__attribute__((__format__(__printf__, 1, 2)));
extern int t_ok(int success, const char *fmt, ...)
	__attribute__((__format__(__printf__, 2, 3)));
#define test t_ok
#define t_req(expr) \
	if (!(expr)) \
		crash("No further testing is possible: " #expr " @%s:%d", __FILE__, __LINE__)
extern int ok_int(int a, int b, const char *name);
extern int ok_uint(unsigned int a, unsigned int b, const char *name);
extern int ok_str(const char *a, const char *b, const char *name);
extern int t_end(void);
extern void t_reset(void);
extern void crash(const char *fmt, ...)
	__attribute__((__format__(__printf__, 1, 2), __noreturn__));
#endif
