## Introduction
The meta package `hd-base` depends on the packages that every hyprdebian system has, so that they are installed with it and stay installed. It installs no files of its own besides its man page, and configures nothing: a package it pulls in is set up the way that package does it.

When a package becomes part of every hyprdebian system, it is added to the `Depends` of `hd-base`, to its man page and to this page.

The package comes with a man page: `man hd-base`.

## Building the package
The package is built from `hd-base` in this repository:
```
cd hd-base
make deb
```

This builds `build/hd-base_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. Install it with:
```
sudo apt install ./build/hd-base_<version>_all.deb
```

The files involved are:
```
hd-base/Makefile
hd-base/version.json
hd-base/deb/control
hd-base/man/hd-base.7
```

## Packages
| Group | Packages |
| --- | --- |
| hyprdebian | `hd-network`, the network configuration; see [hd-network](hd-network.md). |
| Remote access and firewall | `openssh-server`, and `kitty-terminfo` so that the kitty terminal works over ssh; `ufw`, the firewall. |
| ZFS | `zfs-dkms`, `zfsutils-linux` and `zfs-initramfs`; `build-essential` and `initramfs-tools` to build the ZFS module and the initrd. |
| Firmware and boot | `firmware-linux` and `firmware-qlogic`; `efibootmgr` for the boot entries of the firmware. |
| Console | `locales`, `keyboard-configuration`, `console-setup` and `ca-certificates`. |
| System services | `systemd-resolved`, name resolution, and `rsyslog`, logging. |
| Command line tools | `eza`, `fzf`, `nfs-common`, `psmisc`, `net-tools`, `pciutils`, `usbutils`, `acpi`, `bash-completion`, `git`, `git-delta`, `ack`, `command-not-found`, `man-db`, `apt-file` and `zram-tools`. |

`firmware-linux` and `firmware-qlogic` are in the `non-free-firmware` component of Debian, which the apt sources must include.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/share/man/man7/hd-base.7.gz` | The man page. |
