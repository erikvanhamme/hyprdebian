#!/bin/bash

t_optional() {
    return 0
}

o_wifi() {
    add_packages iwd firmware-iwlwifi
    add_services iwd

    mkdir -p ${TARGET_DIR}/etc/iwd
    write_file ${TARGET_DIR}/etc/iwd/main.conf 0644 <<EOF
[General]
EnableNetworkConfiguration=true
EOF
}

o_desktop() {
    add_dependencies "user_desktop" \
        "user_log" \
        "tgt_pipewire" \
        "tgt_sniptool"
}

tgt_font() {
    in_target apt install -y fontconfig wget unzip
    mkdir ${TARGET_DIR}/usr/local/share/fonts
    wget https://github.com/ryanoasis/nerd-fonts/releases/download/v3.4.0/JetBrainsMono.zip
    unzip JetBrainsMono.zip -d ${TARGET_DIR}/usr/local/share/fonts
}

tgt_greetd() {
    add_packages greetd
    add_services greetd

    mkdir -p ${TARGET_DIR}/etc/greetd

    python3 render.py templates/greetd/config.toml.j2 ${TARGET_DIR}/etc/greetd/config.toml -v Q_USER=${Q_USER}
}

tgt_hyprland() {
    add_packages uwsm kitty desktop-base dbus-user-session hyprland hyprland-qtutils \
        wofi hyprpaper libglib2.0-bin hypridle python3-terminaltexteffects hyprlock \
        libnotify-bin mako-notifier audacious mpv imv firefox pipewire wireplumber \
        pulseaudio-utils grim slurp swappy wl-clipboard
}

user_log() {
    touch ${TARGET_DIR}/var/log/hyprland.log
    in_target chown ${Q_USER}:${Q_USER} /var/log/hyprland.log
}

tgt_pipewire() {
    mkdir -p ${TARGET_DIR}/home/${Q_USER}/.config/systemd/user/default.target.wants
    ln -s ${TARGET_DIR}/usr/lib/systemd/user/pipewire.service ${TARGET_DIR}/home/${Q_USER}/.config/systemd/user/default.target.wants/
    ln -s ${TARGET_DIR}/usr/lib/systemd/user/wireplumber.service ${TARGET_DIR}/home/${Q_USER}/.config/systemd/user/default.target.wants/
}

tgt_sniptool() {
    mkdir -p ${TARGET_DIR}/home/${Q_USERN}/Pictures/Screenshots
    in_target chown ${Q_USER}:${Q_USER} /home/${Q_USER}/Pictures/Screenshots
}

o_docker() {
    add_packages docker.io
    add_user_groups docker

    mkdir -p ${TARGET_DIR}/etc/docker
    write_file ${TARGET_DIR}/etc/docker/daemon.json 0644 <<EOF
{
  "bip": "192.168.253.1/24"
}
EOF
}

o_qemu_kvm() {
    add_packages qemu-system-x86 qemu-utils libvirt-daemon-system libvirt-clients bridge-utils ovmf
    if [[ "${Q_DESKTOP}" == "true" ]]; then
        add_packages virt-manager
    fi
    add_user_groups libvirt kvm
    add_services libvirtd
}

o_cups() {
    add_packages cups
    add_user_groups lpadmin
}

add_dependencies "o_desktop" \
    "tgt_font" \
    "tgt_greetd" \
    "tgt_hyprland"

add_dependencies "t_install" \
    "t_optional"
