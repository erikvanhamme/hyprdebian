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
    # upgrade.sh reads the answers back from this copy. install.sh defines the root task install,
    # so command runs /usr/bin/install instead of that function.
    save_config INSTALLED_VERSION "${INSTALLER_VERSION}"
    command install -D -m 0600 "$CONFIG_FILE" "${TARGET_DIR}${INSTALLED_CONFIG_FILE}" || return 1

    # The console shows the installed version above the login prompt.
    write_issue
}

c_save_migrations() {
    # A new installation already has the changes of all migrations, so they must never run on it.
    local file

    mkdir -p "$(dirname "${TARGET_DIR}${MIGRATIONS_FILE}")" || return 1

    for file in migrations/*.sh; do
        if [[ -e "$file" ]]; then
            basename "$file" .sh
        fi
    done > "${TARGET_DIR}${MIGRATIONS_FILE}"
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

c_mok_notice() {
    echo "On the first boot, MokManager asks to enroll the key of the ZFS module, for Secure Boot."
    echo "Press a key, then choose \"Enroll MOK\", \"Continue\" and \"Yes\", enter the password"
    echo "${MOK_PASSWORD} and choose \"Reboot\". The password is also saved in ${INSTALLED_CONFIG_FILE}."
}

c_mok_password() {
    # Shown once more as the very last step, so it is on screen when the installer ends.
    echo "MOK enrollment password: ${MOK_PASSWORD}"
}

add_dependencies "c_main" \
    "c_save_config" \
    "c_save_migrations" \
    "c_snapshots" \
    "c_unmount" \
    "c_mok_notice" \

add_dependencies "c_post" \
    "c_mok_password" \

add_dependencies "install" "c_pre" "c_main" "c_post"
