#ifndef LIBNAGIOS_lnag_utils_h__
#define LIBNAGIOS_lnag_utils_h__

/**
 * @file lnag-utils.h
 * @brief libnagios helper functions that lack a "real" home.
 *
 * @{
 */

#ifdef __cplusplus
/**
 * C++ compatibility macro that avoids confusing indentation programs
 */
# define NAGIOS_BEGIN_DECL extern "C" {
/**
 * Use at end of header file declarations to obtain C++ compatibility
 * ... without confusing indentation programs
 */
# define NAGIOS_END_DECL }
#else
# define NAGIOS_BEGIN_DECL /* nothing */
# define NAGIOS_END_DECL /* more of nothing */
#endif

#ifndef __GNUC__
/** So we can safely use the gcc extension */
# define __attribute__(x) /* nothing */
#endif

/*
 * These macros are widely used throughout Nagios
 */
#define	OK       0   /**< Indicates successful function call in Nagios */
#define ERROR   -2   /**< Non-successful function call in Nagios */

#ifdef FALSE
#undef FALSE
#endif
#define FALSE 0 /**< Not true */

#ifdef TRUE
#undef TRUE
#endif
#define TRUE (!FALSE) /** Not false */

/** Useful macro to safely avoid double-free memory corruption */
#define my_free(ptr) do { if(ptr) { free(ptr); ptr = NULL; } } while(0)

#ifndef ARRAY_SIZE
/** Useful for iterating over all elements in a static array */
# define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif
#ifndef veclen
/** useful for iterating over all elements in a static array */
# define veclen ARRAY_SIZE
#endif

NAGIOS_BEGIN_DECL

/**
 * Check if a number is a power of 2
 * @param x The number to check
 * @return 1 if the number is a power of 2, 0 if it's not
 */
static inline int lnag_ispof2(unsigned int x)
{
	return x > 1 ? !(x & (x - 1)) : 0;
}

#ifdef __GNUC__
# define lnag_clz(x) __builtin_clz(x)
#else
/**
 * Count leading zeroes
 * @param x The unsigned integer to check
 * @return Number of leading zero bits
 */
static inline int lnag_clz(unsigned int x)
{
	for (i = 0; i < sizeof(x) * 8; i++) {
		if (x >> (i * sizeof(x) * 8) == 1)
			return i;
	}
}
#endif

/**
 * Round up to a power of 2
 * Yes, this is the most cryptic function name in all of Nagios, but I
 * like it, so shush.
 * @param r The number to round up
 * @return r, rounded up to the nearest power of 2.
 */
static inline unsigned int rup2pof2(unsigned int r)
{
	return r < 2 ? 4 : lnag_ispof2(r) ? r : 1 << ((sizeof(r) * 8) - (lnag_clz(r)));
}

NAGIOS_END_DECL

/** @} */
#endif /* NAGIOSINCLUDE_pp_utils_h__ */
