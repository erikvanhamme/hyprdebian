#!/bin/bash

# Older versions created bpool and rpool with relatime, so reading a file could still write its
# access time. f_boot_pool and f_root_pool now create the pools with atime=off, which the datasets
# inherit. Turns atime off on both pools.
migrate() {
    local pool

    for pool in bpool rpool; do
        # Not needed when atime is already off.
        if [[ "$(in_target zfs get -H -o value atime "$pool")" == "off" ]]; then
            continue
        fi

        echo "Turning off atime on ${pool}"
        in_target zfs set atime=off "$pool" || return 1
    done
}
