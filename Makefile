# Builds every hyprdebian package and collects them in packages/. A build
# server checks out this repository and runs make all.
#
#   make all        build every package in PACKAGES
#   make <package>  build one package directory: make deb in it, then copy
#                   the packages it built to packages/, each in place of
#                   older versions of that package
#   make clean      remove packages/ and run make clean in every package
#                   directory
#
# Each package is a directory with a Makefile that has the targets deb,
# deb-path, which prints the paths of the packages that deb builds, one or
# more named <name>_<version>_<arch>.deb, and clean. Add every new package
# directory to PACKAGES.

PACKAGES := hd-repo hd-art hd-fonts hd-config hd-network hd-secboot hd-bootloader hd-base
OUT := packages

.PHONY: all clean $(PACKAGES)

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

clean:
	rm -rf $(OUT)
	for package in $(PACKAGES); do $(MAKE) -C $$package clean; done
