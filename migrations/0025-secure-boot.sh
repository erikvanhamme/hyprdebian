#!/bin/bash

# Older versions installed GRUB to /EFI/hyprdebian, or /EFI/hyprdebian (primary) and (secondary) on
# redundant drives. The signed GRUB for Secure Boot only reads its config from /EFI/debian, so
# b_grub2 now installs there, with boot entries named HyprDebian, or HyprDebian (P) and (S), and
# keeps grub upgrades from adding a debian entry of their own. b_mok asks shim to enroll the key
# that DKMS signs the ZFS module with. Neither runs during an upgrade, so both are done here.

# The boot entries of older versions, and the one grub upgrades added.
OLD_LABELS=("hyprdebian" "hyprdebian (primary)" "hyprdebian (secondary)" "debian")

# Succeeds when a boot entry with the given name exists.
# Usage: has_boot_entry <name>
has_boot_entry() {
    in_target efibootmgr | awk -F'\t' -v l="$1" \
        '/^Boot[0-9A-Fa-f]{4}/ && substr($1, 11) == l { found = 1 } END { exit !found }'
}

# Prints the disk of the EFI partition on a mount point. The partition is mounted first, through
# its automount unit, and must be the first partition, like the installer makes it.
# Usage: efi_disk <mount point>
efi_disk() {
    local where="$1"
    local source

    ls "${where}/" > /dev/null || return 1
    source=$(findmnt -n -o SOURCE -t vfat --mountpoint "$where") || return 1

    if [[ "$(cat "/sys/class/block/$(basename "$source")/partition")" != "1" ]]; then
        echo "ERROR: The EFI partition on ${where} (${source}) is not the first partition." >&2
        return 1
    fi

    echo "/dev/$(lsblk -n -o PKNAME "$source")"
}

migrate() {
    local esps=(/boot/efi)
    local labels=("HyprDebian")
    local esp label dir disk i package
    local layout=false
    local enroll=false

    if [[ "${Q_REDUNDANT}" == "true" ]]; then
        esps+=(/boot/efi2)
        labels=("HyprDebian (P)" "HyprDebian (S)")
    fi

    # efibootmgr and mokutil are needed for the checks below.
    local packages=()
    for package in shim-signed mokutil efibootmgr; do
        if ! is_installed "$package"; then
            packages+=("$package")
        fi
    done

    if (( ${#packages[@]} > 0 )); then
        in_target apt install -y "${packages[@]}" || return 1
    fi

    # The layout is not needed when every EFI partition has the shim in /EFI/debian and no old
    # folder, and only the new boot entries exist.
    for esp in "${esps[@]}"; do
        if [[ ! -f "${esp}/EFI/debian/shimx64.efi" ]]; then
            layout=true
        fi

        for dir in "${esp}/EFI/hyprdebian"*; do
            if [[ -e "$dir" ]]; then
                layout=true
            fi
        done
    done

    for label in "${labels[@]}"; do
        if ! has_boot_entry "$label"; then
            layout=true
        fi
    done

    for label in "${OLD_LABELS[@]}"; do
        if has_boot_entry "$label"; then
            layout=true
        fi
    done

    # The key is not needed when it is enrolled already, or waits for MokManager on the next boot.
    # Without a key the upgrade goes on, the system then cannot boot with Secure Boot on.
    if [[ ! -f /var/lib/dkms/mok.pub ]]; then
        echo "WARNING: DKMS has no key to sign the ZFS module with, so the key cannot be enrolled."
    elif in_target mokutil --test-key /var/lib/dkms/mok.pub 2>/dev/null | grep -q 'is not enrolled'; then
        enroll=true
    fi

    if [[ "$layout" == "true" ]]; then
        echo "grub-efi-amd64 grub2/update_nvram boolean false" | in_target debconf-set-selections || return 1

        # The new boot entries are made before the old ones are removed, so the system can always
        # boot, also when this migration stops halfway.
        for i in "${!esps[@]}"; do
            esp="${esps[$i]}"

            in_target grub-install --target=x86_64-efi --efi-directory="$esp" --bootloader-id=debian \
                --no-nvram --recheck --no-floppy || return 1

            disk=$(efi_disk "$esp") || return 1
            efi_boot_entry "$disk" "${labels[$i]}" || return 1
        done

        for label in "${OLD_LABELS[@]}"; do
            efi_boot_entry_remove "$label" || return 1
        done

        for esp in "${esps[@]}"; do
            for dir in "${esp}/EFI/hyprdebian"*; do
                if [[ -e "$dir" ]]; then
                    echo "Removing ${dir}"
                    rm -rf "$dir" || return 1
                fi
            done
        done
    fi

    if [[ "$enroll" == "true" ]]; then
        mok_import || return 1
        mok_notice
        MOK_REQUESTED=true
    fi
}
