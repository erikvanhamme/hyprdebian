#!/bin/bash

add_files() {
    for file in "$@"; do
        for check in "${FILES[@]}"; do
            [[ "$check" == "$file" ]] && continue
        done

        FILES+=("$file")
    done

    save_config FILES "${FILES[*]}" # Persist as space-separated string.
}

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

add_template() {
    for check in "${TEMPLATE_SRCS[@]}"; do
        if [[ "$check" == "$1" ]]; then
            return 0
        fi
    done

    TEMPLATE_SRCS+=("$1")
    TEMPLATE_MODES+=("${2:-0644}")

    save_config TEMPLATE_SRCS "${TEMPLATE_SRCS[*]}" # Persist as space-separated string.
    save_config TEMPLATE_MODES "${TEMPLATE_MODES[*]}" # Persist as space-separated string.
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

ask_options() {
    local var_name="$1"
    local prompt="$2"
    shift 2

    # Collect remaining arguments as options array
    local options=("$@")
    local total_options=${#options[@]}

    if (( total_options == 0 )); then
        echo "Error: No options provided to ask_options" >&2
        return 1
    fi

    local current="${!var_name:-}"
    local default_idx=""
    local i

    # Display numbered options and check for default matching existing var or text
    for (( i=0; i<total_options; i++ )); do
        printf "%2d) %s\n" "$((i + 1))" "${options[i]}"
        if [[ -n "$current" && "${options[i]}" == "$current" ]]; then
            default_idx=$((i + 1))
        fi
    done

    # Fallback to option 1 as default if var_name is unset or doesn't match
    default_idx="${default_idx:-1}"

    local choice_prompt="$prompt [$default_idx]: "
    local input choice_idx

    while true; do
        read -rp "$choice_prompt" input
        input="${input:-$default_idx}"

        # Validate numeric input within valid bounds
        if [[ "$input" =~ ^[0-9]+$ ]] && (( input >= 1 && input <= total_options )); then
            choice_idx=$((input - 1))
            break
        fi

        echo "Invalid choice. Please enter a number between 1 and $total_options." >&2
    done

    local selected_option="${options[choice_idx]}"

    # Assign result back to target variable name
    printf -v "$var_name" "%s" "$selected_option"

    # Save to configuration
    save_config "$var_name" "$selected_option"
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

file_deploy() {
    local src="$1"
    local prefix="${2:-${TARGET_DIR}}"
    local dst=$(file_dst "$src" "$prefix")

    # Extract directory path
    local dir=$(dirname "$dst")

    # Create directory if it doesn't exist
    mkdir -p "$dir"

    cp $src $dst
}

file_deploy_queue() {
    for file in ${FILES[@]}; do
        file_deploy $file
    done
}

file_dst() {
    local src="$1"
    local prefix="$2"

    # Strip leading 'deploy/'
    local dest="${src#deploy/}"

    # Prepend prefix
    echo "${prefix}/${dest}"
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

template_dst() {
    local src="$1"
    local prefix="$2"

    # Strip leading 'templates/'
    local rel_path="${src#templates/}"

    # Strip trailing '.j2'
    local dest="${rel_path%.j2}"

    # Prepend prefix
    echo "${prefix}/${dest}"
}

template_render() {
    local src="$1"
    local mode="$2"
    local prefix="${3:-${TARGET_DIR}}"
    local dst=$(template_dst "$src" "$prefix")

    # Note: render.py will make the needed directories!

    python3 render.py $src $dst -m $mode
}

template_render_queue() {

    # Process entries by index
    for i in "${!TEMPLATE_SRCS[@]}"; do
        src="${TEMPLATE_SRCS[$i]}"
        mode="${TEMPLATE_MODES[$i]}"

        template_render $src $mode
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
