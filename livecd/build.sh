#!/bin/bash
# Builds the HyprDebian live ISO. This is the entrypoint of the docker image
# of the Dockerfile next to it; run it with make iso (see the Makefile).
#
# It expects the repository read-only at /src, with the packages built in
# /src/build, a folder for the ISO at /out, and in DEBS the packages for the
# ISO, separated by spaces, each as <repos>:<path>, with the repos it goes to,
# separated by commas, and its path relative to /src. It builds in /build:
#   1. the live-build configuration: lb config, then the files of
#      /src/livecd/config on top, with the package hd-installer in
#      packages.chroot
#   2. the local repository of the live system, in /srv/hyprdebian/repo, with
#      every package in DEBS in the suites of its repos; hd-installer installs
#      the hd-* packages from it
#   3. the ISO, which is copied to /out/hyprdebian-live-amd64.iso, owned by
#      HOST_UID and HOST_GID
set -euo pipefail

SRC=/src
OUT=/out
BUILD=/build
ISO=hyprdebian-live-amd64.iso
# The local repository, in the live-build configuration. hd-installer has the same path in
# LIVE_REPO_DIR of its constants.sh.
REPO=config/includes.chroot/srv/hyprdebian/repo

version=$(python3 -c 'import json, sys; v = json.load(open(sys.argv[1])); print("%d.%d" % (v["major"], v["minor"]))' \
    "${SRC}/version.json")
echo "=== HyprDebian live ${version} ==="

if [[ -z "${DEBS:-}" ]]; then
    echo "Error: DEBS names no packages; run make iso in the root of the repository." >&2
    exit 1
fi

# The live-build configuration. Recommends are left out to keep the image small, so the package
# lists in config/ name everything the live system needs.
mkdir "${BUILD}/live" && cd "${BUILD}/live"
lb config \
    --distribution trixie \
    --archive-areas "main contrib non-free-firmware" \
    --apt-recommends false \
    --apt-indices false \
    --debian-installer none \
    --firmware-binary false \
    --memtest none \
    --bootloaders "grub-efi syslinux" \
    --binary-images iso-hybrid \
    --bootappend-live "boot=live components hostname=hyprdebian-live username=user" \
    --iso-application "HyprDebian live" \
    --iso-publisher "HyprDebian" \
    --iso-volume "HyprDebian live ${version}"

cp -a "${SRC}/livecd/config/." config/

# The boot menus of live-build, for grub (UEFI) and isolinux (BIOS), wait for a key. These copies
# boot the live system after 5 seconds, and grub does not beep.
mkdir -p config/bootloaders
cp -a /usr/share/live/build/bootloaders/grub-pc /usr/share/live/build/bootloaders/isolinux \
    config/bootloaders/
sed -i -e 's/^set default=0$/set default=0\nset timeout=5/' -e '/^insmod play$/d' -e '/^play /d' \
    config/bootloaders/grub-pc/config.cfg
sed -i 's/^timeout 0$/timeout 50/' config/bootloaders/isolinux/isolinux.cfg

# The local repository: each package in pool/<suite> of every suite it goes to, and hd-installer
# also in packages.chroot, so it is installed on the live system. squashfs stores a package that is
# in two suites once.
installer=""
for entry in ${DEBS}; do
    repos="${entry%%:*}"
    deb="${SRC}/${entry#*:}"
    if [[ ! -f "${deb}" ]]; then
        echo "Error: ${deb} does not exist" >&2
        exit 1
    fi

    IFS=, read -ra suites <<< "${repos}"
    for suite in "${suites[@]}"; do
        install -D -m 0644 "${deb}" "${REPO}/pool/${suite}/$(basename "${deb}")"
    done

    if [[ "$(basename "${deb}")" == hd-installer_* ]]; then
        installer="${deb}"
    fi
done

if [[ -z "${installer}" ]]; then
    echo "Error: DEBS has no package hd-installer" >&2
    exit 1
fi
mkdir -p config/packages.chroot
cp "${installer}" config/packages.chroot/

# The indices of each suite, with an unsigned Release file. The installer trusts the repository,
# as it comes with the installer itself.
for suite_dir in "${REPO}"/pool/*; do
    suite="$(basename "${suite_dir}")"
    mkdir -p "${REPO}/dists/${suite}/main/binary-amd64"
    (cd "${REPO}" && apt-ftparchive packages "pool/${suite}") \
        > "${REPO}/dists/${suite}/main/binary-amd64/Packages"
    (cd "${REPO}/dists/${suite}" && apt-ftparchive \
        -o APT::FTPArchive::Release::Origin=HyprDebian \
        -o APT::FTPArchive::Release::Label="HyprDebian live" \
        -o APT::FTPArchive::Release::Suite="${suite}" \
        -o APT::FTPArchive::Release::Codename="${suite}" \
        -o APT::FTPArchive::Release::Components=main \
        -o APT::FTPArchive::Release::Architectures=amd64 \
        release .) > "${BUILD}/Release"
    mv "${BUILD}/Release" "${REPO}/dists/${suite}/Release"
done

lb build

if [[ ! -f live-image-amd64.hybrid.iso ]]; then
    echo "Error: live-build did not make live-image-amd64.hybrid.iso" >&2
    exit 1
fi

install -m 0644 -o "${HOST_UID:-0}" -g "${HOST_GID:-0}" live-image-amd64.hybrid.iso "${OUT}/${ISO}"
echo "ISO written to ${OUT}/${ISO}"
