#!/bin/bash

u_pre() {
    return 0
}

u_main() {
    return 0
}

u_post() {
    return 0
}

u_source() {
    # git clone refuses a directory that is not empty, so a rerun starts from scratch.
    if [[ -e ${TARGET_DIR}${SOURCE_DIR} ]]; then
        rm -rf ${TARGET_DIR}${SOURCE_DIR} || return 1
    fi

    source_checkout
}

add_dependencies "u_main" \
    "u_source" \

add_dependencies "install" "u_pre" "u_main" "u_post"
