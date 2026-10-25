## Introduction
HyprDebian has its own live ISO. It is a small Debian trixie live system to install HyprDebian from, on the console or over ssh.

The live cd contains:
- A text console, no desktop. The live user `user` is logged in on the console automatically, and has sudo.
- The package `hd-installer`, built from this repository, and the tools it runs on the live system: debootstrap, dosfstools, gdisk, mdadm and parted.
- A local repository in `/srv/hyprdebian/repo` with every package built from this repository, in the suites of the `deploy.json` of its package directory. The kernels of [hd-kernel](../packages/hd-kernel.md), which `make all` leaves out and the installer does not install, are not in it. `hd-installer` installs the hd-* packages from it, so an installation needs no HyprDebian repository. The Debian packages still come from the Debian mirrors.
- ZFS support. The ZFS kernel module is built with DKMS while the ISO is built and is loaded at boot.
- An ssh server, started at boot, which allows logging in with a password, so the installation can also be done from another machine.
- NetworkManager. Wired networks come up by themselves, `nmtui` sets up wifi.
- gpm, so the mouse works on the console for copy and paste.
- The firmware of Debian's non-free-firmware area, for wifi and other hardware.

Recommended packages are left out to keep the ISO small, so the package lists name everything the live system needs.

## Building
The ISO is built with live-build, in a debian:trixie docker container, so live-build and its dependencies stay off the desktop system. From the root of the repository:
```
make iso
```

This builds every package into `build/` first, as `make all` does, then hands them to `livecd/Makefile` in `DEBS`, each with the suites of its `deploy.json`. `make -C livecd iso` alone stops with an error, as it has no packages.

The finished ISO is written to:
```
livecd/out/hyprdebian-live-amd64.iso
```

`make -C livecd image` only builds the docker image, and `make -C livecd clean` removes `livecd/out`.

### Downloading
The build server builds the ISO of `main` every night it has moved and puts it on the package repository, see [hd-buildserver](../packages/hd-buildserver.md). Download it from there with its checksum, and check it:
```
wget http://repo/iso/hyprdebian-live-amd64.iso http://repo/iso/SHA256SUMS
sha256sum -c SHA256SUMS
```

The files involved are:
```
livecd/Makefile
livecd/Dockerfile
livecd/build.sh
livecd/config/package-lists/*.list.chroot
livecd/config/hooks/live/*.hook.chroot
livecd/config/includes.chroot/
```

| File | Purpose |
| --- | --- |
| `Makefile` | Builds the docker image, then runs it with the repository read-only at `/src`, `livecd/out` at `/out` and the packages of `DEBS` in its environment. |
| `Dockerfile` | The debian:trixie image with live-build, and apt-utils for `apt-ftparchive`. Its entrypoint is `build.sh` in `/src`, so the image only has to be rebuilt when the Dockerfile changes. |
| `build.sh` | Runs `lb config`, puts `config/` on top, makes the boot menus start the live system after 5 seconds, puts `hd-installer` from `build/` in `packages.chroot` and every package of `DEBS` in the local repository, runs `lb build` and copies the ISO to `/out`. |
| `config/package-lists/` | The packages of the live system: `hyprdebian.list.chroot` for the installer and its tools, `zfs.list.chroot` for ZFS and `console.list.chroot` for the console, the network and ssh. |
| `config/hooks/live/0100-zfs-dkms.hook.chroot` | Builds the ZFS module for the kernel of the image. |
| `config/hooks/live/0200-enable-services.hook.chroot` | Enables gpm and the ssh server. |
| `config/includes.chroot/` | Files copied into the live system: `/etc/motd`, `/etc/modules-load.d/zfs.conf`, which loads ZFS at boot, and `/etc/ssh/sshd_config.d/hyprdebian-live.conf`, which allows ssh logins with a password. |

Notes:
1. The container needs `--privileged` because live-build mounts filesystems and creates device nodes in its chroot.
2. The whole build happens inside the container. Only the ISO is copied out, owned by the user that runs make. The container is removed afterwards.
3. Every build starts from scratch and downloads all packages again. A build takes a while, mainly because of the ZFS module.
4. `livecd/out` is in .gitignore.
5. The local repository has a `pool/<suite>` and a `dists/<suite>` for each suite, indexed with `apt-ftparchive`. Its `Release` files are not signed; the installer trusts the repository, as it comes with the installer. A package in two suites is stored once in the squashfs of the live system.

### Following the build
The container is named `hyprdebian-livecd-build`. A shell inside the running build can be opened with:
```
docker exec -it hyprdebian-livecd-build bash
```

The live-build working folder is `/build/live` inside the container.

### Expected warnings
mdadm shows "failed to load MD subsystem" and "failed to auto-generate mdadm.conf" during a successful build. There are no arrays to scan inside the build chroot.

## Using the live cd
Write the ISO to a USB stick, for example with:
```
sudo dd if=livecd/out/hyprdebian-live-amd64.iso of=/dev/sdX bs=4M status=progress oflag=sync
```

Replace /dev/sdX with the USB stick. Everything on it will be lost.

Boot from the USB stick. The live cd boots on both UEFI and BIOS machines. The boot menu starts the live system after 5 seconds, to a text console, logged in as the live user `user`. Its password is `live`.

### Installing
Run:
```
sudo hd-installer
```

See `man hd-installer` and [hd-installer](../packages/hd-installer.md).

The installer asks for no HyprDebian repository: it installs the hd-* packages from the local repository of the live cd. The installed system has no HyprDebian repository; to get updates of the hd-* packages, link it to one after the first boot, as root:
```
hd-link-repo <url> --suite <suite>
```

### Copy and paste on the console
gpm runs on the console, so the mouse works there:
- Select text with the left button. Double click selects a word, triple click a line.
- Paste with the middle button.

This is handy for disk wwn's: list them with `ls -l /dev/disk/by-id/`, select one and paste it where the installer asks for it.

### Installing over ssh
The ssh server is started at boot. Find the address of the live system with:
```
ip -br addr
```

Then log in from another machine with:
```
ssh user@<address>
```

The password is `live`. Wifi can be set up with `sudo nmtui`.

The ssh host keys of the live cd can change between boots and between ISO builds. ssh then warns about a changed host key. Remove the old key with `ssh-keygen -R <address>`.

## Notes
The admin tools such as mdadm, gdisk and zpool are in /usr/sbin. That folder is not in the PATH of the live user, so running them without sudo gives "command not found". Run them with sudo:
```
sudo mdadm --version
```
