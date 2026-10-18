## Introduction
The package `hd-plymouth` is the boot splash of HyprDebian: the plymouth theme `hyprdebian`. [hd-desktop](hd-desktop.md) recommends it, so it is installed with the desktop unless recommends are left out.

The splash looks like the lock screen of hyprlock, as `~/.config/hypr/hyprlock.conf` of `hd-desktop` sets it up: the Emerald wallpaper of [hd-art](hd-art.md), scaled to cover the screen, and, when the boot asks for a password, an input field in the middle with a black fill of 80% opacity and an orange outline, in JetBrains Mono of [hd-fonts](hd-fonts.md).

The theme is based on the theme `emerald` of the Debian package `desktop-base`. `hd-plymouth` conflicts with `desktop-base`, whose themes it replaces.

The package comes with the man page `man 7 hd-plymouth`.

## Building the package
The package is built from `packages/hd-plymouth` in this repository:
```
cd packages/hd-plymouth
make deb
```

This builds `build/hd-plymouth_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `plymouth`, `plymouth-label`, `initramfs-tools`, `fontconfig`, `python3`, `hd-art` and `hd-fonts`. Install it with:
```
sudo apt install ./build/hd-plymouth_<version>_all.deb
```

The images of the input field, a dot, a black pixel for the fill and a pixel of each outline colour, are rendered with ImageMagick and kept in the repository, so `make deb` does not need ImageMagick. To render them again:
```
make images
```

The files involved are:
```
packages/hd-plymouth/Makefile
packages/hd-plymouth/version.json
packages/hd-plymouth/deb/control
packages/hd-plymouth/deb/copyright
packages/hd-plymouth/deb/triggers
packages/hd-plymouth/deb/postinst
packages/hd-plymouth/deb/postrm
packages/hd-plymouth/theme/hyprdebian.plymouth
packages/hd-plymouth/theme/hyprdebian.script
packages/hd-plymouth/theme/dot.png, fill.png, outline.png, outline-check.png, outline-fail.png
packages/hd-plymouth/plymouthd.conf
packages/hd-plymouth/initramfs/hd-plymouth
packages/hd-plymouth/man/hd-plymouth.7
```

## Passwords
The ZFS script of the initramfs asks for the password of an encrypted dataset with plymouth when the splash shows, up to three times. The field does not show the prompt of the question.

| State | Outline | In the field |
| --- | --- | --- |
| Empty | orange, `outer_color` of hyprlock.conf | `Enter Password` |
| Typing | orange | a blue dot per key press, `font_color` of hyprlock.conf |
| Checking, after Enter | yellow, the default `check_color` of hyprlock | the dots; the field hides after about two seconds |
| Wrong password | red, the default `fail_color` of hyprlock | `Wrong password (<attempts>)`, until the next key press |

The fill is black at 80% opacity in every state, like `inner_color` of hyprlock.conf. The splash knows a password was wrong when the same prompt comes back after the question ended.

## Setting the theme
plymouth reads its theme from `/etc/plymouth/plymouthd.conf`. The package diverts the one of plymouth to `/etc/plymouth/plymouthd.conf.plymouth` and copies its own in its place, on every installation and upgrade. The settings that it leaves out come from `/usr/share/plymouth/plymouthd.defaults`. Removing the package puts the one of plymouth back.

The initramfs holds the theme, with the wallpaper that `background.png` links to. The hook of plymouth only takes the default fonts, so the hook of `hd-plymouth` adds JetBrains Mono. Installing, upgrading or removing the package activates the `update-initramfs` trigger of initramfs-tools, which rebuilds it once dpkg is done, and [hd-bootloader](hd-bootloader.md) then rebuilds its UKIs.

The splash shows when the kernel command line has `splash`, which hd-bootloader adds by default. The graphical bootloader passes its resolution with `video=`, so the splash starts at that resolution.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/share/plymouth/themes/hyprdebian/` | The theme: the script, the images of the input field: the dot, the fill and the outline colours, and `background.png`, a link to `/usr/share/backgrounds/hyprdebian/emerald-1920x1080.png` of hd-art. |
| `/usr/share/hd-plymouth/plymouthd.conf` | The config of plymouthd that the package copies to `/etc/plymouth/plymouthd.conf`. |
| `/etc/plymouth/plymouthd.conf.plymouth` | The config of plymouth, diverted. |
| `/usr/share/initramfs-tools/hooks/hd-plymouth` | Puts the fonts of the theme in the initramfs. |
| `/usr/share/doc/hd-plymouth/copyright` | The copyright and license of the theme. |
