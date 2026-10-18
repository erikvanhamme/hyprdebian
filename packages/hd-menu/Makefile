# Builds the package hd-menu, a menu in the manner of wofi: a C++ program
# that draws the menu itself, with cairo and librsvg, on a wlr-layer-shell
# surface of the compositor, in the colours, font, border and radius of the
# hd-theme key of /etc/hyprdebian/config.json. It reads the menu as JSON from
# stdin; src/menu.hpp and man/hd-menu.1 describe it.
#
#   make deb    build build/hd-menu_$(DEB_VERSION)_$(ARCH).deb, with the
#               version from version.json. build-hd-menu.sh compiles hd-menu
#               with make all in a $(IMAGE) container, into build/dist;
#               later builds reuse it until a source changes or make clean
#               removes it. It installs:
#                 /usr/bin/hd-menu            the menu
#                 /usr/share/man/man1/hd-menu.1.gz
#                                             the man page in man/
#                 /usr/share/doc/hd-menu/copyright
#                                             deb/copyright
#               with the control file deb/control. Its Depends are the ones
#               dpkg-shlibdeps finds for the binary in the container. Raise
#               the version in version.json for every package that is
#               handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make all    build build/hd-menu here, without a package, to try it:
#
#                 echo '{"banner": "Power off system?", "options": [{"label": "No"}, {"label": "Yes"}]}' | build/hd-menu
#
#               It needs g++, make, pkgconf, libwayland-dev,
#               wayland-protocols, libcairo2-dev and librsvg2-dev on this
#               computer; make deb only needs docker.
#   make clean  remove build/, also the compiled binary of make deb
#
# wayland-scanner, of libwayland-bin, turns the XML of each Wayland protocol
# into a C header and the C tables of its interfaces. wlr-layer-shell is not
# in a Debian package; protocols/ has a copy of it from wlr-protocols. It
# refers to xdg_popup of xdg-shell, so the tables of xdg-shell are linked in
# too.

BUILD := build
PYTHON := python3

DEB_NAME := hd-menu
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
# The container runs on the architecture of this machine.
ARCH := $(shell dpkg --print-architecture)
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_$(ARCH).deb
STAGE := $(BUILD)/deb
DIST := $(BUILD)/dist
DOC := /usr/share/doc/$(DEB_NAME)
# hd-menu is for the desktop, which needs Hyprland, so it is only for
# unstable; the binary needs the libraries of the release it is compiled on.
IMAGE := debian:unstable
# The man pages, named <page>.<section>.
MANPAGES := man/hd-menu.1

PROGRAM := $(BUILD)/hd-menu
PKGS := wayland-client cairo librsvg-2.0
# Without the -dev packages, as for make deb, these stay empty, quietly.
WAYLAND_SCANNER := $(shell pkg-config --variable=wayland_scanner wayland-scanner 2>/dev/null)
XDG_SHELL := $(shell pkg-config --variable=pkgdatadir wayland-protocols 2>/dev/null)/stable/xdg-shell/xdg-shell.xml
LAYER_SHELL := protocols/wlr-layer-shell-unstable-v1.xml

CXXFLAGS ?= -O2 -g
CFLAGS ?= -O2 -g
override CXXFLAGS += -std=c++23 -Wall -Wextra -MMD -MP $(shell pkg-config --cflags $(PKGS) 2>/dev/null) -I$(BUILD)
override CFLAGS += $(shell pkg-config --cflags wayland-client 2>/dev/null)
LDLIBS := $(shell pkg-config --libs $(PKGS) 2>/dev/null)

SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard src/*.hpp)
OBJS := $(SOURCES:src/%.cpp=$(BUILD)/%.o) $(BUILD)/wlr-layer-shell-protocol.o $(BUILD)/xdg-shell-protocol.o

.PHONY: all deb deb-path clean
# Keep the generated C files, to read.
.SECONDARY: $(BUILD)/wlr-layer-shell-protocol.c $(BUILD)/xdg-shell-protocol.c

all: $(PROGRAM)

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# make all, in the container. It compiles into a temporary directory, so a
# build that fails leaves no DIST behind. It pulls the latest $(IMAGE) every
# time.
$(DIST): build-hd-menu.sh Makefile $(SOURCES) $(HEADERS) $(LAYER_SHELL) | $(BUILD)
	rm -rf $@ $@.tmp
	mkdir -p $@.tmp
	docker run --rm --pull always \
		-e HOST_UID=$$(id -u) \
		-e HOST_GID=$$(id -g) \
		-v $(CURDIR):/src:ro \
		-v $(CURDIR)/$@.tmp:/out \
		$(IMAGE) bash /src/build-hd-menu.sh
	mv $@.tmp $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): $(DIST) version.json deb/control deb/copyright $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)$(DOC)
	install -s -m 0755 $(DIST)/hd-menu $(STAGE)/usr/bin/
	install -m 0644 deb/copyright $(STAGE)$(DOC)/
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e 's/@ARCH@/$(ARCH)/' \
		-e "s/@DEPENDS@/$$(cat $(DIST)/depends)/" \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(STAGE) $@

$(PROGRAM): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# The sources include the generated header, so it comes first.
$(BUILD)/%.o: src/%.cpp $(BUILD)/wlr-layer-shell-unstable-v1-client-protocol.h | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/wlr-layer-shell-unstable-v1-client-protocol.h: $(LAYER_SHELL) | $(BUILD)
	$(WAYLAND_SCANNER) client-header $< $@

$(BUILD)/wlr-layer-shell-protocol.c: $(LAYER_SHELL) | $(BUILD)
	$(WAYLAND_SCANNER) private-code $< $@

$(BUILD)/xdg-shell-protocol.c: $(XDG_SHELL) | $(BUILD)
	$(WAYLAND_SCANNER) private-code $< $@

$(BUILD)/%-protocol.o: $(BUILD)/%-protocol.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
