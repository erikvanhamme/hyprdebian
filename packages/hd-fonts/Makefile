# Builds the package hd-fonts, which holds the JetBrains Mono Nerd Font.
# It only installs the fonts; they are patched by the Nerd Fonts project.
#
#   make deb    build build/hd-fonts_$(DEB_VERSION)_all.deb, with the version
#               from version.json. The fonts come from the Nerd Fonts
#               release zip $(NERD_FONTS_VERSION), which is downloaded to
#               build/ once and checked against ZIP_SHA256. It installs:
#                 /usr/share/fonts/truetype/jetbrains-mono-nerd/
#                                             the families JetBrainsMono Nerd
#                                             Font and JetBrainsMono Nerd
#                                             Font Mono from the zip; not the
#                                             Propo and NL families
#                 /usr/share/doc/hd-fonts/copyright
#                                             deb/copyright
#                 /usr/share/doc/hd-fonts/OFL.txt
#                 /usr/share/doc/hd-fonts/README.md
#                                             the license and the readme from
#                                             the zip
#                 /usr/share/man/man7/hd-fonts.7.gz
#                                             the man page in man/
#               with the control file deb/control. fontconfig refreshes its
#               cache through its own dpkg trigger on /usr/share/fonts.
#               Raise the version in version.json for every package that is
#               handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/, also the downloaded zip

BUILD := build
PYTHON := python3

DEB_NAME := hd-fonts
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
FONTS := /usr/share/fonts/truetype/jetbrains-mono-nerd
DOC := /usr/share/doc/$(DEB_NAME)
# The man pages, named <page>.<section>.
MANPAGES := man/hd-fonts.7

# The release of Nerd Fonts, and the checksum of its JetBrains Mono zip.
# Change both together, and check the new zip and its license first.
NERD_FONTS_VERSION := 3.4.0
ZIP_URL := https://github.com/ryanoasis/nerd-fonts/releases/download/v$(NERD_FONTS_VERSION)/JetBrainsMono.zip
ZIP_SHA256 := 76f05ff3ace48a464a6ca57977998784ff7bdbb65a6d915d7e401cd3927c493c
ZIP := $(BUILD)/JetBrainsMono-$(NERD_FONTS_VERSION).zip
# The families in the package: Nerd Font and Nerd Font Mono, each in all
# styles. The pattern of the first does not match the files of the second.
FONT_FILES := 'JetBrainsMonoNerdFont-*.ttf' 'JetBrainsMonoNerdFontMono-*.ttf'

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# A download that does not match the checksum is removed, so the next make
# downloads it again.
$(ZIP): | $(BUILD)
	wget -O $@.tmp $(ZIP_URL)
	if ! echo '$(ZIP_SHA256)  $@.tmp' | sha256sum --check --quiet; then \
		echo "$(ZIP_URL) does not match ZIP_SHA256" >&2; \
		rm -f $@.tmp; \
		exit 1; \
	fi
	mv $@.tmp $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json deb/control deb/copyright $(MANPAGES) $(ZIP) | $(BUILD)
	rm -rf $(STAGE) $(BUILD)/zip
	unzip -q $(ZIP) $(FONT_FILES) OFL.txt README.md -d $(BUILD)/zip
	install -d $(STAGE)/DEBIAN $(STAGE)$(FONTS) $(STAGE)$(DOC)
	install -m 0644 $(BUILD)/zip/*.ttf $(STAGE)$(FONTS)/
	install -m 0644 deb/copyright $(STAGE)$(DOC)/copyright
	install -m 0644 $(BUILD)/zip/OFL.txt $(BUILD)/zip/README.md $(STAGE)$(DOC)/
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e 's/@NERD_FONTS_VERSION@/$(NERD_FONTS_VERSION)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
