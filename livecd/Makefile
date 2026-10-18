# Builds the HyprDebian live ISO in a debian:trixie docker container.
#
#   make iso    build the docker image, then the ISO in a container, and
#               write it to out/hyprdebian-live-amd64.iso. Run it as make iso
#               in the root of the repository, which builds the packages
#               first and hands them over in DEBS
#   make image  only build the docker image
#   make clean  remove out/
#
# DEBS holds the packages for the ISO, separated by spaces, each as
# <repos>:<path>, with the repos it goes to, separated by commas, and its
# path relative to the root of the repository, such as
# trixie,unstable:build/hd-base_1.10_all.deb.
#
# The container gets the repository read-only at /src and builds the ISO in
# its own filesystem, with the live-build configuration in config/ (see
# build.sh). Only the ISO is written back, to out/, owned by the user that
# runs make.
# live-build mounts filesystems and makes device nodes in its chroot, so the
# container runs with --privileged.

IMAGE := hyprdebian-livecd
CONTAINER := hyprdebian-livecd-build
OUT := out
REPO := $(abspath ..)

.PHONY: iso image clean

iso: image | $(OUT)
	@if [ -z "$(DEBS)" ]; then \
		echo "Error: no packages in DEBS; run make iso in the root of the repository." >&2; \
		exit 1; \
	fi
	docker run --rm --privileged --name $(CONTAINER) \
		-e HOST_UID="$$(id -u)" -e HOST_GID="$$(id -g)" -e DEBS="$(DEBS)" \
		-v "$(REPO):/src:ro" -v "$(abspath $(OUT)):/out" \
		$(IMAGE)

image:
	docker build -t $(IMAGE) .

$(OUT):
	mkdir -p $@

clean:
	rm -rf $(OUT)
