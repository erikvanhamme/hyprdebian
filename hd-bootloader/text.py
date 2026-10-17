"""Builds the assets of the text-only frontend of the hyprdebian bootloader (text.cpp).

update-bootloader.py adds them to assets.efi when the config has
"frontend": "text":

    .banner   banner_text, unless it is empty
    .lblN     the label of menu row N

The texts are UCS-2 in little endian without a terminating nul, as the
firmware's text console takes them. The frontend draws the selected row in
inverse video, so a row needs only one label.

The text frontend has no config keys of its own; of those of every frontend
(see update-bootloader.py) it uses banner_text. The postinst of its package
stores them from deb/txt/defaults.json.
"""

import sys

# The config keys of the text frontend, with their defaults.
DEFAULTS = {}
# Those of its keys that are file paths.
PATH_KEYS = []


def to_ucs2(text, what):
    """Returns text as UCS-2 little endian. Exits if it holds characters the text console cannot show."""
    for c in text:
        if ord(c) < 0x20 or 0x7F <= ord(c) < 0xA0 or ord(c) > 0xFFFF:
            sys.exit(f"{what} {text!r}: character U+{ord(c):04X} cannot be shown on the text console")
    return text.encode("utf-16-le")


def sections(config, labels):
    """Returns the {name: data} sections of the frontend for the menu rows with the given label texts.

    labels holds the text of each row, top to bottom; row N (from 1) gets
    .lblN.
    """
    result = {}
    if config["banner_text"]:
        result[".banner"] = to_ucs2(config["banner_text"], "banner_text")
    for row, text in enumerate(labels, start=1):
        result[f".lbl{row}"] = to_ucs2(text, "menu label")
    return result
