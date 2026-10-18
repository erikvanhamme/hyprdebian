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

o_firewall() {
    if [[ "${Q_QEMU_KVM}" == "true" ]]; then
        add_files \
            "deploy/etc/modules-load.d/br_netfilter.conf" \
            "deploy/etc/sysctl.d/50-bridge-netfilter.conf" \

    fi
}

o_qemu_kvm() {
    add_packages qemu-system-x86 qemu-system-gui- qemu-utils libvirt-daemon-system libvirt-clients bridge-utils ovmf

    if [[ "${Q_DESKTOP}" == "true" ]]; then
        add_packages virt-manager
    fi

    if [[ "${Q_QEMU_KVM}" == "true" ]]; then
        add_files \
            deploy/etc/skel/.config/libvirt/libvirt.conf \

    fi

    add_user_groups libvirt kvm

    add_services libvirtd

    add_files \
        deploy/etc/libvirt/libvirtd.conf \
        deploy/etc/libvirt/secret.conf \
        deploy/etc/systemd/network/40-br0.network \

    add_dependencies "pkg_post" "pkg_qemu_kvm"
    add_dependencies "svc_post" "svc_qemu_kvm"

    # Keep an existing address, so a rerun does not change the MAC of the bridge.
    if [[ -z "${BR0_MAC_ADDRESS:-}" ]]; then
        BR0_MAC_ADDRESS=$(random_mac)
        save_config BR0_MAC_ADDRESS ${BR0_MAC_ADDRESS}
    fi

    add_template templates/etc/systemd/network/30-br0.netdev.j2
}

o_cups() {
    add_packages cups avahi-daemon- sane-utils-
    add_user_groups lpadmin
}

o_backuptools() {
    add_dependencies "svc_post" "svc_backuptools"

    add_packages python-is-python3 python3-colorama python3-tabulate mbuffer mt-st open-iscsi

    add_files \
        deploy/etc/systemd/system/iscsi-login.service \
        deploy/etc/udev/rules.d/99-iscsi-custom.rules \
        deploy/usr/local/bin/hd-backup \
        deploy/usr/local/bin/hd-backup-load \
        deploy/usr/local/bin/hd-backup-prep \
        deploy/usr/local/bin/hd-backup-unload \

    if [[ "${Q_QEMU_KVM}" == "true" ]]; then
        add_packages zstd

        add_files \
            deploy/etc/hyprdebian/vm-backups \
            deploy/etc/systemd/system/backup-vms.service \
            deploy/etc/systemd/system/backup-vms.timer \
            deploy/usr/local/bin/hd-backup-vm \
            deploy/usr/local/bin/hd-backup-vms \
            deploy/usr/local/bin/hd-restore-vm \

        add_services \
            backup-vms.timer \
            
    fi

    # Compile block write utility for backups on tape.
    cp src/blockwrite.c ${TARGET_DIR}/tmp/blockwrite.c || return 1
    in_target gcc -O2 -D_GNU_SOURCE -o /usr/local/bin/blockwrite /tmp/blockwrite.c || return 1
    rm -f ${TARGET_DIR}/tmp/blockwrite.c
}

o_rust() {
    add_packages rustup

    add_files deploy/usr/local/bin/syscargo

    add_dependencies "u_post" \
        "o_syscargo" \
        "o_syscargo_permissions"
}

o_syscargo() {
    # The checks for an existing dataset and user let a rerun of this task get past them.
    zfs list rpool/home/cargo > /dev/null 2>&1 || zfs create rpool/home/cargo || return 1
    in_target id -u cargo > /dev/null 2>&1 || in_target useradd -m -r -s /bin/bash cargo || return 1
    in_target chown -R cargo:cargo /home/cargo || return 1
    in_target sudo -u cargo mkdir -p /home/cargo/.cargo || return 1
    in_target sudo -u cargo tee /home/cargo/.cargo/config.toml > /dev/null <<'EOF' || return 1
[install]
root = "/usr/local"
EOF
    in_target sudo -u cargo rustup default stable
}

o_syscargo_permissions() {
    in_target chown -R root:cargo /usr/local || return 1
    in_target chmod -R g+w /usr/local
}

o_report() {
    # Keep an existing token, so a rerun does not lock the system out of the dashboard, which only
    # accepts the first token it sees for a host name.
    if [[ -z "${REPORT_TOKEN:-}" ]]; then
        REPORT_TOKEN=$(random_token)
        save_config REPORT_TOKEN ${REPORT_TOKEN}
    fi

    add_packages python3

    add_files \
        deploy/etc/systemd/system/hd-report.service \
        deploy/etc/systemd/system/hd-report.timer \
        deploy/usr/local/bin/hd-report \

    add_template templates/etc/hyprdebian/report.conf.j2 0600

    add_services hd-report.timer
}

# The firewall is always on, so its rules for the optional components are always queued.
add_dependencies "o_main" "o_firewall"

add_dependencies "install" "o_pre" "o_main" "o_post"
