# Builds every hyprdebian package and collects them in build/, by the repos
# they go to: build/both for trixie and unstable, build/trixie and
# build/unstable for one suite. The package directories are in packages/. A
# build server checks out this repository and runs make iso and make kernel.
#
#   make all        build every package in PACKAGES, one after the other; not
#                   the kernels, see make kernel
#   make <package>  build one package directory, packages/<package>: make deb
#                   in it, with JOBS jobs at the same time, then copy the
#                   packages it built to the folder of their repos in build/.
#                   Every package that no package directory builds now goes
#                   from build/: older versions, the kernels of an older
#                   release, and a package in the folder of repos it no longer
#                   goes to
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
#                   build/<suite>. Apart from make all, as they take long
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
#                   HyprDebian repository and upload every package in
#                   build/ to it, each only for the repos of its folder
#                   (update-repo/update-repo.py), so also the kernels that
#                   make kernel put there. It does not build the kernels: run
#                   make kernel first for new ones
#   make clean-repo ask for the host name of a HyprDebian repository and
#                   remove every package of every suite and the live ISO from
#                   it, after the host name is typed again, keeping the
#                   signing key (update-repo/update-repo.py --clean)
#   make list-debs  print every package in build/ as
#                   <repos>:build/<folder>/<package>.deb, one per line,
#                   without building anything: the arguments of
#                   update-repo.py, for a build that runs make iso and make
#                   kernel apart from the upload, as on a build server
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
# by deb-path as <repos>:<path>; it goes to the folder of those repos, in place
# of the repos of the deploy.json. This way a package directory can build a
# package of the same name for each suite, as hd-kernel does.
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
# The folders of build/: both for the packages of trixie and unstable, trixie
# and unstable for those of one suite.
DIRS := both trixie unstable
# The number of jobs of make deb in each package directory.
JOBS ?= $(shell nproc)
# Prints the repos in the deploy.json given as argument, separated by commas.
DEPLOY_REPOS := python3 -c 'import json, sys; print(",".join(json.load(open(sys.argv[1]))["repos"]))'

.PHONY: all kernel test clean iso provision-vm switch-vm-gpu update-repo clean-repo list-debs $(PACKAGES) $(KERNEL_PACKAGES) $(addprefix test-,$(TESTS))

all: $(PACKAGES)

kernel: $(KERNEL_PACKAGES)

$(OUT):
	mkdir -p $@

# Prints each package of the package directories in $(1) as
# <folder>:packages/<package directory>/<path>, without building it: the
# folder of build/ it goes to, from the repos that deb-path prints with it or
# else the deploy.json of its directory, and the package that deb builds.
define DEBS
for package in $(1); do \
	repos=$$($(DEPLOY_REPOS) $(SRC)/$$package/deploy.json) || exit 1; \
	for entry in $$($(MAKE) -s --no-print-directory -C $(SRC)/$$package deb-path); do \
		deb=$${entry#*:}; \
		dir=$$repos; \
		if [ "$$deb" != "$$entry" ]; then \
			dir=$${entry%%:*}; \
		fi; \
		case $$dir in \
			trixie,unstable|unstable,trixie) dir=both ;; \
			trixie|unstable) ;; \
			*) echo "Error: $$package goes to the repos $$dir, which have no folder in $(OUT)/" >&2; exit 1 ;; \
		esac; \
		echo "$$dir:$(SRC)/$$package/$$deb"; \
	done; \
done
endef

# The repos of the folder of build/ in the shell variable dir: both is trixie
# and unstable.
define FOLDER_REPOS
repos=$$dir; \
if [ "$$dir" = both ]; then \
	repos=trixie,unstable; \
fi
endef

# Removes every package from the folders of build/ that no package directory
# builds now, so they hold only the packages that make update-repo uploads.
define PRUNE
debs=$$($(call DEBS,$(PACKAGES) $(KERNEL_PACKAGES))) || exit 1; \
keep=" "; \
for entry in $$debs; do \
	keep="$${keep}$(OUT)/$${entry%%:*}/$$(basename $${entry#*:}) "; \
done; \
for deb in $(foreach dir,$(DIRS),$(OUT)/$(dir)/*.deb); do \
	if [ -f "$$deb" ]; then \
		case "$$keep" in \
			*" $$deb "*) ;; \
			*) rm -f "$$deb" ;; \
		esac; \
	fi; \
done
endef

# Copies the packages of the package directory to their folders in build/,
# then removes those that no package directory builds now, as the older
# kernels of hd-kernel, whose names change with every release.
$(PACKAGES) $(KERNEL_PACKAGES): | $(OUT)
	$(MAKE) -j$(JOBS) -C $(SRC)/$@ deb
	debs=$$($(call DEBS,$@)) || exit 1; \
	for entry in $$debs; do \
		mkdir -p $(OUT)/$${entry%%:*}; \
		install -m 0644 $${entry#*:} $(OUT)/$${entry%%:*}/; \
	done
	$(PRUNE)

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

# Prints each package in the folders of build/ as
# <repos>:build/<folder>/<package>.deb, for update-repo.py and make list-debs.
define BUILT_DEBS
for dir in $(DIRS); do \
	$(FOLDER_REPOS); \
	for deb in $(OUT)/$$dir/*.deb; do \
		if [ -f "$$deb" ]; then \
			echo "$$repos:$$deb"; \
		fi; \
	done; \
done
endef

# Hands the live cd the packages of the package directories in PACKAGES, as
# <repos>:build/<folder>/<package>.deb, so not the kernels that make kernel
# put in build/.
iso: all
	debs=$$($(call DEBS,$(PACKAGES))) || exit 1; \
	isodebs=""; \
	for entry in $$debs; do \
		dir=$${entry%%:*}; \
		$(FOLDER_REPOS); \
		isodebs="$$isodebs $$repos:$(OUT)/$$dir/$$(basename $${entry#*:})"; \
	done; \
	$(MAKE) -C livecd iso DEBS="$$(echo $$isodebs)"

# Hands update-repo.py every package in build/, also the kernels that make
# kernel built.
update-repo: all
	uploads=$$($(BUILT_DEBS)) || exit 1; \
	python3 update-repo/update-repo.py $$uploads

# Empties a repository; update-repo.py asks for the host name and to type it again.
clean-repo:
	python3 update-repo/update-repo.py --clean

# Prints every package in build/, without building it.
list-debs:
	@$(BUILT_DEBS)
