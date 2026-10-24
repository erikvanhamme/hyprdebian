#!/usr/bin/env bash
# Builds audacious and audacious-plugins with the terminal interface in a Debian container; the
# Makefile runs it with docker. It installs the build tools and the libraries of the plugins,
# compiles the commits AUDACIOUS_COMMIT and PLUGINS_COMMIT of the HyprDebian forks, or the local
# checkouts mounted on /src/audacious and /src/audacious-plugins, with only the terminal interface
# and the plugins for FLAC, MP3 and WAV over PipeWire, and copies what the package needs to /out:
#
#   /out/root                 the installed files, from / of the package on: the libraries,
#                             the plugins and the binaries in /usr/lib/hd-audacious, and the
#                             data, man pages, desktop entry and icons in /usr/share
#   /out/depends              the Depends of the package, from dpkg-shlibdeps
#   /out/COPYING-audacious, /out/COPYING-audacious-plugins
#                             the licenses of both
#
# The libraries are private: they go in /usr/lib/hd-audacious, which the binaries and plugins
# have as RUNPATH, so they never clash with the libaudcore of Debian. Everything in /out ends up
# owned by HOST_UID:HOST_GID, the user who runs make.
set -euo pipefail

AUDACIOUS_URL="https://github.com/erikvanhamme/audacious.git"
PLUGINS_URL="https://github.com/erikvanhamme/audacious-plugins.git"
PREFIX=/usr
PRIVATE=lib/hd-audacious
STAGE=/stage

# The plugins of the package, by the meson option that builds each and its path in the plugin
# directory: the terminal interface, MPRIS for the media keys, the inputs for FLAC, MP3 and WAV
# (sndfile), the output to PipeWire, and the playlist files m3u and pls. Every other boolean
# option of audacious-plugins is turned off, GTK and Qt among them, so no other plugin and none of
# its libraries end up in the package, also not those of plugins added in a later release.
# audpl, the playlist format of audacious itself, has no option and is always built.
declare -A PLUGINS=(
  [tui]=General/tui.so
  [mpris2]=General/mpris2.so
  [flac]=Input/flacng.so
  [mpg123]=Input/madplug.so
  [sndfile]=Input/sndfile.so
  [pipewire]=Output/pipewire.so
  [m3u]=Container/m3u.so
  [pls]=Container/pls.so
)

echo "--> Installing build dependencies..."
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  git \
  ca-certificates \
  build-essential \
  meson \
  ninja-build \
  pkgconf \
  gettext \
  dpkg-dev \
  libglib2.0-dev \
  zlib1g-dev \
  libnotcurses-core-dev \
  libflac-dev \
  libmpg123-dev \
  libsndfile1-dev \
  libpipewire-0.3-dev

# clone <url> <commit> <dir>: fetches only the commit, which needs no branch or tag.
clone() {
  git init -q "$3"
  git -C "$3" fetch -q --depth 1 "$1" "$2"
  git -C "$3" checkout -q FETCH_HEAD
}

# The Makefile mounts local checkouts of the forks on /src for a build of changes that are not
# pushed yet; they are copied, as the build must not write to them.
if [ -d /src/audacious ] && [ -d /src/audacious-plugins ]; then
  echo "--> Copying the local checkouts of audacious and audacious-plugins..."
  cp -a /src/audacious /usr/src/audacious
  cp -a /src/audacious-plugins /usr/src/audacious-plugins
else
  echo "--> Cloning audacious ${AUDACIOUS_COMMIT} and audacious-plugins ${PLUGINS_COMMIT}..."
  clone "${AUDACIOUS_URL}" "${AUDACIOUS_COMMIT}" /usr/src/audacious
  clone "${PLUGINS_URL}" "${PLUGINS_COMMIT}" /usr/src/audacious-plugins
fi

# Meson keeps the RUNPATH of LDFLAGS at the installation, so every binary and plugin finds the
# private libraries.
export LDFLAGS="-Wl,-rpath,${PREFIX}/${PRIVATE}"

# The core goes to / of the container too, so the plugins are built against it, and to the stage
# for the package. D-Bus stays on, for audtool, --enqueue and the media keys.
echo "--> Compiling audacious..."
meson setup /tmp/build-audacious /usr/src/audacious \
  --buildtype=release \
  --prefix="${PREFIX}" \
  --libdir="${PRIVATE}" \
  --bindir="${PRIVATE}/bin" \
  -Dbuildstamp="HyprDebian" \
  -Ddbus=true \
  -Dgtk=false \
  -Dqt=false \
  -Dlibarchive=false \
  -Dvalgrind=false
meson compile -C /tmp/build-audacious
meson install -C /tmp/build-audacious
meson install -C /tmp/build-audacious --destdir "${STAGE}"

export PKG_CONFIG_PATH="${PREFIX}/${PRIVATE}/pkgconfig"

# -D<option>=true for the options in PLUGINS, -D<option>=false for every other boolean option.
options=()
while read -r option; do
  if [ -n "${PLUGINS[${option}]+set}" ]; then
    options+=("-D${option}=true")
  else
    options+=("-D${option}=false")
  fi
done < <(sed -n "s/^option('\([^']*\)', *type: *'boolean'.*/\1/p" /usr/src/audacious-plugins/meson_options.txt)

echo "--> Compiling audacious-plugins with ${options[*]}..."
meson setup /tmp/build-plugins /usr/src/audacious-plugins \
  --buildtype=release \
  --prefix="${PREFIX}" \
  --libdir="${PRIVATE}" \
  "${options[@]}"
meson compile -C /tmp/build-plugins
meson install -C /tmp/build-plugins --destdir "${STAGE}"

# A plugin whose library meson does not find is left out without an error, so check that every
# plugin is there, and that there are no others.
echo "--> Checking the plugins..."
expected=$( (printf '%s\n' "${PLUGINS[@]}"; echo Container/audpl.so) | sort)
built=$(cd "${STAGE}${PREFIX}/${PRIVATE}/audacious" && find . -name '*.so' | sed 's|^\./||' | sort)
if [ "${built}" != "${expected}" ]; then
  echo "The plugins differ from those expected:" >&2
  diff <(echo "${expected}") <(echo "${built}") >&2 || true
  exit 1
fi

# The headers, pkg-config files and unversioned links are for building against audacious, which
# the package is not for.
echo "--> Removing the development files..."
rm -rf "${STAGE}${PREFIX}/include" "${STAGE}${PREFIX}/${PRIVATE}/pkgconfig"
rm -f "${STAGE}${PREFIX}/${PRIVATE}/libaudcore.so" "${STAGE}${PREFIX}/${PRIVATE}/libaudtag.so"

echo "--> Stripping the binaries and libraries..."
elfs=()
while IFS= read -r -d '' file; do
  if [ "$(head -c 4 "${file}")" = $'\x7fELF' ]; then
    elfs+=("${file}")
  fi
done < <(find "${STAGE}" -type f -print0)
strip --strip-unneeded "${elfs[@]}"

if ! readelf -d "${STAGE}${PREFIX}/${PRIVATE}/bin/audacious" | grep -q "RUNPATH.*${PREFIX}/${PRIVATE}"; then
  echo "The binary audacious has no RUNPATH ${PREFIX}/${PRIVATE}" >&2
  exit 1
fi

# dpkg-shlibdeps finds the packages of the libraries the binaries and plugins link to, and wants a
# debian/control to run in. The private libraries are in the package itself: shlibs.local names
# it for them, and -x leaves it out of the Depends again.
echo "--> Finding the dependencies of the binaries and plugins..."
mkdir -p /tmp/shlibdeps/debian
printf 'Source: hd-audacious\n\nPackage: hd-audacious\nArchitecture: any\n' > /tmp/shlibdeps/debian/control
printf 'libaudcore 6 hd-audacious\nlibaudtag 4 hd-audacious\n' > /tmp/shlibdeps/debian/shlibs.local
(cd /tmp/shlibdeps && dpkg-shlibdeps -O -xhd-audacious -l"${STAGE}${PREFIX}/${PRIVATE}" "${elfs[@]}") | \
  sed -n 's/^shlibs:Depends=//p' > /tmp/depends

echo "--> Copying the results to /out..."
cp -a "${STAGE}" /out/root
install -m 0644 /tmp/depends /out/
install -m 0644 /usr/src/audacious/COPYING /out/COPYING-audacious
install -m 0644 /usr/src/audacious-plugins/COPYING /out/COPYING-audacious-plugins
chown -R "${HOST_UID}:${HOST_GID}" /out
