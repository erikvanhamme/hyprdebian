#!/bin/bash

# Points /etc/resolv.conf to the DNS servers of the uplink, like svc_systemd does on a new
# installation. Installing systemd-resolved points it to the stub resolver instead, which
# 00-direct-uplink.conf turns off.
migrate() {
    if [[ "${Q_OS}" != "hyprdebian" ]]; then
        return 0
    fi

    # Not needed when the link is already right, like on systems installed with svc_systemd.
    if [[ "$(readlink ${TARGET_DIR}/etc/resolv.conf)" == "/run/systemd/resolve/resolv.conf" ]]; then
        return 0
    fi

    # systemd-resolved is new for systems installed before it was used, and migrations run
    # before the new packages are installed.
    in_target apt install --no-upgrade systemd-resolved || return 1

    # Without a running systemd-resolved the link would point to nothing, and break name
    # resolution.
    if [[ ! -e ${TARGET_DIR}/run/systemd/resolve/resolv.conf ]]; then
        echo "ERROR: /run/systemd/resolve/resolv.conf not found, is systemd-resolved running?" >&2
        return 1
    fi

    ln -sf /run/systemd/resolve/resolv.conf ${TARGET_DIR}/etc/resolv.conf
}
