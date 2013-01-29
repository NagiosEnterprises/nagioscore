#define _GNU_SOURCE 1
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <limits.h>
#include <libgen.h>
#include "nspath.h"

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

#define PCOMP_IGNORE (1 << 0) /* marks negated or ignored components */
#define PCOMP_ALLOC  (1 << 1) /* future used for symlink resolutions */

/* path component struct */
struct pcomp {
	char *str; /* file- or directoryname */
	unsigned int len;   /* length of this component */
	int flags;
};

static inline int path_components(const char *path)
{
	char *slash;
	int comps = 1;
	if (!path)
		return 0;
	for (slash = strchr(path, '/'); slash; slash = strchr(slash + 1, '/'))
		comps++;
	return comps;
}

static char *pcomp_construct(struct pcomp *pcomp, int comps)
{
	int i, plen = 0, offset = 0;
	char *path;

	for (i = 0; i < comps; i++) {
		if(pcomp[i].flags & PCOMP_IGNORE)
			continue;
		plen += pcomp[i].len + 1;
	}

	path = malloc(plen + 2);

	for (i = 0; i < comps; i++) {
		if(pcomp[i].flags & PCOMP_IGNORE)
			continue;
		memcpy(path + offset, pcomp[i].str, pcomp[i].len);
		offset += pcomp[i].len;
		if (i < comps - 1)
			path[offset++] = '/';
	}
	path[offset] = 0;

	return path;
}

/*
 * Converts "foo/bar/.././lala.txt" to "foo/lala.txt".
 * "../../../../../bar/../foo/" becomes "/foo/"
 */
char *nspath_normalize(const char *orig_path)
{
	struct pcomp *pcomp = NULL;
	int comps, i = 0, m, depth = 0, have_slash = 0;
	char *path, *rpath, *p, *slash;

	if (!orig_path || !*orig_path)
		return NULL;

	rpath = strdup(orig_path);
	comps = path_components(rpath);
	pcomp = calloc(comps, sizeof(struct pcomp));
	if (pcomp == NULL)
		return NULL;

	p = pcomp[0].str = rpath;
	for (; p; p = slash, i++) {
		slash = strchr(p, '/');
		if (slash) {
			have_slash = 1;
			*slash = 0;
			slash++;
		}
		pcomp[i].len = strlen(p);
		pcomp[i].str = p;

		if (*p == '.') {
			if (p[1] == 0) {
				/* dot-slash is always just ignored */
				pcomp[i].flags |= PCOMP_IGNORE;
				continue;
			}
			if ((*orig_path == '/' || depth) && p[1] == '.' && p[2] == 0) {
				/* dot-dot-slash negates the previous non-negated component */
				pcomp[i].flags |= PCOMP_IGNORE;
				for(m = 0; depth && m <= i; m++) {
					if (pcomp[i - m].flags & PCOMP_IGNORE) {
						continue;
					}
					pcomp[i - m].flags |= PCOMP_IGNORE;
					depth--;
					break; /* we only remove one level at most */
				}
				continue;
			}
		}
		/* convert multiple slashes to one */
		if (i && !*p) {
			pcomp[i].flags |= PCOMP_IGNORE;
			continue;
		}
		depth++;
	}

	/*
	 * If we back up all the way to the root we need to add a slash
	 * as the first path component.
	 */
	if (have_slash && depth == 1) {
		pcomp[0].flags &= ~(PCOMP_IGNORE);
		pcomp[0].str[0] = 0;
		pcomp[0].len = 0;
	}
	path = pcomp_construct(pcomp, comps);
	free(rpath);
	free(pcomp);
	return path;
}

char *nspath_absolute(const char *rel_path, const char *base)
{
	char cwd[PATH_MAX];
	int len;
	char *path = NULL, *normpath;

	if (*rel_path == '/')
		return nspath_normalize(rel_path);

	if (!base) {
		if (getcwd(cwd, sizeof(cwd)) == NULL)
			return NULL;
		base = cwd;
	}

	len = asprintf(&path, "%s/%s", base, rel_path);
	if (len <= 0) {
		if (path)
			free(path);
		return NULL;
	}

	normpath = nspath_normalize(path);
	free(path);

	return normpath;
}

char *nspath_real(const char *rel_path, const char *base)
{
	char *abspath, *ret;

	if (!(abspath = nspath_absolute(rel_path, base)))
		return NULL;

	ret = realpath(abspath, NULL);
	free(abspath);
	return ret;
}

/* we must take care not to destroy the original buffer here */
char *nspath_absolute_dirname(const char *path, const char *base)
{
	char *buf, *ret;

	if (!(buf = nspath_absolute(path, base)))
		return NULL;

	ret = strdup(dirname(buf));
	free(buf);
	return ret;
}

int nspath_mkdir_p(const char *orig_path, mode_t mode, int options)
{
	char *sep, *path;
	int ret = 0, mkdir_start = 0;

	if (!orig_path) {
		errno = EFAULT;
		return -1;
	}

	sep = path = strdup(orig_path);
	if (!sep)
		return -1;

	for (;;) {
		struct stat st;

		if ((sep = strchr(sep + 1, '/'))) {
			*sep = 0; /* nul-terminate path */
		} else if (options & NSPATH_MKDIR_SKIP_LAST) {
			break;
		}

		/* stat() our way up the tree and start mkdir() on ENOENT */
		if (!mkdir_start && (ret = stat(path, &st)) < 0) {
			if (errno != ENOENT) {
				break;
			}
			mkdir_start = 1;
		}

		if (mkdir_start && (ret = mkdir(path, mode)) < 0) {
			break;
		}

		/* end of path or trailing slash? */
		if (!sep || !sep[1])
			break;
		*sep = '/';
	}

	free(path);
	return ret;
}
