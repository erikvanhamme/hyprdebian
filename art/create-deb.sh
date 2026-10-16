#!/bin/bash
#
# Builds build/hyprdebian-art_<version>_all.deb, with the version from the
# file version next to this script. It installs:
#
#   /usr/share/backgrounds/hyprdebian/emerald-*.png   the wallpapers
#   /usr/share/doc/hyprdebian-art/copyright           deb/copyright
#
# Raise the number in version for every package that is handed out.

set -euo pipefail

ART_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${ART_DIR}/build"
DEB_NAME="hyprdebian-art"
DEB_ROOT="${BUILD}/deb"
BACKGROUNDS="/usr/share/backgrounds/hyprdebian"
DOC="/usr/share/doc/${DEB_NAME}"

DEB_VERSION="$(tr -d '[:space:]' < "${ART_DIR}/version")"
if [[ -z "${DEB_VERSION}" ]]; then
    echo "error: ${ART_DIR}/version is empty" >&2
    exit 1
fi

shopt -s nullglob
WALLPAPERS=("${ART_DIR}"/wallpapers/*.png)
shopt -u nullglob
if [[ ${#WALLPAPERS[@]} -eq 0 ]]; then
    echo "error: no wallpapers in ${ART_DIR}/wallpapers" >&2
    exit 1
fi

DEB="${BUILD}/${DEB_NAME}_${DEB_VERSION}_all.deb"

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
rm -rf "${DEB_ROOT}"
install -d "${DEB_ROOT}/DEBIAN" "${DEB_ROOT}${BACKGROUNDS}" "${DEB_ROOT}${DOC}"
install -m 0644 "${WALLPAPERS[@]}" "${DEB_ROOT}${BACKGROUNDS}/"
install -m 0644 "${ART_DIR}/deb/copyright" "${DEB_ROOT}${DOC}/copyright"

INSTALLED_SIZE="$(du -sk --apparent-size --exclude=DEBIAN "${DEB_ROOT}" | cut -f1)"
sed -e "s/@VERSION@/${DEB_VERSION}/" \
    -e "s/@INSTALLED_SIZE@/${INSTALLED_SIZE}/" \
    "${ART_DIR}/deb/control" > "${DEB_ROOT}/DEBIAN/control"

dpkg-deb --root-owner-group --build "${DEB_ROOT}" "${DEB}"
