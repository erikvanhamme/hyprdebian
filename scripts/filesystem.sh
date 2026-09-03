#!/bin/bash

f_pre() {
    return 0
}

f_main() {
    return 0
}

f_post() {
    return 0
}

f_efi() {
    for DISK in ${Q_DISKS}; do
        local efi_part=$DISK-part1
        mkfs.fat -F32 ${efi_part}
    done
}

f_swap() {
    if [[ "${Q_SWAP:-1}" -eq 0 ]]; then
        return 0
    fi

    local swap_part=${Q_DISK}-part2
    mkswap ${swap_part}
    swapon ${swap_part}
}

f_boot_pool() {
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

add_dependencies "f_main" \
    "f_efi" \
    "f_swap" \
    "f_boot_pool" \
    "f_root_pool" \
    "f_datasets" \

add_dependencies "install" "f_pre" "f_main" "f_post"
