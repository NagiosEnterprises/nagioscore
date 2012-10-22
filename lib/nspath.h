/**
 * @file nspath.h
 * @brief path handling functions
 *
 * This library handles path normalization and resolution. It's nifty
 * if you want to turn relative paths into absolute ones, or if you
 * want to make insane ones sane, but without chdir()'ing your way
 * around the filesystem.
 *
 * @{
 */

#define _GNU_SOURCE 1
#include "snprintf.h"

/**
 * Normalize a path
 * By "normalize", we mean that we convert dot-slash and dot-dot-slash
 * embedded components into a legible continuous string of characters.
 * Leading and trailing slashes are kept exactly as they are in input,
 * but with sequences of slashes reduced to a single one.
 *
 * "foo/bar/.././lala.txt" becomes "foo/lala.txt"
 * "../../../../bar/../foo/" becomes "/foo/"
 * "////foo////././bar" becomes "/foo/bar"
 * @param orig_path The path to normalize
 * @return A newly allocated string containing the normalized path
 */
extern char *nspath_normalize(const char *orig_path);

/**
 * Make the "base"-relative path "rel_path" absolute.
 * Turns the relative path "rel_path" into an absolute path and
 * resolves it as if we were currently in "base". If "base" is
 * NULL, the current working directory is used. If "base" is not
 * null, it should be an absolute path for the result to make
 * sense.
 *
 * @param rel_path The relative path to convert
 * @param base The base directory (if NULL, we use current working dir)
 * @return A newly allocated string containing the absolute path
 */
extern char *nspath_absolute(const char *rel_path, const char *base);

/**
 * Canonicalize the "base"-relative path "rel_path".
 * errno gets properly set in case of errors.
 * @param rel_path The path to transform
 * @param base The base we should operate relative to
 * @return Newly allocated canonical path on succes, NULL on errors
 */
extern char *nspath_real(const char *rel_path, const char *base);
/* @} */
