%define name jftpgw
%define version 0.13.beta.j
%define release 7
%define prefix /usr

# for install: should scripts used included in tar ball or extra
%define scripts_in_tarball 1

Summary: An FTP proxy/gateway server
Summary(de): Ein FTP Proxy Server
Name: %{name}
Version: %{version}
Release: %{release}
Copyright: GPL
Group: Network/Proxies
URL: http://www.mcknight.de/%{name}/

BuildRoot: %{_tmppath}/%{name}-%{version}-root
Prereq: /sbin/chkconfig

Source: http://www.mcknight.de/%{name}/%{name}-%{version}.tar.gz
%if %{scripts_in_tarball}
# nothing to do
%else
# scripts must be located in SOURCES
Source1: jftpgw.init
Source2: jftpgw.xinetd
%endif

%package standalone
Summary:  jftpgw -- Setup for standalone operation.
Group: Network/Proxies
Requires: jftpgw
Conflicts: jftpgw-xinetd

%package xinetd
Summary:  jftpgw -- Setup for xinetd operation.
Group: Network/Proxies
Requires: jftpgw xinetd >= 2.3.3
Conflicts: jftpgw-standalone

%description
jftpgw is a proxy server for the FTP protocol. If it is running on a
machine, you can use a standard FTP client to connect to that machine. You
log in with remoteuser@destination with the password of the user on the
destination FTP server and the FTP session is forwarded from the `real' FTP
server to the jftpgw proxy and from there to your client. You can also
specify the destination port on the login string. jftpgw supports
passive/active FTP, access restrictions (based on source IP, target IP and
the user name), user rewriting and tries to drop its privileges as often as
possible. You can run jftpgw as a normal user as well.

%description -l de
jftpgw ist ein FTP proxy Server, der Verbindungen zwischen einem FTP Server
und einem FTP Client weiterleitet. Der FTP Client verbindet sich zu dem
jftpgw Rechner auf dessen Port und sendet den Usernamen im Format
"user@zielrechner", damit der Proxy sich als "user" auf dem Rechner
"zielrechner" einloggt und auch das Passwort weiterleitet. jftpgw
unterstützt aktive/passives FTP, Zugangsbeschränkungen (basierend auf
Herkunfts-IP, Ziel-IP und Username), Ersetzung von Usernamen sowie das
Wechseln auf weniger berechtigte Benutzer. jftpgw kann auch von "normalen"
Benutzern ausgeführt werden.

%description standalone
needed to start jftpgw (FTP proxy/gateway) in standalone mode

%description -l de standalone
notwendig, um jftpgw (FTP proxy/gateway) als Serverdienst (standalone) laufen zu lassen.

%description xinetd
needed to start jftpgw (FTP proxy/gateway) by xinetd

%description -l de xinetd
notwendig, um jftpgw (FTP proxy/gateway) von xinetd starten zu lassen.

%prep
%setup -q

%build
CFLAGS=$RPM_OPT_FLAGS ./configure --prefix=%{prefix} --with-logpath=/var/log --with-confpath=/etc --bindir=%{prefix}/sbin
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man1

make install DESTDIR=$RPM_BUILD_ROOT
cp $RPM_BUILD_ROOT/etc/jftpgw.conf $RPM_BUILD_ROOT/etc/jftpgw.conf.sample

# Buggy make install, must help here/PB
install -c -m 644 jftpgw.1 $RPM_BUILD_ROOT%{_mandir}/man1
install -d -m 755 $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}/
install -c -m 644 TODO ChangeLog $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}

# for standalone
install -d $RPM_BUILD_ROOT/etc/rc.d/init.d
%if %{scripts_in_tarball}
install -c -m 755 support/jftpgw.init $RPM_BUILD_ROOT/etc/rc.d/init.d/jftpgw
%else
install -c -m 755 %SOURCE1 $RPM_BUILD_ROOT/etc/rc.d/init.d/jftpgw
%endif

# for xinetd
install -d $RPM_BUILD_ROOT/etc/xinetd.d
%if %{scripts_in_tarball}
install -c -m 644 support/jftpgw.xinetd $RPM_BUILD_ROOT/etc/xinetd.d/jftpgw
%else
install -c -m 644 %SOURCE2 $RPM_BUILD_ROOT/etc/xinetd.d/jftpgw
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%doc COPYING README TODO ChangeLog
%attr(0755,root,root) %{prefix}/sbin/jftpgw
%attr(0644,root,root) %config(noreplace) /etc/jftpgw.conf
%attr(0644,root,root) %config /etc/jftpgw.conf.sample
%{_mandir}/man1/jftpgw.1*

%files standalone
%defattr(-, root, root)
%attr(0755,root,root) %config /etc/rc.d/init.d/jftpgw
 
%files xinetd
%defattr(-, root, root)
%attr(0644,root,root) %config(noreplace) /etc/xinetd.d/jftpgw

%post
if [ "$1" = "0" ]; then
cat <<ENDpost

To run jftpgw you have to install either
 jftpgw-standalone (running in daemon mode)
or
 jftpgw-xinetd (started by xinetd)
or
 add related line in /etc/inetd.conf (started by inetd)

ENDpost
fi

%post xinetd
echo "Reload xinetd services..."
service xinetd reload

%post standalone
/sbin/chkconfig --add jftpgw

%preun standalone
if [ $1 = 0 ]; then
	service jftpgw stop >/dev/null 2>&1
	/sbin/chkconfig --del jftpgw
fi

%postun standalone
if [ "$1" -ge "1" ]; then
	service jftpgw condrestart >/dev/null 2>&1
fi

%postun xinetd
if [ "$1" = "0" ]; then
	if [ -f /etc/xinetd.d/jftpgw ]; then
		echo "WARNING: /etc/xinetd.d/jftpgw still exist, service cannot be disabled"
		echo "Remove this file by hand or move it out from this directory and reload xinetd service using"
		echo " 'service xinetd reload'"
	else
		echo "Reload xinetd services..."
		service xinetd reload
	fi
fi

%changelog
* Tue Dec 25 2001  Joachim Wieland <joe@mcknight.de>
- Added mkdir to create manpage directory, renamed it from man to man1

* Thu Nov 22 2001  Dr. Peter Bieringer <pbieringer@aerasec.de>
- some enhancements on pre/post scripts
- make location of scripts (tarball/SOURCES) switchable
- fix permission of xinetd.d/jftpgw to 644
- fix permission of sbindir/jftpgw 755, also configs to 644 to make jftpgw runned by xinetd/nobody (not really good, perhaps a dedicated group will be better -> todo)
- set user nobody in xinetd example

* Sun Nov 18 2001  Joachim Wieland <joe@mcknight.de>
- adapted to autoconf and integrated into the distribution

* Thu Nov 15 2001  Dr. Peter Bieringer <pbieringer@aerasec.de>
- based on 0.12.2-2 this new spec file is created for 0.13
