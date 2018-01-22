# ===========================================================================
# SYNOPSIS
#
#   AX_NAGIOS_GET_OS
#
# DESCRIPTION
#
#    This macro determines the operating system of the computer it is run on.
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

AU_ALIAS([AC_NAGIOS_GET_OS], [AX_NAGIOS_GET_OS])
AC_DEFUN([AX_NAGIOS_GET_OS],
[

AC_SUBST(opsys)
AC_SUBST(arch)

#
# Get user hints
#
	AC_MSG_CHECKING(what the operating system is )
	AC_ARG_WITH(opsys, AC_HELP_STRING([--with-opsys=OS],
	[specify operating system (linux, osx, bsd, solaris, irix, cygwin,
	 aix, hp-ux, etc.)]),
		[
			#
			# Run this if --with was specified
			#
			if test "x$withval" = x -o x$withval = xno; then
				opsys_wanted=yes
			else
				opsys_wanted=no
				opsys="$withval"
				AC_MSG_RESULT($opsys)
			fi
		], [
			#
			# Run this if --with was not specified
			#
			opsys_wanted=yes
		])

		if test x$opsys = xno; then
			opsys=""
			opsys_wanted=yes
		elif test x$opsys = xyes; then
			AC_MSG_ERROR([you must enter an O/S type if '--with-opsys' is specified])
		fi

		#
		# Determine operating system if it wasn't supplied
		#
		if test $opsys_wanted=yes; then
			opsys=`uname -s | tr ["[A-Z]" "[a-z]"]`
			if test x"$opsys" = "x"; then opsys="unknown"; fi
			AS_CASE([$opsys],
				[darwin*],		opsys="osx",
				[*bsd*],		opsys="bsd",
				[dragonfly],	opsys="bsd",
				[sunos],		opsys="solaris",
				[gnu/hurd],		opsys="linux",
				[irix*],		opsys="irix",
				[cygwin*],		opsys="cygwin",
				[mingw*],		opsys="mingw",
				[msys*],		opsys="msys")
		fi

		arch=`uname -m | tr ["[A-Z]" "[a-z]"]`

		AC_MSG_RESULT($opsys)
])
