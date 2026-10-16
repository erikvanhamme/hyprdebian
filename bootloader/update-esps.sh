#!/usr/bin/env bash
# Runs hd-bootloader-update for every EFI system partition of the system, so
# that each holds the UKIs of the kernels now in /boot. The kernel and
# initramfs hooks of the package hyprdebian-bootloader run this after a
# kernel or initrd is installed or removed.
#
# The ESPs are /boot/efi and, on mirrored installs, /boot/efi2; the installer
# creates /boot/efi2 only on those. Both are automounted. All ESPs are checked
# before any is written, and a missing one fails this script, which makes the
# kernel package or apt fail: a system with a missing drive should not be
# upgraded.
set -euo pipefail

UPDATE=/usr/sbin/hd-bootloader-update

if [[ $EUID -ne 0 ]]; then
    echo "hd-bootloader: must be run as root" >&2
    exit 1
fi

esps=(/boot/efi)
if [[ -d /boot/efi2 ]]; then
    esps+=(/boot/efi2)
fi

for esp in "${esps[@]}"; do
    # Listing the ESP mounts it when it is automounted. With its drive
    # missing, that fails once the mount unit times out.
    if ! ls -A "${esp}/" >/dev/null; then
        echo "hd-bootloader: ${esp}: the EFI system partition cannot be read" >&2
        exit 1
    fi

    if ! mountpoint -q "${esp}"; then
        echo "hd-bootloader: ${esp}: the EFI system partition is not mounted" >&2
        exit 1
    fi

    # An automount point is a mount point of its own, of type autofs, until
    # the ESP is mounted over it.
    if [[ "$(stat -f -c %T "${esp}/.")" == "autofs" ]]; then
        echo "hd-bootloader: ${esp}: the EFI system partition is not mounted" >&2
        exit 1
    fi
done

for esp in "${esps[@]}"; do
    echo "hd-bootloader: updating ${esp}" >&2
    "${UPDATE}" --esp "${esp}"
done
