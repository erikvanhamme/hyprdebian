## Introduction
The package `hd-kernel-vm` switches a HyprDebian KVM virtual machine to the HyprDebian kernel for virtual machines: the kernel of [hd-kernel](hd-kernel.md), with the patches and the config of Debian and the ZFS module built against it on the build server, but with only the modules that a KVM virtual machine needs. It is built, signed and installed like the kernel of `hd-kernel`, which describes how.

## What it keeps
What the config of Debian builds into the kernel stays, and so do the modules that are not drivers: the file systems, the network protocols and netfilter, crypto, and `kvm`, for virtual machines in the virtual machine. Of the drivers only these stay:
- those of the hardware that QEMU emulates for a KVM virtual machine: virtio for disks, network, graphics, sound, input, memory and random numbers; SATA, IDE, NVMe and SCSI disks and cd-roms; the network cards e1000, e1000e and RTL8139; the USB controllers with keyboards, mice, tablets, storage and audio; the standard VGA, QXL and Cirrus graphics; HDA and AC97 sound; the power button of ACPI, by which the host shuts the virtual machine down; the SMBus, LPC and watchdog of the q35 chipset, the watchdogs i6300esb and ib700, and the PC speaker; fw_cfg, pvpanic and the KVM clock for PTP;
- those of software: loop, zram and nbd, device mapper and RAID, and the network devices of containers, virtual machines, VPNs and bonds, such as tun, veth, macvlan, vxlan, wireguard and vhost.

`keep-modules` in the package directory lists them, see [The keep list](hd-kernel.md#the-keep-list).

## The packages
`make deb` builds four packages for each suite, `trixie` and `unstable`, each only for the repository of its suite: `linux-image-<release>`, `zfs-modules-<release>` and `linux-headers-<release>`, as for [hd-kernel](hd-kernel.md#the-packages), and `hd-kernel-vm`, which depends on the exact versions of the kernel and its ZFS module and installs the man page `man hd-kernel-vm`.

The release is `<x.y.z>+debNN-hdvm<id>-amd64`, such as `7.2.12+deb14-hdvm1a2b3c-amd64`, where `<id>` comes from the versions of `linux`, `zfs-linux` and `hd-kernel-vm`, the files that `hd-kernel` builds with, and `deb/`, `man/` and `keep-modules` of the package directory.

## Switching
In a virtual machine with `hd-base`:
```
sudo apt install hd-kernel-vm
```

This removes the other HyprDebian kernels, `hd-kernel`, `hd-kernel-server` and `hd-kernel-desktop`, and `linux-image-amd64`, `linux-headers-amd64`, `linux-base-amd64` and `zfs-dkms`, as installing `hd-kernel` does. The new kernel is at the top of the menu of the bootloader. Once it boots, the other kernels can be purged, as [hd-kernel](hd-kernel.md#switching) describes for the kernels of Debian.

To switch back to the kernel of `hd-kernel`, or to that of Debian:
```
sudo apt install hd-kernel
sudo apt install linux-image-amd64 linux-headers-amd64 zfs-dkms
```

Either removes `hd-kernel-vm`; its kernels stay until they are purged.

## Building the packages
The packages are built from `packages/hd-kernel-vm` in this repository, with `kernel.mk` and `build-kernel.sh` of `packages/hd-kernel`, which has the same targets:
```
cd packages/hd-kernel-vm
make deb
```

In the root of the repository, `make kernel` builds them with the other kernels. See [Building the packages](hd-kernel.md#building-the-packages) of `hd-kernel`. Raise the version in `version.json` for every change of the packages.

The files involved are:
```
packages/hd-kernel-vm/Makefile
packages/hd-kernel-vm/version.json
packages/hd-kernel-vm/deploy.json
packages/hd-kernel-vm/keep-modules
packages/hd-kernel-vm/deb/hd-kernel-vm/control
packages/hd-kernel-vm/man/hd-kernel-vm.7
packages/hd-kernel/kernel.mk
packages/hd-kernel/build-kernel.sh
```

## Files
| Path | Purpose |
| --- | --- |
| `/boot/vmlinuz-<release>` | The kernel. |
| `/lib/modules/<release>` | Its modules. |
| `/lib/modules/<release>/extra/zfs` | The ZFS module. |
| `/usr/share/man/man7/hd-kernel-vm.7.gz` | The man page. |
