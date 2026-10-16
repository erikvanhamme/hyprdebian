#!/bin/bash

pkg_pre() {
    return 0
}

pkg_main() {
    return 0
}

pkg_post() {
    return 0
}

pkg_utilities() {
    in_target apt-file update || return 1
    in_target update-command-not-found
}

# Packages that the deb phase built are in no repo. Their .deb files are copied into the target
# and passed to apt by path, so apt installs them and their dependencies in the same run.
pkg_install() {
    local package deb
    local install=()
    local debs="/tmp/hyprdebian-debs"

    mkdir -p ${TARGET_DIR}${debs} || return 1

    for package in "${PACKAGES[@]}"; do
        deb=$(local_deb "$package")

        if [[ -n "$deb" ]]; then
            cp "$deb" ${TARGET_DIR}${debs}/ || return 1
            install+=("${debs}/$(basename "$deb")")
        else
            install+=("$package")
        fi
    done

    in_target apt install -y "${install[@]}" || return 1
    rm -rf ${TARGET_DIR}${debs}
}

pkg_qemu_kvm() {

    # Add a mask for the secrets file that ships with debian systemd.
    mkdir -p ${TARGET_DIR}/etc/systemd/system/libvirtd.service.d/ || return 1
    ln -sf /dev/null ${TARGET_DIR}/etc/systemd/system/libvirtd.service.d/10-secret.conf
}

pkg_enable_firewall() {
    # Allow SSH before enabling the firewall, so an upgrade over ssh keeps accepting connections.
    if [[ "${Q_OPENSSH}" == "true" ]]; then
        in_target ufw allow SSH || return 1
    fi

    in_target ufw enable || return 1
    in_target ufw logging off
}

add_dependencies "pkg_main" \
    "pkg_install" \

add_dependencies "install" "pkg_pre" "pkg_main" "pkg_post"
