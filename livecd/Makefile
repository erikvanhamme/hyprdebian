# Builds the HyprDebian live ISO in a container of hd-build:trixie, the build
# image of trixie from buildenv/Dockerfile.
#
#   make iso    build the build image of trixie, then the ISO in a container
#               of it, and write it to out/hyprdebian-live-amd64.iso. Run it
#               as make iso in the root of the repository, which builds the
#               packages first and hands them over in DEBS
#   make clean  remove out/
#
# DEBS holds the packages for the ISO, separated by spaces, each as
# <repos>:<path>, with the repos it goes to, separated by commas, and its
# path relative to the root of the repository, such as
# trixie,unstable:build/both/hd-base_1.10_all.deb.
#
# The container gets the repository read-only at /src and runs build.sh of
# it, which builds the ISO in /build in the filesystem of the container, with
# the live-build configuration in config/. Only the ISO is written back, to
# out/, owned by the user that runs make.
# live-build mounts filesystems and makes device nodes in its chroot, so the
# container runs with --privileged.

CONTAINER := hyprdebian-livecd-build
OUT := out
REPO := $(abspath ..)

include ../buildenv/buildenv.mk

.PHONY: iso clean

iso: | $(OUT)
	@if [ -z "$(DEBS)" ]; then \
		echo "Error: no packages in DEBS; run make iso in the root of the repository." >&2; \
		exit 1; \
	fi
	$(call build-image,trixie)
	$(call in-build,trixie, \
		--privileged --name $(CONTAINER) \
		-e HOST_UID="$$(id -u)" -e HOST_GID="$$(id -g)" -e DEBS="$(DEBS)" \
		-v "$(REPO):/src:ro" -v "$(abspath $(OUT)):/out" \
		-w /build, \
		/src/livecd/build.sh)

$(OUT):
	mkdir -p $@

clean:
	rm -rf $(OUT)
