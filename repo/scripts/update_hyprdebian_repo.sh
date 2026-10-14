#!/usr/bin/env bash
set -euo pipefail

# Moves the packages in ./output into the hyprdebian repository, makes the indices and signs
# them. Run it as the user that setup_hyprdebian_repo.sh set up, from this directory.

CONFIG_FILE="/etc/hyprdebian-repo.conf"

if [ ! -f "${CONFIG_FILE}" ]; then
    echo "Error: ${CONFIG_FILE} does not exist, run setup_hyprdebian_repo.sh first." >&2
    exit 1
fi

source "${CONFIG_FILE}"

# The key and the repository belong to this user. Running as root would leave files that the
# next update cannot change.
if [ "$(id -un)" != "${REPO_USER}" ]; then
    echo "Error: This script must be run as ${REPO_USER}, without sudo." >&2
    exit 1
fi

# Path configuration
OUTPUT_DIR="$(pwd)/output"
POOL_DIR="${REPO_DIR}/pool/main"
DISTS_DIR="${REPO_DIR}/dists/sid"
BINARY_DIR="${DISTS_DIR}/main/binary-amd64"
KEY_FILE="${REPO_DIR}/hyprdebian-archive-keyring.pgp"
GPG=(gpg --homedir "${GNUPGHOME}" --batch --yes)

echo "==> Setting up repository directories..."
mkdir -p "${POOL_DIR}"
mkdir -p "${BINARY_DIR}"

# Process output packages
shopt -s nullglob
DEB_FILES=("${OUTPUT_DIR}"/*.deb)
shopt -u nullglob

if [ ${#DEB_FILES[@]} -gt 0 ]; then
    # Only new packages change the repository, a run without them only signs it again.
    SNAPSHOT="${REPO_DATASET}@update-$(date +%Y%m%d-%H%M%S)"
    echo "==> Taking snapshot ${SNAPSHOT}..."
    /usr/sbin/zfs snapshot "${SNAPSHOT}"

    for deb in "${DEB_FILES[@]}"; do
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

# The indices and the Release file are made and signed in a temporary directory, then moved
# into place together. The Release file then only lists the new indices, and clients never
# see indices without the signed Release that matches them.
TMP_DIR=$(mktemp -d)
trap 'rm -rf "${TMP_DIR}"' EXIT
mkdir -p "${TMP_DIR}/sid/main/binary-amd64"

echo "==> Generating Packages index..."
cd "${REPO_DIR}"
apt-ftparchive packages pool/main > "${TMP_DIR}/sid/main/binary-amd64/Packages"
gzip -k -f "${TMP_DIR}/sid/main/binary-amd64/Packages"

# The Release file lists the hashes of the indices, the signature on it covers the packages.
echo "==> Generating Release file..."
apt-ftparchive \
    -o APT::FTPArchive::Release::Origin="HyprDebian Local Repository" \
    -o APT::FTPArchive::Release::Label="HyprDebian Local Repository" \
    -o APT::FTPArchive::Release::Suite="unstable" \
    -o APT::FTPArchive::Release::Codename="sid" \
    -o APT::FTPArchive::Release::Architectures="amd64" \
    -o APT::FTPArchive::Release::Components="main" \
    release "${TMP_DIR}/sid" > "${TMP_DIR}/Release"

echo "==> Signing Release file..."
"${GPG[@]}" --clearsign -o "${TMP_DIR}/InRelease" "${TMP_DIR}/Release"
"${GPG[@]}" --armor --detach-sign -o "${TMP_DIR}/Release.gpg" "${TMP_DIR}/Release"

mv "${TMP_DIR}/sid/main/binary-amd64/Packages" "${TMP_DIR}/sid/main/binary-amd64/Packages.gz" "${BINARY_DIR}/"
mv "${TMP_DIR}/Release" "${TMP_DIR}/Release.gpg" "${TMP_DIR}/InRelease" "${DISTS_DIR}/"

# Clients ask for the suite name, unstable.
ln -sfn sid "${REPO_DIR}/dists/unstable"

# The installer downloads the public key from here.
echo "==> Exporting the public key to ${KEY_FILE}..."
"${GPG[@]}" --export -o "${KEY_FILE}"

echo "=================================================="
echo "SUCCESS: Debian repository index updated and signed!"
echo "=================================================="
