# Builds the package hd-backup, the backup tools: hd-backup for filesets on hard drives and tape,
# hd-backup-prep, hd-backup-load and hd-backup-unload for its hard drives, and hd-backup-vm, hd-backup-vms and hd-restore-vm for virtual machines on ZFS volumes. The
# package is for amd64, as it holds blockwrite, compiled from blockwrite.c.
#
#   make deb    build build/hd-backup_$(DEB_VERSION)_amd64.deb, with the
#               version from version.json. It installs:
#                 /usr/bin/hd-backup          backs up filesets to hard
#                                             drives and tape
#                 /usr/bin/blockwrite         writes the tape stream of
#                                             hd-backup in blocks of 4 MiB
#                 /usr/sbin/hd-backup-prep    turns a drive into an
#                                             encrypted ZFS pool for
#                                             hd-backup
#                 /usr/sbin/hd-backup-load    imports, unlocks and mounts
#                                             such a drive
#                 /usr/sbin/hd-backup-unload  exports it, so it can be
#                                             disconnected
#                 /usr/sbin/hd-backup-vm      backs up a VM
#                 /usr/sbin/hd-backup-vms     backs up the VMs in the key
#                                             hd-backup of
#                                             /etc/hyprdebian/config.json
#                 /usr/sbin/hd-restore-vm     restores a VM
#                 /usr/lib/systemd/system/hd-backup-vms.service
#                                             runs hd-backup-vms as root
#                 /usr/lib/systemd/system/hd-backup-vms.timer
#                                             runs hd-backup-vms.service
#                                             every night at 3:30
#                 /usr/share/man/man1/*.1.gz and
#                 /usr/share/man/man8/*.8.gz  the man pages in man/
#               with the control file deb/control and the maintainer scripts
#               deb/postinst, deb/prerm and deb/postrm. The postinst stores
#               an empty list of VMs in the key hd-backup of
#               /etc/hyprdebian/config.json when there is none; it does not
#               enable the timer. The prerm stops the timer on a removal;
#               the postrm masks it on a removal, and on a purge removes
#               the key. Raise the version in version.json for every
#               package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make test   build blockwrite and run the unittest tests in tests/; the
#               system commands of the VM tools are replaced by fakes
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-backup
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_amd64.deb
STAGE := $(BUILD)/deb
UNIT_DIR := /usr/lib/systemd/system
# hd-backup keeps a database per user, so every user can run it; the drive and VM tools are for
# root.
USER_COMMANDS := hd-backup
ADMIN_COMMANDS := hd-backup-prep hd-backup-load hd-backup-unload hd-backup-vm hd-backup-vms \
	hd-restore-vm
UNITS := hd-backup-vms.service hd-backup-vms.timer
BLOCKWRITE := $(BUILD)/blockwrite
# The maintainer scripts in deb/.
SCRIPTS := deb/postinst deb/prerm deb/postrm
# The man pages, named <page>.<section>.
# hd-backup-unload.8 includes hd-backup-load.8.
MANPAGES := man/hd-backup.1 man/blockwrite.1 man/hd-backup-prep.8 man/hd-backup-load.8 \
	man/hd-backup-unload.8 man/hd-backup-vm.8 man/hd-backup-vms.8 man/hd-restore-vm.8

.PHONY: deb deb-path test clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# Compiled with the hardening flags Debian packages use.
CC := gcc
CFLAGS := $(shell dpkg-buildflags --get CFLAGS) -Wall -Werror
CPPFLAGS := $(shell dpkg-buildflags --get CPPFLAGS) -D_GNU_SOURCE
LDFLAGS := $(shell dpkg-buildflags --get LDFLAGS)

$(BLOCKWRITE): blockwrite.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json $(USER_COMMANDS) $(BLOCKWRITE) $(ADMIN_COMMANDS) $(UNITS) deb/control $(SCRIPTS) $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)/usr/sbin $(STAGE)$(UNIT_DIR)
	install -m 0755 $(USER_COMMANDS) $(STAGE)/usr/bin/
	install -s -m 0755 $(BLOCKWRITE) $(STAGE)/usr/bin/
	install -m 0755 $(ADMIN_COMMANDS) $(STAGE)/usr/sbin/
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

# The tests write no bytecode, so they leave nothing behind in tests/. They test the blockwrite
# that make deb installs.
test: $(BLOCKWRITE)
	BLOCKWRITE=$(BLOCKWRITE) PYTHONDONTWRITEBYTECODE=1 $(PYTHON) -m unittest discover -s tests -p 'test_*.py'

clean:
	rm -rf $(BUILD)
