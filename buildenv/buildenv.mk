# The build images of HyprDebian, for the Makefile of every package directory that compiles, which
# includes this file. hd-build:<suite> is built from the Dockerfile next to this file, for the
# suites trixie and unstable; see docs/adr/0002-one-build-image-per-suite.md.
#
#   $(call build-image,<suite>)
#             build hd-build:<suite> with the latest image of the suite, then remove the build
#             images that the new one replaced. docker only installs the packages again when that
#             image or the Dockerfile has changed. Neither prints anything but errors
#   $(call in-build,<suite>,<docker run options>,<command>)
#             run the command in a container of hd-build:<suite>, which is removed afterwards
#
# Both are lines of a recipe. A Makefile builds the image of its suite before it runs a command in
# it, so make deb works in its package directory on its own.

# The directory of this file, of the Dockerfile.
BUILDENV_SRC := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_IMAGE := hd-build

define build-image
	docker build --pull --quiet --build-arg SUITE=$(1) -t $(BUILD_IMAGE):$(1) $(BUILDENV_SRC) > /dev/null
	docker image prune --force --filter label=org.hyprdebian.image=$(BUILD_IMAGE) > /dev/null
endef

define in-build
	docker run --rm $(2) $(BUILD_IMAGE):$(1) $(3)
endef
