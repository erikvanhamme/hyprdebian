#!/bin/bash

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

    # Stop raid arrays (swap of a previous redundant install) so the disks can be wiped.
    mdadm --stop --scan || true
}

d_wipe() {
    for DISK in ${Q_DISKS}; do
        wipefs -af $DISK || return 1
        sgdisk --zap-all $DISK || return 1
    done
}

d_discard() {
    for DISK in ${Q_DISKS}; do
        blkdiscard -f $DISK || return 1
    done
}

add_dependencies "d_main" \
    "d_unmount_swap" \
    "d_wipe" \
    "d_discard" \

add_dependencies "install" "d_pre" "d_main" "d_post"
