%global _hardened_build 1
# Set bootstrap = 1 to build without depending on plugins for localhost monitoring
%global bootstrap 0

Name:           nagios
Version: 4.4.9
Release:        4%{?dist}

Summary: Host/service/network monitoring program

License:        GPLv2
URL:            https://www.nagios.org/projects/nagios-core/
Source0:        https://github.com/NagiosEnterprises/nagioscore/archive/nagios-%{version}.tar.gz#/nagioscore-nagios-%{version}.tar.gz
Source1: nagios.logrotate
Source2: nagios.htaccess
Source3: nagios.internet.cfg
Source4: nagios.htpasswd
Source5: nagios.upgrade_to_v4.ReadMe
Source6: nagios.upgrade_to_v4.sh
Source8: nagios.tmpfiles.conf
# PNG files from the old nagios-0010-Added-several-images-to-the-sample-config.patch
Source10: printer.png
Source11: router.png
Source12: switch.png
Source13: nagios.README.SELinux.rst
Source14: nagios_epel7.te
Source15: nagios_epel.fc
Source16: nagios_epel6.te

Patch1: nagios-0001-default-init.patch
# Sent upstream
Patch2: nagios-0002-Fix-installation-of-httpd-conf.d-config-file.patch
Patch3: nagios-0003-Install-config-files-too.patch
Patch4: nagios-0004-Fix-path-to-CGI-executables.patch
Patch5: nagios-0005-Fixed-path-to-passwd-file-in-Apache-s-config-file.patch
Patch6: nagios-0006-Added-several-images-to-the-sample-config-revb.patch
Patch9: nagios-0009-fix-localstatedir-for-linux.patch
## This has been requested for security groups not wanting to leak
## their nagios location.
Patch10: nagios-0010-remove-information-leak.patch
## Make it so it knows about all the arches fedora cares about
Patch11: nagios-0011-remove-rpmbuild.patch
Patch12: nagios-0012-fix-spool.patch
Patch13: nagios-0013-fix-plugin.patch
Patch14: nagios-0014-fix-uidgid.patch
Patch15: %{name}-0015-Changelog.patch

BuildRequires:  make
BuildRequires:  doxygen
BuildRequires:  gcc
BuildRequires:  gd-devel > 1.8
BuildRequires:  gperf
BuildRequires:  libjpeg-devel
BuildRequires:  libpng-devel
BuildRequires:  zlib-devel
BuildRequires:  perl-generators
BuildRequires:  perl(CPAN)
BuildRequires:  perl(ExtUtils::MakeMaker)
BuildRequires:  perl(ExtUtils::Embed)
BuildRequires:  perl(Test::Harness)
%if 0%{?el6}%{?fedora}
BuildRequires:  perl(Test::HTML::Lint)
%endif
BuildRequires:  perl(Test::More)
BuildRequires:  perl(Test::Simple)

# For up-to-date config.sub and config.guess
BuildRequires:  libtool

# For selinux tools
BuildRequires: checkpolicy, selinux-policy-devel

%if 0%{?rhel} > 6 || 0%{?fedora} > 20
# For necessary macros
BuildRequires:  systemd
%endif

Requires:       httpd
Requires:       php
Requires:       perl(:MODULE_COMPAT_%(eval "`%{__perl} -V:version`"; echo $version))
Requires:       mailx
Requires:       nagios-common
Requires:       user(nagios)
Requires:       group(nagios)

# This plugins are required for localhost monitoring
%if ! 0%{?bootstrap}
Requires:       nagios-plugins-ping
Requires:       nagios-plugins-load
Requires:       nagios-plugins-users
Requires:       nagios-plugins-http
Requires:       nagios-plugins-disk
Requires:       nagios-plugins-ssh
Requires:       nagios-plugins-swap
Requires:       nagios-plugins-procs
%endif

Requires(pre):    group(nagios)
Requires(pre):    user(nagios)
%if 0%{?rhel} > 6 || 0%{?fedora} > 20
# For necessary macros
BuildRequires:  systemd
%else
Requires(preun):  initscripts, chkconfig
Requires(post):   initscripts, chkconfig
Requires(postun): initscripts
%endif

%description
Nagios is a program that will monitor hosts and services on your
network.  It has the ability to send email or page alerts when a
problem arises and when a problem is resolved.  Nagios is written
in C and is designed to run under Linux (and some other *NIX
variants) as a background process, intermittently running checks
on various services that you specify.

The actual service checks are performed by separate "plugin" programs
which return the status of the checks to Nagios. The plugins are
available at https://github.com/nagios-plugins/nagios-plugins

This package provides the core program, web interface, and documentation
files for Nagios. Development files are built as a separate package.

%package common
Summary:        Provides common directories, uid and gid among nagios-related packages
Requires(pre):  shadow-utils
Requires(post): shadow-utils
Provides:       user(nagios)
Provides:       group(nagios)


%description common
Provides common directories, uid and gid among nagios-related packages.


%package devel
Summary:        Provides include files that Nagios-related applications may compile against
Requires:       %{name} = %{version}-%{release}


%description devel
Nagios is a program that will monitor hosts and services on your
network. It has the ability to email or page you when a problem arises
and when a problem is resolved. Nagios is written in C and is
designed to run under Linux (and some other *NIX variants) as a
background process, intermittently running checks on various services
that you specify.

This package provides include files that Nagios-related applications
may compile against.

%if 0%{?rhel} > 5
%package selinux
Summary:          SELinux context for %{name}
Requires:         %name = %version-%release
Requires(post):   policycoreutils
Requires(postun): policycoreutils


%description selinux
SElinux context for %{name}.
%endif

%package contrib
Summary:          Eventhandlers contributed to nagios
Requires:         %name = %version-%release

%description contrib
Various contributed items used by plugins and other tools.

%prep
%autosetup -p1 -n nagioscore-nagios-%{version}

install -p -m 0644 %{SOURCE10} %{SOURCE11} %{SOURCE12} html/images/logos/


%build
%configure \
    --prefix=%{_datadir}/%{name} \
    --exec-prefix=%{_localstatedir}/lib/%{name} \
    --libdir=%{_libdir}/%{name} \
    --bindir=%{_sbindir} \
    --datadir=%{_datadir}/%{name}/html \
    --libexecdir=%{_libdir}/%{name}/plugins \
    --localstatedir=%{_localstatedir} \
    --with-checkresult-dir=%{_localstatedir}/spool/%{name}/checkresults \
    --with-cgibindir=%{_libdir}/nagios/cgi \
    --sysconfdir=%{_sysconfdir}/%{name} \
    --with-cgiurl=/%{name}/cgi-bin \
    --with-command-user=apache \
    --with-command-group=apache \
    --with-gd-lib=%{_libdir} \
    --with-gd-inc=%{_includedir} \
    --with-htmlurl=/%{name} \
    --with-lockfile=%{_localstatedir}/run/%{name}/%{name}.pid \
%if 0%{?rhel} == 6
    --with-mail=/bin/mail \
    --with-initdir=%{_initrddir} \
    --with-init-type=sysv \
%else
    --with-mail=/usr/bin/mail \
    --with-initdir=%{_unitdir} \
    --with-init-type=systemd \
%endif
    --with-nagios-user=nagios \
    --with-nagios-grp=nagios \
    --with-template-objects \
    --with-template-extinfo \
    --enable-event-broker \
    STRIP=/bin/true

%make_build all

### Build our documentation
%make_build dox

### Apparently contrib does not obey configure !
%make_build -C contrib


sed -e "s|/usr/lib/|%{_libdir}/|" %{SOURCE2} > %{name}.htaccess
cp -f %{SOURCE3} internet.cfg
cp -f %{SOURCE5} UpgradeToVersion4.ReadMe
cp -f %{SOURCE6} UpgradeToVersion4.sh
echo >> html/stylesheets/common.css

%if 0%{?rhel} > 5
## SELinux configs
mkdir selinux
install -pm 644 %{SOURCE13} README.SELinux.rst
%if 0%{?rhel} == 6
cp -p %{SOURCE16} selinux/%{name}_epel.te
%else
cp -p %{SOURCE14} selinux/%{name}_epel.te
%endif
cp -p %{SOURCE15} selinux/%{name}_epel.fc
touch selinux/%{name}_epel.if
%make_build -f %{_datadir}/selinux/devel/Makefile
%endif


%install
%make_install INIT_OPTS="" INSTALL_OPTS="" COMMAND_OPTS="" CGIDIR="%{_libdir}/%{name}/cgi-bin" CFGDIR="%{_sysconfdir}/%{name}" fullinstall

# relocated to sbin (Fedora-specific)
install -d -m 0755 %{buildroot}%{_bindir}
mv %{buildroot}%{_sbindir}/nagiostats %{buildroot}%{_bindir}/nagiostats

install -d -m 0755 %{buildroot}%{_sysconfdir}/%{name}/private
mv %{buildroot}%{_sysconfdir}/%{name}/resource.cfg %{buildroot}%{_sysconfdir}/%{name}/private/resource.cfg

install -D -m 0644 %{SOURCE4} %{buildroot}%{_sysconfdir}/%{name}/passwd

# Install header-file
install -D -m 0644 include/locations.h %{buildroot}%{_includedir}/%{name}/locations.h

# Install logrotate rule
install -D -m 0644 %{SOURCE1} %{buildroot}%{_sysconfdir}/logrotate.d/%{name}

# Make room for event-handlers
install -d -m 0755 %{buildroot}%{_libdir}/%{name}/plugins/eventhandlers
install -d -m 0755 %{buildroot}%{_libdir}/%{name}/plugins/eventhandlers/distributed-monitoring/
install -d -m 0755 %{buildroot}%{_libdir}/%{name}/plugins/eventhandlers/redundancy-scenario1/

install -d -m 0775 %{buildroot}%{_localstatedir}/spool/%{name}/cmd
install -d -m 0775 %{buildroot}%{_localstatedir}/run/%{name}

# Make a run directory
install -d -m 0775 %{buildroot}%{_localstatedir}/run/%{name}

# Make logdirs
install -d -m 0775 %{buildroot}/%{_localstatedir}/log/
install -d -m 0775 %{buildroot}/%{_localstatedir}/log/%{name}/
install -d -m 0775 %{buildroot}/%{_localstatedir}/log/%{name}/archives

# Use systemd unit on rhel7 or any supported Fedora
%if 0%{?rhel} > 6 || 0%{?fedora} > 20
# Install systemd entry
install -D -m 0644 -p %{SOURCE8} %{buildroot}%{_tmpfilesdir}/%{name}.conf

# Remove SystemV init-script
rm -f %{buildroot}%{_initrddir}/nagios

# Fix systemd unit file permissions #1676334
chmod -x %{buildroot}%{_unitdir}/%{name}.service
%endif

# Fix permissions - FIXME remove this when unneeded
chmod 755 %{buildroot}%{_sbindir}/nagios

# Install documentation
install -d -m 0755 %{buildroot}%{_datadir}/nagios/html/docs
%{__cp} -a Documentation/html/* %{buildroot}%{_datadir}/nagios/html/docs

%if 0%{?rhel} >5
# Selinux configs
install -p -m 644 -D %{name}_epel.pp $RPM_BUILD_ROOT%{_datadir}/selinux/packages/%{name}/%{name}_epel.pp
%endif

### CONTRIB ITEMS TAKEN FROM UPSTREAM NAGIOS SPEC
%make_install -C contrib INSTALL_OPTS=""
install -p -m 644 contrib/eventhandlers/disable_active_service_checks %{buildroot}%{_libdir}/nagios/plugins/eventhandlers/
install -p -m 644 contrib/eventhandlers/disable_notifications %{buildroot}%{_libdir}/nagios/plugins/eventhandlers/
install -p -m 644 contrib/eventhandlers/enable_active_service_checks %{buildroot}%{_libdir}/nagios/plugins/eventhandlers/
install -p -m 644 contrib/eventhandlers/enable_notifications %{buildroot}%{_libdir}/nagios/plugins/eventhandlers/
install -p -m 644 contrib/eventhandlers/submit_check_result %{buildroot}%{_libdir}/nagios/plugins/eventhandlers/

install -p -m 644 contrib/eventhandlers/distributed-monitoring/obsessive_svc_handler %{buildroot}%{_libdir}/nagios/plugins/eventhandlers/distributed-monitoring/
install -p -m 644 contrib/eventhandlers/distributed-monitoring/submit_check_result_via_nsca %{buildroot}%{_libdir}/nagios/plugins/eventhandlers/distributed-monitoring/
install -p -m 644 contrib/eventhandlers/redundancy-scenario1/handle-master-host-event %{buildroot}%{_libdir}/nagios/plugins/eventhandlers/redundancy-scenario1/
install -p -m 644 contrib/eventhandlers/redundancy-scenario1/handle-master-proc-event %{buildroot}%{_libdir}/nagios/plugins/eventhandlers/redundancy-scenario1/

%{__mv} contrib/README contrib/README.contrib

%pre common
getent group nagios >/dev/null || groupadd -r nagios
getent passwd nagios >/dev/null || useradd -r -g nagios -d %{_localstatedir}/spool/%{name} -s /sbin/nologin nagios
exit 0


%post
%{_sbindir}/usermod -a -G %{name} apache || :

%if 0%{?rhel} > 6 || 0%{?fedora} > 20
%systemd_post %{name}.service  > /dev/null 2>&1 || :
%else
if [ $1 -eq 1 ]; then
        # Initial installation
        /sbin/chkconfig --add %{name} || :
fi
%endif

%if 0%{?el5}%{?el6}
/sbin/service httpd condrestart > /dev/null 2>&1 || :
if [ $1 -gt 1 ]; then
  /sbin/service nagios reload > /dev/null 2>&1 || :
fi
%else
/usr/bin/systemctl condrestart httpd > /dev/null 2>&1 || :

if [ $1 -gt 1 ]; then
  /usr/bin/systemctl reload nagios  > /dev/null 2>&1 || :
fi
%endif

%preun
%if 0%{?rhel} > 6 || 0%{?fedora} > 20
%systemd_preun %{name}.service
%else
if [ $1 -eq 0 ]; then
        # Package removal, not upgrade
        /sbin/service %{name} stop >/dev/null 2>&1 || :
        /sbin/chkconfig --del %{name} || :
fi
%endif


%postun
%if 0%{?el5}%{?el6}
/sbin/service httpd condrestart > /dev/null 2>&1 || :
%else
/usr/bin/systemctl condrestart httpd  > /dev/null 2>&1 || :
%endif


%if 0%{?fedora} > 20
%triggerun -- %{name} < 3.5.1-2
# Save the current service runlevel info
# User must manually run systemd-sysv-convert --apply opensips
# to migrate them to systemd targets
/usr/bin/systemd-sysv-convert --save %{name} >/dev/null 2>&1 ||:

# Run these because the SysV package being removed won't do them
/sbin/chkconfig --del %{name} >/dev/null 2>&1 || :
/bin/systemctl try-restart %{name}.service >/dev/null 2>&1 || :
%endif

%if 0%{?rhel} >5
%post selinux
%if 0%{?el5}%{?el6}
if [ "$1" -le "1" ]; then # First install
   semodule -i %{_datadir}/selinux/packages/%{name}/%{name}_epel.pp 2>/dev/null || :
   fixfiles -R %{name} restore || :
   /sbin/service %{name} condrestart > /dev/null 2>&1  || :
fi
%else
if [ "$1" -le "1" ]; then # First install
   semodule -i %{_datadir}/selinux/packages/%{name}/%{name}_epel.pp 2>/dev/null || :
   fixfiles -R %{name} restore || :
   %systemd_postun_with_restart %{name}.service
fi
%endif
%endif

%if 0%{?rhel} >5
%preun selinux
%if 0%{?el6}
if [ "$1" -lt "1" ]; then # Final removal
    semodule -r %{name}_epel 2>/dev/null || :
    fixfiles -R %{name} restore || :
    /sbin/service %{name} condrestart > /dev/null 2>&1 || :
fi
%else
if [ "$1" -lt "1" ]; then # Final removal
    semodule -r %{name}_epel 2>/dev/null || :
    fixfiles -R %{name} restore || :
    %systemd_postun_with_restart %{name}.service
fi
%endif
%endif

%if 0%{?rhel} >5
%postun selinux
if [ "$1" -ge "1" ]; then # Upgrade
    # Replaces the module if it is already loaded
    semodule -i %{_datadir}/selinux/packages/%{name}/%{name}_epel.pp 2>/dev/null || :
     # no need to restart the daemon
fi
%endif      



%files
%dir %{_libdir}/%{name}/cgi-bin
%dir %{_datadir}/%{name}
%dir %{_datadir}/%{name}/html
%doc %{_datadir}/%{name}/html/docs
%doc Changelog INSTALLING README.md UPGRADING UpgradeToVersion4.ReadMe UpgradeToVersion4.sh
%doc internet.cfg
%license LICENSE
%{_datadir}/%{name}/html/[^cd]*
%{_datadir}/%{name}/html/contexthelp/
%{_datadir}/%{name}/html/d3/

%{_sbindir}/*
%{_bindir}/*
%{_libdir}/%{name}/cgi-bin/*cgi
%if 0%{?rhel} > 6 || 0%{?fedora} > 20
%{_unitdir}/%{name}.service
%{_tmpfilesdir}/%{name}.conf
%else
%{_initrddir}/nagios
%endif
%config(noreplace) %{_sysconfdir}/httpd/conf.d/nagios.conf
%config(noreplace) %{_sysconfdir}/logrotate.d/%{name}
%config(noreplace) %{_sysconfdir}/%{name}/*cfg
%config(noreplace) %{_sysconfdir}/%{name}/objects/*cfg
%attr(0750,root,nagios) %dir %{_sysconfdir}/%{name}/private
%attr(0750,root,nagios) %dir %{_sysconfdir}/%{name}/objects

%attr(0640,root,nagios) %config(noreplace) %{_sysconfdir}/%{name}/private/resource.cfg
%attr(0640,root,apache) %config(noreplace) %{_sysconfdir}/%{name}/passwd
%attr(0640,root,apache) %config(noreplace) %{_datadir}/%{name}/html/config.inc.php
%attr(2775,nagios,nagios) %dir %{_localstatedir}/spool/%{name}/cmd
%attr(0750,nagios,nagios) %dir %{_localstatedir}/run/%{name}
%attr(0750,nagios,nagios) %dir %{_localstatedir}/log/%{name}
%attr(0750,nagios,nagios) %dir %{_localstatedir}/log/%{name}/archives
%attr(0770,nagios,nagios) %dir %{_localstatedir}/spool/%{name}/checkresults

%files common
%dir %{_sysconfdir}/%{name}
%dir %{_libdir}/%{name}
%attr(0755,root,root) %dir %{_libdir}/%{name}/plugins
%attr(0755,root,root) %dir %{_libdir}/%{name}/plugins/eventhandlers/
%attr(0755,nagios,nagios) %dir %{_localstatedir}/spool/%{name}


%files devel
%{_includedir}/%{name}
%attr(0644,root,root) %{_libdir}/%{name}/libnagios.a

%if 0%{?rhel} > 5
%files selinux
%doc README.SELinux.rst
%{_datadir}/selinux/packages/%{name}/nagios_epel.pp
%endif

%files contrib 
%doc contrib/README.contrib
%attr(0750,root,root) %{_libdir}/%{name}/plugins/eventhandlers/*
%{_libdir}/%{name}/cgi/

%changelog
* Wed Mar 03 2021 Guido Aulisi <guido.aulisi@gmail.com> - 4.4.6-4
- Add missing require for nagios-plugins-ping
- Fix run path

* Sat Feb 27 2021 Guido Aulisi <guido.aulisi@gmail.com> - 4.4.6-3
- Require plugins needed for localhost monitoring (#1932297)

* Tue Feb 23 2021 Guido Aulisi <guido.aulisi@gmail.com> - 4.4.6-2
- Fix systemd unit file permissions #1676334

* Sat Feb 20 2021 Guido Aulisi <guido.aulisi@gmail.com> - 4.4.6-1
- Update to 4.4.6
- Fix for CVE-2020-13977 #BZ1849087
- Some spec cleanup

* Tue Feb 18 2020 Stephen Smoogen <smooge@fedoraproject.org> - 4.4.5-3
- Add change to allow for problems found in mass rebuild and gcc10.
- Fix BZ#1793909

* Wed Jan 29 2020 Fedora Release Engineering <releng@fedoraproject.org> - 4.4.5-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_32_Mass_Rebuild

* Thu Aug 29 2019 Stephen Smoogen <smooge@fedoraproject.org> - 4.4.5-1
- Move to 4.4.5
- Updated patches to cleanly patch

* Fri Jul 26 2019 Stephen Smoogen <smooge@fedoraproject.org> - 4.4.3-7
- Try to put in fixes to allow this to work on EL8

* Thu Jul 25 2019 Fedora Release Engineering <releng@fedoraproject.org> - 4.4.3-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_31_Mass_Rebuild

* Fri May 31 2019 Jitka Plesnikova <jplesnik@redhat.com> - 4.4.3-5
- Perl 5.30 rebuild

* Fri Feb 22 2019 Stephen Smoogen <smooge@fedoraproject.org> - 4.4.3-4
- Fix BZ#1674258 add explicite User and Group to systemctl startup.
- Problem was missed because some config files had this set in them

* Tue Feb  5 2019 Stephen Smoogen <smooge@fedoraproject.org> - 4.4.3-3
- Fix BZ#1672027
- Patch for daemon did not have enough endif in them. However test looks superfluous

* Fri Feb 01 2019 Fedora Release Engineering <releng@fedoraproject.org> - 4.4.3-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_30_Mass_Rebuild

* Wed Jan 16 2019 Stephen Smoogen <smooge@fedoraproject.org> - 4.4.3-1
- Incorporate many fixes from Justin Paulsen <petaris@gmail.com> THANKS!!!
- Update to 4.4.3 for CVE fixes
- BZ#1661479
- BZ#1661480
- BZ#1665200
- BZ#1665201
- BZ#1665206
- BZ#1665207
- BZ#1665209
- BZ#1665210
- Fix BZ#1666209 Add RuntimeDirectory too systemd

* Fri Nov 30 2018 Stephen Smoogen <smooge@fedoraproject.org> - 4.4.2-3
- Remove systemd startup since built in works properly
- Incorporate fixes from patch14 into patch9

* Thu Nov 29 2018 Stephen Smoogen <smooge@fedoraproject.org> - 4.4.2-2
- Fix init-type and initdir for systemd and sysv

* Wed Nov 28 2018 Justin Paulsen <petaris@gmail.com> 4.4.2-1
- Bumped to version 4.4.2
- Updated patches 0001,0002,0003,0006,0009,0010,0011 to reflect upstream changes
- Updates to nagios.spec (this file) to cleanup un-needed elements and
  adjust/fix as required
- As a result of the cleanup I have added a patch nagios-0014-fix-resource.cfg-path.patch

* Tue Jul 24 2018 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.4-13
- Remove section which unset nagios Fix BZ#1568273
- Remove /etc/nagios/conf.d Fix BZ#1504306
- Change perms on dir Fix BZ#1579935
- Close BZ#1273154
- Hopefully Fix BZ#1201849
- Hopefully Fix BZ#1476238
- Hopefully Fix BZ#1494292

* Fri Jul 13 2018 Fedora Release Engineering <releng@fedoraproject.org> - 4.3.4-12
- Rebuilt for https://fedoraproject.org/wiki/Fedora_29_Mass_Rebuild

* Thu Jun 28 2018 Jitka Plesnikova <jplesnik@redhat.com> - 4.3.4-11
- Perl 5.28 rebuild

* Thu Apr 26 2018 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.4-10
- Fix systemd failures due to old versioning.

* Tue Feb 20 2018 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.4-9
- Add buildrequires for gcc

* Thu Feb 08 2018 Fedora Release Engineering <releng@fedoraproject.org> - 4.3.4-8
- Rebuilt for https://fedoraproject.org/wiki/Fedora_28_Mass_Rebuild

* Fri Nov 24 2017 Robert Scheck <robert@fedoraproject.org> - 4.3.4-7
- Fix initscript stop action for RHEL/CentOS 6 (#1515445 #c11)

* Fri Nov 24 2017 Robert Scheck <robert@fedoraproject.org> - 4.3.4-6
- Fix shell syntax error in initscript for RHEL/CentOS 6

* Mon Nov 20 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.4-5
- Fix entry for nagios not stopping correctly sometimes
- Try to make kill smarter. Needs to use the RH killproc instead

* Wed Oct  4 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.4-4
- Fix nagios su lines to work on rhel6

* Wed Sep 20 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.4-3
- Try to fix error on update with systemctl
- Find all the parts i forgot to send to dev null

* Tue Sep 19 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.4-3
- Fix conflict between nagios and nagios-contrib
- Fix reload problem in nagios systemd

* Tue Sep 19 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.4-2
- Merge patches from EPEL tree to master
- Fix contrib entries
- Use rpmlint to fix items in spec file

* Tue Sep 19 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.4-1
- Update to 4.3.4
- Rebase patches to 4.3.4
- Change configure to match upstream recommendations.
- Get the docs from doxygen added.
- Remove the rpmbuild in contrib so other arches can build

* Wed Aug  2 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-12
- Add a contrib section for event handlers
- Fix BZ#1476346
- Fix BZ#1476238
- Remove some contrib makes as not currently implemented for anything other than x86_64

* Wed Jul 26 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-11
- Fix a service problem again. Lost patch
- nagios script

* Wed Jul 26 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-10
- Fix fix

* Wed Jul 26 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-9
- Fix RHBZ#1475447
- How the henry do you mess that up over 4 releases?

* Tue Jul 25 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-8
- Fix the systemd service file reload and other issues

* Tue Jul 25 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-7
- Update initd patch to move mktemp from /tmp to /var/log/nagios where it has permission to write
- Update patches so they aren't offset

* Mon Jul 24 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-7
- And remember to push the release number up one
- Add entries for RHEL-6

* Fri Jul 21 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-6
- Fix nagios selinux entries

* Wed Jun 28 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-5
- Added fix for selinux from Patrick Uiterwijk

* Fri Jun 23 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-4
- Fix a problem with systemctl scripts. Mea culpa, mea culpa

* Wed Jun 14 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-3
- Update to latest in git
- Fix bug 1428111
- Fix bug 1426816
- Fix bug 1218320

* Mon Jun 05 2017 Jitka Plesnikova <jplesnik@redhat.com> - 4.3.2-2
- Perl 5.26 rebuild

* Thu May 11 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.2-1
- Updated from 4.3.1 maint to 4.3.2
- Update selinux

* Thu Apr 20 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.1-4git
- I broke several peoples names because the UTF-8 character was removed. My apologies.

* Thu Apr 20 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.1-3git
- fix selinux items
- change name to work of selinux module not to conflict with shipped one.

* Thu Apr 20 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.1-2
- Update to the maintenance version
- Add a starting selinux options
- Update patches to deal with newer code.

* Fri Feb 24 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.3.1-1
- Updated to 4.3.1
- Removed (de) entries in the spec file for 2 reasons.
  - Some characters break RHEL6
  - The text was diverging from the English and I don't trust google
    translate to put text that isn't insulting.
- And they renamed nagioscore-release to nagioscore-nagios. Ping/Pong
- Need to look at redoing pngs and such

* Fri Feb 10 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.2.4-4
- We find out that RHEL-6 does not like non-UTF so removed German translation

* Thu Feb  9 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.2.4-3
- Figure out that a mkdir only worked by accident on previous package. Move out of ifdef

* Tue Feb 07 2017 Stephen Smoogen <smooge@fedoraproject.org> - 4.2.4-2
- Pulled in version from COPR
- Remove lines which fedpkg lint did not like

* Thu Dec 08 2016 Justin Paulsen <petaris@gmail.com> - 4.2.4-1
- Bumped to version 4.2.4
- Fixed patches to match current version

* Mon Nov  7 2016 Stephen Smoogen <smooge@fedoraproject.org> - 4.2.2-1
- Bumped to version 4.2.2
- Fixed patches to match current version

* Fri Aug 12 2016 Kevin Fenzi <kevin@scrye.com> - 4.0.8-4
- Correct using sysvinit file instead of systemd unit

* Mon May 16 2016 Jitka Plesnikova <jplesnik@redhat.com> - 4.0.8-3
- Perl 5.24 rebuild

* Thu Feb 04 2016 Fedora Release Engineering <releng@fedoraproject.org> - 4.0.8-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Tue Dec 29 2015 Scott Wilkerson <swilkerson@nagios.com> - 4.0.8-2
- Fixed missing PID #1291555
- Fixed selinux permissions #1291734, #1291718

* Sat Sep 19 2015 Scott Wilkerson <swilkerson@nagios.com> - 4.0.8-1
- update to 4.0.8

* Wed Jun 17 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.5.1-9
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Sat Jun 06 2015 Jitka Plesnikova <jplesnik@redhat.com> - 3.5.1-8
- Perl 5.22 rebuild

* Thu Aug 28 2014 Jitka Plesnikova <jplesnik@redhat.com> - 3.5.1-7
- Perl 5.20 rebuild

* Sun Aug 17 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.5.1-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.5.1-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Fri Oct 25 2013 Peter Lemenkov <lemenkov@gmail.com> - 3.5.1-4
- Fixed main binary permissions

* Mon Oct 21 2013 Peter Lemenkov <lemenkov@gmail.com> - 3.5.1-3
- Fixed typo in *.service file

* Fri Oct 18 2013 Peter Lemenkov <lemenkov@gmail.com> - 3.5.1-2
- Drop EL5 support
- Added systemd support (rhbz #913550, also obsoletes rhbz #246990)
- Replace config.sub and config.guess instead of patching them

* Fri Aug 30 2013 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.5.1-1
- update to 3.5.1
- drop patch nagios-3.4.3-spaces-to-plus-signs.patch (upstream bug #407)

* Thu Aug 29 2013 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.5.0-9
- init script overwrites pid file unnecessarily (#983129)
- corrected bogus dates

* Sat Aug 03 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.5.0-8
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Wed Jul 24 2013 Petr Pisar <ppisar@redhat.com> - 3.5.0-7
- Perl 5.18 rebuild

* Fri Jul 19 2013 Keiran Smith <fedora@affix.me> - 3.5.0-6
- implimemt aarch64 patch from bug #926192

* Sat Jun 15 2013 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.5.0-5
- Build package with PIE flags (#965529)
- Insecure temporary file usage in nagios.upgrade_to_v3.sh (#958292)

* Tue Jun 11 2013 Remi Collet <rcollet@redhat.com> - 3.5.0-4
- rebuild for new GD 2.1.0

* Wed Apr 24 2013 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.5.0-3
- Add cfg_dir=/etc/nagios/conf.d to the main nagios configuration file
  (nagios-3.5.0-conf.d-configuration-directory.patch) (#907145#c5)
- Own the configuration directory /etc/nagios/conf.d (#907145#c5)
- Ship the internet.cfg configuration file as documentation (#907145#c5)

* Sat Apr 20 2013 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.5.0-2
- Patch nagios-3.4.3-spaces-to-plus-signs.patch (#952139)
  (upstream http://tracker.nagios.org/view.php?id=407)

* Sat Apr 20 2013 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.5.0-1
- Update to 3.5.0

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.4.4-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Mon Jan 21 2013 Adam Tkac <atkac redhat com> - 3.4.4-2
- rebuild due to "jpeg8-ABI" feature drop

* Sun Jan 13 2013 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.4.4-1
- Update to 3.4.4
- CVE-2012-6096 (#893269)

* Sun Jan 13 2013 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.4.3-5
- Refactored the patch nagios-0010-Added-several-images-to-the-sample-config.patch
  as patch cant create binary files (#875362).
  The old patch10 was replaced by nagios-0010-Added-several-images-to-the-sample-config-revb.patch
  and the PNG files included as sources 10, 11, and 12.

* Fri Dec 21 2012 Adam Tkac <atkac redhat com> - 3.4.3-4
- rebuild against new libjpeg

* Wed Dec  5 2012 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.4.3-3
- Use the Apache 2.4 RequireAll authorization container

* Tue Dec  4 2012 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.4.3-2
- Apache 2.4 configuration fix for Fedora 18+ (#871438);
  Patch nagios-3.4.3-httpd-2.4-and-2.2.patch

* Tue Dec  4 2012 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.4.3-1
- Upgrade to 3.4.3

* Sat Nov 10 2012 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.4.2-1
- Upgrade to 3.4.2

* Fri Jul 20 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.4.1-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Thu Jun 28 2012 Petr Pisar <ppisar@redhat.com> - 3.4.1-2
- Perl 5.16 rebuild

* Mon Jun 25 2012 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.4.1-1
- Upgrade to 3.4.1 (#835047)
- Dropped nagios-0012-Fixed-html-rss-install-files.patch

* Fri Jun 15 2012 Petr Pisar <ppisar@redhat.com> - 3.3.1-4
- Perl 5.16 rebuild

* Fri Feb 10 2012 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.3.1-3
- Move the nagios-commons usermod line to the main nagios package (#627527).

* Fri Feb 10 2012 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.3.1-2
- Add php to the requirements list (#519371, et al.).

* Tue Feb  7 2012 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.3.1-1
- Upgrade to 3.3.1 (#732329);
  includes fixes for CVE-2011-1523 and CVE-2011-2179 (#690880, #690881, #709874).
- Make nagios-common own the /usr/lib{,64}/nagios/plugins directories (#756839).
- Change the ownership of /etc/nagios to the nagios-common package (#756839).
- Retab (tabs -> spaces).

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.2.3-13
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Tue Dec 06 2011 Adam Jackson <ajax@redhat.com> - 3.2.3-12
- Rebuild for new libpng

* Tue Jun 21 2011 Marcela Maslanova <mmaslano@redhat.com> - 3.2.3-11
- Perl mass rebuild

* Wed Mar 23 2011 Jan ONDREJ (SAL) <ondrejj(at)salstar.sk> - 3.2.3-10
- Rebuild against new mysql.

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.2.3-9
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Tue Jan 25 2011 Peter Lemenkov <lemenkov@gmail.com> - 3.2.3-8
- Fixed strange permission on executables (see rhbz #672074)
- Dropped permissions on directories with log-files (see rhbz #672074)

* Tue Nov 23 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.3-7
- Finally fixed path to CGI (rhbz #653291)
- Added runtime dependency - mailx (see rhbz #655541)
- Added more images to the sample config

* Thu Nov 18 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.3-6
- Fixed path to passwd file in Apaches config file

* Tue Nov 16 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.3-5
- Fix building on EL-5

* Mon Nov 15 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.3-4
- Fix path to CGI (rhbz #653291)

* Wed Nov  3 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.3-3
- Disable stripping of binaries (see rhbz #648223).

* Wed Oct 27 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.3-2
- Accidentally forgotten patches added back

* Tue Oct 26 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.3-1
- Ver. 3.2.3
- Further cleanups in spec-file

* Wed Sep 29 2010 jkeating - 3.2.2-2
- Rebuilt for gcc bug 634757

* Sat Sep 11 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.2-1
- Ver. 3.2.2 (rhbz #629439).
- Cleanup spec-file
- Ensure that %%{_sysconfdir}/httpd/conf.d/nagios.conf points to
  the actual passwd file (see rhbz #576571).

* Tue Aug 24 2010 Adam Tkac <atkac redhat com> - 3.2.1-6
- rebuild

* Wed Jun  9 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.1-5
- Removed obsoletes: nagios < 3.2.1-2

* Tue Jun 01 2010 Marcela Maslanova <mmaslano@redhat.com> - 3.2.1-4
- Mass rebuild with perl-5.12.0

* Mon May 17 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.1-3
- Fixed severe issue with uninstalling main nagios package while
  updating (see rhbz #590709 for details).

* Sun Apr 25 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.1-2
- Created 'common' subpackage for gid/uid and common directory

* Sat Mar 13 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.1-1
- Upgrade to 3.2.1 (#572587).
- Fixed SELinux patch (#573119).

* Thu Feb 25 2010 Peter Lemenkov <lemenkov@gmail.com> - 3.2.0-4
- The package builds now with distro CFLAGS, CXXFLAGS (see bz #520979)
- Fixed returning status for init-script (see bz #546561)
- Fixed selinux issue with writing of PID-file (see bz #548638 and bz #539963)
- Fixed build on EPEL (see bz #526817)

* Mon Dec  7 2009 Stepan Kasal <skasal@redhat.com> - 3.2.0-3
- rebuild against perl 5.10.1

* Mon Aug 17 2009 Mike McGrath <mmcgrath@redhat.com> - 3.2.0-2
- s/datarootdir/datadir/

* Sun Aug 16 2009 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.2.0-1
- Upgrade to 3.2.0 (#517210).

* Fri Jul 24 2009 Jose Pedro Oliveira <jpo at di.uminho.pt> - 3.1.2-3
- Corrected the package version in the last two changelog entries (#499853)
- Using configure --datarootdir option instead of --datadir (#499853)
  (fixes the physical_html_path value in cgi.cfg)
- Fixes permissions to the new php configuration file config.inc.php (#499853)
- Re-enables the httpd requirement as its removal caused several problems
  (see #487411 for more information)

* Wed Jul 15 2009 Mike McGrath <mmcgrath@redhat.com> 3.1.2-2
- Release bump for rebuild

* Mon Jun 29 2009  Robert M. Albrecht <fedora@romal.de> 3.1.1-1
- Upstream released a new version

* Mon Jun 22 2009 Mike McGrath <mmcgrath@redhat.com> - 3.0.6-4
- Removing httpd requires for #487411

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.0.6-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Sun Jan 11 2009  Robert M. Albrecht <fedora@romal.de> 3.0.6-2
- I messed my CVS up

* Tue Dec 02 2008  Robert M. Albrecht <fedora@romal.de> 3.0.6-1
- Upstream released a new version

* Mon Nov 24 2008 Mike McGrath <mmcgrath@redhat.com> 3.0.5-1
- Upstream released a new version

* Mon Oct 20 2008 Robert M. Albrecht <romal@gmx.de> 3.0.4-3
- Fixed a typo introduced in fixing Bugzilla 461087

* Mon Oct 20 2008 Robert M. Albrecht <romal@gmx.de> 3.0.4-2
- Bugzilla 461087 wrong path for icons

* Mon Oct 20 2008 Robert M. Albrecht <romal@gmx.de> 3.0.4-1
- Upstream released 3.0.4
- Fixed two typos in nagios.spec

* Sun Sep 28 2008 Mike McGrath <mmcgrath@redhat.com> 3.0.3-9
- License fix

* Wed Jul 23 2008 Robert M. Albrecht <romal@gmx.de> 3.0.3-8
- Matthew Jurgens provided a script and Readme for upgrading your config to Release 3

* Tue Jul 22 2008 Robert M. Albrecht <romal@gmx.de> 3.0.3-7
- Added summary
- Added german translations

* Thu Jul 10 2008 Robert M. Albrecht <romal@gmx.de> 3.0.3-6
- disabled conf.d in nagios.conf for now

* Thu Jul 10 2008 Robert M. Albrecht <romal@gmx.de> 3.0.3-5
- Killed BuildRequirements for unsupported Fedora releases

* Wed Jul 02 2008 Robert M. Albrecht <romal@gmx.de> 3.0.3-4
- renamed preconfigured passwd to .htpasswd to enable Apaches .ht* protection
- added default .htpasswd with user nagiosadmin password nagiosadmin

* Tue Jul 01 2008 Robert M. Albrecht <romal@gmx.de> 3.0.3-3
- Added Apache style conf.d
- Added a working example config named internet.cfg
- The object folder was created twice

* Tue Jul 01 2008 Robert M. Albrecht <romal@gmx.de> 3.0.3-2
- Fixed folder /var/log/nagios/spool/checkresults
- silenced rpmlint (tabs, spaces)
- silenced rpmlint (configure missing libdir)

* Sun Jun 29 2008 Robert M. Albrecht <romal@gmx.de> 3.0.3-1
- Upstream released 3.0.3

* Sun Jun 22 2008 Robert M. Albrecht <romal@gmx.de> 3.0.2-1
- Upstream released 3.0.2

* Mon May 26 2008 Shawn Starr <shawn.starr@rogers.com> 2.12-3
- Fix spec seems to break for Fedora 10+

* Fri May 23 2008 Shawn Starr <shawn.starr@rogers.com> 2.12-2
- Put back fix for Bugzilla #233887

* Fri May 23 2008 Shawn Starr <shawn.starr@rogers.com> 2.12-1
- Upstream released 2.12
- Fixes CVE-2007-5803 XSS issues, Bugzilla #445512

* Tue Mar 18 2008 Tom "spot" Callaway <tcallawa@redhat.com> 2.11-3
- add Requires for versioned perl (libperl.so)
- get rid of pointless file Requires

* Mon Mar 17 2008 Mike McGrath <mmcgrath@redhat.com> 2.11-2
- Upstream released new version
- Added perl-ExtUtils-Embed

* Tue Feb 12 2008 Mike McGrath <mmcgrath@redhat.com> 2.10-6
- Rebuild for gcc43

* Thu Nov 29 2007 Mike McGrath <mmcgrath@redhat.com> 2.10-5
- Upstream released 2.10
- Renamed cfg-sample configs to just .cfg
- Added BR of perl-devel, libjpeg-devel, libpng-devel

* Wed Sep 26 2007 Mike McGrath <mmcgrath@redhat.com> 2.9-5
- rebuild for koji test

* Sat Sep 08 2007 Mike McGrath <mmcgrath@redhat.com> 2.9-4
- rebuild

* Wed Aug 22 2007 Mike McGrath <mmcgrath@redhat.com> 2.9-3
- Rebuild for ppc32 and license

* Tue Jul 10 2007 Mike McGrath <mmcgrath@redhat.com> 2.9-2
- Release bump

* Fri Jun 29 2007 Mike McGrath <mmcgrath@redhat.com> 2.9-1
- Upstream released 2.9

* Tue Feb 06 2007 Mike McGrath <imlinux@gmail.com> 2.7-2
- Upstream released 2.7

* Thu Nov 30 2006 Mike McGrath <imlinux@gmail.com> 2.6-1
- Upstream released 2.6

* Thu Sep 07 2006 Mike McGrath <imlinux@gmail.com> 2.5-3
- Release bump for mass rebuild

* Wed Aug 02 2006 Mike McGrath <imlinux@gmail.com> 2.5-2
- Fixed default permissions for private and the resource file

* Fri Jul 14 2006 Mike McGrath <imlinux@gmail.com> 2.5-1
- Upstream released 2.5

* Mon Jun 26 2006 Mike McGrath <imlinux@gmail.com> 2.4-2
- Added /usr/bin/nagiostats bz# 194461

* Sun Jun 04 2006 Mike McGrath <imlinux@gmail.com> 2.4-1
- Upstream released 2.4
- Cleaned up changelog

* Mon May 15 2006 Mike McGrath <imlinux@gmail.com> 2.3.1-1
- Bug fix for HTTP content_length header integer overflow in CGIs
- Updates no longer remove Nagios from starting up on reboot

* Tue May 09 2006 Mike McGrath <imlinux@gmail.com> 2.3-3
- updates to the init script that prevented nagios from shutting down

* Wed May 03 2006 Mike McGrath <imlinux@gmail.com> 2.3-1
- Upstream released 2.3
- Bug fix for negative HTTP content_length header in CGIs
- Added missing links for notes_url and action_url to service column of status detail page 

* Tue May 02 2006 Mike McGrath <imlinux@gmail.com> 2.2-3
- Upstream released 2.2

* Tue Feb 21 2006 Mike McGrath <imlinux@gmail.com> 2.0-1
- Upstream released 2.0 (changes below)
- Fix for segfault in timed event queue
- Removed length limitations for object vars/vals
- Updated config.sub and config.guess to versions from automake-1.9
- Doc updates

* Sat Feb 04 2006 Mike McGrath <imlinux@gmail.com> 2.0-0.2.rc2
- Fixed default options in Apache config

* Fri Jan 27 2006 Mike McGrath <imlinux@gmail.com> 2.0-0.1.rc2
- Using 2.0rc2 tarball

* Thu Jan 26 2006 Mike McGrath <imlinux@gmail.com> 1.3-15
- Fixed usermod -a issue, Bugzilla #49609

* Sat Jan 15 2005 Mike McGrath <imlinux@gmail.com> 1.3-14
- Fedora friendly spec file

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

* Fri Jun 14 2002 Ethan Galstad <nagios@nagios.org) (1.0b4)
- Modified requirements to work when installed using KickStart (Jeff Frost)
- Changed method used for checking for user/group existence (Jeff Frost)

* Wed May 15 2002 Ethan Galstad <nagios@nagios.org) (1.0b1)
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

# vim:set ai ts=4 sw=4 sts=4 et:
