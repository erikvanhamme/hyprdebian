#!/bin/bash

# Older versions of the upgrade installed mdadm on every system, as a package the upgrade itself
# needed. Without an array, the initramfs looks for one on every boot, and the boot waits for it.
# mdadm is only needed for the swap array of redundant drives, like b_mdadm installs it.
migrate() {
    local removed package

    # Needed for the swap array, and not needed when mdadm is not installed.
    if [[ "${Q_REDUNDANT}" == "true" && "${Q_SWAP:-1}" -gt 0 ]]; then
        return 0
    fi

    if ! is_installed mdadm; then
        return 0
    fi

    # An array that is in use, like one the user created, still needs mdadm.
    if grep -q '^md' /proc/mdstat 2>/dev/null; then
        echo "Keeping mdadm, the system has an active array."
        return 0
    fi

    removed=$(in_target apt-get -s purge mdadm | awk '$1 == "Purg" { print $2 }') || return 1

    for package in $removed; do
        if [[ "$package" != "mdadm" ]]; then
            echo "Keeping mdadm, removing it would also remove: $(echo $removed)"
            return 0
        fi
    done

    # Purged, so its generated mdadm.conf without arrays is removed too.
    in_target apt purge -y mdadm || return 1

    in_target update-initramfs -u -k all
}
