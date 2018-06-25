Name: tsar-devel
Version: 2.1.1
Release: %(echo $RELEASE)%{?dist}
Summary: Taobao Tsar Devel
Group: Taobao/Common
License: Commercial

%description
devel package include tsar header files and module template for the development

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
cd $OLDPWD/../
make DESTDIR=$RPM_BUILD_ROOT tsardevel
make DESTDIR=$RPM_BUILD_ROOT tsarluadevel

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/local/tsar/devel/tsar.h
/usr/local/tsar/devel/Makefile.test
/usr/local/tsar/devel/mod_test.c
/usr/local/tsar/devel/mod_test.conf
/usr/local/tsar/luadevel/Makefile.test
/usr/local/tsar/luadevel/mod_lua_test.lua
/usr/local/tsar/luadevel/mod_lua_test.conf
%attr(755,root,root)
/usr/bin/tsardevel
/usr/bin/tsarluadevel

%changelog

