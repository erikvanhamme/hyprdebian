# Builds every hyprdebian package and collects them in build/. The package
# directories are in packages/. A build server checks out this repository and
# runs make all.
#
#   make all        build every package in PACKAGES
#   make <package>  build one package directory, packages/<package>: make deb
#                   in it, then copy the packages it built to build/, each in
#                   place of older versions of that package
#   make test       run make test in every package directory in TESTS
#   make clean      remove build/ and run make clean in every package
#                   directory
#   make provision-vm
#                   ask for the name, zvol, cpus, memory, bridge and GPU of a
#                   vm, then define it in libvirt and start it from the live cd
#                   (vm/provision-vm.py); make the zvol first
#   make iso        build every package, then the live ISO with them in
#                   livecd/ (livecd/Makefile): hd-installer installed on the
#                   live system, and every package as the local repository
#                   that the installer installs the hd-* packages from, in
#                   the suites of the deploy.json of its directory
#   make update-repo
#                   build every package, then ask for the host name of a
#                   HyprDebian repository and upload the packages of every
#                   package directory in PACKAGES from build/ to it, each
#                   only for the repos in the deploy.json of its directory
#                   (update-repo/update-repo.py)
#
# Each package is a directory in packages/ with a Makefile that has the targets deb,
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

PACKAGES := hd-repo hd-art hd-fonts hd-config hd-network hd-secboot hd-bootloader hd-plymouth hd-dashboard hd-report hd-backup yazi wiremix ouch orcaslicer hd-screensaver hd-desktop hd-docker hd-qemu-kvm hd-cups hd-base hd-installer
# The package directories with tests.
TESTS := hd-dashboard hd-report hd-backup
# The directory with the package directories.
SRC := packages
OUT := build
# Prints the repos in the deploy.json given as argument, separated by commas.
DEPLOY_REPOS := python3 -c 'import json, sys; print(",".join(json.load(open(sys.argv[1]))["repos"]))'

.PHONY: all test clean iso provision-vm update-repo $(PACKAGES) $(addprefix test-,$(TESTS))

all: $(PACKAGES)

$(OUT):
	mkdir -p $@

$(PACKAGES): | $(OUT)
	$(MAKE) -C $(SRC)/$@ deb
	for deb in $$($(MAKE) -s --no-print-directory -C $(SRC)/$@ deb-path); do \
		name=$$(basename $$deb); \
		rm -f $(OUT)/$${name%%_*}_*.deb; \
		install -m 0644 $(SRC)/$@/$$deb $(OUT)/; \
	done

test: $(addprefix test-,$(TESTS))

$(addprefix test-,$(TESTS)):
	$(MAKE) -C $(SRC)/$(@:test-%=%) test

clean:
	rm -rf $(OUT)
	for package in $(PACKAGES); do $(MAKE) -C $(SRC)/$$package clean; done

provision-vm:
	python3 vm/provision-vm.py

# Prints each package of the package directories in PACKAGES as
# <repos>:build/<package>.deb, one per line, for update-repo.py and the live cd.
define LIST_DEBS
for package in $(PACKAGES); do \
	repos=$$($(DEPLOY_REPOS) $(SRC)/$$package/deploy.json) || exit 1; \
	for deb in $$($(MAKE) -s --no-print-directory -C $(SRC)/$$package deb-path); do \
		echo "$$repos:$(OUT)/$$(basename $$deb)"; \
	done; \
done
endef

# Hands the live cd every package, as <repos>:build/<package>.deb.
iso: all
	debs=$$($(LIST_DEBS)) || exit 1; \
	$(MAKE) -C livecd iso DEBS="$$(echo $$debs)"

# Hands update-repo.py each package as <repos>:build/<package>.deb.
update-repo: all
	uploads=$$($(LIST_DEBS)) || exit 1; \
	python3 update-repo/update-repo.py $$uploads
