#!/usr/bin/env bash
set -euo pipefail

# Path configuration
OUTPUT_DIR="$(pwd)/output"
REPO_DIR="$(pwd)/repo"
POOL_DIR="${REPO_DIR}/pool/main"
DISTS_DIR="${REPO_DIR}/dists/sid/main/binary-amd64"

echo "==> Setting up repository directories..."
mkdir -p "${POOL_DIR}"
mkdir -p "${DISTS_DIR}"

# Process output packages
DEB_FILES=$(find "${OUTPUT_DIR}" -maxdepth 1 -name "*.deb" 2>/dev/null)

if [ -n "${DEB_FILES}" ]; then
    for deb in ${DEB_FILES}; do
        # Extract package name (e.g., 'yazi' or 'wiremix')
        PKG_NAME=$(dpkg-deb -f "${deb}" Package)

        echo "==> Cleaning up older versions of ${PKG_NAME} in pool..."
        # Remove old files starting with the same package name pattern
        rm -f "${POOL_DIR}/${PKG_NAME}_"*.deb

        echo "==> Moving ${deb} to ${POOL_DIR}..."
        mv "${deb}" "${POOL_DIR}/"
    done
else
    echo "==> No new .deb packages found in ${OUTPUT_DIR}. Updating indices with existing pool..."
fi

echo "==> Generating Packages index..."
cd "${REPO_DIR}"
apt-ftparchive packages pool/main > dists/sid/main/binary-amd64/Packages
gzip -k -f dists/sid/main/binary-amd64/Packages

echo "==> Generating Release file..."
cat <<EOF > dists/sid/main/binary-amd64/Release
Archive: sid
Component: main
Origin: HyprDebian Local Repository
Label: HyprDebian Local Repository
Architecture: amd64
EOF

echo "=================================================="
echo "SUCCESS: Debian repository index updated!"
echo "=================================================="
