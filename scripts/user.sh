#!/bin/bash

u_pre() {
    return 0
}

u_main() {
    return 0
}

u_post() {
    return 0
}

u_filesystem() {
    zfs create rpool/home/${Q_USER}
}

u_add() {
    in_target STDOUTMSGLEVEL=fatal STDERRMSGLEVEL=fatal adduser ${Q_USER}
}

u_skel() {
    in_target cp -a /etc/skel/. /home/${Q_USER}/
}

u_chown() {
    in_target chown -R ${Q_USER}:${Q_USER} /home/${Q_USER}
}

u_groups() {
    local groups
    groups=$(IFS=,; echo "${USER_GROUPS[*]}")

    in_target usermod -a -G "$groups" "${Q_USER}"
}

u_root_skel() {
    mkdir -p ${TARGET_DIR}/root/.config
    cp -r deploy/etc/skel/.config/nano ${TARGET_DIR}/root/.config
    cp deploy/etc/skel/.bash_aliases ${TARGET_DIR}/root
    cp ${TARGET_DIR}/etc/skel/.bashrc ${TARGET_DIR}/root
    cp ${TARGET_DIR}/etc/skel/.bash_logout ${TARGET_DIR}/root
}

add_dependencies "u_main" \
    "u_filesystem" \
    "u_add" \
    "u_skel" \
    "u_chown" \
    "u_groups" \
    "u_root_skel" \

add_dependencies "install" "u_pre" "u_main" "u_post"
