#!/bin/bash
# The suites and the defaults of the questions.

# The Debian suites of HyprDebian, in the order they are offered.
SUITES=(unstable trixie)

# The defaults, keyed by the variable of the question.
declare -gA DEFAULTS=(
    [Q_SUITE]="unstable"
    [Q_ROOT_REDUNDANT]="n"
    [Q_ENCRYPT]="y"
    [Q_SWAP]="2"
    [Q_USER]="rsq"
    [Q_FULLNAME]="Erik Van Hamme"
    [Q_HOSTNAME]="hyprdebian"
    # The default of the FQDN is the host name followed by this domain.
    [Q_DOMAIN]="rsq-online.ddns.net"
    [Q_WIFI]="n"
)

# Prints the default of a question, if there is one.
# Usage: get_default <variable>
# Prints nothing when there is no default.
get_default() {
    local key="$1"

    if [[ -n "${DEFAULTS[${key}]+set}" ]]; then
        printf '%s\n' "${DEFAULTS[${key}]}"
    fi
}
