# Compiles hd-message, the message box of the HyprDebian desktop: a C++
# program that draws a title, a text and a row of buttons itself, with cairo
# and librsvg, on a wlr-layer-shell surface of the compositor, in the
# colours, font, border and radius of the hd-theme key of
# ~/.config/hyprdebian/config.json, else of /etc/hyprdebian/config.json. It
# reads the message as JSON from stdin; src/message.hpp and
# ../man/hd-message.1 describe it. The code it shares with hd-menu is in
# ../common, with the rules in ../common/program.mk.
#
# make deb of ../Makefile runs ../common/build.sh in a container, which runs
# make all here, and puts hd-message in the package hd-desktop.
#
#   make all    build build/hd-message here, without a package, to try it:
#
#                 echo '{"title": "Reboot system?", "text": "Unsaved work is lost.", "buttons": [{"label": "No"}, {"label": "Yes"}]}' | build/hd-message
#
#               It needs g++, make, pkgconf, libwayland-dev,
#               wayland-protocols, libxkbcommon-dev, libcairo2-dev and
#               librsvg2-dev on this computer; make deb of ../Makefile only
#               needs docker.
#   make clean  remove build/

PROGRAM := hd-message

include ../common/program.mk
