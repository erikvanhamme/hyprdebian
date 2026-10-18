# Builds the package hd-screensaver, the screensaver of the HyprDebian desktop,
# which hypridle of hd-desktop starts.
#
#   make deb    build build/hd-screensaver_$(DEB_VERSION)_all.deb, with the
#               version from version.json. It installs:
#                 /usr/bin/hd-launch-screensaver
#                                             starts the screensaver on
#                                             every monitor
#                 /usr/bin/hd-cmd-screensaver runs the screensaver in its
#                                             kitty window
#                 /usr/bin/hd-kill-screensaver
#                                             stops the screensaver
#                 /usr/share/hd-screensaver/screensaver.txt
#                                             the text that it shows when
#                                             the user has none
#                 /usr/share/man/man7/hd-screensaver.7.gz
#                                             the man page in man/
#               with the control file deb/control. Raise the version in
#               version.json for every package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-screensaver
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
SHARE_DIR := /usr/share/$(DEB_NAME)
# The commands, installed in /usr/bin.
COMMANDS := hd-launch-screensaver hd-cmd-screensaver hd-kill-screensaver
# The man pages, named <page>.<section>.
MANPAGES := man/hd-screensaver.7

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json $(COMMANDS) screensaver.txt deb/control $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)$(SHARE_DIR)
	install -m 0755 $(COMMANDS) $(STAGE)/usr/bin/
	install -m 0644 screensaver.txt $(STAGE)$(SHARE_DIR)/screensaver.txt
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
