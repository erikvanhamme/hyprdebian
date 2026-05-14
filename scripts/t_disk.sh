#!/bin/bash

t_disk() {
    return 0
}

s_unmount() {
    swapoff -a
}

d_wipe() {
    wipefs -af ${Q_DISK}
    sgdisk --zap-all ${Q_DISK}
}

d_discard() {
    blkdiscard -f ${Q_DISK}
}

p_prepare() {
    sgdisk --clear ${Q_DISK}
}

p_efi() {
    sgdisk -n 1:0:+1G -t 1:EF00 -c 1:"EFI System" ${Q_DISK}
}

p_swap() {
    sgdisk -n 2:0:+${Q_SWAP}G -t 2:8200 -c 2:"Swap" ${Q_DISK}
}

p_boot() {
    sgdisk -n 3:0:+4G -t 3:BF01 -c 3:"Boot Pool" ${Q_DISK}
}

p_root() {
    sgdisk -n 4:0:0 -t 4:BF01 -c 4:"Root Pool" ${Q_DISK}
}

p_probe() {
    partprobe ${Q_DISK}
}

p_review() {
    udevadm settle
    lsblk ${Q_DISK}
}

f_efi() {
    local efi_part=${Q_DISK}-part1
    mkfs.fat -F32 ${efi_part}
}

f_swap() {
    local swap_part=${Q_DISK}-part2
    mkswap ${swap_part}
    swapon ${swap_part}
}

f_boot_pool() {
    local boot_pool_part=${Q_DISK}-part3
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

f_root_pool() {
    local root_pool_part=${Q_DISK}-part4
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

f_datasets() {
    zfs create -o canmount=noauto -o mountpoint=/ rpool/hyprdebian
    zfs mount rpool/hyprdebian

    zfs create -o mountpoint=/boot bpool/hyprdebian

    zfs create rpool/home
}


add_dependencies "t_disk" "s_unmount" "d_wipe" "d_discard" "p_prepare" "p_efi" "p_swap" "p_boot" "p_root" "p_probe" "p_review" "f_efi" "f_swap" "f_boot_pool" "f_root_pool" "f_datasets"
add_dependencies "install" "t_disk"
