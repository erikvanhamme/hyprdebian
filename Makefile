# Builds every hyprdebian package and collects them in build/. The package
# directories are in packages/. A build server checks out this repository and
# runs make iso and make kernel.
#
#   make all        build every package in PACKAGES, one after the other; not
#                   the kernels, see make kernel
#   make <package>  build one package directory, packages/<package>: make deb
#                   in it, with JOBS jobs at the same time, then copy the
#                   packages it built to build/, each in place of older
#                   versions of that package
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
#   make kernel     build the kernels, the package directories in
#                   KERNEL_PACKAGES, as make <package> does, into
#                   build/<suite>/. Apart from make all, as they take long
#                   to build; a suite whose kernel is built already is not
#                   built again, see hd-kernel
#   make iso        build every package, then the live ISO with them in
#                   livecd/ (livecd/Makefile): hd-installer installed on the
#                   live system, and every package as the local repository
#                   that the installer installs the hd-* packages from, in
#                   the suites of the deploy.json of its directory; not the
#                   kernels
#   make update-repo
#                   build every package, then ask for the host name of a
#                   HyprDebian repository and upload the packages of every
#                   package directory in PACKAGES from build/ to it, each
#                   only for the repos in the deploy.json of its directory,
#                   and the kernels that make kernel put in build/<suite>/,
#                   each only for its suite (update-repo/update-repo.py). It
#                   does not build the kernels: run make kernel first for
#                   new ones
#   make clean-repo ask for the host name of a HyprDebian repository and
#                   remove every package of every suite and the live ISO from
#                   it, after the host name is typed again, keeping the
#                   signing key (update-repo/update-repo.py --clean)
#   make list-debs  print every package of the package directories in
#                   PACKAGES as <repos>:build/<package>.deb, one per line,
#                   without building them: the arguments of update-repo.py
#                   and the DEBS of the live cd, for a build that runs make
#                   all and make iso apart from the upload, as on a build
#                   server. Then the kernels that make kernel put in
#                   build/<suite>/, as <suite>:build/<suite>/<package>.deb;
#                   none when it did not run, as their names come from the
#                   versions of Debian they are built of
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
# A package built for one repo only, such as a kernel of that suite, is printed
# by deb-path as <repos>:<path>; it goes to build/<repos>/ and only to those
# repos, in place of the repos of the deploy.json. This way a package directory
# can build a package of the same name for each suite, as hd-kernel does.
#
# Add every new package directory to PACKAGES, or to KERNEL_PACKAGES for a
# kernel, and to TESTS when its Makefile also has the target test, which runs
# the tests of the package.
#
# The packages are built one after the other, but make deb of each package
# directory runs with -j$(JOBS), so the targets within a package are built at
# the same time. JOBS is the number of cores by default; make all JOBS=1
# builds one target at a time.

PACKAGES := hd-repo hd-bootloader hd-dashboard hd-report hd-backup yazi wiremix ouch orcaslicer hd-audacious hd-desktop hd-docker hd-qemu-kvm hd-cups hd-buildserver hd-base hd-installer
# The package directories of the kernels, which make kernel builds apart from
# make all. They only go to the package repository, not in the live ISO: the
# installer does not install them, systems switch to them by hand for now.
KERNEL_PACKAGES := hd-kernel
# The package directories with tests.
TESTS := hd-dashboard hd-report hd-backup hd-base
# The directory with the package directories.
SRC := packages
OUT := build
# The number of jobs of make deb in each package directory.
JOBS ?= $(shell nproc)
# Prints the repos in the deploy.json given as argument, separated by commas.
DEPLOY_REPOS := python3 -c 'import json, sys; print(",".join(json.load(open(sys.argv[1]))["repos"]))'

.PHONY: all kernel test clean iso provision-vm switch-vm-gpu update-repo clean-repo list-debs $(PACKAGES) $(KERNEL_PACKAGES) $(addprefix test-,$(TESTS))

all: $(PACKAGES)

kernel: $(KERNEL_PACKAGES)

$(OUT):
	mkdir -p $@

# build/.<package>.debs lists the packages that the package directory put in
# build/ the last time. They are removed before the new ones go there, also
# when their names change, as those of hd-kernel do with every release.
$(PACKAGES) $(KERNEL_PACKAGES): | $(OUT)
	$(MAKE) -j$(JOBS) -C $(SRC)/$@ deb
	if [ -f $(OUT)/.$@.debs ]; then \
		xargs -r rm -f < $(OUT)/.$@.debs; \
		rm -f $(OUT)/.$@.debs; \
	fi
	for entry in $$($(MAKE) -s --no-print-directory -C $(SRC)/$@ deb-path); do \
		deb=$${entry#*:}; \
		dir=$(OUT); \
		if [ "$$deb" != "$$entry" ]; then \
			dir=$(OUT)/$${entry%%:*}; \
		fi; \
		mkdir -p $$dir; \
		name=$$(basename $$deb); \
		rm -f $$dir/$${name%%_*}_*.deb; \
		install -m 0644 $(SRC)/$@/$$deb $$dir/; \
		echo $$dir/$$name >> $(OUT)/.$@.debs; \
	done

test: $(addprefix test-,$(TESTS))

$(addprefix test-,$(TESTS)):
	$(MAKE) -C $(SRC)/$(@:test-%=%) test

clean:
	rm -rf $(OUT)
	for package in $(PACKAGES) $(KERNEL_PACKAGES); do $(MAKE) -C $(SRC)/$$package clean; done

provision-vm:
	python3 vm/provision-vm.py

switch-vm-gpu:
	python3 vm/switch-vm-gpu.py

# Prints each package of the package directories in $(1) as
# <repos>:build/<package>.deb, one per line, for update-repo.py, the live cd and
# make list-debs; a package that deb-path prints with its repos as
# <repos>:build/<repos>/<package>.deb.
define LIST_DEBS
for package in $(1); do \
	repos=$$($(DEPLOY_REPOS) $(SRC)/$$package/deploy.json) || exit 1; \
	for entry in $$($(MAKE) -s --no-print-directory -C $(SRC)/$$package deb-path); do \
		deb=$${entry#*:}; \
		if [ "$$deb" != "$$entry" ]; then \
			echo "$${entry%%:*}:$(OUT)/$${entry%%:*}/$$(basename $$deb)"; \
		else \
			echo "$$repos:$(OUT)/$$(basename $$deb)"; \
		fi; \
	done; \
done
endef

# Prints the kernels that make kernel put in build/<suite>/, from the lists
# build/.<package>.debs of the package directories in KERNEL_PACKAGES, as
# <suite>:build/<suite>/<package>.deb.
define KERNEL_DEBS
for package in $(KERNEL_PACKAGES); do \
	if [ -f $(OUT)/.$$package.debs ]; then \
		while read -r deb; do \
			if [ -f "$$deb" ]; then \
				suite=$${deb#$(OUT)/}; \
				echo "$${suite%%/*}:$$deb"; \
			fi; \
		done < $(OUT)/.$$package.debs; \
	fi; \
done
endef

# Hands the live cd every package, as <repos>:build/<package>.deb.
iso: all
	debs=$$($(call LIST_DEBS,$(PACKAGES))) || exit 1; \
	$(MAKE) -C livecd iso DEBS="$$(echo $$debs)"

# Hands update-repo.py each package as <repos>:build/<package>.deb, and the
# kernels that make kernel built.
update-repo: all
	uploads=$$($(call LIST_DEBS,$(PACKAGES)); $(KERNEL_DEBS)) || exit 1; \
	python3 update-repo/update-repo.py $$uploads

# Empties a repository; update-repo.py asks for the host name and to type it again.
clean-repo:
	python3 update-repo/update-repo.py --clean

# Prints each package as <repos>:build/<package>.deb, without building it, and
# the kernels that make kernel built.
list-debs:
	@$(call LIST_DEBS,$(PACKAGES)); $(KERNEL_DEBS)
