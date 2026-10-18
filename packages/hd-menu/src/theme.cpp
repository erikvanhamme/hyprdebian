#include "theme.hpp"

#include "json.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace {

// "#rrggbb" to a colour; false when it is something else.
bool parse_color(const std::string& text, Color& color)
{
    if (text.size() != 7 || text[0] != '#') {
        return false;
    }
    char* end = nullptr;
    const long v = std::strtol(text.c_str() + 1, &end, 16);
    if (*end != '\0') {
        return false;
    }
    color = {((v >> 16) & 0xff) / 255.0, ((v >> 8) & 0xff) / 255.0, (v & 0xff) / 255.0};
    return true;
}

} // namespace

Theme load_theme(const std::string& path)
{
    Theme theme;
    std::ifstream file(path);
    if (!file) {
        std::cerr << "hd-menu: cannot read " << path << ", using the default theme\n";
        return theme;
    }
    std::stringstream text;
    text << file.rdbuf();

    // The members of hd-theme, by type.
    std::map<std::string, std::string> strings;
    std::map<std::string, double> numbers;
    try {
        JsonReader reader(text.str());
        reader.object([&](const std::string& key) {
            if (key != "hd-theme" || reader.peek() != '{') {
                reader.skip();
                return;
            }
            reader.object([&](const std::string& field) {
                const char c = reader.peek();
                if (c == '"') {
                    strings[field] = reader.string();
                } else if (c == '-' || (c >= '0' && c <= '9')) {
                    numbers[field] = reader.number();
                } else {
                    reader.skip();
                }
            });
        });
        reader.end();
    } catch (const std::exception& e) {
        std::cerr << "hd-menu: " << path << ": " << e.what() << ", using the default theme\n";
        return Theme();
    }

    if (strings.count("font") != 0 && !strings["font"].empty()) {
        theme.font = strings["font"];
    }
    for (auto [name, color] : {std::pair{"highlight", &theme.highlight}, std::pair{"normal", &theme.normal},
                               std::pair{"dark", &theme.dark}, std::pair{"light", &theme.light}}) {
        if (strings.count(name) != 0 && !parse_color(strings[name], *color)) {
            std::cerr << "hd-menu: hd-theme: " << name << " is not a #rrggbb colour\n";
        }
    }
    if (numbers.count("font-size-big") != 0 && numbers["font-size-big"] > 0) {
        theme.font_size_big = numbers["font-size-big"];
    }
    if (numbers.count("icon_size") != 0 && numbers["icon_size"] > 0) {
        theme.icon_size = numbers["icon_size"];
    }
    if (numbers.count("border") != 0 && numbers["border"] >= 0) {
        theme.border = numbers["border"];
    }
    if (numbers.count("radius") != 0 && numbers["radius"] >= 0) {
        theme.radius = numbers["radius"];
    }
    if (numbers.count("opacity") != 0) {
        theme.opacity = std::clamp(numbers["opacity"], 0.0, 1.0);
    }
    return theme;
}
