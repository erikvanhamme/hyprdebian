# HyprDebian
This repo holds the HyprDebian installation scripts.

HyprDebian is a Debian unstable based custom distro focussing on Hyprland.

It is intended to be lightweight.

This is my personal daily driver OS. I target this to have exactly one user, who will be extremely happy.

Feel free to experiment with it.

## Highlights

1. Clean installation of Hyprland.
2. No animations, eye candy or other unneeded crap.
3. Support for wired networks only (WIFI support is planned).
4. Uses yazi as file manager.
5. No pointy-clicky GUI's for any configuration. Nano is your configuration tool.
6. Automatic login to graphical session.
7. Keyboard shortcuts for the commonly used tools.
8. Wofi launcher for the other tools.
9. Basic notification system using mako.

## Installation

To install you will need a Debian Trixie live ISO written to a USB stick to boot from.

After boot, open a terminal and run:

```
sudo apt install -y git
git clone https://github.com/erikvanhamme/hyprdebian.git
ch hyprdebian
script -c ./install.sh install.log
```

Please note:
1. The installer will not do any handholding. If it asks questions, you better have the right answers.
2. Near the end, it will complain that 'rpool' could not be exported. This is a known bug which is very hard to fix, see 'First boot' below for a workaround.

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

SUPER + ENTER: terminal
SUPER + B: firefox

