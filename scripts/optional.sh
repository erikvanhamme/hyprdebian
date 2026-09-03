#!/bin/bash

o_pre() {
    return 0
}

o_main() {
    return 0
}

o_post() {
    return 0
}

o_wifi() {
    return 0
}

o_firewall() {
    add_packages ufw
    add_dependencies "pkg_post" "o_enable_firewall"
}

o_enable_firewall() {
    in_target ufw enable
    
    if [[ "${Q_OPENSSH}" == "true" ]]; then
        in_target ufw allow SSH
    fi
}

o_font() {
    in_target apt install -y fontconfig wget unzip
    mkdir ${TARGET_DIR}/usr/local/share/fonts
    wget https://github.com/ryanoasis/nerd-fonts/releases/download/v3.4.0/JetBrainsMono.zip
    unzip JetBrainsMono.zip -d ${TARGET_DIR}/usr/local/share/fonts
}

o_greetd() {
    add_packages greetd
    add_services greetd

    mkdir -p ${TARGET_DIR}/etc/greetd

    python3 render.py templates/etc/greetd/config.toml.j2 ${TARGET_DIR}/etc/greetd/config.toml -v Q_USER=${Q_USER}
}

o_hyprland() {
    add_packages uwsm kitty desktop-base dbus-user-session hyprland hyprland-qtutils \
        wofi hyprpaper libglib2.0-bin hypridle python3-terminaltexteffects hyprlock \
        libnotify-bin mako-notifier audacious mpv imv firefox pipewire wireplumber \
        pulseaudio-utils grim slurp swappy wl-clipboard wiremix playerctl brightnessctl \
        hyprshutdown
}

o_theme() {
    add_packages nwg-look qt5-gtk-platformtheme qt6-gtk-platformtheme
}

o_user_log() {
    touch ${TARGET_DIR}/var/log/hyprland.log
    in_target chown ${Q_USER}:${Q_USER} /var/log/hyprland.log
}

o_user_icons() {
    mkdir ${TARGET_DIR}/home/${Q_USER}/.icons
    in_target chown ${Q_USER}:${Q_USER} /home/${Q_USER}/.icons
}

o_pipewire() {
    mkdir -p ${TARGET_DIR}/home/${Q_USER}/.config/systemd/user/default.target.wants
    ln -s ${TARGET_DIR}/usr/lib/systemd/user/pipewire.service ${TARGET_DIR}/home/${Q_USER}/.config/systemd/user/default.target.wants/
    ln -s ${TARGET_DIR}/usr/lib/systemd/user/wireplumber.service ${TARGET_DIR}/home/${Q_USER}/.config/systemd/user/default.target.wants/
    in_target chown -R ${Q_USER}:${Q_USER} /home/${Q_USER}/.config/systemd
}

o_sniptool() {
    mkdir -p ${TARGET_DIR}/home/${Q_USER}/Pictures/Screenshots
    in_target chown ${Q_USER}:${Q_USER} /home/${Q_USER}/Pictures/Screenshots
}

o_desktop() {
    add_dependencies "u_post" \
        "o_user_log" \
        "o_user_icons" \
        "o_pipewire" \
        "o_sniptool" \
        
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
    add_packages qemu-system-x86-headless qemu-system-gui- qemu-utils libvirt-daemon-system libvirt-clients bridge-utils ovmf

    if [[ "${Q_DESKTOP}" == "true" ]]; then
        add_packages virt-manager
    fi

    add_user_groups libvirt kvm

    add_services libvirtd

    add_dependencies "pkg_post" "pkg_qemu_kvm"
    add_dependencies "svc_post" "svc_qemu_kvm"
}

o_cups() {
    add_packages cups
    add_user_groups lpadmin
}

o_rust() {
    return 0
}

o_openssh() {
    add_packages openssh-server kitty-terminfo
}

add_dependencies "o_desktop" \
    "o_font" \
    "o_greetd" \
    "o_hyprland" \
    "o_theme" \

add_dependencies "install" "o_pre" "o_main" "o_post"
