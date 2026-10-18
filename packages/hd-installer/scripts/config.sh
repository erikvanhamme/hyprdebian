#!/bin/bash
# The answers and the task dependencies, kept in CONFIG_FILE as variable assignments that are
# sourced again on the next run.

load_config() {
    if [[ -f "${CONFIG_FILE}" ]]; then
        source "${CONFIG_FILE}"
    fi

    # The packages and the groups are saved as a string of names separated by spaces.
    if [[ -n "${PACKAGES:-}" ]]; then
        read -r -a PACKAGES <<< "${PACKAGES}"
    else
        PACKAGES=()
    fi

    if [[ -n "${USER_GROUPS:-}" ]]; then
        read -r -a USER_GROUPS <<< "${USER_GROUPS}"
    else
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

# Stores a variable in CONFIG_FILE, replacing its earlier value.
# Usage: save_config <name> <value>
# TASK_DEPS is stored as the associative array it is; the value is then ignored.
#
# Tasks run without set -e and callers do not check this, while a lost answer only shows up on
# a rerun, so a failure stops the installer here.
save_config() {
    local key="$1"
    local value="$2"
    local tmp="${CONFIG_FILE}.tmp"
    local status=0

    : > "${tmp}" || status=2

    # Keeps every line except those of the key, also when it was stored with declare. grep exits
    # with 1 when no lines are left, which is fine, and with 2 on an error.
    if (( status == 0 )) && [[ -f "${CONFIG_FILE}" ]]; then
        grep -Ev "^(declare [-a-zA-Z]+ )?${key}=" "${CONFIG_FILE}" >> "${tmp}" || status=$?
    fi

    if (( status <= 1 )); then
        status=0
        if [[ "${key}" == "TASK_DEPS" ]]; then
            # declare -gA, so sourcing it from load_config sets the global.
            declare -p TASK_DEPS | sed 's/^declare -A /declare -gA /' >> "${tmp}" || status=$?
        else
            # %q quotes the value, so sourcing the config gives it back unchanged and never
            # runs anything in it, such as $(...) in an answer.
            printf '%s=%q\n' "${key}" "${value}" >> "${tmp}" || status=$?
        fi
    fi

    if (( status == 0 )) && mv "${tmp}" "${CONFIG_FILE}"; then
        return 0
    fi

    echo "Error: Unable to save ${key} to ${CONFIG_FILE}." >&2
    exit 1
}
