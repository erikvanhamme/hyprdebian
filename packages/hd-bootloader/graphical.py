"""Builds the assets of the graphical frontend of the hyprdebian bootloader (graphical.cpp).

update-bootloader.py adds them to assets.efi when the config has
"frontend": "graphical":

    .backgnd  the background, scaled and cropped to res_x x res_y, with the
              work area (the menu box of menubox.py) over it and a divider
              under the banner; the bootloader takes its resolution from this
              image
    .area     the left, top, width and height of the work area in pixels,
              each a 32-bit little-endian unsigned integer
    .banner   banner_text
    .lblN     the label of menu row N, unselected
    .lblNs    the label of menu row N, selected

It also adds video=<res_x>x<res_y> to the kernel command lines of the
UKIs, so that the kernel sets the displays to the resolution of the
bootloader: the boot splash of plymouth then has it too, and so does the
desktop until it sets a mode of its own. An argument video= in cmdline.extra
comes after it and wins.

The menu looks like hd-menu, in the theme of the system: the key hd-theme of
/etc/hyprdebian/config.json, which hd-theme --select sets as root. A member
that is missing or not valid takes the value of the theme default of
hd-desktop, which is also used without a theme. Of the theme it uses:

    background  the family of the background, in
                /usr/share/backgrounds/hyprdebian: the image that best fits
                res_x x res_y, the smallest that covers it, else the largest;
                without one, the screen is dark
    dark        the colour of the work area, at opacity
    opacity     the opacity of the work area
    highlight   the colour of the border of the work area
    border      the width of that border, in pixels at a height of 1080
    radius      the radius of the corners of the border, likewise; inside it,
                the work area and the selection have radius minus border
    light       the colour of the banner and the labels
    normal      the colour of the divider under the banner, and of the bar
                behind the selected label
    font        the font of all texts, a fontconfig name or a font file

The sizes of the theme are scaled with res_y / 1080, so that the menu looks
the same at every resolution.

The work area holds ten rows. The banner is in the top one, the menu rows
start half a row below it. Each label image is a bar as wide as the work area,
less a padding on both sides, and a row high, with the text centred in it: the
selected one has the bar in normal with rounded corners, the unselected one is
transparent. The texts are TEXT_SCALE of the height of a row. Images are in
the raw image format of png2bin.py.

Config keys of the graphical frontend, besides those of every frontend (see
update-bootloader.py), of which it uses banner_text:

    res_x, res_y    display resolution of the bootloader: the size of the
                    background, and the mode of video= on the kernel
                    command lines
    work_area_size  width and height of the work area in percent of res_x
                    and res_y; the texts scale with it
    x_center        x of the centre of the work area in percent of res_x
    y_center        y of the centre of the work area in percent of res_y

The work area, without its border, must lie within the screen.

Their defaults are in DEFAULTS below; the postinst of its package stores them
from deb/gfx/defaults.json, and the two should match. The keys that the
theme replaced, in OBSOLETE, are ignored with a warning by
update-bootloader.py, and removed by the postinst.

Needs python3-pil, and fontconfig for the font.
"""

import io
import json
import os
import re
import struct
import sys

from PIL import Image, ImageDraw

import menubox
import png2bin
import text2png

# The config keys of the graphical frontend, with their defaults.
DEFAULTS = {
    "res_x": 1920,
    "res_y": 1080,
    "work_area_size": menubox.DEFAULT_SIZE,
    "x_center": menubox.DEFAULT_X_CENTER,
    "y_center": menubox.DEFAULT_Y_CENTER,
}
# Those of its keys that are file paths.
PATH_KEYS = []
# The keys of earlier versions that the theme replaced.
OBSOLETE = ["wallpaper", "font", "color", "selected_color", "border_width"]

THEME_FILE = "/etc/hyprdebian/config.json"
THEME_KEY = "hd-theme"
BACKGROUNDS = "/usr/share/backgrounds/hyprdebian"

# The theme default of hd-desktop, for the members that the theme of the system lacks.
THEME_DEFAULTS = {
    "font": "JetBrainsMono NFM Medium",
    "highlight": "#ff6600",
    "normal": "#0066cc",
    "dark": "#071422",
    "light": "#e6edf5",
    "border": 4,
    "radius": 10,
    "opacity": 0.8,
    "background": "emerald",
}
COLOR = re.compile(r"^#[0-9a-fA-F]{6}$")

# The height at which the sizes of the theme are as they are.
THEME_HEIGHT = 1080

# Rows of text that fit in the menu box.
ROWS = 10
# The height of the texts, as a part of the height of a row.
TEXT_SCALE = 0.8
# The space between the edge of the work area and the bars of the labels, and
# the height of the divider, in pixels at THEME_HEIGHT, as in hd-menu.
PADDING = 6
DIVIDER = 1


def warn(message):
    print(f"Warning: {message}", file=sys.stderr)


def is_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def valid(key, value):
    """True when value is a valid value of the member key of a theme."""
    if key in ("font", "background"):
        return isinstance(value, str) and value != ""
    if key in ("highlight", "normal", "dark", "light"):
        return isinstance(value, str) and COLOR.match(value) is not None
    if key in ("border", "radius"):
        return isinstance(value, int) and not isinstance(value, bool) and value >= 0
    if key == "opacity":
        return is_number(value) and 0 <= value <= 1
    return False


def system_theme():
    """The theme of the system, with the defaults for what it lacks."""
    theme = {}
    if os.path.exists(THEME_FILE):
        try:
            with open(THEME_FILE, encoding="utf-8") as f:
                config = json.load(f)
        except (OSError, ValueError) as error:
            warn(f"cannot read {THEME_FILE}: {error}; using the theme default")
            config = {}
        if isinstance(config, dict) and isinstance(config.get(THEME_KEY), dict):
            theme = config[THEME_KEY]
    result = {}
    for key, default in THEME_DEFAULTS.items():
        if key in theme and valid(key, theme[key]):
            result[key] = theme[key]
        else:
            if key in theme:
                warn(f"{key} of the key {THEME_KEY} of {THEME_FILE} is not valid; using {default!r}")
            result[key] = default
    return result


def find_background(family, width, height):
    """The image of the family that best fits width x height: the smallest that covers it, else
    the largest; None when the family has no images."""
    found = []
    try:
        names = os.listdir(BACKGROUNDS)
    except OSError:
        names = []
    for name in names:
        match = re.fullmatch(re.escape(family) + r"-(\d+)x(\d+)\.png", name)
        if match:
            found.append((int(match[1]), int(match[2]), os.path.join(BACKGROUNDS, name)))
    if not found:
        return None
    covering = [f for f in found if f[0] >= width and f[1] >= height]
    if covering:
        return min(covering, key=lambda f: f[0] * f[1])[2]
    return max(found, key=lambda f: f[0] * f[1])[2]


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


def resolution(config):
    """Returns res_x and res_y of the config. Exits if they are not positive whole numbers."""
    for key in ["res_x", "res_y"]:
        value = config[key]
        if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
            sys.exit(f"{key} must be a positive whole number (pixels), not {value!r}")
    return config["res_x"], config["res_y"]


def cmdline(config):
    """Returns the arguments the frontend adds to the kernel command lines: the resolution of the bootloader."""
    res_x, res_y = resolution(config)
    return [f"video={res_x}x{res_y}"]


def background_image(theme, res_x, res_y):
    """The background of the theme, covering res_x x res_y; the colour dark without one."""
    path = find_background(theme["background"], res_x, res_y)
    if path is None:
        warn(f"there is no background {theme['background']} in {BACKGROUNDS}; the screen is dark")
        return Image.new("RGB", (res_x, res_y), theme["dark"])
    with Image.open(path) as image:
        return scale_to_cover(image.convert("RGB"), res_x, res_y)


def label(text, theme, size, text_height, radius, selected):
    """A label: a bar of size with the text centred in it, in the colour normal with rounded corners
    when selected, else transparent."""
    width, height = size
    n = menubox.SUPERSAMPLE
    bar = Image.new("RGBA", (width * n, height * n), (0, 0, 0, 0))
    if selected:
        ImageDraw.Draw(bar).rounded_rectangle(
            (0, 0, width * n - 1, height * n - 1), radius=radius * n, fill=menubox.rgba(theme["normal"])
        )
    bar = bar.resize((width, height), Image.LANCZOS)

    words = text2png.render(text, theme["light"], theme["font"], text_height)
    x = max(0, (width - words.width) // 2)
    y = (height - words.height) // 2
    bar.alpha_composite(words, (x, y))
    return bar


def sections(config, labels):
    """Returns the {name: data} sections of the frontend for the menu rows with the given label texts.

    labels holds the text of each row, top to bottom; row N (from 1) gets
    .lblN and .lblNs.
    """
    res_x, res_y = resolution(config)

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

    theme = system_theme()
    scale = res_y / THEME_HEIGHT
    border = round(theme["border"] * scale)
    radius = round(theme["radius"] * scale)
    inner_radius = max(0, radius - border)
    padding = round(PADDING * scale)
    divider = max(1, round(DIVIDER * scale))

    # The rows of the work area, as graphical.cpp places them: the banner in
    # the top one, the menu rows from half a row below it.
    left, top, box_width, box_height = geometry
    row_height = box_height // ROWS
    text_height = max(1, round(row_height * TEXT_SCALE))
    bar_size = (max(1, box_width - 2 * padding), row_height)

    # The background: the image of the theme with the menu box, and the
    # divider halfway between the banner and the first menu row.
    background = menubox.draw_box(
        background_image(theme, res_x, res_y), geometry, theme["highlight"], border, radius, theme["dark"],
        theme["opacity"],
    )
    divider_y = top + row_height + row_height // 4 - divider // 2
    ImageDraw.Draw(background).rectangle(
        (left + padding, divider_y, left + box_width - padding - 1, divider_y + divider - 1),
        fill=menubox.rgba(theme["normal"]),
    )
    background = background.convert("RGB")

    result = {
        ".backgnd": to_bin(background),
        ".area": struct.pack("<4I", *geometry),
        ".banner": to_bin(text2png.render(config["banner_text"], theme["light"], theme["font"], text_height)),
    }
    for row, text in enumerate(labels, start=1):
        result[f".lbl{row}"] = to_bin(label(text, theme, bar_size, text_height, inner_radius, False))
        result[f".lbl{row}s"] = to_bin(label(text, theme, bar_size, text_height, inner_radius, True))
    return result
