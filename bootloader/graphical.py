"""Builds the assets of the graphical frontend of the hyprdebian bootloader (graphical.cpp).

update-bootloader.py adds them to assets.efi when bootloader.json has
"frontend": "graphical":

    .backgnd  the wallpaper, scaled and cropped to res_x x res_y, with the
              work area (the menu box of menubox.py) over it; the bootloader
              takes its resolution from this image
    .banner   banner_text in white
    .lblN     the label of menu row N, unselected
    .lblNs    the label of menu row N, selected

The work area holds ten rows of text, so the banner and the menu labels are
rendered at a tenth of its height. Images are in the raw image format of
png2bin.py.

Config keys it uses: wallpaper, res_x, res_y, banner_text, font, color,
selected_color and border_width; see update-bootloader.py.

Needs python3-pil.
"""

import io

from PIL import Image

import menubox
import png2bin
import text2png

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

    # The background: the wallpaper with the menu box.
    with Image.open(config["wallpaper"]) as wallpaper:
        background = scale_to_cover(wallpaper.convert("RGB"), res_x, res_y)
    background = menubox.draw_box(background, config["selected_color"], config["border_width"]).convert("RGB")

    # A tenth of the height of the work area is the height of the texts.
    _, _, _, box_height = menubox.box_geometry(res_x, res_y)
    text_height = box_height // ROWS

    result = {
        ".backgnd": to_bin(background),
        ".banner": to_bin(text2png.render(config["banner_text"], BANNER_COLOR, config["font"], text_height)),
    }
    for row, text in enumerate(labels, start=1):
        result[f".lbl{row}"] = to_bin(text2png.render(text, config["color"], config["font"], text_height))
        result[f".lbl{row}s"] = to_bin(text2png.render(text, config["selected_color"], config["font"], text_height))
    return result
