#!/bin/bash

# Older versions trusted the local hyprdebian repo without checking signatures ("Trusted: yes").
# The repo is now signed. Like b_apt_init, downloads its signing key from the repo and renders
# hyprdebian.local.sources again, which checks the signatures with that key.
migrate() {
    local sources="${TARGET_DIR}/etc/apt/sources.list.d/hyprdebian.local.sources"
    # Outside sources.list.d, where apt would mention it on every run.
    local backup="${TARGET_DIR}/etc/apt/hyprdebian.local.sources.unsigned"

    # Not needed when the local repo is not used.
    if [[ "${Q_REPO_ENABLED}" != "true" || ! -e "$sources" ]]; then
        return 0
    fi

    # Not needed when the sources already check the signatures with the key.
    if ! grep -qi '^Trusted:' "$sources" && grep -qF "Signed-By: ${REPO_KEY}" "$sources" && [[ -s "${TARGET_DIR}${REPO_KEY}" ]]; then
        return 0
    fi

    repo_key_download || return 1

    cp "$sources" "$backup" || return 1
    template_render templates/etc/apt/sources.list.d/hyprdebian.local.sources.j2 || return 1

    # Fails when the repo is not signed yet. The old sources are put back, so apt keeps working
    # and the migration runs again on the next upgrade.
    if ! in_target apt update; then
        echo "WARNING: apt update failed with the signed repo, restoring the old ${sources}."
        mv "$backup" "$sources"
        return 1
    fi

    rm -f "$backup"
}
