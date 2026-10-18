# Builds the package hd-art, which holds the hyprdebian wallpapers.
# It only installs the wallpapers; they are rendered elsewhere.
#
#   make deb    build build/hd-art_$(DEB_VERSION)_all.deb, with the version
#               from version.json. It installs:
#                 /usr/share/backgrounds/hyprdebian/emerald-*.png
#                                             the wallpapers in wallpapers/
#                 /usr/share/doc/hd-art/copyright
#                                             deb/copyright
#                 /usr/share/man/man7/hd-art.7.gz
#                                             the man page in man/
#               with the control file deb/control. Raise the version in
#               version.json for every package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-art
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
BACKGROUNDS := /usr/share/backgrounds/hyprdebian
DOC := /usr/share/doc/$(DEB_NAME)
WALLPAPERS := $(wildcard wallpapers/*.png)
# The man pages, named <page>.<section>.
MANPAGES := man/hd-art.7

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json deb/control deb/copyright $(WALLPAPERS) $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)$(BACKGROUNDS) $(STAGE)$(DOC)
	install -m 0644 $(WALLPAPERS) $(STAGE)$(BACKGROUNDS)/
	install -m 0644 deb/copyright $(STAGE)$(DOC)/copyright
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
