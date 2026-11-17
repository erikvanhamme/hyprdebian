# Builds the package hd-agent, which sends the state of the system to the
# hyprdebian dashboard of hd-dashboard every 15 minutes.
#
#   make deb    build build/hd-agent_$(DEB_VERSION)_all.deb, with the
#               version from version.json. It installs:
#                 /usr/sbin/hd-agent          collects the report and sends
#                                             it to the dashboard
#                 /usr/share/hd-agent/defaults.json
#                                             defaults.json, the default
#                                             settings
#                 /usr/lib/systemd/system/hd-agent.service
#                                             runs hd-agent as root
#                 /usr/lib/systemd/system/hd-agent.timer
#                                             runs hd-agent.service every
#                                             15 minutes
#                 /usr/share/man/man8/hd-agent.8.gz
#                                             the man page in man/
#               with the control file deb/control and the maintainer scripts
#               deb/postinst, deb/prerm and deb/postrm. The postinst stores
#               the default settings in the key hd-agent of
#               /etc/hyprdebian/config.json when there are none, generates
#               the token in /etc/hyprdebian/hd-agent/token when there is
#               none, and enables and starts the timer. On an installation
#               over hd-report, it takes over its token, settings and the
#               state of its timer instead. The prerm stops the timer on
#               a removal; the postrm masks it on a removal, and on a purge
#               removes the settings but keeps the token. Raise the version
#               in version.json for every package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make test   run the unittest tests in tests/test_agent.py, which need
#               Python only; they also load ../hd-dashboard/hd-dashboard,
#               which must accept the reports
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-agent
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
SHARE_DIR := /usr/share/$(DEB_NAME)
UNIT_DIR := /usr/lib/systemd/system
UNITS := hd-agent.service hd-agent.timer
# The maintainer scripts in deb/.
SCRIPTS := deb/postinst deb/prerm deb/postrm
# The man pages, named <page>.<section>.
MANPAGES := man/hd-agent.8

.PHONY: deb deb-path test clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json hd-agent defaults.json $(UNITS) deb/control $(SCRIPTS) $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/sbin $(STAGE)$(SHARE_DIR) $(STAGE)$(UNIT_DIR)
	install -m 0755 hd-agent $(STAGE)/usr/sbin/hd-agent
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

# The tests write no bytecode, so they leave nothing behind in tests/.
test:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) -m unittest discover -s tests -p 'test_*.py'

clean:
	rm -rf $(BUILD)
