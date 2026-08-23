#!/bin/bash

# Make the working folder
mkdir live-trixie-hyprdebian && cd live-trixie-hyprdebian


# Configure live build.
lb config \
  --distribution trixie \
  --archive-areas "main contrib non-free non-free-firmware" \
  --apt-options "--yes" \
  --bootloaders "grub-efi syslinux" \
  --debian-installer none \
  --binary-images iso-hybrid


# Ensure ZFS support is baked in
cat <<'EOF' > config/package-lists/zfs.list.chroot
task-gnome-desktop
build-essential
linux-headers-amd64
zfs-dkms
zfsutils-linux
EOF


# Add gparted, git and other required tools
cat <<'EOF' > config/package-lists/hyprdebian.list.chroot
debootstrap
gparted
gdisk
git
python3-jinja2
EOF


# Make a checkout of hyprdebian installer in the skel folder
mkdir -p config/includes.chroot/etc/skel/
cd config/includes.chroot/etc/skel/
git clone https://github.com/erikvanhamme/hyprdebian.git
cd ../../../..


# OS identifier
cat <<'EOF' > config/includes.chroot/etc/skel/hyprdebian.release
hyprdebian
EOF


# Make sure ZFS module is built
mkdir -p config/hooks/live

cat <<'EOF' > config/hooks/live/0100-build-zfs-dkms.hook.chroot
#!/bin/sh
set -e

# Detect installed kernel version inside the chroot
KERNEL_VERSION=$(ls /lib/modules | head -n 1)

if [ -n "$KERNEL_VERSION" ]; then
  echo "Building ZFS DKMS modules for kernel: $KERNEL_VERSION"
  dkms autoinstall -k "$KERNEL_VERSION" || dkms build -m zfs -v $(dpkg-query -W -f='${Version}\n' zfs-dkms | cut -d: -f2 | cut -d- -f1) -k "$KERNEL_VERSION"
  depmod -a "$KERNEL_VERSION"
else
  echo "Error: No kernel headers/modules found in chroot." >&2
  exit 1
fi
EOF

chmod +x config/hooks/live/0100-build-zfs-dkms.hook.chroot


# Make sure ZFS module is loaded.
mkdir -p config/includes.chroot/etc/modules-load.d/

cat <<'EOF' > config/includes.chroot/etc/modules-load.d/zfs.conf
zfs
EOF


# Build the live cd.
sudo lb build


# Make it bootable.
sudo isohybrid --uefi live-image-amd64.hybrid.iso


