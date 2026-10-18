#!/bin/bash

b_pre() {
    return 0
}

b_main() {
    return 0
}

b_post() {
    return 0
}

b_kernel() {
    if [[ "${Q_KERNEL}" == "latest" ]]; then
        return 0
    fi

    mkdir -p ${TARGET_DIR}/tmp/deb || return 1
    cp kernels/*${Q_KERNEL}* ${TARGET_DIR}/tmp/deb/ || return 1

    # dpkg fails when the packages have missing dependencies, apt install -f then installs them.
    in_target dpkg -R -i /tmp/deb/
    in_target apt install -y -f
}

b_systemd() {
    add_services rotate-machine-id

    add_files \
        deploy/etc/systemd/journald.conf \
        deploy/etc/systemd/system/rotate-machine-id.service \

}

add_dependencies "b_main" \
    "b_kernel" \
    "b_systemd" \

add_dependencies "install" "b_pre" "b_main" "b_post"
