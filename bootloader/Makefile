# Builds the hyprdebian EFI bootloader and a FAT32 disk image to boot it in QEMU.
#
#   make        build build/bootx64.efi
#   make esp    run update-bootloader.py into build/esp/: EFI/BOOT/BOOTX64.EFI,
#               the bootloader with its config, background and banner added
#               as sections, and the UKIs of the 4 most recent kernels in
#               /boot, with settings from bootloader.json
#   make image  build build/esp.img (1GB FAT32) with build/esp/
#   make clean  remove build/
#
# update-bootloader.py gives each UKI the modification time of its kernel,
# and mcopy -m makes that the creation time in the image, which the
# bootloader sorts the UKIs by.

BUILD := build
TARGET := $(BUILD)/bootx64.efi
SOURCES := bootloader.cpp
OBJECTS := $(SOURCES:%.cpp=$(BUILD)/%.obj)
ESP := $(BUILD)/esp
ESP_STAMP := $(BUILD)/esp.stamp
IMAGE := $(BUILD)/esp.img
# Each UKI holds a whole initrd, which can be over 100MB.
IMAGE_SIZE := 1G

# What update-bootloader.py reads; the wallpaper is the one bootloader.json names.
ESP_SOURCES := update-bootloader.py mkuki.py text2png.py png2bin.py menubox.py bootloader.json \
	../deploy/etc/skel/Pictures/wallpapers/wallpaper.png \
	$(wildcard /boot/vmlinuz-* /boot/initrd.img-*)

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

.PHONY: all esp image clean

all: $(TARGET)

$(BUILD):
	mkdir -p $@

$(BUILD)/%.obj: %.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(LD) $(LDFLAGS) -out:$@ $^

esp: $(ESP_STAMP)

$(ESP_STAMP): $(TARGET) $(ESP_SOURCES) | $(BUILD)
	$(PYTHON) update-bootloader.py --esp $(ESP) --bootloader $(TARGET)
	touch $@

image: $(IMAGE)

$(IMAGE): $(ESP_STAMP)
	rm -f $@
	truncate -s $(IMAGE_SIZE) $@
	$(MKFS_FAT) -F 32 -n HYPRDEBIAN $@
	mcopy -s -m -i $@ $(ESP)/EFI ::/

clean:
	rm -rf $(BUILD)
