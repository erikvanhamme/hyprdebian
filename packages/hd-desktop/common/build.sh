#!/usr/bin/env bash
# Builds hd-menu and hd-message in a Debian container; ../Makefile runs it
# with docker, with the directories common/, menu/ and message/ in /src,
# read only. It installs the build dependencies, compiles both on every core,
# with the hardening flags of Debian, and copies what the package needs to
# /out:
#
#   /out/hd-menu             the binary of the menu
#   /out/hd-message          the binary of the message box
#   /out/depends             the Depends of the package for both, from dpkg-shlibdeps
#
# Everything in /out ends up owned by HOST_UID:HOST_GID, the user who runs
# make.
set -euo pipefail

PROGRAMS=(menu:hd-menu message:hd-message)

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

# A copy of the sources, so that the build does not write into /src. The
# programs find the shared code in ../common, as in the repository.
mkdir -p /usr/src/hd-desktop
cp -r /src/common /usr/src/hd-desktop/
binaries=()
for program in "${PROGRAMS[@]}"; do
  dir=${program%%:*}
  name=${program#*:}
  echo "--> Compiling ${name}..."
  mkdir -p "/usr/src/hd-desktop/${dir}"
  cp -r "/src/${dir}/Makefile" "/src/${dir}/src" "/usr/src/hd-desktop/${dir}/"
  make -C "/usr/src/hd-desktop/${dir}" -j"$(nproc)" all \
    CPPFLAGS="$(dpkg-buildflags --get CPPFLAGS)" \
    CXXFLAGS="$(dpkg-buildflags --get CXXFLAGS)" \
    CFLAGS="$(dpkg-buildflags --get CFLAGS)" \
    LDFLAGS="$(dpkg-buildflags --get LDFLAGS)"
  binaries+=("/usr/src/hd-desktop/${dir}/build/${name}")
done

# dpkg-shlibdeps finds the packages of the libraries the binaries link to, and wants a
# debian/control to run in.
echo "--> Finding the dependencies of the binaries..."
mkdir -p /tmp/shlibdeps/debian
printf 'Source: hd-desktop\n\nPackage: hd-desktop\nArchitecture: any\n' > /tmp/shlibdeps/debian/control
(cd /tmp/shlibdeps && dpkg-shlibdeps -O "${binaries[@]}") | \
  sed -n 's/^shlibs:Depends=//p' > /tmp/depends

echo "--> Copying the results to /out..."
install -m 0755 "${binaries[@]}" /out/
install -m 0644 /tmp/depends /out/
chown -R "${HOST_UID}:${HOST_GID}" /out
