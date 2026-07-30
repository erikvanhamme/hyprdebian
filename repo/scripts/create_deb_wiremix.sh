#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="debian:sid"
CONTAINER_NAME="wiremix_builder_$(date +%s)"
OUTPUT_DIR="$(pwd)/output"
REPO_URL="https://github.com/tsowell/wiremix.git"

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
      libpipewire-0.3-dev \
      dpkg-dev

    echo "--> Cloning Wiremix repository..."
    git clone --depth 1 "'"${REPO_URL}"'" /usr/src/wiremix
    cd /usr/src/wiremix

    echo "--> Compiling release binary..."
    cargo build --release

    echo "--> Assembling .deb package structure..."
    VERSION=$(cargo metadata --format-version 1 | grep -oP "(?<=\"name\":\"wiremix\",\"version\":\")[^\"]+")
    ARCH=$(dpkg --print-architecture)
    PKG_DIR="/tmp/wiremix_${VERSION}_${ARCH}"

    mkdir -p "${PKG_DIR}/usr/bin"
    mkdir -p "${PKG_DIR}/DEBIAN"

    # Install binary
    cp target/release/wiremix "${PKG_DIR}/usr/bin/"

    # Generate control file
    cat <<EOF > "${PKG_DIR}/DEBIAN/control"
Package: wiremix
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: HyprDebian Local Builder <erik.vanhamme@gmail.com>
Depends: libpipewire-0.3-0
Section: sound
Priority: optional
Description: Simple TUI audio mixer for PipeWire.
 Wiremix is a terminal-based mixer designed for PipeWire, offering
 volume control, stream routing, and device configuration.
EOF

    echo "--> Building .deb package..."
    dpkg-deb --build --root-owner-group "${PKG_DIR}" /out/

    echo "--> Adjusting ownership to host user (${HOST_UID}:${HOST_GID})..."
    chown -R "${HOST_UID}:${HOST_GID}" /out/

    echo "--> Build completed successfully!"
'

echo "=================================================="
echo "SUCCESS: Wiremix .deb package generated at:"
ls -lh "${OUTPUT_DIR}"/*.deb
echo "=================================================="
