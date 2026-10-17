# Builds the package ouch, the compression and decompression tool, from a
# release of ouch. The binary is compiled in a Docker container, so the build
# needs docker and a user that may run it.
#
#   make deb    build build/ouch_$(DEB_VERSION)_$(ARCH).deb. build-ouch.sh
#               compiles the release OUCH_VERSION in a $(IMAGE) container,
#               with the Rust toolchain of Debian, into
#               build/dist-$(OUCH_VERSION); later builds reuse it, until
#               make clean removes it. It installs:
#                 /usr/bin/ouch               the tool
#                 /usr/share/man/man1/ouch*.1.gz
#                                             the man pages of the release
#                 /usr/share/bash-completion/completions/ouch
#                 /usr/share/zsh/site-functions/_ouch
#                 /usr/share/fish/vendor_completions.d/ouch.fish
#                                             the shell completions
#                 /usr/share/doc/ouch/copyright
#                                             deb/copyright
#                 /usr/share/doc/ouch/LICENSE and README.md
#                                             the license and the readme of
#                                             the release
#               with the control file deb/control. Its Depends are the ones
#               dpkg-shlibdeps finds for the binary in the container. The
#               version of the package is the release with DEB_REVISION;
#               raise DEB_REVISION for a package of the same release that is
#               handed out, and set it back to 1 for a new release.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/, also the compiled release

BUILD := build

# The release of ouch, as its tag, and the revision of the package of it.
OUCH_VERSION := 0.8.3
DEB_REVISION := 1

DEB_NAME := ouch
DEB_VERSION := $(OUCH_VERSION)-$(DEB_REVISION)
# The container runs on the architecture of this machine.
ARCH := $(shell dpkg --print-architecture)
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_$(ARCH).deb
STAGE := $(BUILD)/deb
DIST := $(BUILD)/dist-$(OUCH_VERSION)
DOC := /usr/share/doc/$(DEB_NAME)
# ouch needs a newer rustc than the one in trixie, so it is built on sid. The
# Depends from dpkg-shlibdeps say which libc the binary needs.
IMAGE := debian:sid

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The container compiles into a temporary directory, so a build that fails
# leaves no DIST behind. It pulls the latest $(IMAGE) every time.
$(DIST): build-ouch.sh | $(BUILD)
	rm -rf $@ $@.tmp
	mkdir -p $@.tmp
	docker run --rm --pull always \
		-e OUCH_VERSION=$(OUCH_VERSION) \
		-e HOST_UID=$$(id -u) \
		-e HOST_GID=$$(id -g) \
		-v $(CURDIR)/build-ouch.sh:/build-ouch.sh:ro \
		-v $(CURDIR)/$@.tmp:/out \
		$(IMAGE) bash /build-ouch.sh
	mv $@.tmp $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): $(DIST) deb/control deb/copyright | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)/usr/share/man/man1 \
		$(STAGE)/usr/share/bash-completion/completions $(STAGE)/usr/share/zsh/site-functions \
		$(STAGE)/usr/share/fish/vendor_completions.d $(STAGE)$(DOC)
	install -s -m 0755 $(DIST)/ouch $(STAGE)/usr/bin/
	for page in $(DIST)/man/*.1; do \
		gzip -9n < $$page > $(STAGE)/usr/share/man/man1/$$(basename $$page).gz; \
		chmod 0644 $(STAGE)/usr/share/man/man1/$$(basename $$page).gz; \
	done
	install -m 0644 $(DIST)/completions/ouch.bash $(STAGE)/usr/share/bash-completion/completions/ouch
	install -m 0644 $(DIST)/completions/_ouch $(STAGE)/usr/share/zsh/site-functions/
	install -m 0644 $(DIST)/completions/ouch.fish $(STAGE)/usr/share/fish/vendor_completions.d/
	install -m 0644 deb/copyright $(DIST)/LICENSE $(DIST)/README.md $(STAGE)$(DOC)/
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e 's/@OUCH_VERSION@/$(OUCH_VERSION)/' \
		-e 's/@ARCH@/$(ARCH)/' \
		-e "s/@DEPENDS@/$$(cat $(DIST)/depends)/" \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
