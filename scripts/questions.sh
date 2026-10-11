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

q_os() {
    local default="$(get_default Q_OS "${Q_TYPE:-}")"
    ask_options Q_OS "Which OS to install" "${default}" "hyprdebian" "debian-stable" "debian-testing" "debian-unstable"
    case "${Q_OS}" in
        "hyprdebian")
            Q_SUITE="unstable"
            add_dependencies "q_os_questions" \
                "q_kernel" \
                "q_desktop" \
                "q_encrypt" \
                "q_iface" \
                "q_wifi" \
                "q_firewall" \
                "q_docker" \
                "q_qemu_kvm" \
                "q_cups" \
                "q_backuptools" \
                "q_rust" \
                "q_openssh" \
                "q_repo"
            ;;
        "debian-stable")
            Q_SUITE="stable"
            Q_KERNEL="latest"
            add_dependencies "q_os_questions" \
                "q_de" \
                "q_encrypt"
            save_config Q_KERNEL ${Q_KERNEL}
            ;;
        "debian-testing")
            Q_SUITE="testing"
            Q_KERNEL="latest"
            add_dependencies "q_os_questions" \
                "q_de" \
                "q_encrypt"
            save_config Q_KERNEL ${Q_KERNEL}
            ;;
        "debian-unstable")
            Q_SUITE="unstable"
            Q_KERNEL="latest"
            add_dependencies "q_os_questions" \
                "q_de" \
                "q_encrypt"
            save_config Q_KERNEL ${Q_KERNEL}
            ;;
    esac

    save_config Q_SUITE ${Q_SUITE}
}

q_os_questions() {
    return 0
}

q_type() {
    local default="$(get_default Q_TYPE)"
    ask_options Q_TYPE "What type of computer are you installing" "${default}" "${MACHINE_TYPES[@]}"
}

q_defaults() {
    local default="$(get_default Q_DEFAULTS "${Q_TYPE:-}")"
    ask_yes_no Q_DEFAULTS "Use machine type defaults" "${default}" || true
}

q_de() {
    local default="$(get_default Q_DE "${Q_TYPE:-}")"
    ask_options Q_DE "Which desktop environment" "${default}" "none" "cinnamon" "gnome" "kde" "lxde" "lxqt" "mate" "xfce"
    case "${Q_DE}" in
        "none")
            ;;
        "cinnamon")
            add_packages task-cinnamon-desktop
            ;;
        "gnome")
            add_packages task-gnome-desktop
            ;;
        "kde")
            add_packages task-kde-desktop
            ;;
        "lxde")
            add_packages task-lxde-desktop
            ;;
        "lxqt")
            add_packages task-lxqt-desktop
            ;;
        "mate")
            add_packages task-mate-desktop
            ;;
        "xfce")
            add_packages task-xfce-desktop
            ;;
    esac
}

q_redundant() {
    local default="$(get_default Q_REDUNDANT "${Q_TYPE:-}")"
    if ask_yes_no Q_REDUNDANT "Install on redundant drives" "${default}"; then
        add_files \
            "deploy/etc/apt/conf.d/99sync-efi" \
            "deploy/usr/local/bin/hd-sync-efi" \

    fi
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
        save_config Q_SWAP ${Q_SWAP}
        return 0
    fi

    local default="$(get_default Q_SWAP "${Q_TYPE:-}")"
    ask Q_SWAP "Enter size of swap partition (in GiB, 0 = disable swap)" "${default}"
}

q_user() {
    local default="$(get_default Q_USER "${Q_TYPE:-}")"
    ask Q_USER "Username" "${default}"
}

q_password() {
    ask_password Q_PASSWORD "Password for user account and disk encryption"
}

q_fullname() {
    local default="$(get_default Q_FULLNAME "${Q_TYPE:-}")"

    while true; do
        ask Q_FULLNAME "Full name" "${default}"

        # The full name ends up in the GECOS field of /etc/passwd.
        if [[ "${Q_FULLNAME}" != *[:,]* ]]; then
            break
        fi

        echo "Full name cannot contain ':' or ','." >&2
        Q_FULLNAME=""
    done
}

q_hostname() {
    local default="$(get_default Q_HOSTNAME "${Q_TYPE:-}")"
    ask Q_HOSTNAME "Hostname" "${default}"
}

q_fqdn() {
    local default="$(get_default Q_FQDN "${Q_TYPE:-}")"
    ask Q_FQDN "Fully qualified domain name" "${default:-${Q_HOSTNAME}.rsq-online.ddns.net}"
}

q_kernel() {
    local default="$(get_default Q_SPECIFIC_KERNEL "${Q_TYPE:-}")"
    if ! ask_yes_no Q_SPECIFIC_KERNEL "Do you wish to download a specific kernel" "${default}"; then
        Q_KERNEL="latest"
        save_config Q_KERNEL ${Q_KERNEL}
        return 0
    fi

    local default="$(get_default Q_KERNEL_REPO "${Q_TYPE:-}")"
    ask Q_KERNEL_REPO "Which repository would you like to download a kernel from" "${default}"
    python3 kernels.py ${Q_KERNEL_REPO}

    echo "Downloaded kernel versions:"
    if [[ -d kernels ]]; then
        find kernels | grep -E "^kernels/linux-image-[0-9]+\.[0-9]+\.[0-9]+\+deb[0-9]+-amd64" | awk -F'[-+]' '{print $3}' | sort -V
    fi
    ask Q_KERNEL "Select kernel version to install"
}

q_iface() {
    show_ifaces
    local default="$(get_default Q_IFACE "${Q_TYPE:-}")"
    ask Q_IFACE "Select wired interface for DHCP (use path name!)" "${default}"
}

q_wifi() {
    local default="$(get_default Q_WIFI "${Q_TYPE:-}")"
    if ask_yes_no Q_WIFI "Enable wifi" "${default}"; then
        show_ifaces
        ask Q_WIFACE "Select wifi interface for DHCP (use path name!)"

        add_dependencies "o_main" "o_wifi"
    fi
}

q_firewall() {
    local default="$(get_default Q_FIREWALL "${Q_TYPE:-}")"
    if ask_yes_no Q_FIREWALL "Install firewall" "${default}"; then
        add_dependencies "o_main" "o_firewall"
    fi
}

q_desktop() {
    local default="$(get_default Q_DESKTOP "${Q_TYPE:-}")"
    if ask_yes_no Q_DESKTOP "Install hyprland desktop environment" "${default}"; then
        add_dependencies "o_main" "o_desktop"
    fi
}

q_encrypt() {
    local default="$(get_default Q_ENCRYPT "${Q_TYPE:-}")"
    if ! ask_yes_no Q_ENCRYPT "Encrypt root filesystem" "${default}"; then
        return 0
    fi
}

q_docker() {
    local default="$(get_default Q_DOCKER "${Q_TYPE:-}")"
    if ask_yes_no Q_DOCKER "Enable docker" "${default}"; then
        add_dependencies "o_main" "o_docker"
    fi
}

q_qemu_kvm() {
    local default="$(get_default Q_QEMU_KVM "${Q_TYPE:-}")"
    if ask_yes_no Q_QEMU_KVM "Enable Qemu/KVM" "${default}"; then
        add_dependencies "o_main" "o_qemu_kvm"
    fi
}

q_cups() {
    local default="$(get_default Q_CUPS "${Q_TYPE:-}")"
    if ask_yes_no Q_CUPS "Enable CUPS" "${default}"; then
        add_dependencies "o_main" "o_cups"
    fi
}

q_backuptools() {
    local default="$(get_default Q_BACKUPTOOLS "${Q_TYPE:-}")"
    if ask_yes_no Q_BACKUPTOOLS "Install backup tools" "${default}"; then
        add_dependencies "o_main" "o_backuptools"
    fi
}

q_rust() {
    local default="$(get_default Q_RUST "${Q_TYPE:-}")"
    if ask_yes_no Q_RUST "Enable rust and syscargo support" "${default}"; then
        add_dependencies "o_main" "o_rust"
    fi
}

q_openssh() {
    local default="$(get_default Q_OPENSSH "${Q_TYPE:-}")"
    if ask_yes_no Q_OPENSSH "Enable OpenSSH server" "${default}"; then
        add_dependencies "o_main" "o_openssh"
    fi
}

q_repo() {
    local default="$(get_default Q_REPO_ENABLED "${Q_TYPE:-}")"
    if ask_yes_no Q_REPO_ENABLED "Enable hyprdebian repo" "${default}"; then
        local default="$(get_default Q_REPO "${Q_TYPE:-}")"
        ask Q_REPO "Local hyprdebian repository URL" "${default}"
    fi
}

# Add additional questions here.

add_dependencies "q_main" \
    "q_os" \
    "q_type" \
    "q_defaults" \
    "q_redundant" \
    "q_disk" \
    "q_swap" \
    "q_user" \
    "q_password" \
    "q_fullname" \
    "q_hostname" \
    "q_fqdn" \
    "q_os_questions" \

# The password is never saved, so it must be asked again on every run.
always_run "q_password"

add_dependencies "install" "q_pre" "q_main" "q_post"
