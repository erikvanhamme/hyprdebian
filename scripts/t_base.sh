#!/bin/bash

t_base() {
    return 0
}

bootstrap() {
    mkdir ${TARGET_DIR}/run
    mount -t tmpfs tmpfs ${TARGET_DIR}/run
    mkdir ${TARGET_DIR}/run/lock
    mkdir -p ${TARGET_DIR}/var/lib

    debootstrap --arch=amd64 --exclude=ifupdown unstable ${TARGET_DIR} http://deb.debian.org/debian
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

configure_zfs_cache() {
    mkdir ${TARGET_DIR}/etc/zfs
    cp /etc/zfs/zpool.cache ${TARGET_DIR}/etc/zfs
}

deploy_files() {
    mkdir -p /mnt/etc/default
    mkdir -p /mnt/etc/apt
    mkdir -p /mnt/etc/dconf/db/local.d
    mkdir -p /mnt/usr/local/bin

    cp -rv deploy/etc/apt/. /mnt/etc/apt/
    cp -rv deploy/etc/skel/. /mnt/etc/skel/
    cp -rv deploy/etc/dconf/db/local.d/. /mnt/etc/dconf/local.d/
    cp -rv deploy/usr/local/bin/. /mnt/usr/local/bin/
}

tgt_mount() {
    mount --make-private --rbind /dev  /mnt/dev
    mount --make-private --rbind /proc /mnt/proc
    mount --make-private --rbind /sys  /mnt/sys

    in_target rm /dev/log
    in_target touch /dev/log
    mount --bind /run/systemd/journal/dev-log /mnt/dev/log
}

tgt_apt_init() {
    in_target apt update
}

tgt_add_sources() {
    in_target apt install -y curl gpg
    in_target sh -c 'curl -sS https://debian.griffo.io/EA0F721D231FDD3A0A17B9AC7808B4DD62C41256.asc | gpg --dearmor --yes -o /etc/apt/trusted.gpg.d/debian.griffo.io.gpg'
    in_target sh -c 'echo "deb https://debian.griffo.io/apt sid main" | tee /etc/apt/sources.list.d/debian.griffo.io.list'
    in_target apt update
}

tgt_locales() {
    in_target apt install -y locales
    in_target dpkg-reconfigure locales
}

tgt_buildtools() {
    in_target apt install -y build-essential cmake meson ninja-build git pkg-config initramfs-tools
}

tgt_kernel() {
    if [[ "${Q_KERNEL}" == "latest" ]]; then
        in_target apt install -y linux-image-amd64 linux-headers-amd64 firmware-linux
    else
        mkdir -p ${TARGET_DIR}/tmp/deb
        cp kernels/*${Q_KERNEL}* ${TARGET_DIR}/tmp/deb/
        in_target dpkg -R -i /tmp/deb/
        in_target apt install -y -f
        in_target apt install -y firmware-linux
    fi
}

tgt_zfs_support() {
    in_target apt install -y zfs-dkms zfsutils-linux zfs-initramfs
}

tgt_grub2() {
    in_target mkdir /boot/efi
    in_target mount /boot/efi
    in_target apt install -y grub-efi-amd64 shim-signed
    in_target update-initramfs -c -k all
    cp -v deploy/etc/default/grub /mnt/etc/default
    in_target update-grub
    in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=hyprdebian --recheck --no-floppy
}

tgt_systemd() {
    in_target apt install -y systemd-timesyncd
}

tgt_console() {
    in_target apt install -y console-setup command-not-found man-db
    in_target dpkg-reconfigure tzdata keyboard-configuration console-setup
    in_target apt-file update
}

tgt_netplan() {
    in_target apt install -y netplan.io

    python3 render.py templates/netplan/01-wired.yaml.j2 ${TARGET_DIR}/etc/netplan/01-wired.yaml -v Q_IFACE=${Q_IFACE} -v Q_QEMU_KVM=${Q_QEMU_KVM}

    if [[ "${Q_WIFI}" == "true" ]]; then
        python3 render.py templates/netplan/02-wifi.yaml.j2 ${TARGET_DIR}/etc/netplan/02-wifi.yaml -v Q_WIFACE=${Q_WIFACE}
    fi

    in_target chmod 0600 /etc/netplan/*.yaml

    if [[ "${Q_QEMU_KVM}" == "true" ]]; then
        mkdir -p ${TARGET_DIR}/etc/systemd/network/10-netplan-br0.network.d
        cp deploy/etc/systemd/network/10-netplan-br0.network.d/forced_carrier.conf ${TARGET_DIR}/etc/systemd/network/10-netplan-br0.network.d/
    fi

    in_target netplan generate
}

add_dependencies "t_base" "bootstrap" "configure_fstab" "configure_hostname" "configure_hosts" "configure_zfs_cache" "deploy_files" "tgt_mount" "tgt_apt_init" "tgt_add_sources" "tgt_locales" "tgt_buildtools" "tgt_kernel"
add_dependencies "t_base" "tgt_zfs_support" "tgt_grub2" "tgt_systemd" "tgt_console" "tgt_netplan"
add_dependencies "t_install" "t_base"
