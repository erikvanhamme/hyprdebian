# Compiles hd-menu, the keyboard-driven menu of the HyprDebian desktop: a C++
# program that draws the menu itself, with cairo and librsvg, on a
# wlr-layer-shell surface of the compositor, in the colours, font, border and
# radius of the hd-theme key of ~/.config/hyprdebian/config.json, else of
# /etc/hyprdebian/config.json. It reads the menu as JSON from stdin;
# src/menu.hpp and ../man/hd-menu.1 describe it.
#
# make deb of ../Makefile runs build-hd-menu.sh in a container, which runs
# make all here, and puts hd-menu in the package hd-desktop.
#
#   make all    build build/hd-menu here, without a package, to try it:
#
#                 echo '{"banner": "Power off system?", "options": [{"label": "No"}, {"label": "Yes"}]}' | build/hd-menu
#
#               It needs g++, make, pkgconf, libwayland-dev,
#               wayland-protocols, libxkbcommon-dev, libcairo2-dev and
#               librsvg2-dev on this computer; make deb of ../Makefile only
#               needs docker.
#   make clean  remove build/
#
# wayland-scanner, of libwayland-bin, turns the XML of each Wayland protocol
# into a C header and the C tables of its interfaces. wlr-layer-shell is not
# in a Debian package; protocols/ has a copy of it from wlr-protocols. It
# refers to xdg_popup of xdg-shell, so the tables of xdg-shell are linked in
# too.

BUILD := build

PROGRAM := $(BUILD)/hd-menu
PKGS := wayland-client xkbcommon cairo librsvg-2.0
# Without the -dev packages, as for make deb of ../Makefile, these stay empty, quietly.
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

.PHONY: all clean
# Keep the generated C files, to read.
.SECONDARY: $(BUILD)/wlr-layer-shell-protocol.c $(BUILD)/xdg-shell-protocol.c

all: $(PROGRAM)

$(BUILD):
	mkdir -p $@

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
