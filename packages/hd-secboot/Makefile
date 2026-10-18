# Builds the package hd-secboot, which keeps the tools for Secure Boot keys of
# the owner installed and generates those keys on its first installation.
#
#   make deb    build build/hd-secboot_$(DEB_VERSION)_all.deb, with the version
#               from version.json. It installs:
#                 /usr/sbin/hd-secboot-enroll enrolls the keys in the
#                                             firmware
#                 /usr/sbin/hd-secboot-check  checks that Secure Boot is on
#                 /usr/share/man/man7/hd-secboot.7.gz
#                 /usr/share/man/man8/hd-secboot-enroll.8.gz
#                 /usr/share/man/man8/hd-secboot-check.8.gz
#                                             the man pages in man/
#               with the control file deb/control, whose Depends are the
#               tools, and the maintainer script deb/postinst, which
#               generates the keys in /etc/hyprdebian/secboot when they are
#               not there yet. Raise the version in version.json for every
#               package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make test   run the unittest tests in tests/, which need Python only
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-secboot
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
# The man pages, named <page>.<section>.
MANPAGES := man/hd-secboot.7 man/hd-secboot-enroll.8 man/hd-secboot-check.8
# The commands in /usr/sbin.
COMMANDS := hd-secboot-enroll hd-secboot-check

.PHONY: deb deb-path test clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json $(COMMANDS) deb/control deb/postinst $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/sbin
	install -m 0755 $(COMMANDS) $(STAGE)/usr/sbin/
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	install -m 0755 deb/postinst $(STAGE)/DEBIAN/
	dpkg-deb --root-owner-group --build $(STAGE) $@

# The tests write no bytecode, so they leave nothing behind in tests/.
test:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) -m unittest discover -s tests -p 'test_*.py'

clean:
	rm -rf $(BUILD)
