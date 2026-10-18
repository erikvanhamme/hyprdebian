# Build environment for the HyprDebian live ISO: live-build, and apt-utils for
# the index of the local repository of the hd-* packages. The repository is mounted at /src when the
# container runs, so the image only has to be rebuilt when this file changes.
# See the Makefile next to this file.

FROM debian:trixie

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      apt-utils \
      ca-certificates \
      debootstrap \
      dosfstools \
      grub-efi-amd64-bin \
      grub-pc-bin \
      isolinux \
      live-build \
      mtools \
      python3 \
      squashfs-tools \
      syslinux \
      syslinux-efi \
      syslinux-utils \
      xorriso \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /build
ENTRYPOINT ["/src/livecd/build.sh"]
