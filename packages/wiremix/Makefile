# Builds the package wiremix, the terminal audio mixer for PipeWire, from a
# release of wiremix. The binary is compiled in a Docker container, so the
# build needs docker and a user that may run it.
#
#   make deb    build build/wiremix_$(DEB_VERSION)_$(ARCH).deb.
#               build-wiremix.sh compiles the release WIREMIX_VERSION in a
#               $(IMAGE) container, into build/dist-$(WIREMIX_VERSION); later
#               builds reuse it, until make clean removes it. It installs:
#                 /usr/bin/wiremix            the mixer
#                 /usr/share/applications/wiremix.desktop
#                                             the desktop entry
#                 /usr/share/doc/wiremix/copyright
#                                             deb/copyright
#                 /usr/share/doc/wiremix/LICENSE-MIT and LICENSE-APACHE
#                                             the licenses of the release
#                 /usr/share/doc/wiremix/README.md
#                                             the readme of the release
#                 /usr/share/doc/wiremix/examples/wiremix.toml
#                                             the reference of the
#                                             configuration file
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

# The release of wiremix, without the v of its tag, and the revision of the
# package of it.
WIREMIX_VERSION := 0.11.0
DEB_REVISION := 1

DEB_NAME := wiremix
DEB_VERSION := $(WIREMIX_VERSION)-$(DEB_REVISION)
# The container runs on the architecture of this machine.
ARCH := $(shell dpkg --print-architecture)
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_$(ARCH).deb
STAGE := $(BUILD)/deb
DIST := $(BUILD)/dist-$(WIREMIX_VERSION)
DOC := /usr/share/doc/$(DEB_NAME)
# The oldest Debian release the package is for, as the binary needs the
# libraries of the release it is compiled on.
IMAGE := debian:trixie

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The container compiles into a temporary directory, so a build that fails
# leaves no DIST behind. It pulls the latest $(IMAGE) every time.
$(DIST): build-wiremix.sh | $(BUILD)
	rm -rf $@ $@.tmp
	mkdir -p $@.tmp
	docker run --rm --pull always \
		-e WIREMIX_VERSION=$(WIREMIX_VERSION) \
		-e HOST_UID=$$(id -u) \
		-e HOST_GID=$$(id -g) \
		-v $(CURDIR)/build-wiremix.sh:/build-wiremix.sh:ro \
		-v $(CURDIR)/$@.tmp:/out \
		$(IMAGE) bash /build-wiremix.sh
	mv $@.tmp $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): $(DIST) deb/control deb/copyright | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)/usr/share/applications \
		$(STAGE)$(DOC)/examples
	install -s -m 0755 $(DIST)/wiremix $(STAGE)/usr/bin/
	install -m 0644 $(DIST)/wiremix.desktop $(STAGE)/usr/share/applications/
	install -m 0644 deb/copyright $(DIST)/LICENSE-MIT $(DIST)/LICENSE-APACHE $(DIST)/README.md \
		$(STAGE)$(DOC)/
	install -m 0644 $(DIST)/wiremix.toml $(STAGE)$(DOC)/examples/
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e 's/@WIREMIX_VERSION@/$(WIREMIX_VERSION)/' \
		-e 's/@ARCH@/$(ARCH)/' \
		-e "s/@DEPENDS@/$$(cat $(DIST)/depends)/" \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
