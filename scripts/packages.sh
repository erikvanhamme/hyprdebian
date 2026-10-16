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

# Generates the Secure Boot keys of the system in SECUREBOOT_DIR: the platform key (PK), the key
# exchange key (KEK) and the signature database key (db), all with the same owner GUID (GUID.txt).
# Each has its private key (.key), its certificate as PEM (.crt) for sbsign and as DER (.cer) for
# the key menus of firmware setups, an EFI signature list (.esl) and a signed update (.auth) for
# efi-updatevar or KeyTool: PK signs its own and that of KEK, KEK signs that of db. The bootloader,
# assets.efi and the UKIs are signed with the db key. RSA 2048 is the key size that the UEFI spec
# requires every firmware to support.
#
# The keys are made in a directory of their own that only root can enter, and moved into place
# when they are all there. Existing keys are kept, so a rerun or an upgrade never replaces keys
# that may already be enrolled in the firmware.
pkg_secureboot_keys() {
    local dir="${SECUREBOOT_DIR}"
    local tmp="${SECUREBOOT_DIR}.tmp"
    local guid name

    if [[ -d ${TARGET_DIR}${dir} ]]; then
        return 0
    fi

    rm -rf ${TARGET_DIR}${tmp} || return 1
    mkdir -p "$(dirname ${TARGET_DIR}${dir})" || return 1
    mkdir -m 0700 ${TARGET_DIR}${tmp} || return 1

    guid=$(in_target uuidgen --random) || return 1
    echo "$guid" > ${TARGET_DIR}${tmp}/GUID.txt || return 1

    for name in PK KEK db; do
        in_target openssl req -new -x509 -newkey rsa:2048 -nodes -sha256 -days 3650 \
            -subj "/CN=HyprDebian ${Q_HOSTNAME} Secure Boot ${name}/" \
            -keyout ${tmp}/${name}.key -out ${tmp}/${name}.crt || return 1
        in_target openssl x509 -in ${tmp}/${name}.crt -outform DER -out ${tmp}/${name}.cer || return 1
        in_target cert-to-efi-sig-list -g "$guid" ${tmp}/${name}.crt ${tmp}/${name}.esl || return 1
    done

    in_target sign-efi-sig-list -g "$guid" -k ${tmp}/PK.key -c ${tmp}/PK.crt \
        PK ${tmp}/PK.esl ${tmp}/PK.auth || return 1
    in_target sign-efi-sig-list -g "$guid" -k ${tmp}/PK.key -c ${tmp}/PK.crt \
        KEK ${tmp}/KEK.esl ${tmp}/KEK.auth || return 1
    in_target sign-efi-sig-list -g "$guid" -k ${tmp}/KEK.key -c ${tmp}/KEK.crt \
        db ${tmp}/db.esl ${tmp}/db.auth || return 1

    chown -R 0:0 ${TARGET_DIR}${tmp} || return 1
    chmod 0600 ${TARGET_DIR}${tmp}/* || return 1
    mv ${TARGET_DIR}${tmp} ${TARGET_DIR}${dir}
}

# The postinst of the bootloader package wrote the bootloader, assets.efi and the UKIs to the ESPs
# in pkg_install, before pkg_secureboot_keys generated the keys, so they are unsigned. Runs the
# same update again, which now signs them with the db key, and leaves them unchanged otherwise.
pkg_sign_esps() {
    in_target /usr/lib/hyprdebian/bootloader/update-esps.sh
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
