# The clean build environment of hd-buildserver: a debian:trixie image with everything make all
# and make iso need, see docs/HyprDebian/Build requirements.md. Add the build dependencies of a
# new package here too, so the build server gets them with the package.
#
# hd-buildserver runs make iso in a container of this image, with the checkout at the same path
# as on the host and the docker socket of the host. The builds that run in containers of their
# own, the Rust packages, hd-desktop, hd-audacious and the live cd, start them with the docker of
# the host, and the paths they hand it are the same on the host.
#
# Built on trixie, so what is compiled outside a container of its own, such as blockwrite of
# hd-backup, also runs on trixie.

FROM debian:trixie

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential python3 clang lld gnu-efi wget unzip ca-certificates tar util-linux \
        docker-cli docker-buildx \
    && rm -rf /var/lib/apt/lists/*
