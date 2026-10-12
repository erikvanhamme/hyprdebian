#!/bin/bash

# Older versions defined the apt sources in the one-line format: /etc/apt/sources.list for Debian,
# and sources.list.d/hyprdebian.local.list for the local repo. b_apt_init now renders the deb822
# files debian.sources and hyprdebian.local.sources, and removes sources.list.
migrate() {
    local list="${TARGET_DIR}/etc/apt/sources.list"
    local dir="${TARGET_DIR}/etc/apt/sources.list.d"

    # Not needed when the old files are gone.
    if [[ ! -e "$list" && ! -e "$dir/hyprdebian.local.list" ]]; then
        return 0
    fi

    if [[ ! -e "$dir/debian.sources" ]]; then
        template_render templates/etc/apt/sources.list.d/debian.sources.j2 || return 1
    fi

    if [[ "${Q_REPO_ENABLED}" == "true" && ! -e "$dir/hyprdebian.local.sources" ]]; then
        template_render templates/etc/apt/sources.list.d/hyprdebian.local.sources.j2 || return 1
    fi

    # sources.list may have entries of its own, so keep a copy where apt does not read it.
    if [[ -e "$list" ]]; then
        echo "Moving ${list} to ${list}.bak"
        mv "$list" "${list}.bak" || return 1
    fi

    # The installer generated this one completely. A copy in sources.list.d would make apt warn
    # about it on every run. Without its replacement, it is the only source of the local repo.
    if [[ -e "$dir/hyprdebian.local.list" ]]; then
        if [[ -e "$dir/hyprdebian.local.sources" ]]; then
            echo "Removing $dir/hyprdebian.local.list, it is replaced by hyprdebian.local.sources"
            rm -f "$dir/hyprdebian.local.list" || return 1
        else
            echo "WARNING: Keeping $dir/hyprdebian.local.list, the local repo is not enabled in the answers."
        fi
    fi

    in_target apt update
}
