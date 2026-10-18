#include "menu.hpp"

#include "json.hpp"

#include <cstdlib>
#include <iostream>
#include <librsvg/rsvg.h>
#include <iterator>
#include <stdexcept>

namespace {

bool ends_with(const std::string& s, const std::string& end)
{
    return s.size() >= end.size() && s.compare(s.size() - end.size(), end.size(), end) == 0;
}

// Loads an SVG with librsvg, a PNG with cairo, which reads PNG itself.
Icon load_icon(const std::string& path)
{
    Icon icon;
    if (ends_with(path, ".svg") || ends_with(path, ".svgz")) {
        GError* error = nullptr;
        RsvgHandle* handle = rsvg_handle_new_from_file(path.c_str(), &error);
        if (handle == nullptr) {
            std::cerr << "hd-menu: cannot load the icon " << path << ": " << error->message << "\n";
            g_error_free(error);
            return icon;
        }
        icon.svg = std::shared_ptr<RsvgHandle>(handle, g_object_unref);
        return icon;
    }
    cairo_surface_t* image = cairo_image_surface_create_from_png(path.c_str());
    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
        std::cerr << "hd-menu: cannot load the icon " << path << ": "
                  << cairo_status_to_string(cairo_surface_status(image)) << "\n";
        cairo_surface_destroy(image);
        return icon;
    }
    icon.png = std::shared_ptr<cairo_surface_t>(image, cairo_surface_destroy);
    return icon;
}

// A number of pixels, or a string with a percentage.
Size read_size(JsonReader& reader, const std::string& key)
{
    if (reader.peek() != '"') {
        const double value = reader.number();
        if (value < 0) {
            throw std::runtime_error(key + " is negative");
        }
        return {true, false, value};
    }
    const std::string text = reader.string();
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || std::string(end) != "%" || value < 0) {
        throw std::runtime_error(key + " is not a number of pixels or a percentage such as \"50%\"");
    }
    return {true, true, value};
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
