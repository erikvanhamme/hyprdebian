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

# Updates the files from /etc/skel in the home directories of the user and root.
upg_home() {
    local src rel

    find_session

    for src in "${FILES[@]}"; do
        [[ "$src" == deploy/etc/skel/* ]] || continue
        rel="${src#deploy/etc/skel/}"

        home_file "$src" "${Q_USER}" "${TARGET_DIR}/home/${Q_USER}/${rel}" || return 1

        # Root only has the files that u_root_skel copies.
        case "$rel" in
            .bash_aliases|.config/nano/*)
                home_file "$src" root "${TARGET_DIR}/root/${rel}" || return 1
                ;;
        esac
    done
}

upg_save_config() {
    save_config INSTALLED_VERSION "${INSTALLER_VERSION}"
    install -D -m 0600 "$CONFIG_FILE" "${INSTALLED_CONFIG_FILE}"
}

add_dependencies "upg_main" \
    "upg_files" \
    "upg_home" \

add_dependencies "upg_post" \
    "upg_save_config" \

add_dependencies "upgrade" "upg_pre" "upg_main" "upg_post"
