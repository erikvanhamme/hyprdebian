#!/bin/bash

svc_pre() {
    return 0
}

svc_main() {
    return 0
}

svc_post() {
    return 0
}

svc_enable() {
    for svc in ${SERVICES[*]}; do
        in_target systemctl enable $svc || return 1
    done
}

svc_backuptools() {

    # Make sure iscsi target is not started automatically, it should happen on demand.
    in_target systemctl disable iscsid.service
}

add_dependencies "svc_main" \
    "svc_enable" \

add_dependencies "install" "svc_pre" "svc_main" "svc_post"
