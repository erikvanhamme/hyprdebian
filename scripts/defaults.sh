#!/bin/bash

# Defaults for the installer questions, per machine type.
#
# Every machine type has its own table, named DEFAULTS_<type>. The keys are
# the names of the question variables (Q_DESKTOP, Q_QEMU_KVM, ...), the values
# are the defaults to pass to the ask functions. Yes/no questions use "y" and
# "n", the defaults ask_yes_no takes.
#
# DEFAULTS_common holds the defaults that are the same for every machine type.
# A machine type table only needs the keys where that type differs: lookups
# check the machine type table first and fall back to DEFAULTS_common. Before
# the machine type is selected, lookups only use DEFAULTS_common.
#
# To add a machine type, add it to MACHINE_TYPES and declare its table below.
#
# When the user chooses to use the machine type defaults (Q_DEFAULTS), the
# questions that have a default are answered with it without asking, except
# the ones in DEFAULTS_ALWAYS_ASK. Questions without a default are always asked.

# Known machine types. The order is the order in which they are offered.
MACHINE_TYPES=(desktop laptop server vm)

# Questions that are always asked, even when the machine type defaults are used.
DEFAULTS_ALWAYS_ASK=(
    Q_DEFAULTS
)

# Defaults shared by all machine types.
declare -gA DEFAULTS_common=(
    [Q_DEFAULTS]="y"
    [Q_KERNEL_REPO]="stable"
    [Q_SPECIFIC_KERNEL]="n"
    [Q_CUPS]="n"
    [Q_RUST]="n"
    [Q_REPORT]="y"
    [Q_REPORT_URL]="http://dash.rsq-online.ddns.net/"
)

declare -gA DEFAULTS_desktop=(
    [Q_DESKTOP]="y"
    [Q_QEMU_KVM]="n"
    [Q_BACKUPTOOLS]="y"
)

declare -gA DEFAULTS_laptop=(
    [Q_DESKTOP]="y"
    [Q_QEMU_KVM]="n"
    [Q_BACKUPTOOLS]="y"
)

declare -gA DEFAULTS_server=(
    [Q_DESKTOP]="n"
    [Q_QEMU_KVM]="y"
    [Q_BACKUPTOOLS]="y"
)

declare -gA DEFAULTS_vm=(
    [Q_DESKTOP]="n"
    [Q_QEMU_KVM]="n"
    [Q_BACKUPTOOLS]="n"
)

# Prints the default for a question on a machine type.
# Usage: get_default <key> [machine type]
# Without a machine type (it is not selected yet), only DEFAULTS_common is used.
# Returns 1 when there is no default for the key, 2 when the machine type is unknown.
get_default() {
    local key="$1"
    local type="${2:-}"

    if [[ -n "$type" ]]; then
        if ! is_machine_type "$type"; then
            echo "Error: Unknown machine type '$type'" >&2
            return 2
        fi

        local -n type_defaults="DEFAULTS_${type}"

        if [[ -n "${type_defaults[$key]+set}" ]]; then
            printf '%s\n' "${type_defaults[$key]}"
            return 0
        fi
    fi

    if [[ -n "${DEFAULTS_common[$key]+set}" ]]; then
        printf '%s\n' "${DEFAULTS_common[$key]}"
    else
        return 1
    fi
}

# Succeeds when a question should be answered with its default without asking.
# Usage: accept_default <var> <default>
accept_default() {
    local var_name="$1"
    local default="$2"
    local check

    [[ "${Q_DEFAULTS:-}" == "true" && -n "$default" ]] || return 1

    for check in "${DEFAULTS_ALWAYS_ASK[@]}"; do
        [[ "$check" == "$var_name" ]] && return 1
    done

    return 0
}

# Succeeds when the argument is a known machine type.
is_machine_type() {
    local type="$1"
    local check

    for check in "${MACHINE_TYPES[@]}"; do
        [[ "$check" == "$type" ]] && return 0
    done

    return 1
}

# Every machine type needs a table, or lookups for it would fail.
for _type in "${MACHINE_TYPES[@]}"; do
    if ! declare -p "DEFAULTS_${_type}" &> /dev/null; then
        echo "Error: No defaults table DEFAULTS_${_type} for machine type '${_type}'" >&2
        exit 1
    fi
done
unset _type
