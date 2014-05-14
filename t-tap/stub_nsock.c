/* Stub functions for lib/nsock.c */
static inline int nsock_vprintf(int sd, const char *fmt, va_list ap, int plus)
{
	char buf[4096];
	int len;

	/* -2 to accommodate vsnprintf()'s which don't include nul on overflow */
	len = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
	if (len < 0)
		return len;
	buf[len] = 0;
	return len;
}

int nsock_printf_nul(int sd, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = nsock_vprintf(sd, fmt, ap, 1);
	va_end(ap);
	return ret;
}

int nsock_printf(int sd, const char *fmt, ...) {
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = nsock_vprintf(sd, fmt, ap, 0);
	va_end(ap);
	return ret;
}
