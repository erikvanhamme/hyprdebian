#!/bin/bash
set -eo pipefail
exec 2>&1

# Make sure python subprocesses have all variables.
set -a

# Source functions and constants.
source version.sh
source scripts/config.sh
source scripts/constants.sh
source scripts/defaults.sh
source scripts/dependencies.sh
source scripts/helpers.sh

# The upgrade works on the running system, not on a target mounted at /mnt.
TARGET_DIR=""

in_target() {
    "$@"
}

# Keep the upgrade state apart from an installation, and start fresh on every run.
STATE_DIR="/tmp/hyprdebian-upgrade"
CONFIG_FILE="$STATE_DIR/config"

# Start banner.
echo "=== Erik's nifty hyprdebian/debian upgrader $INSTALLER_VERSION ==="

if [[ $EUID -ne 0 ]]; then
    echo "ERROR: The upgrade must be run as root." >&2
    exit 1
fi

rm -rf "$STATE_DIR"
mkdir -p "$STATE_DIR"
declare -A TASK_DEPS

# Load the answers of the installation. The queues and dependencies in the saved config were
# worked out by the installer version that wrote it, so they are left out and worked out again.
if [[ -f "$INSTALLED_CONFIG_FILE" ]]; then
    status=0
    grep -Ev "^(declare [-a-zA-Z]+ )?(TASK_DEPS|FILES|PACKAGES|SERVICES|TEMPLATE_SRCS|TEMPLATE_MODES|USER_GROUPS)=" \
        "$INSTALLED_CONFIG_FILE" > "$CONFIG_FILE" || status=$?

    if (( status > 1 )); then
        echo "ERROR: Unable to read ${INSTALLED_CONFIG_FILE}." >&2
        exit 1
    fi
else
    echo "WARNING: ${INSTALLED_CONFIG_FILE} not found, all questions will be asked."
fi

load_config

echo "Installed version: ${INSTALLED_VERSION:-unknown}"

# Answer the questions with the saved answers. Only questions without a saved answer, like
# questions added after the installation, are asked.
accept_default() {
    local var_name="$1"

    [[ -n "${!var_name:-}" ]]
}

# Upgrade task.
upgrade() {
    return 0
}

# Load the phases that work out the queues from the answers.
source scripts/questions.sh
source scripts/base.sh
source scripts/optional.sh

# Tasks that only make sense during an installation.
mark_done "q_disk"
mark_done "q_password"
mark_done "q_kernel"
mark_done "o_font"

# Deploys the files in /etc/skel and /usr/local/bin that differ from the ones on the system.
upg_files() {
    local src dst

    for src in "${FILES[@]}"; do
        case "$src" in
            deploy/etc/skel/*|deploy/usr/local/bin/*) ;;
            *) continue ;;
        esac

        dst=$(file_dst "$src" "${TARGET_DIR}")

        if [[ -f "$dst" ]] && cmp -s "$src" "$dst"; then
            continue
        fi

        file_deploy "$src" || return 1
    done
}

upg_save_config() {
    save_config INSTALLED_VERSION "${INSTALLER_VERSION}"
    install -D -m 0600 "$CONFIG_FILE" "${INSTALLED_CONFIG_FILE}"
}

add_dependencies "upgrade" \
    "q_main" \
    "b_files" \
    "o_main" \
    "upg_files" \
    "upg_save_config" \

# Start upgrade.
execute_task "upgrade"
