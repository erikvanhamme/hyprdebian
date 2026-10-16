"""Builds the assets of the graphical frontend of the hyprdebian bootloader (graphical.cpp).

update-bootloader.py adds them to assets.efi when bootloader.json has
"frontend": "graphical":

    .backgnd  the wallpaper, scaled and cropped to res_x x res_y, with the
              work area (the menu box of menubox.py) over it; the bootloader
              takes its resolution from this image
    .area     the left, top, width and height of the work area in pixels,
              each a 32-bit little-endian unsigned integer
    .banner   banner_text in white
    .lblN     the label of menu row N, unselected
    .lblNs    the label of menu row N, selected

The work area holds ten rows of text, so the banner and the menu labels are
rendered at a tenth of its height. Images are in the raw image format of
png2bin.py.

Config keys of the graphical frontend, besides those of every frontend (see
update-bootloader.py), of which it uses banner_text:

    wallpaper       PNG or other image; a relative path is relative to the
                    config file
    res_x, res_y    display resolution of the bootloader: the size of the
                    background
    font            fontconfig name or file of the font for all texts
    color           colour of unselected menu labels
    selected_color  colour of the selected menu label and the menu box border
    border_width    width of the menu box border in pixels
    work_area_size  width and height of the work area in percent of res_x
                    and res_y; the texts scale with it
    x_center        x of the centre of the work area in percent of res_x
    y_center        y of the centre of the work area in percent of res_y

The work area, without its border, must lie within the screen.

Their defaults are in DEFAULTS below; the package installs them in its config,
deb/graphical/bootloader.json, and the two should match.

Needs python3-pil, and fontconfig for the font.
"""

import io
import struct
import sys

from PIL import Image

import menubox
import png2bin
import text2png

# The config keys of the graphical frontend, with their defaults.
DEFAULTS = {
    "wallpaper": "/usr/share/backgrounds/hyprdebian/emerald-1920x1080.png",
    "res_x": 1920,
    "res_y": 1080,
    "font": "JetBrainsMono Nerd Font Mono",
    "color": "#0066cc",
    "selected_color": "#ff6600",
    "border_width": 5,
    "work_area_size": menubox.DEFAULT_SIZE,
    "x_center": menubox.DEFAULT_X_CENTER,
    "y_center": menubox.DEFAULT_Y_CENTER,
}
# Those of its keys that are file paths.
PATH_KEYS = ["wallpaper"]

# Rows of text that fit in the menu box.
ROWS = 10
BANNER_COLOR = "#ffffff"


def to_bin(image):
    """Converts a Pillow image to the bootloader's raw image format with png2bin.py."""
    png = io.BytesIO()
    image.save(png, "PNG")
    return png2bin.convert(png.getvalue())


def scale_to_cover(image, width, height):
    """Scales the image to cover width x height and crops it to that, centred."""
    scale = max(width / image.width, height / image.height)
    scaled = image.resize((round(image.width * scale), round(image.height * scale)), Image.LANCZOS)
    left = (scaled.width - width) // 2
    top = (scaled.height - height) // 2
    return scaled.crop((left, top, left + width, top + height))


def sections(config, labels):
    """Returns the {name: data} sections of the frontend for the menu rows with the given label texts.

    labels holds the text of each row, top to bottom; row N (from 1) gets
    .lblN and .lblNs.
    """
    res_x, res_y = config["res_x"], config["res_y"]

    for key in ["work_area_size", "x_center", "y_center"]:
        value = config[key]
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            sys.exit(f"{key} must be a number (percent), not {value!r}")
    try:
        geometry = menubox.box_geometry(
            res_x, res_y, config["work_area_size"], config["x_center"], config["y_center"]
        )
    except ValueError as error:
        sys.exit(f"work_area_size, x_center and y_center: {error}")

    # The background: the wallpaper with the menu box.
    with Image.open(config["wallpaper"]) as wallpaper:
        background = scale_to_cover(wallpaper.convert("RGB"), res_x, res_y)
    background = menubox.draw_box(
        background, config["selected_color"], config["border_width"], geometry
    ).convert("RGB")

    # A tenth of the height of the work area is the height of the texts.
    _, _, _, box_height = geometry
    text_height = box_height // ROWS

    result = {
        ".backgnd": to_bin(background),
        ".area": struct.pack("<4I", *geometry),
        ".banner": to_bin(text2png.render(config["banner_text"], BANNER_COLOR, config["font"], text_height)),
    }
    for row, text in enumerate(labels, start=1):
        result[f".lbl{row}"] = to_bin(text2png.render(text, config["color"], config["font"], text_height))
        result[f".lbl{row}s"] = to_bin(text2png.render(text, config["selected_color"], config["font"], text_height))
    return result
