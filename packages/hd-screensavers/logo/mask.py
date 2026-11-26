#!/usr/bin/env python3
"""Turns the logo of hd-logo, rendered by rsvg-convert to a PNG, into the
mask that hd-logo draws: a C++ header with one channel, the coverage of the
logo, and all its mipmaps, so that hd-logo has no code to load images.

make all of Makefile runs it in the build image:

    rsvg-convert --height 512 hd-logo.svg > build/hd-logo.png
    mask.py build/hd-logo.png build/logo_mask.h

The mask is the alpha channel of the PNG: the colour of the SVG does not
count, hd-logo tints the mask with the colours of the theme. Each mipmap is
half the size of the one before, rounded down and at least 1, as Vulkan
sizes the levels of an image, down to 1 x 1; a pixel is the mean of the 2 x 2
pixels under it. In the header the levels follow each other, each from an
offset that is a multiple of 4, as vkCmdCopyBufferToImage wants it.
"""

import argparse
import struct
import sys
import zlib

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
# The colour types of PNG with an alpha channel, and their bytes per pixel at a depth of 8 bits.
GREY_ALPHA = 4
RGBA = 6
CHANNELS = {GREY_ALPHA: 2, RGBA: 4}


def read_png_alpha(path):
    """Reads the alpha channel of a PNG of 8 bits, not interlaced.

    Returns the width, the height and the alpha, a bytearray of width * height bytes, row by row.
    """
    with open(path, "rb") as f:
        data = f.read()
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError(f"{path} is not a PNG")
    pos = len(PNG_SIGNATURE)
    header = None
    idat = []
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"IEND":
            break
    if header is None:
        raise ValueError(f"{path} has no IHDR")
    width, height, depth, colour, _, _, interlace = header
    if depth != 8 or colour not in CHANNELS or interlace != 0:
        raise ValueError(f"{path}: only PNGs of 8 bits with alpha, not interlaced, will do")
    bpp = CHANNELS[colour]
    raw = zlib.decompress(b"".join(idat))
    stride = width * bpp
    alpha = bytearray(width * height)
    previous = bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        kind = raw[start]
        row = bytearray(raw[start + 1:start + 1 + stride])
        unfilter(kind, row, previous, bpp)
        alpha[y * width:(y + 1) * width] = row[bpp - 1::bpp]
        previous = row
    return width, height, alpha


def unfilter(kind, row, previous, bpp):
    """Undoes the filter of one row of a PNG in place, with the row above it unfiltered."""
    if kind == 0:
        return
    for i in range(len(row)):
        left = row[i - bpp] if i >= bpp else 0
        up = previous[i]
        if kind == 1:
            predictor = left
        elif kind == 2:
            predictor = up
        elif kind == 3:
            predictor = (left + up) // 2
        elif kind == 4:
            corner = previous[i - bpp] if i >= bpp else 0
            p = left + up - corner
            pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
            if pa <= pb and pa <= pc:
                predictor = left
            elif pb <= pc:
                predictor = up
            else:
                predictor = corner
        else:
            raise ValueError(f"unknown PNG filter {kind}")
        row[i] = (row[i] + predictor) & 0xFF


def halve(width, height, pixels):
    """The next mipmap: half the size, rounded down and at least 1, each pixel the mean of 2 x 2."""
    w = max(1, width // 2)
    h = max(1, height // 2)
    out = bytearray(w * h)
    for y in range(h):
        y0 = min(2 * y, height - 1) * width
        y1 = min(2 * y + 1, height - 1) * width
        for x in range(w):
            x0 = min(2 * x, width - 1)
            x1 = min(2 * x + 1, width - 1)
            total = pixels[y0 + x0] + pixels[y0 + x1] + pixels[y1 + x0] + pixels[y1 + x1]
            out[y * w + x] = (total + 2) // 4
    return w, h, out


def main():
    parser = argparse.ArgumentParser(description="Turns the PNG of the logo into the header of the mask of hd-logo.")
    parser.add_argument("png", help="the logo rendered by rsvg-convert")
    parser.add_argument("header", help="the C++ header to write")
    args = parser.parse_args()

    width, height, pixels = read_png_alpha(args.png)
    levels = [(width, height, pixels)]
    while width > 1 or height > 1:
        width, height, pixels = halve(width, height, pixels)
        levels.append((width, height, pixels))

    data = bytearray()
    offsets = []
    for _, _, level in levels:
        offsets.append(len(data))
        data += level
        data += bytes(-len(data) % 4)

    lines = [
        "// The mask of hd-logo, made by mask.py from the PNG of hd-logo.svg; do not edit.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        f"constexpr uint32_t LOGO_MASK_WIDTH = {levels[0][0]};",
        f"constexpr uint32_t LOGO_MASK_HEIGHT = {levels[0][1]};",
        f"constexpr uint32_t LOGO_MASK_LEVELS = {len(levels)};",
        "/** @brief Where each level starts in logo_mask, in bytes. */",
        "constexpr uint32_t LOGO_MASK_OFFSETS[LOGO_MASK_LEVELS] = {" + ", ".join(map(str, offsets)) + "};",
        "/** @brief The coverage of the logo, 0 to 255, level after level, row after row. */",
        f"constexpr uint8_t logo_mask[{len(data)}] = {{",
    ]
    for i in range(0, len(data), 32):
        lines.append("    " + ",".join(map(str, data[i:i + 32])) + ",")
    lines.append("};")
    with open(args.header, "w") as f:
        f.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
