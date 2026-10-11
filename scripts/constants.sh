#!/bin/bash

# General constants.
TARGET_DIR=/mnt
STATE_DIR="/tmp/hyprdebian"
CONFIG_FILE="$STATE_DIR/config"
INSTALLED_CONFIG_FILE="/etc/hyprdebian/installer.config" # Copy of the config on the installed system.
MIGRATIONS_FILE="/etc/hyprdebian/migrations" # The migrations applied on the installed system.

# Color constants.
if [[ -t 1 ]]; then
    COLOR_RESET="\033[0m"
    COLOR_GREEN="\033[32m"
    COLOR_YELLOW="\033[33m"
    COLOR_BLUE="\033[34m"
    COLOR_RED="\033[31m"
else
    COLOR_RESET=""
    COLOR_GREEN=""
    COLOR_YELLOW=""
    COLOR_BLUE=""
    COLOR_RED=""
fi
