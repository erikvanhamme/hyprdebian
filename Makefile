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
#                   ask for the name, zvol, cpus, memory, bridge and GPU (or
#                   none) of a vm, then define it in libvirt and start it from
#                   the live cd (vm/provision-vm.py); make the zvol first
#   make switch-vm-gpu
#                   ask for a vm in libvirt and switch it between running with
#                   a GPU (3D acceleration) and without one (the default
#                   graphics), asking for the GPU (vm/switch-vm-gpu.py)
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
#   make list-debs  print every package of the package directories in
#                   PACKAGES as <repos>:build/<package>.deb, one per line,
#                   without building them: the arguments of update-repo.py
#                   and the DEBS of the live cd, for a build that runs make
#                   all and make iso apart from the upload, as on a build
#                   server
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

PACKAGES := hd-repo hd-bootloader hd-dashboard hd-report hd-backup yazi wiremix ouch orcaslicer hd-audacious hd-desktop hd-docker hd-qemu-kvm hd-cups hd-buildserver hd-base hd-installer
# The package directories with tests.
TESTS := hd-dashboard hd-report hd-backup hd-base
# The directory with the package directories.
SRC := packages
OUT := build
# Prints the repos in the deploy.json given as argument, separated by commas.
DEPLOY_REPOS := python3 -c 'import json, sys; print(",".join(json.load(open(sys.argv[1]))["repos"]))'

.PHONY: all test clean iso provision-vm switch-vm-gpu update-repo list-debs $(PACKAGES) $(addprefix test-,$(TESTS))

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

switch-vm-gpu:
	python3 vm/switch-vm-gpu.py

# Prints each package of the package directories in PACKAGES as
# <repos>:build/<package>.deb, one per line, for update-repo.py, the live cd and
# make list-debs.
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

# Prints each package as <repos>:build/<package>.deb, without building it.
list-debs:
	@$(LIST_DEBS)
