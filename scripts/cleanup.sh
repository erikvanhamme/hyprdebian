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
    command install -D -m 0600 "$CONFIG_FILE" "${TARGET_DIR}${INSTALLED_CONFIG_FILE}"
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

add_dependencies "c_main" \
    "c_save_config" \
    "c_save_migrations" \

add_dependencies "install" "c_pre" "c_main" "c_post"
