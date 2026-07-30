#!/usr/bin/env bash
set -euo pipefail

# Ensure the script is run with root privileges
if [ "${EUID}" -ne 0 ]; then
    echo "Error: This script must be run with sudo or as root." >&2
    exit 1
fi

SERVICE_NAME="local-deb-repo"
SYSTEMD_SYS_DIR="/etc/systemd/system"
UNIT_FILE="${SYSTEMD_SYS_DIR}/${SERVICE_NAME}.service"

if systemctl is-active --quiet "${SERVICE_NAME}.service" 2>/dev/null; then
    echo "==> Stopping ${SERVICE_NAME}.service..."
    systemctl stop "${SERVICE_NAME}.service"
fi

if systemctl is-enabled --quiet "${SERVICE_NAME}.service" 2>/dev/null; then
    echo "==> Disabling ${SERVICE_NAME}.service..."
    systemctl disable "${SERVICE_NAME}.service"
fi

if [ -f "${UNIT_FILE}" ]; then
    echo "==> Removing unit file at ${UNIT_FILE}..."
    rm -f "${UNIT_FILE}"

    echo "==> Reloading systemd daemon..."
    systemctl daemon-reload
    systemctl reset-failed 2>/dev/null || true

    echo "=================================================="
    echo "SUCCESS: ${SERVICE_NAME}.service has been completely removed."
    echo "=================================================="
else
    echo "==> Unit file ${UNIT_FILE} not found. Nothing to remove."
fi
