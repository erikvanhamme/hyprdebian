#!/usr/bin/env bash
set -euo pipefail

# Sets up the hyprdebian repository on a new Debian VM with a ZFS root pool: the repository
# gets its own dataset, nginx serves it, the signing key is generated, and the first (empty)
# index is made and signed. Run it once, with sudo, from this directory. Running it again
# keeps the existing dataset, key and repository.

# Ensure the script is run with root privileges
if [ "${EUID}" -ne 0 ]; then
    echo "Error: This script must be run with sudo or as root." >&2
    exit 1
fi

# The user who invoked sudo owns the repository and the key, and runs the updates.
RUN_USER="${SUDO_USER:-$USER}"
RUN_GROUP=$(id -gn "${RUN_USER}")

if [ "${RUN_USER}" = "root" ]; then
    echo "Error: Run this script with sudo from the user that will maintain the repository." >&2
    exit 1
fi

# Path configuration, read by update_hyprdebian_repo.sh
CONFIG_FILE="/etc/hyprdebian-repo.conf"
REPO_DIR="/srv/hyprdebian-repo"
REPO_DATASET="rpool/hyprdebian-repo"
GNUPGHOME="/var/lib/hyprdebian-repo/gnupg"
PORT="80"
SITE_FILE="/etc/nginx/sites-available/hyprdebian-repo"
KEY_UID="HyprDebian Repository <hyprdebian-repo@$(hostname -f)>"

echo "==> Installing nginx, gnupg, apt-utils and docker..."
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y nginx gnupg apt-utils docker.io

# The create_deb_*.sh scripts run their builds in docker.
echo "==> Adding ${RUN_USER} to the docker group..."
usermod -aG docker "${RUN_USER}"

echo "==> Writing ${CONFIG_FILE}..."
cat <<EOF > "${CONFIG_FILE}"
REPO_USER="${RUN_USER}"
REPO_DIR="${REPO_DIR}"
REPO_DATASET="${REPO_DATASET}"
GNUPGHOME="${GNUPGHOME}"
EOF

# The repository has its own dataset, so update_hyprdebian_repo.sh can snapshot it.
if zfs list "${REPO_DATASET}" > /dev/null 2>&1; then
    echo "==> Keeping the existing dataset ${REPO_DATASET}."
else
    echo "==> Creating the dataset ${REPO_DATASET} on ${REPO_DIR}..."
    zfs create -o mountpoint="${REPO_DIR}" "${REPO_DATASET}"
fi

# Lets the repo user snapshot the dataset without sudo. ZFS needs mount next to snapshot.
echo "==> Allowing ${RUN_USER} to snapshot ${REPO_DATASET}..."
zfs allow -u "${RUN_USER}" snapshot,mount "${REPO_DATASET}"

echo "==> Setting up ${REPO_DIR} and ${GNUPGHOME}..."
chown "${RUN_USER}:${RUN_GROUP}" "${REPO_DIR}"
chmod 0755 "${REPO_DIR}"
install -d -o "${RUN_USER}" -g "${RUN_GROUP}" -m 0700 "${GNUPGHOME}"

# The key has no passphrase, so the updates can sign without asking for one.
if runuser -u "${RUN_USER}" -- gpg --homedir "${GNUPGHOME}" --list-secret-keys --with-colons 2>/dev/null | grep -q '^sec'; then
    echo "==> Keeping the existing signing key in ${GNUPGHOME}."
else
    echo "==> Generating the signing key..."
    runuser -u "${RUN_USER}" -- gpg --homedir "${GNUPGHOME}" --batch --passphrase '' \
        --quick-gen-key "${KEY_UID}" ed25519 sign never
fi

echo "==> Writing the nginx site ${SITE_FILE}..."
cat <<EOF > "${SITE_FILE}"
server {
    listen ${PORT} default_server;
    listen [::]:${PORT} default_server;

    root ${REPO_DIR};
    autoindex on;

    location / {
        try_files \$uri \$uri/ =404;
    }
}
EOF

ln -sfn "${SITE_FILE}" /etc/nginx/sites-enabled/hyprdebian-repo

# The default site of nginx also listens on port 80.
if [ -e /etc/nginx/sites-enabled/default ]; then
    echo "==> Disabling the default nginx site..."
    rm -f /etc/nginx/sites-enabled/default
fi

echo "==> Checking the nginx configuration..."
nginx -t

echo "==> Enabling and reloading nginx..."
systemctl enable nginx
systemctl reload-or-restart nginx

echo "==> Making and signing the first index..."
runuser -u "${RUN_USER}" -- "$(dirname "$(readlink -f "$0")")/update_hyprdebian_repo.sh"

echo "====================================================="
echo "SUCCESS: HyprDebian repository is set up!"
echo "-----------------------------------------------------"
echo "URL: http://$(hostname -f)/"
echo "Signing key:"
runuser -u "${RUN_USER}" -- gpg --homedir "${GNUPGHOME}" --list-keys --keyid-format long
echo "-----------------------------------------------------"
echo "Log out and in again before running the create_deb_*.sh scripts,"
echo "so ${RUN_USER} is in the docker group."
echo "====================================================="
