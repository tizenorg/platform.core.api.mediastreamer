Name:       capi-media-streamer
Summary:    A Media Streamer library in Tizen Native API
Version:    0.1.3
Release:    0
Group:      Multimedia/API
License:    Apache-2.0
URL:        http://source.tizen.org
Source0:    %{name}-%{version}.tar.gz
Source1001:     capi-media-streamer.manifest
BuildRequires:  cmake
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  pkgconfig(capi-media-tool)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  pkgconfig(gstreamer-plugins-base-1.0)
BuildRequires:  pkgconfig(gstreamer-video-1.0)
BuildRequires:  pkgconfig(gstreamer-app-1.0)
BuildRequires:  pkgconfig(iniparser)
BuildRequires:  pkgconfig(bundle)

%description
A MediaStreamer library in Tizen Native API.

%package devel
Summary:    Multimedia Streamer Library in Tizen Native API (Development)
Group:      TO_BE/FILLED_IN
Requires:   %{name} = %{version}-%{release}

%description devel
MediaStreamer Library in Tizen Native API (DEV).

%prep
%setup -q
cp %{SOURCE1001} .

%define ini_path /usr/etc/media_streamer.ini

%build
flags="-DMEDIA_STREAMER_INI_PATH=\\\"%{ini_path}\\\""

%if 0%{?sec_build_binary_debug_enable}
flags="$flags -DTIZEN_DEBUG_ENABLE"
%endif

export CFLAGS="$CFLAGS $flags"
export CXXFLAGS="$CXXFLAGS $flags"
export FFLAGS="$FFLAGS $flags"

MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
%cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DFULLVER=%{version} -DMAJORVER=${MAJORVER}

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_datadir}/license
mkdir -p %{buildroot}/usr/bin
cp LICENSE.APLv2 %{buildroot}%{_datadir}/license/%{name}
cp test/media_streamer_test %{buildroot}/usr/bin

mkdir -p %{buildroot}/usr/etc
cp -rf config/media_streamer.ini %{buildroot}%{ini_path}

%make_install

%post
/sbin/ldconfig
chsmack -a "_" %{ini_path}

%postun -p /sbin/ldconfig


%files
%manifest %{name}.manifest
%license LICENSE.APLv2
%{_libdir}/lib%{name}.so.*
%{_datadir}/license/%{name}
%{_bindir}/*
%config %{ini_path}
%attr(755, -, root) %{ini_path}

%files devel
%manifest %{name}.manifest
%{_includedir}/media/*.h
%{_libdir}/pkgconfig/*.pc
%{_libdir}/lib%{name}.so
