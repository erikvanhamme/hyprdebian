## Introduction
The package `hd-kernel-server` switches a HyprDebian server to the HyprDebian kernel for servers: the kernel of [hd-kernel](hd-kernel.md), with the patches and the config of Debian and the ZFS module built against it on the build server, but with only the modules that a server needs. It is built, signed and installed like the kernel of `hd-kernel`, which describes how.

## What it keeps
What the config of Debian builds into the kernel stays, and so do the modules that are not drivers: the file systems, the network protocols and netfilter, crypto, and `kvm`, for virtual machines. Of the drivers only these stay:
- those of the hardware of servers: SATA and NVMe disks; the SAS and RAID cards of Intel, Broadcom, Adaptec and HPE, with their enclosures; Fibre Channel of QLogic; the network cards of Intel, Broadcom, Realtek, Mellanox ConnectX and Brocade, with Fibre Channel; IPMI, the memory errors, temperatures, watchdog and SMBus of the Intel chipset; the graphics of the BMC; the platform drivers of Dell; USB with keyboards, mice and storage;
- those of storage targets: the SCSI target with iSCSI, Fibre Channel over Ethernet, loopback and the user space backends of tcmu, Fibre Channel with the target mode of QLogic cards, and NVMe over TCP, Fibre Channel and loopback;
- those of software: loop, zram and nbd, device mapper and RAID, and the network devices of containers, virtual machines, VPNs and bonds, such as tun, veth, macvlan, vxlan, wireguard and vhost.

`keep-modules` in the package directory lists them, see [The keep list](hd-kernel.md#the-keep-list).

## The packages
`make deb` builds four packages for each suite, `trixie` and `unstable`, each only for the repository of its suite: `linux-image-<release>`, `zfs-modules-<release>` and `linux-headers-<release>`, as for [hd-kernel](hd-kernel.md#the-packages), and `hd-kernel-server`, which depends on the exact versions of the kernel and its ZFS module and installs the man page `man hd-kernel-server`.

The release is `<x.y.z>+debNN-hdserver<id>-amd64`, such as `7.2.12+deb14-hdserver1a2b3c-amd64`, where `<id>` comes from the versions of `linux`, `zfs-linux` and `hd-kernel-server`, the files that `hd-kernel` builds with, and `deb/`, `man/` and `keep-modules` of the package directory.

## Switching
On a server with `hd-base`:
```
sudo apt install hd-kernel-server
```

This removes the other HyprDebian kernels, `hd-kernel`, `hd-kernel-vm` and `hd-kernel-desktop`, and `linux-image-amd64`, `linux-headers-amd64`, `linux-base-amd64` and `zfs-dkms`, as installing `hd-kernel` does. The new kernel is at the top of the menu of the bootloader.

Before the server boots it, check that the kernel has the drivers of its disks and network cards: a driver that is not in `/lib/modules/<release>` is not in the kernel. Once it boots, the other kernels can be purged, as [hd-kernel](hd-kernel.md#switching) describes for the kernels of Debian.

To switch back to the kernel of `hd-kernel`, or to that of Debian:
```
sudo apt install hd-kernel
sudo apt install linux-image-amd64 linux-headers-amd64 zfs-dkms
```

Either removes `hd-kernel-server`; its kernels stay until they are purged.

## Building the packages
The packages are built from `packages/hd-kernel-server` in this repository, with `kernel.mk` and `build-kernel.sh` of `packages/hd-kernel`, which has the same targets:
```
cd packages/hd-kernel-server
make deb
```

In the root of the repository, `make kernel` builds them with the other kernels. See [Building the packages](hd-kernel.md#building-the-packages) of `hd-kernel`. Raise the version in `version.json` for every change of the packages.

The files involved are:
```
packages/hd-kernel-server/Makefile
packages/hd-kernel-server/version.json
packages/hd-kernel-server/deploy.json
packages/hd-kernel-server/keep-modules
packages/hd-kernel-server/deb/hd-kernel-server/control
packages/hd-kernel-server/man/hd-kernel-server.7
packages/hd-kernel/kernel.mk
packages/hd-kernel/build-kernel.sh
```

## Files
| Path | Purpose |
| --- | --- |
| `/boot/vmlinuz-<release>` | The kernel. |
| `/lib/modules/<release>` | Its modules. |
| `/lib/modules/<release>/extra/zfs` | The ZFS module. |
| `/usr/share/man/man7/hd-kernel-server.7.gz` | The man page. |
