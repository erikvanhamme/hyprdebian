# Builds the package hd-laptop, which holds the parts of HyprDebian that only
# laptops need.
#
#   make deb    build build/hd-laptop_$(DEB_VERSION)_all.deb, with the version
#               from version.json. It installs:
#                 /usr/bin/hd-battery         shows the battery level in a
#                                             notification, run by the key
#                                             binding SUPER + SHIFT + B,
#                                             and with --monitor shows
#                                             notifications of the battery
#                                             level and of the charger, run
#                                             by hd-battery-monitor.service
#                 /usr/...                    the files in usr/, at the
#                                             same path: the user unit
#                                             hd-battery-monitor.service
#                 /usr/share/man/man*/*.gz    the man pages in man/
#               with the control file deb/control, whose Depends are python3,
#               hd-desktop, for mako, and libnotify-bin, for notify-send, and
#               the maintainer scripts deb/postinst, which enables
#               hd-battery-monitor.service for every user, and deb/postrm,
#               which masks it on a removal. Raise the version in
#               version.json for every package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-laptop
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
# The commands in /usr/bin.
COMMANDS := hd-battery
# The files in usr/, installed at the same path.
USR := $(shell find usr -type f | sort)
SCRIPTS := deb/postinst deb/postrm
# The man pages, named <page>.<section>.
MANPAGES := man/hd-laptop.7 man/hd-battery.1

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json deb/control $(SCRIPTS) $(COMMANDS) $(USR) $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin
	install -m 0755 $(COMMANDS) $(STAGE)/usr/bin/
	for file in $(USR); do \
		install -D -m 0644 $$file $(STAGE)/$$file; \
	done
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	install -m 0755 $(SCRIPTS) $(STAGE)/DEBIAN/
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
