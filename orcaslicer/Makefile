# Builds the package orcaslicer, the slicer for 3D printers, from the AppImage
# of a release of OrcaSlicer, unpacked so it runs without FUSE.
#
#   make deb    build build/orcaslicer_$(DEB_VERSION)_$(ARCH).deb. The
#               AppImage of the release ORCA_VERSION is downloaded to build/
#               and checked against APPIMAGE_SHA256, then unpacked with its
#               own --appimage-extract into build/tree-$(ORCA_VERSION); later
#               builds reuse both, until make clean removes them. It
#               installs:
#                 /usr/lib/orcaslicer/        the unpacked AppImage, with
#                                             its own zlib, liblzma, libexpat
#                                             and libmspack
#                 /usr/bin/orcaslicer         starts it, with software
#                                             rendering on Mesa 26 or newer
#                 /usr/share/applications/orcaslicer.desktop
#                                             the desktop entry,
#                                             deb/orcaslicer.desktop
#                 /usr/share/icons/hicolor/192x192/apps/orcaslicer.png
#                                             the icon of the release
#                 /usr/share/doc/orcaslicer/copyright
#                                             deb/copyright
#               with the control file deb/control. The version of the
#               package is the release with DEB_REVISION; raise DEB_REVISION
#               for a package of the same release that is handed out, and
#               set it back to 1 for a new release.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/, also the AppImage

BUILD := build

# The release of OrcaSlicer, without the v of its tag, and the revision of the
# package of it.
ORCA_VERSION := 2.4.2
DEB_REVISION := 1
APPIMAGE := $(BUILD)/OrcaSlicer_Linux_AppImage_Ubuntu2404_V$(ORCA_VERSION).AppImage
APPIMAGE_URL := https://github.com/OrcaSlicer/OrcaSlicer/releases/download/v$(ORCA_VERSION)/$(notdir $(APPIMAGE))
APPIMAGE_SHA256 := d12fb8c8eac1aecd2dfb6377acd48f994f8fa439ed5292fa532dd82880f029fd

DEB_NAME := orcaslicer
DEB_VERSION := $(ORCA_VERSION)-$(DEB_REVISION)
# The AppImage is for amd64 only.
ARCH := amd64
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_$(ARCH).deb
STAGE := $(BUILD)/deb
TREE := $(BUILD)/tree-$(ORCA_VERSION)
LIB_DIR := /usr/lib/$(DEB_NAME)
DOC := /usr/share/doc/$(DEB_NAME)
ICON := /usr/share/icons/hicolor/192x192/apps/$(DEB_NAME).png

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# A download that does not match the checksum is removed, so the next make
# downloads it again.
$(APPIMAGE): | $(BUILD)
	wget -O $@.tmp $(APPIMAGE_URL)
	if ! echo '$(APPIMAGE_SHA256)  $@.tmp' | sha256sum --check --quiet; then \
		echo "$(APPIMAGE_URL) does not match APPIMAGE_SHA256" >&2; \
		rm -f $@.tmp; \
		exit 1; \
	fi
	mv $@.tmp $@

# --appimage-extract unpacks into squashfs-root in the current directory and
# needs no FUSE. It runs in a directory of its own, so a failed run leaves no
# TREE behind.
$(TREE): $(APPIMAGE)
	rm -rf $@ $@.tmp
	mkdir -p $@.tmp
	chmod +x $(APPIMAGE)
	cd $@.tmp && $(CURDIR)/$(APPIMAGE) --appimage-extract > /dev/null
	mv $@.tmp/squashfs-root $@
	rmdir $@.tmp

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root. Of the AppImage only what AppRun runs goes to LIB_DIR;
# the desktop entry and the icon go to their own places.
$(DEB): $(TREE) orcaslicer deb/control deb/copyright deb/orcaslicer.desktop | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)$(LIB_DIR) $(STAGE)/usr/bin \
		$(STAGE)/usr/share/applications $(STAGE)$(DOC)
	cp -a $(TREE)/AppRun $(TREE)/bin $(TREE)/lib $(TREE)/libexec $(TREE)/resources \
		$(STAGE)$(LIB_DIR)/
	chmod -R u+rwX,go+rX,go-w $(STAGE)$(LIB_DIR)
	install -m 0755 orcaslicer $(STAGE)/usr/bin/
	install -m 0644 deb/orcaslicer.desktop $(STAGE)/usr/share/applications/
	install -D -m 0644 $(TREE)/usr/share/icons/hicolor/192x192/apps/OrcaSlicer.png $(STAGE)$(ICON)
	install -m 0644 deb/copyright $(STAGE)$(DOC)/
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e 's/@ORCA_VERSION@/$(ORCA_VERSION)/' \
		-e 's/@ARCH@/$(ARCH)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
