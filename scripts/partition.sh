#!/bin/bash

p_pre() {
    return 0
}

p_main() {
    return 0
}

p_post() {
    return 0
}

p_prepare() {
    for DISK in ${Q_DISKS}; do
        sgdisk --clear $DISK
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

    if [[ "${Q_SWAP:-1}" -gt 0 ]]; then
        for DISK in ${Q_DISKS}; do
            sgdisk -n 2:0:+${Q_SWAP}G -t 2:8200 -c 2:"Swap" $DISK
        done
    fi
}

p_boot() {
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        boot_pool_part="mirror "
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
        root_pool_part="mirror "
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

add_dependencies "p_main" \
    "p_prepare" \
    "p_efi" \
    "p_swap" \
    "p_boot" \
    "p_root" \
    "p_probe" \
    "p_review" \

add_dependencies "install" "p_pre" "p_main" "p_post"
