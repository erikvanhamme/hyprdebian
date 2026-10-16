#!/bin/bash
#
# Builds build/fonts-jetbrainsmono-nerd_<version>_all.deb from the Nerd Fonts
# release zip, with the version of that release. It installs:
#
#   /usr/local/share/fonts/*.ttf                        the fonts, where the
#                                                       installer (o_font) puts them
#   /usr/share/doc/fonts-jetbrainsmono-nerd/copyright   the license file from the zip
#
# The zip is downloaded to and unpacked in a temporary directory in /tmp,
# which is removed when the script ends.

set -euo pipefail

ART_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${ART_DIR}/build"
DEB_NAME="fonts-jetbrainsmono-nerd"
DEB_ROOT="${BUILD}/font-deb"
FONTS="/usr/local/share/fonts"
DOC="/usr/share/doc/${DEB_NAME}"

NERD_FONTS_VERSION="3.4.0"
URL="https://github.com/ryanoasis/nerd-fonts/releases/download/v${NERD_FONTS_VERSION}/JetBrainsMono.zip"

DEB="${BUILD}/${DEB_NAME}_${NERD_FONTS_VERSION}_all.deb"

TMP="$(mktemp -d /tmp/create-font-deb.XXXXXX)"
trap 'rm -rf "${TMP}"' EXIT

wget -O "${TMP}/JetBrainsMono.zip" "${URL}"
unzip -q "${TMP}/JetBrainsMono.zip" -d "${TMP}/zip"

shopt -s nullglob nocaseglob
FONT_FILES=("${TMP}"/zip/*.ttf "${TMP}"/zip/*.otf)
COPYRIGHT_FILES=("${TMP}"/zip/OFL* "${TMP}"/zip/LICENSE* "${TMP}"/zip/LICENCE* "${TMP}"/zip/COPYING* "${TMP}"/zip/COPYRIGHT*)
shopt -u nullglob nocaseglob
if [[ ${#FONT_FILES[@]} -eq 0 ]]; then
    echo "error: no fonts in ${URL}" >&2
    exit 1
fi

# The package is built from a staged root in build/font-deb. dpkg-deb makes
# all files owned by root.
rm -rf "${DEB_ROOT}"
install -d "${DEB_ROOT}/DEBIAN" "${DEB_ROOT}${FONTS}" "${DEB_ROOT}${DOC}"
install -m 0644 "${FONT_FILES[@]}" "${DEB_ROOT}${FONTS}/"

if [[ ${#COPYRIGHT_FILES[@]} -gt 0 ]]; then
    install -m 0644 "${COPYRIGHT_FILES[0]}" "${DEB_ROOT}${DOC}/copyright"
else
    echo "warning: no copyright file in ${URL}" >&2
fi

INSTALLED_SIZE="$(du -sk --apparent-size --exclude=DEBIAN "${DEB_ROOT}" | cut -f1)"
cat > "${DEB_ROOT}/DEBIAN/control" <<EOF
Package: ${DEB_NAME}
Version: ${NERD_FONTS_VERSION}
Architecture: all
Maintainer: Erik Van Hamme <erik.vanhamme@gmail.com>
Installed-Size: ${INSTALLED_SIZE}
Section: fonts
Priority: optional
Description: JetBrains Mono Nerd Font
 The JetBrains Mono font patched with the Nerd Fonts glyphs, release
 v${NERD_FONTS_VERSION}, in ${FONTS}.
EOF

# fontconfig only has a dpkg trigger for /usr/share/fonts, so the font cache
# is refreshed here for /usr/local/share/fonts.
for script in postinst postrm; do
    cat > "${DEB_ROOT}/DEBIAN/${script}" <<EOF
#!/bin/sh
set -e
if command -v fc-cache >/dev/null 2>&1; then
    fc-cache -f ${FONTS} || true
fi
EOF
    chmod 0755 "${DEB_ROOT}/DEBIAN/${script}"
done

dpkg-deb --root-owner-group --build "${DEB_ROOT}" "${DEB}"
