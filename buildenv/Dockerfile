# The build images of HyprDebian, one per suite: hd-build:trixie and hd-build:unstable, built with
# --build-arg SUITE=trixie or unstable from debian:<suite>. buildenv.mk next to this file builds
# them and runs the builds in containers of them; see docs/adr/0002-one-build-image-per-suite.md.
# Add the build dependencies of a new package here, in the layer of the suite it is built on, see
# docs/HyprDebian/Build requirements.md.
#
# The first layer is shared: the build tools of the kernel, and the deb-src entries that apt
# source needs, also in contrib for zfs-linux and in the stanzas of the security updates. After it
# comes only the layer of the suite itself, so a change to the packages of one suite does not
# install the kernel tools again, nor touch the image of the other suite. The release of the
# kernels has the shared layer in its sha256, without comments, so a change to it builds new
# kernels, and a change to the layer of a suite does not.
#
# hd-buildserver builds this without arguments, so of trixie, as its build environment: it runs
# make iso in a container of it, with the checkout at the same path as on the host and the docker
# socket of the host, and the builds start their own containers with the docker of the host.

ARG SUITE=trixie

FROM debian:${SUITE} AS shared

LABEL org.hyprdebian.image=hd-build

RUN sed -i -e 's/^Types: deb$/Types: deb deb-src/' -e 's/^Components: main$/Components: main contrib/' \
        /etc/apt/sources.list.d/debian.sources \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential bc bison flex libssl-dev libelf-dev libdw-dev kmod cpio rsync python3 perl \
        dpkg-dev debhelper fakeroot xz-utils zstd ccache autoconf automake libtool gawk \
        rustc rust-src bindgen pahole ca-certificates \
    && rm -rf /var/lib/apt/lists/*

FROM shared AS trixie

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        clang lld gnu-efi wget unzip tar util-linux docker-cli docker-buildx \
    && rm -rf /var/lib/apt/lists/*

FROM shared AS unstable

# rustup of Debian conflicts with the rustc that the kernel is built with, so the toolchain comes
# from the installer of rustup, in /opt/rust and not in the PATH: the Rust builds source
# $CARGO_HOME/env and run rustup update stable, so they use the latest stable toolchain.
# wiremix needs libpipewire-0.3-dev, and libclang-dev for the bindings that bindgen makes of it.
ENV RUSTUP_HOME=/opt/rust/rustup CARGO_HOME=/opt/rust/cargo

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        git curl pkg-config libpipewire-0.3-dev libclang-dev \
    && rm -rf /var/lib/apt/lists/* \
    && curl --proto =https --tlsv1.2 -sSf https://sh.rustup.rs \
        | sh -s -- -y --no-modify-path --profile minimal --default-toolchain stable

FROM ${SUITE}
