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

    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        # The wipe leaves the md superblock of a previous install inside partition 2, and the
        # new partition 2 starts at the same offset, so udev may reassemble the old array after
        # repartitioning. A rerun of this task also finds the array it created before. Stop the
        # arrays and clear the superblocks so the partitions are free for a fresh array.
        mdadm --stop --scan || true
        for DISK in ${Q_DISKS}; do
            mdadm --zero-superblock ${DISK}-part2 || true
        done

        mdadm --create /dev/md/swap --run --level=1 --raid-devices=2 --metadata=1.2 \
            --homehost=${Q_HOSTNAME} --name=swap ${Q_DISK_A}-part2 ${Q_DISK_B}-part2
        swap_part=/dev/md/swap
    fi

    # Encrypted swap gets a fresh random key and mkswap on every boot (see /etc/crypttab).
    # Never swap to the device unencrypted, not even during the install.
    if [[ "${Q_ENCRYPT}" == "true" ]]; then
        return 0
    fi

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
        -O acltype=posixacl \
        -O xattr=sa \
        -O compression=lz4 \
        -O normalization=formD \
        -O relatime=on \
        -O canmount=off \
        -O mountpoint=/boot \
        -O atime=off \
        -R /mnt \
        bpool ${boot_pool_part}
}

f_root_pool() {
    local options=(
        -f
        -o ashift=12
        -o autotrim=on
        -O acltype=posixacl
        -O xattr=sa
        -O dnodesize=auto
        -O compression=lz4
        -O normalization=formD
        -O relatime=on
        -O canmount=off
        -O mountpoint=/
        -O atime=off
        -R /mnt
    )

    if [[ "${Q_ENCRYPT}" == "true" ]]; then
        # With keylocation=prompt, zpool reads the passphrase from stdin when it is not a terminal.
        printf '%s' "${Q_PASSWORD}" | zpool create "${options[@]}" \
            -O encryption=on \
            -O keylocation=prompt \
            -O keyformat=passphrase \
            rpool ${root_pool_part}
    else
        zpool create "${options[@]}" rpool ${root_pool_part}
    fi
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
