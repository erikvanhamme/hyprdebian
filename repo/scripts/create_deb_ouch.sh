#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="debian:sid"
CONTAINER_NAME="ouch_builder_$(date +%s)"
OUTPUT_DIR="$(pwd)/output"
REPO_URL="https://github.com/ouch-org/ouch"

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

    echo "--> Cloning Ouch repository..."
    git clone --depth 1 "'"${REPO_URL}"'" /usr/src/ouch
    cd /usr/src/ouch

    echo "--> Compiling release binary..."
    cargo build --release

    echo "--> Assembling .deb package structure..."
    VERSION=$(cargo metadata --format-version 1 | grep -oP "(?<=\"name\":\"ouch\",\"version\":\")[^\"]+")
    ARCH=$(dpkg --print-architecture)
    PKG_DIR="/tmp/ouch_${VERSION}_${ARCH}"

    mkdir -p "${PKG_DIR}/usr/bin"
    mkdir -p "${PKG_DIR}/DEBIAN"

    # Install binary
    cp target/release/ouch "${PKG_DIR}/usr/bin/"

    # Generate control file
    cat <<EOF > "${PKG_DIR}/DEBIAN/control"
Package: ouch
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: HyprDebian Local Builder <erik.vanhamme@gmail.com>
Depends: 
Section: tools
Priority: optional
Description: Compression and decompression utility.
EOF

    echo "--> Building .deb package..."
    dpkg-deb --build --root-owner-group "${PKG_DIR}" /out/

    echo "--> Adjusting ownership to host user (${HOST_UID}:${HOST_GID})..."
    chown -R "${HOST_UID}:${HOST_GID}" /out/

    echo "--> Build completed successfully!"
'

echo "=================================================="
echo "SUCCESS: Ouch .deb package generated at:"
ls -lh "${OUTPUT_DIR}"/*.deb
echo "=================================================="
