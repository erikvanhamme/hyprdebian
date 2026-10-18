# Builds the package hd-plymouth, which holds the plymouth theme hyprdebian:
# a boot splash that looks like the lock screen of hyprlock, and asks for
# passwords, such as the one of an encrypted ZFS dataset.
#
#   make deb    build build/hd-plymouth_$(DEB_VERSION)_all.deb, with the
#               version from version.json. It installs:
#                 /usr/share/plymouth/themes/hyprdebian/
#                                             the theme in theme/, and
#                                             background.png, a link to the
#                                             wallpaper $(WALLPAPER) of hd-art
#                 /usr/share/hd-plymouth/plymouthd.conf
#                                             the config of plymouthd that
#                                             names the theme
#                 /usr/share/initramfs-tools/hooks/hd-plymouth
#                                             the hook in initramfs/, which
#                                             puts the fonts of the theme in
#                                             the initramfs
#                 /usr/share/doc/hd-plymouth/copyright
#                                             deb/copyright
#                 /usr/share/man/man7/hd-plymouth.7.gz
#                                             the man page in man/
#               with the control file deb/control, deb/triggers, which has
#               initramfs-tools rebuild the initramfs, and the maintainer
#               scripts deb/postinst, which diverts
#               /etc/plymouth/plymouthd.conf of plymouth and copies the
#               config of hd-plymouth in its place, and deb/postrm, which
#               undoes the diversion on a removal. Raise the version in
#               version.json for every package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make images render the images of the input field in theme/ again with
#               ImageMagick: the dot, a black pixel for the fill and a
#               pixel of each outline colour.
#               They are kept in the repository, so make deb does not need
#               ImageMagick.
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-plymouth
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
THEME_DIR := /usr/share/plymouth/themes/hyprdebian
SHARE_DIR := /usr/share/$(DEB_NAME)
HOOK_DIR := /usr/share/initramfs-tools/hooks
DOC := /usr/share/doc/$(DEB_NAME)
# The background of the theme. The hook of plymouth copies the theme into
# the initramfs, with the wallpaper that the link points to.
WALLPAPER := /usr/share/backgrounds/hyprdebian/emerald-1920x1080.png
THEME := $(wildcard theme/*)
# The man pages, named <page>.<section>.
MANPAGES := man/hd-plymouth.7
# Writes PNGs without time stamps, so the same render gives the same file.
MAGICK := magick
PNG_OPTIONS := -strip -define png:exclude-chunks=date,time

.PHONY: deb deb-path images clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json $(THEME) plymouthd.conf initramfs/hd-plymouth deb/control deb/copyright \
		deb/triggers deb/postinst deb/postrm $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)$(THEME_DIR) $(STAGE)$(SHARE_DIR) $(STAGE)$(HOOK_DIR) \
		$(STAGE)$(DOC)
	install -m 0644 $(THEME) $(STAGE)$(THEME_DIR)/
	ln -s $(WALLPAPER) $(STAGE)$(THEME_DIR)/background.png
	install -m 0644 plymouthd.conf $(STAGE)$(SHARE_DIR)/
	install -m 0755 initramfs/hd-plymouth $(STAGE)$(HOOK_DIR)/
	install -m 0644 deb/copyright $(STAGE)$(DOC)/copyright
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	install -m 0644 deb/triggers $(STAGE)/DEBIAN/
	install -m 0755 deb/postinst deb/postrm $(STAGE)/DEBIAN/
	dpkg-deb --root-owner-group --build $(STAGE) $@

# The colours are those of the input field in hyprlock.conf of hd-desktop:
# font_color for the dot, black for inner_color, whose opacity the theme
# sets, outer_color for the outline, and the defaults of hyprlock for
# check_color and fail_color. The dot is drawn at four times
# its size and scaled down, for smooth edges; the theme scales it again to a
# quarter of the height of the field.
images:
	$(MAGICK) -size 100x100 xc:none -fill 'rgb(0,102,204)' -draw 'circle 49.5,49.5 49.5,0' \
		-resize 25x25 $(PNG_OPTIONS) theme/dot.png
	$(MAGICK) -size 1x1 xc:'rgb(0,0,0)' $(PNG_OPTIONS) theme/fill.png
	$(MAGICK) -size 1x1 xc:'rgb(255,102,0)' $(PNG_OPTIONS) theme/outline.png
	$(MAGICK) -size 1x1 xc:'rgb(204,136,34)' $(PNG_OPTIONS) theme/outline-check.png
	$(MAGICK) -size 1x1 xc:'rgb(204,34,34)' $(PNG_OPTIONS) theme/outline-fail.png

clean:
	rm -rf $(BUILD)
