# Builds the package hd-screensavers, the optional screensavers of the
# HyprDebian desktop, drawn with Vulkan. Each screensaver is a part of the
# package, with its sources in a directory of its own and the code they share
# in common/.
#
#   make deb    build build/hd-screensavers_$(DEB_VERSION)_$(ARCH).deb, with
#               the version from version.json. It installs:
#                 /usr/bin/hd-pipes           the 3D pipes, compiled from
#                                             pipes/; common/build.sh
#                                             compiles every screensaver,
#                                             with the code they share in
#                                             common/, with make all of
#                                             their Makefiles in one
#                                             $(IMAGE) container, into
#                                             build/programs; later builds
#                                             reuse them until a source
#                                             changes or make clean removes
#                                             them
#                 /usr/share/doc/hd-screensavers/copyright
#                                             deb/copyright, with the notice
#                                             of pipes.c
#                 /usr/share/man/man*/*.gz    the man pages in man/
#               with the control file deb/control, whose Depends are
#               hd-desktop, the Vulkan loader, the Vulkan drivers of mesa and
#               the libraries of the screensavers that dpkg-shlibdeps finds in
#               the container, and the maintainer scripts deb/postinst and
#               deb/postrm, which set the key hd-screensaver of the
#               configuration of the system to hd-pipes when it is unset, and
#               remove it when it still is hd-pipes. Raise the version in
#               version.json for every change to the package.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make test   compile each tests/test_*.cpp with TEST_SOURCES into
#               build/tests/ and run it; with g++ on this computer, as the
#               tests need no Wayland or Vulkan
#   make clean  remove build/, also the compiled screensavers, and
#               pipes/build/ of make all in pipes/

BUILD := build
PYTHON := python3

DEB_NAME := hd-screensavers
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
# The screensavers are compiled, so the package is for the architecture of this machine, on which
# the container runs.
ARCH := $(shell dpkg --print-architecture)
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_$(ARCH).deb
STAGE := $(BUILD)/deb
DOC := /usr/share/doc/$(DEB_NAME)
# The binaries of the screensavers, installed in /usr/bin.
BINARIES := hd-pipes
# The man pages, named <page>.<section>.
MANPAGES := man/hd-pipes.6

# The screensavers, compiled in a container. They are for the desktop, which needs Hyprland, so
# only for unstable; the binaries need the libraries of the release they are compiled on.
IMAGE := debian:unstable
PROGRAMS := $(BUILD)/programs
PROGRAM_SOURCES := common/build.sh common/program.mk $(wildcard common/src/*) $(wildcard common/protocols/*) \
	pipes/Makefile $(wildcard pipes/src/*)

# The tests: each tests/test_<name>.cpp is a binary of its own, linked with TEST_SOURCES, the
# sources they test, which have no Wayland or Vulkan in them: the grid walk, the meshes and the
# options.
TESTS := $(wildcard tests/test_*.cpp)
TEST_SOURCES := pipes/src/walk.cpp pipes/src/mesh.cpp pipes/src/settings.cpp common/src/options.cpp common/src/json.cpp
TEST_BINARIES := $(TESTS:tests/%.cpp=$(BUILD)/tests/%)
# The structs are filled with designated initializers, as in common/program.mk, which leave the
# members they skip zero; -Wextra would warn about each.
TEST_CXXFLAGS := -std=c++23 -O2 -g -Wall -Wextra -Wno-missing-field-initializers -Ipipes/src -Icommon/src

.PHONY: deb deb-path test clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD) $(BUILD)/tests:
	mkdir -p $@

# make all of the Makefile of every screensaver, in the container. It compiles into a temporary
# directory, so a build that fails leaves no $(PROGRAMS) behind. It pulls the latest $(IMAGE)
# every time.
$(PROGRAMS): $(PROGRAM_SOURCES) | $(BUILD)
	rm -rf $@ $@.tmp
	mkdir -p $@.tmp
	docker run --rm --pull always \
		-e HOST_UID=$$(id -u) \
		-e HOST_GID=$$(id -g) \
		-v $(CURDIR)/common:/src/common:ro \
		-v $(CURDIR)/pipes:/src/pipes:ro \
		-v $(CURDIR)/$@.tmp:/out \
		$(IMAGE) bash /src/common/build.sh
	mv $@.tmp $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json $(PROGRAMS) deb/control deb/copyright deb/postinst deb/postrm $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)/usr/bin $(STAGE)$(DOC)
	install -s -m 0755 $(addprefix $(PROGRAMS)/,$(BINARIES)) $(STAGE)/usr/bin/
	install -m 0644 deb/copyright $(STAGE)$(DOC)/copyright
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e 's/@ARCH@/$(ARCH)/' \
		-e "s/@PROGRAM_DEPENDS@/$$(cat $(PROGRAMS)/depends)/" \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	install -m 0755 deb/postinst deb/postrm $(STAGE)/DEBIAN/
	dpkg-deb --root-owner-group --build $(STAGE) $@

$(BUILD)/tests/%: tests/%.cpp $(TEST_SOURCES) | $(BUILD)/tests
	$(CXX) $(TEST_CXXFLAGS) -o $@ $< $(TEST_SOURCES)

test: $(TEST_BINARIES)
	for test in $(TEST_BINARIES); do \
		$$test || exit 1; \
	done

clean:
	rm -rf $(BUILD)
	$(MAKE) -C pipes clean
