#!/bin/bash
# The cleanup phase: takes a snapshot of the installed system, unmounts it and exports its pool, so
# the new system can import it when it boots.

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
    if zfs list -t snapshot rpool/hyprdebian@install &> /dev/null; then
        return 0
    fi

    zfs snapshot rpool/hyprdebian@install
}

c_unmount() {
    unmount_target || return 1
    zpool export rpool
}

add_dependencies "c_main" \
    "c_snapshots" \
    "c_unmount" \

add_dependencies "install_root" "c_pre" "c_main" "c_post"
