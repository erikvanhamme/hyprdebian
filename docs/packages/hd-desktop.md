## Introduction
The package `hd-desktop` is the HyprDebian desktop. It depends on the packages of the desktop, so that they are installed with it and stay installed, and boots the system into Hyprland with greetd:
- Hyprland with uwsm, hyprpaper, hypridle, hyprlock and hyprshutdown;
- greetd, which logs the user in to Hyprland at the boot;
- kitty, mako, PipeWire with wiremix, and the screenshot tools grim, slurp and swappy;
- nwg-look with the GTK and Qt themes, and plymouth;
- yazi, and the applications audacious, mpv, imv, firefox and meld.

It holds the parts of the desktop, each with a page of its own:
- [hd-art](hd-art.md): the wallpapers;
- [hd-fonts](hd-fonts.md): JetBrains Mono with the icons of Nerd Fonts;
- [hd-menu](hd-menu.md): the keyboard-driven menu of the key bindings and the application launcher;
- [hd-theme](hd-theme.md): the theme engine;
- [hd-screensaver](hd-screensaver.md): the screensaver, which hypridle starts after 2 minutes;
- [hd-plymouth](hd-plymouth.md): the boot splash, which looks like the lock screen of hyprlock and asks for the password of an encrypted ZFS dataset.

These were the packages `hd-art`, `hd-fonts`, `hd-menu`, `hd-theme`, `hd-screensaver` and `hd-plymouth` before. `hd-desktop` conflicts with them, replaces and provides them, so an upgrade removes them; the themes, the selected theme and the theme files of the users stay.

It also installs the files that new users get in their home directory, the commands that the key bindings run, the menu of the application launcher, the dark theme for GTK applications and the sudoers rule of the mount menus.

The package comes with the man pages `man 7 hd-desktop`, `man hd-desktop-render` and `man hd-launcher`. The man pages of the parts are listed on their pages.

## Building the package
The package is built from `packages/hd-desktop` in this repository:
```
cd packages/hd-desktop
make deb
```

This builds `build/hd-desktop_<version>_<arch>.deb`. Its version is the version of HyprDebian, from `version.json` in the root of this repository, as for [hd-base](hd-base.md); hd-desktop has no version of its own. It depends on `hd-base` of exactly the same version, so the desktop and the base system always come from the same release. hd-menu is compiled in a container, so the package is for the architecture of the build computer; see [hd-menu](hd-menu.md#building-the-package). The fonts are downloaded on the first build; see [hd-fonts](hd-fonts.md#building-the-package). Install it with:
```
sudo apt install ./build/hd-desktop_<version>_<arch>.deb
```

It needs the HyprDebian repository for `hd-base`, `wiremix` and `yazi`.

The files involved are:
```
packages/hd-desktop/Makefile
version.json
packages/hd-desktop/deb/control
packages/hd-desktop/deb/copyright
packages/hd-desktop/deb/triggers
packages/hd-desktop/deb/preinst
packages/hd-desktop/deb/postinst
packages/hd-desktop/deb/postrm
packages/hd-desktop/hd-desktop-render
packages/hd-desktop/hd-launcher
packages/hd-desktop/templates/greetd.toml.j2
packages/hd-desktop/hd-ask-poweroff, hd-ask-reboot, hd-mode-select, hd-mount, hd-mount-helper,
hd-unmount, hd-volume-down, hd-volume-mute, hd-volume-up
packages/hd-desktop/etc/...
packages/hd-desktop/usr/lib/systemd/system/greetd.service.d/hd-desktop.conf
packages/hd-desktop/man/hd-desktop.7
packages/hd-desktop/man/hd-desktop-render.8
packages/hd-desktop/man/hd-launcher.8
```

The files of the parts are listed on their pages.

## Booting into Hyprland
greetd runs on tty7 and logs the user in to Hyprland at the boot, without a password, with `uwsm start hyprland.desktop`. The output of Hyprland goes to `/var/log/hyprland.log`, which belongs to the user. greetd does this once per boot: after a logout from Hyprland, it switches to the console on tty1 and stops.

greetd enables `greetd.service` on its installation. Its config, `/etc/greetd/config.toml`, is written by `hd-desktop-render` from the template `/usr/share/hd-desktop/templates/greetd.toml.j2`; the one of the package greetd is diverted to `/etc/greetd/config.toml.greetd`. The installation runs `hd-desktop-render`, and `greetd.service` runs it before every start, with the drop-in `/usr/lib/systemd/system/greetd.service.d/hd-desktop.conf`. A user that is made after the installation, as by the installer, is thus logged in from the first boot on. When `hd-desktop-render` fails, greetd starts with the config it has.

## Settings
The settings are the key `hd-desktop` of `/etc/hyprdebian/config.json`. There are no defaults: a missing setting is looked up on the system.

| Key | Without it | |
| --- | --- | --- |
| `user` | The user account with the lowest UID from `UID_MIN` to `UID_MAX` of `/etc/login.defs`, the first user that was made. | The user that greetd logs in to Hyprland. |

Without such an account, or when the user of the setting does not exist, greetd logs nobody in: it switches to the console on tty1 and stops. To log in another user from the next boot on:
```
sudo hd-config-update hd-desktop '{"user": "alice"}'
sudo hd-desktop-render
```

`sudo hd-desktop-render --dry-run` shows the user and the config without writing it, and `hd-desktop-render --print-user` only prints the user.

## Files for new users
New users get these files in their home directory, from `/etc/skel`:

| Path | |
| --- | --- |
| `~/.config/hypr/hyprland.lua` | The config of Hyprland, with the [key bindings](../HyprDebian/Key%20Bindings.md). |
| `~/.config/hypr/hypridle.conf` | The screensaver of [hd-screensaver](hd-screensaver.md) after 2 minutes, hyprlock after 4, and the monitors off after that. |
| `~/.config/hypr/hyprlock.conf`, `hyprpaper.conf` | The lock screen and the wallpaper. |
| `~/.config/kitty`, `mako`, `mpv` | The terminal, the notifications and the media player. |
| `~/.config/alsa`, `pipewire` | The sound. |
| `~/.config/nwg-look`, `~/.local/share/nwg-look` | The GTK theme and font, applied by `nwg-look -a` when Hyprland starts. |
| `~/.config/VSCodium` | The settings of VSCodium. |
| `~/.icons`, `~/Pictures/Screenshots` | Empty directories for the cursor themes and the screenshots. |

The configs of Hyprland, hyprlock, hyprpaper, kitty and mako take their font, colours, borders, radius, gaps and background from the theme files that `hd-theme --apply` of [hd-theme](hd-theme.md) writes next to them, from the theme of the system. The user runs `hd-theme --apply` to write them, and again after another theme is selected. Settings below the line that uses the theme file take the place of the theme.

The installation of the package also gives these files to the user who is logged in to Hyprland, the one that `hd-desktop-render --print-user` prints, when that user exists: it copies the files of `/etc/skel` that are missing from their home directory, as that user, with `cp -a --update=none`. Files that the user has are kept. This is done on the first installation, and on an upgrade from a version before 0.3, which did not do it; later upgrades do not copy again, so a file that the user removed stays away. When the installer runs, the user does not exist yet, and the installer copies `/etc/skel` itself.

Other users keep their own files. To copy the files they are missing:
```
cp -a --update=none /etc/skel/. ~/
```

## Commands
The key bindings run these commands, in `/usr/bin`:

| Command | |
| --- | --- |
| `hd-ask-reboot`, `hd-ask-poweroff` | Ask with [hd-menu](hd-menu.md) whether to reboot or power off, then run `hd-reboot` or `hd-poweroff` of [hd-base](hd-base.md). |
| `hd-mode-select` | Choose the resolution and refresh rate of a monitor with [hd-menu](hd-menu.md). |
| `hd-mount`, `hd-unmount` | Mount a drive or an `/etc/fstab` entry, a drive on `/mnt/<user>/<n>`, and unmount it again, with [hd-menu](hd-menu.md). |
| `hd-volume-up`, `hd-volume-down`, `hd-volume-mute` | Change the volume of the default output and show it in a notification. |

The menus above and the application launcher use [hd-menu](hd-menu.md), which has a lock of its own, so a second press of a key binding opens no second menu.

`hd-mount` and `hd-unmount` run `/usr/libexec/hd-desktop/hd-mount-helper` with sudo. `/etc/sudoers.d/hd-desktop` lets the users in the group `sudo` run it, `mount`, `umount` and the `lsblk` of `hd-mount` without a password.

## Application launcher
`SUPER + SPACE` opens the application launcher: the menu `/var/cache/hd-launcher/launcher.json` in [hd-menu](hd-menu.md), 40% of the width and the height of the screen. The application chosen is started with `uwsm app -- <ID>`, as a systemd user unit of its own: its own cgroup, its output in the journal and a clean stop at the logout. The other applications of the key bindings, such as the terminal, the browser and VSCodium, are started with `uwsm app` too; the menus and short commands are not.

`hd-launcher`, in `/usr/sbin`, writes the menu as root from the `.desktop` files in `/usr/share/applications` and its subdirectories. An application is in it when its `.desktop` file is of the `Type` Application, has a `Name` and an `Exec`, is not `NoDisplay` or `Hidden`, is not left out for Hyprland by `OnlyShowIn` or `NotShowIn`, has its `TryExec` installed, when it has one, and is not in the exclusion list. Its `Name` is the label and its desktop file ID, such as `firefox.desktop`, the value; the options are sorted on the `Name`, in ascending order, without regard to case.

The icon is looked up by the `Icon` of the application in the icon themes hicolor and Adwaita and in `/usr/share/pixmaps`, or taken from its path when it is one. A PNG is used as it is; an SVG is rendered to a PNG of the `icon_size` of the [theme](hd-theme.md) in `/var/cache/hd-launcher/icons`, only when the PNG there was not rendered from that SVG at that size.

The apt hook `/etc/apt/apt.conf.d/90hd-launcher` runs `hd-launcher` each time apt has run dpkg: after an installation, an upgrade or a removal. A failure is reported but does not fail apt. The installation of the package runs it too. After another theme is selected, with another `icon_size`, run it again:
```
sudo hd-launcher
```

`hd-launcher --dry-run` prints the menu without writing it, as any user.

The key `hd-launcher` of `/etc/hyprdebian/config.json` is a list of the desktop file IDs to leave out; the `.desktop` at the end may be left out:
```
sudo hd-config-update hd-launcher '["org.gnome.Meld.desktop", "debian-xterm"]'
sudo hd-launcher
```

## Dark theme
`/etc/dconf/db/local.d/00-dark-theme` asks GTK applications for the dark Adwaita theme. `/etc/dconf/profile/user` adds the system database `local` to the one of the user, and the installation compiles it with `dconf update`.

## Removing the package
Removing the package removes the greetd config of `hd-desktop-render` and puts back the one of the package greetd, which logs nobody in. The files in `/etc/skel`, the dconf files, the sudoers rule, the apt hook and the menu of the launcher stay until the package is purged; the apt hook does nothing without `hd-launcher`; the settings in `/etc/hyprdebian/config.json` stay.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/sbin/hd-desktop-render` | Writes the greetd config. |
| `/usr/share/hd-desktop/templates/greetd.toml.j2` | The template of the greetd config. |
| `/usr/lib/systemd/system/greetd.service.d/hd-desktop.conf` | Runs `hd-desktop-render` before greetd starts. |
| `/etc/greetd/config.toml` | The greetd config, written by `hd-desktop-render`. |
| `/etc/greetd/config.toml.greetd` | The config of the package greetd, diverted. |
| `/etc/hyprdebian/config.json` | The settings, in the keys `hd-desktop` and `hd-launcher`. |
| `/usr/sbin/hd-launcher` | Writes the menu of the application launcher. |
| `/var/cache/hd-launcher/launcher.json`, `/var/cache/hd-launcher/icons` | The menu of the application launcher and the PNGs rendered from SVG icons. |
| `/etc/apt/apt.conf.d/90hd-launcher` | Runs `hd-launcher` each time apt has run dpkg. |
| `/var/log/hyprland.log` | The output of Hyprland. |
| `/usr/bin/hd-*` | The commands of the key bindings. |
| `/usr/libexec/hd-desktop/hd-mount-helper` | Mounts and unmounts for the mount menus, run with sudo. |
| `/etc/sudoers.d/hd-desktop` | Lets the group `sudo` mount without a password. |
| `/etc/dconf/profile/user`, `/etc/dconf/db/local.d/00-dark-theme` | The dark theme for GTK applications. |
| `/etc/skel/...` | The files for new users. |
