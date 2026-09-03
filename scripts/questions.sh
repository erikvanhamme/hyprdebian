#!/bin/bash

q_pre() {
    return 0
}

q_main() {
    return 0
}

q_post() {
    return 0
}

q_redundant() {
    ask_yes_no Q_REDUNDANT "Install on redundant drives"
    return 0
}

q_disk() {
    echo "Available block devices:"
    find /dev/disk/by-id

    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        ask Q_DISK_A "Target disk A"
        ask Q_DISK_B "Target disk B"
        Q_DISKS="${Q_DISK_A} ${Q_DISK_B}"
    else
        ask Q_DISK "Target disk"
        Q_DISKS="${Q_DISK}"
    fi

    for DISK in ${Q_DISKS}; do
        if ask_yes_no Q_DESTROY "All data on $DISK will be destroyed. Continue"; then
            continue
        else
            echo "Aborting."
            return 1
        fi
    done
}

q_swap() {
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        Q_SWAP=0
        return 0
    fi

    ask Q_SWAP "Enter size of swap partition (in GiB, 0 = disable swap)" "2"
}

q_user() {
    ask Q_USER "Username" "rsq"
}

q_hostname() {
    ask Q_HOSTNAME "Hostname" "hyprdebian"
}

q_fqdn() {
    ask Q_FQDN "Fully qualified domain name" "${Q_HOSTNAME}.rsq-online.ddns.net"
}

q_kernel() {
    if ! ask_yes_no Q_SPECIFIC_KERNEL "Do you wish to download a specific kernel"; then
        Q_KERNEL="latest"
        save_config Q_KERNEL ${Q_KERNEL}
        return 0
    fi

    ask Q_KERNEL_REPO "Which repository would you like to download a kernel from" "stable"
    python3 kernels.py ${Q_KERNEL_REPO}

    echo "Downloaded kernel versions:"
    if [[ -d kernels ]]; then
        find kernels | grep -E "^kernels/linux-image-[0-9]+\.[0-9]+\.[0-9]+\+deb[0-9]+-amd64" | awk -F'[-+]' '{print $3}' | sort -V
    fi
    ask Q_KERNEL "Select kernel version to install"
}

q_iface() {
    show_ifaces
    ask Q_IFACE "Select wired interface for DHCP (use path name!)"
}

q_wifi() {
    if ask_yes_no Q_WIFI "Enable wifi"; then
        show_ifaces
        ask Q_WIFACE "Select wifi interface for DHCP (use path name!)"

        add_dependencies "o_main" "o_wifi"
    fi
}

q_firewall() {
    if ask_yes_no Q_FIREWALL "Install firewall"; then
        add_dependencies "o_main" "o_firewall"
    fi
}

q_desktop() {
    if ask_yes_no Q_DESKTOP "Install desktop environment"; then
        add_dependencies "o_main" "o_font" "o_desktop"
    fi
}

q_docker() {
    if ask_yes_no Q_DOCKER "Enable docker"; then
        add_dependencies "o_main" "o_docker"
    fi
}

q_qemu_kvm() {
    if ask_yes_no Q_QEMU_KVM "Enable Qemu/KVM"; then
        add_dependencies "o_main" "o_qemu_kvm"
    fi
}

q_cups() {
    if ask_yes_no Q_CUPS "Enable CUPS"; then
        add_dependencies "o_main" "o_cups"
    fi
}

q_rust() {
    if ask_yes_no Q_RUST "Enable rust and syscargo support"; then
        add_dependencies "o_main" "o_rust"
    fi
}

q_openssh() {
    if ask_yes_no Q_OPENSSH "Enable OpenSSH server"; then
        add_dependencies "o_main" "o_openssh"
    fi
}

q_repo() {
    if ask_yes_no Q_REPO_ENABLED "Enable hyprdebian repo"; then
        ask Q_REPO "Local hyprdebian repository URL" "http://repo.rsq-online.ddns.net:8080/"
    fi
}

# Add additional questions here.

add_dependencies "q_main" \
    "q_redundant" \
    "q_disk" \
    "q_swap" \
    "q_user" \
    "q_hostname" \
    "q_fqdn" \
    "q_kernel" \
    "q_iface" \
    "q_wifi" \
    "q_firewall" \
    "q_desktop" \
    "q_docker" \
    "q_qemu_kvm" \
    "q_cups" \
    "q_rust" \
    "q_openssh" \
    "q_repo"

add_dependencies "install" "q_pre" "q_main" "q_post"
