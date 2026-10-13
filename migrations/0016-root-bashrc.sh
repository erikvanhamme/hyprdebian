#!/bin/bash

# Older versions left the .bashrc of Debian for root, which does not load ~/.bash_aliases.
# u_root_skel now copies .bashrc and .bash_logout from /etc/skel to /root on a new installation, and
# the upgrade adds .bash_aliases for root. Updates .bashrc and .bash_logout of root from /etc/skel.
# A file that was changed on the system is kept, replaced or merged, as the user chooses.
migrate() {
    local name

    # update_file leaves a file alone when it is the same as the one in /etc/skel.
    for name in .bashrc .bash_logout; do
        update_file "${TARGET_DIR}/etc/skel/${name}" root "${TARGET_DIR}/root/${name}" || return 1
    done
}
