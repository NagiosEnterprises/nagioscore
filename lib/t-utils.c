#include "t-utils.h"

const char *cyan = "", *red = "", *green = "", *yellow = "", *reset = "";
uint passed, failed, t_verbose = 0;
static uint t_depth;
static const char *indent_str = "  ";

/* can't be used when a or b has side-effects, but we don't care here */
#define max(a, b) (a > b ? a : b)
#define min(a, b) (a < b ? a : b)
#define delta(a, b) ((max(a, b) - (min(a, b))))


void t_reset(void)
{
	passed = failed = 0;
}

void t_set_colors(int force)
{
	if (force == 1 || isatty(fileno(stdout))) {
		cyan = CLR_CYAN;
		red = CLR_RED;
		yellow = CLR_YELLOW;
		green = CLR_GREEN;
		reset = CLR_RESET;
	}
}

static void t_indent(uint depth)
{
	uint i;
	for (i = 0; i < depth; i++) {
		printf("%s", indent_str);
	}
}

void t_start(const char *fmt, ...)
{
	va_list ap;

	t_indent(t_depth++);
	va_start(ap, fmt);
	printf("%s### ", cyan);
	vfprintf(stdout, fmt, ap);
	printf("%s\n", reset);
	va_end(ap);
}

int t_end(void)
{
	if (t_depth)
		t_depth--;
	if (!t_depth || failed) {
		t_indent(t_depth);
		printf("Test results: %s%u passed%s, %s%u failed%s\n",
			   green, passed, reset, failed ? red : "", failed, failed ? reset : "");
	}

	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

static int t_okv(int success, const char *fmt, va_list ap)
{
	passed += !!success;
	failed += !success;

	if (fmt && (!success || t_verbose || (t_verbose = !!getenv("TEST_VERBOSE")))) {
		t_indent(t_depth);
		if (success) {
			printf("%sPASS%s ", green, reset);
		} else {
			printf("%sFAIL%s ", red, reset);
		}
		vfprintf(stdout, fmt, ap);
		putchar('\n');
	}

	return success;
}

int t_ok(int success, const char *fmt, ...)
{
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		t_okv(success, fmt, ap);
		va_end(ap);
	}
	else
		t_okv(success, NULL, NULL);

	return success;
}

void t_pass(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	t_okv(TEST_PASS, fmt, ap);
	va_end(ap);
}

void t_fail(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	t_okv(TEST_FAIL, fmt, ap);
	va_end(ap);
}

void t_diag(const char *fmt, ...)
{
	if (fmt) {
		va_list ap;
		t_indent(t_depth + 1);
		va_start(ap, fmt);
		vfprintf(stdout, fmt, ap);
		va_end(ap);
		putchar('\n');
	}
}

int ok_int(int a, int b, const char *name)
{
	if (a == b) {
		t_pass("%s", name);
		return TEST_PASS;
	}

	t_fail("%s", name);
	t_diag("%d != %d. delta: %d", a, b, delta(a, b));
	return TEST_FAIL;
}

int ok_uint(uint a, uint b, const char *name)
{
	if (a == b) {
		t_pass("%s", name);
		return TEST_PASS;
	}

	t_fail("%s", name);
	t_diag("%u != %u. delta: %u", a, b, delta(a, b));
	return TEST_FAIL;
}

int ok_str(const char *a, const char *b, const char *name)
{
	if ((!a && !b) || (a && b && !strcmp(a, b))) {
		t_pass("%s", name);
		return TEST_PASS;
	}

	t_fail("%s", name);
	t_diag("'%s' != '%s'", a, b);
	return TEST_FAIL;
}

void __attribute__((__noreturn__)) crash(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);

	exit(1);
}
