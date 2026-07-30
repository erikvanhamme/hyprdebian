#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="debian:sid"
CONTAINER_NAME="yazi_builder_$(date +%s)"
OUTPUT_DIR="$(pwd)/output"
REPO_URL="https://github.com/sxyazi/yazi.git"

# Capture host user's UID and GID
HOST_UID="$(id -u)"
HOST_GID="$(id -g)"

echo "==> Preparing output directory at ${OUTPUT_DIR}..."
mkdir -p "${OUTPUT_DIR}"

echo "==> Pulling latest ${IMAGE_NAME} Docker image..."
docker pull "${IMAGE_NAME}"

echo "==> Running build environment in Docker container..."
docker run --rm \
  --name "${CONTAINER_NAME}" \
  -e HOST_UID="${HOST_UID}" \
  -e HOST_GID="${HOST_GID}" \
  -v "${OUTPUT_DIR}:/out" \
  "${IMAGE_NAME}" \
  bash -c '
    set -euo pipefail

    echo "--> Updating APT package lists..."
    apt-get update

    echo "--> Installing build dependencies..."
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
      git \
      build-essential \
      pkg-config \
      cargo \
      rustc \
      clang \
      dpkg-dev

    echo "--> Cloning Yazi repository..."
    git clone --depth 1 "'"${REPO_URL}"'" /usr/src/yazi
    cd /usr/src/yazi

    echo "--> Compiling release binaries (yazi and ya)..."
    cargo build --release

    echo "--> Assembling .deb package structure..."
    VERSION=$(cargo metadata --format-version 1 | grep -oP "(?<=\"name\":\"yazi-fm\",\"version\":\")[^\"]+" | head -n 1)
    ARCH=$(dpkg --print-architecture)
    PKG_DIR="/tmp/yazi_${VERSION}_${ARCH}"

    # Create Debian directory layout
    mkdir -p "${PKG_DIR}/usr/bin"
    mkdir -p "${PKG_DIR}/usr/share/applications"
    mkdir -p "${PKG_DIR}/usr/share/pixmaps"
    mkdir -p "${PKG_DIR}/usr/share/bash-completion/completions"
    mkdir -p "${PKG_DIR}/usr/share/zsh/site-functions"
    mkdir -p "${PKG_DIR}/usr/share/fish/vendor_completions.d"
    mkdir -p "${PKG_DIR}/DEBIAN"

    # Copy compiled binaries (yazi terminal manager + ya CLI tool)
    cp target/release/yazi "${PKG_DIR}/usr/bin/"
    cp target/release/ya "${PKG_DIR}/usr/bin/"

    # Desktop entry & Icon
    if [ -f yazi-cli/assets/yazi.desktop ]; then
      cp yazi-cli/assets/yazi.desktop "${PKG_DIR}/usr/share/applications/"
    elif [ -f assets/yazi.desktop ]; then
      cp assets/yazi.desktop "${PKG_DIR}/usr/share/applications/"
    fi

    if [ -f yazi-cli/assets/logo.png ]; then
      cp yazi-cli/assets/logo.png "${PKG_DIR}/usr/share/pixmaps/yazi.png"
    elif [ -f assets/logo.png ]; then
      cp assets/logo.png "${PKG_DIR}/usr/share/pixmaps/yazi.png"
    fi

    # Shell completions (if present in build outputs/assets)
    [ -f yazi-cli/assets/completions/yazi.bash ] && cp yazi-cli/assets/completions/yazi.bash "${PKG_DIR}/usr/share/bash-completion/completions/yazi"
    [ -f yazi-cli/assets/completions/_yazi ] && cp yazi-cli/assets/completions/_yazi "${PKG_DIR}/usr/share/zsh/site-functions/_yazi"
    [ -f yazi-cli/assets/completions/yazi.fish ] && cp yazi-cli/assets/completions/yazi.fish "${PKG_DIR}/usr/share/fish/vendor_completions.d/yazi.fish"

    # Generate control file
    cat <<EOF > "${PKG_DIR}/DEBIAN/control"
Package: yazi
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: HyprDebian Local Builder <erik.vanhamme@gmail.com>
Recommends: ffmpeg, 7zip, jq, poppler-utils, fd-find, ripgrep, fzf, zoxide
Section: utils
Priority: optional
Description: Blazing fast terminal file manager written in Rust
 Yazi is an async I/O terminal file manager that provides an intuitive
 multi-column interface, image previews via Kitty/Sixel graphics protocol,
 built-in code highlighting, and async plugin support.
EOF

    echo "--> Building .deb package..."
    dpkg-deb --build --root-owner-group "${PKG_DIR}" /out/

    echo "--> Adjusting ownership to host user (${HOST_UID}:${HOST_GID})..."
    chown -R "${HOST_UID}:${HOST_GID}" /out/

    echo "--> Build completed successfully!"
'

echo "=================================================="
echo "SUCCESS: Yazi .deb package generated at:"
ls -lh "${OUTPUT_DIR}"/*.deb
echo "=================================================="
