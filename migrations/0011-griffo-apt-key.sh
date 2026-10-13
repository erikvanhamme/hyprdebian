#!/bin/bash

# Older versions used the debian.griffo.io repo for packages that are now in the local hyprdebian
# repo or in Debian. Its source was dropped first, but its key in trusted.gpg.d was deployed up to
# v0.80, so apt still trusts it for every repo. Removes the key, and the source when it is still
# there.
migrate() {
    local key="${TARGET_DIR}/etc/apt/trusted.gpg.d/debian.griffo.io.gpg"
    local list="${TARGET_DIR}/etc/apt/sources.list.d/debian.griffo.io.list"

    # Not needed when both are gone.
    if [[ ! -e "$key" && ! -e "$list" ]]; then
        return 0
    fi

    if [[ -e "$key" ]]; then
        echo "Removing ${key}, the griffo repo is no longer used"
        rm -f "$key" || return 1
    fi

    # Without its key, apt update would fail on this source.
    if [[ -e "$list" ]]; then
        echo "Removing ${list}, the griffo repo is no longer used"
        rm -f "$list" || return 1

        in_target apt update || return 1
    fi
}
