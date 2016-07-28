# ===========================================================================
# SYNOPSIS
#
#   AX_NAGIOS_GET_INIT
#
# DESCRIPTION
#
#    This macro determines the O/S distribution of the computer it is run on.
#    $init_type will be set and will be one of:
#        unknown (could not be determined)
#        launchd (Mac OS X)
#        bsd     (Slackware Linux)
#        newbsd  (FreeBSD, OpenBSD, NetBSD, Dragonfly, etc)
#        smf10   (Solaris)
#        smf11   (Solaris)
#        systemd (Linux SystemD)
#        gentoo  (Older Gentoo)
#        openrc  (Recent Gentoo and some others)
#        upstart (Older Debian)
#        sysv    (The rest)
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

AU_ALIAS([AC_NAGIOS_GET_INIT], [AX_NAGIOS_GET_INIT])
AC_DEFUN([AX_NAGIOS_GET_INIT],
[

AC_SUBST(init_type)

#
# Get user hints for possible cross-compile
#
	AC_MSG_CHECKING(what init system is being used )
	AC_ARG_WITH(init_type,AC_HELP_STRING([--with-init-type=type],
	 [specify init type (bsd, sysv, systemd, launchd, smf10, smf11, upstart,
		openrc, etc.)]),
		[
			#
			# Run this if --with was specified
			#
			if test "x$withval" = x -o x$withval = xno; then
					init_type_wanted=yes
			else
					init_type_wanted=no
					init_type="$withval"
					AC_MSG_RESULT($init_type)
			fi
		], [
			#
			# Run this if --with was not specified
			#
			init_type_wanted=yes
		])

		if test x$init_type = xno; then
			init_type_wanted=yes
		elif test x$init_type = xyes; then
			AC_MSG_ERROR([you must enter an init type if '--with-init-type' is specified])
		fi

		#
		# Determine init type if it wasn't supplied
		#
		if test $init_type_wanted = yes; then
			init_type=""

			if test x"$opsys" = x; then
				init_type="unknown"
				init_type_wanted=no
			elif test x"$dist_type" = x; then
				init_type="unknown"
				init_type_wanted=no
			elif test "$opsys" = "osx"; then
				init_type="launchd"
				init_type_wanted=no
			elif test "$opsys" = "bsd"; then
				init_type="newbsd"
				init_type_wanted=no
			elif test "$dist_type" = "solaris"; then
				if test -d "/lib/svc/manifest"; then
					init_type="smf11"
					init_type_wanted=no
				elif test -d "/lib/svc/monitor"; then
					init_type="smf10"
					init_type_wanted=no
				else
					init_type="sysv"
					init_type_wanted=no
				fi
			elif test "$dist_type" = "slackware"; then
				init_type="bsd"
				init_type_wanted=no
			elif test "$dist_type" = "aix"; then
				init_type="bsd"
				init_type_wanted=no
			elif test "$dist_type" = "hp-ux"; then
				init_type="unknown"
				init_type_wanted=no
			fi
		fi

		PSCMD="ps -p1 -o args"
		if test $dist_type = solaris; then
			PSCMD="env UNIX95=1; ps -p1 -o args"
		fi

		if test "$init_type_wanted" = yes; then
			pid1=`$PSCMD | grep -vi COMMAND | cut -d' ' -f1`
			if test x"$pid1" = "x"; then
				init_type="unknown"
				init_type_wanted=no
			fi
			if `echo $pid1 | grep "systemd" > /dev/null`; then
				init_type="systemd"
				init_type_wanted=no
			fi

			if test "$init_type_wanted" = yes; then
				if test "$pid1" = "init"; then
					if test -e "/sbin/init"; then
						pid1="/sbin/init";
					elif test -e "/usr/sbin/init"; then
						pid1="/usr/sbin/init"
					else
						init_type="unknown"
						init_type_wanted=no
					fi
				fi
				if test -L "$pid1"; then
					pid1=`readlink "$pid1"`
				fi
			fi

			if test "$init_type_wanted" = yes; then
				if `echo $pid1 | grep "systemd" > /dev/null`; then
					init_type="systemd"
					init_type_wanted=no
				elif test -f "/sbin/rc"; then
					if test -f /sbin/runscript; then
						init_type_wanted=no
						if `/sbin/start-stop-daemon -V | grep "OpenRC" > /dev/null`; then
							init_type="openrc"
						else
							init_type="gentoo"
						fi
					fi
				fi
			fi

			if test "$init_type_wanted" = yes; then
				if test "$pid1" = "/sbin/init" -o "$pid1" = "/usr/sbin/init"; then
					if `$pid1 --version 2>/dev/null | grep "upstart" >/dev/null`; then
						init_type="upstart"
						init_type_wanted=no
					elif test -f "/etc/rc" -a ! -L "/etc/rc"; then
						init_type="newbsd"
						init_type_wanted=no
					else
						init_type="sysv"
						init_type_wanted=no
					fi
				fi
			fi

			if test "$init_type_wanted" = yes; then
				init_type="unknown"
			fi
		fi

		AC_MSG_RESULT($init_type)
])
