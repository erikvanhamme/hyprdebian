# Builds the package hd-installer, which installs HyprDebian from a live
# system.
#
#   make deb    build build/hd-installer_$(DEB_VERSION)_all.deb, with the
#               version from version.json. It installs:
#                 /usr/lib/hd-installer/hd-installer
#                                             the installer
#                 /usr/lib/hd-installer/scripts/*.sh
#                                             its helpers and phases, in
#                                             scripts/
#                 /usr/lib/hd-installer/version.json
#                                             its version, for the banner
#                 /usr/lib/hd-installer/render.py
#                 /usr/lib/hd-installer/templates/
#                                             the jinja2 templates of the
#                                             files it writes in the new
#                                             system, and their renderer
#                 /usr/lib/hd-installer/deploy/
#                                             the files it copies into the
#                                             new system
#                 /usr/sbin/hd-installer      a link to the installer
#                 /usr/share/man/man8/hd-installer.8.gz
#                                             the man page in man/
#               with the control file deb/control. Raise the version in
#               version.json for every package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-installer
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
LIB_DIR := /usr/lib/$(DEB_NAME)
SCRIPTS := $(wildcard scripts/*.sh)
# The templates and the files for the new system, by their path below
# templates/ and deploy/.
TEMPLATES := $(shell find templates -type f)
DEPLOY := $(shell find deploy -type f)
# The man pages, named <page>.<section>.
MANPAGES := man/hd-installer.8

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. The installer finds
# scripts/ and version.json next to itself, also through the link. dpkg-deb
# makes all files owned by root.
$(DEB): version.json hd-installer render.py $(SCRIPTS) $(TEMPLATES) $(DEPLOY) deb/control \
		$(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)$(LIB_DIR)/scripts $(STAGE)/usr/sbin
	install -m 0755 hd-installer $(STAGE)$(LIB_DIR)/
	install -m 0644 $(SCRIPTS) $(STAGE)$(LIB_DIR)/scripts/
	install -m 0644 version.json render.py $(STAGE)$(LIB_DIR)/
	for file in $(TEMPLATES) $(DEPLOY); do \
		install -D -m 0644 $$file $(STAGE)$(LIB_DIR)/$$file; \
	done
	ln -s ../lib/$(DEB_NAME)/hd-installer $(STAGE)/usr/sbin/hd-installer
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
