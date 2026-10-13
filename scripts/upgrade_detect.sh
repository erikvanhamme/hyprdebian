#!/bin/bash

# Works out the answers of an installation from the state of the system, for systems that were
# installed before the installer saved its config. Only answers that can be found are saved, the
# upgrade asks every question with them filled in.

# Saves a detected answer, when one was found.
# Usage: save_detected <var> <value>
save_detected() {
    if [[ -n "$2" ]]; then
        save_config "$1" "$2"
    fi
}

# Prints "true" when the command succeeds, and "false" otherwise.
# Usage: detected_bool <command>...
detected_bool() {
    if "$@"; then
        echo "true"
    else
        echo "false"
    fi
}

# Prints the interface of the first systemd-networkd file that exists of the given files.
# Usage: network_name <file>...
network_name() {
    sed -n 's/^Name=//p' "$@" 2>/dev/null | head -n 1
}

# Succeeds when the user is a member of the group.
# Usage: in_group <user> <group>
in_group() {
    id -nG "$1" 2>/dev/null | tr ' ' '\n' | grep -qxF "$2"
}

# Succeeds when the root pool is mirrored over two disks.
has_mirror() {
    zpool status rpool 2>/dev/null | grep -qw mirror
}

# Succeeds when the root pool is encrypted.
has_encryption() {
    [[ "$(zfs get -H -o value encryption rpool 2>/dev/null)" =~ ^aes ]]
}

# Succeeds when Qemu/KVM was enabled. Its packages alone can be misleading, virt-manager is also
# used to manage other machines. The bridge or the libvirt group of the user show it was enabled.
# Usage: has_qemu_kvm <user>
has_qemu_kvm() {
    [[ -e "${TARGET_DIR}/etc/systemd/network/30-br0.netdev" ]] || in_group "$1" libvirt
}

# Succeeds when the backup tools were enabled: their scripts or their tape tools are there.
has_backuptools() {
    [[ -e "${TARGET_DIR}/usr/local/bin/hd-backup" ]] || { is_installed mbuffer && is_installed mt-st; }
}

detect_config() {
    local net="${TARGET_DIR}/etc/systemd/network"
    local repo_file="${TARGET_DIR}/etc/apt/sources.list.d/hyprdebian.local.sources"
    local repo_list="${TARGET_DIR}/etc/apt/sources.list.d/hyprdebian.local.list"
    local user swap type suite

    # Older versions used /etc/apt/sources.list: "deb <url> <suite> <components>".
    suite=$(sed -n 's/^Suites: *\([^ ]*\).*/\1/p' "${TARGET_DIR}/etc/apt/sources.list.d/debian.sources" 2>/dev/null | head -n 1) || true
    if [[ -z "$suite" ]]; then
        suite=$(awk '$1 == "deb" && $2 !~ /^\[/ {print $3; exit}' "${TARGET_DIR}/etc/apt/sources.list" 2>/dev/null) || true
    fi

    # The questions check for unstable, not for its codename. q_suite is not run by the upgrade,
    # so the suite must always be saved.
    if [[ -z "$suite" || "$suite" == "sid" ]]; then
        suite="unstable"
    fi

    save_detected Q_SUITE "$suite"

    case "$(hostnamectl chassis 2>/dev/null)" in
        laptop|convertible|tablet) type="laptop" ;;
        server) type="server" ;;
        vm|container) type="vm" ;;
        *) type="desktop" ;;
    esac

    save_detected Q_TYPE "$type"

    # The disks.
    save_detected Q_REDUNDANT "$(detected_bool has_mirror)"
    save_detected Q_ENCRYPT "$(detected_bool has_encryption)"

    # The size of the swap partition in GiB, rounded. zram swap is not a partition.
    swap=$(awk 'NR > 1 && $1 !~ /zram/ {print int(($3 + 524288) / 1048576); exit}' /proc/swaps 2>/dev/null) || true
    save_detected Q_SWAP "${swap:-0}"

    # The user is the first regular user with a home directory in /home.
    user=$(getent passwd | awk -F: '$3 >= 1000 && $3 < 60000 && $6 ~ "^/home/" {print $3 ":" $1}' | sort -n | head -n 1 | cut -d: -f2) || true
    save_detected Q_USER "$user"

    if [[ -n "$user" ]]; then
        save_detected Q_FULLNAME "$(getent passwd "$user" | cut -d: -f5 | cut -d, -f1)"
    fi

    save_detected Q_HOSTNAME "$(head -n 1 "${TARGET_DIR}/etc/hostname" 2>/dev/null)"
    save_detected Q_FQDN "$(awk '$1 == "127.0.1.1" {print $2; exit}' "${TARGET_DIR}/etc/hosts" 2>/dev/null)"

    if is_installed linux-image-amd64; then
        save_detected Q_SPECIFIC_KERNEL "false"
        save_detected Q_KERNEL "latest"
    else
        save_detected Q_SPECIFIC_KERNEL "true"
        save_detected Q_KERNEL "$(uname -r | cut -d+ -f1)"
    fi

    # The network. Older versions named the network files after the interface.
    save_detected Q_IFACE "$(network_name "$net"/50-*.network)"
    save_detected Q_WIFI "$(detected_bool is_installed iwd)"
    save_detected Q_WIFACE "$(network_name "$net"/60-*)"

    # The optional components.
    save_detected Q_DESKTOP "$(detected_bool is_installed hyprland)"
    save_detected Q_FIREWALL "$(detected_bool is_installed ufw)"
    save_detected Q_DOCKER "$(detected_bool is_installed docker.io)"
    save_detected Q_QEMU_KVM "$(detected_bool has_qemu_kvm "$user")"
    save_detected BR0_MAC_ADDRESS "$(sed -n 's/^MACAddress=//p' "$net/30-br0.netdev" 2>/dev/null | head -n 1)"
    save_detected Q_CUPS "$(detected_bool is_installed cups)"
    save_detected Q_BACKUPTOOLS "$(detected_bool has_backuptools)"
    save_detected Q_RUST "$(detected_bool is_installed rustup)"
    save_detected Q_OPENSSH "$(detected_bool is_installed openssh-server)"

    # Older versions used the one-line format: "deb [trusted=yes] <url> sid main".
    if [[ -e "$repo_file" ]]; then
        save_detected Q_REPO_ENABLED "true"
        save_detected Q_REPO "$(sed -n 's/^URIs: *//p' "$repo_file" | head -n 1)"
    elif [[ -e "$repo_list" ]]; then
        save_detected Q_REPO_ENABLED "true"
        save_detected Q_REPO "$(grep -o 'https\?://[^ ]*' "$repo_list" | head -n 1)"
    else
        save_detected Q_REPO_ENABLED "false"
    fi
}
