## Introduction
`hd-theme` is the theme engine of HyprDebian, in the package `hd-desktop`. The themes are JSON files in `/etc/hyprdebian/themes`, one per theme, and the file name is the name of the theme. A theme that is selected is copied, with its name, to the key `hd-theme` of a configuration of [hd-config](hd-config.md). There are two:
- the theme of the system, in `/etc/hyprdebian/config.json`, selected by root. It is the theme of the system itself, of the boot splash of [hd-plymouth](hd-plymouth.md), of the graphical bootloader of [hd-bootloader](hd-bootloader.md#the-look), of new users and of every user who has no theme of their own;
- the theme of a user, in `~/.config/hyprdebian/config.json`, selected by that user.

Programs of the desktop, such as [hd-menu](hd-menu.md) and [hd-message](hd-message.md), read the theme of the user who runs them: the key `hd-theme` of `~/.config/hyprdebian/config.json` when it is there, else that of `/etc/hyprdebian/config.json`, else defaults of their own. Root uses the theme of the system.

It has two commands:
- `hd-theme` adds, deletes, lists and selects themes, and with `--apply` writes the theme files of the desktop programs in the home directory of the user who runs it, from the selected theme. Adding, deleting and rendering the cards need root; listing, selecting and applying are for every user, and work on the theme of the system for root and on the theme of the user for other users. Nothing applies a theme on its own: selecting a theme does not change the desktops until each user runs `hd-theme --apply`;
- `hd-theme-card` renders a theme into a card, a PNG that shows its colours, fonts, icons, geometry and how the desktop looks in it.

[hd-desktop](hd-desktop.md) comes with the man pages `man 7 hd-theme`, which describes the themes, `man 1 hd-theme` and `man hd-theme-card`.

## Building the package
The theme engine is part of the package `hd-desktop`, built from `packages/hd-desktop`; see [hd-desktop](hd-desktop.md). It needs the wallpapers and the font of `hd-desktop`, `python3-jinja2` for the templates, `python3-cairo`, `python3-gi` and `gir1.2-pango-1.0` for the cards, and `imv`, whose `imv-wayland` shows them.

`hd-desktop` comes with the themes `default`, on the wallpaper `emerald`, `sand`, in browns on `lines-sand`, and `mulberry`, in reds and purples on `softwaves-mulberry` and in `SauceCodePro NFM Medium`. The installation renders the cards of all themes, and selects the theme `default` when no theme is selected yet. It does not write the theme files in the home directories; after an upgrade with changed templates, each user runs `hd-theme --apply` again.

The files involved are:
```
packages/hd-desktop/hd-theme
packages/hd-desktop/hd-theme-card
packages/hd-desktop/themes/default.json
packages/hd-desktop/themes/sand.json
packages/hd-desktop/themes/mulberry.json
packages/hd-desktop/templates/hd-theme/
packages/hd-desktop/deb/postinst
packages/hd-desktop/deb/postrm
packages/hd-desktop/man/hd-theme.7
packages/hd-desktop/man/hd-theme.1
packages/hd-desktop/man/hd-theme-card.1
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
hd-theme --add <file> [--no-initramfs]
                                add the theme in <file>, or replace the one with its name
hd-theme --delete <name>        delete a theme; not the theme of the system
hd-theme --select <name> [--no-initramfs]
                                make <name> the theme of the system, or of the user
hd-theme --apply [--no-reload]  apply the selected theme to the desktop of the user
hd-theme --cards                render the cards of all themes again
```

`--add` takes the name of the theme from the file name without `.json`: lower case letters, digits, `-` and `_`. It checks the theme and renders its card before it adds it; when either fails, nothing is added. When it replaces the theme of the system, it selects it again for the system; users who selected it themselves keep the old copy until they select it again. `--delete` refuses the theme of the system; users who selected it themselves keep their copy.

`--select` writes the theme to a configuration with `hd-config-update`, which picks it by who runs it: run as root, the theme becomes the theme of the system in `/etc/hyprdebian/config.json`; run as any other user, the theme of that user in `~/.config/hyprdebian/config.json`. Programs that read the theme themselves, such as hd-menu and hd-message, use the new theme at once; the desktop programs only after the user runs `hd-theme --apply`. A user goes back to the theme of the system with `hd-config-remove hd-theme`.

Selected as root, the theme is also rendered into the boot splash with `hd-plymouth-render --update-initramfs`, which rebuilds the initramfs of all kernels when the splash changed; that takes a while. Then, when hd-bootloader is installed, it writes the bootloader to the EFI system partitions again with `/usr/lib/hd-bootloader/update-esps`, so the graphical bootloader has the theme. `--no-initramfs` renders the splash without the rebuild and the bootloader; the next rebuild, such as for a new kernel, takes both. See [hd-plymouth](hd-plymouth.md#rendering-the-splash) and [hd-bootloader](hd-bootloader.md#the-look).

`--apply` applies the theme of the user who runs it, or the theme of the system while they have none; root applies the theme of the system.

`--add`, `--delete` and `--cards` are refused when not run as root. `--list`, `--select` and `--apply` are for every user.

`--list` prints the themes, with `*` before the selected one; for users other than root, it marks the theme of the system as well. It opens `imv-wayland -n <card> /var/cache/hd-theme/cards`, where the cards are, starting at the card of the selected theme.

To make a theme of your own:
```
cp /etc/hyprdebian/themes/default.json meadow.json
editor meadow.json
sudo hd-theme --add meadow.json
hd-theme --list
sudo hd-theme --select meadow
hd-theme --apply
```

To use a theme for yourself only, and to go back to the theme of the system:
```
hd-theme --select sand
hd-theme --apply
hd-config-remove hd-theme
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
