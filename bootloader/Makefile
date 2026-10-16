# Builds the hyprdebian EFI bootloader and a FAT32 disk image to boot it in QEMU.
#
#   make        build build/bootx64-<frontend>.efi for every frontend in
#               FRONTENDS, the core (bootloader.cpp) linked with that
#               frontend (<frontend>.cpp), and build/assets.efi, the stub that
#               update-bootloader.py adds the assets to
#   make esp    run update-bootloader.py into build/esp/ with the config that
#               the package of ESP_FRONTEND installs,
#               deb/$(ESP_FRONTEND)/bootloader.json: EFI/BOOT/BOOTX64.EFI, the
#               bootloader of that frontend; EFI/BOOT/assets.efi, the stub
#               with the config, the menu rows, the DKMS MOK
#               (/var/lib/dkms/mok.pub) and the assets of that frontend added
#               as sections; and the UKIs of the 4 most recent kernels in
#               /boot. make esp ESP_FRONTEND=text tries another frontend.
#   make image  build build/esp.img (1GB FAT32) with build/esp/
#   make deb    build build/hyprdebian-bootloader-<frontend>_$(DEB_VERSION)_amd64.deb
#               for every frontend in FRONTENDS. Only one of them can be
#               installed: each provides, conflicts with and replaces
#               hyprdebian-bootloader. Each installs:
#                 /usr/lib/hyprdebian/bootloader/   bootx64-<frontend>.efi,
#                                                   assets.efi,
#                                                   update-bootloader.py,
#                                                   mkuki.py, the Python
#                                                   scripts of the frontend
#                                                   and update-esps.sh, which
#                                                   runs the one below for
#                                                   every ESP
#                 /usr/sbin/hd-bootloader-update    a link to
#                                                   update-bootloader.py
#                 /etc/hyprdebian/bootloader/bootloader.json
#                                                   deb/<frontend>/bootloader.json,
#                                                   a conffile
#                 /etc/kernel/postinst.d/zz-hd-bootloader
#                 /etc/kernel/postrm.d/zz-hd-bootloader
#                 /etc/initramfs/post-update.d/zz-hd-bootloader
#                                                   hooks from deb/etc that
#                                                   run update-esps.sh when a
#                                                   kernel or initrd changes,
#                                                   conffiles
#               with the control file deb/<frontend>/control and the postinst
#               deb/postinst, which runs update-esps.sh when the package is
#               installed or upgraded. The graphical
#               package depends on hyprdebian-art for the wallpaper and on
#               fonts-jetbrainsmono-nerd for the font, both built in ../art;
#               the text package needs neither.
#               Raise DEB_VERSION for every package that is handed out.
#   make clean  remove build/

BUILD := build
# A frontend shows the menu; see bootloader.hpp. Each has a <frontend>.cpp
# here, a <frontend>.py that builds its assets, which with its helpers is in
# SCRIPTS_<frontend>, and a deb/<frontend>/ with the control file and the
# config of its package, which holds the defaults of that package.
FRONTENDS := graphical text
TARGETS := $(FRONTENDS:%=$(BUILD)/bootx64-%.efi)
ASSETS_STUB := $(BUILD)/assets.efi
ESP := $(BUILD)/esp
# The frontend that make esp writes to build/esp/, with its config.
ESP_FRONTEND := graphical
ESP_CONFIG := deb/$(ESP_FRONTEND)/bootloader.json
# A stamp per frontend, so that make esp runs again when ESP_FRONTEND changes.
ESP_STAMP := $(BUILD)/esp-$(ESP_FRONTEND).stamp
IMAGE := $(BUILD)/esp.img
# Each UKI holds a whole initrd, which can be over 100MB.
IMAGE_SIZE := 1G

# The Python scripts of every package, and those of each frontend.
CORE_SCRIPTS := update-bootloader.py mkuki.py
SCRIPTS_graphical := graphical.py text2png.py png2bin.py menubox.py
SCRIPTS_text := text.py
SCRIPTS := $(CORE_SCRIPTS) $(foreach frontend,$(FRONTENDS),$(SCRIPTS_$(frontend)))
# The wallpaper that deb/graphical/bootloader.json names, as the package
# hyprdebian-art installs it.
WALLPAPER := /usr/share/backgrounds/hyprdebian/emerald-1920x1080.png
# What update-bootloader.py reads for a frontend, besides its config.
ESP_INPUTS_graphical := $(WALLPAPER)
ESP_INPUTS_text :=

# What update-bootloader.py reads.
ESP_SOURCES := $(SCRIPTS) $(ESP_CONFIG) $(ESP_INPUTS_$(ESP_FRONTEND)) \
	$(wildcard /boot/vmlinuz-* /boot/initrd.img-* /var/lib/dkms/mok.pub)

DEB_NAME := hyprdebian-bootloader
DEB_VERSION := 0.1
DEBS := $(FRONTENDS:%=$(BUILD)/$(DEB_NAME)-%_$(DEB_VERSION)_amd64.deb)
DEB_LIB := /usr/lib/hyprdebian/bootloader
DEB_CONF := /etc/hyprdebian/bootloader
# The kernel and initramfs hooks, below deb/ at the path they are installed to.
DEB_HOOKS := /etc/kernel/postinst.d/zz-hd-bootloader \
	/etc/kernel/postrm.d/zz-hd-bootloader \
	/etc/initramfs/post-update.d/zz-hd-bootloader

CXX := clang++
LD := lld-link
MKFS_FAT := /usr/sbin/mkfs.fat
PYTHON := python3
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

.PHONY: all esp image deb clean

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
	@echo "$@ is missing: install the package hyprdebian-art, built by ../art/create-deb.sh" >&2
	@exit 1

image: $(IMAGE)

$(IMAGE): $(ESP_STAMP)
	rm -f $@
	truncate -s $(IMAGE_SIZE) $@
	$(MKFS_FAT) -F 32 -n HYPRDEBIAN $@
	mcopy -s -m -i $@ $(ESP)/EFI ::/

deb: $(DEBS)

# Each package is built from a staged root in build/deb-<frontend>.
# update-bootloader.py finds its helpers, bootx64-<frontend>.efi and
# assets.efi next to itself, also through the link. dpkg-deb makes all files
# owned by root. The scripts of the frontend are a prerequisite through
# secondary expansion, which expands $$* to the frontend.
.SECONDEXPANSION:
$(BUILD)/$(DEB_NAME)-%_$(DEB_VERSION)_amd64.deb: $(BUILD)/bootx64-%.efi $(ASSETS_STUB) \
		$(CORE_SCRIPTS) $$(SCRIPTS_$$*) update-esps.sh deb/%/control deb/%/bootloader.json \
		deb/postinst $(addprefix deb,$(DEB_HOOKS)) | $(BUILD)
	rm -rf $(BUILD)/deb-$*
	install -d $(BUILD)/deb-$*/DEBIAN $(BUILD)/deb-$*$(DEB_LIB) $(BUILD)/deb-$*/usr/sbin \
		$(BUILD)/deb-$*$(DEB_CONF)
	install -m 0644 $(BUILD)/bootx64-$*.efi $(ASSETS_STUB) $(BUILD)/deb-$*$(DEB_LIB)/
	install -m 0755 $(CORE_SCRIPTS) $(SCRIPTS_$*) update-esps.sh $(BUILD)/deb-$*$(DEB_LIB)/
	ln -s ../lib/hyprdebian/bootloader/update-bootloader.py $(BUILD)/deb-$*/usr/sbin/hd-bootloader-update
	install -m 0644 deb/$*/bootloader.json $(BUILD)/deb-$*$(DEB_CONF)/bootloader.json
	for hook in $(DEB_HOOKS); do install -D -m 0755 deb$$hook $(BUILD)/deb-$*$$hook; done
	printf '%s\n' $(DEB_CONF)/bootloader.json $(DEB_HOOKS) > $(BUILD)/deb-$*/DEBIAN/conffiles
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(BUILD)/deb-$* | cut -f1)/" \
		deb/$*/control > $(BUILD)/deb-$*/DEBIAN/control
	install -m 0755 deb/postinst $(BUILD)/deb-$*/DEBIAN/postinst
	dpkg-deb --root-owner-group --build $(BUILD)/deb-$* $@

clean:
	rm -rf $(BUILD)
