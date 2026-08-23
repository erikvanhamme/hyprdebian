#!/bin/bash
set -eo pipefail
exec 2>&1

# Start banner.
echo "=== Erik's nifty debian+hyprland installer v0.59 ==="

# Set up idempotency and config paths.
TARGET_DIR=/mnt
STATE_DIR="/tmp/hyprdebian"
mkdir -p "$STATE_DIR"
CONFIG_FILE="$STATE_DIR/config"
declare -A TASK_DEPS

# Load config and restore/default variables.
if [[ -f "$CONFIG_FILE" ]]; then
    source "$CONFIG_FILE"
fi

if [[ -n "${USER_GROUPS:-}" ]]; then
    read -r -a USER_GROUPS <<< "$USER_GROUPS" # Convert string back to array.
fi

if [[ -n "${PACKAGES:-}" ]]; then
    read -r -a PACKAGES <<< "$PACKAGES" # Convert string back to array.
fi

if [[ -n "${SERVICES:-}" ]]; then
    read -r -a SERVICESS <<< "$SERVICES" # Convert string back to array.
fi

if [[ -z "${USER_GROUPS:-}" ]]; then
    USER_GROUPS=(
        audio
        cdrom
        dip
        input
        plugdev
        sudo
        video
    )
fi

# Color constants
if [[ -t 1 ]]; then
    COLOR_RESET="\033[0m"
    COLOR_GREEN="\033[32m"
    COLOR_YELLOW="\033[33m"
    COLOR_BLUE="\033[34m"
    COLOR_RED="\033[31m"
else
    COLOR_RESET=""
    COLOR_GREEN=""
    COLOR_YELLOW=""
    COLOR_BLUE=""
    COLOR_RED=""
fi

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

is_done() {
    [[ -f "$STATE_DIR/$1.done" ]]
}

mark_done() {
    touch "$STATE_DIR/$1.done"
}

in_target() {
    chroot "${TARGET_DIR}" /usr/bin/env -i \
        HOME=/root \
        TERM="${TERM}" \
        PATH=/usr/sbin:/usr/bin:/sbin:/bin \
        "$@"
}

write_file() {
    local path="$1"
    local mode="$2"

    install -d "$(dirname "$path")"
    install -m "$mode" /dev/null "$path"
    cat > "$path"
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

save_config() {
    local key="$1"
    local value="$2"

    # Use a regex that matches the key even if preceded by 'declare ... '
    # This removes lines like 'key="val"' OR 'declare -A key=(...)'
    grep -Ev "^(declare [-a-zA-Z]+ )?${key}=" "$CONFIG_FILE" 2>/dev/null > "$CONFIG_FILE.tmp" || true

    if [[ "$key" == "TASK_DEPS" ]]; then
        declare -p TASK_DEPS >> "$CONFIG_FILE.tmp"
    else
        printf '%s="%s"\n' "$key" "$value" >> "$CONFIG_FILE.tmp"
    fi

    mv "$CONFIG_FILE.tmp" "$CONFIG_FILE"
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

# Task and dependency handling.
add_dependencies() {
    local task_id=$1
    shift # Remove the first argument (task_id), leaving only dependencies.

    local current_deps=${TASK_DEPS["$task_id"]}

    for new_dep in "$@"; do
        # Check if the dependency is already in the string to avoid duplicates.
        if [[ ! ",$current_deps," =~ ",$new_dep," ]]; then
            current_deps="${current_deps}${current_deps:+,}$new_dep"
        fi
    done

    # Update the array and save.
    TASK_DEPS["$task_id"]="$current_deps"
    save_config "TASK_DEPS" ""
}

execute_task() {
    local func_name=$1
    local display_name=${2:-$func_name}

    # Handle Dependencies first.
    local deps_list=${TASK_DEPS["$func_name"]}
    if [[ -n "$deps_list" ]]; then
        IFS=',' read -ra ADDR <<< "$deps_list"
        for dep in "${ADDR[@]}"; do
            execute_task "$dep"
        done
    fi

    # Idempotency check.
    if is_done "$func_name"; then
        log_status "$COLOR_BLUE" "SKIPPED" "$display_name"
        return 0
    fi

    log_status "$COLOR_YELLOW" "START  " "$display_name"

    if "$func_name"; then
        mark_done "$func_name"
        log_status "$COLOR_GREEN" "DONE   " "$display_name"
    else
        log_status "$COLOR_RED" "FAIL   " "$display_name"
        return 1
    fi
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

# Packages task.
t_packages() {
    in_target apt install -y ${PACKAGES[*]}
}

# Services task.
t_services() {
    for svc in ${SERVICES}; do
        in_target systemctl enable $svc
    done
}

# Installation task.
t_install() {
    return 0
}

# Load tasks for all the phases.
source scripts/t_info.sh
source scripts/t_prereq.sh
source scripts/t_disk.sh
source scripts/t_base.sh
source scripts/t_optional.sh
add_dependencies "t_install" "t_packages" "t_services"
source scripts/t_user.sh
source scripts/t_cleanup.sh

# Start Installation.
execute_task "t_install"
