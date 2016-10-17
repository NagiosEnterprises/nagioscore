# ===========================================================================
# SYNOPSIS
#
#   AX_NAGIOS_GET_DISTRIB_TYPE
#
# DESCRIPTION
#
#    This macro determines the O/S distribution of the computer it is run on.
#    $dist_type will be set and will be one of:
#        unknown (could not be determined)
#        freebsd, netbsd, openbsd, dragonfly, etc (The BSDs)
#        suse, rh, debian, gentoo (and possibly their descendants)
#        Other major Linux distributions (and possibly their descendants)
#        The O/S name for the rest
#
# LICENSE
#
#    Copyright (c) 2016 Nagios Core Development Team
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; either version 2 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.
# ===========================================================================

AU_ALIAS([AC_NAGIOS_GET_DISTRIB_TYPE], [AX_NAGIOS_GET_DISTRIB_TYPE])
AC_DEFUN([AX_NAGIOS_GET_DISTRIB_TYPE],
[

AC_SUBST(dist_type)
AC_SUBST(dist_ver)

#
# Get user hints for possible cross-compile
#
	AC_MSG_CHECKING(what the distribution type is )
	AC_ARG_WITH(dist-type, AC_HELP_STRING([--with-dist-type=type],
	[specify distribution type (suse, rh, debian, etc.)]),
		[
			#
			# Run this if --with was specified
			#
			if test "x$withval" = x -o x$withval = xno; then
				dist_type_wanted=yes
			else
				dist_type_wanted=no
				dist_type="$withval"
				dist_ver="unknown"
				AC_MSG_RESULT($dist_type)
			fi
		], [
			#
			# Run this if --with was not specified
			#
			dist_type_wanted=yes
		])

		if test x$dist_type = xno; then
			dist_type_wanted=yes
		elif test x$dist_type = xyes; then
			AC_MSG_ERROR([you must enter a distribution type if '--with-dist-type' is specified])
		fi

		#
		# Determine distribution type if it wasn't supplied
		#
		dist_ver="unknown"

		if test $dist_type_wanted=yes; then
			dist_type="unknown"

			if test "$opsys" != "linux"; then
				dist_type="$opsys"
				AS_CASE([$opsys],
					[bsd],
						dist_type=`uname -s | tr ["[A-Z]" "[a-z]"]`
						dist_ver=`uname -r`,
					[aix],
						dist_ver="`uname -v`.`uname -r`",
					[hp-ux],
						dist_ver=`uname -r | cut -d'.' -f1-3`,
					[solaris],
						dist_ver=`uname -r | cut -d'.' -f2`,
					[*],
						dist_ver=$OSTYPE
				)

			else

				if test -r "/etc/gentoo-release"; then
					dist_type="gentoo"
					dist_ver=`cat /etc/gentoo-release`

				elif test -r "/etc/os-release"; then
					. /etc/os-release
					if test x"$ID_LIKE" != x; then
						dist_type=`echo $ID_LIKE | cut -d' ' -f1 | tr ["[A-Z]" "[a-z]"]`
					elif test x"$ID" = xol; then
						dist_type=rh
					else
						dist_type=`echo $ID | tr ["[A-Z]" "[a-z]"]`
					fi
					if test x"$dist_type" = sles; then
						dist_type=suse
					fi
					if test x"$VERSION_ID" != x; then
						dist_ver=$VERSION_ID
					elif test x"$VERSION" != x; then
						dist_ver=`echo $VERSION | cut -d'.' -f1 | tr -d [:alpha:][:blank:][:punct:]`
					fi

				elif test -r "/etc/redhat-release"; then
					dist_type=rh
					dist_ver=`cat /etc/redhat-release`

				elif test -r "/etc/debian_version"; then
					dist_type="debian"
					if test -r "/etc/lsb-release"; then
						. /etc/lsb-release
						dist_ver=`echo "$DISTRIB_RELEASE"`
					else
						dist_ver=`cat /etc/debian_version`
					fi

				elif test -r "/etc/SuSE-release"; then
					dist_type=suse
					dist_ver=`grep VERSION /etc/SuSE-release`

				fi

			fi

			if test "$dist_ver" != "unknown"; then
				dist_ver=`echo "$dist_ver" | cut -d'.' -f1 | tr -d [:alpha:][:blank:][:punct:]`
			fi
		fi

		AC_MSG_RESULT($dist_type)
])
