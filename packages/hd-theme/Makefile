# Builds the package hd-theme, the theme engine of HyprDebian: the themes in
# /etc/hyprdebian/themes, the one selected in the key hd-theme of
# /etc/hyprdebian/config.json, and the theme files of the desktop programs in
# the home directories.
#
#   make deb    build build/hd-theme_$(DEB_VERSION)_all.deb, with the version
#               from version.json. It installs:
#                 /usr/bin/hd-theme           adds, deletes, lists,
#                                             selects and applies themes:
#                                             writes the theme files of
#                                             the user from the selected
#                                             theme
#                 /usr/bin/hd-theme-card      renders a theme into a card,
#                                             a PNG
#                 /usr/share/hd-theme/templates/
#                                             the jinja2 templates of the
#                                             theme files, in templates/
#                 /etc/hyprdebian/themes/     the themes in themes/,
#                                             conffiles
#                 /usr/share/man/man1/hd-theme.1.gz
#                 /usr/share/man/man1/hd-theme-card.1.gz
#                 /usr/share/man/man7/hd-theme.7.gz
#                                             the man pages in man/
#               with the control file deb/control and the maintainer
#               scripts deb/postinst, which renders the cards and selects
#               the theme default when none is selected, and deb/postrm,
#               which removes the cards and, on a purge, the key hd-theme.
#               Raise the version in
#               version.json for every package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-theme
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
TEMPLATE_DIR := /usr/share/$(DEB_NAME)/templates
THEME_DIR := /etc/hyprdebian/themes
# The commands, installed in /usr/bin.
COMMANDS := hd-theme hd-theme-card
TEMPLATES := $(wildcard templates/*.j2)
THEMES := $(wildcard themes/*.json)
# The man pages, named <page>.<section>.
MANPAGES := man/hd-theme.7 man/hd-theme.1 man/hd-theme-card.1

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json $(COMMANDS) $(TEMPLATES) $(THEMES) deb/control \
		deb/postinst deb/postrm $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)$(TEMPLATE_DIR) $(STAGE)$(THEME_DIR)
	install -m 0755 $(COMMANDS) $(STAGE)/usr/bin/
	install -m 0644 $(TEMPLATES) $(STAGE)$(TEMPLATE_DIR)/
	for theme in $(THEMES); do \
		install -m 0644 $$theme $(STAGE)$(THEME_DIR)/; \
		echo $(THEME_DIR)/$$(basename $$theme) >> $(STAGE)/DEBIAN/conffiles; \
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
