%define debug_package %{nil}
%define _build_id_links none

%define git_version @VERSION@

Name:           hyperion
Version:        %{git_version}
Release:        0
Summary:        The successor to Hyperion aka Hyperion Next Generation

License:        MIT
URL:            https://github.com/hyperion-project/%{name}.ng
Source:         %{name}.ng.tar.gz

BuildArch:      x86_64
BuildRoot:      %{_tmppath}/%{name}.ng-build
Vendor:         Hyperion Project
Packager:       Hyperion-Project <admin@hyperion-project.org>

BuildRequires:  @BUILD_REQUIRES@
Requires:       @PACKAGE_REQUIRES@

%description
Hyperion is an opensource Bias or Ambient Lighting implementation
which you might know from TV manufacturers.
It supports many LED devices and video grabbers.

%prep
%setup -q -n %{name}.ng

%build
mkdir -p build
cd build
cmake -DUSE_SYSTEM_PROTO_LIBS=ON -DUSE_SYSTEM_FLATBUFFERS_LIBS=ON -DUSE_SYSTEM_MBEDTLS_LIBS=ON -DENABLE_DEPLOY_DEPENDENCIES=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
cd build
make install DESTDIR=%{buildroot}
%{__mkdir} -p %{buildroot}%{_unitdir}
sed -i '12d' %{buildroot}%{_prefix}/share/hyperion/service/%{name}.systemd
sed -i 's/  for user \%i//g' %{buildroot}%{_prefix}/share/hyperion/service/%{name}.systemd
%{__install} -m644 %{buildroot}%{_prefix}/share/hyperion/service/%{name}.systemd %{buildroot}%{_unitdir}/%{name}.service
rm -rf %{buildroot}%{_prefix}/share/hyperion/service %{buildroot}%{_prefix}/share/hyperion/desktop %{buildroot}%{_prefix}/share/hyperion/icons

%clean
%__rm -rf %{buildroot}

%post
%if 0%{?systemd_post:1}
  if [ $1 -gt 1 ] ; then
    /usr/bin/systemctl try-restart %{name}.service >/dev/null 2>&1 || :
  else
    %systemd_post %{name}.service
  fi
%else
  /bin/systemctl daemon-reload &>/dev/null || :
%endif

%preun
%if 0%{?systemd_preun:1}
  %systemd_preun %{name}.service
%else
  if [ $1 -eq 0 ] ; then
    # Package removal, not upgrade
    /bin/systemctl --no-reload disable %{name}.service > /dev/null 2>&1 || :
    /bin/systemctl stop %{name}.service > /dev/null 2>&1 || :
  fi
%endif

%postun
%if 0%{?systemd_post:1}
  %systemd_postun_with_restart %{name}.service
%else
  /bin/systemctl daemon-reload &>/dev/null
  [ $1 -gt 0 ] && /bin/systemctl try-restart %{name}.service &>/dev/null || :
%endif

%posttrans
if [ $1 -eq 1 ]; then
  # Package install, not upgrade
  /bin/systemctl -q enable %{name}.service > /dev/null 2>&1 || :
  /bin/systemctl start %{name}.service >/dev/null 2>&1 || :
fi

%files
%defattr(-,root,root)
%{_prefix}/bin/*
%{_prefix}/share/*
%{_unitdir}/*.service

%changelog
