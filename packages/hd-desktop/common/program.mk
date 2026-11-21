# The rules that compile hd-menu and hd-message: included by menu/Makefile
# and message/Makefile, which set PROGRAM, the name of the binary, first.
# Each program has its own sources in src/; both link the sources in
# ../common/src, the shared code, into the binary build/$(PROGRAM).
#
#   make all    build build/$(PROGRAM) in a container of hd-build:unstable,
#               the build image of unstable from buildenv/Dockerfile, which
#               it builds first, as the user who runs make. The binary runs
#               on a computer with unstable and the libraries it links to:
#               libwayland-client0, libxkbcommon0, libcairo2 and librsvg2-2
#   make clean  remove build/
#
# make all IN_BUILD=1 compiles without a container, as ../common/build.sh and
# the container of make all do. It needs g++, make, pkgconf, libwayland-dev,
# wayland-protocols, libxkbcommon-dev, libcairo2-dev, librsvg2-dev and
# libdav1d-dev, which hd-build:unstable has.
#
# wayland-scanner, of libwayland-bin, turns the XML of each Wayland protocol
# into a C header and the C tables of its interfaces. wlr-layer-shell is not
# in a Debian package; ../common/protocols has a copy of it from
# wlr-protocols. It refers to xdg_popup of xdg-shell, so the tables of
# xdg-shell are linked in too.

BUILD := build
COMMON := ../common

ifeq ($(IN_BUILD),)

# Here, make all runs make all IN_BUILD=1 in the container, with the package directory, which has
# the program and ../common, at the same path.
SUITE := unstable
PACKAGE_DIR := $(abspath ..)

include ../../../buildenv/buildenv.mk

.PHONY: all clean

all:
	$(call build-image,$(SUITE))
	$(call in-build,$(SUITE), \
		--user $$(id -u):$$(id -g) \
		-v $(PACKAGE_DIR):$(PACKAGE_DIR) \
		-w $(CURDIR), \
		make -j$$(nproc) all IN_BUILD=1)

clean:
	rm -rf $(BUILD)

else

PKGS := wayland-client xkbcommon cairo librsvg-2.0
# Only make clean goes without the -dev packages. Anything else stops here with the message of
# pkg-config, as a package it cannot find leaves all the flags below empty.
ifneq ($(MAKECMDGOALS),clean)
PKG_ERRORS := $(shell pkg-config --print-errors --exists $(PKGS) wayland-scanner wayland-protocols 2>&1)
ifneq ($(PKG_ERRORS),)
$(error $(PKG_ERRORS))
endif
endif

# Without the -dev packages, as for make clean, these stay empty, quietly.
WAYLAND_SCANNER := $(shell pkg-config --variable=wayland_scanner wayland-scanner 2>/dev/null)
XDG_SHELL := $(shell pkg-config --variable=pkgdatadir wayland-protocols 2>/dev/null)/stable/xdg-shell/xdg-shell.xml
LAYER_SHELL := $(COMMON)/protocols/wlr-layer-shell-unstable-v1.xml

CXXFLAGS ?= -O2 -g
CFLAGS ?= -O2 -g
override CXXFLAGS += -std=c++23 -Wall -Wextra -MMD -MP $(shell pkg-config --cflags $(PKGS) 2>/dev/null) \
	-Isrc -I$(COMMON)/src -I$(BUILD)
override CFLAGS += $(shell pkg-config --cflags wayland-client 2>/dev/null)
LDLIBS := $(shell pkg-config --libs $(PKGS) 2>/dev/null)

# The objects of the shared sources are named common-*.o, so that they cannot clash with those of
# the program.
SOURCES := $(wildcard src/*.cpp)
COMMON_SOURCES := $(wildcard $(COMMON)/src/*.cpp)
OBJS := $(SOURCES:src/%.cpp=$(BUILD)/%.o) $(COMMON_SOURCES:$(COMMON)/src/%.cpp=$(BUILD)/common-%.o) \
	$(BUILD)/wlr-layer-shell-protocol.o $(BUILD)/xdg-shell-protocol.o

.PHONY: all clean
# Keep the generated C files, to read.
.SECONDARY: $(BUILD)/wlr-layer-shell-protocol.c $(BUILD)/xdg-shell-protocol.c

all: $(BUILD)/$(PROGRAM)

$(BUILD):
	mkdir -p $@

$(BUILD)/$(PROGRAM): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# The sources include the generated header, so it comes first.
$(BUILD)/%.o: src/%.cpp $(BUILD)/wlr-layer-shell-unstable-v1-client-protocol.h | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/common-%.o: $(COMMON)/src/%.cpp $(BUILD)/wlr-layer-shell-unstable-v1-client-protocol.h | $(BUILD)
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

endif
