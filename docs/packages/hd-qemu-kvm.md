## Introduction
The package `hd-qemu-kvm` installs Qemu with KVM, from `qemu-system-x86`, the UEFI firmware OVMF and libvirt, from `libvirt-daemon-system`, and sets them up to run virtual machines on the network through the bridge `br0`. It conflicts with `qemu-system-gui`, which `qemu-system-x86` recommends: virtual machines are shown with `virt-manager` or another spice client. `virt-manager` is not installed with the package; install it on a desktop with `sudo apt install virt-manager`.

Installing the package:
- makes the chosen wired interfaces ports of the bridge `br0` on the first installation, with the commands of [hd-network](hd-network.md); see below;
- installs `/etc/skel/.config/libvirt/libvirt.conf`, so `virsh` and `virt-manager` of new users connect to `qemu:///system`;
- diverts `/etc/libvirt/libvirtd.conf` and `/etc/libvirt/secret.conf` of libvirt and writes its own in their place: libvirtd does not listen on TCP or TLS, and secrets are stored without encryption;
- masks `libvirtd-tcp.socket`, `libvirtd-tls.socket` and the drop-in `libvirtd.service.d/10-secret.conf` of libvirt, which hands libvirtd an encryption key for the secrets;
- loads `br_netfilter` and keeps the traffic over bridges out of iptables, so ufw does not filter the traffic of the virtual machines.

It does not add anyone to the groups `libvirt` and `kvm`. To manage the virtual machines of `qemu:///system` without sudo, add your user to them and log in again, and copy `libvirt.conf` of `/etc/skel` to an existing home directory:
```
sudo usermod -a -G libvirt,kvm <user>
install -D -m 0644 /etc/skel/.config/libvirt/libvirt.conf ~/.config/libvirt/libvirt.conf
```

Members of the group `libvirt` control the virtual machines, and so the whole system.

The package comes with a man page: `man hd-qemu-kvm`.

## Building the package
The package is built from `hd-qemu-kvm` in this repository:
```
cd hd-qemu-kvm
make deb
```

This builds `build/hd-qemu-kvm_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `python3`, `python3-debconf`, `debconf`, `hd-network`, `qemu-system-x86`, `qemu-utils`, `ovmf`, `libvirt-daemon-system`, `libvirt-clients` and `bridge-utils`. Install it with:
```
sudo apt install ./build/hd-qemu-kvm_<version>_all.deb
```

The files involved are:
```
hd-qemu-kvm/Makefile
hd-qemu-kvm/version.json
hd-qemu-kvm/br_netfilter.conf
hd-qemu-kvm/50-bridge-netfilter.conf
hd-qemu-kvm/libvirt/libvirtd.conf
hd-qemu-kvm/libvirt/secret.conf
hd-qemu-kvm/deb/control
hd-qemu-kvm/deb/templates
hd-qemu-kvm/deb/postinst
hd-qemu-kvm/deb/postrm
hd-qemu-kvm/deb/etc/skel/.config/libvirt/libvirt.conf
hd-qemu-kvm/man/hd-qemu-kvm.7
```

## The bridge
The first installation sets up `br0` on a booted system, when the network settings of `hd-network` have no `br0` yet. The wired interfaces are those with a device that are not wifi and not a port of another bridge:
- With one, it becomes the port of `br0`.
- With more, the debconf question `hd-qemu-kvm/bridge-ports` asks which ones, and offers the ones that the network settings configure. More than one port is a valid choice: it joins those networks in one. Choosing none sets up no bridge.
- With none, there is no bridge.

`br0` takes over the address settings of the first chosen interface, such as DHCP or static addresses, and the ports lose theirs. The postinst runs:
```
hd-network-bridge set br0 [the address settings of the first interface]
hd-network-iface set <interface> --bridge br0      (for every chosen interface)
hd-network-render
```

The bridge has a MAC address of its own, so DHCP may give it another address than the interface had, and a connection over SSH may drop. This is done last, after everything else of the postinst.

In a chroot, where the interfaces are those of another system, no bridge is set up. The network stays as it is when the package is removed. To set up the bridge by hand, with `enp3s0` as its port:
```
sudo hd-network-bridge set br0
sudo hd-network-iface set enp3s0 --bridge br0
sudo hd-network-render
```

## The configs of libvirt
libvirt ships `/etc/libvirt/libvirtd.conf` and `/etc/libvirt/secret.conf` as conffiles. `hd-qemu-kvm` diverts them to `libvirtd.conf.libvirt` and `secret.conf.libvirt`, where dpkg keeps the ones of libvirt, and writes its own from `/usr/share/hd-qemu-kvm` in their place when it makes the diversion, after which libvirtd is restarted. Later installations leave them alone, so changes made to them stay. Removing the package removes them and puts the ones of libvirt back.

## Files
| Path | Purpose |
| --- | --- |
| `/etc/libvirt/libvirtd.conf` | The config of libvirtd that `hd-qemu-kvm` wrote: no TCP or TLS. |
| `/etc/libvirt/secret.conf` | The config of the secrets driver that `hd-qemu-kvm` wrote: no encryption. |
| `/etc/libvirt/*.conf.libvirt` | The configs of libvirt. |
| `/etc/skel/.config/libvirt/libvirt.conf` | Connects `virsh` and `virt-manager` to `qemu:///system`, a conffile. |
| `/etc/systemd/system/libvirtd-tcp.socket`, `libvirtd-tls.socket` | Links to `/dev/null`, which mask the sockets. |
| `/etc/systemd/system/libvirtd.service.d/10-secret.conf` | A link to `/dev/null`, which masks the drop-in of libvirt. |
| `/usr/lib/modules-load.d/br_netfilter.conf` | Loads `br_netfilter` at the boot. |
| `/usr/lib/sysctl.d/50-bridge-netfilter.conf` | Keeps the traffic over bridges out of the firewall. |
| `/usr/share/hd-qemu-kvm/` | The configs that are written to `/etc/libvirt`. |
