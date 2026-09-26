#!/bin/bash

svc_pre() {
    return 0
}

svc_main() {
    return 0
}

svc_post() {
    return 0
}

svc_enable() {
    for svc in ${SERVICES[*]}; do
        in_target systemctl enable $svc
    done
}

svc_systemd() {
    ln -sf /run/systemd/resolve/resolv.conf ${TARGET_DIR}/etc/resolv.conf

    in_target systemctl mask systemd-firstboot.service systemd-preset-all.service systemd-machine-id-commit.service rpcbind.socket rpcbind.service 
}

svc_qemu_kvm() {

    # Ensure these are disabled to not force TCP or TLS listening
    in_target systemctl mask libvirtd-tls.socket libvirtd-tcp.socket
}

svc_backuptools() {

    # Make sure iscsi target is not started automatically, it should happen on demand.
    in_target systemctl disable iscsid.service
}

add_dependencies "svc_main" \
    "svc_enable" \
    "svc_systemd" \

add_dependencies "install" "svc_pre" "svc_main" "svc_post"
