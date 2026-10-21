## Introduction
The package `hd-base` depends on the packages that every hyprdebian system has, so that they are installed with it and stay installed. Besides the settings below, a package it pulls in is set up the way that package does it.

It installs `hd-up`, which upgrades the system after a ZFS snapshot, `hd-reboot` and `hd-poweroff`, which reboot and power off the system on a desktop and without one, `hd-issue`, which writes the login banner, `hd-link-repo`, which switches the HyprDebian repository and suite of apt, the files that new users get in their home directory, from `/etc/skel`, with `hd-sync-skel`, which brings them into an existing home directory, and a few settings of the system. The files of the package are in `etc/` and `usr/` in this repository, at the path they are installed to; those in `etc/` are conffiles.

It also holds the parts that every hyprdebian system shares, each with a page of its own:
- [hd-config](hd-config.md): the hyprdebian configuration in `/etc/hyprdebian/config.json`, where hyprdebian packages keep their settings, and that of each user in `~/.config/hyprdebian/config.json`, with `hd-config-update` and `hd-config-remove`;
- [hd-network](hd-network.md): the network configuration, rendered into the systemd-networkd files by `hd-network-render` and changed with `hd-network-iface` and `hd-network-bridge`, and the `.link` files that name the network interfaces by their path;
- [hd-secboot](hd-secboot.md): the Secure Boot keys of the owner, which the postinst generates in `/etc/hyprdebian/secboot` when they are not there yet, with `hd-secboot-enroll`, which enrolls them in the firmware, and `hd-secboot-check`, which checks that Secure Boot is on.

These were the packages `hd-config`, `hd-network` and `hd-secboot` before. `hd-base` conflicts with them, replaces and provides them, so an upgrade removes them; the configuration and the keys stay.

When a package becomes part of every hyprdebian system, it is added to the `Depends` of `hd-base`, to its man page and to this page.

The package comes with man pages: `man hd-base` describes the package, `man hd-up`, `man hd-reboot` (also `man hd-poweroff`), `man hd-sync-skel`, `man hd-issue` and `man hd-link-repo` the commands. The man pages of the shared parts are listed on their pages.

## Building the package
The package is built from `packages/hd-base` in this repository:
```
cd packages/hd-base
make deb
```

This builds `build/hd-base_<version>_all.deb`. Its version is the version of HyprDebian, from `version.json` in the root of this repository, which is raised for every change to a package or the installer; hd-base has no version of its own. Install it with:
```
sudo apt install ./build/hd-base_<version>_all.deb
```

The files involved are:
```
packages/hd-base/Makefile
version.json
packages/hd-base/hd-up
packages/hd-base/hd-reboot
packages/hd-base/hd-poweroff
packages/hd-base/hd-sync-skel
packages/hd-base/hd-issue
packages/hd-base/hd-link-repo
packages/hd-base/templates/hyprdebian.sources.j2
packages/hd-base/etc/skel/.bash_aliases
packages/hd-base/etc/skel/.gitconfig
packages/hd-base/etc/skel/.config/nano/nanorc
packages/hd-base/etc/profile.d/local-bin-path.sh
packages/hd-base/etc/apt/preferences.d/00-block-unwanted
packages/hd-base/etc/update-motd.d/00-hyprdebian
packages/hd-base/usr/lib/systemd/system/getty@.service.d/numlock.conf
packages/hd-base/usr/lib/systemd/zram-generator.conf.d/hyprdebian.conf
packages/hd-base/usr/lib/sysctl.d/50-zram.conf
packages/hd-base/usr/lib/systemd/resolved.conf.d/00-direct-uplink.conf
packages/hd-base/deb/postinst
packages/hd-base/deb/postrm
packages/hd-base/deb/triggers
packages/hd-base/deb/control
packages/hd-base/man/hd-base.7
packages/hd-base/man/hd-up.1
packages/hd-base/man/hd-reboot.1
packages/hd-base/man/hd-poweroff.1
packages/hd-base/man/hd-sync-skel.1
packages/hd-base/man/hd-issue.8
packages/hd-base/man/hd-link-repo.8
```

The files of the shared parts are listed on their pages: [hd-config](hd-config.md#building-the-package), [hd-network](hd-network.md#building-the-package) and [hd-secboot](hd-secboot.md#building-the-package).

`make test` runs the tests in `packages/hd-base/tests`, see [hd-secboot](hd-secboot.md#tests).

## Packages
| Group | Packages |
| --- | --- |
| hyprdebian | `python3` and `sudo`, for `hd-up`; `python3-jinja2`, for `hd-link-repo` and `hd-network-render`; `systemd`, for the network; `kbd`, for Num Lock on the console. |
| Secure Boot | `openssl`, `efitools` and `sbsigntool`, which make the keys, enroll them and sign EFI files with them; see [hd-secboot](hd-secboot.md). |
| Remote access and firewall | `openssh-server`, and `kitty-terminfo` so that the kitty terminal works over ssh; `ufw`, the firewall. |
| ZFS | `zfs-dkms`, `zfsutils-linux` and `zfs-initramfs`; `build-essential` and `initramfs-tools` to build the ZFS module and the initrd. |
| Firmware and boot | `firmware-linux` and `firmware-qlogic`; `efibootmgr` for the boot entries of the firmware. |
| Console | `locales`, `keyboard-configuration`, `console-setup` and `ca-certificates`. |
| System services | `systemd-resolved`, name resolution. The logs stay in the journal of systemd-journald; there is no syslog daemon. |
| Command line tools | `eza`, `fzf`, `nfs-common`, `psmisc`, `net-tools`, `pciutils`, `usbutils`, `acpi`, `bash-completion`, `git`, `git-delta`, `ack`, `command-not-found`, `man-db`, `apt-file` and `systemd-zram-generator`. It conflicts with `zram-tools`, which would set up a second zram swap. |

`firmware-linux` and `firmware-qlogic` are in the `non-free-firmware` component of Debian, which the apt sources must include.

## hd-up
```
hd-up
```

`hd-up` upgrades the system with apt, after a snapshot of the root dataset, so an upgrade that went wrong can be undone with `zfs rollback`:
1. It updates the package lists.
2. It shows the packages that can be upgraded, and stops when there are none.
3. It asks whether to go on; anything but `y` stops it.
4. It takes the snapshot `rpool/hyprdebian@hd-up-<date>_<time>`, and stops when that fails.
5. It runs `apt full-upgrade`, so upgrades that need new packages or remove old ones are not held back.

Run it as a user who may use sudo, or as root. To undo an upgrade:
```
zfs list -t snapshot rpool/hyprdebian
sudo zfs rollback rpool/hyprdebian@hd-up-2026-10-17_143000
```

## hd-reboot and hd-poweroff
```
hd-reboot
hd-poweroff
```

`hd-reboot` reboots the system and `hd-poweroff` powers it off. In a Hyprland session, when `hyprshutdown` is installed, they let it close the applications and show "Rebooting" or "Shutting down" first, then run `systemctl reboot` or `systemctl poweroff`. A session is recognized by `HYPRLAND_INSTANCE_SIGNATURE` in the environment, which Hyprland sets for the programs it starts.

Otherwise, as on the console, over ssh, or on a system without a desktop, they run `systemctl reboot` or `systemctl poweroff` right away. Whether the user may do that is up to polkit, as for `systemctl` itself: a user who is logged in on the console of the system may, a user over ssh is asked for the password of an administrator.

## hd-sync-skel
```
hd-sync-skel
```

`hd-sync-skel` compares the files in `/etc/skel` with those in the home directory of the user who runs it. A file of `/etc/skel` that is missing in the home directory is copied, with the folders it is in. For a file that differs, the user chooses: `k` keeps the own file, `t` takes the new version from `/etc/skel`, and `m` merges the two; a file that is not a text file cannot be merged. Files in the home directory that are not in `/etc/skel` are left alone.

The merge runs in meld in a graphical session, recognized by `WAYLAND_DISPLAY` or `DISPLAY`, when meld is installed: the new version is on the left and the own file on the right, save the right side to keep the merge. Otherwise it runs with `sdiff`, the new version on the left: `l` takes the new lines, `r` keeps the own lines, `e` edits them in `$EDITOR` (nano when it is not set), and `q` quits and leaves the file as it was.

Run it as the user whose home directory is to be updated; with `sudo` it updates the home directory of root.

## hd-issue
```
hd-issue
```

`hd-issue` writes the banner in `/etc/issue`, which the console shows above the login prompt:
```
HyprDebian <version> · <suite> · <console>
```

with the version of HyprDebian from `/etc/hyprdebian/version.json`, the suite of the HyprDebian packages from `/etc/apt/sources.list.d/hyprdebian.sources`, and the name of the console. `HyprDebian` is shown in brown. A value that cannot be found shows as `unknown`.

The postinst runs it on every installation and upgrade. Run it as root after changing the suite yourself:
```
sudo hd-issue
```

## hd-link-repo
```
hd-link-repo [<url>] [--suite trixie|unstable]
```

`hd-link-repo` links the system to a HyprDebian repository, the one apt gets the hd-* packages from, and to a suite of it. Without arguments it shows the current repository and suite, from `/etc/apt/sources.list.d/hyprdebian.sources`.

With a URL it switches to the repository there, and with `--suite` to another suite; what is left out stays as it is. A switch:
1. downloads the signing key from `<url>/hyprdebian-archive-keyring.pgp` to `/usr/share/keyrings/hyprdebian-archive-keyring.pgp`, and stops when that fails;
2. writes the apt source from the jinja2 template `/usr/share/hd-base/templates/hyprdebian.sources.j2`;
3. updates the package lists of that source alone, and puts the old source and key back when that fails;
4. updates all package lists, and writes the login banner again with `hd-issue`, which shows the suite.

It does not upgrade the packages; run `hd-up` for that. Run it as root to switch:
```
hd-link-repo
sudo hd-link-repo http://repo.example/ --suite unstable
```

## Skel files
New users get these files in their home directory, from `/etc/skel`. They are conffiles, so local changes are kept on upgrades.

| File | Content |
| --- | --- |
| `.bash_aliases` | `ls` and `ll` with eza, `y` for yazi, `wav2flac`, which converts the `.wav` files below the current folder to `.flac` with ffmpeg and deletes them, and a history search on Ctrl-R with fzf. |
| `.gitconfig` | delta as the pager and diff filter of git. |
| `.config/nano/nanorc` | Indents with four spaces, remembers the search history, and colors the interface and the syntax. |

## System settings
| File | What it does |
| --- | --- |
| `/etc/profile.d/local-bin-path.sh` | Adds `~/.local/bin` to the `PATH` of login shells, when that folder exists. |
| `/etc/apt/preferences.d/00-block-unwanted` | Keeps apt from installing `ifupdown`, as systemd-networkd runs the network, `os-prober` and `turnstile`. |
| `/usr/lib/systemd/system/getty@.service.d/numlock.conf` | Turns Num Lock on for a console before its login prompt shows, with `setleds`. When that fails, the login prompt still shows. |
| `/etc/update-motd.d/00-hyprdebian` | Shows the HyprDebian banner after a login on the console or over ssh, with the version of HyprDebian from `/etc/hyprdebian/version.json`, the suite of the HyprDebian packages and the Debian version, the kernel, the uptime, the load, the memory, the health of the ZFS pools and the RAID arrays, and the failed units. |
| `/usr/lib/systemd/zram-generator.conf.d/hyprdebian.conf` | A swap in compressed RAM, `/dev/zram0`, of a quarter of the RAM, compressed with zstd, with priority 16384, so it is used before the swap on disk. |
| `/usr/lib/sysctl.d/50-zram.conf` | `vm.swappiness` 180, as swapping to zram is cheaper than dropping the page cache, and `vm.page-cluster` 0, as reading ahead does not help with zram. |
| `/usr/lib/systemd/resolved.conf.d/00-direct-uplink.conf` | systemd-resolved passes the queries straight to the name servers of the network: no stub resolver on 127.0.0.53, no cache, no LLMNR, no multicast DNS and no fallback servers. |

The files below `/usr/lib` are the defaults of the package: to change one, add a file of the same name, or a later one, in the same folder below `/etc`.

The postinst of the package:
- masks `systemd-firstboot.service`, the setup of the first boot, and `rpcbind.socket` and `rpcbind.service`, which `nfs-common` pulls in but HyprDebian does not use. It does this on the first installation, and on an upgrade from a version before 0.2, so a unit that the admin unmasks again stays unmasked on upgrades. A purge unmasks them. An upgrade from a version before 1.1 unmasks `systemd-preset-all.service` and `systemd-machine-id-commit.service`, which those versions masked as well;
- on an upgrade from a version before 1.11, renames the apt source `/etc/apt/sources.list.d/hyprdebian.local.sources`, the name of those versions, to `hyprdebian.sources`, unless that one already exists;
- points `/etc/resolv.conf` at `/run/systemd/resolve/resolv.conf`, the name servers themselves, in place of `stub-resolv.conf`, as there is no stub resolver. It only replaces the link that systemd-resolved made. When that file is missing, because systemd-resolved does not run, as in the chroot of the installer, it is made with the name servers of the old link, so names keep resolving until systemd-resolved starts;
- diverts `/etc/issue` of base-files to `/etc/issue.debian` and runs `hd-issue`, see below, on every installation and upgrade. A removal of the package removes the banner of `hd-issue` and puts the one of base-files back;
- empties `/etc/motd`, on the first installation like the masks, and diverts `/etc/update-motd.d/10-uname` of base-files to `10-uname.base-files`, which run-parts skips, as the banner already shows the kernel. A removal of the package puts it back. base-files only writes `/etc/motd` when it is missing.

## Services
Services are enabled on their installation as on Debian: a package that comes with a service enables it, unless a preset in `/usr/lib/systemd/system-preset` or `/etc/systemd/system-preset` says otherwise, see `man systemd.preset`. To keep a service from being enabled when its package is installed, add `disable <unit>` to a preset file below `/etc/systemd/system-preset`.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/hd-up` | Upgrades the system after a ZFS snapshot. |
| `/usr/bin/hd-reboot`, `/usr/bin/hd-poweroff` | Reboot and power off the system. |
| `/usr/bin/hd-sync-skel` | Brings the files of `/etc/skel` into the home directory. |
| `/usr/sbin/hd-issue` | Writes the login banner in `/etc/issue`. |
| `/usr/sbin/hd-link-repo` | Switches the HyprDebian repository and suite of apt. |
| `/usr/share/hd-base/templates/hyprdebian.sources.j2` | The template of the apt source of `hd-link-repo`. |
| `/etc/hyprdebian/version.json` | The version of HyprDebian, `version.json` of the root of this repository, which is also the version of hd-base. The banners show it. A conffile. |
| `/etc/issue` | The login banner, written by `hd-issue`. The one of base-files is diverted to `/etc/issue.debian`. |
| `/etc/skel/.bash_aliases`, `/etc/skel/.gitconfig`, `/etc/skel/.config/nano/nanorc` | The files new users get, conffiles. |
| `/etc/profile.d/local-bin-path.sh` | Adds `~/.local/bin` to the `PATH`, a conffile. |
| `/etc/apt/preferences.d/00-block-unwanted` | The packages apt never installs, a conffile. |
| `/etc/update-motd.d/00-hyprdebian` | The banner after a login, a conffile. |
| `/usr/lib/systemd/system/getty@.service.d/numlock.conf` | Num Lock on the console. |
| `/usr/lib/systemd/zram-generator.conf.d/hyprdebian.conf` | The zram swap. |
| `/usr/lib/sysctl.d/50-zram.conf` | The sysctls for the zram swap. |
| `/usr/lib/systemd/resolved.conf.d/00-direct-uplink.conf` | The config of systemd-resolved. |
| `/usr/bin/hd-config-*`, `/etc/hyprdebian/config.json` | The hyprdebian configuration, see [hd-config](hd-config.md#files). |
| `/usr/sbin/hd-network-*`, `/usr/lib/hd-network`, `/usr/share/hd-network`, `/usr/lib/systemd/network/*.link` | The network configuration, see [hd-network](hd-network.md#files). |
| `/usr/sbin/hd-secboot-*`, `/etc/hyprdebian/secboot` | The Secure Boot keys, see [hd-secboot](hd-secboot.md#files). |
| `/usr/share/man/man7/hd-base.7.gz`, `/usr/share/man/man1/hd-up.1.gz`, `/usr/share/man/man1/hd-reboot.1.gz`, `/usr/share/man/man1/hd-poweroff.1.gz`, `/usr/share/man/man1/hd-sync-skel.1.gz`, `/usr/share/man/man8/hd-issue.8.gz`, `/usr/share/man/man8/hd-link-repo.8.gz` | The man pages. |
