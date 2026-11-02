# Builds the HyprDebian kernel for KVM virtual machines: the kernel of hd-kernel,
# with the patches and the config of Debian and a prebuilt ZFS module, but with
# only the modules that keep-modules keeps: every module that is not a driver,
# and of the drivers only those of the hardware that QEMU emulates for a KVM vm
# and those of software, such as tun and device mapper. kernel.mk and
# build-kernel.sh of hd-kernel do the work, see there; the targets are those
# of the Makefile of hd-kernel, and make deb builds into build/<suite>:
#
#   linux-image-<release>_<version>_amd64.deb
#               the kernel and its modules
#   zfs-modules-<release>_<version>_amd64.deb
#               zfs.ko and spl.ko, in /lib/modules/<release>/extra/zfs
#   hd-kernel-vm_<version>_amd64.deb
#               depends on both, with the man page in man/; installing it
#               switches a vm to the kernel of HyprDebian for vms
#   linux-headers-<release>_<version>_amd64.deb
#               the headers of the kernel, for DKMS, which nothing depends
#               on
#
# with the control file in deb/. The release is <x.y.z>+debNN-hdvm<id>-amd64,
# <id> from the versions of linux, zfs-linux and hd-kernel-vm, the files that
# hd-kernel builds with, and the files in deb/ and man/ and keep-modules here.
# Raise version.json for every change of the packages.

PACKAGE := hd-kernel-vm
TAG := hdvm
KEEP := keep-modules

include ../hd-kernel/kernel.mk
