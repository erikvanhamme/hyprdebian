#!/bin/bash

c_pre() {
    return 0
}

c_main() {
    return 0
}

c_post() {
    return 0
}

c_snapshots() {
    in_target zfs snapshot bpool/hyprdebian@install
    in_target zfs snapshot rpool/hyprdebian@install
}

c_unmount() {
    umount /mnt/dev/log
    mount | grep -v zfs | tac | awk '/\/mnt/ {print $3}' | xargs -i{} umount -lf {}
    zpool export -a || true
}

add_dependencies "c_main" \
    "c_snapshots" \
    "c_unmount" \

add_dependencies "install" "c_pre" "c_main" "c_post"
