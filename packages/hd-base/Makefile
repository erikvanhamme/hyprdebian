# Builds the package hd-base, which depends on the packages that every
# hyprdebian system has, and holds the configuration database, the network
# configuration and the Secure Boot keys of every system.
#
#   make deb    build build/hd-base_$(DEB_VERSION)_all.deb, with the version
#               from ../../version.json, the version of HyprDebian. It
#               installs:
#                 /etc/hyprdebian/version.json
#                                             ../../version.json, which the
#                                             banners show, a conffile
#                 /usr/bin/hd-up              upgrades the system after a
#                                             ZFS snapshot, by hand and
#                                             every week
#                 /usr/bin/hd-reboot-check    tells whether a reboot is
#                                             needed, and writes
#                                             /run/reboot-required
#                 /usr/bin/hd-reboot
#                 /usr/bin/hd-poweroff        reboot or power off, with
#                                             hd-shutdown of hd-desktop in
#                                             a Hyprland session,
#                                             otherwise with systemctl
#                 /usr/bin/hd-sync-skel       brings the files of /etc/skel
#                                             into the home directory
#                 /usr/bin/hd-config-update   adds a key to the
#                                             configuration or replaces its
#                                             value: of the system for root,
#                                             of the user for other users
#                 /usr/bin/hd-config-remove   removes keys from that
#                                             configuration
#                 /usr/sbin/hd-issue          writes the login banner in
#                                             /etc/issue
#                 /usr/sbin/hd-link-repo      switches the HyprDebian
#                                             repository and suite of apt
#                 /usr/sbin/hd-network-render renders the files in
#                                             /etc/systemd/network
#                 /usr/sbin/hd-network-iface  adds, changes and removes
#                                             interfaces in the settings
#                 /usr/sbin/hd-network-bridge adds, changes and removes
#                                             bridges in the settings
#                 /usr/sbin/hd-secboot        generates the Secure Boot keys,
#                                             writes them in the firmware and
#                                             turns Secure Boot on and off
#                 /usr/sbin/hd-firmware       finds and installs the
#                                             firmware packages that the
#                                             hardware needs
#                 /usr/share/hd-base/templates/*
#                                             the jinja2 templates in
#                                             templates/, rendered by
#                                             hd-link-repo
#                 /usr/share/hd-network/templates/*
#                                             the jinja2 templates in
#                                             templates/hd-network/,
#                                             rendered by hd-network-render
#                 /usr/lib/hd-network/hd_network.py
#                                             lib/hd_network.py, the code
#                                             the hd-network commands share
#                 /etc/...                    the files in etc/, at the
#                                             same path and with the same
#                                             mode, conffiles: the files
#                                             new users get in /etc/skel,
#                                             the PATH of /etc/profile.d,
#                                             the apt preferences, the apt
#                                             config without translations
#                                             and without recommends,
#                                             the files dpkg leaves out,
#                                             the message after a login in
#                                             /etc/update-motd.d and the
#                                             kernel hook that writes
#                                             /run/reboot-required
#                 /usr/...                    the files in usr/, at the
#                                             same path: the Num Lock
#                                             drop-in of getty, the timers
#                                             that scrub and trim the ZFS
#                                             pools and upgrade the
#                                             system, the zram
#                                             swap, the sysctls for it,
#                                             the resolved config, and the
#                                             .link files that name network
#                                             interfaces by their path
#                 /usr/share/man/man*/*.gz    the man pages in man/
#               with the control file deb/control, whose Depends are the
#               packages of the base system, the maintainer scripts
#               deb/postinst, which masks the units HyprDebian does not use,
#               points /etc/resolv.conf at the name servers, empties
#               /etc/motd, diverts 10-uname and /etc/issue of base-files,
#               writes the login banner with hd-issue, generates the
#               Secure Boot keys in /etc/hyprdebian/secboot when they are not
#               there yet, removes the files that dpkg leaves out from the
#               packages installed before it and enables the timers,
#               deb/prerm, which stops the timers on a removal, deb/postrm,
#               which undoes the diversions and masks the timers on a
#               removal and unmasks the units on a purge, and deb/triggers,
#               which has initramfs-tools rebuild the initramfs, so the .link
#               files name the interfaces from the start of the boot. Add a
#               package to deb/control, to the man page and to
#               docs/packages/hd-base.md when every system must have it.
#               Raise the version in ../../version.json for every change to a
#               package or the installer.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make test   run the unittest tests in tests/, which need Python only
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-base
# The version of HyprDebian, in the root of the repository, is the version of hd-base.
VERSION_FILE := ../../version.json
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("$(VERSION_FILE)")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
# The files of the system, by their path below etc/ and usr/.
ETC := $(shell find etc -type f)
USR := $(shell find usr -type f)
# The commands, installed in /usr/bin.
COMMANDS := hd-up hd-reboot-check hd-reboot hd-poweroff hd-sync-skel hd-config-update hd-config-remove
# The commands for root, installed in /usr/sbin.
SBIN_COMMANDS := hd-issue hd-link-repo hd-network-render \
	hd-network-iface hd-network-bridge hd-secboot hd-firmware
# The jinja2 templates of hd-link-repo.
TEMPLATE_DIR := /usr/share/$(DEB_NAME)/templates
TEMPLATES := $(wildcard templates/*.j2)
# The jinja2 templates of hd-network-render, and the code the hd-network commands share.
NETWORK_TEMPLATE_DIR := /usr/share/hd-network/templates
NETWORK_TEMPLATES := $(wildcard templates/hd-network/*.j2)
NETWORK_LIB_DIR := /usr/lib/hd-network
# The man pages, named <page>.<section>. hd-poweroff.1 includes hd-reboot.1.
MANPAGES := man/hd-base.7 man/hd-up.1 man/hd-reboot-check.1 man/hd-reboot.1 man/hd-poweroff.1 man/hd-sync-skel.1 \
	man/hd-issue.8 man/hd-link-repo.8 man/hd-firmware.8 \
	man/hd-config.7 man/hd-config-update.1 man/hd-config-remove.1 \
	man/hd-network.7 man/hd-network-render.8 man/hd-network-iface.8 man/hd-network-bridge.8 \
	man/hd-secboot.7 man/hd-secboot.8

.PHONY: deb deb-path test clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): $(VERSION_FILE) $(COMMANDS) $(SBIN_COMMANDS) $(TEMPLATES) $(NETWORK_TEMPLATES) \
		lib/hd_network.py $(ETC) $(USR) deb/control deb/postinst deb/prerm deb/postrm deb/triggers \
		$(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)/usr/sbin $(STAGE)$(TEMPLATE_DIR) \
		$(STAGE)$(NETWORK_TEMPLATE_DIR) $(STAGE)$(NETWORK_LIB_DIR)
	install -m 0755 $(COMMANDS) $(STAGE)/usr/bin/
	install -m 0755 $(SBIN_COMMANDS) $(STAGE)/usr/sbin/
	install -m 0644 $(TEMPLATES) $(STAGE)$(TEMPLATE_DIR)/
	install -m 0644 $(NETWORK_TEMPLATES) $(STAGE)$(NETWORK_TEMPLATE_DIR)/
	install -m 0644 lib/hd_network.py $(STAGE)$(NETWORK_LIB_DIR)/
	for file in $(ETC); do \
		install -D -m $$(stat -c %a $$file) $$file $(STAGE)/$$file; \
		echo /$$file >> $(STAGE)/DEBIAN/conffiles; \
	done
	for file in $(USR); do \
		install -D -m 0644 $$file $(STAGE)/$$file; \
	done
	install -D -m 0644 $(VERSION_FILE) $(STAGE)/etc/hyprdebian/version.json
	echo /etc/hyprdebian/version.json >> $(STAGE)/DEBIAN/conffiles
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	install -m 0755 deb/postinst deb/prerm deb/postrm $(STAGE)/DEBIAN/
	install -m 0644 deb/triggers $(STAGE)/DEBIAN/
	dpkg-deb --root-owner-group --build $(STAGE) $@

# The tests write no bytecode, so they leave nothing behind in tests/.
test:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) -m unittest discover -s tests -p 'test_*.py'

clean:
	rm -rf $(BUILD)
