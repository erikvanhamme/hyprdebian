#!/usr/bin/env python3
"""Draws the bootloader menu box over a wallpaper.

The box is --size percent of the width and of the height of the wallpaper (50
by default), with its centre at --x-center percent of the width and --y-center
percent of the height (50 by default, the centre of the wallpaper). It is
filled with --fill at --opacity, and surrounded by an opaque border, --border
pixels wide, in --border-color: the dark, opacity, border and highlight of the
theme default of hd-desktop by default. The border lies outside the box, so
the box itself keeps its size. The outer edge of the border is rounded with
--radius, and the box with the inner radius, radius minus border, so that it
follows the curve of the border, as in hd-menu. The curves are antialiased.

Needs python3-pil.

Example:
    menubox.py wallpaper.png --output background.png
"""

import argparse
from pathlib import Path

from PIL import Image, ImageColor, ImageDraw

# The highlight, border, radius, dark and opacity of the theme default of hd-desktop.
DEFAULT_BORDER_COLOR = "#ff6600"
DEFAULT_BORDER_WIDTH = 4
DEFAULT_RADIUS = 10
DEFAULT_FILL = "#071422"
DEFAULT_OPACITY = 0.8

# Size and centre of the box, in percent of the image.
DEFAULT_SIZE = 50
DEFAULT_X_CENTER = 50
DEFAULT_Y_CENTER = 50

# The rounded shapes are drawn this many times larger and scaled down, which
# antialiases their curves.
SUPERSAMPLE = 4


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


def rgba(color, opacity=1.0):
    """A colour such as "#071422" as red, green, blue and alpha from 0 to 255."""
    red, green, blue = ImageColor.getrgb(color)[:3]
    return red, green, blue, round(255 * opacity)


def draw_box(wallpaper, geometry, border_color, border_width, radius, fill, opacity):
    """Draws the box with the given geometry, as box_geometry returns it, over the wallpaper.

    The border, border_width pixels outside the box, is rounded with radius,
    and the box, filled with fill at opacity, with radius minus border_width.
    """
    left, top, box_width, box_height = geometry
    outer_width = box_width + 2 * border_width
    outer_height = box_height + 2 * border_width

    # The overlay covers the box and its border, drawn SUPERSAMPLE times
    # larger. The box replaces the border colour under it, so the wallpaper
    # shows through the box.
    n = SUPERSAMPLE
    overlay = Image.new("RGBA", (outer_width * n, outer_height * n), (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    if border_width > 0:
        draw.rounded_rectangle(
            (0, 0, outer_width * n - 1, outer_height * n - 1), radius=radius * n, fill=rgba(border_color)
        )
    draw.rounded_rectangle(
        (border_width * n, border_width * n, (border_width + box_width) * n - 1, (border_width + box_height) * n - 1),
        radius=max(0, radius - border_width) * n,
        fill=rgba(fill, opacity),
    )
    overlay = overlay.resize((outer_width, outer_height), Image.LANCZOS)

    result = wallpaper.convert("RGBA")
    result.alpha_composite(overlay, (left - border_width, top - border_width))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("wallpaper", type=Path, help="wallpaper PNG")
    parser.add_argument("-o", "--output", type=Path, required=True, help="PNG to write")
    parser.add_argument(
        "--border-color",
        default=DEFAULT_BORDER_COLOR,
        help=f"border colour (default: {DEFAULT_BORDER_COLOR})",
    )
    parser.add_argument(
        "--border",
        type=int,
        default=DEFAULT_BORDER_WIDTH,
        help=f"border width in pixels (default: {DEFAULT_BORDER_WIDTH})",
    )
    parser.add_argument(
        "--radius",
        type=int,
        default=DEFAULT_RADIUS,
        help=f"radius of the outer edge of the border in pixels (default: {DEFAULT_RADIUS})",
    )
    parser.add_argument("--fill", default=DEFAULT_FILL, help=f"colour of the box (default: {DEFAULT_FILL})")
    parser.add_argument(
        "--opacity",
        type=float,
        default=DEFAULT_OPACITY,
        help=f"opacity of the box, from 0 to 1 (default: {DEFAULT_OPACITY})",
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
        result = draw_box(
            wallpaper, geometry, args.border_color, args.border, args.radius, args.fill, args.opacity
        )
    result.convert("RGB").save(args.output)


if __name__ == "__main__":
    main()
