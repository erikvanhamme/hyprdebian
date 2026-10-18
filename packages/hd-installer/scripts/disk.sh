#!/bin/bash
# The disk phase: frees the disks picked in the questions and wipes them.

d_pre() {
    return 0
}

d_main() {
    return 0
}

d_post() {
    return 0
}

d_unmount_swap() {
    swapoff -a || return 1

    # Stop raid arrays (swap of a previous mirrored install) so the disks can be wiped.
    mdadm --stop --scan || true
}

# Unmounts everything below the target, such as the mounts of an earlier attempt, which would keep
# its pool busy.
d_unmount_target() {
    unmount_target
}

# Exports the ZFS pools of the live system, such as the pool of an earlier attempt, so their
# disks can be wiped.
d_export_pools() {
    zpool export -a
}

d_wipe() {
    local disks disk

    all_disks disks
    for disk in "${disks[@]}"; do
        wipefs -af "${disk}" || return 1
        sgdisk --zap-all "${disk}" || return 1
    done
}

# Discards the whole disk, which tells an SSD that all its blocks are free. A disk without
# discard, such as a hardware RAID volume, is left as it is.
d_discard() {
    local disks disk

    all_disks disks
    for disk in "${disks[@]}"; do
        if ! can_discard "${disk}"; then
            echo "${disk} does not support discard."
            continue
        fi
        blkdiscard -f "${disk}" || return 1
    done
}

add_dependencies "d_main" \
    "d_unmount_swap" \
    "d_unmount_target" \
    "d_export_pools" \
    "d_wipe" \
    "d_discard" \

add_dependencies "install_root" "d_pre" "d_main" "d_post"
