#!/usr/bin/env bash
set -euo pipefail

# Ensure the script is run with root privileges
if [ "${EUID}" -ne 0 ]; then
    echo "Error: This script must be run with sudo or as root." >&2
    exit 1
fi

# Get the actual user who invoked sudo (to set process ownership)
RUN_USER="${SUDO_USER:-$USER}"
RUN_GROUP=$(id -gn "${RUN_USER}")

# Path Configuration
SERVICE_NAME="local-deb-repo"
# Target existing ./repo directory from where the script was called
REPO_DIR="$(pwd)/repo"
PORT="8080"
SYSTEMD_SYS_DIR="/etc/systemd/system"
UNIT_FILE="${SYSTEMD_SYS_DIR}/${SERVICE_NAME}.service"

if [ ! -d "${REPO_DIR}" ]; then
    echo "Error: Directory ${REPO_DIR} does not exist." >&2
    echo "Please make sure your repository folder is created and populated before running this script." >&2
    exit 1
fi

echo "==> Generating system-wide systemd unit file at ${UNIT_FILE}..."
cat <<EOF > "${UNIT_FILE}"
[Unit]
Description=Local Debian Repository HTTP Server
After=network.target

[Service]
Type=simple
User=${RUN_USER}
Group=${RUN_GROUP}
WorkingDirectory=${REPO_DIR}
ExecStart=/usr/bin/python3 -m http.server ${PORT}
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
EOF

echo "==> Reloading systemd daemon..."
systemctl daemon-reload

echo "==> Enabling and starting ${SERVICE_NAME}.service..."
systemctl enable --now "${SERVICE_NAME}.service"

echo "====================================================="
echo "SUCCESS: HyprDebian repository HTTP server is active!"
echo "Service status:"
systemctl status "${SERVICE_NAME}.service" --no-pager || true
echo "-----------------------------------------------------"
echo "URL: http://host:${PORT}/"
echo "APT sources.list line:"
echo "  deb [trusted=yes] http://host:${PORT}/ sid main"
echo "====================================================="
