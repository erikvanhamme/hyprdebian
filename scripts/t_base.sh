#!/bin/bash

t_base() {
    return 0
}

bootstrap() {
    mkdir /mnt/run
    mount -t tmpfs tmpfs /mnt/run
    mkdir /mnt/run/lock
    mkdir -p /mnt/var/lib

    debootstrap --arch=amd64 --exclude=ifupdown unstable /mnt http://deb.debian.org/debian
}

configure_fstab() {
    local efi_part swap_part
    local efi_uuid swap_uuid

    efi_part=${Q_DISK}-part1
    swap_part=${Q_DISK}-part2
    efi_uuid=$(blkid -s UUID -o value ${efi_part})
    swap_uuid=$(blkid -s UUID -o value ${swap_part})

    if [[ -z "$efi_uuid" || -z "$swap_uuid" ]]; then
        echo "ERROR: Unable to determine UUIDs for fstab."
        return 1
    fi

    write_file /mnt/etc/fstab 0644 <<EOF
# /etc/fstab: static file system information
#
# <file system>  <mount point>  <type>  <options>         <dump> <pass>

UUID=${efi_uuid}   /boot/efi   vfat   umask=0077        0      1
UUID=${swap_uuid}  none        swap   sw                0      0
EOF
}

configure_hostname() {
    write_file /mnt/etc/hostname 0644 <<EOF
${Q_HOSTNAME}
EOF
}

configure_hosts() {
    write_file /mnt/etc/hosts 0644 <<EOF
127.0.0.1   localhost
127.0.1.1   ${Q_FQDN} ${Q_HOSTNAME}

# IPv6
::1         localhost ip6-localhost ip6-loopback
ff02::1     ip6-allnodes
ff02::2     ip6-allrouters
EOF
}

configure_netplan() {
    mkdir -p /mnt/etc/netplan

    if ${Q_QEMU_KVM}; then
        # Virtualization enabled: Create a bridge (br0) for VM networking.
        write_file /mnt/etc/netplan/01-wired.yaml 0600 <<EOF
network:
  version: 2
  renderer: networkd
  ethernets:
    ${Q_IFACE}:
      dhcp4: false
      dhcp6: false
      optional: true
      ignore-carrier: no
  bridges:
    br0:
      interfaces: [${Q_IFACE}]
      dhcp4: true
      dhcp6: true
      parameters:
        stp: false
        forward-delay: 0
      optional: true
      dhcp4-overrides:
        route-metric: 100
      dhcp6-overrides:
        route-metric: 100
EOF

        mkdir -p /mnt/etc/systemd/network/10-netplan-br0.network.d
        write_file /mnt/etc/systemd/network/10-netplan-br0.network.d/forced_carrier.conf 0600 <<EOF
[Network]
KeepConfiguration=no
EOF
    else
        # Standard installation: DHCP directly on the physical interface.
        write_file /mnt/etc/netplan/01-wired.yaml 0600 <<EOF
network:
  version: 2
  renderer: networkd
  ethernets:
    ${Q_IFACE}:
      dhcp4: true
      dhcp6: true
      optional: true
      ignore-carrier: no
EOF
    fi

    if Q_WIFI; then
        # Wifi enabled: Link will be managed by iwd, but netplan deals with it as a DHCP Ethernet interface.
        write_file /mnt/etc/netplan/02-wifi.yaml 0600 <<EOF
network:
  version: 2
  renderer: networkd
  ethernets:
    ${Q_WIFACE}:
      dhcp4: true
      dhcp6: true
      optional: true
      ignore-carrier: no
      dhcp4-overrides:
        route-metric: 200
      dhcp6-overrides:
        route-metric: 200
EOF
    fi
}

configure_zfs_cache() {
    mkdir /mnt/etc/zfs
    cp /etc/zfs/zpool.cache /mnt/etc/zfs
}

add_dependencies "t_base" "bootstrap" "configure_fstab" "configure_hostname" "configure_hosts" "configure_netplan" "configure_zfs_cache"
add_dependencies "install" "t_base"
