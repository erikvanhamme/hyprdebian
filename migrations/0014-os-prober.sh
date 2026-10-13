#!/bin/bash

# Older versions installed os-prober with grub, as a recommended package. It mounts the partitions
# of other disks to look for other operating systems on every update-grub, which a hyprdebian system
# does not have. b_grub2 now keeps it out, and 00-block-unwanted pins it so it is never installed.
# Removes it, unless other packages depend on it.
migrate() {
    local package removed

    # Not needed when os-prober is not installed.
    if ! is_installed os-prober; then
        return 0
    fi

    # Removing it would also remove the packages that depend on it. Then it is kept, and this
    # migration is done.
    removed=$(in_target apt-get -s remove os-prober | awk '$1 == "Remv" { print $2 }') || return 1

    for package in $removed; do
        if [[ "$package" != "os-prober" ]]; then
            echo "WARNING: Keeping os-prober, removing it would also remove: $(echo $removed)"
            return 0
        fi
    done

    echo "Removing os-prober, the system does not need it"
    in_target apt remove -y os-prober
}
