#!/bin/bash

# Older versions installed avahi-daemon and sane-utils with cups, as recommended packages. Both
# listen on the network, and are not needed for printing. o_cups now keeps them out. Removes them
# on systems with cups, unless other packages depend on them.
migrate() {
    local packages=()
    local package removed

    # Not needed without cups, or when the packages are not installed.
    if [[ "${Q_CUPS}" == "false" ]]; then
        return 0
    fi

    for package in avahi-daemon sane-utils; do
        if is_installed "$package"; then
            packages+=("$package")
        fi
    done

    if (( ${#packages[@]} == 0 )); then
        return 0
    fi

    # Removing them would also remove the packages that depend on them, like a desktop environment
    # that uses avahi. Then they are kept, and this migration is done.
    removed=$(in_target apt-get -s remove "${packages[@]}" | awk '$1 == "Remv" { print $2 }') || return 1

    for package in $removed; do
        if [[ " ${packages[*]} " != *" ${package} "* ]]; then
            echo "WARNING: Keeping ${packages[*]}, removing them would also remove: $(echo $removed)"
            return 0
        fi
    done

    echo "Removing ${packages[*]}, cups does not need them"
    in_target apt remove -y "${packages[@]}"
}
