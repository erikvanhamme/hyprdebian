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
    python3 render.py templates/etc/apt/sources.list.d/hyprdebian.local.sources.j2 ${TARGET_DIR}/etc/apt/sources.list.d/hyprdebian.local.sources -v Q_REPO=${Q_REPO}

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

    add_services clear-machine-id
    add_packages rsyslog
    add_user_groups adm
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

    add_packages eza yazi fzf nfs-common psmisc net-tools pciutils usbutils acpi bash-completion git-delta ack
}

tgt_network() {

    # Render out the template for the wired interface.
    python3 render.py templates/etc/systemd/network/50-ethx.network.j2 ${TARGET_DIR}/etc/systemd/network/50-${Q_IFACE}.network -v Q_IFACE=${Q_IFACE} -v Q_QEMU_KVM=${Q_QEMU_KVM} -m 0644

    # If QEMU/KVM is enabled, generate a random MAC address and render out the br0 netdev file.
    if [[ "${Q_QEMU_KVM}" == "true" ]]; then
        mac=$(random_mac)
        python3 render.py templates/etc/systemd/network/30-br0.netdev.j2 ${TARGET_DIR}/etc/systemd/network/30-br0.netdev -v MAC_ADDRESS=${mac} -m 0644
    fi

    # Render out the template for the wifi interface. Delete the wifi files if wifi is not enabled.
    if [[ "${Q_WIFI}" == "true" ]]; then
        python3 render.py templates/etc/systemd/network/60-wlanx.network.j2 ${TARGET_DIR}/etc/systemd/network/60-${Q_WIFACE}.network -v Q_WIFACE=${Q_WIFACE} -m 0644

	    # If wifi is enabled, do not block the boot if no network comes online during boot.
	    in_target systemctl mask systemd-networkd-wait-online.service
    else
        rm -f ${TARGET_DIR}/etc/systemd/network/20-wifi.link
    fi

    # Have the firewall installed and trigger the enablement.
    if [[ "${Q_FIREWALL}" == "true" ]]; then
        add_packages ufw
        add_dependencies "t_services" "tgt_enable_firewall"
    fi

    # Remove the netfilter-bridge files if there is no reason to have them.
    if [[ "${Q_FIREWALL}" == "false" || "${Q_QEMU_KVM}" == "false" ]]; then
        rm -f ${TARGET_DIR}/etc/modules-load.d/br_netfilter.conf ${TARGET_DIR}/etc/sysctl.d/50-bridge-netfilter.conf
    fi

    # Make sure systemd-networkd is enabled.
    in_target systemctl enable systemd-networkd
}

tgt_enable_firewall() {
    in_target ufw enable

    if [[ "${Q_FIREWALL}" == "true" ]]; then
        in_target ufw allow SSH
    fi
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
    "tgt_network"

add_dependencies "t_install" \
    "t_base"
