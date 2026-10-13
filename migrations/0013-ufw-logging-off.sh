#!/bin/bash

# Older versions left the logging of ufw on, which writes a line for every blocked packet.
# pkg_enable_firewall now turns it off on a new installation. Turns it off on systems with the
# firewall.
migrate() {
    local conf="${TARGET_DIR}/etc/ufw/ufw.conf"

    # Not needed without the firewall, or when the logging is already off.
    if [[ "${Q_FIREWALL}" != "true" ]]; then
        return 0
    fi

    if ! is_installed ufw; then
        return 0
    fi

    if grep -qx 'LOGLEVEL=off' "$conf"; then
        return 0
    fi

    echo "Turning off the logging of ufw"
    in_target ufw logging off
}
