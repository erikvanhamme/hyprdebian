#!/bin/bash
# Helpers for the disks: picking them and checking the swap size. Disks are named by their path
# in DISK_BY_ID_DIR, which stays the same when the kernel names the disks in another order.

# Fills the array named by the argument with one /dev/disk/by-id path per whole disk, in the
# order of the names. A disk has several names; the one with its model and serial number is
# preferred over the nvme-eui. and wwn- ones. Partitions, device mapper and md devices, and
# devices that are not disks, such as optical drives, are left out.
# Usage: list_disks <array>
list_disks() {
    local -n disks_out="$1"
    local -A seen=()
    local pass link name device type

    disks_out=()
    for pass in model id; do
        for link in "${DISK_BY_ID_DIR}"/*; do
            name="$(basename "${link}")"
            if [[ "${name}" =~ -part[0-9]+$ || "${name}" == dm-* || "${name}" == md-* \
                || "${name}" == lvm-* ]]; then
                continue
            fi
            if [[ "${name}" == wwn-* || "${name}" == nvme-eui.* || "${name}" == nvme-nvme.* ]]; then
                if [[ "${pass}" == "model" ]]; then
                    continue
                fi
            elif [[ "${pass}" == "id" ]]; then
                continue
            fi

            device="$(readlink -f "${link}")"
            if [[ -n "${seen[${device}]+set}" ]]; then
                continue
            fi
            type="$(lsblk -dno TYPE "${device}" 2> /dev/null)" || continue
            if [[ "${type}" != "disk" ]]; then
                continue
            fi
            seen["${device}"]=1
            disks_out+=("${link}")
        done
    done
}

# Prints the kernel name, size and model of a disk.
# Usage: disk_info <path>
disk_info() {
    local device
    device="$(readlink -f "$1")"
    printf '%s  %s\n' "$(basename "${device}")" "$(lsblk -dno SIZE,MODEL "${device}" 2> /dev/null)"
}

# Succeeds when two paths name the same disk.
# Usage: same_disk <path> <path>
same_disk() {
    [[ "$(readlink -f "$1")" == "$(readlink -f "$2")" ]]
}

# Asks to pick a disk by number, and stores its /dev/disk/by-id path.
# Usage: ask_disk <variable> <prompt> <default> [disk already picked]
# The saved answer is offered first, then the default; when neither is a disk of the list, a
# number must be typed. A disk already picked, such as the first disk of a mirror, stays in the
# list, so the numbers do not change, but cannot be picked again.
ask_disk() {
    local var_name="$1"
    local prompt="$2"
    local default="$3"
    local picked="${4:-}"
    local disks=()
    local current="${!var_name:-}"
    local default_idx="" i input note

    list_disks disks
    if (( ${#disks[@]} == 0 )); then
        echo "Error: There are no disks to pick for ${var_name}." >&2
        return 1
    fi

    for (( i = 0; i < ${#disks[@]}; i++ )); do
        if [[ -n "${picked}" ]] && same_disk "${disks[i]}" "${picked}"; then
            continue
        fi
        if [[ -n "${current}" && "${disks[i]}" == "${current}" ]]; then
            default_idx=$((i + 1))
        fi
    done
    if [[ -z "${default_idx}" ]]; then
        for (( i = 0; i < ${#disks[@]}; i++ )); do
            if [[ -n "${picked}" ]] && same_disk "${disks[i]}" "${picked}"; then
                continue
            fi
            if [[ -n "${default}" && "${disks[i]}" == "${default}" ]]; then
                default_idx=$((i + 1))
            fi
        done
    fi

    for (( i = 0; i < ${#disks[@]}; i++ )); do
        note=""
        if [[ -n "${picked}" ]] && same_disk "${disks[i]}" "${picked}"; then
            note="  (already picked)"
        fi
        printf "%2d) %s%s\n    %s\n" "$((i + 1))" "$(basename "${disks[i]}")" "${note}" \
            "$(disk_info "${disks[i]}")"
    done

    while true; do
        if [[ -n "${default_idx}" ]]; then
            if ! read -rp "${prompt} [${default_idx}]: " input; then
                echo >&2
                echo "Error: No answer for ${var_name}." >&2
                return 1
            fi
            input="${input:-${default_idx}}"
        elif ! read -rp "${prompt}: " input; then
            echo >&2
            echo "Error: No answer for ${var_name}." >&2
            return 1
        fi
        if ! [[ "${input}" =~ ^[0-9]+$ ]] || (( input < 1 || input > ${#disks[@]} )); then
            echo "Please enter a number from 1 to ${#disks[@]}." >&2
        elif [[ -n "${picked}" ]] && same_disk "${disks[input - 1]}" "${picked}"; then
            echo "That disk is already picked; pick another one." >&2
        else
            break
        fi
    done

    set_answer "${var_name}" "${disks[input - 1]}"
}

# Fills the array named by the first argument with the disks of the other arguments, each disk
# once, also when it is named twice or by two names. Empty arguments are left out.
# Usage: unique_disks <array> <disk>...
unique_disks() {
    local -n unique_out="$1"
    shift
    local disk known seen

    unique_out=()
    for disk in "$@"; do
        if [[ -z "${disk}" ]]; then
            continue
        fi
        seen=false
        for known in "${unique_out[@]}"; do
            if same_disk "${known}" "${disk}"; then
                seen=true
            fi
        done
        if [[ "${seen}" == "false" ]]; then
            unique_out+=("${disk}")
        fi
    done
}

# Fills the array named by the argument with the disks of the system: Q_ROOT_DISK_A, and with
# the mirror Q_ROOT_DISK_B.
# Usage: root_disks <array>
root_disks() {
    local -n root_out="$1"

    root_out=("${Q_ROOT_DISK_A}")
    if [[ "${Q_ROOT_REDUNDANT}" == "true" ]]; then
        root_out+=("${Q_ROOT_DISK_B}")
    fi
}

# Fills the array named by the argument with the disks of the EFI system partitions:
# Q_ESP_DISK_A, and with the mirror Q_ESP_DISK_B.
# Usage: esp_disks <array>
esp_disks() {
    local -n esp_out="$1"

    esp_out=("${Q_ESP_DISK_A}")
    if [[ "${Q_ESP_REDUNDANT}" == "true" ]]; then
        esp_out+=("${Q_ESP_DISK_B}")
    fi
}

# Fills the array named by the argument with every disk the installation uses, each once.
# Usage: all_disks <array>
all_disks() {
    unique_disks "$1" "${Q_ROOT_DISK_A}" "${Q_ROOT_DISK_B}" "${Q_ESP_DISK_A}" "${Q_ESP_DISK_B}"
}

# Prints the path of a partition of a disk, by the name of the disk in /dev/disk/by-id.
# Usage: part <disk> <number>
part() {
    echo "$1-part$2"
}

# Succeeds when a disk supports discard, which a hardware RAID volume often does not.
# Usage: can_discard <disk>
can_discard() {
    local max
    max="$(lsblk -dno DISC-MAX "$(readlink -f "$1")" 2> /dev/null | tr -d ' ')"
    [[ -n "${max}" && "${max}" != "0B" ]]
}

# Unmounts everything below the target, and the target itself, deepest first, as a mount keeps
# the mounts it is on busy.
unmount_target() {
    local target

    while read -r target; do
        umount "${target}" || return 1
    done < <(findmnt -rn -o TARGET | grep -E "^${TARGET_DIR}(/|$)" | sort -r)
}

# A swap size: a whole number of GiB, 0 for no swap.
valid_swap() {
    [[ "$1" =~ ^[0-9]+$ ]]
}
