// hyprdebian EFI bootloader.
//
// Freestanding C++23: no runtime library, no exceptions, no RTTI. The UEFI
// types come from the headers of the Debian package gnu-efi; nothing of
// gnu-efi is linked in.

#include <stddef.h>
#include <stdint.h>

#include <efi.h>

namespace {

// A view of a block of bytes in memory.
struct Bytes {
    uint8_t* data = nullptr;
    size_t size = 0;
};

// Settings from the .config section, with their defaults.
struct Config {
    uint32_t timeout = 5; // Seconds to wait for a key before booting.
};

// The sections that update-bootloader.py adds to this bootloader's own PE
// file: the config, the background and the banner. A Secure Boot signature
// over the file covers them, and the firmware has loaded them into memory
// with the rest of the bootloader. The bootloader takes the display
// resolution from the size of the background.
constexpr const char* config_section = ".config";
constexpr const char* background_section = ".backgnd";
constexpr const char* banner_section = ".banner";

// The UKIs to choose from are the .efi files in this directory.
constexpr const char16_t* uki_directory = u"\\EFI\\Linux";
constexpr size_t max_ukis = 32;
// FAT allows file names of 255 characters; one more for the terminating nul.
constexpr size_t max_name_length = 256;

// The menu labels that mkuki.py puts in each UKI: per profile, the label in
// the unselected and in the selected colour.
constexpr size_t profile_count = 2;
constexpr const char* label_sections[profile_count][2] = {
    {".label0", ".label0s"},
    {".label1", ".label1s"},
};

// The menu offers the profiles of the most recent UKIs only: 8 rows.
constexpr size_t menu_ukis = 4;
constexpr size_t menu_rows = menu_ukis * profile_count;

struct Uki {
    char16_t name[max_name_length];
    EFI_TIME created;
    // Indexed by profile, then 0 for unselected and 1 for selected. Empty if
    // the UKI has no such section.
    Bytes labels[profile_count][2];
};

// Size of the width and height fields in front of the pixels of an image.
constexpr size_t image_header_size = 8;

// The largest images the bootloader accepts: the resolution of an 8K display.
// The limits also keep width * height * 4 far from overflowing.
constexpr uint32_t max_image_width = 7680;
constexpr uint32_t max_image_height = 4320;

// The UEFI APIs take non-const pointers to their GUIDs.
EFI_GUID graphics_output_protocol_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_GUID loaded_image_protocol_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID simple_file_system_protocol_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID device_path_protocol_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;

EFI_HANDLE image = nullptr;
EFI_SYSTEM_TABLE* st = nullptr;
EFI_BOOT_SERVICES* bs = nullptr;
EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = nullptr;
// The device (partition) the bootloader was loaded from, and where the
// firmware loaded the bootloader in memory.
EFI_HANDLE boot_device = nullptr;
const uint8_t* image_base = nullptr;
uint64_t image_size_in_memory = 0;

// The UKIs found in uki_directory, and pointers to them sorted from the most
// recently created to the oldest.
Uki ukis[max_ukis];
Uki* sorted_ukis[max_ukis];
size_t uki_count = 0;

// A row of the menu: a profile of a UKI and where its label is drawn.
struct MenuEntry {
    const Uki* uki;
    size_t profile;
    uint32_t x;
    uint32_t y;
};

// The rows of the menu, top to bottom, and the selected one.
MenuEntry menu[menu_rows];
size_t menu_count = 0;
size_t selected = 0;

// The background image, kept to redraw the parts under menu rows. Empty if
// it could not be loaded; the screen is black then.
Bytes background;
uint32_t background_width = 0;
uint32_t background_height = 0;

// The work area: the black box that menubox.py draws on the background. It
// is 80% of the screen in both directions, centred, and holds ten rows.
struct WorkArea {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t row_height;
};
constexpr uint32_t work_area_rows = 10;

// gnu-efi declares strings as CHAR16* (uint16_t); this converts a u"" literal.
CHAR16* efi_string(const char16_t* text)
{
    return reinterpret_cast<CHAR16*>(const_cast<char16_t*>(text));
}

size_t string_length(const char16_t* text)
{
    size_t length = 0;
    while (text[length] != u'\0') {
        ++length;
    }
    return length;
}

bool is_space(uint8_t c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

// Compares text of the given length with a nul-terminated word.
bool equals(const uint8_t* text, size_t length, const char* word)
{
    size_t i = 0;
    for (; i < length; ++i) {
        if (word[i] == '\0' || text[i] != static_cast<uint8_t>(word[i])) {
            return false;
        }
    }
    return word[i] == '\0';
}

// Parses an unsigned decimal number. Returns false if the text is empty,
// holds anything other than digits, or does not fit in 32 bits.
bool parse_number(const uint8_t* text, size_t length, uint32_t& value)
{
    if (length == 0) {
        return false;
    }

    uint64_t result = 0;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        result = result * 10 + (text[i] - '0');
        if (result > UINT32_MAX) {
            return false;
        }
    }

    value = static_cast<uint32_t>(result);
    return true;
}

// Reads the settings from a config file of "key value" lines. Blank lines and
// lines starting with # are skipped. Lines with an unknown key or a value
// that is not a number are ignored, so their setting keeps its default.
void parse_config(Bytes text, Config& config)
{
    size_t pos = 0;

    while (pos < text.size) {
        size_t begin = pos;
        size_t end = pos;
        while (end < text.size && text.data[end] != '\n') {
            ++end;
        }
        pos = end + 1;

        while (begin < end && is_space(text.data[begin])) {
            ++begin;
        }
        while (end > begin && is_space(text.data[end - 1])) {
            --end;
        }
        if (begin == end || text.data[begin] == '#') {
            continue;
        }

        size_t key_end = begin;
        while (key_end < end && !is_space(text.data[key_end])) {
            ++key_end;
        }
        size_t value_begin = key_end;
        while (value_begin < end && is_space(text.data[value_begin])) {
            ++value_begin;
        }

        const uint8_t* key = text.data + begin;
        const size_t key_length = key_end - begin;

        uint32_t* setting = nullptr;
        if (equals(key, key_length, "timeout")) {
            setting = &config.timeout;
        }

        if (setting != nullptr) {
            parse_number(text.data + value_begin, end - value_begin, *setting);
        }
    }
}

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

uint16_t read_u16_le(const uint8_t* bytes)
{
    return static_cast<uint16_t>(bytes[0] | bytes[1] << 8);
}

uint32_t read_u32_le(const uint8_t* bytes)
{
    return static_cast<uint32_t>(bytes[0]) | static_cast<uint32_t>(bytes[1]) << 8
           | static_cast<uint32_t>(bytes[2]) << 16 | static_cast<uint32_t>(bytes[3]) << 24;
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

// Opens a file or directory on the volume the bootloader was loaded from.
// Returns nullptr on failure.
EFI_FILE_PROTOCOL* open_file(const char16_t* path)
{
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* file_system = nullptr;
    if (bs->HandleProtocol(boot_device, &simple_file_system_protocol_guid,
                           reinterpret_cast<void**>(&file_system))
        != EFI_SUCCESS) {
        return nullptr;
    }

    EFI_FILE_PROTOCOL* root = nullptr;
    if (file_system->OpenVolume(file_system, &root) != EFI_SUCCESS) {
        return nullptr;
    }

    EFI_FILE_PROTOCOL* file = nullptr;
    const EFI_STATUS status = root->Open(root, &file, efi_string(path), EFI_FILE_MODE_READ, 0);
    root->Close(root);
    if (status != EFI_SUCCESS) {
        return nullptr;
    }
    return file;
}

// True if the name ends in .efi, in any case.
bool has_efi_extension(const CHAR16* name)
{
    size_t length = 0;
    while (name[length] != 0) {
        ++length;
    }
    if (length < 4) {
        return false;
    }

    const char16_t* extension = u".efi";
    for (size_t i = 0; i < 4; ++i) {
        CHAR16 c = name[length - 4 + i];
        if (c >= u'A' && c <= u'Z') {
            c = static_cast<CHAR16>(c - u'A' + u'a');
        }
        if (c != extension[i]) {
            return false;
        }
    }
    return true;
}

// True if time a is later than time b. The time zone is ignored: FAT stores
// local times without one.
bool later(const EFI_TIME& a, const EFI_TIME& b)
{
    if (a.Year != b.Year) {
        return a.Year > b.Year;
    }
    if (a.Month != b.Month) {
        return a.Month > b.Month;
    }
    if (a.Day != b.Day) {
        return a.Day > b.Day;
    }
    if (a.Hour != b.Hour) {
        return a.Hour > b.Hour;
    }
    if (a.Minute != b.Minute) {
        return a.Minute > b.Minute;
    }
    if (a.Second != b.Second) {
        return a.Second > b.Second;
    }
    return a.Nanosecond > b.Nanosecond;
}

// Finds a section (at most 8 characters, such as ".banner") of the
// bootloader itself in the memory the firmware loaded it into. Every offset
// is checked against the size of the loaded image. Returns empty Bytes if
// there is no such section.
Bytes own_section(const char* name)
{
    const uint8_t* base = image_base;
    const uint64_t size = image_size_in_memory;
    if (base == nullptr || size < 64 || base[0] != 'M' || base[1] != 'Z') {
        return {};
    }

    // See read_section for the layout of the headers.
    const uint64_t pe_offset = read_u32_le(base + 0x3c);
    if (pe_offset > size || size - pe_offset < 24) {
        return {};
    }
    const uint8_t* pe_header = base + pe_offset;
    if (pe_header[0] != 'P' || pe_header[1] != 'E' || pe_header[2] != 0 || pe_header[3] != 0) {
        return {};
    }
    const uint16_t section_count = read_u16_le(pe_header + 4 + 2);
    const uint16_t optional_header_size = read_u16_le(pe_header + 4 + 16);
    const uint64_t section_table = pe_offset + 24 + optional_header_size;
    if (section_table > size || (size - section_table) / 40 < section_count) {
        return {};
    }

    for (uint16_t i = 0; i < section_count; ++i) {
        const uint8_t* section = base + section_table + i * 40;

        size_t n = 0;
        while (n < 8 && name[n] != '\0' && section[n] == static_cast<uint8_t>(name[n])) {
            ++n;
        }
        if (name[n] != '\0' || (n < 8 && section[n] != 0)) {
            continue;
        }

        const uint64_t virtual_size = read_u32_le(section + 8);
        const uint64_t virtual_address = read_u32_le(section + 12);
        if (virtual_address > size || size - virtual_address < virtual_size) {
            return {};
        }
        return {const_cast<uint8_t*>(base + virtual_address), static_cast<size_t>(virtual_size)};
    }

    return {};
}

// Reads size bytes at offset of a file. Returns false on failure.
bool read_at(EFI_FILE_PROTOCOL* file, uint64_t offset, void* buffer, size_t size)
{
    if (file->SetPosition(file, offset) != EFI_SUCCESS) {
        return false;
    }
    UINTN read_size = size;
    return file->Read(file, &read_size, buffer) == EFI_SUCCESS && read_size == size;
}

// Reads the section with the given name (at most 8 characters, such as
// ".label0") of a PE file such as a UKI into a buffer from the pool. Only
// the headers and that section are read, not the whole file. Returns empty
// Bytes if there is no such section or on failure.
Bytes read_section(EFI_FILE_PROTOCOL* file, const char* name)
{
    // The DOS header starts with "MZ" and holds the offset of the PE header
    // at 0x3c.
    uint8_t dos_header[64];
    if (!read_at(file, 0, dos_header, sizeof(dos_header)) || dos_header[0] != 'M' || dos_header[1] != 'Z') {
        return {};
    }
    const uint32_t pe_offset = read_u32_le(dos_header + 0x3c);

    // "PE\0\0" followed by the COFF header.
    uint8_t pe_header[24];
    if (!read_at(file, pe_offset, pe_header, sizeof(pe_header)) || pe_header[0] != 'P' || pe_header[1] != 'E'
        || pe_header[2] != 0 || pe_header[3] != 0) {
        return {};
    }
    const uint16_t section_count = read_u16_le(pe_header + 4 + 2);
    const uint16_t optional_header_size = read_u16_le(pe_header + 4 + 16);
    const uint64_t section_table = pe_offset + sizeof(pe_header) + optional_header_size;

    for (uint16_t i = 0; i < section_count; ++i) {
        // Name (8 bytes, padded with nul bytes), virtual size, virtual address,
        // size of raw data, pointer to raw data, ...
        uint8_t section[40];
        if (!read_at(file, section_table + i * sizeof(section), section, sizeof(section))) {
            return {};
        }

        size_t n = 0;
        while (n < 8 && name[n] != '\0' && section[n] == static_cast<uint8_t>(name[n])) {
            ++n;
        }
        if (name[n] != '\0' || (n < 8 && section[n] != 0)) {
            continue;
        }

        // The raw data is padded to the file alignment; the virtual size is
        // the size of the data itself.
        const uint32_t virtual_size = read_u32_le(section + 8);
        const uint32_t raw_size = read_u32_le(section + 16);
        const uint32_t raw_offset = read_u32_le(section + 20);
        const uint32_t size = virtual_size != 0 && virtual_size < raw_size ? virtual_size : raw_size;

        void* buffer = nullptr;
        if (bs->AllocatePool(EfiLoaderData, size, &buffer) != EFI_SUCCESS) {
            return {};
        }
        if (!read_at(file, raw_offset, buffer, size)) {
            bs->FreePool(buffer);
            return {};
        }
        return {static_cast<uint8_t*>(buffer), size};
    }

    return {};
}

// Reads the menu labels of a UKI.
void load_labels(Uki& uki)
{
    // uki_directory, a backslash, the name and the terminating nul.
    char16_t path[16 + max_name_length];
    size_t length = 0;
    for (const char16_t* c = uki_directory; *c != u'\0'; ++c) {
        path[length++] = *c;
    }
    path[length++] = u'\\';
    for (const char16_t* c = uki.name; *c != u'\0'; ++c) {
        path[length++] = *c;
    }
    path[length] = u'\0';

    EFI_FILE_PROTOCOL* file = open_file(path);
    if (file == nullptr) {
        return;
    }
    for (size_t profile = 0; profile < profile_count; ++profile) {
        for (size_t selected = 0; selected < 2; ++selected) {
            uki.labels[profile][selected] = read_section(file, label_sections[profile][selected]);
        }
    }
    file->Close(file);
}

// Fills ukis with the .efi files in uki_directory, sorts sorted_ukis from the
// most recently created to the oldest, and loads the menu labels of the
// newest menu_ukis.
void find_ukis()
{
    EFI_FILE_PROTOCOL* directory = open_file(uki_directory);
    if (directory == nullptr) {
        return;
    }

    // Reading a directory returns one EFI_FILE_INFO per call, followed by the
    // file name. This is large enough for any FAT file name.
    alignas(8) static uint8_t buffer[sizeof(EFI_FILE_INFO) + max_name_length * sizeof(CHAR16)];

    for (;;) {
        UINTN size = sizeof(buffer);
        if (directory->Read(directory, &size, buffer) != EFI_SUCCESS || size == 0) {
            break;
        }

        const auto* info = reinterpret_cast<const EFI_FILE_INFO*>(buffer);
        if ((info->Attribute & EFI_FILE_DIRECTORY) != 0 || !has_efi_extension(info->FileName)) {
            continue;
        }
        if (uki_count == max_ukis) {
            break;
        }

        Uki& uki = ukis[uki_count];
        size_t i = 0;
        for (; i < max_name_length - 1 && info->FileName[i] != 0; ++i) {
            uki.name[i] = static_cast<char16_t>(info->FileName[i]);
        }
        uki.name[i] = u'\0';
        uki.created = info->CreateTime;
        sorted_ukis[uki_count] = &uki;
        ++uki_count;
    }
    directory->Close(directory);

    // Insertion sort, newest first; UKIs created at the same time keep their
    // directory order.
    for (size_t i = 1; i < uki_count; ++i) {
        Uki* uki = sorted_ukis[i];
        size_t j = i;
        while (j > 0 && later(uki->created, sorted_ukis[j - 1]->created)) {
            sorted_ukis[j] = sorted_ukis[j - 1];
            --j;
        }
        sorted_ukis[j] = uki;
    }

    for (size_t i = 0; i < uki_count && i < menu_ukis; ++i) {
        load_labels(*sorted_ukis[i]);
    }
}

// The work area on the current screen, computed as menubox.py does: its size
// is 80% of the screen, rounded to the nearest pixel.
WorkArea work_area()
{
    const uint32_t screen_width = gop->Mode->Info->HorizontalResolution;
    const uint32_t screen_height = gop->Mode->Info->VerticalResolution;
    const uint32_t width = (screen_width * 8 + 5) / 10;
    const uint32_t height = (screen_height * 8 + 5) / 10;
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

// Fills menu with a row per profile of the newest menu_ukis UKIs, newest
// first. Profiles without two valid labels are left out. The rows start half a row
// below the banner in the top row of the work area; each label is centred in
// its row.
void build_menu()
{
    if (gop == nullptr) {
        return;
    }

    const WorkArea area = work_area();
    const uint32_t first_row = area.y + area.row_height + area.row_height / 2;

    for (size_t i = 0; i < uki_count && i < menu_ukis; ++i) {
        for (size_t profile = 0; profile < profile_count; ++profile) {
            const Uki* uki = sorted_ukis[i];
            const Bytes label = uki->labels[profile][0];
            uint32_t label_width = 0;
            uint32_t label_height = 0;
            uint32_t selected_width = 0;
            uint32_t selected_height = 0;
            if (!image_size(label, label_width, label_height)
                || !image_size(uki->labels[profile][1], selected_width, selected_height)) {
                continue;
            }
            const uint32_t top = first_row + static_cast<uint32_t>(menu_count) * area.row_height;
            if (top + area.row_height > area.y + area.height) {
                return;
            }
            const uint32_t y = label_height < area.row_height ? top + (area.row_height - label_height) / 2 : top;
            menu[menu_count++] = {uki, profile, centred_x(area, label_width), y};
        }
    }
}

// Draws a row of the menu in its unselected or selected colour. The label is
// blended onto the screen, so the background under it is redrawn first.
void draw_entry(size_t index, bool is_selected)
{
    const MenuEntry& entry = menu[index];
    const Bytes label = entry.uki->labels[entry.profile][is_selected ? 1 : 0];
    uint32_t width = 0;
    uint32_t height = 0;
    if (!image_size(label, width, height)) {
        return;
    }
    draw_background(entry.x, entry.y, width, height);
    blit(label, entry.x, entry.y);
}

void draw_menu()
{
    for (size_t i = 0; i < menu_count; ++i) {
        draw_entry(i, i == selected);
    }
}

void put_char16(uint8_t*& out, char16_t c)
{
    *out++ = static_cast<uint8_t>(c & 0xff);
    *out++ = static_cast<uint8_t>(c >> 8);
}

// Builds the device path of a file on the boot device: the device path of the
// partition followed by a file path node and an end node. The caller frees it
// with FreePool. Returns nullptr on failure.
EFI_DEVICE_PATH* file_device_path(const char16_t* directory, const char16_t* name)
{
    EFI_DEVICE_PATH* device_path = nullptr;
    if (bs->HandleProtocol(boot_device, &device_path_protocol_guid, reinterpret_cast<void**>(&device_path))
        != EFI_SUCCESS) {
        return nullptr;
    }

    // Length of the partition's device path without its end node.
    const auto* device_bytes = reinterpret_cast<const uint8_t*>(device_path);
    size_t device_length = 0;
    for (;;) {
        const uint8_t* node = device_bytes + device_length;
        if (node[0] == END_DEVICE_PATH_TYPE && node[1] == END_ENTIRE_DEVICE_PATH_SUBTYPE) {
            break;
        }
        device_length += static_cast<size_t>(node[2]) | static_cast<size_t>(node[3]) << 8;
    }

    // directory, a backslash, name and the terminating nul.
    const size_t path_chars = string_length(directory) + 1 + string_length(name) + 1;
    const size_t file_node_length = 4 + path_chars * sizeof(CHAR16);
    const size_t end_node_length = 4;

    void* buffer = nullptr;
    if (bs->AllocatePool(EfiLoaderData, device_length + file_node_length + end_node_length, &buffer)
        != EFI_SUCCESS) {
        return nullptr;
    }

    auto* out = static_cast<uint8_t*>(buffer);
    for (size_t i = 0; i < device_length; ++i) {
        *out++ = device_bytes[i];
    }

    *out++ = MEDIA_DEVICE_PATH;
    *out++ = MEDIA_FILEPATH_DP;
    *out++ = static_cast<uint8_t>(file_node_length & 0xff);
    *out++ = static_cast<uint8_t>(file_node_length >> 8);
    for (const char16_t* c = directory; *c != u'\0'; ++c) {
        put_char16(out, *c);
    }
    put_char16(out, u'\\');
    for (const char16_t* c = name; *c != u'\0'; ++c) {
        put_char16(out, *c);
    }
    put_char16(out, u'\0');

    *out++ = END_DEVICE_PATH_TYPE;
    *out++ = END_ENTIRE_DEVICE_PATH_SUBTYPE;
    *out++ = end_node_length;
    *out++ = 0;

    return static_cast<EFI_DEVICE_PATH*>(buffer);
}

// Moves the selection one row up or down; it stops at the first and last row.
void up()
{
    if (selected == 0) {
        return;
    }
    draw_entry(selected, false);
    --selected;
    draw_entry(selected, true);
}

void down()
{
    if (selected + 1 >= menu_count) {
        return;
    }
    draw_entry(selected, false);
    ++selected;
    draw_entry(selected, true);
}

// Handles key presses until Enter is pressed. If no key is pressed within
// timeout seconds, returns as well; the first key press stops the countdown.
void run_key_loop(uint32_t timeout)
{
    if (timeout == 0) {
        return;
    }

    // Without a timer the loop just waits for Enter.
    EFI_EVENT timer = nullptr;
    if (bs->CreateEvent(EVT_TIMER, TPL_APPLICATION, nullptr, nullptr, &timer) != EFI_SUCCESS) {
        timer = nullptr;
    } else {
        // The timer counts in units of 100 ns.
        bs->SetTimer(timer, TimerRelative, static_cast<UINT64>(timeout) * 10'000'000);
    }

    for (;;) {
        EFI_EVENT events[2] = {st->ConIn->WaitForKey, timer};
        const UINTN event_count = timer == nullptr ? 1 : 2;
        UINTN index = 0;
        bs->WaitForEvent(event_count, events, &index);

        if (index == 1) {
            break;
        }

        if (timer != nullptr) {
            bs->CloseEvent(timer);
            timer = nullptr;
        }

        EFI_INPUT_KEY key{};
        if (st->ConIn->ReadKeyStroke(st->ConIn, &key) != EFI_SUCCESS) {
            continue;
        }

        if (key.ScanCode == SCAN_UP) {
            up();
        } else if (key.ScanCode == SCAN_DOWN) {
            down();
        } else if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            break;
        }
    }

    if (timer != nullptr) {
        bs->CloseEvent(timer);
    }
}

// Starts the selected UKI in the selected profile. Returns only if that
// fails or the UKI exits.
void boot()
{
    if (menu_count == 0) {
        return;
    }

    const MenuEntry& entry = menu[selected];
    EFI_DEVICE_PATH* path = file_device_path(uki_directory, entry.uki->name);
    if (path == nullptr) {
        return;
    }

    EFI_HANDLE uki_image = nullptr;
    const EFI_STATUS load_status = bs->LoadImage(FALSE, image, path, nullptr, 0, &uki_image);
    bs->FreePool(path);
    if (load_status != EFI_SUCCESS) {
        return;
    }

    EFI_LOADED_IMAGE_PROTOCOL* loaded_uki = nullptr;
    if (bs->HandleProtocol(uki_image, &loaded_image_protocol_guid, reinterpret_cast<void**>(&loaded_uki))
        != EFI_SUCCESS) {
        bs->UnloadImage(uki_image);
        return;
    }

    // The command line of the UKI; the systemd stub takes @N as profile N.
    // It must stay valid while the UKI runs.
    static char16_t options[] = u"@0";
    options[1] = static_cast<char16_t>(u'0' + entry.profile);
    loaded_uki->LoadOptions = options;
    loaded_uki->LoadOptionsSize = sizeof(options);

    UINTN exit_data_size = 0;
    CHAR16* exit_data = nullptr;
    bs->StartImage(uki_image, &exit_data_size, &exit_data);
    if (exit_data != nullptr) {
        bs->FreePool(exit_data);
    }
}

} // namespace

extern "C" EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* system_table)
{
    image = image_handle;
    st = system_table;
    bs = system_table->BootServices;

    // The firmware arms a 5 minute watchdog before starting a boot option;
    // disable it so waiting for the user does not reset the machine.
    bs->SetWatchdogTimer(0, 0, 0, nullptr);

    EFI_LOADED_IMAGE_PROTOCOL* loaded_image = nullptr;
    if (bs->HandleProtocol(image, &loaded_image_protocol_guid, reinterpret_cast<void**>(&loaded_image))
        != EFI_SUCCESS) {
        return EFI_LOAD_ERROR;
    }
    boot_device = loaded_image->DeviceHandle;
    image_base = static_cast<const uint8_t*>(loaded_image->ImageBase);
    image_size_in_memory = loaded_image->ImageSize;

    Config config;
    parse_config(own_section(config_section), config);

    // The background sets the display resolution.
    background = own_section(background_section);
    if (!image_size(background, background_width, background_height)) {
        background = {};
        background_width = 0;
        background_height = 0;
    }
    if (!set_display_mode(background_width, background_height) && gop == nullptr) {
        return EFI_UNSUPPORTED;
    }
    draw_full_background();

    const WorkArea area = work_area();
    draw_centred(own_section(banner_section), area, area.y);

    find_ukis();
    build_menu();
    draw_menu();

    st->ConIn->Reset(st->ConIn, FALSE);
    run_key_loop(config.timeout);

    boot();

    return EFI_SUCCESS;
}
