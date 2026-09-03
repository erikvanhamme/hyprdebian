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
    for svc in ${SERVICES}; do
        in_target systemctl enable $svc
    done
}

svc_qemu_kvm() {

    # Ensure these are disabled to not force TCP or TLS listening
    in_target sudo systemctl mask libvirtd-tls.socket libvirtd-tcp.socket
}

add_dependencies "svc_main" \
    "svc_enable" \

add_dependencies "install" "svc_pre" "svc_main" "svc_post"
