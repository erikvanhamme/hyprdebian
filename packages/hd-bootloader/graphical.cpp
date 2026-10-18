// hyprdebian EFI bootloader: the graphical frontend.
//
// Draws the menu with the graphics output protocol: the background with the
// work area over it, the banner in the top row of the work area and a label
// image per menu row, in the unselected or the selected colour. The images
// are sections of assets.efi in the raw image format of png2bin.py; the
// bootloader takes the display resolution from the size of the background
// and the place of the work area from the .area section.

#include "bootloader.hpp"

namespace hd {

namespace {

constexpr const char* background_section = ".backgnd";
constexpr const char* banner_section = ".banner";
constexpr const char* area_section = ".area";

// Per slot, the label in the unselected and in the selected colour.
constexpr const char* label_sections[slot_count][2] = {
    {".lbl1", ".lbl1s"}, {".lbl2", ".lbl2s"}, {".lbl3", ".lbl3s"}, {".lbl4", ".lbl4s"},
    {".lbl5", ".lbl5s"}, {".lbl6", ".lbl6s"}, {".lbl7", ".lbl7s"}, {".lbl8", ".lbl8s"},
};

// Size of the width and height fields in front of the pixels of an image.
constexpr size_t image_header_size = 8;

// The largest images the bootloader accepts: the resolution of an 8K display.
// The limits also keep width * height * 4 far from overflowing.
constexpr uint32_t max_image_width = 7680;
constexpr uint32_t max_image_height = 4320;

// The UEFI APIs take non-const pointers to their GUIDs.
EFI_GUID graphics_output_protocol_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = nullptr;

// A row of the menu: its labels (0 unselected, 1 selected) and where they are
// drawn.
struct Row {
    Bytes labels[2];
    uint32_t x;
    uint32_t y;
};

Row rows[slot_count];

// The background image, kept to redraw the parts under menu rows. Empty if
// it could not be loaded; the screen is black then.
Bytes background;
uint32_t background_width = 0;
uint32_t background_height = 0;

// The work area: the black box that menubox.py draws on the background. Its
// size and place are set in the config and come in the .area section;
// it holds ten rows.
struct WorkArea {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t row_height;
};
constexpr uint32_t work_area_rows = 10;

// Size of the .area section: the left, top, width and height of the work
// area in pixels on the background, each a 32-bit little-endian integer.
constexpr size_t area_section_size = 16;

// The work area, set by init.
WorkArea area = {};

// Switches the graphics output to width x height.
// Returns false if no such mode exists or the switch fails.
bool set_display_mode(uint32_t width, uint32_t height)
{
    if (bs->LocateProtocol(&graphics_output_protocol_guid, nullptr, reinterpret_cast<void**>(&gop))
        != EFI_SUCCESS) {
        gop = nullptr;
        return false;
    }

    for (UINT32 mode = 0; mode < gop->Mode->MaxMode; ++mode) {
        UINTN size_of_info = 0;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = nullptr;
        if (gop->QueryMode(gop, mode, &size_of_info, &info) != EFI_SUCCESS) {
            continue;
        }

        const bool match =
            info->HorizontalResolution == width && info->VerticalResolution == height;
        bs->FreePool(info);

        if (match) {
            return gop->SetMode(gop, mode) == EFI_SUCCESS;
        }
    }

    return false;
}

// Clips a rectangle to width_limit x height_limit. Returns false if nothing
// of it is left.
bool clip(uint32_t x, uint32_t y, uint32_t& width, uint32_t& height, uint32_t width_limit,
          uint32_t height_limit)
{
    if (x >= width_limit || y >= height_limit) {
        return false;
    }
    if (width > width_limit - x) {
        width = width_limit - x;
    }
    if (height > height_limit - y) {
        height = height_limit - y;
    }
    return width != 0 && height != 0;
}

// Fills a rectangle of the screen with black. It is clipped to the screen.
void clear_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (gop == nullptr) {
        return;
    }
    if (!clip(x, y, width, height, gop->Mode->Info->HorizontalResolution, gop->Mode->Info->VerticalResolution)) {
        return;
    }
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL black{};
    gop->Blt(gop, &black, EfiBltVideoFill, 0, 0, x, y, width, height, 0);
}

// Copies a rectangle of the background to the same place on the screen. Its
// B, G, R, A pixels are the EFI_GRAPHICS_OUTPUT_BLT_PIXEL layout, so one Blt
// does it. Without a background the rectangle is cleared to black.
void draw_background(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (gop == nullptr) {
        return;
    }
    if (background.size == 0) {
        clear_rectangle(x, y, width, height);
        return;
    }
    if (!clip(x, y, width, height, gop->Mode->Info->HorizontalResolution, gop->Mode->Info->VerticalResolution)
        || !clip(x, y, width, height, background_width, background_height)) {
        return;
    }
    auto* pixels = reinterpret_cast<EFI_GRAPHICS_OUTPUT_BLT_PIXEL*>(background.data + image_header_size);
    gop->Blt(gop, pixels, EfiBltBufferToVideo, x, y, x, y, width, height, static_cast<UINTN>(background_width) * 4);
}

// Draws the whole background. Parts of the screen it does not cover, when
// the display could not be switched to its size, are black.
void draw_full_background()
{
    if (gop == nullptr) {
        return;
    }
    const uint32_t screen_width = gop->Mode->Info->HorizontalResolution;
    const uint32_t screen_height = gop->Mode->Info->VerticalResolution;
    if (background_width < screen_width || background_height < screen_height) {
        clear_rectangle(0, 0, screen_width, screen_height);
    }
    draw_background(0, 0, screen_width, screen_height);
}

// Checks that buffer holds an image in the format of png2bin.py and returns
// its size: the width and height may be at most max_image_width and
// max_image_height, and exactly width * height pixels of 4 bytes must follow.
// The assets come from a partition anyone with root or physical access can
// write, so nothing may use their size before this check.
bool image_size(Bytes buffer, uint32_t& width, uint32_t& height)
{
    if (buffer.size < image_header_size) {
        return false;
    }
    width = read_u32_le(buffer.data);
    height = read_u32_le(buffer.data + 4);
    if (width > max_image_width || height > max_image_height) {
        return false;
    }
    return buffer.size - image_header_size == static_cast<size_t>(width) * height * 4;
}

// Mixes a colour channel of the image into the framebuffer by its alpha.
uint32_t blend(uint32_t src, uint32_t dst, uint32_t alpha)
{
    return (src * alpha + dst * (255 - alpha) + 127) / 255;
}

// Draws an image made by png2bin.py on the framebuffer with its top left
// corner at x, y. The image is clipped to the screen.
bool blit(Bytes buffer, uint32_t x, uint32_t y)
{
    uint32_t width = 0;
    uint32_t height = 0;
    if (gop == nullptr || !image_size(buffer, width, height)) {
        return false;
    }
    const uint8_t* pixels = buffer.data + image_header_size;

    const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = gop->Mode->Info;
    bool bgr = false;
    if (info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        bgr = true;
    } else if (info->PixelFormat != PixelRedGreenBlueReserved8BitPerColor) {
        return false;
    }

    auto* frame_buffer = reinterpret_cast<uint8_t*>(gop->Mode->FrameBufferBase);

    for (uint32_t row = 0; row < height && y + row < info->VerticalResolution; ++row) {
        for (uint32_t col = 0; col < width && x + col < info->HorizontalResolution; ++col) {
            const uint8_t* src = pixels + (static_cast<size_t>(row) * width + col) * 4;
            uint8_t* dst =
                frame_buffer
                + (static_cast<size_t>(y + row) * info->PixelsPerScanLine + x + col) * 4;

            // The image is stored as B, G, R, A.
            const uint32_t alpha = src[3];
            const uint8_t blue = bgr ? 0 : 2;
            const uint8_t red = bgr ? 2 : 0;
            dst[blue] = static_cast<uint8_t>(blend(src[0], dst[blue], alpha));
            dst[1] = static_cast<uint8_t>(blend(src[1], dst[1], alpha));
            dst[red] = static_cast<uint8_t>(blend(src[2], dst[red], alpha));
        }
    }

    return true;
}

// The work area of the .area section. Without a valid one, such as when it
// does not fit on the screen, it is the default of menubox.py: 50% of the
// screen, rounded to the nearest pixel, and centred.
WorkArea work_area()
{
    const uint32_t screen_width = gop->Mode->Info->HorizontalResolution;
    const uint32_t screen_height = gop->Mode->Info->VerticalResolution;

    const Bytes section = asset_section(area_section);
    if (section.size == area_section_size) {
        const uint32_t x = read_u32_le(section.data);
        const uint32_t y = read_u32_le(section.data + 4);
        const uint32_t width = read_u32_le(section.data + 8);
        const uint32_t height = read_u32_le(section.data + 12);
        if (width >= work_area_rows && height >= work_area_rows && x <= screen_width
            && width <= screen_width - x && y <= screen_height && height <= screen_height - y) {
            return {x, y, width, height, height / work_area_rows};
        }
    }

    const uint32_t width = (screen_width + 1) / 2;
    const uint32_t height = (screen_height + 1) / 2;
    return {(screen_width - width) / 2, (screen_height - height) / 2, width, height, height / work_area_rows};
}

// The x at which an image of the given width is centred in the work area.
uint32_t centred_x(const WorkArea& area, uint32_t width)
{
    return width < area.width ? area.x + (area.width - width) / 2 : area.x;
}

// Draws an image centred in a row of the work area, starting at top.
void draw_centred(Bytes image_data, const WorkArea& area, uint32_t top)
{
    uint32_t width = 0;
    uint32_t height = 0;
    if (image_size(image_data, width, height)) {
        const uint32_t y = height < area.row_height ? top + (area.row_height - height) / 2 : top;
        blit(image_data, centred_x(area, width), y);
    }
}

} // namespace

namespace frontend {

// The background sets the display resolution. Without a graphics output there
// is nothing to draw on.
bool init()
{
    background = asset_section(background_section);
    if (!image_size(background, background_width, background_height)) {
        background = {};
        background_width = 0;
        background_height = 0;
    }
    if (!set_display_mode(background_width, background_height) && gop == nullptr) {
        return false;
    }
    draw_full_background();

    area = work_area();
    draw_centred(asset_section(banner_section), area, area.y);
    return true;
}

// A slot needs both its labels as valid images. The rows start half a row
// below the banner in the top row of the work area; each label is centred in
// its row.
bool add_row(size_t slot, size_t row)
{
    Row& entry = rows[row];
    entry.labels[0] = asset_section(label_sections[slot][0]);
    entry.labels[1] = asset_section(label_sections[slot][1]);
    uint32_t label_width = 0;
    uint32_t label_height = 0;
    uint32_t selected_width = 0;
    uint32_t selected_height = 0;
    if (!image_size(entry.labels[0], label_width, label_height)
        || !image_size(entry.labels[1], selected_width, selected_height)) {
        return false;
    }

    const uint32_t first_row = area.y + area.row_height + area.row_height / 2;
    const uint32_t top = first_row + static_cast<uint32_t>(row) * area.row_height;
    if (top + area.row_height > area.y + area.height) {
        return false;
    }
    entry.x = centred_x(area, label_width);
    entry.y = label_height < area.row_height ? top + (area.row_height - label_height) / 2 : top;
    return true;
}

// The label is blended onto the screen, so the background under it is
// redrawn first.
void draw_row(size_t row, bool is_selected)
{
    const Row& entry = rows[row];
    const Bytes label = entry.labels[is_selected ? 1 : 0];
    uint32_t width = 0;
    uint32_t height = 0;
    if (!image_size(label, width, height)) {
        return;
    }
    draw_background(entry.x, entry.y, width, height);
    blit(label, entry.x, entry.y);
}

// The text console may cover only part of a large screen; clearing it would
// leave the menu around it.
void clear()
{
    if (gop != nullptr) {
        clear_rectangle(0, 0, gop->Mode->Info->HorizontalResolution, gop->Mode->Info->VerticalResolution);
    }
}

void release_assets()
{
    background = {};
    background_width = 0;
    background_height = 0;
    for (Row& entry : rows) {
        entry.labels[0] = {};
        entry.labels[1] = {};
    }
}

} // namespace frontend

} // namespace hd
