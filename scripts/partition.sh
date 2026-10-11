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
        sgdisk --clear $DISK || return 1
    done
}

p_efi() {
    for DISK in ${Q_DISKS}; do
        sgdisk -n 1:0:+1G -t 1:EF00 -c 1:"EFI System" $DISK || return 1
    done
}

p_swap() {
    if [[ "${Q_SWAP:-1}" -eq 0 ]]; then
        return 0
    fi

    # On redundant drives, the swap partitions become a raid1 (mdadm) array.
    local swap_type=8200
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        swap_type=FD00
    fi

    for DISK in ${Q_DISKS}; do
        sgdisk -n 2:0:+${Q_SWAP}G -t 2:${swap_type} -c 2:"Swap" $DISK || return 1
    done
}

p_boot() {
    # Start empty, the value saved by an earlier run must not be appended to.
    boot_pool_part=""
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        boot_pool_part="mirror "
    fi

    for DISK in ${Q_DISKS}; do
        if [[ "${Q_SWAP:-1}" -eq 0 ]]; then
            sgdisk -n 2:0:+4G -t 2:BF01 -c 2:"Boot Pool" $DISK || return 1
            boot_pool_part+="$DISK-part2 "
        else
            sgdisk -n 3:0:+4G -t 3:BF01 -c 3:"Boot Pool" $DISK || return 1
            boot_pool_part+="$DISK-part3 "
        fi
    done

    boot_pool_part=$(trim "${boot_pool_part}")

    # Saved, so f_boot_pool still has it when the installer is started again after this task.
    save_config boot_pool_part "${boot_pool_part}"
}

p_root() {
    # Start empty, the value saved by an earlier run must not be appended to.
    root_pool_part=""
    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        root_pool_part="mirror "
    fi

    for DISK in ${Q_DISKS}; do
        if [[ "${Q_SWAP:-1}" -eq 0 ]]; then
            sgdisk -n 3:0:0 -t 3:BF01 -c 3:"Root Pool" $DISK || return 1
            root_pool_part+="$DISK-part3 "
        else
            sgdisk -n 4:0:0 -t 4:BF01 -c 4:"Root Pool" $DISK || return 1
            root_pool_part+="$DISK-part4 "
        fi
    done

    root_pool_part=$(trim "${root_pool_part}")

    # Saved, so f_root_pool still has it when the installer is started again after this task.
    save_config root_pool_part "${root_pool_part}"
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
