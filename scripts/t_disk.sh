#!/bin/bash

t_disk() {
    return 0
}

s_unmount() {
    swapoff -a
}

d_wipe() {
    for DISK in ${Q_DISKS}; do
        wipefs -af $DISK
        sgdisk --zap-all $DISK
    done
}

d_discard() {
    for DISK in ${Q_DISKS}; do
        blkdiscard -f $DISK
    done
}

p_prepare() {
    for DISK in ${Q_DISKS}; do
        sgdisk --clear DISK
    done
}

p_efi() {
    for DISK in ${Q_DISKS}; do
        sgdisk -n 1:0:+1G -t 1:EF00 -c 1:"EFI System" $DISK
    done
}

p_swap() {
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        return 0
    fi

    if [[ "${Q_SWAP:-1}" -eq 0 ]]; then
        for DISK in ${Q_DISKS}; do
            sgdisk -n 2:0:+${Q_SWAP}G -t 2:8200 -c 2:"Swap" $DISK
        done
    fi
}

p_boot() {
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        boot_pool_part = "mirror "
    fi

    for DISK in ${Q_DISKS}; do
        if [[ "${Q_SWAP:-1}" -eq 0 ]]; then
            sgdisk -n 2:0:+4G -t 2:BF01 -c 2:"Boot Pool" $DISK
            boot_pool_part+="$DISK-part2 "
        else
            sgdisk -n 3:0:+4G -t 3:BF01 -c 3:"Boot Pool" $DISK
            boot_pool_part+="$DISK-part3 "
        fi
    done

    boot_pool_part=$(trim "${boot_pool_part}")
}

p_root() {
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        root_pool_part = "mirror "
    fi

    for DISK in ${Q_DISKS}; do
        if [[ "${Q_SWAP:-1}" -eq 0 ]]; then
            sgdisk -n 3:0:0 -t 3:BF01 -c 3:"Root Pool" $DISK
            root_pool_part+="$DISK-part3 "
        else
            sgdisk -n 4:0:0 -t 4:BF01 -c 4:"Root Pool" $DISK
            root_pool_part+="$DISK-part4 "
        fi
    done

    root_pool_part=$(trim "${root_pool_part}")
}

p_probe() {
    for DISK in ${Q_DISKS}; do
        partprobe $DISK
    done
}

p_review() {
    udevadm settle
    for DISK in ${Q_DISKS}; do
        lsblk $DISK
    done
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


add_dependencies "t_disk" "s_unmount" "d_wipe" "d_discard" "p_prepare" "p_efi" "p_swap" "p_boot" "p_root" "p_probe" "p_review" "f_efi" "f_swap" "f_boot_pool" "f_root_pool" "f_datasets"
add_dependencies "t_install" "t_disk"
