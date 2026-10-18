# Builds the package hd-network, which renders the systemd-networkd files from
# the key hd-network in /etc/hyprdebian/config.json.
#
#   make deb    build build/hd-network_$(DEB_VERSION)_all.deb, with the version
#               from version.json. It installs:
#                 /usr/sbin/hd-network-render renders the files in
#                                             /etc/systemd/network
#                 /usr/sbin/hd-network-iface  adds, changes and removes
#                                             interfaces in the settings
#                 /usr/sbin/hd-network-bridge adds, changes and removes
#                                             bridges in the settings
#                 /usr/lib/hd-network/hd_network.py
#                                             lib/hd_network.py, the code
#                                             the three commands share
#                 /usr/share/hd-network/templates/*
#                                             the jinja2 templates in
#                                             templates/, rendered by
#                                             hd-network-render
#                 /usr/lib/systemd/network/10-ethernet.link
#                 /usr/lib/systemd/network/20-wifi.link
#                                             the files in link/, which name
#                                             interfaces by their path
#                 /usr/share/man/man7/hd-network.7.gz
#                 /usr/share/man/man8/hd-network-render.8.gz
#                 /usr/share/man/man8/hd-network-iface.8.gz
#                 /usr/share/man/man8/hd-network-bridge.8.gz
#                                             the man pages in man/
#               with the control file deb/control and deb/triggers, which
#               has initramfs-tools rebuild the initramfs, so the .link files
#               name the interfaces from the start of the boot. Raise the
#               version in version.json for every package that is handed
#               out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-network
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
LIB_DIR := /usr/lib/$(DEB_NAME)
TEMPLATE_DIR := /usr/share/$(DEB_NAME)/templates
SCRIPTS := hd-network-render hd-network-iface hd-network-bridge
LINK_DIR := /usr/lib/systemd/network
TEMPLATES := $(wildcard templates/*)
LINKS := $(wildcard link/*.link)
# The man pages, named <page>.<section>.
MANPAGES := man/hd-network.7 man/hd-network-render.8 man/hd-network-iface.8 \
	man/hd-network-bridge.8

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json $(SCRIPTS) lib/hd_network.py deb/control deb/triggers $(TEMPLATES) $(LINKS) \
		$(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/sbin $(STAGE)$(LIB_DIR) $(STAGE)$(TEMPLATE_DIR) \
		$(STAGE)$(LINK_DIR)
	install -m 0755 $(SCRIPTS) $(STAGE)/usr/sbin/
	install -m 0644 lib/hd_network.py $(STAGE)$(LIB_DIR)/
	install -m 0644 $(TEMPLATES) $(STAGE)$(TEMPLATE_DIR)/
	install -m 0644 $(LINKS) $(STAGE)$(LINK_DIR)/
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	install -m 0644 deb/triggers $(STAGE)/DEBIAN/
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
