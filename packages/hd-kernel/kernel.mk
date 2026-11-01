# The build of a HyprDebian kernel, for the Makefile of the package directory of a kernel, which
# sets these and then includes this file:
#
#   PACKAGE   the package that depends on the kernel and its ZFS module; the package directory
#             has its control file in deb/$(PACKAGE)/control and its man page in
#             man/$(PACKAGE).7
#   TAG       what the release of the kernel has in front of its id:
#             <x.y.z>+debNN-$(TAG)<id>-amd64
#   KEEP      the file of the package directory with the modules that the kernel keeps of the
#             config of Debian, see build-kernel.sh; empty to keep them all
#
# The package directory also has the version of its packages in version.json. build-kernel.sh,
# next to this file, does the work, with the templates of the package of the ZFS module in
# deb/zfs-modules next to it. The Makefile of hd-kernel describes the targets.

# The directory of this file, of build-kernel.sh.
KERNEL_SRC := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD := build
PYTHON := python3

SUITES := trixie unstable
# The docker image of each suite, which the build environment of the suite,
# hd-kernel-buildenv:<suite> of buildenv/Dockerfile, is built on.
IMAGE_trixie := debian:trixie
IMAGE_unstable := debian:sid
BUILDENV := hd-kernel-buildenv
HD_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
# The modules of the test build, as lsmod prints them.
LSMOD := ../../local-testvm/lsmod-test.txt
JOBS := $(shell nproc)
MEMORY := $(shell echo $$(( $(JOBS) * 5 / 8 )))g
HD_KERNEL_CACHE ?=

.PHONY: deb deb-path sources test-build clean $(addprefix deb-,$(SUITES)) $(addprefix sources-,$(SUITES))
# One suite at a time, also under make -j of the Makefile in the root: each
# build already runs JOBS jobs and takes up to MEMORY.
.NOTPARALLEL:

deb: $(addprefix deb-,$(SUITES))

# Runs build-kernel.sh in the mode $(1) for the suite $(2), with the work
# directory $(BUILD)/work/$(3) and the packages into $(BUILD)/$(3), in a
# container of the build environment of the suite. It builds that first, with
# the latest image of the suite: docker only installs the build dependencies
# again when that image has changed. Then it removes the build environments
# that the new one replaced. Neither prints anything but errors, as make
# sources prints only the versions.
define run
	mkdir -p $(BUILD)/work/$(3) $(BUILD)/$(3)
	docker build --pull --quiet --build-arg IMAGE=$(IMAGE_$(2)) -t $(BUILDENV):$(2) \
		$(KERNEL_SRC)/buildenv > /dev/null
	docker image prune --force --filter label=org.hyprdebian.image=$(BUILDENV) > /dev/null
	docker run --rm \
		--memory $(MEMORY) --memory-swap $(MEMORY) \
		-e MODE=$(1) \
		-e SUITE=$(2) \
		-e PACKAGE=$(PACKAGE) \
		-e TAG=$(TAG) \
		-e KEEP=$(KEEP) \
		-e HD_VERSION=$(HD_VERSION) \
		-e JOBS=$(JOBS) \
		-e HOST_UID=$$(id -u) \
		-e HOST_GID=$$(id -g) \
		-v $(KERNEL_SRC):/src:ro \
		-v $(CURDIR):/package:ro \
		-v $(abspath $(LSMOD)):/lsmod:ro \
		-v $(CURDIR)/$(BUILD)/work/$(3):/work \
		-v $(CURDIR)/$(BUILD)/$(3):/out \
		$(if $(HD_KERNEL_CACHE),-v $(abspath $(HD_KERNEL_CACHE)):/cache) \
		$(BUILDENV):$(2) bash /src/build-kernel.sh
endef

$(addprefix deb-,$(SUITES)): deb-%:
	$(if $(HD_KERNEL_CACHE),mkdir -p $(HD_KERNEL_CACHE))
	$(call run,full,$*,$*)

deb-path:
	@for suite in $(SUITES); do \
		for deb in $(BUILD)/$$suite/*.deb; do \
			if [ -f "$$deb" ]; then \
				echo "$$suite:$$deb"; \
			fi; \
		done; \
	done

sources: $(addprefix sources-,$(SUITES))

$(addprefix sources-,$(SUITES)): sources-%:
	$(call run,sources,$*,$*)

test-build:
	$(call run,test,unstable,test)

clean:
	rm -rf $(BUILD)
