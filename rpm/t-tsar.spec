##############################################################
# http://baike.corp.taobao.com/index.php/%E6%B7%98%E5%AE%9Drpm%E6%89%93%E5%8C%85%E8%A7%84%E8%8C%83 # 
# http://www.rpm.org/max-rpm/ch-rpm-inside.html              #
##############################################################
Name: t-tsar
Version: 1.0.0
Release: %(echo $RELEASE)%{?dist} 
# if you want use the parameter of rpm_create on build time,
# uncomment below
Summary: Please write somethings about the package here in English. 
Group: alibaba/application
License: Commercial
AutoReqProv: none
%define _prefix /home/a/project/t-tsar 

# uncomment below, if depend on other packages

#Requires: package_name = 1.0.0


%description
# if you want publish current svn URL or Revision use these macros
请你在这里描述一下关于此包的信息,并在上面的Summary后面用英文描述一下简介。

%debug_package
# support debuginfo package, to reduce runtime package size

# prepare your files
%install
# OLDPWD is the dir of rpm_create running
# _prefix is an inner var of rpmbuild,
# can set by rpm_create, default is "/home/a"
# _lib is an inner var, maybe "lib" or "lib64" depend on OS

# create dirs
mkdir -p .%{_prefix}
cd $OLDPWD/../;
make;
make install DESTDIR=${RPM_BUILD_ROOT}/%{_prefix};

# create a crontab of the package
#echo "
#* * * * * root /home/a/bin/every_min
#3 * * * * ads /home/a/bin/every_hour
#" > %{_crontab}

# package infomation
%files
# set file attribute here
%defattr(-,root,root)
# need not list every file here, keep it as this
%{_prefix}
## create an empy dir

# %dir %{_prefix}/var/log

## need bakup old config file, so indicate here

# %config %{_prefix}/etc/sample.conf

## or need keep old config file, so indicate with "noreplace"

# %config(noreplace) %{_prefix}/etc/sample.conf

## indicate the dir for crontab

# %{_crondir}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig 

%changelog
* Tue Jul 7 2015 xiaokaikai.xk 
- add spec of t-tsar
