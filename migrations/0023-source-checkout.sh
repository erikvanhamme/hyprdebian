#!/bin/bash

# Older versions had no checkout of the installer on the system. o_backuptools copied
# blockwrite.c into /usr/src/hyprdebian to compile it. Like u_source on a new installation,
# clones the checkout that runs the upgrade into /usr/src/hyprdebian, owned by the user.
migrate() {
    # Not needed when the checkout is already there.
    if [[ -d ${TARGET_DIR}${SOURCE_DIR}/.git ]]; then
        return 0
    fi

    # Removes the old copy of blockwrite.c. rmdir fails when the directory holds anything else,
    # so the upgrade stops instead of throwing away files it does not know about.
    if [[ -d ${TARGET_DIR}${SOURCE_DIR} ]]; then
        rm -f ${TARGET_DIR}${SOURCE_DIR}/blockwrite.c || return 1
        rmdir ${TARGET_DIR}${SOURCE_DIR} || return 1
    fi

    source_checkout
}
