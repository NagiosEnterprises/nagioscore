# Upstream: Ethan Galstad <nagios$nagios,org>
# Modified version from original dag spec

### FIXME: TODO: Add sysv script based on template. (remove cmd-file on start-up)
%define logmsg logger -t %{name}/rpm
%define logdir %{_localstatedir}/log/nagios

# Setup some debugging options in case we build with --with debug
%if %{defined _with_debug}
  %define mycflags -O0 -pg -ggdb3
%else
  %define mycflags %{nil}
%endif

# Allow newer compiler to suppress warnings
%if 0%{?el6}
  %define myXcflags -Wno-unused-result
%else
  %define myXcflags %{nil}
%endif

Summary: Open Source host, service and network monitoring program
Name: nagios
Version: 4.3.2
Release: 2%{?dist}
License: GPL
Group: Applications/System
URL: https://www.nagios.org/
Packager: Daniel Wittenberg <dwittenberg2008@gmail.com>
Vendor: Nagios Enterprises (https://www.nagios.org)
Source0: http://dl.sf.net/nagios/nagios-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: gd-devel > 1.8
BuildRequires: zlib-devel
BuildRequires: libpng-devel
BuildRequires: libjpeg-devel
BuildRequires: doxygen
BuildRequires: gperf

Obsoletes: nagios-www <= %{version}
Requires: httpd,php

%description
Nagios is an application, system and network monitoring application.
It can escalate problems by email, pager or any other medium. It is
also useful for incident or SLA reporting.

Nagios is written in C and is designed as a background process,
intermittently running checks on various services that you specify.

The actual service checks are performed by separate "plugin" programs
which return the status of the checks to Nagios. The plugins are
located in the nagios-plugins package.

%package devel
Summary: Header files, libraries and development documentation for %{name}
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
This package contains the header files, static libraries and development
documentation for %{name}. If you are a NEB-module author or wish to
write addons for Nagios using Nagios' own API's, you should install
this package.

%package contrib
Summary: Files from the contrib directory
Group: Development/Utils
Requires: %{name} = %{version}-%{release}

%description contrib
This package contains all the files from the contrib directory

%prep
%setup

# /usr/local/nagios is hardcoded in many places
%{__perl} -pi.orig -e 's|/usr/local/nagios/var/rw|%{_localstatedir}/nagios/rw|g;' contrib/eventhandlers/submit_check_result

%build

CFLAGS="%{mycflags} %{myXcflags}" LDFLAGS="$CFLAGS" %configure \
    --datadir="%{_datadir}/nagios" \
    --libexecdir="%{_libdir}/nagios/plugins" \
    --localstatedir="%{_localstatedir}/nagios" \
    --with-checkresult-dir="%{_localstatedir}/nagios/spool/checkresults" \
    --sbindir="%{_libdir}/nagios/cgi" \
    --sysconfdir="%{_sysconfdir}/nagios" \
    --with-cgiurl="/nagios/cgi-bin" \
    --with-command-user="apache" \
    --with-command-group="apache" \
    --with-gd-lib="%{_libdir}" \
    --with-gd-inc="%{_includedir}" \
    --with-htmurl="/nagios" \
    --with-init-dir="%{_initrddir}" \
    --with-lockfile="%{_localstatedir}/nagios/nagios.pid" \
    --with-mail="/bin/mail" \
    --with-nagios-user="nagios" \
    --with-nagios-group="nagios" \
    --with-perlcache \
    --with-template-objects \
    --with-template-extinfo \
    --enable-event-broker
find . -type f -name Makefile -exec /usr/bin/perl -p -i -e "s/-mtune=generic/-march=nocona/g" Makefile {} \; -print
%{__make} %{?_smp_mflags} all

### Build our documentation
%{__make} dox

### Apparently contrib does not obey configure !
%{__make} %{?_smp_mflags} -C contrib

%install
export PATH=%{_bindir}:/bin:\$PATH
%{__rm} -rf %{buildroot}
%{__make} install-unstripped install-init install-commandmode install-config \
    DESTDIR="%{buildroot}" \
    INSTALL_OPTS="" \
    COMMAND_OPTS="" \
    INIT_OPTS=""

%{__install} -d -m 0755 %{buildroot}%{_includedir}/nagios/
%{__install} -p -m 0644 include/*.h %{buildroot}%{_includedir}/nagios/
%{__mkdir} -p -m 0755 %{buildroot}/%{_includedir}/nagios/lib
%{__install} -m 0644 lib/*.h %{buildroot}/%{_includedir}/nagios/lib

%{__install} -Dp -m 0644 sample-config/httpd.conf %{buildroot}%{_sysconfdir}/httpd/conf.d/nagios.conf

### FIX log-paths
%{__perl} -pi -e '
        s|log_file.*|log_file=%{logdir}/nagios.log|;
        s|log_archive_path=.*|log_archive_path=%{logdir}/archives|;
        s|debug_file=.*|debug_file=%{logdir}/nagios.debug|;
   ' %{buildroot}%{_sysconfdir}/nagios/nagios.cfg

### make logdirs
%{__mkdir_p} %{buildroot}%{logdir}/
%{__mkdir_p} %{buildroot}%{logdir}/archives/

### Install logos
%{__mkdir_p} %{buildroot}%{_datadir}/nagios/images/logos

### Install documentation
%{__mkdir_p} %{buildroot}%{_datadir}/nagios/documentation
%{__cp} -a Documentation/html/* %{buildroot}%{_datadir}/nagios/documentation

# Put the new RC script in place
%{__install} -m 0755 daemon-init %{buildroot}/%{_initrddir}/nagios
%{__install} -d -m 0755 %{buildroot}/%{_sysconfdir}/sysconfig/
%{__install} -m 0644 nagios.sysconfig %{buildroot}/%{_sysconfdir}/sysconfig/nagios

### Apparently contrib wants to do embedded-perl stuff as well and does not obey configure !
%{__make} install -C contrib \
    DESTDIR="%{buildroot}" \
    INSTALL_OPTS=""

### Install libnagios
%{__install} -m 0644 lib/libnagios.a %{buildroot}%{_libdir}/libnagios.a

%{__install} -d -m 0755 %{buildroot}%{_libdir}/nagios/plugins/eventhandlers/
%{__cp} -afpv contrib/eventhandlers/* %{buildroot}%{_libdir}/nagios/plugins/eventhandlers/
%{__mv} contrib/README contrib/README.contrib

CGI=`find contrib/ -name '*.cgi' -type f |sed s/'contrib\/'//g`
CGI=`for i in $CGI; do echo -n "$i|"; done |sed s/\|$//`
find %{buildroot}/%{_libdir}/nagios/cgi -type f -print | sed s!'%{buildroot}'!!g | egrep -ve "($CGI)" > cgi.files
find %{buildroot}/%{_libdir}/nagios/cgi -type f -print | sed s!'%{buildroot}'!!g | egrep "($CGI)" > contrib.files



%pre
if ! /usr/bin/id nagios &>/dev/null; then
    /usr/sbin/useradd -r -d %{logdir} -s /bin/sh -c "nagios" nagios || \
        %logmsg "Unexpected error adding user \"nagios\". Aborting installation."
fi
if ! /usr/bin/getent group nagiocmd &>/dev/null; then
    /usr/sbin/groupadd nagiocmd &>/dev/null || \
        %logmsg "Unexpected error adding group \"nagiocmd\". Aborting installation."
fi

%post
/sbin/chkconfig --add nagios

if /usr/bin/id apache &>/dev/null; then
    if ! /usr/bin/id -Gn apache 2>/dev/null | grep -q nagios ; then
        /usr/sbin/usermod -a -G nagios,nagiocmd apache &>/dev/null
    fi
else
    %logmsg "User \"apache\" does not exist and is not added to group \"nagios\". Sending commands to Nagios from the command CGI is not possible."
fi

%preun
if [ $1 -eq 0 ]; then
    /sbin/service nagios stop &>/dev/null || :
    /sbin/chkconfig --del nagios
fi

%postun
# This could be bad if files are left with this uid/gid and then get owned by a new user
#if [ $1 -eq 0 ]; then
#    /usr/sbin/userdel nagios || %logmsg "User \"nagios\" could not be deleted."
#    /usr/sbin/groupdel nagios || %logmsg "Group \"nagios\" could not be deleted."
#fi
/sbin/service nagios condrestart &>/dev/null || :

%clean
%{__rm} -rf %{buildroot}

%files -f cgi.files
%defattr(-, root, root, 0755)
%doc Changelog INSTALLING LEGAL LICENSE README THANKS UPGRADING
%attr(0644,root,root) %config(noreplace) %{_sysconfdir}/httpd/conf.d/nagios.conf
%attr(0644,root,root) %config(noreplace) %{_sysconfdir}/sysconfig/nagios
%attr(0755,root,root) %config %{_initrddir}/nagios
%attr(0755,root,root) %{_bindir}/nagios
%attr(0755,root,root) %{_bindir}/nagiostats
%attr(0755,root,root) %{_libdir}/nagios/plugins/
%attr(0755,root,root) %{_datadir}/nagios/
%attr(0755,nagios,nagios) %dir %{_sysconfdir}/nagios/
%attr(0644,nagios,nagios) %config(noreplace) %{_sysconfdir}/nagios/*.cfg
%attr(0755,nagios,nagios) %{_sysconfdir}/nagios/objects/
%attr(0644,nagios,nagios) %config(noreplace) %{_sysconfdir}/nagios/objects/*.cfg
%attr(0755,nagios,nagios) %dir %{_localstatedir}/nagios/
%attr(0755,nagios,nagios) %{_localstatedir}/nagios/
%attr(0755,nagios,nagios) %{logdir}/
%attr(0755,nagios,apache) %{_localstatedir}/nagios/rw/
%attr(0644,root,root) %{_libdir}/libnagios.a

%files devel
%attr(0755,root,root) %{_includedir}/nagios/

%files contrib -f contrib.files
%doc contrib/README.contrib
%attr(0755,root,root) %{_bindir}/convertcfg
%attr(0755,root,root) %{_libdir}/nagios/plugins/eventhandlers/

%changelog
* Fri Nov 15 2013 Eric Stanley  <estanley@nagios.com> 4.0.1-1
- Corrected permissions on plugins directory (bug #494 - patch by Karsten Weiss)
- Corrected doc directive (bug #494 - patch by Karsten Weiss)
- Added configuration directive for *.cfg files (bug #494 - patch by Karsten Weiss)

* Wed Sep 18 2013 Daniel Wittenberg <dwittenberg2008@gmail.com> 4.0.0rc2-1
- Fix find command - Florin Andrei, bug #489
- Remove compiler warning option that breaks older builds, bug #488

* Fri Mar 15 2013 Daniel Wittenberg <dwittenberg2008@gmail.com> 3.99.96-1
- Major updates for version 4.0
- New spec file, new RC script, new sysconfig
