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
#               It compiles in a container of hd-build:unstable, the build
#               image of unstable, so it needs docker, as make deb of
#               ../Makefile; the binary runs here, with libwayland-client0,
#               libvulkan1 and a Vulkan driver on this computer.
#   make clean  remove build/

PROGRAM := hd-pipes

include ../common/program.mk
