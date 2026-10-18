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

q_defaults() {
    local default="$(get_default Q_DEFAULTS "${Q_TYPE:-}")"
    ask_yes_no Q_DEFAULTS "Use machine type defaults" "${default}" || true
}

q_kernel() {
    # The specific kernels are only offered on unstable, the ZFS module of the other suites may
    # not build for them.
    if [[ "${Q_SUITE}" != "unstable" ]]; then
        Q_SPECIFIC_KERNEL="false"
        save_config Q_SPECIFIC_KERNEL ${Q_SPECIFIC_KERNEL}
        Q_KERNEL="latest"
        save_config Q_KERNEL ${Q_KERNEL}
        return 0
    fi

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

q_report() {
    local default="$(get_default Q_REPORT "${Q_TYPE:-}")"
    if ask_yes_no Q_REPORT "Send status reports to the hyprdebian dashboard" "${default}"; then
        local default="$(get_default Q_REPORT_URL "${Q_TYPE:-}")"
        ask Q_REPORT_URL "Hyprdebian dashboard URL" "${default}"

        add_dependencies "o_main" "o_report"
    fi
}

# Add additional questions here.

add_dependencies "q_main" \
    "q_defaults" \
    "q_kernel" \
    "q_cups" \
    "q_backuptools" \
    "q_rust" \
    "q_report" \

add_dependencies "install" "q_pre" "q_main" "q_post"
