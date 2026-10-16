#!/bin/bash

# Older versions deployed the wallpaper to /etc/skel/Pictures/wallpapers, and o_font unzipped the
# JetBrains Mono Nerd Font release into /usr/local/share/fonts. The packages hyprdebian-art and
# fonts-jetbrainsmono-nerd, which the upgrade installs, replace them. Removes the wallpaper from
# /etc/skel, and the files of the release that the font package does not take over: OFL.txt and
# README.md. The fonts themselves stay, the package installs the same files over them. The copy of
# the wallpaper in the home directory of the user is left alone.
migrate() {
    local skel="${TARGET_DIR}/etc/skel/Pictures"
    local fonts="${TARGET_DIR}/usr/local/share/fonts"

    if [[ -e ${skel}/wallpapers/wallpaper.png ]]; then
        rm -f ${skel}/wallpapers/wallpaper.png || return 1
        rmdir --ignore-fail-on-non-empty ${skel}/wallpapers || return 1
        rmdir --ignore-fail-on-non-empty ${skel} || return 1
    fi

    # Only when o_font unzipped the release there, so files of the same name from elsewhere stay.
    if [[ -e ${fonts}/JetBrainsMonoNerdFont-Regular.ttf ]]; then
        rm -f ${fonts}/OFL.txt ${fonts}/README.md || return 1
    fi
}
