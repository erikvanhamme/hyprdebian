#!/bin/bash

load_config() {

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
