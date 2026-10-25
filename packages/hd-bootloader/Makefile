# Builds the hyprdebian EFI bootloader, its packages hd-bootloader-gfx and
# hd-bootloader-txt, and a FAT32 disk image to boot it in QEMU.
#
#   make        build build/bootx64-<frontend>.efi for every frontend in
#               FRONTENDS, the core (bootloader.cpp) linked with that
#               frontend (<frontend>.cpp), and build/assets.efi, the stub that
#               update-bootloader.py adds the assets to
#   make esp    run update-bootloader.py into build/esp/ with the defaults of
#               the package ESP_PACKAGE, deb/$(ESP_PACKAGE)/defaults.json:
#               EFI/BOOT/BOOTX64.EFI, the bootloader of its frontend;
#               EFI/BOOT/assets.efi, the stub with the config, the menu rows,
#               the DKMS MOK (/var/lib/dkms/mok.pub) when there is one and the
#               assets of that frontend added as sections; and the UKIs of the
#               4 most recent kernels in /boot with a ZFS module. make esp ESP_PACKAGE=txt tries the text
#               frontend.
#   make image  build build/esp.img (1GB FAT32) with build/esp/
#   make deb    build build/hd-bootloader-<package>_$(DEB_VERSION)_amd64.deb
#               for every package in PACKAGES, with the version from
#               version.json. Only one of them can be installed: each
#               provides, conflicts with and replaces hd-bootloader. Each
#               installs:
#                 /usr/lib/hd-bootloader/           bootx64-<frontend>.efi,
#                                                   assets.efi,
#                                                   update-bootloader.py,
#                                                   mkuki.py, the Python
#                                                   scripts of the frontend
#                                                   and update-esps, which
#                                                   runs the one below for
#                                                   every ESP
#                 /usr/sbin/hd-bootloader-update    a link to
#                                                   update-bootloader.py
#                 /usr/share/hd-bootloader/defaults.json
#                                                   deb/<package>/defaults.json,
#                                                   the default settings
#                 /etc/kernel/postinst.d/zz-hd-bootloader
#                 /etc/kernel/postrm.d/zz-hd-bootloader
#                 /etc/initramfs/post-update.d/zz-hd-bootloader
#                                                   hooks from deb/etc that
#                                                   run update-esps when a
#                                                   kernel or initrd changes,
#                                                   conffiles
#                 /usr/share/man/man7/hd-bootloader.7.gz
#                 /usr/share/man/man8/hd-bootloader-update.8.gz
#                                                   the man pages in man/
#               with the control file deb/<package>/control and the postinst
#               deb/postinst, which stores the default settings in the key
#               hd-bootloader of /etc/hyprdebian/config.json with
#               hd-config-update. It does not update the ESPs. The gfx
#               package depends on hd-desktop for the wallpaper and the
#               font; the txt package needs neither. Raise the
#               version in version.json for every package that is handed
#               out.
#   make deb-path
#               print the paths of the packages that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3
# A frontend shows the menu; see bootloader.hpp. Each has a <frontend>.cpp
# here, a <frontend>.py that builds its assets, which with its helpers is in
# SCRIPTS_<frontend>, and a package in PACKAGES, whose FRONTEND_<package> it
# is, with deb/<package>/ holding its control file and default settings.
FRONTENDS := graphical text
PACKAGES := gfx txt
FRONTEND_gfx := graphical
FRONTEND_txt := text
TARGETS := $(FRONTENDS:%=$(BUILD)/bootx64-%.efi)
ASSETS_STUB := $(BUILD)/assets.efi
ESP := $(BUILD)/esp
# The package whose frontend make esp writes to build/esp/, with its defaults.
ESP_PACKAGE := gfx
ESP_FRONTEND := $(FRONTEND_$(ESP_PACKAGE))
ESP_CONFIG := deb/$(ESP_PACKAGE)/defaults.json
# A stamp per package, so that make esp runs again when ESP_PACKAGE changes.
ESP_STAMP := $(BUILD)/esp-$(ESP_PACKAGE).stamp
IMAGE := $(BUILD)/esp.img
# Each UKI holds a whole initrd, which can be over 100MB.
IMAGE_SIZE := 1G

# The Python scripts of every package, and those of each frontend.
CORE_SCRIPTS := update-bootloader.py mkuki.py
SCRIPTS_graphical := graphical.py text2png.py png2bin.py menubox.py
SCRIPTS_text := text.py
SCRIPTS := $(CORE_SCRIPTS) $(foreach frontend,$(FRONTENDS),$(SCRIPTS_$(frontend)))
# The wallpaper that deb/gfx/defaults.json names, as the package hd-desktop
# installs it.
WALLPAPER := /usr/share/backgrounds/hyprdebian/emerald-1920x1080.png
# What update-bootloader.py reads for a frontend, besides its config.
ESP_INPUTS_graphical := $(WALLPAPER)
ESP_INPUTS_text :=

# What update-bootloader.py reads.
ESP_SOURCES := $(SCRIPTS) $(ESP_CONFIG) $(ESP_INPUTS_$(ESP_FRONTEND)) \
	$(wildcard /boot/vmlinuz-* /boot/initrd.img-* /var/lib/dkms/mok.pub)

DEB_NAME := hd-bootloader
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEBS := $(PACKAGES:%=$(BUILD)/$(DEB_NAME)-%_$(DEB_VERSION)_amd64.deb)
DEB_LIB := /usr/lib/$(DEB_NAME)
DEB_SHARE := /usr/share/$(DEB_NAME)
# The kernel and initramfs hooks, below deb/ at the path they are installed to.
DEB_HOOKS := /etc/kernel/postinst.d/zz-hd-bootloader \
	/etc/kernel/postrm.d/zz-hd-bootloader \
	/etc/initramfs/post-update.d/zz-hd-bootloader
# The man pages, named <page>.<section>.
MANPAGES := man/hd-bootloader.7 man/hd-bootloader-update.8

CXX := clang++
LD := lld-link
MKFS_FAT := /usr/sbin/mkfs.fat
GNU_EFI_INC := /usr/include/efi

# UEFI binaries are PE32+ images with the Microsoft x64 calling convention,
# so compile for the Windows target and link with lld-link. The UEFI types
# come from the headers of the Debian package gnu-efi; nothing of gnu-efi is
# linked in. There is no C or C++ runtime; the only other headers used are
# the <stdint.h> and <stddef.h> that come with clang.
#
# The efibind.h of gnu-efi 3.0 (trixie) only leaves the int types to
# <stdint.h> when __STDC_VERSION__ is C99 or later. C++ does not define it, so
# efibind.h defines them again, int8_t as char, which clashes with the
# signed char of <stdint.h>. Defining it as C17 skips those typedefs and
# changes nothing else; gnu-efi 4.0 does not need it, but takes it as well.
CXXFLAGS := \
	-D__STDC_VERSION__=201710L \
	--target=x86_64-unknown-windows \
	-std=c++23 \
	-ffreestanding \
	-fshort-wchar \
	-fno-exceptions \
	-fno-rtti \
	-fno-stack-protector \
	-fno-stack-check \
	-mno-stack-arg-probe \
	-mno-red-zone \
	-mgeneral-regs-only \
	-O2 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-isystem $(GNU_EFI_INC) \
	-isystem $(GNU_EFI_INC)/x86_64

LDFLAGS := \
	-subsystem:efi_application \
	-entry:efi_main \
	-nodefaultlib

.PHONY: all esp image deb deb-path clean

all: $(TARGETS) $(ASSETS_STUB)

$(BUILD):
	mkdir -p $@

$(BUILD)/%.obj: %.cpp bootloader.hpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/bootx64-%.efi: $(BUILD)/bootloader.obj $(BUILD)/%.obj
	$(LD) $(LDFLAGS) -out:$@ $^

# Made by chained pattern rules, so make would delete them as intermediate
# files and rebuild them every time.
.SECONDARY: $(BUILD)/bootloader.obj $(FRONTENDS:%=$(BUILD)/%.obj)

# ukify adds the sections to the headers of the stub, but lld-link leaves
# only room for the stub's own one in the headers it pads to the file
# alignment of 512 bytes. With a file alignment of 4096 there is room for
# dozens.
$(ASSETS_STUB): $(BUILD)/assets.obj
	$(LD) $(LDFLAGS) -filealign:4096 -out:$@ $^

esp: $(ESP_STAMP)

# update-bootloader.py picks the bootx64-<frontend>.efi in build/.
$(ESP_STAMP): $(BUILD)/bootx64-$(ESP_FRONTEND).efi $(ASSETS_STUB) $(ESP_SOURCES) | $(BUILD)
	rm -f $(BUILD)/esp-*.stamp
	$(PYTHON) update-bootloader.py --esp $(ESP) --config $(ESP_CONFIG) \
		--bootloader-dir $(BUILD) --assets-stub $(ASSETS_STUB)
	touch $@

$(WALLPAPER):
	@echo "$@ is missing: install the package hd-desktop" >&2
	@exit 1

image: $(IMAGE)

$(IMAGE): $(ESP_STAMP)
	rm -f $@
	truncate -s $(IMAGE_SIZE) $@
	$(MKFS_FAT) -F 32 -n HYPRDEBIAN $@
	mcopy -s -m -i $@ $(ESP)/EFI ::/

deb: $(DEBS)

deb-path:
	@echo $(DEBS)

# Each package is built from a staged root in build/deb-<package>.
# update-bootloader.py finds its helpers, bootx64-<frontend>.efi and
# assets.efi next to itself, also through the link. dpkg-deb makes all files
# owned by root. The bootloader and the scripts of the frontend are
# prerequisites through secondary expansion, which expands $$* to the
# package.
.SECONDEXPANSION:
$(BUILD)/$(DEB_NAME)-%_$(DEB_VERSION)_amd64.deb: version.json \
		$(BUILD)/bootx64-$$(FRONTEND_$$*).efi $(ASSETS_STUB) $(CORE_SCRIPTS) \
		$$(SCRIPTS_$$(FRONTEND_$$*)) update-esps deb/%/control deb/%/defaults.json \
		deb/postinst $(addprefix deb,$(DEB_HOOKS)) $(MANPAGES) | $(BUILD)
	rm -rf $(BUILD)/deb-$*
	install -d $(BUILD)/deb-$*/DEBIAN $(BUILD)/deb-$*$(DEB_LIB) $(BUILD)/deb-$*/usr/sbin \
		$(BUILD)/deb-$*$(DEB_SHARE)
	install -m 0644 $(BUILD)/bootx64-$(FRONTEND_$*).efi $(ASSETS_STUB) $(BUILD)/deb-$*$(DEB_LIB)/
	install -m 0755 $(CORE_SCRIPTS) $(SCRIPTS_$(FRONTEND_$*)) update-esps $(BUILD)/deb-$*$(DEB_LIB)/
	ln -s ../lib/$(DEB_NAME)/update-bootloader.py $(BUILD)/deb-$*/usr/sbin/hd-bootloader-update
	install -m 0644 deb/$*/defaults.json $(BUILD)/deb-$*$(DEB_SHARE)/defaults.json
	for hook in $(DEB_HOOKS); do install -D -m 0755 deb$$hook $(BUILD)/deb-$*$$hook; done
	printf '%s\n' $(DEB_HOOKS) > $(BUILD)/deb-$*/DEBIAN/conffiles
	for page in $(MANPAGES); do \
		dir=$(BUILD)/deb-$*/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(BUILD)/deb-$* | cut -f1)/" \
		deb/$*/control > $(BUILD)/deb-$*/DEBIAN/control
	install -m 0755 deb/postinst $(BUILD)/deb-$*/DEBIAN/postinst
	dpkg-deb --root-owner-group --build $(BUILD)/deb-$* $@

clean:
	rm -rf $(BUILD)
