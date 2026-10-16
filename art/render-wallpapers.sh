#!/bin/bash
#
# Renders the wallpapers in wallpapers/ from the Emerald theme SVGs of the
# Debian package desktop-base, which must be installed.
#
# The SVGs are layers of gradients at low opacity. Inkscape renders those
# with 8 bits per channel and no dithering, which shows as bands. Chromium
# dithers gradients, so the renders are screenshots of headless Chromium.
#
# Each render shows an area of an SVG, in SVG units, scaled to the output
# size. The 1920x1080 and 1600x1200 SVGs leave the outermost pixel or two of
# the canvas uncovered, so their area is inset a little. The 3440x1440 render
# is the middle band of the 3840x2160 SVG.

set -euo pipefail

ART_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${ART_DIR}/wallpapers"
SVGS="/usr/share/desktop-base/emerald-theme/wallpaper/contents/images"

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

# render <svg> <svg width> <svg height> <x> <y> <w> <h> <width> <height> <name>
render() {
    local svg="$1" svg_w="$2" svg_h="$3" x="$4" y="$5" w="$6" h="$7"
    local width="$8" height="$9" name="${10}"
    local css

    css="$(python3 -I -c '
import sys
svg_w, svg_h, x, y, w, h, width, height = map(float, sys.argv[1:])
sx, sy = width / w, height / h
print(f"left:{-x * sx}px;top:{-y * sy}px;width:{svg_w * sx}px;height:{svg_h * sy}px")
' "${svg_w}" "${svg_h}" "${x}" "${y}" "${w}" "${h}" "${width}" "${height}")"

    cat > "${WORK}/page.html" <<EOF
<!doctype html>
<html><head><style>
html, body { margin: 0; overflow: hidden; background: transparent; }
img { position: absolute; ${css}; }
</style></head>
<body><img src="file://${SVGS}/${svg}"></body></html>
EOF

    # A transparent page background, so the check below finds pixels the
    # SVG does not cover.
    chromium --headless --disable-gpu --no-sandbox \
        --user-data-dir="${WORK}/profile" \
        --hide-scrollbars --force-device-scale-factor=1 \
        --force-color-profile=srgb --default-background-color=00000000 \
        --window-size="${width},${height}" \
        --screenshot="${WORK}/shot.png" "file://${WORK}/page.html" \
        > /dev/null 2>&1

    python3 -I -c '
import sys
from PIL import Image
shot, out, width, height = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
image = Image.open(shot).convert("RGBA")
if image.size != (width, height):
    sys.exit(f"error: {out}: got {image.size}, expected {(width, height)}")
if image.getchannel("A").getextrema()[0] != 255:
    sys.exit(f"error: {out}: the SVG does not cover the whole image")
image.convert("RGB").save(out, optimize=True)
' "${WORK}/shot.png" "${OUT}/${name}" "${width}" "${height}"

    echo "${OUT}/${name}"
}

mkdir -p "${OUT}"

render 3840x2160.svg 3840 2160 0 0 3840 2160 3840 2160 emerald-3840x2160.png
render 3840x2160.svg 3840 2160 0 276.279 3840 1607.442 3440 1440 emerald-3440x1440.png
render 1920x1080.svg 1920 1080 2 1.125 1916 1077.75 1920 1080 emerald-1920x1080.png
render 1600x1200.svg 1600 1200 4 3 1592 1194 1024 768 emerald-1024x768.png
