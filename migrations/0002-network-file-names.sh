#!/bin/bash

# Older versions named the network files after the interface: 50-<interface>.network and
# 60-<wifi interface>.network. The templates now render 50-ethx.network and 60-wlanx.network,
# and systemd-networkd only uses the first file that matches an interface.

# Moves an old network file to its new name, so the upgrade compares it with the new template.
# When the new file is already there, the old one is removed.
# Usage: migrate_network_file <old file> <new file>
migrate_network_file() {
    local old="$1"
    local new="$2"

    if [[ "$old" == "$new" || ! -e "$old" ]]; then
        return 0
    fi

    if [[ -e "$new" ]]; then
        echo "Removing ${old}, it is replaced by ${new}"
        rm -f "$old"
    else
        echo "Moving ${old} to ${new}"
        mv "$old" "$new"
    fi
}

migrate() {
    local net="${TARGET_DIR}/etc/systemd/network"

    if [[ -n "${Q_IFACE:-}" ]]; then
        migrate_network_file "$net/50-${Q_IFACE}.network" "$net/50-ethx.network" || return 1
    fi

    if [[ "${Q_WIFI}" == "true" && -n "${Q_WIFACE:-}" ]]; then
        migrate_network_file "$net/60-${Q_WIFACE}.network" "$net/60-wlanx.network" || return 1
    fi
}
