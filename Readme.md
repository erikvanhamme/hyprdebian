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
6.  Automatic login to graphical session.
7.  Keyboard shortcuts for the commonly used tools.
8.  Wofi launcher for the other tools.
9.  Basic notification system using mako.
10. Optional printing support through cups. (Configured via cups web interface)
11. Some laptop friendly optimization.
12. Optional docker support.
13. Optional QEMU/KVM support.
14. Optional backup tool for offline hard drives and LTO tape. See [Backup tool](docs/Backup%20tool/Backup%20tool%20overview.md).

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

### From hyprdebian live ISO

The hyprdebian live ISO is a Debian trixie live system with a text console, ZFS support, an ssh server, the tools the installer needs and a checkout of this repo. The installer skips its prerequisites phase when it runs on it.

To install you will need to create the live ISO by running the script in the livecd folder.

To build it in a docker container instead, so the live-build dependencies stay off your system:

```
docker build -t hyprdebian-livecd livecd/
mkdir -p livecd/out
docker run --rm --privileged -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" -v "$PWD/livecd/out:/out" hyprdebian-livecd
```

The ISO is written to livecd/out/hyprdebian-live-amd64.iso.

When the ISO is built, write it to a USB stick, for example with:

```
sudo dd if=livecd/out/hyprdebian-live-amd64.iso of=/dev/sdX bs=4M status=progress oflag=sync
```

Replace /dev/sdX with the USB stick. Everything on it will be lost. The live cd boots on both UEFI and BIOS machines.

The live cd boots to a text console, logged in as the live user. The mouse works on the console: select text with the left button and paste it with the middle button. You can also log in over ssh as user `user` with password `live`.

Then run:

```
cd hyprdebian
git pull
sudo ./install.sh
```

The repo on the live cd is the version from the moment the ISO was built. The `git pull` brings it up to date.

The admin tools such as mdadm, gdisk and zpool are not in the PATH of the live user. Run them with sudo.

See [Live cd](docs/Livecd/Overview.md) for building without docker, following the build and the warnings you can ignore.

### Questions and defaults

The installer first asks all its questions. Nothing is changed on the disks until all questions are answered.

One of the first questions is the machine type: `desktop`, `laptop`, `server` or `vm`. Each machine type has its own default answers, for example wifi on a laptop and redundant drives on a server. When you choose to use the machine type defaults, every question that has a default is answered with it without asking. The disks, the disk destruction confirmation and the password are always asked.

See [Defaults](docs/Installer/Defaults.md) for all the default answers.

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

## Upgrading

An installed system can be upgraded to a newer version of hyprdebian. The installer puts a checkout of this repo in `/usr/src/hyprdebian`, owned by your user, at the version that installed the system. Get the latest version and run the upgrade:

```
cd /usr/src/hyprdebian
git pull
sudo ./upgrade.sh
```

A system installed by an older version has no checkout in `/usr/src/hyprdebian` yet. Clone this repo anywhere and run the upgrade from there once. The upgrade then clones it into `/usr/src/hyprdebian`:

```
git clone https://github.com/erikvanhamme/hyprdebian.git
cd hyprdebian
sudo ./upgrade.sh
```

The upgrade uses the answers given during the installation, and only asks the questions that were added since. It installs new packages, enables new services and updates the hyprdebian scripts and config files, including the ones in your home directory. When a config file was changed on your system, it asks whether to keep yours, take the new version or merge the two, in meld from the desktop or in sdiff from a console.

Packages that are already installed are not upgraded. Use `apt` for that, as usual.

Before it starts, the upgrade checks that the system is up to date, and recommends running `sudo apt full-upgrade` first when it is not.

Systems installed before version v0.152 have no saved answers. The upgrade then works out the answers from the system, and asks every question once with the detected answer filled in. See [Upgrade](docs/Installer/Upgrade.md) for how the upgrade works.

## Keyboard shortcuts

`SUPER + RETURN` opens a terminal and `SUPER + SPACE` opens the application launcher. See [Key Bindings](docs/HyprDebian/Key%20Bindings.md) for all the shortcuts.

## Documentation

The `docs` folder has more documentation:
- [HyprDebian](docs/HyprDebian/About.md): what HyprDebian is, and its [key bindings](docs/HyprDebian/Key%20Bindings.md).
- [Installer](docs/Installer/Structure.md): how the installer is built, for anyone who wants to change or extend it, and how the [upgrade](docs/Installer/Upgrade.md) works.
- [Live cd](docs/Livecd/Overview.md): building and using the hyprdebian live ISO.
- [Backup tool](docs/Backup%20tool/Backup%20tool%20overview.md): the hyprdebian backup tool.
