#!/bin/bash

# Older versions let the firewall accept ssh connections from everywhere, over IPv4 and IPv6, with
# 'ufw allow SSH'. pkg_enable_firewall now accepts them from the SSH_SUBNETS only. Adds the rules
# for these subnets and removes the old rule on systems with the firewall and ssh.
migrate() {
    local added

    # Not needed without the firewall or without ssh.
    if [[ "${Q_FIREWALL}" != "true" || "${Q_OPENSSH}" != "true" ]]; then
        return 0
    fi

    if ! is_installed ufw; then
        return 0
    fi

    # Add the new rules first, so a client in these subnets can always connect. ufw skips rules
    # that are already there. ufw keeps established connections, so an upgrade over ssh from
    # another subnet keeps running, but new connections from there are refused.
    echo "Accepting ssh connections only from ${SSH_SUBNETS[*]}"
    ufw_allow_ssh || return 1

    added=$(in_target ufw show added) || return 1

    if grep -qx 'ufw allow SSH' <<< "$added"; then
        in_target ufw delete allow SSH || return 1
    fi
}
