## Introduction
`hd-menu` is the keyboard-driven menu of the HyprDebian desktop, in the package `hd-desktop`. It reads a menu as JSON from stdin, shows it in the middle of the screen in the hd-theme of the user, and prints the value of the option chosen on stdout. It is a C++ program that draws the menu itself, with cairo and librsvg, on a wlr-layer-shell surface, so it needs Hyprland or another compositor with wlr-layer-shell.

[hd-desktop](hd-desktop.md) uses it for its menus: whether to reboot or power off (`hd-ask-reboot`, `hd-ask-poweroff`), the mode of a monitor (`hd-mode-select`), and the drive to mount or unmount (`hd-mount`, `hd-unmount`). The application launcher of `SUPER + SPACE` shows the menu that `hd-launcher` writes in it.

Only one `hd-menu` is open at a time: a lock in `/run/user/<uid>/hd-menu.lock` makes a second one exit at once with 1.

`hd-desktop` comes with the man page `man 1 hd-menu`.

## Building the package
hd-menu is part of the package `hd-desktop`, built from `packages/hd-desktop`; see [hd-desktop](hd-desktop.md). Its source is in `packages/hd-desktop/menu`. `make deb` of hd-desktop compiles it in a `debian:unstable` container, with `make all` of `menu/Makefile`, and puts the libraries the binary links to, as `dpkg-shlibdeps` finds them, in the Depends of `hd-desktop`, which is therefore built for the architecture of the build computer.

To try a change without a package, `make all` in `menu/` builds `build/hd-menu` there, on the computer itself, which then needs the -dev packages:
```
sudo apt install build-essential pkgconf libwayland-dev wayland-protocols libcairo2-dev librsvg2-dev
cd packages/hd-desktop/menu
make all
echo '{"banner": "Reboot system?", "options": [{"label": "No"}, {"label": "Yes"}]}' | build/hd-menu
```

The files involved are:
```
packages/hd-desktop/menu/Makefile
packages/hd-desktop/menu/build-hd-menu.sh
packages/hd-desktop/menu/protocols/wlr-layer-shell-unstable-v1.xml
packages/hd-desktop/menu/src/
packages/hd-desktop/man/hd-menu.1
packages/hd-desktop/deb/copyright
```

wlr-layer-shell is not in a Debian package, so `protocols/` has a copy of it from wlr-protocols.

## The menu
```
{
    "banner": "Reboot system?",
    "width": 500,
    "options": [
        {"label": "  No", "value": "No", "default": true},
        {"label": "  Yes", "value": "Yes", "icon": "/usr/share/icons/reboot.svg"}
    ]
}
```

| Member | |
| --- | --- |
| `banner` | Optional: a line of text at the top. |
| `width`, `height` | Optional: the size of the menu, in logical pixels (a number) or as a percentage of the output (a string such as `"50%"`). Without them, the menu is just large enough for the banner and the options. It is never larger than the output; options that do not fit scroll. |
| `options` | The options, in this order; at least one. |
| `label` | The text of an option. Glyphs of the Nerd Font can be written as escapes, such as `` (a check mark) and `` (a cross). |
| `value` | Optional: what `hd-menu` prints when the option is chosen; the label when there is none. |
| `icon` | Optional: the path of a PNG or SVG (`.svg` or `.svgz`) image, shown left of the label at the `icon_size` of the theme. |
| `default` | Optional: `true` for the option selected at the start; the first option when none has it. |

The arrows, Tab, Page Up, Page Down, Home and End move the selection, Enter chooses and Escape or `q` closes the menu without a choice. `q` is the key with the letter q in the keyboard layout, with or without Shift. A key that is held repeats at the rate and after the delay of the compositor, `input:repeat_rate` and `input:repeat_delay` in Hyprland, so that holding Down keeps moving the selection.

## The theme
`hd-menu` takes its look from the `hd-theme` key of the configuration of the user, `~/.config/hyprdebian/config.json`, and when that has none, of the configuration of the system, `/etc/hyprdebian/config.json`, of [hd-config](hd-config.md); [hd-theme](hd-theme.md) sets them when a theme is selected. When neither has the key, it uses defaults of its own. Run as root, it only reads the configuration of the system:

| Setting | Used for |
| --- | --- |
| `font`, `font-size-big` | All text. |
| `dark`, `opacity` | The background of the menu; the opacity applies to the background only. |
| `light` | The text. |
| `normal` | The background of the selected option and the divider under the banner. |
| `highlight` | The border of the menu. |
| `border`, `radius` | The width and the corner radius of the border. What is inside it has the radius minus the border. |
| `icon_size` | The size of the icons. |

The font size is in points, like in the other programs of the theme; `hd-menu` draws it at 4/3 logical pixels per point. The border, the radius and the icon size are in logical pixels. The scale of the monitor decides how large that is on the screen, so the menu has the same size as the other programs on every monitor.

A setting that is missing has its default; when the file cannot be read, the whole theme has.

## Exit status
| Status | |
| --- | --- |
| 0 | An option was chosen; its value is on stdout. |
| 1 | The menu was closed without a choice, or another `hd-menu` is open. |
| 2 | An error: arguments, invalid JSON, a menu without options, or no Wayland compositor with wlr-layer-shell. |

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/hd-menu` | The menu. |
| `~/.config/hyprdebian/config.json` | The theme of the user, in `hd-theme`. |
| `/etc/hyprdebian/config.json` | The theme of the system, in `hd-theme`, when the user has none. |
| `/run/user/<uid>/hd-menu.lock` | The lock that keeps a second `hd-menu` from opening. |
