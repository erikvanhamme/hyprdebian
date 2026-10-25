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
    [Q_DEBCONF_DEFAULTS]="y"
)

# The packages whose debconf questions b_console asks: the locales, the time zone, the keyboard and
# the console font.
DEBCONF_PACKAGES=(locales tzdata keyboard-configuration console-setup)

# The default debconf selections of DEBCONF_PACKAGES, each owner question type value, which
# b_debconf sets when Q_DEBCONF_DEFAULTS is true, and their summary for the question.
DEBCONF_DEFAULTS=(
    "locales locales/locales_to_be_generated multiselect en_US.UTF-8 UTF-8"
    "locales locales/default_environment_locale select en_US.UTF-8"
    "tzdata tzdata/Areas select Europe"
    "tzdata tzdata/Zones/Europe select Brussels"
    "keyboard-configuration keyboard-configuration/model select Generic 105-key PC"
    "keyboard-configuration keyboard-configuration/xkb-keymap select us"
    "keyboard-configuration keyboard-configuration/variant select English (US)"
    "console-setup console-setup/charmap47 select UTF-8"
    "console-setup console-setup/codeset47 select Guess optimal character set"
    "console-setup console-setup/fontface47 select TerminusBold"
    "console-setup console-setup/fontsize-fb47 select 8x16"
    "console-setup console-setup/fontsize-text47 select 8x16"
)
DEBCONF_DEFAULTS_SUMMARY=(
    "Locale:       en_US.UTF-8"
    "Time zone:    Europe/Brussels"
    "Keyboard:     English (US), Generic 105-key PC"
    "Console font: TerminusBold 8x16"
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

# Prints the default debconf selections of a package, one per line, when Q_DEBCONF_DEFAULTS is
# true and the answers file has no selections of the package, which take its place.
# Usage: debconf_defaults <package>
debconf_defaults() {
    local package="$1"
    local line

    if [[ "${Q_DEBCONF_DEFAULTS}" != "true" ]]; then
        return 0
    fi
    if debconf_answered "${package}"; then
        return 0
    fi

    for line in "${DEBCONF_DEFAULTS[@]}"; do
        if [[ "${line%%[[:space:]]*}" == "${package}" ]]; then
            printf '%s\n' "${line}"
        fi
    done
}

# Succeeds when a package gets debconf selections, from the answers file or the defaults, so it is
# configured without questions.
# Usage: debconf_selected <package>
debconf_selected() {
    if debconf_answered "$1"; then
        return 0
    fi
    if [[ -n "$(debconf_defaults "$1")" ]]; then
        return 0
    fi
    return 1
}
