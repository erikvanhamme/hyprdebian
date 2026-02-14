#!/usr/bin/env bash
set -euo pipefail

echo "=== Erik's nifty debian+hyprland installer v0.4 ==="

TARGET=/mnt

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

prerequisites() {
    echo "Configuring APT for Debian stable (main + contrib)..."

    # Backup existing sources
    mkdir -p /etc/apt/backup
    cp -a /etc/apt/sources.list /etc/apt/backup/sources.list.$(date +%s) 2>/dev/null || true
    cp -a /etc/apt/sources.list.d /etc/apt/backup/sources.list.d.$(date +%s) 2>/dev/null || true

    # Remove live-specific and existing lists
    rm -f /etc/apt/sources.list
    rm -f /etc/apt/sources.list.d/*.list

    # Write clean Debian trixie sources
    cat > /etc/apt/sources.list <<'EOF'
deb http://deb.debian.org/debian trixie main contrib non-free non-free-firmware
EOF

    # Update sources
    apt update

    # Install prerequisite packages.
    echo "Installing prerequisite packages."
    apt install -y gdisk dosfstools linux-headers-amd64 zfsutils-linux debootstrap
}

partition_disk() {
    local disk="$1"

    # Ask for swap size in GB
    local swap_size
    while true; do
        read -rp "Enter swap size in GB (must be > 0): " swap_size
        if [[ "$swap_size" =~ ^[1-9][0-9]*$ ]]; then
            break
        else
            echo "Invalid value. Enter a number greater than 0."
        fi
    done

    # Wipe existing filesystem signatures
    echo "Wiping filesystem signatures on $disk..."
    wipefs -af "$disk"

    # Discard all blocks (SSD trim)
    echo "Discarding all blocks on $disk..."
    blkdiscard -f "$disk"

    # Wipe existing partitions
    echo "Wiping existing partitions on $disk..."
    sgdisk --zap-all "$disk"

    # Create GPT
    echo "Creating GPT partition table..."
    sgdisk --clear "$disk"

    # Create partitions
    echo "Creating partitions..."
    # Partition 1: EFI, 1G
    sgdisk -n 1:0:+1G -t 1:EF00 -c 1:"EFI System" "$disk"
    # Partition 2: swap, user-specified
    sgdisk -n 2:0:+${swap_size}G -t 2:8200 -c 2:"Swap" "$disk"
    # Partition 3: boot pool, 4G
    sgdisk -n 3:0:+4G -t 3:BF01 -c 3:"Boot Pool" "$disk"
    # Partition 4: root pool, remaining
    sgdisk -n 4:0:0 -t 4:BF01 -c 4:"Root Pool" "$disk"

    echo "Informing kernel of partition table changes..."
    partprobe "$disk" || true

    echo "Waiting for udev to create device nodes and by-id links..."
    udevadm settle

    echo "Partitioning complete. Here's the new layout:"
    lsblk "$disk"
}

create_filesystems() {
    local disk="$1"

    # Define partition names (adjust if using NVMe or different naming)
    local efi_part="${disk}-part1"
    local swap_part="${disk}-part2"
    local boot_pool_part="${disk}-part3"
    local root_pool_part="${disk}-part4"

    echo "=== Creating filesystems ==="

    # 1. EFI partition (FAT32)
    echo "Formatting EFI partition ($efi_part)..."
    mkfs.fat -F32 "$efi_part"

    # 2. Swap partition
    echo "Setting up swap ($swap_part)..."
    mkswap "$swap_part"
    swapon "$swap_part"

    # 3. Boot pool (ZFS)
    echo "Creating boot ZFS pool (bpool)..."
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
        bpool "$boot_pool_part"

    # 4. Root pool (ZFS)
    echo "Creating root ZFS pool (rpool)..."
    zpool create -f \
        -o ashift=12 \
        -o autotrim=on \
        -O encryption=on -O keylocation=prompt -O keyformat=passphrase \
        -O acltype=posixacl -O xattr=sa -O dnodesize=auto \
        -O compression=lz4 \
        -O normalization=formD \
        -O relatime=on \
        -O canmount=off -O mountpoint=/ -R /mnt \
        rpool "$root_pool_part"

    # 5. Datasets
    zfs create -o canmount=noauto -o mountpoint=/ rpool/hyprdebian
    zfs mount rpool/hyprdebian

    zfs create -o mountpoint=/boot bpool/hyprdebian

    zfs create rpool/home

    echo "Filesystems created successfully!"
}

bootstrap_system() {
    echo "Preparing to bootstrap base system."
    mkdir /mnt/run
    mount -t tmpfs tmpfs /mnt/run
    mkdir /mnt/run/lock
    mkdir -p /mnt/var/lib

    echo "Bootstrap base system."
    debootstrap testing /mnt
}

configure_system() {
    local hostname fqdn iface domain

    # 1. Create /etc/fstab
    echo "Creating /etc/fstab file."

    local disk="$1"

    local efi_part swap_part
    local efi_uuid swap_uuid

    efi_part="${disk}-part1"
    swap_part="${disk}-part2"
    efi_uuid=$(blkid -s UUID -o value "$efi_part")
    swap_uuid=$(blkid -s UUID -o value "$swap_part")

    if [[ -z "$efi_uuid" || -z "$swap_uuid" ]]; then
        echo "ERROR: Unable to determine UUIDs for fstab"
        return 1
    fi

    write_file /mnt/etc/fstab 0644 <<EOF
# /etc/fstab: static file system information
#
# <file system>  <mount point>  <type>  <options>         <dump> <pass>

UUID=${efi_uuid}   /boot/efi   vfat   umask=0077        0      1
UUID=${swap_uuid}  none        swap   sw                0      0
EOF

    # 2. Create /etc/hostname and /etc/hosts
    echo "Creating /etc/hostname and /etc/hosts files."

    read -rp "Hostname (short name, e.g. node1): " hostname
    read -rp "FQDN (e.g. node1.example.com): " fqdn

    domain="${fqdn#*.}"

    write_file /mnt/etc/hostname 0644 <<EOF
${hostname}
EOF

    write_file /mnt/etc/hosts 0644 <<EOF
127.0.0.1   localhost
127.0.1.1   ${fqdn} ${hostname}

# IPv6
::1         localhost ip6-localhost ip6-loopback
ff02::1     ip6-allnodes
ff02::2     ip6-allrouters
EOF

    # 3. Create /etc/netplan/01-netcfg.yaml
    echo "Creating netplan/01-netcfg.yaml file."

    echo
    echo "Available wired interfaces:"
    ip -o link show | awk -F': ' '{print $2}' | grep -E '^(en|eth)'

    read -rp "Select wired interface for DHCP (e.g. enp0s3): " iface

    mkdir /mnt/etc/netplan
    write_file /mnt/etc/netplan/01-netcfg.yaml 0600 <<EOF
network:
  version: 2
  renderer: networkd
  ethernets:
    ${iface}:
      dhcp4: true
      dhcp6: true
EOF

    mkdir -p /mnt/etc/default

    # 4. Deploying config files.
    echo "Deploying config files."

    mkdir /mnt/etc/zfs
    cp /etc/zfs/zpool.cache /mnt/etc/zfs

    mkdir -p /mnt/etc/apt
    cp -v deploy/etc/apt/sources.list /mnt/etc/apt/

    cp -rv deploy/etc/skel/. /mnt/etc/skel
}

in_target_mount() {
    mount --make-private --rbind /dev  /mnt/dev
    mount --make-private --rbind /proc /mnt/proc
    mount --make-private --rbind /sys  /mnt/sys
}

in_target_actions() {
    local username

    echo "Enable in target syslog support."
    in_target rm /dev/log
    in_target touch /dev/log
    mount --bind /run/systemd/journal/dev-log /mnt/dev/log

    echo "Update and install basic console support."
    in_target apt update
    in_target apt install -y console-setup locales command-not-found bash-completion man-db psmisc
    in_target dpkg-reconfigure tzdata keyboard-configuration console-setup locales
    in_target apt-file update

    echo "Install ZFS support."
    in_target apt install -y dpkg-dev linux-headers-generic linux-image-generic zfs-initramfs firmware-linux

    echo "Install grub."
    in_target mkdir /boot/efi
    in_target mount /boot/efi
    in_target apt install -y grub-efi-amd64 shim-signed
    in_target apt purge -y os-prober
    in_target update-initramfs -c -k all
    cp -v deploy/etc/default/grub /mnt/etc/default
    in_target update-grub
    in_target grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=hyprdebian --recheck --no-floppy

    echo "Install netplan."
    in_target apt purge -y ifupdown
    in_target apt install -y netplan.io
    in_target netplan generate

    echo "Install support for managed cargo system packages."
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

    in_target chown -R root:cargo /usr/local
    in_target chmod -R g+w /usr/local

    write_file /mnt/usr/local/bin/syscargo 0755 <<EOF
#!/bin/bash
exec sudo -u cargo -H cargo "$@"
EOF

    echo "Install file tools."
    # in_target apt -y install curl gpg
    # in_target sh -c 'curl -sS https://debian.griffo.io/EA0F721D231FDD3A0A17B9AC7808B4DD62C41256.asc | gpg --dearmor --yes -o /etc/apt/trusted.gpg.d/debian.griffo.io.gpg'
    # in_target sh -c 'echo "deb https://debian.griffo.io/apt $(lsb_release -sc 2>/dev/null) main" | tee /etc/apt/sources.list.d/debian.griffo.io.list'
    # in_target apt update
    # in_target apt -y install yazi eza

    echo "Install greetd and autologin related items."
    in_target apt install -y greetd dbus-user-session
    in_target systemctl enable greetd

    echo "Install hyprland and related packages."
    in_target apt install -y kitty desktop-base hyprland hyprland-qtutils fonts-jetbrains-mono wofi swaybg libglib2.0-bin # TODO: Add and fix hyprlock.

    echo "Creating user."
    read -rp "Enter username: " username
    zfs create rpool/home/$username
    in_target STDOUTMSGLEVEL=fatal STDERRMSGLEVEL=fatal adduser $username
    in_target cp -a /etc/skel/. /home/$username/
    in_target chown -R $username:$username /home/$username
    in_target usermod -a -G audio,cdrom,dip,floppy,netdev,plugdev,sudo,video $username
    write_file /mnt/etc/greetd/config.toml 0644 <<EOF
[terminal]
vt = 7

[default_session]
command = "dbus-run-session start-hyprland > /var/log/hyprland.log 2>&1"
user = "$username"
EOF
    touch /mnt/var/log/hyprland.log
    in_target chown $username:$username /var/log/hyprland.log
    in_target sudo -u $username gsettings set org.gnome.desktop.interface color-scheme 'prefer-dark'

    echo "Install audio subsystem + tools and multimedia."
    in_target apt install -y pipewire wireplumber pulseaudio-utils audacious audacity vlc
    mkdir -p /mnt/home/$username/.config/systemd/user/default.target.wants
    ln -s /mnt/usr/lib/systemd/user/pipewire.service /mnt/home/$username/.config/systemd/user/default.target.wants/
    ln -s /mnt/usr/lib/systemd/user/wireplumber.service /mnt/home/$username/.config/systemd/user/default.target.wants/
    in_target apt install pkg-config libpipewire-0.3-dev libclang-dev
    in_target syscargo install wiremix

    echo "Install browser."
    in_target apt install -y firefox

    echo "Install misc. wanted packages."
    in_target apt install -y nfs-common

    echo "Save snapshots."
    in_target zfs snapshot bpool/hyprdebian@install
    in_target zfs snapshot rpool/hyprdebian@install

    echo "Clean up in target syslog support."
    umount /mnt/dev/log
}

unmount_all() {
    mount | grep -v zfs | tac | awk '/\/mnt/ {print $3}' | xargs -i{} umount -lf {}
    zpool export -a
}

main() {
    # Install prerequisites.
    prerequisites

    # List available disks
    echo "Available block devices:"
    find /dev/disk/by-id

    # Ask user which disk to use
    read -rp "Enter the SSD to partition (e.g., /dev/sda): " disk

    # Confirm destructive action
    read -rp "All data on $disk will be destroyed. Continue? [y/N]: " confirm
    [[ "$confirm" =~ ^[Yy]$ ]] || { echo "Aborting."; exit 1; }

    partition_disk "$disk"
    create_filesystems "$disk"
    bootstrap_system
    configure_system "$disk"
    in_target_mount
    in_target_actions
    unmount_all
}

# Call main
main

