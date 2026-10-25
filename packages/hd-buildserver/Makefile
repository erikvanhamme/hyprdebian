# Builds the package hd-buildserver, which builds HyprDebian from its git
# repository every night and updates a HyprDebian package repository with it.
#
#   make deb    build build/hd-buildserver_$(DEB_VERSION)_all.deb, with the
#               version from version.json. It installs:
#                 /usr/bin/hd-buildserver     builds HyprDebian and uploads
#                                             it to the package repository
#                 /usr/share/hd-buildserver/defaults.json
#                                             defaults.json, the default
#                                             settings
#                 /usr/lib/systemd/system/hd-buildserver.service
#                                             runs hd-buildserver as the
#                                             user hd-buildserver
#                 /usr/lib/systemd/system/hd-buildserver.timer
#                                             runs hd-buildserver.service
#                                             every night
#                 /usr/share/man/man8/hd-buildserver.8.gz
#                                             the man page in man/
#               with the control file deb/control and the maintainer scripts
#               deb/postinst, deb/prerm and deb/postrm. The postinst stores
#               the default settings in the key hd-buildserver of
#               /etc/hyprdebian/config.json when there are none, creates the
#               user hd-buildserver in the group docker with its ssh key when
#               there is none, and enables and starts the timer. The prerm
#               stops it on a removal; the postrm masks it on a removal, and
#               on a purge removes the settings, the user and its home
#               directory. Raise the version in version.json for every package
#               that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-buildserver
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
SHARE_DIR := /usr/share/$(DEB_NAME)
UNIT_DIR := /usr/lib/systemd/system
UNITS := hd-buildserver.service hd-buildserver.timer
# The maintainer scripts in deb/.
SCRIPTS := deb/postinst deb/prerm deb/postrm
# The man pages, named <page>.<section>.
MANPAGES := man/hd-buildserver.8

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json hd-buildserver defaults.json $(UNITS) deb/control $(SCRIPTS) $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)$(SHARE_DIR) $(STAGE)$(UNIT_DIR)
	install -m 0755 hd-buildserver $(STAGE)/usr/bin/hd-buildserver
	install -m 0644 defaults.json $(STAGE)$(SHARE_DIR)/defaults.json
	install -m 0644 $(UNITS) $(STAGE)$(UNIT_DIR)/
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
