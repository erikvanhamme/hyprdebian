# Compiles hd-logo, the bouncing logo: a C++ program that draws with Vulkan
# on a wlr-layer-shell surface on every output of the compositor, or with
# --window in one window. The code it shares with the other screensavers is
# in ../common, with the rules in ../common/program.mk.
#
# The logo is hd-logo.svg. rsvg-convert, of librsvg2-bin, renders it 512
# pixels high to build/hd-logo.png, and mask.py turns its alpha channel, with
# all the mipmaps, into the header build/logo_mask.h that src/logo.cpp
# includes; the binary holds the mask, no image files are installed.
#
# make deb of ../Makefile runs ../common/build.sh in a container, which runs
# make all here, and puts hd-logo in the package hd-screensavers.
#
#   make all    build build/hd-logo here, without a package, to try it:
#
#                 build/hd-logo --window
#
#               It compiles in a container of hd-build:unstable, the build
#               image of unstable, so it needs docker, as make deb of
#               ../Makefile; the binary runs here, with libwayland-client0,
#               libvulkan1 and a Vulkan driver on this computer.
#   make clean  remove build/

PROGRAM := hd-logo
# The height of the mask, in pixels; the width follows from the aspect of the SVG.
MASK_HEIGHT := 512
EXTRA_HEADERS := build/logo_mask.h

include ../common/program.mk

ifneq ($(IN_BUILD),)

$(BUILD)/hd-logo.png: hd-logo.svg | $(BUILD)
	rsvg-convert --height $(MASK_HEIGHT) --output $@ $<

$(BUILD)/logo_mask.h: $(BUILD)/hd-logo.png mask.py | $(BUILD)
	python3 mask.py $< $@

endif
