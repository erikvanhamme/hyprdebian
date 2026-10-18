## Introduction
The package `hd-menu` is a menu in the manner of wofi for the HyprDebian desktop. It reads a menu as JSON from stdin, shows it in the middle of the screen in the hd-theme of `/etc/hyprdebian/config.json`, and prints the value of the option chosen on stdout. It is a C++ program that draws the menu itself, with cairo and librsvg, on a wlr-layer-shell surface, so it needs Hyprland or another compositor with wlr-layer-shell.

[hd-desktop](hd-desktop.md) depends on it, and uses it for its menus: whether to reboot or power off (`hd-ask-reboot`, `hd-ask-poweroff`), the mode of a monitor (`hd-mode-select`), and the drive to mount or unmount (`hd-mount`, `hd-unmount`). The app launcher stays wofi.

Only one `hd-menu` is open at a time: a lock in `/run/user/<uid>/hd-menu.lock` makes a second one exit at once with 1.

The package comes with the man page `man 1 hd-menu`.

## Building the package
The package is built from `packages/hd-menu` in this repository:
```
cd packages/hd-menu
make deb
```

This compiles hd-menu in a `debian:unstable` container and builds `build/hd-menu_<version>_<arch>.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. Its Depends are the libraries the binary links to, as `dpkg-shlibdeps` finds them; it recommends `hd-fonts`, for the font of the theme. Install it with:
```
sudo apt install ./build/hd-menu_<version>_<arch>.deb
```

To try a change without a package, `make all` builds `build/hd-menu` on the computer itself, which then needs the -dev packages:
```
sudo apt install build-essential pkgconf libwayland-dev wayland-protocols libcairo2-dev librsvg2-dev
make all
echo '{"banner": "Reboot system?", "options": [{"label": "No"}, {"label": "Yes"}]}' | build/hd-menu
```

The files involved are:
```
packages/hd-menu/Makefile
packages/hd-menu/version.json
packages/hd-menu/build-hd-menu.sh
packages/hd-menu/deb/control
packages/hd-menu/deb/copyright
packages/hd-menu/man/hd-menu.1
packages/hd-menu/protocols/wlr-layer-shell-unstable-v1.xml
packages/hd-menu/src/
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

The arrows, Tab, Page Up, Page Down, Home and End move the selection, Enter chooses and Escape closes the menu without a choice.

## The theme
`hd-menu` takes its look from the `hd-theme` key of `/etc/hyprdebian/config.json`, which [hd-config](hd-config.md) sets:

| Setting | Used for |
| --- | --- |
| `font`, `font-size-big` | All text. |
| `dark`, `opacity` | The background of the menu; the opacity applies to the background only. |
| `light` | The text. |
| `normal` | The background of the selected option and the divider under the banner. |
| `highlight` | The border of the menu. |
| `border`, `radius` | The width and the corner radius of the border. What is inside it has the radius minus the border. |
| `icon_size` | The size of the icons. |

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
| `/etc/hyprdebian/config.json` | The theme, in `hd-theme`. |
| `/run/user/<uid>/hd-menu.lock` | The lock that keeps a second `hd-menu` from opening. |
