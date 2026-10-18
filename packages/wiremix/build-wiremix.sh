#!/usr/bin/env bash
# Builds wiremix in a Debian container; the Makefile runs it with docker. It
# installs the build tools and the latest stable Rust toolchain, compiles the
# release WIREMIX_VERSION of wiremix and copies what the package needs to
# /out:
#
#   /out/wiremix             the binary
#   /out/depends             the Depends of the package, from dpkg-shlibdeps
#   /out/wiremix.desktop     the desktop entry
#   /out/wiremix.toml        the reference of the configuration file
#   /out/README.md           the readme of the release
#   /out/LICENSE-MIT, /out/LICENSE-APACHE
#                            the licenses of wiremix
#
# Everything in /out ends up owned by HOST_UID:HOST_GID, the user who runs
# make.
set -euo pipefail

REPO_URL="https://github.com/tsowell/wiremix.git"

echo "--> Installing build dependencies..."
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y \
  git \
  build-essential \
  pkg-config \
  curl \
  ca-certificates \
  clang \
  libpipewire-0.3-dev \
  dpkg-dev

# The rustc in Debian can be too old for the crates wiremix uses, so install the latest stable
# toolchain with rustup, as for yazi.
echo "--> Installing the latest stable Rust toolchain with rustup..."
curl --proto =https --tlsv1.2 -sSf https://sh.rustup.rs | \
  sh -s -- -y --profile minimal --default-toolchain stable
. "${HOME}/.cargo/env"
echo "--> Using $(rustc --version)"

# The build puts the git description of the tag in the output of wiremix --version.
echo "--> Cloning wiremix v${WIREMIX_VERSION}..."
git clone --depth 1 --branch "v${WIREMIX_VERSION}" "${REPO_URL}" /usr/src/wiremix
cd /usr/src/wiremix

echo "--> Compiling release binary..."
cargo build --release --locked

# dpkg-shlibdeps finds the packages of the libraries the binary links to, and wants a
# debian/control to run in.
echo "--> Finding the dependencies of the binary..."
mkdir -p /tmp/shlibdeps/debian
printf 'Source: wiremix\n\nPackage: wiremix\nArchitecture: any\n' > /tmp/shlibdeps/debian/control
(cd /tmp/shlibdeps && dpkg-shlibdeps -O /usr/src/wiremix/target/release/wiremix) | \
  sed -n 's/^shlibs:Depends=//p' > /tmp/depends

echo "--> Copying the results to /out..."
install -m 0755 target/release/wiremix /out/
install -m 0644 /tmp/depends wiremix.desktop wiremix.toml README.md LICENSE-MIT LICENSE-APACHE /out/
chown -R "${HOST_UID}:${HOST_GID}" /out
