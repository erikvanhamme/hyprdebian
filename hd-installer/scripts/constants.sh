#!/bin/bash
# Constants of the installer.

# Where the new system is mounted while it is installed.
TARGET_DIR="/mnt"

# Signing key of the HyprDebian repository, also in hyprdebian.local.sources.j2.
REPO_KEY="/usr/share/keyrings/hyprdebian-archive-keyring.pgp"

# The partitions of the disks, the same on every disk: the EFI system partition on the ESP
# disks, the swap and the ZFS pool on the system disks. A disk that is only one of the two has
# only its partitions, so the numbers have gaps.
PART_ESP=1
PART_SWAP=2
PART_ROOT=3

# Where the installer keeps its state: the answers and the tasks that are done. A rerun picks up
# where the last run stopped.
STATE_DIR="/tmp/hd-installer"
CONFIG_FILE="${STATE_DIR}/config"

# The links that name the disks.
DISK_BY_ID_DIR="/dev/disk/by-id"

# The network interfaces.
NET_CLASS_DIR="/sys/class/net"

# Colors, only on a terminal.
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
