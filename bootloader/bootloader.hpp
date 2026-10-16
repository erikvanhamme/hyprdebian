// hyprdebian EFI bootloader: what the core (bootloader.cpp) and a frontend
// share.
//
// The core loads assets.efi, builds the menu from its slots, handles the keys
// and boots the selected UKI. A frontend shows the menu: graphical.cpp draws
// it on the wallpaper with the label images in assets.efi. The Makefile links
// the core with one frontend into bootx64-<frontend>.efi; update-bootloader.py
// adds the assets that frontend needs to assets.efi.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <efi.h>

namespace hd {

// A view of a block of bytes in memory.
struct Bytes {
    uint8_t* data = nullptr;
    size_t size = 0;
};

// The menu has at most a row per slot of assets.efi, top to bottom.
constexpr size_t slot_count = 8;

extern EFI_HANDLE image;
extern EFI_SYSTEM_TABLE* st;
extern EFI_BOOT_SERVICES* bs;

// gnu-efi declares strings as CHAR16* (uint16_t); this converts a u"" literal.
CHAR16* efi_string(const char16_t* text);

uint32_t read_u32_le(const uint8_t* bytes);

// Finds a section (at most 8 characters, such as ".banner") of assets.efi.
// Returns empty Bytes if there is no such section or assets.efi is not
// loaded. The data is only valid until the assets are unloaded, after
// frontend::release_assets.
Bytes asset_section(const char* name);

// Implemented by each frontend.
namespace frontend {

// Sets up the screen and draws everything but the menu rows. Returns false if
// there is no display to show the menu on; the bootloader then returns to the
// firmware.
bool init();

// Adds a menu row for a slot (0 to slot_count - 1) whose UKI exists. The rows
// are added top to bottom; row is the number of rows before this one. Returns
// false to leave the slot out, such as when its assets for this frontend are
// missing or the row does not fit on the screen.
bool add_row(size_t slot, size_t row);

// Draws a row in its unselected or selected look.
void draw_row(size_t row, bool is_selected);

// Clears the screen for an error message on the text console.
void clear();

// Drops all pointers into assets.efi, which is about to be unloaded. Nothing
// is drawn afterwards.
void release_assets();

} // namespace frontend

} // namespace hd
