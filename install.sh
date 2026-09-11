#!/bin/bash
set -eo pipefail
exec 2>&1

# Make sure python subprocesses have all variables.
set -a

# Source functions and constants.
source scripts/config.sh
source scripts/constants.sh
source scripts/dependencies.sh
source scripts/helpers.sh

# Start banner.
echo "=== Erik's nifty hyprdebian/debian installer v0.92 ==="

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
source scripts/files.sh
source scripts/templates.sh
source scripts/services.sh
source scripts/user.sh
source scripts/cleanup.sh

# Start Installation.
execute_task "install"
