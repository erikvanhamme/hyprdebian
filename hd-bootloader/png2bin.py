#!/usr/bin/env python3
"""Converts a PNG into the raw image format the bootloader blits.

Output layout (all little-endian):
    uint32 width
    uint32 height
    width * height pixels, row by row from the top, 4 bytes each: B, G, R, A

B, G, R matches the PixelBlueGreenRedReserved8BitPerColor framebuffer format
that UEFI firmware (OVMF and most real hardware) uses. The fourth byte holds
the alpha channel, which the bootloader uses to blend the image.

Only 8-bit, non-interlaced RGB or RGBA PNGs are supported, which is what
Inkscape and most tools write. Uses the Python standard library only.
"""

import struct
import sys
import zlib

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
COLOR_TYPE_RGB = 2
COLOR_TYPE_RGBA = 6


def read_chunks(data):
    pos = len(PNG_SIGNATURE)
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos : pos + 4])
        kind = data[pos + 4 : pos + 8]
        yield kind, data[pos + 8 : pos + 8 + length]
        pos += 12 + length


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def unfilter(raw, width, height, bpp):
    stride = width * bpp
    rows = []
    prev = bytearray(stride)
    pos = 0
    for _ in range(height):
        filter_type = raw[pos]
        row = bytearray(raw[pos + 1 : pos + 1 + stride])
        pos += 1 + stride
        for i in range(stride):
            a = row[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if filter_type == 1:
                row[i] = (row[i] + a) & 0xFF
            elif filter_type == 2:
                row[i] = (row[i] + b) & 0xFF
            elif filter_type == 3:
                row[i] = (row[i] + (a + b) // 2) & 0xFF
            elif filter_type == 4:
                row[i] = (row[i] + paeth(a, b, c)) & 0xFF
            elif filter_type != 0:
                sys.exit(f"unknown PNG filter type {filter_type}")
        rows.append(row)
        prev = row
    return rows


def convert(png):
    if not png.startswith(PNG_SIGNATURE):
        sys.exit("not a PNG file")

    header = None
    idat = bytearray()
    for kind, body in read_chunks(png):
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body

    width, height, depth, color_type, _, _, interlace = header
    if depth != 8 or interlace != 0 or color_type not in (COLOR_TYPE_RGB, COLOR_TYPE_RGBA):
        sys.exit("only 8-bit, non-interlaced RGB or RGBA PNGs are supported")

    bpp = 4 if color_type == COLOR_TYPE_RGBA else 3
    rows = unfilter(zlib.decompress(idat), width, height, bpp)

    out = bytearray(struct.pack("<II", width, height))
    for row in rows:
        for i in range(0, len(row), bpp):
            r, g, b = row[i], row[i + 1], row[i + 2]
            a = row[i + 3] if bpp == 4 else 0xFF
            out += bytes((b, g, r, a))
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit(f"usage: {sys.argv[0]} input.png output.bin")
    with open(sys.argv[1], "rb") as f:
        png = f.read()
    with open(sys.argv[2], "wb") as f:
        f.write(convert(png))


if __name__ == "__main__":
    main()
