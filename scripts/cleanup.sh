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

c_save_config() {
    # upgrade.sh reads the answers back from this copy.
    save_config INSTALLED_VERSION "${INSTALLER_VERSION}"
    install -D -m 0600 "$CONFIG_FILE" "${TARGET_DIR}${INSTALLED_CONFIG_FILE}"
}

c_snapshots() {
    in_target zfs snapshot bpool/hyprdebian@install || return 1
    in_target zfs snapshot rpool/hyprdebian@install
}

c_unmount() {
    umount /mnt/run/systemd/journal/dev-log
    mount | grep -v zfs | tac | awk '/\/mnt/ {print $3}' | xargs -i{} umount -lf {}
    zpool export -a || true
}

add_dependencies "c_main" \
    "c_save_config" \
    "c_snapshots" \
    "c_unmount" \

add_dependencies "install" "c_pre" "c_main" "c_post"
