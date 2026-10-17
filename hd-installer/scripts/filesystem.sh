#!/bin/bash
# The filesystem phase: makes the EFI system partitions, the swap, the ZFS pool and its datasets,
# and mounts the root of the new system on TARGET_DIR. There is no boot pool: /boot is a folder
# of the root dataset, as the bootloader reads the UKIs from the EFI system partitions.

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
    local disks disk

    esp_disks disks
    for disk in "${disks[@]}"; do
        mkfs.fat -F32 "$(part "${disk}" "${PART_ESP}")" || return 1
    done
}

f_swap() {
    local disks disk

    if (( Q_SWAP == 0 )); then
        return 0
    fi

    root_disks disks
    local swap_part
    swap_part="$(part "${Q_ROOT_DISK_A}" "${PART_SWAP}")"

    if [[ "${Q_ROOT_REDUNDANT}" == "true" ]]; then
        # The wipe leaves the md superblock of a previous install inside the swap partition, and
        # the new one starts at the same offset, so udev may reassemble the old array after
        # repartitioning. A rerun of this task also finds the array it created before. Stop the
        # arrays and clear the superblocks so the partitions are free for a fresh array.
        mdadm --stop --scan || true
        for disk in "${disks[@]}"; do
            mdadm --zero-superblock "$(part "${disk}" "${PART_SWAP}")" || true
        done

        mdadm --create /dev/md/swap --run --level=1 --raid-devices=2 --metadata=1.2 \
            --homehost="${Q_HOSTNAME}" --name=swap \
            "$(part "${Q_ROOT_DISK_A}" "${PART_SWAP}")" \
            "$(part "${Q_ROOT_DISK_B}" "${PART_SWAP}")" || return 1
        swap_part=/dev/md/swap
    fi

    # Encrypted swap gets a fresh random key and mkswap on every boot (see /etc/crypttab).
    # Never swap to the device unencrypted, not even during the install.
    if [[ "${Q_ENCRYPT}" == "true" ]]; then
        return 0
    fi

    mkswap "${swap_part}" || return 1
    swapon "${swap_part}"
}

# Makes the target a private mount, so the mounts below it, the pool and those of the base phase,
# do not show up in the mount namespaces of the services of the live system. A copy there would
# keep the pool busy, and the cleanup phase could not export it.
f_private_target() {
    mkdir -p "${TARGET_DIR}" || return 1
    if ! mountpoint -q "${TARGET_DIR}"; then
        mount --bind "${TARGET_DIR}" "${TARGET_DIR}" || return 1
    fi
    mount --make-private "${TARGET_DIR}"
}

f_root_pool() {
    local disks disk
    local vdev=()

    root_disks disks
    if [[ "${Q_ROOT_REDUNDANT}" == "true" ]]; then
        vdev+=(mirror)
    fi
    for disk in "${disks[@]}"; do
        vdev+=("$(part "${disk}" "${PART_ROOT}")")
    done

    # With -R, zpool writes no cache file, which b_zfs_cache copies into the new system, unless
    # it is asked for.
    local options=(
        -f
        -o ashift=12
        -o autotrim=on
        -o cachefile=/etc/zfs/zpool.cache
        -O acltype=posixacl
        -O xattr=sa
        -O dnodesize=auto
        -O compression=lz4
        -O normalization=formD
        -O relatime=on
        -O canmount=off
        -O mountpoint=/
        -O atime=off
        -R "${TARGET_DIR}"
    )

    if [[ "${Q_ENCRYPT}" == "true" ]]; then
        # With keylocation=prompt, zpool reads the passphrase from stdin when it is not a terminal.
        printf '%s' "${Q_PASSWORD}" | zpool create "${options[@]}" \
            -O encryption=on \
            -O keylocation=prompt \
            -O keyformat=passphrase \
            rpool "${vdev[@]}"
    else
        zpool create "${options[@]}" rpool "${vdev[@]}"
    fi
}

f_datasets() {
    zfs create -o canmount=noauto -o mountpoint=/ rpool/hyprdebian || return 1
    zfs mount rpool/hyprdebian || return 1

    zfs create rpool/home
}

add_dependencies "f_main" \
    "f_efi" \
    "f_swap" \
    "f_private_target" \
    "f_root_pool" \
    "f_datasets" \

add_dependencies "install_root" "f_pre" "f_main" "f_post"
