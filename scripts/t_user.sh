#!/bin/bash

t_user() {
    return 0
}

user_filesystem() {
    zfs create rpool/home/${Q_USER}
}

user_add() {
    in_target STDOUTMSGLEVEL=fatal STDERRMSGLEVEL=fatal adduser ${Q_USER}
}

user_skel() {
    in_target cp -a /etc/skel/. /home/${Q_USER}/
}

user_chown() {
    in_target chown -R ${Q_USER}:${Q_USER} /home/${Q_USER}
}

user_groups() {
    local groups
    groups=$(IFS=,; echo "${USER_GROUPS[*]}")

    in_target usermod -a -G "$groups" "${Q_USER}"
}

user_desktop() {
    return 0 # Empty task that will be used for the optional desktop install to append dependencies to.
}

add_dependencies "t_user" \
    "user_filesystem" \
    "user_add" \
    "user_skel" \
    "user_chown" \
    "user_groups" \
    "user_desktop"

add_dependencies "t_install" \
    "t_user"
