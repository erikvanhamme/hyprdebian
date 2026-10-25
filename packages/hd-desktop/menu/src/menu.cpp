#include "menu.hpp"

#include "json.hpp"

#include <iterator>
#include <stdexcept>

namespace {

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
