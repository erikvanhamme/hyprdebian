#!/bin/bash
# The base phase: installs the base system with debootstrap on the ZFS pool, writes the files that
# depend on the disks and the answers, adds the Debian repositories and the local repository of the
# live cd, and installs the kernel and ZFS. The other packages are marked with add_packages and installed by the
# packages phase.

b_pre() {
    return 0
}

b_main() {
    return 0
}

b_post() {
    return 0
}

# The base system is the minbase variant of debootstrap: the required packages and apt, without
# the important ones, such as cron, dhcpcd and vim, which HyprDebian does not use. hd-base depends
# on the ones it needs. systemd comes along, as b_mount_unit enables units with systemctl before
# hd-base is installed, and whiptail, so the debconf questions of b_console are asked in dialogs
# instead of as long numbered lists.
b_bootstrap() {
    mkdir -p "${TARGET_DIR}/run" || return 1
    mount -t tmpfs tmpfs "${TARGET_DIR}/run" || return 1
    mkdir -p "${TARGET_DIR}/run/lock" || return 1
    mkdir -p "${TARGET_DIR}/var/lib" || return 1

    debootstrap --arch=amd64 --variant=minbase --include=ca-certificates,systemd,whiptail \
        "${Q_SUITE}" "${TARGET_DIR}" http://deb.debian.org/debian
}

b_templates() {

    # These templates need to be rendered for sure.
    template_render templates/etc/hostname.j2 || return 1
    template_render templates/etc/hosts.j2
}

b_fstab() {
    if (( Q_SWAP > 0 )) && [[ "${Q_ENCRYPT}" == "true" ]]; then
        template_render templates/etc/fstab.j2 0644 swap_crypt=true
    elif (( Q_SWAP > 0 )); then
        local swap_part swap_uuid
        swap_part="$(part "${Q_ROOT_DISK_A}" "${PART_SWAP}")"
        if [[ "${Q_ROOT_REDUNDANT}" == "true" ]]; then
            swap_part=/dev/md/swap
        fi
        swap_uuid=$(blkid -s UUID -o value "${swap_part}")

        if [[ -z "${swap_uuid}" ]]; then
            echo "ERROR: Unable to determine UUIDs for fstab."
            return 1
        fi

        template_render templates/etc/fstab.j2 0644 swap_uuid="${swap_uuid}"
    else
        template_render templates/etc/fstab.j2
    fi
}

b_mdadm() {
    if [[ "${Q_ROOT_REDUNDANT}" != "true" ]] || (( Q_SWAP == 0 )); then
        return 0
    fi

    local mdadm_array
    mdadm_array=$(mdadm --detail --brief /dev/md/swap)

    if [[ -z "${mdadm_array}" ]]; then
        echo "ERROR: Unable to determine the swap array for mdadm.conf."
        return 1
    fi

    # Written before mdadm is installed, so its postinst keeps this file.
    template_render templates/etc/mdadm/mdadm.conf.j2 0644 mdadm_array="${mdadm_array}" \
        || return 1

    add_packages mdadm
}

b_crypttab() {
    if [[ "${Q_ENCRYPT}" != "true" ]] || (( Q_SWAP == 0 )); then
        return 0
    fi

    # The swap device is overwritten on every boot, so refer to it by a name that can
    # never point at another device.
    local swap_device swap_id
    if [[ "${Q_ROOT_REDUNDANT}" == "true" ]]; then
        swap_id=$(mdadm --detail --export /dev/md/swap | sed -n 's/^MD_UUID=//p')
        swap_device=/dev/disk/by-id/md-uuid-${swap_id}
    else
        swap_id=$(blkid -s PARTUUID -o value "$(part "${Q_ROOT_DISK_A}" "${PART_SWAP}")")
        swap_device=/dev/disk/by-partuuid/${swap_id}
    fi

    if [[ -z "${swap_id}" ]]; then
        echo "ERROR: Unable to determine the swap device for crypttab."
        return 1
    fi

    template_render templates/etc/crypttab.j2 0644 swap_device="${swap_device}" || return 1

    add_packages systemd-cryptsetup
}

# The EFI system partitions are automounted on /boot/efi and, on mirrored ESP disks, /boot/efi2,
# where the bootloader writes its files. After b_mount, as systemctl enables the units in the
# target.
b_mount_unit() {
    local efi_uuid efi2_uuid
    efi_uuid=$(blkid -s UUID -o value "$(part "${Q_ESP_DISK_A}" "${PART_ESP}")")

    if [[ -z "${efi_uuid}" ]]; then
        echo "ERROR: Unable to determine UUIDs for fstab."
        return 1
    fi

    template_render templates/etc/systemd/system/boot-efi.mount.j2 0644 efi_uuid="${efi_uuid}" \
        || return 1
    file_deploy deploy/etc/systemd/system/boot-efi.automount || return 1
    in_target systemctl enable boot-efi.automount || return 1

    if [[ "${Q_ESP_REDUNDANT}" != "true" ]]; then
        return 0
    fi

    efi2_uuid=$(blkid -s UUID -o value "$(part "${Q_ESP_DISK_B}" "${PART_ESP}")")

    if [[ -z "${efi2_uuid}" ]]; then
        echo "ERROR: Unable to determine UUIDs for fstab."
        return 1
    fi

    template_render templates/etc/systemd/system/boot-efi2.mount.j2 0644 \
        efi2_uuid="${efi2_uuid}" || return 1
    file_deploy deploy/etc/systemd/system/boot-efi2.automount || return 1
    in_target systemctl enable boot-efi2.automount
}

b_zfs_cache() {
    mkdir -p "${TARGET_DIR}/etc/zfs" || return 1
    cp /etc/zfs/zpool.cache "${TARGET_DIR}/etc/zfs"
}

b_mount() {
    mount --make-private --rbind /dev  "${TARGET_DIR}/dev" || return 1
    mount --make-private --rbind /proc "${TARGET_DIR}/proc" || return 1
    mount --make-private --rbind /sys  "${TARGET_DIR}/sys" || return 1

    # /dev is shared with the live system, so leave its /dev/log alone. Its symlink
    # points at /run/systemd/journal/dev-log, so expose the journal socket there.
    mkdir -p "${TARGET_DIR}/run/systemd/journal" || return 1
    touch "${TARGET_DIR}/run/systemd/journal/dev-log" || return 1
    mount --bind /run/systemd/journal/dev-log "${TARGET_DIR}/run/systemd/journal/dev-log"
}

# The local repository of the live cd, which the hd-* packages come from. It is bound into /run of
# the new system, a tmpfs, so none of it ends up on the disks. Its apt source is trusted, as the
# repository comes with the installer; c_live_repo removes it again.
b_live_repo() {
    if [[ ! -d "${LIVE_REPO_DIR}/dists/${Q_SUITE}" ]]; then
        echo "ERROR: ${LIVE_REPO_DIR} has no suite ${Q_SUITE}. Run the installer from the" \
            "HyprDebian live cd."
        return 1
    fi

    mkdir -p "${TARGET_DIR}${LIVE_REPO_TARGET}" || return 1
    if ! mountpoint -q "${TARGET_DIR}${LIVE_REPO_TARGET}"; then
        mount --bind -o ro "${LIVE_REPO_DIR}" "${TARGET_DIR}${LIVE_REPO_TARGET}" || return 1
    fi

    template_render templates/etc/apt/sources.list.d/hyprdebian-live.sources.j2 0644 \
        live_repo="${LIVE_REPO_TARGET}"
}

# The Debian repositories, next to the local repository of the live cd. While the installer runs,
# apt in the new system installs no recommends and downloads no translations, as hd-base sets for
# the installed system once it is installed; c_live_repo removes the config of the installer again.
b_apt_init() {
    file_deploy deploy/etc/apt/apt.conf.d/00hd-installer || return 1

    template_render templates/etc/apt/sources.list.d/debian.sources.j2 || return 1

    rm -f "${TARGET_DIR}/etc/apt/sources.list" || return 1

    in_target apt update
}

# The debconf selections of the answers file, and the defaults of the packages it has none of when
# Q_DEBCONF_DEFAULTS is true, set in the new system before the packages that ask them are
# installed. apt does not ask a question that is set; b_console reconfigures the packages it asks
# without questions. The config of console-setup overwrites its selections with the defaults of the
# locale while store_defaults_in_debconf_db is true, as it is on a new system, so that is set to
# false when it has selections.
b_debconf() {
    local package lines
    local selections=()

    for package in "${DEBCONF_PACKAGES[@]}"; do
        lines="$(debconf_defaults "${package}")"
        if [[ -n "${lines}" ]]; then
            selections+=("${lines}")
        fi
    done
    if answered DEBCONF; then
        selections+=("${ANSWERS[DEBCONF]}")
    fi
    if debconf_selected console-setup; then
        selections+=("console-setup console-setup/store_defaults_in_debconf_db boolean false")
    fi

    if (( ${#selections[@]} == 0 )); then
        return 0
    fi

    printf '%s\n' "${selections[@]}" | in_target debconf-set-selections
}

# The locales, the time zone, the keyboard and the console font. A package with debconf selections,
# from the answers file or the defaults, is configured with them, without questions.
b_console() {
    local package

    in_target env DEBIAN_FRONTEND=noninteractive apt install -y locales keyboard-configuration \
        console-setup || return 1

    for package in "${DEBCONF_PACKAGES[@]}"; do
        if ! debconf_selected "${package}"; then
            in_target dpkg-reconfigure "${package}" || return 1
            continue
        fi

        # The configuration of tzdata and locales prefers these files over debconf, so they are
        # written again from the selections.
        case "${package}" in
            tzdata)
                rm -f "${TARGET_DIR}/etc/localtime" "${TARGET_DIR}/etc/timezone" || return 1
                ;;
            locales)
                rm -f "${TARGET_DIR}/etc/locale.gen" || return 1
                ;;
        esac
        in_target dpkg-reconfigure -f noninteractive "${package}" || return 1
    done
}

# initramfs-tools with zstd, so every initramfs is compressed with zstd from the start. The kernel
# headers bring the compiler that DKMS builds the ZFS module with.
b_initramfs() {
    # Deployed before the kernel is installed, so no initramfs ever waits for a resume device.
    file_deploy deploy/etc/initramfs-tools/conf.d/resume || return 1

    in_target apt install -y initramfs-tools zstd
}

b_kernel() {
    in_target apt install -y linux-image-amd64 linux-headers-amd64
}

b_zfs_support() {
    in_target env DEBIAN_FRONTEND=noninteractive apt install -y zfs-dkms zfsutils-linux \
        zfs-initramfs
}

# Mounts the EFI system partitions in the target, where the bootloader writes its files in the
# packages phase.
b_esp() {
    mkdir -p "${TARGET_DIR}/boot/efi" || return 1
    if ! mountpoint -q "${TARGET_DIR}/boot/efi"; then
        mount "$(part "${Q_ESP_DISK_A}" "${PART_ESP}")" "${TARGET_DIR}/boot/efi" || return 1
    fi

    if [[ "${Q_ESP_REDUNDANT}" != "true" ]]; then
        return 0
    fi

    mkdir -p "${TARGET_DIR}/boot/efi2" || return 1
    if ! mountpoint -q "${TARGET_DIR}/boot/efi2"; then
        mount "$(part "${Q_ESP_DISK_B}" "${PART_ESP}")" "${TARGET_DIR}/boot/efi2" || return 1
    fi
}

# The firmware is what the hardware needs: hd-firmware of hd-base finds it in the new system, which
# sees the devices of the live system through /sys, with the modules of its own kernel. It runs
# before pkg_esps, so the UKIs that pkg_esps writes have the firmware in their initrd.
b_firmware() {
    add_dependencies "pkg_post" "pkg_firmware"
}

# The base system gets the text-only bootloader. Its postinst
# does not write the ESPs: pkg_esps does that once all packages are installed, with the DKMS MOK
# that zfs-dkms created in b_zfs_support and the Secure Boot keys of hd-base. pkg_boot_entries
# then adds its firmware boot entries.
b_bootloader() {
    add_packages hd-bootloader-txt

    add_packages efibootmgr

    add_dependencies "pkg_post" "pkg_esps" "pkg_boot_entries"
}

b_systemd() {
    add_packages systemd-timesyncd systemd-resolved

    # adm lets the user read the logs, which stay in the systemd journal.
    add_user_groups adm
}

# The packages of every HyprDebian system are the dependencies of hd-base, which also installs the
# files in /etc/skel that root gets in pkg_root_skel and the user in u_skel, the network commands
# that pkg_network uses, and generates the Secure Boot keys that the bootloader signs its files
# with.
b_utilities() {
    add_packages hd-base

    add_dependencies "pkg_post" "pkg_utilities" "pkg_root_skel"
}

# ufw comes with hd-base. pkg_enable_firewall turns it on once it is installed.
b_firewall() {
    add_dependencies "pkg_post" "pkg_enable_firewall"
}

# The network is configured with the hd-network-* commands of hd-base once it is installed, see
# pkg_network. Wifi needs iwd, which also handles the login on the wifi network. The firmware of the
# wifi card comes with the rest, see b_firmware.
b_network() {
    if [[ "${Q_WIFI}" == "true" ]]; then
        add_packages iwd
    fi

    add_dependencies "pkg_post" "pkg_network"
}

add_dependencies "b_main" \
    "b_bootstrap" \
    "b_templates" \
    "b_fstab" \
    "b_mdadm" \
    "b_crypttab" \
    "b_zfs_cache"  \
    "b_mount" \
    "b_mount_unit" \
    "b_live_repo" \
    "b_apt_init" \
    "b_debconf" \
    "b_console" \
    "b_initramfs" \
    "b_kernel" \
    "b_zfs_support" \
    "b_esp" \
    "b_firmware" \
    "b_bootloader" \
    "b_systemd" \
    "b_utilities" \
    "b_firewall" \
    "b_network" \

add_dependencies "install_root" "b_pre" "b_main" "b_post"
