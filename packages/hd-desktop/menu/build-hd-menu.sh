#!/usr/bin/env bash
# Builds hd-menu in a Debian container; ../Makefile runs it with docker,
# with this directory, menu/, in /src, read only. It installs the build
# dependencies, compiles hd-menu with the hardening flags of Debian and copies
# what the package needs to /out:
#
#   /out/hd-menu             the binary
#   /out/depends             the Depends of the package, from dpkg-shlibdeps
#
# Everything in /out ends up owned by HOST_UID:HOST_GID, the user who runs
# make.
set -euo pipefail

echo "--> Installing build dependencies..."
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential \
  pkgconf \
  dpkg-dev \
  python3 \
  libwayland-dev \
  wayland-protocols \
  libxkbcommon-dev \
  libcairo2-dev \
  librsvg2-dev

# A copy of the sources, so that the build does not write into /src.
echo "--> Compiling hd-menu..."
mkdir -p /usr/src/hd-menu
cp -r /src/Makefile /src/src /src/protocols /usr/src/hd-menu/
cd /usr/src/hd-menu
make all \
  CPPFLAGS="$(dpkg-buildflags --get CPPFLAGS)" \
  CXXFLAGS="$(dpkg-buildflags --get CXXFLAGS)" \
  CFLAGS="$(dpkg-buildflags --get CFLAGS)" \
  LDFLAGS="$(dpkg-buildflags --get LDFLAGS)"

# dpkg-shlibdeps finds the packages of the libraries the binary links to, and wants a
# debian/control to run in.
echo "--> Finding the dependencies of the binary..."
mkdir -p /tmp/shlibdeps/debian
printf 'Source: hd-menu\n\nPackage: hd-menu\nArchitecture: any\n' > /tmp/shlibdeps/debian/control
(cd /tmp/shlibdeps && dpkg-shlibdeps -O /usr/src/hd-menu/build/hd-menu) | \
  sed -n 's/^shlibs:Depends=//p' > /tmp/depends

echo "--> Copying the results to /out..."
install -m 0755 build/hd-menu /out/
install -m 0644 /tmp/depends /out/
chown -R "${HOST_UID}:${HOST_GID}" /out
