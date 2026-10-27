## Introduction
The package `hd-desktop` holds the boot splash of HyprDebian: the plymouth theme `hyprdebian`. See [hd-desktop](hd-desktop.md).

The splash looks like the lock screen of hyprlock, as `~/.config/hypr/hyprlock.conf` of `hd-desktop` sets it up, in the theme of the system of [hd-theme](hd-theme.md): the background of the theme from [hd-art](hd-art.md), scaled to cover the screen, and, when the boot asks for a password, a rounded input field in the middle with the colour `dark` at its `opacity` as the fill and an outline in `highlight`, with text in the font of the theme in the colour `light`.

The script of plymouth cannot read the theme at boot, so `hd-plymouth-render` renders the files of the plymouth theme from it into `/var/lib/hd-plymouth/theme`; see [Rendering the splash](#rendering-the-splash).

The theme is based on the theme `emerald` of the Debian package `desktop-base`. `hd-desktop` conflicts with `desktop-base`, whose themes it replaces.

`hd-desktop` comes with the man pages `man 7 hd-plymouth` and `man hd-plymouth-render`.

## Building the package
The boot splash is part of the package `hd-desktop`, built from `packages/hd-desktop`; see [hd-desktop](hd-desktop.md). It needs `plymouth`, `plymouth-label`, `initramfs-tools`, `fontconfig`, `python3`, `python3-jinja2`, `python3-cairo`, `python3-gi` and `gir1.2-pango-1.0`, and the wallpapers and the font of `hd-desktop`.

The files involved are:
```
packages/hd-desktop/plymouth/theme/hyprdebian.plymouth
packages/hd-desktop/templates/hd-plymouth/hyprdebian.script.j2
packages/hd-desktop/hd-plymouth-render
packages/hd-desktop/plymouth/plymouthd.conf
packages/hd-desktop/plymouth/initramfs/hd-plymouth
packages/hd-desktop/hd-plymouth-handoff
packages/hd-desktop/usr/lib/systemd/system/hd-plymouth-handoff.service
packages/hd-desktop/usr/lib/systemd/system/plymouth-quit-wait.service.d/hd-desktop.conf
packages/hd-desktop/deb/triggers
packages/hd-desktop/deb/postinst
packages/hd-desktop/deb/postrm
packages/hd-desktop/deb/copyright
packages/hd-desktop/man/hd-plymouth.7
packages/hd-desktop/man/hd-plymouth-render.8
```

## Passwords
The ZFS script of the initramfs asks for the password of an encrypted dataset with plymouth when the splash shows, up to three times. The field does not show the prompt of the question.

| State | Outline | In the field |
| --- | --- | --- |
| Empty | `highlight`, `outer_color` of hyprlock.conf | `Enter Password` |
| Typing | `highlight` | a dot per key press in `light`, `font_color` of hyprlock.conf |
| Checking, after Enter | yellow, the default `check_color` of hyprlock | the dots; the field hides after about two seconds |
| Wrong password | red, the default `fail_color` of hyprlock | `Wrong password (<attempts>)`, until the next key press |

The fill is `dark` at `opacity` in every state, like `inner_color` of hyprlock.conf, and the corners are rounded at `radius`, like `rounding`. The splash knows a password was wrong when the same prompt comes back after the question ended.

## Setting the theme
plymouth reads its theme from `/etc/plymouth/plymouthd.conf`. The package diverts the one of plymouth to `/etc/plymouth/plymouthd.conf.plymouth` and copies its own in its place, on every installation and upgrade. The settings that it leaves out come from `/usr/share/plymouth/plymouthd.defaults`. Removing the package puts the one of plymouth back.

The initramfs holds the splash. The hook `hd-plymouth` of `hd-desktop` runs `hd-plymouth-render`, then copies `/var/lib/hd-plymouth/theme` to the same path in the initramfs, with the background that `background.png` links to, and adds the font files of the theme, as the hook of plymouth only takes the default fonts. Installing, upgrading or removing the package activates the `update-initramfs` trigger of initramfs-tools, which rebuilds it once dpkg is done, and [hd-bootloader](hd-bootloader.md) then rebuilds its UKIs.

The splash of a shutdown or reboot, which plymouthd shows from the root file system, uses the same files in `/var/lib/hd-plymouth/theme`.

The splash shows when the kernel command line has `splash`, which hd-bootloader adds by default. The graphical bootloader passes its resolution with `video=`, so the splash starts at that resolution.

## Handing over to Hyprland
At the end of the boot, the screen goes from the splash to Hyprland without the console in between, as with GDM:

1. `hd-plymouth-handoff.service`, which `greetd.service` wants, runs before greetd and takes the place of `plymouth-quit.service`, whose `plymouth quit` would give the screen to the console. It runs `plymouth deactivate`: plymouth stops drawing and lets go of the display, but keeps it open, so the splash stays on the screen.
2. greetd starts the session of Hyprland on tty1, the VT of the splash. It does not switch the VT and does not set it to text mode, which would show the console, as its config has `vt = "none"`; see [hd-desktop](hd-desktop.md#booting-into-hyprland).
3. `/usr/libexec/hd-desktop/hd-plymouth-handoff quit` waits until another process than plymouthd has a DRM card open, as systemd-logind and Hyprland do once Hyprland has the display, and runs `plymouth quit --retain-splash`, which leaves the screen to Hyprland.

When greetd fails, or logs nobody in and starts the console on tty1, or when Hyprland does not have the display after 30 seconds, it runs a plain `plymouth quit`, and the console shows. `greetd.service` starts after `plymouth-quit-wait.service`, whose `plymouth --wait` waits until plymouth is gone; the drop-in `plymouth-quit-wait.service.d/hd-desktop.conf` has it run `hd-plymouth-handoff wait` instead, which returns at once while the splash is handed over, and runs `plymouth --wait` otherwise, as when the boot does not start greetd.

Hyprland shows the colour `dark` of the theme until hyprpaper draws the wallpaper; see [hd-desktop](hd-desktop.md#booting-into-hyprland). When Hyprland sets another mode than the one of the splash, the screen may blank while the mode changes.

## Rendering the splash
```
hd-plymouth-render                      render the splash
hd-plymouth-render --update-initramfs   render it, and rebuild the initramfs when it changed
```

`hd-plymouth-render`, in `/usr/sbin`, renders the splash as root from the key `hd-theme` of `/etc/hyprdebian/config.json`, the theme of the system; the themes of the users are not used. A member that is missing or not valid takes the value of the theme `default`, which is also used when the system has no theme. It writes, in `/var/lib/hd-plymouth/theme`:

| File | Made from |
| --- | --- |
| `hyprdebian.script` | The template `/usr/share/hd-plymouth/templates/hyprdebian.script.j2`, with the font at `font-size-normal` in the field and `font-size-small` below it, `light` for all text and `dark` behind the background. |
| `field-input.png`, `field-check.png`, `field-fail.png` | The field of 650 by 100 pixels: the fill in `dark` at `opacity`, corners of `radius`, and an outline of `border` pixels in `highlight`, yellow and red. The inner corners have the radius minus the border. The script scales it on a screen narrower than the field. |
| `dot.png` | A dot in `light`. |
| `background.png` | A link to the image of the family `background` that best fits the largest connected monitor: the smallest that covers it, else the largest; without one, a screen of 1920x1080. |

`/var/lib/hd-plymouth/fonts` lists the font files, as `fc-match` finds them, for the hook. A file is only written when it changes.

`hd-theme --select` as root, and `hd-theme --add` when it replaces the theme of the system, run `hd-plymouth-render --update-initramfs`, which rebuilds the initramfs of all kernels with `update-initramfs -u -k all` when the splash changed and plymouth uses the theme `hyprdebian`; [hd-bootloader](hd-bootloader.md) then rebuilds its UKIs. That takes a while; `--no-initramfs` of `hd-theme` leaves it out, and the next rebuild, such as for a new kernel, takes the new splash. The installation of `hd-desktop` renders the splash too, and the trigger `update-initramfs` rebuilds the initramfs. A purge removes `/var/lib/hd-plymouth`.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/share/plymouth/themes/hyprdebian/hyprdebian.plymouth` | The theme, which names the files in `/var/lib/hd-plymouth/theme` as its `ImageDir` and `ScriptFile`. |
| `/var/lib/hd-plymouth/theme/` | The rendered script, input fields, dot and `background.png`. |
| `/var/lib/hd-plymouth/fonts` | The font files that the hook puts in the initramfs. |
| `/usr/sbin/hd-plymouth-render` | Renders the splash in the theme of the system. |
| `/usr/share/hd-plymouth/templates/hyprdebian.script.j2` | The template of the script. |
| `/usr/share/hd-plymouth/plymouthd.conf` | The config of plymouthd that the package copies to `/etc/plymouth/plymouthd.conf`. |
| `/etc/plymouth/plymouthd.conf.plymouth` | The config of plymouth, diverted. |
| `/usr/share/initramfs-tools/hooks/hd-plymouth` | Renders the splash and puts it and its fonts in the initramfs. |
| `/usr/lib/systemd/system/hd-plymouth-handoff.service`, `/usr/lib/systemd/system/plymouth-quit-wait.service.d/hd-desktop.conf`, `/usr/libexec/hd-desktop/hd-plymouth-handoff` | Hand the splash over to Hyprland. |
| `/usr/share/doc/hd-desktop/copyright` | The copyright and license of the theme. |
