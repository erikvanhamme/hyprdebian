# Builds the package hd-dashboard, the dashboard that shows the status reports
# of the hyprdebian systems, run by hd-dashboard.service.
#
#   make deb    build build/hd-dashboard_$(DEB_VERSION)_all.deb, with the
#               version from version.json. It installs:
#                 /usr/sbin/hd-dashboard      the dashboard: serves it, lists
#                                             the hosts and forgets a host
#                 /usr/share/hd-dashboard/defaults.json
#                                             defaults.json, the default
#                                             settings
#                 /usr/lib/systemd/system/hd-dashboard.service
#                                             runs hd-dashboard serve
#                 /usr/share/man/man7/hd-dashboard.7.gz
#                 /usr/share/man/man8/hd-dashboard.8.gz
#                                             the man pages in man/
#               with the control file deb/control and the maintainer scripts
#               deb/postinst, deb/prerm and deb/postrm. The postinst stores
#               the default settings in the key hd-dashboard of
#               /etc/hyprdebian/config.json when there are none, and enables
#               and starts the service; it leaves the firewall alone. The
#               prerm stops it on a removal; the postrm masks it on a
#               removal, and on a purge removes the settings and the
#               database. Raise the version in version.json
#               for every package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make test   run the unittest tests in tests/test_dashboard.py, which
#               need Python only
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-dashboard
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
SHARE_DIR := /usr/share/$(DEB_NAME)
UNIT_DIR := /usr/lib/systemd/system
# The maintainer scripts in deb/.
SCRIPTS := deb/postinst deb/prerm deb/postrm
# The man pages, named <page>.<section>.
MANPAGES := man/hd-dashboard.7 man/hd-dashboard.8

.PHONY: deb deb-path test clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json hd-dashboard defaults.json hd-dashboard.service deb/control $(SCRIPTS) \
		$(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/sbin $(STAGE)$(SHARE_DIR) $(STAGE)$(UNIT_DIR)
	install -m 0755 hd-dashboard $(STAGE)/usr/sbin/hd-dashboard
	install -m 0644 defaults.json $(STAGE)$(SHARE_DIR)/defaults.json
	install -m 0644 hd-dashboard.service $(STAGE)$(UNIT_DIR)/hd-dashboard.service
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

# The tests write no bytecode, so they leave nothing behind in tests/.
test:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) -m unittest discover -s tests -p 'test_*.py'

clean:
	rm -rf $(BUILD)
