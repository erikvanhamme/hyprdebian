#!/bin/bash

# Older versions did not mask rpcbind, which nfs-common pulls in, and which listens on the network.
# NFSv4 does not need it. svc_systemd masks rpcbind.socket and rpcbind.service on a new
# installation.
migrate() {
    local units=(rpcbind.socket rpcbind.service)
    local unit
    local needed=false

    # Not needed when the masks are already there.
    for unit in "${units[@]}"; do
        if [[ "$(readlink "${TARGET_DIR}/etc/systemd/system/${unit}")" != "/dev/null" ]]; then
            needed=true
        fi
    done

    if [[ "$needed" != "true" ]]; then
        return 0
    fi

    # Only mask them. Stopping rpcbind now could disturb an NFSv3 mount, so it stops on the next
    # boot.
    in_target systemctl mask "${units[@]}" || return 1
    in_target systemctl daemon-reload
}
