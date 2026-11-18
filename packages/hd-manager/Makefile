# Builds the package hd-manager, which makes the jobs of the estate, signs them
# with the manager key and sends them to the hyprdebian dashboard of
# hd-dashboard.
#
#   make deb    build build/hd-manager_$(DEB_VERSION)_all.deb, with the
#               version from version.json. It installs:
#                 /usr/bin/hd-manager         makes the manager key, and
#                                             schedules and lists the jobs
#                 /usr/share/hd-manager/defaults.json
#                                             defaults.json, the default
#                                             settings
#                 /usr/share/man/man1/hd-manager.1.gz
#                                             the man page in man/
#               with the control file deb/control. It has no maintainer
#               scripts: the settings are those of each user, in
#               ~/.config/hyprdebian/config.json. Raise the version in
#               version.json for every package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make test   run the unittest tests in tests/test_manager.py, which need
#               Python and ssh-keygen only; they run ../hd-dashboard/hd-dashboard
#               as a local test dashboard
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-manager
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
SHARE_DIR := /usr/share/$(DEB_NAME)
# The man pages, named <page>.<section>.
MANPAGES := man/hd-manager.1

.PHONY: deb deb-path test clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json hd-manager defaults.json deb/control $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)$(SHARE_DIR)
	install -m 0755 hd-manager $(STAGE)/usr/bin/hd-manager
	install -m 0644 defaults.json $(STAGE)$(SHARE_DIR)/defaults.json
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

# The tests write no bytecode, so they leave nothing behind in tests/.
test:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) -m unittest discover -s tests -p 'test_*.py'

clean:
	rm -rf $(BUILD)
