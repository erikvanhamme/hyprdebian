#!/usr/bin/env bash
# Builds yazi in a Debian container; the Makefile runs it with docker. It
# installs the build tools and the latest stable Rust toolchain, compiles the
# release YAZI_VERSION of yazi and copies what the package needs to /out:
#
#   /out/yazi, /out/ya       the binaries
#   /out/completions/        the completions of both for bash, zsh and fish
#   /out/yazi.desktop        the desktop entry
#   /out/logo.png            the icon
#   /out/LICENSE, /out/LICENSE-ICONS
#                            the licenses of yazi and of its icons
#
# Everything in /out ends up owned by HOST_UID:HOST_GID, the user who runs
# make.
set -euo pipefail

REPO_URL="https://github.com/sxyazi/yazi.git"

echo "--> Installing build dependencies..."
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y \
  git \
  build-essential \
  pkg-config \
  curl \
  ca-certificates \
  clang

# The rustc in Debian is too old for yazi, so install the latest stable toolchain with rustup.
# cargo stops when it is older than the rust-version yazi asks for.
echo "--> Installing the latest stable Rust toolchain with rustup..."
curl --proto =https --tlsv1.2 -sSf https://sh.rustup.rs | \
  sh -s -- -y --profile minimal --default-toolchain stable
. "${HOME}/.cargo/env"
echo "--> Using $(rustc --version)"

echo "--> Cloning yazi v${YAZI_VERSION}..."
git clone --depth 1 --branch "v${YAZI_VERSION}" "${REPO_URL}" /usr/src/yazi
cd /usr/src/yazi

# YAZI_GEN_COMPLETIONS makes the build write the completions to yazi-boot/completions and
# yazi-cli/completions.
echo "--> Compiling release binaries (yazi and ya)..."
YAZI_GEN_COMPLETIONS=true cargo build --release --locked

echo "--> Copying the results to /out..."
install -d /out/completions
install -m 0755 target/release/yazi target/release/ya /out/
install -m 0644 \
  yazi-boot/completions/yazi.bash yazi-boot/completions/_yazi yazi-boot/completions/yazi.fish \
  yazi-cli/completions/ya.bash yazi-cli/completions/_ya yazi-cli/completions/ya.fish \
  /out/completions/
install -m 0644 assets/yazi.desktop assets/logo.png LICENSE LICENSE-ICONS /out/
chown -R "${HOST_UID}:${HOST_GID}" /out
