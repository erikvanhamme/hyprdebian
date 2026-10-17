#!/bin/bash
# The cleanup phase: saves what the installed system should know about its installation, takes a
# snapshot of it, unmounts it and exports its pool, so the new system can import it when it boots.

c_pre() {
    return 0
}

c_main() {
    return 0
}

c_post() {
    return 0
}

# Saves the type of the installation in the key hd-installer of /etc/hyprdebian/config.json, for
# the banners of hd-base, and writes the login banner again, as hd-base wrote it in the packages
# phase, before the type was saved. Before c_snapshots, so the snapshot has it.
c_save_config() {
    in_target hd-config-update hd-installer "{\"type\": \"${Q_TYPE}\"}" || return 1
    in_target hd-issue
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
    "c_save_config" \
    "c_snapshots" \
    "c_unmount" \

add_dependencies "install_root" "c_pre" "c_main" "c_post"
