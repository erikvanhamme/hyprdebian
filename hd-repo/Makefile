# Builds the package hd-repo, which hosts the hyprdebian package repository.
# It only hosts and serves packages; it builds none.
#
#   make deb    build build/hd-repo_$(DEB_VERSION)_all.deb, with the version
#               from version.json. It installs:
#                 /usr/bin/hd-repo-update     adds the packages in the
#                                             inboxes to the repository and
#                                             signs it, run as the user repo
#                 /etc/nginx/sites-available/hd-repo
#                                             the nginx site that serves
#                                             /srv/hd-repo on port 80, a
#                                             conffile
#                 /usr/share/man/man7/hd-repo.7.gz
#                 /usr/share/man/man1/hd-repo-update.1.gz
#                                             the man pages in man/
#               with the control file deb/control and the maintainer scripts
#               deb/postinst and deb/postrm. The postinst creates the user
#               repo, the inboxes /var/lib/hd-repo/inbox/<suite>, the
#               repository /srv/hd-repo and, when there is none yet, the
#               signing key in /etc/hyprdebian/hd-repo/gnupg; it makes and
#               signs the first indices and enables the nginx site in place
#               of the default one. Raise the version in version.json for
#               every package that is handed out.
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-repo
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
SITE := /etc/nginx/sites-available/hd-repo
# The man pages, named <page>.<section>.
MANPAGES := man/hd-repo.7 man/hd-repo-update.1

.PHONY: deb clean

deb: $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json hd-repo-update deb/control deb/postinst deb/postrm deb$(SITE) \
		$(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin
	install -m 0755 hd-repo-update $(STAGE)/usr/bin/hd-repo-update
	install -D -m 0644 deb$(SITE) $(STAGE)$(SITE)
	printf '%s\n' $(SITE) > $(STAGE)/DEBIAN/conffiles
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
