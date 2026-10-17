# Builds every hyprdebian package and collects them in packages/. A build
# server checks out this repository and runs make all.
#
#   make all        build every package in PACKAGES
#   make <package>  build one package: make deb in its directory, then copy
#                   the package to packages/, in place of older versions of
#                   that package
#   make clean      remove packages/ and run make clean in every package
#                   directory
#
# Each package is a directory with a Makefile that has the targets deb,
# deb-path, which prints the path of the package that deb builds, and clean.
# Add every new package directory to PACKAGES.

PACKAGES := hd-repo hd-art hd-fonts hd-config
OUT := packages

.PHONY: all clean $(PACKAGES)

all: $(PACKAGES)

$(OUT):
	mkdir -p $@

$(PACKAGES): | $(OUT)
	$(MAKE) -C $@ deb
	rm -f $(OUT)/$@_*.deb
	install -m 0644 $@/$$($(MAKE) -s --no-print-directory -C $@ deb-path) $(OUT)/

clean:
	rm -rf $(OUT)
	for package in $(PACKAGES); do $(MAKE) -C $$package clean; done
