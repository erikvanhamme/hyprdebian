#!/bin/bash

# Older versions deployed the apt hook that runs hd-sync-efi to /etc/apt/conf.d/99sync-efi, which apt
# does not read, and let it call /usr/local/sbin/hd-sync-efi, which does not exist. The secondary
# EFI partition was never synchronized. q_redundant now queues the hook in /etc/apt/apt.conf.d,
# which the upgrade adds, together with rsync, which hd-sync-efi needs. Removes the old hook.
migrate() {
    local old="${TARGET_DIR}/etc/apt/conf.d/99sync-efi"

    # Not needed when the old hook is gone.
    [[ -e "$old" ]] || return 0

    echo "Removing ${old}, it is replaced by /etc/apt/apt.conf.d/99sync-efi"
    rm -f "$old" || return 1

    # The installer created /etc/apt/conf.d only for this file.
    rmdir --ignore-fail-on-non-empty "$(dirname "$old")"
}
