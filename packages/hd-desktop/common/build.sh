#!/usr/bin/env bash
# Builds hd-menu and hd-message in a container of hd-build:unstable, which
# has the build tools and their -dev packages; ../Makefile runs it with
# docker, with the directories common/, menu/ and message/ in /src, read
# only. It compiles both on every core, with the hardening flags of Debian,
# and copies what the package needs to /out:
#
#   /out/hd-menu             the binary of the menu
#   /out/hd-message          the binary of the message box
#   /out/depends             the Depends of the package for both, from dpkg-shlibdeps
#
# Everything in /out ends up owned by HOST_UID:HOST_GID, the user who runs
# make.
set -euo pipefail

PROGRAMS=(menu:hd-menu message:hd-message)

# A copy of the sources, so that the build does not write into /src. The
# programs find the shared code in ../common, as in the repository.
# IN_BUILD=1 has program.mk compile in this container; without it, make all
# starts a container of hd-build:unstable to compile in.
mkdir -p /usr/src/hd-desktop
cp -r /src/common /usr/src/hd-desktop/
binaries=()
for program in "${PROGRAMS[@]}"; do
  dir=${program%%:*}
  name=${program#*:}
  echo "--> Compiling ${name}..."
  mkdir -p "/usr/src/hd-desktop/${dir}"
  cp -r "/src/${dir}/Makefile" "/src/${dir}/src" "/usr/src/hd-desktop/${dir}/"
  make -C "/usr/src/hd-desktop/${dir}" -j"$(nproc)" all IN_BUILD=1 \
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
