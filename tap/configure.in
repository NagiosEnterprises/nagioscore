AC_INIT(tap, 1.01)
AC_CONFIG_SRCDIR(src/tap.c)
AM_INIT_AUTOMAKE([foreign])
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_INSTALL

# Checks for libraries
case "$host" in
	*-*-*freebsd4*)
		LDFLAGS="$LDFLAGS -pthread"
		HAVE_LIBPTHREAD=1
		;;
	*)
		AC_CHECK_LIB(pthread, main)
		;;
esac

# Checks for header files
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([pthread.h])

# Checks for  typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([atexit])

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 tests/Makefile
		 tests/diag/Makefile
		 tests/fail/Makefile
		 tests/ok/Makefile
		 tests/ok/ok-hash/Makefile
		 tests/ok/ok-numeric/Makefile
		 tests/ok/ok/Makefile
		 tests/pass/Makefile
		 tests/plan/Makefile
		 tests/plan/no-tests/Makefile
		 tests/plan/no_plan/Makefile
		 tests/plan/not-enough-tests/Makefile
		 tests/plan/sane/Makefile
		 tests/plan/skip_all/Makefile
		 tests/plan/too-many-plans/Makefile
		 tests/plan/too-many-tests/Makefile
		 tests/skip/Makefile
		 tests/todo/Makefile
		])
AC_OUTPUT
