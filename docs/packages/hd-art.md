## Introduction
The package `hd-art` holds the wallpapers of hyprdebian: the Emerald wallpaper of Debian 12, rendered to PNG at four sizes. It only installs the wallpapers; it has no commands and no settings.

The wallpapers are in `/usr/share/backgrounds/hyprdebian`:
```
emerald-3840x2160.png    4K UHD, 16:9
emerald-3440x1440.png    ultrawide, 21:9
emerald-1920x1080.png    full HD, 16:9
emerald-1024x768.png     4:3
```

The 3440x1440 wallpaper is rendered from the middle band of the 3840x2160 SVG.

The package comes with a man page: `man hd-art`.

## Building the package
The package is built from `packages/hd-art` in this repository:
```
cd packages/hd-art
make deb
```

This builds `build/hd-art_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. Install it with:
```
sudo apt install ./build/hd-art_<version>_all.deb
```

The files involved are:
```
packages/hd-art/Makefile
packages/hd-art/version.json
packages/hd-art/deb/control
packages/hd-art/deb/copyright
packages/hd-art/man/hd-art.7
packages/hd-art/wallpapers/emerald-*.png
```

## Using the wallpapers
Pick the wallpaper that matches the resolution of the monitor, or the next larger one. To show it on the desktop with hyprpaper, name it in `~/.config/hypr/hyprpaper.conf`:
```
path = /usr/share/backgrounds/hyprdebian/emerald-1920x1080.png
```

## Copyright
The wallpapers are PNG renders of the Emerald theme SVGs from the Debian package `desktop-base`, by Juliette Taka Belin, under the GPL, version 2 or later. See `/usr/share/doc/hd-art/copyright`.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/share/backgrounds/hyprdebian/emerald-*.png` | The wallpapers. |
| `/usr/share/doc/hd-art/copyright` | The copyright and license of the wallpapers. |
