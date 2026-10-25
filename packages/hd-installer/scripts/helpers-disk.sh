#!/bin/bash
# Helpers for the disks: picking them and checking the swap size. A disk is named by its WWN in
# DISK_BY_ID_DIR, or else its NVMe EUI there, which stay the same when the kernel names the disks
# in another order; a disk with neither is named by its kernel node, such as /dev/vda.

# Prints the name of a disk, by its kernel node: its wwn- link in DISK_BY_ID_DIR, or else its
# nvme-eui. link, or else the kernel node itself.
# Usage: disk_name <device>
disk_name() {
    local device="$1"
    local pattern link

    for pattern in "wwn-*" "nvme-eui.*"; do
        for link in "${DISK_BY_ID_DIR}"/${pattern}; do
            if [[ "${link}" =~ -part[0-9]+$ ]]; then
                continue
            fi
            if [[ -e "${link}" && "$(readlink -f "${link}")" == "${device}" ]]; then
                echo "${link}"
                return 0
            fi
        done
    done

    echo "${device}"
}

# Fills the array named by the argument with the name of every whole disk, see disk_name, in the
# order of their kernel names. Devices that are not disks, such as optical drives, partitions,
# device mapper and md devices, and the disks that are not drives, the zram swap, ZFS volumes,
# network block devices and RAM disks, and empty ones, are left out.
# Usage: list_disks <array>
list_disks() {
    local -n disks_out="$1"
    local device type size

    disks_out=()
    while read -r device type size; do
        if [[ "${type}" != "disk" ]]; then
            continue
        fi
        if [[ "${device}" =~ ^/dev/(zram|zd|nbd|ram)[0-9]+$ ]]; then
            continue
        fi
        if [[ "${size}" == "0" ]]; then
            continue
        fi
        disks_out+=("$(disk_name "${device}")")
    done < <(lsblk -dnpbo NAME,TYPE,SIZE)
}

# Prints the kernel name, size, model and serial number of a disk.
# Usage: disk_info <path>
disk_info() {
    local device
    device="$(readlink -f "$1")"
    printf '%s  %s\n' "$(basename "${device}")" \
        "$(lsblk -dno SIZE,MODEL,SERIAL "${device}" 2> /dev/null)"
}

# Succeeds when two paths name the same disk.
# Usage: same_disk <path> <path>
same_disk() {
    [[ "$(readlink -f "$1")" == "$(readlink -f "$2")" ]]
}

# Asks to pick a disk by number, and stores its name, see disk_name.
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

    # The answers file may name the disk by any path, such as /dev/vdb; it is stored by its name
    # in the list.
    if answered "${var_name}"; then
        show_answer "${prompt}" "${ANSWERS[${var_name}]}"
        if [[ -n "${picked}" ]] && same_disk "${ANSWERS[${var_name}]}" "${picked}"; then
            echo "Error: The disk ${ANSWERS[${var_name}]} of ${var_name} is already picked." >&2
            return 1
        fi
        for (( i = 0; i < ${#disks[@]}; i++ )); do
            if same_disk "${disks[i]}" "${ANSWERS[${var_name}]}"; then
                set_answer "${var_name}" "${disks[i]}"
                return 0
            fi
        done
        echo "Error: ${ANSWERS[${var_name}]} of ${var_name} is not a disk of this computer." >&2
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
        printf "%2d) %s%s\n    %s\n" "$((i + 1))" "${disks[i]}" "${note}" \
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

# Prints the path of a partition of a disk, by the name of the disk: <disk>-part<number> for a
# name in DISK_BY_ID_DIR, and for a kernel node <disk><number>, or <disk>p<number> when the node
# ends in a digit, such as /dev/nvme0n1p1.
# Usage: part <disk> <number>
part() {
    local disk="$1"
    local number="$2"

    if [[ "${disk}" == "${DISK_BY_ID_DIR}"/* ]]; then
        echo "${disk}-part${number}"
    elif [[ "${disk}" =~ [0-9]$ ]]; then
        echo "${disk}p${number}"
    else
        echo "${disk}${number}"
    fi
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
