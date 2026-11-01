# Builds the HyprDebian kernel for servers: the kernel of hd-kernel, with the
# patches and the config of Debian and a prebuilt ZFS module, but with only
# the modules that keep-modules keeps: every module that is not a driver, and
# of the drivers those of the hardware of servers, such as SAS and RAID cards,
# Fibre Channel, the common network cards and IPMI, the targets of iSCSI,
# Fibre Channel and NVMe over TCP and Fibre Channel, and those of software,
# such as tun and device mapper. kernel.mk and build-kernel.sh of hd-kernel do
# the work, see there; the targets are those of the Makefile of hd-kernel, and
# make deb builds into build/<suite>:
#
#   linux-image-<release>_<version>_amd64.deb
#               the kernel and its modules
#   zfs-modules-<release>_<version>_amd64.deb
#               zfs.ko and spl.ko, in /lib/modules/<release>/extra/zfs
#   hd-kernel-server_<version>_amd64.deb
#               depends on both, with the man page in man/; installing it
#               switches a server to the kernel of HyprDebian for servers
#
# with the control file in deb/. The release is <x.y.z>+debNN-hdserver<id>-amd64,
# <id> from the versions of linux, zfs-linux and hd-kernel-server, the files
# that hd-kernel builds with, and the files in deb/ and man/ and keep-modules
# here. Raise version.json for every change of the packages.

PACKAGE := hd-kernel-server
TAG := hdserver
KEEP := keep-modules

include ../hd-kernel/kernel.mk
