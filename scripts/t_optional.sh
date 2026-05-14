#!/bin/bash

t_optional() {
    return 0
}

o_wifi() {
    add_packages iwd firmware-iwlwifi
    add_services iwd

    mkdir -p ${TARGET_DIR}/etc/iwd
    write_file ${TARGET_DIR}/etc/iwd/main.conf 0644 <<EOF
[General]
EnableNetworkConfiguration=true
EOF
}

o_desktop() {
    # TODO: Add tasks for full desktop installation and manage dependencies.
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
    add_packages qemu-system-x86 qemu-utils libvirt-daemon-system libvirt-clients bridge-utils ovmf
    if [[ "${Q_DESKTOP}" == "true" ]]; then
        add_packages virt-manager
    fi
    add_user_groups libvirt kvm
    add_services libvirtd
}

o_cups() {
    add_packages cups
    add_user_groups lpadmin
}

add_dependencies "t_install" "t_optional"
