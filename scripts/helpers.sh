#!/bin/bash

add_packages() {
    for package in "$@"; do
        for check in "${PACKAGES[@]}"; do
            [[ "$check" == "$package" ]] && continue
        done

        PACKAGES+=("$package")
    done

    save_config PACKAGES "${PACKAGES[*]}" # Persist as space-separated string.
}

add_services() {
    for service in "$@"; do
        for check in "${SERVICES[@]}"; do
            [[ "$check" == "$service" ]] && continue
        done

        SERVICES+=("$service")
    done

    save_config SERVICES "${SERVICES[*]}" # Persist as space-separated string.
}

add_user_groups() {
    for group in "$@"; do
        for check in "${USER_GROUPS[@]}"; do
            [[ "$check" == "$group" ]] && continue
        done

        USER_GROUPS+=("$group")
    done

    save_config USER_GROUPS "${USER_GROUPS[*]}" # Persist as space-separated string.
}

ask() {
    local var_name="$1"
    local prompt="$2"
    local default="${3:-}"

    local current="${!var_name:-$default}"

    if [[ -n "${current:-}" ]]; then
        read -rp "$prompt [$current]: " input
        input="${input:-$current}"
    else
        read -rp "$prompt: " input
    fi

    printf -v "$var_name" "%s" "$input"

    save_config "$var_name" "$input"
}

ask_yes_no() {
    local var_name=$1
    local prompt=$2
    local input

    while true; do
        read -rp "$prompt (y/n): " input
        case "$input" in
            [Yy]*)
                printf -v "$var_name" "true"
                save_config "$var_name" "true"
                return 0 # Success (True).
                ;;
            [Nn]*)
                printf -v "$var_name" "false"
                save_config "$var_name" "false"
                return 1 # Failure (False).
                ;;
            *)
                echo "Please answer y or n."
                ;;
        esac
    done
}

in_target() {
    chroot "${TARGET_DIR}" /usr/bin/env -i \
        HOME=/root \
        TERM="${TERM}" \
        PATH=/usr/sbin:/usr/bin:/sbin:/bin \
        "$@"
}

log_status() {
    local color="$1"
    local label="$2"
    local step="$3"
    local timestamp

    # Use Bash built-in for current time (H:M:S).
    printf -v timestamp "%(%H:%M:%S)T" -1

    # Formatting: [LABEL] <TIMESTAMP> STEP.
    printf "%b[%s]%b <%s> %s\n" "$color" "$label" "$COLOR_RESET" "$timestamp" "$step"
}

# Generates a random mac address.
random_mac() {
    local bytes
    bytes=$(od -An -N6 -tx1 /dev/urandom)

    # Set locally-administered bit and clear multicast bit
    set -- $bytes
    printf '%02x:%02x:%02x:%02x:%02x:%02x\n' \
        "$((0x$1 & 0xfe | 0x02))" \
        "0x$2" "0x$3" "0x$4" "0x$5" "0x$6"
}

# Shows network interfaces.
show_ifaces() {
    printf '%-12s %-12s %-12s %s\n' "INTERFACE" "TYPE" "PATH NAME" "IP ADDRESS"
    printf '%-12s %-12s %-12s %s\n' "---------" "----" "---------" "----------"

    for iface in /sys/class/net/*; do
        name=$(basename "$iface")

        path_name=$(udevadm info --query=property --path="/sys/class/net/$name" 2>/dev/null | sed -n 's/^ID_NET_NAME_PATH=//p')

        if [ -d "$iface/wireless" ]; then
            type="wifi"
        elif [ -e "$iface/device" ]; then
            type="wired"
        else
            type="virtual"
        fi

        ip_addr=$(ip -4 -o addr show dev "$name" 2>/dev/null |
            awk '{print $4}' |
            cut -d/ -f1 |
            paste -sd, -)

        printf '%-12s %-12s %-12s %s\n' "$name" "$type" "${path_name:--}" "${ip_addr:--}"
    done
}

trim() {
    local var="$*"
    # Remove leading whitespace
    var="${var#"${var%%[![:space:]]*}"}"
    # Remove trailing whitespace
    var="${var%"${var##*[![:space:]]}"}"
    printf '%s' "$var"
}

write_file() {
    local path="$1"
    local mode="$2"

    install -d "$(dirname "$path")"
    install -m "$mode" /dev/null "$path"
    cat > "$path"
}
