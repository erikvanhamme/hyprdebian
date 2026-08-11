#!/bin/bash

t_base() {
    return 0 # Empty method to hang dependencies off of.
}

bootstrap() {
    mkdir ${TARGET_DIR}/run
    mount -t tmpfs tmpfs ${TARGET_DIR}/run
    mkdir ${TARGET_DIR}/run/lock
    mkdir -p ${TARGET_DIR}/var/lib

    debootstrap --arch=amd64 --exclude=ifupdown --include=ca-certificates unstable ${TARGET_DIR} http://deb.debian.org/debian
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

    python3 render.py templates/etc/fstab.j2 ${TARGET_DIR}/etc/fstab -v efi_uuid=${efi_uuid} -v swap_uuid=${swap_uuid}
}

configure_hostname() {
    python3 render.py templates/etc/hostname.j2 ${TARGET_DIR}/etc/hostname -v Q_HOSTNAME=${Q_HOSTNAME}
}

configure_hosts() {
    python3 render.py templates/etc/hosts.j2 ${TARGET_DIR}/etc/hosts -v Q_FQDN=${Q_FQDN} -v Q_HOSTNAME=${Q_HOSTNAME}
}

configure_zfs_cache() {
    mkdir ${TARGET_DIR}/etc/zfs
    cp /etc/zfs/zpool.cache ${TARGET_DIR}/etc/zfs
}

deploy_files() {
    mkdir -p ${TARGET_DIR}/etc
    mkdir -p ${TARGET_DIR}/usr/local/bin

    cp -rv deploy/etc/. ${TARGET_DIR}/etc/
    cp -rv deploy/usr/local/bin/. ${TARGET_DIR}/usr/local/bin/
}

tgt_mount() {
    mount --make-private --rbind /dev  ${TARGET_DIR}/dev
    mount --make-private --rbind /proc ${TARGET_DIR}/proc
    mount --make-private --rbind /sys  ${TARGET_DIR}/sys

    in_target rm /dev/log
    in_target touch /dev/log
    mount --bind /run/systemd/journal/dev-log ${TARGET_DIR}/dev/log
}

tgt_apt_init() {
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
    cp -v deploy/etc/default/grub ${TARGET_DIR}/etc/default
    in_target update-grub
    in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=hyprdebian --recheck --no-floppy
}

tgt_systemd() {
    in_target apt install -y systemd-timesyncd
}

tgt_console() {
    # 1. Install packages first
    in_target apt install -y keyboard-configuration console-setup tzdata

    # 2. Configure timezone
    in_target dpkg-reconfigure tzdata

    # 3. Configure the keyboard layout FIRST.
    # This satisfies the dependency so console-setup won't ask again.
    in_target dpkg-reconfigure keyboard-configuration

    # 4. Configure the console font/size.
    # It will pull the layout just picked in step 3 automatically.
    in_target dpkg-reconfigure console-setup
}

base_utilities() {
    in_target apt install -y command-not-found man-db apt-file
    in_target apt-file update
    # Note: command-not-found requires an update to the apt-file cache to work.
    in_target update-command-not-found

    add_packages eza yazi fzf nfs-common psmisc net-tools pciutils usbutils acpi bash-completion
}

tgt_netplan() {
    in_target apt install -y netplan.io

    python3 render.py templates/etc/netplan/01-wired.yaml.j2 ${TARGET_DIR}/etc/netplan/01-wired.yaml -v Q_IFACE=${Q_IFACE} -v Q_QEMU_KVM=${Q_QEMU_KVM} -m 0600

    if [[ "${Q_WIFI}" == "true" ]]; then
        python3 render.py templates/etc/netplan/02-wifi.yaml.j2 ${TARGET_DIR}/etc/netplan/02-wifi.yaml -v Q_WIFACE=${Q_WIFACE} -m 0600
    fi

    if [[ "${Q_QEMU_KVM}" == "false" ]]; then
        rm -rf ${TARGET_DIR}/etc/systemd/network/10-netplan-br0.network.d
    fi

    in_target netplan generate
}

add_dependencies "t_base" \
    "bootstrap" \
    "configure_fstab" \
    "configure_hostname" \
    "configure_hosts" \
    "configure_zfs_cache" \
    "deploy_files" \
    "tgt_mount" \
    "tgt_apt_init" \
    "tgt_locales" \
    "tgt_buildtools" \
    "tgt_kernel" \
    "tgt_zfs_support" \
    "tgt_grub2" \
    "tgt_systemd" \
    "tgt_console" \
    "base_utilities" \
    "tgt_netplan"

add_dependencies "t_install" \
    "t_base"
