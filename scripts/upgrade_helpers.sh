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
# questions added after the installation, are asked.
accept_default() {
    local var_name="$1"

    [[ -n "${!var_name:-}" ]]
}

# Copies the answers of the installation to the upgrade config. The queues and dependencies in
# the saved config were worked out by the installer version that wrote it, so they are left out
# and worked out again.
load_installed_config() {
    local status=0

    if [[ ! -f "$INSTALLED_CONFIG_FILE" ]]; then
        echo "WARNING: ${INSTALLED_CONFIG_FILE} not found, all questions will be asked."
        return 0
    fi

    grep -Ev "^(declare [-a-zA-Z]+ )?(TASK_DEPS|FILES|PACKAGES|SERVICES|TEMPLATE_SRCS|TEMPLATE_MODES|USER_GROUPS)=" \
        "$INSTALLED_CONFIG_FILE" > "$CONFIG_FILE" || status=$?

    if (( status > 1 )); then
        echo "ERROR: Unable to read ${INSTALLED_CONFIG_FILE}." >&2
        exit 1
    fi
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

# Lets the user merge the new version of a file into the one in a home directory, with meld when
# upgrade.sh was started from a Wayland session, and with sdiff otherwise. The file is left as it
# was when the merge is not finished.
# Usage: merge_file <new file> <file in home>
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

# Updates one file in a home directory. A missing file is added, a file that differs from the new
# version is kept, replaced or merged, as the user chooses.
# Usage: home_file <new file> <user> <file in home>
home_file() {
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
