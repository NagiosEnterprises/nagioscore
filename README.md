autoconf-macros
===============

The purpose of Nagios autoconf-macros is to have a central place for
autoconf macros that can be maintained in one place, but be used by any
of the Nagios software. It is intended to be used as a git subtree.
See the [Usage](#usage) and [References](#references) sections below.

Since this project will be included in several parent projects, any
changes must be as project-neutral as possible.

Make sure to check out the [CHANGELOG](CHANGELOG.md) for relevant 
information, as well.


Contents
--------

The collection consists of the following macros:

### AX_NAGIOS_GET_OS alias AC_NAGIOS_GET_OS

> Output Variable : `opsys`

This macro detects the operating system, and transforms it into a generic
label. The most common OS's that use Nagios software are recognized and
used in subsequent macros.

### AX_NAGIOS_GET_DISTRIB_TYPE alias AC_NAGIOS_GET_DISTRIB_TYPE

> Output Variables : `dist_type`, `dist_ver`

This macro detects the distribution type. For Linux, this would be rh
(for Red Hat and derivitives), suse (OpenSUSE, SLES, derivitives), gentoo
(Gentoo and derivitives), debian (Debian and derivitives), and so on.
For BSD, this would be openbsd, netbsd, freebsd, dragonfly, etc. It can
also be aix, solaris, osx, and so on for Unix operating systems.

### AX_NAGIOS_GET_INIT alias AC_NAGIOS_GET_INIT

> Output Variable : `init_type`

This macro detects what software is used to start daemons on bootup
or on request, generally knows as the "init system". The init_type
will generally be one of sysv (many), bsd (Slackware), newbsd (*BSD),
launchd (OS X), smf10 or smf11 (Solaris), systemd (newer Linux),
gentoo (older Gentoo), upstart (several), or unknown.

### AX_NAGIOS_GET_INETD alias AC_NAGIOS_GET_INETD

> Output Variable : `inetd_type`

This macro detects what software is used to start daemons or services
on demand, which historically has been "inetd". The inetd_type
will generally be one of inetd, xinetd, launchd (OS X), smf10 or smf11
(Solaris), systemd (newer Linux), upstart (several), or unknown.

### AX_NAGIOS_GET_PATHS alias AC_NAGIOS_GET_PATHS

> Output Variables : **many!**

This macro determines the installation paths for binaries, config files,
PID files, and so on. For a "standard" install of Nagios, NRPE, NDO Utils,
etc., most will be in the /usr/local/nagios hierarchy with startup files
located in /etc. For distributions or software repositories, the
"--enable-install-method=os" option can be used. This will determine the
O/S dependant directories, such as /usr/bin, /usr/sbin, /var/lib/nagios,
/usr/lib/nagios, etc. or for OS X, /Library/LaunchDaemons.

### AX_NAGIOS_GET_FILES alias AC_NAGIOS_GET_FILES

> Output Variables : `src_init`, `src_inetd`, `src_tmpfile`

Each Nagios project will have a top-level directory named "/startup/".
In that directory will be "*.in" files for the various "init_type" and
"inetd_type" systems. This macro will determine which file(s) from
that directory will be needed.

### AX_NAGIOS_GET_SSL alias AC_NAGIOS_GET_SSL

> Output Variables : `HAVE_KRB5_H`, `HAVE_SSL`, `SSL_INC_DIR`, `SSL_LIB_DIR`, `CFLAGS`, `LDFLAGS`, `LIBS`

This macro checks various directories for SSL libraries and header files.
The searches are based on known install locations on various operating
systems and distributions, for openssl, gnutls-openssl, and nss_compat_ossl.
If it finds the headers and libraries, it will then do an `AC_LINK_IFELSE`
on a simple program to make sure a compile and link will work correctly.


Usage
-----

This repo is intended to be used as a git subtree, so changes will
automatically propogate, and still be reasonably easy to use.

* First, Create, checkout, clone, or branch your project. If you do an
`ls -AF` it might look something like this:

           .git/      .gitignore    ChangeLog   LICENSE   Makefile.in
           README     configure.ac  include/    src/

* Then make a reference to _this_ project inside your project.

           git remote add autoconf-macros git@github.com:NagiosEnterprises/autoconf-macros
           git subtree add --prefix=macros/ autoconf-macros master

* After executing the above two commands, if you do an `ls -AF` now,
it should look like this:

           .git/      .gitignore    ChangeLog   LICENSE   Makefile.in
           README     configure.ac  include/    macros/   src/

* The `macros/` directory has been added.

* Now do a `git push` to save everything.

* If you make any changes to autoconf-macros, commit them separately
from any parent-project changes to keep from polluting the commit
history with unrelated comments.

* To submit your changes to autoconf-macros:

           git subtree push --prefix=macros autoconf-macros peters-updates
This will create a new branch called `peters-updates`. You then need to
create a _pull request_ to get your changes merged into autoconf-macros
master.

* To get the latest version of `autoconf-macros` into your parent project:

           git subtgree pull --squash --prefix=macros autoconf-macros master


References
----------

Now that autoconf-macros is available to your project, you will need to
reference it.

* Create (or add these lines to) file `YourProject/aclocal.m4`

           m4_include([macros/ax_nagios_get_os])
           m4_include([macros/ax_nagios_get_distrib])
           m4_include([macros/ax_nagios_get_init])
           m4_include([macros/ax_nagios_get_inetd])
           m4_include([macros/ax_nagios_get_paths])
           m4_include([macros/ax_nagios_get_files])
           m4_include([macros/ax_nagios_get_ssl])

* In your `YourProject/configure.ac` add the following lines. A good place
to put them would be right after any `AC_PROG_*` entries:

           AC_NAGIOS_GET_OS
           AC_NAGIOS_GET_DISTRIB_TYPE
           AC_NAGIOS_GET_INIT
           AC_NAGIOS_GET_INETD
           AC_NAGIOS_GET_PATHS
           AC_NAGIOS_GET_FILES

* If you need SSL functionality, add the following to `YourProject/configure.ac`
where you want to check for SSL:

           AC_NAGIOS_GET_SSL

* You will now be able to reference any of the variables in `config.h.in`
and any files listed in the `AC_CONFIG_FILES` macro in `configure.ac`.


License Notice
--------------

Copyright (c) 2016-2017 Nagios Enterprises, LLC

This work is made available to you under the terms of Version 2 of
the GNU General Public License. A copy of that license should have
been provided with this software, but in any event can be obtained
from http://www.fsf.org.

This work is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301 or visit their web page on the internet at
http://www.fsf.org.


Questions?
----------

If you have questions about this addon, or problems getting things
working, first try searching the nagios-users mailing list archives.
Details on searching the list archives can be found at
http://www.nagios.org

If you don't find an answer there, post a message in the Nagios
Plugin Development forum at https://support.nagios.com/forum/viewforum.php?f=35
