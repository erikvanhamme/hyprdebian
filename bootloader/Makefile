# Builds the hyprdebian EFI bootloader and a FAT32 disk image to boot it in QEMU.
#
#   make        build build/bootx64.efi and build/assets.efi, the stub that
#               update-bootloader.py adds the assets to
#   make esp    run update-bootloader.py into build/esp/: EFI/BOOT/BOOTX64.EFI,
#               the bootloader; EFI/BOOT/assets.efi, the stub with the
#               config, background, banner, menu rows and the DKMS MOK
#               (/var/lib/dkms/mok.pub) added as sections; and the UKIs of
#               the 4 most recent kernels in /boot, with settings from
#               bootloader.json
#   make image  build build/esp.img (1GB FAT32) with build/esp/
#   make deb    build build/hyprdebian-bootloader_$(DEB_VERSION)_amd64.deb,
#               which installs:
#                 /usr/lib/hyprdebian/bootloader/   bootx64.efi, assets.efi,
#                                                   the Python scripts and
#                                                   update-esps.sh, which
#                                                   runs the one below for
#                                                   every ESP
#                 /usr/sbin/hd-bootloader-update    a link to
#                                                   update-bootloader.py
#                 /usr/share/hyprdebian/bootloader/wallpaper.png
#                 /etc/hyprdebian/bootloader/bootloader.json
#                                                   bootloader.json with the
#                                                   wallpaper above, a conffile
#                 /etc/kernel/postinst.d/zz-hd-bootloader
#                 /etc/kernel/postrm.d/zz-hd-bootloader
#                 /etc/initramfs/post-update.d/zz-hd-bootloader
#                                                   hooks from deb/etc that
#                                                   run update-esps.sh when a
#                                                   kernel or initrd changes,
#                                                   conffiles
#               Raise DEB_VERSION for every package that is handed out.
#   make clean  remove build/

BUILD := build
TARGET := $(BUILD)/bootx64.efi
ASSETS_STUB := $(BUILD)/assets.efi
ESP := $(BUILD)/esp
ESP_STAMP := $(BUILD)/esp.stamp
IMAGE := $(BUILD)/esp.img
# Each UKI holds a whole initrd, which can be over 100MB.
IMAGE_SIZE := 1G

SCRIPTS := update-bootloader.py mkuki.py text2png.py png2bin.py menubox.py
# The wallpaper that bootloader.json names.
WALLPAPER := ../deploy/etc/skel/Pictures/wallpapers/wallpaper.png

# What update-bootloader.py reads.
ESP_SOURCES := $(SCRIPTS) bootloader.json $(WALLPAPER) \
	$(wildcard /boot/vmlinuz-* /boot/initrd.img-* /var/lib/dkms/mok.pub)

DEB_NAME := hyprdebian-bootloader
DEB_VERSION := 0.1
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_amd64.deb
DEB_ROOT := $(BUILD)/deb
DEB_LIB := /usr/lib/hyprdebian/bootloader
DEB_SHARE := /usr/share/hyprdebian/bootloader
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
CXXFLAGS := \
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

all: $(TARGET) $(ASSETS_STUB)

$(BUILD):
	mkdir -p $@

$(BUILD)/%.obj: %.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(BUILD)/bootloader.obj
	$(LD) $(LDFLAGS) -out:$@ $^

# ukify adds the sections to the headers of the stub, but lld-link leaves
# only room for the stub's own one in the headers it pads to the file
# alignment of 512 bytes. With a file alignment of 4096 there is room for
# dozens.
$(ASSETS_STUB): $(BUILD)/assets.obj
	$(LD) $(LDFLAGS) -filealign:4096 -out:$@ $^

esp: $(ESP_STAMP)

$(ESP_STAMP): $(TARGET) $(ASSETS_STUB) $(ESP_SOURCES) | $(BUILD)
	$(PYTHON) update-bootloader.py --esp $(ESP) --config bootloader.json \
		--bootloader $(TARGET) --assets-stub $(ASSETS_STUB)
	touch $@

image: $(IMAGE)

$(IMAGE): $(ESP_STAMP)
	rm -f $@
	truncate -s $(IMAGE_SIZE) $@
	$(MKFS_FAT) -F 32 -n HYPRDEBIAN $@
	mcopy -s -m -i $@ $(ESP)/EFI ::/

deb: $(DEB)

# The package is built from a staged root in build/deb. update-bootloader.py
# finds its helpers, bootx64.efi and assets.efi next to itself, also through
# the link. The installed bootloader.json names the wallpaper by its absolute
# path; the one here names it relative to the source tree. dpkg-deb makes all
# files owned by root.
$(DEB): $(TARGET) $(ASSETS_STUB) $(SCRIPTS) update-esps.sh bootloader.json $(WALLPAPER) \
		deb/control $(addprefix deb,$(DEB_HOOKS)) | $(BUILD)
	rm -rf $(DEB_ROOT)
	install -d $(DEB_ROOT)/DEBIAN $(DEB_ROOT)$(DEB_LIB) $(DEB_ROOT)/usr/sbin \
		$(DEB_ROOT)$(DEB_SHARE) $(DEB_ROOT)$(DEB_CONF)
	install -m 0644 $(TARGET) $(ASSETS_STUB) $(DEB_ROOT)$(DEB_LIB)/
	install -m 0755 $(SCRIPTS) update-esps.sh $(DEB_ROOT)$(DEB_LIB)/
	ln -s ../lib/hyprdebian/bootloader/update-bootloader.py $(DEB_ROOT)/usr/sbin/hd-bootloader-update
	install -m 0644 $(WALLPAPER) $(DEB_ROOT)$(DEB_SHARE)/wallpaper.png
	$(PYTHON) -c 'import json, sys; config = json.load(open(sys.argv[1])); config["wallpaper"] = sys.argv[2]; print(json.dumps(config, indent=4))' \
		bootloader.json $(DEB_SHARE)/wallpaper.png > $(DEB_ROOT)$(DEB_CONF)/bootloader.json
	chmod 0644 $(DEB_ROOT)$(DEB_CONF)/bootloader.json
	for hook in $(DEB_HOOKS); do install -D -m 0755 deb$$hook $(DEB_ROOT)$$hook; done
	printf '%s\n' $(DEB_CONF)/bootloader.json $(DEB_HOOKS) > $(DEB_ROOT)/DEBIAN/conffiles
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(DEB_ROOT) | cut -f1)/" \
		deb/control > $(DEB_ROOT)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(DEB_ROOT) $@

clean:
	rm -rf $(BUILD)
