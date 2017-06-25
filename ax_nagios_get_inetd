# ===========================================================================
# SYNOPSIS
#
#   AX_NAGIOS_GET_INETD
#
# DESCRIPTION
#
#    This macro determines whether inetd or xinetd is being used
#    The argument are:
#        the init type as determined by AX_NAGIOS_GET_INIT
#    $inetd_type will be set and will be one of:
#        unknown (could not be determined)
#        launchd (Mac OS X)
#        smf10   (Solaris)
#        smf11   (Solaris)
#        upstart (Older Debian)
#        xinetd  (Most Linux, BSD)
#        inetd   (The rest)
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

AU_ALIAS([AC_NAGIOS_GET_INETD], [AX_NAGIOS_GET_INETD])
AC_DEFUN([AX_NAGIOS_GET_INETD],
[

AC_SUBST(inetd_type)

#
# Get user hints for possible cross-compile
#
	AC_MSG_CHECKING(what inetd is being used )
	AC_ARG_WITH(inetd_type, AC_HELP_STRING([--with-inetd-type=type],
	[which super-server the system runs (inetd, xinetd, systemd, launchd,
	 smf10, smf11, etc.)]),
		[
			inetd_type_wanted=yes
			#
			# Run this if --with was specified
			#
			if test "x$withval" = x -o x$withval = xno; then
				inetd_type_wanted=yes
			else
				inetd_type_wanted=no
				inetd_type="$withval"
				AC_MSG_RESULT($inetd_type)
			fi
		], [
			#
			# Run this if --with was not specified
			#
			inetd_type_wanted=yes
		])

		if test x$inetd_type = xno; then
			inetd_type_wanted=yes
		elif test x$inetd_type = xyes; then
			AC_MSG_ERROR([you must enter an inetd type if '--with-inetd-type' is specified])
		fi

		#
		# Determine inetd type if it wasn't supplied
		#
		if test $inetd_type_wanted = yes; then

			inetd_disabled=""

			AS_CASE([$dist_type],
				[solaris],
					if test x"$init_type" = "xsmf10" -o x"$init_type" = "xsmf11"; then
						inetd_type="$init_type"
					else
						inetd_type="inetd"
					fi,

				[*bsd*],
					inetd_type=`ps -A -o comm -c | grep inetd`,

				[osx],
					inetd_type=`launchd`,

				[aix|hp-ux],
					inetd_type=`UNIX95= ps -A -o comm | grep inetd | head -1`,

				[*],
					inetd_type=[`ps -C "inetd,xinetd" -o fname | grep -vi COMMAND | head -1`])

			if test x"$inetd_type" = x; then
				if test -f /etc/xinetd.conf -a -d /etc/xinetd.d; then
					inetd_disabled="(Not running)"
					inetd_type=xinetd
				elif test -f /etc/inetd.conf -o -f /usr/sbin/inetd; then
					inetd_type=inetd
					inetd_disabled="(Not running)"
				fi
			fi
			
			if test x"$inetd_type" = x; then
				if test x"$init_type" = "xupstart"; then
					inetd_type="upstart"
				fi
			fi

			if test x"$inetd_type" = x; then
				if test x"$init_type" = "xsystemd"; then
					inetd_type="systemd"
				else
					inetd_type="unknown"
				fi
			fi

			if test -n "$inetd_disabled"; then
				AC_MSG_RESULT($inetd_type $inetd_disabled)
			else
				AC_MSG_RESULT($inetd_type)
			fi
		fi
])
