# Builds every hyprdebian package and collects them in packages/. A build
# server checks out this repository and runs make all.
#
#   make all        build every package in PACKAGES
#   make <package>  build one package directory: make deb in it, then copy
#                   the packages it built to packages/, each in place of
#                   older versions of that package
#   make test       run make test in every package directory in TESTS
#   make clean      remove packages/ and run make clean in every package
#                   directory
#   make provision-vm
#                   ask for the name, zvol, cpus, memory, bridge and GPU of a
#                   vm, then define it in libvirt and start it from the live cd
#                   (vm/provision-vm.py); make the zvol first
#   make update-repo
#                   build every package, then ask for the host name of a
#                   HyprDebian repository and upload the packages of every
#                   package directory in PACKAGES from packages/ to it, each
#                   only for the repos in the deploy.json of its directory
#                   (update-repo/update-repo.py)
#
# Each package is a directory with a Makefile that has the targets deb,
# deb-path, which prints the paths of the packages that deb builds, one or
# more named <name>_<version>_<arch>.deb, and clean. It also has a deploy.json
# with the repos its packages go to, trixie, unstable or both:
#
#   {
#       "repos" : ["trixie", "unstable"]
#   }
#
# Add every new package directory to PACKAGES, and to TESTS when its Makefile
# also has the target test, which runs the tests of the package.

PACKAGES := hd-repo hd-art hd-fonts hd-config hd-network hd-secboot hd-bootloader hd-dashboard hd-report hd-backup yazi wiremix ouch orcaslicer hd-screensaver hd-desktop hd-docker hd-qemu-kvm hd-cups hd-base hd-installer
# The package directories with tests.
TESTS := hd-dashboard hd-report hd-backup
OUT := packages
# Prints the repos in the deploy.json given as argument, separated by commas.
DEPLOY_REPOS := python3 -c 'import json, sys; print(",".join(json.load(open(sys.argv[1]))["repos"]))'

.PHONY: all test clean provision-vm update-repo $(PACKAGES) $(addprefix test-,$(TESTS))

all: $(PACKAGES)

$(OUT):
	mkdir -p $@

$(PACKAGES): | $(OUT)
	$(MAKE) -C $@ deb
	for deb in $$($(MAKE) -s --no-print-directory -C $@ deb-path); do \
		name=$$(basename $$deb); \
		rm -f $(OUT)/$${name%%_*}_*.deb; \
		install -m 0644 $@/$$deb $(OUT)/; \
	done

test: $(addprefix test-,$(TESTS))

$(addprefix test-,$(TESTS)):
	$(MAKE) -C $(@:test-%=%) test

clean:
	rm -rf $(OUT)
	for package in $(PACKAGES); do $(MAKE) -C $$package clean; done

provision-vm:
	python3 vm/provision-vm.py

# Hands update-repo.py each package as <repos>:packages/<package>.deb.
update-repo: all
	uploads=; \
	for package in $(PACKAGES); do \
		repos=$$($(DEPLOY_REPOS) $$package/deploy.json) || exit 1; \
		for deb in $$($(MAKE) -s --no-print-directory -C $$package deb-path); do \
			uploads="$$uploads $$repos:$(OUT)/$$(basename $$deb)"; \
		done; \
	done; \
	python3 update-repo/update-repo.py $$uploads
