#!/usr/bin/env python3
"""Draws the bootloader menu box over a wallpaper.

The box is centred, 80% of the width and 80% of the height of the wallpaper,
and black at 80% opacity. It is surrounded by an opaque border, 5 pixels wide
by default, in the selection colour of the menu (border-color in the mako
config that hyprdebian installs). The border lies outside the box, so the box
itself keeps its 80% size.

Needs python3-pil.

Example:
    menubox.py wallpaper.png --output background.png
"""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

DEFAULT_BORDER_COLOR = "#ff6600"
DEFAULT_BORDER_WIDTH = 5

BOX_SIZE = 0.8
BOX_OPACITY = 0.8


def box_geometry(width, height):
    """Returns left, top, width and height of the box on an image of the given size."""
    box_width = round(width * BOX_SIZE)
    box_height = round(height * BOX_SIZE)
    return (width - box_width) // 2, (height - box_height) // 2, box_width, box_height


def draw_box(wallpaper, border_color, border_width):
    left, top, box_width, box_height = box_geometry(*wallpaper.size)
    right = left + box_width - 1
    bottom = top + box_height - 1

    overlay = Image.new("RGBA", wallpaper.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    draw.rectangle(
        (left - border_width, top - border_width, right + border_width, bottom + border_width),
        fill=border_color,
    )
    draw.rectangle((left, top, right, bottom), fill=(0, 0, 0, round(255 * BOX_OPACITY)))

    # The box replaces the border colour under it before the overlay is laid
    # over the wallpaper, so the wallpaper shows through the box.
    return Image.alpha_composite(wallpaper.convert("RGBA"), overlay)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("wallpaper", type=Path, help="wallpaper PNG")
    parser.add_argument("-o", "--output", type=Path, required=True, help="PNG to write")
    parser.add_argument(
        "--border-color",
        default=DEFAULT_BORDER_COLOR,
        help=f"border colour (default: {DEFAULT_BORDER_COLOR}, the selection colour)",
    )
    parser.add_argument(
        "--border-width",
        type=int,
        default=DEFAULT_BORDER_WIDTH,
        help=f"border width in pixels (default: {DEFAULT_BORDER_WIDTH})",
    )
    args = parser.parse_args()

    with Image.open(args.wallpaper) as wallpaper:
        result = draw_box(wallpaper, args.border_color, args.border_width)
    result.convert("RGB").save(args.output)


if __name__ == "__main__":
    main()
