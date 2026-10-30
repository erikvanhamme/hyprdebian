"""The scale of the theme on the monitor of this machine, shared by hd-theme and hd-launcher-cache.

The package hd-desktop installs this as /usr/lib/hd-desktop/hd_display.py.

The sizes of a theme are made for a screen of REFERENCE_DPI pixels per inch, seen from
REFERENCE_DISTANCE centimetres. The key hd-display of /etc/hyprdebian/config.json tells how the
monitor of this machine differs from that, and so by what factor the sizes are multiplied, so a
theme looks the same size on every machine:

    {"diagonal": 42, "distance": 90}    the diagonal of the monitor in inches and the distance
                                        from which it is seen in centimetres
    {"factor": 1.4}                     the factor itself

A factor wins over a diagonal and a distance. The resolution is that of the preferred mode of
the connected monitor with the most pixels, from /sys/class/drm. Without the key, the factor is
1. hd-menu and hd-message work it out in the same way, in common/src/theme.cpp.
"""

import json
import math
import os
import re
import sys

CONFIG_FILE = "/etc/hyprdebian/config.json"
CONFIG_KEY = "hd-display"
DRM = "/sys/class/drm"
REFERENCE_DPI = 96
REFERENCE_DISTANCE = 70


def warn(message):
    print(f"Warning: {message}", file=sys.stderr)


def is_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def round_half_up(value, digits=0):
    """value rounded to digits decimals, halves up, as std::round of hd-menu does."""
    step = 10 ** digits
    return math.floor(value * step + 0.5) / step


def read_text(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read()
    except OSError:
        return None


def settings():
    """The key hd-display of the configuration of the system, or None."""
    text = read_text(CONFIG_FILE)
    if text is None:
        return None
    try:
        config = json.loads(text)
    except ValueError as e:
        warn(f"Cannot read {CONFIG_FILE}: {e}; the theme is not scaled.")
        return None
    if not isinstance(config, dict):
        return None
    return config.get(CONFIG_KEY)


def monitor_size():
    """The width and height in pixels of the preferred mode of the connected monitor with the most
    pixels, or None when there is none."""
    try:
        connectors = os.listdir(DRM)
    except OSError:
        return None
    best = None
    for connector in connectors:
        status = read_text(os.path.join(DRM, connector, "status"))
        if status is None or status.strip() != "connected":
            continue
        # The preferred mode comes first.
        modes = read_text(os.path.join(DRM, connector, "modes"))
        match = re.match(r"(\d+)x(\d+)", modes or "")
        if match is None:
            continue
        size = (int(match[1]), int(match[2]))
        if best is None or size[0] * size[1] > best[0] * best[1]:
            best = size
    return best


def factor():
    """The factor by which the sizes of the theme are multiplied, to two decimals; 1 without the
    key hd-display, or with a warning when it is not valid."""
    display = settings()
    if display is None:
        return 1
    where = f"The key {CONFIG_KEY} of {CONFIG_FILE}"
    if not isinstance(display, dict):
        warn(f"{where} is not a JSON object; the theme is not scaled.")
        return 1
    if "factor" in display:
        if not is_number(display["factor"]) or display["factor"] <= 0:
            warn(f"{where} has a factor that is not a number above 0; the theme is not scaled.")
            return 1
        return round_half_up(display["factor"], 2)
    diagonal = display.get("diagonal")
    distance = display.get("distance")
    if not (is_number(diagonal) and diagonal > 0 and is_number(distance) and distance > 0):
        warn(f"{where} has no factor, nor a diagonal and a distance above 0; the theme is not "
             "scaled.")
        return 1
    size = monitor_size()
    if size is None:
        warn(f"There is no connected monitor in {DRM}; the theme is not scaled.")
        return 1
    dpi = math.hypot(*size) / diagonal
    return round_half_up(dpi / REFERENCE_DPI * distance / REFERENCE_DISTANCE, 2)


def font_size(points, scale):
    """A font size in points, multiplied by scale, to a tenth of a point; whole sizes as int."""
    size = round_half_up(points * scale, 1)
    if size == int(size):
        return int(size)
    return size


def pixels(size, scale):
    """A size in whole pixels, multiplied by scale; at least 1 when it is not 0."""
    if size == 0:
        return 0
    return max(int(round_half_up(size * scale)), 1)
