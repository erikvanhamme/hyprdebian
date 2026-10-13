#!/bin/bash

# Older versions used the swap partition unencrypted, also when the root filesystem is encrypted,
# so memory of the encrypted system could end up on the disk in plain text. b_crypttab now
# encrypts the swap with a random key on each boot. Moves the swap to /dev/mapper/swap. What the old
# swap held stays on the partition until the encrypted swap overwrites it.
migrate() {
    local fstab="${TARGET_DIR}/etc/fstab"
    local crypttab="${TARGET_DIR}/etc/crypttab"
    local entries uuid dev swap_device swap_id

    # Not needed without encryption, or when fstab has no swap partition by UUID, like on systems
    # without swap or with the encrypted swap already.
    [[ "${Q_ENCRYPT}" == "true" ]] || return 0

    entries=$(awk '$1 !~ /^#/ && $3 == "swap" && $1 ~ /^UUID=/ { print substr($1, 6) }' "$fstab")

    if [[ -z "$entries" ]]; then
        return 0
    fi

    if (( $(wc -l <<< "$entries") > 1 )); then
        echo "ERROR: ${fstab} has more than one swap partition, encrypt the swap by hand." >&2
        return 1
    fi

    uuid="$entries"
    dev=$(blkid -U "$uuid")

    if [[ -z "$dev" || "$(blkid -s TYPE -o value "$dev")" != "swap" ]]; then
        echo "ERROR: No swap partition found with UUID ${uuid}." >&2
        return 1
    fi

    # The swap device is overwritten on every boot, so refer to it by a name that can never point
    # at another device, like b_crypttab.
    if [[ "$(lsblk -dn -o TYPE "$dev")" == raid* ]]; then
        swap_id=$(mdadm --detail --export "$dev" | sed -n 's/^MD_UUID=//p')
        swap_device=/dev/disk/by-id/md-uuid-${swap_id}
    else
        swap_id=$(blkid -s PARTUUID -o value "$dev")
        swap_device=/dev/disk/by-partuuid/${swap_id}
    fi

    if [[ -z "$swap_id" || "$(readlink -f "$swap_device")" != "$(readlink -f "$dev")" ]]; then
        echo "ERROR: Unable to determine the swap device for crypttab." >&2
        return 1
    fi

    # Migrations run before the new packages are installed.
    in_target apt install --no-upgrade systemd-cryptsetup || return 1

    # Stop using the swap first. This fails when the memory cannot hold what is in the swap, and then
    # nothing is changed yet.
    if [[ -n "$(swapon --show=NAME --noheadings | xargs -r readlink -f | grep -xF "$(readlink -f "$dev")")" ]]; then
        echo "Turning off the swap on ${dev}"
        in_target swapoff "$dev" || return 1
    fi

    # crypttab may have entries of its own, so the swap line is added to them.
    if ! awk '$1 !~ /^#/ && $1 == "swap" { found = 1 } END { exit !found }' "$crypttab" 2>/dev/null; then
        template_render templates/etc/crypttab.j2 0644 "${STATE_DIR}/migration" > /dev/null || return 1

        if awk '$1 !~ /^#/ && NF > 0 { found = 1 } END { exit !found }' "$crypttab" 2>/dev/null; then
            echo "Adding the swap to ${crypttab}"
            grep -v '^#' "${STATE_DIR}/migration/etc/crypttab" >> "$crypttab" || return 1
        else
            echo "Writing ${crypttab}"
            install -m 0644 "${STATE_DIR}/migration/etc/crypttab" "$crypttab" || return 1
        fi
    fi

    # Keeps a copy where nothing reads it, and writes into the existing file to keep its mode.
    echo "Moving the swap in ${fstab} to /dev/mapper/swap, a copy is in ${fstab}.swap.bak"
    cp -p "$fstab" "${fstab}.swap.bak" || return 1
    awk -v uuid="UUID=${uuid}" '
        $1 == uuid && $3 == "swap" { print "/dev/mapper/swap  none        swap   sw                0      0"; next }
        { print }
    ' "${fstab}.swap.bak" > "$fstab" || return 1

    in_target systemctl daemon-reload || return 1
    in_target systemctl start systemd-cryptsetup@swap.service || return 1
    in_target systemctl start dev-mapper-swap.swap || return 1

    # The initramfs tries to resume from the old swap partition, which the encrypted swap overwrote,
    # and waits for it on every boot. Swap encrypted with a random key cannot be resumed from, so
    # it is not used.
    in_target update-initramfs -u -k all
}
