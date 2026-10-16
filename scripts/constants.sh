#!/bin/bash

# General constants.
TARGET_DIR=/mnt
STATE_DIR="/tmp/hyprdebian"
CONFIG_FILE="$STATE_DIR/config"
DEB_DIR="$STATE_DIR" # Where the deb phase collects the .deb files it builds.
INSTALLED_CONFIG_FILE="/etc/hyprdebian/installer.config" # Copy of the config on the installed system.
MIGRATIONS_FILE="/etc/hyprdebian/migrations" # The migrations applied on the installed system.
REPO_KEY="/usr/share/keyrings/hyprdebian-archive-keyring.pgp" # Signing key of the hyprdebian repo, also in hyprdebian.local.sources.j2.
SOURCE_URL="https://github.com/erikvanhamme/hyprdebian.git" # The origin of the checkout of the installer on the installed system.
SOURCE_DIR="/usr/src/hyprdebian" # Where the checkout of the installer goes on the installed system.
SECUREBOOT_DIR="/etc/hyprdebian/secureboot" # The Secure Boot keys of the installed system.

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
