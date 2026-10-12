#!/bin/bash

# Older versions deleted /etc/machine-id on shutdown with clear-machine-id.service, so systemd saw
# every boot as a first boot and applied the presets. rotate-machine-id.service replaced it, and
# svc_systemd masks the units that act on a first boot. Removes the old service and adds the masks.
migrate() {
    local unit="${TARGET_DIR}/etc/systemd/system/clear-machine-id.service"
    local masks=(systemd-firstboot.service systemd-preset-all.service systemd-machine-id-commit.service)
    local mask
    local needed=false

    # Not needed when the old service is gone and the masks are there.
    if [[ -e "$unit" ]]; then
        needed=true
    fi

    for mask in "${masks[@]}"; do
        if [[ "$(readlink "${TARGET_DIR}/etc/systemd/system/${mask}")" != "/dev/null" ]]; then
            needed=true
        fi
    done

    if [[ "$needed" != "true" ]]; then
        return 0
    fi

    if [[ -e "$unit" ]]; then
        # Only disable it: stopping it now would delete the machine ID of the running system.
        in_target systemctl disable clear-machine-id.service || return 1
        rm -f "$unit" || return 1
    fi

    in_target systemctl mask "${masks[@]}" || return 1
    in_target systemctl daemon-reload
}
