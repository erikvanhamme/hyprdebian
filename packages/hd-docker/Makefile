# Builds the package hd-docker, which installs Docker and Docker Compose and
# sets them up to work with the ufw firewall.
#
#   make deb    build build/hd-docker_$(DEB_VERSION)_all.deb, with the version
#               from version.json. It installs:
#                 /etc/docker/daemon.json     the addresses of the networks
#                                             of Docker and the open-files
#                                             limit of containers, a
#                                             conffile
#                 /usr/share/hd-docker/docker-ufw.rules
#                                             the Docker rules that the
#                                             postinst puts in
#                                             /etc/ufw/after.rules
#                 /usr/share/man/man7/hd-docker.7.gz
#                                             the man page in man/
#               with the control file deb/control, whose Depends are
#               docker.io, docker-compose and ufw, and the maintainer
#               scripts deb/postinst, which puts the rules in
#               /etc/ufw/after.rules and restarts Docker on the first
#               installation, and deb/postrm, which takes the rules out on
#               a removal. Raise the version in version.json for every
#               package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-docker
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
DAEMON_JSON := /etc/docker/daemon.json
SHARE_DIR := /usr/share/$(DEB_NAME)
# The maintainer scripts in deb/.
SCRIPTS := deb/postinst deb/postrm
# The man pages, named <page>.<section>.
MANPAGES := man/hd-docker.7

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json deb$(DAEMON_JSON) docker-ufw.rules deb/control $(SCRIPTS) $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)$(SHARE_DIR)
	install -D -m 0644 deb$(DAEMON_JSON) $(STAGE)$(DAEMON_JSON)
	printf '%s\n' $(DAEMON_JSON) > $(STAGE)/DEBIAN/conffiles
	install -m 0644 docker-ufw.rules $(STAGE)$(SHARE_DIR)/
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
