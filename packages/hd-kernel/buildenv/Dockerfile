# The build environment of the HyprDebian kernels: an image of the suite IMAGE, debian:trixie or
# debian:sid, with the build dependencies of build-kernel.sh, and the deb-src entries that apt
# source needs, also in contrib for zfs-linux and in the stanzas of the security updates.
# kernel.mk builds it as hd-kernel-buildenv:<suite> before every build, with --pull: docker only
# installs the packages again when the image of the suite has changed, and the build runs apt-get
# update for the sources itself.

ARG IMAGE=debian:sid
FROM ${IMAGE}

LABEL org.hyprdebian.image=hd-kernel-buildenv

RUN sed -i -e 's/^Types: deb$/Types: deb deb-src/' -e 's/^Components: main$/Components: main contrib/' \
        /etc/apt/sources.list.d/debian.sources \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential bc bison flex libssl-dev libelf-dev libdw-dev kmod cpio rsync python3 perl \
        dpkg-dev debhelper fakeroot xz-utils zstd ccache autoconf automake libtool gawk \
        rustc rust-src bindgen pahole ca-certificates \
    && rm -rf /var/lib/apt/lists/*
