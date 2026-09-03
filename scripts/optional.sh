#!/bin/bash

o_pre() {
    return 0
}

o_main() {
    return 0
}

o_post() {
    return 0
}

o_wifi() {
    return 0
}

o_firewall() {
    return 0
}

o_font() {
    return 0
}

o_desktop() {
    return 0
}

o_docker() {
    return 0
}

o_qemu_kvm() {
    add_packages qemu-system-x86 qemu-utils libvirt-daemon-system libvirt-clients bridge-utils ovmf

    if [[ "${Q_DESKTOP}" == "true" ]]; then
        add_packages virt-manager
    fi

    add_user_groups libvirt kvm

    add_services libvirtd

    add_dependencies "pkg_post" "pkg_qemu_kvm"
    add_dependencies "svc_post" "svc_qemu_kvm"
}

o_cups() {
    return 0
}

o_rust() {
    return 0
}

o_openssh() {
    return 0
}

add_dependencies "install" "o_pre" "o_main" "o_post"
