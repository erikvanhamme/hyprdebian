# Builds the package hd-desktop, the HyprDebian desktop: it depends on the
# packages of the desktop, boots the system into Hyprland with greetd, and
# holds the parts of the desktop: the wallpapers, the font, the menu, the
# theme engine, the screensaver and the boot splash.
#
#   make deb    build build/hd-desktop_$(DEB_VERSION)_$(ARCH).deb, with the
#               version from ../../version.json, the version of HyprDebian,
#               as hd-base. It installs:
#                 /usr/bin/hd-...             the commands in COMMANDS: those
#                                             that the key bindings and
#                                             hypridle run, hd-theme and
#                                             hd-theme-card of the theme
#                                             engine, and the commands of
#                                             the screensaver
#                 /usr/bin/hd-menu            the menu, compiled from menu/
#                 /usr/bin/hd-message         the message box, compiled from
#                                             message/; common/build.sh
#                                             compiles both, with the code
#                                             they share in common/, with
#                                             make all of menu/Makefile and
#                                             message/Makefile in one
#                                             $(IMAGE) container, into
#                                             build/programs; later builds
#                                             reuse them until a source
#                                             changes or make clean removes
#                                             them
#                 /usr/libexec/hd-desktop/hd-mount-helper
#                                             mounts and unmounts for the
#                                             mount menus, run with sudo
#                 /usr/libexec/hd-desktop/hd-reboot-notify
#                                             shows that a reboot is
#                                             needed, run by
#                                             hd-reboot-notify.service
#                 /usr/libexec/hd-desktop/hd-plymouth-handoff
#                                             hands the boot splash over to
#                                             Hyprland, run by
#                                             hd-plymouth-handoff.service
#                                             and plymouth-quit-wait.service
#                 /usr/libexec/hd-desktop/hd-shutdown
#                                             closes the applications of
#                                             the session, asking with
#                                             hd-message about those that
#                                             do not close, before
#                                             hd-reboot and hd-poweroff
#                 /usr/bin/hd-launcher-cache  writes the menu of the
#                                             launcher in the cache of the
#                                             system or of the user, and
#                                             prints it for the launcher
#                 /usr/lib/hd-desktop/hd_display.py
#                                             lib/hd_display.py, the scale
#                                             of the theme on the monitor,
#                                             shared by hd-theme and
#                                             hd-launcher-cache
#                 /usr/sbin/hd-desktop-render writes /etc/greetd/config.toml
#                 /usr/sbin/hd-plymouth-render
#                                             renders the boot splash in the
#                                             theme of the system in
#                                             /var/lib/hd-plymouth
#                 /usr/share/hd-desktop/templates/greetd.toml.j2
#                                             the template of that config
#                 /usr/share/hd-theme/templates/
#                                             the jinja2 templates of the
#                                             theme files, in
#                                             templates/hd-theme/
#                 /etc/hyprdebian/themes/     the themes in themes/,
#                                             conffiles
#                 /usr/share/hd-screensaver/screensaver.txt
#                                             the text of the screensaver
#                                             when the user has none
#                 /usr/share/backgrounds/hyprdebian/*.png
#                                             the wallpapers in wallpapers/
#                 /usr/share/fonts/truetype/jetbrains-mono-nerd/
#                                             the families JetBrainsMono Nerd
#                                             Font and JetBrainsMono Nerd
#                                             Font Mono from the Nerd Fonts
#                                             release zip
#                                             $(NERD_FONTS_VERSION), which is
#                                             downloaded to build/ once and
#                                             checked against ZIP_SHA256;
#                                             not the Propo and NL families
#                 /usr/share/fonts/truetype/sauce-code-pro-nerd/
#                                             the family SauceCodePro Nerd
#                                             Font Mono from the Nerd Fonts
#                                             release zip of Source Code
#                                             Pro, downloaded and checked
#                                             against SCP_ZIP_SHA256 in the
#                                             same way; not the Nerd Font
#                                             and Propo families
#                 /usr/share/plymouth/themes/hyprdebian/
#                                             the plymouth theme in
#                                             plymouth/theme/, which names
#                                             the files that
#                                             hd-plymouth-render writes
#                 /usr/share/hd-plymouth/templates/
#                                             the jinja2 template of the
#                                             plymouth script, in
#                                             templates/hd-plymouth/
#                 /usr/share/hd-plymouth/plymouthd.conf
#                                             the config of plymouthd that
#                                             names the theme
#                 /usr/share/initramfs-tools/hooks/hd-plymouth
#                                             the hook in plymouth/initramfs/,
#                                             which renders the splash and
#                                             puts it and its fonts in the
#                                             initramfs
#                 /etc/...                    the files in etc/, at the
#                                             same path, conffiles: the
#                                             files new users get in
#                                             /etc/skel, the dark theme
#                                             and the font for dconf and
#                                             the settings.ini of GTK 3
#                                             and GTK 4, the
#                                             sudoers rule of the mount
#                                             menus and the apt hook that
#                                             runs hd-launcher-cache
#                 /etc/skel/.icons
#                 /etc/skel/Pictures/Screenshots
#                                             empty directories for new
#                                             users, for cursor themes and
#                                             the screenshots
#                 /usr/...                    the files in usr/, at the
#                                             same path: the drop-in that
#                                             runs hd-desktop-render before
#                                             greetd starts and puts it on
#                                             tty1, the unit and the drop-in
#                                             of plymouth-quit-wait.service
#                                             that hand the boot splash over
#                                             to Hyprland, the
#                                             warning icon of
#                                             hd-launcher-cache, the
#                                             colour scheme hyprdebian of
#                                             qt5ct and qt6ct and the user
#                                             units that notify of a
#                                             pending reboot
#                 /usr/share/doc/hd-desktop/copyright
#                                             deb/copyright
#                 /usr/share/doc/hd-desktop/nerd-fonts/OFL.txt
#                 /usr/share/doc/hd-desktop/nerd-fonts/README.md
#                                             the license and the readme from
#                                             the zip of JetBrains Mono
#                 /usr/share/doc/hd-desktop/sauce-code-pro-nerd/LICENSE.txt
#                 /usr/share/doc/hd-desktop/sauce-code-pro-nerd/README.md
#                                             the license and the readme from
#                                             the zip of Source Code Pro
#                 /usr/share/man/man*/*.gz    the man pages in man/
#               with the control file deb/control, whose Depends are the
#               packages of the desktop, hd-base of the same version, and
#               the libraries of hd-menu and hd-message that dpkg-shlibdeps
#               finds in the container; deb/triggers, which has initramfs-tools rebuild
#               the initramfs with the boot splash; and the maintainer
#               scripts deb/postinst, which diverts /etc/greetd/config.toml
#               of greetd and writes it with hd-desktop-render, diverts
#               /etc/plymouth/plymouthd.conf of plymouth and copies the
#               config of the boot splash in its place, compiles the dconf
#               database, renders the cards of the themes and selects the
#               theme default when none is selected, renders the boot splash
#               with hd-plymouth-render, and writes the menu of the launcher
#               with hd-launcher-cache, and enables hd-reboot-notify.path
#               for every user, and deb/postrm, which undoes the
#               diversions, removes the cards and masks the user unit on a
#               removal, and
#               removes the menu, the rendered boot splash and the key
#               hd-theme on a purge.
#               deb/preinst, deb/postinst and deb/postrm also remove the
#               conffiles that the package no longer has, with
#               dpkg-maintscript-helper rm_conffile. fontconfig refreshes its
#               cache through its own dpkg trigger on /usr/share/fonts.
#               Raise the version in ../../version.json for every change to a
#               package or the installer.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/, also the compiled hd-menu and hd-message and
#               the downloaded zips of the fonts, and menu/build/ and
#               message/build/ of make all in menu/ and message/

BUILD := build
PYTHON := python3

DEB_NAME := hd-desktop
# The version of HyprDebian, in the root of the repository, is the version of hd-desktop, as of
# hd-base.
VERSION_FILE := ../../version.json
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("$(VERSION_FILE)")); print("%d.%d" % (v["major"], v["minor"]))')
# hd-menu and hd-message are compiled, so the package is for the architecture of this machine, on which the
# container runs.
ARCH := $(shell dpkg --print-architecture)
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_$(ARCH).deb
STAGE := $(BUILD)/deb
LIBEXEC_DIR := /usr/libexec/$(DEB_NAME)
LIB_DIR := /usr/lib/$(DEB_NAME)
TEMPLATE_DIR := /usr/share/$(DEB_NAME)/templates
DOC := /usr/share/doc/$(DEB_NAME)
# The files of the system, by their path below etc/ and usr/.
ETC := $(shell find etc -type f)
USR := $(shell find usr -type f)
# sudo only reads the files in /etc/sudoers.d with this mode.
SUDOERS := etc/sudoers.d/hd-desktop
# The commands, installed in /usr/bin.
COMMANDS := hd-ask-poweroff hd-ask-reboot hd-mode-select hd-mount hd-unmount hd-volume-down \
	hd-volume-mute hd-volume-up hd-theme hd-theme-card hd-launch-screensaver hd-cmd-screensaver \
	hd-kill-screensaver hd-launcher-cache
# The commands for root, installed in /usr/sbin.
SBIN_COMMANDS := hd-desktop-render hd-plymouth-render
TEMPLATES := $(wildcard templates/*.j2)
# The man pages, named <page>.<section>.
MANPAGES := man/hd-desktop.7 man/hd-desktop-render.8 man/hd-launcher-cache.1 man/hd-art.7 \
	man/hd-fonts.7 man/hd-menu.1 man/hd-message.1 man/hd-theme.7 man/hd-theme.1 man/hd-theme-card.1 \
	man/hd-screensaver.7 man/hd-plymouth.7 man/hd-plymouth-render.8

# The theme engine: the templates of the theme files, and the themes.
THEME_TEMPLATE_DIR := /usr/share/hd-theme/templates
THEME_TEMPLATES := $(wildcard templates/hd-theme/*.j2)
THEME_DIR := /etc/hyprdebian/themes
THEMES := $(wildcard themes/*.json)

# The text of the screensaver when the user has none.
SCREENSAVER_DIR := /usr/share/hd-screensaver

# The wallpapers.
BACKGROUNDS := /usr/share/backgrounds/hyprdebian
WALLPAPERS := $(wildcard wallpapers/*.png)

# The fonts, and their license and readme.
FONTS := /usr/share/fonts/truetype/jetbrains-mono-nerd
FONTS_DOC := $(DOC)/nerd-fonts
# The release of Nerd Fonts, and the checksums of its JetBrains Mono and
# Source Code Pro zips. Change them together, and check the new zips and their
# licenses first.
NERD_FONTS_VERSION := 3.4.0
NERD_FONTS_URL := https://github.com/ryanoasis/nerd-fonts/releases/download/v$(NERD_FONTS_VERSION)
ZIP_URL := $(NERD_FONTS_URL)/JetBrainsMono.zip
ZIP_SHA256 := 76f05ff3ace48a464a6ca57977998784ff7bdbb65a6d915d7e401cd3927c493c
ZIP := $(BUILD)/JetBrainsMono-$(NERD_FONTS_VERSION).zip
# The families in the package: Nerd Font and Nerd Font Mono, each in all
# styles. The pattern of the first does not match the files of the second.
FONT_FILES := 'JetBrainsMonoNerdFont-*.ttf' 'JetBrainsMonoNerdFontMono-*.ttf'
# Source Code Pro, as Nerd Fonts names it SauceCodePro: Adobe reserves the
# name Source. Only the family Nerd Font Mono, in all styles; the others would
# make the package 32 MB larger.
SCP_FONTS := /usr/share/fonts/truetype/sauce-code-pro-nerd
SCP_FONTS_DOC := $(DOC)/sauce-code-pro-nerd
SCP_ZIP_URL := $(NERD_FONTS_URL)/SourceCodePro.zip
SCP_ZIP_SHA256 := f8abc5576d1761b8c2a7c03aaff54a692af109f12f2cde641320c8d2f4c5ad53
SCP_ZIP := $(BUILD)/SourceCodePro-$(NERD_FONTS_VERSION).zip
SCP_FONT_FILES := 'SauceCodeProNerdFontMono-*.ttf'

# The boot splash: the plymouth theme, the template of its script, its config and the initramfs
# hook that renders it and puts it in the initramfs.
PLYMOUTH_THEME_DIR := /usr/share/plymouth/themes/hyprdebian
PLYMOUTH_SHARE_DIR := /usr/share/hd-plymouth
PLYMOUTH_TEMPLATE_DIR := $(PLYMOUTH_SHARE_DIR)/templates
HOOK_DIR := /usr/share/initramfs-tools/hooks
PLYMOUTH_THEME := $(wildcard plymouth/theme/*)
PLYMOUTH_TEMPLATES := $(wildcard templates/hd-plymouth/*.j2)

# The menu and the message box, compiled in a container. The desktop needs Hyprland, so they are
# only for unstable; the binaries need the libraries of the release they are compiled on.
IMAGE := debian:unstable
PROGRAMS := $(BUILD)/programs
PROGRAM_SOURCES := common/build.sh common/program.mk $(wildcard common/src/*) \
	$(wildcard common/protocols/*) menu/Makefile $(wildcard menu/src/*) message/Makefile \
	$(wildcard message/src/*)

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# make all of menu/Makefile and message/Makefile, in the container. It compiles into a temporary
# directory, so a build that fails leaves no $(PROGRAMS) behind. It pulls the latest $(IMAGE)
# every time.
$(PROGRAMS): $(PROGRAM_SOURCES) | $(BUILD)
	rm -rf $@ $@.tmp
	mkdir -p $@.tmp
	docker run --rm --pull always \
		-e HOST_UID=$$(id -u) \
		-e HOST_GID=$$(id -g) \
		-v $(CURDIR)/common:/src/common:ro \
		-v $(CURDIR)/menu:/src/menu:ro \
		-v $(CURDIR)/message:/src/message:ro \
		-v $(CURDIR)/$@.tmp:/out \
		$(IMAGE) bash /src/common/build.sh
	mv $@.tmp $@

# A download that does not match the checksum is removed, so the next make
# downloads it again.
$(ZIP): URL := $(ZIP_URL)
$(ZIP): SHA256 := $(ZIP_SHA256)
$(SCP_ZIP): URL := $(SCP_ZIP_URL)
$(SCP_ZIP): SHA256 := $(SCP_ZIP_SHA256)
$(ZIP) $(SCP_ZIP): | $(BUILD)
	wget -O $@.tmp $(URL)
	if ! echo '$(SHA256)  $@.tmp' | sha256sum --check --quiet; then \
		echo "$(URL) does not match its checksum" >&2; \
		rm -f $@.tmp; \
		exit 1; \
	fi
	mv $@.tmp $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): $(VERSION_FILE) $(PROGRAMS) $(ZIP) $(SCP_ZIP) $(COMMANDS) hd-mount-helper hd-reboot-notify hd-plymouth-handoff hd-shutdown lib/hd_display.py $(SBIN_COMMANDS) $(TEMPLATES) \
		$(THEME_TEMPLATES) $(THEMES) screensaver.txt $(WALLPAPERS) $(PLYMOUTH_THEME) $(PLYMOUTH_TEMPLATES) \
		plymouth/plymouthd.conf plymouth/initramfs/hd-plymouth $(ETC) $(USR) deb/control \
		deb/copyright deb/triggers deb/preinst deb/postinst deb/postrm $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE) $(BUILD)/zip $(BUILD)/scp-zip
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)/usr/sbin $(STAGE)$(LIBEXEC_DIR) $(STAGE)$(LIB_DIR) \
		$(STAGE)$(TEMPLATE_DIR) $(STAGE)$(THEME_TEMPLATE_DIR) $(STAGE)$(THEME_DIR) \
		$(STAGE)$(SCREENSAVER_DIR) $(STAGE)$(BACKGROUNDS) $(STAGE)$(FONTS) $(STAGE)$(FONTS_DOC) \
		$(STAGE)$(SCP_FONTS) $(STAGE)$(SCP_FONTS_DOC) \
		$(STAGE)$(PLYMOUTH_THEME_DIR) $(STAGE)$(PLYMOUTH_SHARE_DIR) $(STAGE)$(PLYMOUTH_TEMPLATE_DIR) \
		$(STAGE)$(HOOK_DIR)
	install -m 0755 $(COMMANDS) $(STAGE)/usr/bin/
	install -s -m 0755 $(PROGRAMS)/hd-menu $(PROGRAMS)/hd-message $(STAGE)/usr/bin/
	install -m 0755 hd-mount-helper hd-reboot-notify hd-plymouth-handoff hd-shutdown $(STAGE)$(LIBEXEC_DIR)/
	install -m 0644 lib/hd_display.py $(STAGE)$(LIB_DIR)/
	install -m 0755 $(SBIN_COMMANDS) $(STAGE)/usr/sbin/
	install -m 0644 $(TEMPLATES) $(STAGE)$(TEMPLATE_DIR)/
	install -m 0644 $(THEME_TEMPLATES) $(STAGE)$(THEME_TEMPLATE_DIR)/
	for theme in $(THEMES); do \
		install -m 0644 $$theme $(STAGE)$(THEME_DIR)/; \
		echo $(THEME_DIR)/$$(basename $$theme) >> $(STAGE)/DEBIAN/conffiles; \
	done
	install -m 0644 screensaver.txt $(STAGE)$(SCREENSAVER_DIR)/
	install -m 0644 $(WALLPAPERS) $(STAGE)$(BACKGROUNDS)/
	unzip -q $(ZIP) $(FONT_FILES) OFL.txt README.md -d $(BUILD)/zip
	install -m 0644 $(BUILD)/zip/*.ttf $(STAGE)$(FONTS)/
	install -m 0644 $(BUILD)/zip/OFL.txt $(BUILD)/zip/README.md $(STAGE)$(FONTS_DOC)/
	unzip -q $(SCP_ZIP) $(SCP_FONT_FILES) LICENSE.txt README.md -d $(BUILD)/scp-zip
	install -m 0644 $(BUILD)/scp-zip/*.ttf $(STAGE)$(SCP_FONTS)/
	install -m 0644 $(BUILD)/scp-zip/LICENSE.txt $(BUILD)/scp-zip/README.md $(STAGE)$(SCP_FONTS_DOC)/
	install -m 0644 $(PLYMOUTH_THEME) $(STAGE)$(PLYMOUTH_THEME_DIR)/
	install -m 0644 $(PLYMOUTH_TEMPLATES) $(STAGE)$(PLYMOUTH_TEMPLATE_DIR)/
	install -m 0644 plymouth/plymouthd.conf $(STAGE)$(PLYMOUTH_SHARE_DIR)/
	install -m 0755 plymouth/initramfs/hd-plymouth $(STAGE)$(HOOK_DIR)/
	for file in $(ETC); do \
		if [ "$$file" = "$(SUDOERS)" ]; then mode=0440; else mode=0644; fi; \
		install -D -m $$mode $$file $(STAGE)/$$file; \
		echo /$$file >> $(STAGE)/DEBIAN/conffiles; \
	done
	install -d $(STAGE)/etc/skel/.icons $(STAGE)/etc/skel/Pictures/Screenshots
	for file in $(USR); do \
		install -D -m 0644 $$file $(STAGE)/$$file; \
	done
	install -m 0644 deb/copyright $(STAGE)$(DOC)/copyright
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/g' \
		-e 's/@ARCH@/$(ARCH)/' \
		-e "s/@PROGRAM_DEPENDS@/$$(cat $(PROGRAMS)/depends)/" \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	install -m 0644 deb/triggers $(STAGE)/DEBIAN/
	install -m 0755 deb/preinst deb/postinst deb/postrm $(STAGE)/DEBIAN/
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
	$(MAKE) -C menu clean
	$(MAKE) -C message clean
