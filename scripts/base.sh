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

b_files() {

    # These files need to be deployed for sure.
    add_files \
        deploy/etc/apt/preferences.d/00-block-unwanted \
        deploy/etc/profile.d/local-bin-path.sh \
        deploy/etc/skel/.config/nano/nanorc \
        deploy/etc/skel/.bash_aliases \
        deploy/etc/skel/.gitconfig \
        deploy/usr/local/bin/hd-up \

}

b_templates() {

    # These templates need to be rendered for sure.
    add_template templates/etc/hostname.j2
    add_template templates/etc/hosts.j2
}

b_bootstrap() {
    mkdir ${TARGET_DIR}/run
    mount -t tmpfs tmpfs ${TARGET_DIR}/run
    mkdir ${TARGET_DIR}/run/lock
    mkdir -p ${TARGET_DIR}/var/lib

    debootstrap --arch=amd64 --exclude=ifupdown --include=ca-certificates ${Q_SUITE} ${TARGET_DIR} http://deb.debian.org/debian
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

    template_render templates/etc/fstab.j2
}

b_fstab_noswap() {
    local efi_part efi_uuid
    efi_part=${Q_DISK}-part1
    efi_uuid=$(blkid -s UUID -o value ${efi_part})

    if [[ -z "$efi_uuid" ]] ; then
        echo "ERROR: Unable to determine UUIDs for fstab."
        return 1
    fi

    template_render templates/etc/fstab.j2
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

    template_render templates/etc/fstab.j2
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

b_zfs_cache() {
    mkdir ${TARGET_DIR}/etc/zfs
    cp /etc/zfs/zpool.cache ${TARGET_DIR}/etc/zfs
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
    template_render templates/etc/apt/sources.list.d/debian.sources.j2

    if [[ "${Q_REPO_ENABLED}" == "true" ]]; then
        template_render templates/etc/apt/sources.list.d/hyprdebian.local.sources.j2
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
    
    file_deploy deploy/etc/default/grub
    file_deploy deploy/etc/grub.d/11_zfs_recovery
    
    in_target update-grub
    
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id="${Q_OS} (primary)" --recheck --no-floppy
        in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi2 --bootloader-id="${Q_OS} (secondary)" --recheck --no-floppy
    else
        in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id="${Q_OS}" --recheck --no-floppy
    fi
}

b_systemd() {
    in_target apt install -y systemd-timesyncd

    if [[ "${Q_OS}" == "hyprdebian" ]]; then
        add_services clear-machine-id

        add_packages rsyslog

        add_files \
            deploy/etc/systemd/journald.conf \
            deploy/etc/systemd/system/clear-machine-id.service \
            
    fi

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

    add_packages eza fzf nfs-common psmisc net-tools pciutils usbutils acpi bash-completion git-delta ack qemu-guest-agent \ 
        python-is-python3 python3-colorama python3-tabulate

    if [[ "${Q_REPO_ENABLED}" == "true" ]]; then
        add_packages yazi

        add_files deploy/etc/skel/.config/yazi/yazi.toml 
    fi
}

b_network() {
    add_files deploy/etc/systemd/network/10-ethernet.link

    add_template templates/etc/systemd/network/50-ethx.network.j2

    in_target systemctl enable systemd-networkd
}

add_dependencies "b_pre" \
    "b_files" \
    "b_templates" \

add_dependencies "b_main" \
    "b_bootstrap" \
    "b_fstab" \
    "b_zfs_cache"  \
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
