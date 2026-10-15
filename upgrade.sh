#!/bin/bash
set -eo pipefail
exec 2>&1

# Make sure python subprocesses have all variables.
set -a

# Source functions and constants. The upgrade helpers replace some of the installer ones.
source version.sh
source scripts/config.sh
source scripts/constants.sh
source scripts/defaults.sh
source scripts/dependencies.sh
source scripts/helpers.sh
source scripts/upgrade_helpers.sh
source scripts/upgrade_detect.sh

# Start banner.
echo "=== Erik's nifty hyprdebian upgrader $INSTALLER_VERSION ==="

if [[ $EUID -ne 0 ]]; then
    echo "ERROR: The upgrade must be run as root." >&2
    exit 1
fi

# Prepare a fresh state and config.
rm -rf "$STATE_DIR"
mkdir -p "$STATE_DIR"
declare -A TASK_DEPS

# Check that the system is up to date, snapshot it, then install what the upgrade itself needs.
check_up_to_date
take_snapshots
install_prerequisites

# Load the answers of the installation.
load_installed_config
load_config

echo "Installed version: ${INSTALLED_VERSION:-unknown}"

# Upgrade task.
upgrade() {
    return 0
}

# Load the phases that work out the queues from the answers.
source scripts/questions.sh
source scripts/base.sh
source scripts/optional.sh

# Load the phases with the tasks that the queuing tasks attach to their hooks. Only these tasks
# run, see upg_hooks.
source scripts/packages.sh
source scripts/services.sh
source scripts/user.sh

# Tasks that only make sense during an installation. The suite of an installed system is kept.
mark_done "q_suite"
mark_done "q_disk"
mark_done "q_password"
mark_done "q_kernel"
mark_done "o_font"

add_dependencies "upgrade" \
    "q_main" \
    "b_files" \
    "b_templates" \
    "b_systemd" \
    "b_utilities" \
    "b_network" \
    "o_main" \

# Load the upgrade phase.
source scripts/upgrade.sh

# Start upgrade.
execute_task "upgrade"

# New services only start on the next boot, and migrations may change how the system boots.
echo "The upgrade is done. Reboot the system to finish it."

# Shown again at the very end, so it is on screen when the upgrade ends.
if [[ "${MOK_REQUESTED:-}" == "true" ]]; then
    mok_notice
fi
