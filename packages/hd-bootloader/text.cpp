// hyprdebian EFI bootloader: the text-only frontend.
//
// Draws the menu on the text console of the firmware, in the mode it is in:
// the banner, centred, a tenth of the way down the screen, and below it,
// after a blank line, a row per menu entry. The rows are as wide as the
// longest label plus a space on either side and are centred together; the
// selected one is drawn in inverse video. The texts are sections of
// assets.efi in UCS-2, made by text.py.

#include "bootloader.hpp"

namespace hd {

namespace {

constexpr const char* banner_section = ".banner";

// Per slot, the label.
constexpr const char* label_sections[slot_count] = {
    ".lbl1", ".lbl2", ".lbl3", ".lbl4", ".lbl5", ".lbl6", ".lbl7", ".lbl8",
};

// The longest text the frontend shows, in characters. Wider than any text
// console, so the screen width is what limits a text in practice.
constexpr size_t max_text_length = 256;

constexpr UINTN normal_attribute = EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK);
constexpr UINTN banner_attribute = EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK);
constexpr UINTN selected_attribute = EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY);

// The size of the text console in characters.
UINTN columns = 80;
UINTN lines = 25;

// The line of the banner; the menu rows start two lines below it.
UINTN banner_line = 0;

// A row of the menu: its label, copied out of assets.efi, nul-terminated.
struct Row {
    char16_t label[max_text_length + 1];
    size_t length;
};

Row rows[slot_count];
// The length of the longest label: the rows are drawn this wide, plus a
// space on either side.
size_t label_width = 0;

// Copies a text section of assets.efi into text, nul-terminated, and returns
// its length in length. Returns false if it is empty, longer than
// max_length, not a whole number of UCS-2 characters or holds a character
// the console cannot show: a control character or half of a surrogate pair.
// The assets come from a partition anyone with root or physical access can
// write, so nothing may use them before this check.
bool read_text(Bytes section, char16_t* text, size_t max_length, size_t& length)
{
    if (section.size == 0 || section.size % 2 != 0 || section.size / 2 > max_length) {
        return false;
    }
    length = section.size / 2;
    for (size_t i = 0; i < length; ++i) {
        const auto c = static_cast<char16_t>(section.data[2 * i] | section.data[2 * i + 1] << 8);
        if (c < 0x20 || (c >= 0x7f && c < 0xa0) || (c >= 0xd800 && c < 0xe000)) {
            return false;
        }
        text[i] = c;
    }
    text[length] = u'\0';
    return true;
}

// The column at which a text of the given length is centred.
UINTN centred_column(size_t length)
{
    return length < columns ? (columns - length) / 2 : 0;
}

void output(UINTN column, UINTN line, UINTN attribute, const char16_t* text)
{
    st->ConOut->SetAttribute(st->ConOut, attribute);
    st->ConOut->SetCursorPosition(st->ConOut, column, line);
    st->ConOut->OutputString(st->ConOut, efi_string(text));
}

} // namespace

namespace frontend {

// Uses the console mode the firmware set up. A banner that is missing or not
// valid, or does not fit on a line, is left out.
bool init()
{
    if (st->ConOut == nullptr) {
        return false;
    }

    UINTN mode_columns = 0;
    UINTN mode_lines = 0;
    if (st->ConOut->QueryMode(st->ConOut, st->ConOut->Mode->Mode, &mode_columns, &mode_lines) == EFI_SUCCESS
        && mode_columns != 0 && mode_lines != 0) {
        columns = mode_columns;
        lines = mode_lines;
    }
    banner_line = lines / 10;

    st->ConOut->SetAttribute(st->ConOut, normal_attribute);
    st->ConOut->ClearScreen(st->ConOut);
    st->ConOut->EnableCursor(st->ConOut, FALSE);

    // Printing up to the last column would wrap the line.
    char16_t banner[max_text_length + 1];
    size_t length = 0;
    if (read_text(asset_section(banner_section), banner, columns - 1, length)) {
        output(centred_column(length), banner_line, banner_attribute, banner);
    }
    return true;
}

// A slot needs a valid label that fits on a line with a space on either side,
// and a line above the last one: printing on the last line may scroll the
// screen.
bool add_row(size_t slot, size_t row)
{
    Row& entry = rows[row];
    if (columns < 3
        || !read_text(asset_section(label_sections[slot]), entry.label, columns - 3, entry.length)) {
        return false;
    }
    if (banner_line + 2 + row + 1 >= lines) {
        return false;
    }
    if (entry.length > label_width) {
        label_width = entry.length;
    }
    return true;
}

// Every row is padded to the same width, so the selection bar is as wide for
// each. The attribute goes back to normal afterwards, for whatever the
// console shows next.
void draw_row(size_t row, bool is_selected)
{
    const Row& entry = rows[row];
    char16_t line[max_text_length + 3];
    size_t length = 0;
    line[length++] = u' ';
    for (size_t i = 0; i < entry.length; ++i) {
        line[length++] = entry.label[i];
    }
    while (length < label_width + 1) {
        line[length++] = u' ';
    }
    line[length++] = u' ';
    line[length] = u'\0';

    output(centred_column(length), banner_line + 2 + row, is_selected ? selected_attribute : normal_attribute,
           line);
    st->ConOut->SetAttribute(st->ConOut, normal_attribute);
}

// The core clears the screen after this; the error is shown in the normal
// colours.
void clear()
{
    st->ConOut->SetAttribute(st->ConOut, normal_attribute);
}

// The labels are copies, and the banner is not kept.
void release_assets()
{
}

} // namespace frontend

} // namespace hd
