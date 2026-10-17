#!/bin/bash

add_files() {
    for file in "$@"; do
        for check in "${FILES[@]}"; do
            [[ "$check" == "$file" ]] && continue 2
        done

        FILES+=("$file")
    done

    save_config FILES "${FILES[*]}" # Persist as space-separated string.
}

add_packages() {
    for package in "$@"; do
        for check in "${PACKAGES[@]}"; do
            [[ "$check" == "$package" ]] && continue 2
        done

        PACKAGES+=("$package")
    done

    save_config PACKAGES "${PACKAGES[*]}" # Persist as space-separated string.
}

add_services() {
    for service in "$@"; do
        for check in "${SERVICES[@]}"; do
            [[ "$check" == "$service" ]] && continue 2
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
            [[ "$check" == "$group" ]] && continue 2
        done

        USER_GROUPS+=("$group")
    done

    save_config USER_GROUPS "${USER_GROUPS[*]}" # Persist as space-separated string.
}

# Asks for a value. The optional default is used when the answer is empty.
# With a default, the question is not asked when accept_default says so.
ask() {
    local var_name="$1"
    local prompt="$2"
    local default="${3:-}"

    local current="${!var_name:-$default}"

    if accept_default "$var_name" "$default"; then
        echo "$prompt: $current"
        input="$current"
    elif [[ -n "${current:-}" ]]; then
        read -rp "$prompt [$current]: " input
        input="${input:-$current}"
    else
        read -rp "$prompt: " input
    fi

    printf -v "$var_name" "%s" "$input"

    save_config "$var_name" "$input"
}

# Asks a yes/no question. The optional default ("y" or "n") is used when the answer is empty.
# A value already set for the variable ("true" or "false") wins over the default.
# With a default, the question is not asked when accept_default says so.
ask_yes_no() {
    local var_name=$1
    local prompt=$2
    local default="${3:-}"
    local input choices current

    case "$default" in
        "y"|"n"|"") ;;
        *)
            echo "Error: Invalid default '$default' for ask_yes_no, must be y or n" >&2
            return 2
            ;;
    esac

    # Prefer the current value, then the default.
    case "${!var_name:-}" in
        "true") current="y" ;;
        "false") current="n" ;;
        *) current="$default" ;;
    esac

    case "$current" in
        "y") choices="Y/n" ;;
        "n") choices="y/N" ;;
        "") choices="y/n" ;;
    esac

    while true; do
        if accept_default "$var_name" "$default"; then
            echo "$prompt: $current"
            input="$current"
        else
            read -rp "$prompt ($choices): " input
            input="${input:-$current}"
        fi

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
    mkdir -p "$dir" || return 1

    echo "cp $src $dst"
    cp $src $dst
}

file_deploy_queue() {
    for file in ${FILES[@]}; do
        file_deploy $file || return 1
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

# Prints a random token of 32 hex characters.
random_token() {
    od -An -N16 -tx1 /dev/urandom | tr -d ' \n'
    echo
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
    local mode="${2:-0644}"
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

        template_render $src $mode || return 1
    done
}

write_file() {
    local path="$1"
    local mode="$2"

    # command runs /usr/bin/install, not the root task install of install.sh.
    command install -d "$(dirname "$path")"
    command install -m "$mode" /dev/null "$path"
    cat > "$path"
}
