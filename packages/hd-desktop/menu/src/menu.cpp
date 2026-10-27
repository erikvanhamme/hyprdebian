#include "menu.hpp"

#include "json.hpp"

#include <iterator>
#include <stdexcept>
#include <xkbcommon/xkbcommon.h>

namespace {

// The label in lower case, as code points. The JSON reader gives UTF-8; a
// byte that starts no valid sequence counts as a character of its own.
std::u32string lower_label(const std::string& label)
{
    std::u32string lower;
    std::size_t i = 0;
    while (i < label.size()) {
        const unsigned char first = static_cast<unsigned char>(label[i]);
        std::size_t length = 1;
        char32_t c = first;
        if ((first & 0xe0) == 0xc0) {
            length = 2;
            c = first & 0x1f;
        } else if ((first & 0xf0) == 0xe0) {
            length = 3;
            c = first & 0x0f;
        } else if ((first & 0xf8) == 0xf0) {
            length = 4;
            c = first & 0x07;
        }
        if (i + length > label.size()) {
            length = 1;
            c = first;
        }
        for (std::size_t k = 1; k < length; ++k) {
            c = (c << 6) | (static_cast<unsigned char>(label[i + k]) & 0x3f);
        }
        lower += lower_char(c);
        i += length;
    }
    return lower;
}

Option read_option(JsonReader& reader, bool& is_default)
{
    Option option;
    std::string icon;
    reader.object([&](const std::string& key) {
        if (key == "label") {
            option.label = reader.string();
        } else if (key == "value") {
            option.value = reader.string();
        } else if (key == "icon") {
            icon = reader.string();
        } else if (key == "default") {
            is_default = reader.boolean();
        } else {
            reader.skip();
        }
    });
    if (option.label.empty()) {
        throw std::runtime_error("an option has no label");
    }
    if (option.value.empty()) {
        option.value = option.label;
    }
    if (!icon.empty()) {
        option.icon = load_icon(icon);
    }
    return option;
}

} // namespace

Menu read_menu(std::istream& in)
{
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    JsonReader reader(text);
    Menu menu;
    int default_option = -1;
    reader.object([&](const std::string& key) {
        if (key == "banner") {
            menu.banner = reader.string();
        } else if (key == "width") {
            menu.width = read_size(reader, key);
        } else if (key == "height") {
            menu.height = read_size(reader, key);
        } else if (key == "options") {
            reader.array([&] {
                bool is_default = false;
                menu.options.push_back(read_option(reader, is_default));
                if (is_default && default_option < 0) {
                    default_option = static_cast<int>(menu.options.size()) - 1;
                }
            });
        } else {
            reader.skip();
        }
    });
    reader.end();
    if (menu.options.empty()) {
        throw std::runtime_error("there are no options");
    }
    menu.selected = default_option < 0 ? 0 : default_option;
    return menu;
}

char32_t lower_char(char32_t c)
{
    const xkb_keysym_t sym = xkb_utf32_to_keysym(c);
    if (sym == XKB_KEY_NoSymbol) {
        return c;
    }
    const char32_t lower = xkb_keysym_to_utf32(xkb_keysym_to_lower(sym));
    if (lower == 0) {
        return c;
    }
    return lower;
}

int find_option(const Menu& menu, std::u32string_view typed)
{
    for (std::size_t i = 0; i < menu.options.size(); ++i) {
        if (lower_label(menu.options[i].label).starts_with(typed)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}
