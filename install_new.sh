#!/bin/bash
set -eo pipefail
exec 2>&1

# Source functions and constants.
source scripts/config.sh
source scripts/constants.sh
source scripts/dependencies.sh
source scripts/helpers.sh

# Start banner.
echo "=== Erik's nifty debian+hyprland installer v0.69 ==="

# Set up idempotency and config paths.
mkdir -p "$STATE_DIR"
declare -A TASK_DEPS

# Load the configuration.
load_config

# Installation task.
install() {
    return 0
}

# Load tasks/deps for all the phases, in order.
source scripts/questions.sh
source scripts/prereqs.sh
source scripts/disk.sh
source scripts/partition.sh
source scripts/filesystem.sh
source scripts/base.sh
source scripts/optional.sh
source scripts/packages.sh
source scripts/services.sh
source scripts/user.sh
source scripts/cleanup.sh

# Start Installation.
execute_task "install"
