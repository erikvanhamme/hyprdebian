#!/usr/bin/env python3
"""Draws the bootloader menu box over a wallpaper.

The box is --size percent of the width and of the height of the wallpaper (80
by default), with its centre at --x-center percent of the width and --y-center
percent of the height (50 by default, the centre of the wallpaper). It is
black at 80% opacity. It is surrounded by an opaque border, 5 pixels wide by
default, in the selection colour of the menu. The border lies outside the box,
so the box itself keeps its size.

Needs python3-pil.

Example:
    menubox.py wallpaper.png --output background.png
"""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

DEFAULT_BORDER_COLOR = "#ff6600"
DEFAULT_BORDER_WIDTH = 5

# Size and centre of the box, in percent of the image.
DEFAULT_SIZE = 80
DEFAULT_X_CENTER = 50
DEFAULT_Y_CENTER = 50

BOX_OPACITY = 0.8


def box_geometry(width, height, size=DEFAULT_SIZE, x_center=DEFAULT_X_CENTER, y_center=DEFAULT_Y_CENTER):
    """Returns left, top, width and height of the box on an image of the given size.

    size, x_center and y_center are in percent of the image. Raises
    ValueError if the box is empty or does not lie within the image.
    """
    box_width = round(width * size / 100)
    box_height = round(height * size / 100)
    left = round(width * x_center / 100 - box_width / 2)
    top = round(height * y_center / 100 - box_height / 2)
    if box_width <= 0 or box_height <= 0:
        raise ValueError(f"a box of {size}% of {width}x{height} is empty")
    if left < 0 or top < 0 or left + box_width > width or top + box_height > height:
        raise ValueError(
            f"a box of {size}% of {width}x{height} centred at {x_center}%, {y_center}% "
            "does not fit in the image"
        )
    return left, top, box_width, box_height


def draw_box(wallpaper, border_color, border_width, geometry):
    """Draws the box with the given geometry, as box_geometry returns it, over the wallpaper."""
    left, top, box_width, box_height = geometry
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
    parser.add_argument(
        "--size",
        type=float,
        default=DEFAULT_SIZE,
        help=f"width and height of the box in percent of the wallpaper (default: {DEFAULT_SIZE})",
    )
    parser.add_argument(
        "--x-center",
        type=float,
        default=DEFAULT_X_CENTER,
        help=f"x of the centre of the box in percent of the width (default: {DEFAULT_X_CENTER})",
    )
    parser.add_argument(
        "--y-center",
        type=float,
        default=DEFAULT_Y_CENTER,
        help=f"y of the centre of the box in percent of the height (default: {DEFAULT_Y_CENTER})",
    )
    args = parser.parse_args()

    with Image.open(args.wallpaper) as wallpaper:
        try:
            geometry = box_geometry(*wallpaper.size, args.size, args.x_center, args.y_center)
        except ValueError as error:
            parser.error(str(error))
        result = draw_box(wallpaper, args.border_color, args.border_width, geometry)
    result.convert("RGB").save(args.output)


if __name__ == "__main__":
    main()
