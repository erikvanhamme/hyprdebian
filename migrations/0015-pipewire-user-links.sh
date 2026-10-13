#!/bin/bash

# Older versions enabled pipewire and wireplumber for the user with links in
# ~/.config/systemd/user/default.target.wants, made from outside the target system, so they point
# into /mnt/usr/lib/systemd/user and are broken. The pipewire packages enable their user services
# themselves, so the installer no longer makes these links. Removes the broken links.
migrate() {
    local dir="${TARGET_DIR}/home/${Q_USER}/.config/systemd/user/default.target.wants"
    local name link

    for name in pipewire.service wireplumber.service; do
        link="$dir/$name"

        # Not needed when the link is gone, or does not point into /mnt.
        if [[ ! -L "$link" ]]; then
            continue
        fi

        if [[ "$(readlink "$link")" != /mnt/* ]]; then
            continue
        fi

        echo "Removing the broken link ${link}"
        rm -f "$link" || return 1
    done

    # The installer created this directory only for these links.
    if [[ -d "$dir" ]]; then
        rmdir --ignore-fail-on-non-empty "$dir"
    fi
}
