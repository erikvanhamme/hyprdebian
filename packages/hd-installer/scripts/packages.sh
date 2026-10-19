#!/bin/bash
# The packages phase: installs the packages that the other phases marked with add_packages, from
# the Debian and HyprDebian repositories, then runs the steps that the other phases added to
# pkg_post.

pkg_pre() {
    return 0
}

pkg_main() {
    return 0
}

pkg_post() {
    return 0
}

pkg_install() {
    in_target apt install -y "${PACKAGES[@]}"
}

pkg_utilities() {
    in_target apt-file update || return 1
    in_target update-command-not-found
}

# Gives root the files in /etc/skel, such as the aliases and the nano config of hd-base, as the
# user gets them in u_skel.
pkg_root_skel() {
    in_target cp -a /etc/skel/. /root/
}

# Turns on the firewall, which lets in ssh and blocks the rest. ssh is allowed before the firewall
# is turned on, so an installation over ssh keeps its connection. --force skips the question
# whether to go on, which ufw asks when it may cut off ssh.
pkg_enable_firewall() {
    in_target ufw allow SSH || return 1
    in_target ufw --force enable || return 1
    in_target ufw logging off
}

# Configures the network with the network commands of hd-base: the wired interface, and the wifi
# interface with a higher route metric, so the wired one is preferred, both with DHCP. With wifi,
# the boot does not wait for the network, which may not be in reach.
pkg_network() {
    if [[ -n "${Q_IFACE}" ]]; then
        in_target hd-network-iface set "${Q_IFACE}" || return 1
    fi

    if [[ "${Q_WIFI}" == "true" ]]; then
        in_target hd-network-iface set "${Q_WIFACE}" --metric 200 || return 1
        in_target systemctl mask systemd-networkd-wait-online.service || return 1
    fi

    in_target hd-network-render || return 1
    in_target systemctl enable systemd-networkd
}

# Writes the bootloader, its assets and the UKIs to the EFI system partitions, which b_esp mounted
# in the target.
pkg_esps() {
    in_target /usr/lib/hd-bootloader/update-esps
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
        if [[ ! "${line}" =~ ^Boot([0-9A-Fa-f]{4})[*\ ]\ (.*)$ ]]; then
            continue
        fi

        number="${BASH_REMATCH[1]}"
        name="${BASH_REMATCH[2]%%$'\t'*}"

        if [[ "${name}" == "${label}" ]]; then
            in_target efibootmgr --quiet --delete-bootnum --bootnum "${number}" || return 1
        fi
    done <<< "${entries}"
}

# Adds a firmware boot entry for the HyprDebian bootloader on the EFI system partition of a disk,
# where pkg_esps wrote EFI/BOOT/BOOTX64.EFI. efibootmgr puts a new entry first in the boot order.
# Usage: pkg_boot_entry <disk> <label>
pkg_boot_entry() {
    local disk="$1"
    local label="$2"

    pkg_remove_boot_entries "${label}" || return 1

    in_target efibootmgr --quiet --create --disk "$(readlink -f "${disk}")" --part "${PART_ESP}" \
        --label "${label}" --loader '\EFI\BOOT\BOOTX64.EFI'
}

# The entries of the HyprDebian bootloader. The secondary entry is added first, so the primary one
# ends up first in the boot order.
pkg_boot_entries() {
    if [[ "${Q_ESP_REDUNDANT}" == "true" ]]; then
        pkg_boot_entry "${Q_ESP_DISK_B}" "HyprDebian (secondary)" || return 1
        pkg_boot_entry "${Q_ESP_DISK_A}" "HyprDebian (primary)"
    else
        pkg_boot_entry "${Q_ESP_DISK_A}" "HyprDebian"
    fi
}

add_dependencies "pkg_main" \
    "pkg_install" \

add_dependencies "install_root" "pkg_pre" "pkg_main" "pkg_post"
