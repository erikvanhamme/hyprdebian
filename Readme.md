# HyprDebian
This repo holds the HyprDebian installation scripts.

HyprDebian is a Debian unstable based custom distro focussing on Hyprland.

It is intended to be lightweight.

This is my personal daily driver OS. I target this to have exactly one user, who will be extremely happy.

Feel free to experiment with it.

## Highlights

1.  Clean installation of Hyprland.
2.  No animations, eye candy or other unneeded crap.
3.  Support for wired networks and (optional) wifi networks.
4.  Uses yazi as file manager.
5.  No pointy-clicky GUI's for any configuration. Nano is your configuration tool.
6.  Automatic login to graphical session. See [hd-desktop](docs/packages/hd-desktop.md).
7.  Keyboard shortcuts for the commonly used tools.
8.  Wofi launcher for the other tools.
9.  Basic notification system using mako.
10. Optional printing support through cups. (Configured via cups web interface)
11. Some laptop friendly optimization.
12. Optional docker support. See [hd-docker](docs/packages/hd-docker.md).
13. Optional QEMU/KVM support.
14. Optional backup tool for offline hard drives and LTO tape. See [Backup tool](docs/Backup%20tool/Backup%20tool%20overview.md).
15. Optional status reports to a dashboard that shows all hyprdebian systems on one page. See [hd-dashboard](docs/packages/hd-dashboard.md) and [hd-report](docs/packages/hd-report.md).

## System overview

- **Base:** Debian unstable (sid), with extra packages from the local hyprdebian repository.
- **Storage:** ZFS on root with native encryption, on a single drive or mirrored over two drives. The installer takes an `@install` snapshot of the boot and root datasets.
- **Session:** systemd, with uwsm managing the Hyprland session and greetd for the login.
- **Desktop:** Hyprland, with kitty as terminal, wofi as launcher and mako for notifications.
- **Audio:** PipeWire, with wiremix as mixer.
- **Network:** systemd-networkd, with iwd for wifi.

See [About](docs/HyprDebian/About.md) for more.

## Installation

Please note:
1. The installer will not do any handholding. If it asks questions, you better have the right answers.
2. Near the end, it will complain that 'rpool' could not be exported. This is a known bug which is very hard to fix, see 'First boot' below for a workaround.

### From Debian trixie live ISO

To install you will need a Debian Trixie live ISO written to a USB stick to boot from.

After boot, open a terminal and run:

```
sudo apt install -y git
git clone https://github.com/erikvanhamme/hyprdebian.git
cd hyprdebian
sudo ./install.sh
```

### From the HyprDebian live ISO

The HyprDebian live ISO is a small Debian trixie live system with a text console, ZFS support, an ssh server that allows logging in with a password, and the package `hd-installer` installed.

Build it in a debian:trixie docker container with:

```
make -C livecd iso
```

The ISO is written to livecd/out/hyprdebian-live-amd64.iso. Write it to a USB stick, for example with:

```
sudo dd if=livecd/out/hyprdebian-live-amd64.iso of=/dev/sdX bs=4M status=progress oflag=sync
```

Replace /dev/sdX with the USB stick. Everything on it will be lost. The live cd boots on both UEFI and BIOS machines.

The live cd boots to a text console, logged in as the user `user`. You can also log in over ssh as `user` with the password `live`. Then run:

```
sudo hd-installer
```

See [Live cd](docs/Livecd/Overview.md) for more on building and using it.

### Questions and defaults

The installer first asks all its questions. Nothing is changed on the disks until all questions are answered.

The installer installs the base system, the same on every computer. For a desktop, install `hd-desktop` on it afterwards.

See [hd-installer](docs/packages/hd-installer.md#questions) for all the questions and their default answers.

### When the installer fails

The installer can be started again after a failure. Tasks that completed before are skipped, and the installation continues with the task that failed. Fix the cause and run `sudo ./install.sh` again.

The answers and progress are kept in `/tmp/hyprdebian`. To start a completely fresh installation, remove it:

```
sudo rm -rf /tmp/hyprdebian
```

Because it is in `/tmp`, it is also gone after a reboot of the live system.

## First boot

On first boot, you will be dropped in the busybox shell.

Grub will complain that 'rpool' could not be imported because it is in use by another system.

To correct this, on the busybox prompt, execute:

```
zpool import -f rpool
exit
```

You will then be prompted for the unlock passphrase as entered during installation.

Subsequent boots will be normal.

## Keyboard shortcuts

`SUPER + RETURN` opens a terminal and `SUPER + SPACE` opens the application launcher. See [Key Bindings](docs/HyprDebian/Key%20Bindings.md) for all the shortcuts.

## Documentation

The `docs` folder has more documentation:
- [HyprDebian](docs/HyprDebian/About.md): what HyprDebian is, and its [key bindings](docs/HyprDebian/Key%20Bindings.md).
- [Installer](docs/Installer/Structure.md): how the installer is built, for anyone who wants to change or extend it.
- [Live cd](docs/Livecd/Overview.md): building and using the hyprdebian live ISO.
- [VM provisioning](docs/VM/Provisioning.md): provisioning a vm on the local computer to install with the live cd.
- [Backup tool](docs/Backup%20tool/Backup%20tool%20overview.md): the hyprdebian backup tool.
- [Desktop](docs/packages/hd-desktop.md): the Hyprland desktop and the login with greetd.
- [Dashboard](docs/packages/hd-dashboard.md): the dashboard that shows the systems, and [hd-report](docs/packages/hd-report.md), which sends their status reports.
