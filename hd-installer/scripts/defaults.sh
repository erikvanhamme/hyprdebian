#!/bin/bash
# The machine types, the suites and the defaults of the questions.
#
# Every machine type has a table of defaults, DEFAULTS_<type>, keyed by the variable of the
# question. DEFAULTS_common holds the defaults that are the same for every type: a lookup checks
# the table of the type first, then DEFAULTS_common. Before the type is asked, only
# DEFAULTS_common is used. To add a machine type, add it to MACHINE_TYPES and declare its table.

# The machine types, in the order they are offered.
MACHINE_TYPES=(desktop laptop server vm)

# The Debian suites of the HyprDebian package repository. Desktops and laptops always use
# unstable.
SUITES=(unstable trixie)

declare -gA DEFAULTS_common=(
    [Q_TYPE]="desktop"
    [Q_SUITE]="unstable"
    [Q_ROOT_REDUNDANT]="n"
    [Q_ENCRYPT]="y"
    [Q_SWAP]="2"
    [Q_USER]="rsq"
    [Q_FULLNAME]="Erik Van Hamme"
    # The default of the FQDN is the host name followed by this domain.
    [Q_DOMAIN]="rsq-online.ddns.net"
    [Q_WIFI]="n"
    [Q_REPO]="http://repo/"
)

declare -gA DEFAULTS_desktop=(
    [Q_HOSTNAME]="hyprdebian"
)
declare -gA DEFAULTS_laptop=(
    [Q_HOSTNAME]="hyprlaptop"
    [Q_WIFI]="y"
)
declare -gA DEFAULTS_server=(
    [Q_ROOT_REDUNDANT]="y"
    [Q_HOSTNAME]="hyprserver"
)
declare -gA DEFAULTS_vm=(
    [Q_ENCRYPT]="n"
    [Q_HOSTNAME]="hyprvm"
    [Q_IFACE]="enp1s0"
)

is_machine_type() {
    local type="$1"
    local check

    for check in "${MACHINE_TYPES[@]}"; do
        if [[ "${check}" == "${type}" ]]; then
            return 0
        fi
    done
    return 1
}

# Prints the default of a question for the machine type, if there is one.
# Usage: get_default <variable> [machine type]
# Prints nothing when there is no default.
get_default() {
    local key="$1"
    local type="${2:-}"

    if [[ -n "${type}" ]]; then
        if ! is_machine_type "${type}"; then
            echo "Error: Unknown machine type ${type}." >&2
            return 1
        fi
        local -n type_defaults="DEFAULTS_${type}"
        if [[ -n "${type_defaults[${key}]+set}" ]]; then
            printf '%s\n' "${type_defaults[${key}]}"
            return 0
        fi
    fi

    if [[ -n "${DEFAULTS_common[${key}]+set}" ]]; then
        printf '%s\n' "${DEFAULTS_common[${key}]}"
    fi
}
