#!/bin/bash
# The user phase: makes the user account, with its home directory on a ZFS dataset of its own.

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
    if zfs list "rpool/home/${Q_USER}" &> /dev/null; then
        return 0
    fi

    zfs create "rpool/home/${Q_USER}"
}

u_add() {
    # The home directory is a ZFS dataset created by u_filesystem, and is filled by u_skel. A rerun
    # keeps the account, and sets the password again.
    if ! in_target getent passwd "${Q_USER}" > /dev/null; then
        in_target useradd \
            --no-create-home \
            --home-dir "/home/${Q_USER}" \
            --user-group \
            --shell /bin/bash \
            --comment "${Q_FULLNAME}" \
            "${Q_USER}" || return 1
    fi

    printf '%s:%s\n' "${Q_USER}" "${Q_PASSWORD}" | in_target chpasswd
}

u_skel() {
    in_target cp -a /etc/skel/. "/home/${Q_USER}/"
}

u_chown() {
    in_target chown -R "${Q_USER}:${Q_USER}" "/home/${Q_USER}"
}

u_groups() {
    local groups
    groups=$(IFS=,; echo "${USER_GROUPS[*]}")

    in_target usermod -a -G "${groups}" "${Q_USER}"
}

add_dependencies "u_main" \
    "u_filesystem" \
    "u_add" \
    "u_skel" \
    "u_chown" \
    "u_groups" \

add_dependencies "install_root" "u_pre" "u_main" "u_post"
