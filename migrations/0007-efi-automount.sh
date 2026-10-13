#!/bin/bash

# Older versions mounted the EFI partitions from /etc/fstab. b_mount_unit now renders a mount unit
# for /boot/efi, and /boot/efi2 on redundant drives, with an automount unit that mounts them after
# zfs-mount.service, so after /boot is mounted. b_mount_unit does not run during an upgrade, so
# the units are added here, and the fstab entries are removed.

# Succeeds when fstab has an entry for the mount point.
# Usage: in_fstab <mount point>
in_fstab() {
    awk -v where="$1" '$1 !~ /^#/ && $2 == where { found = 1 } END { exit !found }' "${TARGET_DIR}/etc/fstab"
}

# Prints the UUID of the EFI partition on a mount point: from its fstab entry, or from the mounted
# partition when the entry is already gone, like on a rerun of this migration.
# Usage: efi_uuid <mount point>
efi_uuid() {
    local where="$1"
    local uuid

    uuid=$(awk -v where="$where" '$1 !~ /^#/ && $2 == where && $1 ~ /^UUID=/ { print substr($1, 6); exit }' \
        "${TARGET_DIR}/etc/fstab")

    if [[ -z "$uuid" ]]; then
        uuid=$(findmnt -n -o UUID --mountpoint "${TARGET_DIR}${where}")
    fi

    echo "$uuid"
}

migrate() {
    local fstab="${TARGET_DIR}/etc/fstab"
    local dir="${TARGET_DIR}/etc/systemd/system"
    local units=(boot-efi)
    local unit where
    local needed=false

    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        units+=(boot-efi2)
    fi

    # Not needed when the EFI partitions are not in fstab, and the automount units are enabled.
    for unit in "${units[@]}"; do
        where="/${unit//-//}"

        if in_fstab "$where" || [[ ! -e "$dir/${unit}.mount" ]] || \
            ! in_target systemctl is-enabled --quiet "${unit}.automount"; then
            needed=true
        fi
    done

    if [[ "$needed" != "true" ]]; then
        return 0
    fi

    # The templates use efi_uuid and efi2_uuid, like in b_mount_unit.
    local efi_uuid efi2_uuid uuid
    for unit in "${units[@]}"; do
        where="/${unit//-//}"

        if [[ ! -e "$dir/${unit}.mount" ]]; then
            uuid=$(efi_uuid "$where")

            if [[ -z "$uuid" ]]; then
                echo "ERROR: Unable to determine the UUID of the EFI partition on ${where}." >&2
                return 1
            fi

            if [[ "$unit" == "boot-efi" ]]; then
                efi_uuid="$uuid"
            else
                efi2_uuid="$uuid"
            fi

            template_render "templates/etc/systemd/system/${unit}.mount.j2" || return 1
        fi

        if [[ ! -e "$dir/${unit}.automount" ]]; then
            file_deploy "deploy/etc/systemd/system/${unit}.automount" || return 1
        fi
    done

    # The units in /etc/systemd/system take over from the ones the fstab entries generate. The
    # partitions stay mounted, and the automount units take over on the next boot. Starting them now
    # would hide the mounted partitions.
    in_target systemctl daemon-reload || return 1

    for unit in "${units[@]}"; do
        in_target systemctl enable "${unit}.automount" || return 1
    done

    # Only the entries of the units added above are removed. Keeps a copy where nothing reads it,
    # and writes into the existing file to keep its mode.
    local remove=()
    for unit in "${units[@]}"; do
        where="/${unit//-//}"
        if in_fstab "$where"; then
            remove+=("$where")
        fi
    done

    if (( ${#remove[@]} > 0 )); then
        echo "Removing ${remove[*]} from ${fstab}, a copy is in ${fstab}.bak"
        cp -p "$fstab" "${fstab}.bak" || return 1
        awk -v remove="${remove[*]}" '
            BEGIN { split(remove, list, " "); for (i in list) skip[list[i]] = 1 }
            $1 ~ /^#/ || !($2 in skip)
        ' "${fstab}.bak" > "$fstab" || return 1
    fi

    in_target systemctl daemon-reload
}
