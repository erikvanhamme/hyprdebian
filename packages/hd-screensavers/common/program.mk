# The rules that compile a screensaver: included by the Makefile of each, as
# pipes/Makefile, which sets PROGRAM, the name of the binary, first. Each
# screensaver has its own sources in src/; all link the sources in
# ../common/src, the shared code, into the binary build/$(PROGRAM).
#
#   make all    build build/$(PROGRAM) in a container of hd-build:unstable,
#               the build image of unstable from buildenv/Dockerfile, which
#               it builds first, as the user who runs make. The binary runs
#               on a computer with unstable and the libraries it links to:
#               libwayland-client0, libvulkan1 and a Vulkan driver, as
#               mesa-vulkan-drivers
#   make clean  remove build/
#
# make all IN_BUILD=1 compiles without a container, as ../common/build.sh and
# the container of make all do. It needs g++, make, pkgconf, libwayland-dev,
# wayland-protocols, libvulkan-dev and glslang-tools, which hd-build:unstable
# has.
#
# wayland-scanner, of libwayland-bin, turns the XML of each Wayland protocol
# into a C header and the C tables of its interfaces. wlr-layer-shell is not
# in a Debian package; ../common/protocols has a copy of it from
# wlr-protocols. xdg-shell is for the window of --window, and wlr-layer-shell
# refers to its xdg_popup too.
#
# The shaders are GLSL, src/<name>.vert and src/<name>.frag. glslangValidator
# compiles each to SPIR-V in a C header, build/<name>.vert.h with the array
# <name>_vert, that the sources include; the binary holds them, no shader
# files are installed.

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

PKGS := wayland-client vulkan
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
GLSLANG := glslangValidator

CXXFLAGS ?= -O2 -g
CFLAGS ?= -O2 -g
# VK_USE_PLATFORM_WAYLAND_KHR makes vulkan.h declare the Wayland surfaces. The structs of Vulkan
# and the listeners of Wayland are filled with designated initializers, which leave the members
# they skip zero; -Wextra would warn about each.
override CXXFLAGS += -std=c++23 -Wall -Wextra -Wno-missing-field-initializers -MMD -MP -DVK_USE_PLATFORM_WAYLAND_KHR \
	$(shell pkg-config --cflags $(PKGS) 2>/dev/null) \
	-Isrc -I$(COMMON)/src -I$(BUILD)
override CFLAGS += $(shell pkg-config --cflags wayland-client 2>/dev/null)
LDLIBS := $(shell pkg-config --libs $(PKGS) 2>/dev/null)

# The objects of the shared sources are named common-*.o, so that they cannot clash with those of
# the program.
SOURCES := $(wildcard src/*.cpp)
COMMON_SOURCES := $(wildcard $(COMMON)/src/*.cpp)
OBJS := $(SOURCES:src/%.cpp=$(BUILD)/%.o) $(COMMON_SOURCES:$(COMMON)/src/%.cpp=$(BUILD)/common-%.o) \
	$(BUILD)/wlr-layer-shell-protocol.o $(BUILD)/xdg-shell-protocol.o
# The generated headers, which the sources include, so they come first.
SHADERS := $(wildcard src/*.vert src/*.frag)
HEADERS := $(BUILD)/wlr-layer-shell-unstable-v1-client-protocol.h $(BUILD)/xdg-shell-client-protocol.h \
	$(SHADERS:src/%=$(BUILD)/%.h)

.PHONY: all clean
# Keep the generated C files, to read.
.SECONDARY: $(BUILD)/wlr-layer-shell-protocol.c $(BUILD)/xdg-shell-protocol.c

all: $(BUILD)/$(PROGRAM)

$(BUILD):
	mkdir -p $@

$(BUILD)/$(PROGRAM): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: src/%.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/common-%.o: $(COMMON)/src/%.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/wlr-layer-shell-unstable-v1-client-protocol.h: $(LAYER_SHELL) | $(BUILD)
	$(WAYLAND_SCANNER) client-header $< $@

$(BUILD)/xdg-shell-client-protocol.h: $(XDG_SHELL) | $(BUILD)
	$(WAYLAND_SCANNER) client-header $< $@

$(BUILD)/wlr-layer-shell-protocol.c: $(LAYER_SHELL) | $(BUILD)
	$(WAYLAND_SCANNER) private-code $< $@

$(BUILD)/xdg-shell-protocol.c: $(XDG_SHELL) | $(BUILD)
	$(WAYLAND_SCANNER) private-code $< $@

$(BUILD)/%-protocol.o: $(BUILD)/%-protocol.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# pipe.vert becomes the array pipe_vert.
$(BUILD)/%.h: src/% | $(BUILD)
	$(GLSLANG) -V --target-env vulkan1.3 --vn $(subst .,_,$*) -o $@ $<

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

endif
