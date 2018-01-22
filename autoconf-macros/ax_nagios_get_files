# ===========================================================================
# SYNOPSIS
#
#   AX_NAGIOS_GET_FILES
#
# DESCRIPTION
#
#    This macro figures out which init and/or inetd files to use based
#    on the results of the AX_NAGIOS_GET_OS, AX_NAGIOS_GET_DISTRIB_TYPE,
#    AX_NAGIOS_GET_INIT and AX_NAGIOS_GET_INETD macros. It will select
#    the appropriate files(s) from the 'startup' directory and copy it.
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

AU_ALIAS([AC_NAGIOS_GET_FILES], [AX_NAGIOS_GET_FILES])
AC_DEFUN([AX_NAGIOS_GET_FILES],
[

AC_SUBST(src_init)
AC_SUBST(src_inetd)
AC_SUBST(src_tmpfile)
AC_SUBST(bsd_enable)

src_inetd=""
src_init=""
bsd_enable=""

AC_MSG_CHECKING(for which init file to use )

AS_CASE([$init_type],

	[sysv],
		src_init=default-init,

	[systemd],
		src_tmpfile=tmpfile.conf
		src_init=default-service,

	[bsd],
		src_init=bsd-init,

	[newbsd],
		if test $dist_type = freebsd ; then
			bsd_enable="_enable"
			src_init=newbsd-init
		elif test $dist_type = openbsd ; then
			bsd_enable="_flags"
			src_init=openbsd-init
		elif test $dist_type = netbsd ; then
			bsd_enable=""
			src_init=newbsd-init
		fi,

#	[gentoo],

	[openrc],
		src_init=openrc-init,

	[smf*],
		src_init="solaris-init.xml"
		src_inetd="solaris-inetd.xml",

	[upstart],
		if test $dist_type = rh ; then
			src_init=rh-upstart-init
		else
			src_init=upstart-init
		fi,

	[launchd],
		src_init="mac-init.plist",

	[*],
		src_init="unknown"
)
AC_MSG_RESULT($src_init)

AC_MSG_CHECKING(for which inetd files to use )

if test x$src_inetd = x; then

	AS_CASE([$inetd_type],
		[inetd*],
			src_inetd=default-inetd,

		[xinetd],
			src_inetd=default-xinetd,

		[systemd],
			src_inetd=default-socket,

		[launchd],
			src_inetd="mac-inetd.plist",

		[*],
			src_inetd="unknown"
	)

fi
AC_MSG_RESULT($src_inetd)

])
