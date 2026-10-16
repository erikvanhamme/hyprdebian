#!/bin/bash

deb_pre() {
    return 0
}

deb_main() {
    return 0
}

deb_post() {
    return 0
}

# Runs a command in the checkout as the owner of the checkout, so the build directories in it stay
# owned by that user and not by root.
# Usage: deb_as_owner <command> [<argument> ...]
deb_as_owner() {
    local owner

    owner=$(stat -c %U .) || return 1
    runuser -u "$owner" -- "$@"
}

# Builds a package with an art script and copies it to DEB_DIR. Old builds of the package are
# removed first, so only the new one is copied.
# Usage: deb_art_package <script in art/> <package name>
deb_art_package() {
    local script="$1"
    local name="$2"

    deb_as_owner rm -f art/build/${name}_*_all.deb || return 1
    deb_as_owner "art/${script}" || return 1
    cp art/build/${name}_*_all.deb "$DEB_DIR"/
}

deb_dir() {
    mkdir -p "$DEB_DIR"
}

# make clean leaves only the new packages in bootloader/build.
deb_bootloader() {
    deb_as_owner make -C bootloader clean deb || return 1
    cp bootloader/build/*.deb "$DEB_DIR"/
}

deb_art() {
    deb_art_package create-deb.sh hyprdebian-art
}

deb_font() {
    deb_art_package create-font-deb.sh fonts-jetbrainsmono-nerd
}

add_dependencies "deb_main" \
    "deb_dir" \
    "deb_bootloader" \
    "deb_art" \
    "deb_font" \

add_dependencies "install" "deb_pre" "deb_main" "deb_post"
