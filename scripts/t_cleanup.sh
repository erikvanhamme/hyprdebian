#!/bin/bash

t_cleanup() {
    return 0
}

tgt_snapshots() {
    in_target zfs snapshot bpool/hyprdebian@install
    in_target zfs snapshot rpool/hyprdebian@install
}

umount_all() {
    umount /mnt/dev/log
    mount | grep -v zfs | tac | awk '/\/mnt/ {print $3}' | xargs -i{} umount -lf {}
    zpool export -a || true
}

add_dependencies "t_cleanup" "tgt_snapshots" "umount_all"
add_dependencies "t_install" "t_cleanup"
