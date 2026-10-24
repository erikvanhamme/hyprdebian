#!/bin/bash
# The answers file: a JSON file with answers to the questions and the debconf questions, for an
# installation without questions, read by answers.py. A question that the file answers is not
# asked; the helpers in helpers.sh check its answer instead. The answers are read again on every
# run, as the password is never saved; the path of the file is saved in the config, so a rerun
# without --answers uses the same file.

# The answers of the file, by the variable of their question, and the debconf selections in
# DEBCONF.
declare -A ANSWERS=()

# Reads the answers file, given with --answers or saved by an earlier run, into ANSWERS.
# Usage: load_answers [file]
load_answers() {
    local file="${1:-${ANSWERS_FILE:-}}"
    local key value

    if [[ -z "${file}" ]]; then
        return 0
    fi

    file="$(readlink -f "${file}")"
    if [[ ! -f "${file}" ]]; then
        echo "Error: The answers file ${file} does not exist." >&2
        return 1
    fi

    while IFS= read -r -d '' key && IFS= read -r -d '' value; do
        ANSWERS["${key}"]="${value}"
    done < <(python3 "${INSTALLER_DIR}/answers.py" "${file}")
    if ! wait $!; then
        return 1
    fi

    save_config ANSWERS_FILE "${file}"
    echo "Answers from ${file}."
}

# Succeeds when the answers file answers the question of a variable.
# Usage: answered <variable>
answered() {
    [[ -n "${ANSWERS[$1]+set}" ]]
}

# Shows the answer of the answers file to a question, in place of asking it.
# Usage: show_answer <prompt> <answer>
show_answer() {
    printf '%s: %s (answers file)\n' "$1" "$2"
}

# Succeeds when the answers file has debconf selections of a package.
# Usage: debconf_answered <package>
debconf_answered() {
    local line

    if ! answered DEBCONF; then
        return 1
    fi

    while IFS= read -r line; do
        if [[ "${line%%[[:space:]]*}" == "$1" ]]; then
            return 0
        fi
    done <<< "${ANSWERS[DEBCONF]}"
    return 1
}
