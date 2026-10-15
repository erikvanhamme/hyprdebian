#!/usr/bin/env bash
set -euo pipefail

# Sets up the hyprdebian dashboard on a Debian VM: installs hd-dash, writes its config and its
# systemd service, and starts it. The hyprdebian systems send their reports to it with hd-report.
# Run it with sudo, from this directory. Running it again updates hd-dash and the service, and
# keeps the config and the database.

if [ "${EUID}" -ne 0 ]; then
    echo "Error: This script must be run with sudo or as root." >&2
    exit 1
fi

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
CONFIG_FILE="/etc/hyprdebian-dash.conf"
PROGRAM="/usr/local/bin/hd-dash"
UNIT_FILE="/etc/systemd/system/hd-dash.service"
PORT="80"

echo "==> Installing python3..."
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y python3

echo "==> Installing ${PROGRAM}..."
install -m 0755 "${SCRIPT_DIR}/../hd-dash" "${PROGRAM}"

if [ -e "${CONFIG_FILE}" ]; then
    echo "==> Keeping the existing ${CONFIG_FILE}."
else
    echo "==> Writing ${CONFIG_FILE}..."
    cat <<EOF > "${CONFIG_FILE}"
# Settings of hd-dash, the hyprdebian dashboard. Restart hd-dash.service after a change.
LISTEN=0.0.0.0
PORT=${PORT}
DATABASE=/var/lib/hyprdebian-dash/dash.db

# A host that did not report for this long is shown as stale. hd-report runs every 15 minutes.
STALE_MINUTES=45

# How long the load, memory and pool history is kept.
HISTORY_DAYS=30
EOF
fi

# hd-dash runs as a user of its own that systemd makes up, with the database in its state
# directory. It may bind to port 80 without being root.
echo "==> Writing ${UNIT_FILE}..."
cat <<EOF > "${UNIT_FILE}"
[Unit]
Description=HyprDebian Dashboard
After=network.target

[Service]
ExecStart=${PROGRAM} serve
Restart=on-failure
DynamicUser=yes
StateDirectory=hyprdebian-dash
AmbientCapabilities=CAP_NET_BIND_SERVICE
CapabilityBoundingSet=CAP_NET_BIND_SERVICE
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
PrivateDevices=yes
NoNewPrivileges=yes

[Install]
WantedBy=multi-user.target
EOF

echo "==> Enabling and (re)starting hd-dash..."
systemctl daemon-reload
systemctl enable hd-dash.service
systemctl restart hd-dash.service

# Open the port when ufw is active, like on a hyprdebian vm with the firewall.
if command -v ufw > /dev/null && ufw status | grep -q "^Status: active"; then
    echo "==> Allowing port ${PORT} in ufw..."
    ufw allow "${PORT}/tcp"
fi

echo "====================================================="
echo "SUCCESS: HyprDebian dashboard is set up!"
echo "-----------------------------------------------------"
echo "URL: http://$(hostname -f)/"
echo "Hosts:   sudo hd-dash hosts"
echo "Forget:  sudo hd-dash forget <host>"
echo "====================================================="
