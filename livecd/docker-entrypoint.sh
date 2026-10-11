#!/bin/bash
set -euo pipefail

# Build in the container's own filesystem so the chroot and caches never touch
# the host; only the finished ISO is copied out.
cd /build
bash /livecd/livecd.sh

iso=live-trixie-hyprdebian/live-image-amd64.hybrid.iso
if [ ! -f "$iso" ]; then
  echo "Error: live-build did not produce $iso" >&2
  exit 1
fi

install -m 644 -o "${HOST_UID:-0}" -g "${HOST_GID:-0}" \
  "$iso" /out/hyprdebian-live-amd64.iso
echo "ISO written to /out/hyprdebian-live-amd64.iso"
