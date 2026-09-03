#!/bin/bash

pr_pre() {
    return 0
}

pr_main() {
    return 0
}

pr_post() {
    return 0
}

pr_backup_sources() {
    mkdir -p /etc/apt/backup
    cp -a /etc/apt/sources.list /etc/apt/backup/sources.list.$(date +%s) 2>/dev/null || true
    cp -a /etc/apt/sources.list.d /etc/apt/backup/sources.list.d.$(date +%s) 2>/dev/null || true
}

pr_remove_sources() {
    rm -f /etc/apt/sources.list
    rm -f /etc/apt/sources.list.d/*.list
}

pr_install_sources() {
    cat > /etc/apt/sources.list <<'EOF'
deb http://deb.debian.org/debian trixie main contrib non-free non-free-firmware
EOF
    apt update
}

pr_install_packages() {
    local INSTALL_KVER="$(uname -r)"
    apt install -y gdisk dosfstools linux-headers-${INSTALL_KVER} zfsutils-linux debootstrap unzip python3-jinja2
}

add_dependencies "pr_main" \
    "pr_backup_sources" \
    "pr_remove_sources" \
    "pr_install_packages" \

# Only run the prereqs when not running on a hyprdebian live iso.
TARGET_USER="${SUDO_USER:-$USER}"
USER_HOME=$(eval echo "~$TARGET_USER")
RELEASE_FILE="$USER_HOME/hyprdebian.release"

if [ ! -f "$RELEASE_FILE" ]; then
    add_dependencies "install" "pr_pre" "pr_main" "pr_post"
fi
