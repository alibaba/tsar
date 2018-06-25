Name: tsar
Version: 2.1.1
Release: 19200.aone
Summary: Taobao System Activity Reporter
URL: https://github.com/alibaba/tsar
Group: Taobao/Common
License: Commercial
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Source: tsar-%{version}.tar.gz

%description
tsar is Taobao monitor tool for collect system activity status, and report it.
It have a plugin system that is easy for collect plugin development. and may
setup different output target such as local logfile and remote nagios host.

%prep
%setup -q

%build
make clean;make

%install

rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/local/tsar/
mkdir -p %{buildroot}/usr/local/tsar/modules/
mkdir -p %{buildroot}/usr/local/tsar/conf/
mkdir -p %{buildroot}/usr/local/man/man8/
mkdir -p %{buildroot}/etc/logrotate.d/
mkdir -p %{buildroot}/etc/tsar/
mkdir -p %{buildroot}/etc/cron.d/
mkdir -p %{buildroot}/usr/bin

install -p -D -m 0755 src/tsar  %{buildroot}/usr/bin/tsar
install -p -D -m 0644 conf/tsar.conf %{buildroot}/etc/tsar/tsar.conf
install -p -D -m 0644 modules/*.so %{buildroot}/usr/local/tsar/modules/
install -p -D -m 0644 conf/tsar.cron %{buildroot}/etc/cron.d/tsar
install -p -D -m 0644 conf/tsar.logrotate %{buildroot}/etc/logrotate.d/tsar
install -p -D -m 0644 conf/tsar.8 %{buildroot}/usr/local/man/man8/tsar.8

make -C lualib INSTALL_DIR=%{buildroot}/usr/local/tsar/lualib install

make -C lualib INSTALL_DIR=%{buildroot}/usr/local/tsar/lualib install

%clean
[ "%{buildroot}" != "/" ] && %{__rm} -rf %{buildroot}


%files
%defattr(-,root,root)
/usr/local/tsar/modules/*.so
/usr/local/tsar/lualib

%attr(755,root,root) /usr/bin/tsar
%config(noreplace) /etc/tsar/tsar.conf
%attr(644,root,root) /etc/cron.d/tsar
%attr(644,root,root) /etc/logrotate.d/tsar
%attr(644,root,root) /usr/local/man/man8/tsar.8

%changelog
* Sun Jan  6 2013 Ke Li <kongjian@taobao.com>
- merge inner and opensource tsar
* Thu Dec  9 2010 Ke Li <kongjian@taobao.com>
- add logrotate for tsar.data
* Mon Apr 26 2010 Bin Chen <kuotai@taobao.com>
- first create tsar rpm package
