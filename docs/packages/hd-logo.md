## Introduction
`hd-logo` is the moving logo of the old DVD players, a screensaver of the package [hd-screensavers](hd-screensavers.md): the Debian swirl with "HyprDebian" over it, bouncing on black and changing colour at each bounce. It is made for a laptop with a slow CPU and a worn battery: one textured quad per monitor, 30 frames a second, on the GPU that drives the panel. It is a C++ program that draws with Vulkan on a wlr-layer-shell surface on every monitor, so it needs Hyprland or another compositor with wlr-layer-shell.

The package makes hd-pipes the screensaver of a system that has none yet; to take hd-logo instead, set the key `hd-screensaver` of the configuration, see [hd-screensaver](hd-screensaver.md#the-key):
```
sudo hd-config-update hd-screensaver '"hd-logo"'
```

`hd-screensavers` comes with the man page `man 6 hd-logo`.

## Building the package
hd-logo is part of the package `hd-screensavers`, built from `packages/hd-screensavers`; see [hd-screensavers](hd-screensavers.md#building-the-package). Its sources are in `packages/hd-screensavers/logo`, the code it shares with the other screensavers in `packages/hd-screensavers/common`.

The logo is `logo/hd-logo.svg`, all paths, so no font is needed to build or run it. The build renders it with `rsvg-convert`, of `librsvg2-bin` in the build image, 512 pixels high, and `logo/mask.py` turns its alpha channel into the mask, one channel with all its mipmaps, in a C++ header that the binary holds. hd-logo has no code to load images, and no dependencies beyond those of hd-pipes.

To try a change without a package, `make all` in `logo/` builds `build/hd-logo` there, in a container of `hd-build:unstable`; the binary runs on the computer itself, a HyprDebian system on unstable, with `libvulkan1` and a Vulkan driver:
```
cd packages/hd-screensavers/logo
make all
build/hd-logo --window
```

The files involved are:
```
packages/hd-screensavers/logo/Makefile
packages/hd-screensavers/logo/hd-logo.svg
packages/hd-screensavers/logo/mask.py
packages/hd-screensavers/logo/src/main.cpp
packages/hd-screensavers/logo/src/logo_options.cpp
packages/hd-screensavers/logo/src/theme_colours.cpp
packages/hd-screensavers/logo/src/bounce.cpp
packages/hd-screensavers/logo/src/logo.cpp
packages/hd-screensavers/logo/src/logo.vert
packages/hd-screensavers/logo/src/logo.frag
packages/hd-screensavers/common/
packages/hd-screensavers/tests/test_bounce.cpp
packages/hd-screensavers/tests/test_options.cpp
packages/hd-screensavers/man/hd-logo.6
packages/hd-screensavers/deb/copyright
```

| File | |
| --- | --- |
| `hd-logo.svg` | The logo: "HyprDebian" in JetBrains Mono ExtraBold, as paths, over the swirl of the Debian Open Use Logo. |
| `mask.py` | Turns the PNG of `rsvg-convert` into the header of the mask, with its mipmaps. |
| `main.cpp` | Reads the options and the colours and runs the screen. |
| `logo_options.cpp` | The option `framerate` and `--window`. |
| `theme_colours.cpp` | The colours from the key `hd-theme`. |
| `bounce.cpp` | The size and step per monitor, the start, the ticks on the clock and the bounces. No Wayland or Vulkan, tested by `test_bounce.cpp`. |
| `logo.cpp` | The mask on the device and a scene per monitor that draws the quad. |
| `logo.vert`, `logo.frag` | The shaders: the quad, and the mask tinted with the colour. |

The swirl is the Debian Open Use Logo, © 1999 Software in the Public Interest, Inc., under the GNU LGPL 3 or later or, at your option, CC BY-SA 3.0 Unported; `hd-logo.svg` and `deb/copyright` carry that notice.

## The look
- A background of pure black, #000000.
- One quad per monitor, with the mask filtered linearly between its mipmaps, tinted with one colour, without multisampling.
- The colours are `normal`, `highlight` and `light` of the theme, in that order, from the key `hd-theme`, as hd-menu and hd-message read it: the configuration of the user first, then that of the system. A colour that is missing, or not `#rrggbb`, has its value of the theme `default`: #0066cc, #ff6600 and #e6edf5. They are read once, at the start. `dark` is left out: on black it hardly shows.

The device is the GPU that drives the compositor, as for every screensaver of the package, see [hd-screensavers](hd-screensavers.md#the-gpu); on a laptop with an HD Graphics 4000 for the panel, that is the HD Graphics 4000, on Vulkan 1.2.

## The motion
Every monitor has a logo of its own, with its own size, step, start and colour.

- The logo moves at exactly 45°, on a grid: one step of `s` pixels on both axes per tick. `Rx` and `Ry` are the distances it can travel: the width of the monitor less that of the logo, and its height less that of the logo. `s` divides both, so the logo lands on the edges exactly.
- The ticks follow the clock at `framerate` a second, or with 0 at the refresh rate of the current mode of the monitor; a frame is drawn at each tick, and a late frame skips to the tick of its time, so the logo does not slow down.
- **The size and the step** are found per monitor at the start: the height nearest to 20% of the height of the monitor, the width from the aspect of the SVG, and the step nearest to 10 heights of the monitor per minute divided by the ticks per second, among the pairs where `s` divides `Rx` and `Ry`. The search takes heights of 15–25% of the monitor and steps within ±25% of the target; when no pair fits, it widens both by as much again, 10–30% and ±50%, and so on, until one does. A window too small for a logo that moves gets one that stands still.
- **The start**: a random place on the grid, in a random one of the four diagonal directions, on a path that passes a corner, and a random one of the three colours. In steps, the path passes a corner when `x0 − y0` is a multiple of `gcd(Rx, Ry)`, on the diagonal from the top left to the bottom right, and `x0 + y0` on the other. When the first corner comes is left to chance.
- **A bounce** at an edge moves on to the next colour. A corner hit, the tick on which the logo reaches a vertical and a horizontal edge both, is one bounce.

In a window the logo starts again when the window is resized.

What it gives at 30 frames a second, with the aspect of the logo, 1.31:

| Monitor | Logo | Step | A corner every |
| --- | --- | --- | --- |
| 1920×1080, 2560×1440, 3840×2160 | 276×210, 368×280, 552×420 px | 6, 8, 12 px | ~22 min |
| 1920×1200 | 315×240 px | 5 px | ~11 min |
| 3440×1440 | 386×294 px | 6 px | ~54 min |
| 2560×1080 | 292×222 px | 6 px | ~30 min |
| 1366×768 | 201×153 px | 5 px | ~16 min |

## Input
On the monitors, hd-logo has the keyboard to itself and ends on a key press, a move of the mouse, a click, a scroll or a touch. The pointer that is on a monitor when it starts does not end it; the cursor is hidden on its surfaces. It also ends on SIGTERM, which `hd-kill-screensaver` sends, and on SIGINT.

With `--window` it is one ordinary window instead, with the app id `hd-logo`, which input does not end; Escape or closing the window does.

## Options
| Option | Default | |
| --- | --- | --- |
| `--framerate N` | 30 | The ticks per second on each monitor, 1–1000; 0 for a tick at every refresh of each monitor, from its current mode. It sets the step too, see [The motion](#the-motion). |
| `--window` | | One window instead of every monitor; see [Input](#input). Only on the command line. |
| `--help` | | Prints the options, with their ranges and defaults. |

Each option is `--name value` or `--name=value`; when an option is given more than once, the last wins. The size, the speed and the colours are not options.

## The configuration
The key `hd-logo` of the configuration has the one member `framerate`:
```
{"hd-logo": {"framerate": 60}}
```

It is read in the configuration of the system, `/etc/hyprdebian/config.json`, and then in that of the user, `~/.config/hyprdebian/config.json`. Run as root, it only reads the configuration of the system. The command line wins over both. See [hd-config](hd-config.md). For a user:
```
hd-config-update hd-logo '{"framerate": 60}'
```

In a configuration, a value out of its range or of the wrong type, a member that is not an option, a key that is not an object and a file that is not valid JSON are reported on stderr and skipped, so the value below stays. On the command line a wrong value is an error.

## Exit status
| Status | |
| --- | --- |
| 0 | It ended: on input, on SIGTERM or SIGINT, or after `--help`. |
| 1 | It cannot start, with the reason on stderr: no Vulkan device, no wlr-layer-shell, no monitors; or it lost the compositor. |
| 2 | A wrong argument. |

`hd-launch-screensaver` starts the text screensaver in its place when it ends with an error within 2 seconds.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/hd-logo` | The screensaver. |
| `~/.config/hyprdebian/config.json` | The configuration of the user, with the keys `hd-logo` and `hd-theme`. |
| `/etc/hyprdebian/config.json` | The configuration of the system, with the keys `hd-logo`, `hd-theme` and `hd-screensaver`. |
| `/usr/share/doc/hd-screensavers/copyright` | The notice of the Debian Open Use Logo. |
