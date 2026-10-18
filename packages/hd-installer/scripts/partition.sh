#!/bin/bash
# The partition phase: partitions the disks. The numbers of the partitions are the same on every
# disk (PART_ESP, PART_SWAP and PART_ROOT), so a disk that is both an ESP disk and a system disk
# gets all three, and a disk that is only one of them gets only its own.

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
    local disks disk

    all_disks disks
    for disk in "${disks[@]}"; do
        sgdisk --clear "${disk}" || return 1
    done
}

p_efi() {
    local disks disk

    esp_disks disks
    for disk in "${disks[@]}"; do
        sgdisk -n "${PART_ESP}:0:+1G" -t "${PART_ESP}:EF00" -c "${PART_ESP}:EFI System" \
            "${disk}" || return 1
    done
}

p_swap() {
    local disks disk

    if (( Q_SWAP == 0 )); then
        return 0
    fi

    # On mirrored disks, the swap partitions become a raid1 (mdadm) array.
    local swap_type=8200
    if [[ "${Q_ROOT_REDUNDANT}" == "true" ]]; then
        swap_type=FD00
    fi

    root_disks disks
    for disk in "${disks[@]}"; do
        sgdisk -n "${PART_SWAP}:0:+${Q_SWAP}G" -t "${PART_SWAP}:${swap_type}" \
            -c "${PART_SWAP}:Swap" "${disk}" || return 1
    done
}

p_root() {
    local disks disk

    root_disks disks
    for disk in "${disks[@]}"; do
        sgdisk -n "${PART_ROOT}:0:0" -t "${PART_ROOT}:BF01" -c "${PART_ROOT}:Root Pool" \
            "${disk}" || return 1
    done
}

p_probe() {
    local disks disk

    all_disks disks
    for disk in "${disks[@]}"; do
        partprobe "${disk}" || return 1
    done
}

p_review() {
    local disks disk

    # The links of the new partitions in /dev/disk/by-id are made by udev.
    udevadm settle || return 1

    all_disks disks
    for disk in "${disks[@]}"; do
        lsblk "${disk}" || return 1
    done
}

add_dependencies "p_main" \
    "p_prepare" \
    "p_efi" \
    "p_swap" \
    "p_root" \
    "p_probe" \
    "p_review" \

add_dependencies "install_root" "p_pre" "p_main" "p_post"
