#!/bin/bash

t_user() {
    return 0
}

user_filesystem() {
    zfs create rpool/home/${USERNAME}
}

user_add() {
    in_target STDOUTMSGLEVEL=fatal STDERRMSGLEVEL=fatal adduser ${USERNAME}
}

user_skel() {
    in_target cp -a /etc/skel/. /home/${USERNAME}/
}

user_chown() {
    in_target chown -R ${USERNAME}:${USERNAME} /home/${USERNAME}
}

user_groups() {
    local groups
    groups=$(IFS=,; echo "${USER_GROUPS[*]}")

    in_target usermod -a -G "$groups" "${USERNAME}"
}

add_dependencies "t_user" "user_filesystem" "user_add" "user_skel" "user_chown" "user_groups"
add_dependencies "t_install" "t_user"
