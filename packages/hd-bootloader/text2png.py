#!/usr/bin/env python3
"""Renders a line of text into a PNG with a transparent background.

The image is exactly --height pixels high: the font is scaled so that its line
height (ascent plus descent) fills it. That way images of different texts in
the same font and height line up, whatever their letters. The image is as wide
as the text.

The font is a fontconfig name such as "JetBrainsMono Nerd Font Mono" (looked
up with fc-match) or a path to a font file.

Needs python3-pil.

Example:
    text2png.py --text "HyprDebian-Kernel-7.2.10" --color "#0066cc" \\
        --font "JetBrainsMono Nerd Font Mono" --height 65 --output label.png
"""

import argparse
import math
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# Font size used to measure the proportions of the font.
REFERENCE_SIZE = 1000


def font_file(font):
    if Path(font).is_file():
        return font
    result = subprocess.run(["fc-match", "-f", "%{file}", font], check=True, capture_output=True, text=True)
    return result.stdout


def render(text, color, font, height):
    path = font_file(font)
    ascent, descent = ImageFont.truetype(path, REFERENCE_SIZE).getmetrics()
    scaled = ImageFont.truetype(path, REFERENCE_SIZE * height / (ascent + descent))
    baseline = round(height * ascent / (ascent + descent))

    left, _, right, _ = scaled.getbbox(text, anchor="ls")
    x = -min(left, 0)
    width = x + max(right, math.ceil(scaled.getlength(text)))

    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    ImageDraw.Draw(image).text((x, baseline), text, font=scaled, fill=color, anchor="ls")
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--text", required=True, help="the text to render")
    parser.add_argument("--color", required=True, help='text colour, such as "#0066cc"')
    parser.add_argument("--font", required=True, help="fontconfig font name or font file")
    parser.add_argument("--height", type=int, required=True, help="height of the image in pixels")
    parser.add_argument("-o", "--output", type=Path, required=True, help="PNG to write")
    args = parser.parse_args()

    if args.height <= 0:
        parser.error("--height must be positive")

    render(args.text, args.color, args.font, args.height).save(args.output)


if __name__ == "__main__":
    main()
