# Builds the package hd-desktop, which depends on the packages of the
# HyprDebian desktop and boots the system into Hyprland with greetd.
#
#   make deb    build build/hd-desktop_$(DEB_VERSION)_all.deb, with the
#               version from version.json. It installs:
#                 /usr/bin/hd-...             the commands that the key
#                                             bindings and hypridle run, in
#                                             COMMANDS
#                 /usr/libexec/hd-desktop/hd-mount-helper
#                                             mounts and unmounts for the
#                                             mount menus, run with sudo
#                 /usr/sbin/hd-desktop-render writes /etc/greetd/config.toml
#                 /usr/share/hd-desktop/templates/greetd.toml.j2
#                                             the template of that config
#                 /etc/...                    the files in etc/, at the
#                                             same path, conffiles: the
#                                             files new users get in
#                                             /etc/skel, the dark theme
#                                             for dconf and the sudoers
#                                             rule of the mount menus
#                 /etc/skel/.icons
#                 /etc/skel/Pictures/Screenshots
#                                             empty directories for new
#                                             users, for nwg-look and the
#                                             screenshots
#                 /usr/...                    the files in usr/, at the
#                                             same path: the drop-in that
#                                             runs hd-desktop-render before
#                                             greetd starts
#                 /usr/share/man/man7/hd-desktop.7.gz
#                 /usr/share/man/man8/hd-desktop-render.8.gz
#                                             the man pages in man/
#               with the control file deb/control, whose Depends are the
#               packages of the desktop, and the maintainer scripts
#               deb/postinst, which diverts /etc/greetd/config.toml of
#               greetd, writes it with hd-desktop-render and compiles the
#               dconf database, and deb/postrm, which undoes the diversion
#               on a removal. Raise the version in version.json for every
#               package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-desktop
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
LIBEXEC_DIR := /usr/libexec/$(DEB_NAME)
TEMPLATE_DIR := /usr/share/$(DEB_NAME)/templates
# The files of the system, by their path below etc/ and usr/.
ETC := $(shell find etc -type f)
USR := $(shell find usr -type f)
# sudo only reads the files in /etc/sudoers.d with this mode.
SUDOERS := etc/sudoers.d/hd-desktop
# The commands, installed in /usr/bin.
COMMANDS := hd-ask-poweroff hd-ask-reboot hd-mode-select hd-mount hd-unmount hd-volume-down \
	hd-volume-mute hd-volume-up hd-wofi
# The commands for root, installed in /usr/sbin.
SBIN_COMMANDS := hd-desktop-render
TEMPLATES := $(wildcard templates/*)
# The man pages, named <page>.<section>.
MANPAGES := man/hd-desktop.7 man/hd-desktop-render.8

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json $(COMMANDS) hd-mount-helper $(SBIN_COMMANDS) $(TEMPLATES) $(ETC) $(USR) \
		deb/control deb/postinst deb/postrm $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)/usr/sbin $(STAGE)$(LIBEXEC_DIR) \
		$(STAGE)$(TEMPLATE_DIR)
	install -m 0755 $(COMMANDS) $(STAGE)/usr/bin/
	install -m 0755 hd-mount-helper $(STAGE)$(LIBEXEC_DIR)/
	install -m 0755 $(SBIN_COMMANDS) $(STAGE)/usr/sbin/
	install -m 0644 $(TEMPLATES) $(STAGE)$(TEMPLATE_DIR)/
	for file in $(ETC); do \
		if [ "$$file" = "$(SUDOERS)" ]; then mode=0440; else mode=0644; fi; \
		install -D -m $$mode $$file $(STAGE)/$$file; \
		echo /$$file >> $(STAGE)/DEBIAN/conffiles; \
	done
	install -d $(STAGE)/etc/skel/.icons $(STAGE)/etc/skel/Pictures/Screenshots
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
	install -m 0755 deb/postinst deb/postrm $(STAGE)/DEBIAN/
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
