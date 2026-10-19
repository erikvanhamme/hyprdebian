## Introduction
The package `hd-theme` is the theme engine of HyprDebian. There is one theme for the whole system; there are no themes per user. The themes are JSON files in `/etc/hyprdebian/themes`, one per theme, and the file name is the name of the theme. The theme that is selected is copied, with its name, to the key `hd-theme` of `/etc/hyprdebian/config.json` of [hd-config](hd-config.md), where other packages read it, such as [hd-menu](hd-menu.md).

It has two commands:
- `hd-theme` adds, deletes, lists and selects themes, and with `--apply` writes the theme files of the desktop programs in the home directory of the user who runs it, from the selected theme. Adding, deleting, selecting and rendering the cards need root; listing and applying are for every user. Nothing applies a theme on its own: selecting a theme does not change the desktops until each user runs `hd-theme --apply`;
- `hd-theme-card` renders a theme into a card, a PNG that shows its colours, fonts, icons, geometry and how the desktop looks in it.

[hd-desktop](hd-desktop.md) depends on it. The package comes with the man pages `man 7 hd-theme`, which describes the themes, `man 1 hd-theme` and `man hd-theme-card`.

## Building the package
The package is built from `packages/hd-theme` in this repository:
```
cd packages/hd-theme
make deb
```

This builds `build/hd-theme_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `hd-config`, on `hd-art` and `hd-fonts` for the backgrounds and the font, on `python3-jinja2` for the templates, on `python3-cairo`, `python3-gi` and `gir1.2-pango-1.0` for the cards, and on `imv`, whose `imv-wayland` shows them. Install it with:
```
sudo apt install ./build/hd-theme_<version>_all.deb
```

The package comes with the themes `default` and `sand`. The installation renders the cards of all themes, and selects the theme `default` when no theme is selected yet. It does not write the theme files in the home directories; after an upgrade with changed templates, each user runs `hd-theme --apply` again.

The files involved are:
```
packages/hd-theme/Makefile
packages/hd-theme/version.json
packages/hd-theme/hd-theme
packages/hd-theme/hd-theme-card
packages/hd-theme/themes/default.json
packages/hd-theme/themes/sand.json
packages/hd-theme/templates/
packages/hd-theme/deb/control
packages/hd-theme/deb/postinst
packages/hd-theme/deb/postrm
packages/hd-theme/man/
```

## Themes
A theme is a JSON object with all of these members, and no others:
```
{
    "font": "JetBrainsMono NFM Medium",
    "font-size-big": 24,
    "font-size-normal": 18,
    "font-size-small": 14,
    "highlight": "#ff6600",
    "normal": "#0066cc",
    "dark": "#071422",
    "light": "#e6edf5",
    "border": 4,
    "radius": 10,
    "gap": 4,
    "opacity": 0.8,
    "icon_size": 48,
    "background": "emerald"
}
```

| Member | |
| --- | --- |
| `font` | The font of all text. |
| `font-size-big`, `font-size-normal`, `font-size-small` | The sizes of text, in points: banners and input fields, menus and notifications, terminals and small labels. |
| `dark` | The background of bars, menus and windows, drawn at `opacity`. |
| `normal` | The background of the selected item, unfocused borders and dividers. |
| `highlight` | Focus only: the outline of the focused window, menu or input field. Never a fill behind text. |
| `light` | All text, also on `normal`. |
| `border` | The width of borders, in pixels. |
| `radius` | The outer radius of corners, in pixels, at least `border`. Anything inside a border has the inner radius, `radius` minus `border`. |
| `gap` | The distance between windows, in pixels. |
| `opacity` | The opacity of `dark`, from 0 to 1. Text and borders stay opaque. |
| `icon_size` | The size of icons, in pixels. |
| `background` | A family of images in `/usr/share/backgrounds/hyprdebian` of [hd-art](hd-art.md), named `<family>-<width>x<height>.png`. A screen shows the smallest one that covers it, else the largest. |

In `config.json`, the key `hd-theme` also has the member `name`, the name of the theme.

## hd-theme
```
hd-theme --list                 print the themes and show their cards in imv-wayland
hd-theme --add <file>           add the theme in <file>, or replace the one with its name
hd-theme --delete <name>        delete a theme; not the selected one
hd-theme --select <name>        make <name> the theme of the system
hd-theme --apply [--no-reload]  apply the selected theme to the desktop of the user
hd-theme --cards                render the cards of all themes again
```

`--add` takes the name of the theme from the file name without `.json`: lower case letters, digits, `-` and `_`. It checks the theme and renders its card before it adds it; when either fails, nothing is added. When it replaces the selected theme, it selects it again.

`--select` writes the theme to `config.json` with `hd-config-update`. Programs that read `config.json` themselves, such as hd-menu, use the new theme at once; the desktop programs only after the user runs `hd-theme --apply`.

`--add`, `--delete`, `--select` and `--cards` are refused when not run as root. `--list` and `--apply` are for every user.

`--list` prints the themes, with `*` before the selected one, and opens `imv-wayland -n <card> /var/cache/hd-theme/cards`, where the cards are, starting at the card of the selected theme.

To make a theme of your own:
```
cp /etc/hyprdebian/themes/default.json meadow.json
editor meadow.json
sudo hd-theme --add meadow.json
hd-theme --list
sudo hd-theme --select meadow
hd-theme --apply
```

## Applying a theme
`hd-theme --apply` writes one theme file per program, next to its config, from the jinja2 templates in `/usr/share/hd-theme/templates`:

| Theme file | Used by |
| --- | --- |
| `~/.config/kitty/hd-theme.conf` | `include hd-theme.conf` in `kitty.conf` |
| `~/.config/hypr/hd-theme.lua` | `pcall(require, "hd-theme")` in `hyprland.lua` |
| `~/.config/hypr/hd-theme-hyprlock.conf` | `source` in `hyprlock.conf`, which uses its variables `$hd_font`, `$hd_highlight`, `$hd_normal`, `$hd_light`, `$hd_dark`, `$hd_border`, `$hd_radius` and `$hd_background` |
| `~/.config/hypr/hd-theme-hyprpaper.conf` | `source` in `hyprpaper.conf`, which uses its variable `$hd_background`, the image of the background that fits the monitors of Hyprland |
| `~/.config/mako/hd-theme` | `include=~/.config/mako/hd-theme` in the config of mako |

The configs themselves belong to the user and are never changed; the ones that [hd-desktop](hd-desktop.md) puts in `/etc/skel` use the theme files. A config that does not use its theme file is reported. Settings after the line that uses the theme file take the place of the theme.

A theme file is only written when it changes, and only then its program is reloaded: Hyprland with `hyprctl reload`, hyprpaper by a restart of `hyprpaper.service` of the user, kitty with SIGUSR1 and mako with `makoctl reload`. hyprlock reads its config when it starts. `--no-reload` writes the theme files and reloads nothing.

A new user has no theme files until they run `hd-theme --apply`. Until then, Hyprland and kitty use their own defaults for what the theme sets, and hyprlock and hyprpaper miss the variables of the theme.

## hd-theme-card
```
hd-theme-card <theme.json> <card.png>
```

The theme file is a theme, or a configuration with the key `hd-theme`, such as `/etc/hyprdebian/config.json`. The name on the card is the member `name`, else the name of the file without `.json`. The background is the image of its family that fits a screen of 1920x1080.
