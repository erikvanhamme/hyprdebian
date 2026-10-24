#!/bin/bash
# The cleanup phase: removes the local repository of the live cd from the apt sources, empties the
# cache of apt, takes a snapshot of the installed system, unmounts it and exports its pool, so the
# new system can import it when it boots.

c_pre() {
    return 0
}

c_main() {
    return 0
}

c_post() {
    return 0
}

# The new system keeps only the Debian repositories. Its hd-* packages get updates once it is
# linked to a HyprDebian repository with hd-link-repo, see c_repo_note. The apt config of the
# installer goes too: hd-base has the same settings for the installed system.
c_live_repo() {
    rm -f "${TARGET_DIR}/etc/apt/sources.list.d/hyprdebian-live.sources" || return 1
    rm -f "${TARGET_DIR}/etc/apt/apt.conf.d/00hd-installer" || return 1
    in_target apt update
}

# The packages that apt and debootstrap downloaded, and the caches of the package lists, which apt
# makes again when it needs them.
c_apt_clean() {
    in_target apt clean
}

c_snapshots() {
    if zfs list -t snapshot rpool/hyprdebian@install &> /dev/null; then
        return 0
    fi

    zfs snapshot rpool/hyprdebian@install
}

c_unmount() {
    unmount_target || return 1
    zpool export rpool
}

c_repo_note() {
    echo "The new system has no HyprDebian repository. To get updates of the hd-* packages, link it"
    echo "to one after the first boot, as root:"
    echo "    hd-link-repo <url> --suite ${Q_SUITE}"
}

add_dependencies "c_main" \
    "c_live_repo" \
    "c_apt_clean" \
    "c_snapshots" \
    "c_unmount" \
    "c_repo_note" \

add_dependencies "install_root" "c_pre" "c_main" "c_post"
