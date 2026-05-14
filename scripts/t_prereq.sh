#!/bin/bash

t_prereq() {
    return 0
}

p_backup_sources() {
    mkdir -p /etc/apt/backup
    cp -a /etc/apt/sources.list /etc/apt/backup/sources.list.$(date +%s) 2>/dev/null || true
    cp -a /etc/apt/sources.list.d /etc/apt/backup/sources.list.d.$(date +%s) 2>/dev/null || true
}

p_remove_sources() {
    rm -f /etc/apt/sources.list
    rm -f /etc/apt/sources.list.d/*.list
}

p_install_sources() {
    cat > /etc/apt/sources.list <<'EOF'
deb http://deb.debian.org/debian trixie main contrib non-free non-free-firmware
EOF
    apt update
}

p_install_packages() {
    local INSTALL_KVER="$(uname -r)"
    apt install -y gdisk dosfstools linux-headers-${INSTALL_KVER} zfsutils-linux debootstrap unzip
}

add_dependencies "t_prereq" "p_backup_sources" "p_remove_sources" "p_install_sources" "p_install_packages"
add_dependencies "install" "t_prereq"
