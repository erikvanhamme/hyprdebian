# Build environment for the hyprdebian live ISO.
#
# Build the image:
#   docker build -t hyprdebian-livecd livecd/
#
# Build the ISO (lands in livecd/out/):
#   mkdir -p livecd/out
#   docker run --rm --privileged \
#     -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
#     -v "$PWD/livecd/out:/out" hyprdebian-livecd
#
# live-build mounts filesystems and creates device nodes in its chroot, so the
# container needs --privileged.

FROM debian:trixie

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates \
      debootstrap \
      dosfstools \
      git \
      grub-efi-amd64-bin \
      grub-pc-bin \
      isolinux \
      live-build \
      mtools \
      squashfs-tools \
      sudo \
      syslinux \
      syslinux-efi \
      syslinux-utils \
      xorriso \
 && rm -rf /var/lib/apt/lists/*

COPY livecd.sh docker-entrypoint.sh /livecd/

WORKDIR /build
ENTRYPOINT ["/livecd/docker-entrypoint.sh"]
