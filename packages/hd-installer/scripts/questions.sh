#!/bin/bash
# The questions phase: asks everything the installation needs up front, so the rest runs
# without questions. Each question is a task, so a rerun skips the questions that were
# answered; the answers are in the config.

q_pre() {
    return 0
}

q_main() {
    return 0
}

q_post() {
    return 0
}

q_suite() {
    local default
    default="$(get_default Q_SUITE)" || return 1
    ask_options Q_SUITE "Which suite are you installing" "${default}" "${SUITES[@]}"
}

# The system, the ZFS pool and the swap, goes on one disk, or is mirrored on two.
q_root_redundant() {
    local default
    default="$(get_default Q_ROOT_REDUNDANT)" || return 1
    ask_yes_no Q_ROOT_REDUNDANT "Mirror the system (ZFS pool and swap) on two disks" "${default}"
}

# Whether the ZFS pool is encrypted, with the password of the user as its passphrase.
q_encrypt() {
    local default
    default="$(get_default Q_ENCRYPT)" || return 1
    ask_yes_no Q_ENCRYPT "Encrypt the system" "${default}"
}

q_root_disks() {
    echo "Disks:"
    ask_disk Q_ROOT_DISK_A "Disk for the system" "" || return 1

    if [[ "${Q_ROOT_REDUNDANT}" == "true" ]]; then
        ask_disk Q_ROOT_DISK_B "Second disk for the system" "" "${Q_ROOT_DISK_A}" || return 1
    else
        set_answer Q_ROOT_DISK_B ""
    fi
}


q_swap() {
    local default
    default="$(get_default Q_SWAP)" || return 1
    ask_valid Q_SWAP "Swap size in GiB (0 for no swap)" "${default}" valid_swap \
        "Please enter a whole number of GiB." || return 1

    # Without leading zeros, which bash would read as an octal number.
    set_answer Q_SWAP "$((10#${Q_SWAP}))"
}

# The EFI system partition is asked apart from the system, so it can go on other disks, such as
# a hardware RAID volume next to a ZFS mirror. By default it follows the system.
q_esp_redundant() {
    local default="n"
    if [[ "${Q_ROOT_REDUNDANT}" == "true" ]]; then
        default="y"
    fi
    ask_yes_no Q_ESP_REDUNDANT "Put an EFI system partition on two disks" "${default}"
}

q_esp_disks() {
    echo "Disks:"
    ask_disk Q_ESP_DISK_A "Disk for the EFI system partition" "${Q_ROOT_DISK_A}" || return 1

    if [[ "${Q_ESP_REDUNDANT}" == "true" ]]; then
        ask_disk Q_ESP_DISK_B "Second disk for the EFI system partition" "${Q_ROOT_DISK_B}" \
            "${Q_ESP_DISK_A}" || return 1
    else
        set_answer Q_ESP_DISK_B ""
    fi
}


q_user() {
    local default
    default="$(get_default Q_USER)" || return 1
    ask_valid Q_USER "User name" "${default}" valid_user \
        "A user name is a lower case letter, then lower case letters, digits, - and _." || return 1
}

# The password of the user, and the passphrase of the ZFS pool when it is encrypted. It is never
# saved, so it is asked again on every run.
q_password() {
    local prompt="Password of ${Q_USER}"
    if [[ "${Q_ENCRYPT}" == "true" ]]; then
        prompt="Password of ${Q_USER} and of the disk encryption"
    fi
    ask_password Q_PASSWORD "${prompt}"
}


q_fullname() {
    local default
    default="$(get_default Q_FULLNAME)" || return 1
    ask_valid Q_FULLNAME "Full name" "${default}" valid_fullname \
        "The full name cannot be empty, nor contain : or ,." || return 1
}


q_hostname() {
    local default
    default="$(get_default Q_HOSTNAME)" || return 1
    ask_valid Q_HOSTNAME "Host name" "${default}" valid_hostname \
        "A host name has lower case letters, digits and -, not at the start or the end." \
        || return 1
}


q_fqdn() {
    local default
    default="$(get_default Q_FQDN)" || return 1
    if [[ -z "${default}" ]]; then
        default="${Q_HOSTNAME}.$(get_default Q_DOMAIN)" || return 1
    fi
    ask_valid Q_FQDN "Fully qualified domain name" "${default}" valid_fqdn \
        "The fully qualified domain name is ${Q_HOSTNAME}. followed by the domain, in lower case." \
        || return 1
}

# The wired interface, which gets its address with DHCP. Without a wired interface, as on some
# laptops, there is none.
q_iface() {
    local ifaces default

    list_ifaces ifaces wired
    if (( ${#ifaces[@]} == 0 )); then
        if answered Q_IFACE; then
            echo "Error: The answers file names the wired interface ${ANSWERS[Q_IFACE]}, but" \
                "there is none." >&2
            return 1
        fi
        echo "There is no wired interface."
        set_answer Q_IFACE ""
        return 0
    fi

    default="$(get_default Q_IFACE)" || return 1
    echo "Wired interfaces:"
    ask_iface Q_IFACE "Wired interface" "${default}" wired
}

# Whether wifi is used, and then the wifi interface, which also gets its address with DHCP.
# Without a wifi interface, there is no wifi.
q_wifi() {
    local ifaces default

    list_ifaces ifaces wifi
    if (( ${#ifaces[@]} == 0 )); then
        if answered Q_WIFACE; then
            echo "Error: The answers file names the wifi interface ${ANSWERS[Q_WIFACE]}, but" \
                "there is none." >&2
            return 1
        fi
        echo "There is no wifi interface."
        set_answer Q_WIFI false
        set_answer Q_WIFACE ""
        return 0
    fi

    default="$(get_default Q_WIFI)" || return 1
    ask_yes_no Q_WIFI "Enable wifi" "${default}" || return 1

    if [[ "${Q_WIFI}" == "true" ]]; then
        echo "Wifi interfaces:"
        ask_iface Q_WIFACE "Wifi interface" "" wifi || return 1
    else
        set_answer Q_WIFACE ""
    fi
}

# Whether the debconf questions of the locales, the time zone, the keyboard and the console font
# take the defaults, without asking them. The selections of the answers file go first, so when it
# has selections of every package, there is nothing to ask.
q_debconf_defaults() {
    local package default line
    local all_answered=true

    for package in "${DEBCONF_PACKAGES[@]}"; do
        if ! debconf_answered "${package}"; then
            all_answered=false
        fi
    done
    if [[ "${all_answered}" == "true" ]] && ! answered Q_DEBCONF_DEFAULTS; then
        set_answer Q_DEBCONF_DEFAULTS false
        return 0
    fi

    default="$(get_default Q_DEBCONF_DEFAULTS)" || return 1
    echo "Default settings:"
    for line in "${DEBCONF_DEFAULTS_SUMMARY[@]}"; do
        printf '    %s\n' "${line}"
    done
    ask_yes_no Q_DEBCONF_DEFAULTS "Use the default locale, time zone, keyboard and console font" \
        "${default}"
}

# Every disk picked for the system or the EFI system partition is wiped. Each must be confirmed
# by typing the answer, or at once by wipe in the answers file; the confirmation is not saved.
q_wipe() {
    local disks disk

    all_disks disks

    echo "The installation wipes these disks:"
    for disk in "${disks[@]}"; do
        printf '    %s\n        %s\n' "$(basename "${disk}")" "$(disk_info "${disk}")"
    done

    if answered Q_WIPE; then
        show_answer "Wipe them" "${ANSWERS[Q_WIPE]}"
        if [[ "${ANSWERS[Q_WIPE]}" != "true" ]]; then
            echo "Aborting." >&2
            return 1
        fi
        return 0
    fi
    for disk in "${disks[@]}"; do
        if ! confirm "All data on ${disk} will be destroyed. Continue"; then
            echo "Aborting." >&2
            return 1
        fi
    done
}

# Add additional questions here.

add_dependencies "q_main" \
    "q_suite" \
    "q_root_redundant" \
    "q_encrypt" \
    "q_root_disks" \
    "q_swap" \
    "q_esp_redundant" \
    "q_esp_disks" \
    "q_user" \
    "q_password" \
    "q_fullname" \
    "q_hostname" \
    "q_fqdn" \
    "q_iface" \
    "q_wifi" \
    "q_debconf_defaults" \
    "q_wipe" \

# The password is never saved, so it must be asked again on every run.
always_run "q_password"

add_dependencies "install_root" "q_pre" "q_main" "q_post"
