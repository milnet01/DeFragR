#
# spec file for package defragr: openSUSE and Fedora
#
# Copyright (c) 2026 Anthony Schemel
# SPDX-License-Identifier: MIT
#

%global nasm_version 3.02
%global nasm_sha256 87336eba53b4acfe917424ab5d500d2b0054d9f5148d35c2273ccf2cfb712f0d

# Leap 15's default compiler is GCC 7, too old for C++23.
%if 0%{?suse_version} && 0%{?suse_version} < 1600
%global cxx_pkg gcc13-c++
%global cxx_arg -DCMAKE_CXX_COMPILER=g++-13
%else
%global cxx_pkg gcc-c++
%global cxx_arg %{nil}
%endif

Name:           defragr
Version:        0.2.1
Release:        0%{?dist}
Summary:        Defragmenter for ext4 drives
License:        MIT
URL:            https://github.com/milnet01/DeFragR
Source0:        %{name}-%{version}.tar.gz
# Build-only: no distribution ships NASM 3.02, the engine's floor. Built in
# the build tree and never installed.
Source1:        https://www.nasm.us/pub/nasm/releasebuilds/%{nasm_version}/nasm-%{nasm_version}.tar.xz
Source99:       %{name}-rpmlintrc
# The engine is x86-64 assembly.
ExclusiveArch:  x86_64
BuildRequires:  %{cxx_pkg}
BuildRequires:  cmake >= 3.28
BuildRequires:  desktop-file-utils
BuildRequires:  gcc
BuildRequires:  hicolor-icon-theme
BuildRequires:  make
BuildRequires:  cmake(Qt6DBus) >= 6.4
BuildRequires:  cmake(Qt6LinguistTools) >= 6.4
BuildRequires:  cmake(Qt6Widgets) >= 6.4
# The window runs the engine through pkexec.
Requires:       /usr/bin/pkexec
# openSUSE's rpmlint fails the build on this one
# (branding-requires-unversioned).
%if !0%{?suse_version}
Requires:       hicolor-icon-theme
%endif

%description
DeFragR shows how fragmented an ext4 drive is, lists its worst files and
draws the drive as a map. A small engine written in x86-64 assembly reads
the drive; a Qt 6 window shows the results.

%prep
echo "%{nasm_sha256}  %{SOURCE1}" | sha256sum --check --strict
%setup -q -a 1

%build
# NASM: a build tool only, so the distribution's hardening flags are left out.
pushd nasm-%{nasm_version}
env -u CFLAGS -u CXXFLAGS -u LDFLAGS -u CPPFLAGS ./configure
make %{?_smp_mflags} nasm
popd

export LDFLAGS="${LDFLAGS:-%{?build_ldflags}}"
export CXXFLAGS="${CXXFLAGS:-%{optflags}}"
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DBUILD_TESTING=OFF \
    -DDEFRAGR_BUILD_ID=obs \
    -DCMAKE_ASM_NASM_COMPILER="$PWD/nasm-%{nasm_version}/nasm" \
    -DCMAKE_SKIP_INSTALL_RPATH=ON \
    %{cxx_arg}
cmake --build build --verbose -- %{?_smp_mflags}

%install
DESTDIR=%{buildroot} cmake --install build

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop

%files
%license LICENSE
%doc README.md CHANGELOG.md
%{_bindir}/%{name}
%{_bindir}/%{name}-engine
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/scalable/apps/%{name}.svg
%{_datadir}/icons/hicolor/512x512/apps/%{name}.png

%changelog
