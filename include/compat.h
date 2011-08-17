/* compatibility macros, primarily to keep indentation programs
 * from going bananas when indenting everything below
 #ifdef __cplusplus
 'extern "C" {'
 #endif
 * as if it was a real block, which is just godsdamn annoying.
 */
#ifndef _COMPAT_H
#define _COMPAT_H

#ifdef __cplusplus
# define NAGIOS_BEGIN_DECL extern "C" {
# define NAGIOS_END_DECL }
#else
# define NAGIOS_BEGIN_DECL /* nothing */
# define NAGIOS_END_DECL /* more of nothing */
#endif

#endif /* _COMPAT_H */
