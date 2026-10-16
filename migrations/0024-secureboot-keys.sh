#!/bin/bash

# Older versions did not generate Secure Boot keys. Like b_secureboot and pkg_secureboot_keys on a
# new installation, installs the packages to make and use them, then generates the PK, KEK and db
# keys in /etc/hyprdebian/secureboot, which the hyprdebian bootloader signs its files with.
migrate() {
    # Not needed when the keys are already there. pkg_secureboot_keys never replaces them either.
    if [[ -d ${TARGET_DIR}${SECUREBOOT_DIR} ]]; then
        return 0
    fi

    in_target apt install --no-upgrade openssl efitools sbsigntool uuid-runtime || return 1

    pkg_secureboot_keys
}
