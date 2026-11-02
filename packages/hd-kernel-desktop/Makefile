# Builds the HyprDebian kernel for desktops and laptops: the kernel of
# hd-kernel, with the patches and the config of Debian and a prebuilt ZFS
# module, but with only the modules that keep-modules keeps: every module that
# is not a driver, and of the drivers those of the hardware of desktops and
# laptops, such as the graphics of AMD, Intel and Nvidia, sound, Wi-Fi,
# Bluetooth, the common network cards and the platform drivers of laptops,
# every driver of USB, HID and input for what people plug in, and those of
# software, such as tun and device mapper. kernel.mk and build-kernel.sh of
# hd-kernel do the work, see there; the targets are those of the Makefile of
# hd-kernel, and make deb builds into build/<suite>:
#
#   linux-image-<release>_<version>_amd64.deb
#               the kernel and its modules
#   zfs-modules-<release>_<version>_amd64.deb
#               zfs.ko and spl.ko, in /lib/modules/<release>/extra/zfs
#   hd-kernel-desktop_<version>_amd64.deb
#               depends on both, with the man page in man/; installing it
#               switches a desktop or laptop to the kernel of HyprDebian for
#               desktops
#   linux-headers-<release>_<version>_amd64.deb
#               the headers of the kernel, for DKMS, which nothing depends
#               on
#
# with the control file in deb/. The release is
# <x.y.z>+debNN-hddesktop<id>-amd64, <id> from the versions of linux,
# zfs-linux and hd-kernel-desktop, the files that hd-kernel builds with, and
# the files in deb/ and man/ and keep-modules here. Raise version.json for
# every change of the packages.

PACKAGE := hd-kernel-desktop
TAG := hddesktop
KEEP := keep-modules

include ../hd-kernel/kernel.mk
