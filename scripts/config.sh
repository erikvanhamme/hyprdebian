#!/bin/bash

load_config() {

    # Load config and restore/default variables.
    if [[ -f "$CONFIG_FILE" ]]; then
        source "$CONFIG_FILE"

        if [[ "${Q_REDUNDANT}" == "true" ]]; then
            Q_DISKS="${Q_DISK_A} ${Q_DISK_B}"
        else
            Q_DISKS="${Q_DISK}"
        fi
    fi

    if [[ -n "${USER_GROUPS:-}" ]]; then
        read -r -a USER_GROUPS <<< "$USER_GROUPS" # Convert string back to array.
    fi

    if [[ -n "${PACKAGES:-}" ]]; then
        read -r -a PACKAGES <<< "$PACKAGES" # Convert string back to array.
    fi

    if [[ -n "${SERVICES:-}" ]]; then
        read -r -a SERVICES <<< "$SERVICES" # Convert string back to array.
    fi

    if [[ -n "${TEMPLATE_SRCS:-}" ]]; then
        read -r -a TEMPLATE_SRCS <<< "$TEMPLATE_SRCS" # Convert string back to array.
    fi

    if [[ -n "${TEMPLATE_MODES:-}" ]]; then
        read -r -a TEMPLATE_MODES <<< "$TEMPLATE_MODES" # Convert string back to array.
    fi

    if [[ -n "${FILES:-}" ]]; then
        read -r -a FILES <<< "$FILES" # Convert string back to array.
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
}

save_config() {
    local key="$1"
    local value="$2"
    local status=0

    # Use a regex that matches the key even if preceded by 'declare ... '
    # This removes lines like 'key="val"' OR 'declare -A key=(...)'
    # grep exits with 1 when no lines are left, which is fine, and with 2 on an error.
    : > "$CONFIG_FILE.tmp" || status=2

    if (( status == 0 )) && [[ -f "$CONFIG_FILE" ]]; then
        grep -Ev "^(declare [-a-zA-Z]+ )?${key}=" "$CONFIG_FILE" >> "$CONFIG_FILE.tmp" || status=$?
    fi

    if (( status <= 1 )); then
        status=0

        if [[ "$key" == "TASK_DEPS" ]]; then
            # Save with 'declare -gA' so sourcing it from load_config sets the global.
            declare -p TASK_DEPS | sed 's/^declare -A /declare -gA /' >> "$CONFIG_FILE.tmp" || status=$?
        else
            # %q quotes the value, so sourcing the config gives it back unchanged and never runs
            # anything in it, like $(...) in an answer.
            printf '%s=%q\n' "$key" "$value" >> "$CONFIG_FILE.tmp" || status=$?
        fi
    fi

    # Only replace the config when the new one is complete. Callers do not check this, and a
    # lost value only shows up after a restart, so stop the installer here.
    if (( status == 0 )) && mv "$CONFIG_FILE.tmp" "$CONFIG_FILE"; then
        return 0
    fi

    echo "ERROR: Unable to save ${key} to ${CONFIG_FILE}." >&2
    exit 1
}
