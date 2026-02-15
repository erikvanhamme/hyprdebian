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
    question_hostname
    question_fqdn
    question_iface
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
    configure_fstab
    configure_hostname
    configure_hosts
    configure_netplan
    configure_zfs_cache
    deploy_files
    tgt_mount
    tgt_add_sources
    tgt_upgrade
    tgt_console
    tgt_zfs_support
    tgt_grub2
    tgt_netplan
    tgt_syscargo
    tgt_filetools
    tgt_greetd
    tgt_hyprland
    user_filesystem
    user_add
    user_skel
    user_chown
    user_groups
    user_greetd
    user_log
    user_dark
    tgt_audio
    tgt_browser
    tgt_misc
    tgt_syscargo_permissions
    tgt_wiremix
    tgt_snapshots
    umount_all
)

# Steps disabled by default (optional)
DISABLED_STEPS=(
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
    ask DISK "Target disk (e.g. /dev/sda): "
    read -rp "All data on $DISK will be destroyed. Continue? [y/N]: " confirm
    [[ "$confirm" =~ ^[Yy]$ ]] || { echo "Aborting."; return 1; }
}

question_swap() {
    ask SWAP "Enter size of swap partition (in GiB, must be > 0): "
}

question_username() {
    ask USERNAME "Username: "
}

question_hostname() {
    ask HOSTNAME "Hostname: "
}

question_fqdn() {
    ask FQDN "Fully qualified domain name: "
}

question_iface() {
    echo "Available wired interfaces:"
    ip -o link show | awk -F': ' '{print $2}' | grep -E '^(en|eth)'
    ask IFACE "Select wired interface for DHCP (e.g. enp0s3): "
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

# configure

configure_fstab() {
    local efi_part swap_part
    local efi_uuid swap_uuid

    efi_part=${DISK}-part1
    swap_part=${DISK}-part2
    efi_uuid=$(blkid -s UUID -o value ${efi_part})
    swap_uuid=$(blkid -s UUID -o value ${swap_part})

    if [[ -z "$efi_uuid" || -z "$swap_uuid" ]]; then
        echo "ERROR: Unable to determine UUIDs for fstab."
        return 1
    fi

    write_file /mnt/etc/fstab 0644 <<EOF
# /etc/fstab: static file system information
#
# <file system>  <mount point>  <type>  <options>         <dump> <pass>

UUID=${efi_uuid}   /boot/efi   vfat   umask=0077        0      1
UUID=${swap_uuid}  none        swap   sw                0      0
EOF
}

configure_hostname() {
    write_file /mnt/etc/hostname 0644 <<EOF
${HOSTNAME}
EOF
}

configure_hosts() {
    write_file /mnt/etc/hosts 0644 <<EOF
127.0.0.1   localhost
127.0.1.1   ${FQDN} ${HOSTNAME}

# IPv6
::1         localhost ip6-localhost ip6-loopback
ff02::1     ip6-allnodes
ff02::2     ip6-allrouters
EOF
}

configure_netplan() {
    mkdir /mnt/etc/netplan
    write_file /mnt/etc/netplan/01-netcfg.yaml 0600 <<EOF
network:
  version: 2
  renderer: networkd
  ethernets:
    ${IFACE}:
      dhcp4: true
      dhcp6: true
EOF
}

configure_zfs_cache() {
    mkdir /mnt/etc/zfs
    cp /etc/zfs/zpool.cache /mnt/etc/zfs
}

# deploy

deploy_files() {
    mkdir -p /mnt/etc/default
    mkdir -p /mnt/etc/apt

    cp -v deploy/etc/apt/sources.list /mnt/etc/apt/
    cp -rv deploy/etc/skel/. /mnt/etc/skel
}

# tgt

tgt_mount() { # TODO: Split?
    mount --make-private --rbind /dev  /mnt/dev
    mount --make-private --rbind /proc /mnt/proc
    mount --make-private --rbind /sys  /mnt/sys

    in_target rm /dev/log
    in_target touch /dev/log
    mount --bind /run/systemd/journal/dev-log /mnt/dev/log
}

tgt_add_sources() { # TODO: Split?
    in_target apt update
    in_target apt install -y curl gpg
    in_target sh -c 'curl -sS https://debian.griffo.io/EA0F721D231FDD3A0A17B9AC7808B4DD62C41256.asc | gpg --dearmor --yes -o /etc/apt/trusted.gpg.d/debian.griffo.io.gpg'
    in_target sh -c 'echo "deb https://debian.griffo.io/apt sid main" | tee /etc/apt/sources.list.d/debian.griffo.io.list'
    in_target apt update
}

tgt_upgrade() {
    apt upgrade -y
}

tgt_console() { # TODO: Split?
    in_target apt install -y console-setup locales command-not-found bash-completion man-db psmisc yazi eza
    in_target dpkg-reconfigure tzdata keyboard-configuration console-setup locales
    in_target apt-file update
}

tgt_zfs_support() {
    in_target apt install -y dpkg-dev linux-headers-generic linux-image-generic zfs-initramfs firmware-linux
}

tgt_grub2() { # TODO: Split?
    in_target mkdir /boot/efi
    in_target mount /boot/efi
    in_target apt install -y grub-efi-amd64 shim-signed
    in_target apt purge -y os-prober
    in_target update-initramfs -c -k all
    cp -v deploy/etc/default/grub /mnt/etc/default
    in_target update-grub
    in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=hyprdebian --recheck --no-floppy
}

tgt_netplan() { # TODO: Split?
    in_target apt purge -y ifupdown
    in_target apt install -y netplan.io
    in_target netplan generate
}

tgt_syscargo() { # TODO: Split?
    in_target apt install -y rustup
    zfs create rpool/home/cargo
    in_target useradd -m -r -s /bin/bash cargo
    in_target chown -R cargo:cargo /home/cargo
    in_target sudo -u cargo mkdir -p /home/cargo/.cargo
    in_target sudo -u cargo tee /home/cargo/.cargo/config.toml > /dev/null <<'EOF'
[install]
root = "/usr/local"
EOF
    in_target sudo -u cargo rustup default stable

    write_file /mnt/usr/local/bin/syscargo 0755 <<'EOF'
#!/bin/bash
exec sudo -u cargo -H cargo "$@"
EOF
}

tgt_filetools() {
    in_target apt install -y yazi eza
}

tgt_greetd() { # TODO: Split?
    in_target apt install -y greetd dbus-user-session
    in_target systemctl enable greetd
}

tgt_hyprland() {
    in_target apt install -y kitty desktop-base hyprland hyprland-qtutils fonts-jetbrains-mono wofi swaybg libglib2.0-bin # TODO: Add and fix hyprlock.
}

# user

user_filesystem() {
    zfs create rpool/home/${USERNAME}
}

user_add() {
    in_target STDOUTMSGLEVEL=fatal STDERRMSGLEVEL=fatal adduser ${USERNAME}
}

user_skel() {
    in_target cp -a /etc/skel/. /home/${USERNAME}/
}

user_chown() {
    in_target chown -R ${USERNAME}:${USERNAME} /home/${USERNAME}
}

user_groups() {
    in_target usermod -a -G audio,cdrom,dip,floppy,netdev,plugdev,sudo,video ${USERNAME}
}

user_greetd() {
    write_file /mnt/etc/greetd/config.toml 0644 <<EOF
[terminal]
vt = 7

[default_session]
command = "dbus-run-session start-hyprland > /var/log/hyprland.log 2>&1"
user = "${USERNAME}"
EOF
}

user_log() {
    touch /mnt/var/log/hyprland.log
    in_target chown ${USERNAME}:${USERNAME} /var/log/hyprland.log
}

user_dark() { # TODO: Find out why this does not work.
    in_target sudo -u ${USERNAME} gsettings set org.gnome.desktop.interface color-scheme 'prefer-dark'
}

# tgt

tgt_audio() {
    in_target apt install -y pipewire wireplumber pulseaudio-utils audacious audacity vlc
    mkdir -p /mnt/home/${USERNAME}/.config/systemd/user/default.target.wants
    ln -s /mnt/usr/lib/systemd/user/pipewire.service /mnt/home/${USERNAME}/.config/systemd/user/default.target.wants/
    ln -s /mnt/usr/lib/systemd/user/wireplumber.service /mnt/home/${USERNAME}/.config/systemd/user/default.target.wants/
    in_target apt install -y pkg-config libpipewire-0.3-dev libclang-dev
}

tgt_browser() {
    in_target apt install -y firefox
}

tgt_misc() {
    in_target apt install -y nfs-common
}

tgt_syscargo_permissions() {
    mkdir -p /mnt/usr/local/bin
    in_target chown -R root:cargo /usr/local
    in_target chmod -R g+w /usr/local
}

tgt_wiremix() {
    in_target sudo -u cargo cargo install wiremix
}

tgt_snapshots() {
    in_target zfs snapshot bpool/hyprdebian@install
    in_target zfs snapshot rpool/hyprdebian@install
}

# cleanup

umount_all() {
    umount /mnt/dev/log
    mount | grep -v zfs | tac | awk '/\/mnt/ {print $3}' | xargs -i{} umount -lf {}
    zpool export -a
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
