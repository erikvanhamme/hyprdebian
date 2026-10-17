# Builds the HyprDebian live ISO in a debian:trixie docker container.
#
#   make iso    build the docker image, then the ISO in a container, and
#               write it to out/hyprdebian-live-amd64.iso
#   make image  only build the docker image
#   make clean  remove out/
#
# The container gets the repository read-only at /src and builds everything
# in its own filesystem: the package hd-installer from ../hd-installer, then
# the ISO with the live-build configuration in config/ (see build.sh). Only
# the ISO is written back, to out/, owned by the user that runs make.
# live-build mounts filesystems and makes device nodes in its chroot, so the
# container runs with --privileged.

IMAGE := hyprdebian-livecd
CONTAINER := hyprdebian-livecd-build
OUT := out
REPO := $(abspath ..)

.PHONY: iso image clean

iso: image | $(OUT)
	docker run --rm --privileged --name $(CONTAINER) \
		-e HOST_UID="$$(id -u)" -e HOST_GID="$$(id -g)" \
		-v "$(REPO):/src:ro" -v "$(abspath $(OUT)):/out" \
		$(IMAGE)

image:
	docker build -t $(IMAGE) .

$(OUT):
	mkdir -p $@

clean:
	rm -rf $(OUT)
