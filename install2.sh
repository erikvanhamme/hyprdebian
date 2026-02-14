#!/usr/bin/env bash
set -euo pipefail

echo "=== Erik's nifty debian+hyprland installer v0.7 ==="

TARGET=/mnt
STATE_DIR="/tmp/my-installer"
mkdir -p "$STATE_DIR"
CONFIG_FILE="$STATE_DIR/config"

if [[ -f "$CONFIG_FILE" ]]; then
    source "$CONFIG_FILE"
fi

# ---------------------------
# Step registry (ORDERED)
# ---------------------------

STEPS=(
    question_disk
    question_swap
    question_username
    prerequisites_backup_sources
    prerequisites_remove_sources
    prerequisites_install_sources
    prerequisites_install_packages
    partition_unmount_swap
    partition_wipe
    partition_discard
    partition_prepare
    partition_efi
    partition_swap
    partition_boot
    partition_root
    partition_probe
    partition_review
    filesystem_efi
    filesystem_swap
    filesystem_boot_pool
    filesystem_root_pool
    filesystem_datasets
    bootstrap
)

# Steps disabled by default (optional)
DISABLED_STEPS=(
    question_username
)

# ---------------------------
# Color constants
# ---------------------------

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

# ---------------------------
# Utility helpers
# ---------------------------

log_status() {
    local color="$1"
    local label="$2"
    local step="$3"

    printf "%b[%s]%b %s\n" "$color" "$label" "$COLOR_RESET" "$step"
}

is_disabled() {
    local step="$1"
    for s in "${DISABLED_STEPS[@]}"; do
        [[ "$s" == "$step" ]] && return 0
    done
    return 1
}

is_done() {
    [[ -f "$STATE_DIR/$1.done" ]]
}

mark_done() {
    touch "$STATE_DIR/$1.done"
}

run_step() {
    local step="$1"

    if is_disabled "$step"; then
        log_status "$COLOR_BLUE" "DISABLED" "$step"
        return 0
    fi

    if is_done "$step"; then
        log_status "$COLOR_YELLOW" "SKIPPED " "$step"
        return 0
    fi

    log_status "" "RUN     " "$step"

    if "$step"; then
        mark_done "$step"
        log_status "$COLOR_GREEN" "OK      " "$step"
    else
        log_status "$COLOR_RED" "FAIL    " "$step"
        return 1
    fi
}

in_target() {
    chroot "$TARGET" /usr/bin/env -i \
        HOME=/root \
        TERM="$TERM" \
        PATH=/usr/sbin:/usr/bin:/sbin:/bin \
        "$@"
}

write_file() {
    local path="$1"
    local mode="$2"

    install -d "$(dirname "$path")"
    install -m "$mode" /dev/null "$path"
    cat > "$path"
}

ask() {
    local var_name="$1"
    local prompt="$2"
    local default="${3:-}"

    local current="${!var_name:-$default}"

    if [[ -n "${current:-}" ]]; then
        read -rp "$prompt [$current]: " input
        input="${input:-$current}"
    else
        read -rp "$prompt: " input
    fi

    printf -v "$var_name" "%s" "$input"

    save_config "$var_name" "$input"
}

save_config() {
    local key="$1"
    local value="$2"

    # Remove existing entry
    grep -v "^${key}=" "$CONFIG_FILE" 2>/dev/null > "$CONFIG_FILE.tmp" || true

    printf '%s="%s"\n' "$key" "$value" >> "$CONFIG_FILE.tmp"

    mv "$CONFIG_FILE.tmp" "$CONFIG_FILE"
}

# ---------------------------
# Installation steps
# ---------------------------

# questions

question_disk() {
    echo "Available block devices:"
    find /dev/disk/by-id
    ask DISK "Target disk (e.g. /dev/sda)"
    read -rp "All data on $DISK will be destroyed. Continue? [y/N]: " confirm
    [[ "$confirm" =~ ^[Yy]$ ]] || { echo "Aborting."; return 1; }
}

question_swap() {
    ask SWAP "Enter size of swap partition (in GiB). Must be > 0!"
}

question_username() {
    ask USERNAME "Username"
}

# prerequisites

prerequisites_backup_sources() {
    mkdir -p /etc/apt/backup
    cp -a /etc/apt/sources.list /etc/apt/backup/sources.list.$(date +%s) 2>/dev/null || true
    cp -a /etc/apt/sources.list.d /etc/apt/backup/sources.list.d.$(date +%s) 2>/dev/null || true
}

prerequisites_remove_sources() {
    rm -f /etc/apt/sources.list
    rm -f /etc/apt/sources.list.d/*.list
}

prerequisites_install_sources() {
    cat > /etc/apt/sources.list <<'EOF'
deb http://deb.debian.org/debian trixie main contrib non-free non-free-firmware
EOF
    apt update
}

prerequisites_install_packages() {
    apt install -y gdisk dosfstools linux-headers-amd64 zfsutils-linux debootstrap
}

# partition

partition_unmount_swap() {
    swapoff -a
}

partition_wipe() {
    wipefs -af ${DISK}
    sgdisk --zap-all ${DISK}
}

partition_discard() {
    blkdiscard -f ${DISK}
}

partition_prepare() {
    sgdisk --clear ${DISK}
}

partition_efi() {
    sgdisk -n 1:0:+1G -t 1:EF00 -c 1:"EFI System" ${DISK}
}

partition_swap() {
    sgdisk -n 2:0:+${SWAP}G -t 2:8200 -c 2:"Swap" ${DISK}
}

partition_boot() {
    sgdisk -n 3:0:+4G -t 3:BF01 -c 3:"Boot Pool" ${DISK}
}

partition_root() {
    sgdisk -n 4:0:0 -t 4:BF01 -c 4:"Root Pool" ${DISK}
}

partition_probe() {
    partprobe ${DISK}
}

partition_review() {
    udevadm settle
    lsblk ${DISK}
}

# filesystem

filesystem_efi() {
    local efi_part=${DISK}-part1
    mkfs.fat -F32 ${efi_part}
}

filesystem_swap() {
    local swap_part=${DISK}-part2
    mkswap ${swap_part}
    swapon ${swap_part}
}

filesystem_boot_pool() {
    local boot_pool_part=${DISK}-part3
    zpool create -f \
        -o ashift=12 \
        -o autotrim=on \
        -o compatibility=grub2 \
        -o cachefile=/etc/zfs/zpool.cache \
        -O devices=off \
        -O acltype=posixacl -O xattr=sa \
        -O compression=lz4 \
        -O normalization=formD \
        -O relatime=on \
        -O canmount=off -O mountpoint=/boot -R /mnt \
        bpool ${boot_pool_part}
}

filesystem_root_pool() {
    local root_pool_part=${DISK}-part4
    zpool create -f \
        -o ashift=12 \
        -o autotrim=on \
        -O encryption=on -O keylocation=prompt -O keyformat=passphrase \
        -O acltype=posixacl -O xattr=sa -O dnodesize=auto \
        -O compression=lz4 \
        -O normalization=formD \
        -O relatime=on \
        -O canmount=off -O mountpoint=/ -R /mnt \
        rpool ${root_pool_part}
}

filesystem_datasets() {
    zfs create -o canmount=noauto -o mountpoint=/ rpool/hyprdebian
    zfs mount rpool/hyprdebian

    zfs create -o mountpoint=/boot bpool/hyprdebian

    zfs create rpool/home
}

# bootstrap

bootstrap() {
    mkdir /mnt/run
    mount -t tmpfs tmpfs /mnt/run
    mkdir /mnt/run/lock
    mkdir -p /mnt/var/lib

    debootstrap testing /mnt
}

# ---------------------------
# CLI parsing
# ---------------------------

FROM=""
ONLY=""
SKIP_LIST=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --from)
            FROM="$2"
            shift 2
            ;;
        --only)
            ONLY="$2"
            shift 2
            ;;
        --skip)
            SKIP_LIST+=("$2")
            shift 2
            ;;
        --list)
            printf "%s\n" "${STEPS[@]}"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            exit 1
            ;;
    esac
done

# Apply runtime skip overrides
DISABLED_STEPS+=("${SKIP_LIST[@]}")

# ---------------------------
# Execution engine
# ---------------------------

start_running=false

for step in "${STEPS[@]}"; do

    # --only overrides everything
    if [[ -n "$ONLY" ]]; then
        if [[ "$step" == "$ONLY" ]]; then
            run_step "$step"
        fi
        continue
    fi

    # --from logic
    if [[ -n "$FROM" ]]; then
        if [[ "$step" == "$FROM" ]]; then
            start_running=true
        fi

        if [[ "$start_running" == false ]]; then
            continue
        fi
    fi

    run_step "$step"
done
