#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="debian:sid"
CONTAINER_NAME="orcaslicer_builder_$(date +%s)"
OUTPUT_DIR="$(pwd)/output"
APPIMAGE_URL="https://github.com/OrcaSlicer/OrcaSlicer/releases/download/v2.4.2/OrcaSlicer_Linux_AppImage_Ubuntu2404_V2.4.2.AppImage"
ICON_URL="https://www.orcaslicer.com/images/OrcaSlicer_gradient_circle.svg"

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
  -e APPIMAGE_URL="${APPIMAGE_URL}" \
  -e ICON_URL="${ICON_URL}" \
  -v "${OUTPUT_DIR}:/out" \
  "${IMAGE_NAME}" \
  bash -c '
    set -euo pipefail

    echo "--> Updating APT package lists..."
    apt-get update

    echo "--> Installing build dependencies..."
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
      wget \
      dpkg-dev

    echo "--> Downloading AppImage..."
    wget ${APPIMAGE_URL}
    chmod +x OrcaSlicer_Linux_AppImage_Ubuntu2404_V2.4.2.AppImage

    echo "--> Downloading Icon..."
    wget ${ICON_URL}

    echo "--> Assembling .deb package structure..."
    VERSION="2.4.2"
    ARCH=$(dpkg --print-architecture)
    PKG_DIR="/tmp/OrcaSlicer_${VERSION}_${ARCH}"

    mkdir -p "${PKG_DIR}/usr/bin"
    mkdir -p "${PKG_DIR}/DEBIAN"
    mkdir -p "${PKG_DIR}/usr/share/applications"
    mkdir -p "${PKG_DIR}/usr/share/icons/hicolor/scalable/apps"

    # Install binary
    cp OrcaSlicer_Linux_AppImage_Ubuntu2404_V2.4.2.AppImage "${PKG_DIR}/usr/bin/orcaslicer"

    # Install icon
    cp OrcaSlicer_gradient_circle.svg "${PKG_DIR}/usr/share/icons/hicolor/scalable/apps/orcaslicer.svg"

    # Generate .desktop file
    cat <<EOF > "${PKG_DIR}/usr/share/applications/orcaslicer.desktop"
[Desktop Entry]
Name=Orca Slicer
GenericName=3D Printing Software
Comment=G-code generator for 3D printers
Exec=/usr/bin/orcaslicer %F
Icon=orcaslicer
Terminal=false
Type=Application
Categories=Development;Engineering;3DPrinting;
MimeType=model/stl;application/vnd.ms-3durl;
EOF

    # Generate control file
    cat <<EOF > "${PKG_DIR}/DEBIAN/control"
Package: orcaslicer
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: HyprDebian Local Builder <erik.vanhamme@gmail.com>
Depends: libjavascriptcoregtk-4.1-0,
         libwebkit2gtk-4.1-0
Section: tools
Priority: optional
Description: OrcaSlicer AppImage installer for hyprdebian.
 Installs the AppImage and sets up the .desktop file.
EOF

    echo "--> Building .deb package..."
    dpkg-deb --build --root-owner-group "${PKG_DIR}" /out/

    echo "--> Adjusting ownership to host user (${HOST_UID}:${HOST_GID})..."
    chown -R "${HOST_UID}:${HOST_GID}" /out/

    echo "--> Build completed successfully!"
'

echo "=================================================="
echo "SUCCESS: OrcaSlicer .deb package generated at:"
ls -lh "${OUTPUT_DIR}"/*.deb
echo "=================================================="
