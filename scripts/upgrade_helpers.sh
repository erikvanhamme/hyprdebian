#!/bin/bash

# The upgrade works on the running system, not on a target mounted at /mnt.
TARGET_DIR=""

# Keep the upgrade state apart from an installation, and start fresh on every run.
STATE_DIR="/tmp/hyprdebian-upgrade"
CONFIG_FILE="$STATE_DIR/config"

in_target() {
    "$@"
}

# Answer the questions with the saved answers. Only questions without a saved answer, like
# questions added after the installation, are asked. Answers that were worked out from the
# system are all asked, with the detected answer filled in, so the user can confirm them.
accept_default() {
    local var_name="$1"

    [[ "${CONFIRM_ANSWERS:-false}" != "true" && -n "${!var_name:-}" ]]
}

# Succeeds when a package is installed.
is_installed() {
    [[ "$(dpkg-query -W -f='${db:Status-Status}' "$1" 2>/dev/null)" == "installed" ]]
}

# Checks that the packages of the system are up to date. The upgrade installs the new packages
# with their latest versions, which can pull in upgrades of the packages they depend on. The user
# chooses to stop and upgrade the system first, or to continue.
check_up_to_date() {
    local count answer

    in_target apt update || return 1
    count=$(in_target apt-get -s full-upgrade 2>/dev/null | grep -c '^Inst ') || true

    if (( count == 0 )); then
        return 0
    fi

    echo "The system is not up to date, ${count} packages can be upgraded."
    echo "Upgrading it first is recommended: sudo apt full-upgrade"

    while true; do
        read -rp "Continue the upgrade anyway (y/N): " answer || return 1

        case "$answer" in
            [Yy]*)
                return 0
                ;;
            [Nn]*|"")
                echo "Upgrade stopped."
                exit 0
                ;;
            *)
                echo "Please answer y or n."
                ;;
        esac
    done
}

# Installs the packages the upgrade needs on the system, like python3-jinja2 to render the
# templates.
install_prerequisites() {
    local package
    local packages=()

    for package in python3-jinja2 mdadm; do
        is_installed "$package" || packages+=("$package")
    done

    if (( ${#packages[@]} == 0 )); then
        return 0
    fi

    echo "Installing the packages the upgrade needs: ${packages[*]}"
    in_target apt install --no-upgrade "${packages[@]}"
}

# Copies the answers of the installation to the upgrade config. The queues and dependencies in
# the saved config were worked out by the installer version that wrote it, so they are left out
# and worked out again. Without a saved config, the answers are worked out from the system.
load_installed_config() {
    local status=0

    if [[ ! -f "$INSTALLED_CONFIG_FILE" ]]; then
        echo "WARNING: ${INSTALLED_CONFIG_FILE} not found, the answers are worked out from the system."
        echo "Every question is asked with the detected answer filled in. Check them carefully."
        detect_config
        CONFIRM_ANSWERS="true"
        return 0
    fi

    grep -Ev "^(declare [-a-zA-Z]+ )?(TASK_DEPS|FILES|PACKAGES|SERVICES|TEMPLATE_SRCS|TEMPLATE_MODES|USER_GROUPS)=" \
        "$INSTALLED_CONFIG_FILE" > "$CONFIG_FILE" || status=$?

    if (( status > 1 )); then
        echo "ERROR: Unable to read ${INSTALLED_CONFIG_FILE}." >&2
        exit 1
    fi

    # The queues of the installed version, to find out what the new version adds.
    INSTALLED_PACKAGES=$(source "$INSTALLED_CONFIG_FILE" && echo "${PACKAGES:-}")
    INSTALLED_SERVICES=$(source "$INSTALLED_CONFIG_FILE" && echo "${SERVICES:-}")
    INSTALLED_USER_GROUPS=$(source "$INSTALLED_CONFIG_FILE" && echo "${USER_GROUPS:-}")
}

# Prints the words of the first list that are not in the second list.
# Usage: new_items <list> <list>
new_items() {
    local item check
    local result=()

    for item in $1; do
        for check in $2; do
            [[ "$check" == "$item" ]] && continue 2
        done

        result+=("$item")
    done

    echo "${result[*]}"
}

# Finds the Wayland session upgrade.sh was started from, by looking for WAYLAND_DISPLAY in the
# environment of its parent processes. Sets SESSION_USER and SESSION_ENV, or leaves them empty
# when there is none, for example when the upgrade was started over ssh.
find_session() {
    local pid=$$ environ

    SESSION_USER=""
    SESSION_ENV=()

    while [[ -n "$pid" ]] && (( pid > 1 )); do
        environ=$(tr '\0' '\n' 2>/dev/null < "/proc/$pid/environ") || environ=""

        if grep -q '^WAYLAND_DISPLAY=' <<< "$environ"; then
            SESSION_USER=$(stat -c %U "/proc/$pid")
            mapfile -t SESSION_ENV < <(grep -E '^(WAYLAND_DISPLAY|XDG_RUNTIME_DIR|DBUS_SESSION_BUS_ADDRESS)=' <<< "$environ")
            return 0
        fi

        pid=$(sed -n 's/^PPid:[[:space:]]*//p' "/proc/$pid/status" 2>/dev/null) || pid=""
    done
}

# Lets the user merge the new version of a file into the installed one, with meld when upgrade.sh
# was started from a Wayland session, and with sdiff otherwise. The file is left as it was when
# the merge is not finished.
# Usage: merge_file <new file> <installed file>
merge_file() {
    local src="$1"
    local dst="$2"
    local tmp merged status=0

    tmp=$(mktemp -d) || return 1

    if [[ -n "$SESSION_USER" ]] && command -v meld > /dev/null; then
        # meld runs as the session user, so it gets copies it can write.
        # The new version is on the left, the merge goes from left to right.
        echo "Merging in meld: new version on the left, save the right side to keep the merge."
        merged="$tmp/mine"
        cp "$dst" "$tmp/mine" && cp "$src" "$tmp/new" && chown -R "$SESSION_USER" "$tmp" || status=2

        if (( status == 0 )); then
            runuser -u "$SESSION_USER" -- env "${SESSION_ENV[@]}" meld \
                --label "New version" --label "${dst} (mine)" "$tmp/new" "$tmp/mine" || status=2
        fi
    else
        # sdiff exits with 1 when the files differ, and with 2 on trouble or when the user quits.
        # The new version is on the left, like in meld.
        echo "Merging with sdiff: l = take new, r = keep mine, e = edit, q = quit and leave the file as it is."
        merged="$tmp/merged"
        EDITOR="${EDITOR:-nano}" sdiff -s -o "$merged" -w "$(tput cols 2>/dev/null || echo 160)" "$src" "$dst" || status=$?
    fi

    # Writing into the file keeps its owner and mode.
    if (( status <= 1 )); then
        cat "$merged" > "$dst" || status=2
    else
        echo "WARNING: ${dst} was not merged and is left as it was."
    fi

    rm -rf "$tmp"
}

# Updates one installed file that the user may have changed. A missing file is added, owned by
# the user, a file that differs from the new version is kept, replaced or merged, as the user
# chooses.
# Usage: update_file <new file> <user> <installed file>
update_file() {
    local src="$1"
    local user="$2"
    local dst="$3"
    local choice

    if [[ ! -e "$dst" ]]; then
        echo "Adding ${dst}"
        runuser -u "$user" -- mkdir -p "$(dirname "$dst")" || return 1
        cp "$src" "$dst" || return 1
        chown "$user:$(id -gn "$user")" "$dst"
        return
    fi

    if cmp -s "$src" "$dst"; then
        return 0
    fi

    while true; do
        read -rp "${dst} differs from the new version. [k]eep mine, [t]ake new, [m]erge: " choice || return 1

        case "$choice" in
            [Kk]*)
                return 0
                ;;
            [Tt]*)
                cat "$src" > "$dst"
                return
                ;;
            [Mm]*)
                if grep -qI '' "$src"; then
                    merge_file "$src" "$dst"
                    return 0
                fi

                echo "${dst} is not a text file and cannot be merged."
                ;;
            *)
                echo "Please answer k, t or m."
                ;;
        esac
    done
}
