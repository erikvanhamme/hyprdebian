## Introduction
The package `hd-desktop` is the HyprDebian desktop. It depends on the packages of the desktop, so that they are installed with it and stay installed, and boots the system into Hyprland with greetd:
- Hyprland with uwsm, hyprpaper, hypridle, hyprlock and hyprshutdown;
- greetd, which logs the user in to Hyprland at the boot;
- kitty, wofi, mako, PipeWire with wiremix, and the screenshot tools grim, slurp and swappy;
- nwg-look with the GTK and Qt themes, the wallpapers of [hd-art](hd-art.md) and the fonts of [hd-fonts](hd-fonts.md);
- yazi, and the applications audacious, mpv, imv, firefox and meld.

It recommends the screensaver of [hd-screensaver](hd-screensaver.md), which hypridle starts after 2 minutes. Without it, hypridle only locks the screen and turns the monitors off: the command line that starts the screensaver ends in `|| true`, so hypridle logs no failed command.

It also installs the files that new users get in their home directory, the commands that the key bindings run, the dark theme for GTK applications and the sudoers rule of the mount menus.

The package comes with the man pages `man 7 hd-desktop` and `man hd-desktop-render`.

## Building the package
The package is built from `hd-desktop` in this repository:
```
cd hd-desktop
make deb
```

This builds `build/hd-desktop_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It needs the HyprDebian repository for `hd-base`, `hd-art`, `hd-fonts`, `wiremix` and `yazi`. Install it with:
```
sudo apt install ./build/hd-desktop_<version>_all.deb
```

The files involved are:
```
hd-desktop/Makefile
hd-desktop/version.json
hd-desktop/deb/control
hd-desktop/deb/postinst
hd-desktop/deb/postrm
hd-desktop/hd-desktop-render
hd-desktop/templates/greetd.toml.j2
hd-desktop/hd-ask-poweroff, hd-ask-reboot, hd-mode-select, hd-mount, hd-mount-helper,
hd-unmount, hd-volume-down, hd-volume-mute, hd-volume-up
hd-desktop/etc/...
hd-desktop/usr/lib/systemd/system/greetd.service.d/hd-desktop.conf
hd-desktop/man/hd-desktop.7
hd-desktop/man/hd-desktop-render.8
```

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

`sudo hd-desktop-render --dry-run` shows the user and the config without writing it.

## Files for new users
New users get these files in their home directory, from `/etc/skel`:

| Path | |
| --- | --- |
| `~/.config/hypr/hyprland.lua` | The config of Hyprland, with the [key bindings](../HyprDebian/Key%20Bindings.md). |
| `~/.config/hypr/hypridle.conf` | The screensaver of [hd-screensaver](hd-screensaver.md) after 2 minutes, hyprlock after 4, and the monitors off after that. |
| `~/.config/hypr/hyprlock.conf`, `hyprpaper.conf` | The lock screen and the wallpaper. |
| `~/.config/kitty`, `mako`, `wofi`, `mpv` | The terminal, the notifications, the launcher and the media player. |
| `~/.config/alsa`, `pipewire` | The sound. |
| `~/.config/nwg-look`, `~/.local/share/nwg-look` | The GTK theme and font, applied by `nwg-look -a` when Hyprland starts. |
| `~/.config/VSCodium` | The settings of VSCodium. |
| `~/.icons`, `~/Pictures/Screenshots` | Empty directories for the cursor themes and the screenshots. |

Existing users keep their own files; copy the ones they want from `/etc/skel`.

## Commands
The key bindings run these commands, in `/usr/bin`:

| Command | |
| --- | --- |
| `hd-ask-reboot`, `hd-ask-poweroff` | Ask with wofi whether to reboot or power off, then run `hd-reboot` or `hd-poweroff` of [hd-base](hd-base.md). |
| `hd-mode-select` | Choose the resolution and refresh rate of a monitor with wofi. |
| `hd-mount`, `hd-unmount` | Mount a drive or an `/etc/fstab` entry, a drive on `/mnt/<user>/<n>`, and unmount it again, with wofi. |
| `hd-volume-up`, `hd-volume-down`, `hd-volume-mute` | Change the volume of the default output and show it in a notification. |

`hd-mount` and `hd-unmount` run `/usr/libexec/hd-desktop/hd-mount-helper` with sudo. `/etc/sudoers.d/hd-desktop` lets the users in the group `sudo` run it, `mount`, `umount` and the `lsblk` of `hd-mount` without a password.

## Dark theme
`/etc/dconf/db/local.d/00-dark-theme` asks GTK applications for the dark Adwaita theme. `/etc/dconf/profile/user` adds the system database `local` to the one of the user, and the installation compiles it with `dconf update`.

## Removing the package
Removing the package removes the greetd config of `hd-desktop-render` and puts back the one of the package greetd, which logs nobody in. The files in `/etc/skel`, the dconf files and the sudoers rule stay until the package is purged; the settings in `/etc/hyprdebian/config.json` stay.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/sbin/hd-desktop-render` | Writes the greetd config. |
| `/usr/share/hd-desktop/templates/greetd.toml.j2` | The template of the greetd config. |
| `/usr/lib/systemd/system/greetd.service.d/hd-desktop.conf` | Runs `hd-desktop-render` before greetd starts. |
| `/etc/greetd/config.toml` | The greetd config, written by `hd-desktop-render`. |
| `/etc/greetd/config.toml.greetd` | The config of the package greetd, diverted. |
| `/etc/hyprdebian/config.json` | The settings, in the key `hd-desktop`. |
| `/var/log/hyprland.log` | The output of Hyprland. |
| `/usr/bin/hd-*` | The commands of the key bindings. |
| `/usr/libexec/hd-desktop/hd-mount-helper` | Mounts and unmounts for the mount menus, run with sudo. |
| `/etc/sudoers.d/hd-desktop` | Lets the group `sudo` mount without a password. |
| `/etc/dconf/profile/user`, `/etc/dconf/db/local.d/00-dark-theme` | The dark theme for GTK applications. |
| `/etc/skel/...` | The files for new users. |
