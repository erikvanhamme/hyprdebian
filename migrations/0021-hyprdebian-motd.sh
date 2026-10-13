#!/bin/bash

# Older versions showed the login messages of Debian. The upgrade adds 00-hyprdebian to
# /etc/update-motd.d. Like b_motd on a new installation, empties /etc/motd, removes the uname
# line of Debian, and diverts /etc/issue, so base-files writes its version to /etc/issue.debian.
migrate() {
    if [[ -s ${TARGET_DIR}/etc/motd ]]; then
        : > ${TARGET_DIR}/etc/motd || return 1
    fi

    if [[ -e ${TARGET_DIR}/etc/update-motd.d/10-uname ]]; then
        rm -f ${TARGET_DIR}/etc/update-motd.d/10-uname || return 1
    fi

    if [[ "$(in_target dpkg-divert --truename /etc/issue)" == "/etc/issue" ]]; then
        in_target dpkg-divert --local --rename --divert /etc/issue.debian --add /etc/issue || return 1
    fi

    # upg_save_config writes it again at the end of the upgrade. Writing it now means the console
    # never goes without one when a later part of the upgrade fails.
    write_issue
}
