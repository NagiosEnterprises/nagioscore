#ifndef NAGIOS_CONFIG_PWD_H_INCLUDED
#define NAGIOS_CONFIG_PWD_H_INCLUDED
/**
 * @file include/config_pwd.h
 * A wrapper header to let us conditionally define and then undefine
 * __XOPEN_OR_POSIX to suppress the conflicting 'struct comment' declaration on
 * Solaris. We can't do this in config.h.in since the undef will be commented
 * out when config.h is generated.
 */

#if defined(__sun) && defined(__SVR4) && !defined(__XOPEN_OR_POSIX)
#define __XOPEN_OR_POSIX
#define NEED_TO_UNDEF__XOPEN_OR_POSIX
#endif

#include <pwd.h>

#if defined(NEED_TO_UNDEF__XOPEN_OR_POSIX)
#undef __XOPEN_OR_POSIX
#undef NEED_TO_UNDEF__XOPEN_OR_POSIX
#endif

#endif
