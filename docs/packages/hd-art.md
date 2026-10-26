## Introduction
The package `hd-desktop` holds the wallpapers of hyprdebian, each rendered to PNG at four sizes:

| Family | Wallpaper |
| --- | --- |
| `emerald` | The Emerald wallpaper of Debian 12, in its own colours; the theme `default` uses it. |
| `softwaves-mulberry` | The Soft Waves wallpaper of Debian 9, in the colours of the theme `mulberry`. |
| `lines-sand` | The Lines wallpaper of Debian 8, in the colours of the theme `sand`. |

They are only the wallpapers; they have no commands and no settings.

The wallpapers are in `/usr/share/backgrounds/hyprdebian`:
```
<family>-3840x2160.png    4K UHD, 16:9
<family>-3440x1440.png    ultrawide, 21:9
<family>-1920x1080.png    full HD, 16:9
<family>-1024x768.png     4:3
```

The 3440x1440 wallpapers are rendered from the middle band of the 3840x2160 SVG. The 1024x768 Lines wallpaper is rendered from its 1600x1200 SVG. Soft Waves and Lines are recoloured with a gradient from the colour `dark` of the theme through `normal` to `light`, by the brightness of the original.

`hd-desktop` comes with a man page for them: `man hd-art`.

## Building the package
The wallpapers are part of the package `hd-desktop`, built from `packages/hd-desktop`; see [hd-desktop](hd-desktop.md). The files involved are:
```
packages/hd-desktop/wallpapers/*.png
packages/hd-desktop/man/hd-art.7
packages/hd-desktop/deb/copyright
```

## Using the wallpapers
Pick the wallpaper that matches the resolution of the monitor, or the next larger one. To show it on the desktop with hyprpaper, name it in `~/.config/hypr/hyprpaper.conf`:
```
path = /usr/share/backgrounds/hyprdebian/emerald-1920x1080.png
```

## Copyright
The wallpapers are PNG renders of the Emerald, Soft Waves and Lines theme SVGs from the Debian package `desktop-base`, by Juliette Taka Belin, under the GPL, version 2 or later. See `/usr/share/doc/hd-desktop/copyright`.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/share/backgrounds/hyprdebian/*.png` | The wallpapers. |
| `/usr/share/doc/hd-desktop/copyright` | The copyright and license of the wallpapers. |
