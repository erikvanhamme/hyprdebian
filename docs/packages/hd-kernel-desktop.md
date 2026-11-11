## Introduction
The package `hd-kernel-desktop` switches a HyprDebian desktop or laptop to the HyprDebian kernel for desktops: the kernel of [hd-kernel](hd-kernel.md), with the patches and the config of Debian and the ZFS module built against it on the build server, but with only the modules that a desktop or laptop needs. It is built, signed and installed like the kernel of `hd-kernel`, which describes how.

## What it keeps
What the config of Debian builds into the kernel stays, and so do the modules that are not drivers: the file systems, the network protocols and netfilter, with wireless and Bluetooth, crypto, and `kvm`, for virtual machines. Of the drivers only these stay:
- those of the hardware of desktops and laptops: the graphics of AMD, Intel and Nvidia; HDA sound; SATA and NVMe disks, SD cards and cd-roms; the network cards of Intel, Realtek, Aquantia, Atheros, Broadcom, Mellanox ConnectX and Brocade, with Fibre Channel; the Wi-Fi of Intel, Realtek, MediaTek and Qualcomm Atheros; the sensors, memory errors, watchdogs and SMBus of the chipsets of AMD and Intel; the platform drivers of Asus, Gigabyte and Dell; FireWire; printers on the parallel port;
- every driver of what people plug in: USB, HID and input, Bluetooth, webcams, USB sound and USB network adapters and tethering;
- those of software: loop, zram and nbd, device mapper and RAID, and the network devices of containers, virtual machines, VPNs and bonds, such as tun, veth, macvlan, vxlan, wireguard and vhost.

The network protocols SCTP and TIPC, which the kernel loads when any user opens a socket of them, and the rarely used file systems cramfs, freevxfs, jffs2 and hfs are left out, as they have had holes and HyprDebian does not use them; `hd-harden --modules` of [hd-base](hd-base.md#hd-harden) keeps them from being loaded with the other kernels. hfsplus and udf stay.

`keep-modules` in the package directory lists them, see [The keep list](hd-kernel.md#the-keep-list).

## The packages
`make deb` builds four packages for each suite, `trixie` and `unstable`, each only for the repository of its suite: `linux-image-<release>`, `zfs-modules-<release>` and `linux-headers-<release>`, as for [hd-kernel](hd-kernel.md#the-packages), and `hd-kernel-desktop`, which depends on the exact versions of the kernel and its ZFS module and installs the man page `man hd-kernel-desktop`.

The release is `<x.y.z>+debNN-hddesktop<id>-amd64`, such as `7.2.12+deb14-hddesktop1a2b3c-amd64`, where `<id>` comes from the versions of `linux`, `zfs-linux` and `hd-kernel-desktop`, the files that `hd-kernel` builds with, and `deb/`, `man/` and `keep-modules` of the package directory.

## Switching
On a desktop or laptop with `hd-base`:
```
sudo apt install hd-kernel-desktop
```

This removes the other HyprDebian kernels, `hd-kernel`, `hd-kernel-vm` and `hd-kernel-server`, and `linux-image-amd64`, `linux-headers-amd64`, `linux-base-amd64` and `zfs-dkms`, as installing `hd-kernel` does. The new kernel is at the top of the menu of the bootloader.

Before the system boots it, check that the kernel has the drivers of its graphics, disks and network: a driver that is not in `/lib/modules/<release>` is not in the kernel. Once it boots, the other kernels can be purged, as [hd-kernel](hd-kernel.md#switching) describes for the kernels of Debian.

To switch back to the kernel of `hd-kernel`, or to that of Debian:
```
sudo apt install hd-kernel
sudo apt install linux-image-amd64 linux-headers-amd64 zfs-dkms
```

Either removes `hd-kernel-desktop`; its kernels stay until they are purged.

## Building the packages
The packages are built from `packages/hd-kernel-desktop` in this repository, with `kernel.mk` and `build-kernel.sh` of `packages/hd-kernel`, which has the same targets:
```
cd packages/hd-kernel-desktop
make deb
```

In the root of the repository, `make kernel` builds them with the other kernels. See [Building the packages](hd-kernel.md#building-the-packages) of `hd-kernel`. Raise the version in `version.json` for every change of the packages.

The files involved are:
```
packages/hd-kernel-desktop/Makefile
packages/hd-kernel-desktop/version.json
packages/hd-kernel-desktop/deploy.json
packages/hd-kernel-desktop/keep-modules
packages/hd-kernel-desktop/deb/hd-kernel-desktop/control
packages/hd-kernel-desktop/man/hd-kernel-desktop.7
packages/hd-kernel/kernel.mk
packages/hd-kernel/build-kernel.sh
```

## Files
| Path | Purpose |
| --- | --- |
| `/boot/vmlinuz-<release>` | The kernel. |
| `/lib/modules/<release>` | Its modules. |
| `/lib/modules/<release>/extra/zfs` | The ZFS module. |
| `/usr/share/man/man7/hd-kernel-desktop.7.gz` | The man page. |
