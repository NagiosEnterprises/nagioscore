/************************************************************************
 *
 * Embedded Perl Header File
 * Last Modified: 01-15-2009
 *
 ************************************************************************/


/******** BEGIN EMBEDDED PERL INTERPRETER DECLARATIONS ********/

#include <EXTERN.h>
#include <perl.h>

#include <fcntl.h>
#undef DEBUG /* epn-compiled Nagios spews just - this has a side effect of potentially disabling debug output on epn systems */
#undef ctime    /* don't need perl's threaded version */
#undef printf   /* can't use perl's printf until initialized */

/* In perl.h (or friends) there is a macro that defines sighandler as Perl_sighandler, so we must #undef it so we can use our sighandler() function */
#undef sighandler

/* and we don't need perl's reentrant versions */
#undef localtime
#undef getpwnam
#undef getgrnam
#undef strerror

#ifdef aTHX
EXTERN_C void xs_init(pTHX);
#else
EXTERN_C void xs_init(void);
#endif

/******** END EMBEDDED PERL INTERPRETER DECLARATIONS ********/
