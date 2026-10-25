// hyprdebian EFI bootloader: the core.
//
// Freestanding C++23: no runtime library, no exceptions, no RTTI. The UEFI
// types come from the headers of the Debian package gnu-efi; nothing of
// gnu-efi is linked in.
//
// The core loads assets.efi, builds the menu, handles the keys and boots the
// selected UKI. A frontend, linked in by the Makefile, shows the menu; see
// bootloader.hpp.

#include "bootloader.hpp"

namespace hd {

EFI_HANDLE image = nullptr;
EFI_SYSTEM_TABLE* st = nullptr;
EFI_BOOT_SERVICES* bs = nullptr;

CHAR16* efi_string(const char16_t* text)
{
    return reinterpret_cast<CHAR16*>(const_cast<char16_t*>(text));
}

namespace {

// Settings from the .config section, with their defaults.
struct Config {
    uint32_t timeout = 5; // Seconds to wait for a key before booting.
};

// The assets are sections that update-bootloader.py adds to a stub PE file,
// assets.efi next to the bootloader: the config, the menu rows, the DKMS MOK
// when the system has one, and what the frontend shows. They can change
// without touching the bootloader. The bootloader loads assets.efi with
// LoadImage, so that with Secure Boot on the firmware checks its signature,
// and reads the sections from the loaded image; it never starts it. Without
// a usable assets.efi the bootloader shows an error and returns to the
// firmware: it would not know what to boot, nor the MOK the kernel needs to
// load a ZFS module built by DKMS.
constexpr const char16_t* assets_directory = u"\\EFI\\BOOT";
constexpr const char16_t* assets_name = u"assets.efi";
constexpr const char* config_section = ".config";

// The DKMS MOK: an EFI_SIGNATURE_LIST with the X.509 certificate that DKMS
// signs modules such as ZFS with. Under Secure Boot the kernel only loads
// modules signed with a key it trusts. Shim hands its MOKs to the kernel in
// an EFI configuration table, the MOK variable table; the bootloader does the
// same with this one, in the entry MokListRT. The entry MokListTrustedRT
// tells the kernel to trust the MOKs for modules: it puts them in its
// .machine keyring rather than the .platform keyring. A system with only the
// kernels of hd-kernel has no DKMS MOK and assets.efi no .mok section: their
// ZFS module is signed with the key the kernel was built with.
constexpr const char* mok_section = ".mok";

// The UKIs are the .efi files in this directory.
constexpr const char16_t* uki_directory = u"\\EFI\\Linux";
// FAT allows file names of 255 characters; one more for the terminating nul.
constexpr size_t max_name_length = 256;

// The menu has a row per slot, top to bottom. Per slot, assets.efi holds the
// UKI to boot and its profile as "name profile", such as
// "HyprDebian-Kernel-7.2.10.efi 1"; the frontend has its own sections for
// how the row looks.
constexpr const char* slot_sections[slot_count] = {
    ".slot1", ".slot2", ".slot3", ".slot4", ".slot5", ".slot6", ".slot7", ".slot8",
};

// The UEFI APIs take non-const pointers to their GUIDs.
EFI_GUID loaded_image_protocol_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID simple_file_system_protocol_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID device_path_protocol_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;
// LINUX_EFI_MOK_VARIABLE_TABLE_GUID of the kernel.
EFI_GUID mok_variable_table_guid = {0xc451ed2b, 0x9694, 0x45d3, {0xba, 0xba, 0xed, 0x9f, 0x89, 0x88, 0xa3, 0x89}};

// The device (partition) the bootloader was loaded from.
EFI_HANDLE boot_device = nullptr;
// assets.efi and where the firmware loaded it in memory. Null if it could not
// be loaded or failed the Secure Boot check.
EFI_HANDLE assets_image = nullptr;
const uint8_t* assets_base = nullptr;
uint64_t assets_size = 0;
// The MOK variable table built from the .mok section of assets.efi, to be
// installed just before the UKI starts. Null if assets.efi has no .mok.
void* mok_table = nullptr;

// A row of the menu: the UKI file name in uki_directory and the profile to
// boot it in.
struct MenuEntry {
    char16_t name[max_name_length];
    uint32_t profile;
};

// The rows of the menu, top to bottom, and the selected one.
MenuEntry menu[slot_count];
size_t menu_count = 0;
size_t selected = 0;

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

uint16_t read_u16_le(const uint8_t* bytes)
{
    return static_cast<uint16_t>(bytes[0] | bytes[1] << 8);
}

} // namespace

uint32_t read_u32_le(const uint8_t* bytes)
{
    return static_cast<uint32_t>(bytes[0]) | static_cast<uint32_t>(bytes[1]) << 8
           | static_cast<uint32_t>(bytes[2]) << 16 | static_cast<uint32_t>(bytes[3]) << 24;
}

namespace {

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

} // namespace

// Finds the section in the memory the firmware loaded assets.efi into. Every
// offset is checked against the size of the loaded image.
Bytes asset_section(const char* name)
{
    const uint8_t* base = assets_base;
    const uint64_t size = assets_size;
    if (base == nullptr || size < 64 || base[0] != 'M' || base[1] != 'Z') {
        return {};
    }

    // The DOS header starts with "MZ" and holds the offset of the PE header
    // at 0x3c. The PE header is "PE\0\0" followed by the COFF header, the
    // optional header and the section table.
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
        // Name (8 bytes, padded with nul bytes), virtual size, virtual
        // address, ...
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

namespace {

// Builds the path of a file in uki_directory. Returns false if it does not
// fit in path.
bool uki_path(const char16_t* name, char16_t (&path)[16 + max_name_length])
{
    size_t length = 0;
    for (const char16_t* c = uki_directory; *c != u'\0'; ++c) {
        path[length++] = *c;
    }
    path[length++] = u'\\';
    for (const char16_t* c = name; *c != u'\0'; ++c) {
        if (length == sizeof(path) / sizeof(path[0]) - 1) {
            return false;
        }
        path[length++] = *c;
    }
    path[length] = u'\0';
    return true;
}

// True if the UKI file exists in uki_directory.
bool uki_exists(const char16_t* name)
{
    char16_t path[16 + max_name_length];
    if (!uki_path(name, path)) {
        return false;
    }
    EFI_FILE_PROTOCOL* file = open_file(path);
    if (file == nullptr) {
        return false;
    }
    file->Close(file);
    return true;
}

// Reads a slot of the form "name profile": a UKI file name in uki_directory
// of printable ASCII without slashes or backslashes, a space and a profile
// number of one digit. Trailing white space and a newline are allowed.
// Returns false if the slot is not of that form.
bool parse_slot(Bytes text, MenuEntry& entry)
{
    size_t end = text.size;
    while (end > 0 && (is_space(text.data[end - 1]) || text.data[end - 1] == '\n')) {
        --end;
    }
    if (end < 3 || text.data[end - 2] != ' ' || text.data[end - 1] < '0' || text.data[end - 1] > '9') {
        return false;
    }
    const size_t name_length = end - 2;
    if (name_length >= max_name_length) {
        return false;
    }
    for (size_t i = 0; i < name_length; ++i) {
        const uint8_t c = text.data[i];
        if (c <= ' ' || c > '~' || c == '/' || c == '\\') {
            return false;
        }
        entry.name[i] = static_cast<char16_t>(c);
    }
    entry.name[name_length] = u'\0';
    entry.profile = text.data[end - 1] - '0';
    return true;
}

// Fills menu with a row per slot in assets.efi, in slot order. Slots that are
// not of the form "name profile", name a UKI that does not exist or that the
// frontend does not take are left out.
void build_menu()
{
    for (size_t slot = 0; slot < slot_count; ++slot) {
        MenuEntry& entry = menu[menu_count];
        if (!parse_slot(asset_section(slot_sections[slot]), entry) || !uki_exists(entry.name)
            || !frontend::add_row(slot, menu_count)) {
            continue;
        }
        ++menu_count;
    }
}

void draw_menu()
{
    for (size_t i = 0; i < menu_count; ++i) {
        frontend::draw_row(i, i == selected);
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

// Loads assets.efi with LoadImage, which checks its Secure Boot signature,
// and sets assets_image, assets_base and assets_size. They stay null without
// assets.efi or if it fails the check.
void load_assets()
{
    EFI_DEVICE_PATH* path = file_device_path(assets_directory, assets_name);
    if (path == nullptr) {
        return;
    }

    EFI_HANDLE handle = nullptr;
    const EFI_STATUS status = bs->LoadImage(FALSE, image, path, nullptr, 0, &handle);
    bs->FreePool(path);
    // On EFI_SECURITY_VIOLATION the firmware has loaded the image anyway,
    // only it may not be started. Its contents failed the check, so it is
    // unloaded and none of it is used.
    if (status != EFI_SUCCESS) {
        if (handle != nullptr) {
            bs->UnloadImage(handle);
        }
        return;
    }

    EFI_LOADED_IMAGE_PROTOCOL* loaded_assets = nullptr;
    if (bs->HandleProtocol(handle, &loaded_image_protocol_guid, reinterpret_cast<void**>(&loaded_assets))
        != EFI_SUCCESS) {
        bs->UnloadImage(handle);
        return;
    }

    assets_image = handle;
    assets_base = static_cast<const uint8_t*>(loaded_assets->ImageBase);
    assets_size = loaded_assets->ImageSize;
}

// Unloads assets.efi. Nothing may use the assets afterwards.
void unload_assets()
{
    if (assets_image == nullptr) {
        return;
    }
    frontend::release_assets();
    bs->UnloadImage(assets_image);
    assets_image = nullptr;
    assets_base = nullptr;
    assets_size = 0;
}

// Moves the selection one row up or down; it stops at the first and last row.
void up()
{
    if (selected == 0) {
        return;
    }
    frontend::draw_row(selected, false);
    --selected;
    frontend::draw_row(selected, true);
}

void down()
{
    if (selected + 1 >= menu_count) {
        return;
    }
    frontend::draw_row(selected, false);
    ++selected;
    frontend::draw_row(selected, true);
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

// Starts a UKI in uki_directory in the given profile (0 to 9). Returns only
// if that fails or the UKI exits.
void boot(const char16_t* name, uint32_t profile)
{
    EFI_DEVICE_PATH* path = file_device_path(uki_directory, name);
    if (path == nullptr) {
        return;
    }

    EFI_HANDLE uki_image = nullptr;
    const EFI_STATUS load_status = bs->LoadImage(FALSE, image, path, nullptr, 0, &uki_image);
    bs->FreePool(path);
    if (load_status != EFI_SUCCESS) {
        // On EFI_SECURITY_VIOLATION the image is loaded but may not be started.
        if (uki_image != nullptr) {
            bs->UnloadImage(uki_image);
        }
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
    options[1] = static_cast<char16_t>(u'0' + profile);
    loaded_uki->LoadOptions = options;
    loaded_uki->LoadOptionsSize = sizeof(options);

    UINTN exit_data_size = 0;
    CHAR16* exit_data = nullptr;
    bs->StartImage(uki_image, &exit_data_size, &exit_data);
    if (exit_data != nullptr) {
        bs->FreePool(exit_data);
    }
}

// Checks that the bytes are one EFI_SIGNATURE_LIST and nothing else: its
// SignatureListSize matches, and after the 28 byte header and the signature
// header there is a whole number of signatures, at least one, each more than
// the 16 byte owner GUID.
bool is_signature_list(Bytes list)
{
    constexpr size_t list_header_size = 28;
    if (list.size < list_header_size) {
        return false;
    }
    const uint32_t list_size = read_u32_le(list.data + 16);
    const uint32_t header_size = read_u32_le(list.data + 20);
    const uint32_t signature_size = read_u32_le(list.data + 24);
    if (list_size != list.size || signature_size <= 16 || list.size - list_header_size < header_size) {
        return false;
    }
    const size_t signatures_size = list.size - list_header_size - header_size;
    return signatures_size != 0 && signatures_size % signature_size == 0;
}

// Appends an entry to the MOK variable table: the name, nul padded to 256
// bytes, the size of the data as a uint64 and the data.
void put_mok_entry(uint8_t*& out, const char* name, const uint8_t* data, size_t size)
{
    size_t i = 0;
    for (; name[i] != '\0'; ++i) {
        *out++ = static_cast<uint8_t>(name[i]);
    }
    for (; i < 256; ++i) {
        *out++ = 0;
    }
    for (size_t byte = 0; byte < 8; ++byte) {
        *out++ = static_cast<uint8_t>(static_cast<uint64_t>(size) >> (8 * byte));
    }
    for (size_t byte = 0; byte < size; ++byte) {
        *out++ = data[byte];
    }
}

// Builds the MOK variable table from list, the .mok section of assets.efi,
// in memory that the kernel keeps: MokListRT with the signature list,
// MokListTrustedRT with the byte 1 that shim puts there too, and the
// terminating entry with an empty name and no data. Returns nullptr if list
// is no valid signature list or there is no memory.
void* build_mok_table(const Bytes& list)
{
    if (!is_signature_list(list)) {
        return nullptr;
    }

    constexpr size_t entry_header_size = 256 + 8;
    const uint8_t trusted = 1;
    const size_t size = 3 * entry_header_size + list.size + sizeof(trusted);
    void* table = nullptr;
    if (bs->AllocatePool(EfiRuntimeServicesData, size, &table) != EFI_SUCCESS) {
        return nullptr;
    }

    auto* out = static_cast<uint8_t*>(table);
    put_mok_entry(out, "MokListRT", list.data, list.size);
    put_mok_entry(out, "MokListTrustedRT", &trusted, sizeof(trusted));
    put_mok_entry(out, "", nullptr, 0);
    return table;
}

// Shows an error on the console, waits for a key and returns the status for
// efi_main to return to the firmware, which then tries the next boot option.
EFI_STATUS fail(const char16_t* message, EFI_STATUS status)
{
    frontend::clear();
    st->ConOut->ClearScreen(st->ConOut);
    st->ConOut->OutputString(st->ConOut, efi_string(u"HyprDebian bootloader: "));
    st->ConOut->OutputString(st->ConOut, efi_string(message));
    st->ConOut->OutputString(st->ConOut, efi_string(u"\r\nPress a key to return to the firmware.\r\n"));

    st->ConIn->Reset(st->ConIn, FALSE);
    UINTN index = 0;
    bs->WaitForEvent(1, &st->ConIn->WaitForKey, &index);
    EFI_INPUT_KEY key{};
    st->ConIn->ReadKeyStroke(st->ConIn, &key);
    return status;
}

} // namespace

} // namespace hd

using namespace hd;

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

    load_assets();
    if (assets_image == nullptr) {
        return fail(u"EFI\\BOOT\\assets.efi is missing or failed the Secure Boot check.", EFI_NOT_FOUND);
    }
    // No .mok section means no DKMS MOK; a .mok section that is not a valid
    // signature list is an error, as the kernel could then not load the ZFS
    // module of DKMS.
    const Bytes mok = asset_section(mok_section);
    if (mok.size != 0) {
        mok_table = build_mok_table(mok);
        if (mok_table == nullptr) {
            return fail(u"assets.efi holds no valid DKMS MOK.", EFI_NOT_FOUND);
        }
    }

    Config config;
    parse_config(asset_section(config_section), config);

    if (!frontend::init()) {
        return EFI_UNSUPPORTED;
    }

    build_menu();
    if (menu_count == 0) {
        return fail(u"assets.efi names no UKI in EFI\\Linux that exists.", EFI_NOT_FOUND);
    }
    draw_menu();

    st->ConIn->Reset(st->ConIn, FALSE);
    run_key_loop(config.timeout);

    // The kernel does not need the assets; free their memory before booting.
    // The menu keeps its own copy of the name and profile, so they stay valid.
    const MenuEntry& entry = menu[selected];
    unload_assets();

    // The MOK variable table goes to the kernel through the system table.
    if (mok_table != nullptr) {
        if (bs->InstallConfigurationTable(&mok_variable_table_guid, mok_table) != EFI_SUCCESS) {
            return fail(u"The DKMS MOK could not be handed to the kernel.", EFI_OUT_OF_RESOURCES);
        }
    }
    boot(entry.name, entry.profile);

    return fail(u"The selected UKI could not be started.", EFI_LOAD_ERROR);
}
