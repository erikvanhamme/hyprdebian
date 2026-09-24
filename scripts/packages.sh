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
    in_target apt-file update
    in_target update-command-not-found
}

pkg_install() {
    in_target apt install -y ${PACKAGES[*]}
}

pkg_qemu_kvm() {

    # Add a mask for the secrets file that ships with debian systemd.
    mkdir -p ${TARGET_DIR}/etc/systemd/system/libvirtd.service.d/
    ln -sf /dev/null ${TARGET_DIR}/etc/systemd/system/libvirtd.service.d/10-secret.conf
}

pkg_systemd() {
    ln -sf /run/systemd/resolve/resolv.conf ${TARGET_DIR}/etc/resolv.conf
}

add_dependencies "pkg_main" \
    "pkg_install" \

add_dependencies "install" "pkg_pre" "pkg_main" "pkg_post"
