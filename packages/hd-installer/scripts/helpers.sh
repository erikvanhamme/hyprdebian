#!/bin/bash
# General helpers: asking questions and logging. Every answer is stored in its variable and saved
# in the config, and on a rerun the saved answer is offered as the default. A question that the
# answers file answers is not asked: its answer is checked as a typed one would be, and an answer
# that is not valid fails the question.

# Reads a line of text into a variable, without saving it.
# Usage: read_answer <variable> <prompt> <offered>
# An empty line gives the offered answer.
read_answer() {
    local var_name="$1"
    local prompt="$2"
    local offered="$3"
    local input

    if [[ -n "${offered}" ]]; then
        if ! read -rp "${prompt} [${offered}]: " input; then
            echo >&2
            echo "Error: No answer for ${var_name}." >&2
            return 1
        fi
        input="${input:-${offered}}"
    elif ! read -rp "${prompt}: " input; then
        echo >&2
        echo "Error: No answer for ${var_name}." >&2
        return 1
    fi

    printf -v "${var_name}" "%s" "${input}"
}

# Marks packages to install in the target. pkg_install installs them all in one run.
# Usage: add_packages <package>...
add_packages() {
    local package check

    for package in "$@"; do
        for check in "${PACKAGES[@]}"; do
            if [[ "${check}" == "${package}" ]]; then
                continue 2
            fi
        done
        PACKAGES+=("${package}")
    done

    save_config PACKAGES "${PACKAGES[*]}" # Persist as space-separated string.
}

# Adds groups that the user becomes a member of in the user phase.
# Usage: add_user_groups <group>...
add_user_groups() {
    local group check

    for group in "$@"; do
        for check in "${USER_GROUPS[@]}"; do
            if [[ "${check}" == "${group}" ]]; then
                continue 2
            fi
        done
        USER_GROUPS+=("${group}")
    done

    save_config USER_GROUPS "${USER_GROUPS[*]}" # Persist as space-separated string.
}

# Runs a command in the target, with a clean environment.
# Usage: in_target <command> [argument]...
in_target() {
    chroot "${TARGET_DIR}" /usr/bin/env -i \
        HOME=/root \
        TERM="${TERM:-linux}" \
        PATH=/usr/sbin:/usr/bin:/sbin:/bin \
        "$@"
}

# Copies a file of the installer from deploy/ to the same path in the target.
# Usage: file_deploy <deploy/path>
file_deploy() {
    local src="$1"
    local dst="${TARGET_DIR}/${src#deploy/}"

    mkdir -p "$(dirname "${dst}")" || return 1

    echo "cp ${src} ${dst}"
    cp "${INSTALLER_DIR}/${src}" "${dst}"
}

# Renders a jinja2 template of the installer from templates/ to the same path in the target,
# without .j2. The template gets the answers, except the password, and the extra variables.
# Usage: template_render <templates/path.j2> [mode] [NAME=value]...
template_render() {
    local src="$1"
    local mode="${2:-0644}"
    shift
    if [[ $# -gt 0 ]]; then
        shift
    fi
    local rel="${src#templates/}"
    local dst="${TARGET_DIR}/${rel%.j2}"
    local vars=("PATH=${PATH}")
    local name

    for name in $(compgen -v Q_); do
        if [[ "${name}" == "Q_PASSWORD" ]]; then
            continue
        fi
        vars+=("${name}=${!name}")
    done

    env -i "${vars[@]}" "$@" python3 "${INSTALLER_DIR}/render.py" "${INSTALLER_DIR}/${src}" \
        "${dst}" -m "${mode}"
}

# Asks for a line of text, and stores it.
# Usage: ask <variable> <prompt> <default>
# The saved answer is offered first, then the default; without either, the answer may be empty.
ask() {
    local var_name="$1"
    local prompt="$2"
    local default="$3"

    if answered "${var_name}"; then
        show_answer "${prompt}" "${ANSWERS[${var_name}]}"
        set_answer "${var_name}" "${ANSWERS[${var_name}]}"
        return 0
    fi

    read_answer "${var_name}" "${prompt}" "${!var_name:-${default}}" || return 1
    set_answer "${var_name}" "${!var_name}"
}

# Asks for a line of text until a check accepts it, and stores it.
# Usage: ask_valid <variable> <prompt> <default> <check> <message>
# The check is a function that gets the answer and succeeds when it is valid; otherwise the
# message is shown and the question is asked again. Only a valid answer is stored. The saved
# answer is offered first, then the default.
ask_valid() {
    local var_name="$1"
    local prompt="$2"
    local default="$3"
    local check="$4"
    local message="$5"
    local offered="${!var_name:-${default}}"

    if answered "${var_name}"; then
        show_answer "${prompt}" "${ANSWERS[${var_name}]}"
        if ! "${check}" "${ANSWERS[${var_name}]}"; then
            echo "Error: ${message}" >&2
            return 1
        fi
        set_answer "${var_name}" "${ANSWERS[${var_name}]}"
        return 0
    fi

    while true; do
        read_answer "${var_name}" "${prompt}" "${offered}" || return 1
        if "${check}" "${!var_name}"; then
            set_answer "${var_name}" "${!var_name}"
            return 0
        fi
        echo "${message}" >&2
        offered="${default}"
    done
}

# Asks for a password twice, without showing it, and keeps it in the variable. It is not saved
# in the config, nor passed in the environment of other commands.
# Usage: ask_password <variable> <prompt>
# The password must be at least 8 characters, the least that ZFS takes as a passphrase.
ask_password() {
    local var_name="$1"
    local prompt="$2"
    local pw1 pw2

    if answered "${var_name}"; then
        echo "${prompt}: (answers file)"
        if (( ${#ANSWERS[${var_name}]} < 8 )); then
            echo "Error: The password must be at least 8 characters." >&2
            return 1
        fi
        printf -v "${var_name}" "%s" "${ANSWERS[${var_name}]}"
        export -n "${var_name}"
        return 0
    fi

    while true; do
        if ! read -rsp "${prompt}: " pw1; then
            echo >&2
            echo "Error: No answer for ${var_name}." >&2
            return 1
        fi
        echo
        if ! read -rsp "Repeat the password: " pw2; then
            echo >&2
            echo "Error: No answer for ${var_name}." >&2
            return 1
        fi
        echo

        if [[ "${pw1}" != "${pw2}" ]]; then
            echo "The passwords differ." >&2
        elif (( ${#pw1} < 8 )); then
            echo "The password must be at least 8 characters." >&2
        else
            break
        fi
    done

    printf -v "${var_name}" "%s" "${pw1}"
    export -n "${var_name}"
}

# Asks to pick one of the options by number, and stores the option.
# Usage: ask_options <variable> <prompt> <default> <option>...
# The saved answer is offered first, then the default, then the first option.
ask_options() {
    local var_name="$1"
    local prompt="$2"
    local default="$3"
    shift 3
    local options=("$@")
    local count=${#options[@]}
    local current="${!var_name:-}"
    local current_idx=""
    local default_idx=""
    local i input choice

    if (( count == 0 )); then
        echo "Error: No options for ${var_name}." >&2
        return 1
    fi

    for (( i = 0; i < count; i++ )); do
        if [[ -n "${current}" && "${options[i]}" == "${current}" ]]; then
            current_idx=$((i + 1))
        fi
        if [[ -n "${default}" && "${options[i]}" == "${default}" ]]; then
            default_idx=$((i + 1))
        fi
    done

    if [[ -n "${default}" && -z "${default_idx}" ]]; then
        echo "Error: The default ${default} of ${var_name} is not one of its options." >&2
        return 1
    fi
    default_idx="${current_idx:-${default_idx:-1}}"

    if answered "${var_name}"; then
        show_answer "${prompt}" "${ANSWERS[${var_name}]}"
        for (( i = 0; i < count; i++ )); do
            if [[ "${options[i]}" == "${ANSWERS[${var_name}]}" ]]; then
                set_answer "${var_name}" "${options[i]}"
                return 0
            fi
        done
        echo "Error: The answer of ${var_name} must be one of: ${options[*]}." >&2
        return 1
    fi

    for (( i = 0; i < count; i++ )); do
        printf "%2d) %s\n" "$((i + 1))" "${options[i]}"
    done

    while true; do
        if ! read -rp "${prompt} [${default_idx}]: " input; then
            echo >&2
            echo "Error: No answer for ${var_name}." >&2
            return 1
        fi
        input="${input:-${default_idx}}"
        if [[ "${input}" =~ ^[0-9]+$ ]] && (( input >= 1 && input <= count )); then
            choice="${options[input - 1]}"
            break
        fi
        echo "Please enter a number from 1 to ${count}." >&2
    done

    printf -v "${var_name}" "%s" "${choice}"
    save_config "${var_name}" "${choice}"
}

# Asks a yes or no question, and stores true or false.
# Usage: ask_yes_no <variable> <prompt> <default>
# The default is y or n. The saved answer is offered first, then the default. Succeeds when the
# question is answered; the answer is in the variable.
ask_yes_no() {
    local var_name="$1"
    local prompt="$2"
    local default="$3"
    local current input choices

    if answered "${var_name}"; then
        show_answer "${prompt}" "${ANSWERS[${var_name}]}"
        set_answer "${var_name}" "${ANSWERS[${var_name}]}"
        return 0
    fi

    case "${!var_name:-}" in
        true) current="y" ;;
        false) current="n" ;;
        *) current="${default}" ;;
    esac
    case "${current}" in
        y) choices="Y/n" ;;
        n) choices="y/N" ;;
        *)
            echo "Error: The default of ${var_name} must be y or n." >&2
            return 1
            ;;
    esac

    while true; do
        if ! read -rp "${prompt} (${choices}): " input; then
            echo >&2
            echo "Error: No answer for ${var_name}." >&2
            return 1
        fi
        input="${input:-${current}}"
        case "${input}" in
            [Yy]*)
                set_answer "${var_name}" true
                return 0
                ;;
            [Nn]*)
                set_answer "${var_name}" false
                return 0
                ;;
        esac
        echo "Please answer y or n." >&2
    done
}

# Asks to confirm something that cannot be undone. There is no default, the answer must be
# typed, and it is not saved. Succeeds for yes.
# Usage: confirm <prompt>
confirm() {
    local prompt="$1"
    local input

    while true; do
        if ! read -rp "${prompt} (y/n): " input; then
            echo >&2
            return 1
        fi
        case "${input}" in
            [Yy]*) return 0 ;;
            [Nn]*) return 1 ;;
        esac
        echo "Please answer y or n." >&2
    done
}

# Stores a value without asking, for a question whose answer follows from earlier ones.
# Usage: set_answer <variable> <value>
set_answer() {
    local var_name="$1"
    local value="$2"

    printf -v "${var_name}" "%s" "${value}"
    save_config "${var_name}" "${value}"
}

log_status() {
    local color="$1"
    local label="$2"
    local task="$3"
    local timestamp

    printf -v timestamp "%(%H:%M:%S)T" -1
    printf "%b[%s]%b <%s> %s\n" "${color}" "${label}" "${COLOR_RESET}" "${timestamp}" "${task}"
}
