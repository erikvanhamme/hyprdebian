#!/bin/bash

b_pre() {
    return 0
}

b_main() {
    return 0
}

b_post() {
    return 0
}

b_bootstrap() {
    mkdir ${TARGET_DIR}/run
    mount -t tmpfs tmpfs ${TARGET_DIR}/run
    mkdir ${TARGET_DIR}/run/lock
    mkdir -p ${TARGET_DIR}/var/lib

    debootstrap --arch=amd64 --exclude=ifupdown --include=ca-certificates unstable ${TARGET_DIR} http://deb.debian.org/debian
}

b_fstab_redundant() {
    local efi_part efi2_part efi_uuid efi2_uuid
    efi_part=${Q_DISK_A}-part1
    efi2_part=${Q_DISK_B}-part1
    efi_uuid=$(blkid -s UUID -o value ${efi_part})
    efi2_uuid=$(blkid -s UUID -o value ${efi2_part})

    if [[ -z "$efi_uuid" || -z "$efi2_uuid" ]]; then
        echo "ERROR: Unable to determine UUIDs for fstab."
        return 1
    fi

    python3 render.py templates/etc/fstab.j2 ${TARGET_DIR}/etc/fstab -v efi_uuid=${efi_uuid} -v efi2_uuid=${efi2_uuid}
}

b_fstab_noswap() {
    local efi_part efi_uuid
    efi_part=${Q_DISK}-part1
    efi_uuid=$(blkid -s UUID -o value ${efi_part})

    if [[ -z "$efi_uuid" ]] ; then
        echo "ERROR: Unable to determine UUIDs for fstab."
        return 1
    fi

    python3 render.py templates/etc/fstab.j2 ${TARGET_DIR}/etc/fstab -v efi_uuid=${efi_uuid}
}

b_fstab_swap() {
    local efi_part efi_uuid
    efi_part=${Q_DISK}-part1
    efi_uuid=$(blkid -s UUID -o value ${efi_part})

    local swap_part swap_uuid
    swap_part=${Q_DISK}-part2
    swap_uuid=$(blkid -s UUID -o value ${swap_part})

    if [[ -z "$efi_uuid" || -z "$swap_uuid" ]]; then
        echo "ERROR: Unable to determine UUIDs for fstab."
        return 1
    fi

    python3 render.py templates/etc/fstab.j2 ${TARGET_DIR}/etc/fstab -v efi_uuid=${efi_uuid} -v swap_uuid=${swap_uuid}
}

b_fstab() {
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        b_fstab_redundant
    else
        if [[ "${Q_SWAP:-1}" -eq 0 ]]; then
            b_fstab_noswap
        else
            b_fstab_swap
        fi
    fi
}

b_hostname() {
    python3 render.py templates/etc/hostname.j2 ${TARGET_DIR}/etc/hostname -v Q_HOSTNAME=${Q_HOSTNAME}
}

b_hosts() {
    python3 render.py templates/etc/hosts.j2 ${TARGET_DIR}/etc/hosts -v Q_FQDN=${Q_FQDN} -v Q_HOSTNAME=${Q_HOSTNAME}
}

b_zfs_cache() {
    mkdir ${TARGET_DIR}/etc/zfs
    cp /etc/zfs/zpool.cache ${TARGET_DIR}/etc/zfs
}

b_deploy() {
    mkdir -p ${TARGET_DIR}/etc
    mkdir -p ${TARGET_DIR}/usr/local/bin

    cp -rv deploy/etc/. ${TARGET_DIR}/etc/
    cp -rv deploy/usr/local/bin/. ${TARGET_DIR}/usr/local/bin/

    if [[ "${Q_REDUNDANT}" == "false" ]]; then
        rm ${TARGET_DIR}/usr/local/bin/hd-sync-efi
        rm ${TARGET_DIR}/etc/apt/conf.d/99sync-efi
    fi
}

b_mount() {
    mount --make-private --rbind /dev  ${TARGET_DIR}/dev
    mount --make-private --rbind /proc ${TARGET_DIR}/proc
    mount --make-private --rbind /sys  ${TARGET_DIR}/sys

    in_target rm /dev/log
    in_target touch /dev/log
    mount --bind /run/systemd/journal/dev-log ${TARGET_DIR}/dev/log
}

b_apt_init() {
    if [[ "${Q_REPO_ENABLED}" == "true" ]]; then
        python3 render.py templates/etc/apt/sources.list.d/hyprdebian.local.sources.j2 ${TARGET_DIR}/etc/apt/sources.list.d/hyprdebian.local.sources -v Q_REPO=${Q_REPO}
    fi

    rm -f ${TARGET_DIR}/etc/apt/sources.list

    in_target apt update
}

b_locales() {
    in_target apt install -y locales
    in_target dpkg-reconfigure locales
}

b_buildtools() {
    in_target apt install -y build-essential cmake meson ninja-build git pkg-config initramfs-tools
}

b_kernel() {
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

b_zfs_support() {
    in_target apt install -y zfs-dkms zfsutils-linux zfs-initramfs
}

b_grub2() {
    in_target mkdir /boot/efi
    in_target mount /boot/efi
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        in_target mkdir /boot/efi2
        in_target mount /boot/efi2
    fi
    in_target apt install -y grub-efi-amd64 shim-signed
    in_target update-initramfs -c -k all
    cp -v deploy/etc/default/grub ${TARGET_DIR}/etc/default
    in_target update-grub
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id="hyprdebian (primary)" --recheck --no-floppy
        in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi2 --bootloader-id="hyprdebian (secondary)" --recheck --no-floppy
    else
        in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id="hyprdebian" --recheck --no-floppy
    fi
}

b_systemd() {
    in_target apt install -y systemd-timesyncd

    add_services clear-machine-id
    add_packages rsyslog
    add_user_groups adm
}

b_console() {

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

b_utilities() {
    in_target apt install -y command-not-found man-db apt-file
    in_target apt-file update

    # Note: command-not-found requires an update to the apt-file cache to work.
    in_target update-command-not-found

    add_packages eza fzf nfs-common psmisc net-tools pciutils usbutils acpi bash-completion git-delta ack

    if [[ "${Q_REPO_ENABLED}" == "true" ]]; then
        add_packages yazi
    fi
}

b_network() {

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

    # Make sure systemd-networkd is enabled.
    in_target systemctl enable systemd-networkd
}

add_dependencies "b_main" \
    "b_bootstrap" \
    "b_fstab" \
    "b_hostname"  \
    "b_hosts"  \
    "b_zfs_cache"  \
    "b_deploy" \
    "b_mount" \
    "b_apt_init" \
    "b_locales" \
    "b_buildtools" \
    "b_kernel" \
    "b_zfs_support" \
    "b_grub2" \
    "b_systemd" \
    "b_console" \
    "b_utilities" \
    "b_network" \

add_dependencies "install" "b_pre" "b_main" "b_post"
