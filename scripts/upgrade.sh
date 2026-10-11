#!/bin/bash

upg_pre() {
    return 0
}

upg_main() {
    return 0
}

upg_post() {
    return 0
}

# Merges with meld need the Wayland session upgrade.sh was started from.
upg_session() {
    find_session
}

# Runs the migrations that did not run on this system yet, in order. Each one is recorded right
# after it succeeded, so it never runs twice, even when a later part of the upgrade fails.
upg_migrations() {
    local file name

    for file in migrations/*.sh; do
        [[ -e "$file" ]] || continue
        name=$(basename "$file" .sh)

        if grep -qxF "$name" "${TARGET_DIR}${MIGRATIONS_FILE}" 2>/dev/null; then
            continue
        fi

        log_status "$COLOR_YELLOW" "START  " "migration ${name}"

        unset -f migrate
        source "$file" || return 1

        if ! migrate; then
            log_status "$COLOR_RED" "FAIL   " "migration ${name}"
            return 1
        fi

        echo "$name" >> "${TARGET_DIR}${MIGRATIONS_FILE}" || return 1
        log_status "$COLOR_GREEN" "DONE   " "migration ${name}"
    done
}

# Installs the packages the new version adds. Packages that were removed after an earlier
# installation or upgrade are not installed again.
upg_packages() {
    local packages
    packages=$(new_items "${PACKAGES[*]}" "${INSTALLED_PACKAGES:-}")

    if [[ -z "$packages" ]]; then
        return 0
    fi

    echo "New packages: ${packages}"
    in_target apt update || return 1

    # Upgrading the packages that are already installed is left to the user.
    in_target apt install --no-upgrade ${packages}
}

# Deploys the files in /etc/skel and /usr/local/bin that differ from the ones on the system.
upg_files() {
    local src dst

    for src in "${FILES[@]}"; do
        case "$src" in
            deploy/etc/skel/*|deploy/usr/local/bin/*) ;;
            *) continue ;;
        esac

        dst=$(file_dst "$src" "${TARGET_DIR}")

        if [[ -f "$dst" ]] && cmp -s "$src" "$dst"; then
            continue
        fi

        file_deploy "$src" || return 1
    done
}

# Updates the other deployed files, which are all in /etc. These can be changed on the system, so
# the user chooses what happens with a file that differs from the new version.
upg_etc() {
    local src

    for src in "${FILES[@]}"; do
        case "$src" in
            deploy/etc/skel/*|deploy/usr/local/bin/*) continue ;;
        esac

        update_file "$src" root "$(file_dst "$src" "${TARGET_DIR}")" || return 1
    done
}

# Adds the user to the groups the new version adds. Groups the user was removed from after an
# earlier installation or upgrade are not added again.
upg_groups() {
    local groups
    groups=$(new_items "${USER_GROUPS[*]}" "${INSTALLED_USER_GROUPS:-}")

    if [[ -z "$groups" ]]; then
        return 0
    fi

    echo "Adding ${Q_USER} to the groups: ${groups} (active after logging in again)"
    in_target usermod -a -G "${groups// /,}" "${Q_USER}"
}

# Enables the services the new version adds. Services that were disabled after an earlier
# installation or upgrade are not enabled again.
upg_services() {
    local services svc

    # Unit files may have changed in upg_etc.
    in_target systemctl daemon-reload || return 1

    services=$(new_items "${SERVICES[*]}" "${INSTALLED_SERVICES:-}")

    if [[ -z "$services" ]]; then
        return 0
    fi

    # Only enable them. Starting a service on the running system can disturb it, like
    # rotate-machine-id, so they start on the next boot.
    for svc in ${services}; do
        in_target systemctl enable "$svc" || return 1
    done

    echo "Enabled services: ${services} (started after a reboot)"
}

# Updates the files from /etc/skel in the home directories of the user and root.
upg_home() {
    local src rel

    for src in "${FILES[@]}"; do
        [[ "$src" == deploy/etc/skel/* ]] || continue
        rel="${src#deploy/etc/skel/}"

        update_file "$src" "${Q_USER}" "${TARGET_DIR}/home/${Q_USER}/${rel}" || return 1

        # Root only has the files that u_root_skel copies.
        case "$rel" in
            .bash_aliases|.config/nano/*)
                update_file "$src" root "${TARGET_DIR}/root/${rel}" || return 1
                ;;
        esac
    done
}

upg_save_config() {
    save_config INSTALLED_VERSION "${INSTALLER_VERSION}"
    install -D -m 0600 "$CONFIG_FILE" "${INSTALLED_CONFIG_FILE}"
}

add_dependencies "upg_pre" \
    "upg_session" \

add_dependencies "upg_main" \
    "upg_migrations" \
    "upg_packages" \
    "upg_files" \
    "upg_etc" \
    "upg_groups" \
    "upg_services" \
    "upg_home" \

add_dependencies "upg_post" \
    "upg_save_config" \

add_dependencies "upgrade" "upg_pre" "upg_main" "upg_post"
