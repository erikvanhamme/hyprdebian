## Introduction
The package `hd-desktop` is the HyprDebian desktop. It depends on the packages of the desktop, so that they are installed with it and stay installed, and boots the system into Hyprland with greetd:
- Hyprland with uwsm, hyprpaper, hypridle and hyprlock, its dialogs from `hyprland-guiutils`, and XWayland for the programs that only speak X11;
- greetd, which logs the user in to Hyprland at the boot;
- the desktop portals of Hyprland and GTK, for screen sharing, the file chooser and the dark mode;
- kitty, mako, PipeWire with wiremix, its PulseAudio server and rtkit, and the screenshot tools grim, slurp and swappy;
- the dark GTK theme Adwaita-dark, qt6ct and the Wayland plugin `qt6-wayland` for the Qt applications, the colour emoji of `fonts-noto-color-emoji`, and plymouth;
- yazi, and the applications audacious with its terminal interface, of [hd-audacious](hd-audacious.md), mpv, imv and firefox.

HyprDebian installs no recommends, see [hd-base](hd-base.md), so the package depends on everything the desktop needs.

Versions before 1.27 also depended on `git`, `git-delta`, `meld`, `nwg-look`, `pulseaudio-utils`, `qt5-gtk-platformtheme`, `qt6-gtk-platformtheme` and `libglib2.0-bin`, and had `.gitconfig` and the settings of nwg-look in `/etc/skel`. An upgrade removes those files, unless they were changed. `apt autoremove` then removes the packages, and the postinst says so; mark the ones that are still used as installed by hand first, such as `sudo apt-mark manual git`.

It holds the parts of the desktop, each with a page of its own:
- [hd-art](hd-art.md): the wallpapers;
- [hd-fonts](hd-fonts.md): JetBrains Mono with the icons of Nerd Fonts;
- [hd-menu](hd-menu.md): the keyboard-driven menu of the key bindings and the application launcher;
- [hd-message](hd-message.md): the message box, a title, a text and a row of buttons;
- [hd-theme](hd-theme.md): the theme engine;
- [hd-screensaver](hd-screensaver.md): the screensaver, which hypridle starts after 2 minutes;
- [hd-plymouth](hd-plymouth.md): the boot splash, which looks like the lock screen of hyprlock in the theme of the system and asks for the password of an encrypted ZFS dataset.

These were the packages `hd-art`, `hd-fonts`, `hd-menu`, `hd-theme`, `hd-screensaver` and `hd-plymouth` before. `hd-desktop` conflicts with them, replaces and provides them, so an upgrade removes them; the themes, the selected theme and the theme files of the users stay.

It also installs the files that new users get in their home directory, the commands that the key bindings run, the menu of the application launcher, the dark theme for GTK applications and the sudoers rule of the mount menus.

The package comes with the man pages `man 7 hd-desktop`, `man hd-desktop-render` and `man hd-launcher-cache`. The man pages of the parts are listed on their pages.

## Building the package
The package is built from `packages/hd-desktop` in this repository:
```
cd packages/hd-desktop
make deb
```

This builds `build/hd-desktop_<version>_<arch>.deb`. Its version is the version of HyprDebian, from `version.json` in the root of this repository, as for [hd-base](hd-base.md); hd-desktop has no version of its own. It depends on `hd-base` of exactly the same version, so the desktop and the base system always come from the same release. hd-menu and hd-message are compiled in a container, so the package is for the architecture of the build computer; see [hd-menu](hd-menu.md#building-the-package). The fonts are downloaded on the first build; see [hd-fonts](hd-fonts.md#building-the-package). Install it with:
```
sudo apt install ./build/hd-desktop_<version>_<arch>.deb
```

It needs the HyprDebian repository for `hd-base`, `hd-audacious`, `wiremix` and `yazi`.

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
packages/hd-desktop/hd-launcher-cache
packages/hd-desktop/templates/greetd.toml.j2
packages/hd-desktop/hd-ask-poweroff, hd-ask-reboot, hd-mode-select, hd-mount, hd-mount-helper,
hd-unmount, hd-volume-down, hd-volume-mute, hd-volume-up
packages/hd-desktop/etc/...
packages/hd-desktop/usr/lib/systemd/system/greetd.service.d/hd-desktop.conf
packages/hd-desktop/usr/share/hd-desktop/icons/warning.svg
packages/hd-desktop/hd-reboot-notify
packages/hd-desktop/hd-shutdown
packages/hd-desktop/usr/lib/systemd/user/hd-reboot-notify.path
packages/hd-desktop/usr/lib/systemd/user/hd-reboot-notify.service
packages/hd-desktop/man/hd-desktop.7
packages/hd-desktop/man/hd-desktop-render.8
packages/hd-desktop/man/hd-launcher-cache.1
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
| `~/.config/hypr/hyprtoolkit.conf` | The colours and the font of the dialogs of Hyprland, see [Dark theme](#dark-theme). |
| `~/.config/kitty`, `mako`, `mpv` | The terminal, the notifications and the media player. |
| `~/.config/alsa`, `pipewire` | The sound. |
| `~/.config/qt5ct`, `qt6ct` | The dark theme and the font of the Qt applications, see [Dark theme](#dark-theme). |
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

## Reboot and power off
In the desktop session, `hd-reboot` and `hd-poweroff` of [hd-base](hd-base.md#hd-reboot-and-hd-poweroff), and so `hd-ask-reboot` and `hd-ask-poweroff`, run `/usr/libexec/hd-desktop/hd-shutdown reboot` or `poweroff`. It asks every window to close, as `SUPER + W` does, and runs `systemctl reboot` or `systemctl poweroff` once all are gone.

An application that asks something first, such as whether to save a file, keeps its windows open. When the windows that are left have stayed the same for 3 seconds, [hd-message](hd-message.md) names their applications, one line per process with its window class, the name and pid of the process and the titles of its windows, and offers:

| Button | |
| --- | --- |
| Wait | Closes the message: answer the applications yourself, and the shutdown goes on once their windows are gone. Escape does the same. |
| Kill | Ends the processes of those windows with SIGTERM, and those still running 3 seconds later with SIGKILL. Their unsaved work is lost. |
| Cancel | Stops the shutdown. The applications that have closed stay closed. |

After Wait, the message comes back when an application closes the windows of its question but stays open, as when the question was cancelled, or when `hd-reboot` or `hd-poweroff` runs again, such as by its key binding. The windows of a question are those that `hd-shutdown` did not ask to close. An application that asks something, or closes, does not bring it back, so the questions can be answered in peace. A question inside the window itself, such as that of kitty, is not seen: when it is cancelled, run `hd-reboot` or `hd-poweroff` again to get the message back. An open message closes by itself when the windows change, and shows again with the windows that are left.

`hd-shutdown` does its work in the transient user unit `hd-shutdown.service`, so it is not stopped when it closes the terminal it was started from, and its output is in the journal:
```
journalctl --user -u hd-shutdown
```
While the unit runs, a second `hd-shutdown` sends it SIGUSR1, which shows the message again, and starts no second one.

## Pending reboot
The user unit `hd-reboot-notify.path` starts `hd-reboot-notify.service` in the desktop session when `/run/reboot-required` exists: when an upgrade writes it, and at every login until the reboot, see `hd-reboot-check` in [hd-base](hd-base.md#hd-reboot-check). It runs `/usr/libexec/hd-desktop/hd-reboot-notify`, which shows a notification with the reasons of `hd-reboot-check`. The service stays active after the notification, so it shows once per session. The postinst enables the unit for every user on its first installation, by its preset, and keeps the state the admin gave it over upgrades; a removal masks it, so a reinstall keeps that state, and a purge forgets it. To switch it off for a user:
```
systemctl --user disable --now hd-reboot-notify.path
```

## Application launcher
`SUPER + SPACE` opens the application launcher: the menu that `hd-launcher-cache` prints, in [hd-menu](hd-menu.md), 40% of the width and the height of the screen. The application or item chosen is started with `uwsm app --`, as a systemd user unit of its own: its own cgroup, its output in the journal and a clean stop at the logout. The other applications of the key bindings, such as the terminal, the browser and VSCodium, are started with `uwsm app` too; the menus and short commands are not.

`hd-launcher-cache`, in `/usr/bin`, works for whoever runs it:

| | Root | Other users |
| --- | --- | --- |
| Settings, the keys `hd-launcher` and `hd-theme` | `/etc/hyprdebian/config.json` | `~/.config/hyprdebian/config.json` when it has the key, else `/etc/hyprdebian/config.json` |
| Cache | `/var/cache/hd-launcher` | `~/.cache/hd-launcher` |

```
hd-launcher-cache              print the menu for hd-menu
hd-launcher-cache --update     write the menu and its icons in the cache
hd-launcher-cache --update-all as root: write the cache of the system and of every user in the group users
hd-launcher-cache --dry-run    print the menu that --update would write, write nothing
```

Without arguments, it prints the menu in the cache of the user, else the one in the cache of the system. When neither has one, it prints a menu with the single option `No cache` and a warning icon, `/usr/share/hd-desktop/icons/warning.svg`, which does nothing when chosen. Root only looks in the cache of the system. The key binding in `~/.config/hypr/hyprland.lua` pipes it into `hd-menu`.

`--update` writes the menu from the `.desktop` files in `/usr/share/applications` and its subdirectories. An application is in it when its `.desktop` file is of the `Type` Application, has a `Name` and an `Exec`, is not `NoDisplay` or `Hidden`, is not left out for Hyprland by `OnlyShowIn` or `NotShowIn`, has its `TryExec` installed, when it has one, and is not left out by the settings below. The items added in the settings are in it too. The label is the `Name` of an application or the `name` of an item. The value is the command line that starts it, quoted for the shell, which the launcher runs with `sh -c`: `uwsm app --` with the desktop file ID of an application, such as `firefox.desktop`, or the command of an item. The options are sorted on the label, in ascending order, without regard to case.

The icon is looked up by the `Icon` of the application in the icon themes hicolor and Adwaita and in `/usr/share/pixmaps`, or taken from its path when it is one. A PNG is used as it is; an SVG is rendered to a PNG of the `icon_size` of the [theme](hd-theme.md) in `icons` of the cache, only when the PNG there was not rendered from that SVG at that size.

`--update-all`, for root only, writes the cache of the system and then runs `hd-launcher-cache --update` with `runuser` as each user whose primary or supplementary group is `users` and whose home directory exists, so each cache belongs to its user and is made from the settings of that user. A user whose cache cannot be written is reported; the others are still written.

The apt hook `/etc/apt/apt.conf.d/90hd-launcher` runs `hd-launcher-cache --update-all` each time apt has run dpkg: after an installation, an upgrade or a removal, so the menus of the system and of the users follow the applications. A failure is reported but does not fail apt. The installation of the package runs it too. The cache of a user who is not in the group `users` is only written when that user runs `hd-launcher-cache --update`. After another theme is selected, with another `icon_size`, or after the settings changed, run it again:
```
sudo hd-launcher-cache --update
hd-launcher-cache --update
```

The key `hd-launcher` of the configuration leaves applications out and adds items of its own:
```json
"hd-launcher": {
    "exclude": ["debian-xterm.desktop", "debian-uxterm.desktop", "uuctl.desktop"],
    "add": [
        {
            "name": "Btop",
            "command": ["kitty", "-e", "btop"],
            "icon": "utilities-system-monitor"
        }
    ]
}
```

| Member | | |
| --- | --- | --- |
| `exclude` | Optional | The desktop file IDs to leave out; the `.desktop` at the end may be left out. `hd-launcher-cache --dry-run` shows the ID of every application. |
| `add` | Optional | The items to add. |
| `add[].name` | | The label. |
| `add[].command` | | The words of the command, a list of strings, run without a shell. For pipes, variables or `~`, use `["sh", "-c", "..."]`. |
| `add[].icon` | Optional | The path of a PNG or SVG, or the name of an icon, looked up as the `Icon` of an application. An icon that is not found is reported and left out. |

Other members, or a key that is not such an object, are an error, and the menu is not written. Write the key with `hd-config-update`, then the menu; with sudo for the system, without for yourself only:
```
sudo hd-config-update hd-launcher '{"exclude": ["debian-xterm"], "add": [{"name": "Btop", "command": ["kitty", "-e", "btop"]}]}'
sudo hd-launcher-cache --update
hd-config-update hd-launcher '{"exclude": ["debian-xterm"]}'
hd-launcher-cache --update
```

## Dark theme
The GTK and the Qt applications and the dialogs of Hyprland look the same: dark, in the colours of Adwaita-dark, in `JetBrainsMono NFM Medium 11`, and with the file chooser of GTK.

`/etc/dconf/db/local.d/00-dark-theme` asks GTK applications for the dark Adwaita theme, Adwaita-dark of `gnome-themes-extra-data`, and `01-font` for the font of HyprDebian, `JetBrainsMono NFM Medium 11`, of the interface, the documents and the monospaced text. `/etc/dconf/profile/user` adds the system database `local` to the one of the user, and the installation compiles it with `dconf update`. GTK applications on XWayland have no XSETTINGS and do not read dconf; they get the same from `/etc/gtk-3.0/settings.ini` and `/etc/gtk-4.0/settings.ini`.

The Qt applications get it from qt5ct and qt6ct, which `QT_QPA_PLATFORMTHEME=qt5ct` in `~/.config/hypr/hyprland.lua` loads for Qt 5 and Qt 6 alike: `~/.config/qt5ct/qt5ct.conf` and `~/.config/qt6ct/qt6ct.conf` give them the style Fusion with the colour scheme `hyprdebian`, the colours of Adwaita-dark, in `/usr/share/qt5ct/colors` and `/usr/share/qt6ct/colors`, the font, the Adwaita icons and the file chooser of the desktop portals. The commands `qt5ct` and `qt6ct` change them. The package depends on `qt6ct`, `qt6-xdgdesktopportal-platformtheme` and `qt6-wayland`, which lets the Qt 6 applications run on Wayland instead of XWayland, as `QT_QPA_PLATFORM=wayland;xcb` asks for first. It does not depend on qt5ct, as the desktop has no Qt 5 applications; for those, install:

```bash
sudo apt install qt5ct qt5-xdgdesktopportal-platformtheme qtwayland5
```

The dialogs of Hyprland, of `hyprland-guiutils`, get the same colours and font from `~/.config/hypr/hyprtoolkit.conf` of hyprtoolkit.

An upgrade from a version before 1.28 gives the user who is logged in to Hyprland the folders `~/.config/qt5ct` and `~/.config/qt6ct` and `~/.config/hypr/hyprtoolkit.conf`, as the first installation does, and says to set `QT_QPA_PLATFORMTHEME` to `qt5ct` in their `~/.config/hypr/hyprland.lua`, which still names `gtk3`, and to remove `QT_STYLE_OVERRIDE`, as in `/etc/skel/.config/hypr/hyprland.lua`.

## Removing the package
Removing the package removes the greetd config of `hd-desktop-render` and puts back the one of the package greetd, which logs nobody in. The files in `/etc/skel`, the dconf files, the sudoers rule, the apt hook and the menu of the launcher stay until the package is purged; the apt hook does nothing without `hd-launcher-cache`; the settings in `/etc/hyprdebian/config.json` and the caches and settings of the users in their home directories stay.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/sbin/hd-desktop-render` | Writes the greetd config. |
| `/usr/share/hd-desktop/templates/greetd.toml.j2` | The template of the greetd config. |
| `/usr/lib/systemd/system/greetd.service.d/hd-desktop.conf` | Runs `hd-desktop-render` before greetd starts. |
| `/etc/greetd/config.toml` | The greetd config, written by `hd-desktop-render`. |
| `/etc/greetd/config.toml.greetd` | The config of the package greetd, diverted. |
| `/etc/hyprdebian/config.json` | The settings, in the keys `hd-desktop` and `hd-launcher`. |
| `~/.config/hyprdebian/config.json` | The settings of a user, in the key `hd-launcher`. |
| `/usr/bin/hd-launcher-cache` | Writes and prints the menu of the application launcher. |
| `/var/cache/hd-launcher/launcher.json`, `/var/cache/hd-launcher/icons` | The menu of the application launcher of the system and the PNGs rendered from SVG icons. |
| `~/.cache/hd-launcher/launcher.json`, `~/.cache/hd-launcher/icons` | The same, of a user. |
| `/usr/share/hd-desktop/icons/warning.svg` | The icon of the menu without a cache. |
| `/etc/apt/apt.conf.d/90hd-launcher` | Runs `hd-launcher-cache --update-all` each time apt has run dpkg. |
| `/var/log/hyprland.log` | The output of Hyprland. |
| `/usr/bin/hd-*` | The commands of the key bindings. |
| `/usr/libexec/hd-desktop/hd-mount-helper` | Mounts and unmounts for the mount menus, run with sudo. |
| `/usr/libexec/hd-desktop/hd-reboot-notify`, `/usr/lib/systemd/user/hd-reboot-notify.*` | The notification of a pending reboot. |
| `/usr/libexec/hd-desktop/hd-shutdown` | Closes the applications before a reboot or power off. |
| `/etc/sudoers.d/hd-desktop` | Lets the group `sudo` mount without a password. |
| `/etc/dconf/profile/user`, `/etc/dconf/db/local.d/00-dark-theme`, `01-font` | The dark theme and the font for GTK applications. |
| `/etc/gtk-3.0/settings.ini`, `/etc/gtk-4.0/settings.ini` | The same, for GTK applications on XWayland. |
| `/usr/share/qt5ct/colors/hyprdebian.conf`, `/usr/share/qt6ct/colors/hyprdebian.conf` | The colours of Adwaita-dark for the Qt applications. |
| `/etc/skel/...` | The files for new users. |
