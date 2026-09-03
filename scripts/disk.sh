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
    swapoff -a
}

d_wipe() {
    for DISK in ${Q_DISKS}; do
        wipefs -af $DISK
        sgdisk --zap-all $DISK
    done
}

d_discard() {
    for DISK in ${Q_DISKS}; do
        blkdiscard -f $DISK
    done
}

add_dependencies "d_main" \
    "d_unmount_swap" \
    "d_wipe" \
    "d_discard" \

add_dependencies "install" "d_pre" "d_main" "d_post"
