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
    # hd-report reads the version and the answers from this copy. install.sh defines the root task
    # install, so command runs /usr/bin/install instead of that function.
    save_config INSTALLED_VERSION "${INSTALLER_VERSION}"
    command install -D -m 0600 "$CONFIG_FILE" "${TARGET_DIR}${INSTALLED_CONFIG_FILE}"
}

add_dependencies "c_main" \
    "c_save_config" \

add_dependencies "install" "c_pre" "c_main" "c_post"
