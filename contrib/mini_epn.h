#include <EXTERN.h>
#include <perl.h>

#ifdef aTHX
void xs_init (pTHX);
#else
void xs_init (void);
#endif

#undef ctime	/* don't need perl's threaded version */
#undef printf	/* can't use perl's printf until initialized */

		/* and we don't need perl's reentrant versions */
#undef localtime
#undef getpwnam
#undef getgrnam
#undef strerror



