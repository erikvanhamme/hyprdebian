# Builds the package hd-config, which keeps the hyprdebian configuration in
# /etc/hyprdebian/config.json.
#
#   make deb    build build/hd-config_$(DEB_VERSION)_all.deb, with the version
#               from version.json. It installs:
#                 /usr/sbin/hd-config-update  adds a key to the
#                                             configuration or replaces its
#                                             value
#                 /usr/sbin/hd-config-remove  removes keys from the
#                                             configuration
#                 /usr/share/man/man7/hd-config.7.gz
#                 /usr/share/man/man8/hd-config-update.8.gz
#                 /usr/share/man/man8/hd-config-remove.8.gz
#                                             the man pages in man/
#               with the control file deb/control. Raise the version in
#               version.json for every package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-config
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
SCRIPTS := hd-config-update hd-config-remove
# The man pages, named <page>.<section>.
MANPAGES := man/hd-config.7 man/hd-config-update.8 man/hd-config-remove.8

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json $(SCRIPTS) deb/control $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/sbin
	install -m 0755 $(SCRIPTS) $(STAGE)/usr/sbin/
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
