#!/bin/bash

# Older versions queued iscsi-login.service for the backup tools from deploy/etc/systemd/sytem, a
# path that does not exist. file_deploy created /etc/systemd/sytem, and then failed to copy the
# unit, without stopping the installation. The upgrade adds the unit in /etc/systemd/system, like
# o_backuptools does on a new installation. Removes the misspelled directory.
migrate() {
    local dir="${TARGET_DIR}/etc/systemd/sytem"

    # Not needed when the directory is gone.
    if [[ ! -d "$dir" ]]; then
        return 0
    fi

    # The unit can only be there when it was copied there by hand.
    if [[ -e "$dir/iscsi-login.service" ]]; then
        echo "Removing ${dir}/iscsi-login.service, systemd does not read it there"
        rm -f "$dir/iscsi-login.service" || return 1
    fi

    if [[ -n "$(ls -A "$dir")" ]]; then
        echo "WARNING: Keeping ${dir}, it has other files."
        return 0
    fi

    echo "Removing ${dir}"
    rmdir "$dir"
}
