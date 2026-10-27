## Introduction
The package `hd-base` depends on the packages that every hyprdebian system has, so that they are installed with it and stay installed. Besides the settings below, a package it pulls in is set up the way that package does it.

It installs `hd-up`, which upgrades the system after a ZFS snapshot, by hand and every week, `hd-reboot-check`, which tells whether a reboot is needed, as after a new kernel, `hd-reboot` and `hd-poweroff`, which reboot and power off the system on a desktop and without one, `hd-issue`, which writes the login banner, `hd-link-repo`, which switches the HyprDebian repository and suite of apt, `hd-firmware`, which finds and installs the firmware packages that the hardware needs, the files that new users get in their home directory, from `/etc/skel`, with `hd-sync-skel`, which brings them into an existing home directory, and a few settings of the system. The files of the package are in `etc/` and `usr/` in this repository, at the path they are installed to; those in `etc/` are conffiles.

It also holds the parts that every hyprdebian system shares, each with a page of its own:
- [hd-config](hd-config.md): the hyprdebian configuration in `/etc/hyprdebian/config.json`, where hyprdebian packages keep their settings, and that of each user in `~/.config/hyprdebian/config.json`, with `hd-config-update` and `hd-config-remove`;
- [hd-network](hd-network.md): the network configuration, rendered into the systemd-networkd files by `hd-network-render` and changed with `hd-network-iface` and `hd-network-bridge`, and the `.link` files that name the network interfaces by their path;
- [hd-secboot](hd-secboot.md): the Secure Boot keys of the owner, which the postinst generates in `/etc/hyprdebian/secboot` with `hd-secboot` when they are not there yet; `hd-secboot` also writes them in the firmware and turns Secure Boot on and off.

These were the packages `hd-config`, `hd-network` and `hd-secboot` before. `hd-base` conflicts with them, replaces and provides them, so an upgrade removes them; the configuration and the keys stay.

When a package becomes part of every hyprdebian system, it is added to the `Depends` of `hd-base`, to its man page and to this page.

The package comes with man pages: `man hd-base` describes the package, `man hd-up`, `man hd-reboot-check`, `man hd-reboot` (also `man hd-poweroff`), `man hd-sync-skel`, `man hd-issue`, `man hd-link-repo` and `man hd-firmware` the commands. The man pages of the shared parts are listed on their pages.

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
packages/hd-base/hd-reboot-check
packages/hd-base/hd-reboot
packages/hd-base/hd-poweroff
packages/hd-base/hd-sync-skel
packages/hd-base/hd-issue
packages/hd-base/hd-link-repo
packages/hd-base/hd-firmware
packages/hd-base/templates/hyprdebian.sources.j2
packages/hd-base/etc/skel/.bash_aliases
packages/hd-base/etc/skel/.config/nano/nanorc
packages/hd-base/etc/profile.d/local-bin-path.sh
packages/hd-base/etc/apt/preferences.d/00-block-unwanted
packages/hd-base/etc/apt/apt.conf.d/50hyprdebian-languages
packages/hd-base/etc/apt/apt.conf.d/50hyprdebian-no-recommends
packages/hd-base/etc/dpkg/dpkg.cfg.d/hyprdebian-excludes
packages/hd-base/etc/update-motd.d/00-hyprdebian
packages/hd-base/etc/kernel/postinst.d/zz-hd-reboot-check
packages/hd-base/usr/lib/systemd/system/getty@.service.d/numlock.conf
packages/hd-base/usr/lib/systemd/system/hd-zfs-scrub.service
packages/hd-base/usr/lib/systemd/system/hd-zfs-scrub.timer
packages/hd-base/usr/lib/systemd/system/hd-zfs-trim.service
packages/hd-base/usr/lib/systemd/system/hd-zfs-trim.timer
packages/hd-base/usr/lib/systemd/system/hd-up.service
packages/hd-base/usr/lib/systemd/system/hd-up.timer
packages/hd-base/usr/lib/systemd/zram-generator.conf.d/hyprdebian.conf
packages/hd-base/usr/lib/sysctl.d/50-zram.conf
packages/hd-base/usr/lib/systemd/resolved.conf.d/00-direct-uplink.conf
packages/hd-base/deb/postinst
packages/hd-base/deb/prerm
packages/hd-base/deb/postrm
packages/hd-base/deb/triggers
packages/hd-base/deb/control
packages/hd-base/man/hd-base.7
packages/hd-base/man/hd-up.1
packages/hd-base/man/hd-reboot-check.1
packages/hd-base/man/hd-reboot.1
packages/hd-base/man/hd-poweroff.1
packages/hd-base/man/hd-sync-skel.1
packages/hd-base/man/hd-issue.8
packages/hd-base/man/hd-link-repo.8
packages/hd-base/man/hd-firmware.8
packages/hd-base/tests/test_firmware.py
packages/hd-base/tests/test_issue.py
packages/hd-base/tests/test_reboot_check.py
packages/hd-base/tests/test_up.py
```

The files of the shared parts are listed on their pages: [hd-config](hd-config.md#building-the-package), [hd-network](hd-network.md#building-the-package) and [hd-secboot](hd-secboot.md#building-the-package).

`make test` runs the tests in `packages/hd-base/tests`, see [Firmware](#firmware), [hd-up](#hd-up), [hd-reboot-check](#hd-reboot-check), [hd-issue](#hd-issue) and [hd-secboot](hd-secboot.md#tests).

## Packages
The installer starts from the minimal base of Debian, the `minbase` variant of debootstrap, so these are all the packages besides the required ones of Debian.

| Group | Packages |
| --- | --- |
| hyprdebian | `python3` and `sudo`, for `hd-up`; `python3-jinja2`, for `hd-link-repo` and `hd-network-render`; `kbd`, for Num Lock on the console; `kmod`, for `hd-firmware`. |
| Init | `systemd`, also for the network, `systemd-sysv` and `udev`. |
| Secure Boot | `openssl`, `efitools` and `sbsigntool`, which make the keys, enroll them and sign EFI files with them; see [hd-secboot](hd-secboot.md). |
| Remote access and firewall | `openssh-server` and `openssh-client`, the ssh server and client, which `openssh-server` no longer pulls in; `kitty-terminfo` so that the kitty terminal works over ssh; `ufw`, the firewall. |
| Network | `iproute2` and `iputils-ping`, with `linux-sysctl-defaults`, which lets every user ping; `netbase`. |
| ZFS | `zfs-dkms`, `zfsutils-linux` and `zfs-initramfs`; `zfs-zed`, the event daemon that reports pool errors, resilvers and scrubs; `initramfs-tools` for the initrd, and `zstd` to compress it. DKMS builds the ZFS module with the compiler that the kernel headers bring along; there is no `build-essential`. |
| Boot | `efibootmgr` for the boot entries of the firmware. |
| Console | `locales`, `keyboard-configuration`, `console-setup` and `ca-certificates`; `whiptail`, which shows the debconf questions in dialogs. |
| System services | `systemd-resolved`, name resolution; `libpam-systemd` and `libnss-systemd`, which register every login as a session of systemd-logind, with its runtime directory and the user instance of systemd. The logs stay in the journal of systemd-journald; there is no syslog daemon. |
| Command line tools | `nano`, `less`, `procps`, `eza`, `fzf`, `psmisc`, `bash-completion`, `command-not-found`, `man-db`, `apt-file` and `systemd-zram-generator`. `command-not-found` names the package of a command that is not installed, from the Contents indexes that `apt-file` downloads, so both stay. It conflicts with `zram-tools`, which would set up a second zram swap. |

The firmware is not among them: each system needs other firmware, which `hd-firmware` finds from its hardware and installs, see [Firmware](#firmware).

Versions before 1.26 also depended on `build-essential`, `git`, `git-delta`, `nfs-common`, `net-tools`, `pciutils`, `usbutils`, `acpi` and `ack`. After an upgrade `apt autoremove` removes them, and the postinst says so; mark the ones that are still used as installed by hand first, such as `sudo apt-mark manual nfs-common` on a system that mounts NFS shares.

## Firmware
`hd-firmware` lists the firmware packages that the hardware needs; `hd-firmware -v` also lists the drivers or the CPU that need each one, and `hd-firmware --install` installs them and marks them as installed by hand, so `apt autoremove` keeps them. The installer runs `hd-firmware --install` in the new system, which sees the devices of the live system through `/sys`, so a new system only gets the firmware of its own hardware. Run it again after adding hardware, such as a graphics card or a wifi adapter.

It finds the firmware in three steps:
1. The drivers: the kernel modules that match the `modalias` of each device in `/sys/bus`, with the modules they depend on (`modprobe --show-depends`), and the modules that are loaded.
2. The firmware files that those modules declare (`modinfo -F firmware`).
3. The packages that ship those files, from the Contents indexes of apt, which `apt-file update` downloads. They are read with `apt-get indextargets` and `apt-helper cat-file`.

The microcode comes from the vendor of the CPU, `intel-microcode` or `amd64-microcode`. A virtual machine gets neither, as the host loads the microcode, also not when a driver declares firmware in them: `kvm_amd`, which matches a CPU with AMD-V, depends on `ccp`, whose SEV firmware is in `amd64-microcode` but only works on the security processor of a real host. By default it looks at the modules of the running kernel, or of the newest installed kernel when those of the running one are not installed, as in the installer; `--kernel` picks another.

A module declares all the firmware it can load, not only that of the device at hand, so some packages are listed that the hardware does not use: `btusb` brings the firmware of the Intel, MediaTek and Realtek Bluetooth adapters, for instance. Firmware that a device keeps in its own flash, as many server network cards do, needs no package. Most firmware is in the `non-free-firmware` component of Debian, which the apt sources must include.

Versions of `hd-base` before 1.23 depended on `firmware-linux` and `firmware-qlogic`. After an upgrade those are left for `apt autoremove`, and the postinst says so. Keep the firmware that the hardware needs first:
```
sudo apt-file update
sudo hd-firmware --install
sudo apt autoremove
```

The tests in `packages/hd-base/tests/test_firmware.py` cover the parts that read the output of `modprobe`, the Contents indexes and `/proc/cpuinfo`, and the microcode packages, which a virtual machine never gets.

## hd-up
```
hd-up [--unattended]
```

`hd-up` upgrades the system with apt, after a snapshot of the root dataset, so an upgrade that went wrong can be undone with `zfs rollback`:
1. It updates the package lists.
2. It shows the packages that can be upgraded and those that the upgrade removes, and stops when there are none.
3. It asks whether to go on; anything but `y` stops it.
4. It downloads the packages, so a download that fails leaves no snapshot behind.
5. It takes the snapshot `rpool/hyprdebian@hd-up-<date>_<time>`, with the property `org.hyprdebian:hd-up` set to `manual` or `unattended`, and stops when that fails.
6. It runs `apt full-upgrade`, so upgrades that need new packages or remove old ones are not held back.
7. After an upgrade that succeeded, it destroys the oldest `hd-up-` snapshots, so only the newest 8 are kept. Other snapshots, such as `@install`, are left alone.
8. It runs `hd-reboot-check --write`, which writes `/run/reboot-required` when the new kernel or ZFS module needs a reboot, and says why; also when everything was current.

Run it as a user who may use sudo, or as root; as a user it runs itself again with sudo. Only one `hd-up` runs at a time. To undo an upgrade:
```
zfs list -t snapshot rpool/hyprdebian
sudo zfs rollback rpool/hyprdebian@hd-up-2026-10-17_143000
```

To keep another number of snapshots, set `keep` under the key `hd-up` of `/etc/hyprdebian/config.json`:
```
sudo hd-config-update hd-up '{"keep": 12}'
```

### Unattended upgrades
`hd-up.timer` runs `hd-up.service` every Tuesday at 04:30, or after the next boot when the system was off then. The service runs `hd-up --unattended`, which does not ask and keeps the configuration files that were changed, with the same snapshot as a run by hand. It upgrades nothing when the system runs on battery power, when the pool `rpool` is not `ONLINE`, such as with a missing drive, when `dpkg --audit` shows packages that are not fully installed, or when the upgrade removes packages; run `hd-up` by hand for such an upgrade, which shows them first. In all but the first case the service fails, so the failed unit shows after a login and on the dashboard. It never reboots; see [hd-reboot-check](#hd-reboot-check).

```
journalctl -u hd-up
sudo systemctl disable --now hd-up.timer
sudo systemctl edit hd-up.timer
```

The tests in `packages/hd-base/tests/test_up.py` cover the parts that read the simulation of `apt-get`, the setting `keep` and the list of snapshots.

## hd-reboot-check
```
hd-reboot-check [--write]
```

`hd-reboot-check` prints why the system needs a reboot, one reason per line, or nothing: the newest kernel in `/boot` is not the one running, or the ZFS module for the running kernel is not the one loaded, as after an upgrade of `zfs-dkms`. The newest kernel is the one whose `vmlinuz` was modified last, as [hd-bootloader](hd-bootloader.md) picks it: dpkg keeps the time of the build, and the releases of [hd-kernel](hd-kernel.md) end in a hash, by which their order cannot be told. When neither is the case but `/run/reboot-required` exists, where Debian packages ask for a reboot, the packages in `/run/reboot-required.pkgs` are the reason. With `--write`, as root, it also writes those two files when a reboot is needed; they are in `/run`, so they are gone after the reboot.

`hd-up` runs it with `--write` after every run, and so does the kernel hook `/etc/kernel/postinst.d/zz-hd-reboot-check` after a kernel is installed, outside a chroot. Nothing reboots the system on its own; until the reboot:
- the message after a login has a line `Reboot needed:` with the reasons;
- `hd-report` sends that a reboot is due to the dashboard;
- hd-desktop shows a notification in the desktop session, see [hd-desktop](hd-desktop.md).

The tests in `packages/hd-base/tests/test_reboot_check.py` cover the parts that compare the kernels and the ZFS modules and read `/run/reboot-required.pkgs`.

## hd-reboot and hd-poweroff
```
hd-reboot
hd-poweroff
```

`hd-reboot` reboots the system and `hd-poweroff` powers it off. In a Hyprland session, when [hd-desktop](hd-desktop.md#reboot-and-power-off) is installed, they let its `/usr/libexec/hd-desktop/hd-shutdown` close the applications first, which asks with `hd-message` what to do about those that do not close, then runs `systemctl reboot` or `systemctl poweroff`. A session is recognized by `HYPRLAND_INSTANCE_SIGNATURE` in the environment, which Hyprland sets for the programs it starts.

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

with the version of HyprDebian from `/etc/hyprdebian/version.json`, the suite of the HyprDebian packages from `/etc/apt/sources.list.d/hyprdebian.sources`, and the name of the console. A system that is not linked to a HyprDebian repository, as after an installation, has no such source; then the suite is the first one of `/etc/apt/sources.list.d/debian.sources`. The message after a login finds the suite in the same way. `HyprDebian` is shown in brown. The banner also shows the cursor of the console, with the escape sequence `\e[?25h`, which agetty prints: the kernel command line of [hd-bootloader](hd-bootloader.md) hides it for the boot splash with `vt.global_cursor_default=0`, and getty resets the console before agetty runs, which hides it again, so only agetty can show it. A value that cannot be found shows as `unknown`. The tests in `packages/hd-base/tests/test_issue.py` cover how the suite is found.

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
New users get these files in their home directory, from `/etc/skel`. They are conffiles, so local changes are kept on upgrades. Versions before 1.26 also had `.gitconfig`, with delta for git.

| File | Content |
| --- | --- |
| `.bash_aliases` | `ls` and `ll` with eza, `y` for yazi, `wav2flac`, which converts the `.wav` files below the current folder to `.flac` with ffmpeg and deletes them, and a history search on Ctrl-R with fzf. |
| `.config/nano/nanorc` | Indents with four spaces, remembers the search history, and colors the interface and the syntax. |

## System settings
| File | What it does |
| --- | --- |
| `/etc/profile.d/local-bin-path.sh` | Adds `~/.local/bin` to the `PATH` of login shells, when that folder exists. |
| `/etc/apt/preferences.d/00-block-unwanted` | Keeps apt from installing `ifupdown`, as systemd-networkd runs the network, `os-prober` and `turnstile`. |
| `/etc/apt/apt.conf.d/50hyprdebian-languages` | Keeps apt from downloading the translations of the package descriptions, some 40 MB in `/var/lib/apt/lists`. That includes the English one, so `apt show` has no long descriptions. |
| `/etc/apt/apt.conf.d/50hyprdebian-no-recommends` | Keeps apt from installing the recommends of a package, so the system only gets what its packages depend on. The hd-* packages depend on everything they need. |
| `/etc/dpkg/dpkg.cfg.d/hyprdebian-excludes` | Has dpkg leave out of every package the documentation in `/usr/share/doc` but the copyright files, the translations in `/usr/share/locale`, the man pages in other languages than English, and the info pages. |
| `/usr/lib/systemd/system/hd-up.timer` | Runs `hd-up --unattended` with `hd-up.service` every Tuesday at 04:30, see [hd-up](#hd-up). |
| `/usr/lib/systemd/system/hd-zfs-scrub.timer`, `hd-zfs-trim.timer` | Scrub the ZFS pools on the second Sunday of the month and trim them on the first, with `/usr/lib/zfs-linux/scrub` and `trim` of zfsutils-linux, as its cron job does on a system with cron, which HyprDebian does not have. A pool is left out when its property `org.debian:periodic-scrub` or `org.debian:periodic-trim` is `disable`; at the default, `auto`, only a pool that is only on NVMe disks is trimmed. |
| `/usr/lib/systemd/system/getty@.service.d/numlock.conf` | Turns Num Lock on for a console before its login prompt shows, with `setleds`. When that fails, the login prompt still shows. |
| `/etc/update-motd.d/00-hyprdebian` | Shows the HyprDebian banner after a login on the console or over ssh, with the version of HyprDebian from `/etc/hyprdebian/version.json`, the suite of the HyprDebian packages and the Debian version, the kernel, the uptime, the load, the memory, the health of the ZFS pools and the RAID arrays, the failed units, and why a reboot is needed, as `hd-reboot-check` finds it. |
| `/etc/kernel/postinst.d/zz-hd-reboot-check` | Runs `hd-reboot-check --write` after a kernel is installed, so `/run/reboot-required` says that a reboot is due also when the kernel did not come with `hd-up`. Not in a chroot, such as that of the installer. |
| `/usr/lib/systemd/zram-generator.conf.d/hyprdebian.conf` | A swap in compressed RAM, `/dev/zram0`, of a quarter of the RAM, compressed with zstd, with priority 16384, so it is used before the swap on disk. |
| `/usr/lib/sysctl.d/50-zram.conf` | `vm.swappiness` 180, as swapping to zram is cheaper than dropping the page cache, and `vm.page-cluster` 0, as reading ahead does not help with zram. |
| `/usr/lib/systemd/resolved.conf.d/00-direct-uplink.conf` | systemd-resolved passes the queries straight to the name servers of the network: no stub resolver on 127.0.0.53, no cache, no LLMNR, no multicast DNS and no fallback servers. |

The files below `/usr/lib` are the defaults of the package: to change one, add a file of the same name, or a later one, in the same folder below `/etc`.

The postinst of the package:
- masks `systemd-firstboot.service`, the setup of the first boot, and `rpcbind.socket` and `rpcbind.service`, which `nfs-common` pulls in when it is installed, but which NFS version 4 does not need. It does this on the first installation, and on an upgrade from a version before 0.2, so a unit that the admin unmasks again stays unmasked on upgrades. A purge unmasks them. An upgrade from a version before 1.1 unmasks `systemd-preset-all.service` and `systemd-machine-id-commit.service`, which those versions masked as well;
- on an upgrade from a version before 1.11, renames the apt source `/etc/apt/sources.list.d/hyprdebian.local.sources`, the name of those versions, to `hyprdebian.sources`, unless that one already exists;
- points `/etc/resolv.conf` at `/run/systemd/resolve/resolv.conf`, the name servers themselves, in place of `stub-resolv.conf`, as there is no stub resolver. It only replaces the link that systemd-resolved made. When that file is missing, because systemd-resolved does not run, as in the chroot of the installer, it is made with the name servers of the old link, so names keep resolving until systemd-resolved starts;
- diverts `/etc/issue` of base-files to `/etc/issue.debian` and runs `hd-issue`, see below, on every installation and upgrade. A removal of the package removes the banner of `hd-issue` and puts the one of base-files back;
- removes the files that `hyprdebian-excludes` leaves out from the packages that were installed before hd-base, on the first installation and on an upgrade from a version before 1.26: dpkg only applies the excludes to the packages it installs after. It reads the rules of that file, and the last rule that matches a path decides, as for dpkg. Directories that are left empty go too;
- enables `hd-zfs-scrub.timer`, `hd-zfs-trim.timer` and `hd-up.timer` on their first installation, by their preset, with the steps debhelper adds for a timer, and keeps the state the admin gave them over upgrades. The prerm stops them on a removal, but leaves a running `hd-up.service` alone, as the removal may be part of its upgrade, and the postrm masks them, so a reinstall keeps their state; a purge forgets it;
- empties `/etc/motd`, on the first installation like the masks, and diverts `/etc/update-motd.d/10-uname` of base-files to `10-uname.base-files`, which run-parts skips, as the banner already shows the kernel. A removal of the package puts it back. base-files only writes `/etc/motd` when it is missing.

## Services
Services are enabled on their installation as on Debian: a package that comes with a service enables it, unless a preset in `/usr/lib/systemd/system-preset` or `/etc/systemd/system-preset` says otherwise, see `man systemd.preset`. To keep a service from being enabled when its package is installed, add `disable <unit>` to a preset file below `/etc/systemd/system-preset`.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/hd-up` | Upgrades the system after a ZFS snapshot. |
| `/usr/bin/hd-reboot-check` | Tells whether a reboot is needed. |
| `/usr/bin/hd-reboot`, `/usr/bin/hd-poweroff` | Reboot and power off the system. |
| `/usr/bin/hd-sync-skel` | Brings the files of `/etc/skel` into the home directory. |
| `/usr/sbin/hd-issue` | Writes the login banner in `/etc/issue`. |
| `/usr/sbin/hd-link-repo` | Switches the HyprDebian repository and suite of apt. |
| `/usr/sbin/hd-firmware` | Finds and installs the firmware packages that the hardware needs. |
| `/usr/share/hd-base/templates/hyprdebian.sources.j2` | The template of the apt source of `hd-link-repo`. |
| `/etc/hyprdebian/version.json` | The version of HyprDebian, `version.json` of the root of this repository, which is also the version of hd-base. The banners show it. A conffile. |
| `/etc/issue` | The login banner, written by `hd-issue`. The one of base-files is diverted to `/etc/issue.debian`. |
| `/etc/skel/.bash_aliases`, `/etc/skel/.config/nano/nanorc` | The files new users get, conffiles. |
| `/etc/profile.d/local-bin-path.sh` | Adds `~/.local/bin` to the `PATH`, a conffile. |
| `/etc/apt/preferences.d/00-block-unwanted` | The packages apt never installs, a conffile. |
| `/etc/apt/apt.conf.d/50hyprdebian-languages` | No translations of the package descriptions, a conffile. |
| `/etc/apt/apt.conf.d/50hyprdebian-no-recommends` | No recommends, a conffile. |
| `/etc/dpkg/dpkg.cfg.d/hyprdebian-excludes` | The files dpkg leaves out, a conffile. |
| `/usr/lib/systemd/system/hd-zfs-scrub.*`, `hd-zfs-trim.*` | The monthly scrub and trim of the ZFS pools. |
| `/usr/lib/systemd/system/hd-up.*` | The weekly unattended upgrade. |
| `/etc/update-motd.d/00-hyprdebian` | The banner after a login, a conffile. |
| `/etc/kernel/postinst.d/zz-hd-reboot-check` | Writes `/run/reboot-required` after a kernel is installed, a conffile. |
| `/usr/lib/systemd/system/getty@.service.d/numlock.conf` | Num Lock on the console. |
| `/usr/lib/systemd/zram-generator.conf.d/hyprdebian.conf` | The zram swap. |
| `/usr/lib/sysctl.d/50-zram.conf` | The sysctls for the zram swap. |
| `/usr/lib/systemd/resolved.conf.d/00-direct-uplink.conf` | The config of systemd-resolved. |
| `/usr/bin/hd-config-*`, `/etc/hyprdebian/config.json` | The hyprdebian configuration, see [hd-config](hd-config.md#files). |
| `/usr/sbin/hd-network-*`, `/usr/lib/hd-network`, `/usr/share/hd-network`, `/usr/lib/systemd/network/*.link` | The network configuration, see [hd-network](hd-network.md#files). |
| `/usr/sbin/hd-secboot`, `/etc/hyprdebian/secboot` | The Secure Boot keys, see [hd-secboot](hd-secboot.md#files). |
| `/usr/share/man/man7/hd-base.7.gz`, `/usr/share/man/man1/hd-up.1.gz`, `/usr/share/man/man1/hd-reboot-check.1.gz`, `/usr/share/man/man1/hd-reboot.1.gz`, `/usr/share/man/man1/hd-poweroff.1.gz`, `/usr/share/man/man1/hd-sync-skel.1.gz`, `/usr/share/man/man8/hd-issue.8.gz`, `/usr/share/man/man8/hd-link-repo.8.gz`, `/usr/share/man/man8/hd-firmware.8.gz` | The man pages. |
