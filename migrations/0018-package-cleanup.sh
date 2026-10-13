#!/bin/bash

# Older versions installed packages that the installer no longer installs for every system:
# qemu-guest-agent on all machine types, yazi also on console systems, and build tools in
# b_buildtools. The user may use them, so the upgrade asks before removing them. The packages the
# new version installs for this system are already worked out when the migrations run, so a
# package that is still in that queue is kept.

# Asks whether to remove packages, and removes them when the answer is yes. A package that the new
# version installs, or that is not installed, is left out. When removing them would also remove
# other packages, they are kept.
# Usage: offer_removal <reason> <package>...
offer_removal() {
    local reason="$1"
    shift
    local packages=()
    local package removed answer

    for package in "$@"; do
        if [[ " ${PACKAGES[*]} " == *" ${package} "* ]]; then
            continue
        fi

        if is_installed "$package"; then
            packages+=("$package")
        fi
    done

    if (( ${#packages[@]} == 0 )); then
        return 0
    fi

    removed=$(in_target apt-get -s remove "${packages[@]}" | awk '$1 == "Remv" { print $2 }') || return 1

    for package in $removed; do
        if [[ " ${packages[*]} " != *" ${package} "* ]]; then
            echo "Keeping ${packages[*]}, removing them would also remove: $(echo $removed)"
            return 0
        fi
    done

    while true; do
        read -rp "${packages[*]}: ${reason} Remove (asked only once)? [y/N]: " answer || return 1

        case "$answer" in
            [Yy]*)
                in_target apt remove -y "${packages[@]}"
                return
                ;;
            [Nn]*|"")
                return 0
                ;;
            *)
                echo "Please answer y or n."
                ;;
        esac
    done
}

migrate() {
    offer_removal "Only needed in a virtual machine." qemu-guest-agent || return 1
    offer_removal "Only installed with the Hyprland desktop now." yazi || return 1
    offer_removal "Build tools the installer no longer installs." cmake meson ninja-build pkg-config
}
