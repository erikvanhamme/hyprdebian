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

pkg_install() {
    in_target apt install -y ${PACKAGES[*]}
}

pkg_qemu_kvm() {

    # Make sure this file is gone to ensure that libvirtd can run without the systemd encryption BS.
    rm ${TARGET_DIR}/usr/lib/systemd/system/libvirtd.service.d/10-secret.conf
}

add_dependencies "pkg_main" \
    "pkg_install" \

add_dependencies "install" "pkg_pre" "pkg_main" "pkg_post"
