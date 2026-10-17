# Builds the meta package hd-base, which depends on the packages that every
# hyprdebian system has.
#
#   make deb    build build/hd-base_$(DEB_VERSION)_all.deb, with the version
#               from ../version.json, the version of HyprDebian. It installs:
#                 /etc/hyprdebian/version.json
#                                             ../version.json, which the
#                                             banners show, a conffile
#                 /usr/bin/hd-up              upgrades the system after a
#                                             ZFS snapshot
#                 /usr/bin/hd-reboot
#                 /usr/bin/hd-poweroff        reboot or power off, with
#                                             hyprshutdown in a Hyprland
#                                             session, otherwise with
#                                             systemctl
#                 /usr/sbin/hd-issue          writes the login banner in
#                                             /etc/issue
#                 /etc/...                    the files in etc/, at the
#                                             same path and with the same
#                                             mode, conffiles: the files
#                                             new users get in /etc/skel,
#                                             the PATH of /etc/profile.d,
#                                             the apt preferences and the
#                                             message after a login in
#                                             /etc/update-motd.d
#                 /usr/...                    the files in usr/, at the
#                                             same path: the Num Lock
#                                             drop-in of getty, the zram
#                                             swap, the sysctls for it,
#                                             the resolved config and the
#                                             presets
#                 /usr/share/man/man7/hd-base.7.gz
#                 /usr/share/man/man1/hd-up.1.gz
#                 /usr/share/man/man1/hd-reboot.1.gz
#                 /usr/share/man/man1/hd-poweroff.1.gz
#                 /usr/share/man/man8/hd-issue.8.gz
#                                             the man pages in man/
#               with the control file deb/control, whose Depends are the
#               packages of the base system, and the maintainer scripts
#               deb/postinst, which masks the units HyprDebian does not use,
#               points /etc/resolv.conf at the name servers, empties
#               /etc/motd, diverts 10-uname and /etc/issue of base-files
#               and writes the login banner with hd-issue, and
#               deb/postrm, which undoes the diversions on a removal and
#               unmasks the units on a purge. Add a package there, to the
#               man page and to docs/packages/hd-base.md when every system
#               must have it. Raise the version in ../version.json for every
#               change to a package or the installer.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-base
# The version of HyprDebian, in the root of the repository, is the version of hd-base.
VERSION_FILE := ../version.json
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("$(VERSION_FILE)")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
# The files of the system, by their path below etc/ and usr/.
ETC := $(shell find etc -type f)
USR := $(shell find usr -type f)
# The commands, installed in /usr/bin.
COMMANDS := hd-up hd-reboot hd-poweroff
# The commands for root, installed in /usr/sbin.
SBIN_COMMANDS := hd-issue
# The man pages, named <page>.<section>. hd-poweroff.1 includes hd-reboot.1.
MANPAGES := man/hd-base.7 man/hd-up.1 man/hd-reboot.1 man/hd-poweroff.1 man/hd-issue.8

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): $(VERSION_FILE) $(COMMANDS) $(SBIN_COMMANDS) $(ETC) $(USR) deb/control deb/postinst \
		deb/postrm $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)/usr/sbin
	install -m 0755 $(COMMANDS) $(STAGE)/usr/bin/
	install -m 0755 $(SBIN_COMMANDS) $(STAGE)/usr/sbin/
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
	install -m 0755 deb/postinst deb/postrm $(STAGE)/DEBIAN/
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
