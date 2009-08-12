%define name nagios
%define version 3.2.0
%define release 1
%define nsusr nagios
%define nsgrp nagios
%define cmdgrp nagiocmd
%define wwwusr apache
%define wwwgrp apache

# Performance data handling method to use. By default we will use
# the file-based one (as existed in NetSaint).
# You can select the external command based method (the defaut for
# Nagios) by specifying
# --define 'PERF_EXTERNAL 1'
# in the rpm command-line
%{!?PERF_EXTERNAL:           %define         PERF_EXTERNAL 0}

# Embedded Perl stuff, specify
# --define 'EMBPERL 1'
# in the rpm command-line to enable it
%{!?EMBPERL:           %define         EMBPERL 0}

# Macro that print mesages to syslog at package (un)install time
%define nnmmsg logger -t %{name}/rpm

Summary: Host/service/network monitoring program
Name: %{name}
Version: %{version}
Release: %{release}
License: GPL
Group: Application/System
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-buildroot
Prefix: %{_prefix}
Prefix: /etc/init.d
Prefix: /etc/nagios
Prefix: /var/log/nagios
Prefix: /var/spool/nagios
Requires: gd > 1.8, zlib, libpng, libjpeg, bash, grep
PreReq: /usr/bin/logger, chkconfig, sh-utils, shadow-utils, sed, initscripts, fileutils, mktemp
BuildRequires: gd-devel > 1.8, zlib-devel, libpng-devel, libjpeg-devel

%description
Nagios is a program that will monitor hosts and services on your
network. It has the ability to email or page you when a problem arises
and when a problem is resolved. Nagios is written in C and is
designed to run under Linux (and some other *NIX variants) as a
background process, intermittently running checks on various services
that you specify.

The actual service checks are performed by separate "plugin" programs
which return the status of the checks to Nagios. The plugins are
available at http://sourceforge.net/projects/nagiosplug

This package provide core programs for nagios. The web interface,
documentation, and development files are built as separate packages


%package www
Group: Application/System
Summary: Provides the HTML and CGI files for the Nagios web interface.
Requires: %{name} = %{version}
Requires: webserver


%description www
Nagios is a program that will monitor hosts and services on your
network. It has the ability to email or page you when a problem arises
and when a problem is resolved. Nagios is written in C and is
designed to run under Linux (and some other *NIX variants) as a
background process, intermittently running checks on various services
that you specify.

Several CGI programs are included with Nagios in order to allow you
to view the current service status, problem history, notification
history, and log file via the web. This package provides the HTML and
CGI files for the Nagios web interface. In addition, HTML
documentation is included in this package


%package devel
Group: Application/System
Summary: Provides include files that Nagios-related applications may compile against.
Requires: %{name} = %{version}

%description devel
Nagios is a program that will monitor hosts and services on your
network. It has the ability to email or page you when a problem arises
and when a problem is resolved. Nagios is written in C and is
designed to run under Linux (and some other *NIX variants) as a
background process, intermittently running checks on various services
that you specify.

This package provides include files that Nagios-related applications
may compile against.


%prep
%setup -q


%pre
# Create `nagios' user on the system if necessary
if /usr/bin/id %{nsusr} > /dev/null 2>&1 ; then
	: # user already exists
else
	/usr/sbin/useradd -r -d /var/log/nagios -s /bin/sh -c "%{nsusr}" %{nsusr} || \
		%nnmmsg Unexpected error adding user "%{nsusr}". Aborting install process.
fi

# id cannot tell us if the group already exists
# so just try to create it and assume it works
/usr/sbin/groupadd %{cmdgrp} > /dev/null 2>&1
 
# if LSB standard /etc/init.d does not exist,
# create it as a symlink to the first match we find
if [ -d /etc/init.d -o -L /etc/init.d ]; then
  : # we're done
elif [ -d /etc/rc.d/init.d ]; then
  ln -s /etc/rc.d/init.d /etc/init.d
elif [ -d /usr/local/etc/rc.d ]; then
  ln -s  /usr/local/etc/rc.d /etc/init.d
elif [ -d /sbin/init.d ]; then
  ln -s /sbin/init.d /etc/init.d
fi


%preun
if [ "$1" = 0 ]; then
	/sbin/service nagios stop > /dev/null 2>&1
	/sbin/chkconfig --del nagios
fi

%postun
if [ "$1" -ge "1" ]; then
	/sbin/service nagios condrestart >/dev/null 2>&1 || :
fi
# Delete nagios user and group
# (if grep doesn't find a match, then it is NIS or LDAP served and cannot be deleted)
if [ $1 = 0 ]; then
	/bin/grep '^%{nsusr}:' /etc/passwd > /dev/null 2>&1 && /usr/sbin/userdel %{nsusr} || %nnmmsg "User %{nsusr} could not be deleted."
	/bin/grep '^%{nsgrp}:' /etc/group > /dev/null 2>&1 && /usr/sbin/groupdel %{nsgrp} || %nnmmsg "Group %{nsgrp} could not be deleted."
	/bin/grep '^%{cmdgrp}:' /etc/group > /dev/null 2>&1 && /usr/sbin/groupdel %{cmdgrp} || %nnmmsg "Group %{cmdgrp} could not be deleted."
fi
 

%post www
# If apache is installed, and we can find the apache user, set a shell var
wwwusr=`awk '/^[ \t]*User[ \t]+[a-zA-Z0-9]+/ {print $2}' /etc/httpd/conf/*.conf`
if [ "z" == "z$wwwusr" ]; then # otherwise, use the default
	wwwusr=%{wwwusr}
fi
# if apache user is not in cmdgrp, add it
if /usr/bin/id -Gn $wwwusr 2>/dev/null | /bin/grep -q %{cmdgrp} > /dev/null 2>&1 ; then
	: # $wwwusr (default: apache) is already in nagiocmd group
else
	# first find apache primary group
	pgrp=`/usr/bin/id -gn $wwwusr 2>/dev/null`
	# filter apache primary group from secondary groups
	sgrps=`/usr/bin/id -Gn $wwwusr 2>/dev/null | /bin/sed "s/^$pgrp //;s/ $pgrp //;s/^$pgrp$//;s/ /,/g;"`
	if [ "z" == "z$sgrps" ] ; then
		sgrps=%{cmdgrp}
	else
		sgrps=$sgrps,%{cmdgrp}
	fi
	# modify apache user, adding it to cmdgrp
	/usr/sbin/usermod -G $sgrps $wwwusr >/dev/null 2>&1
	%nnmmsg "User $wwwusr added to group %{cmdgrp} so sending commands to Nagios from the command CGI is possible."
fi


%preun www
if [ $1 = 0 ]; then
if test -f /etc/httpd/conf/httpd.conf; then
	TEMPFILE=`mktemp /etc/httpd/conf/httpd.conf.tmp.XXXXXX`
	if grep "^ *Include /etc/httpd/conf.d/nagios.conf" /etc/httpd/conf/httpd.conf > /dev/null; then
		grep -v "^ *Include /etc/httpd/conf.d/nagios.conf" /etc/httpd/conf/httpd.conf > $TEMPFILE
		mv $TEMPFILE /etc/httpd/conf/httpd.conf
		chmod 664 /etc/httpd/conf/httpd.conf
		/etc/rc.d/init.d/httpd restart
  fi
fi
fi


%build
export PATH=$PATH:/usr/sbin
CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" \
./configure \
	--with-init-dir=/etc/init.d \
	--with-cgiurl=/nagios/cgi-bin \
	--with-htmurl=/nagios \
	--with-lockfile=/var/run/nagios.pid \
	--with-nagios-user=%{nsusr} \
	--with-nagios-group=%{nsgrp} \
	--prefix=%{_prefix} \
	--exec-prefix=%{_prefix}/sbin \
	--bindir=%{_prefix}/sbin \
	--sbindir=%{_prefix}/lib/nagios/cgi \
	--libexecdir=%{_prefix}/lib/nagios/plugins \
	--datadir=%{_prefix}/share/nagios \
	--sysconfdir=/etc/nagios \
	--localstatedir=/var/log/nagios \
%if ! %{PERF_EXTERNAL}
	--with-file-perfdata \
%endif
%if %{EMBPERL}
	--enable-embedded-perl \
%endif
	--with-gd-lib=/usr/lib \
	--with-gd-inc=/usr/include \
	--with-template-objects \
	--with-template-extinfo

make all

# make sample configs
###cd sample-config
###F=`mktemp temp.XXXXXX`
###sed -e 's=/var/log/nagios/rw/=/var/spool/nagios/=;s=@sysconfdir@/resource.cfg=@sysconfdir@/private/resource.cfg=' nagios.cfg > ${F}
###mv ${F} nagios.cfg
###cd ..

# make daemonchk.cgi and event handlers
cd contrib
make
cd eventhandlers
for f in `find . -type f` ; do
	F=`mktemp temp.XXXXXX`
	sed "s=/usr/local/nagios/var/rw/=/var/spool/nagios/=; \
		s=/usr/local/nagios/libexec/eventhandlers/=%{_prefix}/lib/nagios/plugins/eventhandlers=; \
		s=/usr/local/nagios/test/var=/var/log/nagios=" ${f} > ${F}
	mv ${F} ${f}
done
cd ../..


%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT
install -d -m 0775 ${RPM_BUILD_ROOT}/var/spool/nagios
install -d -m 0755 ${RPM_BUILD_ROOT}%{_prefix}/include/nagios
install -d -m 0755 ${RPM_BUILD_ROOT}/etc/init.d
install -d -m 0755 ${RPM_BUILD_ROOT}/etc/logrotate.d
install -d -m 0755 ${RPM_BUILD_ROOT}/etc/httpd/conf.d
install -d -m 0755 ${RPM_BUILD_ROOT}/etc/nagios
install -d -m 0755 ${RPM_BUILD_ROOT}/etc/nagios/objects
### install -d -m 0755 ${RPM_BUILD_ROOT}/etc/nagios/private
make DESTDIR=${RPM_BUILD_ROOT} INSTALL_OPTS="" COMMAND_OPTS="" install
make DESTDIR=${RPM_BUILD_ROOT} INSTALL_OPTS="" COMMAND_OPTS="" INIT_OPTS="" install-daemoninit

# install templated configuration files
cd sample-config
for f in {nagios,cgi}.cfg ; do
  [ -f $f ] && install -c -m 664 $f ${RPM_BUILD_ROOT}/etc/nagios/${f}
done
###mkdir -p ${RPM_BUILD_ROOT}/etc/nagios/private
for f in resource.cfg ; do
  [ -f $f ] && install -c -m 664 $f ${RPM_BUILD_ROOT}/etc/nagios/${f}
done
cd template-object
for f in {commands,contacts,localhost,switch,templates,timeperiods}.cfg
do
  [ -f $f ] && install -c -m 664 $f ${RPM_BUILD_ROOT}/etc/nagios/objects/${f}
done
cd ..
cd ..

# install headers for development package
install -m 0644 include/locations.h ${RPM_BUILD_ROOT}%{_prefix}/include/nagios

# install httpd configuration in RH80-style httpd config subdir
cp sample-config/httpd.conf ${RPM_BUILD_ROOT}/etc/httpd/conf.d/nagios.conf

# install CGIs
cd contrib
make INSTALL=install DESTDIR=${RPM_BUILD_ROOT} INSTALL_OPTS="" COMMAND_OPTS="" CGIDIR=%{_prefix}/lib/nagios/cgi install
#mv ${RPM_BUILD_ROOT}%{_prefix}/lib/nagios/cgi/convertcfg ${RPM_BUILD_ROOT}%{_prefix}/lib/nagios/
#mv ${RPM_BUILD_ROOT}%{_prefix}/lib/nagios/cgi/mini_epn ${RPM_BUILD_ROOT}%{_prefix}/sbin/
cd ..

# install event handlers
cd contrib/eventhandlers
install -d -m 0755 ${RPM_BUILD_ROOT}%{_prefix}/lib/nagios/eventhandlers
for file in * ; do
    test -f $file && install -m 0755 $file ${RPM_BUILD_ROOT}%{_prefix}/lib/nagios/eventhandlers && rm -f $file
done
cd ../..


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(755,root,root)
/etc/init.d/nagios
%{_prefix}/sbin/nagios
%{_prefix}/sbin/nagiostats
%if %{EMBPERL}
%{_prefix}/sbin/p1.pl
%endif
%{_prefix}/sbin/mini_epn
%{_prefix}/sbin/new_mini_epn
%dir %{_prefix}/lib/nagios/eventhandlers
%{_prefix}/lib/nagios/eventhandlers/*
%{_sbindir}/convertcfg
%dir /etc/nagios
%dir /etc/nagios/objects
%defattr(644,root,root)
%config(noreplace) /etc/nagios/*.cfg
%config(noreplace) /etc/nagios/objects/*.cfg
%defattr(750,root,%{nsgrp})
###%dir /etc/nagios/private
%defattr(640,root,%{nsgrp})
### %config(noreplace) /etc/nagios/private/resource.cfg
%defattr(755,%{nsusr},%{nsgrp})
%dir /var/log/nagios
%dir /var/log/nagios/archives
%defattr(2775,%{nsusr},%{nsgrp})
%dir /var/spool/nagios
%doc Changelog INSTALLING LICENSE README UPGRADING


%files www
%defattr(755,root,root)
%dir %{_prefix}/lib/nagios/cgi
%{_prefix}/lib/nagios/cgi/*
%dir %{_prefix}/share/nagios
%defattr(-,root,root)
%{_prefix}/share/nagios/*
%config(noreplace) /etc/httpd/conf.d/nagios.conf


%files devel
%defattr(-,root,root)
%dir %{_prefix}/include/nagios
%{_prefix}/include/nagios/locations.h


%changelog
* Tue Nov 22 2005 Andreas Kasenides <ank {at} cs.ucy.ac.cy>
- packaged %{_prefix}/sbin/new_mini_epn
- moved resource.cfg in /etc/nagios

* Thu Dec 30 2004 Rui Miguel Silva Seabra <rms@sibs.pt>
- FIX spec (wrong tag for License, and update to current state of compile)

* Sat May 31 2003 Karl DeBisschop <kdebisschop@users.sourceforge.net> (1.1-1)
- Merge with CVS for 1.1 release

* Fri May 30 2003 Karl DeBisschop <kdebisschop@users.sourceforge.net> (1.0-4)
- cmdgrp was not always getting created
- patches for cmd.cgi and history.cgi

* Sat May 24 2003 Karl DeBisschop <kdebisschop@users.sourceforge.net> (1.0-3)
- patches for doco and PostgreSQL timestamp
- make sure all files are packaged (otherwise, will not build on RH9)

* Sat May 17 2003 Karl DeBisschop <kdebisschop@users.sourceforge.net> (1.0-2)
- patch for file descriptor leak

* Fri Oct 04 2002 Karl DeBisschop <kdebisschop@users.sourceforge.net>
- merge many improvements from Ramiro Morales <rm-rpms@gmx.net>
  (macros for PERF_EXTERNAL and EMBPERL, cleanup pre/post scripts,
   nnmmsg logger macro, include eventhandlers, convertcfg, mini_epn)
- use LSB-standard /etc/init.d/nagios startup location

* Tue Aug 13 2002 Karl DeBisschop <kdebisschop@users.sourceforge.net>
- INSTALL was renamed INSTALLING
- p1.pl script included in package
- web server restarted because Red Hat 7.3 init does not do 'reload'

* Fri Jun 14 2002 Ethan Galstad <egalstad@nagios.org) (1.0b4)
- Modified requirements to work when installed using KickStart (Jeff Frost)
- Changed method used for checking for user/group existence (Jeff Frost)

* Tue May 15 2002 Ethan Galstad <egalstad@nagios.org) (1.0b1)
- Updated to work with new sample template-based config files (Darren Gamble)

* Sun Feb 17 2002 Ole Gjerde <gjerde@ignus.com> (1.0a4)
- Fixed spec file to work with Nagios

* Wed Jan 17 2001 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.7a5-1)
- switch from /usr/libexec to /usr/lib because linux FHS has no libexec
- use global macro to set location of init script
- fold htaccess.sample into contrib directory of tarball

* Fri Nov 03 2000 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.6-1)
- Rebuild with final sources

* Wed Sep 06 2000 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.6b5-1)
- Create separate cgi, html, and devel packages
- Include commands.cfg

* Sun Aug 27 2000 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.6b5-1)
- beta 5

* Sun Jul 23 2000 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.6b3-2)
- fixes for daemon-init, multi-OS RPM building

* Wed Jul 12 2000 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.6b3-1)
- beta 3

* Sun Jun 25 2000 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.6b2-3)
- true beta2 sources

* Sat Jun 24 2000 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.6b2-2)
- cleanup spec, still using pre-beta2 sources

* Sat Jun 24 2000 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.6b2-1)
- mandrake merge using pre-beta2 sources (many thanks to Stefan van der Eijk <s.vandereijk@chello.nl>)

* Wed Jun 14 2000 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.6b1-1)
- add stylesheet diffs

* Mon Jun 12 2000 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.6b1-1)
- adapt for 0.0.6b1

* Mon Jun 05 2000 Karl DeBisschop <kdebisschop@users.sourceforge.net> (0.0.5-4)
- add traceroute.cgi and htaccess.sample
- move placement of docs (in files) to avoid group warnings
- change www user and group to nobody and add warning

* Mon Jun 05 2000 Karsten Weiss <knweiss@gmx.de> (0.0.5-3)
- official group name
- improved user detection

* Tue Oct 19 1999 Mike McHenry <mmchen@minn.net) (0.0.4-2)
- Fixed init.d scripts to better fit new Redhat init.d script formats

* Fri Sep 03 1999 Mike McHenry <mmchen@minn.net> (0.0.4-1)
- Upgraded package from 0.0.4b4 to 0.0.4

* Mon Aug 16 1999 Mike McHenry <mmchen@minn.net>
- First RPM build (0.0.4b4)
