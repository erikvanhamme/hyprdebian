#!/usr/bin/env bash
# Builds the HyprDebian kernel of one suite in a Debian container of that suite; the Makefile runs
# it with docker. MODE says what it does:
#
#   sources  print the versions of the source packages linux and zfs-linux that a build would use,
#            the most recent ones in the suite, as "<suite> linux <version> zfs-linux <version>"
#   full     build the kernel with the config of Debian, and the ZFS module against it
#   test     the same, quicker: only the modules in /lsmod and no debug info
#
# A build gets the source packages and the config of Debian for amd64, linux-config-<series> of the
# same source, and builds the kernel with the patches of Debian, then the ZFS module against that
# kernel tree as DKMS would, and makes three packages in /out:
#
#   linux-image-<release>_<version>_amd64.deb     the kernel and its modules
#   zfs-modules-<release>_<version>_amd64.deb     zfs.ko and spl.ko, in extra/zfs
#   hd-kernel_<version>_amd64.deb                 depends on both, see deb/hd-kernel/control
#
# The release is <x.y.z>+debNN-hd<id>-amd64: debNN as in the release of the kernel of Debian, and
# <id> six hex digits of the sha256 of the versions of linux, zfs-linux and hd-kernel. Every build
# signs the modules with a key of its own, so a build of other versions also gets another release:
# it installs next to the kernel that runs, whose modules stay signed with the key it trusts.
#
# ZFS is not copied into the kernel tree: with copy-builtin, the configure of ZFS takes it to be
# built into the kernel and skips its checks for the symbols that only GPL modules may use, so a
# zfs.ko built that way does not link. Built apart, configure checks them and zfs.ko uses its own
# code where it may not use the kernel's.
#
# The config is that of Debian, with these changes:
#   - the kernel trusts no keys of Debian, and every module, zfs.ko too, is signed with a key that
#     the build makes in certs/signing_key.pem and removes once zfs.ko is signed
#   - the kernel release ends in the suffix above, set as CONFIG_LOCALVERSION
# and in the mode test also only the modules in /lsmod (make localmodconfig) and no debug info, so
# no BTF. The mode full checks that every option Debian has on is on in the config, as olddefconfig
# silently turns off what the tools in the container cannot build, such as Rust.
#
# The container gets:
#   /src    the package directory, read-only: the control files and maintainer scripts in deb/
#   /work   the source packages and the trees, kept for the next build of the same versions, and
#           the ccache of the compiler
#   /out    the packages
#   /cache  only with HD_KERNEL_CACHE in the Makefile: the packages of earlier builds, one
#           directory per release; a build of a release that is there copies its packages instead
#   /lsmod  only in the mode test: the modules, as lsmod prints them
#
# HD_VERSION is the version of hd-kernel, SUITE the suite, JOBS the number of jobs. make bindeb-pkg
# writes its own debian/ into the kernel tree, over the one of the source package; the patches of
# Debian are applied by then. Everything in /work, /out and /cache ends up owned by
# HOST_UID:HOST_GID, the user who runs make.
set -euo pipefail

MAINTAINER_NAME="Erik Van Hamme"
MAINTAINER_EMAIL="erik.vanhamme@gmail.com"

if [[ -d /cache ]]; then
  trap 'chown -R "${HOST_UID}:${HOST_GID}" /work /out /cache' EXIT
else
  trap 'chown -R "${HOST_UID}:${HOST_GID}" /work /out' EXIT
fi

# Prints the most recent version of the source package $1 in the suite.
latest_source() {
  local latest="" version
  for version in $(apt-cache showsrc --only-source "$1" | sed -n 's/^Version: //p'); do
    if [[ -z "${latest}" ]] || dpkg --compare-versions "${version}" gt "${latest}"; then
      latest="${version}"
    fi
  done
  if [[ -z "${latest}" ]]; then
    echo "No source package $1 in ${SUITE}" >&2
    return 1
  fi
  echo "${latest}"
}

# zfs-linux is in contrib, and apt source needs the deb-src entries. The stanzas of the security
# updates get them too.
sed -i -e 's/^Types: deb$/Types: deb deb-src/' -e 's/^Components: main$/Components: main contrib/' \
  /etc/apt/sources.list.d/debian.sources
apt-get update -qq

KERNEL_VERSION="$(latest_source linux)"
ZFS_VERSION="$(latest_source zfs-linux)"

if [[ "${MODE}" == "sources" ]]; then
  echo "${SUITE} linux ${KERNEL_VERSION} zfs-linux ${ZFS_VERSION}"
  exit 0
fi

UPSTREAM="${KERNEL_VERSION%-*}"
SERIES="$(cut -d. -f1-2 <<< "${UPSTREAM}")"
ZFS_UPSTREAM="${ZFS_VERSION%-*}"
# The debNN of the release of the kernel of Debian, from the name of its package in the record of
# that version.
DEBIAN_RELEASE="$(apt-cache showsrc --only-source linux \
  | awk -v version="${KERNEL_VERSION}" 'BEGIN { RS = "" } index($0, "\nVersion: " version "\n")' \
  | grep -oE "linux-image-${UPSTREAM//./\\.}\+deb[0-9]+-amd64" | sed -n 1p)"
if [[ -z "${DEBIAN_RELEASE}" ]]; then
  echo "No linux-image-${UPSTREAM}+debNN-amd64 in linux ${KERNEL_VERSION}" >&2
  exit 1
fi
DEBNN="$(grep -oE '\+deb[0-9]+' <<< "${DEBIAN_RELEASE}")"
ID="$(printf '%s %s %s' "${KERNEL_VERSION}" "${ZFS_VERSION}" "${HD_VERSION}" | sha256sum | cut -c1-6)"
if [[ "${MODE}" == "test" ]]; then
  ID="test${ID}"
fi
LOCALVERSION_SUFFIX="${DEBNN}-hd${ID}-amd64"
RELEASE="${UPSTREAM}${LOCALVERSION_SUFFIX}"
KERNEL_DEB_VERSION="${KERNEL_VERSION}+hd${HD_VERSION}"
ZFS_DEB_VERSION="${ZFS_VERSION}+hd${HD_VERSION}"
# hd-kernel goes up with each of the three versions, so it pulls in every new build.
META_DEB_VERSION="${KERNEL_VERSION}+hd${HD_VERSION}+zfs${ZFS_VERSION//-/.}"
KERNEL_DIR="/work/src/linux-${UPSTREAM}"
ZFS_DIR="/work/src/zfs-linux-${ZFS_UPSTREAM}"

echo "--> ${SUITE}: linux ${KERNEL_VERSION}, zfs-linux ${ZFS_VERSION}, release ${RELEASE}"

if [[ -d /cache && -f "/cache/${SUITE}/${RELEASE}/complete" ]]; then
  echo "--> Copying the packages of ${RELEASE} from the cache..."
  install -m 0644 "/cache/${SUITE}/${RELEASE}"/*.deb /out/
  exit 0
fi

echo "--> Installing build dependencies..."
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  build-essential bc bison flex libssl-dev libelf-dev libdw-dev kmod cpio rsync python3 perl \
  dpkg-dev debhelper fakeroot xz-utils zstd ccache autoconf automake libtool gawk \
  rustc rust-src bindgen pahole ca-certificates

mkdir -p /work/src /work/ccache
cd /work/src

# The trees and source packages of other versions are of no use any more.
for old in linux-* linux_* zfs-linux-* zfs-linux_*; do
  case "${old}" in
    "linux-${UPSTREAM}" | "linux_${UPSTREAM}"* | "zfs-linux-${ZFS_UPSTREAM}" | "zfs-linux_${ZFS_UPSTREAM}"*) ;;
    *)
      if [[ -e "${old}" ]]; then
        rm -rf "${old}"
      fi
      ;;
  esac
done

if [[ ! -d "${KERNEL_DIR}" ]]; then
  echo "--> Getting linux ${KERNEL_VERSION}..."
  apt-get source --only-source "linux=${KERNEL_VERSION}"
fi

if [[ ! -d "${ZFS_DIR}" ]]; then
  echo "--> Getting zfs-linux ${ZFS_VERSION}..."
  apt-get source --only-source "zfs-linux=${ZFS_VERSION}"
fi

# The config of Debian for amd64 is in linux-config-<series>, built from the same source.
echo "--> Getting the config of Debian..."
rm -rf /work/config
mkdir -p /work/config
cd /work/config
apt-get download "linux-config-${SERIES}=${KERNEL_VERSION}"
dpkg-deb -x linux-config-*.deb root
xz -dc "root/usr/src/linux-config-${SERIES}/config.amd64_none_amd64.xz" > debian.config

echo "--> Setting up the config..."
cd "${KERNEL_DIR}"
cp /work/config/debian.config .config
make olddefconfig
if [[ "${MODE}" == "test" ]]; then
  # localmodconfig asks for the options that are new to the config; with no input it takes the
  # defaults, and says "Error in reading or end of file" for each.
  make LSMOD=/lsmod localmodconfig < /dev/null
  scripts/config \
    -d DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT -d DEBUG_INFO_DWARF4 -d DEBUG_INFO_DWARF5 \
    -e DEBUG_INFO_NONE -d DEBUG_INFO_BTF
fi
scripts/config \
  --set-str SYSTEM_TRUSTED_KEYS "" \
  --set-str SYSTEM_REVOCATION_KEYS "" \
  -e MODULE_SIG -e MODULE_SIG_ALL \
  --set-str MODULE_SIG_KEY "certs/signing_key.pem" \
  --set-str LOCALVERSION "${LOCALVERSION_SUFFIX}" \
  -d LOCALVERSION_AUTO
make olddefconfig

if [[ "${MODE}" == "full" ]]; then
  missing="$(grep -E '^CONFIG_[A-Z0-9_]+=[ym]$' /work/config/debian.config | cut -d= -f1 \
    | while read -r option; do
        if ! grep -qE "^${option}=[ym]$" .config; then
          echo "${option}"
        fi
      done)"
  if [[ -n "${missing}" ]]; then
    echo "These options of the config of Debian are off in the config of the build:" >&2
    echo "${missing}" >&2
    exit 1
  fi
fi

# A new key for every build: the key of a build that failed before it was removed is removed now,
# so the build makes another.
rm -f certs/signing_key.pem certs/signing_key.x509

echo "--> Building the kernel..."
rm -f /work/src/*.deb /work/src/*.buildinfo /work/src/*.changes
export CCACHE_DIR=/work/ccache
export DEBFULLNAME="${MAINTAINER_NAME}" DEBEMAIL="${MAINTAINER_EMAIL}"
export KBUILD_BUILD_USER=hyprdebian KBUILD_BUILD_HOST=hd-kernel
time make -j"${JOBS}" CC="ccache gcc" KDEB_PKGVERSION="${KERNEL_DEB_VERSION}" \
  KDEB_CHANGELOG_DIST="${SUITE}" bindeb-pkg
if [[ "$(make -s kernelrelease)" != "${RELEASE}" ]]; then
  echo "The kernel release is $(make -s kernelrelease), not ${RELEASE}" >&2
  exit 1
fi
image_deb="/work/src/linux-image-${RELEASE}_${KERNEL_DEB_VERSION}_amd64.deb"

# configure runs every build, as the kernel tree may have changed since the last one.
echo "--> Building the ZFS module..."
cd "${ZFS_DIR}"
if [[ ! -x configure ]]; then
  ./autogen.sh
fi
./configure --with-config=kernel --with-linux="${KERNEL_DIR}" --with-linux-obj="${KERNEL_DIR}"
make -C module clean
time make -j"${JOBS}" -C module modules

# The modules go where those of the kernel package are, under extra/zfs as in zfs-modules of Debian.
# sed reads the whole list, so dpkg-deb does not end on a closed pipe, as it would with head.
moddir="$(dpkg-deb -c "${image_deb}" | grep -o "[a-z/.]*/modules/${RELEASE}/kernel/" | sed -n 1p)"
moddir="${moddir#.}"
moddir="${moddir%kernel/}"

# Fills in the @...@ of a template of deb/ into $2.
fill() {
  sed -e "s|@RELEASE@|${RELEASE}|g" \
    -e "s|@KERNEL_DEB_VERSION@|${KERNEL_DEB_VERSION}|g" \
    -e "s|@ZFS_DEB_VERSION@|${ZFS_DEB_VERSION}|g" \
    -e "s|@META_DEB_VERSION@|${META_DEB_VERSION}|g" \
    -e "s|@KERNEL_VERSION@|${KERNEL_VERSION}|g" \
    -e "s|@ZFS_VERSION@|${ZFS_VERSION}|g" \
    -e "s|@ZFS_UPSTREAM@|${ZFS_UPSTREAM}|g" \
    -e "s|@SUITE@|${SUITE}|g" \
    -e "s|@MODDIR@|${moddir}|g" \
    -e "s|@INSTALLED_SIZE@|$(du -sk --apparent-size --exclude=DEBIAN "$3" | cut -f1)|g" \
    "$1" > "$2"
}

echo "--> Signing the ZFS module..."
stage=/work/zfs-deb
rm -rf "${stage}"
install -d "${stage}/DEBIAN" "${stage}${moddir}extra/zfs"
find module -name '*.ko' -exec install -m 0644 {} "${stage}${moddir}extra/zfs/" \;
for module in "${stage}${moddir}"extra/zfs/*.ko; do
  strip --strip-debug "${module}"
  "${KERNEL_DIR}/scripts/sign-file" sha256 "${KERNEL_DIR}/certs/signing_key.pem" \
    "${KERNEL_DIR}/certs/signing_key.x509" "${module}"
done
rm -f "${KERNEL_DIR}/certs/signing_key.pem"

echo "--> Packaging the ZFS module..."
install -D -m 0644 debian/copyright "${stage}/usr/share/doc/zfs-modules-${RELEASE}/copyright"
fill /src/deb/zfs-modules/control "${stage}/DEBIAN/control" "${stage}"
fill /src/deb/zfs-modules/postinst "${stage}/DEBIAN/postinst" "${stage}"
fill /src/deb/zfs-modules/postrm "${stage}/DEBIAN/postrm" "${stage}"
chmod 0755 "${stage}/DEBIAN/postinst" "${stage}/DEBIAN/postrm"
zfs_deb="/work/src/zfs-modules-${RELEASE}_${ZFS_DEB_VERSION}_amd64.deb"
dpkg-deb --root-owner-group --build "${stage}" "${zfs_deb}"

echo "--> Packaging hd-kernel..."
stage=/work/meta-deb
rm -rf "${stage}"
install -d "${stage}/DEBIAN" "${stage}/usr/share/man/man7"
gzip -9n < /src/man/hd-kernel.7 > "${stage}/usr/share/man/man7/hd-kernel.7.gz"
chmod 0644 "${stage}/usr/share/man/man7/hd-kernel.7.gz"
fill /src/deb/hd-kernel/control "${stage}/DEBIAN/control" "${stage}"
meta_deb="/work/src/hd-kernel_${META_DEB_VERSION}_amd64.deb"
dpkg-deb --root-owner-group --build "${stage}" "${meta_deb}"

echo "--> Copying the packages to /out..."
install -m 0644 "${image_deb}" "${zfs_deb}" "${meta_deb}" /out/

if [[ -d /cache ]]; then
  echo "--> Storing the packages of ${RELEASE} in the cache..."
  rm -rf "/cache/${SUITE}"
  install -d "/cache/${SUITE}/${RELEASE}"
  install -m 0644 "${image_deb}" "${zfs_deb}" "${meta_deb}" "/cache/${SUITE}/${RELEASE}/"
  touch "/cache/${SUITE}/${RELEASE}/complete"
fi
ccache -s
