#!/bin/bash

# Older versions did not mask the TCP and TLS sockets of libvirtd, nor the 10-secret.conf drop-in
# of Debian, which makes libvirtd load an encrypted systemd credential. Without a credential key on
# the system, libvirtd fails to start. svc_qemu_kvm and pkg_qemu_kvm add these masks on a new
# installation. They are added whenever libvirt is installed, also when it is only there for
# virt-manager.
migrate() {
    local dropin="${TARGET_DIR}/etc/systemd/system/libvirtd.service.d/10-secret.conf"
    local sockets=(libvirtd-tls.socket libvirtd-tcp.socket)
    local socket
    local needed=false

    # Not needed without libvirt, or when the masks are already there.
    if ! is_installed libvirt-daemon-system; then
        return 0
    fi

    if [[ "$(readlink "$dropin")" != "/dev/null" ]]; then
        needed=true
    fi

    for socket in "${sockets[@]}"; do
        if [[ "$(readlink "${TARGET_DIR}/etc/systemd/system/${socket}")" != "/dev/null" ]]; then
            needed=true
        fi
    done

    if [[ "$needed" != "true" ]]; then
        return 0
    fi

    in_target systemctl stop "${sockets[@]}" || return 1
    in_target systemctl mask "${sockets[@]}" || return 1

    mkdir -p "$(dirname "$dropin")" || return 1
    ln -sf /dev/null "$dropin" || return 1

    in_target systemctl daemon-reload || return 1

    # Start libvirtd again when it is enabled, it may have failed before. A failure here shows
    # that the masks did not fix it.
    if in_target systemctl is-enabled --quiet libvirtd.service; then
        in_target systemctl reset-failed libvirtd.service
        in_target systemctl restart libvirtd.service || return 1
    fi
}
