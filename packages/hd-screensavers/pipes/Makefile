# Compiles hd-pipes, the 3D pipes of Windows NT after pipes.c of
# xscreensaver: a C++ program that draws with Vulkan on a wlr-layer-shell
# surface on every output of the compositor, or with --window in one
# window. The code it shares with the other screensavers is in ../common,
# with the rules in ../common/program.mk.
#
# make deb of ../Makefile runs ../common/build.sh in a container, which runs
# make all here, and puts hd-pipes in the package hd-screensavers.
#
#   make all    build build/hd-pipes here, without a package, to try it:
#
#                 build/hd-pipes --window
#
#               It needs g++, make, pkgconf, libwayland-dev,
#               wayland-protocols, libvulkan-dev and glslang-tools on this
#               computer; make deb of ../Makefile only needs docker.
#   make clean  remove build/

PROGRAM := hd-pipes

include ../common/program.mk
