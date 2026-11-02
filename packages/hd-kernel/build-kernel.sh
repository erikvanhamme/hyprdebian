#!/usr/bin/env bash
# Builds a HyprDebian kernel of one suite in a container of the build environment of that suite,
# hd-kernel-buildenv:<suite> of buildenv/Dockerfile, with its build dependencies; kernel.mk runs it
# with docker, for hd-kernel, hd-kernel-vm, hd-kernel-server and hd-kernel-desktop. MODE says what
# it does:
#
#   sources  print the versions of the source packages linux and zfs-linux that a build would use,
#            the most recent ones in the suite, as "<suite> linux <version> zfs-linux <version>"
#   full     build the kernel with the config of Debian, and the ZFS module against it
#   test     the same, quicker: only the modules in /lsmod and no debug info
#
# A build gets the source packages and the config of Debian for amd64, linux-config-<series> of the
# same source, and builds the kernel with the patches of Debian, then the ZFS module against that
# kernel tree as DKMS would, and makes four packages in /out:
#
#   linux-image-<release>_<version>_amd64.deb     the kernel and its modules
#   zfs-modules-<release>_<version>_amd64.deb     zfs.ko and spl.ko, in extra/zfs
#   <PACKAGE>_<version>_amd64.deb                 depends on both, see deb/<PACKAGE>/control
#   linux-headers-<release>_<version>_amd64.deb   the headers of the kernel, for DKMS, which
#                                                 nothing depends on
#
# PACKAGE is one of those four. The release is <x.y.z>+debNN-<TAG><id>-amd64: debNN as in the
# release of the kernel of Debian, TAG hd for hd-kernel, hdvm for hd-kernel-vm, hdserver for
# hd-kernel-server and hddesktop for hd-kernel-desktop, and <id> six hex digits of a sha256 of
# everything the packages are made of: the versions of linux, zfs-linux and PACKAGE, this script,
# the Dockerfile of the build environment, the templates of zfs-modules, the control file, the man
# page and the keep list of PACKAGE, and in the mode test the modules in /lsmod. Every build signs
# the modules with a key of its own, so a build of anything else also gets another release: it
# installs next to the kernel that runs, whose modules stay signed with the key it trusts.
#
# A build only builds when it has to. When /out already has the four packages of the release, it
# stops there: they only get there at the end of a build that went through. A make clean removes
# them, and with them the reason to skip. Otherwise, when /cache has them, it copies them from
# there. Only then does it build, and first removes the packages of other releases from /out.
#
# ZFS is not copied into the kernel tree: with copy-builtin, the configure of ZFS takes it to be
# built into the kernel and skips its checks for the symbols that only GPL modules may use, so a
# zfs.ko built that way does not link. Built apart, configure checks them and zfs.ko uses its own
# code where it may not use the kernel's.
#
# The config is that of Debian, with these changes:
#   - with KEEP, the keep list of PACKAGE, only the modules it keeps (make localmodconfig)
#   - the kernel trusts no keys of Debian, and every module, zfs.ko too, is signed with a key that
#     the build makes in certs/signing_key.pem and removes once zfs.ko is signed
#   - the kernel release ends in the suffix above, set as CONFIG_LOCALVERSION
# and in the mode test also only the modules in /lsmod (make localmodconfig) and no debug info, so
# no BTF. The mode full checks that every option Debian has on is on in the config before those
# changes, as olddefconfig silently turns off what the tools in the container cannot build, such
# as Rust, and with KEEP that the kernel has every module that the keep list names.
#
# The keep list KEEP, a file of /package, has a line per directory of the kernel tree, which ends
# in /, whose modules all stay, as do those of the directories below it; a ! in front of such a
# line leaves out that directory. Any other line names a module, as lsmod prints it, that stays,
# or names separated by |, the names of one module in different kernels, such as
# cirrus_qemu|cirrus: the kernel has to have one of them.
# Comments start with #. The modules of a directory are those its Makefiles build.
#
# The container gets:
#   /src    the directory of this script, read-only: the templates of zfs-modules in
#           deb/zfs-modules
#   /package
#           the package directory of PACKAGE, read-only: its control file in deb/<PACKAGE>,
#           its man page in man/<PACKAGE>.7 and its keep list; that of hd-kernel is /src
#   /work   the work of the suite, shared by the kernels of every package, kept for the next build:
#             src/linux, src/zfs-linux
#                     the sources of linux and zfs-linux, as apt source unpacks them, with the
#                     patches of Debian; src/<name>.version holds their version. The kernels
#                     build out of tree from src/linux, which stays as apt source left it, and
#                     ZFS builds in src/zfs-linux for one kernel after another
#             TREE/linux
#                     the kernel tree of this build, of make O=, with the version of linux in
#                     TREE/linux.version; it starts anew with another version
#             TREE    also the config of Debian, the packages of make bindeb-pkg and the stages
#                     of the other packages
#             lock    keeps a second build of the suite from starting before the first is done
#   /ccache the ccache of the compiler, of every package and suite, of at most CCACHE_SIZE. It
#           hashes neither the directory of the build nor the paths below /work, so a new
#           version of linux, whose sources and trees have the same paths, compiles from it what
#           it did not change
#   /out    the packages, kept for the next build, which skips the build when they are those of
#           its release
#   /cache  only with HD_KERNEL_CACHE in the Makefile: the packages of earlier builds, in
#           <PACKAGE>/<suite>/<release>; a build of a release that is there copies its packages
#           instead
#   /lsmod  only in the mode test: the modules, as lsmod prints them
#
# HD_VERSION is the version of PACKAGE, SUITE the suite, JOBS the number of jobs, TREE the
# directory of this build in /work: PACKAGE, or PACKAGE-test in the mode test. make bindeb-pkg
# writes its own debian/ into the kernel tree of the build, not into src/linux. What the build
# writes in /work, /out and /cache ends up owned by HOST_UID:HOST_GID, the user who runs make.
set -euo pipefail

MAINTAINER_NAME="Erik Van Hamme"
MAINTAINER_EMAIL="erik.vanhamme@gmail.com"

# Hands what the build writes to the user who runs make; not all of /work, which also has the
# trees of the other packages, and only the files that are not that user's yet, as the ccache
# has many.
chown_back() {
  local path
  for path in /work/src "/work/${TREE}" /work/lock /ccache /out /cache; do
    if [[ -e "${path}" ]]; then
      find "${path}" \( ! -user "${HOST_UID}" -o ! -group "${HOST_GID}" \) \
        -exec chown -h "${HOST_UID}:${HOST_GID}" {} +
    fi
  done
}
trap chown_back EXIT

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

# Prints the lines of the keep list, without comments and blanks.
keep_lines() {
  sed -e 's/#.*//' -e 's/[[:space:]]//g' -e '/^$/d' "/package/${KEEP}"
}

# Prints the lines of the keep list that name modules, one per line: a module, or the names of
# one module separated by |.
keep_names() {
  keep_lines | sed -e '/\/$/d'
}

# Prints the modules that the keep list keeps, one per line, for make localmodconfig: those it
# names, and those that the Makefiles in its directories build with obj-$(CONFIG_...), with _ for
# -, as lsmod prints them. Run it in the kernel tree.
keep_modules() {
  local line dirs=() prune=()
  while read -r line; do
    case "${line}" in
      "!"*/) prune+=(-path "${line:1:-1}" -prune -o) ;;
      */) dirs+=("${line%/}") ;;
    esac
  done < <(keep_lines)
  keep_names | tr '|' '\n'
  if (( ${#dirs[@]} > 0 )); then
    find "${dirs[@]}" "${prune[@]}" \( -name Makefile -o -name Kbuild \) -print \
      | xargs -r awk '
          /\\$/ { sub(/\\$/, ""); text = text $0 " "; next }
          {
            text = text $0
            if (text ~ /^[[:space:]]*obj-\$\(CONFIG_[A-Za-z0-9_]+\)[[:space:]]*[+:]?=/) {
              sub(/^[^=]*=/, "", text)
              n = split(text, words)
              for (i = 1; i <= n; i++) {
                if (words[i] ~ /^[A-Za-z0-9_-]+\.o$/) {
                  sub(/\.o$/, "", words[i])
                  gsub(/-/, "_", words[i])
                  print words[i]
                }
              }
            }
            text = ""
          }'
  fi
}

# The build environment has the deb-src entries that apt source needs, but not the newest lists.
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
# The sums of the files the packages are made of, by their path in /src and /package.
inputs="$(cd /src && find build-kernel.sh buildenv deb/zfs-modules -type f | LC_ALL=C sort | xargs sha256sum)"
package_files=("deb/${PACKAGE}" "man/${PACKAGE}.7")
if [[ -n "${KEEP}" ]]; then
  package_files+=("${KEEP}")
fi
inputs="${inputs}"$'\n'"$(cd /package && find "${package_files[@]}" -type f | LC_ALL=C sort | xargs sha256sum)"
if [[ "${MODE}" == "test" ]]; then
  inputs="${inputs}"$'\n'"$(sha256sum /lsmod)"
fi
ID="$(printf '%s %s %s\n%s\n' "${KERNEL_VERSION}" "${ZFS_VERSION}" "${HD_VERSION}" "${inputs}" \
  | sha256sum | cut -c1-6)"
if [[ "${MODE}" == "test" ]]; then
  ID="test${ID}"
fi
LOCALVERSION_SUFFIX="${DEBNN}-${TAG}${ID}-amd64"
RELEASE="${UPSTREAM}${LOCALVERSION_SUFFIX}"
KERNEL_DEB_VERSION="${KERNEL_VERSION}+hd${HD_VERSION}"
ZFS_DEB_VERSION="${ZFS_VERSION}+hd${HD_VERSION}"
# PACKAGE goes up with each of the three versions, so it pulls in every new build.
META_DEB_VERSION="${KERNEL_VERSION}+hd${HD_VERSION}+zfs${ZFS_VERSION//-/.}"
# The sources, and the directory of this build with its kernel tree. Their paths do not change
# with the versions.
KERNEL_DIR=/work/src/linux
ZFS_DIR=/work/src/zfs-linux
BUILD_DIR="/work/${TREE}"
OBJ_DIR="${BUILD_DIR}/linux"
PACKAGES=(
  "linux-image-${RELEASE}_${KERNEL_DEB_VERSION}_amd64.deb"
  "linux-headers-${RELEASE}_${KERNEL_DEB_VERSION}_amd64.deb"
  "zfs-modules-${RELEASE}_${ZFS_DEB_VERSION}_amd64.deb"
  "${PACKAGE}_${META_DEB_VERSION}_amd64.deb"
)
CACHE="/cache/${PACKAGE}/${SUITE}"

echo "--> ${PACKAGE} of ${SUITE}: linux ${KERNEL_VERSION}, zfs-linux ${ZFS_VERSION}, release ${RELEASE}"

# True when the directory $1 has the four packages of the release.
has_packages() {
  local package
  for package in "${PACKAGES[@]}"; do
    if [[ ! -f "$1/${package}" ]]; then
      return 1
    fi
  done
  return 0
}

if has_packages /out; then
  echo "--> The packages of ${RELEASE} are built already."
  exit 0
fi

rm -f /out/*.deb

if [[ -d /cache ]] && has_packages "${CACHE}/${RELEASE}"; then
  echo "--> Copying the packages of ${RELEASE} from the cache..."
  for package in "${PACKAGES[@]}"; do
    install -m 0644 "${CACHE}/${RELEASE}/${package}" /out/
  done
  exit 0
fi

# One build of the suite at a time, as they share the sources: a build of another package waits
# here until this one is done.
exec 9> /work/lock
if ! flock -n 9; then
  echo "--> Waiting for the build of another package of ${SUITE}..."
  flock 9
fi

mkdir -p /work/src "${BUILD_DIR}"

# What the layouts before /ccache and the directories per package left in /work.
for old in ccache config zfs-deb meta-deb keep.lsmod; do
  if [[ -e "/work/${old}" ]]; then
    rm -rf "/work/${old:?}"
  fi
done

cd /work/src

# Everything else in src is of no use any more: the trees of the layout before src/linux, and
# what a get_source that stopped halfway left.
for old in *; do
  case "${old}" in
    linux | linux.version | zfs-linux | zfs-linux.version) ;;
    *)
      if [[ -e "${old}" ]]; then
        rm -rf "${old}"
      fi
      ;;
  esac
done

# Unpacks the source package $1 of the version $2 into /work/src/$1, unless it is there already:
# apt source unpacks it into $1-<upstream version>, with the patches of Debian, and $1.version
# says which version /work/src/$1 has. The source packages go once they are unpacked.
get_source() {
  local name="$1" version="$2"
  if [[ -d "${name}" && -f "${name}.version" && "$(< "${name}.version")" == "${version}" ]]; then
    return 0
  fi
  echo "--> Getting ${name} ${version}..."
  rm -rf "${name}" "${name}.version"
  apt-get source --only-source "${name}=${version}"
  mv "${name}-${version%-*}" "${name}"
  rm -f "${name}_"*
  echo "${version}" > "${name}.version"
}
get_source linux "${KERNEL_VERSION}"
get_source zfs-linux "${ZFS_VERSION}"

# The kernel tree of this build starts anew with another version of linux: the files of a new
# source have the times of the source package, which can be older than the objects of the tree.
if [[ ! -f "${OBJ_DIR}.version" || "$(< "${OBJ_DIR}.version")" != "${KERNEL_VERSION}" ]]; then
  rm -rf "${OBJ_DIR}"
  mkdir -p "${OBJ_DIR}"
  echo "${KERNEL_VERSION}" > "${OBJ_DIR}.version"
fi

# Runs make in the kernel tree of this build, with the source in src/linux.
kmake() {
  make -C "${KERNEL_DIR}" O="${OBJ_DIR}" "$@"
}

# The config of Debian for amd64 is in linux-config-<series>, built from the same source.
echo "--> Getting the config of Debian..."
rm -rf "${BUILD_DIR}/config"
mkdir -p "${BUILD_DIR}/config"
cd "${BUILD_DIR}/config"
apt-get download "linux-config-${SERIES}=${KERNEL_VERSION}"
dpkg-deb -x linux-config-*.deb root
xz -dc "root/usr/src/linux-config-${SERIES}/config.amd64_none_amd64.xz" > debian.config

echo "--> Setting up the config..."
cd "${OBJ_DIR}"
cp "${BUILD_DIR}/config/debian.config" .config
kmake olddefconfig

# Prints the options that the config $1 has on, as y or m, sorted.
options_on() {
  grep -E '^CONFIG_[A-Z0-9_]+=[ym]$' "$1" | cut -d= -f1 | LC_ALL=C sort -u
}

if [[ "${MODE}" == "full" ]]; then
  missing="$(LC_ALL=C comm -23 <(options_on "${BUILD_DIR}/config/debian.config") <(options_on .config))"
  if [[ -n "${missing}" ]]; then
    echo "These options of the config of Debian are off in the config of the build:" >&2
    echo "${missing}" >&2
    exit 1
  fi
fi

if [[ -n "${KEEP}" ]]; then
  (cd "${KERNEL_DIR}" && keep_modules) > "${BUILD_DIR}/keep.lsmod"
  kmake LSMOD="${BUILD_DIR}/keep.lsmod" localmodconfig < /dev/null
fi
if [[ "${MODE}" == "test" ]]; then
  # localmodconfig asks for the options that are new to the config; with no input it takes the
  # defaults, and says "Error in reading or end of file" for each.
  kmake LSMOD=/lsmod localmodconfig < /dev/null
  "${KERNEL_DIR}/scripts/config" --file .config \
    -d DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT -d DEBUG_INFO_DWARF4 -d DEBUG_INFO_DWARF5 \
    -e DEBUG_INFO_NONE -d DEBUG_INFO_BTF
fi
"${KERNEL_DIR}/scripts/config" --file .config \
  --set-str SYSTEM_TRUSTED_KEYS "" \
  --set-str SYSTEM_REVOCATION_KEYS "" \
  -e MODULE_SIG -e MODULE_SIG_ALL \
  --set-str MODULE_SIG_KEY "certs/signing_key.pem" \
  --set-str LOCALVERSION "${LOCALVERSION_SUFFIX}" \
  -d LOCALVERSION_AUTO
kmake olddefconfig

# A new key for every build: the key of a build that failed before it was removed is removed now,
# so the build makes another.
rm -f certs/signing_key.pem certs/signing_key.x509

echo "--> Building the kernel..."
# make bindeb-pkg puts the packages next to the kernel tree, in the directory of the build.
rm -f "${BUILD_DIR}"/*.deb "${BUILD_DIR}"/*.buildinfo "${BUILD_DIR}"/*.changes
export CCACHE_DIR=/ccache CCACHE_MAXSIZE="${CCACHE_SIZE}" CCACHE_NOHASHDIR=true CCACHE_BASEDIR=/work
# The statistics at the end are those of this build.
ccache --zero-stats > /dev/null
export DEBFULLNAME="${MAINTAINER_NAME}" DEBEMAIL="${MAINTAINER_EMAIL}"
export KBUILD_BUILD_USER=hyprdebian KBUILD_BUILD_HOST="${PACKAGE}"
# Not the package of the debug info, linux-image-<release>-dbg, which goes nowhere: it takes an
# objcopy of every module, one after the other, and a compression of gigabytes.
export DEB_BUILD_PROFILES="pkg.linux-upstream.nokerneldbg"
time kmake -j"${JOBS}" CC="ccache gcc" KDEB_PKGVERSION="${KERNEL_DEB_VERSION}" \
  KDEB_CHANGELOG_DIST="${SUITE}" bindeb-pkg
kernelrelease="$(kmake -s --no-print-directory kernelrelease)"
if [[ "${kernelrelease}" != "${RELEASE}" ]]; then
  echo "The kernel release is ${kernelrelease}, not ${RELEASE}" >&2
  exit 1
fi
image_deb="${BUILD_DIR}/linux-image-${RELEASE}_${KERNEL_DEB_VERSION}_amd64.deb"
headers_deb="${BUILD_DIR}/linux-headers-${RELEASE}_${KERNEL_DEB_VERSION}_amd64.deb"

# The modules of the kernel, from modules.order, and those built in, from modules.builtin, with _
# for - as lsmod prints them. In the mode test, localmodconfig turned off what /lsmod has not.
if [[ "${MODE}" == "full" && -n "${KEEP}" ]]; then
  built="$(sed -e 's|.*/||' -e 's/\.k\?o$//' -e 's/-/_/g' modules.order modules.builtin | sort -u)"
  missing="$(keep_names | while read -r names; do
      found=""
      for module in ${names//|/ }; do
        if grep -qxF "${module}" <<< "${built}"; then
          found="${module}"
        fi
      done
      if [[ -z "${found}" ]]; then
        echo "${names}"
      fi
    done)"
  if [[ -n "${missing}" ]]; then
    echo "These modules of the keep list are not in the kernel, as a module or built in:" >&2
    echo "${missing}" >&2
    exit 1
  fi
fi

# configure runs every build, as the kernel tree may have changed since the last one.
echo "--> Building the ZFS module..."
cd "${ZFS_DIR}"
if [[ ! -x configure ]]; then
  ./autogen.sh
fi
./configure --with-config=kernel --with-linux="${KERNEL_DIR}" --with-linux-obj="${OBJ_DIR}"
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
    -e "s|@PACKAGE@|${PACKAGE}|g" \
    -e "s|@MODDIR@|${moddir}|g" \
    -e "s|@INSTALLED_SIZE@|$(du -sk --apparent-size --exclude=DEBIAN "$3" | cut -f1)|g" \
    "$1" > "$2"
}

echo "--> Signing the ZFS module..."
stage="${BUILD_DIR}/zfs-deb"
rm -rf "${stage}"
install -d "${stage}/DEBIAN" "${stage}${moddir}extra/zfs"
find module -name '*.ko' -exec install -m 0644 {} "${stage}${moddir}extra/zfs/" \;
for module in "${stage}${moddir}"extra/zfs/*.ko; do
  strip --strip-debug "${module}"
  "${OBJ_DIR}/scripts/sign-file" sha256 "${OBJ_DIR}/certs/signing_key.pem" \
    "${OBJ_DIR}/certs/signing_key.x509" "${module}"
done
rm -f "${OBJ_DIR}/certs/signing_key.pem"

echo "--> Packaging the ZFS module..."
install -D -m 0644 debian/copyright "${stage}/usr/share/doc/zfs-modules-${RELEASE}/copyright"
fill /src/deb/zfs-modules/control "${stage}/DEBIAN/control" "${stage}"
fill /src/deb/zfs-modules/postinst "${stage}/DEBIAN/postinst" "${stage}"
fill /src/deb/zfs-modules/postrm "${stage}/DEBIAN/postrm" "${stage}"
chmod 0755 "${stage}/DEBIAN/postinst" "${stage}/DEBIAN/postrm"
zfs_deb="${BUILD_DIR}/zfs-modules-${RELEASE}_${ZFS_DEB_VERSION}_amd64.deb"
dpkg-deb --root-owner-group --build "${stage}" "${zfs_deb}"

echo "--> Packaging ${PACKAGE}..."
stage="${BUILD_DIR}/meta-deb"
rm -rf "${stage}"
install -d "${stage}/DEBIAN" "${stage}/usr/share/man/man7"
gzip -9n < "/package/man/${PACKAGE}.7" > "${stage}/usr/share/man/man7/${PACKAGE}.7.gz"
chmod 0644 "${stage}/usr/share/man/man7/${PACKAGE}.7.gz"
fill "/package/deb/${PACKAGE}/control" "${stage}/DEBIAN/control" "${stage}"
meta_deb="${BUILD_DIR}/${PACKAGE}_${META_DEB_VERSION}_amd64.deb"
dpkg-deb --root-owner-group --build "${stage}" "${meta_deb}"

# The packages go to /out last, so a build that stops halfway leaves none there.
echo "--> Copying the packages to /out..."
install -m 0644 "${image_deb}" "${headers_deb}" "${zfs_deb}" "${meta_deb}" /out/

# Into a directory of another name first, so the cache never has a release with only some of its
# packages.
if [[ -d /cache ]]; then
  echo "--> Storing the packages of ${RELEASE} in the cache..."
  rm -rf "${CACHE}.new"
  install -d "${CACHE}.new/${RELEASE}"
  install -m 0644 "${image_deb}" "${headers_deb}" "${zfs_deb}" "${meta_deb}" "${CACHE}.new/${RELEASE}/"
  rm -rf "${CACHE}"
  mv "${CACHE}.new" "${CACHE}"
fi
ccache -s
