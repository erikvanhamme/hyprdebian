#!/bin/bash

# HyprDebian does not support hibernation, but the initramfs picks a swap partition to resume
# from when no resume device is configured. On every boot it waits for that device, and gives up
# after a while. b_buildtools now deploys /etc/initramfs-tools/conf.d/resume with RESUME=none.
migrate() {
    local conf="${TARGET_DIR}/etc/initramfs-tools/conf.d/resume"

    # Not needed when the initramfs already resumes from nothing.
    if cmp -s deploy/etc/initramfs-tools/conf.d/resume "$conf"; then
        return 0
    fi

    file_deploy deploy/etc/initramfs-tools/conf.d/resume || return 1

    in_target update-initramfs -u -k all
}
