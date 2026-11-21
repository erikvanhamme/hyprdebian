# Compiles hd-menu, the keyboard-driven menu of the HyprDebian desktop: a C++
# program that draws the menu itself, with cairo and librsvg, on a
# wlr-layer-shell surface of the compositor, in the colours, font, border and
# radius of the hd-theme key of ~/.config/hyprdebian/config.json, else of
# /etc/hyprdebian/config.json. It reads the menu as JSON from stdin;
# src/menu.hpp and ../man/hd-menu.1 describe it. The code it shares with
# hd-message is in ../common, with the rules in ../common/program.mk.
#
# make deb of ../Makefile runs ../common/build.sh in a container, which runs
# make all here, and puts hd-menu in the package hd-desktop.
#
#   make all    build build/hd-menu here, without a package, to try it:
#
#                 echo '{"banner": "Power off system?", "options": [{"label": "No"}, {"label": "Yes"}]}' | build/hd-menu
#
#               It compiles in a container of hd-build:unstable, the build
#               image of unstable, so it needs docker, as make deb of
#               ../Makefile; the binary runs here, with libwayland-client0,
#               libxkbcommon0, libcairo2 and librsvg2-2 on this computer.
#   make clean  remove build/

PROGRAM := hd-menu

include ../common/program.mk
