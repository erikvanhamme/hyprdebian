# Builds the package yazi, the terminal file manager, from a release of yazi,
# with hd-yazi, which starts yazi where it was left the last time.
# The binaries are compiled in a Docker container, so the build needs docker
# and a user that may run it.
#
#   make deb    build build/yazi_$(DEB_VERSION)_$(ARCH).deb. build-yazi.sh
#               compiles the release YAZI_VERSION in a $(IMAGE) container,
#               into build/dist-$(YAZI_VERSION); later builds reuse it, until
#               make clean removes it. It installs:
#                 /usr/bin/yazi               the file manager
#                 /usr/bin/ya                 the command line tool of yazi,
#                                             for packages and messages
#                 /usr/bin/hd-yazi            starts yazi in the directory
#                                             where it was left the last time
#                 /usr/share/applications/yazi.desktop
#                                             the desktop entry
#                 /usr/share/pixmaps/yazi.png the icon
#                 /usr/share/bash-completion/completions/{yazi,ya}
#                 /usr/share/zsh/site-functions/{_yazi,_ya}
#                 /usr/share/fish/vendor_completions.d/{yazi,ya}.fish
#                                             the shell completions
#                 /usr/share/doc/yazi/copyright
#                                             deb/copyright
#                 /usr/share/doc/yazi/LICENSE and LICENSE-ICONS
#                                             the licenses of the release
#                 /usr/share/man/man1/hd-yazi.1.gz
#                                             the man page in man/
#                 /etc/skel/.config/yazi/yazi.toml
#                                             the configuration of yazi new
#                                             users get, a conffile
#                 /etc/skel/.config/yazi/plugins/hd-ouch.yazi/main.lua
#                                             the previewer of archives
#                                             with ouch, of that
#                                             configuration, a conffile
#               with the control file deb/control. The version of the
#               package is the release with DEB_REVISION; raise DEB_REVISION
#               for a package of the same release that is handed out, and
#               set it back to 1 for a new release.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/, also the compiled release

BUILD := build

# The release of yazi, without the v of its tag, and the revision of the
# package of it.
YAZI_VERSION := 26.9.1
DEB_REVISION := 2

DEB_NAME := yazi
DEB_VERSION := $(YAZI_VERSION)-$(DEB_REVISION)
# The container runs on the architecture of this machine.
ARCH := $(shell dpkg --print-architecture)
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_$(ARCH).deb
STAGE := $(BUILD)/deb
DIST := $(BUILD)/dist-$(YAZI_VERSION)
DOC := /usr/share/doc/$(DEB_NAME)
# The oldest Debian release the package is for, as the binaries need the libc
# of the release they are compiled on.
IMAGE := debian:trixie
# The commands of HyprDebian in the package, and their man pages, named <page>.<section>.
COMMANDS := hd-yazi
MANPAGES := man/hd-yazi.1
# The files in etc/, installed at the same path as conffiles.
CONFFILES := etc/skel/.config/yazi/yazi.toml etc/skel/.config/yazi/plugins/hd-ouch.yazi/main.lua

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The container compiles into a temporary directory, so a build that fails
# leaves no DIST behind. It pulls the latest $(IMAGE) every time.
$(DIST): build-yazi.sh | $(BUILD)
	rm -rf $@ $@.tmp
	mkdir -p $@.tmp
	docker run --rm --pull always \
		-e YAZI_VERSION=$(YAZI_VERSION) \
		-e HOST_UID=$$(id -u) \
		-e HOST_GID=$$(id -g) \
		-v $(CURDIR)/build-yazi.sh:/build-yazi.sh:ro \
		-v $(CURDIR)/$@.tmp:/out \
		$(IMAGE) bash /build-yazi.sh
	mv $@.tmp $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): $(DIST) $(COMMANDS) $(MANPAGES) $(CONFFILES) deb/control deb/copyright | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)/usr/share/applications \
		$(STAGE)/usr/share/pixmaps $(STAGE)/usr/share/bash-completion/completions \
		$(STAGE)/usr/share/zsh/site-functions $(STAGE)/usr/share/fish/vendor_completions.d \
		$(STAGE)$(DOC)
	install -s -m 0755 $(DIST)/yazi $(DIST)/ya $(STAGE)/usr/bin/
	install -m 0755 $(COMMANDS) $(STAGE)/usr/bin/
	install -m 0644 $(DIST)/yazi.desktop $(STAGE)/usr/share/applications/
	install -m 0644 $(DIST)/logo.png $(STAGE)/usr/share/pixmaps/yazi.png
	install -m 0644 $(DIST)/completions/yazi.bash $(STAGE)/usr/share/bash-completion/completions/yazi
	install -m 0644 $(DIST)/completions/ya.bash $(STAGE)/usr/share/bash-completion/completions/ya
	install -m 0644 $(DIST)/completions/_yazi $(DIST)/completions/_ya $(STAGE)/usr/share/zsh/site-functions/
	install -m 0644 $(DIST)/completions/yazi.fish $(DIST)/completions/ya.fish \
		$(STAGE)/usr/share/fish/vendor_completions.d/
	install -m 0644 deb/copyright $(DIST)/LICENSE $(DIST)/LICENSE-ICONS $(STAGE)$(DOC)/
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	for file in $(CONFFILES); do \
		install -D -m 0644 $$file $(STAGE)/$$file; \
		echo /$$file >> $(STAGE)/DEBIAN/conffiles; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e 's/@YAZI_VERSION@/$(YAZI_VERSION)/' \
		-e 's/@ARCH@/$(ARCH)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
