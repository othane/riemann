%define module nwriemann
%define version 0.1.0.0
%define scratch /tmp/fedora_package_scratch

%define is_rh   %(test -e /etc/redhat-release && echo 1 || echo 0)
%define is_fc   %(test -e /etc/fedora-release && echo 1 || echo 0)
%define is_mdk  %(test -e /etc/mandrake-release && echo 1 || echo 0)
%define is_suse %(test -e /etc/SuSE-release && echo 1 || echo 0)

Summary: nwriemann dkms package
Name: %{module}
Version: %{version}
Release: 2dkms
Vendor: Down Under Ind.
License: GPL
Packager: Daniel Newton <djpnewton@gmail.com>
Group: System Environment/Base
BuildArch: noarch
Requires: bash, gcc, dkms >= 1.00
%if %{is_suse}
Requires: make, kernel-syms
%endif

# There is no Source# line for dkms.conf since it has been placed
# into the source tarball of SOURCE0
Source0: %{module}-%{version}-src.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root/

%description
This package contains the hid-nwriemann module wrapped for
the DKMS framework.

%prep
rm -rf %{module}-%{version}
mkdir %{module}-%{version}
cd %{module}-%{version}
tar xvzf %{scratch}/%{module}-%{version}-src.tar.gz

%install
if [ "$RPM_BUILD_ROOT" != "/" ]; then
	rm -rf $RPM_BUILD_ROOT
fi
mkdir -p $RPM_BUILD_ROOT/usr/src/%{module}-%{version}/
cp -rf %{module}-%{version}/* $RPM_BUILD_ROOT/usr/src/%{module}-%{version}

mkdir -p $RPM_BUILD_ROOT/etc/udev/rules.d
cp %{scratch}/40-nwriemann.rules $RPM_BUILD_ROOT/etc/udev/rules.d
mkdir -p $RPM_BUILD_ROOT/etc/init.d
cp %{scratch}/nwriemann_udev $RPM_BUILD_ROOT/etc/init.d
mkdir -p $RPM_BUILD_ROOT/etc/rc5.d
ln -sf /etc/init.d/nwriemann_udev $RPM_BUILD_ROOT/etc/rc5.d/S95nwriemann
mkdir -p $RPM_BUILD_ROOT/etc/nwriemann
cp %{scratch}/load_riemann.py $RPM_BUILD_ROOT/etc/nwriemann

%clean
if [ "$RPM_BUILD_ROOT" != "/" ]; then
	rm -rf $RPM_BUILD_ROOT
fi

%files
%defattr(-,root,root)
/usr/src/%{module}-%{version}/

/etc/udev/rules.d/40-nwriemann.rules
/etc/init.d/nwriemann_udev
/etc/rc5.d/S95nwriemann
/etc/nwriemann/load_riemann.py

%pre

%post
dkms add -m %{module} -v %{version} --rpm_safe_upgrade

# try building the module for the current kernel
if [ `uname -r | grep -c "BOOT"` -eq 0 ] && [ -e /lib/modules/`uname -r`/build/include ]; then
	dkms build -m %{module} -v %{version}
	dkms install -m %{module} -v %{version}
elif [ `uname -r | grep -c "BOOT"` -gt 0 ]; then
	echo -e ""
	echo -e "Module build for the currently running kernel was skipped since you"
	echo -e "are running a BOOT variant of the kernel."
else
	echo -e ""
	echo -e "Module build for the currently running kernel was skipped since the"
	echo -e "kernel source for this kernel does not seem to be installed."
fi
exit 0

%preun
echo -e
echo -e "Uninstall of nwfermi module (version %{version}) beginning:"
dkms remove -m %{module} -v %{version} --all --rpm_safe_upgrade
exit 0
