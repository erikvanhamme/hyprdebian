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
        deploy/etc/default/zramswap \
        deploy/etc/profile.d/local-bin-path.sh \
        deploy/etc/sysctl.d/50-zram.conf \
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
    mkdir -p ${TARGET_DIR}/run || return 1
    mount -t tmpfs tmpfs ${TARGET_DIR}/run || return 1
    mkdir -p ${TARGET_DIR}/run/lock || return 1
    mkdir -p ${TARGET_DIR}/var/lib || return 1

    debootstrap --arch=amd64 --exclude=ifupdown --include=ca-certificates ${Q_SUITE} ${TARGET_DIR} http://deb.debian.org/debian
}

b_fstab() {
    if [[ "${Q_SWAP:-1}" -gt 0 && "${Q_ENCRYPT}" == "true" ]]; then
        local swap_crypt=true

        template_render templates/etc/fstab.j2
    elif [[ "${Q_SWAP:-1}" -gt 0 ]]; then
        local swap_part swap_uuid
        swap_part=${Q_DISK}-part2
        if [[ "${Q_REDUNDANT}" == "true" ]]; then
            swap_part=/dev/md/swap
        fi
        swap_uuid=$(blkid -s UUID -o value ${swap_part})

        if [[ -z "$swap_uuid" ]]; then
            echo "ERROR: Unable to determine UUIDs for fstab."
            return 1
        fi

        template_render templates/etc/fstab.j2
    else
        template_render templates/etc/fstab.j2
    fi
}

b_mdadm() {
    if [[ "${Q_REDUNDANT}" != "true" || "${Q_SWAP:-1}" -eq 0 ]]; then
        return 0
    fi

    local mdadm_array
    mdadm_array=$(mdadm --detail --brief /dev/md/swap)

    if [[ -z "$mdadm_array" ]]; then
        echo "ERROR: Unable to determine the swap array for mdadm.conf."
        return 1
    fi

    # Written before mdadm is installed, so its postinst keeps this file.
    template_render templates/etc/mdadm/mdadm.conf.j2 || return 1

    add_packages mdadm
}

b_crypttab() {
    if [[ "${Q_ENCRYPT}" != "true" || "${Q_SWAP:-1}" -eq 0 ]]; then
        return 0
    fi

    # The swap device is overwritten on every boot, so refer to it by a name that can
    # never point at another device.
    local swap_device swap_id
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        swap_id=$(mdadm --detail --export /dev/md/swap | sed -n 's/^MD_UUID=//p')
        swap_device=/dev/disk/by-id/md-uuid-${swap_id}
    else
        swap_id=$(blkid -s PARTUUID -o value ${Q_DISK}-part2)
        swap_device=/dev/disk/by-partuuid/${swap_id}
    fi

    if [[ -z "$swap_id" ]]; then
        echo "ERROR: Unable to determine the swap device for crypttab."
        return 1
    fi

    template_render templates/etc/crypttab.j2 || return 1

    add_packages systemd-cryptsetup
}

b_mount_unit_redundant() {
    local efi_part efi2_part efi_uuid efi2_uuid
    efi_part=${Q_DISK_A}-part1
    efi2_part=${Q_DISK_B}-part1
    efi_uuid=$(blkid -s UUID -o value ${efi_part})
    efi2_uuid=$(blkid -s UUID -o value ${efi2_part})

    if [[ -z "$efi_uuid" || -z "$efi2_uuid" ]]; then
        echo "ERROR: Unable to determine UUIDs for fstab."
        return 1
    fi

    template_render templates/etc/systemd/system/boot-efi.mount.j2 || return 1
    template_render templates/etc/systemd/system/boot-efi2.mount.j2 || return 1

    add_files \
        deploy/etc/systemd/system/boot-efi.automount \
        deploy/etc/systemd/system/boot-efi2.automount \

    add_services \
        boot-efi.automount \
        boot-efi2.automount \

}

b_mount_unit_single() {
    local efi_part efi_uuid
    efi_part=${Q_DISK}-part1
    efi_uuid=$(blkid -s UUID -o value ${efi_part})

    if [[ -z "$efi_uuid" ]] ; then
        echo "ERROR: Unable to determine UUIDs for fstab."
        return 1
    fi

    template_render templates/etc/systemd/system/boot-efi.mount.j2 || return 1

    add_files \
        deploy/etc/systemd/system/boot-efi.automount \

    add_services \
        boot-efi.automount \

}

b_mount_unit() {
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        b_mount_unit_redundant
    else
        b_mount_unit_single
    fi
}

b_zfs_cache() {
    mkdir -p ${TARGET_DIR}/etc/zfs || return 1
    cp /etc/zfs/zpool.cache ${TARGET_DIR}/etc/zfs
}

b_mount() {
    mount --make-private --rbind /dev  ${TARGET_DIR}/dev || return 1
    mount --make-private --rbind /proc ${TARGET_DIR}/proc || return 1
    mount --make-private --rbind /sys  ${TARGET_DIR}/sys || return 1

    # /dev is shared with the live system, so leave its /dev/log alone. Its symlink
    # points at /run/systemd/journal/dev-log, so expose the journal socket there.
    mkdir -p ${TARGET_DIR}/run/systemd/journal || return 1
    touch ${TARGET_DIR}/run/systemd/journal/dev-log || return 1
    mount --bind /run/systemd/journal/dev-log ${TARGET_DIR}/run/systemd/journal/dev-log
}

b_apt_init() {
    template_render templates/etc/apt/sources.list.d/debian.sources.j2 || return 1

    if [[ "${Q_REPO_ENABLED}" == "true" ]]; then
        template_render templates/etc/apt/sources.list.d/hyprdebian.local.sources.j2 || return 1
    fi

    rm -f ${TARGET_DIR}/etc/apt/sources.list || return 1

    in_target apt update
}

b_console() {
    in_target env DEBIAN_FRONTEND=noninteractive apt install -y locales keyboard-configuration console-setup || return 1

    in_target dpkg-reconfigure locales || return 1

    in_target dpkg-reconfigure tzdata || return 1

    in_target dpkg-reconfigure keyboard-configuration || return 1

    in_target dpkg-reconfigure console-setup
}

b_buildtools() {
    # Deployed before the kernel is installed, so no initramfs ever waits for a resume device.
    file_deploy deploy/etc/initramfs-tools/conf.d/resume || return 1

    in_target apt install -y build-essential initramfs-tools
}

b_kernel() {
    if [[ "${Q_KERNEL}" == "latest" ]]; then
        in_target apt install -y linux-image-amd64 linux-headers-amd64 || return 1
    else
        mkdir -p ${TARGET_DIR}/tmp/deb || return 1
        cp kernels/*${Q_KERNEL}* ${TARGET_DIR}/tmp/deb/ || return 1

        # dpkg fails when the packages have missing dependencies, apt install -f then installs them.
        in_target dpkg -R -i /tmp/deb/
        in_target apt install -y -f || return 1
    fi

    add_packages firmware-linux firmware-qlogic
}

b_zfs_support() {
    in_target env DEBIAN_FRONTEND=noninteractive apt install -y zfs-dkms zfsutils-linux zfs-initramfs
}

b_grub2() {
    # The checks for an existing mount let a rerun of this task get past the mounts.
    in_target mkdir -p /boot/efi || return 1

    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        in_target mkdir -p /boot/efi2 || return 1

        in_target mountpoint -q /boot/efi || in_target mount ${Q_DISK_A}-part1 /boot/efi || return 1
        in_target mountpoint -q /boot/efi2 || in_target mount ${Q_DISK_B}-part1 /boot/efi2 || return 1
    else
        in_target mountpoint -q /boot/efi || in_target mount ${Q_DISK}-part1 /boot/efi || return 1
    fi

    in_target apt install -y grub-efi-amd64 shim-signed os-prober- || return 1
    
    in_target update-initramfs -c -k all || return 1
    
    file_deploy deploy/etc/default/grub || return 1
    file_deploy deploy/etc/grub.d/11_zfs_recovery || return 1
    
    in_target update-grub || return 1
    
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id="hyprdebian (primary)" --recheck --no-floppy || return 1
        in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi2 --bootloader-id="hyprdebian (secondary)" --recheck --no-floppy
    else
        in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id="hyprdebian" --recheck --no-floppy
    fi
}

b_systemd() {
    if [[ "${Q_TYPE}" != "vm" ]]; then
        add_packages systemd-timesyncd
    fi

    add_services rotate-machine-id

    add_packages systemd-resolved rsyslog

    add_files \
        deploy/etc/systemd/journald.conf \
        deploy/etc/systemd/resolved.conf.d/00-direct-uplink.conf \
        deploy/etc/systemd/system/rotate-machine-id.service \
        deploy/etc/systemd/system-preset/00-ignore-all.preset \


    add_user_groups adm
}

b_utilities() {
    add_packages eza fzf nfs-common psmisc net-tools pciutils usbutils acpi bash-completion git git-delta ack \
        command-not-found man-db apt-file zram-tools python3-jinja2

    if [[ "${Q_TYPE}" == "vm" ]]; then
        add_packages qemu-guest-agent
    fi

    add_dependencies "pkg_post" "pkg_utilities"
}

b_network() {
    add_files deploy/etc/systemd/network/10-ethernet.link

    add_template templates/etc/systemd/network/50-ethx.network.j2

    add_services systemd-networkd
}

add_dependencies "b_pre" \
    "b_files" \
    "b_templates" \

add_dependencies "b_main" \
    "b_bootstrap" \
    "b_fstab" \
    "b_mdadm" \
    "b_crypttab" \
    "b_mount_unit" \
    "b_zfs_cache"  \
    "b_mount" \
    "b_apt_init" \
    "b_console" \
    "b_buildtools" \
    "b_kernel" \
    "b_zfs_support" \
    "b_grub2" \
    "b_systemd" \
    "b_utilities" \
    "b_network" \

add_dependencies "install" "b_pre" "b_main" "b_post"
