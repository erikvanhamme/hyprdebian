#!/bin/bash
# Builds the HyprDebian live ISO. This is the entrypoint of the docker image
# of the Dockerfile next to it; run it with make iso (see the Makefile).
#
# It expects the repository read-only at /src and a folder for the ISO at
# /out, and builds in /build:
#   1. the package hd-installer, from a copy of /src/packages/hd-installer
#   2. the live-build configuration: lb config, then the files of
#      /src/livecd/config on top, with the package in packages.chroot
#   3. the ISO, which is copied to /out/hyprdebian-live-amd64.iso, owned by
#      HOST_UID and HOST_GID
set -euo pipefail

SRC=/src
OUT=/out
BUILD=/build
ISO=hyprdebian-live-amd64.iso

version=$(python3 -c 'import json, sys; v = json.load(open(sys.argv[1])); print("%d.%d" % (v["major"], v["minor"]))' \
    "${SRC}/version.json")
echo "=== HyprDebian live ${version} ==="

# The package hd-installer, built from a copy so that the checkout stays untouched.
cp -a "${SRC}/packages/hd-installer" "${BUILD}/hd-installer"
make -C "${BUILD}/hd-installer" clean deb
deb="${BUILD}/hd-installer/$(make -s --no-print-directory -C "${BUILD}/hd-installer" deb-path)"

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

mkdir -p config/packages.chroot
cp "${deb}" config/packages.chroot/

lb build

if [[ ! -f live-image-amd64.hybrid.iso ]]; then
    echo "Error: live-build did not make live-image-amd64.hybrid.iso" >&2
    exit 1
fi

install -m 0644 -o "${HOST_UID:-0}" -g "${HOST_GID:-0}" live-image-amd64.hybrid.iso "${OUT}/${ISO}"
echo "ISO written to ${OUT}/${ISO}"
