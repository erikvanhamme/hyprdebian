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
    add_packages ufw
    add_dependencies "pkg_post" "o_enable_firewall"
}

o_enable_firewall() {
    in_target ufw enable
    
    if [[ "${Q_OPENSSH}" == "true" ]]; then
        in_target ufw allow SSH
    fi
}

o_font() {
    return 0
}

o_desktop() {
    return 0
}

o_docker() {
    add_packages docker.io
    add_user_groups docker

    mkdir -p ${TARGET_DIR}/etc/docker
    write_file ${TARGET_DIR}/etc/docker/daemon.json 0644 <<EOF
{
  "bip": "192.168.253.1/24"
}
EOF
}

o_qemu_kvm() {
    add_packages qemu-system-x86-headless qemu-system-gui- qemu-utils libvirt-daemon-system libvirt-clients bridge-utils ovmf

    if [[ "${Q_DESKTOP}" == "true" ]]; then
        add_packages virt-manager
    fi

    add_user_groups libvirt kvm

    add_services libvirtd

    add_dependencies "pkg_post" "pkg_qemu_kvm"
    add_dependencies "svc_post" "svc_qemu_kvm"
}

o_cups() {
    add_packages cups
    add_user_groups lpadmin
}

o_rust() {
    return 0
}

o_openssh() {
    add_packages openssh-server kitty-terminfo
}

add_dependencies "install" "o_pre" "o_main" "o_post"
