#!/bin/bash

pkg_pre() {
    return 0
}

pkg_main() {
    return 0
}

pkg_post() {
    return 0
}

pkg_utilities() {
    in_target apt-file update || return 1
    in_target update-command-not-found
}

# Packages that the deb phase built are in no repo. Their .deb files are copied into the target
# and passed to apt by path, so apt installs them and their dependencies in the same run.
pkg_install() {
    local package deb
    local install=()
    local debs="/tmp/hyprdebian-debs"

    mkdir -p ${TARGET_DIR}${debs} || return 1

    for package in "${PACKAGES[@]}"; do
        deb=$(local_deb "$package")

        if [[ -n "$deb" ]]; then
            cp "$deb" ${TARGET_DIR}${debs}/ || return 1
            install+=("${debs}/$(basename "$deb")")
        else
            install+=("$package")
        fi
    done

    in_target apt install -y "${install[@]}" || return 1
    rm -rf ${TARGET_DIR}${debs}
}

# Removes the firmware boot entries with the given label, so a rerun does not add the same entry
# twice. efibootmgr lists an entry as "Boot0003* label", with a space for the * when the entry is
# inactive, followed by its device path after a tab.
# Usage: pkg_remove_boot_entries <label>
pkg_remove_boot_entries() {
    local label="$1"
    local entries line number name

    entries=$(in_target efibootmgr) || return 1

    while IFS= read -r line; do
        if [[ ! "$line" =~ ^Boot([0-9A-Fa-f]{4})[*\ ]\ (.*)$ ]]; then
            continue
        fi

        number="${BASH_REMATCH[1]}"
        name="${BASH_REMATCH[2]%%$'\t'*}"

        if [[ "$name" == "$label" ]]; then
            in_target efibootmgr --quiet --delete-bootnum --bootnum "$number" || return 1
        fi
    done <<< "$entries"
}

# Adds a firmware boot entry for the hyprdebian bootloader on the ESP of a disk, partition 1, where
# its postinst wrote EFI/BOOT/BOOTX64.EFI. efibootmgr puts a new entry first in the boot order.
# Usage: pkg_boot_entry <disk> <label>
pkg_boot_entry() {
    local disk="$1"
    local label="$2"

    pkg_remove_boot_entries "$label" || return 1

    in_target efibootmgr --quiet --create --disk "$(readlink -f "$disk")" --part 1 --label "$label" \
        --loader '\EFI\BOOT\BOOTX64.EFI'
}

# The entries of the hyprdebian bootloader, named like the grub entries of b_grub2. The secondary
# entry is added first, so the primary one ends up first in the boot order.
pkg_boot_entries() {
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        pkg_boot_entry "${Q_DISK_B}" "HyprDebian (secondary)" || return 1
        pkg_boot_entry "${Q_DISK_A}" "HyprDebian (primary)"
    else
        pkg_boot_entry "${Q_DISK}" "HyprDebian"
    fi
}

pkg_qemu_kvm() {

    # Add a mask for the secrets file that ships with debian systemd.
    mkdir -p ${TARGET_DIR}/etc/systemd/system/libvirtd.service.d/ || return 1
    ln -sf /dev/null ${TARGET_DIR}/etc/systemd/system/libvirtd.service.d/10-secret.conf
}

pkg_enable_firewall() {
    # Allow SSH before enabling the firewall, so an upgrade over ssh keeps accepting connections.
    if [[ "${Q_OPENSSH}" == "true" ]]; then
        in_target ufw allow SSH || return 1
    fi

    in_target ufw enable || return 1
    in_target ufw logging off
}

add_dependencies "pkg_main" \
    "pkg_install" \

add_dependencies "install" "pkg_pre" "pkg_main" "pkg_post"
