#!/bin/bash
# Helpers for the network: picking the interfaces.
# Interfaces are named by their path name (ID_NET_NAME_PATH of udev), such as enp3s0, which is the
# name they get on the installed system. The live system may name them otherwise.

# Prints the path name of an interface, or nothing when it has none.
# Usage: iface_path_name <kernel name>
iface_path_name() {
    udevadm info --query=property --path="${NET_CLASS_DIR}/$1" 2> /dev/null \
        | sed -n 's/^ID_NET_NAME_PATH=//p'
}

# Fills the array named by the first argument with the path names of the wired or the wifi
# interfaces, sorted. Virtual interfaces, such as bridges, and interfaces without a path name are
# left out.
# Usage: list_ifaces <array> <wired|wifi>
list_ifaces() {
    local -n ifaces_out="$1"
    local kind="$2"
    local iface name path_name
    local found=()

    ifaces_out=()
    for iface in "${NET_CLASS_DIR}"/*; do
        name="$(basename "${iface}")"
        if [[ ! -e "${iface}/device" ]]; then
            continue
        fi
        if [[ -d "${iface}/wireless" && "${kind}" == "wired" ]]; then
            continue
        fi
        if [[ ! -d "${iface}/wireless" && "${kind}" == "wifi" ]]; then
            continue
        fi
        path_name="$(iface_path_name "${name}")"
        if [[ -n "${path_name}" ]]; then
            found+=("${path_name}")
        fi
    done

    if (( ${#found[@]} > 0 )); then
        mapfile -t ifaces_out < <(printf '%s\n' "${found[@]}" | sort)
    fi
}

# Prints the kernel name of the interface with a path name on the live system, and its IPv4
# addresses.
# Usage: iface_info <path name>
iface_info() {
    local iface name addresses

    for iface in "${NET_CLASS_DIR}"/*; do
        name="$(basename "${iface}")"
        if [[ "$(iface_path_name "${name}")" == "$1" ]]; then
            addresses="$(ip -4 -o addr show dev "${name}" 2> /dev/null | awk '{print $4}' \
                | paste -sd, -)"
            printf 'now %s, %s\n' "${name}" "${addresses:-no IPv4 address}"
            return 0
        fi
    done
}

# Asks to pick a wired or wifi interface by number, and stores its path name.
# Usage: ask_iface <variable> <prompt> <default> <wired|wifi>
# The saved answer is offered first, then the default, then the first interface.
ask_iface() {
    local var_name="$1"
    local prompt="$2"
    local default="$3"
    local kind="$4"
    local ifaces=()
    local current="${!var_name:-}"
    local default_idx="" i input

    list_ifaces ifaces "${kind}"
    if (( ${#ifaces[@]} == 0 )); then
        echo "Error: There are no ${kind} interfaces to pick for ${var_name}." >&2
        return 1
    fi

    if answered "${var_name}"; then
        show_answer "${prompt}" "${ANSWERS[${var_name}]}"
        for (( i = 0; i < ${#ifaces[@]}; i++ )); do
            if [[ "${ifaces[i]}" == "${ANSWERS[${var_name}]}" ]]; then
                set_answer "${var_name}" "${ifaces[i]}"
                return 0
            fi
        done
        echo "Error: ${ANSWERS[${var_name}]} of ${var_name} is not one of the ${kind}" \
            "interfaces: ${ifaces[*]}." >&2
        return 1
    fi

    for (( i = 0; i < ${#ifaces[@]}; i++ )); do
        if [[ -n "${current}" && "${ifaces[i]}" == "${current}" ]]; then
            default_idx=$((i + 1))
        fi
    done
    if [[ -z "${default_idx}" ]]; then
        for (( i = 0; i < ${#ifaces[@]}; i++ )); do
            if [[ -n "${default}" && "${ifaces[i]}" == "${default}" ]]; then
                default_idx=$((i + 1))
            fi
        done
    fi
    default_idx="${default_idx:-1}"

    for (( i = 0; i < ${#ifaces[@]}; i++ )); do
        printf "%2d) %s\n    %s\n" "$((i + 1))" "${ifaces[i]}" "$(iface_info "${ifaces[i]}")"
    done

    while true; do
        if ! read -rp "${prompt} [${default_idx}]: " input; then
            echo >&2
            echo "Error: No answer for ${var_name}." >&2
            return 1
        fi
        input="${input:-${default_idx}}"
        if [[ "${input}" =~ ^[0-9]+$ ]] && (( input >= 1 && input <= ${#ifaces[@]} )); then
            break
        fi
        echo "Please enter a number from 1 to ${#ifaces[@]}." >&2
    done

    set_answer "${var_name}" "${ifaces[input - 1]}"
}
