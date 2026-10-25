# Builds the HyprDebian kernel: the kernel of Debian, with its patches and its
# config, and the ZFS module of the zfs-linux of Debian built against it and
# signed with its key, so a system needs no compiler and no DKMS for ZFS.
# Each suite in SUITES gets a kernel of its own, built from the most recent
# linux and zfs-linux of that suite in a container of it, so the build needs
# docker and a user that may run it. build-kernel.sh does the work, see there.
#
#   make deb    build the packages of every suite, into build/<suite>:
#                 linux-image-<release>_<version>_amd64.deb
#                               the kernel and its modules
#                 zfs-modules-<release>_<version>_amd64.deb
#                               zfs.ko and spl.ko, in
#                               /lib/modules/<release>/extra/zfs
#                 hd-kernel_<version>_amd64.deb
#                               depends on both, with the man page in man/;
#                               installing it switches a system to the
#                               kernel of HyprDebian
#               with the control files and maintainer scripts in deb/. The
#               release is <x.y.z>+debNN-hd<id>-amd64, <id> from the versions
#               of linux, zfs-linux and hd-kernel; the versions of the
#               packages are those of linux and zfs-linux with +hd and the
#               version of version.json. Raise it for every change of the
#               packages. The release also changes with build-kernel.sh and
#               the files in deb/ and man/, so it says what the packages are
#               made of: a suite whose packages of that release are in
#               build/<suite> already is not built again, until make clean
#               removes them. That costs a container start and an apt update
#               per suite. With HD_KERNEL_CACHE set to a directory, a suite
#               whose release is already built there is copied from it
#               instead of built, and every build is stored there.
#   make deb-<suite>
#               build the packages of one suite
#   make deb-path
#               print the packages that make deb built, as
#               <suite>:build/<suite>/<package>.deb, for the Makefile in the
#               root, which puts each only in the repository of its suite;
#               nothing for a suite that is not built
#   make sources
#               print the versions of linux and zfs-linux that make deb would
#               build, a line per suite, as
#               "<suite> linux <version> zfs-linux <version>"; run it with
#               make -s, so make does not print the commands
#   make test-build
#               build the packages of unstable quicker, into build/test: with
#               only the modules in LSMOD (the output of lsmod on the test vm
#               by default, see make localmodconfig) and without debug info;
#               the release has hdtest<id>. Only for trying out on the test
#               vm, never for a repository.
#   make clean  remove build/
#
# The kernel trees stay in build/work/<suite>, with the ccache of the
# compiler, so a build of the same versions only compiles what changed. The
# container gets at most MEMORY of memory, 5/8 GB per job, so that when the
# compilers need more, the kernel kills the build and not other programs; set
# JOBS lower for a machine with less memory free.

BUILD := build
PYTHON := python3

SUITES := trixie unstable
# The docker image of each suite.
IMAGE_trixie := debian:trixie
IMAGE_unstable := debian:sid
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
# directory $(BUILD)/work/$(3) and the packages into $(BUILD)/$(3). It pulls
# the latest image of the suite every time.
define run
	mkdir -p $(BUILD)/work/$(3) $(BUILD)/$(3)
	docker run --rm --pull always --quiet \
		--memory $(MEMORY) --memory-swap $(MEMORY) \
		-e MODE=$(1) \
		-e SUITE=$(2) \
		-e HD_VERSION=$(HD_VERSION) \
		-e JOBS=$(JOBS) \
		-e HOST_UID=$$(id -u) \
		-e HOST_GID=$$(id -g) \
		-v $(CURDIR):/src:ro \
		-v $(abspath $(LSMOD)):/lsmod:ro \
		-v $(CURDIR)/$(BUILD)/work/$(3):/work \
		-v $(CURDIR)/$(BUILD)/$(3):/out \
		$(if $(HD_KERNEL_CACHE),-v $(abspath $(HD_KERNEL_CACHE)):/cache) \
		$(IMAGE_$(2)) bash /src/build-kernel.sh
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
