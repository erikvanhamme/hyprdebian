#!/bin/bash

t_info() {
    return 0
}

q_disk() {
    echo "Available block devices:"
    find /dev/disk/by-id
    ask Q_DISK "Target disk (e.g. /dev/sda)"

    if ask_yes_no Q_DESTROY "All data on $Q_DISK will be destroyed. Continue"; then
        return 0
    else
        echo "Aborting."
        return 1
    fi
}

q_swap() {
    ask Q_SWAP "Enter size of swap partition (in GiB, must be > 0)" "2"
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
    echo "Available interfaces:"
    show_ifaces
    ask Q_IFACE "Select wired interface for DHCP (use path name!)"
}

q_wifi() {
    if ask_yes_no Q_WIFI "Enable wifi"; then

        echo "Available interfaces:"
        show_ifaces
        ask Q_WIFACE "Select wifi interface for DHCP (use path name!)"

        add_dependencies "t_optional" "o_wifi"
    else
        return 0
    fi
}

q_firewall() {
    ask_yes_no Q_FIREWALL "Install firewall"
}

q_desktop() {
    if ask_yes_no Q_DESKTOP "Install desktop environment"; then
        add_dependencies "t_optional" "o_desktop"
    else
        return 0
    fi
}

q_docker() {
    if ask_yes_no Q_DOCKER "Enable docker"; then
        add_dependencies "t_optional" "o_docker"
    else
        return 0
    fi
}

q_qemu_kvm() {
    if ask_yes_no Q_QEMU_KVM "Enable Qemu/KVM"; then
        add_dependencies "t_optional" "o_qemu_kvm"
    else
        return 0
    fi
}

q_cups() {
    if ask_yes_no Q_CUPS "Enable CUPS"; then
        add_dependencies "t_optional" "o_cups"
    else
        return 0
    fi
}

q_rust() {
    if ask_yes_no Q_RUST "Enable rust and syscargo support"; then
        add_dependencies "t_optional" "o_rust"
    fi
}

q_openssh() {
    if ask_yes_no Q_OPENSSH "Enable OpenSSH server"; then
        add_packages openssh-server
    fi
}

q_laptop() {
    if ask_yes_no Q_LAPTOP "Is this a laptop"; then
        add_dependencies "t_optional" "o_laptop"
    fi
}

q_repo() {
    ask Q_REPO "Local hyprdebian repository URL" "http://bitbucket5:8080/"
}

add_dependencies "t_info" \
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
    "q_laptop" \
    "q_repo"

add_dependencies "t_install" \
    "t_info"

