# Builds the package hd-audacious, the audio player audacious with a terminal interface, from the
# HyprDebian forks of audacious and audacious-plugins. The player is compiled in a Docker
# container, so the build needs docker and a user that may run it.
#
#   make deb    build build/hd-audacious_$(DEB_VERSION)_$(ARCH).deb, with the version from
#               version.json. build-hd-audacious.sh compiles the commits AUDACIOUS_COMMIT and
#               PLUGINS_COMMIT in a $(IMAGE) container, into build/dist-<commits>; later builds
#               reuse it, until make clean removes it. It installs:
#                 /usr/bin/audacious          starts audacious with its terminal interface, in
#                                             a kitty window when there is no terminal and
#                                             kitty is installed
#                 /usr/bin/audtool            the command line remote control, a link to the
#                                             binary in /usr/lib/hd-audacious/bin
#                 /usr/lib/hd-audacious/bin/audacious, audtool
#                                             the binaries
#                 /usr/lib/hd-audacious/libaudcore.so.6, libaudtag.so.4
#                                             the private libraries
#                 /usr/lib/hd-audacious/audacious/
#                                             the plugins: the terminal interface, MPRIS,
#                                             FLAC, MP3, WAV, PipeWire, m3u, pls and audpl
#                 /usr/share/applications/audacious.desktop, /usr/share/icons/hicolor/...,
#                 /usr/share/metainfo/audacious.metainfo.xml, /usr/share/audacious/,
#                 /usr/share/locale/...       the desktop entry, icons, data and translations
#                 /usr/share/man/man1/audacious.1.gz, audtool.1.gz
#                                             the man pages of the forks
#                 /usr/share/man/man7/hd-audacious.7.gz
#                                             the man page in man/
#                 /usr/share/doc/hd-audacious/copyright
#                                             deb/copyright
#                 /usr/share/doc/hd-audacious/COPYING-audacious, COPYING-audacious-plugins
#                                             the licenses of the forks
#               with the control file deb/control. Its Depends are the ones dpkg-shlibdeps
#               finds for the binaries and plugins in the container, and those of
#               /usr/bin/audacious. Raise the version in version.json for every package that is
#               handed out.
#   make deb LOCAL_SRC=<dir>
#               the same, from the checkouts <dir>/audacious and <dir>/audacious-plugins, with
#               the changes that are not committed or pushed yet, into build/dist-local, which
#               is compiled again on every build. For trying changes to the forks only: hand
#               out packages of commits.
#   make deb-path
#               print the path of the package that make deb builds, relative to this directory,
#               for the Makefile in the root
#   make clean  remove build/, also the compiled player

BUILD := build
PYTHON := python3

# The commits of the forks, which add the terminal interface to the release in UPSTREAM_VERSION.
AUDACIOUS_COMMIT := 9577210f61b25d7097f0b3af2b2d61e9d4c810e8
PLUGINS_COMMIT := f8704c08418ad3a78e5d6efb9677b68c3a4c8e7d
UPSTREAM_VERSION := 4.6.2

DEB_NAME := hd-audacious
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
# The container runs on the architecture of this machine.
ARCH := $(shell dpkg --print-architecture)
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_$(ARCH).deb
STAGE := $(BUILD)/deb
# The directory with local checkouts of the forks, for make deb LOCAL_SRC=<dir>; empty for the
# commits.
LOCAL_SRC :=
ifeq ($(LOCAL_SRC),)
DIST := $(BUILD)/dist-$(shell echo $(AUDACIOUS_COMMIT) | cut -c1-12)-$(shell echo $(PLUGINS_COMMIT) | cut -c1-12)
SOURCE := the commits $(shell echo $(AUDACIOUS_COMMIT) | cut -c1-12) and $(shell echo $(PLUGINS_COMMIT) | cut -c1-12)
SOURCE_MOUNTS :=
DIST_FORCE :=
else
DIST := $(BUILD)/dist-local
SOURCE := local checkouts
SOURCE_MOUNTS := -v $(abspath $(LOCAL_SRC))/audacious:/src/audacious:ro \
	-v $(abspath $(LOCAL_SRC))/audacious-plugins:/src/audacious-plugins:ro
DIST_FORCE := FORCE
endif
DOC := /usr/share/doc/$(DEB_NAME)
# The player is compiled on the release it is for, as the desktop: unstable.
IMAGE := debian:unstable
# The command in front of the binary, and the man pages, named <page>.<section>.
COMMANDS := audacious
MANPAGES := man/hd-audacious.7

.PHONY: deb deb-path clean FORCE

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The container compiles into a temporary directory, so a build that fails leaves no DIST behind.
# It pulls the latest $(IMAGE) every time.
$(DIST): build-hd-audacious.sh $(DIST_FORCE) | $(BUILD)
	rm -rf $@ $@.tmp
	mkdir -p $@.tmp
	docker run --rm --pull always \
		-e AUDACIOUS_COMMIT=$(AUDACIOUS_COMMIT) \
		-e PLUGINS_COMMIT=$(PLUGINS_COMMIT) \
		-e HOST_UID=$$(id -u) \
		-e HOST_GID=$$(id -g) \
		-v $(CURDIR)/build-hd-audacious.sh:/build-hd-audacious.sh:ro \
		-v $(CURDIR)/$@.tmp:/out \
		$(SOURCE_MOUNTS) \
		$(IMAGE) bash /build-hd-audacious.sh
	mv $@.tmp $@

# The package is built from a staged root in build/deb, from the files the container installed.
# The man pages of the forks are compressed as Debian wants them. dpkg-deb makes all files owned
# by root.
$(DEB): $(DIST) Makefile version.json $(COMMANDS) $(MANPAGES) deb/control deb/copyright | $(BUILD)
	rm -rf $(STAGE)
	cp -a $(DIST)/root $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)$(DOC)
	install -m 0755 $(COMMANDS) $(STAGE)/usr/bin/
	ln -s ../lib/hd-audacious/bin/audtool $(STAGE)/usr/bin/audtool
	gzip -9n $(STAGE)/usr/share/man/man1/audacious.1 $(STAGE)/usr/share/man/man1/audtool.1
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	install -m 0644 deb/copyright $(DIST)/COPYING-audacious $(DIST)/COPYING-audacious-plugins \
		$(STAGE)$(DOC)/
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e 's/@UPSTREAM_VERSION@/$(UPSTREAM_VERSION)/' \
		-e 's/@SOURCE@/$(SOURCE)/' \
		-e 's/@ARCH@/$(ARCH)/' \
		-e "s/@DEPENDS@/$$(cat $(DIST)/depends)/" \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)

FORCE:
