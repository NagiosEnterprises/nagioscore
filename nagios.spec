%define name nagios
%define version 1.0b2
%define release 1ignus2
%define nsusr nagios
%define nsgrp nagios
%define cmdusr apache
%define cmdgrp apache

%global init_dir %(init_dir=/etc/rc.d/init.d; if test -d /etc/rc.d/init.d; then init_dir="/etc/rc.d/init.d"; elif test -d /usr/local/etc/rc.d; then init_dir="/usr/local/etc/rc.d"; elif test -d /etc/rc.d; then init_dir="/etc/rc.d"; elif test -d /etc/init.d; then init_dir="/etc/init.d"; elif test -d /sbin/init.d; then init_dir="/sbin/init.d"; fi; echo $init_dir)
%global startup_script %{init_dir}/nagios


Summary: Host/service/network monitoring program
Name: %{name}
Version: %{version}
Release: %{release}
Copyright: GPL
Group: Application/System
Source0: %{name}-%{version}.tar.gz
#Patch0: %{name}-%{version}-%{release}.patch
BuildRoot: %{_tmppath}/%{name}-buildroot
Prefix: %{_prefix}
Prefix: /etc/rc.d/init.d
Prefix: /etc/init.d
Prefix: /etc/nagios
Prefix: /var/log/nagios
Prefix: /var/spool/nagios
Requires: gd > 1.8

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
#%patch -p1
mv contrib/htaccess.sample .


%build
export PATH=$PATH:/usr/sbin
CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" \
./configure \
	--with-cgiurl=/nagios/cgi-bin \
	--with-htmurl=/nagios \
	--with-lockfile=/var/run/nagios.pid \
	--with-nagios-user=%{nsusr} \
	--with-nagios-grp=%{nsgrp} \
	--with-command-user=%{cmdusr} \
	--with-command-grp=%{cmdgrp} \
	--prefix=%{_prefix} \
	--exec-prefix=%{_prefix}/sbin \
	--bindir=%{_prefix}/sbin \
	--sbindir=%{_prefix}/lib/nagios/cgi \
	--libexecdir=%{_prefix}/lib/nagios/plugins \
	--datadir=%{_prefix}/share/nagios \
	--sysconfdir=/etc/nagios \
	--localstatedir=/var/log/nagios \
	--enable-embedded-perl

make all
cd sample-config
mv nagios.cfg nagios.tmp
sed -e 's=/var/log/nagios/rw/=/var/spool/nagios/=' nagios.tmp > nagios.cfg
cd ..
cd contrib
make


%install
install -d -m 0775 ${RPM_BUILD_ROOT}/var/spool/nagios
install -d -m 0755 ${RPM_BUILD_ROOT}%{_prefix}/include/nagios
install -d -m 0755 ${RPM_BUILD_ROOT}/etc/rc.d/init.d
install -d -m 0755 ${RPM_BUILD_ROOT}/etc/init.d
install -d -m 0755 ${RPM_BUILD_ROOT}/etc/httpd/conf
install -d -m 0755 ${RPM_BUILD_ROOT}/etc/nagios
make DESTDIR=${RPM_BUILD_ROOT} INSTALL_OPTS="" COMMAND_OPTS="" install
make DESTDIR=${RPM_BUILD_ROOT} INSTALL_OPTS="" COMMAND_OPTS="" install-commandmode
make DESTDIR=${RPM_BUILD_ROOT} INSTALL_OPTS="" COMMAND_OPTS="" INIT_OPTS="" install-daemoninit
# make DESTDIR=${RPM_BUILD_ROOT} INSTALL_OPTS="" COMMAND_OPTS="" CGICFGDIR=/etc/nagios install-config
install -m 0644 sample-config/nagios.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 sample-config/cgi.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0640 sample-config/resource.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 sample-config/template-object/checkcommands.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 sample-config/template-object/contactgroups.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 sample-config/template-object/contacts.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 sample-config/template-object/dependencies.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 sample-config/template-object/escalations.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 sample-config/template-object/hostgroups.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 sample-config/template-object/hosts.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 sample-config/template-object/misccommands.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 sample-config/template-object/services.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 sample-config/template-object/timeperiods.cfg ${RPM_BUILD_ROOT}/etc/nagios
# cp test.cfg ${RPM_BUILD_ROOT}/etc/nagios
install -m 0644 common/locations.h ${RPM_BUILD_ROOT}%{_prefix}/include/nagios
cp htaccess.sample ${RPM_BUILD_ROOT}/etc/httpd/conf/nagios-httpd.conf
cd contrib; make INSTALL=install INSTALL_OPTS="" COMMAND_OPTS="" CGIDIR=${RPM_BUILD_ROOT}%{_prefix}/lib/nagios/cgi install
%global initscript %(find %{_tmppath}/%{name}-buildroot/etc/ -name nagios -type f | sed 's=%{_tmppath}/%{name}-buildroot==')


%postun
if [ $1 = 0 ]; then
   if [ `egrep "^%{nsusr}:" /etc/passwd | wc -l` = 1 ]; then
      /usr/sbin/userdel %{nsusr}
   fi
   if [ `egrep "^%{nsgrp}:" /etc/group | wc -l` = 1 ]; then
      /usr/sbin/groupdel %{nsgrp}
   fi
fi


%pre
if [ `egrep "^%{nsusr}:" /etc/passwd | wc -l` = 0 ]; then
	/usr/sbin/useradd -d /var/log/nagios -s /bin/sh -c "%{nsusr}" %{nsusr}
fi
initscript=`find /etc/ -name nagios -type f`
if [ -n $initscript ]; then
	if [ -f /var/lock/subsys/nagios ]; then
		$initscript stop
	elif [ -f /var/run/nagios.pid ]; then
		$initscript stop
	elif [ -f /var/log/nagios/nagios.lock ]; then
		$initscript stop
	fi
fi


%preun
initscript=`find /etc/ -name nagios -type f`
if [ $1 = 0 ]; then
	if [ -f /var/lock/subsys/nagios ]; then
		$initscript stop
	elif [ -f /var/run/nagios.pid ]; then
  	$initscript stop
	elif [ -f /var/log/nagios/nagios.lock ]; then
		$initscript stop
	fi
	if test -x /sbin/chkconfig; then
		/sbin/chkconfig --del nagios
	fi
fi


%post
initscript=`find /etc/ -name nagios -type f`
$initscript start
if test -x /sbin/chkconfig; then
	/sbin/chkconfig --add nagios
fi


%post www
cmdgrp=`awk '/^[ \t]*Group[ \t]+[a-zA-Z0-9]+/ {print $2}' /etc/httpd/conf/*.conf`
if test -z "$cmdgrp"; then
  cmdgrp=%{cmdgrp}
fi
if test -f /etc/httpd/conf/httpd.conf; then
	if ! grep "Include /etc/httpd/conf/nagios-httpd.conf" /etc/httpd/conf/httpd.conf >> /dev/null; then
		echo "Include /etc/httpd/conf/nagios-httpd.conf" >> /etc/httpd/conf/httpd.conf
		/etc/rc.d/init.d/httpd reload
	fi
fi


%preun www
if test -f /etc/httpd/conf/httpd.conf; then
	TEMPFILE=`mktemp /etc/httpd/conf/httpd.conf.tmp.XXXXXX`
	if grep "^ *Include /etc/httpd/conf/nagios-httpd.conf" /etc/httpd/conf/httpd.conf >> /dev/null; then
		grep -v "^ *Include /etc/httpd/conf/nagios-httpd.conf" /etc/httpd/conf/httpd.conf > $TEMPFILE
		mv $TEMPFILE /etc/httpd/conf/httpd.conf
		chmod 664 /etc/httpd/conf/httpd.conf
		/etc/rc.d/init.d/httpd reload
  fi
fi


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(755,root,root)
%init_dir/nagios
%dir /etc/nagios
%{_prefix}/sbin/nagios

%defattr(644,root,root)
%config(noreplace) /etc/nagios/nagios.cfg
%config(noreplace) /etc/nagios/cgi.cfg
#%config(noreplace) /etc/nagios/test.cfg
%config(noreplace) /etc/nagios/checkcommands.cfg
%config(noreplace) /etc/nagios/contactgroups.cfg
%config(noreplace) /etc/nagios/contacts.cfg
%config(noreplace) /etc/nagios/dependencies.cfg
%config(noreplace) /etc/nagios/escalations.cfg
%config(noreplace) /etc/nagios/hostgroups.cfg
%config(noreplace) /etc/nagios/hosts.cfg
%config(noreplace) /etc/nagios/misccommands.cfg
%config(noreplace) /etc/nagios/services.cfg
%config(noreplace) /etc/nagios/timeperiods.cfg

%defattr(640,root,%{nsgrp})
%config(noreplace) /etc/nagios/resource.cfg

%defattr(755,%{nsusr},%{nsgrp})
%dir /var/log/nagios
%dir /var/log/nagios/archives

%defattr(775,%{nsusr},%{cmdgrp})
%dir /var/spool/nagios

%doc Changelog INSTALL LICENSE README UPGRADING htaccess.sample


%files www
%defattr(755,root,root)
%dir %{_prefix}/lib/nagios/cgi
%{_prefix}/lib/nagios/cgi/*
%dir %{_prefix}/share/nagios
%defattr(-,root,root)
%{_prefix}/share/nagios/*
%config(noreplace) /etc/httpd/conf/nagios-httpd.conf


%files devel
%defattr(-,root,root)
%dir %{_prefix}/include/nagios
%{_prefix}/include/nagios/locations.h


%changelog
* Tue May 15 2002 Ethan Galstad <nagios@nagios.org) (1.0b1)
- Updated to work with new sample config files (template-based)
- Bugs were pointed out by Darren Gamble

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
